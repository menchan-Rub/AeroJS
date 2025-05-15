#include "ir_optimizer.h"

#include <cassert>
#include <algorithm>
#include <unordered_set>
#include "../profiler/execution_profiler.h"

namespace aerojs {
namespace core {

// 最適化パスのインスタンス
static ConstantFoldingPass s_constantFoldingPass;
// 未実装の最適化パスのフォワード宣言

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
  // 新しい定数畳み込みパスを使用
  return s_constantFoldingPass.run(&function);
}

// デッドコード除去
bool IROptimizer::DeadCodeElimination(IRFunction& function, uint32_t functionId) noexcept {
  bool changed = false;
  
  // 命令リストの取得
  const auto& instructions = function.GetInstructions();
  
  // 現在のところ、単純な最適化のみ実装: Nop 命令の除去
  std::vector<IRInstruction> optimizedInstructions;
  
  for (const auto& inst : instructions) {
    if (inst.opcode != Opcode::kNop) {
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
  bool changed = false;
  const auto& instructions = function.GetInstructions();
  
  // 共通部分式の検出に使用する辞書
  struct ExpressionHash {
    std::size_t operator()(const IRInstruction& inst) const {
      std::size_t h = std::hash<int>()(static_cast<int>(inst.opcode));
      for (auto arg : inst.args) {
        h ^= std::hash<int32_t>()(arg) + 0x9e3779b9 + (h << 6) + (h >> 2);
      }
      return h;
    }
  };
  
  struct ExpressionEqual {
    bool operator()(const IRInstruction& a, const IRInstruction& b) const {
      if (a.opcode != b.opcode || a.args.size() != b.args.size()) {
        return false;
      }
      for (size_t i = 0; i < a.args.size(); ++i) {
        if (a.args[i] != b.args[i]) {
  return false;
        }
      }
      return true;
    }
  };
  
  // 式のハッシュマップを構築
  std::unordered_map<IRInstruction, int32_t, ExpressionHash, ExpressionEqual> expressionMap;
  
  // 置換マップ（古いレジスタ→新しいレジスタ）
  std::unordered_map<int32_t, int32_t> replacementMap;
  
  // 式の重複を検出し、置換マップを構築
  for (size_t i = 0; i < instructions.size(); ++i) {
    const auto& inst = instructions[i];
    
    // レジスタオペランドの置換を適用（以前に見つかった共通部分式の結果を使用）
    auto updatedInst = inst;
    for (size_t j = 1; j < updatedInst.args.size(); ++j) {
      auto it = replacementMap.find(updatedInst.args[j]);
      if (it != replacementMap.end()) {
        updatedInst.args[j] = it->second;
        changed = true;
      }
    }
    
    // 単純な代入操作はスキップ
    if (updatedInst.opcode == Opcode::kMove || updatedInst.opcode == Opcode::kNop ||
        updatedInst.opcode == Opcode::kJump || updatedInst.opcode == Opcode::kJumpIfTrue ||
        updatedInst.opcode == Opcode::kJumpIfFalse || updatedInst.opcode == Opcode::kCall ||
        updatedInst.opcode == Opcode::kReturn) {
      continue;
    }
    
    // 式が以前に計算されたかチェック
    auto it = expressionMap.find(updatedInst);
    if (it != expressionMap.end()) {
      // 共通部分式が見つかった場合、結果レジスタを置き換える
      int32_t resultReg = it->second;
      int32_t currentReg = updatedInst.args[0];
      
      replacementMap[currentReg] = resultReg;
      changed = true;
    } else {
      // 新しい式を追加
      expressionMap[updatedInst] = updatedInst.args[0];
    }
  }
  
  // 置換マップを適用して命令リストを更新
  if (changed) {
    std::vector<IRInstruction> optimizedInstructions;
    std::unordered_set<int> skipIndices;
    
    for (size_t i = 0; i < instructions.size(); ++i) {
      if (skipIndices.find(i) != skipIndices.end()) {
        continue;
      }
      
      auto updatedInst = instructions[i];
      
      // 出力レジスタが置換されている場合、この命令はスキップ
      auto it = replacementMap.find(updatedInst.args[0]);
      if (it != replacementMap.end() && 
          (updatedInst.opcode != Opcode::kMove && updatedInst.opcode != Opcode::kJumpIfTrue &&
           updatedInst.opcode != Opcode::kJumpIfFalse)) {
        skipIndices.insert(i);
        continue;
      }
      
      // 入力オペランドを置換
      for (size_t j = 1; j < updatedInst.args.size(); ++j) {
        auto it = replacementMap.find(updatedInst.args[j]);
        if (it != replacementMap.end()) {
          updatedInst.args[j] = it->second;
        }
      }
      
      optimizedInstructions.push_back(updatedInst);
    }
    
    function.Clear();
    for (const auto& inst : optimizedInstructions) {
      function.AddInstruction(inst);
    }
  }
  
  return changed;
}

// 命令の組み合わせ最適化
bool IROptimizer::InstructionCombining(IRFunction& function, uint32_t functionId) noexcept {
  bool changed = false;
  const auto& instructions = function.GetInstructions();
  std::vector<IRInstruction> optimizedInstructions;
  
  for (size_t i = 0; i < instructions.size(); ++i) {
    // 現在の命令
    const auto& inst = instructions[i];
    
    // 次の命令（存在する場合）
    const IRInstruction* nextInst = (i + 1 < instructions.size()) ? &instructions[i + 1] : nullptr;
    
    // パターン1: 符号反転と加算/減算の組み合わせ
    // x = -a; y = x + b; → y = b - a;
    if (inst.opcode == Opcode::kNeg && nextInst && nextInst->opcode == Opcode::kAdd) {
      if (inst.args[0] == nextInst->args[1]) {
        // y = b - a パターンを生成
        IRInstruction combinedInst;
        combinedInst.opcode = Opcode::kSub;
        combinedInst.args.push_back(nextInst->args[0]); // 結果
        combinedInst.args.push_back(nextInst->args[2]); // b
        combinedInst.args.push_back(inst.args[1]);      // a
        
        optimizedInstructions.push_back(combinedInst);
        i++; // 次の命令をスキップ
        changed = true;
        continue;
      }
    }
    
    // パターン2: 定数の乗算と加算
    // x = a * const1; y = x + const2; → y = a * const1 + const2;
    if (inst.opcode == Opcode::kMul && nextInst && nextInst->opcode == Opcode::kAdd) {
      if (inst.args[0] == nextInst->args[1] && inst.args.size() >= 3 && 
          inst.args[2] >= 0 && inst.args[2] < function.GetConstantCount()) {
        // 定数マルチプライヤーと追加の定数
        IRInstruction combinedInst;
        combinedInst.opcode = Opcode::kMulAdd;
        combinedInst.args.push_back(nextInst->args[0]); // 結果
        combinedInst.args.push_back(inst.args[1]);      // a
        combinedInst.args.push_back(inst.args[2]);      // const1
        combinedInst.args.push_back(nextInst->args[2]); // const2/b
        
        optimizedInstructions.push_back(combinedInst);
        i++; // 次の命令をスキップ
        changed = true;
        continue;
      }
    }
    
    // パターン3: ビット操作の連鎖
    // x = a & const1; y = x | const2; → y = (a & const1) | const2;
    if (inst.opcode == Opcode::kBitAnd && nextInst && nextInst->opcode == Opcode::kBitOr) {
      if (inst.args[0] == nextInst->args[1]) {
        IRInstruction combinedInst;
        combinedInst.opcode = Opcode::kBitAndOr;
        combinedInst.args.push_back(nextInst->args[0]); // 結果
        combinedInst.args.push_back(inst.args[1]);      // a
        combinedInst.args.push_back(inst.args[2]);      // const1
        combinedInst.args.push_back(nextInst->args[2]); // const2
        
        optimizedInstructions.push_back(combinedInst);
        i++;
        changed = true;
        continue;
      }
    }
    
    // パターン4: 乗算と除算の最適化
    // x = a * const1; y = x / const1; → y = a;
    if (inst.opcode == Opcode::kMul && nextInst && nextInst->opcode == Opcode::kDiv) {
      if (inst.args[0] == nextInst->args[1] && inst.args.size() >= 3 && nextInst->args.size() >= 3 &&
          inst.args[2] == nextInst->args[2]) {
        // 乗算と除算で同じ定数を使用
        IRInstruction moveInst;
        moveInst.opcode = Opcode::kMove;
        moveInst.args.push_back(nextInst->args[0]); // 結果
        moveInst.args.push_back(inst.args[1]);      // a
        
        optimizedInstructions.push_back(moveInst);
        i++;
        changed = true;
        continue;
      }
    }
    
    // デフォルト: 通常の命令を追加
    optimizedInstructions.push_back(inst);
  }
  
  if (changed) {
    function.Clear();
    for (const auto& inst : optimizedInstructions) {
      function.AddInstruction(inst);
    }
  }
  
  return changed;
}

// ループ最適化
bool IROptimizer::LoopOptimization(IRFunction& function, uint32_t functionId) noexcept {
  bool changed = false;
  
  // ループ解析
  std::vector<std::pair<size_t, size_t>> loops; // <開始インデックス, 終了インデックス>
  
  // ベーシックブロック解析
  struct BasicBlock {
    size_t startIndex;
    size_t endIndex;
    std::vector<size_t> successors;
    std::vector<size_t> predecessors;
    bool isLoopHeader = false;
    bool isInLoop = false;
    int loopDepth = 0;
  };
  
  std::vector<BasicBlock> basicBlocks;
  std::unordered_map<size_t, size_t> labelToBlock;
  
  const auto& instructions = function.GetInstructions();
  size_t blockStart = 0;
  
  // ベーシックブロックを識別
  for (size_t i = 0; i < instructions.size(); ++i) {
    const auto& inst = instructions[i];
    
    // ジャンプなどの制御フロー命令があれば、現在のブロック終了
    if (inst.opcode == Opcode::kJump ||
        inst.opcode == Opcode::kJumpIfTrue ||
        inst.opcode == Opcode::kJumpIfFalse ||
        inst.opcode == Opcode::kReturn) {
      
      BasicBlock block;
      block.startIndex = blockStart;
      block.endIndex = i;
      basicBlocks.push_back(block);
      
      blockStart = i + 1;
    }
    
    // ラベルはブロック開始点
    if (inst.opcode == Opcode::kLabel) {
      if (blockStart != i) {
        BasicBlock block;
        block.startIndex = blockStart;
        block.endIndex = i - 1;
        basicBlocks.push_back(block);
      }
      
      labelToBlock[inst.args[0]] = basicBlocks.size();
      blockStart = i;
    }
  }
  
  // 最後のブロックを追加
  if (blockStart < instructions.size()) {
    BasicBlock block;
    block.startIndex = blockStart;
    block.endIndex = instructions.size() - 1;
    basicBlocks.push_back(block);
  }
  
  // 後続と先行を識別
  for (size_t i = 0; i < basicBlocks.size(); ++i) {
    BasicBlock& block = basicBlocks[i];
    const auto& lastInst = instructions[block.endIndex];
    
    if (lastInst.opcode == Opcode::kJump) {
      auto targetIt = labelToBlock.find(lastInst.args[0]);
      if (targetIt != labelToBlock.end()) {
        block.successors.push_back(targetIt->second);
        basicBlocks[targetIt->second].predecessors.push_back(i);
      }
    } else if (lastInst.opcode == Opcode::kJumpIfTrue || lastInst.opcode == Opcode::kJumpIfFalse) {
      // Fall-through
      if (i + 1 < basicBlocks.size()) {
        block.successors.push_back(i + 1);
        basicBlocks[i + 1].predecessors.push_back(i);
      }
      
      // Jump target
      auto targetIt = labelToBlock.find(lastInst.args[1]);
      if (targetIt != labelToBlock.end()) {
        block.successors.push_back(targetIt->second);
        basicBlocks[targetIt->second].predecessors.push_back(i);
      }
    } else if (lastInst.opcode != Opcode::kReturn) {
      // Not a return, fall through
      if (i + 1 < basicBlocks.size()) {
        block.successors.push_back(i + 1);
        basicBlocks[i + 1].predecessors.push_back(i);
      }
    }
  }
  
  // ループヘッダを識別（自分自身に戻るエッジを持つ）
  for (size_t i = 0; i < basicBlocks.size(); ++i) {
    BasicBlock& block = basicBlocks[i];
    for (auto succ : block.successors) {
      if (succ <= i && !basicBlocks[succ].isLoopHeader) {
        basicBlocks[succ].isLoopHeader = true;
        
        // ループの一部として、ヘッダからそのブロックまでの全てのブロックをマーク
        std::vector<bool> visited(basicBlocks.size(), false);
        std::function<void(size_t, int)> markLoop = [&](size_t blockIdx, int depth) {
          if (visited[blockIdx]) return;
          visited[blockIdx] = true;
          
          BasicBlock& b = basicBlocks[blockIdx];
          b.isInLoop = true;
          b.loopDepth = std::max(b.loopDepth, depth);
          
          for (auto s : b.successors) {
            if (s != succ) { // ループバックエッジ以外を追跡
              markLoop(s, depth);
            }
          }
        };
        
        markLoop(succ, basicBlocks[succ].loopDepth + 1);
        
        // ループの範囲を記録
        size_t loopStart = succ;
        size_t loopEnd = i;
        loops.push_back({loopStart, loopEnd});
      }
    }
  }
  
  // ループ内での不変式（変更されないレジスタや操作）を識別
  for (auto [loopStart, loopEnd] : loops) {
    std::unordered_set<int32_t> invariantRegs;
    std::unordered_set<int32_t> modifiedRegs;
    
    // 最初のパス: ループ内で変更されるレジスタを特定
    for (size_t i = loopStart; i <= loopEnd; ++i) {
      const auto& block = basicBlocks[i];
      for (size_t j = block.startIndex; j <= block.endIndex; ++j) {
        const auto& inst = instructions[j];
        
        // 結果レジスタをマーク
        if (!inst.args.empty()) {
          modifiedRegs.insert(inst.args[0]);
        }
      }
    }
    
    // 2番目のパス: ループ不変式を特定
    for (size_t i = loopStart; i <= loopEnd; ++i) {
      const auto& block = basicBlocks[i];
      for (size_t j = block.startIndex; j <= block.endIndex; ++j) {
        const auto& inst = instructions[j];
        
        // 入力がすべてループ不変かチェック
        bool allInputsInvariant = true;
        for (size_t k = 1; k < inst.args.size(); ++k) {
          if (modifiedRegs.find(inst.args[k]) != modifiedRegs.end()) {
            allInputsInvariant = false;
            break;
          }
        }
        
        if (allInputsInvariant && !inst.hasMemorySideEffects()) {
          invariantRegs.insert(inst.args[0]);
        }
      }
    }
    
    // ループ不変式のホイスト
    if (!invariantRegs.empty()) {
      std::vector<IRInstruction> optimizedInstructions;
      std::unordered_set<size_t> hoistedIndices;
      
      // ループ前にホイスト
      for (size_t i = loopStart; i <= loopEnd; ++i) {
        const auto& block = basicBlocks[i];
        for (size_t j = block.startIndex; j <= block.endIndex; ++j) {
          const auto& inst = instructions[j];
          
          if (!inst.args.empty() && invariantRegs.find(inst.args[0]) != invariantRegs.end()) {
            optimizedInstructions.push_back(inst);
            hoistedIndices.insert(j);
          }
        }
      }
      
      // ループとその他の部分を結合
      for (size_t i = 0; i < instructions.size(); ++i) {
        if (hoistedIndices.find(i) == hoistedIndices.end()) {
          optimizedInstructions.push_back(instructions[i]);
        }
      }
      
      function.Clear();
      for (const auto& inst : optimizedInstructions) {
        function.AddInstruction(inst);
      }
      
      changed = true;
    }
  }
  
  return changed;
}

// 型特化最適化
bool IROptimizer::TypeSpecialization(IRFunction& function, uint32_t functionId) noexcept {
  // 型安定性が高い関数に対してのみ実行
  const ProfileData* profileData = ExecutionProfiler::Instance().GetProfileData(functionId);
  if (!profileData || !profileData->isStable) {
    return false;
  }
  
  bool changed = false;
  const auto& instructions = function.GetInstructions();
  std::vector<IRInstruction> optimizedInstructions;
  
  // 整数特化レジスタの追跡
  std::unordered_set<int32_t> intRegisters;
  std::unordered_set<int32_t> floatRegisters;
  
  // プロファイルデータから型情報を取得
  for (const auto& typeRecord : profileData->typeRecords) {
    if (typeRecord.type == TypeFeedbackRecord::TypeCategory::Integer) {
      intRegisters.insert(typeRecord.registerIndex);
    } else if (typeRecord.type == TypeFeedbackRecord::TypeCategory::Float) {
      floatRegisters.insert(typeRecord.registerIndex);
    }
  }
  
  // 命令を型特化
  for (const auto& inst : instructions) {
    IRInstruction optimizedInst = inst;
    
    // 整数特化
    if (inst.args.size() > 0 && intRegisters.find(inst.args[0]) != intRegisters.end()) {
      switch (inst.opcode) {
        case Opcode::kAdd:
          optimizedInst.opcode = Opcode::kAddInt;
          break;
        case Opcode::kSub:
          optimizedInst.opcode = Opcode::kSubInt;
          break;
        case Opcode::kMul:
          optimizedInst.opcode = Opcode::kMulInt;
          break;
        case Opcode::kDiv:
          optimizedInst.opcode = Opcode::kDivInt;
          break;
        case Opcode::kMod:
          optimizedInst.opcode = Opcode::kModInt;
          break;
        default:
          break;
      }
    }
    // 浮動小数点特化
    else if (inst.args.size() > 0 && floatRegisters.find(inst.args[0]) != floatRegisters.end()) {
      switch (inst.opcode) {
        case Opcode::kAdd:
          optimizedInst.opcode = Opcode::kAddFloat;
          break;
        case Opcode::kSub:
          optimizedInst.opcode = Opcode::kSubFloat;
          break;
        case Opcode::kMul:
          optimizedInst.opcode = Opcode::kMulFloat;
          break;
        case Opcode::kDiv:
          optimizedInst.opcode = Opcode::kDivFloat;
          break;
        default:
          break;
      }
    }
    
    if (optimizedInst.opcode != inst.opcode) {
      changed = true;
    }
    
    optimizedInstructions.push_back(optimizedInst);
  }
  
  if (changed) {
    function.Clear();
    for (const auto& inst : optimizedInstructions) {
      function.AddInstruction(inst);
    }
  }
  
  return changed;
}

}  // namespace core
}  // namespace aerojs 