/**
 * @file boolean.cpp
 * @brief JavaScriptのBoolean組み込みオブジェクトの実装
 * @copyright Copyright (c) 2023 AeroJS project authors
 */

#include "boolean.h"
#include "core/runtime/context.h"
#include "core/runtime/error.h"
#include "core/runtime/function.h"
#include "core/runtime/global_object.h"
#include "core/runtime/string.h"

namespace aero {

BooleanObject::BooleanObject(bool value)
    : Object(nullptr)
    , m_value(value)
{
}

BooleanObject::~BooleanObject() = default;

bool BooleanObject::value() const
{
    return m_value;
}

Value booleanConstructor(Context* context, Value thisValue, const std::vector<Value>& args)
{
    // 引数がない場合はデフォルトでfalse
    bool boolValue = false;
    
    if (!args.empty()) {
        // 最初の引数をブール値に変換
        boolValue = args[0].toBoolean();
    }
    
    // new演算子で呼び出された場合はBooleanオブジェクトを返す
    if (thisValue.isConstructorCall()) {
        auto* booleanObject = new BooleanObject(boolValue);
        booleanObject->setPrototype(context->globalObject()->booleanPrototype());
        return Value(booleanObject);
    }
    
    // 通常の関数呼び出しの場合はプリミティブなブール値を返す
    return Value(boolValue);
}

Value booleanToString(Context* context, Value thisValue, const std::vector<Value>&)
{
    // thisValueがBooleanObjectの場合
    if (thisValue.isBooleanObject()) {
        auto* booleanObject = thisValue.asBooleanObject();
        return Value(context->staticStrings()->booleanToString(booleanObject->value()));
    }
    
    // thisValueがプリミティブなブール値の場合
    if (thisValue.isBoolean()) {
        return Value(context->staticStrings()->booleanToString(thisValue.asBoolean()));
    }
    
    // 不正なthisValue
    context->throwTypeError("Boolean.prototype.toString requires that 'this' be a Boolean");
    return Value();
}

Value booleanValueOf(Context* context, Value thisValue, const std::vector<Value>&)
{
    // thisValueがBooleanObjectの場合
    if (thisValue.isBooleanObject()) {
        return Value(thisValue.asBooleanObject()->value());
    }
    
    // thisValueがプリミティブなブール値の場合
    if (thisValue.isBoolean()) {
        return thisValue;
    }
    
    // 不正なthisValue
    context->throwTypeError("Boolean.prototype.valueOf requires that 'this' be a Boolean");
    return Value();
}

Value BooleanObject::initializePrototype(Context* context)
{
    // Booleanのプロトタイプオブジェクトを作成
    Object* prototype = new Object(context->objectPrototype());
    
    // プロトタイプにメソッドを設定
    prototype->defineOwnProperty(context->staticStrings()->constructor,
        Object::PropertyDescriptor(Value(context->globalObject()->booleanConstructor()), 
                                  Object::PropertyDescriptor::Writable | 
                                  Object::PropertyDescriptor::Configurable));
    
    prototype->defineOwnProperty(context->staticStrings()->toString,
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, booleanToString, 0, 
                                  context->staticStrings()->toString), 
                                  Object::PropertyDescriptor::Writable | 
                                  Object::PropertyDescriptor::Configurable));
    
    prototype->defineOwnProperty(context->staticStrings()->valueOf,
        Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, booleanValueOf, 0, 
                                  context->staticStrings()->valueOf), 
                                  Object::PropertyDescriptor::Writable | 
                                  Object::PropertyDescriptor::Configurable));
    
    // プロトタイプをグローバルオブジェクトに設定
    context->globalObject()->setBooleanPrototype(prototype);
    
    // Booleanコンストラクタを作成
    FunctionObject* constructor = new NativeFunctionObject(context, prototype, booleanConstructor, 1, context->staticStrings()->Boolean);
    
    // コンストラクタにプロパティを設定
    constructor->defineOwnProperty(context->staticStrings()->prototype,
        Object::PropertyDescriptor(Value(prototype), Object::PropertyDescriptor::None));
    
    constructor->defineOwnProperty(context->staticStrings()->length,
        Object::PropertyDescriptor(Value(1), Object::PropertyDescriptor::None));
    
    // グローバルオブジェクトにコンストラクタを設定
    context->globalObject()->setBooleanConstructor(constructor);
    
    return Value(constructor);
}

Value initializeBoolean(Context* context)
{
    // BooleanコンストラクタとBooleanプロトタイプを初期化
    return BooleanObject::initializePrototype(context);
}

} // namespace aero 