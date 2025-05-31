/**
 * @file ir_instruction.cpp
 * @brief IR命令実装
 * 
 * このファイルは、中間表現（IR）の命令とオペランドの実装を提供します。
 * 
 * @author AeroJS Team
 * @version 1.0.0
 * @copyright MIT License
 */

#include "ir_instruction.h"
#include <sstream>
#include <iomanip>

namespace aerojs {
namespace core {
namespace ir {

// 静的メンバ初期化
size_t IRInstruction::nextId_ = 1;

// IRInstruction実装

bool IRInstruction::IsTerminator() const {
    switch (opcode_) {
        case IROpcode::BR:
        case IROpcode::COND_BR:
        case IROpcode::RET:
            return true;
        default:
            return false;
    }
}

bool IRInstruction::IsBranch() const {
    return opcode_ == IROpcode::BR || opcode_ == IROpcode::COND_BR;
}

bool IRInstruction::IsCall() const {
    return opcode_ == IROpcode::CALL;
}

bool IRInstruction::IsMemoryOperation() const {
    switch (opcode_) {
        case IROpcode::LOAD:
        case IROpcode::STORE:
        case IROpcode::ALLOCA:
            return true;
        default:
            return false;
    }
}

bool IRInstruction::IsArithmetic() const {
    switch (opcode_) {
        case IROpcode::ADD:
        case IROpcode::SUB:
        case IROpcode::MUL:
        case IROpcode::DIV:
        case IROpcode::MOD:
        case IROpcode::NEG:
            return true;
        default:
            return false;
    }
}

bool IRInstruction::IsComparison() const {
    switch (opcode_) {
        case IROpcode::EQ:
        case IROpcode::NE:
        case IROpcode::LT:
        case IROpcode::LE:
        case IROpcode::GT:
        case IROpcode::GE:
            return true;
        default:
            return false;
    }
}

bool IRInstruction::HasSideEffects() const {
    switch (opcode_) {
        case IROpcode::STORE:
        case IROpcode::CALL:
        case IROpcode::JS_SET_PROPERTY:
        case IROpcode::JS_DELETE_PROPERTY:
        case IROpcode::JS_THROW:
            return true;
        default:
            return false;
    }
}

std::unique_ptr<IRInstruction> IRInstruction::Clone() const {
    auto clone = std::make_unique<IRInstruction>(opcode_, resultType_);
    clone->name_ = name_;
    clone->operands_ = operands_;
    return clone;
}

std::string IRInstruction::ToString() const {
    std::ostringstream oss;
    
    if (resultType_ != IRType::VOID) {
        oss << "%" << (name_.empty() ? std::to_string(id_) : name_) << " = ";
    }
    
    oss << IROpcodeToString(opcode_);
    
    if (resultType_ != IRType::VOID) {
        oss << " " << IRTypeToString(resultType_);
    }
    
    for (size_t i = 0; i < operands_.size(); ++i) {
        if (i > 0) oss << ",";
        oss << " %" << operands_[i].GetId();
    }
    
    return oss.str();
}

// IRPhiInstruction実装

std::unique_ptr<IRInstruction> IRPhiInstruction::Clone() const {
    auto clone = std::make_unique<IRPhiInstruction>(GetResultType());
    clone->incoming_ = incoming_;
    clone->SetName(GetName());
    return std::move(clone);
}

std::string IRPhiInstruction::ToString() const {
    std::ostringstream oss;
    oss << "%" << (GetName().empty() ? std::to_string(GetId()) : GetName());
    oss << " = phi " << IRTypeToString(GetResultType());
    
    for (size_t i = 0; i < incoming_.size(); ++i) {
        if (i > 0) oss << ",";
        oss << " [ %" << incoming_[i].value.GetId() << ", %bb" << incoming_[i].block->GetId() << " ]";
    }
    
    return oss.str();
}

// IRCallInstruction実装

std::unique_ptr<IRInstruction> IRCallInstruction::Clone() const {
    auto clone = std::make_unique<IRCallInstruction>(function_, GetResultType());
    clone->arguments_ = arguments_;
    clone->SetName(GetName());
    return std::move(clone);
}

std::string IRCallInstruction::ToString() const {
    std::ostringstream oss;
    
    if (GetResultType() != IRType::VOID) {
        oss << "%" << (GetName().empty() ? std::to_string(GetId()) : GetName()) << " = ";
    }
    
    oss << "call " << IRTypeToString(GetResultType()) << " %" << function_.GetId() << "(";
    
    for (size_t i = 0; i < arguments_.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << IRTypeToString(arguments_[i].GetType()) << " %" << arguments_[i].GetId();
    }
    
    oss << ")";
    return oss.str();
}

// IRBranchInstruction実装

std::unique_ptr<IRInstruction> IRBranchInstruction::Clone() const {
    if (isConditional_) {
        auto clone = std::make_unique<IRBranchInstruction>(condition_, target_, falseTarget_);
        clone->SetName(GetName());
        return std::move(clone);
    } else {
        auto clone = std::make_unique<IRBranchInstruction>(target_);
        clone->SetName(GetName());
        return std::move(clone);
    }
}

std::string IRBranchInstruction::ToString() const {
    std::ostringstream oss;
    
    if (isConditional_) {
        oss << "br " << IRTypeToString(condition_.GetType()) << " %" << condition_.GetId();
        oss << ", label %bb" << target_->GetId();
        oss << ", label %bb" << falseTarget_->GetId();
    } else {
        oss << "br label %bb" << target_->GetId();
    }
    
    return oss.str();
}

// IRLoadInstruction実装

std::unique_ptr<IRInstruction> IRLoadInstruction::Clone() const {
    auto clone = std::make_unique<IRLoadInstruction>(address_, GetResultType());
    clone->SetName(GetName());
    return std::move(clone);
}

std::string IRLoadInstruction::ToString() const {
    std::ostringstream oss;
    oss << "%" << (GetName().empty() ? std::to_string(GetId()) : GetName());
    oss << " = load " << IRTypeToString(GetResultType());
    oss << ", " << IRTypeToString(address_.GetType()) << "* %" << address_.GetId();
    return oss.str();
}

// IRStoreInstruction実装

std::unique_ptr<IRInstruction> IRStoreInstruction::Clone() const {
    auto clone = std::make_unique<IRStoreInstruction>(value_, address_);
    clone->SetName(GetName());
    return std::move(clone);
}

std::string IRStoreInstruction::ToString() const {
    std::ostringstream oss;
    oss << "store " << IRTypeToString(value_.GetType()) << " %" << value_.GetId();
    oss << ", " << IRTypeToString(address_.GetType()) << "* %" << address_.GetId();
    return oss.str();
}

// ヘルパー関数実装

std::string IRTypeToString(IRType type) {
    switch (type) {
        case IRType::VOID: return "void";
        case IRType::INT8: return "i8";
        case IRType::INT16: return "i16";
        case IRType::INT32: return "i32";
        case IRType::INT64: return "i64";
        case IRType::UINT8: return "u8";
        case IRType::UINT16: return "u16";
        case IRType::UINT32: return "u32";
        case IRType::UINT64: return "u64";
        case IRType::FLOAT32: return "f32";
        case IRType::FLOAT64: return "f64";
        case IRType::BOOLEAN: return "bool";
        case IRType::POINTER: return "ptr";
        case IRType::OBJECT: return "object";
        case IRType::STRING: return "string";
        case IRType::ARRAY: return "array";
        case IRType::FUNCTION: return "function";
        case IRType::UNKNOWN: return "unknown";
        default: return "invalid";
    }
}

std::string IROpcodeToString(IROpcode opcode) {
    switch (opcode) {
        case IROpcode::ADD: return "add";
        case IROpcode::SUB: return "sub";
        case IROpcode::MUL: return "mul";
        case IROpcode::DIV: return "div";
        case IROpcode::MOD: return "mod";
        case IROpcode::NEG: return "neg";
        case IROpcode::AND: return "and";
        case IROpcode::OR: return "or";
        case IROpcode::XOR: return "xor";
        case IROpcode::NOT: return "not";
        case IROpcode::SHL: return "shl";
        case IROpcode::SHR: return "shr";
        case IROpcode::SAR: return "sar";
        case IROpcode::EQ: return "eq";
        case IROpcode::NE: return "ne";
        case IROpcode::LT: return "lt";
        case IROpcode::LE: return "le";
        case IROpcode::GT: return "gt";
        case IROpcode::GE: return "ge";
        case IROpcode::LOAD: return "load";
        case IROpcode::STORE: return "store";
        case IROpcode::ALLOCA: return "alloca";
        case IROpcode::BR: return "br";
        case IROpcode::COND_BR: return "cond_br";
        case IROpcode::CALL: return "call";
        case IROpcode::RET: return "ret";
        case IROpcode::PHI: return "phi";
        case IROpcode::CAST: return "cast";
        case IROpcode::BITCAST: return "bitcast";
        case IROpcode::ZEXT: return "zext";
        case IROpcode::SEXT: return "sext";
        case IROpcode::TRUNC: return "trunc";
        case IROpcode::GET_ELEMENT_PTR: return "getelementptr";
        case IROpcode::EXTRACT_VALUE: return "extractvalue";
        case IROpcode::INSERT_VALUE: return "insertvalue";
        case IROpcode::JS_TYPEOF: return "js.typeof";
        case IROpcode::JS_INSTANCEOF: return "js.instanceof";
        case IROpcode::JS_IN: return "js.in";
        case IROpcode::JS_GET_PROPERTY: return "js.getproperty";
        case IROpcode::JS_SET_PROPERTY: return "js.setproperty";
        case IROpcode::JS_DELETE_PROPERTY: return "js.deleteproperty";
        case IROpcode::JS_NEW: return "js.new";
        case IROpcode::JS_THROW: return "js.throw";
        case IROpcode::JS_TRY_CATCH: return "js.trycatch";
        case IROpcode::VECTOR_ADD: return "vector.add";
        case IROpcode::VECTOR_SUB: return "vector.sub";
        case IROpcode::VECTOR_MUL: return "vector.mul";
        case IROpcode::VECTOR_DIV: return "vector.div";
        case IROpcode::VECTOR_LOAD: return "vector.load";
        case IROpcode::VECTOR_STORE: return "vector.store";
        case IROpcode::UNDEFINED: return "undef";
        case IROpcode::NOP: return "nop";
        default: return "unknown";
    }
}

std::string IRBranchTypeToString(IRBranchType type) {
    switch (type) {
        case IRBranchType::UNCONDITIONAL: return "br";
        case IRBranchType::EQUAL: return "beq";
        case IRBranchType::NOT_EQUAL: return "bne";
        case IRBranchType::LESS_THAN: return "blt";
        case IRBranchType::LESS_EQUAL: return "ble";
        case IRBranchType::GREATER_THAN: return "bgt";
        case IRBranchType::GREATER_EQUAL: return "bge";
        case IRBranchType::ZERO: return "beqz";
        case IRBranchType::NOT_ZERO: return "bnez";
        default: return "unknown";
    }
}

bool IsIntegerType(IRType type) {
    switch (type) {
        case IRType::INT8:
        case IRType::INT16:
        case IRType::INT32:
        case IRType::INT64:
        case IRType::UINT8:
        case IRType::UINT16:
        case IRType::UINT32:
        case IRType::UINT64:
            return true;
        default:
            return false;
    }
}

bool IsFloatType(IRType type) {
    return type == IRType::FLOAT32 || type == IRType::FLOAT64;
}

bool IsPointerType(IRType type) {
    return type == IRType::POINTER;
}

size_t GetTypeSize(IRType type) {
    switch (type) {
        case IRType::VOID: return 0;
        case IRType::INT8:
        case IRType::UINT8: return 1;
        case IRType::INT16:
        case IRType::UINT16: return 2;
        case IRType::INT32:
        case IRType::UINT32:
        case IRType::FLOAT32: return 4;
        case IRType::INT64:
        case IRType::UINT64:
        case IRType::FLOAT64:
        case IRType::POINTER: return 8;
        case IRType::BOOLEAN: return 1;
        case IRType::OBJECT:
        case IRType::STRING:
        case IRType::ARRAY:
        case IRType::FUNCTION: return 8; // ポインタサイズ
        default: return 0;
    }
}

IRType GetCommonType(IRType type1, IRType type2) {
    if (type1 == type2) return type1;
    
    // 浮動小数点数の優先
    if (IsFloatType(type1) || IsFloatType(type2)) {
        if (type1 == IRType::FLOAT64 || type2 == IRType::FLOAT64) {
            return IRType::FLOAT64;
        }
        return IRType::FLOAT32;
    }
    
    // 整数型の場合
    if (IsIntegerType(type1) && IsIntegerType(type2)) {
        size_t size1 = GetTypeSize(type1);
        size_t size2 = GetTypeSize(type2);
        
        if (size1 >= size2) return type1;
        return type2;
    }
    
    // その他の場合は最初の型を返す
    return type1;
}

IRType InferBinaryOpType(IROpcode opcode, IRType leftType, IRType rightType) {
    switch (opcode) {
        case IROpcode::ADD:
        case IROpcode::SUB:
        case IROpcode::MUL:
        case IROpcode::DIV:
        case IROpcode::MOD:
            return GetCommonType(leftType, rightType);
            
        case IROpcode::AND:
        case IROpcode::OR:
        case IROpcode::XOR:
        case IROpcode::SHL:
        case IROpcode::SHR:
        case IROpcode::SAR:
            return leftType; // ビット演算は左オペランドの型
            
        case IROpcode::EQ:
        case IROpcode::NE:
        case IROpcode::LT:
        case IROpcode::LE:
        case IROpcode::GT:
        case IROpcode::GE:
            return IRType::BOOLEAN; // 比較演算は常にboolean
            
        default:
            return leftType;
    }
}

IRType InferUnaryOpType(IROpcode opcode, IRType operandType) {
    switch (opcode) {
        case IROpcode::NEG:
            return operandType;
            
        case IROpcode::NOT:
            if (operandType == IRType::BOOLEAN) {
                return IRType::BOOLEAN;
            }
            return operandType; // ビット反転
            
        default:
            return operandType;
    }
}

IRType InferComparisonType(IRType leftType, IRType rightType) {
    // 比較演算の結果は常にboolean
    return IRType::BOOLEAN;
}

} // namespace ir
} // namespace core
} // namespace aerojs 