/**
 * @file private_identifier.cpp
 * @brief AST Private Identifier ノードの実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 */

#include "private_identifier.h"
#include "../../visitors/ast_visitor.h"
#include <nlohmann/json.hpp>
#include <cassert>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

PrivateIdentifier::PrivateIdentifier(const std::string& name, const SourceLocation& location, Node* parent)
    : ExpressionNode(NodeType::PrivateIdentifier, location, parent), m_name(name) {
    // プライベート識別子は '#' で始まる必要がある
    assert(!m_name.empty() && m_name[0] == '#' && "Private identifier must start with '#'");
}

const std::string& PrivateIdentifier::getName() const noexcept {
    return m_name;
}

void PrivateIdentifier::accept(AstVisitor& visitor) {
    visitor.Visit(*this);
}

void PrivateIdentifier::accept(ConstAstVisitor& visitor) const {
    visitor.Visit(*this);
}

std::vector<Node*> PrivateIdentifier::getChildren() {
    return {}; // PrivateIdentifier は子ノードを持たない
}

std::vector<const Node*> PrivateIdentifier::getChildren() const {
    return {}; // PrivateIdentifier は子ノードを持たない
}

nlohmann::json PrivateIdentifier::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    // ESTree 仕様では、name フィールドには '#' を含まない名前を入れることが多い
    // ここでは内部的に '#' を含む名前を保持しているため、JSON 出力時に '#' を除外する
    jsonNode["name"] = m_name.substr(1);
    return jsonNode;
}

std::string PrivateIdentifier::toString() const {
    return "PrivateIdentifier<" + m_name + ">";
}

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 