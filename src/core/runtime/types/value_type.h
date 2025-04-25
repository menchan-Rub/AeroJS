/**
 * @file value_type.h
 * @brief JavaScript値の型システムの定義
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_VALUE_TYPE_H
#define AEROJS_VALUE_TYPE_H

#include <cstdint>
#include <string>

namespace aerojs {
namespace core {

/**
 * @brief JavaScript値の基本型
 */
enum class ValueType : uint8_t {
  Undefined = 0,
  Null,
  Boolean,
  Number,
  String,
  Symbol,
  Object,
  Function,
  BigInt,

  // オブジェクトのサブタイプ（内部使用）
  Array,
  Date,
  RegExp,
  Map,
  Set,
  WeakMap,
  WeakSet,
  ArrayBuffer,
  SharedArrayBuffer,
  DataView,
  TypedArray,
  Promise,
  Proxy,
  Error,

  // 内部専用型
  Internal,
  Empty,   // 空スロット用
  Deleted  // 削除済みエントリー用
};

/**
 * @brief Number値のサブタイプ
 */
enum class NumberType : uint8_t {
  Integer,          // 整数
  Double,           // 倍精度浮動小数点数
  NaN,              // Not a Number
  Infinity,         // 無限大
  NegativeInfinity  // 負の無限大
};

/**
 * @brief TypedArray種別
 */
enum class TypedArrayType : uint8_t {
  Int8Array,
  Uint8Array,
  Uint8ClampedArray,
  Int16Array,
  Uint16Array,
  Int32Array,
  Uint32Array,
  Float32Array,
  Float64Array,
  BigInt64Array,
  BigUint64Array
};

/**
 * @brief エラーの種別
 */
enum class ErrorType : uint8_t {
  Error,           // 汎用エラー
  EvalError,       // eval()関連エラー
  RangeError,      // 値が範囲外のエラー
  ReferenceError,  // 無効な参照エラー
  SyntaxError,     // 構文エラー
  TypeError,       // 型エラー
  URIError,        // URI処理エラー
  AggregateError,  // 複数エラーの集約
  InternalError    // 内部エラー
};

/**
 * @brief Propertyの属性フラグ
 */
enum class PropertyFlags : uint8_t {
  None = 0,
  Writable = 1 << 0,      // 書き込み可能
  Enumerable = 1 << 1,    // 列挙可能
  Configurable = 1 << 2,  // 構成可能

  // 組み合わせ
  Default = Writable | Enumerable | Configurable,
  Sealed = Writable,  // 構成と列挙を禁止
  Frozen = None,      // 全て禁止

  // 内部フラグ
  Accessor = 1 << 3,  // アクセサプロパティ
  Internal = 1 << 4,  // 内部プロパティ
  Deleted = 1 << 5,   // 削除済みプロパティ
  Modified = 1 << 6   // 変更済みプロパティ
};

// 演算子オーバーロード（ビット演算）
inline PropertyFlags operator|(PropertyFlags a, PropertyFlags b) {
  return static_cast<PropertyFlags>(
      static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline PropertyFlags operator&(PropertyFlags a, PropertyFlags b) {
  return static_cast<PropertyFlags>(
      static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline PropertyFlags& operator|=(PropertyFlags& a, PropertyFlags b) {
  a = a | b;
  return a;
}

inline PropertyFlags& operator&=(PropertyFlags& a, PropertyFlags b) {
  a = a & b;
  return a;
}

inline bool operator!(PropertyFlags a) {
  return static_cast<uint8_t>(a) == 0;
}

/**
 * @brief オブジェクトの内部フラグ
 */
enum class ObjectFlags : uint16_t {
  None = 0,
  Extensible = 1 << 0,  // 拡張可能
  Sealed = 1 << 1,      // シール済み
  Frozen = 1 << 2,      // フリーズ済み

  // 特殊なオブジェクト種別
  Array = 1 << 3,     // 配列
  Function = 1 << 4,  // 関数
  Error = 1 << 5,     // エラー
  Date = 1 << 6,      // 日付
  RegExp = 1 << 7,    // 正規表現
  Map = 1 << 8,       // マップ
  Set = 1 << 9,       // セット
  Promise = 1 << 10,  // プロミス
  Proxy = 1 << 11,    // プロキシ

  // 実装詳細
  HasIndexedProperties = 1 << 12,  // インデックス付きプロパティを持つ
  HasSpecialProperty = 1 << 13,    // 特殊なプロパティを持つ
  HasGetterSetter = 1 << 14,       // ゲッターまたはセッターを持つ
  Internal = 1 << 15               // 内部オブジェクト
};

// 演算子オーバーロード（ビット演算）
inline ObjectFlags operator|(ObjectFlags a, ObjectFlags b) {
  return static_cast<ObjectFlags>(
      static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline ObjectFlags operator&(ObjectFlags a, ObjectFlags b) {
  return static_cast<ObjectFlags>(
      static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

inline ObjectFlags& operator|=(ObjectFlags& a, ObjectFlags b) {
  a = a | b;
  return a;
}

inline ObjectFlags& operator&=(ObjectFlags& a, ObjectFlags b) {
  a = a & b;
  return a;
}

inline bool operator!(ObjectFlags a) {
  return static_cast<uint16_t>(a) == 0;
}

/**
 * @brief 型変換ユーティリティ関数
 */
namespace type_conversion {

/**
 * @brief ValueTypeの文字列表現を取得
 * @param type 型
 * @return 型の名前
 */
inline std::string valueTypeToString(ValueType type) {
  switch (type) {
    case ValueType::Undefined:
      return "undefined";
    case ValueType::Null:
      return "null";
    case ValueType::Boolean:
      return "boolean";
    case ValueType::Number:
      return "number";
    case ValueType::String:
      return "string";
    case ValueType::Symbol:
      return "symbol";
    case ValueType::Object:
      return "object";
    case ValueType::Function:
      return "function";
    case ValueType::BigInt:
      return "bigint";

    case ValueType::Array:
      return "array";
    case ValueType::Date:
      return "date";
    case ValueType::RegExp:
      return "regexp";
    case ValueType::Map:
      return "map";
    case ValueType::Set:
      return "set";
    case ValueType::WeakMap:
      return "weakmap";
    case ValueType::WeakSet:
      return "weakset";
    case ValueType::ArrayBuffer:
      return "arraybuffer";
    case ValueType::SharedArrayBuffer:
      return "sharedarraybuffer";
    case ValueType::DataView:
      return "dataview";
    case ValueType::TypedArray:
      return "typedarray";
    case ValueType::Promise:
      return "promise";
    case ValueType::Proxy:
      return "proxy";
    case ValueType::Error:
      return "error";

    case ValueType::Internal:
      return "internal";
    case ValueType::Empty:
      return "empty";
    case ValueType::Deleted:
      return "deleted";

    default:
      return "unknown";
  }
}

/**
 * @brief オブジェクトのクラス名を取得
 * @param flags オブジェクトフラグ
 * @return クラス名
 */
inline std::string objectClassToString(ObjectFlags flags) {
  if ((flags & ObjectFlags::Array) != ObjectFlags::None) return "Array";
  if ((flags & ObjectFlags::Function) != ObjectFlags::None) return "Function";
  if ((flags & ObjectFlags::Error) != ObjectFlags::None) return "Error";
  if ((flags & ObjectFlags::Date) != ObjectFlags::None) return "Date";
  if ((flags & ObjectFlags::RegExp) != ObjectFlags::None) return "RegExp";
  if ((flags & ObjectFlags::Map) != ObjectFlags::None) return "Map";
  if ((flags & ObjectFlags::Set) != ObjectFlags::None) return "Set";
  if ((flags & ObjectFlags::Promise) != ObjectFlags::None) return "Promise";
  if ((flags & ObjectFlags::Proxy) != ObjectFlags::None) return "Proxy";

  return "Object";
}

/**
 * @brief ErrorTypeの文字列表現を取得
 * @param type エラータイプ
 * @return エラー型の名前
 */
inline std::string errorTypeToString(ErrorType type) {
  switch (type) {
    case ErrorType::Error:
      return "Error";
    case ErrorType::EvalError:
      return "EvalError";
    case ErrorType::RangeError:
      return "RangeError";
    case ErrorType::ReferenceError:
      return "ReferenceError";
    case ErrorType::SyntaxError:
      return "SyntaxError";
    case ErrorType::TypeError:
      return "TypeError";
    case ErrorType::URIError:
      return "URIError";
    case ErrorType::AggregateError:
      return "AggregateError";
    case ErrorType::InternalError:
      return "InternalError";

    default:
      return "Unknown";
  }
}

/**
 * @brief TypedArrayTypeの文字列表現を取得
 * @param type TypedArray型
 * @return TypedArray型の名前
 */
inline std::string typedArrayTypeToString(TypedArrayType type) {
  switch (type) {
    case TypedArrayType::Int8Array:
      return "Int8Array";
    case TypedArrayType::Uint8Array:
      return "Uint8Array";
    case TypedArrayType::Uint8ClampedArray:
      return "Uint8ClampedArray";
    case TypedArrayType::Int16Array:
      return "Int16Array";
    case TypedArrayType::Uint16Array:
      return "Uint16Array";
    case TypedArrayType::Int32Array:
      return "Int32Array";
    case TypedArrayType::Uint32Array:
      return "Uint32Array";
    case TypedArrayType::Float32Array:
      return "Float32Array";
    case TypedArrayType::Float64Array:
      return "Float64Array";
    case TypedArrayType::BigInt64Array:
      return "BigInt64Array";
    case TypedArrayType::BigUint64Array:
      return "BigUint64Array";

    default:
      return "Unknown";
  }
}

}  // namespace type_conversion

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_VALUE_TYPE_H