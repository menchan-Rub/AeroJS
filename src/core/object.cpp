/**
 * @file object.cpp
 * @brief AeroJS JavaScript エンジンのオブジェクトクラスの実装
 * @version 0.1.0
 * @license MIT
 */

#include "object.h"
#include "context.h"
#include "value.h"
#include "symbol.h"
#include <algorithm>
#include <sstream>
#include <limits>
#include <functional>

namespace aerojs {
namespace core {

// PropertyKeyのメソッド実装
bool PropertyKey::operator==(const PropertyKey& other) const {
    if (m_type != other.m_type) {
        return false;
    }
    
    switch (m_type) {
        case String:
            return m_stringKey == other.m_stringKey;
        case Symbol:
            return m_symbolPtr == other.m_symbolPtr;
        case Integer:
            return m_intKey == other.m_intKey;
        default:
            return false;
    }
}

size_t PropertyKey::hash() const {
    switch (m_type) {
        case String:
            return std::hash<std::string>{}(m_stringKey);
        case Symbol:
            return std::hash<void*>{}(static_cast<void*>(m_symbolPtr));
        case Integer:
            return std::hash<uint32_t>{}(m_intKey);
        default:
            return 0;
    }
}

std::string PropertyKey::toString() const {
    switch (m_type) {
        case String:
            return m_stringKey;
        case Symbol:
            return m_symbolPtr ? m_symbolPtr->toString() : "Symbol()";
        case Integer:
            return std::to_string(m_intKey);
        default:
            return "";
    }
}

// Object クラスの実装
Object::Object(Context* ctx)
    : m_context(ctx)
    , m_prototype(nullptr)
    , m_properties([](const PropertyKey& key) { return key.hash(); },
                  [](const PropertyKey& a, const PropertyKey& b) { return a == b; })
    , m_objectClass(ObjectClass::Base)
    , m_extensible(true)
    , m_sealed(false)
    , m_frozen(false)
    , m_internalSlots()
    , m_customData()
    , m_refCount(1)
{
    // オブジェクト作成時のメモリ使用量を追跡
    if (m_context && m_context->getMemoryTracker()) {
        m_context->getMemoryTracker()->trackObjectAllocation(this, sizeof(Object));
    }
}

Object::~Object() {
    // カスタムデータは自動的にクリーンアップされます
    // m_customDataのデストラクタがCustomDataEntryのデストラクタを呼び出します
    
    // プロパティの値を解放
    for (auto& pair : m_properties) {
        PropertyDescriptor& desc = pair.second;
        if (desc.value) {
            desc.value->release();
        }
        if (desc.getter) {
            desc.getter->release();
        }
        if (desc.setter) {
            desc.setter->release();
        }
    }
    
    // 内部スロットの値を解放
    for (auto& pair : m_internalSlots) {
        if (pair.second) {
            pair.second->release();
        }
    }
    
    // オブジェクト破棄時のメモリ追跡
    if (m_context && m_context->getMemoryTracker()) {
        m_context->getMemoryTracker()->trackObjectDeallocation(this, sizeof(Object));
    }
}

void Object::setPrototype(Object* proto) {
    // 循環参照の検出
    Object* current = proto;
    while (current) {
        if (current == this) {
            // プロトタイプチェーンに循環参照が発生するため、設定を拒否
            m_context->throwTypeError("Cyclic prototype chain detected");
            return;
        }
        current = current->m_prototype;
    }
    
    m_prototype = proto;
}

bool Object::setProperty(const PropertyKey& key, Value* value, uint32_t attributes) {
    if (!value) {
        return false;
    }
    
    PropertyDescriptor desc;
    desc.value = value;
    desc.attributes = attributes;
    
    return defineProperty(key, desc);
}

bool Object::setProperty(const std::string& key, Value* value, uint32_t attributes) {
    return setProperty(PropertyKey(key), value, attributes);
}

bool Object::setProperty(uint32_t index, Value* value, uint32_t attributes) {
    return setProperty(PropertyKey(index), value, attributes);
}

bool Object::setProperty(Symbol* symbol, Value* value, uint32_t attributes) {
    if (!symbol) {
        return false;
    }
    return setProperty(PropertyKey(symbol), value, attributes);
}

Value* Object::getProperty(const PropertyKey& key) const {
    // まず自身のプロパティを探す
    auto it = m_properties.find(key);
    if (it != m_properties.end()) {
        const PropertyDescriptor& desc = it->second;
        
        if (desc.isAccessor()) {
            // アクセサプロパティの場合はgetterを呼び出す
            if (desc.get) {
                // getterを呼び出す
                Value* thisValue = const_cast<Object*>(this)->toValue();
                std::vector<Value*> args;
                return m_context->callFunction(desc.get, thisValue, args);
            }
            return m_context->getUndefinedValue();
        }
        
        return desc.value;
    }
    
    // 自身のプロパティになければプロトタイプチェーンを探索
    if (m_prototype) {
        return m_prototype->getProperty(key);
    }
    
    return m_context->getUndefinedValue();
}

Value* Object::getProperty(const std::string& key) const {
    return getProperty(PropertyKey(key));
}

Value* Object::getProperty(uint32_t index) const {
    return getProperty(PropertyKey(index));
}

Value* Object::getProperty(Symbol* symbol) const {
    if (!symbol) {
        return m_context->getUndefinedValue();
    }
    return getProperty(PropertyKey(symbol));
}

bool Object::deleteProperty(const PropertyKey& key) {
    if (!m_extensible) {
        return false;
    }

    auto it = m_properties.find(key);
    if (it != m_properties.end()) {
        const PropertyDescriptor& desc = it->second;
        
        // 設定変更不可の場合は削除できない
        if (!desc.isConfigurable()) {
            return false;
        }
        
        m_properties.erase(it);
        return true;
    }
    
    // プロパティが存在しなかった場合も成功扱い
    return true;
}

bool Object::hasProperty(const PropertyKey& key) const {
    // 自身のプロパティを確認
    if (hasOwnProperty(key)) {
        return true;
    }
    
    // プロトタイプチェーンを探索
    if (m_prototype) {
        return m_prototype->hasProperty(key);
    }
    
    return false;
}

bool Object::hasOwnProperty(const PropertyKey& key) const {
    return m_properties.find(key) != m_properties.end();
}

PropertyDescriptor Object::getOwnPropertyDescriptor(const PropertyKey& key) const {
    auto it = m_properties.find(key);
    if (it != m_properties.end()) {
        return it->second;
    }
    
    // プロパティが存在しない場合は、デフォルトの空ディスクリプタを返す
    PropertyDescriptor emptyDesc;
    emptyDesc.attributes = PropertyAttribute::None;
    return emptyDesc;
}

bool Object::defineProperty(const PropertyKey& key, const PropertyDescriptor& desc) {
    // オブジェクトが拡張不可の場合、新しいプロパティは追加できない
    if (!m_extensible && m_properties.find(key) == m_properties.end()) {
        return false;
    }

    auto it = m_properties.find(key);
    
    // プロパティが存在しない場合、または設定変更可能な場合
    if (it == m_properties.end()) {
        m_properties[key] = desc;
        return true;
    }
    
    // 既存のプロパティが設定変更不可の場合は、同一の設定のみ許可
    const PropertyDescriptor& existing = it->second;
    
    // 設定変更不可の場合
    if (!existing.isConfigurable()) {
        // 属性が変更されていないか確認
        if ((desc.hasEnumerable() && desc.isEnumerable() != existing.isEnumerable()) ||
            (desc.hasConfigurable() && desc.isConfigurable() != existing.isConfigurable())) {
            return false;
        }
        
        // アクセサからデータプロパティ、またはその逆の変更は禁止
        if (existing.isAccessor() && desc.hasValue()) {
            return false;
        }
        
        if (!existing.isAccessor() && (desc.hasGet() || desc.hasSet())) {
            return false;
        }
        
        if (existing.isAccessor()) {
            // アクセサプロパティの場合
            if ((desc.hasGet() && desc.get != existing.get) ||
                (desc.hasSet() && desc.set != existing.set)) {
                return false;
            }
        } else {
            // データプロパティの場合
            if (!existing.isWritable()) {
                if (desc.hasValue() && !m_context->strictEquals(desc.value, existing.value)) {
                    return false;
                }
                
                if (desc.hasWritable() && desc.isWritable()) {
                    return false;
                }
            }
        }
    }
    
    // 変更を適用（既存の属性を維持しながら値のみ更新）
    PropertyDescriptor updatedDesc = existing;
    
    if (desc.hasValue()) {
        updatedDesc.value = desc.value;
    }
    
    if (desc.hasGet()) {
        updatedDesc.get = desc.get;
    }
    
    if (desc.hasSet()) {
        updatedDesc.set = desc.set;
    }
    
    // 属性フラグの更新
    if (desc.hasWritable()) {
        if (desc.isWritable()) {
            updatedDesc.attributes |= PropertyAttribute::Writable;
        } else {
            updatedDesc.attributes &= ~PropertyAttribute::Writable;
        }
    }
    
    if (desc.hasEnumerable()) {
        if (desc.isEnumerable()) {
            updatedDesc.attributes |= PropertyAttribute::Enumerable;
        } else {
            updatedDesc.attributes &= ~PropertyAttribute::Enumerable;
        }
    }
    
    if (desc.hasConfigurable()) {
        if (desc.isConfigurable()) {
            updatedDesc.attributes |= PropertyAttribute::Configurable;
        } else {
            updatedDesc.attributes &= ~PropertyAttribute::Configurable;
        }
    }
    
    m_properties[key] = updatedDesc;
    return true;
}

std::vector<PropertyKey> Object::getOwnPropertyKeys(bool includeNonEnumerable, bool includeSymbols) const {
    std::vector<PropertyKey> keys;
    std::vector<PropertyKey> stringKeys;
    std::vector<PropertyKey> integerKeys;
    std::vector<PropertyKey> symbolKeys;
    
    for (const auto& entry : m_properties) {
        if (includeNonEnumerable || entry.second.isEnumerable()) {
            const PropertyKey& key = entry.first;
            
            if (key.isInteger()) {
                integerKeys.push_back(key);
            } else if (key.isString()) {
                stringKeys.push_back(key);
            } else if (includeSymbols && key.isSymbol()) {
                symbolKeys.push_back(key);
            }
        }
    }
    
    // インデックスプロパティを数値順でソート
    std::sort(integerKeys.begin(), integerKeys.end(), [](const PropertyKey& a, const PropertyKey& b) {
        return a.asInteger() < b.asInteger();
    });
    
    // 文字列プロパティを辞書順でソート
    std::sort(stringKeys.begin(), stringKeys.end(), [](const PropertyKey& a, const PropertyKey& b) {
        return a.asString() < b.asString();
    });
    
    // 結果を連結: 整数キー → 文字列キー → シンボルキー
    keys.insert(keys.end(), integerKeys.begin(), integerKeys.end());
    keys.insert(keys.end(), stringKeys.begin(), stringKeys.end());
    keys.insert(keys.end(), symbolKeys.begin(), symbolKeys.end());
    
    return keys;
}

std::vector<PropertyKey> Object::getEnumerablePropertyKeys() const {
    return getOwnPropertyKeys(false, false);
}

std::vector<PropertyKey> Object::getOwnPropertyNames() const {
    return getOwnPropertyKeys(true, false);
}

std::vector<PropertyKey> Object::getOwnPropertySymbols() const {
    std::vector<PropertyKey> keys;
    
    for (const auto& entry : m_properties) {
        const PropertyKey& key = entry.first;
        if (key.isSymbol()) {
            keys.push_back(key);
        }
    }
    
    return keys;
}

void Object::preventExtensions() {
    m_extensible = false;
}

bool Object::isExtensible() const {
    return m_extensible;
}

void Object::seal() {
    // すべてのプロパティを設定変更不可にする
    for (auto& entry : m_properties) {
        PropertyDescriptor& desc = entry.second;
        desc.attributes &= ~PropertyAttribute::Configurable;
    }
    
    // オブジェクトを拡張不可にする
    preventExtensions();
}

void Object::freeze() {
    // すべてのデータプロパティを書き込み不可かつ設定変更不可にする
    for (auto& entry : m_properties) {
        PropertyDescriptor& desc = entry.second;
        if (!desc.isAccessor()) {
            desc.attributes &= ~PropertyAttribute::Writable;
        }
        desc.attributes &= ~PropertyAttribute::Configurable;
    }
    
    // オブジェクトを拡張不可にする
    preventExtensions();
}

bool Object::isSealed() const {
    // 拡張可能な場合はシールされていない
    if (m_extensible) {
        return false;
    }
    
    // すべてのプロパティが設定変更不可かチェック
    for (const auto& entry : m_properties) {
        if (entry.second.isConfigurable()) {
            return false;
        }
    }
    
    return true;
}

bool Object::isFrozen() const {
    // 拡張可能な場合は凍結されていない
    if (m_extensible) {
        return false;
    }
    
    // すべてのプロパティが設定変更不可かつデータプロパティが書き込み不可かチェック
    for (const auto& entry : m_properties) {
        const PropertyDescriptor& desc = entry.second;
        
        // 設定変更可能なプロパティがあれば凍結されていない
        if (desc.isConfigurable()) {
            return false;
        }
        
        // データプロパティで書き込み可能なものがあれば凍結されていない
        if (!desc.isAccessor() && desc.isWritable()) {
            return false;
        }
    }
    
    return true;
}

std::string Object::toString() const {
    // ECMAScript仕様に準拠した[object Object]形式の文字列を返す
    return "[object Object]";
}

double Object::toNumber() const {
    // オブジェクトを数値に変換する際のデフォルト動作はNaNを返す
    return std::numeric_limits<double>::quiet_NaN();
}

Value* Object::toPrimitive(PrimitiveHint hint) const {
    // ECMAScript仕様に基づくtoPrimitiveアルゴリズムの実装
    const std::vector<std::string> methods = 
        (hint == PrimitiveHint::String) ? 
            std::vector<std::string>{"toString", "valueOf"} : 
            std::vector<std::string>{"valueOf", "toString"};
    
    for (const auto& methodName : methods) {
        Value* method = getProperty(methodName);
        if (method && method->isCallable()) {
            Value* thisValue = const_cast<Object*>(this)->toValue();
            std::vector<Value*> args;
            Value* result = m_context->callFunction(method, thisValue, args);
            
            if (result && !result->isObject()) {
                return result;
            }
        }
    }
    
    // プリミティブ値に変換できない場合はTypeErrorをスロー
    m_context->throwTypeError("Cannot convert object to primitive value");
    return m_context->getUndefinedValue();
}

Value* Object::defaultValue(PrimitiveHint hint) const {
    // デフォルト値の取得はtoPrimitiveに委譲
    return toPrimitive(hint);
}

void Object::setObjectClass(ObjectClass objectClass) {
    m_objectClass = objectClass;
}

ObjectClass Object::getObjectClass() const {
    return m_objectClass;
}

Value* Object::toValue() const {
    // このオブジェクトをValueにラップして返す
    if (!m_context) {
        // コンテキストが存在しない場合は安全に処理
        return nullptr;
    }
    
    // オブジェクトの状態を検証
    if (isDestroyed()) {
        // 破棄されたオブジェクトの場合はundefinedを返す
        return m_context->getUndefinedValue();
    }
    
    // パフォーマンス最適化のためにキャッシュされた値があれば使用
    if (m_cachedValue && m_cachedValue->isValid()) {
        return m_cachedValue;
    }
    
    // オブジェクトをValueにラップ
    Value* result = m_context->createObjectValue(const_cast<Object*>(this));
    
    // 循環参照を避けるため、弱参照としてキャッシュを更新
    const_cast<Object*>(this)->m_cachedValue = result;
    
    return result;
}

void Object::mark() {
    // 既にマークされている場合は処理を終了（循環参照対策）
    if (isMarked()) {
        return;
    }
    
    // このオブジェクトをマークする
    RefCounted::mark();
    
    // プロトタイプをマーク
    if (m_prototype) {
        m_prototype->mark();
    }
    
    // すべてのプロパティ値をマーク
    for (auto& entry : m_properties) {
        PropertyDescriptor& desc = entry.second;
        
        if (desc.value) {
            desc.value->mark();
        }
        
        if (desc.get) {
            desc.get->mark();
        }
        
        if (desc.set) {
            desc.set->mark();
        }
    }
    
    // カスタムデータをマーク
    for (auto& entry : m_customData) {
        if (entry.second && entry.second->markCallback) {
            entry.second->markCallback(entry.second->data);
        }
    }
}

bool Object::setCustomData(const std::string& key, void* data, CustomDataFinalizer finalizer, CustomDataMarker marker) {
    auto entry = std::make_unique<CustomDataEntry>();
    entry->data = data;
    entry->finalizerCallback = finalizer;
    entry->markCallback = marker;
    
    m_customData[key] = std::move(entry);
    return true;
}

void* Object::getCustomData(const std::string& key) const {
    auto it = m_customData.find(key);
    if (it != m_customData.end() && it->second) {
        return it->second->data;
    }
    return nullptr;
}

bool Object::removeCustomData(const std::string& key) {
    auto it = m_customData.find(key);
    if (it != m_customData.end()) {
        m_customData.erase(it);
        return true;
    }
    return false;
}

} // namespace core
} // namespace aerojs