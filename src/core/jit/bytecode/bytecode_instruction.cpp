#include "bytecode_instruction.h"
#include <stdexcept>
#include <sstream>
#include <unordered_map>

namespace aerojs {
namespace core {

// BytecodeInstruction クラスの実装

BytecodeInstruction::BytecodeInstruction()
    : m_opcode(BytecodeOp::Nop), m_operands{} {
}

BytecodeInstruction::BytecodeInstruction(BytecodeOp opcode)
    : m_opcode(opcode), m_operands{} {
}

BytecodeInstruction::BytecodeInstruction(BytecodeOp opcode, uint32_t op1)
    : m_opcode(opcode), m_operands{op1} {
}

BytecodeInstruction::BytecodeInstruction(BytecodeOp opcode, uint32_t op1, uint32_t op2)
    : m_opcode(opcode), m_operands{op1, op2} {
}

BytecodeInstruction::BytecodeInstruction(BytecodeOp opcode, uint32_t op1, uint32_t op2, uint32_t op3)
    : m_opcode(opcode), m_operands{op1, op2, op3} {
}

uint8_t BytecodeInstruction::GetOperandCount() const {
    return GetOperandCountForOpcode(m_opcode);
}

uint32_t BytecodeInstruction::GetOperand(uint8_t index) const {
    if (index >= m_operands.size() || index >= MAX_OPERANDS) {
        throw std::out_of_range("Operand index out of range");
    }
    return m_operands[index];
}

uint8_t BytecodeInstruction::GetSize() const {
    // サイズ計算: オペコード(1バイト) + オペランド数 × オペランドサイズ(4バイト)
    return 1 + GetOperandCount() * 4;
}

std::string BytecodeInstruction::ToString() const {
    std::stringstream ss;
    ss << GetOpcodeString(m_opcode);

    // 命令の種類に応じて引数を表示
    switch (m_opcode) {
        case BytecodeOp::LoadConst:
        case BytecodeOp::GetLocal:
        case BytecodeOp::SetLocal:
        case BytecodeOp::GetGlobal:
        case BytecodeOp::SetGlobal:
            if (GetOperandCount() > 0) {
                ss << " " << m_operands[0];
            }
            break;
            
        case BytecodeOp::GetProperty:
        case BytecodeOp::SetProperty:
        case BytecodeOp::DeleteProperty:
        case BytecodeOp::GetElement:
        case BytecodeOp::SetElement:
        case BytecodeOp::DeleteElement:
            // これらの命令は通常2つの引数を持つ
            if (GetOperandCount() > 1) {
                ss << " " << m_operands[0] << ", " << m_operands[1];
            }
            break;
            
        case BytecodeOp::Jump:
        case BytecodeOp::JumpIfTrue:
        case BytecodeOp::JumpIfFalse:
            // ジャンプ先のオフセットを表示
            if (GetOperandCount() > 0) {
                ss << " -> " << m_operands[0];
            }
            break;
            
        case BytecodeOp::Call:
            // 関数呼び出しの引数の数を表示
            if (GetOperandCount() > 0) {
                ss << " (args: " << m_operands[0] << ")";
            }
            break;
            
        case BytecodeOp::CreateObject:
        case BytecodeOp::CreateArray:
            // 初期サイズを表示
            if (GetOperandCount() > 0) {
                ss << " (size: " << m_operands[0] << ")";
            }
            break;
            
        default:
            // その他の命令は追加情報なし
            break;
    }
    
    return ss.str();
}

uint8_t BytecodeInstruction::GetOperandCountForOpcode(BytecodeOp opcode) {
    switch (opcode) {
        // 引数なし
        case BytecodeOp::Nop:
        case BytecodeOp::Pop:
        case BytecodeOp::Dup:
        case BytecodeOp::Swap:
        case BytecodeOp::LoadNull:
        case BytecodeOp::LoadUndefined:
        case BytecodeOp::LoadTrue:
        case BytecodeOp::LoadFalse:
        case BytecodeOp::LoadZero:
        case BytecodeOp::LoadOne:
        case BytecodeOp::Add:
        case BytecodeOp::Sub:
        case BytecodeOp::Mul:
        case BytecodeOp::Div:
        case BytecodeOp::Mod:
        case BytecodeOp::Neg:
        case BytecodeOp::Inc:
        case BytecodeOp::Dec:
        case BytecodeOp::BitAnd:
        case BytecodeOp::BitOr:
        case BytecodeOp::BitXor:
        case BytecodeOp::BitNot:
        case BytecodeOp::ShiftLeft:
        case BytecodeOp::ShiftRight:
        case BytecodeOp::ShiftRightUnsigned:
        case BytecodeOp::LogicalAnd:
        case BytecodeOp::LogicalOr:
        case BytecodeOp::LogicalNot:
        case BytecodeOp::Equal:
        case BytecodeOp::NotEqual:
        case BytecodeOp::StrictEqual:
        case BytecodeOp::StrictNotEqual:
        case BytecodeOp::LessThan:
        case BytecodeOp::LessThanOrEqual:
        case BytecodeOp::GreaterThan:
        case BytecodeOp::GreaterThanOrEqual:
        case BytecodeOp::Return:
        case BytecodeOp::Throw:
        case BytecodeOp::TypeOf:
        case BytecodeOp::InstanceOf:
        case BytecodeOp::In:
        case BytecodeOp::DebugBreak:
            return 0;
            
        // 1つの引数
        case BytecodeOp::Push:
        case BytecodeOp::LoadConst:
        case BytecodeOp::GetLocal:
        case BytecodeOp::SetLocal:
        case BytecodeOp::GetGlobal:
        case BytecodeOp::SetGlobal:
        case BytecodeOp::Jump:
        case BytecodeOp::JumpIfTrue:
        case BytecodeOp::JumpIfFalse:
        case BytecodeOp::Call:
        case BytecodeOp::CreateObject:
        case BytecodeOp::CreateArray:
        case BytecodeOp::EnterTry:
        case BytecodeOp::ExitTry:
            return 1;
            
        // 2つの引数
        case BytecodeOp::GetProperty:
        case BytecodeOp::SetProperty:
        case BytecodeOp::DeleteProperty:
        case BytecodeOp::GetElement:
        case BytecodeOp::SetElement:
        case BytecodeOp::DeleteElement:
            return 2;
            
        default:
            return 0;
    }
}

const char* BytecodeInstruction::GetOpcodeString(BytecodeOp opcode) {
    static const std::unordered_map<BytecodeOp, const char*> opcodeStrings = {
        { BytecodeOp::Nop, "NOP" },
        { BytecodeOp::Push, "PUSH" },
        { BytecodeOp::Pop, "POP" },
        { BytecodeOp::Dup, "DUP" },
        { BytecodeOp::Swap, "SWAP" },
        
        { BytecodeOp::LoadConst, "LOAD_CONST" },
        { BytecodeOp::LoadNull, "LOAD_NULL" },
        { BytecodeOp::LoadUndefined, "LOAD_UNDEFINED" },
        { BytecodeOp::LoadTrue, "LOAD_TRUE" },
        { BytecodeOp::LoadFalse, "LOAD_FALSE" },
        { BytecodeOp::LoadZero, "LOAD_ZERO" },
        { BytecodeOp::LoadOne, "LOAD_ONE" },
        
        { BytecodeOp::GetLocal, "GET_LOCAL" },
        { BytecodeOp::SetLocal, "SET_LOCAL" },
        
        { BytecodeOp::GetGlobal, "GET_GLOBAL" },
        { BytecodeOp::SetGlobal, "SET_GLOBAL" },
        
        { BytecodeOp::GetProperty, "GET_PROPERTY" },
        { BytecodeOp::SetProperty, "SET_PROPERTY" },
        { BytecodeOp::DeleteProperty, "DELETE_PROPERTY" },
        
        { BytecodeOp::GetElement, "GET_ELEMENT" },
        { BytecodeOp::SetElement, "SET_ELEMENT" },
        { BytecodeOp::DeleteElement, "DELETE_ELEMENT" },
        
        { BytecodeOp::Add, "ADD" },
        { BytecodeOp::Sub, "SUB" },
        { BytecodeOp::Mul, "MUL" },
        { BytecodeOp::Div, "DIV" },
        { BytecodeOp::Mod, "MOD" },
        { BytecodeOp::Neg, "NEG" },
        { BytecodeOp::Inc, "INC" },
        { BytecodeOp::Dec, "DEC" },
        
        { BytecodeOp::BitAnd, "BIT_AND" },
        { BytecodeOp::BitOr, "BIT_OR" },
        { BytecodeOp::BitXor, "BIT_XOR" },
        { BytecodeOp::BitNot, "BIT_NOT" },
        { BytecodeOp::ShiftLeft, "SHIFT_LEFT" },
        { BytecodeOp::ShiftRight, "SHIFT_RIGHT" },
        { BytecodeOp::ShiftRightUnsigned, "SHIFT_RIGHT_UNSIGNED" },
        
        { BytecodeOp::LogicalAnd, "LOGICAL_AND" },
        { BytecodeOp::LogicalOr, "LOGICAL_OR" },
        { BytecodeOp::LogicalNot, "LOGICAL_NOT" },
        
        { BytecodeOp::Equal, "EQUAL" },
        { BytecodeOp::NotEqual, "NOT_EQUAL" },
        { BytecodeOp::StrictEqual, "STRICT_EQUAL" },
        { BytecodeOp::StrictNotEqual, "STRICT_NOT_EQUAL" },
        { BytecodeOp::LessThan, "LESS_THAN" },
        { BytecodeOp::LessThanOrEqual, "LESS_THAN_OR_EQUAL" },
        { BytecodeOp::GreaterThan, "GREATER_THAN" },
        { BytecodeOp::GreaterThanOrEqual, "GREATER_THAN_OR_EQUAL" },
        
        { BytecodeOp::Jump, "JUMP" },
        { BytecodeOp::JumpIfTrue, "JUMP_IF_TRUE" },
        { BytecodeOp::JumpIfFalse, "JUMP_IF_FALSE" },
        { BytecodeOp::Call, "CALL" },
        { BytecodeOp::Return, "RETURN" },
        
        { BytecodeOp::Throw, "THROW" },
        { BytecodeOp::EnterTry, "ENTER_TRY" },
        { BytecodeOp::ExitTry, "EXIT_TRY" },
        
        { BytecodeOp::CreateObject, "CREATE_OBJECT" },
        { BytecodeOp::CreateArray, "CREATE_ARRAY" },
        
        { BytecodeOp::TypeOf, "TYPE_OF" },
        { BytecodeOp::InstanceOf, "INSTANCE_OF" },
        { BytecodeOp::In, "IN" },
        
        { BytecodeOp::DebugBreak, "DEBUG_BREAK" }
    };
    
    auto it = opcodeStrings.find(opcode);
    if (it != opcodeStrings.end()) {
        return it->second;
    }
    
    return "UNKNOWN";
}

// ExceptionHandler クラスの実装

bool ExceptionHandler::IsInTryBlock(uint32_t offset) const {
    return offset >= m_try_start && offset < m_try_end;
}

} // namespace core
} // namespace aerojs