/**
 * @file variable_declaration.cpp
 * @brief AeroJS AST の変数宣言関連ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`variable_declaration.h` で宣言された
 * `VariableDeclarator` および `VariableDeclaration` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "variable_declaration.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept> // For std::out_of_range

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- VariableDeclarator Implementation ---

VariableDeclarator::VariableDeclarator(const SourceLocation& location,
                                     NodePtr&& id,
                                     NodePtr&& init, // Optional, can be nullptr
                                     Node* parent)
    : Node(NodeType::VariableDeclarator, location, parent),
      m_id(std::move(id)),
      m_init(std::move(init))
{
    if (m_id) {
        m_id->setParent(this);
    } else {
        // ID is mandatory for a declarator
        // Throw an exception or handle error appropriately based on project policy
        // For now, let's assume parser ensures id is valid.
        // throw std::runtime_error("VariableDeclarator must have an ID");
    }
    if (m_init) {
        m_init->setParent(this);
    }
}

NodePtr& VariableDeclarator::getId() noexcept {
    return m_id;
}

const NodePtr& VariableDeclarator::getId() const noexcept {
    return m_id;
}

NodePtr& VariableDeclarator::getInit() noexcept {
    return m_init;
}

const NodePtr& VariableDeclarator::getInit() const noexcept {
    return m_init;
}

void VariableDeclarator::accept(AstVisitor* visitor) {
    visitor->visitVariableDeclarator(this);
}

void VariableDeclarator::accept(ConstAstVisitor* visitor) const {
    visitor->visitVariableDeclarator(this);
}

std::vector<Node*> VariableDeclarator::getChildren() {
    std::vector<Node*> children;
    if (m_id) children.push_back(m_id.get());
    if (m_init) children.push_back(m_init.get());
    return children;
}

std::vector<const Node*> VariableDeclarator::getChildren() const {
    std::vector<const Node*> children;
    if (m_id) children.push_back(m_id.get());
    if (m_init) children.push_back(m_init.get());
    return children;
}

nlohmann::json VariableDeclarator::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    if (m_id) {
        jsonNode["id"] = m_id->toJson(pretty);
    } else {
        jsonNode["id"] = nullptr; // Should ideally not happen
    }
    if (m_init) {
        jsonNode["init"] = m_init->toJson(pretty);
    } else {
        jsonNode["init"] = nullptr;
    }
    return jsonNode;
}

std::string VariableDeclarator::toString() const {
    std::string str = "VariableDeclarator";
    // Could add id/init info for better debugging
    return str;
}

// --- VariableDeclarationKind Utility ---

const char* variableDeclarationKindToString(VariableDeclarationKind kind) {
    switch (kind) {
        case VariableDeclarationKind::Var: return "var";
        case VariableDeclarationKind::Let: return "let";
        case VariableDeclarationKind::Const: return "const";
        default: throw std::out_of_range("Invalid VariableDeclarationKind");
    }
}

// --- VariableDeclaration Implementation ---

VariableDeclaration::VariableDeclaration(const SourceLocation& location,
                                     std::vector<NodePtr>&& declarations,
                                     VariableDeclarationKind kind,
                                     Node* parent)
    : Node(NodeType::VariableDeclaration, location, parent),
      m_declarations(std::move(declarations)),
      m_kind(kind)
{
    if (m_declarations.empty()) {
        // Warning or error? A declaration usually needs at least one declarator.
        // Let's assume parser ensures this.
    }
    for (auto& decl : m_declarations) {
        if (decl) {
            // Ensure the child is actually a VariableDeclarator? Type safety check.
            // assert(decl->getType() == NodeType::VariableDeclarator);
            decl->setParent(this);
        } else {
            // Handle potential nullptrs in the vector if the parser allows them
        }
    }
}

std::vector<NodePtr>& VariableDeclaration::getDeclarations() noexcept {
    return m_declarations;
}

const std::vector<NodePtr>& VariableDeclaration::getDeclarations() const noexcept {
    return m_declarations;
}

VariableDeclarationKind VariableDeclaration::getKind() const noexcept {
    return m_kind;
}

void VariableDeclaration::accept(AstVisitor* visitor) {
    visitor->visitVariableDeclaration(this);
}

void VariableDeclaration::accept(ConstAstVisitor* visitor) const {
    visitor->visitVariableDeclaration(this);
}

std::vector<Node*> VariableDeclaration::getChildren() {
    std::vector<Node*> children;
    children.reserve(m_declarations.size());
    for (const auto& node_ptr : m_declarations) {
        if (node_ptr) {
             children.push_back(node_ptr.get());
        }
    }
    return children;
}

std::vector<const Node*> VariableDeclaration::getChildren() const {
    std::vector<const Node*> children;
    children.reserve(m_declarations.size());
    for (const auto& node_ptr : m_declarations) {
         if (node_ptr) {
             children.push_back(node_ptr.get());
         }
    }
    return children;
}

nlohmann::json VariableDeclaration::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["kind"] = variableDeclarationKindToString(m_kind);
    jsonNode["declarations"] = nlohmann::json::array();
    for (const auto& decl : m_declarations) {
        if (decl) {
            jsonNode["declarations"].push_back(decl->toJson(pretty));
        } else {
             jsonNode["declarations"].push_back(nullptr);
        }
    }
    // ESTree often includes 'declare' for TypeScript ambient contexts
    // jsonNode["declare"] = false; // Add if supporting TS
    return jsonNode;
}

std::string VariableDeclaration::toString() const {
    std::ostringstream oss;
    oss << "VariableDeclaration<" << variableDeclarationKindToString(m_kind)
        << ", count=" << m_declarations.size() << ">";
    return oss.str();
}


} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 