/**
 * @file statements.cpp
 * @brief AeroJS AST の基本的な文ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`statements.h` で宣言された `EmptyStatement` および
 * `BlockStatement` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "statements.h"
#include <nlohmann/json.hpp>
#include <sstream>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- EmptyStatement Implementation ---

EmptyStatement::EmptyStatement(const SourceLocation& location, Node* parent)
    : Node(NodeType::EmptyStatement, location, parent)
{}

void EmptyStatement::accept(AstVisitor* visitor) {
    visitor->visitEmptyStatement(this);
}

void EmptyStatement::accept(ConstAstVisitor* visitor) const {
    visitor->visitEmptyStatement(this);
}

std::vector<Node*> EmptyStatement::getChildren() {
    return {}; // EmptyStatement has no children
}

std::vector<const Node*> EmptyStatement::getChildren() const {
    return {}; // EmptyStatement has no children
}

nlohmann::json EmptyStatement::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    // EmptyStatement has no specific properties other than type and loc
    return jsonNode;
}

std::string EmptyStatement::toString() const {
    return "EmptyStatement";
}

// --- BlockStatement Implementation ---

BlockStatement::BlockStatement(const SourceLocation& location,
                               std::vector<NodePtr>&& body,
                               Node* parent)
    : Node(NodeType::BlockStatement, location, parent),
      m_body(std::move(body))
{
    // Set parent pointers for children
    for (auto& child : m_body) {
        if (child) {
            child->setParent(this);
        }
    }
}

std::vector<NodePtr>& BlockStatement::getBody() noexcept {
    return m_body;
}

const std::vector<NodePtr>& BlockStatement::getBody() const noexcept {
    return m_body;
}

void BlockStatement::accept(AstVisitor* visitor) {
    visitor->visitBlockStatement(this);
}

void BlockStatement::accept(ConstAstVisitor* visitor) const {
    visitor->visitBlockStatement(this);
}

std::vector<Node*> BlockStatement::getChildren() {
    std::vector<Node*> children;
    children.reserve(m_body.size());
    for (const auto& node_ptr : m_body) {
        children.push_back(node_ptr.get());
    }
    return children;
}

std::vector<const Node*> BlockStatement::getChildren() const {
    std::vector<const Node*> children;
    children.reserve(m_body.size());
    for (const auto& node_ptr : m_body) {
        children.push_back(node_ptr.get());
    }
    return children;
}

nlohmann::json BlockStatement::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["body"] = nlohmann::json::array();
    for (const auto& stmt : m_body) {
        if (stmt) {
            jsonNode["body"].push_back(stmt->toJson(pretty));
        } else {
            jsonNode["body"].push_back(nullptr); // Should not happen typically
        }
    }
    return jsonNode;
}

std::string BlockStatement::toString() const {
    std::ostringstream oss;
    oss << "BlockStatement[bodySize=" << m_body.size() << "]";
    return oss.str();
}

// --- ExpressionStatement Implementation ---

ExpressionStatement::ExpressionStatement(const SourceLocation& location,
                                       NodePtr&& expression,
                                       Node* parent)
    : Node(NodeType::ExpressionStatement, location, parent),
      m_expression(std::move(expression))
{
    if (m_expression) {
        m_expression->setParent(this);
    }
    // Add validation if needed (e.g., ensure m_expression is actually an expression node type)
}

NodePtr& ExpressionStatement::getExpression() noexcept {
    return m_expression;
}

const NodePtr& ExpressionStatement::getExpression() const noexcept {
    return m_expression;
}

void ExpressionStatement::accept(AstVisitor* visitor) {
    visitor->visitExpressionStatement(this);
}

void ExpressionStatement::accept(ConstAstVisitor* visitor) const {
    visitor->visitExpressionStatement(this);
}

std::vector<Node*> ExpressionStatement::getChildren() {
    if (m_expression) {
        return {m_expression.get()};
    }
    return {};
}

std::vector<const Node*> ExpressionStatement::getChildren() const {
    if (m_expression) {
        return {m_expression.get()};
    }
    return {};
}

nlohmann::json ExpressionStatement::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    if (m_expression) {
        jsonNode["expression"] = m_expression->toJson(pretty);
    } else {
        jsonNode["expression"] = nullptr; // Should not happen typically
    }
    return jsonNode;
}

std::string ExpressionStatement::toString() const {
    return "ExpressionStatement"; // Expression details could be added
}

// --- IfStatement Implementation ---

IfStatement::IfStatement(const SourceLocation& location,
                       NodePtr&& test,
                       NodePtr&& consequent,
                       NodePtr&& alternate, // Optional, can be nullptr
                       Node* parent)
    : Node(NodeType::IfStatement, location, parent),
      m_test(std::move(test)),
      m_consequent(std::move(consequent)),
      m_alternate(std::move(alternate))
{
    if (m_test) {
        m_test->setParent(this);
    } else {
        // Test expression is mandatory
        // throw std::runtime_error("IfStatement must have a test expression");
    }
    if (m_consequent) {
        m_consequent->setParent(this);
    } else {
        // Consequent statement is mandatory
        // throw std::runtime_error("IfStatement must have a consequent statement");
    }
    if (m_alternate) {
        m_alternate->setParent(this);
    }
}

NodePtr& IfStatement::getTest() noexcept {
    return m_test;
}

const NodePtr& IfStatement::getTest() const noexcept {
    return m_test;
}

NodePtr& IfStatement::getConsequent() noexcept {
    return m_consequent;
}

const NodePtr& IfStatement::getConsequent() const noexcept {
    return m_consequent;
}

NodePtr& IfStatement::getAlternate() noexcept {
    return m_alternate;
}

const NodePtr& IfStatement::getAlternate() const noexcept {
    return m_alternate;
}

void IfStatement::accept(AstVisitor* visitor) {
    visitor->visitIfStatement(this);
}

void IfStatement::accept(ConstAstVisitor* visitor) const {
    visitor->visitIfStatement(this);
}

std::vector<Node*> IfStatement::getChildren() {
    std::vector<Node*> children;
    if (m_test) children.push_back(m_test.get());
    if (m_consequent) children.push_back(m_consequent.get());
    if (m_alternate) children.push_back(m_alternate.get());
    return children;
}

std::vector<const Node*> IfStatement::getChildren() const {
    std::vector<const Node*> children;
    if (m_test) children.push_back(m_test.get());
    if (m_consequent) children.push_back(m_consequent.get());
    if (m_alternate) children.push_back(m_alternate.get());
    return children;
}

nlohmann::json IfStatement::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    if (m_test) {
        jsonNode["test"] = m_test->toJson(pretty);
    } else {
        jsonNode["test"] = nullptr; // Should not happen
    }
    if (m_consequent) {
        jsonNode["consequent"] = m_consequent->toJson(pretty);
    } else {
        jsonNode["consequent"] = nullptr; // Should not happen
    }
    if (m_alternate) {
        jsonNode["alternate"] = m_alternate->toJson(pretty);
    } else {
        jsonNode["alternate"] = nullptr;
    }
    return jsonNode;
}

std::string IfStatement::toString() const {
    std::string str = "IfStatement";
    if (m_alternate) {
        str += "[else]";
    }
    return str;
}

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 