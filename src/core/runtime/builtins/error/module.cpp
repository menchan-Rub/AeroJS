/**
 * @file module.cpp
 * @brief JavaScript のエラーモジュール登録
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#include "core/runtime/execution_context.h"
#include "core/runtime/global_object.h"
#include "error.h"

namespace aero {

/**
 * @brief エラーモジュールをグローバルオブジェクトに登録します
 *
 * @param global グローバルオブジェクト
 */
void registerErrorModule(GlobalObject* global) {
  // 実行コンテキストを取得
  auto* ctx = ExecutionContext::current();

  // エラー関連の組み込みオブジェクトを登録
  registerErrorObjects(ctx, global);
}

}  // namespace aero