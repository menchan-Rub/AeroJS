/**
 * @file module_init.cpp
 * @brief WeakRefモジュールの初期化関数の実装
 * @copyright 2023 AeroJS プロジェクト
 */

#include "../modules.h"
#include "../../global_object.h"
#include "weakref.h"
#include "../../../utils/memory/gc/garbage_collector.h"
#include "../../../utils/memory/smart_ptr/handle_manager.h"

namespace aero {

/**
 * @brief WeakRefモジュールをグローバルオブジェクトに登録する
 * @param globalObj グローバルオブジェクト
 */
void registerWeakRefBuiltin(GlobalObject* globalObj) {
  // WeakRefオブジェクトを初期化
  initWeakRefObject(globalObj);
  
  // ガベージコレクタとの連携を確保
  GarbageCollector* gc = GarbageCollector::getInstance();
  if (gc) {
    // WeakRefオブジェクトをGCに登録し、ガベージコレクション後のコールバックを設定
    gc->registerWeakRefProvider([globalObj]() {
      // GC後に必要な処理があればここに実装
      // 例：タイミングの良いタイミングでのWeakRefの状態更新など
    });
  }
  
  // ハンドルマネージャーとの連携を確保
  HandleManager* handleManager = HandleManager::getInstance();
  if (handleManager) {
    // WeakRefオブジェクトからのWeakHandleの適切な管理を設定
    handleManager->registerHandleProvider("WeakRef", [globalObj](Object* obj) {
      // WeakRefオブジェクトに対する特別な処理があればここに実装
      return obj && obj->isWeakRef();
    });
  }
  
  if (globalObj->context() && globalObj->context()->debugMode()) {
    globalObj->context()->logger()->info("WeakRef module initialized");
  }
}

} // namespace aero 