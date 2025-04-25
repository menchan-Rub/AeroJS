/**
 * @file expression_statement.cpp
 * @brief AeroJS AST の式文 (Expression Statement) ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`expression_statement.h` で宣言された
 * `ExpressionStatement` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "expression_statement.h"

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

ExpressionStatement::ExpressionStatement(const SourceLocation& location,
                                         NodePtr&& expression,
                                         Node* parent)
    : StatementNode(NodeType::ExpressionStatement, location, parent),
      m_expression(std::move(expression)) {
  if (!m_expression) {
    throw std::runtime_error("ExpressionStatement must have an expression.");
  }
  m_expression->setParent(this);

  // 型安全性の検証は開発モードでのみ行う
  #ifdef AEROJS_DEBUG
  if (dynamic_cast<ExpressionNode*>(m_expression.get()) == nullptr) {
    throw std::runtime_error("Child of ExpressionStatement must be an Expression.");
  }
  #endif
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
  return {m_expression.get()};
}

std::vector<const Node*> ExpressionStatement::getChildren() const {
  return {m_expression.get()};
}

nlohmann::json ExpressionStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["expression"] = m_expression->toJson(pretty);
  return jsonNode;
}

std::string ExpressionStatement::toString() const {
  std::ostringstream oss;
  oss << "ExpressionStatement";
  if (m_expression) {
    oss << "(" << m_expression->toString() << ");";
  }
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs