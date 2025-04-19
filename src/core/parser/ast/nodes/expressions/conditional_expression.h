// === src/core/parser/ast/nodes/expressions/conditional_expression.h ===
#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_CONDITIONAL_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_CONDITIONAL_EXPRESSION_H

#include "../node.h"
#include <memory>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Forward declaration
class ExpressionNode;

/**
 * @class ConditionalExpression
 * @brief 条件 (三項) 演算子を表す AST ノード (例: `test ? consequent : alternate`)
 */
class ConditionalExpression final : public ExpressionNode {
public:
    ConditionalExpression(std::unique_ptr<ExpressionNode> test,
                          std::unique_ptr<ExpressionNode> consequent,
                          std::unique_ptr<ExpressionNode> alternate,
                          const SourceLocation& location,
                          Node* parent = nullptr);
    ~ConditionalExpression() override = default;

    // --- Getters ---
    const ExpressionNode& getTest() const;
    ExpressionNode& getTest();
    const ExpressionNode& getConsequent() const;
    ExpressionNode& getConsequent();
    const ExpressionNode& getAlternate() const;
    ExpressionNode& getAlternate();

    // Visitor パターンを受け入れる
    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;

    // 子ノードを取得 (test, consequent, alternate)
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;

    // JSON 表現を生成
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;

private:
    std::unique_ptr<ExpressionNode> m_test;
    std::unique_ptr<ExpressionNode> m_consequent;
    std::unique_ptr<ExpressionNode> m_alternate;
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_CONDITIONAL_EXPRESSION_H 