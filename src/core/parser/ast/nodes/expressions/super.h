/**
 * @file super.h
 * @brief AeroJS AST の super キーワードノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの `super` キーワード
 * (例: `super()`, `super.method()`) を表すASTノード `Super` を定義します。
 * このノード自体は子を持ちませんが、`CallExpression` や `MemberExpression`
 * の一部として使用されます。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_SUPER_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_SUPER_H

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
 * @class Super
 * @brief `super` キーワードを表すノード。
 *
 * @details
 * クラスのコンストラクタやメソッド内で親クラスを参照するために使用されます。
 * このノード自体は子を持ちません。
 */
class Super final : public ExpressionNode {
 public:
  /**
   * @brief Superノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param parent 親ノード (オプション)。
   */
  explicit Super(const SourceLocation& location, Node* parent = nullptr);

  ~Super() override = default;

  Super(const Super&) = delete;
  Super& operator=(const Super&) = delete;
  Super(Super&&) noexcept = default;
  Super& operator=(Super&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::Super;
  }

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_EXPRESSIONS_SUPER_H