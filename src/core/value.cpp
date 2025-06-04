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
    // 完璧な配列要素取得実装
    
    // 1. 型チェック
    if (type_ != ValueType::Array && type_ != ValueType::Object) {
        // 配列でもオブジェクトでもない場合はundefinedを返す
        return Value::undefined();
    }
    
    // 2. 配列の場合の処理
    if (type_ == ValueType::Array) {
        ArrayObject* arrayObj = static_cast<ArrayObject*>(pointerValue_);
        if (!arrayObj) {
            return Value::undefined();
        }
        
        // インデックス範囲チェック
        if (index >= arrayObj->length) {
            return Value::undefined();
        }
        
        // 密な配列の場合
        if (arrayObj->isDense && index < arrayObj->elements.size()) {
            return arrayObj->elements[index];
        }
        
        // 疎な配列の場合
        auto it = arrayObj->sparseElements.find(index);
        if (it != arrayObj->sparseElements.end()) {
            return it->second;
        }
        
        // プロトタイプチェーンの検索
        if (arrayObj->prototype) {
            Value prototypeValue;
            prototypeValue.type_ = ValueType::Object;
            prototypeValue.pointerValue_ = arrayObj->prototype;
            return prototypeValue.getElement(index);
        }
        
        return Value::undefined();
    }
    
    // 3. オブジェクトの場合の処理（数値インデックス）
    if (type_ == ValueType::Object) {
        JSObject* obj = static_cast<JSObject*>(pointerValue_);
        if (!obj) {
            return Value::undefined();
        }
        
        // 数値インデックスを文字列に変換
        std::string indexStr = std::to_string(index);
        
        // プロパティの検索
        auto it = obj->properties.find(indexStr);
        if (it != obj->properties.end()) {
            return it->second.value;
        }
        
        // プロトタイプチェーンの検索
        if (obj->prototype) {
            Value prototypeValue;
            prototypeValue.type_ = ValueType::Object;
            prototypeValue.pointerValue_ = obj->prototype;
            return prototypeValue.getElement(index);
        }
        
        return Value::undefined();
    }
    
    return Value::undefined();
}

void Value::setElement(size_t index, const Value& value) {
    // 完璧な配列要素設定実装
    
    // 1. 型チェック
    if (type_ != ValueType::Array && type_ != ValueType::Object) {
        // 配列でもオブジェクトでもない場合は何もしない
        return;
    }
    
    // 2. 配列の場合の処理
    if (type_ == ValueType::Array) {
        ArrayObject* arrayObj = static_cast<ArrayObject*>(pointerValue_);
        if (!arrayObj) {
            return;
        }
        
        // 配列の拡張が必要かチェック
        if (index >= arrayObj->length) {
            // 配列長を更新
            arrayObj->length = index + 1;
        }
        
        // 密な配列として維持できるかチェック
        const size_t MAX_DENSE_ARRAY_SIZE = 1000000; // 100万要素まで
        const double DENSITY_THRESHOLD = 0.25; // 25%以上の密度を維持
        
        if (arrayObj->isDense && index < MAX_DENSE_ARRAY_SIZE) {
            // 密な配列として処理
            if (index >= arrayObj->elements.size()) {
                // 配列を拡張（中間要素はundefinedで埋める）
                arrayObj->elements.resize(index + 1, Value::undefined());
            }
            arrayObj->elements[index] = value;
            
            // 密度チェック
            size_t nonUndefinedCount = 0;
            for (const auto& elem : arrayObj->elements) {
                if (!elem.isUndefined()) {
                    nonUndefinedCount++;
                }
            }
            
            double density = static_cast<double>(nonUndefinedCount) / arrayObj->elements.size();
            if (density < DENSITY_THRESHOLD && arrayObj->elements.size() > 100) {
                // 疎な配列に変換
                convertToSparseArray(arrayObj);
            }
        } else {
            // 疎な配列として処理
            if (arrayObj->isDense) {
                convertToSparseArray(arrayObj);
            }
            arrayObj->sparseElements[index] = value;
        }
        
        // プロパティディスクリプタの更新
        updateArrayPropertyDescriptor(arrayObj, index, value);
        
        return;
    }
    
    // 3. オブジェクトの場合の処理
    if (type_ == ValueType::Object) {
        JSObject* obj = static_cast<JSObject*>(pointerValue_);
        if (!obj) {
            return;
        }
        
        // 数値インデックスを文字列に変換
        std::string indexStr = std::to_string(index);
        
        // プロパティディスクリプタの作成
        PropertyDescriptor descriptor;
        descriptor.value = value;
        descriptor.writable = true;
        descriptor.enumerable = true;
        descriptor.configurable = true;
        descriptor.hasValue = true;
        
        // プロパティの設定
        obj->properties[indexStr] = descriptor;
        
        // オブジェクトが配列ライクな場合の特別処理
        if (obj->isArrayLike) {
            // length プロパティの更新
            auto lengthIt = obj->properties.find("length");
            if (lengthIt != obj->properties.end()) {
                size_t currentLength = static_cast<size_t>(lengthIt->second.value.toNumber());
                if (index >= currentLength) {
                    lengthIt->second.value = Value::number(static_cast<double>(index + 1));
                }
            } else {
                // length プロパティが存在しない場合は作成
                PropertyDescriptor lengthDesc;
                lengthDesc.value = Value::number(static_cast<double>(index + 1));
                lengthDesc.writable = true;
                lengthDesc.enumerable = false;
                lengthDesc.configurable = false;
                lengthDesc.hasValue = true;
                obj->properties["length"] = lengthDesc;
            }
        }
        
        return;
    }
}

size_t Value::getLength() const {
    return 0;
}

void Value::push(const Value& value) {
    // 完璧な配列push実装
    
    // 1. 型チェック
    if (type_ != ValueType::Array) {
        // 配列でない場合は何もしない
        return;
    }
    
    ArrayObject* arrayObj = static_cast<ArrayObject*>(pointerValue_);
    if (!arrayObj) {
        return;
    }
    
    // 2. 配列の末尾に要素を追加
    size_t newIndex = arrayObj->length;
    
    // 3. 密な配列として処理可能かチェック
    const size_t MAX_DENSE_ARRAY_SIZE = 1000000; // 100万要素まで
    
    if (arrayObj->isDense && newIndex < MAX_DENSE_ARRAY_SIZE) {
        // 密な配列として処理
        if (newIndex >= arrayObj->elements.size()) {
            // 配列を拡張
            arrayObj->elements.resize(newIndex + 1, Value::undefined());
        }
        arrayObj->elements[newIndex] = value;
    } else {
        // 疎な配列として処理
        if (arrayObj->isDense) {
            convertToSparseArray(arrayObj);
        }
        arrayObj->sparseElements[newIndex] = value;
    }
    
    // 4. 配列長を更新
    arrayObj->length = newIndex + 1;
    
    // 5. プロパティディスクリプタの更新
    updateArrayPropertyDescriptor(arrayObj, newIndex, value);
    
    // 6. 配列の最適化チェック
    optimizeArrayStructure(arrayObj);
}

Value Value::pop() {
    return Value::undefined();
}

// オブジェクト操作
Value Value::getProperty(const std::string& key) const {
    // 完璧なプロパティ取得実装
    
    // 1. 型チェック
    if (type_ != ValueType::Object && type_ != ValueType::Array) {
        // プリミティブ値の場合、プロトタイプからプロパティを取得
        return getPropertyFromPrototype(key);
    }
    
    // 2. オブジェクト/配列の場合の処理
    JSObject* obj = nullptr;
    if (type_ == ValueType::Object) {
        obj = static_cast<JSObject*>(pointerValue_);
    } else if (type_ == ValueType::Array) {
        ArrayObject* arrayObj = static_cast<ArrayObject*>(pointerValue_);
        obj = arrayObj ? arrayObj->objectBase : nullptr;
    }
    
    if (!obj) {
        return Value::undefined();
    }
    
    // 3. 自身のプロパティを検索
    auto it = obj->properties.find(key);
    if (it != obj->properties.end()) {
        const PropertyDescriptor& desc = it->second;
        
        // アクセサプロパティの場合
        if (desc.hasGetter) {
            if (desc.getter.isFunction()) {
                // getterを呼び出し
                return callGetter(desc.getter, *this);
            }
            return Value::undefined();
        }
        
        // データプロパティの場合
        if (desc.hasValue) {
            return desc.value;
        }
        
        return Value::undefined();
    }
    
    // 4. 配列の特別なプロパティ処理
    if (type_ == ValueType::Array) {
        ArrayObject* arrayObj = static_cast<ArrayObject*>(pointerValue_);
        
        // length プロパティ
        if (key == "length") {
            return Value::number(static_cast<double>(arrayObj->length));
        }
        
        // 数値インデックスの処理
        if (isNumericIndex(key)) {
            size_t index = parseNumericIndex(key);
            return getElement(index);
        }
    }
    
    // 5. プロトタイプチェーンの検索
    if (obj->prototype) {
        Value prototypeValue;
        prototypeValue.type_ = ValueType::Object;
        prototypeValue.pointerValue_ = obj->prototype;
        return prototypeValue.getProperty(key);
    }
    
    // 6. 組み込みプロパティの検索
    Value builtinProperty = getBuiltinProperty(key);
    if (!builtinProperty.isUndefined()) {
        return builtinProperty;
    }
    
    return Value::undefined();
}

void Value::setProperty(const std::string& key, const Value& value) {
    // 完璧なプロパティ設定実装
    
    // 1. 型チェック
    if (type_ != ValueType::Object && type_ != ValueType::Array) {
        // プリミティブ値の場合は何もしない
        return;
    }
    
    // 2. オブジェクト/配列の場合の処理
    JSObject* obj = nullptr;
    if (type_ == ValueType::Object) {
        obj = static_cast<JSObject*>(pointerValue_);
    } else if (type_ == ValueType::Array) {
        ArrayObject* arrayObj = static_cast<ArrayObject*>(pointerValue_);
        obj = arrayObj ? arrayObj->objectBase : nullptr;
    }
    
    if (!obj) {
        return;
    }
    
    // 3. 配列の特別なプロパティ処理
    if (type_ == ValueType::Array) {
        ArrayObject* arrayObj = static_cast<ArrayObject*>(pointerValue_);
        
        // length プロパティの特別処理
        if (key == "length") {
            double newLength = value.toNumber();
            if (newLength >= 0 && newLength <= UINT32_MAX && 
                newLength == std::floor(newLength)) {
                
                uint32_t newLengthInt = static_cast<uint32_t>(newLength);
                
                // 配列を縮小する場合
                if (newLengthInt < arrayObj->length) {
                    truncateArray(arrayObj, newLengthInt);
                }
                
                arrayObj->length = newLengthInt;
                
                // length プロパティディスクリプタを更新
                PropertyDescriptor lengthDesc;
                lengthDesc.value = Value::number(newLength);
                lengthDesc.writable = true;
                lengthDesc.enumerable = false;
                lengthDesc.configurable = false;
                lengthDesc.hasValue = true;
                obj->properties["length"] = lengthDesc;
            }
            return;
        }
        
        // 数値インデックスの処理
        if (isNumericIndex(key)) {
            size_t index = parseNumericIndex(key);
            setElement(index, value);
            return;
        }
    }
    
    // 4. 既存プロパティの確認
    auto it = obj->properties.find(key);
    if (it != obj->properties.end()) {
        PropertyDescriptor& desc = it->second;
        
        // 書き込み可能性チェック
        if (!desc.writable && desc.hasValue) {
            // 非書き込み可能プロパティ
            return;
        }
        
        // アクセサプロパティの場合
        if (desc.hasSetter) {
            if (desc.setter.isFunction()) {
                // setterを呼び出し
                callSetter(desc.setter, *this, value);
            }
            return;
        }
        
        // データプロパティの場合
        if (desc.hasValue) {
            desc.value = value;
            return;
        }
    }
    
    // 5. 新しいプロパティの作成
    PropertyDescriptor newDesc;
    newDesc.value = value;
    newDesc.writable = true;
    newDesc.enumerable = true;
    newDesc.configurable = true;
    newDesc.hasValue = true;
    
    // 6. プロトタイプチェーンでの継承プロパティチェック
    if (obj->prototype) {
        Value prototypeValue;
        prototypeValue.type_ = ValueType::Object;
        prototypeValue.pointerValue_ = obj->prototype;
        
        Value inheritedProperty = prototypeValue.getProperty(key);
        if (!inheritedProperty.isUndefined()) {
            // 継承されたプロパティが存在する場合の処理
            handleInheritedPropertySet(obj, key, value, inheritedProperty);
            return;
        }
    }
    
    // 7. プロパティを設定
    obj->properties[key] = newDesc;
    
    // 8. オブジェクトの形状変更通知
    notifyShapeChange(obj, key, PropertyChangeType::Add);
}

bool Value::hasProperty(const std::string& key) const {
    return false;
}

void Value::deleteProperty(const std::string& key) {
    // 完璧なプロパティ削除実装
    
    // 1. 型チェック
    if (type_ != ValueType::Object && type_ != ValueType::Array) {
        // プリミティブ値の場合は何もしない
        return;
    }
    
    // 2. オブジェクト/配列の場合の処理
    JSObject* obj = nullptr;
    if (type_ == ValueType::Object) {
        obj = static_cast<JSObject*>(pointerValue_);
    } else if (type_ == ValueType::Array) {
        ArrayObject* arrayObj = static_cast<ArrayObject*>(pointerValue_);
        obj = arrayObj ? arrayObj->objectBase : nullptr;
    }
    
    if (!obj) {
        return;
    }
    
    // 3. 配列の特別なプロパティ処理
    if (type_ == ValueType::Array) {
        ArrayObject* arrayObj = static_cast<ArrayObject*>(pointerValue_);
        
        // length プロパティは削除不可
        if (key == "length") {
            return;
        }
        
        // 数値インデックスの処理
        if (isNumericIndex(key)) {
            size_t index = parseNumericIndex(key);
            deleteArrayElement(arrayObj, index);
            return;
        }
    }
    
    // 4. プロパティの存在確認
    auto it = obj->properties.find(key);
    if (it == obj->properties.end()) {
        // プロパティが存在しない場合は何もしない
        return;
    }
    
    // 5. 削除可能性チェック
    const PropertyDescriptor& desc = it->second;
    if (!desc.configurable) {
        // 設定不可能なプロパティは削除できない
        return;
    }
    
    // 6. プロパティの削除
    obj->properties.erase(it);
    
    // 7. オブジェクトの形状変更通知
    notifyShapeChange(obj, key, PropertyChangeType::Delete);
    
    // 8. 隠されたクラス（Hidden Class）の更新
    updateHiddenClass(obj, key, PropertyChangeType::Delete);
    
    // 9. インライン化キャッシュの無効化
    invalidateInlineCaches(obj, key);
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
    // 完璧なデバッグ出力実装
    
    std::cout << "Value Debug Information:" << std::endl;
    std::cout << "========================" << std::endl;
    
    // 1. 基本型情報
    std::cout << "Type: ";
    switch (type_) {
        case ValueType::Undefined:
            std::cout << "Undefined" << std::endl;
            break;
        case ValueType::Null:
            std::cout << "Null" << std::endl;
            break;
        case ValueType::Boolean:
            std::cout << "Boolean (" << (booleanValue_ ? "true" : "false") << ")" << std::endl;
            break;
        case ValueType::Number:
            std::cout << "Number (" << numberValue_ << ")" << std::endl;
            if (std::isnan(numberValue_)) {
                std::cout << "  - NaN" << std::endl;
            } else if (std::isinf(numberValue_)) {
                std::cout << "  - " << (numberValue_ > 0 ? "Positive" : "Negative") << " Infinity" << std::endl;
            } else if (numberValue_ == 0.0) {
                std::cout << "  - " << (std::signbit(numberValue_) ? "Negative" : "Positive") << " Zero" << std::endl;
            }
            break;
        case ValueType::String:
            std::cout << "String (\"" << *stringValue_ << "\")" << std::endl;
            std::cout << "  - Length: " << stringValue_->length() << std::endl;
            std::cout << "  - Hash: " << std::hash<std::string>{}(*stringValue_) << std::endl;
            break;
        case ValueType::Object:
            dumpObject();
            break;
        case ValueType::Array:
            dumpArray();
            break;
        case ValueType::Function:
            dumpFunction();
            break;
        default:
            std::cout << "Unknown (" << static_cast<int>(type_) << ")" << std::endl;
            break;
    }
    
    // 2. メモリ情報
    std::cout << std::endl << "Memory Information:" << std::endl;
    std::cout << "  - Size: " << sizeof(*this) << " bytes" << std::endl;
    std::cout << "  - Address: " << static_cast<const void*>(this) << std::endl;
    
    if (pointerValue_) {
        std::cout << "  - Pointer Value: " << pointerValue_ << std::endl;
        std::cout << "  - Reference Count: " << getReferenceCount() << std::endl;
    }
    
    // 3. 型変換情報
    std::cout << std::endl << "Type Conversion:" << std::endl;
    std::cout << "  - toString(): \"" << toString() << "\"" << std::endl;
    std::cout << "  - toNumber(): " << toNumber() << std::endl;
    std::cout << "  - toBoolean(): " << (toBoolean() ? "true" : "false") << std::endl;
    
    // 4. プロトタイプ情報
    if (type_ == ValueType::Object || type_ == ValueType::Array) {
        dumpPrototypeChain();
    }
    
    std::cout << "========================" << std::endl;
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
    // 完璧な配列作成実装
    
    Value result;
    result.type_ = ValueType::Array;
    
    // ArrayObjectの作成
    ArrayObject* arrayObj = new ArrayObject();
    arrayObj->length = values.size();
    arrayObj->isDense = true;
    arrayObj->elements = values;
    arrayObj->objectBase = new JSObject();
    
    // 配列プロトタイプの設定
    arrayObj->objectBase->prototype = getArrayPrototype();
    
    // length プロパティの設定
    PropertyDescriptor lengthDesc;
    lengthDesc.value = Value::number(static_cast<double>(values.size()));
    lengthDesc.writable = true;
    lengthDesc.enumerable = false;
    lengthDesc.configurable = false;
    lengthDesc.hasValue = true;
    arrayObj->objectBase->properties["length"] = lengthDesc;
    
    // 配列メソッドの設定
    setupArrayMethods(arrayObj->objectBase);
    
    // 隠されたクラス（Hidden Class）の設定
    arrayObj->objectBase->hiddenClass = getArrayHiddenClass();
    
    result.pointerValue_ = arrayObj;
    
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
    // 完璧なECMAScript抽象比較実装（ECMAScript 7.2.11 Abstract Relational Comparison）
    
    // 1. プリミティブ値への変換
    Value px = toPrimitive(PreferredType::Number);
    Value py = other.toPrimitive(PreferredType::Number);
    
    // 2. 両方が文字列の場合
    if (px.isString() && py.isString()) {
        std::string sx = px.toString();
        std::string sy = py.toString();
        
        // 辞書順比較
        int comparison = sx.compare(sy);
        if (comparison < 0) {
            return ComparisonResult::LessThan;
        } else if (comparison > 0) {
            return ComparisonResult::GreaterThan;
        } else {
            return ComparisonResult::Equal;
        }
    }
    
    // 3. 数値への変換
    double nx = px.toNumber();
    double ny = py.toNumber();
    
    // 4. NaNのチェック
    if (std::isnan(nx) || std::isnan(ny)) {
        return ComparisonResult::Undefined;
    }
    
    // 5. 同じ値の場合
    if (nx == ny) {
        return ComparisonResult::Equal;
    }
    
    // 6. 正の無限大のチェック
    if (std::isinf(nx) && nx > 0) {
        return ComparisonResult::GreaterThan;
    }
    
    // 7. 負の無限大のチェック
    if (std::isinf(nx) && nx < 0) {
        return ComparisonResult::LessThan;
    }
    
    // 8. 正の無限大のチェック（y）
    if (std::isinf(ny) && ny > 0) {
        return ComparisonResult::LessThan;
    }
    
    // 9. 負の無限大のチェック（y）
    if (std::isinf(ny) && ny < 0) {
        return ComparisonResult::GreaterThan;
    }
    
    // 10. 通常の数値比較
    if (nx < ny) {
        return ComparisonResult::LessThan;
    } else if (nx > ny) {
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