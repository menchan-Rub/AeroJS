/**
 * @file value.cpp
 * @brief AeroJS JavaScript値システム実装
 * @version 0.3.0
 * @license MIT
 */

#include "value.h"
#include <cstring>
#include <cmath>
#include <limits>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <functional>

namespace aerojs {
namespace core {

// コンストラクタ
Value::Value() : type_(ValueType::Undefined), 
                 writable_(true), enumerable_(true), configurable_(true),
                 frozen_(false), sealed_(false), extensible_(true), 
                 markedForGC_(false), refCount_(0), 
                 hashValue_(0), hashComputed_(false) {
    pointerValue_ = nullptr;
}

Value::Value(const Value& other) : 
                 writable_(true), enumerable_(true), configurable_(true),
                 frozen_(false), sealed_(false), extensible_(true), 
                 markedForGC_(false), refCount_(0), 
                 hashValue_(0), hashComputed_(false) {
    copyFrom(other);
}

Value::Value(Value&& other) noexcept :
                 writable_(true), enumerable_(true), configurable_(true),
                 frozen_(false), sealed_(false), extensible_(true), 
                 markedForGC_(false), refCount_(0), 
                 hashValue_(0), hashComputed_(false) {
    moveFrom(std::move(other));
}

Value::Value(bool value) : type_(ValueType::Boolean),
                          writable_(true), enumerable_(true), configurable_(true),
                          frozen_(false), sealed_(false), extensible_(true), 
                          markedForGC_(false), refCount_(0), 
                          hashValue_(0), hashComputed_(false) {
    boolValue_ = value;
}

Value::Value(int32_t value) : type_(ValueType::Number),
                             writable_(true), enumerable_(true), configurable_(true),
                             frozen_(false), sealed_(false), extensible_(true), 
                             markedForGC_(false), refCount_(0), 
                             hashValue_(0), hashComputed_(false) {
    numberValue_ = static_cast<double>(value);
}

Value::Value(double value) : type_(ValueType::Number),
                            writable_(true), enumerable_(true), configurable_(true),
                            frozen_(false), sealed_(false), extensible_(true), 
                            markedForGC_(false), refCount_(0), 
                            hashValue_(0), hashComputed_(false) {
    numberValue_ = value;
}

Value::Value(const std::string& value) : type_(ValueType::String),
                                        writable_(true), enumerable_(true), configurable_(true),
                                        frozen_(false), sealed_(false), extensible_(true), 
                                        markedForGC_(false), refCount_(0), 
                                        hashValue_(0), hashComputed_(false) {
    stringValue_ = new std::string(value);
}

Value::Value(const char* value) : type_(ValueType::String),
                                 writable_(true), enumerable_(true), configurable_(true),
                                 frozen_(false), sealed_(false), extensible_(true), 
                                 markedForGC_(false), refCount_(0), 
                                 hashValue_(0), hashComputed_(false) {
    stringValue_ = new std::string(value ? value : "");
}

Value::~Value() {
    cleanup();
}

ValueType Value::getType() const {
    return type_;
}

// 型チェック
bool Value::isUndefined() const {
    return type_ == ValueType::Undefined;
}

bool Value::isNull() const {
    return type_ == ValueType::Null;
}

bool Value::isNullish() const {
    return isNull() || isUndefined();
}

bool Value::isBoolean() const {
    return type_ == ValueType::Boolean;
}

bool Value::isNumber() const {
    return type_ == ValueType::Number;
}

bool Value::isInteger() const {
    if (!isNumber()) return false;
    return std::floor(numberValue_) == numberValue_ && std::isfinite(numberValue_);
}

bool Value::isFinite() const {
    return isNumber() && std::isfinite(numberValue_);
}

bool Value::isNaN() const {
    return isNumber() && std::isnan(numberValue_);
}

bool Value::isString() const {
    return type_ == ValueType::String;
}

bool Value::isSymbol() const {
    return type_ == ValueType::Symbol;
}

bool Value::isBigInt() const {
    return type_ == ValueType::BigInt;
}

bool Value::isObject() const {
    return type_ == ValueType::Object;
}

bool Value::isArray() const {
    return type_ == ValueType::Array;
}

bool Value::isFunction() const {
    return type_ == ValueType::Function;
}

bool Value::isCallable() const {
    return isFunction();
}

bool Value::isPrimitive() const {
    return type_ == ValueType::Undefined || type_ == ValueType::Null ||
           type_ == ValueType::Boolean || type_ == ValueType::Number ||
           type_ == ValueType::String || type_ == ValueType::Symbol ||
           type_ == ValueType::BigInt;
}

bool Value::isTruthy() const {
    return !isFalsy();
}

bool Value::isFalsy() const {
    switch (type_) {
        case ValueType::Undefined:
        case ValueType::Null:
            return true;
        case ValueType::Boolean:
            return !boolValue_;
        case ValueType::Number:
            return numberValue_ == 0.0 || std::isnan(numberValue_);
        case ValueType::String:
            return stringValue_->empty();
        default:
            return false;
    }
}

// 型変換
bool Value::toBoolean() const {
    return !isFalsy();
}

double Value::toNumber() const {
    switch (type_) {
        case ValueType::Undefined:
            return std::numeric_limits<double>::quiet_NaN();
        case ValueType::Null:
            return 0.0;
        case ValueType::Boolean:
            return boolValue_ ? 1.0 : 0.0;
        case ValueType::Number:
            return numberValue_;
        case ValueType::String:
            return stringToNumber(*stringValue_);
        default:
            return std::numeric_limits<double>::quiet_NaN();
    }
}

int32_t Value::toInt32() const {
    double num = toNumber();
    if (std::isnan(num) || std::isinf(num) || num == 0.0) {
        return 0;
    }
    return static_cast<int32_t>(num);
}

uint32_t Value::toUint32() const {
    double num = toNumber();
    if (std::isnan(num) || std::isinf(num) || num == 0.0) {
        return 0;
    }
    return static_cast<uint32_t>(num);
}

int64_t Value::toInt64() const {
    double num = toNumber();
    if (std::isnan(num) || std::isinf(num) || num == 0.0) {
        return 0;
    }
    return static_cast<int64_t>(num);
}

uint64_t Value::toUint64() const {
    double num = toNumber();
    if (std::isnan(num) || std::isinf(num) || num == 0.0) {
        return 0;
    }
    return static_cast<uint64_t>(num);
}

std::string Value::toString() const {
    switch (type_) {
        case ValueType::Undefined:
            return "undefined";
        case ValueType::Null:
            return "null";
        case ValueType::Boolean:
            return boolValue_ ? "true" : "false";
        case ValueType::Number:
            return numberToString(numberValue_);
        case ValueType::String:
            return *stringValue_;
        default:
            return "[object Object]";
    }
}

std::string Value::toStringRepresentation() const {
    return toString();
}

void* Value::toObject() const {
    if (isObject()) {
        return pointerValue_;
    }
    return nullptr;
}

// 安全な型変換
bool Value::tryToBoolean(bool& result) const {
    result = toBoolean();
    return true;
}

bool Value::tryToNumber(double& result) const {
    result = toNumber();
    return !std::isnan(result);
}

bool Value::tryToInt32(int32_t& result) const {
    result = toInt32();
    return true;
}

bool Value::tryToString(std::string& result) const {
    result = toString();
    return true;
}

// アクセサ
const char* Value::getTypeName() const {
    switch (type_) {
        case ValueType::Undefined: return "undefined";
        case ValueType::Null: return "null";
        case ValueType::Boolean: return "boolean";
        case ValueType::Number: return "number";
        case ValueType::String: return "string";
        case ValueType::Symbol: return "symbol";
        case ValueType::BigInt: return "bigint";
        case ValueType::Object: return "object";
        case ValueType::Array: return "array";
        case ValueType::Function: return "function";
        default: return "unknown";
    }
}

size_t Value::getSize() const {
    switch (type_) {
        case ValueType::String:
            return stringValue_->size();
        default:
            return sizeof(*this);
    }
}

uint64_t Value::getHash() const {
    if (!hashComputed_) {
        hashValue_ = computeHash();
        hashComputed_ = true;
    }
    return hashValue_;
}

// 比較演算
bool Value::equals(const Value& other) const {
    if (type_ != other.type_) {
        return false;
    }
    
    switch (type_) {
        case ValueType::Undefined:
        case ValueType::Null:
            return true;
        case ValueType::Boolean:
            return boolValue_ == other.boolValue_;
        case ValueType::Number:
            return numberValue_ == other.numberValue_;
        case ValueType::String:
            return *stringValue_ == *other.stringValue_;
        default:
            return pointerValue_ == other.pointerValue_;
    }
}

bool Value::strictEquals(const Value& other) const {
    return equals(other);
}

bool Value::sameValue(const Value& other) const {
    return equals(other);
}

ComparisonResult Value::compare(const Value& other) const {
    return abstractComparison(other);
}

// 演算子オーバーロード
bool Value::operator==(const Value& other) const {
    return equals(other);
}

bool Value::operator!=(const Value& other) const {
    return !equals(other);
}

bool Value::operator<(const Value& other) const {
    return compare(other) == ComparisonResult::LessThan;
}

bool Value::operator<=(const Value& other) const {
    auto result = compare(other);
    return result == ComparisonResult::LessThan || result == ComparisonResult::Equal;
}

bool Value::operator>(const Value& other) const {
    return compare(other) == ComparisonResult::GreaterThan;
}

bool Value::operator>=(const Value& other) const {
    auto result = compare(other);
    return result == ComparisonResult::GreaterThan || result == ComparisonResult::Equal;
}

// 配列操作
Value Value::getElement(size_t index) const {
    // 簡略化された実装
    return Value::undefined();
}

void Value::setElement(size_t index, const Value& value) {
    // 簡略化された実装
}

size_t Value::getLength() const {
    return 0;
}

void Value::push(const Value& value) {
    // 簡略化された実装
}

Value Value::pop() {
    return Value::undefined();
}

// オブジェクト操作
Value Value::getProperty(const std::string& key) const {
    // 簡略化された実装
    return Value::undefined();
}

void Value::setProperty(const std::string& key, const Value& value) {
    // 簡略化された実装
}

bool Value::hasProperty(const std::string& key) const {
    return false;
}

void Value::deleteProperty(const std::string& key) {
    // 簡略化された実装
}

std::vector<std::string> Value::getPropertyNames() const {
    return {};
}

// 関数呼び出し
Value Value::call(const std::vector<Value>& args) const {
    return Value::undefined();
}

Value Value::call(const Value& thisValue, const std::vector<Value>& args) const {
    return Value::undefined();
}

// ユーティリティ
Value Value::clone() const {
    return Value(*this);
}

void Value::freeze() {
    frozen_ = true;
}

bool Value::isFrozen() const {
    return frozen_;
}

void Value::seal() {
    sealed_ = true;
}

bool Value::isSealed() const {
    return sealed_;
}

bool Value::isExtensible() const {
    return extensible_;
}

void Value::preventExtensions() {
    extensible_ = false;
}

// デバッグ・診断
std::string Value::inspect() const {
    return toString();
}

void Value::dump() const {
    // 簡略化された実装
}

bool Value::isValid() const {
    return true;
}

// メモリ管理
void Value::markForGC() {
    markedForGC_ = true;
}

void Value::unmarkForGC() {
    markedForGC_ = false;
}

bool Value::isMarkedForGC() const {
    return markedForGC_;
}

void Value::incrementRefCount() {
    refCount_++;
}

void Value::decrementRefCount() {
    if (refCount_ > 0) {
        refCount_--;
    }
}

size_t Value::getRefCount() const {
    return refCount_;
}

// 静的ファクトリメソッド
Value Value::undefined() {
    return Value();
}

Value Value::null() {
    Value v;
    v.type_ = ValueType::Null;
    v.pointerValue_ = nullptr;
    return v;
}

Value Value::fromBoolean(bool value) {
    return Value(value);
}

Value Value::fromNumber(double value) {
    return Value(value);
}

Value Value::fromString(const std::string& value) {
    return Value(value);
}

Value Value::fromObject(void* object) {
    Value result;
    result.type_ = ValueType::Object;
    result.pointerValue_ = object;
    return result;
}

Value Value::fromInteger(int32_t value) {
    return Value(value);
}

Value Value::fromArray(const std::vector<Value>& values) {
    Value result;
    result.type_ = ValueType::Array;
    result.pointerValue_ = nullptr; // 簡略化実装
    return result;
}

Value Value::fromFunction(void* function) {
    Value result;
    result.type_ = ValueType::Function;
    result.pointerValue_ = function;
    return result;
}

// 内部ヘルパーメソッド
void Value::cleanup() {
    if (type_ == ValueType::String && stringValue_) {
        delete stringValue_;
        stringValue_ = nullptr;
    }
}

void Value::copyFrom(const Value& other) {
    type_ = other.type_;
    
    switch (type_) {
        case ValueType::Boolean:
            boolValue_ = other.boolValue_;
            break;
        case ValueType::Number:
            numberValue_ = other.numberValue_;
            break;
        case ValueType::String:
            stringValue_ = new std::string(*other.stringValue_);
            break;
        default:
            pointerValue_ = other.pointerValue_;
            break;
    }
}

void Value::moveFrom(Value&& other) {
    type_ = other.type_;
    
    switch (type_) {
        case ValueType::Boolean:
            boolValue_ = other.boolValue_;
            break;
        case ValueType::Number:
            numberValue_ = other.numberValue_;
            break;
        case ValueType::String:
            stringValue_ = other.stringValue_;
            other.stringValue_ = nullptr;
            break;
        default:
            pointerValue_ = other.pointerValue_;
            other.pointerValue_ = nullptr;
            break;
    }
    
    other.type_ = ValueType::Undefined;
}

double Value::stringToNumber(const std::string& str) const {
    if (str.empty()) return 0.0;
    
    try {
        return std::stod(str);
    } catch (...) {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

std::string Value::numberToString(double num) const {
    if (std::isnan(num)) return "NaN";
    if (std::isinf(num)) return num > 0 ? "Infinity" : "-Infinity";
    
    std::ostringstream oss;
    oss << num;
    return oss.str();
}

ComparisonResult Value::abstractComparison(const Value& other) const {
    // 簡略化された抽象比較
    double thisNum = toNumber();
    double otherNum = other.toNumber();
    
    if (std::isnan(thisNum) || std::isnan(otherNum)) {
        return ComparisonResult::Undefined;
    }
    
    if (thisNum < otherNum) {
        return ComparisonResult::LessThan;
    } else if (thisNum > otherNum) {
        return ComparisonResult::GreaterThan;
    } else {
        return ComparisonResult::Equal;
    }
}

uint64_t Value::computeHash() const {
    std::hash<std::string> hasher;
    
    switch (type_) {
        case ValueType::Undefined:
            return 0;
        case ValueType::Null:
            return 1;
        case ValueType::Boolean:
            return boolValue_ ? 2 : 3;
        case ValueType::Number:
            return std::hash<double>{}(numberValue_);
        case ValueType::String:
            return hasher(*stringValue_);
        default:
            return reinterpret_cast<uint64_t>(pointerValue_);
    }
}

} // namespace core
} // namespace aerojs 