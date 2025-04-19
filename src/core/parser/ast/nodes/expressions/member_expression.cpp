/**
 * @file member_expression.cpp
 * @brief AeroJS AST のメンバーアクセス式ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`member_expression.h` で宣言された
 * `MemberExpression` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "member_expression.h"
#include "../node.h" // For Super
#include "identifier.h"
#include "private_identifier.h"
#include "../../visitors/ast_visitor.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

MemberExpression::MemberExpression(std::unique_ptr<Node> object,
                                   std::unique_ptr<Node> property,
                                   bool computed,
                                   bool optional,
                                   const SourceLocation& location,
                                   Node* parent)
    : ExpressionNode(NodeType::MemberExpression, location, parent),
      m_object(std::move(object)),
      m_property(std::move(property)),
      m_computed(computed),
      m_optional(optional) {
    if (!m_object) throw std::runtime_error("MemberExpression object cannot be null");
    if (!m_property) throw std::runtime_error("MemberExpression property cannot be null");
    m_object->setParent(this);
    m_property->setParent(this);
}

const Node& MemberExpression::getObject() const { return *m_object; }
Node& MemberExpression::getObject() { return *m_object; }
const Node& MemberExpression::getProperty() const { return *m_property; }
Node& MemberExpression::getProperty() { return *m_property; }
bool MemberExpression::isComputed() const noexcept { return m_computed; }
bool MemberExpression::isOptional() const noexcept { return m_optional; }

void MemberExpression::accept(AstVisitor& visitor) { visitor.Visit(*this); }
void MemberExpression::accept(ConstAstVisitor& visitor) const { visitor.Visit(*this); }

std::vector<Node*> MemberExpression::getChildren() {
    return {m_object.get(), m_property.get()};
}
std::vector<const Node*> MemberExpression::getChildren() const {
    return {m_object.get(), m_property.get()};
}

nlohmann::json MemberExpression::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["object"] = m_object->toJson(pretty);
    jsonNode["property"] = m_property->toJson(pretty);
    jsonNode["computed"] = m_computed;
    jsonNode["optional"] = m_optional;
    return jsonNode;
}
std::string MemberExpression::toString() const { return "MemberExpression"; }

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 