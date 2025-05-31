/**
 * @file module_init.cpp
 * @brief FinalizationRegistryモジュールの初期化関数の実装
 * @copyright 2023 AeroJS プロジェクト
 */

#include "../modules.h"
#include "../../global_object.h"
#include "finalization_registry.h"
#include "../../../utils/memory/gc/garbage_collector.h"
#include "../../../utils/memory/smart_ptr/handle_manager.h"

namespace aero {

/**
 * @brief FinalizationRegistryモジュールをグローバルオブジェクトに登録する
 * @param globalObj グローバルオブジェクト
 */
void registerFinalizationRegistryBuiltin(GlobalObject* globalObj) {
  // FinalizationRegistryオブジェクトを初期化
  initFinalizationRegistryObject(globalObj);
  
  // GCとFinalizationRegistryの連携を本格実装
  GarbageCollector* gc = GarbageCollector::getInstance();
  if (gc) {
    // FinalizationRegistryインスタンスをGCが直接管理し、GCサイクル後にcleanupSomeを呼び出す
    gc->registerFinalizationCallback([globalObj]() {
      Value registryVal = globalObj->get("FinalizationRegistry");
      if (!registryVal.isObject() || !registryVal.asObject()->isConstructor()) {
        return;
      }
      // GCが全FinalizationRegistryインスタンスを列挙し、各インスタンスのcleanupSomeを呼び出す
      auto registries = GarbageCollector::getInstance()->getAllFinalizationRegistries();
      for (auto* reg : registries) {
        if (reg) reg->cleanupSome();
      }
    });
  }
  
  // ハンドルマネージャーとの連携を確保
  HandleManager* handleManager = HandleManager::getInstance();
  if (handleManager) {
    // FinalizationRegistryオブジェクトからのWeakHandleの適切な管理を設定
    handleManager->registerHandleProvider("FinalizationRegistry", [globalObj](Object* obj) {
      // FinalizationRegistryオブジェクトに対する特別な処理があればここに実装
      return obj && obj->isFinalizationRegistry();
    });
  }
  
  if (globalObj->context() && globalObj->context()->debugMode()) {
    globalObj->context()->logger()->info("FinalizationRegistry module initialized");
  }
}

} // namespace aero 