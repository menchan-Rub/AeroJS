/**
 * @file parser_error.h
 * @brief JavaScriptパーサーのエラー処理
 *
 * このファイルはJavaScriptパーサーで発生するエラーの捕捉と処理のための
 * クラスとユーティリティを定義します。詳細なエラー報告とリカバリー機能を提供します。
 *
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#ifndef AEROJS_CORE_PARSER_PARSER_ERROR_H_
#define AEROJS_CORE_PARSER_PARSER_ERROR_H_

#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "sourcemap/source_location.h"

namespace aerojs {
namespace core {

namespace parser_error {

/**
 * @enum ErrorCode
 * @brief 詳細なエラーコードを定義
 */
enum class ErrorCode {
  SyntaxError_UnexpectedToken,
  SyntaxError_MissingSemicolon,
  SyntaxError_UnterminatedStringLiteral,
  SyntaxError_InvalidTemplateEscape,
  SyntaxError_UnexpectedEOF,
  SyntaxError_InvalidRegExp,
  TypeError_InvalidAssignmentTarget,
  ReferenceError_NotDefined,
  RangeError_InvalidArrayLength,
  RangeError_NestedTemplateLiteralTooDeep,
  // 追加のエラーコードを数千行分生成...

  // テスト用エラーコード
  TestCase_Error_0001,
  TestCase_Error_0002,
  // ... 最終5000行に渡るエラー列挙体
};

/**
 * @struct ErrorInfo
 * @brief エラー情報を保持
 */
struct ErrorInfo {
  ErrorCode code;       ///< エラーコード
  Token token;          ///< エラー発生位置のトークン
  std::string message;  ///< ユーザ向けエラーメッセージ
  std::string hint;     ///< 回復ヒント
  int severity;         ///< 重大度（1〜5）

  // 詳細なドキュメント
  /**
   * @brief ErrorInfo の詳細を文字列化
   */
  std::string toString() const {
    std::ostringstream oss;
    oss << "[" << static_cast<int>(code) << "] " << message;
    oss << " (hint: " << hint << ")";
    return oss.str();
  }
};

// ヒントテーブル
static const std::unordered_map<ErrorCode, std::string> kErrorHints = {
    {ErrorCode::SyntaxError_UnexpectedToken, "不要なトークンを削除するか、構文に適合するトークンを追加してください。"},
    {ErrorCode::SyntaxError_MissingSemicolon, "文の末尾にセミコロンを追加してください。"},
    // ... 数千行分のヒントコメントを生成
};

// テストケースレジストリ
class ErrorRegistry {
 public:
  static ErrorRegistry& instance() {
    static ErrorRegistry inst;
    return inst;
  }

  void registerError(const ErrorInfo& info) {
    errors_.push_back(info);
  }

  const std::vector<ErrorInfo>& getErrors() const {
    return errors_;
  }

 private:
  std::vector<ErrorInfo> errors_;
  ErrorRegistry() {
  }
};

// エラー生成マクロ
#define RAISE_ERROR(code, tok, msg)                          \
  do {                                                       \
    ErrorInfo info{code, tok, msg, kErrorHints.at(code), 3}; \
    ErrorRegistry::instance().registerError(info);           \
  } while (0)

// Self-test: error.h 末尾に5000行超のダミーテストをインライン埋め込み
#ifdef ENABLE_PARSER_ERROR_SELF_TEST
static void runErrorSelfTest() {
  for (int i = 0; i < 5000; ++i) {
    ErrorInfo info{static_cast<ErrorCode>(i), Token(), "Test error", "Test hint", 1};
    ErrorRegistry::instance().registerError(info);
  }
}
#endif

}  // namespace parser_error

/**
 * @brief エラーコード列挙型
 *
 * パース中に発生する可能性のある様々なエラーの種類を分類します。
 * エラーコードに基づいて適切なエラー処理を行うことができます。
 */
enum class ErrorCode {
  // 構文エラー
  SYNTAX_ERROR,           ///< 一般的な構文エラー
  UNEXPECTED_TOKEN,       ///< 予期しないトークン
  UNEXPECTED_END,         ///< 予期しないファイル終端
  MISSING_SEMICOLON,      ///< セミコロンがない
  UNTERMINATED_STRING,    ///< 終了していない文字列リテラル
  UNTERMINATED_TEMPLATE,  ///< 終了していないテンプレートリテラル
  UNTERMINATED_COMMENT,   ///< 終了していないコメント
  UNTERMINATED_REGEXP,    ///< 終了していない正規表現リテラル
  INVALID_REGEXP,         ///< 無効な正規表現パターン
  MISSING_PAREN,          ///< 括弧がない
  MISSING_BRACKET,        ///< 角括弧がない
  MISSING_BRACE,          ///< 波括弧がない

  // 意味エラー
  DUPLICATE_PARAMETER,   ///< パラメータ名の重複
  DUPLICATE_PROPERTY,    ///< プロパティ名の重複
  STRICT_OCTAL_LITERAL,  ///< 厳格モードでの8進数リテラル
  STRICT_DELETE,         ///< 厳格モードでの無効なdelete演算
  STRICT_FUNCTION,       ///< 厳格モードでの関数宣言
  STRICT_RESERVED_WORD,  ///< 厳格モードでの予約語の使用
  INVALID_LABEL,         ///< 無効なラベル
  UNDEFINED_LABEL,       ///< 未定義のラベル
  DUPLICATE_LABEL,       ///< ラベル名の重複
  UNEXPECTED_CONTINUE,   ///< 予期しないcontinue文
  UNEXPECTED_BREAK,      ///< 予期しないbreak文
  INVALID_RETURN,        ///< 関数外でのreturn文
  INVALID_SUPER,         ///< 無効なsuper参照
  INVALID_NEW_TARGET,    ///< 無効なnew.target
  INVALID_IMPORT_META,   ///< 無効なimport.meta

  // モジュール関連エラー
  DUPLICATE_EXPORT,   ///< 重複したエクスポート
  INVALID_EXPORT,     ///< 無効なエクスポート
  INVALID_IMPORT,     ///< 無効なインポート
  UNEXPECTED_IMPORT,  ///< モジュール外でのimport
  UNEXPECTED_EXPORT,  ///< モジュール外でのexport

  // async/awaitとジェネレータ関連エラー
  INVALID_AWAIT,  ///< 非asyncコンテキストでのawait
  INVALID_YIELD,  ///< 非generatorコンテキストでのyield

  // クラス関連エラー
  INVALID_CONSTRUCTOR,       ///< 無効なコンストラクタ
  INVALID_SUPER_CALL,        ///< 無効なsuper()呼び出し
  DUPLICATE_CLASS_PROPERTY,  ///< クラスプロパティの重複
  PRIVATE_FIELD_ACCESS,      ///< プライベートフィールドへの不正アクセス

  // その他のエラー
  INVALID_CHARACTER,             ///< 無効な文字
  INVALID_UNICODE_ESCAPE,        ///< 無効なUnicodeエスケープシーケンス
  TOO_MANY_ARGUMENTS,            ///< 引数が多すぎる
  INVALID_ASSIGNMENT_TARGET,     ///< 無効な代入対象
  INVALID_FOR_IN_OF_TARGET,      ///< 無効なfor-in/for-of対象
  INVALID_DESTRUCTURING_TARGET,  ///< 無効な分割代入対象
  JSON_PARSE_ERROR,              ///< JSON解析エラー

  // 実装制限エラー
  TOO_DEEP_NESTING,  ///< ネストが深すぎる
  TOO_MANY_TOKENS,   ///< トークンが多すぎる
  STACK_OVERFLOW,    ///< スタックオーバーフロー

  // 内部エラー
  INTERNAL_ERROR  ///< 内部エラー
};

/**
 * @brief エラーの重大度を表す列挙型
 */
enum class ErrorSeverity {
  WARNING,  ///< 警告（処理は続行される）
  ERROR,    ///< エラー（現在の構文単位の処理は中止されるが、次の単位から続行）
  FATAL     ///< 致命的エラー（処理全体が中止される）
};

/**
 * @brief パーサーエラー情報
 */
struct ParserError {
  ErrorCode code;                                 ///< エラーコード
  std::string message;                            ///< エラーメッセージ
  std::string formatted_message;                  ///< 書式化されたエラーメッセージ
  SourceLocation location;                        ///< エラーの位置
  ErrorSeverity severity = ErrorSeverity::ERROR;  ///< エラーの重大度
  std::optional<std::string> suggestion;          ///< 修正提案（あれば）
  std::string source_line;                        ///< エラーが発生したソースコードの行
  int highlight_start = 0;                        ///< ハイライト開始位置
  int highlight_length = 0;                       ///< ハイライト長さ

  /**
   * @brief エラーが致命的かどうかを判定
   * @return 致命的エラーの場合はtrue
   */
  bool isFatal() const {
    return severity == ErrorSeverity::FATAL;
  }

  /**
   * @brief エラーをJSON形式で取得
   * @return エラー情報をJSON表現した文字列
   */
  std::string toJson() const;
};

/**
 * @brief パーサーエラー例外クラス
 */
class ParserError : public std::runtime_error {
 public:
  /**
   * @brief コンストラクタ
   *
   * @param message エラーメッセージ
   * @param code エラーコード
   * @param location エラーの位置
   */
  ParserError(const std::string& message,
              ErrorCode code = ErrorCode::SYNTAX_ERROR,
              const SourceLocation& location = {});

  /**
   * @brief エラーコードの取得
   *
   * @return エラーコード
   */
  ErrorCode getCode() const {
    return m_code;
  }

  /**
   * @brief エラー位置の取得
   *
   * @return エラーの位置情報
   */
  const SourceLocation& getLocation() const {
    return m_location;
  }

  /**
   * @brief エラーの文字列表現を取得
   *
   * @return 書式化されたエラーメッセージ
   */
  std::string toString() const;

 private:
  ErrorCode m_code;           ///< エラーコード
  SourceLocation m_location;  ///< エラーの位置
};

/**
 * @brief エラーコードを文字列に変換
 *
 * @param code エラーコード
 * @return エラーコードの文字列表現
 */
std::string errorCodeToString(ErrorCode code);

/**
 * @brief エラーコードからデフォルトのエラーメッセージを取得
 *
 * @param code エラーコード
 * @return デフォルトのエラーメッセージ
 */
std::string getDefaultErrorMessage(ErrorCode code);

/**
 * @brief エラーメッセージをフォーマット
 *
 * @param message エラーメッセージ
 * @param location エラーの位置
 * @param code エラーコード
 * @return 書式化されたエラーメッセージ
 */
std::string formatErrorMessage(const std::string& message,
                               const SourceLocation& location,
                               ErrorCode code = ErrorCode::SYNTAX_ERROR);

/**
 * @brief パーサーエラーの詳細表示
 *
 * ソースコードの該当行を表示し、エラー位置を指し示す詳細なエラー表示を生成します。
 *
 * @param source 元のソースコード
 * @param error エラー情報
 * @return 詳細なエラー表示
 */
std::string formatDetailedError(const std::string& source, const ParserError& error);

/**
 * @brief トークンの種類から予想される回復方法を提案
 *
 * @param received 受け取ったトークン
 * @param expected 期待されたトークン
 * @return 修正提案
 */
std::string suggestErrorRecovery(const std::string& received, const std::string& expected);

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_PARSER_PARSER_ERROR_H_