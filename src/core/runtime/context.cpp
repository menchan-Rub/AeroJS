/**
 * @file context.cpp
 * @brief JavaScript実行コンテキストの実装
 * @version 0.1.0
 * @license MIT
 */

#include "context.h"
#include "values/value.h"
#include "values/object.h"
#include "values/function.h"
#include "values/string.h"
#include "values/array.h"
#include "builtins/builtins_manager.h"
#include "../engine.h"

#include <stdexcept>
#include <chrono>

namespace aerojs {
namespace core {

// 静的メンバの初期化
thread_local Context* Context::currentContext_ = nullptr;

ContextOptions Context::getDefaultOptions() {
  ContextOptions options;
  options.maxStackSize = 1024 * 1024;  // 1MB
  options.enableExceptions = true;
  options.strictMode = false;
  options.enableDebugger = false;
  options.timezone = "UTC";
  options.locale = "en-US";
  options.maxExecutionTime = 0;  // 無制限
  options.maxMemoryUsage = 0;    // 無制限
  options.maxAllocations = 0;    // 無制限
  options.enableJIT = true;
  options.jitThreshold = 100;
  options.optimizationLevel = 2;
  return options;
}

Context::Context() : Context(getDefaultOptions()) {
}

Context::Context(const ContextOptions& options)
    : options_(options), globalObject_(nullptr), lastException_(Value::createUndefined()),
      stackSize_(0), allocationCount_(0), executionStartTime_(0),
      isExecuting_(false), shouldAbort_(false) {
  
  initialize();
}

Context::~Context() {
  // グローバルオブジェクトとビルトイン関数の解放
  if (globalObject_) {
    globalObject_->unref();
  }
  
  // カスタムデータのクリーンアップ
  for (auto& pair : contextData_) {
    if (pair.second) {
      // TODO: カスタムデータのクリーナーがあれば呼び出す
      delete static_cast<char*>(pair.second);
    }
  }
  contextData_.clear();
}

const ContextOptions& Context::getOptions() const {
  return options_;
}

void Context::setOptions(const ContextOptions& options) {
  options_ = options;
  
  // 動的に変更可能なオプションの適用
  if (globalObject_) {
    // strictModeの変更など
  }
}

Object* Context::getGlobalObject() const {
  return globalObject_;
}

bool Context::isStrictMode() const {
  return options_.strictMode;
}

void Context::setStrictMode(bool strict) {
  options_.strictMode = strict;
}

Value Context::getLastException() const {
  return lastException_;
}

void Context::clearLastException() {
  if (lastException_.getRefCount() > 0) {
    lastException_.unref();
  }
  lastException_ = Value::createUndefined();
}

void Context::setLastException(const Value& exception) {
  clearLastException();
  lastException_ = exception;
  if (!lastException_.isUndefined()) {
    lastException_.ref();
  }
}

Value Context::evaluateScript(const std::string& source, const std::string& filename) {
  // 実行時間制限チェック
  if (options_.maxExecutionTime > 0) {
    executionStartTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
  }
  
  // 現在のコンテキストを設定
  Context* previousContext = currentContext_;
  currentContext_ = this;
  isExecuting_ = true;
  shouldAbort_ = false;
  
  try {
    // TODO: パーサーとインタープリターの実装
    // 現在は簡単な実装として文字列評価を行う
    
    // 基本的な式評価のサンプル実装
    if (source == "undefined") {
      return Value::createUndefined();
    } else if (source == "null") {
      return Value::createNull();
    } else if (source == "true") {
      return Value::createBoolean(true);
    } else if (source == "false") {
      return Value::createBoolean(false);
    }
    
    // 数値の簡単なパース
    try {
      double number = std::stod(source);
      return Value::createNumber(number);
    } catch (...) {
      // 数値でない場合は文字列として処理
    }
    
    // グローバル変数の参照
    if (globalObject_->has(source)) {
      return globalObject_->get(source);
    }
    
    // 文字列リテラルの簡単な処理
    if (source.size() >= 2 && source.front() == '"' && source.back() == '"') {
      std::string content = source.substr(1, source.size() - 2);
      return Value::createString(content);
    }
    
    // 簡単な足し算の処理
    size_t plusPos = source.find(" + ");
    if (plusPos != std::string::npos) {
      std::string left = source.substr(0, plusPos);
      std::string right = source.substr(plusPos + 3);
      
      Value leftVal = evaluateScript(left, filename);
      Value rightVal = evaluateScript(right, filename);
      
      if (leftVal.isNumber() && rightVal.isNumber()) {
        return Value::createNumber(leftVal.asNumber() + rightVal.asNumber());
      }
    }
    
    // デフォルトでundefinedを返す
    return Value::createUndefined();
    
  } catch (const std::exception& e) {
    // 例外をコンテキストに設定
    Value errorValue = Value::createString(e.what());
    setLastException(errorValue);
    return Value::createUndefined();
  }
  
  // コンテキストの復元
  isExecuting_ = false;
  currentContext_ = previousContext;
}

Function* Context::createFunction(const std::string& name, 
                                 std::function<Value*(Context*, const std::vector<Value*>&)> impl,
                                 uint32_t length) {
  
  auto nativeFunc = [impl](Context* ctx, const std::vector<Value*>& args) -> Value* {
    return impl(ctx, args);
  };
  
  return Function::createNative(name, nativeFunc, length);
}

bool Context::registerGlobalFunction(const std::string& name, 
                                    std::function<Value*(Context*, const std::vector<Value*>&)> impl,
                                    uint32_t length) {
  if (!globalObject_) {
    return false;
  }
  
  Function* func = createFunction(name, impl, length);
  if (!func) {
    return false;
  }
  
  Value funcValue = Value::createFunction(func);
  return globalObject_->set(name, &funcValue);
}

bool Context::registerGlobalObject(const std::string& name, Object* object) {
  if (!globalObject_ || !object) {
    return false;
  }
  
  Value objValue = Value::createObject(object);
  return globalObject_->set(name, &objValue);
}

bool Context::registerGlobalValue(const std::string& name, const Value& value) {
  if (!globalObject_) {
    return false;
  }
  
  return globalObject_->set(name, &const_cast<Value&>(value));
}

Value Context::getGlobalProperty(const std::string& name) const {
  if (!globalObject_) {
    return Value::createUndefined();
  }
  
  if (globalObject_->has(name)) {
    Value* prop = globalObject_->get(name);
    return prop ? *prop : Value::createUndefined();
  }
  
  return Value::createUndefined();
}

bool Context::setGlobalProperty(const std::string& name, const Value& value) {
  if (!globalObject_) {
    return false;
  }
  
  return globalObject_->set(name, &const_cast<Value&>(value));
}

bool Context::deleteGlobalProperty(const std::string& name) {
  if (!globalObject_) {
    return false;
  }
  
  return globalObject_->deleteProperty(PropertyKey(name));
}

bool Context::hasGlobalProperty(const std::string& name) const {
  if (!globalObject_) {
    return false;
  }
  
  return globalObject_->has(name);
}

void Context::setCustomData(const std::string& key, void* data) {
  std::lock_guard<std::mutex> lock(contextDataMutex_);
  contextData_[key] = data;
}

void* Context::getCustomData(const std::string& key) const {
  std::lock_guard<std::mutex> lock(contextDataMutex_);
  auto it = contextData_.find(key);
  return (it != contextData_.end()) ? it->second : nullptr;
}

void Context::removeCustomData(const std::string& key) {
  std::lock_guard<std::mutex> lock(contextDataMutex_);
  contextData_.erase(key);
}

Context* Context::getCurrentContext() {
  return currentContext_;
}

void Context::checkLimits() {
  // 実行時間制限チェック
  if (options_.maxExecutionTime > 0 && isExecuting_) {
    uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    if (currentTime - executionStartTime_ > options_.maxExecutionTime) {
      shouldAbort_ = true;
      throw std::runtime_error("Execution time limit exceeded");
    }
  }
  
  // メモリ使用量制限チェック
  if (options_.maxMemoryUsage > 0) {
    // TODO: メモリ使用量の実際の測定
    // 現在は簡単なアロケーション回数チェック
    if (allocationCount_ > options_.maxAllocations && options_.maxAllocations > 0) {
      throw std::runtime_error("Memory allocation limit exceeded");
    }
  }
  
  // スタックサイズ制限チェック
  if (stackSize_ > options_.maxStackSize) {
    throw std::runtime_error("Stack overflow");
  }
}

void Context::initialize() {
  // グローバルオブジェクトの作成
  globalObject_ = Object::create();
  if (globalObject_) {
    globalObject_->ref();
  }
  
  // ビルトインオブジェクトの初期化
  initializeBuiltins();
  
  // 初期設定
  stackSize_ = 0;
  allocationCount_ = 0;
  executionStartTime_ = 0;
  isExecuting_ = false;
  shouldAbort_ = false;
}

void Context::initializeBuiltins() {
  if (!globalObject_) {
    return;
  }
  
  // BuiltinsManagerを使用してビルトインオブジェクトを初期化
  BuiltinsManager* builtinsManager = new BuiltinsManager(this);
  
  // グローバルオブジェクトにビルトイン関数とオブジェクトを登録
  builtinsManager->initializeGlobalObject(globalObject_);
  
  // よく使用される基本的なオブジェクトを直接登録
  
  // undefined, null は Value で管理
  Value undefinedVal = Value::createUndefined();
  Value nullVal = Value::createNull();
  globalObject_->set("undefined", &undefinedVal);
  globalObject_->set("null", &nullVal);
  
  // グローバル関数の登録
  registerGlobalFunction("eval", [](Context* ctx, const std::vector<Value*>& args) -> Value* {
    if (args.empty() || !args[0] || !args[0]->isString()) {
      return Value::createUndefined();
    }
    
    String* codeStr = static_cast<String*>(args[0]->asString());
    std::string code = codeStr->value();
    
    Value result = ctx->evaluateScript(code, "<eval>");
    return Value::createCopy(result);
  }, 1);
  
  registerGlobalFunction("isNaN", [](Context* ctx, const std::vector<Value*>& args) -> Value* {
    if (args.empty() || !args[0]) {
      return Value::createBoolean(true);
    }
    
    if (!args[0]->isNumber()) {
      return Value::createBoolean(true);
    }
    
    double num = args[0]->asNumber();
    return Value::createBoolean(std::isnan(num));
  }, 1);
  
  registerGlobalFunction("isFinite", [](Context* ctx, const std::vector<Value*>& args) -> Value* {
    if (args.empty() || !args[0] || !args[0]->isNumber()) {
      return Value::createBoolean(false);
    }
    
    double num = args[0]->asNumber();
    return Value::createBoolean(std::isfinite(num));
  }, 1);
  
  registerGlobalFunction("parseInt", [](Context* ctx, const std::vector<Value*>& args) -> Value* {
    if (args.empty() || !args[0]) {
      return Value::createNumber(NAN);
    }
    
    std::string str;
    if (args[0]->isString()) {
      String* strObj = static_cast<String*>(args[0]->asString());
      str = strObj->value();
    } else if (args[0]->isNumber()) {
      str = std::to_string(args[0]->asNumber());
    } else {
      return Value::createNumber(NAN);
    }
    
    int radix = 10;
    if (args.size() > 1 && args[1] && args[1]->isNumber()) {
      radix = static_cast<int>(args[1]->asNumber());
      if (radix < 2 || radix > 36) {
        return Value::createNumber(NAN);
      }
    }
    
    try {
      long long result = std::stoll(str, nullptr, radix);
      return Value::createNumber(static_cast<double>(result));
    } catch (...) {
      return Value::createNumber(NAN);
    }
  }, 2);
  
  registerGlobalFunction("parseFloat", [](Context* ctx, const std::vector<Value*>& args) -> Value* {
    if (args.empty() || !args[0]) {
      return Value::createNumber(NAN);
    }
    
    std::string str;
    if (args[0]->isString()) {
      String* strObj = static_cast<String*>(args[0]->asString());
      str = strObj->value();
    } else if (args[0]->isNumber()) {
      return args[0];  // 既に数値の場合はそのまま返す
    } else {
      return Value::createNumber(NAN);
    }
    
    try {
      double result = std::stod(str);
      return Value::createNumber(result);
    } catch (...) {
      return Value::createNumber(NAN);
    }
  }, 1);
}

// ファクトリ関数
Context* createContext() {
  return new Context();
}

Context* createContext(const ContextOptions& options) {
  return new Context(options);
}

}  // namespace core
}  // namespace aerojs 