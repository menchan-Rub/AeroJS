/**
 * @file property.cpp
 * @brief AST Property ノードの実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 */

#include "property.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

#include "../../visitors/ast_visitor.h"
#include "identifier.h"  // Include specific node types used
#include "literal.h"
#include "private_identifier.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

Property::Property(std::unique_ptr<Node> key,
                   std::unique_ptr<ExpressionNode> value,
                   PropertyKind kind,
                   bool computed,
                   bool method,
                   bool shorthand,
                   const SourceLocation& location,
                   Node* parent)
    : Node(NodeType::Property, location, parent),  // NodeType は Property
      m_key(std::move(key)),
      m_value(std::move(value)),
      m_kind(kind),
      m_computed(computed),
      m_method(method),
      m_shorthand(shorthand) {
  if (m_key) m_key->setParent(this);
  if (m_value) m_value->setParent(this);
}

const Node& Property::getKey() const {
  if (!m_key) throw std::runtime_error("Property key is null");
  return *m_key;
}

Node& Property::getKey() {
  if (!m_key) throw std::runtime_error("Property key is null");
  return *m_key;
}

const ExpressionNode& Property::getValue() const {
  if (!m_value) throw std::runtime_error("Property value is null");
  return *m_value;
}

ExpressionNode& Property::getValue() {
  if (!m_value) throw std::runtime_error("Property value is null");
  return *m_value;
}

PropertyKind Property::getKind() const noexcept {
  return m_kind;
}

bool Property::isComputed() const noexcept {
  return m_computed;
}

bool Property::isMethod() const noexcept {
  return m_method;
}

bool Property::isShorthand() const noexcept {
  return m_shorthand;
}

void Property::accept(AstVisitor& visitor) {
  visitor.Visit(*this);
}

void Property::accept(ConstAstVisitor& visitor) const {
  visitor.Visit(*this);
}

std::vector<Node*> Property::getChildren() {
  std::vector<Node*> children;
  if (m_key) children.push_back(m_key.get());
  if (m_value) children.push_back(m_value.get());
  return children;
}

std::vector<const Node*> Property::getChildren() const {
  std::vector<const Node*> children;
  if (m_key) children.push_back(m_key.get());
  if (m_value) children.push_back(m_value.get());
  return children;
}

nlohmann::json Property::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["key"] = m_key ? m_key->toJson(pretty) : nullptr;
  jsonNode["value"] = m_value ? m_value->toJson(pretty) : nullptr;
  jsonNode["computed"] = m_computed;
  jsonNode["method"] = m_method;
  jsonNode["shorthand"] = m_shorthand;

  switch (m_kind) {
    case PropertyKind::Init:
      jsonNode["kind"] = "init";
      break;
    case PropertyKind::Get:
      jsonNode["kind"] = "get";
      break;
    case PropertyKind::Set:
      jsonNode["kind"] = "set";
      break;
  }
  return jsonNode;
}

std::string Property::toString() const {
  std::string keyStr = m_key ? m_key->toString() : "null";
  std::string valStr = m_value ? m_value->toString() : "null";
  std::string kindStr;
  switch (m_kind) {
    case PropertyKind::Init:
      kindStr = "init";
      break;
    case PropertyKind::Get:
      kindStr = "get";
      break;
    case PropertyKind::Set:
      kindStr = "set";
      break;
  }
  return "Property<kind:" + kindStr +
         ", computed:" + (m_computed ? "true" : "false") +
         ", method:" + (m_method ? "true" : "false") +
         ", shorthand:" + (m_shorthand ? "true" : "false") +
         ", key:" + keyStr + ", value:" + valStr + ">";
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs