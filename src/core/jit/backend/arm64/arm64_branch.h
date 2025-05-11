#ifndef AEROJS_CORE_JIT_BACKEND_ARM64_BRANCH_H
#define AEROJS_CORE_JIT_BACKEND_ARM64_BRANCH_H

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>

namespace aerojs {
namespace core {

/**
 * @brief 条件コード定義
 */
enum ConditionCode {
    COND_EQ = 0x0,   // Equal
    COND_NE = 0x1,   // Not Equal
    COND_HS = 0x2,   // Unsigned higher or same (または CS, Carry Set)
    COND_LO = 0x3,   // Unsigned lower (または CC, Carry Clear)
    COND_MI = 0x4,   // Minus, Negative
    COND_PL = 0x5,   // Plus, Positive or Zero
    COND_VS = 0x6,   // Overflow
    COND_VC = 0x7,   // No Overflow
    COND_HI = 0x8,   // Unsigned Higher
    COND_LS = 0x9,   // Unsigned Lower or Same
    COND_GE = 0xA,   // Signed Greater Than or Equal
    COND_LT = 0xB,   // Signed Less Than
    COND_GT = 0xC,   // Signed Greater Than
    COND_LE = 0xD,   // Signed Less Than or Equal
    COND_AL = 0xE,   // Always (unconditional)
    COND_NV = 0xF    // 予約済み（使用しない）
};

/**
 * @brief 比較オペレーション
 */
enum CompareOperation {
    CMP_EQ,    // Equal
    CMP_NE,    // Not Equal
    CMP_LT,    // Less Than
    CMP_LE,    // Less Than or Equal
    CMP_GT,    // Greater Than
    CMP_GE,    // Greater Than or Equal
    CMP_ULT,   // Unsigned Less Than
    CMP_ULE,   // Unsigned Less Than or Equal
    CMP_UGT,   // Unsigned Greater Than
    CMP_UGE    // Unsigned Greater Than or Equal
};

/**
 * @brief 分岐参照情報を保持する構造体
 */
struct BranchRef {
    size_t srcPos;        // 分岐命令の位置
    size_t dstPos;        // 分岐先の位置
    ConditionCode cond;   // 条件コード
    bool isResolved;      // 解決済みかどうか
    std::string label;    // ラベル（オプション）
};

/**
 * @brief ARM64の分岐命令を管理するクラス
 */
class ARM64BranchManager {
public:
    ARM64BranchManager() = default;
    
    /**
     * @brief 無条件分岐命令を生成する
     * 
     * @param out 出力バッファ
     * @param offset 相対オフセット（バイト単位）
     */
    static void emitBranch(std::vector<uint8_t>& out, int32_t offset);
    
    /**
     * @brief リンクありの無条件分岐命令を生成する (BL)
     * 
     * @param out 出力バッファ
     * @param offset 相対オフセット（バイト単位）
     */
    static void emitBranchLink(std::vector<uint8_t>& out, int32_t offset);
    
    /**
     * @brief 条件付き分岐命令を生成する
     * 
     * @param out 出力バッファ
     * @param condition 条件コード
     * @param offset 相対オフセット（バイト単位）
     */
    static void emitBranchCond(std::vector<uint8_t>& out, ConditionCode condition, int32_t offset);
    
    /**
     * @brief レジスタ比較とフラグ設定 (CMP)
     * 
     * @param out 出力バッファ
     * @param rn 比較レジスタ1
     * @param rm 比較レジスタ2
     */
    static void emitCompare(std::vector<uint8_t>& out, int rn, int rm);
    
    /**
     * @brief レジスタと即値の比較 (CMP immediate)
     * 
     * @param out 出力バッファ
     * @param rn 比較レジスタ
     * @param imm 即値（12ビット以内）
     */
    static void emitCompareImm(std::vector<uint8_t>& out, int rn, int32_t imm);
    
    /**
     * @brief 条件付きレジスタ設定 (CSEL)
     * 
     * @param out 出力バッファ
     * @param rd 出力レジスタ
     * @param rn 条件が真の場合の入力レジスタ
     * @param rm 条件が偽の場合の入力レジスタ
     * @param condition 条件コード
     */
    static void emitCondSelect(std::vector<uint8_t>& out, int rd, int rn, int rm, ConditionCode condition);
    
    /**
     * @brief 比較操作結果をレジスタに設定 (CSET)
     * 
     * @param out 出力バッファ
     * @param rd 出力レジスタ
     * @param condition 条件コード
     */
    static void emitCondSet(std::vector<uint8_t>& out, int rd, ConditionCode condition);
    
    /**
     * @brief 未解決の分岐参照を追加
     * 
     * @param out 出力バッファ
     * @param condition 条件コード
     * @param label 分岐先ラベル
     * @return 分岐参照インデックス
     */
    size_t addBranchRef(std::vector<uint8_t>& out, ConditionCode condition, const std::string& label);
    
    /**
     * @brief 分岐先ラベルを定義
     * 
     * @param label ラベル名
     * @param pos ラベル位置（バイト単位）
     */
    void defineLabel(const std::string& label, size_t pos);
    
    /**
     * @brief 全ての未解決分岐を解決する
     * 
     * @param out 出力バッファ
     */
    void resolveAllBranches(std::vector<uint8_t>& out);
    
    /**
     * @brief 特定ラベルへの全ての未解決分岐を解決する
     * 
     * @param out 出力バッファ
     * @param label ラベル名
     * @param targetPos ラベル位置（バイト単位）
     */
    void resolveBranchesToLabel(std::vector<uint8_t>& out, const std::string& label, size_t targetPos);
    
    /**
     * @brief 比較操作を条件コードに変換
     * 
     * @param op 比較操作
     * @return 対応する条件コード
     */
    static ConditionCode compareOpToCondCode(CompareOperation op);
    
    /**
     * @brief 逆条件を取得
     * 
     * @param cond 条件コード
     * @return 逆条件
     */
    static ConditionCode invertCondition(ConditionCode cond);
    
    /**
     * @brief 比較系命令と条件分岐を生成する
     * 
     * @param out 出力バッファ
     * @param lhs 左辺レジスタ
     * @param rhs 右辺レジスタ
     * @param op 比較操作
     * @param offset 分岐オフセット
     */
    static void emitCompareAndBranch(std::vector<uint8_t>& out, int lhs, int rhs, CompareOperation op, int32_t offset);
    
    /**
     * @brief ゼロ比較と分岐を単一命令で生成 (CBZ/CBNZ)
     * 
     * @param out 出力バッファ
     * @param reg テストするレジスタ
     * @param isZero ゼロ比較の場合true, 非ゼロ比較の場合false
     * @param offset 分岐オフセット
     */
    static void emitTestAndBranch(std::vector<uint8_t>& out, int reg, bool isZero, int32_t offset);
    
private:
    // 未解決分岐参照
    std::vector<BranchRef> branchRefs_;
    
    // ラベル位置マップ
    std::unordered_map<std::string, size_t> labelPositions_;
    
    /**
     * @brief 分岐命令をパッチする
     * 
     * @param out 出力バッファ
     * @param pos パッチする位置
     * @param offset 相対オフセット（バイト単位）
     * @param isCond 条件付き分岐の場合true
     */
    static void patchBranchOffset(std::vector<uint8_t>& out, size_t pos, int32_t offset, bool isCond);
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_BACKEND_ARM64_BRANCH_H 