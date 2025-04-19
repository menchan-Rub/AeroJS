/**
 * @file call_expression.cpp
 * @brief AeroJS AST の関数呼び出しおよびnew式ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`call_expression.h` で宣言された
 * `CallExpression` および `NewExpression` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "call_expression.h"
#include "../patterns/rest_spread.h" // For SpreadElement
#include "../node.h" // For Super
#include "../../visitors/ast_visitor.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- CallExpression ---
CallExpression::CallExpression(std::unique_ptr<Node> callee,
                               std::vector<ArgumentType> arguments,
                               bool optional,
                               const SourceLocation& location,
                               Node* parent)
    : ExpressionNode(NodeType::CallExpression, location, parent),
      m_callee(std::move(callee)),
      m_arguments(std::move(arguments)),
      m_optional(optional) {
    if (!m_callee) throw std::runtime_error("CallExpression callee cannot be null");
    m_callee->setParent(this);
    for (auto& arg : m_arguments) {
        if (!arg) throw std::runtime_error("CallExpression argument cannot be null");
        arg->setParent(this);
    }
}

const Node& CallExpression::getCallee() const { return *m_callee; }
Node& CallExpression::getCallee() { return *m_callee; }
const std::vector<CallExpression::ArgumentType>& CallExpression::getArguments() const noexcept { return m_arguments; }
bool CallExpression::isOptional() const noexcept { return m_optional; }

void CallExpression::accept(AstVisitor& visitor) { visitor.Visit(*this); }
void CallExpression::accept(ConstAstVisitor& visitor) const { visitor.Visit(*this); }

std::vector<Node*> CallExpression::getChildren() {
    std::vector<Node*> children;
    children.push_back(m_callee.get());
    children.reserve(children.size() + m_arguments.size());
    for (const auto& arg : m_arguments) {
        children.push_back(arg.get());
    }
    return children;
}

std::vector<const Node*> CallExpression::getChildren() const {
     std::vector<const Node*> children;
    children.push_back(m_callee.get());
    children.reserve(children.size() + m_arguments.size());
    for (const auto& arg : m_arguments) {
        children.push_back(arg.get());
    }
    return children;
}

nlohmann::json CallExpression::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["callee"] = m_callee->toJson(pretty);
    jsonNode["arguments"] = nlohmann::json::array();
    for(const auto& arg : m_arguments) {
        jsonNode["arguments"].push_back(arg->toJson(pretty));
    }
    jsonNode["optional"] = m_optional;
    return jsonNode;
}
std::string CallExpression::toString() const { return "CallExpression"; }


// --- NewExpression ---
NewExpression::NewExpression(std::unique_ptr<ExpressionNode> callee,
                             std::vector<ArgumentType> arguments,
                             const SourceLocation& location,
                             Node* parent)
    : ExpressionNode(NodeType::NewExpression, location, parent),
      m_callee(std::move(callee)),
      m_arguments(std::move(arguments)) {
    if (!m_callee) throw std::runtime_error("NewExpression callee cannot be null");
    m_callee->setParent(this);
    for (auto& arg : m_arguments) {
         if (!arg) throw std::runtime_error("NewExpression argument cannot be null");
        arg->setParent(this);
    }
}

const ExpressionNode& NewExpression::getCallee() const { return *m_callee; }
ExpressionNode& NewExpression::getCallee() { return *m_callee; }
const std::vector<NewExpression::ArgumentType>& NewExpression::getArguments() const noexcept { return m_arguments; }

void NewExpression::accept(AstVisitor& visitor) { visitor.Visit(*this); }
void NewExpression::accept(ConstAstVisitor& visitor) const { visitor.Visit(*this); }

std::vector<Node*> NewExpression::getChildren() {
     std::vector<Node*> children;
    children.push_back(m_callee.get());
    children.reserve(children.size() + m_arguments.size());
    for (const auto& arg : m_arguments) {
        children.push_back(arg.get());
    }
    return children;
}
std::vector<const Node*> NewExpression::getChildren() const {
    std::vector<const Node*> children;
    children.push_back(m_callee.get());
    children.reserve(children.size() + m_arguments.size());
    for (const auto& arg : m_arguments) {
        children.push_back(arg.get());
    }
    return children;
}

nlohmann::json NewExpression::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["callee"] = m_callee->toJson(pretty);
    jsonNode["arguments"] = nlohmann::json::array();
    for(const auto& arg : m_arguments) {
        jsonNode["arguments"].push_back(arg->toJson(pretty));
    }
    return jsonNode;
}
std::string NewExpression::toString() const { return "NewExpression"; }


} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 