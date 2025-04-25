/**
 * @file unary_expression.cpp
 * @brief AST UnaryExpression および UpdateExpression ノードの実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`unary_expression.h` で宣言された
 * `UnaryExpression`, `UpdateExpression` クラスおよび関連ヘルパー関数の実装を提供します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "unary_expression.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

#include "../../visitors/ast_visitor.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- UnaryOperator ---
const char* unaryOperatorToString(UnaryOperator op) {
  switch (op) {
    case UnaryOperator::Minus:
      return "-";
    case UnaryOperator::Plus:
      return "+";
    case UnaryOperator::LogicalNot:
      return "!";
    case UnaryOperator::BitwiseNot:
      return "~";
    case UnaryOperator::TypeOf:
      return "typeof";
    case UnaryOperator::Void:
      return "void";
    case UnaryOperator::Delete:
      return "delete";
    default:
      return "unknown";
  }
}

// --- UnaryExpression ---
UnaryExpression::UnaryExpression(UnaryOperator op,
                                 std::unique_ptr<ExpressionNode> argument,
                                 const SourceLocation& location,
                                 Node* parent)
    : ExpressionNode(NodeType::UnaryExpression, location, parent),
      m_operator(op),
      m_argument(std::move(argument)) {
  if (m_argument) m_argument->setParent(this);
}

UnaryOperator UnaryExpression::getOperator() const noexcept {
  return m_operator;
}

const ExpressionNode& UnaryExpression::getArgument() const {
  if (!m_argument) throw std::runtime_error("UnaryExpression argument is null");
  return *m_argument;
}

ExpressionNode& UnaryExpression::getArgument() {
  if (!m_argument) throw std::runtime_error("UnaryExpression argument is null");
  return *m_argument;
}

void UnaryExpression::accept(AstVisitor& visitor) {
  visitor.Visit(*this);
}

void UnaryExpression::accept(ConstAstVisitor& visitor) const {
  visitor.Visit(*this);
}

std::vector<Node*> UnaryExpression::getChildren() {
  return {m_argument.get()};
}

std::vector<const Node*> UnaryExpression::getChildren() const {
  return {m_argument.get()};
}

nlohmann::json UnaryExpression::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["operator"] = unaryOperatorToString(m_operator);
  jsonNode["argument"] = m_argument ? m_argument->toJson(pretty) : nullptr;
  // ESTreeではUnaryExpressionに対してprefix=trueを使用
  jsonNode["prefix"] = true;
  return jsonNode;
}

std::string UnaryExpression::toString() const {
  std::string argStr = m_argument ? m_argument->toString() : "null";
  return "UnaryExpression<op:'" + std::string(unaryOperatorToString(m_operator)) + "', arg:" + argStr + ">";
}

// --- UpdateOperator ---
const char* updateOperatorToString(UpdateOperator op) {
  switch (op) {
    case UpdateOperator::Increment:
      return "++";
    case UpdateOperator::Decrement:
      return "--";
    default:
      return "unknown";
  }
}

// --- UpdateExpression ---
UpdateExpression::UpdateExpression(UpdateOperator op,
                                   std::unique_ptr<ExpressionNode> argument,
                                   bool prefix,
                                   const SourceLocation& location,
                                   Node* parent)
    : ExpressionNode(NodeType::UpdateExpression, location, parent),
      m_operator(op),
      m_argument(std::move(argument)),
      m_prefix(prefix) {
  if (m_argument) m_argument->setParent(this);
  
  // 引数が有効なLeftHandSideExpressionであることを確認
  if (m_argument) {
    NodeType type = m_argument->getType();
    if (type != NodeType::Identifier && 
        type != NodeType::MemberExpression && 
        type != NodeType::ArrayPattern && 
        type != NodeType::ObjectPattern) {
      throw std::invalid_argument("UpdateExpression引数は有効なLeftHandSideExpressionである必要があります");
    }
  }
}

UpdateOperator UpdateExpression::getOperator() const noexcept {
  return m_operator;
}

const ExpressionNode& UpdateExpression::getArgument() const {
  if (!m_argument) throw std::runtime_error("UpdateExpression argument is null");
  return *m_argument;
}

ExpressionNode& UpdateExpression::getArgument() {
  if (!m_argument) throw std::runtime_error("UpdateExpression argument is null");
  return *m_argument;
}

bool UpdateExpression::isPrefix() const noexcept {
  return m_prefix;
}

void UpdateExpression::accept(AstVisitor& visitor) {
  visitor.Visit(*this);
}

void UpdateExpression::accept(ConstAstVisitor& visitor) const {
  visitor.Visit(*this);
}

std::vector<Node*> UpdateExpression::getChildren() {
  return {m_argument.get()};
}

std::vector<const Node*> UpdateExpression::getChildren() const {
  return {m_argument.get()};
}

nlohmann::json UpdateExpression::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["operator"] = updateOperatorToString(m_operator);
  jsonNode["argument"] = m_argument ? m_argument->toJson(pretty) : nullptr;
  jsonNode["prefix"] = m_prefix;
  return jsonNode;
}

std::string UpdateExpression::toString() const {
  std::string argStr = m_argument ? m_argument->toString() : "null";
  return "UpdateExpression<op:'" + std::string(updateOperatorToString(m_operator)) +
         "', prefix:" + (m_prefix ? "true" : "false") + ", arg:" + argStr + ">";
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs