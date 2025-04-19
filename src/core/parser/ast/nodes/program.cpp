/**
 * @file program.cpp
 * @brief AeroJS AST のプログラムノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`program.h` で宣言された `Program` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "program.h"
#include <nlohmann/json.hpp> // For JSON implementation
#include <sstream> // For toString implementation

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

Program::Program(const SourceLocation& location,
                 std::vector<NodePtr>&& body,
                 ProgramType type,
                 Node* parent)
    : Node(NodeType::Program, location, parent),
      m_body(std::move(body)),
      m_programType(type)
{
    // 子ノードの親ポインタを設定
    for (auto& child : m_body) {
        if (child) {
            child->setParent(this);
        }
    }
}

ProgramType Program::getProgramType() const noexcept {
    return m_programType;
}

std::vector<NodePtr>& Program::getBody() noexcept {
    return m_body;
}

const std::vector<NodePtr>& Program::getBody() const noexcept {
    return m_body;
}

void Program::accept(AstVisitor* visitor) {
    visitor->visitProgram(this);
}

void Program::accept(ConstAstVisitor* visitor) const {
    visitor->visitProgram(this);
}

std::vector<Node*> Program::getChildren() {
    std::vector<Node*> children;
    children.reserve(m_body.size());
    for (const auto& node_ptr : m_body) {
        children.push_back(node_ptr.get());
    }
    return children;
}

std::vector<const Node*> Program::getChildren() const {
    std::vector<const Node*> children;
    children.reserve(m_body.size());
    for (const auto& node_ptr : m_body) {
        children.push_back(node_ptr.get());
    }
    return children;
}

nlohmann::json Program::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["sourceType"] = (m_programType == ProgramType::Module) ? "module" : "script";
    jsonNode["body"] = nlohmann::json::array();
    for (const auto& stmt : m_body) {
        if (stmt) {
            jsonNode["body"].push_back(stmt->toJson(pretty));
        } else {
            jsonNode["body"].push_back(nullptr); // Handle potential nullptrs if needed
        }
    }
    return jsonNode;
}

std::string Program::toString() const {
    std::ostringstream oss;
    oss << "Program[type="
        << ((m_programType == ProgramType::Module) ? "Module" : "Script")
        << ", bodySize=" << m_body.size() << "]";
    return oss.str();
}

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 