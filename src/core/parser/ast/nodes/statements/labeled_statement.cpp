/**
 * @file labeled_statement.cpp
 * @brief AeroJS AST のラベル付き文ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`labeled_statement.h` で宣言された `LabeledStatement`
 * クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "labeled_statement.h"

#include <cassert>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

#include "src/core/parser/ast/utils/json_utils.h"
#include "src/core/parser/ast/utils/string_utils.h"
#include "src/core/parser/ast/visitors/ast_visitor.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

LabeledStatement::LabeledStatement(const SourceLocation& location,
                                   NodePtr&& label,
                                   NodePtr&& body,
                                   Node* parent)
    : StatementNode(NodeType::LabeledStatement, location, parent),
      m_label(std::move(label)),
      m_body(std::move(body)) {
  if (!m_label) {
    throw std::runtime_error("LabeledStatement must have a label.");
  }
  m_label->setParent(this);
  
  if (m_label->getType() != NodeType::Identifier) {
    throw std::runtime_error("LabeledStatement label must be an Identifier.");
  }

  if (!m_body) {
    throw std::runtime_error("LabeledStatement must have a body statement.");
  }
  m_body->setParent(this);
  
  if (!isStatement(m_body->getType())) {
    throw std::runtime_error("LabeledStatement body must be a Statement.");
  }
}

NodePtr& LabeledStatement::getLabel() noexcept {
  return m_label;
}

const NodePtr& LabeledStatement::getLabel() const noexcept {
  return m_label;
}

NodePtr& LabeledStatement::getBody() noexcept {
  return m_body;
}

const NodePtr& LabeledStatement::getBody() const noexcept {
  return m_body;
}

void LabeledStatement::accept(AstVisitor* visitor) {
  visitor->visitLabeledStatement(this);
}

void LabeledStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitLabeledStatement(this);
}

std::vector<Node*> LabeledStatement::getChildren() {
  return {m_label.get(), m_body.get()};
}

std::vector<const Node*> LabeledStatement::getChildren() const {
  return {m_label.get(), m_body.get()};
}

nlohmann::json LabeledStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["label"] = m_label->toJson(pretty);
  jsonNode["body"] = m_body->toJson(pretty);
  return jsonNode;
}

std::string LabeledStatement::toString() const {
  std::ostringstream oss;
  oss << "LabeledStatement(" << m_label->toString() << ")";
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs