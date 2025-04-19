// === src/core/parser/ast/nodes/expressions/yield_await.h ===
#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_YIELD_AWAIT_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_YIELD_AWAIT_H

#include "../node.h"
#include <memory>
#include <optional>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Forward declaration
class ExpressionNode;

/**
 * @class YieldExpression
 * @brief `yield` 式を表す AST ノード (ジェネレーター関数内)
 */
class YieldExpression final : public ExpressionNode {
public:
    YieldExpression(std::optional<std::unique_ptr<ExpressionNode>> argument,
                    bool delegate, // yield*
                    const SourceLocation& location,
                    Node* parent = nullptr);
    ~YieldExpression() override = default;

    const std::optional<std::unique_ptr<ExpressionNode>>& getArgument() const;
    bool isDelegate() const noexcept;

    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;

private:
    std::optional<std::unique_ptr<ExpressionNode>> m_argument;
    bool m_delegate;
};

/**
 * @class AwaitExpression
 * @brief `await` 式を表す AST ノード (async 関数内)
 */
class AwaitExpression final : public ExpressionNode {
public:
     AwaitExpression(std::unique_ptr<ExpressionNode> argument,
                    const SourceLocation& location,
                    Node* parent = nullptr);
    ~AwaitExpression() override = default;

    const ExpressionNode& getArgument() const;
    ExpressionNode& getArgument();

    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;

private:
    std::unique_ptr<ExpressionNode> m_argument;
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_YIELD_AWAIT_H 