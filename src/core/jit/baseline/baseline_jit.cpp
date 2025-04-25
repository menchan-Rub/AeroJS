#include "baseline_jit.h"
#include "bytecode_decoder.h"
#include "register_allocator.h"

#include <cstring>  // for std::memcpy
#include <algorithm>
#include <cassert>
#include <chrono>   // for std::chrono

// アーキテクチャ固有のコードジェネレーター
#ifdef __x86_64__
#include "../backend/x86_64/x86_64_code_generator.h"
#elif defined(__aarch64__)
#include "../backend/arm64/arm64_code_generator.h"
#elif defined(__riscv)
#include "../backend/riscv/riscv_code_generator.h"
#else
#error "サポートされていないアーキテクチャです"
#endif

namespace aerojs {
namespace core {

// 静的メンバ変数の定義
JITProfiler BaselineJIT::m_profiler;

BaselineJIT::BaselineJIT(uint32_t functionId, bool enableProfiling) noexcept
    : m_decoder(std::make_unique<BytecodeDecoder>()),
      m_regAllocator(std::make_unique<RegisterAllocator>()),
      m_irBuilder(),
      m_functionId(functionId),
      m_profilingEnabled(enableProfiling) {
}

BaselineJIT::~BaselineJIT() noexcept = default;

std::unique_ptr<uint8_t[]> BaselineJIT::Compile(const std::vector<uint8_t>& bytecodes,
                                                size_t& outCodeSize) noexcept {
  // プロファイリングが有効な場合、関数を登録
  if (m_profilingEnabled && m_functionId != 0) {
    m_profiler.RegisterFunction(m_functionId, bytecodes.size());
  }

  // コンパイル開始時刻を記録
  auto startTime = std::chrono::high_resolution_clock::now();

  // IRを生成
  auto irFunction = GenerateIR(bytecodes);
  if (!irFunction) {
    outCodeSize = 0;
    return nullptr;
  }

  // マシンコードを生成
  auto result = GenerateMachineCode(irFunction.get(), outCodeSize);

  // プロファイリングが有効な場合、コンパイル時間を記録
  if (m_profilingEnabled && m_functionId != 0) {
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();
    
    // 自身のコンパイル時間も一種の関数実行としてプロファイル
    m_profiler.RecordExecution(m_functionId, 0);
  }

  return result;
}

std::unique_ptr<IRFunction> BaselineJIT::GenerateIR(
    const std::vector<uint8_t>& bytecodes) noexcept {
  if (bytecodes.empty()) {
    return nullptr;
  }

  // 内部状態をリセット
  m_state.offsetToIRIndex.clear();
  m_state.labelToIRIndex.clear();
  m_irBuilder.Reset();

  // ジャンプターゲットを解決
  auto jumpTargets = ResolveJumpTargets(bytecodes);

  // バイトコードデコーダーを初期化
  m_decoder->SetBytecode(bytecodes.data(), bytecodes.size());
  m_decoder->Reset();

  // IR関数の作成
  auto irFunction = std::make_unique<IRFunction>();
  m_irBuilder.SetFunction(irFunction.get());

  // バイトコードをIRに変換
  while (m_decoder->HasMoreInstructions()) {
    size_t currentOffset = m_decoder->GetCurrentOffset();
    
    // 現在のオフセットをIRインデックスにマッピング
    m_state.offsetToIRIndex[currentOffset] = irFunction->GetInstructionCount();
    
    // プロファイリングが有効な場合、実行カウンターを挿入
    if (m_profilingEnabled && m_functionId != 0) {
      // 実行プロファイリング用のIR命令を挿入
      m_irBuilder.BuildProfileExecution(static_cast<uint32_t>(currentOffset));
    }
    
    // バイトコード命令をデコード
    BytecodeOpcode opcode;
    std::vector<uint32_t> operands;
    if (!m_decoder->DecodeNextInstruction(opcode, operands)) {
      // デコード失敗
      return nullptr;
    }

    // 命令ごとに対応するIR命令を生成
    switch (opcode) {
      case BytecodeOpcode::kNop:
        m_irBuilder.BuildNop();
        break;

      case BytecodeOpcode::kLoadConst:
        if (operands.size() == 2) {
          m_irBuilder.BuildLoadConst(operands[0], operands[1]);
          
          // 型プロファイリング（定数値の型を記録）
          if (m_profilingEnabled && m_functionId != 0) {
            // 定数値の型を推定
            uint32_t constValue = operands[1];
            TypeFeedbackRecord::TypeCategory typeCategory;
            
            // 単純な型推定（実際の実装では値の範囲や特性に基づいてより詳細に判定）
            if (constValue == 0 || constValue == 1) {
              typeCategory = TypeFeedbackRecord::TypeCategory::Boolean;
            } else {
              typeCategory = TypeFeedbackRecord::TypeCategory::Integer;
            }
            
            ProfileType(static_cast<uint32_t>(currentOffset), typeCategory);
          }
        }
        break;

      case BytecodeOpcode::kMove:
        if (operands.size() == 2) {
          m_irBuilder.BuildMove(operands[0], operands[1]);
        }
        break;

      case BytecodeOpcode::kAdd:
        if (operands.size() == 3) {
          m_irBuilder.BuildAdd(operands[0], operands[1], operands[2]);
          
          // 演算命令の型プロファイリング
          if (m_profilingEnabled && m_functionId != 0) {
            ProfileType(static_cast<uint32_t>(currentOffset), TypeFeedbackRecord::TypeCategory::Integer);
          }
        }
        break;

      case BytecodeOpcode::kSub:
        if (operands.size() == 3) {
          m_irBuilder.BuildSub(operands[0], operands[1], operands[2]);
          
          if (m_profilingEnabled && m_functionId != 0) {
            ProfileType(static_cast<uint32_t>(currentOffset), TypeFeedbackRecord::TypeCategory::Integer);
          }
        }
        break;

      case BytecodeOpcode::kMul:
        if (operands.size() == 3) {
          m_irBuilder.BuildMul(operands[0], operands[1], operands[2]);
          
          if (m_profilingEnabled && m_functionId != 0) {
            ProfileType(static_cast<uint32_t>(currentOffset), TypeFeedbackRecord::TypeCategory::Integer);
          }
        }
        break;

      case BytecodeOpcode::kDiv:
        if (operands.size() == 3) {
          m_irBuilder.BuildDiv(operands[0], operands[1], operands[2]);
          
          if (m_profilingEnabled && m_functionId != 0) {
            // 除算は結果がDouble型になる可能性が高い
            ProfileType(static_cast<uint32_t>(currentOffset), TypeFeedbackRecord::TypeCategory::Double);
          }
        }
        break;

      case BytecodeOpcode::kEq:
        if (operands.size() == 3) {
          m_irBuilder.BuildCompareEq(operands[0], operands[1], operands[2]);
          
          if (m_profilingEnabled && m_functionId != 0) {
            ProfileType(static_cast<uint32_t>(currentOffset), TypeFeedbackRecord::TypeCategory::Boolean);
          }
        }
        break;

      case BytecodeOpcode::kNeq:
        if (operands.size() == 3) {
          m_irBuilder.BuildCompareNe(operands[0], operands[1], operands[2]);
          
          if (m_profilingEnabled && m_functionId != 0) {
            ProfileType(static_cast<uint32_t>(currentOffset), TypeFeedbackRecord::TypeCategory::Boolean);
          }
        }
        break;

      case BytecodeOpcode::kLt:
        if (operands.size() == 3) {
          m_irBuilder.BuildCompareLt(operands[0], operands[1], operands[2]);
          
          if (m_profilingEnabled && m_functionId != 0) {
            ProfileType(static_cast<uint32_t>(currentOffset), TypeFeedbackRecord::TypeCategory::Boolean);
          }
        }
        break;

      case BytecodeOpcode::kLe:
        if (operands.size() == 3) {
          m_irBuilder.BuildCompareLe(operands[0], operands[1], operands[2]);
          
          if (m_profilingEnabled && m_functionId != 0) {
            ProfileType(static_cast<uint32_t>(currentOffset), TypeFeedbackRecord::TypeCategory::Boolean);
          }
        }
        break;

      case BytecodeOpcode::kGt:
        if (operands.size() == 3) {
          m_irBuilder.BuildCompareGt(operands[0], operands[1], operands[2]);
          
          if (m_profilingEnabled && m_functionId != 0) {
            ProfileType(static_cast<uint32_t>(currentOffset), TypeFeedbackRecord::TypeCategory::Boolean);
          }
        }
        break;

      case BytecodeOpcode::kGe:
        if (operands.size() == 3) {
          m_irBuilder.BuildCompareGe(operands[0], operands[1], operands[2]);
          
          if (m_profilingEnabled && m_functionId != 0) {
            ProfileType(static_cast<uint32_t>(currentOffset), TypeFeedbackRecord::TypeCategory::Boolean);
          }
        }
        break;

      case BytecodeOpcode::kJump: {
        if (operands.size() == 1) {
          // ジャンプ先のオフセットを取得
          size_t targetOffset = operands[0];
          std::string labelName = "L" + std::to_string(targetOffset);
          m_irBuilder.BuildJump(labelName);
        }
        break;
      }

      case BytecodeOpcode::kJumpIfTrue: {
        if (operands.size() == 2) {
          size_t condition = operands[0];
          size_t targetOffset = operands[1];
          std::string labelName = "L" + std::to_string(targetOffset);
          m_irBuilder.BuildJumpIfTrue(condition, labelName);
          
          if (m_profilingEnabled && m_functionId != 0) {
            ProfileType(static_cast<uint32_t>(currentOffset), TypeFeedbackRecord::TypeCategory::Boolean);
          }
        }
        break;
      }

      case BytecodeOpcode::kJumpIfFalse: {
        if (operands.size() == 2) {
          size_t condition = operands[0];
          size_t targetOffset = operands[1];
          std::string labelName = "L" + std::to_string(targetOffset);
          m_irBuilder.BuildJumpIfFalse(condition, labelName);
          
          if (m_profilingEnabled && m_functionId != 0) {
            ProfileType(static_cast<uint32_t>(currentOffset), TypeFeedbackRecord::TypeCategory::Boolean);
          }
        }
        break;
      }

      case BytecodeOpcode::kCall:
        if (operands.size() >= 2) {
          // 第1引数: 結果を格納するレジスタ
          // 第2引数: 関数のアドレスを格納するレジスタ
          // 第3引数以降: 引数レジスタ
          std::vector<uint32_t> args(operands.begin() + 2, operands.end());
          m_irBuilder.BuildCall(operands[0], operands[1], args);
          
          if (m_profilingEnabled && m_functionId != 0) {
            // 呼び出し先の関数IDは実行時に決まるため、IR命令で呼び出しサイトを記録
            m_irBuilder.BuildProfileCallSite(static_cast<uint32_t>(currentOffset));
          }
        }
        break;

      case BytecodeOpcode::kReturn:
        if (operands.size() == 1) {
          m_irBuilder.BuildReturn(operands[0]);
        } else {
          m_irBuilder.BuildReturn();
        }
        break;

      default:
        // サポートされていない命令
        break;
    }
  }

  // ラベルの解決
  for (const auto& [label, offset] : jumpTargets) {
    auto it = m_state.offsetToIRIndex.find(offset);
    if (it != m_state.offsetToIRIndex.end()) {
      m_state.labelToIRIndex[label] = it->second;
    }
  }

  return irFunction;
}

std::unique_ptr<uint8_t[]> BaselineJIT::GenerateMachineCode(
    IRFunction* irFunction, size_t& outCodeSize) noexcept {
  if (!irFunction) {
    outCodeSize = 0;
    return nullptr;
  }

  // アーキテクチャに応じたコードジェネレーターを作成
#ifdef __x86_64__
  X86_64CodeGenerator codeGenerator;
#elif defined(__aarch64__)
  ARM64CodeGenerator codeGenerator;
#elif defined(__riscv)
  RISCVCodeGenerator codeGenerator;
#endif

  // ラベル情報をコードジェネレーターに設定
  for (const auto& [label, irIndex] : m_state.labelToIRIndex) {
    codeGenerator.DefineLabel(label, irIndex);
  }

  // ジェネレーターにプロファイリング情報を設定
  if (m_profilingEnabled && m_functionId != 0) {
    codeGenerator.EnableProfiling(true);
    codeGenerator.SetFunctionId(m_functionId);
  }

  // IR関数からマシンコードを生成
  auto machineCode = codeGenerator.Generate(*irFunction, outCodeSize);
  return machineCode;
}

std::unordered_map<std::string, size_t> BaselineJIT::ResolveJumpTargets(
    const std::vector<uint8_t>& bytecodes) noexcept {
  std::unordered_map<std::string, size_t> jumpTargets;
  
  if (bytecodes.empty()) {
    return jumpTargets;
  }
  
  // ジャンプ命令を探してターゲットアドレスを収集
  m_decoder->SetBytecode(bytecodes.data(), bytecodes.size());
  m_decoder->Reset();
  
  while (m_decoder->HasMoreInstructions()) {
    size_t currentOffset = m_decoder->GetCurrentOffset();
    
    BytecodeOpcode opcode;
    std::vector<uint32_t> operands;
    if (!m_decoder->DecodeNextInstruction(opcode, operands)) {
      break;
    }
    
    switch (opcode) {
      case BytecodeOpcode::kJump:
        if (operands.size() == 1) {
          size_t targetOffset = operands[0];
          std::string labelName = "L" + std::to_string(targetOffset);
          jumpTargets[labelName] = targetOffset;
        }
        break;
        
      case BytecodeOpcode::kJumpIfTrue:
      case BytecodeOpcode::kJumpIfFalse:
        if (operands.size() == 2) {
          size_t targetOffset = operands[1];
          std::string labelName = "L" + std::to_string(targetOffset);
          jumpTargets[labelName] = targetOffset;
        }
        break;
        
      default:
        break;
    }
  }
  
  return jumpTargets;
}

void BaselineJIT::Reset() noexcept {
  if (m_decoder) {
    m_decoder->Reset();
  }
  
  if (m_regAllocator) {
    m_regAllocator->Reset();
  }
  
  m_irBuilder.Reset();
  m_state.offsetToIRIndex.clear();
  m_state.labelToIRIndex.clear();
}

void BaselineJIT::ProfileExecution(uint32_t offset) noexcept {
  if (m_profilingEnabled && m_functionId != 0) {
    m_profiler.RecordExecution(m_functionId, offset);
  }
}

void BaselineJIT::ProfileType(uint32_t offset, TypeFeedbackRecord::TypeCategory type) noexcept {
  if (m_profilingEnabled && m_functionId != 0) {
    m_profiler.RecordTypeObservation(m_functionId, offset, type);
  }
}

void BaselineJIT::ProfileCallSite(uint32_t offset, uint32_t calleeFunctionId, uint64_t executionTimeNs) noexcept {
  if (m_profilingEnabled && m_functionId != 0) {
    m_profiler.RecordCallSite(m_functionId, offset, calleeFunctionId, executionTimeNs);
  }
}

}  // namespace core
}  // namespace aerojs