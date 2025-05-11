#ifndef AEROJS_CORE_JIT_BACKEND_ARM64_CODE_GENERATOR_H
#define AEROJS_CORE_JIT_BACKEND_ARM64_CODE_GENERATOR_H

#include <cstdint>
#include <vector>

#include "../../ir/ir_function.h"
#include "../../ir/ir_instruction.h"

namespace aerojs {
namespace core {

/**
 * @brief ARM64アーキテクチャ向けのコード生成クラス
 * 
 * IRからARM64ネイティブコードへの変換を担当します。
 */
class ARM64CodeGenerator {
public:
    /**
     * @brief コンストラクタ
     */
    ARM64CodeGenerator() = default;
    
    /**
     * @brief デストラクタ
     */
    ~ARM64CodeGenerator() = default;
    
    /**
     * @brief IR関数からネイティブコードを生成
     * @param ir 入力IR関数
     * @param outCode 出力バイナリコード
     */
    void Generate(const IRFunction& ir, std::vector<uint8_t>& outCode) noexcept;

private:
    /**
     * @brief プロローグコードを生成
     * @param out 出力バイナリコード
     */
    void EmitPrologue(std::vector<uint8_t>& out) noexcept;
    
    /**
     * @brief エピローグコードを生成
     * @param out 出力バイナリコード
     */
    void EmitEpilogue(std::vector<uint8_t>& out) noexcept;
    
    /**
     * @brief IR命令に対応するネイティブコードを生成
     * @param inst IR命令
     * @param out 出力バイナリコード
     */
    void EmitInstruction(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept;
    
    /**
     * @brief 汎用レジスタに即値をロード
     * @param reg レジスタ番号
     * @param value 即値
     * @param out 出力バイナリコード
     */
    void EmitLoadImmediate(int reg, int64_t value, std::vector<uint8_t>& out) noexcept;
    
    /**
     * @brief メモリからレジスタにロード
     * @param reg レジスタ番号
     * @param base ベースレジスタ
     * @param offset オフセット
     * @param out 出力バイナリコード
     */
    void EmitLoadMemory(int reg, int base, int offset, std::vector<uint8_t>& out) noexcept;
    
    /**
     * @brief レジスタからメモリへストア
     * @param reg レジスタ番号
     * @param base ベースレジスタ
     * @param offset オフセット
     * @param out 出力バイナリコード
     */
    void EmitStoreMemory(int reg, int base, int offset, std::vector<uint8_t>& out) noexcept;
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_BACKEND_ARM64_CODE_GENERATOR_H