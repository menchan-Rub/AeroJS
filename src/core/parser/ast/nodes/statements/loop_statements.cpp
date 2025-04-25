/**
 * @file loop_statements.cpp
 * @brief AeroJS AST のループ文ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`loop_statements.h` で宣言されたループ文クラス
 * (`WhileStatement`, `DoWhileStatement`, `ForStatement`)
 * のメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "loop_statements.h"

#include <algorithm>  // std::transform
#include <cassert>    // assert のため
#include <iterator>   // std::back_inserter
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>  // std::runtime_error
#include <vector>

#include "src/core/parser/ast/utils/json_utils.h"
#include "src/core/parser/ast/utils/string_utils.h"
#include "src/core/parser/ast/visitors/ast_visitor.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- WhileStatement 実装 ---

WhileStatement::WhileStatement(const SourceLocation& location,
                               NodePtr&& test,
                               NodePtr&& body,
                               Node* parent)
    : StatementNode(NodeType::WhileStatement, location, parent),
      m_test(std::move(test)),
      m_body(std::move(body)) {
  if (!m_test) {
    throw std::runtime_error("WhileStatement must have a test expression.");
  }
  m_test->setParent(this);
  
  if (!isExpression(m_test->getType())) {
    throw std::runtime_error("WhileStatement test must be an Expression");
  }

  if (!m_body) {
    throw std::runtime_error("WhileStatement must have a body statement.");
  }
  m_body->setParent(this);
  
  if (!isStatement(m_body->getType())) {
    throw std::runtime_error("WhileStatement body must be a Statement");
  }
}

NodePtr& WhileStatement::getTest() noexcept {
  return m_test;
}

const NodePtr& WhileStatement::getTest() const noexcept {
  return m_test;
}

NodePtr& WhileStatement::getBody() noexcept {
  return m_body;
}

const NodePtr& WhileStatement::getBody() const noexcept {
  return m_body;
}

void WhileStatement::accept(AstVisitor* visitor) {
  visitor->visitWhileStatement(this);
}

void WhileStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitWhileStatement(this);
}

std::vector<Node*> WhileStatement::getChildren() {
  // test と body は必須
  return {m_test.get(), m_body.get()};
}

std::vector<const Node*> WhileStatement::getChildren() const {
  return {m_test.get(), m_body.get()};
}

nlohmann::json WhileStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["test"] = m_test->toJson(pretty);
  jsonNode["body"] = m_body->toJson(pretty);
  return jsonNode;
}

std::string WhileStatement::toString() const {
  std::ostringstream oss;
  oss << "WhileStatement";
  oss << "(条件: " << m_test->toString() << ", 本体: " << m_body->toString() << ")";
  return oss.str();
}

// --- DoWhileStatement 実装 ---

DoWhileStatement::DoWhileStatement(const SourceLocation& location,
                                   NodePtr&& body,
                                   NodePtr&& test,
                                   Node* parent)
    : StatementNode(NodeType::DoWhileStatement, location, parent),
      m_body(std::move(body)),
      m_test(std::move(test)) {
  if (!m_body) {
    throw std::runtime_error("DoWhileStatement must have a body statement.");
  }
  m_body->setParent(this);
  
  if (!isStatement(m_body->getType())) {
    throw std::runtime_error("DoWhileStatement body must be a Statement");
  }

  if (!m_test) {
    throw std::runtime_error("DoWhileStatement must have a test expression.");
  }
  m_test->setParent(this);
  
  if (!isExpression(m_test->getType())) {
    throw std::runtime_error("DoWhileStatement test must be an Expression");
  }
}

NodePtr& DoWhileStatement::getBody() noexcept {
  return m_body;
}

const NodePtr& DoWhileStatement::getBody() const noexcept {
  return m_body;
}

NodePtr& DoWhileStatement::getTest() noexcept {
  return m_test;
}

const NodePtr& DoWhileStatement::getTest() const noexcept {
  return m_test;
}

void DoWhileStatement::accept(AstVisitor* visitor) {
  visitor->visitDoWhileStatement(this);
}

void DoWhileStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitDoWhileStatement(this);
}

std::vector<Node*> DoWhileStatement::getChildren() {
  // body と test は必須
  return {m_body.get(), m_test.get()};
}

std::vector<const Node*> DoWhileStatement::getChildren() const {
  return {m_body.get(), m_test.get()};
}

nlohmann::json DoWhileStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["body"] = m_body->toJson(pretty);
  jsonNode["test"] = m_test->toJson(pretty);
  return jsonNode;
}

std::string DoWhileStatement::toString() const {
  std::ostringstream oss;
  oss << "DoWhileStatement";
  oss << "(本体: " << m_body->toString() << ", 条件: " << m_test->toString() << ")";
  return oss.str();
}

// --- ForStatement 実装 ---

ForStatement::ForStatement(const SourceLocation& location,
                           NodePtr&& init,    // オプション
                           NodePtr&& test,    // オプション
                           NodePtr&& update,  // オプション
                           NodePtr&& body,
                           Node* parent)
    : StatementNode(NodeType::ForStatement, location, parent),
      m_init(std::move(init)),
      m_test(std::move(test)),
      m_update(std::move(update)),
      m_body(std::move(body)) {
  if (m_init) {
    m_init->setParent(this);
    
    if (m_init->getType() != NodeType::VariableDeclaration && !isExpression(m_init->getType())) {
      throw std::runtime_error("ForStatement init must be a VariableDeclaration or an Expression");
    }
  }
  
  if (m_test) {
    m_test->setParent(this);
    
    if (!isExpression(m_test->getType())) {
      throw std::runtime_error("ForStatement test must be an Expression");
    }
  }
  
  if (m_update) {
    m_update->setParent(this);
    
    if (!isExpression(m_update->getType())) {
      throw std::runtime_error("ForStatement update must be an Expression");
    }
  }

  if (!m_body) {
    // ループ本体は必須
    throw std::runtime_error("ForStatement must have a body statement.");
  }
  m_body->setParent(this);
  
  if (!isStatement(m_body->getType())) {
    throw std::runtime_error("ForStatement body must be a Statement");
  }
}

NodePtr& ForStatement::getInit() noexcept {
  return m_init;
}

const NodePtr& ForStatement::getInit() const noexcept {
  return m_init;
}

NodePtr& ForStatement::getTest() noexcept {
  return m_test;
}

const NodePtr& ForStatement::getTest() const noexcept {
  return m_test;
}

NodePtr& ForStatement::getUpdate() noexcept {
  return m_update;
}

const NodePtr& ForStatement::getUpdate() const noexcept {
  return m_update;
}

NodePtr& ForStatement::getBody() noexcept {
  return m_body;
}

const NodePtr& ForStatement::getBody() const noexcept {
  return m_body;
}

void ForStatement::accept(AstVisitor* visitor) {
  visitor->visitForStatement(this);
}

void ForStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitForStatement(this);
}

std::vector<Node*> ForStatement::getChildren() {
  std::vector<Node*> children;
  if (m_init) children.push_back(m_init.get());
  if (m_test) children.push_back(m_test.get());
  if (m_update) children.push_back(m_update.get());
  children.push_back(m_body.get());  // body は必須
  return children;
}

std::vector<const Node*> ForStatement::getChildren() const {
  std::vector<const Node*> children;
  if (m_init) children.push_back(m_init.get());
  if (m_test) children.push_back(m_test.get());
  if (m_update) children.push_back(m_update.get());
  children.push_back(m_body.get());
  return children;
}

nlohmann::json ForStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["init"] = m_init ? m_init->toJson(pretty) : nullptr;
  jsonNode["test"] = m_test ? m_test->toJson(pretty) : nullptr;
  jsonNode["update"] = m_update ? m_update->toJson(pretty) : nullptr;
  jsonNode["body"] = m_body->toJson(pretty);  // body は必須
  return jsonNode;
}

std::string ForStatement::toString() const {
  std::ostringstream oss;
  oss << "ForStatement";
  oss << "(初期化: " << (m_init ? m_init->toString() : "なし") 
      << ", 条件: " << (m_test ? m_test->toString() : "なし") 
      << ", 更新: " << (m_update ? m_update->toString() : "なし") 
      << ", 本体: " << m_body->toString() << ")";
  return oss.str();
}

// --- ForInStatement 実装 ---

ForInStatement::ForInStatement(const SourceLocation& location,
                               NodePtr&& left,
                               NodePtr&& right,
                               NodePtr&& body,
                               Node* parent)
    : StatementNode(NodeType::ForInStatement, location, parent),
      m_left(std::move(left)),
      m_right(std::move(right)),
      m_body(std::move(body)) {
  if (!m_left) {
    throw std::runtime_error("ForInStatement must have a left-hand side.");
  }
  m_left->setParent(this);
  
  if (m_left->getType() != NodeType::VariableDeclaration && !isPattern(m_left->getType())) {
    throw std::runtime_error("ForInStatement left must be a VariableDeclaration or a Pattern");
  }

  if (!m_right) {
    throw std::runtime_error("ForInStatement must have a right-hand side expression.");
  }
  m_right->setParent(this);
  
  if (!isExpression(m_right->getType())) {
    throw std::runtime_error("ForInStatement right must be an Expression");
  }

  if (!m_body) {
    throw std::runtime_error("ForInStatement must have a body statement.");
  }
  m_body->setParent(this);
  
  if (!isStatement(m_body->getType())) {
    throw std::runtime_error("ForInStatement body must be a Statement");
  }
}

NodePtr& ForInStatement::getLeft() noexcept {
  return m_left;
}

const NodePtr& ForInStatement::getLeft() const noexcept {
  return m_left;
}

NodePtr& ForInStatement::getRight() noexcept {
  return m_right;
}

const NodePtr& ForInStatement::getRight() const noexcept {
  return m_right;
}

NodePtr& ForInStatement::getBody() noexcept {
  return m_body;
}

const NodePtr& ForInStatement::getBody() const noexcept {
  return m_body;
}

void ForInStatement::accept(AstVisitor* visitor) {
  visitor->visitForInStatement(this);
}

void ForInStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitForInStatement(this);
}

std::vector<Node*> ForInStatement::getChildren() {
  // left, right, body は必須
  return {m_left.get(), m_right.get(), m_body.get()};
}

std::vector<const Node*> ForInStatement::getChildren() const {
  return {m_left.get(), m_right.get(), m_body.get()};
}

nlohmann::json ForInStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["left"] = m_left->toJson(pretty);
  jsonNode["right"] = m_right->toJson(pretty);
  jsonNode["body"] = m_body->toJson(pretty);
  return jsonNode;
}

std::string ForInStatement::toString() const {
  std::ostringstream oss;
  oss << "ForInStatement";
  oss << "(左辺: " << m_left->toString() << ", 右辺: " << m_right->toString() << ", 本体: " << m_body->toString() << ")";
  return oss.str();
}

// --- ForOfStatement 実装 ---

ForOfStatement::ForOfStatement(const SourceLocation& location,
                               bool isAwait,
                               NodePtr&& left,
                               NodePtr&& right,
                               NodePtr&& body,
                               Node* parent)
    : StatementNode(NodeType::ForOfStatement, location, parent),
      m_isAwait(isAwait),
      m_left(std::move(left)),
      m_right(std::move(right)),
      m_body(std::move(body)) {
  if (!m_left) {
    throw std::runtime_error("ForOfStatement must have a left-hand side.");
  }
  m_left->setParent(this);
  
  if (m_left->getType() != NodeType::VariableDeclaration && !isPattern(m_left->getType())) {
    throw std::runtime_error("ForOfStatement left must be a VariableDeclaration or a Pattern");
  }

  if (!m_right) {
    throw std::runtime_error("ForOfStatement must have a right-hand side expression.");
  }
  m_right->setParent(this);
  
  if (!isExpression(m_right->getType())) {
    throw std::runtime_error("ForOfStatement right must be an Expression");
  }

  if (!m_body) {
    throw std::runtime_error("ForOfStatement must have a body statement.");
  }
  m_body->setParent(this);
  
  if (!isStatement(m_body->getType())) {
    throw std::runtime_error("ForOfStatement body must be a Statement");
  }
}

bool ForOfStatement::isAwait() const noexcept {
  return m_isAwait;
}

NodePtr& ForOfStatement::getLeft() noexcept {
  return m_left;
}

const NodePtr& ForOfStatement::getLeft() const noexcept {
  return m_left;
}

NodePtr& ForOfStatement::getRight() noexcept {
  return m_right;
}

const NodePtr& ForOfStatement::getRight() const noexcept {
  return m_right;
}

NodePtr& ForOfStatement::getBody() noexcept {
  return m_body;
}

const NodePtr& ForOfStatement::getBody() const noexcept {
  return m_body;
}

void ForOfStatement::accept(AstVisitor* visitor) {
  visitor->visitForOfStatement(this);
}

void ForOfStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitForOfStatement(this);
}

std::vector<Node*> ForOfStatement::getChildren() {
  // left, right, body は必須
  return {m_left.get(), m_right.get(), m_body.get()};
}

std::vector<const Node*> ForOfStatement::getChildren() const {
  return {m_left.get(), m_right.get(), m_body.get()};
}

nlohmann::json ForOfStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["await"] = m_isAwait;
  jsonNode["left"] = m_left->toJson(pretty);
  jsonNode["right"] = m_right->toJson(pretty);
  jsonNode["body"] = m_body->toJson(pretty);
  return jsonNode;
}

std::string ForOfStatement::toString() const {
  std::ostringstream oss;
  if (m_isAwait) {
    oss << "ForAwaitOfStatement";
  } else {
    oss << "ForOfStatement";
  }
  oss << "(左辺: " << m_left->toString() << ", 右辺: " << m_right->toString() << ", 本体: " << m_body->toString() 
      << (m_isAwait ? ", await: true" : "") << ")";
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs