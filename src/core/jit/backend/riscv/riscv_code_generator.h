#ifndef AEROJS_CORE_JIT_BACKEND_RISCV_CODE_GENERATOR_H
#define AEROJS_CORE_JIT_BACKEND_RISCV_CODE_GENERATOR_H

#include <cstdint>
#include <vector>

#include "../../ir/ir_function.h"
#include "../../ir/ir_instruction.h"

namespace aerojs {
namespace core {

/**
 * @brief RISC-Vアーキテクチャ向けのコード生成クラス
 * 
 * IRからRISC-Vネイティブコードへの変換を担当します。
 * RV64GC（64ビット汎用RISC-V + 圧縮命令）をターゲットとします。
 */
class RISCVCodeGenerator {
public:
    /**
     * @brief コンストラクタ
     */
    RISCVCodeGenerator() = default;
    
    /**
     * @brief デストラクタ
     */
    ~RISCVCodeGenerator() = default;
    
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
     * @brief 即値をレジスタにロード
     * @param reg 対象レジスタ
     * @param value 即値
     * @param out 出力バイナリコード
     */
    void EmitLoadImmediate(int reg, int64_t value, std::vector<uint8_t>& out) noexcept;
    
    /**
     * @brief メモリからレジスタにロード
     * @param reg 対象レジスタ
     * @param base ベースレジスタ
     * @param offset オフセット値
     * @param out 出力バイナリコード
     */
    void EmitLoadMemory(int reg, int base, int offset, std::vector<uint8_t>& out) noexcept;
    
    /**
     * @brief レジスタからメモリへストア
     * @param reg ソースレジスタ
     * @param base ベースレジスタ
     * @param offset オフセット値
     * @param out 出力バイナリコード
     */
    void EmitStoreMemory(int reg, int base, int offset, std::vector<uint8_t>& out) noexcept;
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_BACKEND_RISCV_CODE_GENERATOR_H