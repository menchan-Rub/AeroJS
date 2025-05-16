/**
 * @file arm64_sve.h
 * @brief ARM64 Scalable Vector Extension (SVE) のサポート
 * @version 1.0.0
 * @license MIT
 */

#ifndef AEROJS_CORE_JIT_BACKEND_ARM64_SVE_H
#define AEROJS_CORE_JIT_BACKEND_ARM64_SVE_H

#include <cstdint>
#include <vector>
#include <string>
#include "../../ir/ir_instruction.h"
#include "arm64_simd.h"

namespace aerojs {
namespace core {

// SVE プレディケートレジスタ
enum ARM64SVEPredicateRegisters {
    P0 = 0,
    P1 = 1,
    P2 = 2,
    P3 = 3,
    P4 = 4,
    P5 = 5,
    P6 = 6,
    P7 = 7,
    P8 = 8,
    P9 = 9,
    P10 = 10,
    P11 = 11,
    P12 = 12,
    P13 = 13,
    P14 = 14,
    P15 = 15
};

// SVE プレディケートパターン
enum PredicatePattern {
    PT_ALL     = 0x1F, // すべての要素がアクティブ
    PT_NONE    = 0x00, // アクティブな要素なし
    PT_FIRST   = 0x01, // 最初の要素がアクティブ
    PT_VL1     = 0x05, // 最初のVL要素がアクティブ
    PT_VL2     = 0x09, // 最初の2*VL要素がアクティブ
    PT_VL3     = 0x0D, // 最初の3*VL要素がアクティブ
    PT_VL4     = 0x11, // 最初の4*VL要素がアクティブ
    PT_VL5     = 0x15, // 最初の5*VL要素がアクティブ
    PT_VL6     = 0x19, // 最初の6*VL要素がアクティブ
    PT_VL7     = 0x1D, // 最初の7*VL要素がアクティブ
    PT_VL8     = 0x21, // 最初の8*VL要素がアクティブ
    PT_VL16    = 0x29, // 最初の16*VL要素がアクティブ
    PT_VL32    = 0x31, // 最初の32*VL要素がアクティブ
    PT_VL64    = 0x39, // 最初の64*VL要素がアクティブ
    PT_VL128   = 0x3D  // 最初の128*VL要素がアクティブ
};

/**
 * @brief ARM64 SVE操作の管理クラス
 */
class ARM64SVE {
public:
    /**
     * @brief SVE命令をエンコードしてバッファに追加
     * 
     * @param out 出力バッファ
     * @param instruction 32ビット命令
     */
    static void appendInstruction(std::vector<uint8_t>& out, uint32_t instruction);
    
    /**
     * @brief プレディケートレジスタを初期化
     * 
     * @param out 出力バッファ
     * @param pd 設定するプレディケートレジスタ
     * @param pattern プレディケートパターン
     * @param elementSize 要素サイズ
     */
    static void emitPredicateInit(std::vector<uint8_t>& out, int pd, PredicatePattern pattern, ElementSize elementSize);
    
    /**
     * @brief SVEレジスタを連続ロード
     * 
     * @param out 出力バッファ
     * @param zt ターゲットレジスタ
     * @param pg プレディケートレジスタ
     * @param xn ベースレジスタ
     * @param elementSize 要素サイズ
     */
    static void emitContiguousLoad(std::vector<uint8_t>& out, int zt, int pg, int xn, ElementSize elementSize);
    
    /**
     * @brief SVEレジスタを連続ストア
     * 
     * @param out 出力バッファ
     * @param zt ソースレジスタ
     * @param pg プレディケートレジスタ
     * @param xn ベースレジスタ
     * @param elementSize 要素サイズ
     */
    static void emitContiguousStore(std::vector<uint8_t>& out, int zt, int pg, int xn, ElementSize elementSize);
    
    /**
     * @brief SVEベクタ加算
     * 
     * @param out 出力バッファ
     * @param zd 出力レジスタ
     * @param zn 入力レジスタ1
     * @param zm 入力レジスタ2
     * @param pg プレディケートレジスタ
     * @param elementSize 要素サイズ
     */
    static void emitVectorAdd(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize);
    
    /**
     * @brief SVEベクタ減算
     * 
     * @param out 出力バッファ
     * @param zd 出力レジスタ
     * @param zn 入力レジスタ1
     * @param zm 入力レジスタ2
     * @param pg プレディケートレジスタ
     * @param elementSize 要素サイズ
     */
    static void emitVectorSub(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize);
    
    /**
     * @brief SVEベクタ乗算
     * 
     * @param out 出力バッファ
     * @param zd 出力レジスタ
     * @param zn 入力レジスタ1
     * @param zm 入力レジスタ2
     * @param pg プレディケートレジスタ
     * @param elementSize 要素サイズ
     */
    static void emitVectorMul(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize);
    
    /**
     * @brief SVEベクタ乗算加算
     * 
     * @param out 出力バッファ
     * @param zd 出力レジスタ
     * @param zn 入力レジスタ1
     * @param zm 入力レジスタ2
     * @param pg プレディケートレジスタ
     * @param elementSize 要素サイズ
     */
    static void emitVectorMulAdd(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize);
    
    /**
     * @brief SVE水平加算
     * 
     * @param out 出力バッファ
     * @param vd 出力スカラーレジスタ
     * @param pg プレディケートレジスタ
     * @param zn 入力ベクトルレジスタ
     * @param elementSize 要素サイズ
     */
    static void emitHorizontalAdd(std::vector<uint8_t>& out, int vd, int pg, int zn, ElementSize elementSize);
    
    /**
     * @brief SVEゼロ初期化
     * 
     * @param out 出力バッファ
     * @param zd 初期化するレジスタ
     */
    static void emitClearVector(std::vector<uint8_t>& out, int zd);
    
    /**
     * @brief SVEスカラー値からブロードキャスト
     * 
     * @param out 出力バッファ
     * @param zd 出力レジスタ
     * @param vn 入力スカラーレジスタ
     * @param pg プレディケートレジスタ
     * @param elementSize 要素サイズ
     */
    static void emitBroadcast(std::vector<uint8_t>& out, int zd, int pg, int vn, ElementSize elementSize);
    
    /**
     * @brief SVEレジスタ間コピー
     * 
     * @param out 出力バッファ
     * @param zd 出力レジスタ
     * @param pg プレディケートレジスタ
     * @param zn 入力レジスタ
     */
    static void emitMove(std::vector<uint8_t>& out, int zd, int pg, int zn);
    
    /**
     * @brief SVEベクタ比較（等しい）
     * 
     * @param out 出力バッファ
     * @param pd 出力プレディケートレジスタ
     * @param pg プレディケートレジスタ
     * @param zn 入力レジスタ1
     * @param zm 入力レジスタ2
     * @param elementSize 要素サイズ
     */
    static void emitVectorCompareEQ(std::vector<uint8_t>& out, int pd, int pg, int zn, int zm, ElementSize elementSize);
    
    /**
     * @brief SVEベクタ比較（より大きい）
     * 
     * @param out 出力バッファ
     * @param pd 出力プレディケートレジスタ
     * @param pg プレディケートレジスタ
     * @param zn 入力レジスタ1
     * @param zm 入力レジスタ2
     * @param elementSize 要素サイズ
     */
    static void emitVectorCompareGT(std::vector<uint8_t>& out, int pd, int pg, int zn, int zm, ElementSize elementSize);
    
    /**
     * @brief SVEベクタ最大値
     * 
     * @param out 出力バッファ
     * @param zd 出力レジスタ
     * @param pg プレディケートレジスタ
     * @param zn 入力レジスタ1
     * @param zm 入力レジスタ2
     * @param elementSize 要素サイズ
     */
    static void emitVectorMax(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize);
    
    /**
     * @brief SVEベクタ最小値
     * 
     * @param out 出力バッファ
     * @param zd 出力レジスタ
     * @param pg プレディケートレジスタ
     * @param zn 入力レジスタ1
     * @param zm 入力レジスタ2
     * @param elementSize 要素サイズ
     */
    static void emitVectorMin(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize);
    
    /**
     * @brief SVEを使用した行列乗算の実装
     * 
     * @param out 出力バッファ
     * @param rows 行数
     * @param cols 列数
     * @param shared 共有次元サイズ
     */
    static void emitMatrixMultiply(std::vector<uint8_t>& out, int rows, int cols, int shared);
    
    /**
     * @brief 特定ループをSVEで自動ベクトル化
     * 
     * @param loopInsts ループ中の命令列
     * @param out 出力バッファ
     * @return ベクトル化成功の場合true
     */
    static bool autoVectorizeLoop(const std::vector<IRInstruction>& loopInsts, std::vector<uint8_t>& out);
    
    /**
     * @brief SVEを使用した数値積分の実装
     * 
     * @param out 出力バッファ
     * @param dataReg データレジスタ
     * @param resultReg 結果レジスタ
     * @param length データ長
     */
    static void emitNumericalIntegration(std::vector<uint8_t>& out, int dataReg, int resultReg, int length);
    
    /**
     * @brief SVEのベクトル長を取得
     * 
     * @param out 出力バッファ
     * @param xd 結果を格納する汎用レジスタ
     * @param elementSize 要素サイズ
     */
    static void emitGetVectorLength(std::vector<uint8_t>& out, int xd, ElementSize elementSize);
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_BACKEND_ARM64_SVE_H 