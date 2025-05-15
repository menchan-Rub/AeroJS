// AeroJS JavaScriptエンジン
// 高性能JITコンパイラのための中間表現（IR）グラフ実装

#include "ir_graph.h"
#include <sstream>
#include <cassert>
#include <algorithm>

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
    // 簡易的なハッシュ計算
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
    // ループ解析が必要
    // 単純化のため、バックエッジを持つノードをヒューリスティックに探す
    std::vector<IRNode*> result;
    
    // 詳細な実装は省略
    
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
    // ループ解析アルゴリズム
    // 単純化のため、基本的なループ検出を行う
    std::vector<Loop> result;
    
    // 詳細な実装は省略
    
    return result;
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

} // namespace aerojs 