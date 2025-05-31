#include "ir_builder.h"
#include "../jit_profiler.h"
#include <cassert>
#include <unordered_map>
#include <unordered_set>

namespace aerojs {
namespace core {

IRBuilder::IRBuilder() 
    : m_profiler(nullptr),
      m_enableOptimizations(true),
      m_instructionEmitCallback(nullptr),
      m_typeInferenceCallback(nullptr) {
}

IRBuilder::~IRBuilder() {
}

std::unique_ptr<IRFunction> IRBuilder::BuildFromBytecode(const std::vector<uint8_t>& bytecodes, uint32_t functionId) {
    auto function = std::make_unique<IRFunction>();
    function->SetFunctionId(functionId);
    
    // バイトコードからIRを構築
    BuildBasicBlocks(bytecodes, *function);
    
    // プロファイル情報を使って最適化（プロファイラが設定されている場合）
    if (m_profiler && m_enableOptimizations) {
        OptimizeUsingProfile(*function, functionId);
    }
    
    return function;
}

void IRBuilder::SetProfiler(JITProfiler* profiler) {
    m_profiler = profiler;
}

void IRBuilder::SetInstructionEmitCallback(std::function<void(uint32_t, IROpcode, uint32_t)> callback) {
    m_instructionEmitCallback = std::move(callback);
}

void IRBuilder::SetTypeInferenceCallback(std::function<void(uint32_t, uint32_t, IRType)> callback) {
    m_typeInferenceCallback = std::move(callback);
}

void IRBuilder::EnableOptimizations(bool enable) {
    m_enableOptimizations = enable;
}

void IRBuilder::BuildBasicBlocks(const std::vector<uint8_t>& bytecodes, IRFunction& function) {
    // バイトコードを解析して基本ブロックを構築
    // 各バイトコード命令をIR命令に変換
    
    // バイトコードオフセットごとに処理
    for (size_t offset = 0; offset < bytecodes.size();) {
        // バイトコード命令の解析とIR命令への変換
        EmitIRForBytecode(bytecodes.data(), offset, function);
        
        // バイトコードのサイズに応じてオフセットを進める
        // 可変長命令に対応
        uint8_t opcode = bytecodes[offset];
        size_t instructionSize = GetBytecodeInstructionSize(opcode, bytecodes.data() + offset);
        offset += instructionSize;
    }
}

size_t IRBuilder::GetBytecodeInstructionSize(uint8_t opcode, const uint8_t* bytecode) const {
    // 各バイトコード命令のサイズを取得
    // バイトコードの先頭1バイトはオペコード、残りはオペランド
    BytecodeOpcode op = static_cast<BytecodeOpcode>(opcode);
    
    // オペコードに基づいて命令サイズを返す
    switch (op) {
        // 固定サイズの命令（オペコードのみ）
        case BytecodeOpcode::Return:
        case BytecodeOpcode::BitNot:
        case BytecodeOpcode::Not:
        case BytecodeOpcode::Neg:
            return 1;
            
        // オペコード + 1バイトオペランド命令
        case BytecodeOpcode::Inc:
        case BytecodeOpcode::Dec:
            return 2;
            
        // オペコード + 2バイトオペランド命令
        case BytecodeOpcode::LoadVar:
        case BytecodeOpcode::StoreVar:
        case BytecodeOpcode::Move:
            return 3;
            
        // オペコード + 4バイトオペランド命令
        case BytecodeOpcode::LoadConst:
        case BytecodeOpcode::Jump:
        case BytecodeOpcode::JumpIfTrue:
        case BytecodeOpcode::JumpIfFalse:
            return 5;
            
        // 複数オペランドを持つ命令
        case BytecodeOpcode::Add:
        case BytecodeOpcode::Sub:
        case BytecodeOpcode::Mul:
        case BytecodeOpcode::Div:
        case BytecodeOpcode::Mod:
        case BytecodeOpcode::BitAnd:
        case BytecodeOpcode::BitOr:
        case BytecodeOpcode::BitXor:
        case BytecodeOpcode::Shl:
        case BytecodeOpcode::Shr:
        case BytecodeOpcode::UShr:
        case BytecodeOpcode::Eq:
        case BytecodeOpcode::Ne:
        case BytecodeOpcode::Lt:
        case BytecodeOpcode::Le:
        case BytecodeOpcode::Gt:
        case BytecodeOpcode::Ge:
        case BytecodeOpcode::And:
        case BytecodeOpcode::Or:
            return 4; // オペコード + 3つのオペランド（dest, src1, src2）
            
        // Callは可変長：オペコード + 関数インデックス + 引数カウント + 引数リスト
        case BytecodeOpcode::Call: {
            // 最初の2バイトは関数インデックス
            uint16_t functionIndex = bytecode[1] | (static_cast<uint16_t>(bytecode[2]) << 8);
            // 3バイト目は引数カウント
            uint8_t argCount = bytecode[3];
            // オペコード + 関数インデックス(2) + 引数カウント(1) + 引数(argCount)
            return 1 + 2 + 1 + argCount;
        }
            
        // デフォルトは安全のため固定サイズを返す
        default:
            return 4;
    }
}

void IRBuilder::EmitIRForBytecode(const uint8_t* bytecode, size_t offset, IRFunction& function) {
    // バイトコード命令を解析してIR命令を生成
    BytecodeOpcode opcode = static_cast<BytecodeOpcode>(bytecode[offset]);
    IROpcode irOpcode = MapBytecodeToIROpcode(opcode);
    
    // IR命令を生成
    IRInstruction inst(irOpcode);
    
    // オペコードに基づいてオペランドを設定
    switch (opcode) {
        case BytecodeOpcode::LoadConst: {
            // 定数インデックスを読み取り
            uint32_t constIndex = bytecode[offset+1] | 
                                (static_cast<uint32_t>(bytecode[offset+2]) << 8) |
                                (static_cast<uint32_t>(bytecode[offset+3]) << 16) |
                                (static_cast<uint32_t>(bytecode[offset+4]) << 24);
            inst.SetOperand(0, constIndex);
            break;
        }
            
        case BytecodeOpcode::LoadVar:
        case BytecodeOpcode::StoreVar: {
            // 変数インデックスを読み取り
            uint16_t varIndex = bytecode[offset+1] | (static_cast<uint16_t>(bytecode[offset+2]) << 8);
            inst.SetOperand(0, varIndex);
            break;
        }
            
        case BytecodeOpcode::Move: {
            // 移動元と移動先のインデックスを読み取り
            uint8_t dest = bytecode[offset+1];
            uint8_t src = bytecode[offset+2];
            inst.SetOperand(0, dest);
            inst.SetOperand(1, src);
            break;
        }
            
        case BytecodeOpcode::Jump:
        case BytecodeOpcode::JumpIfTrue:
        case BytecodeOpcode::JumpIfFalse: {
            // ジャンプ先のオフセットを読み取り
            uint32_t jumpOffset = bytecode[offset+1] | 
                                 (static_cast<uint32_t>(bytecode[offset+2]) << 8) |
                                 (static_cast<uint32_t>(bytecode[offset+3]) << 16) |
                                 (static_cast<uint32_t>(bytecode[offset+4]) << 24);
            inst.SetOperand(0, jumpOffset);
            break;
        }
            
        case BytecodeOpcode::Call: {
            // 関数インデックスを読み取り
            uint16_t functionIndex = bytecode[offset+1] | (static_cast<uint16_t>(bytecode[offset+2]) << 8);
            // 引数カウントを読み取り
            uint8_t argCount = bytecode[offset+3];
            
            inst.SetOperand(0, functionIndex);
            inst.SetOperand(1, argCount);
            
            // 引数インデックスを読み取り
            for (uint8_t i = 0; i < argCount && i < IRInstruction::MAX_OPERANDS - 2; i++) {
                inst.SetOperand(i + 2, bytecode[offset + 4 + i]);
            }
            break;
        }
            
        case BytecodeOpcode::Add:
        case BytecodeOpcode::Sub:
        case BytecodeOpcode::Mul:
        case BytecodeOpcode::Div:
        case BytecodeOpcode::Mod:
        case BytecodeOpcode::BitAnd:
        case BytecodeOpcode::BitOr:
        case BytecodeOpcode::BitXor:
        case BytecodeOpcode::Shl:
        case BytecodeOpcode::Shr:
        case BytecodeOpcode::UShr:
        case BytecodeOpcode::Eq:
        case BytecodeOpcode::Ne:
        case BytecodeOpcode::Lt:
        case BytecodeOpcode::Le:
        case BytecodeOpcode::Gt:
        case BytecodeOpcode::Ge:
        case BytecodeOpcode::And:
        case BytecodeOpcode::Or: {
            // 演算結果の格納先と2つのオペランドを読み取り
            uint8_t dest = bytecode[offset+1];
            uint8_t src1 = bytecode[offset+2];
            uint8_t src2 = bytecode[offset+3];
            
            inst.SetOperand(0, dest);
            inst.SetOperand(1, src1);
            inst.SetOperand(2, src2);
            break;
        }
            
        case BytecodeOpcode::BitNot:
        case BytecodeOpcode::Not:
        case BytecodeOpcode::Neg: {
            // 単項演算の結果格納先とオペランドを読み取り
            uint8_t dest = bytecode[offset+1];
            uint8_t src = bytecode[offset+2];
            
            inst.SetOperand(0, dest);
            inst.SetOperand(1, src);
            break;
        }
            
        case BytecodeOpcode::Inc:
        case BytecodeOpcode::Dec: {
            // インクリメント/デクリメント対象の変数インデックスを読み取り
            uint8_t varIndex = bytecode[offset+1];
            inst.SetOperand(0, varIndex);
            inst.SetOperand(1, varIndex);  // ソースも同じ
            inst.SetOperand(2, 1);         // Inc/Decは常に1を加減算
            break;
        }
            
        default:
            // その他の命令は何もしない
            break;
    }
    
    // 命令を関数に追加
    function.AddInstruction(inst);
    
    // 型推論を行う
    IRType type = InferType(inst);
    
    // 型推論コールバックを呼び出す
    if (m_typeInferenceCallback) {
        uint32_t functionId = function.GetFunctionId();
        uint32_t varIndex = inst.GetDest();
        m_typeInferenceCallback(functionId, varIndex, type);
    }
    
    // 命令エミットコールバックを呼び出す
    if (m_instructionEmitCallback) {
        uint32_t functionId = function.GetFunctionId();
        uint32_t bytecodeOffset = static_cast<uint32_t>(offset);
        m_instructionEmitCallback(functionId, irOpcode, bytecodeOffset);
    }
}

IROpcode IRBuilder::MapBytecodeToIROpcode(BytecodeOpcode bytecodeOp) {
    // バイトコード命令をIR命令に対応付け
    switch (bytecodeOp) {
        case BytecodeOpcode::Add:
            return IROpcode::Add;
        case BytecodeOpcode::Sub:
            return IROpcode::Sub;
        case BytecodeOpcode::Mul:
            return IROpcode::Mul;
        case BytecodeOpcode::Div:
            return IROpcode::Div;
        case BytecodeOpcode::Mod:
            return IROpcode::Mod;
        case BytecodeOpcode::BitAnd:
            return IROpcode::BitAnd;
        case BytecodeOpcode::BitOr:
            return IROpcode::BitOr;
        case BytecodeOpcode::BitXor:
            return IROpcode::BitXor;
        case BytecodeOpcode::Shl:
            return IROpcode::Shl;
        case BytecodeOpcode::Shr:
            return IROpcode::Shr;
        case BytecodeOpcode::UShr:
            return IROpcode::UShr;
        case BytecodeOpcode::Eq:
            return IROpcode::Eq;
        case BytecodeOpcode::Ne:
            return IROpcode::Ne;
        case BytecodeOpcode::Lt:
            return IROpcode::Lt;
        case BytecodeOpcode::Le:
            return IROpcode::Le;
        case BytecodeOpcode::Gt:
            return IROpcode::Gt;
        case BytecodeOpcode::Ge:
            return IROpcode::Ge;
        case BytecodeOpcode::And:
            return IROpcode::And;
        case BytecodeOpcode::Or:
            return IROpcode::Or;
        case BytecodeOpcode::Not:
            return IROpcode::Not;
        case BytecodeOpcode::BitNot:
            return IROpcode::BitNot;
        case BytecodeOpcode::Neg:
            return IROpcode::Neg;
        case BytecodeOpcode::Inc:
            return IROpcode::Add;  // Inc命令はAdd命令に変換
        case BytecodeOpcode::Dec:
            return IROpcode::Sub;  // Dec命令はSub命令に変換
        case BytecodeOpcode::LoadConst:
            return IROpcode::LoadConst;
        case BytecodeOpcode::LoadVar:
            return IROpcode::LoadVar;
        case BytecodeOpcode::StoreVar:
            return IROpcode::StoreVar;
        case BytecodeOpcode::Move:
            return IROpcode::Move;
        case BytecodeOpcode::Call:
            return IROpcode::Call;
        case BytecodeOpcode::Return:
            return IROpcode::Return;
        case BytecodeOpcode::Jump:
            return IROpcode::Jump;
        case BytecodeOpcode::JumpIfTrue:
            return IROpcode::JumpIfTrue;
        case BytecodeOpcode::JumpIfFalse:
            return IROpcode::JumpIfFalse;
        default:
            // デフォルトは無効命令
            return IROpcode::Invalid;
    }
}

IRType IRBuilder::InferType(const IRInstruction& inst) {
    // 命令に基づいて型を推論
    switch (inst.GetOpcode()) {
        case IROpcode::Add:
        case IROpcode::Sub:
        case IROpcode::Mul:
        case IROpcode::Div:
        case IROpcode::Mod:
            // 算術演算は一般的に数値型を返す
            // プロファイル情報があれば、さらに詳細な型を推論可能
            return IRType::Double;
            
        case IROpcode::BitAnd:
        case IROpcode::BitOr:
        case IROpcode::BitXor:
        case IROpcode::BitNot:
        case IROpcode::Shl:
        case IROpcode::Shr:
        case IROpcode::UShr:
            // ビット演算は常に整数を返す
            return IRType::Int32;
            
        case IROpcode::Eq:
        case IROpcode::Ne:
        case IROpcode::Lt:
        case IROpcode::Le:
        case IROpcode::Gt:
        case IROpcode::Ge:
        case IROpcode::And:
        case IROpcode::Or:
        case IROpcode::Not:
            // 論理/比較演算は常に真偽値を返す
            return IRType::Boolean;
            
        case IROpcode::LoadConst:
            // 定数ロードは定数の型による
            return inst.GetConstant().GetType();
            
        default:
            // デフォルトは不明な型
            return IRType::Unknown;
    }
}

void IRBuilder::OptimizeUsingProfile(IRFunction& function, uint32_t functionId) {
    // プロファイル情報を用いた最適化
    if (!m_profiler) return;
    
    // 型プロファイル情報を取得して型特化
    for (auto& inst : function.instructions()) {
        if (inst.IsArithmeticOp()) {
            // 命令からバイトコードオフセットを取得
            uint32_t bytecodeOffset = inst.GetBytecodeOffset();
            // プロファイル情報が利用可能なら、その情報に基づいて型を特化
            auto* typeProfile = m_profiler->GetTypeProfile(functionId, bytecodeOffset);
            if (typeProfile && typeProfile->isStable()) {
                // 型が安定していれば命令を型特化
                inst.SpecializeType(typeProfile->getDominantType());
            }
        }
    }
    
    // 分岐予測情報を用いたコード配置最適化
    // （実際の最適化はここに実装）
}

} // namespace core
} // namespace aerojs 