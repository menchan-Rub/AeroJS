/**
 * @file module.cpp
 * @brief JavaScript の RegExp モジュール登録
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#include "regexp.h"
#include "core/runtime/global_object.h"
#include "core/runtime/execution_context.h"

namespace aero {

/**
 * @brief RegExp モジュールをグローバルオブジェクトに登録します
 * 
 * @param global グローバルオブジェクト
 */
void registerRegExpModule(GlobalObject* global) {
    // 実行コンテキストを取得
    auto* ctx = ExecutionContext::current();
    
    // RegExp 組み込みオブジェクトを登録
    registerRegExpObject(ctx, global);
}

} // namespace aero 