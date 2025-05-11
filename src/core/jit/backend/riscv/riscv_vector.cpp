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
    // （実際の実装では、ループ長を取得するコードを生成）
    
    // 2. ベクトル設定の初期化
    // vsetvli t0, a0, e32, m4, ta, ma
    emitSetVL(out, 5 /*t0*/, 10 /*a0*/, SEW_32, LMUL_4);
    
    // 3. ベクトルレジスタの初期化（必要に応じて）
    // 例: 累積用ベクトルレジスタをゼロ初期化
    
    // 4. アドレス計算とストライド設定
    // （実際の実装では、配列ベースアドレスとストライドの設定コードを生成）
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

} // namespace core
} // namespace aerojs 