/**
 * @file identifier_lookup_optimization.h
 * @brief 識別子検索の最適化トランスフォーマーのヘッダーファイル
 * @version 0.1
 * @date 2024-01-11
 *
 * @copyright Copyright (c) 2024 AeroJS Project
 *
 * @license
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef AERO_TRANSFORMERS_IDENTIFIER_LOOKUP_OPTIMIZATION_H_
#define AERO_TRANSFORMERS_IDENTIFIER_LOOKUP_OPTIMIZATION_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "transformer.h"

namespace aero {
namespace transformers {

/**
 * @class IdentifierLookupOptimizer
 * @brief JavaScriptコード内の識別子検索を最適化するトランスフォーマー
 *
 * このトランスフォーマーは、JavaScriptのASTを解析して、識別子の参照と宣言の関係を
 * 解析し、スコープチェーンのルックアップを最適化します。変数の参照解決を高速化するために、
 * 各識別子に解決済みのスコープ情報を付加します。
 *
 * 主な最適化:
 * 1. 各識別子に対して宣言が存在するスコープを事前に解決
 * 2. スコープ階層をインデックス化して高速アクセス可能に
 * 3. 複数のスコープに跨る参照を最適化
 * 4. クロージャーや変数のシャドーイングを適切に処理
 */
class IdentifierLookupOptimizer : public Transformer {
 public:
  /**
   * @brief コンストラクタ
   */
  IdentifierLookupOptimizer();

  /**
   * @brief ASTノードを最適化する
   * @param node 最適化対象のノード
   * @return 最適化されたノード
   */
  ast::NodePtr transform(const ast::NodePtr& node) override;

  /**
   * @brief 統計情報の収集を有効/無効にする
   * @param enabled 有効にする場合はtrue、無効にする場合はfalse
   */
  void enableStatistics(bool enabled);

  /**
   * @brief 最適化された識別子の数を取得
   * @return 最適化された識別子の数
   */
  size_t getOptimizedIdentifiersCount() const;

  /**
   * @brief 高速検索のヒット数を取得
   * @return 高速検索のヒット数
   */
  size_t getFastLookupHitsCount() const;

  /**
   * @brief スコープ階層の最適化数を取得
   * @return スコープ階層の最適化数
   */
  size_t getScopeHierarchyOptimizationsCount() const;

  /**
   * @brief カウンターをリセットする
   */
  void resetCounters();

 private:
  /**
   * @brief スコープの種類
   */
  enum class ScopeType {
    Global,    ///< グローバルスコープ
    Function,  ///< 関数スコープ
    Block,     ///< ブロックスコープ
    Class      ///< クラススコープ
  };

  /**
   * @brief シンボル情報を格納する構造体
   */
  struct SymbolInfo {
    std::string name;   ///< シンボル名
    ast::NodePtr node;  ///< シンボルが定義されたノード
    std::string kind;   ///< 変数の種類（"var", "let", "const", ""）
    size_t scopeIndex;  ///< シンボルが定義されたスコープのインデックス
  };

  /**
   * @brief スコープ情報を格納する構造体
   */
  struct ScopeInfo {
    ScopeType type;                                       ///< スコープの種類
    int parentIndex;                                      ///< 親スコープのインデックス
    std::unordered_map<std::string, SymbolInfo> symbols;  ///< スコープ内のシンボルマップ
  };

  /**
   * @brief ASTを走査してスコープとシンボル情報を構築
   * @param node ASTのルートノード
   */
  void buildScopeAndSymbolInfo(const ast::NodePtr& node);

  /**
   * @brief 新しいスコープを作成して進入
   * @param type スコープの種類
   * @return 作成したスコープのインデックス
   */
  size_t createAndEnterScope(ScopeType type);

  /**
   * @brief 現在のスコープから抜ける
   */
  void exitScope();

  /**
   * @brief シンボル（変数、関数など）を現在のスコープに登録
   * @param name シンボル名
   * @param node シンボルが定義されたノード
   * @param kind 変数の種類（"var", "let", "const"）
   */
  void registerSymbol(const std::string& name, const ast::NodePtr& node, const std::string& kind = "");

  /**
   * @brief パターン（オブジェクトパターン、配列パターンなど）から識別子を抽出して登録
   * @param node パターンノード
   * @param kind 変数の種類（"var", "let", "const"）
   */
  void extractAndRegisterPatternIdentifiers(const ast::NodePtr& node, const std::string& kind = "");

  /**
   * @brief スコープベースの最適化を適用
   * @param node 最適化対象のノード
   * @return 最適化されたノード
   */
  ast::NodePtr applyOptimizations(const ast::NodePtr& node);

  /**
   * @brief 特定のノードに最適化を適用
   * @param node 最適化対象のノード
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeNode(const ast::NodePtr& node);

  /**
   * @brief 識別子に対応するシンボル情報を解決
   * @param name 識別子名
   * @return 見つかったシンボル情報へのポインタ、または見つからない場合はnullptr
   */
  SymbolInfo* resolveSymbol(const std::string& name);

  /**
   * @brief 特定のスコープの深さを計算
   * @param scopeIndex スコープのインデックス
   * @return 計算されたスコープの深さ
   */
  int calculateScopeDepth(size_t scopeIndex);

  // スコープ情報
  std::vector<ScopeInfo> scopes_;  ///< スコープの配列
  std::vector<size_t> scopePath_;  ///< 現在のスコープパス
  size_t currentScopeIndex_;       ///< 現在のスコープインデックス

  // 統計情報
  bool statisticsEnabled_;                   ///< 統計情報が有効かどうか
  size_t optimizedIdentifiersCount_;         ///< 最適化された識別子の数
  size_t fastLookupHitsCount_;               ///< 高速検索のヒット数
  size_t scopeHierarchyOptimizationsCount_;  ///< スコープ階層の最適化数
};

}  // namespace transformers
}  // namespace aero

#endif  // AERO_TRANSFORMERS_IDENTIFIER_LOOKUP_OPTIMIZATION_H_