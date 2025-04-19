/**
 * @file object_expression.cpp
 * @brief AST ObjectExpression ノードの実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 */

#include "object_expression.h"
#include "property.h"       // Include specific node types used
#include "spread_element.h" // Include SpreadElement if used
#include "../../visitors/ast_visitor.h"
#include <nlohmann/json.hpp>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

ObjectExpression::ObjectExpression(std::vector<PropertyType> properties,
                                   const SourceLocation& location,
                                   Node* parent)
    : ExpressionNode(NodeType::ObjectExpression, location, parent),
      m_properties(std::move(properties)) {
    for (auto& prop : m_properties) {
        if (prop) { // Should generally not be null, unlike array elements
            prop->setParent(this);
        }
    }
}

const std::vector<ObjectExpression::PropertyType>& ObjectExpression::getProperties() const noexcept {
    return m_properties;
}

void ObjectExpression::accept(AstVisitor& visitor) {
    visitor.Visit(*this);
}

void ObjectExpression::accept(ConstAstVisitor& visitor) const {
    visitor.Visit(*this);
}

std::vector<Node*> ObjectExpression::getChildren() {
    std::vector<Node*> children;
    children.reserve(m_properties.size());
    for (const auto& prop : m_properties) {
        if (prop) {
            children.push_back(prop.get());
        }
    }
    return children;
}

std::vector<const Node*> ObjectExpression::getChildren() const {
    std::vector<const Node*> children;
    children.reserve(m_properties.size());
    for (const auto& prop : m_properties) {
        if (prop) {
            children.push_back(prop.get());
        }
    }
    return children;
}

nlohmann::json ObjectExpression::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["properties"] = nlohmann::json::array();
    for(const auto& prop : m_properties) {
        if (prop) {
             jsonNode["properties"].push_back(prop->toJson(pretty));
        }
        // else: エラーまたは予期しない状態？ オブジェクトプロパティは通常nullではない
    }
    return jsonNode;
}

std::string ObjectExpression::toString() const {
    std::string propsStr = "{";
    bool first = true;
    for (const auto& prop : m_properties) {
        if (!first) propsStr += ", ";
        propsStr += (prop ? prop->toString() : "null");
        first = false;
    }
    propsStr += "}";
    return "ObjectExpression<properties: " + propsStr + ">";
}


} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 