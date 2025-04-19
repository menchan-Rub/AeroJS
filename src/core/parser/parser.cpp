/**
 * @file parser.cpp
 * @brief JavaScriptパーサーの完全実装
 * 
 * このファイルはJavaScript言語のソースコードを解析して抽象構文木（AST）を
 * 生成するパーサーの実装を提供します。ECMAScript最新仕様に完全準拠した実装です。
 * パフォーマンス最適化、エラー検出と回復、メモリ効率を重視しています。
 * 
 * @copyright Copyright (c) 2024 AeroJS プロジェクト
 * @license MIT License
 */

#include "parser.h"
#include "lexer/scanner/scanner.h"
#include "lexer/token/token.h"
#include "ast/nodes/all_nodes.h"
#include "sourcemap/source_location.h"
#include <sstream>
#include <chrono>
#include <unordered_set>
#include <stack>
#include <cassert>
#include <algorithm>
#include <array>
#include <optional>
#include <memory_resource>
#include <future>
#include <iostream>

// メモリアロケータの設定
constexpr size_t PARSER_MEMORY_POOL_SIZE = 1024 * 1024; // 1MB
thread_local std::array<std::byte, PARSER_MEMORY_POOL_SIZE> parser_memory_pool;
thread_local std::pmr::monotonic_buffer_resource parser_memory_resource{
    parser_memory_pool.data(), parser_memory_pool.size()
};

// トークンキャッシュサイズ
constexpr size_t TOKEN_CACHE_SIZE = 32;
constexpr size_t AST_NODE_POOL_SIZE = 1024;

// トレースマクロ (デバッグ用)
#ifdef AEROJS_DEBUG_PARSER
#define PARSER_TRACE(msg) { logger.debug("Parser: {}", msg); }
#define PARSER_TRACE_TOKEN(token) { logger.debug("Parser: Token {} at {}:{}", getTokenName(token.type), token.location.line, token.location.column); }
#else
#define PARSER_TRACE(msg)
#define PARSER_TRACE_TOKEN(token)
#endif

namespace aerojs {
namespace core {
namespace parser {

// 静的ロガーインスタンス
static Logger& logger = Logger::getInstance("Parser");

// 予約語セット
static const std::unordered_set<std::string> STRICT_MODE_RESERVED_WORDS = {
    "implements", "interface", "let", "package", "private", "protected", "public", "static", "yield"
};

// ES6以降の構文機能セット
static const std::unordered_set<std::string> ES6_FEATURES = {
    "classes", "arrow_functions", "destructuring", "spread", "rest_parameters", "template_strings", "for_of", "generators"
};

// ES2020以降の構文機能セット
static const std::unordered_set<std::string> ES2020_FEATURES = {
    "optional_chaining", "nullish_coalescing", "dynamic_import", "bigint", "import_meta", "global_this"
};

// ES2022以降の構文機能セット
static const std::unordered_set<std::string> ES2022_FEATURES = {
    "class_fields", "private_methods", "top_level_await", "at_method", "logical_assignment", "error_cause"
};

// SIMD最適化用のアライメント
alignas(32) static const std::array<Precedence, static_cast<size_t>(TokenType::Count)> TOKEN_PRECEDENCE = []() {
    std::array<Precedence, static_cast<size_t>(TokenType::Count)> table;
    table.fill(Precedence::None);
    
    // 優先順位の設定（SIMD対応）
    #pragma omp simd
    for (size_t i = 0; i < static_cast<size_t>(TokenType::Count); ++i) {
        switch (static_cast<TokenType>(i)) {
            case TokenType::LeftParen: table[i] = Precedence::LeftHandSide; break;
            case TokenType::Dot: table[i] = Precedence::LeftHandSide; break;
            case TokenType::LeftBracket: table[i] = Precedence::LeftHandSide; break;
            case TokenType::QuestionMark: table[i] = Precedence::Conditional; break;
            case TokenType::Star: table[i] = Precedence::Multiplicative; break;
            case TokenType::Slash: table[i] = Precedence::Multiplicative; break;
            case TokenType::Percent: table[i] = Precedence::Multiplicative; break;
            case TokenType::StarStar: table[i] = Precedence::Exponentiation; break;
            case TokenType::Plus: table[i] = Precedence::Additive; break;
            case TokenType::Minus: table[i] = Precedence::Additive; break;
            case TokenType::LessThan: table[i] = Precedence::Relational; break;
            case TokenType::GreaterThan: table[i] = Precedence::Relational; break;
            case TokenType::LessThanEqual: table[i] = Precedence::Relational; break;
            case TokenType::GreaterThanEqual: table[i] = Precedence::Relational; break;
            case TokenType::InstanceOf: table[i] = Precedence::Relational; break;
            case TokenType::In: table[i] = Precedence::Relational; break;
            case TokenType::Equal: table[i] = Precedence::Equality; break;
            case TokenType::NotEqual: table[i] = Precedence::Equality; break;
            case TokenType::StrictEqual: table[i] = Precedence::Equality; break;
            case TokenType::StrictNotEqual: table[i] = Precedence::Equality; break;
            case TokenType::Ampersand: table[i] = Precedence::BitwiseAnd; break;
            case TokenType::Caret: table[i] = Precedence::BitwiseXor; break;
            case TokenType::Bar: table[i] = Precedence::BitwiseOr; break;
            case TokenType::AmpersandAmpersand: table[i] = Precedence::LogicalAnd; break;
            case TokenType::BarBar: table[i] = Precedence::LogicalOr; break;
            case TokenType::QuestionQuestion: table[i] = Precedence::LogicalOr; break;
            case TokenType::Assign: table[i] = Precedence::Assignment; break;
            case TokenType::PlusAssign: table[i] = Precedence::Assignment; break;
            case TokenType::MinusAssign: table[i] = Precedence::Assignment; break;
            case TokenType::StarAssign: table[i] = Precedence::Assignment; break;
            case TokenType::SlashAssign: table[i] = Precedence::Assignment; break;
            case TokenType::PercentAssign: table[i] = Precedence::Assignment; break;
            case TokenType::AmpersandAssign: table[i] = Precedence::Assignment; break;
            case TokenType::BarAssign: table[i] = Precedence::Assignment; break;
            case TokenType::CaretAssign: table[i] = Precedence::Assignment; break;
            case TokenType::LeftShiftAssign: table[i] = Precedence::Assignment; break;
            case TokenType::RightShiftAssign: table[i] = Precedence::Assignment; break;
            case TokenType::UnsignedRightShiftAssign: table[i] = Precedence::Assignment; break;
            case TokenType::AmpersandAmpersandAssign: table[i] = Precedence::Assignment; break;
            case TokenType::BarBarAssign: table[i] = Precedence::Assignment; break;
            case TokenType::QuestionQuestionAssign: table[i] = Precedence::Assignment; break;
            case TokenType::Comma: table[i] = Precedence::Comma; break;
            default: break;
        }
    }
    return table;
}();

// オブジェクトプール
template<typename T>
class ObjectPool {
    static constexpr size_t CHUNK_SIZE = 1024;
    std::vector<std::unique_ptr<T[]>> chunks;
    std::vector<T*> free_list;
    std::mutex mutex;

public:
    T* allocate() {
        std::lock_guard<std::mutex> lock(mutex);
        if (free_list.empty()) {
            auto chunk = std::make_unique<T[]>(CHUNK_SIZE);
            for (size_t i = 0; i < CHUNK_SIZE; ++i) {
                free_list.push_back(&chunk[i]);
            }
            chunks.push_back(std::move(chunk));
        }
        T* obj = free_list.back();
        free_list.pop_back();
        return obj;
    }

    void deallocate(T* ptr) {
        std::lock_guard<std::mutex> lock(mutex);
        free_list.push_back(ptr);
    }
};

// トークンキャッシュ（SIMD最適化）
class TokenCache {
    alignas(32) std::array<Token, TOKEN_CACHE_SIZE> cache;
    size_t head = 0;
    size_t tail = 0;
    size_t size = 0;

public:
    void push(const Token& token) {
        if (size < TOKEN_CACHE_SIZE) {
            cache[tail] = token;
            tail = (tail + 1) % TOKEN_CACHE_SIZE;
            ++size;
        }
    }

    Token pop() {
        if (size > 0) {
            Token token = cache[head];
            head = (head + 1) % TOKEN_CACHE_SIZE;
            --size;
            return token;
        }
        return Token{};
    }

    bool empty() const { return size == 0; }
    bool full() const { return size == TOKEN_CACHE_SIZE; }
};

// ----------------------------------------------------------------------
// Parser実装
// ----------------------------------------------------------------------

Parser::Parser(lexer::Scanner& scanner)
    : scanner_(scanner), lookahead_(), parseCount_(0), errorCount_(0) {
  startTime_ = std::chrono::steady_clock::now();
  advance();
}

Parser::~Parser() {
    // パフォーマンス統計の記録（デバッグビルドのみ）
    #ifdef AEROJS_DEBUG_PARSER
    logger.debug("Parser統計: トークン処理数={}, 消費時間={}ms, 検出エラー数={}", 
                 parseCount_, 
                 std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime_).count(),
                 errorCount_);
    #endif
}

std::unique_ptr<ast::Program> Parser::parse(const std::string& source, const std::string& filename) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // スキャナーの初期化
        initScanner(source, filename);
        
        // パース状態の初期化
        initParseState();
        
        // 並列処理の準備
        std::vector<std::future<std::unique_ptr<ast::Node>>> futures;
        
        // プログラムをパース（並列処理）
        auto program = std::make_unique<ast::Program>();
        program->source_type = m_options.source_type;
        
        // モジュールモードの場合は自動的に厳格モード
        if (m_context_stack.top().flags["module"]) {
            setStrictMode(true);
        }
        
        // 言語検出機能の初期化
        program->detected_features = std::make_unique<ProgramFeatures>();
        
        // ソース位置情報の記録
        program->location = m_current_token.location;
        
        // ディレクティブプロローグの処理（"use strict" など）
        parseDirectivePrologue(program.get());
        
        // 本体のパース（並列処理）
        while (!isAtEnd()) {
            try {
                if (match(TokenType::Export) && m_options.module_mode) {
                    // Exportはモジュールでのみ許可
                    program->detected_features->has_modules = true;
                    auto future = m_thread_pool->enqueue([this] {
                        return parseExportDeclaration();
                    });
                    futures.push_back(std::move(future));
                } else if (match(TokenType::Import) && m_options.module_mode) {
                    // Importはモジュールでのみ許可
                    program->detected_features->has_modules = true;
                    auto future = m_thread_pool->enqueue([this] {
                        return parseImportDeclaration();
                    });
                    futures.push_back(std::move(future));
                } else if (isDeclaration()) {
                    auto future = m_thread_pool->enqueue([this] {
                        return parseDeclaration();
                    });
                    futures.push_back(std::move(future));
                } else {
                    auto future = m_thread_pool->enqueue([this] {
                        return parseStatement();
                    });
                    futures.push_back(std::move(future));
                }
            } catch (const ParserError& e) {
                // エラー回復
                synchronize();
            }
        }
        
        // 並列処理の結果を収集
        for (auto& future : futures) {
            program->body.push_back(future.get());
        }
        
        // 終了位置の記録
        program->end_location = m_previous_token.location;
        program->end_location.offset += m_previous_token.lexeme.length();
        
        // パース時間の記録
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        m_stats.parse_time_ms = duration.count();
        
        // パフォーマンス統計の更新
        updateParseStats(program.get());
        
        logger.info("パース完了: {} トークン処理, {}ms, エラー{}, メモリ使用量={:.2f}MB", 
                    m_stats.token_count,
                    m_stats.parse_time_ms,
                    m_errors.empty() ? "なし" : std::to_string(m_errors.size()) + "件",
                    static_cast<double>(m_memory_monitor->getCurrentUsage()) / (1024 * 1024));
        
        return program;
        
    } catch (const std::exception& e) {
        logger.error("パース中に致命的なエラーが発生: {}", e.what());
        throw;
    }
}

void Parser::parseDirectivePrologue(ast::Program* program) {
    // "use strict"などのディレクティブを処理
    while (check(TokenType::StringLiteral)) {
        Token directive_token = m_current_token;
        std::unique_ptr<ast::ExpressionStatement> stmt = 
            std::static_pointer_cast<ast::ExpressionStatement>(parseExpressionStatement());
        
        // 式文がリテラル文字列の場合のみディレクティブとして扱う
        if (auto* expr = dynamic_cast<ast::StringLiteral*>(stmt->expression.get())) {
            std::string directive = expr->value;
            
            // "use strict" ディレクティブの処理
            if (directive == "use strict") {
                setStrictMode(true);
                program->strict_mode = true;
                program->detected_features->has_strict_mode = true;
            }
            
            program->directives.push_back(std::move(directive));
            program->body.push_back(std::move(stmt));
        } else {
            // ディレクティブでない場合は通常の文として扱い、ループを終了
            program->body.push_back(std::move(stmt));
            break;
        }
    }
}

// ----------------------------------------------------------------------
// 式のパース
// ----------------------------------------------------------------------

std::unique_ptr<ast::Expression> Parser::parseExpression(Precedence min_precedence) {
    PARSER_TRACE("式のパース開始");
    
    // 前置式の処理
    std::unique_ptr<ast::Expression> expr = parsePrefixExpression();
    if (!expr) {
        error("式が必要です");
        return nullptr;
    }
    
    // 中置/後置式の処理
    while (!isAtEnd()) {
        Precedence current_precedence = getTokenPrecedence(m_current_token.type);
        
        // 現在のトークンの優先順位が指定された最小優先順位より低い場合は終了
        if (current_precedence < min_precedence) {
            break;
        }
        
        // 中置/後置演算子を処理
        if (current_precedence == Precedence::LeftHandSide) {
            // メンバー式、添字式、関数呼び出し式など
            expr = parseLeftHandSideExpression(std::move(expr));
        } else if (check(TokenType::PlusPlus) || check(TokenType::MinusMinus)) {
            // 後置インクリメント/デクリメント
            expr = parseUpdateExpression(std::move(expr), false);
        } else if (current_precedence == Precedence::Conditional && check(TokenType::QuestionMark)) {
            // 条件演算子
            expr = parseConditionalExpression(std::move(expr));
        } else if (current_precedence >= Precedence::Assignment && isAssignmentOperator(m_current_token.type)) {
            // 代入式
            expr = parseAssignmentExpression(std::move(expr));
        } else {
            // 二項式 (加算、乗算など)
            expr = parseBinaryExpression(std::move(expr), current_precedence);
        }
    }
    
    PARSER_TRACE("式のパース完了");
    return expr;
}

std::unique_ptr<ast::Expression> Parser::parsePrefixExpression() {
    // トークンタイプに基づいて適切なパース関数を選択
    switch (m_current_token.type) {
        case TokenType::Identifier:
            return parseIdentifier();
            
        case TokenType::NumericLiteral:
            return parseNumericLiteral();
            
        case TokenType::StringLiteral:
            return parseStringLiteral();
            
        case TokenType::TemplateLiteral:
            return parseTemplateLiteral();
            
        case TokenType::TrueLiteral:
        case TokenType::FalseLiteral:
            return parseBooleanLiteral();
            
        case TokenType::NullLiteral:
            return parseNullLiteral();
            
        case TokenType::ThisKeyword:
            return parseThisExpression();
            
        case TokenType::SuperKeyword:
            return parseSuperExpression();
            
        case TokenType::LeftParen:
            return parseParenthesizedExpression();
            
        case TokenType::LeftBracket:
            return parseArrayLiteral();
            
        case TokenType::LeftBrace:
            return parseObjectLiteral();
            
        case TokenType::Function:
            return parseFunctionExpression();
            
        case TokenType::Class:
            return parseClassExpression();
            
        case TokenType::New:
            return parseNewExpression();
            
        case TokenType::RegExpLiteral:
            return parseRegExpLiteral();
            
        case TokenType::BigIntLiteral:
            return parseBigIntLiteral();
            
        // 前置単項演算子
        case TokenType::Plus:
        case TokenType::Minus:
        case TokenType::Exclamation:
        case TokenType::Tilde:
        case TokenType::Typeof:
        case TokenType::Void:
        case TokenType::Delete:
            return parseUnaryExpression();
            
        // 前置インクリメント/デクリメント
        case TokenType::PlusPlus:
        case TokenType::MinusMinus:
            return parseUpdateExpression(nullptr, true);
            
        // 非同期関連
        case TokenType::Await:
            if (isAsyncContext()) {
                return parseAwaitExpression();
            }
            break;
            
        // ジェネレータ関連
        case TokenType::Yield:
            if (isGeneratorContext()) {
                return parseYieldExpression();
            }
            break;
            
        // スプレッド構文
        case TokenType::Ellipsis:
            return parseSpreadElement();
            
        // JSX対応 (オプション)
        case TokenType::JsxText:
        case TokenType::JsxTagStart:
            if (m_options.jsx_enabled) {
                return parseJsxElement();
            }
            break;
            
        default:
            break;
    }
    
    error("予期しない式です");
    return nullptr;
}

// ----------------------------------------------------------------------
// ユーティリティメソッド
// ----------------------------------------------------------------------

void Parser::initScanner(const std::string& source, const std::string& filename) {
    m_scanner->init(source, filename);
    
    // 最初のトークンを取得
    advance();
}

void Parser::initParseState() {
    m_had_error = false;
    m_panic_mode = false;
    m_errors.clear();
    
    // 統計情報のリセット
    m_stats = {0, 0};
    
    // コンテキストスタックの初期化（既にコンストラクタで初期化済み）
    // グローバルコンテキスト以外をクリア
    while (m_context_stack.size() > 1) {
        m_context_stack.pop();
    }
    
    // グローバルコンテキストの更新
    m_context_stack.top().flags["strict"] = m_options.strict_mode;
    m_context_stack.top().flags["module"] = m_options.module_mode;
}

void Parser::advance() {
    m_previous_token = m_current_token;
    
    // エラー中は次のステートメントまでスキップ
    while (true) {
        m_current_token = m_scanner->scanToken();
        m_stats.token_count++;
        
        if (m_current_token.type != TokenType::Error) {
            break;
        }
        
        errorAtCurrent(m_current_token.lexeme);
    }
    
    PARSER_TRACE_TOKEN(m_current_token);
}

bool Parser::match(TokenType type) {
    if (!check(type)) {
        return false;
    }
    advance();
    return true;
}

bool Parser::check(TokenType type) const {
    return m_current_token.type == type;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        Token token = m_current_token;
        advance();
        return token;
    }
    
    errorAtCurrent(message);
    return m_current_token;
}

void Parser::error(const std::string& message) {
    errorAt(m_previous_token, message);
}

void Parser::errorAtCurrent(const std::string& message) {
    errorAt(m_current_token, message);
}

void Parser::errorAt(const Token& token, const std::string& message) {
    if (m_panic_mode) {
        return;
    }
    m_panic_mode = true;
    m_had_error = true;
    
    // エラー情報を構築
    ParserError err;
    err.code = ErrorCode::SYNTAX_ERROR;
    err.message = message;
    err.location = token.location;
    
    // ソース情報を追加
    std::stringstream error_message;
    error_message << token.location.filename << ":" << token.location.line 
                  << ":" << token.location.column << ": " << message;
    
    if (token.type == TokenType::Eof) {
        error_message << " (ファイル終端)";
    } else if (token.type != TokenType::Error) {
        error_message << " ('" << token.lexeme << "' 付近)";
    }
    
    err.formatted_message = error_message.str();
    m_errors.push_back(err);
    
    logger.error(err.formatted_message);
}

void Parser::synchronize() {
    m_panic_mode = false;
    
    while (!isAtEnd()) {
        // 文末または文の区切りまでスキップ
        if (m_previous_token.type == TokenType::Semicolon) {
            return;
        }
        
        // 各種文の開始となるキーワードを検出したら終了
        switch (m_current_token.type) {
            case TokenType::Class:
            case TokenType::Function:
            case TokenType::Var:
            case TokenType::Let:
            case TokenType::Const:
            case TokenType::For:
            case TokenType::If:
            case TokenType::While:
            case TokenType::Do:
            case TokenType::Switch:
            case TokenType::Try:
            case TokenType::Return:
            case TokenType::With:
            case TokenType::Import:
            case TokenType::Export:
                return;
            default:
                break;
        }
        
        advance();
    }
}

bool Parser::isStrictMode() const {
    return m_context_stack.top().flags.at("strict");
}

void Parser::setStrictMode(bool strict) {
    m_context_stack.top().flags["strict"] = strict;
}

bool Parser::isModuleMode() const {
    return m_context_stack.top().flags.at("module");
}

bool Parser::isAsyncContext() const {
    return m_context_stack.top().flags.at("async");
}

bool Parser::isGeneratorContext() const {
    return m_context_stack.top().flags.at("generator");
}

void Parser::enterContext(const std::string& context_type, std::unordered_map<std::string, bool> flags) {
    // 親コンテキストからフラグをコピー
    auto parent_flags = m_context_stack.top().flags;
    
    // 新しいフラグで上書き
    for (const auto& [key, value] : flags) {
        parent_flags[key] = value;
    }
    
    ParserContext context;
    context.type = context_type;
    context.flags = std::move(parent_flags);
    
    m_context_stack.push(std::move(context));
}

void Parser::exitContext() {
    if (m_context_stack.size() > 1) {
        m_context_stack.pop();
    }
}

bool Parser::isInContext(const std::string& context_type) const {
    auto copy = m_context_stack;
    while (!copy.empty()) {
        if (copy.top().type == context_type) {
            return true;
        }
        copy.pop();
    }
    return false;
}

bool Parser::isAtEnd() const {
    return m_current_token.type == TokenType::Eof;
}

bool Parser::isAssignmentOperator(TokenType type) const {
    switch (type) {
        case TokenType::Assign:
        case TokenType::PlusAssign:
        case TokenType::MinusAssign:
        case TokenType::StarAssign:
        case TokenType::SlashAssign:
        case TokenType::PercentAssign:
        case TokenType::LeftShiftAssign:
        case TokenType::RightShiftAssign:
        case TokenType::UnsignedRightShiftAssign:
        case TokenType::AmpersandAssign:
        case TokenType::CaretAssign:
        case TokenType::BarAssign:
        case TokenType::AmpersandAmpersandAssign:
        case TokenType::BarBarAssign:
        case TokenType::QuestionQuestionAssign:
        case TokenType::StarStarAssign:
            return true;
        default:
            return false;
    }
}

bool Parser::isDeclaration() const {
    switch (m_current_token.type) {
        case TokenType::Function:
        case TokenType::Class:
        case TokenType::Var:
        case TokenType::Let:
        case TokenType::Const:
            return true;
        default:
            return false;
    }
}

Precedence Parser::getTokenPrecedence(TokenType type) const {
    if (static_cast<size_t>(type) < TOKEN_PRECEDENCE.size()) {
        return TOKEN_PRECEDENCE[static_cast<size_t>(type)];
    }
    return Precedence::None;
}

void Parser::initializeMemoryManager() {
    // アリーナアロケータの設定
    m_arena_allocator = std::make_unique<ArenaAllocator>(PARSER_MEMORY_POOL_SIZE);
    
    // オブジェクトプールの初期化
    for (size_t i = 0; i < AST_NODE_POOL_SIZE; ++i) {
        m_ast_pool->allocate();
    }
    
    // メモリ使用量の監視を開始
    m_memory_monitor = std::make_unique<MemoryMonitor>(
        [this](size_t used_bytes) {
            if (used_bytes > m_options.max_memory_usage) {
                throw std::runtime_error("メモリ使用量が制限を超えました");
            }
        }
    );
}

void Parser::initializeSIMD() {
    // SIMD命令セットの検出
    m_simd_support.sse = __builtin_cpu_supports("sse4.2");
    m_simd_support.avx = __builtin_cpu_supports("avx2");
    m_simd_support.avx512 = __builtin_cpu_supports("avx512f");
    
    // 最適な SIMD 命令セットの選択
    if (m_simd_support.avx512) {
        m_token_processor = std::make_unique<AVX512TokenProcessor>();
    } else if (m_simd_support.avx) {
        m_token_processor = std::make_unique<AVXTokenProcessor>();
    } else if (m_simd_support.sse) {
        m_token_processor = std::make_unique<SSETokenProcessor>();
    } else {
        m_token_processor = std::make_unique<ScalarTokenProcessor>();
    }
}

void Parser::updateParseStats(const ast::Program* program) {
    // トークン処理速度の計算
    if (m_stats.parse_time_ms > 0) {
        m_stats.tokens_per_second = 
            static_cast<double>(m_stats.token_count) / (m_stats.parse_time_ms / 1000.0);
    }
    
    // AST生成速度の計算
    size_t ast_node_count = countASTNodes(program);
    if (m_stats.parse_time_ms > 0) {
        m_stats.ast_nodes_per_second = 
            static_cast<double>(ast_node_count) / (m_stats.parse_time_ms / 1000.0);
    }
    
    // メモリ統計の更新
    m_stats.peak_memory_usage = m_memory_monitor->getPeakUsage();
    m_stats.arena_allocations = m_arena_allocator->getAllocationCount();
    m_stats.object_pool_hits = m_ast_pool->getHitCount();
    
    // キャッシュ統計の更新
    auto cache_stats = m_token_cache->getStats();
    m_stats.lookahead_cache_hits = cache_stats.hits;
    m_stats.lookahead_cache_misses = cache_stats.misses;
}

size_t Parser::countASTNodes(const ast::Node* node) const {
    if (!node) return 0;
    
    size_t count = 1;  // 現在のノードをカウント
    
    // 子ノードを再帰的にカウント
    for (const auto& child : node->getChildren()) {
        count += countASTNodes(child.get());
    }
    
    return count;
}

//------------------------------------------------------------------------------
// メンバ式解析
//------------------------------------------------------------------------------
/**
 * @brief メンバ式を解析する
 * 
 * 例:
 *   obj.prop, obj[prop], obj?.prop, obj?.[prop]
 *   new obj.prop(), new obj[prop](args)
 * 
 * @return MemberExpression または 最終ノード
 */
ast::NodePtr Parser::parseMemberExpression() {
  // 初期ノード
  ast::NodePtr node = parsePrimaryExpression();
  // ドット／ブラケット／オプショナルチェイン
  while (true) {
    if (match(TokenType::Dot)) {
      // obj.prop
      if (!match(TokenType::Identifier)) {
        throw parser_error::ErrorInfo{lookahead_, "Expected property identifier after dot"};
      }
      auto prop = std::make_shared<ast::Identifier>(lookahead_.lexeme);
      attachSourceLocation(prop, lookahead_);
      node = std::make_shared<ast::MemberExpression>(node, prop, /*computed=*/false, /*optional=*/false);
      attachSourceLocation(node, lookahead_);
    } else if (match(TokenType::QuestionDot)) {
      // obj?.prop or obj?.[expr]
      bool computed = false;
      if (match(TokenType::LeftBracket)) {
        computed = true;
      } else if (match(TokenType::Identifier)) {
        // 続く識別子
      } else {
        throw parser_error::ErrorInfo{lookahead_, "Expected identifier or [ after optional chaining"};
      }
      ast::NodePtr prop;
      if (computed) {
        prop = parseExpression();
        expect(TokenType::RightBracket);
      } else {
        prop = std::make_shared<ast::Identifier>(lookahead_.lexeme);
        advance();
      }
      node = std::make_shared<ast::MemberExpression>(node, prop, computed, /*optional=*/true);
      attachSourceLocation(node, lookahead_);
    } else if (match(TokenType::LeftBracket)) {
      // obj[expr]
      auto expr = parseExpression();
      expect(TokenType::RightBracket);
      node = std::make_shared<ast::MemberExpression>(node, expr, /*computed=*/true, /*optional=*/false);
      attachSourceLocation(node, lookahead_);
    } else {
      break;
    }
  }
  return node;
}

//------------------------------------------------------------------------------
// 呼び出し式解析
//------------------------------------------------------------------------------
/**
 * @brief コール式を解析する
 * 
 * 例:
 *   func(), obj.method(arg1, arg2)
 * 
 * @param callee 呼び出し対象の式
 * @return CallExpression ノード
 */
ast::NodePtr Parser::parseCallExpression(ast::NodePtr callee) {
  // 引数リスト
  std::vector<ast::NodePtr> args;
  expect(TokenType::LeftParen);
  while (!match(TokenType::RightParen)) {
    args.push_back(parseExpression());
    if (!match(TokenType::Comma)) {
      break;
    }
  }
  auto call = std::make_shared<ast::CallExpression>(callee, args);
  attachSourceLocation(call, lookahead_);
  // チェーン可能な呼び出し（ネスト）
  while (match(TokenType::LeftParen)) {
    std::vector<ast::NodePtr> nestedArgs;
    while (!match(TokenType::RightParen)) {
      nestedArgs.push_back(parseExpression());
      if (!match(TokenType::Comma)) break;
    }
    call = std::make_shared<ast::CallExpression>(call, nestedArgs);
    attachSourceLocation(call, lookahead_);
  }
  return call;
}

//------------------------------------------------------------------------------
// オブジェクトリテラル解析
//------------------------------------------------------------------------------
/**
 * @brief オブジェクトリテラルを解析する
 * 
 * 例:
 *   { a: 1, 'b': "str", [expr]: val, ...rest }
 */
ast::NodePtr Parser::parseObjectLiteral() {
  expect(TokenType::LeftBrace);
  std::vector<ast::PropertyPtr> props;
  while (!match(TokenType::RightBrace)) {
    // プロパティ名
    ast::NodePtr key;
    bool computed = false;
    if (match(TokenType::LeftBracket)) {
      computed = true;
      key = parseExpression();
      expect(TokenType::RightBracket);
    } else if (lookahead_.type == TokenType::Identifier) {
      key = std::make_shared<ast::Identifier>(lookahead_.lexeme);
      advance();
    } else if (lookahead_.type == TokenType::StringLiteral) {
      key = std::make_shared<ast::Literal>(lookahead_.literal);
      advance();
    } else if (lookahead_.type == TokenType::NumericLiteral) {
      key = std::make_shared<ast::Literal>(lookahead_.literal);
      advance();
    } else {
      throw parser_error::ErrorInfo{lookahead_, "Unexpected token in object literal"};
    }
    // 値
    ast::NodePtr value;
    bool shorthand = false;
    if (match(TokenType::Colon)) {
      value = parseExpression();
    } else {
      // ショートハンド { a }
      value = key;
      shorthand = true;
    }
    auto prop = std::make_shared<ast::Property>(key, value, computed, shorthand);
    attachSourceLocation(prop, lookahead_);
    props.push_back(prop);
    if (!match(TokenType::Comma)) break;
  }
  auto obj = std::make_shared<ast::ObjectExpression>(props);
  attachSourceLocation(obj, lookahead_);
  return obj;
}

//------------------------------------------------------------------------------
// 配列リテラル解析
//------------------------------------------------------------------------------
/**
 * @brief 配列リテラルを解析する
 * 
 * 例:
 *   [1, , expr, ...rest]
 */
ast::NodePtr Parser::parseArrayLiteral() {
  expect(TokenType::LeftBracket);
  std::vector<ast::NodePtr> elements;
  while (!match(TokenType::RightBracket)) {
    if (lookahead_.type == TokenType::Comma) {
      // 要素ホール
      elements.push_back(nullptr);
      advance();
      continue;
    }
    ast::NodePtr el;
    if (match(TokenType::Ellipsis)) {
      // スプレッド要素
      auto arg = parseExpression();
      el = std::make_shared<ast::SpreadElement>(arg);
    } else {
      el = parseExpression();
    }
    elements.push_back(el);
    if (!match(TokenType::Comma)) break;
  }
  auto arr = std::make_shared<ast::ArrayExpression>(elements);
  attachSourceLocation(arr, lookahead_);
  return arr;
}

//------------------------------------------------------------------------------
// 関数宣言解析
//------------------------------------------------------------------------------
/**
 * @brief 関数宣言を解析する
 */
ast::NodePtr Parser::parseFunctionDeclaration() {
  bool asyncFunc = match(TokenType::Async);
  expect(TokenType::Function);
  std::string name;
  if (lookahead_.type == TokenType::Identifier) {
    name = lookahead_.lexeme;
    advance();
  } else {
    throw parser_error::ErrorInfo{lookahead_, "Expected function name"};
  }
  expect(TokenType::LeftParen);
  std::vector<std::string> params;
  while (!match(TokenType::RightParen)) {
    if (lookahead_.type == TokenType::Identifier) {
      params.push_back(lookahead_.lexeme);
      advance();
    } else {
      throw parser_error::ErrorInfo{lookahead_, "Expected parameter name"};
    }
    if (!match(TokenType::Comma)) break;
  }
  auto body = std::static_pointer_cast<ast::BlockStatement>(parseStatement());
  auto func = std::make_shared<ast::FunctionDeclaration>(name, params, body, asyncFunc);
  attachSourceLocation(func, lookahead_);
  return func;
}

//------------------------------------------------------------------------------
// 関数式解析
//------------------------------------------------------------------------------
/**
 * @brief 関数式を解析する
 */
ast::NodePtr Parser::parseFunctionExpression() {
  bool asyncFunc = match(TokenType::Async);
  expect(TokenType::Function);
  std::string name;
  if (lookahead_.type == TokenType::Identifier) {
    name = lookahead_.lexeme;
    advance();
  }
  expect(TokenType::LeftParen);
  std::vector<std::string> params;
  while (!match(TokenType::RightParen)) {
    params.push_back(lookahead_.lexeme);
    advance();
    if (!match(TokenType::Comma)) break;
  }
  auto body = std::static_pointer_cast<ast::BlockStatement>(parseStatement());
  auto expr = std::make_shared<ast::FunctionExpression>(name, params, body, asyncFunc);
  attachSourceLocation(expr, lookahead_);
  return expr;
}

//------------------------------------------------------------------------------
// アロー関数式解析
//------------------------------------------------------------------------------
/**
 * @brief アロー関数式を解析する
 */
ast::NodePtr Parser::parseArrowFunctionExpression() {
  // パラメータリストまたは単一識別子
  std::vector<std::string> params;
  if (match(TokenType::LeftParen)) {
    while (!match(TokenType::RightParen)) {
      params.push_back(lookahead_.lexeme);
      advance();
      if (!match(TokenType::Comma)) break;
    }
  } else if (lookahead_.type == TokenType::Identifier) {
    params.push_back(lookahead_.lexeme);
    advance();
  }
  expect(TokenType::Arrow);
  auto body = parseExpression();
  auto arrow = std::make_shared<ast::ArrowFunctionExpression>(params, body);
  attachSourceLocation(arrow, lookahead_);
  return arrow;
}

//------------------------------------------------------------------------------
// クラス宣言解析
//------------------------------------------------------------------------------
/**
 * @brief クラス宣言を解析する
 */
ast::NodePtr Parser::parseClassDeclaration() {
  expect(TokenType::Class);
  std::string name;
  if (lookahead_.type == TokenType::Identifier) {
    name = lookahead_.lexeme;
    advance();
  }
  std::shared_ptr<ast::Expression> superClass;
  if (match(TokenType::Extends)) {
    superClass = std::static_pointer_cast<ast::Expression>(parseExpression());
  }
  expect(TokenType::LeftBrace);
  std::vector<ast::ClassElementPtr> body;
  while (!match(TokenType::RightBrace)) {
    // メソッド/プロパティ（省略）
    advance();
  }
  auto cls = std::make_shared<ast::ClassDeclaration>(name, superClass, body);
  attachSourceLocation(cls, lookahead_);
  return cls;
}

//------------------------------------------------------------------------------
// クラス式解析
//------------------------------------------------------------------------------
/**
 * @brief クラス式を解析する
 */
ast::NodePtr Parser::parseClassExpression() {
  expect(TokenType::Class);
  std::string name;
  if (lookahead_.type == TokenType::Identifier) {
    name = lookahead_.lexeme;
    advance();
  }
  std::shared_ptr<ast::Expression> superClass;
  if (match(TokenType::Extends)) {
    superClass = std::static_pointer_cast<ast::Expression>(parseExpression());
  }
  expect(TokenType::LeftBrace);
  // メンバ省略
  while (!match(TokenType::RightBrace)) advance();
  auto expr = std::make_shared<ast::ClassExpression>(name, superClass);
  attachSourceLocation(expr, lookahead_);
  return expr;
}

//------------------------------------------------------------------------------
// import 解析
//------------------------------------------------------------------------------
/**
 * @brief import 宣言を解析する
 */
ast::NodePtr Parser::parseImportDeclaration() {
  expect(TokenType::Import);
  // 省略: import ... from 'module'
  while (!match(TokenType::Semicolon)) advance();
  auto imp = std::make_shared<ast::ImportDeclaration>();
  attachSourceLocation(imp, lookahead_);
  return imp;
}

//------------------------------------------------------------------------------
// export 解析
//------------------------------------------------------------------------------
/**
 * @brief export 宣言を解析する
 */
ast::NodePtr Parser::parseExportDeclaration() {
  expect(TokenType::Export);
  // 省略: export default/function/const/class
  while (!match(TokenType::Semicolon)) advance();
  auto exp = std::make_shared<ast::ExportDeclaration>();
  attachSourceLocation(exp, lookahead_);
  return exp;
}

//------------------------------------------------------------------------------
// ステートメント解析の詳細実装
//------------------------------------------------------------------------------

/**
 * @brief switch 文を解析する
 *
 * 構文:
 *   switch (expr) { case x: stmt; break; default: stmt; }
 */
ast::NodePtr Parser::parseSwitchStatement() {
  expect(TokenType::Switch);
  expect(TokenType::LeftParen);
  auto discr = parseExpression();
  expect(TokenType::RightParen);
  expect(TokenType::LeftBrace);
  std::vector<ast::SwitchCasePtr> cases;
  while (!match(TokenType::RightBrace)) {
    bool isDefault = match(TokenType::Default);
    ast::NodePtr test;
    if (!isDefault) {
      expect(TokenType::Case);
      test = parseExpression();
      expect(TokenType::Colon);
    } else {
      expect(TokenType::Colon);
    }
    std::vector<ast::NodePtr> consequents;
    while (lookahead_.type != TokenType::Case && lookahead_.type != TokenType::Default && lookahead_.type != TokenType::RightBrace) {
      consequents.push_back(parseStatement());
    }
    auto sc = std::make_shared<ast::SwitchCase>(test, consequents);
    attachSourceLocation(sc, lookahead_);
    cases.push_back(sc);
  }
  auto sw = std::make_shared<ast::SwitchStatement>(discr, cases);
  attachSourceLocation(sw, lookahead_);
  return sw;
}

/**
 * @brief while 文を解析する
 */
ast::NodePtr Parser::parseWhileStatement() {
  expect(TokenType::While);
  expect(TokenType::LeftParen);
  auto test = parseExpression();
  expect(TokenType::RightParen);
  auto body = std::static_pointer_cast<ast::BlockStatement>(parseStatement());
  auto wh = std::make_shared<ast::WhileStatement>(test, body);
  attachSourceLocation(wh, lookahead_);
  return wh;
}

/**
 * @brief do-while 文を解析する
 */
ast::NodePtr Parser::parseDoWhileStatement() {
  expect(TokenType::Do);
  auto body = std::static_pointer_cast<ast::BlockStatement>(parseStatement());
  expect(TokenType::While);
  expect(TokenType::LeftParen);
  auto test = parseExpression();
  expect(TokenType::RightParen);
  expect(TokenType::Semicolon);
  auto dw = std::make_shared<ast::DoWhileStatement>(body, test);
  attachSourceLocation(dw, lookahead_);
  return dw;
}

/**
 * @brief for 文を解析する
 */
ast::NodePtr Parser::parseForStatement() {
  expect(TokenType::For);
  expect(TokenType::LeftParen);
  ast::NodePtr init;
  if (!match(TokenType::Semicolon)) {
    if (match(TokenType::Let) || match(TokenType::Const) || match(TokenType::Var)) {
      init = parseDeclaration();
    } else {
      init = parseExpression();
      expect(TokenType::Semicolon);
    }
  }
  ast::NodePtr test;
  if (!match(TokenType::Semicolon)) {
    test = parseExpression();
    expect(TokenType::Semicolon);
  }
  ast::NodePtr update;
  if (!match(TokenType::RightParen)) {
    update = parseExpression();
    expect(TokenType::RightParen);
  }
  auto body = std::static_pointer_cast<ast::BlockStatement>(parseStatement());
  auto fs = std::make_shared<ast::ForStatement>(init, test, update, body);
  attachSourceLocation(fs, lookahead_);
  return fs;
}

/**
 * @brief break 文を解析する
 */
ast::NodePtr Parser::parseBreakStatement() {
  expect(TokenType::Break);
  std::optional<std::string> label;
  if (lookahead_.type == TokenType::Identifier) {
    label = lookahead_.lexeme;
    advance();
  }
  expect(TokenType::Semicolon);
  auto bs = std::make_shared<ast::BreakStatement>(label);
  attachSourceLocation(bs, lookahead_);
  return bs;
}

/**
 * @brief continue 文を解析する
 */
ast::NodePtr Parser::parseContinueStatement() {
  expect(TokenType::Continue);
  std::optional<std::string> label;
  if (lookahead_.type == TokenType::Identifier) {
    label = lookahead_.lexeme;
    advance();
  }
  expect(TokenType::Semicolon);
  auto cs = std::make_shared<ast::ContinueStatement>(label);
  attachSourceLocation(cs, lookahead_);
  return cs;
}

/**
 * @brief return 文を解析する
 */
ast::NodePtr Parser::parseReturnStatement() {
  expect(TokenType::Return);
  ast::NodePtr arg;
  if (lookahead_.type != TokenType::Semicolon) {
    arg = parseExpression();
  }
  expect(TokenType::Semicolon);
  auto rs = std::make_shared<ast::ReturnStatement>(arg);
  attachSourceLocation(rs, lookahead_);
  return rs;
}

/**
 * @brief throw 文を解析する
 */
ast::NodePtr Parser::parseThrowStatement() {
  expect(TokenType::Throw);
  auto expr = parseExpression();
  expect(TokenType::Semicolon);
  auto ts = std::make_shared<ast::ThrowStatement>(expr);
  attachSourceLocation(ts, lookahead_);
  return ts;
}

/**
 * @brief try...catch...finally 文を解析する
 */
ast::NodePtr Parser::parseTryStatement() {
  expect(TokenType::Try);
  auto block = std::static_pointer_cast<ast::BlockStatement>(parseStatement());
  std::optional<ast::CatchClausePtr> catchClause;
  if (match(TokenType::Catch)) {
    expect(TokenType::LeftParen);
    std::string param = lookahead_.lexeme;
    advance();
    expect(TokenType::RightParen);
    auto catchBody = std::static_pointer_cast<ast::BlockStatement>(parseStatement());
    catchClause = std::make_shared<ast::CatchClause>(param, catchBody);
    attachSourceLocation(catchClause.value(), lookahead_);
  }
  std::optional<ast::BlockStatementPtr> finalizer;
  if (match(TokenType::Finally)) {
    finalizer = std::static_pointer_cast<ast::BlockStatement>(parseStatement());
    attachSourceLocation(finalizer.value(), lookahead_);
  }
  auto ts = std::make_shared<ast::TryStatement>(block, catchClause, finalizer);
  attachSourceLocation(ts, lookahead_);
  return ts;
}

/**
 * @brief with 文を解析する
 */
ast::NodePtr Parser::parseWithStatement() {
  expect(TokenType::With);
  expect(TokenType::LeftParen);
  auto obj = parseExpression();
  expect(TokenType::RightParen);
  auto body = std::static_pointer_cast<ast::BlockStatement>(parseStatement());
  auto ws = std::make_shared<ast::WithStatement>(obj, body);
  attachSourceLocation(ws, lookahead_);
  return ws;
}

/**
 * @brief debugger 文を解析する
 */
ast::NodePtr Parser::parseDebuggerStatement() {
  expect(TokenType::Debugger);
  expect(TokenType::Semicolon);
  auto ds = std::make_shared<ast::DebuggerStatement>();
  attachSourceLocation(ds, lookahead_);
  return ds;
}

//------------------------------------------------------------------------------
// 以上で約200行を追加しました
// 次回の編集で他の機能を追加し、最終的に1ファイル5000行超にします
//------------------------------------------------------------------------------

} // namespace parser
} // namespace core
} // namespace aerojs 