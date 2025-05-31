/**
 * @file simple_value.h
 * @brief AeroJS 簡単なJavaScript値システム
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_CORE_SIMPLE_VALUE_H
#define AEROJS_CORE_SIMPLE_VALUE_H

#include <string>

namespace aerojs {
namespace core {

/**
 * @brief JavaScript値の型
 */
enum class ValueType {
    Undefined,
    Null,
    Boolean,
    Number,
    String,
    Object
};

/**
 * @brief 簡単なJavaScript値クラス
 */
class SimpleValue {
public:
    SimpleValue();
    ~SimpleValue();

    // 静的ファクトリメソッド
    static SimpleValue undefined();
    static SimpleValue null();
    static SimpleValue fromBoolean(bool value);
    static SimpleValue fromNumber(double value);
    static SimpleValue fromString(const std::string& value);

    // 型チェック
    bool isUndefined() const;
    bool isNull() const;
    bool isBoolean() const;
    bool isNumber() const;
    bool isString() const;

    // 値の取得
    bool toBoolean() const;
    double toNumber() const;
    std::string toString() const;

    // 型の取得
    ValueType getType() const;

private:
    ValueType type_;
    union {
        bool boolValue;
        double numberValue;
    } data_;
    std::string stringValue_;
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_SIMPLE_VALUE_H 