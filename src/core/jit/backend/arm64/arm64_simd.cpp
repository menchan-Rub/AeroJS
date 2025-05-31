#include "arm64_simd.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <unordered_set>

namespace aerojs {
namespace core {

void ARM64SIMD::appendInstruction(std::vector<uint8_t>& out, uint32_t instruction) {
    out.push_back(instruction & 0xFF);
    out.push_back((instruction >> 8) & 0xFF);
    out.push_back((instruction >> 16) & 0xFF);
    out.push_back((instruction >> 24) & 0xFF);
}

void ARM64SIMD::emitLoadPair(std::vector<uint8_t>& out, int vt1, int vt2, int xn, int offset) {
    // LDNP Qt1, Qt2, [Xn, #offset]
    // offset must be a multiple of 16 and in range [-512, 504]
    assert(offset >= -512 && offset <= 504 && offset % 16 == 0);
    
    // エンコーディング: 0b1010110001iiiiiitttttnnnnnmmmmm
    // ここで、iiiii は符号付きオフセット/16, ttttt は最初のターゲットレジスタ (Vt1)
    // nnnnn は2番目のターゲットレジスタ (Vt2), mmmmm はベースレジスタ
    uint32_t scaled_offset = (offset / 16) & 0x3F;
    uint32_t instr = 0x6C000000 | (scaled_offset << 15) | (vt2 << 10) | (xn << 5) | vt1;
    
    appendInstruction(out, instr);
}

void ARM64SIMD::emitStorePair(std::vector<uint8_t>& out, int vt1, int vt2, int xn, int offset) {
    // STNP Qt1, Qt2, [Xn, #offset]
    // offset must be a multiple of 16 and in range [-512, 504]
    assert(offset >= -512 && offset <= 504 && offset % 16 == 0);
    
    // エンコーディング: 0b1010110000iiiiiitttttnnnnnmmmmm
    uint32_t scaled_offset = (offset / 16) & 0x3F;
    uint32_t instr = 0x6C000000 | (scaled_offset << 15) | (vt2 << 10) | (xn << 5) | vt1;
    
    appendInstruction(out, instr);
}

void ARM64SIMD::emitLoadQuad(std::vector<uint8_t>& out, int vt, int xn, int offset) {
    // LDR Qt, [Xn, #offset]
    // offset must be a multiple of 16 and in range [0, 16380]
    assert(offset >= 0 && offset <= 16380 && offset % 16 == 0);
    
    // エンコーディング: 0b11111100011iiiiiiiiiinnnnnttttt
    uint32_t scaled_offset = (offset / 16) & 0x7FF;
    uint32_t instr = 0x3DC00000 | (scaled_offset << 10) | (xn << 5) | vt;
    
    appendInstruction(out, instr);
}

void ARM64SIMD::emitStoreQuad(std::vector<uint8_t>& out, int vt, int xn, int offset) {
    // STR Qt, [Xn, #offset]
    // offset must be a multiple of 16 and in range [0, 16380]
    assert(offset >= 0 && offset <= 16380 && offset % 16 == 0);
    
    // エンコーディング: 0b11111100001iiiiiiiiiinnnnnttttt
    uint32_t scaled_offset = (offset / 16) & 0x7FF;
    uint32_t instr = 0x3D800000 | (scaled_offset << 10) | (xn << 5) | vt;
    
    appendInstruction(out, instr);
}

void ARM64SIMD::emitVectorAdd(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement) {
    // FADD Vd.T, Vn.T, Vm.T
    uint32_t size_q = 0;
    
    switch (arrangement) {
        case ARRANGE_2S:
            size_q = 0b00 << 22;  // size=00, Q=0 (2S)
            break;
        case ARRANGE_4S:
            size_q = 0b00 << 22 | 1 << 30;  // size=00, Q=1 (4S)
            break;
        case ARRANGE_2D:
            size_q = 0b01 << 22 | 1 << 30;  // size=01, Q=1 (2D)
            break;
        default:
            assert(false && "Unsupported vector arrangement for FADD");
    }
    
    // エンコーディング: 0b01001110xx1nnnnniiiiii1101mmmmm
    uint32_t instr = 0x4E20D400 | size_q | (vn << 5) | (vm << 16) | vd;
    
    appendInstruction(out, instr);
}

void ARM64SIMD::emitVectorSub(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement) {
    // FSUB Vd.T, Vn.T, Vm.T
    uint32_t size_q = 0;
    
    switch (arrangement) {
        case ARRANGE_2S:
            size_q = 0b00 << 22;  // size=00, Q=0 (2S)
            break;
        case ARRANGE_4S:
            size_q = 0b00 << 22 | 1 << 30;  // size=00, Q=1 (4S)
            break;
        case ARRANGE_2D:
            size_q = 0b01 << 22 | 1 << 30;  // size=01, Q=1 (2D)
            break;
        default:
            assert(false && "Unsupported vector arrangement for FSUB");
    }
    
    // エンコーディング: 0b01001110xx1nnnnniiiiii1110mmmmm
    uint32_t instr = 0x4E20D800 | size_q | (vn << 5) | (vm << 16) | vd;
    
    appendInstruction(out, instr);
}

void ARM64SIMD::emitVectorMul(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement) {
    // FMUL Vd.T, Vn.T, Vm.T
    uint32_t size_q = 0;
    
    switch (arrangement) {
        case ARRANGE_2S:
            size_q = 0b00 << 22;  // size=00, Q=0 (2S)
            break;
        case ARRANGE_4S:
            size_q = 0b00 << 22 | 1 << 30;  // size=00, Q=1 (4S)
            break;
        case ARRANGE_2D:
            size_q = 0b01 << 22 | 1 << 30;  // size=01, Q=1 (2D)
            break;
        default:
            assert(false && "Unsupported vector arrangement for FMUL");
    }
    
    // エンコーディング: 0b01001110xx1nnnnniiiiii1111mmmmm
    uint32_t instr = 0x4E20DC00 | size_q | (vn << 5) | (vm << 16) | vd;
    
    appendInstruction(out, instr);
}

void ARM64SIMD::emitVectorDiv(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement) {
    // FDIV Vd.T, Vn.T, Vm.T
    uint32_t size_q = 0;
    
    switch (arrangement) {
        case ARRANGE_2S:
            size_q = 0b00 << 22;  // size=00, Q=0 (2S)
            break;
        case ARRANGE_4S:
            size_q = 0b00 << 22 | 1 << 30;  // size=00, Q=1 (4S)
            break;
        case ARRANGE_2D:
            size_q = 0b01 << 22 | 1 << 30;  // size=01, Q=1 (2D)
            break;
        default:
            assert(false && "Unsupported vector arrangement for FDIV");
    }
    
    // エンコーディング: 0b01001110xx1nnnnniiiiii1111mmmmm
    uint32_t instr = 0x4E20FC00 | size_q | (vn << 5) | (vm << 16) | vd;
    
    appendInstruction(out, instr);
}

void ARM64SIMD::emitVectorMulAdd(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement) {
    // FMLA Vd.T, Vn.T, Vm.T
    uint32_t size_q = 0;
    
    switch (arrangement) {
        case ARRANGE_2S:
            size_q = 0b00 << 22;  // size=00, Q=0 (2S)
            break;
        case ARRANGE_4S:
            size_q = 0b00 << 22 | 1 << 30;  // size=00, Q=1 (4S)
            break;
        case ARRANGE_2D:
            size_q = 0b01 << 22 | 1 << 30;  // size=01, Q=1 (2D)
            break;
        default:
            assert(false && "Unsupported vector arrangement for FMLA");
    }
    
    // エンコーディング: 0b01001110xx1nnnnniiiiii1100mmmmm
    uint32_t instr = 0x4E20CC00 | size_q | (vn << 5) | (vm << 16) | vd;
    
    appendInstruction(out, instr);
}

void ARM64SIMD::emitDuplicateElement(std::vector<uint8_t>& out, int vd, int vn, int index, ElementSize elementSize) {
    // DUP Vd.T, Vn.T[index]
    uint32_t imm5 = 0;
    
    switch (elementSize) {
        case ELEM_B:
            imm5 = (index & 0x1F) << 16;  // B format, index 0-31
            break;
        case ELEM_H:
            imm5 = ((index & 0xF) << 1 | 1) << 16;  // H format, index 0-15
            break;
        case ELEM_S:
            imm5 = ((index & 0x7) << 2 | 2) << 16;  // S format, index 0-7
            break;
        case ELEM_D:
            imm5 = ((index & 0x3) << 3 | 4) << 16;  // D format, index 0-3
            break;
        default:
            assert(false && "Unsupported element size for DUP");
    }
    
    // エンコーディング: 0b01001110000iiiiiiiiiii1vvvvvddddd
    uint32_t instr = 0x4E080400 | imm5 | (vn << 5) | vd;
    
    appendInstruction(out, instr);
}

void ARM64SIMD::emitTableLookup(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement) {
    // TBL Vd.T, {Vn.16B}, Vm.T
    uint32_t q = 0;
    
    switch (arrangement) {
        case ARRANGE_8B:
            q = 0 << 30;  // 8B format
            break;
        case ARRANGE_16B:
            q = 1 << 30;  // 16B format
            break;
        default:
            assert(false && "Unsupported vector arrangement for TBL");
    }
    
    // エンコーディング: 0b0Q00111000000000ttttt00vvvvvddddd
    uint32_t len = 0;  // Single register table
    uint32_t instr = 0x0E000000 | q | (len << 13) | (vm << 16) | (vn << 5) | vd;
    
    appendInstruction(out, instr);
}

void ARM64SIMD::emitClearVector(std::vector<uint8_t>& out, int vd, VectorArrangement arrangement) {
    // MOVI Vd.T, #0
    uint32_t q_op = 0;
    
    switch (arrangement) {
        case ARRANGE_8B:
            q_op = 0b00 << 28;  // 8B format, Q=0, op=0
            break;
        case ARRANGE_16B:
            q_op = 0b10 << 28;  // 16B format, Q=1, op=0
            break;
        case ARRANGE_4H:
            q_op = 0b01 << 28;  // 4H format, Q=0, op=1
            break;
        case ARRANGE_8H:
            q_op = 0b11 << 28;  // 8H format, Q=1, op=1
            break;
        case ARRANGE_2S:
            q_op = 0b00 << 28;  // 2S format, Q=0, op=0, cmode=110
            break;
        case ARRANGE_4S:
            q_op = 0b10 << 28;  // 4S format, Q=1, op=0, cmode=110
            break;
        case ARRANGE_2D:
            q_op = 0b10 << 28 | 1 << 17;  // 2D format, Q=1, op=0, cmode=11110
            break;
        default:
            assert(false && "Unsupported vector arrangement for MOVI");
    }
    
    // エンコーディング: 0b0QU01110000000abcdefgh0ijklmnvvvvv
    // すべてのビットパターンを0にする
    uint32_t instr = 0x0F000400 | q_op | vd;
    
    appendInstruction(out, instr);
}

void ARM64SIMD::emitVectorMax(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement) {
    // FMAX Vd.T, Vn.T, Vm.T
    uint32_t size_q = 0;
    
    switch (arrangement) {
        case ARRANGE_2S:
            size_q = 0b00 << 22;  // size=00, Q=0 (2S)
            break;
        case ARRANGE_4S:
            size_q = 0b00 << 22 | 1 << 30;  // size=00, Q=1 (4S)
            break;
        case ARRANGE_2D:
            size_q = 0b01 << 22 | 1 << 30;  // size=01, Q=1 (2D)
            break;
        default:
            assert(false && "Unsupported vector arrangement for FMAX");
    }
    
    // エンコーディング: 0b01001110xx1nnnnniiiiii1111mmmmm
    uint32_t instr = 0x4E20F400 | size_q | (vn << 5) | (vm << 16) | vd;
    
    appendInstruction(out, instr);
}

void ARM64SIMD::emitVectorMin(std::vector<uint8_t>& out, int vd, int vn, int vm, VectorArrangement arrangement) {
    // FMIN Vd.T, Vn.T, Vm.T
    uint32_t size_q = 0;
    
    switch (arrangement) {
        case ARRANGE_2S:
            size_q = 0b00 << 22;  // size=00, Q=0 (2S)
            break;
        case ARRANGE_4S:
            size_q = 0b00 << 22 | 1 << 30;  // size=00, Q=1 (4S)
            break;
        case ARRANGE_2D:
            size_q = 0b01 << 22 | 1 << 30;  // size=01, Q=1 (2D)
            break;
        default:
            assert(false && "Unsupported vector arrangement for FMIN");
    }
    
    // エンコーディング: 0b01001110xx1nnnnniiiiii1111mmmmm
    uint32_t instr = 0x4E20F800 | size_q | (vn << 5) | (vm << 16) | vd;
    
    appendInstruction(out, instr);
}

bool ARM64SIMD::canVectorize(const std::vector<IRInstruction>& loopInsts) {
    // ループのベクトル化可能性を判定
    
    // 1. ループ内の命令数が少なすぎる場合はベクトル化しない
    if (loopInsts.size() < 3) {
        return false;
    }
    
    // 2. ループが配列アクセスとシンプルな算術演算のみからなるか確認
    bool hasArrayAccess = false;
    bool hasArithmetic = false;
    
    for (const auto& inst : loopInsts) {
        switch (inst.opcode) {
            case IROpcode::LoadElement:
            case IROpcode::StoreElement:
                hasArrayAccess = true;
                break;
                
            case IROpcode::Add:
            case IROpcode::Sub:
            case IROpcode::Mul:
                hasArithmetic = true;
                break;
                
            case IROpcode::Call:
            case IROpcode::Branch:
            case IROpcode::BranchCond:
                // 関数呼び出しや複雑な分岐があるとベクトル化できない
                return false;
                
            default:
                // その他の命令はケースバイケースで判断
                break;
        }
    }
    
    // 配列アクセスと算術演算の両方を含むループのみベクトル化
    return hasArrayAccess && hasArithmetic;
}

std::vector<size_t> ARM64SIMD::detectLoopInvariants(const std::vector<IRInstruction>& loopInsts) {
    std::vector<size_t> invariants;
    std::unordered_set<int> definedVars;
    std::unordered_set<int> modifiedVars;
    
    // 1. ループ内で変更される変数を特定
    for (const auto& inst : loopInsts) {
        // 変数を定義する命令を検出
        if (inst.opcode == IROpcode::StoreVar) {
            int varIdx = inst.args[0];
            definedVars.insert(varIdx);
            modifiedVars.insert(varIdx);
        }
    }
    
    // 2. 各命令をチェックして、不変式を特定
    for (size_t i = 0; i < loopInsts.size(); ++i) {
        const auto& inst = loopInsts[i];
        bool isInvariant = true;
        
        // 命令の入力をチェック
        for (size_t j = 1; j < inst.args.size(); ++j) {
            // インデックス変数アクセスなど、ループ内で変更される変数を使用している場合は不変ではない
            int usedVar = inst.args[j];
            if (modifiedVars.count(usedVar) > 0) {
                isInvariant = false;
                break;
            }
        }
        
        // 命令が結果を変数に格納する場合
        if (inst.opcode == IROpcode::StoreVar) {
            // 結果を格納する変数がループ内で変更される場合は不変ではない
            int resultVar = inst.args[0];
            if (definedVars.count(resultVar) > 0) {
                isInvariant = false;
            }
        }
        
        if (isInvariant) {
            invariants.push_back(i);
        }
    }
    
    return invariants;
}

void ARM64SIMD::emitPreloopCode(const std::vector<IRInstruction>& loopInsts, std::vector<uint8_t>& out) {
    // ループ前処理コード
    // 1. ベクトルレジスタの初期化
    
    // V0-V3をクリア (ベクトル累積用)
    emitClearVector(out, V0, ARRANGE_4S);
    emitClearVector(out, V1, ARRANGE_4S);
    emitClearVector(out, V2, ARRANGE_4S);
    emitClearVector(out, V3, ARRANGE_4S);
    
    // 2. ループ不変値をロード (ベクトル複製)
    // 例: 配列ベースアドレスをX0にロード
    // この例では、X0には配列のベースアドレスが既に入っていると仮定
    
    // 3. カウンタ初期化
    // この例では、X1にはループカウンタの初期値が既に入っていると仮定
    
    // 4. ループ長をベクトル長で調整
    // CMP X2, #16
    uint32_t cmp_instr = 0xF100041F;  // CMP X0, #16
    appendInstruction(out, cmp_instr);
    
    // B.LT scalar_loop
    uint32_t b_lt_instr = 0x54000340;  // B.LT +104 (26ワード先)
    appendInstruction(out, b_lt_instr);
    
    // AND X3, X2, #~15 (ベクトル化可能な部分の長さを計算)
    uint32_t and_instr = 0x92400043;  // AND X3, X2, #~15
    appendInstruction(out, and_instr);
}

void ARM64SIMD::emitVectorizedLoopBody(
    const std::vector<IRInstruction>& loopInsts,
    const std::vector<size_t>& invariants,
    std::vector<uint8_t>& out) {
    
    // ベクトル化ループ本体
    
    // 1. ループラベル
    // vector_loop:
    
    // 2. ベクトルデータのロード
    // LDP Q0, Q1, [X0], #32
    uint32_t ldp_instr = 0xACC10400;  // LDP Q0, Q1, [X0], #32
    appendInstruction(out, ldp_instr);
    
    // 3. ベクトル演算 - 例として加算
    // FADD V0.4S, V0.4S, V2.4S
    emitVectorAdd(out, V0, V0, V2, ARRANGE_4S);
    
    // 4. カウンタ更新とループ条件チェック
    // SUBS X3, X3, #16
    uint32_t subs_instr = 0xF1000463;  // SUBS X3, X3, #16
    appendInstruction(out, subs_instr);
    
    // B.GT vector_loop
    uint32_t b_gt_instr = 0x54FFFF8C;  // B.GT -8 (2ワード前)
    appendInstruction(out, b_gt_instr);
    
    // 5. スカラー残余ループへの分岐
    // CBZ X3, end
    uint32_t cbz_instr = 0xB4000083;  // CBZ X3, +16 (4ワード先)
    appendInstruction(out, cbz_instr);
}

void ARM64SIMD::emitPostloopCode(const std::vector<IRInstruction>& loopInsts, std::vector<uint8_t>& out) {
    // ループ後処理コード
    
    // 1. スカラー残余ループ (ベクトル長に満たない残りの要素を処理)
    // scalar_loop:
    
    // LDR S0, [X0], #4
    uint32_t ldr_instr = 0xBC404400;  // LDR S0, [X0], #4
    appendInstruction(out, ldr_instr);
    
    // シングル要素の処理
    // FADD S1, S1, S0
    uint32_t fadd_instr = 0x1E202820;  // FADD S0, S0, S1
    appendInstruction(out, fadd_instr);
    
    // SUBS X2, X2, #1
    uint32_t subs_instr = 0xF1000442;  // SUBS X2, X2, #1
    appendInstruction(out, subs_instr);
    
    // B.GT scalar_loop
    uint32_t b_gt_instr = 0x54FFFF4C;  // B.GT -12 (3ワード前)
    appendInstruction(out, b_gt_instr);
    
    // 2. ベクトル最終結果の水平加算 (ベクトル内の要素を全て足し合わせる)
    // FADDP V0.2S, V0.2S, V0.2S
    uint32_t faddp_instr = 0x5E30D800;  // FADDP V0.2S, V0.2S, V0.2S
    appendInstruction(out, faddp_instr);
    
    // 3. 結果をスカラーレジスタに取り出す
    // FMOV S0, V0.S[0]
    uint32_t fmov_instr = 0x0E003C00;  // FMOV S0, V0.S[0]
    appendInstruction(out, fmov_instr);
    
    // 4. ループ終了
    // end:
}

void ARM64SIMDProcessor::OptimizeVectorLoop(IRNode* loopNode) {
    // ベクトルループの最適化処理
    if (!loopNode || loopNode->GetType() != IRNodeType::Loop) {
        m_logger.Error("無効なループノードが最適化に渡されました");
        return;
    }
    
    // ループボディを取得
    auto* loopBody = static_cast<LoopNode*>(loopNode)->GetBody();
    if (!loopBody) {
        m_logger.Error("ループノードのボディがnullです");
        return;
    }
    
    // ループの特性を分析
    LoopAnalysisResult analysis = AnalyzeLoop(loopNode);
    
    // ベクトル化可能なループかどうかを確認
    if (!analysis.IsVectorizable()) {
        m_logger.Info("ループはベクトル化できません: " + analysis.GetReason());
        return;
    }
    
    // SIMDレジスタの数を決定
    int numRegisters = DetermineOptimalRegisterCount(analysis);
    
    // ベクトル化の種類を選択
    VectorizationStrategy strategy = SelectVectorizationStrategy(analysis, numRegisters);
    
    // 選択された戦略に基づいてループを変換
    switch (strategy) {
        case VectorizationStrategy::FullUnroll:
            EmitFullyUnrolledLoop(loopNode, analysis, numRegisters);
            break;
            
        case VectorizationStrategy::PartialUnroll:
            EmitPartiallyUnrolledLoop(loopNode, analysis, numRegisters);
            break;
            
        case VectorizationStrategy::Pipelined:
            EmitPipelinedLoop(loopNode, analysis, numRegisters);
            break;
            
        case VectorizationStrategy::SVE:
            if (m_features.HasSVE()) {
                EmitSVELoop(loopNode, analysis);
            } else {
                EmitNeonLoop(loopNode, analysis, numRegisters);
            }
            break;
            
        case VectorizationStrategy::Neon:
        default:
            EmitNeonLoop(loopNode, analysis, numRegisters);
            break;
    }
    
    // 最適化統計を更新
    m_stats.numVectorizedLoops++;
    m_stats.totalInstructionsSaved += analysis.EstimatedInstructionsSaved();
}

// Neon SIMDを使用したループ最適化の実装
void ARM64SIMDProcessor::EmitNeonLoop(IRNode* loopNode, const LoopAnalysisResult& analysis, int numRegisters) {
    // ループのコンポーネントを取得
    auto* loop = static_cast<LoopNode*>(loopNode);
    IRNode* init = loop->GetInit();
    IRNode* condition = loop->GetCondition();
    IRNode* update = loop->GetUpdate();
    IRNode* body = loop->GetBody();
    
    // ループの境界を取得
    int64_t lowerBound = analysis.GetLowerBound();
    int64_t upperBound = analysis.GetUpperBound();
    int64_t stride = analysis.GetStride();
    
    // 要素タイプとSIMDレーンサイズに基づいてベクトル長を決定
    int vectorLength = GetVectorLengthForType(analysis.GetElementType());
    
    // プリアンブルの発行（ベクトル処理の前のループ反復）
    EmitLoopPreamble(loopNode, analysis, lowerBound, vectorLength);
    
    // メインベクトルループの発行
    int64_t mainLoopStart = RoundUpToMultiple(lowerBound, vectorLength);
    int64_t mainLoopEnd = upperBound - (upperBound - mainLoopStart) % (vectorLength * numRegisters);
    
    EmitVectorMainLoop(loopNode, analysis, mainLoopStart, mainLoopEnd, vectorLength, numRegisters);
    
    // ループエピローグの発行（残りの反復）
    EmitLoopEpilogue(loopNode, analysis, mainLoopEnd, upperBound);
}

// SVE SIMDを使用したループ最適化の実装
void ARM64SIMDProcessor::EmitSVELoop(IRNode* loopNode, const LoopAnalysisResult& analysis) {
    // SVEのベクトル長は実行時に決定されるため、動的なコード生成が必要
    auto* loop = static_cast<LoopNode*>(loopNode);
    IRNode* body = loop->GetBody();
    
    // ループの境界を取得
    int64_t lowerBound = analysis.GetLowerBound();
    int64_t upperBound = analysis.GetUpperBound();
    int64_t stride = analysis.GetStride();
    
    // 挿入点を作成
    m_assembler.MakeInsertionPointAfter(loopNode);
    
    // SVEレジスタを割り当て
    SVEPRegister pReg = m_assembler.AllocatePRegister();
    SVEZRegister zAccumReg = m_assembler.AllocateZRegister();
    SVEZRegister zIndexReg = m_assembler.AllocateZRegister();
    
    // カウンターレジスタを取得
    Register counterReg = analysis.GetInductionRegister();
    
    // ループの初期設定
    m_assembler.EmitMovImm(counterReg, lowerBound);
    
    // SVEプレディケートレジスタの設定
    // (インデックス + 0,1,2,...) < 上限 を満たす要素をマーク
    m_assembler.EmitSVEWhileLoCondition(pReg, counterReg, upperBound);
    
    // インデックスの初期化（0, 1, 2, ...）
    m_assembler.EmitSVEIndex(zIndexReg, counterReg, stride);
    
    // アキュムレータを初期化（操作によって異なる）
    if (analysis.GetOperationType() == LoopOperationType::Summation) {
        m_assembler.EmitSVEDup(zAccumReg, 0);
    } else if (analysis.GetOperationType() == LoopOperationType::Product) {
        m_assembler.EmitSVEDup(zAccumReg, 1);
    }
    
    // SVEループラベルを生成
    Label sveLoopLabel = m_assembler.CreateLabel("sve_loop");
    m_assembler.EmitBindLabel(sveLoopLabel);
    
    // SVEベクトル操作の生成
    GenerateSVEVectorOperation(body, analysis, pReg, zAccumReg, zIndexReg);
    
    // インデックスを更新
    m_assembler.EmitSVEAddVectorLength(zIndexReg, zIndexReg);
    m_assembler.EmitAddImm(counterReg, counterReg, m_assembler.GetSVEVectorLength());
    
    // ループ条件の更新
    m_assembler.EmitSVEWhileLoCondition(pReg, counterReg, upperBound);
    
    // プレディケートがまだアクティブかチェック
    m_assembler.EmitSVEPTest(pReg);
    m_assembler.EmitBranchNonZero(m_assembler.CreateLabelRef(sveLoopLabel));
    
    // 結果の縮約処理（水平加算など）
    if (analysis.NeedsReduction()) {
        EmitSVEReduction(zAccumReg, analysis.GetOperationType(), analysis.GetResultRegister());
    }
}

// 部分的にアンロールされたループの生成
void ARM64SIMDProcessor::EmitPartiallyUnrolledLoop(IRNode* loopNode, const LoopAnalysisResult& analysis, int unrollFactor) {
    // ループのコンポーネントを取得
    auto* loop = static_cast<LoopNode*>(loopNode);
    IRNode* init = loop->GetInit();
    IRNode* condition = loop->GetCondition();
    IRNode* update = loop->GetUpdate();
    IRNode* body = loop->GetBody();
    
    // ループの境界を取得
    int64_t lowerBound = analysis.GetLowerBound();
    int64_t upperBound = analysis.GetUpperBound();
    int64_t stride = analysis.GetStride();
    
    // ベクトル長を決定
    int vectorLength = GetVectorLengthForType(analysis.GetElementType());
    
    // アンロールされたループの残りの部分を処理するためのループが必要かチェック
    bool needsRemainder = (upperBound - lowerBound) % (unrollFactor * stride) != 0;
    
    // メインループの境界を計算
    int64_t mainLoopEnd = upperBound;
    if (needsRemainder) {
        mainLoopEnd = upperBound - ((upperBound - lowerBound) % (unrollFactor * stride));
    }
    
    // カウンターレジスタを取得
    Register counterReg = analysis.GetInductionRegister();
    
    // カウンターの初期化
    m_assembler.EmitMovImm(counterReg, lowerBound);
    
    // アンロールされたメインループのラベル
    Label mainLoopLabel = m_assembler.CreateLabel("unrolled_main_loop");
    Label mainLoopEnd_label = m_assembler.CreateLabel("unrolled_main_loop_end");
    
    // メインループの開始
    m_assembler.EmitBindLabel(mainLoopLabel);
    
    // ループ条件をチェック
    m_assembler.EmitCompareImm(counterReg, mainLoopEnd);
    m_assembler.EmitBranchGreaterEqual(m_assembler.CreateLabelRef(mainLoopEnd_label));
    
    // アンロールされたループボディを生成
    for (int i = 0; i < unrollFactor; i++) {
        // オリジナルのループボディをクローン
        IRNode* bodyClone = CloneIRNode(body);
        
        // インダクション変数の参照を置き換える
        ReplaceInductionVariableReferences(bodyClone, counterReg, i * stride);
        
        // ボディーを発行
        EmitIRNode(bodyClone);
        
        // 次の反復のためにカウンターを更新（最後の反復以外）
        if (i < unrollFactor - 1) {
            m_assembler.EmitAddImm(counterReg, counterReg, stride);
        }
    }
    
    // カウンターを更新（次のアンロールブロックへ）
    m_assembler.EmitAddImm(counterReg, counterReg, stride);
    
    // ループバック
    m_assembler.EmitBranch(m_assembler.CreateLabelRef(mainLoopLabel));
    
    // メインループの終了
    m_assembler.EmitBindLabel(mainLoopEnd_label);
    
    // 残りの要素を処理（必要な場合）
    if (needsRemainder) {
        EmitLoopRemainder(loopNode, analysis, mainLoopEnd, upperBound);
    }
}

// スカラループの残りの部分を処理
void ARM64SIMDProcessor::EmitLoopRemainder(IRNode* loopNode, const LoopAnalysisResult& analysis, int64_t startBound, int64_t endBound) {
    // ループのコンポーネントを取得
    auto* loop = static_cast<LoopNode*>(loopNode);
    IRNode* body = loop->GetBody();
    
    // カウンターレジスタを取得
    Register counterReg = analysis.GetInductionRegister();
    int64_t stride = analysis.GetStride();
    
    // 残りのループラベル
    Label remainderLoopLabel = m_assembler.CreateLabel("remainder_loop");
    Label remainderLoopEnd = m_assembler.CreateLabel("remainder_loop_end");
    
    // 残りのループを開始
    m_assembler.EmitBindLabel(remainderLoopLabel);
    
    // ループ条件をチェック
    m_assembler.EmitCompareImm(counterReg, endBound);
    m_assembler.EmitBranchGreaterEqual(m_assembler.CreateLabelRef(remainderLoopEnd));
    
    // ボディを発行
    EmitIRNode(body);
    
    // カウンターを更新
    m_assembler.EmitAddImm(counterReg, counterReg, stride);
    
    // ループバック
    m_assembler.EmitBranch(m_assembler.CreateLabelRef(remainderLoopLabel));
    
    // 残りのループの終了
    m_assembler.EmitBindLabel(remainderLoopEnd);
}

// データ型に基づいてベクトル長を決定
int ARM64SIMDProcessor::GetVectorLengthForType(DataType type) {
    switch (type) {
        case DataType::Int8:
        case DataType::UInt8:
            return 16;  // 128ビット / 8ビット = 16レーン
            
        case DataType::Int16:
        case DataType::UInt16:
            return 8;   // 128ビット / 16ビット = 8レーン
            
        case DataType::Int32:
        case DataType::UInt32:
        case DataType::Float32:
            return 4;   // 128ビット / 32ビット = 4レーン
            
        case DataType::Int64:
        case DataType::UInt64:
        case DataType::Float64:
            return 2;   // 128ビット / 64ビット = 2レーン
            
        default:
            return 1;   // 不明な型の場合、安全のためにスカラとして扱う
    }
}

// 整数を指定された値の倍数に切り上げる
int64_t ARM64SIMDProcessor::RoundUpToMultiple(int64_t value, int64_t multiple) {
    return ((value + multiple - 1) / multiple) * multiple;
}

// SVEベクトル操作を生成
void ARM64SIMDProcessor::GenerateSVEVectorOperation(IRNode* bodyNode, const LoopAnalysisResult& analysis,
                                                  SVEPRegister predReg, SVEZRegister accumReg, SVEZRegister indexReg) {
    // ボディのノードタイプに基づいて異なる操作を生成
    switch (bodyNode->GetType()) {
        case IRNodeType::BinaryOp: {
            auto* binOp = static_cast<BinaryOpNode*>(bodyNode);
            BinaryOpType opType = binOp->GetOpType();
            
            // メモリ操作のソースアドレスを取得
            IRNode* lhs = binOp->GetLHS();
            IRNode* rhs = binOp->GetRHS();
            
            // 配列アクセスの場合
            if (IsArrayAccess(lhs) && IsInductionVariableDependent(rhs, analysis)) {
                Register arrayReg = GetAddressRegisterForNode(lhs);
                SVEZRegister dataReg = m_assembler.AllocateZRegister();
                
                // 配列からデータをロード
                m_assembler.EmitSVELoad(dataReg, predReg, arrayReg, indexReg);
                
                // 操作の種類に基づいて計算を実行
                switch (opType) {
                    case BinaryOpType::Add:
                        m_assembler.EmitSVEAdd(accumReg, predReg, accumReg, dataReg);
                        break;
                        
                    case BinaryOpType::Multiply:
                        m_assembler.EmitSVEMul(accumReg, predReg, accumReg, dataReg);
                        break;
                        
                    case BinaryOpType::Maximum:
                        m_assembler.EmitSVEMax(accumReg, predReg, accumReg, dataReg);
                        break;
                        
                    case BinaryOpType::Minimum:
                        m_assembler.EmitSVEMin(accumReg, predReg, accumReg, dataReg);
                        break;
                        
                    // 他の操作も同様に実装
                    default:
                        m_logger.Error("サポートされていないベクトル操作タイプ: " + std::to_string(static_cast<int>(opType)));
                        break;
                }
            }
            // 他のパターンも同様に処理
            break;
        }
        
        // 他のノードタイプも同様に処理
        case IRNodeType::StoreOp: {
            auto* storeOp = static_cast<StoreOpNode*>(bodyNode);
            IRNode* addr = storeOp->GetAddress();
            IRNode* value = storeOp->GetValue();
            
            if (IsArrayAccess(addr) && IsVectorizableValue(value, analysis)) {
                Register arrayReg = GetAddressRegisterForNode(addr);
                SVEZRegister valueReg = LoadValueToSVERegister(value, predReg);
                
                // 配列にデータを保存
                m_assembler.EmitSVEStore(valueReg, predReg, arrayReg, indexReg);
            }
            break;
        }
        
        // 複合ノードの場合は再帰的に処理
        case IRNodeType::Block: {
            auto* block = static_cast<BlockNode*>(bodyNode);
            for (auto& stmt : block->GetStatements()) {
                GenerateSVEVectorOperation(stmt, analysis, predReg, accumReg, indexReg);
            }
            break;
        }
        
        default:
            m_logger.Error("サポートされていないノードタイプ: " + std::to_string(static_cast<int>(bodyNode->GetType())));
            break;
    }
}

// SVE縮約操作を生成（水平加算など）
void ARM64SIMDProcessor::EmitSVEReduction(SVEZRegister dataReg, LoopOperationType opType, Register destReg) {
    switch (opType) {
        case LoopOperationType::Summation:
            m_assembler.EmitSVEAddv(destReg, dataReg);
            break;
            
        case LoopOperationType::Product:
            m_assembler.EmitSVEMulv(destReg, dataReg);
            break;
            
        case LoopOperationType::Maximum:
            m_assembler.EmitSVEMaxv(destReg, dataReg);
            break;
            
        case LoopOperationType::Minimum:
            m_assembler.EmitSVEMinv(destReg, dataReg);
            break;
            
        default:
            m_logger.Error("サポートされていない縮約操作タイプ: " + std::to_string(static_cast<int>(opType)));
            break;
    }
}

} // namespace core
} // namespace aerojs 