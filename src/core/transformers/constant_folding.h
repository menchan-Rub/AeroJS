/**
 * @file constant_folding.h
 * @version 1.0.0
 * @copyright Copyright (c) 2023 AeroJS Project
 * @license MIT License
 * @brief 定数畳み込み最適化を行うトランスフォーマー
 */

#ifndef AERO_CONSTANT_FOLDING_H
#define AERO_CONSTANT_FOLDING_H

#include "core/ast/node.h"
#include "core/ast/ast_node_types.h"
#include <string>
#include <memory>
#include <unordered_set>
#include <vector>

namespace aero {
namespace transformers {

/**
 * @brief 定数畳み込み最適化トランスフォーマー
 *
 * コンパイル時に計算可能な式を評価して単純化します。
 * 例：
 * - 算術演算（例：1 + 2 -> 3）
 * - 文字列連結（例："Hello " + "World" -> "Hello World"）
 * - 条件式（例：true ? a : b -> a）
 * - 論理演算（例：true && x -> x, false && x -> false）
 */
class ConstantFoldingTransformer {
public:
    /**
     * @brief コンストラクタ
     */
    ConstantFoldingTransformer();
    
    /**
     * @brief デストラクタ
     */
    ~ConstantFoldingTransformer();
    
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
     * @brief 畳み込まれた式の数を取得します
     * @return 畳み込まれた式の数
     */
    size_t getFoldedExpressions() const;
    
    /**
     * @brief 訪問したノードの数を取得します
     * @return 訪問したノードの数
     */
    size_t getVisitedNodes() const;
    
    /**
     * @brief ASTノードを変換します
     * @param node 変換するノード
     * @return 変換後のノード、変換が行われなかった場合は元のノード
     */
    ast::NodePtr transform(ast::NodePtr node);
    
private:
    /**
     * @brief 子ノードを再帰的に処理します
     * @param node 処理するノード
     * @return 処理後のノード
     */
    ast::NodePtr traverseChildren(ast::NodePtr node);
    
    /**
     * @brief 二項演算式を折りたたみます
     * @param expr 二項演算式
     * @return 折りたたまれた式、または元の式
     */
    ast::NodePtr foldBinaryExpression(std::shared_ptr<ast::BinaryExpression> expr);
    
    /**
     * @brief 単項演算式を折りたたみます
     * @param expr 単項演算式
     * @return 折りたたまれた式、または元の式
     */
    ast::NodePtr foldUnaryExpression(std::shared_ptr<ast::UnaryExpression> expr);
    
    /**
     * @brief 論理演算式を折りたたみます
     * @param expr 論理演算式
     * @return 折りたたまれた式、または元の式
     */
    ast::NodePtr foldLogicalExpression(std::shared_ptr<ast::LogicalExpression> expr);
    
    /**
     * @brief 条件式を折りたたみます
     * @param expr 条件式
     * @return 折りたたまれた式、または元の式
     */
    ast::NodePtr foldConditionalExpression(std::shared_ptr<ast::ConditionalExpression> expr);
    
    /**
     * @brief 配列式を処理します
     * @param expr 配列式
     * @return 処理後の式
     */
    ast::NodePtr foldArrayExpression(std::shared_ptr<ast::ArrayExpression> expr);
    
    /**
     * @brief オブジェクト式を処理します
     * @param expr オブジェクト式
     * @return 処理後の式
     */
    ast::NodePtr foldObjectExpression(std::shared_ptr<ast::ObjectExpression> expr);
    
    /**
     * @brief 関数呼び出し式を処理します
     * @param expr 関数呼び出し式
     * @return 処理後の式
     */
    ast::NodePtr foldCallExpression(std::shared_ptr<ast::CallExpression> expr);
    
    /**
     * @brief メンバーアクセス式を処理します
     * @param expr メンバーアクセス式
     * @return 処理後の式
     */
    ast::NodePtr foldMemberExpression(std::shared_ptr<ast::MemberExpression> expr);
    
    /**
     * @brief ノードが定数かどうかをチェックします
     * @param node チェックするノード
     * @return 定数の場合はtrue
     */
    bool isConstant(ast::NodePtr node) const;
    
    /**
     * @brief 値が真値と評価されるかをチェックします
     * @param literal リテラル値
     * @return 真値と評価される場合はtrue
     */
    bool isTruthy(std::shared_ptr<ast::Literal> literal) const;
    
    /**
     * @brief 二項演算を評価します
     * @param op 演算子
     * @param left 左辺値
     * @param right 右辺値
     * @return 評価結果、評価できない場合はnullptr
     */
    ast::NodePtr evaluateBinaryOperation(
        ast::BinaryOperator op,
        std::shared_ptr<ast::Literal> left,
        std::shared_ptr<ast::Literal> right);
    
    /**
     * @brief 単項演算を評価します
     * @param op 演算子
     * @param arg 引数
     * @return 評価結果、評価できない場合はnullptr
     */
    ast::NodePtr evaluateUnaryOperation(
        ast::UnaryOperator op,
        std::shared_ptr<ast::Literal> arg);
    
    /**
     * @brief 論理演算を評価します
     * @param op 演算子
     * @param left 左辺値
     * @param right 右辺値
     * @return 評価結果、評価できない場合はnullptr
     */
    ast::NodePtr evaluateLogicalOperation(
        ast::LogicalOperator op,
        std::shared_ptr<ast::Literal> left,
        std::shared_ptr<ast::Literal> right);
    
    /**
     * @brief リテラルを文字列に変換します
     * @param literal 変換するリテラル
     * @return 文字列表現
     */
    std::string literalToString(std::shared_ptr<ast::Literal> literal) const;
    
    /**
     * @brief 組み込みのMath関数を評価します
     * @param name 関数名
     * @param args 数値引数のリスト
     * @param result 計算結果
     * @return 計算に成功した場合はtrue
     */
    bool evaluateBuiltInMathFunction(
        const std::string& name,
        const std::vector<double>& args,
        double& result);
    
    bool m_statisticsEnabled;      ///< 統計収集が有効かどうか
    size_t m_foldedExpressions;    ///< 畳み込まれた式の数
    size_t m_visitedNodes;         ///< 訪問したノードの数
};

} // namespace transformers
} // namespace aero

#endif // AERO_CONSTANT_FOLDING_H 