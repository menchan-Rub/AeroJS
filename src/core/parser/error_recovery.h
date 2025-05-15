/**
 * @file error_recovery.h
 * @brief パーサーのエラーリカバリー機能の定義
 *
 * パース中のエラーからの復帰とエラーコンテキストの提供機能を定義します。
 * IDE統合のためのエラーレポートと修正提案機能も含みます。
 *
 * @copyright Copyright (c) 2024 AeroJS プロジェクト
 * @license MIT License
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>

#include "lexer/token/token.h"
#include "parser_error.h"
#include "sourcemap/source_location.h"

namespace aero {
namespace parser {

/**
 * @brief エラー修正案の種類
 */
enum class FixKind {
  INSERT,       ///< トークンの挿入
  DELETE,       ///< トークンの削除
  REPLACE,      ///< トークンの置換
  WRAP,         ///< トークンの囲い込み
  INDENT,       ///< インデントの調整
  MOVE,         ///< トークンの移動
  SMART_FIX     ///< 複合的な修正
};

/**
 * @brief エラー修正案を表す構造体
 */
struct ErrorFix {
  FixKind kind;                    ///< 修正の種類
  SourceLocation location;         ///< 修正の位置情報
  std::string message;             ///< 修正内容の説明
  std::string replacementText;     ///< 置換するテキスト
  double confidence;               ///< 修正の確信度（0.0〜1.0）
  std::vector<std::string> tags;   ///< 修正のタグ
};

/**
 * @brief エラーの重要度
 */
enum class ErrorSeverity {
  HINT,       ///< ヒント
  INFO,       ///< 情報
  WARNING,    ///< 警告
  ERROR,      ///< エラー
  FATAL       ///< 致命的エラー
};

/**
 * @brief エラーをグループ化するカテゴリ
 */
enum class ErrorCategory {
  SYNTAX,           ///< 構文エラー
  SEMANTIC,         ///< 意味エラー
  TYPE,             ///< 型エラー
  REFERENCE,        ///< 参照エラー
  DECLARATION,      ///< 宣言エラー
  STYLE,            ///< スタイルエラー
  PERFORMANCE,      ///< パフォーマンスエラー
  BEST_PRACTICE     ///< ベストプラクティス
};

/**
 * @brief 詳細なエラー情報を表す構造体
 */
struct EnhancedErrorInfo : public parser_error::ErrorInfo {
  std::vector<ErrorFix> fixes;                  ///< 修正案
  std::string code;                             ///< エラーコード
  ErrorCategory category;                       ///< エラーカテゴリ
  std::string helpUrl;                          ///< ヘルプURL
  std::vector<std::string> relatedTokens;       ///< 関連するトークン
  std::vector<SourceLocation> relatedLocations; ///< 関連する位置情報
  
  /**
   * @brief コンストラクタ
   */
  EnhancedErrorInfo(
      const SourceLocation& location = {},
      const std::string& message = "",
      ErrorSeverity severity = ErrorSeverity::ERROR)
      : parser_error::ErrorInfo{location, message, 
                                static_cast<parser_error::ErrorSeverity>(severity)},
        category(ErrorCategory::SYNTAX) {}
  
  /**
   * @brief 基本エラー情報から拡張エラー情報を作成
   */
  static EnhancedErrorInfo fromErrorInfo(const parser_error::ErrorInfo& info) {
    return EnhancedErrorInfo{
        info.location,
        info.message,
        static_cast<ErrorSeverity>(info.severity)
    };
  }
};

/**
 * @brief パニックモード・エラーリカバリー戦略
 */
enum class RecoveryStrategy {
  SKIP_TOKEN,          ///< 現在のトークンをスキップ
  SKIP_TO_STATEMENT,   ///< 次のステートメントまでスキップ
  SKIP_TO_DELIMITER,   ///< 次の区切り文字までスキップ
  INSERT_TOKEN,        ///< トークンを挿入
  SYNCHRONIZE,         ///< 同期ポイントまでスキップ
  BACKTRACK            ///< バックトラック
};

/**
 * @brief エラーリカバリーマネージャー
 */
class ErrorRecoveryManager {
public:
  /**
   * @brief コンストラクタ
   */
  ErrorRecoveryManager();
  
  /**
   * @brief デストラクタ
   */
  ~ErrorRecoveryManager();
  
  /**
   * @brief エラーを記録する
   * 
   * @param info エラー情報
   */
  void recordError(const EnhancedErrorInfo& info);
  
  /**
   * @brief エラーリカバリー戦略を決定する
   * 
   * @param currentToken 現在のトークン
   * @param expectedTokens 期待されるトークン
   * @param context パース文脈
   * @return RecoveryStrategy 回復戦略
   */
  RecoveryStrategy determineStrategy(
      const lexer::Token& currentToken,
      const std::vector<lexer::TokenType>& expectedTokens,
      const std::string& context);
  
  /**
   * @brief リカバリーに使用するトークンを取得する
   * 
   * @param strategy リカバリー戦略
   * @param currentToken 現在のトークン
   * @param expectedTokens 期待されるトークン
   * @return std::optional<lexer::Token> 挿入するトークン
   */
  std::optional<lexer::Token> getRecoveryToken(
      RecoveryStrategy strategy,
      const lexer::Token& currentToken,
      const std::vector<lexer::TokenType>& expectedTokens);
  
  /**
   * @brief エラー情報を取得する
   * 
   * @return const std::vector<EnhancedErrorInfo>& エラー情報リスト
   */
  const std::vector<EnhancedErrorInfo>& getErrors() const;
  
  /**
   * @brief すべてのエラーに対する修正案を生成する
   * 
   * @param source ソースコード
   * @return std::vector<ErrorFix> 修正案リスト
   */
  std::vector<ErrorFix> generateFixes(const std::string& source) const;
  
  /**
   * @brief エラーの詳細診断を実行する
   * 
   * @param source ソースコード
   */
  void diagnoseErrors(const std::string& source);
  
  /**
   * @brief エラーを修正したソースコードを生成する
   * 
   * @param source 元のソースコード
   * @param fixIndices 適用する修正案のインデックス
   * @return std::string 修正後のソースコード
   */
  std::string applyFixes(const std::string& source, const std::vector<size_t>& fixIndices = {}) const;
  
  /**
   * @brief エラーをクリアする
   */
  void clearErrors();
  
  /**
   * @brief パースペナルティを判定する
   * エラー時にどのようなペナルティを与えるかを決定
   * 
   * @param tokenType トークン種別
   * @param context コンテキスト
   * @return int ペナルティ値（高いほど重い）
   */
  int determineParsePenalty(lexer::TokenType tokenType, const std::string& context) const;
  
  /**
   * @brief エラーメッセージをフォーマットする
   * 
   * @param error エラー情報
   * @param source ソースコード
   * @param colorize 色付け有無
   * @return std::string フォーマットされたエラーメッセージ
   */
  std::string formatErrorMessage(
      const EnhancedErrorInfo& error,
      const std::string& source,
      bool colorize = false) const;
  
  /**
   * @brief エラーからの回復位置を検索する
   * 
   * @param tokens トークンリスト
   * @param currentPos 現在位置
   * @return size_t 回復位置
   */
  size_t findRecoveryPosition(
      const std::vector<lexer::Token>& tokens,
      size_t currentPos) const;

private:
  /**
   * @brief トークンのバランスを取る戦略
   * 
   * @param tokens トークンリスト
   * @param position 現在位置
   * @param openToken 開始トークン
   * @param closeToken 終了トークン
   * @return size_t バランスの取れる位置
   */
  size_t balanceTokens(
      const std::vector<lexer::Token>& tokens,
      size_t position,
      lexer::TokenType openToken,
      lexer::TokenType closeToken) const;
  
  /**
   * @brief トークンの関連性を計算する
   * 
   * @param a トークンA
   * @param b トークンB
   * @return double 関連度（0.0〜1.0）
   */
  double calculateTokenRelevance(
      lexer::TokenType a,
      lexer::TokenType b) const;
  
  /**
   * @brief 文脈に応じた修正案を生成する
   * 
   * @param error エラー情報
   * @param source ソースコード
   * @return std::vector<ErrorFix> 修正案リスト
   */
  std::vector<ErrorFix> generateContextualFixes(
      const EnhancedErrorInfo& error,
      const std::string& source) const;
  
  /**
   * @brief 一般的なエラー修正パターンを適用する
   * 
   * @param error エラー情報
   * @param source ソースコード
   * @return std::vector<ErrorFix> 修正案リスト
   */
  std::vector<ErrorFix> applyCommonFixPatterns(
      const EnhancedErrorInfo& error,
      const std::string& source) const;
  
  std::vector<EnhancedErrorInfo> m_errors;          ///< エラー情報
  std::unordered_map<std::string, int> m_errorFrequency; ///< エラー頻度
  
  // エラー診断と修正のためのヒューリスティック
  struct ErrorPattern {
    std::string pattern;          ///< エラーパターン
    std::string message;          ///< エラーメッセージ
    std::function<std::vector<ErrorFix>(const EnhancedErrorInfo&, const std::string&)> fixGenerator;  ///< 修正生成関数
  };
  std::vector<ErrorPattern> m_errorPatterns;  ///< エラーパターン
};

} // namespace parser
} // namespace aero 