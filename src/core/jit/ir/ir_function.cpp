/**
 * @file ir_function.cpp
 * @brief IR関数実装
 * 
 * このファイルは、中間表現（IR）の関数と基本ブロックの実装を提供します。
 * 
 * @author AeroJS Team
 * @version 1.0.0
 * @copyright MIT License
 */

#include "ir_function.h"
#include <sstream>
#include <algorithm>
#include <stack>
#include <queue>

namespace aerojs {
namespace core {
namespace ir {

// 静的メンバ初期化
size_t IRBasicBlock::nextId_ = 1;
size_t IRFunction::nextId_ = 1;

// IRBasicBlock実装

void IRBasicBlock::AddInstruction(std::unique_ptr<IRInstruction> instruction) {
    instruction->SetParent(this);
    instructions_.push_back(std::move(instruction));
}

void IRBasicBlock::InsertInstruction(size_t index, std::unique_ptr<IRInstruction> instruction) {
    if (index >= instructions_.size()) {
        AddInstruction(std::move(instruction));
        return;
    }
    
    instruction->SetParent(this);
    instructions_.insert(instructions_.begin() + index, std::move(instruction));
}

void IRBasicBlock::RemoveInstruction(size_t index) {
    if (index < instructions_.size()) {
        instructions_.erase(instructions_.begin() + index);
    }
}

void IRBasicBlock::RemoveInstruction(IRInstruction* instruction) {
    auto it = std::find_if(instructions_.begin(), instructions_.end(),
        [instruction](const std::unique_ptr<IRInstruction>& instr) {
            return instr.get() == instruction;
        });
    
    if (it != instructions_.end()) {
        instructions_.erase(it);
    }
}

bool IRBasicBlock::HasTerminator() const {
    return !instructions_.empty() && instructions_.back()->IsTerminator();
}

IRInstruction* IRBasicBlock::GetTerminator() const {
    if (!instructions_.empty() && instructions_.back()->IsTerminator()) {
        return instructions_.back().get();
    }
    return nullptr;
}

std::string IRBasicBlock::ToString() const {
    std::ostringstream oss;
    
    oss << "bb" << id_;
    if (!name_.empty()) {
        oss << " (" << name_ << ")";
    }
    oss << ":\n";
    
    for (const auto& instruction : instructions_) {
        oss << "  " << instruction->ToString() << "\n";
    }
    
    return oss.str();
}

std::unique_ptr<IRBasicBlock> IRBasicBlock::Clone() const {
    auto clone = std::make_unique<IRBasicBlock>(name_);
    
    for (const auto& instruction : instructions_) {
        clone->AddInstruction(instruction->Clone());
    }
    
    clone->isLoopHeader_ = isLoopHeader_;
    clone->loopDepth_ = loopDepth_;
    
    return clone;
}

// IRFunctionType実装

std::string IRFunctionType::ToString() const {
    std::ostringstream oss;
    oss << IRTypeToString(returnType) << "(";
    
    for (size_t i = 0; i < parameterTypes.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << IRTypeToString(parameterTypes[i]);
    }
    
    if (isVariadic) {
        if (!parameterTypes.empty()) oss << ", ";
        oss << "...";
    }
    
    oss << ")";
    return oss.str();
}

// IRFunction実装

IRBasicBlock* IRFunction::CreateBasicBlock(const std::string& name) {
    auto block = std::make_unique<IRBasicBlock>(name.empty() ? "bb" + std::to_string(basicBlocks_.size()) : name);
    block->SetParent(this);
    
    auto* ptr = block.get();
    basicBlocks_.push_back(std::move(block));
    return ptr;
}

void IRFunction::AddBasicBlock(std::unique_ptr<IRBasicBlock> block) {
    block->SetParent(this);
    basicBlocks_.push_back(std::move(block));
}

void IRFunction::RemoveBasicBlock(IRBasicBlock* block) {
    auto it = std::find_if(basicBlocks_.begin(), basicBlocks_.end(),
        [block](const std::unique_ptr<IRBasicBlock>& bb) {
            return bb.get() == block;
        });
    
    if (it != basicBlocks_.end()) {
        basicBlocks_.erase(it);
    }
}

void IRFunction::RemoveBasicBlock(size_t index) {
    if (index < basicBlocks_.size()) {
        basicBlocks_.erase(basicBlocks_.begin() + index);
    }
}

IRBasicBlock* IRFunction::GetBasicBlock(const std::string& name) const {
    for (const auto& block : basicBlocks_) {
        if (block->GetName() == name) {
            return block.get();
        }
    }
    return nullptr;
}

void IRFunction::BuildControlFlowGraph() {
    if (cfgBuilt_) return;
    
    // 既存の接続をクリア
    for (auto& block : basicBlocks_) {
        block->predecessors_.clear();
        block->successors_.clear();
    }
    
    UpdateBlockConnections();
    cfgBuilt_ = true;
}

void IRFunction::ComputeDominators() {
    if (dominatorsComputed_) return;
    
    BuildControlFlowGraph();
    ComputeDominatorsHelper();
    dominatorsComputed_ = true;
}

void IRFunction::DetectLoops() {
    if (loopsDetected_) return;
    
    ComputeDominators();
    DetectLoopsHelper();
    loopsDetected_ = true;
}

bool IRFunction::Verify() const {
    auto errors = GetVerificationErrors();
    return errors.empty();
}

std::vector<std::string> IRFunction::GetVerificationErrors() const {
    std::vector<std::string> errors;
    
    // エントリーブロックの存在確認
    if (basicBlocks_.empty()) {
        errors.push_back("関数にエントリーブロックがありません");
        return errors;
    }
    
    // 各基本ブロックの検証
    for (const auto& block : basicBlocks_) {
        // 空のブロックチェック
        if (block->IsEmpty()) {
            errors.push_back("空の基本ブロックがあります: " + block->GetName());
            continue;
        }
        
        // ターミネータの存在確認
        if (!block->HasTerminator()) {
            errors.push_back("基本ブロックにターミネータがありません: " + block->GetName());
        }
        
        // PHI命令の配置確認
        bool phiRegion = true;
        for (size_t i = 0; i < block->GetInstructionCount(); ++i) {
            auto* instruction = block->GetInstruction(i);
            
            if (instruction->GetOpcode() == IROpcode::PHI) {
                if (!phiRegion) {
                    errors.push_back("PHI命令がブロックの先頭にありません: " + block->GetName());
                }
            } else {
                phiRegion = false;
            }
        }
    }
    
    return errors;
}

size_t IRFunction::GetInstructionCount() const {
    size_t count = 0;
    for (const auto& block : basicBlocks_) {
        count += block->GetInstructionCount();
    }
    return count;
}

size_t IRFunction::GetVariableCount() const {
    // 簡略化実装：引数数を返す
    return arguments_.size();
}

size_t IRFunction::GetMaxLoopDepth() const {
    size_t maxDepth = 0;
    for (const auto& block : basicBlocks_) {
        maxDepth = std::max(maxDepth, block->GetLoopDepth());
    }
    return maxDepth;
}

std::string IRFunction::ToString() const {
    std::ostringstream oss;
    
    oss << "define " << type_.ToString() << " @" << name_ << "(";
    
    for (size_t i = 0; i < arguments_.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << IRTypeToString(arguments_[i]->GetType()) << " %" << arguments_[i]->GetName();
    }
    
    oss << ") {\n";
    
    for (const auto& block : basicBlocks_) {
        oss << block->ToString();
    }
    
    oss << "}\n";
    return oss.str();
}

std::unique_ptr<IRFunction> IRFunction::Clone() const {
    auto clone = std::make_unique<IRFunction>(name_, type_);
    
    // 基本ブロックのクローン
    for (const auto& block : basicBlocks_) {
        clone->AddBasicBlock(block->Clone());
    }
    
    clone->attributes_ = attributes_;
    clone->metadata_ = metadata_;
    
    return clone;
}

// プライベートヘルパーメソッド

void IRFunction::UpdateBlockConnections() {
    for (auto& block : basicBlocks_) {
        auto* terminator = block->GetTerminator();
        if (!terminator) continue;
        
        if (auto* branchInstr = dynamic_cast<IRBranchInstruction*>(terminator)) {
            if (branchInstr->IsConditional()) {
                // 条件分岐
                auto* trueTarget = branchInstr->GetTarget();
                auto* falseTarget = branchInstr->GetFalseTarget();
                
                if (trueTarget) {
                    block->AddSuccessor(trueTarget);
                    trueTarget->AddPredecessor(block.get());
                }
                
                if (falseTarget) {
                    block->AddSuccessor(falseTarget);
                    falseTarget->AddPredecessor(block.get());
                }
            } else {
                // 無条件分岐
                auto* target = branchInstr->GetTarget();
                if (target) {
                    block->AddSuccessor(target);
                    target->AddPredecessor(block.get());
                }
            }
        }
    }
}

void IRFunction::ComputeDominatorsHelper() {
    if (basicBlocks_.empty()) return;
    
    // 簡略化された支配関係計算
    auto* entry = basicBlocks_[0].get();
    entry->SetImmediateDominator(nullptr);
    
    for (size_t i = 1; i < basicBlocks_.size(); ++i) {
        auto* block = basicBlocks_[i].get();
        
        // 最初の前任者を即座支配者として設定（簡略化）
        if (!block->GetPredecessors().empty()) {
            auto* firstPred = *block->GetPredecessors().begin();
            block->SetImmediateDominator(firstPred);
            firstPred->AddDominatedBlock(block);
        }
    }
}

void IRFunction::DetectLoopsHelper() {
    // 簡略化されたループ検出
    for (auto& block : basicBlocks_) {
        block->SetLoopHeader(false);
        block->SetLoopDepth(0);
    }
    
    // バックエッジの検出
    for (auto& block : basicBlocks_) {
        for (auto* successor : block->GetSuccessors()) {
            // 支配関係を使ってバックエッジを判定
            if (successor->GetImmediateDominator() == block.get() ||
                successor == block.get()) {
                successor->SetLoopHeader(true);
                
                // ループ深度の計算（簡略化）
                size_t depth = 1;
                auto* current = block.get();
                while (current && current->IsLoopHeader()) {
                    depth++;
                    current = current->GetImmediateDominator();
                }
                
                block->SetLoopDepth(depth);
            }
        }
    }
}

// IRModule実装

IRFunction* IRModule::CreateFunction(const std::string& name, const IRFunctionType& type) {
    auto function = std::make_unique<IRFunction>(name, type);
    function->SetParent(this);
    
    auto* ptr = function.get();
    functions_.push_back(std::move(function));
    return ptr;
}

void IRModule::AddFunction(std::unique_ptr<IRFunction> function) {
    function->SetParent(this);
    functions_.push_back(std::move(function));
}

void IRModule::RemoveFunction(const std::string& name) {
    auto it = std::find_if(functions_.begin(), functions_.end(),
        [&name](const std::unique_ptr<IRFunction>& func) {
            return func->GetName() == name;
        });
    
    if (it != functions_.end()) {
        functions_.erase(it);
    }
}

void IRModule::RemoveFunction(IRFunction* function) {
    auto it = std::find_if(functions_.begin(), functions_.end(),
        [function](const std::unique_ptr<IRFunction>& func) {
            return func.get() == function;
        });
    
    if (it != functions_.end()) {
        functions_.erase(it);
    }
}

IRFunction* IRModule::GetFunction(const std::string& name) const {
    for (const auto& function : functions_) {
        if (function->GetName() == name) {
            return function.get();
        }
    }
    return nullptr;
}

bool IRModule::Verify() const {
    auto errors = GetVerificationErrors();
    return errors.empty();
}

std::vector<std::string> IRModule::GetVerificationErrors() const {
    std::vector<std::string> errors;
    
    // 関数名の重複チェック
    std::unordered_map<std::string, size_t> functionNames;
    for (const auto& function : functions_) {
        const std::string& name = function->GetName();
        if (functionNames.find(name) != functionNames.end()) {
            errors.push_back("重複する関数名: " + name);
        }
        functionNames[name]++;
    }
    
    // 各関数の検証
    for (const auto& function : functions_) {
        auto functionErrors = function->GetVerificationErrors();
        for (const auto& error : functionErrors) {
            errors.push_back("関数 " + function->GetName() + ": " + error);
        }
    }
    
    return errors;
}

std::string IRModule::ToString() const {
    std::ostringstream oss;
    
    oss << "; Module: " << name_ << "\n\n";
    
    // グローバル変数
    if (!globals_.empty()) {
        oss << "; Global variables\n";
        for (const auto& global : globals_) {
            oss << "@" << global.first << " = global " << IRTypeToString(global.second) << "\n";
        }
        oss << "\n";
    }
    
    // 関数
    for (const auto& function : functions_) {
        oss << function->ToString() << "\n";
    }
    
    return oss.str();
}

std::unique_ptr<IRModule> IRModule::Clone() const {
    auto clone = std::make_unique<IRModule>(name_);
    
    for (const auto& function : functions_) {
        clone->AddFunction(function->Clone());
    }
    
    clone->globals_ = globals_;
    clone->metadata_ = metadata_;
    
    return clone;
}

// ヘルパー関数実装

std::vector<IRBasicBlock*> ComputePostOrder(IRFunction* function) {
    std::vector<IRBasicBlock*> postOrder;
    std::unordered_set<IRBasicBlock*> visited;
    
    if (function->GetBasicBlockCount() == 0) return postOrder;
    
    std::function<void(IRBasicBlock*)> dfs = [&](IRBasicBlock* block) {
        if (visited.find(block) != visited.end()) return;
        visited.insert(block);
        
        for (auto* successor : block->GetSuccessors()) {
            dfs(successor);
        }
        
        postOrder.push_back(block);
    };
    
    dfs(function->GetEntryBlock());
    return postOrder;
}

std::vector<IRBasicBlock*> ComputeReversePostOrder(IRFunction* function) {
    auto postOrder = ComputePostOrder(function);
    std::reverse(postOrder.begin(), postOrder.end());
    return postOrder;
}

bool IsReachable(IRBasicBlock* from, IRBasicBlock* to) {
    if (from == to) return true;
    
    std::unordered_set<IRBasicBlock*> visited;
    std::queue<IRBasicBlock*> queue;
    
    queue.push(from);
    visited.insert(from);
    
    while (!queue.empty()) {
        auto* current = queue.front();
        queue.pop();
        
        for (auto* successor : current->GetSuccessors()) {
            if (successor == to) return true;
            
            if (visited.find(successor) == visited.end()) {
                visited.insert(successor);
                queue.push(successor);
            }
        }
    }
    
    return false;
}

std::unordered_map<IRBasicBlock*, IRBasicBlock*> ComputeImmediateDominators(IRFunction* function) {
    std::unordered_map<IRBasicBlock*, IRBasicBlock*> idom;
    
    if (function->GetBasicBlockCount() == 0) return idom;
    
    // 簡略化実装：エントリーブロックの即座支配者はnull
    auto* entry = function->GetEntryBlock();
    idom[entry] = nullptr;
    
    // その他のブロックは前任者の一つを即座支配者とする
    for (size_t i = 1; i < function->GetBasicBlockCount(); ++i) {
        auto* block = function->GetBasicBlock(i);
        if (!block->GetPredecessors().empty()) {
            idom[block] = *block->GetPredecessors().begin();
        }
    }
    
    return idom;
}

std::unordered_map<IRBasicBlock*, std::unordered_set<IRBasicBlock*>> ComputeDominanceFrontier(IRFunction* function) {
    std::unordered_map<IRBasicBlock*, std::unordered_set<IRBasicBlock*>> df;
    
    // 簡略化実装：空の支配境界を返す
    for (size_t i = 0; i < function->GetBasicBlockCount(); ++i) {
        auto* block = function->GetBasicBlock(i);
        df[block] = std::unordered_set<IRBasicBlock*>();
    }
    
    return df;
}

std::vector<std::unique_ptr<LoopInfo>> DetectNaturalLoops(IRFunction* function) {
    std::vector<std::unique_ptr<LoopInfo>> loops;
    
    // 簡略化実装：基本的なループ検出
    for (size_t i = 0; i < function->GetBasicBlockCount(); ++i) {
        auto* block = function->GetBasicBlock(i);
        
        // 自己ループの検出
        for (auto* successor : block->GetSuccessors()) {
            if (successor == block) {
                auto loop = std::make_unique<LoopInfo>(block);
                loop->blocks.insert(block);
                loop->depth = 1;
                loops.push_back(std::move(loop));
            }
        }
    }
    
    return loops;
}

// 関数属性操作

IRFunctionAttribute operator|(IRFunctionAttribute a, IRFunctionAttribute b) {
    return static_cast<IRFunctionAttribute>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

IRFunctionAttribute operator&(IRFunctionAttribute a, IRFunctionAttribute b) {
    return static_cast<IRFunctionAttribute>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

IRFunctionAttribute operator~(IRFunctionAttribute a) {
    return static_cast<IRFunctionAttribute>(~static_cast<uint32_t>(a));
}

} // namespace ir
} // namespace core
} // namespace aerojs 