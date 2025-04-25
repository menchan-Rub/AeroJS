/**
 * @file module.cpp
 * @brief JavaScript の Reflect モジュール登録
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#include "core/runtime/execution_context.h"
#include "core/runtime/global_object.h"
#include "reflect.h"

namespace aero {

/**
 * @brief Reflect モジュールをグローバルオブジェクトに登録します
 *
 * @param global グローバルオブジェクト
 */
void registerReflectModule(GlobalObject* global) {
  // 実行コンテキストを取得
  auto* ctx = ExecutionContext::current();

  // Reflect 組み込みオブジェクトを登録
  registerReflectObject(ctx, global);
}

}  // namespace aero