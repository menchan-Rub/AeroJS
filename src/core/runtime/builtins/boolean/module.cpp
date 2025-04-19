/**
 * @file module.cpp
 * @brief JavaScript の Boolean 組み込みオブジェクトのモジュール登録
 * @copyright Copyright (c) 2023 AeroJS project authors
 */

#include "boolean.h"
#include "../../global_object.h"

namespace aero {

/**
 * @brief Boolean 組み込みオブジェクトをグローバルオブジェクトに登録
 * 
 * @param global グローバルオブジェクト
 */
void registerBooleanBuiltin(GlobalObject* global) {
    if (!global) {
        return;
    }
    
    // 実行コンテキストを取得
    Context* context = global->context();
    if (!context) {
        return;
    }
    
    // Booleanオブジェクトを初期化
    Value booleanConstructor = initializeBoolean(context);
    
    // BooleanコンストラクタをグローバルオブジェクトにBooleanという名前で登録
    global->defineOwnProperty(
        context->staticStrings()->Boolean,
        Object::PropertyDescriptor(
            booleanConstructor,
            Object::PropertyDescriptor::Writable | Object::PropertyDescriptor::Configurable
        )
    );
}

} // namespace aero 