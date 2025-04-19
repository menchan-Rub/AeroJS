/**
 * @file inline_functions.cpp
 * @version 1.0.0
 * @copyright Copyright (c) 2023 AeroJS Project
 * @license MIT License
 * @brief 関数インライン化を行うトランスフォーマーの実装
 */

#include "inline_functions.h"
#include "core/ast/visitor.h"
#include "core/ast/node_factory.h"
#include "core/ast/ast_node_types.h"
#include <algorithm>
#include <sstream>
#include <stack>
#include <cassert>

namespace aero {
namespace transformers {

using namespace ast;

// コンストラクタ
InlineFunctionsTransformer::InlineFunctionsTransformer(
    size_t maxInlineSize,
    size_t maxRecursionDepth,
    bool enableStatistics
)
    : Transformer("InlineFunctions", "関数インライン化を行うトランスフォーマー")
    , m_maxInlineSize(maxInlineSize)
    , m_maxRecursionDepth(maxRecursionDepth)
    , m_currentRecursionDepth(0)
    , m_statisticsEnabled(enableStatistics)
    , m_inlinedFunctionsCount(0)
    , m_inlinedCallsCount(0)
    , m_visitedNodesCount(0)
    , m_nextUniqueId(0)
{
}

void InlineFunctionsTransformer::reset() {
    m_functionMap.clear();
    m_anonymousFunctions.clear();
    m_scopeStack.clear();
    m_currentRecursionDepth = 0;
    m_inlinedFunctionsCount = 0;
    m_inlinedCallsCount = 0;
    m_visitedNodesCount = 0;
    m_nextUniqueId = 0;
}

void InlineFunctionsTransformer::enableStatistics(bool enable) {
    m_statisticsEnabled = enable;
}

size_t InlineFunctionsTransformer::getInlinedFunctionsCount() const {
    return m_inlinedFunctionsCount;
}

size_t InlineFunctionsTransformer::getInlinedCallsCount() const {
    return m_inlinedCallsCount;
}

size_t InlineFunctionsTransformer::getVisitedNodesCount() const {
    return m_visitedNodesCount;
}

// AST訪問メソッド

void InlineFunctionsTransformer::visitProgram(Program* node) {
    if (m_statisticsEnabled) {
        m_visitedNodesCount++;
    }
    
    // トップレベルスコープを作成
    m_scopeStack.push_back({});
    
    // まず関数宣言を収集
    for (auto& stmt : node->body) {
        if (auto* funcDecl = dynamic_cast<FunctionDeclaration*>(stmt.get())) {
            FunctionInfo info;
            info.functionNode = stmt;
            info.name = funcDecl->id->name;
            info.size = calculateFunctionSize(stmt);
            info.hasSideEffects = checkForSideEffects(stmt);
            
            // パラメータと本体を保存
            for (auto& param : funcDecl->params) {
                info.parameters.push_back(param);
            }
            info.body = funcDecl->body;
            
            // 仮に非再帰としてマークし、後で更新
            info.isRecursive = false;
            info.hasMultipleReturns = false;  // TODO: 実際にはreturn文をカウントする必要がある
            
            // インライン化可能かどうかを判断（後で更新される）
            info.isEligibleForInlining = false;
            
            // 関数マップに追加
            m_functionMap[info.name] = info;
            
            // スコープに関数名を追加
            m_scopeStack.back().insert(info.name);
        }
    }
    
    // 再帰関数をマーク
    for (auto& [name, info] : m_functionMap) {
        info.isRecursive = isRecursiveFunction(info);
        info.isEligibleForInlining = isFunctionInlinable(info);
    }
    
    // すべてのステートメントを変換
    bool localChanged = false;
    for (size_t i = 0; i < node->body.size(); ++i) {
        auto result = transformNode(node->body[i]);
        if (result.changed) {
            localChanged = true;
            node->body[i] = std::move(result.node);
        }
    }
    
    // スコープをポップ
    m_scopeStack.pop_back();
    
    m_changed = localChanged;
}

void InlineFunctionsTransformer::visitFunctionDeclaration(FunctionDeclaration* node) {
    if (m_statisticsEnabled) {
        m_visitedNodesCount++;
    }
    
    // 関数スコープを作成
    m_scopeStack.push_back({});
    
    // パラメータ名をスコープに追加
    for (auto& param : node->params) {
        if (auto* identifier = dynamic_cast<Identifier*>(param.get())) {
            m_scopeStack.back().insert(identifier->name);
        }
    }
    
    // 関数本体を変換
    auto bodyResult = transformNode(node->body);
    if (bodyResult.changed) {
        m_changed = true;
        node->body = std::dynamic_pointer_cast<BlockStatement>(bodyResult.node);
    }
    
    // スコープをポップ
    m_scopeStack.pop_back();
}

void InlineFunctionsTransformer::visitFunctionExpression(FunctionExpression* node) {
    if (m_statisticsEnabled) {
        m_visitedNodesCount++;
    }
    
    // 匿名関数情報を作成
    FunctionInfo info;
    info.functionNode = nullptr;  // 保持する必要はない
    info.name = node->id ? node->id->name : "";
    info.size = calculateFunctionSize(NodePtr(node));
    info.hasSideEffects = checkForSideEffects(NodePtr(node));
    
    // パラメータと本体を保存
    for (auto& param : node->params) {
        info.parameters.push_back(param);
    }
    info.body = node->body;
    
    // その他の情報を設定
    info.isRecursive = !info.name.empty() && isRecursiveFunction(info);
    info.hasMultipleReturns = false;  // TODO: 実際にはreturn文をカウントする必要がある
    info.isEligibleForInlining = isFunctionInlinable(info);
    
    // 匿名関数リストに追加
    if (info.isEligibleForInlining) {
        m_anonymousFunctions.push_back(info);
    }
    
    // 関数スコープを作成
    m_scopeStack.push_back({});
    
    // 関数名が存在すれば自身の名前をスコープに追加（再帰用）
    if (node->id) {
        m_scopeStack.back().insert(node->id->name);
    }
    
    // パラメータ名をスコープに追加
    for (auto& param : node->params) {
        if (auto* identifier = dynamic_cast<Identifier*>(param.get())) {
            m_scopeStack.back().insert(identifier->name);
        }
    }
    
    // 関数本体を変換
    auto bodyResult = transformNode(node->body);
    if (bodyResult.changed) {
        m_changed = true;
        node->body = std::dynamic_pointer_cast<BlockStatement>(bodyResult.node);
    }
    
    // スコープをポップ
    m_scopeStack.pop_back();
}

void InlineFunctionsTransformer::visitArrowFunctionExpression(ArrowFunctionExpression* node) {
    if (m_statisticsEnabled) {
        m_visitedNodesCount++;
    }
    
    // アロー関数情報を作成
    FunctionInfo info;
    info.functionNode = nullptr;  // 保持する必要はない
    info.name = "";  // アロー関数は名前を持たない
    info.size = calculateFunctionSize(NodePtr(node));
    info.hasSideEffects = checkForSideEffects(NodePtr(node));
    
    // パラメータと本体を保存
    for (auto& param : node->params) {
        info.parameters.push_back(param);
    }
    info.body = node->body;
    
    // その他の情報を設定
    info.isRecursive = false;  // アロー関数は名前を持たないので直接的な再帰はない
    info.hasMultipleReturns = false;  // TODO: 実際にはreturn文をカウントする必要がある
    info.isEligibleForInlining = isFunctionInlinable(info);
    
    // 匿名関数リストに追加
    if (info.isEligibleForInlining) {
        m_anonymousFunctions.push_back(info);
    }
    
    // 関数スコープを作成
    m_scopeStack.push_back({});
    
    // パラメータ名をスコープに追加
    for (auto& param : node->params) {
        if (auto* identifier = dynamic_cast<Identifier*>(param.get())) {
            m_scopeStack.back().insert(identifier->name);
        }
    }
    
    // 関数本体を変換
    if (node->body && node->expression) {
        // 式本体の場合
        auto exprResult = transformNode(node->body);
        if (exprResult.changed) {
            m_changed = true;
            node->body = exprResult.node;
        }
    } else if (node->body) {
        // ブロック本体の場合
        auto bodyResult = transformNode(node->body);
        if (bodyResult.changed) {
            m_changed = true;
            node->body = bodyResult.node;
        }
    }
    
    // スコープをポップ
    m_scopeStack.pop_back();
}

void InlineFunctionsTransformer::visitCallExpression(CallExpression* node) {
    if (m_statisticsEnabled) {
        m_visitedNodesCount++;
    }
    
    // まずは引数を変換
    bool argsChanged = false;
    for (size_t i = 0; i < node->arguments.size(); ++i) {
        auto argResult = transformNode(node->arguments[i]);
        if (argResult.changed) {
            argsChanged = true;
            node->arguments[i] = argResult.node;
        }
    }
    
    // 名前付き関数の呼び出しかどうかをチェック
    std::string funcName;
    if (auto* callee = dynamic_cast<Identifier*>(node->callee.get())) {
        funcName = callee->name;
        
        // 関数マップに存在するかチェック
        auto it = m_functionMap.find(funcName);
        if (it != m_functionMap.end() && it->second.isEligibleForInlining) {
            // 再帰的な呼び出しの場合は深度をチェック
            if (it->second.isRecursive) {
                if (m_currentRecursionDepth >= m_maxRecursionDepth) {
                    m_changed = argsChanged;
                    return;  // 最大再帰深度に達した場合はインライン化しない
                }
                m_currentRecursionDepth++;
            }
            
            // 関数呼び出しをインライン化
            NodePtr inlined = inlineCall(node, it->second);
            if (inlined) {
                m_result = inlined;
                m_changed = true;
                
                if (m_statisticsEnabled) {
                    m_inlinedCallsCount++;
                }
                
                // 再帰カウンタをデクリメント
                if (it->second.isRecursive) {
                    m_currentRecursionDepth--;
                }
                
                return;
            }
            
            // 再帰カウンタをデクリメント
            if (it->second.isRecursive) {
                m_currentRecursionDepth--;
            }
        }
    }
    
    // 匿名関数/アロー関数の呼び出しをチェック
    // メンバー式や複雑な呼び出しパターンは現在サポートしていない
    
    m_changed = argsChanged;
}

// ヘルパーメソッド

bool InlineFunctionsTransformer::isFunctionInlinable(const FunctionInfo& functionInfo) const {
    // サイズが大きすぎる場合はインライン化しない
    if (functionInfo.size > m_maxInlineSize) {
        return false;
    }
    
    // 複数のリターン文がある場合はインライン化が複雑になる
    if (functionInfo.hasMultipleReturns) {
        return false;
    }
    
    // 再帰関数は制限付きでインライン化
    if (functionInfo.isRecursive && m_maxRecursionDepth == 0) {
        return false;
    }
    
    // 副作用を持つ関数は慎重にインライン化
    // ここでは単純化のため、副作用の有無は関数の大きさに関連付ける
    if (functionInfo.hasSideEffects && functionInfo.size > m_maxInlineSize / 2) {
        return false;
    }
    
    return true;
}

NodePtr InlineFunctionsTransformer::inlineCall(CallExpression* callExpr, const FunctionInfo& funcInfo) {
    // パラメータと引数の数をチェック
    if (funcInfo.parameters.size() != callExpr->arguments.size()) {
        // 引数の数が一致しない場合はインライン化しない
        return nullptr;
    }
    
    // 関数本体がブロック文でない場合（アロー関数の式本体など）
    if (!funcInfo.body || !dynamic_cast<BlockStatement*>(funcInfo.body.get())) {
        // 現在は単純なケースのみサポート
        return nullptr;
    }
    
    // インライン化用の新しいブロック文を作成
    auto inlinedBlock = std::make_shared<BlockStatement>();
    
    // パラメータ/引数のマッピングを作成
    // 単純なケースでは引数を直接使用できるが、複雑な場合は一時変数が必要
    std::unordered_map<std::string, NodePtr> paramArgMap;
    
    for (size_t i = 0; i < funcInfo.parameters.size(); ++i) {
        if (auto* param = dynamic_cast<Identifier*>(funcInfo.parameters[i].get())) {
            // 単純な識別子パラメータの場合
            
            // 引数が単純な場合は直接マッピング
            bool isSimpleArg = dynamic_cast<Identifier*>(callExpr->arguments[i].get()) ||
                               dynamic_cast<Literal*>(callExpr->arguments[i].get());
            
            if (isSimpleArg) {
                paramArgMap[param->name] = callExpr->arguments[i];
            } else {
                // 複雑な引数は一時変数に格納
                auto tempVarName = generateUniqueVarName(param->name);
                auto tempVar = std::make_shared<Identifier>();
                tempVar->name = tempVarName;
                
                // 一時変数の宣言を作成
                auto varDecl = std::make_shared<VariableDeclaration>();
                varDecl->kind = "const";
                
                auto declarator = std::make_shared<VariableDeclarator>();
                declarator->id = std::make_shared<Identifier>();
                static_cast<Identifier*>(declarator->id.get())->name = tempVarName;
                declarator->init = callExpr->arguments[i];
                
                varDecl->declarations.push_back(declarator);
                
                // インラインブロックに変数宣言を追加
                inlinedBlock->body.push_back(varDecl);
                
                // マッピングに一時変数を追加
                paramArgMap[param->name] = tempVar;
            }
        } else {
            // パターンマッチングパラメータなど複雑なケースはまだサポートしていない
            return nullptr;
        }
    }
    
    // 関数本体をクローンしてパラメータを引数に置き換える
    auto functionBody = std::dynamic_pointer_cast<BlockStatement>(funcInfo.body);
    for (auto& stmt : functionBody->body) {
        // 文をクローンして引数で置き換え
        // TODO: 実際には深いクローンが必要で、すべての識別子を確認する必要がある
        // 現在はプレースホルダー実装
        inlinedBlock->body.push_back(stmt->clone());
    }
    
    // インライン化したブロックを返す
    return inlinedBlock;
}

size_t InlineFunctionsTransformer::calculateFunctionSize(NodePtr node) const {
    // 関数のステートメント数を計算
    // 単純な実装では、関数本体のステートメント数のみをカウント
    size_t size = 0;
    
    if (auto* funcDecl = dynamic_cast<FunctionDeclaration*>(node.get())) {
        if (auto* body = dynamic_cast<BlockStatement*>(funcDecl->body.get())) {
            size = body->body.size();
        }
    } else if (auto* funcExpr = dynamic_cast<FunctionExpression*>(node.get())) {
        if (auto* body = dynamic_cast<BlockStatement*>(funcExpr->body.get())) {
            size = body->body.size();
        }
    } else if (auto* arrowFunc = dynamic_cast<ArrowFunctionExpression*>(node.get())) {
        if (arrowFunc->expression) {
            // 式本体の場合は1とカウント
            size = 1;
        } else if (auto* body = dynamic_cast<BlockStatement*>(arrowFunc->body.get())) {
            size = body->body.size();
        }
    }
    
    return size;
}

bool InlineFunctionsTransformer::checkForSideEffects(NodePtr node) const {
    // TODO: 本来は再帰的にノードを解析して副作用をチェックする必要があるが、
    // 簡易版では関数サイズに基づいて判断
    size_t size = calculateFunctionSize(node);
    return size > 5;  // 5行以上の関数は副作用があると仮定
}

bool InlineFunctionsTransformer::isRecursiveFunction(const FunctionInfo& funcInfo) const {
    // 関数が自分自身を呼び出すかどうかをチェック
    // 本来は関数本体を解析して、自分自身を呼び出すコードがあるかをチェックする
    
    // 名前のない関数は再帰できない
    if (funcInfo.name.empty()) {
        return false;
    }
    
    // TODO: 正確には関数本体内で自分自身を参照する呼び出しがあるかチェックする必要がある
    // 現在はプレースホルダー実装
    return false;
}

bool InlineFunctionsTransformer::isIdentifierInScope(const std::string& name) const {
    // スコープスタックを上から順に検索
    for (auto it = m_scopeStack.rbegin(); it != m_scopeStack.rend(); ++it) {
        if (it->find(name) != it->end()) {
            return true;
        }
    }
    return false;
}

std::string InlineFunctionsTransformer::generateUniqueVarName(const std::string& baseName) {
    std::stringstream ss;
    ss << "$" << baseName << "_" << m_nextUniqueId++;
    return ss.str();
}

} // namespace transformers
} // namespace aero 