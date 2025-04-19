/**
 * @file lexer.cpp
 * @brief JavaScript用の字句解析器（Lexer）の実装
 * 
 * このファイルはJavaScriptのソースコードを解析してトークン列に変換する
 * 字句解析器の実装を提供します。
 */

#include "lexer.h"
#include "../parser_error.h"
#include <cctype>
#include <string>
#include <unordered_map>

namespace aerojs {
namespace core {

// キーワードマップの初期化
const std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
  // JavaScript予約語
  {"break", TokenType::BREAK},
  {"case", TokenType::CASE},
  {"catch", TokenType::CATCH},
  {"class", TokenType::CLASS},
  {"const", TokenType::CONST},
  {"continue", TokenType::CONTINUE},
  {"debugger", TokenType::DEBUGGER},
  {"default", TokenType::DEFAULT},
  {"delete", TokenType::DELETE},
  {"do", TokenType::DO},
  {"else", TokenType::ELSE},
  {"enum", TokenType::ENUM},
  {"export", TokenType::EXPORT},
  {"extends", TokenType::EXTENDS},
  {"false", TokenType::FALSE},
  {"finally", TokenType::FINALLY},
  {"for", TokenType::FOR},
  {"function", TokenType::FUNCTION},
  {"if", TokenType::IF},
  {"import", TokenType::IMPORT},
  {"in", TokenType::IN},
  {"instanceof", TokenType::INSTANCEOF},
  {"new", TokenType::NEW},
  {"null", TokenType::NULL_LITERAL},
  {"return", TokenType::RETURN},
  {"super", TokenType::SUPER},
  {"switch", TokenType::SWITCH},
  {"this", TokenType::THIS},
  {"throw", TokenType::THROW},
  {"true", TokenType::TRUE},
  {"try", TokenType::TRY},
  {"typeof", TokenType::TYPEOF},
  {"var", TokenType::VAR},
  {"void", TokenType::VOID},
  {"while", TokenType::WHILE},
  {"with", TokenType::WITH},
  
  // ECMAScript 2015+ (ES6+)
  {"yield", TokenType::YIELD},
  {"let", TokenType::LET},
  {"static", TokenType::STATIC},
  {"await", TokenType::AWAIT},
  {"async", TokenType::ASYNC},
  {"of", TokenType::OF},
  
  // 将来の予約語
  {"implements", TokenType::IMPLEMENTS},
  {"interface", TokenType::INTERFACE},
  {"package", TokenType::PACKAGE},
  {"private", TokenType::PRIVATE},
  {"protected", TokenType::PROTECTED},
  {"public", TokenType::PUBLIC}
};

Lexer::Lexer(const std::string& source, const std::string& filename)
  : source_(source), filename_(filename), start_(0), current_(0), line_(1), column_(1) {
}

Lexer::~Lexer() = default;

Token Lexer::scanToken() {
  if (!savedTokens_.empty()) {
    Token token = savedTokens_.front();
    savedTokens_.erase(savedTokens_.begin());
    return token;
  }
  
  skipWhitespace();
  
  start_ = current_;
  
  if (isAtEnd()) {
    return Token(TokenType::END_OF_FILE, "", nullptr, createSourceLocation());
  }
  
  char c = advance();
  
  // 識別子
  if (isalpha(c) || c == '_' || c == '$') {
    return scanIdentifier();
  }
  
  // 数値
  if (isdigit(c)) {
    return scanNumber();
  }
  
  // 特殊文字とトークンのマッピング
  switch (c) {
    // 一文字トークン
    case '(': return Token(TokenType::LEFT_PAREN, "(", nullptr, createSourceLocation());
    case ')': return Token(TokenType::RIGHT_PAREN, ")", nullptr, createSourceLocation());
    case '{': return Token(TokenType::LEFT_BRACE, "{", nullptr, createSourceLocation());
    case '}': return Token(TokenType::RIGHT_BRACE, "}", nullptr, createSourceLocation());
    case '[': return Token(TokenType::LEFT_BRACKET, "[", nullptr, createSourceLocation());
    case ']': return Token(TokenType::RIGHT_BRACKET, "]", nullptr, createSourceLocation());
    case ';': return Token(TokenType::SEMICOLON, ";", nullptr, createSourceLocation());
    case ',': return Token(TokenType::COMMA, ",", nullptr, createSourceLocation());
    case '.': {
      // 数値リテラル（.123形式）か、ドット演算子かを判断
      if (isdigit(peek())) {
        return scanNumber();
      } 
      // スプレッド演算子 (...)
      else if (peek() == '.' && peekNext() == '.') {
        advance(); // 2つ目の.
        advance(); // 3つ目の.
        return Token(TokenType::DOT_DOT_DOT, "...", nullptr, createSourceLocation());
      }
      // 通常のドット
      return Token(TokenType::DOT, ".", nullptr, createSourceLocation());
    }
    
    // 二文字トークン
    case '!': 
      return Token(
        match('=') ? (match('=') ? TokenType::BANG_EQUAL_EQUAL : TokenType::BANG_EQUAL) : TokenType::BANG, 
        source_.substr(start_, current_ - start_), 
        nullptr,
        createSourceLocation()
      );
    case '=': 
      if (match('=')) {
        if (match('=')) {
          return Token(TokenType::EQUAL_EQUAL_EQUAL, "===", nullptr, createSourceLocation());
        }
        return Token(TokenType::EQUAL_EQUAL, "==", nullptr, createSourceLocation());
      } else if (match('>')) {
        return Token(TokenType::ARROW, "=>", nullptr, createSourceLocation());
      }
      return Token(TokenType::EQUAL, "=", nullptr, createSourceLocation());
    case '<': 
      if (match('=')) {
        return Token(TokenType::LESS_EQUAL, "<=", nullptr, createSourceLocation());
      } else if (match('<')) {
        if (match('=')) {
          return Token(TokenType::LESS_LESS_EQUAL, "<<=", nullptr, createSourceLocation());
        }
        return Token(TokenType::LESS_LESS, "<<", nullptr, createSourceLocation());
      }
      return Token(TokenType::LESS, "<", nullptr, createSourceLocation());
    case '>': 
      if (match('=')) {
        return Token(TokenType::GREATER_EQUAL, ">=", nullptr, createSourceLocation());
      } else if (match('>')) {
        if (match('>')) {
          if (match('=')) {
            return Token(TokenType::GREATER_GREATER_GREATER_EQUAL, ">>>=", nullptr, createSourceLocation());
          }
          return Token(TokenType::GREATER_GREATER_GREATER, ">>>", nullptr, createSourceLocation());
        } else if (match('=')) {
          return Token(TokenType::GREATER_GREATER_EQUAL, ">>=", nullptr, createSourceLocation());
        }
        return Token(TokenType::GREATER_GREATER, ">>", nullptr, createSourceLocation());
      }
      return Token(TokenType::GREATER, ">", nullptr, createSourceLocation());
    
    // 算術演算子
    case '+': 
      if (match('=')) {
        return Token(TokenType::PLUS_EQUAL, "+=", nullptr, createSourceLocation());
      } else if (match('+')) {
        return Token(TokenType::PLUS_PLUS, "++", nullptr, createSourceLocation());
      }
      return Token(TokenType::PLUS, "+", nullptr, createSourceLocation());
    case '-': 
      if (match('=')) {
        return Token(TokenType::MINUS_EQUAL, "-=", nullptr, createSourceLocation());
      } else if (match('-')) {
        return Token(TokenType::MINUS_MINUS, "--", nullptr, createSourceLocation());
      }
      return Token(TokenType::MINUS, "-", nullptr, createSourceLocation());
    case '*': 
      if (match('=')) {
        return Token(TokenType::STAR_EQUAL, "*=", nullptr, createSourceLocation());
      } else if (match('*')) {
        if (match('=')) {
          return Token(TokenType::STAR_STAR_EQUAL, "**=", nullptr, createSourceLocation());
        }
        return Token(TokenType::STAR_STAR, "**", nullptr, createSourceLocation()); // 指数演算子
      }
      return Token(TokenType::STAR, "*", nullptr, createSourceLocation());
    case '/': 
      // コメント
      if (match('/')) {
        // 行コメント
        while (peek() != '\n' && !isAtEnd()) {
          advance();
        }
        return scanToken(); // コメントはスキップして次のトークンへ
      } else if (match('*')) {
        // ブロックコメント
        while (!(peek() == '*' && peekNext() == '/') && !isAtEnd()) {
          if (peek() == '\n') {
            line_++;
            column_ = 1;
          }
          advance();
        }
        
        if (isAtEnd()) {
          return errorToken("終了していないブロックコメント", createSourceLocation());
        }
        
        // '*/'を消費
        advance(); // *
        advance(); // /
        
        return scanToken(); // コメントはスキップして次のトークンへ
      } else if (match('=')) {
        return Token(TokenType::SLASH_EQUAL, "/=", nullptr, createSourceLocation());
      }
      // 正規表現リテラルの開始かもしれない
      // パーサーコンテキストに依存するので、実際の実装ではもっと複雑になる
      return Token(TokenType::SLASH, "/", nullptr, createSourceLocation());
    case '%': 
      if (match('=')) {
        return Token(TokenType::PERCENT_EQUAL, "%=", nullptr, createSourceLocation());
      }
      return Token(TokenType::PERCENT, "%", nullptr, createSourceLocation());
    
    // ビット演算子
    case '&': 
      if (match('=')) {
        return Token(TokenType::AMPERSAND_EQUAL, "&=", nullptr, createSourceLocation());
      } else if (match('&')) {
        if (match('=')) {
          return Token(TokenType::AMPERSAND_AMPERSAND_EQUAL, "&&=", nullptr, createSourceLocation());
        }
        return Token(TokenType::AMPERSAND_AMPERSAND, "&&", nullptr, createSourceLocation());
      }
      return Token(TokenType::AMPERSAND, "&", nullptr, createSourceLocation());
    case '|': 
      if (match('=')) {
        return Token(TokenType::PIPE_EQUAL, "|=", nullptr, createSourceLocation());
      } else if (match('|')) {
        if (match('=')) {
          return Token(TokenType::PIPE_PIPE_EQUAL, "||=", nullptr, createSourceLocation());
        }
        return Token(TokenType::PIPE_PIPE, "||", nullptr, createSourceLocation());
      }
      return Token(TokenType::PIPE, "|", nullptr, createSourceLocation());
    case '^': 
      if (match('=')) {
        return Token(TokenType::CARET_EQUAL, "^=", nullptr, createSourceLocation());
      }
      return Token(TokenType::CARET, "^", nullptr, createSourceLocation());
    case '~': 
      return Token(TokenType::TILDE, "~", nullptr, createSourceLocation());
    
    // 文字列リテラル
    case '"': return scanString();
    case '\'': return scanString();
    
    // テンプレートリテラル
    case '`': return scanTemplate();
    
    // nullishコアレシング演算子
    case '?': 
      if (match('?')) {
        if (match('=')) {
          return Token(TokenType::QUESTION_QUESTION_EQUAL, "??=", nullptr, createSourceLocation());
        }
        return Token(TokenType::QUESTION_QUESTION, "??", nullptr, createSourceLocation());
      } else if (match('.')) {
        // オプショナルチェーン
        return Token(TokenType::QUESTION_DOT, "?.", nullptr, createSourceLocation());
      }
      return Token(TokenType::QUESTION, "?", nullptr, createSourceLocation());
    
    case ':': return Token(TokenType::COLON, ":", nullptr, createSourceLocation());
    
    default:
      return errorToken("予期しない文字", createSourceLocation());
  }
}

Token Lexer::peekToken() {
  if (savedTokens_.empty()) {
    savedTokens_.push_back(scanToken());
  }
  return savedTokens_.front();
}

Token Lexer::peekToken(int n) {
  while (static_cast<int>(savedTokens_.size()) < n) {
    savedTokens_.push_back(scanToken());
  }
  return savedTokens_[n - 1];
}

bool Lexer::match(TokenType type) {
  if (peekToken().type == type) {
    scanToken(); // トークンを消費
    return true;
  }
  return false;
}

std::optional<Token> Lexer::consume(TokenType type) {
  if (peekToken().type == type) {
    return scanToken();
  }
  return std::nullopt;
}

Token Lexer::expect(TokenType type, const std::string& message) {
  if (peekToken().type == type) {
    return scanToken();
  }
  throw ParserError(message, getCurrentLocation());
}

std::vector<Token> Lexer::scanAllTokens() {
  std::vector<Token> tokens;
  while (true) {
    Token token = scanToken();
    tokens.push_back(token);
    if (token.type == TokenType::END_OF_FILE) {
      break;
    }
  }
  return tokens;
}

int Lexer::getCurrentLine() const {
  return line_;
}

int Lexer::getCurrentColumn() const {
  return column_;
}

SourceLocation Lexer::getCurrentLocation() const {
  return SourceLocation(filename_, line_, column_ - (current_ - start_));
}

Token Lexer::errorToken(const std::string& message, const SourceLocation& location) {
  return Token(TokenType::ERROR, message, nullptr, location);
}

bool Lexer::isAtEnd() const {
  return current_ >= source_.length();
}

char Lexer::advance() {
  char c = source_[current_++];
  if (c == '\n') {
    line_++;
    column_ = 1;
  } else {
    column_++;
  }
  return c;
}

char Lexer::peek() const {
  if (isAtEnd()) return '\0';
  return source_[current_];
}

char Lexer::peekNext() const {
  if (current_ + 1 >= source_.length()) return '\0';
  return source_[current_ + 1];
}

bool Lexer::match(char expected) {
  if (isAtEnd()) return false;
  if (source_[current_] != expected) return false;
  
  current_++;
  column_++;
  return true;
}

SourceLocation Lexer::createSourceLocation() const {
  return SourceLocation(filename_, line_, column_ - (current_ - start_));
}
Token Lexer::scanString() {
  // 開始の引用符を保存（'"'または''''）
  char quoteType = source_[start_];
  
  while (peek() != quoteType && !isAtEnd()) {
    if (peek() == '\n') {
      line_++;
      column_ = 1;
    }
    
    // エスケープシーケンス
    if (peek() == '\\') {
      advance(); // バックスラッシュをスキップ
    }
    
    advance();
  }
  
  if (isAtEnd()) {
    return errorToken("終了していない文字列", createSourceLocation());
  }
  
  // 終了の引用符
  advance();
  
  // 引用符を除いた文字列の値を取得
  std::string value = source_.substr(start_ + 1, current_ - start_ - 2);
  
  // エスケープシーケンスを処理
  std::string processedValue;
  processedValue.reserve(value.length());
  
  for (size_t i = 0; i < value.length(); ++i) {
    if (value[i] == '\\' && i + 1 < value.length()) {
      switch (value[i + 1]) {
        case 'n': processedValue += '\n'; break;
        case 'r': processedValue += '\r'; break;
        case 't': processedValue += '\t'; break;
        case 'b': processedValue += '\b'; break;
        case 'f': processedValue += '\f'; break;
        case 'v': processedValue += '\v'; break;
        case '0': processedValue += '\0'; break;
        case '\'': processedValue += '\''; break;
        case '\"': processedValue += '\"'; break;
        case '\\': processedValue += '\\'; break;
        case 'x': 
          if (i + 3 < value.length() && isxdigit(value[i + 2]) && isxdigit(value[i + 3])) {
            std::string hex = value.substr(i + 2, 2);
            char ch = static_cast<char>(std::stoi(hex, nullptr, 16));
            processedValue += ch;
            i += 3;
          } else {
            processedValue += 'x';
          }
          break;
        case 'u':
          if (i + 5 < value.length() && value[i + 2] == '{' && isxdigit(value[i + 3]) && 
              isxdigit(value[i + 4]) && isxdigit(value[i + 5]) && value[i + 6] == '}') {
            // Unicode コードポイント形式 \u{XXX}
            std::string hex = value.substr(i + 3, 3);
            uint32_t codePoint = std::stoul(hex, nullptr, 16);
            
            // UTF-8 エンコーディング
            if (codePoint <= 0x7F) {
              // 1バイト文字
              processedValue += static_cast<char>(codePoint);
            } else if (codePoint <= 0x7FF) {
              // 2バイト文字
              processedValue += static_cast<char>(0xC0 | (codePoint >> 6));
              processedValue += static_cast<char>(0x80 | (codePoint & 0x3F));
            } else if (codePoint <= 0xFFFF) {
              // 3バイト文字
              processedValue += static_cast<char>(0xE0 | (codePoint >> 12));
              processedValue += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
              processedValue += static_cast<char>(0x80 | (codePoint & 0x3F));
            } else if (codePoint <= 0x10FFFF) {
              // 4バイト文字
              processedValue += static_cast<char>(0xF0 | (codePoint >> 18));
              processedValue += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
              processedValue += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
              processedValue += static_cast<char>(0x80 | (codePoint & 0x3F));
            }
            i += 6;
          } else if (i + 5 < value.length() && isxdigit(value[i + 2]) && isxdigit(value[i + 3]) && 
                     isxdigit(value[i + 4]) && isxdigit(value[i + 5])) {
            // 標準の \uXXXX 形式
            std::string hex = value.substr(i + 2, 4);
            uint16_t codeUnit = static_cast<uint16_t>(std::stoul(hex, nullptr, 16));
            
            // サロゲートペアの処理
            if (codeUnit >= 0xD800 && codeUnit <= 0xDBFF && 
                i + 7 < value.length() && value[i + 6] == '\\' && value[i + 7] == 'u' &&
                i + 11 < value.length()) {
              // 上位サロゲート検出、下位サロゲートを確認
              std::string lowHex = value.substr(i + 8, 4);
              uint16_t lowSurrogate = static_cast<uint16_t>(std::stoul(lowHex, nullptr, 16));
              
              if (lowSurrogate >= 0xDC00 && lowSurrogate <= 0xDFFF) {
                // 有効なサロゲートペア
                uint32_t codePoint = 0x10000 + (((codeUnit - 0xD800) << 10) | (lowSurrogate - 0xDC00));
                
                // UTF-8 エンコーディング (4バイト)
                processedValue += static_cast<char>(0xF0 | (codePoint >> 18));
                processedValue += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
                processedValue += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                processedValue += static_cast<char>(0x80 | (codePoint & 0x3F));
                
                i += 11; // サロゲートペア全体をスキップ
              } else {
                // 無効なサロゲートペア、単独の上位サロゲートとして処理
                // ECMAScript仕様に従い、置換文字U+FFFDを使用
                processedValue += "\xEF\xBF\xBD"; // UTF-8でのU+FFFD
                i += 5;
              }
            } else {
              // 通常のUnicode文字またはサロゲートではない
              if (codeUnit >= 0xD800 && codeUnit <= 0xDFFF) {
                // 孤立したサロゲート、置換文字を使用
                processedValue += "\xEF\xBF\xBD"; // UTF-8でのU+FFFD
              } else if (codeUnit <= 0x7F) {
                // 1バイト文字
                processedValue += static_cast<char>(codeUnit);
              } else if (codeUnit <= 0x7FF) {
                // 2バイト文字
                processedValue += static_cast<char>(0xC0 | (codeUnit >> 6));
                processedValue += static_cast<char>(0x80 | (codeUnit & 0x3F));
              } else {
                // 3バイト文字
                processedValue += static_cast<char>(0xE0 | (codeUnit >> 12));
                processedValue += static_cast<char>(0x80 | ((codeUnit >> 6) & 0x3F));
                processedValue += static_cast<char>(0x80 | (codeUnit & 0x3F));
              }
              i += 5;
            }
          } else {
            // 不正なUnicodeエスケープシーケンス
            processedValue += 'u';
          }
          break;
        default:
          // ECMAScript仕様に従い、バックスラッシュの後の文字をそのまま出力
          processedValue += value[i + 1];
      }
      i++; // エスケープシーケンスの2文字目をスキップ
    } else {
      processedValue += value[i];
    }
  }
  
  // 文字列リテラルの末尾にnがある場合、BigIntリテラルとして処理
  if (peek() == 'n') {
    advance(); // 'n'を消費
    return Token(TokenType::BIGINT, source_.substr(start_, current_ - start_), processedValue, createSourceLocation());
  }
  
  return Token(TokenType::STRING, source_.substr(start_, current_ - start_), processedValue, createSourceLocation());
}

Token Lexer::scanNumber() {
  bool isFloat = false;
  bool isHex = false;
  bool isBinary = false;
  bool isOctal = false;
  bool isExponential = false;
  bool isBigInt = false;
  
  // 0xで始まる16進数
  if (source_[start_] == '0' && (peek() == 'x' || peek() == 'X')) {
    isHex = true;
    advance(); // 'x'をスキップ
    
    // 16進数の桁をスキャン
    while (isxdigit(peek())) {
      advance();
    }
    
    // 16進数の後に不正な文字がある場合
    if (isalpha(peek()) || peek() == '_' || peek() == '$') {
      while (isalnum(peek()) || peek() == '_' || peek() == '$') {
        advance();
      }
      return errorToken("不正な16進数リテラル", createSourceLocation());
    }
  }
  // 0bで始まる2進数
  else if (source_[start_] == '0' && (peek() == 'b' || peek() == 'B')) {
    isBinary = true;
    advance(); // 'b'をスキップ
    
    // 2進数の桁をスキャン
    while (peek() == '0' || peek() == '1') {
      advance();
    }
    
    // 2進数の後に不正な文字がある場合
    if (isalnum(peek()) || peek() == '_' || peek() == '$') {
      while (isalnum(peek()) || peek() == '_' || peek() == '$') {
        advance();
      }
      return errorToken("不正な2進数リテラル", createSourceLocation());
    }
  }
  // 0oで始まる8進数
  else if (source_[start_] == '0' && (peek() == 'o' || peek() == 'O')) {
    isOctal = true;
    advance(); // 'o'をスキップ
    
    // 8進数の桁をスキャン
    while (peek() >= '0' && peek() <= '7') {
      advance();
    }
    
    // 8進数の後に不正な文字がある場合
    if (isalnum(peek()) || peek() == '_' || peek() == '$') {
      while (isalnum(peek()) || peek() == '_' || peek() == '$') {
        advance();
      }
      return errorToken("不正な8進数リテラル", createSourceLocation());
    }
  }
  // 通常の10進数
  else {
    // 整数部分
    while (isdigit(peek())) {
      advance();
    }
    
    // 数値セパレータ（ES2021）のサポート
    if (peek() == '_') {
      while (peek() == '_') {
        advance();
        if (!isdigit(peek())) {
          return errorToken("数値セパレータの後には数字が必要です", createSourceLocation());
        }
        while (isdigit(peek())) {
          advance();
        }
      }
    }
    
    // 小数部分
    if (peek() == '.' && (isdigit(peekNext()) || peekNext() == '_')) {
      isFloat = true;
      advance(); // '.'をスキップ
      
      while (isdigit(peek()) || peek() == '_') {
        if (peek() == '_') {
          advance();
          if (!isdigit(peek())) {
            return errorToken("数値セパレータの後には数字が必要です", createSourceLocation());
          }
        } else {
          advance();
        }
      }
    }
    
    // 指数部分
    if (peek() == 'e' || peek() == 'E') {
      isExponential = true;
      isFloat = true; // 指数表記は浮動小数点として扱う
      advance(); // 'e'または'E'をスキップ
      
      // 符号
      if (peek() == '-' || peek() == '+') {
        advance();
      }
      
      // 指数の桁
      if (!isdigit(peek())) {
        return errorToken("不正な指数表記", createSourceLocation());
      }
      
      while (isdigit(peek()) || peek() == '_') {
        if (peek() == '_') {
          advance();
          if (!isdigit(peek())) {
            return errorToken("数値セパレータの後には数字が必要です", createSourceLocation());
          }
        } else {
          advance();
        }
      }
    }
  }
  
  // BigInt接尾辞
  if (peek() == 'n' && !isFloat && !isExponential) {
    isBigInt = true;
    advance(); // 'n'をスキップ
  }
  
  // 数値の後に不正な識別子文字がある場合
  if (isalpha(peek()) || peek() == '_' || peek() == '$') {
    while (isalnum(peek()) || peek() == '_' || peek() == '$') {
      advance();
    }
    return errorToken("不正な数値リテラル", createSourceLocation());
  }
  
  std::string lexeme = source_.substr(start_, current_ - start_);
  
  try {
    // 数値セパレータを除去
    std::string cleanLexeme = lexeme;
    cleanLexeme.erase(std::remove(cleanLexeme.begin(), cleanLexeme.end(), '_'), cleanLexeme.end());
    
    if (isBigInt) {
      // BigIntの処理
      std::string bigintValue = cleanLexeme.substr(0, cleanLexeme.length() - 1);
      
      if (isHex) {
        // 16進数BigInt
        size_t pos = 2; // "0x"をスキップ
        std::string hexValue = bigintValue.substr(pos);
        return Token(TokenType::BIGINT, lexeme, hexValue, createSourceLocation());
      } else if (isBinary) {
        // 2進数BigInt
        size_t pos = 2; // "0b"をスキップ
        std::string binValue = bigintValue.substr(pos);
        return Token(TokenType::BIGINT, lexeme, binValue, createSourceLocation());
      } else if (isOctal) {
        // 8進数BigInt
        size_t pos = 2; // "0o"をスキップ
        std::string octValue = bigintValue.substr(pos);
        return Token(TokenType::BIGINT, lexeme, octValue, createSourceLocation());
      } else {
        // 10進数BigInt
        return Token(TokenType::BIGINT, lexeme, bigintValue, createSourceLocation());
      }
    } else if (isHex) {
      // 16進数の処理
      size_t pos = 2; // "0x"をスキップ
      std::string hexValue = cleanLexeme.substr(pos);
      long long value = std::stoll(hexValue, nullptr, 16);
      return Token(TokenType::NUMBER, lexeme, static_cast<double>(value), createSourceLocation());
    } else if (isBinary) {
      // 2進数の処理
      size_t pos = 2; // "0b"をスキップ
      std::string binValue = cleanLexeme.substr(pos);
      long long value = std::stoll(binValue, nullptr, 2);
      return Token(TokenType::NUMBER, lexeme, static_cast<double>(value), createSourceLocation());
    } else if (isOctal) {
      // 8進数の処理
      size_t pos = 2; // "0o"をスキップ
      std::string octValue = cleanLexeme.substr(pos);
      long long value = std::stoll(octValue, nullptr, 8);
      return Token(TokenType::NUMBER, lexeme, static_cast<double>(value), createSourceLocation());
    } else {
      // 10進数の処理
      double value = std::stod(cleanLexeme);
      return Token(TokenType::NUMBER, lexeme, value, createSourceLocation());
    }
  } catch (const std::exception& e) {
    return errorToken("不正な数値: " + std::string(e.what()), createSourceLocation());
  }
}

Token Lexer::scanIdentifier() {
  while (isalnum(peek()) || peek() == '_' || peek() == '$') {
    advance();
  }
  
  std::string lexeme = source_.substr(start_, current_ - start_);
  
  // キーワードかどうかチェック
  auto it = keywords_.find(lexeme);
  TokenType type = (it != keywords_.end()) ? it->second : TokenType::IDENTIFIER;
  
  // 予約語の特別処理
  if (type == TokenType::TRUE) {
    return Token(type, lexeme, true, createSourceLocation());
  } else if (type == TokenType::FALSE) {
    return Token(type, lexeme, false, createSourceLocation());
  } else if (type == TokenType::NULL_TOKEN) {
    return Token(type, lexeme, nullptr, createSourceLocation());
  } else if (type == TokenType::UNDEFINED) {
    return Token(type, lexeme, nullptr, createSourceLocation());
  }
  
  return Token(type, lexeme, lexeme, createSourceLocation());
}
Token Lexer::scanTemplate() {
  bool isTaggedTemplate = false;
  // パーサーの状態からタグ付きテンプレートかどうかを判断
  if (previousToken_ && previousToken_->type == TokenType::IDENTIFIER) {
    isTaggedTemplate = true;
  }
  
  std::string templateContent;
  bool inEscapeSequence = false;
  
  while (peek() != '`' && !isAtEnd()) {
    if (peek() == '\n') {
      line_++;
      column_ = 1;
      templateContent += peek();
      advance();
    } else if (peek() == '$' && peekNext() == '{' && !inEscapeSequence) {
      // テンプレート式の開始
      advance(); // '$'をスキップ
      advance(); // '{'をスキップ
      
      // テンプレートの部分文字列を取得（開始の'`'と'${'を除く）
      std::string part = source_.substr(start_ + 1, current_ - start_ - 3);
      
      // エスケープシーケンスを処理
      std::string processedPart = processEscapeSequences(part);
      
      TokenType templateType = isTaggedTemplate ? TokenType::TAGGED_TEMPLATE_HEAD : TokenType::TEMPLATE_HEAD;
      return Token(templateType, source_.substr(start_, current_ - start_), processedPart, createSourceLocation());
    } else if (peek() == '\\' && !inEscapeSequence) {
      // エスケープシーケンスの開始
      inEscapeSequence = true;
      templateContent += peek();
      advance();
      
      if (peek() == 'u') {
        // Unicode エスケープシーケンス (例: \u00A9)
        templateContent += peek();
        advance(); // 'u'をスキップ
        
        // 4桁の16進数を処理
        for (int i = 0; i < 4 && !isAtEnd() && isHexDigit(peek()); i++) {
          templateContent += peek();
          advance();
        }
      } else if (peek() == 'x') {
        // 16進数エスケープシーケンス (例: \x41)
        templateContent += peek();
        advance(); // 'x'をスキップ
        
        // 2桁の16進数を処理
        for (int i = 0; i < 2 && !isAtEnd() && isHexDigit(peek()); i++) {
          templateContent += peek();
          advance();
        }
      } else if (peek() == 'u' && peekNext() == '{') {
        // 拡張Unicode エスケープシーケンス (例: \u{1F600})
        templateContent += peek();
        advance(); // 'u'をスキップ
        templateContent += peek();
        advance(); // '{'をスキップ
        
        // 1〜6桁の16進数を処理
        while (!isAtEnd() && peek() != '}' && isHexDigit(peek())) {
          templateContent += peek();
          advance();
        }
        
        if (peek() == '}') {
          templateContent += peek();
          advance(); // '}'をスキップ
        }
      } else {
        // その他のエスケープシーケンス (例: \n, \t, \\, \', \", \`)
        templateContent += peek();
        advance();
      }
      
      inEscapeSequence = false;
    } else {
      templateContent += peek();
      advance();
    }
  }
  
  if (isAtEnd()) {
    return errorToken("終了していないテンプレートリテラル", createSourceLocation());
  }
  // 終了のバッククォート
  advance();
  
  // 値を取得（開始と終了の'`'を除く）
  std::string value = source_.substr(start_ + 1, current_ - start_ - 2);
  
  // エスケープシーケンスを処理
  std::string processedValue;
  processedValue.reserve(value.length());
  
  for (size_t i = 0; i < value.length(); ++i) {
    if (value[i] == '\\' && i + 1 < value.length()) {
      switch (value[i + 1]) {
        case 'n': processedValue += '\n'; break;
        case 'r': processedValue += '\r'; break;
        case 't': processedValue += '\t'; break;
        case 'b': processedValue += '\b'; break;
        case 'f': processedValue += '\f'; break;
        case 'v': processedValue += '\v'; break;
        case '0': processedValue += '\0'; break;
        case '\'': processedValue += '\''; break;
        case '\"': processedValue += '\"'; break;
        case '\\': processedValue += '\\'; break;
        case '`': processedValue += '`'; break;
        case '$': processedValue += '$'; break;
        default: processedValue += value[i + 1];
      }
      i++; // エスケープシーケンスの2文字目をスキップ
    } else {
      processedValue += value[i];
    }
  }
  
  return Token(isTaggedTemplate ? TokenType::TEMPLATE_TAGGED : TokenType::TEMPLATE, 
              source_.substr(start_, current_ - start_), 
              processedValue, 
              createSourceLocation());
}

void Lexer::skipWhitespace() {
  while (true) {
    char c = peek();
    switch (c) {
      case ' ':
      case '\r':
      case '\t':
        advance();
        break;
      case '\n':
        line_++;
        column_ = 1;
        advance();
        break;
      case '/':
        if (peekNext() == '/') {
          // 行コメント
          advance(); // 最初の'/'をスキップ
          advance(); // 2番目の'/'をスキップ
          
          // 行末までスキップ
          while (peek() != '\n' && !isAtEnd()) {
            advance();
          }
        } else if (peekNext() == '*') {
          // ブロックコメント
          advance(); // 最初の'/'をスキップ
          advance(); // '*'をスキップ
          
          // コメント終了までスキップ
          while (!(peek() == '*' && peekNext() == '/') && !isAtEnd()) {
            if (peek() == '\n') {
              line_++;
              column_ = 1;
            }
            advance();
          }
          
          if (isAtEnd()) {
            // 終了していないブロックコメント
            // エラーを報告せずに処理を続行
          } else {
            advance(); // '*'をスキップ
            advance(); // '/'をスキップ
          }
        } else {
          return; // 除算演算子または正規表現リテラルの開始
        }
        break;
      default:
        return;
    }
  }
}

} // namespace core
} // namespace aerojs 