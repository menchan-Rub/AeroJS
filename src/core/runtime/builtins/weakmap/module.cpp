/**
 * @file module.cpp
 * @brief JavaScript の WeakMap 組み込みオブジェクトのモジュール登録
 * @copyright Copyright (c) 2023 AeroJS project authors
 */

#include "weakmap.h"
#include "../../global_object.h"

namespace aero {

/**
 * @brief WeakMap 組み込みオブジェクトをグローバルオブジェクトに登録
 * 
 * @param global グローバルオブジェクト
 */
void registerWeakMapBuiltin(GlobalObject* global) {
    if (!global) {
        return;
    }
    
    // 実行コンテキストを取得
    Context* context = global->context();
    if (!context) {
        return;
    }
    
    // WeakMapオブジェクトを初期化
    Value weakMapConstructor = initializeWeakMap(context);
    
    // WeakMapコンストラクタをグローバルオブジェクトにWeakMapという名前で登録
    global->defineOwnProperty(
        context->staticStrings()->WeakMap,
        Object::PropertyDescriptor(
            weakMapConstructor,
            Object::PropertyDescriptor::Writable | Object::PropertyDescriptor::Configurable
        )
    );
}

} // namespace aero 