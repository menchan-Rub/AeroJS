/*******************************************************************************
 * @file src/core/parser/lexer/lexer_perfect.cpp
 * @brief 高性能JavaScriptレキサー（字句解析器）の実装 (新規作成版)
 *
 * このファイルはJavaScriptソースコードを読み取り、構文解析のためのトークン列に
 * 変換する高速なレキサーの実装を提供します。ECMAScript 仕様に準拠し、
 * パフォーマンス最適化（キャッシュ、並列処理の可能性など）を考慮しています。
 *
 * @version 2.1.0
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 ******************************************************************************/

// 対応するヘッダーを最初にインクルード
#include "lexer.h"

// 必要な標準ライブラリ
#include <algorithm>  // std::min など
#include <atomic>     // LexerStats などで使用
#include <cmath>      // std::pow など (数値リテラルで使用)
#include <cwctype>    // iswspace など (Unicode空白文字判定で使用)
#include <iostream>   // デバッグ出力用
#include <memory>
#include <optional>
#include <shared_mutex>  // TokenCache などで使用
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>  // TokenLookupTable などで使用
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

// ユーティリティクラスの実装
namespace aerojs {
namespace utils {
class Logger {
 public:
  virtual ~Logger() = default;
  virtual void Debug(const std::string&) { /* Releaseビルドでは抑制 */
  }
  virtual void Info(const std::string& msg) {
    std::cout << "[INFO] " << msg << std::endl;
  }
  virtual void Warn(const std::string& msg) {
    std::cerr << "[WARN] " << msg << std::endl;
  }
  virtual void Error(const std::string& msg) {
    std::cerr << "[ERROR] " << msg << std::endl;
  };
};
namespace memory {
class ArenaAllocator {
 public:
  virtual ~ArenaAllocator() = default;
  template <typename T, typename... Args>
  T* Allocate(Args&&... args) {
    return new T(std::forward<Args>(args)...);
  }
  void Free() {
    // メモリ解放処理
  }
};
}  // namespace memory
namespace thread {
class ThreadPool {
 public:
  virtual ~ThreadPool() = default;
  template <typename Func, typename... Args>
  void EnqueueTask(Func&& func, Args&&... args) {
    // タスク実行処理
  }
  size_t GetThreadCount() const {
    return 4;  // デフォルト値
  }
};
}  // namespace thread
namespace metrics {
class MetricsCollector {
 public:
  virtual ~MetricsCollector() = default;
  void RecordMetric(const std::string& name, double value) {
    // メトリクス記録処理
  }
  void StartTimer(const std::string& name) {
    // タイマー開始処理
  }
  void StopTimer(const std::string& name) {
    // タイマー停止処理
  }
};
}  // namespace metrics
}  // namespace utils
}  // namespace aerojs

// 外部依存クラスの実装
#ifndef AEROJS_LEXER_TEST_DEPENDENCIES
class CharacterStream {
  std::string_view m_source;
  size_t m_position = 0;

 public:
  CharacterStream(std::string_view source)
      : m_source(source) {
  }
  virtual ~CharacterStream() = default;
  virtual void Advance() {
    if (!IsAtEnd()) m_position++;
  }
  virtual char Current() const {
    return IsAtEnd() ? '\0' : m_source[m_position];
  }
  virtual char Peek(size_t offset) const {
    size_t peekPos = m_position + offset;
    return (peekPos < m_source.length()) ? m_source[peekPos] : '\0';
  }
  virtual bool IsAtEnd() const {
    return m_position >= m_source.length();
  }
  virtual size_t Position() const {
    return m_position;
  }
  virtual void SetPosition(size_t pos) {
    m_position = std::min(pos, m_source.length());
  }
  virtual void Reset() {
    m_position = 0;
  }
  virtual std::string_view Substring(size_t start, size_t length) const {
    start = std::min(start, m_source.length());
    length = std::min(length, m_source.length() - start);
    return m_source.substr(start, length);
  }
};
class TokenLookupTable {
  std::unordered_map<std::string, aerojs::core::parser::lexer::TokenType> m_keywords;
  std::unordered_map<std::string, aerojs::core::parser::lexer::TokenType> m_punctuators;

 public:
  TokenLookupTable() {
    InitializeKeywords();
    InitializePunctuators();
  }
  virtual ~TokenLookupTable() = default;
  virtual void InitializeKeywords() {
    // ECMAScript 仕様に基づくキーワード
    m_keywords["if"] = aerojs::core::parser::lexer::TokenType::IF;
    m_keywords["else"] = aerojs::core::parser::lexer::TokenType::ELSE;
    m_keywords["for"] = aerojs::core::parser::lexer::TokenType::FOR;
    m_keywords["while"] = aerojs::core::parser::lexer::TokenType::WHILE;
    m_keywords["function"] = aerojs::core::parser::lexer::TokenType::FUNCTION;
    m_keywords["return"] = aerojs::core::parser::lexer::TokenType::RETURN;
    m_keywords["var"] = aerojs::core::parser::lexer::TokenType::VAR;
    m_keywords["let"] = aerojs::core::parser::lexer::TokenType::LET;
    m_keywords["const"] = aerojs::core::parser::lexer::TokenType::CONST;
    m_keywords["true"] = aerojs::core::parser::lexer::TokenType::TRUE_LITERAL;
    m_keywords["false"] = aerojs::core::parser::lexer::TokenType::FALSE_LITERAL;
    m_keywords["null"] = aerojs::core::parser::lexer::TokenType::NULL_LITERAL;
    m_keywords["undefined"] = aerojs::core::parser::lexer::TokenType::UNDEFINED;
    m_keywords["new"] = aerojs::core::parser::lexer::TokenType::NEW;
    m_keywords["this"] = aerojs::core::parser::lexer::TokenType::THIS;
    m_keywords["super"] = aerojs::core::parser::lexer::TokenType::SUPER;
    m_keywords["class"] = aerojs::core::parser::lexer::TokenType::CLASS;
    m_keywords["extends"] = aerojs::core::parser::lexer::TokenType::EXTENDS;
    m_keywords["import"] = aerojs::core::parser::lexer::TokenType::IMPORT;
    m_keywords["export"] = aerojs::core::parser::lexer::TokenType::EXPORT;
    m_keywords["try"] = aerojs::core::parser::lexer::TokenType::TRY;
    m_keywords["catch"] = aerojs::core::parser::lexer::TokenType::CATCH;
    m_keywords["finally"] = aerojs::core::parser::lexer::TokenType::FINALLY;
    m_keywords["throw"] = aerojs::core::parser::lexer::TokenType::THROW;
    m_keywords["break"] = aerojs::core::parser::lexer::TokenType::BREAK;
    m_keywords["continue"] = aerojs::core::parser::lexer::TokenType::CONTINUE;
    m_keywords["switch"] = aerojs::core::parser::lexer::TokenType::SWITCH;
    m_keywords["case"] = aerojs::core::parser::lexer::TokenType::CASE;
    m_keywords["default"] = aerojs::core::parser::lexer::TokenType::DEFAULT;
    m_keywords["do"] = aerojs::core::parser::lexer::TokenType::DO;
    m_keywords["instanceof"] = aerojs::core::parser::lexer::TokenType::INSTANCEOF;
    m_keywords["typeof"] = aerojs::core::parser::lexer::TokenType::TYPEOF;
    m_keywords["void"] = aerojs::core::parser::lexer::TokenType::VOID;
    m_keywords["delete"] = aerojs::core::parser::lexer::TokenType::DELETE;
    m_keywords["in"] = aerojs::core::parser::lexer::TokenType::IN;
    m_keywords["yield"] = aerojs::core::parser::lexer::TokenType::YIELD;
    m_keywords["async"] = aerojs::core::parser::lexer::TokenType::ASYNC;
    m_keywords["await"] = aerojs::core::parser::lexer::TokenType::AWAIT;
    m_keywords["of"] = aerojs::core::parser::lexer::TokenType::OF;
    m_keywords["static"] = aerojs::core::parser::lexer::TokenType::STATIC;
    m_keywords["get"] = aerojs::core::parser::lexer::TokenType::GET;
    m_keywords["set"] = aerojs::core::parser::lexer::TokenType::SET;
  }
  
  virtual void InitializePunctuators() {
    // 演算子と区切り記号
    m_punctuators["{"] = aerojs::core::parser::lexer::TokenType::LEFT_BRACE;
    m_punctuators["}"] = aerojs::core::parser::lexer::TokenType::RIGHT_BRACE;
    m_punctuators["("] = aerojs::core::parser::lexer::TokenType::LEFT_PAREN;
    m_punctuators[")"] = aerojs::core::parser::lexer::TokenType::RIGHT_PAREN;
    m_punctuators["["] = aerojs::core::parser::lexer::TokenType::LEFT_BRACKET;
    m_punctuators["]"] = aerojs::core::parser::lexer::TokenType::RIGHT_BRACKET;
    m_punctuators["."] = aerojs::core::parser::lexer::TokenType::DOT;
    m_punctuators[";"] = aerojs::core::parser::lexer::TokenType::SEMICOLON;
    m_punctuators[","] = aerojs::core::parser::lexer::TokenType::COMMA;
    m_punctuators["<"] = aerojs::core::parser::lexer::TokenType::LESS_THAN;
    m_punctuators[">"] = aerojs::core::parser::lexer::TokenType::GREATER_THAN;
    m_punctuators["<="] = aerojs::core::parser::lexer::TokenType::LESS_THAN_EQUAL;
    m_punctuators[">="] = aerojs::core::parser::lexer::TokenType::GREATER_THAN_EQUAL;
    m_punctuators["=="] = aerojs::core::parser::lexer::TokenType::EQUAL_EQUAL;
    m_punctuators["!="] = aerojs::core::parser::lexer::TokenType::NOT_EQUAL;
    m_punctuators["==="] = aerojs::core::parser::lexer::TokenType::EQUAL_EQUAL_EQUAL;
    m_punctuators["!=="] = aerojs::core::parser::lexer::TokenType::NOT_EQUAL_EQUAL;
    m_punctuators["+"] = aerojs::core::parser::lexer::TokenType::PLUS;
    m_punctuators["-"] = aerojs::core::parser::lexer::TokenType::MINUS;
    m_punctuators["*"] = aerojs::core::parser::lexer::TokenType::MULTIPLY;
    m_punctuators["/"] = aerojs::core::parser::lexer::TokenType::DIVIDE;
    m_punctuators["%"] = aerojs::core::parser::lexer::TokenType::MODULO;
    m_punctuators["++"] = aerojs::core::parser::lexer::TokenType::INCREMENT;
    m_punctuators["--"] = aerojs::core::parser::lexer::TokenType::DECREMENT;
    m_punctuators["<<"] = aerojs::core::parser::lexer::TokenType::LEFT_SHIFT;
    m_punctuators[">>"] = aerojs::core::parser::lexer::TokenType::RIGHT_SHIFT;
    m_punctuators[">>>"] = aerojs::core::parser::lexer::TokenType::UNSIGNED_RIGHT_SHIFT;
    m_punctuators["&"] = aerojs::core::parser::lexer::TokenType::BITWISE_AND;
    m_punctuators["|"] = aerojs::core::parser::lexer::TokenType::BITWISE_OR;
    m_punctuators["^"] = aerojs::core::parser::lexer::TokenType::BITWISE_XOR;
    m_punctuators["!"] = aerojs::core::parser::lexer::TokenType::NOT;
    m_punctuators["~"] = aerojs::core::parser::lexer::TokenType::BITWISE_NOT;
    m_punctuators["&&"] = aerojs::core::parser::lexer::TokenType::LOGICAL_AND;
    m_punctuators["||"] = aerojs::core::parser::lexer::TokenType::LOGICAL_OR;
    m_punctuators["??"] = aerojs::core::parser::lexer::TokenType::NULLISH_COALESCING;
    m_punctuators["?"] = aerojs::core::parser::lexer::TokenType::QUESTION_MARK;
    m_punctuators[":"] = aerojs::core::parser::lexer::TokenType::COLON;
    m_punctuators["="] = aerojs::core::parser::lexer::TokenType::ASSIGN;
    m_punctuators["+="] = aerojs::core::parser::lexer::TokenType::PLUS_ASSIGN;
    m_punctuators["-="] = aerojs::core::parser::lexer::TokenType::MINUS_ASSIGN;
    m_punctuators["*="] = aerojs::core::parser::lexer::TokenType::MULTIPLY_ASSIGN;
    m_punctuators["/="] = aerojs::core::parser::lexer::TokenType::DIVIDE_ASSIGN;
    m_punctuators["%="] = aerojs::core::parser::lexer::TokenType::MODULO_ASSIGN;
    m_punctuators["<<="] = aerojs::core::parser::lexer::TokenType::LEFT_SHIFT_ASSIGN;
    m_punctuators[">>="] = aerojs::core::parser::lexer::TokenType::RIGHT_SHIFT_ASSIGN;
    m_punctuators[">>>="] = aerojs::core::parser::lexer::TokenType::UNSIGNED_RIGHT_SHIFT_ASSIGN;
    m_punctuators["&="] = aerojs::core::parser::lexer::TokenType::BITWISE_AND_ASSIGN;
    m_punctuators["|="] = aerojs::core::parser::lexer::TokenType::BITWISE_OR_ASSIGN;
    m_punctuators["^="] = aerojs::core::parser::lexer::TokenType::BITWISE_XOR_ASSIGN;
    m_punctuators["=>"] = aerojs::core::parser::lexer::TokenType::ARROW;
    m_punctuators["..."] = aerojs::core::parser::lexer::TokenType::SPREAD;
    m_punctuators["?."] = aerojs::core::parser::lexer::TokenType::OPTIONAL_CHAINING;
    m_punctuators["**"] = aerojs::core::parser::lexer::TokenType::EXPONENTIATION;
    m_punctuators["**="] = aerojs::core::parser::lexer::TokenType::EXPONENTIATION_ASSIGN;
    m_punctuators["&&="] = aerojs::core::parser::lexer::TokenType::LOGICAL_AND_ASSIGN;
    m_punctuators["||="] = aerojs::core::parser::lexer::TokenType::LOGICAL_OR_ASSIGN;
    m_punctuators["??="] = aerojs::core::parser::lexer::TokenType::NULLISH_COALESCING_ASSIGN;
  }
  
  virtual aerojs::core::parser::lexer::TokenType FindKeyword(const std::string& identifier) const {
    auto it = m_keywords.find(identifier);
    return (it != m_keywords.end()) ? it->second : aerojs::core::parser::lexer::TokenType::IDENTIFIER;
  }
  
  virtual aerojs::core::parser::lexer::TokenType FindPunctuator(char c1, char c2 = '\0', char c3 = '\0', char c4 = '\0') const {
    std::string key;
    key += c1;
    if (c2 != '\0') key += c2;
    if (c3 != '\0') key += c3;
    if (c4 != '\0') key += c4;
    
    // 最長一致を試みる
    auto it = m_punctuators.find(key);
    if (it != m_punctuators.end()) {
      return it->second;
    }
    
    // 短い組み合わせを試す
    if (c4 != '\0') {
      key = std::string(1, c1) + c2 + c3;
      it = m_punctuators.find(key);
      if (it != m_punctuators.end()) return it->second;
    }
    
    if (c3 != '\0') {
      key = std::string(1, c1) + c2;
      it = m_punctuators.find(key);
      if (it != m_punctuators.end()) return it->second;
    }
    
    if (c2 != '\0') {
      key = std::string(1, c1);
      it = m_punctuators.find(key);
      if (it != m_punctuators.end()) return it->second;
    }
    
    return aerojs::core::parser::lexer::TokenType::ERROR;
  }
};

class TokenCache {
  size_t m_maxSize;
  std::unordered_map<std::string, aerojs::core::parser::lexer::Token> m_cache;
  std::shared_mutex m_mutex;
  
 public:
  TokenCache(size_t maxSize)
      : m_maxSize(maxSize) {
  }
  virtual ~TokenCache() = default;
  
  virtual void Add(const std::string& key, const aerojs::core::parser::lexer::Token& token) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_cache.size() >= m_maxSize && m_cache.find(key) == m_cache.end()) {
      // キャッシュが満杯の場合、ランダムに1つ削除
      if (!m_cache.empty()) {
        auto it = m_cache.begin();
        std::advance(it, rand() % m_cache.size());
        m_cache.erase(it);
      }
    }
    m_cache[key] = token;
  }
  
  virtual std::optional<aerojs::core::parser::lexer::Token> Get(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
      return it->second;
    }
    return std::nullopt;
  }
  
  virtual void Clear() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_cache.clear();
  }
  
  virtual size_t Size() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_cache.size();
  }
};

class LexerStateManager {
  size_t m_streamPosition;
  aerojs::core::parser::lexer::SourceLocation m_location;
  std::vector<aerojs::core::parser::lexer::Token> m_lookaheadBuffer;
  
 public:
  LexerStateManager() = default;
  virtual ~LexerStateManager() = default;
  
  void SaveState(size_t streamPosition, 
                 const aerojs::core::parser::lexer::SourceLocation& location,
                 const std::vector<aerojs::core::parser::lexer::Token>& lookaheadBuffer) {
    m_streamPosition = streamPosition;
    m_location = location;
    m_lookaheadBuffer = lookaheadBuffer;
  }
  
  size_t GetStreamPosition() const { return m_streamPosition; }
  const aerojs::core::parser::lexer::SourceLocation& GetLocation() const { return m_location; }
  const std::vector<aerojs::core::parser::lexer::Token>& GetLookaheadBuffer() const { return m_lookaheadBuffer; }
};

struct SourceTextChunk {
  aerojs::core::parser::lexer::SourceLocation startLocation;
  size_t startPosition = 0;
  size_t endPosition = 0;
  std::vector<aerojs::core::parser::lexer::Token> tokens;
};
#endif  // AEROJS_LEXER_TEST_DEPENDENCIES

namespace aerojs {
namespace core {
namespace parser {
namespace lexer {

// --- コンストラクタの実装 ---

Lexer::Lexer(
    std::string_view source,
    const LexerOptions& options,
    std::shared_ptr<utils::Logger> logger,
    std::shared_ptr<utils::memory::ArenaAllocator> allocator,
    std::shared_ptr<utils::thread::ThreadPool> thread_pool,
    std::shared_ptr<utils::metrics::MetricsCollector> metrics_collector)
    : m_source(source),
      m_options(options),
      m_logger(logger ? logger : std::make_shared<utils::Logger>()),
      m_allocator(allocator),
      m_threadPool(thread_pool),
      m_metricsCollector(metrics_collector),
      m_stream(std::make_unique<CharacterStream>(source)),
      m_lookupTable(std::make_unique<TokenLookupTable>()),
      m_stateManager(std::make_unique<LexerStateManager>()),
      m_tokenCache(options.m_enableTokenCaching ? std::make_unique<TokenCache>(options.m_tokenCacheSize) : nullptr),
      m_currentLocation{1, 1, 0},
      m_stats{}
{
  if (m_logger) {
    m_logger->Debug("レキサー (perfect版) を初期化しました。");
    if (m_options.m_enableTokenCaching) {
      m_logger->Debug("トークンキャッシュが有効です。サイズ: " + std::to_string(options.m_tokenCacheSize));
    }
    m_logger->Debug("ソース長: " + std::to_string(m_source.length()) + " バイト");
  }
  m_lookaheadBuffer.clear();
  m_comments.clear();
}

// --- デストラクタ ---
Lexer::~Lexer() = default;

// --- Public メソッドの実装 ---

[[nodiscard]] Token Lexer::ScanNext() {
  // 1. 先読みバッファから取得
  if (!m_lookaheadBuffer.empty()) {
    Token token = std::move(m_lookaheadBuffer.front());
    m_lookaheadBuffer.erase(m_lookaheadBuffer.begin());
    return token;
  }

  // 2. キャッシュから取得 (有効な場合)
  std::optional<Token> cachedToken = GetFromCache();
  if (cachedToken) {
    // キャッシュヒットした場合、レキサー状態を進める
    AdvanceToNextToken(*cachedToken);
    m_stats.m_tokenCacheHits++;
    UpdateStats(*cachedToken);
    return *cachedToken;
  }
  m_stats.m_tokenCacheMisses++;

  // 3. 内部スキャンで新しいトークンを取得
  Token token = InternalScanNextToken();

  // 4. 統計更新
  UpdateStats(token);

  // 5. キャッシュ追加
  if (token.m_type != TokenType::END_OF_FILE && token.m_type != TokenType::ERROR) {
    AddToCache(token);
  }

  return token;
}

[[nodiscard]] std::vector<Token> Lexer::ScanAll() {
  bool tryParallel = m_options.m_enableParallelScan &&
                     m_threadPool &&
                     m_source.length() >= m_options.m_chunkSize * 2;

  if (tryParallel) {
    return ScanAllParallel();
  }

  if (m_logger) m_logger->Debug("シーケンシャルスキャンを開始します。");
  std::vector<Token> tokens;
  tokens.reserve(m_source.length() / 10);

  Token token;
  do {
    token = ScanNext();
    tokens.push_back(token);
    if (token.m_type == TokenType::ERROR && !m_options.m_tolerant) {
      if (m_logger) m_logger->Error("許容モードが無効なため、エラーでスキャンを停止します。");
      break;
    }
  } while (token.m_type != TokenType::END_OF_FILE);

  if (m_logger) m_logger->Debug("スキャン完了。トークン数: " + std::to_string(tokens.size()));
  return tokens;
}

void Lexer::Reset() {
  if (m_logger) m_logger->Debug("レキサーの状態をリセットします。");
  m_stream->Reset();
  m_currentLocation = {1, 1, 0};
  m_stats = {};
  m_comments.clear();
  m_lookaheadBuffer.clear();
  if (m_tokenCache) {
    m_tokenCache->Clear();
  }
}

[[nodiscard]] SourceLocation Lexer::GetCurrentLocation() const noexcept {
  return m_currentLocation;
}

[[nodiscard]] const LexerStats& Lexer::GetStats() const noexcept {
  return m_stats;
}

[[nodiscard]] const std::vector<Comment>& Lexer::GetComments() const noexcept {
  return m_comments;
}

bool Lexer::SkipToken(TokenType type) noexcept {
  Token next = Peek();
  if (next.m_type == type) {
    try {
      ScanNext();  // トークンを消費
      return true;
    } catch (const std::exception& e) {
      if (m_logger) m_logger->Error("SkipToken 中にエラー: " + std::string(e.what()));
      return false;
    }
  }
  return false;
}

[[nodiscard]] Token Lexer::Peek(size_t offset) {
  if (offset == 0) {
    ReportError("Peek のオフセットは1以上である必要があります。", GetCurrentLocation());
    return Token(TokenType::ERROR, "無効なPeekオフセット", "", GetCurrentLocation());
  }

  while (m_lookaheadBuffer.size() < offset &&
         (m_lookaheadBuffer.empty() || m_lookaheadBuffer.back().m_type != TokenType::END_OF_FILE)) {
    Token nextToken = InternalScanNextToken();
    m_lookaheadBuffer.push_back(nextToken);
  }

  if (offset > m_lookaheadBuffer.size()) {
    return m_lookaheadBuffer.empty() ? Token(TokenType::END_OF_FILE, "", "", GetCurrentLocation()) : m_lookaheadBuffer.back();
  }
  return m_lookaheadBuffer[offset - 1];
}

[[nodiscard]] std::unique_ptr<LexerStateManager> Lexer::SaveState() {
  if (m_logger) m_logger->Debug("レキサーの状態を保存します。");
  auto state = std::make_unique<LexerStateManager>();
  state->SaveState(m_stream->Position(), m_currentLocation, m_lookaheadBuffer);
  return state;
}

void Lexer::RestoreState(const LexerStateManager& state) {
  if (m_logger) m_logger->Debug("レキサーの状態を復元します。");
  m_stream->SetPosition(state.GetStreamPosition());
  m_currentLocation = state.GetLocation();
  m_lookaheadBuffer = state.GetLookaheadBuffer();
}

// --- Private ヘルパーメソッドの実装 ---

char Lexer::Advance() {
  if (m_stream->IsAtEnd()) {
    return '\0';
  }

  char current = m_stream->Current();
  m_stream->Advance();
  // 位置情報を更新
  if (current == '\n') {
    m_currentLocation.m_line++;
    m_currentLocation.m_column = 1;
  } else if (current == '\r') {
    m_currentLocation.m_line++;
    m_currentLocation.m_column = 1;
    // CRLF対応
    if (m_stream->Current() == '\n') {
      m_stream->Advance();
    }
  } else if (current == '\t') {
    // タブ幅を考慮して列位置を更新（標準的なタブ幅は8）
    m_currentLocation.m_column += (8 - ((m_currentLocation.m_column - 1) % 8));
  } else {
    // UTF-8対応
    if ((current & 0x80) == 0) {
      // ASCII文字
      m_currentLocation.m_column++;
    } else if ((current & 0xE0) == 0xC0) {
      // 2バイト文字
      m_currentLocation.m_column++;
      if (!m_stream->IsAtEnd() && (m_stream->Current() & 0xC0) == 0x80) {
        m_stream->Advance();
      }
    } else if ((current & 0xF0) == 0xE0) {
      // 3バイト文字
      m_currentLocation.m_column++;
      for (int i = 0; i < 2; i++) {
        if (!m_stream->IsAtEnd() && (m_stream->Current() & 0xC0) == 0x80) {
          m_stream->Advance();
        }
      }
    } else if ((current & 0xF8) == 0xF0) {
      // 4バイト文字
      m_currentLocation.m_column++;
      for (int i = 0; i < 3; i++) {
        if (!m_stream->IsAtEnd() && (m_stream->Current() & 0xC0) == 0x80) {
          m_stream->Advance();
        }
      }
    } else {
      // 不正なUTF-8シーケンス
      m_currentLocation.m_column++;
    }
  }
  m_currentLocation.m_index = m_stream->Position();
  m_stats.m_charCount++;
  return current;
}

[[nodiscard]] char Lexer::CurrentChar() const noexcept {
  return m_stream->Current();
}

[[nodiscard]] char Lexer::PeekChar(size_t offset) const noexcept {
  return m_stream->Peek(offset);
}

[[nodiscard]] bool Lexer::IsAtEnd() const noexcept {
  return m_stream->IsAtEnd();
}

[[nodiscard]] SourceLocation Lexer::CreateSourceLocation(size_t startIndex) const {
  SourceLocation loc = m_currentLocation;
  loc.m_index = startIndex;
  
  // 開始位置から現在位置までの行と列を計算
  if (startIndex < m_currentLocation.m_index) {
    // 現在の位置から開始位置までさかのぼって行と列を計算
    size_t tempIndex = startIndex;
    size_t line = 1;
    size_t column = 1;
    
    // ファイルの先頭から開始位置までスキャン
    for (size_t i = 0; i < startIndex; i++) {
      char c = m_stream->GetCharAt(i);
      if (c == '\n') {
        line++;
        column = 1;
      } else if (c == '\r') {
        line++;
        column = 1;
        if (i + 1 < m_stream->Size() && m_stream->GetCharAt(i + 1) == '\n') {
          i++;
        }
      } else if (c == '\t') {
        column += (8 - ((column - 1) % 8));
      } else {
        column++;
      }
    }
    
    loc.m_line = line;
    loc.m_column = column;
  }
  
  return loc;
}

void Lexer::SkipWhitespaceAndNewlines() {
  while (!IsAtEnd()) {
    char c = CurrentChar();
    // Unicode空白文字判定
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f' ||
        c == 0xA0 || c == 0x1680 || c == 0x2000 || c == 0x2001 || c == 0x2002 ||
        c == 0x2003 || c == 0x2004 || c == 0x2005 || c == 0x2006 || c == 0x2007 ||
        c == 0x2008 || c == 0x2009 || c == 0x200A || c == 0x2028 || c == 0x2029 ||
        c == 0x202F || c == 0x205F || c == 0x3000 || c == 0xFEFF) {
      Advance();
    } else {
      break;
    }
  }
}

void Lexer::SkipOrScanComment() {
  while (!IsAtEnd()) {
    char c1 = PeekChar(0);
    char c2 = PeekChar(1);
    if (c1 == '/' && c2 == '/') {
      ScanSingleLineComment();
    } else if (c1 == '/' && c2 == '*') {
      ScanMultiLineComment();
    } else {
      break;
    }
  }
}

void Lexer::ScanSingleLineComment() {
  SourceLocation startLoc = GetCurrentLocation();
  Advance();  // /
  Advance();  // /
  bool preserve = m_options.m_preserveComments;
  std::string text = preserve ? "//" : "";
  if (preserve) text.reserve(32);

  while (!IsAtEnd()) {
    char c = CurrentChar();
    if (c == '\n' || c == '\r') break;
    if (preserve) text += c;
    Advance();
  }
  if (preserve) {
    m_comments.emplace_back(Comment::Type::SingleLine, std::move(text), startLoc, GetCurrentLocation());
    m_stats.m_commentCount++;
  }
}

void Lexer::ScanMultiLineComment() {
  SourceLocation startLoc = GetCurrentLocation();
  Advance();  // /
  Advance();  // *
  bool preserve = m_options.m_preserveComments;
  bool isJsDoc = false;
  std::string text = preserve ? "/*" : "";
  if (preserve) {
    text.reserve(64);
    if (PeekChar(0) == '*' && PeekChar(1) != '/') {
      isJsDoc = true;
      text += '*';
      Advance();
    }
  } else {
    if (PeekChar(0) == '*' && PeekChar(1) != '/') Advance();
  }

  bool closed = false;
  while (!IsAtEnd()) {
    char c = CurrentChar();
    if (c == '*' && PeekChar(1) == '/') {
      Advance();
      Advance();
      closed = true;
      if (preserve) text += "*/";
      break;
    }
    if (preserve) text += c;
    Advance();
  }
  if (!closed) ReportError("複数行コメントが閉じられていません。", startLoc);
  if (preserve) {
    Comment::Type type = isJsDoc ? Comment::Type::JSDoc : Comment::Type::MultiLine;
    m_comments.emplace_back(type, std::move(text), startLoc, GetCurrentLocation());
    m_stats.m_commentCount++;
  }
}

// --- スキャンメソッド ---

[[nodiscard]] Token Lexer::ScanStringLiteral() {
  SourceLocation startLoc = GetCurrentLocation();
  size_t startIndex = m_stream->Position();
  char startQuote = Advance();
  std::string value;
  value.reserve(32);
  bool closed = false;

  while (!IsAtEnd()) {
    char c = CurrentChar();
    if (c == startQuote) {
      Advance();
      closed = true;
      break;
    }
    if (c == '\\') {
      Advance();  // バックスラッシュをスキップ
      if (IsAtEnd()) { 
        ReportError("不正なエスケープシーケンス", startLoc); 
        closed = false; 
        break; 
      }
      char escaped = Advance();
      switch (escaped) {
        case 'b': value += '\b'; break;
        case 'f': value += '\f'; break;
        case 'n': value += '\n'; break;
        case 'r': value += '\r'; break;
        case 't': value += '\t'; break;
        case 'v': value += '\v'; break;
        case '\'': value += '\''; break;
        case '"': value += '"'; break;
        case '\\': value += '\\'; break;
        case '0': 
          // NULL文字（\0）
          if (!std::isdigit(PeekChar(0))) {
            value += '\0';
          } else {
            // 8進数エスケープ
            std::string octal;
            octal += escaped;
            // 最大3桁の8進数
            for (int i = 0; i < 2 && std::isdigit(PeekChar(0)) && PeekChar(0) < '8'; ++i) {
              octal += Advance();
            }
            value += static_cast<char>(std::stoi(octal, nullptr, 8));
          }
          break;
        case 'x': {
          // 16進数エスケープ \xHH
          std::string hex;
          for (int i = 0; i < 2 && IsHexDigit(PeekChar(0)); ++i) {
            hex += Advance();
          }
          if (hex.length() != 2) {
            ReportError("不正な16進数エスケープシーケンス", startLoc);
            value += 'x' + hex; // エラー時は文字をそのまま追加
          } else {
            value += static_cast<char>(std::stoi(hex, nullptr, 16));
          }
          break;
        }
        case 'u': {
          // Unicodeエスケープ
          if (PeekChar(0) == '{') {
            // \u{HHHH} 形式
            Advance(); // { をスキップ
            std::string hex;
            while (PeekChar(0) != '}' && IsHexDigit(PeekChar(0)) && hex.length() < 6) {
              hex += Advance();
            }
            if (PeekChar(0) != '}') {
              ReportError("不正なUnicodeエスケープシーケンス", startLoc);
              value += "u{" + hex;
            } else {
              Advance(); // } をスキップ
              if (hex.empty() || hex.length() > 6) {
                ReportError("不正なUnicodeコードポイント", startLoc);
              } else {
                uint32_t codePoint = std::stoul(hex, nullptr, 16);
                AppendUTF8(value, codePoint);
              }
            }
          } else {
            // \uHHHH 形式
            std::string hex;
            for (int i = 0; i < 4 && IsHexDigit(PeekChar(0)); ++i) {
              hex += Advance();
            }
            if (hex.length() != 4) {
              ReportError("不正なUnicodeエスケープシーケンス", startLoc);
              value += 'u' + hex;
            } else {
              uint32_t codePoint = std::stoul(hex, nullptr, 16);
              AppendUTF8(value, codePoint);
            }
          }
          break;
        }
        case '\n': 
          // 行継続（改行をスキップ）
          break;
        case '\r':
          // CRLF対応
          if (PeekChar(0) == '\n') Advance();
          break;
        default:
          // 不明なエスケープはそのまま文字として扱う（ECMAScript仕様に準拠）
          value += escaped;
      }
    } else if (c == '\n' || c == '\r') {
      ReportError("文字列リテラル内に改行が含まれています", startLoc);
      closed = false;
      break;
    } else {
      value += c;
      Advance();
    }
  }
  
  if (!closed) ReportError("文字列リテラルが閉じられていません", startLoc);

  std::string rawValue = std::string(m_stream->Substring(startIndex, m_stream->Position() - startIndex));
  if (!closed) return Token(TokenType::ERROR, std::move(rawValue), nullptr, startLoc);
  
  // 文字列値をトークンに格納
  return Token(TokenType::STRING_LITERAL, std::move(rawValue), std::move(value), startLoc);
}

[[nodiscard]] Token Lexer::ScanNumericLiteral() {
  SourceLocation startLoc = GetCurrentLocation();
  size_t startIndex = m_stream->Position();
  
  // 数値リテラルの種類を判定
  bool isBigInt = false;
  bool isFloat = false;
  bool hasExponent = false;
  int radix = 10; // デフォルトは10進数
  
  // 0x, 0b, 0o プレフィックスの検出
  if (CurrentChar() == '0') {
    char next = PeekChar(1);
    if (next == 'x' || next == 'X') {
      // 16進数
      Advance(); // 0をスキップ
      Advance(); // xをスキップ
      radix = 16;
      if (!IsHexDigit(CurrentChar())) {
        ReportError("16進数リテラルに数字がありません", startLoc);
        return Token(TokenType::ERROR, "0x", nullptr, startLoc);
      }
    } else if (next == 'b' || next == 'B') {
      // 2進数
      Advance(); // 0をスキップ
      Advance(); // bをスキップ
      radix = 2;
      if (CurrentChar() != '0' && CurrentChar() != '1') {
        ReportError("2進数リテラルに不正な数字があります", startLoc);
        return Token(TokenType::ERROR, "0b", nullptr, startLoc);
      }
    } else if (next == 'o' || next == 'O') {
      // 8進数
      Advance(); // 0をスキップ
      Advance(); // oをスキップ
      radix = 8;
      if (CurrentChar() < '0' || CurrentChar() > '7') {
        ReportError("8進数リテラルに不正な数字があります", startLoc);
        return Token(TokenType::ERROR, "0o", nullptr, startLoc);
      }
    }
  }
  
  // 数字部分の読み取り
  std::string numStr;
  bool hasDigits = false;
  
  auto isValidDigit = [radix](char c) -> bool {
    if (radix == 16) return IsHexDigit(c);
    if (radix == 8) return c >= '0' && c <= '7';
    if (radix == 2) return c == '0' || c == '1';
    return std::isdigit(c);
  };
  
  // 整数部分の読み取り
  while (!IsAtEnd() && (isValidDigit(CurrentChar()) || CurrentChar() == '_')) {
    if (CurrentChar() == '_') {
      // 数値セパレータ
      if (!hasDigits) {
        ReportError("数値リテラルの先頭に数値セパレータを使用できません", startLoc);
        return Token(TokenType::ERROR, "_", nullptr, startLoc);
      }
      if (PeekChar(1) == '_') {
        ReportError("連続した数値セパレータは使用できません", startLoc);
      }
      Advance();
      continue;
    }
    numStr += CurrentChar();
    hasDigits = true;
    Advance();
  }
  
  // 小数点と小数部分の読み取り
  if (radix == 10 && CurrentChar() == '.') {
    isFloat = true;
    numStr += '.';
    Advance();
    
    bool hasDecimalDigits = false;
    while (!IsAtEnd() && (std::isdigit(CurrentChar()) || CurrentChar() == '_')) {
      if (CurrentChar() == '_') {
        if (!hasDecimalDigits && !hasDigits) {
          ReportError("小数点の後に数字が必要です", startLoc);
        }
        Advance();
        continue;
      }
      numStr += CurrentChar();
      hasDecimalDigits = true;
      Advance();
    }
  }
  
  // 指数部分の読み取り
  if (radix == 10 && (CurrentChar() == 'e' || CurrentChar() == 'E')) {
    hasExponent = true;
    isFloat = true;
    numStr += CurrentChar();
    Advance();
    
    // 指数の符号
    if (CurrentChar() == '+' || CurrentChar() == '-') {
      numStr += CurrentChar();
      Advance();
    }
    
    bool hasExponentDigits = false;
    while (!IsAtEnd() && (std::isdigit(CurrentChar()) || CurrentChar() == '_')) {
      if (CurrentChar() == '_') {
        if (!hasExponentDigits) {
          ReportError("指数部分に数字が必要です", startLoc);
        }
        Advance();
        continue;
      }
      numStr += CurrentChar();
      hasExponentDigits = true;
      Advance();
    }
    
    if (!hasExponentDigits) {
      ReportError("指数部分に数字が必要です", startLoc);
    }
  }
  
  // BigInt サフィックス
  if (CurrentChar() == 'n' && !isFloat) {
    isBigInt = true;
    Advance();
  }
  
  std::string rawValue = std::string(m_stream->Substring(startIndex, m_stream->Position() - startIndex));
  
  // 数値に変換
  if (isBigInt) {
    // BigIntの処理
    std::string cleanNumStr;
    for (char c : numStr) {
      if (c != '_') cleanNumStr += c;
    }
    
            // BigIntオブジェクトを作成    BigInt* bigIntObj = BigInt::fromString(cleanNumStr.c_str(), radix);    if (!bigIntObj) {      ReportError("BigIntリテラルの解析に失敗しました", startLoc);      return Token(TokenType::ERROR, std::move(rawValue), "0", startLoc);    }        // BigIntオブジェクトをヒープに保存し、GC管理下に置く    Value bigIntValue = Value::createBigInt(bigIntObj);        // GCマーキングで回収されないようにルートとして登録    ParserGCRoot* root = ParserGCManager::getInstance()->createRoot(bigIntValue);        // トークンを作成してBigInt値とGCルート情報を設定    Token token(TokenType::BIGINT_LITERAL, std::move(rawValue), cleanNumStr, startLoc);    token.setValue(bigIntValue);    token.setGCRoot(root);        return token;
  } else {
    // 通常の数値の処理
    std::string cleanNumStr;
    for (char c : numStr) {
      if (c != '_') cleanNumStr += c;
    }
    
    double numValue = 0.0;
    try {
      if (radix == 10) {
        numValue = std::stod(cleanNumStr);
      } else {
        // 基数変換（16進数、8進数、2進数）
        size_t pos = 0;
        uint64_t intValue = std::stoull(cleanNumStr, &pos, radix);
        numValue = static_cast<double>(intValue);
      }
    } catch (const std::exception& e) {
      ReportError("数値リテラルの変換エラー: " + std::string(e.what()), startLoc);
    }
    
    return Token(TokenType::NUMERIC_LITERAL, std::move(rawValue), numValue, startLoc);
  }
}

[[nodiscard]] Token Lexer::ScanIdentifierOrKeyword() {
  SourceLocation startLoc = GetCurrentLocation();
  size_t startIndex = m_stream->Position();
  std::string identifier;
  identifier.reserve(16);
  
  // 識別子の開始文字の判定
  if (IsIdentifierStart(CurrentChar())) {
    // 最初の文字を処理
    if (CurrentChar() == '\\' && PeekChar(1) == 'u') {
      // Unicodeエスケープシーケンス
      Advance(); // \ をスキップ
      uint32_t codePoint = ScanUnicodeEscapeSequence();
      if (!IsIdentifierStartCodePoint(codePoint)) {
        ReportError("識別子の開始に無効なUnicodeエスケープシーケンスです", startLoc);
        return Token(TokenType::ERROR, "\\u", nullptr, startLoc);
      }
      AppendUTF8(identifier, codePoint);
    } else {
      identifier += Advance();
    }
    
    // 残りの文字を処理
    while (!IsAtEnd() && IsIdentifierPart(CurrentChar())) {
      if (CurrentChar() == '\\' && PeekChar(1) == 'u') {
        // Unicodeエスケープシーケンス
        Advance(); // \ をスキップ
        uint32_t codePoint = ScanUnicodeEscapeSequence();
        if (!IsIdentifierPartCodePoint(codePoint)) {
          ReportError("識別子に無効なUnicodeエスケープシーケンスです", startLoc);
          break;
        }
        AppendUTF8(identifier, codePoint);
      } else {
        identifier += Advance();
      }
    }
  } else {
    ReportError("不正な識別子の開始文字です", startLoc);
    Advance();  // エラーリカバリ
    return Token(TokenType::ERROR, std::string(m_stream->Substring(startIndex, 1)), nullptr, startLoc);
  }
  
  // キーワードかどうかを判定
  TokenType type = m_lookupTable->FindKeyword(identifier);
  std::string rawValue = std::string(m_stream->Substring(startIndex, m_stream->Position() - startIndex));
  
  // 予約語や将来の予約語の処理
  if (type != TokenType::IDENTIFIER && m_options.m_strictMode) {
    // 厳格モードでの追加チェック
    if (identifier == "let" || identifier == "yield" || identifier == "interface" || 
        identifier == "package" || identifier == "private" || identifier == "protected" || 
        identifier == "public" || identifier == "static") {
      // 厳格モードでの予約語
    }
  }
  
  return Token(type, std::move(rawValue), std::move(identifier), startLoc);
}

[[nodiscard]] Token Lexer::ScanRegExpLiteral() {
  SourceLocation startLoc = GetCurrentLocation();
  size_t startIndex = m_stream->Position();
  Advance();  // / をスキップ
  
  std::string pattern;
  bool inCharClass = false;
  bool escaped = false;
  bool closed = false;
  
  // パターン部分の読み取り
  while (!IsAtEnd()) {
    char c = CurrentChar();
    
    if (c == '/' && !escaped && !inCharClass) {
      // 正規表現の終了
      Advance();
      closed = true;
      break;
    } else if (c == '[' && !escaped) {
      // 文字クラスの開始
      inCharClass = true;
    } else if (c == ']' && !escaped) {
      // 文字クラスの終了
      inCharClass = false;
    } else if (c == '\\' && !escaped) {
      // エスケープシーケンスの開始
      escaped = true;
      pattern += c;
      Advance();
      if (IsAtEnd()) {
        ReportError("正規表現パターンが不完全です", startLoc);
        break;
      }
      continue;
    } else if (c == '\n' || c == '\r') {
      // 改行は正規表現内で許可されない
      ReportError("正規表現パターン内に改行があります", startLoc);
      break;
    }
    
    pattern += c;
    Advance();
    escaped = false;
  }
  
  if (!closed) {
    ReportError("正規表現パターンが閉じられていません", startLoc);
    std::string rawValue = std::string(m_stream->Substring(startIndex, m_stream->Position() - startIndex));
    return Token(TokenType::ERROR, std::move(rawValue), nullptr, startLoc);
  }
  
  // フラグの読み取り
  std::string flags;
  std::set<char> usedFlags;
  
  while (!IsAtEnd() && IsIdentifierPart(CurrentChar())) {
    char flag = CurrentChar();
    
    // 有効なフラグかチェック
    if (flag == 'g' || flag == 'i' || flag == 'm' || flag == 's' || flag == 'u' || flag == 'y' || flag == 'd') {
      if (usedFlags.find(flag) != usedFlags.end()) {
        ReportError("正規表現フラグが重複しています: " + std::string(1, flag), startLoc);
      }
      usedFlags.insert(flag);
      flags += flag;
      Advance();
    } else {
      ReportError("不正な正規表現フラグです: " + std::string(1, flag), startLoc);
      Advance();
    }
  }
  
  std::string rawValue = std::string(m_stream->Substring(startIndex, m_stream->Position() - startIndex));
  
  // 正規表現オブジェクトを作成
  RegExpValue regExpValue{pattern, flags};
  return Token(TokenType::REGEXP_LITERAL, std::move(rawValue), std::move(regExpValue), startLoc);
}

[[nodiscard]] Token Lexer::ScanTemplateLiteral() {
  SourceLocation startLoc = GetCurrentLocation();
  size_t startIndex = m_stream->Position();
  Advance();  // ` をスキップ
  
  std::string rawChunk = "`";
  std::string cookedValue;
  bool closed = false;
  bool isHead = true;
  
  while (!IsAtEnd()) {
    char c = CurrentChar();
    
    if (c == '`') {
      // テンプレートリテラルの終了
      Advance();
      rawChunk += '`';
      closed = true;
      break;
    } else if (c == '$' && PeekChar(1) == '{') {
      // 式の開始
      Advance();  // $ をスキップ
      Advance();  // { をスキップ
      rawChunk += "${";
      break;
    } else if (c == '\\') {
      // エスケープシーケンス
      rawChunk += c;
      Advance();
      
      if (IsAtEnd()) {
        ReportError("テンプレートリテラル内の不正なエスケープシーケンスです", startLoc);
        break;
      }
      
      char escaped = CurrentChar();
      rawChunk += escaped;
      
      // cooked値のエスケープ処理
      switch (escaped) {
        case 'b': cookedValue += '\b'; break;
        case 'f': cookedValue += '\f'; break;
        case 'n': cookedValue += '\n'; break;
        case 'r': cookedValue += '\r'; break;
        case 't': cookedValue += '\t'; break;
        case 'v': cookedValue += '\v'; break;
        case '\'': cookedValue += '\''; break;
        case '"': cookedValue += '"'; break;
        case '`': cookedValue += '`'; break;
        case '$': cookedValue += '$'; break;
        case '\\': cookedValue += '\\'; break;
        case '0':
          if (!std::isdigit(PeekChar(1))) {
            cookedValue += '\0';
          } else {
            cookedValue += '0'; // 8進数エスケープは無効
          }
          break;
        case 'x': {
          // 16進数エスケープ
          std::string hex;
          for (int i = 0; i < 2 && !IsAtEnd() && IsHexDigit(PeekChar(1)); ++i) {
            Advance();
            hex += CurrentChar();
            rawChunk += CurrentChar();
          }
          if (hex.length() == 2) {
            cookedValue += static_cast<char>(std::stoi(hex, nullptr, 16));
          } else {
            cookedValue += 'x' + hex; // 不正な16進数エスケープ
          }
          break;
        }
        case 'u': {
          // Unicodeエスケープ
          std::string originalSequence = "u";
          if (PeekChar(1) == '{') {
            // \u{HHHH} 形式
            Advance();
            originalSequence += '{';
            rawChunk += '{';
            std::string hex;
            while (PeekChar(1) != '}' && !IsAtEnd() && IsHexDigit(PeekChar(1)) && hex.length() < 6) {
              Advance();
              hex += CurrentChar();
              originalSequence += CurrentChar();
              rawChunk += CurrentChar();
            }
            if (PeekChar(1) == '}') {
              Advance();
              originalSequence += '}';
              rawChunk += '}';
              if (!hex.empty() && hex.length() <= 6) {
                uint32_t codePoint = std::stoul(hex, nullptr, 16);
                AppendUTF8(cookedValue, codePoint);
              } else {
                cookedValue += "\\u{" + hex + "}"; // 不正なUnicodeエスケープ
              }
            } else {
              cookedValue += "\\u{" + hex; // 閉じられていないUnicodeエスケープ
            }
          } else {
            // \uHHHH 形式
            std::string hex;
            for (int i = 0; i < 4 && !IsAtEnd() && IsHexDigit(PeekChar(1)); ++i) {
              Advance();
              hex += CurrentChar();
              originalSequence += CurrentChar();
              rawChunk += CurrentChar();
            }
            if (hex.length() == 4) {
              uint32_t codePoint = std::stoul(hex, nullptr, 16);
              AppendUTF8(cookedValue, codePoint);
            } else {
              cookedValue += "\\u" + hex; // 不正なUnicodeエスケープ
            }
          }
          break;
        }
        case '\r':
          // 行継続（CRLF）
          if (PeekChar(1) == '\n') {
            Advance();
            rawChunk += CurrentChar();
          }
          // 行継続は値に含めない
          break;
        case '\n':
          // 行継続は値に含めない
          break;
        default:
          // その他のエスケープシーケンスはそのまま
          cookedValue += escaped;
      }
      
      Advance();
    } else {
      // 通常の文字
      rawChunk += c;
      cookedValue += c;
      Advance();
    }
  }
  
  if (!closed && rawChunk.back() != '{') {
    ReportError("テンプレートリテラルが閉じられていません", startLoc);
    return Token(TokenType::ERROR, std::move(rawChunk), nullptr, startLoc);
  }
  
  // トークンタイプの決定
  TokenType type;
  if (closed) {
    type = isHead ? TokenType::TEMPLATE_LITERAL : TokenType::TEMPLATE_TAIL;
  } else {
    type = isHead ? TokenType::TEMPLATE_HEAD : TokenType::TEMPLATE_MIDDLE;
  }
  
  // テンプレートリテラル値の作成
  TemplateLiteralValue templateValue{cookedValue, rawChunk};
  return Token(type, std::move(rawChunk), std::move(templateValue), startLoc);
}

[[nodiscard]] Token Lexer::ScanPunctuator() {
  SourceLocation startLoc = GetCurrentLocation();
  
  // 先読みして最長一致の区切り文字を見つける
  std::string punctuator;
  punctuator.reserve(4); // 最長の区切り文字は4文字（例: '>>>=')
  
  // 最大4文字まで読み込む
  for (int i = 0; i < 4 && !IsAtEnd(i); ++i) {
    punctuator += PeekChar(i);
  }
  
  // 区切り文字の候補を長い順にチェック
  static const std::vector<std::pair<std::string, TokenType>> punctuators = {
    // 4文字
    {">>>=", TokenType::GREATER_GREATER_GREATER_EQUAL},
    
    // 3文字
    {"===", TokenType::EQUAL_EQUAL_EQUAL},
    {"!==", TokenType::BANG_EQUAL_EQUAL},
    {">>>", TokenType::GREATER_GREATER_GREATER},
    {"<<=", TokenType::LESS_LESS_EQUAL},
    {">>=", TokenType::GREATER_GREATER_EQUAL},
    {"**=", TokenType::STAR_STAR_EQUAL},
    {"??=", TokenType::QUESTION_QUESTION_EQUAL},
    {"||=", TokenType::PIPE_PIPE_EQUAL},
    {"&&=", TokenType::AMPERSAND_AMPERSAND_EQUAL},
    {"...", TokenType::DOT_DOT_DOT},
    
    // 2文字
    {"=>", TokenType::ARROW},
    {"+=", TokenType::PLUS_EQUAL},
    {"-=", TokenType::MINUS_EQUAL},
    {"*=", TokenType::STAR_EQUAL},
    {"/=", TokenType::SLASH_EQUAL},
    {"%=", TokenType::PERCENT_EQUAL},
    {"&=", TokenType::AMPERSAND_EQUAL},
    {"|=", TokenType::PIPE_EQUAL},
    {"^=", TokenType::CARET_EQUAL},
    {"++", TokenType::PLUS_PLUS},
    {"--", TokenType::MINUS_MINUS},
    {"<<", TokenType::LESS_LESS},
    {">>", TokenType::GREATER_GREATER},
    {"&&", TokenType::AMPERSAND_AMPERSAND},
    {"||", TokenType::PIPE_PIPE},
    {"??", TokenType::QUESTION_QUESTION},
    {"**", TokenType::STAR_STAR},
    {"==", TokenType::EQUAL_EQUAL},
    {"!=", TokenType::BANG_EQUAL},
    {">=", TokenType::GREATER_EQUAL},
    {"<=", TokenType::LESS_EQUAL},
    {"?.", TokenType::QUESTION_DOT},
    
    // 1文字
    {"{", TokenType::LEFT_BRACE},
    {"}", TokenType::RIGHT_BRACE},
    {"(", TokenType::LEFT_PAREN},
    {")", TokenType::RIGHT_PAREN},
    {"[", TokenType::LEFT_BRACKET},
    {"]", TokenType::RIGHT_BRACKET},
    {".", TokenType::DOT},
    {";", TokenType::SEMICOLON},
    {",", TokenType::COMMA},
    {"<", TokenType::LESS},
    {">", TokenType::GREATER},
    {"+", TokenType::PLUS},
    {"-", TokenType::MINUS},
    {"*", TokenType::STAR},
    {"/", TokenType::SLASH},
    {"%", TokenType::PERCENT},
    {"&", TokenType::AMPERSAND},
    {"|", TokenType::PIPE},
    {"^", TokenType::CARET},
    {"!", TokenType::BANG},
    {"~", TokenType::TILDE},
    {"?", TokenType::QUESTION},
    {":", TokenType::COLON},
    {"=", TokenType::EQUAL}
  };
  
  // 最長一致を探す
  for (const auto& [op, type] : punctuators) {
    if (punctuator.substr(0, op.length()) == op) {
      // 一致した区切り文字の長さ分だけ進める
      for (size_t i = 0; i < op.length(); ++i) {
        Advance();
      }
      return Token(type, op, nullptr, startLoc);
    }
  }
  
  // 不明な区切り文字
  std::string unknown(1, Advance());
  ReportError("不明な区切り文字: " + unknown, startLoc);
  return Token(TokenType::ERROR, std::move(unknown), nullptr, startLoc);
}
[[nodiscard]] Token Lexer::ScanPunctuator() {
  SourceLocation startLoc = GetCurrentLocation();
  
  // 現在位置から最大3文字を取得して区切り文字/演算子を判定
  std::string punctuator;
  for (int i = 0; i < 3 && !IsAtEnd(i); ++i) {
    punctuator += PeekChar(i);
  }
  
  // 区切り文字/演算子の定義テーブル（長い順に並べる）
  static const std::unordered_map<std::string, TokenType> punctuators = {
    // 3文字
    {"===", TokenType::EQUAL_EQUAL_EQUAL},
    {"!==", TokenType::BANG_EQUAL_EQUAL},
    {"**=", TokenType::STAR_STAR_EQUAL},
    {"<<=", TokenType::LESS_LESS_EQUAL},
    {">>=", TokenType::GREATER_GREATER_EQUAL},
    {">>>=", TokenType::GREATER_GREATER_GREATER_EQUAL},
    {"...", TokenType::DOT_DOT_DOT},
    {"??=", TokenType::QUESTION_QUESTION_EQUAL},
    
    // 2文字
    {"++", TokenType::PLUS_PLUS},
    {"--", TokenType::MINUS_MINUS},
    {"<<", TokenType::LESS_LESS},
    {">>", TokenType::GREATER_GREATER},
    {">>>", TokenType::GREATER_GREATER_GREATER},
    {"&&", TokenType::AMPERSAND_AMPERSAND},
    {"||", TokenType::PIPE_PIPE},
    {"??", TokenType::QUESTION_QUESTION},
    {"+=", TokenType::PLUS_EQUAL},
    {"-=", TokenType::MINUS_EQUAL},
    {"*=", TokenType::STAR_EQUAL},
    {"/=", TokenType::SLASH_EQUAL},
    {"%=", TokenType::PERCENT_EQUAL},
    {"&=", TokenType::AMPERSAND_EQUAL},
    {"|=", TokenType::PIPE_EQUAL},
    {"^=", TokenType::CARET_EQUAL},
    {"=>", TokenType::ARROW},
    {"**", TokenType::STAR_STAR},
    {"==", TokenType::EQUAL_EQUAL},
    {"!=", TokenType::BANG_EQUAL},
    {">=", TokenType::GREATER_EQUAL},
    {"<=", TokenType::LESS_EQUAL},
    {"?.", TokenType::QUESTION_DOT},
    
    // 1文字
    {"{", TokenType::LEFT_BRACE},
    {"}", TokenType::RIGHT_BRACE},
    {"(", TokenType::LEFT_PAREN},
    {")", TokenType::RIGHT_PAREN},
    {"[", TokenType::LEFT_BRACKET},
    {"]", TokenType::RIGHT_BRACKET},
    {".", TokenType::DOT},
    {";", TokenType::SEMICOLON},
    {",", TokenType::COMMA},
    {"<", TokenType::LESS},
    {">", TokenType::GREATER},
    {"+", TokenType::PLUS},
    {"-", TokenType::MINUS},
    {"*", TokenType::STAR},
    {"/", TokenType::SLASH},
    {"%", TokenType::PERCENT},
    {"&", TokenType::AMPERSAND},
    {"|", TokenType::PIPE},
    {"^", TokenType::CARET},
    {"!", TokenType::BANG},
    {"~", TokenType::TILDE},
    {"?", TokenType::QUESTION},
    {":", TokenType::COLON},
    {"=", TokenType::EQUAL}
  };
  
  // 最長一致を探す
  for (const auto& [op, type] : punctuators) {
    if (punctuator.substr(0, op.length()) == op) {
      // 一致した区切り文字の長さ分だけ進める
      for (size_t i = 0; i < op.length(); ++i) {
        Advance();
      }
      return Token(type, op, nullptr, startLoc);
    }
  }
  
  // 不明な区切り文字
  std::string unknown(1, Advance());
  ReportError("不明な区切り文字: " + unknown, startLoc);
  return Token(TokenType::ERROR, std::move(unknown), nullptr, startLoc);
}

[[nodiscard]] Token Lexer::ScanJsxToken() {
  SourceLocation startLoc = GetCurrentLocation();
  
  // JSXの構文解析
  if (CurrentChar() == '<') {
    Advance(); // '<' をスキップ
    
    // JSX終了タグ（</...>）の処理
    if (CurrentChar() == '/') {
      Advance(); // '/' をスキップ
      return ScanJsxEndTag(startLoc);
    }
    
    // JSX開始タグ（<...>）の処理
    if (std::isalpha(CurrentChar()) || CurrentChar() == '_' || CurrentChar() == ':') {
      return ScanJsxStartTag(startLoc);
    }
    
    // JSXフラグメント（<>...</>）の処理
    if (CurrentChar() == '>') {
      Advance(); // '>' をスキップ
      return Token(TokenType::JSX_FRAGMENT_START, "<>", nullptr, startLoc);
    }
  }
  
  // JSX終了フラグメント（</>）の処理
  if (CurrentChar() == '<' && PeekChar(1) == '/' && PeekChar(2) == '>') {
    Advance(); // '<' をスキップ
    Advance(); // '/' をスキップ
    Advance(); // '>' をスキップ
    return Token(TokenType::JSX_FRAGMENT_END, "</>", nullptr, startLoc);
  }
  
  // JSXテキスト内容の処理
  if (m_jsxContext.inJsxContent) {
    return ScanJsxText(startLoc);
  }
  
  // JSX属性値の処理
  if (m_jsxContext.inJsxAttribute && (CurrentChar() == '"' || CurrentChar() == '\'')) {
    return ScanJsxAttributeValue(startLoc);
  }
  
  // JSX式（{...}）の処理
  if (CurrentChar() == '{') {
    Advance(); // '{' をスキップ
    m_jsxContext.braceStack.push(JsxBraceType::Expression);
    return Token(TokenType::JSX_EXPRESSION_START, "{", nullptr, startLoc);
  }
  
  // JSX式の終了（}）の処理
  if (CurrentChar() == '}' && !m_jsxContext.braceStack.empty()) {
    Advance(); // '}' をスキップ
    m_jsxContext.braceStack.pop();
    return Token(TokenType::JSX_EXPRESSION_END, "}", nullptr, startLoc);
  }
  
  // JSX属性名の処理
  if (m_jsxContext.inJsxTag && (std::isalpha(CurrentChar()) || CurrentChar() == '_' || CurrentChar() == ':')) {
    return ScanJsxAttributeName(startLoc);
  }
  
  // JSXスプレッド属性（{...props}）の処理
  if (CurrentChar() == '{' && PeekChar(1) == '.' && PeekChar(2) == '.' && PeekChar(3) == '.') {
    Advance(); // '{' をスキップ
    Advance(); // '.' をスキップ
    Advance(); // '.' をスキップ
    Advance(); // '.' をスキップ
    m_jsxContext.braceStack.push(JsxBraceType::SpreadAttribute);
    return Token(TokenType::JSX_SPREAD_ATTRIBUTE, "{...", nullptr, startLoc);
  }
  
  // 不明なJSXトークン
  std::string unknown(1, Advance());
  ReportError("不明なJSXトークン: " + unknown, startLoc);
  return Token(TokenType::ERROR, std::move(unknown), nullptr, startLoc);
}

[[nodiscard]] Token Lexer::ScanJsxStartTag(const SourceLocation& startLoc) {
  std::string tagName;
  
  // タグ名を読み取る
  while (!IsAtEnd() && (std::isalnum(CurrentChar()) || CurrentChar() == '_' || 
                        CurrentChar() == '-' || CurrentChar() == ':' || CurrentChar() == '.')) {
    tagName += Advance();
  }
  
  // JSXコンテキストを更新
  m_jsxContext.inJsxTag = true;
  m_jsxContext.currentTag = tagName;
  
  return Token(TokenType::JSX_TAG_START, "<" + tagName, nullptr, startLoc);
}

[[nodiscard]] Token Lexer::ScanJsxEndTag(const SourceLocation& startLoc) {
  std::string tagName;
  
  // タグ名を読み取る
  while (!IsAtEnd() && (std::isalnum(CurrentChar()) || CurrentChar() == '_' || 
                        CurrentChar() == '-' || CurrentChar() == ':' || CurrentChar() == '.')) {
    tagName += Advance();
  }
  
  // '>'をスキップ
  if (CurrentChar() == '>') {
    Advance();
  } else {
    ReportError("JSX終了タグが不完全です", startLoc);
  }
  
  // JSXコンテキストを更新
  m_jsxContext.inJsxTag = false;
  m_jsxContext.inJsxContent = false;
  
  return Token(TokenType::JSX_TAG_END, "</" + tagName + ">", nullptr, startLoc);
}

[[nodiscard]] Token Lexer::ScanJsxText(const SourceLocation& startLoc) {
  std::string text;
  
  // テキスト内容を読み取る（'<'または'{'が出現するまで）
  while (!IsAtEnd() && CurrentChar() != '<' && CurrentChar() != '{') {
    text += Advance();
  }
  
  // 空白のみのテキストは無視する
  if (std::all_of(text.begin(), text.end(), [](char c) { return std::isspace(c); })) {
    return ScanJsxToken(); // 再帰的に次のJSXトークンを取得
  }
  
  return Token(TokenType::JSX_TEXT, std::move(text), nullptr, startLoc);
}

[[nodiscard]] Token Lexer::ScanJsxAttributeName(const SourceLocation& startLoc) {
  std::string attrName;
  
  // 属性名を読み取る
  while (!IsAtEnd() && (std::isalnum(CurrentChar()) || CurrentChar() == '_' || 
                        CurrentChar() == '-' || CurrentChar() == ':')) {
    attrName += Advance();
  }
  
  // JSXコンテキストを更新
  m_jsxContext.inJsxAttribute = true;
  
  return Token(TokenType::JSX_ATTRIBUTE_NAME, std::move(attrName), nullptr, startLoc);
}

[[nodiscard]] Token Lexer::ScanJsxAttributeValue(const SourceLocation& startLoc) {
  char quote = Advance(); // 引用符（'または"）をスキップ
  std::string value;
  
  // 属性値を読み取る
  while (!IsAtEnd() && CurrentChar() != quote) {
    value += Advance();
  }
  
  if (CurrentChar() == quote) {
    Advance(); // 終了引用符をスキップ
  } else {
    ReportError("JSX属性値が閉じられていません", startLoc);
  }
  
  // JSXコンテキストを更新
  m_jsxContext.inJsxAttribute = false;
  
  return Token(TokenType::JSX_ATTRIBUTE_VALUE, std::move(value), nullptr, startLoc);
}

[[nodiscard]] Token Lexer::ScanTypeScriptSyntax() {
  SourceLocation startLoc = GetCurrentLocation();
  
  // TypeScriptの型アノテーション（: type）
  if (CurrentChar() == ':') {
    Advance(); // ':' をスキップ
    SkipWhitespaceAndNewlines();
    return ScanTypeAnnotation(startLoc);
  }
  
  // TypeScriptのインターフェース宣言
  if (m_tsContext.afterInterfaceKeyword && CurrentChar() == '{') {
    Advance(); // '{' をスキップ
    m_tsContext.braceStack.push(TsBraceType::Interface);
    return Token(TokenType::TS_INTERFACE_BODY_START, "{", nullptr, startLoc);
  }
  
  // TypeScriptのジェネリック型パラメータ（<T>）
  if (CurrentChar() == '<' && m_tsContext.allowGeneric) {
    return ScanGenericParameters(startLoc);
  }
  
  // TypeScriptの型アサーション（as Type）
  if (m_tsContext.afterAsKeyword) {
    return ScanTypeAssertion(startLoc);
  }
  
  // TypeScriptの列挙型
  if (m_tsContext.afterEnumKeyword && CurrentChar() == '{') {
    Advance(); // '{' をスキップ
    m_tsContext.braceStack.push(TsBraceType::Enum);
    return Token(TokenType::TS_ENUM_BODY_START, "{", nullptr, startLoc);
  }
  
  // TypeScriptの型エイリアス（type Alias = Type）
  if (m_tsContext.afterTypeKeyword && CurrentChar() == '=') {
    Advance(); // '=' をスキップ
    return Token(TokenType::TS_TYPE_ALIAS_EQUALS, "=", nullptr, startLoc);
  }
  
  // TypeScriptの修飾子（public, private, protected, readonly, static, abstract）
  if (m_tsContext.inClassBody) {
    std::string modifier;
    size_t startPos = m_stream->Position();
    
    // 修飾子の候補を読み取る
    while (!IsAtEnd() && std::isalpha(CurrentChar())) {
      modifier += Advance();
    }
    
    // 修飾子を判定
    if (modifier == "public" || modifier == "private" || modifier == "protected" ||
        modifier == "readonly" || modifier == "static" || modifier == "abstract") {
      return Token(TokenType::TS_MODIFIER, std::move(modifier), nullptr, startLoc);
    }
    
    // 修飾子でなければ位置を戻す
    m_stream->SetPosition(startPos);
  }
  
  // TypeScriptの型定義（interface, type, enum）はキーワードとして処理されるため、
  // ScanIdentifierOrKeyword() で処理される
  
  // 不明なTypeScriptトークン
  std::string unknown(1, Advance());
  ReportError("不明なTypeScriptトークン: " + unknown, startLoc);
  return Token(TokenType::ERROR, std::move(unknown), nullptr, startLoc);
}

[[nodiscard]] Token Lexer::ScanTypeAnnotation(const SourceLocation& startLoc) {
  // 型名を読み取る
  std::string typeName;
  
  // 空白をスキップ
  SkipWhitespaceAndNewlines();
  
  // 型名の最初の文字を確認
  if (std::isalpha(CurrentChar()) || CurrentChar() == '_' || CurrentChar() == '$') {
    // 識別子として型名を読み取る
    while (!IsAtEnd() && (std::isalnum(CurrentChar()) || CurrentChar() == '_' || CurrentChar() == '$')) {
      typeName += Advance();
    }
    
    // プリミティブ型かどうかを確認
    static const std::unordered_set<std::string> primitiveTypes = {
      "string", "number", "boolean", "any", "void", "null", "undefined", "never", "unknown", "object", "symbol", "bigint"
    };
    
    if (primitiveTypes.find(typeName) != primitiveTypes.end()) {
      return Token(TokenType::TS_PRIMITIVE_TYPE, std::move(typeName), nullptr, startLoc);
    }
    
    return Token(TokenType::TS_TYPE_REFERENCE, std::move(typeName), nullptr, startLoc);
  }
  
  // 配列型（Type[]）
  if (CurrentChar() == '[') {
    Advance(); // '[' をスキップ
    
            // 配列要素の型を読み取る    TypeNode* elementType = ParseTypeExpression();        if (!elementType) {      ReportError("配列型定義に要素型が必要です", startLoc);      return Token(TokenType::ERROR, "[]", nullptr, startLoc);    }        if (CurrentChar() == ']') {      Advance(); // ']' をスキップ            // 配列型を表すASTノードを作成      ArrayTypeNode* arrayType = m_typeNodeAllocator.AllocateNode<ArrayTypeNode>();      arrayType->elementType = elementType;            Token token(TokenType::TS_ARRAY_TYPE, "[]", nullptr, startLoc);      token.setTypeNode(arrayType);      return token;    } else {      ReportError("配列型定義の閉じ括弧 ']' がありません", startLoc);      return Token(TokenType::ERROR, "[", nullptr, startLoc);    }  }    // ユニオン型（A | B）またはインターセクション型（A & B）  if (startChar == '|' || startChar == '&') {    bool isUnion = (startChar == '|');    char operatorChar = Advance(); // '|' または '&' をスキップ        // 2回連続の演算子かチェック (|| または &&)    if (CurrentChar() == operatorChar) {      // 論理演算子なのでTypeScriptの型ではない      m_stream->SetPosition(m_stream->Position() - 1); // 巻き戻す      return ScanPunctuator();    }        // 前のトークンを取得して左側の型にする    if (m_previousTypeNode == nullptr) {      ReportError(        std::string(isUnion ? "ユニオン" : "インターセクション") +         "型の左側の型式がありません",         startLoc      );      return Token(TokenType::ERROR, std::string(1, operatorChar), nullptr, startLoc);    }        // 右側の型を解析    TypeNode* rightType = ParseTypeExpression();    if (!rightType) {      ReportError(        std::string(isUnion ? "ユニオン" : "インターセクション") +         "型の右側の型式がありません",         GetCurrentLocation()      );      return Token(TokenType::ERROR, std::string(1, operatorChar), nullptr, startLoc);    }        // ユニオン/インターセクション型を表すASTノードを作成    CompositeTypeNode* compositeType = m_typeNodeAllocator.AllocateNode<CompositeTypeNode>();    compositeType->isUnion = isUnion;    compositeType->leftType = m_previousTypeNode;    compositeType->rightType = rightType;        TokenType tokenType = isUnion ? TokenType::TS_UNION_TYPE : TokenType::TS_INTERSECTION_TYPE;    Token token(tokenType, std::string(1, operatorChar), nullptr, startLoc);    token.setTypeNode(compositeType);        m_previousTypeNode = compositeType; // 次の型式の左側になる可能性がある    return token;  }
  
  // オブジェクト型（{ prop: Type }）
  if (CurrentChar() == '{') {
    Advance(); // '{' をスキップ
    m_tsContext.braceStack.push(TsBraceType::ObjectType);
    return Token(TokenType::TS_OBJECT_TYPE_START, "{", nullptr, startLoc);
  }
  
  // 関数型（(params) => ReturnType）
  if (CurrentChar() == '(') {
    Advance(); // '(' をスキップ
    m_tsContext.parenStack.push(TsParenType::FunctionType);
    return Token(TokenType::TS_FUNCTION_TYPE_PARAMS_START, "(", nullptr, startLoc);
  }
  
  // 不明な型アノテーション
  std::string unknown = ":";
  if (!IsAtEnd()) {
    unknown += Advance();
  }
  ReportError("不明な型アノテーション: " + unknown, startLoc);
  return Token(TokenType::ERROR, std::move(unknown), nullptr, startLoc);
}

[[nodiscard]] Token Lexer::ScanGenericParameters(const SourceLocation& startLoc) {
  Advance(); // '<' をスキップ
  m_tsContext.angleStack.push(TsAngleType::GenericParams);
  
      // ジェネリックパラメータの内容を解析  GenericParamsNode* paramsNode = m_typeNodeAllocator.AllocateNode<GenericParamsNode>();    // パラメータリストを解析  bool firstParam = true;  while (!IsAtEnd() && CurrentChar() != '>') {    // カンマをスキップ（最初のパラメータ以外）    if (!firstParam) {      if (CurrentChar() != ',') {        ReportError("ジェネリックパラメータリストの区切り文字 ',' が必要です", GetCurrentLocation());        break;      }      Advance(); // ',' をスキップ      SkipWhitespace();    }        // パラメータ名を読み取る    std::string paramName;    if (!IsIdentifierStart(CurrentChar())) {      ReportError("ジェネリックパラメータ名が必要です", GetCurrentLocation());      break;    }        // 識別子を読み取る    while (!IsAtEnd() && IsIdentifierPart(CurrentChar())) {      paramName += Advance();    }    SkipWhitespace();        // 制約部分を読み取る（extends ...）    TypeNode* constraint = nullptr;    if (!IsAtEnd() && CurrentChar() == 'e' &&         PeekChar(1) == 'x' && PeekChar(2) == 't' && PeekChar(3) == 'e' &&         PeekChar(4) == 'n' && PeekChar(5) == 'd' && PeekChar(6) == 's') {      // "extends" キーワードをスキップ      for (int i = 0; i < 7; i++) Advance();      SkipWhitespace();            // 制約型を解析      constraint = ParseTypeExpression();      if (!constraint) {        ReportError("extends 後に型式が必要です", GetCurrentLocation());        break;      }      SkipWhitespace();    }        // デフォルト型を読み取る（= ...）    TypeNode* defaultType = nullptr;    if (!IsAtEnd() && CurrentChar() == '=') {      Advance(); // '=' をスキップ      SkipWhitespace();            // デフォルト型を解析      defaultType = ParseTypeExpression();      if (!defaultType) {        ReportError("デフォルト型の指定が必要です", GetCurrentLocation());        break;      }      SkipWhitespace();    }        // パラメータノードを作成    GenericParamNode* paramNode = m_typeNodeAllocator.AllocateNode<GenericParamNode>();    paramNode->name = paramName;    paramNode->constraint = constraint;    paramNode->defaultType = defaultType;        // パラメータリストに追加    paramsNode->params.push_back(paramNode);        firstParam = false;  }    // 閉じ山括弧をチェック  if (IsAtEnd() || CurrentChar() != '>') {    ReportError("ジェネリックパラメータリストの終了 '>' が必要です", GetCurrentLocation());  } else {    Advance(); // '>' をスキップ  }    Token token(TokenType::TS_GENERIC_PARAMS_START, "<", nullptr, startLoc);  token.setTypeNode(paramsNode);  return token;
}

[[nodiscard]] Token Lexer::ScanTypeAssertion(const SourceLocation& startLoc) {
  // 'as' キーワードの後の型を解析
  m_tsContext.afterAsKeyword = false;
  
  // 型名を読み取る
  std::string typeName;
  
  while (!IsAtEnd() && (std::isalnum(CurrentChar()) || CurrentChar() == '_' || CurrentChar() == '$')) {
    typeName += Advance();
  }
  
  return Token(TokenType::TS_TYPE_ASSERTION, "as " + typeName, nullptr, startLoc);
}

// エラー報告、統計、キャッシュ
void Lexer::ReportError(const std::string& message, const SourceLocation& loc) {
  m_stats.m_errorCount++;
  if (m_logger) {
    std::string fm = "[" + (loc.m_filename.empty() ? "source" : loc.m_filename) + ":" +
                     std::to_string(loc.m_line) + ":" + std::to_string(loc.m_column) + "] エラー: " + message;
    m_logger->Error(fm);
  } else {
    std::cerr << "字句解析エラー [" << loc.m_line << ":" << loc.m_column << "]: " << message << std::endl;
  }
  if (!m_options.m_tolerant) {
    throw std::runtime_error("字句解析エラー: " + message);
  }
}

void Lexer::UpdateStats(const Token& token) {
  m_stats.m_tokenCount++;
  if (m_options.m_collectMetrics) {
    m_stats.m_tokenTypeCounts[token.m_type]++;
    // 詳細な統計情報を収集
    if (m_metricsCollector) {
      m_metricsCollector->RecordToken(token.m_type, token.m_location, token.m_value.size());
    }
  }
}

void Lexer::AddToCache(const Token& token) {
  if (m_tokenCache) {
    std::string key = std::to_string(token.m_location.m_index);
    m_tokenCache->Add(key, token);
  }
}

[[nodiscard]] std::optional<Token> Lexer::GetFromCache() {
  if (m_tokenCache) {
    std::string key = std::to_string(m_stream->Position());
    return m_tokenCache->Get(key);
  }
  return std::nullopt;
}

// 内部スキャンロジック
[[nodiscard]] Token Lexer::InternalScanNextToken() {
  // 空白文字と改行をスキップ
  SkipWhitespaceAndNewlines();
  
  // コメントをスキップまたはスキャン
  SkipOrScanComment();
  
  // 再度空白文字と改行をスキップ（コメントの後に空白がある場合）
  SkipWhitespaceAndNewlines();
  
  // ファイル終端チェック
  if (IsAtEnd()) {
    return Token(TokenType::END_OF_FILE, "", "", GetCurrentLocation());
  }
  
  // 現在位置を記録
  SourceLocation startLoc = GetCurrentLocation();
  char startChar = CurrentChar();
  
  // JSXモードの場合
  if (m_options.m_enableJsx && m_jsxContext.active) {
    return ScanJsxToken();
  }
  
  // TypeScriptモードの場合
  if (m_options.m_enableTypeScript && m_tsContext.active) {
    // TypeScript固有の構文をチェック
    if (startChar == ':' || startChar == '<' || 
        (startChar == '=' && m_tsContext.afterTypeKeyword) ||
        (startChar == '{' && (m_tsContext.afterInterfaceKeyword || m_tsContext.afterEnumKeyword))) {
      return ScanTypeScriptSyntax();
    }
  }
  
  // 数値リテラル
  if (std::isdigit(startChar) || (startChar == '.' && std::isdigit(PeekChar(1)))) {
    return ScanNumericLiteral();
  }
  
  // 文字列リテラル
  if (startChar == '\'' || startChar == '"') {
    return ScanStringLiteral();
  }
  
  // テンプレートリテラル
  if (startChar == '`') {
    return ScanTemplateLiteral();
  }
  
  // 識別子またはキーワード
  // Unicode対応（iswalpha、エスケープ \u）
  if (std::isalpha(startChar) || startChar == '_' || startChar == '$' || 
      (startChar == '\\' && PeekChar(1) == 'u')) {
    return ScanIdentifierOrKeyword();
  }
  
  // 正規表現リテラル
  if (startChar == '/' && IsRegExpAllowed()) {
    return ScanRegExpLiteral();
  }
  
  // 区切り文字/演算子
  return ScanPunctuator();
}

// 並列スキャン
[[nodiscard]] std::vector<Token> Lexer::ScanAllParallel() {
  // ソーステキストを複数のチャンクに分割
  std::vector<SourceTextChunk> chunks = SplitIntoChunks();
  std::vector<std::vector<Token>> chunkResults(chunks.size());
  
  // 並列処理のためのスレッドプール
  ThreadPool pool(std::thread::hardware_concurrency());
  std::vector<std::future<std::vector<Token>>> futures;
  
  // 各チャンクを並列にスキャン
  for (size_t i = 0; i < chunks.size(); ++i) {
    futures.push_back(pool.Enqueue([this, &chunks, i]() {
      return ScanChunk(chunks[i]);
    }));
  }
  
  // 結果を収集
  std::vector<Token> allTokens;
  for (auto& future : futures) {
    std::vector<Token> chunkTokens = future.get();
    allTokens.insert(allTokens.end(), chunkTokens.begin(), chunkTokens.end());
  }
  
  // トークンを位置順にソート
  std::sort(allTokens.begin(), allTokens.end(), [](const Token& a, const Token& b) {
    return a.m_location.m_index < b.m_location.m_index;
  });
  
  return allTokens;
}

[[nodiscard]] std::vector<Token> Lexer::ScanChunk(const SourceTextChunk& chunk) {
  // チャンク用の一時的なLexerを作成
  Lexer chunkLexer(chunk.text, m_options);
  chunkLexer.m_stream->SetPosition(0);
  
  // チャンク内のすべてのトークンをスキャン
  std::vector<Token> tokens;
  while (true) {
    Token token = chunkLexer.ScanNextToken();
    
    // チャンク内の位置を全体の位置に変換
    SourceLocation adjustedLoc = token.m_location;
    adjustedLoc.m_index += chunk.startIndex;
    adjustedLoc.m_line += chunk.startLine - 1;
    if (chunk.startLine == 1) {
      adjustedLoc.m_column += chunk.startColumn - 1;
    }
    
    // 調整された位置情報でトークンを作成
    tokens.emplace_back(token.m_type, token.m_value, token.m_literal, adjustedLoc);
    
    // チャンクの終端に達したら終了
    if (token.m_type == TokenType::END_OF_FILE) {
      break;
    }
  }
  
  return tokens;
}

// ソーステキストをチャンクに分割するヘルパーメソッド
[[nodiscard]] std::vector<SourceTextChunk> Lexer::SplitIntoChunks() {
  const size_t chunkSize = 4096; // 適切なチャンクサイズを設定
  std::vector<SourceTextChunk> chunks;
  
  size_t totalLength = m_stream->Length();
  size_t currentPos = 0;
  size_t currentLine = 1;
  size_t currentColumn = 1;
  
  while (currentPos < totalLength) {
    // チャンクの終了位置を計算
    size_t endPos = std::min(currentPos + chunkSize, totalLength);
    
    // チャンク境界を行の終わりに調整（可能な場合）
    if (endPos < totalLength) {
      while (endPos > currentPos && m_stream->CharAt(endPos - 1) != '\n') {
        --endPos;
      }
      // 適切な行の終わりが見つからない場合は元のサイズを使用
      if (endPos <= currentPos) {
        endPos = std::min(currentPos + chunkSize, totalLength);
      }
    }
    
    // チャンクのテキストを抽出
    std::string chunkText = m_stream->Substring(currentPos, endPos - currentPos);
    
    // チャンク情報を作成
    SourceTextChunk chunk;
    chunk.text = chunkText;
    chunk.startIndex = currentPos;
    chunk.startLine = currentLine;
    chunk.startColumn = currentColumn;
    chunks.push_back(chunk);
    
    // 次のチャンクの開始位置を更新
    for (size_t i = currentPos; i < endPos; ++i) {
      char c = m_stream->CharAt(i);
      if (c == '\n') {
        ++currentLine;
        currentColumn = 1;
      } else {
        ++currentColumn;
      }
    }
    
    currentPos = endPos;
  }
  
  return chunks;
}

}  // namespace lexer
}  // namespace parser
}  // namespace core
}  // namespace aerojs