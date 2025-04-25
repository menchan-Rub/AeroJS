#include "ic_logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <regex>
#include <ctime>
#include <filesystem>

namespace aerojs {
namespace core {

//==============================================================================
// ユーティリティ関数
//==============================================================================

namespace {

// フォーマット文字列内のプレースホルダを置換する
std::string ReplaceTokens(
    const std::string& format,
    const std::map<std::string, std::string>& tokens) {
    
    std::string result = format;
    for (const auto& token : tokens) {
        const std::string placeholder = "{" + token.first + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), token.second);
            pos += token.second.length();
        }
    }
    return result;
}

// 指定されたファイルのサイズを取得する
size_t GetFileSize(const std::string& filePath) {
    try {
        return std::filesystem::file_size(filePath);
    } catch (const std::filesystem::filesystem_error&) {
        return 0;
    }
}

// ワイルドカードパターンにマッチするかどうかを判定する
bool MatchWildcard(const std::string& text, const std::string& pattern) {
    if (pattern.empty()) return true;
    if (pattern == "*") return true;
    
    const char* p = pattern.c_str();
    const char* t = text.c_str();
    const char* mp = nullptr;
    const char* mt = nullptr;
    
    while (*t) {
        // 現在の文字がマッチするか、パターンが'?'ならば次の文字へ
        if (*p == '?' || *p == *t) {
            p++;
            t++;
            continue;
        }
        
        // '*'があれば、その位置を記録し、パターンを一つ進める
        if (*p == '*') {
            mp = p++;
            mt = t;
            continue;
        }
        
        // マッチしなかったが、'*'がある場合は前の'*'に戻って
        // テキストの次の文字からマッチを試みる
        if (mp) {
            p = mp + 1;
            t = ++mt;
            continue;
        }
        
        // マッチしなかった場合はfalseを返す
        return false;
    }
    
    // パターンの残りが全て'*'かどうかを確認
    while (*p == '*') p++;
    
    // パターンの終わりまできていればマッチ成功
    return !*p;
}

} // namespace

//==============================================================================
// ICConsoleLogSink
//==============================================================================

ICConsoleLogSink::ICConsoleLogSink(bool useColors)
    : m_useColors(useColors) {
}

void ICConsoleLogSink::Write(const ICLogEntry& entry) {
    const std::string formattedMessage = ICLogger::Instance().FormatLogMessage(entry);
    
    std::lock_guard<std::mutex> lock(m_consoleMutex);
    if (m_useColors) {
        std::cout << GetColorCode(entry.level) << formattedMessage << RESET_COLOR << std::endl;
    } else {
        std::cout << formattedMessage << std::endl;
    }
}

void ICConsoleLogSink::Flush() {
    std::lock_guard<std::mutex> lock(m_consoleMutex);
    std::cout.flush();
}

std::string ICConsoleLogSink::GetName() const {
    return "Console";
}

void ICConsoleLogSink::SetUseColors(bool useColors) {
    m_useColors = useColors;
}

std::string ICConsoleLogSink::GetColorCode(ICLogLevel level) const {
    switch (level) {
    case ICLogLevel::Debug:
        return "\033[90m"; // 暗い灰色
    case ICLogLevel::Info:
        return "\033[32m"; // 緑色
    case ICLogLevel::Warning:
        return "\033[33m"; // 黄色
    case ICLogLevel::Error:
        return "\033[31m"; // 赤色
    case ICLogLevel::Critical:
        return "\033[1;31m"; // 太字の赤色
    default:
        return "";
    }
}

//==============================================================================
// ICFileLogSink
//==============================================================================

ICFileLogSink::ICFileLogSink(const std::string& filePath, bool append)
    : m_filePath(filePath)
    , m_rotationSizeBytes(0)
    , m_maxRotationFiles(0) {
    
    std::lock_guard<std::mutex> lock(m_fileMutex);
    const std::ios_base::openmode mode = std::ios::out | (append ? std::ios::app : std::ios::trunc);
    m_fileStream.open(filePath, mode);
    
    if (!m_fileStream) {
        std::cerr << "Failed to open log file: " << filePath << std::endl;
    }
}

ICFileLogSink::~ICFileLogSink() {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    if (m_fileStream.is_open()) {
        m_fileStream.flush();
        m_fileStream.close();
    }
}

void ICFileLogSink::Write(const ICLogEntry& entry) {
    const std::string formattedMessage = ICLogger::Instance().FormatLogMessage(entry);
    
    std::lock_guard<std::mutex> lock(m_fileMutex);
    
    if (m_fileStream.is_open()) {
        m_fileStream << formattedMessage << std::endl;
        
        // ファイルローテーションが必要かチェック
        if (m_rotationSizeBytes > 0) {
            RotateLogFileIfNeeded();
        }
    }
}

void ICFileLogSink::Flush() {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    if (m_fileStream.is_open()) {
        m_fileStream.flush();
    }
}

std::string ICFileLogSink::GetName() const {
    return "File:" + m_filePath;
}

bool ICFileLogSink::SwitchFile(const std::string& newFilePath, bool append) {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    
    // 現在のファイルを閉じる
    if (m_fileStream.is_open()) {
        m_fileStream.flush();
        m_fileStream.close();
    }
    
    // 新しいファイルを開く
    m_filePath = newFilePath;
    const std::ios_base::openmode mode = std::ios::out | (append ? std::ios::app : std::ios::trunc);
    m_fileStream.open(newFilePath, mode);
    
    return m_fileStream.is_open();
}

std::string ICFileLogSink::GetFilePath() const {
    return m_filePath;
}

size_t ICFileLogSink::GetFileSize() const {
    std::lock_guard<std::mutex> lock(m_fileMutex);
    
    if (m_fileStream.is_open()) {
        return GetFileSize(m_filePath);
    }
    
    return 0;
}

void ICFileLogSink::SetRotationSize(size_t maxSizeBytes) {
    m_rotationSizeBytes = maxSizeBytes;
}

void ICFileLogSink::SetMaxRotationFiles(size_t maxFiles) {
    m_maxRotationFiles = maxFiles;
}

void ICFileLogSink::RotateLogFileIfNeeded() {
    if (!m_fileStream.is_open() || m_rotationSizeBytes == 0) {
        return;
    }
    
    // 現在のファイルサイズを確認
    size_t currentSize = GetFileSize(m_filePath);
    if (currentSize < m_rotationSizeBytes) {
        return;
    }
    
    // ファイルを閉じる
    m_fileStream.flush();
    m_fileStream.close();
    
    // ファイルのローテーション
    std::string directory = std::filesystem::path(m_filePath).parent_path().string();
    std::string baseName = std::filesystem::path(m_filePath).filename().string();
    std::string extension = std::filesystem::path(m_filePath).extension().string();
    std::string nameWithoutExt = baseName;
    
    if (!extension.empty()) {
        nameWithoutExt = baseName.substr(0, baseName.length() - extension.length());
    }
    
    // 既存のログファイルを移動
    for (int i = m_maxRotationFiles - 1; i >= 1; --i) {
        std::string oldPath = directory + "/" + nameWithoutExt + "." + std::to_string(i) + extension;
        std::string newPath = directory + "/" + nameWithoutExt + "." + std::to_string(i + 1) + extension;
        
        if (std::filesystem::exists(oldPath)) {
            try {
                std::filesystem::rename(oldPath, newPath);
            } catch (const std::filesystem::filesystem_error&) {
                // エラー処理（必要に応じて）
            }
        }
    }
    
    // 現在のログファイルを.1にリネーム
    std::string backupPath = directory + "/" + nameWithoutExt + ".1" + extension;
    try {
        std::filesystem::rename(m_filePath, backupPath);
    } catch (const std::filesystem::filesystem_error&) {
        // エラー処理（必要に応じて）
    }
    
    // 新しいログファイルを開く
    m_fileStream.open(m_filePath, std::ios::out | std::ios::trunc);
    
    // 古いローテーションファイルをクリーンアップ
    if (m_maxRotationFiles > 0) {
        CleanupRotatedFiles();
    }
}

void ICFileLogSink::CleanupRotatedFiles() {
    std::string directory = std::filesystem::path(m_filePath).parent_path().string();
    std::string baseName = std::filesystem::path(m_filePath).filename().string();
    std::string extension = std::filesystem::path(m_filePath).extension().string();
    std::string nameWithoutExt = baseName;
    
    if (!extension.empty()) {
        nameWithoutExt = baseName.substr(0, baseName.length() - extension.length());
    }
    
    // m_maxRotationFiles+1より大きい番号のファイルを削除
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            const std::string& path = entry.path().string();
            const std::string& filename = entry.path().filename().string();
            
            // ファイル名がパターンに一致するか確認
            std::string pattern = nameWithoutExt + ".(\\d+)" + extension;
            if (std::regex_match(filename, std::regex(pattern))) {
                // ファイル番号を抽出
                int fileNumber = std::stoi(filename.substr(
                    nameWithoutExt.length() + 1,
                    filename.length() - nameWithoutExt.length() - 1 - extension.length()));
                
                if (fileNumber > static_cast<int>(m_maxRotationFiles)) {
                    std::filesystem::remove(path);
                }
            }
        }
    } catch (const std::exception&) {
        // エラー処理（必要に応じて）
    }
}

//==============================================================================
// ICCustomLogSink
//==============================================================================

ICCustomLogSink::ICCustomLogSink(
    const std::string& name,
    LogCallback logCallback,
    FlushCallback flushCallback)
    : m_name(name)
    , m_logCallback(logCallback)
    , m_flushCallback(flushCallback) {
}

void ICCustomLogSink::Write(const ICLogEntry& entry) {
    if (m_logCallback) {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_logCallback(entry);
    }
}

void ICCustomLogSink::Flush() {
    if (m_flushCallback) {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_flushCallback();
    }
}

std::string ICCustomLogSink::GetName() const {
    return m_name;
}

//==============================================================================
// ICMemoryLogSink
//==============================================================================

ICMemoryLogSink::ICMemoryLogSink(size_t maxEntries)
    : m_maxEntries(maxEntries) {
}

void ICMemoryLogSink::Write(const ICLogEntry& entry) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    
    m_entries.push_back(entry);
    
    // エントリ数が最大数を超えた場合、古いエントリを削除
    while (m_entries.size() > m_maxEntries) {
        m_entries.erase(m_entries.begin());
    }
}

void ICMemoryLogSink::Flush() {
    // メモリ内のみなのでフラッシュ操作は不要
}

std::string ICMemoryLogSink::GetName() const {
    return "Memory";
}

void ICMemoryLogSink::SetMaxEntries(size_t maxEntries) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    
    m_maxEntries = maxEntries;
    
    // エントリ数が新しい最大数を超えた場合、古いエントリを削除
    while (m_entries.size() > m_maxEntries) {
        m_entries.erase(m_entries.begin());
    }
}

std::vector<ICLogEntry> ICMemoryLogSink::GetEntries() const {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    return m_entries;
}

std::vector<ICLogEntry> ICMemoryLogSink::GetEntriesByLevel(ICLogLevel minLevel) const {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    
    std::vector<ICLogEntry> result;
    std::copy_if(m_entries.begin(), m_entries.end(), std::back_inserter(result),
        [minLevel](const ICLogEntry& entry) {
            return static_cast<int>(entry.level) >= static_cast<int>(minLevel);
        });
    
    return result;
}

std::vector<ICLogEntry> ICMemoryLogSink::GetEntriesByCategory(const std::string& category) const {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    
    std::vector<ICLogEntry> result;
    std::copy_if(m_entries.begin(), m_entries.end(), std::back_inserter(result),
        [&category](const ICLogEntry& entry) {
            return MatchWildcard(entry.category, category);
        });
    
    return result;
}

void ICMemoryLogSink::Clear() {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    m_entries.clear();
}

//==============================================================================
// ICLogQueue
//==============================================================================

ICLogQueue::ICLogQueue()
    : m_running(false) {
}

ICLogQueue::~ICLogQueue() {
    Stop();
}

void ICLogQueue::EnqueueEntry(const ICLogEntry& entry) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_queue.push(entry);
    }
    
    m_condition.notify_one();
}

bool ICLogQueue::DequeueEntry(ICLogEntry& entry) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    
    if (m_queue.empty()) {
        return false;
    }
    
    entry = m_queue.front();
    m_queue.pop();
    return true;
}

void ICLogQueue::Start() {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    
    if (m_running) {
        return;
    }
    
    m_running = true;
    m_workerThread = std::thread(&ICLogQueue::ProcessQueue, this);
}

void ICLogQueue::Stop() {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        
        if (!m_running) {
            return;
        }
        
        m_running = false;
    }
    
    m_condition.notify_all();
    
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

bool ICLogQueue::IsEmpty() const {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_queue.empty();
}

size_t ICLogQueue::GetSize() const {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_queue.size();
}

void ICLogQueue::ProcessQueue() {
    ICLogEntry entry;
    
    while (true) {
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            
            // キューが空で、まだ実行中の場合は待機
            m_condition.wait(lock, [this] {
                return !m_queue.empty() || !m_running;
            });
            
            // 停止が要求された場合、スレッドを終了
            if (!m_running && m_queue.empty()) {
                return;
            }
            
            // キューからエントリを取得
            if (!m_queue.empty()) {
                entry = m_queue.front();
                m_queue.pop();
            } else {
                continue;
            }
        }
        
        // ロガーでエントリを処理
        ICLogger::Instance().ProcessLogEntry(entry);
    }
}

//==============================================================================
// ICLogger
//==============================================================================

ICLogger& ICLogger::Instance() {
    static ICLogger instance;
    return instance;
}

ICLogger::ICLogger()
    : m_minLogLevel(ICLogLevel::Info)
    , m_logFormat("[{timestamp}] [{level}] [{category}] {message}")
    , m_asyncLoggingEnabled(false)
    , m_logQueue(std::make_unique<ICLogQueue>()) {
}

ICLogger::~ICLogger() {
    if (m_asyncLoggingEnabled) {
        EnableAsyncLogging(false);
    }
    
    FlushAllSinks();
}

void ICLogger::Log(
    ICLogLevel level,
    const std::string& message,
    const std::string& category,
    const std::string& source) {
    
    // ログレベルとカテゴリのフィルタリング
    if (static_cast<int>(level) < static_cast<int>(m_minLogLevel)) {
        return;
    }
    
    if (!category.empty() && !IsCategoryEnabled(category)) {
        return;
    }
    
    // ログエントリの作成
    ICLogEntry entry(level, message, category, source);
    
    // 非同期モードの場合はキューに追加、そうでなければ直接処理
    if (m_asyncLoggingEnabled) {
        m_logQueue->EnqueueEntry(entry);
    } else {
        ProcessLogEntry(entry);
    }
}

void ICLogger::Debug(
    const std::string& message,
    const std::string& category,
    const std::string& source) {
    
    Log(ICLogLevel::Debug, message, category, source);
}

void ICLogger::Info(
    const std::string& message,
    const std::string& category,
    const std::string& source) {
    
    Log(ICLogLevel::Info, message, category, source);
}

void ICLogger::Warning(
    const std::string& message,
    const std::string& category,
    const std::string& source) {
    
    Log(ICLogLevel::Warning, message, category, source);
}

void ICLogger::Error(
    const std::string& message,
    const std::string& category,
    const std::string& source) {
    
    Log(ICLogLevel::Error, message, category, source);
}

void ICLogger::Critical(
    const std::string& message,
    const std::string& category,
    const std::string& source) {
    
    Log(ICLogLevel::Critical, message, category, source);
}

void ICLogger::AddSink(std::shared_ptr<ICLogSink> sink) {
    if (!sink) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_sinksMutex);
    
    // 既に同じ名前のシンクが存在する場合は削除
    const std::string& name = sink->GetName();
    m_sinks.erase(
        std::remove_if(m_sinks.begin(), m_sinks.end(),
            [&name](const std::shared_ptr<ICLogSink>& existingSink) {
                return existingSink->GetName() == name;
            }),
        m_sinks.end());
    
    m_sinks.push_back(sink);
}

bool ICLogger::RemoveSink(const std::string& sinkName) {
    std::lock_guard<std::mutex> lock(m_sinksMutex);
    
    const size_t originalSize = m_sinks.size();
    
    m_sinks.erase(
        std::remove_if(m_sinks.begin(), m_sinks.end(),
            [&sinkName](const std::shared_ptr<ICLogSink>& sink) {
                return sink->GetName() == sinkName;
            }),
        m_sinks.end());
    
    return m_sinks.size() < originalSize;
}

std::shared_ptr<ICLogSink> ICLogger::GetSink(const std::string& sinkName) {
    std::lock_guard<std::mutex> lock(m_sinksMutex);
    
    auto it = std::find_if(m_sinks.begin(), m_sinks.end(),
        [&sinkName](const std::shared_ptr<ICLogSink>& sink) {
            return sink->GetName() == sinkName;
        });
    
    if (it != m_sinks.end()) {
        return *it;
    }
    
    return nullptr;
}

std::vector<std::string> ICLogger::GetSinkNames() const {
    std::lock_guard<std::mutex> lock(m_sinksMutex);
    
    std::vector<std::string> names;
    names.reserve(m_sinks.size());
    
    for (const auto& sink : m_sinks) {
        names.push_back(sink->GetName());
    }
    
    return names;
}

void ICLogger::FlushAllSinks() {
    std::lock_guard<std::mutex> lock(m_sinksMutex);
    
    for (const auto& sink : m_sinks) {
        sink->Flush();
    }
}

void ICLogger::SetMinLogLevel(ICLogLevel level) {
    m_minLogLevel = level;
}

ICLogLevel ICLogger::GetMinLogLevel() const {
    return m_minLogLevel;
}

void ICLogger::AddCategoryFilter(const std::string& category) {
    std::lock_guard<std::mutex> lock(m_filtersMutex);
    m_categoryFilters[category] = true;
}

void ICLogger::RemoveCategoryFilter(const std::string& category) {
    std::lock_guard<std::mutex> lock(m_filtersMutex);
    
    auto it = m_categoryFilters.find(category);
    if (it != m_categoryFilters.end()) {
        m_categoryFilters.erase(it);
    }
}

void ICLogger::ClearCategoryFilters() {
    std::lock_guard<std::mutex> lock(m_filtersMutex);
    m_categoryFilters.clear();
}

bool ICLogger::IsCategoryEnabled(const std::string& category) const {
    std::lock_guard<std::mutex> lock(m_filtersMutex);
    
    // フィルタが空の場合は全てのカテゴリを許可
    if (m_categoryFilters.empty()) {
        return true;
    }
    
    // フィルタとのマッチチェック
    for (const auto& filter : m_categoryFilters) {
        if (MatchWildcard(category, filter.first)) {
            return filter.second;
        }
    }
    
    // デフォルトは許可
    return true;
}

void ICLogger::EnableAsyncLogging(bool enable) {
    if (m_asyncLoggingEnabled == enable) {
        return;
    }
    
    m_asyncLoggingEnabled = enable;
    
    if (enable) {
        m_logQueue->Start();
    } else {
        m_logQueue->Stop();
        
        // 残っているエントリを処理
        ICLogEntry entry;
        while (m_logQueue->DequeueEntry(entry)) {
            ProcessLogEntry(entry);
        }
    }
}

bool ICLogger::IsAsyncLoggingEnabled() const {
    return m_asyncLoggingEnabled;
}

void ICLogger::SetupDefaultSinks(bool console, const std::string& logFilePath) {
    // 既存のシンクをクリア
    {
        std::lock_guard<std::mutex> lock(m_sinksMutex);
        m_sinks.clear();
    }
    
    // コンソール出力が有効な場合
    if (console) {
        AddSink(std::make_shared<ICConsoleLogSink>());
    }
    
    // ファイル出力が有効な場合
    if (!logFilePath.empty()) {
        AddSink(std::make_shared<ICFileLogSink>(logFilePath));
    }
}

void ICLogger::SetLogFormat(const std::string& format) {
    std::lock_guard<std::mutex> lock(m_formatMutex);
    m_logFormat = format;
}

std::string ICLogger::GetLogFormat() const {
    std::lock_guard<std::mutex> lock(m_formatMutex);
    return m_logFormat;
}

std::string ICLogger::FormatLogMessage(const ICLogEntry& entry) const {
    std::lock_guard<std::mutex> lock(m_formatMutex);
    
    // トークンマップの作成
    std::map<std::string, std::string> tokens;
    tokens["timestamp"] = FormatTimestamp(entry.timestamp);
    tokens["level"] = LogLevelToString(entry.level);
    tokens["category"] = entry.category.empty() ? "General" : entry.category;
    tokens["message"] = entry.message;
    tokens["thread"] = entry.threadId;
    tokens["source"] = entry.source;
    
    // フォーマット文字列のトークンを置換
    return ReplaceTokens(m_logFormat, tokens);
}

void ICLogger::ProcessLogEntry(const ICLogEntry& entry) {
    std::lock_guard<std::mutex> lock(m_sinksMutex);
    
    for (const auto& sink : m_sinks) {
        sink->Write(entry);
    }
}

std::string ICLogger::LogLevelToString(ICLogLevel level) {
    switch (level) {
    case ICLogLevel::Debug:
        return "DEBUG";
    case ICLogLevel::Info:
        return "INFO";
    case ICLogLevel::Warning:
        return "WARN";
    case ICLogLevel::Error:
        return "ERROR";
    case ICLogLevel::Critical:
        return "CRITICAL";
    default:
        return "UNKNOWN";
    }
}

ICLogLevel ICLogger::StringToLogLevel(const std::string& levelStr) {
    std::string upperLevel = levelStr;
    std::transform(upperLevel.begin(), upperLevel.end(), upperLevel.begin(), ::toupper);
    
    if (upperLevel == "DEBUG") {
        return ICLogLevel::Debug;
    } else if (upperLevel == "INFO") {
        return ICLogLevel::Info;
    } else if (upperLevel == "WARN" || upperLevel == "WARNING") {
        return ICLogLevel::Warning;
    } else if (upperLevel == "ERROR") {
        return ICLogLevel::Error;
    } else if (upperLevel == "CRITICAL" || upperLevel == "FATAL") {
        return ICLogLevel::Critical;
    } else {
        // デフォルトはInfo
        return ICLogLevel::Info;
    }
}

std::string ICLogger::FormatTimestamp(const std::chrono::system_clock::time_point& timestamp) const {
    const auto time = std::chrono::system_clock::to_time_t(timestamp);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

// ユーティリティマクロの定義
#define IC_LOG_DEBUG(message, category) \
    ::aerojs::core::ICLogger::Instance().Debug(message, category, __FILE__ ":" + std::to_string(__LINE__))

#define IC_LOG_INFO(message, category) \
    ::aerojs::core::ICLogger::Instance().Info(message, category, __FILE__ ":" + std::to_string(__LINE__))

#define IC_LOG_WARNING(message, category) \
    ::aerojs::core::ICLogger::Instance().Warning(message, category, __FILE__ ":" + std::to_string(__LINE__))

#define IC_LOG_ERROR(message, category) \
    ::aerojs::core::ICLogger::Instance().Error(message, category, __FILE__ ":" + std::to_string(__LINE__))

#define IC_LOG_CRITICAL(message, category) \
    ::aerojs::core::ICLogger::Instance().Critical(message, category, __FILE__ ":" + std::to_string(__LINE__))

} // namespace core
} // namespace aerojs 