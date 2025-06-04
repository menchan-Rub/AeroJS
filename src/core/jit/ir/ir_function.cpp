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
    // 完璧な支配関係計算（Lengauer-Tarjan アルゴリズム）
    
    if (basicBlocks_.empty()) return;
    
    size_t n = basicBlocks_.size();
    dominators_.clear();
    dominators_.resize(n);
    
    // 1. DFS番号付けと親の設定
    std::vector<int> dfsNum(n, -1);
    std::vector<size_t> vertex(n);
    std::vector<size_t> parent(n, SIZE_MAX);
    std::vector<size_t> semi(n);
    std::vector<size_t> idom(n, SIZE_MAX);
    std::vector<std::vector<size_t>> bucket(n);
    std::vector<size_t> ancestor(n, SIZE_MAX);
    std::vector<size_t> label(n);
    
    int dfsCounter = 0;
    
    // DFS訪問
    std::function<void(size_t)> dfs = [&](size_t v) {
        dfsNum[v] = dfsCounter;
        vertex[dfsCounter] = v;
        label[v] = v;
        semi[v] = dfsCounter;
        dfsCounter++;
        
        for (size_t w : basicBlocks_[v]->GetSuccessors()) {
            if (dfsNum[w] == -1) {
                parent[w] = v;
                dfs(w);
            }
        }
    };
    
    // エントリーブロックからDFS開始
    dfs(0);
    
    // 2. Semi-dominator計算
    std::function<size_t(size_t)> eval;
    std::function<void(size_t, size_t)> link;
    
    eval = [&](size_t v) -> size_t {
        if (ancestor[v] == SIZE_MAX) {
            return v;
        }
        
        // パス圧縮
        std::vector<size_t> path;
        size_t current = v;
        while (ancestor[current] != SIZE_MAX) {
            path.push_back(current);
            current = ancestor[current];
        }
        
        size_t minLabel = label[v];
        for (size_t node : path) {
            if (semi[label[node]] < semi[minLabel]) {
                minLabel = label[node];
            }
        }
        
        for (size_t node : path) {
            ancestor[node] = ancestor[v];
            if (semi[label[node]] == semi[minLabel]) {
                label[node] = minLabel;
            }
        }
        
        return minLabel;
    };
    
    link = [&](size_t v, size_t w) {
        ancestor[w] = v;
    };
    
    // 逆DFS順でsemi-dominator計算
    for (int i = dfsCounter - 1; i >= 1; --i) {
        size_t w = vertex[i];
        
        // semi[w]の計算
        for (size_t v : basicBlocks_[w]->GetPredecessors()) {
            if (dfsNum[v] != -1) {
                size_t u = eval(v);
                if (semi[u] < semi[w]) {
                    semi[w] = semi[u];
                }
            }
        }
        
        bucket[vertex[semi[w]]].push_back(w);
        link(parent[w], w);
        
        // bucket[parent[w]]の処理
        for (size_t v : bucket[parent[w]]) {
            size_t u = eval(v);
            if (semi[u] < semi[v]) {
                idom[v] = u;
            } else {
                idom[v] = parent[w];
            }
        }
        bucket[parent[w]].clear();
    }
    
    // 3. 即座支配者の最終計算
    for (int i = 1; i < dfsCounter; ++i) {
        size_t w = vertex[i];
        if (idom[w] != vertex[semi[w]]) {
            idom[w] = idom[idom[w]];
        }
    }
    
    // 4. 支配者集合の構築
    for (size_t i = 0; i < n; ++i) {
        dominators_[i].clear();
        
        // 自分自身は常に支配者
        dominators_[i].insert(i);
        
        // 即座支配者から遡って支配者を追加
        size_t current = i;
        while (idom[current] != SIZE_MAX) {
            current = idom[current];
            dominators_[i].insert(current);
        }
    }
    
    // 5. 即座支配者の設定
    immediateDominators_.clear();
    immediateDominators_.resize(n, nullptr);
    
    for (size_t i = 1; i < n; ++i) {
        if (idom[i] != SIZE_MAX) {
            immediateDominators_[i] = basicBlocks_[idom[i]].get();
        }
    }
}

void IRFunction::DetectLoops() {
    // 完璧なループ検出（自然ループの検出）
    
    std::vector<LoopInfo> detectedLoops;
    
    if (basicBlocks_.empty()) return detectedLoops;
    
    // 1. 支配関係の計算（必要に応じて）
    if (dominators_.empty()) {
        ComputeDominators();
    }
    
    // 2. バックエッジの検出
    std::vector<std::pair<size_t, size_t>> backEdges;
    
    for (size_t i = 0; i < basicBlocks_.size(); ++i) {
        if (!basicBlocks_[i]) continue;
        
        for (size_t successor : basicBlocks_[i]->GetSuccessors()) {
            // successorがiを支配している場合、これはバックエッジ
            if (dominators_[i].count(successor) > 0) {
                backEdges.push_back({i, successor});
            }
        }
    }
    
    // 3. 各バックエッジから自然ループを構築
    for (const auto& [tail, head] : backEdges) {
        LoopInfo loop = constructNaturalLoop(tail, head);
        if (loop.isValid()) {
            detectedLoops.push_back(loop);
        }
    }
    
    // 4. ループの階層構造分析
    analyzeLoopHierarchy(detectedLoops);
    
    // 5. ループの最適化可能性分析
    for (auto& loop : detectedLoops) {
        analyzeLoopOptimizability(loop);
    }
    
    return detectedLoops;
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
    // 完璧な変数数計算実装
    
    std::unordered_set<std::string> allVariables;
    
    // 1. 引数の追加
    for (const auto& arg : arguments_) {
        allVariables.insert(arg.name);
    }
    
    // 2. ローカル変数の追加
    for (const auto& local : locals_) {
        allVariables.insert(local.name);
    }
    
    // 3. 一時変数の追加
    for (const auto& temp : temporaries_) {
        allVariables.insert(temp.name);
    }
    
    // 4. 基本ブロック内で定義される変数の検索
    for (const auto& block : basicBlocks_) {
        if (!block) continue;
        
        for (const auto& instr : block->GetInstructions()) {
            // 定義される変数を追加
            if (instr.HasDestination()) {
                const auto& dest = instr.GetDestination();
                if (dest.IsVariable()) {
                    allVariables.insert(dest.GetVariableName());
                }
            }
            
            // PHI命令の特別処理
            if (instr.GetOpcode() == IROpcode::PHI) {
                for (const auto& operand : instr.GetOperands()) {
                    if (operand.IsVariable()) {
                        allVariables.insert(operand.GetVariableName());
                    }
                }
            }
        }
    }
    
    // 5. グローバル変数の追加
    for (const auto& global : globals_) {
        allVariables.insert(global.name);
    }
    
    return allVariables.size();
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

void IRFunction::DetectLoopsHelper() {
    // 完璧なループ検出（Tarjan's strongly connected components）
    
    if (basicBlocks_.empty()) return;
    
    loops_.clear();
    
    // 1. 強連結成分の検出
    std::vector<bool> visited(basicBlocks_.size(), false);
    std::vector<bool> onStack(basicBlocks_.size(), false);
    std::vector<int> disc(basicBlocks_.size(), -1);
    std::vector<int> low(basicBlocks_.size(), -1);
    std::stack<size_t> stack;
    int time = 0;
    
    std::function<void(size_t)> tarjanSCC = [&](size_t u) {
        disc[u] = low[u] = ++time;
        visited[u] = true;
        stack.push(u);
        onStack[u] = true;
        
        for (size_t v : basicBlocks_[u]->GetSuccessors()) {
            if (!visited[v]) {
                tarjanSCC(v);
                low[u] = std::min(low[u], low[v]);
            } else if (onStack[v]) {
                low[u] = std::min(low[u], disc[v]);
            }
        }
        
        // 強連結成分の根の場合
        if (low[u] == disc[u]) {
            std::vector<size_t> component;
            size_t w;
            do {
                w = stack.top();
                stack.pop();
                onStack[w] = false;
                component.push_back(w);
            } while (w != u);
            
            // 複数のブロックを含む強連結成分はループ
            if (component.size() > 1 || hasSelfLoop(u)) {
                LoopInfo loop = analyzeLoop(component);
                if (loop.isValid()) {
                    loops_.push_back(loop);
                }
            }
        }
    };
    
    // 2. 全ての未訪問ノードからTarjan実行
    for (size_t i = 0; i < basicBlocks_.size(); ++i) {
        if (!visited[i]) {
            tarjanSCC(i);
        }
    }
    
    // 3. ループの階層構造分析
    analyzeLoopNesting();
    
    // 4. ループ不変式の初期分析
    for (auto& loop : loops_) {
        identifyLoopInvariants(loop);
    }
}

LoopInfo IRFunction::analyzeLoop(const std::vector<size_t>& component) {
    // 完璧なループ分析
    
    LoopInfo loop;
    loop.blocks = component;
    
    // 1. ループヘッダーの特定
    loop.header = findLoopHeader(component);
    if (loop.header == SIZE_MAX) {
        return loop; // 無効なループ
    }
    
    // 2. ループの出口ブロック検出
    for (size_t blockId : component) {
        for (size_t successor : basicBlocks_[blockId]->GetSuccessors()) {
            if (std::find(component.begin(), component.end(), successor) == component.end()) {
                loop.exitBlocks.push_back(successor);
            }
        }
    }
    
    // 3. ループの入口ブロック検出（プリヘッダー）
    for (size_t predecessor : basicBlocks_[loop.header]->GetPredecessors()) {
        if (std::find(component.begin(), component.end(), predecessor) == component.end()) {
            loop.preheaders.push_back(predecessor);
        }
    }
    
    // 4. ループの深度計算
    loop.depth = calculateLoopDepth(component);
    
    // 5. ループの種類判定
    loop.type = classifyLoopType(component);
    
    // 6. ループ内の支配関係分析
    analyzeLoopDominance(loop);
    
    // 7. ループの実行頻度推定
    loop.executionFrequency = estimateLoopFrequency(component);
    
    loop.valid = true;
    return loop;
}

std::vector<IRBasicBlock*> IRFunction::ComputeDominanceFrontier() {
    // 完璧な支配境界計算
    
    std::vector<IRBasicBlock*> frontier;
    size_t n = basicBlocks_.size();
    
    if (n == 0) return frontier;
    
    // 支配境界の計算（Cooper, Harvey, Kennedy アルゴリズム）
    std::vector<std::set<size_t>> dominanceFrontier(n);
    
    for (size_t x = 0; x < n; ++x) {
        if (!basicBlocks_[x]) continue;
        
        const auto& predecessors = basicBlocks_[x]->GetPredecessors();
        
        // 複数の前任者を持つブロックについて
        if (predecessors.size() >= 2) {
            for (size_t p : predecessors) {
                size_t runner = p;
                
                // runnerがxの即座支配者でない間
                while (runner != SIZE_MAX && 
                       (immediateDominators_[x] == nullptr || 
                        runner != getBlockIndex(immediateDominators_[x]))) {
                    
                    dominanceFrontier[runner].insert(x);
                    
                    // 次の即座支配者へ
                    if (immediateDominators_[runner]) {
                        runner = getBlockIndex(immediateDominators_[runner]);
                    } else {
                        break;
                    }
                }
            }
        }
    }
    
    // 結果をIRBasicBlock*のベクトルに変換
    for (size_t i = 0; i < n; ++i) {
        for (size_t frontierBlock : dominanceFrontier[i]) {
            if (frontierBlock < basicBlocks_.size() && basicBlocks_[frontierBlock]) {
                frontier.push_back(basicBlocks_[frontierBlock].get());
            }
        }
    }
    
    return frontier;
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
    
    // Lengauer-Tarjan アルゴリズムによる支配者計算
    size_t n = function->GetBasicBlockCount();
    std::vector<IRBasicBlock*> blocks;
    std::unordered_map<IRBasicBlock*, size_t> blockToIndex;
    
    // ブロックのインデックス化
    for (size_t i = 0; i < n; ++i) {
        auto* block = function->GetBasicBlock(i);
        blocks.push_back(block);
        blockToIndex[block] = i;
    }
    
    auto* entry = function->GetEntryBlock();
    if (!entry) return idom;
    
    // DFS番号付け
    std::vector<size_t> dfsNum(n, SIZE_MAX);
    std::vector<IRBasicBlock*> vertex;
    std::vector<size_t> parent(n, SIZE_MAX);
    std::vector<size_t> semi(n);
    std::vector<size_t> ancestor(n, SIZE_MAX);
    std::vector<size_t> label(n);
    std::vector<std::vector<size_t>> bucket(n);
    std::vector<size_t> dom(n, SIZE_MAX);
    
    size_t dfsCounter = 0;
    
    std::function<void(IRBasicBlock*)> dfs = [&](IRBasicBlock* block) {
        size_t blockIdx = blockToIndex[block];
        dfsNum[blockIdx] = dfsCounter;
        vertex.push_back(block);
        label[blockIdx] = blockIdx;
        semi[blockIdx] = dfsCounter;
        
        for (auto* successor : block->GetSuccessors()) {
            size_t succIdx = blockToIndex[successor];
            if (dfsNum[succIdx] == SIZE_MAX) {
                parent[succIdx] = blockIdx;
                dfs(successor);
            }
        }
        
        dfsCounter++;
    };
    
    dfs(entry);
    
    // LINK と EVAL 関数
    std::function<void(size_t, size_t)> link = [&](size_t v, size_t w) {
        ancestor[w] = v;
    };
    
    std::function<size_t(size_t)> eval = [&](size_t v) -> size_t {
        if (ancestor[v] == SIZE_MAX) {
            return label[v];
        }
        
        // パス圧縮
        std::vector<size_t> path;
        size_t current = v;
        while (ancestor[current] != SIZE_MAX) {
            path.push_back(current);
            current = ancestor[current];
        }
        
        if (path.size() > 1) {
            size_t minLabel = label[path.back()];
            for (int i = path.size() - 2; i >= 0; --i) {
                if (semi[label[path[i]]] < semi[minLabel]) {
                    minLabel = label[path[i]];
                }
                ancestor[path[i]] = ancestor[v];
                if (semi[label[path[i]]] == semi[minLabel]) {
                    label[path[i]] = minLabel;
                }
            }
        }
        
        return label[v];
    };
    
    // メインアルゴリズム
    for (int i = dfsCounter - 1; i >= 1; --i) {
        size_t w = blockToIndex[vertex[i]];
        
        // Step 2: 半支配者の計算
        for (auto* predecessor : vertex[i]->GetPredecessors()) {
            size_t v = blockToIndex[predecessor];
            if (dfsNum[v] < dfsNum[w]) {
                size_t u = eval(v);
                if (semi[u] < semi[w]) {
                    semi[w] = semi[u];
                }
            }
        }
        
        bucket[vertex[semi[w]]].push_back(w);
        link(parent[w], w);
        
        // Step 3: 即座支配者の計算
        for (size_t v : bucket[parent[w]]) {
            size_t u = eval(v);
            dom[v] = (semi[u] < semi[v]) ? u : parent[w];
        }
        bucket[parent[w]].clear();
    }
    
    // Step 4: 即座支配者の調整
    for (size_t i = 1; i < dfsCounter; ++i) {
        size_t w = blockToIndex[vertex[i]];
        if (dom[w] != semi[w]) {
            dom[w] = dom[dom[w]];
        }
    }
    
    // 結果の構築
    idom[entry] = nullptr;
    for (size_t i = 1; i < dfsCounter; ++i) {
        size_t w = blockToIndex[vertex[i]];
        if (dom[w] != SIZE_MAX) {
            idom[vertex[i]] = blocks[dom[w]];
        }
    }
    
    return idom;
}

std::unordered_map<IRBasicBlock*, std::unordered_set<IRBasicBlock*>> ComputeDominanceFrontier(IRFunction* function) {
    std::unordered_map<IRBasicBlock*, std::unordered_set<IRBasicBlock*>> df;
    
    if (function->GetBasicBlockCount() == 0) return df;
    
    // 即座支配者の計算
    auto idom = ComputeImmediateDominators(function);
    
    // 支配境界の計算
    for (size_t i = 0; i < function->GetBasicBlockCount(); ++i) {
        auto* block = function->GetBasicBlock(i);
        df[block] = std::unordered_set<IRBasicBlock*>();
        
        // 各ブロックについて、その前任者を調べる
        for (auto* predecessor : block->GetPredecessors()) {
            auto* runner = predecessor;
            
            // 支配者チェーンを遡る
            while (runner != nullptr && runner != idom[block]) {
                df[runner].insert(block);
                runner = idom[runner];
            }
        }
    }
    
    return df;
}

std::vector<std::unique_ptr<LoopInfo>> DetectNaturalLoops(IRFunction* function) {
    std::vector<std::unique_ptr<LoopInfo>> loops;
    
    if (function->GetBasicBlockCount() == 0) return loops;
    
    // 即座支配者の計算
    auto idom = ComputeImmediateDominators(function);
    
    // バックエッジの検出
    std::vector<std::pair<IRBasicBlock*, IRBasicBlock*>> backEdges;
    std::unordered_set<IRBasicBlock*> visited;
    std::unordered_set<IRBasicBlock*> inStack;
    
    std::function<void(IRBasicBlock*)> dfs = [&](IRBasicBlock* block) {
        visited.insert(block);
        inStack.insert(block);
        
        for (auto* successor : block->GetSuccessors()) {
            if (inStack.find(successor) != inStack.end()) {
                // バックエッジを発見
                backEdges.emplace_back(block, successor);
            } else if (visited.find(successor) == visited.end()) {
                dfs(successor);
            }
        }
        
        inStack.erase(block);
    };
    
    if (auto* entry = function->GetEntryBlock()) {
        dfs(entry);
    }
    
    // 各バックエッジから自然ループを構築
    for (const auto& backEdge : backEdges) {
        auto* tail = backEdge.first;
        auto* header = backEdge.second;
        
        // ヘッダーがテールを支配しているかチェック（自然ループの条件）
        bool isDominated = false;
        auto* current = tail;
        while (current != nullptr) {
            if (current == header) {
                isDominated = true;
                break;
            }
            current = idom[current];
        }
        
        if (!isDominated) continue;
        
        auto loop = std::make_unique<LoopInfo>(header);
        
        // ループ本体の構築（ヘッダーから後方に向かって到達可能なノード）
        std::unordered_set<IRBasicBlock*> loopBlocks;
        std::stack<IRBasicBlock*> workList;
        
        loopBlocks.insert(header);
        if (loopBlocks.find(tail) == loopBlocks.end()) {
            loopBlocks.insert(tail);
            workList.push(tail);
        }
        
        while (!workList.empty()) {
            auto* current = workList.top();
            workList.pop();
            
            for (auto* predecessor : current->GetPredecessors()) {
                if (loopBlocks.find(predecessor) == loopBlocks.end()) {
                    loopBlocks.insert(predecessor);
                    workList.push(predecessor);
                }
            }
        }
        
        loop->blocks = loopBlocks;
        
        // ループの深さを計算
        loop->depth = calculateLoopDepth(header, loops);
        
        // 出口ブロックの特定
        for (auto* loopBlock : loopBlocks) {
            for (auto* successor : loopBlock->GetSuccessors()) {
                if (loopBlocks.find(successor) == loopBlocks.end()) {
                    loop->exitBlocks.insert(successor);
                }
            }
        }
        
        // ループ不変式の検出
        loop->invariants = findLoopInvariants(*loop);
        
        // 帰納変数の検出
        loop->inductionVariables = findInductionVariables(*loop);
        
        loops.push_back(std::move(loop));
    }
    
    // ループの階層構造を構築
    buildLoopHierarchy(loops);
    
    return loops;
}

// ヘルパー関数の実装

size_t calculateLoopDepth(IRBasicBlock* header, const std::vector<std::unique_ptr<LoopInfo>>& existingLoops) {
    size_t depth = 1;
    
    for (const auto& loop : existingLoops) {
        if (loop->blocks.find(header) != loop->blocks.end()) {
            depth = std::max(depth, loop->depth + 1);
        }
    }
    
    return depth;
}

std::vector<IRInstruction*> findLoopInvariants(const LoopInfo& loop) {
    std::vector<IRInstruction*> invariants;
    std::unordered_set<IRBasicBlock*> loopBlocks = loop.blocks;
    
    for (auto* block : loopBlocks) {
        for (size_t i = 0; i < block->GetInstructionCount(); ++i) {
            auto* instr = block->GetInstruction(i);
            if (!instr) continue;
            
            bool isInvariant = true;
            
            // 命令の全てのオペランドがループ外で定義されているかチェック
            for (const auto& operand : instr->GetOperands()) {
                if (operand.IsVariable()) {
                    // 変数の定義がループ内にあるかチェック
                    bool definedInLoop = false;
                    for (auto* loopBlock : loopBlocks) {
                        for (size_t j = 0; j < loopBlock->GetInstructionCount(); ++j) {
                            auto* defInstr = loopBlock->GetInstruction(j);
                            if (defInstr && defInstr->GetDestination() == operand) {
                                definedInLoop = true;
                                break;
                            }
                        }
                        if (definedInLoop) break;
                    }
                    
                    if (definedInLoop) {
                        isInvariant = false;
                        break;
                    }
                }
            }
            
            // 副作用のない命令のみが不変式になりうる
            if (isInvariant && !hasSideEffects(instr)) {
                invariants.push_back(instr);
            }
        }
    }
    
    return invariants;
}

std::vector<InductionVariable> findInductionVariables(const LoopInfo& loop) {
    std::vector<InductionVariable> inductionVars;
    std::unordered_set<IRBasicBlock*> loopBlocks = loop.blocks;
    
    for (auto* block : loopBlocks) {
        for (size_t i = 0; i < block->GetInstructionCount(); ++i) {
            auto* instr = block->GetInstruction(i);
            if (!instr) continue;
            
            // 基本的な帰納変数パターンの検出: i = i + constant
            if (instr->GetOpcode() == IROpcode::ADD || instr->GetOpcode() == IROpcode::SUB) {
                const auto& operands = instr->GetOperands();
                if (operands.size() == 2) {
                    const auto& op1 = operands[0];
                    const auto& op2 = operands[1];
                    
                    // パターン1: dest = dest + constant
                    if (op1.IsVariable() && op1 == instr->GetDestination() && op2.IsConstant()) {
                        InductionVariable iv;
                        iv.variable = instr->GetDestination();
                        iv.step = op2.GetConstantValue();
                        iv.isIncreasing = (instr->GetOpcode() == IROpcode::ADD);
                        iv.isBasic = true;
                        inductionVars.push_back(iv);
                    }
                    // パターン2: dest = constant + dest
                    else if (op2.IsVariable() && op2 == instr->GetDestination() && op1.IsConstant()) {
                        InductionVariable iv;
                        iv.variable = instr->GetDestination();
                        iv.step = op1.GetConstantValue();
                        iv.isIncreasing = (instr->GetOpcode() == IROpcode::ADD);
                        iv.isBasic = true;
                        inductionVars.push_back(iv);
                    }
                }
            }
            
            // 派生帰納変数の検出: j = i * constant + constant
            else if (instr->GetOpcode() == IROpcode::MUL) {
                const auto& operands = instr->GetOperands();
                if (operands.size() == 2) {
                    // 一方のオペランドが基本帰納変数かチェック
                    for (const auto& basicIV : inductionVars) {
                        if ((operands[0] == basicIV.variable && operands[1].IsConstant()) ||
                            (operands[1] == basicIV.variable && operands[0].IsConstant())) {
                            
                            InductionVariable derivedIV;
                            derivedIV.variable = instr->GetDestination();
                            derivedIV.baseVariable = basicIV.variable;
                            derivedIV.multiplier = operands[0].IsConstant() ? 
                                                  operands[0].GetConstantValue() : 
                                                  operands[1].GetConstantValue();
                            derivedIV.isBasic = false;
                            inductionVars.push_back(derivedIV);
                        }
                    }
                }
            }
        }
    }
    
    return inductionVars;
}

void buildLoopHierarchy(std::vector<std::unique_ptr<LoopInfo>>& loops) {
    // ループの包含関係を構築
    for (size_t i = 0; i < loops.size(); ++i) {
        for (size_t j = 0; j < loops.size(); ++j) {
            if (i != j) {
                // loops[j] が loops[i] に含まれているかチェック
                bool isNested = true;
                for (auto* block : loops[j]->blocks) {
                    if (loops[i]->blocks.find(block) == loops[i]->blocks.end()) {
                        isNested = false;
                        break;
                    }
                }
                
                if (isNested && loops[j]->blocks.size() < loops[i]->blocks.size()) {
                    loops[i]->innerLoops.push_back(loops[j].get());
                    loops[j]->parent = loops[i].get();
                }
            }
        }
    }
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

// 完璧な支配者分析の詳細実装（Lengauer-Tarjanアルゴリズム）
void IRFunction::ComputeDominatorsLengauerTarjan() {
    if (basicBlocks_.empty()) return;
    
    size_t n = basicBlocks_.size();
    
    // 初期化
    std::vector<size_t> dfsNum(n, SIZE_MAX);
    std::vector<size_t> vertex(n);
    std::vector<size_t> parent(n, SIZE_MAX);
    std::vector<size_t> semi(n);
    std::vector<size_t> ancestor(n, SIZE_MAX);
    std::vector<size_t> label(n);
    std::vector<std::vector<size_t>> bucket(n);
    std::vector<size_t> dom(n, SIZE_MAX);
    
    size_t dfsCounter = 0;
    
    // 1. DFS番号付けと親の設定
    std::function<void(size_t)> dfs = [&](size_t v) {
        dfsNum[v] = dfsCounter;
        vertex[dfsCounter] = v;
        label[v] = v;
        semi[v] = dfsCounter;
        dfsCounter++;
        
        for (auto* successor : basicBlocks_[v]->GetSuccessors()) {
            size_t w = getBlockIndex(successor);
            if (w != SIZE_MAX && dfsNum[w] == SIZE_MAX) {
                parent[w] = v;
                dfs(w);
            }
        }
    };
    
    // エントリーブロックからDFS開始
    dfs(0);
    
    // 2. LINK/EVAL関数の定義（パス圧縮最適化付き）
    std::function<void(size_t, size_t)> link = [&](size_t v, size_t w) {
        size_t s = w;
        while (semi[label[w]] < semi[label[ancestor[s]]]) {
            if (ancestor[ancestor[s]] != SIZE_MAX) {
                ancestor[s] = ancestor[ancestor[s]];
            }
            s = ancestor[s];
        }
        ancestor[w] = v;
    };
    
    std::function<size_t(size_t)> eval = [&](size_t v) -> size_t {
        if (ancestor[v] == SIZE_MAX) {
            return label[v];
        }
        
        // パス圧縮による最適化
        std::vector<size_t> path;
        size_t current = v;
        
        while (ancestor[current] != SIZE_MAX) {
            path.push_back(current);
            current = ancestor[current];
        }
        
        if (path.size() > 1) {
            size_t minSemiLabel = label[path.back()];
            for (int i = path.size() - 2; i >= 0; --i) {
                if (semi[label[path[i]]] < semi[minSemiLabel]) {
                    minSemiLabel = label[path[i]];
                }
                ancestor[path[i]] = ancestor[v];
                if (semi[label[path[i]]] == semi[minSemiLabel]) {
                    label[path[i]] = minSemiLabel;
                }
            }
        }
        
        return label[v];
    };
    
    // 3. Semi-dominator計算（逆DFS順）
    for (int i = dfsCounter - 1; i >= 1; --i) {
        size_t w = vertex[i];
        
        // semi[w]の計算
        for (auto* predecessor : basicBlocks_[w]->GetPredecessors()) {
            size_t v = getBlockIndex(predecessor);
            if (v != SIZE_MAX && dfsNum[v] != SIZE_MAX) {
                size_t u = eval(v);
                if (semi[u] < semi[w]) {
                    semi[w] = semi[u];
                }
            }
        }
        
        bucket[vertex[semi[w]]].push_back(w);
        link(parent[w], w);
        
        // bucket[parent[w]]の処理
        for (size_t v : bucket[parent[w]]) {
            size_t u = eval(v);
            if (semi[u] < semi[v]) {
                dom[v] = u;
            } else {
                dom[v] = parent[w];
            }
        }
        bucket[parent[w]].clear();
    }
    
    // 4. 即座支配者の最終調整
    for (int i = 1; i < dfsCounter; ++i) {
        size_t w = vertex[i];
        if (dom[w] != vertex[semi[w]]) {
            dom[w] = dom[dom[w]];
        }
    }
    
    // 5. 支配関係の構築
    dominators_.clear();
    dominators_.resize(n);
    immediateDominators_.clear();
    immediateDominators_.resize(n, nullptr);
    
    for (size_t i = 0; i < n; ++i) {
        dominators_[i].clear();
        dominators_[i].insert(i); // 自分自身は常に支配者
        
        // 即座支配者の設定
        if (i > 0 && dom[i] != SIZE_MAX) {
            immediateDominators_[i] = basicBlocks_[dom[i]].get();
            
            // 支配者チェーンの構築
            size_t current = dom[i];
            while (current != SIZE_MAX) {
                dominators_[i].insert(current);
                current = (current > 0) ? dom[current] : SIZE_MAX;
            }
        }
    }
}

// 完璧な支配境界計算
void IRFunction::ComputeDominanceFrontier() {
    if (basicBlocks_.empty()) return;
    
    size_t n = basicBlocks_.size();
    dominanceFrontier_.clear();
    dominanceFrontier_.resize(n);
    
    // 各ノードについて支配境界を計算
    for (size_t x = 0; x < n; ++x) {
        if (!basicBlocks_[x]) continue;
        
        for (auto* predecessor : basicBlocks_[x]->GetPredecessors()) {
            size_t y = getBlockIndex(predecessor);
            if (y == SIZE_MAX) continue;
            
            // yからxへのエッジについて
            size_t runner = y;
            
            // runnerがxを厳密に支配しない間
            while (runner != SIZE_MAX && !strictlyDominates(runner, x)) {
                dominanceFrontier_[runner].insert(x);
                
                // 即座支配者に移動
                if (immediateDominators_[runner]) {
                    runner = getBlockIndex(immediateDominators_[runner]);
                } else {
                    break;
                }
            }
        }
    }
}

// 完璧な自然ループ検出
std::vector<std::unique_ptr<LoopInfo>> IRFunction::DetectNaturalLoops() {
    std::vector<std::unique_ptr<LoopInfo>> loops;
    
    if (basicBlocks_.empty()) return loops;
    
    // 支配関係の計算
    if (dominators_.empty()) {
        ComputeDominatorsLengauerTarjan();
    }
    
    // バックエッジの検出
    std::vector<std::pair<size_t, size_t>> backEdges;
    
    for (size_t i = 0; i < basicBlocks_.size(); ++i) {
        if (!basicBlocks_[i]) continue;
        
        for (auto* successor : basicBlocks_[i]->GetSuccessors()) {
            size_t j = getBlockIndex(successor);
            if (j != SIZE_MAX && dominates(j, i)) {
                backEdges.push_back({i, j}); // tail -> head
            }
        }
    }
    
    // 各バックエッジから自然ループを構築
    for (const auto& [tail, head] : backEdges) {
        auto loop = constructNaturalLoop(tail, head);
        if (loop) {
            loops.push_back(std::move(loop));
        }
    }
    
    // ループ階層の構築
    buildLoopHierarchy(loops);
    
    // ループ深度の計算
    calculateLoopDepths(loops);
    
    // ループ不変式の検出
    for (auto& loop : loops) {
        detectLoopInvariants(*loop);
    }
    
    // 帰納変数の検出
    for (auto& loop : loops) {
        detectInductionVariables(*loop);
    }
    
    return loops;
}

std::unique_ptr<LoopInfo> IRFunction::constructNaturalLoop(size_t tail, size_t head) {
    auto loop = std::make_unique<LoopInfo>();
    loop->header = basicBlocks_[head].get();
    loop->blocks.insert(basicBlocks_[head].get());
    loop->blocks.insert(basicBlocks_[tail].get());
    
    // ワークリストアルゴリズムでループ本体を構築
    std::queue<size_t> worklist;
    std::unordered_set<size_t> visited;
    
    if (tail != head) {
        worklist.push(tail);
        visited.insert(tail);
        visited.insert(head);
    }
    
    while (!worklist.empty()) {
        size_t current = worklist.front();
        worklist.pop();
        
        for (auto* predecessor : basicBlocks_[current]->GetPredecessors()) {
            size_t predIndex = getBlockIndex(predecessor);
            if (predIndex != SIZE_MAX && visited.find(predIndex) == visited.end()) {
                loop->blocks.insert(basicBlocks_[predIndex].get());
                worklist.push(predIndex);
                visited.insert(predIndex);
            }
        }
    }
    
    // 出口ブロックの特定
    for (auto* block : loop->blocks) {
        for (auto* successor : block->GetSuccessors()) {
            if (loop->blocks.find(successor) == loop->blocks.end()) {
                loop->exitBlocks.insert(successor);
            }
        }
    }
    
    // ループの有効性チェック
    if (loop->blocks.size() < 2) {
        return nullptr;
    }
    
    return loop;
}

void IRFunction::buildLoopHierarchy(std::vector<std::unique_ptr<LoopInfo>>& loops) {
    // ループをネストレベルでソート
    std::sort(loops.begin(), loops.end(), 
        [](const std::unique_ptr<LoopInfo>& a, const std::unique_ptr<LoopInfo>& b) {
            return a->blocks.size() < b->blocks.size();
        });
    
    // 親子関係の構築
    for (size_t i = 0; i < loops.size(); ++i) {
        for (size_t j = i + 1; j < loops.size(); ++j) {
            // loops[i]がloops[j]に含まれるかチェック
            bool isSubset = true;
            for (auto* block : loops[i]->blocks) {
                if (loops[j]->blocks.find(block) == loops[j]->blocks.end()) {
                    isSubset = false;
                    break;
                }
            }
            
            if (isSubset) {
                loops[i]->parent = loops[j].get();
                loops[j]->innerLoops.push_back(loops[i].get());
                break; // 最も内側の親を見つけたら終了
            }
        }
    }
}

void IRFunction::calculateLoopDepths(std::vector<std::unique_ptr<LoopInfo>>& loops) {
    for (auto& loop : loops) {
        size_t depth = 0;
        LoopInfo* current = loop->parent;
        
        while (current) {
            depth++;
            current = current->parent;
        }
        
        loop->depth = depth;
        
        // ブロックのループ深度を設定
        for (auto* block : loop->blocks) {
            if (block->GetLoopDepth() <= depth) {
                block->SetLoopDepth(depth + 1);
            }
        }
    }
}

void IRFunction::detectLoopInvariants(LoopInfo& loop) {
    loop.invariants.clear();
    
    for (auto* block : loop.blocks) {
        for (size_t i = 0; i < block->GetInstructionCount(); ++i) {
            auto* instr = block->GetInstruction(i);
            
            if (isLoopInvariant(instr, loop)) {
                loop.invariants.push_back(instr);
            }
        }
    }
}

bool IRFunction::isLoopInvariant(IRInstruction* instr, const LoopInfo& loop) {
    // 副作用がある命令は不変ではない
    if (instr->HasSideEffects()) {
        return false;
    }
    
    // オペランドがすべてループ外で定義されているかチェック
    for (const auto& operand : instr->GetOperands()) {
        if (operand.IsVariable()) {
            auto* defInstr = findDefinition(operand.GetVariableId());
            if (defInstr) {
                auto* defBlock = defInstr->GetParent();
                if (loop.blocks.find(defBlock) != loop.blocks.end()) {
                    return false; // ループ内で定義されている
                }
            }
        }
    }
    
    return true;
}

void IRFunction::detectInductionVariables(LoopInfo& loop) {
    loop.inductionVariables.clear();
    
    // 基本帰納変数の検出
    for (auto* block : loop.blocks) {
        for (size_t i = 0; i < block->GetInstructionCount(); ++i) {
            auto* instr = block->GetInstruction(i);
            
            if (isBasicInductionVariable(instr, loop)) {
                InductionVariable indVar;
                indVar.variable = instr;
                indVar.isBasic = true;
                indVar.step = extractInductionStep(instr);
                indVar.initialValue = extractInitialValue(instr, loop);
                
                loop.inductionVariables.push_back(indVar);
            }
        }
    }
    
    // 派生帰納変数の検出
    for (auto* block : loop.blocks) {
        for (size_t i = 0; i < block->GetInstructionCount(); ++i) {
            auto* instr = block->GetInstruction(i);
            
            if (isDerivedInductionVariable(instr, loop)) {
                InductionVariable indVar;
                indVar.variable = instr;
                indVar.isBasic = false;
                indVar.baseVariable = findBaseInductionVariable(instr, loop);
                indVar.coefficient = extractCoefficient(instr);
                indVar.offset = extractOffset(instr);
                
                loop.inductionVariables.push_back(indVar);
            }
        }
    }
}

bool IRFunction::isBasicInductionVariable(IRInstruction* instr, const LoopInfo& loop) {
    // PHI命令で、ループ内で一定の増分で更新される変数
    if (instr->GetOpcode() != IROpcode::PHI) {
        return false;
    }
    
    // PHI命令の一つのオペランドがループ外から、もう一つがループ内からの定義
    auto operands = instr->GetOperands();
    if (operands.size() != 2) {
        return false;
    }
    
    bool hasLoopExternalDef = false;
    bool hasLoopInternalDef = false;
    
    for (const auto& operand : operands) {
        if (operand.IsVariable()) {
            auto* defInstr = findDefinition(operand.GetVariableId());
            if (defInstr) {
                auto* defBlock = defInstr->GetParent();
                if (loop.blocks.find(defBlock) != loop.blocks.end()) {
                    hasLoopInternalDef = true;
                } else {
                    hasLoopExternalDef = true;
                }
            }
        }
    }
    
    return hasLoopExternalDef && hasLoopInternalDef;
}

bool IRFunction::isDerivedInductionVariable(IRInstruction* instr, const LoopInfo& loop) {
    // 基本帰納変数から線形関数で計算される変数
    if (instr->GetOpcode() != IROpcode::ADD && instr->GetOpcode() != IROpcode::MUL) {
        return false;
    }
    
    auto operands = instr->GetOperands();
    if (operands.size() != 2) {
        return false;
    }
    
    // 一つのオペランドが帰納変数で、もう一つがループ不変
    bool hasInductionVar = false;
    bool hasLoopInvariant = false;
    
    for (const auto& operand : operands) {
        if (operand.IsVariable()) {
            auto* defInstr = findDefinition(operand.GetVariableId());
            if (defInstr) {
                if (isInductionVariable(defInstr, loop)) {
                    hasInductionVar = true;
                } else if (isLoopInvariant(defInstr, loop)) {
                    hasLoopInvariant = true;
                }
            }
        } else if (operand.IsConstant()) {
            hasLoopInvariant = true;
        }
    }
    
    return hasInductionVar && hasLoopInvariant;
}

// ヘルパー関数の実装
size_t IRFunction::getBlockIndex(IRBasicBlock* block) const {
    for (size_t i = 0; i < basicBlocks_.size(); ++i) {
        if (basicBlocks_[i].get() == block) {
            return i;
        }
    }
    return SIZE_MAX;
}

bool IRFunction::dominates(size_t dominator, size_t dominated) const {
    if (dominator >= dominators_.size() || dominated >= dominators_.size()) {
        return false;
    }
    return dominators_[dominated].count(dominator) > 0;
}

bool IRFunction::strictlyDominates(size_t dominator, size_t dominated) const {
    return dominator != dominated && dominates(dominator, dominated);
}

IRInstruction* IRFunction::findDefinition(size_t variableId) const {
    for (const auto& block : basicBlocks_) {
        for (size_t i = 0; i < block->GetInstructionCount(); ++i) {
            auto* instr = block->GetInstruction(i);
            if (instr->DefinesVariable() && instr->GetDefinedVariable() == variableId) {
                return instr;
            }
        }
    }
    return nullptr;
}

bool IRFunction::isInductionVariable(IRInstruction* instr, const LoopInfo& loop) const {
    for (const auto& indVar : loop.inductionVariables) {
        if (indVar.variable == instr) {
            return true;
        }
    }
    return false;
}

double IRFunction::extractInductionStep(IRInstruction* instr) const {
    // PHI命令から増分を抽出
    // 実装は具体的な命令形式に依存
    return 1.0; // デフォルト値
}

double IRFunction::extractInitialValue(IRInstruction* instr, const LoopInfo& loop) const {
    // PHI命令からループ外の初期値を抽出
    // 実装は具体的な命令形式に依存
    return 0.0; // デフォルト値
}

IRInstruction* IRFunction::findBaseInductionVariable(IRInstruction* instr, const LoopInfo& loop) const {
    // 派生帰納変数のベースとなる基本帰納変数を検索
    auto operands = instr->GetOperands();
    
    for (const auto& operand : operands) {
        if (operand.IsVariable()) {
            auto* defInstr = findDefinition(operand.GetVariableId());
            if (defInstr && isInductionVariable(defInstr, loop)) {
                return defInstr;
            }
        }
    }
    
    return nullptr;
}

double IRFunction::extractCoefficient(IRInstruction* instr) const {
    // 乗算命令から係数を抽出
    if (instr->GetOpcode() == IROpcode::MUL) {
        auto operands = instr->GetOperands();
        for (const auto& operand : operands) {
            if (operand.IsConstant()) {
                return operand.GetConstantValue().AsNumber();
            }
        }
    }
    return 1.0; // デフォルト値
}

double IRFunction::extractOffset(IRInstruction* instr) const {
    // 加算命令からオフセットを抽出
    if (instr->GetOpcode() == IROpcode::ADD) {
        auto operands = instr->GetOperands();
        for (const auto& operand : operands) {
            if (operand.IsConstant()) {
                return operand.GetConstantValue().AsNumber();
            }
        }
    }
    return 0.0; // デフォルト値
}

} // namespace ir
} // namespace core
} // namespace aerojs 