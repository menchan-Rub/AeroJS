/**
 * @file try_statement.cpp
 * @brief AeroJS AST の try...catch...finally 文関連ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`try_statement.h` で宣言された `TryStatement`
 * および `CatchClause` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "try_statement.h"

#include <cassert>  // assert のため
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

// --- CatchClause 実装 ---

CatchClause::CatchClause(const SourceLocation& location,
                         NodePtr&& param,  // null の場合あり
                         NodePtr&& body,
                         Node* parent)
    : Node(NodeType::CatchClause, location, parent),
      m_param(std::move(param)),
      m_body(std::move(body)) {
  if (m_param) {
    m_param->setParent(this);
    // パラメータの型チェック（Identifier または Pattern のみ許可）
    if (m_param->getType() != NodeType::Identifier && !isPattern(m_param->getType())) {
      throw std::runtime_error("CatchClause パラメータは Identifier または Pattern である必要があります");
    }
  }
  // パラメータは省略可能（ECMAScript 2019以降の catch {} 構文）

  if (!m_body) {
    throw std::runtime_error("CatchClause にはブロック本体が必要です");
  }
  m_body->setParent(this);
  
  // ボディの型チェック（BlockStatement のみ許可）
  if (m_body->getType() != NodeType::BlockStatement) {
    throw std::runtime_error("CatchClause のボディは BlockStatement である必要があります");
  }
}

NodePtr& CatchClause::getParam() noexcept {
  return m_param;
}

const NodePtr& CatchClause::getParam() const noexcept {
  return m_param;
}

NodePtr& CatchClause::getBody() noexcept {
  return m_body;
}

const NodePtr& CatchClause::getBody() const noexcept {
  return m_body;
}

void CatchClause::accept(AstVisitor* visitor) {
  visitor->visitCatchClause(this);
}

void CatchClause::accept(ConstAstVisitor* visitor) const {
  visitor->visitCatchClause(this);
}

std::vector<Node*> CatchClause::getChildren() {
  std::vector<Node*> children;
  if (m_param) {
    children.push_back(m_param.get());
  }
  children.push_back(m_body.get());  // body は必須
  return children;
}

std::vector<const Node*> CatchClause::getChildren() const {
  std::vector<const Node*> children;
  if (m_param) {
    children.push_back(m_param.get());
  }
  children.push_back(m_body.get());
  return children;
}

nlohmann::json CatchClause::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["param"] = m_param ? m_param->toJson(pretty) : nullptr;
  jsonNode["body"] = m_body->toJson(pretty);  // body は必須
  return jsonNode;
}

std::string CatchClause::toString() const {
  std::ostringstream oss;
  oss << "CatchClause";
  if (m_param) {
    oss << "(パラメータ: " << m_param->toString() << ")";
  } else {
    oss << "(パラメータなし)";
  }
  oss << " { ... }";
  return oss.str();
}

// --- TryStatement 実装 ---

TryStatement::TryStatement(const SourceLocation& location,
                           NodePtr&& block,
                           NodePtr&& handler,    // オプション
                           NodePtr&& finalizer,  // オプション
                           Node* parent)
    : StatementNode(NodeType::TryStatement, location, parent),
      m_block(std::move(block)),
      m_handler(std::move(handler)),
      m_finalizer(std::move(finalizer)) {
  if (!m_block) {
    throw std::runtime_error("TryStatement には try ブロックが必要です");
  }
  m_block->setParent(this);
  
  // ブロックの型チェック（BlockStatement のみ許可）
  if (m_block->getType() != NodeType::BlockStatement) {
    throw std::runtime_error("TryStatement のブロックは BlockStatement である必要があります");
  }

  if (!m_handler && !m_finalizer) {
    // catch も finally もない try は構文エラー
    throw std::runtime_error("TryStatement には少なくとも catch ハンドラまたは finally ブロックが必要です");
  }

  if (m_handler) {
    m_handler->setParent(this);
    // ハンドラの型チェック（CatchClause のみ許可）
    if (m_handler->getType() != NodeType::CatchClause) {
      throw std::runtime_error("TryStatement のハンドラは CatchClause である必要があります");
    }
  }

  if (m_finalizer) {
    m_finalizer->setParent(this);
    // ファイナライザの型チェック（BlockStatement のみ許可）
    if (m_finalizer->getType() != NodeType::BlockStatement) {
      throw std::runtime_error("TryStatement のファイナライザは BlockStatement である必要があります");
    }
  }
}

NodePtr& TryStatement::getBlock() noexcept {
  return m_block;
}

const NodePtr& TryStatement::getBlock() const noexcept {
  return m_block;
}

NodePtr& TryStatement::getHandler() noexcept {
  return m_handler;
}

const NodePtr& TryStatement::getHandler() const noexcept {
  return m_handler;
}

NodePtr& TryStatement::getFinalizer() noexcept {
  return m_finalizer;
}

const NodePtr& TryStatement::getFinalizer() const noexcept {
  return m_finalizer;
}

void TryStatement::accept(AstVisitor* visitor) {
  visitor->visitTryStatement(this);
}

void TryStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitTryStatement(this);
}

std::vector<Node*> TryStatement::getChildren() {
  std::vector<Node*> children;
  children.push_back(m_block.get());  // block は必須
  if (m_handler) {
    children.push_back(m_handler.get());
  }
  if (m_finalizer) {
    children.push_back(m_finalizer.get());
  }
  return children;
}

std::vector<const Node*> TryStatement::getChildren() const {
  std::vector<const Node*> children;
  children.push_back(m_block.get());
  if (m_handler) {
    children.push_back(m_handler.get());
  }
  if (m_finalizer) {
    children.push_back(m_finalizer.get());
  }
  return children;
}

nlohmann::json TryStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["block"] = m_block->toJson(pretty);  // block は必須
  jsonNode["handler"] = m_handler ? m_handler->toJson(pretty) : nullptr;
  jsonNode["finalizer"] = m_finalizer ? m_finalizer->toJson(pretty) : nullptr;
  return jsonNode;
}

std::string TryStatement::toString() const {
  std::ostringstream oss;
  oss << "TryStatement";
  oss << " { try ブロック }";
  if (m_handler) {
    oss << " catch " << m_handler->toString();
  }
  if (m_finalizer) {
    oss << " finally { ... }";
  }
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs