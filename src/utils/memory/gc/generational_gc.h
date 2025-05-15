/**
 * @file generational_gc.h
 * @brief 世代別ガベージコレクションシステム
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include <vector>
#include <unordered_set>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include "../../../core/runtime/values/value.h"

namespace aerojs {
namespace utils {
namespace memory {

// GCセルの状態
enum class CellState {
  White, // 未マーク
  Gray,  // マーク中
  Black  // マーク済み
};

// GCセル（ガベージコレクション管理対象）
class GCCell {
public:
  GCCell() : state(CellState::White), age(0) {}
  virtual ~GCCell() = default;
  
  virtual void trace(class GarbageCollector* gc) = 0;
  virtual size_t getSize() const = 0;
  
  CellState state;
  uint8_t age;
};

// 世代の定義
enum class Generation {
  Young, // 若い世代（新しいオブジェクト）
  Old    // 古い世代（長寿命オブジェクト）
};

// ガベージコレクション設定
struct GCConfig {
  size_t initialHeapSize = 4 * 1024 * 1024;   // 初期ヒープサイズ（4MB）
  size_t maxHeapSize = 1024 * 1024 * 1024;    // 最大ヒープサイズ（1GB）
  size_t youngGenerationSize = 1024 * 1024;   // 若い世代のサイズ（1MB）
  size_t oldGenerationSize = 8 * 1024 * 1024; // 古い世代の初期サイズ（8MB）
  
  uint8_t promotionAge = 3;                   // 昇格年齢（何回GC生き残ったら昇格させるか）
  float heapGrowthFactor = 1.5f;              // ヒープ拡大係数
  float gcTriggerRatio = 0.75f;               // GC開始トリガー比率
  
  bool enableConcurrentMark = true;           // 並行マーキングの有効化
  bool enableGenerational = true;             // 世代別GCの有効化
  bool enableCompaction = true;               // コンパクションの有効化
  
  uint32_t minorGCInterval = 1000;            // マイナーGC間隔（ms）
  uint32_t majorGCInterval = 10000;           // メジャーGC間隔（ms）
};

// ガベージコレクションの統計情報
struct GCStats {
  size_t totalAllocatedBytes = 0;             // 合計割り当てバイト数
  size_t currentHeapSize = 0;                 // 現在のヒープサイズ
  size_t totalGCCount = 0;                    // GC実行回数
  size_t minorGCCount = 0;                    // マイナーGC回数
  size_t majorGCCount = 0;                    // メジャーGC回数
  
  uint64_t totalGCTimeMs = 0;                 // GC合計時間（ms）
  uint64_t longestPauseMs = 0;                // 最長停止時間（ms）
  
  size_t freedObjects = 0;                    // 解放されたオブジェクト数
  size_t freedBytes = 0;                      // 解放されたバイト数
};

// 世代別ガベージコレクタークラス
class GenerationalGC {
public:
  GenerationalGC(const GCConfig& config = GCConfig());
  ~GenerationalGC();
  
  // メモリ割り当て
  template<typename T, typename... Args>
  T* allocate(Args&&... args);
  
  // 値の記録
  void writeBarrier(GCCell* object, const core::runtime::Value& value);
  void writeBarrier(GCCell* parent, GCCell* child);
  
  // GC実行
  void collectGarbage(bool forceMajor = false);
  void minorCollection();
  void majorCollection();
  
  // GC制御
  void enableGC(bool enable);
  void scheduleMajorGC();
  void scheduleMinorGC();
  
  // 統計情報
  const GCStats& getStats() const { return stats; }
  
private:
  // GC内部処理
  void mark(GCCell* root);
  void markConcurrent();
  void sweep();
  void compact();
  void promoteObject(GCCell* object);
  
  // ルート設定
  void addRoot(GCCell** root);
  void removeRoot(GCCell** root);
  
  // 定期GC実行スレッド
  void gcThread();
  
  // メンバ変数
  GCConfig config;
  GCStats stats;
  
  std::vector<GCCell*> youngGeneration;
  std::vector<GCCell*> oldGeneration;
  std::unordered_set<GCCell*> remembered;
  
  std::vector<GCCell**> roots;
  std::mutex rootsMutex;
  
  std::atomic<bool> gcEnabled;
  std::atomic<bool> concurrentMarkingActive;
  std::atomic<bool> shouldStop;
  
  std::thread gcWorker;
  std::mutex gcMutex;
  
  friend class GCCell;
};

} // namespace memory
} // namespace utils
} // namespace aerojs 