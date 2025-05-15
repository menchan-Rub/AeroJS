/**
 * @file value_inline.h
 * @brief AeroJS JavaScript エンジンの値クラスの最適化された実装
 * @version 1.0.0
 * @license MIT
 */

#ifndef AEROJS_CORE_VALUE_INLINE_H
#define AEROJS_CORE_VALUE_INLINE_H

namespace aerojs {
namespace core {

// インライン関数の実装 - 最も頻繁に使用される関数のパフォーマンスを最大化

// 型チェック系関数 - 超高速実装
inline bool Value::isUndefined() const noexcept {
  return (m_value.m_bits & TAG_MASK) == TAG_UNDEFINED;
}

inline bool Value::isNull() const noexcept {
  return (m_value.m_bits & TAG_MASK) == TAG_NULL;
}

inline bool Value::isNullish() const noexcept {
  uint64_t tag = m_value.m_bits & TAG_MASK;
  return tag == TAG_UNDEFINED || tag == TAG_NULL;
}

inline bool Value::isBoolean() const noexcept {
  return (m_value.m_bits & TAG_MASK) == TAG_BOOLEAN;
}

inline bool Value::isNumber() const noexcept {
  // 通常の浮動小数点数
  if ((m_value.m_bits & QUIET_NAN_TAG) != QUIET_NAN_TAG) {
    return true;
  }
  
  // インライン整数も数値
  return (m_value.m_bits & TAG_MASK) == TAG_INT32;
}

inline bool Value::isInt32() const noexcept {
  return (m_value.m_bits & TAG_MASK) == TAG_INT32;
}

inline bool Value::isString() const noexcept {
  uint64_t tag = m_value.m_bits & TAG_MASK;
  return tag == TAG_STRING || tag == TAG_SMALL_STR;
}

inline bool Value::isSmallString() const noexcept {
  return (m_value.m_bits & TAG_MASK) == TAG_SMALL_STR;
}

inline bool Value::isSymbol() const noexcept {
  return (m_value.m_bits & TAG_MASK) == TAG_SYMBOL;
}

inline bool Value::isObject() const noexcept {
  return ((m_value.m_bits & TAG_MASK) & TAG_OBJECT) == TAG_OBJECT;
}

inline bool Value::isFunction() const noexcept {
  return ((m_value.m_bits & TAG_MASK) & (TAG_OBJECT | OBJECT_TYPE_FUNCTION)) 
      == (TAG_OBJECT | OBJECT_TYPE_FUNCTION);
}

inline bool Value::isArray() const noexcept {
  return ((m_value.m_bits & TAG_MASK) & (TAG_OBJECT | OBJECT_TYPE_ARRAY)) 
      == (TAG_OBJECT | OBJECT_TYPE_ARRAY);
}

inline bool Value::isDate() const noexcept {
  return ((m_value.m_bits & TAG_MASK) & (TAG_OBJECT | OBJECT_TYPE_DATE)) 
      == (TAG_OBJECT | OBJECT_TYPE_DATE);
}

inline bool Value::isRegExp() const noexcept {
  return ((m_value.m_bits & TAG_MASK) & (TAG_OBJECT | OBJECT_TYPE_REGEXP)) 
      == (TAG_OBJECT | OBJECT_TYPE_REGEXP);
}

inline bool Value::isError() const noexcept {
  return ((m_value.m_bits & TAG_MASK) & (TAG_OBJECT | OBJECT_TYPE_ERROR)) 
      == (TAG_OBJECT | OBJECT_TYPE_ERROR);
}

inline bool Value::isBigInt() const noexcept {
  return (m_value.m_bits & TAG_MASK) == TAG_BIGINT;
}

inline bool Value::isTypedArray() const noexcept {
  return ((m_value.m_bits & TAG_MASK) & (TAG_OBJECT | OBJECT_TYPE_TYPED_ARRAY)) 
      == (TAG_OBJECT | OBJECT_TYPE_TYPED_ARRAY);
}

inline bool Value::isPrimitive() const noexcept {
  // オブジェクト型でないことを確認（高速パス）
  if (((m_value.m_bits & TAG_MASK) & TAG_OBJECT) == TAG_OBJECT) {
    return false;
  }
  
  return true; // オブジェクト以外はすべてプリミティブ
}

// 変換系メソッド - 高速実装
inline bool Value::toBoolean() const noexcept {
  // 最も一般的なケースの高速パス
  
  // 論理値の直接変換
  if (isBoolean()) {
    return reinterpret_cast<uintptr_t>(extractPayload()) != 0;
  }
  
  // 数値型の高速変換
  if ((m_value.m_bits & QUIET_NAN_TAG) != QUIET_NAN_TAG) {
    // 通常の数値
    double val = m_value.m_number;
    return val != 0.0 && !std::isnan(val);
  }
  
  if (isInt32()) {
    // インライン整数
    return m_value.m_int32.m_int32 != 0;
  }
  
  // インライン文字列
  if (isSmallString()) {
    return m_value.m_smallString.m_length > 0;
  }
  
  // undefined/nullは常にfalse
  if (isUndefined() || isNull()) {
    return false;
  }
  
  // その他のオブジェクト型は常にtrue
  return true;
}

inline int32_t Value::toInt32() const noexcept {
  // インライン整数の場合、直接返却
  if (isInt32()) {
    return m_value.m_int32.m_int32;
  }
  
  // 通常の数値型
  if ((m_value.m_bits & QUIET_NAN_TAG) != QUIET_NAN_TAG) {
    double val = m_value.m_number;
    
    // 0, NaN, Infinity は 0
    if (val == 0 || !std::isfinite(val)) {
      return 0;
    }
    
    // 32ビットに丸める
    double d32 = std::fmod(std::trunc(val), 4294967296.0);
    if (d32 < 0) {
      d32 += 4294967296.0;
    }
    
    // 符号付き32ビット整数に変換
    uint32_t u32 = static_cast<uint32_t>(d32);
    return static_cast<int32_t>(u32);
  }
  
  // その他の型はより複雑な変換が必要、非インライン関数へ
  return toInt32();
}

inline uint32_t Value::toUint32() const noexcept {
  // インライン整数の場合
  if (isInt32()) {
    return static_cast<uint32_t>(m_value.m_int32.m_int32);
  }
  
  // 通常の数値型
  if ((m_value.m_bits & QUIET_NAN_TAG) != QUIET_NAN_TAG) {
    double val = m_value.m_number;
    
    // 0, NaN, Infinity は 0
    if (val == 0 || !std::isfinite(val)) {
      return 0;
    }
    
    // 32ビットに丸める
    double d32 = std::fmod(std::trunc(val), 4294967296.0);
    if (d32 < 0) {
      d32 += 4294967296.0;
    }
    
    return static_cast<uint32_t>(d32);
  }
  
  // その他の型はより複雑な変換が必要、非インライン関数へ
  return toUint32();
}

inline double Value::toNumber() const noexcept {
  // 通常の数値型の直接返却
  if ((m_value.m_bits & QUIET_NAN_TAG) != QUIET_NAN_TAG) {
    return m_value.m_number;
  }
  
  // インライン整数
  if (isInt32()) {
    return static_cast<double>(m_value.m_int32.m_int32);
  }
  
  // undefined -> NaN
  if (isUndefined()) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  
  // null -> 0
  if (isNull()) {
    return 0.0;
  }
  
  // boolean -> 1 or 0
  if (isBoolean()) {
    return reinterpret_cast<uintptr_t>(extractPayload()) ? 1.0 : 0.0;
  }
  
  // 複雑な変換は非インライン関数に委譲
  return toNumber();
}

inline bool Value::strictEquals(const Value& other) const noexcept {
  // 最も高速なケース: ビット一致
  if (m_value.m_bits == other.m_value.m_bits) {
    return true;
  }
  
  // 両方とも数値型の場合
  if (isNumber() && other.isNumber()) {
    // NaNチェック
    double a = toNumber();
    double b = other.toNumber();
    if (std::isnan(a) || std::isnan(b)) {
      return false; // NaN !== NaN
    }
    return a == b;
  }
  
  // 異なる型の場合は常にfalse
  uint64_t tag1 = m_value.m_bits & TAG_MASK;
  uint64_t tag2 = other.m_value.m_bits & TAG_MASK;
  
  if (tag1 != tag2) {
    // インライン文字列と通常文字列の特殊ケース
    if ((tag1 == TAG_SMALL_STR && tag2 == TAG_STRING) ||
        (tag1 == TAG_STRING && tag2 == TAG_SMALL_STR)) {
      return false; // 内容比較は非インライン関数で
    }
    return false;
  }
  
  // オブジェクト型の同一性チェック
  if (isObject()) {
    return extractPayload() == other.extractPayload();
  }
  
  // インライン文字列の比較
  if (tag1 == TAG_SMALL_STR) {
    if (m_value.m_smallString.m_length != other.m_value.m_smallString.m_length) {
      return false;
    }
    return std::memcmp(m_value.m_smallString.m_chars, 
                       other.m_value.m_smallString.m_chars, 
                       m_value.m_smallString.m_length) == 0;
  }
  
  // その他の型は非インライン関数に委譲
  return strictEquals(other);
}

// フラグ操作 - 高速アクセス
inline bool Value::hasFlag(ValueFlags flag) const noexcept {
  return (m_flags & flag) != 0;
}

inline void Value::setFlag(ValueFlags flag) noexcept {
  m_flags |= flag;
}

inline void Value::clearFlag(ValueFlags flag) noexcept {
  m_flags &= ~flag;
}

// ペイロード抽出 - 高速アクセス
inline void* Value::extractPayload() const noexcept {
  return reinterpret_cast<void*>(m_value.m_bits & PAYLOAD_MASK);
}

}  // namespace core
}  // namespace aerojs

#endif // AEROJS_CORE_VALUE_INLINE_H 