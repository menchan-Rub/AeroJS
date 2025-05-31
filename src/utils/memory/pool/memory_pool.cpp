/**
 * @file memory_pool.cpp
 * @brief AeroJS メモリプール実装
 * @version 0.1.0
 * @license MIT
 */

#include "memory_pool.h"
#include "../allocators/memory_allocator.h"

namespace aerojs {
namespace utils {
namespace memory {

MemoryPool::MemoryPool() : allocator_(nullptr), totalSize_(0), usedSize_(0) {
}

MemoryPool::~MemoryPool() = default;

bool MemoryPool::initialize(MemoryAllocator* allocator) {
    if (!allocator) {
        return false;
    }
    
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
    
    void* ptr = allocator_->allocate(size);
    if (ptr) {
        usedSize_ += size;
    }
    return ptr;
}

void MemoryPool::deallocate(void* ptr) {
    if (!allocator_ || !ptr) return;
    
    // 簡易実装：実際のサイズ追跡は省略
    allocator_->deallocate(ptr);
}

size_t MemoryPool::getTotalSize() const {
    return totalSize_;
}

size_t MemoryPool::getUsedSize() const {
    return usedSize_;
}

size_t MemoryPool::getFreeSize() const {
    return totalSize_ > usedSize_ ? totalSize_ - usedSize_ : 0;
}

} // namespace memory
} // namespace utils
} // namespace aerojs 