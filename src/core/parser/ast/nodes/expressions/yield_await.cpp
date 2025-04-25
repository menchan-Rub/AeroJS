#include "yield_await.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

#include "../../visitors/ast_visitor.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- YieldExpression ---
YieldExpression::YieldExpression(std::optional<std::unique_ptr<ExpressionNode>> argument,
                                 bool delegate,
                                 const SourceLocation& location,
                                 Node* parent)
    : ExpressionNode(NodeType::YieldExpression, location, parent),
      m_argument(std::move(argument)),
      m_delegate(delegate) {
  if (m_argument) (*m_argument)->setParent(this);
}

const std::optional<std::unique_ptr<ExpressionNode>>& YieldExpression::getArgument() const {
  return m_argument;
}
bool YieldExpression::isDelegate() const noexcept {
  return m_delegate;
}

void YieldExpression::accept(AstVisitor& visitor) {
  visitor.Visit(*this);
}
void YieldExpression::accept(ConstAstVisitor& visitor) const {
  visitor.Visit(*this);
}

std::vector<Node*> YieldExpression::getChildren() {
  if (m_argument) return {m_argument->get()};
  return {};
}
std::vector<const Node*> YieldExpression::getChildren() const {
  if (m_argument) return {m_argument->get()};
  return {};
}

nlohmann::json YieldExpression::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["argument"] = m_argument ? (*m_argument)->toJson(pretty) : nullptr;
  jsonNode["delegate"] = m_delegate;
  return jsonNode;
}
std::string YieldExpression::toString() const {
  return "YieldExpression";
}

// --- AwaitExpression ---
AwaitExpression::AwaitExpression(std::unique_ptr<ExpressionNode> argument,
                                 const SourceLocation& location,
                                 Node* parent)
    : ExpressionNode(NodeType::AwaitExpression, location, parent),
      m_argument(std::move(argument)) {
  if (!m_argument) throw std::runtime_error("AwaitExpression argument cannot be null");
  m_argument->setParent(this);
}

const ExpressionNode& AwaitExpression::getArgument() const {
  return *m_argument;
}
ExpressionNode& AwaitExpression::getArgument() {
  return *m_argument;
}

void AwaitExpression::accept(AstVisitor& visitor) {
  visitor.Visit(*this);
}
void AwaitExpression::accept(ConstAstVisitor& visitor) const {
  visitor.Visit(*this);
}

std::vector<Node*> AwaitExpression::getChildren() {
  return {m_argument.get()};
}
std::vector<const Node*> AwaitExpression::getChildren() const {
  return {m_argument.get()};
}

nlohmann::json AwaitExpression::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["argument"] = m_argument->toJson(pretty);
  return jsonNode;
}
std::string AwaitExpression::toString() const {
  return "AwaitExpression";
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs