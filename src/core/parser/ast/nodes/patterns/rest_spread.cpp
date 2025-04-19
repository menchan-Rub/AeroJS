#include "rest_spread.h"
#include "../../visitors/ast_visitor.h"
#include "../node.h" // Need ExpressionNode/PatternNode definitions
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- SpreadElement ---
SpreadElement::SpreadElement(std::unique_ptr<ExpressionNode> argument, const SourceLocation& location, Node* parent)
    : Node(NodeType::SpreadElement, location, parent), m_argument(std::move(argument)) {
    if (!m_argument) throw std::runtime_error("SpreadElement argument cannot be null");
    m_argument->setParent(this);
}
const ExpressionNode& SpreadElement::getArgument() const { return *m_argument; }
ExpressionNode& SpreadElement::getArgument() { return *m_argument; }
void SpreadElement::accept(AstVisitor& visitor) { visitor.Visit(*this); }
void SpreadElement::accept(ConstAstVisitor& visitor) const { visitor.Visit(*this); }
std::vector<Node*> SpreadElement::getChildren() { return {m_argument.get()}; }
std::vector<const Node*> SpreadElement::getChildren() const { return {m_argument.get()}; }
nlohmann::json SpreadElement::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["argument"] = m_argument->toJson(pretty);
    return jsonNode;
}
std::string SpreadElement::toString() const { return "SpreadElement<arg:" + m_argument->toString() + ">"; }

// --- RestElement ---
RestElement::RestElement(std::unique_ptr<PatternNode> argument, const SourceLocation& location, Node* parent)
    : PatternNode(NodeType::RestElement, location, parent), m_argument(std::move(argument)) {
     if (!m_argument) throw std::runtime_error("RestElement argument cannot be null");
    m_argument->setParent(this);
}
const PatternNode& RestElement::getArgument() const { return *m_argument; }
PatternNode& RestElement::getArgument() { return *m_argument; }
void RestElement::accept(AstVisitor& visitor) { visitor.Visit(*this); }
void RestElement::accept(ConstAstVisitor& visitor) const { visitor.Visit(*this); }
std::vector<Node*> RestElement::getChildren() { return {m_argument.get()}; }
std::vector<const Node*> RestElement::getChildren() const { return {m_argument.get()}; }
nlohmann::json RestElement::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["argument"] = m_argument->toJson(pretty);
    return jsonNode;
}
std::string RestElement::toString() const { return "RestElement<arg:" + m_argument->toString() + ">"; }


} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 