/**
 * @file parser.h
 * @brief 高性能JavaScriptパーサーのインターフェース定義
 * @version 2.0.0
 * @copyright 2024 AeroJS プロジェクト
 * @license MIT
 */

#ifndef AEROJS_CORE_PARSER_PARSER_H_
#define AEROJS_CORE_PARSER_PARSER_H_

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <memory_resource>
#include <optional>
#include <simd/simd.h>
#include <span>
#include <stack>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

#include "../utils/logger.h"
#include "../utils/memory/arena_allocator.h"
#include "../utils/memory/object_pool.h"
#include "../utils/metrics/metrics_collector.h"
#include "../utils/thread/thread_pool.h"
#include "ast/ast.h"
#include "lexer/lexer.h"
#include "lexer/token/token.h"
#include "parser_error.h"

namespace aerojs {
namespace core {

// 前方宣言
class Scanner;
class ParserMemoryManager;
class ParserErrorRecovery;
class ParserMetrics;
class ParserSecurity;
class TokenCache;
class TokenProcessor;
class MemoryMonitor;
class ArenaAllocator;

// SIMD サポート情報
struct SIMDSupport {
  bool sse = false;
  bool avx = false;
  bool avx512 = false;
};

/**
 * @brief パーサー設定オプション（拡張版）
 */
struct ParserOptions {
  // 基本設定
  bool strict_mode = false;            ///< 厳格モードでパースするかどうか
  bool module_mode = false;            ///< ESモジュールとしてパースするかどうか
  bool jsx_enabled = false;            ///< JSXパースを有効にするかどうか
  bool typescript_enabled = false;     ///< TypeScriptパースを有効にするかどうか
  bool webassembly_enabled = false;    ///< WebAssembly関連機能を有効にするかどうか
  int ecmascript_version = 2024;       ///< ECMAScriptのバージョン（デフォルトは最新）
  std::string source_type = "script";  ///< ソースタイプ（"script", "module", "json"）
  bool preserve_parens = false;        ///< 括弧の情報をASTに保持するかどうか
  bool locations = true;               ///< 位置情報を保持するかどうか
  bool tolerant = false;               ///< 致命的でないエラーを許容するかどうか

  // パフォーマンス設定
  size_t max_tokens = 1000000;      ///< 最大トークン数
  size_t max_ast_depth = 1000;      ///< 最大AST深さ
  size_t thread_pool_size = 4;      ///< 並列パース用スレッド数
  size_t lookahead_cache_size = 8;  ///< ルックアヘッドキャッシュサイズ

  // メモリ設定
  size_t arena_block_size = 16384;               ///< アリーナブロックサイズ
  size_t max_memory_usage = 1024 * 1024 * 1024;  ///< 最大メモリ使用量(1GB)
  bool use_object_pool = true;                   ///< オブジェクトプールを使用するか

  // エラー処理設定
  bool detailed_errors = true;      ///< 詳細なエラー情報を生成するか
  size_t max_errors = 100;          ///< 最大エラー報告数
  bool recover_from_errors = true;  ///< エラーから回復を試みるか

  // セキュリティ設定
  size_t max_recursion_depth = 500;         ///< 最大再帰深さ
  bool sanitize_literals = true;            ///< リテラルのサニタイズを行うか
  bool prevent_prototype_pollution = true;  ///< プロトタイプ汚染を防ぐ

  // 拡張機能設定
  bool enable_private_fields = true;    ///< プライベートフィールドを有効にするか
  bool enable_decorators = true;        ///< デコレータを有効にするか
  bool enable_top_level_await = true;   ///< トップレベルawaitを有効にするか
  bool enable_pattern_matching = true;  ///< パターンマッチングを有効にするか

  ParserOptions() = default;
};

/**
 * @brief パーサー統計情報（拡張版）
 */
struct ParserStats {
  // 基本統計
  size_t token_count = 0;     ///< 処理されたトークン数
  int64_t parse_time_ms = 0;  ///< パース処理にかかった時間（ミリ秒）

  // メモリ統計
  size_t peak_memory_usage = 0;  ///< ピークメモリ使用量
  size_t arena_allocations = 0;  ///< アリーナアロケーション回数
  size_t object_pool_hits = 0;   ///< オブジェクトプールヒット数

  // キャッシュ統計
  size_t lookahead_cache_hits = 0;    ///< ルックアヘッドキャッシュヒット数
  size_t lookahead_cache_misses = 0;  ///< ルックアヘッドキャッシュミス数

  // エラー統計
  size_t error_count = 0;       ///< エラー総数
  size_t recovered_errors = 0;  ///< 回復したエラー数
  size_t fatal_errors = 0;      ///< 致命的エラー数

  // パフォーマンス統計
  double tokens_per_second = 0.0;     ///< トークン処理速度
  double ast_nodes_per_second = 0.0;  ///< AST生成速度
  size_t parallel_parses = 0;         ///< 並列パース回数
};

/**
 * @brief プログラムで検出された言語機能
 */
struct ProgramFeatures {
  bool has_strict_mode = false;         ///< strict mode
  bool has_modules = false;             ///< ES modules
  bool has_classes = false;             ///< クラス
  bool has_arrow_functions = false;     ///< アロー関数
  bool has_let_declarations = false;    ///< let宣言
  bool has_const_declarations = false;  ///< const宣言
  bool has_destructuring = false;       ///< 分割代入
  bool has_default_params = false;      ///< デフォルトパラメータ
  bool has_rest_params = false;         ///< レストパラメータ
  bool has_spread_operator = false;     ///< スプレッド演算子
  bool has_template_literals = false;   ///< テンプレートリテラル
  bool has_generators = false;          ///< ジェネレータ
  bool has_async_await = false;         ///< async/await
  bool has_for_of = false;              ///< for-of
  bool has_object_spread = false;       ///< オブジェクトスプレッド
  bool has_nullish_coalescing = false;  ///< nullish coalescing
  bool has_optional_chaining = false;   ///< オプショナルチェイニング
  bool has_big_int = false;             ///< BigInt
  bool has_dynamic_import = false;      ///< 動的import()
  bool has_private_methods = false;     ///< プライベートメソッド
  bool has_class_fields = false;        ///< クラスフィールド
  bool has_top_level_await = false;     ///< トップレベルawait
  bool has_logical_assignment = false;  ///< 論理代入 (||=, &&=, ??=)
  bool has_numeric_separators = false;  ///< 数値セパレータ
  bool has_import_meta = false;         ///< import.meta

  // 危険な機能や非推奨機能の検出
  bool has_eval = false;                  ///< eval()の使用
  bool has_with_statement = false;        ///< with文
  bool has_function_constructor = false;  ///< Function constructor
  bool has_implicit_globals = false;      ///< 暗黙のグローバル変数
};

/**
 * @brief オペレータの優先順位
 */
enum class Precedence {
  None,            ///< 優先順位なし
  Comma,           ///< カンマ演算子
  Assignment,      ///< 代入 =, +=, -=, etc.
  Conditional,     ///< 条件演算子 ?:
  LogicalOr,       ///< 論理和 ||, ??
  LogicalAnd,      ///< 論理積 &&
  BitwiseOr,       ///< ビット論理和 |
  BitwiseXor,      ///< ビット排他的論理和 ^
  BitwiseAnd,      ///< ビット論理積 &
  Equality,        ///< 等価 ==, !=, ===, !==
  Relational,      ///< 関係 <, >, <=, >=, instanceof, in
  Shift,           ///< シフト <<, >>, >>>
  Additive,        ///< 加減算 +, -
  Multiplicative,  ///< 乗除算 *, /, %
  Exponentiation,  ///< べき乗 **
  Unary,           ///< 前置演算子 !, ~, +, -, typeof, void, delete
  Update,          ///< 更新演算子 ++, --
  LeftHandSide,    ///< メンバーアクセス ., [], ()
  Primary          ///< リテラル、識別子など
};

/**
 * @brief パーサーコンテキスト
 */
struct ParserContext {
  std::string type;                             ///< コンテキストタイプ
  std::unordered_map<std::string, bool> flags;  ///< コンテキストフラグ
};

namespace parser {

/**
 * @class Parser
 * @brief ECMAScript仕様完全対応のPEG+LR混合構文解析器
 *
 * 高度なエラーリカバリ、インクリメンタル解析、AST生成機能を提供。
 */
class Parser {
 public:
  /**
   * @brief Scannerから初期化する
   * @param scanner 走査器インスタンス
   */
  explicit Parser(lexer::Scanner& scanner);

  /**
   * @brief プログラム全体を解析してASTを生成する
   * @return ルートASTノード (Program)
   */
  ast::NodePtr parseProgram();

  /**
   * @brief モジュールとしてパース
   * @return ルートASTノード (Module)
   */
  ast::NodePtr parseModule();

  /**
   * @brief ステートメントリストを解析
   * @return ステートメントリストノード
   */
  ast::NodePtr parseStatementList();

  /**
   * @brief 単一ステートメントを解析
   * @return ステートメントノード
   */
  ast::NodePtr parseStatement();

  /**
   * @brief 任意の式を解析
   * @return 式ノード
   */
  ast::NodePtr parseExpression();

  /**
   * @brief パースイベントの統計をリセット
   */
  void resetStatistics();

  /**
   * @brief パース失敗時のエラーリストを取得
   * @return エラー情報リスト
   */
  const std::vector<parser_error::ErrorInfo>& getErrors() const;

  /**
   * @brief 現在のトークンを取得
   */
  Token currentToken() const;

 private:
  lexer::Scanner& scanner_;  ///< 字句解析器
  Token lookahead_;          ///< 先読みトークン

  // 基本ユーティリティ
  void advance();
  bool match(TokenType type);
  void expect(TokenType type);

  // 構文解析メソッド
  ast::NodePtr parsePrimaryExpression();
  ast::NodePtr parseAssignmentExpression();
  ast::NodePtr parseBinaryExpression(int minPrecedence);
  ast::NodePtr parseUnaryExpression();
  ast::NodePtr parseMemberExpression();
  ast::NodePtr parseCallExpression(ast::NodePtr callee);
  ast::NodePtr parseTemplateExpression();
  ast::NodePtr parseObjectLiteral();
  ast::NodePtr parseArrayLiteral();
  ast::NodePtr parseFunctionDeclaration();
  ast::NodePtr parseFunctionExpression();
  ast::NodePtr parseArrowFunctionExpression();
  ast::NodePtr parseClassDeclaration();
  ast::NodePtr parseClassExpression();
  ast::NodePtr parseImportDeclaration();
  ast::NodePtr parseExportDeclaration();

  // PEG + LR 混合パーステーブル
  static const int kParseTable[][/*states*/];
  static const int kProductionRules[][/*rules*/];

  // エラーリカバリヘルパー
  void recoverFromError();
  ast::NodePtr createErrorNode(const std::string& message);

  // インクリメンタル解析フック
  bool incrementalMode_ = false;
  void setIncrementalMode(bool enable);
  void applyChanges(const std::string& delta);

  // メタデータ生成用
  void attachSourceLocation(ast::NodePtr node, const Token& tok);
  void attachScopeInfo(ast::NodePtr node);

  // パフォーマンス計測
  size_t parseCount_ = 0;
  size_t errorCount_ = 0;
  std::chrono::steady_clock::time_point startTime_;
};

}  // namespace parser

/**
 * @brief 高性能JavaScriptパーサークラス（拡張版）
 */
class Parser {
 public:
  /**
   * @brief デフォルトコンストラクタ
   */
  Parser();

  /**
   * @brief オプション指定コンストラクタ
   *
   * @param options パーサーオプション
   */
  explicit Parser(const ParserOptions& options);

  /**
   * @brief デストラクタ
   */
  ~Parser();

  /**
   * @brief ソースコードをパースしてASTを生成
   *
   * @param source ソースコード
   * @param filename ファイル名（エラーメッセージに使用）
   * @return パース結果のAST
   */
  std::unique_ptr<ast::Program> parse(const std::string& source, const std::string& filename = "input.js");

  /**
   * @brief 式だけをパース
   *
   * @param source 式を含むソースコード
   * @param filename ファイル名（エラーメッセージに使用）
   * @return 式のAST
   */
  std::unique_ptr<ast::Expression> parseExpression(const std::string& source, const std::string& filename = "input.js");

  /**
   * @brief JSONをパース
   *
   * @param json JSONソース
   * @param filename ファイル名（エラーメッセージに使用）
   * @return パース結果のAST
   */
  std::unique_ptr<ast::Node> parseJson(const std::string& json, const std::string& filename = "input.json");

  /**
   * @brief パーサーオプションの取得
   *
   * @return 現在のパーサーオプション
   */
  const ParserOptions& getOptions() const;

  /**
   * @brief パーサーオプションの設定
   *
   * @param options 新しいパーサーオプション
   */
  void setOptions(const ParserOptions& options);

  /**
   * @brief パースエラーの取得
   *
   * @return パース中に検出されたエラーのリスト
   */
  const std::vector<ParserError>& getErrors() const {
    return m_errors;
  }

  /**
   * @brief エラーが発生したかどうかの確認
   *
   * @return エラーが発生した場合はtrue
   */
  bool hasError() const {
    return m_had_error;
  }

  /**
   * @brief パース統計情報の取得
   *
   * @return パース統計情報
   */
  const ParserStats& getStats() const {
    return m_stats;
  }

  /**
   * @brief 並列パース処理の実行
   */
  std::vector<std::unique_ptr<ast::Program>> parseParallel(
      const std::vector<std::string>& sources,
      const std::vector<std::string>& filenames = {});

  /**
   * @brief メモリ使用状況の取得
   */
  std::string getMemoryStats() const;

  /**
   * @brief パフォーマンスメトリクスの取得
   */
  std::string getPerformanceMetrics() const;

  /**
   * @brief セキュリティ設定の更新
   */
  void updateSecuritySettings(const ParserOptions& options);

 private:
  // メインパース関数
  std::unique_ptr<ast::Program> parseProgram();
  void parseDirectivePrologue(ast::Program* program);
  std::unique_ptr<ast::Statement> parseStatement();
  std::unique_ptr<ast::Declaration> parseDeclaration();
  std::unique_ptr<ast::Expression> parseExpression(Precedence min_precedence = Precedence::Assignment);
  std::unique_ptr<ast::Expression> parsePrefixExpression();

  // 文の種類別パース関数
  std::unique_ptr<ast::BlockStatement> parseBlockStatement();
  std::unique_ptr<ast::ExpressionStatement> parseExpressionStatement();
  std::unique_ptr<ast::IfStatement> parseIfStatement();
  std::unique_ptr<ast::SwitchStatement> parseSwitchStatement();
  std::unique_ptr<ast::ForStatement> parseForStatement();
  std::unique_ptr<ast::ForInStatement> parseForInStatement();
  std::unique_ptr<ast::ForOfStatement> parseForOfStatement();
  std::unique_ptr<ast::WhileStatement> parseWhileStatement();
  std::unique_ptr<ast::DoWhileStatement> parseDoWhileStatement();
  std::unique_ptr<ast::TryStatement> parseTryStatement();
  std::unique_ptr<ast::ThrowStatement> parseThrowStatement();
  std::unique_ptr<ast::ReturnStatement> parseReturnStatement();
  std::unique_ptr<ast::BreakStatement> parseBreakStatement();
  std::unique_ptr<ast::ContinueStatement> parseContinueStatement();
  std::unique_ptr<ast::WithStatement> parseWithStatement();
  std::unique_ptr<ast::EmptyStatement> parseEmptyStatement();
  std::unique_ptr<ast::LabeledStatement> parseLabeledStatement();
  std::unique_ptr<ast::DebuggerStatement> parseDebuggerStatement();

  // 宣言のパース関数
  std::unique_ptr<ast::FunctionDeclaration> parseFunctionDeclaration();
  std::unique_ptr<ast::VariableDeclaration> parseVariableDeclaration(const std::string& kind = "var");
  std::unique_ptr<ast::ClassDeclaration> parseClassDeclaration();
  std::unique_ptr<ast::ImportDeclaration> parseImportDeclaration();
  std::unique_ptr<ast::ExportDeclaration> parseExportDeclaration();

  // 式のパース関数
  std::unique_ptr<ast::Identifier> parseIdentifier();
  std::unique_ptr<ast::Literal> parseNumericLiteral();
  std::unique_ptr<ast::Literal> parseStringLiteral();
  std::unique_ptr<ast::Literal> parseBooleanLiteral();
  std::unique_ptr<ast::Literal> parseNullLiteral();
  std::unique_ptr<ast::RegExpLiteral> parseRegExpLiteral();
  std::unique_ptr<ast::Literal> parseBigIntLiteral();
  std::unique_ptr<ast::ThisExpression> parseThisExpression();
  std::unique_ptr<ast::Super> parseSuperExpression();
  std::unique_ptr<ast::Expression> parseParenthesizedExpression();
  std::unique_ptr<ast::ArrayExpression> parseArrayLiteral();
  std::unique_ptr<ast::ObjectExpression> parseObjectLiteral();
  std::unique_ptr<ast::FunctionExpression> parseFunctionExpression();
  std::unique_ptr<ast::ArrowFunctionExpression> parseArrowFunctionExpression();
  std::unique_ptr<ast::ClassExpression> parseClassExpression();
  std::unique_ptr<ast::TemplateLiteral> parseTemplateLiteral();
  std::unique_ptr<ast::TaggedTemplateExpression> parseTaggedTemplateExpression(std::unique_ptr<ast::Expression> tag);
  std::unique_ptr<ast::UpdateExpression> parseUpdateExpression(std::unique_ptr<ast::Expression> argument, bool prefix);
  std::unique_ptr<ast::UnaryExpression> parseUnaryExpression();
  std::unique_ptr<ast::BinaryExpression> parseBinaryExpression(std::unique_ptr<ast::Expression> left, Precedence precedence);
  std::unique_ptr<ast::AssignmentExpression> parseAssignmentExpression(std::unique_ptr<ast::Expression> left);
  std::unique_ptr<ast::LogicalExpression> parseLogicalExpression(std::unique_ptr<ast::Expression> left);
  std::unique_ptr<ast::ConditionalExpression> parseConditionalExpression(std::unique_ptr<ast::Expression> test);
  std::unique_ptr<ast::NewExpression> parseNewExpression();
  std::unique_ptr<ast::CallExpression> parseCallExpression(std::unique_ptr<ast::Expression> callee);
  std::unique_ptr<ast::MemberExpression> parseMemberExpression(std::unique_ptr<ast::Expression> object);
  std::unique_ptr<ast::ChainExpression> parseChainExpression(std::unique_ptr<ast::Expression> expression);
  std::unique_ptr<ast::Expression> parseLeftHandSideExpression(std::unique_ptr<ast::Expression> expr);
  std::unique_ptr<ast::YieldExpression> parseYieldExpression();
  std::unique_ptr<ast::AwaitExpression> parseAwaitExpression();
  std::unique_ptr<ast::SpreadElement> parseSpreadElement();
  std::unique_ptr<ast::Expression> parseJsxElement();

  // パラメータとパターンのパース
  std::unique_ptr<ast::Pattern> parsePattern();
  std::unique_ptr<ast::ObjectPattern> parseObjectPattern();
  std::unique_ptr<ast::ArrayPattern> parseArrayPattern();
  std::unique_ptr<ast::AssignmentPattern> parseAssignmentPattern(std::unique_ptr<ast::Pattern> left);
  std::unique_ptr<ast::RestElement> parseRestElement();
  std::vector<std::unique_ptr<ast::Parameter>> parseFormalParameters();

  // クラス関連
  std::unique_ptr<ast::ClassBody> parseClassBody();
  std::unique_ptr<ast::MethodDefinition> parseMethodDefinition();
  std::unique_ptr<ast::PropertyDefinition> parsePropertyDefinition();

  // オブジェクトリテラル関連
  std::unique_ptr<ast::Property> parseProperty();
  std::unique_ptr<ast::PrivateIdentifier> parsePrivateIdentifier();

  // モジュール関連
  std::unique_ptr<ast::ImportSpecifier> parseImportSpecifier();
  std::unique_ptr<ast::ExportSpecifier> parseExportSpecifier();

  // ヘルパーメソッド
  void initScanner(const std::string& source, const std::string& filename);
  void initParseState();
  void initParseRules();
  void advance();
  bool match(TokenType type);
  bool check(TokenType type) const;
  Token consume(TokenType type, const std::string& message);
  bool isAtEnd() const;

  // エラー処理
  void error(const std::string& message);
  void errorAtCurrent(const std::string& message);
  void errorAt(const Token& token, const std::string& message);
  void synchronize();

  // コンテキスト管理
  bool isStrictMode() const;
  void setStrictMode(bool strict);
  bool isModuleMode() const;
  bool isAsyncContext() const;
  bool isGeneratorContext() const;
  void enterContext(const std::string& context_type, std::unordered_map<std::string, bool> flags = {});
  void exitContext();
  bool isInContext(const std::string& context_type) const;

  // ユーティリティ
  bool isAssignmentOperator(TokenType type) const;
  bool isDeclaration() const;
  Precedence getTokenPrecedence(TokenType type) const;
  bool isValidSimpleAssignmentTarget(const ast::Expression* expr) const;

  // スコープ管理 (変数の重複宣言チェックなど)
  void enterScope();
  void exitScope();
  bool declareVariable(const std::string& name);
  bool checkVariable(const std::string& name) const;

  // メンバー変数
  ParserOptions m_options;                                          ///< パーサーオプション
  std::unique_ptr<Scanner> m_scanner;                               ///< スキャナー
  Token m_current_token;                                            ///< 現在のトークン
  Token m_previous_token;                                           ///< 前のトークン
  bool m_had_error;                                                 ///< エラーフラグ
  bool m_panic_mode;                                                ///< パニックモードフラグ
  std::vector<ParserError> m_errors;                                ///< パースエラーリスト
  std::stack<ParserContext> m_context_stack;                        ///< コンテキストスタック
  std::stack<std::unordered_map<std::string, bool>> m_scope_stack;  ///< スコープスタック
  ParserStats m_stats;                                              ///< パース統計

  // メモリ管理
  std::unique_ptr<ArenaAllocator> m_arena_allocator;
  std::unique_ptr<ObjectPool<ast::Node>> m_ast_pool;
  std::unique_ptr<MemoryMonitor> m_memory_monitor;

  // SIMD サポート
  SIMDSupport m_simd_support;
  std::unique_ptr<TokenProcessor> m_token_processor;

  // トークン処理
  std::unique_ptr<TokenCache> m_token_cache;
  std::vector<Token> m_lookahead_cache;

  // 並列処理
  std::unique_ptr<ThreadPool> m_thread_pool;
  std::atomic<size_t> m_recursion_depth{0};

  // メモリプール
  std::pmr::monotonic_buffer_resource m_memory_resource;
  std::pmr::polymorphic_allocator<std::byte> m_allocator;

  // パフォーマンス最適化
  alignas(64) std::array<uint8_t, 256> m_token_lookup_table;
  alignas(32) std::array<Precedence, static_cast<size_t>(TokenType::Count)> m_precedence_table;

  // キャッシュライン最適化
  alignas(64) struct {
    std::atomic<size_t> token_count{0};
    std::atomic<size_t> error_count{0};
    std::atomic<size_t> node_count{0};
  } m_counters;

  // メソッド
  void initializeMemoryManager();
  void initializeSIMD();
  void initializeThreadPool();
  void updateParseStats(const ast::Program* program);
  size_t countASTNodes(const ast::Node* node) const;

  // SIMD最適化メソッド
  void processTokensBatch(std::span<Token> tokens);
  void optimizeTokenLookup();
  void prefetchTokens(size_t count);

  // メモリ管理メソッド
  void* allocateNode(size_t size);
  void deallocateNode(void* ptr, size_t size);
  void recycleNodes();

  // スレッド安全なメソッド
  void incrementTokenCount() noexcept;
  void incrementErrorCount() noexcept;
  void incrementNodeCount() noexcept;

  // キャッシュ最適化
  void prefetchNodeData(const ast::Node* node);
  void optimizeMemoryLayout();

  // エラー処理
  void handleError(const ParserError& error) noexcept;
  void recoverFromError();

  // パフォーマンスモニタリング
  void updateMetrics(const std::string& metric_name, double value);
  void reportPerformanceMetrics();

  // 既存のプライベートメソッド...
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_PARSER_PARSER_H_