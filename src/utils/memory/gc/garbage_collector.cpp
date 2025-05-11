/**
 * @file garbage_collector.cpp
 * @brief ガベージコレクタの実装
 * @copyright 2023 AeroJS プロジェクト
 */

#include "garbage_collector.h"
#include "../../../core/runtime/object.h"
#include "../smart_ptr/handle_manager.h"

namespace aero {

// 非テンプレートメソッドの実装

void GarbageCollector::registerWeakRefProvider(std::function<void()> provider) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_weakRefProviders.push_back(provider);
}

void GarbageCollector::registerFinalizationCallback(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_finalizationCallbacks.push_back(callback);
}

void GarbageCollector::triggerGC(bool force) {
  // GCが既に実行中ならスキップ
  bool expected = false;
  if (!m_isCollecting.compare_exchange_strong(expected, true)) {
    return;
  }

  try {
    // 並列コレクションが設定されている場合は別スレッドで実行
    if (m_parallelCollection && !force) {
      std::thread collector([this]() {
        try {
          this->collectGarbage();
        } catch (...) {
          // 例外を無視
        }
        m_isCollecting.store(false);
      });
      collector.detach();
    } else {
      // 直接実行
      collectGarbage();
      m_isCollecting.store(false);
    }
  } catch (...) {
    // 例外発生時にもフラグをリセット
    m_isCollecting.store(false);
    throw;
  }
}

void GarbageCollector::processFinalizationRegistries() {
  std::vector<std::function<void()>> callbacks;
  
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    callbacks = m_finalizationCallbacks;
  }
  
  // 各コールバックを実行
  for (const auto& callback : callbacks) {
    try {
      callback();
    } catch (...) {
      // エラーを無視（コールバックチェーンを中断しない）
    }
  }
}

void GarbageCollector::setParallelCollection(bool enabled) {
  m_parallelCollection = enabled;
}

void GarbageCollector::setDebugMode(bool enabled) {
  m_debugMode = enabled;
}

void GarbageCollector::collectGarbage() {
  if (m_debugMode) {
    printf("GC: Starting garbage collection\n");
  }

  // HandleManagerとの連携
  auto handleManager = HandleManager::getInstance();
  if (handleManager) {
    handleManager->prepareForGC();
  }

  // フェーズ1: マーク処理（WeakRefオブジェクトの特定を含む）
  markPhase();

  // フェーズ2: スイープ処理（オブジェクトの解放）
  std::vector<Object*> collectedObjects = sweepPhase();

  // HandleManagerに回収オブジェクトを通知
  if (handleManager && !collectedObjects.empty()) {
    handleManager->afterGC(collectedObjects);
  }

  // フェーズ3: WeakRefの更新
  updateWeakRefs();

  // フェーズ4: Finalizationコールバックの実行
  if (!collectedObjects.empty()) {
    processFinalizationRegistries();
  }

  if (m_debugMode) {
    printf("GC: Completed garbage collection, collected %zu objects\n", 
            collectedObjects.size());
  }
}

void GarbageCollector::markPhase() {
  // 実際の実装では、到達可能なオブジェクトをマークする処理が入る
  // ここではモックアップとして空の実装
}

std::vector<Object*> GarbageCollector::sweepPhase() {
  // 実際の実装では、マークされていないオブジェクトを解放する処理が入る
  // ここではモックアップとして空のリストを返す
  return std::vector<Object*>();
}

void GarbageCollector::updateWeakRefs() {
  std::vector<std::function<void()>> providers;
  
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    providers = m_weakRefProviders;
  }
  
  // 各プロバイダを実行
  for (const auto& provider : providers) {
    try {
      provider();
    } catch (...) {
      // エラーを無視
    }
  }
}

} // namespace aero 