/*******************************************************************************
 * @file src/core/transformers/constant_folding.h
 * @version 1.0.0
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 * @brief 定数畳み込み (Constant Folding) 最適化を行う AST Transformer のインターフェース。
 *
 * この Transformer は、Abstract Syntax Tree (AST) を走査し、コンパイル時に
 * 値が確定する式 (定数式) を事前に計算し、その結果のリテラル値で置き換えます。
 * これにより、ランタイムの計算コストを削減し、パフォーマンスを向上させます。
 *
 * 対応する最適化の例:
 * - 算術演算: `1 + 2` -> `3`
 * - 文字列連結: `"a" + "b"` -> `"ab"`
 * - 論理演算: `true && x` -> `x`, `false || y` -> `y`
 * - 条件式: `true ? a : b` -> `a`
 * - 組み込み関数呼び出し: `Math.pow(2, 3)` -> `8`
 *
 * スレッド安全性: このクラスのインスタンスメソッドはスレッドセーフではありません。
 *              複数のスレッドから同時に transform メソッドを呼び出す場合は、
 *              スレッドごとに個別のインスタンスを使用するか、外部で排他制御を
 *              行う必要があります。
 ******************************************************************************/

#ifndef AEROJS_CORE_TRANSFORMERS_CONSTANT_FOLDING_H
#define AEROJS_CORE_TRANSFORMERS_CONSTANT_FOLDING_H

#include <cstddef>   // For size_t
#include <memory>    // For std::shared_ptr
#include <optional>  // For std::optional (evaluateBuiltInMathFunction の戻り値に使用)
#include <string>    // For evaluateBuiltInMathFunction
#include <vector>    // For evaluateBuiltInMathFunction

// Forward declarations for AST nodes and enums to minimize header dependencies
namespace aerojs::parser::ast {
class Node;
class BinaryExpression;
class UnaryExpression;
class LogicalExpression;
class ConditionalExpression;
class ArrayExpression;
class ObjectExpression;
class CallExpression;
class MemberExpression;
class Literal;
enum class BinaryOperator : int;
enum class UnaryOperator : int;
enum class LogicalOperator : int;
// NodePtr の定義 (プロジェクト全体で一貫性を持たせるべき)
using NodePtr = std::shared_ptr<Node>;
}  // namespace aerojs::parser::ast

namespace aerojs::core::transformers {

/**
 * @class ConstantFoldingTransformer
 * @brief 定数畳み込み最適化を実行する AST Transformer。
 *
 * AST を走査し、コンパイル時に評価可能な定数式を計算して置き換えます。
 * 状態として統計情報（畳み込んだ式の数、訪問ノード数）を保持します。
 */
class ConstantFoldingTransformer {
 public:
  /**
   * @brief ConstantFoldingTransformer のインスタンスを構築します。
   *
   * 統計収集はデフォルトで無効になっています。
   */
  ConstantFoldingTransformer();

  // デフォルトのデストラクタで十分なので宣言不要
  // ~ConstantFoldingTransformer() = default;

  // --- コピー禁止 ---
  ConstantFoldingTransformer(const ConstantFoldingTransformer&) = delete;
  ConstantFoldingTransformer& operator=(const ConstantFoldingTransformer&) = delete;

  // --- ムーブ許可 --- (デフォルトで十分だが明示)
  ConstantFoldingTransformer(ConstantFoldingTransformer&&) noexcept = default;
  ConstantFoldingTransformer& operator=(ConstantFoldingTransformer&&) noexcept = default;

  /**
   * @brief 統計カウンター (畳み込んだ式の数、訪問したノード数) をリセットします。
   */
  void ResetCounters();

  /**
   * @brief 統計情報の収集を有効または無効にします。
   *
   * @param enable true の場合、統計収集を有効にします。false の場合、無効にします。
   *               デフォルトは false (無効) です。
   */
  void EnableStatistics(bool enable);

  /**
   * @brief これまでに行った定数畳み込みの回数を取得します。
   *
   * @return 畳み込まれた式の総数。
   * @note 統計収集が有効な場合にのみ正確な値が保証されます。
   */
  [[nodiscard]] size_t GetFoldedExpressionsCount() const noexcept;

  /**
   * @brief 変換処理中に訪問した AST ノードの総数を取得します。
   *
   * @return 訪問したノードの総数。
   * @note 統計収集が有効な場合にのみ正確な値が保証されます。
   */
  [[nodiscard]] size_t GetVisitedNodesCount() const noexcept;

  /**
   * @brief 指定された AST ノードとその子孫に対して定数畳み込みを実行します。
   *
   * このメソッドは AST を再帰的に走査し、定数式を見つけて評価・置換します。
   * ノードが畳み込みによって変更された場合、新しいノード (通常は Literal) への
   * ポインタが返されます。変更がない場合は、元のノードへのポインタが返されます。
   *
   * @param node 変換を開始するルート AST ノードへのポインタ。
   *             所有権は共有されます (shared_ptr)。
   * @return 変換後の AST ノードへのポインタ (shared_ptr)。
   *         入力ノードが null の場合は null を返します。
   * @throws このメソッド自体は例外をスローしませんが、内部処理 (ノード作成など)
   *         でメモリ確保例外などが発生する可能性はあります。
   */
  [[nodiscard]] parser::ast::NodePtr Transform(parser::ast::NodePtr node);

 private:
  // --- 再帰的な走査 ---

  /**
   * @brief 指定されたノードを訪問し、定数畳み込みを試みます。
   *
   * ノードの種類に応じて適切な `Fold*` メソッドを呼び出します。
   * 子ノードを持つノードの場合、先に子ノードを再帰的に処理します。
   *
   * @param node 処理対象のノード (shared_ptr)。
   * @return 畳み込み後のノード (shared_ptr)。変更がない場合は元のノード。
   */
  parser::ast::NodePtr VisitNode(parser::ast::NodePtr node);

  // --- 各ノードタイプに対する畳み込み処理 ---
  // Fold* メソッドの引数は const std::shared_ptr<T>& に変更し、不要なコピーを防ぐ

  parser::ast::NodePtr FoldBinaryExpression(const std::shared_ptr<parser::ast::BinaryExpression>& expr);
  parser::ast::NodePtr FoldUnaryExpression(const std::shared_ptr<parser::ast::UnaryExpression>& expr);
  parser::ast::NodePtr FoldLogicalExpression(const std::shared_ptr<parser::ast::LogicalExpression>& expr);
  parser::ast::NodePtr FoldConditionalExpression(const std::shared_ptr<parser::ast::ConditionalExpression>& expr);
  parser::ast::NodePtr FoldArrayExpression(const std::shared_ptr<parser::ast::ArrayExpression>& expr);
  parser::ast::NodePtr FoldObjectExpression(const std::shared_ptr<parser::ast::ObjectExpression>& expr);
  parser::ast::NodePtr FoldCallExpression(const std::shared_ptr<parser::ast::CallExpression>& expr);
  parser::ast::NodePtr FoldMemberExpression(const std::shared_ptr<parser::ast::MemberExpression>& expr);

  // --- ヘルパー関数 ---

  /**
   * @brief 指定された AST ノードが定数リテラルかどうかを判定します。
   *
   * @param node 判定対象のノード (shared_ptr)。
   * @return ノードが Literal であり、その値がコンパイル時に確定している場合は true。
   */
  [[nodiscard]] bool IsConstant(const parser::ast::NodePtr& node) const noexcept;

  /**
   * @brief 指定されたリテラルが JavaScript において "truthy" (真値) と評価されるか判定します。
   *
   * @param literal 判定対象のリテラルへの参照。
   * @return リテラルが truthy な値 (例: 非 0 数値, 空でない文字列, true) であれば true。
   *         null リテラルや falsy な値の場合は false。
   */
  [[nodiscard]] bool IsTruthy(const parser::ast::Literal& literal) const noexcept;

  /**
   * @brief 2つの定数リテラルに対して二項演算を実行します。
   *
   * @param op 実行する二項演算子。
   * @param left 左オペランドのリテラルへの参照。
   * @param right 右オペランドのリテラルへの参照。
   * @return 演算結果を表す新しい Literal ノードへのポインタ (shared_ptr)。
   *         演算が定義されていない、または実行できない場合 (例: 型エラー) は nullptr。
   */
  [[nodiscard]] parser::ast::NodePtr EvaluateBinaryOperation(
      parser::ast::BinaryOperator op,
      const parser::ast::Literal& left,
      const parser::ast::Literal& right) const;

  /**
   * @brief 定数リテラルに対して単項演算を実行します。
   *
   * @param op 実行する単項演算子。
   * @param arg オペランドのリテラルへの参照。
   * @return 演算結果を表す新しい Literal ノードへのポインタ (shared_ptr)。
   *         演算が定義されていない、または実行できない場合は nullptr。
   */
  [[nodiscard]] parser::ast::NodePtr EvaluateUnaryOperation(
      parser::ast::UnaryOperator op,
      const parser::ast::Literal& arg) const;

  /**
   * @brief 2つの定数リテラルに対して論理演算を実行します。
   *
   * ショートサーキット評価はここでは考慮せず、単純に両オペランドを用いた結果を返します。
   * (ショートサーキットは `FoldLogicalExpression` で処理されます)
   *
   * @param op 実行する論理演算子 (`&&` または `||`)。
   * @param left 左オペランドのリテラルへの参照。
   * @param right 右オペランドのリテラルへの参照。
   * @return JavaScript の論理演算規則に従った結果のリテラル (左または右オペランドの値) へのポインタ (shared_ptr)。
   *         演算が不正な場合は nullptr (通常発生しない)。
   */
  [[nodiscard]] parser::ast::NodePtr EvaluateLogicalOperation(
      parser::ast::LogicalOperator op,
      const parser::ast::Literal& left,
      const parser::ast::Literal& right) const;

  /**
   * @brief リテラルノードをその文字列表現に変換します。
   *
   * @param literal 文字列に変換するリテラルへの参照。
   * @return リテラルの文字列表現。
   */
  [[nodiscard]] std::string LiteralToString(const parser::ast::Literal& literal) const;

  /**
   * @brief 特定の組み込み Math 関数の呼び出しを評価します。
   *
   * `Math.pow`, `Math.sqrt`, `Math.abs` などの静的な Math 関数で、
   * 引数がすべて数値リテラルの場合に計算を実行します。
   *
   * @param name 呼び出される Math 関数の名前 (例: "pow", "sqrt")。
   * @param args 関数に渡される数値引数のリスト。
   * @return 計算結果の数値を含む std::optional<double>。
   *         計算できない、またはサポートされていない関数の場合は `std::nullopt`。
   */
  [[nodiscard]] std::optional<double> EvaluateBuiltInMathFunction(
      const std::string& name,
      const std::vector<double>& args) const;

  // --- 内部状態 ---
  bool m_statisticsEnabled = false;  ///< 統計収集が有効かどうかを示すフラグ (デフォルトは false)。
  size_t m_foldedExpressions = 0;    ///< 畳み込まれた式の総数カウンター (デフォルトは 0)。
  size_t m_visitedNodes = 0;         ///< 訪問したノードの総数カウンター (デフォルトは 0)。
};

}  // namespace aerojs::core::transformers

#endif  // AEROJS_CORE_TRANSFORMERS_CONSTANT_FOLDING_H
