/**
 * @file globals.cpp
 * @brief JavaScriptのグローバルオブジェクトと関数の実装
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#include "globals.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#include "../error/error.h"
#include "../parser/parser.h"
#include "../utils/encoding.h"
#include "../utils/string_utils.h"
#include "globals_object.h"

namespace aero {

// グローバルオブジェクトのキャッシュ
static std::unordered_map<ExecutionContext*, std::unique_ptr<GlobalsObject>> s_globalsCache;
static std::mutex s_globalsMutex;

GlobalsObject* getGlobalsObject(ExecutionContext* ctx) {
  std::lock_guard<std::mutex> lock(s_globalsMutex);

  auto it = s_globalsCache.find(ctx);
  if (it != s_globalsCache.end()) {
    return it->second.get();
  }

  auto globals = std::make_unique<GlobalsObject>(ctx);
  globals->initialize();

  auto* result = globals.get();
  s_globalsCache[ctx] = std::move(globals);

  return result;
}

void initializeGlobalFunctions(ExecutionContext* ctx, Object* globalObj) {
  // グローバル関数の定義
  globalObj->defineProperty(ctx, "eval", Value::createFunction(ctx, globalEval, 1, "eval"),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Configurable));

  globalObj->defineProperty(ctx, "isFinite", Value::createFunction(ctx, globalIsFinite, 1, "isFinite"),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Configurable));

  globalObj->defineProperty(ctx, "isNaN", Value::createFunction(ctx, globalIsNaN, 1, "isNaN"),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Configurable));

  globalObj->defineProperty(ctx, "parseInt", Value::createFunction(ctx, globalParseInt, 2, "parseInt"),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Configurable));

  globalObj->defineProperty(ctx, "parseFloat", Value::createFunction(ctx, globalParseFloat, 1, "parseFloat"),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Configurable));

  globalObj->defineProperty(ctx, "encodeURI", Value::createFunction(ctx, globalEncodeURI, 1, "encodeURI"),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Configurable));

  globalObj->defineProperty(ctx, "decodeURI", Value::createFunction(ctx, globalDecodeURI, 1, "decodeURI"),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Configurable));

  globalObj->defineProperty(ctx, "encodeURIComponent", Value::createFunction(ctx, globalEncodeURIComponent, 1, "encodeURIComponent"),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Configurable));

  globalObj->defineProperty(ctx, "decodeURIComponent", Value::createFunction(ctx, globalDecodeURIComponent, 1, "decodeURIComponent"),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Configurable));

  // 非推奨だが互換性のために実装
  globalObj->defineProperty(ctx, "escape", Value::createFunction(ctx, globalEscape, 1, "escape"),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Configurable));

  globalObj->defineProperty(ctx, "unescape", Value::createFunction(ctx, globalUnescape, 1, "unescape"),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Configurable));

  // 名前空間オブジェクトの初期化
  json::initialize(ctx, globalObj);
  math::initialize(ctx, globalObj);
  reflect::initialize(ctx, globalObj);
}

Value globalEval(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数がない場合やstring型でない場合は、undefined または引数をそのまま返す
  if (args.empty() || !args[0].isString()) {
    return args.empty() ? Value::createUndefined() : args[0];
  }

  // 評価するコードを取得
  std::string code = args[0].toString(ctx)->getValue();
  
  // 空文字列の場合はundefinedを返す
  if (code.empty()) {
    return Value::createUndefined();
  }

  try {
    // 直接呼び出しかどうかの判定
    // ECMAScript仕様に基づき、呼び出し元の情報から直接評価かどうかを判断
    bool isDirectCall = ctx->isDirectEvalCall();
    
    if (isDirectCall) {
      // 直接評価の場合、現在のレキシカルスコープで実行
      // 変数環境、レキシカル環境、thisバインディングを保持
      ScopePtr currentScope = ctx->getLexicalScope();
      
      // 厳格モードの継承
      bool isStrict = ctx->isStrictMode() || StringUtils::startsWithStrictDirective(code);
      
      // 現在のスコープで評価を実行
      return ctx->getParser()->parseAndEvaluate(code, currentScope, isStrict);
    } else {
      // 間接評価の場合、グローバルコンテキストで実行
      // ただし、呼び出し元の厳格モードは考慮する
      bool isStrict = StringUtils::startsWithStrictDirective(code);
      
      // 新しい実行コンテキストを作成し、グローバルスコープで評価
      return ctx->getParser()->parseAndEvaluate(code, ctx->getGlobalObject(), isStrict);
    }
  } catch (SyntaxError& e) {
    // 構文エラーはそのまま伝播
    ctx->throwError(e);
    return Value::createUndefined();
  } catch (Error& e) {
    // その他のエラーは適切なスタックトレース情報を付加
    e.addToStackTrace("at eval (eval)");
    ctx->throwError(e);
    return Value::createUndefined();
  } catch (std::exception& e) {
    // 予期せぬ例外を適切なJavaScriptエラーに変換
    ctx->throwError(ErrorType::Error, std::string("内部エラー: ") + e.what());
    return Value::createUndefined();
  }
}

Value globalIsFinite(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createBoolean(false);
  }

  double num = args[0].toNumber(ctx);
  return Value::createBoolean(std::isfinite(num));
}

Value globalIsNaN(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createBoolean(true);
  }

  double num = args[0].toNumber(ctx);
  return Value::createBoolean(std::isnan(num));
}

Value globalParseInt(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createNaN();
  }

  std::string str = args[0].toString(ctx)->getValue();

  // 先頭と末尾の空白を除去
  str = StringUtils::trim(str);

  if (str.empty()) {
    return Value::createNaN();
  }

  // 基数の取得
  int radix = 10;
  if (args.size() > 1 && !args[1].isUndefined()) {
    radix = static_cast<int>(args[1].toNumber(ctx));

    // 基数が0または範囲外の場合の特別な処理
    if (radix == 0) {
      radix = 10;
    } else if (radix < 2 || radix > 36) {
      return Value::createNaN();
    }
  }

  // "0x"や"0X"で始まり、かつ基数が0または16の場合、16進数として扱う
  if ((radix == 0 || radix == 16) && str.size() >= 2 &&
      (str.substr(0, 2) == "0x" || str.substr(0, 2) == "0X")) {
    str = str.substr(2);
    radix = 16;
  }

  // 符号の処理
  bool negative = false;
  if (!str.empty() && (str[0] == '-' || str[0] == '+')) {
    negative = (str[0] == '-');
    str = str.substr(1);
  }

  // 数値変換
  size_t pos = 0;
  double result = 0;

  for (char c : str) {
    int digit;

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

    result = result * radix + digit;
    pos++;
  }

  if (pos == 0) {
    return Value::createNaN();
  }

  return Value::createNumber(negative ? -result : result);
}

Value globalParseFloat(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createNaN();
  }

  std::string str = args[0].toString(ctx)->getValue();

  // 先頭と末尾の空白を除去
  str = StringUtils::trim(str);

  if (str.empty()) {
    return Value::createNaN();
  }

  // 無限大と非数の特別な処理
  if (str == "Infinity" || str == "+Infinity") {
    return Value::createNumber(std::numeric_limits<double>::infinity());
  } else if (str == "-Infinity") {
    return Value::createNumber(-std::numeric_limits<double>::infinity());
  } else if (str == "NaN") {
    return Value::createNaN();
  }

  // 数値変換
  try {
    size_t pos;
    double result = std::stod(str, &pos);

    // 文字列の先頭が有効な数値形式でない場合はNaNを返す
    if (pos == 0) {
      return Value::createNaN();
    }

    return Value::createNumber(result);
  } catch (std::exception&) {
    return Value::createNaN();
  }
}

Value globalEncodeURI(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createString(ctx, "undefined");
  }

  std::string uri = args[0].toString(ctx)->getValue();
  std::string result;

  try {
    result = Encoding::encodeURI(uri);
    return Value::createString(ctx, result);
  } catch (Error& e) {
    ctx->throwError(e);
    return Value::createUndefined();
  }
}

Value globalDecodeURI(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createString(ctx, "undefined");
  }

  std::string encodedURI = args[0].toString(ctx)->getValue();
  std::string result;

  try {
    result = Encoding::decodeURI(encodedURI);
    return Value::createString(ctx, result);
  } catch (Error& e) {
    ctx->throwError(e);
    return Value::createUndefined();
  }
}

Value globalEncodeURIComponent(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createString(ctx, "undefined");
  }

  std::string component = args[0].toString(ctx)->getValue();
  std::string result;

  try {
    result = Encoding::encodeURIComponent(component);
    return Value::createString(ctx, result);
  } catch (Error& e) {
    ctx->throwError(e);
    return Value::createUndefined();
  }
}

Value globalDecodeURIComponent(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createString(ctx, "undefined");
  }

  std::string encodedComponent = args[0].toString(ctx)->getValue();
  std::string result;

  try {
    result = Encoding::decodeURIComponent(encodedComponent);
    return Value::createString(ctx, result);
  } catch (Error& e) {
    ctx->throwError(e);
    return Value::createUndefined();
  }
}

Value globalEscape(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createString(ctx, "undefined");
  }

  std::string str = args[0].toString(ctx)->getValue();
  std::string result;

  for (unsigned char c : str) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '@' || c == '*' || c == '_' ||
        c == '+' || c == '-' || c == '.' || c == '/') {
      result += c;
    } else if (c <= 0x7F) {
      // ASCII文字
      std::stringstream ss;
      ss << "%" << std::hex << std::uppercase << static_cast<int>(c);
      result += ss.str();
    } else {
      // Unicode文字
      std::stringstream ss;
      ss << "%u" << std::setfill('0') << std::setw(4) << std::hex << std::uppercase << static_cast<int>(c);
      result += ss.str();
    }
  }

  return Value::createString(ctx, result);
}

Value globalUnescape(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createString(ctx, "undefined");
  }

  std::string str = args[0].toString(ctx)->getValue();
  std::string result;

  for (size_t i = 0; i < str.length(); i++) {
    if (str[i] == '%') {
      if (i + 5 < str.length() && (str[i + 1] == 'u' || str[i + 1] == 'U')) {
        // %uXXXX形式
        try {
          std::string hexValue = str.substr(i + 2, 4);
          int codePoint = std::stoi(hexValue, nullptr, 16);

          if (codePoint <= 0xFFFF) {
            // UTF-8でエンコード
            if (codePoint <= 0x7F) {
              result += static_cast<char>(codePoint);
            } else if (codePoint <= 0x7FF) {
              result += static_cast<char>(0xC0 | (codePoint >> 6));
              result += static_cast<char>(0x80 | (codePoint & 0x3F));
            } else {
              result += static_cast<char>(0xE0 | (codePoint >> 12));
              result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
              result += static_cast<char>(0x80 | (codePoint & 0x3F));
            }
          }

          i += 5;
        } catch (std::exception&) {
          result += '%';
        }
      } else if (i + 2 < str.length()) {
        // %XX形式
        try {
          std::string hexValue = str.substr(i + 1, 2);
          int value = std::stoi(hexValue, nullptr, 16);
          result += static_cast<char>(value);
          i += 2;
        } catch (std::exception&) {
          result += '%';
        }
      } else {
        result += '%';
      }
    } else {
      result += str[i];
    }
  }

  return Value::createString(ctx, result);
}

namespace json {

Object* initialize(ExecutionContext* ctx, Object* globalObj) {
  // JSONオブジェクトの作成
  Object* jsonObj = Object::create(ctx);

  // メソッドの定義
  jsonObj->defineProperty(ctx, "parse", Value::createFunction(ctx, parse, 2, "parse"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  jsonObj->defineProperty(ctx, "stringify", Value::createFunction(ctx, stringify, 3, "stringify"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  // グローバルオブジェクトにJSONオブジェクトを追加
  globalObj->defineProperty(ctx, "JSON", Value(jsonObj),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Configurable));

  return jsonObj;
}

Value parse(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    ctx->throwError(Error::createSyntaxError(ctx, "JSON.parse: Invalid or unexpected token"));
    return Value::createUndefined();
  }

  std::string text = args[0].toString(ctx)->getValue();

  try {
    // JSONパーサーを使用して解析
    Value result = ctx->getJSONParser()->parse(text);

    // リバイバー関数が指定されている場合は適用
    if (args.size() > 1 && args[1].isCallable()) {
      Function* reviver = args[1].asFunction();

      // ルートオブジェクトを作成
      Object* root = Object::create(ctx);
      root->defineProperty(ctx, "", result, PropertyDescriptor::createDataDescriptorFlags(PropertyDescriptor::Writable | PropertyDescriptor::Enumerable | PropertyDescriptor::Configurable));

      // リバイバー関数を適用
      std::vector<Value> reviverArgs = {Value::createString(ctx, ""), result};
      return reviver->call(ctx, Value(root), reviverArgs);
    }

    return result;
  } catch (Error& e) {
    ctx->throwError(e);
    return Value::createUndefined();
  }
}

// JSONシリアライズ用の補助関数
static Value serializeJSONProperty(ExecutionContext* ctx, Object* holder, const std::string& key,
                                   Function* replacer, Value space,
                                   std::unordered_set<Object*>& stack) {
  // プロパティ値の取得
  Value property = holder->get(ctx, key);

  // toJSON関数を持つオブジェクトなら呼び出す
  if (property.isObject()) {
    Object* obj = property.asObject();
    Value toJSON = obj->get(ctx, "toJSON");

    if (toJSON.isCallable()) {
      Function* toJSONFn = toJSON.asFunction();
      std::vector<Value> args = {Value::createString(ctx, key)};
      property = toJSONFn->call(ctx, property, args);
    }
  }

  // Replacer関数があれば適用
  if (replacer) {
    std::vector<Value> args = {Value::createString(ctx, key), property};
    property = replacer->call(ctx, Value(holder), args);
  }

  // 値の種類に応じた処理
  if (property.isObject()) {
    Object* obj = property.asObject();

    // 循環参照チェック
    if (stack.find(obj) != stack.end()) {
      ctx->throwError(Error::createTypeError(ctx, "JSON.stringify: cyclic object value"));
      return Value::createUndefined();
    }

    // 型に応じた特別処理
    if (property.isNumber() || property.isString() || property.isBoolean()) {
      // プリミティブラッパーオブジェクト
      return property.toPrimitive(ctx);
    } else if (property.isNull()) {
      return Value::createNull();
    } else if (property.isFunction() || property.isSymbol()) {
      return Value::createUndefined();
    }

    // 配列かオブジェクトかの判定
    if (property.isArray()) {
      // 配列の処理
      Array* array = property.asArray();
      stack.insert(obj);

      std::vector<std::string> serializedItems;
      uint32_t length = array->getLength();

      for (uint32_t i = 0; i < length; i++) {
        std::string indexStr = std::to_string(i);
        Value item = serializeJSONProperty(ctx, array, indexStr, replacer, space, stack);

        if (item.isUndefined()) {
          serializedItems.push_back("null");
        } else {
          serializedItems.push_back(item.toString(ctx)->getValue());
        }
      }

      stack.erase(obj);

      // 整形のための空白
      std::string indent = "";
      if (space.isNumber()) {
        int spaceCount = std::min(10, static_cast<int>(space.toNumber(ctx)));
        indent = std::string(spaceCount, ' ');
      } else if (space.isString()) {
        indent = space.toString(ctx)->getValue();
        if (indent.length() > 10) {
          indent = indent.substr(0, 10);
        }
      }

      // 結果の組み立て
      std::string result;
      if (indent.empty()) {
        // コンパクト表示
        result = "[" + StringUtils::join(serializedItems, ",") + "]";
      } else {
        // 整形表示
        std::string itemsStr = StringUtils::join(serializedItems, ",\n" + indent);
        result = "[\n" + indent + itemsStr + "\n]";
      }

      return Value::createString(ctx, result);
    } else {
      // オブジェクトの処理
      stack.insert(obj);

      std::vector<std::string> serializedProps;
      auto properties = obj->getOwnPropertyKeys(ctx);

      for (const auto& key : properties) {
        if (obj->getPropertyAttributes(ctx, key) & PropertyDescriptor::Enumerable) {
          Value propValue = serializeJSONProperty(ctx, obj, key, replacer, space, stack);

          if (!propValue.isUndefined()) {
            std::string serializedKey = "\"" + StringUtils::escapeString(key) + "\"";
            serializedProps.push_back(serializedKey + ":" + propValue.toString(ctx)->getValue());
          }
        }
      }

      stack.erase(obj);

      // 整形のための空白
      std::string indent = "";
      if (space.isNumber()) {
        int spaceCount = std::min(10, static_cast<int>(space.toNumber(ctx)));
        indent = std::string(spaceCount, ' ');
      } else if (space.isString()) {
        indent = space.toString(ctx)->getValue();
        if (indent.length() > 10) {
          indent = indent.substr(0, 10);
        }
      }

      // 結果の組み立て
      std::string result;
      if (serializedProps.empty()) {
        result = "{}";
      } else if (indent.empty()) {
        // コンパクト表示
        result = "{" + StringUtils::join(serializedProps, ",") + "}";
      } else {
        // 整形表示
        std::string propsStr = StringUtils::join(serializedProps, ",\n" + indent);
        result = "{\n" + indent + propsStr + "\n}";
      }

      return Value::createString(ctx, result);
    }
  } else if (property.isString()) {
    // 文字列はダブルクォートでエスケープ
    std::string str = property.toString(ctx)->getValue();
    return Value::createString(ctx, "\"" + StringUtils::escapeString(str) + "\"");
  } else if (property.isNumber()) {
    // 数値は文字列に変換
    double num = property.toNumber(ctx);
    if (std::isfinite(num)) {
      return Value::createString(ctx, std::to_string(num));
    } else {
      return Value::createString(ctx, "null");
    }
  } else if (property.isBoolean()) {
    // 真偽値は "true" または "false" に変換
    return Value::createString(ctx, property.toBoolean() ? "true" : "false");
  } else if (property.isNull()) {
    // null は "null" に変換
    return Value::createString(ctx, "null");
  } else {
    // それ以外はundefinedとして扱い、スキップされるべき
    return Value::createUndefined();
  }
}

Value stringify(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createUndefined();
  }

  Value value = args[0];
  Function* replacer = nullptr;
  Value space = Value::createString(ctx, "");

  // replacerがArrayの場合の処理
  std::vector<std::string> propertyList;
  if (args.size() > 1 && !args[1].isUndefined() && !args[1].isNull()) {
    if (args[1].isFunction()) {
      replacer = args[1].asFunction();
    } else if (args[1].isArray()) {
      Array* array = args[1].asArray();
      uint32_t length = array->getLength();

      for (uint32_t i = 0; i < length; i++) {
        Value item = array->get(ctx, std::to_string(i));
        std::string name;

        if (item.isString()) {
          name = item.toString(ctx)->getValue();
        } else if (item.isNumber()) {
          name = std::to_string(static_cast<int>(item.toNumber(ctx)));
        } else if (item.isObject() && (item.isString() || item.isNumber())) {
          name = item.toPrimitive(ctx).toString(ctx)->getValue();
        }

        if (!name.empty() && std::find(propertyList.begin(), propertyList.end(), name) == propertyList.end()) {
          propertyList.push_back(name);
        }
      }
    }
  }

  // spaceの処理
  if (args.size() > 2 && !args[2].isUndefined()) {
    space = args[2];

    if (space.isNumber()) {
      int spaceCount = std::min(10, static_cast<int>(space.toNumber(ctx)));
      if (spaceCount > 0) {
        space = Value::createString(ctx, std::string(spaceCount, ' '));
      } else {
        space = Value::createString(ctx, "");
      }
    } else if (space.isString()) {
      std::string spaceStr = space.toString(ctx)->getValue();
      if (spaceStr.length() > 10) {
        spaceStr = spaceStr.substr(0, 10);
      }
      space = Value::createString(ctx, spaceStr);
    } else {
      space = Value::createString(ctx, "");
    }
  }

  // ルートオブジェクトを作成
  Object* wrapper = Object::create(ctx);
  wrapper->defineProperty(ctx, "", value, PropertyDescriptor::createDataDescriptorFlags(PropertyDescriptor::Writable | PropertyDescriptor::Enumerable | PropertyDescriptor::Configurable));

  // シリアライズ処理
  std::unordered_set<Object*> stack;
  Value serialized = serializeJSONProperty(ctx, wrapper, "", replacer, space, stack);

  if (serialized.isUndefined()) {
    return Value::createUndefined();
  }

  // ダブルクォートを取り除く（文字列以外の場合）
  std::string result = serialized.toString(ctx)->getValue();
  if (result.length() >= 2 && result[0] == '"' && result[result.length() - 1] == '"' && !value.isString()) {
    result = result.substr(1, result.length() - 2);
  }

  return Value::createString(ctx, result);
}

}  // namespace json

namespace math {

// 乱数生成器
static std::mt19937 s_randomEngine(std::random_device{}());
static std::uniform_real_distribution<double> s_uniformDist(0.0, 1.0);
Object* initialize(ExecutionContext* ctx, Object* globalObj) {
  // Mathオブジェクトの作成
  Object* mathObj = Object::create(ctx);

  // 数学定数の定義
  mathObj->defineProperty(ctx, "E", Value::createNumber(M_E),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "LN10", Value::createNumber(M_LN10),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "LN2", Value::createNumber(M_LN2),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "LOG10E", Value::createNumber(M_LOG10E),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "LOG2E", Value::createNumber(M_LOG2E),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "PI", Value::createNumber(M_PI),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "SQRT1_2", Value::createNumber(M_SQRT1_2),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "SQRT2", Value::createNumber(M_SQRT2),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Configurable));

  // 基本的な数学関数の定義
  mathObj->defineProperty(ctx, "abs", Value::createFunction(ctx, abs, 1, "abs"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "acos", Value::createFunction(ctx, acos, 1, "acos"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "acosh", Value::createFunction(ctx, acosh, 1, "acosh"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "asin", Value::createFunction(ctx, asin, 1, "asin"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "asinh", Value::createFunction(ctx, asinh, 1, "asinh"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "atan", Value::createFunction(ctx, atan, 1, "atan"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "atan2", Value::createFunction(ctx, atan2, 2, "atan2"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "atanh", Value::createFunction(ctx, atanh, 1, "atanh"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "cbrt", Value::createFunction(ctx, cbrt, 1, "cbrt"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "ceil", Value::createFunction(ctx, ceil, 1, "ceil"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "clz32", Value::createFunction(ctx, clz32, 1, "clz32"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "cos", Value::createFunction(ctx, cos, 1, "cos"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "cosh", Value::createFunction(ctx, cosh, 1, "cosh"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "exp", Value::createFunction(ctx, exp, 1, "exp"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "expm1", Value::createFunction(ctx, expm1, 1, "expm1"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "floor", Value::createFunction(ctx, floor, 1, "floor"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "fround", Value::createFunction(ctx, fround, 1, "fround"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "hypot", Value::createFunction(ctx, hypot, 2, "hypot"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "imul", Value::createFunction(ctx, imul, 2, "imul"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "log", Value::createFunction(ctx, log, 1, "log"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "log10", Value::createFunction(ctx, log10, 1, "log10"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "log1p", Value::createFunction(ctx, log1p, 1, "log1p"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "log2", Value::createFunction(ctx, log2, 1, "log2"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "max", Value::createFunction(ctx, max, 2, "max"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "min", Value::createFunction(ctx, min, 2, "min"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "pow", Value::createFunction(ctx, pow, 2, "pow"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "random", Value::createFunction(ctx, random, 0, "random"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "round", Value::createFunction(ctx, round, 1, "round"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "sign", Value::createFunction(ctx, sign, 1, "sign"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "sin", Value::createFunction(ctx, sin, 1, "sin"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "sinh", Value::createFunction(ctx, sinh, 1, "sinh"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "sqrt", Value::createFunction(ctx, sqrt, 1, "sqrt"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "tan", Value::createFunction(ctx, tan, 1, "tan"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "tanh", Value::createFunction(ctx, tanh, 1, "tanh"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  mathObj->defineProperty(ctx, "trunc", Value::createFunction(ctx, trunc, 1, "trunc"),
                          PropertyDescriptor::createDataDescriptorFlags(
                              PropertyDescriptor::Writable |
                              PropertyDescriptor::Configurable));

  // グローバルオブジェクトにMathオブジェクトを追加
  globalObj->defineProperty(ctx, "Math", Value(mathObj),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Configurable));

  return mathObj;
}

Value abs(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createNaN();
  }

  double num = args[0].toNumber(ctx);
  return Value::createNumber(std::abs(num));
}

Value random(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  return Value::createNumber(s_uniformDist(s_randomEngine));
}

Value max(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  double result = -std::numeric_limits<double>::infinity();

  if (args.empty()) {
    return Value::createNumber(result);
  }

  for (const auto& arg : args) {
    double num = arg.toNumber(ctx);
    if (std::isnan(num)) {
      return Value::createNaN();
    }
    result = std::max(result, num);
  }

  return Value::createNumber(result);
}

Value min(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  double result = std::numeric_limits<double>::infinity();

  if (args.empty()) {
    return Value::createNumber(result);
  }

  for (const auto& arg : args) {
    double num = arg.toNumber(ctx);
    if (std::isnan(num)) {
      return Value::createNaN();
    }
    result = std::min(result, num);
  }

  return Value::createNumber(result);
}

Value floor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createNaN();
  }

  double num = args[0].toNumber(ctx);
  return Value::createNumber(std::floor(num));
}

Value ceil(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createNaN();
  }

  double num = args[0].toNumber(ctx);
  return Value::createNumber(std::ceil(num));
}

Value round(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createNaN();
  }

  double num = args[0].toNumber(ctx);
  // ECMAScriptの仕様に従って丸める
  if (std::isnan(num) || std::isinf(num) || num == 0.0) {
    return Value::createNumber(num);
  }

  // -0.5から0.5の間は特別な処理
  if (num > -0.5 && num < 0.5) {
    return Value::createNumber(num < 0 ? -0.0 : 0.0);
  }

  // 標準的な丸め
  return Value::createNumber(std::floor(num + 0.5));
}

Value pow(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.size() < 2) {
    return Value::createNaN();
  }

  double base = args[0].toNumber(ctx);
  double exponent = args[1].toNumber(ctx);

  // 特殊ケースの処理
  if (std::isnan(exponent)) {
    return Value::createNaN();
  }

  if (exponent == 0) {
    return Value::createNumber(1.0);
  }

  if (std::isnan(base) && exponent != 0) {
    return Value::createNaN();
  }

  if (std::abs(base) == 1.0 && std::isinf(exponent)) {
    return Value::createNaN();
  }

  if ((base == 0 && exponent < 0) || (std::isinf(base) && exponent < 0)) {
    return Value::createNumber(0);
  }

  if (base == 0 && exponent > 0) {
    return Value::createNumber(exponent % 2 == 1 ? 0 : 0);
  }

  if (std::isinf(base) && exponent > 0) {
    return Value::createNumber(exponent % 2 == 1 ? base : std::abs(base));
  }

  if (base < 0 && std::fmod(exponent, 1.0) != 0.0) {
    return Value::createNaN();
  }

  return Value::createNumber(std::pow(base, exponent));
}

Value sqrt(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    return Value::createNaN();
  }

  double num = args[0].toNumber(ctx);
  if (num < 0) {
    return Value::createNaN();
  }

  return Value::createNumber(std::sqrt(num));
}

}  // namespace math

namespace reflect {

Object* initialize(ExecutionContext* ctx, Object* globalObj) {
  // Reflectオブジェクトの作成
  Object* reflectObj = Object::create(ctx);

  // メソッドの定義
  reflectObj->defineProperty(ctx, "apply", Value::createFunction(ctx, apply, 3, "apply"),
                             PropertyDescriptor::createDataDescriptorFlags(
                                 PropertyDescriptor::Writable |
                                 PropertyDescriptor::Configurable));

  reflectObj->defineProperty(ctx, "construct", Value::createFunction(ctx, construct, 3, "construct"),
                             PropertyDescriptor::createDataDescriptorFlags(
                                 PropertyDescriptor::Writable |
                                 PropertyDescriptor::Configurable));

  reflectObj->defineProperty(ctx, "defineProperty", Value::createFunction(ctx, defineProperty, 3, "defineProperty"),
                             PropertyDescriptor::createDataDescriptorFlags(
                                 PropertyDescriptor::Writable |
                                 PropertyDescriptor::Configurable));

  reflectObj->defineProperty(ctx, "deleteProperty", Value::createFunction(ctx, deleteProperty, 2, "deleteProperty"),
                             PropertyDescriptor::createDataDescriptorFlags(
                                 PropertyDescriptor::Writable |
                                 PropertyDescriptor::Configurable));

  reflectObj->defineProperty(ctx, "get", Value::createFunction(ctx, get, 3, "get"),
                             PropertyDescriptor::createDataDescriptorFlags(
                                 PropertyDescriptor::Writable |
                                 PropertyDescriptor::Configurable));

  reflectObj->defineProperty(ctx, "getOwnPropertyDescriptor",
                             Value::createFunction(ctx, getOwnPropertyDescriptor, 2, "getOwnPropertyDescriptor"),
                             PropertyDescriptor::createDataDescriptorFlags(
                                 PropertyDescriptor::Writable |
                                 PropertyDescriptor::Configurable));

  reflectObj->defineProperty(ctx, "has", Value::createFunction(ctx, has, 2, "has"),
                             PropertyDescriptor::createDataDescriptorFlags(
                                 PropertyDescriptor::Writable |
                                 PropertyDescriptor::Configurable));

  reflectObj->defineProperty(ctx, "ownKeys", Value::createFunction(ctx, ownKeys, 1, "ownKeys"),
                             PropertyDescriptor::createDataDescriptorFlags(
                                 PropertyDescriptor::Writable |
                                 PropertyDescriptor::Configurable));

  reflectObj->defineProperty(ctx, "preventExtensions",
                             Value::createFunction(ctx, preventExtensions, 1, "preventExtensions"),
                             PropertyDescriptor::createDataDescriptorFlags(
                                 PropertyDescriptor::Writable |
                                 PropertyDescriptor::Configurable));

  reflectObj->defineProperty(ctx, "set", Value::createFunction(ctx, set, 4, "set"),
                             PropertyDescriptor::createDataDescriptorFlags(
                                 PropertyDescriptor::Writable |
                                 PropertyDescriptor::Configurable));

  reflectObj->defineProperty(ctx, "setPrototypeOf",
                             Value::createFunction(ctx, setPrototypeOf, 2, "setPrototypeOf"),
                             PropertyDescriptor::createDataDescriptorFlags(
                                 PropertyDescriptor::Writable |
                                 PropertyDescriptor::Configurable));

  // グローバルオブジェクトにReflectオブジェクトを追加
  globalObj->defineProperty(ctx, "Reflect", Value(reflectObj),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Configurable));

  return reflectObj;
}

Value apply(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 3) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.apply: At least 3 arguments required"));
    return Value::createUndefined();
  }

  if (!args[0].isCallable()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.apply: First argument must be callable"));
    return Value::createUndefined();
  }

  if (!args[2].isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.apply: Third argument must be an array-like object"));
    return Value::createUndefined();
  }

  // ターゲット関数と引数の取得
  Function* target = args[0].asFunction();
  Value thisArg = args[1];
  Object* argsArray = args[2].asObject();

  // 引数配列の処理
  std::vector<Value> argList;
  uint32_t length = 0;
  Value lengthValue = argsArray->get(ctx, "length");

  if (lengthValue.isNumber()) {
    length = static_cast<uint32_t>(lengthValue.toNumber(ctx));
  }

  for (uint32_t i = 0; i < length; i++) {
    argList.push_back(argsArray->get(ctx, std::to_string(i)));
  }

  // 関数の呼び出し
  return target->call(ctx, thisArg, argList);
}

Value construct(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 2) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.construct: At least 2 arguments required"));
    return Value::createUndefined();
  }

  if (!args[0].isConstructor()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.construct: First argument must be a constructor"));
    return Value::createUndefined();
  }

  if (!args[1].isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.construct: Second argument must be an array-like object"));
    return Value::createUndefined();
  }

  // newTargetの検証（指定されている場合）
  Function* newTarget = nullptr;
  if (args.size() >= 3) {
    if (!args[2].isConstructor()) {
      ctx->throwError(Error::createTypeError(ctx, "Reflect.construct: Third argument must be a constructor"));
      return Value::createUndefined();
    }
    newTarget = args[2].asFunction();
  } else {
    newTarget = args[0].asFunction();
  }

  // ターゲットコンストラクタと引数の取得
  Function* target = args[0].asFunction();
  Object* argsArray = args[1].asObject();

  // 引数配列の処理
  std::vector<Value> argList;
  uint32_t length = 0;
  Value lengthValue = argsArray->get(ctx, "length");

  if (lengthValue.isNumber()) {
    length = static_cast<uint32_t>(lengthValue.toNumber(ctx));
  }

  for (uint32_t i = 0; i < length; i++) {
    argList.push_back(argsArray->get(ctx, std::to_string(i)));
  }

  // オブジェクトの構築
  return target->construct(ctx, argList, newTarget);
}

Value defineProperty(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証 - 最低3つの引数が必要
  if (args.size() < 3) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.defineProperty: At least 3 arguments required"));
    return Value::createBoolean(false);
  }

  // 第一引数はオブジェクトである必要がある
  if (!args[0].isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.defineProperty: First argument must be an object"));
    return Value::createBoolean(false);
  }

  // ターゲットオブジェクトの取得
  Object* target = args[0].asObject();
  
  // プロパティキーの処理 - シンボルまたは文字列に変換
  std::string propertyKey;
  if (args[1].isSymbol()) {
    // シンボルの場合は内部記述子を取得
    propertyKey = args[1].asSymbol()->getDescription()->getValue();
  } else {
    // それ以外の場合は文字列に変換
    propertyKey = args[1].toString(ctx)->getValue();
    
    // 文字列変換中にエラーが発生した場合の処理
    if (ctx->hasException()) {
      return Value::createBoolean(false);
    }
  }

  // プロパティ記述子オブジェクトの検証
  if (!args[2].isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.defineProperty: Third argument must be an object"));
    return Value::createBoolean(false);
  }

  // 記述子オブジェクトの取得と変換
  Object* attributes = args[2].asObject();
  PropertyDescriptor descriptor;

  // 記述子の適用とプロパティ定義の実行
  try {
    // オブジェクトからプロパティ記述子を構築
    descriptor = PropertyDescriptor::fromObject(ctx, attributes);
    
    // 例外が発生していないか確認
    if (ctx->hasException()) {
      return Value::createBoolean(false);
    }
    
    // ターゲットオブジェクトにプロパティを定義
    bool success = target->defineOwnProperty(ctx, propertyKey, descriptor);
    return Value::createBoolean(success);
  } catch (Error& e) {
    // エラーが発生した場合は例外をスローして失敗を返す
    ctx->throwError(e);
    return Value::createBoolean(false);
  }
}

Value deleteProperty(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 2) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.deleteProperty: At least 2 arguments required"));
    return Value::createBoolean(false);
  }

  if (!args[0].isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.deleteProperty: First argument must be an object"));
    return Value::createBoolean(false);
  }

  // オブジェクトとプロパティキーの取得
  Object* target = args[0].asObject();
  std::string propertyKey;

  if (args[1].isSymbol()) {
    propertyKey = args[1].asSymbol()->getDescription()->getValue();
  } else {
    propertyKey = args[1].toString(ctx)->getValue();
  }

  // プロパティの削除
  bool success = target->deleteProperty(ctx, propertyKey);
  return Value::createBoolean(success);
}

Value get(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 2) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.get: At least 2 arguments required"));
    return Value::createUndefined();
  }

  if (!args[0].isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.get: First argument must be an object"));
    return Value::createUndefined();
  }

  // オブジェクトとプロパティキーの取得
  Object* target = args[0].asObject();
  std::string propertyKey;

  if (args[1].isSymbol()) {
    propertyKey = args[1].asSymbol()->getDescription()->getValue();
  } else {
    propertyKey = args[1].toString(ctx)->getValue();
  }

  // レシーバーの処理（指定されている場合）
  Value receiver = args.size() >= 3 ? args[2] : args[0];

  // プロパティの取得
  return target->get(ctx, propertyKey, receiver);
}

Value getOwnPropertyDescriptor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 2) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.getOwnPropertyDescriptor: At least 2 arguments required"));
    return Value::createUndefined();
  }

  if (!args[0].isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.getOwnPropertyDescriptor: First argument must be an object"));
    return Value::createUndefined();
  }

  // オブジェクトとプロパティキーの取得
  Object* target = args[0].asObject();
  std::string propertyKey;

  if (args[1].isSymbol()) {
    propertyKey = args[1].asSymbol()->getDescription()->getValue();
  } else {
    propertyKey = args[1].toString(ctx)->getValue();
  }

  // プロパティ記述子の取得
  PropertyDescriptor descriptor;
  if (!target->getOwnPropertyDescriptor(ctx, propertyKey, &descriptor)) {
    return Value::createUndefined();
  }

  // 記述子オブジェクトの作成
  Object* descObj = Object::create(ctx);

  // 記述子プロパティの設定
  if (descriptor.hasValue()) {
    descObj->defineProperty(ctx, "value", descriptor.getValue(),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Enumerable |
                                PropertyDescriptor::Configurable));
  }

  if (descriptor.hasWritable()) {
    descObj->defineProperty(ctx, "writable", Value::createBoolean(descriptor.isWritable()),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Enumerable |
                                PropertyDescriptor::Configurable));
  }

  if (descriptor.hasGet()) {
    descObj->defineProperty(ctx, "get", descriptor.getGetter(),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Enumerable |
                                PropertyDescriptor::Configurable));
  }

  if (descriptor.hasSet()) {
    descObj->defineProperty(ctx, "set", descriptor.getSetter(),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Enumerable |
                                PropertyDescriptor::Configurable));
  }

  if (descriptor.hasEnumerable()) {
    descObj->defineProperty(ctx, "enumerable", Value::createBoolean(descriptor.isEnumerable()),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Enumerable |
                                PropertyDescriptor::Configurable));
  }

  if (descriptor.hasConfigurable()) {
    descObj->defineProperty(ctx, "configurable", Value::createBoolean(descriptor.isConfigurable()),
                            PropertyDescriptor::createDataDescriptorFlags(
                                PropertyDescriptor::Writable |
                                PropertyDescriptor::Enumerable |
                                PropertyDescriptor::Configurable));
  }

  return Value(descObj);
}

Value has(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 2) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.has: At least 2 arguments required"));
    return Value::createBoolean(false);
  }

  if (!args[0].isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.has: First argument must be an object"));
    return Value::createBoolean(false);
  }

  // オブジェクトとプロパティキーの取得
  Object* target = args[0].asObject();
  std::string propertyKey;

  if (args[1].isSymbol()) {
    propertyKey = args[1].asSymbol()->getDescription()->getValue();
  } else {
    propertyKey = args[1].toString(ctx)->getValue();
  }

  // プロパティの存在確認
  return Value::createBoolean(target->hasProperty(ctx, propertyKey));
}

Value ownKeys(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.empty()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.ownKeys: At least 1 argument required"));
    return Value::createUndefined();
  }

  if (!args[0].isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.ownKeys: First argument must be an object"));
    return Value::createUndefined();
  }

  // オブジェクトの取得
  Object* target = args[0].asObject();

  // プロパティキーの取得
  auto keys = target->getOwnPropertyKeys(ctx);

  // 配列の作成
  Array* result = Array::create(ctx);

  for (size_t i = 0; i < keys.size(); i++) {
    const auto& key = keys[i];

    // キーがシンボルかどうかを判定
    Value keyValue;
    if (target->hasSymbolProperty(key)) {
      // シンボル名からシンボルオブジェクトを取得
      keyValue = Symbol::for_(ctx, key);
    } else {
      keyValue = Value::createString(ctx, key);
    }

    result->defineProperty(ctx, std::to_string(i), keyValue,
                           PropertyDescriptor::createDataDescriptorFlags(
                               PropertyDescriptor::Writable |
                               PropertyDescriptor::Enumerable |
                               PropertyDescriptor::Configurable));
  }

  // 配列の長さを設定
  result->setLength(ctx, keys.size());

  return Value(result);
}

Value preventExtensions(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.empty()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.preventExtensions: At least 1 argument required"));
    return Value::createBoolean(false);
  }

  if (!args[0].isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.preventExtensions: First argument must be an object"));
    return Value::createBoolean(false);
  }

  // オブジェクトの取得
  Object* target = args[0].asObject();

  // 拡張防止
  bool success = target->preventExtensions(ctx);
  return Value::createBoolean(success);
}

Value set(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 3) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.set: At least 3 arguments required"));
    return Value::createBoolean(false);
  }

  if (!args[0].isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.set: First argument must be an object"));
    return Value::createBoolean(false);
  }

  // オブジェクトとプロパティキーの取得
  Object* target = args[0].asObject();
  std::string propertyKey;

  if (args[1].isSymbol()) {
    propertyKey = args[1].asSymbol()->getDescription()->getValue();
  } else {
    propertyKey = args[1].toString(ctx)->getValue();
  }

  // 値の取得
  Value value = args[2];

  // レシーバーの処理（指定されている場合）
  Value receiver = args.size() >= 4 ? args[3] : args[0];

  // プロパティの設定
  bool success = target->set(ctx, propertyKey, value, receiver);
  return Value::createBoolean(success);
}

Value setPrototypeOf(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 2) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.setPrototypeOf: At least 2 arguments required"));
    return Value::createBoolean(false);
  }

  if (!args[0].isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.setPrototypeOf: First argument must be an object"));
    return Value::createBoolean(false);
  }

  if (!args[1].isObject() && !args[1].isNull()) {
    ctx->throwError(Error::createTypeError(ctx, "Reflect.setPrototypeOf: Second argument must be an object or null"));
    return Value::createBoolean(false);
  }

  // オブジェクトの取得
  Object* target = args[0].asObject();
  Object* proto = args[1].isNull() ? nullptr : args[1].asObject();

  // プロトタイプの設定
  bool success = target->setPrototype(ctx, proto);
  return Value::createBoolean(success);
}

}  // namespace reflect

}  // namespace aero