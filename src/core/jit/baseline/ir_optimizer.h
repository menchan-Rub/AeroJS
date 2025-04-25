#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <string>
#include <bitset>

#include "../ir/ir.h"
#include "../ir/ir_builder.h"

namespace aerojs {
namespace core {

/**
 * @brief 最適化パスを表す列挙型
 */
enum class OptimizationPass : uint32_t {
  kConstantFolding         = 0,   // 定数畳み込み
  kConstantPropagation     = 1,   // 定数伝播
  kDeadCodeElimination     = 2,   // デッドコード除去
  kCommonSubexprElimination = 3,  // 共通部分式除去
  kCopyPropagation         = 4,   // コピー伝播
  kInstructionCombining    = 5,   // 命令結合
  kLoopInvariantCodeMotion = 6,   // ループ不変コード移動
  kInlineExpansion         = 7,   // インライン展開
  kValueNumbering         = 8,    // 値番号付け
  kDeadStoreElimination    = 9,   // デッドストア除去
  kRedundantLoadElimination = 10, // 冗長ロード除去
  kStrengthReduction       = 11,  // 強度削減
  kTailCallOptimization    = 12,  // 末尾呼び出し最適化
  kBranchOptimization      = 13,  // 分岐最適化
  kLoopUnrolling           = 14,  // ループ展開
  kHoisting                = 15,  // コード引き上げ
  kRegisterPromotion       = 16,  // レジスタ昇格
  kLoadStoreOptimization   = 17,  // ロード・ストア最適化
  kPeephole                = 18,  // ピープホール最適化
  kTypeSpecialization      = 19,  // 型特化
  kLoopVectorization       = 20,  // ループベクトル化
  kFunctionInlining        = 21,  // 関数インライン化
  kMemoryAccessOptimization = 22, // メモリアクセス最適化
  kMax                     = 23   // 最大値（最適化パスの数）
};

/**
 * @brief 最適化レベルを表す列挙型
 */
enum class OptimizationLevel : uint8_t {
  kNone,    // 最適化なし
  kO1,      // 基本的な最適化
  kO2,      // 中レベルの最適化
  kO3,      // 高レベルの最適化
  kSize,    // サイズ優先の最適化
  kSpeed    // 速度優先の最適化
};

/**
 * @brief IR最適化器の統計情報
 */
struct OptimizationStats {
  uint32_t iterationCount;                // 全体の繰り返し回数
  std::vector<uint32_t> passIterations;   // 各パスの繰り返し回数
  std::vector<uint32_t> changesPerPass;   // 各パスによる変更回数
  uint64_t totalTimeNs;                   // 全体の実行時間（ナノ秒）
  std::vector<uint64_t> timePerPassNs;    // 各パスの実行時間（ナノ秒）
  
  OptimizationStats() : iterationCount(0), totalTimeNs(0) {
    passIterations.resize(static_cast<size_t>(OptimizationPass::kMax), 0);
    changesPerPass.resize(static_cast<size_t>(OptimizationPass::kMax), 0);
    timePerPassNs.resize(static_cast<size_t>(OptimizationPass::kMax), 0);
  }
};

/**
 * @brief IR最適化器クラス
 *
 * IRに対して各種最適化を適用し、コード性能を向上させる責務を持ちます。
 * 複数の最適化パスをサポートし、最適化レベルに応じて適用するパスを選択します。
 */
class IROptimizer {
public:
  /**
   * @brief コンストラクタ
   * @param level 最適化レベル
   */
  explicit IROptimizer(OptimizationLevel level = OptimizationLevel::kO2) noexcept;
  
  /**
   * @brief デストラクタ
   */
  ~IROptimizer() noexcept;
  
  /**
   * @brief IR関数に対して最適化を実行する
   * @param function 最適化対象のIR関数
   * @return 最適化によって変更があった場合true
   */
  bool Optimize(IRFunction& function) noexcept;
  
  /**
   * @brief 特定の最適化パスを有効/無効にする
   * @param pass 設定する最適化パス
   * @param enabled 有効にする場合true
   */
  void SetPassEnabled(OptimizationPass pass, bool enabled) noexcept;
  
  /**
   * @brief 最適化レベルを設定する
   * @param level 設定する最適化レベル
   */
  void SetOptimizationLevel(OptimizationLevel level) noexcept;
  
  /**
   * @brief 最適化の統計情報を取得する
   * @return 統計情報
   */
  const OptimizationStats& GetStats() const noexcept;
  
  /**
   * @brief 統計情報をリセットする
   */
  void ResetStats() noexcept;
  
  /**
   * @brief 最大繰り返し回数を設定する
   * @param count 最大繰り返し回数
   */
  void SetMaxIterations(uint32_t count) noexcept;
  
  /**
   * @brief コスト閾値を設定する
   * @param threshold コスト閾値（この値を超える最適化は適用しない）
   */
  void SetCostThreshold(int32_t threshold) noexcept;

private:
  /**
   * @brief 定数畳み込みを実行する
   * @param function 対象関数
   * @return 変更があった場合true
   */
  bool RunConstantFolding(IRFunction& function) noexcept;
  
  /**
   * @brief 定数伝播を実行する
   * @param function 対象関数
   * @return 変更があった場合true
   */
  bool RunConstantPropagation(IRFunction& function) noexcept;
  
  /**
   * @brief デッドコード除去を実行する
   * @param function 対象関数
   * @return 変更があった場合true
   */
  bool RunDeadCodeElimination(IRFunction& function) noexcept;
  
  /**
   * @brief 共通部分式除去を実行する
   * @param function 対象関数
   * @return 変更があった場合true
   */
  bool RunCommonSubexprElimination(IRFunction& function) noexcept;
  
  /**
   * @brief コピー伝播を実行する
   * @param function 対象関数
   * @return 変更があった場合true
   */
  bool RunCopyPropagation(IRFunction& function) noexcept;
  
  /**
   * @brief 命令結合を実行する
   * @param function 対象関数
   * @return 変更があった場合true
   */
  bool RunInstructionCombining(IRFunction& function) noexcept;
  
  /**
   * @brief ループ不変コード移動を実行する
   * @param function 対象関数
   * @return 変更があった場合true
   */
  bool RunLoopInvariantCodeMotion(IRFunction& function) noexcept;
  
  /**
   * @brief 制御フロー解析を実行する
   * @param function 対象関数
   */
  void AnalyzeControlFlow(IRFunction& function) noexcept;
  
  /**
   * @brief データフロー解析を実行する
   * @param function 対象関数
   */
  void AnalyzeDataFlow(IRFunction& function) noexcept;
  
  /**
   * @brief 使用-定義連鎖を構築する
   * @param function 対象関数
   */
  void BuildUseDefChains(IRFunction& function) noexcept;
  
  /**
   * @brief 最適化レベルに基づいて最適化パスを設定する
   * @param level 最適化レベル
   */
  void ConfigurePassesForLevel(OptimizationLevel level) noexcept;
  
  /**
   * @brief 指定されたパスが有効かどうかを取得する
   * @param pass チェックする最適化パス
   * @return 有効な場合true
   */
  bool IsPassEnabled(OptimizationPass pass) const noexcept;
  
  /**
   * @brief 最適化パスを実行し、統計情報を更新する
   * @param pass 実行する最適化パス
   * @param function 対象関数
   * @return 変更があった場合true
   */
  bool RunOptimizationPass(OptimizationPass pass, IRFunction& function) noexcept;

private:
  // 最適化レベル
  OptimizationLevel m_level;
  
  // 有効な最適化パスを示すビットセット
  std::bitset<static_cast<size_t>(OptimizationPass::kMax)> m_enabledPasses;
  
  // 最適化パスの実行順序
  std::vector<OptimizationPass> m_passOrder;
  
  // 最大繰り返し回数
  uint32_t m_maxIterations;
  
  // コスト閾値
  int32_t m_costThreshold;
  
  // 統計情報
  OptimizationStats m_stats;
  
  // 使用-定義連鎖のキャッシュ
  std::unordered_map<uint32_t, std::vector<size_t>> m_defUseMap;
  std::unordered_map<uint32_t, std::vector<size_t>> m_useDefMap;
  
  // 制御フロー情報
  struct {
    std::vector<std::vector<size_t>> predecessors;  // 各ブロックの先行者
    std::vector<std::vector<size_t>> successors;    // 各ブロックの後続者
    std::vector<size_t> blockEntries;               // 各ブロックの開始命令インデックス
    std::vector<size_t> blockExits;                 // 各ブロックの終了命令インデックス
    std::vector<bool> isLoopHeader;                 // 各ブロックがループヘッダかどうか
  } m_cfg;
};

/**
 * @brief 最適化パスを文字列に変換する
 * @param pass 変換する最適化パス
 * @return パスの文字列表現
 */
std::string OptimizationPassToString(OptimizationPass pass);

/**
 * @brief 最適化レベルを文字列に変換する
 * @param level 変換する最適化レベル
 * @return レベルの文字列表現
 */
std::string OptimizationLevelToString(OptimizationLevel level);

} // namespace core
} // namespace aerojs 