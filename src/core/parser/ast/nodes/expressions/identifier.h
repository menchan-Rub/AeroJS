/**
 * @file identifier.h
 * @brief AST Identifier ノードの定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの識別子 (`Identifier`) および
 * プライベート識別子 (`PrivateIdentifier`) を表すASTノードを定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_IDENTIFIER_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_IDENTIFIER_H

#include "../node.h" // 基底クラス Node と ExpressionNode
#include <string>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class Identifier
 * @brief 識別子を表す AST ノード (例: variableName, functionName)
 */
class Identifier final : public ExpressionNode {
public:
    /**
     * @brief Identifier ノードのコンストラクタ
     * @param name 識別子の名前
     * @param location ソースコード上の位置情報
     * @param parent 親ノード (オプション)
     */
    Identifier(const std::string& name, const SourceLocation& location, Node* parent = nullptr);

    ~Identifier() override = default;

    /**
     * @brief 識別子の名前を取得します。
     * @return 識別子の名前 (std::string)
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
    std::string m_name; ///< 識別子の名前
};

/**
 * @class PrivateIdentifier
 * @brief JavaScriptのプライベート識別子（クラス内の `#field` など）を表すノード。
 */
class PrivateIdentifier final : public Node {
public:
    /**
     * @brief PrivateIdentifierノードのコンストラクタ。
     * @param location ソースコード内のこのノードの位置情報。
     * @param name プライベート識別子の名前 (`#` を含む)。
     * @param parent 親ノード (オプション)。
     */
    explicit PrivateIdentifier(const SourceLocation& location,
                               std::string name,
                               Node* parent = nullptr);

    ~PrivateIdentifier() override = default;

    PrivateIdentifier(const PrivateIdentifier&) = delete;
    PrivateIdentifier& operator=(const PrivateIdentifier&) = delete;
    PrivateIdentifier(PrivateIdentifier&&) noexcept = default;
    PrivateIdentifier& operator=(PrivateIdentifier&&) noexcept = default;

    NodeType getType() const noexcept override final { return NodeType::PrivateIdentifier; }

    /**
     * @brief プライベート識別子の名前 (`#` を含む) を取得します。
     * @return プライベート識別子の名前 (`std::string`)。
     */
    const std::string& getName() const noexcept;

    void accept(AstVisitor* visitor) override final;
    void accept(ConstAstVisitor* visitor) const override final;

    std::vector<Node*> getChildren() override final;
    std::vector<const Node*> getChildren() const override final;

    nlohmann::json toJson(bool pretty = false) const override final;
    std::string toString() const override final;

private:
    std::string m_name; ///< プライベート識別子の名前 (`#` を含む)
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_IDENTIFIER_H 