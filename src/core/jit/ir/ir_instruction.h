/**
 * @file ir_instruction.h
 * @brief IR命令定義
 * 
 * このファイルは、中間表現（IR）の命令とオペランドを定義します。
 * AeroJSエンジンの最適化とコード生成で使用される基本的なデータ構造です。
 * 
 * @author AeroJS Team
 * @version 1.0.0
 * @copyright MIT License
 */

#ifndef AEROJS_IR_INSTRUCTION_H
#define AEROJS_IR_INSTRUCTION_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <variant>

namespace aerojs {
namespace core {
namespace ir {

// 前方宣言
class IRInstruction;
class IRBasicBlock;
class IRFunction;

// IR値の型
enum class IRType {
    VOID,
    INT8,
    INT16,
    INT32,
    INT64,
    UINT8,
    UINT16,
    UINT32,
    UINT64,
    FLOAT32,
    FLOAT64,
    BOOLEAN,
    POINTER,
    OBJECT,
    STRING,
    ARRAY,
    FUNCTION,
    UNKNOWN
};

// IR命令のオペコード
enum class IROpcode {
    // 算術演算
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    NEG,
    
    // ビット演算
    AND,
    OR,
    XOR,
    NOT,
    SHL,
    SHR,
    SAR,
    
    // 比較演算
    EQ,
    NE,
    LT,
    LE,
    GT,
    GE,
    
    // メモリ操作
    LOAD,
    STORE,
    ALLOCA,
    
    // 制御フロー
    BR,
    COND_BR,
    CALL,
    RET,
    PHI,
    
    // 型変換
    CAST,
    BITCAST,
    ZEXT,
    SEXT,
    TRUNC,
    
    // 配列・オブジェクト操作
    GET_ELEMENT_PTR,
    EXTRACT_VALUE,
    INSERT_VALUE,
    
    // JavaScript固有
    JS_TYPEOF,
    JS_INSTANCEOF,
    JS_IN,
    JS_GET_PROPERTY,
    JS_SET_PROPERTY,
    JS_DELETE_PROPERTY,
    JS_NEW,
    JS_THROW,
    JS_TRY_CATCH,
    
    // SIMD/ベクトル演算
    VECTOR_ADD,
    VECTOR_SUB,
    VECTOR_MUL,
    VECTOR_DIV,
    VECTOR_LOAD,
    VECTOR_STORE,
    
    // その他
    UNDEFINED,
    NOP
};

// 分岐の種類
enum class IRBranchType {
    UNCONDITIONAL,
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    LESS_EQUAL,
    GREATER_THAN,
    GREATER_EQUAL,
    ZERO,
    NOT_ZERO
};

// ベクトル演算のオペコード
enum class VectorOpcode {
    VADD_VV,    // Vector-Vector Add
    VADD_VI,    // Vector-Immediate Add
    VSUB_VV,    // Vector-Vector Subtract
    VMUL_VV,    // Vector-Vector Multiply
    VDIV_VV,    // Vector-Vector Divide
    VAND_VV,    // Vector-Vector AND
    VOR_VV,     // Vector-Vector OR
    VXOR_VV,    // Vector-Vector XOR
    VSLL_VV,    // Vector-Vector Shift Left
    VSRL_VV,    // Vector-Vector Shift Right
    VLOAD,      // Vector Load
    VSTORE,     // Vector Store
    VFADD_VV,   // Vector Float Add
    VFMUL_VV,   // Vector Float Multiply
    VFMADD_VV   // Vector Float Multiply-Add
};

// リロケーションの種類
enum class RelocationType {
    ABSOLUTE,
    RELATIVE,
    PC_RELATIVE,
    GOT_RELATIVE,
    PLT_RELATIVE
};

// IR値クラス
class IRValue {
public:
    enum class Kind {
        INSTRUCTION,
        CONSTANT,
        ARGUMENT,
        BASIC_BLOCK,
        FUNCTION,
        GLOBAL,
        UNDEFINED
    };
    
    IRValue() : kind_(Kind::UNDEFINED), type_(IRType::VOID), id_(0) {}
    IRValue(Kind kind, IRType type, size_t id) : kind_(kind), type_(type), id_(id) {}
    
    // アクセサ
    Kind GetKind() const { return kind_; }
    IRType GetType() const { return type_; }
    size_t GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    
    // セッター
    void SetName(const std::string& name) { name_ = name; }
    void SetType(IRType type) { type_ = type; }
    
    // 型判定
    bool IsInstruction() const { return kind_ == Kind::INSTRUCTION; }
    bool IsConstant() const { return kind_ == Kind::CONSTANT; }
    bool IsArgument() const { return kind_ == Kind::ARGUMENT; }
    bool IsBasicBlock() const { return kind_ == Kind::BASIC_BLOCK; }
    bool IsFunction() const { return kind_ == Kind::FUNCTION; }
    bool IsGlobal() const { return kind_ == Kind::GLOBAL; }
    bool IsUndefined() const { return kind_ == Kind::UNDEFINED; }
    
    // 比較演算子
    bool operator==(const IRValue& other) const {
        return kind_ == other.kind_ && type_ == other.type_ && id_ == other.id_;
    }
    
    bool operator!=(const IRValue& other) const {
        return !(*this == other);
    }
    
    // ハッシュ関数用
    struct Hash {
        size_t operator()(const IRValue& value) const {
            return std::hash<size_t>()(value.id_) ^ 
                   std::hash<int>()(static_cast<int>(value.kind_)) ^
                   std::hash<int>()(static_cast<int>(value.type_));
        }
    };
    
private:
    Kind kind_;
    IRType type_;
    size_t id_;
    std::string name_;
};

// IR定数値
class IRConstant : public IRValue {
public:
    using Value = std::variant<int64_t, uint64_t, double, bool, std::string>;
    
    IRConstant(IRType type, Value value) 
        : IRValue(Kind::CONSTANT, type, 0), value_(value) {}
    
    const Value& GetValue() const { return value_; }
    
    // 型別アクセサ
    int64_t GetIntValue() const { return std::get<int64_t>(value_); }
    uint64_t GetUIntValue() const { return std::get<uint64_t>(value_); }
    double GetFloatValue() const { return std::get<double>(value_); }
    bool GetBoolValue() const { return std::get<bool>(value_); }
    const std::string& GetStringValue() const { return std::get<std::string>(value_); }
    
    // 型判定
    bool IsInteger() const { return std::holds_alternative<int64_t>(value_) || 
                                    std::holds_alternative<uint64_t>(value_); }
    bool IsFloat() const { return std::holds_alternative<double>(value_); }
    bool IsBool() const { return std::holds_alternative<bool>(value_); }
    bool IsString() const { return std::holds_alternative<std::string>(value_); }
    
private:
    Value value_;
};

// IR命令クラス
class IRInstruction {
public:
    IRInstruction(IROpcode opcode, IRType resultType = IRType::VOID)
        : opcode_(opcode), resultType_(resultType), id_(nextId_++), parent_(nullptr) {}
    
    virtual ~IRInstruction() = default;
    
    // アクセサ
    IROpcode GetOpcode() const { return opcode_; }
    IRType GetResultType() const { return resultType_; }
    size_t GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    IRBasicBlock* GetParent() const { return parent_; }
    
    // オペランド管理
    void AddOperand(const IRValue& operand) { operands_.push_back(operand); }
    void SetOperand(size_t index, const IRValue& operand) { 
        if (index < operands_.size()) operands_[index] = operand; 
    }
    const IRValue& GetOperand(size_t index) const { return operands_[index]; }
    size_t GetOperandCount() const { return operands_.size(); }
    const std::vector<IRValue>& GetOperands() const { return operands_; }
    
    // セッター
    void SetName(const std::string& name) { name_ = name; }
    void SetParent(IRBasicBlock* parent) { parent_ = parent; }
    void SetResultType(IRType type) { resultType_ = type; }
    
    // 型判定
    bool IsTerminator() const;
    bool IsBranch() const;
    bool IsCall() const;
    bool IsMemoryOperation() const;
    bool IsArithmetic() const;
    bool IsComparison() const;
    bool HasSideEffects() const;
    
    // クローン
    virtual std::unique_ptr<IRInstruction> Clone() const;
    
    // 文字列表現
    virtual std::string ToString() const;
    
    // 結果値として使用
    IRValue AsValue() const {
        return IRValue(IRValue::Kind::INSTRUCTION, resultType_, id_);
    }
    
protected:
    IROpcode opcode_;
    IRType resultType_;
    size_t id_;
    std::string name_;
    std::vector<IRValue> operands_;
    IRBasicBlock* parent_;
    
    static size_t nextId_;
};

// 特殊命令クラス

// PHI命令
class IRPhiInstruction : public IRInstruction {
public:
    struct PhiPair {
        IRValue value;
        IRBasicBlock* block;
    };
    
    IRPhiInstruction(IRType type) : IRInstruction(IROpcode::PHI, type) {}
    
    void AddIncoming(const IRValue& value, IRBasicBlock* block) {
        incoming_.push_back({value, block});
    }
    
    const std::vector<PhiPair>& GetIncoming() const { return incoming_; }
    
    std::unique_ptr<IRInstruction> Clone() const override;
    std::string ToString() const override;
    
private:
    std::vector<PhiPair> incoming_;
};

// 関数呼び出し命令
class IRCallInstruction : public IRInstruction {
public:
    IRCallInstruction(const IRValue& function, IRType returnType)
        : IRInstruction(IROpcode::CALL, returnType), function_(function) {}
    
    const IRValue& GetFunction() const { return function_; }
    void SetFunction(const IRValue& function) { function_ = function; }
    
    void AddArgument(const IRValue& arg) { arguments_.push_back(arg); }
    const std::vector<IRValue>& GetArguments() const { return arguments_; }
    
    std::unique_ptr<IRInstruction> Clone() const override;
    std::string ToString() const override;
    
private:
    IRValue function_;
    std::vector<IRValue> arguments_;
};

// 分岐命令
class IRBranchInstruction : public IRInstruction {
public:
    // 無条件分岐
    IRBranchInstruction(IRBasicBlock* target)
        : IRInstruction(IROpcode::BR), target_(target), isConditional_(false) {}
    
    // 条件分岐
    IRBranchInstruction(const IRValue& condition, IRBasicBlock* trueTarget, 
                       IRBasicBlock* falseTarget)
        : IRInstruction(IROpcode::COND_BR), condition_(condition),
          target_(trueTarget), falseTarget_(falseTarget), isConditional_(true) {}
    
    bool IsConditional() const { return isConditional_; }
    const IRValue& GetCondition() const { return condition_; }
    IRBasicBlock* GetTarget() const { return target_; }
    IRBasicBlock* GetFalseTarget() const { return falseTarget_; }
    
    std::unique_ptr<IRInstruction> Clone() const override;
    std::string ToString() const override;
    
private:
    IRValue condition_;
    IRBasicBlock* target_;
    IRBasicBlock* falseTarget_;
    bool isConditional_;
};

// ロード命令
class IRLoadInstruction : public IRInstruction {
public:
    IRLoadInstruction(const IRValue& address, IRType type)
        : IRInstruction(IROpcode::LOAD, type), address_(address) {}
    
    const IRValue& GetAddress() const { return address_; }
    
    std::unique_ptr<IRInstruction> Clone() const override;
    std::string ToString() const override;
    
private:
    IRValue address_;
};

// ストア命令
class IRStoreInstruction : public IRInstruction {
public:
    IRStoreInstruction(const IRValue& value, const IRValue& address)
        : IRInstruction(IROpcode::STORE), value_(value), address_(address) {}
    
    const IRValue& GetValue() const { return value_; }
    const IRValue& GetAddress() const { return address_; }
    
    std::unique_ptr<IRInstruction> Clone() const override;
    std::string ToString() const override;
    
private:
    IRValue value_;
    IRValue address_;
};

// ヘルパー関数

std::string IRTypeToString(IRType type);
std::string IROpcodeToString(IROpcode opcode);
std::string IRBranchTypeToString(IRBranchType type);
bool IsIntegerType(IRType type);
bool IsFloatType(IRType type);
bool IsPointerType(IRType type);
size_t GetTypeSize(IRType type);
IRType GetCommonType(IRType type1, IRType type2);

// 型推論
IRType InferBinaryOpType(IROpcode opcode, IRType leftType, IRType rightType);
IRType InferUnaryOpType(IROpcode opcode, IRType operandType);
IRType InferComparisonType(IRType leftType, IRType rightType);

} // namespace ir
} // namespace core
} // namespace aerojs

// ハッシュ特殊化
namespace std {
    template<>
    struct hash<aerojs::core::ir::IRValue> {
        size_t operator()(const aerojs::core::ir::IRValue& value) const {
            return aerojs::core::ir::IRValue::Hash{}(value);
        }
    };
}

#endif // AEROJS_IR_INSTRUCTION_H 