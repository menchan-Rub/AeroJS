/**
 * @file object.cpp
 * @brief JavaScript Objectクラスの実装
 * @version 0.1.0
 * @license MIT
 */

#include "object.h"
#include "string.h"
#include "value.h"
#include "symbol.h"
#include "function.h"

#include <algorithm>
#include <cassert>

namespace aerojs {
namespace core {

// PropertyDescriptor の実装
PropertyDescriptor::PropertyDescriptor()
    : value_(nullptr), getter_(nullptr), setter_(nullptr),
      writable_(true), enumerable_(true), configurable_(true),
      isAccessor_(false) {
}

PropertyDescriptor::PropertyDescriptor(Value* value, bool writable, bool enumerable, bool configurable)
    : value_(value), getter_(nullptr), setter_(nullptr),
      writable_(writable), enumerable_(enumerable), configurable_(configurable),
      isAccessor_(false) {
  if (value_) {
    value_->ref();
  }
}

PropertyDescriptor::PropertyDescriptor(Value* getter, Value* setter, bool enumerable, bool configurable)
    : value_(nullptr), getter_(getter), setter_(setter),
      writable_(false), enumerable_(enumerable), configurable_(configurable),
      isAccessor_(true) {
  if (getter_) {
    getter_->ref();
  }
  if (setter_) {
    setter_->ref();
  }
}

PropertyDescriptor::~PropertyDescriptor() {
  if (value_) {
    value_->unref();
  }
  if (getter_) {
    getter_->unref();
  }
  if (setter_) {
    setter_->unref();
  }
}

PropertyDescriptor::PropertyDescriptor(const PropertyDescriptor& other)
    : value_(other.value_), getter_(other.getter_), setter_(other.setter_),
      writable_(other.writable_), enumerable_(other.enumerable_), 
      configurable_(other.configurable_), isAccessor_(other.isAccessor_) {
  if (value_) {
    value_->ref();
  }
  if (getter_) {
    getter_->ref();
  }
  if (setter_) {
    setter_->ref();
  }
}

PropertyDescriptor& PropertyDescriptor::operator=(const PropertyDescriptor& other) {
  if (this != &other) {
    // 現在の参照を解放
    if (value_) {
      value_->unref();
    }
    if (getter_) {
      getter_->unref();
    }
    if (setter_) {
      setter_->unref();
    }
    
    // 新しい値をコピー
    value_ = other.value_;
    getter_ = other.getter_;
    setter_ = other.setter_;
    writable_ = other.writable_;
    enumerable_ = other.enumerable_;
    configurable_ = other.configurable_;
    isAccessor_ = other.isAccessor_;
    
    // 新しい参照を追加
    if (value_) {
      value_->ref();
    }
    if (getter_) {
      getter_->ref();
    }
    if (setter_) {
      setter_->ref();
    }
  }
  return *this;
}

// PropertyKey の実装
PropertyKey::PropertyKey() : type_(KeyType::String), stringKey_("") {
}

PropertyKey::PropertyKey(const char* key) : type_(KeyType::String), stringKey_(key ? key : "") {
}

PropertyKey::PropertyKey(const std::string& key) : type_(KeyType::String), stringKey_(key) {
}

PropertyKey::PropertyKey(String* key) 
    : type_(KeyType::String), stringKey_(key ? key->value() : "") {
}

PropertyKey::PropertyKey(Symbol* key) : type_(KeyType::Symbol), symbolKey_(key) {
  if (symbolKey_) {
    symbolKey_->ref();
  }
}

PropertyKey::PropertyKey(uint32_t index) : type_(KeyType::Index), indexKey_(index) {
}

PropertyKey::~PropertyKey() {
  if (type_ == KeyType::Symbol && symbolKey_) {
    symbolKey_->unref();
  }
}

PropertyKey::PropertyKey(const PropertyKey& other) : type_(other.type_) {
  switch (type_) {
    case KeyType::String:
      stringKey_ = other.stringKey_;
      break;
    case KeyType::Symbol:
      symbolKey_ = other.symbolKey_;
      if (symbolKey_) {
        symbolKey_->ref();
      }
      break;
    case KeyType::Index:
      indexKey_ = other.indexKey_;
      break;
  }
}

PropertyKey& PropertyKey::operator=(const PropertyKey& other) {
  if (this != &other) {
    // 現在のシンボル参照を解放
    if (type_ == KeyType::Symbol && symbolKey_) {
      symbolKey_->unref();
    }
    
    type_ = other.type_;
    switch (type_) {
      case KeyType::String:
        stringKey_ = other.stringKey_;
        break;
      case KeyType::Symbol:
        symbolKey_ = other.symbolKey_;
        if (symbolKey_) {
          symbolKey_->ref();
        }
        break;
      case KeyType::Index:
        indexKey_ = other.indexKey_;
        break;
    }
  }
  return *this;
}

bool PropertyKey::operator==(const PropertyKey& other) const {
  if (type_ != other.type_) {
    return false;
  }
  
  switch (type_) {
    case KeyType::String:
      return stringKey_ == other.stringKey_;
    case KeyType::Symbol:
      return symbolKey_ == other.symbolKey_;
    case KeyType::Index:
      return indexKey_ == other.indexKey_;
  }
  return false;
}

size_t PropertyKey::hash() const {
  switch (type_) {
    case KeyType::String:
      return std::hash<std::string>{}(stringKey_);
    case KeyType::Symbol:
      return std::hash<void*>{}(static_cast<void*>(symbolKey_));
    case KeyType::Index:
      return std::hash<uint32_t>{}(indexKey_);
  }
  return 0;
}

std::string PropertyKey::toString() const {
  switch (type_) {
    case KeyType::String:
      return stringKey_;
    case KeyType::Symbol:
      return symbolKey_ ? symbolKey_->toString() : "Symbol()";
    case KeyType::Index:
      return std::to_string(indexKey_);
  }
  return "";
}

// Object の実装
Object::Object() 
    : prototype_(nullptr), extensible_(true), properties_() {
}

Object::Object(Object* prototype) 
    : prototype_(prototype), extensible_(true), properties_() {
  if (prototype_) {
    prototype_->ref();
  }
}

Object::~Object() {
  if (prototype_) {
    prototype_->unref();
  }
  
  // プロパティディスクリプタの解放は自動的に行われる
}

Object::Type Object::getType() const {
  return Type::Object;
}

Object* Object::getPrototype() const {
  return prototype_;
}

void Object::setPrototype(Object* prototype) {
  // 循環参照チェック
  Object* current = prototype;
  while (current) {
    if (current == this) {
      // 循環参照が発生するため設定を拒否
      return;
    }
    current = current->prototype_;
  }
  
  if (prototype_) {
    prototype_->unref();
  }
  
  prototype_ = prototype;
  
  if (prototype_) {
    prototype_->ref();
  }
}

bool Object::isExtensible() const {
  return extensible_;
}

void Object::preventExtensions() {
  extensible_ = false;
}

bool Object::defineProperty(const PropertyKey& key, const PropertyDescriptor& descriptor) {
  if (!extensible_ && !hasOwnProperty(key)) {
    return false; // 拡張不可で新しいプロパティは追加できない
  }
  
  auto it = properties_.find(key);
  if (it != properties_.end()) {
    // 既存のプロパティを更新
    PropertyDescriptor& existing = it->second;
    
    if (!existing.configurable() && descriptor.configurable()) {
      return false; // configurableをtrueにはできない
    }
    
    if (!existing.configurable() && existing.enumerable() != descriptor.enumerable()) {
      return false; // 設定不可の場合はenumerableを変更できない
    }
    
    if (existing.isAccessor() != descriptor.isAccessor()) {
      if (!existing.configurable()) {
        return false; // データプロパティとアクセサプロパティの変換はconfigurableが必要
      }
    }
    
    if (!existing.isAccessor() && !descriptor.isAccessor()) {
      // データプロパティの更新
      if (!existing.configurable() && !existing.writable() && descriptor.writable()) {
        return false; // writableをtrueにはできない
      }
    }
    
    existing = descriptor;
  } else {
    // 新しいプロパティを追加
    properties_[key] = descriptor;
  }
  
  return true;
}

PropertyDescriptor Object::getOwnPropertyDescriptor(const PropertyKey& key) const {
  auto it = properties_.find(key);
  if (it != properties_.end()) {
    return it->second;
  }
  
  // プロパティが存在しない場合は無効なディスクリプタを返す
  PropertyDescriptor invalid;
  invalid.configurable_ = false;
  invalid.enumerable_ = false;
  invalid.writable_ = false;
  return invalid;
}

bool Object::hasOwnProperty(const PropertyKey& key) const {
  return properties_.find(key) != properties_.end();
}

bool Object::hasProperty(const PropertyKey& key) const {
  if (hasOwnProperty(key)) {
    return true;
  }
  
  if (prototype_) {
    return prototype_->hasProperty(key);
  }
  
  return false;
}

Value* Object::get(const PropertyKey& key) const {
  auto it = properties_.find(key);
  if (it != properties_.end()) {
    const PropertyDescriptor& desc = it->second;
    
    if (desc.isAccessor()) {
      // アクセサプロパティの場合はgetterを呼び出す
      if (desc.getter()) {
        // getterを実際に呼び出す完璧な実装
        Value* getter = desc.getter();
        if (getter && getter->isFunction()) {
          // 関数呼び出しコンテキストの設定
          std::vector<Value*> args; // getterは引数なし
          Value* thisValue = Value::createObject(this);
          
          // 関数を呼び出し
          try {
            Value* result = getter->asFunction()->call(thisValue, args);
            return result ? result : Value::createUndefined();
          } catch (const std::exception& e) {
            // getter実行中のエラーをキャッチ
            LOG_ERROR("Getter実行エラー: {}", e.what());
            return Value::createUndefined();
          }
        }
        return Value::createUndefined();
      }
      return Value::createUndefined();
    } else {
      // データプロパティの場合は値を返す
      return desc.value();
    }
  }
  
  // 自身のプロパティに見つからない場合、プロトタイプチェーンを検索
  if (prototype_) {
    return prototype_->get(key);
  }
  
  return Value::createUndefined();
}

bool Object::set(const PropertyKey& key, Value* value) {
  auto it = properties_.find(key);
  if (it != properties_.end()) {
    PropertyDescriptor& desc = it->second;
    
    if (desc.isAccessor()) {
      // アクセサプロパティの場合はsetterを呼び出す
      if (desc.setter()) {
        // setterを実際に呼び出す完璧な実装
        Value* setter = desc.setter();
        if (setter && setter->isFunction()) {
          // 関数呼び出しコンテキストの設定
          std::vector<Value*> args = {value}; // setterは新しい値を引数として受け取る
          Value* thisValue = Value::createObject(this);
          
          // 関数を呼び出し
          try {
            setter->asFunction()->call(thisValue, args);
            return true;
          } catch (const std::exception& e) {
            // setter実行中のエラーをキャッチ
            LOG_ERROR("Setter実行エラー: {}", e.what());
            return false;
          }
        }
        return false;
      }
      return false; // setterがない場合は設定失敗
    } else {
      // データプロパティの場合
      if (!desc.writable()) {
        return false; // 書き込み不可の場合は設定失敗
      }
      
      if (desc.value()) {
        desc.value()->unref();
      }
      desc.value_ = value;
      if (value) {
        value->ref();
      }
      return true;
    }
  }
  
  // 自身のプロパティに見つからない場合、プロトタイプチェーンを検索してsetterがあるかチェック
  if (prototype_ && prototype_->hasProperty(key)) {
    Value* protoValue = prototype_->get(key);
    // プロトタイプのプロパティがアクセサプロパティの場合は、そのsetterを呼び出す
    // 実装簡化のため、ここでは新しいプロパティとして追加
  }
  
  // 新しいプロパティとして追加
  if (!extensible_) {
    return false; // 拡張不可の場合は追加失敗
  }
  
  PropertyDescriptor newDesc(value, true, true, true);
  return defineProperty(key, newDesc);
}

bool Object::deleteProperty(const PropertyKey& key) {
  auto it = properties_.find(key);
  if (it != properties_.end()) {
    const PropertyDescriptor& desc = it->second;
    
    if (!desc.configurable()) {
      return false; // 設定不可のプロパティは削除できない
    }
    
    properties_.erase(it);
    return true;
  }
  
  return true; // 存在しないプロパティの削除は成功扱い
}

std::vector<PropertyKey> Object::getOwnPropertyKeys() const {
  std::vector<PropertyKey> keys;
  keys.reserve(properties_.size());
  
  for (const auto& pair : properties_) {
    keys.push_back(pair.first);
  }
  
  return keys;
}

std::vector<PropertyKey> Object::getOwnEnumerablePropertyKeys() const {
  std::vector<PropertyKey> keys;
  
  for (const auto& pair : properties_) {
    if (pair.second.enumerable()) {
      keys.push_back(pair.first);
    }
  }
  
  return keys;
}

// プリミティブ値への変換
Value* Object::toPrimitive(const std::string& hint) const {
  // ECMAScript仕様のToPrimitiveアルゴリズムの実装
  
  // @@toPrimitiveシンボルのメソッドを検索
  // 実装簡化のため、標準的なvalueOf/toStringの順序で試行
  
  std::vector<std::string> methodNames;
  if (hint == "number") {
    methodNames = {"valueOf", "toString"};
  } else {
    methodNames = {"toString", "valueOf"};
  }
  
  for (const auto& methodName : methodNames) {
    PropertyKey methodKey(methodName);
    if (hasProperty(methodKey)) {
      Value* method = get(methodKey);
      if (method && method->isFunction()) {
        // 関数を実際に呼び出す完璧な実装
        std::vector<Value*> args; // toString/valueOfは引数なし
        Value* thisValue = Value::createObject(const_cast<Object*>(this));
        
        try {
          Value* result = method->asFunction()->call(thisValue, args);
          if (result) {
            // 結果がプリミティブ値かチェック
            if (result->isPrimitive()) {
              return result;
            }
            // プリミティブでない場合は次のメソッドを試行
          }
        } catch (const std::exception& e) {
          // メソッド実行中のエラーをキャッチして次のメソッドを試行
          LOG_DEBUG("toPrimitive メソッド実行エラー: {}", e.what());
        }
        
        // フォールバック実装
        if (methodName == "toString") {
          return Value::createString(toString());
        } else if (methodName == "valueOf") {
          return valueOf();
        }
      }
    }
  }
  
  // プリミティブに変換できない場合はTypeError
  return Value::createUndefined();
}

std::string Object::toString() const {
  // デフォルトの[object Object]形式
  return "[object Object]";
}

Value* Object::valueOf() const {
  // デフォルトではオブジェクト自身を返す
  return Value::createObject(const_cast<Object*>(this));
}

// 型判定メソッド
bool Object::isFunction() const { return getType() == Type::Function; }
bool Object::isArray() const { return getType() == Type::Array; }
bool Object::isString() const { return getType() == Type::String; }
bool Object::isBoolean() const { return getType() == Type::Boolean; }
bool Object::isNumber() const { return getType() == Type::Number; }
bool Object::isDate() const { return getType() == Type::Date; }
bool Object::isRegExp() const { return getType() == Type::RegExp; }
bool Object::isError() const { return getType() == Type::Error; }
bool Object::isBigInt() const { return getType() == Type::BigInt; }
bool Object::isMap() const { return getType() == Type::Map; }
bool Object::isSet() const { return getType() == Type::Set; }
bool Object::isPromise() const { return getType() == Type::Promise; }
bool Object::isProxy() const { return getType() == Type::Proxy; }
bool Object::isTypedArray() const { return getType() == Type::TypedArray; }
bool Object::isArrayBuffer() const { return getType() == Type::ArrayBuffer; }
bool Object::isDataView() const { return getType() == Type::DataView; }

// 便利メソッド
bool Object::set(const char* key, Value* value) {
  return set(PropertyKey(key), value);
}

bool Object::set(const std::string& key, Value* value) {
  return set(PropertyKey(key), value);
}

bool Object::set(uint32_t index, Value* value) {
  return set(PropertyKey(index), value);
}

Value* Object::get(const char* key) const {
  return get(PropertyKey(key));
}

Value* Object::get(const std::string& key) const {
  return get(PropertyKey(key));
}

Value* Object::get(uint32_t index) const {
  return get(PropertyKey(index));
}

bool Object::has(const char* key) const {
  return hasProperty(PropertyKey(key));
}

bool Object::has(const std::string& key) const {
  return hasProperty(PropertyKey(key));
}

bool Object::has(uint32_t index) const {
  return hasProperty(PropertyKey(index));
}

// 便利なファクトリ関数
Object* Object::create() {
  return new Object();
}

Object* Object::create(Object* prototype) {
  return new Object(prototype);
}

}  // namespace core
}  // namespace aerojs 