/**
 * @file node.h
 * @brief AeroJS AST ノードの基底クラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、AeroJSのAST (Abstract Syntax Tree) の基底ノードクラスを定義します。
 */

#ifndef AEROJS_PARSER_AST_NODE_H
#define AEROJS_PARSER_AST_NODE_H

#include <string>
#include <memory>
#include <vector>

namespace aerojs {
namespace parser {
namespace ast {

// 前方宣言
class NodeVisitor;

/**
 * @enum NodeType
 * @brief ASTノードの種類を表す列挙型
 */
enum class NodeType {
    // 式
    kIdentifier,
    kLiteral,
    kArrayExpression,
    kObjectExpression,
    kFunctionExpression,
    kArrowFunctionExpression,
    kCallExpression,
    kNewExpression,
    kMemberExpression,
    kBinaryExpression,
    kUnaryExpression,
    kUpdateExpression,
    kAssignmentExpression,
    kLogicalExpression,
    kConditionalExpression,
    kSequenceExpression,
    kThisExpression,
    kTemplateExpression,
    kTaggedTemplateExpression,
    kSpreadElement,
    kAwaitExpression,
    kYieldExpression,
    kSuperExpression,
    kClassExpression,

    // 文
    kBlockStatement,
    kExpressionStatement,
    kReturnStatement,
    kIfStatement,
    kSwitchStatement,
    kTryStatement,
    kThrowStatement,
    kWhileStatement,
    kDoWhileStatement,
    kForStatement,
    kForInStatement,
    kForOfStatement,
    kBreakStatement,
    kContinueStatement,
    kLabeledStatement,
    kEmptyStatement,
    kWithStatement,
    kDebuggerStatement,

    // 宣言
    kFunctionDeclaration,
    kVariableDeclaration,
    kClassDeclaration,
    kImportDeclaration,
    kExportDeclaration,

    // プログラム
    kProgram,

    // その他
    kUnknown
};

/**
 * @class Node
 * @brief AST ノードの基底クラス
 */
class Node {
public:
    // コンストラクタ
    Node(NodeType type) : m_type(type) {}
    
    // デストラクタ (派生クラスのデストラクタが呼ばれるようにvirtual)
    virtual ~Node() = default;

    // ノードの種類を取得
    NodeType GetType() const { return m_type; }

    // ノードを文字列形式で表現
    virtual std::string ToString() const { return "Node"; }

    // ビジターによる訪問を受け入れる
    virtual bool Accept(NodeVisitor* visitor) = 0;

private:
    NodeType m_type;
};

// ノードへのスマートポインタ型定義
using NodePtr = std::shared_ptr<Node>;

} // namespace ast
} // namespace parser
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODE_H 