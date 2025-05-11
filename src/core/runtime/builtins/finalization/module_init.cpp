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
  
  // ガベージコレクタとの連携を確保
  GarbageCollector* gc = GarbageCollector::getInstance();
  if (gc) {
    // FinalizationRegistryオブジェクトをGCに登録し、ガベージコレクション後のコールバックを設定
    gc->registerFinalizationCallback([globalObj]() {
      // グローバルオブジェクトから登録されているFinalizationRegistryを取得
      Value registryVal = globalObj->get("FinalizationRegistry");
      if (!registryVal.isObject() || !registryVal.asObject()->isConstructor()) {
        return; // FinalizationRegistryが存在しない場合は何もしない
      }
      
      // コンストラクタから作成されたインスタンスをトラバースし、cleanupSomeを呼び出す
      // 実際の実装ではGC自体が直接FinalizationRegistryを認識しているはず
      gc->processFinalizationRegistries();
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