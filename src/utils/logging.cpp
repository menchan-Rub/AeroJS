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
    // 完璧なWindowsスタックトレース実装
    
#ifdef _WIN32
    // Windows環境での完璧なスタックトレース
    
    // 1. シンボル情報の初期化
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    
    if (!SymInitialize(process, NULL, TRUE)) {
        LOG_ERROR("シンボル情報の初期化に失敗: {}", GetLastError());
        return;
    }
    
    // シンボルオプションの設定
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    
    // 2. スタックフレームの取得
    CONTEXT context;
    RtlCaptureContext(&context);
    
    STACKFRAME64 stackFrame;
    ZeroMemory(&stackFrame, sizeof(STACKFRAME64));
    
#ifdef _M_X64
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_IX86
    DWORD machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = context.Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_ARM64
    DWORD machineType = IMAGE_FILE_MACHINE_ARM64;
    stackFrame.AddrPC.Offset = context.Pc;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Fp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Sp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#else
    DWORD machineType = IMAGE_FILE_MACHINE_UNKNOWN;
#endif
    
    // 3. スタックトレースの出力
    LOG_INFO("=== スタックトレース ({}) ===", category ? category : "Unknown");
    
    int frameIndex = 0;
    const int maxFrames = 64;
    
    while (frameIndex < maxFrames) {
        BOOL result = StackWalk64(
            machineType,
            process,
            thread,
            &stackFrame,
            &context,
            NULL,
            SymFunctionTableAccess64,
            SymGetModuleBase64,
            NULL
        );
        
        if (!result || stackFrame.AddrPC.Offset == 0) {
            break;
        }
        
        // 4. シンボル名の取得
        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        PSYMBOL_INFO symbol = (PSYMBOL_INFO)symbolBuffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        
        DWORD64 displacement = 0;
        std::string symbolName = "Unknown";
        
        if (SymFromAddr(process, stackFrame.AddrPC.Offset, &displacement, symbol)) {
            symbolName = symbol->Name;
        }
        
        // 5. ファイル名と行番号の取得
        IMAGEHLP_LINE64 line;
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD lineDisplacement = 0;
        
        std::string fileName = "Unknown";
        DWORD lineNumber = 0;
        
        if (SymGetLineFromAddr64(process, stackFrame.AddrPC.Offset, &lineDisplacement, &line)) {
            fileName = line.FileName;
            lineNumber = line.LineNumber;
            
            // ファイル名のパスを短縮
            size_t lastSlash = fileName.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                fileName = fileName.substr(lastSlash + 1);
            }
        }
        
        // 6. モジュール情報の取得
        IMAGEHLP_MODULE64 moduleInfo;
        moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
        
        std::string moduleName = "Unknown";
        if (SymGetModuleInfo64(process, stackFrame.AddrPC.Offset, &moduleInfo)) {
            moduleName = moduleInfo.ModuleName;
        }
        
        // 7. フレーム情報の出力
        LOG_INFO("#{:2d}: 0x{:016X} {} + 0x{:X}", 
                frameIndex, 
                stackFrame.AddrPC.Offset,
                symbolName,
                displacement);
        
        if (lineNumber > 0) {
            LOG_INFO("     at {}:{} in {}", fileName, lineNumber, moduleName);
        } else {
            LOG_INFO("     in {}", moduleName);
        }
        
        // 8. 追加のデバッグ情報
        if (frameIndex == 0) {
            // 最初のフレームでは追加情報を出力
            LOG_INFO("     Frame: 0x{:016X}, Stack: 0x{:016X}", 
                    stackFrame.AddrFrame.Offset, 
                    stackFrame.AddrStack.Offset);
        }
        
        frameIndex++;
    }
    
    // 9. クリーンアップ
    SymCleanup(process);
    
    LOG_INFO("=== スタックトレース終了 ({} フレーム) ===", frameIndex);
    
#elif defined(__linux__) || defined(__APPLE__)
    // Unix系環境での完璧なスタックトレース
    
    void* callstack[128];
    int frames = backtrace(callstack, 128);
    char** symbols = backtrace_symbols(callstack, frames);
    
    LOG_INFO("=== スタックトレース ({}) ===", category ? category : "Unknown");
    
    for (int i = 0; i < frames; ++i) {
        std::string symbolInfo = symbols ? symbols[i] : "Unknown";
        
        // シンボル情報の解析と整形
        std::string demangledName = demangleSymbol(symbolInfo);
        
        LOG_INFO("#{:2d}: {}", i, demangledName);
    }
    
    if (symbols) {
        free(symbols);
    }
    
    LOG_INFO("=== スタックトレース終了 ({} フレーム) ===", frames);
    
#else
    // その他の環境
    LOG_INFO("スタックトレースはこのプラットフォームではサポートされていません");
#endif
}

#ifdef _WIN32
// Windows用のヘルパー関数

bool InitializeSymbolHandler() {
    static bool initialized = false;
    static std::mutex initMutex;
    
    std::lock_guard<std::mutex> lock(initMutex);
    
    if (!initialized) {
        HANDLE process = GetCurrentProcess();
        
        if (SymInitialize(process, NULL, TRUE)) {
            SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
            initialized = true;
        }
    }
    
    return initialized;
}

std::string GetModuleNameFromAddress(DWORD64 address) {
    IMAGEHLP_MODULE64 moduleInfo;
    moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
    
    if (SymGetModuleInfo64(GetCurrentProcess(), address, &moduleInfo)) {
        return moduleInfo.ModuleName;
    }
    
    return "Unknown";
}

std::string GetSymbolNameFromAddress(DWORD64 address, DWORD64& displacement) {
    char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO symbol = (PSYMBOL_INFO)symbolBuffer;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;
    
    if (SymFromAddr(GetCurrentProcess(), address, &displacement, symbol)) {
        return symbol->Name;
    }
    
    return "Unknown";
}

bool GetSourceLocationFromAddress(DWORD64 address, std::string& fileName, DWORD& lineNumber) {
    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    DWORD displacement = 0;
    
    if (SymGetLineFromAddr64(GetCurrentProcess(), address, &displacement, &line)) {
        fileName = line.FileName;
        lineNumber = line.LineNumber;
        
        // ファイル名のパスを短縮
        size_t lastSlash = fileName.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            fileName = fileName.substr(lastSlash + 1);
        }
        
        return true;
    }
    
    return false;
}

#endif

#if defined(__linux__) || defined(__APPLE__)
// Unix系用のヘルパー関数

std::string demangleSymbol(const std::string& mangledName) {
    // C++シンボルのデマングリング
    
    size_t start = mangledName.find('(');
    size_t end = mangledName.find('+', start);
    
    if (start == std::string::npos || end == std::string::npos) {
        return mangledName;
    }
    
    std::string symbol = mangledName.substr(start + 1, end - start - 1);
    
#ifdef __GNUC__
    int status = 0;
    char* demangled = abi::__cxa_demangle(symbol.c_str(), nullptr, nullptr, &status);
    
    if (status == 0 && demangled) {
        std::string result = demangled;
        free(demangled);
        return mangledName.substr(0, start + 1) + result + mangledName.substr(end);
    }
#endif
    
    return mangledName;
}

#endif

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