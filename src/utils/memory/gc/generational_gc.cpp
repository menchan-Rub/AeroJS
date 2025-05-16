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
  
  // Valueオブジェクトから内部ポインタを取得
  GCCell* childObj = nullptr;
  
  // タグ付きポインタから実際のオブジェクトを取得
  if (value.isObject()) {
    childObj = static_cast<GCCell*>(value.asObject());
  } else if (value.isString()) {
    childObj = static_cast<GCCell*>(value.asString());
  } else if (value.isArray()) {
    childObj = static_cast<GCCell*>(value.asArray());
  } else if (value.isFunction()) {
    childObj = static_cast<GCCell*>(value.asFunction());
  }
  
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
void GenerationalGC::promoteObject(GCCell* obj) {
  // 古い世代へ移動
  oldGeneration.push_back(obj);
  
  // 世代情報を更新
  obj->generation = Generation::Old;
  
  // メトリクスを更新
  stats.promotedObjects++;
  stats.promotedBytes += obj->getSize();
  
  // 実際の実装ではオブジェクトの再配置とポインタの修正が必要
  // 1. 移動先のメモリ領域を確保
  void* newLocation = nullptr;
  size_t objSize = obj->getSize();
  
  // 古い世代用のメモリプールから割り当て
  // ここではシンプルにmallocを使用しているが、実際には専用アロケータを使用
  newLocation = malloc(objSize);
  if (!newLocation) {
    // メモリ確保に失敗した場合の処理
    // エラーログを出力し、昇格をキャンセル
    std::cerr << "メモリ確保に失敗しました。サイズ: " << objSize << " バイト" << std::endl;
    return;
  }
  
  // 2. オブジェクトをコピー
  memcpy(newLocation, obj, objSize);
  
  // 3. フォワーディングポインタを設定
  // 元のオブジェクトに新しい場所を記録
  obj->forwardingAddress = newLocation;
  
  // 4. 参照を更新
  // 一時的に新しいオブジェクトを取得
  GCCell* newObj = static_cast<GCCell*>(newLocation);
  
  // 5. 内部の参照を修正（再帰的にforwardingAddressをチェック）
  newObj->visitMutableReferences([this](GCCell** ref) {
    if (*ref && (*ref)->forwardingAddress) {
      // 参照先が既に移動していれば、フォワーディングポインタに更新
      *ref = static_cast<GCCell*>((*ref)->forwardingAddress);
    }
  });
  
  // 6. グローバルレジストリの参照を更新
  updateReferences(obj, newObj);
  
  // 7. 統計情報を更新
  stats.relocatedObjects++;
  stats.relocatedBytes += objSize;
}

// 全ての参照を更新
void GenerationalGC::updateReferences(GCCell* oldPtr, GCCell* newPtr) {
  // ルート参照の更新
  for (auto& root : roots) {
    if (*root == oldPtr) {
      *root = newPtr;
    }
  }
  
  // 弱参照の更新
  for (auto& weakRef : weakRefs) {
    if (weakRef.target == oldPtr) {
      weakRef.target = newPtr;
    }
  }
  
  // 管理対象オブジェクト内の参照も更新
  for (auto* managedObj : youngGeneration) {
    if (managedObj != oldPtr) { // 自分自身は更新しない
      managedObj->visitMutableReferences([oldPtr, newPtr](GCCell** ref) {
        if (*ref == oldPtr) {
          *ref = newPtr;
        }
      });
    }
  }
  
  for (auto* managedObj : oldGeneration) {
    if (managedObj != oldPtr) { // 自分自身は更新しない
      managedObj->visitMutableReferences([oldPtr, newPtr](GCCell** ref) {
        if (*ref == oldPtr) {
          *ref = newPtr;
        }
      });
    }
  }
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

// 弱参照管理
WeakRef* GenerationalGC::createWeakRef(GCCell* target) {
  if (!target) {
    return nullptr;
  }
  
  std::lock_guard<std::mutex> lock(gcMutex);
  
  // 新しい弱参照を作成
  weakRefs.emplace_back(target);
  
  // 最後に追加した要素へのポインタを返す
  return &weakRefs.back();
}

void GenerationalGC::releaseWeakRef(WeakRef* ref) {
  if (!ref) {
    return;
  }
  
  std::lock_guard<std::mutex> lock(gcMutex);
  
  // 指定された弱参照を検索
  for (auto it = weakRefs.begin(); it != weakRefs.end(); ++it) {
    if (&(*it) == ref) {
      // 見つかったら削除
      weakRefs.erase(it);
      return;
    }
  }
}

// GCスレッド処理
void GenerationalGC::gcThread() {
  auto lastMinorGC = std::chrono::steady_clock::now();
  auto lastMajorGC = lastMinorGC;
  
  while (!shouldStop) {
    auto now = std::chrono::steady_clock::now();
    
    // マイナーGCの実行判定
    if (gcEnabled && 
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMinorGC).count() >= config.minorGCInterval) {
      minorCollection();
      lastMinorGC = std::chrono::steady_clock::now();
    }
    
    // メジャーGCの実行判定
    if (gcEnabled && 
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMajorGC).count() >= config.majorGCInterval) {
      majorCollection();
      lastMajorGC = std::chrono::steady_clock::now();
    }
    
    // スリープ（短時間）
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

} // namespace memory
} // namespace utils
} // namespace aerojs 