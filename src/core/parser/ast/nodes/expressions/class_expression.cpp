// === src/core/parser/ast/nodes/expressions/class_expression.cpp ===
#include "class_expression.h"
#include "identifier.h"
#include "../declarations/class_declaration.h" // For ClassBody
#include "../../visitors/ast_visitor.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

ClassExpression::ClassExpression(std::optional<std::unique_ptr<Identifier>> id,
                                 std::optional<std::unique_ptr<ExpressionNode>> superClass,
                                 std::unique_ptr<ClassBody> body,
                                 const SourceLocation& location,
                                 Node* parent)
    : ExpressionNode(NodeType::ClassExpression, location, parent),
      m_id(std::move(id)),
      m_superClass(std::move(superClass)),
      m_body(std::move(body)) {
    if (m_id) (*m_id)->setParent(this);
    if (m_superClass) (*m_superClass)->setParent(this);
    if (!m_body) throw std::runtime_error("ClassExpression body cannot be null");
    m_body->setParent(this);
}

const std::optional<std::unique_ptr<Identifier>>& ClassExpression::getId() const { return m_id; }
const std::optional<std::unique_ptr<ExpressionNode>>& ClassExpression::getSuperClass() const { return m_superClass; }
const ClassBody& ClassExpression::getBody() const { return *m_body; }
ClassBody& ClassExpression::getBody() { return *m_body; }

void ClassExpression::accept(AstVisitor& visitor) { visitor.Visit(*this); }
void ClassExpression::accept(ConstAstVisitor& visitor) const { visitor.Visit(*this); }

std::vector<Node*> ClassExpression::getChildren() {
    std::vector<Node*> children;
    if (m_id) children.push_back(m_id->get());
    if (m_superClass) children.push_back(m_superClass->get());
    children.push_back(m_body.get());
    return children;
}
std::vector<const Node*> ClassExpression::getChildren() const {
     std::vector<const Node*> children;
    if (m_id) children.push_back(m_id->get());
    if (m_superClass) children.push_back(m_superClass->get());
    children.push_back(m_body.get());
    return children;
}

nlohmann::json ClassExpression::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["id"] = m_id ? (*m_id)->toJson(pretty) : nullptr;
    jsonNode["superClass"] = m_superClass ? (*m_superClass)->toJson(pretty) : nullptr;
    jsonNode["body"] = m_body->toJson(pretty);
    return jsonNode;
}
std::string ClassExpression::toString() const { return "ClassExpression"; }

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 