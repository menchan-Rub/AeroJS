/*******************************************************************************
 * @file src/core/transformers/dead_code_elimination.cpp
 * @version 1.0.0
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 * @brief 不要なコード（デッドコード）を除去するトランスフォーマーの実装ファイル。
 ******************************************************************************/

#include "src/core/transformers/dead_code_elimination.h"  // 対応ヘッダーを最初にインクルード

#include <algorithm>  // std::remove_if など
#include <cmath>      // isnan, isinf などの数値評価関数
#include <limits>     // 数値の限界値
#include <memory>     // std::make_shared, std::move
#include <vector>     // std::vector

// AST関連
#include "src/core/common/logger.h"                // ログ出力
#include "src/core/parser/ast/ast_node_factory.h"  // ノード生成
#include "src/core/parser/ast/ast_node_types.h"
#include "src/core/parser/ast/nodes/all_nodes.h"

namespace aerojs::core::transformers {

// 必要な型のみ using 宣言
using aerojs::parser::ast::BlockStatement;
using aerojs::parser::ast::CaseClause;
using aerojs::parser::ast::ExpressionStatement;
using aerojs::parser::ast::ForStatement;
using aerojs::parser::ast::IfStatement;
using aerojs::parser::ast::LiteralPtr;
using aerojs::parser::ast::LiteralType;
using aerojs::parser::ast::Node;
using aerojs::parser::ast::NodePtr;
using aerojs::parser::ast::NodeType;
using aerojs::parser::ast::ReturnStatement;
using aerojs::parser::ast::SwitchStatement;
using aerojs::parser::ast::UnaryOperator;

// === コンストラクタ ===

DeadCodeEliminationTransformer::DeadCodeEliminationTransformer(
    optimization::OptimizationLevel optimizationLevel)
    : Transformer("DeadCodeEliminationTransformer", "デッドコードと到達不能コードを除去します"),
      m_optimizationLevel(optimizationLevel),
      m_unreachableCodeDetected(false),
      m_usedGlobals()
{
  // スコープスタックは空の状態で初期化される
}

// === ITransformer 実装 ===

TransformResult DeadCodeEliminationTransformer::Transform(NodePtr node) {
  // 統計情報をリセット
  m_statistics = Statistics();
  
  // スコープスタックをクリア
  while (!m_scopeStack.empty()) {
    m_scopeStack.pop();
  }
  
  // グローバル変数の使用状況をリセット
  m_usedGlobals.clear();
  
  // 到達不能フラグをリセット
  m_unreachableCodeDetected = false;
  
  // 基底クラスの変換処理を呼び出す
  return Transformer::Transform(std::move(node));
}

// === 設定・統計 ===

void DeadCodeEliminationTransformer::SetOptimizationLevel(optimization::OptimizationLevel level) {
  m_optimizationLevel = level;
}

optimization::OptimizationLevel DeadCodeEliminationTransformer::GetOptimizationLevel() const noexcept {
  return m_optimizationLevel;
}

const DeadCodeEliminationTransformer::Statistics& DeadCodeEliminationTransformer::GetStatistics() const noexcept {
  return m_statistics;
}

// === AST Visitor メソッドの実装 ===

void DeadCodeEliminationTransformer::VisitBlockStatement(BlockStatement* node) {
  // ブロックスコープに入る
  EnterScope("block");

  // 現在のスコープが到達不能かチェック
  if (IsCurrentScopeUnreachable()) {
    // このブロック全体が到達不能なので空にする
    if (!node->GetBody().empty()) {
      node->ClearBody();
      m_statistics.m_removedStatements += node->GetBody().size();
    }
    LeaveScope();
    return;
  }

  // 到達不能コードを削除
  bool unreachableRemoved = RemoveUnreachableCode(node->GetBodyMut());
  if (unreachableRemoved) {
    m_statistics.m_unreachableCodeBlocks++;
  }

  // 効果のない式を削除
  bool noEffectRemoved = RemoveNoEffectExpressions(node->GetBodyMut());
  if (noEffectRemoved) {
    m_statistics.m_removedStatements++;
  }

  // 子ノードを訪問
  Transformer::VisitBlockStatement(node);

  // スコープから出る
  LeaveScope();
}

void DeadCodeEliminationTransformer::VisitIfStatement(IfStatement* node) {
  // 到達不能なら何もしない
  if (IsCurrentScopeUnreachable()) return;

  // 先に子ノードを訪問して定数畳み込みなどを適用させる
  Transformer::VisitIfStatement(node);

  // 条件式を評価してみる
  std::optional<bool> conditionValue = TryEvaluateAsBoolean(node->GetTest());

  if (conditionValue.has_value()) {
    m_statistics.m_optimizedConditions++;
    NodePtr replacementNode = nullptr;
    if (conditionValue.value()) {
      // 条件が常に true -> then 節を採用
      common::Logger::Debug("if文の条件が常にtrueのため、then節に置き換えます");
      replacementNode = node->GetConsequent();
    } else {
      // 条件が常に false -> alternate 節を採用 (なければ null)
      common::Logger::Debug("if文の条件が常にfalseのため、else節に置き換えます");
      replacementNode = node->GetAlternate();
    }

    // ノードを置き換える
    ReplaceCurrentNode(std::move(replacementNode));
  }
}

void DeadCodeEliminationTransformer::VisitSwitchStatement(SwitchStatement* node) {
  if (IsCurrentScopeUnreachable()) return;

  // 子ノードを訪問
  Transformer::VisitSwitchStatement(node);

  // switch文の最適化
  std::optional<parser::ast::Value> discriminantValue = TryEvaluateConstant(node->GetDiscriminant());
  if (discriminantValue.has_value()) {
    // discriminantが定数の場合、一致するcaseだけを残す
    bool foundMatch = false;
    NodePtr matchingCase = nullptr;
    
    for (const auto& caseNode : node->GetCases()) {
      auto* caseClause = dynamic_cast<CaseClause*>(caseNode.get());
      if (!caseClause) continue;
      
      if (caseClause->GetTest()) {
        std::optional<parser::ast::Value> testValue = TryEvaluateConstant(caseClause->GetTest());
        if (testValue.has_value() && ValuesEqual(*discriminantValue, *testValue)) {
          foundMatch = true;
          matchingCase = caseNode;
          break;
        }
      } else {
        // default case
        matchingCase = caseNode;
      }
    }
    
    if (foundMatch) {
      // 一致するcaseが見つかった場合、そのcaseのブロックに置き換える
      common::Logger::Debug("switch文の条件が定数のため、一致するcaseに置き換えます");
      auto* caseClause = dynamic_cast<CaseClause*>(matchingCase.get());
      auto blockNode = parser::ast::ASTNodeFactory::CreateBlockStatement(
          node->GetLocation(), caseClause->GetConsequent());
      ReplaceCurrentNode(std::move(blockNode));
      m_statistics.m_optimizedSwitches++;
    }
  }

  // 空の case 節を削除
  auto& cases = node->GetCasesMut();
  size_t originalSize = cases.size();
  cases.erase(
      std::remove_if(cases.begin(), cases.end(),
                     [](const NodePtr& caseNode) {
                       if (!caseNode) return true;
                       auto* caseClause = dynamic_cast<CaseClause*>(caseNode.get());
                       return !caseClause || caseClause->GetConsequent().empty();
                     }),
      cases.end());

  if (cases.size() != originalSize) {
    m_statistics.m_removedStatements += (originalSize - cases.size());
  }

  // ケース節がすべてなくなった場合
  if (cases.empty()) {
    common::Logger::Debug("switch文のすべてのcaseが削除されたため、switch文自体を削除します");
    ReplaceCurrentNode(nullptr);
  }
}

void DeadCodeEliminationTransformer::VisitForStatement(ForStatement* node) {
  if (IsCurrentScopeUnreachable()) return;

  // 子ノードを訪問
  Transformer::VisitForStatement(node);

  // 条件式が常に false かチェック
  if (node->GetTest()) {
    std::optional<bool> testValue = TryEvaluateAsBoolean(node->GetTest());
    if (testValue.has_value() && !testValue.value()) {
      // ループは実行されない
      m_statistics.m_optimizedLoops++;
      NodePtr replacementNode = nullptr;
      if (node->GetInit() && HasSideEffects(node->GetInit())) {
        // 副作用のある init だけ残す
        common::Logger::Debug("for文の条件が常にfalseのため、初期化式のみ残します");
        replacementNode = parser::ast::ASTNodeFactory::CreateExpressionStatement(
            node->GetLocation(), node->GetInit());
      } else {
        // init も副作用がないか存在しない -> ループ全体を削除
        common::Logger::Debug("for文の条件が常にfalseで初期化式も副作用がないため、for文全体を削除します");
      }
      ReplaceCurrentNode(std::move(replacementNode));
    }
  } else if (node->GetBody()) {
    // for(;;) のような無限ループの場合、ループ本体に到達不能コードがあるか確認
    auto* bodyBlock = dynamic_cast<BlockStatement*>(node->GetBody().get());
    if (bodyBlock) {
      bool hasBreak = false;
      bool hasReturn = false;
      
      // ループ本体内にbreak/returnがあるか確認
      for (const auto& stmt : bodyBlock->GetBody()) {
        if (stmt->GetType() == NodeType::BREAK_STATEMENT) {
          hasBreak = true;
          break;
        } else if (stmt->GetType() == NodeType::RETURN_STATEMENT) {
          hasReturn = true;
          break;
        }
      }
      
      // 脱出手段がない無限ループの後のコードは到達不能
      if (!hasBreak && !hasReturn) {
        MarkUnreachable();
      }
    }
  }
}

void DeadCodeEliminationTransformer::VisitReturnStatement(ReturnStatement* node) {
  if (IsCurrentScopeUnreachable()) return;

  // 子ノード (argument) を訪問
  Transformer::VisitReturnStatement(node);

  // この return 文以降は到達不能になることをマーク
  MarkUnreachable();
}

void DeadCodeEliminationTransformer::VisitBreakStatement(parser::ast::BreakStatement* node) {
  if (IsCurrentScopeUnreachable()) return;
  Transformer::VisitBreakStatement(node);
  MarkUnreachable();
}

void DeadCodeEliminationTransformer::VisitContinueStatement(parser::ast::ContinueStatement* node) {
  if (IsCurrentScopeUnreachable()) return;
  Transformer::VisitContinueStatement(node);
  MarkUnreachable();
}

void DeadCodeEliminationTransformer::VisitThrowStatement(parser::ast::ThrowStatement* node) {
  if (IsCurrentScopeUnreachable()) return;
  Transformer::VisitThrowStatement(node);
  MarkUnreachable();
}

void DeadCodeEliminationTransformer::VisitExpressionStatement(ExpressionStatement* node) {
  if (IsCurrentScopeUnreachable()) {
    ReplaceCurrentNode(nullptr);
    m_statistics.m_removedStatements++;
    return;
  }

  // 子ノードを訪問
  Transformer::VisitExpressionStatement(node);

  // 副作用のない式文を削除
  if (node->GetExpression() && !HasSideEffects(node->GetExpression())) {
    common::Logger::Debug("副作用のない式文を削除します");
    ReplaceCurrentNode(nullptr);
    m_statistics.m_removedStatements++;
  }
}

void DeadCodeEliminationTransformer::VisitVariableDeclaration(parser::ast::VariableDeclaration* node) {
  if (IsCurrentScopeUnreachable()) {
    ReplaceCurrentNode(nullptr);
    m_statistics.m_removedStatements++;
    return;
  }

  // 子ノードを訪問
  Transformer::VisitVariableDeclaration(node);

  // 変数宣言を処理
  for (const auto& declarator : node->GetDeclarations()) {
    auto* varDeclarator = dynamic_cast<parser::ast::VariableDeclarator*>(declarator.get());
    if (varDeclarator && varDeclarator->GetId()) {
      auto* identifier = dynamic_cast<parser::ast::Identifier*>(varDeclarator->GetId().get());
      if (identifier) {
        // 変数を現在のスコープに登録
        DeclareVariable(identifier->GetName());
      }
    }
  }

  // 未使用変数の削除は LeaveScope() で行う
}

void DeadCodeEliminationTransformer::VisitIdentifier(parser::ast::Identifier* node) {
  if (IsCurrentScopeUnreachable()) return;

  // 変数の使用をマーク
  MarkVariableUsed(node->GetName());

  // 基底クラスの処理を呼び出す
  Transformer::VisitIdentifier(node);
}

// === ヘルパーメソッドの実装 ===

bool DeadCodeEliminationTransformer::HasSideEffects(const NodePtr& node) const {
  if (!node) return false;

  switch (node->GetType()) {
    case NodeType::LITERAL:
    case NodeType::IDENTIFIER:
    case NodeType::THIS_EXPRESSION:
    case NodeType::SUPER:
    case NodeType::ARRAY_EXPRESSION:  // 配列リテラル自体は副作用なし
    case NodeType::OBJECT_EXPRESSION: // オブジェクトリテラル自体は副作用なし
      return false;

    case NodeType::CALL_EXPRESSION:
      // 関数呼び出しは基本的に副作用があると見なす
      // 将来的には純粋関数のリストを持つことも検討
      return true;

    case NodeType::NEW_EXPRESSION:
    case NodeType::ASSIGNMENT_EXPRESSION:
    case NodeType::UPDATE_EXPRESSION:
    case NodeType::YIELD_EXPRESSION:
    case NodeType::AWAIT_EXPRESSION:
      return true;

    case NodeType::UNARY_EXPRESSION: {
      // delete演算子は副作用あり、それ以外は引数に依存
      auto* unaryExpr = dynamic_cast<parser::ast::UnaryExpression*>(node.get());
      if (unaryExpr && unaryExpr->GetOperator() == parser::ast::UnaryOperator::DELETE) {
        return true;
      }
      return HasSideEffects(unaryExpr->GetArgument());
    }

    case NodeType::BINARY_EXPRESSION: {
      // 二項演算子は両方のオペランドに依存
      auto* binaryExpr = dynamic_cast<parser::ast::BinaryExpression*>(node.get());
      return HasSideEffects(binaryExpr->GetLeft()) || HasSideEffects(binaryExpr->GetRight());
    }

    case NodeType::LOGICAL_EXPRESSION: {
      // 論理演算子は両方のオペランドに依存
      auto* logicalExpr = dynamic_cast<parser::ast::LogicalExpression*>(node.get());
      return HasSideEffects(logicalExpr->GetLeft()) || HasSideEffects(logicalExpr->GetRight());
    }

    case NodeType::CONDITIONAL_EXPRESSION: {
      // 条件演算子は全てのオペランドに依存
      auto* condExpr = dynamic_cast<parser::ast::ConditionalExpression*>(node.get());
      return HasSideEffects(condExpr->GetTest()) || 
             HasSideEffects(condExpr->GetConsequent()) || 
             HasSideEffects(condExpr->GetAlternate());
    }

    case NodeType::MEMBER_EXPRESSION: {
      // メンバー式はオブジェクトに依存
      auto* memberExpr = dynamic_cast<parser::ast::MemberExpression*>(node.get());
      return HasSideEffects(memberExpr->GetObject());
    }

    default:
      // 安全側に倒して、不明な式は副作用があると見なす
      return true;
  }
}

std::optional<bool> DeadCodeEliminationTransformer::TryEvaluateAsBoolean(const NodePtr& node) {
  if (!node) return std::nullopt;

  // リテラル値の場合
  if (auto literal = std::dynamic_pointer_cast<parser::ast::Literal>(node)) {
    switch (literal->GetLiteralType()) {
      case LiteralType::Boolean:
        return literal->GetBooleanValue();
      case LiteralType::Null:
        return false;
      case LiteralType::Number: {
        double value = literal->GetNumberValue();
        return value != 0 && !std::isnan(value);
      }
      case LiteralType::String:
        return !literal->GetStringValue().empty();
      case LiteralType::BigInt:
        // BigIntは0でなければtrue
        return literal->GetBigIntValue() != "0";
      case LiteralType::RegExp:
        // 正規表現は常にtrue
        return true;
      default:
        return std::nullopt;
    }
  }

  // 単項演算子の場合
  if (auto unaryExpr = std::dynamic_pointer_cast<parser::ast::UnaryExpression>(node)) {
    if (unaryExpr->GetOperator() == UnaryOperator::LOGICAL_NOT) {
      auto operandValue = TryEvaluateAsBoolean(unaryExpr->GetArgument());
      if (operandValue.has_value()) {
        return !operandValue.value();
      }
    } else if (unaryExpr->GetOperator() == UnaryOperator::VOID) {
      // void演算子は常にundefinedを返す
      return false;
    }
  }

  // 論理演算子の場合
  if (auto logicalExpr = std::dynamic_pointer_cast<parser::ast::LogicalExpression>(node)) {
    auto leftValue = TryEvaluateAsBoolean(logicalExpr->GetLeft());
    
    if (leftValue.has_value()) {
      if (logicalExpr->GetOperator() == parser::ast::LogicalOperator::AND) {
        // &&演算子: 左辺がfalseなら全体はfalse
        if (!leftValue.value()) return false;
        
        // 左辺がtrueなら右辺の値
        return TryEvaluateAsBoolean(logicalExpr->GetRight());
      } else if (logicalExpr->GetOperator() == parser::ast::LogicalOperator::OR) {
        // ||演算子: 左辺がtrueなら全体はtrue
        if (leftValue.value()) return true;
        
        // 左辺がfalseなら右辺の値
        return TryEvaluateAsBoolean(logicalExpr->GetRight());
      }
    }
  }

  // 二項演算子の場合
  if (auto binaryExpr = std::dynamic_pointer_cast<parser::ast::BinaryExpression>(node)) {
    auto leftValue = TryEvaluateConstant(binaryExpr->GetLeft());
    auto rightValue = TryEvaluateConstant(binaryExpr->GetRight());
    
    if (leftValue.has_value() && rightValue.has_value()) {
      // 比較演算子の評価
      switch (binaryExpr->GetOperator()) {
        case parser::ast::BinaryOperator::EQUAL:
          return ValuesEqual(*leftValue, *rightValue);
        case parser::ast::BinaryOperator::NOT_EQUAL:
          return !ValuesEqual(*leftValue, *rightValue);
        case parser::ast::BinaryOperator::STRICT_EQUAL:
          return ValuesStrictEqual(*leftValue, *rightValue);
        case parser::ast::BinaryOperator::STRICT_NOT_EQUAL:
          return !ValuesStrictEqual(*leftValue, *rightValue);
        case parser::ast::BinaryOperator::LESS_THAN:
          return ValuesLessThan(*leftValue, *rightValue);
        case parser::ast::BinaryOperator::GREATER_THAN:
          return ValuesGreaterThan(*leftValue, *rightValue);
        case parser::ast::BinaryOperator::LESS_THAN_EQUAL:
          return ValuesLessThanEqual(*leftValue, *rightValue);
        case parser::ast::BinaryOperator::GREATER_THAN_EQUAL:
          return ValuesGreaterThanEqual(*leftValue, *rightValue);
        default:
          break;
      }
    }
  }

  return std::nullopt;  // 静的に評価できない
}

std::optional<parser::ast::Value> DeadCodeEliminationTransformer::TryEvaluateConstant(const NodePtr& node) {
  if (!node) return std::nullopt;

  // リテラル値の場合
  if (auto literal = std::dynamic_pointer_cast<parser::ast::Literal>(node)) {
    parser::ast::Value value;
    
    switch (literal->GetLiteralType()) {
      case LiteralType::Boolean:
        value.type = parser::ast::ValueType::Boolean;
        value.booleanValue = literal->GetBooleanValue();
        return value;
      case LiteralType::Null:
        value.type = parser::ast::ValueType::Null;
        return value;
      case LiteralType::Number:
        value.type = parser::ast::ValueType::Number;
        value.numberValue = literal->GetNumberValue();
        return value;
      case LiteralType::String:
        value.type = parser::ast::ValueType::String;
        value.stringValue = literal->GetStringValue();
        return value;
      case LiteralType::BigInt:
        value.type = parser::ast::ValueType::BigInt;
        value.bigIntValue = literal->GetBigIntValue();
        return value;
      default:
        return std::nullopt;
    }
  }

  // 単項演算子の場合
  if (auto unaryExpr = std::dynamic_pointer_cast<parser::ast::UnaryExpression>(node)) {
    auto operandValue = TryEvaluateConstant(unaryExpr->GetArgument());
    if (operandValue.has_value()) {
      parser::ast::Value result;
      
      switch (unaryExpr->GetOperator()) {
        case UnaryOperator::PLUS:
          if (operandValue->type == parser::ast::ValueType::Number) {
            result.type = parser::ast::ValueType::Number;
            result.numberValue = operandValue->numberValue;
            return result;
          }
          break;
        case UnaryOperator::MINUS:
          if (operandValue->type == parser::ast::ValueType::Number) {
            result.type = parser::ast::ValueType::Number;
            result.numberValue = -operandValue->numberValue;
            return result;
          }
          break;
        case UnaryOperator::LOGICAL_NOT:
          result.type = parser::ast::ValueType::Boolean;
          result.booleanValue = !IsValueTruthy(*operandValue);
          return result;
        default:
          break;
      }
    }
  }

  // 二項演算子の場合
  if (auto binaryExpr = std::dynamic_pointer_cast<parser::ast::BinaryExpression>(node)) {
    auto leftValue = TryEvaluateConstant(binaryExpr->GetLeft());
    auto rightValue = TryEvaluateConstant(binaryExpr->GetRight());
    
    if (leftValue.has_value() && rightValue.has_value()) {
      parser::ast::Value result;
      
      // 数値演算
      if (leftValue->type == parser::ast::ValueType::Number && 
          rightValue->type == parser::ast::ValueType::Number) {
        result.type = parser::ast::ValueType::Number;
        
        switch (binaryExpr->GetOperator()) {
          case parser::ast::BinaryOperator::PLUS:
            result.numberValue = leftValue->numberValue + rightValue->numberValue;
            return result;
          case parser::ast::BinaryOperator::MINUS:
            result.numberValue = leftValue->numberValue - rightValue->numberValue;
            return result;
          case parser::ast::BinaryOperator::MULTIPLY:
            result.numberValue = leftValue->numberValue * rightValue->numberValue;
            return result;
          case parser::ast::BinaryOperator::DIVIDE:
            if (rightValue->numberValue != 0) {
              result.numberValue = leftValue->numberValue / rightValue->numberValue;
              return result;
            }
            break;
          case parser::ast::BinaryOperator::MODULO:
            if (rightValue->numberValue != 0) {
              result.numberValue = std::fmod(leftValue->numberValue, rightValue->numberValue);
              return result;
            }
            break;
          default:
            break;
        }
      }
      
      // 文字列連結
      if (binaryExpr->GetOperator() == parser::ast::BinaryOperator::PLUS) {
        if (leftValue->type == parser::ast::ValueType::String || 
            rightValue->type == parser::ast::ValueType::String) {
          result.type = parser::ast::ValueType::String;
          result.stringValue = ValueToString(*leftValue) + ValueToString(*rightValue);
          return result;
        }
      }
    }
  }

  return std::nullopt;  // 静的に評価できない
}

bool DeadCodeEliminationTransformer::IsValueTruthy(const parser::ast::Value& value) {
  switch (value.type) {
    case parser::ast::ValueType::Boolean:
      return value.booleanValue;
    case parser::ast::ValueType::Null:
      return false;
    case parser::ast::ValueType::Number:
      return value.numberValue != 0 && !std::isnan(value.numberValue);
    case parser::ast::ValueType::String:
      return !value.stringValue.empty();
    case parser::ast::ValueType::BigInt:
      return value.bigIntValue != "0";
    default:
      return true;  // Object, Function, Symbol は常にtruthy
  }
}

std::string DeadCodeEliminationTransformer::ValueToString(const parser::ast::Value& value) {
  switch (value.type) {
    case parser::ast::ValueType::Boolean:
      return value.booleanValue ? "true" : "false";
    case parser::ast::ValueType::Null:
      return "null";
    case parser::ast::ValueType::Number:
      return std::to_string(value.numberValue);
    case parser::ast::ValueType::String:
      return value.stringValue;
    case parser::ast::ValueType::BigInt:
      return value.bigIntValue;
    case parser::ast::ValueType::Undefined:
      return "undefined";
    case parser::ast::ValueType::Symbol:
      return "Symbol()";
    default:
      return "[object Object]";  // オブジェクト型のデフォルト文字列表現
  }
}

bool DeadCodeEliminationTransformer::ValuesEqual(const parser::ast::Value& left, const parser::ast::Value& right) {
  // 型が同じ場合は厳密等価
  if (left.type == right.type) {
    return ValuesStrictEqual(left, right);
  }
  
  // 型が異なる場合の等価比較
  // 数値と文字列
  if (left.type == parser::ast::ValueType::Number && right.type == parser::ast::ValueType::String) {
    try {
      return left.numberValue == std::stod(right.stringValue);
    } catch (...) {
      return false;
    }
  }
  if (left.type == parser::ast::ValueType::String && right.type == parser::ast::ValueType::Number) {
    try {
      return std::stod(left.stringValue) == right.numberValue;
    } catch (...) {
      return false;
    }
  }
  
  // nullとundefined
  if ((left.type == parser::ast::ValueType::Null && right.type == parser::ast::ValueType::Undefined) ||
      (left.type == parser::ast::ValueType::Undefined && right.type == parser::ast::ValueType::Null)) {
    return true;
  }
  
  // 数値とブール値
  if (left.type == parser::ast::ValueType::Number && right.type == parser::ast::ValueType::Boolean) {
    return left.numberValue == (right.booleanValue ? 1.0 : 0.0);
  }
  if (left.type == parser::ast::ValueType::Boolean && right.type == parser::ast::ValueType::Number) {
    return (left.booleanValue ? 1.0 : 0.0) == right.numberValue;
  }
  
  // 文字列とブール値
  if (left.type == parser::ast::ValueType::String && right.type == parser::ast::ValueType::Boolean) {
    if (left.stringValue.empty()) {
      return !right.booleanValue;
    }
    return right.booleanValue;
  }
  if (left.type == parser::ast::ValueType::Boolean && right.type == parser::ast::ValueType::String) {
    if (right.stringValue.empty()) {
      return !left.booleanValue;
    }
    return left.booleanValue;
  }
  
  return false;
}

bool DeadCodeEliminationTransformer::ValuesStrictEqual(const parser::ast::Value& left, const parser::ast::Value& right) {
  if (left.type != right.type) {
    return false;
  }
  
  switch (left.type) {
    case parser::ast::ValueType::Boolean:
      return left.booleanValue == right.booleanValue;
    case parser::ast::ValueType::Null:
    case parser::ast::ValueType::Undefined:
      return true;  // null同士、undefined同士は常に等しい
    case parser::ast::ValueType::Number:
      // NaNは自分自身と等しくない
      if (std::isnan(left.numberValue) || std::isnan(right.numberValue)) {
        return false;
      }
      // +0と-0は等しい
      if (left.numberValue == 0 && right.numberValue == 0) {
        return true;
      }
      return left.numberValue == right.numberValue;
    case parser::ast::ValueType::String:
      return left.stringValue == right.stringValue;
    case parser::ast::ValueType::BigInt:
      return left.bigIntValue == right.bigIntValue;
    case parser::ast::ValueType::Symbol:
      // シンボルは参照で比較
      return left.symbolId == right.symbolId;
    default:
      // オブジェクトは参照で比較（実際のJSエンジンでは参照の比較）
      return false;  // 異なるオブジェクトは常に異なる
  }
}

void DeadCodeEliminationTransformer::MarkVariableUsed(const std::string& name) {
  if (m_scopeStack.empty()) {
    m_usedGlobals.insert(name);  // グローバル変数が使用されたとマーク
    return;
  }
  
  // 現在のスコープから外側に向かって探す
  auto tempStack = m_scopeStack;
  bool found = false;
  
  while (!tempStack.empty() && !found) {
    auto& scope = tempStack.top();
    if (scope.m_declaredVars.count(name) > 0) {
      scope.m_usedVars.insert(name);
      found = true;
    }
    tempStack.pop();
  }
  
  // どのスコープにも見つからなかった場合はグローバル変数として扱う
  if (!found) {
    m_usedGlobals.insert(name);
  }
}

void DeadCodeEliminationTransformer::MarkUnreachable() {
  if (!m_scopeStack.empty()) {
    m_scopeStack.top().m_unreachableCodeDetected = true;
  }
  m_unreachableCodeDetected = true;
}

bool DeadCodeEliminationTransformer::IsCurrentScopeUnreachable() const {
  return !m_scopeStack.empty() && m_scopeStack.top().m_unreachableCodeDetected;
}

// スコープ管理のヘルパーメソッド
void DeadCodeEliminationTransformer::EnterScope() {
  m_scopeStack.push(ScopeInfo());
}

void DeadCodeEliminationTransformer::ExitScope() {
  if (!m_scopeStack.empty()) {
    m_scopeStack.pop();
  }
}

void DeadCodeEliminationTransformer::DeclareVariable(const std::string& name) {
  if (!m_scopeStack.empty()) {
    m_scopeStack.top().m_declaredVars.insert(name);
  }
}

bool DeadCodeEliminationTransformer::IsVariableUsed(const std::string& name) const {
  if (m_scopeStack.empty()) {
    return m_usedGlobals.count(name) > 0;
  }
  
  return m_scopeStack.top().m_usedVars.count(name) > 0;
}

bool DeadCodeEliminationTransformer::RemoveUnreachableCode(std::vector<NodePtr>& statements) {
  bool unreachableFound = false;
  bool changed = false;

  for (auto it = statements.begin(); it != statements.end();) {
    if (unreachableFound) {
      // 到達不能コードが見つかった場合、それ以降のコードを削除
      it = statements.erase(it);
      changed = true;
    } else {
      NodePtr& stmt = *it;

      // 一時的にフラグをリセット
      bool prevUnreachable = m_unreachableCodeDetected;
      m_unreachableCodeDetected = false;

      // ステートメントを変換
      auto result = Transform(stmt);
      if (result.changed) {
        stmt = std::move(result.node);
        changed = true;
      }

      // このステートメントが制御フローを終了するかチェック
      unreachableFound = m_unreachableCodeDetected;
      
      // 親スコープの状態を復元
      if (!unreachableFound) {
        m_unreachableCodeDetected = prevUnreachable;
      }

      ++it;
    }
  }

  return changed;
}

bool DeadCodeEliminationTransformer::RemoveNoEffectExpressions(std::vector<NodePtr>& statements) {
  bool changed = false;
  
  for (auto it = statements.begin(); it != statements.end();) {
    NodePtr& stmt = *it;
    
    // 式文（ExpressionStatement）の場合
    if (auto exprStmt = std::dynamic_pointer_cast<parser::ast::ExpressionStatement>(stmt)) {
      auto expr = exprStmt->GetExpression();
      
      // 副作用のない式を検出
      if (IsExpressionWithoutSideEffects(expr)) {
        it = statements.erase(it);
        changed = true;
        continue;
      }
    }
    
    ++it;
  }
  
  return changed;
}

bool DeadCodeEliminationTransformer::IsExpressionWithoutSideEffects(const NodePtr& expr) {
  // リテラル（数値、文字列、真偽値など）は副作用なし
  if (std::dynamic_pointer_cast<parser::ast::Literal>(expr)) {
    return true;
  }
  
  // 識別子参照は副作用なし
  if (std::dynamic_pointer_cast<parser::ast::Identifier>(expr)) {
    return true;
  }
  
  // オブジェクトリテラルや配列リテラルは、その要素に副作用がなければ副作用なし
  if (auto objExpr = std::dynamic_pointer_cast<parser::ast::ObjectExpression>(expr)) {
    for (const auto& prop : objExpr->GetProperties()) {
      if (!IsExpressionWithoutSideEffects(prop->GetValue())) {
        return false;
      }
    }
    return true;
  }
  
  if (auto arrExpr = std::dynamic_pointer_cast<parser::ast::ArrayExpression>(expr)) {
    for (const auto& element : arrExpr->GetElements()) {
      if (element && !IsExpressionWithoutSideEffects(element)) {
        return false;
      }
    }
    return true;
  }
  
  // 二項演算子は、両オペランドに副作用がなければ副作用なし
  if (auto binExpr = std::dynamic_pointer_cast<parser::ast::BinaryExpression>(expr)) {
    return IsExpressionWithoutSideEffects(binExpr->GetLeft()) && 
           IsExpressionWithoutSideEffects(binExpr->GetRight());
  }
  
  // 単項演算子は、オペランドに副作用がなければ副作用なし（delete演算子を除く）
  if (auto unaryExpr = std::dynamic_pointer_cast<parser::ast::UnaryExpression>(expr)) {
    if (unaryExpr->GetOperator() == parser::ast::UnaryOperator::DELETE) {
      return false;  // deleteは副作用あり
    }
    return IsExpressionWithoutSideEffects(unaryExpr->GetArgument());
  }
  
  // その他の式は副作用ありと仮定
  return false;
}

NodePtr DeadCodeEliminationTransformer::ReplaceCurrentNode(NodePtr node) {
  // 現在処理中のノードを置き換える
  if (m_currentParent.empty()) {
    return node;  // 親がない場合は単に新しいノードを返す
  }
  
  auto parent = m_currentParent.top();
  
  // 親ノードの種類に応じて適切に子ノードを置き換える
  if (auto blockStmt = std::dynamic_pointer_cast<parser::ast::BlockStatement>(parent)) {
    // ブロック文の場合、現在の文を置き換える
    auto& statements = blockStmt->GetBody();
    if (m_currentIndex < statements.size()) {
      statements[m_currentIndex] = node;
    }
  } else if (auto ifStmt = std::dynamic_pointer_cast<parser::ast::IfStatement>(parent)) {
    // if文の場合、条件式、then節、else節のいずれかを置き換える
    if (m_currentChildType == "test") {
      ifStmt->SetTest(std::dynamic_pointer_cast<parser::ast::Expression>(node));
    } else if (m_currentChildType == "consequent") {
      ifStmt->SetConsequent(node);
    } else if (m_currentChildType == "alternate") {
      ifStmt->SetAlternate(node);
    }
  } else if (auto forStmt = std::dynamic_pointer_cast<parser::ast::ForStatement>(parent)) {
    // for文の場合、初期化式、条件式、更新式、本体のいずれかを置き換える
    if (m_currentChildType == "init") {
      forStmt->SetInit(std::dynamic_pointer_cast<parser::ast::Expression>(node));
    } else if (m_currentChildType == "test") {
      forStmt->SetTest(std::dynamic_pointer_cast<parser::ast::Expression>(node));
    } else if (m_currentChildType == "update") {
      forStmt->SetUpdate(std::dynamic_pointer_cast<parser::ast::Expression>(node));
    } else if (m_currentChildType == "body") {
      forStmt->SetBody(node);
    }
  } else if (auto switchStmt = std::dynamic_pointer_cast<parser::ast::SwitchStatement>(parent)) {
    // switch文の場合、判別式または各caseを置き換える
    if (m_currentChildType == "discriminant") {
      switchStmt->SetDiscriminant(std::dynamic_pointer_cast<parser::ast::Expression>(node));
    } else if (m_currentChildType == "case" && m_currentIndex < switchStmt->GetCases().size()) {
      auto& cases = switchStmt->GetCasesMut();
      cases[m_currentIndex] = node;
    }
  } else if (auto returnStmt = std::dynamic_pointer_cast<parser::ast::ReturnStatement>(parent)) {
    // return文の場合、戻り値を置き換える
    if (m_currentChildType == "argument") {
      returnStmt->SetArgument(std::dynamic_pointer_cast<parser::ast::Expression>(node));
    }
  } else if (auto exprStmt = std::dynamic_pointer_cast<parser::ast::ExpressionStatement>(parent)) {
    // 式文の場合、式を置き換える
    if (m_currentChildType == "expression") {
      exprStmt->SetExpression(std::dynamic_pointer_cast<parser::ast::Expression>(node));
    }
  } else if (auto binaryExpr = std::dynamic_pointer_cast<parser::ast::BinaryExpression>(parent)) {
    // 二項演算式の場合、左辺または右辺を置き換える
    if (m_currentChildType == "left") {
      binaryExpr->SetLeft(std::dynamic_pointer_cast<parser::ast::Expression>(node));
    } else if (m_currentChildType == "right") {
      binaryExpr->SetRight(std::dynamic_pointer_cast<parser::ast::Expression>(node));
    }
  } else if (auto unaryExpr = std::dynamic_pointer_cast<parser::ast::UnaryExpression>(parent)) {
    // 単項演算式の場合、引数を置き換える
    if (m_currentChildType == "argument") {
      unaryExpr->SetArgument(std::dynamic_pointer_cast<parser::ast::Expression>(node));
    }
  } else if (auto callExpr = std::dynamic_pointer_cast<parser::ast::CallExpression>(parent)) {
    // 関数呼び出し式の場合、呼び出し対象または引数を置き換える
    if (m_currentChildType == "callee") {
      callExpr->SetCallee(std::dynamic_pointer_cast<parser::ast::Expression>(node));
    } else if (m_currentChildType == "argument" && m_currentIndex < callExpr->GetArguments().size()) {
      auto& args = callExpr->GetArgumentsMut();
      args[m_currentIndex] = std::dynamic_pointer_cast<parser::ast::Expression>(node);
    }
  } else if (auto varDecl = std::dynamic_pointer_cast<parser::ast::VariableDeclaration>(parent)) {
    // 変数宣言の場合、各宣言子を置き換える
    if (m_currentChildType == "declarator" && m_currentIndex < varDecl->GetDeclarations().size()) {
      auto& decls = varDecl->GetDeclarationsMut();
      decls[m_currentIndex] = std::dynamic_pointer_cast<parser::ast::VariableDeclarator>(node);
    }
  } else if (auto varDeclr = std::dynamic_pointer_cast<parser::ast::VariableDeclarator>(parent)) {
    // 変数宣言子の場合、初期化式を置き換える
    if (m_currentChildType == "init") {
      varDeclr->SetInit(std::dynamic_pointer_cast<parser::ast::Expression>(node));
    }
  }
  
  // 変更が行われたことを記録
  m_statistics.m_transformedNodes++;
  
  return node;
}

}  // namespace aerojs::core::transformers