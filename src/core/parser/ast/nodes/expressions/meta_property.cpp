// === src/core/parser/ast/nodes/expressions/meta_property.cpp ===
#include "meta_property.h"
#include "identifier.h"
#include "../../visitors/ast_visitor.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

MetaProperty::MetaProperty(std::unique_ptr<Identifier> meta,
                           std::unique_ptr<Identifier> property,
                           const SourceLocation& location,
                           Node* parent)
    : ExpressionNode(NodeType::MetaProperty, location, parent),
      m_meta(std::move(meta)),
      m_property(std::move(property)) {
    if (!m_meta) throw std::runtime_error("MetaProperty meta identifier cannot be null");
    if (!m_property) throw std::runtime_error("MetaProperty property identifier cannot be null");
    m_meta->setParent(this);
    m_property->setParent(this);
}

const Identifier& MetaProperty::getMeta() const { return *m_meta; }
Identifier& MetaProperty::getMeta() { return *m_meta; }
const Identifier& MetaProperty::getProperty() const { return *m_property; }
Identifier& MetaProperty::getProperty() { return *m_property; }

void MetaProperty::accept(AstVisitor& visitor) { visitor.Visit(*this); }
void MetaProperty::accept(ConstAstVisitor& visitor) const { visitor.Visit(*this); }

std::vector<Node*> MetaProperty::getChildren() { return {m_meta.get(), m_property.get()}; }
std::vector<const Node*> MetaProperty::getChildren() const { return {m_meta.get(), m_property.get()}; }

nlohmann::json MetaProperty::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["meta"] = m_meta->toJson(pretty);
    jsonNode["property"] = m_property->toJson(pretty);
    return jsonNode;
}
std::string MetaProperty::toString() const { return "MetaProperty"; }

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 