/**
 * @file switch_statement.h
 * @brief AeroJS AST の switch 文関連ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの `switch` 文 (`SwitchStatement`)
 * およびその中の `case` / `default` 節 (`SwitchCase`) を表すASTノードを定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_STATEMENTS_SWITCH_STATEMENT_H
#define AEROJS_PARSER_AST_NODES_STATEMENTS_SWITCH_STATEMENT_H

#include <memory>
#include <string>
#include <vector>

#include "../../common.h"
#include "../../visitors/ast_visitor.h"
#include "../expressions/expression_node.h"  // 条件式 (discriminant, test)
#include "statement_node.h"                  // 基底クラス

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class SwitchCase
 * @brief `switch` 文内の `case` または `default` 節を表すノード。
 *
 * @details
 * `case` 節の場合は `test` 式を持ち、`default` 節の場合は持ちません (null)。
 * 各節は複数の文 (`consequent`) を持つことができます。
 */
class SwitchCase final : public Node {  // StatementNode ではない点に注意
 public:
  /**
   * @brief SwitchCaseノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param test case 節のテスト式 (Expression)。default 節の場合は null。ムーブされる。
   * @param consequent この節の文リスト (Statement のリスト)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit SwitchCase(const SourceLocation& location,
                      NodePtr&& test,  // default 節の場合は null
                      std::vector<NodePtr>&& consequent,
                      Node* parent = nullptr);

  ~SwitchCase() override = default;

  SwitchCase(const SwitchCase&) = delete;
  SwitchCase& operator=(const SwitchCase&) = delete;
  SwitchCase(SwitchCase&&) noexcept = default;
  SwitchCase& operator=(SwitchCase&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::SwitchCase;
  }

  /**
   * @brief テスト式を取得します (非const版)。
   * @return テスト式ノードへの参照 (`NodePtr&`)。default 節の場合はnullを指すunique_ptr。
   */
  NodePtr& getTest() noexcept;

  /**
   * @brief テスト式を取得します (const版)。
   * @return テスト式ノードへのconst参照 (`const NodePtr&`)。default 節の場合はnullを指すunique_ptr。
   */
  const NodePtr& getTest() const noexcept;

  /**
   * @brief この節の文リストを取得します (非const版)。
   * @return 文ノードのリストへの参照 (`std::vector<NodePtr>&`)。
   */
  std::vector<NodePtr>& getConsequent() noexcept;

  /**
   * @brief この節の文リストを取得します (const版)。
   * @return 文ノードのリストへのconst参照 (`const std::vector<NodePtr>&`)。
   */
  const std::vector<NodePtr>& getConsequent() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_test;                     ///< テスト式 (Expression or null for default)
  std::vector<NodePtr> m_consequent;  ///< この節の文リスト (Statement)
};

/**
 * @class SwitchStatement
 * @brief `switch` 文 (`switch (discriminant) { cases... }`) を表すノード。
 */
class SwitchStatement final : public StatementNode {
 public:
  /**
   * @brief SwitchStatementノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param discriminant 比較対象の式 (Expression)。ムーブされる。
   * @param cases switch 文内の case/default 節のリスト (`SwitchCase` のリスト)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit SwitchStatement(const SourceLocation& location,
                           NodePtr&& discriminant,
                           std::vector<NodePtr>&& cases,
                           Node* parent = nullptr);

  ~SwitchStatement() override = default;

  SwitchStatement(const SwitchStatement&) = delete;
  SwitchStatement& operator=(const SwitchStatement&) = delete;
  SwitchStatement(SwitchStatement&&) noexcept = default;
  SwitchStatement& operator=(SwitchStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::SwitchStatement;
  }

  /**
   * @brief 比較対象の式を取得します (非const版)。
   * @return 比較対象の式ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getDiscriminant() noexcept;

  /**
   * @brief 比較対象の式を取得します (const版)。
   * @return 比較対象の式ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getDiscriminant() const noexcept;

  /**
   * @brief case/default 節のリストを取得します (非const版)。
   * @return `SwitchCase` ノードのリストへの参照 (`std::vector<NodePtr>&`)。
   */
  std::vector<NodePtr>& getCases() noexcept;

  /**
   * @brief case/default 節のリストを取得します (const版)。
   * @return `SwitchCase` ノードのリストへのconst参照 (`const std::vector<NodePtr>&`)。
   */
  const std::vector<NodePtr>& getCases() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_discriminant;        ///< 比較対象の式 (Expression)
  std::vector<NodePtr> m_cases;  ///< case/default 節 (`SwitchCase`) のリスト
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_STATEMENTS_SWITCH_STATEMENT_H