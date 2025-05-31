/**
 * @file parallel_gc.h
 * @brief 超高性能並列ガベージコレクションシステム
 * @version 2.0.0
 * @license MIT
 */

#pragma once

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <condition_variable>
#include <queue>
#include <optional>
#include <chrono>
#include <array>
#include <bitset>

#include "../../../core/runtime/values/value.h"
#include "../allocators/memory_allocator.h"
#include "generational_gc.h"

namespace aerojs {
namespace utils {
namespace memory {

// 拡張GC設定
struct ParallelGCConfig {
  // 基本設定
  size_t initialHeapSize = 8 * 1024 * 1024;    // 初期ヒープサイズ（8MB）
  size_t maxHeapSize = 4 * 1024 * 1024 * 1024; // 最大ヒープサイズ（4GB）
  
  // 世代設定
  size_t nurserySize = 2 * 1024 * 1024;        // ナーサリー（新生代）サイズ（2MB）
  size_t youngGenSize = 16 * 1024 * 1024;      // ヤング世代サイズ（16MB）
  size_t mediumGenSize = 64 * 1024 * 1024;     // ミディアム世代サイズ（64MB）
  
  // 昇格設定
  uint8_t nurseryToYoungAge = 1;               // ナーサリーからヤング世代への昇格年齢
  uint8_t youngToMediumAge = 3;                // ヤング世代からミディアム世代への昇格年齢
  uint8_t mediumToOldAge = 5;                  // ミディアム世代からオールド世代への昇格年齢
  
  // 並列処理設定
  uint32_t workerThreadCount = 0;              // ワーカースレッド数（0は自動）
  uint32_t markingQuantum = 10;                // マーキング量子（ms）
  uint32_t mutatorQuantum = 5;                 // ミューテータ量子（ms）
  uint32_t markingStepSize = 1024;             // 1ステップあたりのマーキング数
  
  // GC間隔設定
  uint32_t minorGCInterval = 500;              // マイナーGC間隔（ms）
  uint32_t mediumGCInterval = 5000;            // ミディアムGC間隔（ms）
  uint32_t majorGCInterval = 30000;            // メジャーGC間隔（ms）
  
  // GCトリガー設定
  float minorGCTriggerRatio = 0.7f;            // マイナーGCトリガー比率
  float mediumGCTriggerRatio = 0.6f;           // ミディアムGCトリガー比率
  float majorGCTriggerRatio = 0.5f;            // メジャーGCトリガー比率
  
  // 機能設定
  bool enableIncrementalMarking = true;        // インクリメンタルマーキング有効化
  bool enableConcurrentMarking = true;         // 並行マーキング有効化
  bool enableConcurrentSweeping = true;        // 並行スイーピング有効化
  bool enableCompaction = true;                // コンパクション有効化
  bool enablePreciseSweeping = true;           // 精密スイーピング有効化
  bool enableAdaptiveCollection = true;        // 適応的コレクション有効化
  bool enableLazyReferences = true;            // 遅延参照追跡有効化
  bool enablePredictiveCollection = true;      // 予測的コレクション有効化
  bool enableLargeObjectSpace = true;          // 大きいオブジェクト特別領域有効化
  
  // バッファ設定
  size_t writeBarrierBufferSize = 4096;        // 書き込みバリアバッファサイズ
  size_t markingWorkQueueSize = 8192;          // マーキングワークキューサイズ
  
  // メモリサイズしきい値
  size_t largeObjectThreshold = 32 * 1024;     // 大きいオブジェクトしきい値（32KB）
  
  // 緊急GC設定
  float emergencyGCHeapRatio = 0.95f;          // 緊急GCヒープ比率
  float heapGrowthFactor = 1.5f;               // ヒープ拡大係数
};

// 拡張世代区分
enum class ExtendedGeneration {
  Nursery,   // ナーサリー（新生代）
  Young,     // ヤング世代
  Medium,    // ミディアム世代
  Old,       // オールド世代
  LargeObj   // 大きいオブジェクト
};

// GC種別
enum class GCType {
  Minor,     // マイナーGC（ナーサリーとヤング）
  Medium,    // ミディアムGC（ナーサリー、ヤング、ミディアム）
  Major      // メジャーGC（全世代）
};

// GC原因
enum class GCCause {
  Allocation,          // 割り当て失敗
  Scheduled,           // スケジュール実行
  ExplicitRequest,     // 明示的リクエスト
  LowMemory,           // メモリ不足
  Idle,                // アイドル状態
  MemoryPressure,      // メモリ圧迫
  MetricsCollection,   // メトリクス収集
  PreventiveCollection // 予防的コレクション
};

// 拡張GC統計情報
struct ParallelGCStats : public GCStats {
  // 追加統計情報
  size_t concurrentMarkingPasses = 0;          // 並行マーキングパス数
  size_t incrementalMarkingPasses = 0;         // インクリメンタルマーキングパス数
  
  uint64_t totalMarkingTimeMs = 0;             // マーキング合計時間
  uint64_t totalSweepingTimeMs = 0;            // スイーピング合計時間
  uint64_t totalCompactionTimeMs = 0;          // コンパクション合計時間
  uint64_t totalPromotionTimeMs = 0;           // 昇格合計時間
  
  size_t totalIncrementalMarkSteps = 0;        // インクリメンタルマーキングステップ数
  size_t totalConcurrentMarkSteps = 0;         // 並行マーキングステップ数
  
  size_t writeBarrierInvocations = 0;          // 書き込みバリア呼び出し数
  size_t cardTableUpdates = 0;                 // カードテーブル更新数
  size_t rememberSetEntries = 0;               // 記憶セットエントリ数
  
  // GCカウント統計
  size_t minorGCCount = 0;                     // マイナーGC実行回数
  size_t mediumGCCount = 0;                    // ミディアムGC実行回数
  size_t majorGCCount = 0;                     // メジャーGC実行回数
  uint64_t totalMinorGCTimeMs = 0;             // マイナーGC合計時間
  uint64_t totalMediumGCTimeMs = 0;            // ミディアムGC合計時間
  uint64_t totalMajorGCTimeMs = 0;             // メジャーGC合計時間
  size_t promotionCount = 0;                   // オブジェクト昇格回数
  
  // 世代別統計
  std::array<size_t, 5> generationObjectCount = {0}; // 世代別オブジェクト数
  std::array<size_t, 5> generationByteSize = {0};    // 世代別バイトサイズ
  std::array<size_t, 5> generationFreedObjects = {0}; // 世代別解放オブジェクト数
  std::array<size_t, 5> generationFreedBytes = {0};   // 世代別解放バイト数
  
  // パフォーマンス指標
  float pauseTimeRatio = 0.0f;                 // 停止時間比率
  float throughput = 0.0f;                     // スループット
  float allocationRate = 0.0f;                 // 割り当てレート（バイト/秒）
  float promotionRate = 0.0f;                  // 昇格レート（バイト/秒）
  
  // 最後のGC情報
  GCType lastGCType = GCType::Minor;           // 最後のGC種別
  GCCause lastGCCause = GCCause::Scheduled;    // 最後のGC原因
  uint64_t lastGCTimestamp = 0;                // 最後のGCタイムスタンプ
  uint64_t lastGCDurationMs = 0;               // 最後のGC所要時間
  
  // ヒープ使用状況
  float heapUsageRatio = 0.0f;                 // ヒープ使用率
  bool isHeapFragmented = false;               // ヒープ断片化フラグ
  float fragmentationRatio = 0.0f;             // 断片化率
};

// カード表
class CardTable {
public:
  CardTable(size_t heapSize, size_t cardSize = 512);
  ~CardTable();
  
  void markCard(void* ptr);
  bool isCardMarked(void* ptr) const;
  void clearCard(void* ptr);
  void clearAll();
  
  uint8_t* cardFor(void* ptr) const;
  size_t getCardCount() const { return m_cardCount; }
  
private:
  uint8_t* m_cards;
  size_t m_cardCount;
  size_t m_cardSize;
  size_t m_heapSize;
  size_t m_heapStart;
};

// 記憶セット（リメンバーセット）
class RememberSet {
public:
  RememberSet();
  ~RememberSet();
  
  void add(GCCell* fromObject, GCCell* toObject);
  void remove(GCCell* fromObject, GCCell* toObject);
  bool contains(GCCell* fromObject, GCCell* toObject) const;
  const std::unordered_set<GCCell*>& getReferencesFrom(GCCell* obj) const;
  const std::unordered_set<GCCell*>& getReferencesTo(GCCell* obj) const;
  void clear();
  
private:
  std::unordered_map<GCCell*, std::unordered_set<GCCell*>> m_fromToRefs;
  std::unordered_map<GCCell*, std::unordered_set<GCCell*>> m_toFromRefs;
  mutable std::mutex m_mutex;
};

// ワークスティーリングキュー（作業盗取型キュー）
template<typename T>
class WorkStealingQueue {
public:
  WorkStealingQueue(size_t capacity = 1024);
  ~WorkStealingQueue();
  
  bool push(const T& item);
  bool pop(T& item);
  bool steal(T& item);
  size_t size() const;
  bool empty() const;
  void clear();
  
private:
  std::vector<T> m_buffer;
  std::atomic<size_t> m_head;
  std::atomic<size_t> m_tail;
  size_t m_capacity;
  mutable std::mutex m_mutex;
};

// 並列ガベージコレクタクラス
class ParallelGC {
public:
  ParallelGC(const ParallelGCConfig& config = ParallelGCConfig());
  ~ParallelGC();
  
  // メモリ割り当て
  template<typename T, typename... Args>
  T* allocate(Args&&... args);
  
  // 大きいオブジェクト割り当て
  template<typename T, typename... Args>
  T* allocateLarge(Args&&... args);
  
  // 値の記録（書き込みバリア）
  void writeBarrier(GCCell* object, const aerojs::core::runtime::Value& value);
  void writeBarrier(GCCell* parent, GCCell* child);
  
  // GC実行
  void collectGarbage(GCType type, GCCause cause);
  void minorCollection(GCCause cause = GCCause::Scheduled);
  void mediumCollection(GCCause cause = GCCause::Scheduled);
  void majorCollection(GCCause cause = GCCause::Scheduled);
  
  // インクリメンタルマーキングステップ
  void incrementalMarkingStep(size_t stepSize = 0);
  
  // GC制御
  void enableGC(bool enable);
  void scheduleCollection(GCType type, uint32_t delayMs = 0);
  void scheduleMinorGC(uint32_t delayMs = 0);
  void scheduleMediumGC(uint32_t delayMs = 0);
  void scheduleMajorGC(uint32_t delayMs = 0);
  
  // 統計情報
  const ParallelGCStats& getStats() const { return m_stats; }
  
  // 弱参照管理
  WeakRef* createWeakRef(GCCell* target);
  void releaseWeakRef(WeakRef* ref);
  
  // ヒープ情報
  size_t getHeapSize() const;
  size_t getUsedMemory() const;
  float getHeapUsageRatio() const;
  
  // デバッグ・テスト用
  void verifyHeap() const;
  void dumpHeapStats() const;
  
private:
  // ワーカースレッド管理
  void initWorkerThreads();
  void shutdownWorkerThreads();
  void workerThreadMain(int threadId);
  
  // GC内部フェーズ
  void prepareCollection(GCType type);
  void markRoots();
  void markConcurrent();
  void markIncrementalStep(size_t stepSize);
  void finishMarking();
  void sweep(bool concurrent);
  void compact();
  void promoteObjects();
  
  // GC内部処理
  void mark(GCCell* root);
  void updateReferences(GCCell* oldPtr, GCCell* newPtr);
  void processMarkingWorkQueue(int threadId);
  bool stealWork(int threadId, GCCell*& cell);
  
  // メモリ管理
  void* allocateRaw(size_t size, ExtendedGeneration gen);
  void freeRaw(void* ptr, size_t size);
  void expandHeap(size_t additionalSize);
  
  // 世代管理
  void addToGeneration(GCCell* cell, ExtendedGeneration gen);
  void promoteObject(GCCell* object, ExtendedGeneration targetGen);
  
  // ルート設定
  void addRoot(GCCell** root);
  void removeRoot(GCCell** root);
  
  // 定期GC実行・監視スレッド
  void gcSupervisorThread();
  void scheduleGC(GCType type, uint32_t delayMs);
  
  // 適応的GC
  void updateGCMetrics();
  void adjustGCParameters();
  GCType determineGCType();
  
  // メンバ変数
  ParallelGCConfig m_config;
  ParallelGCStats m_stats;
  
  // メモリ領域
  std::vector<GCCell*> m_nurseryGen;
  std::vector<GCCell*> m_youngGen;
  std::vector<GCCell*> m_mediumGen;
  std::vector<GCCell*> m_oldGen;
  std::unordered_set<GCCell*> m_largeObjects;
  
  // 記憶セットとカード表
  std::unique_ptr<RememberSet> m_rememberSet;
  std::unique_ptr<CardTable> m_cardTable;
  
  // マーキングキュー
  std::vector<std::unique_ptr<WorkStealingQueue<GCCell*>>> m_markingQueues;
  
  // ルートオブジェクト
  std::vector<GCCell**> m_roots;
  std::mutex m_rootsMutex;
  
  // 弱参照
  std::vector<WeakRef> m_weakRefs;
  std::mutex m_weakRefMutex;
  
  // ワーカースレッド管理
  std::vector<std::thread> m_workerThreads;
  std::atomic<bool> m_workersActive;
  std::atomic<bool> m_shuttingDown;
  std::mutex m_workerMutex;
  std::condition_variable m_workerCV;
  
  // GC制御
  std::atomic<bool> m_gcEnabled;
  std::atomic<bool> m_collectionInProgress;
  std::atomic<bool> m_concurrentMarkingActive;
  std::atomic<bool> m_incrementalMarkingActive;
  
  // GCスーパーバイザースレッド
  std::thread m_supervisorThread;
  std::mutex m_scheduleMutex;
  std::condition_variable m_scheduleCV;
  std::queue<std::pair<GCType, uint64_t>> m_scheduledGCs;
  
  // アロケータ
  std::unique_ptr<allocators::MemoryAllocator> m_allocator;
  
  // GC同期
  std::mutex m_gcMutex;
  std::condition_variable m_gcCV;
  
  // GC状態
  GCType m_currentGCType;
  GCCause m_currentGCCause;
  std::chrono::steady_clock::time_point m_lastGCTime;
  
  // GCバリア
  struct SyncBarrier {
    std::mutex mutex;
    std::condition_variable cv;
    int count;
    int total;
    
    SyncBarrier(int threadCount) : count(0), total(threadCount) {}
    
    void wait() {
      std::unique_lock<std::mutex> lock(mutex);
      if (++count == total) {
        count = 0;
        cv.notify_all();
      } else {
        cv.wait(lock, [this] { return count == 0; });
      }
    }
  };
  
  std::unique_ptr<SyncBarrier> m_barrier;
  
  friend class GCCell;
};

} // namespace memory
} // namespace utils
} // namespace aerojs 