/*******************************************************************************
 * @file src/core/transformers/inline_functions.cpp
 * @version 1.0.0
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 * @brief 関数インライン化を行うトランスフォーマーの実装ファイル。
 ******************************************************************************/

#include "src/core/transformers/inline_functions.h"  // 対応ヘッダーを最初にインクルード

#include <algorithm>  // std::find_if など (副作用チェックで使用)
#include <cassert>    // assert (デバッグビルド用)
#include <memory>     // std::dynamic_pointer_cast, std::make_shared など
#include <sstream>    // std::ostringstream (一意な名前生成で使用)
#include <stack>      // スコープスタックとして使用 (ただし現在は vector)
#include <utility>    // std::move

// AST関連
#include "src/core/common/logger.h"                // ロギング用
#include "src/core/parser/ast/ast_node_factory.h"  // ノード生成用
#include "src/core/parser/ast/ast_node_types.h"
#include "src/core/parser/ast/nodes/all_nodes.h"       // AST ノードクラス
#include "src/core/parser/ast/visitor/ast_cloner.h"    // ノードのディープコピー用
#include "src/core/parser/ast/visitor/ast_replacer.h"  // ノード置換用 (インライン化で使用)

namespace aerojs::core::transformers {

// 必要な型のみ using 宣言
using aerojs::parser::ast::ArrowFunctionExpression;
using aerojs::parser::ast::BlockStatement;
using aerojs::parser::ast::CallExpression;
using aerojs::parser::ast::Expression;
using aerojs::parser::ast::FunctionDeclaration;
using aerojs::parser::ast::FunctionExpression;
using aerojs::parser::ast::Identifier;
using aerojs::parser::ast::Node;
using aerojs::parser::ast::NodePtr;
using aerojs::parser::ast::NodeType;
using aerojs::parser::ast::Program;
using aerojs::parser::ast::ReturnStatement;
using aerojs::parser::ast::Statement;
using aerojs::parser::ast::VariableDeclaration;
using aerojs::parser::ast::VariableDeclarator;
using aerojs::parser::ast::visitor::ASTCloner;
using aerojs::parser::ast::visitor::ASTReplacer;
using aerojs::parser::ast::ASTNodeFactory;

// === コンストラクタ ===

InlineFunctionsTransformer::InlineFunctionsTransformer(
    size_t maxInlineSize,
    size_t maxRecursionDepth,
    bool enableStatistics)
    : Transformer("InlineFunctionsTransformer", "Performs function inlining optimization.")  // 基底クラスコンストラクタ呼び出し
      ,
      m_maxInlineSize(maxInlineSize),
      m_maxRecursionDepth(maxRecursionDepth),
      m_currentRecursionDepth(0),
      m_statisticsEnabled(enableStatistics),
      m_inlinedFunctionsCount(0),
      m_inlinedCallsCount(0),
      m_visitedNodesCount(0),
      m_nextUniqueId(0) {
  // 基底クラスのコンストラクタで Visitor の設定などが行われる想定
}

// === 状態リセット・統計 ===

void InlineFunctionsTransformer::Reset() {
  m_functionMap.clear();
  m_anonymousFunctions.clear();
  m_scopeStack.clear();
  m_currentRecursionDepth = 0;
  m_inlinedFunctionsCount = 0;
  m_inlinedCallsCount = 0;
  m_visitedNodesCount = 0;
  m_nextUniqueId = 0;
  // 基底クラスのリセットも呼び出す
  Transformer::Reset();
}

void InlineFunctionsTransformer::EnableStatistics(bool enable) {
  m_statisticsEnabled = enable;
}

size_t InlineFunctionsTransformer::GetInlinedFunctionsCount() const noexcept {
  return m_inlinedFunctionsCount;
}

size_t InlineFunctionsTransformer::GetInlinedCallsCount() const noexcept {
  return m_inlinedCallsCount;
}

size_t InlineFunctionsTransformer::GetVisitedNodesCount() const noexcept {
  return m_visitedNodesCount;
}

// === AST Visitor メソッドの実装 ===

void InlineFunctionsTransformer::VisitProgram(Program* node) {
  if (m_statisticsEnabled) {
    m_visitedNodesCount++;
  }

  // グローバルスコープを開始
  EnterScope();

  // 1st パス: 関数宣言を収集し、情報を登録
  for (const auto& stmt : node->GetBody()) {
    if (auto* funcDecl = dynamic_cast<FunctionDeclaration*>(stmt.get())) {
      // 関数名をグローバルスコープに登録
      if (funcDecl->GetId()) {
        DeclareVariableInCurrentScope(funcDecl->GetId()->GetName());
        CollectFunctionInfo(stmt, funcDecl->GetId()->GetName());
      }
    } else if (auto* varDecl = dynamic_cast<VariableDeclaration*>(stmt.get())) {
      // 変数宣言内の関数式も収集
      for (const auto& declarator : varDecl->GetDeclarations()) {
        if (auto* funcExpr = dynamic_cast<FunctionExpression*>(declarator->GetInit().get())) {
          if (auto* id = dynamic_cast<Identifier*>(declarator->GetId().get())) {
            DeclareVariableInCurrentScope(id->GetName());
            CollectFunctionInfo(funcExpr->shared_from_this(), id->GetName());
          }
        }
      }
    }
  }

  // 再帰フラグとインライン化可能性を更新
  for (auto& [name, info] : m_functionMap) {
    info.m_isRecursive = IsRecursiveFunction(info);
    info.m_hasMultipleReturns = CheckForMultipleReturns(info.m_body);
    info.m_isEligibleForInlining = !info.m_hasMultipleReturns && 
                                   info.m_size <= m_maxInlineSize && 
                                   !info.m_hasSideEffects;
  }

  // 2nd パス: ステートメントを変換 (インライン化を実行)
  Transformer::VisitProgram(node);

  // グローバルスコープを終了
  LeaveScope();
}

void InlineFunctionsTransformer::VisitFunctionDeclaration(FunctionDeclaration* node) {
  if (m_statisticsEnabled) {
    m_visitedNodesCount++;
  }

  // 関数スコープを開始
  EnterScope();

  // パラメータを現在のスコープに登録
  for (const auto& param : node->GetParams()) {
    if (auto* identifier = dynamic_cast<Identifier*>(param.get())) {
      DeclareVariableInCurrentScope(identifier->GetName());
    } else {
      // パターン (ArrayPattern, ObjectPattern) の場合
      RegisterPatternVariables(param);
    }
  }
  // 関数名自体もスコープに追加 (再帰呼び出しのため)
  if (node->GetId()) {
    DeclareVariableInCurrentScope(node->GetId()->GetName());
  }

  // 関数本体を訪問
  Transformer::VisitFunctionDeclaration(node);

  // 関数スコープを終了
  LeaveScope();
}

void InlineFunctionsTransformer::VisitFunctionExpression(FunctionExpression* node) {
  if (m_statisticsEnabled) {
    m_visitedNodesCount++;
  }

  // 関数式を収集
  std::string name = node->GetId() ? node->GetId()->GetName() : "";
  CollectFunctionInfo(node->shared_from_this(), name);

  // 関数スコープを開始
  EnterScope();

  // パラメータと関数名 (あれば) をスコープに追加
  if (!name.empty()) {
    DeclareVariableInCurrentScope(name);
  }
  for (const auto& param : node->GetParams()) {
    if (auto* identifier = dynamic_cast<Identifier*>(param.get())) {
      DeclareVariableInCurrentScope(identifier->GetName());
    } else {
      // パターン (ArrayPattern, ObjectPattern) の場合
      RegisterPatternVariables(param);
    }
  }

  // 関数本体を訪問
  Transformer::VisitFunctionExpression(node);

  // 関数スコープを終了
  LeaveScope();
}

void InlineFunctionsTransformer::VisitArrowFunctionExpression(ArrowFunctionExpression* node) {
  if (m_statisticsEnabled) {
    m_visitedNodesCount++;
  }

  // アロー関数を収集
  CollectFunctionInfo(node->shared_from_this(), "");  // アロー関数は常に匿名

  // 関数スコープを開始
  EnterScope();

  // パラメータをスコープに追加
  for (const auto& param : node->GetParams()) {
    if (auto* identifier = dynamic_cast<Identifier*>(param.get())) {
      DeclareVariableInCurrentScope(identifier->GetName());
    } else {
      // パターン (ArrayPattern, ObjectPattern) の場合
      RegisterPatternVariables(param);
    }
  }

  // 関数本体を訪問
  Transformer::VisitArrowFunctionExpression(node);

  // 関数スコープを終了
  LeaveScope();
}

void InlineFunctionsTransformer::VisitCallExpression(CallExpression* node) {
  if (m_statisticsEnabled) {
    m_visitedNodesCount++;
  }

  // まず子ノード (callee, arguments) を訪問する
  Transformer::VisitCallExpression(node);

  // インライン化の試行
  // 呼び出し対象 (callee) が単純な識別子の場合のみ考慮
  if (auto* calleeIdent = dynamic_cast<Identifier*>(node->GetCallee().get())) {
    const std::string& funcName = calleeIdent->GetName();

    // 関数マップに関数情報が存在するか確認
    auto it = m_functionMap.find(funcName);
    if (it != m_functionMap.end()) {
      const FunctionInfo& funcInfo = it->second;

      // インライン化可能かチェック
      if (IsFunctionInlinable(funcInfo, node)) {
        // 再帰深度チェック
        if (m_currentRecursionDepth < m_maxRecursionDepth) {
          m_currentRecursionDepth++;  // 再帰深度をインクリメント

          // インライン化を実行
          NodePtr inlinedNode = InlineCall(node, funcInfo);

          m_currentRecursionDepth--;  // 再帰深度をデクリメント

          if (inlinedNode) {
            // インライン化成功
            if (m_statisticsEnabled) {
              m_inlinedCallsCount++;
            }
            common::Logger::Debug("関数 '{}' の呼び出しをインライン化しました", funcName);

            // 現在処理中のノードをインライン化後のノードで置き換える
            ReplaceCurrentNode(std::move(inlinedNode));
            m_changed = true;  // 変更フラグを立てる
            return;
          }
        } else {
          common::Logger::Debug("関数 '{}' の最大再帰深度に達しました", funcName);
        }
      } else {
        common::Logger::Debug("関数 '{}' はこの呼び出し箇所ではインライン化できません", funcName);
      }
    } else {
      // 匿名関数/関数式のインライン化を試みる
      for (const auto& anonFunc : m_anonymousFunctions) {
        if (IsFunctionInlinable(anonFunc, node)) {
          NodePtr inlinedNode = InlineCall(node, anonFunc);
          if (inlinedNode) {
            if (m_statisticsEnabled) {
              m_inlinedCallsCount++;
            }
            common::Logger::Debug("匿名関数の呼び出しをインライン化しました");
            ReplaceCurrentNode(std::move(inlinedNode));
            m_changed = true;
            return;
          }
        }
      }
    }
  }
}

// === ヘルパーメソッド ===

void InlineFunctionsTransformer::CollectFunctionInfo(NodePtr funcNode, const std::string& name) {
  FunctionInfo info;
  info.m_functionNode = funcNode;
  info.m_name = name;

  if (auto* funcDecl = dynamic_cast<FunctionDeclaration*>(funcNode.get())) {
    info.m_body = funcDecl->GetBody();
    for (const auto& param : funcDecl->GetParams()) info.m_parameters.push_back(param);
  } else if (auto* funcExpr = dynamic_cast<FunctionExpression*>(funcNode.get())) {
    info.m_body = funcExpr->GetBody();
    for (const auto& param : funcExpr->GetParams()) info.m_parameters.push_back(param);
  } else if (auto* arrowFunc = dynamic_cast<ArrowFunctionExpression*>(funcNode.get())) {
    info.m_body = arrowFunc->GetBody();
    for (const auto& param : arrowFunc->GetParams()) info.m_parameters.push_back(param);
  } else {
    common::Logger::Warning("CollectFunctionInfo: サポートされていない関数ノードタイプです");
    return;
  }

  if (!info.m_body) return;  // 本体がない場合は無視

  info.m_size = CalculateFunctionSize(info.m_body);
  info.m_hasSideEffects = CheckForSideEffects(info.m_body);
  info.m_hasMultipleReturns = CheckForMultipleReturns(info.m_body);
  info.m_isRecursive = false;  // IsRecursiveFunction で後で設定
  info.m_isEligibleForInlining = (info.m_size <= m_maxInlineSize && !info.m_hasSideEffects && !info.m_hasMultipleReturns);

  if (!name.empty()) {
    // 名前付き関数の場合、マップに登録 (または更新)
    m_functionMap[name] = std::move(info);
    if (m_statisticsEnabled) {
      m_inlinedFunctionsCount++;
    }
  } else {
    // 匿名関数の場合
    m_anonymousFunctions.push_back(std::move(info));
  }
}

void InlineFunctionsTransformer::EnterScope() {
  m_scopeStack.push_back({});
}

void InlineFunctionsTransformer::LeaveScope() {
  if (!m_scopeStack.empty()) {
    m_scopeStack.pop_back();
  }
}

void InlineFunctionsTransformer::DeclareVariableInCurrentScope(const std::string& name) {
  if (!m_scopeStack.empty()) {
    m_scopeStack.back().insert(name);
  }
}

void InlineFunctionsTransformer::RegisterPatternVariables(const NodePtr& pattern) {
  // パターン内の識別子をすべて登録する
  class PatternVisitor : public parser::ast::visitor::ASTVisitor {
  public:
    PatternVisitor(InlineFunctionsTransformer& transformer) : m_transformer(transformer) {}

    void VisitIdentifier(Identifier* node) override {
      m_transformer.DeclareVariableInCurrentScope(node->GetName());
    }

  private:
    InlineFunctionsTransformer& m_transformer;
  };

  PatternVisitor visitor(*this);
  pattern->Accept(visitor);
}

bool InlineFunctionsTransformer::IsFunctionInlinable(
    const FunctionInfo& funcInfo,
    const CallExpression* callNode) const {
  // 基本的な条件チェック
  if (!funcInfo.m_isEligibleForInlining) return false;
  if (funcInfo.m_hasSideEffects) return false;
  if (funcInfo.m_hasMultipleReturns) return false;
  if (funcInfo.m_isRecursive && m_currentRecursionDepth >= m_maxRecursionDepth) return false;

  // 引数の数チェック
  if (callNode && callNode->GetArguments().size() != funcInfo.m_parameters.size()) {
    return false;
  }

  // 引数の副作用チェック
  if (callNode) {
    for (const auto& arg : callNode->GetArguments()) {
      if (CheckForSideEffects(arg)) {
        return false;  // 引数に副作用がある場合はインライン化を避ける
      }
    }
  }

  // スコープ競合チェック
  // 関数内で使用される変数と現在のスコープの変数が衝突しないか確認
  class VariableCollector : public parser::ast::visitor::ASTVisitor {
  public:
    std::set<std::string> variables;

    void VisitIdentifier(Identifier* node) override {
      variables.insert(node->GetName());
    }
  };

  VariableCollector collector;
  if (funcInfo.m_body) {
    funcInfo.m_body->Accept(collector);
  }

  // 現在のスコープスタック内の変数と衝突するか確認
  for (const auto& varName : collector.variables) {
    if (IsIdentifierInScope(varName)) {
      // 衝突する場合でも、一意な名前に変更できるので問題ない
      // ただし、変数名の変更が必要なことを記録しておく
    }
  }

  return true;
}

NodePtr InlineFunctionsTransformer::InlineCall(
    CallExpression* callExpr,
    const FunctionInfo& funcInfo) {
  // 1. 関数本体をディープコピー
  ASTCloner cloner;
  NodePtr bodyCopy;
  
  if (auto* blockStmt = dynamic_cast<BlockStatement*>(funcInfo.m_body.get())) {
    bodyCopy = cloner.Clone(blockStmt);
  } else {
    // アロー関数の場合、本体が式の場合がある
    // その場合は式を return 文でラップする
    auto returnStmt = std::make_shared<ReturnStatement>();
    returnStmt->SetArgument(cloner.Clone(funcInfo.m_body));
    
    auto blockStmt = std::make_shared<BlockStatement>();
    std::vector<NodePtr> statements;
    statements.push_back(returnStmt);
    blockStmt->SetBody(std::move(statements));
    
    bodyCopy = blockStmt;
  }
  
  auto* blockBody = dynamic_cast<BlockStatement*>(bodyCopy.get());
  if (!blockBody) {
    common::Logger::Error("インライン化中にブロック文への変換に失敗しました");
    return nullptr;
  }

  // 2. 引数の置き換え
  std::map<std::string, NodePtr> paramMap;
  for (size_t i = 0; i < funcInfo.m_parameters.size(); ++i) {
    if (i < callExpr->GetArguments().size()) {
      if (auto* paramId = dynamic_cast<Identifier*>(funcInfo.m_parameters[i].get())) {
        // 単純な識別子パラメータの場合
        paramMap[paramId->GetName()] = cloner.Clone(callExpr->GetArguments()[i]);
      } else {
        // パターンパラメータの場合は複雑なので、現時点ではインライン化を避ける
        common::Logger::Warning("パターンパラメータを持つ関数のインライン化はサポートされていません");
        return nullptr;
      }
    }
  }

  // 3. 変数名の衝突を解決
  std::map<std::string, std::string> renameMap;
  class VariableRenamer : public parser::ast::visitor::ASTVisitor {
  public:
    VariableRenamer(
        InlineFunctionsTransformer& transformer,
        std::map<std::string, std::string>& renameMap)
        : m_transformer(transformer), m_renameMap(renameMap) {}

    void VisitIdentifier(Identifier* node) override {
      const std::string& name = node->GetName();
      if (m_transformer.IsIdentifierInScope(name) && m_renameMap.find(name) == m_renameMap.end()) {
        // 衝突する変数名を一意な名前に変更
        m_renameMap[name] = m_transformer.GenerateUniqueVarName(name);
      }
    }

  private:
    InlineFunctionsTransformer& m_transformer;
    std::map<std::string, std::string>& m_renameMap;
  };

  VariableRenamer renamer(*this, renameMap);
  bodyCopy->Accept(renamer);

  // 変数名の置き換えを実行
  for (const auto& [oldName, newName] : renameMap) {
    class IdentifierReplacer : public ASTReplacer {
    public:
      IdentifierReplacer(const std::string& oldName, const std::string& newName)
          : m_oldName(oldName), m_newName(newName) {}

      NodePtr ReplaceIdentifier(Identifier* node) override {
        if (node->GetName() == m_oldName) {
          auto newId = std::make_shared<Identifier>();
          newId->SetName(m_newName);
          return newId;
        }
        return nullptr;
      }

    private:
      std::string m_oldName;
      std::string m_newName;
    };

    IdentifierReplacer replacer(oldName, newName);
    bodyCopy = replacer.Replace(bodyCopy);
  }

  // 4. パラメータを実引数で置き換え
  for (const auto& [paramName, argNode] : paramMap) {
    class ParamReplacer : public ASTReplacer {
    public:
      ParamReplacer(const std::string& paramName, const NodePtr& argNode)
          : m_paramName(paramName), m_argNode(argNode) {}

      NodePtr ReplaceIdentifier(Identifier* node) override {
        if (node->GetName() == m_paramName) {
          return m_argNode->Clone();
        }
        return nullptr;
      }

    private:
      std::string m_paramName;
      NodePtr m_argNode;
    };

    ParamReplacer replacer(paramName, argNode);
    bodyCopy = replacer.Replace(bodyCopy);
  }

  // 5. return 文を適切な式に変換
  // 単一の return 文を持つ関数の場合、return の式を直接返す
  auto* blockStmt = dynamic_cast<BlockStatement*>(bodyCopy.get());
  if (blockStmt && blockStmt->GetBody().size() == 1) {
    if (auto* returnStmt = dynamic_cast<ReturnStatement*>(blockStmt->GetBody()[0].get())) {
      return returnStmt->GetArgument();
    }
  }

  // 複数のステートメントを持つ場合は、IIFE (即時実行関数式) を使用
  // (function() { ... インライン化された本体 ... })()
  auto factory = ASTNodeFactory::GetInstance();
  auto iife = factory->CreateImmediatelyInvokedFunctionExpression(bodyCopy);
  
  return iife;
}

size_t InlineFunctionsTransformer::CalculateFunctionSize(const NodePtr& node) const {
  if (!node) return 0;
  
  // ノード数をカウントする訪問者
  class NodeCounter : public parser::ast::visitor::ASTVisitor {
  public:
    size_t count = 0;
    
    void Visit(Node* node) override {
      count++;
      ASTVisitor::Visit(node);
    }
  };
  
  NodeCounter counter;
  node->Accept(counter);
  return counter.count;
}

bool InlineFunctionsTransformer::CheckForSideEffects(const NodePtr& node) const {
  if (!node) return false;
  
  // 副作用をチェックする訪問者
  class SideEffectChecker : public parser::ast::visitor::ASTVisitor {
  public:
    bool hasSideEffects = false;
    
    void Visit(Node* node) override {
      if (hasSideEffects) return;  // 既に副作用が見つかっていれば早期リターン
      
      // 副作用を持つ可能性のある構文を検出
      switch (node->GetType()) {
        case NodeType::AssignmentExpression:
        case NodeType::UpdateExpression:  // ++, --
        case NodeType::AwaitExpression:
        case NodeType::YieldExpression:
        case NodeType::ThrowStatement:
          hasSideEffects = true;
          return;
          
        case NodeType::CallExpression:
          // 関数呼び出しは副作用を持つ可能性が高い
          // 純粋関数のホワイトリストがあれば例外にできる
          hasSideEffects = true;
          return;
          
        default:
          break;
      }
      
      ASTVisitor::Visit(node);
    }
  };
  
  SideEffectChecker checker;
  node->Accept(checker);
  return checker.hasSideEffects;
}

bool InlineFunctionsTransformer::CheckForMultipleReturns(const NodePtr& node) const {
  if (!node) return false;
  
  // 複数のreturn文をチェックする訪問者
  class MultipleReturnChecker : public parser::ast::visitor::ASTVisitor {
  public:
    int returnCount = 0;
    
    void VisitReturnStatement(ReturnStatement* node) override {
      returnCount++;
      ASTVisitor::VisitReturnStatement(node);
    }
  };
  
  MultipleReturnChecker checker;
  node->Accept(checker);
  return checker.returnCount > 1;
}

bool InlineFunctionsTransformer::IsRecursiveFunction(const FunctionInfo& funcInfo) const {
  if (funcInfo.m_name.empty()) return false;  // 匿名関数は再帰的でない
  
  // 再帰呼び出しをチェックする訪問者
  class RecursionChecker : public parser::ast::visitor::ASTVisitor {
  public:
    RecursionChecker(const std::string& funcName) : m_funcName(funcName), m_isRecursive(false) {}
    
    bool IsRecursive() const { return m_isRecursive; }
    
    void VisitCallExpression(CallExpression* node) override {
      if (m_isRecursive) return;  // 既に再帰が見つかっていれば早期リターン
      
      if (auto* callee = dynamic_cast<Identifier*>(node->GetCallee().get())) {
        if (callee->GetName() == m_funcName) {
          m_isRecursive = true;
          return;
        }
      }
      
      ASTVisitor::VisitCallExpression(node);
    }
    
  private:
    std::string m_funcName;
    bool m_isRecursive;
  };
  
  RecursionChecker checker(funcInfo.m_name);
  if (funcInfo.m_body) {
    funcInfo.m_body->Accept(checker);
  }
  
  return checker.IsRecursive();
}

bool InlineFunctionsTransformer::IsIdentifierInScope(const std::string& name) const {
  for (auto it = m_scopeStack.rbegin(); it != m_scopeStack.rend(); ++it) {
    if (it->count(name)) {
      return true;
    }
  }
  return false;
}

std::string InlineFunctionsTransformer::GenerateUniqueVarName(const std::string& baseName) {
  std::string newName;
  do {
    std::ostringstream oss;
    oss << baseName << "$" << m_nextUniqueId++;
    newName = oss.str();
  } while (IsIdentifierInScope(newName));
  return newName;
}

}  // namespace aerojs::core::transformers