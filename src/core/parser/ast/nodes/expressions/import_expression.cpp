// === src/core/parser/ast/nodes/expressions/import_expression.cpp ===
#include "import_expression.h"
#include "../../visitors/ast_visitor.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

ImportExpression::ImportExpression(std::unique_ptr<ExpressionNode> source,
                                   const SourceLocation& location,
                                   Node* parent)
    : ExpressionNode(NodeType::ImportExpression, location, parent),
      m_source(std::move(source)) {
    if (!m_source) throw std::runtime_error("ImportExpression source cannot be null");
    m_source->setParent(this);
}

const ExpressionNode& ImportExpression::getSource() const { return *m_source; }
ExpressionNode& ImportExpression::getSource() { return *m_source; }

void ImportExpression::accept(AstVisitor& visitor) { visitor.Visit(*this); }
void ImportExpression::accept(ConstAstVisitor& visitor) const { visitor.Visit(*this); }

std::vector<Node*> ImportExpression::getChildren() { return {m_source.get()}; }
std::vector<const Node*> ImportExpression::getChildren() const { return {m_source.get()}; }

nlohmann::json ImportExpression::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["source"] = m_source->toJson(pretty);
    return jsonNode;
}
std::string ImportExpression::toString() const { return "ImportExpression"; }

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 