/**
 * @file incremental_gc.h
 * @brief 増分ガベージコレクション実装
 * 
 * このファイルは、実行時停止時間を最小化する増分ガベージコレクションを実装します。
 * 三色マーキングアルゴリズムとライトバリアを使用して、JavaScriptの実行を止めることなく
 * メモリ回収を行います。
 * 
 * @author AeroJS Team
 * @version 2.0.0
 * @copyright MIT License
 */

#ifndef AEROJS_INCREMENTAL_GC_H
#define AEROJS_INCREMENTAL_GC_H

#include "garbage_collector.h"
#include "../../containers/vector.h"
#include "../../containers/hashmap.h"
#include "../../../runtime/values/value.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>

namespace aerojs {
namespace utils {
namespace memory {

// オブジェクトの色状態（三色マーキング）
enum class ObjectColor : uint8_t {
    WHITE = 0,    // 未マーク（回収対象候補）
    GRAY = 1,     // マーク済みだが子オブジェクト未処理
    BLACK = 2     // マーク済みかつ子オブジェクト処理済み
};

// ライトバリアの種類
enum class WriteBarrierType : uint8_t {
    NONE = 0,
    SNAPSHOT_AT_BEGINNING = 1,   // スナップショット・アット・ザ・ビギニング
    INCREMENTAL_UPDATE = 2,      // インクリメンタル・アップデート
    GENERATIONAL = 3             // 世代別バリア
};

// GCフェーズ
enum class GCPhase : uint8_t {
    IDLE = 0,           // 非動作中
    MARKING = 1,        // マーキングフェーズ
    SWEEPING = 2,       // スイープフェーズ
    FINALIZING = 3      // ファイナライゼーション
};

// 増分GCの統計情報
struct IncrementalGCStats {
    size_t totalCollections = 0;
    size_t totalIncrements = 0;
    double totalMarkingTime = 0.0;     // ミリ秒
    double totalSweepingTime = 0.0;    // ミリ秒
    double averageIncrementTime = 0.0;  // ミリ秒
    size_t maxIncrementTime = 0;       // マイクロ秒
    size_t objectsMarked = 0;
    size_t objectsSwept = 0;
    size_t writeBarrierActivations = 0;
    double mutatorUtilization = 0.0;   // 実行時間の割合
};

// オブジェクトヘッダ（GC用メタデータ）
struct ObjectHeader {
    ObjectColor color : 2;
    bool marked : 1;
    bool finalizable : 1;
    uint8_t generation : 2;
    uint8_t reserved : 2;
    uint32_t size;
    void* typeInfo;
} __attribute__((packed));

// 増分GCクラス
class IncrementalGC : public GarbageCollector {
public:
    IncrementalGC();
    ~IncrementalGC() override;
    
    // GarbageCollectorインターフェースの実装
    void* Allocate(size_t size, size_t alignment = sizeof(void*)) override;
    void Collect() override;
    void RegisterRoot(void** root) override;
    void UnregisterRoot(void** root) override;
    size_t GetHeapSize() const override;
    size_t GetUsedMemory() const override;
    
    // 増分GC固有のメソッド
    void PerformIncrement(size_t maxTimeUs = 1000);  // デフォルト1ms
    void SetIncrementBudget(size_t timeUs) { incrementBudgetUs_ = timeUs; }
    void SetWriteBarrierType(WriteBarrierType type) { barrierType_ = type; }
    
    // ライトバリア
    void WriteBarrier(void* object, void* field, void* newValue);
    void ReadBarrier(void* object, void* field);
    
    // 状態取得
    GCPhase GetCurrentPhase() const { return currentPhase_; }
    bool IsRunning() const { return currentPhase_ != GCPhase::IDLE; }
    double GetProgressPercent() const;
    
    // 統計情報
    const IncrementalGCStats& GetStats() const { return stats_; }
    void ResetStats();
    
    // 設定
    void SetConcurrentMode(bool enable) { concurrentMode_ = enable; }
    void SetTargetHeapUtilization(double ratio) { targetHeapUtilization_ = ratio; }
    void SetAllocationRate(size_t bytesPerSecond) { allocationRate_ = bytesPerSecond; }
    
    // デバッグ・プロファイリング
    void EnableDebugMode(bool enable) { debugMode_ = enable; }
    void DumpHeapSnapshot(const std::string& filename);
    void ValidateHeap();

private:
    // フェーズ別処理
    void StartCollection();
    void PerformMarkingIncrement(size_t budgetUs);
    void PerformSweepingIncrement(size_t budgetUs);
    void FinalizeCollection();
    
    // マーキング
    void InitializeMarking();
    void MarkRoots();
    void MarkObject(void* object);
    void MarkGrayObjects(size_t budgetUs);
    void BlackenObject(void* object);
    void ScanObject(void* object);
    
    // スイープ
    void InitializeSweeping();
    void SweepPages(size_t budgetUs);
    void SweepPage(void* pageStart, size_t pageSize);
    void FreeObject(void* object);
    
    // ライトバリア実装
    void SnapshotAtBeginningBarrier(void* object, void* field, void* newValue);
    void IncrementalUpdateBarrier(void* object, void* field, void* newValue);
    void GenerationalBarrier(void* object, void* field, void* newValue);
    
    // オブジェクト操作
    ObjectHeader* GetObjectHeader(void* object);
    ObjectColor GetObjectColor(void* object);
    void SetObjectColor(void* object, ObjectColor color);
    size_t GetObjectSize(void* object);
    void** GetObjectFields(void* object, size_t& fieldCount);
    
    // ヒープ管理
    void* AllocateFromHeap(size_t size, size_t alignment);
    void AddToGrayStack(void* object);
    void* PopFromGrayStack();
    bool IsGrayStackEmpty() const;
    
    // 並行処理
    void StartConcurrentMarking();
    void StopConcurrentMarking();
    void ConcurrentMarkingThread();
    
    // タイミング制御
    void UpdateAllocationRate();
    size_t CalculateIncrementBudget();
    bool ShouldTriggerCollection();
    double CalculateMutatorUtilization();
    
    // 統計・デバッグ
    void RecordIncrementTime(size_t timeUs);
    void UpdateStats(GCPhase phase, size_t timeUs);
    void LogGCEvent(const std::string& event);
    
    // ヘルパー関数
    bool IsValidPointer(void* ptr);
    bool IsInHeap(void* ptr);
    void* AlignPointer(void* ptr, size_t alignment);
    size_t AlignSize(size_t size, size_t alignment);

private:
    // GCの状態
    std::atomic<GCPhase> currentPhase_;
    std::atomic<bool> collectionRequested_;
    std::atomic<bool> shouldStop_;
    
    // マーキング状態
    std::vector<void*> grayStack_;
    std::unordered_set<void*> markedObjects_;
    size_t markingProgress_;
    size_t totalObjectsToMark_;
    
    // スイープ状態
    std::vector<void*> heapPages_;
    size_t sweepingProgress_;
    size_t totalPagesToSweep_;
    std::vector<void*> freedObjects_;
    
    // ルートセット
    std::unordered_set<void**> rootSet_;
    mutable std::mutex rootSetMutex_;
    
    // ヒープ管理
    std::vector<void*> allocatedPages_;
    size_t heapSize_;
    size_t usedMemory_;
    size_t lastAllocationSize_;
    
    // タイミング制御
    size_t incrementBudgetUs_;
    std::chrono::steady_clock::time_point lastIncrementTime_;
    std::chrono::steady_clock::time_point collectionStartTime_;
    
    // 割り当て追跡
    size_t allocationsSinceLastGC_;
    size_t bytesAllocatedSinceLastGC_;
    size_t allocationRate_;  // bytes/second
    std::chrono::steady_clock::time_point lastAllocationTime_;
    
    // 設定
    WriteBarrierType barrierType_;
    bool concurrentMode_;
    double targetHeapUtilization_;
    bool debugMode_;
    
    // 並行処理
    std::unique_ptr<std::thread> concurrentThread_;
    std::atomic<bool> concurrentMarkingActive_;
    mutable std::mutex grayStackMutex_;
    
    // 統計情報
    IncrementalGCStats stats_;
    std::vector<size_t> incrementTimes_;
    std::chrono::steady_clock::time_point totalStartTime_;
    
    // メモリページ管理
    static constexpr size_t PAGE_SIZE = 4096;
    static constexpr size_t OBJECT_ALIGNMENT = 8;
    static constexpr size_t MIN_OBJECT_SIZE = 16;
    static constexpr size_t MAX_INCREMENT_TIME_US = 2000;  // 2ms
    static constexpr double DEFAULT_HEAP_UTILIZATION = 0.7;
    static constexpr size_t GRAY_STACK_INITIAL_SIZE = 1024;
    
    // デバッグ・検証
    #ifdef DEBUG
    std::unordered_map<void*, std::string> objectTypes_;
    std::unordered_map<void*, size_t> allocationSizes_;
    size_t totalAllocations_;
    size_t totalDeallocations_;
    #endif
};

// ライトバリア用のマクロ
#define AEROJS_WRITE_BARRIER(gc, obj, field, value) \
    do { \
        if ((gc)->IsRunning()) { \
            (gc)->WriteBarrier((obj), &(obj)->field, (value)); \
        } \
        (obj)->field = (value); \
    } while(0)

#define AEROJS_READ_BARRIER(gc, obj, field) \
    do { \
        if ((gc)->IsRunning()) { \
            (gc)->ReadBarrier((obj), &(obj)->field); \
        } \
    } while(0)

// 増分GCファクトリ
class IncrementalGCFactory {
public:
    static std::unique_ptr<IncrementalGC> Create(
        size_t initialHeapSize = 64 * 1024 * 1024,  // 64MB
        WriteBarrierType barrierType = WriteBarrierType::SNAPSHOT_AT_BEGINNING,
        bool concurrentMode = true
    );
    
    static void Configure(
        IncrementalGC* gc,
        size_t incrementBudgetUs = 1000,
        double targetUtilization = 0.7,
        bool debugMode = false
    );
};

// ヒープダンプ用の構造体
struct HeapSnapshot {
    struct ObjectInfo {
        void* address;
        size_t size;
        ObjectColor color;
        std::string type;
        std::vector<void*> references;
    };
    
    std::vector<ObjectInfo> objects;
    size_t totalSize;
    size_t totalObjects;
    std::chrono::steady_clock::time_point timestamp;
};

// パフォーマンス測定用のヘルパークラス
class GCProfiler {
public:
    GCProfiler(IncrementalGC* gc) : gc_(gc) {
        startTime_ = std::chrono::high_resolution_clock::now();
    }
    
    ~GCProfiler() {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime_).count();
        
        if (gc_) {
            gc_->RecordIncrementTime(duration);
        }
    }

private:
    IncrementalGC* gc_;
    std::chrono::high_resolution_clock::time_point startTime_;
};

#define AEROJS_GC_PROFILE(gc) GCProfiler _profiler(gc)

} // namespace memory
} // namespace utils
} // namespace aerojs

#endif // AEROJS_INCREMENTAL_GC_H 