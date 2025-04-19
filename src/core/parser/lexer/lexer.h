/**
 * @file lexer.h
 * @brief 高性能JavaScriptレキサー（字句解析器）の定義
 * 
 * このファイルはJavaScriptソースコードを読み取り、構文解析のためのトークン列に
 * 変換する高速なレキサーを定義します。ECMAScript最新仕様に準拠し、
 * JSX、TypeScriptなどの拡張構文にも対応しています。
 * 
 * @version 2.1.0
 * @copyright Copyright (c) 2024 AeroJS プロジェクト
 * @license MIT License
 */

#ifndef AEROJS_CORE_PARSER_LEXER_LEXER_H_
#define AEROJS_CORE_PARSER_LEXER_LEXER_H_

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string_view>
#include <functional>
#include <array>
#include <bitset>
#include <span>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <future>
#include <queue>
#include <variant>
#include <optional>
#include <immintrin.h>
#include "../token.h"
#include "../../utils/logger.h"
#include "../../utils/memory/arena_allocator.h"
#include "../../utils/memory/memory_pool.h"
#include "../../utils/thread/thread_pool.h"
#include "../../utils/metrics/metrics_collector.h"
#include "../../sourcemap/source_location.h"

// SIMD対応マクロ
#ifdef __AVX2__
#define AEROJS_LEXER_SIMD_AVX2 1
#endif

#ifdef __SSE4_2__
#define AEROJS_LEXER_SIMD_SSE42 1
#endif

namespace aerojs {
namespace core {

// 前方宣言
class CharacterStream;
class TokenCache;
class TokenLookupTable;
class LexerStateManager;
class SourceTextChunk;

/**
 * @brief レキサーオプション（拡張版）
 */
struct LexerOptions {
    // 基本オプション
    bool jsx_enabled = false;                ///< JSX構文を有効にするかどうか
    bool typescript_enabled = false;         ///< TypeScript構文を有効にするかどうか
    bool tolerant = false;                   ///< 厳密でない字句解析を許容するかどうか
    bool preserve_comments = false;          ///< コメントをトークンとして保持するかどうか
    bool support_bigint = true;              ///< BigInt構文をサポートするかどうか
    bool support_numeric_separators = true;  ///< 数値セパレータ（1_000_000など）をサポートするかどうか
    int ecmascript_version = 2024;           ///< ECMAScriptのバージョン
    
    // 高性能化オプション
    bool enable_simd = true;                 ///< SIMD最適化を有効にするかどうか
    bool enable_parallel_scan = true;        ///< 並列スキャンを有効にするかどうか
    bool optimize_memory = true;             ///< メモリ最適化を有効にするかどうか
    bool enable_token_caching = true;        ///< トークンキャッシングを有効にするかどうか
    size_t token_cache_size = 10000;         ///< トークンキャッシュサイズ
    size_t chunk_size = 32 * 1024;           ///< 並列処理時のチャンクサイズ
    size_t thread_count = 0;                 ///< 使用するスレッド数（0=自動）
    size_t memory_pool_size = 1024 * 1024;   ///< メモリプールサイズ（バイト）
    
    // エラー処理オプション
    bool detailed_error_messages = true;     ///< 詳細なエラーメッセージを生成するかどうか
    bool strict_mode = false;                ///< 厳格モードで解析するかどうか
    size_t max_errors = 100;                 ///< 最大エラー数
    
    // デバッグオプション
    bool trace_enabled = false;              ///< トレース出力を有効にするかどうか
    bool collect_metrics = true;             ///< メトリクス収集を有効にするかどうか
    bool validate_tokens = false;            ///< トークン検証を有効にするかどうか（デバッグ用）
};

/**
 * @brief レキサー統計情報（拡張版）
 */
struct LexerStats {
    // 基本統計
    size_t line_count = 0;                    ///< 処理された行数
    size_t token_count = 0;                   ///< 生成されたトークン数
    size_t comment_count = 0;                 ///< 検出されたコメント数
    size_t error_count = 0;                   ///< 検出されたエラー数
    int64_t scan_time_ns = 0;                 ///< スキャン処理時間（ナノ秒）
    
    // パフォーマンス統計
    double tokens_per_second = 0.0;           ///< トークン生成速度（毎秒）
    double characters_per_second = 0.0;       ///< 文字処理速度（毎秒）
    size_t peak_memory_usage_bytes = 0;       ///< ピークメモリ使用量（バイト）
    
    // キャッシュ統計
    size_t token_cache_hits = 0;              ///< トークンキャッシュヒット数
    size_t token_cache_misses = 0;            ///< トークンキャッシュミス数
    size_t lookahead_cache_hits = 0;          ///< 先読みキャッシュヒット数
    
    // 最適化統計
    size_t simd_operations = 0;               ///< SIMD操作回数
    size_t parallel_chunks_processed = 0;     ///< 並列処理されたチャンク数
    size_t memory_pool_allocations = 0;       ///< メモリプールからの割り当て回数
    
    // トークン種別統計
    std::unordered_map<TokenType, size_t> token_type_counts; ///< トークンタイプ別カウント
};

/**
 * @brief コメント情報（拡張版）
 */
struct Comment {
    enum class Type {
        SingleLine,     ///< 単一行コメント (// ...)
        MultiLine,      ///< 複数行コメント (/* ... */)
        JSDoc           ///< JSDocコメント (/** ... */)
    };
    
    Type type;                     ///< コメントの種類
    std::string value;             ///< コメントの内容
    SourceLocation location;       ///< コメントの位置
    SourceLocation end_location;   ///< コメントの終了位置
    bool is_trailing = false;      ///< 行末コメントかどうか
    
    // JSDoc解析用
    struct JSDocInfo {
        bool is_parsed = false;
        std::unordered_map<std::string, std::string> tags;
    };
    
    std::optional<JSDocInfo> jsdoc_info; ///< JSDoc情報（JSDocコメントの場合）
};

/**
 * @brief 文字ストリームクラス
 * 
 * 効率的な文字アクセスを提供し、SIMD操作をサポートします。
 */
class CharacterStream {
public:
    CharacterStream(std::string_view source);
    
    char current() const;
    char peek(size_t offset = 1) const;
    char advance();
    bool isAtEnd() const;
    size_t position() const;
    void setPosition(size_t pos);
    
    // SIMD最適化メソッド
    bool findNextInSet(const std::bitset<256>& char_set, size_t& pos);
    bool skipWhitespaceSSE();
    bool skipWhitespaceAVX2();
    std::string_view getSubview(size_t start, size_t length) const;
    
private:
    std::string_view m_source;
    size_t m_position = 0;
};

/**
 * @brief トークンキャッシュクラス
 * 
 * 生成されたトークンをキャッシュし、再利用を可能にします。
 */
class TokenCache {
public:
    explicit TokenCache(size_t capacity);
    
    void add(const std::string& key, const Token& token);
    std::optional<Token> get(const std::string& key);
    void clear();
    
    // 統計情報
    size_t hits() const;
    size_t misses() const;
    size_t size() const;
    
private:
    std::unordered_map<std::string, Token> m_cache;
    size_t m_capacity;
    std::shared_mutex m_mutex;
    std::atomic<size_t> m_hits{0};
    std::atomic<size_t> m_misses{0};
};

/**
 * @brief 高性能JavaScriptレキサークラス（最適化版）
 * 
 * JavaScriptソースコードを読み取り、トークン列に変換します。
 * 最新のECMAScript仕様に準拠し、高速かつメモリ効率の良い実装です。
 * SIMD命令、並列処理、メモリプールなどの最適化を採用しています。
 */
class Lexer {
public:
    /**
     * @brief デフォルトコンストラクタ
     */
    Lexer();
    
    /**
     * @brief オプション指定コンストラクタ
     * 
     * @param options レキサーオプション
     */
    explicit Lexer(const LexerOptions& options);
    
    /**
     * @brief デストラクタ
     */
    ~Lexer();
    
    /**
     * @brief ソースコードの初期化
     * 
     * @param source ソースコード
     * @param file_name ファイル名（エラーメッセージと位置情報に使用）
     */
    void init(const std::string& source, const std::string& file_name = "input.js");
    
    /**
     * @brief 次のトークンを取得
     * 
     * @return 次のトークン
     */
    Token nextToken();
    
    /**
     * @brief 次のトークンを先読み
     * 
     * 現在の解析位置を変更せずに、次のトークンを取得します。
     * 
     * @return 次のトークン
     */
    Token peekToken();
    
    /**
     * @brief 指定したオフセット先のトークンを先読み
     * 
     * 現在の解析位置を変更せずに、指定したオフセット先のトークンを取得します。
     * 
     * @param offset 先読みするトークンのオフセット（1以上）
     * @return 指定したオフセット先のトークン
     */
    Token peekToken(size_t offset);
    
    /**
     * @brief すべてのトークンをスキャン
     * 
     * ソースコード全体をスキャンし、すべてのトークンを取得します。
     * 高性能モードでは並列処理を使用します。
     * 
     * @return トークンのリスト
     */
    std::vector<Token> scanAll();
    
    /**
     * @brief 並列モードですべてのトークンをスキャン
     * 
     * マルチスレッドを使用して高速にトークン化します。
     * 
     * @return トークンのリスト
     */
    std::vector<Token> scanAllParallel();
    
    /**
     * @brief コメントの取得
     * 
     * @return スキャン中に検出されたコメントのリスト
     */
    const std::vector<Comment>& getComments() const;
    
    /**
     * @brief コメントを解析してJSDoc情報を抽出
     * 
     * @param parse_all すべてのコメントを解析するかどうか（falseの場合はJSDocコメントのみ）
     */
    void parseComments(bool parse_all = false);
    
    /**
     * @brief 現在の行番号を取得
     * 
     * @return 現在の行番号
     */
    int getLine() const;
    
    /**
     * @brief 現在の列番号を取得
     * 
     * @return 現在の列番号
     */
    int getColumn() const;
    
    /**
     * @brief 現在の文字位置を取得
     * 
     * @return 現在の文字位置
     */
    size_t getOffset() const;
    
    /**
     * @brief オプションの取得
     * 
     * @return 現在のレキサーオプション
     */
    const LexerOptions& getOptions() const;
    
    /**
     * @brief オプションの設定
     * 
     * @param options 新しいレキサーオプション
     */
    void setOptions(const LexerOptions& options);
    
    /**
     * @brief 統計情報の取得
     * 
     * @return レキサー統計情報
     */
    const LexerStats& getStats() const;
    
    /**
     * @brief トークンの文字列表現を取得
     * 
     * @param token トークン
     * @return トークンの文字列表現
     */
    static std::string tokenToString(const Token& token);
    
    /**
     * @brief トークンタイプの文字列表現を取得
     * 
     * @param type トークンタイプ
     * @return トークンタイプの文字列表現
     */
    static std::string tokenTypeToString(TokenType type);
    
    /**
     * @brief メモリ使用統計を取得
     * 
     * @return メモリ使用統計の文字列表現
     */
    std::string getMemoryStats() const;
    
    /**
     * @brief パフォーマンスメトリクスを取得
     * 
     * @return パフォーマンスメトリクスの文字列表現
     */
    std::string getPerformanceMetrics() const;
    
    /**
     * @brief すべてのキャッシュをクリア
     */
    void clearCaches();
    
    /**
     * @brief キーワードテーブルの初期化
     * 
     * キーワードの検索を高速化するためのテーブルを構築します。
     */
    static void initializeKeywordTables();
    
    /**
     * @brief SIMD最適化をサポートしているかどうかを確認
     * 
     * @return サポートしている場合はtrue
     */
    static bool hasSIMDSupport();
    
    /**
     * @brief パーサーの状態を保存
     * 
     * @return 状態ID
     */
    size_t saveState();
    
    /**
     * @brief パーサーの状態を復元
     * 
     * @param state_id 保存された状態ID
     * @return 成功した場合はtrue
     */
    bool restoreState(size_t state_id);

private:
    // トークンスキャン関数
    Token scanToken();
    void skipWhitespace();
    void skipComment();
    
    // SIMD最適化スキャン関数
    Token scanTokenSIMD();
    void skipWhitespaceSIMD();
    void skipCommentSIMD();
    
    // 並列スキャン関数
    std::vector<Token> scanChunk(size_t start, size_t end);
    void mergeChunkResults(std::vector<std::vector<Token>>& chunk_results);
    void prepareForParallelScan();
    
    // 文字処理関数
    char advance();
    char peek(size_t offset = 0) const;
    bool match(char expected);
    bool matchSequence(const std::string& seq);
    
    // 位置情報
    SourceLocation currentLocation() const;
    void newLine();
    void updateLocation();
    
    // トークン生成関数
    Token makeToken(TokenType type);
    Token errorToken(const std::string& message);
    
    // 最適化されたトークン生成
    Token makeTokenFast(TokenType type, std::string_view lexeme);
    Token allocateToken(TokenType type, std::string_view lexeme, const SourceLocation& loc);
    
    // 識別子とキーワード
    Token identifier();
    TokenType identifierType(std::string_view ident);
    TokenType checkKeyword(std::string_view ident);
    bool isKeyword(std::string_view ident) const;
    
    // リテラル
    Token number();
    Token string();
    Token templateLiteral();
    Token regExp();
    
    // JSX関連
    Token jsxIdentifier();
    Token jsxString();
    Token jsxText();
    Token jsxTagStart();
    Token jsxTagEnd();
    
    // キャッシュ関連
    std::string generateCacheKey(size_t position, char current_char) const;
    void cacheToken(const Token& token);
    std::optional<Token> getCachedToken(size_t position, char current_char);
    
    // メモリ管理
    void* allocateMemory(size_t size);
    void deallocateMemory(void* ptr, size_t size);
    std::string internString(std::string_view sv);
    
    // ヘルパー関数
    bool isDigit(char c) const;
    bool isHexDigit(char c) const;
    bool isOctalDigit(char c) const;
    bool isBinaryDigit(char c) const;
    bool isAlpha(char c) const;
    bool isAlphaNumeric(char c) const;
    bool isIdentifierStart(char c) const;
    bool isIdentifierPart(char c) const;
    bool isWhitespace(char c) const;
    bool isNewLine(char c) const;
    bool isEscapeSequence(char c) const;
    
    // SIMD最適化ヘルパー関数
    bool isFastPathIdentifier(char c) const;
    bool isSimpleToken(char c) const;
    TokenType recognizeSimpleToken(char c) const;
    bool tryFastPathScan(Token& result);
    void prefetchNextTokens();
    
    // 文字エスケープ処理
    bool scanEscapeSequence(std::string& result);
    bool scanUnicodeEscapeSequence(std::string& result);
    
    // 初期化関数
    void initKeywordMap();
    void initializeCharacterTables();
    void initializeSIMD();
    void initializeMemoryPool();
    void initializeThreadPool();
    
    // キーワードマップ
    static const std::unordered_map<std::string, TokenType>& getKeywordMap();
    
    // SIMD関連メンバ
    static constexpr size_t SIMD_REGISTER_SIZE = 32; // AVX2の場合
    alignas(32) std::array<char, SIMD_REGISTER_SIZE> m_simd_buffer;
    
    // ビットセット（高速判定用）
    alignas(32) std::bitset<256> m_whitespace_chars;
    alignas(32) std::bitset<256> m_identifier_start_chars;
    alignas(32) std::bitset<256> m_identifier_part_chars;
    alignas(32) std::bitset<256> m_digit_chars;
    alignas(32) std::bitset<256> m_hex_digit_chars;
    
    // メモリ最適化
    std::unordered_set<std::string> m_string_pool;
    
    // コメント処理
    std::vector<Comment> m_comments;
    bool m_inside_comment = false;
    
    // メンバ変数
    LexerOptions m_options;
    LexerStats m_stats;
    std::string m_source;
    std::string m_file_name;
    size_t m_current_pos = 0;
    size_t m_line = 1;
    size_t m_column = 0;
    
    // 最適化用オブジェクト
    std::unique_ptr<CharacterStream> m_char_stream;
    std::unique_ptr<TokenCache> m_token_cache;
    std::unique_ptr<MemoryPool> m_memory_pool;
    std::unique_ptr<LexerStateManager> m_state_manager;
    std::unique_ptr<ThreadPool> m_thread_pool;
    
    // タイプテーブル（高速ルックアップ用）
    alignas(64) std::array<uint8_t, 256> m_char_type_table;
    alignas(64) std::array<TokenType, 256> m_simple_token_table;
    
    // ルックアヘッドキャッシュ
    static constexpr size_t LOOKAHEAD_SIZE = 8;
    std::array<Token, LOOKAHEAD_SIZE> m_lookahead_cache;
    size_t m_lookahead_count = 0;
    size_t m_lookahead_pos = 0;
    
    // 統計取得用タイマー
    std::chrono::high_resolution_clock::time_point m_start_time;
    
    // 共有変数用ミューテックス
    mutable std::shared_mutex m_mutex;
    
    // メトリクス収集
    std::unique_ptr<MetricsCollector> m_metrics;
    
    // シリアル化状態
    struct LexerState {
        size_t position;
        size_t line;
        size_t column;
        std::vector<Token> lookahead_cache;
        size_t lookahead_count;
        size_t lookahead_pos;
    };
    
    std::unordered_map<size_t, LexerState> m_saved_states;
    size_t m_next_state_id = 1;
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_PARSER_LEXER_LEXER_H_ 