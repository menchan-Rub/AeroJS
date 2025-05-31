/**
 * @file simple_value.cpp
 * @brief AeroJS 簡単なJavaScript値システム実装
 * @version 0.1.0
 * @license MIT
 */

#include "simple_value.h"
#include <cstring>

namespace aerojs {
namespace core {

SimpleValue::SimpleValue() : type_(ValueType::Undefined) {
    std::memset(&data_, 0, sizeof(data_));
}

SimpleValue::~SimpleValue() = default;

SimpleValue SimpleValue::undefined() {
    SimpleValue v;
    v.type_ = ValueType::Undefined;
    return v;
}

SimpleValue SimpleValue::null() {
    SimpleValue v;
    v.type_ = ValueType::Null;
    return v;
}

SimpleValue SimpleValue::fromBoolean(bool value) {
    SimpleValue v;
    v.type_ = ValueType::Boolean;
    v.data_.boolValue = value;
    return v;
}

SimpleValue SimpleValue::fromNumber(double value) {
    SimpleValue v;
    v.type_ = ValueType::Number;
    v.data_.numberValue = value;
    return v;
}

SimpleValue SimpleValue::fromString(const std::string& value) {
    SimpleValue v;
    v.type_ = ValueType::String;
    v.stringValue_ = value;
    return v;
}

bool SimpleValue::isUndefined() const {
    return type_ == ValueType::Undefined;
}

bool SimpleValue::isNull() const {
    return type_ == ValueType::Null;
}

bool SimpleValue::isBoolean() const {
    return type_ == ValueType::Boolean;
}

bool SimpleValue::isNumber() const {
    return type_ == ValueType::Number;
}

bool SimpleValue::isString() const {
    return type_ == ValueType::String;
}

bool SimpleValue::toBoolean() const {
    switch (type_) {
        case ValueType::Boolean:
            return data_.boolValue;
        case ValueType::Number:
            return data_.numberValue != 0.0;
        case ValueType::String:
            return !stringValue_.empty();
        case ValueType::Undefined:
        case ValueType::Null:
        default:
            return false;
    }
}

double SimpleValue::toNumber() const {
    switch (type_) {
        case ValueType::Number:
            return data_.numberValue;
        case ValueType::Boolean:
            return data_.boolValue ? 1.0 : 0.0;
        case ValueType::String:
            try {
                return std::stod(stringValue_);
            } catch (...) {
                return 0.0; // NaN相当
            }
        case ValueType::Undefined:
        case ValueType::Null:
        default:
            return 0.0;
    }
}

std::string SimpleValue::toString() const {
    switch (type_) {
        case ValueType::String:
            return stringValue_;
        case ValueType::Number:
            return std::to_string(data_.numberValue);
        case ValueType::Boolean:
            return data_.boolValue ? "true" : "false";
        case ValueType::Null:
            return "null";
        case ValueType::Undefined:
            return "undefined";
        default:
            return "";
    }
}

ValueType SimpleValue::getType() const {
    return type_;
}

} // namespace core
} // namespace aerojs 