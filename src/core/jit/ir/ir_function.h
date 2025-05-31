/**
 * @file ir_function.h
 * @brief IR関数定義
 * 
 * このファイルは、中間表現（IR）の関数と基本ブロックを定義します。
 * AeroJSエンジンの最適化とコード生成における制御フロー構造を表現します。
 * 
 * @author AeroJS Team
 * @version 1.0.0
 * @copyright MIT License
 */

#ifndef AEROJS_IR_FUNCTION_H
#define AEROJS_IR_FUNCTION_H

#include "ir_instruction.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace aerojs {
namespace core {
namespace ir {

// 前方宣言
class IRModule;
class IRFunction;

// 関数引数
class IRArgument : public IRValue {
public:
    IRArgument(IRType type, size_t index, const std::string& name = "")
        : IRValue(Kind::ARGUMENT, type, index), index_(index) {
        if (!name.empty()) {
            SetName(name);
        }
    }
    
    size_t GetIndex() const { return index_; }
    
private:
    size_t index_;
};

// 基本ブロック
class IRBasicBlock {
public:
    explicit IRBasicBlock(const std::string& name = "")
        : name_(name), id_(nextId_++), parent_(nullptr) {}
    
    ~IRBasicBlock() = default;
    
    // アクセサ
    const std::string& GetName() const { return name_; }
    size_t GetId() const { return id_; }
    IRFunction* GetParent() const { return parent_; }
    
    // セッター
    void SetName(const std::string& name) { name_ = name; }
    void SetParent(IRFunction* parent) { parent_ = parent; }
    
    // 命令管理
    void AddInstruction(std::unique_ptr<IRInstruction> instruction);
    void InsertInstruction(size_t index, std::unique_ptr<IRInstruction> instruction);
    void RemoveInstruction(size_t index);
    void RemoveInstruction(IRInstruction* instruction);
    
    // 命令アクセス
    IRInstruction* GetInstruction(size_t index) const {
        return index < instructions_.size() ? instructions_[index].get() : nullptr;
    }
    size_t GetInstructionCount() const { return instructions_.size(); }
    
    // イテレータ
    auto begin() { return instructions_.begin(); }
    auto end() { return instructions_.end(); }
    auto begin() const { return instructions_.begin(); }
    auto end() const { return instructions_.end(); }
    
    // 制御フロー
    void AddPredecessor(IRBasicBlock* block) { predecessors_.insert(block); }
    void AddSuccessor(IRBasicBlock* block) { successors_.insert(block); }
    void RemovePredecessor(IRBasicBlock* block) { predecessors_.erase(block); }
    void RemoveSuccessor(IRBasicBlock* block) { successors_.erase(block); }
    
    const std::unordered_set<IRBasicBlock*>& GetPredecessors() const { return predecessors_; }
    const std::unordered_set<IRBasicBlock*>& GetSuccessors() const { return successors_; }
    
    // 判定
    bool IsEmpty() const { return instructions_.empty(); }
    bool HasTerminator() const;
    IRInstruction* GetTerminator() const;
    
    // ループ情報
    bool IsLoopHeader() const { return isLoopHeader_; }
    void SetLoopHeader(bool isHeader) { isLoopHeader_ = isHeader; }
    size_t GetLoopDepth() const { return loopDepth_; }
    void SetLoopDepth(size_t depth) { loopDepth_ = depth; }
    
    // 支配情報
    void AddDominatedBlock(IRBasicBlock* block) { dominated_.insert(block); }
    void SetImmediateDominator(IRBasicBlock* dominator) { immediateDominator_ = dominator; }
    IRBasicBlock* GetImmediateDominator() const { return immediateDominator_; }
    const std::unordered_set<IRBasicBlock*>& GetDominatedBlocks() const { return dominated_; }
    
    // 値として使用
    IRValue AsValue() const {
        return IRValue(IRValue::Kind::BASIC_BLOCK, IRType::VOID, id_);
    }
    
    // 文字列表現
    std::string ToString() const;
    
    // クローン
    std::unique_ptr<IRBasicBlock> Clone() const;
    
private:
    std::string name_;
    size_t id_;
    IRFunction* parent_;
    std::vector<std::unique_ptr<IRInstruction>> instructions_;
    
    // 制御フロー
    std::unordered_set<IRBasicBlock*> predecessors_;
    std::unordered_set<IRBasicBlock*> successors_;
    
    // ループ情報
    bool isLoopHeader_ = false;
    size_t loopDepth_ = 0;
    
    // 支配関係
    IRBasicBlock* immediateDominator_ = nullptr;
    std::unordered_set<IRBasicBlock*> dominated_;
    
    static size_t nextId_;
};

// 関数型情報
struct IRFunctionType {
    IRType returnType;
    std::vector<IRType> parameterTypes;
    bool isVariadic;
    
    IRFunctionType(IRType ret = IRType::VOID, bool variadic = false)
        : returnType(ret), isVariadic(variadic) {}
    
    void AddParameter(IRType type) { parameterTypes.push_back(type); }
    size_t GetParameterCount() const { return parameterTypes.size(); }
    IRType GetParameterType(size_t index) const {
        return index < parameterTypes.size() ? parameterTypes[index] : IRType::UNKNOWN;
    }
    
    std::string ToString() const;
};

// 関数属性
enum class IRFunctionAttribute {
    NONE = 0,
    INLINE = 1 << 0,
    NO_INLINE = 1 << 1,
    PURE = 1 << 2,
    CONST = 1 << 3,
    NO_RETURN = 1 << 4,
    NO_THROW = 1 << 5,
    COLD = 1 << 6,
    HOT = 1 << 7,
    OPTIMIZE_FOR_SIZE = 1 << 8
};

// IR関数
class IRFunction {
public:
    IRFunction(const std::string& name, const IRFunctionType& type)
        : name_(name), type_(type), id_(nextId_++), parent_(nullptr) {
        
        // 引数を作成
        for (size_t i = 0; i < type.GetParameterCount(); ++i) {
            arguments_.emplace_back(std::make_unique<IRArgument>(
                type.GetParameterType(i), i, "arg" + std::to_string(i)));
        }
    }
    
    ~IRFunction() = default;
    
    // アクセサ
    const std::string& GetName() const { return name_; }
    size_t GetId() const { return id_; }
    const IRFunctionType& GetType() const { return type_; }
    IRModule* GetParent() const { return parent_; }
    
    // セッター
    void SetName(const std::string& name) { name_ = name; }
    void SetParent(IRModule* parent) { parent_ = parent; }
    
    // 引数管理
    IRArgument* GetArgument(size_t index) const {
        return index < arguments_.size() ? arguments_[index].get() : nullptr;
    }
    size_t GetArgumentCount() const { return arguments_.size(); }
    const std::vector<std::unique_ptr<IRArgument>>& GetArguments() const { return arguments_; }
    
    // 基本ブロック管理
    IRBasicBlock* CreateBasicBlock(const std::string& name = "");
    void AddBasicBlock(std::unique_ptr<IRBasicBlock> block);
    void RemoveBasicBlock(IRBasicBlock* block);
    void RemoveBasicBlock(size_t index);
    
    IRBasicBlock* GetBasicBlock(size_t index) const {
        return index < basicBlocks_.size() ? basicBlocks_[index].get() : nullptr;
    }
    IRBasicBlock* GetBasicBlock(const std::string& name) const;
    IRBasicBlock* GetEntryBlock() const {
        return !basicBlocks_.empty() ? basicBlocks_[0].get() : nullptr;
    }
    size_t GetBasicBlockCount() const { return basicBlocks_.size(); }
    
    // イテレータ
    auto begin() { return basicBlocks_.begin(); }
    auto end() { return basicBlocks_.end(); }
    auto begin() const { return basicBlocks_.begin(); }
    auto end() const { return basicBlocks_.end(); }
    
    // 関数属性
    void AddAttribute(IRFunctionAttribute attr) { attributes_ |= static_cast<uint32_t>(attr); }
    void RemoveAttribute(IRFunctionAttribute attr) { attributes_ &= ~static_cast<uint32_t>(attr); }
    bool HasAttribute(IRFunctionAttribute attr) const {
        return (attributes_ & static_cast<uint32_t>(attr)) != 0;
    }
    
    // メタデータ
    void SetMetadata(const std::string& key, const std::string& value) {
        metadata_[key] = value;
    }
    std::string GetMetadata(const std::string& key) const {
        auto it = metadata_.find(key);
        return it != metadata_.end() ? it->second : "";
    }
    
    // 制御フロー解析
    void BuildControlFlowGraph();
    void ComputeDominators();
    void DetectLoops();
    
    // 検証
    bool Verify() const;
    std::vector<std::string> GetVerificationErrors() const;
    
    // 統計情報
    size_t GetInstructionCount() const;
    size_t GetVariableCount() const;
    size_t GetMaxLoopDepth() const;
    
    // 値として使用
    IRValue AsValue() const {
        return IRValue(IRValue::Kind::FUNCTION, type_.returnType, id_);
    }
    
    // 文字列表現
    std::string ToString() const;
    
    // クローン
    std::unique_ptr<IRFunction> Clone() const;
    
private:
    // ヘルパーメソッド
    void UpdateBlockConnections();
    void ComputeDominatorsHelper();
    void DetectLoopsHelper();
    
private:
    std::string name_;
    IRFunctionType type_;
    size_t id_;
    IRModule* parent_;
    
    std::vector<std::unique_ptr<IRArgument>> arguments_;
    std::vector<std::unique_ptr<IRBasicBlock>> basicBlocks_;
    
    uint32_t attributes_ = 0;
    std::unordered_map<std::string, std::string> metadata_;
    
    // 解析結果キャッシュ
    mutable bool cfgBuilt_ = false;
    mutable bool dominatorsComputed_ = false;
    mutable bool loopsDetected_ = false;
    
    static size_t nextId_;
};

// IRモジュール（関数のコンテナ）
class IRModule {
public:
    explicit IRModule(const std::string& name = "") : name_(name) {}
    ~IRModule() = default;
    
    // アクセサ
    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }
    
    // 関数管理
    IRFunction* CreateFunction(const std::string& name, const IRFunctionType& type);
    void AddFunction(std::unique_ptr<IRFunction> function);
    void RemoveFunction(const std::string& name);
    void RemoveFunction(IRFunction* function);
    
    IRFunction* GetFunction(const std::string& name) const;
    IRFunction* GetFunction(size_t index) const {
        return index < functions_.size() ? functions_[index].get() : nullptr;
    }
    size_t GetFunctionCount() const { return functions_.size(); }
    
    // イテレータ
    auto begin() { return functions_.begin(); }
    auto end() { return functions_.end(); }
    auto begin() const { return functions_.begin(); }
    auto end() const { return functions_.end(); }
    
    // グローバル変数（簡略化）
    void AddGlobal(const std::string& name, IRType type) {
        globals_[name] = type;
    }
    bool HasGlobal(const std::string& name) const {
        return globals_.find(name) != globals_.end();
    }
    IRType GetGlobalType(const std::string& name) const {
        auto it = globals_.find(name);
        return it != globals_.end() ? it->second : IRType::UNKNOWN;
    }
    
    // 検証
    bool Verify() const;
    std::vector<std::string> GetVerificationErrors() const;
    
    // 文字列表現
    std::string ToString() const;
    
    // クローン
    std::unique_ptr<IRModule> Clone() const;
    
private:
    std::string name_;
    std::vector<std::unique_ptr<IRFunction>> functions_;
    std::unordered_map<std::string, IRType> globals_;
    std::unordered_map<std::string, std::string> metadata_;
};

// ヘルパー関数

// 制御フロー解析
std::vector<IRBasicBlock*> ComputePostOrder(IRFunction* function);
std::vector<IRBasicBlock*> ComputeReversePostOrder(IRFunction* function);
bool IsReachable(IRBasicBlock* from, IRBasicBlock* to);

// 支配関係
std::unordered_map<IRBasicBlock*, IRBasicBlock*> ComputeImmediateDominators(IRFunction* function);
std::unordered_map<IRBasicBlock*, std::unordered_set<IRBasicBlock*>> ComputeDominanceFrontier(IRFunction* function);

// ループ検出
struct LoopInfo {
    IRBasicBlock* header;
    std::unordered_set<IRBasicBlock*> blocks;
    size_t depth;
    std::vector<LoopInfo*> innerLoops;
    LoopInfo* parent;
    
    LoopInfo(IRBasicBlock* h) : header(h), depth(0), parent(nullptr) {}
};

std::vector<std::unique_ptr<LoopInfo>> DetectNaturalLoops(IRFunction* function);

// 関数属性操作
IRFunctionAttribute operator|(IRFunctionAttribute a, IRFunctionAttribute b);
IRFunctionAttribute operator&(IRFunctionAttribute a, IRFunctionAttribute b);
IRFunctionAttribute operator~(IRFunctionAttribute a);

} // namespace ir
} // namespace core
} // namespace aerojs

#endif // AEROJS_IR_FUNCTION_H 