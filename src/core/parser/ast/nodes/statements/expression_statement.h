/**
 * @file expression_statement.h
 * @brief AeroJS AST の式文 (Expression Statement) ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * 式が単独で文として扱われる場合 (例: `funcCall();`, `a = 1;`) の
 * ASTノード `ExpressionStatement` を定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_STATEMENTS_EXPRESSION_STATEMENT_H
#define AEROJS_PARSER_AST_NODES_STATEMENTS_EXPRESSION_STATEMENT_H

#include <memory>
#include <nlohmann/json_fwd.hpp>  // nlohmann::json の前方宣言
#include <string>
#include <vector>

#include "src/core/parser/ast/nodes/expressions/expression_node.h"
#include "src/core/parser/ast/nodes/node.h"  // NodePtr のために必要
#include "src/core/parser/ast/nodes/statement_node.h"
#include "src/core/parser/ast/visitors/ast_visitor.h"
#include "src/core/parser/common.h"  // SourceLocation のために必要

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class ExpressionStatement
 * @brief 式文を表すASTノード。
 *
 * @details
 * 単一の式 (Expression) を保持します。
 * 例: `console.log("Hello");` や `i++;` など。
 */
class ExpressionStatement final : public StatementNode {
 public:
  /**
   * @brief ExpressionStatementノードのコンストラクタ。
   * @param location ソースコード内のこのノードの位置情報。
   * @param expression この文を構成する式。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit ExpressionStatement(const SourceLocation& location,
                               NodePtr&& expression,
                               Node* parent = nullptr);

  ~ExpressionStatement() override = default;

  ExpressionStatement(const ExpressionStatement&) = delete;
  ExpressionStatement& operator=(const ExpressionStatement&) = delete;
  ExpressionStatement(ExpressionStatement&&) noexcept = default;
  ExpressionStatement& operator=(ExpressionStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::ExpressionStatement;
  }

  /**
   * @brief この文に含まれる式を取得します (非const版)。
   * @return 式ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getExpression() noexcept;

  /**
   * @brief この文に含まれる式を取得します (const版)。
   * @return 式ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getExpression() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_expression;  ///< この文を構成する式 (Expression)
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_STATEMENTS_EXPRESSION_STATEMENT_H