/**
 * @file logger.h
 * @brief Simple logging functionality
 * @version 0.1.0
 * @license MIT
 */

#ifndef AERO_LOGGER_H
#define AERO_LOGGER_H

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <string_view>
#include <array>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#endif

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
 * @brief ログ出力先を表す列挙型
 */
enum class LogTarget {
  CONSOLE,  ///< コンソール出力
  FILE,     ///< ファイル出力
  CALLBACK, ///< コールバック関数
  SYSLOG    ///< システムログ
};

/**
 * @brief ログ出力オプション
 */
struct LoggerOptions {
  LogLevel level = LogLevel::INFO;
  bool useColors = true;
  bool showTimestamp = true;
  bool showLevel = true;
  bool showCategory = true;
  std::string dateTimeFormat = "%Y-%m-%d %H:%M:%S";
  std::string logFilePath;
  bool appendToFile = true;
  std::vector<LogTarget> targets = {LogTarget::CONSOLE};
  size_t maxFileSizeBytes = 10 * 1024 * 1024;
  int maxBackupFiles = 3;
  bool asyncLogging = false;
  size_t asyncQueueSize = 1024;
  size_t bufferSize = 8192;
};

/**
 * @brief シンプルなロガークラス
 */
class Logger {
public:
  /**
   * @brief 指定したカテゴリのロガーインスタンスを取得する
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
   * @brief オプションを設定する
   */
  void setOptions(const LoggerOptions& options) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_options = options;
  }

  /**
   * @brief ログレベルを設定する
   */
  void setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_options.level = level;
  }

  /**
   * @brief ログ出力先を設定する
   */
  void setTargets(const std::vector<LogTarget>& targets) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_options.targets = targets;
  }

  /**
   * @brief ログファイルを設定する
   */
  void setLogFile(const std::string& filePath, bool append = true) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_options.logFilePath = filePath;
    m_options.appendToFile = append;
  }

  /**
   * @brief ログコールバックを設定する
   */
  void setLogCallback(std::function<void(LogLevel, const std::string&, const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logCallback = callback;
  }

  /**
   * @brief TRACEレベルでログを出力する
   */
  template <typename... Args>
  void trace(const char* format, Args&&... args) {
    log(LogLevel::TRACE, format, std::forward<Args>(args)...);
  }

  /**
   * @brief DEBUGレベルでログを出力する
   */
  template <typename... Args>
  void debug(const char* format, Args&&... args) {
    log(LogLevel::DEBUG, format, std::forward<Args>(args)...);
  }

  /**
   * @brief INFOレベルでログを出力する
   */
  template <typename... Args>
  void info(const char* format, Args&&... args) {
    log(LogLevel::INFO, format, std::forward<Args>(args)...);
  }

  /**
   * @brief WARNINGレベルでログを出力する
   */
  template <typename... Args>
  void warning(const char* format, Args&&... args) {
    log(LogLevel::WARNING, format, std::forward<Args>(args)...);
  }

  /**
   * @brief ERRORレベルでログを出力する
   */
  template <typename... Args>
  void error(const char* format, Args&&... args) {
    log(LogLevel::ERROR, format, std::forward<Args>(args)...);
  }

  /**
   * @brief CRITICALレベルでログを出力する
   */
  template <typename... Args>
  void critical(const char* format, Args&&... args) {
    log(LogLevel::CRITICAL, format, std::forward<Args>(args)...);
  }

private:
  /**
   * @brief プライベートコンストラクタ
   */
  explicit Logger(const std::string& category) : m_category(category) {}

  /**
   * @brief ログを出力する内部メソッド
   */
  template <typename... Args>
  void log(LogLevel level, const char* format, Args&&... args) {
    if (level < m_options.level) return;

    char buffer[1024];
    snprintf(buffer, sizeof(buffer), format, std::forward<Args>(args)...);
    
    log_internal(level, buffer);
  }

  /**
   * @brief ログを出力する内部メソッド
   */
  void log_internal(LogLevel level, const char* message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string formattedMessage = formatMessage(level, message);

    // コンソール出力
    for (const auto& target : m_options.targets) {
      if (target == LogTarget::CONSOLE) {
        std::cout << formattedMessage << std::endl;
      }
      else if (target == LogTarget::FILE && !m_options.logFilePath.empty()) {
        writeToFile(formattedMessage);
      }
      else if (target == LogTarget::CALLBACK && m_logCallback) {
        m_logCallback(level, message, formattedMessage);
      }
    }
  }

  /**
   * @brief メッセージをフォーマットする
   */
  std::string formatMessage(LogLevel level, const char* message) {
    std::ostringstream oss;

    if (m_options.showTimestamp) {
      auto now = std::chrono::system_clock::now();
      auto time_t = std::chrono::system_clock::to_time_t(now);
      oss << std::put_time(std::localtime(&time_t), m_options.dateTimeFormat.c_str()) << " ";
    }

    if (m_options.showLevel) {
      oss << "[" << logLevelToString(level) << "] ";
    }

    if (m_options.showCategory) {
      oss << "[" << m_category << "] ";
    }

    oss << message;

    return oss.str();
  }

  /**
   * @brief ファイルに書き込む
   */
  void writeToFile(const std::string& message) {
    std::ofstream file(m_options.logFilePath, 
                      m_options.appendToFile ? std::ios::app : std::ios::out);
    if (file.is_open()) {
      file << message << std::endl;
    }
  }

  /**
   * @brief ログレベルを文字列に変換する
   */
  std::string_view logLevelToString(LogLevel level) {
    static constexpr std::array<std::string_view, 7> levelStrings = {
      "TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL", "OFF"
    };
    
    return levelStrings[static_cast<size_t>(level)];
  }

  std::string m_category;
  LoggerOptions m_options;
  std::mutex m_mutex;
  std::function<void(LogLevel, const std::string&, const std::string&)> m_logCallback;
};

} // namespace aero

#endif // AERO_LOGGER_H