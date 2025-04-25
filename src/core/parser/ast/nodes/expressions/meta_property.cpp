/**
 * @file meta_property.cpp
 * @brief AeroJS AST のメタプロパティ式ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`meta_property.h` で宣言された `MetaProperty` クラスの
 * メソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "meta_property.h"

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

MetaProperty::MetaProperty(const SourceLocation& location,
                           NodePtr&& meta,
                           NodePtr&& property,
                           Node* parent)
    : ExpressionNode(NodeType::MetaProperty, location, parent),
      m_meta(std::move(meta)),
      m_property(std::move(property)) {
  if (!m_meta) {
    throw std::runtime_error("MetaProperty must have a meta identifier.");
  }
  m_meta->setParent(this);
  assert(m_meta->getType() == NodeType::Identifier && "MetaProperty meta must be an Identifier (new or import)");
  // さらに 'new' または 'import' であるかチェックすることも可能
  // auto metaName = dynamic_cast<Identifier*>(m_meta.get())->getName();
  // assert((metaName == "new" || metaName == "import") && ...);

  if (!m_property) {
    throw std::runtime_error("MetaProperty must have a property identifier.");
  }
  m_property->setParent(this);
  assert(m_property->getType() == NodeType::Identifier && "MetaProperty property must be an Identifier (target or meta)");
  // さらに 'target' または 'meta' であるかチェックすることも可能
  // auto propName = dynamic_cast<Identifier*>(m_property.get())->getName();
  // assert((propName == "target" || propName == "meta") && ...);

  // new.target と import.meta の組み合わせバリデーション (パーサーで行うべき)
  // if (metaName == "new" && propName != "target") { ... }
  // if (metaName == "import" && propName != "meta") { ... }
}

NodePtr& MetaProperty::getMeta() noexcept {
  return m_meta;
}

const NodePtr& MetaProperty::getMeta() const noexcept {
  return m_meta;
}

NodePtr& MetaProperty::getProperty() noexcept {
  return m_property;
}

const NodePtr& MetaProperty::getProperty() const noexcept {
  return m_property;
}

void MetaProperty::accept(AstVisitor* visitor) {
  visitor->visitMetaProperty(this);
}

void MetaProperty::accept(ConstAstVisitor* visitor) const {
  visitor->visitMetaProperty(this);
}

std::vector<Node*> MetaProperty::getChildren() {
  // meta と property は Identifier であり、通常これ以上の子を持たない
  // ここでは ESTree 仕様に従い、子ノードとはしないことが多いが、
  // 構造として含めるならこうなる
  return {m_meta.get(), m_property.get()};
}

std::vector<const Node*> MetaProperty::getChildren() const {
  return {m_meta.get(), m_property.get()};
}

nlohmann::json MetaProperty::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["meta"] = m_meta->toJson(pretty);          // 必須
  jsonNode["property"] = m_property->toJson(pretty);  // 必須
  return jsonNode;
}

std::string MetaProperty::toString() const {
  std::ostringstream oss;
  oss << "MetaProperty";
  // oss << "(" << m_meta->toString() << "." << m_property->toString() << ")";
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs