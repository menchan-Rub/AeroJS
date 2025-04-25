#include "template_literal.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

#include "../../visitors/ast_visitor.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- TemplateElement ---
TemplateElement::TemplateElement(TemplateElementValue value, bool tail, const SourceLocation& location, Node* parent)
    : Node(NodeType::TemplateElement, location, parent), m_value(std::move(value)), m_tail(tail) {
}

const TemplateElementValue& TemplateElement::getValue() const {
  return m_value;
}
bool TemplateElement::isTail() const noexcept {
  return m_tail;
}

void TemplateElement::accept(AstVisitor& visitor) {
  visitor.Visit(*this);
}
void TemplateElement::accept(ConstAstVisitor& visitor) const {
  visitor.Visit(*this);
}

std::vector<Node*> TemplateElement::getChildren() {
  return {};
}
std::vector<const Node*> TemplateElement::getChildren() const {
  return {};
}

nlohmann::json TemplateElement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["value"] = {
      {"raw", m_value.raw},
      {"cooked", m_value.cooked}  // Consider making cooked optional/nullable
  };
  jsonNode["tail"] = m_tail;
  return jsonNode;
}
std::string TemplateElement::toString() const {
  return "TemplateElement<raw:'" + m_value.raw + "'>";
}

// --- TemplateLiteral ---
TemplateLiteral::TemplateLiteral(ElementList quasis, ExpressionList expressions, const SourceLocation& location, Node* parent)
    : ExpressionNode(NodeType::TemplateLiteral, location, parent),
      m_quasis(std::move(quasis)),
      m_expressions(std::move(expressions)) {
  if (m_quasis.empty()) throw std::runtime_error("TemplateLiteral must have at least one quasi element");
  if (m_quasis.size() != m_expressions.size() + 1) {
    // throw std::runtime_error("Mismatch between quasis and expressions count in TemplateLiteral");
  }
  for (auto& q : m_quasis) {
    if (!q) throw std::runtime_error("TemplateLiteral quasi cannot be null");
    q->setParent(this);
  }
  for (auto& e : m_expressions) {
    if (!e) throw std::runtime_error("TemplateLiteral expression cannot be null");
    e->setParent(this);
  }
}

const TemplateLiteral::ElementList& TemplateLiteral::getQuasis() const {
  return m_quasis;
}
const TemplateLiteral::ExpressionList& TemplateLiteral::getExpressions() const {
  return m_expressions;
}

void TemplateLiteral::accept(AstVisitor& visitor) {
  visitor.Visit(*this);
}
void TemplateLiteral::accept(ConstAstVisitor& visitor) const {
  visitor.Visit(*this);
}

std::vector<Node*> TemplateLiteral::getChildren() {
  std::vector<Node*> children;
  children.reserve(m_quasis.size() + m_expressions.size());
  for (const auto& q : m_quasis) children.push_back(q.get());
  for (const auto& e : m_expressions) children.push_back(e.get());
  return children;
}
std::vector<const Node*> TemplateLiteral::getChildren() const {
  std::vector<const Node*> children;
  children.reserve(m_quasis.size() + m_expressions.size());
  for (const auto& q : m_quasis) children.push_back(q.get());
  for (const auto& e : m_expressions) children.push_back(e.get());
  return children;
}

nlohmann::json TemplateLiteral::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["quasis"] = nlohmann::json::array();
  for (const auto& q : m_quasis) jsonNode["quasis"].push_back(q->toJson(pretty));
  jsonNode["expressions"] = nlohmann::json::array();
  for (const auto& e : m_expressions) jsonNode["expressions"].push_back(e->toJson(pretty));
  return jsonNode;
}
std::string TemplateLiteral::toString() const {
  return "TemplateLiteral";
}

// --- TaggedTemplateExpression ---
TaggedTemplateExpression::TaggedTemplateExpression(std::unique_ptr<ExpressionNode> tag, std::unique_ptr<TemplateLiteral> quasi, const SourceLocation& location, Node* parent)
    : ExpressionNode(NodeType::TaggedTemplateExpression, location, parent),
      m_tag(std::move(tag)),
      m_quasi(std::move(quasi)) {
  if (!m_tag) throw std::runtime_error("TaggedTemplateExpression tag cannot be null");
  if (!m_quasi) throw std::runtime_error("TaggedTemplateExpression quasi cannot be null");
  m_tag->setParent(this);
  m_quasi->setParent(this);
}

const ExpressionNode& TaggedTemplateExpression::getTag() const {
  return *m_tag;
}
ExpressionNode& TaggedTemplateExpression::getTag() {
  return *m_tag;
}
const TemplateLiteral& TaggedTemplateExpression::getQuasi() const {
  return *m_quasi;
}
TemplateLiteral& TaggedTemplateExpression::getQuasi() {
  return *m_quasi;
}

void TaggedTemplateExpression::accept(AstVisitor& visitor) {
  visitor.Visit(*this);
}
void TaggedTemplateExpression::accept(ConstAstVisitor& visitor) const {
  visitor.Visit(*this);
}

std::vector<Node*> TaggedTemplateExpression::getChildren() {
  return {m_tag.get(), m_quasi.get()};
}
std::vector<const Node*> TaggedTemplateExpression::getChildren() const {
  return {m_tag.get(), m_quasi.get()};
}

nlohmann::json TaggedTemplateExpression::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["tag"] = m_tag->toJson(pretty);
  jsonNode["quasi"] = m_quasi->toJson(pretty);
  return jsonNode;
}
std::string TaggedTemplateExpression::toString() const {
  return "TaggedTemplateExpression";
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs