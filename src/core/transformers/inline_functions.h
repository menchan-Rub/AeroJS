/*******************************************************************************
 * @file src/core/transformers/inline_functions.h
 * @version 1.0.0
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 * @brief 関数インライン化 (Function Inlining) 最適化を行う AST Transformer。
 *
 * この Transformer は、Abstract Syntax Tree (AST) を走査し、特定の条件を満たす
 * 関数呼び出しを、その関数の本体で置き換えます。
 * 関数呼び出しのオーバーヘッドを削減し、他の最適化 (定数畳み込みなど) の機会を
 * 増やすことを目的としています。
 *
 * インライン化の判断基準:
 * - 関数のサイズ (ステートメント数)
 * - 再帰呼び出しの有無と深度
 * - 副作用の有無
 * - 参照スコープの問題
 *
 * スレッド安全性: このクラスのインスタンスメソッドはスレッドセーフではありません。
 *              複数のスレッドから同時に transform (または基底クラスの Visit) メソッドを
 *              呼び出す場合は、スレッドごとに個別のインスタンスを使用するか、
 *              外部で排他制御を行う必要があります。
 ******************************************************************************/

#ifndef AEROJS_CORE_TRANSFORMERS_INLINE_FUNCTIONS_H
#define AEROJS_CORE_TRANSFORMERS_INLINE_FUNCTIONS_H

#include <cstddef>  // For size_t
#include <memory>   // For std::shared_ptr
#include <string>   // For std::string
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/core/transformers/transformer.h"

namespace aerojs::parser::ast {
class Node;
class Program;
class FunctionDeclaration;
class FunctionExpression;
class ArrowFunctionExpression;
class CallExpression;
using NodePtr = std::shared_ptr<Node>;
}  // namespace aerojs::parser::ast

namespace aerojs::core::transformers {

/**
 * @class InlineFunctionsTransformer
 * @brief 関数インライン化最適化を実行する AST Transformer。
 *
 * Transformer 基底クラスの Visitor パターンを実装し、関数定義を収集し、
 * 呼び出し箇所でインライン化を試みます。
 */
class InlineFunctionsTransformer : public Transformer {
 public:
  /**
   * @brief コンストラクタ。
   *
   * @param maxInlineSize インライン化対象とする関数の最大推定サイズ (ノード数や文の数など、実装依存)。
   *                      デフォルトは 20。
   * @param maxRecursionDepth 再帰関数をインライン展開する際の最大深度。
   *                          0 は再帰関数のインライン化を無効にします。デフォルトは 2。
   * @param enableStatistics true の場合、インライン化に関する統計情報を収集します。
   *                         デフォルトは false。
   */
  explicit InlineFunctionsTransformer(
      size_t maxInlineSize = 20,
      size_t maxRecursionDepth = 2,
      bool enableStatistics = false);

  ~InlineFunctionsTransformer() override = default;

  // コピー禁止
  InlineFunctionsTransformer(const InlineFunctionsTransformer&) = delete;
  InlineFunctionsTransformer& operator=(const InlineFunctionsTransformer&) = delete;

  // ムーブ許可
  InlineFunctionsTransformer(InlineFunctionsTransformer&&) noexcept = default;
  InlineFunctionsTransformer& operator=(InlineFunctionsTransformer&&) noexcept = default;

  /**
   * @brief 内部状態 (関数マップ、統計カウンターなど) をリセットします。
   *        複数回の変換処理を行う前に呼び出すことを想定しています。
   */
  void reset();

  /**
   * @brief 統計情報の収集を有効または無効にします。
   *
   * @param enable true の場合、統計収集を有効にします。false の場合、無効にします。
   */
  void enableStatistics(bool enable);

  /**
   * @brief これまでにインライン化された一意の関数の数を取得します。
   *
   * @return インライン化された関数の総数。
   * @note 統計収集が有効な場合にのみ正確な値が保証されます。
   */
  [[nodiscard]] size_t getInlinedFunctionsCount() const noexcept;

  /**
   * @brief これまでに行われた関数呼び出しのインライン化の総数を取得します。
   *        (同じ関数が複数回インライン化された場合、それぞれカウントされます)
   *
   * @return インライン化された呼び出しの総数。
   * @note 統計収集が有効な場合にのみ正確な値が保証されます。
   */
  [[nodiscard]] size_t getInlinedCallsCount() const noexcept;

  /**
   * @brief 変換処理中に訪問した AST ノードの総数を取得します。
   *
   * @return 訪問したノードの総数。
   * @note 統計収集が有効な場合にのみ正確な値が保証されます。
   */
  [[nodiscard]] size_t getVisitedNodesCount() const noexcept;

 protected:
  // AST Visitor メソッドのオーバーライド
  void visitProgram(parser::ast::Program* node) override;
  void visitFunctionDeclaration(parser::ast::FunctionDeclaration* node) override;
  void visitFunctionExpression(parser::ast::FunctionExpression* node) override;
  void visitArrowFunctionExpression(parser::ast::ArrowFunctionExpression* node) override;
  void visitCallExpression(parser::ast::CallExpression* node) override;

 private:
  /**
   * @struct FunctionInfo
   * @brief インライン化候補となる関数の情報を保持する内部構造体。
   */
  struct FunctionInfo {
    parser::ast::NodePtr functionNode;   // 関数定義ノード
    std::string name;                    // 関数名 (宣言の場合)。匿名の場合は空
    size_t size = 0;                     // 関数の推定サイズ
    bool hasSideEffects = true;          // 副作用を持つ可能性があるか
    bool hasMultipleReturns = false;     // 複数の return 文を持つか
    bool isRecursive = false;            // 再帰呼び出しを含むか
    bool isEligibleForInlining = false;  // インライン化の基本条件を満たすか

    // 関数シグネチャと本体
    std::vector<parser::ast::NodePtr> parameters;  // 仮引数ノードのリスト
    parser::ast::NodePtr body;                     // 関数本体ノード
  };

  /**
   * @brief 関数呼び出しをインライン化可能かどうかを判断します。
   *
   * @param funcInfo 対象関数の情報。
   * @param callNode 呼び出し元の CallExpression ノード。
   * @return インライン化可能であれば true。
   */
  [[nodiscard]] bool isFunctionInlinable(
      const FunctionInfo& funcInfo,
      const parser::ast::CallExpression* callNode) const;

  /**
   * @brief 実際の関数インライン化処理を実行します。
   *
   * @param callExpr 置き換え対象の関数呼び出し式。
   * @param funcInfo インライン化する関数の情報。
   * @return インライン化によって生成された新しい AST ノード。
   *         インライン化に失敗した場合は nullptr。
   */
  [[nodiscard]] parser::ast::NodePtr inlineCall(
      parser::ast::CallExpression* callExpr,
      const FunctionInfo& funcInfo);

  /**
   * @brief 関数の推定サイズを計算します。
   *
   * @param node 対象の関数本体ノード。
   * @return 推定された関数のサイズ。
   */
  [[nodiscard]] size_t calculateFunctionSize(const parser::ast::NodePtr& node) const;

  /**
   * @brief 関数本体が副作用を持つ可能性があるかチェックします。
   *
   * @param node 対象の関数本体ノード。
   * @return 副作用を持つ可能性がある場合は true。
   */
  [[nodiscard]] bool checkForSideEffects(const parser::ast::NodePtr& node) const;

  /**
   * @brief 関数が自身を再帰的に呼び出しているかチェックします。
   *
   * @param funcInfo 対象関数の情報。
   * @return 再帰呼び出しが含まれる場合は true。
   */
  [[nodiscard]] bool isRecursiveFunction(const FunctionInfo& funcInfo) const;

  /**
   * @brief 指定された識別子名が現在のスコープスタック内で宣言されているか確認します。
   *
   * @param name 確認する識別子名。
   * @return 現在のスコープまたは外側のスコープで宣言されていれば true。
   */
  [[nodiscard]] bool isIdentifierInScope(const std::string& name) const;

  /**
   * @brief インライン展開時に変数名の衝突を避けるため、一意な名前を生成します。
   *
   * @param baseName 元の変数名。
   * @return 現在のスコープで一意な新しい変数名。
   */
  [[nodiscard]] std::string generateUniqueVarName(const std::string& baseName);

  /**
   * @brief 関数情報を収集し、マップに登録します。
   * 
   * @param funcNode 関数定義ノード。
   * @param name 関数名 (匿名の場合は空)。
   */
  void collectFunctionInfo(parser::ast::NodePtr funcNode, const std::string& name = "");

  /**
   * @brief 新しいスコープを開始します。
   */
  void enterScope();

  /**
   * @brief 現在のスコープを終了します。
   */
  void leaveScope();

  /**
   * @brief 現在のスコープに変数を宣言します。
   * 
   * @param name 宣言する変数名。
   */
  void declareVariableInCurrentScope(const std::string& name);

  // 関数情報
  std::unordered_map<std::string, FunctionInfo> m_namedFunctions;  // 名前付き関数の情報
  std::vector<FunctionInfo> m_anonymousFunctions;                  // 匿名関数の情報リスト

  // スコープ管理
  std::vector<std::unordered_set<std::string>> m_scopeStack;  // 現在のスコープチェーン

  // インライン化オプション
  size_t m_maxInlineSize;              // インライン化対象の最大関数サイズ
  size_t m_maxRecursionDepth;          // 再帰インライン化の最大深度
  size_t m_currentRecursionDepth = 0;  // 現在の再帰インライン化深度

  // 統計情報
  bool m_statisticsEnabled = false;    // 統計収集フラグ
  size_t m_inlinedFunctionsCount = 0;  // インライン化された一意の関数の数
  size_t m_inlinedCallsCount = 0;      // インライン化された呼び出しの総数
  size_t m_visitedNodesCount = 0;      // 訪問したノードの総数

  // 一意な名前生成用カウンター
  size_t m_nextUniqueId = 0;  // 変数名の衝突回避用カウンター
};

}  // namespace aerojs::core::transformers

#endif  // AEROJS_CORE_TRANSFORMERS_INLINE_FUNCTIONS_H