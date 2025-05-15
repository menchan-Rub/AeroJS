#include "ir_optimizer.h"
#include <unordered_map>
#include <vector>
#include <cassert>
#include <cmath>
#include <limits>

namespace aerojs {
namespace core {

/**
 * @brief 定数畳み込み最適化を実行する
 * 
 * IRFunction内の定数式を検出し、コンパイル時に計算することで
 * 実行時の計算を削減します。例えば:
 * 
 * LoadConst 5    ->    LoadConst 15
 * LoadConst 10
 * Add
 * 
 * @param function 最適化対象のIR関数
 * @return 最適化が行われた場合はtrue
 */
bool ConstantFoldingPass::run(IRFunction* function) {
    assert(function && "IRFunction cannot be null");
    
    bool changed = false;
    std::vector<IRInstruction> optimizedInstructions;
    const auto& instructions = function->GetInstructions();
    
    // 定数値レジスタを追跡するマップ
    std::unordered_map<int32_t, int32_t> constantRegisters;
    // 命令の値がある場合のみのレジスタ番号を追跡するマップ
    std::unordered_map<int32_t, int32_t> lastDefinitionMap;
    
    // 命令を走査して最適化を適用
    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto& inst = instructions[i];
        
        // 定数ロード命令の追跡
        if (inst.opcode == Opcode::kLoadConst && inst.args.size() >= 2) {
            int32_t destReg = inst.args[0];    // 宛先レジスタ
            int32_t constValue = inst.args[1]; // 定数値
            
            // 定数マップを更新
            constantRegisters[destReg] = constValue;
            lastDefinitionMap[destReg] = i;
            
            // この命令はそのまま保持
            optimizedInstructions.push_back(inst);
            continue;
        }
        
        // 算術演算の定数畳み込み
        if ((inst.opcode >= Opcode::kAdd && inst.opcode <= Opcode::kDiv) && 
            inst.args.size() >= 3) {
            
            int32_t destReg = inst.args[0];    // 結果レジスタ
            int32_t leftReg = inst.args[1];    // 左オペランドレジスタ
            int32_t rightReg = inst.args[2];   // 右オペランドレジスタ
            
            // 両方のオペランドが定数かチェック
            auto leftIt = constantRegisters.find(leftReg);
            auto rightIt = constantRegisters.find(rightReg);
            
            if (leftIt != constantRegisters.end() && rightIt != constantRegisters.end()) {
                int32_t leftVal = leftIt->second;
                int32_t rightVal = rightIt->second;
                int32_t result = 0;
                bool validOperation = true;
                
                // 演算を実行
                switch (inst.opcode) {
                    case Opcode::kAdd:
                        result = leftVal + rightVal;
                        break;
                    case Opcode::kSub:
                        result = leftVal - rightVal;
                        break;
                    case Opcode::kMul:
                        result = leftVal * rightVal;
                        break;
                    case Opcode::kDiv:
                        if (rightVal != 0) {
                            result = leftVal / rightVal;
                        } else {
                            // 除算でゼロ除算は回避
                            validOperation = false;
                        }
                        break;
                    case Opcode::kMod:
                        if (rightVal != 0) {
                            result = leftVal % rightVal;
                        } else {
                            validOperation = false;
                        }
                        break;
                    default:
                        validOperation = false;
                        break;
                }
                
                if (validOperation) {
                    // 元の演算命令を定数ロードに置き換え
                    IRInstruction newInst;
                    newInst.opcode = Opcode::kLoadConst;
                    newInst.args = {destReg, result};
                    if (!inst.metadata.empty()) {
                        newInst.metadata = inst.metadata + " [folded]";
                    } else {
                        newInst.metadata = "[folded]";
                    }
                    
                    optimizedInstructions.push_back(newInst);
                    
                    // 定数マップを更新
                    constantRegisters[destReg] = result;
                    lastDefinitionMap[destReg] = i;
                    
                    changed = true;
                    continue;
                }
            }
        }
        
        // 単項演算の定数畳み込み（符号反転など）
        if (inst.opcode == Opcode::kNeg && inst.args.size() >= 2) {
            int32_t destReg = inst.args[0];
            int32_t srcReg = inst.args[1];
            
            auto srcIt = constantRegisters.find(srcReg);
            if (srcIt != constantRegisters.end()) {
                int32_t srcVal = srcIt->second;
                int32_t result = -srcVal;
                
                // 符号反転命令を定数ロードに置き換え
                IRInstruction newInst;
                newInst.opcode = Opcode::kLoadConst;
                newInst.args = {destReg, result};
                if (!inst.metadata.empty()) {
                    newInst.metadata = inst.metadata + " [folded]";
                } else {
                    newInst.metadata = "[folded]";
                }
                
                optimizedInstructions.push_back(newInst);
                
                // 定数マップを更新
                constantRegisters[destReg] = result;
                lastDefinitionMap[destReg] = i;
                
                changed = true;
                continue;
            }
        }
        
        // 比較命令の定数畳み込み
        if (inst.opcode >= Opcode::kCompareEq && inst.opcode <= Opcode::kCompareGe && 
            inst.args.size() >= 3) {
            
            int32_t destReg = inst.args[0];
            int32_t leftReg = inst.args[1];
            int32_t rightReg = inst.args[2];
            
            auto leftIt = constantRegisters.find(leftReg);
            auto rightIt = constantRegisters.find(rightReg);
            
            if (leftIt != constantRegisters.end() && rightIt != constantRegisters.end()) {
                int32_t leftVal = leftIt->second;
                int32_t rightVal = rightIt->second;
                int32_t result = 0;
                
                // 比較演算を実行
                switch (inst.opcode) {
                    case Opcode::kCompareEq:
                        result = (leftVal == rightVal) ? 1 : 0;
                        break;
                    case Opcode::kCompareNe:
                        result = (leftVal != rightVal) ? 1 : 0;
                        break;
                    case Opcode::kCompareLt:
                        result = (leftVal < rightVal) ? 1 : 0;
                        break;
                    case Opcode::kCompareLe:
                        result = (leftVal <= rightVal) ? 1 : 0;
                        break;
                    case Opcode::kCompareGt:
                        result = (leftVal > rightVal) ? 1 : 0;
                        break;
                    case Opcode::kCompareGe:
                        result = (leftVal >= rightVal) ? 1 : 0;
                        break;
                    default:
                        // 上記の条件分岐で全てカバーされるはずなので、
                        // このパスには来ない
                        break;
                }
                
                // 比較命令を定数ロードに置き換え
                IRInstruction newInst;
                newInst.opcode = Opcode::kLoadConst;
                newInst.args = {destReg, result};
                if (!inst.metadata.empty()) {
                    newInst.metadata = inst.metadata + " [folded]";
                } else {
                    newInst.metadata = "[folded]";
                }
                
                optimizedInstructions.push_back(newInst);
                
                // 定数マップを更新
                constantRegisters[destReg] = result;
                lastDefinitionMap[destReg] = i;
                
                changed = true;
                continue;
            }
        }
        
        // 論理演算の定数畳み込み
        if ((inst.opcode >= Opcode::kAnd && inst.opcode <= Opcode::kNot) && 
            inst.args.size() >= (inst.opcode == Opcode::kNot ? 2 : 3)) {
            
            if (inst.opcode == Opcode::kNot) {
                int32_t destReg = inst.args[0];
                int32_t srcReg = inst.args[1];
                
                auto srcIt = constantRegisters.find(srcReg);
                if (srcIt != constantRegisters.end()) {
                    int32_t srcVal = srcIt->second;
                    int32_t result = srcVal == 0 ? 1 : 0;
                    
                    // NOT命令を定数ロードに置き換え
                    IRInstruction newInst;
                    newInst.opcode = Opcode::kLoadConst;
                    newInst.args = {destReg, result};
                    if (!inst.metadata.empty()) {
                        newInst.metadata = inst.metadata + " [folded]";
                    } else {
                        newInst.metadata = "[folded]";
                    }
                    
                    optimizedInstructions.push_back(newInst);
                    
                    // 定数マップを更新
                    constantRegisters[destReg] = result;
                    lastDefinitionMap[destReg] = i;
                    
                    changed = true;
                    continue;
                }
            } else {
                int32_t destReg = inst.args[0];
                int32_t leftReg = inst.args[1];
                int32_t rightReg = inst.args[2];
                
                auto leftIt = constantRegisters.find(leftReg);
                auto rightIt = constantRegisters.find(rightReg);
                
                if (leftIt != constantRegisters.end() && rightIt != constantRegisters.end()) {
                    int32_t leftVal = leftIt->second;
                    int32_t rightVal = rightIt->second;
                    int32_t result = 0;
                    
                    // 論理演算を実行
                    switch (inst.opcode) {
                        case Opcode::kAnd:
                            result = (leftVal != 0 && rightVal != 0) ? 1 : 0;
                            break;
                        case Opcode::kOr:
                            result = (leftVal != 0 || rightVal != 0) ? 1 : 0;
                            break;
                        default:
                            // 上記の条件分岐で全てカバーされるはずなので、
                            // このパスには来ない
                            break;
                    }
                    
                    // 論理演算命令を定数ロードに置き換え
                    IRInstruction newInst;
                    newInst.opcode = Opcode::kLoadConst;
                    newInst.args = {destReg, result};
                    if (!inst.metadata.empty()) {
                        newInst.metadata = inst.metadata + " [folded]";
                    } else {
                        newInst.metadata = "[folded]";
                    }
                    
                    optimizedInstructions.push_back(newInst);
                    
                    // 定数マップを更新
                    constantRegisters[destReg] = result;
                    lastDefinitionMap[destReg] = i;
                    
                    changed = true;
                    continue;
                }
            }
        }
        
        // ビット演算の定数畳み込み
        if ((inst.opcode >= Opcode::kBitAnd && inst.opcode <= Opcode::kShiftRight) && 
            inst.args.size() >= (inst.opcode == Opcode::kBitNot ? 2 : 3)) {
            
            if (inst.opcode == Opcode::kBitNot) {
                int32_t destReg = inst.args[0];
                int32_t srcReg = inst.args[1];
                
                auto srcIt = constantRegisters.find(srcReg);
                if (srcIt != constantRegisters.end()) {
                    int32_t srcVal = srcIt->second;
                    int32_t result = ~srcVal;
                    
                    // BitNot命令を定数ロードに置き換え
                    IRInstruction newInst;
                    newInst.opcode = Opcode::kLoadConst;
                    newInst.args = {destReg, result};
                    if (!inst.metadata.empty()) {
                        newInst.metadata = inst.metadata + " [folded]";
                    } else {
                        newInst.metadata = "[folded]";
                    }
                    
                    optimizedInstructions.push_back(newInst);
                    
                    // 定数マップを更新
                    constantRegisters[destReg] = result;
                    lastDefinitionMap[destReg] = i;
                    
                    changed = true;
                    continue;
                }
            } else {
                int32_t destReg = inst.args[0];
                int32_t leftReg = inst.args[1];
                int32_t rightReg = inst.args[2];
                
                auto leftIt = constantRegisters.find(leftReg);
                auto rightIt = constantRegisters.find(rightReg);
                
                if (leftIt != constantRegisters.end() && rightIt != constantRegisters.end()) {
                    int32_t leftVal = leftIt->second;
                    int32_t rightVal = rightIt->second;
                    int32_t result = 0;
                    bool validOperation = true;
                    
                    // ビット演算を実行
                    switch (inst.opcode) {
                        case Opcode::kBitAnd:
                            result = leftVal & rightVal;
                            break;
                        case Opcode::kBitOr:
                            result = leftVal | rightVal;
                            break;
                        case Opcode::kBitXor:
                            result = leftVal ^ rightVal;
                            break;
                        case Opcode::kShiftLeft:
                            // シフト値が負または大きすぎる場合は最適化しない
                            if (rightVal >= 0 && rightVal < 32) {
                                result = leftVal << rightVal;
                            } else {
                                validOperation = false;
                            }
                            break;
                        case Opcode::kShiftRight:
                            // シフト値が負または大きすぎる場合は最適化しない
                            if (rightVal >= 0 && rightVal < 32) {
                                result = leftVal >> rightVal;
                            } else {
                                validOperation = false;
                            }
                            break;
                        default:
                            validOperation = false;
                            break;
                    }
                    
                    if (validOperation) {
                        // ビット演算命令を定数ロードに置き換え
                        IRInstruction newInst;
                        newInst.opcode = Opcode::kLoadConst;
                        newInst.args = {destReg, result};
                        if (!inst.metadata.empty()) {
                            newInst.metadata = inst.metadata + " [folded]";
                        } else {
                            newInst.metadata = "[folded]";
                        }
                        
                        optimizedInstructions.push_back(newInst);
                        
                        // 定数マップを更新
                        constantRegisters[destReg] = result;
                        lastDefinitionMap[destReg] = i;
                        
                        changed = true;
                        continue;
                    }
                }
            }
        }
        
        // 定数伝播：Move命令の最適化
        if (inst.opcode == Opcode::kMove && inst.args.size() >= 2) {
            int32_t destReg = inst.args[0];
            int32_t srcReg = inst.args[1];
            
            auto srcIt = constantRegisters.find(srcReg);
            if (srcIt != constantRegisters.end()) {
                int32_t constValue = srcIt->second;
                
                // Move命令を定数ロードに置き換え
                IRInstruction newInst;
                newInst.opcode = Opcode::kLoadConst;
                newInst.args = {destReg, constValue};
                if (!inst.metadata.empty()) {
                    newInst.metadata = inst.metadata + " [propagated]";
                } else {
                    newInst.metadata = "[propagated]";
                }
                
                optimizedInstructions.push_back(newInst);
                
                // 定数マップを更新
                constantRegisters[destReg] = constValue;
                lastDefinitionMap[destReg] = i;
                
                changed = true;
                continue;
            }
        }
        
        // レジスタの再定義があった場合は定数マップから削除
        if (inst.args.size() >= 1) {
            int32_t destReg = inst.args[0];
            constantRegisters.erase(destReg);
        }
        
        // その他の命令はそのまま追加
        optimizedInstructions.push_back(inst);
    }
    
    // 最適化が行われた場合、命令リストを更新
    if (changed) {
        function->Clear();
        for (const auto& inst : optimizedInstructions) {
            function->AddInstruction(inst);
        }
    }
    
    return changed;
}

// 定数畳み込みパスのコンストラクタとデストラクタ
ConstantFoldingPass::ConstantFoldingPass() {}
ConstantFoldingPass::~ConstantFoldingPass() {}

}  // namespace core
}  // namespace aerojs 