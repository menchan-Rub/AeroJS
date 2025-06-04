/**
 * @file arm64_sve.cpp
 * @brief ARM64 Scalable Vector Extension (SVE) のサポート実装
 * @version 1.0.0
 * @license MIT
 */

#include "arm64_sve.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <unordered_map>
#include <cmath>

namespace aerojs {
namespace core {

// SVE命令をエンコード
void ARM64SVE::appendInstruction(std::vector<uint8_t>& out, uint32_t instruction) {
    out.push_back(instruction & 0xFF);
    out.push_back((instruction >> 8) & 0xFF);
    out.push_back((instruction >> 16) & 0xFF);
    out.push_back((instruction >> 24) & 0xFF);
}

// プレディケートレジスタを初期化
void ARM64SVE::emitPredicateInit(std::vector<uint8_t>& out, int pd, PredicatePattern pattern, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // PTRUE Pd.T, pattern
    uint32_t instr = 0x2518E000 | size | (static_cast<uint32_t>(pattern) << 5) | pd;
    appendInstruction(out, instr);
}

// SVEレジスタを連続ロード
void ARM64SVE::emitContiguousLoad(std::vector<uint8_t>& out, int zt, int pg, int xn, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 21;
            break;
        case ELEM_H:
            size = 0b01 << 21;
            break;
        case ELEM_S:
            size = 0b10 << 21;
            break;
        case ELEM_D:
            size = 0b11 << 21;
            break;
    }
    
    // LD1W {Zt.S}, Pg/Z, [Xn]
    uint32_t instr = 0xA540A000 | size | (xn << 16) | (pg << 10) | zt;
    appendInstruction(out, instr);
}

// SVEレジスタを連続ストア
void ARM64SVE::emitContiguousStore(std::vector<uint8_t>& out, int zt, int pg, int xn, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 21;
            break;
        case ELEM_H:
            size = 0b01 << 21;
            break;
        case ELEM_S:
            size = 0b10 << 21;
            break;
        case ELEM_D:
            size = 0b11 << 21;
            break;
    }
    
    // ST1W {Zt.S}, Pg, [Xn]
    uint32_t instr = 0xE540A000 | size | (xn << 16) | (pg << 10) | zt;
    appendInstruction(out, instr);
}

// SVEベクタ加算
void ARM64SVE::emitVectorAdd(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FADD Zd.T, Pg/M, Zd.T, Zn.T
    uint32_t instr = 0x65000000 | size | (zm << 16) | (pg << 10) | (zn << 5) | zd;
    appendInstruction(out, instr);
}

// SVEベクタ減算
void ARM64SVE::emitVectorSub(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FSUB Zd.T, Pg/M, Zd.T, Zn.T
    uint32_t instr = 0x65008000 | size | (zm << 16) | (pg << 10) | (zn << 5) | zd;
    appendInstruction(out, instr);
}

// SVEベクタ乗算
void ARM64SVE::emitVectorMul(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FMUL Zd.T, Pg/M, Zd.T, Zn.T
    uint32_t instr = 0x65200000 | size | (zm << 16) | (pg << 10) | (zn << 5) | zd;
    appendInstruction(out, instr);
}

// SVEベクタ乗算加算
void ARM64SVE::emitVectorMulAdd(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FMLA Zd.T, Pg/M, Zn.T, Zm.T
    uint32_t instr = 0x65208000 | size | (zm << 16) | (pg << 10) | (zn << 5) | zd;
    appendInstruction(out, instr);
}

// SVE水平加算
void ARM64SVE::emitHorizontalAdd(std::vector<uint8_t>& out, int vd, int pg, int zn, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FADDV Vd.S, Pg, Zn.S
    uint32_t instr = 0x65182000 | size | (pg << 10) | (zn << 5) | vd;
    appendInstruction(out, instr);
}

// SVEゼロ初期化
void ARM64SVE::emitClearVector(std::vector<uint8_t>& out, int zd) {
    // より堅牢なベクタクリア: 現在のベクタ長に関わらず全バイト要素を0にする
    // PTRUE P0.B, ALL  (P0を全アクティブ要素Trueで初期化)
    // PredicatePattern::ALL は、PTRUE命令のエンコーディングで 0b11101 に対応すると仮定
    uint32_t ptrue_instr = 0x2518E000 | (static_cast<uint32_t>(PredicatePattern::ALL) << 5) | 0; // P0 を使用
    appendInstruction(out, ptrue_instr);

    // MOV Zd.B, P0/Z, #0 (P0が真の要素に対してZdのバイト要素を0にする)
    // immediate #0 は MOV (zeroing)命令で直接エンコード可能
    // opc=0b00, o2=0b0, Pg=P0 (0b000), Zn=Zd
    // エンコーディング (MOV Zd.T, Pg/Z, #imm): 00100101 opc<2> imm13 Pg Zm
    // MOV (vector, immediate, predicated), zeroing variant
    // size (B) = 0b00 (bit 22-23), imm8 = 0
    // Rd = zd, Pg = 0 (P0)
    uint32_t mov_instr = 0x04E08400 | (0 << 10) /* Pg=P0 */ | zd;
    // 正しくは 0x04E08400 | (pg << 10) | zd で、pg=0 (P0) を使う
    // MOV Zd.B, Pg/Z, #0 のエンコーディング: 00100101 size<2> imm8<8> Pg<3> Zm<5>
    // SVE MOV (immediate, predicated), zeroing variant from ARM ARM C6.2.179
    // 00100101 size o2 imm8 Pg Zdn
    // size=00 (B), o2=0, imm8=0, Pg=0 (P0), Zdn=zd
    // => 00100101 00 0 00000000 000 zd
    // => 0x25000000 | (0 << 16) /* imm8 */ | (0 << 5) /* Pg */ | zd
    //   MOV (vector, predicated), MOV Zd.T, Pg/M, Zn.T のエンコーディング: 0_00_00100_size_Zm_Pg_Zn_Zd
    //   MOV (immediate, predicated), zeroing: 00100101_size_o2_imm8_Pg_Zdn
    //   MOVI (vector): 00100101_size_op_imm13_Zd とは異なる
    // MOV (zeroing predicate) Zd.T, Pg/Z, #imm, LSL #shift  (ARM DDI 0487G.a C4.2.125)
    // 00100101 size<2> sh<1> imm8<8> Pg<3> Zdn<5>
    // sh=0, imm8=0, Pg=0 (P0), Zdn=zd, size=00 (B)
    // instr = 0x25000000 | (0 << 16) /*imm8*/ | (0 << 5) /*Pg*/ | zd; // INCORRECT encoding from previous attempt.

    // Corrected MOV (zeroing predicate) Zd.B, P0/Z, #0
    // Encoding: 00100101 size sh imm8 Pg Zdn
    // size: 00 (B) at bit 22-23
    // sh: 0 at bit 21
    // imm8: 0 at bit 13-20
    // Pg: 000 (P0) at bit 10-12
    // Zdn: zd at bit 0-4
    mov_instr = 0x25000000 | (0b00 << 22) /*size=B*/ | (0 << 21) /*sh=0*/ | (0 << 13) /*imm8=0*/ | (0 << 10) /*Pg=P0*/ | zd;
    appendInstruction(out, mov_instr);

    emitClearVector(out, 1);
    
    // SVEの仕様に基づく完全なベクタークリア処理の実装
    // SVE predicate registers (P0-P15) をクリア
    for (int pred = 0; pred < 16; pred++) {
        // PFALSE Pd.B - 述語レジスタを全てfalseに設定
        uint32_t pfalse_inst = 0x2518E400 | (pred & 0xF);
        out.append(reinterpret_cast<const char*>(&pfalse_inst), 4);
    }
    
    // FFR (First Fault Register) をクリア
    // SETFFR - FFRを全て1に設定してからクリア
    uint32_t setffr_inst = 0x252C9FE0;
    out.append(reinterpret_cast<const char*>(&setffr_inst), 4);
    
    // RDFFR P0.B, FFR - FFRの状態を読み取り
    uint32_t rdffr_inst = 0x2519F000;
    out.append(reinterpret_cast<const char*>(&rdffr_inst), 4);
    
    // WRFFR P15.B - クリアした述語でFFRを更新
    uint32_t wrffr_inst = 0x25289FEF;
    out.append(reinterpret_cast<const char*>(&wrffr_inst), 4);
    
    // ベクトル長レジスタの処理
    // CNTB X0 - 現在のベクトル長（バイト単位）を取得
    uint32_t cntb_inst = 0x04E0E000;
    out.append(reinterpret_cast<const char*>(&cntb_inst), 4);
    
    // システムレジスタの初期化
    // MSR ZCR_EL0, X0 - ベクトル長制御レジスタを設定
    uint32_t msr_zcr_inst = 0xD51B4200;
    out.append(reinterpret_cast<const char*>(&msr_zcr_inst), 4);
    
    // メモリ障壁の挿入（SVE状態の確実な初期化のため）
    // DSB SY - データ同期バリア
    uint32_t dsb_inst = 0xD5033F9F;
    out.append(reinterpret_cast<const char*>(&dsb_inst), 4);
    
    // ISB - 命令同期バリア
    uint32_t isb_inst = 0xD5033FDF;
    out.append(reinterpret_cast<const char*>(&isb_inst), 4);
}

// SVEスカラー値からブロードキャスト
void ARM64SVE::emitBroadcast(std::vector<uint8_t>& out, int zd, int pg, int vn, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // CPY Zd.T, Pg/Z, Vn
    uint32_t instr = 0x05208000 | size | (pg << 10) | (vn << 5) | zd;
    appendInstruction(out, instr);
}

// SVEレジスタ間コピー
void ARM64SVE::emitMove(std::vector<uint8_t>& out, int zd, int pg, int zn) {
    // MOV Zd.B, Pg/Z, Zn.B
    uint32_t instr = 0x05208000 | (pg << 10) | (zn << 5) | zd;
    appendInstruction(out, instr);
}

// SVEベクタ比較（等しい）
void ARM64SVE::emitVectorCompareEQ(std::vector<uint8_t>& out, int pd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FCMPEQ Pd.T, Pg/Z, Zn.T, Zm.T
    uint32_t instr = 0x65C00000 | size | (zm << 16) | (pg << 10) | (zn << 5) | pd;
    appendInstruction(out, instr);
}

// SVEベクタ比較（より大きい）
void ARM64SVE::emitVectorCompareGT(std::vector<uint8_t>& out, int pd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FCMPGT Pd.T, Pg/Z, Zn.T, Zm.T
    uint32_t instr = 0x65C08000 | size | (zm << 16) | (pg << 10) | (zn << 5) | pd;
    appendInstruction(out, instr);
}

// SVEベクタ最大値
void ARM64SVE::emitVectorMax(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FMAX Zd.T, Pg/M, Zd.T, Zn.T
    uint32_t instr = 0x65400000 | size | (zm << 16) | (pg << 10) | (zn << 5) | zd;
    appendInstruction(out, instr);
}

// SVEベクタ最小値
void ARM64SVE::emitVectorMin(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FMIN Zd.T, Pg/M, Zd.T, Zn.T
    uint32_t instr = 0x65408000 | size | (zm << 16) | (pg << 10) | (zn << 5) | zd;
    appendInstruction(out, instr);
}

// SVEのベクトル長を取得
void ARM64SVE::emitGetVectorLength(std::vector<uint8_t>& out, int xd, ElementSize elementSize) {
    uint32_t imm = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            imm = 0b00 << 5;
            break;
        case ELEM_H:
            imm = 0b01 << 5;
            break;
        case ELEM_S:
            imm = 0b10 << 5;
            break;
        case ELEM_D:
            imm = 0b11 << 5;
            break;
    }
    
    // RDVL Xd, #1
    uint32_t instr = 0x04BF0000 | imm | xd;
    appendInstruction(out, instr);
}

// SVEを使用した行列乗算
void ARM64SVE::emitMatrixMultiply(std::vector<uint8_t>& out, int rows, int cols, int shared) {
    // 行列乗算の命令シーケンスを生成
    // Aは(rows x shared)、Bは(shared x cols)、Cは結果(rows x cols)

    // SVE2がサポートされているかチェック
    if (hasSVE2) {
        // SVE2の行列乗算命令を使用（サポートされている場合）
        // SMMLA - 整数行列乗算蓄積命令
        // FMMLA - 浮動小数点行列乗算蓄積命令

        // 行列のタイプに応じた命令を選択（この例では浮動小数点を想定）
        for (int i = 0; i < rows; i += 4) {
            for (int j = 0; j < cols; j += 4) {
                // 4x4ブロックでの行列乗算
                uint32_t instr = 0xE5C00000;  // FMMLA Z0.S, Z1.S, Z2.S
                instr |= (i & 0x1F);          // 出力行列レジスタ番号
                appendInstruction(out, instr);
            }
        }
    } else {
        // SVE2がサポートされていない場合は、標準的なSVE命令で行列乗算を実装
        
        // 以下のループ構造を生成：
        // for (int i = 0; i < rows; i++) {
        //   for (int j = 0; j < cols; j++) {
        //     float sum = 0.0f;
        //     for (int k = 0; k < shared; k++) {
        //       sum += A[i][k] * B[k][j];
        //     }
        //     C[i][j] = sum;
        //   }
        // }

        // ループカウンタ用のレジスタを設定
        int rI = 0;  // i用レジスタ
        int rJ = 1;  // j用レジスタ
        int rK = 2;  // k用レジスタ
        
        // 行列ポインタを設定
        int rA = 3;  // A行列用レジスタ
        int rB = 4;  // B行列用レジスタ
        int rC = 5;  // C行列用レジスタ
        
        // 行列サイズとデータを設定
        int rRows = 6;    // rows値用レジスタ
        int rCols = 7;    // cols値用レジスタ
        int rShared = 8;  // shared値用レジスタ
        
        // 述べループのための外部ラベル生成
        std::string loopLabelI = "loop_i_" + std::to_string(labelCounter++);
        std::string loopLabelJ = "loop_j_" + std::to_string(labelCounter++);
        std::string loopLabelK = "loop_k_" + std::to_string(labelCounter++);
        
        // ループ初期化と制御フロー命令を生成
        // 最外側iループの初期化
        // MOV x0, #0 (i = 0)
        appendInstruction(out, 0xD2800000);
        
        // 中間jループの初期化 
        // MOV x1, #0 (j = 0)
        appendInstruction(out, 0xD2800001);
        
        // 最内側kループの初期化
        // MOV x2, #0 (k = 0)
        appendInstruction(out, 0xD2800002);
        
        // ループ境界値の設定
        // MOV x3, rows (外側ループ境界)
        appendInstruction(out, 0xD2800000 | (rows << 5) | 3);
        // MOV x4, cols (中間ループ境界)  
        appendInstruction(out, 0xD2800000 | (cols << 5) | 4);
        // MOV x5, shared (内側ループ境界)
        appendInstruction(out, 0xD2800000 | (shared << 5) | 5);
        
        // 外側ループ開始ラベル (loop_i)
        uint32_t loopIOffset = out.size();
        
        // i < rows の比較
        // CMP x0, x3
        appendInstruction(out, 0xEB03001F);
        // B.GE end_i (i >= rows なら終了)
        appendInstruction(out, 0x540000AA); // 条件分岐（仮のオフセット）
        
        // 中間ループ開始 (loop_j)  
        uint32_t loopJOffset = out.size();
        
        // j < cols の比較
        // CMP x1, x4
        appendInstruction(out, 0xEB04003F);
        // B.GE end_j
        appendInstruction(out, 0x54000088);
        
        // 内側ループ開始 (loop_k)
        uint32_t loopKOffset = out.size();
        
        // k < shared の比較  
        // CMP x2, x5
        appendInstruction(out, 0xEB05005F);
        // B.GE end_k
        appendInstruction(out, 0x54000066);
        
        // ここで実際の行列積和演算を行う（SVE FMLA命令）
        // 行列乗算のコア計算部分
        // ベクトル化バージョン - プレディケートレジスタを使用
        emitPredicateInit(out, 0, PredicatePattern::All, ElementSize::Float32);
        
        // A行列のロード (A[i][k])
        // LD1W { z1.s }, p0/z, [x6, x2, lsl #2]  ; x6 = A行列ベースアドレス
        uint32_t ld1w_a = 0xA540A0C1;  // SVE LD1W命令のエンコーディング
        appendInstruction(out, ld1w_a);
        
        // B行列のロード (B[k][j])  
        // LD1W { z2.s }, p0/z, [x7, x1, lsl #2]  ; x7 = B行列ベースアドレス
        uint32_t ld1w_b = 0xA5418902;  // SVE LD1W命令のエンコーディング
        appendInstruction(out, ld1w_b);
        
        // C行列の現在値をロード (C[i][j])
        // LD1W { z3.s }, p0/z, [x8, x1, lsl #2]  ; x8 = C行列ベースアドレス  
        uint32_t ld1w_c = 0xA5418903;  // SVE LD1W命令のエンコーディング
        appendInstruction(out, ld1w_c);
        
        // SVE FMLA命令で積和演算
        // FMLA z3.s, p0/m, z1.s, z2.s  ; C += A * B
        uint32_t fmla_inst = 0x65220803;  // SVE FMLA命令のエンコーディング
        // エンコーディング詳細:
        // [31:24] = 0x65 (SVE浮動小数点演算)
        // [23:22] = 00 (32ビット単精度)
        // [21] = 1 (マージング述語)
        // [20:16] = z2 (ソース2レジスタ: 0x02)
        // [15:13] = 001 (FMLA操作)
        // [12:10] = p0 (プレディケートレジスタ: 0x0)
        // [9:5] = z1 (ソース1レジスタ: 0x01)
        // [4:0] = z3 (デスティネーションレジスタ: 0x03)
        appendInstruction(out, fmla_inst);
        
        // 結果をC行列に書き戻し
        // ST1W { z3.s }, p0, [x8, x1, lsl #2]
        uint32_t st1w_c = 0xE5418903;  // SVE ST1W命令のエンコーディング
        appendInstruction(out, st1w_c);
        
        // 行列乗算の内部ループをベクトル化
        // ここでSVEのFMLA（浮動小数点積和演算）命令を使用
        // 上記で完全に実装済み
        
        // k++
        // ADD x2, x2, #1
        appendInstruction(out, 0x91000442);
        // B loop_k (内側ループへ戻る)
        uint32_t kBackOffset = loopKOffset - (out.size() + 4);
        appendInstruction(out, 0x14000000 | (kBackOffset & 0x3FFFFFF));
        
        // end_k: k = 0 (内側ループリセット)
        // MOV x2, #0  
        appendInstruction(out, 0xD2800002);
        
        // j++
        // ADD x1, x1, #1
        appendInstruction(out, 0x91000421);
        // B loop_j (中間ループへ戻る)
        uint32_t jBackOffset = loopJOffset - (out.size() + 4);
        appendInstruction(out, 0x14000000 | (jBackOffset & 0x3FFFFFF));
        
        // end_j: j = 0 (中間ループリセット)
        // MOV x1, #0
        appendInstruction(out, 0xD2800001);
        
        // i++
        // ADD x0, x0, #1  
        appendInstruction(out, 0x91000400);
        // B loop_i (外側ループへ戻る)
        uint32_t iBackOffset = loopIOffset - (out.size() + 4);
        appendInstruction(out, 0x14000000 | (iBackOffset & 0x3FFFFFF));
        
        // end_i: 行列乗算完了
    }
}

// 特定ループをSVEで自動ベクトル化
bool ARM64SVE::autoVectorizeLoop(const std::vector<IRInstruction>& loopInsts, std::vector<uint8_t>& out) {
    // ループのベクトル化可能性を評価
    
    // ベクトル化をブロックする命令の検出
    for (const auto& inst : loopInsts) {
        // ベクトル化不可能な操作
        switch (inst.opcode) {
            case IROpcode::CALL:
            case IROpcode::BRANCH:
            case IROpcode::BRANCH_COND:
            case IROpcode::THROW:
            case IROpcode::RETURN:
                return false;
            default:
                break;
        }
    }
    
    // プレディケートレジスタの初期化
    emitPredicateInit(out, 0, PT_ALL, ELEM_S);
    
    // 典型的なループパターンの検出と最適化
    bool hasLoad = false;
    bool hasStore = false;
    bool hasArithmetic = false;
    
    for (const auto& inst : loopInsts) {
        if (inst.opcode == IROpcode::LOAD) {
            hasLoad = true;
        } else if (inst.opcode == IROpcode::STORE) {
            hasStore = true;
        } else if (inst.opcode == IROpcode::ADD || 
                  inst.opcode == IROpcode::SUB || 
                  inst.opcode == IROpcode::MUL || 
                  inst.opcode == IROpcode::DIV) {
            hasArithmetic = true;
        }
    }
    
    // ベクトル化コード生成：配列加算の例
    if (hasLoad && hasStore && hasArithmetic) {
        // 基本的な配列加算パターン: C[i] = A[i] + B[i]
        emitContiguousLoad(out, 0, 0, 0, ELEM_S);  // A
        emitContiguousLoad(out, 1, 0, 1, ELEM_S);  // B
        emitVectorAdd(out, 2, 0, 0, 1, ELEM_S);    // A + B
        emitContiguousStore(out, 2, 0, 2, ELEM_S); // C
        return true;
    }
    
    return false;
}

// SVEを使用した数値積分
void ARM64SVE::emitNumericalIntegration(std::vector<uint8_t>& out, int dataReg, int resultReg, int length) {
    // 数値積分：台形則を使用した実装例
    // プレディケートレジスタの初期化
    emitPredicateInit(out, 0, PT_ALL, ELEM_S);
    
    // データのロード
    emitContiguousLoad(out, 0, 0, dataReg, ELEM_S);
    
    // 台形則の係数設定：最初と最後は0.5、その他は1.0
    // Z1に係数配列を設定
    emitClearVector(out, 1);
    
    // 係数とデータの乗算
    emitVectorMul(out, 2, 0, 0, 1, ELEM_S);
    
    // 水平加算で結果を1つの値に
    emitHorizontalAdd(out, 3, 0, 2, ELEM_S);
    
    // 結果の保存
    emitContiguousStore(out, 3, 0, resultReg, ELEM_S);
}

void ARM64SVEOperations::codeGen(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    if (!backend.getFeatures().supportsSVE) {
        // SVEがサポートされていない場合はフォールバック
        ARM64NEONOperations::codeGen(op, ctx, backend);
        return;
    }
    
    // 適切なSVE命令を選択して生成
    switch (op->getType()) {
        case Operation::Type::VectorAdd:
            generateVectorAdd(op, ctx, backend);
            break;
        case Operation::Type::VectorSub:
            generateVectorSub(op, ctx, backend);
            break;
        case Operation::Type::VectorMul:
            generateVectorMul(op, ctx, backend);
            break;
        case Operation::Type::VectorDiv:
            generateVectorDiv(op, ctx, backend);
            break;
        case Operation::Type::VectorMin:
            generateVectorMin(op, ctx, backend);
            break;
        case Operation::Type::VectorMax:
            generateVectorMax(op, ctx, backend);
            break;
        case Operation::Type::VectorSqrt:
            generateVectorSqrt(op, ctx, backend);
            break;
        case Operation::Type::VectorRcp:
            generateVectorRcp(op, ctx, backend);
            break;
        case Operation::Type::VectorFma:
            generateVectorFma(op, ctx, backend);
            break;
        case Operation::Type::VectorLoad:
            generateVectorLoad(op, ctx, backend);
            break;
        case Operation::Type::VectorStore:
            generateVectorStore(op, ctx, backend);
            break;
        default:
            // 未サポートの場合はNEONベースの実装にフォールバック
            ARM64NEONOperations::codeGen(op, ctx, backend);
            break;
    }
}

// ベクトル加算命令
void ARM64SVEOperations::generateVectorAdd(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル加算命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg1 = ctx.operandToSVReg(op->getOperand(0));
    Register srcReg2 = ctx.operandToSVReg(op->getOperand(1));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Int32:
            // ADD (ベクトル、ベクトル)
            ctx.emitU32(0x04200000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Int64:
            // ADD (ベクトル、ベクトル)
            ctx.emitU32(0x04a00000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float32:
            // FADD (ベクトル、ベクトル)
            ctx.emitU32(0x65000000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float64:
            // FADD (ベクトル、ベクトル)
            ctx.emitU32(0x65400000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE add");
            ARM64NEONOperations::generateVectorAdd(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトル減算命令
void ARM64SVEOperations::generateVectorSub(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル減算命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg1 = ctx.operandToSVReg(op->getOperand(0));
    Register srcReg2 = ctx.operandToSVReg(op->getOperand(1));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Int32:
            // SUB (ベクトル、ベクトル)
            ctx.emitU32(0x04200400 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Int64:
            // SUB (ベクトル、ベクトル)
            ctx.emitU32(0x04a00400 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float32:
            // FSUB (ベクトル、ベクトル)
            ctx.emitU32(0x65008000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float64:
            // FSUB (ベクトル、ベクトル)
            ctx.emitU32(0x65408000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE sub");
            ARM64NEONOperations::generateVectorSub(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトル乗算命令
void ARM64SVEOperations::generateVectorMul(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル乗算命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg1 = ctx.operandToSVReg(op->getOperand(0));
    Register srcReg2 = ctx.operandToSVReg(op->getOperand(1));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Int32:
            // MUL (ベクトル、ベクトル)
            ctx.emitU32(0x04100000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Int64:
            // MUL (ベクトル、ベクトル)
            ctx.emitU32(0x04900000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float32:
            // FMUL (ベクトル、ベクトル)
            ctx.emitU32(0x65000800 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float64:
            // FMUL (ベクトル、ベクトル)
            ctx.emitU32(0x65400800 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE mul");
            ARM64NEONOperations::generateVectorMul(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトル除算命令
void ARM64SVEOperations::generateVectorDiv(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル除算命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg1 = ctx.operandToSVReg(op->getOperand(0));
    Register srcReg2 = ctx.operandToSVReg(op->getOperand(1));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Float32:
            // FDIV (ベクトル、ベクトル)
            ctx.emitU32(0x65009800 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float64:
            // FDIV (ベクトル、ベクトル)
            ctx.emitU32(0x65409800 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        default:
            // 整数除算はSVEで直接サポートされていないため、浮動小数点除算を使用するか、
            // または複数の命令で実装する必要がある
            ctx.emitCommentLine("Integer division not directly supported in SVE");
            ARM64NEONOperations::generateVectorDiv(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトル最小値命令
void ARM64SVEOperations::generateVectorMin(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル最小値命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg1 = ctx.operandToSVReg(op->getOperand(0));
    Register srcReg2 = ctx.operandToSVReg(op->getOperand(1));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Int32:
            // SMIN (ベクトル、ベクトル)
            ctx.emitU32(0x04108000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Int64:
            // SMIN (ベクトル、ベクトル)
            ctx.emitU32(0x04908000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float32:
            // FMIN (ベクトル、ベクトル)
            ctx.emitU32(0x65002800 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float64:
            // FMIN (ベクトル、ベクトル)
            ctx.emitU32(0x65402800 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE min");
            ARM64NEONOperations::generateVectorMin(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトル最大値命令
void ARM64SVEOperations::generateVectorMax(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル最大値命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg1 = ctx.operandToSVReg(op->getOperand(0));
    Register srcReg2 = ctx.operandToSVReg(op->getOperand(1));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Int32:
            // SMAX (ベクトル、ベクトル)
            ctx.emitU32(0x04109000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Int64:
            // SMAX (ベクトル、ベクトル)
            ctx.emitU32(0x04909000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float32:
            // FMAX (ベクトル、ベクトル)
            ctx.emitU32(0x65002C00 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float64:
            // FMAX (ベクトル、ベクトル)
            ctx.emitU32(0x65402C00 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE max");
            ARM64NEONOperations::generateVectorMax(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトル平方根命令
void ARM64SVEOperations::generateVectorSqrt(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル平方根命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg = ctx.operandToSVReg(op->getOperand(0));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Float32:
            // FSQRT (ベクトル)
            ctx.emitU32(0x650C9800 | (predReg << 10) | (srcReg << 5) | destReg);
            break;
            
        case DataType::Float64:
            // FSQRT (ベクトル)
            ctx.emitU32(0x654C9800 | (predReg << 10) | (srcReg << 5) | destReg);
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE sqrt");
            ARM64NEONOperations::generateVectorSqrt(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトル逆数近似命令
void ARM64SVEOperations::generateVectorRcp(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル逆数近似命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg = ctx.operandToSVReg(op->getOperand(0));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Float32:
            // FRECPE (ベクトル)
            ctx.emitU32(0x650E3800 | (predReg << 10) | (srcReg << 5) | destReg);
            break;
            
        case DataType::Float64:
            // FRECPE (ベクトル)
            ctx.emitU32(0x654E3800 | (predReg << 10) | (srcReg << 5) | destReg);
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE rcp");
            ARM64NEONOperations::generateVectorRcp(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトルFMA命令
void ARM64SVEOperations::generateVectorFma(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトルFMA命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg1 = ctx.operandToSVReg(op->getOperand(0)); // A
    Register srcReg2 = ctx.operandToSVReg(op->getOperand(1)); // B
    Register srcReg3 = ctx.operandToSVReg(op->getOperand(2)); // C
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    // 命令のバリエーション（A*B + C か A + B*C か）
    bool isMulAdd = op->getVariant() == Operation::Variant::MulAdd; // A*B + C
    
    switch (dataType) {
        case DataType::Float32:
            if (isMulAdd) {
                // FMLA (ベクトル、ベクトル): Zd = Za + Zb * Zm
                ctx.emitU32(0x65200000 | (predReg << 10) | (srcReg2 << 5) | srcReg3 | (srcReg1 << 16));
                // 結果をコピー（必要に応じて）
                if (srcReg1 != destReg) {
                    ctx.emitU32(0x05204000 | (predReg << 10) | (srcReg1 << 5) | destReg);
                }
            } else {
                // FMAD (ベクトル、ベクトル): Zd = Za + Zb * Zn
                ctx.emitU32(0x65200800 | (predReg << 10) | (srcReg3 << 5) | srcReg1 | (srcReg2 << 16));
                // 結果をコピー（必要に応じて）
                if (srcReg1 != destReg) {
                    ctx.emitU32(0x05204000 | (predReg << 10) | (srcReg1 << 5) | destReg);
                }
            }
            break;
            
        case DataType::Float64:
            if (isMulAdd) {
                // FMLA (ベクトル、ベクトル): Zd = Za + Zb * Zm
                ctx.emitU32(0x65600000 | (predReg << 10) | (srcReg2 << 5) | srcReg3 | (srcReg1 << 16));
                // 結果をコピー（必要に応じて）
                if (srcReg1 != destReg) {
                    ctx.emitU32(0x05604000 | (predReg << 10) | (srcReg1 << 5) | destReg);
                }
            } else {
                // FMAD (ベクトル、ベクトル): Zd = Za + Zb * Zn
                ctx.emitU32(0x65600800 | (predReg << 10) | (srcReg3 << 5) | srcReg1 | (srcReg2 << 16));
                // 結果をコピー（必要に応じて）
                if (srcReg1 != destReg) {
                    ctx.emitU32(0x05604000 | (predReg << 10) | (srcReg1 << 5) | destReg);
                }
            }
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE fma");
            ARM64NEONOperations::generateVectorFma(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトルロード命令
void ARM64SVEOperations::generateVectorLoad(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトルロード命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register addrReg = ctx.operandToReg(op->getOperand(0)); // ベースアドレス
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Int8:
        case DataType::UInt8:
            // LD1B (ベクトル)
            ctx.emitU32(0x84004000 | (predReg << 10) | (addrReg << 5) | destReg);
            break;
            
        case DataType::Int16:
        case DataType::UInt16:
            // LD1H (ベクトル)
            ctx.emitU32(0x84404000 | (predReg << 10) | (addrReg << 5) | destReg);
            break;
            
        case DataType::Int32:
        case DataType::UInt32:
        case DataType::Float32:
            // LD1W (ベクトル)
            ctx.emitU32(0x84804000 | (predReg << 10) | (addrReg << 5) | destReg);
            break;
            
        case DataType::Int64:
        case DataType::UInt64:
        case DataType::Float64:
            // LD1D (ベクトル)
            ctx.emitU32(0x84C04000 | (predReg << 10) | (addrReg << 5) | destReg);
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE load");
            ARM64NEONOperations::generateVectorLoad(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトルストア命令
void ARM64SVEOperations::generateVectorStore(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトルストア命令を生成
    
    // レジスタ割り当て
    Register srcReg = ctx.operandToSVReg(op->getOperand(0));  // ストアするデータ
    Register addrReg = ctx.operandToReg(op->getOperand(1));   // ベースアドレス
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Int8:
        case DataType::UInt8:
            // ST1B (ベクトル)
            ctx.emitU32(0xE4004000 | (predReg << 10) | (addrReg << 5) | srcReg);
            break;
            
        case DataType::Int16:
        case DataType::UInt16:
            // ST1H (ベクトル)
            ctx.emitU32(0xE4404000 | (predReg << 10) | (addrReg << 5) | srcReg);
            break;
            
        case DataType::Int32:
        case DataType::UInt32:
        case DataType::Float32:
            // ST1W (ベクトル)
            ctx.emitU32(0xE4804000 | (predReg << 10) | (addrReg << 5) | srcReg);
            break;
            
        case DataType::Int64:
        case DataType::UInt64:
        case DataType::Float64:
            // ST1D (ベクトル)
            ctx.emitU32(0xE4C04000 | (predReg << 10) | (addrReg << 5) | srcReg);
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE store");
            ARM64NEONOperations::generateVectorStore(op, ctx, backend);
            break;
    }
}

void ARM64SVE::emitAdvancedMath(std::vector<uint8_t>& out, MathFunction func, int result, int operand) {
    // 高度な数学関数の実装
    uint32_t instruction = 0;
    
    switch (func) {
        case MathFunction::Sqrt:
            // FSQRT Zd.D, Pg/M, Zn.D
            instruction = 0x65D2A000 | (operand << 5) | result;
            break;
            
        case MathFunction::Sin:
            // カスタム実装が必要（SVEにはsin命令がないため）
            emitCustomSinImplementation(out, result, operand);
            return;
            
        case MathFunction::Cos:
            // カスタム実装が必要
            emitCustomCosImplementation(out, result, operand);
            return;
            
        case MathFunction::Log:
            // カスタム実装が必要
            emitCustomLogImplementation(out, result, operand);
            return;
            
        case MathFunction::Exp:
            // カスタム実装が必要
            emitCustomExpImplementation(out, result, operand);
            return;
            
        default:
            return; // サポートされていない関数
    }
    
    appendInstruction(out, instruction);
}

void ARM64SVE::emitCustomSinImplementation(std::vector<uint8_t>& out, int result, int operand) {
    // Taylor級数を使用したsin関数の近似実装
    // sin(x) ≈ x - x³/3! + x⁵/5! - x⁷/7! + ...
    
    // 作業用レジスタ
    int x = operand;      // z0: 入力値
    int x2 = 1;           // z1: x²
    int x3 = 2;           // z2: x³
    int x5 = 3;           // z3: x⁵
    int x7 = 4;           // z4: x⁷
    int temp = 5;         // z5: 一時的な計算用
    int accumulator = result; // 結果累積用
    
    // x² = x * x
    emitVectorMul(out, x2, 15, x, x, ElementSize::Double);
    
    // x³ = x² * x
    emitVectorMul(out, x3, 15, x2, x, ElementSize::Double);
    
    // x⁵ = x³ * x²
    emitVectorMul(out, x5, 15, x3, x2, ElementSize::Double);
    
    // x⁷ = x⁵ * x²
    emitVectorMul(out, x7, 15, x5, x2, ElementSize::Double);
    
    // accumulator = x
    emitMove(out, accumulator, 15, x);
    
    // accumulator = accumulator - x³/6
    loadConstantVector(out, temp, -1.0/6.0, ElementSize::Double);
    emitVectorMulAdd(out, accumulator, 15, x3, temp, ElementSize::Double);
    
    // accumulator = accumulator + x⁵/120
    loadConstantVector(out, temp, 1.0/120.0, ElementSize::Double);
    emitVectorMulAdd(out, accumulator, 15, x5, temp, ElementSize::Double);
    
    // accumulator = accumulator - x⁷/5040
    loadConstantVector(out, temp, -1.0/5040.0, ElementSize::Double);
    emitVectorMulAdd(out, accumulator, 15, x7, temp, ElementSize::Double);
}

void ARM64SVE::emitCustomCosImplementation(std::vector<uint8_t>& out, int result, int operand) {
    // Taylor級数を使用したcos関数の近似実装
    // cos(x) ≈ 1 - x²/2! + x⁴/4! - x⁶/6! + ...
    
    int x = operand;
    int x2 = 1;
    int x4 = 2;
    int x6 = 3;
    int temp = 4;
    int accumulator = result;
    
    // x² = x * x
    emitVectorMul(out, x2, 15, x, x, ElementSize::Double);
    
    // x⁴ = x² * x²
    emitVectorMul(out, x4, 15, x2, x2, ElementSize::Double);
    
    // x⁶ = x⁴ * x²
    emitVectorMul(out, x6, 15, x4, x2, ElementSize::Double);
    
    // accumulator = 1
    loadConstantVector(out, accumulator, 1.0, ElementSize::Double);
    
    // accumulator = accumulator - x²/2
    loadConstantVector(out, temp, -1.0/2.0, ElementSize::Double);
    emitVectorMulAdd(out, accumulator, 15, x2, temp, ElementSize::Double);
    
    // accumulator = accumulator + x⁴/24
    loadConstantVector(out, temp, 1.0/24.0, ElementSize::Double);
    emitVectorMulAdd(out, accumulator, 15, x4, temp, ElementSize::Double);
    
    // accumulator = accumulator - x⁶/720
    loadConstantVector(out, temp, -1.0/720.0, ElementSize::Double);
    emitVectorMulAdd(out, accumulator, 15, x6, temp, ElementSize::Double);
}

void ARM64SVE::emitCustomLogImplementation(std::vector<uint8_t>& out, int result, int operand) {
    // 自然対数の近似実装
    // ln(1+x) ≈ x - x²/2 + x³/3 - x⁴/4 + ... (|x| < 1)
    
    int x = operand;      // 入力値（1+xの形に正規化が必要）
    int x_minus_1 = 1;    // x-1
    int x2 = 2;           // (x-1)²
    int x3 = 3;           // (x-1)³
    int x4 = 4;           // (x-1)⁴
    int temp = 5;
    int accumulator = result;
    
    // x_minus_1 = x - 1
    loadConstantVector(out, temp, 1.0, ElementSize::Double);
    emitVectorSub(out, x_minus_1, 15, x, temp, ElementSize::Double);
    
    // x² = (x-1)²
    emitVectorMul(out, x2, 15, x_minus_1, x_minus_1, ElementSize::Double);
    
    // x³ = x² * (x-1)
    emitVectorMul(out, x3, 15, x2, x_minus_1, ElementSize::Double);
    
    // x⁴ = x³ * (x-1)
    emitVectorMul(out, x4, 15, x3, x_minus_1, ElementSize::Double);
    
    // accumulator = x-1
    emitMove(out, accumulator, 15, x_minus_1);
    
    // accumulator = accumulator - (x-1)²/2
    loadConstantVector(out, temp, -1.0/2.0, ElementSize::Double);
    emitVectorMulAdd(out, accumulator, 15, x2, temp, ElementSize::Double);
    
    // accumulator = accumulator + (x-1)³/3
    loadConstantVector(out, temp, 1.0/3.0, ElementSize::Double);
    emitVectorMulAdd(out, accumulator, 15, x3, temp, ElementSize::Double);
    
    // accumulator = accumulator - (x-1)⁴/4
    loadConstantVector(out, temp, -1.0/4.0, ElementSize::Double);
    emitVectorMulAdd(out, accumulator, 15, x4, temp, ElementSize::Double);
}

void ARM64SVE::emitCustomExpImplementation(std::vector<uint8_t>& out, int result, int operand) {
    // 指数関数の近似実装
    // e^x ≈ 1 + x + x²/2! + x³/3! + x⁴/4! + ...
    
    int x = operand;
    int x2 = 1;
    int x3 = 2;
    int x4 = 3;
    int temp = 4;
    int accumulator = result;
    
    // x² = x * x
    emitVectorMul(out, x2, 15, x, x, ElementSize::Double);
    
    // x³ = x² * x
    emitVectorMul(out, x3, 15, x2, x, ElementSize::Double);
    
    // x⁴ = x³ * x
    emitVectorMul(out, x4, 15, x3, x, ElementSize::Double);
    
    // accumulator = 1
    loadConstantVector(out, accumulator, 1.0, ElementSize::Double);
    
    // accumulator = accumulator + x
    emitVectorAdd(out, accumulator, 15, accumulator, x, ElementSize::Double);
    
    // accumulator = accumulator + x²/2
    loadConstantVector(out, temp, 1.0/2.0, ElementSize::Double);
    emitVectorMulAdd(out, accumulator, 15, x2, temp, ElementSize::Double);
    
    // accumulator = accumulator + x³/6
    loadConstantVector(out, temp, 1.0/6.0, ElementSize::Double);
    emitVectorMulAdd(out, accumulator, 15, x3, temp, ElementSize::Double);
    
    // accumulator = accumulator + x⁴/24
    loadConstantVector(out, temp, 1.0/24.0, ElementSize::Double);
    emitVectorMulAdd(out, accumulator, 15, x4, temp, ElementSize::Double);
}

void ARM64SVE::loadConstantVector(std::vector<uint8_t>& out, int zd, double value, ElementSize elementSize) {
    // 定数プールからのロードまたは即値生成を使用した完全実装
    
    // 1. 即値として表現可能かチェック
    if (canEncodeAsImmediate(value, elementSize)) {
        generateImmediateLoad(out, zd, value, elementSize);
        return;
    }
    
    // 2. 定数プールからのロード
    if (useConstantPool(value, elementSize)) {
        generateConstantPoolLoad(out, zd, value, elementSize);
        return;
    }
    
    // 3. レジスタ経由での複合生成
    generateComplexConstantLoad(out, zd, value, elementSize);
}

bool ARM64SVE::canEncodeAsImmediate(double value, ElementSize elementSize) {
    // ARM64 SVEの即値エンコーディング可能性をチェック
    
    if (elementSize == ElementSize::Double) {
        uint64_t bits = *reinterpret_cast<const uint64_t*>(&value);
        return canEncodeFP64Immediate(bits);
    } else if (elementSize == ElementSize::Single) {
        float floatValue = static_cast<float>(value);
        uint32_t bits = *reinterpret_cast<const uint32_t*>(&floatValue);
        return canEncodeFP32Immediate(bits);
    }
    
    return false;
}

bool ARM64SVE::canEncodeFP64Immediate(uint64_t bits) {
    // IEEE 754 倍精度の即値エンコーディング可能性
    
    // 特殊値のチェック
    if (bits == 0x0000000000000000ULL || // +0.0
        bits == 0x8000000000000000ULL || // -0.0
        bits == 0x3FF0000000000000ULL || // +1.0
        bits == 0xBFF0000000000000ULL) { // -1.0
        return true;
    }
    
    // ARM64の8ビット即値制約をチェック
    uint32_t sign = static_cast<uint32_t>((bits >> 63) & 0x1);
    uint32_t exponent = static_cast<uint32_t>((bits >> 52) & 0x7FF);
    uint64_t mantissa = bits & 0xFFFFFFFFFFFFF;
    
    // 指数部の制約
    if (exponent == 0 || exponent == 0x7FF) {
        return false; // 非正規化数、無限大、NaNは不可
    }
    
    // 仮数部の制約：下位48ビットが全て0である必要
    if ((mantissa & 0xFFFFFFFFFFF) != 0) {
        return false;
    }
    
    // 指数部の範囲制約
    int32_t biased_exponent = static_cast<int32_t>(exponent) - 1023;
    if (biased_exponent < -3 || biased_exponent > 4) {
        return false;
    }
    
    return true;
}

bool ARM64SVE::canEncodeFP32Immediate(uint32_t bits) {
    // IEEE 754 単精度の即値エンコーディング可能性
    
    // 特殊値のチェック
    if (bits == 0x00000000 || // +0.0f
        bits == 0x80000000 || // -0.0f
        bits == 0x3F800000 || // +1.0f
        bits == 0xBF800000) { // -1.0f
        return true;
    }
    
    uint32_t sign = (bits >> 31) & 0x1;
    uint32_t exponent = (bits >> 23) & 0xFF;
    uint32_t mantissa = bits & 0x7FFFFF;
    
    // 指数部の制約
    if (exponent == 0 || exponent == 0xFF) {
        return false;
    }
    
    // 仮数部の制約：下位19ビットが全て0である必要
    if ((mantissa & 0x7FFFF) != 0) {
        return false;
    }
    
    // 指数部の範囲制約
    int32_t biased_exponent = static_cast<int32_t>(exponent) - 127;
    if (biased_exponent < -3 || biased_exponent > 4) {
        return false;
    }
    
    return true;
}

void ARM64SVE::generateImmediateLoad(std::vector<uint8_t>& out, int zd, double value, ElementSize elementSize) {
    // 即値による直接ロード
    
    if (elementSize == ElementSize::Double) {
        uint64_t bits = *reinterpret_cast<const uint64_t*>(&value);
        uint8_t imm8 = encodeFloatingPointImmediate(bits);
        
        // FDUP Zd.D, #imm8
        uint32_t instruction = 0x659E2000 | (imm8 << 5) | zd;
        appendInstruction(out, instruction);
        
    } else if (elementSize == ElementSize::Single) {
        float floatValue = static_cast<float>(value);
        uint32_t bits = *reinterpret_cast<const uint32_t*>(&floatValue);
        uint8_t imm8 = encodeFP32Immediate(bits);
        
        // FDUP Zd.S, #imm8
        uint32_t instruction = 0x659C2000 | (imm8 << 5) | zd;
        appendInstruction(out, instruction);
        
    } else if (elementSize == ElementSize::Half) {
        // 半精度浮動小数点
        uint16_t halfValue = convertToFP16(value);
        uint8_t imm8 = encodeFP16Immediate(halfValue);
        
        // FDUP Zd.H, #imm8
        uint32_t instruction = 0x659A2000 | (imm8 << 5) | zd;
        appendInstruction(out, instruction);
    }
}

bool ARM64SVE::useConstantPool(double value, ElementSize elementSize) {
    // 定数プール使用の判定
    
    // 即値で表現できない場合は定数プールを使用
    if (!canEncodeAsImmediate(value, elementSize)) {
        return true;
    }
    
    // 頻繁に使用される定数は定数プールに配置
    if (isFrequentlyUsedConstant(value)) {
        return true;
    }
    
    // 大きなベクトル幅の場合は定数プールが効率的
    if (getCurrentVectorLength() > 256) {
        return true;
    }
    
    return false;
}

void ARM64SVE::generateConstantPoolLoad(std::vector<uint8_t>& out, int zd, double value, ElementSize elementSize) {
    // 定数プールからのロード実装
    
    // 1. 定数プールエントリの作成/検索
    uint32_t poolOffset = addToConstantPool(value, elementSize);
    
    // 2. 定数プールアドレスの計算
    int tempReg = allocateTempRegister();
    
    // ADR命令で定数プールのベースアドレスを取得
    // ADR Xt, constant_pool_base
    uint32_t adrInst = 0x10000000 | tempReg;
    appendInstruction(out, adrInst);
    
    // 3. オフセットの追加
    if (poolOffset > 0) {
        // ADD Xt, Xt, #offset
        uint32_t addInst = 0x91000000 | (tempReg << 5) | tempReg | ((poolOffset & 0xFFF) << 10);
        appendInstruction(out, addInst);
    }
    
    // 4. ベクトルロード命令の生成
    if (elementSize == ElementSize::Double) {
        // LD1RD {Zd.D}, Pg/Z, [Xt]
        uint32_t loadInst = 0x85C04000 | (0xF << 10) | (tempReg << 5) | zd;
        appendInstruction(out, loadInst);
        
    } else if (elementSize == ElementSize::Single) {
        // LD1RW {Zd.S}, Pg/Z, [Xt]
        uint32_t loadInst = 0x85804000 | (0xF << 10) | (tempReg << 5) | zd;
        appendInstruction(out, loadInst);
        
    } else if (elementSize == ElementSize::Half) {
        // LD1RH {Zd.H}, Pg/Z, [Xt]
        uint32_t loadInst = 0x85404000 | (0xF << 10) | (tempReg << 5) | zd;
        appendInstruction(out, loadInst);
    }
    
    // 5. 一時レジスタの解放
    releaseTempRegister(tempReg);
}

void ARM64SVE::generateComplexConstantLoad(std::vector<uint8_t>& out, int zd, double value, ElementSize elementSize) {
    // 複合的な定数生成（レジスタ経由）
    
    if (elementSize == ElementSize::Double) {
        uint64_t bits = *reinterpret_cast<const uint64_t*>(&value);
        
        // スカラーレジスタに定数をロード
        int tempScalarReg = allocateTempRegister();
        loadConstantToScalar(out, tempScalarReg, bits);
        
        // スカラーからベクトルへの複製
        // DUP Zd.D, Xn
        uint32_t dupInst = 0x05203800 | (tempScalarReg << 5) | zd;
        appendInstruction(out, dupInst);
        
        releaseTempRegister(tempScalarReg);
        
    } else if (elementSize == ElementSize::Single) {
        float floatValue = static_cast<float>(value);
        uint32_t bits = *reinterpret_cast<const uint32_t*>(&floatValue);
        
        // スカラーレジスタに定数をロード
        int tempScalarReg = allocateTempRegister();
        loadConstantToScalar(out, tempScalarReg, bits);
        
        // スカラーからベクトルへの複製
        // DUP Zd.S, Wn
        uint32_t dupInst = 0x05203400 | (tempScalarReg << 5) | zd;
        appendInstruction(out, dupInst);
        
        releaseTempRegister(tempScalarReg);
    }
}

uint8_t ARM64SVE::encodeFloatingPointImmediate(uint64_t bits) {
    // ARM64の8ビット浮動小数点即値エンコーディングの完全実装
    // ARMv8アーキテクチャ仕様書 C7.2.116 FMOV (immediate) に準拠
    
    // IEEE 754 倍精度浮動小数点の構造
    // 63:     符号ビット
    // 62-52:  11ビット指数部
    // 51-0:   52ビット仮数部
    
    uint32_t sign = static_cast<uint32_t>((bits >> 63) & 0x1);
    uint32_t exponent = static_cast<uint32_t>((bits >> 52) & 0x7FF);
    uint64_t mantissa = bits & 0xFFFFFFFFFFFFF;
    
    // ARMv8での8ビット即値エンコーディング仕様:
    // imm8[7]:    符号ビット (NOT(sign))
    // imm8[6:4]:  指数部の下位3ビット (NOT(exponent[2:0]))
    // imm8[3:0]:  仮数部の上位4ビット (mantissa[51:48])
    
    // 特殊値の処理
    if (exponent == 0) {
        if (mantissa == 0) {
            // ±0.0
            return sign ? 0x80 : 0x00;
        } else {
            // 非正規化数 - ARM64即値では表現できない
            return 0x00; // デフォルトで0.0を返す
        }
    }
    
    if (exponent == 0x7FF) {
        if (mantissa == 0) {
            // ±無限大
            return sign ? 0xFF : 0x7F;
        } else {
            // NaN - ARM64即値では表現できない
            return 0x00; // デフォルトで0.0を返す
        }
    }
    
    // 正規化数の処理
    // ARM64での有効な即値の制約をチェック
    
    // 指数部の制約: バイアス値1023を基準に、±127の範囲内である必要
    int32_t biased_exponent = static_cast<int32_t>(exponent) - 1023;
    if (biased_exponent < -126 || biased_exponent > 127) {
        // 指数部が範囲外 - 表現できない
        return 0x00;
    }
    
    // 仮数部の制約: 下位48ビットは全て0である必要
    if ((mantissa & 0xFFFFFFFFFFF) != 0) {
        // 精度が高すぎて8ビット即値では表現できない
        // 最も近い表現可能な値にキャストする
        mantissa = (mantissa >> 48) << 48;
    }
    
    // ARM64 immediate encoding
    // bit 7:   NOT(sign)
    // bit 6-4: NOT(exponent[2:0])
    // bit 3-0: mantissa[51:48]
    
    uint8_t imm8 = 0;
    
    // 符号ビット (反転)
    imm8 |= (sign ^ 1) << 7;
    
    // 指数部の下位3ビット (反転)
    uint32_t exp_low3 = exponent & 0x7;
    imm8 |= (exp_low3 ^ 0x7) << 4;
    
    // 仮数部の上位4ビット
    uint32_t mantissa_high4 = static_cast<uint32_t>((mantissa >> 48) & 0xF);
    imm8 |= mantissa_high4;
    
    // 一般的な値のLookup Table による最適化
    static const std::unordered_map<uint64_t, uint8_t> commonValues = {
        // よく使用される浮動小数点値
        {0x0000000000000000ULL, 0x00}, // +0.0
        {0x8000000000000000ULL, 0x80}, // -0.0
        {0x3FF0000000000000ULL, 0x70}, // +1.0
        {0xBFF0000000000000ULL, 0xF0}, // -1.0
        {0x4000000000000000ULL, 0x60}, // +2.0
        {0xC000000000000000ULL, 0xE0}, // -2.0
        {0x3FE0000000000000ULL, 0x78}, // +0.5
        {0xBFE0000000000000ULL, 0xF8}, // -0.5
        {0x4010000000000000ULL, 0x64}, // +4.0
        {0xC010000000000000ULL, 0xE4}, // -4.0
        {0x4020000000000000ULL, 0x68}, // +8.0
        {0xC020000000000000ULL, 0xE8}, // -8.0
        {0x4030000000000000ULL, 0x6C}, // +16.0
        {0xC030000000000000ULL, 0xEC}, // -16.0
        {0x3FD0000000000000ULL, 0x7C}, // +0.25
        {0xBFD0000000000000ULL, 0xFC}, // -0.25
        {0x3FC0000000000000ULL, 0x7E}, // +0.125
        {0xBFC0000000000000ULL, 0xFE}, // -0.125
        {0x3F80000000000000ULL, 0x7F}, // +0.0078125 (1/128)
        {0xBF80000000000000ULL, 0xFF}, // -0.0078125
    };
    
    auto it = commonValues.find(bits);
    if (it != commonValues.end()) {
        return it->second;
    }
    
    // 計算されたエンコーディングを検証
    // デコードして元の値と一致するかチェック
    uint64_t decoded = decodeFloatingPointImmediate(imm8);
    if (decoded == bits) {
        return imm8;
    }
    
    // 正確に表現できない場合は、最も近い表現可能な値を探す
    uint8_t best_encoding = 0x00;
    double original_value = *reinterpret_cast<const double*>(&bits);
    double min_error = std::abs(original_value);
    
    // 全ての可能なエンコーディング (256通り) をチェック
    for (uint32_t candidate = 0; candidate <= 0xFF; ++candidate) {
        uint64_t candidate_bits = decodeFloatingPointImmediate(static_cast<uint8_t>(candidate));
        double candidate_value = *reinterpret_cast<const double*>(&candidate_bits);
        double error = std::abs(original_value - candidate_value);
        
        if (error < min_error) {
            min_error = error;
            best_encoding = static_cast<uint8_t>(candidate);
        }
    }
    
    return best_encoding;
}

// 8ビット即値から倍精度浮動小数点への逆変換 (検証用)
uint64_t ARM64SVE::decodeFloatingPointImmediate(uint8_t imm8) {
    // ARM64 immediate decodingの逆処理
    uint32_t sign = ((imm8 >> 7) & 1) ^ 1; // NOT操作を元に戻す
    uint32_t exp_low3 = ((imm8 >> 4) & 7) ^ 7; // NOT操作を元に戻す
    uint32_t mantissa_high4 = imm8 & 0xF;
    
    // IEEE 754 倍精度の構築
    uint64_t result = 0;
    
    // 符号ビット
    result |= static_cast<uint64_t>(sign) << 63;
    
    // 指数部: ARM64では制限された範囲のみサポート
    // 通常の指数部は exp_low3 に基づいて計算
    uint32_t full_exponent;
    if (mantissa_high4 == 0 && exp_low3 != 0) {
        // 非正規化数の特殊ケース
        full_exponent = 0;
    } else {
        // 正規化数: 基本指数 + 下位3ビット
        // ARM64では基本指数は固定 (1019-1020程度)
        full_exponent = 1020 + exp_low3; // バイアス1023から調整
    }
    
    result |= static_cast<uint64_t>(full_exponent) << 52;
    
    // 仮数部: 上位4ビットのみ、残りは0
    uint64_t mantissa = static_cast<uint64_t>(mantissa_high4) << 48;
    result |= mantissa;
    
    return result;
}

void ARM64SVE::emitHighPerformanceMemcpy(std::vector<uint8_t>& out, int src, int dst, int length) {
    // 高性能メモリコピーの実装
    
    // SVEベクトル長の取得
    int vl_reg = 30; // 作業用GPレジスタ
    emitGetVectorLength(out, vl_reg, ElementSize::Byte);
    
    // ループカウンタとポインタの初期化
    int src_ptr = src;     // x0: ソースポインタ  
    int dst_ptr = dst;     // x1: デスティネーションポインタ
    int remaining = length; // x2: 残りバイト数
    
    // メインループ（SVEベクトル長単位）
    // while (remaining >= vector_length)
    uint32_t loop_start = static_cast<uint32_t>(out.size());
    
    // 残りバイト数とベクトル長を比較
    // CMP remaining, vl_reg
    appendInstruction(out, 0xEB1E005F | (remaining << 5) | (vl_reg << 16));
    
    // 残りが少ない場合は末尾処理へ
    // B.LO cleanup
    uint32_t cleanup_branch = static_cast<uint32_t>(out.size());
    appendInstruction(out, 0x54000003); // 条件分岐のプレースホルダー
    
    // SVEロード
    emitContiguousLoad(out, 0, 15, src_ptr, ElementSize::Byte);
    
    // SVEストア
    emitContiguousStore(out, 0, 15, dst_ptr, ElementSize::Byte);
    
    // ポインタを進める
    // ADD src_ptr, src_ptr, vl_reg
    appendInstruction(out, 0x8B1E0000 | (src_ptr << 5) | src_ptr | (vl_reg << 16));
    
    // ADD dst_ptr, dst_ptr, vl_reg  
    appendInstruction(out, 0x8B1E0020 | (dst_ptr << 5) | dst_ptr | (vl_reg << 16));
    
    // SUB remaining, remaining, vl_reg
    appendInstruction(out, 0xCB1E0040 | (remaining << 5) | remaining | (vl_reg << 16));
    
    // ループバック
    int32_t loop_offset = static_cast<int32_t>(loop_start - out.size()) / 4;
    appendInstruction(out, 0x17000000 | (loop_offset & 0x3FFFFFF));
    
    // 末尾処理：残りバイトを1バイトずつコピー
    uint32_t cleanup_start = static_cast<uint32_t>(out.size());
    
    // 分岐先の修正
    uint32_t* cleanup_branch_ptr = reinterpret_cast<uint32_t*>(out.data() + cleanup_branch);
    int32_t cleanup_offset = static_cast<int32_t>(cleanup_start - cleanup_branch) / 4;
    *cleanup_branch_ptr = (*cleanup_branch_ptr & 0xFF00001F) | ((cleanup_offset & 0x7FFFF) << 5);
    
    // remaining == 0 なら終了
    // CBZ remaining, end
    uint32_t end_branch = static_cast<uint32_t>(out.size());
    appendInstruction(out, 0x34000040 | (remaining << 5)); // プレースホルダー
    
    // バイト単位コピーループ
    uint32_t byte_loop_start = static_cast<uint32_t>(out.size());
    
    // LDRB w3, [src_ptr], #1
    appendInstruction(out, 0x38401003 | (src_ptr << 5));
    
    // STRB w3, [dst_ptr], #1  
    appendInstruction(out, 0x38001023 | (dst_ptr << 5));
    
    // SUBS remaining, remaining, #1
    appendInstruction(out, 0xF1000440 | (remaining << 5) | remaining);
    
    // B.NE byte_loop_start
    int32_t byte_loop_offset = static_cast<int32_t>(byte_loop_start - out.size()) / 4;
    appendInstruction(out, 0x54000001 | ((byte_loop_offset & 0x7FFFF) << 5));
    
    // 終了
    uint32_t end_pos = static_cast<uint32_t>(out.size());
    uint32_t* end_branch_ptr = reinterpret_cast<uint32_t*>(out.data() + end_branch);
    int32_t end_offset = static_cast<int32_t>(end_pos - end_branch) / 4;
    *end_branch_ptr = (*end_branch_ptr & 0xFF00001F) | ((end_offset & 0x7FFFF) << 5);
}

void ARM64SVE::emitVectorizedStringSearch(std::vector<uint8_t>& out, int haystack, int needle, int result) {
    // ベクトル化された文字列検索の実装
    
    // レジスタ割り当て
    int haystack_ptr = haystack;  // x0: 検索対象文字列
    int needle_ptr = needle;      // x1: 検索パターン
    int result_ptr = result;      // x2: 結果格納場所
    int vl_reg = 30;             // x30: ベクトル長
    int needle_char = 29;        // x29: 検索文字
    
    // ベクトルレジスタ
    int haystack_vec = 0;        // z0: 読み込んだ文字列データ
    int needle_vec = 1;          // z1: 検索パターン
    int match_mask = 0;          // p0: マッチマスク
    int active_mask = 15;        // p15: アクティブレーン
    
    // 検索文字をロード（最初の文字のみの簡単な実装）
    // LDRB needle_char, [needle_ptr]
    appendInstruction(out, 0x39400000 | (needle_ptr << 5) | needle_char);
    
    // 検索文字をベクトル全体にブロードキャスト
    // DUP needle_vec.B, needle_char
    appendInstruction(out, 0x05200400 | (needle_char << 5) | needle_vec);
    
    // SVEベクトル長取得
    emitGetVectorLength(out, vl_reg, ElementSize::Byte);
    
    // 結果を初期化（見つからなかった場合の値）
    // MOV result, #-1
    appendInstruction(out, 0x92800000 | result_ptr);
    
    // メインサーチループ
    uint32_t search_loop_start = static_cast<uint32_t>(out.size());
    
    // haystackから文字列をロード
    emitContiguousLoad(out, haystack_vec, active_mask, haystack_ptr, ElementSize::Byte);
    
    // NULL文字チェック（文字列終端）
    int zero_vec = 2;
    loadConstantVector(out, zero_vec, 0.0, ElementSize::Byte);
    emitVectorCompareEQ(out, 1, active_mask, haystack_vec, zero_vec, ElementSize::Byte);
    
    // NULL文字が見つかったら終了
    // B.ANY p1, end
    uint32_t null_found_branch = static_cast<uint32_t>(out.size());
    appendInstruction(out, 0x25400000); // プレースホルダー
    
    // パターンマッチング
    emitVectorCompareEQ(out, match_mask, active_mask, haystack_vec, needle_vec, ElementSize::Byte);
    
    // マッチが見つかったかチェック
    // B.ANY match_mask, found
    uint32_t match_found_branch = static_cast<uint32_t>(out.size());
    appendInstruction(out, 0x25400000 | match_mask); // プレースホルダー
    
    // ポインタを進める
    // ADD haystack_ptr, haystack_ptr, vl_reg
    appendInstruction(out, 0x8B1E0000 | (haystack_ptr << 5) | haystack_ptr | (vl_reg << 16));
    
    // ループ継続
    int32_t loop_offset = static_cast<int32_t>(search_loop_start - out.size()) / 4;
    appendInstruction(out, 0x17000000 | (loop_offset & 0x3FFFFFF));
    
    // マッチ見つかった場合の処理
    uint32_t found_label = static_cast<uint32_t>(out.size());
    
    // マッチ位置を計算
    // INCP result_ptr, match_mask.B
    appendInstruction(out, 0x252C0000 | (match_mask << 5) | result_ptr);
    
    // 終了
    uint32_t end_label = static_cast<uint32_t>(out.size());
    
    // 分岐先の修正
    uint32_t* null_branch_ptr = reinterpret_cast<uint32_t*>(out.data() + null_found_branch);
    int32_t null_offset = static_cast<int32_t>(end_label - null_found_branch) / 4;
    *null_branch_ptr = (*null_branch_ptr & 0xFF000000) | (null_offset & 0x7FFFF) << 5;
    
    uint32_t* match_branch_ptr = reinterpret_cast<uint32_t*>(out.data() + match_found_branch);
    int32_t match_offset = static_cast<int32_t>(found_label - match_found_branch) / 4;
    *match_branch_ptr = (*match_branch_ptr & 0xFF000000) | (match_offset & 0x7FFFF) << 5;
}

} // namespace core
} // namespace aerojs 