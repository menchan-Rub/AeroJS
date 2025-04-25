/**
 * @file node.cpp
 * @brief AeroJS 抽象構文木 (AST) の基底ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * このファイルは、`node.h` で宣言された `Node` クラスのメソッドと、
 * `NodeType` 列挙型を文字列に変換するユーティリティ関数を実装します。
 * AST の全てのノードはこの `Node` クラスを直接的または間接的に継承します。
 */

#include "node.h"

#include <cassert>
#include <format>
#include <iterator>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "node_type_utils.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// NodeType を文字列に変換するためのルックアップテーブル
static const char* const NODE_TYPE_STRINGS[] = {
    "Uninitialized",
    "Program", "BlockStatement", "EmptyStatement",
    "FunctionDeclaration", "VariableDeclaration", "VariableDeclarator", "ClassDeclaration", "ClassBody", "MethodDefinition", "ImportDeclaration", "ImportSpecifier", "ImportDefaultSpecifier", "ImportNamespaceSpecifier", "ExportNamedDeclaration", "ExportDefaultDeclaration", "ExportAllDeclaration", "ExportSpecifier",
    "ExpressionStatement", "IfStatement", "SwitchStatement", "SwitchCase", "ReturnStatement", "ThrowStatement", "TryStatement", "CatchClause", "WhileStatement", "DoWhileStatement", "ForStatement", "ForInStatement", "ForOfStatement", "BreakStatement", "ContinueStatement", "LabeledStatement", "WithStatement", "DebuggerStatement",
    "Identifier", "PrivateIdentifier", "Literal", "ThisExpression", "ArrayExpression", "ObjectExpression", "Property", "FunctionExpression", "ArrowFunctionExpression", "UnaryExpression", "UpdateExpression", "BinaryExpression", "LogicalExpression", "AssignmentExpression", "ConditionalExpression", "CallExpression", "NewExpression", "MemberExpression", "SequenceExpression", "YieldExpression", "AwaitExpression", "MetaProperty", "TaggedTemplateExpression", "TemplateLiteral", "TemplateElement", "AssignmentPattern", "ArrayPattern", "ObjectPattern", "RestElement", "SpreadElement", "ClassExpression", "Super", "ImportExpression",
    "JsxElement", "JsxOpeningElement", "JsxClosingElement", "JsxAttribute", "JsxSpreadAttribute", "JsxExpressionContainer", "JsxFragment", "JsxText",
    "TsTypeAnnotation", "TsTypeReference", "TsParameterProperty", "TsDeclareFunction", "TsDeclareMethod", "TsQualifiedName", "TsCallSignatureDeclaration", "TsConstructSignatureDeclaration", "TsPropertySignature", "TsMethodSignature", "TsIndexSignature", "TsTypePredicate", "TsNonNullExpression", "TsAsExpression", "TsSatisfiesExpression", "TsTypeAliasDeclaration", "TsInterfaceDeclaration", "TsInterfaceBody", "TsEnumDeclaration", "TsEnumMember", "TsModuleDeclaration", "TsModuleBlock", "TsImportType", "TsImportEqualsDeclaration", "TsExternalModuleReference", "TsTypeParameterDeclaration", "TsTypeParameterInstantiation", "TsTypeParameter", "TsConditionalType", "TsInferType", "TsParenthesizedType", "TsFunctionType", "TsConstructorType", "TsTypeLiteral", "TsArrayType", "TsTupleType", "TsOptionalType", "TsRestType", "TsUnionType", "TsIntersectionType", "TsIndexedAccessType", "TsMappedType", "TsLiteralType", "TsThisType", "TsTypeOperator", "TsTemplateLiteralType", "TsDecorator"
};

/**
 * @brief NodeType 列挙子を対応する文字列形式に変換します。
 * @param type 変換対象の NodeType 列挙子
 * @return NodeType に対応する文字列へのポインタ
 */
const char* nodeTypeToString(NodeType type) noexcept {
  const size_t index = static_cast<size_t>(type);

  if (type == NodeType::Uninitialized) {
    assert(index == 0 && "NodeType::Uninitialized の値が 0 ではありません");
    assert(std::string_view(NODE_TYPE_STRINGS[0]) == "Uninitialized" && "NODE_TYPE_STRINGS[0] が \"Uninitialized\" ではありません");
    return NODE_TYPE_STRINGS[0];
  }

  const size_t count = static_cast<size_t>(NodeType::Count);
  if (index > 0 && index < count) {
    assert(std::size(NODE_TYPE_STRINGS) == count && "NODE_TYPE_STRINGS のサイズが NodeType::Count と一致しません");
    return NODE_TYPE_STRINGS[index];
  }

  assert(false && "不明または未対応の NodeType が検出されました");
  return "Unknown";
}

// Node クラス実装

/**
 * @brief Node クラスのコンストラクタ
 */
Node::Node(NodeType type, const SourceLocation& location, Node* parent)
    : m_type(type),
      m_location(location),
      m_parent(parent) {
  assert(type != NodeType::Uninitialized && "ノードの型として NodeType::Uninitialized を指定することはできません");
}

/**
 * @brief ノードの型を取得します
 */
NodeType Node::getType() const noexcept {
  return m_type;
}

/**
 * @brief ノードのソースコード上の位置情報を取得します
 */
const SourceLocation& Node::getLocation() const noexcept {
  return m_location;
}

/**
 * @brief ノードの開始オフセットを取得します
 */
size_t Node::getStartOffset() const noexcept {
  return m_location.offset;
}

/**
 * @brief ノードの終了オフセットを取得します
 */
size_t Node::getEndOffset() const noexcept {
  return m_location.offset + m_location.length;
}

/**
 * @brief 親ノードへのポインタを取得します
 */
Node* Node::getParent() const noexcept {
  return m_parent;
}

/**
 * @brief 親ノードへのポインタを設定します
 */
void Node::setParent(Node* parent) noexcept {
  assert(parent != this && "ノードを自身の親として設定することはできません");
  m_parent = parent;
}

/**
 * @brief このノードが直接持つ子ノードのリストを取得します (非 const 版)
 */
std::vector<Node*> Node::getChildren() {
  return {};
}

/**
 * @brief このノードが直接持つ子ノードのリストを取得します (const 版)
 */
std::vector<const Node*> Node::getChildren() const {
  return {};
}

/**
 * @brief ノードの基本情報をJSONオブジェクトに追加します
 */
void Node::baseJson(nlohmann::json& jsonNode) const {
  // ノードタイプを文字列として追加
  jsonNode["type"] = nodeTypeToString(m_type);

  // ソース位置情報を追加
  nlohmann::json loc;
  loc["start"] = {
      {"line", m_location.line},
      {"column", m_location.column},
      {"offset", m_location.offset}
  };

  // 終了位置情報
  loc["end"] = {
      {"offset", getEndOffset()}
  };

  // ソースファイル識別子の追加
  if (m_location.file_id.has_value()) {
    loc["source"] = m_location.file_id.value();
  } else {
    loc["source"] = nullptr;
  }

  jsonNode["loc"] = loc;

  // 開始・終了オフセットの範囲を追加
  jsonNode["range"] = {getStartOffset(), getEndOffset()};
}

/**
 * @brief ノードをJSONオブジェクト形式に変換します
 */
nlohmann::json Node::toJson([[maybe_unused]] bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  return jsonNode;
}

/**
 * @brief ノードの基本情報を表す文字列表現を取得します
 */
std::string Node::toString() const {
  try {
    return std::format("Node<type: {}, loc: L{}C{} O{}>",
                       nodeTypeToString(m_type),
                       m_location.line,
                       m_location.column,
                       m_location.offset);
  } catch (const std::format_error& e) {
    return std::string("Node<type: ") + nodeTypeToString(m_type) +
           ", loc: L" + std::to_string(m_location.line) +
           "C" + std::to_string(m_location.column) +
           " O" + std::to_string(m_location.offset) + ">";
  }
}

// 中間基底クラスの実装

/**
 * @brief StatementNode のコンストラクタ
 */
StatementNode::StatementNode(NodeType type, const SourceLocation& location, Node* parent)
    : Node(type, location, parent) {
  assert(isStatement(type) && "StatementNode のコンストラクタに文ではない NodeType が渡されました");
}

/**
 * @brief ExpressionNode のコンストラクタ
 */
ExpressionNode::ExpressionNode(NodeType type, const SourceLocation& location, Node* parent)
    : Node(type, location, parent) {
  assert(isExpression(type) && "ExpressionNode のコンストラクタに式ではない NodeType が渡されました");
}

/**
 * @brief DeclarationNode のコンストラクタ
 */
DeclarationNode::DeclarationNode(NodeType type, const SourceLocation& location, Node* parent)
    : Node(type, location, parent) {
  assert(isDeclaration(type) && "DeclarationNode のコンストラクタに宣言ではない NodeType が渡されました");
}

/**
 * @brief PatternNode のコンストラクタ
 */
PatternNode::PatternNode(NodeType type, const SourceLocation& location, Node* parent)
    : Node(type, location, parent) {
  assert(isPattern(type) && "PatternNode のコンストラクタにパターンではない NodeType が渡されました");
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs