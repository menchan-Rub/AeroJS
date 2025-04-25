#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <memory>

#include "src/core/jit/ir/ir.h"
#include "src/core/jit/ir/type_analyzer.h"

namespace aerojs::core {

/**
 * @brief IR最適化を行うクラス
 * 
 * IRの最適化パスを実行し、より効率的なコードを生成します。
 */
class IROptimizer {
public:
  IROptimizer() noexcept;
  ~IROptimizer() noexcept;
  
  /**
   * @brief 最適化レベルの設定
   */
  enum class OptimizationLevel {
    None,       ///< 最適化なし
    Basic,      ///< 基本的な最適化のみ（ベースラインJIT向け）
    Medium,     ///< 中程度の最適化（標準的な最適化JIT向け）
    Aggressive  ///< 積極的な最適化（超最適化JIT向け）
  };
  
  /**
   * @brief IRの最適化を実行する
   * @param irFunction 最適化対象のIR関数
   * @param functionId 関数ID（プロファイル情報取得用）
   * @param level 最適化レベル
   * @return 最適化されたIR関数
   */
  std::unique_ptr<IRFunction> Optimize(std::unique_ptr<IRFunction> irFunction,
                                      uint32_t functionId,
                                      OptimizationLevel level) noexcept;
  
  /**
   * @brief 最適化パスの型定義
   */
  using OptimizationPass = std::function<bool(IRFunction&, uint32_t)>;
  
  /**
   * @brief 内部状態をリセットする
   */
  void Reset() noexcept;

private:
  // 最適化パスを登録するマップ
  std::unordered_map<OptimizationLevel, std::vector<OptimizationPass>> m_optimizationPasses;
  
  // 最適化パスの初期化
  void InitOptimizationPasses() noexcept;
  
  // 各種最適化パスの実装
  bool ConstantFolding(IRFunction& function, uint32_t functionId) noexcept;
  bool DeadCodeElimination(IRFunction& function, uint32_t functionId) noexcept;
  bool CommonSubexpressionElimination(IRFunction& function, uint32_t functionId) noexcept;
  bool InstructionCombining(IRFunction& function, uint32_t functionId) noexcept;
  bool TypeSpecialization(IRFunction& function, uint32_t functionId) noexcept;
  bool LoopOptimization(IRFunction& function, uint32_t functionId) noexcept;
};

// 最適化パスの基底クラス
class OptimizationPass {
public:
    virtual ~OptimizationPass() = default;
    
    // 最適化パスの実行
    virtual bool run(IRFunction* function) = 0;
    
    // 最適化パスの名前
    virtual std::string name() const = 0;
};

// 定数畳み込み最適化
class ConstantFoldingPass : public OptimizationPass {
public:
    ConstantFoldingPass();
    ~ConstantFoldingPass() override;
    
    bool run(IRFunction* function) override;
    std::string name() const override { return "定数畳み込み"; }
};

// 共通部分式削除
class CommonSubexpressionEliminationPass : public OptimizationPass {
public:
    CommonSubexpressionEliminationPass();
    ~CommonSubexpressionEliminationPass() override;
    
    bool run(IRFunction* function) override;
    std::string name() const override { return "共通部分式削除"; }
};

// デッドコード削除
class DeadCodeEliminationPass : public OptimizationPass {
public:
    DeadCodeEliminationPass();
    ~DeadCodeEliminationPass() override;
    
    bool run(IRFunction* function) override;
    std::string name() const override { return "デッドコード削除"; }
};

// ループ不変式のホイスト
class LoopInvariantCodeMotionPass : public OptimizationPass {
public:
    LoopInvariantCodeMotionPass();
    ~LoopInvariantCodeMotionPass() override;
    
    bool run(IRFunction* function) override;
    std::string name() const override { return "ループ不変式のホイスト"; }
};

// 型特化最適化
class TypeSpecializationPass : public OptimizationPass {
public:
    TypeSpecializationPass();
    ~TypeSpecializationPass() override;
    
    bool run(IRFunction* function) override;
    std::string name() const override { return "型特化"; }
    
private:
    TypeAnalyzer m_typeAnalyzer;
};

}  // namespace aerojs::core

namespace aerojs {
namespace core {

}  // namespace core
}  // namespace aerojs 