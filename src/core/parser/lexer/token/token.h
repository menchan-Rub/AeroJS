/**
 * @file token.h
 * @brief JavaScriptトークン定義の強化版
 * 
 * このファイルはJavaScript言語の字句解析で使用するトークンの種類と
 * トークンデータ構造の拡張定義を提供します。ECMAScript最新仕様に準拠しています。
 */

#ifndef AEROJS_CORE_PARSER_LEXER_TOKEN_TOKEN_H_
#define AEROJS_CORE_PARSER_LEXER_TOKEN_TOKEN_H_

#include <string>
#include <variant>
#include <optional>
#include <unordered_map>
#include "../../sourcemap/source_location.h"

namespace aerojs {
namespace core {

/**
 * @brief トークンの種類を表す列挙型
 * 
 * JavaScript言語で使用されるすべてのトークン種類を定義します。
 * ECMAScript最新仕様に準拠しています。
 */
enum class TokenType {
  // 終端トークン
  END_OF_FILE,       ///< ファイルの終端
  ERROR,             ///< エラートークン
  
  // リテラル
  IDENTIFIER,        ///< 識別子
  NUMERIC_LITERAL,   ///< 数値リテラル
  STRING_LITERAL,    ///< 文字列リテラル
  TEMPLATE_LITERAL,  ///< テンプレートリテラル
  REGEXP_LITERAL,    ///< 正規表現リテラル
  BIGINT_LITERAL,    ///< BigInt リテラル
  NULL_LITERAL,      ///< null リテラル
  BOOLEAN_LITERAL,   ///< 真偽値リテラル (true, false)
  
  // テンプレートリテラル関連
  TEMPLATE_HEAD,     ///< テンプレートリテラルの先頭部分 (`foo${)
  TEMPLATE_MIDDLE,   ///< テンプレートリテラルの中間部分 (}bar${)
  TEMPLATE_TAIL,     ///< テンプレートリテラルの末尾部分 (}baz`)
  
  // キーワード
  AWAIT,             ///< await
  BREAK,             ///< break
  CASE,              ///< case
  CATCH,             ///< catch
  CLASS,             ///< class
  CONST,             ///< const
  CONTINUE,          ///< continue
  DEBUGGER,          ///< debugger
  DEFAULT,           ///< default
  DELETE,            ///< delete
  DO,                ///< do
  ELSE,              ///< else
  ENUM,              ///< enum
  EXPORT,            ///< export
  EXTENDS,           ///< extends
  FALSE,             ///< false
  FINALLY,           ///< finally
  FOR,               ///< for
  FUNCTION,          ///< function
  IF,                ///< if
  IMPORT,            ///< import
  IN,                ///< in
  INSTANCEOF,        ///< instanceof
  LET,               ///< let
  NEW,               ///< new
  NULL,              ///< null
  RETURN,            ///< return
  SUPER,             ///< super
  SWITCH,            ///< switch
  THIS,              ///< this
  THROW,             ///< throw
  TRUE,              ///< true
  TRY,               ///< try
  TYPEOF,            ///< typeof
  VAR,               ///< var
  VOID,              ///< void
  WHILE,             ///< while
  WITH,              ///< with
  YIELD,             ///< yield
  
  // 将来の予約語
  IMPLEMENTS,        ///< implements (future reserved)
  INTERFACE,         ///< interface (future reserved)
  PACKAGE,           ///< package (future reserved)
  PRIVATE,           ///< private (future reserved)
  PROTECTED,         ///< protected (future reserved)
  PUBLIC,            ///< public (future reserved)
  
  // モジュール関連
  AS,                ///< as
  FROM,              ///< from
  OF,                ///< of
  
  // クラス関連
  STATIC,            ///< static
  GET,               ///< get
  SET,               ///< set
  ASYNC,             ///< async
  
  // ECMAScript 2022以降
  ACCESSOR,          ///< accessor (class fields)
  
  // 区切り記号
  SEMICOLON,         ///< ;
  COLON,             ///< :
  QUESTION,          ///< ?
  COMMA,             ///< ,
  DOT,               ///< .
  
  // 括弧
  LEFT_PAREN,        ///< (
  RIGHT_PAREN,       ///< )
  LEFT_BRACE,        ///< {
  RIGHT_BRACE,       ///< }
  LEFT_BRACKET,      ///< [
  RIGHT_BRACKET,     ///< ]
  
  // 演算子
  // 算術演算子
  PLUS,              ///< +
  MINUS,             ///< -
  MULTIPLY,          ///< *
  DIVIDE,            ///< /
  MODULO,            ///< %
  EXPONENTIATION,    ///< **
  
  // インクリメント/デクリメント
  INCREMENT,         ///< ++
  DECREMENT,         ///< --
  
  // 代入演算子
  ASSIGN,            ///< =
  PLUS_ASSIGN,       ///< +=
  MINUS_ASSIGN,      ///< -=
  MULTIPLY_ASSIGN,   ///< *=
  DIVIDE_ASSIGN,     ///< /=
  MODULO_ASSIGN,     ///< %=
  EXPONENT_ASSIGN,   ///< **=
  
  // ビット演算子
  BITWISE_AND,       ///< &
  BITWISE_OR,        ///< |
  BITWISE_XOR,       ///< ^
  BITWISE_NOT,       ///< ~
  LEFT_SHIFT,        ///< <<
  RIGHT_SHIFT,       ///< >>
  UNSIGNED_RIGHT_SHIFT, ///< >>>
  
  // ビット代入演算子
  AND_ASSIGN,        ///< &=
  OR_ASSIGN,         ///< |=
  XOR_ASSIGN,        ///< ^=
  LEFT_SHIFT_ASSIGN, ///< <<=
  RIGHT_SHIFT_ASSIGN, ///< >>=
  UNSIGNED_RIGHT_SHIFT_ASSIGN, ///< >>>=
  
  // 比較演算子
  EQUAL,             ///< ==
  NOT_EQUAL,         ///< !=
  STRICT_EQUAL,      ///< ===
  STRICT_NOT_EQUAL,  ///< !==
  GREATER,           ///< >
  GREATER_EQUAL,     ///< >=
  LESS,              ///< <
  LESS_EQUAL,        ///< <=
  
  // 論理演算子
  LOGICAL_AND,       ///< &&
  LOGICAL_OR,        ///< ||
  LOGICAL_NOT,       ///< !
  
  // Nullish演算子
  NULLISH_COALESCING, ///< ??
  
  // 論理代入演算子
  LOGICAL_AND_ASSIGN, ///< &&=
  LOGICAL_OR_ASSIGN,  ///< ||=
  NULLISH_ASSIGN,     ///< ??=
  
  // その他の演算子
  ARROW,             ///< =>
  ELLIPSIS,          ///< ...
  OPTIONAL_CHAIN,    ///< ?.
  
  // プライベートフィールド
  PRIVATE_IDENTIFIER, ///< #identifier
  
  // コメント（一部のパーサーでは保持）
  SINGLE_LINE_COMMENT, ///< // コメント
  MULTI_LINE_COMMENT   ///< /* コメント */
};

/**
 * @brief リテラル値の型
 * 
 * トークンが持つ値の型を表します。
 */
using TokenValue = std::variant<
  std::monostate,         // 値なし
  std::string,            // 文字列、識別子など
  double,                 // 数値
  bool,                   // 真偽値
  int64_t,                // BigIntの値（簡易表現）
  std::nullptr_t,         // null値
  std::pair<std::string, std::string>  // 正規表現（パターンとフラグ）
>;

/**
 * @brief トークン情報
 * 
 * 字句解析で生成されるトークンの情報を保持します。
 */
class Token {
public:
  /**
   * @brief デフォルトコンストラクタ
   */
  Token() : m_type(TokenType::ERROR) {}
  
  /**
   * @brief パラメータ付きコンストラクタ
   * 
   * @param type トークンの種類
   * @param lexeme トークンの原文
   * @param value トークンの解釈値
   * @param location ソースコード内の位置
   */
  Token(TokenType type, 
        const std::string& lexeme, 
        const TokenValue& value,
        const SourceLocation& location)
    : m_type(type), 
      m_lexeme(lexeme), 
      m_value(value),
      m_location(location) {}
  
  /**
   * @brief トークンの種類を取得
   * 
   * @return トークンの種類
   */
  TokenType getType() const { return m_type; }
  
  /**
   * @brief トークンの原文を取得
   * 
   * @return トークンの原文
   */
  const std::string& getLexeme() const { return m_lexeme; }
  
  /**
   * @brief トークンの値を取得
   * 
   * @return トークンの解釈値
   */
  const TokenValue& getValue() const { return m_value; }
  
  /**
   * @brief トークンの位置情報を取得
   * 
   * @return ソースコード内の位置情報
   */
  const SourceLocation& getLocation() const { return m_location; }
  
  /**
   * @brief トークンが指定の種類かどうか確認
   * 
   * @param type 確認するトークン種類
   * @return 指定の種類の場合はtrue
   */
  bool is(TokenType type) const { return m_type == type; }
  
  /**
   * @brief トークンが指定の種類のいずれかかどうか確認
   * 
   * @param types 確認するトークン種類の配列
   * @return 指定の種類のいずれかの場合はtrue
   */
  template<typename... Types>
  bool isOneOf(Types... types) const {
    return (... || (m_type == types));
  }
  
  /**
   * @brief トークンが識別子かどうか確認
   * 
   * @return 識別子の場合はtrue
   */
  bool isIdentifier() const { return m_type == TokenType::IDENTIFIER; }
  
  /**
   * @brief トークンがキーワードかどうか確認
   * 
   * @return キーワードの場合はtrue
   */
  bool isKeyword() const;
  
  /**
   * @brief トークンが演算子かどうか確認
   * 
   * @return 演算子の場合はtrue
   */
  bool isOperator() const;
  
  /**
   * @brief トークンがリテラルかどうか確認
   * 
   * @return リテラルの場合はtrue
   */
  bool isLiteral() const;
  
  /**
   * @brief トークンの種類を文字列で取得
   * 
   * @return トークン種類の文字列表現
   */
  std::string getTypeName() const;
  
  /**
   * @brief トークンの文字列表現を取得
   * 
   * @return デバッグ用の文字列表現
   */
  std::string toString() const;
  
  /**
   * @brief トークン種類から文字列表現を取得する静的メソッド
   * 
   * @param type トークン種類
   * @return トークン種類の文字列表現
   */
  static std::string getTokenTypeName(TokenType type);

private:
  TokenType m_type;       ///< トークンの種類
  std::string m_lexeme;   ///< トークンの原文
  TokenValue m_value;     ///< トークンの解釈値
  SourceLocation m_location; ///< ソースコード内の位置
  
  static const std::unordered_map<TokenType, std::string> s_tokenTypeNames; ///< トークン種類名のマップ
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_PARSER_LEXER_TOKEN_TOKEN_H_ 