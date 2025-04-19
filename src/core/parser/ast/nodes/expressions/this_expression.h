/**
 * @file this_expression.h
 * @brief AST ThisExpression ノードの定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_THIS_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_THIS_EXPRESSION_H

#include "../node.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class ThisExpression
 * @brief `this` キーワードを表す AST ノード
 */
class ThisExpression final : public ExpressionNode {
public:
    /**
     * @brief ThisExpression ノードのコンストラクタ
     * @param location ソースコード上の位置情報
     * @param parent 親ノード (オプション)
     */
    ThisExpression(const SourceLocation& location, Node* parent = nullptr);

    ~ThisExpression() override = default;

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
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_THIS_EXPRESSION_H 