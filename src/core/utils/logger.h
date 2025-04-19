/**
 * @file logger.h
 * @brief ロギング機能を提供するクラスの定義
 * 
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#ifndef AERO_LOGGER_H
#define AERO_LOGGER_H

#include <string>
#include <mutex>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <memory>

namespace aero {

/**
 * @brief ログレベルを表す列挙型
 */
enum class LogLevel {
    TRACE,     ///< トレースレベル（最も詳細）
    DEBUG,     ///< デバッグレベル
    INFO,      ///< 情報レベル
    WARNING,   ///< 警告レベル
    ERROR,     ///< エラーレベル
    CRITICAL,  ///< 致命的なエラーレベル
    OFF        ///< ログ無効
};

/**
 * @brief 文字列からログレベルに変換する
 * 
 * @param levelStr ログレベル文字列
 * @return LogLevel 対応するログレベル（不明な文字列の場合はINFO）
 */
inline LogLevel stringToLogLevel(const std::string& levelStr) {
    if (levelStr == "trace" || levelStr == "TRACE") return LogLevel::TRACE;
    if (levelStr == "debug" || levelStr == "DEBUG") return LogLevel::DEBUG;
    if (levelStr == "info" || levelStr == "INFO") return LogLevel::INFO;
    if (levelStr == "warning" || levelStr == "WARNING") return LogLevel::WARNING;
    if (levelStr == "error" || levelStr == "ERROR") return LogLevel::ERROR;
    if (levelStr == "critical" || levelStr == "CRITICAL") return LogLevel::CRITICAL;
    if (levelStr == "off" || levelStr == "OFF") return LogLevel::OFF;
    return LogLevel::INFO;  // デフォルト
}

/**
 * @brief ログレベルを文字列に変換する
 * 
 * @param level ログレベル
 * @return std::string 対応する文字列
 */
inline std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        case LogLevel::OFF: return "OFF";
        default: return "UNKNOWN";
    }
}

/**
 * @brief ログ出力先を表す列挙型
 */
enum class LogTarget {
    CONSOLE,    ///< コンソール出力
    FILE,       ///< ファイル出力
    CALLBACK    ///< コールバック関数
};

/**
 * @brief ログ出力オプション
 */
struct LoggerOptions {
    LogLevel level = LogLevel::INFO;              ///< ログレベル
    bool useColors = true;                        ///< 色付き出力を使用するか
    bool showTimestamp = true;                    ///< タイムスタンプを表示するか
    bool showLevel = true;                        ///< ログレベルを表示するか
    bool showCategory = true;                     ///< カテゴリを表示するか
    bool showSourceLocation = false;              ///< ソースの場所を表示するか
    std::string dateTimeFormat = "%Y-%m-%d %H:%M:%S"; ///< 日時フォーマット
    std::string logFilePath;                      ///< ログファイルパス
    bool appendToFile = true;                     ///< ファイルに追記するか
    std::vector<LogTarget> targets = {LogTarget::CONSOLE}; ///< ログ出力先
    size_t maxFileSizeBytes = 10 * 1024 * 1024;   ///< 最大ファイルサイズ（バイト）
    int maxBackupFiles = 3;                       ///< 最大バックアップファイル数
};

/**
 * @brief フォーマット置換用ヘルパー関数（再帰終了用）
 */
template<typename... Args>
void formatHelper(std::ostringstream& ss, const char* format) {
    ss << format;
}

/**
 * @brief フォーマット置換用ヘルパー関数
 */
template<typename T, typename... Args>
void formatHelper(std::ostringstream& ss, const char* format, T&& value, Args&&... args) {
    while (*format) {
        if (*format == '{' && *(format + 1) == '}') {
            ss << value;
            formatHelper(ss, format + 2, std::forward<Args>(args)...);
            return;
        }
        ss << *format++;
    }
}

/**
 * @brief ロガークラス
 * 
 * アプリケーション全体で使用するログ機能を提供するクラス。
 * シングルトンパターンを使用して、カテゴリごとにインスタンスを管理します。
 */
class Logger {
public:
    /**
     * @brief 指定したカテゴリのロガーインスタンスを取得する
     * 
     * @param category ログカテゴリ
     * @return Logger& ロガーインスタンス
     */
    static Logger& getInstance(const std::string& category = "default") {
        static std::map<std::string, std::unique_ptr<Logger>> instances;
        static std::mutex instancesMutex;
        
        std::lock_guard<std::mutex> lock(instancesMutex);
        
        auto it = instances.find(category);
        if (it == instances.end()) {
            instances[category] = std::unique_ptr<Logger>(new Logger(category));
            it = instances.find(category);
        }
        
        return *it->second;
    }

    /**
     * @brief ロガーオプションを設定する
     * 
     * @param options 新しいオプション
     */
    void setOptions(const LoggerOptions& options) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_options = options;
        
        // ファイル出力が有効な場合、ファイルを開く
        if (isTargetEnabled(LogTarget::FILE) && !m_options.logFilePath.empty()) {
            openLogFile();
        }
    }

    /**
     * @brief ログレベルを設定する
     * 
     * @param level 新しいログレベル
     */
    void setLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_options.level = level;
    }

    /**
     * @brief ログレベルを文字列で設定する
     * 
     * @param levelStr ログレベル文字列
     */
    void setLevel(const std::string& levelStr) {
        setLevel(stringToLogLevel(levelStr));
    }

    /**
     * @brief ログターゲットを設定する
     * 
     * @param targets ログターゲットのリスト
     */
    void setTargets(const std::vector<LogTarget>& targets) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_options.targets = targets;
        
        // ファイル出力が有効な場合、ファイルを開く
        if (isTargetEnabled(LogTarget::FILE) && !m_options.logFilePath.empty()) {
            openLogFile();
        }
    }

    /**
     * @brief ログファイルパスを設定する
     * 
     * @param filePath ログファイルパス
     * @param append 追記モードを使用するか
     */
    void setLogFile(const std::string& filePath, bool append = true) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_options.logFilePath = filePath;
        m_options.appendToFile = append;
        
        // ファイル出力が有効な場合、ファイルを開く
        if (isTargetEnabled(LogTarget::FILE)) {
            openLogFile();
        }
    }

    /**
     * @brief ログコールバック関数を設定する
     * 
     * @param callback コールバック関数
     */
    void setLogCallback(std::function<void(LogLevel, const std::string&, const std::string&)> callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logCallback = callback;
    }

    /**
     * @brief TRACEレベルのログを出力する
     * 
     * @param format フォーマット文字列
     * @param args フォーマット引数
     */
    template<typename... Args>
    void trace(const char* format, Args&&... args) {
        log(LogLevel::TRACE, format, std::forward<Args>(args)...);
    }

    /**
     * @brief DEBUGレベルのログを出力する
     * 
     * @param format フォーマット文字列
     * @param args フォーマット引数
     */
    template<typename... Args>
    void debug(const char* format, Args&&... args) {
        log(LogLevel::DEBUG, format, std::forward<Args>(args)...);
    }

    /**
     * @brief INFOレベルのログを出力する
     * 
     * @param format フォーマット文字列
     * @param args フォーマット引数
     */
    template<typename... Args>
    void info(const char* format, Args&&... args) {
        log(LogLevel::INFO, format, std::forward<Args>(args)...);
    }

    /**
     * @brief WARNINGレベルのログを出力する
     * 
     * @param format フォーマット文字列
     * @param args フォーマット引数
     */
    template<typename... Args>
    void warning(const char* format, Args&&... args) {
        log(LogLevel::WARNING, format, std::forward<Args>(args)...);
    }

    /**
     * @brief ERRORレベルのログを出力する
     * 
     * @param format フォーマット文字列
     * @param args フォーマット引数
     */
    template<typename... Args>
    void error(const char* format, Args&&... args) {
        log(LogLevel::ERROR, format, std::forward<Args>(args)...);
    }

    /**
     * @brief CRITICALレベルのログを出力する
     * 
     * @param format フォーマット文字列
     * @param args フォーマット引数
     */
    template<typename... Args>
    void critical(const char* format, Args&&... args) {
        log(LogLevel::CRITICAL, format, std::forward<Args>(args)...);
    }

    /**
     * @brief 指定したレベルのログを出力する
     * 
     * @param level ログレベル
     * @param format フォーマット文字列
     * @param args フォーマット引数
     */
    template<typename... Args>
    void log(LogLevel level, const char* format, Args&&... args) {
        if (level < m_options.level || m_options.level == LogLevel::OFF) {
            return;
        }
        
        std::ostringstream message;
        formatHelper(message, format, std::forward<Args>(args)...);
        
        std::string formattedMessage = formatLogMessage(level, m_category, message.str());
        
        outputLog(level, message.str(), formattedMessage);
    }

private:
    /**
     * @brief コンストラクタ
     * 
     * @param category ログカテゴリ
     */
    Logger(const std::string& category)
        : m_category(category) {
    }
    
    /**
     * @brief デストラクタ
     */
    ~Logger() {
        if (m_logFile.is_open()) {
            m_logFile.close();
        }
    }
    
    /**
     * @brief コピーコンストラクタ（禁止）
     */
    Logger(const Logger&) = delete;
    
    /**
     * @brief 代入演算子（禁止）
     */
    Logger& operator=(const Logger&) = delete;
    
    /**
     * @brief ログファイルを開く
     */
    void openLogFile() {
        if (m_logFile.is_open()) {
            m_logFile.close();
        }
        
        if (m_options.appendToFile) {
            m_logFile.open(m_options.logFilePath, std::ios_base::app);
        } else {
            m_logFile.open(m_options.logFilePath, std::ios_base::out);
        }
        
        if (!m_logFile.is_open()) {
            std::cerr << "ログファイルを開けませんでした: " << m_options.logFilePath << std::endl;
        }
    }
    
    /**
     * @brief ログメッセージをフォーマットする
     * 
     * @param level ログレベル
     * @param category ログカテゴリ
     * @param message ログメッセージ
     * @return std::string フォーマットされたログメッセージ
     */
    std::string formatLogMessage(LogLevel level, const std::string& category, const std::string& message) {
        std::ostringstream ss;
        
        // タイムスタンプ
        if (m_options.showTimestamp) {
            auto now = std::chrono::system_clock::now();
            auto nowTime = std::chrono::system_clock::to_time_t(now);
            ss << std::put_time(std::localtime(&nowTime), m_options.dateTimeFormat.c_str()) << " ";
        }
        
        // ログレベル
        if (m_options.showLevel) {
            if (m_options.useColors) {
                ss << getColorCode(level);
            }
            
            ss << "[" << logLevelToString(level) << "]";
            
            if (m_options.useColors) {
                ss << "\033[0m";
            }
            
            ss << " ";
        }
        
        // カテゴリ
        if (m_options.showCategory && !category.empty()) {
            ss << "[" << category << "] ";
        }
        
        // メッセージ
        ss << message;
        
        return ss.str();
    }
    
    /**
     * @brief ログを出力する
     * 
     * @param level ログレベル
     * @param rawMessage 生のメッセージ
     * @param formattedMessage フォーマット済みメッセージ
     */
    void outputLog(LogLevel level, const std::string& rawMessage, const std::string& formattedMessage) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // コンソール出力
        if (isTargetEnabled(LogTarget::CONSOLE)) {
            if (level >= LogLevel::ERROR) {
                std::cerr << formattedMessage << std::endl;
            } else {
                std::cout << formattedMessage << std::endl;
            }
        }
        
        // ファイル出力
        if (isTargetEnabled(LogTarget::FILE) && m_logFile.is_open()) {
            m_logFile << formattedMessage << std::endl;
            
            // ログローテーション
            if (m_logFile.tellp() > static_cast<std::streampos>(m_options.maxFileSizeBytes)) {
                rotateLogFiles();
            }
        }
        
        // コールバック出力
        if (isTargetEnabled(LogTarget::CALLBACK) && m_logCallback) {
            m_logCallback(level, m_category, rawMessage);
        }
    }
    
    /**
     * @brief ログファイルをローテーションする
     */
    void rotateLogFiles() {
        m_logFile.close();
        
        // バックアップファイルをシフト
        for (int i = m_options.maxBackupFiles - 1; i > 0; --i) {
            std::string oldFile = m_options.logFilePath + "." + std::to_string(i);
            std::string newFile = m_options.logFilePath + "." + std::to_string(i + 1);
            
            if (std::ifstream(oldFile)) {
                std::rename(oldFile.c_str(), newFile.c_str());
            }
        }
        
        // 現在のログファイルをバックアップ
        std::string backupFile = m_options.logFilePath + ".1";
        std::rename(m_options.logFilePath.c_str(), backupFile.c_str());
        
        // 新しいログファイルを開く
        openLogFile();
    }
    
    /**
     * @brief 指定したログターゲットが有効かどうかを確認する
     * 
     * @param target 確認するログターゲット
     * @return bool ターゲットが有効ならtrue
     */
    bool isTargetEnabled(LogTarget target) const {
        for (auto& t : m_options.targets) {
            if (t == target) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief ログレベルに対応する色コードを取得する
     * 
     * @param level ログレベル
     * @return std::string 色コード
     */
    std::string getColorCode(LogLevel level) const {
        switch (level) {
            case LogLevel::TRACE: return "\033[90m";    // 灰色
            case LogLevel::DEBUG: return "\033[36m";    // 水色
            case LogLevel::INFO: return "\033[32m";     // 緑色
            case LogLevel::WARNING: return "\033[33m";  // 黄色
            case LogLevel::ERROR: return "\033[31m";    // 赤色
            case LogLevel::CRITICAL: return "\033[35m"; // マゼンタ
            default: return "\033[0m";                  // リセット
        }
    }
    
    std::string m_category;                         ///< ログカテゴリ
    LoggerOptions m_options;                        ///< ロガーオプション
    std::mutex m_mutex;                             ///< スレッド同期用ミューテックス
    std::ofstream m_logFile;                        ///< ログファイルストリーム
    std::function<void(LogLevel, const std::string&, const std::string&)> m_logCallback; ///< ログコールバック
};

} // namespace aero

#endif // AERO_LOGGER_H 