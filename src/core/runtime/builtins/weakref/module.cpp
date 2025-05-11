/**
 * @file module.cpp
 * @brief WeakRefモジュールのエントリーポイント
 * @copyright 2023 AeroJS プロジェクト
 */

#include "weakref.h"
#include "../../global_object.h"

namespace aero {

/**
 * @brief WeakRefモジュールを初期化
 * @param globalObj グローバルオブジェクト
 */
void initWeakRefModule(GlobalObject* globalObj) {
  // WeakRefオブジェクトを初期化
  initWeakRefObject(globalObj);
}

} // namespace aero 