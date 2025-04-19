/**
 * @file call_expression.h
 * @brief AeroJS AST の関数呼び出しおよびnew式ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの関数呼び出し (`CallExpression`) および
 * `new` 式 (`NewExpression`) を表すASTノードを定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_CALL_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_CALL_EXPRESSION_H

#include "../node.h"
#include <vector>
#include <memory>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Forward declaration
class ExpressionNode;
class SpreadElement;
class Super;

/**
 * @class CallExpression
 * @brief 関数呼び出しを表す AST ノード (例: `func(a, ...b)`, `obj.method?.()`)
 */
class CallExpression final : public ExpressionNode {
public:
    // 引数は Expression または SpreadElement
    using ArgumentType = std::unique_ptr<Node>;

    /**
     * @brief CallExpression ノードのコンストラクタ
     * @param callee 呼び出される関数 (Expression または Super)
     * @param arguments 引数のリスト (Expression または SpreadElement)
     * @param optional オプショナルチェイニング (`?.()`) かどうか
     * @param location ソースコード上の位置情報
     * @param parent 親ノード (オプション)
     */
    CallExpression(std::unique_ptr<Node> callee, // Can be Expression or Super
                   std::vector<ArgumentType> arguments,
                   bool optional,
                   const SourceLocation& location,
                   Node* parent = nullptr);

    ~CallExpression() override = default;

    // --- Getters ---
    const Node& getCallee() const;
    Node& getCallee();
    const std::vector<ArgumentType>& getArguments() const noexcept;
    bool isOptional() const noexcept;

    // Visitor パターンを受け入れる
    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;

    // 子ノードを取得 (callee, arguments)
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;

    // JSON 表現を生成
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;

private:
    std::unique_ptr<Node> m_callee;
    std::vector<ArgumentType> m_arguments;
    bool m_optional;
};

/**
 * @class NewExpression
 * @brief `new` 演算子による呼び出しを表す AST ノード (例: `new Date(arg)`)
 */
class NewExpression final : public ExpressionNode {
public:
    using ArgumentType = std::unique_ptr<Node>;

    NewExpression(std::unique_ptr<ExpressionNode> callee, // Must be an Expression
                  std::vector<ArgumentType> arguments,
                  const SourceLocation& location,
                  Node* parent = nullptr);

    ~NewExpression() override = default;

    // --- Getters ---
    const ExpressionNode& getCallee() const;
    ExpressionNode& getCallee();
    const std::vector<ArgumentType>& getArguments() const noexcept;

    // Visitor パターンを受け入れる
    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;

    // 子ノードを取得 (callee, arguments)
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;

    // JSON 表現を生成
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;

private:
    std::unique_ptr<ExpressionNode> m_callee;
    std::vector<ArgumentType> m_arguments;
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_CALL_EXPRESSION_H 