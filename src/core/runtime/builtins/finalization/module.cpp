/**
 * @file module.cpp
 * @brief FinalizationRegistryモジュールのエントリーポイント
 * @copyright 2023 AeroJS プロジェクト
 */

#include "finalization_registry.h"
#include "../../global_object.h"

namespace aero {

/**
 * @brief FinalizationRegistryモジュールを初期化
 * @param globalObj グローバルオブジェクト
 */
void initFinalizationModule(GlobalObject* globalObj) {
  // FinalizationRegistryオブジェクトを初期化
  initFinalizationRegistryObject(globalObj);
}

} // namespace aero 