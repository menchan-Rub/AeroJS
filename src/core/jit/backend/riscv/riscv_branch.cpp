#include "riscv_branch.h"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace aerojs {
namespace core {

void RISCVBranchManager::appendInstruction(std::vector<uint8_t>& out, uint32_t instruction) {
    // RISC-Vはリトルエンディアン
    out.push_back(instruction & 0xFF);
    out.push_back((instruction >> 8) & 0xFF);
    out.push_back((instruction >> 16) & 0xFF);
    out.push_back((instruction >> 24) & 0xFF);
}

void RISCVBranchManager::emitBranchCond(std::vector<uint8_t>& out, int rs1, int rs2, int32_t offset, BranchCondition condition) {
    // B-type命令: | imm[12|10:5] | rs2 | rs1 | funct3 | imm[4:1|11] | opcode |
    assert(offset % 2 == 0 && "Branch offset must be a multiple of 2");
    assert(offset >= -4096 && offset < 4096 && "Branch offset out of range");
    
    // オフセットを各位置にエンコード
    uint32_t imm12 = ((offset >> 12) & 0x1) << 31;
    uint32_t imm11 = ((offset >> 11) & 0x1) << 7;
    uint32_t imm10_5 = ((offset >> 5) & 0x3F) << 25;
    uint32_t imm4_1 = ((offset >> 1) & 0xF) << 8;
    
    // オペコードと条件コード
    uint32_t opcode = 0x63; // B-type命令
    uint32_t funct3;
    
    switch (condition) {
        case BRANCH_EQ:  funct3 = 0x0; break;
        case BRANCH_NE:  funct3 = 0x1; break;
        case BRANCH_LT:  funct3 = 0x4; break;
        case BRANCH_GE:  funct3 = 0x5; break;
        case BRANCH_LTU: funct3 = 0x6; break;
        case BRANCH_GEU: funct3 = 0x7; break;
        default: assert(false && "Invalid branch condition");
    }
    
    uint32_t instr = opcode | (funct3 << 12) | (rs1 << 15) | (rs2 << 20) | imm12 | imm11 | imm10_5 | imm4_1;
    appendInstruction(out, instr);
}

void RISCVBranchManager::emitJump(std::vector<uint8_t>& out, int rd, int32_t offset) {
    // JAL命令: | imm[20|10:1|11|19:12] | rd | opcode |
    assert(offset % 2 == 0 && "Jump offset must be a multiple of 2");
    assert(offset >= -1048576 && offset < 1048576 && "Jump offset out of range");
    
    // オフセットを各位置にエンコード
    uint32_t imm20 = ((offset >> 20) & 0x1) << 31;
    uint32_t imm19_12 = ((offset >> 12) & 0xFF) << 12;
    uint32_t imm11 = ((offset >> 11) & 0x1) << 20;
    uint32_t imm10_1 = ((offset >> 1) & 0x3FF) << 21;
    
    // オペコード
    uint32_t opcode = 0x6F; // JAL命令
    
    uint32_t instr = opcode | (rd << 7) | imm20 | imm19_12 | imm11 | imm10_1;
    appendInstruction(out, instr);
}

void RISCVBranchManager::emitJumpRegister(std::vector<uint8_t>& out, int rd, int rs1, int32_t offset) {
    // JALR命令: | imm[11:0] | rs1 | funct3 | rd | opcode |
    assert(offset >= -2048 && offset < 2048 && "JALR offset out of range");
    
    // オペコード
    uint32_t opcode = 0x67; // JALR命令
    uint32_t funct3 = 0x0;
    
    uint32_t instr = opcode | (rd << 7) | (funct3 << 12) | (rs1 << 15) | ((offset & 0xFFF) << 20);
    appendInstruction(out, instr);
}

void RISCVBranchManager::emitBranchZero(std::vector<uint8_t>& out, int rs, int32_t offset, bool eq) {
    // BEQZ/BNEZ命令: BEQ/BNE rs, x0, offset
    emitBranchCond(out, rs, 0, offset, eq ? BRANCH_EQ : BRANCH_NE);
}

BranchCondition RISCVBranchManager::invertCondition(BranchCondition condition) {
    switch (condition) {
        case BRANCH_EQ:  return BRANCH_NE;
        case BRANCH_NE:  return BRANCH_EQ;
        case BRANCH_LT:  return BRANCH_GE;
        case BRANCH_GE:  return BRANCH_LT;
        case BRANCH_LTU: return BRANCH_GEU;
        case BRANCH_GEU: return BRANCH_LTU;
        default: assert(false && "Invalid branch condition"); return BRANCH_EQ;
    }
}

void RISCVBranchManager::defineLabel(const std::string& label, size_t pos) {
    labelPositions_[label] = pos;
    resolveBranchesToLabel(nullptr, label);
}

size_t RISCVBranchManager::addBranchToLabel(std::vector<uint8_t>& out, int rs1, int rs2, const std::string& targetLabel, BranchCondition condition) {
    // 現在の位置を記録
    size_t srcPos = out.size();
    
    // ラベルの位置が既に分かっていれば直接分岐命令を生成
    auto it = labelPositions_.find(targetLabel);
    if (it != labelPositions_.end()) {
        int32_t offset = static_cast<int32_t>(it->second - srcPos);
        emitBranchCond(out, rs1, rs2, offset, condition);
        return 0; // 参照は必要ない
    }
    
    // そうでなければ、仮の分岐命令を生成（オフセット0）
    emitBranchCond(out, rs1, rs2, 0, condition);
    
    // 未解決の分岐参照を追加
    BranchRef ref;
    ref.srcPos = srcPos;
    ref.targetLabel = targetLabel;
    ref.cond = condition;
    ref.isConditional = true;
    ref.rs1 = rs1;
    ref.rs2 = rs2;
    branchRefs_.push_back(ref);
    
    return branchRefs_.size() - 1;
}

size_t RISCVBranchManager::addJumpToLabel(std::vector<uint8_t>& out, int rd, const std::string& targetLabel) {
    // 現在の位置を記録
    size_t srcPos = out.size();
    
    // ラベルの位置が既に分かっていれば直接分岐命令を生成
    auto it = labelPositions_.find(targetLabel);
    if (it != labelPositions_.end()) {
        int32_t offset = static_cast<int32_t>(it->second - srcPos);
        emitJump(out, rd, offset);
        return 0; // 参照は必要ない
    }
    
    // そうでなければ、仮の分岐命令を生成（オフセット0）
    emitJump(out, rd, 0);
    
    // 未解決の分岐参照を追加
    BranchRef ref;
    ref.srcPos = srcPos;
    ref.targetLabel = targetLabel;
    ref.isConditional = false;
    ref.rs1 = rd; // 無条件分岐の場合、rs1にrdを保存
    branchRefs_.push_back(ref);
    
    return branchRefs_.size() - 1;
}

void RISCVBranchManager::resolveAllBranches(std::vector<uint8_t>& out) {
    std::vector<BranchRef> unresolvedRefs;
    
    for (const auto& ref : branchRefs_) {
        auto it = labelPositions_.find(ref.targetLabel);
        if (it != labelPositions_.end()) {
            // ラベルの位置が分かった場合、分岐命令をパッチ
            size_t targetPos = it->second;
            int32_t offset = static_cast<int32_t>(targetPos - ref.srcPos);
            
            if (out) {
                patchBranchOffset(out, ref.srcPos, offset, ref.isConditional);
            }
        } else {
            // まだ解決できない参照を保存
            unresolvedRefs.push_back(ref);
            std::cerr << "Warning: Unresolved branch to label: " << ref.targetLabel << std::endl;
        }
    }
    
    // 未解決の参照で更新
    branchRefs_ = std::move(unresolvedRefs);
}

void RISCVBranchManager::resolveBranchesToLabel(std::vector<uint8_t>& out, const std::string& label) {
    auto it = labelPositions_.find(label);
    if (it == labelPositions_.end()) {
        return; // ラベルがまだ定義されていない
    }
    
    size_t targetPos = it->second;
    std::vector<BranchRef> unresolvedRefs;
    
    for (const auto& ref : branchRefs_) {
        if (ref.targetLabel == label) {
            // このラベルへの参照を解決
            int32_t offset = static_cast<int32_t>(targetPos - ref.srcPos);
            
            if (out) {
                patchBranchOffset(out, ref.srcPos, offset, ref.isConditional);
            }
        } else {
            // 他のラベルへの参照を保存
            unresolvedRefs.push_back(ref);
        }
    }
    
    // 未解決の参照で更新
    branchRefs_ = std::move(unresolvedRefs);
}

void RISCVBranchManager::patchBranchOffset(std::vector<uint8_t>& out, size_t pos, int32_t offset, bool isConditional) {
    assert(pos + 4 <= out.size() && "Invalid branch position");
    
    // 現在の命令を取得
    uint32_t instr = static_cast<uint32_t>(out[pos]) |
                    (static_cast<uint32_t>(out[pos + 1]) << 8) |
                    (static_cast<uint32_t>(out[pos + 2]) << 16) |
                    (static_cast<uint32_t>(out[pos + 3]) << 24);
    
    // オペコードと関数コードを保持
    uint32_t opcode = instr & 0x7F;
    uint32_t newInstr;
    
    if (isConditional) {
        // 条件付き分岐 (B-type)
        assert(offset % 2 == 0 && "Branch offset must be a multiple of 2");
        assert(offset >= -4096 && offset < 4096 && "Branch offset out of range");
        
        // オペコードと関数コードを保持するマスク
        uint32_t mask = 0x00000FFF; // funct3, rs1, rs2, opcode
        
        // オフセットを各位置にエンコード
        uint32_t imm12 = ((offset >> 12) & 0x1) << 31;
        uint32_t imm11 = ((offset >> 11) & 0x1) << 7;
        uint32_t imm10_5 = ((offset >> 5) & 0x3F) << 25;
        uint32_t imm4_1 = ((offset >> 1) & 0xF) << 8;
        
        newInstr = (instr & mask) | imm12 | imm11 | imm10_5 | imm4_1;
    } else {
        // 無条件分岐 (JAL)
        assert(offset % 2 == 0 && "Jump offset must be a multiple of 2");
        assert(offset >= -1048576 && offset < 1048576 && "Jump offset out of range");
        
        // オペコードとrdを保持するマスク
        uint32_t mask = 0x00000FFF; // rd, opcode
        
        // オフセットを各位置にエンコード
        uint32_t imm20 = ((offset >> 20) & 0x1) << 31;
        uint32_t imm19_12 = ((offset >> 12) & 0xFF) << 12;
        uint32_t imm11 = ((offset >> 11) & 0x1) << 20;
        uint32_t imm10_1 = ((offset >> 1) & 0x3FF) << 21;
        
        newInstr = (instr & mask) | imm20 | imm19_12 | imm11 | imm10_1;
    }
    
    // 命令を更新
    out[pos] = newInstr & 0xFF;
    out[pos + 1] = (newInstr >> 8) & 0xFF;
    out[pos + 2] = (newInstr >> 16) & 0xFF;
    out[pos + 3] = (newInstr >> 24) & 0xFF;
}

} // namespace core
} // namespace aerojs 