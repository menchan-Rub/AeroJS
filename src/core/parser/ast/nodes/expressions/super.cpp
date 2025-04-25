/**
 * @file super.cpp
 * @brief AeroJS AST の super キーワードノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`super.h` で宣言された `Super` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "super.h"

#include <nlohmann/json.hpp>
#include <sstream>

#include "src/core/parser/ast/utils/json_utils.h"
#include "src/core/parser/ast/utils/string_utils.h"
#include "src/core/parser/ast/visitors/ast_visitor.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

Super::Super(const SourceLocation& location, Node* parent)
    : ExpressionNode(NodeType::Super, location, parent) {
}

void Super::accept(AstVisitor* visitor) {
  visitor->visitSuper(this);
}

void Super::accept(ConstAstVisitor* visitor) const {
  visitor->visitSuper(this);
}

std::vector<Node*> Super::getChildren() {
  return {};  // Super は子を持たない
}

std::vector<const Node*> Super::getChildren() const {
  return {};  // Super は子を持たない
}

nlohmann::json Super::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  // Super は type と loc 以外のプロパティを持たない
  return jsonNode;
}

std::string Super::toString() const {
  return "Super";
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs