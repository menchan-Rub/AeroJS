/**
 * @file logging.cpp
 * @brief AeroJS統合ロギングユーティリティ実装
 * 
 * @author AeroJS Team
 * @version 1.0.0
 * @copyright MIT License
 */

#include "logging.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <sstream>
#include <cstdlib>
#include <iomanip>
#include <mutex>

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
#else
    #include <unistd.h>
    #include <sys/resource.h>
    #include <execinfo.h>
#endif

namespace aerojs {
namespace utils {

// 初期化済みフラグ
static bool g_loggingInitialized = false;
static std::mutex g_initMutex;

void InitializeLogging(const aero::LoggerOptions& options) {
    std::lock_guard<std::mutex> lock(g_initMutex);
    
    if (g_loggingInitialized) {
        return;
    }
    
    // 各カテゴリのロガーを初期化
    const char* categories[] = {
        DEFAULT_CATEGORY,
        JIT_CATEGORY,
        PARSER_CATEGORY,
        RUNTIME_CATEGORY,
        NETWORK_CATEGORY,
        GC_CATEGORY,
        OPTIMIZER_CATEGORY,
        PROFILER_CATEGORY
    };
    
    for (const char* category : categories) {
        auto& logger = GetLogger(category);
        logger.setOptions(options);
    }
    
    g_loggingInitialized = true;
    
    // 初期化ログ
    AEROJS_LOG_INFO("AeroJS ロギングシステムが初期化されました");
    AEROJS_LOG_DEBUG("ログレベル: %s", aero::logLevelToString(options.level).data());
    AEROJS_LOG_DEBUG("非同期ロギング: %s", options.asyncLogging ? "有効" : "無効");
}

void ShutdownLogging() {
    std::lock_guard<std::mutex> lock(g_initMutex);
    
    if (!g_loggingInitialized) {
        return;
    }
    
    AEROJS_LOG_INFO("AeroJS ロギングシステムをシャットダウンします");
    
    // 各ロガーのリソースクリーンアップは自動的に行われる
    g_loggingInitialized = false;
}

bool GetMemoryUsage(size_t& rss, size_t& vsize) {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), 
                           reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), 
                           sizeof(pmc))) {
        rss = pmc.WorkingSetSize;
        vsize = pmc.PrivateUsage;
        return true;
    }
    return false;
#else
    std::ifstream file("/proc/self/status");
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    rss = 0;
    vsize = 0;
    
    while (std::getline(file, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line.substr(6));
            iss >> rss;
            rss *= 1024; // kB to bytes
        } else if (line.substr(0, 7) == "VmSize:") {
            std::istringstream iss(line.substr(7));
            iss >> vsize;
            vsize *= 1024; // kB to bytes
        }
    }
    
    return rss > 0 && vsize > 0;
#endif
}

std::string FormatBytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    int unitIndex = 0;
    
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    return oss.str();
}

std::string GetThreadId() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

void DumpStackTrace(const char* category) {
#ifdef _WIN32
    // Windows用スタックトレース（簡略化）
    GetLogger(category).error("スタックトレース機能はWindows環境では制限されています");
#else
    void* array[20];
    size_t size = backtrace(array, 20);
    char** strings = backtrace_symbols(array, size);
    
    if (strings) {
        auto& logger = GetLogger(category);
        logger.error("スタックトレース:");
        
        for (size_t i = 0; i < size; i++) {
            logger.error("  [%zu] %s", i, strings[i]);
        }
        
        free(strings);
    }
#endif
}

void SetLogLevel(LogLevel level, const char* category) {
    GetLogger(category).setLevel(level);
}

void SetLogFile(const std::string& filename, const char* category) {
    GetLogger(category).setLogFile(filename);
}

void EnableConsoleColors(bool enable, const char* category) {
    auto options = aero::LoggerOptions{};
    options.useColors = enable;
    GetLogger(category).setOptions(options);
}

void EnableAsyncLogging(bool enable, const char* category) {
    auto options = aero::LoggerOptions{};
    options.asyncLogging = enable;
    GetLogger(category).setOptions(options);
}

void ConfigureDefaultLogging() {
    aero::LoggerOptions options;
    options.level = LogLevel::INFO;
    options.useColors = true;
    options.showTimestamp = true;
    options.showLevel = true;
    options.showCategory = true;
    options.targets = {aero::LogTarget::CONSOLE};
    options.asyncLogging = false;
    
    InitializeLogging(options);
}

void ConfigureProductionLogging() {
    aero::LoggerOptions options;
    options.level = LogLevel::WARNING;
    options.useColors = false;
    options.showTimestamp = true;
    options.showLevel = true;
    options.showCategory = true;
    options.logFilePath = "aerojs.log";
    options.targets = {aero::LogTarget::FILE};
    options.asyncLogging = true;
    options.maxFileSizeBytes = 100 * 1024 * 1024; // 100MB
    options.maxBackupFiles = 5;
    
    InitializeLogging(options);
}

void ConfigureDebugLogging() {
    aero::LoggerOptions options;
    options.level = LogLevel::TRACE;
    options.useColors = true;
    options.showTimestamp = true;
    options.showLevel = true;
    options.showCategory = true;
    options.showSourceLocation = true;
    options.targets = {aero::LogTarget::CONSOLE, aero::LogTarget::FILE};
    options.logFilePath = "aerojs_debug.log";
    options.asyncLogging = false; // デバッグ時は同期
    
    InitializeLogging(options);
}

void ConfigurePerformanceLogging() {
    aero::LoggerOptions options;
    options.level = LogLevel::INFO;
    options.useColors = false;
    options.showTimestamp = true;
    options.showLevel = false;
    options.showCategory = true;
    options.logFilePath = "aerojs_performance.log";
    options.targets = {aero::LogTarget::FILE};
    options.asyncLogging = true;
    options.bufferSize = 16384; // パフォーマンス重視で大きなバッファ
    
    InitializeLogging(options);
}

} // namespace utils
} // namespace aerojs 