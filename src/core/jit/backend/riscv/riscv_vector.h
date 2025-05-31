#ifndef AEROJS_CORE_JIT_BACKEND_RISCV_VECTOR_H
#define AEROJS_CORE_JIT_BACKEND_RISCV_VECTOR_H

#include <cstdint>
#include <vector>
#include "../../ir/ir_instruction.h"

namespace aerojs {
namespace core {

/**
 * @brief RISC-V Vector Extension (RVV) のベクトル長設定
 */
enum RVVectorLMUL {
    LMUL_1_8 = 0b101,  // LMUL=1/8
    LMUL_1_4 = 0b110,  // LMUL=1/4
    LMUL_1_2 = 0b111,  // LMUL=1/2
    LMUL_1   = 0b000,  // LMUL=1
    LMUL_2   = 0b001,  // LMUL=2
    LMUL_4   = 0b010,  // LMUL=4
    LMUL_8   = 0b011   // LMUL=8
};

/**
 * @brief RISC-V Vector Extension (RVV) の要素幅
 */
enum RVVectorSEW {
    SEW_8  = 0b000,  // 8-bit elements
    SEW_16 = 0b001,  // 16-bit elements
    SEW_32 = 0b010,  // 32-bit elements
    SEW_64 = 0b011   // 64-bit elements
};

/**
 * @brief RISC-V Vector Extension (RVV) のベクトルマスク
 */
enum RVVectorMask {
    MASK_NONE    = 0b0,  // マスクなし（全て実行）
    MASK_ENABLED = 0b1   // マスク有効
};

/**
 * @brief RISC-V Vector Extension (RVV) の丸めモード
 */
enum RVVectorRM {
    RM_RNE = 0b000,  // Round to Nearest, ties to Even
    RM_RTZ = 0b001,  // Round towards Zero
    RM_RDN = 0b010,  // Round Down (towards -infinity)
    RM_RUP = 0b011,  // Round Up (towards +infinity)
    RM_RMM = 0b100,  // Round to Nearest, ties to Max Magnitude
    RM_DYN = 0b111   // Dynamic rounding mode
};

/**
 * @brief RISC-V Vector Extension (RVV) のテールポリシー
 */
enum RVVectorVTA {
    VTA_DEFAULT = 0b0,  // テールagnostic (非アクティブな要素を変更可能)
    VTA_UNDISTURBED = 0b1  // テールundisturbed (非アクティブな要素を保持)
};

/**
 * @brief RISC-V Vector Extension (RVV) のマスクポリシー
 */
enum RVVectorVMA {
    VMA_DEFAULT = 0b0,  // マスクagnostic (マスクされた要素を変更可能)
    VMA_UNDISTURBED = 0b1  // マスクundisturbed (マスクされた要素を保持)
};

/**
 * @brief RISC-V Vector Extension (RVV) 操作クラス
 */
class RISCVVector {
public:
    /**
     * @brief 命令をエンコードして出力バッファに追加
     * 
     * @param out 出力バッファ
     * @param instruction 32ビット命令
     */
    static void appendInstruction(std::vector<uint8_t>& out, uint32_t instruction);
    
    /**
     * @brief ベクトル長設定 (vsetvli)
     * 
     * @param out 出力バッファ
     * @param rd 出力レジスタ (x0でvl無視)
     * @param rs1 最大ベクトル長レジスタ (x0でAVLMAX)
     * @param sew 要素幅
     * @param lmul ベクトル長倍率
     * @param vta テールポリシー
     * @param vma マスクポリシー
     */
    static void emitSetVL(std::vector<uint8_t>& out, int rd, int rs1, 
                         RVVectorSEW sew, RVVectorLMUL lmul,
                         RVVectorVTA vta = VTA_DEFAULT, RVVectorVMA vma = VMA_DEFAULT);
    
    /**
     * @brief ベクトルロード (VLE)
     * 
     * @param out 出力バッファ
     * @param vd 宛先ベクトルレジスタ
     * @param rs1 ベースアドレスレジスタ
     * @param vm マスク指定
     * @param width 要素幅 (8/16/32/64)
     */
    static void emitVectorLoad(std::vector<uint8_t>& out, int vd, int rs1, RVVectorMask vm, int width);
    
    /**
     * @brief ストライドベクトルロード (VLSE)
     * 
     * @param out 出力バッファ
     * @param vd 宛先ベクトルレジスタ
     * @param rs1 ベースアドレスレジスタ
     * @param rs2 ストライドレジスタ
     * @param vm マスク指定
     * @param width 要素幅 (8/16/32/64)
     */
    static void emitVectorLoadStrided(std::vector<uint8_t>& out, int vd, int rs1, int rs2, RVVectorMask vm, int width);
    
    /**
     * @brief ベクトルストア (VSE)
     * 
     * @param out 出力バッファ
     * @param vs3 ソースベクトルレジスタ
     * @param rs1 ベースアドレスレジスタ
     * @param vm マスク指定
     * @param width 要素幅 (8/16/32/64)
     */
    static void emitVectorStore(std::vector<uint8_t>& out, int vs3, int rs1, RVVectorMask vm, int width);
    
    /**
     * @brief ストライドベクトルストア (VSSE)
     * 
     * @param out 出力バッファ
     * @param vs3 ソースベクトルレジスタ
     * @param rs1 ベースアドレスレジスタ
     * @param rs2 ストライドレジスタ
     * @param vm マスク指定
     * @param width 要素幅 (8/16/32/64)
     */
    static void emitVectorStoreStrided(std::vector<uint8_t>& out, int vs3, int rs1, int rs2, RVVectorMask vm, int width);
    
    /**
     * @brief ベクトル-ベクトル加算 (VADD)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ
     * @param vs1 ソースベクトルレジスタ1
     * @param vs2 ソースベクトルレジスタ2
     * @param vm マスク指定
     */
    static void emitVectorAdd(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm);
    
    /**
     * @brief ベクトル-スカラー加算 (VADD.VX)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ
     * @param vs2 ソースベクトルレジスタ
     * @param rs1 ソーススカラーレジスタ
     * @param vm マスク指定
     */
    static void emitVectorAddScalar(std::vector<uint8_t>& out, int vd, int vs2, int rs1, RVVectorMask vm);
    
    /**
     * @brief ベクトル-ベクトル減算 (VSUB)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ
     * @param vs1 ソースベクトルレジスタ1
     * @param vs2 ソースベクトルレジスタ2
     * @param vm マスク指定
     */
    static void emitVectorSub(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm);
    
    /**
     * @brief ベクトル-ベクトル乗算 (VMUL)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ
     * @param vs1 ソースベクトルレジスタ1
     * @param vs2 ソースベクトルレジスタ2
     * @param vm マスク指定
     */
    static void emitVectorMul(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm);
    
    /**
     * @brief ベクトル-スカラー乗算 (VMUL.VX)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ
     * @param vs2 ソースベクトルレジスタ
     * @param rs1 ソーススカラーレジスタ
     * @param vm マスク指定
     */
    static void emitVectorMulScalar(std::vector<uint8_t>& out, int vd, int vs2, int rs1, RVVectorMask vm);
    
    /**
     * @brief ベクトル-ベクトル除算 (VDIV)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ
     * @param vs1 ソースベクトルレジスタ1
     * @param vs2 ソースベクトルレジスタ2
     * @param vm マスク指定
     */
    static void emitVectorDiv(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm);
    
    /**
     * @brief 乗算加算融合命令 (VMACC)
     * 
     * @param out 出力バッファ
     * @param vd 加算対象/出力ベクトルレジスタ
     * @param vs1 乗算ソースベクトルレジスタ1
     * @param vs2 乗算ソースベクトルレジスタ2
     * @param vm マスク指定
     */
    static void emitVectorMulAdd(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm);
    
    /**
     * @brief ベクトル要素最大値 (VREDMAX)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ (v0のスカラー要素)
     * @param vs1 ソースベクトルレジスタ1 (初期値)
     * @param vs2 ソースベクトルレジスタ2
     * @param vm マスク指定
     */
    static void emitVectorRedMax(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm);
    
    /**
     * @brief ベクトル要素最小値 (VREDMIN)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ (v0のスカラー要素)
     * @param vs1 ソースベクトルレジスタ1 (初期値)
     * @param vs2 ソースベクトルレジスタ2
     * @param vm マスク指定
     */
    static void emitVectorRedMin(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm);
    
    /**
     * @brief ベクトル要素合計 (VREDSUM)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ (v0のスカラー要素)
     * @param vs1 ソースベクトルレジスタ1 (初期値)
     * @param vs2 ソースベクトルレジスタ2
     * @param vm マスク指定
     */
    static void emitVectorRedSum(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm);
    
    /**
     * @brief ベクトルマスク生成 (VMSEQ/VMSNE/etc)
     * 
     * @param out 出力バッファ
     * @param vd マスク出力ベクトルレジスタ
     * @param vs1 ソースベクトルレジスタ1
     * @param vs2 ソースベクトルレジスタ2
     * @param vm マスク指定
     * @param cmp 比較操作（0=eq, 1=ne, 2=lt, 3=le, 4=gt, 5=ge）
     */
    static void emitVectorCompare(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm, int cmp);
    
    /**
     * @brief スライド命令 (VSLIDEUP/VSLIDEDOWN)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ
     * @param vs2 ソースベクトルレジスタ
     * @param rs1 オフセットスカラーレジスタ
     * @param vm マスク指定
     * @param up 上方向スライドの場合true、下方向の場合false
     */
    static void emitVectorSlide(std::vector<uint8_t>& out, int vd, int vs2, int rs1, RVVectorMask vm, bool up);
    
    /**
     * @brief ループが自動ベクトル化可能か判定
     * 
     * @param loopInsts ループ内の命令列
     * @return ベクトル化可能な場合true
     */
    static bool canVectorize(const std::vector<IRInstruction>& loopInsts);
    
    /**
     * @brief ベクトル化前処理コード生成
     * 
     * @param loopInsts ループ命令列
     * @param out 出力バッファ
     */
    static void emitPreloopCode(const std::vector<IRInstruction>& loopInsts, std::vector<uint8_t>& out);
    
    /**
     * @brief ベクトル化ループ本体生成
     * 
     * @param loopInsts ループ命令列
     * @param out 出力バッファ
     */
    static void emitVectorizedLoop(const std::vector<IRInstruction>& loopInsts, std::vector<uint8_t>& out);
    
    /**
     * @brief ベクトル化後処理コード生成
     * 
     * @param loopInsts ループ命令列
     * @param out 出力バッファ
     */
    static void emitPostloopCode(const std::vector<IRInstruction>& loopInsts, std::vector<uint8_t>& out);
    
    /**
     * @brief ベクトル数学関数 - 平方根 (VSQRT)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ
     * @param vs2 ソースベクトルレジスタ
     * @param vm マスク指定
     */
    static void emitVectorSqrt(std::vector<uint8_t>& out, int vd, int vs2, RVVectorMask vm);
    
    /**
     * @brief ベクトル数学関数 - 絶対値 (VABS)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ
     * @param vs2 ソースベクトルレジスタ
     * @param vm マスク指定
     */
    static void emitVectorAbs(std::vector<uint8_t>& out, int vd, int vs2, RVVectorMask vm);
    
    /**
     * @brief ベクトルビット操作 - 論理AND (VAND)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ
     * @param vs1 ソースベクトルレジスタ1
     * @param vs2 ソースベクトルレジスタ2
     * @param vm マスク指定
     */
    static void emitVectorAnd(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm);
    
    /**
     * @brief ベクトルビット操作 - 論理OR (VOR)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ
     * @param vs1 ソースベクトルレジスタ1
     * @param vs2 ソースベクトルレジスタ2
     * @param vm マスク指定
     */
    static void emitVectorOr(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm);
    
    /**
     * @brief ベクトルビット操作 - 論理XOR (VXOR)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ
     * @param vs1 ソースベクトルレジスタ1
     * @param vs2 ソースベクトルレジスタ2
     * @param vm マスク指定
     */
    static void emitVectorXor(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm);
    
    /**
     * @brief ベクトルビット操作 - 論理NOT (VNOT)
     * 
     * @param out 出力バッファ
     * @param vd 出力ベクトルレジスタ
     * @param vs2 ソースベクトルレジスタ
     * @param vm マスク指定
     */
    static void emitVectorNot(std::vector<uint8_t>& out, int vd, int vs2, RVVectorMask vm);
    
    /**
     * @brief 行列乗算操作
     *
     * @param out 出力バッファ
     * @param rows 行数
     * @param cols 列数
     * @param inner 内側の次元サイズ
     */
    static void emitMatrixMultiply(std::vector<uint8_t>& out, int rows, int cols, int inner);
    
    /**
     * @brief JavaScript配列のSIMD処理用ファンクション
     *
     * @param out 出力バッファ
     * @param operation 操作種類 (0=map, 1=filter, 2=reduce, 3=forEach)
     * @param arrayReg 入力配列レジスタ
     * @param resultReg 結果配列レジスタ
     * @param length 配列長
     */
    static void emitJSArrayOperation(std::vector<uint8_t>& out, int operation, int arrayReg, int resultReg, int length);
    
    // ヘルパーメソッド
    
    /**
     * @brief VSET命令のエンコード
     *
     * @param rd 出力レジスタ
     * @param uimm 即値
     * @param sew 要素幅
     * @param lmul ベクトル長倍率
     * @return エンコードされた命令
     */
    static uint32_t encodeVsetivli(int rd, int uimm, RVVectorSEW sew, RVVectorLMUL lmul);
    
    /**
     * @brief ベクトル操作のエンコード
     *
     * @param opcode 操作コード
     * @param vd 出力ベクトルレジスタ
     * @param vs1 ソースベクトルレジスタ1
     * @param vs2 ソースベクトルレジスタ2
     * @param vm マスク指定
     * @param funct6 関数コード
     * @return エンコードされた命令
     */
    static uint32_t encodeVectorOp(uint32_t opcode, uint32_t vd, uint32_t vs1, uint32_t vs2, uint32_t vm, uint32_t funct6);
    
    /**
     * @brief ベクトルロード命令のエンコード
     *
     * @param vd 出力ベクトルレジスタ
     * @param rs1 ベースアドレスレジスタ
     * @param vm マスク指定
     * @param width 要素幅
     * @return エンコードされた命令
     */
    static uint32_t encodeVectorLoad(uint32_t vd, uint32_t rs1, RVVectorMask vm, uint32_t width);
    
    /**
     * @brief ベクトルストライドロード命令のエンコード
     *
     * @param vd 出力ベクトルレジスタ
     * @param rs1 ベースアドレスレジスタ
     * @param rs2 ストライドレジスタ
     * @param stride ストライド値
     * @param vm マスク指定
     * @param width 要素幅
     * @return エンコードされた命令
     */
    static uint32_t encodeVectorStrideLoad(uint32_t vd, uint32_t rs1, uint32_t rs2, uint32_t stride, RVVectorMask vm, uint32_t width);
    
    /**
     * @brief ベクトルストア命令のエンコード
     *
     * @param vs3 ソースベクトルレジスタ
     * @param rs1 ベースアドレスレジスタ
     * @param rs2 オフセットレジスタ
     * @param vm マスク指定
     * @param width 要素幅
     * @return エンコードされた命令
     */
    static uint32_t encodeVectorStore(uint32_t vs3, uint32_t rs1, uint32_t rs2, RVVectorMask vm, uint32_t width);
    
    /**
     * @brief 条件分岐命令のエンコード (Bタイプ)
     *
     * @param rs1 ソースレジスタ1
     * @param rs2 ソースレジスタ2
     * @param funct3 関数コード
     * @param opcode 操作コード
     * @param offset 分岐オフセット
     * @return エンコードされた命令
     */
    static uint32_t encodeBType(uint32_t rs1, uint32_t rs2, uint32_t funct3, uint32_t opcode, int32_t offset);
    
    /**
     * @brief Rタイプ命令のエンコード
     *
     * @param funct7 関数コード7
     * @param rs2 ソースレジスタ2
     * @param rs1 ソースレジスタ1
     * @param funct3 関数コード3
     * @param rd 出力レジスタ
     * @param opcode 操作コード
     * @return エンコードされた命令
     */
    static uint32_t encodeRType(uint32_t funct7, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode);
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_BACKEND_RISCV_VECTOR_H 