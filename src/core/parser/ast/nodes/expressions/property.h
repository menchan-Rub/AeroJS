/**
 * @file property.h
 * @brief AST Property ノード (オブジェクトリテラル内) の定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_PROPERTY_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_PROPERTY_H

#include <memory>  // For std::unique_ptr
#include <string>

#include "../node.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Forward declaration
class ExpressionNode;     // Key and Value are expressions
class Identifier;         // Key can be an Identifier
class PrivateIdentifier;  // Key can be a PrivateIdentifier

/**
 * @enum PropertyKind
 * @brief オブジェクトプロパティの種類
 */
enum class PropertyKind {
  Init,  ///< 通常のプロパティ (key: value)
  Get,   ///< ゲッター (get key() { ... })
  Set    ///< セッター (set key(val) { ... })
};

/**
 * @class Property
 * @brief オブジェクトリテラル内のプロパティを表す AST ノード
 */
class Property final : public Node {  // Property is not an expression itself
 public:
  /**
   * @brief Property ノードのコンストラクタ
   * @param key プロパティのキー (Identifier, Literal, or Computed Expression)
   * @param value プロパティの値 (Expression)
   * @param kind プロパティの種類 (Init, Get, Set)
   * @param computed キーが計算されたものか (例: `[keyVar]: value`)
   * @param method プロパティがメソッドか (関数式を持つプロパティ)
   * @param shorthand プロパティがショートハンドか (例: `{ name }` は `{ name: name }` の省略形)
   * @param location ソースコード上の位置情報
   * @param parent 親ノード (オプション)
   */
  Property(std::unique_ptr<Node> key,  // Can be Identifier, Literal, PrivateIdentifier, or Expression
           std::unique_ptr<ExpressionNode> value,
           PropertyKind kind,
           bool computed,
           bool method,
           bool shorthand,
           const SourceLocation& location,
           Node* parent = nullptr);

  ~Property() override = default;

  // --- Getters ---
  const Node& getKey() const;
  Node& getKey();
  const ExpressionNode& getValue() const;
  ExpressionNode& getValue();
  PropertyKind getKind() const noexcept;
  bool isComputed() const noexcept;
  bool isMethod() const noexcept;
  bool isShorthand() const noexcept;

  // Visitor パターンを受け入れる
  void accept(AstVisitor& visitor) override;
  void accept(ConstAstVisitor& visitor) const override;

  // 子ノードを取得 (key, value)
  std::vector<Node*> getChildren() override;
  std::vector<const Node*> getChildren() const override;

  // JSON 表現を生成
  nlohmann::json toJson(bool pretty = false) const override;

  // 文字列表現を生成 (デバッグ用)
  std::string toString() const override;

 private:
  std::unique_ptr<Node> m_key;
  std::unique_ptr<ExpressionNode> m_value;
  PropertyKind m_kind;
  bool m_computed;
  bool m_method;
  bool m_shorthand;
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_EXPRESSIONS_PROPERTY_H