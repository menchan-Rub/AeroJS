/**
 * @file unary_expression.h
 * @brief AST UnaryExpression および UpdateExpression ノードの定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの単項演算子 (`UnaryExpression`) および
 * 更新式 (`UpdateExpression`) を表すASTノードを定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_UNARY_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_UNARY_EXPRESSION_H

#include "../node.h"
#include <memory>
#include <string>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Forward declaration
class ExpressionNode;

/**
 * @enum UnaryOperator
 * @brief 単項演算子の種類
 */
enum class UnaryOperator {
    Minus,      // -
    Plus,       // +
    LogicalNot, // !
    BitwiseNot, // ~
    TypeOf,     // typeof
    Void,       // void
    Delete      // delete
};

const char* unaryOperatorToString(UnaryOperator op);

/**
 * @class UnaryExpression
 * @brief 単項演算を表す AST ノード
 */
class UnaryExpression final : public ExpressionNode {
public:
    /**
     * @brief UnaryExpression ノードのコンストラクタ
     * @param op 単項演算子
     * @param argument 演算対象の式
     * @param location ソースコード上の位置情報
     * @param parent 親ノード (オプション)
     */
    UnaryExpression(UnaryOperator op,
                    std::unique_ptr<ExpressionNode> argument,
                    const SourceLocation& location,
                    Node* parent = nullptr);

    ~UnaryExpression() override = default;

    // --- Getters ---
    UnaryOperator getOperator() const noexcept;
    const ExpressionNode& getArgument() const;
    ExpressionNode& getArgument();

    // Visitor パターンを受け入れる
    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;

    // 子ノードを取得 (argument)
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;

    // JSON 表現を生成
    nlohmann::json toJson(bool pretty = false) const override;

    // 文字列表現を生成 (デバッグ用)
    std::string toString() const override;

private:
    UnaryOperator m_operator;
    std::unique_ptr<ExpressionNode> m_argument;
};

/**
 * @enum UpdateOperator
 * @brief 更新演算子の種類
 */
enum class UpdateOperator {
    Increment, // ++
    Decrement  // --
};

const char* updateOperatorToString(UpdateOperator op);

/**
 * @class UpdateExpression
 * @brief 更新演算 (インクリメント/デクリメント) を表す AST ノード
 */
class UpdateExpression final : public ExpressionNode {
public:
    /**
     * @brief UpdateExpression ノードのコンストラクタ
     * @param op 更新演算子 (++ or --)
     * @param argument 演算対象の式 (通常は Identifier or MemberExpression)
     * @param prefix 演算子が前方にあるか (例: `++x` なら true, `x++` なら false)
     * @param location ソースコード上の位置情報
     * @param parent 親ノード (オプション)
     */
    UpdateExpression(UpdateOperator op,
                     std::unique_ptr<ExpressionNode> argument,
                     bool prefix,
                     const SourceLocation& location,
                     Node* parent = nullptr);

    ~UpdateExpression() override = default;

    // --- Getters ---
    UpdateOperator getOperator() const noexcept;
    const ExpressionNode& getArgument() const;
    ExpressionNode& getArgument();
    bool isPrefix() const noexcept;

    // Visitor パターンを受け入れる
    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;

    // 子ノードを取得 (argument)
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;

    // JSON 表現を生成
    nlohmann::json toJson(bool pretty = false) const override;

    // 文字列表現を生成 (デバッグ用)
    std::string toString() const override;

private:
    UpdateOperator m_operator;
    std::unique_ptr<ExpressionNode> m_argument;
    bool m_prefix;
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_UNARY_EXPRESSION_H 