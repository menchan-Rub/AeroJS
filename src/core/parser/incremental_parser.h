/**
 * @file incremental_parser.h
 * @brief 高性能インクリメンタルJavaScriptパーサーの定義
 *
 * エディタ統合やIDEなど、ソースコードが頻繁に変更される環境向けに最適化された
 * インクリメンタルパーサーを提供します。既存のASTとソースコードの差分に基づいて
 * 効率的に再解析を行います。
 *
 * @copyright Copyright (c) 2024 AeroJS プロジェクト
 * @license MIT License
 */

#pragma once

#include <memory>
#include <string>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>

#include "parser.h"
#include "ast/node.h"
#include "sourcemap/source_location.h"

namespace aero {
namespace parser {

/**
 * @brief ソースコード変更を表す構造体
 */
struct SourceEdit {
  size_t start;          ///< 変更開始位置
  size_t end;            ///< 変更終了位置
  std::string newText;   ///< 新しいテキスト
  
  /**
   * @brief 変更サイズの計算
   * @return int 変更サイズ（正: 増加、負: 減少）
   */
  int getDelta() const {
    return static_cast<int>(newText.size()) - static_cast<int>(end - start);
  }
};

/**
 * @brief パース結果を表す構造体
 */
struct ParseResult {
  std::unique_ptr<ast::Node> ast;              ///< 解析結果のAST
  std::vector<lexer::Token> tokens;            ///< トークンリスト
  std::vector<parser_error::ErrorInfo> errors; ///< エラー情報
  std::chrono::microseconds parseTime;         ///< 解析時間
  bool isPartial = false;                      ///< 部分的な解析結果かどうか
};

/**
 * @brief インクリメンタルパーサーのオプション
 */
struct IncrementalParserOptions {
  bool enableCaching = true;                   ///< キャッシュを有効にする
  bool reuseAst = true;                        ///< 可能であればASTを再利用する
  bool tolerantMode = true;                    ///< エラーに寛容なモード
  size_t maxCacheSize = 50;                    ///< 最大キャッシュサイズ
  bool collectComments = true;                 ///< コメントを収集する
  std::chrono::milliseconds maxParseTime{500}; ///< 最大解析時間
};

/**
 * @brief インクリメンタルパーサークラス
 */
class IncrementalParser {
public:
  /**
   * @brief コンストラクタ
   * @param options パーサーオプション
   */
  explicit IncrementalParser(const IncrementalParserOptions& options = IncrementalParserOptions());
  
  /**
   * @brief デストラクタ
   */
  ~IncrementalParser();
  
  /**
   * @brief ソースコードを解析する
   * 
   * @param source ソースコード
   * @param filename ファイル名
   * @return ParseResult 解析結果
   */
  ParseResult parse(const std::string& source, const std::string& filename = "input.js");
  
  /**
   * @brief インクリメンタルにソースコードを解析する
   * 
   * @param edit ソースコード編集情報
   * @param filename ファイル名
   * @return ParseResult 解析結果
   */
  ParseResult parseIncremental(const SourceEdit& edit, const std::string& filename = "input.js");
  
  /**
   * @brief 複数の変更を適用して解析する
   * 
   * @param edits ソースコード編集情報のリスト
   * @param filename ファイル名
   * @return ParseResult 解析結果
   */
  ParseResult parseIncrementalBatch(const std::vector<SourceEdit>& edits, const std::string& filename = "input.js");
  
  /**
   * @brief 現在のソースコードを取得する
   * 
   * @return const std::string& 現在のソースコード
   */
  const std::string& getCurrentSource() const;
  
  /**
   * @brief 現在のASTを取得する
   * 
   * @return const ast::Node* 現在のAST
   */
  const ast::Node* getCurrentAst() const;
  
  /**
   * @brief パーサーをリセットする
   */
  void reset();
  
  /**
   * @brief キャッシュをクリアする
   */
  void clearCache();
  
  /**
   * @brief オプションを設定する
   * 
   * @param options パーサーオプション
   */
  void setOptions(const IncrementalParserOptions& options);
  
  /**
   * @brief オプションを取得する
   * 
   * @return const IncrementalParserOptions& パーサーオプション
   */
  const IncrementalParserOptions& getOptions() const;
  
  /**
   * @brief 統計情報を取得する
   * 
   * @return JSON形式の統計情報
   */
  std::string getStats() const;
  
  /**
   * @brief ノード範囲内のソースコードを取得する
   * 
   * @param node ASTノード
   * @return std::string ノード範囲内のソースコード
   */
  std::string getNodeSource(const ast::Node* node) const;
  
  /**
   * @brief 位置に対応するノードを取得する
   * 
   * @param offset ソースコードのオフセット
   * @return const ast::Node* 対応するノード
   */
  const ast::Node* getNodeAtOffset(size_t offset) const;
  
  /**
   * @brief 範囲に対応するノードを取得する
   * 
   * @param start 開始オフセット
   * @param end 終了オフセット
   * @return std::vector<const ast::Node*> 対応するノードのリスト
   */
  std::vector<const ast::Node*> getNodesInRange(size_t start, size_t end) const;

private:
  /**
   * @brief インクリメンタル解析の準備を行う
   * 
   * @param edit 編集情報
   * @return bool 準備成功時はtrue
   */
  bool prepareIncremental(const SourceEdit& edit);
  
  /**
   * @brief 変更によって影響を受けるノードを特定する
   * 
   * @param edit 編集情報
   * @return std::vector<ast::Node*> 影響を受けるノードのリスト
   */
  std::vector<ast::Node*> findAffectedNodes(const SourceEdit& edit);
  
  /**
   * @brief ノードの位置情報を更新する
   * 
   * @param node 更新するノード
   * @param delta 位置の変化量
   * @param editEnd 編集の終了位置
   */
  void updateNodeLocations(ast::Node* node, int delta, size_t editEnd);
  
  /**
   * @brief 部分的な再解析を行う
   * 
   * @param affectedNodes 影響を受けるノード
   * @param edit 編集情報
   * @return bool 再解析成功時はtrue
   */
  bool reparseNodes(const std::vector<ast::Node*>& affectedNodes, const SourceEdit& edit);
  
  /**
   * @brief 現在のソースの全体を再解析する
   * 
   * @return ParseResult 解析結果
   */
  ParseResult fullReparse();
  
  /**
   * @brief キャッシュから結果を取得する
   * 
   * @param source ソースコード
   * @param filename ファイル名
   * @return std::optional<ParseResult> キャッシュされた結果（存在しない場合はnullopt）
   */
  std::optional<ParseResult> getFromCache(const std::string& source, const std::string& filename);
  
  /**
   * @brief 結果をキャッシュに追加する
   * 
   * @param source ソースコード
   * @param filename ファイル名
   * @param result 解析結果
   */
  void addToCache(const std::string& source, const std::string& filename, const ParseResult& result);

  // メンバ変数
  IncrementalParserOptions m_options;                ///< パーサーオプション
  std::string m_currentSource;                       ///< 現在のソースコード
  std::string m_currentFilename;                     ///< 現在のファイル名
  std::unique_ptr<ast::Node> m_currentAst;           ///< 現在のAST
  std::vector<lexer::Token> m_currentTokens;         ///< 現在のトークンリスト
  std::vector<parser_error::ErrorInfo> m_currentErrors; ///< 現在のエラー情報
  
  // キャッシュ
  struct CacheEntry {
    ParseResult result;
    std::chrono::system_clock::time_point timestamp;
  };
  std::unordered_map<std::string, CacheEntry> m_cache; ///< キャッシュ
  
  // 統計情報
  struct Stats {
    size_t totalParses = 0;                ///< 総解析回数
    size_t incrementalParses = 0;          ///< インクリメンタル解析回数
    size_t fullReparses = 0;               ///< 完全再解析回数
    size_t cacheHits = 0;                  ///< キャッシュヒット数
    size_t cacheMisses = 0;                ///< キャッシュミス数
    std::chrono::microseconds totalTime{0}; ///< 総解析時間
    size_t totalEdits = 0;                 ///< 総編集回数
    size_t largestEdit = 0;                ///< 最大編集サイズ
  };
  Stats m_stats;                           ///< 統計情報
  
  // ロガー
  std::function<void(const std::string&)> m_logger; ///< ロガー関数
};

} // namespace parser
} // namespace aero 