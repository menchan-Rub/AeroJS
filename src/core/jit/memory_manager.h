#pragma once

#include <cstddef>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace aerojs {
namespace core {

enum class MemoryProtection {
    NoAccess,      // アクセス不可
    ReadOnly,      // 読み取り専用
    ReadWrite,     // 読み取り/書き込み可能
    ReadExecute,   // 読み取り/実行可能
    ReadWriteExecute // 読み取り/書き込み/実行可能（非推奨、W^X違反）
};

/**
 * @brief JITコンパイル用メモリ管理クラス
 * 
 * JITコンパイル用のメモリ領域を管理し、適切なメモリプロテクションを
 * 設定するためのクラスです。セキュリティを考慮したW^X（Write XOR Execute）
 * モデルを実装しています。
 */
class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager();

    /**
     * @brief 実行可能メモリ領域を確保
     * @param size 確保するバイト数
     * @return 確保した領域へのポインタ (失敗時は nullptr)
     */
    void* allocateExecutableMemory(size_t size);

    /**
     * @brief メモリ保護設定の変更
     * @param ptr メモリ領域のポインタ
     * @param size メモリ領域のサイズ
     * @param protection 設定する保護モード
     * @return 成功した場合は true、失敗した場合は false
     */
    bool protectMemory(void* ptr, size_t size, MemoryProtection protection);

    /**
     * @brief メモリ領域の解放
     * @param ptr 解放するメモリ領域のポインタ
     * @return 成功した場合は true、失敗した場合は false
     */
    bool freeMemory(void* ptr);

    /**
     * @brief 命令キャッシュをフラッシュ
     * @param ptr メモリ領域の開始ポインタ
     * @param size メモリ領域のサイズ
     */
    void flushInstructionCache(void* ptr, size_t size);

    /**
     * @brief 割り当てられたメモリの総量を取得
     * @return 割り当て済みメモリのバイト数
     */
    size_t getTotalAllocatedMemory() const;

private:
    struct MemoryRegion {
        void* baseAddress;
        size_t size;
        MemoryProtection currentProtection;
    };

    std::unordered_map<void*, MemoryRegion> m_memoryRegions;
    mutable std::mutex m_mutex;
    size_t m_totalAllocatedMemory;

    /**
     * @brief システムのページサイズを取得
     * @return ページサイズ（バイト）
     */
    static size_t getPageSize();

    /**
     * @brief アドレスをページサイズでアラインメント
     * @param size アラインメントするサイズ
     * @return アラインメントされたサイズ
     */
    static size_t alignToPageSize(size_t size);
};

/**
 * @brief 下位互換性のためのレガシー関数
 * @deprecated MemoryManager クラスを使用してください
 */
void* allocate_executable_memory(const void* code, size_t size) noexcept;

/**
 * @brief 下位互換性のためのレガシー関数
 * @deprecated MemoryManager クラスを使用してください
 */
void free_executable_memory(void* ptr, size_t size) noexcept;

}  // namespace core
}  // namespace aerojs