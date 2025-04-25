/**
 * @file json.cpp
 * @brief JavaScript の JSON 組み込みオブジェクトの実装
 * @copyright 2023 AeroJS Project
 */

#include "json.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "core/runtime/array.h"
#include "core/runtime/context.h"
#include "core/runtime/error.h"
#include "core/runtime/function.h"
#include "core/runtime/global_object.h"

namespace aero {

// JSONパーサーの実装
JSONParser::JSONParser(const std::string& text, Context* context)
    : m_text(text), m_length(text.length()), m_pos(0), m_context(context) {
}

Value JSONParser::parse(const std::string& text, Context* context) {
  JSONParser parser(text, context);
  return parser.doParse();
}

Value JSONParser::doParse() {
  skipWhitespace();

  // JSONテキストは1つの値から構成される
  Value result = parseValue();

  skipWhitespace();

  // すべてのテキストが消費されていることを確認
  if (m_pos < m_length) {
    throwSyntaxError("予期しない追加の文字があります");
    return Value::undefined();
  }

  return result;
}

Value JSONParser::parseValue() {
  skipWhitespace();

  char c = peekChar();

  switch (c) {
    case '{':
      return parseObject();
    case '[':
      return parseArray();
    case '"':
      return parseString();
    case 't':
    case 'f':
    case 'n':
      return parseKeyword();
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return parseNumber();
    default:
      throwSyntaxError("予期しないトークン");
      return Value::undefined();
  }
}

Value JSONParser::parseObject() {
  // 新しいオブジェクトを作成
  Object* obj = new Object(m_context->globalObject()->objectPrototype());

  // 開始の波括弧をスキップ
  nextChar();  // '{'
  skipWhitespace();

  // 空のオブジェクトの場合
  if (peekChar() == '}') {
    nextChar();  // '}'
    return Value(obj);
  }

  while (true) {
    skipWhitespace();

    // キーは文字列でなければならない
    if (peekChar() != '"') {
      throwSyntaxError("オブジェクトキーは文字列でなければなりません");
      return Value::undefined();
    }

    // キーを解析
    Value keyValue = parseString();
    std::string key = keyValue.toString();

    skipWhitespace();

    // キーと値の間はコロンで区切られる
    if (nextChar() != ':') {
      throwSyntaxError("オブジェクトのキーと値の間にコロンが必要です");
      return Value::undefined();
    }

    skipWhitespace();

    // 値を解析
    Value value = parseValue();

    // プロパティを設定
    obj->set(key, value);

    skipWhitespace();

    // オブジェクトの終了または次のプロパティ
    char next = nextChar();
    if (next == '}') {
      break;  // オブジェクトの終了
    } else if (next != ',') {
      throwSyntaxError("オブジェクトのプロパティはカンマで区切る必要があります");
      return Value::undefined();
    }

    // 末尾のカンマは許可されていない
    skipWhitespace();
    if (peekChar() == '}') {
      throwSyntaxError("オブジェクトの末尾にカンマを使用できません");
      return Value::undefined();
    }
  }

  return Value(obj);
}

Value JSONParser::parseArray() {
  // 新しい配列を作成
  ArrayObject* array = new ArrayObject(m_context->globalObject()->arrayPrototype());

  // 開始の角括弧をスキップ
  nextChar();  // '['
  skipWhitespace();

  // 空の配列の場合
  if (peekChar() == ']') {
    nextChar();  // ']'
    return Value(array);
  }

  size_t index = 0;

  while (true) {
    skipWhitespace();

    // 要素を解析
    Value element = parseValue();

    // 要素を追加
    array->set(std::to_string(index), element);
    index++;

    skipWhitespace();

    // 配列の終了または次の要素
    char next = nextChar();
    if (next == ']') {
      break;  // 配列の終了
    } else if (next != ',') {
      throwSyntaxError("配列の要素はカンマで区切る必要があります");
      return Value::undefined();
    }

    // 末尾のカンマは許可されていない
    skipWhitespace();
    if (peekChar() == ']') {
      throwSyntaxError("配列の末尾にカンマを使用できません");
      return Value::undefined();
    }
  }

  // lengthプロパティを設定
  array->defineOwnProperty("length", Object::PropertyDescriptor(Value(static_cast<double>(index)),
                                                                Object::PropertyDescriptor::Writable |
                                                                    Object::PropertyDescriptor::Configurable));

  return Value(array);
}

Value JSONParser::parseString() {
  std::string str;

  // 開始の二重引用符をスキップ
  nextChar();  // '"'

  while (m_pos < m_length) {
    char c = nextChar();

    if (c == '"') {
      // 文字列の終了
      return Value(new String(str));
    } else if (c == '\\') {
      // エスケープシーケンス
      if (m_pos >= m_length) {
        throwSyntaxError("文字列が終了していません");
        return Value::undefined();
      }

      char escape = nextChar();
      switch (escape) {
        case '"':
        case '\\':
        case '/':
          str += escape;
          break;
        case 'b':
          str += '\b';
          break;
        case 'f':
          str += '\f';
          break;
        case 'n':
          str += '\n';
          break;
        case 'r':
          str += '\r';
          break;
        case 't':
          str += '\t';
          break;
        case 'u': {
          // 4桁の16進数
          if (m_pos + 4 > m_length) {
            throwSyntaxError("不完全なUnicodeエスケープシーケンス");
            return Value::undefined();
          }

          std::string hex;
          for (int i = 0; i < 4; i++) {
            hex += nextChar();
          }

          // 16進数を解析
          char* end;
          int codePoint = std::strtol(hex.c_str(), &end, 16);

          if (*end != '\0') {
            throwSyntaxError("無効なUnicodeエスケープシーケンス");
            return Value::undefined();
          }

          // コードポイントをUTF-8エンコーディングに変換
          if (codePoint < 0x80) {
            // 1バイト
            str += static_cast<char>(codePoint);
          } else if (codePoint < 0x800) {
            // 2バイト
            str += static_cast<char>(0xC0 | (codePoint >> 6));
            str += static_cast<char>(0x80 | (codePoint & 0x3F));
          } else {
            // 3バイト
            str += static_cast<char>(0xE0 | (codePoint >> 12));
            str += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            str += static_cast<char>(0x80 | (codePoint & 0x3F));
          }
          break;
        }
        default:
          throwSyntaxError("無効なエスケープシーケンス");
          return Value::undefined();
      }
    } else if (static_cast<unsigned char>(c) < 0x20) {
      // 制御文字は許可されていない
      throwSyntaxError("文字列に制御文字を含めることはできません");
      return Value::undefined();
    } else {
      // 通常の文字
      str += c;
    }
  }

  // 終了の二重引用符なしで文字列が終了した
  throwSyntaxError("文字列が終了していません");
  return Value::undefined();
}

Value JSONParser::parseNumber() {
  std::string numStr;
  bool negative = false;

  // 符号
  if (peekChar() == '-') {
    negative = true;
    numStr += nextChar();
  }

  // 整数部
  if (peekChar() == '0') {
    numStr += nextChar();

    // 0の後に数字が続くことはできない
    if (m_pos < m_length && std::isdigit(peekChar())) {
      throwSyntaxError("数値の整数部が0で始まる場合、その後に数字を続けることはできません");
      return Value::undefined();
    }
  } else if (std::isdigit(peekChar())) {
    // 1-9の数字
    numStr += nextChar();

    // 追加の数字
    while (m_pos < m_length && std::isdigit(peekChar())) {
      numStr += nextChar();
    }
  } else {
    throwSyntaxError("無効な数値");
    return Value::undefined();
  }

  // 小数部
  if (m_pos < m_length && peekChar() == '.') {
    numStr += nextChar();

    // 小数点の後には少なくとも1つの数字が必要
    if (m_pos >= m_length || !std::isdigit(peekChar())) {
      throwSyntaxError("小数点の後には少なくとも1つの数字が必要です");
      return Value::undefined();
    }

    // 小数部の数字
    while (m_pos < m_length && std::isdigit(peekChar())) {
      numStr += nextChar();
    }
  }

  // 指数部
  if (m_pos < m_length && (peekChar() == 'e' || peekChar() == 'E')) {
    numStr += nextChar();

    // 指数の符号
    if (m_pos < m_length && (peekChar() == '+' || peekChar() == '-')) {
      numStr += nextChar();
    }

    // 指数には少なくとも1つの数字が必要
    if (m_pos >= m_length || !std::isdigit(peekChar())) {
      throwSyntaxError("指数の後には少なくとも1つの数字が必要です");
      return Value::undefined();
    }

    // 指数部の数字
    while (m_pos < m_length && std::isdigit(peekChar())) {
      numStr += nextChar();
    }
  }

  // 文字列を数値に変換
  try {
    double num = std::stod(numStr);
    return Value(num);
  } catch (const std::exception&) {
    throwSyntaxError("数値の解析に失敗しました");
    return Value::undefined();
  }
}

Value JSONParser::parseKeyword() {
  // true, false, nullのいずれかを検証
  if (m_pos + 4 <= m_length && m_text.substr(m_pos, 4) == "true") {
    // "true"
    m_pos += 4;
    return Value(true);
  } else if (m_pos + 5 <= m_length && m_text.substr(m_pos, 5) == "false") {
    // "false"
    m_pos += 5;
    return Value(false);
  } else if (m_pos + 4 <= m_length && m_text.substr(m_pos, 4) == "null") {
    // "null"
    m_pos += 4;
    return Value::null();
  } else {
    throwSyntaxError("無効なトークン");
    return Value::undefined();
  }
}

void JSONParser::skipWhitespace() {
  while (m_pos < m_length) {
    char c = m_text[m_pos];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      m_pos++;
    } else {
      break;
    }
  }
}

char JSONParser::nextChar() {
  if (m_pos >= m_length) {
    throwSyntaxError("予期しないテキストの終了");
    return '\0';
  }
  return m_text[m_pos++];
}

char JSONParser::peekChar() {
  if (m_pos >= m_length) {
    throwSyntaxError("予期しないテキストの終了");
    return '\0';
  }
  return m_text[m_pos];
}

void JSONParser::backChar() {
  if (m_pos > 0) {
    m_pos--;
  }
}

void JSONParser::throwSyntaxError(const std::string& message) {
  m_context->throwSyntaxError(message);
}

// JSON文字列化の実装
JSONStringifier::JSONStringifier(Value replacer, Value space, Context* context)
    : m_indent(""), m_replacerFunction(Value::undefined()), m_propertyList(), m_context(context), m_stack() {
  // spaceパラメータの処理
  if (space.isNumber()) {
    // 数値の場合、その数値分のスペースをインデントとして使用
    int spaces = std::min(10, static_cast<int>(std::max(0.0, space.toNumber())));
    for (int i = 0; i < spaces; i++) {
      m_indent += " ";
    }
  } else if (space.isString()) {
    // 文字列の場合、最初の10文字までをインデントとして使用
    std::string spaceStr = space.toString();
    m_indent = spaceStr.substr(0, std::min(10u, static_cast<unsigned int>(spaceStr.length())));
  }

  // replacerパラメータの処理
  if (replacer.isFunction()) {
    // 関数の場合、置換関数として使用
    m_replacerFunction = replacer;
  } else if (replacer.isObject() && replacer.asObject()->isArray()) {
    // 配列の場合、プロパティのホワイトリストとして使用
    Object* array = replacer.asObject();
    Value lengthVal = array->get("length");

    if (lengthVal.isNumber()) {
      size_t length = static_cast<size_t>(lengthVal.toNumber());

      for (size_t i = 0; i < length; i++) {
        Value item = array->get(std::to_string(i));

        // プロパティ名として使用できる値（文字列または数値）のみを追加
        if (item.isString() || item.isNumber()) {
          std::string propName = item.toString();

          // 重複を避ける
          if (std::find(m_propertyList.begin(), m_propertyList.end(), propName) == m_propertyList.end()) {
            m_propertyList.push_back(propName);
          }
        }
      }
    }
  }
}

Value JSONStringifier::stringify(Value value, Value replacer, Value space, Context* context) {
  JSONStringifier stringifier(replacer, space, context);

  // ルートオブジェクトを{"": value}としてラップ
  Object* wrapper = new Object(context->globalObject()->objectPrototype());
  wrapper->set("", value);

  std::string result = stringifier.doStringify(value, Value(wrapper), "");

  if (result.empty()) {
    return Value::undefined();
  }

  return Value(new String(result));
}

std::string JSONStringifier::doStringify(Value value, Value holder, const std::string& key) {
  // 置換関数を適用
  Value original = value;
  value = applyReplacer(holder, key, value);

  // 型に基づいて文字列化
  if (value.isString()) {
    return escapeString(value.toString());
  } else if (value.isNumber()) {
    // 特殊な値をチェック
    double num = value.toNumber();
    if (std::isnan(num) || std::isinf(num)) {
      return "null";  // NaNとInfinityはnullに変換
    }

    // 通常の数値を文字列化
    std::ostringstream ss;
    ss << std::setprecision(16) << num;
    return ss.str();
  } else if (value.isBoolean()) {
    return value.toBoolean() ? "true" : "false";
  } else if (value.isNull()) {
    return "null";
  } else if (value.isUndefined()) {
    return "";  // undefinedは結果から除外される
  } else if (value.isObject()) {
    Object* obj = value.asObject();

    // toJSONメソッドをチェック
    if (obj->has("toJSON") && obj->get("toJSON").isFunction()) {
      // toJSONメソッドを呼び出し
      std::vector<Value> args = {Value(new String(key))};
      Value jsonValue = obj->callMethod("toJSON", args, m_context);

      // 結果を再帰的に文字列化
      return doStringify(jsonValue, holder, key);
    }

    // 循環参照のチェック
    for (Object* stackObj : m_stack) {
      if (stackObj == obj) {
        m_context->throwTypeError("循環参照を文字列化できません");
        return "";
      }
    }

    // オブジェクトをスタックに追加
    m_stack.push_back(obj);

    std::string result;

    // 配列とオブジェクトの文字列化
    if (obj->isArray()) {
      result = stringifyArray(Value(obj));
    } else {
      result = stringifyObject(Value(obj));
    }

    // スタックからオブジェクトを削除
    m_stack.pop_back();

    return result;
  }

  // 未知の型（通常は発生しない）
  return "";
}

std::string JSONStringifier::stringifyObject(Value value) {
  Object* obj = value.asObject();
  std::vector<std::string> properties;

  // プロパティリストが指定されている場合はそれを使用
  // そうでなければオブジェクトのすべてのプロパティを使用
  std::vector<std::string> keys;
  if (!m_propertyList.empty()) {
    keys = m_propertyList;
  } else {
    keys = obj->getOwnPropertyNames();
  }

  // 各プロパティを文字列化
  for (const std::string& key : keys) {
    // プロパティが存在するか確認
    if (!obj->has(key)) {
      continue;
    }

    Value propValue = obj->get(key);
    std::string propStr = doStringify(propValue, value, key);

    // 結果が空でない場合のみ追加（undefinedや循環参照の場合は除外）
    if (!propStr.empty()) {
      properties.push_back(escapeString(key) + ":" +
                           (m_indent.empty() ? "" : " ") +
                           propStr);
    }
  }

  // プロパティがない場合は空のオブジェクト
  if (properties.empty()) {
    return "{}";
  }

  // インデントがない場合は1行で
  if (m_indent.empty()) {
    return "{" + joinString(properties, ",") + "}";
  }

  // インデントがある場合は複数行で
  std::string result = "{\n";
  for (size_t i = 0; i < properties.size(); i++) {
    result += makeIndent(1) + properties[i];
    if (i < properties.size() - 1) {
      result += ",";
    }
    result += "\n";
  }
  result += "}";

  return result;
}

std::string JSONStringifier::stringifyArray(Value value) {
  Object* array = value.asObject();
  Value lengthVal = array->get("length");

  if (!lengthVal.isNumber()) {
    return "[]";  // 長さがない場合は空の配列
  }

  size_t length = static_cast<size_t>(lengthVal.toNumber());
  std::vector<std::string> elements;

  // 各要素を文字列化
  for (size_t i = 0; i < length; i++) {
    std::string key = std::to_string(i);

    // 配列の要素が存在するか確認
    if (!array->has(key)) {
      elements.push_back("null");  // 存在しない要素はnull
      continue;
    }

    Value elemValue = array->get(key);
    std::string elemStr = doStringify(elemValue, value, key);

    // 結果が空の場合はnullを使用（undefinedや循環参照の場合）
    if (elemStr.empty()) {
      elements.push_back("null");
    } else {
      elements.push_back(elemStr);
    }
  }

  // 要素がない場合は空の配列
  if (elements.empty()) {
    return "[]";
  }

  // インデントがない場合は1行で
  if (m_indent.empty()) {
    return "[" + joinString(elements, ",") + "]";
  }

  // インデントがある場合は複数行で
  std::string result = "[\n";
  for (size_t i = 0; i < elements.size(); i++) {
    result += makeIndent(1) + elements[i];
    if (i < elements.size() - 1) {
      result += ",";
    }
    result += "\n";
  }
  result += "]";

  return result;
}

std::string JSONStringifier::escapeString(const std::string& str) {
  std::string result = "\"";

  for (char c : str) {
    switch (c) {
      case '"':
        result += "\\\"";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '/':
        result += "\\/";
        break;
      case '\b':
        result += "\\b";
        break;
      case '\f':
        result += "\\f";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          // 制御文字はユニコードエスケープシーケンスで表現
          char buf[7];
          std::snprintf(buf, sizeof(buf), "\\u%04X", static_cast<unsigned char>(c));
          result += buf;
        } else {
          result += c;
        }
        break;
    }
  }

  result += "\"";
  return result;
}

std::string JSONStringifier::makeIndent(int level) {
  std::string result;
  for (int i = 0; i < level; i++) {
    result += m_indent;
  }
  return result;
}

Value JSONStringifier::applyReplacer(Value holder, const std::string& key, Value value) {
  // 置換関数が定義されていれば適用
  if (m_replacerFunction.isFunction()) {
    std::vector<Value> args = {Value(new String(key)), value};
    return m_replacerFunction.call(holder, args, m_context);
  }

  return value;
}

// ヘルパー関数: 文字列の配列を結合
std::string joinString(const std::vector<std::string>& strings, const std::string& delimiter) {
  std::string result;
  for (size_t i = 0; i < strings.size(); i++) {
    result += strings[i];
    if (i < strings.size() - 1) {
      result += delimiter;
    }
  }
  return result;
}

// JSON.parse実装
Value jsonParse(Value thisValue, const std::vector<Value>& arguments, Context* context) {
  // 引数のチェック
  if (arguments.empty()) {
    context->throwSyntaxError("JSON.parseには少なくとも1つの引数が必要です");
    return Value::undefined();
  }

  // 第1引数をテキストとして取得
  std::string text = arguments[0].toString();

  // テキストを解析
  Value result = JSONParser::parse(text, context);

  // reviverが指定されている場合は適用
  if (arguments.size() > 1 && arguments[1].isFunction()) {
    // reviverをラップするオブジェクトを作成
    Object* wrapper = new Object(context->globalObject()->objectPrototype());
    wrapper->set("", result);

    // 再帰的に処理
    std::function<Value(Object*, const std::string&)> walk = [&](Object* obj, const std::string& name) -> Value {
      Value val = obj->get(name);

      if (val.isObject()) {
        Object* valObj = val.asObject();

        if (valObj->isArray()) {
          // 配列の場合はインデックスベースでreviver適用
          Value lengthVal = valObj->get("length");
          if (lengthVal.isNumber()) {
            size_t length = static_cast<size_t>(lengthVal.toNumber());

            for (size_t i = 0; i < length; i++) {
              std::string index = std::to_string(i);
              Value newVal = walk(valObj, index);

              if (newVal.isUndefined()) {
                valObj->remove(index);
              } else {
                valObj->set(index, newVal);
              }
            }
          }
        } else {
          // 通常のオブジェクトの場合はプロパティベースでreviver適用
          std::vector<std::string> keys = valObj->getOwnPropertyNames();

          for (const std::string& key : keys) {
            Value newVal = walk(valObj, key);

            if (newVal.isUndefined()) {
              valObj->remove(key);
            } else {
              valObj->set(key, newVal);
            }
          }
        }
      }

      // reviverを適用
      std::vector<Value> args = {Value(new String(name)), val};
      return arguments[1].call(Value(obj), args, context);
    };

    // ルートから処理を開始
    result = walk(wrapper, "");
  }

  return result;
}

// JSON.stringify実装
Value jsonStringify(Value thisValue, const std::vector<Value>& arguments, Context* context) {
  // 引数のチェック
  if (arguments.empty()) {
    return Value::undefined();
  }

  // 第1引数は変換する値
  Value value = arguments[0];

  // 第2引数はレプラサー（デフォルトはundefined）
  Value replacer = arguments.size() > 1 ? arguments[1] : Value::undefined();

  // 第3引数はスペース（デフォルトはundefined）
  Value space = arguments.size() > 2 ? arguments[2] : Value::undefined();

  // 値をJSON文字列に変換
  return JSONStringifier::stringify(value, replacer, space, context);
}

// JSON組み込みオブジェクトの登録
void registerJSONBuiltin(GlobalObject* global) {
  if (!global) {
    return;
  }

  Context* context = global->context();
  if (!context) {
    return;
  }

  // JSONオブジェクトを作成
  Object* jsonObj = new Object(context->objectPrototype());

  // JSONメソッドを追加
  jsonObj->defineOwnProperty("parse",
                             Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, jsonParse, 2,
                                                                                 context->staticStrings()->parse),
                                                        Object::PropertyDescriptor::Writable |
                                                            Object::PropertyDescriptor::Configurable));

  jsonObj->defineOwnProperty("stringify",
                             Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, jsonStringify, 3,
                                                                                 context->staticStrings()->stringify),
                                                        Object::PropertyDescriptor::Writable |
                                                            Object::PropertyDescriptor::Configurable));

  // JSON[@@toStringTag]
  jsonObj->defineOwnProperty(Symbol::wellKnown(context)->toStringTag,
                             Object::PropertyDescriptor(context->staticStrings()->JSON,
                                                        Object::PropertyDescriptor::Configurable));

  // グローバルオブジェクトにJSONを登録
  global->defineOwnProperty(context->staticStrings()->JSON,
                            Object::PropertyDescriptor(Value(jsonObj),
                                                       Object::PropertyDescriptor::Writable |
                                                           Object::PropertyDescriptor::Configurable));
}

}  // namespace aero