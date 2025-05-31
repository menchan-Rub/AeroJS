/**
 * @file hyper_gc.h
 * @brief AeroJS 超高速ガベージコレクタシステム - 世界最高性能
 * @version 3.0.0
 * @license MIT
 */

#ifndef AEROJS_HYPER_GC_H
#define AEROJS_HYPER_GC_H

#include "aerojs.h"
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
namespace gc {

/**
 * @brief GC戦略
 */
enum class GCStrategy {
    Conservative,       // 保守的GC
    Generational,       // 世代別GC
    Incremental,        // インクリメンタルGC
    Concurrent,         // 並行GC
    Parallel,           // 並列GC
    Adaptive,           // 適応的GC
    Predictive,         // 予測的GC
    Quantum,            // 量子GC
    Transcendent        // 超越GC（実験的）
};

/**
 * @brief 世代
 */
enum class Generation {
    Young = 0,          // 若い世代
    Middle = 1,         // 中間世代
    Old = 2,            // 古い世代
    Permanent = 3       // 永続世代
};

/**
 * @brief GC統計
 */
struct HyperGCStats {
    std::atomic<uint64_t> totalCollections{0};
    std::atomic<uint64_t> youngCollections{0};
    std::atomic<uint64_t> middleCollections{0};
    std::atomic<uint64_t> oldCollections{0};
    std::atomic<uint64_t> fullCollections{0};
    std::atomic<uint64_t> concurrentCollections{0};
    std::atomic<uint64_t> parallelCollections{0};
    std::atomic<uint64_t> incrementalCollections{0};
    std::atomic<uint64_t> predictiveCollections{0};
    std::atomic<uint64_t> quantumCollections{0};
    
    std::atomic<uint64_t> totalGCTimeNs{0};
    std::atomic<uint64_t> averageGCTimeNs{0};
    std::atomic<uint64_t> maxGCTimeNs{0};
    std::atomic<uint64_t> minGCTimeNs{UINT64_MAX};
    
    std::atomic<uint64_t> totalBytesCollected{0};
    std::atomic<uint64_t> totalBytesAllocated{0};
    std::atomic<uint64_t> currentHeapSize{0};
    std::atomic<uint64_t> maxHeapSize{0};
    std::atomic<uint64_t> youngHeapSize{0};
    std::atomic<uint64_t> middleHeapSize{0};
    std::atomic<uint64_t> oldHeapSize{0};
    std::atomic<uint64_t> permanentHeapSize{0};
    
    std::atomic<uint32_t> objectCount{0};
    std::atomic<uint32_t> youngObjectCount{0};
    std::atomic<uint32_t> middleObjectCount{0};
    std::atomic<uint32_t> oldObjectCount{0};
    std::atomic<uint32_t> permanentObjectCount{0};
    
    std::atomic<double> gcEfficiency{0.0};
    std::atomic<double> heapUtilization{0.0};
    std::atomic<double> fragmentationRatio{0.0};
    std::atomic<double> promotionRate{0.0};
    std::atomic<double> survivalRate{0.0};
    
    void reset() {
        totalCollections = 0;
        youngCollections = 0;
        middleCollections = 0;
        oldCollections = 0;
        fullCollections = 0;
        concurrentCollections = 0;
        parallelCollections = 0;
        incrementalCollections = 0;
        predictiveCollections = 0;
        quantumCollections = 0;
        
        totalGCTimeNs = 0;
        averageGCTimeNs = 0;
        maxGCTimeNs = 0;
        minGCTimeNs = UINT64_MAX;
        
        totalBytesCollected = 0;
        totalBytesAllocated = 0;
        currentHeapSize = 0;
        maxHeapSize = 0;
        youngHeapSize = 0;
        middleHeapSize = 0;
        oldHeapSize = 0;
        permanentHeapSize = 0;
        
        objectCount = 0;
        youngObjectCount = 0;
        middleObjectCount = 0;
        oldObjectCount = 0;
        permanentObjectCount = 0;
        
        gcEfficiency = 0.0;
        heapUtilization = 0.0;
        fragmentationRatio = 0.0;
        promotionRate = 0.0;
        survivalRate = 0.0;
    }
};

/**
 * @brief GC設定
 */
struct HyperGCConfig {
    GCStrategy strategy = GCStrategy::Adaptive;
    
    // ヒープサイズ設定
    size_t initialHeapSize = 64 * 1024 * 1024;      // 64MB
    size_t maxHeapSize = 8ULL * 1024 * 1024 * 1024; // 8GB
    size_t youngGenerationSize = 16 * 1024 * 1024;   // 16MB
    size_t middleGenerationSize = 32 * 1024 * 1024;  // 32MB
    size_t oldGenerationSize = 128 * 1024 * 1024;    // 128MB
    
    // GCトリガー設定
    double youngGCThreshold = 0.8;      // 80%で若い世代GC
    double middleGCThreshold = 0.85;    // 85%で中間世代GC
    double oldGCThreshold = 0.9;        // 90%で古い世代GC
    double fullGCThreshold = 0.95;      // 95%でフルGC
    
    // 並行・並列設定
    uint32_t maxGCThreads = std::thread::hardware_concurrency();
    bool enableConcurrentGC = true;
    bool enableParallelGC = true;
    bool enableIncrementalGC = true;
    bool enablePredictiveGC = true;
    bool enableQuantumGC = true;
    bool enableAdaptiveGC = true;
    
    // 最適化設定
    bool enableCompaction = true;
    bool enableDeduplication = true;
    bool enablePrefetching = true;
    bool enableWriteBarrier = true;
    bool enableCardMarking = true;
    bool enableRememberedSet = true;
    
    // タイミング設定
    uint32_t gcIntervalMs = 100;        // 100ms間隔でGCチェック
    uint32_t maxGCPauseMs = 10;         // 最大10msのGC停止時間
    uint32_t incrementalStepMs = 1;     // 1msのインクリメンタルステップ
    
    // 予測設定
    uint32_t allocationPredictionWindow = 1000;  // 1000回の割り当てで予測
    double allocationRateThreshold = 1.5;       // 1.5倍の割り当て率で予測GC
    
    // デバッグ設定
    bool enableGCLogging = false;
    bool enableGCProfiling = false;
    bool enableGCVisualization = false;
};

/**
 * @brief GCオブジェクト
 */
struct GCObject {
    void* data = nullptr;
    size_t size = 0;
    Generation generation = Generation::Young;
    uint32_t age = 0;
    bool marked = false;
    bool pinned = false;
    bool forwarded = false;
    void* forwardingAddress = nullptr;
    std::chrono::steady_clock::time_point allocationTime;
    std::chrono::steady_clock::time_point lastAccessTime;
    std::vector<GCObject*> references;
    std::function<void()> finalizer;
};

/**
 * @brief 超高速ガベージコレクタ
 */
class HyperGC {
public:
    explicit HyperGC(const HyperGCConfig& config = HyperGCConfig{});
    ~HyperGC();

    // 初期化・終了処理
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized_; }

    // メモリ管理
    void* allocate(size_t size);
    void* allocateInGeneration(size_t size, Generation generation);
    void deallocate(void* ptr);
    void pin(void* ptr);
    void unpin(void* ptr);
    void addFinalizer(void* ptr, std::function<void()> finalizer);

    // GC実行
    void collect();
    void collectGeneration(Generation generation);
    void collectYoung();
    void collectMiddle();
    void collectOld();
    void collectFull();
    void collectConcurrent();
    void collectParallel();
    void collectIncremental();
    void collectPredictive();
    void collectQuantum();

    // 適応的GC
    void performAdaptiveCollection();
    void analyzePredictivePatterns();
    void optimizeGenerationSizes();
    void tuneGCParameters();

    // 統計・設定
    const HyperGCStats& getStats() const { return stats_; }
    void resetStats() { stats_.reset(); }
    void setConfig(const HyperGCConfig& config) { config_ = config; }
    const HyperGCConfig& getConfig() const { return config_; }

    // ヒープ管理
    size_t getHeapSize() const;
    size_t getUsedHeapSize() const;
    size_t getFreeHeapSize() const;
    size_t getGenerationSize(Generation generation) const;
    double getHeapUtilization() const;
    double getFragmentationRatio() const;

    // オブジェクト管理
    uint32_t getObjectCount() const;
    uint32_t getObjectCount(Generation generation) const;
    std::vector<GCObject*> getObjects() const;
    std::vector<GCObject*> getObjects(Generation generation) const;

    // デバッグ・診断
    std::string getGCReport() const;
    std::string getHeapReport() const;
    std::string getPerformanceReport() const;
    void dumpHeap() const;
    void visualizeHeap() const;

    // イベントハンドラ
    void setGCStartHandler(std::function<void(GCStrategy)> handler);
    void setGCEndHandler(std::function<void(GCStrategy, uint64_t)> handler);
    void setAllocationHandler(std::function<void(void*, size_t)> handler);
    void setDeallocationHandler(std::function<void(void*)> handler);

private:
    // 内部実装
    bool initializeHeap();
    bool initializeGCThreads();
    void shutdownGCThreads();
    void gcWorker();
    void backgroundGCWorker();
    
    void markPhase();
    void sweepPhase();
    void compactPhase();
    void promoteObjects();
    void updateReferences();
    
    void markObject(GCObject* object);
    void sweepGeneration(Generation generation);
    void compactGeneration(Generation generation);
    void promoteFromGeneration(Generation from, Generation to);
    
    bool shouldCollect() const;
    bool shouldCollectGeneration(Generation generation) const;
    GCStrategy selectOptimalStrategy() const;
    
    void updateStats(const std::string& operation, uint64_t value);
    void recordGCEvent(GCStrategy strategy, uint64_t duration);
    void predictNextGC();

private:
    HyperGCConfig config_;
    HyperGCStats stats_;
    bool initialized_ = false;
    
    // ヒープ管理
    std::unordered_map<Generation, std::vector<GCObject*>> heapGenerations_;
    std::unordered_map<void*, GCObject*> objectMap_;
    std::unordered_set<GCObject*> pinnedObjects_;
    std::unordered_set<GCObject*> rootObjects_;
    
    // GCスレッド
    std::vector<std::thread> gcThreads_;
    std::thread backgroundGCThread_;
    std::atomic<bool> shutdownRequested_{false};
    std::atomic<bool> gcInProgress_{false};
    
    // 同期プリミティブ
    mutable std::mutex heapMutex_;
    mutable std::mutex statsMutex_;
    mutable std::mutex gcMutex_;
    std::condition_variable gcCondition_;
    
    // イベントハンドラ
    std::function<void(GCStrategy)> gcStartHandler_;
    std::function<void(GCStrategy, uint64_t)> gcEndHandler_;
    std::function<void(void*, size_t)> allocationHandler_;
    std::function<void(void*)> deallocationHandler_;
    
    // 予測・適応
    std::vector<uint64_t> allocationHistory_;
    std::vector<uint64_t> gcHistory_;
    std::chrono::steady_clock::time_point lastGCTime_;
    std::chrono::steady_clock::time_point nextPredictedGC_;
};

} // namespace gc
} // namespace aerojs

#endif // AEROJS_HYPER_GC_H 