/**
 * @file binary_expression.cpp
 * @brief AST BinaryExpression および LogicalExpression ノードの実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 */

#include "binary_expression.h"
#include "../../visitors/ast_visitor.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- BinaryOperator ---
const char* binaryOperatorToString(BinaryOperator op) {
    switch (op) {
        case BinaryOperator::Add: return "+";
        case BinaryOperator::Subtract: return "-";
        case BinaryOperator::Multiply: return "*";
        case BinaryOperator::Divide: return "/";
        case BinaryOperator::Modulo: return "%";
        case BinaryOperator::Exponentiation: return "**";
        case BinaryOperator::BitwiseOr: return "|";
        case BinaryOperator::BitwiseAnd: return "&";
        case BinaryOperator::BitwiseXor: return "^";
        case BinaryOperator::LeftShift: return "<<";
        case BinaryOperator::RightShift: return ">>";
        case BinaryOperator::UnsignedRightShift: return ">>>";
        case BinaryOperator::Equal: return "==";
        case BinaryOperator::NotEqual: return "!=";
        case BinaryOperator::StrictEqual: return "===";
        case BinaryOperator::StrictNotEqual: return "!==";
        case BinaryOperator::LessThan: return "<";
        case BinaryOperator::LessThanOrEqual: return "<=";
        case BinaryOperator::GreaterThan: return ">";
        case BinaryOperator::GreaterThanOrEqual: return ">=";
        case BinaryOperator::In: return "in";
        case BinaryOperator::InstanceOf: return "instanceof";
        default: return "unknown";
    }
}

// --- BinaryExpression ---
BinaryExpression::BinaryExpression(BinaryOperator op,
                                   std::unique_ptr<ExpressionNode> left,
                                   std::unique_ptr<ExpressionNode> right,
                                   const SourceLocation& location,
                                   Node* parent)
    : ExpressionNode(NodeType::BinaryExpression, location, parent),
      m_operator(op),
      m_left(std::move(left)),
      m_right(std::move(right)) {
    if (m_left) m_left->setParent(this);
    if (m_right) m_right->setParent(this);
}

BinaryOperator BinaryExpression::getOperator() const noexcept {
    return m_operator;
}

const ExpressionNode& BinaryExpression::getLeft() const {
    if (!m_left) throw std::runtime_error("BinaryExpression left operand is null");
    return *m_left;
}

ExpressionNode& BinaryExpression::getLeft() {
    if (!m_left) throw std::runtime_error("BinaryExpression left operand is null");
    return *m_left;
}

const ExpressionNode& BinaryExpression::getRight() const {
     if (!m_right) throw std::runtime_error("BinaryExpression right operand is null");
    return *m_right;
}

ExpressionNode& BinaryExpression::getRight() {
     if (!m_right) throw std::runtime_error("BinaryExpression right operand is null");
    return *m_right;
}

void BinaryExpression::accept(AstVisitor& visitor) {
    visitor.Visit(*this);
}

void BinaryExpression::accept(ConstAstVisitor& visitor) const {
    visitor.Visit(*this);
}

std::vector<Node*> BinaryExpression::getChildren() {
    return {m_left.get(), m_right.get()};
}

std::vector<const Node*> BinaryExpression::getChildren() const {
    return {m_left.get(), m_right.get()};
}

nlohmann::json BinaryExpression::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["operator"] = binaryOperatorToString(m_operator);
    jsonNode["left"] = m_left ? m_left->toJson(pretty) : nullptr;
    jsonNode["right"] = m_right ? m_right->toJson(pretty) : nullptr;
    return jsonNode;
}

std::string BinaryExpression::toString() const {
    std::string leftStr = m_left ? m_left->toString() : "null";
    std::string rightStr = m_right ? m_right->toString() : "null";
    return "BinaryExpression<op:'" + std::string(binaryOperatorToString(m_operator)) +
           "', left:" + leftStr + ", right:" + rightStr + ">";
}


// --- LogicalOperator ---
const char* logicalOperatorToString(LogicalOperator op) {
    switch (op) {
        case LogicalOperator::LogicalAnd: return "&&";
        case LogicalOperator::LogicalOr: return "||";
        case LogicalOperator::Coalesce: return "??";
        default: return "unknown";
    }
}

// --- LogicalExpression ---
LogicalExpression::LogicalExpression(LogicalOperator op,
                                     std::unique_ptr<ExpressionNode> left,
                                     std::unique_ptr<ExpressionNode> right,
                                     const SourceLocation& location,
                                     Node* parent)
    : ExpressionNode(NodeType::LogicalExpression, location, parent),
      m_operator(op),
      m_left(std::move(left)),
      m_right(std::move(right)) {
    if (m_left) m_left->setParent(this);
    if (m_right) m_right->setParent(this);
}

LogicalOperator LogicalExpression::getOperator() const noexcept {
    return m_operator;
}

const ExpressionNode& LogicalExpression::getLeft() const {
    if (!m_left) throw std::runtime_error("LogicalExpression left operand is null");
    return *m_left;
}

ExpressionNode& LogicalExpression::getLeft() {
    if (!m_left) throw std::runtime_error("LogicalExpression left operand is null");
    return *m_left;
}

const ExpressionNode& LogicalExpression::getRight() const {
     if (!m_right) throw std::runtime_error("LogicalExpression right operand is null");
    return *m_right;
}

ExpressionNode& LogicalExpression::getRight() {
     if (!m_right) throw std::runtime_error("LogicalExpression right operand is null");
    return *m_right;
}

void LogicalExpression::accept(AstVisitor& visitor) {
    visitor.Visit(*this);
}

void LogicalExpression::accept(ConstAstVisitor& visitor) const {
    visitor.Visit(*this);
}

std::vector<Node*> LogicalExpression::getChildren() {
    return {m_left.get(), m_right.get()};
}

std::vector<const Node*> LogicalExpression::getChildren() const {
    return {m_left.get(), m_right.get()};
}

nlohmann::json LogicalExpression::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["operator"] = logicalOperatorToString(m_operator);
    jsonNode["left"] = m_left ? m_left->toJson(pretty) : nullptr;
    jsonNode["right"] = m_right ? m_right->toJson(pretty) : nullptr;
    return jsonNode;
}

std::string LogicalExpression::toString() const {
    std::string leftStr = m_left ? m_left->toString() : "null";
    std::string rightStr = m_right ? m_right->toString() : "null";
    return "LogicalExpression<op:'" + std::string(logicalOperatorToString(m_operator)) +
           "', left:" + leftStr + ", right:" + rightStr + ">";
}


} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 