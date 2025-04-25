#include "bytecode_decoder.h"
#include <cassert>
#include <cstring>

namespace aerojs {
namespace core {

BytecodeDecoder::BytecodeDecoder()
    : m_bytecodes(nullptr)
    , m_bytecodeLength(0)
    , m_currentOffset(0)
{
    // 初期化コンストラクタ
}

BytecodeDecoder::~BytecodeDecoder()
{
    // 特に何もする必要なし
}

void BytecodeDecoder::SetBytecode(const uint8_t* bytecodes, size_t length)
{
    assert(bytecodes || length == 0);
    
    m_bytecodes = bytecodes;
    m_bytecodeLength = length;
    m_currentOffset = 0;
}

bool BytecodeDecoder::DecodeNext(Bytecode& outBytecode)
{
    if (!m_bytecodes || m_currentOffset >= m_bytecodeLength) {
        return false;
    }
    
    // オペコードを読み取る
    uint8_t opcodeValue = m_bytecodes[m_currentOffset++];
    if (opcodeValue >= static_cast<uint8_t>(BytecodeOpcode::Count)) {
        outBytecode.opcode = BytecodeOpcode::Invalid;
        return false;
    }
    
    BytecodeOpcode opcode = static_cast<BytecodeOpcode>(opcodeValue);
    outBytecode.opcode = opcode;
    
    // オペランド数を取得
    size_t operandCount = GetOperandCount(opcode);
    outBytecode.operandCount = operandCount;
    
    // オペランドタイプを取得
    OperandTypes operandTypes = GetOperandTypes(opcode);
    
    // 各オペランドを読み取る
    for (size_t i = 0; i < operandCount; ++i) {
        if (m_currentOffset >= m_bytecodeLength) {
            return false;  // バイトコードが途中で終了
        }
        
        OperandType operandType = GetOperandType(operandTypes, i);
        size_t operandSize = GetOperandSize(operandType);
        
        if (m_currentOffset + operandSize > m_bytecodeLength) {
            return false;  // バイトコードが途中で終了
        }
        
        outBytecode.operands[i] = ReadOperand(operandType, m_currentOffset);
        m_currentOffset += operandSize;
    }
    
    return true;
}

void BytecodeDecoder::SeekTo(size_t offset)
{
    assert(offset <= m_bytecodeLength);
    m_currentOffset = offset;
}

size_t BytecodeDecoder::CurrentOffset() const
{
    return m_currentOffset;
}

void BytecodeDecoder::Reset()
{
    m_bytecodes = nullptr;
    m_bytecodeLength = 0;
    m_currentOffset = 0;
}

size_t BytecodeDecoder::GetOperandCount(BytecodeOpcode opcode) const
{
    // 各オペコードのオペランド数を定義
    switch (opcode) {
        case BytecodeOpcode::Nop:
            return 0;
        case BytecodeOpcode::LoadConst:
        case BytecodeOpcode::LoadVar:
        case BytecodeOpcode::StoreVar:
            return 1;
        case BytecodeOpcode::Add:
        case BytecodeOpcode::Sub:
        case BytecodeOpcode::Mul:
        case BytecodeOpcode::Div:
        case BytecodeOpcode::Equal:
        case BytecodeOpcode::NotEqual:
        case BytecodeOpcode::LessThan:
        case BytecodeOpcode::LessThanOrEqual:
        case BytecodeOpcode::GreaterThan:
        case BytecodeOpcode::GreaterThanOrEqual:
            return 2;
        case BytecodeOpcode::Jump:
        case BytecodeOpcode::JumpIfTrue:
        case BytecodeOpcode::JumpIfFalse:
            return 1;
        case BytecodeOpcode::Call:
            return 2;  // 関数IDと引数の数
        case BytecodeOpcode::Return:
            return 0;
        default:
            return 0;
    }
}

OperandTypes BytecodeDecoder::GetOperandTypes(BytecodeOpcode opcode) const
{
    // 各オペコードのオペランドタイプを定義
    // 各オペランドに対して最大4ビットを使用し、合計16ビットにエンコード
    switch (opcode) {
        case BytecodeOpcode::Nop:
            return EncodeOperandTypes(OperandType::None, OperandType::None, OperandType::None, OperandType::None);
        case BytecodeOpcode::LoadConst:
            return EncodeOperandTypes(OperandType::Uint16, OperandType::None, OperandType::None, OperandType::None);
        case BytecodeOpcode::LoadVar:
            return EncodeOperandTypes(OperandType::Uint8, OperandType::None, OperandType::None, OperandType::None);
        case BytecodeOpcode::StoreVar:
            return EncodeOperandTypes(OperandType::Uint8, OperandType::None, OperandType::None, OperandType::None);
        case BytecodeOpcode::Add:
        case BytecodeOpcode::Sub:
        case BytecodeOpcode::Mul:
        case BytecodeOpcode::Div:
            return EncodeOperandTypes(OperandType::Uint8, OperandType::Uint8, OperandType::None, OperandType::None);
        case BytecodeOpcode::Equal:
        case BytecodeOpcode::NotEqual:
        case BytecodeOpcode::LessThan:
        case BytecodeOpcode::LessThanOrEqual:
        case BytecodeOpcode::GreaterThan:
        case BytecodeOpcode::GreaterThanOrEqual:
            return EncodeOperandTypes(OperandType::Uint8, OperandType::Uint8, OperandType::None, OperandType::None);
        case BytecodeOpcode::Jump:
            return EncodeOperandTypes(OperandType::Uint16, OperandType::None, OperandType::None, OperandType::None);
        case BytecodeOpcode::JumpIfTrue:
        case BytecodeOpcode::JumpIfFalse:
            return EncodeOperandTypes(OperandType::Uint16, OperandType::None, OperandType::None, OperandType::None);
        case BytecodeOpcode::Call:
            return EncodeOperandTypes(OperandType::Uint16, OperandType::Uint8, OperandType::None, OperandType::None);
        case BytecodeOpcode::Return:
            return EncodeOperandTypes(OperandType::None, OperandType::None, OperandType::None, OperandType::None);
        default:
            return EncodeOperandTypes(OperandType::None, OperandType::None, OperandType::None, OperandType::None);
    }
}

OperandType BytecodeDecoder::GetOperandType(OperandTypes types, size_t index) const
{
    assert(index < kMaxBytecodeOperands);
    
    // 各オペランドは4ビットでエンコードされている
    constexpr uint16_t kOperandTypeMask = 0xF;
    
    // インデックスに応じたビットシフトを行う
    uint16_t shiftAmount = index * 4;
    uint16_t operandType = (types >> shiftAmount) & kOperandTypeMask;
    
    return static_cast<OperandType>(operandType);
}

OperandTypes BytecodeDecoder::EncodeOperandTypes(OperandType type0, OperandType type1, OperandType type2, OperandType type3) const
{
    // 各オペランドタイプを4ビットずつシフトして結合
    return static_cast<uint16_t>(type0) |
           (static_cast<uint16_t>(type1) << 4) |
           (static_cast<uint16_t>(type2) << 8) |
           (static_cast<uint16_t>(type3) << 12);
}

uint32_t BytecodeDecoder::GetOperandSize(OperandType type) const
{
    switch (type) {
        case OperandType::None:
            return 0;
        case OperandType::Uint8:
            return 1;
        case OperandType::Uint16:
            return 2;
        case OperandType::Uint32:
            return 4;
        default:
            assert(false && "不明なオペランドタイプ");
            return 0;
    }
}

uint32_t BytecodeDecoder::ReadOperand(OperandType type, size_t offset) const
{
    assert(m_bytecodes);
    assert(offset < m_bytecodeLength);
    
    switch (type) {
        case OperandType::None:
            return 0;
            
        case OperandType::Uint8:
            return static_cast<uint32_t>(m_bytecodes[offset]);
            
        case OperandType::Uint16: {
            uint16_t value = 0;
            std::memcpy(&value, &m_bytecodes[offset], sizeof(uint16_t));
            return static_cast<uint32_t>(value);
        }
            
        case OperandType::Uint32: {
            uint32_t value = 0;
            std::memcpy(&value, &m_bytecodes[offset], sizeof(uint32_t));
            return value;
        }
            
        default:
            assert(false && "不明なオペランドタイプ");
            return 0;
    }
}

} // namespace core
} // namespace aerojs 