/**
 * @file node_type.h
 * @brief ASTノードタイプ定義
 * @version 0.1.0
 * @license MIT
 */

#pragma once

#include <memory>

namespace aerojs {
namespace parser {
namespace ast {

// 前方宣言
class Node;

// NodePtrの定義
using NodePtr = std::shared_ptr<Node>;

/**
 * @enum NodeType
 * @brief ASTノードの型を表す列挙型
 */
enum class NodeType {
    // 基本ノード
    Program,
    Identifier,
    Literal,
    
    // 式
    BinaryExpression,
    UnaryExpression,
    AssignmentExpression,
    UpdateExpression,
    LogicalExpression,
    ConditionalExpression,
    CallExpression,
    MemberExpression,
    ArrayExpression,
    ObjectExpression,
    FunctionExpression,
    ArrowFunctionExpression,
    
    // 文
    ExpressionStatement,
    BlockStatement,
    EmptyStatement,
    DebuggerStatement,
    WithStatement,
    ReturnStatement,
    LabeledStatement,
    BreakStatement,
    ContinueStatement,
    IfStatement,
    SwitchStatement,
    ThrowStatement,
    TryStatement,
    WhileStatement,
    DoWhileStatement,
    ForStatement,
    ForInStatement,
    ForOfStatement,
    
    // 宣言
    FunctionDeclaration,
    VariableDeclaration,
    VariableDeclarator,
    
    // その他
    Property,
    SwitchCase,
    CatchClause,
    Super,
    SpreadElement,
    RestElement,
    AssignmentPattern,
    ArrayPattern,
    ObjectPattern,
    
    // ES6+
    ClassDeclaration,
    ClassExpression,
    MethodDefinition,
    ImportDeclaration,
    ExportNamedDeclaration,
    ExportDefaultDeclaration,
    ExportAllDeclaration,
    ImportSpecifier,
    ExportSpecifier,
    ImportDefaultSpecifier,
    ImportNamespaceSpecifier,
    
    // JSX (将来の拡張用)
    JSXElement,
    JSXFragment,
    JSXText,
    JSXExpressionContainer,
    JSXAttribute,
    JSXSpreadAttribute,
    JSXOpeningElement,
    JSXClosingElement,
    JSXIdentifier,
    JSXMemberExpression,
    JSXNamespacedName,
    
    // TypeScript (将来の拡張用)
    TSTypeAnnotation,
    TSTypeReference,
    TSInterfaceDeclaration,
    TSTypeAliasDeclaration,
    TSEnumDeclaration,
    TSModuleDeclaration,
    
    // その他の特殊ノード
    TemplateLiteral,
    TaggedTemplateExpression,
    TemplateElement,
    MetaProperty,
    AwaitExpression,
    YieldExpression,
    
    // エラー処理用
    Unknown,
    Error
};

/**
 * @brief ノードタイプを文字列に変換
 * @param type ノードタイプ
 * @return 文字列表現
 */
const char* nodeTypeToString(NodeType type);

/**
 * @brief ノードタイプが式かどうかを判定
 * @param type ノードタイプ
 * @return 式の場合true
 */
bool isExpression(NodeType type);

/**
 * @brief ノードタイプが文かどうかを判定
 * @param type ノードタイプ
 * @return 文の場合true
 */
bool isStatement(NodeType type);

/**
 * @brief ノードタイプが宣言かどうかを判定
 * @param type ノードタイプ
 * @return 宣言の場合true
 */
bool isDeclaration(NodeType type);

} // namespace ast
} // namespace parser
} // namespace aerojs 