/**
 * @file literal.cpp
 * @brief AST Literal ノードの実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 */

#include "literal.h"
#include "../../visitors/ast_visitor.h"
#include <nlohmann/json.hpp>
#include <stdexcept> // For std::visit exception handling
#include <sstream>   // For toString

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

Literal::Literal(LiteralValue value, std::string rawValue, LiteralType type, const SourceLocation& location, Node* parent)
    : ExpressionNode(NodeType::Literal, location, parent),
      m_value(std::move(value)),
      m_rawValue(std::move(rawValue)),
      m_literalType(type) {}

const LiteralValue& Literal::getValue() const noexcept {
    return m_value;
}

LiteralType Literal::getLiteralType() const noexcept {
    return m_literalType;
}

const std::string& Literal::getRawValue() const noexcept {
    return m_rawValue;
}

void Literal::accept(AstVisitor& visitor) {
    visitor.Visit(*this);
}

void Literal::accept(ConstAstVisitor& visitor) const {
    visitor.Visit(*this);
}

std::vector<Node*> Literal::getChildren() {
    return {}; // Literal は子ノードを持たない
}

std::vector<const Node*> Literal::getChildren() const {
    return {}; // Literal は子ノードを持たない
}

nlohmann::json Literal::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["raw"] = m_rawValue;

    // Add 'value' field based on the type
    std::visit([&jsonNode](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            jsonNode["value"] = nullptr;
        } else if constexpr (std::is_same_v<T, bool>) {
            jsonNode["value"] = arg;
        } else if constexpr (std::is_same_v<T, double>) {
            jsonNode["value"] = arg;
        } else if constexpr (std::is_same_v<T, std::string>) {
            // Could be string or BigInt representation
            jsonNode["value"] = arg;
        } else if constexpr (std::is_same_v<T, RegExpLiteral>) {
            // ESTree 'value' for regex is often null or the RegExp object itself.
            // Let's represent it as an object for clarity.
            jsonNode["value"] = nullptr; // Or provide object? Check ESTree closely.
            jsonNode["regex"] = {
                {"pattern", arg.pattern},
                {"flags", arg.flags}
            };
        } else {
            // Should not happen with the defined variant
            jsonNode["value"] = "[Unknown Literal Type]";
        }
    }, m_value);

    // Add bigint field if it's a BigInt literal
    if (m_literalType == LiteralType::BigInt) {
         if (const auto* pval = std::get_if<std::string>(&m_value)) {
             jsonNode["bigint"] = *pval;
             // ESTree often puts the string representation in 'value' too for bigint
             jsonNode["value"] = *pval;
         }
    }


    return jsonNode;
}

std::string Literal::toString() const {
     std::stringstream ss;
     ss << "Literal<type: ";
     switch(m_literalType) {
         case LiteralType::Null: ss << "Null"; break;
         case LiteralType::Boolean: ss << "Boolean"; break;
         case LiteralType::Number: ss << "Number"; break;
         case LiteralType::String: ss << "String"; break;
         case LiteralType::RegExp: ss << "RegExp"; break;
         case LiteralType::BigInt: ss << "BigInt"; break;
     }
     ss << ", raw: " << m_rawValue << ">";
     return ss.str();
}

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 