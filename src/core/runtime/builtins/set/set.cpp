/**
 * @file set.cpp
 * @brief JavaScriptのSet組み込みオブジェクトの実装
 * @copyright Copyright (c) 2023 AeroJS project authors
 */

#include "set.h"
#include "core/runtime/context.h"
#include "core/runtime/error.h"
#include "core/runtime/function.h"
#include "core/runtime/global_object.h"
#include "core/runtime/symbol.h"
#include <algorithm>
#include <stdexcept>

namespace aero {

// ValueHashの実装
size_t SetObject::ValueHash::operator()(const Value& value) const {
    if (value.isString()) {
        return std::hash<std::string>{}(value.toString());
    } else if (value.isNumber()) {
        return std::hash<double>{}(value.toNumber());
    } else if (value.isBoolean()) {
        return std::hash<bool>{}(value.toBoolean());
    } else if (value.isNull()) {
        return std::hash<std::nullptr_t>{}(nullptr);
    } else if (value.isUndefined()) {
        return 0;
    } else if (value.isObject()) {
        // オブジェクトの場合はポインタのハッシュ値を使用
        return std::hash<Object*>{}(value.asObject());
    } else if (value.isSymbol()) {
        // シンボルの場合はIDをハッシュ値として使用
        return std::hash<int>{}(value.asSymbol()->id());
    }
    
    // その他の型の場合はデフォルトのハッシュ
    return 0;
}

// ValueEqualの実装
bool SetObject::ValueEqual::operator()(const Value& lhs, const Value& rhs) const {
    // 型が異なる場合は常に不一致
    if (lhs.type() != rhs.type()) {
        return false;
    }
    
    // 型ごとの比較
    switch (lhs.type()) {
        case Value::Type::Undefined:
        case Value::Type::Null:
            return true; // 同じ型のundefinedまたはnullは常に等しい
            
        case Value::Type::Boolean:
            return lhs.asBoolean() == rhs.asBoolean();
            
        case Value::Type::Number: {
            double lnum = lhs.asNumber();
            double rnum = rhs.asNumber();
            
            // NaNはNaNと等しい (通常のJavaScriptとは異なる、Setの仕様)
            if (std::isnan(lnum) && std::isnan(rnum)) {
                return true;
            }
            
            // +0と-0は同一とみなす
            if (lnum == 0 && rnum == 0) {
                return true;
            }
            
            return lnum == rnum;
        }
            
        case Value::Type::String:
            return lhs.asString()->value() == rhs.asString()->value();
            
        case Value::Type::Symbol:
            return lhs.asSymbol() == rhs.asSymbol();
            
        case Value::Type::Object:
            // オブジェクトは参照比較
            return lhs.asObject() == rhs.asObject();
            
        case Value::Type::BigInt:
            // BigIntの比較
            return lhs.asBigInt().equals(rhs.asBigInt());
            
        default:
            return false;
    }
}

// SetObjectの実装
SetObject::SetObject(Context* context, Object* prototype)
    : Object(context, prototype), m_values()
{
}

SetObject::~SetObject() = default;

Value SetObject::add(const Value& value) {
    // 既に存在する値を上書き
    m_values[value] = value;
    return Value(this); // メソッドチェーン用にthisを返す
}

bool SetObject::has(const Value& value) const {
    return m_values.find(value) != m_values.end();
}

bool SetObject::remove(const Value& value) {
    return m_values.erase(value) > 0;
}

void SetObject::clear() {
    m_values.clear();
}

size_t SetObject::size() const {
    return m_values.size();
}

Value SetObject::values(Context* context) const {
    // 値を配列に変換
    Object* array = context->createArray(m_values.size());
    size_t index = 0;
    
    for (const auto& pair : m_values) {
        array->set(std::to_string(index), pair.second);
        ++index;
    }
    
    return Value(array);
}

// Setコンストラクタ関数の実装
Value setConstructor(Context* context, Value thisValue, const std::vector<Value>& args) {
    // new演算子なしで呼ばれた場合はエラー
    if (!thisValue.isConstructorCall()) {
        context->throwTypeError("Set constructor must be called with new");
        return Value();
    }
    
    // Setオブジェクトを作成
    SetObject* set = new SetObject(context, context->globalObject()->objectPrototype());
    
    // イテラブルな引数があれば、その要素を追加
    if (!args.empty() && args[0].isObject()) {
        // TODO: イテラブルプロトコルのサポート
        // 現在は配列のみサポート
        Object* iterable = args[0].asObject();
        
        // length属性があれば配列とみなす
        if (iterable->has("length")) {
            Value lengthValue = iterable->get("length");
            if (lengthValue.isNumber()) {
                size_t length = static_cast<size_t>(lengthValue.asNumber());
                
                for (size_t i = 0; i < length; ++i) {
                    Value element = iterable->get(std::to_string(i));
                    set->add(element);
                }
            }
        }
    }
    
    return Value(set);
}

// Set.prototype.add実装
Value setAdd(Context* context, Value thisValue, const std::vector<Value>& args) {
    // thisがSetオブジェクトか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
        context->throwTypeError("Set.prototype.add called on non-Set object");
        return Value();
    }
    
    SetObject* set = static_cast<SetObject*>(thisValue.asObject());
    
    // 値が与えられていなければundefinedを追加
    if (args.empty()) {
        return set->add(Value::undefined());
    }
    
    // 値を追加
    return set->add(args[0]);
}

// Set.prototype.clear実装
Value setClear(Context* context, Value thisValue, const std::vector<Value>&) {
    // thisがSetオブジェクトか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
        context->throwTypeError("Set.prototype.clear called on non-Set object");
        return Value();
    }
    
    SetObject* set = static_cast<SetObject*>(thisValue.asObject());
    set->clear();
    
    return Value::undefined();
}

// Set.prototype.delete実装
Value setDelete(Context* context, Value thisValue, const std::vector<Value>& args) {
    // thisがSetオブジェクトか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
        context->throwTypeError("Set.prototype.delete called on non-Set object");
        return Value();
    }
    
    SetObject* set = static_cast<SetObject*>(thisValue.asObject());
    
    // 値が与えられていなければundefinedを削除する
    if (args.empty()) {
        return Value(set->remove(Value::undefined()));
    }
    
    // 値を削除
    return Value(set->remove(args[0]));
}

// Set.prototype.has実装
Value setHas(Context* context, Value thisValue, const std::vector<Value>& args) {
    // thisがSetオブジェクトか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
        context->throwTypeError("Set.prototype.has called on non-Set object");
        return Value();
    }
    
    SetObject* set = static_cast<SetObject*>(thisValue.asObject());
    
    // 値が与えられていなければundefinedを確認
    if (args.empty()) {
        return Value(set->has(Value::undefined()));
    }
    
    // 値を確認
    return Value(set->has(args[0]));
}

// Set.prototype.forEach実装
Value setForEach(Context* context, Value thisValue, const std::vector<Value>& args) {
    // thisがSetオブジェクトか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
        context->throwTypeError("Set.prototype.forEach called on non-Set object");
        return Value();
    }
    
    // コールバック関数が提供されていることを確認
    if (args.empty() || !args[0].isFunction()) {
        context->throwTypeError("Set.prototype.forEach requires a callback function");
        return Value();
    }
    
    SetObject* set = static_cast<SetObject*>(thisValue.asObject());
    Value callback = args[0];
    Value thisArg = args.size() > 1 ? args[1] : Value::undefined();
    
    // すべての値に対してコールバックを実行
    std::vector<Value> callbackArgs(3);
    callbackArgs[2] = thisValue; // 第3引数は常にSetオブジェクト自身
    
    for (const auto& pair : set->m_values) {
        callbackArgs[0] = pair.second; // 値
        callbackArgs[1] = pair.second; // 値と同じ（Mapと違い）
        
        // コールバック関数を実行
        context->callFunction(callback.asObject()->asFunction(), thisArg, callbackArgs);
        
        // 例外が発生したら即座に停止
        if (context->hasException()) {
            return Value();
        }
    }
    
    return Value::undefined();
}

// Set.prototype.values実装
Value setValues(Context* context, Value thisValue, const std::vector<Value>&) {
    // thisがSetオブジェクトか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
        context->throwTypeError("Set.prototype.values called on non-Set object");
        return Value();
    }
    
    SetObject* set = static_cast<SetObject*>(thisValue.asObject());
    
    // SetIteratorオブジェクトを作成して返す
    Object* iteratorObj = context->createObject(context->getSetIteratorPrototype());
    SetIterator* iterator = new SetIterator(set, SetIterator::IterationType::Values);
    iteratorObj->setInternalSlot("iterator", Value(iterator));
    iteratorObj->setInternalSlot("iteratedObject", thisValue);
    iteratorObj->setInternalSlot("iterationKind", Value(static_cast<int>(SetIterator::IterationType::Values)));
    iteratorObj->setInternalSlot("nextIndex", Value(0));
    iteratorObj->setInternalSlot("done", Value(false));
    
    // イテレータの状態を追跡するためのセットへの参照を保持
    set->registerIterator(iterator);
    
    return Value(iteratorObj);
}

// Set.prototype.keys実装（valuesと同じ）
Value setKeys(Context* context, Value thisValue, const std::vector<Value>& args) {
    // keys()はvalues()と同じ動作
    return setValues(context, thisValue, args);
}

// Set.prototype.entries実装
Value setEntries(Context* context, Value thisValue, const std::vector<Value>&) {
    // thisがSetオブジェクトか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
        context->throwTypeError("Set.prototype.entries called on non-Set object");
        return Value();
    }
    
    SetObject* set = static_cast<SetObject*>(thisValue.asObject());
    
    // [値, 値]のペアの配列を作成
    Object* array = context->createArray(set->size());
    size_t index = 0;
    
    for (const auto& pair : set->m_values) {
        // 各エントリは[値, 値]の配列
        Object* entry = context->createArray(2);
        entry->set("0", pair.second);
        entry->set("1", pair.second);
        
        array->set(std::to_string(index), Value(entry));
        ++index;
    }
    
    return Value(array);
}

// Set.prototype[Symbol.iterator]実装（valuesと同じ）
Value setIterator(Context* context, Value thisValue, const std::vector<Value>& args) {
    // [Symbol.iterator]()はvalues()と同じ動作
    return setValues(context, thisValue, args);
}

// Set.prototype.size取得関数
Value setSize(Context* context, Value thisValue, const std::vector<Value>&) {
    // thisがSetオブジェクトか確認
    if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
        context->throwTypeError("Set.prototype.size called on non-Set object");
        return Value();
    }
    
    SetObject* set = static_cast<SetObject*>(thisValue.asObject());
    
    return Value(static_cast<double>(set->size()));
}

// Setプロトタイプの初期化
Value SetObject::initializePrototype(Context* context) {
    // プロトタイプオブジェクトを作成
    Object* prototype = new Object(context->objectPrototype());
    
    // Setコンストラクタを作成
    FunctionObject* constructor = new NativeFunctionObject(context, prototype, setConstructor, 0, context->staticStrings()->Set);
    
    // プロトタイプにメソッドを追加
    prototype->defineOwnProperty(context->staticStrings()->constructor,
        Object::PropertyDescriptor(Value(constructor), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // add メソッド
    prototype->defineOwnProperty(context->staticStrings()->add,
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setAdd, 1, 
                                  context->staticStrings()->add), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // clear メソッド
    prototype->defineOwnProperty(context->staticStrings()->clear,
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setClear, 0, 
                                  context->staticStrings()->clear), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // delete メソッド
    prototype->defineOwnProperty(context->staticStrings()->delete_,
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setDelete, 1, 
                                  context->staticStrings()->delete_), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // has メソッド
    prototype->defineOwnProperty(context->staticStrings()->has,
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setHas, 1, 
                                  context->staticStrings()->has), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // forEach メソッド
    prototype->defineOwnProperty(context->staticStrings()->forEach,
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setForEach, 1, 
                                  context->staticStrings()->forEach), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // values メソッド
    prototype->defineOwnProperty(context->staticStrings()->values,
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setValues, 0, 
                                  context->staticStrings()->values), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // keys メソッド
    prototype->defineOwnProperty(context->staticStrings()->keys,
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setKeys, 0, 
                                  context->staticStrings()->keys), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // entries メソッド
    prototype->defineOwnProperty(context->staticStrings()->entries,
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setEntries, 0, 
                                  context->staticStrings()->entries), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // [Symbol.iterator] メソッド
    prototype->defineOwnProperty(context->getSymbolFor("iterator"),
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setIterator, 0, 
                                  context->staticStrings()->iterator), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // size プロパティ (getter)
    Object* sizeGetter = new NativeFunctionObject(context, nullptr, setSize, 0, context->staticStrings()->getSize);
    prototype->defineOwnProperty(context->staticStrings()->size,
        Object::PropertyDescriptor(nullptr, sizeGetter, nullptr,
                                 Object::PropertyDescriptor::Configurable));
    
    // constructor.prototype
    constructor->defineOwnProperty(context->staticStrings()->prototype,
        Object::PropertyDescriptor(Value(prototype), Object::PropertyDescriptor::None));
    
    // Set.prototype[@@toStringTag]
    prototype->defineOwnProperty(Symbol::wellKnown(context)->toStringTag,
        Object::PropertyDescriptor(context->staticStrings()->Set,
                                 Object::PropertyDescriptor::Configurable));
    
    return Value(constructor);
}

// Setオブジェクトの初期化
Value initializeSet(Context* context) {
    return SetObject::initializePrototype(context);
}

// Set組み込みオブジェクトの登録
void registerSetBuiltin(GlobalObject* global) {
    if (!global) {
        return;
    }
    
    Context* context = global->context();
    if (!context) {
        return;
    }
    
    // Setコンストラクタの初期化と登録
    Value setConstructor = initializeSet(context);
    
    global->defineOwnProperty(context->staticStrings()->Set,
        Object::PropertyDescriptor(setConstructor,
                                 Object::PropertyDescriptor::Writable |
                                 Object::PropertyDescriptor::Configurable));
}

} // namespace aero 