/**
 * @file memory_allocator.cpp
 * @brief AeroJS メモリアロケータの実装
 * @version 0.1.0
 * @license MIT
 */

#include "memory_allocator.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <stdexcept>

#ifdef _WIN32
#include <malloc.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

namespace aerojs {
namespace utils {
namespace memory {

// =====================================================
// StandardAllocator Implementation
// =====================================================

StandardAllocator::StandardAllocator() : memoryLimit_(1024 * 1024 * 1024) { // 1GB
}

StandardAllocator::~StandardAllocator() = default;

bool StandardAllocator::initialize() {
    return true;
}

void* StandardAllocator::allocate(size_t size, size_t alignment) {
    if (size == 0) return nullptr;
    
    void* ptr = nullptr;
    
#ifdef _WIN32
    ptr = _aligned_malloc(size, alignment);
#else
    if (posix_memalign(&ptr, alignment, size) != 0) {
        ptr = nullptr;
    }
#endif
    
    if (ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        allocations_[ptr] = {size, alignment, std::chrono::steady_clock::now()};
        stats_.totalAllocations++;
        stats_.currentAllocations++;
        stats_.totalBytes += size;
        stats_.currentBytes += size;
        stats_.totalAllocated += size;
        stats_.currentAllocated += size;
        
        if (stats_.currentBytes > stats_.peakBytes) {
            stats_.peakBytes = stats_.currentBytes;
        }
    } else {
        stats_.failedAllocations++;
    }
    
    return ptr;
}

void StandardAllocator::deallocate(void* ptr) {
    if (!ptr) return;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = allocations_.find(ptr);
        if (it != allocations_.end()) {
            stats_.currentAllocations--;
            stats_.currentBytes -= it->second.size;
            stats_.currentAllocated -= it->second.size;
            allocations_.erase(it);
        }
    }
    
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

void* StandardAllocator::reallocate(void* ptr, size_t newSize, size_t alignment) {
    if (!ptr) {
        return allocate(newSize, alignment);
    }
    
    if (newSize == 0) {
        deallocate(ptr);
        return nullptr;
    }
    
    void* newPtr = allocate(newSize, alignment);
    if (newPtr) {
        size_t oldSize = getSize(ptr);
        std::memcpy(newPtr, ptr, std::min(oldSize, newSize));
        deallocate(ptr);
    }
    
    return newPtr;
}

size_t StandardAllocator::getSize(void* ptr) const {
    if (!ptr) return 0;
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = allocations_.find(ptr);
    if (it != allocations_.end()) {
        return it->second.size;
    }
    return 0;
}

size_t StandardAllocator::getCurrentAllocatedSize() const {
    return stats_.currentAllocated;
}

size_t StandardAllocator::getTotalAllocatedSize() const {
    return stats_.totalAllocated;
}

void StandardAllocator::setMemoryLimit(size_t limit) {
    memoryLimit_ = limit;
}

size_t StandardAllocator::getMemoryLimit() const {
    return memoryLimit_;
}

const AllocatorStats& StandardAllocator::getStats() const {
    return stats_;
}

void StandardAllocator::prepareForGC() {
    // GC準備処理
}

void StandardAllocator::finishGC() {
    // GC完了後の処理
}

std::vector<void*> StandardAllocator::getAllocatedObjects() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<void*> objects;
    objects.reserve(allocations_.size());
    for (const auto& pair : allocations_) {
        objects.push_back(pair.first);
    }
    return objects;
}

void StandardAllocator::startGC() {
    stats_.gcCount++;
}

// =====================================================
// PoolAllocator Implementation  
// =====================================================

PoolAllocator::PoolAllocator(size_t blockSize, size_t poolSize)
    : blockSize_(blockSize), poolSize_(poolSize), pools_(nullptr), 
      freeList_(nullptr), memoryLimit_(SIZE_MAX) {
    
    // 最小ブロックサイズの調整
    if (blockSize_ < sizeof(Block)) {
        blockSize_ = sizeof(Block);
    }
    
    // アライメント調整
    blockSize_ = (blockSize_ + 7) & ~7;
    
    // 初期プールの作成
    createPool();
}

PoolAllocator::~PoolAllocator() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 全プールを解放
    Pool* current = pools_;
    while (current) {
        Pool* next = current->next;
        std::free(current->memory);
        delete current;
        current = next;
    }
}

void* PoolAllocator::allocate(size_t size, size_t alignment) {
    (void)alignment; // 未使用パラメータ警告を回避
    
    if (size > blockSize_) {
        // 大きなブロックは標準アロケータにフォールバック
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // メモリ制限チェック
    if (stats_.currentBytes + blockSize_ > memoryLimit_) {
        stats_.failedAllocations++;
        return nullptr;
    }
    
    // フリーリストから取得
    if (!freeList_) {
        // 新しいプールを作成
        if (!createPool()) {
            stats_.failedAllocations++;
            return nullptr;
        }
    }
    
    Block* block = freeList_;
    freeList_ = block->next;
    block->inUse = true;
    
    // 統計を更新
    stats_.totalAllocations++;
    stats_.currentAllocations++;
    stats_.totalBytes += blockSize_;
    stats_.currentBytes += blockSize_;
    
    if (stats_.currentBytes > stats_.peakBytes) {
        stats_.peakBytes = stats_.currentBytes;
    }
    
    return block;
}

void PoolAllocator::deallocate(void* ptr) {
    if (!ptr) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    Block* block = static_cast<Block*>(ptr);
    block->inUse = false;
    block->next = freeList_;
    freeList_ = block;
    
    // 統計を更新
    stats_.currentAllocations--;
    stats_.currentBytes -= blockSize_;
}

void* PoolAllocator::reallocate(void* ptr, size_t newSize, size_t alignment) {
    (void)alignment; // 未使用パラメータ警告を回避
    (void)ptr; // 未使用パラメータ警告を回避
    (void)newSize; // 未使用パラメータ警告を回避
    
    if (newSize > blockSize_) {
        return nullptr;
    }
    
    if (!ptr) {
        return allocate(newSize, alignment);
    }
    
    // プールアロケータでは固定サイズなので、新しいブロックを割り当ててコピー
    void* newPtr = allocate(newSize, alignment);
    if (!newPtr) {
        return nullptr;
    }
    
    std::memcpy(newPtr, ptr, std::min(newSize, blockSize_));
    deallocate(ptr);
    
    return newPtr;
}

size_t PoolAllocator::getSize(void* ptr) const {
    (void)ptr; // 未使用パラメータ警告を回避
    return ptr ? blockSize_ : 0;
}

size_t PoolAllocator::getCurrentAllocatedSize() const {
    return stats_.currentBytes;
}

size_t PoolAllocator::getTotalAllocatedSize() const {
    return stats_.totalBytes;
}

void PoolAllocator::setMemoryLimit(size_t limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    memoryLimit_ = limit;
}

size_t PoolAllocator::getMemoryLimit() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return memoryLimit_;
}

const AllocatorStats& PoolAllocator::getStats() const {
    return stats_;
}

void PoolAllocator::prepareForGC() {
    // GC準備処理
}

void PoolAllocator::finishGC() {
    // GC完了処理
}

PoolAllocator::Pool* PoolAllocator::createPool() {
    Pool* pool = new Pool;
    pool->size = poolSize_;
    pool->memory = std::malloc(poolSize_);
    pool->next = pools_;
    pools_ = pool;
    
    if (!pool->memory) {
        delete pool;
        return nullptr;
    }
    
    initializePool(pool);
    return pool;
}

void PoolAllocator::initializePool(Pool* pool) {
    char* memory = static_cast<char*>(pool->memory);
    size_t blockCount = poolSize_ / blockSize_;
    
    // ブロックチェーンを構築
    for (size_t i = 0; i < blockCount; ++i) {
        Block* block = reinterpret_cast<Block*>(memory + i * blockSize_);
        block->inUse = false;
        block->next = (i + 1 < blockCount) ? 
            reinterpret_cast<Block*>(memory + (i + 1) * blockSize_) : nullptr;
    }
    
    // フリーリストに追加
    Block* firstBlock = reinterpret_cast<Block*>(memory);
    Block* lastBlock = reinterpret_cast<Block*>(memory + (blockCount - 1) * blockSize_);
    lastBlock->next = freeList_;
    freeList_ = firstBlock;
}

// =====================================================
// StackAllocator Implementation
// =====================================================

StackAllocator::StackAllocator(size_t size) 
    : size_(size), current_(0), memoryLimit_(size) {
    memory_ = std::malloc(size);
    if (!memory_) {
        throw std::bad_alloc();
    }
}

StackAllocator::~StackAllocator() {
    std::free(memory_);
}

void* StackAllocator::allocate(size_t size, size_t alignment) {
    if (size == 0) return nullptr;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // アライメント調整
    size_t alignedCurrent = (current_ + alignment - 1) & ~(alignment - 1);
    
    if (alignedCurrent + size > size_) {
        stats_.failedAllocations++;
        return nullptr;
    }
    
    void* ptr = static_cast<char*>(memory_) + alignedCurrent;
    current_ = alignedCurrent + size;
    
    // 統計を更新
    stats_.totalAllocations++;
    stats_.currentAllocations++;
    stats_.totalBytes += size;
    stats_.currentBytes += size;
    
    if (stats_.currentBytes > stats_.peakBytes) {
        stats_.peakBytes = stats_.currentBytes;
    }
    
    return ptr;
}

void StackAllocator::deallocate(void* ptr) {
    (void)ptr; // 未使用パラメータ警告を回避
    // スタックアロケータでは個別の解放はサポートしない
    // reset()またはresetToMarker()を使用する
}

void* StackAllocator::reallocate(void* ptr, size_t newSize, size_t alignment) {
    (void)ptr; // 未使用パラメータ警告を回避
    (void)newSize; // 未使用パラメータ警告を回避
    (void)alignment; // 未使用パラメータ警告を回避
    // スタックアロケータでは再割り当てはサポートしない
    return nullptr;
}

size_t StackAllocator::getSize(void* ptr) const {
    (void)ptr; // 未使用パラメータ警告を回避
    // スタックアロケータでは個別サイズの追跡は困難
    return 0;
}

size_t StackAllocator::getCurrentAllocatedSize() const {
    return current_;
}

size_t StackAllocator::getTotalAllocatedSize() const {
    return stats_.totalBytes;
}

void StackAllocator::setMemoryLimit(size_t limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    memoryLimit_ = std::min(limit, size_);
}

size_t StackAllocator::getMemoryLimit() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return memoryLimit_;
}

const AllocatorStats& StackAllocator::getStats() const {
    return stats_;
}

void StackAllocator::prepareForGC() {
    // GC準備処理
}

void StackAllocator::finishGC() {
    // GC完了処理
}

void StackAllocator::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    current_ = 0;
    stats_.currentAllocations = 0;
    stats_.currentBytes = 0;
}

size_t StackAllocator::setMarker() {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

void StackAllocator::resetToMarker(size_t marker) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (marker <= current_) {
        size_t freedBytes = current_ - marker;
        current_ = marker;
        stats_.currentBytes -= freedBytes;
        // 解放されたオブジェクトの数は正確には分からないので近似値
        stats_.currentAllocations = stats_.currentAllocations / 2;
    }
}

}  // namespace memory
}  // namespace utils
}  // namespace aerojs 