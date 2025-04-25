/*******************************************************************************
 * @file src/core/parser/lexer/lexer.h
 * @brief 高性能JavaScriptレキサー（字句解析器）の定義
 *
 * このファイルはJavaScriptソースコードを読み取り、構文解析のためのトークン列に
 * 変換する高速なレキサーを定義します。ECMAScript最新仕様に準拠し、
 * JSX、TypeScriptなどの拡張構文にも対応しています。
 *
 * @version 2.1.0
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 ******************************************************************************/

#pragma once

// C++ 標準ライブラリ
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// プロジェクト内の依存関係
#include "src/core/parser/lexer/token/token.h"
#include "src/core/sourcemap/source_location.h"

// SIMD サポート検出
#ifdef __AVX2__
#define AEROJS_LEXER_SIMD_AVX2 1
#endif

#ifdef __SSE4_2__
#define AEROJS_LEXER_SIMD_SSE42 1
#endif

namespace aerojs::core::parser::lexer {

// 前方宣言
class TokenLookupTable;
class LexerStateManager;
class SourceTextChunk;
struct Token;
struct SourceLocation;

namespace utils {
// 外部コンポーネント用の前方宣言
}

/**
 * @brief レキサーオプション
 */
struct LexerOptions {
  // 基本オプション
  bool m_jsxEnabled = false;
  bool m_typescriptEnabled = false;
  bool m_tolerant = false;
  bool m_preserveComments = false;
  bool m_supportBigInt = true;
  bool m_supportNumericSeparators = true;
  int m_ecmascriptVersion = 2024;

  // 高性能化オプション
  bool m_enableSimd = true;
  bool m_enableParallelScan = true;
  bool m_optimizeMemory = true;
  bool m_enableTokenCaching = true;
  size_t m_tokenCacheSize = 10000;
  size_t m_chunkSize = 32 * 1024;
  size_t m_threadCount = 0;
  size_t m_memoryPoolSize = 1024 * 1024;

  // エラー処理オプション
  bool m_detailedErrorMessages = true;
  bool m_strictMode = false;
  size_t m_maxErrors = 100;

  // デバッグオプション
  bool m_traceEnabled = false;
  bool m_collectMetrics = true;
  bool m_validateTokens = false;
};

/**
 * @brief レキサー統計情報
 */
struct LexerStats {
  // 基本統計
  size_t m_lineCount = 0;
  size_t m_tokenCount = 0;
  size_t m_commentCount = 0;
  size_t m_errorCount = 0;
  int64_t m_scanTimeNs = 0;

  // パフォーマンス統計
  double m_tokensPerSecond = 0.0;
  double m_charactersPerSecond = 0.0;
  size_t m_peakMemoryUsageBytes = 0;

  // キャッシュ統計
  size_t m_tokenCacheHits = 0;
  size_t m_tokenCacheMisses = 0;
  size_t m_lookaheadCacheHits = 0;

  // 最適化統計
  size_t m_simdOperations = 0;
  size_t m_parallelChunksProcessed = 0;
  size_t m_memoryPoolAllocations = 0;

  // トークン種別統計
  std::unordered_map<TokenType, size_t> m_tokenTypeCounts;
};

/**
 * @brief コメント情報
 */
struct Comment {
  enum class Type {
    SingleLine,  // 単一行コメント (// ...)
    MultiLine,   // 複数行コメント (/* ... */)
    JSDoc        // JSDocコメント (/** ... */)
  };

  /**
   * @struct JSDocInfo
   * @brief JSDocコメントの解析情報
   */
  struct JSDocInfo {
    bool m_isParsed = false;
    std::unordered_map<std::string, std::string> m_tags;
  };

  Type m_type;
  std::string m_value;
  SourceLocation m_location;
  SourceLocation m_endLocation;
  bool m_isTrailing = false;
  std::optional<JSDocInfo> m_jsdocInfo;
};

/**
 * @brief 文字ストリームクラス
 * 効率的な文字アクセスを提供し、SIMD操作をサポートします。
 */
class CharacterStream {
 public:
  explicit CharacterStream(std::string_view source);
  ~CharacterStream() = default;

  // コピー/ムーブ制御
  CharacterStream(const CharacterStream&) = default;
  CharacterStream& operator=(const CharacterStream&) = default;
  CharacterStream(CharacterStream&&) noexcept = default;
  CharacterStream& operator=(CharacterStream&&) noexcept = default;

  [[nodiscard]] char Current() const noexcept;
  [[nodiscard]] char Peek(size_t offset = 1) const noexcept;
  char Advance() noexcept;
  [[nodiscard]] bool IsAtEnd() const noexcept;
  [[nodiscard]] size_t Position() const noexcept;
  void SetPosition(size_t pos) noexcept;

  // SIMD最適化メソッド
  [[nodiscard]] bool FindNextInSet(const std::bitset<256>& char_set, size_t& pos) const;
  bool SkipWhitespaceSSE() noexcept;
  bool SkipWhitespaceAVX2() noexcept;
  [[nodiscard]] std::string_view GetSubview(size_t start, size_t length) const;

 private:
  std::string_view m_source;
  size_t m_position = 0;
};

/**
 * @brief トークンキャッシュクラス
 * 生成されたトークンをキャッシュし、再利用を可能にします。
 */
class TokenCache {
 public:
  explicit TokenCache(size_t capacity);
  ~TokenCache() = default;

  // コピー禁止
  TokenCache(const TokenCache&) = delete;
  TokenCache& operator=(const TokenCache&) = delete;

  // ムーブ許可
  TokenCache(TokenCache&&) noexcept = default;
  TokenCache& operator=(TokenCache&&) noexcept = default;

  void Add(const std::string& key, const Token& token);
  [[nodiscard]] std::optional<Token> Get(const std::string& key);
  void Clear();

  // 統計情報
  [[nodiscard]] size_t Hits() const noexcept;
  [[nodiscard]] size_t Misses() const noexcept;
  [[nodiscard]] size_t Size() const noexcept;

 private:
  std::unordered_map<std::string, Token> m_cache;
  size_t m_capacity;
  std::shared_mutex m_mutex;
  std::atomic<size_t> m_hits{0};
  std::atomic<size_t> m_misses{0};
};

/**
 * @class Lexer
 * @brief 高性能JavaScriptレキサークラス
 *
 * JavaScriptソースコードを読み取り、トークン列に変換します。
 * 最新のECMAScript仕様に準拠し、高速かつメモリ効率の良い実装です。
 * SIMD命令、並列処理、メモリプールなどの最適化を採用しています。
 */
class Lexer {
 public:
  /**
   * @brief コンストラクタ
   * @param source 解析対象のソースコード
   * @param options レキサーオプション
   * @param logger オプションのロガー
   * @param allocator オプションのメモリアロケータ
   * @param thread_pool オプションのスレッドプール
   * @param metrics_collector オプションのメトリクスコレクタ
   */
  explicit Lexer(
      std::string_view source,
      const LexerOptions& options,
      std::shared_ptr<utils::Logger> logger = nullptr,
      std::shared_ptr<utils::memory::ArenaAllocator> allocator = nullptr,
      std::shared_ptr<utils::thread::ThreadPool> thread_pool = nullptr,
      std::shared_ptr<utils::metrics::MetricsCollector> metrics_collector = nullptr);

  ~Lexer() = default;

  // コピー禁止
  Lexer(const Lexer&) = delete;
  Lexer& operator=(const Lexer&) = delete;

  // ムーブ許可
  Lexer(Lexer&&) noexcept = default;
  Lexer& operator=(Lexer&&) noexcept = default;

  /**
   * @brief 次のトークンをスキャンして返します
   * @return 次のトークン。ストリームの終端に達した場合はEOFトークンを返します
   * @throws LexerError 字句解析エラーが発生した場合
   */
  [[nodiscard]] Token ScanNext();

  /**
   * @brief 全てのトークンをスキャンして返します
   * @return ソースコード全体のトークンリスト
   * @throws LexerError 字句解析エラーが発生した場合
   */
  [[nodiscard]] std::vector<Token> ScanAll();

  /**
   * @brief 現在のレキサーの状態をリセットします
   */
  void Reset();

  /**
   * @brief 現在のレキサーの位置を返します
   * @return 現在のSourceLocation
   */
  [[nodiscard]] SourceLocation GetCurrentLocation() const noexcept;

  /**
   * @brief レキサー統計情報を取得します
   * @return 現在のLexerStatsオブジェクトへの参照
   */
  [[nodiscard]] const LexerStats& GetStats() const noexcept;

  /**
   * @brief 収集されたコメントを取得します
   * @return コメントのリストへの参照
   */
  [[nodiscard]] const std::vector<Comment>& GetComments() const noexcept;

  /**
   * @brief 特定のトークンタイプをスキップします
   * @param type スキップするTokenType
   * @return スキップした場合はtrue、そうでなければfalse
   */
  bool SkipToken(TokenType type) noexcept;

  /**
   * @brief 次のトークンを先読みします
   * @param offset 先読みするトークン数（デフォルトは1）
   * @return 先読みしたトークン
   */
  [[nodiscard]] Token Peek(size_t offset = 1);

  /**
   * @brief レキサーの状態を保存します
   * @return 状態を表すオブジェクト
   */
  [[nodiscard]] std::unique_ptr<LexerStateManager> SaveState();

  /**
   * @brief 保存した状態を復元します
   * @param state SaveState()で取得した状態オブジェクト
   */
  void RestoreState(const LexerStateManager& state);

 private:
  // メンバー変数
  std::string_view m_source;
  LexerOptions m_options;
  std::shared_ptr<utils::Logger> m_logger;
  std::shared_ptr<utils::memory::ArenaAllocator> m_allocator;
  std::shared_ptr<utils::thread::ThreadPool> m_threadPool;
  std::shared_ptr<utils::metrics::MetricsCollector> m_metricsCollector;

  std::unique_ptr<CharacterStream> m_stream;
  std::unique_ptr<TokenCache> m_tokenCache;
  std::unique_ptr<TokenLookupTable> m_lookupTable;
  std::unique_ptr<LexerStateManager> m_stateManager;

  SourceLocation m_currentLocation;
  LexerStats m_stats;
  std::vector<Comment> m_comments;
  std::vector<Token> m_lookaheadBuffer;

  // ヘルパーメソッド
  char Advance();
  [[nodiscard]] char CurrentChar() const noexcept;
  [[nodiscard]] char PeekChar(size_t offset = 1) const noexcept;
  [[nodiscard]] bool IsAtEnd() const noexcept;
  [[nodiscard]] SourceLocation CreateSourceLocation() const;

  void SkipWhitespaceAndNewlines();
  void SkipOrScanComment();
  void ScanSingleLineComment();
  void ScanMultiLineComment();
  [[nodiscard]] Token ScanStringLiteral();
  [[nodiscard]] Token ScanNumericLiteral();
  [[nodiscard]] Token ScanIdentifierOrKeyword();
  [[nodiscard]] Token ScanRegExpLiteral();
  [[nodiscard]] Token ScanTemplateLiteral();
  [[nodiscard]] Token ScanPunctuator();
  [[nodiscard]] Token ScanJsxToken();
  [[nodiscard]] Token ScanTypeScriptSyntax();

  void ReportError(const std::string& message, const SourceLocation& loc);
  void UpdateStats(const Token& token);
  void AddToCache(const Token& token);
  [[nodiscard]] std::optional<Token> GetFromCache();

  [[nodiscard]] std::vector<Token> ScanAllParallel();
  [[nodiscard]] std::vector<Token> ScanChunk(const SourceTextChunk& chunk);
};

}  // namespace aerojs::core::parser::lexer

#endif  // AEROJS_CORE_PARSER_LEXER_LEXER_H_