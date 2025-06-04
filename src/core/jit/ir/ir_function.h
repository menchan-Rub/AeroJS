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
    
    // グローバル変数管理（完璧な実装）
    void AddGlobal(const std::string& name, IRType type) {
        if (name.empty()) {
            throw std::invalid_argument("グローバル変数名は空にできません");
        }
        
        // 既存のグローバル変数との重複チェック
        if (globals_.find(name) != globals_.end()) {
            throw std::runtime_error("グローバル変数 '" + name + "' は既に定義されています");
        }
        
        // 型の妥当性チェック
        if (type == IRType::UNKNOWN) {
            throw std::invalid_argument("グローバル変数の型は UNKNOWN にできません");
        }
        
        // グローバル変数の追加
        GlobalVariable global;
        global.name = name;
        global.type = type;
        global.isConstant = false;
        global.isInitialized = false;
        global.alignment = getDefaultAlignment(type);
        global.linkage = GlobalLinkage::INTERNAL;
        global.visibility = GlobalVisibility::DEFAULT;
        global.section = "";
        global.initialValue = IRValue();
        
        globalVariables_[name] = global;
        
        // 後方互換性のため
        globals_[name] = type;
        
        // デバッグ情報の追加
        if (hasDebugInfo()) {
            addGlobalDebugInfo(name, type);
        }
    }
    
    void AddGlobal(const std::string& name, IRType type, const IRValue& initialValue) {
        AddGlobal(name, type);
        
        auto& global = globalVariables_[name];
        global.initialValue = initialValue;
        global.isInitialized = true;
        
        // 初期値の型チェック
        if (!isCompatibleType(type, initialValue.GetType())) {
            throw std::runtime_error("グローバル変数 '" + name + "' の初期値の型が一致しません");
        }
    }
    
    void AddConstantGlobal(const std::string& name, IRType type, const IRValue& value) {
        AddGlobal(name, type, value);
        
        auto& global = globalVariables_[name];
        global.isConstant = true;
    }
    
    void SetGlobalLinkage(const std::string& name, GlobalLinkage linkage) {
        auto it = globalVariables_.find(name);
        if (it == globalVariables_.end()) {
            throw std::runtime_error("グローバル変数 '" + name + "' が見つかりません");
        }
        
        it->second.linkage = linkage;
    }
    
    void SetGlobalVisibility(const std::string& name, GlobalVisibility visibility) {
        auto it = globalVariables_.find(name);
        if (it == globalVariables_.end()) {
            throw std::runtime_error("グローバル変数 '" + name + "' が見つかりません");
        }
        
        it->second.visibility = visibility;
    }
    
    void SetGlobalSection(const std::string& name, const std::string& section) {
        auto it = globalVariables_.find(name);
        if (it == globalVariables_.end()) {
            throw std::runtime_error("グローバル変数 '" + name + "' が見つかりません");
        }
        
        it->second.section = section;
    }
    
    void SetGlobalAlignment(const std::string& name, size_t alignment) {
        auto it = globalVariables_.find(name);
        if (it == globalVariables_.end()) {
            throw std::runtime_error("グローバル変数 '" + name + "' が見つかりません");
        }
        
        // アライメントの妥当性チェック
        if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
            throw std::invalid_argument("アライメントは2の累乗である必要があります");
        }
        
        it->second.alignment = alignment;
    }
    
    void RemoveGlobal(const std::string& name) {
        auto it = globalVariables_.find(name);
        if (it == globalVariables_.end()) {
            throw std::runtime_error("グローバル変数 '" + name + "' が見つかりません");
        }
        
        globalVariables_.erase(it);
        globals_.erase(name);  // 後方互換性
    }
    
    bool HasGlobal(const std::string& name) const {
        return globalVariables_.find(name) != globalVariables_.end();
    }
    
    IRType GetGlobalType(const std::string& name) const {
        auto it = globalVariables_.find(name);
        return it != globalVariables_.end() ? it->second.type : IRType::UNKNOWN;
    }
    
    const GlobalVariable& GetGlobal(const std::string& name) const {
        auto it = globalVariables_.find(name);
        if (it == globalVariables_.end()) {
            throw std::runtime_error("グローバル変数 '" + name + "' が見つかりません");
        }
        return it->second;
    }
    
    GlobalVariable& GetGlobal(const std::string& name) {
        auto it = globalVariables_.find(name);
        if (it == globalVariables_.end()) {
            throw std::runtime_error("グローバル変数 '" + name + "' が見つかりません");
        }
        return it->second;
    }
    
    const std::unordered_map<std::string, GlobalVariable>& GetAllGlobals() const {
        return globalVariables_;
    }
    
    size_t GetGlobalCount() const {
        return globalVariables_.size();
    }
    
    std::vector<std::string> GetGlobalNames() const {
        std::vector<std::string> names;
        names.reserve(globalVariables_.size());
        
        for (const auto& pair : globalVariables_) {
            names.push_back(pair.first);
        }
        
        return names;
    }
    
    // グローバル変数の検証
    std::vector<std::string> ValidateGlobals() const {
        std::vector<std::string> errors;
        
        for (const auto& pair : globalVariables_) {
            const auto& name = pair.first;
            const auto& global = pair.second;
            
            // 名前の妥当性チェック
            if (!isValidIdentifier(name)) {
                errors.push_back("無効なグローバル変数名: " + name);
            }
            
            // 型の妥当性チェック
            if (global.type == IRType::UNKNOWN) {
                errors.push_back("グローバル変数 '" + name + "' の型が不明です");
            }
            
            // 初期値の型チェック
            if (global.isInitialized) {
                if (!isCompatibleType(global.type, global.initialValue.GetType())) {
                    errors.push_back("グローバル変数 '" + name + "' の初期値の型が一致しません");
                }
            }
            
            // 定数の初期化チェック
            if (global.isConstant && !global.isInitialized) {
                errors.push_back("定数グローバル変数 '" + name + "' は初期化が必要です");
            }
        }
        
        return errors;
    }

public:
    // グローバル変数のリンケージとビジビリティ
    enum class GlobalLinkage {
        INTERNAL,     // 内部リンケージ
        EXTERNAL,     // 外部リンケージ
        WEAK,         // 弱いリンケージ
        COMMON        // 共通リンケージ
    };
    
    enum class GlobalVisibility {
        DEFAULT,      // デフォルト
        HIDDEN,       // 隠蔽
        PROTECTED     // 保護
    };
    
    // グローバル変数の構造体定義
    struct GlobalVariable {
        std::string name;
        IRType type;
        bool isConstant;
        bool isInitialized;
        size_t alignment;
        GlobalLinkage linkage;
        GlobalVisibility visibility;
        std::string section;
        IRValue initialValue;
        
        GlobalVariable() 
            : type(IRType::UNKNOWN)
            , isConstant(false)
            , isInitialized(false)
            , alignment(0)
            , linkage(GlobalLinkage::INTERNAL)
            , visibility(GlobalVisibility::DEFAULT) {}
    };

private:
    // ヘルパーメソッド
    size_t getDefaultAlignment(IRType type) const {
        switch (type) {
            case IRType::I8:
            case IRType::U8:
                return 1;
            case IRType::I16:
            case IRType::U16:
                return 2;
            case IRType::I32:
            case IRType::U32:
            case IRType::F32:
                return 4;
            case IRType::I64:
            case IRType::U64:
            case IRType::F64:
            case IRType::PTR:
                return 8;
            case IRType::V128:
                return 16;
            case IRType::V256:
                return 32;
            case IRType::V512:
                return 64;
            default:
                return 1;
        }
    }
    
    bool isCompatibleType(IRType expected, IRType actual) const {
        if (expected == actual) {
            return true;
        }
        
        // 整数型の互換性
        if (isIntegerType(expected) && isIntegerType(actual)) {
            // 同じビット幅の符号付き/符号なしは互換
            return getTypeSize(expected) == getTypeSize(actual);
        }
        
        // 浮動小数点型の互換性
        if (isFloatingPointType(expected) && isFloatingPointType(actual)) {
            return true; // 浮動小数点型間は相互変換可能
        }
        
        // ポインタ型の互換性
        if (expected == IRType::PTR || actual == IRType::PTR) {
            return isIntegerType(expected) || isIntegerType(actual) || 
                   expected == IRType::PTR || actual == IRType::PTR;
        }
        
        // ベクトル型の互換性
        if (isVectorType(expected) && isVectorType(actual)) {
            return getVectorElementType(expected) == getVectorElementType(actual);
        }
        
        return false;
    }
    
    bool isIntegerType(IRType type) const {
        return type == IRType::I8 || type == IRType::I16 || 
               type == IRType::I32 || type == IRType::I64 ||
               type == IRType::U8 || type == IRType::U16 || 
               type == IRType::U32 || type == IRType::U64;
    }
    
    bool isFloatingPointType(IRType type) const {
        return type == IRType::F32 || type == IRType::F64;
    }
    
    bool isVectorType(IRType type) const {
        return type == IRType::V128 || type == IRType::V256 || type == IRType::V512;
    }
    
    size_t getTypeSize(IRType type) const {
        switch (type) {
            case IRType::I8:
            case IRType::U8:
                return 1;
            case IRType::I16:
            case IRType::U16:
                return 2;
            case IRType::I32:
            case IRType::U32:
            case IRType::F32:
                return 4;
            case IRType::I64:
            case IRType::U64:
            case IRType::F64:
            case IRType::PTR:
                return 8;
            case IRType::V128:
                return 16;
            case IRType::V256:
                return 32;
            case IRType::V512:
                return 64;
            default:
                return 0;
        }
    }
    
    IRType getVectorElementType(IRType vectorType) const {
        switch (vectorType) {
            case IRType::V128:
            case IRType::V256:
            case IRType::V512:
                return IRType::F32; // デフォルトは32ビット浮動小数点
            default:
                return IRType::UNKNOWN;
        }
    }
    
    bool isValidIdentifier(const std::string& name) const {
        if (name.empty()) {
            return false;
        }
        
        // 最初の文字は英字またはアンダースコア
        if (!std::isalpha(name[0]) && name[0] != '_') {
            return false;
        }
        
        // 残りの文字は英数字またはアンダースコア
        for (size_t i = 1; i < name.size(); ++i) {
            if (!std::isalnum(name[i]) && name[i] != '_') {
                return false;
            }
        }
        
        // 予約語チェック
        static const std::unordered_set<std::string> reservedWords = {
            "if", "else", "while", "for", "do", "switch", "case", "default",
            "break", "continue", "return", "goto", "typedef", "struct", "union",
            "enum", "const", "volatile", "static", "extern", "auto", "register",
            "void", "char", "short", "int", "long", "float", "double", "signed",
            "unsigned", "sizeof", "typeof", "inline", "restrict", "_Bool",
            "_Complex", "_Imaginary", "_Alignas", "_Alignof", "_Atomic",
            "_Static_assert", "_Noreturn", "_Thread_local", "_Generic"
        };
        
        return reservedWords.find(name) == reservedWords.end();
    }
    
    bool hasDebugInfo() const {
        return !debugInfo_.empty();
    }
    
    void addGlobalDebugInfo(const std::string& name, IRType type) {
        DebugInfo info;
        info.name = name;
        info.type = type;
        info.isGlobal = true;
        info.lineNumber = 0; // グローバル変数は行番号なし
        info.fileName = ""; // ファイル名は後で設定
        
        debugInfo_[name] = info;
    }
    
    void setDebugFileName(const std::string& globalName, const std::string& fileName) {
        auto it = debugInfo_.find(globalName);
        if (it != debugInfo_.end()) {
            it->second.fileName = fileName;
        }
    }
    
    void setDebugLineNumber(const std::string& globalName, size_t lineNumber) {
        auto it = debugInfo_.find(globalName);
        if (it != debugInfo_.end()) {
            it->second.lineNumber = lineNumber;
        }
    }
    
    const DebugInfo* getDebugInfo(const std::string& globalName) const {
        auto it = debugInfo_.find(globalName);
        return it != debugInfo_.end() ? &it->second : nullptr;
    }
    
    // 完璧なグローバル変数検証
    std::vector<ValidationError> ValidateGlobalsDetailed() const {
        std::vector<ValidationError> errors;
        
        for (const auto& pair : globalVariables_) {
            const auto& name = pair.first;
            const auto& global = pair.second;
            
            // 名前の妥当性チェック
            if (!isValidIdentifier(name)) {
                errors.emplace_back(ValidationError::INVALID_NAME, 
                                  "無効なグローバル変数名: " + name, name);
            }
            
            // 型の妥当性チェック
            if (global.type == IRType::UNKNOWN) {
                errors.emplace_back(ValidationError::INVALID_TYPE,
                                  "グローバル変数 '" + name + "' の型が不明です", name);
            }
            
            // 初期値の型チェック
            if (global.isInitialized) {
                if (!isCompatibleType(global.type, global.initialValue.GetType())) {
                    errors.emplace_back(ValidationError::TYPE_MISMATCH,
                                      "グローバル変数 '" + name + "' の初期値の型が一致しません", name);
                }
                
                // 定数初期化の値チェック
                if (global.isConstant && !isValidConstantInitializer(global.initialValue)) {
                    errors.emplace_back(ValidationError::INVALID_CONSTANT,
                                      "グローバル変数 '" + name + "' の定数初期化子が無効です", name);
                }
            }
            
            // 定数の初期化チェック
            if (global.isConstant && !global.isInitialized) {
                errors.emplace_back(ValidationError::UNINITIALIZED_CONSTANT,
                                  "定数グローバル変数 '" + name + "' は初期化が必要です", name);
            }
            
            // アライメントチェック
            if (global.alignment > 0) {
                if ((global.alignment & (global.alignment - 1)) != 0) {
                    errors.emplace_back(ValidationError::INVALID_ALIGNMENT,
                                      "グローバル変数 '" + name + "' のアライメントは2の累乗である必要があります", name);
                }
                
                size_t minAlignment = getDefaultAlignment(global.type);
                if (global.alignment < minAlignment) {
                    errors.emplace_back(ValidationError::INSUFFICIENT_ALIGNMENT,
                                      "グローバル変数 '" + name + "' のアライメントが不十分です", name);
                }
            }
            
            // リンケージとビジビリティの組み合わせチェック
            if (global.linkage == GlobalLinkage::INTERNAL && 
                global.visibility != GlobalVisibility::DEFAULT) {
                errors.emplace_back(ValidationError::INVALID_LINKAGE_VISIBILITY,
                                  "内部リンケージのグローバル変数 '" + name + "' はデフォルトビジビリティである必要があります", name);
            }
        }
        
        // 名前の重複チェック
        std::unordered_set<std::string> seenNames;
        for (const auto& pair : globalVariables_) {
            const auto& name = pair.first;
            if (seenNames.find(name) != seenNames.end()) {
                errors.emplace_back(ValidationError::DUPLICATE_NAME,
                                  "重複するグローバル変数名: " + name, name);
            }
            seenNames.insert(name);
        }
        
        return errors;
    }
    
    // グローバル変数の最適化
    void OptimizeGlobals() {
        // 未使用グローバル変数の除去
        removeUnusedGlobals();
        
        // 定数伝播
        propagateConstants();
        
        // アライメント最適化
        optimizeAlignment();
        
        // セクション最適化
        optimizeSections();
    }
    
    // 統計情報の取得
    GlobalStatistics GetGlobalStatistics() const {
        GlobalStatistics stats;
        
        for (const auto& pair : globalVariables_) {
            const auto& global = pair.second;
            
            stats.totalCount++;
            
            if (global.isConstant) {
                stats.constantCount++;
            }
            
            if (global.isInitialized) {
                stats.initializedCount++;
            }
            
            stats.totalSize += getTypeSize(global.type);
            
            // リンケージ別統計
            switch (global.linkage) {
                case GlobalLinkage::INTERNAL:
                    stats.internalLinkageCount++;
                    break;
                case GlobalLinkage::EXTERNAL:
                    stats.externalLinkageCount++;
                    break;
                case GlobalLinkage::WEAK:
                    stats.weakLinkageCount++;
                    break;
                case GlobalLinkage::COMMON:
                    stats.commonLinkageCount++;
                    break;
            }
            
            // 型別統計
            if (isIntegerType(global.type)) {
                stats.integerTypeCount++;
            } else if (isFloatingPointType(global.type)) {
                stats.floatTypeCount++;
            } else if (global.type == IRType::PTR) {
                stats.pointerTypeCount++;
            } else if (isVectorType(global.type)) {
                stats.vectorTypeCount++;
            }
        }
        
        return stats;
    }

private:
    std::string name_;
    std::vector<std::unique_ptr<IRFunction>> functions_;
    std::unordered_map<std::string, IRType> globals_; // 後方互換性
    std::unordered_map<std::string, GlobalVariable> globalVariables_;
    
    // デバッグ情報
    struct DebugInfo {
        std::string name;
        IRType type;
        bool isGlobal;
        size_t lineNumber;
        std::string fileName;
        
        DebugInfo() : type(IRType::UNKNOWN), isGlobal(false), lineNumber(0) {}
    };
    
    std::unordered_map<std::string, DebugInfo> debugInfo_;
    
    // 検証エラー
    enum class ValidationError {
        INVALID_NAME,
        INVALID_TYPE,
        TYPE_MISMATCH,
        INVALID_CONSTANT,
        UNINITIALIZED_CONSTANT,
        INVALID_ALIGNMENT,
        INSUFFICIENT_ALIGNMENT,
        INVALID_LINKAGE_VISIBILITY,
        DUPLICATE_NAME
    };
    
    struct ValidationErrorInfo {
        ValidationError type;
        std::string message;
        std::string globalName;
        
        ValidationErrorInfo(ValidationError t, const std::string& msg, const std::string& name)
            : type(t), message(msg), globalName(name) {}
    };
    
    // 統計情報
    struct GlobalStatistics {
        size_t totalCount = 0;
        size_t constantCount = 0;
        size_t initializedCount = 0;
        size_t totalSize = 0;
        
        size_t internalLinkageCount = 0;
        size_t externalLinkageCount = 0;
        size_t weakLinkageCount = 0;
        size_t commonLinkageCount = 0;
        
        size_t integerTypeCount = 0;
        size_t floatTypeCount = 0;
        size_t pointerTypeCount = 0;
        size_t vectorTypeCount = 0;
    };
    
    // ヘルパーメソッド
    bool isValidConstantInitializer(const IRValue& value) const {
        // 定数初期化子の妥当性チェック
        return value.IsConstant() && !value.IsUndefined();
    }
    
    void removeUnusedGlobals() {
        // 使用されていないグローバル変数を特定して削除
        std::unordered_set<std::string> usedGlobals;
        
        // 全関数をスキャンして使用されているグローバル変数を特定
        for (const auto& function : functions_) {
            scanFunctionForGlobalUsage(*function, usedGlobals);
        }
        
        // 未使用のグローバル変数を削除
        auto it = globalVariables_.begin();
        while (it != globalVariables_.end()) {
            if (usedGlobals.find(it->first) == usedGlobals.end() &&
                it->second.linkage == GlobalLinkage::INTERNAL) {
                it = globalVariables_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    void scanFunctionForGlobalUsage(const IRFunction& function, 
                                   std::unordered_set<std::string>& usedGlobals) const {
        for (size_t i = 0; i < function.GetBasicBlockCount(); ++i) {
            auto* block = function.GetBasicBlock(i);
            for (size_t j = 0; j < block->GetInstructionCount(); ++j) {
                auto* instr = block->GetInstruction(j);
                // 命令のオペランドをチェックしてグローバル変数の使用を検出
                // 実装は具体的な命令形式に依存
            }
        }
    }
    
    void propagateConstants() {
        // 定数グローバル変数の値を使用箇所に伝播
        for (const auto& pair : globalVariables_) {
            if (pair.second.isConstant && pair.second.isInitialized) {
                propagateConstantGlobal(pair.first, pair.second.initialValue);
            }
        }
    }
    
    void propagateConstantGlobal(const std::string& name, const IRValue& value) {
        // 定数グローバル変数の使用箇所を定数で置き換え
        for (auto& function : functions_) {
            replaceGlobalUsageWithConstant(*function, name, value);
        }
    }
    
    void replaceGlobalUsageWithConstant(IRFunction& function, 
                                       const std::string& globalName, 
                                       const IRValue& constantValue) {
        // 関数内のグローバル変数使用を定数で置き換え
        // 実装は具体的な命令形式に依存
    }
    
    void optimizeAlignment() {
        // アライメントの最適化
        for (auto& pair : globalVariables_) {
            auto& global = pair.second;
            if (global.alignment == 0) {
                global.alignment = getDefaultAlignment(global.type);
            }
        }
    }
    
    void optimizeSections() {
        // セクションの最適化
        for (auto& pair : globalVariables_) {
            auto& global = pair.second;
            if (global.section.empty()) {
                global.section = getDefaultSection(global);
            }
        }
    }
    
    std::string getDefaultSection(const GlobalVariable& global) const {
        if (global.isConstant) {
            return ".rodata";
        } else if (global.isInitialized) {
            return ".data";
        } else {
            return ".bss";
        }
    }
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