/**
 * @file switch_statement.cpp
 * @brief AeroJS AST の switch 文関連ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`switch_statement.h` で宣言された `SwitchStatement`
 * および `SwitchCase` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "switch_statement.h"

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

// --- SwitchCase 実装 ---

SwitchCase::SwitchCase(const SourceLocation& location,
                       NodePtr&& test,  // default 節の場合は null
                       std::vector<NodePtr>&& consequent,
                       Node* parent)
    : Node(NodeType::SwitchCase, location, parent),
      m_test(std::move(test)),
      m_consequent(std::move(consequent)) {
  if (m_test) {
    m_test->setParent(this);
    // テスト式が Expression 型であることを検証
    if (!isExpression(m_test->getType())) {
      throw std::invalid_argument("SwitchCase のテスト式は Expression 型である必要があります");
    }
  }
  // default 節の場合、m_test は null

  // consequent (文リスト) の各要素に親を設定
  for (auto& stmt : m_consequent) {
    if (stmt) {
      stmt->setParent(this);
      // 各文が Statement 型であることを検証
      if (!isStatement(stmt->getType())) {
        throw std::invalid_argument("SwitchCase の consequent 要素は Statement 型である必要があります");
      }
    } else {
      // case 節の文リストに null は通常含まれないはず
      throw std::runtime_error("SwitchCase consequent リストに null 要素が含まれています");
    }
  }
  // consequent リストが空の場合もある (例: case 1: case 2: ...)
}

NodePtr& SwitchCase::getTest() noexcept {
  return m_test;
}

const NodePtr& SwitchCase::getTest() const noexcept {
  return m_test;
}

std::vector<NodePtr>& SwitchCase::getConsequent() noexcept {
  return m_consequent;
}

const std::vector<NodePtr>& SwitchCase::getConsequent() const noexcept {
  return m_consequent;
}

void SwitchCase::accept(AstVisitor* visitor) {
  visitor->visitSwitchCase(this);
}

void SwitchCase::accept(ConstAstVisitor* visitor) const {
  visitor->visitSwitchCase(this);
}

std::vector<Node*> SwitchCase::getChildren() {
  std::vector<Node*> children;
  if (m_test) {
    children.push_back(m_test.get());
  }
  children.reserve(children.size() + m_consequent.size());
  std::transform(m_consequent.begin(), m_consequent.end(), std::back_inserter(children),
                 [](const NodePtr& ptr) { return ptr.get(); });
  return children;
}

std::vector<const Node*> SwitchCase::getChildren() const {
  std::vector<const Node*> children;
  if (m_test) {
    children.push_back(m_test.get());
  }
  children.reserve(children.size() + m_consequent.size());
  std::transform(m_consequent.begin(), m_consequent.end(), std::back_inserter(children),
                 [](const NodePtr& ptr) { return ptr.get(); });
  return children;
}

nlohmann::json SwitchCase::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["test"] = m_test ? m_test->toJson(pretty) : nullptr;
  jsonNode["consequent"] = nlohmann::json::array();
  for (const auto& stmt : m_consequent) {
    jsonNode["consequent"].push_back(stmt->toJson(pretty));
  }
  return jsonNode;
}

std::string SwitchCase::toString() const {
  std::ostringstream oss;
  if (m_test) {
    oss << "Case";
    oss << " (" << m_test->toString() << ")";
  } else {
    oss << "Default";
  }
  oss << " [" << m_consequent.size() << " 文]";
  return oss.str();
}

// --- SwitchStatement 実装 ---

SwitchStatement::SwitchStatement(const SourceLocation& location,
                                 NodePtr&& discriminant,
                                 std::vector<NodePtr>&& cases,
                                 Node* parent)
    : StatementNode(NodeType::SwitchStatement, location, parent),
      m_discriminant(std::move(discriminant)),
      m_cases(std::move(cases)) {
  if (!m_discriminant) {
    throw std::runtime_error("SwitchStatement には判別式が必要です");
  }
  m_discriminant->setParent(this);
  // 判別式が Expression 型であることを検証
  if (!isExpression(m_discriminant->getType())) {
    throw std::invalid_argument("SwitchStatement の判別式は Expression 型である必要があります");
  }

  for (auto& caseNode : m_cases) {
    if (caseNode) {
      caseNode->setParent(this);
      // caseNode が SwitchCase 型であることを検証
      if (caseNode->getType() != NodeType::SwitchCase) {
        throw std::invalid_argument("SwitchStatement の cases 要素は SwitchCase 型である必要があります");
      }
    } else {
      throw std::runtime_error("SwitchStatement cases リストに null 要素が含まれています");
    }
  }
}

NodePtr& SwitchStatement::getDiscriminant() noexcept {
  return m_discriminant;
}

const NodePtr& SwitchStatement::getDiscriminant() const noexcept {
  return m_discriminant;
}

std::vector<NodePtr>& SwitchStatement::getCases() noexcept {
  return m_cases;
}

const std::vector<NodePtr>& SwitchStatement::getCases() const noexcept {
  return m_cases;
}

void SwitchStatement::accept(AstVisitor* visitor) {
  visitor->visitSwitchStatement(this);
}

void SwitchStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitSwitchStatement(this);
}

std::vector<Node*> SwitchStatement::getChildren() {
  std::vector<Node*> children;
  children.push_back(m_discriminant.get());  // discriminant は必須
  children.reserve(children.size() + m_cases.size());
  std::transform(m_cases.begin(), m_cases.end(), std::back_inserter(children),
                 [](const NodePtr& ptr) { return ptr.get(); });
  return children;
}

std::vector<const Node*> SwitchStatement::getChildren() const {
  std::vector<const Node*> children;
  children.push_back(m_discriminant.get());
  children.reserve(children.size() + m_cases.size());
  std::transform(m_cases.begin(), m_cases.end(), std::back_inserter(children),
                 [](const NodePtr& ptr) { return ptr.get(); });
  return children;
}

nlohmann::json SwitchStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["discriminant"] = m_discriminant->toJson(pretty);  // 必須
  jsonNode["cases"] = nlohmann::json::array();
  for (const auto& caseNode : m_cases) {
    jsonNode["cases"].push_back(caseNode->toJson(pretty));
  }
  return jsonNode;
}

std::string SwitchStatement::toString() const {
  std::ostringstream oss;
  oss << "SwitchStatement";
  oss << "(判別式: " << m_discriminant->toString() << ", Case数: " << m_cases.size() << ")";
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs