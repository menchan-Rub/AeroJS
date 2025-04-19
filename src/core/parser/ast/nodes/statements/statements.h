/**
 * @file statements.h
 * @brief AeroJS AST の基本的な文ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの基本的な文（EmptyStatement, BlockStatement）を
 * 表すASTノードを定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_STATEMENTS_STATEMENTS_H
#define AEROJS_PARSER_AST_NODES_STATEMENTS_STATEMENTS_H

#include <vector>
#include <memory>

#include "../node.h"
#include "../../visitors/ast_visitor.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class EmptyStatement
 * @brief 空文 (`;`) を表すノード。
 */
class EmptyStatement final : public Node {
public:
    /**
     * @brief EmptyStatementノードのコンストラクタ。
     * @param location ソースコード内のこのノードの位置情報。
     * @param parent 親ノード (オプション)。
     */
    explicit EmptyStatement(const SourceLocation& location, Node* parent = nullptr);

    ~EmptyStatement() override = default;

    EmptyStatement(const EmptyStatement&) = delete;
    EmptyStatement& operator=(const EmptyStatement&) = delete;
    EmptyStatement(EmptyStatement&&) noexcept = default;
    EmptyStatement& operator=(EmptyStatement&&) noexcept = default;

    NodeType getType() const noexcept override final { return NodeType::EmptyStatement; }

    void accept(AstVisitor* visitor) override final;
    void accept(ConstAstVisitor* visitor) const override final;

    std::vector<Node*> getChildren() override final;
    std::vector<const Node*> getChildren() const override final;

    nlohmann::json toJson(bool pretty = false) const override final;
    std::string toString() const override final;
};

/**
 * @class BlockStatement
 * @brief ブロック文 (`{ ... }`) を表すノード。
 *
 * @details
 * 文のリストを本体として保持します。
 */
class BlockStatement final : public Node {
public:
    /**
     * @brief BlockStatementノードのコンストラクタ。
     * @param location ソースコード内のこのノードの位置情報。
     * @param body ブロック内の文リスト (ムーブされる)。
     * @param parent 親ノード (オプション)。
     */
    explicit BlockStatement(const SourceLocation& location,
                            std::vector<NodePtr>&& body,
                            Node* parent = nullptr);

    ~BlockStatement() override = default;

    BlockStatement(const BlockStatement&) = delete;
    BlockStatement& operator=(const BlockStatement&) = delete;
    BlockStatement(BlockStatement&&) noexcept = default;
    BlockStatement& operator=(BlockStatement&&) noexcept = default;

    NodeType getType() const noexcept override final { return NodeType::BlockStatement; }

    /**
     * @brief ブロック本体の文リストを取得します (非const版)。
     * @return ブロック内の文リストへの参照 (`std::vector<NodePtr>&`)。
     */
    std::vector<NodePtr>& getBody() noexcept;

    /**
     * @brief ブロック本体の文リストを取得します (const版)。
     * @return ブロック内の文リストへのconst参照 (`const std::vector<NodePtr>&`)。
     */
    const std::vector<NodePtr>& getBody() const noexcept;

    void accept(AstVisitor* visitor) override final;
    void accept(ConstAstVisitor* visitor) const override final;

    std::vector<Node*> getChildren() override final;
    std::vector<const Node*> getChildren() const override final;

    nlohmann::json toJson(bool pretty = false) const override final;
    std::string toString() const override final;

private:
    std::vector<NodePtr> m_body; ///< ブロック内の文リスト
};

/**
 * @class ExpressionStatement
 * @brief 式文を表すノード。
 *
 * @details
 * 単一の式 (Expression) をラップし、文として扱います。
 * 主な副作用のために使用されます (例: 関数呼び出し、代入)。
 */
class ExpressionStatement final : public Node {
public:
    /**
     * @brief ExpressionStatementノードのコンストラクタ。
     * @param location ソースコード内のこのノードの位置情報。
     * @param expression この文を構成する式 (ムーブされる)。
     * @param parent 親ノード (オプション)。
     */
    explicit ExpressionStatement(const SourceLocation& location,
                                 NodePtr&& expression, // Expecting an expression node
                                 Node* parent = nullptr);

    ~ExpressionStatement() override = default;

    ExpressionStatement(const ExpressionStatement&) = delete;
    ExpressionStatement& operator=(const ExpressionStatement&) = delete;
    ExpressionStatement(ExpressionStatement&&) noexcept = default;
    ExpressionStatement& operator=(ExpressionStatement&&) noexcept = default;

    NodeType getType() const noexcept override final { return NodeType::ExpressionStatement; }

    /**
     * @brief この文の式を取得します (非const版)。
     * @return 式ノードへの参照 (`NodePtr&`)。
     */
    NodePtr& getExpression() noexcept;

    /**
     * @brief この文の式を取得します (const版)。
     * @return 式ノードへのconst参照 (`const NodePtr&`)。
     */
    const NodePtr& getExpression() const noexcept;

    void accept(AstVisitor* visitor) override final;
    void accept(ConstAstVisitor* visitor) const override final;

    std::vector<Node*> getChildren() override final;
    std::vector<const Node*> getChildren() const override final;

    nlohmann::json toJson(bool pretty = false) const override final;
    std::string toString() const override final;

private:
    NodePtr m_expression; ///< この文を構成する式
};

/**
 * @class IfStatement
 * @brief if文 (`if (test) consequent else alternate`) を表すノード。
 */
class IfStatement final : public Node {
public:
    /**
     * @brief IfStatementノードのコンストラクタ。
     * @param location ソースコード内のこのノードの位置情報。
     * @param test 条件式 (Expression)。ムーブされる。
     * @param consequent 条件が真の場合に実行される文 (Statement)。ムーブされる。
     * @param alternate 条件が偽の場合に実行される文 (Statement、オプション)。ムーブされる。
     * @param parent 親ノード (オプション)。
     */
    explicit IfStatement(const SourceLocation& location,
                         NodePtr&& test,
                         NodePtr&& consequent,
                         NodePtr&& alternate, // Optional, can be nullptr
                         Node* parent = nullptr);

    ~IfStatement() override = default;

    IfStatement(const IfStatement&) = delete;
    IfStatement& operator=(const IfStatement&) = delete;
    IfStatement(IfStatement&&) noexcept = default;
    IfStatement& operator=(IfStatement&&) noexcept = default;

    NodeType getType() const noexcept override final { return NodeType::IfStatement; }

    /**
     * @brief 条件式を取得します (非const版)。
     * @return 条件式ノードへの参照 (`NodePtr&`)。
     */
    NodePtr& getTest() noexcept;

    /**
     * @brief 条件式を取得します (const版)。
     * @return 条件式ノードへのconst参照 (`const NodePtr&`)。
     */
    const NodePtr& getTest() const noexcept;

    /**
     * @brief 条件が真の場合の文 (consequent) を取得します (非const版)。
     * @return consequent 文ノードへの参照 (`NodePtr&`)。
     */
    NodePtr& getConsequent() noexcept;

    /**
     * @brief 条件が真の場合の文 (consequent) を取得します (const版)。
     * @return consequent 文ノードへのconst参照 (`const NodePtr&`)。
     */
    const NodePtr& getConsequent() const noexcept;

    /**
     * @brief 条件が偽の場合の文 (alternate) を取得します (非const版)。
     * @return alternate 文ノードへの参照 (`NodePtr&`)。else節がない場合はnullを指すunique_ptr。
     */
    NodePtr& getAlternate() noexcept;

    /**
     * @brief 条件が偽の場合の文 (alternate) を取得します (const版)。
     * @return alternate 文ノードへのconst参照 (`const NodePtr&`)。else節がない場合はnullを指すunique_ptr。
     */
    const NodePtr& getAlternate() const noexcept;

    void accept(AstVisitor* visitor) override final;
    void accept(ConstAstVisitor* visitor) const override final;

    std::vector<Node*> getChildren() override final;
    std::vector<const Node*> getChildren() const override final;

    nlohmann::json toJson(bool pretty = false) const override final;
    std::string toString() const override final;

private:
    NodePtr m_test;       ///< 条件式 (Expression)
    NodePtr m_consequent; ///< 条件が真の場合の文 (Statement)
    NodePtr m_alternate;  ///< 条件が偽の場合の文 (Statement or null)
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_STATEMENTS_STATEMENTS_H 