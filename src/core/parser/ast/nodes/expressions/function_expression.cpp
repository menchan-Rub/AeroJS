#include "function_expression.h"
#include "identifier.h"
#include "../statements/statements.h" // For BlockStatement
#include "../patterns/patterns.h"     // For parameter patterns
#include "../../visitors/ast_visitor.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- FunctionExpression ---
FunctionExpression::FunctionExpression(FunctionData data, const SourceLocation& location, Node* parent)
    : ExpressionNode(NodeType::FunctionExpression, location, parent), m_data(std::move(data)) {
    if (m_data.id) (*m_data.id)->setParent(this);
    for(auto& param : m_data.params) {
        if (!param) throw std::runtime_error("Function parameter cannot be null");
        param->setParent(this);
    }
    if (!m_data.body || m_data.body->getType() != NodeType::BlockStatement) {
         throw std::runtime_error("FunctionExpression body must be a BlockStatement");
    }
    m_data.body->setParent(this);
    // Arrow functions cannot be generators
    if (m_data.generator && getType() == NodeType::ArrowFunctionExpression) {
         throw std::runtime_error("Arrow function cannot be a generator");
    }
}

const std::optional<std::unique_ptr<Identifier>>& FunctionExpression::getId() const { return m_data.id; }
const std::vector<std::unique_ptr<PatternNode>>& FunctionExpression::getParams() const { return m_data.params; }
const BlockStatement& FunctionExpression::getBody() const { return static_cast<const BlockStatement&>(*m_data.body); }
BlockStatement& FunctionExpression::getBody() { return static_cast<BlockStatement&>(*m_data.body); }
bool FunctionExpression::isGenerator() const { return m_data.generator; }
bool FunctionExpression::isAsync() const { return m_data.async; }

void FunctionExpression::accept(AstVisitor& visitor) { visitor.Visit(*this); }
void FunctionExpression::accept(ConstAstVisitor& visitor) const { visitor.Visit(*this); }

std::vector<Node*> FunctionExpression::getChildren() {
    std::vector<Node*> children;
    if (m_data.id) children.push_back(m_data.id->get());
    children.reserve(children.size() + m_data.params.size() + 1);
    for(const auto& p : m_data.params) children.push_back(p.get());
    children.push_back(m_data.body.get());
    return children;
}
std::vector<const Node*> FunctionExpression::getChildren() const {
     std::vector<const Node*> children;
    if (m_data.id) children.push_back(m_data.id->get());
    children.reserve(children.size() + m_data.params.size() + 1);
    for(const auto& p : m_data.params) children.push_back(p.get());
    children.push_back(m_data.body.get());
    return children;
}

nlohmann::json FunctionExpression::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["id"] = m_data.id ? (*m_data.id)->toJson(pretty) : nullptr;
    jsonNode["params"] = nlohmann::json::array();
    for(const auto& p : m_data.params) jsonNode["params"].push_back(p->toJson(pretty));
    jsonNode["body"] = m_data.body->toJson(pretty);
    jsonNode["generator"] = m_data.generator;
    jsonNode["async"] = m_data.async;
    jsonNode["expression"] = false; // Never true for FunctionExpression
    return jsonNode;
}
std::string FunctionExpression::toString() const { return "FunctionExpression"; }


// --- ArrowFunctionExpression ---
ArrowFunctionExpression::ArrowFunctionExpression(FunctionData data, const SourceLocation& location, Node* parent)
     : ExpressionNode(NodeType::ArrowFunctionExpression, location, parent), m_data(std::move(data)) {
    if (m_data.id) throw std::runtime_error("ArrowFunctionExpression cannot have an id");
    if (m_data.generator) throw std::runtime_error("ArrowFunctionExpression cannot be a generator");
    if (!m_data.body) throw std::runtime_error("ArrowFunctionExpression body cannot be null");

     for(auto& param : m_data.params) {
        if (!param) throw std::runtime_error("Arrow function parameter cannot be null");
        param->setParent(this);
    }
    m_data.body->setParent(this);
    // Determine expression flag based on body type
    m_data.expression = (m_data.body->getType() != NodeType::BlockStatement);
}

const std::vector<std::unique_ptr<PatternNode>>& ArrowFunctionExpression::getParams() const { return m_data.params; }
const Node& ArrowFunctionExpression::getBody() const { return *m_data.body; }
Node& ArrowFunctionExpression::getBody() { return *m_data.body; }
bool ArrowFunctionExpression::isGenerator() const { return false; } // Always false
bool ArrowFunctionExpression::isAsync() const { return m_data.async; }
bool ArrowFunctionExpression::isExpression() const { return m_data.expression; }

void ArrowFunctionExpression::accept(AstVisitor& visitor) { visitor.Visit(*this); }
void ArrowFunctionExpression::accept(ConstAstVisitor& visitor) const { visitor.Visit(*this); }

std::vector<Node*> ArrowFunctionExpression::getChildren() {
    std::vector<Node*> children;
    children.reserve(m_data.params.size() + 1);
    for(const auto& p : m_data.params) children.push_back(p.get());
    children.push_back(m_data.body.get());
    return children;
}
std::vector<const Node*> ArrowFunctionExpression::getChildren() const {
    std::vector<const Node*> children;
    children.reserve(m_data.params.size() + 1);
    for(const auto& p : m_data.params) children.push_back(p.get());
    children.push_back(m_data.body.get());
    return children;
}

nlohmann::json ArrowFunctionExpression::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["id"] = nullptr; // Arrow functions have no id
    jsonNode["params"] = nlohmann::json::array();
    for(const auto& p : m_data.params) jsonNode["params"].push_back(p->toJson(pretty));
    jsonNode["body"] = m_data.body->toJson(pretty);
    jsonNode["generator"] = false;
    jsonNode["async"] = m_data.async;
    jsonNode["expression"] = m_data.expression;
    return jsonNode;
}
std::string ArrowFunctionExpression::toString() const { return "ArrowFunctionExpression"; }


} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 