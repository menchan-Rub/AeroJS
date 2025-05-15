/**
 * @file value.h
 * @brief JavaScript値の基本クラス定義
 * @version 1.0.0
 * @license MIT
 */

#ifndef AEROJS_VALUE_H
#define AEROJS_VALUE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <limits>

#include "src/core/runtime/types/value_type.h"
#include "src/utils/memory/smart_ptr/ref_counted.h"

namespace aerojs {
namespace core {

// 前方宣言
class Object;
class String;
class Symbol;
class BigInt;
class Context;
class Function;
class Array;

/**
 * @brief 最適化されたNaN-Boxing実装のための定数定義
 * JavaScriptの値をNaN-Boxing手法で64ビットに格納する
 * 最先端の実装として以下の特徴を持つ：
 * - 高速なタグ判定（単一の比較で型判定可能）
 * - ポインタの完全なアドレス空間の利用
 * - 数値とポインタ型の高速な判別
 */
namespace detail {
// IEEE-754倍精度浮動小数点数のビットパターン定数
constexpr uint64_t QUIET_NAN_MASK = 0x7FF8000000000000ULL;  // 静かなNaN
constexpr uint64_t SIGN_BIT_MASK = 0x8000000000000000ULL;   // 符号ビット
constexpr uint64_t EXPONENT_MASK = 0x7FF0000000000000ULL;   // 指数部マスク
constexpr uint64_t MANTISSA_MASK = 0x000FFFFFFFFFFFFFULL;   // 仮数部マスク

// タグビットパターン (52ビットの仮数部の上位8ビットを使用)
constexpr uint64_t TAG_BITS_MASK = 0x000FF00000000000ULL;   // タグビットマスク（8ビット）
constexpr uint64_t TAG_UNDEFINED = 0x0001000000000000ULL;   // undefined
constexpr uint64_t TAG_NULL = 0x0002000000000000ULL;        // null
constexpr uint64_t TAG_BOOLEAN = 0x0003000000000000ULL;     // boolean
constexpr uint64_t TAG_SYMBOL = 0x0004000000000000ULL;      // シンボル
constexpr uint64_t TAG_STRING = 0x0005000000000000ULL;      // 文字列ポインタ
constexpr uint64_t TAG_OBJECT = 0x0006000000000000ULL;      // オブジェクトポインタ
constexpr uint64_t TAG_FUNCTION = 0x0007000000000000ULL;    // 関数ポインタ
constexpr uint64_t TAG_ARRAY = 0x0008000000000000ULL;       // 配列ポインタ
constexpr uint64_t TAG_REGEXP = 0x0009000000000000ULL;      // 正規表現ポインタ
constexpr uint64_t TAG_DATE = 0x000A000000000000ULL;        // 日付ポインタ
constexpr uint64_t TAG_ERROR = 0x000B000000000000ULL;       // エラーポインタ
constexpr uint64_t TAG_BIGINT = 0x000C000000000000ULL;      // BigIntポインタ
constexpr uint64_t TAG_PROMISE = 0x000D000000000000ULL;     // Promiseポインタ
constexpr uint64_t TAG_MAP = 0x000E000000000000ULL;         // Mapポインタ
constexpr uint64_t TAG_SET = 0x000F000000000000ULL;         // Setポインタ
constexpr uint64_t TAG_TYPED_ARRAY = 0x0010000000000000ULL; // TypedArrayポインタ

// 高速判定用ビットパターン
constexpr uint64_t NON_OBJECT_MASK = 0xFFF8000000000000ULL; // オブジェクト以外の型判定
constexpr uint64_t NUMBER_TYPE_MASK = 0xFFF0000000000000ULL; // 数値型判定
constexpr uint64_t POINTER_TYPE_MASK = 0xFFFF000000000000ULL; // ポインタ型判定

// 値用の追加ビット
constexpr uint64_t BOOLEAN_TRUE = 0x0000000000000001ULL;  // true値

// ポインタ用のマスク（48ビットアドレス空間対応）
constexpr uint64_t PAYLOAD_MASK = 0x0000FFFFFFFFFFFFULL;  // ペイロードマスク (48ビット)

// ポインタ変換用のアライメント
constexpr uint64_t POINTER_ALIGNMENT = 8ULL; // 8バイトアライメント
}  // namespace detail

/**
 * @brief JavaScriptの値を表現する高性能クラス
 *
 * 最適化されたNaN-Boxingを使用して64ビットに全ての値を格納する
 * - 数値: IEEE-754倍精度浮動小数点数形式で格納
 * - その他: 静かなNaNの領域にタグとペイロードを格納
 * 
 * メモリ使用量を最小化しつつ、高速な型チェックと値アクセスを提供
 */
class Value {
 public:
  // 空の値（undefined）を作成
  Value()
      : bits_(detail::QUIET_NAN_MASK | detail::TAG_UNDEFINED) {
  }

  // コピーと代入
  Value(const Value& other) = default;
  Value& operator=(const Value& other) = default;

  // 特殊値のコンストラクタ
  static Value createUndefined() {
    return Value(detail::QUIET_NAN_MASK | detail::TAG_UNDEFINED);
  }

  static Value createNull() {
    return Value(detail::QUIET_NAN_MASK | detail::TAG_NULL);
  }

  // ブール値
  static Value createBoolean(bool value) {
    return Value(detail::QUIET_NAN_MASK | detail::TAG_BOOLEAN |
                 (value ? detail::BOOLEAN_TRUE : 0));
  }

  // 数値 (倍精度浮動小数点数)
  static Value createNumber(double value) {
    union {
      double d;
      uint64_t bits;
    } u;
    u.d = value;
    return Value(u.bits);
  }

  // 整数
  static Value createInteger(int32_t value) {
    return createNumber(static_cast<double>(value));
  }

  // オブジェクト系のファクトリメソッド (実装は別ファイルで)
  static Value createObject(Object* object);
  static Value createString(String* str);
  static Value createSymbol(Symbol* symbol);
  static Value createBigInt(BigInt* bigint);
  static Value createFunction(Function* function);
  static Value createArray(Array* array);

  // 高速型判定関数 - JITコンパイルで最適化されるシングル命令比較
  bool isUndefined() const {
    return bits_ == (detail::QUIET_NAN_MASK | detail::TAG_UNDEFINED);
  }

  bool isNull() const {
    return bits_ == (detail::QUIET_NAN_MASK | detail::TAG_NULL);
  }

  bool isBoolean() const {
    return (bits_ & (detail::QUIET_NAN_MASK | detail::TAG_BITS_MASK)) ==
           (detail::QUIET_NAN_MASK | detail::TAG_BOOLEAN);
  }

  // 数値型判定 - 高速ビットマスク
  bool isNumber() const {
    return (bits_ & detail::NUMBER_TYPE_MASK) != detail::QUIET_NAN_MASK;
  }

  // IEEE-754で表現可能な安全な整数かどうか
  bool isInteger() const {
    if (!isNumber()) return false;
    double value = toNumber();
    return std::trunc(value) == value &&
           value >= -9007199254740991.0 &&  // -(2^53 - 1)
           value <= 9007199254740991.0;     // 2^53 - 1
  }

  // 符号付き32ビット整数の範囲内かどうか
  bool isInt32() const {
    if (!isNumber()) return false;
    double value = toNumber();
    return std::trunc(value) == value &&
           value >= -2147483648.0 &&  // -2^31
           value <= 2147483647.0;     // 2^31 - 1
  }

  // オブジェクト型判定 - 高速ビットマスク
  bool isObject() const {
    return (bits_ & (detail::QUIET_NAN_MASK | detail::TAG_BITS_MASK)) ==
           (detail::QUIET_NAN_MASK | detail::TAG_OBJECT);
  }

  bool isString() const {
    return (bits_ & (detail::QUIET_NAN_MASK | detail::TAG_BITS_MASK)) ==
           (detail::QUIET_NAN_MASK | detail::TAG_STRING);
  }

  bool isSymbol() const {
    return (bits_ & (detail::QUIET_NAN_MASK | detail::TAG_BITS_MASK)) ==
           (detail::QUIET_NAN_MASK | detail::TAG_SYMBOL);
  }

  bool isBigInt() const {
    return (bits_ & (detail::QUIET_NAN_MASK | detail::TAG_BITS_MASK)) ==
           (detail::QUIET_NAN_MASK | detail::TAG_BIGINT);
  }

  // 特殊オブジェクト高速判定関数
  bool isFunction() const {
    return (bits_ & (detail::QUIET_NAN_MASK | detail::TAG_BITS_MASK)) ==
           (detail::QUIET_NAN_MASK | detail::TAG_FUNCTION);
  }
  
  bool isArray() const {
    return (bits_ & (detail::QUIET_NAN_MASK | detail::TAG_BITS_MASK)) ==
           (detail::QUIET_NAN_MASK | detail::TAG_ARRAY);
  }
  
  bool isDate() const {
    return (bits_ & (detail::QUIET_NAN_MASK | detail::TAG_BITS_MASK)) ==
           (detail::QUIET_NAN_MASK | detail::TAG_DATE);
  }
  
  bool isRegExp() const {
    return (bits_ & (detail::QUIET_NAN_MASK | detail::TAG_BITS_MASK)) ==
           (detail::QUIET_NAN_MASK | detail::TAG_REGEXP);
  }
  
  bool isError() const {
    return (bits_ & (detail::QUIET_NAN_MASK | detail::TAG_BITS_MASK)) ==
           (detail::QUIET_NAN_MASK | detail::TAG_ERROR);
  }
  
  bool isPromise() const {
    return (bits_ & (detail::QUIET_NAN_MASK | detail::TAG_BITS_MASK)) ==
           (detail::QUIET_NAN_MASK | detail::TAG_PROMISE);
  }
  
  bool isMap() const {
    return (bits_ & (detail::QUIET_NAN_MASK | detail::TAG_BITS_MASK)) ==
           (detail::QUIET_NAN_MASK | detail::TAG_MAP);
  }
  
  bool isSet() const {
    return (bits_ & (detail::QUIET_NAN_MASK | detail::TAG_BITS_MASK)) ==
           (detail::QUIET_NAN_MASK | detail::TAG_SET);
  }

  // 複合型判定（最適化）
  bool isNullOrUndefined() const {
    return (bits_ & ~detail::TAG_NULL) == (detail::QUIET_NAN_MASK | detail::TAG_UNDEFINED) ||
           bits_ == (detail::QUIET_NAN_MASK | detail::TAG_NULL);
  }

  bool isPrimitive() const {
    return isNumber() || isString() || isBoolean() || 
           isSymbol() || isBigInt() || isNullOrUndefined();
  }

  // 値抽出関数
  bool toBoolean() const {
    if (isBoolean()) {
      return (bits_ & detail::BOOLEAN_TRUE) != 0;
    }

    // boolean以外の型の変換 (JavaScriptのルールに従う)
    if (isNumber()) {
      double d = toNumber();
      return d != 0.0 && !std::isnan(d);
    }
    if (isString()) {
      return !asString()->isEmpty();
    }
    if (isUndefined() || isNull()) {
      return false;
    }
    // オブジェクトは常にtrue
    return true;
  }

  double toNumber() const {
    if (isNumber()) {
      union {
        uint64_t bits;
        double d;
      } u;
      u.bits = bits_;
      return u.d;
    }

    // 数値以外の型の変換 (JavaScriptの仕様に準拠)
    if (isUndefined()) return std::numeric_limits<double>::quiet_NaN();
    if (isNull()) return 0.0;
    if (isBoolean()) return toBoolean() ? 1.0 : 0.0;

    // オブジェクトや文字列などの複雑な変換は別ファイルに実装
    return std::numeric_limits<double>::quiet_NaN();
  }

  int32_t toInt32() const {
    // IEEE-754倍精度浮動小数点数からint32への最適化変換
    double num = toNumber();
    
    // 最適化: 一般的なケースを高速処理
    if (isInt32()) {
      return static_cast<int32_t>(num);
    }
    
    // エッジケース処理
    if (std::isnan(num) || std::isinf(num) || num == 0.0) {
      return 0;
    }

    constexpr double TWO_32 = 4294967296.0;  // 2^32

    // 小数部分を切り捨て
    num = std::trunc(num);

    // モジュロ2^32演算
    num = std::fmod(num, TWO_32);

    // 負の値を正の値に調整
    if (num < 0) {
      num += TWO_32;
    }

    // 符号付き32ビット整数に変換
    if (num >= TWO_32 / 2) {
      return static_cast<int32_t>(num - TWO_32);
    }
    return static_cast<int32_t>(num);
  }

  // 高速アクセス関数
  int32_t asInt32() const {
    // 数値型の場合のみ直接変換（型チェックなし）
    union {
      uint64_t bits;
      double d;
    } u;
    u.bits = bits_;
    return static_cast<int32_t>(u.d);
  }

  double asNumber() const {
    // 数値型の場合のみ直接変換（型チェックなし）
    union {
      uint64_t bits;
      double d;
    } u;
    u.bits = bits_;
    return u.d;
  }
  
  // ポインタ抽出関数（JIT最適化用）
  Object* asObject() const {
    return reinterpret_cast<Object*>(bits_ & detail::PAYLOAD_MASK);
  }
  
  String* asString() const {
    return reinterpret_cast<String*>(bits_ & detail::PAYLOAD_MASK);
  }
  
  Symbol* asSymbol() const {
    return reinterpret_cast<Symbol*>(bits_ & detail::PAYLOAD_MASK);
  }
  
  BigInt* asBigInt() const {
    return reinterpret_cast<BigInt*>(bits_ & detail::PAYLOAD_MASK);
  }
  
  Function* asFunction() const {
    return reinterpret_cast<Function*>(bits_ & detail::PAYLOAD_MASK);
  }
  
  Array* asArray() const {
    return reinterpret_cast<Array*>(bits_ & detail::PAYLOAD_MASK);
  }

  // 文字列変換 (詳細実装は別ファイルで)
  std::string toString() const;

  // 値の型を取得
  ValueType getType() const {
    if (isUndefined()) return ValueType::Undefined;
    if (isNull()) return ValueType::Null;
    if (isBoolean()) return ValueType::Boolean;
    if (isNumber()) return ValueType::Number;
    if (isString()) return ValueType::String;
    if (isSymbol()) return ValueType::Symbol;
    if (isBigInt()) return ValueType::BigInt;
    if (isFunction()) return ValueType::Function;
    if (isArray()) return ValueType::Array;
    if (isRegExp()) return ValueType::RegExp;
    if (isDate()) return ValueType::Date;
    if (isError()) return ValueType::Error;
    if (isPromise()) return ValueType::Promise;
    if (isMap()) return ValueType::Map;
    if (isSet()) return ValueType::Set;
    return ValueType::Object;
  }

  // 生のビット値を取得（デバッグ用）
  uint64_t getRawBits() const {
    return bits_;
  }

  // 等価性比較
  bool equals(const Value& other) const;
  bool strictEquals(const Value& other) const;

private:
  // 生のビット値からの構築（内部使用のみ）
  explicit Value(uint64_t bits)
      : bits_(bits) {
  }

  // NaN-Boxingされた値のビット表現
  uint64_t bits_;
};

// よく使う定数値（グローバルインスタンス）
inline Value undefined() {
  return Value::createUndefined();
}

inline Value null() {
  return Value::createNull();
}

inline Value trueValue() {
  return Value::createBoolean(true);
}

inline Value falseValue() {
  return Value::createBoolean(false);
}

inline Value zero() {
  return Value::createNumber(0.0);
}

inline Value one() {
  return Value::createNumber(1.0);
}

inline Value nan() {
  return Value::createNumber(std::numeric_limits<double>::quiet_NaN());
}

inline Value infinity() {
  return Value::createNumber(std::numeric_limits<double>::infinity());
}

inline Value negativeInfinity() {
  return Value::createNumber(-std::numeric_limits<double>::infinity());
}

} // namespace core
} // namespace aerojs

#endif // AEROJS_VALUE_H