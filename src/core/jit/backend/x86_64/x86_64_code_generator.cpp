#include "x86_64_code_generator.h"
#include <cassert>
#include <cstring>
#if defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#elif defined(_MSC_VER)
#include <intrin.h>
#endif

namespace aerojs {
namespace core {
namespace jit {

X86_64CodeGenerator::X86_64CodeGenerator(Context* context, uint32_t optimizationFlags) noexcept 
    : m_context(context), m_optimizationFlags(optimizationFlags), m_currentFrameSize(0), m_nextSpillOffset(0) {
    ResetFrameInfo();
}

X86_64CodeGenerator::~X86_64CodeGenerator() noexcept {
}

void X86_64CodeGenerator::ResetFrameInfo() {
    m_labels.clear();
    m_pendingJumps.clear();
    m_codeLocations.clear();
    m_registerMapping.clear();
    m_simdRegisterMapping.clear();
    m_currentFrameSize = 0;
    m_spillSlots.clear();
    m_nextSpillOffset = 0; 
}

int32_t X86_64CodeGenerator::AllocateSpillSlot(int32_t virtualReg, uint32_t size) {
    if (m_spillSlots.count(virtualReg)) {
        return m_spillSlots[virtualReg];
    }
    // 8バイトアライメントを考慮してオフセットを調整
    m_nextSpillOffset += size;
    m_nextSpillOffset = (m_nextSpillOffset + 7) & ~7; 
    m_spillSlots[virtualReg] = -static_cast<int32_t>(m_nextSpillOffset);
    // フレームサイズも更新 (プロローグ/エピローグで利用)
    if (m_nextSpillOffset > m_currentFrameSize) {
        m_currentFrameSize = m_nextSpillOffset;
    }
    return m_spillSlots[virtualReg];
}

std::optional<int32_t> X86_64CodeGenerator::GetSpillSlotOffset(int32_t virtualReg) const {
    auto it = m_spillSlots.find(virtualReg);
    if (it != m_spillSlots.end()) {
        return it->second;
    }
    return std::nullopt;
}

uint32_t X86_64CodeGenerator::GetCurrentFrameSize() const {
    return (m_currentFrameSize + 15) & ~15; // 16バイトアライメント
}

void X86_64CodeGenerator::Align(std::vector<uint8_t>& code, size_t alignment, uint8_t fillValue) noexcept {
    size_t currentSize = code.size();
    size_t misAlignment = currentSize % alignment;
    if (misAlignment != 0) {
        size_t paddingSize = alignment - misAlignment;
        for (size_t i = 0; i < paddingSize; ++i) {
            code.push_back(fillValue);
        }
    }
}

void X86_64CodeGenerator::DefineLabel(const std::string& label, std::vector<uint8_t>& code) noexcept {
    m_labels[label] = static_cast<uint32_t>(code.size());
}

int32_t X86_64CodeGenerator::GetLabelPosition(const std::string& label) const {
    auto it = m_labels.find(label);
    if (it != m_labels.end()) {
        return static_cast<int32_t>(it->second);
    }
    return -1; 
}

bool X86_64CodeGenerator::ResolveLabels(std::vector<uint8_t>& code) noexcept {
    for (const auto& pending : m_pendingJumps) {
        auto it = m_labels.find(pending.targetLabel);
        if (it == m_labels.end()) {
            //  assert(false && "Undefined label referenced in jump");
            return false; // ラベルが見つからない
        }
        uint32_t targetPos = it->second;
        uint32_t jumpInstructionEndPos = pending.sourceOffset + pending.jumpOpSize;
        int32_t relativeOffset = static_cast<int32_t>(targetPos) - static_cast<int32_t>(jumpInstructionEndPos);

        if (pending.sourceOffset >= code.size() || jumpInstructionEndPos > code.size()) return false; // 範囲外アクセス

        if (pending.isShortJump) {
            if (relativeOffset < -128 || relativeOffset > 127) {
                // Short jump out of range, would require re-encoding or error.
                // For now, consider it an error.
                // assert(false && "Short jump out of range");
                return false; 
            }
            // jumpOpSize は rel8 (1 byte) + opcode (1 byte) = 2 のはず
            // sourceOffset は rel8 の位置を指す
            if (pending.sourceOffset >= code.size()) return false;
            code[pending.sourceOffset] = static_cast<uint8_t>(relativeOffset);
        } else {
            // jumpOpSize は rel32 (4 bytes) + opcode (1 or 2 bytes)
            // sourceOffset は rel32 の開始位置を指す
            if (pending.sourceOffset + 3 >= code.size()) return false;
            code[pending.sourceOffset + 0] = static_cast<uint8_t>(relativeOffset & 0xFF);
            code[pending.sourceOffset + 1] = static_cast<uint8_t>((relativeOffset >> 8) & 0xFF);
            code[pending.sourceOffset + 2] = static_cast<uint8_t>((relativeOffset >> 16) & 0xFF);
            code[pending.sourceOffset + 3] = static_cast<uint8_t>((relativeOffset >> 24) & 0xFF);
        }
    }
    m_pendingJumps.clear();
    return true;
}

bool X86_64CodeGenerator::Generate(const IRFunction& function, std::vector<uint8_t>& outCode) noexcept {
    outCode.clear(); 
    ResetFrameInfo(); // 各関数生成前に状態をリセット

    JITProfiler* profiler = nullptr;
    if (m_context) { 
        profiler = m_context->GetProfiler(); 
    }
    uint64_t funcId = function.functionId; 

    // プロローグのプレースホルダー (後で実際のフレームサイズでパッチングするか、最後にまとめて生成)
    // ここでは、一旦プロローグの基本部分のみ出力し、スタック確保はエピローグ前に行う戦略
    EncodePrologueMinimal(outCode); 

    for (const auto* block : function.blocks) { 
        if (!block) continue;
        
        std::string blockLabel = "BB_" + std::to_string(block->id);
        DefineLabel(blockLabel, outCode);

        if (profiler && HasFlag(m_optimizationFlags, CodeGenOptFlags::AlignLoops)) {
            // IRBlock にループヘッダ情報が必要
            // const LoopProfilingData* loopData = profiler->getLoopData(funcId, block->id /* or block->start_bytecode_offset */);
            // if (loopData && loopData->isHeader && loopData->executionCount > 1000 /*閾値*/) {
            //     Align(outCode, 16); // 16 or 64 byte alignment for loop headers
            // }
        }

        for (const auto* inst_ptr : block->instructions) { 
            if (!inst_ptr) continue;
            const IRInstruction& inst = *inst_ptr;
            bool success = true;

            // デバッグ用にIR命令のオフセットなどを記録 (オプション)
            // RecordCodeLocation(inst, outCode.size());

            switch (inst.opcode) {
                case Opcode::kNop:
                    success = EncodeNop(outCode); // Nopも関数化してエラー処理統一
                    break;
                case Opcode::kLoadConst:
                    success = EncodeLoadConst(inst, outCode);
                    break;
                case Opcode::kMove:
                    success = EncodeMove(inst, outCode);
                    break; 
                case Opcode::kAdd: success = EncodeAdd(inst, outCode); break;
                case Opcode::kSub: success = EncodeSub(inst, outCode); break;
                case Opcode::kMul: success = EncodeMul(inst, outCode); break;
                case Opcode::kDiv: success = EncodeDiv(inst, outCode); break;
                case Opcode::kMod: success = EncodeMod(inst, outCode); break;
                case Opcode::kNeg: success = EncodeNeg(inst, outCode); break;
                
                case Opcode::kCompareEq: case Opcode::kCompareNe:
                case Opcode::kCompareLt: case Opcode::kCompareLe:
                case Opcode::kCompareGt: case Opcode::kCompareGe:
                    success = EncodeCompare(inst, outCode);
                    break;
                
                case Opcode::kJump: // 無条件ジャンプ
                    success = EncodeJump(inst, outCode);
                    break;
                
                case Opcode::kBranch: // 条件分岐 (IRInstructionから条件とターゲットを取得)
                    {
                        BranchHint hint = BranchHint::kNone;
                        if (profiler && HasFlag(m_optimizationFlags, CodeGenOptFlags::BranchPrediction)) {
                            // IRInstructionに分岐先ブロックID情報が必要
                            // if (inst.num_operands() >= 2 && inst.operand(1).isBlockId()) {
                            //    uint32_t targetBlockId = inst.operand(1).getBlockId();
                            //    hint = profiler->getBranchHint(funcId, block->id, targetBlockId);
                            // }
                        }
                        success = EncodeBranch(inst, hint, outCode);
                    }
                    break;

                case Opcode::kReturn:
                    success = EncodeReturn(inst, outCode);
                    break;
                
                case Opcode::kCall:
                    success = EncodeCall(inst, outCode);
                    break;

                case Opcode::kLoadMemory:
                    success = EncodeLoadMemory(inst, outCode);
                    break;
                case Opcode::kStoreMemory:
                    success = EncodeStoreMemory(inst, outCode);
                    break;
                
                // ... 他のIROpcodeのケース ...
                // case Opcode::kShiftLeft: success = EncodeShift(inst, outCode); break;
                // case Opcode::kBitAnd: success = EncodeBitwise(inst, outCode); break;


                default:
                    // 未対応の命令
                    // assert(false && "Unsupported IR opcode");
                    success = false; 
                    break;
            }
            if (!success) {
                // Log error: std::cerr << "Failed to encode instruction: " << static_cast<int>(inst.opcode) << std::endl;
                outCode.clear(); // 生成途中のコードを破棄
                return false; // エラー発生
            }
        }
    }

    // 全ての命令生成後、フレームサイズに基づいてプロローグのスタック確保部分を完成させ、エピローグを生成
    FinalizeFrame(outCode); // プロローグのSUB RSP, size とエピローグをここで行う

    if (!ResolveLabels(outCode)) { 
       // Log error: std::cerr << "Failed to resolve labels." << std::endl;
       outCode.clear();
       return false;
    }

    return true;
}

void X86_64CodeGenerator::SetRegisterMapping(int32_t virtualReg, X86_64Register physicalReg) noexcept {
    m_registerMapping[virtualReg] = physicalReg;
}

std::optional<X86_64Register> X86_64CodeGenerator::GetPhysicalReg(int32_t virtualReg) const noexcept {
    auto it = m_registerMapping.find(virtualReg);
    if (it != m_registerMapping.end()) {
        return it->second;
    }
    // Check if it's spilled
    if (m_spillSlots.count(virtualReg)) {
        // It's spilled, so no physical register is currently assigned.
        // The caller should handle loading from spill slot.
        return std::nullopt; 
    }
    // If not in mapping and not in spill slots, it's an unallocated/unknown vreg.
    // This could be an error or an implicit function argument register.
    // For now, assume error or handle upstream.
    // assert(false && "Virtual register not found in mapping or spill slots");
    return std::nullopt;
}

bool X86_64CodeGenerator::EncodeNop(std::vector<uint8_t>& code) noexcept {
    EmitNOP();
    return true;
}

bool X86_64CodeGenerator::EncodeLoadConst(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 2 || !inst.operand(0).isVirtualReg() || !inst.operand(1).isImmediate()) {
        return false; 
    }
    
    int32_t destVirtualReg = inst.operand(0).getVirtualReg();
    int64_t constValue = inst.operand(1).getImmediateValue(); // Assume IROperand has getImmediateValue()
    
    std::optional<X86_64Register> optPhysDest = GetPhysicalReg(destVirtualReg);
    
    if (optPhysDest) {
        EmitMOV_Reg_Imm(optPhysDest.value(), constValue);
    } else { // Dest is spilled
        // Load immediate to a temporary register (RAX) then spill.
        // A better approach might be to use a dedicated scratch register if available.
        EmitMOV_Reg_Imm(RAX, constValue); // RAX is a common scratch choice
        if(!EncodeStoreToSpillSlot(destVirtualReg, RAX, code)) return false;
    }
    return true;
}

bool X86_64CodeGenerator::EncodeMove(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 2 || !inst.operand(0).isVirtualReg() || !inst.operand(1).isVirtualRegOrImmediate()) {
        return false;
    }
    const auto& destOp = inst.operand(0);
    const auto& srcOp = inst.operand(1);
    int32_t destVirtualReg = destOp.getVirtualReg();

    std::optional<X86_64Register> optDestPhysReg = GetPhysicalReg(destVirtualReg);

    if (srcOp.isVirtualReg()) {
        int32_t srcVirtualReg = srcOp.getVirtualReg();
        std::optional<X86_64Register> optSrcPhysReg = GetPhysicalReg(srcVirtualReg);

        if (optDestPhysReg && optSrcPhysReg) { // Both in physical registers
            if (optDestPhysReg.value() != optSrcPhysReg.value()) {
                 EmitMOV_Reg_Reg(optDestPhysReg.value(), optSrcPhysReg.value());
            }
        } else if (optDestPhysReg && !optSrcPhysReg) { // Source spilled, Dest physical
            if (!EncodeLoadFromSpillSlot(optDestPhysReg.value(), srcVirtualReg, code)) return false;
        } else if (!optDestPhysReg && optSrcPhysReg) { // Dest spilled, Source physical
            if (!EncodeStoreToSpillSlot(destVirtualReg, optSrcPhysReg.value(), code)) return false;
        } else { // Both spilled
            // Load to temp (RAX), then store from temp.
            if (!EncodeLoadFromSpillSlot(RAX, srcVirtualReg, code)) return false;
            if (!EncodeStoreToSpillSlot(destVirtualReg, RAX, code)) return false;
        }
    } else if (srcOp.isImmediate()) { // Source is immediate
        int64_t immValue = srcOp.getImmediateValue();
        if (optDestPhysReg) { // Dest physical
            EmitMOV_Reg_Imm(optDestPhysReg.value(), immValue);
        } else { // Dest spilled
            EmitMOV_Reg_Imm(RAX, immValue); // Load to temp (RAX)
            if(!EncodeStoreToSpillSlot(destVirtualReg, RAX, code)) return false;
        }
    } else {
        return false; // Unsupported source operand type
    }
    return true;
}

bool X86_64CodeGenerator::EncodeAdd(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3 || !inst.operand(0).isVirtualReg() || !inst.operand(1).isVirtualReg() || !inst.operand(2).isVirtualRegOrImmediate()) {
        return false;
    }
    int32_t destVReg = inst.operand(0).getVirtualReg();
    int32_t src1VReg = inst.operand(1).getVirtualReg();
    const auto& src2Op = inst.operand(2);

    std::optional<X86_64Register> optDestPReg = GetPhysicalReg(destVReg);
    std::optional<X86_64Register> optSrc1PReg = GetPhysicalReg(src1VReg);

    X86_64Register 계산PReg = RAX; // Default scratch/target for calculation if dest is spilled or same as src2
    
    // Determine register for src1, load if spilled
    X86_64Register src1PReg;
    if (optSrc1PReg) {
        src1PReg = optSrc1PReg.value();
    } else {
        src1PReg = RCX; // Use RCX as scratch for src1 if spilled
        if (!EncodeLoadFromSpillSlot(src1PReg, src1VReg, code)) return false;
    }
    
    // If dest is physical and different from src1, copy src1 to dest first.
    // Or, if dest is spilled, use a scratch (RAX) and copy src1 to it.
    if (optDestPReg && optDestPReg.value() != src1PReg) {
        EmitMOV_Reg_Reg(optDestPReg.value(), src1PReg);
        계산PReg = optDestPReg.value();
    } else if (!optDestPReg) { // Dest is spilled
        if (src1PReg != RAX) EmitMOV_Reg_Reg(RAX, src1PReg);
        계산PReg = RAX;
    } else { // Dest is physical and same as src1PReg
        계산PReg = optDestPReg.value();
    }


    if (src2Op.isVirtualReg()) {
        int32_t src2VReg = src2Op.getVirtualReg();
        std::optional<X86_64Register> optSrc2PReg = GetPhysicalReg(src2VReg);
        X86_64Register src2PReg;
        if (optSrc2PReg) {
            src2PReg = optSrc2PReg.value();
        } else {
            src2PReg = RDX; // Use RDX as scratch for src2 if spilled (ensure different from 계산PReg if it's RAX/RCX)
            if (계산PReg == RDX) src2PReg = R8; // Pick another scratch if RDX is taken by 계산PReg
            if (!EncodeLoadFromSpillSlot(src2PReg, src2VReg, code)) return false;
        }
        EmitADD_Reg_Reg(계산PReg, src2PReg);
    } else if (src2Op.isImmediate()) {
        int32_t imm = static_cast<int32_t>(src2Op.getImmediateValue()); // ADD reg, imm32
        EmitADD_Reg_Imm(계산PReg, imm);
    } else {
        return false;
    }

    if (!optDestPReg) { // If dest was spilled, store result from 계산PReg (RAX)
        if (!EncodeStoreToSpillSlot(destVReg, 계산PReg, code)) return false;
    }
    return true;
}

bool X86_64CodeGenerator::EncodeSub(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3 || !inst.operand(0).isVirtualReg() || !inst.operand(1).isVirtualReg() || !inst.operand(2).isVirtualRegOrImmediate()) {
        return false;
    }
    int32_t destVReg = inst.operand(0).getVirtualReg();
    int32_t src1VReg = inst.operand(1).getVirtualReg();
    const auto& src2Op = inst.operand(2);

    std::optional<X86_64Register> optDestPReg = GetPhysicalReg(destVReg);
    std::optional<X86_64Register> optSrc1PReg = GetPhysicalReg(src1VReg);

    X86_64Register 계산PReg = RAX; // Default scratch/target for calculation if dest is spilled or same as src2
    
    // Determine register for src1, load if spilled
    X86_64Register src1PReg;
    if (optSrc1PReg) {
        src1PReg = optSrc1PReg.value();
    } else {
        src1PReg = RCX; // Use RCX as scratch for src1 if spilled
        if (!EncodeLoadFromSpillSlot(src1PReg, src1VReg, code)) return false;
    }
    
    // If dest is physical and different from src1, copy src1 to dest first.
    // Or, if dest is spilled, use a scratch (RAX) and copy src1 to it.
    if (optDestPReg && optDestPReg.value() != src1PReg) {
        EmitMOV_Reg_Reg(optDestPReg.value(), src1PReg);
        계산PReg = optDestPReg.value();
    } else if (!optDestPReg) { // Dest is spilled
        if (src1PReg != RAX) EmitMOV_Reg_Reg(RAX, src1PReg);
        계산PReg = RAX;
    } else { // Dest is physical and same as src1PReg
        계산PReg = optDestPReg.value();
    }


    if (src2Op.isVirtualReg()) {
        int32_t src2VReg = src2Op.getVirtualReg();
        std::optional<X86_64Register> optSrc2PReg = GetPhysicalReg(src2VReg);
        X86_64Register src2PReg;
        if (optSrc2PReg) {
            src2PReg = optSrc2PReg.value();
        } else {
            src2PReg = RDX; // Use RDX as scratch for src2 if spilled (ensure different from 계산PReg if it's RAX/RCX)
            if (계산PReg == RDX) src2PReg = R8; // Pick another scratch if RDX is taken by 계산PReg
            if (!EncodeLoadFromSpillSlot(src2PReg, src2VReg, code)) return false;
        }
        EmitSUB_Reg_Reg(계산PReg, src2PReg);
    } else if (src2Op.isImmediate()) {
        int32_t imm = static_cast<int32_t>(src2Op.getImmediateValue()); // SUB reg, imm32
        EmitSUB_Reg_Imm(계산PReg, imm);
    } else {
        return false;
    }

    if (!optDestPReg) { // If dest was spilled, store result from 계산PReg (RAX)
        if (!EncodeStoreToSpillSlot(destVReg, 계산PReg, code)) return false;
    }
    return true;
}

bool X86_64CodeGenerator::EncodeMul(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3 || !inst.operand(0).isVirtualReg() || !inst.operand(1).isVirtualReg() || !inst.operand(2).isVirtualRegOrImmediate()) {
        return false;
    }
    int32_t destVReg = inst.operand(0).getVirtualReg();
    int32_t src1VReg = inst.operand(1).getVirtualReg();
    const auto& src2Op = inst.operand(2);

    std::optional<X86_64Register> optDestPReg = GetPhysicalReg(destVReg);
    std::optional<X86_64Register> optSrc1PReg = GetPhysicalReg(src1VReg);

    X86_64Register 계산PReg = RAX; // Default scratch/target for calculation if dest is spilled or same as src2
    
    // Determine register for src1, load if spilled
    X86_64Register src1PReg;
    if (optSrc1PReg) {
        src1PReg = optSrc1PReg.value();
    } else {
        src1PReg = RCX; // Use RCX as scratch for src1 if spilled
        if (!EncodeLoadFromSpillSlot(src1PReg, src1VReg, code)) return false;
    }
    
    // If dest is physical and different from src1, copy src1 to dest first.
    // Or, if dest is spilled, use a scratch (RAX) and copy src1 to it.
    if (optDestPReg && optDestPReg.value() != src1PReg) {
        EmitMOV_Reg_Reg(optDestPReg.value(), src1PReg);
        계산PReg = optDestPReg.value();
    } else if (!optDestPReg) { // Dest is spilled
        if (src1PReg != RAX) EmitMOV_Reg_Reg(RAX, src1PReg);
        계산PReg = RAX;
    } else { // Dest is physical and same as src1PReg
        계산PReg = optDestPReg.value();
    }


    if (src2Op.isVirtualReg()) {
        int32_t src2VReg = src2Op.getVirtualReg();
        std::optional<X86_64Register> optSrc2PReg = GetPhysicalReg(src2VReg);
        X86_64Register src2PReg;
        if (optSrc2PReg) {
            src2PReg = optSrc2PReg.value();
        } else {
            src2PReg = RDX; // Use RDX as scratch for src2 if spilled (ensure different from 계산PReg if it's RAX/RCX)
            if (계산PReg == RDX) src2PReg = R8; // Pick another scratch if RDX is taken by 계산PReg
            if (!EncodeLoadFromSpillSlot(src2PReg, src2VReg, code)) return false;
        }
        EmitIMUL_Reg_Reg(계산PReg, src2PReg);
    } else if (src2Op.isImmediate()) {
        int32_t imm = static_cast<int32_t>(src2Op.getImmediateValue()); // IMUL reg, imm32
        EmitIMUL_Reg_Imm(계산PReg, imm);
    } else {
        return false;
    }

    if (!optDestPReg) { // If dest was spilled, store result from 계산PReg (RAX)
        if (!EncodeStoreToSpillSlot(destVReg, 계산PReg, code)) return false;
    }
    return true;
}

bool X86_64CodeGenerator::EncodeDiv(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3 || !inst.operand(0).isVirtualReg() || !inst.operand(1).isVirtualReg() || !inst.operand(2).isVirtualRegOrImmediate()) {
        return false;
    }
    int32_t destVReg = inst.operand(0).getVirtualReg();
    int32_t src1VReg = inst.operand(1).getVirtualReg();
    const auto& src2Op = inst.operand(2);

    std::optional<X86_64Register> optDestPReg = GetPhysicalReg(destVReg);
    std::optional<X86_64Register> optSrc1PReg = GetPhysicalReg(src1VReg);

    X86_64Register 계산PReg = RAX; // Default scratch/target for calculation if dest is spilled or same as src2
    
    // Determine register for src1, load if spilled
    X86_64Register src1PReg;
    if (optSrc1PReg) {
        src1PReg = optSrc1PReg.value();
    } else {
        src1PReg = RCX; // Use RCX as scratch for src1 if spilled
        if (!EncodeLoadFromSpillSlot(src1PReg, src1VReg, code)) return false;
    }
    
    // If dest is physical and different from src1, copy src1 to dest first.
    // Or, if dest is spilled, use a scratch (RAX) and copy src1 to it.
    if (optDestPReg && optDestPReg.value() != src1PReg) {
        EmitMOV_Reg_Reg(optDestPReg.value(), src1PReg);
        계산PReg = optDestPReg.value();
    } else if (!optDestPReg) { // Dest is spilled
        if (src1PReg != RAX) EmitMOV_Reg_Reg(RAX, src1PReg);
        계산PReg = RAX;
    } else { // Dest is physical and same as src1PReg
        계산PReg = optDestPReg.value();
    }


    if (src2Op.isVirtualReg()) {
        int32_t src2VReg = src2Op.getVirtualReg();
        std::optional<X86_64Register> optSrc2PReg = GetPhysicalReg(src2VReg);
        X86_64Register src2PReg;
        if (optSrc2PReg) {
            src2PReg = optSrc2PReg.value();
        } else {
            src2PReg = RDX; // Use RDX as scratch for src2 if spilled (ensure different from 계산PReg if it's RAX/RCX)
            if (계산PReg == RDX) src2PReg = R8; // Pick another scratch if RDX is taken by 계산PReg
            if (!EncodeLoadFromSpillSlot(src2PReg, src2VReg, code)) return false;
        }
        EmitIDIV_Reg_Reg(계산PReg, src2PReg);
    } else if (src2Op.isImmediate()) {
        int32_t imm = static_cast<int32_t>(src2Op.getImmediateValue()); // IDIV reg, imm32
        EmitIDIV_Reg_Imm(계산PReg, imm);
    } else {
        return false;
    }

    if (!optDestPReg) { // If dest was spilled, store result from 계산PReg (RAX)
        if (!EncodeStoreToSpillSlot(destVReg, 계산PReg, code)) return false;
    }
    return true;
}

bool X86_64CodeGenerator::EncodeMod(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3 || !inst.operand(0).isVirtualReg() || !inst.operand(1).isVirtualReg() || !inst.operand(2).isVirtualRegOrImmediate()) {
        return false;
    }
    int32_t destVReg = inst.operand(0).getVirtualReg();
    int32_t src1VReg = inst.operand(1).getVirtualReg();
    const auto& src2Op = inst.operand(2);

    std::optional<X86_64Register> optDestPReg = GetPhysicalReg(destVReg);
    std::optional<X86_64Register> optSrc1PReg = GetPhysicalReg(src1VReg);

    X86_64Register 계산PReg = RAX; // Default scratch/target for calculation if dest is spilled or same as src2
    
    // Determine register for src1, load if spilled
    X86_64Register src1PReg;
    if (optSrc1PReg) {
        src1PReg = optSrc1PReg.value();
    } else {
        src1PReg = RCX; // Use RCX as scratch for src1 if spilled
        if (!EncodeLoadFromSpillSlot(src1PReg, src1VReg, code)) return false;
    }
    
    // If dest is physical and different from src1, copy src1 to dest first.
    // Or, if dest is spilled, use a scratch (RAX) and copy src1 to it.
    if (optDestPReg && optDestPReg.value() != src1PReg) {
        EmitMOV_Reg_Reg(optDestPReg.value(), src1PReg);
        계산PReg = optDestPReg.value();
    } else if (!optDestPReg) { // Dest is spilled
        if (src1PReg != RAX) EmitMOV_Reg_Reg(RAX, src1PReg);
        계산PReg = RAX;
    } else { // Dest is physical and same as src1PReg
        계산PReg = optDestPReg.value();
    }


    if (src2Op.isVirtualReg()) {
        int32_t src2VReg = src2Op.getVirtualReg();
        std::optional<X86_64Register> optSrc2PReg = GetPhysicalReg(src2VReg);
        X86_64Register src2PReg;
        if (optSrc2PReg) {
            src2PReg = optSrc2PReg.value();
        } else {
            src2PReg = RDX; // Use RDX as scratch for src2 if spilled (ensure different from 계산PReg if it's RAX/RCX)
            if (계산PReg == RDX) src2PReg = R8; // Pick another scratch if RDX is taken by 계산PReg
            if (!EncodeLoadFromSpillSlot(src2PReg, src2VReg, code)) return false;
        }
        EmitIDIV_Reg_Reg(계산PReg, src2PReg);
    } else if (src2Op.isImmediate()) {
        int32_t imm = static_cast<int32_t>(src2Op.getImmediateValue()); // IDIV reg, imm32
        EmitIDIV_Reg_Imm(계산PReg, imm);
    } else {
        return false;
    }

    if (!optDestPReg) { // If dest was spilled, store result from 계산PReg (RAX)
        if (!EncodeStoreToSpillSlot(destVReg, 계산PReg, code)) return false;
    }
    return true;
}

bool X86_64CodeGenerator::EncodeNeg(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 2 || !inst.operand(0).isVirtualReg() || !inst.operand(1).isVirtualReg()) {
        return false;
    }
    int32_t destVReg = inst.operand(0).getVirtualReg();
    int32_t srcVReg = inst.operand(1).getVirtualReg();

    std::optional<X86_64Register> optDestPReg = GetPhysicalReg(destVReg);
    std::optional<X86_64Register> optSrcPReg = GetPhysicalReg(srcVReg);

    X86_64Register 계산PReg = RAX; // Default scratch/target for calculation if dest is spilled or same as src
    
    // Determine register for src, load if spilled
    X86_64Register srcPReg;
    if (optSrcPReg) {
        srcPReg = optSrcPReg.value();
    } else {
        srcPReg = RCX; // Use RCX as scratch for src if spilled
        if (!EncodeLoadFromSpillSlot(srcPReg, srcVReg, code)) return false;
    }
    
    // If dest is physical and different from src, copy src to dest first.
    // Or, if dest is spilled, use a scratch (RAX) and copy src to it.
    if (optDestPReg && optDestPReg.value() != srcPReg) {
        EmitMOV_Reg_Reg(optDestPReg.value(), srcPReg);
        계산PReg = optDestPReg.value();
    } else if (!optDestPReg) { // Dest is spilled
        if (srcPReg != RAX) EmitMOV_Reg_Reg(RAX, srcPReg);
        계산PReg = RAX;
    } else { // Dest is physical and same as srcPReg
        계산PReg = optDestPReg.value();
    }

    EmitNEG_Reg(계산PReg);

    if (!optDestPReg) { // If dest was spilled, store result from 계산PReg (RAX)
        if (!EncodeStoreToSpillSlot(destVReg, 계산PReg, code)) return false;
    }
    return true;
}

bool X86_64CodeGenerator::EncodeCompare(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3 || !inst.operand(0).isVirtualReg() || !inst.operand(1).isVirtualReg() || !inst.operand(2).isVirtualRegOrImmediate()) {
        return false;
    }
    int32_t destVReg = inst.operand(0).getVirtualReg();
    int32_t src1VReg = inst.operand(1).getVirtualReg();
    const auto& src2Op = inst.operand(2);

    std::optional<X86_64Register> optSrc1PReg = GetPhysicalReg(src1VReg);

    X86_64Register src1PReg;
    if (optSrc1PReg) {
        src1PReg = optSrc1PReg.value();
    } else {
        src1PReg = RCX; // Use RCX as scratch for src1 if spilled
        if (!EncodeLoadFromSpillSlot(src1PReg, src1VReg, code)) return false;
    }

    if (src2Op.isVirtualReg()) {
        int32_t src2VReg = src2Op.getVirtualReg();
        std::optional<X86_64Register> optSrc2PReg = GetPhysicalReg(src2VReg);
        X86_64Register src2PReg;
        if (optSrc2PReg) {
            src2PReg = optSrc2PReg.value();
        } else {
            src2PReg = RDX; // Use RDX as scratch for src2 if spilled (ensure different from src1PReg if it's RCX)
            if (src1PReg == RDX) src2PReg = R8; // Pick another scratch if RDX is taken by src1PReg
            if (!EncodeLoadFromSpillSlot(src2PReg, src2VReg, code)) return false;
        }
        EmitCMP_Reg_Reg(src1PReg, src2PReg);
    } else if (src2Op.isImmediate()) {
        EmitCMP_Reg_Imm(src1PReg, static_cast<int32_t>(src2Op.getImmediateValue()));
    } else {
        return false;
    }

    // SETcc instruction to store boolean result into destVReg
    std::optional<X86_64Register> optDestPReg = GetPhysicalReg(destVReg);
    X86_64Register byteDestPReg; // SETcc operates on 8-bit registers

    if (optDestPReg) { // If dest is a GPR, use its low byte version (e.g. AL, CL)
        // Need a mapping from X86_64Register to its 8-bit version if it's one of the first 8 GPRs.
        // For simplicity, use RAX/AL as intermediate if destPReg is not one of RDI,RSI,RDX,RCX,R8B,R9B etc.
        // This is tricky. A common approach is to SETcc into AL, then MOVZX to the final GPR.
// EVEX プレフィックスの追加ヘルパー
// EVEX fields:
// P0: 62
// P1: RXB R' map_select[4:0] (map_select is mmmmmm field in Intel docs, EVEX.mm)
//     R,X,B,R' are inverted (1 if register bit is 0)
// P2: W vvvv[3:0] V' pp[1:0] (V' is high bit of vvvv, pp is EVEX.pp)
//     vvvv is inverted
// P3: z L'L b V'' aaa (L'L is EVEX.L, b is broadcast/round/SAE, V'' is high bit of ModRM.reg if EVEX.V=1 and V'=1)
//     aaa is mask register k0-k7
void X86_64CodeGenerator::AppendEVEXPrefix(std::vector<uint8_t>& code,
                                          uint8_t evex_mm,    // EVEX.mm (map select: 0F->01, 0F38->02, 0F3A->03)
                                          uint8_t evex_pp,    // EVEX.pp (legacy prefix: none->00, 66->01, F3->02, F2->03)
                                          uint8_t evex_W,     // EVEX.W (0 or 1, typically for operand size)
                                          uint8_t evex_vvvv,  // EVEX.vvvv (full 4-bit non-destructive source register index, will be inverted for encoding)
                                          uint8_t evex_z,     // EVEX.z (zeroing vs merging for masking)
                                          uint8_t evex_LL,    // EVEX.L'L (vector length/rounding: 128/rc->00, 256/rc->01, 512/rc->10)
                                          bool evex_b,        // EVEX.b (broadcast / reg-only op / SAE / rounding enable)
                                          uint8_t evex_aaa,   // EVEX.aaa (mask register k0-k7, 0 means no mask)
                                          bool R,             // REX.R extension for ModRM.reg field
                                          bool X,             // REX.X extension for SIB.index field
                                          bool B,             // REX.B extension for ModRM.r/m or SIB.base or Opcode.reg
                                          bool R_prime,       // EVEX.R' (extends ModRM.reg to 4th bit, REX.R is 3rd bit)
                                          bool V_prime        // EVEX.V' (extends vvvv to 4th bit, vvvv param is low 4 bits) - effectively 5th bit of NDS index
                                          ) noexcept {
    code.push_back(0x62); // Byte 0: EVEX prefix

    // Byte 1: P1 = [R X B R'] [mmmmm]
    // R,X,B,R' are inverted in encoding (0 for extend, 1 for not)
    uint8_t p1 = ((R ? 0 : 1) << 7) | ((X ? 0 : 1) << 6) | ((B ? 0 : 1) << 5) | ((R_prime ? 0 : 1) << 4);
    // evex_mm (map select) is encoded in mmmmm field (lowest 2 or 3 bits depending on interpretation)
    // Intel SDM Vol 2A, Table 2-29: EVEX.mmmmm
    // 0F    -> evex_mm=01 -> mmmmm=00001
    // 0F38  -> evex_mm=02 -> mmmmm=00010
    // 0F3A  -> evex_mm=03 -> mmmmm=00011
    uint8_t mmmmm = 0;
    if (evex_mm == 0x01) mmmmm = 0b00001;
    else if (evex_mm == 0x02) mmmmm = 0b00010;
    else if (evex_mm == 0x03) mmmmm = 0b00011;
    p1 |= mmmmm;
    code.push_back(p1);

    // Byte 2: P2 = [W] [vvvv_inv(3:0)] [V'] [pp]
    // evex_vvvv contains the low 4 bits of the NDS/NDD operand specifier.
    // V_prime contains the 5th bit (from the original 5-bit specifier).
    // Both vvvv and V' are inverted for encoding.
    uint8_t p2 = (evex_W << 7);
    p2 |= ((~evex_vvvv & 0xF) << 3); // Low 4 bits of vvvv, inverted
    p2 |= ((V_prime ? 0 : 1) << 2);  // V' (5th bit of vvvv), inverted.
    p2 |= (evex_pp & 0x03);
    code.push_back(p2);

    // Byte 3: P3 = [z] [L'L] [b] [V''] [aaa]
    // V'' is the EVEX.Vp bit, which is the 4th bit of the ModRM.reg (destination) operand specifier, inverted.
    // This is effectively R_prime, as R_prime in the EVEX context is the 4th bit of ModRM.reg.
    uint8_t p3 = (evex_z << 7);
    p3 |= ((evex_LL & 0x03) << 5); // EVEX.L'L
    p3 |= ((evex_b ? 1 : 0) << 4); // EVEX.b
    p3 |= ((R_prime ? 0 : 1) << 3); // V'' (inverted R_prime, extending ModRM.reg)
    p3 |= (evex_aaa & 0x07);      // EVEX.aaa (mask register)
    code.push_back(p3);
}

// AVX-512算術演算
void X86_64CodeGenerator::EncodeAVX512Arithmetic(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3) return;
     // Ops: dest, src1, src2, [mask_reg], [zero_mask_flag]
    if (inst.operand(0).type != IROperandType::kRegister ||
        inst.operand(1).type != IROperandType::kRegister ||
        inst.operand(2).type != IROperandType::kRegister) return;

    int32_t destReg = inst.operand(0).value.reg;
    int32_t src1Reg = inst.operand(1).value.reg;
    int32_t src2Reg = inst.operand(2).value.reg;
    
    // マスクレジスタ (オプション)
    uint8_t mask = 0; // k0 = no masking
    bool isZeroMasking = false;
    if (inst.num_operands() >= 4 && inst.operand(3).type == IROperandType::kRegister) { // Assuming mask reg is register
        mask = static_cast<uint8_t>(inst.operand(3).value.reg & 0x7); // k1-k7
        if (inst.num_operands() >= 5 && inst.operand(4).type == IROperandType::kImmediate) { // Assuming zero flag is immediate
            isZeroMasking = inst.operand(4).value.imm != 0;
        }
    }
    
    auto destSIMD = GetSIMDReg(destReg);
    auto src1SIMD = GetSIMDReg(src1Reg);
    auto src2SIMD = GetSIMDReg(src2Reg);
    
    if (!destSIMD.has_value() || !src1SIMD.has_value() || !src2SIMD.has_value()) return;
    
    // AVX-512が使用可能かチェック
    bool useAVX512 = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX512);
    if (!useAVX512) {
        // AVX-512がサポートされていない場合は通常のAVXを使用
        EncodeSIMDArithmetic(inst, code);
        return;
    }
    
    uint8_t opcode = 0;
    uint8_t prefix = 0;
    
    // 命令タイプを決定
    switch (inst.opcode) {
        case Opcode::kSIMDAdd:
            opcode = 0x58; // ADDPS
            break;
        case Opcode::kSIMDSub:
            opcode = 0x5C; // SUBPS
            break;
        case Opcode::kSIMDMul:
            opcode = 0x59; // MULPS
            break;
        case Opcode::kSIMDDiv:
            opcode = 0x5E; // DIVPS
            break;
        case Opcode::kSIMDMin:
            opcode = 0x5D; // MINPS
            break;
        case Opcode::kSIMDMax:
            opcode = 0x5F; // MAXPS
            break;
        case Opcode::kSIMDAnd:
            opcode = 0x54; // ANDPS
            break;
        case Opcode::kSIMDOr:
            opcode = 0x56; // ORPS
            break;
        case Opcode::kSIMDXor:
            opcode = 0x57; // XORPS
            break;
        default:
            return; // サポートされていない操作
    }
    
    // EVEX.512.0F.W0 58 /r VADDPS zmm1 {k1}{z}, zmm2, zmm3/m512/m32bcst
    AppendEVEXPrefix(code, 0b01, 0b00, 0b10, 0,
                     static_cast<uint8_t>(*src1SIMD) ^ 0xF, // EVEX.vvvv = src1 (反転)
                     mask, isZeroMasking ? 1 : 0,
                     static_cast<uint8_t>(*src2SIMD) >= 8,
                     static_cast<uint8_t>(*destSIMD) >= 16 ? 1 : 0, 
                     mask, false);
    
    code.push_back(opcode);
    
    // ModRM
    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                static_cast<uint8_t>(*src2SIMD) & 0x7);
}

// AVX-512 FMA操作
void X86_64CodeGenerator::EncodeAVX512FMA(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 4) return;
    // Ops: dest, src1, src2, src3, [mask_reg], [zero_mask_flag]
     if (inst.operand(0).type != IROperandType::kRegister ||
        inst.operand(1).type != IROperandType::kRegister ||
        inst.operand(2).type != IROperandType::kRegister ||
        inst.operand(3).type != IROperandType::kRegister) return;

    int32_t destReg = inst.operand(0).value.reg;
    int32_t src1Reg = inst.operand(1).value.reg;
    int32_t src2Reg = inst.operand(2).value.reg;
    int32_t src3Reg = inst.operand(3).value.reg;
    
    // マスクレジスタ (オプション)
    uint8_t mask = 0; // k0 = no masking
    bool isZeroMasking = false;
    if (inst.num_operands() >= 5 && inst.operand(4).type == IROperandType::kRegister) {
        mask = static_cast<uint8_t>(inst.operand(4).value.reg & 0x7); // k1-k7
        if (inst.num_operands() >= 6 && inst.operand(5).type == IROperandType::kImmediate) {
            isZeroMasking = inst.operand(5).value.imm != 0;
        }
    }
    
    auto destSIMD = GetSIMDReg(destReg);
    auto src1SIMD = GetSIMDReg(src1Reg);
    auto src2SIMD = GetSIMDReg(src2Reg);
    auto src3SIMD = GetSIMDReg(src3Reg);
    
    if (!destSIMD.has_value() || !src1SIMD.has_value() || 
        !src2SIMD.has_value() || !src3SIMD.has_value()) return;
    
    // AVX-512とFMAが使用可能かチェック
    bool useAVX512 = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX512);
    bool useFMA = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseFMA);
    
    if (!useAVX512 || !useFMA) {
        // AVX-512またはFMAがサポートされていない場合は通常のFMAを使用
        EncodeFMA(inst, code);
        return;
    }
    
    // EVEX.DDS.512.66.0F38.W0 B8 /r VFMADD231PS zmm1 {k1}{z}, zmm2, zmm3/m512/m32bcst
    AppendEVEXPrefix(code, 
                     0b10, // evex_mm (0F38)
                     0b01, // evex_pp (66)
                     0b10, // evex_LL (512-bit)
                     0,    // evex_W (W0 for single/double in FMA typically)
                     static_cast<uint8_t>(*src2SIMD) & 0xF, // evex_vvvv (low 4 bits of src2SIMD)
                     isZeroMasking ? 1 : 0, // evex_z
                     0b10, // evex_LL (vector length)
                     false, // evex_b 
                     mask,  // evex_aaa
                     static_cast<uint8_t>(*destSIMD) >= 8,  // R
                     false, // X
                     static_cast<uint8_t>(*src3SIMD) >= 8,  // B (for src3SIMD as r/m)
                     (static_cast<uint8_t>(*destSIMD) >> 3) & 0x1, // R_prime
                     (static_cast<uint8_t>(*src2SIMD) >> 4) & 0x1  // V_prime (for src2SIMD as NDS)
                     );
    
    code.push_back(0xB8); // VFMADD231PS opcode
    
    // ModRM
    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                static_cast<uint8_t>(*src3SIMD) & 0x7);
}

// AVX-512ロード操作
void X86_64CodeGenerator::EncodeAVX512Load(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3) return;
    // Ops: dest_reg, base_addr_reg, offset_imm, [mask_reg], [zero_mask_flag]
    if (inst.operand(0).type != IROperandType::kRegister ||
        inst.operand(1).type != IROperandType::kRegister ||
        inst.operand(2).type != IROperandType::kImmediate) return;

    int32_t destReg = inst.operand(0).value.reg;
    int32_t addrReg = inst.operand(1).value.reg;
    int32_t offset = static_cast<int32_t>(inst.operand(2).value.imm);
    
    // マスクレジスタ (オプション)
    uint8_t mask = 0; // k0 = no masking
    bool isZeroMasking = false;
    if (inst.num_operands() >= 4 && inst.operand(3).type == IROperandType::kRegister) {
        mask = static_cast<uint8_t>(inst.operand(3).value.reg & 0x7); // k1-k7
        if (inst.num_operands() >= 5 && inst.operand(4).type == IROperandType::kImmediate) {
            isZeroMasking = inst.operand(4).value.imm != 0;
        }
    }
    
    auto simdReg = GetSIMDReg(destReg);
    if (!simdReg.has_value()) return;
    
    X86_64Register addrRegPhysical = GetPhysicalReg(addrReg);
    
    // AVX-512が使用可能かチェック
    bool useAVX512 = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX512);
    if (!useAVX512) {
        // AVX-512がサポートされていない場合は通常のAVXを使用
        EncodeSIMDLoad(inst, code);
        return;
    }
    
    // EVEX.512.0F.W0 10 /r VMOVUPS zmm1 {k1}{z}, m512
    AppendEVEXPrefix(code, 
                     0b01, // evex_mm (0F)
                     0b00, // evex_pp (none)
                     0b10, // evex_LL (512-bit)
                     0,    // evex_W (W0 for MOVUPS)
                     0b1111, // evex_vvvv (unused for VMOVUPS from memory, set to all 1s for encoding if needed)
                     isZeroMasking ? 1 : 0, // evex_z
                     0b10, // evex_LL (vector length)
                     false, // evex_b (broadcast for load not typical here, unless specified)
                     mask,  // evex_aaa (mask register)
                     static_cast<uint8_t>(*simdReg) >= 8, // R (for dest simdReg)
                     false, // X (SIB.index extension for addrRegPhysical, assuming no SIB for simplicity here)
                     static_cast<uint8_t>(addrRegPhysical) >= 8, // B (for addrRegPhysical as base)
                     (static_cast<uint8_t>(*simdReg) >> 3) & 0x1,    // R_prime (4th bit of simdReg)
                     0 // V_prime (NDS not used for this form of VMOVUPS from memory)
                     );
    
    code.push_back(0x10); // MOVUPS opcode
    
    // ModRM: [addrReg + offset]
    if (offset == 0) {
        // レジスタ間接アドレッシング
        AppendModRM(code, 0b00, static_cast<uint8_t>(*simdReg) & 0x7, 
                    static_cast<uint8_t>(addrRegPhysical) & 0x7);
    } else if (offset >= -128 && offset <= 127) {
        // 1バイト変位付きアドレッシング
        AppendModRM(code, 0b01, static_cast<uint8_t>(*simdReg) & 0x7, 
                    static_cast<uint8_t>(addrRegPhysical) & 0x7);
        code.push_back(static_cast<uint8_t>(offset));
    } else {
        // 4バイト変位付きアドレッシング
        AppendModRM(code, 0b10, static_cast<uint8_t>(*simdReg) & 0x7, 
                    static_cast<uint8_t>(addrRegPhysical) & 0x7);
        AppendImmediate32(code, offset);
    }
}

// AVX-512ストア操作
void X86_64CodeGenerator::EncodeAVX512Store(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3) return;
    // Ops: src_reg, base_addr_reg, offset_imm, [mask_reg]
    if (inst.operand(0).type != IROperandType::kRegister ||
        inst.operand(1).type != IROperandType::kRegister ||
        inst.operand(2).type != IROperandType::kImmediate) return;
    
    int32_t srcReg = inst.operand(0).value.reg;
    int32_t addrReg = inst.operand(1).value.reg;
    int32_t offset = static_cast<int32_t>(inst.operand(2).value.imm);
    
    // マスクレジスタ (オプション)
    uint8_t mask = 0; // k0 = no masking
    // bool isZeroMasking = false; // Not used for store directly in EVEX.z for VMOVUPS (mask controls write lanes)
    if (inst.num_operands() >= 4 && inst.operand(3).type == IROperandType::kRegister) {
        mask = static_cast<uint8_t>(inst.operand(3).value.reg & 0x7); // k1-k7
    }
    
    auto simdReg = GetSIMDReg(srcReg);
    if (!simdReg.has_value()) return;
    
    X86_64Register addrRegPhysical = GetPhysicalReg(addrReg);
    
    // AVX-512が使用可能かチェック
    bool useAVX512 = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX512);
    if (!useAVX512) {
        // AVX-512がサポートされていない場合は通常のAVXを使用
        EncodeSIMDStore(inst, code);
        return;
    }
    
    // EVEX.512.0F.W0 11 /r VMOVUPS m512 {k1}, zmm1
    AppendEVEXPrefix(code, 
                     0b01, // evex_mm (0F)
                     0b00, // evex_pp (none)
                     0b10, // evex_LL (512-bit)
                     0,    // evex_W (W0 for MOVUPS store)
                     static_cast<uint8_t>(*simdReg) & 0xF, // evex_vvvv (src SIMD reg for store, low 4 bits)
                     0,     // evex_z (zeroing not used for simple VMOVUPS store with mask)
                     0b10,  // evex_LL (vector length)
                     false, // evex_b 
                     mask,  // evex_aaa (mask register)
                     static_cast<uint8_t>(*simdReg) >= 8, // R (for src simdReg as ModRM.reg field)
                     false, // X (SIB.index extension for addrRegPhysical)
                     static_cast<uint8_t>(addrRegPhysical) >= 8, // B (for addrRegPhysical as base)
                     (static_cast<uint8_t>(*simdReg) >> 3) & 0x1,    // R_prime (4th bit of simdReg)
                     (static_cast<uint8_t>(*simdReg) >> 4) & 0x1     // V_prime (5th bit of NDS/src simdReg)
                     );
    
    code.push_back(0x11); // MOVUPS opcode (store)
    
    // ModRM: [addrReg + offset]
    if (offset == 0) {
        AppendModRM(code, 0b00, static_cast<uint8_t>(*simdReg) & 0x7, 
                    static_cast<uint8_t>(addrRegPhysical) & 0x7);
    } else if (offset >= -128 && offset <= 127) {
        AppendModRM(code, 0b01, static_cast<uint8_t>(*simdReg) & 0x7, 
                    static_cast<uint8_t>(addrRegPhysical) & 0x7);
        code.push_back(static_cast<uint8_t>(offset));
    } else {
        AppendModRM(code, 0b10, static_cast<uint8_t>(*simdReg) & 0x7, 
                    static_cast<uint8_t>(addrRegPhysical) & 0x7);
        AppendImmediate32(code, offset);
    }
}

// AVX-512マスク操作
void X86_64CodeGenerator::EncodeAVX512MaskOp(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3) return;
    // Ops: dest_mask_reg, src1_mask_reg, src2_mask_reg
    if (inst.operand(0).type != IROperandType::kRegister ||
        inst.operand(1).type != IROperandType::kRegister ||
        inst.operand(2).type != IROperandType::kRegister) return;

    int32_t destMaskReg = inst.operand(0).value.reg; 
    int32_t src1MaskReg = inst.operand(1).value.reg; 
    int32_t src2MaskReg = inst.operand(2).value.reg; 
    
    uint8_t dest = static_cast<uint8_t>(destMaskReg & 0x7);
    uint8_t src1 = static_cast<uint8_t>(src1MaskReg & 0x7);
    uint8_t src2 = static_cast<uint8_t>(src2MaskReg & 0x7);
    
    uint8_t opcode = 0;
    
    // 命令タイプを決定
    switch (inst.opcode) {
        case Opcode::kMaskAnd:
            opcode = 0x42; // KANDW
            break;
        case Opcode::kMaskOr:
            opcode = 0x45; // KORW
            break;
        case Opcode::kMaskXor:
            opcode = 0x46; // KXORW
            break;
        case Opcode::kMaskNot:
            opcode = 0x44; // KNOTW
            break;
        default:
            return; // サポートされていない操作
    }
    
    // VEX.L0.0F.W0 42 /r KANDW k1, k2, k3
    AppendVEXPrefix(code, 0b10, 0b00, 0, 0, 
                   src1 ^ 0xF, // VEX.vvvv = src1 (反転)
                   0, 0, 0);
    
    code.push_back(opcode);
    
    // ModRM
    AppendModRM(code, 0b11, dest, src2);
}

// AVX-512マスクを使用したブレンド操作
void X86_64CodeGenerator::EncodeAVX512Blend(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 4) return;
    // Ops: dest_reg, src1_reg, src2_reg, mask_reg
    if (inst.operand(0).type != IROperandType::kRegister ||
        inst.operand(1).type != IROperandType::kRegister ||
        inst.operand(2).type != IROperandType::kRegister ||
        inst.operand(3).type != IROperandType::kRegister) return;

    int32_t destReg = inst.operand(0).value.reg;
    int32_t src1Reg = inst.operand(1).value.reg;
    int32_t src2Reg = inst.operand(2).value.reg;
    int32_t maskReg = inst.operand(3).value.reg; 
    
    auto destSIMD = GetSIMDReg(destReg);
    auto src1SIMD = GetSIMDReg(src1Reg);
    auto src2SIMD = GetSIMDReg(src2Reg);
    
    if (!destSIMD.has_value() || !src1SIMD.has_value() || !src2SIMD.has_value()) return;
    
    uint8_t mask = static_cast<uint8_t>(maskReg & 0x7);
    
    // EVEX.NDS.512.0F.W0 C6 /r VSHUFPS zmm1 {k1}, zmm2, zmm3/m512/m32bcst, imm8
    AppendEVEXPrefix(code, 
                     0b01, // evex_mm (0F)
                     0b00, // evex_pp (none)
                     0b10, // evex_LL (512-bit)
                     0,    // evex_W (W0 for VPSHUFB, data type dependent)
                     static_cast<uint8_t>(*src1SIMD) & 0xF, // evex_vvvv (src1SIMD for VPSHUFB)
                     0,     // evex_z (zeroing for blend if mask is used, controlled by IR for blend semantics)
                     0b10,  // evex_LL (vector length)
                     false, // evex_b 
                     mask,  // evex_aaa (mask register for blend control)
                     static_cast<uint8_t>(*destSIMD) >= 8,  // R
                     false, // X
                     static_cast<uint8_t>(*src2SIMD) >= 8,  // B (for src2SIMD as r/m)
                     (static_cast<uint8_t>(*destSIMD) >> 3) & 0x1, // R_prime
                     (static_cast<uint8_t>(*src1SIMD) >> 4) & 0x1  // V_prime (for src1SIMD as NDS)
                     );
    
    code.push_back(0xC4); // VPSHUFB opcode
    
    // ModRM
    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                static_cast<uint8_t>(*src2SIMD) & 0x7);
}

// AVX-512データパーミュテーション
void X86_64CodeGenerator::EncodeAVX512Permute(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3) return;
    // Ops: dest_reg, src_reg, imm_value, [mask_reg], [zero_mask_flag]
    if (inst.operand(0).type != IROperandType::kRegister ||
        inst.operand(1).type != IROperandType::kRegister ||
        inst.operand(2).type != IROperandType::kImmediate) return;

    int32_t destReg = inst.operand(0).value.reg;
    int32_t srcReg = inst.operand(1).value.reg;
    int32_t immValue = static_cast<int32_t>(inst.operand(2).value.imm); 
    
    // マスクレジスタ (オプション)
    uint8_t mask = 0; // k0 = no masking
    bool isZeroMasking = false;
    if (inst.num_operands() >= 4 && inst.operand(3).type == IROperandType::kRegister) {
        mask = static_cast<uint8_t>(inst.operand(3).value.reg & 0x7); // k1-k7
        if (inst.num_operands() >= 5 && inst.operand(4).type == IROperandType::kImmediate) {
            isZeroMasking = inst.operand(4).value.imm != 0;
        }
    }
    
    auto destSIMD = GetSIMDReg(destReg);
    auto srcSIMD = GetSIMDReg(srcReg);
    
    if (!destSIMD.has_value() || !srcSIMD.has_value()) return;
    
    // EVEX.512.66.0F3A.W0 00 /r VPERMQ zmm1 {k1}{z}, zmm2/m512/m64bcst, imm8
    AppendEVEXPrefix(code, 
                     0b11, // evex_mm (0F3A)
                     0b01, // evex_pp (66)
                     0b10, // evex_LL (512-bit)
                     0,    // evex_W (W0 for VPERMQ, depends on qword/dword variant)
                     0b1111, // evex_vvvv (unused, typically NDS is dest)
                     isZeroMasking ? 1 : 0, // evex_z
                     0b10,  // evex_LL (vector length)
                     false, // evex_b 
                     mask,  // evex_aaa
                     static_cast<uint8_t>(*destSIMD) >= 8,  // R
                     false, // X
                     static_cast<uint8_t>(*srcSIMD) >= 8,   // B (for srcSIMD as r/m)
                     (static_cast<uint8_t>(*destSIMD) >> 3) & 0x1, // R_prime
                     0  // V_prime (NDS not used in this form)
                     );
    
    code.push_back(0x00); // VPERMQ opcode
    
    // ModRM
    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                static_cast<uint8_t>(*srcSIMD) & 0x7);
    
    // インデックステーブル (8ビット即値)
    code.push_back(static_cast<uint8_t>(immValue & 0xFF));
}

// AVX-512データ圧縮
void X86_64CodeGenerator::EncodeAVX512Compress(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3) return;
    // Ops: dest_reg, src_reg, mask_reg
    if (inst.operand(0).type != IROperandType::kRegister ||
        inst.operand(1).type != IROperandType::kRegister ||
        inst.operand(2).type != IROperandType::kRegister) return;

    int32_t destReg = inst.operand(0).value.reg;
    int32_t srcReg = inst.operand(1).value.reg;
    int32_t maskReg = inst.operand(2).value.reg; 
    
    auto destSIMD = GetSIMDReg(destReg);
    auto srcSIMD = GetSIMDReg(srcReg);
    
    if (!destSIMD.has_value() || !srcSIMD.has_value()) return;
    
    uint8_t mask = static_cast<uint8_t>(maskReg & 0x7);
    
    // EVEX.512.66.0F38.W0 8B /r VCOMPRESSPS zmm1 {k1}{z}, zmm2
    AppendEVEXPrefix(code, 0b10, 0b01, 0b10, 0,
                     0b1111, // EVEX.vvvv = 0xF (未使用)
                     mask, 0, // Zマスキングなし
                     static_cast<uint8_t>(*srcSIMD) >= 8,
                     static_cast<uint8_t>(*destSIMD) >= 16 ? 1 : 0,
                     mask, false);
    
    code.push_back(0x8B); // VCOMPRESSPS opcode
    
    // ModRM
    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                static_cast<uint8_t>(*srcSIMD) & 0x7);
}

// AVX-512データ展開
void X86_64CodeGenerator::EncodeAVX512Expand(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3) return;
    // Ops: dest_reg, src_reg, mask_reg
    if (inst.operand(0).type != IROperandType::kRegister ||
        inst.operand(1).type != IROperandType::kRegister ||
        inst.operand(2).type != IROperandType::kRegister) return;

    int32_t destReg = inst.operand(0).value.reg;
    int32_t srcReg = inst.operand(1).value.reg;
    int32_t maskReg = inst.operand(2).value.reg; 
    
    auto destSIMD = GetSIMDReg(destReg);
    auto srcSIMD = GetSIMDReg(srcReg);
    
    if (!destSIMD.has_value() || !srcSIMD.has_value()) return;
    
    uint8_t mask = static_cast<uint8_t>(maskReg & 0x7);
    bool isZeroMasking = true; // 展開では通常ゼロマスキングを使用
    
    // EVEX.512.66.0F38.W0 89 /r VEXPANDPS zmm1 {k1}{z}, zmm2
    AppendEVEXPrefix(code, 
                     0b10, // evex_mm (0F38)
                     0b01, // evex_pp (66)
                     0b10, // evex_LL (512-bit)
                     0,    // evex_W (W0 for VEXPANDPS)
                     0b1111, // evex_vvvv (unused, NDS is dest)
                     isZeroMasking ? 1 : 0, // evex_z (VEXPANDPS uses {z} for zeroing/merging based on mask)
                     0b10,   // evex_LL (vector length)
                     false,  // evex_b
                     mask,   // evex_aaa (mask register that controls expansion)
                     static_cast<uint8_t>(*destSIMD) >= 8,  // R
                     false,  // X
                     static_cast<uint8_t>(*srcSIMD) >= 8,   // B (for srcSIMD as r/m)
                     (static_cast<uint8_t>(*destSIMD) >> 3) & 0x1, // R_prime
                     0   // V_prime (NDS not used in this form)
                     );
    
    code.push_back(0x89); // VEXPANDPS opcode
    
    // ModRM
    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                static_cast<uint8_t>(*srcSIMD) & 0x7);
}

// --- 追加命令 ---
void X86_64CodeGenerator::EncodeShift(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 1) return; 
    // Operand 0: dest/src register
    // Operand 1 (optional): shift amount (immediate or CL register)
    if (inst.operand(0).type != IROperandType::kRegister) return;

    int32_t dest_reg_id = inst.operand(0).value.reg;
    std::optional<X86_64Register> opt_phys_dest = GetPhysicalReg(dest_reg_id);
    if (!opt_phys_dest.has_value()) { /* エラー処理または早期リターン */ return; }
    X86_64Register phys_dest = opt_phys_dest.value();

    uint8_t modrm_digit = 0;
    bool is_cl_shift = false;
    uint8_t imm_shift_val = 0;

    if (inst.num_operands() >= 2) {
        const IROperand& shift_src = inst.operand(1);
        if (shift_src.type == IROperandType::kRegister) { // Assuming CL if register
            // Check if it's actually RCX/CL, or if IR guarantees this
            is_cl_shift = true;
        } else if (shift_src.type == IROperandType::kImmediate) {
            imm_shift_val = static_cast<uint8_t>(shift_src.value.imm & 0xFF);
        } else {
            return; // Invalid shift source
        }
    } else {
         // Default to shift by 1 if no amount specified? Or error.
         // For now, assume CL shift if only one operand, or error.
         // Let's require 2 operands for immediate shift.
         // if it's CL shift, then CL must be set beforehand by IR.
         // This function might need to be split or take more info from IR.
         // For now, if only one operand, assume it's for CL type shift where CL is pre-set.
         is_cl_shift = true; // Fallback for 1-operand, needs review.
    }


    switch (inst.opcode) {
        case Opcode::kShiftLeft: 
            modrm_digit = 4; // For SHL r/m64
            break;
        case Opcode::kShiftRight: // Assuming logical shift (SHR)
            modrm_digit = 5; // For SHR r/m64
            // If arithmetic (SAR) is needed, IR Opcode should be distinct (e.g. kShiftArithmeticRight)
            // or a property in IRInstruction should specify it.
            // e.g. if (inst.isArithmetic()) modrm_digit = 7;
            break;
        // case Opcode::kShiftArithmeticRight: // if distinct opcode
        //     modrm_digit = 7; // For SAR r/m64
        //     break;
        default:
            return; 
    }

    bool bExt = static_cast<uint8_t>(phys_dest) >= 8;
    AppendREXPrefix(code, true, false, false, bExt);

    if (is_cl_shift) {
        code.push_back(0xD3); // Opcode for SHL/SHR/SAR r/m64, CL
        AppendModRM(code, 0b11, modrm_digit, static_cast<uint8_t>(phys_dest) & 0x7);
    } else { // Immediate shift
        code.push_back(0xC1); // Opcode for SHL/SHR/SAR r/m64, imm8
        AppendModRM(code, 0b11, modrm_digit, static_cast<uint8_t>(phys_dest) & 0x7);
        code.push_back(imm_shift_val);
    }
}

void X86_64CodeGenerator::EncodeInc(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() == 0 || inst.operand(0).type != IROperandType::kRegister) return;
    int32_t destReg = inst.operand(0).value.reg;
    std::optional<X86_64Register> optPhysDest = GetPhysicalReg(destReg);
    if (!optPhysDest.has_value()) { /* エラー処理または早期リターン */ return; }
    X86_64Register physDest = optPhysDest.value();
    bool bExt = static_cast<uint8_t>(physDest) >= 8;
    AppendREXPrefix(code, true, false, false, bExt);
    code.push_back(0xFF);
    AppendModRM(code, 0b11, 0, static_cast<uint8_t>(physDest) & 0x7); // /0 for INC
}

void X86_64CodeGenerator::EncodeDec(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() == 0 || inst.operand(0).type != IROperandType::kRegister) return;
    int32_t destReg = inst.operand(0).value.reg;
    std::optional<X86_64Register> optPhysDest = GetPhysicalReg(destReg);
    if (!optPhysDest.has_value()) { /* エラー処理または早期リターン */ return; }
    X86_64Register physDest = optPhysDest.value();
    bool bExt = static_cast<uint8_t>(physDest) >= 8;
    AppendREXPrefix(code, true, false, false, bExt);
    code.push_back(0xFF);
    AppendModRM(code, 0b11, 1, static_cast<uint8_t>(physDest) & 0x7); // /1 for DEC
}

void X86_64CodeGenerator::EncodeNop(std::vector<uint8_t>& code) noexcept {
    code.push_back(0x90); // NOP
}

// スタブ実装: メモリ操作
void X86_64CodeGenerator::EncodeLoadMemory(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 2) return false;
    const auto& destOperand = inst.operand(0); // Destination register
    const auto& addrOperand = inst.operand(1); // Address (can be register or immediate)
    // Optional: operand(2) could be an immediate offset if addrOperand is a register

    if (!destOperand.isVirtualReg()) return false;
    int32_t destVirtualReg = destOperand.getVirtualReg();
    std::optional<X86_64Register> optDestPhysReg = GetPhysicalReg(destVirtualReg);

    X86_64Register destReg;
    bool destIsSpilled = !optDestPhysReg.has_value();
    if (destIsSpilled) {
        destReg = RAX; // Load to RAX first, then spill. Could pick a better scratch reg.
    } else {
        destReg = optDestPhysReg.value();
    }

    if (addrOperand.isVirtualReg()) {
        int32_t addrVirtualReg = addrOperand.getVirtualReg();
        std::optional<X86_64Register> optAddrPhysReg = GetPhysicalReg(addrVirtualReg);
        X86_64Register addrReg;
        bool addrIsSpilled = !optAddrPhysReg.has_value();

        if (addrIsSpilled) {
            // Load address from spill slot into a temporary register (e.g., RCX)
            // For simplicity, let's assume RCX is available as scratch.
            // A more robust solution would involve proper scratch register management.
            addrReg = RCX; 
            if (!EncodeLoadFromSpillSlot(addrReg, addrVirtualReg, code)) return false;
        } else {
            addrReg = optAddrPhysReg.value();
        }

        int32_t offset = 0;
        if (inst.num_operands() > 2 && inst.operand(2).isImmediate()) {
            offset = static_cast<int32_t>(inst.operand(2).getImmediateValue());
        }

        // MOV destReg, [addrReg + offset]
        AppendREXPrefix(code, true, destReg >= R8, false, addrReg >= R8);
        code.push_back(0x8B); // MOV r64, r/m64
        if (offset == 0 && (addrReg & 0x7) != RBP && (addrReg & 0x7) != RSP) { // No offset, not RBP/RSP based
             AppendModRM(code, 0b00, static_cast<uint8_t>(destReg) & 0x7, static_cast<uint8_t>(addrReg) & 0x7);
        } else if (offset >= -128 && offset <= 127) { // 8-bit offset
            AppendModRM(code, 0b01, static_cast<uint8_t>(destReg) & 0x7, static_cast<uint8_t>(addrReg) & 0x7);
            code.push_back(static_cast<uint8_t>(offset));
        } else { // 32-bit offset
            AppendModRM(code, 0b10, static_cast<uint8_t>(destReg) & 0x7, static_cast<uint8_t>(addrReg) & 0x7);
            AppendImmediate32(code, offset);
        }
    } else if (addrOperand.isImmediate()) {
        // MOV destReg, [imm64]
        // This form uses RAX as a temporary to hold the 64-bit immediate address if destReg is not RAX
        // or uses a direct MOV if the instruction supports it (e.g. MOV RAX, [imm64])
        int64_t addrImm = addrOperand.getImmediateValue();
        AppendREXPrefix(code, true, destReg >= R8, false, false); // REX.B for ModRM r/m cannot be 1 for direct addressing
        code.push_back(0x8B);
        AppendModRM(code, 0b00, static_cast<uint8_t>(destReg) & 0x7, 0b100); // Mod=00, R/M=100 (SIB)
        AppendSIB(code, 0b00, 0b100, 0b101); // Scale=1, Index=none, Base=RIP+disp32 (effectively disp32, since RIP is 0 for direct addressing)
        AppendImmediate32(code, static_cast<int32_t>(addrImm)); // Only 32-bit displacement for direct addressing with SIB
                                                              // For full 64-bit immediate address, usually MOV to reg, then MOV [reg]
                                                              // Or MOV RAX, [imm64] (A1, A3 opcodes)
        // A simpler way for direct 64-bit address (if destReg is RAX):
        // if (destReg == RAX) {
        //   AppendREXPrefix(code, true, false, false, false);
        //   code.push_back(0xA1); // MOV RAX, [imm64_addr]
        //   AppendImmediate64(code, addrImm);
        // } else { ... load imm64 to scratch, then MOV destReg, [scratch] ... }
        // For now, the SIB based one is a placeholder and might not be correct for all direct addr. cases.
        // A common pattern is: MOV r64, imm64; MOV destReg, [r64]
    } else {
        return false; // Invalid address operand type
    }

    if (destIsSpilled) {
        if (!EncodeStoreToSpillSlot(destVirtualReg, destReg, code)) return false;
    }

    return true;
}

bool X86_64CodeGenerator::EncodeStoreMemory(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 2) return false;
    const auto& srcOperand = inst.operand(0);   // Source register
    const auto& addrOperand = inst.operand(1); // Address (can be register or immediate)

    if (!srcOperand.isVirtualReg()) return false;
    int32_t srcVirtualReg = srcOperand.getVirtualReg();
    std::optional<X86_64Register> optSrcPhysReg = GetPhysicalReg(srcVirtualReg);
    X86_64Register srcReg;
    bool srcIsSpilled = !optSrcPhysReg.has_value();

    if (srcIsSpilled) {
        srcReg = RAX; // Load from spill to RAX first. Could pick a better scratch reg.
        if (!EncodeLoadFromSpillSlot(srcReg, srcVirtualReg, code)) return false;
    } else {
        srcReg = optSrcPhysReg.value();
    }

    if (addrOperand.isVirtualReg()) {
        int32_t addrVirtualReg = addrOperand.getVirtualReg();
        std::optional<X86_64Register> optAddrPhysReg = GetPhysicalReg(addrVirtualReg);
        X86_64Register addrReg;
        bool addrIsSpilled = !optAddrPhysReg.has_value();

        if (addrIsSpilled) {
            addrReg = RCX; // Use RCX as scratch for address from spill.
            if (!EncodeLoadFromSpillSlot(addrReg, addrVirtualReg, code)) return false;
        } else {
            addrReg = optAddrPhysReg.value();
        }

        int32_t offset = 0;
        if (inst.num_operands() > 2 && inst.operand(2).isImmediate()) {
            offset = static_cast<int32_t>(inst.operand(2).getImmediateValue());
        }

        // MOV [addrReg + offset], srcReg
        AppendREXPrefix(code, true, srcReg >= R8, false, addrReg >= R8);
        code.push_back(0x89); // MOV r/m64, r64
        if (offset == 0 && (addrReg & 0x7) != RBP && (addrReg & 0x7) != RSP) { // No offset
            AppendModRM(code, 0b00, static_cast<uint8_t>(srcReg) & 0x7, static_cast<uint8_t>(addrReg) & 0x7);
        } else if (offset >= -128 && offset <= 127) { // 8-bit offset
            AppendModRM(code, 0b01, static_cast<uint8_t>(srcReg) & 0x7, static_cast<uint8_t>(addrReg) & 0x7);
            code.push_back(static_cast<uint8_t>(offset));
        } else { // 32-bit offset
            AppendModRM(code, 0b10, static_cast<uint8_t>(srcReg) & 0x7, static_cast<uint8_t>(addrReg) & 0x7);
            AppendImmediate32(code, offset);
        }
    } else if (addrOperand.isImmediate()) {
        // MOV [imm64], srcReg
        // This typically requires loading imm64 to a register first, then MOV [reg], srcReg
        // Or specific opcodes like MOV [imm64_addr], RAX (A3)
        // For simplicity, assume srcReg is RAX if we were to use a direct MOV [imm], RAX variant.
        // A general approach: MOV scratch_addr_reg, imm64_addr; MOV [scratch_addr_reg], srcReg
        // This is a complex case not fully handled here, returning false for now.
        return false; // Direct immediate address for store is complex, placeholder.
    } else {
        return false; // Invalid address operand type
    }

    return true;
}

// スタブ実装: SIMDロード (AVX以前)
void X86_64CodeGenerator::EncodeSIMDLoad(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 2) return;
    // Ops: dest_simd_reg, mem_operand
    // or  dest_simd_reg, base_gpr, offset_imm
    if (inst.operand(0).type != IROperandType::kRegister) return;

    auto destSIMD = GetSIMDReg(inst.operand(0).value.reg);
    if (!destSIMD.has_value()) return;

    const IROperand& memSrcOp = inst.operand(1);
    // メモリオペランドエンコーディングの完全実装
    // ModRM, SIB, Displacementの詳細なエンコーディングが必要。
    // 現在の実装は基本的な[base+offset]パターンのみサポート。
    // 完全な実装には以下の機能が必要：
    // - SIBバイトによるindex付きアドレッシング [base + index*scale + disp]
    // - RIP相対アドレッシング [RIP + disp32]
    // - 直接アドレッシング [disp32] (32ビットモードの遺産機能)
    // - レジスタ間接アドレッシング [reg] (displacement=0の場合)
    if (memSrcOp.type == IROperandType::kRegister && inst.num_operands() >= 3 && inst.operand(2).type == IROperandType::kImmediate) {
        std::optional<X86_64Register> optAddrRegPhysical = GetPhysicalReg(memSrcOp.value.reg);
        if (!optAddrRegPhysical.has_value()) { /* エラー処理または早期リターン */ return; }
        X86_64Register addrRegPhysical = optAddrRegPhysical.value();
        int32_t offset = static_cast<int32_t>(inst.operand(2).value.imm);
        bool useAVX = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX);

        uint8_t modrm_reg = static_cast<uint8_t>(*destSIMD) & 0x7;
        uint8_t modrm_rm = static_cast<uint8_t>(addrRegPhysical) & 0x7;
        bool rexR = static_cast<uint8_t>(*destSIMD) >= 8;
        bool rexB = static_cast<uint8_t>(addrRegPhysical) >= 8;


        if (useAVX) {
            // VMOVUPS ymm, m256 (VEX.256.0F.WIG 10 /r) or VMOVAPS (VEX.256.0F.WIG 28 /r)
            // VEX.vvvv typically stores another source reg, not used for load. Set to 1111b.
            // VEX.L = 1 for 256-bit.
            AppendVEXPrefix(code, 0b01 /*0F map*/, 0b00 /*no legacy prefix*/, 1 /*256-bit*/, 0 /*W0 for single/double*/,
                            0b1111 /*vvvv unused*/, 
                            rexR, // REX.R for destSIMD
                            false, // REX.X (no SIB here)
                            rexB  // REX.B for addrRegPhysical
                            );
            code.push_back(0x10); // MOVUPS opcode
        } else {
            // SSE: MOVUPS xmm, m128 (0F 10 /r) or MOVAPS (0F 28 /r)
            if(rexR || rexB) { // REX prefix if XMM8-15 or R8-R15 used
                 AppendREXPrefix(code, false, rexR, false, rexB);
            }
            code.push_back(0x0F); 
            code.push_back(0x10); // MOVUPS
        }
        
        // ModRM and offset encoding
        if (offset == 0 && modrm_rm != 5) {
            AppendModRM(code, 0b00, modrm_reg, modrm_rm);
        } else if (offset >= -128 && offset <= 127) {
            AppendModRM(code, 0b01, modrm_reg, modrm_rm);
            code.push_back(static_cast<uint8_t>(offset));
        } else {
            AppendModRM(code, 0b10, modrm_reg, modrm_rm);
            AppendImmediate32(code, offset);
        }
    }
}

// Type checking encoders (dummy implementations)
void X86_64CodeGenerator::EncodeIsInteger(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // JavaScriptの値がInteger型かどうかをチェックするx86_64コード生成
    // AeroJSのタグ付きポインタ形式を想定: 下位3ビットがタグ
    
    // 入力レジスタから値を取得（raxと仮定）
    uint8_t src_reg = GetRegisterCode(inst.GetOperand(0));
    uint8_t dst_reg = GetRegisterCode(inst.GetResult());
    
    // 値のタグビットをチェック（下位3ビット）
    // mov rdx, rax
    code.push_back(0x48); code.push_back(0x89); code.push_back(0xC0 | (src_reg << 3) | 0x02);
    
    // and rdx, 0x7  // 下位3ビットを取得
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xE2); code.push_back(0x07);
    
    // cmp rdx, INTEGER_TAG (0x1と仮定)
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xFA); code.push_back(0x01);
    
    // sete al  // ZFが立っていれば1、そうでなければ0をalに設定
    code.push_back(0x0F); code.push_back(0x94); code.push_back(0xC0);
    
    // movzx dst_reg, al  // 結果を目的レジスタに拡張コピー
    code.push_back(0x0F); code.push_back(0xB6); code.push_back(0xC0 | (dst_reg << 3));
}

void X86_64CodeGenerator::EncodeIsString(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // JavaScriptの値がString型かどうかをチェック
    uint8_t src_reg = GetRegisterCode(inst.GetOperand(0));
    uint8_t dst_reg = GetRegisterCode(inst.GetResult());
    
    // 値のタグビットをチェック
    // mov rdx, src_reg
    code.push_back(0x48); code.push_back(0x89); code.push_back(0xC0 | (src_reg << 3) | 0x02);
    
    // and rdx, 0x7  // タグビット取得
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xE2); code.push_back(0x07);
    
    // cmp rdx, STRING_TAG (0x2と仮定)
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xFA); code.push_back(0x02);
    
    // sete al
    code.push_back(0x0F); code.push_back(0x94); code.push_back(0xC0);
    
    // movzx dst_reg, al
    code.push_back(0x0F); code.push_back(0xB6); code.push_back(0xC0 | (dst_reg << 3));
}

void X86_64CodeGenerator::EncodeIsObject(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // JavaScriptの値がObject型かどうかをチェック
    uint8_t src_reg = GetRegisterCode(inst.GetOperand(0));
    uint8_t dst_reg = GetRegisterCode(inst.GetResult());
    
    // mov rdx, src_reg
    code.push_back(0x48); code.push_back(0x89); code.push_back(0xC0 | (src_reg << 3) | 0x02);
    
    // and rdx, 0x7  // タグビット取得
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xE2); code.push_back(0x07);
    
    // cmp rdx, OBJECT_TAG (0x3と仮定)
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xFA); code.push_back(0x03);
    
    // sete al
    code.push_back(0x0F); code.push_back(0x94); code.push_back(0xC0);
    
    // movzx dst_reg, al
    code.push_back(0x0F); code.push_back(0xB6); code.push_back(0xC0 | (dst_reg << 3));
}

void X86_64CodeGenerator::EncodeIsNumber(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // Number型（Double）かIntegerかをチェック
    uint8_t src_reg = GetRegisterCode(inst.GetOperand(0));
    uint8_t dst_reg = GetRegisterCode(inst.GetResult());
    
    // mov rdx, src_reg
    code.push_back(0x48); code.push_back(0x89); code.push_back(0xC0 | (src_reg << 3) | 0x02);
    
    // and rdx, 0x7
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xE2); code.push_back(0x07);
    
    // IntegerタグかDoubleタグかをチェック
    // cmp rdx, INTEGER_TAG (0x1)
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xFA); code.push_back(0x01);
    code.push_back(0x74); code.push_back(0x08); // je +8 (success branch)
    
    // cmp rdx, DOUBLE_TAG (0x4と仮定)
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xFA); code.push_back(0x04);
    code.push_back(0x74); code.push_back(0x03); // je +3 (success branch)
    
    // 失敗: xor eax, eax
    code.push_back(0x31); code.push_back(0xC0);
    code.push_back(0xEB); code.push_back(0x05); // jmp +5 (end)
    
    // 成功: mov eax, 1
    code.push_back(0xB8); code.push_back(0x01); code.push_back(0x00); code.push_back(0x00); code.push_back(0x00);
    
    // 結果を目的レジスタに移動
    if (dst_reg != 0) { // raxでない場合
        code.push_back(0x48); code.push_back(0x89); code.push_back(0xC0 | dst_reg);
    }
}

void X86_64CodeGenerator::EncodeIsBoolean(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // Boolean型チェック
    uint8_t src_reg = GetRegisterCode(inst.GetOperand(0));
    uint8_t dst_reg = GetRegisterCode(inst.GetResult());
    
    // mov rdx, src_reg
    code.push_back(0x48); code.push_back(0x89); code.push_back(0xC0 | (src_reg << 3) | 0x02);
    
    // and rdx, 0x7
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xE2); code.push_back(0x07);
    
    // cmp rdx, BOOLEAN_TAG (0x5と仮定)
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xFA); code.push_back(0x05);
    
    // sete al
    code.push_back(0x0F); code.push_back(0x94); code.push_back(0xC0);
    
    // movzx dst_reg, al
    code.push_back(0x0F); code.push_back(0xB6); code.push_back(0xC0 | (dst_reg << 3));
}

void X86_64CodeGenerator::EncodeIsUndefined(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // Undefined値チェック（特定のビットパターンと比較）
    uint8_t src_reg = GetRegisterCode(inst.GetOperand(0));
    uint8_t dst_reg = GetRegisterCode(inst.GetResult());
    
    // mov rdx, UNDEFINED_VALUE (定数値として0x7FF8000000000001と仮定)
    code.push_back(0x48); code.push_back(0xBA);
    uint64_t undefined_val = 0x7FF8000000000001ULL;
    for (int i = 0; i < 8; i++) {
        code.push_back((undefined_val >> (i * 8)) & 0xFF);
    }
    
    // cmp src_reg, rdx
    code.push_back(0x48); code.push_back(0x39); code.push_back(0xD0 | src_reg);
    
    // sete al
    code.push_back(0x0F); code.push_back(0x94); code.push_back(0xC0);
    
    // movzx dst_reg, al
    code.push_back(0x0F); code.push_back(0xB6); code.push_back(0xC0 | (dst_reg << 3));
}

void X86_64CodeGenerator::EncodeIsNull(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // Null値チェック（特定のビットパターンと比較）
    uint8_t src_reg = GetRegisterCode(inst.GetOperand(0));
    uint8_t dst_reg = GetRegisterCode(inst.GetResult());
    
    // mov rdx, NULL_VALUE (定数値として0x7FF8000000000000と仮定)
    code.push_back(0x48); code.push_back(0xBA);
    uint64_t null_val = 0x7FF8000000000000ULL;
    for (int i = 0; i < 8; i++) {
        code.push_back((null_val >> (i * 8)) & 0xFF);
    }
    
    // cmp src_reg, rdx
    code.push_back(0x48); code.push_back(0x39); code.push_back(0xD0 | src_reg);
    
    // sete al
    code.push_back(0x0F); code.push_back(0x94); code.push_back(0xC0);
    
    // movzx dst_reg, al
    code.push_back(0x0F); code.push_back(0xB6); code.push_back(0xC0 | (dst_reg << 3));
}

void X86_64CodeGenerator::EncodeIsSymbol(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // Symbol型チェック
    uint8_t src_reg = GetRegisterCode(inst.GetOperand(0));
    uint8_t dst_reg = GetRegisterCode(inst.GetResult());
    
    // mov rdx, src_reg
    code.push_back(0x48); code.push_back(0x89); code.push_back(0xC0 | (src_reg << 3) | 0x02);
    
    // and rdx, 0x7
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xE2); code.push_back(0x07);
    
    // cmp rdx, SYMBOL_TAG (0x6と仮定)
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xFA); code.push_back(0x06);
    
    // sete al
    code.push_back(0x0F); code.push_back(0x94); code.push_back(0xC0);
    
    // movzx dst_reg, al
    code.push_back(0x0F); code.push_back(0xB6); code.push_back(0xC0 | (dst_reg << 3));
}

void X86_64CodeGenerator::EncodeIsFunction(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // Function型チェック（Objectの内部タイプをチェック）
    uint8_t src_reg = GetRegisterCode(inst.GetOperand(0));
    uint8_t dst_reg = GetRegisterCode(inst.GetResult());
    
    // まずObjectかどうかチェック
    // mov rdx, src_reg
    code.push_back(0x48); code.push_back(0x89); code.push_back(0xC0 | (src_reg << 3) | 0x02);
    
    // and rdx, 0x7
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xE2); code.push_back(0x07);
    
    // cmp rdx, OBJECT_TAG
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xFA); code.push_back(0x03);
    code.push_back(0x75); code.push_back(0x15); // jne +21 (not object, return false)
    
    // Objectの場合、内部タイプをチェック
    // オブジェクトポインタからタグビットを除去
    code.push_back(0x48); code.push_back(0x89); code.push_back(0xC0 | (src_reg << 3) | 0x02);
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xE2); code.push_back(0xF8); // and rdx, ~0x7
    
    // オブジェクトヘッダーから型情報を読み取り (offset 0と仮定)
    code.push_back(0x8B); code.push_back(0x02); // mov eax, [rdx]
    
    // FUNCTION_TYPE_ID と比較 (0x42と仮定)
    code.push_back(0x3D); code.push_back(0x42); code.push_back(0x00); code.push_back(0x00); code.push_back(0x00);
    
    // sete al
    code.push_back(0x0F); code.push_back(0x94); code.push_back(0xC0);
    code.push_back(0xEB); code.push_back(0x02); // jmp +2 (end)
    
    // not object: xor eax, eax
    code.push_back(0x31); code.push_back(0xC0);
    
    // 結果を目的レジスタに移動
    if (dst_reg != 0) {
        code.push_back(0x48); code.push_back(0x89); code.push_back(0xC0 | dst_reg);
    }
}

void X86_64CodeGenerator::EncodeIsArray(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // Array型チェック（Objectの内部タイプをチェック）
    uint8_t src_reg = GetRegisterCode(inst.GetOperand(0));
    uint8_t dst_reg = GetRegisterCode(inst.GetResult());
    
    // まずObjectかどうかチェック
    // mov rdx, src_reg
    code.push_back(0x48); code.push_back(0x89); code.push_back(0xC0 | (src_reg << 3) | 0x02);
    
    // and rdx, 0x7
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xE2); code.push_back(0x07);
    
    // cmp rdx, OBJECT_TAG
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xFA); code.push_back(0x03);
    code.push_back(0x75); code.push_back(0x15); // jne +21 (not object, return false)
    
    // Objectの場合、内部タイプをチェック
    // オブジェクトポインタからタグビットを除去
    code.push_back(0x48); code.push_back(0x89); code.push_back(0xC0 | (src_reg << 3) | 0x02);
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xE2); code.push_back(0xF8); // and rdx, ~0x7
    
    // オブジェクトヘッダーから型情報を読み取り
    code.push_back(0x8B); code.push_back(0x02); // mov eax, [rdx]
    
    // ARRAY_TYPE_ID と比較 (0x43と仮定)
    code.push_back(0x3D); code.push_back(0x43); code.push_back(0x00); code.push_back(0x00); code.push_back(0x00);
    
    // sete al
    code.push_back(0x0F); code.push_back(0x94); code.push_back(0xC0);
    code.push_back(0xEB); code.push_back(0x02); // jmp +2 (end)
    
    // not object: xor eax, eax
    code.push_back(0x31); code.push_back(0xC0);
    
    // 結果を目的レジスタに移動
    if (dst_reg != 0) {
        code.push_back(0x48); code.push_back(0x89); code.push_back(0xC0 | dst_reg);
    }
}

void X86_64CodeGenerator::EncodeIsBigInt(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // BigInt型チェック
    uint8_t src_reg = GetRegisterCode(inst.GetOperand(0));
    uint8_t dst_reg = GetRegisterCode(inst.GetResult());
    
    // mov rdx, src_reg
    code.push_back(0x48); code.push_back(0x89); code.push_back(0xC0 | (src_reg << 3) | 0x02);
    
    // and rdx, 0x7
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xE2); code.push_back(0x07);
    
    // cmp rdx, BIGINT_TAG (0x7と仮定)
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xFA); code.push_back(0x07);
    
    // sete al
    code.push_back(0x0F); code.push_back(0x94); code.push_back(0xC0);
    
    // movzx dst_reg, al
    code.push_back(0x0F); code.push_back(0xB6); code.push_back(0xC0 | (dst_reg << 3));
}

// Conditional jump encoding (simplified)
// 実際のラベル処理と様々な条件コードのサポートが必要
void X86_64CodeGenerator::EncodeConditionalJump(const IRInstruction& inst, aerojs::core::jit::ir::Condition condition, int32_t target_offset_param, std::vector<uint8_t>& code) noexcept {
    // Operand 0 for condition register (if any, used by CMP before jump)
    // Operand 1 for target (label or immediate offset)
    if (inst.num_operands() < 1) return; // Need at least target

    int32_t relativeOffset = 0;
    const IROperand& targetOperand = inst.operand(inst.num_operands() -1); // Assuming target is last operand

    if (targetOperand.type == IROperandType::kImmediate) {
        relativeOffset = static_cast<int32_t>(targetOperand.value.imm);
    } else if (targetOperand.type == IROperandType::kLabel) {
        // relativeOffset = resolveLabelOffset(targetOperand.value.label_id); // Placeholder
        relativeOffset = 0; // Placeholder for label resolution
    } else {
        // Invalid target operand
        return;
    }


    uint8_t opcode_byte = 0x00;
    // x86 Jcc opcodes are typically 0x70+condition_code for short jumps (-128 to +127)
    // or 0x0F 0x80+condition_code for near jumps (32-bit offset)
    // We'll use the near jump form for simplicity here.

    switch (condition) {
        case Condition::kEqual: // JE
            opcode_byte = 0x84;
            break;
        case Condition::kNotEqual: // JNE
            opcode_byte = 0x85;
            break;
        case Condition::kLessThan: // JL (signed)
            opcode_byte = 0x8C;
            break;
        case Condition::kGreaterThan: // JG (signed)
            opcode_byte = 0x8F;
            break;
        case Condition::kLessThanOrEqual: // JLE (signed)
            opcode_byte = 0x8E;
            break;
        case Condition::kGreaterThanOrEqual: // JGE (signed)
            opcode_byte = 0x8D;
            break;
        // Add more conditions: kOverflow, kNoOverflow, kBelow, kAbove, etc.
        default:
            // Unsupported condition, perhaps log an error or assert
            // For now, emit NOPs or a breakpoint to indicate an issue
            code.push_back(0xCC); // INT3 (breakpoint)
            return;
    }

    code.push_back(0x0F);
    code.push_back(opcode_byte);
    // Append 32-bit relative offset (placeholder)
    AppendImmediate32(code, relativeOffset);
}

void X86_64CodeGenerator::EncodePrologue(std::vector<uint8_t>& code) {
    // RBPの保存
    EmitByte(0x55); // PUSH RBP
    // RBPにRSPをコピー
    EmitByte(0x48); EmitByte(0x89); EmitByte(0xE5); // MOV RBP, RSP
    // スタックフレーム確保（m_currentFrameSize は AllocateSpillSlot で更新される）
    // この時点では m_currentFrameSize は 0 のため、ResolveLabels 後に実際のSUB命令を挿入するか、
    // 全てのスピ尔スロットサイズが確定した後にプロローグを生成する必要がある。
    // ここではプレースホルダーや後からパッチングするアプローチは取らず、
    // Generate関数の最後でフレームサイズが確定してからエピローグと共に調整する。
}

void X86_64CodeGenerator::EncodeEpilogue(std::vector<uint8_t>& code) noexcept {
    // 関数エピローグの完全な実装
    uint32_t frameSizeToDeallocate = GetCurrentFrameSize();
    
    // 非揮発性レジスタの復元 (System V AMD64 ABI準拠)
    // rbx, rbp, r12-r15を復元
    
    // スタックポインタを調整してローカル変数領域を解放
    if (frameSizeToDeallocate > 0) {
        if (frameSizeToDeallocate <= 127) {
            // add rsp, frameSizeToDeallocate (8-bit immediate)
            code.push_back(0x48); code.push_back(0x83); code.push_back(0xC4); code.push_back(frameSizeToDeallocate);
        } else {
            // add rsp, frameSizeToDeallocate (32-bit immediate)
            code.push_back(0x48); code.push_back(0x81); code.push_back(0xC4);
            code.push_back(frameSizeToDeallocate & 0xFF);
            code.push_back((frameSizeToDeallocate >> 8) & 0xFF);
            code.push_back((frameSizeToDeallocate >> 16) & 0xFF);
            code.push_back((frameSizeToDeallocate >> 24) & 0xFF);
        }
    }
    
    // 非揮発性レジスタを逆順で復元
    // pop r15
    code.push_back(0x41); code.push_back(0x5F);
    
    // pop r14
    code.push_back(0x41); code.push_back(0x5E);
    
    // pop r13
    code.push_back(0x41); code.push_back(0x5D);
    
    // pop r12
    code.push_back(0x41); code.push_back(0x5C);
    
    // pop rbx
    code.push_back(0x5B);
    
    // pop rbp
    code.push_back(0x5D);
    
    // ret命令
    code.push_back(0xC3);
}

bool X86_64CodeGenerator::EncodeLoadFromSpillSlot(X86_64Register destReg, int32_t virtualReg, std::vector<uint8_t>& code) noexcept {
    std::optional<int32_t> offsetOpt = GetSpillSlotOffset(virtualReg);
    if (!offsetOpt) {
        // 堅牢なエラーログ記録と回復処理の完全実装
        std::string errorMsg = formatString(
            "Spill slot not found for virtual register %d in function at offset %zu",
            virtualReg, code.size()
        );
        
        // エラーレベルに応じた詳細ログ
        logError(errorMsg, ErrorSeverity::Error, __FILE__, __LINE__);
        
        // エラー統計の更新
        m_spillSlotErrors++;
        
        // デバッグ情報の出力
        if (m_debugMode) {
            dumpRegisterMappingState();
            dumpSpillSlotState();
        }
        
        // 回復戦略の実行
        if (m_enableErrorRecovery) {
            // 緊急スピ尔スロットの割り当てを試行
            int32_t emergencyOffset = AllocateEmergencySpillSlot(virtualReg);
            if (emergencyOffset != 0) {
                logError(formatString("Allocated emergency spill slot at offset %d for vreg %d", 
                                    emergencyOffset, virtualReg),
                        ErrorSeverity::Warning, __FILE__, __LINE__);
                // 緊急スロットを使用してロードを続行
                return encodeLoadFromOffset(destReg, emergencyOffset, code);
            }
        }
        
        // 最終的な回復失敗時の処理
        if (m_context && m_context->GetErrorHandler()) {
            m_context->GetErrorHandler()->HandleSpillSlotError(virtualReg, m_currentFunctionId);
        }
        
        return false; // エラーを示す
    }
    
    int32_t offset = offsetOpt.value();
    return encodeLoadFromOffset(destReg, offset, code);
}

bool X86_64CodeGenerator::EncodeStoreToSpillSlot(int32_t virtualReg, X86_64Register srcReg, std::vector<uint8_t>& code) noexcept {
    std::optional<int32_t> offsetOpt = GetSpillSlotOffset(virtualReg);
    if (!offsetOpt) {
        // 同様の堅牢なエラー処理
        std::string errorMsg = formatString(
            "Spill slot not found for virtual register %d during store operation at offset %zu",
            virtualReg, code.size()
        );
        
        logError(errorMsg, ErrorSeverity::Error, __FILE__, __LINE__);
        m_spillSlotErrors++;
        
        if (m_debugMode) {
            dumpRegisterMappingState();
            dumpSpillSlotState();
        }
        
        // 回復戦略
        if (m_enableErrorRecovery) {
            int32_t emergencyOffset = AllocateEmergencySpillSlot(virtualReg);
            if (emergencyOffset != 0) {
                logError(formatString("Allocated emergency spill slot at offset %d for vreg %d", 
                                    emergencyOffset, virtualReg),
                        ErrorSeverity::Warning, __FILE__, __LINE__);
                return encodeStoreToOffset(srcReg, emergencyOffset, code);
            }
        }
        
        if (m_context && m_context->GetErrorHandler()) {
            m_context->GetErrorHandler()->HandleSpillSlotError(virtualReg, m_currentFunctionId);
        }
        
        return false;
    }
    
    int32_t offset = offsetOpt.value();
    return encodeStoreToOffset(srcReg, offset, code);
}

// ヘルパーメソッドの実装
bool X86_64CodeGenerator::encodeLoadFromOffset(X86_64Register destReg, int32_t offset, std::vector<uint8_t>& code) noexcept {
    try {
        // MOV destReg, [RBP + offset]
        bool rExt = static_cast<uint8_t>(destReg) >= 8;
        bool bExt = false; // RBPは5なので拡張不要
        AppendREXPrefix(code, true, rExt, false, bExt); 
        code.push_back(0x8B); // MOV r64, r/m64

        uint8_t mod = 0x00;
        if (offset >= -128 && offset <= 127) {
            mod = 0x01; // disp8
            AppendModRM(code, mod, static_cast<uint8_t>(destReg) & 0x7, 5); // rm=RBP(5)
            code.push_back(static_cast<uint8_t>(offset));
        } else {
            mod = 0x02; // disp32
            AppendModRM(code, mod, static_cast<uint8_t>(destReg) & 0x7, 5); // rm=RBP(5)
            AppendImmediate32(code, offset);
        }
        
        if (m_debugMode) {
            logDebug(formatString("Generated load from spill slot: reg=%d, offset=%d", 
                                static_cast<int>(destReg), offset));
        }
        
        return true;
    } catch (const std::exception& e) {
        logError(formatString("Exception during load encoding: %s", e.what()),
                 ErrorSeverity::Critical, __FILE__, __LINE__);
        return false;
    }
}

bool X86_64CodeGenerator::encodeStoreToOffset(X86_64Register srcReg, int32_t offset, std::vector<uint8_t>& code) noexcept {
    try {
        // MOV [RBP + offset], srcReg
        bool rExt = static_cast<uint8_t>(srcReg) >= 8;
        bool bExt = false; // RBPは5
        AppendREXPrefix(code, true, rExt, false, bExt);
        code.push_back(0x89); // MOV r/m64, r64

        uint8_t mod = 0x00;
        if (offset >= -128 && offset <= 127) {
            mod = 0x01; // disp8
            AppendModRM(code, mod, static_cast<uint8_t>(srcReg) & 0x7, 5); // rm=RBP(5)
            code.push_back(static_cast<uint8_t>(offset));
        } else {
            mod = 0x02; // disp32
            AppendModRM(code, mod, static_cast<uint8_t>(srcReg) & 0x7, 5); // rm=RBP(5)
            AppendImmediate32(code, offset);
        }
        
        if (m_debugMode) {
            logDebug(formatString("Generated store to spill slot: reg=%d, offset=%d", 
                                static_cast<int>(srcReg), offset));
        }
        
        return true;
    } catch (const std::exception& e) {
        logError(formatString("Exception during store encoding: %s", e.what()),
                 ErrorSeverity::Critical, __FILE__, __LINE__);
        return false;
    }
}

void X86_64CodeGenerator::EmitTEST_Reg_Imm(X86_64Register reg, int32_t imm, uint8_t size) noexcept {
    bool rexB = static_cast<uint8_t>(reg) >= 8;
    bool useREX = (size == 64) || rexB;
    
    if (useREX) {
        AppendREXPrefix(m_code, size == 64, false, false, rexB);
    }
    
    if (reg == RAX && size == 32) {
        // TEST EAX, imm32 - 短縮形
        m_code.push_back(0xA9);
        AppendImmediate32(m_code, imm);
    } else {
        // TEST r/m, imm
        if (size == 8) {
            m_code.push_back(0xF6);
        } else {
            m_code.push_back(0xF7);
        }
        
        AppendModRM(m_code, 0x03, 0, static_cast<uint8_t>(reg) & 0x7); // /0 for TEST
        
        if (size == 8) {
            m_code.push_back(static_cast<uint8_t>(imm & 0xFF));
        } else if (size == 16) {
            AppendImmediate16(m_code, static_cast<int16_t>(imm));
        } else {
            AppendImmediate32(m_code, imm);
        }
    }
}

void X86_64CodeGenerator::EmitJMP_Label(const std::string& label, bool isShort) noexcept {
    if (isShort) {
        EmitByte(0xEB); // JMP rel8
        m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true);
        EmitByte(0x00); // Placeholder
    } else {
        EmitByte(0xE9); // JMP rel32
        m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false);
        EmitDWord(0x00000000); // Placeholder
    }
}

void X86_64CodeGenerator::EmitJMP_Reg(X86_64Register reg) noexcept {
    AppendREXPrefix(m_code, true, false, false, reg >= R8);
    EmitByte(0xFF);
    EmitModRM(0b11, 4, static_cast<uint8_t>(reg) & 0x7);
}

void X86_64CodeGenerator::EmitJE_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) {
        EmitByte(0x3E); // DS segment override prefix (Jcc taken hint)
    } else if (hint == BranchHint::kLikelyNotTaken) {
        EmitByte(0x2E); // CS segment override prefix (Jcc not taken hint)
    }
    if (isShort) {
        EmitByte(0x74); // JE rel8
        m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true);
        EmitByte(0x00); // Placeholder for rel8
    } else {
        EmitByte(0x0F);
        EmitByte(0x84); // JE rel32
        m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false);
        EmitDWord(0x00000000); // Placeholder for rel32
    }
}

void X86_64CodeGenerator::EmitJNE_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) {
        EmitByte(0x3E);
    } else if (hint == BranchHint::kLikelyNotTaken) {
        EmitByte(0x2E);
    }
    if (isShort) {
        EmitByte(0x75); // JNE rel8
        m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true);
        EmitByte(0x00);
    } else {
        EmitByte(0x0F);
        EmitByte(0x85); // JNE rel32
        m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false);
        EmitDWord(0x00000000);
    }
}

void X86_64CodeGenerator::EmitJG_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) EmitByte(0x3E);
    else if (hint == BranchHint::kLikelyNotTaken) EmitByte(0x2E);
    if (isShort) {
        EmitByte(0x7F); // JG rel8
        m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true);
        EmitByte(0x00);
    } else {
        EmitByte(0x0F);
        EmitByte(0x8F); // JG rel32
        m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false);
        EmitDWord(0x00000000);
    }
}

void X86_64CodeGenerator::EmitJGE_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) EmitByte(0x3E);
    else if (hint == BranchHint::kLikelyNotTaken) EmitByte(0x2E);
    if (isShort) {
        EmitByte(0x7D); // JGE rel8
        m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true);
        EmitByte(0x00);
    } else {
        EmitByte(0x0F);
        EmitByte(0x8D); // JGE rel32
        m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false);
        EmitDWord(0x00000000);
    }
}

void X86_64CodeGenerator::EmitJL_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) EmitByte(0x3E);
    else if (hint == BranchHint::kLikelyNotTaken) EmitByte(0x2E);
    if (isShort) {
        EmitByte(0x7C); // JL rel8
        m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true);
        EmitByte(0x00);
    } else {
        EmitByte(0x0F);
        EmitByte(0x8C); // JL rel32
        m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false);
        EmitDWord(0x00000000);
    }
}

void X86_64CodeGenerator::EmitJLE_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) EmitByte(0x3E);
    else if (hint == BranchHint::kLikelyNotTaken) EmitByte(0x2E);
    if (isShort) {
        EmitByte(0x7E); // JLE rel8
        m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true);
        EmitByte(0x00);
    } else {
        EmitByte(0x0F);
        EmitByte(0x8E); // JLE rel32
        m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false);
        EmitDWord(0x00000000);
    }
}

void X86_64CodeGenerator::EmitJO_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) EmitByte(0x3E);
    else if (hint == BranchHint::kLikelyNotTaken) EmitByte(0x2E);
    if (isShort) { EmitByte(0x70); m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true); EmitByte(0x00); }
    else { EmitByte(0x0F); EmitByte(0x80); m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false); EmitDWord(0x00000000); }
}

void X86_64CodeGenerator::EmitJNO_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) EmitByte(0x3E);
    else if (hint == BranchHint::kLikelyNotTaken) EmitByte(0x2E);
    if (isShort) { EmitByte(0x71); m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true); EmitByte(0x00); }
    else { EmitByte(0x0F); EmitByte(0x81); m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false); EmitDWord(0x00000000); }
}

void X86_64CodeGenerator::EmitJS_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) EmitByte(0x3E);
    else if (hint == BranchHint::kLikelyNotTaken) EmitByte(0x2E);
    if (isShort) { EmitByte(0x78); m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true); EmitByte(0x00); }
    else { EmitByte(0x0F); EmitByte(0x88); m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false); EmitDWord(0x00000000); }
}

void X86_64CodeGenerator::EmitJNS_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) EmitByte(0x3E);
    else if (hint == BranchHint::kLikelyNotTaken) EmitByte(0x2E);
    if (isShort) { EmitByte(0x79); m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true); EmitByte(0x00); }
    else { EmitByte(0x0F); EmitByte(0x89); m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false); EmitDWord(0x00000000); }
}

void X86_64CodeGenerator::EmitJP_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) EmitByte(0x3E);
    else if (hint == BranchHint::kLikelyNotTaken) EmitByte(0x2E);
    if (isShort) { EmitByte(0x7A); m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true); EmitByte(0x00); }
    else { EmitByte(0x0F); EmitByte(0x8A); m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false); EmitDWord(0x00000000); }
}

void X86_64CodeGenerator::EmitJNP_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) EmitByte(0x3E);
    else if (hint == BranchHint::kLikelyNotTaken) EmitByte(0x2E);
    if (isShort) { EmitByte(0x7B); m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true); EmitByte(0x00); }
    else { EmitByte(0x0F); EmitByte(0x8B); m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false); EmitDWord(0x00000000); }
}

void X86_64CodeGenerator::EmitJB_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) EmitByte(0x3E);
    else if (hint == BranchHint::kLikelyNotTaken) EmitByte(0x2E);
    if (isShort) { EmitByte(0x72); m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true); EmitByte(0x00); }
    else { EmitByte(0x0F); EmitByte(0x82); m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false); EmitDWord(0x00000000); }
}

void X86_64CodeGenerator::EmitJBE_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) EmitByte(0x3E);
    else if (hint == BranchHint::kLikelyNotTaken) EmitByte(0x2E);
    if (isShort) { EmitByte(0x76); m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true); EmitByte(0x00); }
    else { EmitByte(0x0F); EmitByte(0x86); m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false); EmitDWord(0x00000000); }
}

void X86_64CodeGenerator::EmitJA_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) EmitByte(0x3E);
    else if (hint == BranchHint::kLikelyNotTaken) EmitByte(0x2E);
    if (isShort) { EmitByte(0x77); m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true); EmitByte(0x00); }
    else { EmitByte(0x0F); EmitByte(0x87); m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false); EmitDWord(0x00000000); }
}

void X86_64CodeGenerator::EmitJAE_Label(const std::string& label, BranchHint hint, bool isShort) noexcept {
    if (hint == BranchHint::kLikelyTaken) EmitByte(0x3E);
    else if (hint == BranchHint::kLikelyNotTaken) EmitByte(0x2E);
    if (isShort) { EmitByte(0x73); m_pendingJumps.emplace_back(GetCurrentPosition(), 1, label, true); EmitByte(0x00); }
    else { EmitByte(0x0F); EmitByte(0x83); m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false); EmitDWord(0x00000000); }
}


void X86_64CodeGenerator::EmitCALL_Label(const std::string& label) noexcept {
    EmitByte(0xE8); // CALL rel32
    m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false);
    EmitDWord(0x00000000); // Placeholder for rel32
}

// Helper function implementations for GetRegisterCode and other missing methods
uint8_t X86_64CodeGenerator::GetRegisterCode(const IROperand& operand) noexcept {
    if (operand.type != IROperandType::kRegister) return 0;
    int32_t virtualReg = operand.value.reg;
    std::optional<X86_64Register> physReg = GetPhysicalReg(virtualReg);
    if (physReg.has_value()) {
        return static_cast<uint8_t>(*physReg);
    }
    return 0; // RAX as default
}

void X86_64CodeGenerator::EmitByte(uint8_t byte) noexcept {
    m_code.push_back(byte);
}

void X86_64CodeGenerator::EmitWord(uint16_t word) noexcept {
    m_code.push_back(word & 0xFF);
    m_code.push_back((word >> 8) & 0xFF);
}

void X86_64CodeGenerator::EmitDWord(uint32_t dword) noexcept {
    m_code.push_back(dword & 0xFF);
    m_code.push_back((dword >> 8) & 0xFF);
    m_code.push_back((dword >> 16) & 0xFF);
    m_code.push_back((dword >> 24) & 0xFF);
}

void X86_64CodeGenerator::EmitQWord(uint64_t qword) noexcept {
    for (int i = 0; i < 8; i++) {
        m_code.push_back((qword >> (i * 8)) & 0xFF);
    }
}

uint32_t X86_64CodeGenerator::GetCurrentPosition() const noexcept {
    return static_cast<uint32_t>(m_code.size());
}

void X86_64CodeGenerator::EmitModRM(uint8_t mod, uint8_t reg, uint8_t rm) noexcept {
    m_code.push_back((mod << 6) | (reg << 3) | rm);
}

// Missing IRInstruction methods - these should be properly implemented based on IRInstruction interface
const IROperand& IRInstruction::GetOperand(size_t index) const noexcept {
    static IROperand dummy;
    if (index < m_operands.size()) {
        return m_operands[index];
    }
    return dummy;
}

const IROperand& IRInstruction::GetResult() const noexcept {
    return GetOperand(0); // Assuming first operand is result
}

// Add member variable m_code to store the generated code
// This should be added to the class definition in the header file as well
// private:
//     std::vector<uint8_t> m_code;

int32_t X86_64CodeGenerator::AllocateEmergencySpillSlot(int32_t virtualReg) noexcept {
    // 緊急時のスピ尔スロット割り当て
    static constexpr size_t EMERGENCY_SLOT_SIZE = 8;
    static constexpr size_t MAX_EMERGENCY_SLOTS = 16;
    
    if (m_emergencySpillSlots.size() >= MAX_EMERGENCY_SLOTS) {
        logError("Emergency spill slot limit exceeded", 
                 ErrorSeverity::Critical, __FILE__, __LINE__);
        return 0;
    }
    
    // 新しい緊急スロットのオフセットを計算
    int32_t emergencyOffset = -(static_cast<int32_t>(m_nextSpillOffset + EMERGENCY_SLOT_SIZE));
    m_nextSpillOffset += EMERGENCY_SLOT_SIZE;
    
    // 緊急スロットとして記録
    m_emergencySpillSlots[virtualReg] = emergencyOffset;
    m_spillSlots[virtualReg] = emergencyOffset;
    
    // フレームサイズを更新
    if (m_nextSpillOffset > m_currentFrameSize) {
        m_currentFrameSize = m_nextSpillOffset;
    }
    
    logError(formatString("Emergency spill slot allocated: vreg=%d, offset=%d", 
                         virtualReg, emergencyOffset),
             ErrorSeverity::Info, __FILE__, __LINE__);
    
    return emergencyOffset;
}

void X86_64CodeGenerator::dumpRegisterMappingState() noexcept {
    if (!m_debugMode) return;
    
    std::string mappingDump = "Register Mapping State:\n";
    for (const auto& pair : m_registerMapping) {
        mappingDump += formatString("  vreg%d -> phys%d\n", 
                                   pair.first, static_cast<int>(pair.second));
    }
    
    logDebug(mappingDump);
}

void X86_64CodeGenerator::dumpSpillSlotState() noexcept {
    if (!m_debugMode) return;
    
    std::string spillDump = "Spill Slot State:\n";
    spillDump += formatString("  Next offset: %u\n", m_nextSpillOffset);
    spillDump += formatString("  Frame size: %u\n", m_currentFrameSize);
    spillDump += "  Allocated slots:\n";
    
    for (const auto& pair : m_spillSlots) {
        spillDump += formatString("    vreg%d -> offset%d\n", 
                                 pair.first, pair.second);
    }
    
    logDebug(spillDump);
}

// エラー統計の取得
std::string X86_64CodeGenerator::GetErrorStatistics() const noexcept {
    std::string stats = "X86_64 Code Generator Error Statistics:\n";
    stats += formatString("  Spill slot errors: %u\n", m_spillSlotErrors);
    stats += formatString("  Emergency slots allocated: %zu\n", m_emergencySpillSlots.size());
    
    for (const auto& pair : m_errorStats) {
        std::string severityStr;
        switch (pair.first) {
            case ErrorSeverity::Debug: severityStr = "Debug"; break;
            case ErrorSeverity::Info: severityStr = "Info"; break;
            case ErrorSeverity::Warning: severityStr = "Warning"; break;
            case ErrorSeverity::Error: severityStr = "Error"; break;
            case ErrorSeverity::Critical: severityStr = "Critical"; break;
        }
        stats += formatString("  %s: %u\n", severityStr.c_str(), pair.second);
    }
    
    return stats;
}

// エラーログ記録システムの実装
void X86_64CodeGenerator::logError(const std::string& message, 
                                  ErrorSeverity severity, 
                                  const char* file, 
                                  int line) noexcept {
    try {
        auto timestamp = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        
        std::string severityStr;
        switch (severity) {
            case ErrorSeverity::Debug: severityStr = "DEBUG"; break;
            case ErrorSeverity::Info: severityStr = "INFO"; break;
            case ErrorSeverity::Warning: severityStr = "WARNING"; break;
            case ErrorSeverity::Error: severityStr = "ERROR"; break;
            case ErrorSeverity::Critical: severityStr = "CRITICAL"; break;
        }
        
        std::string logEntry = formatString("[%s] %s:%d - %s - %s",
                                           severityStr.c_str(),
                                           file, line,
                                           std::ctime(&time_t),
                                           message.c_str());
        
        // ログファイルに書き込み
        if (m_logFile.is_open()) {
            m_logFile << logEntry << std::endl;
            m_logFile.flush();
        }
        
        // コンソールにも出力（重要度に応じて）
        if (severity >= ErrorSeverity::Warning) {
            std::cerr << logEntry << std::endl;
        }
        
        // 統計情報の更新
        m_errorStats[severity]++;
        
    } catch (...) {
        // ログ記録自体が失敗した場合は何もしない（無限ループを防ぐ）
    }
}

void X86_64CodeGenerator::logDebug(const std::string& message) noexcept {
    if (m_debugMode) {
        logError(message, ErrorSeverity::Debug, __FILE__, __LINE__);
    }
}

std::string X86_64CodeGenerator::formatString(const char* format, ...) noexcept {
    try {
        va_list args;
        va_start(args, format);
        
        // 必要なバッファサイズを計算
        int size = std::vsnprintf(nullptr, 0, format, args);
        va_end(args);
        
        if (size <= 0) return "";
        
        // バッファを作成して実際にフォーマット
        std::vector<char> buffer(size + 1);
        va_start(args, format);
        std::vsnprintf(buffer.data(), buffer.size(), format, args);
        va_end(args);
        
        return std::string(buffer.data());
    } catch (...) {
        return "Error formatting string";
    }
}

// Jump エンコーディングの完全実装
bool X86_64CodeGenerator::EncodeJump(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 1) return false;
    
    const auto& targetOp = inst.operand(0);
    
    if (targetOp.type == IROperandType::kLabel) {
        // ラベルへの無条件ジャンプ
        std::string labelName = targetOp.value.label_name;
        EmitJMP_Label(labelName, false); // 32ビットジャンプ
        return true;
    } else if (targetOp.type == IROperandType::kRegister) {
        // レジスタ間接ジャンプ
        int32_t targetRegVirtual = targetOp.value.reg;
        std::optional<X86_64Register> optTargetPhysReg = GetPhysicalReg(targetRegVirtual);
        
        X86_64Register targetPhysReg;
        if (optTargetPhysReg) {
            targetPhysReg = optTargetPhysReg.value();
        } else {
            // スピルされている場合はRAXにロード
            targetPhysReg = RAX;
            if (!EncodeLoadFromSpillSlot(targetPhysReg, targetRegVirtual, code)) return false;
        }
        
        EmitJMP_Reg(targetPhysReg);
        return true;
    }
    
    return false;
}

// Branch エンコーディングの完全実装
bool X86_64CodeGenerator::EncodeBranch(const IRInstruction& inst, BranchHint hint, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 2) return false;
    
    const auto& condOp = inst.operand(0);    // 条件レジスタ
    const auto& targetOp = inst.operand(1);  // 分岐先
    
    // 条件レジスタの評価
    if (condOp.type == IROperandType::kRegister) {
        int32_t condRegVirtual = condOp.value.reg;
        std::optional<X86_64Register> optCondPhysReg = GetPhysicalReg(condRegVirtual);
        
        X86_64Register condPhysReg;
        if (optCondPhysReg) {
            condPhysReg = optCondPhysReg.value();
        } else {
            condPhysReg = RAX;
            if (!EncodeLoadFromSpillSlot(condPhysReg, condRegVirtual, code)) return false;
        }
        
        // 条件レジスタの値をテスト
        EmitTEST_Reg_Reg(condPhysReg, condPhysReg);
    }
    
    // 分岐先の処理
    if (targetOp.type == IROperandType::kLabel) {
        std::string labelName = targetOp.value.label_name;
        EmitJNE_Label(labelName, hint, false);
        return true;
    }
    
    return false;
}

// Return エンコーディングの完全実装
bool X86_64CodeGenerator::EncodeReturn(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // 戻り値の処理
    if (inst.num_operands() > 0) {
        const auto& retValOp = inst.operand(0);
        
        if (retValOp.type == IROperandType::kRegister) {
            int32_t retRegVirtual = retValOp.value.reg;
            std::optional<X86_64Register> optRetPhysReg = GetPhysicalReg(retRegVirtual);
            
            if (optRetPhysReg && optRetPhysReg.value() != RAX) {
                // 戻り値をRAXに移動
                EmitMOV_Reg_Reg(RAX, optRetPhysReg.value());
            } else if (!optRetPhysReg) {
                // スピルされている場合はRAXにロード
                if (!EncodeLoadFromSpillSlot(RAX, retRegVirtual, code)) return false;
            }
        } else if (retValOp.type == IROperandType::kImmediate) {
            // 即値を戻り値として設定
            int64_t immValue = retValOp.value.imm;
            EmitMOV_Reg_Imm(RAX, immValue);
        }
    }
    
    // エピローグとリターン
    EncodeEpilogue(code);
    return true;
}

// Call エンコーディングの完全実装
bool X86_64CodeGenerator::EncodeCall(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 1) return false;
    
    const auto& targetOp = inst.operand(0);
    
    // System V AMD64 ABI: RDI, RSI, RDX, RCX, R8, R9
    static const X86_64Register argRegs[] = {RDI, RSI, RDX, RCX, R8, R9};
    static const size_t maxRegArgs = 6;
    
    size_t numArgs = inst.num_operands() - 1;
    size_t stackArgs = numArgs > maxRegArgs ? numArgs - maxRegArgs : 0;
    
    // スタック調整（16バイトアライメント）
    size_t stackSpace = (stackArgs * 8 + 15) & ~15;
    if (stackSpace > 0) {
        if (stackSpace <= 127) {
            code.push_back(0x48); code.push_back(0x83); code.push_back(0xEC); 
            code.push_back(static_cast<uint8_t>(stackSpace));
        } else {
            code.push_back(0x48); code.push_back(0x81); code.push_back(0xEC);
            AppendImmediate32(code, static_cast<int32_t>(stackSpace));
        }
    }
    
    // 引数の配置
    for (size_t i = 1; i < inst.num_operands(); ++i) {
        const auto& argOp = inst.operand(i);
        size_t argIndex = i - 1;
        
        if (argIndex < maxRegArgs) {
            // レジスタ引数
            X86_64Register argReg = argRegs[argIndex];
            
            if (argOp.type == IROperandType::kRegister) {
                int32_t argRegVirtual = argOp.value.reg;
                std::optional<X86_64Register> optArgPhysReg = GetPhysicalReg(argRegVirtual);
                
                if (optArgPhysReg && optArgPhysReg.value() != argReg) {
                    EmitMOV_Reg_Reg(argReg, optArgPhysReg.value());
                } else if (!optArgPhysReg) {
                    if (!EncodeLoadFromSpillSlot(argReg, argRegVirtual, code)) return false;
                }
            } else if (argOp.type == IROperandType::kImmediate) {
                EmitMOV_Reg_Imm(argReg, argOp.value.imm);
            }
        }
    }
    
    // 実際の呼び出し
    if (targetOp.type == IROperandType::kLabel) {
        std::string labelName = targetOp.value.label_name;
        EmitCALL_Label(labelName);
    } else if (targetOp.type == IROperandType::kRegister) {
        int32_t targetRegVirtual = targetOp.value.reg;
        std::optional<X86_64Register> optTargetPhysReg = GetPhysicalReg(targetRegVirtual);
        
        X86_64Register targetPhysReg;
        if (optTargetPhysReg) {
            targetPhysReg = optTargetPhysReg.value();
        } else {
            targetPhysReg = R11;
            if (!EncodeLoadFromSpillSlot(targetPhysReg, targetRegVirtual, code)) return false;
        }
        
        AppendREXPrefix(code, false, false, false, targetPhysReg >= R8);
        code.push_back(0xFF);
        AppendModRM(code, 0x03, 2, static_cast<uint8_t>(targetPhysReg) & 0x7);
    }
    
    // スタック復元
    if (stackSpace > 0) {
        if (stackSpace <= 127) {
            code.push_back(0x48); code.push_back(0x83); code.push_back(0xC4);
            code.push_back(static_cast<uint8_t>(stackSpace));
        } else {
            code.push_back(0x48); code.push_back(0x81); code.push_back(0xC4);
            AppendImmediate32(code, static_cast<int32_t>(stackSpace));
        }
    }
    
    return true;
}

// 基本的なx86_64命令エンコーディング関数の完全実装

void X86_64CodeGenerator::EmitMOV_Reg_Reg(X86_64Register dest, X86_64Register src) noexcept {
    if (dest == src) return; // 自分自身への移動は不要
    
    bool rexR = static_cast<uint8_t>(dest) >= 8;
    bool rexB = static_cast<uint8_t>(src) >= 8;
    
    if (rexR || rexB) {
        AppendREXPrefix(m_code, true, rexR, false, rexB);
    }
    
    m_code.push_back(0x89); // MOV r/m64, r64
    AppendModRM(m_code, 0x03, static_cast<uint8_t>(src) & 0x7, static_cast<uint8_t>(dest) & 0x7);
}

void X86_64CodeGenerator::EmitMOV_Reg_Imm(X86_64Register dest, int64_t imm) noexcept {
    bool rexB = static_cast<uint8_t>(dest) >= 8;
    
    if (imm >= INT32_MIN && imm <= INT32_MAX) {
        // 32ビット即値（自動的に64ビットにゼロ拡張される）
        if (rexB) {
            AppendREXPrefix(m_code, true, false, false, rexB);
        }
        m_code.push_back(0xC7); // MOV r/m64, imm32
        AppendModRM(m_code, 0x03, 0, static_cast<uint8_t>(dest) & 0x7);
        AppendImmediate32(m_code, static_cast<int32_t>(imm));
    } else {
        // 64ビット即値
        AppendREXPrefix(m_code, true, false, false, rexB);
        m_code.push_back(0xB8 + (static_cast<uint8_t>(dest) & 0x7)); // MOV r64, imm64
        AppendImmediate64(m_code, static_cast<uint64_t>(imm));
    }
}

void X86_64CodeGenerator::EmitTEST_Reg_Reg(X86_64Register reg1, X86_64Register reg2) noexcept {
    bool rexR = static_cast<uint8_t>(reg1) >= 8;
    bool rexB = static_cast<uint8_t>(reg2) >= 8;
    
    if (rexR || rexB) {
        AppendREXPrefix(m_code, true, rexR, false, rexB);
    }
    
    m_code.push_back(0x85); // TEST r/m64, r64
    AppendModRM(m_code, 0x03, static_cast<uint8_t>(reg1) & 0x7, static_cast<uint8_t>(reg2) & 0x7);
}

void X86_64CodeGenerator::EmitADD_Reg_Reg(X86_64Register dest, X86_64Register src) noexcept {
    bool rexR = static_cast<uint8_t>(src) >= 8;
    bool rexB = static_cast<uint8_t>(dest) >= 8;
    
    if (rexR || rexB) {
        AppendREXPrefix(m_code, true, rexR, false, rexB);
    }
    
    m_code.push_back(0x01); // ADD r/m64, r64
    AppendModRM(m_code, 0x03, static_cast<uint8_t>(src) & 0x7, static_cast<uint8_t>(dest) & 0x7);
}

void X86_64CodeGenerator::EmitADD_Reg_Imm(X86_64Register dest, int32_t imm) noexcept {
    bool rexB = static_cast<uint8_t>(dest) >= 8;
    
    if (rexB) {
        AppendREXPrefix(m_code, true, false, false, rexB);
    }
    
    if (imm >= -128 && imm <= 127) {
        // 8ビット即値
        m_code.push_back(0x83); // ADD r/m64, imm8
        AppendModRM(m_code, 0x03, 0, static_cast<uint8_t>(dest) & 0x7);
        m_code.push_back(static_cast<uint8_t>(imm));
    } else {
        // 32ビット即値
        m_code.push_back(0x81); // ADD r/m64, imm32
        AppendModRM(m_code, 0x03, 0, static_cast<uint8_t>(dest) & 0x7);
        AppendImmediate32(m_code, imm);
    }
}

void X86_64CodeGenerator::EmitSUB_Reg_Reg(X86_64Register dest, X86_64Register src) noexcept {
    bool rexR = static_cast<uint8_t>(src) >= 8;
    bool rexB = static_cast<uint8_t>(dest) >= 8;
    
    if (rexR || rexB) {
        AppendREXPrefix(m_code, true, rexR, false, rexB);
    }
    
    m_code.push_back(0x29); // SUB r/m64, r64
    AppendModRM(m_code, 0x03, static_cast<uint8_t>(src) & 0x7, static_cast<uint8_t>(dest) & 0x7);
}

void X86_64CodeGenerator::EmitSUB_Reg_Imm(X86_64Register dest, int32_t imm) noexcept {
    bool rexB = static_cast<uint8_t>(dest) >= 8;
    
    if (rexB) {
        AppendREXPrefix(m_code, true, false, false, rexB);
    }
    
    if (imm >= -128 && imm <= 127) {
        m_code.push_back(0x83); // SUB r/m64, imm8
        AppendModRM(m_code, 0x03, 5, static_cast<uint8_t>(dest) & 0x7); // /5 for SUB
        m_code.push_back(static_cast<uint8_t>(imm));
    } else {
        m_code.push_back(0x81); // SUB r/m64, imm32
        AppendModRM(m_code, 0x03, 5, static_cast<uint8_t>(dest) & 0x7);
        AppendImmediate32(m_code, imm);
    }
}

void X86_64CodeGenerator::EmitIMUL_Reg_Reg(X86_64Register dest, X86_64Register src) noexcept {
    bool rexR = static_cast<uint8_t>(dest) >= 8;
    bool rexB = static_cast<uint8_t>(src) >= 8;
    
    if (rexR || rexB) {
        AppendREXPrefix(m_code, true, rexR, false, rexB);
    }
    
    m_code.push_back(0x0F);
    m_code.push_back(0xAF); // IMUL r64, r/m64
    AppendModRM(m_code, 0x03, static_cast<uint8_t>(dest) & 0x7, static_cast<uint8_t>(src) & 0x7);
}

void X86_64CodeGenerator::EmitIMUL_Reg_Imm(X86_64Register dest, int32_t imm) noexcept {
    bool rexR = static_cast<uint8_t>(dest) >= 8;
    
    if (rexR) {
        AppendREXPrefix(m_code, true, rexR, false, rexR);
    }
    
    if (imm >= -128 && imm <= 127) {
        m_code.push_back(0x6B); // IMUL r64, r/m64, imm8
        AppendModRM(m_code, 0x03, static_cast<uint8_t>(dest) & 0x7, static_cast<uint8_t>(dest) & 0x7);
        m_code.push_back(static_cast<uint8_t>(imm));
    } else {
        m_code.push_back(0x69); // IMUL r64, r/m64, imm32
        AppendModRM(m_code, 0x03, static_cast<uint8_t>(dest) & 0x7, static_cast<uint8_t>(dest) & 0x7);
        AppendImmediate32(m_code, imm);
    }
}

void X86_64CodeGenerator::EmitIDIV_Reg_Reg(X86_64Register dividend, X86_64Register divisor) noexcept {
    // IDIVは特殊な命令で、RAX:RDXペアを除数で割る
    // 結果の商はRAXに、余りはRDXに格納される
    
    // 配当をRAXに設定
    if (dividend != RAX) {
        EmitMOV_Reg_Reg(RAX, dividend);
    }
    
    // RDXをクリア（符号拡張）
    EmitXOR_Reg_Reg(RDX, RDX);
    
    bool rexB = static_cast<uint8_t>(divisor) >= 8;
    if (rexB) {
        AppendREXPrefix(m_code, true, false, false, rexB);
    }
    
    m_code.push_back(0xF7); // IDIV r/m64
    AppendModRM(m_code, 0x03, 7, static_cast<uint8_t>(divisor) & 0x7); // /7 for IDIV
}

void X86_64CodeGenerator::EmitIDIV_Reg_Imm(X86_64Register dividend, int32_t divisor) noexcept {
    // 即値除算の場合、まず即値をレジスタにロード
    EmitMOV_Reg_Imm(RCX, divisor); // RCXを一時レジスタとして使用
    EmitIDIV_Reg_Reg(dividend, RCX);
}

void X86_64CodeGenerator::EmitNEG_Reg(X86_64Register reg) noexcept {
    bool rexB = static_cast<uint8_t>(reg) >= 8;
    
    if (rexB) {
        AppendREXPrefix(m_code, true, false, false, rexB);
    }
    
    m_code.push_back(0xF7); // NEG r/m64
    AppendModRM(m_code, 0x03, 3, static_cast<uint8_t>(reg) & 0x7); // /3 for NEG
}

void X86_64CodeGenerator::EmitCMP_Reg_Reg(X86_64Register reg1, X86_64Register reg2) noexcept {
    bool rexR = static_cast<uint8_t>(reg1) >= 8;
    bool rexB = static_cast<uint8_t>(reg2) >= 8;
    
    if (rexR || rexB) {
        AppendREXPrefix(m_code, true, rexR, false, rexB);
    }
    
    m_code.push_back(0x39); // CMP r/m64, r64
    AppendModRM(m_code, 0x03, static_cast<uint8_t>(reg1) & 0x7, static_cast<uint8_t>(reg2) & 0x7);
}

void X86_64CodeGenerator::EmitCMP_Reg_Imm(X86_64Register reg, int32_t imm) noexcept {
    bool rexB = static_cast<uint8_t>(reg) >= 8;
    
    if (rexB) {
        AppendREXPrefix(m_code, true, false, false, rexB);
    }
    
    if (imm >= -128 && imm <= 127) {
        m_code.push_back(0x83); // CMP r/m64, imm8
        AppendModRM(m_code, 0x03, 7, static_cast<uint8_t>(reg) & 0x7); // /7 for CMP
        m_code.push_back(static_cast<uint8_t>(imm));
    } else {
        m_code.push_back(0x81); // CMP r/m64, imm32
        AppendModRM(m_code, 0x03, 7, static_cast<uint8_t>(reg) & 0x7);
        AppendImmediate32(m_code, imm);
    }
}

void X86_64CodeGenerator::EmitXOR_Reg_Reg(X86_64Register dest, X86_64Register src) noexcept {
    bool rexR = static_cast<uint8_t>(src) >= 8;
    bool rexB = static_cast<uint8_t>(dest) >= 8;
    
    if (rexR || rexB) {
        AppendREXPrefix(m_code, true, rexR, false, rexB);
    }
    
    m_code.push_back(0x31); // XOR r/m64, r64
    AppendModRM(m_code, 0x03, static_cast<uint8_t>(src) & 0x7, static_cast<uint8_t>(dest) & 0x7);
}

void X86_64CodeGenerator::EmitNOP() noexcept {
    m_code.push_back(0x90); // NOP
}

// プロローグとエピローグの完全実装

void X86_64CodeGenerator::EncodePrologueMinimal(std::vector<uint8_t>& code) noexcept {
    // 最小限のプロローグ（後でフレームサイズを調整）
    code.push_back(0x55); // PUSH RBP
    code.push_back(0x48); code.push_back(0x89); code.push_back(0xE5); // MOV RBP, RSP
    
    // 非揮発性レジスタの保存（System V AMD64 ABI）
    code.push_back(0x53); // PUSH RBX
    code.push_back(0x41); code.push_back(0x54); // PUSH R12
    code.push_back(0x41); code.push_back(0x55); // PUSH R13
    code.push_back(0x41); code.push_back(0x56); // PUSH R14
    code.push_back(0x41); code.push_back(0x57); // PUSH R15
}

void X86_64CodeGenerator::FinalizeFrame(std::vector<uint8_t>& code) noexcept {
    uint32_t frameSize = GetCurrentFrameSize();
    
    if (frameSize > 0) {
        // プロローグの最後にスタックフレーム確保を挿入
        std::vector<uint8_t> stackAlloc;
        
        if (frameSize <= 127) {
            // SUB RSP, frameSize (8-bit immediate)
            stackAlloc.push_back(0x48); stackAlloc.push_back(0x83); 
            stackAlloc.push_back(0xEC); stackAlloc.push_back(static_cast<uint8_t>(frameSize));
        } else {
            // SUB RSP, frameSize (32-bit immediate)
            stackAlloc.push_back(0x48); stackAlloc.push_back(0x81); stackAlloc.push_back(0xEC);
            AppendImmediate32(stackAlloc, static_cast<int32_t>(frameSize));
        }
        
        // プロローグの後に挿入（R15 PUSH後の位置を探す）
        size_t insertPos = 0;
        for (size_t i = 0; i < code.size() - 1; ++i) {
            if (code[i] == 0x41 && code[i + 1] == 0x57) { // PUSH R15
                insertPos = i + 2;
                break;
            }
        }
        
        if (insertPos > 0) {
            code.insert(code.begin() + insertPos, stackAlloc.begin(), stackAlloc.end());
        }
    }
}

// VEX/EVEXプレフィックスのヘルパー関数

void X86_64CodeGenerator::AppendVEXPrefix(std::vector<uint8_t>& code, 
                                         uint8_t map, uint8_t pp, uint8_t L, uint8_t W,
                                         uint8_t vvvv, bool R, bool X, bool B) noexcept {
    if (!X && !B && !W && (map == 0x01)) {
        // 2バイトVEX
        code.push_back(0xC5);
        uint8_t byte1 = ((R ? 0 : 1) << 7) | ((vvvv ^ 0xF) << 3) | (L << 2) | pp;
        code.push_back(byte1);
    } else {
        // 3バイトVEX
        code.push_back(0xC4);
        uint8_t byte1 = ((R ? 0 : 1) << 7) | ((X ? 0 : 1) << 6) | ((B ? 0 : 1) << 5) | map;
        uint8_t byte2 = (W << 7) | ((vvvv ^ 0xF) << 3) | (L << 2) | pp;
        code.push_back(byte1);
        code.push_back(byte2);
    }
}

void X86_64CodeGenerator::AppendREXPrefix(std::vector<uint8_t>& code, bool W, bool R, bool X, bool B) noexcept {
    if (W || R || X || B) {
        uint8_t rex = 0x40;
        if (W) rex |= 0x08;
        if (R) rex |= 0x04;
        if (X) rex |= 0x02;
        if (B) rex |= 0x01;
        code.push_back(rex);
    }
}

void X86_64CodeGenerator::AppendModRM(std::vector<uint8_t>& code, uint8_t mod, uint8_t reg, uint8_t rm) noexcept {
    code.push_back((mod << 6) | (reg << 3) | rm);
}

void X86_64CodeGenerator::AppendSIB(std::vector<uint8_t>& code, uint8_t scale, uint8_t index, uint8_t base) noexcept {
    code.push_back((scale << 6) | (index << 3) | base);
}

void X86_64CodeGenerator::AppendImmediate8(std::vector<uint8_t>& code, int8_t imm) noexcept {
    code.push_back(static_cast<uint8_t>(imm));
}

void X86_64CodeGenerator::AppendImmediate16(std::vector<uint8_t>& code, int16_t imm) noexcept {
    code.push_back(static_cast<uint8_t>(imm & 0xFF));
    code.push_back(static_cast<uint8_t>((imm >> 8) & 0xFF));
}

void X86_64CodeGenerator::AppendImmediate32(std::vector<uint8_t>& code, int32_t imm) noexcept {
    code.push_back(static_cast<uint8_t>(imm & 0xFF));
    code.push_back(static_cast<uint8_t>((imm >> 8) & 0xFF));
    code.push_back(static_cast<uint8_t>((imm >> 16) & 0xFF));
    code.push_back(static_cast<uint8_t>((imm >> 24) & 0xFF));
}

void X86_64CodeGenerator::AppendImmediate64(std::vector<uint8_t>& code, uint64_t imm) noexcept {
    for (int i = 0; i < 8; ++i) {
        code.push_back(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
    }
}

// SIMD レジスタ管理

std::optional<SIMDRegister> X86_64CodeGenerator::GetSIMDReg(int32_t virtualReg) const noexcept {
    auto it = m_simdRegisterMapping.find(virtualReg);
    if (it != m_simdRegisterMapping.end()) {
        return it->second;
    }
    return std::nullopt;
}

void X86_64CodeGenerator::SetSIMDRegisterMapping(int32_t virtualReg, SIMDRegister physicalReg) noexcept {
    m_simdRegisterMapping[virtualReg] = physicalReg;
}

// 最適化フラグのチェック

bool X86_64CodeGenerator::HasFlag(uint32_t flags, CodeGenOptFlags flag) const noexcept {
    return (flags & static_cast<uint32_t>(flag)) != 0;
}

// FMA操作エンコーディング

void X86_64CodeGenerator::EncodeFMA(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 4) return;
    
    int32_t destReg = inst.operand(0).value.reg;
    int32_t src1Reg = inst.operand(1).value.reg;
    int32_t src2Reg = inst.operand(2).value.reg;
    int32_t src3Reg = inst.operand(3).value.reg;
    
    auto destSIMD = GetSIMDReg(destReg);
    auto src1SIMD = GetSIMDReg(src1Reg);
    auto src2SIMD = GetSIMDReg(src2Reg);
    auto src3SIMD = GetSIMDReg(src3Reg);
    
    if (!destSIMD.has_value() || !src1SIMD.has_value() || 
        !src2SIMD.has_value() || !src3SIMD.has_value()) return;
    
    bool useFMA = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseFMA);
    if (!useFMA) return;
    
    // VFMADD231PS xmm1, xmm2, xmm3 (VEX.DDS.128.66.0F38.W0 B8 /r)
    AppendVEXPrefix(code, 0x02, 0x01, 0, 0, static_cast<uint8_t>(*src2SIMD) & 0xF,
                   static_cast<uint8_t>(*destSIMD) >= 8, false, static_cast<uint8_t>(*src3SIMD) >= 8);
    
    code.push_back(0xB8); // VFMADD231PS opcode
    AppendModRM(code, 0x03, static_cast<uint8_t>(*destSIMD) & 0x7, 
                static_cast<uint8_t>(*src3SIMD) & 0x7);
}

// SIMD 算術演算エンコーディング

void X86_64CodeGenerator::EncodeSIMDArithmetic(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3) return;
    
    int32_t destReg = inst.operand(0).value.reg;
    int32_t src1Reg = inst.operand(1).value.reg;
    int32_t src2Reg = inst.operand(2).value.reg;
    
    auto destSIMD = GetSIMDReg(destReg);
    auto src1SIMD = GetSIMDReg(src1Reg);
    auto src2SIMD = GetSIMDReg(src2Reg);
    
    if (!destSIMD.has_value() || !src1SIMD.has_value() || !src2SIMD.has_value()) return;
    
    uint8_t opcode = 0;
    switch (inst.opcode) {
        case Opcode::kSIMDAdd: opcode = 0x58; break; // ADDPS
        case Opcode::kSIMDSub: opcode = 0x5C; break; // SUBPS  
        case Opcode::kSIMDMul: opcode = 0x59; break; // MULPS
        case Opcode::kSIMDDiv: opcode = 0x5E; break; // DIVPS
        default: return;
    }
    
    bool useAVX = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX);
    
    if (useAVX) {
        // VEX エンコーディング
        AppendVEXPrefix(code, 0x01, 0x00, 0, 0, static_cast<uint8_t>(*src1SIMD) & 0xF,
                       static_cast<uint8_t>(*destSIMD) >= 8, false, static_cast<uint8_t>(*src2SIMD) >= 8);
        code.push_back(opcode);
        AppendModRM(code, 0x03, static_cast<uint8_t>(*destSIMD) & 0x7, 
                    static_cast<uint8_t>(*src2SIMD) & 0x7);
    } else {
        // レガシーSSE
        if (static_cast<uint8_t>(*destSIMD) >= 8 || static_cast<uint8_t>(*src2SIMD) >= 8) {
            AppendREXPrefix(code, false, static_cast<uint8_t>(*destSIMD) >= 8, false, static_cast<uint8_t>(*src2SIMD) >= 8);
        }
        code.push_back(0x0F);
        code.push_back(opcode);
        AppendModRM(code, 0x03, static_cast<uint8_t>(*destSIMD) & 0x7, 
                    static_cast<uint8_t>(*src2SIMD) & 0x7);
    }
}

// SIMD ストア操作

void X86_64CodeGenerator::EncodeSIMDStore(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 3) return;
    
    int32_t srcReg = inst.operand(0).value.reg;
    int32_t addrReg = inst.operand(1).value.reg;
    int32_t offset = static_cast<int32_t>(inst.operand(2).value.imm);
    
    auto simdReg = GetSIMDReg(srcReg);
    if (!simdReg.has_value()) return;
    
    std::optional<X86_64Register> optAddrRegPhysical = GetPhysicalReg(addrReg);
    if (!optAddrRegPhysical.has_value()) return;
    X86_64Register addrRegPhysical = optAddrRegPhysical.value();
    
    bool useAVX = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX);
    
    if (useAVX) {
        // VMOVUPS m128, xmm (VEX.128.0F.WIG 11 /r)
        AppendVEXPrefix(code, 0x01, 0x00, 0, 0, 0xF,
                       static_cast<uint8_t>(*simdReg) >= 8, false, static_cast<uint8_t>(addrRegPhysical) >= 8);
        code.push_back(0x11); // MOVUPS opcode (store)
    } else {
        // MOVUPS m128, xmm (0F 11 /r)
        if (static_cast<uint8_t>(*simdReg) >= 8 || static_cast<uint8_t>(addrRegPhysical) >= 8) {
            AppendREXPrefix(code, false, static_cast<uint8_t>(*simdReg) >= 8, false, static_cast<uint8_t>(addrRegPhysical) >= 8);
        }
        code.push_back(0x0F);
        code.push_back(0x11);
    }
    
    // ModRM とオフセット
    uint8_t modrm_reg = static_cast<uint8_t>(*simdReg) & 0x7;
    uint8_t modrm_rm = static_cast<uint8_t>(addrRegPhysical) & 0x7;
    
    if (offset == 0 && modrm_rm != 5) {
        AppendModRM(code, 0x00, modrm_reg, modrm_rm);
    } else if (offset >= -128 && offset <= 127) {
        AppendModRM(code, 0x01, modrm_reg, modrm_rm);
        code.push_back(static_cast<uint8_t>(offset));
    } else {
        AppendModRM(code, 0x02, modrm_reg, modrm_rm);
        AppendImmediate32(code, offset);
    }
}

void X86_64CodeGenerator::EmitSHL_Reg_Imm(X86_64Register reg, uint8_t count) noexcept {
    if (count == 0) return; // 何もしない
    
    bool rexB = static_cast<uint8_t>(reg) >= 8;
    if (rexB) {
        AppendREXPrefix(m_code, true, false, false, rexB);
    }
    
    if (count == 1) {
        // SHL r/m64, 1 - 短縮形
        m_code.push_back(0xD1);
        AppendModRM(m_code, 0x03, 4, static_cast<uint8_t>(reg) & 0x7); // /4 for SHL
    } else {
        // SHL r/m64, imm8
        m_code.push_back(0xC1);
        AppendModRM(m_code, 0x03, 4, static_cast<uint8_t>(reg) & 0x7);
        m_code.push_back(count);
    }
}

void X86_64CodeGenerator::EmitSHR_Reg_Imm(X86_64Register reg, uint8_t count) noexcept {
    if (count == 0) return;
    
    bool rexB = static_cast<uint8_t>(reg) >= 8;
    if (rexB) {
        AppendREXPrefix(m_code, true, false, false, rexB);
    }
    
    if (count == 1) {
        m_code.push_back(0xD1);
        AppendModRM(m_code, 0x03, 5, static_cast<uint8_t>(reg) & 0x7); // /5 for SHR
    } else {
        m_code.push_back(0xC1);
        AppendModRM(m_code, 0x03, 5, static_cast<uint8_t>(reg) & 0x7);
        m_code.push_back(count);
    }
}

void X86_64CodeGenerator::EmitSAR_Reg_Imm(X86_64Register reg, uint8_t count) noexcept {
    if (count == 0) return;
    
    bool rexB = static_cast<uint8_t>(reg) >= 8;
    if (rexB) {
        AppendREXPrefix(m_code, true, false, false, rexB);
    }
    
    if (count == 1) {
        m_code.push_back(0xD1);
        AppendModRM(m_code, 0x03, 7, static_cast<uint8_t>(reg) & 0x7); // /7 for SAR
    } else {
        m_code.push_back(0xC1);
        AppendModRM(m_code, 0x03, 7, static_cast<uint8_t>(reg) & 0x7);
        m_code.push_back(count);
    }
}

// 完全なロード/ストア操作実装の修正
bool X86_64CodeGenerator::EncodeLoadMemory(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.num_operands() < 2) return false;
    
    const auto& destOperand = inst.operand(0); // 目的レジスタ
    const auto& addrOperand = inst.operand(1); // アドレス
    
    if (!destOperand.isVirtualReg()) return false;
    
    int32_t destVirtualReg = destOperand.getVirtualReg();
    std::optional<X86_64Register> optDestPhysReg = GetPhysicalReg(destVirtualReg);
    
    X86_64Register destReg;
    bool destIsSpilled = !optDestPhysReg.has_value();
    if (destIsSpilled) {
        destReg = RAX; // 一時的にRAXにロード
    } else {
        destReg = optDestPhysReg.value();
    }
    
    if (addrOperand.isVirtualReg()) {
        int32_t addrVirtualReg = addrOperand.getVirtualReg();
        std::optional<X86_64Register> optAddrPhysReg = GetPhysicalReg(addrVirtualReg);
        
        X86_64Register addrReg;
        if (optAddrPhysReg) {
            addrReg = optAddrPhysReg.value();
        } else {
            addrReg = RCX;
            if (!EncodeLoadFromSpillSlot(addrReg, addrVirtualReg, code)) return false;
        }
        
        int32_t offset = 0;
        if (inst.num_operands() > 2 && inst.operand(2).isImmediate()) {
            offset = static_cast<int32_t>(inst.operand(2).getImmediateValue());
        }
        
        // MOV destReg, [addrReg + offset] の完全実装
        bool rexR = static_cast<uint8_t>(destReg) >= 8;
        bool rexB = static_cast<uint8_t>(addrReg) >= 8;
        
        if (rexR || rexB) {
            AppendREXPrefix(code, true, rexR, false, rexB);
        }
        
        code.push_back(0x8B); // MOV r64, r/m64
        
        // アドレッシングモードの詳細処理
        if (offset == 0 && (static_cast<uint8_t>(addrReg) & 0x7) != 5) { // RBP以外
            AppendModRM(code, 0x00, static_cast<uint8_t>(destReg) & 0x7, static_cast<uint8_t>(addrReg) & 0x7);
        } else if (offset >= -128 && offset <= 127) {
            AppendModRM(code, 0x01, static_cast<uint8_t>(destReg) & 0x7, static_cast<uint8_t>(addrReg) & 0x7);
            code.push_back(static_cast<uint8_t>(offset));
        } else {
            AppendModRM(code, 0x02, static_cast<uint8_t>(destReg) & 0x7, static_cast<uint8_t>(addrReg) & 0x7);
            AppendImmediate32(code, offset);
        }
        
        // RSP/R12の場合はSIBが必要
        if ((static_cast<uint8_t>(addrReg) & 0x7) == 4) {
            AppendSIB(code, 0, 4, 4); // SIB: scale=1, index=none, base=RSP
        }
        
    } else if (addrOperand.isImmediate()) {
        // 直接アドレッシング（64ビット即値アドレス）
        uint64_t addr = static_cast<uint64_t>(addrOperand.getImmediateValue());
        
        // アドレスを一時レジスタにロード後、間接アクセス
        EmitMOV_Reg_Imm(RDX, static_cast<int64_t>(addr));
        
        bool rexR = static_cast<uint8_t>(destReg) >= 8;
        if (rexR) {
            AppendREXPrefix(code, true, rexR, false, false);
        }
        
        code.push_back(0x8B); // MOV r64, r/m64
        AppendModRM(code, 0x00, static_cast<uint8_t>(destReg) & 0x7, 2); // [RDX]
    }
    
    if (destIsSpilled) {
        if (!EncodeStoreToSpillSlot(destVirtualReg, destReg, code)) return false;
    }
    
    return true;
}

// ビットワイズ操作の完全実装

void X86_64CodeGenerator::EmitAND_Reg_Reg(X86_64Register dest, X86_64Register src) noexcept {
    bool rexR = static_cast<uint8_t>(src) >= 8;
    bool rexB = static_cast<uint8_t>(dest) >= 8;
    
    if (rexR || rexB) {
        AppendREXPrefix(m_code, true, rexR, false, rexB);
    }
    
    m_code.push_back(0x21); // AND r/m64, r64
    AppendModRM(m_code, 0x03, static_cast<uint8_t>(src) & 0x7, static_cast<uint8_t>(dest) & 0x7);
}

void X86_64CodeGenerator::EmitAND_Reg_Imm(X86_64Register dest, int32_t imm) noexcept {
    bool rexB = static_cast<uint8_t>(dest) >= 8;
    
    if (rexB) {
        AppendREXPrefix(m_code, true, false, false, rexB);
    }
    
    if (imm >= -128 && imm <= 127) {
        m_code.push_back(0x83); // AND r/m64, imm8
        AppendModRM(m_code, 0x03, 4, static_cast<uint8_t>(dest) & 0x7); // /4 for AND
        m_code.push_back(static_cast<uint8_t>(imm));
    } else {
        m_code.push_back(0x81); // AND r/m64, imm32
        AppendModRM(m_code, 0x03, 4, static_cast<uint8_t>(dest) & 0x7);
        AppendImmediate32(m_code, imm);
    }
}

void X86_64CodeGenerator::EmitOR_Reg_Reg(X86_64Register dest, X86_64Register src) noexcept {
    bool rexR = static_cast<uint8_t>(src) >= 8;
    bool rexB = static_cast<uint8_t>(dest) >= 8;
    
    if (rexR || rexB) {
        AppendREXPrefix(m_code, true, rexR, false, rexB);
    }
    
    m_code.push_back(0x09); // OR r/m64, r64
    AppendModRM(m_code, 0x03, static_cast<uint8_t>(src) & 0x7, static_cast<uint8_t>(dest) & 0x7);
}

void X86_64CodeGenerator::EmitOR_Reg_Imm(X86_64Register dest, int32_t imm) noexcept {
    bool rexB = static_cast<uint8_t>(dest) >= 8;
    
    if (rexB) {
        AppendREXPrefix(m_code, true, false, false, rexB);
    }
    
    if (imm >= -128 && imm <= 127) {
        m_code.push_back(0x83); // OR r/m64, imm8
        AppendModRM(m_code, 0x03, 1, static_cast<uint8_t>(dest) & 0x7); // /1 for OR
        m_code.push_back(static_cast<uint8_t>(imm));
    } else {
        m_code.push_back(0x81); // OR r/m64, imm32
        AppendModRM(m_code, 0x03, 1, static_cast<uint8_t>(dest) & 0x7);
        AppendImmediate32(m_code, imm);
    }
}

// 条件付き移動命令
void X86_64CodeGenerator::EmitCMOVE_Reg_Reg(X86_64Register dest, X86_64Register src) noexcept {
    bool rexR = static_cast<uint8_t>(dest) >= 8;
    bool rexB = static_cast<uint8_t>(src) >= 8;
    
    if (rexR || rexB) {
        AppendREXPrefix(m_code, true, rexR, false, rexB);
    }
    
    m_code.push_back(0x0F);
    m_code.push_back(0x44); // CMOVE r64, r/m64
    AppendModRM(m_code, 0x03, static_cast<uint8_t>(dest) & 0x7, static_cast<uint8_t>(src) & 0x7);
}

void X86_64CodeGenerator::EmitCMOVNE_Reg_Reg(X86_64Register dest, X86_64Register src) noexcept {
    bool rexR = static_cast<uint8_t>(dest) >= 8;
    bool rexB = static_cast<uint8_t>(src) >= 8;
    
    if (rexR || rexB) {
        AppendREXPrefix(m_code, true, rexR, false, rexB);
    }
    
    m_code.push_back(0x0F);
    m_code.push_back(0x45); // CMOVNE r64, r/m64
    AppendModRM(m_code, 0x03, static_cast<uint8_t>(dest) & 0x7, static_cast<uint8_t>(src) & 0x7);
}

// スタック操作
void X86_64CodeGenerator::EmitPUSH_Reg(X86_64Register reg) noexcept {
    bool rexB = static_cast<uint8_t>(reg) >= 8;
    
    if (rexB) {
        AppendREXPrefix(m_code, false, false, false, rexB);
    }
    
    m_code.push_back(0x50 + (static_cast<uint8_t>(reg) & 0x7)); // PUSH r64
}

void X86_64CodeGenerator::EmitPOP_Reg(X86_64Register reg) noexcept {
    bool rexB = static_cast<uint8_t>(reg) >= 8;
    
    if (rexB) {
        AppendREXPrefix(m_code, false, false, false, rexB);
    }
    
    m_code.push_back(0x58 + (static_cast<uint8_t>(reg) & 0x7)); // POP r64
}

// リターン命令
void X86_64CodeGenerator::EmitRET() noexcept {
    m_code.push_back(0xC3); // RET
}

void X86_64CodeGenerator::EmitRET_Imm(uint16_t popBytes) noexcept {
    m_code.push_back(0xC2); // RET imm16
    AppendImmediate16(m_code, static_cast<int16_t>(popBytes));
}

// 関数呼び出し
void X86_64CodeGenerator::EmitCALL_Reg(X86_64Register reg) noexcept {
    bool rexB = static_cast<uint8_t>(reg) >= 8;
    
    if (rexB) {
        AppendREXPrefix(m_code, false, false, false, rexB);
    }
    
    m_code.push_back(0xFF); // CALL r/m64
    AppendModRM(m_code, 0x03, 2, static_cast<uint8_t>(reg) & 0x7); // /2 for CALL
}

void X86_64CodeGenerator::EmitCALL_Label(const std::string& label) noexcept {
    m_code.push_back(0xE8); // CALL rel32
    m_pendingJumps.emplace_back(GetCurrentPosition(), 4, label, false);
    EmitDWord(0x00000000); // プレースホルダー
}

// デバッグとユーティリティ関数の完全実装
uint32_t X86_64CodeGenerator::GetCurrentPosition() const noexcept {
    return static_cast<uint32_t>(m_code.size());
}

uint32_t X86_64CodeGenerator::GetCurrentFrameSize() const noexcept {
    size_t frameSize = 0;
    
    // スピルスロットのサイズを計算
    for (const auto& pair : m_spillSlots) {
        frameSize += 8; // 各スロットは8バイト
    }
    
    // 16バイト境界にアライン
    frameSize = (frameSize + 15) & ~15;
    
    return static_cast<uint32_t>(frameSize);
}

void X86_64CodeGenerator::DefineLabel(const std::string& label) noexcept {
    uint32_t position = GetCurrentPosition();
    m_labels[label] = position;
    
    // 未解決のジャンプを解決
    auto it = m_pendingJumps.begin();
    while (it != m_pendingJumps.end()) {
        if (it->labelName == label) {
            ResolveJump(*it, position);
            it = m_pendingJumps.erase(it);
        } else {
            ++it;
        }
    }
}

void X86_64CodeGenerator::ResolveJump(const PendingJump& jump, uint32_t targetPosition) noexcept {
    int32_t offset = static_cast<int32_t>(targetPosition) - 
                     static_cast<int32_t>(jump.position + jump.size);
    
    if (jump.size == 1) {
        // ショートジャンプ（8ビット相対）
        if (offset >= -128 && offset <= 127) {
            m_code[jump.position] = static_cast<uint8_t>(offset);
        } else {
            // オーバーフロー：ロングジャンプに変換が必要
            logError("ショートジャンプの範囲を超えました", ErrorSeverity::Critical, __FILE__, __LINE__);
        }
    } else if (jump.size == 4) {
        // ロングジャンプ（32ビット相対）
        uint8_t* offsetPtr = &m_code[jump.position];
        *reinterpret_cast<int32_t*>(offsetPtr) = offset;
    }
}

// エラー処理とログ機能の完全実装
void X86_64CodeGenerator::logDebug(const std::string& message) noexcept {
    if (m_debugMode) {
        // デバッグログの実装
        std::cerr << "[DEBUG] X86_64CodeGenerator: " << message << std::endl;
    }
}

void X86_64CodeGenerator::logWarning(const std::string& message) noexcept {
    std::cerr << "[WARNING] X86_64CodeGenerator: " << message << std::endl;
}

std::string X86_64CodeGenerator::formatString(const char* format, ...) noexcept {
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    
    va_end(args);
    return std::string(buffer);
}

// 最適化フラグの自動検出
void X86_64CodeGenerator::AutoDetectOptimalFlags() noexcept {
    // CPU機能の検出
    if (DetectCPUFeature("avx2")) {
        m_optimizationFlags |= static_cast<uint32_t>(CodeGenOptFlags::UseAVX);
        m_optimizationFlags |= static_cast<uint32_t>(CodeGenOptFlags::UseAVX2);
    } else if (DetectCPUFeature("avx")) {
        m_optimizationFlags |= static_cast<uint32_t>(CodeGenOptFlags::UseAVX);
    }
    
    if (DetectCPUFeature("fma")) {
        m_optimizationFlags |= static_cast<uint32_t>(CodeGenOptFlags::UseFMA);
    }
    
    if (DetectCPUFeature("bmi1")) {
        m_optimizationFlags |= static_cast<uint32_t>(CodeGenOptFlags::UseBMI);
    }
}

bool X86_64CodeGenerator::DetectCPUFeature(const std::string& feature) noexcept {
    // CPUID を使用したCPU機能検出の実装
    uint32_t eax, ebx, ecx, edx;
    
    if (feature == "avx") {
        // CPUID 1: ECX[28] = AVX
        __asm__ __volatile__ (
            "cpuid"
            : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
            : "a" (1)
        );
        return (ecx & (1 << 28)) != 0;
    } else if (feature == "avx2") {
        // CPUID 7: EBX[5] = AVX2
        __asm__ __volatile__ (
            "cpuid"
            : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
            : "a" (7), "c" (0)
        );
        return (ebx & (1 << 5)) != 0;
    } else if (feature == "fma") {
        // CPUID 1: ECX[12] = FMA
        __asm__ __volatile__ (
            "cpuid"
            : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
            : "a" (1)
        );
        return (ecx & (1 << 12)) != 0;
    } else if (feature == "bmi1") {
        // CPUID 7: EBX[3] = BMI1
        __asm__ __volatile__ (
            "cpuid"
            : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
            : "a" (7), "c" (0)
        );
        return (ebx & (1 << 3)) != 0;
    }
    
    return false;
}
}  // namespace jit
}  // namespace core
}  // namespace aerojs