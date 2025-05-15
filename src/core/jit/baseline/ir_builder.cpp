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
        // 実際の実装では可変長命令に対応する必要がある
        offset += 4; // 仮に固定サイズとする
    }
}

void IRBuilder::EmitIRForBytecode(const uint8_t* bytecode, size_t offset, IRFunction& function) {
    // バイトコード命令を解析してIR命令を生成
    BytecodeOpcode opcode = static_cast<BytecodeOpcode>(bytecode[offset]);
    IROpcode irOpcode = MapBytecodeToIROpcode(opcode);
    
    // IR命令を生成
    IRInstruction inst(irOpcode);
    
    // オペランドを設定（実際の実装ではバイトコードフォーマットに応じて設定）
    // ...
    
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
            uint32_t bytecodeOffset = 0; // 実際の実装では命令からバイトコードオフセットを取得
            
            // プロファイル情報が利用可能なら、その情報に基づいて型を特化
            // 例: 整数加算に特化、浮動小数点加算に特化など
        }
    }
    
    // 分岐予測情報を用いたコード配置最適化
    // （実際の最適化はここに実装）
}

} // namespace core
} // namespace aerojs 