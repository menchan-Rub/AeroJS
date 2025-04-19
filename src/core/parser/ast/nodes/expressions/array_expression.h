/**
 * @file array_expression.h
 * @brief AST ArrayExpression ノードの定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_ARRAY_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_ARRAY_EXPRESSION_H

#include "../node.h"
#include <vector>
#include <memory> // For std::unique_ptr

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Forward declaration
class ExpressionNode; // Array elements are expressions
class SpreadElement;  // Can also contain spread elements

/**
 * @class ArrayExpression
 * @brief 配列リテラルを表す AST ノード (例: `[1, "a", , ...spread]`)
 */
class ArrayExpression final : public ExpressionNode {
public:
    // 要素は Expression または SpreadElement、または null (Elision)
    using ElementType = std::unique_ptr<Node>; // Use base Node to hold Expression or SpreadElement

    /**
     * @brief ArrayExpression ノードのコンストラクタ
     * @param elements 配列の要素リスト。要素は ExpressionNode または SpreadElement。
     *                 空要素 (elision) は nullptr で表現します。
     * @param location ソースコード上の位置情報
     * @param parent 親ノード (オプション)
     */
    ArrayExpression(std::vector<ElementType> elements,
                    const SourceLocation& location,
                    Node* parent = nullptr);

    ~ArrayExpression() override = default;

    /**
     * @brief 配列の要素リストを取得します。
     * @return 要素のベクター (ElementType のベクター)。空要素は nullptr。
     */
    const std::vector<ElementType>& getElements() const noexcept;

    // Visitor パターンを受け入れる
    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;

    // 子ノードを取得 (要素リスト内の非nullノード)
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;

    // JSON 表現を生成
    nlohmann::json toJson(bool pretty = false) const override;

    // 文字列表現を生成 (デバッグ用)
    std::string toString() const override;

private:
    std::vector<ElementType> m_elements; ///< 配列要素 (nullptr は Elision)
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_ARRAY_EXPRESSION_H 