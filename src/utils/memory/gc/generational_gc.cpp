/**
 * @file generational_gc.cpp
 * @brief 世代別ガベージコレクションシステムの実装
 * @version 1.0.0
 * @license MIT
 */

#include "generational_gc.h"
#include <chrono>
#include <algorithm>
#include <iostream>

namespace aerojs {
namespace utils {
namespace memory {

// コンストラクタ
GenerationalGC::GenerationalGC(const GCConfig& config)
  : config(config),
    gcEnabled(true),
    concurrentMarkingActive(false),
    shouldStop(false)
{
  // 初期ヒープ確保
  youngGeneration.reserve(config.youngGenerationSize / 64); // 平均オブジェクトサイズ64バイトと仮定
  oldGeneration.reserve(config.oldGenerationSize / 128);   // 平均オブジェクトサイズ128バイトと仮定
  
  // 統計情報初期化
  stats.currentHeapSize = config.initialHeapSize;
  
  // GCスレッド開始
  if (config.enableConcurrentMark) {
    gcWorker = std::thread(&GenerationalGC::gcThread, this);
  }
}

// デストラクタ
GenerationalGC::~GenerationalGC() {
  // GCスレッド停止
  shouldStop = true;
  if (gcWorker.joinable()) {
    gcWorker.join();
  }
  
  // 全オブジェクト解放
  for (auto* obj : youngGeneration) {
    delete obj;
  }
  for (auto* obj : oldGeneration) {
    delete obj;
  }
  
  youngGeneration.clear();
  oldGeneration.clear();
  remembered.clear();
}

// メモリ割り当てテンプレート実装
template<typename T, typename... Args>
T* GenerationalGC::allocate(Args&&... args) {
  // メモリが足りなくなったらGC実行
  size_t objSize = sizeof(T);
  size_t youngGenSize = 0;
  for (auto* obj : youngGeneration) {
    youngGenSize += obj->getSize();
  }
  
  if (youngGenSize + objSize > config.youngGenerationSize) {
    minorCollection();
  }
  
  // それでも足りなければMajor GCを実行
  if (youngGenSize + objSize > config.youngGenerationSize) {
    majorCollection();
  }
  
  // オブジェクト生成
  T* newObj = new T(std::forward<Args>(args)...);
  
  // 若い世代に追加
  youngGeneration.push_back(newObj);
  
  // 統計情報更新
  stats.totalAllocatedBytes += objSize;
  stats.currentHeapSize += objSize;
  
  return newObj;
}

// 書き込みバリア (Value型)
void GenerationalGC::writeBarrier(GCCell* object, const core::runtime::Value& value) {
  // Valueが参照型でない場合は何もしない
  if (!value.isObject()) {
    return;
  }
  
  // 以下は実際の実装では正しく参照を取得する必要がある
  // （サンプルコードなのでここでは簡略化）
  GCCell* childObj = nullptr;
  
  if (childObj) {
    writeBarrier(object, childObj);
  }
}

// 書き込みバリア (オブジェクト間)
void GenerationalGC::writeBarrier(GCCell* parent, GCCell* child) {
  // 親が古い世代、子が若い世代の場合のみ記録
  // (世代間参照のトラッキング)
  bool parentInOld = std::find(oldGeneration.begin(), oldGeneration.end(), parent) != oldGeneration.end();
  bool childInYoung = std::find(youngGeneration.begin(), youngGeneration.end(), child) != youngGeneration.end();
  
  if (parentInOld && childInYoung) {
    remembered.insert(parent);
  }
}

// ガベージコレクション実行
void GenerationalGC::collectGarbage(bool forceMajor) {
  if (!gcEnabled) {
    return;
  }
  
  auto start = std::chrono::high_resolution_clock::now();
  
  if (forceMajor) {
    majorCollection();
  } else {
    minorCollection();
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  
  stats.totalGCTimeMs += duration;
  stats.longestPauseMs = std::max(stats.longestPauseMs, static_cast<uint64_t>(duration));
  stats.totalGCCount++;
}

// マイナーコレクション（若い世代のみ）
void GenerationalGC::minorCollection() {
  std::unique_lock<std::mutex> lock(gcMutex);
  
  stats.minorGCCount++;
  
  // マークフェーズ：ルートからの参照を辿る
  for (auto& root : roots) {
    if (*root && std::find(youngGeneration.begin(), youngGeneration.end(), *root) != youngGeneration.end()) {
      mark(*root);
    }
  }
  
  // 記憶セットからの参照を辿る
  for (auto* remembered : remembered) {
    mark(remembered);
  }
  
  // スイープフェーズ：マークされていないオブジェクトを回収
  size_t freedObjs = 0;
  size_t freedMem = 0;
  
  auto it = youngGeneration.begin();
  while (it != youngGeneration.end()) {
    auto* obj = *it;
    
    if (obj->state == CellState::White) {
      // 未マークなので回収
      freedMem += obj->getSize();
      freedObjs++;
      delete obj;
      it = youngGeneration.erase(it);
    } else {
      // マークされたオブジェクトは生存
      obj->age++;
      
      // 昇格条件を満たせば古い世代へ
      if (obj->age >= config.promotionAge) {
        promoteObject(obj);
        it = youngGeneration.erase(it);
      } else {
        // マークをリセット
        obj->state = CellState::White;
        ++it;
      }
    }
  }
  
  // 記憶セットもクリア
  remembered.clear();
  
  // 統計情報更新
  stats.freedObjects += freedObjs;
  stats.freedBytes += freedMem;
  stats.currentHeapSize -= freedMem;
}

// メジャーコレクション（全世代）
void GenerationalGC::majorCollection() {
  std::unique_lock<std::mutex> lock(gcMutex);
  
  stats.majorGCCount++;
  
  // マークフェーズ：ルートからの参照を辿る
  for (auto& root : roots) {
    if (*root) {
      mark(*root);
    }
  }
  
  // スイープフェーズ：マークされていないオブジェクトを回収
  size_t freedObjs = 0;
  size_t freedMem = 0;
  
  // 若い世代のスイープ
  auto yit = youngGeneration.begin();
  while (yit != youngGeneration.end()) {
    auto* obj = *yit;
    
    if (obj->state == CellState::White) {
      // 未マークなので回収
      freedMem += obj->getSize();
      freedObjs++;
      delete obj;
      yit = youngGeneration.erase(yit);
    } else {
      // マークされたオブジェクトは生存
      obj->age++;
      
      // 昇格条件を満たせば古い世代へ
      if (obj->age >= config.promotionAge) {
        promoteObject(obj);
        yit = youngGeneration.erase(yit);
      } else {
        // マークをリセット
        obj->state = CellState::White;
        ++yit;
      }
    }
  }
  
  // 古い世代のスイープ
  auto oit = oldGeneration.begin();
  while (oit != oldGeneration.end()) {
    auto* obj = *oit;
    
    if (obj->state == CellState::White) {
      // 未マークなので回収
      freedMem += obj->getSize();
      freedObjs++;
      delete obj;
      oit = oldGeneration.erase(oit);
    } else {
      // マークをリセット
      obj->state = CellState::White;
      ++oit;
    }
  }
  
  // 記憶セットクリア
  remembered.clear();
  
  // コンパクションが有効な場合は実行
  if (config.enableCompaction) {
    compact();
  }
  
  // 統計情報更新
  stats.freedObjects += freedObjs;
  stats.freedBytes += freedMem;
  stats.currentHeapSize -= freedMem;
}

// マーキング処理
void GenerationalGC::mark(GCCell* obj) {
  if (!obj || obj->state != CellState::White) {
    return;
  }
  
  // マーク処理（トライカラーマーキング）
  obj->state = CellState::Gray;
  
  // 子オブジェクトを辿る
  obj->trace(reinterpret_cast<class GarbageCollector*>(this));
  
  // 完全にマーク済みに
  obj->state = CellState::Black;
}

// 並行マーキング処理
void GenerationalGC::markConcurrent() {
  // このメソッドは別スレッドで実行される
  concurrentMarkingActive = true;
  
  // 現在のルートセットをコピー
  std::vector<GCCell**> rootsCopy;
  {
    std::unique_lock<std::mutex> lock(rootsMutex);
    rootsCopy = roots;
  }
  
  // ルートからマーキング
  for (auto& root : rootsCopy) {
    if (*root) {
      mark(*root);
    }
  }
  
  concurrentMarkingActive = false;
}

// オブジェクトのコンパクション処理
void GenerationalGC::compact() {
  // 簡略化のためここでは実装していない
  // 実際の実装ではオブジェクトの再配置とポインタの修正が必要
}

// オブジェクト昇格処理
void GenerationalGC::promoteObject(GCCell* object) {
  oldGeneration.push_back(object);
}

// ルート追加
void GenerationalGC::addRoot(GCCell** root) {
  std::unique_lock<std::mutex> lock(rootsMutex);
  roots.push_back(root);
}

// ルート削除
void GenerationalGC::removeRoot(GCCell** root) {
  std::unique_lock<std::mutex> lock(rootsMutex);
  roots.erase(std::remove(roots.begin(), roots.end(), root), roots.end());
}

// GC有効化/無効化
void GenerationalGC::enableGC(bool enable) {
  gcEnabled = enable;
}

// メジャーGCスケジュール
void GenerationalGC::scheduleMajorGC() {
  if (config.enableConcurrentMark) {
    // 別スレッドでGCを実行
    std::thread([this]() {
      majorCollection();
    }).detach();
  } else {
    majorCollection();
  }
}

// マイナーGCスケジュール
void GenerationalGC::scheduleMinorGC() {
  if (config.enableConcurrentMark) {
    // 別スレッドでGCを実行
    std::thread([this]() {
      minorCollection();
    }).detach();
  } else {
    minorCollection();
  }
}

// GCワーカースレッド処理
void GenerationalGC::gcThread() {
  while (!shouldStop) {
    // マイナーGC定期実行
    std::this_thread::sleep_for(std::chrono::milliseconds(config.minorGCInterval));
    if (shouldStop) break;
    
    scheduleMinorGC();
    
    // メジャーGC定期実行
    static int minorCount = 0;
    if (++minorCount >= config.majorGCInterval / config.minorGCInterval) {
      minorCount = 0;
      scheduleMajorGC();
    }
  }
}

} // namespace memory
} // namespace utils
} // namespace aerojs 