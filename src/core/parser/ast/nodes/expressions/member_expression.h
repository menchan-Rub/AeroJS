/**
 * @file member_expression.h
 * @brief AeroJS AST のメンバーアクセス式ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptのメンバーアクセス式
 * (`object.property`, `object[property]`, `object?.property` など) を
 * 表すASTノード `MemberExpression` を定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_MEMBER_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_MEMBER_EXPRESSION_H

#include "../node.h"
#include <memory>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Forward declaration
class ExpressionNode;
class Identifier;
class PrivateIdentifier;
class Super;

/**
 * @class MemberExpression
 * @brief プロパティアクセスを表す AST ノード (例: `obj.prop`, `obj[expr]`, `obj?.prop`)
 */
class MemberExpression final : public ExpressionNode {
public:
    /**
     * @brief MemberExpression ノードのコンストラクタ
     * @param object アクセス対象のオブジェクト (Expression または Super)
     * @param property プロパティ (Identifier, PrivateIdentifier, または Expression)
     * @param computed プロパティアクセスがブラケット表記 (`[]`) かどうか
     * @param optional オプショナルチェイニング (`?.` または `?.[]`) かどうか
     * @param location ソースコード上の位置情報
     * @param parent 親ノード (オプション)
     */
    MemberExpression(std::unique_ptr<Node> object, // Can be Expression or Super
                     std::unique_ptr<Node> property, // Can be Identifier, PrivateIdentifier, or Expression
                     bool computed,
                     bool optional,
                     const SourceLocation& location,
                     Node* parent = nullptr);

    ~MemberExpression() override = default;

    // --- Getters ---
    const Node& getObject() const;
    Node& getObject();
    const Node& getProperty() const;
    Node& getProperty();
    bool isComputed() const noexcept;
    bool isOptional() const noexcept;

    // Visitor パターンを受け入れる
    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;

    // 子ノードを取得 (object, property)
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;

    // JSON 表現を生成
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;

private:
    std::unique_ptr<Node> m_object;
    std::unique_ptr<Node> m_property;
    bool m_computed;
    bool m_optional;
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_MEMBER_EXPRESSION_H 