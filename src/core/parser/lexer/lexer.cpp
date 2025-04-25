/*******************************************************************************
 * @file src/core/parser/lexer/lexer.cpp
 * @brief 高性能JavaScriptレキサー（字句解析器）の実装
 * @version 2.1.0
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 ******************************************************************************/

// 対応するヘッダーを最初にインクルード
#include "lexer.h"

// 必要な標準ライブラリ
#include <atomic>
#include <iostream>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// 依存する他のモジュール
#include "character_stream.h"
#include "comment.h"
#include "lexer_options.h"
#include "lexer_state_manager.h"
#include "lexer_stats.h"
#include "src/core/sourcemap/source_location.h"
#include "token.h"
#include "token_cache.h"
#include "token_lookup_table.h"
#include "src/core/parser/parser_error.h"
#include "src/utils/logger.h"
#include "src/utils/memory/arena_allocator.h"
#include "src/utils/thread/thread_pool.h"
#include "src/utils/metrics/metrics_collector.h"

namespace aerojs {
namespace core {
namespace parser {
namespace lexer {

/**
 * @brief Lexerクラスの実装
 * 
 * JavaScriptソースコードを字句解析し、トークン列に変換する
 */
Lexer::Lexer(std::unique_ptr<CharacterStream> stream, const LexerOptions& options)
    : stream_(std::move(stream)),
      options_(options),
      current_token_(),
      lookup_table_(std::make_unique<TokenLookupTable>()),
      token_cache_(std::make_unique<TokenCache>(options.token_cache_size)),
      state_manager_(std::make_unique<LexerStateManager>()),
      stats_(std::make_unique<LexerStats>()),
      logger_(nullptr),
      allocator_(nullptr),
      thread_pool_(nullptr),
      metrics_(nullptr) {
  
  // キーワードテーブルの初期化
  lookup_table_->InitializeKeywords();
  
  // 最初のトークンを読み込む
  Advance();
}

Lexer::~Lexer() = default;

void Lexer::SetLogger(std::shared_ptr<utils::Logger> logger) {
  logger_ = std::move(logger);
}

void Lexer::SetAllocator(std::shared_ptr<utils::memory::ArenaAllocator> allocator) {
  allocator_ = std::move(allocator);
}

void Lexer::SetThreadPool(std::shared_ptr<utils::thread::ThreadPool> thread_pool) {
  thread_pool_ = std::move(thread_pool);
}

void Lexer::SetMetricsCollector(std::shared_ptr<utils::metrics::MetricsCollector> metrics) {
  metrics_ = std::move(metrics);
}

Token Lexer::CurrentToken() const {
  return current_token_;
}

Token Lexer::NextToken() {
  Advance();
  return current_token_;
}

Token Lexer::PeekToken() {
  // 現在位置を保存
  size_t current_pos = stream_->Position();
  
  // 現在のトークンを保存
  Token current = current_token_;
  
  // 次のトークンを取得
  Advance();
  Token next = current_token_;
  
  // 状態を元に戻す
  current_token_ = current;
  stream_->SetPosition(current_pos);
  
  return next;
}

void Lexer::Advance() {
  // 空白文字とコメントをスキップ
  SkipWhitespaceAndComments();
  
  // ファイルの終端に達した場合
  if (stream_->IsAtEnd()) {
    current_token_ = Token(TokenType::EOF_TOKEN, "", SourceLocation(stream_->Position(), stream_->Position()));
    return;
  }
  
  // トークンの開始位置を記録
  size_t start_pos = stream_->Position();
  
  // 現在の文字を取得
  char c = stream_->Current();
  
  // 文字の種類に応じてトークンを識別
  if (IsDigit(c)) {
    ScanNumber();
  } else if (IsIdentifierStart(c)) {
    ScanIdentifier();
  } else if (c == '"' || c == '\'') {
    ScanString();
  } else if (c == '`') {
    ScanTemplate();
  } else if (c == '/') {
    // 正規表現リテラルかどうかを判断
    if (IsRegExpStart()) {
      ScanRegExp();
    } else {
      ScanOperator();
    }
  } else {
    ScanOperator();
  }
  
  // 統計情報の更新
  stats_->IncrementTokenCount();
  
  // メトリクス収集
  if (metrics_) {
    metrics_->RecordTokenProcessed(current_token_.Type());
  }
}

void Lexer::SkipWhitespaceAndComments() {
  bool skipped = true;
  
  while (skipped && !stream_->IsAtEnd()) {
    skipped = false;
    
    // 空白文字のスキップ
    while (!stream_->IsAtEnd() && IsWhitespace(stream_->Current())) {
      stream_->Advance();
      skipped = true;
    }
    
    // コメントのスキップ
    if (!stream_->IsAtEnd() && stream_->Current() == '/') {
      if (stream_->Peek(1) == '/') {
        // 単一行コメント
        SkipLineComment();
        skipped = true;
      } else if (stream_->Peek(1) == '*') {
        // 複数行コメント
        SkipBlockComment();
        skipped = true;
      }
    }
  }
}

void Lexer::SkipLineComment() {
  // '//'をスキップ
  stream_->Advance();
  stream_->Advance();
  
  // 行末または入力終端までスキップ
  while (!stream_->IsAtEnd() && !IsLineTerminator(stream_->Current())) {
    stream_->Advance();
  }
}

void Lexer::SkipBlockComment() {
  // '/*'をスキップ
  stream_->Advance();
  stream_->Advance();
  
  // '*/'が見つかるまでスキップ
  while (!stream_->IsAtEnd()) {
    if (stream_->Current() == '*' && stream_->Peek(1) == '/') {
      // '*/'をスキップ
      stream_->Advance();
      stream_->Advance();
      return;
    }
    stream_->Advance();
  }
  
  // 閉じられていないブロックコメントはエラー
  if (logger_) {
    logger_->Warn("閉じられていないブロックコメントがあります");
  }
}

void Lexer::ScanNumber() {
  size_t start_pos = stream_->Position();
  bool is_float = false;
  bool is_hex = false;
  bool is_binary = false;
  bool is_octal = false;
  
  // 0xなどの接頭辞をチェック
  if (stream_->Current() == '0') {
    stream_->Advance();
    
    if (!stream_->IsAtEnd()) {
      char next = stream_->Current();
      if (next == 'x' || next == 'X') {
        // 16進数
        is_hex = true;
        stream_->Advance();
        ScanHexDigits();
      } else if (next == 'b' || next == 'B') {
        // 2進数
        is_binary = true;
        stream_->Advance();
        ScanBinaryDigits();
      } else if (next == 'o' || next == 'O') {
        // 8進数
        is_octal = true;
        stream_->Advance();
        ScanOctalDigits();
      } else if (IsDigit(next)) {
        // 旧式の8進数または10進数
        ScanDigits();
      }
    }
  } else {
    // 通常の10進数
    ScanDigits();
  }
  
  // 小数部分
  if (!is_hex && !is_binary && !is_octal && !stream_->IsAtEnd() && stream_->Current() == '.') {
    is_float = true;
    stream_->Advance();
    ScanDigits();
  }
  
  // 指数部分
  if (!is_binary && !is_octal && !stream_->IsAtEnd() && 
      (stream_->Current() == 'e' || stream_->Current() == 'E')) {
    is_float = true;
    stream_->Advance();
    
    // 符号
    if (!stream_->IsAtEnd() && (stream_->Current() == '+' || stream_->Current() == '-')) {
      stream_->Advance();
    }
    
    // 指数の数字部分
    if (!stream_->IsAtEnd() && IsDigit(stream_->Current())) {
      ScanDigits();
    } else {
      // 指数の後に数字がない場合はエラー
      if (logger_) {
        logger_->Error("数値リテラルの指数部分に数字がありません");
      }
    }
  }
  
  // BigInt
  bool is_bigint = false;
  if (!is_float && !stream_->IsAtEnd() && stream_->Current() == 'n') {
    is_bigint = true;
    stream_->Advance();
  }
  
  // トークンの作成
  size_t end_pos = stream_->Position();
  std::string_view lexeme = stream_->Substring(start_pos, end_pos);
  
  TokenType type = is_float ? TokenType::FLOAT_LITERAL : 
                   is_bigint ? TokenType::BIGINT_LITERAL : 
                   TokenType::INTEGER_LITERAL;
  
  current_token_ = Token(type, std::string(lexeme), SourceLocation(start_pos, end_pos));
}

void Lexer::ScanDigits() {
  while (!stream_->IsAtEnd() && IsDigit(stream_->Current())) {
    stream_->Advance();
  }
}

void Lexer::ScanHexDigits() {
  while (!stream_->IsAtEnd() && IsHexDigit(stream_->Current())) {
    stream_->Advance();
  }
}

void Lexer::ScanBinaryDigits() {
  while (!stream_->IsAtEnd() && IsBinaryDigit(stream_->Current())) {
    stream_->Advance();
  }
}

void Lexer::ScanOctalDigits() {
  while (!stream_->IsAtEnd() && IsOctalDigit(stream_->Current())) {
    stream_->Advance();
  }
}

void Lexer::ScanIdentifier() {
  size_t start_pos = stream_->Position();
  
  // 識別子の先頭文字
  stream_->Advance();
  
  // 識別子の残りの部分
  while (!stream_->IsAtEnd() && IsIdentifierPart(stream_->Current())) {
    stream_->Advance();
  }
  
  size_t end_pos = stream_->Position();
  std::string_view lexeme = stream_->Substring(start_pos, end_pos);
  std::string lexeme_str(lexeme);
  
  // キャッシュをチェック
  auto cached_token = token_cache_->Get(lexeme_str);
  if (cached_token) {
    current_token_ = *cached_token;
    current_token_.UpdateLocation(SourceLocation(start_pos, end_pos));
    stats_->IncrementCacheHits();
    return;
  }
  
  // キーワードかどうかをチェック
  TokenType type = lookup_table_->FindKeyword(lexeme_str);
  
  // トークンの作成
  current_token_ = Token(type, lexeme_str, SourceLocation(start_pos, end_pos));
  
  // キャッシュに追加
  token_cache_->Add(lexeme_str, current_token_);
}

void Lexer::ScanString() {
  size_t start_pos = stream_->Position();
  char quote = stream_->Current();
  
  // 開始引用符をスキップ
  stream_->Advance();
  
  std::string value;
  
  while (!stream_->IsAtEnd() && stream_->Current() != quote) {
    // エスケープシーケンス
    if (stream_->Current() == '\\') {
      stream_->Advance();
      
      if (stream_->IsAtEnd()) {
        break;
      }
      
      char escaped = stream_->Current();
      switch (escaped) {
        case 'n': value += '\n'; break;
        case 'r': value += '\r'; break;
        case 't': value += '\t'; break;
        case 'b': value += '\b'; break;
        case 'f': value += '\f'; break;
        case 'v': value += '\v'; break;
        case '0': value += '\0'; break;
        case '\\': value += '\\'; break;
        case '\'': value += '\''; break;
        case '"': value += '"'; break;
        case '`': value += '`'; break;
        case 'u': {
          // Unicode エスケープシーケンス
          value += ScanUnicodeEscapeSequence();
          continue;
        }
        case 'x': {
          // 16進数エスケープシーケンス
          stream_->Advance();
          if (stream_->IsAtEnd() || !IsHexDigit(stream_->Current())) {
            if (logger_) {
              logger_->Error("不正な16進数エスケープシーケンスです");
            }
            value += 'x';
          } else {
            int hex_value = 0;
            for (int i = 0; i < 2 && !stream_->IsAtEnd() && IsHexDigit(stream_->Current()); i++) {
              hex_value = hex_value * 16 + HexDigitValue(stream_->Current());
              stream_->Advance();
            }
            value += static_cast<char>(hex_value);
            continue;
          }
          break;
        }
        default:
          // 認識できないエスケープシーケンスは文字をそのまま使用
          value += escaped;
      }
    } else if (IsLineTerminator(stream_->Current())) {
      // 文字列リテラル内の改行はエラー（テンプレートリテラルを除く）
      if (logger_) {
        logger_->Error("文字列リテラル内に改行があります");
      }
      break;
    } else {
      value += stream_->Current();
    }
    
    stream_->Advance();
  }
  
  // 終了引用符をスキップ
  if (!stream_->IsAtEnd() && stream_->Current() == quote) {
    stream_->Advance();
  } else {
    // 閉じられていない文字列リテラルはエラー
    if (logger_) {
      logger_->Error("閉じられていない文字列リテラルがあります");
    }
  }
  
  size_t end_pos = stream_->Position();
  
  // トークンの作成
  current_token_ = Token(TokenType::STRING_LITERAL, value, SourceLocation(start_pos, end_pos));
}

std::string Lexer::ScanUnicodeEscapeSequence() {
  stream_->Advance(); // 'u'をスキップ
  
  if (stream_->IsAtEnd()) {
    return "u";
  }
  
  // \u{XXXXXX} 形式のチェック
  if (stream_->Current() == '{') {
    stream_->Advance();
    
    int code_point = 0;
    int digit_count = 0;
    
    while (!stream_->IsAtEnd() && stream_->Current() != '}' && digit_count < 6) {
      if (!IsHexDigit(stream_->Current())) {
        break;
      }
      
      code_point = code_point * 16 + HexDigitValue(stream_->Current());
      digit_count++;
      stream_->Advance();
    }
    
    if (stream_->IsAtEnd() || stream_->Current() != '}') {
      if (logger_) {
        logger_->Error("不正なUnicodeエスケープシーケンスです");
      }
      return "u{";
    }
    
    stream_->Advance(); // '}'をスキップ
    
    // コードポイントからUTF-8文字列に変換
    return CodePointToUTF8(code_point);
  } else {
    // \uXXXX 形式
    int code_point = 0;
    
    for (int i = 0; i < 4; i++) {
      if (stream_->IsAtEnd() || !IsHexDigit(stream_->Current())) {
        if (logger_) {
          logger_->Error("不正なUnicodeエスケープシーケンスです");
        }
        return "u";
      }
      
      code_point = code_point * 16 + HexDigitValue(stream_->Current());
      stream_->Advance();
    }
    
    // コードポイントからUTF-8文字列に変換
    return CodePointToUTF8(code_point);
  }
}

std::string Lexer::CodePointToUTF8(int code_point) {
  std::string result;
  
  if (code_point <= 0x7F) {
    // 1バイト (0xxxxxxx)
    result += static_cast<char>(code_point);
  } else if (code_point <= 0x7FF) {
    // 2バイト (110xxxxx 10xxxxxx)
    result += static_cast<char>(0xC0 | (code_point >> 6));
    result += static_cast<char>(0x80 | (code_point & 0x3F));
  } else if (code_point <= 0xFFFF) {
    // 3バイト (1110xxxx 10xxxxxx 10xxxxxx)
    result += static_cast<char>(0xE0 | (code_point >> 12));
    result += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
    result += static_cast<char>(0x80 | (code_point & 0x3F));
  } else if (code_point <= 0x10FFFF) {
    // 4バイト (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
    result += static_cast<char>(0xF0 | (code_point >> 18));
    result += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
    result += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
    result += static_cast<char>(0x80 | (code_point & 0x3F));
  } else {
    // 無効なコードポイント
    if (logger_) {
      logger_->Error("無効なUnicodeコードポイントです");
    }
    result = "�"; // 置換文字
  }
  
  return result;
}

void Lexer::ScanTemplate() {
  size_t start_pos = stream_->Position();
  
  // 開始バッククォートをスキップ
  stream_->Advance();
  
  std::string value;
  bool is_head = true;
  bool is_tail = true;
  
  while (!stream_->IsAtEnd() && stream_->Current() != '`') {
    if (stream_->Current() == '$' && stream_->Peek(1) == '{') {
      // テンプレート式の開始
      is_tail = false;
      stream_->Advance(); // '$'をスキップ
      stream_->Advance(); // '{'をスキップ
      break;
    } else if (stream_->Current() == '\\') {
      // エスケープシーケンス
      stream_->Advance();
      
      if (stream_->IsAtEnd()) {
        break;
      }
      
      char escaped = stream_->Current();
      switch (escaped) {
        case 'n': value += '\n'; break;
        case 'r': value += '\r'; break;
        case 't': value += '\t'; break;
        case 'b': value += '\b'; break;
        case 'f': value += '\f'; break;
        case 'v': value += '\v'; break;
        case '0': value += '\0'; break;
        case '\\': value += '\\'; break;
        case '\'': value += '\''; break;
        case '"': value += '"'; break;
        case '`': value += '`'; break;
        case '$': value += '$'; break;
        case 'u': {
          // Unicode エスケープシーケンス
          value += ScanUnicodeEscapeSequence();
          continue;
        }
        case 'x': {
          // 16進数エスケープシーケンス
          stream_->Advance();
          if (stream_->IsAtEnd() || !IsHexDigit(stream_->Current())) {
            if (logger_) {
              logger_->Error("不正な16進数エスケープシーケンスです");
            }
            value += 'x';
          } else {
            int hex_value = 0;
            for (int i = 0; i < 2 && !stream_->IsAtEnd() && IsHexDigit(stream_->Current()); i++) {
              hex_value = hex_value * 16 + HexDigitValue(stream_->Current());
              stream_->Advance();
            }
            value += static_cast<char>(hex_value);
            continue;
          }
          break;
        }
        default:
          // テンプレートリテラルでは、認識できないエスケープシーケンスも許可される
          value += escaped;
      }
    } else {
      value += stream_->Current();
    }
    
    stream_->Advance();
  }
  
  // 終了バッククォートをスキップ
  if (!stream_->IsAtEnd() && stream_->Current() == '`') {
    stream_->Advance();
  } else if (is_tail) {
    // 閉じられていないテンプレートリテラルはエラー
    if (logger_) {
      logger_->Error("閉じられていないテンプレートリテラルがあります");
    }
  }
  
  size_t end_pos = stream_->Position();
  
  // トークンの作成
  TokenType type;
  if (is_head && is_tail) {
    type = TokenType::TEMPLATE_LITERAL;
  } else if (is_head) {
    type = TokenType::TEMPLATE_HEAD;
  } else if (is_tail) {
    type = TokenType::TEMPLATE_TAIL;
  } else {
    type = TokenType::TEMPLATE_MIDDLE;
  }
  
  current_token_ = Token(type, value, SourceLocation(start_pos, end_pos));
}

bool Lexer::IsRegExpStart() const {
  // 正規表現リテラルの開始かどうかを判断するヒューリスティック
  // 例: 演算子の後や制御構文の後は正規表現リテラルの可能性が高い
  
  // 現在のトークンの種類に基づいて判断
  switch (current_token_.Type()) {
    case TokenType::IDENTIFIER:
    case TokenType::INTEGER_LITERAL:
    case TokenType::FLOAT_LITERAL:
    case TokenType::STRING_LITERAL:
    case TokenType::TEMPLATE_LITERAL:
    case TokenType::TEMPLATE_TAIL:
    case TokenType::REGEXP_LITERAL:
    case TokenType::TRUE:
    case TokenType::FALSE:
    case TokenType::NULL_LITERAL:
    case TokenType::THIS:
    case TokenType::SUPER:
    case TokenType::RIGHT_PAREN:
    case TokenType::RIGHT_BRACKET:
    case TokenType::RIGHT_BRACE:
    case TokenType::INCREMENT:
    case TokenType::DECREMENT:
      // これらの後に/が来た場合は除算演算子
      return false;
    
    default:
      // それ以外の場合は正規表現リテラルの可能性
      return true;
  }
}

void Lexer::ScanRegExp() {
  size_t start_pos = stream_->Position();
  
  // '/'をスキップ
  stream_->Advance();
  
  std::string pattern;
  
  // パターン部分の解析
  bool in_character_class = false;
  
  while (!stream_->IsAtEnd() && 
         (stream_->Current() != '/' || in_character_class)) {
    
    if (stream_->Current() == '\\') {
      // エスケープシーケンス処理
      pattern += stream_->Current();
      stream_->Advance();
      
      if (stream_->IsAtEnd()) {
        break;
      }
      
      pattern += stream_->Current();
    } else if (stream_->Current() == '[') {
      in_character_class = true;
      pattern += stream_->Current();
    } else if (stream_->Current() == ']') {
      in_character_class = false;
      pattern += stream_->Current();
    } else if (IsLineTerminator(stream_->Current())) {
      // 正規表現内の改行はエラー
      if (logger_) {
        logger_->Error("正規表現リテラル内に改行が含まれています");
      }
      break;
    } else {
      pattern += stream_->Current();
    }
    
    stream_->Advance();
  }
  
  // 終了'/'の処理
  if (!stream_->IsAtEnd() && stream_->Current() == '/') {
    stream_->Advance();
  } else {
    if (logger_) {
      logger_->Error("正規表現リテラルが閉じられていません");
    }
  }
  
  // フラグ部分の解析
  std::string flags;
  
  while (!stream_->IsAtEnd() && IsIdentifierPart(stream_->Current())) {
    flags += stream_->Current();
    stream_->Advance();
  }
  
  size_t end_pos = stream_->Position();
  
  // トークンの生成
  current_token_ = Token(TokenType::REGEXP_LITERAL, pattern, SourceLocation(start_pos, end_pos));
  current_token_.SetFlags(flags);
}

void Lexer::ScanOperator() {
  size_t start_pos = stream_->Position();
  char c = stream_->Current();
  
  stream_->Advance();
  TokenType type;
  
  switch (c) {
    case '{': type = TokenType::LEFT_BRACE; break;
    case '}': type = TokenType::RIGHT_BRACE; break;
    case '(': type = TokenType::LEFT_PAREN; break;
    case ')': type = TokenType::RIGHT_PAREN; break;
    case '[': type = TokenType::LEFT_BRACKET; break;
    case ']': type = TokenType::RIGHT_BRACKET; break;
    case '.': {
      // スプレッド演算子の検出
      if (!stream_->IsAtEnd() && stream_->Current() == '.' && 
          !stream_->IsAtEnd(1) && stream_->Peek(1) == '.') {
        stream_->Advance();
        stream_->Advance();
        type = TokenType::SPREAD;
      } else {
        type = TokenType::DOT;
      }
      break;
    }
    case ';': type = TokenType::SEMICOLON; break;
    case ',': type = TokenType::COMMA; break;
    case ':': type = TokenType::COLON; break;
    case '?': {
      // オプショナルチェーンとナリッシュコアレッシングの検出
      if (!stream_->IsAtEnd() && stream_->Current() == '.') {
        stream_->Advance();
        type = TokenType::OPTIONAL_CHAIN;
      } else if (!stream_->IsAtEnd() && stream_->Current() == '?') {
        stream_->Advance();
        type = TokenType::NULLISH_COALESCING;
      } else {
        type = TokenType::QUESTION;
      }
      break;
    }
    case '~': type = TokenType::BITWISE_NOT; break;
    case '!': {
      // 否定演算子と不等価演算子の検出
      if (!stream_->IsAtEnd() && stream_->Current() == '=') {
        stream_->Advance();
        if (!stream_->IsAtEnd() && stream_->Current() == '=') {
          stream_->Advance();
          type = TokenType::STRICT_NOT_EQUAL;
        } else {
          type = TokenType::NOT_EQUAL;
        }
      } else {
        type = TokenType::LOGICAL_NOT;
      }
      break;
    }
    case '+': {
      // 加算演算子と増分演算子の検出
      if (!stream_->IsAtEnd() && stream_->Current() == '+') {
        stream_->Advance();
        type = TokenType::INCREMENT;
      } else if (!stream_->IsAtEnd() && stream_->Current() == '=') {
        stream_->Advance();
        type = TokenType::PLUS_ASSIGN;
      } else {
        type = TokenType::PLUS;
      }
      break;
    }
    case '-': {
      // 減算演算子と減分演算子の検出
      if (!stream_->IsAtEnd() && stream_->Current() == '-') {
        stream_->Advance();
        type = TokenType::DECREMENT;
      } else if (!stream_->IsAtEnd() && stream_->Current() == '=') {
        stream_->Advance();
        type = TokenType::MINUS_ASSIGN;
      } else {
        type = TokenType::MINUS;
      }
      break;
    }
    case '*': {
      // 乗算演算子と累乗演算子の検出
      if (!stream_->IsAtEnd() && stream_->Current() == '*') {
        stream_->Advance();
        if (!stream_->IsAtEnd() && stream_->Current() == '=') {
          stream_->Advance();
          type = TokenType::EXPONENT_ASSIGN;
        } else {
          type = TokenType::EXPONENT;
        }
      } else if (!stream_->IsAtEnd() && stream_->Current() == '=') {
        stream_->Advance();
        type = TokenType::MULTIPLY_ASSIGN;
      } else {
        type = TokenType::MULTIPLY;
      }
      break;
    }
    case '/': {
      // 除算演算子の検出
      if (!stream_->IsAtEnd() && stream_->Current() == '=') {
        stream_->Advance();
        type = TokenType::DIVIDE_ASSIGN;
      } else {
        type = TokenType::DIVIDE;
      }
      break;
    }
    case '%': {
      // 剰余演算子の検出
      if (!stream_->IsAtEnd() && stream_->Current() == '=') {
        stream_->Advance();
        type = TokenType::MODULO_ASSIGN;
      } else {
        type = TokenType::MODULO;
      }
      break;
    }
    case '&': {
      // ビット演算子と論理演算子の検出
      if (!stream_->IsAtEnd() && stream_->Current() == '&') {
        stream_->Advance();
        if (!stream_->IsAtEnd() && stream_->Current() == '=') {
          stream_->Advance();
          type = TokenType::LOGICAL_AND_ASSIGN;
        } else {
          type = TokenType::LOGICAL_AND;
        }
      } else if (!stream_->IsAtEnd() && stream_->Current() == '=') {
        stream_->Advance();
        type = TokenType::BITWISE_AND_ASSIGN;
      } else {
        type = TokenType::BITWISE_AND;
      }
      break;
    }
    case '|': {
      // ビット演算子と論理演算子の検出
      if (!stream_->IsAtEnd() && stream_->Current() == '|') {
        stream_->Advance();
        if (!stream_->IsAtEnd() && stream_->Current() == '=') {
          stream_->Advance();
          type = TokenType::LOGICAL_OR_ASSIGN;
        } else {
          type = TokenType::LOGICAL_OR;
        }
      } else if (!stream_->IsAtEnd() && stream_->Current() == '=') {
        stream_->Advance();
        type = TokenType::BITWISE_OR_ASSIGN;
      } else {
        type = TokenType::BITWISE_OR;
      }
      break;
    }
    case '^': {
      // XOR演算子の検出
      if (!stream_->IsAtEnd() && stream_->Current() == '=') {
        stream_->Advance();
        type = TokenType::BITWISE_XOR_ASSIGN;
      } else {
        type = TokenType::BITWISE_XOR;
      }
      break;
    }
    case '<': {
      // 比較演算子とシフト演算子の検出
      if (!stream_->IsAtEnd() && stream_->Current() == '<') {
        stream_->Advance();
        if (!stream_->IsAtEnd() && stream_->Current() == '=') {
          stream_->Advance();
          type = TokenType::LEFT_SHIFT_ASSIGN;
        } else {
          type = TokenType::LEFT_SHIFT;
        }
      } else if (!stream_->IsAtEnd() && stream_->Current() == '=') {
        stream_->Advance();
        type = TokenType::LESS_EQUAL;
      } else {
        type = TokenType::LESS_THAN;
      }
      break;
    }
    case '>': {
      // 比較演算子とシフト演算子の検出
      if (!stream_->IsAtEnd() && stream_->Current() == '>') {
        stream_->Advance();
        if (!stream_->IsAtEnd() && stream_->Current() == '>') {
          stream_->Advance();
          if (!stream_->IsAtEnd() && stream_->Current() == '=') {
            stream_->Advance();
            type = TokenType::UNSIGNED_RIGHT_SHIFT_ASSIGN;
          } else {
            type = TokenType::UNSIGNED_RIGHT_SHIFT;
          }
        } else if (!stream_->IsAtEnd() && stream_->Current() == '=') {
          stream_->Advance();
          type = TokenType::RIGHT_SHIFT_ASSIGN;
        } else {
          type = TokenType::RIGHT_SHIFT;
        }
      } else if (!stream_->IsAtEnd() && stream_->Current() == '=') {
        stream_->Advance();
        type = TokenType::GREATER_EQUAL;
      } else {
        type = TokenType::GREATER_THAN;
      }
      break;
    }
    case '=': {
      // 代入演算子と等価演算子の検出
      if (!stream_->IsAtEnd() && stream_->Current() == '=') {
        stream_->Advance();
        if (!stream_->IsAtEnd() && stream_->Current() == '=') {
          stream_->Advance();
          type = TokenType::STRICT_EQUAL;
        } else {
          type = TokenType::EQUAL;
        }
      } else if (!stream_->IsAtEnd() && stream_->Current() == '>') {
        stream_->Advance();
        type = TokenType::ARROW;
      } else {
        type = TokenType::ASSIGN;
      }
      break;
    }
    default:
      type = TokenType::UNKNOWN;
      if (logger_) {
        logger_->Error("不明な演算子: " + std::string(1, c));
      }
      break;
  }
  
  size_t end_pos = stream_->Position();
  current_token_ = Token(type, "", SourceLocation(start_pos, end_pos));
}

class Logger {
public:
  virtual ~Logger() = default;
  virtual void Error(const std::string& message) = 0;
  virtual void Warning(const std::string& message) = 0;
  virtual void Info(const std::string& message) = 0;
};

namespace utils {
class Logger {
public:
  virtual ~Logger() = default;
  virtual void Error(const std::string& message) = 0;
  virtual void Warning(const std::string& message) = 0;
  virtual void Info(const std::string& message) = 0;
};
namespace memory {
class ArenaAllocator {
public:
  ArenaAllocator(size_t initial_size = 4096);
  ~ArenaAllocator();
  
  void* Allocate(size_t size, size_t alignment = 8);
  void Reset();
  size_t UsedMemory() const;
  
private:
  struct Block {
    void* memory;
    size_t size;
    size_t used;
    Block* next;
  };
  
  Block* current_block_;
  size_t total_allocated_;
};
}  // namespace memory

namespace thread {
class ThreadPool {
public:
  ThreadPool(size_t num_threads = 0);
  ~ThreadPool();
  
  template<typename F, typename... Args>
  auto Enqueue(F&& f, Args&&... args);
  
  void WaitAll();
  size_t NumThreads() const;
  
private:
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex queue_mutex_;
  std::condition_variable condition_;
  bool stop_;
};
}  // namespace thread

namespace metrics {
class MetricsCollector {
public:
  MetricsCollector();
  
  void RecordTime(const std::string& name, double time_ms);
  void RecordCount(const std::string& name, size_t count);
  void RecordSize(const std::string& name, size_t size_bytes);
  
  std::map<std::string, double> GetTimings() const;
  std::map<std::string, size_t> GetCounts() const;
  std::map<std::string, size_t> GetSizes() const;
  
  void Reset();
  
private:
  std::map<std::string, double> timings_;
  std::map<std::string, size_t> counts_;
  std::map<std::string, size_t> sizes_;
  std::mutex mutex_;
};
}  // namespace metrics
}  // namespace utils
}  // namespace aerojs

// 外部依存クラスの実装
class CharacterStream {
public:
  CharacterStream(std::string_view source) : source_(source), position_(0) {}
  
  void Advance() {
    if (!IsAtEnd()) {
      position_++;
    }
  }
  
  char Current() const {
    return IsAtEnd() ? '\0' : source_[position_];
  }
  
  char Peek(size_t offset) const {
    size_t pos = position_ + offset;
    return (pos < source_.length()) ? source_[pos] : '\0';
  }
  
  bool IsAtEnd() const {
    return position_ >= source_.length();
  }
  
  bool IsAtEnd(size_t offset) const {
    return position_ + offset >= source_.length();
  }
  
  size_t Position() const {
    return position_;
  }
  
  void SetPosition(size_t position) {
    position_ = std::min(position, source_.length());
  }
  
  void Reset() {
    position_ = 0;
  }
  
  std::string_view Substring(size_t start, size_t end) const {
    if (start >= source_.length() || start > end) {
      return "";
    }
    end = std::min(end, source_.length());
    return source_.substr(start, end - start);
  }
  
private:
  std::string_view source_;
  size_t position_;
};

class TokenLookupTable {
public:
  TokenLookupTable() {
    InitializeKeywords();
  }
  
  void InitializeKeywords() {
    // JavaScriptの予約語を登録
    keywords_["break"] = aerojs::core::parser::lexer::TokenType::BREAK;
    keywords_["case"] = aerojs::core::parser::lexer::TokenType::CASE;
    keywords_["catch"] = aerojs::core::parser::lexer::TokenType::CATCH;
    keywords_["class"] = aerojs::core::parser::lexer::TokenType::CLASS;
    keywords_["const"] = aerojs::core::parser::lexer::TokenType::CONST;
    keywords_["continue"] = aerojs::core::parser::lexer::TokenType::CONTINUE;
    keywords_["debugger"] = aerojs::core::parser::lexer::TokenType::DEBUGGER;
    keywords_["default"] = aerojs::core::parser::lexer::TokenType::DEFAULT;
    keywords_["delete"] = aerojs::core::parser::lexer::TokenType::DELETE;
    keywords_["do"] = aerojs::core::parser::lexer::TokenType::DO;
    keywords_["else"] = aerojs::core::parser::lexer::TokenType::ELSE;
    keywords_["enum"] = aerojs::core::parser::lexer::TokenType::ENUM;
    keywords_["export"] = aerojs::core::parser::lexer::TokenType::EXPORT;
    keywords_["extends"] = aerojs::core::parser::lexer::TokenType::EXTENDS;
    keywords_["false"] = aerojs::core::parser::lexer::TokenType::FALSE;
    keywords_["finally"] = aerojs::core::parser::lexer::TokenType::FINALLY;
    keywords_["for"] = aerojs::core::parser::lexer::TokenType::FOR;
    keywords_["function"] = aerojs::core::parser::lexer::TokenType::FUNCTION;
    keywords_["if"] = aerojs::core::parser::lexer::TokenType::IF;
    keywords_["import"] = aerojs::core::parser::lexer::TokenType::IMPORT;
    keywords_["in"] = aerojs::core::parser::lexer::TokenType::IN;
    keywords_["instanceof"] = aerojs::core::parser::lexer::TokenType::INSTANCEOF;
    keywords_["new"] = aerojs::core::parser::lexer::TokenType::NEW;
    keywords_["null"] = aerojs::core::parser::lexer::TokenType::NULL_LITERAL;
    keywords_["return"] = aerojs::core::parser::lexer::TokenType::RETURN;
    keywords_["super"] = aerojs::core::parser::lexer::TokenType::SUPER;
    keywords_["switch"] = aerojs::core::parser::lexer::TokenType::SWITCH;
    keywords_["this"] = aerojs::core::parser::lexer::TokenType::THIS;
    keywords_["throw"] = aerojs::core::parser::lexer::TokenType::THROW;
    keywords_["true"] = aerojs::core::parser::lexer::TokenType::TRUE;
    keywords_["try"] = aerojs::core::parser::lexer::TokenType::TRY;
    keywords_["typeof"] = aerojs::core::parser::lexer::TokenType::TYPEOF;
    keywords_["var"] = aerojs::core::parser::lexer::TokenType::VAR;
    keywords_["void"] = aerojs::core::parser::lexer::TokenType::VOID;
    keywords_["while"] = aerojs::core::parser::lexer::TokenType::WHILE;
    keywords_["with"] = aerojs::core::parser::lexer::TokenType::WITH;
    keywords_["yield"] = aerojs::core::parser::lexer::TokenType::YIELD;
    keywords_["let"] = aerojs::core::parser::lexer::TokenType::LET;
    keywords_["static"] = aerojs::core::parser::lexer::TokenType::STATIC;
    keywords_["async"] = aerojs::core::parser::lexer::TokenType::ASYNC;
    keywords_["await"] = aerojs::core::parser::lexer::TokenType::AWAIT;
    keywords_["of"] = aerojs::core::parser::lexer::TokenType::OF;
  }
  
  aerojs::core::parser::lexer::TokenType FindKeyword(const std::string& identifier) const {
    auto it = keywords_.find(identifier);
    if (it != keywords_.end()) {
      return it->second;
    }
    return aerojs::core::parser::lexer::TokenType::IDENTIFIER;
  }
  
private:
  std::unordered_map<std::string, aerojs::core::parser::lexer::TokenType> keywords_;
};

class TokenCache {
public:
  TokenCache(size_t capacity = 1024) : capacity_(capacity) {}
  
  void Add(const std::string& key, const aerojs::core::parser::lexer::Token& token) {
    if (cache_.size() >= capacity_) {
      // キャッシュが一杯の場合、最も古いエントリを削除
      cache_.erase(cache_.begin());
    }
    cache_[key] = token;
  }
  
  std::optional<aerojs::core::parser::lexer::Token> Get(const std::string& key) {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      return it->second;
    }
    return std::nullopt;
  }
  
  void Clear() {
    cache_.clear();
  }
  
private:
  std::unordered_map<std::string, aerojs::core::parser::lexer::Token> cache_;
  size_t capacity_;
};

class LexerStateManager {
public:
  LexerStateManager() = default;
  
  struct State {
    size_t position;
    aerojs::core::parser::lexer::Token current_token;
  };
  
  void PushState(size_t position, const aerojs::core::parser::lexer::Token& token) {
    states_.push({position, token});
  }
  
  std::optional<State> PopState() {
    if (states_.empty()) {
      return std::nullopt;
    }
    State state = states_.top();
    states_.pop();
    return state;
  }
  
  bool HasSavedStates() const {
    return !states_.empty();
  }
  
private:
  std::stack<State> states_;
};

struct SourceTextChunk {
  std::string_view text;
  aerojs::core::parser::lexer::SourceLocation startLocation;
  
  SourceTextChunk(std::string_view t, const aerojs::core::parser::lexer::SourceLocation& loc)
    : text(t), startLocation(loc) {}
};

namespace aerojs {
namespace core {
namespace parser {
namespace lexer {

// Lexerクラスの実装

}  // namespace lexer
}  // namespace parser
}  // namespace core
}  // namespace aerojs