/**
 * @file all_nodes.h
 * @brief AeroJS AST の全ての具象ノードヘッダーをインクルードするファイル
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このヘッダーファイルは、`src/core/parser/ast/nodes/` ディレクトリおよび
 * そのサブディレクトリに定義されている全ての具象ASTノードクラスのヘッダーファイルを
 * インクルードします。AST全体を扱う際に、個々のヘッダーをインクルードする手間を省きます。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_ALL_NODES_H
#define AEROJS_PARSER_AST_NODES_ALL_NODES_H

// --- 基底クラス ---
#include "node.h"

// --- プログラム構造 ---
#include "program.h"
#include "statements/statements.h" // Includes BlockStatement, EmptyStatement
// #include "block_statement.h" // TODO: Implement -> Merged
// #include "empty_statement.h" // TODO: Implement -> Merged

// --- 宣言 ---
#include "declarations/variable_declaration.h" // Includes VariableDeclaration, VariableDeclarator
// #include "declarations/function_declaration.h" // TODO: Implement
// #include "declarations/class_declaration.h" // TODO: Implement
// #include "declarations/import_export.h" // TODO: Implement

// --- 文 (Statements) ---
// #include "statements/expression_statement.h" // TODO: Implement
// #include "statements/if_statement.h" // TODO: Implement
// #include "statements/switch_statement.h" // TODO: Implement
// #include "statements/loop_statements.h" // TODO: Implement (for, while, do-while, for-in, for-of)
// #include "statements/control_flow.h" // TODO: Implement (return, break, continue, throw)
// #include "statements/try_statement.h" // TODO: Implement
// #include "statements/labeled_statement.h" // TODO: Implement
// #include "statements/with_statement.h" // TODO: Implement
// #include "statements/debugger_statement.h" // TODO: Implement

// --- 式 (Expressions) ---
#include "expressions/literals.h" // Includes Literal
#include "expressions/identifier.h" // Includes Identifier, PrivateIdentifier
#include "expressions/binary_expression.h" // Includes BinaryExpression, LogicalExpression
#include "expressions/unary_expression.h" // Includes UnaryExpression, UpdateExpression
#include "expressions/assignment_expression.h" // Includes AssignmentExpression
#include "expressions/call_expression.h" // Includes CallExpression, NewExpression
#include "expressions/member_expression.h" // Includes MemberExpression
#include "src/core/parser/ast/nodes/expressions/this_expression.h"
#include "src/core/parser/ast/nodes/expressions/array_expression.h"
#include "src/core/parser/ast/nodes/expressions/arrow_function_expression.h"
#include "src/core/parser/ast/nodes/expressions/assignment_expression.h"
#include "src/core/parser/ast/nodes/expressions/await_expression.h"
#include "src/core/parser/ast/nodes/expressions/binary_expression.h"
#include "src/core/parser/ast/nodes/expressions/identifier.h"
#include "src/core/parser/ast/nodes/expressions/literal.h"
#include "src/core/parser/ast/nodes/expressions/logical_expression.h"
#include "src/core/parser/ast/nodes/expressions/object_expression.h"
#include "src/core/parser/ast/nodes/expressions/private_identifier.h"
#include "src/core/parser/ast/nodes/expressions/property.h"
#include "src/core/parser/ast/nodes/expressions/this_expression.h"
#include "src/core/parser/ast/nodes/expressions/unary_expression.h"
#include "src/core/parser/ast/nodes/expressions/update_expression.h"
// TODO: Implement the following expression headers
#include "src/core/parser/ast/nodes/expressions/call_expression.h"
#include "src/core/parser/ast/nodes/expressions/conditional_expression.h"
#include "src/core/parser/ast/nodes/expressions/function_expression.h"
#include "src/core/parser/ast/nodes/expressions/yield_await.h"
#include "src/core/parser/ast/nodes/expressions/member_expression.h"
#include "src/core/parser/ast/nodes/expressions/meta_property.h"
#include "src/core/parser/ast/nodes/expressions/sequence_expression.h"
#include "src/core/parser/ast/nodes/expressions/template_literal.h"
#include "src/core/parser/ast/nodes/expressions/class_expression.h"
#include "src/core/parser/ast/nodes/expressions/super.h"
#include "src/core/parser/ast/nodes/expressions/import_expression.h"

// --- パターン (Patterns) ---
// TODO: Implement the following pattern headers
// #include "patterns/assignment_pattern.h"
// #include "patterns/array_pattern.h"
// #include "patterns/object_pattern.h"
#include "src/core/parser/ast/nodes/patterns/rest_spread.h" // Includes RestElement, SpreadElement

// --- JSX (Optional) ---
// #ifdef AEROJS_ENABLE_JSX
// #include "jsx/jsx_element.h"
// ... other JSX nodes
// #endif

// --- TypeScript (Optional) ---
// #ifdef AEROJS_ENABLE_TYPESCRIPT
// #include "typescript/ts_types.h"
// ... other TypeScript nodes
// #endif

#endif // AEROJS_PARSER_AST_NODES_ALL_NODES_H