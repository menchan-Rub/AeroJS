/**
 * @file context.cpp
 * @brief AeroJS JavaScript エンジンの実行コンテキストクラス実装
 * @version 0.1.0
 * @license MIT
 */

#include "context.h"

#include <fstream>
#include <sstream>

#include "../utils/file/file_utils.h"
#include "bytecode_generator.h"
#include "engine.h"
#include "exception.h"
#include "function.h"
#include "interpreter.h"
#include "jit_compiler.h"
#include "lexer.h"
#include "object.h"
#include "parser.h"
#include "scope.h"
#include "symbol_table.h"
#include "value.h"

namespace aerojs {
namespace core {

Context::Context(const ContextOptions& options)
    : options_(options),
      globalObject_(nullptr),
      lastException_(nullptr),
      currentScope_(nullptr),
      symbolTable_(nullptr),
      parser_(nullptr),
      lexer_(nullptr),
      bytecodeGenerator_(nullptr),
      interpreter_(nullptr),
      jitCompiler_(nullptr) {
  // エンジンからメモリアロケータを取得
  auto* engine = Engine::getInstance();
  auto* allocator = engine->getMemoryAllocator();

  // グローバルオブジェクトの作成
  globalObject_ = new (allocator->allocate(sizeof(Object))) Object(this);

  // 各コンポーネントの初期化
  symbolTable_ = new (allocator->allocate(sizeof(SymbolTable))) SymbolTable(this);
  lexer_ = new (allocator->allocate(sizeof(Lexer))) Lexer(this);
  parser_ = new (allocator->allocate(sizeof(Parser))) Parser(this, lexer_);
  bytecodeGenerator_ = new (allocator->allocate(sizeof(BytecodeGenerator))) BytecodeGenerator(this, symbolTable_);
  interpreter_ = new (allocator->allocate(sizeof(Interpreter))) Interpreter(this);

  // JITコンパイラは条件付きで初期化
  if (engine->isJITEnabled()) {
    jitCompiler_ = new (allocator->allocate(sizeof(JITCompiler))) JITCompiler(this);
  }

  // グローバルスコープの作成
  currentScope_ = new (allocator->allocate(sizeof(Scope))) Scope(this, nullptr, globalObject_);

  // ビルトインオブジェクトの初期化
  initializeBuiltins();

  // エンジンにコンテキストを登録
  engine->registerContext(this);
}

Context::~Context() {
  // エンジンからコンテキストを削除
  Engine::getInstance()->unregisterContext(this);

  // カスタムデータのクリーンアップ
  {
    std::lock_guard<std::mutex> lock(contextDataMutex_);
    for (auto& entry : contextData_) {
      if (entry.second.cleaner) {
        entry.second.cleaner(entry.second.data);
      }
    }
    contextData_.clear();
  }

  // 強制ガベージコレクション実行
  collectGarbage(true);

  // 各コンポーネントの解放
  auto* allocator = Engine::getInstance()->getMemoryAllocator();

  if (jitCompiler_) {
    jitCompiler_->~JITCompiler();
    allocator->deallocate(jitCompiler_);
  }

  interpreter_->~Interpreter();
  allocator->deallocate(interpreter_);

  bytecodeGenerator_->~BytecodeGenerator();
  allocator->deallocate(bytecodeGenerator_);

  parser_->~Parser();
  allocator->deallocate(parser_);

  lexer_->~Lexer();
  allocator->deallocate(lexer_);

  symbolTable_->~SymbolTable();
  allocator->deallocate(symbolTable_);

  currentScope_->~Scope();
  allocator->deallocate(currentScope_);

  // グローバルオブジェクトの解放は最後に
  globalObject_->~Object();
  allocator->deallocate(globalObject_);
}

void Context::clearLastException() {
  lastException_ = nullptr;
}

void Context::setLastException(Exception* exception) {
  lastException_ = exception;
}

Exception* Context::setErrorException(const std::string& type, const std::string& message) {
  auto* engine = Engine::getInstance();
  auto* allocator = engine->getMemoryAllocator();

  // エラーオブジェクトを作成
  auto* exception = new (allocator->allocate(sizeof(Exception))) Exception(this, type, message);
  lastException_ = exception;
  return exception;
}

Function* Context::createFunction(const std::string& name, NativeFunction func, int argCount) {
  auto* engine = Engine::getInstance();
  auto* allocator = engine->getMemoryAllocator();

  // 関数オブジェクトを作成
  auto* function = new (allocator->allocate(sizeof(Function))) Function(this, name, func, argCount);
  return function;
}

bool Context::registerGlobalFunction(const std::string& name, NativeFunction func, int argCount) {
  if (!globalObject_) {
    return false;
  }

  // 関数オブジェクトを作成
  auto* function = createFunction(name, func, argCount);
  if (!function) {
    return false;
  }

  // グローバルオブジェクトにプロパティとして設定
  return globalObject_->setProperty(name, Value::fromObject(function));
}

bool Context::registerGlobalValue(const std::string& name, Value* value) {
  if (!globalObject_ || !value) {
    return false;
  }

  // グローバルオブジェクトにプロパティとして設定
  return globalObject_->setProperty(name, value);
}

Value* Context::evaluateScript(const std::string& script,
                               const std::string& fileName,
                               int lineNumber) {
  // 現在の例外をクリア
  clearLastException();

  try {
    // レキサーにソースコードをセット
    lexer_->setSource(script, fileName, lineNumber);

    // パース
    auto* ast = parser_->parse();
    if (!ast) {
      // パースエラーの場合、例外は既にセットされている
      return Value::createUndefined(this);
    }

    // バイトコード生成
    auto* bytecode = bytecodeGenerator_->generate(ast);
    if (!bytecode) {
      // バイトコード生成エラーの場合、例外は既にセットされている
      return Value::createUndefined(this);
    }

    // JITコンパイル対象かどうかを確認
    auto* engine = Engine::getInstance();
    if (engine->isJITEnabled() && jitCompiler_ &&
        bytecode->getExecutionCount() >= engine->getJITThreshold()) {
      // JITコンパイル実行
      auto* jitCode = jitCompiler_->compile(bytecode);
      if (jitCode) {
        // JITコンパイル成功した場合はJITコードを実行
        return jitCompiler_->execute(jitCode);
      }
      // JITコンパイル失敗した場合はインタプリタにフォールバック
    }

    // インタプリタでバイトコードを実行
    return interpreter_->execute(bytecode);
  } catch (const std::exception& e) {
    // 未処理の例外を捕捉
    setErrorException("InternalError", e.what());
    return Value::createUndefined(this);
  }
}

Value* Context::evaluateScriptFile(const std::string& fileName) {
  // ファイルを読み込む
  std::string content;
  try {
    content = utils::file::readTextFile(fileName);
  } catch (const std::exception& e) {
    setErrorException("FileError", std::string("Failed to read file: ") + e.what());
    return Value::createUndefined(this);
  }

  // 読み込んだコードを評価
  return evaluateScript(content, fileName, 1);
}

Value* Context::callFunction(Function* func, Object* thisObj, Value** args, int argCount) {
  if (!func) {
    setErrorException("TypeError", "Function is null or undefined");
    return Value::createUndefined(this);
  }

  // this値がnullの場合はグローバルオブジェクトを使用
  if (!thisObj) {
    thisObj = globalObject_;
  }

  // 関数呼び出し用の新しいスコープをプッシュ
  auto* engine = Engine::getInstance();
  auto* allocator = engine->getMemoryAllocator();
  auto* callScope = new (allocator->allocate(sizeof(Scope))) Scope(this, currentScope_, thisObj);
  pushScope(callScope);

  // 関数を実行
  Value* result = nullptr;
  try {
    result = func->call(thisObj, args, argCount);
  } catch (const std::exception& e) {
    // 未処理の例外を捕捉
    setErrorException("InternalError", e.what());
    result = Value::createUndefined(this);
  }

  // スコープをポップ
  popScope();

  // スコープオブジェクトを解放
  callScope->~Scope();
  allocator->deallocate(callScope);

  return result;
}

Value* Context::callMethod(Object* obj, const std::string& methodName, Value** args, int argCount) {
  if (!obj) {
    setErrorException("TypeError", "Cannot call method on null or undefined");
    return Value::createUndefined(this);
  }

  // オブジェクトからメソッドを取得
  auto* method = obj->getProperty(methodName);
  if (!method || !method->isFunction()) {
    setErrorException("TypeError", "Property '" + methodName + "' is not a function");
    return Value::createUndefined(this);
  }

  // 関数として呼び出し
  return callFunction(method->asFunction(), obj, args, argCount);
}

void Context::collectGarbage(bool force) {
  // エンジンのGCを呼び出す
  // 実際のGC処理はエンジンが管理するGCシステムに委譲
  Engine::getInstance()->collectGarbage(this, force);
}

bool Context::setContextData(const std::string& key, void* data, ContextDataCleaner cleaner) {
  std::lock_guard<std::mutex> lock(contextDataMutex_);

  // 既存のデータがある場合はクリーナー関数を使ってクリーンアップ
  auto it = contextData_.find(key);
  if (it != contextData_.end()) {
    if (it->second.cleaner) {
      it->second.cleaner(it->second.data);
    }
  }

  // 新しいデータを登録
  ContextDataEntry entry = {data, cleaner};
  contextData_[key] = entry;
  return true;
}

void* Context::getContextData(const std::string& key) const {
  std::lock_guard<std::mutex> lock(contextDataMutex_);

  auto it = contextData_.find(key);
  if (it != contextData_.end()) {
    return it->second.data;
  }
  return nullptr;
}

bool Context::removeContextData(const std::string& key) {
  std::lock_guard<std::mutex> lock(contextDataMutex_);

  auto it = contextData_.find(key);
  if (it != contextData_.end()) {
    if (it->second.cleaner) {
      it->second.cleaner(it->second.data);
    }
    contextData_.erase(it);
    return true;
  }
  return false;
}

void Context::reset() {
  // 現在の例外状態をクリア
  clearLastException();

  // グローバルスコープ以外のスコープをすべて破棄
  while (currentScope_->getParent() != nullptr) {
    popScope();
  }

  // シンボルテーブルをリセット
  symbolTable_->reset();

  // インタプリタとJITコンパイラの状態をリセット
  interpreter_->reset();
  if (jitCompiler_) {
    jitCompiler_->reset();
  }

  // 強制的にガベージコレクションを実行して未使用メモリを回収
  collectGarbage(true);

  // グローバルオブジェクトを保持したまま再初期化
  globalObject_->reset();
  initializeBuiltins();
}

void Context::pushScope(Scope* scope) {
  if (scope) {
    scope->setParent(currentScope_);
    currentScope_ = scope;
    
    // スコープチェーンの深さが最大値を超えないか確認
    int depth = 0;
    Scope* current = currentScope_;
    while (current) {
      depth++;
      current = current->getParent();
    }
    
    if (depth > MAX_SCOPE_DEPTH) {
      throw RuntimeException("最大スコープ深度を超えました。再帰が深すぎる可能性があります。");
    }
  }
}

void Context::popScope() {
  if (currentScope_ && currentScope_->getParent()) {
    auto* parent = currentScope_->getParent();
    currentScope_ = parent;
  }
}

void Context::initializeBuiltins() {
  // ビルトインオブジェクトの初期化
  auto* engine = Engine::getInstance();
  auto* allocator = engine->getMemoryAllocator();
  
  // console オブジェクトの初期化
  auto* consoleObj = new (allocator->allocate(sizeof(Object))) Object(this);
  
  // console.log 関数の実装
  auto consoleLogFunc = [](Context* ctx, int argc, Value** argv) -> Value* {
    std::stringstream ss;
    for (int i = 0; i < argc; i++) {
      if (i > 0) ss << " ";
      if (argv[i]) {
        ss << argv[i]->toString();
      } else {
        ss << "undefined";
      }
    }
    printf("%s\n", ss.str().c_str());
    return Value::createUndefined(ctx);
  };
  
  // console.error 関数の実装
  auto consoleErrorFunc = [](Context* ctx, int argc, Value** argv) -> Value* {
    std::stringstream ss;
    ss << "\x1B[31m"; // 赤色のANSIエスケープコード
    for (int i = 0; i < argc; i++) {
      if (i > 0) ss << " ";
      if (argv[i]) {
        ss << argv[i]->toString();
      } else {
        ss << "undefined";
      }
    }
    ss << "\x1B[0m"; // リセット
    fprintf(stderr, "%s\n", ss.str().c_str());
    return Value::createUndefined(ctx);
  };
  
  // console.warn 関数の実装
  auto consoleWarnFunc = [](Context* ctx, int argc, Value** argv) -> Value* {
    std::stringstream ss;
    ss << "\x1B[33m"; // 黄色のANSIエスケープコード
    for (int i = 0; i < argc; i++) {
      if (i > 0) ss << " ";
      if (argv[i]) {
        ss << argv[i]->toString();
      } else {
        ss << "undefined";
      }
    }
    ss << "\x1B[0m"; // リセット
    fprintf(stderr, "%s\n", ss.str().c_str());
    return Value::createUndefined(ctx);
  };
  
  // console.info 関数の実装
  auto consoleInfoFunc = [](Context* ctx, int argc, Value** argv) -> Value* {
    std::stringstream ss;
    ss << "\x1B[36m"; // シアンのANSIエスケープコード
    for (int i = 0; i < argc; i++) {
      if (i > 0) ss << " ";
      if (argv[i]) {
        ss << argv[i]->toString();
      } else {
        ss << "undefined";
      }
    }
    ss << "\x1B[0m"; // リセット
    printf("%s\n", ss.str().c_str());
    return Value::createUndefined(ctx);
  };

  // console オブジェクトに各メソッドを追加
  auto* logFunc = createFunction("log", consoleLogFunc, -1);
  auto* errorFunc = createFunction("error", consoleErrorFunc, -1);
  auto* warnFunc = createFunction("warn", consoleWarnFunc, -1);
  auto* infoFunc = createFunction("info", consoleInfoFunc, -1);
  
  consoleObj->setProperty("log", Value::fromObject(logFunc));
  consoleObj->setProperty("error", Value::fromObject(errorFunc));
  consoleObj->setProperty("warn", Value::fromObject(warnFunc));
  consoleObj->setProperty("info", Value::fromObject(infoFunc));
  
  // グローバルオブジェクトにconsoleオブジェクトを追加
  globalObject_->setProperty("console", Value::fromObject(consoleObj));

  // タイマー関連の関数を実装
  auto setTimeoutFunc = [](Context* ctx, int argc, Value** argv) -> Value* {
    // 引数の検証
    if (argc < 1) {
      ctx->setErrorException("TypeError", "setTimeout関数には少なくとも1つの引数が必要です");
      return Value::createUndefined(ctx);
    }
    
    // 第1引数がコールバック関数であることを確認
    if (!argv[0]->isFunction() && !argv[0]->isString()) {
      ctx->setErrorException("TypeError", "setTimeout関数の第1引数は関数またはコード文字列である必要があります");
      return Value::createUndefined(ctx);
    }
    
    // 遅延時間の取得（デフォルトは0ms）
    int delay = 0;
    if (argc >= 2 && argv[1]->isNumber()) {
      delay = std::max(0, static_cast<int>(argv[1]->toNumber()));
    }
    
    // コールバック関数と追加引数の処理
    Value* callbackValue = argv[0];
    Function* callback = nullptr;
    std::string codeString;
    
    if (callbackValue->isFunction()) {
      callback = callbackValue->asFunction();
    } else if (callbackValue->isString()) {
      // 文字列の場合は後でevalで実行するために保存
      codeString = callbackValue->toString();
    }
    
    // 追加引数の収集
    std::vector<Value*> callbackArgs;
    for (int i = 2; i < argc; i++) {
      // 引数の参照カウントを増やして安全に保持
      Value* argCopy = ctx->copyValue(argv[i]);
      callbackArgs.push_back(argCopy);
    }
    
    // タイマーIDを生成（一意のIDを保証）
    static std::atomic<uint32_t> nextTimerId{1};
    uint32_t timerId = nextTimerId.fetch_add(1, std::memory_order_relaxed);
    
    // タイマーをエンジンに登録
    if (callback) {
      Engine::getInstance()->registerTimer(ctx, timerId, callback, callbackArgs, delay);
    } else {
      // 文字列コードの場合
      Engine::getInstance()->registerCodeTimer(ctx, timerId, codeString, delay);
    }
    
    // タイマーIDを返す（数値型）
    return Value::createNumber(ctx, static_cast<double>(timerId));
  };
  
  auto clearTimeoutFunc = [](Context* ctx, int argc, Value** argv) -> Value* {
    if (argc < 1 || !argv[0]->isNumber()) {
      ctx->setErrorException("TypeError", "clearTimeout関数の引数はタイマーIDである必要があります");
      return Value::createUndefined(ctx);
    }
    
    uint32_t timerId = static_cast<uint32_t>(argv[0]->toNumber());
    Engine::getInstance()->cancelTimer(timerId);
    
    return Value::createUndefined(ctx);
  };
  
  auto setIntervalFunc = [](Context* ctx, int argc, Value** argv) -> Value* {
    if (argc < 2 || !argv[0]->isFunction()) {
      ctx->setErrorException("TypeError", "setInterval関数の第1引数は関数である必要があります");
      return Value::createUndefined(ctx);
    }
    
    int interval = 0;
    if (argc >= 2 && argv[1]->isNumber()) {
      interval = static_cast<int>(argv[1]->toNumber());
    }
    
    // コールバック関数と引数を保存
    Function* callback = argv[0]->asFunction();
    std::vector<Value*> callbackArgs;
    for (int i = 2; i < argc; i++) {
      callbackArgs.push_back(argv[i]);
    }
    
    // タイマーIDを生成
    static uint32_t nextTimerId = 1000000; // setTimeoutと区別するため大きな値から開始
    uint32_t timerId = nextTimerId++;
    
    // 定期タイマーを登録
    Engine::getInstance()->registerInterval(ctx, timerId, callback, callbackArgs, interval);
    
    return Value::createNumber(ctx, timerId);
  };
  
  auto clearIntervalFunc = [](Context* ctx, int argc, Value** argv) -> Value* {
    if (argc < 1 || !argv[0]->isNumber()) {
      ctx->setErrorException("TypeError", "clearInterval関数の引数はタイマーIDである必要があります");
      return Value::createUndefined(ctx);
    }
    
    uint32_t timerId = static_cast<uint32_t>(argv[0]->toNumber());
    Engine::getInstance()->cancelTimer(timerId);
    
    return Value::createUndefined(ctx);
  };

  // グローバル関数として登録
  registerGlobalFunction("setTimeout", setTimeoutFunc, 2);
  registerGlobalFunction("clearTimeout", clearTimeoutFunc, 1);
  registerGlobalFunction("setInterval", setIntervalFunc, 2);
  registerGlobalFunction("clearInterval", clearIntervalFunc, 1);

  // 標準ビルトインオブジェクトの初期化
  initializeObjectPrototype();
  initializeFunctionPrototype();
  initializeArrayPrototype();
  initializeStringPrototype();
  initializeNumberPrototype();
  initializeBooleanPrototype();
  initializeDatePrototype();
  initializeRegExpPrototype();
  initializeErrorPrototypes();
  initializeMathObject();
  initializeJSONObject();
  initializePromisePrototype();
  
  // ES6+ 機能の初期化
  initializeMapPrototype();
  initializeSetPrototype();
  initializeWeakMapPrototype();
  initializeWeakSetPrototype();
  initializeSymbolPrototype();
  initializeTypedArrayPrototypes();
  initializeReflectObject();
  initializeProxyConstructor();
}

}  // namespace core
}  // namespace aerojs