/**
 * @file variable_declaration.cpp
 * @brief AeroJS AST の変数宣言関連ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`variable_declaration.h` で宣言された
 * `VariableDeclarator` および `VariableDeclaration` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "variable_declaration.h"

#include <algorithm>  // std::transform
#include <cassert>    // assert のため
#include <iterator>   // std::back_inserter
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>  // std::out_of_range のため

#include "src/core/parser/ast/utils/json_utils.h"      // baseJson のため
#include "src/core/parser/ast/utils/string_utils.h"    // stringify のため
#include "src/core/parser/ast/visitors/ast_visitor.h"  // visitor のため

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- VariableDeclarator 実装 ---

VariableDeclarator::VariableDeclarator(const SourceLocation& location,
                                       NodePtr&& id,
                                       NodePtr&& init,
                                       Node* parent)
    : Node(NodeType::VariableDeclarator, location, parent),
      m_id(std::move(id)),
      m_init(std::move(init))  // 初期化子は null の場合あり
{
  if (!m_id) {
    // 変数宣言子は必ず ID (Identifier または Pattern) を持つ必要がある
    throw std::runtime_error("VariableDeclarator must have an identifier or pattern.");
  }
  m_id->setParent(this);

  if (m_init) {
    m_init->setParent(this);
  }

  // id が Identifier または Pattern であることを確認
  if (!(m_id->getType() == NodeType::Identifier || isPattern(m_id->getType()))) {
    throw std::invalid_argument("VariableDeclarator id must be an Identifier or a Pattern");
  }
  
  // init が Expression であることを確認（存在する場合）
  if (m_init && !isExpression(m_init->getType())) {
    throw std::invalid_argument("VariableDeclarator init must be an Expression");
  }
}

NodePtr& VariableDeclarator::getId() noexcept {
  return m_id;
}

const NodePtr& VariableDeclarator::getId() const noexcept {
  return m_id;
}

NodePtr& VariableDeclarator::getInit() noexcept {
  return m_init;
}

const NodePtr& VariableDeclarator::getInit() const noexcept {
  return m_init;
}

void VariableDeclarator::accept(AstVisitor* visitor) {
  visitor->visitVariableDeclarator(this);
}

void VariableDeclarator::accept(ConstAstVisitor* visitor) const {
  visitor->visitVariableDeclarator(this);
}

std::vector<Node*> VariableDeclarator::getChildren() {
  std::vector<Node*> children;
  children.push_back(m_id.get());
  if (m_init) {
    children.push_back(m_init.get());
  }
  return children;
}

std::vector<const Node*> VariableDeclarator::getChildren() const {
  std::vector<const Node*> children;
  children.push_back(m_id.get());
  if (m_init) {
    children.push_back(m_init.get());
  }
  return children;
}

nlohmann::json VariableDeclarator::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["id"] = m_id->toJson(pretty);
  if (m_init) {
    jsonNode["init"] = m_init->toJson(pretty);
  } else {
    // JSON では null を明示的に示す
    jsonNode["init"] = nullptr;
  }
  return jsonNode;
}

std::string VariableDeclarator::toString() const {
  std::ostringstream oss;
  oss << "VariableDeclarator(";
  oss << "id: " << m_id->toString();
  if (m_init) {
    oss << ", init: " << m_init->toString();
  } else {
    oss << ", init: null";
  }
  oss << ")";
  return oss.str();
}

// --- VariableDeclaration 実装 ---

const char* variableDeclarationKindToString(VariableDeclarationKind kind) {
  switch (kind) {
    case VariableDeclarationKind::Var:
      return "var";
    case VariableDeclarationKind::Let:
      return "let";
    case VariableDeclarationKind::Const:
      return "const";
    default:
      // 無効な値が渡された場合のエラーハンドリング
      throw std::out_of_range("Invalid VariableDeclarationKind value");
  }
}

VariableDeclaration::VariableDeclaration(const SourceLocation& location,
                                         std::vector<NodePtr>&& declarations,
                                         VariableDeclarationKind kind,
                                         Node* parent)
    : Node(NodeType::VariableDeclaration, location, parent),
      m_declarations(std::move(declarations)),
      m_kind(kind) {
  if (m_declarations.empty()) {
    // 変数宣言は少なくとも1つの宣言子を持つ必要がある
    throw std::runtime_error("VariableDeclaration must have at least one declarator.");
  }
  for (auto& decl : m_declarations) {
    if (!decl) {
      // リスト内に null ポインタがあってはならない
      throw std::runtime_error("VariableDeclaration contains a null declarator.");
    }
    // 各宣言子の親を設定
    decl->setParent(this);
    
    // 各宣言子が VariableDeclarator であることを確認
    if (decl->getType() != NodeType::VariableDeclarator) {
      throw std::invalid_argument("Child of VariableDeclaration must be a VariableDeclarator");
    }
  }
}

std::vector<NodePtr>& VariableDeclaration::getDeclarations() noexcept {
  return m_declarations;
}

const std::vector<NodePtr>& VariableDeclaration::getDeclarations() const noexcept {
  return m_declarations;
}

VariableDeclarationKind VariableDeclaration::getKind() const noexcept {
  return m_kind;
}

void VariableDeclaration::accept(AstVisitor* visitor) {
  visitor->visitVariableDeclaration(this);
}

void VariableDeclaration::accept(ConstAstVisitor* visitor) const {
  visitor->visitVariableDeclaration(this);
}

std::vector<Node*> VariableDeclaration::getChildren() {
  std::vector<Node*> children;
  children.reserve(m_declarations.size());  // 事前に容量を確保
  // std::transform を使って Node* に変換して追加
  std::transform(m_declarations.begin(), m_declarations.end(), std::back_inserter(children),
                 [](const NodePtr& ptr) { return ptr.get(); });
  return children;
}

std::vector<const Node*> VariableDeclaration::getChildren() const {
  std::vector<const Node*> children;
  children.reserve(m_declarations.size());
  std::transform(m_declarations.begin(), m_declarations.end(), std::back_inserter(children),
                 [](const NodePtr& ptr) { return ptr.get(); });
  return children;
}

nlohmann::json VariableDeclaration::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["kind"] = variableDeclarationKindToString(m_kind);
  jsonNode["declarations"] = nlohmann::json::array();
  for (const auto& decl : m_declarations) {
    jsonNode["declarations"].push_back(decl->toJson(pretty));
  }
  return jsonNode;
}

std::string VariableDeclaration::toString() const {
  std::ostringstream oss;
  oss << "VariableDeclaration(" << variableDeclarationKindToString(m_kind) << ", [";
  
  // 宣言子のリストを構築
  bool first = true;
  for (const auto& decl : m_declarations) {
    if (!first) {
      oss << ", ";
    }
    oss << decl->toString();
    first = false;
  }
  
  oss << "])";
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs