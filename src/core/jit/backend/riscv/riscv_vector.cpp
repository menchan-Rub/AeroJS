#include "riscv_vector.h"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace aerojs {
namespace core {

void RISCVVector::appendInstruction(std::vector<uint8_t>& out, uint32_t instruction) {
    // RISC-Vはリトルエンディアン
    out.push_back(instruction & 0xFF);
    out.push_back((instruction >> 8) & 0xFF);
    out.push_back((instruction >> 16) & 0xFF);
    out.push_back((instruction >> 24) & 0xFF);
}

void RISCVVector::emitSetVL(std::vector<uint8_t>& out, int rd, int rs1, 
                           RVVectorSEW sew, RVVectorLMUL lmul,
                           RVVectorVTA vta, RVVectorVMA vma) {
    // vsetvli rd, rs1, vtypei
    // フォーマット: imm[10:0] = zimm[10:0]
    // zimm[10:7] = 0
    // zimm[6:5] = vediv (00=1, 01=2, 10=4, 11=8)
    // zimm[4:2] = vsew (000=8, 001=16, 010=32, 011=64)
    // zimm[1:0] = vlmul (000=1, 001=2, 010=4, 011=8, 101=1/8, 110=1/4, 111=1/2)
    
    uint32_t zimm = (static_cast<uint32_t>(sew) << 3) |
                   (static_cast<uint32_t>(lmul)) |
                   (static_cast<uint32_t>(vta) << 6) |
                   (static_cast<uint32_t>(vma) << 7);
    
    // vsetvli opcode: 0b0010111, funct3=111
    uint32_t instr = 0x57 | (0x7 << 12) | (rs1 << 15) | (rd << 7) | (zimm << 20);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorLoad(std::vector<uint8_t>& out, int vd, int rs1, RVVectorMask vm, int width) {
    // VLE.V vd, (rs1)
    // フォーマット: | nf[31:29] | mew[28] | width[27:26] | mop[25:24] | vm[23] | lumop[22:20] | rs2[19:15] | rs1[14:10] | funct3[9:7] | vd[6:0] | opcode[6:0] |
    
    uint32_t width_bits;
    switch (width) {
        case 8:  width_bits = 0b00; break;
        case 16: width_bits = 0b01; break;
        case 32: width_bits = 0b10; break;
        case 64: width_bits = 0b11; break;
        default: assert(false && "Invalid vector element width");
    }
    
    // VLE.V opcode: 0b0000111, funct3=111, nf=000, mew=0, mop=00, lumop=000, rs2=00000
    uint32_t instr = 0x07 | (0x7 << 7) | (rs1 << 12) | (0x0 << 17) | ((vm & 0x1) << 23) | 
                    (width_bits << 26) | (0x0 << 28);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorLoadStrided(std::vector<uint8_t>& out, int vd, int rs1, int rs2, RVVectorMask vm, int width) {
    // VLSE.V vd, (rs1), rs2
    // フォーマット: | nf[31:29] | mew[28] | width[27:26] | mop[25:24] | vm[23] | lumop[22:20] | rs2[19:15] | rs1[14:10] | funct3[9:7] | vd[6:0] | opcode[6:0] |
    
    uint32_t width_bits;
    switch (width) {
        case 8:  width_bits = 0b00; break;
        case 16: width_bits = 0b01; break;
        case 32: width_bits = 0b10; break;
        case 64: width_bits = 0b11; break;
        default: assert(false && "Invalid vector element width");
    }
    
    // VLSE.V opcode: 0b0000111, funct3=111, nf=000, mew=0, mop=00, lumop=010
    uint32_t instr = 0x07 | (0x7 << 7) | (rs1 << 12) | (rs2 << 17) | ((vm & 0x1) << 23) | 
                    (0x2 << 20) | (width_bits << 26) | (0x0 << 28);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorStore(std::vector<uint8_t>& out, int vs3, int rs1, RVVectorMask vm, int width) {
    // VSE.V vs3, (rs1)
    // フォーマット: | nf[31:29] | mew[28] | width[27:26] | mop[25:24] | vm[23] | sumop[22:20] | rs2[19:15] | rs1[14:10] | funct3[9:7] | vd[6:0] | opcode[6:0] |
    
    uint32_t width_bits;
    switch (width) {
        case 8:  width_bits = 0b00; break;
        case 16: width_bits = 0b01; break;
        case 32: width_bits = 0b10; break;
        case 64: width_bits = 0b11; break;
        default: assert(false && "Invalid vector element width");
    }
    
    // VSE.V opcode: 0b0100111, funct3=111, nf=000, mew=0, mop=00, sumop=000, rs2=00000
    uint32_t instr = 0x27 | (0x7 << 7) | (vs3 << 7) | (rs1 << 12) | (0x0 << 17) | ((vm & 0x1) << 23) | 
                    (width_bits << 26) | (0x0 << 28);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorStoreStrided(std::vector<uint8_t>& out, int vs3, int rs1, int rs2, RVVectorMask vm, int width) {
    // VSSE.V vs3, (rs1), rs2
    // フォーマット: | nf[31:29] | mew[28] | width[27:26] | mop[25:24] | vm[23] | sumop[22:20] | rs2[19:15] | rs1[14:10] | funct3[9:7] | vd[6:0] | opcode[6:0] |
    
    uint32_t width_bits;
    switch (width) {
        case 8:  width_bits = 0b00; break;
        case 16: width_bits = 0b01; break;
        case 32: width_bits = 0b10; break;
        case 64: width_bits = 0b11; break;
        default: assert(false && "Invalid vector element width");
    }
    
    // VSSE.V opcode: 0b0100111, funct3=111, nf=000, mew=0, mop=00, sumop=010
    uint32_t instr = 0x27 | (0x7 << 7) | (vs3 << 7) | (rs1 << 12) | (rs2 << 17) | ((vm & 0x1) << 23) | 
                    (0x2 << 20) | (width_bits << 26) | (0x0 << 28);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorAdd(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm) {
    // VADD.VV vd, vs1, vs2
    // フォーマット: | funct6[31:26] | vm[25] | vs2[24:20] | vs1[19:15] | funct3[14:12] | vd[11:7] | opcode[6:0] |
    
    // VADD.VV opcode: 0b1010111, funct3=000, funct6=000000
    uint32_t instr = 0x57 | (0x0 << 12) | (vd << 7) | (vs1 << 15) | (vs2 << 20) | ((vm & 0x1) << 25);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorAddScalar(std::vector<uint8_t>& out, int vd, int vs2, int rs1, RVVectorMask vm) {
    // VADD.VX vd, vs2, rs1
    // フォーマット: | funct6[31:26] | vm[25] | vs2[24:20] | rs1[19:15] | funct3[14:12] | vd[11:7] | opcode[6:0] |
    
    // VADD.VX opcode: 0b1010111, funct3=100, funct6=000000
    uint32_t instr = 0x57 | (0x4 << 12) | (vd << 7) | (rs1 << 15) | (vs2 << 20) | ((vm & 0x1) << 25);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorSub(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm) {
    // VSUB.VV vd, vs1, vs2
    // フォーマット: | funct6[31:26] | vm[25] | vs2[24:20] | vs1[19:15] | funct3[14:12] | vd[11:7] | opcode[6:0] |
    
    // VSUB.VV opcode: 0b1010111, funct3=000, funct6=000010
    uint32_t instr = 0x57 | (0x0 << 12) | (vd << 7) | (vs1 << 15) | (vs2 << 20) | ((vm & 0x1) << 25) | (0x2 << 26);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorMul(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm) {
    // VMUL.VV vd, vs1, vs2
    // フォーマット: | funct6[31:26] | vm[25] | vs2[24:20] | vs1[19:15] | funct3[14:12] | vd[11:7] | opcode[6:0] |
    
    // VMUL.VV opcode: 0b1010111, funct3=000, funct6=100101
    uint32_t instr = 0x57 | (0x0 << 12) | (vd << 7) | (vs1 << 15) | (vs2 << 20) | ((vm & 0x1) << 25) | (0x25 << 26);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorMulScalar(std::vector<uint8_t>& out, int vd, int vs2, int rs1, RVVectorMask vm) {
    // VMUL.VX vd, vs2, rs1
    // フォーマット: | funct6[31:26] | vm[25] | vs2[24:20] | rs1[19:15] | funct3[14:12] | vd[11:7] | opcode[6:0] |
    
    // VMUL.VX opcode: 0b1010111, funct3=100, funct6=100101
    uint32_t instr = 0x57 | (0x4 << 12) | (vd << 7) | (rs1 << 15) | (vs2 << 20) | ((vm & 0x1) << 25) | (0x25 << 26);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorDiv(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm) {
    // VDIV.VV vd, vs1, vs2
    // フォーマット: | funct6[31:26] | vm[25] | vs2[24:20] | vs1[19:15] | funct3[14:12] | vd[11:7] | opcode[6:0] |
    
    // VDIV.VV opcode: 0b1010111, funct3=000, funct6=100001
    uint32_t instr = 0x57 | (0x0 << 12) | (vd << 7) | (vs1 << 15) | (vs2 << 20) | ((vm & 0x1) << 25) | (0x21 << 26);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorMulAdd(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm) {
    // VMACC.VV vd, vs1, vs2
    // フォーマット: | funct6[31:26] | vm[25] | vs2[24:20] | vs1[19:15] | funct3[14:12] | vd[11:7] | opcode[6:0] |
    
    // VMACC.VV opcode: 0b1010111, funct3=000, funct6=101101
    uint32_t instr = 0x57 | (0x0 << 12) | (vd << 7) | (vs1 << 15) | (vs2 << 20) | ((vm & 0x1) << 25) | (0x2D << 26);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorRedMax(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm) {
    // VREDMAX.VS vd, vs2, vs1
    // フォーマット: | funct6[31:26] | vm[25] | vs2[24:20] | vs1[19:15] | funct3[14:12] | vd[11:7] | opcode[6:0] |
    
    // VREDMAX.VS opcode: 0b1010111, funct3=010, funct6=000111
    uint32_t instr = 0x57 | (0x2 << 12) | (vd << 7) | (vs1 << 15) | (vs2 << 20) | ((vm & 0x1) << 25) | (0x7 << 26);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorRedMin(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm) {
    // VREDMIN.VS vd, vs2, vs1
    // フォーマット: | funct6[31:26] | vm[25] | vs2[24:20] | vs1[19:15] | funct3[14:12] | vd[11:7] | opcode[6:0] |
    
    // VREDMIN.VS opcode: 0b1010111, funct3=010, funct6=000110
    uint32_t instr = 0x57 | (0x2 << 12) | (vd << 7) | (vs1 << 15) | (vs2 << 20) | ((vm & 0x1) << 25) | (0x6 << 26);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorRedSum(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm) {
    // VREDSUM.VS vd, vs2, vs1
    // フォーマット: | funct6[31:26] | vm[25] | vs2[24:20] | vs1[19:15] | funct3[14:12] | vd[11:7] | opcode[6:0] |
    
    // VREDSUM.VS opcode: 0b1010111, funct3=010, funct6=000000
    uint32_t instr = 0x57 | (0x2 << 12) | (vd << 7) | (vs1 << 15) | (vs2 << 20) | ((vm & 0x1) << 25) | (0x0 << 26);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorCompare(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm, int cmp) {
    // VMSEQ/VMSNE/VMSLT/VMSLE/VMSGT/VMSGE.VV vd, vs1, vs2
    // フォーマット: | funct6[31:26] | vm[25] | vs2[24:20] | vs1[19:15] | funct3[14:12] | vd[11:7] | opcode[6:0] |
    
    uint32_t funct6;
    switch (cmp) {
        case 0: funct6 = 0x10; break; // EQ
        case 1: funct6 = 0x11; break; // NE
        case 2: funct6 = 0x14; break; // LT
        case 3: funct6 = 0x15; break; // LE
        case 4: funct6 = 0x17; break; // GT
        case 5: funct6 = 0x16; break; // GE
        default: assert(false && "Invalid comparison operation");
    }
    
    // VMSxx.VV opcode: 0b1010111, funct3=000
    uint32_t instr = 0x57 | (0x0 << 12) | (vd << 7) | (vs1 << 15) | (vs2 << 20) | ((vm & 0x1) << 25) | (funct6 << 26);
    
    appendInstruction(out, instr);
}

void RISCVVector::emitVectorSlide(std::vector<uint8_t>& out, int vd, int vs2, int rs1, RVVectorMask vm, bool up) {
    // VSLIDEUP/VSLIDEDOWN.VX vd, vs2, rs1
    // フォーマット: | funct6[31:26] | vm[25] | vs2[24:20] | rs1[19:15] | funct3[14:12] | vd[11:7] | opcode[6:0] |
    
    uint32_t funct6 = up ? 0x0E : 0x0F; // VSLIDEUP=001110, VSLIDEDOWN=001111
    
    // VSLIDEx.VX opcode: 0b1010111, funct3=100
    uint32_t instr = 0x57 | (0x4 << 12) | (vd << 7) | (rs1 << 15) | (vs2 << 20) | ((vm & 0x1) << 25) | (funct6 << 26);
    
    appendInstruction(out, instr);
}

bool RISCVVector::canVectorize(const std::vector<IRInstruction>& loopInsts) {
    // ベクトル化可能性の判定
    if (loopInsts.size() < 3) {
        return false;  // 小さすぎるループはベクトル化しない
    }
    
    bool hasArrayAccess = false;
    bool hasSimpleArithmetic = false;
    
    for (const auto& inst : loopInsts) {
        switch (inst.opcode) {
            case IROpcode::LoadElement:
            case IROpcode::StoreElement:
                hasArrayAccess = true;
                break;
                
            case IROpcode::Add:
            case IROpcode::Sub:
            case IROpcode::Mul:
                hasSimpleArithmetic = true;
                break;
                
            case IROpcode::Call:
            case IROpcode::Branch:
                // 関数呼び出しや分岐がある場合はベクトル化しない
                return false;
                
            default:
                // その他の命令はケースバイケースで評価
                break;
        }
    }
    
    // 配列アクセスと単純な算術演算を含むループのみベクトル化
    return hasArrayAccess && hasSimpleArithmetic;
}

void RISCVVector::emitPreloopCode(const std::vector<IRInstruction>& loopInsts, std::vector<uint8_t>& out) {
    // ベクトル化前のプロローグコード生成
    
    // 1. ベクトル化するループ長の分析
    // ループ長を動的に計算し、適切なベクトル長設定を生成
    emitComment("ベクトルループ長の計算");

    // ループ長を取得（引数またはメモリからロード）
    if (hasConstantLength) {
        // 定数長の場合は直接設定
        int vl = std::min(staticLoopLength, maxVectorLength);
        emitLI(t0, vl);
    } else {
        // 動的長の場合はメモリから取得
        emitLD(t0, a0, lengthOffset); // a0にはデータ構造のベースアドレスが格納されている
        
        // 最大ベクトル長との比較
        emitLI(t1, maxVectorLength);
        emitMINI(t0, t0, t1); // min(loop_length, max_vector_length)
    }

    // ベクトル長設定命令: vsetvli t0, t0, e32, m4, ta, ma
    // - t0: 要求ベクトル長
    // - e32: 32ビット要素
    // - m4: 4つのベクトルレジスタをグループ化
    // - ta: tail agnostic（残り要素は無視）
    // - ma: mask agnostic（マスクされた要素は無視）
    emitVSETVLI(t0, t0, VLParam(VSEW_32, VLMUL_4, VTAIL_AGNOSTIC, VMASK_AGNOSTIC));
    
    // 2. ベクトル設定の初期化
    // vsetvli t0, a0, e32, m4, ta, ma
    emitSetVL(out, 5 /*t0*/, 10 /*a0*/, SEW_32, LMUL_4);
    
    // 3. ベクトルレジスタの初期化（必要に応じて）
    // 例: 累積用ベクトルレジスタをゼロ初期化
    
    // 4. アドレス計算とストライド設定
    // 配列ベースアドレスとストライドの設定コードを生成
    emitComment("配列アドレスと要素ストライド設定");
    
    // 配列の先頭アドレスをレジスタに読み込む
    // lw a0, obj_offset(base)
    uint32_t arrayOffsetInObject = 8; // オブジェクト内の配列フィールドオフセット（仮定）
    encodeIType(arrayOffsetInObject, baseReg, 2, A0, RV_OP_LOAD);
    
    // アドレスがnullでないことを確認
    // beqz a0, error_handler
    uint32_t nullCheckTargetOffset = 12; // エラーハンドラへのオフセット（バイト単位）
    encodeBType(nullCheckTargetOffset, A0, X0, 0, RV_BRANCH);
    
    // 配列要素サイズに基づいてストライド（要素間隔）を計算
    // li a1, element_size
    uint32_t elementSize = 0;
    switch (elementType) {
        case ElementType::Int8:
            elementSize = 1;
            break;
        case ElementType::Int16:
            elementSize = 2;
            break;
        case ElementType::Int32:
        case ElementType::Float32:
            elementSize = 4;
            break;
        case ElementType::Int64:
        case ElementType::Float64:
            elementSize = 8;
            break;
        default:
            elementSize = 4; // デフォルトは4バイト
    }
    encodeIType(elementSize, X0, 0, A1, RV_OP_IMM);
    
    // 配列長を取得（バウンドチェック用）
    // lw a2, length_offset(base)
    uint32_t lengthOffsetInObject = 4; // オブジェクト内の長さフィールドオフセット（仮定）
    encodeIType(lengthOffsetInObject, baseReg, 2, A2, RV_OP_LOAD);
    
    // インデックスが配列長未満かチェック
    // bge index, a2, error_handler  (index >= length はエラー)
    uint32_t boundCheckTargetOffset = 12; // エラーハンドラへのオフセット
    encodeBType(boundCheckTargetOffset, indexReg, A2, 5, RV_BRANCH);
    
    // インデックス値をストライドに掛けてオフセットを計算
    // mul a3, index, a1
    encodeRType(0, indexReg, A1, 0, A3, RV_OP_MUL);
    
    // 配列ベースアドレスにオフセットを加算
    // add a4, a0, a3
    encodeRType(0, A3, A0, 0, A4, RV_OP_ADD);
    
    // 計算したアドレスをvl* 命令の基底アドレスとして使用
    // 例えば: vle32.v v0, (a4) など
}

void RISCVVector::emitVectorizedLoop(const std::vector<IRInstruction>& loopInsts, std::vector<uint8_t>& out) {
    // ベクトル化されたループ本体の生成
    
    // 1. ループラベル
    // vector_loop:
    
    // 2. ベクトルロード
    // 例: 配列からデータをベクトルレジスタにロード
    emitVectorLoad(out, 1 /*v1*/, 10 /*a0*/, MASK_NONE, 32);
    
    // 3. ベクトル演算
    // 例: ベクトル加算
    emitVectorAdd(out, 2 /*v2*/, 1 /*v1*/, 3 /*v3*/, MASK_NONE);
    
    // 4. ベクトルストア
    // 例: 結果を配列に書き戻し
    emitVectorStore(out, 2 /*v2*/, 11 /*a1*/, MASK_NONE, 32);
    
    // 5. アドレス更新
    // 例: a0 += 4*vl (ベクトル長 * 要素サイズ)
    // 例: a1 += 4*vl
    
    // 6. ループカウンタ更新と条件チェック
    // 例: a2 -= vl
    // 例: bgtz a2, vector_loop
}

void RISCVVector::emitPostloopCode(const std::vector<IRInstruction>& loopInsts, std::vector<uint8_t>& out) {
    // ベクトル化後のエピローグコード生成
    
    // 1. 残りの要素をスカラーループで処理
    // scalar_loop:
    
    // 2. 単一要素のロード
    // 例: lw t0, 0(a0)
    
    // 3. スカラー演算
    // 例: add t0, t0, t1
    
    // 4. 単一要素のストア
    // 例: sw t0, 0(a1)
    
    // 5. アドレス更新
    // 例: a0 += 4
    // 例: a1 += 4
    
    // 6. ループカウンタ更新と条件チェック
    // 例: a2 -= 1
    // 例: bgtz a2, scalar_loop
    
    // 7. ループ終了
    // end:
}

// 行列乗算操作の実装
void RISCVVector::emitMatrixMultiply(std::vector<uint8_t>& out, int rows, int cols, int inner) {
    // 行列乗算の実装（A[rows][inner] × B[inner][cols] = C[rows][cols]）
    
    // 必要なレジスタ割り当てを想定
    //   vd: 結果行列用
    //   vs1: 入力行列A用
    //   vs2: 入力行列B用
    //   t0: 行インデックス
    //   t1: 列インデックス
    //   t2: 内部ループインデックス
    
    // 行列乗算の定数設定
    uint32_t vsetivli_rows = encodeVsetivli(T0, rows, RVVectorSEW::E32, RVVectorLMUL::M8);
    appendInstruction(out, vsetivli_rows);
    
    // 行ループ開始（t0 = 0）
    uint32_t addi_t0_zero_0 = encodeIType(0, 0, 0, T0, 0x13); // addi t0, zero, 0
    appendInstruction(out, addi_t0_zero_0);
    
    // 行ループラベル
    size_t row_loop_start = out.size();
    
    // 列ループ初期化（t1 = 0）
    uint32_t addi_t1_zero_0 = encodeIType(0, 0, 0, T1, 0x13); // addi t1, zero, 0
    appendInstruction(out, addi_t1_zero_0);
    
    // 列ループラベル
    size_t col_loop_start = out.size();
    
    // 結果ベクトルの初期化（ゼロクリア）
    uint32_t zero_vd = encodeVectorOp(0x57, 0, 0, 0, 0, 0x23); // vxor.vv v0, v0, v0
    appendInstruction(out, zero_vd);
    
    // 内部ループ初期化（t2 = 0）
    uint32_t addi_t2_zero_0 = encodeIType(0, 0, 0, T2, 0x13); // addi t2, zero, 0
    appendInstruction(out, addi_t2_zero_0);
    
    // 内部ループラベル
    size_t inner_loop_start = out.size();
    
    // 行列A要素読み込み（ストライドはinner）
    uint32_t load_vs1 = encodeVectorStrideLoad(V1, T0, T2, inner, RVVectorMask::UNMASKED, 4);
    appendInstruction(out, load_vs1);
    
    // 行列B要素読み込み（ストライドはcols）
    uint32_t load_vs2 = encodeVectorStrideLoad(V2, T2, T1, cols, RVVectorMask::UNMASKED, 4);
    appendInstruction(out, load_vs2);
    
    // 積和演算 - FMA命令を使用
    uint32_t vfmacc = encodeVectorOp(0x57, V0, V1, V2, 0, 0x45); // vfmacc.vv v0, v1, v2
    appendInstruction(out, vfmacc);
    
    // 内部ループカウンタインクリメント
    uint32_t addi_t2_t2_1 = encodeIType(1, T2, 0, T2, 0x13); // addi t2, t2, 1
    appendInstruction(out, addi_t2_t2_1);
    
    // 内部ループ条件チェック
    uint32_t blt_t2_inner = encodeBType(T2, inner, 7, 0, inner_loop_start - out.size()); // blt t2, inner, inner_loop
    appendInstruction(out, blt_t2_inner);
    
    // 結果を保存（t0行、t1列の位置に）
    uint32_t store_vd = encodeVectorStore(V0, T0, T1, RVVectorMask::UNMASKED, 4);
    appendInstruction(out, store_vd);
    
    // 列ループカウンタインクリメント
    uint32_t addi_t1_t1_1 = encodeIType(1, T1, 0, T1, 0x13); // addi t1, t1, 1
    appendInstruction(out, addi_t1_t1_1);
    
    // 列ループ条件チェック
    uint32_t blt_t1_cols = encodeBType(T1, cols, 7, 0, col_loop_start - out.size()); // blt t1, cols, col_loop
    appendInstruction(out, blt_t1_cols);
    
    // 行ループカウンタインクリメント
    uint32_t addi_t0_t0_1 = encodeIType(1, T0, 0, T0, 0x13); // addi t0, t0, 1
    appendInstruction(out, addi_t0_t0_1);
    
    // 行ループ条件チェック
    uint32_t blt_t0_rows = encodeBType(T0, rows, 7, 0, row_loop_start - out.size()); // blt t0, rows, row_loop
    appendInstruction(out, blt_t0_rows);
}

// ベクトル数学関数の実装 - 平方根
void RISCVVector::emitVectorSqrt(std::vector<uint8_t>& out, int vd, int vs2, RVVectorMask vm) {
    // vsqrt.v vd, vs2, vm
    uint32_t vsqrt = encodeVectorOp(0x57, vd, 0, vs2, vm == RVVectorMask::MASKED ? 1 : 0, 0x4F);
    appendInstruction(out, vsqrt);
}

// ベクトル数学関数の実装 - 絶対値
void RISCVVector::emitVectorAbs(std::vector<uint8_t>& out, int vd, int vs2, RVVectorMask vm) {
    // vabs.v vd, vs2, vm
    uint32_t vabs = encodeVectorOp(0x57, vd, 0, vs2, vm == RVVectorMask::MASKED ? 1 : 0, 0x4B);
    appendInstruction(out, vabs);
}

// ベクトルビット操作 - 論理AND
void RISCVVector::emitVectorAnd(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm) {
    // vand.vv vd, vs1, vs2, vm
    uint32_t vand = encodeVectorOp(0x57, vd, vs1, vs2, vm == RVVectorMask::MASKED ? 1 : 0, 0x27);
    appendInstruction(out, vand);
}

// ベクトルビット操作 - 論理OR
void RISCVVector::emitVectorOr(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm) {
    // vor.vv vd, vs1, vs2, vm
    uint32_t vor = encodeVectorOp(0x57, vd, vs1, vs2, vm == RVVectorMask::MASKED ? 1 : 0, 0x25);
    appendInstruction(out, vor);
}

// ベクトルビット操作 - 論理XOR
void RISCVVector::emitVectorXor(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm) {
    // vxor.vv vd, vs1, vs2, vm
    uint32_t vxor = encodeVectorOp(0x57, vd, vs1, vs2, vm == RVVectorMask::MASKED ? 1 : 0, 0x23);
    appendInstruction(out, vxor);
}

// ベクトルビット操作 - 論理NOT（補数）
void RISCVVector::emitVectorNot(std::vector<uint8_t>& out, int vd, int vs2, RVVectorMask vm) {
    // vnot.v vd, vs2, vm
    uint32_t vnot = encodeVectorOp(0x57, vd, 0, vs2, vm == RVVectorMask::MASKED ? 1 : 0, 0x2F);
    appendInstruction(out, vnot);
}

// ベクトル縮約操作 - 総和
void RISCVVector::emitVectorRedSum(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm) {
    // vredsum.vs vd, vs1, vs2, vm
    uint32_t vredsum = encodeVectorOp(0x57, vd, vs1, vs2, vm == RVVectorMask::MASKED ? 1 : 0, 0x03);
    appendInstruction(out, vredsum);
}

// ベクトル縮約操作 - 最大値
void RISCVVector::emitVectorRedMax(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm) {
    // vredmax.vs vd, vs1, vs2, vm
    uint32_t vredmax = encodeVectorOp(0x57, vd, vs1, vs2, vm == RVVectorMask::MASKED ? 1 : 0, 0x07);
    appendInstruction(out, vredmax);
}

// ベクトル縮約操作 - 最小値
void RISCVVector::emitVectorRedMin(std::vector<uint8_t>& out, int vd, int vs1, int vs2, RVVectorMask vm) {
    // vredmin.vs vd, vs1, vs2, vm
    uint32_t vredmin = encodeVectorOp(0x57, vd, vs1, vs2, vm == RVVectorMask::MASKED ? 1 : 0, 0x05);
    appendInstruction(out, vredmin);
}

// ヘルパーメソッド - VSET命令のエンコード
uint32_t RISCVVector::encodeVsetivli(int rd, int uimm, RVVectorSEW sew, RVVectorLMUL lmul) {
    // vsetivli rd, uimm, sew+lmul
    uint32_t sew_lmul = static_cast<uint32_t>(sew) | static_cast<uint32_t>(lmul);
    uint32_t zimm = (uimm & 0x1F) | ((sew_lmul & 0x7F) << 5);
    return encodeIType(zimm, 0, 7, rd, 0x57);
}

// ヘルパーメソッド - ベクトル操作のエンコード
uint32_t RISCVVector::encodeVectorOp(uint32_t opcode, uint32_t vd, uint32_t vs1, uint32_t vs2, uint32_t vm, uint32_t funct6) {
    uint32_t instr = opcode & 0x7F;
    instr |= (vd & 0x1F) << 7;
    instr |= (vs1 & 0x1F) << 15;
    instr |= (vs2 & 0x1F) << 20;
    instr |= (vm & 0x1) << 25;
    instr |= (funct6 & 0x3F) << 26;
    return instr;
}

// ヘルパーメソッド - ベクトルロード命令のエンコード
uint32_t RISCVVector::encodeVectorLoad(uint32_t vd, uint32_t rs1, RVVectorMask vm, uint32_t width) {
    // VLE<width>.v vd, (rs1), vm
    uint32_t funct3 = 0;
    switch (width) {
        case 1: funct3 = 0; break; // バイト
        case 2: funct3 = 5; break; // ハーフワード
        case 4: funct3 = 6; break; // ワード
        case 8: funct3 = 7; break; // ダブルワード
        default: funct3 = 6; break; // デフォルトはワード
    }
    
    uint32_t instr = 0x07 | (vd << 7) | (rs1 << 15) | (funct3 << 12) | (vm == RVVectorMask::MASKED ? 1 << 25 : 0) | (0 << 26);
    return instr;
}

// ヘルパーメソッド - ベクトルストライドロード命令のエンコード
uint32_t RISCVVector::encodeVectorStrideLoad(uint32_t vd, uint32_t rs1, uint32_t rs2, uint32_t stride, RVVectorMask vm, uint32_t width) {
    // VLSE<width>.v vd, (rs1), rs2, vm
    uint32_t funct3 = 0;
    switch (width) {
        case 1: funct3 = 0; break; // バイト
        case 2: funct3 = 5; break; // ハーフワード
        case 4: funct3 = 6; break; // ワード
        case 8: funct3 = 7; break; // ダブルワード
        default: funct3 = 6; break; // デフォルトはワード
    }
    
    uint32_t instr = 0x27 | (vd << 7) | (rs1 << 15) | (rs2 << 20) | (funct3 << 12) | (vm == RVVectorMask::MASKED ? 1 << 25 : 0) | (0 << 26);
    return instr;
}

// ヘルパーメソッド - ベクトルストア命令のエンコード
uint32_t RISCVVector::encodeVectorStore(uint32_t vs3, uint32_t rs1, uint32_t rs2, RVVectorMask vm, uint32_t width) {
    // VSE<width>.v vs3, (rs1), vm
    uint32_t funct3 = 0;
    switch (width) {
        case 1: funct3 = 0; break; // バイト
        case 2: funct3 = 5; break; // ハーフワード
        case 4: funct3 = 6; break; // ワード
        case 8: funct3 = 7; break; // ダブルワード
        default: funct3 = 6; break; // デフォルトはワード
    }
    
    uint32_t instr = 0x27 | (vs3 << 7) | (rs1 << 15) | (rs2 << 20) | (funct3 << 12) | (vm == RVVectorMask::MASKED ? 1 << 25 : 0) | (0 << 26);
    return instr;
}

// Bタイプ命令エンコード用ヘルパー（条件分岐命令）
uint32_t RISCVVector::encodeBType(uint32_t rs1, uint32_t rs2, uint32_t funct3, uint32_t opcode, int32_t offset) {
    // offset must be even and within ±4KiB range
    offset &= 0xFFFFFFFE; // ensure even alignment
    
    uint32_t imm12 = (offset >> 12) & 0x1;
    uint32_t imm10_5 = (offset >> 5) & 0x3F;
    uint32_t imm4_1 = (offset >> 1) & 0xF;
    uint32_t imm11 = (offset >> 11) & 0x1;
    
    uint32_t instr = opcode & 0x7F;
    instr |= (imm11 << 7) | (imm4_1 << 8) | (funct3 << 12) | (rs1 << 15) | (rs2 << 20) | (imm10_5 << 25) | (imm12 << 31);
    
    return instr;
}

// JavaScript配列のSIMD処理用ファンクション
void RISCVVector::emitJSArrayOperation(std::vector<uint8_t>& out, int operation, int arrayReg, int resultReg, int length) {
    // JavaScript配列操作のSIMD実装
    // operation: 0=map, 1=filter, 2=reduce, 3=forEach
    
    // ベクトル長設定（length要素まで）
    uint32_t vsetvli = encodeVsetivli(T0, length, RVVectorSEW::E64, RVVectorLMUL::M4);
    appendInstruction(out, vsetvli);
    
    // 配列ポインタ設定（arrayReg → t1）
    uint32_t mv_t1_array = encodeRType(0, arrayReg, 0, 0, T1, 0x33); // mv t1, arrayReg
    appendInstruction(out, mv_t1_array);
    
    // 結果ポインタ設定（resultReg → t2）
    uint32_t mv_t2_result = encodeRType(0, resultReg, 0, 0, T2, 0x33); // mv t2, resultReg
    appendInstruction(out, mv_t2_result);
    
    // 配列要素をベクトルレジスタにロード
    uint32_t vle64_v = encodeVectorLoad(V1, T1, RVVectorMask::UNMASKED, 8);
    appendInstruction(out, vle64_v);
    
    // 操作に応じた処理
    switch (operation) {
        case 0: // map
            // 各要素にマップ関数適用（例えば2倍の値を計算）
            emitVectorMulScalar(out, V2, V1, X10, RVVectorMask::UNMASKED); // X10に乗数が入っていると仮定
            
            // 結果を保存
            uint32_t vse64_v = encodeVectorStore(V2, T2, 0, RVVectorMask::UNMASKED, 8);
            appendInstruction(out, vse64_v);
            break;
            
        case 1: // filter
            // 条件チェック（例：正の値のみフィルタリング）
            emitVectorCompare(out, V0, V1, 0, RVVectorMask::UNMASKED, 1); // 0より大きいかチェック
            
            // マスクを使用して値を保存
            uint32_t vse64_v_mask = encodeVectorStore(V1, T2, 0, RVVectorMask::MASKED, 8);
            appendInstruction(out, vse64_v_mask);
            break;
            
        case 2: // reduce
            // 縮約操作（例：合計を計算）
            emitVectorRedSum(out, V0, V1, V1, RVVectorMask::UNMASKED);
            
            // 単一結果を保存
            uint32_t vse64_single = encodeVectorStore(V0, T2, 0, RVVectorMask::UNMASKED, 8);
            appendInstruction(out, vse64_single);
            break;
            
        case 3: // forEach
            // 各要素に対して操作を行うが、結果は保存しない（サイドエフェクトのみ）
            // この例では、各要素の絶対値を計算
            emitVectorAbs(out, V2, V1, RVVectorMask::UNMASKED);
            break;
    }
}

// 関数の末尾に追加
uint32_t RISCVVector::encodeRType(uint32_t funct7, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
    uint32_t instr = opcode & 0x7F;
    instr |= (rd & 0x1F) << 7;
    instr |= (funct3 & 0x7) << 12;
    instr |= (rs1 & 0x1F) << 15;
    instr |= (rs2 & 0x1F) << 20;
    instr |= (funct7 & 0x7F) << 25;
    return instr;
}

bool RISCVVectorProcessor::EmitVectorizedLoopForDataType(IRNode* loop, DataType elementType, LoopPattern pattern) {
    // データタイプとループパターンに基づいたベクタライズ実装
    if (!m_assembler.SupportsExtension(RISCVExtension::V)) {
        m_logger.Warning("RISC-V Vectorベクトル拡張機能がサポートされていません。スカラコードを生成します。");
        return false;
    }
    
    // ループのコンポーネントを取得
    auto* loopNode = static_cast<LoopNode*>(loop);
    IRNode* body = loopNode->GetBody();
    IRNode* init = loopNode->GetInit();
    IRNode* cond = loopNode->GetCondition();
    IRNode* update = loopNode->GetUpdate();
    
    // ループ分析
    LoopInfo info = AnalyzeLoop(loop);
    
    // ベクトル化可能かどうかチェック
    if (!info.IsVectorizable()) {
        m_logger.Info("ループはベクトル化できません: " + info.GetReason());
        return false;
    }
    
    // レジスタ割り当て
    Register counterReg = m_assembler.AllocateRegister();
    Register endReg = m_assembler.AllocateRegister();
    VRegister maskReg = m_assembler.AllocateVRegister();
    
    // ループパターンに基づいて実装を選択
    switch (pattern) {
        case LoopPattern::Map:
            return EmitVectorizedMapLoop(loopNode, info, elementType, counterReg, endReg, maskReg);
            
        case LoopPattern::Reduce:
            return EmitVectorizedReduceLoop(loopNode, info, elementType, counterReg, endReg, maskReg);
            
        case LoopPattern::Scan:
            return EmitVectorizedScanLoop(loopNode, info, elementType, counterReg, endReg, maskReg);
            
        case LoopPattern::Gather:
            return EmitVectorizedGatherLoop(loopNode, info, elementType, counterReg, endReg, maskReg);
            
        case LoopPattern::Scatter:
            return EmitVectorizedScatterLoop(loopNode, info, elementType, counterReg, endReg, maskReg);
            
        default:
            m_logger.Error("サポートされていないループパターン: " + std::to_string(static_cast<int>(pattern)));
            return false;
    }
}

bool RISCVVectorProcessor::EmitVectorizedMapLoop(LoopNode* loop, const LoopInfo& info, DataType elementType,
                                              Register counterReg, Register endReg, VRegister maskReg) {
    // Mapパターン: foreach(i) output[i] = f(input[i])
    
    // ループの境界情報
    int64_t start = info.GetStartValue();
    int64_t end = info.GetEndValue();
    int64_t step = info.GetStepValue();
    
    // 入力配列と出力配列の情報を取得
    IRNode* body = loop->GetBody();
    ArrayAccess inputAccess, outputAccess;
    
    if (!ExtractMapPatternArrayAccess(body, info, inputAccess, outputAccess)) {
        m_logger.Error("Mapパターンの配列アクセスを特定できませんでした");
        return false;
    }
    
    // 入力/出力配列のベースアドレスを取得
    Register inputBaseReg = m_assembler.AllocateRegister();
    Register outputBaseReg = m_assembler.AllocateRegister();
    
    m_assembler.EmitLoadEffectiveAddress(inputBaseReg, inputAccess.baseReg, inputAccess.offset);
    m_assembler.EmitLoadEffectiveAddress(outputBaseReg, outputAccess.baseReg, outputAccess.offset);
    
    // 必要なベクトルレジスタを割り当て
    VRegister inputVReg = m_assembler.AllocateVRegister();
    VRegister outputVReg = m_assembler.AllocateVRegister();
    
    // データタイプに基づいてレジスタタイプとサイズを設定
    VSEW vsew = GetVSEWForDataType(elementType);
    VLMUL vlmul = GetOptimalVLMUL(elementType);
    
    // カウンタの初期化
    m_assembler.EmitLoadImmediate(counterReg, start);
    m_assembler.EmitLoadImmediate(endReg, end);
    
    // メインループラベル
    Label mainLoopLabel = m_assembler.CreateLabel("vector_map_loop");
    Label exitLabel = m_assembler.CreateLabel("vector_map_exit");
    
    // CSRの設定（ベクトル長、データ幅、倍率）
    m_assembler.EmitVsetvli(counterReg, endReg, vsew, vlmul);
    
    // メインループ開始
    m_assembler.EmitBindLabel(mainLoopLabel);
    
    // 終了条件チェック
    m_assembler.EmitBranchIfGreaterEqual(counterReg, endReg, m_assembler.CreateLabelRef(exitLabel));
    
    // ベクトル長を設定
    m_assembler.EmitVsetvli(counterReg, endReg, vsew, vlmul);
    
    // 入力配列からベクトルロード
    m_assembler.EmitVectorLoad(inputVReg, inputBaseReg, counterReg, elementType);
    
    // ベクトル操作を生成（Mapパターンの処理）
    EmitVectorOperation(body, info, inputVReg, outputVReg, elementType);
    
    // 出力配列にベクトルストア
    m_assembler.EmitVectorStore(outputVReg, outputBaseReg, counterReg, elementType);
    
    // カウンタを更新
    m_assembler.EmitAdd(counterReg, counterReg, step);
    
    // ループバック
    m_assembler.EmitBranch(m_assembler.CreateLabelRef(mainLoopLabel));
    
    // 終了ラベル
    m_assembler.EmitBindLabel(exitLabel);
    
    return true;
}

bool RISCVVectorProcessor::EmitVectorizedReduceLoop(LoopNode* loop, const LoopInfo& info, DataType elementType,
                                                 Register counterReg, Register endReg, VRegister maskReg) {
    // Reduceパターン: result = foreach(i) reduce(function, input[i])
    
    // ループの境界情報
    int64_t start = info.GetStartValue();
    int64_t end = info.GetEndValue();
    int64_t step = info.GetStepValue();
    
    // 入力配列と還元演算の情報を取得
    IRNode* body = loop->GetBody();
    ArrayAccess inputAccess;
    ReduceOperation reduceOp;
    
    if (!ExtractReducePatternInfo(body, info, inputAccess, reduceOp)) {
        m_logger.Error("Reduceパターンの情報を特定できませんでした");
        return false;
    }
    
    // 入力配列のベースアドレスを取得
    Register inputBaseReg = m_assembler.AllocateRegister();
    m_assembler.EmitLoadEffectiveAddress(inputBaseReg, inputAccess.baseReg, inputAccess.offset);
    
    // 必要なベクトルレジスタを割り当て
    VRegister inputVReg = m_assembler.AllocateVRegister();
    VRegister resultVReg = m_assembler.AllocateVRegister();
    
    // データタイプに基づいてレジスタタイプとサイズを設定
    VSEW vsew = GetVSEWForDataType(elementType);
    VLMUL vlmul = GetOptimalVLMUL(elementType);
    
    // カウンタの初期化
    m_assembler.EmitLoadImmediate(counterReg, start);
    m_assembler.EmitLoadImmediate(endReg, end);
    
    // 結果レジスタの初期化（還元演算の種類による）
    InitializeVectorForReduction(resultVReg, reduceOp, elementType);
    
    // メインループラベル
    Label mainLoopLabel = m_assembler.CreateLabel("vector_reduce_loop");
    Label exitLabel = m_assembler.CreateLabel("vector_reduce_exit");
    
    // CSRの設定（ベクトル長、データ幅、倍率）
    m_assembler.EmitVsetvli(counterReg, endReg, vsew, vlmul);
    
    // メインループ開始
    m_assembler.EmitBindLabel(mainLoopLabel);
    
    // 終了条件チェック
    m_assembler.EmitBranchIfGreaterEqual(counterReg, endReg, m_assembler.CreateLabelRef(exitLabel));
    
    // ベクトル長を設定
    m_assembler.EmitVsetvli(counterReg, endReg, vsew, vlmul);
    
    // 入力配列からベクトルロード
    m_assembler.EmitVectorLoad(inputVReg, inputBaseReg, counterReg, elementType);
    
    // 還元演算を実行
    EmitVectorReduction(reduceOp, resultVReg, inputVReg, elementType);
    
    // カウンタを更新
    m_assembler.EmitAdd(counterReg, counterReg, step);
    
    // ループバック
    m_assembler.EmitBranch(m_assembler.CreateLabelRef(mainLoopLabel));
    
    // 終了ラベル
    m_assembler.EmitBindLabel(exitLabel);
    
    // 最終的な還元結果をスカラレジスタに移動
    Register resultReg = reduceOp.resultReg;
    EmitVectorToScalarReduction(resultReg, resultVReg, elementType);
    
    return true;
}

bool RISCVVectorProcessor::EmitVectorizedScanLoop(LoopNode* loop, const LoopInfo& info, DataType elementType,
                                               Register counterReg, Register endReg, VRegister maskReg) {
    // Scanパターン: output[i] = reduce(function, input[0..i])
    
    // ループの境界情報
    int64_t start = info.GetStartValue();
    int64_t end = info.GetEndValue();
    int64_t step = info.GetStepValue();
    
    // 入力配列、出力配列、演算の情報を取得
    IRNode* body = loop->GetBody();
    ArrayAccess inputAccess, outputAccess;
    ReduceOperation scanOp;
    
    if (!ExtractScanPatternInfo(body, info, inputAccess, outputAccess, scanOp)) {
        m_logger.Error("Scanパターンの情報を特定できませんでした");
        return false;
    }
    
    // 入力/出力配列のベースアドレスを取得
    Register inputBaseReg = m_assembler.AllocateRegister();
    Register outputBaseReg = m_assembler.AllocateRegister();
    
    m_assembler.EmitLoadEffectiveAddress(inputBaseReg, inputAccess.baseReg, inputAccess.offset);
    m_assembler.EmitLoadEffectiveAddress(outputBaseReg, outputAccess.baseReg, outputAccess.offset);
    
    // 必要なベクトルレジスタを割り当て
    VRegister inputVReg = m_assembler.AllocateVRegister();
    VRegister scanVReg = m_assembler.AllocateVRegister();
    VRegister tempVReg = m_assembler.AllocateVRegister();
    
    // データタイプに基づいてレジスタタイプとサイズを設定
    VSEW vsew = GetVSEWForDataType(elementType);
    VLMUL vlmul = GetOptimalVLMUL(elementType);
    
    // カウンタの初期化
    m_assembler.EmitLoadImmediate(counterReg, start);
    m_assembler.EmitLoadImmediate(endReg, end);
    
    // スキャン結果の初期化
    InitializeVectorForReduction(scanVReg, scanOp, elementType);
    
    // メインループラベル
    Label mainLoopLabel = m_assembler.CreateLabel("vector_scan_loop");
    Label exitLabel = m_assembler.CreateLabel("vector_scan_exit");
    
    // CSRの設定（ベクトル長、データ幅、倍率）
    m_assembler.EmitVsetvli(counterReg, endReg, vsew, vlmul);
    
    // メインループ開始
    m_assembler.EmitBindLabel(mainLoopLabel);
    
    // 終了条件チェック
    m_assembler.EmitBranchIfGreaterEqual(counterReg, endReg, m_assembler.CreateLabelRef(exitLabel));
    
    // ベクトル長を設定
    m_assembler.EmitVsetvli(counterReg, endReg, vsew, vlmul);
    
    // 入力配列からベクトルロード
    m_assembler.EmitVectorLoad(inputVReg, inputBaseReg, counterReg, elementType);
    
    // スキャン演算を実行（プレフィックススキャン）
    EmitVectorScan(scanOp, tempVReg, inputVReg, scanVReg, elementType);
    
    // 出力配列にベクトルストア
    m_assembler.EmitVectorStore(tempVReg, outputBaseReg, counterReg, elementType);
    
    // カウンタを更新
    m_assembler.EmitAdd(counterReg, counterReg, step);
    
    // ループバック
    m_assembler.EmitBranch(m_assembler.CreateLabelRef(mainLoopLabel));
    
    // 終了ラベル
    m_assembler.EmitBindLabel(exitLabel);
    
    return true;
}

void RISCVVectorProcessor::EmitVectorOperation(IRNode* node, const LoopInfo& info, 
                                            VRegister inputReg, VRegister outputReg, 
                                            DataType elementType) {
    // ノードのタイプに基づいてベクトル操作を発行
    switch (node->GetType()) {
        case IRNodeType::BinaryOp: {
            auto* binOp = static_cast<BinaryOpNode*>(node);
            BinaryOpType opType = binOp->GetOpType();
            IRNode* lhs = binOp->GetLHS();
            IRNode* rhs = binOp->GetRHS();
            
            // 特定のバイナリ演算をベクトル命令にマッピング
            EmitVectorBinaryOp(opType, outputReg, inputReg, GetOperandRegister(rhs, info), elementType);
            break;
        }
            
        case IRNodeType::UnaryOp: {
            auto* unaryOp = static_cast<UnaryOpNode*>(node);
            UnaryOpType opType = unaryOp->GetOpType();
            
            // 単項演算をベクトル命令にマッピング
            EmitVectorUnaryOp(opType, outputReg, inputReg, elementType);
            break;
        }
            
        case IRNodeType::ConditionalOp: {
            auto* condOp = static_cast<ConditionalOpNode*>(node);
            IRNode* condition = condOp->GetCondition();
            IRNode* trueExpr = condOp->GetTrueExpr();
            IRNode* falseExpr = condOp->GetFalseExpr();
            
            // 条件付き操作をベクトル選択またはマスク操作にマッピング
            EmitVectorConditionalOp(condition, trueExpr, falseExpr, outputReg, info, elementType);
            break;
        }
            
        case IRNodeType::Block: {
            auto* block = static_cast<BlockNode*>(node);
            for (auto* stmt : block->GetStatements()) {
                EmitVectorOperation(stmt, info, inputReg, outputReg, elementType);
            }
            break;
        }
            
        default:
            m_logger.Error("サポートされていないノードタイプ: " + std::to_string(static_cast<int>(node->GetType())));
            break;
    }
}

void RISCVVectorProcessor::EmitVectorBinaryOp(BinaryOpType opType, VRegister destReg, 
                                           VRegister srcReg1, VRegister srcReg2, 
                                           DataType elementType) {
    // データタイプに基づいて適切なベクトル命令を選択
    bool isFloat = IsFloatingPointType(elementType);
    
    switch (opType) {
        case BinaryOpType::Add:
            if (isFloat) {
                m_assembler.EmitVFADD(destReg, srcReg1, srcReg2);
            } else {
                m_assembler.EmitVADD(destReg, srcReg1, srcReg2);
            }
            break;
            
        case BinaryOpType::Subtract:
            if (isFloat) {
                m_assembler.EmitVFSUB(destReg, srcReg1, srcReg2);
            } else {
                m_assembler.EmitVSUB(destReg, srcReg1, srcReg2);
            }
            break;
            
        case BinaryOpType::Multiply:
            if (isFloat) {
                m_assembler.EmitVFMUL(destReg, srcReg1, srcReg2);
            } else {
                m_assembler.EmitVMUL(destReg, srcReg1, srcReg2);
            }
            break;
            
        case BinaryOpType::Divide:
            if (isFloat) {
                m_assembler.EmitVFDIV(destReg, srcReg1, srcReg2);
            } else {
                if (IsSignedIntegerType(elementType)) {
                    m_assembler.EmitVDIV(destReg, srcReg1, srcReg2);
                } else {
                    m_assembler.EmitVDIVU(destReg, srcReg1, srcReg2);
                }
            }
            break;
            
        case BinaryOpType::Modulo:
            if (isFloat) {
                m_logger.Error("浮動小数点型のモジュロ演算は直接サポートされていません");
            } else {
                if (IsSignedIntegerType(elementType)) {
                    m_assembler.EmitVREM(destReg, srcReg1, srcReg2);
                } else {
                    m_assembler.EmitVREMU(destReg, srcReg1, srcReg2);
                }
            }
            break;
            
        case BinaryOpType::And:
            m_assembler.EmitVAND(destReg, srcReg1, srcReg2);
            break;
            
        case BinaryOpType::Or:
            m_assembler.EmitVOR(destReg, srcReg1, srcReg2);
            break;
            
        case BinaryOpType::Xor:
            m_assembler.EmitVXOR(destReg, srcReg1, srcReg2);
            break;
            
        case BinaryOpType::ShiftLeft:
            m_assembler.EmitVSLL(destReg, srcReg1, srcReg2);
            break;
            
        case BinaryOpType::ShiftRight:
            if (IsSignedIntegerType(elementType)) {
                m_assembler.EmitVSRA(destReg, srcReg1, srcReg2);
            } else {
                m_assembler.EmitVSRL(destReg, srcReg1, srcReg2);
            }
            break;
            
        case BinaryOpType::Minimum:
            if (isFloat) {
                m_assembler.EmitVFMIN(destReg, srcReg1, srcReg2);
            } else {
                if (IsSignedIntegerType(elementType)) {
                    m_assembler.EmitVMIN(destReg, srcReg1, srcReg2);
                } else {
                    m_assembler.EmitVMINU(destReg, srcReg1, srcReg2);
                }
            }
            break;
            
        case BinaryOpType::Maximum:
            if (isFloat) {
                m_assembler.EmitVFMAX(destReg, srcReg1, srcReg2);
            } else {
                if (IsSignedIntegerType(elementType)) {
                    m_assembler.EmitVMAX(destReg, srcReg1, srcReg2);
                } else {
                    m_assembler.EmitVMAXU(destReg, srcReg1, srcReg2);
                }
            }
            break;
            
        default:
            m_logger.Error("サポートされていないバイナリ操作: " + std::to_string(static_cast<int>(opType)));
            break;
    }
}

void RISCVVectorProcessor::EmitVectorReduction(const ReduceOperation& reduceOp, VRegister destReg, 
                                            VRegister srcReg, DataType elementType) {
    // 還元演算の種類に基づいてベクトル還元命令を選択
    bool isFloat = IsFloatingPointType(elementType);
    
    switch (reduceOp.type) {
        case ReduceOpType::Sum:
            if (isFloat) {
                m_assembler.EmitVFREDSUM(destReg, destReg, srcReg);
            } else {
                m_assembler.EmitVREDSUM(destReg, destReg, srcReg);
            }
            break;
            
        case ReduceOpType::Product:
            if (isFloat) {
                // RVVには直接的な乗算還元がないため、対数を使用するか、
                // またはループ内でvfredosum+vfmulを使用して実装
                EmitManualVectorReduction(reduceOp, destReg, srcReg, elementType);
            } else {
                EmitManualVectorReduction(reduceOp, destReg, srcReg, elementType);
            }
            break;
            
        case ReduceOpType::Min:
            if (isFloat) {
                m_assembler.EmitVFREDMIN(destReg, destReg, srcReg);
            } else {
                if (IsSignedIntegerType(elementType)) {
                    m_assembler.EmitVREDMIN(destReg, destReg, srcReg);
                } else {
                    m_assembler.EmitVREDMINU(destReg, destReg, srcReg);
                }
            }
            break;
            
        case ReduceOpType::Max:
            if (isFloat) {
                m_assembler.EmitVFREDMAX(destReg, destReg, srcReg);
            } else {
                if (IsSignedIntegerType(elementType)) {
                    m_assembler.EmitVREDMAX(destReg, destReg, srcReg);
                } else {
                    m_assembler.EmitVREDMAXU(destReg, destReg, srcReg);
                }
            }
            break;
            
        case ReduceOpType::And:
            m_assembler.EmitVREDAND(destReg, destReg, srcReg);
            break;
            
        case ReduceOpType::Or:
            m_assembler.EmitVREDOR(destReg, destReg, srcReg);
            break;
            
        case ReduceOpType::Xor:
            m_assembler.EmitVREDXOR(destReg, destReg, srcReg);
            break;
            
        default:
            m_logger.Error("サポートされていない還元操作: " + std::to_string(static_cast<int>(reduceOp.type)));
            break;
    }
}

void RISCVVectorProcessor::InitializeVectorForReduction(VRegister reg, const ReduceOperation& reduceOp, DataType elementType) {
    // 還元演算の種類に基づいて初期値を設定
    switch (reduceOp.type) {
        case ReduceOpType::Sum:
            // 0で初期化
            m_assembler.EmitVMV_V_I(reg, 0);
            break;
            
        case ReduceOpType::Product:
            // 1で初期化
            m_assembler.EmitVMV_V_I(reg, 1);
            break;
            
        case ReduceOpType::Min:
            // 最大値で初期化
            if (IsFloatingPointType(elementType)) {
                // 浮動小数点型の場合は、正の無限大
                m_assembler.EmitVFMV_V_F(reg, std::numeric_limits<float>::infinity());
            } else if (IsSignedIntegerType(elementType)) {
                // 符号付き整数型の場合は、型の最大値
                m_assembler.EmitVMV_V_I(reg, GetTypeMaxValue(elementType));
            } else {
                // 符号なし整数型の場合は、型の最大値
                m_assembler.EmitVMV_V_I(reg, GetTypeMaxValue(elementType));
            }
            break;
            
        case ReduceOpType::Max:
            // 最小値で初期化
            if (IsFloatingPointType(elementType)) {
                // 浮動小数点型の場合は、負の無限大
                m_assembler.EmitVFMV_V_F(reg, -std::numeric_limits<float>::infinity());
            } else if (IsSignedIntegerType(elementType)) {
                // 符号付き整数型の場合は、型の最小値
                m_assembler.EmitVMV_V_I(reg, GetTypeMinValue(elementType));
            } else {
                // 符号なし整数型の場合は、0
                m_assembler.EmitVMV_V_I(reg, 0);
            }
            break;
            
        case ReduceOpType::And:
            // すべてのビットが1の値で初期化
            m_assembler.EmitVMV_V_I(reg, -1);
            break;
            
        case ReduceOpType::Or:
        case ReduceOpType::Xor:
            // 0で初期化
            m_assembler.EmitVMV_V_I(reg, 0);
            break;
            
        default:
            m_logger.Error("サポートされていない還元初期化操作: " + std::to_string(static_cast<int>(reduceOp.type)));
            break;
    }
}

// 配列アドレスと要素ストライド設定
void RISCVVector::SetupArrayBaseAndStride(Register base, Register stride, int elementSize, int numElements) {
    Emit("vsetvli t0, zero, e" + std::to_string(elementSize * 8) + ", m1");
    Emit("li " + stride.name + ", " + std::to_string(elementSize));
}

} // namespace core
} // namespace aerojs 