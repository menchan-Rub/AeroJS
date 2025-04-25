/**
 * @file private_identifier.h
 * @brief AST Private Identifier ノードの定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_PRIVATE_IDENTIFIER_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_PRIVATE_IDENTIFIER_H

#include <string>

#include "../node.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class PrivateIdentifier
 * @brief プライベート識別子を表す AST ノード (例: #privateField)
 * @note これは式としてのみ有効です。宣言には使用されません。
 */
class PrivateIdentifier final : public ExpressionNode {
 public:
  /**
   * @brief PrivateIdentifier ノードのコンストラクタ
   * @param name プライベート識別子の名前 ('#' を含む)
   * @param location ソースコード上の位置情報
   * @param parent 親ノード (オプション)
   */
  PrivateIdentifier(const std::string& name, const SourceLocation& location, Node* parent = nullptr);

  ~PrivateIdentifier() override = default;

  /**
   * @brief プライベート識別子の名前 ('#' を含む) を取得します。
   * @return プライベート識別子の名前 (std::string)
   */
  const std::string& getName() const noexcept;

  // Visitor パターンを受け入れる
  void accept(AstVisitor& visitor) override;
  void accept(ConstAstVisitor& visitor) const override;

  // 子ノードは持たない
  std::vector<Node*> getChildren() override;
  std::vector<const Node*> getChildren() const override;

  // JSON 表現を生成
  nlohmann::json toJson(bool pretty = false) const override;

  // 文字列表現を生成 (デバッグ用)
  std::string toString() const override;

 private:
  std::string m_name;  ///< プライベート識別子の名前 ('#' を含む)
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_EXPRESSIONS_PRIVATE_IDENTIFIER_H