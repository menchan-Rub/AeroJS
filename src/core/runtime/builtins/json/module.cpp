/**
 * @file module.cpp
 * @brief JavaScript の JSON 組み込みオブジェクトのモジュール登録
 * @copyright Copyright (c) 2023 AeroJS project authors
 */

#include "json.h"
#include "../../global_object.h"

namespace aero {

/**
 * @brief JSON 組み込みオブジェクトをグローバルオブジェクトに登録
 * 
 * @param global グローバルオブジェクト
 */
void registerJSONBuiltin(GlobalObject* global) {
    if (!global) {
        return;
    }
    
    // 実行コンテキストを取得
    Context* context = global->context();
    if (!context) {
        return;
    }
    
    // JSONオブジェクトを作成
    Object* jsonObj = new Object(context->objectPrototype());
    
    // JSONのメソッドを追加
    jsonObj->defineOwnProperty("parse",
        Object::PropertyDescriptor(
            new NativeFunctionObject(context, nullptr, jsonParse, 2, context->staticStrings()->parse),
            Object::PropertyDescriptor::Writable | Object::PropertyDescriptor::Configurable
        )
    );
    
    jsonObj->defineOwnProperty("stringify",
        Object::PropertyDescriptor(
            new NativeFunctionObject(context, nullptr, jsonStringify, 3, context->staticStrings()->stringify),
            Object::PropertyDescriptor::Writable | Object::PropertyDescriptor::Configurable
        )
    );
    
    // JSON[@@toStringTag]プロパティを設定
    jsonObj->defineOwnProperty(
        Symbol::wellKnown(context)->toStringTag,
        Object::PropertyDescriptor(
            context->staticStrings()->JSON,
            Object::PropertyDescriptor::Configurable
        )
    );
    
    // JSONオブジェクトをグローバルオブジェクトに登録
    global->defineOwnProperty(
        context->staticStrings()->JSON,
        Object::PropertyDescriptor(
            Value(jsonObj),
            Object::PropertyDescriptor::Writable | Object::PropertyDescriptor::Configurable
        )
    );
}

} // namespace aero 