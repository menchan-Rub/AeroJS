/**
 * @file string.cpp
 * @brief JavaScript の String オブジェクトの実装
 */

#include "string.h"

#include <algorithm>
#include <codecvt>
#include <iomanip>
#include <locale>
#include <sstream>

#include "../../../function.h"
#include "../../../object.h"
#include "../../../value.h"

namespace aerojs {
namespace core {

// StringObjectクラスの実装
StringObject::StringObject(const std::string& value, ObjectPtr prototype)
    : Object(prototype), m_primitiveValue(value) {
}

StringObjectPtr StringObject::create(const std::string& value, ObjectPtr prototype) {
  auto obj = std::make_shared<StringObject>(value, prototype);

  // length プロパティを設定
  obj->defineProperty("length", Value::fromNumber(static_cast<double>(obj->length())),
                      PropertyAttributes::ReadOnly | PropertyAttributes::DontEnum | PropertyAttributes::DontDelete);

  return obj;
}

const std::string& StringObject::primitiveValue() const {
  return m_primitiveValue;
}
size_t StringObject::length() const {
  // ECMAScript仕様に従い、文字列の長さはUTF-16コードユニットの数で計算する
  // UTF-8からUTF-16コードユニット数を正確に計算
  const char* str = m_primitiveValue.c_str();
  size_t len = 0;
  size_t i = 0;

  while (i < m_primitiveValue.length()) {
    unsigned char c = static_cast<unsigned char>(str[i]);

    if (c < 0x80) {
      // ASCII文字 (1バイト)
      len++;
      i++;
    } else if ((c & 0xE0) == 0xC0) {
      // 2バイトUTF-8シーケンス (U+0080 - U+07FF)
      len++;
      i += 2;
    } else if ((c & 0xF0) == 0xE0) {
      // 3バイトUTF-8シーケンス (U+0800 - U+FFFF)
      len++;
      i += 3;
    } else if ((c & 0xF8) == 0xF0) {
      // 4バイトUTF-8シーケンス (U+10000 - U+10FFFF)
      // サロゲートペアとして2つのUTF-16コードユニットになる
      len += 2;
      i += 4;
    } else {
      // 不正なUTF-8シーケンス、安全のため1バイト進める
      i++;
    }
  }

  return len;
}

// Stringクラスの静的メソッド
ObjectPtr String::initialize(ObjectPtr globalObject) {
  auto prototype = createPrototype();
  auto constructor = createConstructor(prototype);

  // グローバルオブジェクトに String プロパティとして追加
  globalObject->defineProperty("String", Value::fromObject(constructor),
                               PropertyAttributes::DontEnum);

  return constructor;
}

ObjectPtr String::createPrototype() {
  auto prototype = Object::create();

  // プロトタイプのプロパティ設定
  prototype->defineProperty("constructor", Value::undefined(),
                            PropertyAttributes::Writable | PropertyAttributes::Configurable);

  // プロトタイプメソッドのインストール
  installPrototypeMethods(prototype);

  return prototype;
}

ObjectPtr String::createConstructor(ObjectPtr prototype) {
  // String コンストラクタ関数の作成
  auto constructor = Function::createConstructor("String", String::construct, 1, prototype);

  // コンストラクタのプロパティ設定
  constructor->defineProperty("prototype", Value::fromObject(prototype),
                              PropertyAttributes::DontEnum | PropertyAttributes::DontDelete | PropertyAttributes::ReadOnly);

  // プロトタイプの constructor プロパティを設定
  prototype->defineProperty("constructor", Value::fromObject(constructor),
                            PropertyAttributes::Writable | PropertyAttributes::Configurable);

  // 静的メソッドのインストール
  installStaticMethods(constructor);

  return constructor;
}

ValuePtr String::construct(const std::vector<ValuePtr>& arguments, ValuePtr newTarget) {
  std::string value;

  // 引数が渡された場合、最初の引数を文字列に変換
  if (!arguments.empty() && arguments[0]) {
    value = arguments[0]->toString();
  }

  // new String() として呼ばれた場合
  if (newTarget && !newTarget->isUndefined()) {
    // プロトタイプを取得
    auto constructor = Value::fromObject(Object::getGlobalObject()->get("String")->toObject());
    auto prototype = constructor->get("prototype")->toObject();

    // StringObject を作成して返す
    return Value::fromObject(StringObject::create(value, prototype));
  }

  // String() として呼ばれた場合、プリミティブ文字列を返す
  return Value::fromString(value);
}

ValuePtr String::fromValue(ValuePtr value) {
  if (!value) {
    return Value::fromString("");
  }

  return Value::fromString(value->toString());
}

void String::installPrototypeMethods(ObjectPtr prototype) {
  // Function::create を使ってメソッドを作成
  auto defineMethod = [&](const std::string& name, StringMethod method, int length) {
    auto function = Function::create(name, method, length);
    prototype->defineProperty(name, Value::fromObject(function),
                              PropertyAttributes::Writable | PropertyAttributes::Configurable);
  };

  // ECMAScript 仕様のすべてのStringプロトタイプメソッド
  defineMethod("charAt", String::charAt, 1);
  defineMethod("charCodeAt", String::charCodeAt, 1);
  defineMethod("codePointAt", String::codePointAt, 1);
  defineMethod("concat", String::concat, 1);
  defineMethod("endsWith", String::endsWith, 1);
  defineMethod("includes", String::includes, 1);
  defineMethod("indexOf", String::indexOf, 1);
  defineMethod("lastIndexOf", String::lastIndexOf, 1);
  defineMethod("localeCompare", String::localeCompare, 1);
  defineMethod("match", String::match, 1);
  defineMethod("matchAll", String::matchAll, 1);
  defineMethod("normalize", String::normalize, 0);
  defineMethod("padEnd", String::padEnd, 1);
  defineMethod("padStart", String::padStart, 1);
  defineMethod("repeat", String::repeat, 1);
  defineMethod("replace", String::replace, 2);
  defineMethod("replaceAll", String::replaceAll, 2);
  defineMethod("search", String::search, 1);
  defineMethod("slice", String::slice, 2);
  defineMethod("split", String::split, 2);
  defineMethod("startsWith", String::startsWith, 1);
  defineMethod("substring", String::substring, 2);
  defineMethod("toLocaleLowerCase", String::toLocaleLowerCase, 0);
  defineMethod("toLocaleUpperCase", String::toLocaleUpperCase, 0);
  defineMethod("toLowerCase", String::toLowerCase, 0);
  defineMethod("toString", String::toString, 0);
  defineMethod("toUpperCase", String::toUpperCase, 0);
  defineMethod("trim", String::trim, 0);
  defineMethod("trimEnd", String::trimEnd, 0);
  defineMethod("trimStart", String::trimStart, 0);
  defineMethod("valueOf", String::valueOf, 0);

  // ES2015以前の非推奨メソッド (後方互換性のため)
  defineMethod("substr", String::substring, 2);
  defineMethod("trimLeft", String::trimStart, 0);
  defineMethod("trimRight", String::trimEnd, 0);
}

void String::installStaticMethods(ObjectPtr constructor) {
  // 静的メソッドを定義
  auto defineStaticMethod = [&](const std::string& name, StringMethod method, int length) {
    auto function = Function::create(name, method, length);
    constructor->defineProperty(name, Value::fromObject(function),
                                PropertyAttributes::Writable | PropertyAttributes::Configurable);
  };

  defineStaticMethod("fromCharCode", String::fromCharCode, 1);
  defineStaticMethod("fromCodePoint", String::fromCodePoint, 1);
  defineStaticMethod("raw", String::raw, 1);
}

// this 値から文字列を取得するユーティリティ関数
static std::string getStringFromThis(const std::vector<ValuePtr>& arguments) {
  // this 値が undefined または null の場合はエラー
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("String.prototype method called on null or undefined");
  }

  // this 値を文字列に変換
  if (arguments[0]->isObject() && arguments[0]->toObject()->isStringObject()) {
    // StringObject の場合はプリミティブ値を取得
    auto stringObj = std::static_pointer_cast<StringObject>(arguments[0]->toObject());
    return stringObj->primitiveValue();
  }

  // その他の値は toString() で変換
  return arguments[0]->toString();
}

// 引数を数値インデックスに変換するユーティリティ関数
static int toIndex(ValuePtr value, int defaultValue, int max) {
  if (!value || value->isUndefined()) {
    return defaultValue;
  }

  double index = value->toNumber();

  if (std::isnan(index) || index == 0) {
    return 0;
  }

  // 符号から整数部分を計算
  int intIndex = static_cast<int>(index);
  if (index < 0) {
    intIndex = std::max(0, max + intIndex);
  } else if (intIndex > max) {
    intIndex = max;
  }

  return intIndex;
}

// String.prototype.valueOf と String.prototype.toString の実装
ValuePtr String::valueOf(const std::vector<ValuePtr>& arguments) {
  // this 値が StringObject の場合はプリミティブ値を返す
  if (!arguments.empty() && arguments[0]->isObject() && arguments[0]->toObject()->isStringObject()) {
    auto stringObj = std::static_pointer_cast<StringObject>(arguments[0]->toObject());
    return Value::fromString(stringObj->primitiveValue());
  }

  // その他の場合は this 値を文字列に変換
  try {
    std::string thisValue = getStringFromThis(arguments);
    return Value::fromString(thisValue);
  } catch (const std::exception&) {
    // this 値が null または undefined の場合
    throw std::runtime_error("String.prototype.valueOf called on null or undefined");
  }
}

ValuePtr String::toString(const std::vector<ValuePtr>& arguments) {
  // valueOf と同じ実装
  return valueOf(arguments);
}

}  // namespace core
}  // namespace aerojs