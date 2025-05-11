#ifndef AEROJS_CORE_JIT_BACKEND_ARM64_SIMD_H
#define AEROJS_CORE_JIT_BACKEND_ARM64_SIMD_H

#include <cstdint>
#include <vector>
#include <string>

#include "../../ir/ir_instruction.h"

namespace aerojs {
namespace core {

// SIMD レジスタ定義
enum ARM64SIMDRegisters {
    V0 = 0,
    V1 = 1,
    V2 = 2,
    V3 = 3,
    V4 = 4,
    V5 = 5,
    V6 = 6,
    V7 = 7,
    V8 = 8,
    V9 = 9,
    V10 = 10,
    V11 = 11,
    V12 = 12,
    V13 = 13,
    V14 = 14,
    V15 = 15,
    V16 = 16,
    V17 = 17,
    V18 = 18,
    V19 = 19,
    V20 = 20,
    V21 = 21,
    V22 = 22,
    V23 = 23,
    V24 = 24,
    V25 = 25,
    V26 = 26,
    V27 = 27,
    V28 = 28,
    V29 = 29,
    V30 = 30,
    V31 = 31
};

// SIMD ベクトル長設定
enum VectorArrangement {
    ARRANGE_8B  = 0x00, // 8x 8-bit
    ARRANGE_16B = 0x01, // 16x 8-bit
    ARRANGE_4H  = 0x02, // 4x 16-bit
    ARRANGE_8H  = 0x03, // 8x 16-bit
    ARRANGE_2S  = 0x04, // 2x 32-bit
    ARRANGE_4S  = 0x05, // 4x 32-bit
    ARRANGE_1D  = 0x06, // 1x 64-bit
    ARRANGE_2D  = 0x07  // 2x 64-bit
};

// ベクトル要素サイズ
enum ElementSize {
    ELEM_B = 0x00,  // 8-bit
    ELEM_H = 0x01,  // 16-bit
    ELEM_S = 0x02,  // 32-bit
    ELEM_D = 0x03   // 64-bit
};

/**
 * @brief ARM64 SIMD操作の管理クラス
 */
class ARM64SIMD {
public:
    /**
     * @brief SIMD命令をエンコードしてバッファに追加
     * 
     * @param out 出力バッファ
     * @param instruction 32ビット命令
     */
    static void appendInstruction(std::vector<uint8_t>& out, uint32_t instruction);
    
    /**
     * @brief 2つのSIMDレジスタをロード (LDNP)
     * 
     * @param out 出力バッファ
     * @param vt1 ターゲットレジスタ1
     * @param vt2 ターゲットレジスタ2
     * @param xn ベースレジスタ
     * @param offset オフセット
     */
    static void emitLoadPair(std::vector<uint8_t>& out, int vt1, int vt2, int xn, int offset);
    
    /**
     * @brief 2つのSIMDレジスタをストア (STNP)
     * 
     * @param out 出力バッファ
     * @param vt1 ソースレジスタ1
     * @param vt2 ソースレジスタ2
     * @param xn ベースレジスタ
     * @param offset オフセット
     */
    static void emitStorePair(std::vector<uint8_t>& out, int vt1, int vt2, int xn, int offset);
    
    /**
     * @brief 64ビット値をロード (LDR Q)
     * 
     * @param out 出力バッファ
     * @param vt ターゲットレジスタ
     * @param xn ベースレジスタ
     * @param offset オフセット
     */
    static void emitLoadQuad(std::vector<uint8_t>& out, int vt, int xn, int offset);
    
    /**
     * @brief 64ビット値をストア (STR Q)
     * 
     * @param out 出力バッファ
     * @param vt ソースレジスタ
     * @param xn ベースレジスタ
     * @param offset オフセット
     */
    static void emitStoreQuad(std::vector<uint8_t>& out, int vt, int xn, int offset);
    
    /**
     * @brief ベクタ加算 (FADD)
     * 
     * @param out 出力バッファ
     * @param vd 出力レジスタ
     * @param vn 入力レジスタ1
     * @param vm 入力レジスタ2
     * @param arrangement ベクトル配置
     */
    static void emitVectorAdd(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement);
    
    /**
     * @brief ベクタ減算 (FSUB)
     * 
     * @param out 出力バッファ
     * @param vd 出力レジスタ
     * @param vn 入力レジスタ1
     * @param vm 入力レジスタ2
     * @param arrangement ベクトル配置
     */
    static void emitVectorSub(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement);
    
    /**
     * @brief ベクタ乗算 (FMUL)
     * 
     * @param out 出力バッファ
     * @param vd 出力レジスタ
     * @param vn 入力レジスタ1
     * @param vm 入力レジスタ2
     * @param arrangement ベクトル配置
     */
    static void emitVectorMul(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement);
    
    /**
     * @brief ベクタ除算 (FDIV)
     * 
     * @param out 出力バッファ
     * @param vd 出力レジスタ
     * @param vn 入力レジスタ1
     * @param vm 入力レジスタ2
     * @param arrangement ベクトル配置
     */
    static void emitVectorDiv(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement);
    
    /**
     * @brief 乗算加算融合 (FMLA)
     * 
     * @param out 出力バッファ
     * @param vd 出力および加算レジスタ
     * @param vn 入力レジスタ1
     * @param vm 入力レジスタ2
     * @param arrangement ベクトル配置
     */
    static void emitVectorMulAdd(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement);
    
    /**
     * @brief 全要素を同一値に設定 (DUP)
     * 
     * @param out 出力バッファ
     * @param vd 出力レジスタ
     * @param vn 入力レジスタ
     * @param index 入力要素インデックス
     * @param elementSize 要素サイズ
     */
    static void emitDuplicateElement(std::vector<uint8_t>& out, int vd, int vn, int index, ElementSize elementSize);
    
    /**
     * @brief ベクトル要素シャッフル (TBL)
     * 
     * @param out 出力バッファ
     * @param vd 出力レジスタ
     * @param vn 入力レジスタ（テーブル）
     * @param vm インデックスレジスタ
     * @param arrangement ベクトル配置
     */
    static void emitTableLookup(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement);
    
    /**
     * @brief SIMD レジスタをゼロクリア (MOVI 0)
     * 
     * @param out 出力バッファ
     * @param vd 出力レジスタ
     * @param arrangement ベクトル配置
     */
    static void emitClearVector(std::vector<uint8_t>& out, int vd, VectorArrangement arrangement);
    
    /**
     * @brief ベクトル最大値 (FMAX)
     * 
     * @param out 出力バッファ
     * @param vd 出力レジスタ
     * @param vn 入力レジスタ1
     * @param vm 入力レジスタ2
     * @param arrangement ベクトル配置
     */
    static void emitVectorMax(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement);
    
    /**
     * @brief ベクトル最小値 (FMIN)
     * 
     * @param out 出力バッファ
     * @param vd 出力レジスタ
     * @param vn 入力レジスタ1
     * @param vm 入力レジスタ2
     * @param arrangement ベクトル配置
     */
    static void emitVectorMin(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement);
    
    /**
     * @brief 特定ループをベクトル化できるか判定
     * 
     * @param loopInsts ループ中の命令列
     * @return ベクトル化可能な場合true
     */
    static bool canVectorize(const std::vector<IRInstruction>& loopInsts);
    
    /**
     * @brief ループ不変式を検出
     * 
     * @param loopInsts ループ中の命令列
     * @return ループ不変式のインデックスリスト
     */
    static std::vector<size_t> detectLoopInvariants(const std::vector<IRInstruction>& loopInsts);
    
    /**
     * @brief ベクトル化前コード生成
     * 
     * @param loopInsts ループ命令列
     * @param out 出力バッファ
     */
    static void emitPreloopCode(const std::vector<IRInstruction>& loopInsts, std::vector<uint8_t>& out);
    
    /**
     * @brief ベクトル化メインループコード生成
     * 
     * @param loopInsts ループ命令列
     * @param invariants ループ不変式
     * @param out 出力バッファ
     */
    static void emitVectorizedLoopBody(
        const std::vector<IRInstruction>& loopInsts,
        const std::vector<size_t>& invariants,
        std::vector<uint8_t>& out);
    
    /**
     * @brief ベクトル化後コード生成
     * 
     * @param loopInsts ループ命令列
     * @param out 出力バッファ
     */
    static void emitPostloopCode(const std::vector<IRInstruction>& loopInsts, std::vector<uint8_t>& out);
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_BACKEND_ARM64_SIMD_H 