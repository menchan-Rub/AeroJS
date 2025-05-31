/**
 * @file memory_allocator.h
 * @brief AeroJS メモリアロケータインターフェースと実装
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_UTILS_MEMORY_ALLOCATORS_MEMORY_ALLOCATOR_H
#define AEROJS_UTILS_MEMORY_ALLOCATORS_MEMORY_ALLOCATOR_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <vector>
#include <chrono>

namespace aerojs {
namespace utils {
namespace memory {

/**
 * @brief メモリ統計情報
 */
struct AllocatorStats {
    size_t totalAllocations{0};      // 総割り当て回数
    size_t currentAllocations{0};    // 現在の割り当て数
    size_t totalBytes{0};            // 総割り当てバイト数
    size_t currentBytes{0};          // 現在の割り当てバイト数
    size_t peakBytes{0};             // ピーク時の割り当てバイト数
    size_t failedAllocations{0};     // 失敗した割り当て回数
    size_t totalAllocated{0};        // 総割り当てサイズ
    size_t currentAllocated{0};      // 現在の割り当てサイズ
    size_t gcCount{0};               // GC実行回数
};

/**
 * @brief メモリアロケータの基底クラス
 */
class MemoryAllocator {
public:
    virtual ~MemoryAllocator() = default;

    /**
     * @brief メモリを割り当てる
     * @param size 割り当てサイズ（バイト）
     * @param alignment メモリアライメント（デフォルト: 8）
     * @return void* 割り当てられたメモリへのポインタ（失敗時はnullptr）
     */
    virtual void* allocate(size_t size, size_t alignment = 8) = 0;

    /**
     * @brief メモリを解放する
     * @param ptr 解放するメモリのポインタ
     */
    virtual void deallocate(void* ptr) = 0;

    /**
     * @brief メモリを再割り当てする
     * @param ptr 再割り当てするメモリのポインタ
     * @param newSize 新しいサイズ（バイト）
     * @param alignment メモリアライメント（デフォルト: 8）
     * @return void* 再割り当てされたメモリへのポインタ
     */
    virtual void* reallocate(void* ptr, size_t newSize, size_t alignment = 8) = 0;

    /**
     * @brief 割り当てられたメモリのサイズを取得
     * @param ptr メモリのポインタ
     * @return size_t メモリサイズ（バイト）
     */
    virtual size_t getSize(void* ptr) const = 0;

    /**
     * @brief 現在割り当てられているメモリサイズを取得
     * @return size_t 現在のメモリサイズ（バイト）
     */
    virtual size_t getCurrentAllocatedSize() const = 0;

    /**
     * @brief 総割り当てメモリサイズを取得
     * @return size_t 総メモリサイズ（バイト）
     */
    virtual size_t getTotalAllocatedSize() const = 0;

    /**
     * @brief メモリ制限を設定
     * @param limit メモリ制限（バイト）
     */
    virtual void setMemoryLimit(size_t limit) = 0;

    /**
     * @brief メモリ制限を取得
     * @return size_t メモリ制限（バイト）
     */
    virtual size_t getMemoryLimit() const = 0;

    /**
     * @brief 統計情報を取得
     * @return const AllocatorStats& 統計情報への参照
     */
    virtual const AllocatorStats& getStats() const = 0;

    /**
     * @brief ガベージコレクション準備
     */
    virtual void prepareForGC() = 0;

    /**
     * @brief ガベージコレクション完了通知
     */
    virtual void finishGC() = 0;

    /**
     * @brief 割り当て済みオブジェクトのリストを取得（GC用）
     * @return std::vector<void*> 割り当て済みオブジェクトのリスト
     */
    virtual std::vector<void*> getAllocatedObjects() const = 0;

    /**
     * @brief ガベージコレクション開始
     */
    virtual void startGC() = 0;

    /**
     * @brief 初期化
     * @return bool 成功時はtrue
     */
    virtual bool initialize() = 0;
};

/**
 * @brief 標準的なメモリアロケータ実装
 */
class StandardAllocator : public MemoryAllocator {
public:
    StandardAllocator();
    ~StandardAllocator() override;

    void* allocate(size_t size, size_t alignment = 8) override;
    void deallocate(void* ptr) override;
    void* reallocate(void* ptr, size_t newSize, size_t alignment = 8) override;
    size_t getSize(void* ptr) const override;
    size_t getCurrentAllocatedSize() const override;
    size_t getTotalAllocatedSize() const override;
    void setMemoryLimit(size_t limit) override;
    size_t getMemoryLimit() const override;
    const AllocatorStats& getStats() const override;
    void prepareForGC() override;
    void finishGC() override;
    std::vector<void*> getAllocatedObjects() const override;
    void startGC() override;
    bool initialize() override;

private:
    struct AllocationInfo {
        size_t size;
        size_t alignment;
        std::chrono::steady_clock::time_point timestamp;
    };

    mutable std::mutex mutex_;
    std::unordered_map<void*, AllocationInfo> allocations_;
    AllocatorStats stats_;
    size_t memoryLimit_;
};

/**
 * @brief プールアロケータ実装
 */
class PoolAllocator : public MemoryAllocator {
 public:
  explicit PoolAllocator(size_t blockSize, size_t poolSize = 1024 * 1024);
  ~PoolAllocator() override;

  void* allocate(size_t size, size_t alignment = 8) override;
  void deallocate(void* ptr) override;
  void* reallocate(void* ptr, size_t newSize, size_t alignment = 8) override;
  size_t getSize(void* ptr) const override;
  size_t getCurrentAllocatedSize() const override;
  size_t getTotalAllocatedSize() const override;
  void setMemoryLimit(size_t limit) override;
  size_t getMemoryLimit() const override;
  const AllocatorStats& getStats() const override;
  void prepareForGC() override;
  void finishGC() override;

 private:
  struct Block {
    Block* next;
    bool inUse;
  };

  struct Pool {
    void* memory;
    size_t size;
    Block* freeList;
    Pool* next;
  };

  size_t blockSize_;
  size_t poolSize_;
  Pool* pools_;
  Block* freeList_;
  mutable std::mutex mutex_;
  AllocatorStats stats_;
  size_t memoryLimit_;

  /**
   * @brief 新しいプールを作成
   * @return Pool* 作成されたプール
   */
  Pool* createPool();

  /**
   * @brief プールを初期化
   * @param pool 初期化するプール
   */
  void initializePool(Pool* pool);
};

/**
 * @brief スタックアロケータ実装
 */
class StackAllocator : public MemoryAllocator {
 public:
  explicit StackAllocator(size_t size);
  ~StackAllocator() override;

  void* allocate(size_t size, size_t alignment = 8) override;
  void deallocate(void* ptr) override;
  void* reallocate(void* ptr, size_t newSize, size_t alignment = 8) override;
  size_t getSize(void* ptr) const override;
  size_t getCurrentAllocatedSize() const override;
  size_t getTotalAllocatedSize() const override;
  void setMemoryLimit(size_t limit) override;
  size_t getMemoryLimit() const override;
  const AllocatorStats& getStats() const override;
  void prepareForGC() override;
  void finishGC() override;

  /**
   * @brief スタックの先頭に戻す
   */
  void reset();

  /**
   * @brief マーカーを設定
   * @return size_t マーカー位置
   */
  size_t setMarker();

  /**
   * @brief マーカー位置に戻す
   * @param marker マーカー位置
   */
  void resetToMarker(size_t marker);

 private:
  void* memory_;
  size_t size_;
  size_t current_;
  mutable std::mutex mutex_;
  AllocatorStats stats_;
  size_t memoryLimit_;
};

}  // namespace memory
}  // namespace utils
}  // namespace aerojs

#endif  // AEROJS_UTILS_MEMORY_ALLOCATORS_MEMORY_ALLOCATOR_H