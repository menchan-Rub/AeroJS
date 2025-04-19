/**
 * @file binary_expression.h
 * @brief AeroJS AST の二項演算子および論理演算子ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの二項演算子 (`BinaryExpression`) および
 * 論理演算子 (`LogicalExpression`) を表すASTノードを定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_BINARY_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_BINARY_EXPRESSION_H

#include <string>
#include <memory>
#include <stdexcept> // For std::out_of_range

#include "../node.h"
#include "../../visitors/ast_visitor.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @enum BinaryOperator
 * @brief 二項演算子の種類。
 */
enum class BinaryOperator {
    // Relational
    Equal,              // ==
    NotEqual,           // !=
    StrictEqual,        // ===
    StrictNotEqual,     // !==
    LessThan,           // <
    LessThanOrEqual,    // <=
    GreaterThan,        // >
    GreaterThanOrEqual, // >=
    // Bitwise
    BitwiseAnd,         // &
    BitwiseOr,          // |
    BitwiseXor,         // ^
    LeftShift,          // <<
    RightShift,         // >>
    UnsignedRightShift, // >>>
    // Arithmetic
    Add,                // +
    Subtract,           // -
    Multiply,           // *
    Divide,             // /
    Modulo,             // %
    Exponentiation,     // **
    // Membership
    In,                 // in
    InstanceOf          // instanceof
};

/**
 * @brief BinaryOperator を文字列に変換します。
 */
const char* binaryOperatorToString(BinaryOperator op);

/**
 * @class BinaryExpression
 * @brief 二項演算子式を表すノード。
 */
class BinaryExpression final : public Node {
public:
    /**
     * @brief BinaryExpressionノードのコンストラクタ。
     * @param location ソースコード内のこのノードの位置情報。
     * @param op 二項演算子。
     * @param left 左オペランド (Expression)。ムーブされる。
     * @param right 右オペランド (Expression)。ムーブされる。
     * @param parent 親ノード (オプション)。
     */
    explicit BinaryExpression(const SourceLocation& location,
                              BinaryOperator op,
                              NodePtr&& left,
                              NodePtr&& right,
                              Node* parent = nullptr);

    ~BinaryExpression() override = default;

    BinaryExpression(const BinaryExpression&) = delete;
    BinaryExpression& operator=(const BinaryExpression&) = delete;
    BinaryExpression(BinaryExpression&&) noexcept = default;
    BinaryExpression& operator=(BinaryExpression&&) noexcept = default;

    NodeType getType() const noexcept override final { return NodeType::BinaryExpression; }

    /**
     * @brief 二項演算子を取得します。
     * @return 演算子 (`BinaryOperator`)。
     */
    BinaryOperator getOperator() const noexcept;

    /**
     * @brief 左オペランドを取得します (非const版)。
     * @return 左オペランドノードへの参照 (`NodePtr&`)。
     */
    NodePtr& getLeft() noexcept;

    /**
     * @brief 左オペランドを取得します (const版)。
     * @return 左オペランドノードへのconst参照 (`const NodePtr&`)。
     */
    const NodePtr& getLeft() const noexcept;

    /**
     * @brief 右オペランドを取得します (非const版)。
     * @return 右オペランドノードへの参照 (`NodePtr&`)。
     */
    NodePtr& getRight() noexcept;

    /**
     * @brief 右オペランドを取得します (const版)。
     * @return 右オペランドノードへのconst参照 (`const NodePtr&`)。
     */
    const NodePtr& getRight() const noexcept;

    void accept(AstVisitor* visitor) override final;
    void accept(ConstAstVisitor* visitor) const override final;

    std::vector<Node*> getChildren() override final;
    std::vector<const Node*> getChildren() const override final;

    nlohmann::json toJson(bool pretty = false) const override final;
    std::string toString() const override final;

private:
    BinaryOperator m_operator; ///< 二項演算子
    NodePtr m_left;            ///< 左オペランド
    NodePtr m_right;           ///< 右オペランド
};

/**
 * @enum LogicalOperator
 * @brief 論理演算子の種類。
 */
enum class LogicalOperator {
    LogicalAnd, // &&
    LogicalOr,  // ||
    NullishCoalescing // ??
};

/**
 * @brief LogicalOperator を文字列に変換します。
 */
const char* logicalOperatorToString(LogicalOperator op);

/**
 * @class LogicalExpression
 * @brief 論理演算子式 (`&&`, `||`, `??`) を表すノード。
 *
 * @details
 * 構造は BinaryExpression と似ていますが、短絡評価のセマンティクスが異なります。
 */
class LogicalExpression final : public Node {
public:
    /**
     * @brief LogicalExpressionノードのコンストラクタ。
     * @param location ソースコード内のこのノードの位置情報。
     * @param op 論理演算子。
     * @param left 左オペランド (Expression)。ムーブされる。
     * @param right 右オペランド (Expression)。ムーブされる。
     * @param parent 親ノード (オプション)。
     */
    explicit LogicalExpression(const SourceLocation& location,
                               LogicalOperator op,
                               NodePtr&& left,
                               NodePtr&& right,
                               Node* parent = nullptr);

    ~LogicalExpression() override = default;

    LogicalExpression(const LogicalExpression&) = delete;
    LogicalExpression& operator=(const LogicalExpression&) = delete;
    LogicalExpression(LogicalExpression&&) noexcept = default;
    LogicalExpression& operator=(LogicalExpression&&) noexcept = default;

    NodeType getType() const noexcept override final { return NodeType::LogicalExpression; }

    /**
     * @brief 論理演算子を取得します。
     * @return 演算子 (`LogicalOperator`)。
     */
    LogicalOperator getOperator() const noexcept;

    /**
     * @brief 左オペランドを取得します (非const版)。
     * @return 左オペランドノードへの参照 (`NodePtr&`)。
     */
    NodePtr& getLeft() noexcept;

    /**
     * @brief 左オペランドを取得します (const版)。
     * @return 左オペランドノードへのconst参照 (`const NodePtr&`)。
     */
    const NodePtr& getLeft() const noexcept;

    /**
     * @brief 右オペランドを取得します (非const版)。
     * @return 右オペランドノードへの参照 (`NodePtr&`)。
     */
    NodePtr& getRight() noexcept;

    /**
     * @brief 右オペランドを取得します (const版)。
     * @return 右オペランドノードへのconst参照 (`const NodePtr&`)。
     */
    const NodePtr& getRight() const noexcept;

    void accept(AstVisitor* visitor) override final;
    void accept(ConstAstVisitor* visitor) const override final;

    std::vector<Node*> getChildren() override final;
    std::vector<const Node*> getChildren() const override final;

    nlohmann::json toJson(bool pretty = false) const override final;
    std::string toString() const override final;

private:
    LogicalOperator m_operator; ///< 論理演算子
    NodePtr m_left;             ///< 左オペランド
    NodePtr m_right;            ///< 右オペランド
};


} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_BINARY_EXPRESSION_H 