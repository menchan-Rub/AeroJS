/**
 * @file identifier_lookup_optimization.cpp
 * @brief 識別子検索の最適化トランスフォーマーの実装
 * @version 0.1
 * @date 2024-01-11
 * 
 * @copyright Copyright (c) 2024 AeroJS Project
 * 
 * @license
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "identifier_lookup_optimization.h"
#include "../ast/ast_visitor.h"
#include "../ast/ast_node.h"
#include "../ast/ast_node_types.h"
#include "../utils/logger.h"
#include <stack>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <cassert>
#include <iostream>

namespace aero {
namespace transformers {

/**
 * @brief 識別子検索の最適化トランスフォーマーのコンストラクタ
 */
IdentifierLookupOptimizer::IdentifierLookupOptimizer()
    : statisticsEnabled_(false),
      optimizedIdentifiersCount_(0),
      fastLookupHitsCount_(0),
      scopeHierarchyOptimizationsCount_(0) {
}

/**
 * @brief 統計情報の収集を有効/無効にする
 * @param enabled 有効にする場合はtrue、無効にする場合はfalse
 */
void IdentifierLookupOptimizer::enableStatistics(bool enabled) {
    statisticsEnabled_ = enabled;
}

/**
 * @brief 最適化された識別子の数を取得
 * @return 最適化された識別子の数
 */
size_t IdentifierLookupOptimizer::getOptimizedIdentifiersCount() const {
    return optimizedIdentifiersCount_;
}

/**
 * @brief 高速検索のヒット数を取得
 * @return 高速検索のヒット数
 */
size_t IdentifierLookupOptimizer::getFastLookupHitsCount() const {
    return fastLookupHitsCount_;
}

/**
 * @brief スコープ階層の最適化数を取得
 * @return スコープ階層の最適化数
 */
size_t IdentifierLookupOptimizer::getScopeHierarchyOptimizationsCount() const {
    return scopeHierarchyOptimizationsCount_;
}

/**
 * @brief カウンターをリセットする
 */
void IdentifierLookupOptimizer::resetCounters() {
    optimizedIdentifiersCount_ = 0;
    fastLookupHitsCount_ = 0;
    scopeHierarchyOptimizationsCount_ = 0;
}

/**
 * @brief ASTノードを最適化する
 * @param node 最適化対象のノード
 * @return 最適化されたノード
 */
ast::NodePtr IdentifierLookupOptimizer::transform(const ast::NodePtr& node) {
    if (!node) {
        return nullptr;
    }

    // スコープとシンボルの情報をリセット
    scopes_.clear();
    currentScopeIndex_ = 0;
    scopePath_.clear();

    // グローバルスコープを作成
    ScopeInfo globalScope;
    globalScope.type = ScopeType::Global;
    globalScope.parentIndex = -1; // グローバルスコープは親を持たない
    scopes_.push_back(globalScope);
    scopePath_.push_back(0); // グローバルスコープのインデックス

    // AST全体を走査し、スコープとシンボルを記録
    buildScopeAndSymbolInfo(node);

    // スコープベースの最適化を適用
    return applyOptimizations(node);
}

/**
 * @brief ASTを走査してスコープとシンボル情報を構築
 * @param node ASTのルートノード
 */
void IdentifierLookupOptimizer::buildScopeAndSymbolInfo(const ast::NodePtr& node) {
    // 現在のスコープのインデックスを取得
    size_t scopeIndex = scopePath_.back();
    
    // ノードタイプに基づいて処理
    switch (node->getType()) {
        case ast::NodeType::Program:
        case ast::NodeType::BlockStatement:
            // ブロックスコープを作成
            if (node->getType() != ast::NodeType::Program) {
                // プログラムノードはすでにグローバルスコープを持っているので、
                // ブロック文の場合のみ新しいスコープを作成
                createAndEnterScope(ScopeType::Block);
            }
            // 子ノードを処理
            for (const auto& child : node->getChildren()) {
                buildScopeAndSymbolInfo(child);
            }
            // ブロックスコープを終了
            if (node->getType() != ast::NodeType::Program) {
                exitScope();
            }
            break;
            
        case ast::NodeType::FunctionDeclaration:
        case ast::NodeType::FunctionExpression:
        case ast::NodeType::ArrowFunctionExpression: {
            // 関数名を登録（関数宣言の場合）
            if (node->getType() == ast::NodeType::FunctionDeclaration && node->getName() != "") {
                registerSymbol(node->getName(), node);
            }
            
            // 関数スコープを作成
            createAndEnterScope(ScopeType::Function);
            
            // パラメータを登録
            const auto& params = node->getPropertyAsNodeArray("params");
            for (const auto& param : params) {
                if (param->getType() == ast::NodeType::Identifier) {
                    registerSymbol(param->getName(), param);
                } else {
                    // パターンや分割代入などの複雑なパラメータの場合は再帰的に処理
                    extractAndRegisterPatternIdentifiers(param);
                }
            }
            
            // 関数本体を処理
            const auto& body = node->getPropertyAsNode("body");
            if (body) {
                buildScopeAndSymbolInfo(body);
            }
            
            // 関数スコープを終了
            exitScope();
            break;
        }
            
        case ast::NodeType::VariableDeclaration: {
            // 変数宣言を処理
            const auto& declarations = node->getPropertyAsNodeArray("declarations");
            const std::string& kind = node->getPropertyAsString("kind");
            
            for (const auto& declaration : declarations) {
                const auto& id = declaration->getPropertyAsNode("id");
                const auto& init = declaration->getPropertyAsNode("init");
                
                // 初期化式があれば先に処理（クロージャー内の参照などのために）
                if (init) {
                    buildScopeAndSymbolInfo(init);
                }
                
                // 識別子を登録
                if (id->getType() == ast::NodeType::Identifier) {
                    registerSymbol(id->getName(), id, kind);
                } else {
                    // パターンや分割代入の場合は再帰的に処理
                    extractAndRegisterPatternIdentifiers(id, kind);
                }
            }
            break;
        }
            
        case ast::NodeType::Identifier: {
            // 識別子の参照を処理
            const std::string& name = node->getName();
            // シンボルの解決（定義箇所の検索）は後の最適化フェーズで行う
            break;
        }
            
        case ast::NodeType::ClassDeclaration:
        case ast::NodeType::ClassExpression: {
            // クラス名を登録（クラス宣言の場合）
            if (node->getType() == ast::NodeType::ClassDeclaration && node->getName() != "") {
                registerSymbol(node->getName(), node);
            }
            
            // クラススコープを作成
            createAndEnterScope(ScopeType::Class);
            
            // スーパークラスがあれば処理
            const auto& superClass = node->getPropertyAsNode("superClass");
            if (superClass) {
                buildScopeAndSymbolInfo(superClass);
            }
            
            // クラス本体を処理
            const auto& body = node->getPropertyAsNode("body");
            if (body) {
                buildScopeAndSymbolInfo(body);
            }
            
            // クラススコープを終了
            exitScope();
            break;
        }
            
        case ast::NodeType::ForStatement:
        case ast::NodeType::ForInStatement:
        case ast::NodeType::ForOfStatement: {
            // ループスコープを作成
            createAndEnterScope(ScopeType::Block);
            
            // 初期化部分を処理
            const auto& init = node->getPropertyAsNode("init");
            if (init) {
                buildScopeAndSymbolInfo(init);
            }
            
            // テスト部分を処理
            const auto& test = node->getPropertyAsNode("test");
            if (test) {
                buildScopeAndSymbolInfo(test);
            }
            
            // 更新部分を処理
            const auto& update = node->getPropertyAsNode("update");
            if (update) {
                buildScopeAndSymbolInfo(update);
            }
            
            // ループ本体を処理
            const auto& body = node->getPropertyAsNode("body");
            if (body) {
                buildScopeAndSymbolInfo(body);
            }
            
            // ループスコープを終了
            exitScope();
            break;
        }
            
        case ast::NodeType::CatchClause: {
            // キャッチ節スコープを作成
            createAndEnterScope(ScopeType::Block);
            
            // パラメータを登録
            const auto& param = node->getPropertyAsNode("param");
            if (param) {
                if (param->getType() == ast::NodeType::Identifier) {
                    registerSymbol(param->getName(), param);
                } else {
                    // パターンや分割代入の場合は再帰的に処理
                    extractAndRegisterPatternIdentifiers(param);
                }
            }
            
            // キャッチ本体を処理
            const auto& body = node->getPropertyAsNode("body");
            if (body) {
                buildScopeAndSymbolInfo(body);
            }
            
            // キャッチスコープを終了
            exitScope();
            break;
        }
            
        default:
            // 他のノードタイプは子ノードを再帰的に処理
            for (const auto& child : node->getChildren()) {
                buildScopeAndSymbolInfo(child);
            }
            break;
    }
}

/**
 * @brief 新しいスコープを作成して進入
 * @param type スコープの種類
 * @return 作成したスコープのインデックス
 */
size_t IdentifierLookupOptimizer::createAndEnterScope(ScopeType type) {
    ScopeInfo scope;
    scope.type = type;
    scope.parentIndex = scopePath_.back();
    
    scopes_.push_back(scope);
    size_t newScopeIndex = scopes_.size() - 1;
    scopePath_.push_back(newScopeIndex);
    
    return newScopeIndex;
}

/**
 * @brief 現在のスコープから抜ける
 */
void IdentifierLookupOptimizer::exitScope() {
    if (scopePath_.size() > 1) { // グローバルスコープは維持
        scopePath_.pop_back();
    }
}

/**
 * @brief シンボル（変数、関数など）を現在のスコープに登録
 * @param name シンボル名
 * @param node シンボルが定義されたノード
 * @param kind 変数の種類（"var", "let", "const"）
 */
void IdentifierLookupOptimizer::registerSymbol(const std::string& name, const ast::NodePtr& node, const std::string& kind) {
    size_t scopeIndex = scopePath_.back();
    
    // "var" 宣言の場合、関数スコープまたはグローバルスコープに登録
    if (kind == "var") {
        // 関数スコープまたはグローバルスコープを探す
        for (int i = static_cast<int>(scopePath_.size()) - 1; i >= 0; --i) {
            size_t checkScopeIndex = scopePath_[i];
            if (scopes_[checkScopeIndex].type == ScopeType::Function || 
                scopes_[checkScopeIndex].type == ScopeType::Global) {
                scopeIndex = checkScopeIndex;
                break;
            }
        }
    }
    
    // シンボル情報を作成
    SymbolInfo symbol;
    symbol.name = name;
    symbol.node = node;
    symbol.kind = kind;
    symbol.scopeIndex = scopeIndex;
    
    // シンボルを現在のスコープに追加
    scopes_[scopeIndex].symbols[name] = symbol;
}

/**
 * @brief パターン（オブジェクトパターン、配列パターンなど）から識別子を抽出して登録
 * @param node パターンノード
 * @param kind 変数の種類（"var", "let", "const"）
 */
void IdentifierLookupOptimizer::extractAndRegisterPatternIdentifiers(const ast::NodePtr& node, const std::string& kind) {
    switch (node->getType()) {
        case ast::NodeType::Identifier:
            // 識別子の場合は直接登録
            registerSymbol(node->getName(), node, kind);
            break;
            
        case ast::NodeType::ObjectPattern: {
            // オブジェクトパターンの場合、各プロパティを処理
            const auto& properties = node->getPropertyAsNodeArray("properties");
            for (const auto& prop : properties) {
                const auto& value = prop->getPropertyAsNode("value");
                if (value) {
                    extractAndRegisterPatternIdentifiers(value, kind);
                }
            }
            break;
        }
            
        case ast::NodeType::ArrayPattern: {
            // 配列パターンの場合、各要素を処理
            const auto& elements = node->getPropertyAsNodeArray("elements");
            for (const auto& element : elements) {
                if (element) { // nullの要素はスキップ（スプレッド演算子や空の位置）
                    extractAndRegisterPatternIdentifiers(element, kind);
                }
            }
            break;
        }
            
        case ast::NodeType::RestElement: {
            // レスト要素の場合、引数を処理
            const auto& argument = node->getPropertyAsNode("argument");
            if (argument) {
                extractAndRegisterPatternIdentifiers(argument, kind);
            }
            break;
        }
            
        case ast::NodeType::AssignmentPattern: {
            // デフォルト値パターンの場合、左辺を処理
            const auto& left = node->getPropertyAsNode("left");
            if (left) {
                extractAndRegisterPatternIdentifiers(left, kind);
            }
            // 右辺（デフォルト値）は処理しない
            break;
        }
            
        default:
            // その他のノードタイプは無視
            break;
    }
}

/**
 * @brief スコープベースの最適化を適用
 * @param node 最適化対象のノード
 * @return 最適化されたノード
 */
ast::NodePtr IdentifierLookupOptimizer::applyOptimizations(const ast::NodePtr& node) {
    // 最適化処理用のスコープ状態をリセット
    scopePath_.clear();
    scopePath_.push_back(0); // グローバルスコープから開始
    
    // 最適化を適用
    return optimizeNode(node);
}

/**
 * @brief 特定のノードに最適化を適用
 * @param node 最適化対象のノード
 * @return 最適化されたノード
 */
ast::NodePtr IdentifierLookupOptimizer::optimizeNode(const ast::NodePtr& node) {
    if (!node) {
        return nullptr;
    }
    
    // スコープ変更を追跡
    bool scopeChanged = false;
    
    // ノードタイプに基づいて処理
    switch (node->getType()) {
        case ast::NodeType::Program:
        case ast::NodeType::BlockStatement:
            // ブロックスコープを更新
            if (node->getType() != ast::NodeType::Program) {
                // スコープ情報を見つける
                for (size_t i = 0; i < scopes_.size(); ++i) {
                    if (scopes_[i].parentIndex == scopePath_.back() && 
                        scopes_[i].type == ScopeType::Block) {
                        scopePath_.push_back(i);
                        scopeChanged = true;
                        break;
                    }
                }
            }
            break;
            
        case ast::NodeType::FunctionDeclaration:
        case ast::NodeType::FunctionExpression:
        case ast::NodeType::ArrowFunctionExpression:
            // 関数スコープを更新
            for (size_t i = 0; i < scopes_.size(); ++i) {
                if (scopes_[i].parentIndex == scopePath_.back() && 
                    scopes_[i].type == ScopeType::Function) {
                    scopePath_.push_back(i);
                    scopeChanged = true;
                    break;
                }
            }
            break;
            
        case ast::NodeType::ClassDeclaration:
        case ast::NodeType::ClassExpression:
            // クラススコープを更新
            for (size_t i = 0; i < scopes_.size(); ++i) {
                if (scopes_[i].parentIndex == scopePath_.back() && 
                    scopes_[i].type == ScopeType::Class) {
                    scopePath_.push_back(i);
                    scopeChanged = true;
                    break;
                }
            }
            break;
            
        case ast::NodeType::Identifier: {
            // 識別子の参照を最適化
            const std::string& name = node->getName();
            
            // シンボルを解決
            SymbolInfo* symbol = resolveSymbol(name);
            if (symbol) {
                // 識別子ノードに最適化情報を追加
                node->setProperty("resolvedSymbol", symbol->name);
                node->setProperty("resolvedScopeIndex", static_cast<int>(symbol->scopeIndex));
                node->setProperty("resolvedScopeDepth", calculateScopeDepth(symbol->scopeIndex));
                
                if (statisticsEnabled_) {
                    optimizedIdentifiersCount_++;
                }
            }
            break;
        }
            
        default:
            // 他のノードタイプは特別な処理不要
            break;
    }
    
    // 子ノードを再帰的に最適化
    for (auto& child : node->getChildren()) {
        child = optimizeNode(child);
    }
    
    // スコープを元に戻す
    if (scopeChanged) {
        scopePath_.pop_back();
    }
    
    return node;
}

/**
 * @brief 識別子に対応するシンボル情報を解決
 * @param name 識別子名
 * @return 見つかったシンボル情報へのポインタ、または見つからない場合はnullptr
 */
IdentifierLookupOptimizer::SymbolInfo* IdentifierLookupOptimizer::resolveSymbol(const std::string& name) {
    // スコープ階層を上から順に検索
    for (int i = static_cast<int>(scopePath_.size()) - 1; i >= 0; --i) {
        size_t scopeIndex = scopePath_[i];
        auto& scope = scopes_[scopeIndex];
        
        // 現在のスコープでシンボルを検索
        auto it = scope.symbols.find(name);
        if (it != scope.symbols.end()) {
            // シンボルが見つかった
            if (statisticsEnabled_) {
                // スコープ階層最適化の測定
                if (i < static_cast<int>(scopePath_.size()) - 1) {
                    scopeHierarchyOptimizationsCount_++;
                }
                
                // 高速検索ヒットの測定（最初のスコープで見つかった場合）
                if (i == static_cast<int>(scopePath_.size()) - 1) {
                    fastLookupHitsCount_++;
                }
            }
            
            return &it->second;
        }
    }
    
    // シンボルが見つからなかった
    return nullptr;
}

/**
 * @brief 特定のスコープの深さを計算
 * @param scopeIndex スコープのインデックス
 * @return 計算されたスコープの深さ
 */
int IdentifierLookupOptimizer::calculateScopeDepth(size_t scopeIndex) {
    int depth = 0;
    int parentIndex = static_cast<int>(scopeIndex);
    
    while (parentIndex > 0) {
        parentIndex = scopes_[parentIndex].parentIndex;
        depth++;
    }
    
    return depth;
}

} // namespace transformers
} // namespace aero 