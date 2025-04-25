/**
 * @file debugger_statement.cpp
 * @brief AeroJS AST の debugger 文ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`debugger_statement.h` で宣言された `DebuggerStatement`
 * クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "debugger_statement.h"

#include <nlohmann/json.hpp>
#include <sstream>

#include "src/core/parser/ast/utils/json_utils.h"
#include "src/core/parser/ast/utils/string_utils.h"
#include "src/core/parser/ast/visitors/ast_visitor.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

DebuggerStatement::DebuggerStatement(const SourceLocation& location,
                                     Node* parent)
    : StatementNode(NodeType::DebuggerStatement, location, parent) {
}

void DebuggerStatement::accept(AstVisitor* visitor) {
  visitor->visitDebuggerStatement(this);
}

void DebuggerStatement::accept(ConstAstVisitor* visitor) const {
  visitor->visitDebuggerStatement(this);
}

std::vector<Node*> DebuggerStatement::getChildren() {
  return {};  // DebuggerStatement は子を持たない
}

std::vector<const Node*> DebuggerStatement::getChildren() const {
  return {};  // DebuggerStatement は子を持たない
}

nlohmann::json DebuggerStatement::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  // DebuggerStatement は type と loc 以外のプロパティを持たない
  return jsonNode;
}

std::string DebuggerStatement::toString() const {
  return "DebuggerStatement";
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs