/**
 * @file weakmap.cpp
 * @brief JavaScript のWeakMap組み込みオブジェクトの実装
 * @copyright 2023 AeroJS Project
 */

#include "weakmap.h"
#include "core/runtime/context.h"
#include "core/runtime/error.h"
#include "core/runtime/function.h"
#include "core/runtime/global_object.h"
#include "core/runtime/symbol.h"
#include <algorithm>
#include <stdexcept>

namespace aero {

// ObjectWeakPtrクラスの実装
WeakMapObject::ObjectWeakPtr::ObjectWeakPtr(Object* obj) 
    : m_ptr(obj) 
    , m_hash(std::hash<Object*>()(obj))
{
}

// WeakMapObjectコンストラクタの実装
WeakMapObject::WeakMapObject(Object* prototype)
    : Object(prototype)
    , m_entries()
{
    // 空のWeakMapを作成
}

// イテレーブルからWeakMapObjectを構築するコンストラクタ
WeakMapObject::WeakMapObject(Object* prototype, Value iterable)
    : Object(prototype)
    , m_entries()
{
    if (iterable.isNullOrUndefined()) {
        // nullまたはundefinedの場合は空のWeakMapを作成
        return;
    }
    
    Context* context = Context::current();
    
    // イテレーブルからエントリを追加
    Value iterator = iterable.getIterator(context);
    if (!iterator.isObject()) {
        context->throwTypeError("イテレーブルオブジェクトが必要です");
        return;
    }
    
    // イテレータからキーと値のペアを取得して追加
    while (true) {
        Value next = iterator.iteratorNext(context);
        if (next.iteratorDone(context)) {
            break;
        }
        
        Value entry = next.iteratorValue(context);
        
        // エントリはオブジェクトで[key, value]の形式でなければならない
        if (!entry.isObject() || !entry.asObject()->has("0") || !entry.asObject()->has("1")) {
            context->throwTypeError("イテレーブルのエントリは[key, value]の形式である必要があります");
            return;
        }
        
        Value key = entry.asObject()->get("0");
        Value value = entry.asObject()->get("1");
        
        // set()メソッドを使用して追加（キーのバリデーションを含む）
        set(key, value);
    }
}

// デストラクタ
WeakMapObject::~WeakMapObject() = default;

// キーが有効かどうかを確認（オブジェクトであること）
bool WeakMapObject::validateKey(Value key) {
    // キーはオブジェクトでなければならない
    if (!key.isObject()) {
        Context* context = Context::current();
        context->throwTypeError("WeakMapのキーはオブジェクトでなければなりません");
        return false;
    }
    return true;
}

// 指定されたキーに値を設定
Value WeakMapObject::set(Value key, Value value) {
    // キーの検証
    if (!validateKey(key)) {
        return Value::undefined();
    }
    
    // キーをオブジェクトとして取得
    Object* keyObj = key.asObject();
    
    // オブジェクトへの弱参照を作成してキーとして使用
    ObjectWeakPtr weakPtr(keyObj);
    
    // 値をマップに設定
    m_entries[weakPtr] = value;
    
    // メソッドチェーン用にthisを返す
    return Value(this);
}

// 指定されたキーに関連付けられた値を取得
Value WeakMapObject::get(Value key) const {
    // キーの検証
    if (!validateKey(key)) {
        return Value::undefined();
    }
    
    // キーをオブジェクトとして取得
    Object* keyObj = key.asObject();
    
    // オブジェクトへの弱参照を作成してキーとして使用
    ObjectWeakPtr weakPtr(keyObj);
    
    // マップからキーを探索
    auto it = m_entries.find(weakPtr);
    if (it != m_entries.end()) {
        return it->second;
    }
    
    // キーが存在しない場合はundefinedを返す
    return Value::undefined();
}

// 指定されたキーが存在するかどうかを確認
bool WeakMapObject::has(Value key) const {
    // キーの検証
    if (!validateKey(key)) {
        return false;
    }
    
    // キーをオブジェクトとして取得
    Object* keyObj = key.asObject();
    
    // オブジェクトへの弱参照を作成してキーとして使用
    ObjectWeakPtr weakPtr(keyObj);
    
    // マップにキーが存在するかチェック
    return m_entries.find(weakPtr) != m_entries.end();
}

// 指定されたキーとそれに関連付けられた値を削除
bool WeakMapObject::remove(Value key) {
    // キーの検証
    if (!validateKey(key)) {
        return false;
    }
    
    // キーをオブジェクトとして取得
    Object* keyObj = key.asObject();
    
    // オブジェクトへの弱参照を作成してキーとして使用
    ObjectWeakPtr weakPtr(keyObj);
    
    // マップからキーを削除し、削除が成功したかを返す
    return m_entries.erase(weakPtr) > 0;
}

// WeakMap.prototype.delete 実装
Value weakMapDelete(Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // thisがWeakMapオブジェクトであることを確認
    if (!thisValue.isObject() || !thisValue.asObject()->isWeakMapObject()) {
        context->throwTypeError("WeakMap.prototype.delete called on non-WeakMap object");
        return Value::undefined();
    }
    
    WeakMapObject* weakMap = static_cast<WeakMapObject*>(thisValue.asObject());
    
    // 引数が与えられていない場合
    if (arguments.empty()) {
        return Value(false); // キーが必要なので常にfalseを返す
    }
    
    // 引数の最初の値をキーとして使用
    return Value(weakMap->remove(arguments[0]));
}

// WeakMap.prototype.get 実装
Value weakMapGet(Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // thisがWeakMapオブジェクトであることを確認
    if (!thisValue.isObject() || !thisValue.asObject()->isWeakMapObject()) {
        context->throwTypeError("WeakMap.prototype.get called on non-WeakMap object");
        return Value::undefined();
    }
    
    WeakMapObject* weakMap = static_cast<WeakMapObject*>(thisValue.asObject());
    
    // 引数が与えられていない場合
    if (arguments.empty()) {
        context->throwTypeError("WeakMapのキーはオブジェクトでなければなりません");
        return Value::undefined();
    }
    
    // 引数の最初の値をキーとして使用
    return weakMap->get(arguments[0]);
}

// WeakMap.prototype.has 実装
Value weakMapHas(Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // thisがWeakMapオブジェクトであることを確認
    if (!thisValue.isObject() || !thisValue.asObject()->isWeakMapObject()) {
        context->throwTypeError("WeakMap.prototype.has called on non-WeakMap object");
        return Value::undefined();
    }
    
    WeakMapObject* weakMap = static_cast<WeakMapObject*>(thisValue.asObject());
    
    // 引数が与えられていない場合
    if (arguments.empty()) {
        return Value(false); // キーが必要なので常にfalseを返す
    }
    
    // 引数の最初の値をキーとして使用
    return Value(weakMap->has(arguments[0]));
}

// WeakMap.prototype.set 実装
Value weakMapSet(Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // thisがWeakMapオブジェクトであることを確認
    if (!thisValue.isObject() || !thisValue.asObject()->isWeakMapObject()) {
        context->throwTypeError("WeakMap.prototype.set called on non-WeakMap object");
        return Value::undefined();
    }
    
    WeakMapObject* weakMap = static_cast<WeakMapObject*>(thisValue.asObject());
    
    // 引数が与えられていない場合
    if (arguments.empty()) {
        context->throwTypeError("WeakMap.prototype.set requires a key argument");
        return Value::undefined();
    }
    
    // 引数の最初の値をキーとして、2番目の値（ない場合はundefined）を値として使用
    Value key = arguments[0];
    Value value = arguments.size() > 1 ? arguments[1] : Value::undefined();
    
    return weakMap->set(key, value);
}

// WeakMapコンストラクタ関数
Value WeakMapObject::weakMapConstructor(Value callee, Value thisValue, const std::vector<Value>& arguments, Context* context) {
    // new演算子なしで呼ばれた場合はエラー
    if (!thisValue.isConstructorCall()) {
        context->throwTypeError("WeakMap constructor must be called with new");
        return Value::undefined();
    }
    
    // WeakMapオブジェクトを作成
    WeakMapObject* weakMap = new WeakMapObject(context->globalObject()->objectPrototype());
    
    // イテラブルな引数があれば、その要素を追加
    if (!arguments.empty() && !arguments[0].isNullOrUndefined()) {
        Value iterable = arguments[0];
        
        // イテレータを取得
        Value iterator = iterable.getIterator(context);
        if (!iterator.isObject()) {
            context->throwTypeError("イテレーブルオブジェクトが必要です");
            return Value(weakMap);
        }
        
        // イテレータからキーと値のペアを取得して追加
        while (true) {
            Value next = iterator.iteratorNext(context);
            if (next.iteratorDone(context)) {
                break;
            }
            
            Value entry = next.iteratorValue(context);
            
            // エントリはオブジェクトで[key, value]の形式でなければならない
            if (!entry.isObject() || !entry.asObject()->has("0") || !entry.asObject()->has("1")) {
                context->throwTypeError("イテレーブルのエントリは[key, value]の形式である必要があります");
                return Value(weakMap);
            }
            
            Value key = entry.asObject()->get("0");
            Value value = entry.asObject()->get("1");
            
            // キーと値を追加
            weakMap->set(key, value);
            
            // 例外が発生した場合は中断
            if (context->hasException()) {
                return Value(weakMap);
            }
        }
    }
    
    return Value(weakMap);
}

// WeakMapプロトタイプの初期化
Value WeakMapObject::initializePrototype(Context* context) {
    // プロトタイプオブジェクトを作成
    Object* prototype = new Object(context->objectPrototype());
    
    // WeakMapコンストラクタを作成
    FunctionObject* constructor = new NativeFunctionObject(context, prototype, weakMapConstructor, 0, context->staticStrings()->WeakMap);
    
    // プロトタイプにメソッドを追加
    prototype->defineOwnProperty(context->staticStrings()->constructor,
        Object::PropertyDescriptor(Value(constructor), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // delete メソッド
    prototype->defineOwnProperty(context->staticStrings()->delete_,
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, weakMapDelete, 1, 
                                  context->staticStrings()->delete_), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // get メソッド
    prototype->defineOwnProperty(context->staticStrings()->get,
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, weakMapGet, 1, 
                                  context->staticStrings()->get), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // has メソッド
    prototype->defineOwnProperty(context->staticStrings()->has,
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, weakMapHas, 1, 
                                  context->staticStrings()->has), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // set メソッド
    prototype->defineOwnProperty(context->staticStrings()->set,
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, weakMapSet, 2, 
                                  context->staticStrings()->set), 
                                 Object::PropertyDescriptor::Writable | 
                                 Object::PropertyDescriptor::Configurable));
    
    // constructor.prototype
    constructor->defineOwnProperty(context->staticStrings()->prototype,
        Object::PropertyDescriptor(Value(prototype), Object::PropertyDescriptor::None));
    
    // WeakMap.prototype[@@toStringTag]
    prototype->defineOwnProperty(Symbol::wellKnown(context)->toStringTag,
        Object::PropertyDescriptor(context->staticStrings()->WeakMap,
                                 Object::PropertyDescriptor::Configurable));
    
    return Value(constructor);
}

// WeakMapオブジェクトの初期化
Value initializeWeakMap(Context* context) {
    return WeakMapObject::initializePrototype(context);
}

// WeakMap組み込みオブジェクトの登録
void registerWeakMapBuiltin(GlobalObject* global) {
    if (!global) {
        return;
    }
    
    Context* context = global->context();
    if (!context) {
        return;
    }
    
    // WeakMapコンストラクタの初期化と登録
    Value weakMapConstructor = initializeWeakMap(context);
    
    global->defineOwnProperty(context->staticStrings()->WeakMap,
        Object::PropertyDescriptor(weakMapConstructor,
                                 Object::PropertyDescriptor::Writable |
                                 Object::PropertyDescriptor::Configurable));
}

} // namespace aero 