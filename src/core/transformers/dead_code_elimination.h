/*******************************************************************************
 * @file src/core/transformers/dead_code_elimination.h
 * @version 1.0.0 // TODO: バージョン管理
 * @copyright Copyright (c) 2024 AeroJS Project // 年更新
 * @license MIT License
 * @brief 不要なコード（デッドコード）を除去する AST Transformer。
 *
 * @details
 * この Transformer は、到達不能なコード、使用されていない変数や関数、
 * 効果のない式などを検出し、AST から削除します。
 * 定数伝播や制御フロー解析の結果を利用して、より高度なデッドコード除去を
 * 行うことも可能です。
 *
 * 対応する最適化の例:
 * - 到達不能コード (例: return 文の後)
 * - 未使用のローカル変数・関数宣言
 * - 静的に false と判断される条件分岐 (例: `if (false) { ... }`)
 * - 副作用のない式文 (例: `"hello";`)
 *
 * スレッド安全性: このクラスのインスタンスメソッドはスレッドセーフではありません。
 *              複数回の `Transform` 呼び出しは内部状態に影響を与える可能性があります。
 *              スレッドごとに個別のインスタンスを使用するか、外部で排他制御が必要です。
 ******************************************************************************/

#ifndef AEROJS_CORE_TRANSFORMERS_DEAD_CODE_ELIMINATION_H  // インクルードガード修正
#define AEROJS_CORE_TRANSFORMERS_DEAD_CODE_ELIMINATION_H

#include <bitset>   // For m_optimizationFlags
#include <cstddef>  // For size_t
#include <memory>   // For std::shared_ptr, std::unique_ptr
#include <stack>    // For m_scopeStack
#include <string>   // For std::string
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/core/transformers/transformer.h"  // 基底クラスヘッダーを最初にインクルード
// #include <optional> // 現状未使用?

// 外部コンポーネント (前方宣言で可能なものを検討)
#include "src/core/optimization/optimization_level.h"  // Enum なのでヘッダーが必要
namespace aerojs::core::analysis {
class ControlFlowGraph;
class DataFlowAnalysis;
// class ConstantPropagation; // 必要に応じて前方宣言
}  // namespace aerojs::core::analysis

// AST ノードの前方宣言
namespace aerojs::parser::ast {
class Node;
using NodePtr = std::shared_ptr<Node>;
using LiteralPtr = std::shared_ptr<Literal>;  // LiteralPtr をここで定義
class Program;
class BlockStatement;
class IfStatement;
class SwitchStatement;
class ForStatement;
class WhileStatement;
class DoWhileStatement;
class TryStatement;
class ReturnStatement;
class BreakStatement;
class ContinueStatement;
class ThrowStatement;
class ExpressionStatement;
class VariableDeclaration;
class FunctionDeclaration;
class FunctionExpression;
class ArrowFunctionExpression;
class ClassDeclaration;
class ClassExpression;
class Literal;
class Identifier;
class BinaryExpression;
class LogicalExpression;
class UnaryExpression;
class ConditionalExpression;
class CallExpression;
class MemberExpression;
class ArrayExpression;
class ObjectExpression;
}  // namespace aerojs::parser::ast

namespace aerojs::core::transformers {  // 名前空間修正

/**
 * @class DeadCodeEliminationTransformer
 * @brief デッドコード除去最適化を実行する AST Transformer。
 *
 * Transformer 基底クラスの Visitor パターンを実装し、到達不能コードや
 * 未使用の変数・関数などを削除します。
 */
class DeadCodeEliminationTransformer : public Transformer {  // public 継承を明示
 public:
  /**
   * @brief 統計情報を保持する構造体。
   */
  struct Statistics {
    size_t m_removedStatements = 0;      ///< 削除されたステートメント数 (m_ プレフィックス追加)
    size_t m_removedVariables = 0;       ///< 削除された変数宣言数 (m_ プレフィックス追加)
    size_t m_simplifiedExpressions = 0;  ///< 簡略化された式の数 (m_ プレフィックス追加)
    size_t m_removedFunctions = 0;       ///< 削除された関数数 (m_ プレフィックス追加)
    size_t m_unreachableCodeBlocks = 0;  ///< 削除された到達不能コードブロック数 (m_ プレフィックス追加)
    size_t m_optimizedConditions = 0;    ///< 最適化された条件式数 (m_ プレフィックス追加)
    size_t m_optimizedLoops = 0;         ///< 最適化されたループ数 (m_ プレフィックス追加)

    // デフォルトコンストラクタはメンバー初期化があれば不要
    // Statistics() = default;
  };

  /**
   * @brief コンストラクタ。
   *
   * @param optimizationLevel 適用する最適化のレベル。
   *                          レベルに応じて実行する解析や除去処理が変わる可能性があります。
   */
  explicit DeadCodeEliminationTransformer(
      optimization::OptimizationLevel optimizationLevel = optimization::OptimizationLevel::Normal);

  // デフォルトデストラクタで十分
  ~DeadCodeEliminationTransformer() override = default;

  // --- コピー禁止 ---
  DeadCodeEliminationTransformer(const DeadCodeEliminationTransformer&) = delete;
  DeadCodeEliminationTransformer& operator=(const DeadCodeEliminationTransformer&) = delete;

  // --- ムーブ許可 --- (デフォルトで十分だが明示)
  DeadCodeEliminationTransformer(DeadCodeEliminationTransformer&&) noexcept = default;
  DeadCodeEliminationTransformer& operator=(DeadCodeEliminationTransformer&&) noexcept = default;

  // --- ITransformer インターフェースの実装 ---

  /**
   * @brief 指定された AST ノードに対してデッドコード除去を実行します。
   *
   * @param node 変換対象の AST ノード (unique_ptr)。
   * @return TransformResult 変換結果。
   */
  TransformResult Transform(parser::ast::NodePtr node) override;

  // --- 設定・統計 ---

  /**
   * @brief 適用する最適化レベルを設定します。
   *
   * @param level 設定する最適化レベル。
   */
  void SetOptimizationLevel(optimization::OptimizationLevel level);

  /**
   * @brief 現在設定されている最適化レベルを取得します。
   *
   * @return 現在の最適化レベル。
   */
  [[nodiscard]] optimization::OptimizationLevel GetOptimizationLevel() const noexcept;

  /**
   * @brief 実行された最適化に関する統計情報を取得します。
   *
   * @return 統計情報を含む Statistics 構造体への const 参照。
   */
  [[nodiscard]] const Statistics& GetStatistics() const noexcept;

 protected:
  // --- AST Visitor メソッドのオーバーライド ---
  // 基底クラスの Transformer (Visitor パターン実装) をオーバーライド

  void VisitProgram(parser::ast::Program* node) override;
  void VisitBlockStatement(parser::ast::BlockStatement* node) override;
  void VisitIfStatement(parser::ast::IfStatement* node) override;
  void VisitSwitchStatement(parser::ast::SwitchStatement* node) override;
  void VisitForStatement(parser::ast::ForStatement* node) override;
  void VisitWhileStatement(parser::ast::WhileStatement* node) override;
  void VisitDoWhileStatement(parser::ast::DoWhileStatement* node) override;
  void VisitTryStatement(parser::ast::TryStatement* node) override;
  void VisitReturnStatement(parser::ast::ReturnStatement* node) override;
  void VisitBreakStatement(parser::ast::BreakStatement* node) override;
  void VisitContinueStatement(parser::ast::ContinueStatement* node) override;
  void VisitThrowStatement(parser::ast::ThrowStatement* node) override;
  void VisitExpressionStatement(parser::ast::ExpressionStatement* node) override;
  void VisitVariableDeclaration(parser::ast::VariableDeclaration* node) override;
  void VisitFunctionDeclaration(parser::ast::FunctionDeclaration* node) override;
  void VisitFunctionExpression(parser::ast::FunctionExpression* node) override;
  void VisitArrowFunctionExpression(parser::ast::ArrowFunctionExpression* node) override;
  void VisitClassDeclaration(parser::ast::ClassDeclaration* node) override;
  void VisitClassExpression(parser::ast::ClassExpression* node) override;
  void VisitLiteral(parser::ast::Literal* node) override;
  void VisitIdentifier(parser::ast::Identifier* node) override;
  void VisitBinaryExpression(parser::ast::BinaryExpression* node) override;
  void VisitLogicalExpression(parser::ast::LogicalExpression* node) override;
  void VisitUnaryExpression(parser::ast::UnaryExpression* node) override;
  void VisitConditionalExpression(parser::ast::ConditionalExpression* node) override;
  void VisitCallExpression(parser::ast::CallExpression* node) override;
  void VisitMemberExpression(parser::ast::MemberExpression* node) override;
  void VisitArrayExpression(parser::ast::ArrayExpression* node) override;
  void VisitObjectExpression(parser::ast::ObjectExpression* node) override;

 private:
  // --- 解析ヘルパー ---

  /**
   * @brief 式が副作用を持つ可能性をチェックします (保守的な判断)。
   * @param node チェック対象のノード (shared_ptr)。
   * @return 副作用を持つ可能性がある場合は true。
   */
  [[nodiscard]] bool HasSideEffects(const parser::ast::NodePtr& node) const;

  /**
   * @brief 式が静的に "truthy" または "falsy" に評価されるか判断します。
   * @param node チェック対象のノード (shared_ptr)。
   * @return "truthy" なら std::optional<true>、"falsy" なら std::optional<false>、
   *         静的に判断できない場合は std::nullopt。
   */
  [[nodiscard]] std::optional<bool> TryEvaluateAsBoolean(const parser::ast::NodePtr& node);

  /**
   * @brief 指定されたスコープ内で使用されている変数名を収集します。
   *        (FunctionDeclaration や FunctionExpression の本体を解析する際に使用)
   * @param node 解析を開始するノード (shared_ptr)。
   * @param usedVariables [out] 使用されている変数名を格納するセット。
   */
  void CollectUsedVariables(const parser::ast::NodePtr& node, std::unordered_set<std::string>& usedVariables);

  /**
   * @brief 式を静的に評価し、結果のリテラルを返します。
   *        (ConstantFoldingTransformer の機能の一部を借用または再実装)
   * @param node 評価する式ノード (shared_ptr)。
   * @return 評価結果のリテラル (LiteralPtr)。評価できない場合は nullptr。
   */
  [[nodiscard]] parser::ast::LiteralPtr TryEvaluateConstantExpression(const parser::ast::NodePtr& node);

  // --- 除去・最適化ヘルパー ---

  /**
   * @brief ステートメントのリストから到達不能なコードを除去します。
   *        return, break, continue, throw の後にあるコードが対象です。
   * @param statements [in, out] 変更対象のステートメントリスト。
   * @return リストに変更があった場合は true。
   */
  bool RemoveUnreachableCode(std::vector<parser::ast::NodePtr>& statements);

  /**
   * @brief ステートメントのリストから、指定されたセットに含まれない変数宣言を除去します。
   * @param statements [in, out] 変更対象のステートメントリスト。
   * @param usedVariables 使用されている変数名のセット。
   * @return リストに変更があった場合は true。
   */
  bool RemoveUnusedVariables(std::vector<parser::ast::NodePtr>& statements,
                             const std::unordered_set<std::string>& usedVariables);

  /**
   * @brief ステートメントのリストから空のブロックステートメントを除去します。
   * @param statements [in, out] 変更対象のステートメントリスト。
   * @return リストに変更があった場合は true。
   */
  bool RemoveEmptyBlocks(std::vector<parser::ast::NodePtr>& statements);

  /**
   * @brief ステートメントのリストから、副作用のない式ステートメントを除去します。
   * @param statements [in, out] 変更対象のステートメントリスト。
   * @return リストに変更があった場合は true。
   */
  bool RemoveNoEffectExpressions(std::vector<parser::ast::NodePtr>& statements);

  /**
   * @brief IfStatement を最適化します。
   *        条件式が定数に評価できる場合、不要な分岐を除去します。
   * @param node 最適化対象の IfStatement へのポインタ。
   * @return 最適化後のノード (元の IfStatement、then/else ブロック、または nullptr)。
   *         このメソッドは VisitIfStatement 内から呼び出され、ノードの置き換えに使われます。
   */
  [[nodiscard]] parser::ast::NodePtr OptimizeIfStatement(parser::ast::IfStatement* node);

  /**
   * @brief ループ (For, While, DoWhile) を最適化します。
   *        (例: `while(false)` のようなループを除去)
   * @tparam T ループノードの型 (ForStatement, WhileStatement, DoWhileStatement)。
   * @param node 最適化対象のループノードへのポインタ。
   * @return 最適化後のノード (元のループノードまたは nullptr)。
   */
  template <typename T>
  [[nodiscard]] parser::ast::NodePtr OptimizeLoop(T* node);

  // --- スコープと状態管理 ---

  /**
   * @struct ScopeInfo
   * @brief スコープ内の変数や関数の使用状況を追跡するための内部構造体。
   */
  struct ScopeInfo {
    std::string m_name;                              // スコープ名 (デバッグ用)
    std::unordered_set<std::string> m_declaredVars;  // このスコープで宣言された変数
    std::unordered_set<std::string> m_usedVars;      // このスコープで使用された変数
    // std::unordered_map<std::string, bool> m_functions; // 必要であれば関数の追跡も
    bool m_unreachableCodeDetected = false;  // このスコープ内で到達不能コードが検出されたか

    explicit ScopeInfo(std::string n)
        : m_name(std::move(n)) {
    }  // explicit 追加
  };

  /**
   * @brief 新しいスコープを開始し、スタックに追加します。
   * @param scopeName スコープの名前 (デバッグ用)。
   */
  void EnterScope(const std::string& scopeName);

  /**
   * @brief 現在のスコープをスタックから削除します。
   */
  void LeaveScope();

  /**
   * @brief 現在のスコープで変数が宣言されたことを記録します。
   * @param name 宣言された変数名。
   */
  void DeclareVariable(const std::string& name);

  /**
   * @brief 変数が使用されたことを記録します。
   *        スコープチェーンを遡って、宣言されているスコープを見つけます。
   * @param name 使用された変数名。
   */
  void MarkVariableUsed(const std::string& name);

  /**
   * @brief 現在のスコープで到達不能コードが検出されたことをマークします。
   */
  void MarkUnreachable();

  /**
   * @brief 現在のスコープが到達不能かどうかを返します。
   */
  [[nodiscard]] bool IsCurrentScopeUnreachable() const;

  // --- 内部状態 ---
  std::stack<ScopeInfo> m_scopeStack;             ///< スコープ情報を管理するスタック。
  std::unordered_set<std::string> m_usedGlobals;  ///< 使用されたグローバル変数/関数名。
  // std::unordered_map<std::string, parser::ast::LiteralPtr> m_constantValues; ///< 定数伝播の結果 (より高度な実装用)。
  // std::bitset<64> m_optimizationFlags;                ///< 最適化フラグ (現状未使用?)。
  optimization::OptimizationLevel m_optimizationLevel;  ///< 適用する最適化レベル。
  Statistics m_statistics;                              ///< 収集した統計情報。
  // bool m_changed; // 状態変数削除。変換は TransformResult で管理。

  // --- 高度な解析 (オプショナル) ---
  // これらの実装は最適化レベルに応じて有効化される想定

  /**
   * @brief 制御フローグラフ (CFG) を構築します。
   * @param node CFG を構築する対象の関数またはプログラムノード。
   * @return 構築された CFG へのユニークポインタ。
   */
  [[nodiscard]] std::unique_ptr<analysis::ControlFlowGraph> BuildCFG(const parser::ast::NodePtr& node);

  /**
   * @brief データフロー解析 (例: Liveness Analysis) を実行します。
   * @param cfg 解析対象の CFG。
   * @return データフロー解析の結果 (具体的な型は解析による)。
   */
  [[nodiscard]] std::unique_ptr<analysis::DataFlowAnalysis> PerformDataFlowAnalysis(
      const analysis::ControlFlowGraph& cfg);

  /**
   * @brief 定数伝播解析を実行します。
   * @param node 解析対象のノード。
   * @return 定数値が確定した変数のマップ。
   */
  [[nodiscard]] std::unordered_map<std::string, parser::ast::LiteralPtr> PerformConstantPropagation(
      const parser::ast::NodePtr& node);

  /**
   * @brief ループ不変コードを検出します。
   * @param loopNode 解析対象のループノード。
   * @param cfg 関連する CFG。
   * @return ループ不変と判断された式のリスト。
   */
  [[nodiscard]] std::vector<parser::ast::NodePtr> DetectLoopInvariantCode(
      const parser::ast::NodePtr& loopNode,
      const analysis::ControlFlowGraph& cfg);
};

}  // namespace aerojs::core::transformers

#endif  // AEROJS_CORE_TRANSFORMERS_DEAD_CODE_ELIMINATION_H