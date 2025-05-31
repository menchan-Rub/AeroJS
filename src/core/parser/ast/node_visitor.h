/**
 * @file node_visitor.h
 * @brief AeroJS AST ビジターパターンの基底クラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、AeroJSのAST (Abstract Syntax Tree) を走査するための
 * シンプルなビジターパターンインターフェースを定義します。
 */

#ifndef AEROJS_PARSER_AST_NODE_VISITOR_H
#define AEROJS_PARSER_AST_NODE_VISITOR_H

#include "node.h"

namespace aerojs {
namespace parser {
namespace ast {

// 前方宣言
class Node;
class CallExpression;
class IfStatement;
class ForStatement;
class WhileStatement;
class ForOfStatement;
class TryStatement;
class BinaryExpression;
class ConditionalExpression;
class MemberExpression;
class AssignmentExpression;
class Identifier;
class ArrayExpression;
class UpdateExpression;
class NewExpression;

/**
 * @class NodeVisitor
 * @brief ASTノードを訪問するためのシンプルなビジターインターフェース
 */
class NodeVisitor {
public:
    virtual ~NodeVisitor() = default;

    // ビジターメソッド
    virtual bool Visit(Node* node) { return true; }
    virtual bool Visit(CallExpression* node) { return Visit(static_cast<Node*>(node)); }
    virtual bool Visit(IfStatement* node) { return Visit(static_cast<Node*>(node)); }
    virtual bool Visit(ForStatement* node) { return Visit(static_cast<Node*>(node)); }
    virtual bool Visit(WhileStatement* node) { return Visit(static_cast<Node*>(node)); }
    virtual bool Visit(ForOfStatement* node) { return Visit(static_cast<Node*>(node)); }
    virtual bool Visit(TryStatement* node) { return Visit(static_cast<Node*>(node)); }
    virtual bool Visit(BinaryExpression* node) { return Visit(static_cast<Node*>(node)); }
    virtual bool Visit(ConditionalExpression* node) { return Visit(static_cast<Node*>(node)); }
    virtual bool Visit(MemberExpression* node) { return Visit(static_cast<Node*>(node)); }
    virtual bool Visit(AssignmentExpression* node) { return Visit(static_cast<Node*>(node)); }
    virtual bool Visit(Identifier* node) { return Visit(static_cast<Node*>(node)); }
    virtual bool Visit(ArrayExpression* node) { return Visit(static_cast<Node*>(node)); }
    virtual bool Visit(UpdateExpression* node) { return Visit(static_cast<Node*>(node)); }
    virtual bool Visit(NewExpression* node) { return Visit(static_cast<Node*>(node)); }
};

} // namespace ast
} // namespace parser
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODE_VISITOR_H 