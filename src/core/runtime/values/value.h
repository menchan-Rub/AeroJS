/**
 * @file value.h
 * @brief JavaScript値の基本クラス定義
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_VALUE_H
#define AEROJS_VALUE_H

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

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

/**
 * @brief NaN-Boxing実装のための定数定義
 * JavaScriptの値をNaN-Boxing手法で64ビットに格納する
 */
namespace detail {
    // IEEE-754倍精度浮動小数点数のビットパターン定数
    constexpr uint64_t QUIET_NAN_MASK     = 0x7FF8000000000000ULL; // 静かなNaN
    constexpr uint64_t SIGN_BIT_MASK      = 0x8000000000000000ULL; // 符号ビット
    constexpr uint64_t EXPONENT_MASK      = 0x7FF0000000000000ULL; // 指数部マスク
    constexpr uint64_t MANTISSA_MASK      = 0x000FFFFFFFFFFFFFULL; // 仮数部マスク
    
    // タグビットパターン (52ビットの仮数部の上位4ビットを使用)
    constexpr uint64_t TAG_BITS_MASK      = 0x000F000000000000ULL; // タグビットマスク
    constexpr uint64_t TAG_UNDEFINED      = 0x0001000000000000ULL; // undefined
    constexpr uint64_t TAG_NULL           = 0x0002000000000000ULL; // null
    constexpr uint64_t TAG_BOOLEAN        = 0x0003000000000000ULL; // boolean
    constexpr uint64_t TAG_OBJECT         = 0x0004000000000000ULL; // オブジェクトへのポインタ
    constexpr uint64_t TAG_STRING         = 0x0005000000000000ULL; // 文字列へのポインタ
    constexpr uint64_t TAG_SYMBOL         = 0x0006000000000000ULL; // シンボルへのポインタ
    constexpr uint64_t TAG_BIGINT         = 0x0007000000000000ULL; // BigIntへのポインタ
    
    // タイプ判定用マスク
    constexpr uint64_t NUMBER_TYPE_MASK   = 0xFFF0000000000000ULL; // 数値型判定
    
    // 値用の追加ビット
    constexpr uint64_t BOOLEAN_TRUE       = 0x0000000000000001ULL; // true値
    
    // ポインタ用のマスク
    constexpr uint64_t PAYLOAD_MASK       = 0x0000FFFFFFFFFFFFULL; // ペイロードマスク (48ビット)
}

/**
 * @brief JavaScriptの値を表現するクラス
 * 
 * NaN-Boxingを使用して64ビットに全ての値を格納する
 * - 数値: IEEE-754倍精度浮動小数点数形式で格納
 * - その他: 静かなNaNの領域にタグとペイロードを格納
 */
class Value {
public:
    // 空の値を作成
    Value() : bits_(detail::QUIET_NAN_MASK | detail::TAG_UNDEFINED) {}
    
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
    
    // オブジェクト (ヘッダファイルのため宣言のみで実装は別ファイルで)
    static Value createObject(Object* object);
    
    // 文字列
    static Value createString(String* str);
    
    // シンボル
    static Value createSymbol(Symbol* symbol);
    
    // BigInt
    static Value createBigInt(BigInt* bigint);
    
    // 型チェック関数
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
    
    bool isNumber() const {
        return (bits_ & detail::NUMBER_TYPE_MASK) != detail::QUIET_NAN_MASK;
    }
    
    bool isInteger() const {
        if (!isNumber()) return false;
        double value = toNumber();
        return std::trunc(value) == value && 
               value >= -9007199254740991.0 && // -(2^53 - 1)
               value <= 9007199254740991.0;    // 2^53 - 1
    }
    
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
    
    // 特殊オブジェクト判定関数 (実装は別ファイルで)
    bool isFunction() const;
    bool isArray() const;
    bool isDate() const;
    bool isRegExp() const;
    bool isError() const;
    bool isPromise() const;
    bool isProxy() const;
    bool isMap() const;
    bool isSet() const;
    bool isWeakMap() const;
    bool isWeakSet() const;
    
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
            return toString().length() > 0;
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
        
        // 数値以外の型の変換 (実装は別ファイルで)
        if (isUndefined()) return std::numeric_limits<double>::quiet_NaN();
        if (isNull()) return 0.0;
        if (isBoolean()) return toBoolean() ? 1.0 : 0.0;
        
        // オブジェクトや文字列などの変換は別ファイルに実装する
        // ここでは単純な場合のみ扱う
        return std::numeric_limits<double>::quiet_NaN();
    }
    
    int32_t toInt32() const {
        // IEEE-754倍精度浮動小数点数からint32への標準変換
        double num = toNumber();
        if (std::isnan(num) || std::isinf(num)) {
            return 0;
        }
        
        constexpr double TWO_32 = 4294967296.0; // 2^32
        
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
        if (isObject()) {
            // オブジェクトのサブタイプは別途判定関数を呼び出す
            if (isArray()) return ValueType::Array;
            if (isFunction()) return ValueType::Function;
            if (isDate()) return ValueType::Date;
            if (isRegExp()) return ValueType::RegExp;
            if (isError()) return ValueType::Error;
            if (isMap()) return ValueType::Map;
            if (isSet()) return ValueType::Set;
            if (isWeakMap()) return ValueType::WeakMap;
            if (isWeakSet()) return ValueType::WeakSet;
            if (isPromise()) return ValueType::Promise;
            if (isProxy()) return ValueType::Proxy;
            // デフォルトはオブジェクト
            return ValueType::Object;
        }
        
        // 内部専用型 (通常は到達しない)
        return ValueType::Internal;
    }
    
    // 内部表現へのアクセス (レイヤーブレイク要注意)
    uint64_t getRawBits() const { return bits_; }
    
    // ポインタ抽出 (詳細実装は別ファイルで)
    Object* asObject() const;
    String* asString() const;
    Symbol* asSymbol() const;
    BigInt* asBigInt() const;
    Function* asFunction() const;
    
    // 比較関数
    bool equals(const Value& other) const;
    bool strictEquals(const Value& other) const;
    
private:
    // 内部ビットパターンから直接値を構築
    explicit Value(uint64_t bits) : bits_(bits) {}
    
    // 値の内部表現 (64ビット)
    uint64_t bits_;
};

// 標準値の定数
namespace constants {
    inline Value undefined() { return Value::createUndefined(); }
    inline Value null() { return Value::createNull(); }
    inline Value trueValue() { return Value::createBoolean(true); }
    inline Value falseValue() { return Value::createBoolean(false); }
    inline Value zero() { return Value::createNumber(0); }
    inline Value one() { return Value::createNumber(1); }
    inline Value nan() { return Value::createNumber(std::numeric_limits<double>::quiet_NaN()); }
    inline Value infinity() { return Value::createNumber(std::numeric_limits<double>::infinity()); }
    inline Value negativeInfinity() { return Value::createNumber(-std::numeric_limits<double>::infinity()); }
}

} // namespace core
} // namespace aerojs

#endif // AEROJS_VALUE_H 