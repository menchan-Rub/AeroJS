#include "src/core/jit/ir/ir_optimizer.h"

#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <vector>

namespace aerojs::core {

TypeSpecializationPass::TypeSpecializationPass() = default;
TypeSpecializationPass::~TypeSpecializationPass() = default;

bool TypeSpecializationPass::run(IRFunction* function) {
    if (!function) {
        return false;
    }
    
    bool changed = false;
    
    // 1. 型分析を実行
    m_typeAnalyzer.analyze(function);
    
    // 2. 型に基づいて最適化
    
    // 算術演算の特化
    changed |= specializeArithmeticOperations(function);
    
    // 比較演算の特化
    changed |= specializeComparisonOperations(function);
    
    // 型チェックの特化
    changed |= specializeTypeChecks(function);
    
    // 型変換の特化
    changed |= specializeTypeConversions(function);
    
    // リセット
    m_typeAnalyzer.reset();
    
    return changed;
}

bool TypeSpecializationPass::specializeArithmeticOperations(IRFunction* function) {
    bool changed = false;
    std::vector<IRInstruction*> instructionsToReplace;
    std::vector<std::pair<IRInstruction*, Opcode>> replacements;
    
    // 算術演算命令を特定
    for (auto& instruction : function->instructions()) {
        switch (instruction.opcode()) {
            case Opcode::Add:
            case Opcode::Sub:
            case Opcode::Mul:
            case Opcode::Div:
            case Opcode::Mod:
                if (instruction.operandCount() >= 2) {
                    TypeInfo leftType = m_typeAnalyzer.getValueType(instruction.operand(0));
                    TypeInfo rightType = m_typeAnalyzer.getValueType(instruction.operand(1));
                    
                    // 両方のオペランドが整数型の場合、整数専用の命令に置き換え
                    if (leftType.isInt32() && rightType.isInt32()) {
                        Opcode specializedOpcode = getSpecializedIntegerOpcode(instruction.opcode());
                        if (specializedOpcode != instruction.opcode()) {
                            instructionsToReplace.push_back(&instruction);
                            replacements.emplace_back(&instruction, specializedOpcode);
                            changed = true;
                        }
                    }
                    // 浮動小数点演算の特化
                    else if (leftType.isFloat64() || rightType.isFloat64()) {
                        Opcode specializedOpcode = getSpecializedFloatOpcode(instruction.opcode());
                        if (specializedOpcode != instruction.opcode()) {
                            instructionsToReplace.push_back(&instruction);
                            replacements.emplace_back(&instruction, specializedOpcode);
                            changed = true;
                        }
                    }
                    // 文字列連結の特化（加算のみ）
                    else if (instruction.opcode() == Opcode::Add && 
                             (leftType.isString() || rightType.isString())) {
                        instructionsToReplace.push_back(&instruction);
                        replacements.emplace_back(&instruction, Opcode::StringConcat);
                        changed = true;
                    }
                }
                break;
            default:
                break;
        }
    }
    
    // 命令を置き換え
    for (size_t i = 0; i < instructionsToReplace.size(); ++i) {
        IRInstruction* oldInst = instructionsToReplace[i];
        Opcode newOpcode = replacements[i].second;
        
        // 新しい命令を作成
        IRInstruction* newInst = function->createInstruction(newOpcode);
        
        // オペランドをコピー
        for (size_t j = 0; j < oldInst->operandCount(); ++j) {
            newInst->addOperand(oldInst->operand(j));
        }
        
        // 古い命令の使用箇所を新しい命令に置き換え
        std::vector<std::pair<IRInstruction*, size_t>> usages;
        function->findAllUsages(oldInst, usages);
        
        for (auto& usage : usages) {
            IRInstruction* user = usage.first;
            size_t operandIndex = usage.second;
            
            // 使用箇所のオペランドを新しい命令に置き換え
            InstructionValue* newValue = function->createInstructionValue(newInst);
            user->replaceOperand(operandIndex, newValue);
        }
        
        // 古い命令を削除
        function->removeInstruction(oldInst->id());
    }
    
    return changed;
}

bool TypeSpecializationPass::specializeComparisonOperations(IRFunction* function) {
    bool changed = false;
    std::vector<IRInstruction*> instructionsToReplace;
    std::vector<std::pair<IRInstruction*, Opcode>> replacements;
    
    // 比較演算命令を特定
    for (auto& instruction : function->instructions()) {
        switch (instruction.opcode()) {
            case Opcode::Eq:
            case Opcode::Neq:
            case Opcode::Lt:
            case Opcode::Lte:
            case Opcode::Gt:
            case Opcode::Gte:
                if (instruction.operandCount() >= 2) {
                    TypeInfo leftType = m_typeAnalyzer.getValueType(instruction.operand(0));
                    TypeInfo rightType = m_typeAnalyzer.getValueType(instruction.operand(1));
                    
                    // 整数比較の特化
                    if (leftType.isInt32() && rightType.isInt32()) {
                        Opcode specializedOpcode = getSpecializedIntegerCompareOpcode(instruction.opcode());
                        if (specializedOpcode != instruction.opcode()) {
                            instructionsToReplace.push_back(&instruction);
                            replacements.emplace_back(&instruction, specializedOpcode);
                            changed = true;
                        }
                    }
                    // 浮動小数点比較の特化
                    else if (leftType.isFloat64() || rightType.isFloat64()) {
                        Opcode specializedOpcode = getSpecializedFloatCompareOpcode(instruction.opcode());
                        if (specializedOpcode != instruction.opcode()) {
                            instructionsToReplace.push_back(&instruction);
                            replacements.emplace_back(&instruction, specializedOpcode);
                            changed = true;
                        }
                    }
                    // 文字列比較の特化
                    else if (leftType.isString() && rightType.isString()) {
                        Opcode specializedOpcode = getSpecializedStringCompareOpcode(instruction.opcode());
                        if (specializedOpcode != instruction.opcode()) {
                            instructionsToReplace.push_back(&instruction);
                            replacements.emplace_back(&instruction, specializedOpcode);
                            changed = true;
                        }
                    }
                }
                break;
            default:
                break;
        }
    }
    
    // 命令を置き換え
    for (size_t i = 0; i < instructionsToReplace.size(); ++i) {
        IRInstruction* oldInst = instructionsToReplace[i];
        Opcode newOpcode = replacements[i].second;
        
        // 新しい命令を作成
        IRInstruction* newInst = function->createInstruction(newOpcode);
        
        // オペランドをコピー
        for (size_t j = 0; j < oldInst->operandCount(); ++j) {
            newInst->addOperand(oldInst->operand(j));
        }
        
        // 古い命令の使用箇所を新しい命令に置き換え
        std::vector<std::pair<IRInstruction*, size_t>> usages;
        function->findAllUsages(oldInst, usages);
        
        for (auto& usage : usages) {
            IRInstruction* user = usage.first;
            size_t operandIndex = usage.second;
            
            // 使用箇所のオペランドを新しい命令に置き換え
            InstructionValue* newValue = function->createInstructionValue(newInst);
            user->replaceOperand(operandIndex, newValue);
        }
        
        // 古い命令を削除
        function->removeInstruction(oldInst->id());
    }
    
    return changed;
}

bool TypeSpecializationPass::specializeTypeChecks(IRFunction* function) {
    bool changed = false;
    std::vector<IRInstruction*> instructionsToReplace;
    std::vector<std::pair<IRInstruction*, bool>> replacementConstants; // ブール定数に置き換え
    
    // 型チェック命令を特定
    for (auto& instruction : function->instructions()) {
        if (instruction.opcode() == Opcode::TypeOf && instruction.operandCount() >= 2) {
            Value* operand = instruction.operand(0);
            Value* typeNameValue = instruction.operand(1);
            
            if (operand && typeNameValue && typeNameValue->isConstant()) {
                ConstantValue* typeConst = static_cast<ConstantValue*>(typeNameValue);
                if (typeConst->isString()) {
                    std::string typeName = typeConst->asString();
                    TypeInfo operandType = m_typeAnalyzer.getValueType(operand);
                    
                    // 型が確定している場合、定数の結果に置き換え
                    if (!operandType.isUnknown()) {
                        bool result = checkTypeMatch(operandType, typeName);
                        instructionsToReplace.push_back(&instruction);
                        replacementConstants.emplace_back(&instruction, result);
                        changed = true;
                    }
                }
            }
        }
    }
    
    // 命令を定数に置き換え
    for (size_t i = 0; i < instructionsToReplace.size(); ++i) {
        IRInstruction* oldInst = instructionsToReplace[i];
        bool constResult = replacementConstants[i].second;
        
        // 定数ロード命令を作成
        IRInstruction* loadConst = function->createInstruction(Opcode::LoadConst);
        ConstantValue* constValue = new ConstantValue(constResult);
        loadConst->addOperand(constValue);
        
        // 古い命令の使用箇所を新しい命令に置き換え
        std::vector<std::pair<IRInstruction*, size_t>> usages;
        function->findAllUsages(oldInst, usages);
        
        for (auto& usage : usages) {
            IRInstruction* user = usage.first;
            size_t operandIndex = usage.second;
            
            // 使用箇所のオペランドを新しい命令に置き換え
            InstructionValue* newValue = function->createInstructionValue(loadConst);
            user->replaceOperand(operandIndex, newValue);
        }
        
        // 古い命令を削除
        function->removeInstruction(oldInst->id());
    }
    
    return changed;
}

bool TypeSpecializationPass::specializeTypeConversions(IRFunction* function) {
    bool changed = false;
    std::vector<IRInstruction*> instructionsToRemove; // 不要な型変換を削除
    
    // 型変換命令を特定
    for (auto& instruction : function->instructions()) {
        if ((instruction.opcode() == Opcode::ToNumber || 
             instruction.opcode() == Opcode::ToString || 
             instruction.opcode() == Opcode::ToBoolean) && 
            instruction.operandCount() >= 1) {
            
            Value* operand = instruction.operand(0);
            TypeInfo operandType = m_typeAnalyzer.getValueType(operand);
            
            // 既に目的の型である場合、変換は不要
            if ((instruction.opcode() == Opcode::ToNumber && operandType.isNumeric()) || 
                (instruction.opcode() == Opcode::ToString && operandType.isString()) || 
                (instruction.opcode() == Opcode::ToBoolean && operandType.isBoolean())) {
                
                // 使用箇所を直接オペランドに置き換え
                std::vector<std::pair<IRInstruction*, size_t>> usages;
                function->findAllUsages(&instruction, usages);
                
                for (auto& usage : usages) {
                    IRInstruction* user = usage.first;
                    size_t operandIndex = usage.second;
                    
                    // 使用箇所のオペランドを元のオペランドに置き換え
                    user->replaceOperand(operandIndex, operand);
                }
                
                instructionsToRemove.push_back(&instruction);
                changed = true;
            }
        }
    }
    
    // 不要な命令を削除
    for (IRInstruction* inst : instructionsToRemove) {
        function->removeInstruction(inst->id());
    }
    
    return changed;
}

Opcode TypeSpecializationPass::getSpecializedIntegerOpcode(Opcode genericOpcode) const {
    switch (genericOpcode) {
        case Opcode::Add: return Opcode::AddInt;
        case Opcode::Sub: return Opcode::SubInt;
        case Opcode::Mul: return Opcode::MulInt;
        case Opcode::Div: return Opcode::DivInt;
        case Opcode::Mod: return Opcode::ModInt;
        default: return genericOpcode;
    }
}

Opcode TypeSpecializationPass::getSpecializedFloatOpcode(Opcode genericOpcode) const {
    switch (genericOpcode) {
        case Opcode::Add: return Opcode::AddFloat;
        case Opcode::Sub: return Opcode::SubFloat;
        case Opcode::Mul: return Opcode::MulFloat;
        case Opcode::Div: return Opcode::DivFloat;
        case Opcode::Mod: return Opcode::ModFloat;
        default: return genericOpcode;
    }
}

Opcode TypeSpecializationPass::getSpecializedIntegerCompareOpcode(Opcode genericOpcode) const {
    switch (genericOpcode) {
        case Opcode::Eq: return Opcode::EqInt;
        case Opcode::Neq: return Opcode::NeqInt;
        case Opcode::Lt: return Opcode::LtInt;
        case Opcode::Lte: return Opcode::LteInt;
        case Opcode::Gt: return Opcode::GtInt;
        case Opcode::Gte: return Opcode::GteInt;
        default: return genericOpcode;
    }
}

Opcode TypeSpecializationPass::getSpecializedFloatCompareOpcode(Opcode genericOpcode) const {
    switch (genericOpcode) {
        case Opcode::Eq: return Opcode::EqFloat;
        case Opcode::Neq: return Opcode::NeqFloat;
        case Opcode::Lt: return Opcode::LtFloat;
        case Opcode::Lte: return Opcode::LteFloat;
        case Opcode::Gt: return Opcode::GtFloat;
        case Opcode::Gte: return Opcode::GteFloat;
        default: return genericOpcode;
    }
}

Opcode TypeSpecializationPass::getSpecializedStringCompareOpcode(Opcode genericOpcode) const {
    switch (genericOpcode) {
        case Opcode::Eq: return Opcode::EqString;
        case Opcode::Neq: return Opcode::NeqString;
        case Opcode::Lt: return Opcode::LtString;
        case Opcode::Lte: return Opcode::LteString;
        case Opcode::Gt: return Opcode::GtString;
        case Opcode::Gte: return Opcode::GteString;
        default: return genericOpcode;
    }
}

bool TypeSpecializationPass::checkTypeMatch(const TypeInfo& type, const std::string& typeName) const {
    if (typeName == "number") {
        return type.isNumeric();
    } else if (typeName == "string") {
        return type.isString();
    } else if (typeName == "boolean") {
        return type.isBoolean();
    } else if (typeName == "object") {
        return type.isObjectLike() || type.isNull(); // JavaScriptでは null は typeof で "object"
    } else if (typeName == "function") {
        return type.isFunction();
    } else if (typeName == "undefined") {
        return type.isUndefined();
    }
    
    return false;
}

} // namespace aerojs::core 