/**
 * @file map.cpp
 * @brief JavaScript のMap組み込みオブジェクトの実装
 * @copyright 2023 AeroJS Project
 */

#include "map.h"
#include "core/runtime/array.h"
#include "core/runtime/builtin_function.h"
#include "core/runtime/error.h"
#include "core/runtime/context.h"
#include "core/runtime/iterator_helper.h"
#include <algorithm>

namespace aero {

// ValueHasherの実装
size_t ValueHasher::operator()(const Value& key) const {
    if (key.isNumber()) {
        // 数値のハッシュ
        return std::hash<double>()(key.asNumber());
    } else if (key.isString()) {
        // 文字列のハッシュ
        return std::hash<std::string>()(key.asString()->string());
    } else if (key.isBoolean()) {
        // ブールのハッシュ
        return std::hash<bool>()(key.asBoolean());
    } else if (key.isNull()) {
        // nullのハッシュ
        return std::hash<std::nullptr_t>()(nullptr);
    } else if (key.isUndefined()) {
        // undefinedのハッシュ
        return std::hash<int>()(1);
    } else if (key.isSymbol()) {
        // シンボルのハッシュ
        return std::hash<int>()(key.asSymbol()->id());
    } else if (key.isObject()) {
        // オブジェクトのハッシュ - ポインタの値を使用
        return std::hash<void*>()(key.asObject());
    }
    
    // それ以外のケース（ほぼ発生しない）
    return 0;
}

// MapObjectのコンストラクタ
MapObject::MapObject(Object* prototype)
    : Object(prototype)
    , m_entries()
    , m_keyMap()
    , m_size(0) 
{
    // 空のMapを初期化
}

// イテレーブルからMapObjectを構築
MapObject::MapObject(Object* prototype, Value iterable)
    : Object(prototype)
    , m_entries()
    , m_keyMap()
    , m_size(0)
{
    if (iterable.isNullOrUndefined()) {
        // イテレーブルがnullまたはundefinedの場合は空のMapを作成
        return;
    }
    
    // イテレーブルからエントリを追加
    auto iterator = IteratorHelper::getIterator(iterable, Context::current());
    if (!iterator.isObject()) {
        Error::throwError(Error::TypeError, "イテレーブルオブジェクトが必要です");
        return;
    }
    
    Value iteratorResult;
    while (!(iteratorResult = IteratorHelper::next(iterator)).isFalse()) {
        Value entry = iteratorResult;
        
        // エントリは[key, value]の形式である必要がある
        if (!entry.isObject() || !entry.asObject()->has("0") || !entry.asObject()->has("1")) {
            Error::throwError(Error::TypeError, "イテレーブルのエントリは[key, value]の形式である必要があります");
            return;
        }
        
        Value key = entry.asObject()->get("0");
        Value value = entry.asObject()->get("1");
        
        // キーと値をMapに追加
        set(key, value);
    }
}

// デストラクタ
MapObject::~MapObject() {
    // 動的に確保したリソースがある場合はここで解放
    // このケースでは特に必要なし
}

// キーに値を設定
Value MapObject::set(Value key, Value value) {
    // キーが既に存在するか確認
    auto it = m_keyMap.find(key);
    if (it != m_keyMap.end()) {
        // 既存のキーの値を更新
        m_entries[it->second].second = value;
    } else {
        // 新しいキーを追加
        m_entries.emplace_back(key, value);
        m_keyMap[key] = m_entries.size() - 1;
        m_size++;
    }
    
    return Value(this); // メソッドチェーン用にthisを返す
}

// キーに関連付けられた値を取得
Value MapObject::get(Value key) const {
    auto it = m_keyMap.find(key);
    if (it != m_keyMap.end()) {
        return m_entries[it->second].second;
    }
    
    return Value::undefined(); // キーが存在しない場合はundefinedを返す
}

// キーが存在するか確認
bool MapObject::has(Value key) const {
    return m_keyMap.find(key) != m_keyMap.end();
}

// キーとその値を削除
bool MapObject::remove(Value key) {
    auto it = m_keyMap.find(key);
    if (it == m_keyMap.end()) {
        return false; // キーが存在しない
    }
    
    // インデックスを取得
    size_t index = it->second;
    
    // エントリを削除
    m_entries.erase(m_entries.begin() + index);
    m_keyMap.erase(it);
    m_size--;
    
    // 削除後のインデックスを更新
    for (auto& pair : m_keyMap) {
        if (pair.second > index) {
            pair.second--;
        }
    }
    
    return true;
}

// すべてのエントリをクリア
void MapObject::clear() {
    m_entries.clear();
    m_keyMap.clear();
    m_size = 0;
}

// すべてのキーを配列として取得
Value MapObject::keys() const {
    auto* array = new ArrayObject(Context::current()->arrayPrototype());
    
    for (size_t i = 0; i < m_entries.size(); i++) {
        array->push(m_entries[i].first);
    }
    
    return Value(array);
}

// すべての値を配列として取得
Value MapObject::values() const {
    auto* array = new ArrayObject(Context::current()->arrayPrototype());
    
    for (size_t i = 0; i < m_entries.size(); i++) {
        array->push(m_entries[i].second);
    }
    
    return Value(array);
}

// すべてのエントリを配列として取得
Value MapObject::entries() const {
    auto* array = new ArrayObject(Context::current()->arrayPrototype());
    
    for (size_t i = 0; i < m_entries.size(); i++) {
        auto* entryArray = new ArrayObject(Context::current()->arrayPrototype());
        entryArray->push(m_entries[i].first);
        entryArray->push(m_entries[i].second);
        array->push(Value(entryArray));
    }
    
    return Value(array);
}

// 各エントリに対してコールバックを実行
void MapObject::forEach(Value callback, Value thisArg) {
    if (!callback.isCallable()) {
        Error::throwError(Error::TypeError, "コールバックは関数である必要があります");
        return;
    }
    
    Context* context = Context::current();
    std::vector<Value> args(3);
    args[2] = Value(this); // 第3引数: マップ自体
    
    for (size_t i = 0; i < m_entries.size(); i++) {
        args[0] = m_entries[i].second; // 第1引数: 値
        args[1] = m_entries[i].first;  // 第2引数: キー
        callback.call(thisArg, args, context);
    }
}

// Map.prototype.setの実装
Value mapSet(Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // thisがMapオブジェクトかどうか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isMapObject()) {
        Error::throwError(Error::TypeError, "Map.prototype.set called on incompatible receiver");
        return Value::undefined();
    }
    
    auto* mapObj = static_cast<MapObject*>(thisValue.asObject());
    
    // 引数が少なくとも1つあることを確認
    if (arguments.size() < 1) {
        Error::throwError(Error::TypeError, "Map.prototype.set requires at least 1 argument");
        return Value::undefined();
    }
    
    Value key = arguments[0];
    Value value = arguments.size() > 1 ? arguments[1] : Value::undefined();
    
    return mapObj->set(key, value);
}

// Map.prototype.getの実装
Value mapGet(Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // thisがMapオブジェクトかどうか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isMapObject()) {
        Error::throwError(Error::TypeError, "Map.prototype.get called on incompatible receiver");
        return Value::undefined();
    }
    
    auto* mapObj = static_cast<MapObject*>(thisValue.asObject());
    
    // 引数が少なくとも1つあることを確認
    if (arguments.size() < 1) {
        return Value::undefined();
    }
    
    return mapObj->get(arguments[0]);
}

// Map.prototype.hasの実装
Value mapHas(Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // thisがMapオブジェクトかどうか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isMapObject()) {
        Error::throwError(Error::TypeError, "Map.prototype.has called on incompatible receiver");
        return Value::undefined();
    }
    
    auto* mapObj = static_cast<MapObject*>(thisValue.asObject());
    
    // 引数が少なくとも1つあることを確認
    if (arguments.size() < 1) {
        return Value(false);
    }
    
    return Value(mapObj->has(arguments[0]));
}

// Map.prototype.deleteの実装
Value mapDelete(Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // thisがMapオブジェクトかどうか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isMapObject()) {
        Error::throwError(Error::TypeError, "Map.prototype.delete called on incompatible receiver");
        return Value::undefined();
    }
    
    auto* mapObj = static_cast<MapObject*>(thisValue.asObject());
    
    // 引数が少なくとも1つあることを確認
    if (arguments.size() < 1) {
        return Value(false);
    }
    
    return Value(mapObj->remove(arguments[0]));
}

// Map.prototype.clearの実装
Value mapClear(Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // thisがMapオブジェクトかどうか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isMapObject()) {
        Error::throwError(Error::TypeError, "Map.prototype.clear called on incompatible receiver");
        return Value::undefined();
    }
    
    auto* mapObj = static_cast<MapObject*>(thisValue.asObject());
    mapObj->clear();
    
    return Value::undefined();
}

// Map.prototype.sizeのゲッター実装
Value mapSize(Value thisValue, Context* context) {
    // thisがMapオブジェクトかどうか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isMapObject()) {
        Error::throwError(Error::TypeError, "get Map.prototype.size called on incompatible receiver");
        return Value::undefined();
    }
    
    auto* mapObj = static_cast<MapObject*>(thisValue.asObject());
    return Value(static_cast<double>(mapObj->size()));
}

// Map.prototype.forEachの実装
Value mapForEach(Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // thisがMapオブジェクトかどうか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isMapObject()) {
        Error::throwError(Error::TypeError, "Map.prototype.forEach called on incompatible receiver");
        return Value::undefined();
    }
    
    auto* mapObj = static_cast<MapObject*>(thisValue.asObject());
    
    // 引数が少なくとも1つあることを確認
    if (arguments.size() < 1 || !arguments[0].isCallable()) {
        Error::throwError(Error::TypeError, "コールバックは関数である必要があります");
        return Value::undefined();
    }
    
    Value callback = arguments[0];
    Value thisArg = arguments.size() > 1 ? arguments[1] : Value::undefined();
    
    mapObj->forEach(callback, thisArg);
    
    return Value::undefined();
}

// Map.prototype.keysの実装
Value mapKeys(Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // thisがMapオブジェクトかどうか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isMapObject()) {
        Error::throwError(Error::TypeError, "Map.prototype.keys called on incompatible receiver");
        return Value::undefined();
    }
    
    auto* mapObj = static_cast<MapObject*>(thisValue.asObject());
    return mapObj->keys();
}

// Map.prototype.valuesの実装
Value mapValues(Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // thisがMapオブジェクトかどうか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isMapObject()) {
        Error::throwError(Error::TypeError, "Map.prototype.values called on incompatible receiver");
        return Value::undefined();
    }
    
    auto* mapObj = static_cast<MapObject*>(thisValue.asObject());
    return mapObj->values();
}

// Map.prototype.entriesの実装
Value mapEntries(Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // thisがMapオブジェクトかどうか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isMapObject()) {
        Error::throwError(Error::TypeError, "Map.prototype.entries called on incompatible receiver");
        return Value::undefined();
    }
    
    auto* mapObj = static_cast<MapObject*>(thisValue.asObject());
    return mapObj->entries();
}

// Mapコンストラクタの実装
Value MapObject::mapConstructor(Value callee, Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // new演算子を使わずに呼び出された場合、エラーをスロー
    if (!thisValue.isObject() || !thisValue.asObject()->isMapObject()) {
        Error::throwError(Error::TypeError, "Map constructor must be called with new");
        return Value::undefined();
    }
    
    // thisValueは新しいMapオブジェクト
    auto* mapObj = static_cast<MapObject*>(thisValue.asObject());
    
    // イテレーブルが渡された場合はそれを使用して初期化
    if (arguments.size() > 0 && !arguments[0].isNullOrUndefined()) {
        auto iterator = IteratorHelper::getIterator(arguments[0], context);
        if (!iterator.isObject()) {
            Error::throwError(Error::TypeError, "イテレーブルオブジェクトが必要です");
            return thisValue;
        }
        
        Value iteratorResult;
        while (!(iteratorResult = IteratorHelper::next(iterator)).isFalse()) {
            Value entry = iteratorResult;
            
            // エントリは[key, value]の形式である必要がある
            if (!entry.isObject() || !entry.asObject()->has("0") || !entry.asObject()->has("1")) {
                Error::throwError(Error::TypeError, "イテレーブルのエントリは[key, value]の形式である必要があります");
                return thisValue;
            }
            
            Value key = entry.asObject()->get("0");
            Value value = entry.asObject()->get("1");
            
            // キーと値をMapに追加
            mapObj->set(key, value);
        }
    }
    
    return thisValue;
}

// Mapプロトタイプの初期化
Value MapObject::initializePrototype(Context* context) {
    // Map.prototype オブジェクトを作成
    auto* prototype = new Object(context->objectPrototype());
    
    // メソッドを追加
    prototype->defineNativeFunction(context->symbolToStringTag(), "Map", Object::defaultAttributes);
    prototype->defineNativeFunction("set", mapSet, 2, Object::defaultAttributes);
    prototype->defineNativeFunction("get", mapGet, 1, Object::defaultAttributes);
    prototype->defineNativeFunction("has", mapHas, 1, Object::defaultAttributes);
    prototype->defineNativeFunction("delete", mapDelete, 1, Object::defaultAttributes);
    prototype->defineNativeFunction("clear", mapClear, 0, Object::defaultAttributes);
    prototype->defineAccessor("size", mapSize, nullptr, Object::defaultAttributes);
    prototype->defineNativeFunction("forEach", mapForEach, 1, Object::defaultAttributes);
    prototype->defineNativeFunction("keys", mapKeys, 0, Object::defaultAttributes);
    prototype->defineNativeFunction("values", mapValues, 0, Object::defaultAttributes);
    prototype->defineNativeFunction("entries", mapEntries, 0, Object::defaultAttributes);
    
    // イテレータメソッドとしてentriesを設定
    prototype->defineNativeFunction(context->symbolIterator(), mapEntries, 0, Object::defaultAttributes);
    
    return Value(prototype);
}

} // namespace aero 