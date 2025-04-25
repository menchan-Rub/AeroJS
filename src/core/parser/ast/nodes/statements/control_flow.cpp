/**
 * @file control_flow.cpp
 * @brief AeroJS AST の制御フロー文ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`control_flow.h` で宣言された制御フロー文クラス
 * (`ReturnStatement`, `BreakStatement`, `ContinueStatement`, `ThrowStatement`)
 * のメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "control_flow.h"

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

// --- ReturnStatement 実装 ---

ReturnStatement::ReturnStatement(const SourceLocation& location,
                                 NodePtr&& argument,  // オプション
                                 Node* parent)
    : StatementNode(NodeType::ReturnStatement, location, parent),
      m_argument(std::move(argument)) {
  if (m_argument) {
    m_argument->setParent(this);
    // 引数が式ノードであることを検証
    if (!isExpression(m_argument->getType())) {
      throw std::invalid_argument("ReturnStatement の引数は式ノードでなければなりません");
    }
  }
}

NodePtr& ReturnStatement::getArgument() noexcept {
  return m_argument;
}

const NodePtr& ReturnStatement::getArgument() const noexcept {
  return m_argument;
}

void ReturnStatement::accept(AstVisitor* visitor) {
  visitor->visitReturnStatement(this);
}

void ReturnStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitReturnStatement(this);
}

std::vector<Node*> ReturnStatement::getChildren() {
  if (m_argument) {
    return {m_argument.get()};
  }
  return {};
}

std::vector<const Node*> ReturnStatement::getChildren() const {
  if (m_argument) {
    return {m_argument.get()};
  }
  return {};
}

nlohmann::json ReturnStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  if (m_argument) {
    jsonNode["argument"] = m_argument->toJson(pretty);
  } else {
    jsonNode["argument"] = nullptr;
  }
  return jsonNode;
}

std::string ReturnStatement::toString() const {
  std::ostringstream oss;
  oss << "ReturnStatement";
  // 詳細表示は必要に応じて拡張可能
  return oss.str();
}

// --- BreakStatement 実装 ---

BreakStatement::BreakStatement(const SourceLocation& location,
                               NodePtr&& label,  // オプション
                               Node* parent)
    : StatementNode(NodeType::BreakStatement, location, parent),
      m_label(std::move(label)) {
  if (m_label) {
    m_label->setParent(this);
    // ラベルが識別子ノードであることを検証
    if (m_label->getType() != NodeType::Identifier) {
      throw std::invalid_argument("BreakStatement のラベルは識別子ノードでなければなりません");
    }
  }
}

NodePtr& BreakStatement::getLabel() noexcept {
  return m_label;
}

const NodePtr& BreakStatement::getLabel() const noexcept {
  return m_label;
}

void BreakStatement::accept(AstVisitor* visitor) {
  visitor->visitBreakStatement(this);
}

void BreakStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitBreakStatement(this);
}

std::vector<Node*> BreakStatement::getChildren() {
  if (m_label) {
    return {m_label.get()};
  }
  return {};
}

std::vector<const Node*> BreakStatement::getChildren() const {
  if (m_label) {
    return {m_label.get()};
  }
  return {};
}

nlohmann::json BreakStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  if (m_label) {
    jsonNode["label"] = m_label->toJson(pretty);
  } else {
    jsonNode["label"] = nullptr;
  }
  return jsonNode;
}

std::string BreakStatement::toString() const {
  std::ostringstream oss;
  oss << "BreakStatement";
  // ラベル情報は詳細デバッグ時に必要になる場合がある
  return oss.str();
}

// --- ContinueStatement 実装 ---

ContinueStatement::ContinueStatement(const SourceLocation& location,
                                     NodePtr&& label,  // オプション
                                     Node* parent)
    : StatementNode(NodeType::ContinueStatement, location, parent),
      m_label(std::move(label)) {
  if (m_label) {
    m_label->setParent(this);
    // ラベルが識別子ノードであることを検証
    if (m_label->getType() != NodeType::Identifier) {
      throw std::invalid_argument("ContinueStatement のラベルは識別子ノードでなければなりません");
    }
  }
}

NodePtr& ContinueStatement::getLabel() noexcept {
  return m_label;
}

const NodePtr& ContinueStatement::getLabel() const noexcept {
  return m_label;
}

void ContinueStatement::accept(AstVisitor* visitor) {
  visitor->visitContinueStatement(this);
}

void ContinueStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitContinueStatement(this);
}

std::vector<Node*> ContinueStatement::getChildren() {
  if (m_label) {
    return {m_label.get()};
  }
  return {};
}

std::vector<const Node*> ContinueStatement::getChildren() const {
  if (m_label) {
    return {m_label.get()};
  }
  return {};
}

nlohmann::json ContinueStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  if (m_label) {
    jsonNode["label"] = m_label->toJson(pretty);
  } else {
    jsonNode["label"] = nullptr;
  }
  return jsonNode;
}

std::string ContinueStatement::toString() const {
  std::ostringstream oss;
  oss << "ContinueStatement";
  // 簡潔な表現を維持（デバッグ時に拡張可能）
  return oss.str();
}

// --- ThrowStatement 実装 ---

ThrowStatement::ThrowStatement(const SourceLocation& location,
                               NodePtr&& argument,
                               Node* parent)
    : StatementNode(NodeType::ThrowStatement, location, parent),
      m_argument(std::move(argument)) {
  if (!m_argument) {
    // throw 文は必ず引数 (スローする値) を持つ
    throw std::runtime_error("ThrowStatement には引数が必須です");
  }
  m_argument->setParent(this);
  // 引数が式ノードであることを検証
  if (!isExpression(m_argument->getType())) {
    throw std::invalid_argument("ThrowStatement の引数は式ノードでなければなりません");
  }
}

NodePtr& ThrowStatement::getArgument() noexcept {
  return m_argument;
}

const NodePtr& ThrowStatement::getArgument() const noexcept {
  return m_argument;
}

void ThrowStatement::accept(AstVisitor* visitor) {
  visitor->visitThrowStatement(this);
}

void ThrowStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitThrowStatement(this);
}

std::vector<Node*> ThrowStatement::getChildren() {
  // m_argument は必須なのでチェック不要
  return {m_argument.get()};
}

std::vector<const Node*> ThrowStatement::getChildren() const {
  return {m_argument.get()};
}

nlohmann::json ThrowStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  // 引数は必須なのでチェック不要
  jsonNode["argument"] = m_argument->toJson(pretty);
  return jsonNode;
}

std::string ThrowStatement::toString() const {
  std::ostringstream oss;
  oss << "ThrowStatement";
  // 例外オブジェクトの詳細は必要に応じて拡張可能
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs