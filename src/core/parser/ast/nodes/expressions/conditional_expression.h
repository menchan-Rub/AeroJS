/**
 * @file conditional_expression.h
 * @brief AeroJS AST の条件 (三項) 演算子式ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの条件 (三項) 演算子式
 * (`test ? consequent : alternate`) を表すASTノード
 * `ConditionalExpression` を定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_CONDITIONAL_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_CONDITIONAL_EXPRESSION_H

#include <memory>
#include <string>
#include <vector>

#include "../../common.h"
#include "../../visitors/ast_visitor.h"
#include "../node.h"
#include "expression_node.h"  // 基底クラス

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class ConditionalExpression
 * @brief 条件 (三項) 演算子式 (`test ? consequent : alternate`) を表すノード。
 */
class ConditionalExpression final : public ExpressionNode {
 public:
  /**
   * @brief ConditionalExpressionノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param test 条件式 (Expression)。ムーブされる。
   * @param consequent 条件が真の場合の式 (Expression)。ムーブされる。
   * @param alternate 条件が偽の場合の式 (Expression)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit ConditionalExpression(const SourceLocation& location,
                                 NodePtr&& test,
                                 NodePtr&& consequent,
                                 NodePtr&& alternate,
                                 Node* parent = nullptr);

  ~ConditionalExpression() override = default;

  ConditionalExpression(const ConditionalExpression&) = delete;
  ConditionalExpression& operator=(const ConditionalExpression&) = delete;
  ConditionalExpression(ConditionalExpression&&) noexcept = default;
  ConditionalExpression& operator=(ConditionalExpression&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::ConditionalExpression;
  }

  /**
   * @brief 条件式を取得します (非const版)。
   * @return 条件式ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getTest() noexcept;

  /**
   * @brief 条件式を取得します (const版)。
   * @return 条件式ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getTest() const noexcept;

  /**
   * @brief consequent (真の場合の式) を取得します (非const版)。
   * @return consequent 式ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getConsequent() noexcept;

  /**
   * @brief consequent (真の場合の式) を取得します (const版)。
   * @return consequent 式ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getConsequent() const noexcept;

  /**
   * @brief alternate (偽の場合の式) を取得します (非const版)。
   * @return alternate 式ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getAlternate() noexcept;

  /**
   * @brief alternate (偽の場合の式) を取得します (const版)。
   * @return alternate 式ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getAlternate() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_test;        ///< 条件式 (Expression)
  NodePtr m_consequent;  ///< 真の場合の式 (Expression)
  NodePtr m_alternate;   ///< 偽の場合の式 (Expression)
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_EXPRESSIONS_CONDITIONAL_EXPRESSION_H