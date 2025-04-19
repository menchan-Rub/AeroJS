/**
 * @file inline_functions.h
 * @version 1.0.0
 * @copyright Copyright (c) 2023 AeroJS Project
 * @license MIT License
 * @brief 関数インライン化を行うトランスフォーマー
 */

#ifndef AERO_INLINE_FUNCTIONS_H
#define AERO_INLINE_FUNCTIONS_H

#include "transformer.h"
#include "core/ast/node.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <memory>
#include <vector>

namespace aero {
namespace transformers {

/**
 * @brief 関数インライン化トランスフォーマー
 *
 * 小さな関数や頻繁に呼び出される関数を呼び出し箇所にインライン化して、
 * 関数呼び出しのオーバーヘッドを削減します。
 * 
 * 以下のような最適化を行います：
 * - 小さな関数のインライン化
 * - 単一リターン関数のインライン化
 * - 再帰関数や大きすぎる関数のインライン化回避
 * - 副作用を考慮したインライン化の安全性確保
 */
class InlineFunctionsTransformer : public Transformer {
public:
    /**
     * @brief コンストラクタ
     * @param maxInlineSize インライン化する関数の最大サイズ（ステートメント数）
     * @param maxRecursionDepth 再帰関数の最大インライン化深度
     * @param enableStatistics 統計収集を有効にするかどうか
     */
    InlineFunctionsTransformer(
        size_t maxInlineSize = 20,
        size_t maxRecursionDepth = 2,
        bool enableStatistics = false
    );
    
    /**
     * @brief デストラクタ
     */
    ~InlineFunctionsTransformer() override = default;
    
    /**
     * @brief 統計カウンタをリセットします
     */
    void reset();
    
    /**
     * @brief 統計収集を有効または無効にします
     * @param enable 統計収集を有効にする場合はtrue
     */
    void enableStatistics(bool enable);
    
    /**
     * @brief インライン化された関数の数を取得します
     * @return インライン化された関数の数
     */
    size_t getInlinedFunctionsCount() const;
    
    /**
     * @brief インライン化された呼び出しの数を取得します
     * @return インライン化された呼び出しの数
     */
    size_t getInlinedCallsCount() const;
    
    /**
     * @brief 訪問したノードの数を取得します
     * @return 訪問したノードの数
     */
    size_t getVisitedNodesCount() const;
    
protected:
    // AST訪問メソッドのオーバーライド
    void visitProgram(ast::Program* node) override;
    void visitFunctionDeclaration(ast::FunctionDeclaration* node) override;
    void visitFunctionExpression(ast::FunctionExpression* node) override;
    void visitArrowFunctionExpression(ast::ArrowFunctionExpression* node) override;
    void visitCallExpression(ast::CallExpression* node) override;
    
private:
    /**
     * @brief 関数情報を保持する構造体
     */
    struct FunctionInfo {
        ast::NodePtr functionNode;     ///< 関数ノード
        std::string name;              ///< 関数名（匿名関数の場合は空文字列）
        size_t size;                   ///< 関数のサイズ（ステートメント数）
        bool hasSideEffects;           ///< 副作用があるかどうか
        bool hasMultipleReturns;       ///< 複数のreturn文があるかどうか
        bool isRecursive;              ///< 再帰関数かどうか
        bool isEligibleForInlining;    ///< インライン化可能かどうか
        
        // パラメータと本体の情報
        std::vector<ast::NodePtr> parameters;  ///< パラメータノード
        ast::NodePtr body;                     ///< 関数本体
    };
    
    /**
     * @brief 関数をインライン化可能かどうかチェックします
     * @param functionInfo 関数情報
     * @return インライン化可能な場合はtrue
     */
    bool isFunctionInlinable(const FunctionInfo& functionInfo) const;
    
    /**
     * @brief 関数呼び出しをインライン化します
     * @param callExpr 関数呼び出し式
     * @param funcInfo 関数情報
     * @return インライン化した式、またはnullptrでインライン化不可
     */
    ast::NodePtr inlineCall(ast::CallExpression* callExpr, const FunctionInfo& funcInfo);
    
    /**
     * @brief 関数のサイズを計算します（ステートメント数）
     * @param node 関数ノード
     * @return ステートメントの数
     */
    size_t calculateFunctionSize(ast::NodePtr node) const;
    
    /**
     * @brief 関数に副作用があるかどうかをチェックします
     * @param node 関数ノード
     * @return 副作用がある場合はtrue
     */
    bool checkForSideEffects(ast::NodePtr node) const;
    
    /**
     * @brief 関数が再帰的かどうかをチェックします
     * @param funcInfo 関数情報
     * @return 再帰関数の場合はtrue
     */
    bool isRecursiveFunction(const FunctionInfo& funcInfo) const;
    
    /**
     * @brief 識別子が現在のスコープで参照可能かチェックします
     * @param name 識別子名
     * @return スコープ内に存在する場合はtrue
     */
    bool isIdentifierInScope(const std::string& name) const;
    
    /**
     * @brief 変数名の衝突を回避する一意の名前を生成します
     * @param baseName 基本名
     * @return 一意の変数名
     */
    std::string generateUniqueVarName(const std::string& baseName);
    
    // 関数マップとスコープ情報
    std::unordered_map<std::string, FunctionInfo> m_functionMap;  ///< 名前付き関数のマップ
    std::vector<FunctionInfo> m_anonymousFunctions;               ///< 匿名関数のリスト
    std::vector<std::unordered_set<std::string>> m_scopeStack;    ///< スコープスタック
    
    // インライン化オプション
    size_t m_maxInlineSize;            ///< インライン化する関数の最大サイズ
    size_t m_maxRecursionDepth;        ///< 再帰関数の最大インライン化深度
    size_t m_currentRecursionDepth;    ///< 現在の再帰深度
    
    // 統計情報
    bool m_statisticsEnabled;          ///< 統計収集が有効かどうか
    size_t m_inlinedFunctionsCount;    ///< インライン化された関数の数
    size_t m_inlinedCallsCount;        ///< インライン化された呼び出しの数
    size_t m_visitedNodesCount;        ///< 訪問したノードの数
    
    // 次に生成する一意の変数番号
    size_t m_nextUniqueId;             ///< 一意のID生成用カウンタ
};

} // namespace transformers
} // namespace aero

#endif // AERO_INLINE_FUNCTIONS_H 