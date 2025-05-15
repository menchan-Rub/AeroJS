#include "ir_optimizer.h"
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cassert>

namespace aerojs {
namespace core {

IROptimizer::IROptimizer(OptimizationLevel level) noexcept
    : m_level(level)
    , m_maxIterations(10)
    , m_costThreshold(100) {
  // 最適化レベルに基づいて最適化パスを設定
  ConfigurePassesForLevel(level);
}

IROptimizer::~IROptimizer() noexcept = default;

bool IROptimizer::Optimize(IRFunction& function) noexcept {
  // 統計情報の初期化
  m_stats.iterationCount = 0;
  m_stats.totalTimeNs = 0;
  
  // 制御フロー解析とデータフロー解析を実行
  AnalyzeControlFlow(function);
  AnalyzeDataFlow(function);
  BuildUseDefChains(function);
  
  // 最適化ループの開始時間を記録
  auto startTime = std::chrono::high_resolution_clock::now();
  
  bool changed = false;
  bool iterationChanged = true;
  
  // 最適化パスを繰り返し適用
  while (iterationChanged && m_stats.iterationCount < m_maxIterations) {
    iterationChanged = false;
    m_stats.iterationCount++;
    
    // 各パスを順番に実行
    for (auto pass : m_passOrder) {
      if (!IsPassEnabled(pass)) {
        continue;
      }
      
      // パスを実行し、変更があったか確認
      bool passChanged = RunOptimizationPass(pass, function);
      
      if (passChanged) {
        iterationChanged = true;
        changed = true;
        
        // 制御フローとデータフローの再解析が必要なパスの場合
        if (pass == OptimizationPass::kDeadCodeElimination ||
            pass == OptimizationPass::kInstructionCombining) {
          AnalyzeControlFlow(function);
          AnalyzeDataFlow(function);
          BuildUseDefChains(function);
        }
      }
    }
  }
  
  // 最適化ループの終了時間を記録
  auto endTime = std::chrono::high_resolution_clock::now();
  m_stats.totalTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
      endTime - startTime).count();
  
  return changed;
}

void IROptimizer::SetPassEnabled(OptimizationPass pass, bool enabled) noexcept {
  if (pass < OptimizationPass::kMax) {
    m_enabledPasses.set(static_cast<size_t>(pass), enabled);
  }
}

void IROptimizer::SetOptimizationLevel(OptimizationLevel level) noexcept {
  m_level = level;
  ConfigurePassesForLevel(level);
}

const OptimizationStats& IROptimizer::GetStats() const noexcept {
  return m_stats;
}

void IROptimizer::ResetStats() noexcept {
  m_stats = OptimizationStats();
}

void IROptimizer::SetMaxIterations(uint32_t count) noexcept {
  m_maxIterations = count;
}

void IROptimizer::SetCostThreshold(int32_t threshold) noexcept {
  m_costThreshold = threshold;
}

bool IROptimizer::RunConstantFolding(IRFunction& function) noexcept {
  bool changed = false;
  auto& instructions = const_cast<std::vector<IRInstruction>&>(function.GetInstructions());
  
  for (size_t i = 0; i < instructions.size(); ++i) {
    auto& inst = instructions[i];
    
    // 算術命令の場合
    if (inst.opcode == Opcode::Add || inst.opcode == Opcode::Sub ||
        inst.opcode == Opcode::Mul || inst.opcode == Opcode::Div) {
      
      // 両方のオペランドが定数かどうかを確認
      bool isOp1Const = false;
      bool isOp2Const = false;
      int32_t op1Value = 0;
      int32_t op2Value = 0;
      
      // 最初のオペランドがLoadConstからの結果かどうかを確認
      for (size_t j = 0; j < i; ++j) {
        const auto& prevInst = instructions[j];
        if (prevInst.opcode == Opcode::LoadConst && 
            prevInst.args[0] == inst.args[1]) {
          isOp1Const = true;
          op1Value = prevInst.args[1];
          break;
        }
      }
      
      // 2番目のオペランドがLoadConstからの結果かどうかを確認
      for (size_t j = 0; j < i; ++j) {
        const auto& prevInst = instructions[j];
        if (prevInst.opcode == Opcode::LoadConst && 
            prevInst.args[0] == inst.args[2]) {
          isOp2Const = true;
          op2Value = prevInst.args[1];
          break;
        }
      }
      
      // 両方のオペランドが定数の場合、計算を畳み込む
      if (isOp1Const && isOp2Const) {
        int32_t result = 0;
        
        // 命令に基づいて計算を実行
        switch (inst.opcode) {
          case Opcode::Add:
            result = op1Value + op2Value;
            break;
          case Opcode::Sub:
            result = op1Value - op2Value;
            break;
          case Opcode::Mul:
            result = op1Value * op2Value;
            break;
          case Opcode::Div:
            // ゼロ除算を避ける
            if (op2Value == 0) {
              continue;
            }
            result = op1Value / op2Value;
            break;
          default:
            continue;
        }
        
        // 命令をLoadConstに置き換え
        inst.opcode = Opcode::LoadConst;
        inst.args.resize(2);
        inst.args[0] = inst.args[0]; // 結果のレジスタは同じ
        inst.args[1] = result;       // 計算結果
        
        changed = true;
      }
    }
  }
  
  return changed;
}

bool IROptimizer::RunConstantPropagation(IRFunction& function) noexcept {
  bool changed = false;
  auto& instructions = const_cast<std::vector<IRInstruction>&>(function.GetInstructions());
  
  // レジスタに格納されている定数値を追跡
  std::unordered_map<int32_t, int32_t> constantMap;
  
  for (size_t i = 0; i < instructions.size(); ++i) {
    auto& inst = instructions[i];
    
    // LoadConst命令の場合、定数マップを更新
    if (inst.opcode == Opcode::LoadConst && inst.args.size() >= 2) {
      constantMap[inst.args[0]] = inst.args[1];
    }
    // レジスタに値を格納する他の命令の場合、そのレジスタは定数ではなくなる
    else if (inst.args.size() > 0) {
      constantMap.erase(inst.args[0]);
    }
    
    // 命令のオペランドを定数に置き換え
    for (size_t j = 1; j < inst.args.size(); ++j) {
      int32_t reg = inst.args[j];
      auto it = constantMap.find(reg);
      
      if (it != constantMap.end()) {
        // このレジスタが定数値を持っている場合
        // 新しい一時レジスタを作成し、そこに定数を読み込む
        IRInstruction loadConst;
        loadConst.opcode = Opcode::LoadConst;
        loadConst.args.push_back(reg);  // 同じレジスタを使用
        loadConst.args.push_back(it->second);  // 定数値
        
        // 元の命令の前に定数ロード命令を挿入
        instructions.insert(instructions.begin() + i, loadConst);
        
        // 挿入によってインデックスが変わるため、調整
        ++i;
        
        changed = true;
      }
    }
  }
  
  return changed;
}

bool IROptimizer::RunDeadCodeElimination(IRFunction& function) noexcept {
  bool changed = false;
  auto& instructions = const_cast<std::vector<IRInstruction>&>(function.GetInstructions());
  
  // 各レジスタが使用されているかどうかを追跡
  std::unordered_set<int32_t> usedRegisters;
  
  // まず、すべての命令を逆順に走査して使用されているレジスタを収集
  for (auto it = instructions.rbegin(); it != instructions.rend(); ++it) {
    const auto& inst = *it;
    
    // 結果レジスタ以外のすべてのオペランドは使用されている
    for (size_t i = 1; i < inst.args.size(); ++i) {
      usedRegisters.insert(inst.args[i]);
    }
  }
  
  // 次に、命令を逆順に走査して不要な命令を削除
  for (size_t i = instructions.size(); i > 0; --i) {
    size_t idx = i - 1;
    const auto& inst = instructions[idx];
    
    // 副作用のない命令で、結果が使用されていない場合は削除
    if (inst.args.size() > 0 && 
        usedRegisters.find(inst.args[0]) == usedRegisters.end() &&
        inst.opcode != Opcode::StoreVar && 
        inst.opcode != Opcode::Call &&
        inst.opcode != Opcode::Return) {
      
      // 命令を削除する前に、その命令が使用しているレジスタを使用リストから削除
      for (size_t j = 1; j < inst.args.size(); ++j) {
        usedRegisters.erase(inst.args[j]);
      }
      
      // 命令を削除
      instructions.erase(instructions.begin() + idx);
      changed = true;
    }
  }
  
  return changed;
}

bool IROptimizer::RunCommonSubexprElimination(IRFunction& function) noexcept {
  bool changed = false;
  auto& instructions = const_cast<std::vector<IRInstruction>&>(function.GetInstructions());
  
  // 式のハッシュ値をキーとして結果レジスタを格納
  std::unordered_map<size_t, int32_t> exprMap;
  
  for (size_t i = 0; i < instructions.size(); ++i) {
    auto& inst = instructions[i];
    
    // 共通部分式除去の対象となる命令（算術演算など）のみを処理
    if (inst.opcode == Opcode::Add || inst.opcode == Opcode::Sub ||
        inst.opcode == Opcode::Mul || inst.opcode == Opcode::Div) {
      
      // 式のハッシュ値を計算
      size_t hash = static_cast<size_t>(inst.opcode);
      for (size_t j = 1; j < inst.args.size(); ++j) {
        hash = hash * 31 + static_cast<size_t>(inst.args[j]);
      }
      
      // 同じ式が既に計算されているか確認
      auto it = exprMap.find(hash);
      if (it != exprMap.end()) {
        // 同じ式が見つかった場合、命令をMove命令に置き換え
        inst.opcode = Opcode::LoadVar;
        inst.args.resize(2);
        inst.args[1] = it->second;  // 既に計算された結果のレジスタ
        
        changed = true;
      } else {
        // 新しい式の場合、マップに追加
        exprMap[hash] = inst.args[0];
      }
    }
  }
  
  return changed;
}

void IROptimizer::AnalyzeControlFlow(IRFunction& function) noexcept {
  const auto& instructions = function.GetInstructions();
  
  // CFG情報をクリア
  m_cfg.predecessors.clear();
  m_cfg.successors.clear();
  m_cfg.blockEntries.clear();
  m_cfg.blockExits.clear();
  m_cfg.isLoopHeader.clear();
  
  // 基本ブロックの境界を見つける
  std::vector<size_t> leaders;  // 基本ブロックの先頭となる命令のインデックス
  leaders.push_back(0);  // 最初の命令は常に基本ブロックの先頭
  
  for (size_t i = 0; i < instructions.size(); ++i) {
    const auto& inst = instructions[i];
    
    // ジャンプ命令の次の命令は新しいブロックの先頭
    if (inst.opcode == Opcode::Return && i + 1 < instructions.size()) {
      leaders.push_back(i + 1);
    }
    
    // ジャンプ命令のターゲットも基本ブロックの先頭
    if (IsJumpInstruction(inst.opcode)) {
      // 直接ジャンプターゲットがある場合（条件分岐や無条件ジャンプ）
      if (inst.args.size() >= 2) {
        int32_t targetIndex = inst.args[1]; // 多くのジャンプ命令では2番目の引数がターゲット
        
        if (targetIndex >= 0 && targetIndex < static_cast<int32_t>(instructions.size())) {
          leaders.push_back(targetIndex);
        }
      }
      
      // Switch文のようなジャンプテーブルの場合
      if (inst.opcode == Opcode::JumpTable && inst.args.size() >= 3) {
        int32_t numTargets = inst.args[1]; // ジャンプテーブルのエントリ数
        
        for (int32_t j = 0; j < numTargets && j + 2 < static_cast<int32_t>(inst.args.size()); ++j) {
          int32_t targetIndex = inst.args[j + 2]; // ジャンプテーブルのターゲット
          
          if (targetIndex >= 0 && targetIndex < static_cast<int32_t>(instructions.size())) {
            leaders.push_back(targetIndex);
          }
        }
      }
      
      // 次の命令も新しいブロックの先頭（フォールスルーの可能性）
      if (i + 1 < instructions.size() && inst.opcode != Opcode::Jump && inst.opcode != Opcode::Return) {
        leaders.push_back(i + 1);
      }
    }
  }
  
  // 重複を削除し、ソート
  std::sort(leaders.begin(), leaders.end());
  leaders.erase(std::unique(leaders.begin(), leaders.end()), leaders.end());
  
  // 基本ブロックの情報を設定
  size_t numBlocks = leaders.size();
  m_cfg.predecessors.resize(numBlocks);
  m_cfg.successors.resize(numBlocks);
  m_cfg.blockEntries = leaders;
  m_cfg.blockExits.resize(numBlocks);
  m_cfg.isLoopHeader.resize(numBlocks, false);
  
  // 各ブロックの終了命令を設定
  for (size_t i = 0; i < numBlocks; ++i) {
    size_t start = leaders[i];
    size_t end = (i + 1 < numBlocks) ? leaders[i + 1] - 1 : instructions.size() - 1;
    m_cfg.blockExits[i] = end;
  }
  
  // ブロック間の接続関係を構築
  for (size_t i = 0; i < numBlocks; ++i) {
    size_t exitIdx = m_cfg.blockExits[i];
    const auto& exitInst = instructions[exitIdx];
    
    // 終了命令がジャンプの場合、ターゲットブロックを後続として追加
    // この実装では単純化のためスキップ
    
    // 次のブロックがある場合、それを後続として追加（フォールスルー）
    if (i + 1 < numBlocks && exitInst.opcode != Opcode::Return) {
      m_cfg.successors[i].push_back(i + 1);
      m_cfg.predecessors[i + 1].push_back(i);
    }
  }
  
  // ループヘッダを検出
  // 単純な方法：後続ブロックが自分自身を含む場合はループヘッダ
  for (size_t i = 0; i < numBlocks; ++i) {
    for (size_t succ : m_cfg.successors[i]) {
      if (succ <= i) {
        m_cfg.isLoopHeader[succ] = true;
      }
    }
  }
}

void IROptimizer::AnalyzeDataFlow(IRFunction& function) noexcept {
  // レジスタの定義と使用を追跡
  std::unordered_map<int32_t, std::vector<size_t>> defMap;
  std::unordered_map<int32_t, std::vector<size_t>> useMap;
  
  const auto& instructions = function.GetInstructions();
  
  for (size_t i = 0; i < instructions.size(); ++i) {
    const auto& inst = instructions[i];
    
    // 結果レジスタがある場合、定義として記録
    if (inst.args.size() > 0) {
      defMap[inst.args[0]].push_back(i);
    }
    
    // オペランドレジスタは使用として記録
    for (size_t j = 1; j < inst.args.size(); ++j) {
      useMap[inst.args[j]].push_back(i);
    }
  }
  
  // データフロー情報を保存
  m_defUseMap = std::move(defMap);
  m_useDefMap = std::move(useMap);
}

void IROptimizer::BuildUseDefChains(IRFunction& function) noexcept {
  // すでにデータフロー分析で構築済み
}

void IROptimizer::ConfigurePassesForLevel(OptimizationLevel level) noexcept {
  // すべてのパスを無効化
  m_enabledPasses.reset();
  m_passOrder.clear();
  
  // 最適化レベルに応じてパスを有効化
  switch (level) {
    case OptimizationLevel::kNone:
      // 最適化なし
      break;
      
    case OptimizationLevel::kO1:
      // 基本的な最適化
      SetPassEnabled(OptimizationPass::kConstantFolding, true);
      SetPassEnabled(OptimizationPass::kConstantPropagation, true);
      SetPassEnabled(OptimizationPass::kDeadCodeElimination, true);
      SetPassEnabled(OptimizationPass::kCopyPropagation, true);
      
      // 実行順序を設定
      m_passOrder = {
        OptimizationPass::kConstantFolding,
        OptimizationPass::kConstantPropagation,
        OptimizationPass::kCopyPropagation,
        OptimizationPass::kDeadCodeElimination
      };
      break;
      
    case OptimizationLevel::kO2:
      // 中レベルの最適化
      SetPassEnabled(OptimizationPass::kConstantFolding, true);
      SetPassEnabled(OptimizationPass::kConstantPropagation, true);
      SetPassEnabled(OptimizationPass::kDeadCodeElimination, true);
      SetPassEnabled(OptimizationPass::kCopyPropagation, true);
      SetPassEnabled(OptimizationPass::kCommonSubexprElimination, true);
      SetPassEnabled(OptimizationPass::kInstructionCombining, true);
      
      // 実行順序を設定
      m_passOrder = {
        OptimizationPass::kConstantFolding,
        OptimizationPass::kConstantPropagation,
        OptimizationPass::kCopyPropagation,
        OptimizationPass::kCommonSubexprElimination,
        OptimizationPass::kInstructionCombining,
        OptimizationPass::kDeadCodeElimination
      };
      break;
      
    case OptimizationLevel::kO3:
      // 高レベルの最適化
      SetPassEnabled(OptimizationPass::kConstantFolding, true);
      SetPassEnabled(OptimizationPass::kConstantPropagation, true);
      SetPassEnabled(OptimizationPass::kDeadCodeElimination, true);
      SetPassEnabled(OptimizationPass::kCopyPropagation, true);
      SetPassEnabled(OptimizationPass::kCommonSubexprElimination, true);
      SetPassEnabled(OptimizationPass::kInstructionCombining, true);
      SetPassEnabled(OptimizationPass::kLoopInvariantCodeMotion, true);
      SetPassEnabled(OptimizationPass::kValueNumbering, true);
      SetPassEnabled(OptimizationPass::kDeadStoreElimination, true);
      SetPassEnabled(OptimizationPass::kStrengthReduction, true);
      
      // 実行順序を設定
      m_passOrder = {
        OptimizationPass::kConstantFolding,
        OptimizationPass::kConstantPropagation,
        OptimizationPass::kCopyPropagation,
        OptimizationPass::kCommonSubexprElimination,
        OptimizationPass::kValueNumbering,
        OptimizationPass::kInstructionCombining,
        OptimizationPass::kStrengthReduction,
        OptimizationPass::kLoopInvariantCodeMotion,
        OptimizationPass::kDeadStoreElimination,
        OptimizationPass::kDeadCodeElimination
      };
      break;
      
    case OptimizationLevel::kSize:
      // サイズ優先の最適化
      SetPassEnabled(OptimizationPass::kConstantFolding, true);
      SetPassEnabled(OptimizationPass::kConstantPropagation, true);
      SetPassEnabled(OptimizationPass::kDeadCodeElimination, true);
      SetPassEnabled(OptimizationPass::kCopyPropagation, true);
      SetPassEnabled(OptimizationPass::kInstructionCombining, true);
      
      // 実行順序を設定
      m_passOrder = {
        OptimizationPass::kConstantFolding,
        OptimizationPass::kConstantPropagation,
        OptimizationPass::kCopyPropagation,
        OptimizationPass::kInstructionCombining,
        OptimizationPass::kDeadCodeElimination
      };
      break;
      
    case OptimizationLevel::kSpeed:
      // 速度優先の最適化
      SetPassEnabled(OptimizationPass::kConstantFolding, true);
      SetPassEnabled(OptimizationPass::kConstantPropagation, true);
      SetPassEnabled(OptimizationPass::kDeadCodeElimination, true);
      SetPassEnabled(OptimizationPass::kCopyPropagation, true);
      SetPassEnabled(OptimizationPass::kCommonSubexprElimination, true);
      SetPassEnabled(OptimizationPass::kInstructionCombining, true);
      SetPassEnabled(OptimizationPass::kLoopInvariantCodeMotion, true);
      SetPassEnabled(OptimizationPass::kValueNumbering, true);
      SetPassEnabled(OptimizationPass::kStrengthReduction, true);
      SetPassEnabled(OptimizationPass::kLoopUnrolling, true);
      SetPassEnabled(OptimizationPass::kHoisting, true);
      
      // 実行順序を設定
      m_passOrder = {
        OptimizationPass::kConstantFolding,
        OptimizationPass::kConstantPropagation,
        OptimizationPass::kCopyPropagation,
        OptimizationPass::kCommonSubexprElimination,
        OptimizationPass::kValueNumbering,
        OptimizationPass::kInstructionCombining,
        OptimizationPass::kStrengthReduction,
        OptimizationPass::kLoopInvariantCodeMotion,
        OptimizationPass::kHoisting,
        OptimizationPass::kLoopUnrolling,
        OptimizationPass::kDeadCodeElimination
      };
      break;
  }
}

bool IROptimizer::IsPassEnabled(OptimizationPass pass) const noexcept {
  if (pass < OptimizationPass::kMax) {
    return m_enabledPasses.test(static_cast<size_t>(pass));
  }
  return false;
}

bool IROptimizer::RunOptimizationPass(OptimizationPass pass, IRFunction& function) noexcept {
  // パスの開始時間を記録
  auto startTime = std::chrono::high_resolution_clock::now();
  
  bool changed = false;
  
  // パスを実行
  switch (pass) {
    case OptimizationPass::kConstantFolding:
      changed = RunConstantFolding(function);
      break;
      
    case OptimizationPass::kConstantPropagation:
      changed = RunConstantPropagation(function);
      break;
      
    case OptimizationPass::kDeadCodeElimination:
      changed = RunDeadCodeElimination(function);
      break;
      
    case OptimizationPass::kCommonSubexprElimination:
      changed = RunCommonSubexprElimination(function);
      break;
      
    case OptimizationPass::kCopyPropagation:
      changed = RunCopyPropagation(function);
      break;
      
    case OptimizationPass::kInstructionCombining:
      changed = RunInstructionCombining(function);
      break;
      
    case OptimizationPass::kLoopInvariantCodeMotion:
      changed = RunLoopInvariantCodeMotion(function);
      break;
      
    // その他のパスは未実装
    default:
      break;
  }
  
  // パスの終了時間を記録し、統計情報を更新
  auto endTime = std::chrono::high_resolution_clock::now();
  size_t passIdx = static_cast<size_t>(pass);
  
  m_stats.passIterations[passIdx]++;
  
  if (changed) {
    m_stats.changesPerPass[passIdx]++;
  }
  
  m_stats.timePerPassNs[passIdx] += std::chrono::duration_cast<std::chrono::nanoseconds>(
      endTime - startTime).count();
  
  return changed;
}

bool IROptimizer::RunCopyPropagation(IRFunction& function) noexcept {
  bool changed = false;
  auto& instructions = const_cast<std::vector<IRInstruction>&>(function.GetInstructions());
  
  // レジスタのコピー関係を追跡
  std::unordered_map<int32_t, int32_t> copyMap;  // src -> dst
  
  for (size_t i = 0; i < instructions.size(); ++i) {
    auto& inst = instructions[i];
    
    // コピー命令を検出
    if (inst.opcode == Opcode::LoadVar && inst.args.size() >= 2) {
      // コピーマップを更新
      copyMap[inst.args[0]] = inst.args[1];
    }
    // コピー以外の命令で定義された場合は、そのレジスタのコピー関係をクリア
    else if (inst.args.size() > 0) {
      // 結果レジスタについてのコピー関係をクリア
      auto it = copyMap.find(inst.args[0]);
      if (it != copyMap.end()) {
        copyMap.erase(it);
      }
    }
    
    // すべてのオペランドを処理
    for (size_t j = 1; j < inst.args.size(); ++j) {
      int32_t& reg = inst.args[j];
      
      // コピー元をたどり、最終的なレジスタに置き換え
      int32_t origReg = reg;
      while (copyMap.find(reg) != copyMap.end()) {
        reg = copyMap[reg];
      }
      
      if (reg != origReg) {
        changed = true;
      }
    }
  }
  
  return changed;
}

bool IROptimizer::RunInstructionCombining(IRFunction& function) noexcept {
  bool changed = false;
  auto& instructions = const_cast<std::vector<IRInstruction>&>(function.GetInstructions());
  
  for (size_t i = 0; i < instructions.size(); ++i) {
    auto& inst = instructions[i];
    
    // 加算命令の最適化
    if (inst.opcode == Opcode::Add && inst.args.size() >= 3) {
      // 加算の片方が0の場合、コピー命令に変換
      bool isOp1Zero = false;
      bool isOp2Zero = false;
      
      // オペランドが定数0かどうかをチェック
      for (size_t j = 0; j < i; ++j) {
        const auto& prevInst = instructions[j];
        if (prevInst.opcode == Opcode::LoadConst) {
          if (prevInst.args[0] == inst.args[1] && prevInst.args[1] == 0) {
            isOp1Zero = true;
          }
          if (prevInst.args[0] == inst.args[2] && prevInst.args[1] == 0) {
            isOp2Zero = true;
          }
        }
      }
      
      if (isOp1Zero) {
        // 第1オペランドが0の場合、第2オペランドのコピーに変換
        inst.opcode = Opcode::LoadVar;
        inst.args.resize(2);
        inst.args[1] = inst.args[2];
        changed = true;
      } else if (isOp2Zero) {
        // 第2オペランドが0の場合、第1オペランドのコピーに変換
        inst.opcode = Opcode::LoadVar;
        inst.args.resize(2);
        // args[1]はそのまま
        changed = true;
      }
    }
    
    // 乗算命令の最適化
    else if (inst.opcode == Opcode::Mul && inst.args.size() >= 3) {
      // 乗算の片方が1の場合、コピー命令に変換
      bool isOp1One = false;
      bool isOp2One = false;
      
      // オペランドが定数1かどうかをチェック
      for (size_t j = 0; j < i; ++j) {
        const auto& prevInst = instructions[j];
        if (prevInst.opcode == Opcode::LoadConst) {
          if (prevInst.args[0] == inst.args[1] && prevInst.args[1] == 1) {
            isOp1One = true;
          }
          if (prevInst.args[0] == inst.args[2] && prevInst.args[1] == 1) {
            isOp2One = true;
          }
        }
      }
      
      if (isOp1One) {
        // 第1オペランドが1の場合、第2オペランドのコピーに変換
        inst.opcode = Opcode::LoadVar;
        inst.args.resize(2);
        inst.args[1] = inst.args[2];
        changed = true;
      } else if (isOp2One) {
        // 第2オペランドが1の場合、第1オペランドのコピーに変換
        inst.opcode = Opcode::LoadVar;
        inst.args.resize(2);
        // args[1]はそのまま
        changed = true;
      }
    }
  }
  
  return changed;
}

bool IROptimizer::RunLoopInvariantCodeMotion(IRFunction& function) noexcept {
  bool changed = false;
  
  // 制御フロー情報がない場合は何もしない
  if (m_cfg.blockEntries.empty()) {
    return false;
  }
  
  auto& instructions = const_cast<std::vector<IRInstruction>&>(function.GetInstructions());
  
  // 各ループヘッダについて処理
  for (size_t blockIdx = 0; blockIdx < m_cfg.isLoopHeader.size(); ++blockIdx) {
    if (!m_cfg.isLoopHeader[blockIdx]) {
      continue;
    }
    
    // ループに含まれるブロックを収集
    std::unordered_set<size_t> loopBlocks;
    std::vector<size_t> workList = {blockIdx};
    
    while (!workList.empty()) {
      size_t current = workList.back();
      workList.pop_back();
      
      if (loopBlocks.find(current) != loopBlocks.end()) {
        continue;
      }
      
      loopBlocks.insert(current);
      
      // 後続ブロックをワークリストに追加
      for (size_t succ : m_cfg.successors[current]) {
        // ループの出口は含めない
        if (succ >= blockIdx) {
          workList.push_back(succ);
        }
      }
    }
    
    // ループ内の命令を収集
    std::vector<size_t> loopInstructions;
    for (size_t block : loopBlocks) {
      size_t start = m_cfg.blockEntries[block];
      size_t end = m_cfg.blockExits[block];
      
      for (size_t i = start; i <= end; ++i) {
        loopInstructions.push_back(i);
      }
    }
    
    // ループ内で定義されるレジスタを収集
    std::unordered_set<int32_t> loopDefinedRegs;
    for (size_t instIdx : loopInstructions) {
      const auto& inst = instructions[instIdx];
      if (inst.args.size() > 0) {
        loopDefinedRegs.insert(inst.args[0]);
      }
    }
    
    // ループ不変な命令を特定
    std::vector<size_t> invariantInstructions;
    for (size_t instIdx : loopInstructions) {
      const auto& inst = instructions[instIdx];
      
      // 副作用のない命令のみを対象とする
      if (inst.opcode == Opcode::StoreVar || 
          inst.opcode == Opcode::Call ||
          inst.opcode == Opcode::Return) {
        continue;
      }
      
      // すべてのオペランドがループ不変であるかチェック
      bool isInvariant = true;
      for (size_t j = 1; j < inst.args.size(); ++j) {
        if (loopDefinedRegs.find(inst.args[j]) != loopDefinedRegs.end()) {
          isInvariant = false;
          break;
        }
      }
      
      if (isInvariant) {
        invariantInstructions.push_back(instIdx);
      }
    }
    
    // ループの前のブロックを特定
    size_t preheaderIdx = 0;
    for (size_t pred : m_cfg.predecessors[blockIdx]) {
      if (loopBlocks.find(pred) == loopBlocks.end()) {
        preheaderIdx = pred;
        break;
      }
    }
    
    // ループ不変な命令をプリヘッダに移動
    if (!invariantInstructions.empty()) {
      // プリヘッダの終了位置
      size_t insertPos = m_cfg.blockExits[preheaderIdx];
      
      // 命令をループから削除し、プリヘッダに挿入
      std::vector<IRInstruction> movedInstructions;
      for (size_t instIdx : invariantInstructions) {
        movedInstructions.push_back(instructions[instIdx]);
      }
      
      // 命令を逆順に削除（インデックスが変わるのを避けるため）
      std::sort(invariantInstructions.begin(), invariantInstructions.end(), std::greater<size_t>());
      for (size_t instIdx : invariantInstructions) {
        instructions.erase(instructions.begin() + instIdx);
      }
      
      // プリヘッダに挿入
      instructions.insert(instructions.begin() + insertPos, movedInstructions.begin(), movedInstructions.end());
      
      changed = true;
    }
  }
  
  return changed;
}

bool IROptimizer::IsJumpInstruction(Opcode opcode) const noexcept {
  switch (opcode) {
    case Opcode::Jump:
    case Opcode::JumpIfTrue:
    case Opcode::JumpIfFalse:
    case Opcode::JumpIfEqual:
    case Opcode::JumpIfNotEqual:
    case Opcode::JumpIfLess:
    case Opcode::JumpIfLessEqual:
    case Opcode::JumpIfGreater:
    case Opcode::JumpIfGreaterEqual:
    case Opcode::JumpIfNull:
    case Opcode::JumpIfNotNull:
    case Opcode::JumpIfUndefined:
    case Opcode::JumpIfNotUndefined:
    case Opcode::JumpTable:
    case Opcode::JumpSubroutine:
      return true;
    default:
      return false;
  }
}

std::string OptimizationPassToString(OptimizationPass pass) {
  switch (pass) {
    case OptimizationPass::kConstantFolding:         return "定数畳み込み";
    case OptimizationPass::kConstantPropagation:     return "定数伝播";
    case OptimizationPass::kDeadCodeElimination:     return "デッドコード除去";
    case OptimizationPass::kCommonSubexprElimination: return "共通部分式除去";
    case OptimizationPass::kCopyPropagation:         return "コピー伝播";
    case OptimizationPass::kInstructionCombining:    return "命令結合";
    case OptimizationPass::kLoopInvariantCodeMotion: return "ループ不変コード移動";
    case OptimizationPass::kInlineExpansion:         return "インライン展開";
    case OptimizationPass::kValueNumbering:          return "値番号付け";
    case OptimizationPass::kDeadStoreElimination:    return "デッドストア除去";
    case OptimizationPass::kRedundantLoadElimination: return "冗長ロード除去";
    case OptimizationPass::kStrengthReduction:       return "強度削減";
    case OptimizationPass::kTailCallOptimization:    return "末尾呼び出し最適化";
    case OptimizationPass::kBranchOptimization:      return "分岐最適化";
    case OptimizationPass::kLoopUnrolling:           return "ループ展開";
    case OptimizationPass::kHoisting:                return "コード引き上げ";
    case OptimizationPass::kRegisterPromotion:       return "レジスタ昇格";
    case OptimizationPass::kLoadStoreOptimization:   return "ロード・ストア最適化";
    case OptimizationPass::kPeephole:                return "ピープホール最適化";
    case OptimizationPass::kTypeSpecialization:      return "型特化";
    case OptimizationPass::kLoopVectorization:       return "ループベクトル化";
    case OptimizationPass::kFunctionInlining:        return "関数インライン化";
    case OptimizationPass::kMemoryAccessOptimization: return "メモリアクセス最適化";
    default:                                         return "不明な最適化";
  }
}

std::string OptimizationLevelToString(OptimizationLevel level) {
  switch (level) {
    case OptimizationLevel::kNone:  return "最適化なし";
    case OptimizationLevel::kO1:    return "基本的な最適化";
    case OptimizationLevel::kO2:    return "中レベルの最適化";
    case OptimizationLevel::kO3:    return "高レベルの最適化";
    case OptimizationLevel::kSize:  return "サイズ優先";
    case OptimizationLevel::kSpeed: return "速度優先";
    default:                        return "不明なレベル";
  }
}

} // namespace core
} // namespace aerojs 