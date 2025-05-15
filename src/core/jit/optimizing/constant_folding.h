/**
 * @file constant_folding.h
 * @brief AeroJS JavaScript エンジンの定数畳み込み最適化
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include <cstdint>
#include <memory>
#include "../ir/ir.h"

namespace aerojs {
namespace core {
namespace jit {

/**
 * @brief 定数畳み込み最適化パス
 *
 * IRFunction内の定数式を検出し、コンパイル時に計算する最適化パス。
 * 例えば、`Add(Const(2), Const(3))` という式は `Const(5)` に置き換えられる。
 * サポートする演算: 算術演算、比較演算、論理演算、ビット演算、Move命令など
 */
class ConstantFolding {
public:
  /**
   * @brief IRFunctionに対して定数畳み込み最適化を実行
   * @param func 最適化対象のIRFunction
   * @return 最適化により定数畳み込みが行われた回数
   */
  static int32_t Run(IRFunction& func);

  /**
   * @brief 初期化
   */
  static void Initialize();
  
  /**
   * @brief シャットダウン
   */
  static void Shutdown();
  
  /**
   * @brief 最適化レベルを設定
   * 
   * @param level 最適化レベル (0-3)
   */
  static void SetOptimizationLevel(int level);
  
  /**
   * @brief 現在の最適化レベルを取得
   * 
   * @return 最適化レベル
   */
  static int GetOptimizationLevel();
  
  /**
   * @brief 浮動小数点畳み込みを有効/無効化
   * 
   * @param enable 有効化する場合はtrue
   */
  static void EnableFloatingPointFolding(bool enable);
  
  /**
   * @brief 副作用を持つ可能性のある命令の畳み込みを有効/無効化
   * 
   * @param enable 有効化する場合はtrue
   */
  static void EnableAggressiveFolding(bool enable);

private:
  /**
   * @brief 定数式かどうかを判定
   * @param inst 対象の命令
   * @return 定数式の場合はtrue
   */
  static bool IsConstantExpression(const IRInstruction& inst);

  /**
   * @brief 定数式を評価
   * @param inst 対象の命令
   * @return 評価結果の定数命令
   */
  static IRInstruction FoldConstantExpression(const IRInstruction& inst);

  /**
   * @brief 算術演算を定数畳み込み
   * @param inst 対象の算術命令
   * @return 畳み込み結果の定数命令
   */
  static IRInstruction FoldArithmeticOp(const IRInstruction& inst);

  /**
   * @brief 比較演算を定数畳み込み
   * @param inst 対象の比較命令
   * @return 畳み込み結果の定数命令
   */
  static IRInstruction FoldComparisonOp(const IRInstruction& inst);

  /**
   * @brief 論理演算を定数畳み込み
   * @param inst 対象の論理命令
   * @return 畳み込み結果の定数命令
   */
  static IRInstruction FoldLogicalOp(const IRInstruction& inst);

  /**
   * @brief ビット演算を定数畳み込み
   * @param inst 対象のビット命令
   * @return 畳み込み結果の定数命令
   */
  static IRInstruction FoldBitwiseOp(const IRInstruction& inst);

  /**
   * @brief 定数値を取得
   * @param value 対象のIRValue
   * @return 定数値
   */
  static IRConstant GetConstantValue(const IRValue& value);

  // 内部状態
  static int s_optimizationLevel;
  static bool s_enableFloatingPointFolding;
  static bool s_enableAggressiveFolding;
  
  // 具体的な最適化実装
  static bool FoldConstants(IRFunction& function);
  static bool FoldArithmeticOps(IRFunction& function);
  static bool FoldComparisonOps(IRFunction& function);
  static bool FoldLogicalOps(IRFunction& function);
  static bool FoldConversions(IRFunction& function);
  static bool FoldMemoryOps(IRFunction& function);
  
  // 定数式の評価
  static std::optional<int> EvaluateIntExpression(const IRInstruction& inst, 
                                                 const std::unordered_map<int, int>& constMap);
  static std::optional<double> EvaluateFloatExpression(const IRInstruction& inst, 
                                                      const std::unordered_map<int, double>& constMap);
  static std::optional<bool> EvaluateBoolExpression(const IRInstruction& inst, 
                                                   const std::unordered_map<int, int>& constMap);
  
  // 定数を追跡し、定数伝播を行う
  static void TrackConstants(const IRFunction& function, 
                            std::unordered_map<int, int>& intConstants,
                            std::unordered_map<int, double>& floatConstants,
                            std::unordered_map<int, bool>& boolConstants);
};

} // namespace jit
} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_OPTIMIZING_CONSTANT_FOLDING_H 