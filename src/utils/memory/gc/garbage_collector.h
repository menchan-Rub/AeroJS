/**
 * @file garbage_collector.h
 * @brief AeroJS ガベージコレクタヘッダー
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_UTILS_MEMORY_GC_GARBAGE_COLLECTOR_H
#define AEROJS_UTILS_MEMORY_GC_GARBAGE_COLLECTOR_H

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <vector>
#include <memory>
#include <atomic>

namespace aerojs {
namespace utils {
namespace memory {

// 前方宣言
class MemoryAllocator;
class MemoryPool;

/**
 * @brief ガベージコレクションモード
 */
enum class GCMode {
    MARK_SWEEP,      // マーク&スイープ
    GENERATIONAL,    // 世代別GC
    INCREMENTAL,     // インクリメンタルGC
    CONCURRENT       // 並行GC
};

/**
 * @brief GC統計情報
 */
struct GCStats {
    size_t totalCollections{0};      // 総GC実行回数
    uint64_t totalCollectionTime{0}; // 総GC実行時間（マイクロ秒）
    uint64_t lastCollectionTime{0};  // 最後のGC実行時間（マイクロ秒）
    size_t objectsCollected{0};      // 回収されたオブジェクト数
    size_t bytesCollected{0};        // 回収されたバイト数
    double averageCollectionTime{0}; // 平均GC実行時間
};

/**
 * @brief ガベージコレクタ基底クラス
 */
class GCCollector {
public:
    virtual ~GCCollector() = default;

    /**
     * @brief ガベージコレクションを実行
     */
    virtual void collect() = 0;

    /**
     * @brief オブジェクトをマーク
     * @param object マークするオブジェクト
     */
    virtual void markObject(void* object) = 0;

    /**
     * @brief ルートオブジェクトを追加
     * @param root ルートオブジェクト
     */
    virtual void addRoot(void* root) = 0;

    /**
     * @brief ルートオブジェクトを削除
     * @param root ルートオブジェクト
     */
    virtual void removeRoot(void* root) = 0;

    /**
     * @brief 統計情報を取得
     * @return GCStats 統計情報
     */
    virtual GCStats getStats() const = 0;
};

/**
 * @brief マーク&スイープガベージコレクタ
 */
class MarkSweepCollector : public GCCollector {
public:
    explicit MarkSweepCollector(MemoryAllocator* allocator);
    ~MarkSweepCollector() override;

    void collect() override;
    void markObject(void* object) override;
    void addRoot(void* root) override;
    void removeRoot(void* root) override;
    GCStats getStats() const override;

private:
    MemoryAllocator* allocator_;
    std::unordered_set<void*> roots_;
    std::unordered_set<void*> markedObjects_;
    mutable std::mutex mutex_;
    GCStats stats_;

    void markPhase();
    void sweepPhase();
    void markReachableObjects();
    void sweepUnmarkedObjects();
};

/**
 * @brief 世代別ガベージコレクタ
 */
class GenerationalCollector : public GCCollector {
public:
    explicit GenerationalCollector(MemoryAllocator* allocator);
    ~GenerationalCollector() override;

    void collect() override;
    void markObject(void* object) override;
    void addRoot(void* root) override;
    void removeRoot(void* root) override;
    GCStats getStats() const override;

    /**
     * @brief 若い世代のGCを実行
     */
    void collectYoungGeneration();

    /**
     * @brief 古い世代のGCを実行
     */
    void collectOldGeneration();

private:
    MemoryAllocator* allocator_;
    std::unordered_set<void*> roots_;
    std::unordered_set<void*> youngGeneration_;
    std::unordered_set<void*> oldGeneration_;
    mutable std::mutex mutex_;
    GCStats stats_;
    size_t youngGenThreshold_;

    void promoteToOldGeneration(void* object);
    bool shouldPromote(void* object) const;
};

/**
 * @brief ガベージコレクタマネージャー
 */
class GarbageCollector {
public:
    explicit GarbageCollector(MemoryAllocator* allocator, MemoryPool* pool = nullptr);
    ~GarbageCollector();

    /**
     * @brief ガベージコレクションを実行
     */
    void collect();

    /**
     * @brief GCモードを設定
     * @param mode GCモード
     */
    void setMode(GCMode mode);

    /**
     * @brief GCモードを取得
     * @return GCMode 現在のGCモード
     */
    GCMode getMode() const;

    /**
     * @brief GC閾値を設定
     * @param threshold 閾値（バイト）
     */
    void setThreshold(size_t threshold);

    /**
     * @brief GC閾値を取得
     * @return size_t 現在の閾値
     */
    size_t getThreshold() const;

    /**
     * @brief 最大ヒープサイズを設定
     * @param maxSize 最大サイズ（バイト）
     */
    void setMaxHeapSize(size_t maxSize);

    /**
     * @brief 最大ヒープサイズを取得
     * @return size_t 最大ヒープサイズ
     */
    size_t getMaxHeapSize() const;

    /**
     * @brief GCが実行中かどうか
     * @return bool 実行中の場合true
     */
    bool isRunning() const;

    /**
     * @brief 総GC実行回数を取得
     * @return size_t 総実行回数
     */
    size_t getTotalCollections() const;

    /**
     * @brief 総GC実行時間を取得
     * @return uint64_t 総実行時間（マイクロ秒）
     */
    uint64_t getTotalCollectionTime() const;

    /**
     * @brief 最後のGC実行時間を取得
     * @return uint64_t 最後の実行時間（マイクロ秒）
     */
    uint64_t getLastCollectionTime() const;

    /**
     * @brief ルートオブジェクトを追加
     * @param root ルートオブジェクト
     */
    void addRoot(void* root);

    /**
     * @brief ルートオブジェクトを削除
     * @param root ルートオブジェクト
     */
    void removeRoot(void* root);

    /**
     * @brief 統計情報を取得
     * @return GCStats 統計情報
     */
    GCStats getStats() const;

private:
    MemoryAllocator* allocator_;
    MemoryPool* pool_;
    GCMode mode_;
    std::unique_ptr<GCCollector> collector_;
    
    std::unordered_set<void*> roots_;
    std::unordered_set<void*> markedObjects_;
    mutable std::mutex rootsMutex_;
    
    std::atomic<bool> running_;
    size_t totalCollections_;
    uint64_t totalCollectionTime_;
    uint64_t lastCollectionTime_;
    size_t threshold_;
    size_t maxHeapSize_;

    // GC実装メソッド
    void performMarkSweep();
    void performGenerational();
    void performIncremental();
    void performConcurrent();

    // ヘルパーメソッド
    void markReachableObjects();
    void markObject(void* object);
    void sweepUnmarkedObjects();
    bool shouldCompact() const;
    void compactHeap();
    
    // 世代別GC用
    void collectYoungGeneration();
    void collectOldGeneration();
    bool shouldCollectOldGeneration() const;
    
    // インクリメンタルGC用
    void incrementalMark();
    void incrementalSweep();
};

} // namespace memory
} // namespace utils
} // namespace aerojs

#endif // AEROJS_UTILS_MEMORY_GC_GARBAGE_COLLECTOR_H 