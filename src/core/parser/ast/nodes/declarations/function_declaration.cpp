/**
 * @file function_declaration.cpp
 * @brief AeroJS AST の関数宣言ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`function_declaration.h` で宣言された
 * `FunctionDeclaration` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "function_declaration.h"

#include <algorithm>  // std::transform
#include <cassert>    // assert のため
#include <iterator>   // std::back_inserter
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>  // std::runtime_error のため

#include "src/core/parser/ast/utils/json_utils.h"      // baseJson のため
#include "src/core/parser/ast/utils/string_utils.h"    // stringify のため
#include "src/core/parser/ast/visitors/ast_visitor.h"  // visitor のため

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

FunctionDeclaration::FunctionDeclaration(const SourceLocation& location,
                                         NodePtr&& id,
                                         std::vector<NodePtr>&& params,
                                         NodePtr&& body,
                                         bool isAsync,
                                         bool isGenerator,
                                         Node* parent)
    : Node(NodeType::FunctionDeclaration, location, parent),
      m_id(std::move(id)),  // 関数名は null の場合がある (export default)
      m_params(std::move(params)),
      m_body(std::move(body)),
      m_isAsync(isAsync),
      m_isGenerator(isGenerator) {
  // 関数の ID (名前) がある場合、親を設定
  if (m_id) {
    m_id->setParent(this);
    // 関数宣言の識別子は必ず Identifier 型である必要がある
    assert(m_id->getType() == NodeType::Identifier && "FunctionDeclaration id must be an Identifier");
  }

  // パラメータリストの各要素に親を設定
  for (auto& param : m_params) {
    if (param) {
      param->setParent(this);
      // パラメータは Identifier または Pattern 派生型である必要がある
      assert((param->getType() == NodeType::Identifier || isPattern(param->getType())) &&
             "Function parameter must be an Identifier or a Pattern");
    } else {
      // パラメータリストに null は含まれないはず（パーサーの実装ミス）
      throw std::runtime_error("関数パラメータリストに無効な要素が含まれています");
    }
  }

  // 関数本体が必須であり、BlockStatement であることを確認
  if (!m_body) {
    throw std::runtime_error("関数宣言にはブロック文の本体が必要です");
  }
  
  // 関数本体は必ず BlockStatement である必要がある
  if (m_body->getType() != NodeType::BlockStatement) {
    throw std::runtime_error("関数宣言の本体は BlockStatement である必要があります");
  }
  m_body->setParent(this);

  // async と generator が同時に true になることは ECMAScript 仕様で禁止されている
  if (m_isAsync && m_isGenerator) {
    // 通常はパーサーレベルでチェックされるが、安全性のため二重チェック
    throw std::runtime_error("関数は async と generator を同時に指定できません");
  }
}

NodePtr& FunctionDeclaration::getId() noexcept {
  return m_id;
}

const NodePtr& FunctionDeclaration::getId() const noexcept {
  return m_id;
}

std::vector<NodePtr>& FunctionDeclaration::getParams() noexcept {
  return m_params;
}

const std::vector<NodePtr>& FunctionDeclaration::getParams() const noexcept {
  return m_params;
}

NodePtr& FunctionDeclaration::getBody() noexcept {
  return m_body;
}

const NodePtr& FunctionDeclaration::getBody() const noexcept {
  return m_body;
}

bool FunctionDeclaration::isAsync() const noexcept {
  return m_isAsync;
}

bool FunctionDeclaration::isGenerator() const noexcept {
  return m_isGenerator;
}

void FunctionDeclaration::accept(AstVisitor* visitor) {
  visitor->visitFunctionDeclaration(this);
}

void FunctionDeclaration::accept(ConstAstVisitor* visitor) const {
  visitor->visitFunctionDeclaration(this);
}

std::vector<Node*> FunctionDeclaration::getChildren() {
  std::vector<Node*> children;
  // ID があれば追加
  if (m_id) {
    children.push_back(m_id.get());
  }
  // パラメータを追加（効率化のためにメモリを事前確保）
  children.reserve(children.size() + m_params.size() + 1);  // 本体分も予約
  std::transform(m_params.begin(), m_params.end(), std::back_inserter(children),
                 [](const NodePtr& ptr) { return ptr.get(); });
  // 本体を追加
  children.push_back(m_body.get());
  return children;
}

std::vector<const Node*> FunctionDeclaration::getChildren() const {
  std::vector<const Node*> children;
  if (m_id) {
    children.push_back(m_id.get());
  }
  children.reserve(children.size() + m_params.size() + 1);
  std::transform(m_params.begin(), m_params.end(), std::back_inserter(children),
                 [](const NodePtr& ptr) { return ptr.get(); });
  children.push_back(m_body.get());
  return children;
}

nlohmann::json FunctionDeclaration::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["async"] = m_isAsync;
  jsonNode["generator"] = m_isGenerator;

  if (m_id) {
    jsonNode["id"] = m_id->toJson(pretty);
  } else {
    // export default function() {} のような場合
    jsonNode["id"] = nullptr;
  }

  jsonNode["params"] = nlohmann::json::array();
  for (const auto& param : m_params) {
    jsonNode["params"].push_back(param->toJson(pretty));
  }

  jsonNode["body"] = m_body->toJson(pretty);

  // ESTree 仕様に準拠
  jsonNode["expression"] = false;

  return jsonNode;
}

std::string FunctionDeclaration::toString() const {
  std::ostringstream oss;
  
  // 非同期関数の場合は "async" キーワードを追加
  if (m_isAsync) oss << "async ";
  
  oss << "function";
  
  // ジェネレータ関数の場合は "*" を追加
  if (m_isGenerator) oss << "*";
  
  // 関数名の出力
  if (m_id) {
    oss << " " << m_id->toString();
  } else {
    // 無名関数（export default などで使用される）
    oss << " [anonymous]";
  }
  
  // パラメータリストの出力
  oss << "(";
  for (size_t i = 0; i < m_params.size(); ++i) {
    if (i > 0) oss << ", ";
    oss << m_params[i]->toString();
  }
  oss << ") ";
  
  // 関数本体の出力
  oss << m_body->toString();
  
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs