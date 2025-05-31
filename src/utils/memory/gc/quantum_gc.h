/**
 * @file quantum_gc.h
 * @brief AeroJS 量子レベル ガベージコレクタシステム
 * @version 1.0.0
 * @license MIT
 */

#ifndef AEROJS_UTILS_MEMORY_GC_QUANTUM_GC_H
#define AEROJS_UTILS_MEMORY_GC_QUANTUM_GC_H

#include "garbage_collector.h"
#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>

namespace aerojs {
namespace utils {
namespace memory {

/**
 * @brief GC戦略
 */
enum class GCStrategy {
    Conservative,       // 保守的GC
    Generational,      // 世代別GC
    Incremental,       // インクリメンタルGC
    Concurrent,        // 並行GC
    Parallel,          // 並列GC
    Quantum            // 量子GC（最先端）
};

/**
 * @brief GC統計情報
 */
struct QuantumGCStats {
    std::atomic<uint64_t> totalCollections{0};
    std::atomic<uint64_t> youngGenCollections{0};
    std::atomic<uint64_t> oldGenCollections{0};
    std::atomic<uint64_t> fullCollections{0};
    std::atomic<uint64_t> bytesCollected{0};
    std::atomic<uint64_t> objectsCollected{0};
    std::atomic<double> averageCollectionTime{0.0};
    std::atomic<double> throughput{0.0};
    std::atomic<double> pauseTime{0.0};
    std::atomic<double> memoryEfficiency{0.0};
    std::chrono::high_resolution_clock::time_point startTime;
};

/**
 * @brief オブジェクト世代
 */
enum class Generation {
    Young = 0,      // 若い世代
    Middle = 1,     // 中間世代
    Old = 2,        // 古い世代
    Permanent = 3   // 永続世代
};

/**
 * @brief オブジェクトメタデータ
 */
struct ObjectMetadata {
    Generation generation = Generation::Young;
    uint32_t age = 0;
    uint32_t referenceCount = 0;
    bool marked = false;
    bool pinned = false;
    bool finalizable = false;
    std::chrono::high_resolution_clock::time_point lastAccess;
    size_t size = 0;
    void* typeInfo = nullptr;
};

/**
 * @brief 世界最先端量子ガベージコレクタ
 */
class QuantumGC : public GarbageCollector {
public:
    QuantumGC(MemoryAllocator* allocator, MemoryPool* pool);
    ~QuantumGC() override;

    // 基本GC操作
    void collect() override;
    void collectGeneration(Generation gen);
    void collectFull();
    void collectIncremental();

    // 並行・並列GC
    void enableConcurrentGC(bool enable);
    void enableParallelGC(bool enable);
    void setGCThreads(uint32_t threads);

    // 世代別GC設定
    void setGenerationThreshold(Generation gen, uint32_t threshold);
    void setPromotionThreshold(uint32_t threshold);
    void enableGenerationalGC(bool enable);

    // 適応的GC
    void enableAdaptiveGC(bool enable);
    void setTargetPauseTime(std::chrono::milliseconds target);
    void setTargetThroughput(double target);

    // 高度な機能
    void enableQuantumOptimization(bool enable);
    void enablePredictiveCollection(bool enable);
    void enableMemoryCompaction(bool enable);
    void enableWeakReferenceSupport(bool enable);

    // オブジェクト管理
    void* allocateObject(size_t size, void* typeInfo = nullptr);
    void pinObject(void* object);
    void unpinObject(void* object);
    void addFinalizer(void* object, std::function<void()> finalizer);

    // 統計・監視
    const QuantumGCStats& getStats() const;
    std::string getPerformanceReport() const;
    void resetStats();
    double getMemoryPressure() const;
    double getFragmentation() const;

    // デバッグ・診断
    void enableDebugMode(bool enable);
    std::vector<std::string> getGCLog() const;
    void dumpHeapState() const;
    bool verifyHeapIntegrity() const;

    // 設定
    void setStrategy(GCStrategy strategy);
    GCStrategy getStrategy() const;
    void setMemoryPressureThreshold(double threshold);
    void setFragmentationThreshold(double threshold);

private:
    struct HeapRegion;
    struct GCWorker;
    struct MarkingContext;
    struct SweepingContext;

    // ヒープ管理
    std::vector<std::unique_ptr<HeapRegion>> youngGeneration_;
    std::vector<std::unique_ptr<HeapRegion>> middleGeneration_;
    std::vector<std::unique_ptr<HeapRegion>> oldGeneration_;
    std::vector<std::unique_ptr<HeapRegion>> permanentGeneration_;

    // オブジェクト追跡
    std::unordered_map<void*, std::unique_ptr<ObjectMetadata>> objectMetadata_;
    std::unordered_set<void*> pinnedObjects_;
    std::unordered_map<void*, std::function<void()>> finalizers_;
    mutable std::mutex metadataMutex_;

    // 並行・並列処理
    std::vector<std::unique_ptr<GCWorker>> workers_;
    std::atomic<bool> concurrentGC_{false};
    std::atomic<bool> parallelGC_{false};
    std::atomic<uint32_t> gcThreads_{std::thread::hardware_concurrency()};
    std::mutex gcMutex_;
    std::condition_variable gcCondition_;

    // 設定
    GCStrategy strategy_{GCStrategy::Quantum};
    std::atomic<bool> generationalGC_{true};
    std::atomic<bool> adaptiveGC_{true};
    std::atomic<bool> quantumOptimization_{true};
    std::atomic<bool> predictiveCollection_{true};
    std::atomic<bool> memoryCompaction_{true};
    std::atomic<bool> weakReferenceSupport_{true};

    // 閾値・ターゲット
    std::atomic<uint32_t> youngGenThreshold_{1024 * 1024};    // 1MB
    std::atomic<uint32_t> middleGenThreshold_{8 * 1024 * 1024}; // 8MB
    std::atomic<uint32_t> oldGenThreshold_{64 * 1024 * 1024};   // 64MB
    std::atomic<uint32_t> promotionThreshold_{5};
    std::atomic<double> memoryPressureThreshold_{0.8};
    std::atomic<double> fragmentationThreshold_{0.3};
    std::chrono::milliseconds targetPauseTime_{10};
    std::atomic<double> targetThroughput_{0.95};

    // 統計
    mutable QuantumGCStats stats_;
    std::atomic<bool> debugMode_{false};
    std::vector<std::string> gcLog_;
    mutable std::mutex logMutex_;

    // 内部メソッド
    void initializeWorkers();
    void shutdownWorkers();
    void workerLoop(GCWorker* worker);

    // マーキングフェーズ
    void markPhase();
    void markObject(void* object, MarkingContext& context);
    void markRoots(MarkingContext& context);
    void markReachableObjects(MarkingContext& context);

    // スイープフェーズ
    void sweepPhase();
    void sweepGeneration(Generation gen, SweepingContext& context);
    void sweepRegion(HeapRegion* region, SweepingContext& context);

    // コンパクションフェーズ
    void compactPhase();
    void compactGeneration(Generation gen);
    void updateReferences(void* oldPtr, void* newPtr);

    // 世代管理
    void promoteObjects();
    bool shouldPromote(const ObjectMetadata& metadata);
    void moveToGeneration(void* object, Generation targetGen);

    // 適応的調整
    void adaptiveAdjustment();
    void adjustThresholds();
    void adjustStrategy();
    void predictNextCollection();

    // ユーティリティ
    Generation getObjectGeneration(void* object) const;
    ObjectMetadata* getObjectMetadata(void* object) const;
    void updateObjectAccess(void* object);
    void logGCEvent(const std::string& event);
    double calculateMemoryPressure() const;
    double calculateFragmentation() const;
    void updateStats(const std::string& operation, double duration, size_t bytesCollected);
};

/**
 * @brief ヒープ領域
 */
struct QuantumGC::HeapRegion {
    void* start = nullptr;
    size_t size = 0;
    size_t used = 0;
    Generation generation = Generation::Young;
    std::atomic<bool> inUse{false};
    std::vector<void*> objects;
    std::mutex regionMutex;

    HeapRegion(size_t regionSize, Generation gen);
    ~HeapRegion();

    void* allocate(size_t objectSize);
    void deallocate(void* object);
    double getUtilization() const;
    void compact();
};

/**
 * @brief GCワーカー
 */
struct QuantumGC::GCWorker {
    std::thread thread;
    std::atomic<bool> active{false};
    std::atomic<bool> working{false};
    std::mutex workMutex;
    std::condition_variable workCondition;
    std::function<void()> currentTask;

    GCWorker();
    ~GCWorker();

    void assignTask(std::function<void()> task);
    void waitForCompletion();
};

/**
 * @brief マーキングコンテキスト
 */
struct QuantumGC::MarkingContext {
    std::unordered_set<void*> markedObjects;
    std::vector<void*> markingStack;
    std::atomic<size_t> markedBytes{0};
    std::atomic<size_t> markedCount{0};
    std::mutex contextMutex;

    void markObject(void* object);
    bool isMarked(void* object) const;
    void pushToStack(void* object);
    void* popFromStack();
    bool isStackEmpty() const;
};

/**
 * @brief スイープコンテキスト
 */
struct QuantumGC::SweepingContext {
    std::atomic<size_t> sweptBytes{0};
    std::atomic<size_t> sweptCount{0};
    std::vector<void*> freedObjects;
    std::mutex contextMutex;

    void recordFreed(void* object, size_t size);
    std::vector<void*> getFreedObjects();
};

} // namespace memory
} // namespace utils
} // namespace aerojs

#endif // AEROJS_UTILS_MEMORY_GC_QUANTUM_GC_H 