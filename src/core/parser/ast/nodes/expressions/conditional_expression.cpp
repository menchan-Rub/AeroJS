// === src/core/parser/ast/nodes/expressions/conditional_expression.cpp ===
#include "conditional_expression.h"
#include "../../visitors/ast_visitor.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

ConditionalExpression::ConditionalExpression(std::unique_ptr<ExpressionNode> test,
                                             std::unique_ptr<ExpressionNode> consequent,
                                             std::unique_ptr<ExpressionNode> alternate,
                                             const SourceLocation& location,
                                             Node* parent)
    : ExpressionNode(NodeType::ConditionalExpression, location, parent),
      m_test(std::move(test)),
      m_consequent(std::move(consequent)),
      m_alternate(std::move(alternate)) {
    if (!m_test) throw std::runtime_error("ConditionalExpression test cannot be null");
    if (!m_consequent) throw std::runtime_error("ConditionalExpression consequent cannot be null");
    if (!m_alternate) throw std::runtime_error("ConditionalExpression alternate cannot be null");
    m_test->setParent(this);
    m_consequent->setParent(this);
    m_alternate->setParent(this);
}

const ExpressionNode& ConditionalExpression::getTest() const { return *m_test; }
ExpressionNode& ConditionalExpression::getTest() { return *m_test; }
const ExpressionNode& ConditionalExpression::getConsequent() const { return *m_consequent; }
ExpressionNode& ConditionalExpression::getConsequent() { return *m_consequent; }
const ExpressionNode& ConditionalExpression::getAlternate() const { return *m_alternate; }
ExpressionNode& ConditionalExpression::getAlternate() { return *m_alternate; }

void ConditionalExpression::accept(AstVisitor& visitor) { visitor.Visit(*this); }
void ConditionalExpression::accept(ConstAstVisitor& visitor) const { visitor.Visit(*this); }

std::vector<Node*> ConditionalExpression::getChildren() {
    return {m_test.get(), m_consequent.get(), m_alternate.get()};
}
std::vector<const Node*> ConditionalExpression::getChildren() const {
     return {m_test.get(), m_consequent.get(), m_alternate.get()};
}

nlohmann::json ConditionalExpression::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["test"] = m_test->toJson(pretty);
    jsonNode["consequent"] = m_consequent->toJson(pretty);
    jsonNode["alternate"] = m_alternate->toJson(pretty);
    return jsonNode;
}
std::string ConditionalExpression::toString() const { return "ConditionalExpression"; }

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 