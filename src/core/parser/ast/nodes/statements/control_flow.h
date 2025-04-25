/**
 * @file control_flow.h
 * @brief AeroJS AST の制御フロー文ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの制御フロー文 (`return`, `break`, `continue`, `throw`)
 * を表すASTノードを定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_STATEMENTS_CONTROL_FLOW_H
#define AEROJS_PARSER_AST_NODES_STATEMENTS_CONTROL_FLOW_H

#include <memory>
#include <string>
#include <vector>

#include "../../common.h"
#include "../../visitors/ast_visitor.h"
#include "../expressions/expression_node.h"  // 引数用
#include "../expressions/identifier.h"       // ラベル用
#include "statement_node.h"                  // 基底クラス

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class ReturnStatement
 * @brief `return` 文を表すノード。
 *
 * @details
 * オプションで返す値 (式) を持ちます。
 */
class ReturnStatement final : public StatementNode {
 public:
  /**
   * @brief ReturnStatementノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param argument 返す値 (式、オプション)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit ReturnStatement(const SourceLocation& location,
                           NodePtr&& argument,  // オプション、nullptr の場合あり
                           Node* parent = nullptr);

  ~ReturnStatement() override = default;

  ReturnStatement(const ReturnStatement&) = delete;
  ReturnStatement& operator=(const ReturnStatement&) = delete;
  ReturnStatement(ReturnStatement&&) noexcept = default;
  ReturnStatement& operator=(ReturnStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::ReturnStatement;
  }

  /**
   * @brief return する値 (式) を取得します (非const版)。
   * @return 式ノードへの参照 (`NodePtr&`)。値がない場合はnullを指すunique_ptr。
   */
  NodePtr& getArgument() noexcept;

  /**
   * @brief return する値 (式) を取得します (const版)。
   * @return 式ノードへのconst参照 (`const NodePtr&`)。値がない場合はnullを指すunique_ptr。
   */
  const NodePtr& getArgument() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_argument;  ///< return する値 (Expression or null)
};

/**
 * @class BreakStatement
 * @brief `break` 文を表すノード。
 *
 * @details
 * オプションで脱出先のラベル (Identifier) を持ちます。
 */
class BreakStatement final : public StatementNode {
 public:
  /**
   * @brief BreakStatementノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param label 脱出先のラベル (Identifier、オプション)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit BreakStatement(const SourceLocation& location,
                          NodePtr&& label,  // オプション、nullptr の場合あり
                          Node* parent = nullptr);

  ~BreakStatement() override = default;

  BreakStatement(const BreakStatement&) = delete;
  BreakStatement& operator=(const BreakStatement&) = delete;
  BreakStatement(BreakStatement&&) noexcept = default;
  BreakStatement& operator=(BreakStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::BreakStatement;
  }

  /**
   * @brief ラベルを取得します (非const版)。
   * @return ラベル (Identifier) ノードへの参照 (`NodePtr&`)。ラベルがない場合はnullを指すunique_ptr。
   */
  NodePtr& getLabel() noexcept;

  /**
   * @brief ラベルを取得します (const版)。
   * @return ラベル (Identifier) ノードへのconst参照 (`const NodePtr&`)。ラベルがない場合はnullを指すunique_ptr。
   */
  const NodePtr& getLabel() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_label;  ///< 脱出先のラベル (Identifier or null)
};

/**
 * @class ContinueStatement
 * @brief `continue` 文を表すノード。
 *
 * @details
 * オプションで継続先のラベル (Identifier) を持ちます。
 */
class ContinueStatement final : public StatementNode {
 public:
  /**
   * @brief ContinueStatementノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param label 継続先のラベル (Identifier、オプション)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit ContinueStatement(const SourceLocation& location,
                             NodePtr&& label,  // オプション、nullptr の場合あり
                             Node* parent = nullptr);

  ~ContinueStatement() override = default;

  ContinueStatement(const ContinueStatement&) = delete;
  ContinueStatement& operator=(const ContinueStatement&) = delete;
  ContinueStatement(ContinueStatement&&) noexcept = default;
  ContinueStatement& operator=(ContinueStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::ContinueStatement;
  }

  /**
   * @brief ラベルを取得します (非const版)。
   * @return ラベル (Identifier) ノードへの参照 (`NodePtr&`)。ラベルがない場合はnullを指すunique_ptr。
   */
  NodePtr& getLabel() noexcept;

  /**
   * @brief ラベルを取得します (const版)。
   * @return ラベル (Identifier) ノードへのconst参照 (`const NodePtr&`)。ラベルがない場合はnullを指すunique_ptr。
   */
  const NodePtr& getLabel() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_label;  ///< 継続先のラベル (Identifier or null)
};

/**
 * @class ThrowStatement
 * @brief `throw` 文を表すノード。
 *
 * @details
 * スローする値 (式) を持ちます。
 */
class ThrowStatement final : public StatementNode {
 public:
  /**
   * @brief ThrowStatementノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param argument スローする値 (式)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit ThrowStatement(const SourceLocation& location,
                          NodePtr&& argument,
                          Node* parent = nullptr);

  ~ThrowStatement() override = default;

  ThrowStatement(const ThrowStatement&) = delete;
  ThrowStatement& operator=(const ThrowStatement&) = delete;
  ThrowStatement(ThrowStatement&&) noexcept = default;
  ThrowStatement& operator=(ThrowStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::ThrowStatement;
  }

  /**
   * @brief スローする値 (式) を取得します (非const版)。
   * @return 式ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getArgument() noexcept;

  /**
   * @brief スローする値 (式) を取得します (const版)。
   * @return 式ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getArgument() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_argument;  ///< スローする値 (Expression)
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_STATEMENTS_CONTROL_FLOW_H