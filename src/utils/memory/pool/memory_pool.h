/**
 * @file memory_pool.h
 * @brief 固定サイズメモリブロック用のメモリプール
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_MEMORY_POOL_H
#define AEROJS_MEMORY_POOL_H

#include "../allocators/memory_allocator.h"
#include <cassert>
#include <cstdint>
#include <mutex>
#include <vector>

namespace aerojs {
namespace utils {

/**
 * @brief 固定サイズのメモリブロックを管理するメモリプール
 * 
 * 指定されたサイズのメモリブロックを効率的に確保・解放するためのメモリプールです。
 * 小さなオブジェクトの頻繁な確保と解放に最適化されています。
 */
class MemoryPool : public MemoryAllocator {
public:
    /**
     * @brief コンストラクタ
     * @param blockSize メモリブロックのサイズ (バイト単位)
     * @param alignment メモリブロックのアライメント (バイト単位)
     * @param blocksPerChunk 一度に確保するブロック数
     * @param baseAllocator 下層のアロケータ
     */
    MemoryPool(
        std::size_t blockSize,
        std::size_t alignment = alignof(std::max_align_t),
        std::size_t blocksPerChunk = 1024,
        MemoryAllocator* baseAllocator = nullptr
    )
        : m_blockSize(std::max(blockSize, sizeof(void*)))
        , m_alignment(alignment)
        , m_blocksPerChunk(blocksPerChunk)
        , m_baseAllocator(baseAllocator ? baseAllocator : &m_defaultAllocator)
        , m_freeList(nullptr)
        , m_stats{0, 0, 0, 0, 0, 0, 0}
    {
        // ブロックサイズをアライメントに合わせる
        m_blockSize = (m_blockSize + m_alignment - 1) & ~(m_alignment - 1);
    }
    
    /**
     * @brief デストラクタ
     */
    virtual ~MemoryPool() {
        // すべてのチャンクを解放
        for (auto& chunk : m_chunks) {
            m_baseAllocator->deallocate(chunk.memory, chunk.size);
        }
        m_chunks.clear();
        m_freeList = nullptr;
    }
    
    /**
     * @brief メモリを確保する
     * @param size 確保するサイズ (バイト単位)
     * @param alignment アライメント (バイト単位)
     * @param flags メモリ領域のフラグ
     * @return 確保されたメモリへのポインタ、失敗した場合はnullptr
     */
    virtual void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t), MemoryRegionFlags flags = MemoryRegionFlags::DefaultData) override {
        // 要求サイズが大きすぎる場合は基底アロケータにフォールバック
        if (size > m_blockSize || alignment > m_alignment) {
            return m_baseAllocator->allocate(size, alignment, flags);
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // フリーリストが空の場合は新しいチャンクを確保
        if (m_freeList == nullptr) {
            if (!allocateChunk()) {
                m_stats.failedAllocations++;
                return nullptr;
            }
        }
        
        assert(m_freeList != nullptr);
        
        // フリーリストから最初のブロックを取得
        void* block = m_freeList;
        m_freeList = *reinterpret_cast<void**>(block);
        
        // 統計情報を更新
        m_stats.totalAllocated += m_blockSize;
        m_stats.currentAllocated += m_blockSize;
        m_stats.maxAllocated = std::max(m_stats.maxAllocated, m_stats.currentAllocated);
        m_stats.totalAllocations++;
        m_stats.activeAllocations++;
        
        return block;
    }
    
    /**
     * @brief メモリを解放する
     * @param ptr 解放するメモリへのポインタ
     * @param size 確保時のサイズ (バイト単位)
     * @param alignment 確保時のアライメント (バイト単位)
     * @return 成功した場合はtrue、それ以外はfalse
     */
    virtual bool deallocate(void* ptr, std::size_t size = 0, std::size_t alignment = alignof(std::max_align_t)) override {
        if (ptr == nullptr) {
            return true;
        }
        
        // サイズやアライメントが異なる場合は基底アロケータに委譲
        if ((size > 0 && size > m_blockSize) || (alignment > m_alignment)) {
            return m_baseAllocator->deallocate(ptr, size, alignment);
        }
        
        // このプールで確保したメモリかどうかを確認
        if (!ownsMemory(ptr)) {
            return m_baseAllocator->deallocate(ptr, size, alignment);
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // フリーリストに追加
        *reinterpret_cast<void**>(ptr) = m_freeList;
        m_freeList = ptr;
        
        // 統計情報を更新
        m_stats.currentAllocated -= m_blockSize;
        m_stats.totalDeallocations++;
        m_stats.activeAllocations--;
        
        return true;
    }
    
    /**
     * @brief 統計情報を取得する
     * @return 統計情報
     */
    virtual Stats getStats() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stats;
    }
    
    /**
     * @brief 統計情報をリセットする
     */
    virtual void resetStats() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats = {0, 0, 0, 0, 0, 0, 0};
    }
    
    /**
     * @brief アロケータ名を取得する
     * @return アロケータ名
     */
    virtual const char* getName() const override {
        return "MemoryPool";
    }
    
    /**
     * @brief プールで管理しているメモリかどうかを確認
     * @param ptr 確認するメモリへのポインタ
     * @return プールで管理しているメモリの場合はtrue、それ以外はfalse
     */
    bool ownsMemory(void* ptr) const {
        // 各チャンクに対してポインタが含まれるかを確認
        for (const auto& chunk : m_chunks) {
            const char* start = static_cast<const char*>(chunk.memory);
            const char* end = start + chunk.size;
            if (ptr >= start && ptr < end) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief ブロックサイズを取得
     * @return ブロックサイズ (バイト単位)
     */
    std::size_t getBlockSize() const {
        return m_blockSize;
    }
    
    /**
     * @brief アライメントを取得
     * @return アライメント (バイト単位)
     */
    std::size_t getAlignment() const {
        return m_alignment;
    }
    
    /**
     * @brief チャンクあたりのブロック数を取得
     * @return チャンクあたりのブロック数
     */
    std::size_t getBlocksPerChunk() const {
        return m_blocksPerChunk;
    }
    
private:
    struct Chunk {
        void* memory;    // チャンクのメモリ
        std::size_t size; // チャンクのサイズ
    };
    
    std::size_t m_blockSize;      // メモリブロックのサイズ
    std::size_t m_alignment;      // メモリブロックのアライメント
    std::size_t m_blocksPerChunk; // チャンクあたりのブロック数
    MemoryAllocator* m_baseAllocator; // 下層のアロケータ
    StandardAllocator m_defaultAllocator; // デフォルトの基底アロケータ
    void* m_freeList;            // 空きブロックのリスト
    std::vector<Chunk> m_chunks;  // 確保したチャンクのリスト
    mutable std::mutex m_mutex;   // スレッド同期用のミューテックス
    Stats m_stats;                // 統計情報
    
    /**
     * @brief 新しいチャンクを確保する
     * @return 成功した場合はtrue、失敗した場合はfalse
     */
    bool allocateChunk() {
        // チャンクのサイズを計算
        std::size_t chunkSize = m_blockSize * m_blocksPerChunk;
        
        // メモリを確保
        void* memory = m_baseAllocator->allocate(chunkSize, m_alignment);
        if (memory == nullptr) {
            return false;
        }
        
        // チャンク情報を保存
        m_chunks.push_back({memory, chunkSize});
        
        // ブロックをフリーリストに追加
        char* blockPtr = static_cast<char*>(memory);
        for (std::size_t i = 0; i < m_blocksPerChunk - 1; ++i) {
            *reinterpret_cast<void**>(blockPtr) = blockPtr + m_blockSize;
            blockPtr += m_blockSize;
        }
        
        // 最後のブロックのnextポインタはnullptr
        *reinterpret_cast<void**>(blockPtr) = nullptr;
        
        // フリーリストを新しいチャンクの先頭に設定
        m_freeList = memory;
        
        return true;
    }
};

/**
 * @brief 複数サイズのメモリプールを管理するクラス
 * 
 * 複数の異なるサイズのメモリプールを管理し、適切なサイズのプールを選択します。
 */
class MemoryPoolManager : public MemoryAllocator {
public:
    /**
     * @brief コンストラクタ
     * @param baseAllocator 下層のアロケータ
     */
    explicit MemoryPoolManager(MemoryAllocator* baseAllocator = nullptr)
        : m_baseAllocator(baseAllocator ? baseAllocator : &m_defaultAllocator)
        , m_stats{0, 0, 0, 0, 0, 0, 0}
    {
        // デフォルトで一般的なサイズのプールを初期化
        initializeDefaultPools();
    }
    
    /**
     * @brief デストラクタ
     */
    virtual ~MemoryPoolManager() = default;
    
    /**
     * @brief メモリを確保する
     * @param size 確保するサイズ (バイト単位)
     * @param alignment アライメント (バイト単位)
     * @param flags メモリ領域のフラグ
     * @return 確保されたメモリへのポインタ、失敗した場合はnullptr
     */
    virtual void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t), MemoryRegionFlags flags = MemoryRegionFlags::DefaultData) override {
        if (size == 0) {
            return nullptr;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 適切なプールを探す
        for (auto& pool : m_pools) {
            if (size <= pool->getBlockSize() && alignment <= pool->getAlignment()) {
                void* ptr = pool->allocate(size, alignment, flags);
                if (ptr) {
                    // 統計情報を更新
                    m_stats.totalAllocated += size;
                    m_stats.currentAllocated += size;
                    m_stats.maxAllocated = std::max(m_stats.maxAllocated, m_stats.currentAllocated);
                    m_stats.totalAllocations++;
                    m_stats.activeAllocations++;
                    return ptr;
                }
            }
        }
        
        // 適切なプールが見つからない場合は基底アロケータを使用
        void* ptr = m_baseAllocator->allocate(size, alignment, flags);
        if (ptr) {
            // 統計情報を更新
            m_stats.totalAllocated += size;
            m_stats.currentAllocated += size;
            m_stats.maxAllocated = std::max(m_stats.maxAllocated, m_stats.currentAllocated);
            m_stats.totalAllocations++;
            m_stats.activeAllocations++;
        } else {
            m_stats.failedAllocations++;
        }
        
        return ptr;
    }
    
    /**
     * @brief メモリを解放する
     * @param ptr 解放するメモリへのポインタ
     * @param size 確保時のサイズ (バイト単位)
     * @param alignment 確保時のアライメント (バイト単位)
     * @return 成功した場合はtrue、それ以外はfalse
     */
    virtual bool deallocate(void* ptr, std::size_t size = 0, std::size_t alignment = alignof(std::max_align_t)) override {
        if (ptr == nullptr) {
            return true;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // メモリを所有しているプールを探す
        for (auto& pool : m_pools) {
            if (pool->ownsMemory(ptr)) {
                bool result = pool->deallocate(ptr, size, alignment);
                if (result && size > 0) {
                    // 統計情報を更新
                    m_stats.currentAllocated -= size;
                    m_stats.totalDeallocations++;
                    m_stats.activeAllocations--;
                }
                return result;
            }
        }
        
        // どのプールも所有していない場合は基底アロケータに委譲
        bool result = m_baseAllocator->deallocate(ptr, size, alignment);
        if (result && size > 0) {
            // 統計情報を更新
            m_stats.currentAllocated -= size;
            m_stats.totalDeallocations++;
            m_stats.activeAllocations--;
        }
        
        return result;
    }
    
    /**
     * @brief 新しいプールを追加する
     * @param blockSize ブロックサイズ (バイト単位)
     * @param alignment アライメント (バイト単位)
     * @param blocksPerChunk チャンクあたりのブロック数
     * @return 追加されたプールへのポインタ
     */
    MemoryPool* addPool(std::size_t blockSize, std::size_t alignment = alignof(std::max_align_t), std::size_t blocksPerChunk = 1024) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 同じサイズのプールがすでに存在するか確認
        for (auto& pool : m_pools) {
            if (pool->getBlockSize() == blockSize && pool->getAlignment() == alignment) {
                return pool.get();
            }
        }
        
        // 新しいプールを作成して追加
        auto pool = std::make_unique<MemoryPool>(blockSize, alignment, blocksPerChunk, m_baseAllocator);
        MemoryPool* result = pool.get();
        m_pools.push_back(std::move(pool));
        
        // プールをブロックサイズの昇順でソート
        std::sort(m_pools.begin(), m_pools.end(), 
                 [](const std::unique_ptr<MemoryPool>& a, const std::unique_ptr<MemoryPool>& b) {
                     return a->getBlockSize() < b->getBlockSize();
                 });
        
        return result;
    }
    
    /**
     * @brief 統計情報を取得する
     * @return 統計情報
     */
    virtual Stats getStats() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stats;
    }
    
    /**
     * @brief 統計情報をリセットする
     */
    virtual void resetStats() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats = {0, 0, 0, 0, 0, 0, 0};
        for (auto& pool : m_pools) {
            pool->resetStats();
        }
    }
    
    /**
     * @brief アロケータ名を取得する
     * @return アロケータ名
     */
    virtual const char* getName() const override {
        return "MemoryPoolManager";
    }
    
private:
    MemoryAllocator* m_baseAllocator;             // 下層のアロケータ
    StandardAllocator m_defaultAllocator;         // デフォルトの基底アロケータ
    std::vector<std::unique_ptr<MemoryPool>> m_pools; // プールのリスト
    mutable std::mutex m_mutex;                   // スレッド同期用のミューテックス
    Stats m_stats;                                // 統計情報
    
    /**
     * @brief デフォルトのプールを初期化する
     */
    void initializeDefaultPools() {
        // 小さいサイズから大きいサイズまで、一般的なオブジェクトサイズをカバーするプールを作成
        // 16バイトから始めて8KBまで
        addPool(16);      // 16バイト
        addPool(32);      // 32バイト
        addPool(64);      // 64バイト
        addPool(128);     // 128バイト
        addPool(256);     // 256バイト
        addPool(512);     // 512バイト
        addPool(1024);    // 1KB
        addPool(2048);    // 2KB
        addPool(4096);    // 4KB
        addPool(8192);    // 8KB
    }
};

} // namespace utils
} // namespace aerojs

#endif // AEROJS_MEMORY_POOL_H 