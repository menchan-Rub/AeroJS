/**
 * @file sequence_expression.h
 * @brief AeroJS AST のカンマ演算子式ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptのカンマ演算子式 (`expr1, expr2, ...`)
 * を表すASTノード `SequenceExpression` を定義します。
 * カンマ演算子は左から右へ式を評価し、最後の式の値を返します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_SEQUENCE_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_SEQUENCE_EXPRESSION_H

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
 * @class SequenceExpression
 * @brief カンマ演算子式 (`expr1, expr2, ...`) を表すノード。
 *
 * @details
 * 複数の式 (Expressions) のリストを保持します。
 * 最低2つの式が必要です。
 */
class SequenceExpression final : public ExpressionNode {
 public:
  /**
   * @brief SequenceExpressionノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param expressions 式のリスト (Expression のリスト)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit SequenceExpression(const SourceLocation& location,
                              std::vector<NodePtr>&& expressions,
                              Node* parent = nullptr);

  ~SequenceExpression() override = default;

  SequenceExpression(const SequenceExpression&) = delete;
  SequenceExpression& operator=(const SequenceExpression&) = delete;
  SequenceExpression(SequenceExpression&&) noexcept = default;
  SequenceExpression& operator=(SequenceExpression&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::SequenceExpression;
  }

  /**
   * @brief 式のリストを取得します (非const版)。
   * @return 式ノードのリストへの参照 (`std::vector<NodePtr>&`)。
   */
  std::vector<NodePtr>& getExpressions() noexcept;

  /**
   * @brief 式のリストを取得します (const版)。
   * @return 式ノードのリストへのconst参照 (`const std::vector<NodePtr>&`)。
   */
  const std::vector<NodePtr>& getExpressions() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  std::vector<NodePtr> m_expressions;  ///< 式のリスト (Expression)
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_EXPRESSIONS_SEQUENCE_EXPRESSION_H