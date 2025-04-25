#include "src/core/jit/ir/ir_optimizer.h"

#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <unordered_set>
#include <queue>

namespace aerojs::core {

DeadCodeEliminationPass::DeadCodeEliminationPass() = default;
DeadCodeEliminationPass::~DeadCodeEliminationPass() = default;

bool DeadCodeEliminationPass::run(IRFunction* function) {
    if (!function) {
        return false;
    }
    
    bool changed = false;
    
    // 1. 到達可能性分析を行う
    std::unordered_set<uint32_t> reachableInstructions = findReachableInstructions(function);
    
    // 2. 有効な使用箇所を持つ命令を特定
    std::unordered_set<uint32_t> liveInstructions = findLiveInstructions(function, reachableInstructions);
    
    // 3. 不要な命令を削除
    std::vector<uint32_t> instructionsToRemove;
    for (const auto& instruction : function->instructions()) {
        uint32_t id = instruction.id();
        if (reachableInstructions.find(id) != reachableInstructions.end() && 
            liveInstructions.find(id) == liveInstructions.end()) {
            instructionsToRemove.push_back(id);
        }
    }
    
    // 命令を削除
    for (uint32_t id : instructionsToRemove) {
        function->removeInstruction(id);
        changed = true;
    }
    
    return changed;
}

std::unordered_set<uint32_t> DeadCodeEliminationPass::findReachableInstructions(const IRFunction* function) const {
    std::unordered_set<uint32_t> reachable;
    std::queue<uint32_t> workList;
    
    // エントリポイント（最初の命令）から開始
    if (!function->instructions().empty()) {
        uint32_t entryId = function->instructions().front().id();
        workList.push(entryId);
        reachable.insert(entryId);
    }
    
    // 制御フローを追跡して到達可能な命令を特定
    while (!workList.empty()) {
        uint32_t currentId = workList.front();
        workList.pop();
        
        const IRInstruction* instruction = function->findInstruction(currentId);
        if (!instruction) {
            continue;
        }
        
        // 制御フロー命令の場合、ターゲットも追加
        if (instruction->opcode() == Opcode::Jmp && instruction->operandCount() > 0) {
            Value* target = instruction->operand(0);
            if (target && target->isBlockLabel()) {
                BlockLabelValue* labelValue = static_cast<BlockLabelValue*>(target);
                uint32_t targetId = labelValue->targetInstructionId();
                if (reachable.find(targetId) == reachable.end()) {
                    reachable.insert(targetId);
                    workList.push(targetId);
                }
            }
        } else if ((instruction->opcode() == Opcode::JmpIf || instruction->opcode() == Opcode::JmpIfNot) && 
                   instruction->operandCount() > 1) {
            // 条件付きジャンプの場合、ジャンプ先と次の命令の両方が到達可能
            Value* target = instruction->operand(1);
            if (target && target->isBlockLabel()) {
                BlockLabelValue* labelValue = static_cast<BlockLabelValue*>(target);
                uint32_t targetId = labelValue->targetInstructionId();
                if (reachable.find(targetId) == reachable.end()) {
                    reachable.insert(targetId);
                    workList.push(targetId);
                }
            }
            
            // 次の命令も到達可能
            uint32_t nextId = findNextInstructionId(function, currentId);
            if (nextId != 0 && reachable.find(nextId) == reachable.end()) {
                reachable.insert(nextId);
                workList.push(nextId);
            }
        } else if (instruction->opcode() != Opcode::Return && instruction->opcode() != Opcode::ReturnValue) {
            // Return以外の通常命令は次の命令に続く
            uint32_t nextId = findNextInstructionId(function, currentId);
            if (nextId != 0 && reachable.find(nextId) == reachable.end()) {
                reachable.insert(nextId);
                workList.push(nextId);
            }
        }
    }
    
    return reachable;
}

uint32_t DeadCodeEliminationPass::findNextInstructionId(const IRFunction* function, uint32_t currentId) const {
    bool foundCurrent = false;
    
    for (const auto& instruction : function->instructions()) {
        if (foundCurrent) {
            return instruction.id();
        }
        
        if (instruction.id() == currentId) {
            foundCurrent = true;
        }
    }
    
    return 0; // 次の命令が見つからない場合
}

std::unordered_set<uint32_t> DeadCodeEliminationPass::findLiveInstructions(
    const IRFunction* function, 
    const std::unordered_set<uint32_t>& reachableInstructions) const {
    
    std::unordered_set<uint32_t> liveInstructions;
    std::queue<uint32_t> workList;
    
    // 1. 初期の生きている命令を特定（副作用のある命令）
    for (const auto& instruction : function->instructions()) {
        if (reachableInstructions.find(instruction.id()) == reachableInstructions.end()) {
            continue; // 到達不可能な命令はスキップ
        }
        
        if (hasEffects(&instruction)) {
            liveInstructions.insert(instruction.id());
            workList.push(instruction.id());
        }
    }
    
    // 2. 生きている命令のオペランドも生きている
    while (!workList.empty()) {
        uint32_t currentId = workList.front();
        workList.pop();
        
        const IRInstruction* instruction = function->findInstruction(currentId);
        if (!instruction) {
            continue;
        }
        
        // この命令のオペランドを調査
        for (size_t i = 0; i < instruction->operandCount(); ++i) {
            Value* operand = instruction->operand(i);
            if (operand && operand->isInstruction()) {
                InstructionValue* instrValue = static_cast<InstructionValue*>(operand);
                uint32_t operandId = instrValue->instruction()->id();
                
                // 到達可能かつまだマークされていない場合
                if (reachableInstructions.find(operandId) != reachableInstructions.end() && 
                    liveInstructions.find(operandId) == liveInstructions.end()) {
                    liveInstructions.insert(operandId);
                    workList.push(operandId);
                }
            }
        }
    }
    
    return liveInstructions;
}

bool DeadCodeEliminationPass::hasEffects(const IRInstruction* instruction) const {
    if (!instruction) {
        return false;
    }
    
    // 副作用のある命令を特定
    switch (instruction->opcode()) {
        // メモリへの書き込み
        case Opcode::StoreVar:
        case Opcode::StoreGlobal:
        case Opcode::SetProperty:
            return true;
        
        // 制御フロー操作
        case Opcode::Return:
        case Opcode::ReturnValue:
        case Opcode::Jmp:
        case Opcode::JmpIf:
        case Opcode::JmpIfNot:
            return true;
        
        // 関数呼び出し（副作用を持つ可能性がある）
        case Opcode::Call:
            return true;
        
        // その他の副作用
        case Opcode::CreateObject:
        case Opcode::CreateArray:
        case Opcode::CreateFunction:
            return true;
        
        // 副作用のない命令
        case Opcode::Nop:
        case Opcode::LoadConst:
        case Opcode::LoadVar:
        case Opcode::LoadGlobal:
        case Opcode::Add:
        case Opcode::Sub:
        case Opcode::Mul:
        case Opcode::Div:
        case Opcode::Mod:
        case Opcode::Eq:
        case Opcode::Neq:
        case Opcode::Lt:
        case Opcode::Lte:
        case Opcode::Gt:
        case Opcode::Gte:
        case Opcode::GetProperty:
        case Opcode::Not:
        case Opcode::LogicalAnd:
        case Opcode::LogicalOr:
            return false;
        
        default:
            // 不明な命令は安全のため副作用ありとみなす
            return true;
    }
}

} // namespace aerojs::core 