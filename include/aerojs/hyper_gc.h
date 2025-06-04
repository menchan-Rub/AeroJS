/**
 * @file hyper_gc.h
 * @brief 超高速ガベージコレクタシステム
 */

#pragma once

#include <string>
#include <memory>
#include <functional>
#include <cstdint>

namespace aerojs {
namespace gc {

/**
 * @brief GC戦略
 */
enum class GCStrategy {
    Conservative,
    Generational,
    Incremental,
    Concurrent,
    Parallel,
    Adaptive,
    Predictive,
    Quantum,
    Transcendent
};

/**
 * @brief 世代
 */
enum class Generation {
    Young,
    Middle,
    Old,
    Permanent
};

/**
 * @brief GC統計
 */
struct GCStats {
    size_t totalCollections = 0;
    size_t youngCollections = 0;
    size_t middleCollections = 0;
    size_t oldCollections = 0;
    size_t fullCollections = 0;
    size_t concurrentCollections = 0;
    size_t parallelCollections = 0;
    size_t incrementalCollections = 0;
    size_t predictiveCollections = 0;
    size_t quantumCollections = 0;
    double totalGCTime = 0.0;
    double averageGCTime = 0.0;
    size_t totalMemoryFreed = 0;
    size_t peakMemoryUsage = 0;
};

/**
 * @brief GC設定
 */
struct HyperGCConfig {
    GCStrategy strategy = GCStrategy::Quantum;
    bool enableQuantumGC = true;
    bool enablePredictiveGC = true;
    bool enableConcurrentGC = true;
    bool enableParallelGC = true;
    size_t maxHeapSize = 2ULL * 1024 * 1024 * 1024; // 2GB
    size_t youngGenSize = 256 * 1024 * 1024; // 256MB
    size_t middleGenSize = 512 * 1024 * 1024; // 512MB
    size_t oldGenSize = 1024 * 1024 * 1024; // 1GB
    double gcThreshold = 0.8; // 80%
    size_t threadCount = 4;
};

/**
 * @brief 超高速ガベージコレクタ
 */
class HyperGC {
public:
    explicit HyperGC(const HyperGCConfig& config = HyperGCConfig{});
    ~HyperGC();

    // 初期化・終了
    bool initialize();
    void shutdown();

    // メモリ割り当て
    void* allocate(size_t size);
    void* allocateInGeneration(size_t size, Generation generation);
    void deallocate(void* ptr);

    // オブジェクト管理
    void pin(void* ptr);
    void unpin(void* ptr);
    void addFinalizer(void* ptr, std::function<void()> finalizer);

    // GC実行
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

    // ヒープ管理
    size_t getHeapSize() const;
    size_t getUsedHeapSize() const;
    size_t getFreeHeapSize() const;
    double getHeapUtilization() const;
    double getFragmentationRatio() const;

    // オブジェクト管理
    uint32_t getObjectCount() const;
    uint32_t getObjectCount(Generation generation) const;

    // 統計・レポート
    const GCStats& getStats() const;
    std::string getGCReport() const;
    std::string getHeapReport() const;
    std::string getPerformanceReport() const;

private:
    HyperGCConfig config_;
    GCStats stats_;
    bool initialized_ = false;
    size_t currentHeapSize_ = 0;
    size_t usedHeapSize_ = 0;
};

} // namespace gc
} // namespace aerojs 