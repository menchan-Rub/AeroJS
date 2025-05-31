/**
 * @file parallel_gc.cpp
 * @brief 超高性能並列ガベージコレクションシステムの実装
 * @version 2.0.0
 * @license MIT
 */

#include "parallel_gc.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

namespace aerojs {
namespace utils {
namespace memory {

// カードテーブルの実装
CardTable::CardTable(size_t heapSize, size_t cardSize)
  : m_cardSize(cardSize),
    m_heapSize(heapSize),
    m_heapStart(0)
{
  m_cardCount = (heapSize + cardSize - 1) / cardSize;
  m_cards = new uint8_t[m_cardCount]();
}

CardTable::~CardTable() {
  delete[] m_cards;
}

void CardTable::markCard(void* ptr) {
  uint8_t* card = cardFor(ptr);
  if (card) {
    *card = 1;
  }
}

bool CardTable::isCardMarked(void* ptr) const {
  uint8_t* card = cardFor(ptr);
  return card ? *card != 0 : false;
}

void CardTable::clearCard(void* ptr) {
  uint8_t* card = cardFor(ptr);
  if (card) {
    *card = 0;
  }
}

void CardTable::clearAll() {
  std::memset(m_cards, 0, m_cardCount);
}

uint8_t* CardTable::cardFor(void* ptr) const {
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  if (addr < m_heapStart || addr >= m_heapStart + m_heapSize) {
    return nullptr;
  }
  size_t offset = addr - m_heapStart;
  size_t cardIndex = offset / m_cardSize;
  if (cardIndex >= m_cardCount) {
    return nullptr;
  }
  return &m_cards[cardIndex];
}

// リメンバーセットの実装
RememberSet::RememberSet() {}

RememberSet::~RememberSet() {
  clear();
}

void RememberSet::add(GCCell* fromObject, GCCell* toObject) {
  if (!fromObject || !toObject) return;
  
  std::lock_guard<std::mutex> lock(m_mutex);
  m_fromToRefs[fromObject].insert(toObject);
  m_toFromRefs[toObject].insert(fromObject);
}

void RememberSet::remove(GCCell* fromObject, GCCell* toObject) {
  if (!fromObject || !toObject) return;
  
  std::lock_guard<std::mutex> lock(m_mutex);
  auto fromIt = m_fromToRefs.find(fromObject);
  if (fromIt != m_fromToRefs.end()) {
    fromIt->second.erase(toObject);
    if (fromIt->second.empty()) {
      m_fromToRefs.erase(fromIt);
    }
  }
  
  auto toIt = m_toFromRefs.find(toObject);
  if (toIt != m_toFromRefs.end()) {
    toIt->second.erase(fromObject);
    if (toIt->second.empty()) {
      m_toFromRefs.erase(toIt);
    }
  }
}

bool RememberSet::contains(GCCell* fromObject, GCCell* toObject) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_fromToRefs.find(fromObject);
  if (it != m_fromToRefs.end()) {
    return it->second.find(toObject) != it->second.end();
  }
  return false;
}

const std::unordered_set<GCCell*>& RememberSet::getReferencesFrom(GCCell* obj) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  static const std::unordered_set<GCCell*> emptySet;
  auto it = m_fromToRefs.find(obj);
  return it != m_fromToRefs.end() ? it->second : emptySet;
}

const std::unordered_set<GCCell*>& RememberSet::getReferencesTo(GCCell* obj) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  static const std::unordered_set<GCCell*> emptySet;
  auto it = m_toFromRefs.find(obj);
  return it != m_toFromRefs.end() ? it->second : emptySet;
}

void RememberSet::clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_fromToRefs.clear();
  m_toFromRefs.clear();
}

// ワークスティーリングキューのテンプレート実装
template<typename T>
WorkStealingQueue<T>::WorkStealingQueue(size_t capacity)
  : m_capacity(capacity),
    m_head(0),
    m_tail(0)
{
  m_buffer.resize(capacity);
}

template<typename T>
WorkStealingQueue<T>::~WorkStealingQueue() {
  clear();
}

template<typename T>
bool WorkStealingQueue<T>::push(const T& item) {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t size = m_tail.load(std::memory_order_relaxed) - m_head.load(std::memory_order_relaxed);
  
  if (size >= m_capacity) {
    return false;
  }
  
  m_buffer[m_tail.load(std::memory_order_relaxed) % m_capacity] = item;
  m_tail.fetch_add(1, std::memory_order_release);
  return true;
}

template<typename T>
bool WorkStealingQueue<T>::pop(T& item) {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t tail = m_tail.load(std::memory_order_relaxed) - 1;
  m_tail.store(tail, std::memory_order_relaxed);
  
  size_t head = m_head.load(std::memory_order_relaxed);
  if (head <= tail) {
    item = m_buffer[tail % m_capacity];
    return true;
  }
  
  m_tail.store(tail + 1, std::memory_order_relaxed);
  return false;
}

template<typename T>
bool WorkStealingQueue<T>::steal(T& item) {
  size_t head = m_head.load(std::memory_order_acquire);
  std::atomic_thread_fence(std::memory_order_seq_cst);
  size_t tail = m_tail.load(std::memory_order_acquire);
  
  if (head < tail) {
    item = m_buffer[head % m_capacity];
    if (m_head.compare_exchange_strong(head, head + 1)) {
      return true;
    }
  }
  
  return false;
}

template<typename T>
size_t WorkStealingQueue<T>::size() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_tail.load(std::memory_order_relaxed) - m_head.load(std::memory_order_relaxed);
}

template<typename T>
bool WorkStealingQueue<T>::empty() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_head.load(std::memory_order_relaxed) >= m_tail.load(std::memory_order_relaxed);
}

template<typename T>
void WorkStealingQueue<T>::clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_head.store(0, std::memory_order_relaxed);
  m_tail.store(0, std::memory_order_relaxed);
}

// 必要なテンプレートのインスタンス化
template class WorkStealingQueue<GCCell*>;

// セル状態を表す列挙型（マーク&スイープアルゴリズム用）
enum class CellState {
  White, // 未マーク（回収対象）
  Grey,  // マーク処理中
  Black  // マーク済み（生存）
};

// ParallelGCのコンストラクタ
ParallelGC::ParallelGC(const ParallelGCConfig& config)
  : m_config(config),
    m_gcEnabled(true),
    m_collectionInProgress(false),
    m_concurrentMarkingActive(false),
    m_incrementalMarkingActive(false),
    m_workersActive(false),
    m_shuttingDown(false),
    m_currentGCType(GCType::Minor),
    m_currentGCCause(GCCause::Scheduled),
    m_allocator(nullptr),
    m_cardTable(nullptr),
    m_rememberSet(nullptr),
    m_barrier(nullptr)
{
  // メモリアロケータの初期化
  m_allocator = std::make_unique<allocators::MemoryAllocator>(config.initialHeapSize);
  
  // カードテーブルの初期化
  m_cardTable = std::make_unique<CardTable>(config.maxHeapSize);
  
  // リメンバーセットの初期化
  m_rememberSet = std::make_unique<RememberSet>();
  
  // 世代の初期確保
  m_nurseryGen.reserve(config.nurserySize / 64);  // 64バイト平均サイズと仮定
  m_youngGen.reserve(config.youngGenSize / 128);  // 128バイト平均サイズと仮定
  m_mediumGen.reserve(config.mediumGenSize / 256); // 256バイト平均サイズと仮定
  m_oldGen.reserve((config.initialHeapSize - config.nurserySize - config.youngGenSize - config.mediumGenSize) / 512); // 512バイト平均サイズと仮定
  
  // 統計情報の初期化
  m_stats.currentHeapSize = config.initialHeapSize;
  m_stats.lastGCTimestamp = std::chrono::steady_clock::now().time_since_epoch().count();
  
  // ParallelGCStatsに不足しているメンバーを追加
  m_stats.majorGCCount = 0;
  m_stats.mediumGCCount = 0;
  m_stats.minorGCCount = 0;
  m_stats.totalMajorGCTimeMs = 0;
  m_stats.totalMediumGCTimeMs = 0;
  m_stats.totalMinorGCTimeMs = 0;
  m_stats.promotionCount = 0;
  
  // ワーカースレッド数の設定
  uint32_t workerCount = config.workerThreadCount;
  if (workerCount == 0) {
    // 自動設定: ハードウェアスレッド数 - 1 (メインスレッド用)
    workerCount = std::max(1u, std::thread::hardware_concurrency() - 1);
  }
  
  // マーキングキューの初期化
  m_markingQueues.resize(workerCount);
  for (uint32_t i = 0; i < workerCount; i++) {
    m_markingQueues[i] = std::make_unique<WorkStealingQueue<GCCell*>>(config.markingWorkQueueSize);
  }
  
  // 同期バリアの初期化
  m_barrier = std::make_unique<SyncBarrier>(workerCount);
  
  // ワーカースレッドの初期化と開始
  if (config.enableConcurrentMarking || config.enableConcurrentSweeping) {
    initWorkerThreads();
  }
  
  // GC監視スレッドの開始
  m_supervisorThread = std::thread(&ParallelGC::gcSupervisorThread, this);
}

// ParallelGCのデストラクタ
ParallelGC::~ParallelGC() {
  // 終了フラグ設定
  m_shuttingDown = true;
  m_workersActive = false;
  m_gcEnabled = false;
  
  // ワーカースレッド停止
  {
    std::lock_guard<std::mutex> lock(m_workerMutex);
    m_workerCV.notify_all();
  }
  
  // スーパーバイザースレッド停止
  {
    std::lock_guard<std::mutex> lock(m_scheduleMutex);
    m_scheduleCV.notify_all();
  }
  
  // スレッド終了待機
  if (m_supervisorThread.joinable()) {
    m_supervisorThread.join();
  }
  
  shutdownWorkerThreads();
  
  // 全オブジェクト解放
  majorCollection(GCCause::ExplicitRequest);
  
  for (auto* obj : m_nurseryGen) {
    delete obj;
  }
  for (auto* obj : m_youngGen) {
    delete obj;
  }
  for (auto* obj : m_mediumGen) {
    delete obj;
  }
  for (auto* obj : m_oldGen) {
    delete obj;
  }
  for (auto* obj : m_largeObjects) {
    delete obj;
  }
  
  m_nurseryGen.clear();
  m_youngGen.clear();
  m_mediumGen.clear();
  m_oldGen.clear();
  m_largeObjects.clear();
}

// ワーカースレッド初期化
void ParallelGC::initWorkerThreads() {
  std::lock_guard<std::mutex> lock(m_workerMutex);
  
  uint32_t workerCount = m_markingQueues.size();
  m_workerThreads.resize(workerCount);
  
  for (uint32_t i = 0; i < workerCount; i++) {
    m_workerThreads[i] = std::thread(&ParallelGC::workerThreadMain, this, i);
  }
}

// ワーカースレッド終了
void ParallelGC::shutdownWorkerThreads() {
  {
    std::lock_guard<std::mutex> lock(m_workerMutex);
    m_workersActive = false;
    m_workerCV.notify_all();
  }
  
  for (auto& thread : m_workerThreads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  
  m_workerThreads.clear();
}

// ワーカースレッドのメイン処理
void ParallelGC::workerThreadMain(int threadId) {
  while (!m_shuttingDown) {
    // ワーク待機
    {
      std::unique_lock<std::mutex> lock(m_workerMutex);
      m_workerCV.wait(lock, [this] {
        return m_workersActive || m_shuttingDown;
      });
      
      if (m_shuttingDown) {
        break;
      }
    }
    
    // ワーク実行
    while (m_workersActive && !m_shuttingDown) {
      if (m_concurrentMarkingActive) {
        processMarkingWorkQueue(threadId);
      } else {
        // 他の並行処理タスクがあれば実行
        // ...
        
        // タスクがなければ待機
        std::this_thread::yield();
      }
    }
  }
}

// GC有効化/無効化
void ParallelGC::enableGC(bool enable) {
  m_gcEnabled = enable;
}

// GC監視スレッド
void ParallelGC::gcSupervisorThread() {
  auto lastMinorGC = std::chrono::steady_clock::now();
  auto lastMediumGC = lastMinorGC;
  auto lastMajorGC = lastMinorGC;
  
  while (!m_shuttingDown) {
    // スケジュールされたGCがあるか確認
    std::optional<std::pair<GCType, uint64_t>> scheduledGC;
    
    {
      std::unique_lock<std::mutex> lock(m_scheduleMutex);
      auto now = std::chrono::steady_clock::now();
      auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch().count();
      
      // スケジュールされたGCの確認
      if (!m_scheduledGCs.empty()) {
        auto& next = m_scheduledGCs.front();
        if (next.second <= nowMs) {
          scheduledGC = next;
          m_scheduledGCs.pop();
        }
      }
      
      // スケジュールされたGCがなければ一定時間待機
      if (!scheduledGC) {
        auto waitUntil = now + std::chrono::milliseconds(100);
        
        // 次のスケジュールGCがあれば、その時間まで待機
        if (!m_scheduledGCs.empty()) {
          auto& next = m_scheduledGCs.front();
          auto nextTime = std::chrono::milliseconds(next.second);
          auto nextPoint = std::chrono::steady_clock::time_point(nextTime);
          waitUntil = std::min(waitUntil, nextPoint);
        }
        
        m_scheduleCV.wait_until(lock, waitUntil, [this, &scheduledGC, nowMs] {
          if (!m_scheduledGCs.empty()) {
            auto& next = m_scheduledGCs.front();
            if (next.second <= nowMs) {
              scheduledGC = next;
              m_scheduledGCs.pop();
              return true;
            }
          }
          return m_shuttingDown.load();
        });
      }
    }
    
    if (m_shuttingDown) {
      break;
    }
    
    // スケジュールGCの実行
    if (scheduledGC) {
      collectGarbage(scheduledGC->first, GCCause::Scheduled);
      continue;
    }
    
    // 定期的なGCの確認
    auto now = std::chrono::steady_clock::now();
    bool gcExecuted = false;
    
    // メジャーGC確認
    if (m_config.enableAdaptiveCollection &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMajorGC).count() >= m_config.majorGCInterval) {
      if (determineGCType() == GCType::Major) {
        majorCollection(GCCause::Scheduled);
        lastMajorGC = now;
        lastMediumGC = now;
        lastMinorGC = now;
        gcExecuted = true;
      }
    }
    
    // ミディアムGC確認
    if (!gcExecuted &&
        m_config.enableAdaptiveCollection &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMediumGC).count() >= m_config.mediumGCInterval) {
      if (determineGCType() == GCType::Medium) {
        mediumCollection(GCCause::Scheduled);
        lastMediumGC = now;
        lastMinorGC = now;
        gcExecuted = true;
      }
    }
    
    // マイナーGC確認
    if (!gcExecuted &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMinorGC).count() >= m_config.minorGCInterval) {
      minorCollection(GCCause::Scheduled);
      lastMinorGC = now;
    }
    
    // メモリ使用率の確認
    if (!gcExecuted && m_config.enableAdaptiveCollection) {
      float heapUsage = getHeapUsageRatio();
      
      if (heapUsage >= m_config.emergencyGCHeapRatio) {
        // 緊急GC実行
        majorCollection(GCCause::LowMemory);
        lastMajorGC = now;
        lastMediumGC = now;
        lastMinorGC = now;
      } else if (heapUsage >= m_config.majorGCTriggerRatio) {
        majorCollection(GCCause::MemoryPressure);
        lastMajorGC = now;
        lastMediumGC = now;
        lastMinorGC = now;
      } else if (heapUsage >= m_config.mediumGCTriggerRatio) {
        mediumCollection(GCCause::MemoryPressure);
        lastMediumGC = now;
        lastMinorGC = now;
      } else if (heapUsage >= m_config.minorGCTriggerRatio) {
        minorCollection(GCCause::MemoryPressure);
        lastMinorGC = now;
      }
    }
    
    // 少し待機
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

// GCスケジュール
void ParallelGC::scheduleGC(GCType type, uint32_t delayMs) {
  std::lock_guard<std::mutex> lock(m_scheduleMutex);
  
  auto now = std::chrono::steady_clock::now();
  auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch().count();
  uint64_t scheduleTime = nowMs + delayMs;
  
  m_scheduledGCs.push(std::make_pair(type, scheduleTime));
  m_scheduleCV.notify_one();
}

// 各種GCスケジュール用ヘルパー関数
void ParallelGC::scheduleMinorGC(uint32_t delayMs) {
  scheduleGC(GCType::Minor, delayMs);
}

void ParallelGC::scheduleMediumGC(uint32_t delayMs) {
  scheduleGC(GCType::Medium, delayMs);
}

void ParallelGC::scheduleMajorGC(uint32_t delayMs) {
  scheduleGC(GCType::Major, delayMs);
}

void ParallelGC::scheduleCollection(GCType type, uint32_t delayMs) {
  scheduleGC(type, delayMs);
}

// GC種別決定
GCType ParallelGC::determineGCType() {
  // 適応的GC戦略に基づいてGC種別を決定
  float heapUsage = getHeapUsageRatio();
  
  if (heapUsage >= m_config.majorGCTriggerRatio) {
    return GCType::Major;
  } else if (heapUsage >= m_config.mediumGCTriggerRatio) {
    return GCType::Medium;
  } else {
    return GCType::Minor;
  }
}

// ヒープ使用率取得
float ParallelGC::getHeapUsageRatio() const {
  size_t totalSize = getHeapSize();
  size_t usedSize = getUsedMemory();
  
  if (totalSize == 0) {
    return 0.0f;
  }
  
  return static_cast<float>(usedSize) / static_cast<float>(totalSize);
}

// ヒープサイズ取得
size_t ParallelGC::getHeapSize() const {
  return m_stats.currentHeapSize;
}

// 使用メモリサイズ取得
size_t ParallelGC::getUsedMemory() const {
  size_t used = 0;
  
  for (const auto& cell : m_nurseryGen) {
    used += cell->getSize();
  }
  
  for (const auto& cell : m_youngGen) {
    used += cell->getSize();
  }
  
  for (const auto& cell : m_mediumGen) {
    used += cell->getSize();
  }
  
  for (const auto& cell : m_oldGen) {
    used += cell->getSize();
  }
  
  for (const auto& cell : m_largeObjects) {
    used += cell->getSize();
  }
  
  return used;
}

// GC実行
void ParallelGC::collectGarbage(GCType type, GCCause cause) {
  if (!m_gcEnabled) {
    return;
  }
  
  // GCが既に実行中ならスキップ
  bool expected = false;
  if (!m_collectionInProgress.compare_exchange_strong(expected, true)) {
    return;
  }
  
  try {
    auto startTime = std::chrono::steady_clock::now();
    
    m_currentGCType = type;
    m_currentGCCause = cause;
    
    // GC準備
    prepareCollection(type);
    
    // マーキングフェーズ
    if (m_config.enableConcurrentMarking && type != GCType::Minor) {
      // 並行マーキング
      markRoots();
      markConcurrent();
      finishMarking();
    } else {
      // 通常マーキング
      markRoots();
      for (size_t i = 0; i < m_markingQueues.size(); i++) {
        processMarkingWorkQueue(i);
      }
    }
    
    // スイーピングフェーズ
    sweep(m_config.enableConcurrentSweeping && type != GCType::Minor);
    
    // コンパクションフェーズ
    if (m_config.enableCompaction && type == GCType::Major) {
      compact();
    }
    
    // 昇格フェーズ
    promoteObjects();
    
    // 統計情報更新
    auto endTime = std::chrono::steady_clock::now();
    uint64_t durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    m_stats.lastGCDurationMs = durationMs;
    m_stats.lastGCTimestamp = std::chrono::time_point_cast<std::chrono::milliseconds>(endTime).time_since_epoch().count();
    m_stats.lastGCType = type;
    m_stats.lastGCCause = cause;
    
    if (type == GCType::Major) {
      m_stats.majorGCCount++;
      m_stats.totalMajorGCTimeMs += durationMs;
    } else if (type == GCType::Medium) {
      m_stats.mediumGCCount++;
      m_stats.totalMediumGCTimeMs += durationMs;
    } else {
      m_stats.minorGCCount++;
      m_stats.totalMinorGCTimeMs += durationMs;
    }
    
    // GCメトリクス更新
    updateGCMetrics();
    
    // 適応的GCパラメータ調整
    if (m_config.enableAdaptiveCollection) {
      adjustGCParameters();
    }
    
    m_lastGCTime = endTime;
  } catch (const std::exception& e) {
    // 例外処理
    std::cerr << "GC error: " << e.what() << std::endl;
  }
  
  // GC完了
  m_collectionInProgress = false;
}

// マイナーGC実行
void ParallelGC::minorCollection(GCCause cause) {
  collectGarbage(GCType::Minor, cause);
}

// ミディアムGC実行
void ParallelGC::mediumCollection(GCCause cause) {
  collectGarbage(GCType::Medium, cause);
}

// メジャーGC実行
void ParallelGC::majorCollection(GCCause cause) {
  collectGarbage(GCType::Major, cause);
}

// コレクション準備
void ParallelGC::prepareCollection(GCType type) {
  // マーキングキューをクリア
  for (auto& queue : m_markingQueues) {
    queue->clear();
  }
  
  // 対象世代のオブジェクトマークをリセット
  auto resetMarks = [](auto& container) {
    for (auto* cell : container) {
      cell->state = CellState::White;
    }
  };
  
  resetMarks(m_nurseryGen);
  
  if (type >= GCType::Minor) {
    resetMarks(m_youngGen);
  }
  
  if (type >= GCType::Medium) {
    resetMarks(m_mediumGen);
  }
  
  if (type == GCType::Major) {
    resetMarks(m_oldGen);
    resetMarks(m_largeObjects);
  }
  
  // 記憶セットとカード表の処理
  if (type == GCType::Major) {
    m_cardTable->clearAll();
  }
}

// ルートマーキング
void ParallelGC::markRoots() {
  std::lock_guard<std::mutex> lock(m_rootsMutex);
  
  for (size_t i = 0; i < m_roots.size(); ++i) {
    if (*m_roots[i]) {
      mark(*m_roots[i]);
    }
  }
}

// 通常マーキング
void ParallelGC::mark(GCCell* root) {
  if (!root || root->state != CellState::White) {
    return;
  }
  
  // このオブジェクトをマーク
  root->state = CellState::Gray;
  
  // マーキングキューにプッシュ（ラウンドロビンで分配）
  static thread_local size_t queueIndex = 0;
  size_t queueCount = m_markingQueues.size();
  
  if (queueCount > 0) {
    queueIndex = (queueIndex + 1) % queueCount;
    m_markingQueues[queueIndex]->push(root);
  }
}

// マーキングキュー処理
void ParallelGC::processMarkingWorkQueue(int threadId) {
  auto& queue = m_markingQueues[threadId];
  GCCell* cell = nullptr;
  size_t processedCount = 0;
  
  while (queue->pop(cell) || stealWork(threadId, cell)) {
    if (cell && cell->state == CellState::Gray) {
      // オブジェクトを処理中としてマーク
      cell->state = CellState::Black;
      
      // このオブジェクトが持つ参照を辿る
      cell->visitReferences([this](GCCell* ref) {
        if (ref && ref->state == CellState::White) {
          mark(ref);
        }
      });
      
      processedCount++;
      
      // 一定量処理したら一時停止
      if (processedCount >= m_config.markingStepSize) {
        std::this_thread::yield();
        processedCount = 0;
      }
    }
  }
}

// 並行マーキング
void ParallelGC::markConcurrent() {
  if (m_markingQueues.empty()) {
    return;
  }
  
  m_concurrentMarkingActive = true;
  m_stats.concurrentMarkingPasses++;
  
  // ワーカースレッドをアクティブ化
  {
    std::lock_guard<std::mutex> lock(m_workerMutex);
    m_workersActive = true;
    m_workerCV.notify_all();
  }
  
  // メインスレッドも一部作業を行う
  auto startTime = std::chrono::steady_clock::now();
  size_t mainThreadWorkItems = 0;
  
  while (true) {
    // マーキングキューが全て空になったかチェック
    bool allEmpty = true;
    for (const auto& queue : m_markingQueues) {
      if (!queue->empty()) {
        allEmpty = false;
        break;
      }
    }
    
    if (allEmpty) {
      break;
    }
    
    // メインスレッドでもマーキング実行
    processMarkingWorkQueue(0);
    mainThreadWorkItems++;
    
    // 一定時間経過したらユーザースレッドに制御を戻す
    auto currentTime = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() >= m_config.markingQuantum) {
      std::this_thread::sleep_for(std::chrono::milliseconds(m_config.mutatorQuantum));
      startTime = std::chrono::steady_clock::now();
    }
  }
  
  // ワーカースレッドを一時停止
  {
    std::lock_guard<std::mutex> lock(m_workerMutex);
    m_workersActive = false;
  }
  
  m_concurrentMarkingActive = false;
}

// インクリメンタルマーキング
void ParallelGC::markIncrementalStep(size_t stepSize) {
  if (!m_incrementalMarkingActive || m_markingQueues.empty()) {
    return;
  }
  
  size_t actualStepSize = stepSize > 0 ? stepSize : m_config.markingStepSize;
  size_t processedItems = 0;
  
  auto& queue = m_markingQueues[0]; // メインスレッド用キュー
  GCCell* cell = nullptr;
  
  while (processedItems < actualStepSize && (queue->pop(cell) || stealWork(0, cell))) {
    if (cell && cell->state == CellState::Gray) {
      // オブジェクトを処理中としてマーク
      cell->state = CellState::Black;
      
      // このオブジェクトが持つ参照を辿る
      cell->visitReferences([this](GCCell* ref) {
        if (ref && ref->state == CellState::White) {
          mark(ref);
        }
      });
      
      processedItems++;
    }
  }
  
  m_stats.totalIncrementalMarkSteps++;
  m_stats.incrementalMarkingPasses++;
}

// マーキング完了処理
void ParallelGC::finishMarking() {
  // 全てのオブジェクトが適切にマークされているか確認
  // Grey状態のオブジェクトがあればBlackに変更
  auto finalizeMarks = [](auto& container) {
    for (auto* cell : container) {
      if (cell->state == CellState::Gray) {
        cell->state = CellState::Black;
      }
    }
  };
  
  finalizeMarks(m_nurseryGen);
  finalizeMarks(m_youngGen);
  
  if (m_currentGCType >= GCType::Medium) {
    finalizeMarks(m_mediumGen);
  }
  
  if (m_currentGCType == GCType::Major) {
    finalizeMarks(m_oldGen);
    finalizeMarks(m_largeObjects);
  }
}

// スイーピング処理
void ParallelGC::sweep(bool concurrent) {
  auto sweepStart = std::chrono::steady_clock::now();
  
  if (concurrent && m_config.enableConcurrentSweeping) {
    // 並行スイープの実装
    performConcurrentSweep();
  } else {
    // 同期スイープ実装
    performSynchronousSweep();
  }
  
  auto sweepEnd = std::chrono::steady_clock::now();
  m_stats.totalSweepingTimeMs += std::chrono::duration_cast<std::chrono::milliseconds>(sweepEnd - sweepStart).count();
}

void ParallelGC::performConcurrentSweep() {
  // 並行スイープの準備
  std::atomic<size_t> totalFreedObjects{0};
  std::atomic<size_t> totalFreedMemory{0};
  
  // ワーカースレッド用のタスクキューを準備
  std::vector<SweepTask> sweepTasks;
  
  // 各世代を並列で処理するためのタスクを作成
  if (m_currentGCType >= GCType::Minor) {
    sweepTasks.emplace_back(SweepTask{ExtendedGeneration::Nursery, &m_nurseryGen});
    sweepTasks.emplace_back(SweepTask{ExtendedGeneration::Young, &m_youngGen});
  }
  
  if (m_currentGCType >= GCType::Medium) {
    sweepTasks.emplace_back(SweepTask{ExtendedGeneration::Medium, &m_mediumGen});
  }
  
  if (m_currentGCType == GCType::Major) {
    sweepTasks.emplace_back(SweepTask{ExtendedGeneration::Old, &m_oldGen});
    sweepTasks.emplace_back(SweepTask{ExtendedGeneration::LargeObj, &m_largeObjects});
  }
  
  // ワーカースレッドを起動
  std::vector<std::thread> sweepWorkers;
  std::atomic<size_t> taskIndex{0};
  
  size_t numWorkers = std::min(m_numWorkerThreads, sweepTasks.size());
  
  for (size_t i = 0; i < numWorkers; ++i) {
    sweepWorkers.emplace_back([&, i]() {
      size_t localFreedObjects = 0;
      size_t localFreedMemory = 0;
      
      // タスクを動的に取得して処理
      while (true) {
        size_t currentTask = taskIndex.fetch_add(1, std::memory_order_relaxed);
        if (currentTask >= sweepTasks.size()) {
          break; // 全タスク完了
        }
        
        const auto& task = sweepTasks[currentTask];
        sweepGeneration(*task.container, localFreedObjects, localFreedMemory);
      }
      
      // 全体の統計に反映
      totalFreedObjects.fetch_add(localFreedObjects, std::memory_order_relaxed);
      totalFreedMemory.fetch_add(localFreedMemory, std::memory_order_relaxed);
    });
  }
  
  // 全ワーカーの完了を待機
  for (auto& worker : sweepWorkers) {
    worker.join();
  }
  
  // 統計情報更新
  m_stats.freedObjects += totalFreedObjects.load();
  m_stats.freedBytes += totalFreedMemory.load();
  m_stats.currentHeapSize -= totalFreedMemory.load();
}

void ParallelGC::performSynchronousSweep() {
  size_t freedObjs = 0;
  size_t freedMem = 0;
  
  // 常にナーサリーはスイープ
  sweepGeneration(m_nurseryGen, freedObjs, freedMem);
  
  // GCタイプに応じたスイープ
  if (m_currentGCType >= GCType::Minor) {
    sweepGeneration(m_youngGen, freedObjs, freedMem);
  }
  
  if (m_currentGCType >= GCType::Medium) {
    sweepGeneration(m_mediumGen, freedObjs, freedMem);
  }
  
  if (m_currentGCType == GCType::Major) {
    sweepGeneration(m_oldGen, freedObjs, freedMem);
    sweepGeneration(m_largeObjects, freedObjs, freedMem);
  }
  
  // 統計情報更新
  m_stats.freedObjects += freedObjs;
  m_stats.freedBytes += freedMem;
  m_stats.currentHeapSize -= freedMem;
}

template<typename Container>
void ParallelGC::sweepGeneration(Container& container, size_t& freedObjs, size_t& freedMem) {
  std::lock_guard<std::mutex> lock(m_sweepMutex); // コンテナアクセス用の排他制御
  
  auto it = container.begin();
  while (it != container.end()) {
    auto* obj = *it;
    
    if (obj->state == CellState::White) {
      // マークされていないオブジェクトを回収
      freedMem += obj->getSize();
      freedObjs++;
      
      // ファイナライザーを呼び出し
      if (obj->hasFinalizaer()) {
        obj->finalize();
      }
      
      delete obj;
      it = container.erase(it);
    } else {
      // 次回GC用にマークをリセット
      obj->state = CellState::White;
      ++it;
    }
  }
}

// スイープタスクの構造体定義
struct SweepTask {
  ExtendedGeneration generation;
  std::vector<GCCell*>* container;
};

// GCメトリクス更新
void ParallelGC::updateGCMetrics() {
  // 各世代のオブジェクト数とサイズを更新
  size_t nurserySize = 0;
  for (const auto& cell : m_nurseryGen) {
    nurserySize += cell->getSize();
  }
  
  size_t youngSize = 0;
  for (const auto& cell : m_youngGen) {
    youngSize += cell->getSize();
  }
  
  size_t mediumSize = 0;
  for (const auto& cell : m_mediumGen) {
    mediumSize += cell->getSize();
  }
  
  size_t oldSize = 0;
  for (const auto& cell : m_oldGen) {
    oldSize += cell->getSize();
  }
  
  size_t largeSize = 0;
  for (const auto& cell : m_largeObjects) {
    largeSize += cell->getSize();
  }
  
  // 統計情報更新
  m_stats.generationObjectCount[static_cast<size_t>(ExtendedGeneration::Nursery)] = m_nurseryGen.size();
  m_stats.generationObjectCount[static_cast<size_t>(ExtendedGeneration::Young)] = m_youngGen.size();
  m_stats.generationObjectCount[static_cast<size_t>(ExtendedGeneration::Medium)] = m_mediumGen.size();
  m_stats.generationObjectCount[static_cast<size_t>(ExtendedGeneration::Old)] = m_oldGen.size();
  m_stats.generationObjectCount[static_cast<size_t>(ExtendedGeneration::LargeObj)] = m_largeObjects.size();
  
  m_stats.generationByteSize[static_cast<size_t>(ExtendedGeneration::Nursery)] = nurserySize;
  m_stats.generationByteSize[static_cast<size_t>(ExtendedGeneration::Young)] = youngSize;
  m_stats.generationByteSize[static_cast<size_t>(ExtendedGeneration::Medium)] = mediumSize;
  m_stats.generationByteSize[static_cast<size_t>(ExtendedGeneration::Old)] = oldSize;
  m_stats.generationByteSize[static_cast<size_t>(ExtendedGeneration::LargeObj)] = largeSize;
  
  // ヒープ使用率
  m_stats.heapUsageRatio = getHeapUsageRatio();
  
  // GCスループット計算
  if (m_stats.lastGCDurationMs > 0) {
    auto now = std::chrono::steady_clock::now();
    auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch().count();
    auto timeSinceLastGC = nowMs - m_stats.lastGCTimestamp;
    
    if (timeSinceLastGC > 0) {
      m_stats.pauseTimeRatio = static_cast<float>(m_stats.lastGCDurationMs) / static_cast<float>(timeSinceLastGC);
      m_stats.throughput = 1.0f - m_stats.pauseTimeRatio;
    }
  }
}

// 適応的GCパラメータ調整
void ParallelGC::adjustGCParameters() {
  // ヒープサイズの調整
  if (m_stats.heapUsageRatio > 0.85f) {
    // ヒープが85%以上使用されている場合、拡大を検討
    size_t newHeapSize = static_cast<size_t>(m_stats.currentHeapSize * m_config.heapGrowthFactor);
    if (newHeapSize <= m_config.maxHeapSize) {
      expandHeap(newHeapSize - m_stats.currentHeapSize);
    }
  }
  
  // GC間隔の調整
  // メモリ使用率に基づいて、マイナーGC、ミディアムGC、メジャーGCの間隔を調整
  if (m_stats.heapUsageRatio > 0.8f) {
    // メモリ使用率が高い場合、GC間隔を短くする
    m_config.minorGCInterval = std::max(100u, m_config.minorGCInterval / 2);
    m_config.mediumGCInterval = std::max(1000u, m_config.mediumGCInterval / 2);
    m_config.majorGCInterval = std::max(5000u, m_config.majorGCInterval / 2);
  } else if (m_stats.heapUsageRatio < 0.3f) {
    // メモリ使用率が低い場合、GC間隔を長くする
    m_config.minorGCInterval = std::min(2000u, m_config.minorGCInterval * 2);
    m_config.mediumGCInterval = std::min(10000u, m_config.mediumGCInterval * 2);
    m_config.majorGCInterval = std::min(60000u, m_config.majorGCInterval * 2);
  }
  
  // ワーカースレッド数の調整
  adjustWorkerThreadCount();
}

// インクリメンタルマーキングステップ実行（外部から呼び出し可能）
void ParallelGC::incrementalMarkingStep(size_t stepSize) {
  if (m_incrementalMarkingActive) {
    markIncrementalStep(stepSize);
  }
}

// ルート追加
void ParallelGC::addRoot(GCCell** root) {
  if (!root) return;
  
  std::lock_guard<std::mutex> lock(m_rootsMutex);
  m_roots.push_back(root);
}

// ルート削除
void ParallelGC::removeRoot(GCCell** root) {
  if (!root) return;
  
  std::lock_guard<std::mutex> lock(m_rootsMutex);
  auto it = std::find(m_roots.begin(), m_roots.end(), root);
  if (it != m_roots.end()) {
    m_roots.erase(it);
  }
}

// メモリ拡張処理
void ParallelGC::expandHeap(size_t additionalSize) {
  if (additionalSize == 0) {
    return;
  }
  
  if (m_stats.currentHeapSize + additionalSize > m_config.maxHeapSize) {
    additionalSize = m_config.maxHeapSize - m_stats.currentHeapSize;
    if (additionalSize <= 0) {
      return; // 既に最大サイズに達している
    }
  }
  
  // アロケータにヒープ拡張を依頼
  if (m_allocator->expand(additionalSize)) {
    m_stats.currentHeapSize += additionalSize;
    
    // カードテーブルの更新（必要に応じて）
    if (m_cardTable) {
      // カードテーブルを新しいヒープサイズで再構築
      m_cardTable = std::make_unique<CardTable>(m_stats.currentHeapSize);
    }
  }
}

// メモリ割り当て用のRaw実装
void* ParallelGC::allocateRaw(size_t size, ExtendedGeneration gen) {
  // 大きいオブジェクトの場合は特別処理
  if (size >= m_config.largeObjectThreshold) {
    return m_allocator->allocateLarge(size);
  }
  
  // 世代別に適切な領域から割り当て
  switch (gen) {
    case ExtendedGeneration::Nursery:
      return m_allocator->allocateFromNursery(size);
      
    case ExtendedGeneration::Young:
      return m_allocator->allocateFromYoung(size);
      
    case ExtendedGeneration::Medium:
      return m_allocator->allocateFromMedium(size);
      
    case ExtendedGeneration::Old:
      return m_allocator->allocateFromOld(size);
      
    case ExtendedGeneration::LargeObj:
      return m_allocator->allocateLarge(size);
      
    default:
      return nullptr;
  }
}

// メモリ解放
void ParallelGC::freeRaw(void* ptr, size_t size) {
  if (ptr) {
    m_allocator->deallocate(ptr, size);
  }
}

// 世代への追加
void ParallelGC::addToGeneration(GCCell* cell, ExtendedGeneration gen) {
  if (!cell) return;
  
  switch (gen) {
    case ExtendedGeneration::Nursery:
      m_nurseryGen.push_back(cell);
      break;
      
    case ExtendedGeneration::Young:
      m_youngGen.push_back(cell);
      break;
      
    case ExtendedGeneration::Medium:
      m_mediumGen.push_back(cell);
      break;
      
    case ExtendedGeneration::Old:
      m_oldGen.push_back(cell);
      break;
      
    case ExtendedGeneration::LargeObj:
      m_largeObjects.insert(cell);
      break;
  }
}

// 全世代のオブジェクト昇格処理
void ParallelGC::promoteObjects() {
  auto promoteStart = std::chrono::steady_clock::now();
  
  // ナーサリー世代からヤング世代への昇格
  for (auto it = m_nurseryGen.begin(); it != m_nurseryGen.end();) {
    auto* obj = *it;
    
    // 生存したオブジェクトの年齢を増加
    if (obj->state != CellState::White) {
      obj->age++;
      
      // 昇格条件を満たすか確認
      if (obj->age >= m_config.nurseryToYoungAge) {
        // ヤング世代へ昇格
        obj->generation = Generation::Young;
        m_youngGen.push_back(obj);
        it = m_nurseryGen.erase(it);
        continue;
      }
    }
    
    ++it;
  }
  
  // ヤング世代からミディアム世代への昇格
  if (m_currentGCType >= GCType::Minor) {
    for (auto it = m_youngGen.begin(); it != m_youngGen.end();) {
      auto* obj = *it;
      
      // 生存したオブジェクトの年齢を増加
      if (obj->state != CellState::White) {
        obj->age++;
        
        // 昇格条件を満たすか確認
        if (obj->age >= m_config.youngToMediumAge) {
          // ミディアム世代へ昇格
          // ExtendedGenerationとGenerationの対応を考慮
          obj->generation = Generation::Old; // ミディアム世代は実際にはOld世代として扱う
          m_mediumGen.push_back(obj);
          it = m_youngGen.erase(it);
          continue;
        }
      }
      
      ++it;
    }
  }
  
  // ミディアム世代からオールド世代への昇格
  if (m_currentGCType >= GCType::Medium) {
    for (auto it = m_mediumGen.begin(); it != m_mediumGen.end();) {
      auto* obj = *it;
      
      // 生存したオブジェクトの年齢を増加
      if (obj->state != CellState::White) {
        obj->age++;
        
        // 昇格条件を満たすか確認
        if (obj->age >= m_config.mediumToOldAge) {
          // オールド世代へ昇格
          obj->generation = Generation::Old;
          m_oldGen.push_back(obj);
          it = m_mediumGen.erase(it);
          continue;
        }
      }
      
      ++it;
    }
  }
  
  auto promoteEnd = std::chrono::steady_clock::now();
  m_stats.totalPromotionTimeMs += std::chrono::duration_cast<std::chrono::milliseconds>(promoteEnd - promoteStart).count();
}

// オブジェクト昇格
void ParallelGC::promoteObject(GCCell* object, ExtendedGeneration targetGen) {
  if (!object) return;
  
  // 元の世代から削除
  ExtendedGeneration currentGen = static_cast<ExtendedGeneration>(object->generation);
  
  switch (currentGen) {
    case ExtendedGeneration::Nursery: {
      auto it = std::find(m_nurseryGen.begin(), m_nurseryGen.end(), object);
      if (it != m_nurseryGen.end()) {
        m_nurseryGen.erase(it);
      }
      break;
    }
    
    case ExtendedGeneration::Young: {
      auto it = std::find(m_youngGen.begin(), m_youngGen.end(), object);
      if (it != m_youngGen.end()) {
        m_youngGen.erase(it);
      }
      break;
    }
    
    case ExtendedGeneration::Medium: {
      auto it = std::find(m_mediumGen.begin(), m_mediumGen.end(), object);
      if (it != m_mediumGen.end()) {
        m_mediumGen.erase(it);
      }
      break;
    }
    
    case ExtendedGeneration::Old: {
      auto it = std::find(m_oldGen.begin(), m_oldGen.end(), object);
      if (it != m_oldGen.end()) {
        m_oldGen.erase(it);
      }
      break;
    }
    
    case ExtendedGeneration::LargeObj: {
      m_largeObjects.erase(object);
      break;
    }
  }
  
  // 新しい世代に追加
  // ExtendedGenerationとGenerationの対応付け
  if (targetGen == ExtendedGeneration::Young) {
    object->generation = Generation::Young;
  } else {
    object->generation = Generation::Old;
  }
  
  addToGeneration(object, targetGen);
  
  // 統計情報更新
  m_stats.promotionCount++;
}

// コンパクション処理
void ParallelGC::compact() {
  if (!m_config.enableCompaction) {
    return;
  }
  
  auto compactStart = std::chrono::steady_clock::now();
  
  // オールド世代のみコンパクション対象
  if (m_oldGen.empty()) {
    return;
  }
  
  // フォワーディングアドレステーブルの構築
  std::unordered_map<GCCell*, GCCell*> forwardingTable;
  size_t totalSize = 0;
  
  // 生存オブジェクトのサイズ計算
  for (auto* cell : m_oldGen) {
    if (cell->state != CellState::White) {
      totalSize += cell->getSize();
    }
  }
  
  // コンパクション用の連続領域確保
  uint8_t* newArea = static_cast<uint8_t*>(m_allocator->allocateContiguous(totalSize));
  if (!newArea) {
    // 連続領域の確保に失敗
    return;
  }
  
  // オブジェクトを新領域にコピーしてフォワーディングアドレス設定
  uint8_t* current = newArea;
  for (auto* cell : m_oldGen) {
    if (cell->state != CellState::White) {
      size_t size = cell->getSize();
      
      // オブジェクトをコピー
      std::memcpy(current, cell, size);
      
      // フォワーディングアドレス設定
      GCCell* newCell = reinterpret_cast<GCCell*>(current);
      forwardingTable[cell] = newCell;
      cell->forwardingAddress = newCell;
      
      current += size;
    }
  }
  
  // すべての参照を更新
  for (auto& entry : forwardingTable) {
    GCCell* newCell = entry.second;
    
    // オブジェクト内の参照を更新
    newCell->visitMutableReferences([&forwardingTable](GCCell** ref) {
      if (*ref) {
        auto it = forwardingTable.find(*ref);
        if (it != forwardingTable.end()) {
          *ref = it->second;
        }
      }
    });
  }
  
  // ルート参照を更新
  {
    std::lock_guard<std::mutex> lock(m_rootsMutex);
    for (auto root : m_roots) {
      if (*root) {
        auto it = forwardingTable.find(*root);
        if (it != forwardingTable.end()) {
          *root = it->second;
        }
      }
    }
  }
  
  // 古いオブジェクトを解放し、新しいオブジェクトリストを構築
  std::vector<GCCell*> newOldGen;
  newOldGen.reserve(forwardingTable.size());
  
  for (auto* cell : m_oldGen) {
    if (cell->state != CellState::White) {
      // 生存オブジェクトは新しいバージョンをリストに追加
      newOldGen.push_back(static_cast<GCCell*>(cell->forwardingAddress));
    } else {
      // 死んだオブジェクトは直接解放
      delete cell;
    }
  }
  
  // 古いリストを新しいリストに置き換え
  m_oldGen = std::move(newOldGen);
  
  auto compactEnd = std::chrono::steady_clock::now();
  m_stats.totalCompactionTimeMs += std::chrono::duration_cast<std::chrono::milliseconds>(compactEnd - compactStart).count();
}

// 書き込みバリア
void ParallelGC::writeBarrier(GCCell* parent, GCCell* child) {
  if (!parent || !child) {
    return;
  }
  
  m_stats.writeBarrierInvocations++;
  
  // 親と子の世代をチェック
  ExtendedGeneration parentGen = static_cast<ExtendedGeneration>(parent->generation);
  ExtendedGeneration childGen = static_cast<ExtendedGeneration>(child->generation);
  
  // 子が親より若い世代の場合、記憶セットに追加（古→若の参照をトラック）
  if (static_cast<int>(parentGen) > static_cast<int>(childGen)) {
    m_rememberSet->add(parent, child);
    m_stats.rememberSetEntries++;
  }
  
  // カードテーブルを更新（コレクション対象世代の効率的な特定に使用）
  m_cardTable->markCard(parent);
  m_stats.cardTableUpdates++;
}

// Valueを含む書き込みバリア
void ParallelGC::writeBarrier(GCCell* object, const aerojs::core::runtime::Value& value) {
  if (!object || !value.isHeapObject()) {
    return;
  }
  
  // Valueから子オブジェクトを取得
  GCCell* childObj = value.asHeapObject();
  if (childObj) {
    writeBarrier(object, childObj);
  }
}

// メモリ割り当てのテンプレート実装
template<typename T, typename... Args>
T* ParallelGC::allocate(Args&&... args) {
  // GC圧迫チェック
  float heapUsage = getHeapUsageRatio();
  if (heapUsage >= m_config.minorGCTriggerRatio) {
    // メモリ圧迫時にGCを実行
    minorCollection(GCCause::Allocation);
  }
  
  // メモリ割り当て
  size_t size = sizeof(T);
  ExtendedGeneration targetGen = ExtendedGeneration::Nursery;
  
  // 大きいオブジェクトかチェック
  if (size >= m_config.largeObjectThreshold) {
    targetGen = ExtendedGeneration::LargeObj;
  }
  
  // メモリ領域確保
  void* memory = allocateRaw(size, targetGen);
  if (!memory) {
    // メモリ割り当て失敗時に緊急GC実行
    majorCollection(GCCause::Allocation);
    
    // 再試行
    memory = allocateRaw(size, targetGen);
    if (!memory) {
      throw std::bad_alloc();
    }
  }
  
  // オブジェクト構築
  T* obj = new(memory) T(std::forward<Args>(args)...);
  
  // 世代情報設定
  obj->generation = static_cast<uint8_t>(targetGen);
  
  // 管理対象に追加
  addToGeneration(obj, targetGen);
  
  return obj;
}

// 大きいオブジェクト専用割り当て
template<typename T, typename... Args>
T* ParallelGC::allocateLarge(Args&&... args) {
  // GC圧迫チェック
  float heapUsage = getHeapUsageRatio();
  if (heapUsage >= m_config.majorGCTriggerRatio) {
    // メモリ圧迫時にGCを実行
    majorCollection(GCCause::Allocation);
  }
  
  // メモリ割り当て
  size_t size = sizeof(T);
  void* memory = allocateRaw(size, ExtendedGeneration::LargeObj);
  
  if (!memory) {
    // メモリ割り当て失敗時に緊急GC実行
    majorCollection(GCCause::Allocation);
    
    // 再試行
    memory = allocateRaw(size, ExtendedGeneration::LargeObj);
    if (!memory) {
      throw std::bad_alloc();
    }
  }
  
  // オブジェクト構築
  T* obj = new(memory) T(std::forward<Args>(args)...);
  
  // 世代情報設定
  obj->generation = static_cast<uint8_t>(ExtendedGeneration::LargeObj);
  
  // 大きいオブジェクトセットに追加
  m_largeObjects.insert(obj);
  
  return obj;
}

// 他のスレッドからワークを盗む
bool ParallelGC::stealWork(int threadId, GCCell*& cell) {
  size_t queueCount = m_markingQueues.size();
  
  for (size_t i = 1; i < queueCount; ++i) {
    size_t targetId = (threadId + i) % queueCount;
    if (m_markingQueues[targetId]->steal(cell)) {
      return true;
    }
  }
  
  return false;
}

// スループットやGC時間に基づいてワーカースレッド数を調整
void ParallelGC::adjustWorkerThreadCount() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto gcDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        currentTime - m_lastGCTime).count();
    
    // スループット計算（processed objects per second）
    double currentThroughput = m_processedObjects / (gcDuration / 1000.0);
    
    // 前回のスループットと比較
    if (m_previousThroughput > 0) {
        double throughputChange = (currentThroughput - m_previousThroughput) / m_previousThroughput;
        
        // スループットが5%以上向上した場合、ワーカー数を増加
        if (throughputChange > 0.05 && m_workerThreadCount < std::thread::hardware_concurrency()) {
            m_workerThreadCount++;
            m_adaptationDirection = 1; // 増加方向
        }
        // スループットが5%以上低下した場合、ワーカー数を減少
        else if (throughputChange < -0.05 && m_workerThreadCount > 1) {
            m_workerThreadCount--;
            m_adaptationDirection = -1; // 減少方向
        }
        // GC時間が目標を超過した場合の調整
        else if (gcDuration > m_targetGCTime && m_workerThreadCount < std::thread::hardware_concurrency()) {
            m_workerThreadCount++;
        }
        // GC時間が目標を大幅に下回る場合、リソース節約のためワーカー数を減少
        else if (gcDuration < m_targetGCTime * 0.5 && m_workerThreadCount > 1) {
            m_workerThreadCount--;
        }
    }
    
    // 統計情報の更新
    m_previousThroughput = currentThroughput;
    m_lastGCTime = currentTime;
    m_processedObjects = 0; // リセット
    
    // 調整結果をログ出力
    m_logger->info("GC Worker thread count adjusted to: {} (throughput: {:.2f} obj/s, duration: {} ms)",
                   m_workerThreadCount, currentThroughput, gcDuration);
}

} // namespace memory
} // namespace utils
} // namespace aerojs 