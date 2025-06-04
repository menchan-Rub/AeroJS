// AeroJS JavaScriptエンジン
// 高性能JITコンパイラのための中間表現（IR）グラフ実装

#include "ir_graph.h"
#include <sstream>
#include <cassert>
#include <algorithm>
#include <unordered_set>
#include <queue>
#include <functional>

namespace aerojs {

// 静的メンバ初期化
uint64_t IRNode::nextId = 1;

// IRNodeの実装

IRNode::IRNode(OpType op, ValueType type)
    : _opType(op),
      _valueType(type),
      _id(nextId++) {
}

bool IRNode::isTypeDependent() const {
    // 型情報に依存する操作を列挙
    switch (_opType) {
        case OpType::Add:
        case OpType::Equal:
        case OpType::StrictEqual:
        case OpType::Call:
        case OpType::LoadProperty:
        case OpType::StoreProperty:
        case OpType::TypeOf:
        case OpType::InstanceOf:
            return true;
        default:
            return false;
    }
}

void IRNode::addInput(IRNode* node) {
    if (node) {
        _inputs.push_back(node);
        node->addUser(this);
    }
}

void IRNode::replaceInput(IRNode* oldNode, IRNode* newNode) {
    if (!oldNode || !newNode) return;
    
    for (size_t i = 0; i < _inputs.size(); i++) {
        if (_inputs[i] == oldNode) {
            _inputs[i] = newNode;
            oldNode->removeUser(this);
            newNode->addUser(this);
        }
    }
}

void IRNode::removeInput(IRNode* node) {
    if (!node) return;
    
    auto it = std::find(_inputs.begin(), _inputs.end(), node);
    if (it != _inputs.end()) {
        _inputs.erase(it);
        node->removeUser(this);
    }
}

void IRNode::addUser(IRNode* node) {
    if (node && std::find(_users.begin(), _users.end(), node) == _users.end()) {
        _users.push_back(node);
    }
}

void IRNode::removeUser(IRNode* node) {
    if (!node) return;
    
    auto it = std::find(_users.begin(), _users.end(), node);
    if (it != _users.end()) {
        _users.erase(it);
    }
}

bool IRNode::allInputsAreConstant() const {
    return std::all_of(_inputs.begin(), _inputs.end(), 
                      [](const IRNode* node) { return node->isConstant(); });
}

bool IRNode::hasSideEffects() const {
    switch (_opType) {
        case OpType::Call:
        case OpType::New:
        case OpType::StoreProperty:
        case OpType::StoreElement:
        case OpType::StoreGlobal:
        case OpType::StoreLocal:
        case OpType::Deoptimize:
            return true;
        default:
            return false;
    }
}

bool IRNode::isArithmeticOp() const {
    switch (_opType) {
        case OpType::Add:
        case OpType::Sub:
        case OpType::Mul:
        case OpType::Div:
        case OpType::Mod:
        case OpType::Neg:
        case OpType::BitwiseAnd:
        case OpType::BitwiseOr:
        case OpType::BitwiseXor:
        case OpType::BitwiseNot:
        case OpType::ShiftLeft:
        case OpType::ShiftRight:
        case OpType::UnsignedShiftRight:
            return true;
        default:
            return false;
    }
}

bool IRNode::isPure() const {
    return !hasSideEffects() && !isControlFlow();
}

bool IRNode::isControlFlow() const {
    switch (_opType) {
        case OpType::Entry:
        case OpType::Exit:
        case OpType::Jump:
        case OpType::Branch:
        case OpType::Return:
        case OpType::Deoptimize:
            return true;
        default:
            return false;
    }
}

bool IRNode::isProperty() const {
    return _opType == OpType::LoadProperty || _opType == OpType::StoreProperty;
}

size_t IRNode::computeHash() const {
    // IRノードの衝突耐性ハッシュ計算
    // 現在は基本的な組み合わせハッシュを使用。
    // 本格的な実装では以下の改善が必要：
    // 1. FNV-1a, xxHash, CityHashなどの高品質ハッシュ関数の使用
    // 2. ノードの型、入力数、付加情報も考慮したハッシュ
    // 3. ハッシュテーブルのサイズに応じた分散最適化
    // 4. 暗号学的安全性が必要な場合は、ソルト追加の検討
    size_t result = static_cast<size_t>(_opType);
    
    for (const auto* input : _inputs) {
        result = result * 31 + input->id();
    }
    
    return result;
}

bool IRNode::equals(const IRNode* other) const {
    if (!other || _opType != other->_opType || _inputs.size() != other->_inputs.size()) {
        return false;
    }
    
    // 定数は値を比較
    if (isConstant() && other->isConstant()) {
        const auto* thisConst = static_cast<const ConstantNode*>(this);
        const auto* otherConst = static_cast<const ConstantNode*>(other);
        // Valueクラスの等価性比較を使用
        return thisConst->value().equals(otherConst->value());
    }
    
    // 入力ノードを比較
    for (size_t i = 0; i < _inputs.size(); i++) {
        if (_inputs[i]->id() != other->_inputs[i]->id()) {
            return false;
        }
    }
    
    return true;
}

std::string IRNode::toString() const {
    std::stringstream ss;
    ss << "Node[" << _id << "]: ";
    
    // 操作タイプ
    switch (_opType) {
        case OpType::Entry: ss << "Entry"; break;
        case OpType::Exit: ss << "Exit"; break;
        case OpType::Jump: ss << "Jump"; break;
        case OpType::Branch: ss << "Branch"; break;
        case OpType::Return: ss << "Return"; break;
        case OpType::LoadProperty: ss << "LoadProperty"; break;
        case OpType::StoreProperty: ss << "StoreProperty"; break;
        case OpType::Add: ss << "Add"; break;
        case OpType::Call: ss << "Call"; break;
        case OpType::Constant: ss << "Constant"; break;
        // ... 他の操作も同様に
        default: ss << "Unknown"; break;
    }
    
    // 型情報
    ss << " (";
    switch (_valueType) {
        case ValueType::Unknown: ss << "Unknown"; break;
        case ValueType::Undefined: ss << "Undefined"; break;
        case ValueType::Null: ss << "Null"; break;
        case ValueType::Boolean: ss << "Boolean"; break;
        case ValueType::Int32: ss << "Int32"; break;
        case ValueType::Float64: ss << "Float64"; break;
        case ValueType::String: ss << "String"; break;
        case ValueType::Symbol: ss << "Symbol"; break;
        case ValueType::Object: ss << "Object"; break;
        case ValueType::Function: ss << "Function"; break;
        case ValueType::Array: ss << "Array"; break;
    }
    ss << ")";
    
    // 入力ノード
    ss << " Inputs: [";
    for (size_t i = 0; i < _inputs.size(); i++) {
        if (i > 0) ss << ", ";
        ss << _inputs[i]->id();
    }
    ss << "]";
    
    return ss.str();
}

// ConstantNode実装

ConstantNode::ConstantNode(const Value& value)
    : IRNode(OpType::Constant),
      _value(value) {
    
    // 値の型に基づいて、IRノードの型を設定
    if (value.isUndefined()) {
        _valueType = ValueType::Undefined;
    } else if (value.isNull()) {
        _valueType = ValueType::Null;
    } else if (value.isBoolean()) {
        _valueType = ValueType::Boolean;
    } else if (value.isInt()) {
        _valueType = ValueType::Int32;
    } else if (value.isNumber()) {
        _valueType = ValueType::Float64;
    } else if (value.isString()) {
        _valueType = ValueType::String;
    } else if (value.isSymbol()) {
        _valueType = ValueType::Symbol;
    } else if (value.isObject()) {
        _valueType = ValueType::Object;
    } else {
        _valueType = ValueType::Unknown;
    }
}

// BasicBlock実装

BasicBlock::BasicBlock(IRGraph* graph, uint32_t id)
    : _graph(graph),
      _id(id) {
}

void BasicBlock::addNode(IRNode* node) {
    if (node) {
        _nodes.push_back(node);
    }
}

void BasicBlock::removeNode(IRNode* node) {
    if (!node) return;
    
    auto it = std::find(_nodes.begin(), _nodes.end(), node);
    if (it != _nodes.end()) {
        _nodes.erase(it);
    }
}

void BasicBlock::addPredecessor(BasicBlock* block) {
    if (block && std::find(_predecessors.begin(), _predecessors.end(), block) == _predecessors.end()) {
        _predecessors.push_back(block);
    }
}

void BasicBlock::addSuccessor(BasicBlock* block) {
    if (block && std::find(_successors.begin(), _successors.end(), block) == _successors.end()) {
        _successors.push_back(block);
        block->addPredecessor(this);
    }
}

void BasicBlock::removePredecessor(BasicBlock* block) {
    if (!block) return;
    
    auto it = std::find(_predecessors.begin(), _predecessors.end(), block);
    if (it != _predecessors.end()) {
        _predecessors.erase(it);
    }
}

void BasicBlock::removeSuccessor(BasicBlock* block) {
    if (!block) return;
    
    auto it = std::find(_successors.begin(), _successors.end(), block);
    if (it != _successors.end()) {
        _successors.erase(it);
        block->removePredecessor(this);
    }
}

// Loop実装

Loop::Loop(BasicBlock* header)
    : _header(header),
      _parentLoop(nullptr) {
    
    addBlock(header);
}

void Loop::addBlock(BasicBlock* block) {
    if (block && std::find(_blocks.begin(), _blocks.end(), block) == _blocks.end()) {
        _blocks.push_back(block);
    }
}

void Loop::addNestedLoop(Loop* loop) {
    if (!loop) return;
    
    _nestedLoops.push_back(loop);
    loop->setParentLoop(this);
    
    // ネストされたループの全ブロックを追加
    for (auto* block : loop->blocks()) {
        addBlock(block);
    }
}

std::vector<IRNode*> Loop::getNodes() const {
    std::vector<IRNode*> result;
    
    for (auto* block : _blocks) {
        auto blockNodes = block->nodes();
        result.insert(result.end(), blockNodes.begin(), blockNodes.end());
    }
    
    return result;
}

// IRGraph実装

IRGraph::IRGraph()
    : _entryNode(nullptr),
      _entryBlock(nullptr),
      _nextBlockId(0) {
}

IRGraph::~IRGraph() {
    // ノードと基本ブロックは_nodesと_blocksが所有
}

IRNode* IRGraph::createNode(OpType op, ValueType type) {
    auto node = std::make_unique<IRNode>(op, type);
    IRNode* result = node.get();
    _nodes.push_back(std::move(node));
    return result;
}

ConstantNode* IRGraph::createConstantNode(const Value& value) {
    auto node = std::make_unique<ConstantNode>(value);
    ConstantNode* result = node.get();
    _nodes.push_back(std::move(node));
    return result;
}

void IRGraph::addNode(IRNode* node) {
    // 既に所有されているかチェック
    for (const auto& ownedNode : _nodes) {
        if (ownedNode.get() == node) {
            return;
        }
    }
    
    // 所有されていない場合は警告
    assert(false && "IRGraph::addNode - ノードがグラフによって所有されていません");
}

void IRGraph::replaceNode(IRNode* oldNode, IRNode* newNode) {
    if (!oldNode || !newNode) return;
    
    // すべてのノードのユーザーからoldNodeを探し、newNodeに置き換える
    for (auto& node : _nodes) {
        node->replaceInput(oldNode, newNode);
    }
    
    // 基本ブロックからノードを置き換える
    for (auto& block : _blocks) {
        if (block->firstNode() == oldNode || block->lastNode() == oldNode) {
            block->removeNode(oldNode);
            block->addNode(newNode);
        }
    }
    
    // グラフのエントリーノードを置き換える
    if (_entryNode == oldNode) {
        _entryNode = newNode;
    }
}

void IRGraph::removeNode(IRNode* node) {
    if (!node) return;
    
    // すべての入力からnodesを削除
    for (auto* input : node->inputs()) {
        input->removeUser(node);
    }
    
    // すべてのユーザーから入力を削除
    for (auto* user : node->users()) {
        user->removeInput(node);
    }
    
    // 基本ブロックからノードを削除
    for (auto& block : _blocks) {
        block->removeNode(node);
    }
    
    // ノードの削除（所有権を解放）
    auto it = std::find_if(_nodes.begin(), _nodes.end(),
                          [node](const std::unique_ptr<IRNode>& ptr) {
                              return ptr.get() == node;
                          });
    
    if (it != _nodes.end()) {
        _nodes.erase(it);
    }
}

BasicBlock* IRGraph::createBasicBlock() {
    auto block = std::make_unique<BasicBlock>(this, _nextBlockId++);
    BasicBlock* result = block.get();
    _blocks.push_back(std::move(block));
    return result;
}

void IRGraph::removeBasicBlock(BasicBlock* block) {
    if (!block) return;
    
    // すべてのブロック間の接続を削除
    for (auto* pred : block->predecessors()) {
        pred->removeSuccessor(block);
    }
    
    for (auto* succ : block->successors()) {
        succ->removePredecessor(block);
    }
    
    // ブロックの削除（所有権を解放）
    auto it = std::find_if(_blocks.begin(), _blocks.end(),
                          [block](const std::unique_ptr<BasicBlock>& ptr) {
                              return ptr.get() == block;
                          });
    
    if (it != _blocks.end()) {
        _blocks.erase(it);
    }
}

std::vector<IRNode*> IRGraph::getAllNodes() const {
    std::vector<IRNode*> result;
    result.reserve(_nodes.size());
    
    for (const auto& node : _nodes) {
        result.push_back(node.get());
    }
    
    return result;
}

std::vector<BasicBlock*> IRGraph::getAllBlocks() const {
    std::vector<BasicBlock*> result;
    result.reserve(_blocks.size());
    
    for (const auto& block : _blocks) {
        result.push_back(block.get());
    }
    
    return result;
}

std::vector<IRNode*> IRGraph::findCallNodes() const {
    std::vector<IRNode*> result;
    
    for (const auto& node : _nodes) {
        if (node->opType() == OpType::Call) {
            result.push_back(node.get());
        }
    }
    
    return result;
}

std::vector<IRNode*> IRGraph::findLoopNodes() const {
    // ループ解析の基本実装
    // 現在はバックエッジ検出によるヒューリスティック手法を使用。
    // 本格的な実装では以下の改善が必要：
    // 1. Tarjanの強連結成分アルゴリズムによる自然ループ検出
    // 2. 支配木を利用したより正確なループ境界の特定
    // 3. ネストしたループの階層構造の構築
    // 4. 非構造化制御フローに対する堅牢性の向上
    // 5. ループ不変式の検出とホイスティング候補の特定
    std::vector<IRNode*> result;
    
    // 深度優先探索によるバックエッジ検出を実装
    std::unordered_set<BasicBlock*> visiting;
    std::unordered_set<BasicBlock*> visited;
    std::unordered_map<BasicBlock*, std::vector<BasicBlock*>> backEdges;
    
    std::function<void(BasicBlock*)> dfs = [&](BasicBlock* block) {
        if (visited.count(block)) return;
        if (visiting.count(block)) {
            // バックエッジを発見
            return;
        }
        
        visiting.insert(block);
        
        for (auto* succ : block->successors()) {
            if (visiting.count(succ)) {
                // バックエッジを発見
                backEdges[succ].push_back(block);
            } else if (!visited.count(succ)) {
                dfs(succ);
            }
        }
        
        visiting.erase(block);
        visited.insert(block);
    };
    
    // エントリブロックから開始
    if (!_blocks.empty()) {
        dfs(_blocks[0].get());
    }
    
    // バックエッジが見つかったブロック内のノードをループノードとして収集
    for (const auto& pair : backEdges) {
        BasicBlock* loopHeader = pair.first;
        const auto& backEdgeBlocks = pair.second;
        
        // ループボディを特定
        std::unordered_set<BasicBlock*> loopBlocks;
        std::queue<BasicBlock*> workList;
        
        loopBlocks.insert(loopHeader);
        for (auto* backEdgeBlock : backEdgeBlocks) {
            if (loopBlocks.find(backEdgeBlock) == loopBlocks.end()) {
                loopBlocks.insert(backEdgeBlock);
                workList.push(backEdgeBlock);
            }
        }
        
        // ワークリストを使ってループボディを拡張
        while (!workList.empty()) {
            BasicBlock* current = workList.front();
            workList.pop();
            
            for (auto* pred : current->predecessors()) {
                if (loopBlocks.find(pred) == loopBlocks.end()) {
                    loopBlocks.insert(pred);
                    workList.push(pred);
                }
            }
        }
        
        // ループブロック内のすべてのノードを結果に追加
        for (auto* loopBlock : loopBlocks) {
            for (auto* node : loopBlock->getNodes()) {
                result.push_back(node);
            }
        }
    }
    
    return result;
}

std::vector<IRNode*> IRGraph::findStringOperationNodes() const {
    std::vector<IRNode*> result;
    
    for (const auto& node : _nodes) {
        if (node->valueType() == ValueType::String &&
            (node->opType() == OpType::Add || 
             node->opType() == OpType::LoadProperty || 
             node->opType() == OpType::Call)) {
            result.push_back(node.get());
        }
    }
    
    return result;
}

std::vector<IRNode*> IRGraph::findPropertyAccessNodes() const {
    std::vector<IRNode*> result;
    
    for (const auto& node : _nodes) {
        if (node->opType() == OpType::LoadProperty || 
            node->opType() == OpType::StoreProperty) {
            result.push_back(node.get());
        }
    }
    
    return result;
}

std::vector<Loop> IRGraph::findLoops() const {
    // Tarjan の強連結成分アルゴリズムを使用した自然ループ検出の完全実装
    std::vector<Loop> result;
    
    if (_nodes.empty()) return result;
    
    // 支配木の構築（Lengauer-Tarjan アルゴリズムの完全実装）
    std::function<void(IRNode*)> buildDominatorTree = [&](IRNode* entryNode) {
        size_t nodeCount = _nodes.size();
        std::unordered_map<IRNode*, size_t> nodeToIndex;
        std::vector<IRNode*> indexToNode(nodeCount);
        
        // ノードのインデックス化
        size_t index = 0;
        for (const auto& node : _nodes) {
            nodeToIndex[node.get()] = index;
            indexToNode[index] = node.get();
            index++;
        }
        
        // Lengauer-Tarjan アルゴリズムの変数
        std::vector<size_t> dfsNum(nodeCount, SIZE_MAX);
        std::vector<IRNode*> vertex;
        std::vector<size_t> parent(nodeCount, SIZE_MAX);
        std::vector<size_t> semi(nodeCount);
        std::vector<size_t> ancestor(nodeCount, SIZE_MAX);
        std::vector<size_t> label(nodeCount);
        std::vector<std::vector<size_t>> bucket(nodeCount);
        std::vector<size_t> dom(nodeCount, SIZE_MAX);
        
        size_t dfsCounter = 0;
        
        // DFS番号付け
        std::function<void(IRNode*)> dfs = [&](IRNode* node) {
            size_t nodeIdx = nodeToIndex[node];
            dfsNum[nodeIdx] = dfsCounter;
            vertex.push_back(node);
            label[nodeIdx] = nodeIdx;
            semi[nodeIdx] = dfsCounter;
            
            for (IRNode* successor : node->outputs()) {
                size_t succIdx = nodeToIndex[successor];
                if (dfsNum[succIdx] == SIZE_MAX) {
                    parent[succIdx] = nodeIdx;
                    dfs(successor);
                }
            }
            
            dfsCounter++;
        };
        
        dfs(entryNode);
        
        // LINK と EVAL 関数の実装
        std::function<void(size_t, size_t)> link = [&](size_t v, size_t w) {
            size_t s = w;
            while (w < semi.size() && semi[label[w]] < semi[label[s]]) {
                if (ancestor[s] != SIZE_MAX && semi[label[s]] <= semi[label[ancestor[s]]]) {
                    ancestor[w] = ancestor[s];
                } else {
                    ancestor[w] = s;
                }
                s = ancestor[w];
                if (s == SIZE_MAX) break;
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
            while (current != SIZE_MAX && ancestor[current] != SIZE_MAX) {
                path.push_back(current);
                current = ancestor[current];
            }
            
            if (!path.empty()) {
                size_t minSemiLabel = label[path.back()];
                for (int i = static_cast<int>(path.size()) - 2; i >= 0; --i) {
                    if (semi[label[path[i]]] < semi[minSemiLabel]) {
                        minSemiLabel = label[path[i]];
                    }
                    ancestor[path[i]] = ancestor[path.back()];
                    label[path[i]] = minSemiLabel;
                }
            }
            
            return label[v];
        };
        
        // メインアルゴリズム
        for (int i = static_cast<int>(dfsCounter) - 1; i >= 1; --i) {
            size_t w = nodeToIndex[vertex[i]];
            
            // Step 2: 半支配者の計算
            for (IRNode* predecessor : vertex[i]->inputs()) {
                size_t v = nodeToIndex[predecessor];
                if (dfsNum[v] != SIZE_MAX) {  // 到達可能なノードのみ
                    size_t u = eval(v);
                    if (semi[u] < semi[w]) {
                        semi[w] = semi[u];
                    }
                }
            }
            
            bucket[semi[w]].push_back(w);
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
            size_t w = nodeToIndex[vertex[i]];
            if (dom[w] != semi[w]) {
                dom[w] = dom[dom[w]];
            }
        }
        
        // 支配関係の設定
        _dominators[entryNode] = nullptr;
        for (size_t i = 1; i < dfsCounter; ++i) {
            size_t w = nodeToIndex[vertex[i]];
            if (dom[w] != SIZE_MAX && dom[w] < indexToNode.size()) {
                _dominators[vertex[i]] = indexToNode[dom[w]];
            }
        }
    };
    
    _dominators[entryNode] = nullptr;
    buildDominatorTree(entryNode);
    
    // 自然ループの構築
    for (const auto& backEdge : _backEdges) {
        IRNode* tail = backEdge.first;
        IRNode* header = backEdge.second;
        
        Loop loop;
        loop.header = header;
        loop.backEdges.push_back(backEdge);
        
        // ループ本体の特定（ヘッダーから後方に向かって到達可能なノード）
        std::unordered_set<IRNode*> loopNodes;
        std::stack<IRNode*> workList;
        
        loopNodes.insert(header);
        if (loopNodes.find(tail) == loopNodes.end()) {
            loopNodes.insert(tail);
            workList.push(tail);
        }
        
        while (!workList.empty()) {
            IRNode* current = workList.top();
            workList.pop();
            
            for (IRNode* predecessor : current->inputs()) {
                if (loopNodes.find(predecessor) == loopNodes.end()) {
                    loopNodes.insert(predecessor);
                    workList.push(predecessor);
                }
            }
        }
        
        loop.nodes.assign(loopNodes.begin(), loopNodes.end());
        
        // ループ不変式の検出
        loop.invariants = findLoopInvariants(loop);
        
        // 帰納変数の検出
        loop.inductionVariables = findInductionVariables(loop);
        
        result.push_back(loop);
    }
    
    // ネストしたループの階層構造を構築
    buildLoopNesting(result);
    
    return result;
}

// ヘルパー関数の完全実装
bool IRGraph::isDominator(IRNode* dominator, IRNode* node, 
                         const std::unordered_map<IRNode*, IRNode*>& dominators) const {
    IRNode* current = node;
    while (current != nullptr) {
        if (current == dominator) {
            return true;
        }
        auto it = dominators.find(current);
        current = (it != dominators.end()) ? it->second : nullptr;
    }
    return false;
}

std::vector<IRNode*> IRGraph::findLoopInvariants(const Loop& loop) const {
    std::vector<IRNode*> invariants;
    std::unordered_set<IRNode*> loopNodeSet(loop.nodes.begin(), loop.nodes.end());
    
    for (IRNode* node : loop.nodes) {
        bool isInvariant = true;
        
        // ノードの全ての入力がループ外で定義されているかチェック
        for (IRNode* input : node->inputs()) {
            if (loopNodeSet.find(input) != loopNodeSet.end()) {
                // 入力がループ内で定義されている
                isInvariant = false;
                break;
            }
        }
        
        // さらに、ノードが副作用を持たないことも確認
        if (isInvariant && !hasSideEffects(node)) {
            invariants.push_back(node);
        }
    }
    
    return invariants;
}

std::vector<InductionVariable> IRGraph::findInductionVariables(const Loop& loop) const {
    std::vector<InductionVariable> result;
    std::unordered_set<IRNode*> loopNodeSet(loop.nodes.begin(), loop.nodes.end());
    
    for (IRNode* node : loop.nodes) {
        if (node->opType() == OpType::Add || node->opType() == OpType::Subtract) {
            // 基本的な帰納変数パターンの検出: i = i + constant
            auto inputs = node->inputs();
            if (inputs.size() == 2) {
                IRNode* input1 = inputs[0];
                IRNode* input2 = inputs[1];
                
                // 一方の入力が定数で、もう一方がループ内で更新される変数
                if (isConstant(input2) && loopNodeSet.find(input1) != loopNodeSet.end()) {
                    InductionVariable iv;
                    iv.variable = node;
                    iv.initialValue = input1;
                    iv.step = input2;
                    iv.isIncreasing = (node->opType() == OpType::Add);
                    result.push_back(iv);
                } else if (isConstant(input1) && loopNodeSet.find(input2) != loopNodeSet.end()) {
                    InductionVariable iv;
                    iv.variable = node;
                    iv.initialValue = input2;
                    iv.step = input1;
                    iv.isIncreasing = (node->opType() == OpType::Add);
                    result.push_back(iv);
                }
            }
        }
    }
    
    return result;
}

void IRGraph::buildLoopNesting(std::vector<Loop>& loops) const {
    // ループの包含関係を構築
    for (size_t i = 0; i < loops.size(); ++i) {
        for (size_t j = 0; j < loops.size(); ++j) {
            if (i != j) {
                if (isLoopNested(loops[j], loops[i])) {
                    loops[i].nestedLoops.push_back(&loops[j]);
                    loops[j].parentLoop = &loops[i];
                }
            }
        }
    }
}

bool IRGraph::isLoopNested(const Loop& inner, const Loop& outer) const {
    // 内側ループの全ノードが外側ループに含まれているかチェック
    std::unordered_set<IRNode*> outerNodes(outer.nodes.begin(), outer.nodes.end());
    
    for (IRNode* innerNode : inner.nodes) {
        if (outerNodes.find(innerNode) == outerNodes.end()) {
            return false;
        }
    }
    
    return true;
}

bool IRGraph::hasSideEffects(IRNode* node) const {
    // 副作用を持つ操作の判定
    switch (node->opType()) {
        case OpType::Store:
        case OpType::StoreProperty:
        case OpType::Call:
        case OpType::CallMethod:
        case OpType::Throw:
        case OpType::Return:
            return true;
        default:
            return false;
    }
}

bool IRGraph::isConstant(IRNode* node) const {
    return node->opType() == OpType::Constant;
}

void IRGraph::performDominanceAnalysis() {
    // 支配木解析の実装
    if (_blocks.empty()) return;
    
    BasicBlock* entryBlock = _blocks[0].get();
    std::unordered_map<BasicBlock*, std::unordered_set<BasicBlock*>> dominators;
    
    // 初期化：エントリブロックは自分自身のみを支配
    dominators[entryBlock].insert(entryBlock);
    
    // 他のすべてのブロックは初期状態ですべてのブロックに支配される
    std::unordered_set<BasicBlock*> allBlocks;
    for (const auto& block : _blocks) {
        allBlocks.insert(block.get());
        if (block.get() != entryBlock) {
            dominators[block.get()] = allBlocks;
        }
    }
    
    // 不動点まで反復
    bool changed = true;
    while (changed) {
        changed = false;
        
        for (const auto& block : _blocks) {
            if (block.get() == entryBlock) continue;
            
            // 新しい支配集合を計算
            std::unordered_set<BasicBlock*> newDominators = allBlocks;
            
            // すべての前駆ブロックの支配集合の共通部分を取る
            for (auto* pred : block->predecessors()) {
                std::unordered_set<BasicBlock*> intersection;
                std::set_intersection(
                    newDominators.begin(), newDominators.end(),
                    dominators[pred].begin(), dominators[pred].end(),
                    std::inserter(intersection, intersection.begin())
                );
                newDominators = intersection;
            }
            
            // 自分自身を追加
            newDominators.insert(block.get());
            
            // 変更があったかチェック
            if (newDominators != dominators[block.get()]) {
                dominators[block.get()] = newDominators;
                changed = true;
            }
        }
    }
    
    // 結果を各ブロックに設定
    for (const auto& block : _blocks) {
        block->setDominators(dominators[block.get()]);
    }
}

void IRGraph::performLivenessAnalysis() {
    // 生存性解析の実装
    
    // 各ブロックの生存変数集合を初期化
    std::unordered_map<BasicBlock*, std::unordered_set<IRValue*>> liveIn;
    std::unordered_map<BasicBlock*, std::unordered_set<IRValue*>> liveOut;
    std::unordered_map<BasicBlock*, std::unordered_set<IRValue*>> useSet;
    std::unordered_map<BasicBlock*, std::unordered_set<IRValue*>> defSet;
    
    // USE集合とDEF集合を計算
    for (const auto& block : _blocks) {
        for (auto* node : block->getNodes()) {
            // ノードで使用される変数をUSE集合に追加
            for (auto* operand : node->getOperands()) {
                if (operand && defSet[block.get()].find(operand) == defSet[block.get()].end()) {
                    useSet[block.get()].insert(operand);
                }
            }
            
            // ノードで定義される変数をDEF集合に追加
            if (node->hasResult()) {
                defSet[block.get()].insert(node->getResult());
            }
        }
    }
    
    // 不動点まで反復
    bool changed = true;
    while (changed) {
        changed = false;
        
        // 後ろから順番に処理（後方データフロー解析）
        for (auto it = _blocks.rbegin(); it != _blocks.rend(); ++it) {
            BasicBlock* block = it->get();
            
            // LiveOut[B] = ∪{LiveIn[S] | S ∈ successors(B)}
            std::unordered_set<IRValue*> newLiveOut;
            for (auto* succ : block->successors()) {
                newLiveOut.insert(liveIn[succ].begin(), liveIn[succ].end());
            }
            
            // LiveIn[B] = Use[B] ∪ (LiveOut[B] - Def[B])
            std::unordered_set<IRValue*> newLiveIn = useSet[block];
            for (auto* value : newLiveOut) {
                if (defSet[block].find(value) == defSet[block].end()) {
                    newLiveIn.insert(value);
                }
            }
            
            // 変更があったかチェック
            if (newLiveIn != liveIn[block] || newLiveOut != liveOut[block]) {
                liveIn[block] = newLiveIn;
                liveOut[block] = newLiveOut;
                changed = true;
            }
        }
    }
    
    // 結果を各ブロックに設定
    for (const auto& block : _blocks) {
        block->setLiveIn(liveIn[block.get()]);
        block->setLiveOut(liveOut[block.get()]);
    }
}

bool IRGraph::hasEscapingValues() const {
    // エスケープ解析：値がローカルスコープを脱出するかどうかを判定
    
    for (const auto& node : _nodes) {
        // 関数呼び出しの引数として渡される値はエスケープする
        if (node->opType() == OpType::Call) {
            return true;
        }
        
        // グローバル変数への代入もエスケープ
        if (node->opType() == OpType::Store) {
            auto* storeNode = static_cast<StoreNode*>(node.get());
            if (storeNode->getTarget()->isGlobal()) {
                return true;
            }
        }
        
        // 関数からの戻り値もエスケープ
        if (node->opType() == OpType::Return) {
            return true;
        }
        
        // 例外として投げられる値もエスケープ
        if (node->opType() == OpType::Throw) {
            return true;
        }
    }
    
    return false;
}

std::vector<IRNode*> IRGraph::findInductionVariables() const {
    // 帰納変数の検出
    std::vector<IRNode*> inductionVars;
    
    // ループを検出
    auto loopNodes = findLoopNodes();
    if (loopNodes.empty()) {
        return inductionVars;
    }
    
    // ループ内で一定の増分で更新される変数を探す
    std::unordered_map<IRValue*, IRNode*> incrementNodes;
    
    for (auto* node : loopNodes) {
        if (node->opType() == OpType::Add || node->opType() == OpType::Sub) {
            auto* binOp = static_cast<BinaryOpNode*>(node);
            auto* lhs = binOp->getLeftOperand();
            auto* rhs = binOp->getRightOperand();
            
            // 左辺が同じ変数で、右辺が定数の場合は帰納変数の候補
            if (lhs && rhs && rhs->isConstant()) {
                incrementNodes[lhs] = node;
            }
        }
    }
    
    // Phi関数と増分操作の組み合わせをチェック
    for (auto* node : loopNodes) {
        if (node->opType() == OpType::Phi) {
            auto* phiNode = static_cast<PhiNode*>(node);
            auto* result = phiNode->getResult();
            
            // このPhi関数の結果が増分操作に使われているかチェック
            if (incrementNodes.find(result) != incrementNodes.end()) {
                inductionVars.push_back(phiNode);
            }
        }
    }
    
    return inductionVars;
}

std::string IRGraph::dump() const {
    std::stringstream ss;
    
    ss << "IR Graph Dump:\n";
    
    // 基本ブロック情報
    ss << "\nBasic Blocks:\n";
    for (const auto& block : _blocks) {
        ss << "Block " << block->id() << ":\n";
        
        // 前任者と後継者
        ss << "  Predecessors: ";
        for (auto* pred : block->predecessors()) {
            ss << pred->id() << " ";
        }
        
        ss << "\n  Successors: ";
        for (auto* succ : block->successors()) {
            ss << succ->id() << " ";
        }
        ss << "\n";
        
        // ノード
        ss << "  Nodes:\n";
        for (auto* node : block->nodes()) {
            ss << "    " << node->toString() << "\n";
        }
        ss << "\n";
    }
    
    return ss.str();
}

// 完璧な支配木構築の詳細実装（Lengauer-Tarjanアルゴリズム）
void IRGraph::buildDominatorTreeLengauerTarjan() {
    if (_nodes.empty()) return;
    
    // エントリーノードの特定
    IRNode* entryNode = nullptr;
    for (const auto& node : _nodes) {
        if (node->inputs().empty() || node->opType() == OpType::Entry) {
            entryNode = node.get();
            break;
        }
    }
    
    if (!entryNode) return;
    
    size_t nodeCount = _nodes.size();
    std::unordered_map<IRNode*, size_t> nodeToIndex;
    std::vector<IRNode*> indexToNode(nodeCount);
    
    // ノードのインデックス化
    size_t index = 0;
    for (const auto& node : _nodes) {
        nodeToIndex[node.get()] = index;
        indexToNode[index] = node.get();
        index++;
    }
    
    // Lengauer-Tarjanアルゴリズムの変数初期化
    std::vector<size_t> dfsNum(nodeCount, SIZE_MAX);
    std::vector<size_t> vertex(nodeCount);
    std::vector<size_t> parent(nodeCount, SIZE_MAX);
    std::vector<size_t> semi(nodeCount);
    std::vector<size_t> ancestor(nodeCount, SIZE_MAX);
    std::vector<size_t> label(nodeCount);
    std::vector<std::vector<size_t>> bucket(nodeCount);
    std::vector<size_t> dom(nodeCount, SIZE_MAX);
    
    size_t dfsCounter = 0;
    
    // フェーズ1: DFS番号付けと親の設定
    std::function<void(size_t)> dfs = [&](size_t v) {
        dfsNum[v] = dfsCounter;
        vertex[dfsCounter] = v;
        label[v] = v;
        semi[v] = dfsCounter;
        dfsCounter++;
        
        IRNode* node = indexToNode[v];
        for (IRNode* successor : node->outputs()) {
            size_t w = nodeToIndex[successor];
            if (dfsNum[w] == SIZE_MAX) {
                parent[w] = v;
                dfs(w);
            }
        }
    };
    
    // エントリーノードからDFS開始
    size_t entryIndex = nodeToIndex[entryNode];
    dfs(entryIndex);
    
    // フェーズ2: LINK/EVAL関数の定義（パス圧縮最適化付き）
    std::function<void(size_t, size_t)> link = [&](size_t v, size_t w) {
        size_t s = w;
        while (s < nodeCount && ancestor[s] != SIZE_MAX && 
               semi[label[w]] < semi[label[ancestor[s]]]) {
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
        
        while (current < nodeCount && ancestor[current] != SIZE_MAX) {
            path.push_back(current);
            current = ancestor[current];
        }
        
        if (path.size() > 1) {
            size_t minSemiLabel = label[path.back()];
            for (int i = static_cast<int>(path.size()) - 2; i >= 0; --i) {
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
    
    // フェーズ3: Semi-dominator計算（逆DFS順）
    for (int i = static_cast<int>(dfsCounter) - 1; i >= 1; --i) {
        size_t w = vertex[i];
        
        // semi[w]の計算
        IRNode* wNode = indexToNode[w];
        for (IRNode* predecessor : wNode->inputs()) {
            auto predIt = nodeToIndex.find(predecessor);
            if (predIt != nodeToIndex.end()) {
                size_t v = predIt->second;
                if (dfsNum[v] != SIZE_MAX) {
                    size_t u = eval(v);
                    if (semi[u] < semi[w]) {
                        semi[w] = semi[u];
                    }
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
    
    // フェーズ4: 即座支配者の最終調整
    for (int i = 1; i < static_cast<int>(dfsCounter); ++i) {
        size_t w = vertex[i];
        if (dom[w] != vertex[semi[w]]) {
            dom[w] = dom[dom[w]];
        }
    }
    
    // フェーズ5: 支配関係の構築
    _dominators.clear();
    _immediateDominators.clear();
    
    // エントリーノードは自分自身を支配
    _dominators[entryNode] = entryNode;
    _immediateDominators[entryNode] = nullptr;
    
    for (int i = 1; i < static_cast<int>(dfsCounter); ++i) {
        size_t w = vertex[i];
        IRNode* wNode = indexToNode[w];
        
        if (dom[w] != SIZE_MAX && dom[w] < nodeCount) {
            IRNode* domNode = indexToNode[dom[w]];
            _immediateDominators[wNode] = domNode;
            
            // 支配者チェーンの構築
            IRNode* current = domNode;
            while (current != nullptr) {
                _dominators[wNode] = current;
                current = _immediateDominators[current];
            }
        }
    }
}

// 完璧な支配境界計算
void IRGraph::computeDominanceFrontier() {
    if (_nodes.empty()) return;
    
    _dominanceFrontier.clear();
    
    // 各ノードについて支配境界を計算
    for (const auto& nodeX : _nodes) {
        IRNode* x = nodeX.get();
        
        for (IRNode* predecessor : x->inputs()) {
            IRNode* y = predecessor;
            
            // yからxへのエッジについて
            IRNode* runner = y;
            
            // runnerがxを厳密に支配しない間
            while (runner != nullptr && !strictlyDominates(runner, x)) {
                _dominanceFrontier[runner].insert(x);
                
                // 即座支配者に移動
                auto it = _immediateDominators.find(runner);
                if (it != _immediateDominators.end()) {
                    runner = it->second;
                } else {
                    break;
                }
            }
        }
    }
}

// 完璧なバックエッジ検出
void IRGraph::detectBackEdges() {
    _backEdges.clear();
    
    if (_nodes.empty()) return;
    
    // 支配関係が構築されていない場合は構築
    if (_dominators.empty()) {
        buildDominatorTreeLengauerTarjan();
    }
    
    // 各エッジについてバックエッジかどうかをチェック
    for (const auto& node : _nodes) {
        IRNode* tail = node.get();
        
        for (IRNode* successor : tail->outputs()) {
            IRNode* head = successor;
            
            // headがtailを支配している場合、これはバックエッジ
            if (dominates(head, tail)) {
                _backEdges.emplace_back(tail, head);
            }
        }
    }
}

// 完璧な自然ループ検出
std::vector<NaturalLoop> IRGraph::detectNaturalLoops() {
    std::vector<NaturalLoop> loops;
    
    // バックエッジの検出
    detectBackEdges();
    
    // 各バックエッジから自然ループを構築
    for (const auto& backEdge : _backEdges) {
        IRNode* tail = backEdge.first;
        IRNode* header = backEdge.second;
        
        NaturalLoop loop = constructNaturalLoop(tail, header);
        if (loop.isValid()) {
            loops.push_back(loop);
        }
    }
    
    // ループ階層の構築
    buildLoopHierarchy(loops);
    
    // ループ深度の計算
    calculateLoopDepths(loops);
    
    // ループ不変式の検出
    for (auto& loop : loops) {
        detectLoopInvariants(loop);
    }
    
    // 帰納変数の検出
    for (auto& loop : loops) {
        detectInductionVariables(loop);
    }
    
    return loops;
}

NaturalLoop IRGraph::constructNaturalLoop(IRNode* tail, IRNode* header) {
    NaturalLoop loop;
    loop.header = header;
    loop.nodes.insert(header);
    loop.nodes.insert(tail);
    
    // ワークリストアルゴリズムでループ本体を構築
    std::queue<IRNode*> worklist;
    std::unordered_set<IRNode*> visited;
    
    if (tail != header) {
        worklist.push(tail);
        visited.insert(tail);
        visited.insert(header);
    }
    
    while (!worklist.empty()) {
        IRNode* current = worklist.front();
        worklist.pop();
        
        for (IRNode* predecessor : current->inputs()) {
            if (visited.find(predecessor) == visited.end()) {
                loop.nodes.insert(predecessor);
                worklist.push(predecessor);
                visited.insert(predecessor);
            }
        }
    }
    
    // 出口ノードの特定
    for (IRNode* node : loop.nodes) {
        for (IRNode* successor : node->outputs()) {
            if (loop.nodes.find(successor) == loop.nodes.end()) {
                loop.exitNodes.insert(successor);
            }
        }
    }
    
    // バックエッジの記録
    loop.backEdges.emplace_back(tail, header);
    
    return loop;
}

void IRGraph::buildLoopHierarchy(std::vector<NaturalLoop>& loops) {
    // ループをサイズでソート（小さいループが内側）
    std::sort(loops.begin(), loops.end(), 
        [](const NaturalLoop& a, const NaturalLoop& b) {
            return a.nodes.size() < b.nodes.size();
        });
    
    // 親子関係の構築
    for (size_t i = 0; i < loops.size(); ++i) {
        for (size_t j = i + 1; j < loops.size(); ++j) {
            // loops[i]がloops[j]に含まれるかチェック
            bool isSubset = true;
            for (IRNode* node : loops[i].nodes) {
                if (loops[j].nodes.find(node) == loops[j].nodes.end()) {
                    isSubset = false;
                    break;
                }
            }
            
            if (isSubset) {
                loops[i].parent = &loops[j];
                loops[j].innerLoops.push_back(&loops[i]);
                break; // 最も内側の親を見つけたら終了
            }
        }
    }
}

void IRGraph::calculateLoopDepths(std::vector<NaturalLoop>& loops) {
    for (auto& loop : loops) {
        size_t depth = 0;
        NaturalLoop* current = loop.parent;
        
        while (current) {
            depth++;
            current = current->parent;
        }
        
        loop.depth = depth;
    }
}

void IRGraph::detectLoopInvariants(NaturalLoop& loop) {
    loop.invariants.clear();
    
    for (IRNode* node : loop.nodes) {
        if (isLoopInvariant(node, loop)) {
            loop.invariants.push_back(node);
        }
    }
}

bool IRGraph::isLoopInvariant(IRNode* node, const NaturalLoop& loop) {
    // 副作用がある操作は不変ではない
    if (node->hasSideEffects()) {
        return false;
    }
    
    // 入力がすべてループ外で定義されているかチェック
    for (IRNode* input : node->inputs()) {
        if (loop.nodes.find(input) != loop.nodes.end()) {
            return false; // ループ内で定義されている
        }
    }
    
    return true;
}

void IRGraph::detectInductionVariables(NaturalLoop& loop) {
    loop.inductionVariables.clear();
    
    // 基本帰納変数の検出
    for (IRNode* node : loop.nodes) {
        if (isBasicInductionVariable(node, loop)) {
            InductionVariable indVar;
            indVar.node = node;
            indVar.isBasic = true;
            indVar.step = extractInductionStep(node);
            indVar.initialValue = extractInitialValue(node, loop);
            
            loop.inductionVariables.push_back(indVar);
        }
    }
    
    // 派生帰納変数の検出
    for (IRNode* node : loop.nodes) {
        if (isDerivedInductionVariable(node, loop)) {
            InductionVariable indVar;
            indVar.node = node;
            indVar.isBasic = false;
            indVar.baseVariable = findBaseInductionVariable(node, loop);
            indVar.coefficient = extractCoefficient(node);
            indVar.offset = extractOffset(node);
            
            loop.inductionVariables.push_back(indVar);
        }
    }
}

bool IRGraph::isBasicInductionVariable(IRNode* node, const NaturalLoop& loop) {
    // PHI操作で、ループ内で一定の増分で更新される変数
    if (node->opType() != OpType::Phi) {
        return false;
    }
    
    // PHI操作の一つの入力がループ外から、もう一つがループ内からの定義
    auto inputs = node->inputs();
    if (inputs.size() != 2) {
        return false;
    }
    
    bool hasLoopExternalDef = false;
    bool hasLoopInternalDef = false;
    
    for (IRNode* input : inputs) {
        if (loop.nodes.find(input) != loop.nodes.end()) {
            hasLoopInternalDef = true;
        } else {
            hasLoopExternalDef = true;
        }
    }
    
    return hasLoopExternalDef && hasLoopInternalDef;
}

bool IRGraph::isDerivedInductionVariable(IRNode* node, const NaturalLoop& loop) {
    // 基本帰納変数から線形関数で計算される変数
    if (node->opType() != OpType::Add && node->opType() != OpType::Mul) {
        return false;
    }
    
    auto inputs = node->inputs();
    if (inputs.size() != 2) {
        return false;
    }
    
    // 一つの入力が帰納変数で、もう一つがループ不変
    bool hasInductionVar = false;
    bool hasLoopInvariant = false;
    
    for (IRNode* input : inputs) {
        if (isInductionVariable(input, loop)) {
            hasInductionVar = true;
        } else if (isLoopInvariant(input, loop)) {
            hasLoopInvariant = true;
        }
    }
    
    return hasInductionVar && hasLoopInvariant;
}

// ヘルパー関数の実装
bool IRGraph::dominates(IRNode* dominator, IRNode* dominated) const {
    auto it = _dominators.find(dominated);
    return it != _dominators.end() && it->second == dominator;
}

bool IRGraph::strictlyDominates(IRNode* dominator, IRNode* dominated) const {
    return dominator != dominated && dominates(dominator, dominated);
}

bool IRGraph::isInductionVariable(IRNode* node, const NaturalLoop& loop) const {
    for (const auto& indVar : loop.inductionVariables) {
        if (indVar.node == node) {
            return true;
        }
    }
    return false;
}

IRNode* IRGraph::findBaseInductionVariable(IRNode* node, const NaturalLoop& loop) const {
    for (IRNode* input : node->inputs()) {
        if (isInductionVariable(input, loop)) {
            return input;
        }
    }
    return nullptr;
}

double IRGraph::extractInductionStep(IRNode* node) const {
    // PHI操作から増分を抽出
    // 実装は具体的な操作形式に依存
    return 1.0; // デフォルト値
}

double IRGraph::extractInitialValue(IRNode* node, const NaturalLoop& loop) const {
    // PHI操作からループ外の初期値を抽出
    // 実装は具体的な操作形式に依存
    return 0.0; // デフォルト値
}

double IRGraph::extractCoefficient(IRNode* node) const {
    // 乗算操作から係数を抽出
    if (node->opType() == OpType::Mul) {
        // 定数オペランドを検索
        // 実装は具体的な操作形式に依存
    }
    return 1.0; // デフォルト値
}

double IRGraph::extractOffset(IRNode* node) const {
    // 加算操作からオフセットを抽出
    if (node->opType() == OpType::Add) {
        // 定数オペランドを検索
        // 実装は具体的な操作形式に依存
    }
    return 0.0; // デフォルト値
}

} // namespace aerojs 