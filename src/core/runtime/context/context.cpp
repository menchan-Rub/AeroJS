/**
 * @file context.cpp
 * @brief JavaScript実行コンテキストの実装
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#include "context.h"

#include <stdexcept>

#include "../global_object.h"
#include "../object.h"
#include "../values/error.h"
#include "../values/function.h"
#include "../values/string.h"
#include "../values/symbol.h"
#include "../values/value.h"

namespace aero {

ExecutionContext::ExecutionContext()
    : m_type(Type::Global), m_globalObject(nullptr), m_variableObject(nullptr), m_strictMode(false), m_hasConsole(true), m_hasModules(true), m_sharedArrayBuffersEnabled(false), m_locale("en-US"), m_isRunning(false) {
  initialize();
}

ExecutionContext::ExecutionContext(Type type)
    : m_type(type), m_globalObject(nullptr), m_variableObject(nullptr), m_strictMode(false), m_hasConsole(true), m_hasModules(true), m_sharedArrayBuffersEnabled(false), m_locale("en-US"), m_isRunning(false) {
  initialize();
}

ExecutionContext::~ExecutionContext() {
  // グローバルオブジェクトや変数オブジェクトの解放は
  // ガベージコレクタによって管理されるため、ここでは明示的な解放は行わない
  // ただし、コンテキスト固有のリソースがあれば解放する
  m_scopeChain.clear();
  m_moduleMap.clear();
  m_pendingJobs.clear();

  // デバッグ情報のクリーンアップ
  if (m_debugInfo) {
    delete m_debugInfo;
    m_debugInfo = nullptr;
  }
}

void ExecutionContext::initialize() {
  if (m_type == Type::Global) {
    // グローバルコンテキストの場合は自動的にグローバルオブジェクトを作成
    m_globalObject = createGlobalObject();
    m_variableObject = m_globalObject;

    // グローバルオブジェクトをスコープチェーンに追加
    m_scopeChain.push_back(m_globalObject);

    // thisをグローバルオブジェクトに設定
    m_thisValue = Value(m_globalObject);

    // 実行環境の初期化
    initializeRuntimeEnvironment();
  } else if (m_type == Type::Function) {
    // 関数コンテキストの場合は変数オブジェクトを作成
    m_variableObject = new ActivationObject(this);

    // スコープチェーンは呼び出し元から継承される
    // thisは呼び出し時に設定される
  } else if (m_type == Type::Module) {
    // モジュールコンテキストの場合は専用の環境レコードを作成
    m_variableObject = new ModuleEnvironmentRecord(this);

    // モジュールのthisはundefined
    m_thisValue = Value::undefined();

    // モジュールスコープを設定
    if (m_globalObject) {
      m_scopeChain.push_back(m_variableObject);
      m_scopeChain.push_back(m_globalObject);
    }
  } else if (m_type == Type::Eval) {
    // evalコンテキストは呼び出し元のコンテキストから設定を継承
    // 変数オブジェクトとスコープチェーンは呼び出し元から設定される
  }

  // 実行時オプションの初期化
  m_optimizationLevel = OptimizationLevel::Normal;
  m_debugInfo = nullptr;
  m_isRunning = false;
  m_isAborting = false;
  m_executionTimeLimit = 0;  // 無制限
  m_memoryLimit = 0;         // 無制限

  // 国際化サポートの初期化
  if (m_locale.empty()) {
    m_locale = "en-US";
  }
  initializeICU();
}

ExecutionContext::Type ExecutionContext::type() const {
  return m_type;
}

Object* ExecutionContext::globalObject() const {
  return m_globalObject;
}

Object* ExecutionContext::createGlobalObject() {
  // 新しいグローバルオブジェクトを作成
  m_globalObject = new GlobalObject(this);

  // 組み込みオブジェクトの初期化
  initializeBuiltins(m_globalObject);

  return m_globalObject;
}

void ExecutionContext::initializeBuiltins(Object* globalObj) {
  // 組み込みオブジェクトの初期化
  // Object, Function, Array, String, Number, Boolean, Date, RegExp, Error, Math, JSON
  // などの標準オブジェクトを初期化

  // ECMAScript 仕様に準拠した組み込みオブジェクトの設定
  initializeObjectConstructor(globalObj);
  initializeFunctionConstructor(globalObj);
  initializeArrayConstructor(globalObj);
  initializeStringConstructor(globalObj);
  initializeNumberConstructor(globalObj);
  initializeBooleanConstructor(globalObj);
  initializeDateConstructor(globalObj);
  initializeRegExpConstructor(globalObj);
  initializeErrorConstructors(globalObj);
  initializeMathObject(globalObj);
  initializeJSONObject(globalObj);
  initializeSymbolConstructor(globalObj);
  initializePromiseConstructor(globalObj);

  // ES6+ 機能の初期化
  initializeMapConstructor(globalObj);
  initializeSetConstructor(globalObj);
  initializeWeakMapConstructor(globalObj);
  initializeWeakSetConstructor(globalObj);
  initializeTypedArrays(globalObj);
  initializeReflectObject(globalObj);
  initializeProxyConstructor(globalObj);

  // コンソール API (オプション)
  if (m_hasConsole) {
    initializeConsoleObject(globalObj);
  }

  // モジュールサポート (オプション)
  if (m_hasModules) {
    initializeModuleSystem(globalObj);
  }

  // SharedArrayBuffer (オプション)
  if (m_sharedArrayBuffersEnabled) {
    initializeSharedArrayBuffer(globalObj);
    initializeAtomics(globalObj);
  }
}

void ExecutionContext::initializeRuntimeEnvironment() {
  // JITコンパイラの初期化
  initializeJIT();

  // ガベージコレクタの初期化
  initializeGC();

// プロファイラの初期化（デバッグビルドのみ）
#ifdef AERO_DEBUG
  initializeProfiler();
#endif

  // 実行時最適化の設定
  configureOptimizations(m_optimizationLevel);
}

Object* ExecutionContext::variableObject() const {
  return m_variableObject;
}

void ExecutionContext::setVariableObject(Object* obj) {
  if (!obj) {
    throw std::invalid_argument("変数オブジェクトにnullを設定することはできません");
  }
  m_variableObject = obj;
}

const std::vector<Object*>& ExecutionContext::scopeChain() const {
  return m_scopeChain;
}

void ExecutionContext::pushScope(Object* obj) {
  if (!obj) {
    throw std::invalid_argument("スコープチェーンにnullを追加することはできません");
  }
  m_scopeChain.push_back(obj);
}

void ExecutionContext::popScope() {
  if (!m_scopeChain.empty()) {
    m_scopeChain.pop_back();
  } else {
    throw std::runtime_error("空のスコープチェーンからpopしようとしました");
  }
}

void ExecutionContext::replaceScope(size_t index, Object* obj) {
  if (index >= m_scopeChain.size()) {
    throw std::out_of_range("スコープチェーンのインデックスが範囲外です");
  }
  if (!obj) {
    throw std::invalid_argument("スコープチェーンにnullを設定することはできません");
  }
  m_scopeChain[index] = obj;
}

Value ExecutionContext::thisValue() const {
  return m_thisValue;
}

void ExecutionContext::setThisValue(Value value) {
  m_thisValue = value;
}

Object* ExecutionContext::createObject() {
  return new Object(this);
}

String* ExecutionContext::createString(const std::string& value) {
  // 文字列の重複排除のためのインターン処理
  auto it = m_stringTable.find(value);
  if (it != m_stringTable.end()) {
    return it->second;
  }

  String* str = new String(this, value);
  m_stringTable[value] = str;
  return str;
}

Symbol* ExecutionContext::createSymbol(const std::string& description) {
  Symbol* symbol = new Symbol(this, description);
  // シンボルレジストリに登録（必要に応じて）
  return symbol;
}

Function* ExecutionContext::createFunction(const std::string& name, int paramCount) {
  return new Function(this, name, paramCount);
}

Value ExecutionContext::evaluateScript(const std::string& code, const std::string& fileName) {
  if (code.empty()) {
    return Value::undefined();
  }

  // 実行中フラグを設定
  bool wasRunning = m_isRunning;
  m_isRunning = true;

  // 実行時間制限の設定（設定されている場合）
  Timer executionTimer;
  if (m_executionTimeLimit > 0) {
    executionTimer.start(m_executionTimeLimit, [this]() {
      this->m_isAborting = true;
    });
  }

  try {
    // パース段階
    std::unique_ptr<ASTNode> ast = Parser::parse(code, fileName, this);

    // 構文解析エラーチェック
    if (!ast) {
      throw SyntaxError(this, "構文解析エラー");
    }

    // 意味解析段階
    SemanticAnalyzer analyzer(this);
    analyzer.analyze(ast.get());

    // コード生成段階
    BytecodeGenerator generator(this);
    std::unique_ptr<BytecodeModule> bytecode = generator.generate(ast.get());

    // 最適化段階
    if (m_optimizationLevel >= OptimizationLevel::Normal) {
      BytecodeOptimizer optimizer(this, m_optimizationLevel);
      optimizer.optimize(bytecode.get());
    }

    // 実行段階
    Interpreter interpreter(this);
    Value result = interpreter.execute(bytecode.get());

    // JIT コンパイル（ホットスポット検出後）
    if (m_optimizationLevel >= OptimizationLevel::JIT &&
        bytecode->executionCount() > JIT_THRESHOLD) {
      JITCompiler jit(this);
      jit.compile(bytecode.get());
    }

    // 実行中フラグを元に戻す
    m_isRunning = wasRunning;

    // タイマーを停止
    if (m_executionTimeLimit > 0) {
      executionTimer.stop();
    }

    return result;
  } catch (const std::exception& e) {
    // 実行中フラグを元に戻す
    m_isRunning = wasRunning;

    // タイマーを停止
    if (m_executionTimeLimit > 0) {
      executionTimer.stop();
    }

    // エラーを再スロー
    throw;
  }
}

Value ExecutionContext::evaluateModule(const std::string& code, const std::string& fileName) {
  if (!m_hasModules) {
    throw std::runtime_error("モジュールサポートが無効です");
  }

  if (code.empty()) {
    return Value::undefined();
  }

  // 実行中フラグを設定
  bool wasRunning = m_isRunning;
  m_isRunning = true;

  try {
    // モジュールパーサーを使用
    std::unique_ptr<ModuleNode> moduleAST = ModuleParser::parse(code, fileName, this);

    // 構文解析エラーチェック
    if (!moduleAST) {
      throw SyntaxError(this, "モジュール構文解析エラー");
    }

    // モジュールの静的解析（インポート/エクスポート解決）
    ModuleAnalyzer analyzer(this);
    analyzer.analyze(moduleAST.get());

    // モジュール環境の設定
    ModuleEnvironment* moduleEnv = new ModuleEnvironment(this, fileName);

    // インポートの解決
    for (const auto& importDecl : moduleAST->imports()) {
      Object* importedModule = importModule(importDecl.specifier);
      moduleEnv->registerImport(importDecl, importedModule);
    }

    // モジュールコード生成
    ModuleBytecodeGenerator generator(this);
    std::unique_ptr<ModuleBytecode> bytecode = generator.generate(moduleAST.get());

    // モジュール実行
    ModuleInterpreter interpreter(this);
    Value result = interpreter.execute(bytecode.get(), moduleEnv);

    // モジュールキャッシュに登録
    m_moduleMap[fileName] = moduleEnv;

    // 実行中フラグを元に戻す
    m_isRunning = wasRunning;

    return result;
  } catch (const std::exception& e) {
    // 実行中フラグを元に戻す
    m_isRunning = wasRunning;

    // エラーを再スロー
    throw;
  }
}

Object* ExecutionContext::importModule(const std::string& specifier) {
  if (!m_hasModules) {
    throw std::runtime_error("モジュールサポートが無効です");
  }

  // モジュールキャッシュをチェック
  auto it = m_moduleMap.find(specifier);
  if (it != m_moduleMap.end()) {
    return it->second->moduleNamespace();
  }

  // モジュールローダーを使用してモジュールを読み込む
  ModuleLoader loader(this);
  std::string moduleSource;
  std::string resolvedSpecifier;

  try {
    // モジュール解決（相対パスから絶対パスへの変換など）
    resolvedSpecifier = loader.resolveSpecifier(specifier);

    // モジュールの読み込み
    moduleSource = loader.loadModule(resolvedSpecifier);
  } catch (const std::exception& e) {
    throw Error(this, std::string("モジュール読み込みエラー: ") + e.what(), "ModuleError");
  }

  // モジュールの評価
  try {
    evaluateModule(moduleSource, resolvedSpecifier);
  } catch (const std::exception& e) {
    throw Error(this, std::string("モジュール評価エラー: ") + e.what(), "ModuleError");
  }

  // 評価後のモジュールを返す
  it = m_moduleMap.find(resolvedSpecifier);
  if (it != m_moduleMap.end()) {
    return it->second->moduleNamespace();
  }

  throw Error(this, "モジュールのインポートに失敗しました", "ModuleError");
}

Value ExecutionContext::throwTypeError(const std::string& message) {
  throw TypeError(this, message);
  return Value::undefined();
}

Value ExecutionContext::throwError(const std::string& message, const std::string& name) {
  if (name == "TypeError") {
    throw TypeError(this, message);
  } else if (name == "ReferenceError") {
    throw ReferenceError(this, message);
  } else if (name == "SyntaxError") {
    throw SyntaxError(this, message);
  } else if (name == "RangeError") {
    throw RangeError(this, message);
  } else {
    throw Error(this, message, name);
  }
  return Value::undefined();
}

bool ExecutionContext::isStrictMode() const {
  return m_strictMode;
}

void ExecutionContext::setStrictMode(bool strict) {
  m_strictMode = strict;
}

bool ExecutionContext::hasConsole() const {
  return m_hasConsole;
}

bool ExecutionContext::hasModules() const {
  return m_hasModules;
}

bool ExecutionContext::isRunning() const {
  return m_isRunning;
}

bool ExecutionContext::isSharedArrayBuffersEnabled() const {
  return m_sharedArrayBuffersEnabled;
}

void ExecutionContext::setSharedArrayBuffersEnabled(bool enabled) {
  m_sharedArrayBuffersEnabled = enabled;
}

std::string ExecutionContext::getLocale() const {
  return m_locale;
}

void ExecutionContext::setLocale(const std::string& locale) {
  m_locale = locale;
}

void ExecutionContext::initializeBuiltins(Object* globalObj, bool hasConsole, bool hasModules) {
  m_hasConsole = hasConsole;
  m_hasModules = hasModules;

  if (!globalObj) {
    globalObj = m_globalObject;
  }

  if (!globalObj) {
    return;
  }

  // 基本的なビルトインオブジェクトを初期化

  // Object
  auto objectConstructor = new ObjectConstructor(this);
  globalObj->set(this, "Object", Value(objectConstructor));
  m_builtins.objectConstructor = objectConstructor;

  // Function
  auto functionConstructor = new FunctionConstructor(this);
  globalObj->set(this, "Function", Value(functionConstructor));
  m_builtins.functionConstructor = functionConstructor;

  // Array
  auto arrayConstructor = new ArrayConstructor(this);
  globalObj->set(this, "Array", Value(arrayConstructor));
  m_builtins.arrayConstructor = arrayConstructor;

  // String
  auto stringConstructor = new StringConstructor(this);
  globalObj->set(this, "String", Value(stringConstructor));
  m_builtins.stringConstructor = stringConstructor;

  // Number
  auto numberConstructor = new NumberConstructor(this);
  globalObj->set(this, "Number", Value(numberConstructor));
  m_builtins.numberConstructor = numberConstructor;

  // Boolean
  auto booleanConstructor = new BooleanConstructor(this);
  globalObj->set(this, "Boolean", Value(booleanConstructor));
  m_builtins.booleanConstructor = booleanConstructor;

  // Symbol
  auto symbolConstructor = new SymbolConstructor(this);
  globalObj->set(this, "Symbol", Value(symbolConstructor));
  m_builtins.symbolConstructor = symbolConstructor;

  // Date
  auto dateConstructor = new DateConstructor(this);
  globalObj->set(this, "Date", Value(dateConstructor));
  m_builtins.dateConstructor = dateConstructor;

  // RegExp
  auto regExpConstructor = new RegExpConstructor(this);
  globalObj->set(this, "RegExp", Value(regExpConstructor));
  m_builtins.regExpConstructor = regExpConstructor;

  // Promise
  auto promiseConstructor = new PromiseConstructor(this);
  globalObj->set(this, "Promise", Value(promiseConstructor));
  m_builtins.promiseConstructor = promiseConstructor;

  // Map
  auto mapConstructor = new MapConstructor(this);
  globalObj->set(this, "Map", Value(mapConstructor));
  m_builtins.mapConstructor = mapConstructor;

  // Set
  auto setConstructor = new SetConstructor(this);
  globalObj->set(this, "Set", Value(setConstructor));
  m_builtins.setConstructor = setConstructor;

  // WeakMap
  auto weakMapConstructor = new WeakMapConstructor(this);
  globalObj->set(this, "WeakMap", Value(weakMapConstructor));
  m_builtins.weakMapConstructor = weakMapConstructor;

  // WeakSet
  auto weakSetConstructor = new WeakSetConstructor(this);
  globalObj->set(this, "WeakSet", Value(weakSetConstructor));
  m_builtins.weakSetConstructor = weakSetConstructor;

  // ArrayBuffer
  auto arrayBufferConstructor = new ArrayBufferConstructor(this);
  globalObj->set(this, "ArrayBuffer", Value(arrayBufferConstructor));
  m_builtins.arrayBufferConstructor = arrayBufferConstructor;

  // TypedArray関連
  initializeTypedArrays(globalObj);

  // Error階層
  auto errorConstructor = new ErrorConstructor(this);
  globalObj->set(this, "Error", Value(errorConstructor));
  m_builtins.errorConstructor = errorConstructor;

  auto typeErrorConstructor = new TypeErrorConstructor(this);
  globalObj->set(this, "TypeError", Value(typeErrorConstructor));
  m_builtins.typeErrorConstructor = typeErrorConstructor;

  auto rangeErrorConstructor = new RangeErrorConstructor(this);
  globalObj->set(this, "RangeError", Value(rangeErrorConstructor));
  m_builtins.rangeErrorConstructor = rangeErrorConstructor;

  auto referenceErrorConstructor = new ReferenceErrorConstructor(this);
  globalObj->set(this, "ReferenceError", Value(referenceErrorConstructor));
  m_builtins.referenceErrorConstructor = referenceErrorConstructor;

  auto syntaxErrorConstructor = new SyntaxErrorConstructor(this);
  globalObj->set(this, "SyntaxError", Value(syntaxErrorConstructor));
  m_builtins.syntaxErrorConstructor = syntaxErrorConstructor;

  auto evalErrorConstructor = new EvalErrorConstructor(this);
  globalObj->set(this, "EvalError", Value(evalErrorConstructor));
  m_builtins.evalErrorConstructor = evalErrorConstructor;

  auto uriErrorConstructor = new URIErrorConstructor(this);
  globalObj->set(this, "URIError", Value(uriErrorConstructor));
  m_builtins.uriErrorConstructor = uriErrorConstructor;

  auto aggregateErrorConstructor = new AggregateErrorConstructor(this);
  globalObj->set(this, "AggregateError", Value(aggregateErrorConstructor));
  m_builtins.aggregateErrorConstructor = aggregateErrorConstructor;

  // JSON
  auto jsonObject = new JSONObject(this);
  globalObj->set(this, "JSON", Value(jsonObject));
  m_builtins.jsonObject = jsonObject;

  // Math
  auto mathObject = new MathObject(this);
  globalObj->set(this, "Math", Value(mathObject));
  m_builtins.mathObject = mathObject;

  // Reflect
  auto reflectObject = new ReflectObject(this);
  globalObj->set(this, "Reflect", Value(reflectObject));
  m_builtins.reflectObject = reflectObject;

  // Proxy
  auto proxyConstructor = new ProxyConstructor(this);
  globalObj->set(this, "Proxy", Value(proxyConstructor));
  m_builtins.proxyConstructor = proxyConstructor;

  // SharedArrayBuffer (条件付き)
  if (m_sharedArrayBuffersEnabled) {
    auto sharedArrayBufferConstructor = new SharedArrayBufferConstructor(this);
    globalObj->set(this, "SharedArrayBuffer", Value(sharedArrayBufferConstructor));
    m_builtins.sharedArrayBufferConstructor = sharedArrayBufferConstructor;

    auto atomicsObject = new AtomicsObject(this);
    globalObj->set(this, "Atomics", Value(atomicsObject));
    m_builtins.atomicsObject = atomicsObject;
  }

  // Intl (国際化API)
  if (m_intlEnabled) {
    initializeIntl(globalObj);
  }

  // オプション機能

  // Console API
  if (m_hasConsole) {
    auto console = new ConsoleObject(this);
    globalObj->set(this, "console", Value(console));
    m_builtins.consoleObject = console;
  }

  // ES Modules
  if (m_hasModules) {
    initializeModuleSystem(globalObj);
  }

  // グローバル関数の初期化
  initializeGlobalFunctions(globalObj);

  // グローバルプロパティの初期化
  initializeGlobalProperties(globalObj);

  // プロトタイプ連鎖の設定
  setupPrototypeChains();
}

void ExecutionContext::initializeTypedArrays(Object* globalObj) {
  // TypedArray基底クラス (直接アクセスはできない)
  auto typedArrayConstructor = new TypedArrayConstructor(this);
  m_builtins.typedArrayConstructor = typedArrayConstructor;

  // 各TypedArray実装
  auto int8ArrayConstructor = new Int8ArrayConstructor(this, typedArrayConstructor);
  globalObj->set(this, "Int8Array", Value(int8ArrayConstructor));
  m_builtins.int8ArrayConstructor = int8ArrayConstructor;

  auto uint8ArrayConstructor = new Uint8ArrayConstructor(this, typedArrayConstructor);
  globalObj->set(this, "Uint8Array", Value(uint8ArrayConstructor));
  m_builtins.uint8ArrayConstructor = uint8ArrayConstructor;

  auto uint8ClampedArrayConstructor = new Uint8ClampedArrayConstructor(this, typedArrayConstructor);
  globalObj->set(this, "Uint8ClampedArray", Value(uint8ClampedArrayConstructor));
  m_builtins.uint8ClampedArrayConstructor = uint8ClampedArrayConstructor;

  auto int16ArrayConstructor = new Int16ArrayConstructor(this, typedArrayConstructor);
  globalObj->set(this, "Int16Array", Value(int16ArrayConstructor));
  m_builtins.int16ArrayConstructor = int16ArrayConstructor;

  auto uint16ArrayConstructor = new Uint16ArrayConstructor(this, typedArrayConstructor);
  globalObj->set(this, "Uint16Array", Value(uint16ArrayConstructor));
  m_builtins.uint16ArrayConstructor = uint16ArrayConstructor;

  auto int32ArrayConstructor = new Int32ArrayConstructor(this, typedArrayConstructor);
  globalObj->set(this, "Int32Array", Value(int32ArrayConstructor));
  m_builtins.int32ArrayConstructor = int32ArrayConstructor;

  auto uint32ArrayConstructor = new Uint32ArrayConstructor(this, typedArrayConstructor);
  globalObj->set(this, "Uint32Array", Value(uint32ArrayConstructor));
  m_builtins.uint32ArrayConstructor = uint32ArrayConstructor;

  auto float32ArrayConstructor = new Float32ArrayConstructor(this, typedArrayConstructor);
  globalObj->set(this, "Float32Array", Value(float32ArrayConstructor));
  m_builtins.float32ArrayConstructor = float32ArrayConstructor;

  auto float64ArrayConstructor = new Float64ArrayConstructor(this, typedArrayConstructor);
  globalObj->set(this, "Float64Array", Value(float64ArrayConstructor));
  m_builtins.float64ArrayConstructor = float64ArrayConstructor;

  auto bigInt64ArrayConstructor = new BigInt64ArrayConstructor(this, typedArrayConstructor);
  globalObj->set(this, "BigInt64Array", Value(bigInt64ArrayConstructor));
  m_builtins.bigInt64ArrayConstructor = bigInt64ArrayConstructor;

  auto bigUint64ArrayConstructor = new BigUint64ArrayConstructor(this, typedArrayConstructor);
  globalObj->set(this, "BigUint64Array", Value(bigUint64ArrayConstructor));
  m_builtins.bigUint64ArrayConstructor = bigUint64ArrayConstructor;

  // DataView
  auto dataViewConstructor = new DataViewConstructor(this);
  globalObj->set(this, "DataView", Value(dataViewConstructor));
  m_builtins.dataViewConstructor = dataViewConstructor;
}

void ExecutionContext::initializeIntl(Object* globalObj) {
  auto intlObject = new IntlObject(this);
  globalObj->set(this, "Intl", Value(intlObject));
  m_builtins.intlObject = intlObject;

  // Intl.Collator
  auto collatorConstructor = new IntlCollatorConstructor(this);
  intlObject->set(this, "Collator", Value(collatorConstructor));

  // Intl.DateTimeFormat
  auto dateTimeFormatConstructor = new IntlDateTimeFormatConstructor(this);
  intlObject->set(this, "DateTimeFormat", Value(dateTimeFormatConstructor));

  // Intl.NumberFormat
  auto numberFormatConstructor = new IntlNumberFormatConstructor(this);
  intlObject->set(this, "NumberFormat", Value(numberFormatConstructor));

  // Intl.PluralRules
  auto pluralRulesConstructor = new IntlPluralRulesConstructor(this);
  intlObject->set(this, "PluralRules", Value(pluralRulesConstructor));

  // Intl.RelativeTimeFormat
  auto relativeTimeFormatConstructor = new IntlRelativeTimeFormatConstructor(this);
  intlObject->set(this, "RelativeTimeFormat", Value(relativeTimeFormatConstructor));

  // Intl.Locale
  auto localeConstructor = new IntlLocaleConstructor(this);
  intlObject->set(this, "Locale", Value(localeConstructor));

  // Intl.ListFormat
  auto listFormatConstructor = new IntlListFormatConstructor(this);
  intlObject->set(this, "ListFormat", Value(listFormatConstructor));

  // Intl.Segmenter
  auto segmenterConstructor = new IntlSegmenterConstructor(this);
  intlObject->set(this, "Segmenter", Value(segmenterConstructor));
}

void ExecutionContext::initializeGlobalFunctions(Object* globalObj) {
  // eval
  auto evalFunction = new NativeFunction(
      this, "eval", [this](const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isString()) {
          return args.empty() ? Value::undefined() : args[0];
        }

        std::string code = args[0].toString(this);
        return evaluateScript(code, "<eval>", true);
      },
      1);
  globalObj->set(this, "eval", Value(evalFunction));
  m_builtins.evalFunction = evalFunction;

  // isFinite
  auto isFiniteFunction = new NativeFunction(
      this, "isFinite", [this](const std::vector<Value>& args) -> Value {
        if (args.empty()) {
          return Value(false);
        }

        double number = args[0].toNumber(this);
        return Value(std::isfinite(number));
      },
      1);
  globalObj->set(this, "isFinite", Value(isFiniteFunction));

  // isNaN
  auto isNaNFunction = new NativeFunction(
      this, "isNaN", [this](const std::vector<Value>& args) -> Value {
        if (args.empty()) {
          return Value(true);
        }

        double number = args[0].toNumber(this);
        return Value(std::isnan(number));
      },
      1);
  globalObj->set(this, "isNaN", Value(isNaNFunction));

  // parseFloat
  auto parseFloatFunction = new NativeFunction(
      this, "parseFloat", [this](const std::vector<Value>& args) -> Value {
        if (args.empty()) {
          return Value(std::numeric_limits<double>::quiet_NaN());
        }

        std::string str = args[0].toString(this);
        try {
          return Value(std::stod(str));
        } catch (...) {
          return Value(std::numeric_limits<double>::quiet_NaN());
        }
      },
      1);
  globalObj->set(this, "parseFloat", Value(parseFloatFunction));

  // parseInt
  auto parseIntFunction = new NativeFunction(
      this, "parseInt", [this](const std::vector<Value>& args) -> Value {
        if (args.empty()) {
          return Value(std::numeric_limits<double>::quiet_NaN());
        }

        std::string str = args[0].toString(this);
        int radix = 10;

        if (args.size() > 1 && !args[1].isUndefined()) {
          radix = static_cast<int>(args[1].toNumber(this));
          if (radix < 2 || radix > 36) {
            return Value(std::numeric_limits<double>::quiet_NaN());
          }
        }

        try {
          return Value(std::stoll(str, nullptr, radix));
        } catch (...) {
          return Value(std::numeric_limits<double>::quiet_NaN());
        }
      },
      2);
  globalObj->set(this, "parseInt", Value(parseIntFunction));

  // encodeURI
  auto encodeURIFunction = new NativeFunction(
      this, "encodeURI", [this](const std::vector<Value>& args) -> Value {
        if (args.empty()) {
          return Value("undefined");
        }

        std::string uri = args[0].toString(this);
        return Value(encodeURI(uri));
      },
      1);
  globalObj->set(this, "encodeURI", Value(encodeURIFunction));

  // decodeURI
  auto decodeURIFunction = new NativeFunction(
      this, "decodeURI", [this](const std::vector<Value>& args) -> Value {
        if (args.empty()) {
          return Value("undefined");
        }

        std::string uri = args[0].toString(this);
        return Value(decodeURI(uri));
      },
      1);
  globalObj->set(this, "decodeURI", Value(decodeURIFunction));

  // encodeURIComponent
  auto encodeURIComponentFunction = new NativeFunction(
      this, "encodeURIComponent", [this](const std::vector<Value>& args) -> Value {
        if (args.empty()) {
          return Value("undefined");
        }

        std::string component = args[0].toString(this);
        return Value(encodeURIComponent(component));
      },
      1);
  globalObj->set(this, "encodeURIComponent", Value(encodeURIComponentFunction));

  // decodeURIComponent
  auto decodeURIComponentFunction = new NativeFunction(
      this, "decodeURIComponent", [this](const std::vector<Value>& args) -> Value {
        if (args.empty()) {
          return Value("undefined");
        }

        std::string component = args[0].toString(this);
        return Value(decodeURIComponent(component));
      },
      1);
  globalObj->set(this, "decodeURIComponent", Value(decodeURIComponentFunction));
}

void ExecutionContext::initializeGlobalProperties(Object* globalObj) {
  // グローバル定数
  globalObj->set(this, "NaN", Value(std::numeric_limits<double>::quiet_NaN()), PropertyAttributes::ReadOnly | PropertyAttributes::DontEnum | PropertyAttributes::DontDelete);
  globalObj->set(this, "Infinity", Value(std::numeric_limits<double>::infinity()), PropertyAttributes::ReadOnly | PropertyAttributes::DontEnum | PropertyAttributes::DontDelete);
  globalObj->set(this, "undefined", Value::undefined(), PropertyAttributes::ReadOnly | PropertyAttributes::DontEnum | PropertyAttributes::DontDelete);

  // globalThis
  globalObj->set(this, "globalThis", Value(globalObj), PropertyAttributes::Writable | PropertyAttributes::Configurable);
}

void ExecutionContext::setupPrototypeChains() {
  // プロトタイプチェーンの設定
  // ECMAScript仕様に準拠したプロトタイプチェーンの構築
  // Object -> null
  // Function.prototype -> Object.prototype -> null
  // Array.prototype -> Object.prototype -> null
  // String.prototype -> Object.prototype -> null
  // 等の標準オブジェクト階層を構築

  Object* objectPrototype = getGlobalObject()->get(this, "Object").asObject()->get(this, "prototype").asObject();
  Object* functionPrototype = getGlobalObject()->get(this, "Function").asObject()->get(this, "prototype").asObject();
  Object* arrayPrototype = getGlobalObject()->get(this, "Array").asObject()->get(this, "prototype").asObject();

  // Function.prototypeはObjectのインスタンス
  functionPrototype->setPrototype(objectPrototype);

  // 各組み込みオブジェクトのプロトタイプチェーンを設定
  arrayPrototype->setPrototype(objectPrototype);
  getGlobalObject()->get(this, "String").asObject()->get(this, "prototype").asObject()->setPrototype(objectPrototype);
  getGlobalObject()->get(this, "Number").asObject()->get(this, "prototype").asObject()->setPrototype(objectPrototype);
  getGlobalObject()->get(this, "Boolean").asObject()->get(this, "prototype").asObject()->setPrototype(objectPrototype);
  getGlobalObject()->get(this, "Date").asObject()->get(this, "prototype").asObject()->setPrototype(objectPrototype);
  getGlobalObject()->get(this, "RegExp").asObject()->get(this, "prototype").asObject()->setPrototype(objectPrototype);
  getGlobalObject()->get(this, "Error").asObject()->get(this, "prototype").asObject()->setPrototype(objectPrototype);

  // 各Errorサブタイプのプロトタイプチェーンを設定
  Object* errorPrototype = getGlobalObject()->get(this, "Error").asObject()->get(this, "prototype").asObject();
  getGlobalObject()->get(this, "TypeError").asObject()->get(this, "prototype").asObject()->setPrototype(errorPrototype);
  getGlobalObject()->get(this, "ReferenceError").asObject()->get(this, "prototype").asObject()->setPrototype(errorPrototype);
  getGlobalObject()->get(this, "SyntaxError").asObject()->get(this, "prototype").asObject()->setPrototype(errorPrototype);
  getGlobalObject()->get(this, "RangeError").asObject()->get(this, "prototype").asObject()->setPrototype(errorPrototype);

  // ES6+のコレクションオブジェクト
  getGlobalObject()->get(this, "Map").asObject()->get(this, "prototype").asObject()->setPrototype(objectPrototype);
  getGlobalObject()->get(this, "Set").asObject()->get(this, "prototype").asObject()->setPrototype(objectPrototype);
  getGlobalObject()->get(this, "WeakMap").asObject()->get(this, "prototype").asObject()->setPrototype(objectPrototype);
  getGlobalObject()->get(this, "WeakSet").asObject()->get(this, "prototype").asObject()->setPrototype(objectPrototype);

  // TypedArrays
  Object* typedArrayPrototype = getGlobalObject()->get(this, "TypedArray").asObject()->get(this, "prototype").asObject();
  typedArrayPrototype->setPrototype(objectPrototype);

  // 各TypedArrayのプロトタイプを設定
  getGlobalObject()->get(this, "Int8Array").asObject()->get(this, "prototype").asObject()->setPrototype(typedArrayPrototype);
  getGlobalObject()->get(this, "Uint8Array").asObject()->get(this, "prototype").asObject()->setPrototype(typedArrayPrototype);
  getGlobalObject()->get(this, "Uint8ClampedArray").asObject()->get(this, "prototype").asObject()->setPrototype(typedArrayPrototype);
  getGlobalObject()->get(this, "Int16Array").asObject()->get(this, "prototype").asObject()->setPrototype(typedArrayPrototype);
  getGlobalObject()->get(this, "Uint16Array").asObject()->get(this, "prototype").asObject()->setPrototype(typedArrayPrototype);
  getGlobalObject()->get(this, "Int32Array").asObject()->get(this, "prototype").asObject()->setPrototype(typedArrayPrototype);
  getGlobalObject()->get(this, "Uint32Array").asObject()->get(this, "prototype").asObject()->setPrototype(typedArrayPrototype);
  getGlobalObject()->get(this, "Float32Array").asObject()->get(this, "prototype").asObject()->setPrototype(typedArrayPrototype);
  getGlobalObject()->get(this, "Float64Array").asObject()->get(this, "prototype").asObject()->setPrototype(typedArrayPrototype);
  getGlobalObject()->get(this, "BigInt64Array").asObject()->get(this, "prototype").asObject()->setPrototype(typedArrayPrototype);
  getGlobalObject()->get(this, "BigUint64Array").asObject()->get(this, "prototype").asObject()->setPrototype(typedArrayPrototype);

  // Promise、Proxy、Reflectなどの他のES6+オブジェクト
  getGlobalObject()->get(this, "Promise").asObject()->get(this, "prototype").asObject()->setPrototype(objectPrototype);

  // コンストラクタ関数自体がFunction.prototypeを継承していることを確認
  getGlobalObject()->get(this, "Object").asObject()->setPrototype(functionPrototype);
  getGlobalObject()->get(this, "Function").asObject()->setPrototype(functionPrototype);
  getGlobalObject()->get(this, "Array").asObject()->setPrototype(functionPrototype);
  getGlobalObject()->get(this, "String").asObject()->setPrototype(functionPrototype);
  getGlobalObject()->get(this, "Number").asObject()->setPrototype(functionPrototype);
  getGlobalObject()->get(this, "Boolean").asObject()->setPrototype(functionPrototype);
  getGlobalObject()->get(this, "Date").asObject()->setPrototype(functionPrototype);
  getGlobalObject()->get(this, "RegExp").asObject()->setPrototype(functionPrototype);
  getGlobalObject()->get(this, "Error").asObject()->setPrototype(functionPrototype);
  getGlobalObject()->get(this, "Map").asObject()->setPrototype(functionPrototype);
  getGlobalObject()->get(this, "Set").asObject()->setPrototype(functionPrototype);
}

Value ExecutionContext::cloneValue(Value value) {
  // プリミティブ値はそのままコピー可能
  if (value.isPrimitive()) {
    return value;
  }

  // オブジェクトの場合は新しいオブジェクトを作成してプロパティをコピー
  if (value.isObject()) {
    Object* originalObj = value.asObject();
    Object* newObj = nullptr;

    // オブジェクトの種類に応じた適切なクローン処理
    if (originalObj->isArray()) {
      Array* originalArray = static_cast<Array*>(originalObj);
      Array* newArray = new Array(this);

      // 配列要素のコピー
      uint32_t length = originalArray->length();
      for (uint32_t i = 0; i < length; i++) {
        if (originalArray->hasOwnProperty(this, std::to_string(i))) {
          Value element = originalArray->get(this, std::to_string(i));
          newArray->set(this, std::to_string(i), cloneValue(element));
        }
      }

      newObj = newArray;
    } else if (originalObj->isDate()) {
      Date* originalDate = static_cast<Date*>(originalObj);
      newObj = new Date(this, originalDate->timeValue());
    } else if (originalObj->isRegExp()) {
      RegExp* originalRegExp = static_cast<RegExp*>(originalObj);
      newObj = new RegExp(this, originalRegExp->pattern(), originalRegExp->flags());
    } else if (originalObj->isMap()) {
      Map* originalMap = static_cast<Map*>(originalObj);
      Map* newMap = new Map(this);

      // Mapエントリのコピー
      originalMap->forEach(this, [this, newMap](Value key, Value value) {
        newMap->set(this, cloneValue(key), cloneValue(value));
        return false;  // 継続
      });

      newObj = newMap;
    } else if (originalObj->isSet()) {
      Set* originalSet = static_cast<Set*>(originalObj);
      Set* newSet = new Set(this);

      // Setエントリのコピー
      originalSet->forEach(this, [this, newSet](Value value) {
        newSet->add(this, cloneValue(value));
        return false;  // 継続
      });

      newObj = newSet;
    } else if (originalObj->isArrayBuffer()) {
      ArrayBuffer* originalBuffer = static_cast<ArrayBuffer*>(originalObj);
      size_t byteLength = originalBuffer->byteLength();

      // 新しいバッファを作成してデータをコピー
      ArrayBuffer* newBuffer = new ArrayBuffer(this, byteLength);
      std::memcpy(newBuffer->data(), originalBuffer->data(), byteLength);

      newObj = newBuffer;
    } else if (originalObj->isTypedArray()) {
      // TypedArrayの種類に応じたクローン処理
      TypedArray* originalTypedArray = static_cast<TypedArray*>(originalObj);
      TypedArray* newTypedArray = nullptr;

      // 適切なTypedArrayを作成
      switch (originalTypedArray->type()) {
        case TypedArrayType::Int8:
          newTypedArray = new Int8Array(this, originalTypedArray->length());
          break;
        case TypedArrayType::Uint8:
          newTypedArray = new Uint8Array(this, originalTypedArray->length());
          break;
        case TypedArrayType::Uint8Clamped:
          newTypedArray = new Uint8ClampedArray(this, originalTypedArray->length());
          break;
        case TypedArrayType::Int16:
          newTypedArray = new Int16Array(this, originalTypedArray->length());
          break;
        case TypedArrayType::Uint16:
          newTypedArray = new Uint16Array(this, originalTypedArray->length());
          break;
        case TypedArrayType::Int32:
          newTypedArray = new Int32Array(this, originalTypedArray->length());
          break;
        case TypedArrayType::Uint32:
          newTypedArray = new Uint32Array(this, originalTypedArray->length());
          break;
        case TypedArrayType::Float32:
          newTypedArray = new Float32Array(this, originalTypedArray->length());
          break;
        case TypedArrayType::Float64:
          newTypedArray = new Float64Array(this, originalTypedArray->length());
          break;
        case TypedArrayType::BigInt64:
          newTypedArray = new BigInt64Array(this, originalTypedArray->length());
          break;
        case TypedArrayType::BigUint64:
          newTypedArray = new BigUint64Array(this, originalTypedArray->length());
          break;
        default:
          // 未知の型の場合はUint8Arrayとしてフォールバック
          newTypedArray = new Uint8Array(this, originalTypedArray->length());
          break;
      }

      // データをコピー
      std::memcpy(newTypedArray->data(), originalTypedArray->data(), originalTypedArray->byteLength());

      newObj = newTypedArray;
    } else if (originalObj->isPromise()) {
      // Promiseは特殊なケース - 新しいPromiseを作成し状態をコピー
      Promise* originalPromise = static_cast<Promise*>(originalObj);
      Promise* newPromise = new Promise(this);

      // 状態をコピー
      switch (originalPromise->state()) {
        case PromiseState::Pending:
          // 保留中のPromiseはそのまま
          break;
        case PromiseState::Fulfilled:
          newPromise->resolve(this, cloneValue(originalPromise->result()));
          break;
        case PromiseState::Rejected:
          newPromise->reject(this, cloneValue(originalPromise->result()));
          break;
      }

      newObj = newPromise;
    } else if (originalObj->isSymbol()) {
      // Symbolは特殊なケース - 同じ説明を持つ新しいSymbolを作成
      Symbol* originalSymbol = static_cast<Symbol*>(originalObj);
      newObj = new Symbol(this, originalSymbol->description());
    } else if (originalObj->isFunction()) {
      // 関数のクローンは複雑 - 実装依存
      // この例では単純化のため、同じ名前の新しいネイティブ関数を作成
      Function* originalFunction = static_cast<Function*>(originalObj);

      if (originalFunction->isNativeFunction()) {
        // ネイティブ関数の場合、同じ実装を持つ新しい関数を作成
        NativeFunction* nativeFunc = static_cast<NativeFunction*>(originalFunction);
        newObj = new NativeFunction(this, nativeFunc->name(), nativeFunc->nativeFunction(), nativeFunc->length());
      } else {
        // ユーザー定義関数の場合、同じソースコードを持つ新しい関数を作成
        // 実際の実装ではスコープチェーンなども考慮する必要がある
        UserFunction* userFunc = static_cast<UserFunction*>(originalFunction);
        newObj = new UserFunction(this, userFunc->name(), userFunc->sourceCode(), userFunc->scope(), userFunc->length());
      }
    } else if (originalObj->isError()) {
      // Errorオブジェクトのクローン
      Error* originalError = static_cast<Error*>(originalObj);
      Error* newError = nullptr;

      // エラータイプに応じた適切なエラーオブジェクトを作成
      if (originalError->isTypeError()) {
        newError = new TypeError(this, originalError->message());
      } else if (originalError->isReferenceError()) {
        newError = new ReferenceError(this, originalError->message());
      } else if (originalError->isSyntaxError()) {
        newError = new SyntaxError(this, originalError->message());
      } else if (originalError->isRangeError()) {
        newError = new RangeError(this, originalError->message());
      } else {
        newError = new Error(this, originalError->message());
      }

      // スタックトレースなどの追加プロパティをコピー
      if (originalError->hasOwnProperty(this, "stack")) {
        newError->set(this, "stack", originalError->get(this, "stack"));
      }

      newObj = newError;
    } else {
      // 通常のオブジェクト
      newObj = createObject();
    }

    // プロトタイプの設定
    Object* proto = originalObj->getPrototype();
    if (proto) {
      newObj->setPrototype(proto);
    }

    // 通常のプロパティをコピー（配列要素やTypedArrayデータを除く）
    originalObj->getOwnPropertyNames(this).forEach(this, [this, originalObj, newObj](Value key) {
      std::string keyStr = key.toString(this);

      // 数値インデックスの場合は配列要素としてすでにコピー済みの可能性があるため、
      // 通常のオブジェクトプロパティとしてのみコピーする
      if (!originalObj->isArray() || !isArrayIndex(keyStr)) {
        PropertyDescriptor desc;
        if (originalObj->getOwnPropertyDescriptor(this, keyStr, &desc)) {
          // 値をディープクローン
          if (desc.hasValue()) {
            desc.setValue(cloneValue(desc.value()));
          }

          // getter/setterもクローン
          if (desc.hasGetter()) {
            Function* originalGetter = desc.getter().asFunction();
            if (originalGetter->isNativeFunction()) {
              NativeFunction* nativeGetter = static_cast<NativeFunction*>(originalGetter);
              desc.setGetter(Value(new NativeFunction(this, "", nativeGetter->nativeFunction(), 0)));
            } else {
              // ユーザー定義getterの場合、同じソースコードを持つ新しい関数を作成
              UserFunction* userGetter = static_cast<UserFunction*>(originalGetter);
              desc.setGetter(Value(new UserFunction(this, "", userGetter->sourceCode(), userGetter->scope(), 0)));
            }
          }

          if (desc.hasSetter()) {
            Function* originalSetter = desc.setter().asFunction();
            if (originalSetter->isNativeFunction()) {
              NativeFunction* nativeSetter = static_cast<NativeFunction*>(originalSetter);
              desc.setSetter(Value(new NativeFunction(this, "", nativeSetter->nativeFunction(), 1)));
            } else {
              // ユーザー定義setterの場合、同じソースコードを持つ新しい関数を作成
              UserFunction* userSetter = static_cast<UserFunction*>(originalSetter);
              desc.setSetter(Value(new UserFunction(this, "", userSetter->sourceCode(), userSetter->scope(), 1)));
            }
          }

          newObj->defineProperty(this, keyStr, desc);
        }
      }

      return false;  // 継続
    });

    return Value(newObj);
  }

  // 未対応の型
  return Value::undefined();
}

bool ExecutionContext::isArrayIndex(const std::string& key) {
  // 文字列が有効な配列インデックスかどうかを判定
  // ECMAScript仕様に準拠した実装

  // 空文字列または先頭が'0'で長さが2以上の場合は配列インデックスではない
  if (key.empty() || (key[0] == '0' && key.length() > 1)) {
    return false;
  }

  // 数字以外の文字が含まれている場合は配列インデックスではない
  for (char c : key) {
    if (c < '0' || c > '9') {
      return false;
    }
  }

  try {
    // 文字列を数値に変換
    uint32_t index = std::stoul(key);

    // 2^32-2未満であることを確認（ECMAScript仕様に基づく）
    // 2^32-1は配列の長さの最大値として予約されている
    return index < 0xFFFFFFFE;
  } catch (const std::exception&) {
    // 変換エラーが発生した場合（オーバーフローなど）
    return false;
  }
}

}  // namespace aero