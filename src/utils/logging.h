/**
 * @file logging.h
 * @brief AeroJS統合ロギングユーティリティ
 * 
 * このファイルは、AeroJSプロジェクト全体で使用される
 * 統一されたロギングインターフェースを提供します。
 * 
 * 特徴:
 * - 高性能な条件付きコンパイル対応マクロ
 * - カテゴリ別ロガー管理
 * - デバッグ情報付きロギング
 * - プロファイリング対応
 * - メモリ効率的な実装
 * 
 * @author AeroJS Team
 * @version 1.0.0
 * @copyright MIT License
 */

#ifndef AEROJS_UTILS_LOGGING_H
#define AEROJS_UTILS_LOGGING_H

#include "../core/utils/logger.h"
#include <chrono>
#include <string>
#include <sstream>
#include <memory>

namespace aerojs {
namespace utils {

// ロギングレベル（core::LogLevelのエイリアス）
using LogLevel = aero::LogLevel;

// デフォルトカテゴリ定数
constexpr const char* DEFAULT_CATEGORY = "aerojs";
constexpr const char* JIT_CATEGORY = "jit";
constexpr const char* PARSER_CATEGORY = "parser";
constexpr const char* RUNTIME_CATEGORY = "runtime";
constexpr const char* NETWORK_CATEGORY = "network";
constexpr const char* GC_CATEGORY = "gc";
constexpr const char* OPTIMIZER_CATEGORY = "optimizer";
constexpr const char* PROFILER_CATEGORY = "profiler";

// カテゴリ別ロガー取得関数
inline aero::Logger& GetLogger(const char* category = DEFAULT_CATEGORY) {
    return aero::Logger::getInstance(category);
}

// 特殊用途ロガー取得関数
inline aero::Logger& GetJitLogger() { return GetLogger(JIT_CATEGORY); }
inline aero::Logger& GetParserLogger() { return GetLogger(PARSER_CATEGORY); }
inline aero::Logger& GetRuntimeLogger() { return GetLogger(RUNTIME_CATEGORY); }
inline aero::Logger& GetNetworkLogger() { return GetLogger(NETWORK_CATEGORY); }
inline aero::Logger& GetGcLogger() { return GetLogger(GC_CATEGORY); }
inline aero::Logger& GetOptimizerLogger() { return GetLogger(OPTIMIZER_CATEGORY); }
inline aero::Logger& GetProfilerLogger() { return GetLogger(PROFILER_CATEGORY); }

// ログ初期化関数
void InitializeLogging(const aero::LoggerOptions& options = {});
void ShutdownLogging();

// パフォーマンス測定用クラス
class ScopedTimer {
public:
    ScopedTimer(const char* name, const char* category = DEFAULT_CATEGORY)
        : name_(name), logger_(GetLogger(category)),
          start_(std::chrono::high_resolution_clock::now()) {}
    
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        logger_.info("[TIMER] %s: %lld μs", name_, duration.count());
    }

private:
    const char* name_;
    aero::Logger& logger_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

// エラーコンテキスト情報
struct ErrorContext {
    const char* file;
    int line;
    const char* function;
    
    ErrorContext(const char* f, int l, const char* func)
        : file(f), line(l), function(func) {}
    
    std::string ToString() const {
        std::ostringstream oss;
        oss << file << ":" << line << " in " << function;
        return oss.str();
    }
};

} // namespace utils
} // namespace aerojs

// 基本ログマクロ（カテゴリ指定なし）
#define AEROJS_LOG_TRACE(format, ...) \
    AERO_LOG_TRACE(::aerojs::utils::GetLogger(), format, ##__VA_ARGS__)

#define AEROJS_LOG_DEBUG(format, ...) \
    AERO_LOG_DEBUG(::aerojs::utils::GetLogger(), format, ##__VA_ARGS__)

#define AEROJS_LOG_INFO(format, ...) \
    AERO_LOG_INFO(::aerojs::utils::GetLogger(), format, ##__VA_ARGS__)

#define AEROJS_LOG_WARNING(format, ...) \
    AERO_LOG_WARNING(::aerojs::utils::GetLogger(), format, ##__VA_ARGS__)

#define AEROJS_LOG_ERROR(format, ...) \
    AERO_LOG_ERROR(::aerojs::utils::GetLogger(), format, ##__VA_ARGS__)

#define AEROJS_LOG_CRITICAL(format, ...) \
    AERO_LOG_CRITICAL(::aerojs::utils::GetLogger(), format, ##__VA_ARGS__)

// カテゴリ指定ログマクロ
#define AEROJS_LOG_TRACE_CAT(category, format, ...) \
    AERO_LOG_TRACE(::aerojs::utils::GetLogger(category), format, ##__VA_ARGS__)

#define AEROJS_LOG_DEBUG_CAT(category, format, ...) \
    AERO_LOG_DEBUG(::aerojs::utils::GetLogger(category), format, ##__VA_ARGS__)

#define AEROJS_LOG_INFO_CAT(category, format, ...) \
    AERO_LOG_INFO(::aerojs::utils::GetLogger(category), format, ##__VA_ARGS__)

#define AEROJS_LOG_WARNING_CAT(category, format, ...) \
    AERO_LOG_WARNING(::aerojs::utils::GetLogger(category), format, ##__VA_ARGS__)

#define AEROJS_LOG_ERROR_CAT(category, format, ...) \
    AERO_LOG_ERROR(::aerojs::utils::GetLogger(category), format, ##__VA_ARGS__)

#define AEROJS_LOG_CRITICAL_CAT(category, format, ...) \
    AERO_LOG_CRITICAL(::aerojs::utils::GetLogger(category), format, ##__VA_ARGS__)

// 専用カテゴリマクロ
#define AEROJS_JIT_LOG_TRACE(format, ...) \
    AERO_LOG_TRACE(::aerojs::utils::GetJitLogger(), format, ##__VA_ARGS__)

#define AEROJS_JIT_LOG_DEBUG(format, ...) \
    AERO_LOG_DEBUG(::aerojs::utils::GetJitLogger(), format, ##__VA_ARGS__)

#define AEROJS_JIT_LOG_INFO(format, ...) \
    AERO_LOG_INFO(::aerojs::utils::GetJitLogger(), format, ##__VA_ARGS__)

#define AEROJS_JIT_LOG_WARNING(format, ...) \
    AERO_LOG_WARNING(::aerojs::utils::GetJitLogger(), format, ##__VA_ARGS__)

#define AEROJS_JIT_LOG_ERROR(format, ...) \
    AERO_LOG_ERROR(::aerojs::utils::GetJitLogger(), format, ##__VA_ARGS__)

#define AEROJS_PARSER_LOG_TRACE(format, ...) \
    AERO_LOG_TRACE(::aerojs::utils::GetParserLogger(), format, ##__VA_ARGS__)

#define AEROJS_PARSER_LOG_DEBUG(format, ...) \
    AERO_LOG_DEBUG(::aerojs::utils::GetParserLogger(), format, ##__VA_ARGS__)

#define AEROJS_PARSER_LOG_INFO(format, ...) \
    AERO_LOG_INFO(::aerojs::utils::GetParserLogger(), format, ##__VA_ARGS__)

#define AEROJS_RUNTIME_LOG_TRACE(format, ...) \
    AERO_LOG_TRACE(::aerojs::utils::GetRuntimeLogger(), format, ##__VA_ARGS__)

#define AEROJS_RUNTIME_LOG_DEBUG(format, ...) \
    AERO_LOG_DEBUG(::aerojs::utils::GetRuntimeLogger(), format, ##__VA_ARGS__)

#define AEROJS_RUNTIME_LOG_INFO(format, ...) \
    AERO_LOG_INFO(::aerojs::utils::GetRuntimeLogger(), format, ##__VA_ARGS__)

#define AEROJS_GC_LOG_TRACE(format, ...) \
    AERO_LOG_TRACE(::aerojs::utils::GetGcLogger(), format, ##__VA_ARGS__)

#define AEROJS_GC_LOG_DEBUG(format, ...) \
    AERO_LOG_DEBUG(::aerojs::utils::GetGcLogger(), format, ##__VA_ARGS__)

#define AEROJS_GC_LOG_INFO(format, ...) \
    AERO_LOG_INFO(::aerojs::utils::GetGcLogger(), format, ##__VA_ARGS__)

#define AEROJS_NETWORK_LOG_TRACE(format, ...) \
    AERO_LOG_TRACE(::aerojs::utils::GetNetworkLogger(), format, ##__VA_ARGS__)

#define AEROJS_NETWORK_LOG_DEBUG(format, ...) \
    AERO_LOG_DEBUG(::aerojs::utils::GetNetworkLogger(), format, ##__VA_ARGS__)

#define AEROJS_NETWORK_LOG_INFO(format, ...) \
    AERO_LOG_INFO(::aerojs::utils::GetNetworkLogger(), format, ##__VA_ARGS__)

// デバッグ情報付きログマクロ
#define AEROJS_LOG_ERROR_WITH_CONTEXT(format, ...) \
    do { \
        ::aerojs::utils::ErrorContext ctx(__FILE__, __LINE__, __FUNCTION__); \
        AERO_LOG_ERROR(::aerojs::utils::GetLogger(), "[%s] " format, ctx.ToString().c_str(), ##__VA_ARGS__); \
    } while(0)

#define AEROJS_LOG_CRITICAL_WITH_CONTEXT(format, ...) \
    do { \
        ::aerojs::utils::ErrorContext ctx(__FILE__, __LINE__, __FUNCTION__); \
        AERO_LOG_CRITICAL(::aerojs::utils::GetLogger(), "[%s] " format, ctx.ToString().c_str(), ##__VA_ARGS__); \
    } while(0)

// パフォーマンス測定マクロ
#define AEROJS_SCOPED_TIMER(name) \
    ::aerojs::utils::ScopedTimer _timer(name)

#define AEROJS_SCOPED_TIMER_CAT(name, category) \
    ::aerojs::utils::ScopedTimer _timer(name, category)

#define AEROJS_JIT_SCOPED_TIMER(name) \
    ::aerojs::utils::ScopedTimer _timer(name, ::aerojs::utils::JIT_CATEGORY)

#define AEROJS_GC_SCOPED_TIMER(name) \
    ::aerojs::utils::ScopedTimer _timer(name, ::aerojs::utils::GC_CATEGORY)

// 条件付きログマクロ（デバッグビルドのみ）
#ifdef NDEBUG
    #define AEROJS_DEBUG_ONLY_LOG(level, format, ...) do {} while(0)
#else
    #define AEROJS_DEBUG_ONLY_LOG(level, format, ...) \
        AEROJS_LOG_##level(format, ##__VA_ARGS__)
#endif

// アサーション付きログマクロ
#define AEROJS_LOG_ASSERT(condition, format, ...) \
    do { \
        if (!(condition)) { \
            AEROJS_LOG_CRITICAL_WITH_CONTEXT("Assertion failed: " #condition " - " format, ##__VA_ARGS__); \
            std::abort(); \
        } \
    } while(0)

// 戻り値チェック付きログマクロ
#define AEROJS_LOG_CHECK(expression, format, ...) \
    ({ \
        auto _result = (expression); \
        if (!_result) { \
            AEROJS_LOG_ERROR_WITH_CONTEXT("Check failed: " #expression " - " format, ##__VA_ARGS__); \
        } \
        _result; \
    })

// メモリ使用量ログマクロ
#define AEROJS_LOG_MEMORY_USAGE(message) \
    do { \
        size_t rss, vsize; \
        if (::aerojs::utils::GetMemoryUsage(rss, vsize)) { \
            AEROJS_LOG_INFO("%s - RSS: %zu KB, VSize: %zu KB", message, rss / 1024, vsize / 1024); \
        } \
    } while(0)

// 後方互換性マクロ（既存コードとの互換性維持）
#define LOG_TRACE AEROJS_LOG_TRACE
#define LOG_DEBUG AEROJS_LOG_DEBUG
#define LOG_INFO AEROJS_LOG_INFO
#define LOG_WARNING AEROJS_LOG_WARNING
#define LOG_ERROR AEROJS_LOG_ERROR
#define LOG_CRITICAL AEROJS_LOG_CRITICAL

namespace aerojs {
namespace utils {

// ユーティリティ関数宣言
bool GetMemoryUsage(size_t& rss, size_t& vsize);
std::string FormatBytes(size_t bytes);
std::string GetThreadId();
void DumpStackTrace(const char* category = DEFAULT_CATEGORY);

// ログ設定ヘルパー関数
void SetLogLevel(LogLevel level, const char* category = DEFAULT_CATEGORY);
void SetLogFile(const std::string& filename, const char* category = DEFAULT_CATEGORY);
void EnableConsoleColors(bool enable = true, const char* category = DEFAULT_CATEGORY);
void EnableAsyncLogging(bool enable = true, const char* category = DEFAULT_CATEGORY);

// グローバル設定関数
void ConfigureDefaultLogging();
void ConfigureProductionLogging();
void ConfigureDebugLogging();
void ConfigurePerformanceLogging();

} // namespace utils
} // namespace aerojs

#endif // AEROJS_UTILS_LOGGING_H 