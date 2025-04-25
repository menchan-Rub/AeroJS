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

#include <cassert>  // assert のため
#include <nlohmann/json.hpp>
#include <sstream>
#include <vector>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- EmptyStatement 実装 ---

EmptyStatement::EmptyStatement(const SourceLocation& location, Node* parent)
    : Node(NodeType::EmptyStatement, location, parent) {
}

void EmptyStatement::accept(AstVisitor* visitor) {
  visitor->visitEmptyStatement(this);
}

void EmptyStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitEmptyStatement(this);
}

std::vector<Node*> EmptyStatement::getChildren() {
  return {};  // EmptyStatement は子を持たない
}

std::vector<const Node*> EmptyStatement::getChildren() const {
  return {};  // EmptyStatement は子を持たない
}

nlohmann::json EmptyStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  // EmptyStatement は type と loc 以外のプロパティを持たない
  return jsonNode;
}

std::string EmptyStatement::toString() const {
  return "EmptyStatement";
}

// --- BlockStatement 実装 ---

BlockStatement::BlockStatement(const SourceLocation& location,
                               std::vector<NodePtr>&& body,
                               Node* parent)
    : Node(NodeType::BlockStatement, location, parent),
      m_body(std::move(body)) {
  // 子要素の親ポインタを設定
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
      jsonNode["body"].push_back(nullptr);  // 通常は発生しないはず
    }
  }
  return jsonNode;
}

std::string BlockStatement::toString() const {
  std::ostringstream oss;
  oss << "BlockStatement[bodySize=" << m_body.size() << "]";
  return oss.str();
}

// --- ExpressionStatement 実装 ---

ExpressionStatement::ExpressionStatement(const SourceLocation& location,
                                         NodePtr&& expression,
                                         Node* parent)
    : Node(NodeType::ExpressionStatement, location, parent),
      m_expression(std::move(expression)) {
  if (m_expression) {
    m_expression->setParent(this);
  }
  // 式ノードの型チェック
  if (m_expression && !isExpression(m_expression->getType())) {
    throw std::invalid_argument("ExpressionStatement には Expression 型のノードが必要です");
  }
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
    jsonNode["expression"] = nullptr;  // 異常系の場合
  }
  return jsonNode;
}

std::string ExpressionStatement::toString() const {
  std::ostringstream oss;
  oss << "ExpressionStatement[";
  if (m_expression) {
    oss << m_expression->toString();
  } else {
    oss << "null";
  }
  oss << "]";
  return oss.str();
}

// --- IfStatement 実装 ---

IfStatement::IfStatement(const SourceLocation& location,
                         NodePtr&& test,
                         NodePtr&& consequent,
                         NodePtr&& alternate,  // オプション、nullptr の場合あり
                         Node* parent)
    : Node(NodeType::IfStatement, location, parent),
      m_test(std::move(test)),
      m_consequent(std::move(consequent)),
      m_alternate(std::move(alternate)) {
  if (m_test) {
    m_test->setParent(this);
    // 条件式は Expression 型である必要がある
    if (!isExpression(m_test->getType())) {
      throw std::invalid_argument("IfStatement の条件式は Expression 型である必要があります");
    }
  } else {
    // test 式は必須
    throw std::runtime_error("IfStatement には条件式が必要です");
  }
  if (m_consequent) {
    m_consequent->setParent(this);
    // then 節は Statement 型である必要がある
    if (!isStatement(m_consequent->getType())) {
      throw std::invalid_argument("IfStatement の then 節は Statement 型である必要があります");
    }
  } else {
    // consequent 文は必須
    throw std::runtime_error("IfStatement には then 節が必要です");
  }
  if (m_alternate) {
    m_alternate->setParent(this);
    // else 節は Statement 型である必要がある
    if (!isStatement(m_alternate->getType())) {
      throw std::invalid_argument("IfStatement の else 節は Statement 型である必要があります");
    }
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
  children.reserve(3);  // 最大3つの子ノード
  if (m_test) children.push_back(m_test.get());
  if (m_consequent) children.push_back(m_consequent.get());
  if (m_alternate) children.push_back(m_alternate.get());
  return children;
}

std::vector<const Node*> IfStatement::getChildren() const {
  std::vector<const Node*> children;
  children.reserve(3);  // 最大3つの子ノード
  if (m_test) children.push_back(m_test.get());
  if (m_consequent) children.push_back(m_consequent.get());
  if (m_alternate) children.push_back(m_alternate.get());
  return children;
}

nlohmann::json IfStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["test"] = m_test->toJson(pretty);
  jsonNode["consequent"] = m_consequent->toJson(pretty);
  if (m_alternate) {
    jsonNode["alternate"] = m_alternate->toJson(pretty);
  } else {
    jsonNode["alternate"] = nullptr;  // else 節がない場合は null
  }
  return jsonNode;
}

std::string IfStatement::toString() const {
  std::ostringstream oss;
  oss << "IfStatement[";
  if (m_test) {
    oss << "test=" << m_test->toString();
  }
  if (m_alternate) {
    oss << ", hasElse=true";
  }
  oss << "]";
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs