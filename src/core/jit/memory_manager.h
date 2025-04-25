#pragma once

#include <cstddef>

namespace aerojs {
namespace core {

/**
 * @brief コード生成後のバイト列を実行可能メモリに配置し、実行権限を設定
 * @param code コピー元のコードバイト配列
 * @param size コピーするバイト数
 * @return 実行可能領域へのポインタ (失敗時は nullptr)
 */
void* allocate_executable_memory(const void* code, size_t size) noexcept;

/**
 * @brief 実行可能メモリ領域を解放する
 * @param ptr 解放する領域のポインタ
 * @param size 領域のバイト数 (allocate_executable_memory と同じ値)
 */
void free_executable_memory(void* ptr, size_t size) noexcept;

}  // namespace core
}  // namespace aerojs