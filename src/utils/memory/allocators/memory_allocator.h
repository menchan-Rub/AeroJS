/**
 * @file memory_allocator.h
 * @brief メモリアロケータの基本インターフェース
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_MEMORY_ALLOCATOR_H
#define AEROJS_MEMORY_ALLOCATOR_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace aerojs {
namespace utils {

/**
 * @brief メモリ領域の属性フラグ
 */
enum class MemoryRegionFlags : uint32_t {
    None = 0,
    // メモリの用途
    Code = 1 << 0,        // 実行可能コード用
    Data = 1 << 1,        // データ用
    GCHeap = 1 << 2,      // GCヒープ用
    Stack = 1 << 3,       // スタック用
    
    // アクセス権限
    Read = 1 << 4,        // 読み取り可能
    Write = 1 << 5,       // 書き込み可能
    Execute = 1 << 6,     // 実行可能
    
    // GC関連フラグ
    GCManaged = 1 << 7,   // GC管理下
    GCRoot = 1 << 8,      // GCルート
    
    // その他フラグ
    Shared = 1 << 9,      // 共有メモリ
    Mapped = 1 << 10,     // メモリマップド
    Huge = 1 << 11,       // 巨大ページ
    
    // 一般的な組み合わせ
    ReadWrite = Read | Write,
    ReadExecute = Read | Execute,
    ReadWriteExecute = Read | Write | Execute,
    DefaultData = Data | ReadWrite,
    DefaultCode = Code | ReadExecute,
    DefaultHeap = GCHeap | ReadWrite | GCManaged
};

// ビット演算子のオーバーロード
inline MemoryRegionFlags operator|(MemoryRegionFlags a, MemoryRegionFlags b) {
    return static_cast<MemoryRegionFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

inline MemoryRegionFlags operator&(MemoryRegionFlags a, MemoryRegionFlags b) {
    return static_cast<MemoryRegionFlags>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
    );
}

inline MemoryRegionFlags& operator|=(MemoryRegionFlags& a, MemoryRegionFlags b) {
    a = a | b;
    return a;
}

inline MemoryRegionFlags& operator&=(MemoryRegionFlags& a, MemoryRegionFlags b) {
    a = a & b;
    return a;
}

/**
 * @brief メモリアロケータの基本インターフェース
 * 
 * すべてのカスタムアロケータの基底クラスです。
 */
class MemoryAllocator {
public:
    virtual ~MemoryAllocator() = default;
    
    /**
     * @brief メモリを確保する
     * @param size 確保するサイズ (バイト単位)
     * @param alignment アライメント (バイト単位)
     * @param flags メモリ領域のフラグ
     * @return 確保されたメモリへのポインタ、失敗した場合はnullptr
     */
    virtual void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t), MemoryRegionFlags flags = MemoryRegionFlags::DefaultData) = 0;
    
    /**
     * @brief メモリを解放する
     * @param ptr 解放するメモリへのポインタ
     * @param size 確保時のサイズ (バイト単位)
     * @param alignment 確保時のアライメント (バイト単位)
     * @return 成功した場合はtrue、それ以外はfalse
     */
    virtual bool deallocate(void* ptr, std::size_t size = 0, std::size_t alignment = alignof(std::max_align_t)) = 0;
    
    /**
     * @brief メモリ領域のサイズを変更する
     * @param ptr 現在のメモリへのポインタ
     * @param oldSize 現在のサイズ (バイト単位)
     * @param newSize 新しいサイズ (バイト単位)
     * @param alignment アライメント (バイト単位)
     * @return 新しいメモリへのポインタ、失敗した場合はnullptr
     */
    virtual void* reallocate(void* ptr, std::size_t oldSize, std::size_t newSize, std::size_t alignment = alignof(std::max_align_t)) {
        // デフォルト実装: 新しいメモリを確保してコピー
        if (newSize == 0) {
            deallocate(ptr, oldSize, alignment);
            return nullptr;
        }
        
        if (ptr == nullptr) {
            return allocate(newSize, alignment);
        }
        
        void* newPtr = allocate(newSize, alignment);
        if (newPtr == nullptr) {
            return nullptr;
        }
        
        std::memcpy(newPtr, ptr, (oldSize < newSize) ? oldSize : newSize);
        deallocate(ptr, oldSize, alignment);
        
        return newPtr;
    }
    
    /**
     * @brief メモリ領域のアクセス権限を変更する
     * @param ptr メモリ領域へのポインタ
     * @param size サイズ (バイト単位)
     * @param flags 新しいメモリ領域のフラグ
     * @return 成功した場合はtrue、失敗した場合はfalse
     */
    virtual bool setMemoryProtection(void* ptr, std::size_t size, MemoryRegionFlags flags) {
        // 基本実装はなにもしない（サブクラスで必要に応じて実装）
        return false;
    }
    
    /**
     * @brief アロケータ統計情報
     */
    struct Stats {
        std::size_t totalAllocated;      // 合計確保サイズ
        std::size_t currentAllocated;    // 現在確保サイズ
        std::size_t maxAllocated;        // 最大確保サイズ
        std::size_t totalAllocations;    // 確保回数
        std::size_t activeAllocations;   // 現在のアクティブな確保数
        std::size_t totalDeallocations;  // 解放回数
        std::size_t failedAllocations;   // 失敗した確保回数
    };
    
    /**
     * @brief 統計情報を取得する
     * @return 統計情報
     */
    virtual Stats getStats() const = 0;
    
    /**
     * @brief 統計情報をリセットする
     */
    virtual void resetStats() = 0;
    
    /**
     * @brief アロケータ名を取得する
     * @return アロケータ名
     */
    virtual const char* getName() const = 0;
};

/**
 * @brief 標準アロケータの実装
 * 
 * new/deleteを使用した基本的なアロケータです。
 */
class StandardAllocator : public MemoryAllocator {
public:
    StandardAllocator() : m_stats{0, 0, 0, 0, 0, 0, 0} {}
    
    virtual ~StandardAllocator() = default;
    
    virtual void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t), MemoryRegionFlags flags = MemoryRegionFlags::DefaultData) override {
        if (size == 0) return nullptr;
        
        void* ptr = nullptr;
        
        if (alignment <= alignof(std::max_align_t)) {
            // 標準的なアライメントの場合はnew[]を使用
            ptr = ::operator new(size, std::nothrow);
        } else {
            // カスタムアライメントの場合はaligned_allocを使用
            ptr = aligned_alloc(alignment, size);
        }
        
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
    
    virtual bool deallocate(void* ptr, std::size_t size = 0, std::size_t alignment = alignof(std::max_align_t)) override {
        if (ptr == nullptr) return true;
        
        if (alignment <= alignof(std::max_align_t)) {
            // 標準的なアライメントの場合はdeleteを使用
            ::operator delete(ptr);
        } else {
            // カスタムアライメントの場合はfreeを使用
            free(ptr);
        }
        
        // 統計情報を更新
        if (size > 0) {
            m_stats.currentAllocated -= size;
        }
        m_stats.totalDeallocations++;
        m_stats.activeAllocations--;
        
        return true;
    }
    
    virtual Stats getStats() const override {
        return m_stats;
    }
    
    virtual void resetStats() override {
        m_stats = {0, 0, 0, 0, 0, 0, 0};
    }
    
    virtual const char* getName() const override {
        return "StandardAllocator";
    }
    
private:
    Stats m_stats;
    
    // aligned_allocのクロスプラットフォーム実装
    void* aligned_alloc(std::size_t alignment, std::size_t size) {
#if defined(_MSC_VER)
        return _aligned_malloc(size, alignment);
#elif defined(__APPLE__)
        void* ptr = nullptr;
        if (posix_memalign(&ptr, alignment, size) != 0) {
            return nullptr;
        }
        return ptr;
#else
        // C11で標準化されたaligned_allocを使用
        // サイズはアライメントの倍数である必要がある
        size_t alignedSize = (size + alignment - 1) & ~(alignment - 1);
        return ::aligned_alloc(alignment, alignedSize);
#endif
    }
};

/**
 * @brief オブジェクト生成・破棄のためのユーティリティ関数
 */
template <typename T, typename Allocator, typename... Args>
T* createObject(Allocator& allocator, Args&&... args) {
    void* memory = allocator.allocate(sizeof(T), alignof(T));
    if (!memory) {
        return nullptr;
    }
    
    try {
        return new(memory) T(std::forward<Args>(args)...);
    } catch (...) {
        allocator.deallocate(memory, sizeof(T), alignof(T));
        throw;
    }
}

template <typename T, typename Allocator>
void destroyObject(Allocator& allocator, T* object) {
    if (object) {
        object->~T();
        allocator.deallocate(object, sizeof(T), alignof(T));
    }
}

} // namespace utils
} // namespace aerojs

#endif // AEROJS_MEMORY_ALLOCATOR_H 