/**
 * @file conditional_expression.cpp
 * @brief AeroJS AST の条件 (三項) 演算子式ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`conditional_expression.h` で宣言された
 * `ConditionalExpression` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "conditional_expression.h"

#include <cassert>  // assert のため
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>  // std::runtime_error

#include "src/core/parser/ast/utils/json_utils.h"
#include "src/core/parser/ast/utils/string_utils.h"
#include "src/core/parser/ast/visitors/ast_visitor.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

ConditionalExpression::ConditionalExpression(const SourceLocation& location,
                                             NodePtr&& test,
                                             NodePtr&& consequent,
                                             NodePtr&& alternate,
                                             Node* parent)
    : ExpressionNode(NodeType::ConditionalExpression, location, parent),
      m_test(std::move(test)),
      m_consequent(std::move(consequent)),
      m_alternate(std::move(alternate)) {
  if (!m_test) {
    throw std::runtime_error("ConditionalExpression must have a test expression.");
  }
  m_test->setParent(this);
  assert(isExpression(m_test->getType()) && "ConditionalExpression test must be an Expression");

  if (!m_consequent) {
    throw std::runtime_error("ConditionalExpression must have a consequent expression.");
  }
  m_consequent->setParent(this);
  assert(isExpression(m_consequent->getType()) && "ConditionalExpression consequent must be an Expression");

  if (!m_alternate) {
    throw std::runtime_error("ConditionalExpression must have an alternate expression.");
  }
  m_alternate->setParent(this);
  assert(isExpression(m_alternate->getType()) && "ConditionalExpression alternate must be an Expression");
}

NodePtr& ConditionalExpression::getTest() noexcept {
  return m_test;
}

const NodePtr& ConditionalExpression::getTest() const noexcept {
  return m_test;
}

NodePtr& ConditionalExpression::getConsequent() noexcept {
  return m_consequent;
}

const NodePtr& ConditionalExpression::getConsequent() const noexcept {
  return m_consequent;
}

NodePtr& ConditionalExpression::getAlternate() noexcept {
  return m_alternate;
}

const NodePtr& ConditionalExpression::getAlternate() const noexcept {
  return m_alternate;
}

void ConditionalExpression::accept(AstVisitor* visitor) {
  visitor->visitConditionalExpression(this);
}

void ConditionalExpression::accept(ConstAstVisitor* visitor) const {
  visitor->visitConditionalExpression(this);
}

std::vector<Node*> ConditionalExpression::getChildren() {
  // test, consequent, alternate は必須
  return {m_test.get(), m_consequent.get(), m_alternate.get()};
}

std::vector<const Node*> ConditionalExpression::getChildren() const {
  return {m_test.get(), m_consequent.get(), m_alternate.get()};
}

nlohmann::json ConditionalExpression::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["test"] = m_test->toJson(pretty);              // 必須
  jsonNode["consequent"] = m_consequent->toJson(pretty);  // 必須
  jsonNode["alternate"] = m_alternate->toJson(pretty);    // 必須
  return jsonNode;
}

std::string ConditionalExpression::toString() const {
  std::ostringstream oss;
  oss << "ConditionalExpression";
  // oss << "(Test: " << m_test->toString()
  //     << ", Consequent: " << m_consequent->toString()
  //     << ", Alternate: " << m_alternate->toString() << ")";
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs