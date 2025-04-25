/**
 * @file sequence_expression.cpp
 * @brief AeroJS AST のカンマ演算子式ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`sequence_expression.h` で宣言された
 * `SequenceExpression` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "sequence_expression.h"

#include <algorithm>  // std::transform
#include <cassert>    // assert のため
#include <iterator>   // std::back_inserter
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

SequenceExpression::SequenceExpression(const SourceLocation& location,
                                       std::vector<NodePtr>&& expressions,
                                       Node* parent)
    : ExpressionNode(NodeType::SequenceExpression, location, parent),
      m_expressions(std::move(expressions)) {
  if (m_expressions.size() < 2) {
    // カンマ演算子は最低2つの式が必要
    throw std::runtime_error("SequenceExpression must have at least two expressions.");
  }

  for (auto& expr : m_expressions) {
    if (expr) {
      expr->setParent(this);
      assert(isExpression(expr->getType()) && "Element in SequenceExpression must be an Expression");
    } else {
      throw std::runtime_error("SequenceExpression expressions list contains a null element.");
    }
  }
}

std::vector<NodePtr>& SequenceExpression::getExpressions() noexcept {
  return m_expressions;
}

const std::vector<NodePtr>& SequenceExpression::getExpressions() const noexcept {
  return m_expressions;
}

void SequenceExpression::accept(AstVisitor* visitor) {
  visitor->visitSequenceExpression(this);
}

void SequenceExpression::accept(ConstAstVisitor* visitor) const {
  visitor->visitSequenceExpression(this);
}

std::vector<Node*> SequenceExpression::getChildren() {
  std::vector<Node*> children;
  children.reserve(m_expressions.size());
  std::transform(m_expressions.begin(), m_expressions.end(), std::back_inserter(children),
                 [](const NodePtr& ptr) { return ptr.get(); });
  return children;
}

std::vector<const Node*> SequenceExpression::getChildren() const {
  std::vector<const Node*> children;
  children.reserve(m_expressions.size());
  std::transform(m_expressions.begin(), m_expressions.end(), std::back_inserter(children),
                 [](const NodePtr& ptr) { return ptr.get(); });
  return children;
}

nlohmann::json SequenceExpression::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["expressions"] = nlohmann::json::array();
  for (const auto& expr : m_expressions) {
    jsonNode["expressions"].push_back(expr->toJson(pretty));
  }
  return jsonNode;
}

std::string SequenceExpression::toString() const {
  std::ostringstream oss;
  oss << "SequenceExpression [" << m_expressions.size() << " exprs]";
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs