/**
 * @file object_expression.h
 * @brief AST ObjectExpression ノードの定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_OBJECT_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_OBJECT_EXPRESSION_H

#include "../node.h"
#include <vector>
#include <memory> // For std::unique_ptr

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Forward declaration
class Property;      // Properties are the main content
class SpreadElement; // Can also contain spread elements

/**
 * @class ObjectExpression
 * @brief オブジェクトリテラルを表す AST ノード (例: `{ key: "value", ...spread }`)
 */
class ObjectExpression final : public ExpressionNode {
public:
    // プロパティリストの要素型 (Property または SpreadElement)
    using PropertyType = std::unique_ptr<Node>; // Base Node to hold Property or SpreadElement

    /**
     * @brief ObjectExpression ノードのコンストラクタ
     * @param properties プロパティ (Property) またはスプレッド要素 (SpreadElement) のリスト
     * @param location ソースコード上の位置情報
     * @param parent 親ノード (オプション)
     */
    ObjectExpression(std::vector<PropertyType> properties,
                     const SourceLocation& location,
                     Node* parent = nullptr);

    ~ObjectExpression() override = default;

    /**
     * @brief プロパティ/スプレッド要素のリストを取得します。
     * @return 要素のベクター (PropertyType のベクター)
     */
    const std::vector<PropertyType>& getProperties() const noexcept;

    // Visitor パターンを受け入れる
    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;

    // 子ノードを取得 (プロパティリスト内のノード)
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;

    // JSON 表現を生成
    nlohmann::json toJson(bool pretty = false) const override;

    // 文字列表現を生成 (デバッグ用)
    std::string toString() const override;

private:
    std::vector<PropertyType> m_properties; ///< プロパティ/スプレッド要素のリスト
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_OBJECT_EXPRESSION_H 