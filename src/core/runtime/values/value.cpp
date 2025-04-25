/**
 * @file value.cpp
 * @brief Value クラスの実装
 * @version 0.1.0
 * @license MIT
 */

#include "src/core/runtime/values/value.h"

#include <cmath>
#include <limits>

#include "src/core/runtime/values/bigint.h"
#include "src/core/runtime/values/function.h"
#include "src/core/runtime/values/object.h"
#include "src/core/runtime/values/string.h"
#include "src/core/runtime/values/symbol.h"

namespace aerojs {
namespace core {

// ポインタのデコード関数
template <typename T>
static T* decodePointer(uint64_t bits) {
  // パディングを除去してポインタを復元
  return reinterpret_cast<T*>(bits & detail::PAYLOAD_MASK);
}

// ポインタのエンコード関数
template <typename T>
static uint64_t encodePointer(T* ptr, uint64_t tag) {
  // 48ビットのペイロードにパディングとタグを追加
  uint64_t ptrBits = reinterpret_cast<uint64_t>(ptr) & detail::PAYLOAD_MASK;
  return detail::QUIET_NAN_MASK | tag | ptrBits;
}

// オブジェクト生成
Value Value::createObject(Object* object) {
  if (!object) return createNull();
  return Value(encodePointer(object, detail::TAG_OBJECT));
}

// 文字列生成
Value Value::createString(String* str) {
  if (!str) return createNull();
  return Value(encodePointer(str, detail::TAG_STRING));
}

// シンボル生成
Value Value::createSymbol(Symbol* symbol) {
  if (!symbol) return createNull();
  return Value(encodePointer(symbol, detail::TAG_SYMBOL));
}

// BigInt生成
Value Value::createBigInt(BigInt* bigint) {
  if (!bigint) return createNull();
  return Value(encodePointer(bigint, detail::TAG_BIGINT));
}

// ポインタ抽出関数
Object* Value::asObject() const {
  if (!isObject()) return nullptr;
  return decodePointer<Object>(bits_);
}

String* Value::asString() const {
  if (!isString()) return nullptr;
  return decodePointer<String>(bits_);
}

Symbol* Value::asSymbol() const {
  if (!isSymbol()) return nullptr;
  return decodePointer<Symbol>(bits_);
}

BigInt* Value::asBigInt() const {
  if (!isBigInt()) return nullptr;
  return decodePointer<BigInt>(bits_);
}

Function* Value::asFunction() const {
  if (!isFunction()) return nullptr;
  return static_cast<Function*>(asObject());
}

// オブジェクトタイプ判定関数
bool Value::isFunction() const {
  if (!isObject()) return false;
  Object* obj = asObject();
  return obj && obj->isFunction();
}

bool Value::isArray() const {
  if (!isObject()) return false;
  Object* obj = asObject();
  return obj && obj->isArray();
}

bool Value::isDate() const {
  if (!isObject()) return false;
  Object* obj = asObject();
  return obj && obj->isDate();
}

bool Value::isRegExp() const {
  if (!isObject()) return false;
  Object* obj = asObject();
  return obj && obj->isRegExp();
}

bool Value::isError() const {
  if (!isObject()) return false;
  Object* obj = asObject();
  return obj && obj->isError();
}

bool Value::isPromise() const {
  if (!isObject()) return false;
  Object* obj = asObject();
  return obj && obj->isPromise();
}

bool Value::isProxy() const {
  if (!isObject()) return false;
  Object* obj = asObject();
  return obj && obj->isProxy();
}

bool Value::isMap() const {
  if (!isObject()) return false;
  Object* obj = asObject();
  return obj && obj->isMap();
}

bool Value::isSet() const {
  if (!isObject()) return false;
  Object* obj = asObject();
  return obj && obj->isSet();
}

bool Value::isWeakMap() const {
  if (!isObject()) return false;
  Object* obj = asObject();
  return obj && obj->isWeakMap();
}

bool Value::isWeakSet() const {
  if (!isObject()) return false;
  Object* obj = asObject();
  return obj && obj->isWeakSet();
}

// 文字列変換
std::string Value::toString() const {
  // 文字列オブジェクトの場合はそのまま返す
  if (isString()) {
    String* str = asString();
    if (str) return str->value();
    return "";
  }

  // その他の型は適切に変換
  if (isUndefined()) return "undefined";
  if (isNull()) return "null";
  if (isBoolean()) return toBoolean() ? "true" : "false";

  if (isNumber()) {
    double num = toNumber();

    // NaN
    if (std::isnan(num)) return "NaN";

    // Infinity
    if (std::isinf(num)) {
      return num > 0 ? "Infinity" : "-Infinity";
    }

    // ゼロ (正負を区別)
    if (num == 0.0) {
      return std::signbit(num) ? "-0" : "0";
    }

    // 整数かどうか確認して書式を変更
    if (std::floor(num) == num &&
        num <= 9007199254740991.0 &&
        num >= -9007199254740991.0) {
      // 整数の場合小数点以下を表示しない
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%.0f", num);
      return buffer;
    } else {
      // 浮動小数点数
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%.16g", num);
      return buffer;
    }
  }

  // シンボルはTypeError (JavaScriptの仕様)
  if (isSymbol()) {
    return "Symbol()";  // 実際には変換時にエラーが発生する
  }

  // オブジェクトの場合は[object Object]を返す (詳細な実装は別途)
  if (isObject()) {
    if (isArray()) return "[object Array]";
    if (isFunction()) return "[object Function]";
    if (isDate()) {
      Object* obj = asObject();
      // 日付オブジェクトの場合は日付を文字列に変換（詳細な実装は省略）
      return "[object Date]";
    }
    if (isRegExp()) {
      Object* obj = asObject();
      // 正規表現の文字列表現（詳細な実装は省略）
      return "[object RegExp]";
    }
    return "[object Object]";
  }

  if (isBigInt()) {
    BigInt* bigint = asBigInt();
    if (bigint) return bigint->toString() + "n";
    return "0n";
  }

  // 未知の型
  return "[unknown]";
}

// 値の比較 (JavaScriptの緩い等価性, ==演算子)
bool Value::equals(const Value& other) const {
  // 同じ型同士の比較は厳密比較と同じ
  if (getType() == other.getType()) {
    return strictEquals(other);
  }

  // null と undefined は互いに等しい
  if ((isNull() && other.isUndefined()) ||
      (isUndefined() && other.isNull())) {
    return true;
  }

  // 数値と文字列の比較
  if (isNumber() && other.isString()) {
    // 文字列を数値に変換して比較
    try {
      double otherNum = std::stod(other.toString());
      return toNumber() == otherNum;
    } catch (const std::invalid_argument&) {
      return false;  // 変換できない場合はfalse
    } catch (const std::out_of_range&) {
      return false;  // 範囲外の場合はfalse
    }
  }

  if (isString() && other.isNumber()) {
    // 文字列を数値に変換して比較
    try {
      double thisNum = std::stod(toString());
      return thisNum == other.toNumber();
    } catch (const std::invalid_argument&) {
      return false;  // 変換できない場合はfalse
    } catch (const std::out_of_range&) {
      return false;  // 範囲外の場合はfalse
    }
  }

  // BigIntとの比較
  if (isBigInt() && other.isString()) {
    try {
      // 文字列をBigIntに変換して比較
      BigInt* otherBigInt = BigInt::fromString(other.toString());
      bool result = asBigInt()->equals(*otherBigInt);
      delete otherBigInt;
      return result;
    } catch (...) {
      return false;
    }
  }

  if (isString() && other.isBigInt()) {
    try {
      // 文字列をBigIntに変換して比較
      BigInt* thisBigInt = BigInt::fromString(toString());
      bool result = thisBigInt->equals(*other.asBigInt());
      delete thisBigInt;
      return result;
    } catch (...) {
      return false;
    }
  }

  if (isBigInt() && other.isNumber()) {
    // BigIntと数値の比較 (整数の場合のみ等しい可能性がある)
    double otherNum = other.toNumber();
    if (std::isnan(otherNum) || std::isinf(otherNum)) {
      return false;
    }
    if (std::floor(otherNum) != otherNum) {
      return false;  // 小数部がある場合は常に不一致
    }
    return asBigInt()->equalsToDouble(otherNum);
  }

  if (isNumber() && other.isBigInt()) {
    // 数値とBigIntの比較
    double thisNum = toNumber();
    if (std::isnan(thisNum) || std::isinf(thisNum)) {
      return false;
    }
    if (std::floor(thisNum) != thisNum) {
      return false;  // 小数部がある場合は常に不一致
    }
    return other.asBigInt()->equalsToDouble(thisNum);
  }

  // ブール値は数値に変換して比較
  if (isBoolean()) {
    // booleanをnumberに変換して比較
    Value numValue(toNumber());
    return numValue.equals(other);
  }

  if (other.isBoolean()) {
    // otherのbooleanをnumberに変換して比較
    Value otherNumValue(other.toNumber());
    return equals(otherNumValue);
  }

  // オブジェクトとプリミティブの比較
  if (isObject() && !other.isObject()) {
    // オブジェクトをプリミティブに変換 (ToPrimitive)
    Value primitive = toPrimitive(PreferredType::NUMBER);
    return primitive.equals(other);
  }

  if (!isObject() && other.isObject()) {
    // オブジェクトをプリミティブに変換 (ToPrimitive)
    Value otherPrimitive = other.toPrimitive(PreferredType::NUMBER);
    return equals(otherPrimitive);
  }

  // シンボルとの比較は常にfalse (異なる型の場合)
  if (isSymbol() || other.isSymbol()) {
    return false;
  }

  // その他の型の組み合わせは等しくない
  return false;
}

// 値の厳密比較 (JavaScriptの厳密等価性, ===演算子)
bool Value::strictEquals(const Value& other) const {
  // 型が異なる場合は常に不一致
  if (getType() != other.getType()) {
    return false;
  }

  // 型ごとの比較処理
  if (isUndefined() || isNull()) {
    // undefined と null は同じ型の場合は常に一致
    return true;
  }

  if (isBoolean()) {
    // ブール値の比較
    return toBoolean() == other.toBoolean();
  }

  if (isNumber()) {
    double a = toNumber();
    double b = other.toNumber();

    // NaNは自分自身と等しくない (IEEE-754の規定)
    if (std::isnan(a) && std::isnan(b)) {
      return false;
    }

    // +0 と -0 は厳密には異なるが、JavaScriptでは等しいと判定
    if (a == 0 && b == 0) {
      return true;
    }

    // その他の数値は通常の比較
    return a == b;
  }

  if (isString()) {
    // 文字列の比較
    return toString() == other.toString();
  }

  if (isSymbol()) {
    // シンボルはポインタの比較
    return asSymbol() == other.asSymbol();
  }

  if (isBigInt()) {
    // BigIntの比較
    return asBigInt()->equals(*other.asBigInt());
  }

  if (isObject()) {
    // オブジェクトは参照比較（同一オブジェクトかどうか）
    return asObject() == other.asObject();
  }

  // 未知の型
  return false;
}

}  // namespace core
}  // namespace aerojs