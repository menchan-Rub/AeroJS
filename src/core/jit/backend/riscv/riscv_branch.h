#ifndef AEROJS_CORE_JIT_BACKEND_RISCV_BRANCH_H
#define AEROJS_CORE_JIT_BACKEND_RISCV_BRANCH_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace aerojs {
namespace core {

/**
 * @brief RISC-V条件分岐命令の条件コード
 */
enum BranchCondition {
    BRANCH_EQ,  // Equal (BEQ)
    BRANCH_NE,  // Not Equal (BNE)
    BRANCH_LT,  // Less Than (BLT)
    BRANCH_GE,  // Greater Than or Equal (BGE)
    BRANCH_LTU, // Less Than Unsigned (BLTU)
    BRANCH_GEU  // Greater Than or Equal Unsigned (BGEU)
};

/**
 * @brief 未解決の分岐参照
 */
struct BranchRef {
    size_t srcPos;           // 分岐命令の位置
    std::string targetLabel; // 対象ラベル
    BranchCondition cond;    // 条件コード (無条件分岐の場合は使用されない)
    bool isConditional;      // 条件付き分岐かどうか
    int rs1;                 // 条件付き分岐での比較レジスタ1
    int rs2;                 // 条件付き分岐での比較レジスタ2
};

/**
 * @brief RISC-V分岐命令を管理するクラス
 */
class RISCVBranchManager {
public:
    RISCVBranchManager() = default;
    
    /**
     * @brief 命令をエンコードして出力バッファに追加
     * 
     * @param out 出力バッファ
     * @param instruction 32ビット命令
     */
    static void appendInstruction(std::vector<uint8_t>& out, uint32_t instruction);
    
    /**
     * @brief 条件分岐命令を生成 (BEQ/BNE/BLT/BGE/BLTU/BGEU)
     * 
     * @param out 出力バッファ
     * @param rs1 比較レジスタ1
     * @param rs2 比較レジスタ2
     * @param offset 分岐オフセット
     * @param condition 分岐条件
     */
    static void emitBranchCond(std::vector<uint8_t>& out, int rs1, int rs2, int32_t offset, BranchCondition condition);
    
    /**
     * @brief 無条件分岐命令を生成 (JAL rd, offset)
     * 
     * @param out 出力バッファ
     * @param rd 戻りアドレスを格納するレジスタ（ゼロレジスタの場合は使用しない）
     * @param offset 分岐オフセット
     */
    static void emitJump(std::vector<uint8_t>& out, int rd, int32_t offset);
    
    /**
     * @brief レジスタ間接無条件分岐命令を生成 (JALR rd, rs1, offset)
     * 
     * @param out 出力バッファ
     * @param rd 戻りアドレスを格納するレジスタ（ゼロレジスタの場合は使用しない）
     * @param rs1 ベースレジスタ
     * @param offset 分岐オフセット
     */
    static void emitJumpRegister(std::vector<uint8_t>& out, int rd, int rs1, int32_t offset);
    
    /**
     * @brief ゼロ比較条件分岐命令を生成 (BEQZ/BNEZ)
     * 
     * @param out 出力バッファ
     * @param rs 比較レジスタ
     * @param offset 分岐オフセット
     * @param eq ゼロ比較の場合true、非ゼロ比較の場合false
     */
    static void emitBranchZero(std::vector<uint8_t>& out, int rs, int32_t offset, bool eq);
    
    /**
     * @brief ラベルを定義
     * 
     * @param label ラベル名
     * @param pos 現在位置
     */
    void defineLabel(const std::string& label, size_t pos);
    
    /**
     * @brief ラベルへの条件分岐を追加
     * 
     * @param out 出力バッファ
     * @param rs1 比較レジスタ1
     * @param rs2 比較レジスタ2
     * @param targetLabel 対象ラベル
     * @param condition 分岐条件
     * @return 分岐参照ID
     */
    size_t addBranchToLabel(std::vector<uint8_t>& out, int rs1, int rs2, const std::string& targetLabel, BranchCondition condition);
    
    /**
     * @brief ラベルへの無条件分岐を追加
     * 
     * @param out 出力バッファ
     * @param rd 戻りアドレスを格納するレジスタ（ゼロレジスタの場合は使用しない）
     * @param targetLabel 対象ラベル
     * @return 分岐参照ID
     */
    size_t addJumpToLabel(std::vector<uint8_t>& out, int rd, const std::string& targetLabel);
    
    /**
     * @brief すべての未解決分岐を解決
     * 
     * @param out 出力バッファ
     */
    void resolveAllBranches(std::vector<uint8_t>& out);
    
    /**
     * @brief 指定ラベルへのすべての未解決分岐を解決
     * 
     * @param out 出力バッファ
     * @param label 対象ラベル
     */
    void resolveBranchesToLabel(std::vector<uint8_t>& out, const std::string& label);
    
    /**
     * @brief 条件を反転
     * 
     * @param condition 分岐条件
     * @return 反転された条件
     */
    static BranchCondition invertCondition(BranchCondition condition);

private:
    // 未解決の分岐参照リスト
    std::vector<BranchRef> branchRefs_;
    
    // ラベル位置マップ
    std::unordered_map<std::string, size_t> labelPositions_;
    
    /**
     * @brief 分岐命令のオフセットをパッチ
     * 
     * @param out 出力バッファ
     * @param pos 分岐命令の位置
     * @param offset 新しいオフセット
     * @param isConditional 条件付き分岐かどうか
     */
    static void patchBranchOffset(std::vector<uint8_t>& out, size_t pos, int32_t offset, bool isConditional);
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_BACKEND_RISCV_BRANCH_H 