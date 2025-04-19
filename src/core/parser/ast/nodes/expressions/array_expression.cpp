/**
 * @file array_expression.cpp
 * @brief AST ArrayExpression ノードの実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 */

#include "array_expression.h"
#include "spread_element.h" // Include SpreadElement if used
#include "../../visitors/ast_visitor.h"
#include <nlohmann/json.hpp>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

ArrayExpression::ArrayExpression(std::vector<ElementType> elements,
                                 const SourceLocation& location,
                                 Node* parent)
    : ExpressionNode(NodeType::ArrayExpression, location, parent),
      m_elements(std::move(elements)) {
    for (auto& element : m_elements) {
        if (element) {
            element->setParent(this);
        }
    }
}

const std::vector<ArrayExpression::ElementType>& ArrayExpression::getElements() const noexcept {
    return m_elements;
}

void ArrayExpression::accept(AstVisitor& visitor) {
    visitor.Visit(*this);
}

void ArrayExpression::accept(ConstAstVisitor& visitor) const {
    visitor.Visit(*this);
}

std::vector<Node*> ArrayExpression::getChildren() {
    std::vector<Node*> children;
    children.reserve(m_elements.size());
    for (const auto& element : m_elements) {
        if (element) { // nullptr (Elision) は含めない
            children.push_back(element.get());
        }
    }
    return children;
}

std::vector<const Node*> ArrayExpression::getChildren() const {
    std::vector<const Node*> children;
    children.reserve(m_elements.size());
    for (const auto& element : m_elements) {
        if (element) { // nullptr (Elision) は含めない
            children.push_back(element.get());
        }
    }
    return children;
}

nlohmann::json ArrayExpression::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["elements"] = nlohmann::json::array();
    for(const auto& element : m_elements) {
        if (element) {
            jsonNode["elements"].push_back(element->toJson(pretty));
        } else {
            // Elision を null で表現 (ESTree 仕様)
            jsonNode["elements"].push_back(nullptr);
        }
    }
    return jsonNode;
}

std::string ArrayExpression::toString() const {
    std::string elementsStr = "[";
    bool first = true;
    for (const auto& elem : m_elements) {
        if (!first) elementsStr += ", ";
        elementsStr += (elem ? elem->toString() : "null");
        first = false;
    }
    elementsStr += "]";
    return "ArrayExpression<elements: " + elementsStr + ">";
}

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 