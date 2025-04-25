#include "ir_generator.h"
#include <cassert>
#include <iostream>
#include <unordered_map>

namespace aerojs {
namespace core {

IRGenerator::IRGenerator()
    : m_irFunction(nullptr)
    , m_nextTempReg(0)
    , m_currentOffset(0)
{
    // 初期化コンストラクタ
}

IRGenerator::~IRGenerator()
{
    // 特に解放処理は不要
}

std::unique_ptr<IRFunction> IRGenerator::Generate(const uint8_t* bytecodes, size_t length)
{
    assert(bytecodes || length == 0);
    
    // 内部状態のリセット
    Reset();
    
    // バイトコードデコーダの初期化
    m_decoder.SetBytecode(bytecodes, length);
    
    // 新しいIR関数の作成
    m_irFunction = std::make_unique<IRFunction>();
    
    // IRビルダーの初期化
    m_irBuilder.SetIRFunction(m_irFunction.get());
    
    // バイトコードをIRに変換
    Bytecode bytecode;
    while (m_decoder.DecodeNext(bytecode)) {
        // 現在のバイトコードオフセットを記録
        m_currentOffset = m_decoder.GetCurrentOffset() - 1;
        
        // バイトコードをIRに変換
        if (!EmitIRForBytecode(bytecode)) {
            std::cerr << "バイトコード変換エラー: オフセット " << m_currentOffset << std::endl;
            return nullptr;
        }
    }
    
    // 解析終了後、最終的なIR関数の検証
    if (!m_irFunction->Validate()) {
        std::cerr << "IR検証エラー" << std::endl;
        return nullptr;
    }
    
    return std::move(m_irFunction);
}

void IRGenerator::Reset()
{
    m_irFunction = nullptr;
    m_offsetToIRMap.clear();
    m_stackRegs.clear();
    m_varRegs.clear();
    m_nextTempReg = 0;
    m_currentOffset = 0;
}

bool IRGenerator::EmitIRForBytecode(const Bytecode& bytecode)
{
    // バイトコードオフセットと対応するIR命令のインデックスのマッピングを更新
    m_offsetToIRMap[m_currentOffset] = m_irFunction->GetInstructions().size();
    
    // バイトコード命令のオペコードに基づいてIR命令を生成
    switch (bytecode.opcode) {
        case BytecodeOpcode::Nop:
            EmitNop();
            break;
            
        case BytecodeOpcode::LoadConst: {
            // 定数値を取得して、LoadConst命令を生成
            int64_t constValue = 0;
            if (bytecode.operandTypes[0] == OperandType::Int32) {
                constValue = static_cast<int64_t>(bytecode.operands[0].i32);
            } else if (bytecode.operandTypes[0] == OperandType::Int64) {
                constValue = bytecode.operands[0].i64;
            } else {
                // サポートされていない定数型
                return false;
            }
            
            uint32_t resultReg = EmitLoadConst(constValue);
            m_stackRegs.push_back(resultReg);
            break;
        }
            
        case BytecodeOpcode::LoadVar: {
            // 変数インデックスを取得して、LoadVar命令を生成
            uint32_t varIndex = 0;
            if (bytecode.operandTypes[0] == OperandType::Uint8) {
                varIndex = bytecode.operands[0].u8;
            } else if (bytecode.operandTypes[0] == OperandType::Uint16) {
                varIndex = bytecode.operands[0].u16;
            } else if (bytecode.operandTypes[0] == OperandType::Uint32) {
                varIndex = bytecode.operands[0].u32;
            } else {
                // サポートされていないインデックス型
                return false;
            }
            
            uint32_t resultReg = EmitLoadVar(varIndex);
            m_stackRegs.push_back(resultReg);
            break;
        }
            
        case BytecodeOpcode::StoreVar: {
            // 変数インデックスを取得
            uint32_t varIndex = 0;
            if (bytecode.operandTypes[0] == OperandType::Uint8) {
                varIndex = bytecode.operands[0].u8;
            } else if (bytecode.operandTypes[0] == OperandType::Uint16) {
                varIndex = bytecode.operands[0].u16;
            } else if (bytecode.operandTypes[0] == OperandType::Uint32) {
                varIndex = bytecode.operands[0].u32;
            } else {
                // サポートされていないインデックス型
                return false;
            }
            
            // スタックから値を取得
            if (m_stackRegs.empty()) {
                // スタックが空
                return false;
            }
            
            uint32_t valueReg = m_stackRegs.back();
            m_stackRegs.pop_back();
            
            EmitStoreVar(varIndex, valueReg);
            break;
        }
            
        case BytecodeOpcode::Add:
        case BytecodeOpcode::Sub:
        case BytecodeOpcode::Mul:
        case BytecodeOpcode::Div: {
            // スタックから2つの値を取得
            if (m_stackRegs.size() < 2) {
                // スタックに十分な値がない
                return false;
            }
            
            uint32_t rightReg = m_stackRegs.back();
            m_stackRegs.pop_back();
            
            uint32_t leftReg = m_stackRegs.back();
            m_stackRegs.pop_back();
            
            // 対応するIRオペコードを選択
            Opcode irOpcode;
            switch (bytecode.opcode) {
                case BytecodeOpcode::Add: irOpcode = Opcode::Add; break;
                case BytecodeOpcode::Sub: irOpcode = Opcode::Sub; break;
                case BytecodeOpcode::Mul: irOpcode = Opcode::Mul; break;
                case BytecodeOpcode::Div: irOpcode = Opcode::Div; break;
                default: return false;
            }
            
            uint32_t resultReg = EmitBinaryOp(irOpcode, leftReg, rightReg);
            m_stackRegs.push_back(resultReg);
            break;
        }
            
        case BytecodeOpcode::Equal:
        case BytecodeOpcode::NotEqual:
        case BytecodeOpcode::LessThan:
        case BytecodeOpcode::LessThanOrEqual:
        case BytecodeOpcode::GreaterThan:
        case BytecodeOpcode::GreaterThanOrEqual: {
            // スタックから2つの値を取得
            if (m_stackRegs.size() < 2) {
                // スタックに十分な値がない
                return false;
            }
            
            uint32_t rightReg = m_stackRegs.back();
            m_stackRegs.pop_back();
            
            uint32_t leftReg = m_stackRegs.back();
            m_stackRegs.pop_back();
            
            // 対応するIRオペコードを選択
            Opcode irOpcode;
            switch (bytecode.opcode) {
                case BytecodeOpcode::Equal: irOpcode = Opcode::Equal; break;
                case BytecodeOpcode::NotEqual: irOpcode = Opcode::NotEqual; break;
                case BytecodeOpcode::LessThan: irOpcode = Opcode::LessThan; break;
                case BytecodeOpcode::LessThanOrEqual: irOpcode = Opcode::LessThanOrEqual; break;
                case BytecodeOpcode::GreaterThan: irOpcode = Opcode::GreaterThan; break;
                case BytecodeOpcode::GreaterThanOrEqual: irOpcode = Opcode::GreaterThanOrEqual; break;
                default: return false;
            }
            
            uint32_t resultReg = EmitCompare(irOpcode, leftReg, rightReg);
            m_stackRegs.push_back(resultReg);
            break;
        }
            
        case BytecodeOpcode::Jump: {
            // ジャンプ先オフセットを取得
            uint32_t targetOffset = 0;
            if (bytecode.operandTypes[0] == OperandType::Uint8) {
                targetOffset = bytecode.operands[0].u8;
            } else if (bytecode.operandTypes[0] == OperandType::Uint16) {
                targetOffset = bytecode.operands[0].u16;
            } else if (bytecode.operandTypes[0] == OperandType::Uint32) {
                targetOffset = bytecode.operands[0].u32;
            } else {
                // サポートされていないオフセット型
                return false;
            }
            
            EmitJump(targetOffset);
            break;
        }
            
        case BytecodeOpcode::JumpIfTrue:
        case BytecodeOpcode::JumpIfFalse: {
            // ジャンプ先オフセットを取得
            uint32_t targetOffset = 0;
            if (bytecode.operandTypes[0] == OperandType::Uint8) {
                targetOffset = bytecode.operands[0].u8;
            } else if (bytecode.operandTypes[0] == OperandType::Uint16) {
                targetOffset = bytecode.operands[0].u16;
            } else if (bytecode.operandTypes[0] == OperandType::Uint32) {
                targetOffset = bytecode.operands[0].u32;
            } else {
                // サポートされていないオフセット型
                return false;
            }
            
            // スタックから条件値を取得
            if (m_stackRegs.empty()) {
                // スタックが空
                return false;
            }
            
            uint32_t condReg = m_stackRegs.back();
            m_stackRegs.pop_back();
            
            // 条件付きジャンプ命令を生成
            EmitCondJump(condReg, targetOffset, bytecode.opcode == BytecodeOpcode::JumpIfTrue);
            break;
        }
            
        case BytecodeOpcode::Call: {
            // 引数の数を取得
            uint32_t argCount = 0;
            if (bytecode.operandTypes[0] == OperandType::Uint8) {
                argCount = bytecode.operands[0].u8;
            } else if (bytecode.operandTypes[0] == OperandType::Uint16) {
                argCount = bytecode.operands[0].u16;
            } else {
                // サポートされていない引数カウント型
                return false;
            }
            
            // スタックから引数と関数を取得
            if (m_stackRegs.size() < argCount + 1) {
                // スタックに十分な値がない
                return false;
            }
            
            std::vector<uint32_t> args;
            for (uint32_t i = 0; i < argCount; ++i) {
                uint32_t argReg = m_stackRegs.back();
                m_stackRegs.pop_back();
                args.push_back(argReg);
            }
            
            // 引数を逆順にする（最後の引数が最初にポップされるため）
            std::reverse(args.begin(), args.end());
            
            // 関数レジスタを取得
            uint32_t funcReg = m_stackRegs.back();
            m_stackRegs.pop_back();
            
            // 関数呼び出し命令を生成
            uint32_t resultReg = EmitCall(funcReg, args);
            m_stackRegs.push_back(resultReg);
            break;
        }
            
        case BytecodeOpcode::Return: {
            // スタックから戻り値を取得（存在する場合）
            uint32_t valueReg = 0xFFFFFFFF;
            if (!m_stackRegs.empty()) {
                valueReg = m_stackRegs.back();
                m_stackRegs.pop_back();
            }
            
            EmitReturn(valueReg);
            break;
        }
            
        default:
            // サポートされていないオペコード
            return false;
    }
    
    return true;
}

void IRGenerator::EmitNop()
{
    m_irBuilder.EmitNop();
}

uint32_t IRGenerator::EmitLoadConst(int64_t value)
{
    uint32_t resultReg = m_regAllocator.AllocateVirtualRegister(RegisterAllocator::RegisterType::General);
    m_irBuilder.EmitLoadConst(resultReg, value);
    return resultReg;
}

uint32_t IRGenerator::EmitLoadVar(uint32_t varIndex)
{
    // 変数レジスタのマッピングを拡張
    if (varIndex >= m_varRegs.size()) {
        m_varRegs.resize(varIndex + 1, 0xFFFFFFFF);
    }
    
    // まだ初期化されていない変数の場合、ゼロをロード
    if (m_varRegs[varIndex] == 0xFFFFFFFF) {
        m_varRegs[varIndex] = EmitLoadConst(0);
    }
    
    // 変数の値を新しいレジスタにコピー
    uint32_t resultReg = m_regAllocator.AllocateVirtualRegister(RegisterAllocator::RegisterType::General);
    m_irBuilder.EmitMove(resultReg, m_varRegs[varIndex]);
    
    return resultReg;
}

void IRGenerator::EmitStoreVar(uint32_t varIndex, uint32_t valueReg)
{
    // 変数レジスタのマッピングを拡張
    if (varIndex >= m_varRegs.size()) {
        m_varRegs.resize(varIndex + 1, 0xFFFFFFFF);
    }
    
    // 変数に値を格納
    if (m_varRegs[varIndex] == 0xFFFFFFFF) {
        m_varRegs[varIndex] = m_regAllocator.AllocateVirtualRegister(RegisterAllocator::RegisterType::General);
    }
    
    m_irBuilder.EmitMove(m_varRegs[varIndex], valueReg);
}

uint32_t IRGenerator::EmitBinaryOp(Opcode opcode, uint32_t leftReg, uint32_t rightReg)
{
    uint32_t resultReg = m_regAllocator.AllocateVirtualRegister(RegisterAllocator::RegisterType::General);
    m_irBuilder.EmitBinaryOp(opcode, resultReg, leftReg, rightReg);
    return resultReg;
}

uint32_t IRGenerator::EmitCompare(Opcode opcode, uint32_t leftReg, uint32_t rightReg)
{
    uint32_t resultReg = m_regAllocator.AllocateVirtualRegister(RegisterAllocator::RegisterType::General);
    m_irBuilder.EmitCompare(opcode, resultReg, leftReg, rightReg);
    return resultReg;
}

void IRGenerator::EmitJump(uint32_t targetOffset)
{
    // ターゲットラベルを取得または作成
    auto it = m_offsetToIRMap.find(targetOffset);
    if (it == m_offsetToIRMap.end()) {
        // まだターゲットオフセットが処理されていない場合、
        // 新しいラベルを作成
        size_t labelId = m_irFunction->CreateLabel();
        m_offsetToIRMap[targetOffset] = labelId;
        m_irBuilder.EmitJump(labelId);
    } else {
        // 既存のラベルにジャンプ
        m_irBuilder.EmitJump(it->second);
    }
}

void IRGenerator::EmitCondJump(uint32_t condReg, uint32_t targetOffset, bool jumpIfTrue)
{
    // ターゲットラベルを取得または作成
    auto it = m_offsetToIRMap.find(targetOffset);
    size_t labelId;
    
    if (it == m_offsetToIRMap.end()) {
        // まだターゲットオフセットが処理されていない場合、
        // 新しいラベルを作成
        labelId = m_irFunction->CreateLabel();
        m_offsetToIRMap[targetOffset] = labelId;
    } else {
        labelId = it->second;
    }
    
    // 条件付きジャンプ命令を生成
    if (jumpIfTrue) {
        m_irBuilder.EmitJumpIfTrue(condReg, labelId);
    } else {
        m_irBuilder.EmitJumpIfFalse(condReg, labelId);
    }
}

uint32_t IRGenerator::EmitCall(uint32_t funcReg, const std::vector<uint32_t>& args)
{
    uint32_t resultReg = m_regAllocator.AllocateVirtualRegister(RegisterAllocator::RegisterType::General);
    m_irBuilder.EmitCall(resultReg, funcReg, args);
    return resultReg;
}

void IRGenerator::EmitReturn(uint32_t valueReg)
{
    if (valueReg == 0xFFFFFFFF) {
        // 戻り値なし
        m_irBuilder.EmitReturn();
    } else {
        // 戻り値あり
        m_irBuilder.EmitReturn(valueReg);
    }
}

} // namespace core
} // namespace aerojs 