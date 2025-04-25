/**
 * @file token.cpp
 * @brief JavaScriptトークン定義の実装
 *
 * このファイルはJavaScript言語の字句解析で使用するトークンの実装を提供します。
 * ECMAScript最新仕様に準拠しています。
 */

#include "token.h"

#include <iomanip>
#include <sstream>

namespace aerojs {
namespace core {

// トークン種類名のマッピング
const std::unordered_map<TokenType, std::string> Token::s_tokenTypeNames = {
    // 終端トークン
    {TokenType::END_OF_FILE, "END_OF_FILE"},
    {TokenType::ERROR, "ERROR"},

    // リテラル
    {TokenType::IDENTIFIER, "IDENTIFIER"},
    {TokenType::NUMERIC_LITERAL, "NUMERIC_LITERAL"},
    {TokenType::STRING_LITERAL, "STRING_LITERAL"},
    {TokenType::TEMPLATE_LITERAL, "TEMPLATE_LITERAL"},
    {TokenType::REGEXP_LITERAL, "REGEXP_LITERAL"},
    {TokenType::BIGINT_LITERAL, "BIGINT_LITERAL"},
    {TokenType::NULL_LITERAL, "NULL_LITERAL"},
    {TokenType::BOOLEAN_LITERAL, "BOOLEAN_LITERAL"},

    // テンプレートリテラル関連
    {TokenType::TEMPLATE_HEAD, "TEMPLATE_HEAD"},
    {TokenType::TEMPLATE_MIDDLE, "TEMPLATE_MIDDLE"},
    {TokenType::TEMPLATE_TAIL, "TEMPLATE_TAIL"},

    // キーワード
    {TokenType::AWAIT, "AWAIT"},
    {TokenType::BREAK, "BREAK"},
    {TokenType::CASE, "CASE"},
    {TokenType::CATCH, "CATCH"},
    {TokenType::CLASS, "CLASS"},
    {TokenType::CONST, "CONST"},
    {TokenType::CONTINUE, "CONTINUE"},
    {TokenType::DEBUGGER, "DEBUGGER"},
    {TokenType::DEFAULT, "DEFAULT"},
    {TokenType::DELETE, "DELETE"},
    {TokenType::DO, "DO"},
    {TokenType::ELSE, "ELSE"},
    {TokenType::ENUM, "ENUM"},
    {TokenType::EXPORT, "EXPORT"},
    {TokenType::EXTENDS, "EXTENDS"},
    {TokenType::FALSE, "FALSE"},
    {TokenType::FINALLY, "FINALLY"},
    {TokenType::FOR, "FOR"},
    {TokenType::FUNCTION, "FUNCTION"},
    {TokenType::IF, "IF"},
    {TokenType::IMPORT, "IMPORT"},
    {TokenType::IN, "IN"},
    {TokenType::INSTANCEOF, "INSTANCEOF"},
    {TokenType::LET, "LET"},
    {TokenType::NEW, "NEW"},
    {TokenType::NULL, "NULL"},
    {TokenType::RETURN, "RETURN"},
    {TokenType::SUPER, "SUPER"},
    {TokenType::SWITCH, "SWITCH"},
    {TokenType::THIS, "THIS"},
    {TokenType::THROW, "THROW"},
    {TokenType::TRUE, "TRUE"},
    {TokenType::TRY, "TRY"},
    {TokenType::TYPEOF, "TYPEOF"},
    {TokenType::VAR, "VAR"},
    {TokenType::VOID, "VOID"},
    {TokenType::WHILE, "WHILE"},
    {TokenType::WITH, "WITH"},
    {TokenType::YIELD, "YIELD"},

    // 将来の予約語
    {TokenType::IMPLEMENTS, "IMPLEMENTS"},
    {TokenType::INTERFACE, "INTERFACE"},
    {TokenType::PACKAGE, "PACKAGE"},
    {TokenType::PRIVATE, "PRIVATE"},
    {TokenType::PROTECTED, "PROTECTED"},
    {TokenType::PUBLIC, "PUBLIC"},

    // モジュール関連
    {TokenType::AS, "AS"},
    {TokenType::FROM, "FROM"},
    {TokenType::OF, "OF"},

    // クラス関連
    {TokenType::STATIC, "STATIC"},
    {TokenType::GET, "GET"},
    {TokenType::SET, "SET"},
    {TokenType::ASYNC, "ASYNC"},

    // ECMAScript 2022以降
    {TokenType::ACCESSOR, "ACCESSOR"},

    // 区切り記号
    {TokenType::SEMICOLON, "SEMICOLON"},
    {TokenType::COLON, "COLON"},
    {TokenType::QUESTION, "QUESTION"},
    {TokenType::COMMA, "COMMA"},
    {TokenType::DOT, "DOT"},

    // 括弧
    {TokenType::LEFT_PAREN, "LEFT_PAREN"},
    {TokenType::RIGHT_PAREN, "RIGHT_PAREN"},
    {TokenType::LEFT_BRACE, "LEFT_BRACE"},
    {TokenType::RIGHT_BRACE, "RIGHT_BRACE"},
    {TokenType::LEFT_BRACKET, "LEFT_BRACKET"},
    {TokenType::RIGHT_BRACKET, "RIGHT_BRACKET"},

    // 演算子
    // 算術演算子
    {TokenType::PLUS, "PLUS"},
    {TokenType::MINUS, "MINUS"},
    {TokenType::MULTIPLY, "MULTIPLY"},
    {TokenType::DIVIDE, "DIVIDE"},
    {TokenType::MODULO, "MODULO"},
    {TokenType::EXPONENTIATION, "EXPONENTIATION"},

    // インクリメント/デクリメント
    {TokenType::INCREMENT, "INCREMENT"},
    {TokenType::DECREMENT, "DECREMENT"},

    // 代入演算子
    {TokenType::ASSIGN, "ASSIGN"},
    {TokenType::PLUS_ASSIGN, "PLUS_ASSIGN"},
    {TokenType::MINUS_ASSIGN, "MINUS_ASSIGN"},
    {TokenType::MULTIPLY_ASSIGN, "MULTIPLY_ASSIGN"},
    {TokenType::DIVIDE_ASSIGN, "DIVIDE_ASSIGN"},
    {TokenType::MODULO_ASSIGN, "MODULO_ASSIGN"},
    {TokenType::EXPONENT_ASSIGN, "EXPONENT_ASSIGN"},

    // ビット演算子
    {TokenType::BITWISE_AND, "BITWISE_AND"},
    {TokenType::BITWISE_OR, "BITWISE_OR"},
    {TokenType::BITWISE_XOR, "BITWISE_XOR"},
    {TokenType::BITWISE_NOT, "BITWISE_NOT"},
    {TokenType::LEFT_SHIFT, "LEFT_SHIFT"},
    {TokenType::RIGHT_SHIFT, "RIGHT_SHIFT"},
    {TokenType::UNSIGNED_RIGHT_SHIFT, "UNSIGNED_RIGHT_SHIFT"},

    // ビット代入演算子
    {TokenType::AND_ASSIGN, "AND_ASSIGN"},
    {TokenType::OR_ASSIGN, "OR_ASSIGN"},
    {TokenType::XOR_ASSIGN, "XOR_ASSIGN"},
    {TokenType::LEFT_SHIFT_ASSIGN, "LEFT_SHIFT_ASSIGN"},
    {TokenType::RIGHT_SHIFT_ASSIGN, "RIGHT_SHIFT_ASSIGN"},
    {TokenType::UNSIGNED_RIGHT_SHIFT_ASSIGN, "UNSIGNED_RIGHT_SHIFT_ASSIGN"},

    // 比較演算子
    {TokenType::EQUAL, "EQUAL"},
    {TokenType::NOT_EQUAL, "NOT_EQUAL"},
    {TokenType::STRICT_EQUAL, "STRICT_EQUAL"},
    {TokenType::STRICT_NOT_EQUAL, "STRICT_NOT_EQUAL"},
    {TokenType::GREATER, "GREATER"},
    {TokenType::GREATER_EQUAL, "GREATER_EQUAL"},
    {TokenType::LESS, "LESS"},
    {TokenType::LESS_EQUAL, "LESS_EQUAL"},

    // 論理演算子
    {TokenType::LOGICAL_AND, "LOGICAL_AND"},
    {TokenType::LOGICAL_OR, "LOGICAL_OR"},
    {TokenType::LOGICAL_NOT, "LOGICAL_NOT"},

    // Nullish演算子
    {TokenType::NULLISH_COALESCING, "NULLISH_COALESCING"},

    // 論理代入演算子
    {TokenType::LOGICAL_AND_ASSIGN, "LOGICAL_AND_ASSIGN"},
    {TokenType::LOGICAL_OR_ASSIGN, "LOGICAL_OR_ASSIGN"},
    {TokenType::NULLISH_ASSIGN, "NULLISH_ASSIGN"},

    // その他の演算子
    {TokenType::ARROW, "ARROW"},
    {TokenType::ELLIPSIS, "ELLIPSIS"},
    {TokenType::OPTIONAL_CHAIN, "OPTIONAL_CHAIN"},

    // プライベートフィールド
    {TokenType::PRIVATE_IDENTIFIER, "PRIVATE_IDENTIFIER"},

    // コメント
    {TokenType::SINGLE_LINE_COMMENT, "SINGLE_LINE_COMMENT"},
    {TokenType::MULTI_LINE_COMMENT, "MULTI_LINE_COMMENT"}};

// キーワードチェック
bool Token::isKeyword() const {
  // キーワードトークンタイプの範囲チェック
  return m_type >= TokenType::AWAIT && m_type <= TokenType::PUBLIC;
}

// 演算子チェック
bool Token::isOperator() const {
  // 演算子の範囲チェック（算術、比較、論理、ビット演算子など）
  return (m_type >= TokenType::PLUS && m_type <= TokenType::OPTIONAL_CHAIN) &&
         m_type != TokenType::SEMICOLON &&
         m_type != TokenType::COLON &&
         m_type != TokenType::QUESTION &&
         m_type != TokenType::COMMA &&
         m_type != TokenType::DOT &&
         m_type != TokenType::LEFT_PAREN &&
         m_type != TokenType::RIGHT_PAREN &&
         m_type != TokenType::LEFT_BRACE &&
         m_type != TokenType::RIGHT_BRACE &&
         m_type != TokenType::LEFT_BRACKET &&
         m_type != TokenType::RIGHT_BRACKET;
}

// リテラルチェック
bool Token::isLiteral() const {
  return m_type == TokenType::NUMERIC_LITERAL ||
         m_type == TokenType::STRING_LITERAL ||
         m_type == TokenType::TEMPLATE_LITERAL ||
         m_type == TokenType::REGEXP_LITERAL ||
         m_type == TokenType::BIGINT_LITERAL ||
         m_type == TokenType::NULL_LITERAL ||
         m_type == TokenType::BOOLEAN_LITERAL;
}

// トークン種類名の取得
std::string Token::getTypeName() const {
  return getTokenTypeName(m_type);
}

// 静的メソッド：トークン種類名の取得
std::string Token::getTokenTypeName(TokenType type) {
  auto it = s_tokenTypeNames.find(type);
  if (it != s_tokenTypeNames.end()) {
    return it->second;
  }
  return "UNKNOWN_TOKEN_TYPE";
}

// トークンの文字列表現の取得
std::string Token::toString() const {
  std::ostringstream oss;

  // 基本情報
  oss << "Token{type=" << getTypeName() << ", lexeme='";

  // lexemeのエスケープ処理
  for (char c : m_lexeme) {
    if (c == '\n')
      oss << "\\n";
    else if (c == '\r')
      oss << "\\r";
    else if (c == '\t')
      oss << "\\t";
    else if (c == '\\')
      oss << "\\\\";
    else if (c == '\'')
      oss << "\\'";
    else
      oss << c;
  }

  // 位置情報の追加
  oss << "', location=["
      << m_location.line << ":"
      << m_location.column << "], ";

  // 値の追加（型に応じて適切なフォーマット）
  oss << "value=";
  if (std::holds_alternative<std::monostate>(m_value)) {
    oss << "null";
  } else if (std::holds_alternative<std::string>(m_value)) {
    oss << "'" << std::get<std::string>(m_value) << "'";
  } else if (std::holds_alternative<double>(m_value)) {
    oss << std::fixed << std::setprecision(15) << std::get<double>(m_value);
    // 整数値の場合は小数点以下を表示しない
    std::string numStr = oss.str();
    size_t valuePos = numStr.find("value=") + 6;
    size_t dotPos = numStr.find('.', valuePos);
    if (dotPos != std::string::npos) {
      size_t nonZeroPos = numStr.find_first_not_of('0', dotPos + 1);
      if (nonZeroPos == std::string::npos || numStr[nonZeroPos] == '}') {
        numStr.erase(dotPos, nonZeroPos - dotPos);
        oss.str(numStr);
      }
    }
  } else if (std::holds_alternative<bool>(m_value)) {
    oss << (std::get<bool>(m_value) ? "true" : "false");
  } else if (std::holds_alternative<int64_t>(m_value)) {
    oss << std::get<int64_t>(m_value) << "n";  // BigInt表記
  } else if (std::holds_alternative<std::nullptr_t>(m_value)) {
    oss << "null";
  } else if (std::holds_alternative<std::pair<std::string, std::string>>(m_value)) {
    auto [pattern, flags] = std::get<std::pair<std::string, std::string>>(m_value);
    oss << "/" << pattern << "/" << flags;
  }

  oss << "}";
  return oss.str();
}

}  // namespace core
}  // namespace aerojs