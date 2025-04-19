/**
 * @file global_object.cpp
 * @brief JavaScript のグローバルオブジェクトの実装
 * @copyright Copyright (c) 2023 AeroJS project authors
 */

#include "global_object.h"
#include "context.h"
#include "function.h"
#include "builtins/array/array.h"
#include "builtins/boolean/boolean.h"
#include "builtins/date/date.h"
#include "builtins/error/error.h"
#include "builtins/function/function_prototype.h"
#include "builtins/json/json.h"
#include "builtins/map/map.h"
#include "builtins/math/math.h"
#include "builtins/number/number.h"
#include "builtins/object/object.h"
#include "builtins/promise/promise.h"
#include "builtins/proxy/proxy.h"
#include "builtins/reflect/reflect.h"
#include "builtins/regexp/regexp.h"
#include "builtins/set/set.h"
#include "builtins/string/string.h"
#include "builtins/symbol/symbol.h"
#include "builtins/typed_array/typed_array.h"
#include "builtins/weakmap/weakmap.h"
#include "builtins/weakset/weakset.h"
#include <stdexcept>
#include <chrono>
#include <memory>

namespace aero {

GlobalObject::GlobalObject(Context* context)
    : Object(nullptr)
    , m_objectPrototype(nullptr)
    , m_functionPrototype(nullptr)
    , m_arrayPrototype(nullptr)
    , m_stringPrototype(nullptr)
    , m_numberPrototype(nullptr)
    , m_booleanPrototype(nullptr)
    , m_datePrototype(nullptr)
    , m_regExpPrototype(nullptr)
    , m_errorPrototype(nullptr)
    , m_setPrototype(nullptr)
    , m_mapPrototype(nullptr)
    , m_weakMapPrototype(nullptr)
    , m_weakSetPrototype(nullptr)
    , m_promisePrototype(nullptr)
    , m_symbolPrototype(nullptr)
    , m_proxyPrototype(nullptr)
    , m_typedArrayPrototype(nullptr)
    , m_objectConstructor(nullptr)
    , m_functionConstructor(nullptr)
    , m_arrayConstructor(nullptr)
    , m_stringConstructor(nullptr)
    , m_numberConstructor(nullptr)
    , m_booleanConstructor(nullptr)
    , m_dateConstructor(nullptr)
    , m_regExpConstructor(nullptr)
    , m_errorConstructor(nullptr)
    , m_setConstructor(nullptr)
    , m_mapConstructor(nullptr)
    , m_weakMapConstructor(nullptr)
    , m_weakSetConstructor(nullptr)
    , m_promiseConstructor(nullptr)
    , m_symbolConstructor(nullptr)
    , m_proxyConstructor(nullptr)
    , m_typedArrayConstructor(nullptr)
    , m_context(context)
    , m_initialized(false)
    , m_initializationStartTime(std::chrono::high_resolution_clock::now())
{
    // グローバルオブジェクトの初期化
    initialize();
    
    auto initEnd = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        initEnd - m_initializationStartTime).count();
    
    if (m_context && m_context->debugMode()) {
        m_context->logger()->debug("GlobalObject initialization completed in {} ms", duration);
    }
}

GlobalObject::~GlobalObject()
{
    // リソースのクリーンアップ
    cleanupBuiltins();
}

void GlobalObject::initialize()
{
    // 初期化の多重実行を防止
    std::lock_guard<std::mutex> lock(m_initMutex);
    if (m_initialized) {
        return;
    }

    try {
        // プロトタイプチェーンの構築
        setupPrototypeChain();
        
        // 組み込みオブジェクトの初期化
        initializeBuiltins();
        
        // グローバル関数の登録
        registerGlobalFunctions();
        
        // グローバルプロパティの設定
        setupGlobalProperties();
        
        m_initialized = true;
    } catch (const std::exception& e) {
        if (m_context) {
            m_context->logger()->error("GlobalObject initialization failed: {}", e.what());
        }
        throw std::runtime_error(std::string("Failed to initialize GlobalObject: ") + e.what());
    }
}

void GlobalObject::setupPrototypeChain()
{
    // Object.prototypeの初期化（最も基本的なプロトタイプ）
    m_objectPrototype = new Object(nullptr);
    m_objectPrototype->setPrototype(nullptr);
    
    // Function.prototypeの初期化
    m_functionPrototype = new FunctionPrototype(m_context);
    m_functionPrototype->setPrototype(m_objectPrototype);
    
    // その他の基本プロトタイプの初期化
    m_arrayPrototype = new Object(m_objectPrototype);
    m_stringPrototype = new Object(m_objectPrototype);
    m_numberPrototype = new Object(m_objectPrototype);
    m_booleanPrototype = new Object(m_objectPrototype);
    m_datePrototype = new Object(m_objectPrototype);
    m_regExpPrototype = new Object(m_objectPrototype);
    m_errorPrototype = new Object(m_objectPrototype);
    m_setPrototype = new Object(m_objectPrototype);
    m_mapPrototype = new Object(m_objectPrototype);
    m_weakMapPrototype = new Object(m_objectPrototype);
    m_weakSetPrototype = new Object(m_objectPrototype);
    m_promisePrototype = new Object(m_objectPrototype);
    m_symbolPrototype = new Object(m_objectPrototype);
    m_proxyPrototype = new Object(m_objectPrototype);
    m_typedArrayPrototype = new Object(m_objectPrototype);
}

void GlobalObject::initializeBuiltins()
{
    // 基本的な組み込みオブジェクトの初期化
    registerObjectBuiltin(this);
    registerFunctionBuiltin(this);
    registerArrayBuiltin(this);
    registerStringBuiltin(this);
    registerNumberBuiltin(this);
    registerBooleanBuiltin(this);
    registerDateBuiltin(this);
    registerRegExpBuiltin(this);
    registerErrorBuiltin(this);
    
    // コレクション関連の組み込みオブジェクト
    registerSetBuiltin(this);
    registerMapBuiltin(this);
    registerWeakMapBuiltin(this);
    registerWeakSetBuiltin(this);
    
    // ES6以降の組み込みオブジェクト
    registerPromiseBuiltin(this);
    registerSymbolBuiltin(this);
    registerProxyBuiltin(this);
    registerReflectBuiltin(this);
    registerTypedArrayBuiltin(this);
    
    // ユーティリティオブジェクト
    registerJSONBuiltin(this);
    registerMathBuiltin(this);
    
    // エラータイプの初期化
    initializeErrorTypes();
}

void GlobalObject::initializeErrorTypes()
{
    // 標準エラータイプの初期化
    m_errorConstructors["Error"] = m_errorConstructor;
    
    // 各種エラータイプの登録
    const std::vector<std::string> errorTypes = {
        "EvalError", "RangeError", "ReferenceError", 
        "SyntaxError", "TypeError", "URIError"
    };
    
    for (const auto& type : errorTypes) {
        auto constructor = createErrorConstructor(type);
        m_errorConstructors[type] = constructor;
        defineProperty(type, constructor, PropertyAttributes::DontEnum);
    }
}

void GlobalObject::registerGlobalFunctions()
{
    // グローバルスコープで利用可能な関数の登録
    const std::vector<std::pair<std::string, NativeFunction>> globalFunctions = {
        {"eval", std::bind(&GlobalObject::eval, this, std::placeholders::_1, std::placeholders::_2)},
        {"parseInt", std::bind(&GlobalObject::parseInt, this, std::placeholders::_1, std::placeholders::_2)},
        {"parseFloat", std::bind(&GlobalObject::parseFloat, this, std::placeholders::_1, std::placeholders::_2)},
        {"isNaN", std::bind(&GlobalObject::isNaN, this, std::placeholders::_1, std::placeholders::_2)},
        {"isFinite", std::bind(&GlobalObject::isFinite, this, std::placeholders::_1, std::placeholders::_2)},
        {"encodeURI", std::bind(&GlobalObject::encodeURI, this, std::placeholders::_1, std::placeholders::_2)},
        {"decodeURI", std::bind(&GlobalObject::decodeURI, this, std::placeholders::_1, std::placeholders::_2)},
        {"encodeURIComponent", std::bind(&GlobalObject::encodeURIComponent, this, std::placeholders::_1, std::placeholders::_2)},
        {"decodeURIComponent", std::bind(&GlobalObject::decodeURIComponent, this, std::placeholders::_1, std::placeholders::_2)}
    };
    
    for (const auto& func : globalFunctions) {
        auto functionObject = new Function(m_context, func.second, func.first, 0);
        defineProperty(func.first, functionObject, PropertyAttributes::DontEnum);
    }
}

void GlobalObject::setupGlobalProperties()
{
    // グローバル定数の設定
    defineProperty("undefined", Value::undefined(), PropertyAttributes::ReadOnly | PropertyAttributes::DontEnum | PropertyAttributes::DontDelete);
    defineProperty("NaN", Value(std::numeric_limits<double>::quiet_NaN()), PropertyAttributes::ReadOnly | PropertyAttributes::DontEnum | PropertyAttributes::DontDelete);
    defineProperty("Infinity", Value(std::numeric_limits<double>::infinity()), PropertyAttributes::ReadOnly | PropertyAttributes::DontEnum | PropertyAttributes::DontDelete);
    
    // グローバルオブジェクト自身への参照
    defineProperty("globalThis", this, PropertyAttributes::DontEnum);
    
    // ブラウザ環境の場合の追加プロパティ
    if (m_context && m_context->environmentType() == EnvironmentType::Browser) {
        setupBrowserEnvironment();
    }
}

void GlobalObject::setupBrowserEnvironment()
{
    // ブラウザ環境特有のグローバルオブジェクトを設定
    defineProperty("window", this, PropertyAttributes::DontEnum);
    
    // DOM関連のオブジェクトを初期化
    auto documentObj = new DocumentObject(m_context);
    defineProperty("document", documentObj, PropertyAttributes::DontEnum);
    
    // ロケーション情報
    auto locationObj = new LocationObject(m_context);
    defineProperty("location", locationObj, PropertyAttributes::DontEnum);
    
    // ナビゲーター情報
    auto navigatorObj = new NavigatorObject(m_context);
    defineProperty("navigator", navigatorObj, PropertyAttributes::DontEnum);
    
    // 履歴操作
    auto historyObj = new HistoryObject(m_context);
    defineProperty("history", historyObj, PropertyAttributes::DontEnum);
    
    // スクリーン情報
    auto screenObj = new ScreenObject(m_context);
    defineProperty("screen", screenObj, PropertyAttributes::DontEnum);
    
    // ローカルストレージとセッションストレージ
    auto localStorageObj = new StorageObject(m_context, StorageType::Local);
    auto sessionStorageObj = new StorageObject(m_context, StorageType::Session);
    defineProperty("localStorage", localStorageObj, PropertyAttributes::DontEnum);
    defineProperty("sessionStorage", sessionStorageObj, PropertyAttributes::DontEnum);
    
    // XMLHttpRequest
    auto xhrConstructor = new XMLHttpRequestConstructor(m_context);
    defineProperty("XMLHttpRequest", xhrConstructor, PropertyAttributes::DontEnum);
    
    // Fetch API
    auto fetchFunc = new Function(m_context, 
        std::bind(&GlobalObject::fetchImplementation, this, std::placeholders::_1, std::placeholders::_2),
        "fetch", 1);
    defineProperty("fetch", fetchFunc, PropertyAttributes::DontEnum);
    
    // Web APIの初期化
    setupWebAPIs();
    
    // イベント処理システム
    setupEventSystem();
    
    // タイマー関数
    setupTimerFunctions();
}

void GlobalObject::cleanupBuiltins()
{
    // メモリリークを防ぐためのクリーンアップ処理
    // 注意: 循環参照を適切に処理する必要がある
    
    // プロトタイプオブジェクトのクリーンアップ
    safeDelete(m_objectPrototype);
    safeDelete(m_functionPrototype);
    safeDelete(m_arrayPrototype);
    safeDelete(m_stringPrototype);
    safeDelete(m_numberPrototype);
    safeDelete(m_booleanPrototype);
    safeDelete(m_datePrototype);
    safeDelete(m_regExpPrototype);
    safeDelete(m_errorPrototype);
    safeDelete(m_setPrototype);
    safeDelete(m_mapPrototype);
    safeDelete(m_weakMapPrototype);
    safeDelete(m_weakSetPrototype);
    safeDelete(m_promisePrototype);
    safeDelete(m_symbolPrototype);
    safeDelete(m_proxyPrototype);
    safeDelete(m_typedArrayPrototype);
    
    // コンストラクタオブジェクトのクリーンアップ
    safeDelete(m_objectConstructor);
    safeDelete(m_functionConstructor);
    safeDelete(m_arrayConstructor);
    safeDelete(m_stringConstructor);
    safeDelete(m_numberConstructor);
    safeDelete(m_booleanConstructor);
    safeDelete(m_dateConstructor);
    safeDelete(m_regExpConstructor);
    safeDelete(m_errorConstructor);
    safeDelete(m_setConstructor);
    safeDelete(m_mapConstructor);
    safeDelete(m_weakMapConstructor);
    safeDelete(m_weakSetConstructor);
    safeDelete(m_promiseConstructor);
    safeDelete(m_symbolConstructor);
    safeDelete(m_proxyConstructor);
    safeDelete(m_typedArrayConstructor);
    
    // エラーコンストラクタのクリーンアップ
    for (auto& pair : m_errorConstructors) {
        safeDelete(pair.second);
    }
    m_errorConstructors.clear();
}

template<typename T>
void GlobalObject::safeDelete(T*& ptr)
{
    if (ptr) {
        delete ptr;
        ptr = nullptr;
    }
}

Value GlobalObject::eval(const std::vector<Value>& args, Object* thisObject)
{
    if (args.empty() || !args[0].isString()) {
        return args.empty() ? Value::undefined() : args[0];
    }
    
    std::string code = args[0].toString();
    
    try {
        // コードの解析と実行
        return m_context->evaluateScript(code, "eval", 1);
    } catch (const std::exception& e) {
        // eval内でのエラーを適切に処理
        m_context->throwError("EvalError", e.what());
        return Value::undefined();
    }
}

Value GlobalObject::parseInt(const std::vector<Value>& args, Object* thisObject)
{
    if (args.empty()) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    std::string str = args[0].toString();
    int radix = args.size() > 1 ? args[1].toInt32() : 10;
    
    // 文字列の先頭の空白をスキップ
    size_t i = 0;
    while (i < str.length() && std::isspace(str[i])) {
        i++;
    }
    
    // 符号の処理
    bool negative = false;
    if (i < str.length() && (str[i] == '+' || str[i] == '-')) {
        negative = (str[i] == '-');
        i++;
    }
    
    // 基数の自動検出
    if (radix == 0) {
        radix = 10;
        if (i + 1 < str.length() && str[i] == '0') {
            if (str[i + 1] == 'x' || str[i + 1] == 'X') {
                radix = 16;
                i += 2;
            } else {
                radix = 8;
                i++;
            }
        }
    } else if (radix < 2 || radix > 36) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    // 数値の解析
    double result = 0;
    bool validDigit = false;
    
    while (i < str.length()) {
        int digit;
        char c = str[i];
        
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'A' && c <= 'Z') {
            digit = c - 'A' + 10;
        } else if (c >= 'a' && c <= 'z') {
            digit = c - 'a' + 10;
        } else {
            break;
        }
        
        if (digit >= radix) {
            break;
        }
        
        validDigit = true;
        result = result * radix + digit;
        i++;
    }
    
    if (!validDigit) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    return Value(negative ? -result : result);
}

Value GlobalObject::parseFloat(const std::vector<Value>& args, Object* thisObject)
{
    if (args.empty()) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    std::string str = args[0].toString();
    
    // 文字列の先頭の空白をスキップ
    size_t i = 0;
    while (i < str.length() && std::isspace(str[i])) {
        i++;
    }
    
    // 空文字列または空白のみの場合
    if (i >= str.length()) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    // 符号の処理
    bool negative = false;
    if (str[i] == '+' || str[i] == '-') {
        negative = (str[i] == '-');
        i++;
    }
    
    // 数値部分の解析
    double result = 0;
    bool hasDigit = false;
    
    // 整数部分
    while (i < str.length() && std::isdigit(str[i])) {
        result = result * 10 + (str[i] - '0');
        hasDigit = true;
        i++;
    }
    
    // 小数部分
    if (i < str.length() && str[i] == '.') {
        i++;
        double fraction = 0;
        double divisor = 1;
        
        while (i < str.length() && std::isdigit(str[i])) {
            fraction = fraction * 10 + (str[i] - '0');
            divisor *= 10;
            hasDigit = true;
            i++;
        }
        
        result += fraction / divisor;
    }
    
    // 指数部分
    if (hasDigit && i < str.length() && (str[i] == 'e' || str[i] == 'E')) {
        i++;
        bool expNegative = false;
        
        if (i < str.length() && (str[i] == '+' || str[i] == '-')) {
            expNegative = (str[i] == '-');
            i++;
        }
        
        int exponent = 0;
        bool hasExponentDigit = false;
        
        while (i < str.length() && std::isdigit(str[i])) {
            exponent = exponent * 10 + (str[i] - '0');
            hasExponentDigit = true;
            i++;
        }
        
        if (hasExponentDigit) {
            double power = 1;
            for (int j = 0; j < exponent; j++) {
                power *= 10;
            }
            
            if (expNegative) {
                result /= power;
            } else {
                result *= power;
            }
        }
    }
    
    if (!hasDigit) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    return Value(negative ? -result : result);
}

Value GlobalObject::isNaN(const std::vector<Value>& args, Object* thisObject)
{
    if (args.empty()) {
        return Value(true);
    }
    
    double num = args[0].toNumber();
    return Value(std::isnan(num));
}

Value GlobalObject::isFinite(const std::vector<Value>& args, Object* thisObject)
{
    if (args.empty()) {
        return Value(false);
    }
    
    double num = args[0].toNumber();
    return Value(std::isfinite(num));
}

Value GlobalObject::encodeURI(const std::vector<Value>& args, Object* thisObject)
{
    if (args.empty()) {
        return Value("undefined");
    }
    
    std::string uri = args[0].toString();
    std::string result;
    
    for (size_t i = 0; i < uri.length(); i++) {
        unsigned char c = uri[i];
        
        // RFC 3986で予約されていない文字、および一部の予約文字はそのまま
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '!' || 
            c == '~' || c == '*' || c == '\'' || c == '(' || c == ')' ||
            c == ';' || c == '/' || c == '?' || c == ':' || c == '@' ||
            c == '&' || c == '=' || c == '+' || c == '$' || c == ',') {
            result += c;
        } else {
            // UTF-8エンコーディングを考慮
            if ((c & 0x80) == 0) {
                // ASCII文字
                char hex[4];
                std::snprintf(hex, sizeof(hex), "%%%02X", c);
                result += hex;
            } else {
                // マルチバイト文字の処理
                // UTF-8エンコーディングの詳細な実装は省略
                // 実際の実装ではUTF-8のバイトシーケンスを適切に処理する必要がある
                char hex[4];
                std::snprintf(hex, sizeof(hex), "%%%02X", c);
                result += hex;
            }
        }
    }
    
    return Value(result);
}

Value GlobalObject::decodeURI(const std::vector<Value>& args, Object* thisObject)
{
    if (args.empty()) {
        return Value("undefined");
    }
    
    std::string uri = args[0].toString();
    std::string result;
    
    for (size_t i = 0; i < uri.length(); i++) {
        if (uri[i] == '%' && i + 2 < uri.length()) {
            // %エスケープシーケンスのデコード
            std::string hexStr = uri.substr(i + 1, 2);
            char* endPtr;
            unsigned char decodedChar = static_cast<unsigned char>(std::strtol(hexStr.c_str(), &endPtr, 16));
            
            if (*endPtr == '\0') {
                // 有効な16進数の場合
                result += decodedChar;
                i += 2;
            } else {
                // 無効な16進数の場合
                result += uri[i];
            }
        } else {
            result += uri[i];
        }
    }
    
    return Value(result);
}

Value GlobalObject::encodeURIComponent(const std::vector<Value>& args, Object* thisObject)
{
    if (args.empty()) {
        return Value("undefined");
    }
    
    std::string uri = args[0].toString();
    std::string result;
    
    for (size_t i = 0; i < uri.length(); i++) {
        unsigned char c = uri[i];
        
        // RFC 3986で予約されていない文字のみそのまま
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '!' || 
            c == '~' || c == '*' || c == '\'' || c == '(' || c == ')') {
            result += c;
        } else {
            // その他の文字はエスケープ
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%%%02X", c);
            result += hex;
        }
    }
    
    return Value(result);
}

Value GlobalObject::decodeURIComponent(const std::vector<Value>& args, Object* thisObject)
{
    if (args.empty()) {
        return Value("undefined");
    }
    
    std::string uri = args[0].toString();
    std::string result;
    
    for (size_t i = 0; i < uri.length(); i++) {
        if (uri[i] == '%' && i + 2 < uri.length()) {
            // %エスケープシーケンスのデコード
            std::string hexStr = uri.substr(i + 1, 2);
            char* endPtr;
            unsigned char decodedChar = static_cast<unsigned char>(std::strtol(hexStr.c_str(), &endPtr, 16));
            
            if (*endPtr == '\0') {
                // 有効な16進数の場合
                result += decodedChar;
                i += 2;
            } else {
                // 無効な16進数の場合
                result += uri[i];
            }
        } else {
            result += uri[i];
        }
    }
    
    return Value(result);
}

Object* GlobalObject::createErrorConstructor(const std::string& errorType)
{
    // 特定のエラータイプのコンストラクタを作成
    auto constructor = new Function(m_context, 
        [this, errorType](const std::vector<Value>& args, Object* thisObject) -> Value {
            std::string message = args.empty() ? "" : args[0].toString();
            auto errorObj = new ErrorObject(m_context, errorType, message);
            return Value(errorObj);
        }, 
        errorType, 1);
    
    // プロトタイプの設定
    auto prototype = new Object(m_errorPrototype);
    constructor->defineProperty("prototype", prototype, PropertyAttributes::ReadOnly | PropertyAttributes::DontEnum | PropertyAttributes::DontDelete);
    prototype->defineProperty("constructor", constructor, PropertyAttributes::DontEnum);
    prototype->defineProperty("name", Value(errorType), PropertyAttributes::DontEnum);
    
    return constructor;
}

} // namespace aero 