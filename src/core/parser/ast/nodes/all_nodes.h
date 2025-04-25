/*******************************************************************************
 * @file src/core/parser/ast/nodes/all_nodes.h
 * @brief 全ての具象AeroJS ASTノード型をまとめてインクルードするヘッダー
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このヘッダーはASTノード定義を一元的にまとめることで、
 * インクルードを簡潔にし、保守性を向上させます。
 * 必要に応じて条件付きコンパイルで追加ユーティリティも含みます。
 ******************************************************************************/

#ifndef AEROJS_PARSER_AST_NODES_ALL_NODES_H
#define AEROJS_PARSER_AST_NODES_ALL_NODES_H

#include "expressions/array_expression.h"
#include "expressions/arrow_function_expression.h"
#include "expressions/assignment_expression.h"
#include "expressions/await_expression.h"
#include "expressions/binary_expression.h"
#include "expressions/call_expression.h"
#include "expressions/class_expression.h"
#include "expressions/conditional_expression.h"
#include "expressions/function_expression.h"
#include "expressions/identifier.h"
#include "expressions/import_expression.h"
#include "expressions/literal.h"
#include "expressions/logical_expression.h"
#include "expressions/member_expression.h"
#include "expressions/meta_property.h"
#include "expressions/new_expression.h"
#include "expressions/object_expression.h"
#include "expressions/private_identifier.h"
#include "expressions/sequence_expression.h"
#include "expressions/super.h"
#include "expressions/tagged_template_expression.h"
#include "expressions/this_expression.h"
#include "expressions/unary_expression.h"
#include "expressions/update_expression.h"
#include "expressions/yield_expression.h"
#include "node.h"  // ASTノードの基本
#include "nodes/declarations/class_declaration.h"
#include "nodes/declarations/function_declaration.h"
#include "nodes/declarations/import_export.h"
#include "nodes/declarations/variable_declaration.h"
#include "patterns/array_pattern.h"
#include "patterns/assignment_pattern.h"
#include "patterns/object_pattern.h"
#include "patterns/rest_spread.h"
#include "program.h"                // Programノード
#include "statements/statements.h"  // 主要な文ノードをまとめて

#ifdef AEROJS_ENABLE_JSX
#include "jsx/jsx_element.h"
#endif

#ifdef AEROJS_ENABLE_TYPESCRIPT
#include "typescript/ts_types.h"
#endif

#include "nodes/declarations/declaration_node.h"
#include "statements/statement_node.h"

#ifdef AEROJS_ALL_NODES_EXTRA_UTILS

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace aerojs::parser::ast {
class Node;
enum class NodeType;
class Identifier;
class Literal;
class BinaryExpression;
class UnaryExpression;
class MemberExpression;
class LogicalExpression;
class CallExpression;
class NewExpression;
class AssignmentExpression;
class UpdateExpression;
class YieldExpression;
class AwaitExpression;
class FunctionExpression;
class ArrowFunctionExpression;
class ExpressionStatement;
class Program;
class BlockStatement;
class FunctionDeclaration;
class MethodDefinition;
class ClassDeclaration;
class ClassExpression;
class ClassBody;
enum class BinaryOperator;
enum class UnaryOperator;
class TemplateLiteral;
class Property;
class SpreadElement;
}  // namespace aerojs::parser::ast

namespace aerojs::parser::ast::header_utils {

/**
 * @brief NodeType を文字列に変換します。
 */
inline const char* NodeTypeToString(NodeType type) {
  switch (type) {
      // プログラム
    case NodeType::PROGRAM:
      return "Program";
    // 式
    case NodeType::IDENTIFIER:
      return "Identifier";
    case NodeType::PRIVATE_IDENTIFIER:
      return "PrivateIdentifier";
    case NodeType::SUPER:
      return "Super";
    case NodeType::THIS_EXPRESSION:
      return "ThisExpression";
    case NodeType::META_PROPERTY:
      return "MetaProperty";
    case NodeType::STRING_LITERAL:
      return "StringLiteral";
    case NodeType::NUMERIC_LITERAL:
      return "NumericLiteral";
    case NodeType::BOOLEAN_LITERAL:
      return "BooleanLiteral";
    case NodeType::NULL_LITERAL:
      return "NullLiteral";
    case NodeType::REGEXP_LITERAL:
      return "RegExpLiteral";
    case NodeType::TEMPLATE_LITERAL:
      return "TemplateLiteral";
    case NodeType::ARRAY_EXPRESSION:
      return "ArrayExpression";
    case NodeType::ARROW_FUNCTION_EXPRESSION:
      return "ArrowFunctionExpression";
    case NodeType::ASSIGNMENT_EXPRESSION:
      return "AssignmentExpression";
    case NodeType::AWAIT_EXPRESSION:
      return "AwaitExpression";
    case NodeType::BINARY_EXPRESSION:
      return "BinaryExpression";
    case NodeType::CALL_EXPRESSION:
      return "CallExpression";
    case NodeType::CLASS_EXPRESSION:
      return "ClassExpression";
    case NodeType::CONDITIONAL_EXPRESSION:
      return "ConditionalExpression";
    case NodeType::FUNCTION_EXPRESSION:
      return "FunctionExpression";
    case NodeType::IMPORT_EXPRESSION:
      return "ImportExpression";
    case NodeType::LOGICAL_EXPRESSION:
      return "LogicalExpression";
    case NodeType::MEMBER_EXPRESSION:
      return "MemberExpression";
    case NodeType::NEW_EXPRESSION:
      return "NewExpression";
    case NodeType::OBJECT_EXPRESSION:
      return "ObjectExpression";
    case NodeType::SEQUENCE_EXPRESSION:
      return "SequenceExpression";
    case NodeType::TAGGED_TEMPLATE_EXPRESSION:
      return "TaggedTemplateExpression";
    case NodeType::UNARY_EXPRESSION:
      return "UnaryExpression";
    case NodeType::UPDATE_EXPRESSION:
      return "UpdateExpression";
    case NodeType::YIELD_EXPRESSION:
      return "YieldExpression";
    // 文
    case NodeType::BLOCK_STATEMENT:
      return "BlockStatement";
    case NodeType::EXPRESSION_STATEMENT:
      return "ExpressionStatement";
    case NodeType::EMPTY_STATEMENT:
      return "EmptyStatement";
    case NodeType::IF_STATEMENT:
      return "IfStatement";
    case NodeType::RETURN_STATEMENT:
      return "ReturnStatement";
    case NodeType::FOR_STATEMENT:
      return "ForStatement";
    case NodeType::WHILE_STATEMENT:
      return "WhileStatement";
    case NodeType::DO_WHILE_STATEMENT:
      return "DoWhileStatement";
    case NodeType::FOR_IN_STATEMENT:
      return "ForInStatement";
    case NodeType::FOR_OF_STATEMENT:
      return "ForOfStatement";
    case NodeType::SWITCH_STATEMENT:
      return "SwitchStatement";
    case NodeType::SWITCH_CASE:
      return "SwitchCase";
    case NodeType::BREAK_STATEMENT:
      return "BreakStatement";
    case NodeType::CONTINUE_STATEMENT:
      return "ContinueStatement";
    case NodeType::THROW_STATEMENT:
      return "ThrowStatement";
    case NodeType::TRY_STATEMENT:
      return "TryStatement";
    case NodeType::CATCH_CLAUSE:
      return "CatchClause";
    // 宣言
    case NodeType::VARIABLE_DECLARATION:
      return "VariableDeclaration";
    case NodeType::VARIABLE_DECLARATOR:
      return "VariableDeclarator";
    case NodeType::FUNCTION_DECLARATION:
      return "FunctionDeclaration";
    case NodeType::CLASS_DECLARATION:
      return "ClassDeclaration";
    case NodeType::CLASS_BODY:
      return "ClassBody";
    case NodeType::METHOD_DEFINITION:
      return "MethodDefinition";
    case NodeType::IMPORT_DECLARATION:
      return "ImportDeclaration";
    case NodeType::EXPORT_NAMED_DECLARATION:
      return "ExportNamedDeclaration";
    case NodeType::EXPORT_DEFAULT_DECLARATION:
      return "ExportDefaultDeclaration";
    case NodeType::EXPORT_ALL_DECLARATION:
      return "ExportAllDeclaration";
    case NodeType::EXPORT_SPECIFIER:
      return "ExportSpecifier";
    // パターン
    case NodeType::OBJECT_PATTERN:
      return "ObjectPattern";
    case NodeType::ARRAY_PATTERN:
      return "ArrayPattern";
    case NodeType::ASSIGNMENT_PATTERN:
      return "AssignmentPattern";
    case NodeType::REST_ELEMENT:
      return "RestElement";
    case NodeType::SPREAD_ELEMENT:
      return "SpreadElement";
      // その他
    case NodeType::PROPERTY:
      return "Property";
    case NodeType::TEMPLATE_ELEMENT:
      return "TemplateElement";

    default:
      return "UnknownNodeType";
  }
}

/**
 * @brief 式に副作用があるかを保守的に判定します。
 * @param node チェック対象の式ノード
 * @param max_depth 再帰深度の上限。0以下で打ち切り副作用ありと判断
 * @return 副作用の可能性がある場合は true
 */
inline bool MightHaveSideEffects(const Node* node, int max_depth = 50) {
  if (!node) {
    return false;  // null は副作用なし
  }
  if (max_depth <= 0) {
    return true;  // 深度上限到達時は保守的に副作用あり
  }

  switch (node->GetType()) {
    // リテラル類: 定数のみで副作用なし
    case NodeType::NUMERIC_LITERAL:
    case NodeType::STRING_LITERAL:
    case NodeType::BOOLEAN_LITERAL:
    case NodeType::NULL_LITERAL:
    case NodeType::REGEXP_LITERAL:
      return false;

    // テンプレートリテラル: 埋め込み式を再帰チェック
    case NodeType::TEMPLATE_LITERAL: {
      const auto* tmpl = dynamic_cast<const TemplateLiteral*>(node);
      if (!tmpl) {
        return true;
      }
      for (const auto* expr : tmpl->GetExpressions()) {
        if (MightHaveSideEffects(expr, max_depth - 1)) {
          return true;
        }
      }
      return false;
    }

    // 識別子・this: 参照のみで副作用なし
    case NodeType::IDENTIFIER:
    case NodeType::THIS_EXPRESSION:
      return false;

    // 関数・クラスの定義: 定義時は副作用なし
    case NodeType::FUNCTION_EXPRESSION:
    case NodeType::ARROW_FUNCTION_EXPRESSION:
    case NodeType::CLASS_EXPRESSION:
      return false;

    // 単項演算子: delete は副作用あり、その他はオペランドをチェック
    case NodeType::UNARY_EXPRESSION: {
      const auto* unary = dynamic_cast<const UnaryExpression*>(node);
      if (!unary) {
        return true;
      }
      if (unary->GetOperator() == UnaryOperator::DELETE) {
        return true;
      }
      return MightHaveSideEffects(unary->GetOperand(), max_depth - 1);
    }

    // 明らかな副作用を伴う表現
    case NodeType::ASSIGNMENT_EXPRESSION:
    case NodeType::UPDATE_EXPRESSION:
    case NodeType::CALL_EXPRESSION:
    case NodeType::NEW_EXPRESSION:
    case NodeType::YIELD_EXPRESSION:
    case NodeType::AWAIT_EXPRESSION:
    case NodeType::TAGGED_TEMPLATE_EXPRESSION:
      return true;

    // 二項演算
    case NodeType::BINARY_EXPRESSION: {
      const auto* bin = dynamic_cast<const BinaryExpression*>(node);
      if (!bin) {
        return true;
      }
      return MightHaveSideEffects(bin->GetLeft(), max_depth - 1) || MightHaveSideEffects(bin->GetRight(), max_depth - 1);
    }

    // 論理演算 (短絡評価)
    case NodeType::LOGICAL_EXPRESSION: {
      const auto* log = dynamic_cast<const LogicalExpression*>(node);
      if (!log) {
        return true;
      }
      return MightHaveSideEffects(log->GetLeft(), max_depth - 1) || MightHaveSideEffects(log->GetRight(), max_depth - 1);
    }

    // 条件演算子
    case NodeType::CONDITIONAL_EXPRESSION: {
      const auto* cond = dynamic_cast<const ConditionalExpression*>(node);
      if (!cond) {
        return true;
      }
      return MightHaveSideEffects(cond->GetTest(), max_depth - 1) || MightHaveSideEffects(cond->GetConsequent(), max_depth - 1) || MightHaveSideEffects(cond->GetAlternate(), max_depth - 1);
    }

    // メンバーアクセス: オブジェクトとプロパティをチェック
    case NodeType::MEMBER_EXPRESSION: {
      const auto* mem = dynamic_cast<const MemberExpression*>(node);
      if (!mem) {
        return true;
      }
      bool objSE = MightHaveSideEffects(mem->GetObject(), max_depth - 1);
      bool propSE = mem->IsComputed()
                        ? MightHaveSideEffects(mem->GetProperty(), max_depth - 1)
                        : false;
      // ゲッター呼び出しの可能性にも注意
      return objSE || propSE;
    }

    // カンマ演算子
    case NodeType::SEQUENCE_EXPRESSION: {
      const auto* seq = dynamic_cast<const SequenceExpression*>(node);
      if (!seq) {
        return true;
      }
      for (const auto* e : seq->GetExpressions()) {
        if (MightHaveSideEffects(e, max_depth - 1)) {
          return true;
        }
      }
      return false;
    }

    // 配列リテラル
    case NodeType::ARRAY_EXPRESSION: {
      const auto* arr = dynamic_cast<const ArrayExpression*>(node);
      if (!arr) {
        return true;
      }
      for (const auto* el : arr->GetElements()) {
        if (el && MightHaveSideEffects(el, max_depth - 1)) {
          return true;
        }
      }
      return false;
    }

    // オブジェクトリテラル
    case NodeType::OBJECT_EXPRESSION: {
      const auto* obj = dynamic_cast<const ObjectExpression*>(node);
      if (!obj) {
        return true;
      }
      for (const auto* pn : obj->GetProperties()) {
        if (!pn) continue;
        if (const auto* prop = dynamic_cast<const Property*>(pn)) {
          if (prop->IsComputed() &&
              MightHaveSideEffects(prop->GetKey(), max_depth - 1)) {
            return true;
          }
          if (MightHaveSideEffects(prop->GetValue(), max_depth - 1)) {
            return true;
          }
        } else if (const auto* sp = dynamic_cast<const SpreadElement*>(pn)) {
          if (MightHaveSideEffects(sp->GetArgument(), max_depth - 1)) {
            return true;
          }
        } else {
          // 想定外は保守的に副作用ありと見なす
          return true;
        }
      }
      return false;
    }

    // スプレッド要素
    case NodeType::SPREAD_ELEMENT: {
      const auto* sp = dynamic_cast<const SpreadElement*>(node);
      if (!sp) {
        return true;
      }
      return MightHaveSideEffects(sp->GetArgument(), max_depth - 1);
    }

    // その他は保守的に副作用あり
    default:
      return true;
  }
}

}  // namespace aerojs::parser::ast::header_utils

#endif  // AEROJS_ALL_NODES_EXTRA_UTILS

#endif  // AEROJS_PARSER_AST_NODES_ALL_NODES_H