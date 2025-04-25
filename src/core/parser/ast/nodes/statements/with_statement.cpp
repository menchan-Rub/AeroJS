/**
 * @file with_statement.cpp
 * @brief AeroJS AST の with 文ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`with_statement.h` で宣言された `WithStatement`
 * クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "with_statement.h"

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

WithStatement::WithStatement(const SourceLocation& location,
                             NodePtr&& object,
                             NodePtr&& body,
                             Node* parent)
    : StatementNode(NodeType::WithStatement, location, parent),
      m_object(std::move(object)),
      m_body(std::move(body)) {
  if (!m_object) {
    throw std::runtime_error("WithStatement must have an object expression.");
  }
  m_object->setParent(this);
  
  // オブジェクト式が Expression 型であることを検証
  if (!isExpression(m_object->getType())) {
    throw std::runtime_error("WithStatement object must be an Expression");
  }

  if (!m_body) {
    throw std::runtime_error("WithStatement must have a body statement.");
  }
  m_body->setParent(this);
  
  // ボディが Statement 型であることを検証
  if (!isStatement(m_body->getType())) {
    throw std::runtime_error("WithStatement body must be a Statement");
  }
}

NodePtr& WithStatement::getObject() noexcept {
  return m_object;
}

const NodePtr& WithStatement::getObject() const noexcept {
  return m_object;
}

NodePtr& WithStatement::getBody() noexcept {
  return m_body;
}

const NodePtr& WithStatement::getBody() const noexcept {
  return m_body;
}

void WithStatement::accept(AstVisitor* visitor) {
  visitor->visitWithStatement(this);
}

void WithStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitWithStatement(this);
}

std::vector<Node*> WithStatement::getChildren() {
  // オブジェクト式とボディを子ノードとして返す
  return {m_object.get(), m_body.get()};
}

std::vector<const Node*> WithStatement::getChildren() const {
  return {m_object.get(), m_body.get()};
}

nlohmann::json WithStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["object"] = m_object->toJson(pretty);
  jsonNode["body"] = m_body->toJson(pretty);
  return jsonNode;
}

std::string WithStatement::toString() const {
  std::ostringstream oss;
  oss << "WithStatement";
  oss << "(Object: " << m_object->toString() << ", Body: " << m_body->toString() << ")";
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs