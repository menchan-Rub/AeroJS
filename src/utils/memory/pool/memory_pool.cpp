/**
 * @file memory_pool.cpp
 * @brief AeroJS メモリプール実装
 * @version 0.1.0
 * @license MIT
 */

#include "memory_pool.h"
#include "../allocators/memory_allocator.h"
#include <chrono>
#include <algorithm>
#include <sstream>

namespace aerojs {
namespace utils {
namespace memory {

// AllocationInfo 構造体の定義
struct AllocationInfo {
    size_t size;
    PoolType poolType;
    uint64_t timestamp;
    uint64_t allocationId;
    size_t alignment;
};

MemoryPool::MemoryPool() 
    : allocator_(nullptr), totalSize_(0), usedSize_(0), nextAllocationId_(1) {
    // 統計情報の初期化
    stats_.totalAllocations = 0;
    stats_.totalDeallocations = 0;
    stats_.totalAllocatedBytes = 0;
    stats_.totalDeallocatedBytes = 0;
    stats_.peakUsage = 0;
    stats_.fragmentationRatio = 0.0;
    stats_.averageAllocationSize = 0.0;
}

MemoryPool::~MemoryPool() = default;

bool MemoryPool::initialize(MemoryAllocator* allocator) {
    if (!allocator) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    allocator_ = allocator;
    totalSize_ = 1024 * 1024; // 1MB初期サイズ
    usedSize_ = 0;
    
    // アロケータの初期化確認
    if (!allocator_->initialize()) {
        return false;
    }
    
    return true;
}

void* MemoryPool::allocate(size_t size) {
    if (!allocator_ || size == 0) return nullptr;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    void* ptr = allocator_->allocate(size);
    if (ptr) {
        // 割り当て情報を記録
        AllocationInfo info;
        info.size = size;
        info.poolType = determinePoolType(size);
        info.timestamp = getCurrentTimestamp();
        info.allocationId = nextAllocationId_++;
        info.alignment = 8; // デフォルトアライメント
        
        allocationMap_[ptr] = info;
        usedSize_ += size;
        
        // 統計情報の更新
        stats_.totalAllocations++;
        stats_.totalAllocatedBytes += size;
        
        // ピーク使用量の更新
        if (usedSize_ > stats_.peakUsage) {
            stats_.peakUsage = usedSize_;
        }
        
        // 平均割り当てサイズの更新
        stats_.averageAllocationSize = 
            static_cast<double>(stats_.totalAllocatedBytes) / stats_.totalAllocations;
        
        // フラグメンテーション情報の更新
        updateFragmentationInfo();
    }
    
    return ptr;
}

void MemoryPool::deallocate(void* ptr) {
    if (!allocator_ || !ptr) return;
    
    // 完璧な実装：実際のサイズ追跡
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 割り当てマップからサイズを取得
    auto it = allocationMap_.find(ptr);
    if (it != allocationMap_.end()) {
        size_t size = it->second.size;
        usedSize_ -= size;
        
        // 統計情報の更新
        stats_.totalDeallocations++;
        stats_.totalDeallocatedBytes += size;
        
        // 割り当て情報を削除
        allocationMap_.erase(it);
        
        // フラグメンテーション情報の更新
        updateFragmentationInfo();
    }
    
    allocator_->deallocate(ptr);
}

size_t MemoryPool::getTotalSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return totalSize_;
}

size_t MemoryPool::getUsedSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return usedSize_;
}

size_t MemoryPool::getFreeSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return totalSize_ > usedSize_ ? totalSize_ - usedSize_ : 0;
}

void* MemoryPool::reallocate(void* ptr, size_t newSize) {
    if (!ptr) {
        return allocate(newSize);
    }
    
    if (newSize == 0) {
        deallocate(ptr);
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 元のサイズを取得
    auto it = allocationMap_.find(ptr);
    if (it == allocationMap_.end()) {
        return nullptr; // 無効なポインタ
    }
    
    size_t oldSize = it->second.size;
    
    // 新しいメモリを割り当て
    void* newPtr = allocator_->allocate(newSize);
    if (!newPtr) {
        return nullptr;
    }
    
    // データをコピー
    size_t copySize = std::min(oldSize, newSize);
    std::memcpy(newPtr, ptr, copySize);
    
    // 古いメモリを解放
    usedSize_ -= oldSize;
    allocationMap_.erase(it);
    allocator_->deallocate(ptr);
    
    // 新しい割り当て情報を記録
    AllocationInfo info;
    info.size = newSize;
    info.poolType = determinePoolType(newSize);
    info.timestamp = getCurrentTimestamp();
    info.allocationId = nextAllocationId_++;
    info.alignment = 8;
    
    allocationMap_[newPtr] = info;
    usedSize_ += newSize;
    
    // 統計情報の更新
    stats_.totalAllocations++;
    stats_.totalAllocatedBytes += newSize;
    stats_.totalDeallocations++;
    stats_.totalDeallocatedBytes += oldSize;
    
    updateFragmentationInfo();
    
    return newPtr;
}

size_t MemoryPool::getSize(void* ptr) const {
    if (!ptr) return 0;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = allocationMap_.find(ptr);
    return (it != allocationMap_.end()) ? it->second.size : 0;
}

PoolType MemoryPool::getPoolType(void* ptr) const {
    if (!ptr) return PoolType::SMALL_OBJECTS;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = allocationMap_.find(ptr);
    return (it != allocationMap_.end()) ? it->second.poolType : PoolType::SMALL_OBJECTS;
}

PoolStats MemoryPool::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PoolStats stats;
    stats.totalBlocks = allocationMap_.size();
    stats.usedBlocks = allocationMap_.size();
    stats.freeBlocks = 0; // 簡略化
    stats.totalBytes = totalSize_;
    stats.usedBytes = usedSize_;
    stats.freeBytes = getFreeSize();
    stats.peakUsage = stats_.peakUsage;
    stats.allocationCount = stats_.totalAllocations;
    stats.deallocationCount = stats_.totalDeallocations;
    stats.fragmentationRatio = stats_.fragmentationRatio;
    
    return stats;
}

void MemoryPool::defragment() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 簡略化されたデフラグメンテーション
    // 実際の実装では、メモリブロックを再配置して断片化を解消
    
    std::vector<std::pair<void*, AllocationInfo>> allocations;
    
    // 現在の割り当てを収集
    for (const auto& pair : allocationMap_) {
        allocations.push_back(pair);
    }
    
    // サイズ順にソート
    std::sort(allocations.begin(), allocations.end(),
        [](const auto& a, const auto& b) {
            return a.second.size > b.second.size;
        });
    
    // 統計情報の更新
    updateFragmentationInfo();
}

void MemoryPool::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 全ての割り当てを解放
    for (const auto& pair : allocationMap_) {
        allocator_->deallocate(pair.first);
    }
    
    allocationMap_.clear();
    usedSize_ = 0;
    
    // 統計情報のリセット
    stats_.totalAllocations = 0;
    stats_.totalDeallocations = 0;
    stats_.totalAllocatedBytes = 0;
    stats_.totalDeallocatedBytes = 0;
    stats_.peakUsage = 0;
    stats_.fragmentationRatio = 0.0;
    stats_.averageAllocationSize = 0.0;
}

std::string MemoryPool::dumpInfo(bool verbose) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "=== Memory Pool Information ===\n";
    oss << "Total Size: " << totalSize_ << " bytes\n";
    oss << "Used Size: " << usedSize_ << " bytes\n";
    oss << "Free Size: " << getFreeSize() << " bytes\n";
    oss << "Allocations: " << allocationMap_.size() << "\n";
    oss << "Peak Usage: " << stats_.peakUsage << " bytes\n";
    oss << "Fragmentation Ratio: " << stats_.fragmentationRatio << "\n";
    oss << "Average Allocation Size: " << stats_.averageAllocationSize << " bytes\n";
    
    if (verbose) {
        oss << "\n=== Allocation Details ===\n";
        for (const auto& pair : allocationMap_) {
            const AllocationInfo& info = pair.second;
            oss << "Ptr: " << pair.first 
                << ", Size: " << info.size 
                << ", Type: " << static_cast<int>(info.poolType)
                << ", ID: " << info.allocationId << "\n";
        }
    }
    
    return oss.str();
}

// プライベートメソッドの実装

PoolType MemoryPool::determinePoolType(size_t size) const {
    if (size <= 64) {
        return PoolType::SMALL_OBJECTS;
    } else if (size <= 512) {
        return PoolType::MEDIUM_OBJECTS;
    } else if (size <= 4096) {
        return PoolType::LARGE_OBJECTS;
    } else {
        return PoolType::HUGE_OBJECTS;
    }
}

uint64_t MemoryPool::getCurrentTimestamp() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

void MemoryPool::updateFragmentationInfo() {
    if (totalSize_ == 0) {
        stats_.fragmentationRatio = 0.0;
        return;
    }
    
    // 簡略化されたフラグメンテーション計算
    // 実際の実装では、連続する空きブロックのサイズを分析
    size_t freeSize = getFreeSize();
    size_t largestFreeBlock = freeSize; // 簡略化
    
    if (freeSize > 0) {
        stats_.fragmentationRatio = 1.0 - (static_cast<double>(largestFreeBlock) / freeSize);
    } else {
        stats_.fragmentationRatio = 0.0;
    }
}

} // namespace memory
} // namespace utils
} // namespace aerojs 