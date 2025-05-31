/**
 * @file function_expression.cpp
 * @brief AeroJS AST の関数式ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`function_expression.h` で宣言された
 * `FunctionExpression` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "function_expression.h"

#include <algorithm>  // std::transform
#include <cassert>    // assert のため
#include <iterator>   // std::back_inserter
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

#include "../../visitors/ast_visitor.h"
#include "../patterns/patterns.h"      // パラメータパターン用
#include "../statements/statements.h"  // BlockStatement用
#include "identifier.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- FunctionExpression ---
FunctionExpression::FunctionExpression(const SourceLocation& location,
                                       NodePtr&& id,  // オプション
                                       std::vector<NodePtr>&& params,
                                       NodePtr&& body,
                                       bool isAsync,
                                       bool isGenerator,
                                       Node* parent)
    : ExpressionNode(NodeType::FunctionExpression, location, parent),
      m_id(std::move(id)),
      m_params(std::move(params)),
      m_body(std::move(body)),
      m_isAsync(isAsync),
      m_isGenerator(isGenerator) {
  // 関数の ID (名前) がある場合、親を設定し型チェック
  if (m_id) {
    m_id->setParent(this);
    assert(m_id->getType() == NodeType::Identifier && "FunctionExpression id must be an Identifier");
  }

  // パラメータリストの各要素に親を設定し型チェック
  for (auto& param : m_params) {
    if (param) {
      param->setParent(this);
      assert((param->getType() == NodeType::Identifier || isPattern(param->getType())) &&
             "Function parameter must be an Identifier or a Pattern");
    } else {
      // パラメータリストに null は通常含まれないはず
      throw std::runtime_error("Function parameter list contains a null element.");
    }
  }

  // 関数本体が必須であり、BlockStatement であることを確認
  if (!m_body) {
    throw std::runtime_error("FunctionExpression must have a body.");
  }
  assert(m_body->getType() == NodeType::BlockStatement && "FunctionExpression body must be a BlockStatement");
  m_body->setParent(this);

  // async と generator が同時に true になることは構文的にありえない (パーサーでチェックされるはず)
  if (m_isAsync && m_isGenerator) {
    // 本来はパーサーレベルのエラーだが、念のためチェック
    throw std::runtime_error("Function cannot be both async and generator.");
  }
}

NodePtr& FunctionExpression::getId() noexcept {
  return m_id;
}

const NodePtr& FunctionExpression::getId() const noexcept {
  return m_id;
}

std::vector<NodePtr>& FunctionExpression::getParams() noexcept {
  return m_params;
}

const std::vector<NodePtr>& FunctionExpression::getParams() const noexcept {
  return m_params;
}

NodePtr& FunctionExpression::getBody() noexcept {
  return m_body;
}

const NodePtr& FunctionExpression::getBody() const noexcept {
  return m_body;
}

bool FunctionExpression::isAsync() const noexcept {
  return m_isAsync;
}

bool FunctionExpression::isGenerator() const noexcept {
  return m_isGenerator;
}

void FunctionExpression::accept(AstVisitor* visitor) {
  visitor->visitFunctionExpression(this);
}

void FunctionExpression::accept(ConstAstVisitor* visitor) const {
  visitor->visitFunctionExpression(this);
}

std::vector<Node*> FunctionExpression::getChildren() {
  std::vector<Node*> children;
  // ID があれば追加
  if (m_id) {
    children.push_back(m_id.get());
  }
  // パラメータを追加
  children.reserve(children.size() + m_params.size() + 1);  // 本体分も予約
  std::transform(m_params.begin(), m_params.end(), std::back_inserter(children),
                 [](const NodePtr& ptr) { return ptr.get(); });
  // 本体を追加
  children.push_back(m_body.get());
  return children;
}

std::vector<const Node*> FunctionExpression::getChildren() const {
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

nlohmann::json FunctionExpression::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["async"] = m_isAsync;
  jsonNode["generator"] = m_isGenerator;
  // FunctionExpression は式なので expression: true を追加
  jsonNode["expression"] = true;

  if (m_id) {
    jsonNode["id"] = m_id->toJson(pretty);
  } else {
    // 匿名関数式の場合
    jsonNode["id"] = nullptr;
  }

  jsonNode["params"] = nlohmann::json::array();
  for (const auto& param : m_params) {
    jsonNode["params"].push_back(param->toJson(pretty));
  }

  jsonNode["body"] = m_body->toJson(pretty);  // 本体は必須

  return jsonNode;
}

std::string FunctionExpression::toString() const {
  std::ostringstream oss;
  
  // 関数の種類に応じたプレフィックスを追加
  if (m_isAsync) oss << "async ";
  oss << "function";
  if (m_isGenerator) oss << "*";
  
  // 関数名の追加
  if (m_id) {
    auto* identifier = static_cast<Identifier*>(m_id.get());
    oss << " " << identifier->getName();
  }
  
  // パラメータリストの構築
  oss << "(";
  for (size_t i = 0; i < m_params.size(); ++i) {
    if (i > 0) oss << ", ";
    oss << m_params[i]->toString();
  }
  oss << ")";
  
  // 関数本体の追加
  if (m_body) {
    oss << " " << m_body->toString();
  } else {
    oss << " { /* body not available */ }"; // フォールバック
  }
  
  return oss.str();
}

// --- ArrowFunctionExpression ---
ArrowFunctionExpression::ArrowFunctionExpression(FunctionData data, const SourceLocation& location, Node* parent)
    : ExpressionNode(NodeType::ArrowFunctionExpression, location, parent), m_data(std::move(data)) {
  if (m_data.id) throw std::runtime_error("ArrowFunctionExpression cannot have an id");
  if (m_data.generator) throw std::runtime_error("ArrowFunctionExpression cannot be a generator");
  if (!m_data.body) throw std::runtime_error("ArrowFunctionExpression body cannot be null");

  for (auto& param : m_data.params) {
    if (!param) throw std::runtime_error("Arrow function parameter cannot be null");
    param->setParent(this);
  }
  m_data.body->setParent(this);
  // 本体の型に基づいて式フラグを決定
  m_data.expression = (m_data.body->getType() != NodeType::BlockStatement);
}

const std::vector<std::unique_ptr<PatternNode>>& ArrowFunctionExpression::getParams() const {
  return m_data.params;
}
const Node& ArrowFunctionExpression::getBody() const {
  return *m_data.body;
}
Node& ArrowFunctionExpression::getBody() {
  return *m_data.body;
}
bool ArrowFunctionExpression::isGenerator() const {
  return false;
}  // 常にfalse
bool ArrowFunctionExpression::isAsync() const {
  return m_data.async;
}
bool ArrowFunctionExpression::isExpression() const {
  return m_data.expression;
}

void ArrowFunctionExpression::accept(AstVisitor& visitor) {
  visitor.Visit(*this);
}
void ArrowFunctionExpression::accept(ConstAstVisitor& visitor) const {
  visitor.Visit(*this);
}

std::vector<Node*> ArrowFunctionExpression::getChildren() {
  std::vector<Node*> children;
  children.reserve(m_data.params.size() + 1);
  for (const auto& p : m_data.params) children.push_back(p.get());
  children.push_back(m_data.body.get());
  return children;
}
std::vector<const Node*> ArrowFunctionExpression::getChildren() const {
  std::vector<const Node*> children;
  children.reserve(m_data.params.size() + 1);
  for (const auto& p : m_data.params) children.push_back(p.get());
  children.push_back(m_data.body.get());
  return children;
}

nlohmann::json ArrowFunctionExpression::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["id"] = nullptr;  // アロー関数にはIDがない
  jsonNode["params"] = nlohmann::json::array();
  for (const auto& p : m_data.params) jsonNode["params"].push_back(p->toJson(pretty));
  jsonNode["body"] = m_data.body->toJson(pretty);
  jsonNode["generator"] = false;
  jsonNode["async"] = m_data.async;
  jsonNode["expression"] = m_data.expression;
  return jsonNode;
}
std::string ArrowFunctionExpression::toString() const {
  std::ostringstream oss;
  
  // async修飾子の追加
  if (m_data.async) oss << "async ";
  
  // パラメータリストの構築
  if (m_data.params.size() == 1) {
    // 単一パラメータの場合は括弧を省略可能な場合がある
    oss << m_data.params[0]->toString();
  } else {
    oss << "(";
    for (size_t i = 0; i < m_data.params.size(); ++i) {
      if (i > 0) oss << ", ";
      oss << m_data.params[i]->toString();
    }
    oss << ")";
  }
  
  // アロー演算子
  oss << " => ";
  
  // 本体の追加
  if (m_data.expression) {
    // 式本体
    oss << m_data.body->toString();
  } else {
    // ブロック本体 - 実際のブロック内容を出力
    oss << m_data.body->toString();
  }
  
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs