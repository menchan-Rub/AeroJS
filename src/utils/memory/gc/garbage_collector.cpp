/**
 * @file garbage_collector.cpp
 * @brief AeroJS ガベージコレクタ実装
 * @version 0.1.0
 * @license MIT
 */

#include "garbage_collector.h"
#include "../allocators/memory_allocator.h"
#include "../pool/memory_pool.h"
#include <algorithm>
#include <chrono>
#include <thread>

namespace aerojs {
namespace utils {
namespace memory {

GarbageCollector::GarbageCollector(MemoryAllocator* allocator, MemoryPool* pool)
    : allocator_(allocator),
      pool_(pool),
      mode_(GCMode::MARK_SWEEP),
      isRunning_(false),
      totalCollections_(0),
      totalCollectionTime_(0),
      lastCollectionTime_(0),
      threshold_(1024 * 1024), // 1MB
      maxHeapSize_(512 * 1024 * 1024) { // 512MB
}

GarbageCollector::~GarbageCollector() = default;

void GarbageCollector::collect() {
    if (isRunning_) {
        return; // 既にGCが実行中
    }
    
    isRunning_ = true;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        switch (mode_) {
            case GCMode::MARK_SWEEP:
                performMarkSweep();
                break;
            case GCMode::GENERATIONAL:
                performGenerational();
                break;
            case GCMode::INCREMENTAL:
                performIncremental();
                break;
            case GCMode::CONCURRENT:
                performConcurrent();
                break;
        }
        
        totalCollections_++;
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        lastCollectionTime_ = duration.count();
        totalCollectionTime_ += lastCollectionTime_;
        
    } catch (...) {
        // GCエラーの処理
    }
    
    isRunning_ = false;
}

void GarbageCollector::setMode(GCMode mode) {
    if (!isRunning_) {
        mode_ = mode;
    }
}

GCMode GarbageCollector::getMode() const {
    return mode_;
}

void GarbageCollector::setThreshold(size_t threshold) {
    threshold_ = threshold;
}

size_t GarbageCollector::getThreshold() const {
    return threshold_;
}

void GarbageCollector::setMaxHeapSize(size_t maxSize) {
    maxHeapSize_ = maxSize;
}

size_t GarbageCollector::getMaxHeapSize() const {
    return maxHeapSize_;
}

bool GarbageCollector::isRunning() const {
    return isRunning_;
}

size_t GarbageCollector::getTotalCollections() const {
    return totalCollections_;
}

uint64_t GarbageCollector::getTotalCollectionTime() const {
    return totalCollectionTime_;
}

uint64_t GarbageCollector::getLastCollectionTime() const {
    return lastCollectionTime_;
}

void GarbageCollector::addRoot(void* root) {
    std::lock_guard<std::mutex> lock(rootsMutex_);
    roots_.insert(root);
}

void GarbageCollector::removeRoot(void* root) {
    std::lock_guard<std::mutex> lock(rootsMutex_);
    roots_.erase(root);
}

void GarbageCollector::performMarkSweep() {
    // マーク＆スイープGCの実装
    
    // 1. マークフェーズ：到達可能なオブジェクトをマーク
    markReachableObjects();
    
    // 2. スイープフェーズ：未マークのオブジェクトを回収
    sweepUnmarkedObjects();
    
    // 3. コンパクションフェーズ（オプション）
    if (shouldCompact()) {
        compactHeap();
    }
}

void GarbageCollector::performGenerational() {
    // 世代別GCの実装
    // 若い世代のGCを頻繁に、古い世代のGCを稀に実行
    
    // 若い世代のGC
    collectYoungGeneration();
    
    // 必要に応じて古い世代のGC
    if (shouldCollectOldGeneration()) {
        collectOldGeneration();
    }
}

void GarbageCollector::performIncremental() {
    // インクリメンタルGCの実装
    // GCを小さなステップに分割して実行
    
    static int phase = 0;
    const int maxPhases = 10;
    
    switch (phase % maxPhases) {
        case 0: case 1: case 2:
            // マークフェーズを分割実行
            incrementalMark();
            break;
        case 3: case 4:
            // スイープフェーズを分割実行
            incrementalSweep();
            break;
        default:
            // 休止フェーズ
            break;
    }
    
    phase++;
}

void GarbageCollector::performConcurrent() {
    // 並行GCの実装
    // メインスレッドと並行してGCを実行
    
    // 簡易実装：バックグラウンドでマーク＆スイープを実行
    std::thread gcThread([this]() {
        performMarkSweep();
    });
    
    gcThread.detach();
}

void GarbageCollector::markReachableObjects() {
    std::lock_guard<std::mutex> lock(rootsMutex_);
    
    // ルートオブジェクトから開始してマーク
    for (void* root : roots_) {
        markObject(root);
    }
}

void GarbageCollector::markObject(void* object) {
    if (!object || markedObjects_.count(object)) {
        return; // nullまたは既にマーク済み
    }
    
    markedObjects_.insert(object);
    
    // オブジェクトが参照する他のオブジェクトも再帰的にマーク
    // 実際の実装では、オブジェクトの型に応じて参照を辿る
}

void GarbageCollector::sweepUnmarkedObjects() {
    // アロケータから全オブジェクトを取得し、未マークのものを回収
    auto allocatedObjects = allocator_->getAllocatedObjects();
    
    for (void* object : allocatedObjects) {
        if (markedObjects_.count(object) == 0) {
            // 未マークのオブジェクトを回収
            allocator_->deallocate(object);
        }
    }
    
    // マークをクリア
    markedObjects_.clear();
}

bool GarbageCollector::shouldCompact() const {
    // ヒープの断片化率に基づいてコンパクションの必要性を判断
    size_t totalSize = allocator_->getTotalAllocatedSize();
    size_t usedSize = allocator_->getCurrentAllocatedSize();
    
    if (totalSize == 0) return false;
    
    double fragmentationRatio = 1.0 - (static_cast<double>(usedSize) / totalSize);
    return fragmentationRatio > 0.3; // 30%以上断片化している場合
}

void GarbageCollector::compactHeap() {
    // ヒープのコンパクション実装
    // 生きているオブジェクトを連続したメモリ領域に移動
}

void GarbageCollector::collectYoungGeneration() {
    // 若い世代のGC実装
}

void GarbageCollector::collectOldGeneration() {
    // 古い世代のGC実装
}

bool GarbageCollector::shouldCollectOldGeneration() const {
    // 古い世代のGCが必要かどうかを判断
    return totalCollections_ % 10 == 0; // 10回に1回
}

void GarbageCollector::incrementalMark() {
    // インクリメンタルマークの実装
}

void GarbageCollector::incrementalSweep() {
    // インクリメンタルスイープの実装
}

} // namespace memory
} // namespace utils
} // namespace aerojs 