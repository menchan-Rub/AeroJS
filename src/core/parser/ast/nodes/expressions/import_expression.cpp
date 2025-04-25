/**
 * @file import_expression.cpp
 * @brief AeroJS AST の動的インポート式ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`import_expression.h` で宣言された `ImportExpression`
 * クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "import_expression.h"

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

ImportExpression::ImportExpression(const SourceLocation& location,
                                   NodePtr&& source,
                                   NodePtr&& options,  // 将来用
                                   Node* parent)
    : ExpressionNode(NodeType::ImportExpression, location, parent),
      m_source(std::move(source)),
      m_options(std::move(options)) {
  if (!m_source) {
    throw std::runtime_error("ImportExpression must have a source argument.");
  }
  m_source->setParent(this);
  assert(isExpression(m_source->getType()) && "ImportExpression source must be an Expression");

  // 現時点では options は未サポート
  if (m_options) {
    // 将来的に Options が ObjectExpression などであることを確認
    m_options->setParent(this);
    // assert(m_options->getType() == NodeType::ObjectExpression && "ImportExpression options must be an ObjectExpression (currently unsupported)");
    throw std::runtime_error("ImportExpression options (import attributes) are not yet supported.");
  }
}

NodePtr& ImportExpression::getSource() noexcept {
  return m_source;
}

const NodePtr& ImportExpression::getSource() const noexcept {
  return m_source;
}

NodePtr& ImportExpression::getOptions() noexcept {
  return m_options;
}

const NodePtr& ImportExpression::getOptions() const noexcept {
  return m_options;
}

void ImportExpression::accept(AstVisitor* visitor) {
  visitor->visitImportExpression(this);
}

void ImportExpression::accept(ConstAstVisitor* visitor) const {
  visitor->visitImportExpression(this);
}

std::vector<Node*> ImportExpression::getChildren() {
  std::vector<Node*> children;
  children.push_back(m_source.get());  // source は必須
  if (m_options) {
    children.push_back(m_options.get());
  }
  return children;
}

std::vector<const Node*> ImportExpression::getChildren() const {
  std::vector<const Node*> children;
  children.push_back(m_source.get());
  if (m_options) {
    children.push_back(m_options.get());
  }
  return children;
}

nlohmann::json ImportExpression::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["source"] = m_source->toJson(pretty);  // 必須
  // options は ESTree 仕様では optional であり、まだ広くサポートされていない
  if (m_options) {
    jsonNode["options"] = m_options->toJson(pretty);
  }
  // ESTree 仕様では Phase プロパティを持つことがある (現在は 'execution' のみ)
  // jsonNode["phase"] = "execution";
  return jsonNode;
}

std::string ImportExpression::toString() const {
  std::ostringstream oss;
  oss << "ImportExpression";
  // oss << "(Source: " << m_source->toString() << ")";
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs