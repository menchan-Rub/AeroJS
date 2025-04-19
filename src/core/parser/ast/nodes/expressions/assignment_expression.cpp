/**
 * @file assignment_expression.cpp
 * @brief AeroJS AST の代入式ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`assignment_expression.h` で宣言された
 * `AssignmentExpression` クラスおよび関連ヘルパー関数の実装を提供します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "assignment_expression.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include "src/core/parser/ast/visitors/ast_visitor.h"
#include "src/core/parser/ast/utils/json_factory.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- AssignmentOperator Utility ---

std::string AssignmentOperatorToString(AssignmentOperator op) {
    switch (op) {
        case AssignmentOperator::Assign:
            return "=";
        case AssignmentOperator::AdditionAssign:
            return "+=";
        case AssignmentOperator::SubtractionAssign:
            return "-=";
        case AssignmentOperator::MultiplicationAssign:
            return "*=";
        case AssignmentOperator::DivisionAssign:
            return "/=";
        case AssignmentOperator::RemainderAssign:
            return "%";
        case AssignmentOperator::ExponentiationAssign:
            return "**=";
        case AssignmentOperator::LeftShiftAssign:
            return "<<=";
        case AssignmentOperator::RightShiftAssign:
            return ">>=";
        case AssignmentOperator::UnsignedRightShiftAssign:
            return ">>>=";
        case AssignmentOperator::BitwiseAndAssign:
            return "&=";
        case AssignmentOperator::BitwiseOrAssign:
            return "|=";
        case AssignmentOperator::BitwiseXorAssign:
            return "^=";
        case AssignmentOperator::LogicalAndAssign:
            return "&&=";
        case AssignmentOperator::LogicalOrAssign:
            return "||=";
        case AssignmentOperator::NullishCoalescingAssign:
            return "??=";
    }
    // Should not be reachable if all enum values are handled.
    throw std::out_of_range("Invalid AssignmentOperator value");
}

// --- AssignmentExpression Implementation ---

AssignmentExpression::AssignmentExpression(const SourceLocation& location,
                                         AssignmentOperator op,
                                         NodePtr&& left,
                                         NodePtr&& right,
                                         Node* parent)
    : Node(NodeType::AssignmentExpression, location, parent),
      m_operator(op),
      m_left(std::move(left)),
      m_right(std::move(right))
{
    if (m_left) {
        // Add validation: Check if left is a valid LVal (Identifier, MemberExpression, Pattern)
        // NodeType leftType = m_left->getType();
        // assert(leftType == NodeType::Identifier || leftType == NodeType::MemberExpression || /* Add Pattern types */ );
        m_left->setParent(this);
    } else {
        // throw std::runtime_error("AssignmentExpression must have a left operand");
    }
    if (m_right) {
        m_right->setParent(this);
    } else {
        // throw std::runtime_error("AssignmentExpression must have a right operand");
    }
}

AssignmentOperator AssignmentExpression::getOperator() const noexcept {
    return m_operator;
}

NodePtr& AssignmentExpression::getLeft() noexcept {
    return m_left;
}

const NodePtr& AssignmentExpression::getLeft() const noexcept {
    return m_left;
}

NodePtr& AssignmentExpression::getRight() noexcept {
    return m_right;
}

const NodePtr& AssignmentExpression::getRight() const noexcept {
    return m_right;
}

void AssignmentExpression::accept(AstVisitor* visitor) {
    visitor->visitAssignmentExpression(this);
}

void AssignmentExpression::accept(ConstAstVisitor* visitor) const {
    visitor->visitAssignmentExpression(this);
}

std::vector<Node*> AssignmentExpression::getChildren() {
    std::vector<Node*> children;
    if (m_left) children.push_back(m_left.get());
    if (m_right) children.push_back(m_right.get());
    return children;
}

std::vector<const Node*> AssignmentExpression::getChildren() const {
    std::vector<const Node*> children;
    if (m_left) children.push_back(m_left.get());
    if (m_right) children.push_back(m_right.get());
    return children;
}

nlohmann::json AssignmentExpression::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    jsonNode["operator"] = AssignmentOperatorToString(m_operator);
    if (m_left) {
        jsonNode["left"] = m_left->toJson(pretty);
    } else {
        jsonNode["left"] = nullptr;
    }
    if (m_right) {
        jsonNode["right"] = m_right->toJson(pretty);
    } else {
        jsonNode["right"] = nullptr;
    }
    return jsonNode;
}

std::string AssignmentExpression::toString() const {
    std::ostringstream oss;
    oss << "AssignmentExpression<" << AssignmentOperatorToString(m_operator) << ">";
    return oss.str();
}

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 