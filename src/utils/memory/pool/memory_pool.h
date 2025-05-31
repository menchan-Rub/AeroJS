/**
 * @file memory_pool.h
 * @brief AeroJS メモリプール管理
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_UTILS_MEMORY_POOL_MEMORY_POOL_H
#define AEROJS_UTILS_MEMORY_POOL_MEMORY_POOL_H

#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <memory>

namespace aerojs {
namespace utils {
namespace memory {

// 前方宣言
class MemoryAllocator;

/**
 * @brief メモリプールのタイプ
 */
enum class PoolType {
  SMALL_OBJECTS,     // 小さなオブジェクト用 (1-64 bytes)
  MEDIUM_OBJECTS,    // 中程度のオブジェクト用 (65-512 bytes) 
  LARGE_OBJECTS,     // 大きなオブジェクト用 (513-4096 bytes)
  HUGE_OBJECTS,      // 巨大なオブジェクト用 (4097+ bytes)
  STRINGS,           // 文字列専用
  ARRAYS,            // 配列専用
  FUNCTIONS,         // 関数オブジェクト専用
  BYTECODE,          // バイトコード専用
  JIT_CODE,          // JITコンパイル後のコード専用
  TEMP_OBJECTS       // 一時オブジェクト用
};

/**
 * @brief メモリブロック
 */
struct MemoryBlock {
  void* data;
  size_t size;
  size_t alignment;
  bool inUse;
  PoolType poolType;
  uint64_t allocationId;
  uint64_t timestamp;
  MemoryBlock* next;
  MemoryBlock* prev;
};

/**
 * @brief メモリプール統計情報
 */
struct PoolStats {
  size_t totalBlocks;
  size_t usedBlocks;
  size_t freeBlocks;
  size_t totalBytes;
  size_t usedBytes;
  size_t freeBytes;
  size_t peakUsage;
  size_t allocationCount;
  size_t deallocationCount;
  double fragmentationRatio;
};

/**
 * @brief 単一サイズのメモリプール
 */
class FixedSizePool {
 public:
  FixedSizePool(size_t blockSize, size_t poolSize, MemoryAllocator* allocator);
  ~FixedSizePool();

  /**
   * @brief ブロックを割り当てる
   * @return MemoryBlock* 割り当てられたブロック（失敗時はnullptr）
   */
  MemoryBlock* allocate();

  /**
   * @brief ブロックを解放する
   * @param block 解放するブロック
   */
  void deallocate(MemoryBlock* block);

  /**
   * @brief 統計情報を取得
   * @return PoolStats 統計情報
   */
  PoolStats getStats() const;

  /**
   * @brief ブロックサイズを取得
   * @return size_t ブロックサイズ
   */
  size_t getBlockSize() const { return blockSize_; }

  /**
   * @brief プールをリセット
   */
  void reset();

  /**
   * @brief デフラグメンテーション実行
   */
  void defragment();

 private:
  size_t blockSize_;
  size_t poolSize_;
  MemoryAllocator* allocator_;
  void* memory_;
  MemoryBlock* freeList_;
  MemoryBlock* usedList_;
  mutable std::mutex mutex_;
  PoolStats stats_;
  uint64_t nextAllocationId_;

  /**
   * @brief プールを初期化
   */
  void initializePool();

  /**
   * @brief 新しいチャンクを追加
   */
  bool expandPool();
};

/**
 * @brief メモリプールマネージャー
 */
class MemoryPoolManager {
 public:
  explicit MemoryPoolManager(MemoryAllocator* allocator);
  ~MemoryPoolManager();

  /**
   * @brief 指定サイズのメモリを割り当て
   * @param size サイズ
   * @param poolType プールタイプ
   * @param alignment アライメント（デフォルト: 8）
   * @return void* 割り当てられたメモリ（失敗時はnullptr）
   */
  void* allocate(size_t size, PoolType poolType = PoolType::SMALL_OBJECTS, size_t alignment = 8);

  /**
   * @brief メモリを解放
   * @param ptr 解放するメモリ
   */
  void deallocate(void* ptr);

  /**
   * @brief メモリを再割り当て
   * @param ptr 元のメモリ
   * @param newSize 新しいサイズ
   * @param poolType プールタイプ
   * @param alignment アライメント
   * @return void* 再割り当てされたメモリ
   */
  void* reallocate(void* ptr, size_t newSize, PoolType poolType = PoolType::SMALL_OBJECTS, size_t alignment = 8);

  /**
   * @brief メモリサイズを取得
   * @param ptr メモリポインタ
   * @return size_t メモリサイズ
   */
  size_t getSize(void* ptr) const;

  /**
   * @brief プールタイプを取得
   * @param ptr メモリポインタ
   * @return PoolType プールタイプ
   */
  PoolType getPoolType(void* ptr) const;

  /**
   * @brief 統計情報を取得
   * @param poolType 指定プールの統計（省略時は全体）
   * @return PoolStats 統計情報
   */
  PoolStats getStats(PoolType poolType = PoolType::SMALL_OBJECTS) const;

  /**
   * @brief 全プールの統計情報を取得
   * @return std::unordered_map<PoolType, PoolStats> プールごとの統計情報
   */
  std::unordered_map<PoolType, PoolStats> getAllStats() const;

  /**
   * @brief ガベージコレクション準備
   */
  void prepareForGC();

  /**
   * @brief ガベージコレクション完了通知
   */
  void finishGC();

  /**
   * @brief デフラグメンテーション実行
   * @param poolType 対象プール（省略時は全プール）
   */
  void defragment(PoolType poolType = PoolType::SMALL_OBJECTS);

  /**
   * @brief プールをリセット
   * @param poolType 対象プール（省略時は全プール）
   */
  void resetPool(PoolType poolType = PoolType::SMALL_OBJECTS);

  /**
   * @brief メモリリークチェック
   * @return std::vector<MemoryBlock*> リークしているブロックのリスト
   */
  std::vector<MemoryBlock*> checkMemoryLeaks() const;

  /**
   * @brief プール情報をダンプ
   * @param verbose 詳細情報を含むかどうか
   * @return std::string プール情報
   */
  std::string dumpPoolInfo(bool verbose = false) const;

 private:
  MemoryAllocator* allocator_;
  std::unordered_map<PoolType, std::unique_ptr<FixedSizePool>> pools_;
  std::unordered_map<void*, MemoryBlock*> blockMap_;
  mutable std::mutex globalMutex_;

  // プールサイズの設定
  static const std::unordered_map<PoolType, std::pair<size_t, size_t>> POOL_CONFIGS;

  /**
   * @brief プールを初期化
   */
  void initializePools();

  /**
   * @brief 適切なプールを取得
   * @param size サイズ
   * @param poolType プールタイプ
   * @return FixedSizePool* プール（見つからない場合はnullptr）
   */
  FixedSizePool* getPool(size_t size, PoolType poolType);

  /**
   * @brief サイズに基づいてプールタイプを決定
   * @param size サイズ
   * @return PoolType 適切なプールタイプ
   */
  PoolType determinePoolType(size_t size) const;

  /**
   * @brief メモリブロックを作成
   * @param data データポインタ
   * @param size サイズ
   * @param poolType プールタイプ
   * @param alignment アライメント
   * @return MemoryBlock* 作成されたブロック
   */
  MemoryBlock* createBlock(void* data, size_t size, PoolType poolType, size_t alignment);

  /**
   * @brief メモリブロックを削除
   * @param block 削除するブロック
   */
  void destroyBlock(MemoryBlock* block);
};

/**
 * @brief メモリプール管理クラス
 */
class MemoryPool {
public:
    MemoryPool();
    ~MemoryPool();

    // 初期化
    bool initialize(MemoryAllocator* allocator);

    // メモリ割り当て
    void* allocate(size_t size);
    void deallocate(void* ptr);

    // 統計情報
    size_t getTotalSize() const;
    size_t getUsedSize() const;
    size_t getFreeSize() const;

private:
    MemoryAllocator* allocator_;
    size_t totalSize_;
    size_t usedSize_;
};

}  // namespace memory
}  // namespace utils
}  // namespace aerojs

#endif  // AEROJS_UTILS_MEMORY_POOL_MEMORY_POOL_H