/**
 * @file node.cpp
 * @brief AeroJS Abstract Syntax Tree (AST) の基底ノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`node.h` で宣言された `Node` クラスのメソッドと、
 * `NodeType` 列挙型を文字列に変換するユーティリティ関数を実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "node.h"
#include <stdexcept> // For std::out_of_range in nodeTypeToString
#include <nlohmann/json.hpp> // For JSON implementation
#include <string_view> // For efficient string comparisons
#include <cassert> // For assert()

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// NodeType を文字列に変換するための静的配列
static const char* const NODE_TYPE_STRINGS[] = {
    "Uninitialized",
    "Program", "BlockStatement", "EmptyStatement",
    "FunctionDeclaration", "VariableDeclaration", "VariableDeclarator", "ClassDeclaration", "ClassBody", "MethodDefinition", "ImportDeclaration", "ImportSpecifier", "ImportDefaultSpecifier", "ImportNamespaceSpecifier", "ExportNamedDeclaration", "ExportDefaultDeclaration", "ExportAllDeclaration", "ExportSpecifier",
    "ExpressionStatement", "IfStatement", "SwitchStatement", "SwitchCase", "ReturnStatement", "ThrowStatement", "TryStatement", "CatchClause", "WhileStatement", "DoWhileStatement", "ForStatement", "ForInStatement", "ForOfStatement", "BreakStatement", "ContinueStatement", "LabeledStatement", "WithStatement", "DebuggerStatement",
    "Identifier", "PrivateIdentifier", "Literal", "ThisExpression", "ArrayExpression", "ObjectExpression", "Property", "FunctionExpression", "ArrowFunctionExpression", "UnaryExpression", "UpdateExpression", "BinaryExpression", "LogicalExpression", "AssignmentExpression", "ConditionalExpression", "CallExpression", "NewExpression", "MemberExpression", "SequenceExpression", "YieldExpression", "AwaitExpression", "MetaProperty", "TaggedTemplateExpression", "TemplateLiteral", "TemplateElement", "AssignmentPattern", "ArrayPattern", "ObjectPattern", "RestElement", "SpreadElement", "ClassExpression", "Super", "ImportExpression",
    "JsxElement", "JsxOpeningElement", "JsxClosingElement", "JsxAttribute", "JsxSpreadAttribute", "JsxExpressionContainer", "JsxFragment", "JsxText",
    "TsTypeAnnotation", "TsTypeReference", "TsParameterProperty", "TsDeclareFunction", "TsDeclareMethod", "TsQualifiedName", "TsCallSignatureDeclaration", "TsConstructSignatureDeclaration", "TsPropertySignature", "TsMethodSignature", "TsIndexSignature", "TsTypePredicate", "TsNonNullExpression", "TsAsExpression", "TsSatisfiesExpression", "TsTypeAliasDeclaration", "TsInterfaceDeclaration", "TsInterfaceBody", "TsEnumDeclaration", "TsEnumMember", "TsModuleDeclaration", "TsModuleBlock", "TsImportType", "TsImportEqualsDeclaration", "TsExternalModuleReference", "TsTypeParameterDeclaration", "TsTypeParameterInstantiation", "TsTypeParameter", "TsConditionalType", "TsInferType", "TsParenthesizedType", "TsFunctionType", "TsConstructorType", "TsTypeLiteral", "TsArrayType", "TsTupleType", "TsOptionalType", "TsRestType", "TsUnionType", "TsIntersectionType", "TsIndexedAccessType", "TsMappedType", "TsLiteralType", "TsThisType", "TsTypeOperator", "TsTemplateLiteralType", "TsDecorator"
    // Count はここには含めない
};

const char* nodeTypeToString(NodeType type) {
    size_t index = static_cast<size_t>(type);
    if (index > 0 && index < static_cast<size_t>(NodeType::Count)) {
        // 配列のインデックスは 1 から始まる NodeType に対応するため、1を引く
        return NODE_TYPE_STRINGS[index -1];
    } else if (type == NodeType::Uninitialized) {
         return NODE_TYPE_STRINGS[0]; // "Uninitialized"
    }
    return "Unknown";
}

// --- Node Class Implementation ---

Node::Node(NodeType type, const SourceLocation& location, Node* parent)
    : m_type(type),
      m_location(location),
      m_parent(parent)
{
    // Uninitialized は許可しない
    assert(type != NodeType::Uninitialized && "Node type cannot be Uninitialized");
}

NodeType Node::getType() const noexcept {
    return m_type;
}

const SourceLocation& Node::getLocation() const noexcept {
    return m_location;
}

size_t Node::getStartOffset() const noexcept {
    return m_location.offset;
}

size_t Node::getEndOffset() const noexcept {
    // SourceLocation should store length or end offset directly
    // Assuming SourceLocation has a 'length' member for now.
    // Replace 'length' with the actual member name if different.
    return m_location.offset + m_location.length; // Or m_location.endOffset if available
}

Node* Node::getParent() const noexcept {
    return m_parent;
}

void Node::setParent(Node* parent) noexcept {
    // 自分自身を親に設定しようとしていないかチェック
    assert(parent != this && "Node cannot be its own parent");
    m_parent = parent;
}

// Default implementations for getChildren()
// Concrete node classes must override these if they have children.
std::vector<Node*> Node::getChildren() {
    return {};
}

std::vector<const Node*> Node::getChildren() const {
    return {};
}

// Base implementation for toJson()
void Node::baseJson(nlohmann::json& jsonNode) const {
     jsonNode["type"] = nodeTypeToString(m_type);
     jsonNode["loc"] = {
         {"start", {
             {"line", m_location.line},
             {"column", m_location.column},
             {"offset", m_location.offset}
         }},
         // Assuming SourceLocation stores length or end info
         {"end", {
             // Calculate end line/column if not stored directly (complex)
             // For simplicity, just store end offset for now
             // TODO: Enhance SourceLocation or calculate end line/col
             {"offset", getEndOffset()}
         }}
         // Potentially add source file ID if available in SourceLocation
         // {"sourceFileId", m_location.file_id}
     };
     // Optionally include range [startOffset, endOffset]
     jsonNode["range"] = {getStartOffset(), getEndOffset()};
}

nlohmann::json Node::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode);
    // Derived classes should override and add their specific properties AFTER calling baseJson
    // or incorporating its logic.
    return jsonNode;
}

// Base implementation for toString()
std::string Node::toString() const {
    // Provides a minimal representation, primarily for debugging.
    return std::string("Node<type: ") + nodeTypeToString(m_type) +
           ", loc: L" + std::to_string(m_location.line) +
           "C" + std::to_string(m_location.column) +
           " O" + std::to_string(m_location.offset) + ">";
}

// --- Intermediate Base Class Implementations ---

StatementNode::StatementNode(NodeType type, const SourceLocation& location, Node* parent)
    : Node(type, location, parent) {
    // TODO: ここで Statement Node 固有のチェックや初期化が必要か確認
    // 例: 渡された type が本当に文ノードタイプかどうかの assert など
}

ExpressionNode::ExpressionNode(NodeType type, const SourceLocation& location, Node* parent)
    : Node(type, location, parent) {
    // TODO: Expression Node 固有のチェックや初期化が必要か確認
}

DeclarationNode::DeclarationNode(NodeType type, const SourceLocation& location, Node* parent)
    : Node(type, location, parent) {
    // TODO: Declaration Node 固有のチェックや初期化が必要か確認
}

PatternNode::PatternNode(NodeType type, const SourceLocation& location, Node* parent)
    : Node(type, location, parent) {
    // TODO: Pattern Node 固有のチェックや初期化が必要か確認
}

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs