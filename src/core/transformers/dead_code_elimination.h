/**
 * @file dead_code_elimination.h
 * @version 1.0.0
 * @copyright Copyright (c) 2023 AeroJS Project
 * @license MIT License
 * @brief 不要なコード（デッドコード）を除去するトランスフォーマー
 */

#ifndef AERO_DEAD_CODE_ELIMINATION_H
#define AERO_DEAD_CODE_ELIMINATION_H

#include "transformer.h"
#include "core/ast/node.h"
#include "core/ast/visitor.h"
#include "core/analysis/control_flow_graph.h"
#include "core/analysis/data_flow_analysis.h"
#include "core/analysis/constant_propagation.h"
#include "core/optimization/optimization_level.h"
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <optional>
#include <stack>
#include <bitset>

namespace aero {
namespace transformers {

/**
 * @brief デッドコード除去トランスフォーマー
 *
 * 実行されない、または効果のないコードを検出して削除します。
 * 以下のような最適化を行います：
 * - 到達不能なコード（例：return後のステートメント）の除去
 * - 使用されない変数の除去
 * - 条件が静的に決まる場合の条件式の簡略化（if (true) { ... }）
 * - 空のブロックの除去
 * - 効果のない式ステートメントの除去
 * - 未使用の関数の除去（モジュールスコープ内）
 * - 定数畳み込みと組み合わせた条件分岐の最適化
 * - ループ不変コードの検出と最適化
 */
class DeadCodeEliminationTransformer : public Transformer {
public:
    /**
     * @brief コンストラクタ
     * @param optimizationLevel 最適化レベル
     */
    explicit DeadCodeEliminationTransformer(
        optimization::OptimizationLevel optimizationLevel = optimization::OptimizationLevel::Normal);
    
    /**
     * @brief デストラクタ
     */
    ~DeadCodeEliminationTransformer() override = default;
    
    /**
     * @brief 変換処理の実行
     * @param ast 変換対象のAST
     * @return 変換後のAST
     */
    ast::NodePtr transform(ast::NodePtr ast) override;
    
    /**
     * @brief 最適化レベルの設定
     * @param level 設定する最適化レベル
     */
    void setOptimizationLevel(optimization::OptimizationLevel level);
    
    /**
     * @brief 現在の最適化レベルの取得
     * @return 現在の最適化レベル
     */
    optimization::OptimizationLevel getOptimizationLevel() const;
    
    /**
     * @brief 統計情報の取得
     * @return 最適化の統計情報
     */
    struct Statistics {
        size_t removedStatements;       ///< 削除されたステートメント数
        size_t removedVariables;        ///< 削除された変数宣言数
        size_t simplifiedExpressions;   ///< 簡略化された式の数
        size_t removedFunctions;        ///< 削除された関数数
        size_t unreachableCodeBlocks;   ///< 削除された到達不能コードブロック数
        size_t optimizedConditions;     ///< 最適化された条件式数
        size_t optimizedLoops;          ///< 最適化されたループ数
        
        Statistics() : removedStatements(0), removedVariables(0), 
                      simplifiedExpressions(0), removedFunctions(0),
                      unreachableCodeBlocks(0), optimizedConditions(0),
                      optimizedLoops(0) {}
    };
    
    /**
     * @brief 統計情報の取得
     * @return 最適化の統計情報
     */
    const Statistics& getStatistics() const;
    
protected:
    // 特定のノードタイプに対する変換処理の実装
    void visitProgram(ast::Program* node) override;
    void visitBlockStatement(ast::BlockStatement* node) override;
    void visitIfStatement(ast::IfStatement* node) override;
    void visitSwitchStatement(ast::SwitchStatement* node) override;
    void visitForStatement(ast::ForStatement* node) override;
    void visitWhileStatement(ast::WhileStatement* node) override;
    void visitDoWhileStatement(ast::DoWhileStatement* node) override;
    void visitTryStatement(ast::TryStatement* node) override;
    void visitReturnStatement(ast::ReturnStatement* node) override;
    void visitBreakStatement(ast::BreakStatement* node) override;
    void visitContinueStatement(ast::ContinueStatement* node) override;
    void visitThrowStatement(ast::ThrowStatement* node) override;
    void visitExpressionStatement(ast::ExpressionStatement* node) override;
    void visitVariableDeclaration(ast::VariableDeclaration* node) override;
    void visitFunctionDeclaration(ast::FunctionDeclaration* node) override;
    void visitFunctionExpression(ast::FunctionExpression* node) override;
    void visitArrowFunctionExpression(ast::ArrowFunctionExpression* node) override;
    void visitClassDeclaration(ast::ClassDeclaration* node) override;
    void visitClassExpression(ast::ClassExpression* node) override;
    void visitLiteral(ast::Literal* node) override;
    void visitIdentifier(ast::Identifier* node) override;
    void visitBinaryExpression(ast::BinaryExpression* node) override;
    void visitLogicalExpression(ast::LogicalExpression* node) override;
    void visitUnaryExpression(ast::UnaryExpression* node) override;
    void visitConditionalExpression(ast::ConditionalExpression* node) override;
    void visitCallExpression(ast::CallExpression* node) override;
    void visitMemberExpression(ast::MemberExpression* node) override;
    void visitArrayExpression(ast::ArrayExpression* node) override;
    void visitObjectExpression(ast::ObjectExpression* node) override;
    
private:
    /**
     * @brief 式が副作用を持つかどうかをチェックする
     * @param node チェックする式ノード
     * @return 副作用を持つ場合はtrue
     */
    bool hasSideEffects(ast::NodePtr node);
    
    /**
     * @brief リテラル値が真値（true）になるかどうかをチェックする
     * @param node チェックする式ノード
     * @param result 結果を格納するブール値への参照
     * @return 真偽値が静的に決定できる場合はtrue
     */
    bool evaluatesToTruthy(ast::NodePtr node, bool& result);
    
    /**
     * @brief 変数の使用箇所を収集する
     * @param node 解析するノード
     * @param usedVariables 使用されている変数名のセット
     */
    void collectUsedVariables(ast::NodePtr node, std::unordered_set<std::string>& usedVariables);
    
    /**
     * @brief 静的に評価可能な二項演算を計算する
     * @param node 二項演算ノード
     * @param result 結果値を格納するリテラルノード
     * @return 計算が可能な場合はtrue
     */
    bool evaluateBinaryExpression(ast::BinaryExpression* node, ast::LiteralPtr& result);
    
    /**
     * @brief 静的に評価可能な論理演算を計算する
     * @param node 論理演算ノード
     * @param result 結果値を格納するリテラルノード
     * @return 計算が可能な場合はtrue
     */
    bool evaluateLogicalExpression(ast::LogicalExpression* node, ast::LiteralPtr& result);
    
    /**
     * @brief 静的に評価可能な単項演算を計算する
     * @param node 単項演算ノード
     * @param result 結果値を格納するリテラルノード
     * @return 計算が可能な場合はtrue
     */
    bool evaluateUnaryExpression(ast::UnaryExpression* node, ast::LiteralPtr& result);
    
    /**
     * @brief 静的に評価可能な条件式を計算する
     * @param node 条件式ノード
     * @param result 結果値を格納するリテラルノード
     * @return 計算が可能な場合はtrue
     */
    bool evaluateConditionalExpression(ast::ConditionalExpression* node, ast::LiteralPtr& result);
    
    /**
     * @brief 到達不能コードを削除する
     * @param statements 文のリスト
     * @return 変更があった場合はtrue
     */
    bool removeUnreachableCode(std::vector<ast::NodePtr>& statements);
    
    /**
     * @brief 使用されていない変数宣言を削除する
     * @param statements 文のリスト
     * @param usedVariables 使用されている変数名のセット
     * @return 変更があった場合はtrue
     */
    bool removeUnusedVariables(std::vector<ast::NodePtr>& statements, 
                              const std::unordered_set<std::string>& usedVariables);
    
    /**
     * @brief 空のブロックを削除する
     * @param statements 文のリスト
     * @return 変更があった場合はtrue
     */
    bool removeEmptyBlocks(std::vector<ast::NodePtr>& statements);
    
    /**
     * @brief 効果のない式ステートメントを削除する
     * @param statements 文のリスト
     * @return 変更があった場合はtrue
     */
    bool removeNoEffectExpressions(std::vector<ast::NodePtr>& statements);
    
    /**
     * @brief 条件分岐を最適化する
     * @param node 条件分岐ノード
     * @return 最適化されたノード
     */
    ast::NodePtr optimizeConditionalStatement(ast::IfStatement* node);
    
    /**
     * @brief ループを最適化する
     * @param node ループノード
     * @return 最適化されたノード
     */
    template<typename T>
    ast::NodePtr optimizeLoop(T* node);
    
    /**
     * @brief スコープに入る
     * @param scopeName スコープ名
     */
    void enterScope(const std::string& scopeName);
    
    /**
     * @brief スコープから出る
     */
    void exitScope();
    
    /**
     * @brief 変数の使用状態を追跡する
     * @param name 変数名
     * @param isDeclaration 宣言かどうか
     */
    void trackVariable(const std::string& name, bool isDeclaration = false);
    
    /**
     * @brief 関数の使用状態を追跡する
     * @param name 関数名
     * @param isDeclaration 宣言かどうか
     */
    void trackFunction(const std::string& name, bool isDeclaration = false);
    
    /**
     * @brief 制御フローグラフを構築する
     * @param node 解析対象のノード
     * @return 構築されたCFG
     */
    std::unique_ptr<analysis::ControlFlowGraph> buildCFG(ast::NodePtr node);
    
    /**
     * @brief データフロー解析を実行する
     * @param cfg 制御フローグラフ
     * @return データフロー解析結果
     */
    std::unique_ptr<analysis::DataFlowAnalysis> performDataFlowAnalysis(
        const analysis::ControlFlowGraph& cfg);
    
    /**
     * @brief 定数伝播解析を実行する
     * @param node 解析対象のノード
     * @return 定数値のマップ
     */
    std::unordered_map<std::string, ast::LiteralPtr> performConstantPropagation(ast::NodePtr node);
    
    /**
     * @brief ループ不変コードを検出する
     * @param loopNode ループノード
     * @return ループ不変の式のリスト
     */
    std::vector<ast::NodePtr> detectLoopInvariantCode(ast::NodePtr loopNode);
    
    // 現在のスコープ情報
    struct ScopeInfo {
        std::string name;
        std::unordered_map<std::string, bool> variables;  // 変数名 -> 使用されたか
        std::unordered_map<std::string, bool> functions;  // 関数名 -> 使用されたか
        bool unreachableCodeDetected;
        
        ScopeInfo(const std::string& n) : name(n), unreachableCodeDetected(false) {}
    };
    
    std::stack<ScopeInfo> m_scopeStack;                   ///< スコープスタック
    std::unordered_set<std::string> m_globalIdentifiers;  ///< グローバル識別子
    std::unordered_map<std::string, ast::LiteralPtr> m_constantValues; ///< 定数値
    std::bitset<64> m_optimizationFlags;                  ///< 最適化フラグ
    optimization::OptimizationLevel m_optimizationLevel;  ///< 最適化レベル
    Statistics m_statistics;                              ///< 統計情報
    bool m_changed;                                       ///< 変更があったかどうか
};

} // namespace transformers
} // namespace aero

#endif // AERO_DEAD_CODE_ELIMINATION_H 