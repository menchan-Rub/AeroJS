/**
 * @file identifier.cpp
 * @brief AST Identifier ノードの実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`identifier.h` で宣言された `Identifier` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "identifier.h"

#include <nlohmann/json.hpp>

#include "../../visitors/ast_visitor.h"  // Visitor の前方宣言のためではなく、実装のために必要

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

Identifier::Identifier(const std::string& name, const SourceLocation& location, Node* parent)
    : ExpressionNode(NodeType::Identifier, location, parent), m_name(name) {
  // 名前の検証 (空でないかなど) はパーサーで行う想定
}

const std::string& Identifier::getName() const noexcept {
  return m_name;
}

void Identifier::accept(AstVisitor& visitor) {
  visitor.Visit(*this);
}

void Identifier::accept(ConstAstVisitor& visitor) const {
  visitor.Visit(*this);
}

std::vector<Node*> Identifier::getChildren() {
  return {};  // Identifier は子ノードを持たない
}

std::vector<const Node*> Identifier::getChildren() const {
  return {};  // Identifier は子ノードを持たない
}

nlohmann::json Identifier::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);  // 基底クラスの情報を追加
  jsonNode["name"] = m_name;
  return jsonNode;
}

std::string Identifier::toString() const {
  return "Identifier<" + m_name + ">";
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs