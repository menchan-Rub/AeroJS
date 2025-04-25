/**
 * @file meta_property.h
 * @brief AeroJS AST のメタプロパティ式ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptのメタプロパティ (`new.target`, `import.meta`)
 * を表すASTノード `MetaProperty` を定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_META_PROPERTY_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_META_PROPERTY_H

#include <memory>
#include <string>
#include <vector>

#include "../../common.h"
#include "../../visitors/ast_visitor.h"
#include "../node.h"
#include "expression_node.h"  // 基底クラス
#include "identifier.h"       // meta と property 用

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class MetaProperty
 * @brief メタプロパティ (`new.target`, `import.meta`) を表すノード。
 *
 * @details
 * `meta` 識別子 (`new` または `import`) と `property` 識別子 (`target` または `meta`)
 * を保持します。
 */
class MetaProperty final : public ExpressionNode {
 public:
  /**
   * @brief MetaPropertyノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param meta `new` または `import` を表す Identifier。ムーブされる。
   * @param property `target` または `meta` を表す Identifier。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit MetaProperty(const SourceLocation& location,
                        NodePtr&& meta,
                        NodePtr&& property,
                        Node* parent = nullptr);

  ~MetaProperty() override = default;

  MetaProperty(const MetaProperty&) = delete;
  MetaProperty& operator=(const MetaProperty&) = delete;
  MetaProperty(MetaProperty&&) noexcept = default;
  MetaProperty& operator=(MetaProperty&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::MetaProperty;
  }

  /**
   * @brief `meta` 識別子を取得します (非const版)。
   * @return `meta` Identifier ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getMeta() noexcept;

  /**
   * @brief `meta` 識別子を取得します (const版)。
   * @return `meta` Identifier ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getMeta() const noexcept;

  /**
   * @brief `property` 識別子を取得します (非const版)。
   * @return `property` Identifier ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getProperty() noexcept;

  /**
   * @brief `property` 識別子を取得します (const版)。
   * @return `property` Identifier ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getProperty() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_meta;      ///< `new` または `import` (Identifier)
  NodePtr m_property;  ///< `target` または `meta` (Identifier)
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_EXPRESSIONS_META_PROPERTY_H