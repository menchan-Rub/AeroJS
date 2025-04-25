#include "ir_builder.h"

#include <cassert>
#include <functional>
#include "../profiler/execution_profiler.h"

namespace aerojs {
namespace core {

// バイトコード命令仮定義
enum BytecodeOp : uint8_t {
  BC_NOP = 0x00,
  BC_LOAD_CONST = 0x01,
  BC_LOAD_VAR = 0x02,
  BC_STORE_VAR = 0x03,
  BC_ADD = 0x04,
  BC_SUB = 0x05,
  BC_MUL = 0x06,
  BC_DIV = 0x07,
  BC_CALL = 0x08,
  BC_RETURN = 0x09
};

IRBuilder::IRBuilder() noexcept 
    : m_function(nullptr) {
  InitBytecodeHandlers();
}

IRBuilder::~IRBuilder() noexcept = default;

std::unique_ptr<IRFunction> IRBuilder::BuildIR(const std::vector<uint8_t>& bytecodes, 
                                             uint32_t functionId) noexcept {
  // 内部状態をリセット
  Reset();
  
  // 新しいIR関数を作成
  auto irFunction = std::make_unique<IRFunction>();
  m_function = irFunction.get();
  
  // バイトコード処理の実装はここに追加
  
  return irFunction;
}

void IRBuilder::Reset() noexcept {
  m_function = nullptr;
}

// IR命令生成メソッド
void IRBuilder::BuildNop() noexcept {
  if (m_function) {
    AddInstruction(Opcode::kNop);
  }
}

void IRBuilder::BuildLoadConst(uint32_t dest, uint32_t value) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kLoadConst, {static_cast<int32_t>(dest), static_cast<int32_t>(value)});
  }
}

void IRBuilder::BuildMove(uint32_t dest, uint32_t src) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kMove, {static_cast<int32_t>(dest), static_cast<int32_t>(src)});
  }
}

void IRBuilder::BuildAdd(uint32_t dest, uint32_t src1, uint32_t src2) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kAdd, {static_cast<int32_t>(dest), 
                                static_cast<int32_t>(src1), 
                                static_cast<int32_t>(src2)});
  }
}

void IRBuilder::BuildSub(uint32_t dest, uint32_t src1, uint32_t src2) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kSub, {static_cast<int32_t>(dest), 
                                static_cast<int32_t>(src1), 
                                static_cast<int32_t>(src2)});
  }
}

void IRBuilder::BuildMul(uint32_t dest, uint32_t src1, uint32_t src2) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kMul, {static_cast<int32_t>(dest), 
                                static_cast<int32_t>(src1), 
                                static_cast<int32_t>(src2)});
  }
}

void IRBuilder::BuildDiv(uint32_t dest, uint32_t src1, uint32_t src2) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kDiv, {static_cast<int32_t>(dest), 
                                static_cast<int32_t>(src1), 
                                static_cast<int32_t>(src2)});
  }
}

void IRBuilder::BuildCompareEq(uint32_t dest, uint32_t src1, uint32_t src2) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kCompareEq, {static_cast<int32_t>(dest), 
                                      static_cast<int32_t>(src1), 
                                      static_cast<int32_t>(src2)});
  }
}

void IRBuilder::BuildCompareNe(uint32_t dest, uint32_t src1, uint32_t src2) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kCompareNe, {static_cast<int32_t>(dest), 
                                      static_cast<int32_t>(src1), 
                                      static_cast<int32_t>(src2)});
  }
}

void IRBuilder::BuildCompareLt(uint32_t dest, uint32_t src1, uint32_t src2) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kCompareLt, {static_cast<int32_t>(dest), 
                                      static_cast<int32_t>(src1), 
                                      static_cast<int32_t>(src2)});
  }
}

void IRBuilder::BuildCompareLe(uint32_t dest, uint32_t src1, uint32_t src2) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kCompareLe, {static_cast<int32_t>(dest), 
                                      static_cast<int32_t>(src1), 
                                      static_cast<int32_t>(src2)});
  }
}

void IRBuilder::BuildCompareGt(uint32_t dest, uint32_t src1, uint32_t src2) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kCompareGt, {static_cast<int32_t>(dest), 
                                      static_cast<int32_t>(src1), 
                                      static_cast<int32_t>(src2)});
  }
}

void IRBuilder::BuildCompareGe(uint32_t dest, uint32_t src1, uint32_t src2) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kCompareGe, {static_cast<int32_t>(dest), 
                                      static_cast<int32_t>(src1), 
                                      static_cast<int32_t>(src2)});
  }
}

void IRBuilder::BuildJump(const std::string& label) noexcept {
  if (m_function) {
    // ラベル名をIR命令に格納（実際の実装ではラベルハンドリングが必要）
    auto labelId = m_function->RegisterLabel(label);
    AddInstruction(Opcode::kJump, {static_cast<int32_t>(labelId)});
  }
}

void IRBuilder::BuildJumpIfTrue(uint32_t condReg, const std::string& label) noexcept {
  if (m_function) {
    auto labelId = m_function->RegisterLabel(label);
    AddInstruction(Opcode::kJumpIfTrue, {static_cast<int32_t>(condReg), 
                                       static_cast<int32_t>(labelId)});
  }
}

void IRBuilder::BuildJumpIfFalse(uint32_t condReg, const std::string& label) noexcept {
  if (m_function) {
    auto labelId = m_function->RegisterLabel(label);
    AddInstruction(Opcode::kJumpIfFalse, {static_cast<int32_t>(condReg), 
                                        static_cast<int32_t>(labelId)});
  }
}

void IRBuilder::BuildCall(uint32_t dest, uint32_t funcReg, const std::vector<uint32_t>& args) noexcept {
  if (m_function) {
    std::vector<int32_t> callArgs = {static_cast<int32_t>(dest), static_cast<int32_t>(funcReg)};
    
    // 引数を追加
    for (auto arg : args) {
      callArgs.push_back(static_cast<int32_t>(arg));
    }
    
    AddInstruction(Opcode::kCall, callArgs);
  }
}

void IRBuilder::BuildReturn() noexcept {
  if (m_function) {
    AddInstruction(Opcode::kReturn);
  }
}

void IRBuilder::BuildReturn(uint32_t retReg) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kReturn, {static_cast<int32_t>(retReg)});
  }
}

void IRBuilder::BuildProfileExecution(uint32_t bytecodeOffset) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kProfileExecution, {static_cast<int32_t>(bytecodeOffset)});
  }
}

void IRBuilder::BuildProfileType(uint32_t bytecodeOffset, uint32_t typeCategory) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kProfileType, {static_cast<int32_t>(bytecodeOffset), 
                                        static_cast<int32_t>(typeCategory)});
  }
}

void IRBuilder::BuildProfileCallSite(uint32_t bytecodeOffset) noexcept {
  if (m_function) {
    AddInstruction(Opcode::kProfileCallSite, {static_cast<int32_t>(bytecodeOffset)});
  }
}

void IRBuilder::InitBytecodeHandlers() noexcept {
  // バイトコード命令ハンドラを登録する処理
  // ここではサンプルとしていくつかの命令のみ登録
  m_bytecodeHandlers[0] = [this](uint8_t opcode, const std::vector<uint8_t>& bytecodes, size_t& offset) {
    this->HandleNop(opcode, bytecodes, offset);
  };
  
  // 他の命令ハンドラも同様に登録
}

void IRBuilder::HandleNop(uint8_t opcode, const std::vector<uint8_t>& bytecodes, size_t& offset) noexcept {
  BuildNop();
  offset += 1; // オペコードのサイズ分進める
}

void IRBuilder::HandleLoadConst(uint8_t opcode, const std::vector<uint8_t>& bytecodes, size_t& offset) noexcept {
  // バイトコード読み取りとIR生成の実装
  // オペランドを解析して BuildLoadConst() を呼び出す
}

// 他のバイトコード命令ハンドラも同様に実装

void IRBuilder::AddInstruction(Opcode opcode, const std::vector<int32_t>& args) noexcept {
  assert(m_function && "IR関数が設定されていません");
  
  // 命令オブジェクトを作成
  IRInstruction instruction;
  instruction.opcode = opcode;
  instruction.args = args;
  
  // IR関数に命令を追加
  m_function->AddInstruction(instruction);
}

}  // namespace core
}  // namespace aerojs 