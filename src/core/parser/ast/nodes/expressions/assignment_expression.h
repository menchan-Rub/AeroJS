/**
 * @file assignment_expression.h
 * @brief AeroJS AST の代入式ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの代入式 (`=`, `+=`, `-=`, `&&=` など) を
 * 表すASTノード `AssignmentExpression` を定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_ASSIGNMENT_EXPRESSION_H_
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_ASSIGNMENT_EXPRESSION_H_

#include <string>
#include <utility>
#include <vector>

#include "src/core/parser/ast/node.h"
#include "src/core/parser/ast/nodes/expressions/expression.h"
#include "src/core/source_location.h"
#include "src/core/utils/macros.h"
#include "src/core/utils/optional.h"

namespace aerojs::parser::ast {

// Forward declaration for visitor pattern
class AstVisitor;
class ConstAstVisitor;

/**
 * @enum AssignmentOperator
 * @brief 代入演算子の種類。
 */
enum class AssignmentOperator {
  Assign,                    // =
  AdditionAssign,            // +=
  SubtractionAssign,         // -=
  MultiplicationAssign,      // *=
  DivisionAssign,            // /=
  RemainderAssign,           // %=
  ExponentiationAssign,      // **=
  LeftShiftAssign,           // <<=
  RightShiftAssign,          // >>=
  UnsignedRightShiftAssign,  // >>>=
  BitwiseAndAssign,          // &=
  BitwiseOrAssign,           // |=
  BitwiseXorAssign,          // ^=
  LogicalAndAssign,          // &&=
  LogicalOrAssign,           // ||=
  NullishCoalescingAssign    // ??=
};

/**
 * @brief AssignmentOperator を文字列に変換します。
 */
std::string AssignmentOperatorToString(AssignmentOperator op);

/**
 * @class AssignmentExpression
 * @brief 代入式を表すノード。
 *
 * @details
 * 左辺値 (Identifier または MemberExpression) と右辺の式、および代入演算子を持ちます。
 */
class AssignmentExpression final : public Expression {
 public:
  /**
   * @brief AssignmentExpressionノードのコンストラクタ。
   * @param location ソースコード内のこのノードの位置情報。
   * @param op 代入演算子。
   * @param left 左辺値 (Identifier or MemberExpression or Pattern)。ムーブされる。
   * @param right 右辺の式 (Expression)。ムーブされる。
   */
  AssignmentExpression(SourceLocation location, AssignmentOperator op,
                       std::unique_ptr<Expression> left,
                       std::unique_ptr<Expression> right);

  ~AssignmentExpression() override;

  // Delete copy and move operations
  AssignmentExpression(const AssignmentExpression&) = delete;
  AssignmentExpression& operator=(const AssignmentExpression&) = delete;
  AssignmentExpression(AssignmentExpression&&) = delete;
  AssignmentExpression& operator=(AssignmentExpression&&) = delete;

  /**
   * @brief 代入演算子を取得します。
   * @return 演算子 (`AssignmentOperator`)。
   */
  [[nodiscard]] AssignmentOperator GetOperator() const {
    return operator_;
  }

  /**
   * @brief 左辺値を取得します (非const版)。
   * @return 左辺値ノード (Identifier, MemberExpr, Pattern) への参照 (`Expression*`)。
   */
  [[nodiscard]] Expression* GetLeft() const {
    return left_.get();
  }

  /**
   * @brief 左辺値を取得します (const版)。
   * @return 左辺値ノード (Identifier, MemberExpr, Pattern) へのconst参照 (`const Expression*`)。
   */
  [[nodiscard]] const Expression* GetLeft() const {
    return left_.get();
  }

  /**
   * @brief 右辺の式を取得します (非const版)。
   * @return 右辺の式ノードへの参照 (`Expression*`)。
   */
  [[nodiscard]] Expression* GetRight() const {
    return right_.get();
  }

  /**
   * @brief 右辺の式を取得します (const版)。
   * @return 右辺の式ノードへのconst参照 (`const Expression*`)。
   */
  [[nodiscard]] const Expression* GetRight() const {
    return right_.get();
  }

  /**
   * @brief ノードを受け入れるためのビジターパターン。
   * @param visitor 受け入れるビジター。
   */
  void Accept(AstVisitor* visitor) override;

  /**
   * @brief ノードを受け入れるためのビジターパターン。
   * @param visitor 受け入れるビジター。
   */
  void Accept(ConstAstVisitor* visitor) const override;

  /**
   * @brief この式の子ノードを取得します。
   * @return 子ノードのポインタのベクター。
   */
  [[nodiscard]] std::vector<Node*> GetChildren() override;
  [[nodiscard]] std::vector<const Node*> GetChildren() const override;

  /**
   * @brief ノードをJSON形式にシリアライズします。
   * @param factory JSONファクトリーを使用してJSON値を作成します。
   * @return ノードのJSON表現。
   */
  [[nodiscard]] JsonValue toJson(JsonFactory& factory) const override;

  /**
   * @brief ノードの型を文字列として返します。
   * @return ノードの型の文字列 "AssignmentExpression"。
   */
  [[nodiscard]] const char* GetTypeString() const override {
    return "AssignmentExpression";
  }

 private:
  AssignmentOperator operator_;
  std::unique_ptr<Expression> left_;  // Must be a LeftHandSideExpression conceptually
  std::unique_ptr<Expression> right_;
};

}  // namespace aerojs::parser::ast

#endif  // AEROJS_PARSER_AST_NODES_EXPRESSIONS_ASSIGNMENT_EXPRESSION_H_