/**
 * @file scanner.cpp
 * @brief ECMAScript用の文字スキャナーの実装
 * 
 * このファイルはソースコードの文字列を一文字ずつ解析するための
 * スキャナークラスの実装を提供します。
 */

#include "scanner.h"
#include <cctype>
#include <stdexcept>

namespace aerojs {
namespace core {

Scanner::Scanner(const std::string& source, const std::string& filename, int start_line)
    : m_source(source),
      m_filename(filename),
      m_current(0),
      m_marked_position(0),
      m_line(start_line),
      m_column(1) {
}

Scanner::Scanner(const std::string_view& source, const std::string& filename, int start_line)
    : m_source(source),
      m_filename(filename),
      m_current(0),
      m_marked_position(0),
      m_line(start_line),
      m_column(1) {
}

char Scanner::advance() {
  if (isAtEnd()) {
    return '\0';
  }

  char current_char = m_source[m_current++];
  
  // 改行文字の処理
  if (current_char == '\n') {
    m_line++;
    m_column = 1;
  } else if (current_char == '\r') {
    // CRLFの場合は両方で1行としてカウント
    if (peek() == '\n') {
      m_current++;
    }
    m_line++;
    m_column = 1;
  } else {
    m_column++;
  }
  
  return current_char;
}

char Scanner::peek() const {
  if (isAtEnd()) {
    return '\0';
  }
  
  return m_source[m_current];
}

char Scanner::peekNext() const {
  if (m_current + 1 >= m_source.size()) {
    return '\0';
  }
  
  return m_source[m_current + 1];
}

char Scanner::peekAhead(size_t n) const {
  if (m_current + n >= m_source.size()) {
    return '\0';
  }
  
  return m_source[m_current + n];
}

bool Scanner::match(char expected) {
  if (isAtEnd() || m_source[m_current] != expected) {
    return false;
  }
  
  m_current++;
  if (expected == '\n') {
    m_line++;
    m_column = 1;
  } else {
    m_column++;
  }
  
  return true;
}

bool Scanner::isAtEnd() const {
  return m_current >= m_source.size();
}

SourceLocation Scanner::getCurrentLocation() const {
  return {
    m_line,
    m_column,
    static_cast<int>(m_current),
    m_filename
  };
}

int Scanner::getLine() const {
  return m_line;
}

int Scanner::getColumn() const {
  return m_column;
}

void Scanner::markPosition() {
  m_marked_position = m_current;
}

void Scanner::resetToMark() {
  // 行と列の情報をリセットする必要がある
  // 現在位置からマーク位置まで戻る
  if (m_marked_position < m_current) {
    // マーク位置から現在位置まで再スキャンして行と列を正確に計算
    size_t saved_current = m_current;
    int saved_line = m_line;
    int saved_column = m_column;
    
    // マーク位置から始める
    m_current = m_marked_position;
    
    // マーク位置での行と列を計算
    // ここではシンプルのため先頭から再スキャン
    m_line = 1;
    m_column = 1;
    
    for (size_t i = 0; i < m_marked_position; i++) {
      if (m_source[i] == '\n') {
        m_line++;
        m_column = 1;
      } else if (m_source[i] == '\r') {
        if (i + 1 < m_source.size() && m_source[i + 1] == '\n') {
          i++; // CRLFをスキップ
        }
        m_line++;
        m_column = 1;
      } else {
        m_column++;
      }
    }
  }
}

std::string Scanner::getMarkedString() const {
  if (m_marked_position > m_current) {
    return "";
  }
  
  return std::string(m_source.substr(m_marked_position, m_current - m_marked_position));
}

std::string Scanner::getStringFrom(size_t start) const {
  if (start > m_current || start >= m_source.size()) {
    return "";
  }
  
  return std::string(m_source.substr(start, m_current - start));
}

size_t Scanner::getCurrentIndex() const {
  return m_current;
}

bool Scanner::isWhitespace(char c) {
  // ECMAScript仕様に基づく空白文字の定義
  return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\xA0' || 
         c == '\r' || c == '\n' || c == '\u2028' || c == '\u2029';
}

bool Scanner::isDigit(char c) {
  return c >= '0' && c <= '9';
}

bool Scanner::isHexDigit(char c) {
  return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool Scanner::isOctalDigit(char c) {
  return c >= '0' && c <= '7';
}

bool Scanner::isBinaryDigit(char c) {
  return c == '0' || c == '1';
}

bool Scanner::isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
}

bool Scanner::isIdentifierStart(char c) {
  // ECMAScript仕様に基づく識別子の開始文字
  // ASCII文字のみ対応している簡易版
  // 本番環境では Unicode 文字も対応する必要がある
  return isAlpha(c);
}

bool Scanner::isIdentifierPart(char c) {
  // ECMAScript仕様に基づく識別子に使用できる文字
  // ASCII文字のみ対応している簡易版
  return isAlpha(c) || isDigit(c);
}

bool Scanner::isLineTerminator(char c) {
  // ECMAScript仕様に基づく行終端文字
  return c == '\n' || c == '\r' || c == '\u2028' || c == '\u2029';
}

} // namespace core
} // namespace aerojs 