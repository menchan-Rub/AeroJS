/**
 * @file this_expression.cpp
 * @brief AST ThisExpression ノードの実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 */

#include "this_expression.h"
#include "../../visitors/ast_visitor.h"
#include <nlohmann/json.hpp>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

ThisExpression::ThisExpression(const SourceLocation& location, Node* parent)
    : ExpressionNode(NodeType::ThisExpression, location, parent) {}

void ThisExpression::accept(AstVisitor& visitor) {
    visitor.Visit(*this);
}

void ThisExpression::accept(ConstAstVisitor& visitor) const {
    visitor.Visit(*this);
}

std::vector<Node*> ThisExpression::getChildren() {
    return {}; // ThisExpression は子ノードを持たない
}

std::vector<const Node*> ThisExpression::getChildren() const {
    return {}; // ThisExpression は子ノードを持たない
}

nlohmann::json ThisExpression::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode); // 基底クラスの情報を追加
    return jsonNode;    // ThisExpression 固有のプロパティはない
}

std::string ThisExpression::toString() const {
    return "ThisExpression";
}

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 