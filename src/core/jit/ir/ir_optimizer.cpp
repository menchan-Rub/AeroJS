#include "ir_optimizer.h"

#include <cassert>
#include <algorithm>
#include <unordered_set>
#include "../profiler/execution_profiler.h"

namespace aerojs {
namespace core {

IROptimizer::IROptimizer() noexcept {
  InitOptimizationPasses();
}

IROptimizer::~IROptimizer() noexcept = default;

std::unique_ptr<IRFunction> IROptimizer::Optimize(std::unique_ptr<IRFunction> irFunction,
                                                 uint32_t functionId,
                                                 OptimizationLevel level) noexcept {
  assert(irFunction && "IRFunction cannot be null");
  
  // レベルに基づいた最適化パスを取得
  auto passesIt = m_optimizationPasses.find(level);
  if (passesIt == m_optimizationPasses.end() || passesIt->second.empty()) {
    // 最適化なしの場合、そのまま返す
    return irFunction;
  }
  
  // プロファイル情報の取得
  const ProfileData* profileData = ExecutionProfiler::Instance().GetProfileData(functionId);
  
  // 各最適化パスを実行
  bool changed = false;
  for (const auto& pass : passesIt->second) {
    changed |= pass(*irFunction, functionId);
  }
  
  return irFunction;
}

void IROptimizer::Reset() noexcept {
  // 特に状態を持たないため、何もしない
}

void IROptimizer::InitOptimizationPasses() noexcept {
  // レベル：なし（最適化なし）
  m_optimizationPasses[OptimizationLevel::None] = {};
  
  // レベル：基本（ベースラインJIT向け）
  m_optimizationPasses[OptimizationLevel::Basic] = {
    [this](IRFunction& function, uint32_t functionId) {
      return this->ConstantFolding(function, functionId);
    },
    [this](IRFunction& function, uint32_t functionId) {
      return this->DeadCodeElimination(function, functionId);
    }
  };
  
  // レベル：中程度（標準最適化JIT向け）
  m_optimizationPasses[OptimizationLevel::Medium] = {
    [this](IRFunction& function, uint32_t functionId) {
      return this->ConstantFolding(function, functionId);
    },
    [this](IRFunction& function, uint32_t functionId) {
      return this->DeadCodeElimination(function, functionId);
    },
    [this](IRFunction& function, uint32_t functionId) {
      return this->CommonSubexpressionElimination(function, functionId);
    },
    [this](IRFunction& function, uint32_t functionId) {
      return this->InstructionCombining(function, functionId);
    }
  };
  
  // レベル：積極的（超最適化JIT向け）
  m_optimizationPasses[OptimizationLevel::Aggressive] = {
    [this](IRFunction& function, uint32_t functionId) {
      return this->ConstantFolding(function, functionId);
    },
    [this](IRFunction& function, uint32_t functionId) {
      return this->DeadCodeElimination(function, functionId);
    },
    [this](IRFunction& function, uint32_t functionId) {
      return this->CommonSubexpressionElimination(function, functionId);
    },
    [this](IRFunction& function, uint32_t functionId) {
      return this->InstructionCombining(function, functionId);
    },
    [this](IRFunction& function, uint32_t functionId) {
      return this->TypeSpecialization(function, functionId);
    },
    [this](IRFunction& function, uint32_t functionId) {
      return this->LoopOptimization(function, functionId);
    }
  };
}

// 定数畳み込み最適化
bool IROptimizer::ConstantFolding(IRFunction& function, uint32_t functionId) noexcept {
  bool changed = false;
  
  // 命令リストの取得 - GetInstructions()は参照を返すメソッド
  // しかし、IRFunctionを直接変更することはできないので、新しい命令リストを構築
  std::vector<IRInstruction> optimizedInstructions;
  const auto& instructions = function.GetInstructions();
  
  // 定数値を追跡するマップ
  std::unordered_map<int32_t, int32_t> constantValues;
  
  for (size_t i = 0; i < instructions.size(); ++i) {
    const auto& inst = instructions[i];
    
    // 算術演算の定数畳み込み
    if ((inst.opcode == Opcode::Add || inst.opcode == Opcode::Sub ||
         inst.opcode == Opcode::Mul || inst.opcode == Opcode::Div) &&
        i >= 2) {
      
      const auto& prev1 = instructions[i-1];
      const auto& prev2 = instructions[i-2];
      
      // 両方の引数が定数ロードであれば
      if (prev1.opcode == Opcode::LoadConst && prev2.opcode == Opcode::LoadConst) {
        int32_t val1 = prev1.args.empty() ? 0 : prev1.args[0];
        int32_t val2 = prev2.args.empty() ? 0 : prev2.args[0];
        int32_t result = 0;
        
        // 演算の実行
        switch (inst.opcode) {
          case Opcode::Add:
            result = val2 + val1;
            break;
          case Opcode::Sub:
            result = val2 - val1;
            break;
          case Opcode::Mul:
            result = val2 * val1;
            break;
          case Opcode::Div:
            if (val1 != 0) {
              result = val2 / val1;
            } else {
              // 除算エラーの場合は最適化を行わない
              optimizedInstructions.push_back(inst);
              continue;
            }
            break;
          default:
            // 想定外のオペコード
            optimizedInstructions.push_back(inst);
            continue;
        }
        
        // prev2とprev1は既に処理済みとして最適化されたリストから削除
        if (!optimizedInstructions.empty()) {
          optimizedInstructions.pop_back(); // prev2 を削除
        }
        if (!optimizedInstructions.empty()) {
          optimizedInstructions.pop_back(); // prev1 を削除
        }
        
        // 定数値のロード命令に置き換え
        IRInstruction newInst;
        newInst.opcode = Opcode::LoadConst;
        newInst.args = {result};
        optimizedInstructions.push_back(newInst);
        
        changed = true;
        continue;
      }
    }
    
    // その他の最適化できない命令はそのまま追加
    optimizedInstructions.push_back(inst);
  }
  
  // 最適化された命令リストで元の関数を置き換え
  if (changed) {
    function.Clear();
    for (const auto& inst : optimizedInstructions) {
      function.AddInstruction(inst);
    }
  }
  
  return changed;
}

// デッドコード除去
bool IROptimizer::DeadCodeElimination(IRFunction& function, uint32_t functionId) noexcept {
  bool changed = false;
  
  // 命令リストの取得
  const auto& instructions = function.GetInstructions();
  
  // 現在のところ、単純な最適化のみ実装: Nop 命令の除去
  std::vector<IRInstruction> optimizedInstructions;
  
  for (const auto& inst : instructions) {
    if (inst.opcode != Opcode::Nop) {
      optimizedInstructions.push_back(inst);
    } else {
      changed = true;
    }
  }
  
  // 最適化された命令リストで元の関数を置き換え
  if (changed) {
    function.Clear();
    for (const auto& inst : optimizedInstructions) {
      function.AddInstruction(inst);
    }
  }
  
  return changed;
}

// 共通部分式の除去
bool IROptimizer::CommonSubexpressionElimination(IRFunction& function, uint32_t functionId) noexcept {
  // 現時点では簡略実装
  // 実際には以下のようなアルゴリズムが必要:
  // 1. 基本ブロック分析
  // 2. 利用可能な式の追跡
  // 3. 式の置き換え
  return false;
}

// 命令の組み合わせ最適化
bool IROptimizer::InstructionCombining(IRFunction& function, uint32_t functionId) noexcept {
  // 現時点では簡略実装
  return false;
}

// 型特化最適化
bool IROptimizer::TypeSpecialization(IRFunction& function, uint32_t functionId) noexcept {
  // 型安定性が高い関数に対してのみ実行
  const ProfileData* profileData = ExecutionProfiler::Instance().GetProfileData(functionId);
  if (!profileData || !profileData->isStable) {
    return false;
  }
  
  // 現時点では簡略実装
  return false;
}

// ループ最適化
bool IROptimizer::LoopOptimization(IRFunction& function, uint32_t functionId) noexcept {
  // 現時点では簡略実装
  return false;
}

}  // namespace core
}  // namespace aerojs 