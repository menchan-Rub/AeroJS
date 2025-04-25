/**
 * @file import_expression.h
 * @brief AeroJS AST の動的インポート式ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの動的インポート式 (`import(source)`) を表す
 * ASTノード `ImportExpression` を定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_IMPORT_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_IMPORT_EXPRESSION_H

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
 * @class ImportExpression
 * @brief 動的インポート式 (`import(source)`) を表すノード。
 *
 * @details
 * モジュールを動的にロードするために使用されます。
 * インポートするモジュールのソース指定子 (通常は文字列リテラルまたは式) を持ちます。
 * 将来的にオプション (import attributes) を持つ可能性があります。
 */
class ImportExpression final : public ExpressionNode {
 public:
  /**
   * @brief ImportExpressionノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param source インポートするモジュールのソース指定子 (Expression)。ムーブされる。
   * @param options インポートオプション (例: import attributes)。現在は未サポートのため nullptr。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit ImportExpression(const SourceLocation& location,
                            NodePtr&& source,
                            NodePtr&& options,  // 将来用 (Import Attributes)
                            Node* parent = nullptr);

  ~ImportExpression() override = default;

  ImportExpression(const ImportExpression&) = delete;
  ImportExpression& operator=(const ImportExpression&) = delete;
  ImportExpression(ImportExpression&&) noexcept = default;
  ImportExpression& operator=(ImportExpression&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::ImportExpression;
  }

  /**
   * @brief ソース指定子を取得します (非const版)。
   * @return ソース指定子 (Expression) ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getSource() noexcept;

  /**
   * @brief ソース指定子を取得します (const版)。
   * @return ソース指定子 (Expression) ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getSource() const noexcept;

  /**
   * @brief インポートオプションを取得します (非const版)。
   * @return オプションノードへの参照 (`NodePtr&`)。現在は常に null を指す。
   */
  NodePtr& getOptions() noexcept;

  /**
   * @brief インポートオプションを取得します (const版)。
   * @return オプションノードへのconst参照 (`const NodePtr&`)。現在は常に null を指す。
   */
  const NodePtr& getOptions() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_source;   ///< モジュールのソース指定子 (Expression)
  NodePtr m_options;  ///< インポートオプション (将来用、現在は null)
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_EXPRESSIONS_IMPORT_EXPRESSION_H