/**
 * @file lexer.cpp
 * @brief JavaScript字句解析器の実装
 * @version 0.1.0
 * @license MIT
 */

#include "lexer.h"
#include "scanner/scanner.h"
#include "token/token.h"

#include <cctype>
#include <unordered_map>
#include <stdexcept>

namespace aerojs {
namespace core {
namespace parser {

// キーワードテーブル
static const std::unordered_map<std::string, TokenType> keywords = {
    {"abstract", TokenType::ABSTRACT},
    {"arguments", TokenType::ARGUMENTS},
    {"await", TokenType::AWAIT},
    {"boolean", TokenType::BOOLEAN},
    {"break", TokenType::BREAK},
    {"byte", TokenType::BYTE},
    {"case", TokenType::CASE},
    {"catch", TokenType::CATCH},
    {"char", TokenType::CHAR},
    {"class", TokenType::CLASS},
    {"const", TokenType::CONST},
    {"continue", TokenType::CONTINUE},
    {"debugger", TokenType::DEBUGGER},
    {"default", TokenType::DEFAULT},
    {"delete", TokenType::DELETE},
    {"do", TokenType::DO},
    {"double", TokenType::DOUBLE},
    {"else", TokenType::ELSE},
    {"enum", TokenType::ENUM},
    {"eval", TokenType::EVAL},
    {"export", TokenType::EXPORT},
    {"extends", TokenType::EXTENDS},
    {"false", TokenType::FALSE},
    {"final", TokenType::FINAL},
    {"finally", TokenType::FINALLY},
    {"float", TokenType::FLOAT},
    {"for", TokenType::FOR},
    {"function", TokenType::FUNCTION},
    {"goto", TokenType::GOTO},
    {"if", TokenType::IF},
    {"implements", TokenType::IMPLEMENTS},
    {"import", TokenType::IMPORT},
    {"in", TokenType::IN},
    {"instanceof", TokenType::INSTANCEOF},
    {"int", TokenType::INT},
    {"interface", TokenType::INTERFACE},
    {"let", TokenType::LET},
    {"long", TokenType::LONG},
    {"native", TokenType::NATIVE},
    {"new", TokenType::NEW},
    {"null", TokenType::NULL_LITERAL},
    {"package", TokenType::PACKAGE},
    {"private", TokenType::PRIVATE},
    {"protected", TokenType::PROTECTED},
    {"public", TokenType::PUBLIC},
    {"return", TokenType::RETURN},
    {"short", TokenType::SHORT},
    {"static", TokenType::STATIC},
    {"super", TokenType::SUPER},
    {"switch", TokenType::SWITCH},
    {"synchronized", TokenType::SYNCHRONIZED},
    {"this", TokenType::THIS},
    {"throw", TokenType::THROW},
    {"throws", TokenType::THROWS},
    {"transient", TokenType::TRANSIENT},
    {"true", TokenType::TRUE},
    {"try", TokenType::TRY},
    {"typeof", TokenType::TYPEOF},
    {"undefined", TokenType::UNDEFINED},
    {"var", TokenType::VAR},
    {"void", TokenType::VOID},
    {"volatile", TokenType::VOLATILE},
    {"while", TokenType::WHILE},
    {"with", TokenType::WITH},
    {"yield", TokenType::YIELD},
};

Lexer::Lexer(const std::string& source, const std::string& filename)
    : source_(source), filename_(filename), position_(0), line_(1), column_(1),
      tokenStart_(0), tokenEnd_(0), hasError_(false) {
}

Lexer::~Lexer() {
}

Token Lexer::nextToken() {
  skipWhitespace();
  
  if (isAtEnd()) {
    return makeToken(TokenType::EOF_TOKEN);
  }
  
  tokenStart_ = position_;
  char ch = advance();
  
  // 識別子またはキーワード
  if (std::isalpha(ch) || ch == '_' || ch == '$') {
    return identifierOrKeyword();
  }
  
  // 数値リテラル
  if (std::isdigit(ch)) {
    return numberLiteral();
  }
  
  // 文字列リテラル
  if (ch == '"' || ch == '\'' || ch == '`') {
    return stringLiteral(ch);
  }
  
  // 2文字演算子のチェック
  if (!isAtEnd()) {
    char next = peek();
    switch (ch) {
      case '=':
        if (next == '=') {
          advance();
          if (peek() == '=') {
            advance();
            return makeToken(TokenType::STRICT_EQUAL);
          }
          return makeToken(TokenType::EQUAL);
        }
        if (next == '>') {
          advance();
          return makeToken(TokenType::ARROW);
        }
        return makeToken(TokenType::ASSIGN);
        
      case '!':
        if (next == '=') {
          advance();
          if (peek() == '=') {
            advance();
            return makeToken(TokenType::STRICT_NOT_EQUAL);
          }
          return makeToken(TokenType::NOT_EQUAL);
        }
        return makeToken(TokenType::NOT);
        
      case '<':
        if (next == '=') {
          advance();
          return makeToken(TokenType::LESS_EQUAL);
        }
        if (next == '<') {
          advance();
          if (peek() == '=') {
            advance();
            return makeToken(TokenType::LEFT_SHIFT_ASSIGN);
          }
          return makeToken(TokenType::LEFT_SHIFT);
        }
        return makeToken(TokenType::LESS);
        
      case '>':
        if (next == '=') {
          advance();
          return makeToken(TokenType::GREATER_EQUAL);
        }
        if (next == '>') {
          advance();
          if (peek() == '>') {
            advance();
            if (peek() == '=') {
              advance();
              return makeToken(TokenType::UNSIGNED_RIGHT_SHIFT_ASSIGN);
            }
            return makeToken(TokenType::UNSIGNED_RIGHT_SHIFT);
          }
          if (peek() == '=') {
            advance();
            return makeToken(TokenType::RIGHT_SHIFT_ASSIGN);
          }
          return makeToken(TokenType::RIGHT_SHIFT);
        }
        return makeToken(TokenType::GREATER);
        
      case '&':
        if (next == '&') {
          advance();
          return makeToken(TokenType::LOGICAL_AND);
        }
        if (next == '=') {
          advance();
          return makeToken(TokenType::BITWISE_AND_ASSIGN);
        }
        return makeToken(TokenType::BITWISE_AND);
        
      case '|':
        if (next == '|') {
          advance();
          return makeToken(TokenType::LOGICAL_OR);
        }
        if (next == '=') {
          advance();
          return makeToken(TokenType::BITWISE_OR_ASSIGN);
        }
        return makeToken(TokenType::BITWISE_OR);
        
      case '+':
        if (next == '+') {
          advance();
          return makeToken(TokenType::INCREMENT);
        }
        if (next == '=') {
          advance();
          return makeToken(TokenType::PLUS_ASSIGN);
        }
        return makeToken(TokenType::PLUS);
        
      case '-':
        if (next == '-') {
          advance();
          return makeToken(TokenType::DECREMENT);
        }
        if (next == '=') {
          advance();
          return makeToken(TokenType::MINUS_ASSIGN);
        }
        return makeToken(TokenType::MINUS);
        
      case '*':
        if (next == '*') {
          advance();
          if (peek() == '=') {
            advance();
            return makeToken(TokenType::EXPONENT_ASSIGN);
          }
          return makeToken(TokenType::EXPONENT);
        }
        if (next == '=') {
          advance();
          return makeToken(TokenType::MULTIPLY_ASSIGN);
        }
        return makeToken(TokenType::MULTIPLY);
        
      case '/':
        if (next == '/') {
          // 単行コメント
          advance();
          skipLineComment();
          return nextToken();
        }
        if (next == '*') {
          // 複数行コメント
          advance();
          skipBlockComment();
          return nextToken();
        }
        if (next == '=') {
          advance();
          return makeToken(TokenType::DIVIDE_ASSIGN);
        }
        return makeToken(TokenType::DIVIDE);
        
      case '%':
        if (next == '=') {
          advance();
          return makeToken(TokenType::MODULO_ASSIGN);
        }
        return makeToken(TokenType::MODULO);
        
      case '^':
        if (next == '=') {
          advance();
          return makeToken(TokenType::BITWISE_XOR_ASSIGN);
        }
        return makeToken(TokenType::BITWISE_XOR);
        
      case '.':
        if (next == '.' && peekNext() == '.') {
          advance();
          advance();
          return makeToken(TokenType::SPREAD);
        }
        return makeToken(TokenType::DOT);
        
      case '?':
        if (next == '?') {
          advance();
          return makeToken(TokenType::NULLISH_COALESCING);
        }
        if (next == '.') {
          advance();
          return makeToken(TokenType::OPTIONAL_CHAINING);
        }
        return makeToken(TokenType::QUESTION);
    }
  }
  
  // 1文字演算子
  switch (ch) {
    case '(': return makeToken(TokenType::LEFT_PAREN);
    case ')': return makeToken(TokenType::RIGHT_PAREN);
    case '[': return makeToken(TokenType::LEFT_BRACKET);
    case ']': return makeToken(TokenType::RIGHT_BRACKET);
    case '{': return makeToken(TokenType::LEFT_BRACE);
    case '}': return makeToken(TokenType::RIGHT_BRACE);
    case ',': return makeToken(TokenType::COMMA);
    case ';': return makeToken(TokenType::SEMICOLON);
    case ':': return makeToken(TokenType::COLON);
    case '~': return makeToken(TokenType::BITWISE_NOT);
    default:
      return makeErrorToken("Unexpected character");
  }
}

Token Lexer::peekToken() {
  size_t savedPosition = position_;
  size_t savedLine = line_;
  size_t savedColumn = column_;
  
  Token token = nextToken();
  
  position_ = savedPosition;
  line_ = savedLine;
  column_ = savedColumn;
  
  return token;
}

bool Lexer::hasError() const {
  return hasError_;
}

std::string Lexer::getErrorMessage() const {
  return errorMessage_;
}

SourceLocation Lexer::getCurrentLocation() const {
  return SourceLocation{filename_, line_, column_};
}

char Lexer::advance() {
  if (isAtEnd()) return '\0';
  
  char ch = source_[position_++];
  if (ch == '\n') {
    line_++;
    column_ = 1;
  } else {
    column_++;
  }
  return ch;
}

char Lexer::peek() const {
  if (isAtEnd()) return '\0';
  return source_[position_];
}

char Lexer::peekNext() const {
  if (position_ + 1 >= source_.length()) return '\0';
  return source_[position_ + 1];
}

bool Lexer::isAtEnd() const {
  return position_ >= source_.length();
}

void Lexer::skipWhitespace() {
  while (!isAtEnd()) {
    char ch = peek();
    if (std::isspace(ch)) {
      advance();
    } else {
      break;
    }
  }
}

void Lexer::skipLineComment() {
  while (!isAtEnd() && peek() != '\n') {
    advance();
  }
}

void Lexer::skipBlockComment() {
  while (!isAtEnd()) {
    if (peek() == '*' && peekNext() == '/') {
      advance(); // '*'
      advance(); // '/'
      break;
    }
    advance();
  }
}

Token Lexer::identifierOrKeyword() {
  while (!isAtEnd() && (std::isalnum(peek()) || peek() == '_' || peek() == '$')) {
    advance();
  }
  
  std::string text = source_.substr(tokenStart_, position_ - tokenStart_);
  
  auto it = keywords.find(text);
  if (it != keywords.end()) {
    return makeToken(it->second);
  }
  
  return makeToken(TokenType::IDENTIFIER);
}

Token Lexer::numberLiteral() {
  // 整数部分
  while (!isAtEnd() && std::isdigit(peek())) {
    advance();
  }
  
  // 小数点と小数部分
  if (!isAtEnd() && peek() == '.' && std::isdigit(peekNext())) {
    advance(); // '.'
    while (!isAtEnd() && std::isdigit(peek())) {
      advance();
    }
  }
  
  // 指数部分
  if (!isAtEnd() && (peek() == 'e' || peek() == 'E')) {
    advance();
    if (!isAtEnd() && (peek() == '+' || peek() == '-')) {
      advance();
    }
    while (!isAtEnd() && std::isdigit(peek())) {
      advance();
    }
  }
  
  return makeToken(TokenType::NUMBER);
}

Token Lexer::stringLiteral(char quote) {
  while (!isAtEnd() && peek() != quote) {
    if (peek() == '\\') {
      advance(); // '\\'
      if (!isAtEnd()) {
        advance(); // エスケープされた文字
      }
    } else {
      advance();
    }
  }
  
  if (isAtEnd()) {
    return makeErrorToken("Unterminated string literal");
  }
  
  advance(); // 閉じクォート
  
  if (quote == '`') {
    return makeToken(TokenType::TEMPLATE_LITERAL);
  } else {
    return makeToken(TokenType::STRING);
  }
}

Token Lexer::makeToken(TokenType type) {
  tokenEnd_ = position_;
  std::string text = source_.substr(tokenStart_, tokenEnd_ - tokenStart_);
  
  SourceLocation location{filename_, line_, column_ - static_cast<size_t>(tokenEnd_ - tokenStart_)};
  
  return Token{type, text, location};
}

Token Lexer::makeErrorToken(const std::string& message) {
  hasError_ = true;
  errorMessage_ = message;
  
  SourceLocation location{filename_, line_, column_};
  
  return Token{TokenType::ERROR, message, location};
}

}  // namespace parser
}  // namespace core
}  // namespace aerojs