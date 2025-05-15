/**
 * @file value.h
 * @brief AeroJS JavaScript エンジンの値クラスの定義 - 世界最高性能の実装
 * @version 1.1.0
 * @license MIT
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <cassert>
#include <string>
#include <type_traits>
#include <immintrin.h> // AVX/SSE サポート用

#include "../utils/containers/string/string_view.h"
#include "../utils/memory/smart_ptr/ref_counted.h"
#include "../utils/platform/cpu_features.h"

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class Object;
class Function;
class Array;
class String;
class Symbol;
class BigInt;
class Error;
class RegExp;
class Date;
class TypedArray;
class Map;
class Set;
class WeakMap;
class WeakSet;
class Promise;

/**
 * @brief JavaScriptの値の型を表す列挙型
 */
enum class ValueType : uint8_t {
  Undefined,
  Null,
  Boolean,
  Number,
  String,
  Symbol,
  Object,
  Function,
  Array,
  Date,
  RegExp,
  Error,
  BigInt,
  TypedArray,
  Map,
  Set,
  WeakMap,
  WeakSet,
  Promise,
  // 最適化のための特殊型
  IntegerNumber,  // 整数型の数値（高速演算用）
  SmallString,    // 短い文字列（インライン保存用）
  Float32Number,  // 32ビット浮動小数点数（SIMD操作用）
  SmallObject     // インラインプロパティのみを持つ小さなオブジェクト
};

// NaN-boxingを使用した高効率な値表現
class AERO_PACKED Value {
private:
    alignas(8) uint64_t _encoded;
    
    // NaN-boxingのためのタグ定義
    static constexpr uint64_t QNAN_MASK      = 0x7FFF800000000000ULL;
    static constexpr uint64_t TAG_MASK       = 0xFFFF000000000000ULL;
    static constexpr uint64_t VALUE_MASK     = 0x00007FFFFFFFFFFFULL;
    
    // タグ値（最適化のためビットパターンを調整）
    static constexpr uint64_t TAG_NAN        = 0x7FF8000000000000ULL;
    static constexpr uint64_t TAG_INT        = 0xFFFA000000000000ULL;
    static constexpr uint64_t TAG_BOOL       = 0xFFFB000000000000ULL;
    static constexpr uint64_t TAG_NULL       = 0xFFFC000000000000ULL;
    static constexpr uint64_t TAG_UNDEFINED  = 0xFFFD000000000000ULL;
    static constexpr uint64_t TAG_OBJECT     = 0xFFFE000000000000ULL;
    static constexpr uint64_t TAG_STRING     = 0xFFFF000000000000ULL;
    static constexpr uint64_t TAG_SYMBOL     = 0xFFF9000000000000ULL;
    static constexpr uint64_t TAG_BIGINT     = 0xFFF8000000000000ULL;
    static constexpr uint64_t TAG_SMALL_STRING = 0xFFF7000000000000ULL;
    static constexpr uint64_t TAG_SMALL_OBJECT = 0xFFF6000000000000ULL;
    
    // ポインタエンコーディング用マスク
    static constexpr uint64_t PTR_MASK       = 0x0000FFFFFFFFFFFFULL;
    
    // 予め計算された特別な値
    static constexpr uint64_t ENCODED_NULL   = TAG_NULL;
    static constexpr uint64_t ENCODED_UNDEFINED = TAG_UNDEFINED;
    static constexpr uint64_t ENCODED_TRUE   = TAG_BOOL | 1ULL;
    static constexpr uint64_t ENCODED_FALSE  = TAG_BOOL | 0ULL;
    static constexpr uint64_t ENCODED_ZERO   = 0x0000000000000000ULL;  // Double +0
    static constexpr uint64_t ENCODED_ONE    = 0x3FF0000000000000ULL;  // Double 1
    static constexpr uint64_t ENCODED_MINUS_ZERO = 0x8000000000000000ULL; // Double -0
    static constexpr uint64_t ENCODED_NEGATIVE_ONE = 0xBFF0000000000000ULL; // Double -1

 public:
    // デフォルトコンストラクタ - undefined
    Value() noexcept : _encoded(ENCODED_UNDEFINED) {}
    
    // デストラクタ
    ~Value() = default;
    
    // コピーコンストラクタとコピー代入演算子
    Value(const Value& other) = default;
    Value& operator=(const Value& other) = default;
    
    // 移動コンストラクタと移動代入演算子
    Value(Value&& other) noexcept = default;
    Value& operator=(Value&& other) noexcept = default;
    
    // 高速な型チェックメソッド - コンパイラの分岐予測を助けるCLIKELY/UNLIKELYマクロ使用
    
    inline bool isNumber() const AERO_LIKELY { 
        // 浮動小数点数は特別なエンコードを持たないので、
        // 他のすべてのタグの否定として検出
        return (_encoded & QNAN_MASK) != QNAN_MASK; 
    }
    
    inline bool isInt() const AERO_LIKELY { 
        return (_encoded & TAG_MASK) == TAG_INT; 
    }
    
    inline bool isBoolean() const AERO_UNLIKELY { 
        return (_encoded & TAG_MASK) == TAG_BOOL; 
    }
    
    inline bool isNull() const AERO_UNLIKELY { 
        return _encoded == ENCODED_NULL; 
    }
    
    inline bool isUndefined() const AERO_UNLIKELY { 
        return _encoded == ENCODED_UNDEFINED; 
    }
    
    inline bool isObject() const AERO_LIKELY { 
        return (_encoded & TAG_MASK) == TAG_OBJECT; 
    }
    
    inline bool isString() const AERO_LIKELY { 
        return (_encoded & TAG_MASK) == TAG_STRING; 
    }
    
    inline bool isSmallString() const AERO_UNLIKELY {
        return (_encoded & TAG_MASK) == TAG_SMALL_STRING;
    }
    
    inline bool isStringAny() const AERO_LIKELY {
        return isString() || isSmallString();
    }
    
    inline bool isSmallObject() const AERO_UNLIKELY {
        return (_encoded & TAG_MASK) == TAG_SMALL_OBJECT;
    }
    
    inline bool isObjectAny() const AERO_LIKELY {
        return isObject() || isSmallObject();
    }
    
    inline bool isSymbol() const AERO_UNLIKELY { 
        return (_encoded & TAG_MASK) == TAG_SYMBOL; 
    }
    
    inline bool isBigInt() const AERO_UNLIKELY { 
        return (_encoded & TAG_MASK) == TAG_BIGINT; 
    }
    
    // 便利な複合チェック
    inline bool isNullOrUndefined() const AERO_UNLIKELY {
        // 最適化: タグ部分の比較で高速化
        return (_encoded >= TAG_NULL) && (_encoded <= TAG_UNDEFINED);
    }
    
    inline bool isPrimitive() const AERO_LIKELY {
        // 高速化: オブジェクト系のタグをチェック
        return (_encoded & 0xFFF6000000000000ULL) != TAG_OBJECT;
    }
    
    // 値抽出メソッド - 高速化のためにコンパイル時条件式を使用
    inline double asDouble() const AERO_LIKELY {
        union { uint64_t bits; double num; } u;
        u.bits = _encoded;
        return u.num;
    }
    
    inline int32_t asInt32() const AERO_LIKELY {
        return static_cast<int32_t>(_encoded & 0xFFFFFFFF);
    }
    
    inline bool asBoolean() const AERO_UNLIKELY {
        return (_encoded & 1ULL) != 0;
    }
    
    inline void* asPtr() const AERO_LIKELY {
        return reinterpret_cast<void*>(_encoded & PTR_MASK);
    }
    
    // SIMD対応の高速値抽出メソッド
    inline __m128d asSSE() const {
        return _mm_set_sd(asDouble());
    }
    
    // 小さい文字列のインライン表現
    inline const char* asSmallStringPtr() const {
        return reinterpret_cast<const char*>(&_encoded);
    }
    
    inline uint8_t getSmallStringLength() const {
        return static_cast<uint8_t>((_encoded >> 48) & 0xFF);
    }
    
    // 値構築静的メソッド
    static inline Value fromDouble(double d) noexcept {
        Value v;
        union { double num; uint64_t bits; } u;
        u.num = d;
        v._encoded = u.bits;
        return v;
    }
    
    static inline Value fromInt32(int32_t i) noexcept {
        // 最適化: 小さい整数は浮動小数点数として格納
        if (AERO_LIKELY(i >= -0x80000 && i <= 0x7FFFF)) {
            return fromDouble(static_cast<double>(i));
        }
        
        Value v;
        // 32ビット整数をエンコード
        v._encoded = TAG_INT | static_cast<uint64_t>(static_cast<uint32_t>(i));
        return v;
    }
    
    static inline Value fromUint32(uint32_t i) noexcept {
        // 最適化: 小さい整数は浮動小数点数として格納
        if (AERO_LIKELY(i <= 0x7FFFF)) {
            return fromDouble(static_cast<double>(i));
        }
        
        Value v;
        v._encoded = TAG_INT | static_cast<uint64_t>(i);
        return v;
    }
    
    static inline Value fromBoolean(bool b) noexcept {
        Value v;
        v._encoded = b ? ENCODED_TRUE : ENCODED_FALSE;
        return v;
    }
    
    static inline Value null() noexcept {
        Value v;
        v._encoded = ENCODED_NULL;
        return v;
    }
    
    static inline Value undefined() noexcept {
        Value v;
        v._encoded = ENCODED_UNDEFINED;
        return v;
    }
    
    static inline Value fromObject(void* ptr) noexcept {
        assert(ptr != nullptr);
        Value v;
        v._encoded = TAG_OBJECT | reinterpret_cast<uint64_t>(ptr);
        return v;
    }
    
    static inline Value fromSmallObject(void* ptr) noexcept {
        assert(ptr != nullptr);
        Value v;
        v._encoded = TAG_SMALL_OBJECT | reinterpret_cast<uint64_t>(ptr);
        return v;
    }
    
    static inline Value fromString(void* ptr) noexcept {
        assert(ptr != nullptr);
        Value v;
        v._encoded = TAG_STRING | reinterpret_cast<uint64_t>(ptr);
        return v;
    }
    
    static inline Value fromSmallString(const char* str, uint8_t length) noexcept {
        assert(str != nullptr);
        assert(length <= 7); // 最大7バイトまで
        
        Value v;
        v._encoded = TAG_SMALL_STRING;
        
        // 長さを格納 (48-55ビット)
        v._encoded |= static_cast<uint64_t>(length) << 48;
        
        // 小さい文字列を直接64ビット値にパック
        uint64_t packedChars = 0;
        for (uint8_t i = 0; i < length; i++) {
            packedChars |= static_cast<uint64_t>(static_cast<uint8_t>(str[i])) << (i * 8);
        }
        
        v._encoded |= packedChars & 0x0000FFFFFFFFFFFFULL;
        return v;
    }
    
    static inline Value fromSymbol(void* ptr) noexcept {
        assert(ptr != nullptr);
        Value v;
        v._encoded = TAG_SYMBOL | reinterpret_cast<uint64_t>(ptr);
        return v;
    }
    
    static inline Value fromBigInt(void* ptr) noexcept {
        assert(ptr != nullptr);
        Value v;
        v._encoded = TAG_BIGINT | reinterpret_cast<uint64_t>(ptr);
        return v;
    }
    
    // 頻出定数値の高速取得
    static inline Value zero() noexcept {
        Value v;
        v._encoded = ENCODED_ZERO;
        return v;
    }
    
    static inline Value one() noexcept {
        Value v;
        v._encoded = ENCODED_ONE;
        return v;
    }
    
    static inline Value minusZero() noexcept {
        Value v;
        v._encoded = ENCODED_MINUS_ZERO;
        return v;
    }
    
    static inline Value minusOne() noexcept {
        Value v;
        v._encoded = ENCODED_NEGATIVE_ONE;
        return v;
    }

    // 高速な等価性チェック - ビット比較最適化
    bool equals(const Value& other) const AERO_LIKELY {
        // 単純なビット比較の最適化
        if (_encoded == other._encoded)
            return true;
            
        // NaN値の特別処理
        if (isNumber() && other.isNumber()) {
            // NaNはNaNと等しくない
            if (std::isnan(asDouble()) || std::isnan(other.asDouble()))
                return false;
                
            // +0と-0は等しい
            if (asDouble() == 0.0 && other.asDouble() == 0.0)
                return true;
        }
        
        // 文字列の特別処理
        if (isStringAny() && other.isStringAny()) {
            // 両方がsmall stringの場合
            if (isSmallString() && other.isSmallString()) {
                return _encoded == other._encoded;
            }
            
            // それ以外は実際の文字列比較が必要
            // 実装は値のアクセサに依存
            return false;
        }
        
        return false;
    }
    
    // 高速なSIMD対応加算
    Value addFast(const Value& other) const AERO_LIKELY {
        // 両方が数値の場合の高速パス
        if (AERO_LIKELY(isNumber() && other.isNumber())) {
            // SIMD 加算を使用
            __m128d a = _mm_set_sd(asDouble());
            __m128d b = _mm_set_sd(other.asDouble());
            __m128d result = _mm_add_sd(a, b);
            
            // 結果を取り出す
            double d;
            _mm_store_sd(&d, result);
            return fromDouble(d);
        }
        
        // その他のケースは遅いパス（実装は別途）
        return Value();
    }
    
    // 内部ビットパターン取得（デバッグ/テスト用）
    uint64_t rawBits() const { return _encoded; }
    
    // 型情報取得
    ValueType type() const {
        if (isNumber()) {
            // 数値の詳細型判定
            double d = asDouble();
            if (std::trunc(d) == d && d >= INT_MIN && d <= INT_MAX) {
                return ValueType::IntegerNumber;
            }
            return ValueType::Number;
        }
        
        if (isString()) return ValueType::String;
        if (isSmallString()) return ValueType::SmallString;
        if (isObject()) return ValueType::Object;
        if (isSmallObject()) return ValueType::SmallObject;
        if (isBoolean()) return ValueType::Boolean;
        if (isNull()) return ValueType::Null;
        if (isUndefined()) return ValueType::Undefined;
        if (isSymbol()) return ValueType::Symbol;
        if (isBigInt()) return ValueType::BigInt;
        
        // 実際の実装ではより詳細な型判別が必要
        return ValueType::Undefined;
    }
};

// 値の配列操作用のSIMDヘルパー
class ValueSIMD {
public:
    // 複数の値を並列に変換
    static void toDouble4(__m256d& out, const Value* values);
    static void fromDouble4(Value* out, const __m256d& values);
    
    // 並列演算
    static void add4(Value* result, const Value* a, const Value* b);
    static void sub4(Value* result, const Value* a, const Value* b);
    static void mul4(Value* result, const Value* a, const Value* b);
    static void div4(Value* result, const Value* a, const Value* b);
    
    // 型判定の並列化
    static uint32_t testNumber4(const Value* values);
    static uint32_t testInt4(const Value* values);
};

} // namespace core
} // namespace aerojs