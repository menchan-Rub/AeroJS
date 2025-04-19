// === src/core/parser/ast/nodes/expressions/sequence_expression.h ===
#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_SEQUENCE_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_SEQUENCE_EXPRESSION_H

#include "../node.h"
#include <vector>
#include <memory>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Forward declaration
class ExpressionNode;

/**
 * @class SequenceExpression
 * @brief カンマ演算子によって結合された一連の式を表す AST ノード (例: `a, b, c`)
 */
class SequenceExpression final : public ExpressionNode {
public:
    using ExpressionList = std::vector<std::unique_ptr<ExpressionNode>>;

    SequenceExpression(ExpressionList expressions,
                       const SourceLocation& location,
                       Node* parent = nullptr);
    ~SequenceExpression() override = default;

    const ExpressionList& getExpressions() const noexcept;

    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;

private:
    ExpressionList m_expressions;
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_SEQUENCE_EXPRESSION_H 