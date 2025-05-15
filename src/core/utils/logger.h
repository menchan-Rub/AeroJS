/**
 * @file logger.h
 * @brief ロギング機能を提供するクラスの定義
 *
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
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

// コンパイル時ログレベル制御マクロ（リリースビルドでのTRACE/DEBUGログ無効化のため）
#ifndef AERO_LOG_LEVEL
  #ifdef NDEBUG
    #define AERO_LOG_LEVEL LogLevel::INFO
  #else
    #define AERO_LOG_LEVEL LogLevel::TRACE
  #endif
#endif

// メモリ効率の良いロギングマクロ
#define AERO_LOG_TRACE(logger, format, ...) \
  if (::aero::AERO_LOG_LEVEL <= ::aero::LogLevel::TRACE) (logger).trace(format, ##__VA_ARGS__)

#define AERO_LOG_DEBUG(logger, format, ...) \
  if (::aero::AERO_LOG_LEVEL <= ::aero::LogLevel::DEBUG) (logger).debug(format, ##__VA_ARGS__)

#define AERO_LOG_INFO(logger, format, ...) \
  if (::aero::AERO_LOG_LEVEL <= ::aero::LogLevel::INFO) (logger).info(format, ##__VA_ARGS__)

#define AERO_LOG_WARNING(logger, format, ...) \
  if (::aero::AERO_LOG_LEVEL <= ::aero::LogLevel::WARNING) (logger).warning(format, ##__VA_ARGS__)

#define AERO_LOG_ERROR(logger, format, ...) \
  if (::aero::AERO_LOG_LEVEL <= ::aero::LogLevel::ERROR) (logger).error(format, ##__VA_ARGS__)

#define AERO_LOG_CRITICAL(logger, format, ...) \
  if (::aero::AERO_LOG_LEVEL <= ::aero::LogLevel::CRITICAL) (logger).critical(format, ##__VA_ARGS__)

// ソースファイル位置情報付きロギングマクロ
#define AERO_LOG_TRACE_WITH_POS(logger, format, ...) \
  if (::aero::AERO_LOG_LEVEL <= ::aero::LogLevel::TRACE) (logger).trace_with_pos(__FILE__, __LINE__, format, ##__VA_ARGS__)

#define AERO_LOG_DEBUG_WITH_POS(logger, format, ...) \
  if (::aero::AERO_LOG_LEVEL <= ::aero::LogLevel::DEBUG) (logger).debug_with_pos(__FILE__, __LINE__, format, ##__VA_ARGS__)

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
inline LogLevel stringToLogLevel(const std::string_view levelStr) {
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
 * @return std::string_view 対応する文字列
 */
inline std::string_view logLevelToString(LogLevel level) {
  // 静的配列を使用して変換（高速）
  static constexpr std::array<std::string_view, 7> levelStrings = {
    "TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL", "OFF"
  };
  
  return levelStrings[static_cast<size_t>(level)];
}

/**
 * @brief ログ出力先を表す列挙型
 */
enum class LogTarget {
  CONSOLE,  ///< コンソール出力
  FILE,     ///< ファイル出力
  CALLBACK, ///< コールバック関数
  SYSLOG    ///< システムログ（新規追加）
};

/**
 * @brief ログ出力オプション
 */
struct LoggerOptions {
  LogLevel level = LogLevel::INFO;                        ///< ログレベル
  bool useColors = true;                                  ///< 色付き出力を使用するか
  bool showTimestamp = true;                              ///< タイムスタンプを表示するか
  bool showLevel = true;                                  ///< ログレベルを表示するか
  bool showCategory = true;                               ///< カテゴリを表示するか
  bool showSourceLocation = false;                        ///< ソースの場所を表示するか
  std::string dateTimeFormat = "%Y-%m-%d %H:%M:%S";       ///< 日時フォーマット
  std::string logFilePath;                                ///< ログファイルパス
  bool appendToFile = true;                               ///< ファイルに追記するか
  std::vector<LogTarget> targets = {LogTarget::CONSOLE};  ///< ログ出力先
  size_t maxFileSizeBytes = 10 * 1024 * 1024;             ///< 最大ファイルサイズ（バイト）
  int maxBackupFiles = 3;                                 ///< 最大バックアップファイル数
  bool asyncLogging = false;                              ///< 非同期ロギングを使用するか (新規追加)
  size_t asyncQueueSize = 1024;                           ///< 非同期キューサイズ (新規追加)
  bool logMessageCoalescing = false;                      ///< 類似メッセージの統合 (新規追加)
  size_t bufferSize = 8192;                               ///< 内部バッファサイズ (新規追加)
};

// 文字列フォーマット用の高速実装（C++20のstd::formatの簡易版）
namespace detail {

// 文字列置換の高速実装
template <typename... Args>
inline std::string format_string(const char* format, Args&&... args) {
  // 小さな文字列用にスタック上にバッファを確保（ヒープアロケーション回避）
  constexpr size_t STACK_BUFFER_SIZE = 1024;
  char stackBuffer[STACK_BUFFER_SIZE];
  
  int size = snprintf(stackBuffer, STACK_BUFFER_SIZE, format, std::forward<Args>(args)...);
  
  if (size < 0) {
    return "FORMAT_ERROR";
  }
  
  if (size < STACK_BUFFER_SIZE) {
    return std::string(stackBuffer, size);
  } else {
    // スタックバッファが小さすぎる場合はヒープを使用
    std::string result(size + 1, '\0');
    snprintf(&result[0], size + 1, format, std::forward<Args>(args)...);
    result.resize(size);
    return result;
  }
}

} // namespace detail

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
    
    // 非同期ロギングの設定変更を処理
    if (m_options.asyncLogging != options.asyncLogging) {
      if (options.asyncLogging) {
        startAsyncLogging(options.asyncQueueSize);
      } else {
        stopAsyncLogging();
      }
    }
    
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
  void setLevel(const std::string_view levelStr) {
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
  template <typename... Args>
  void trace(const char* format, Args&&... args) {
    if (AERO_LOG_LEVEL <= LogLevel::TRACE) {
      log(LogLevel::TRACE, format, std::forward<Args>(args)...);
    }
  }

  /**
   * @brief ソース位置情報付きTRACEレベルログを出力
   */
  template <typename... Args>
  void trace_with_pos(const char* file, int line, const char* format, Args&&... args) {
    if (AERO_LOG_LEVEL <= LogLevel::TRACE) {
      std::string message = detail::format_string(format, std::forward<Args>(args)...);
      std::string sourceInfo = detail::format_string("%s:%d ", file, line);
      log_internal(LogLevel::TRACE, (sourceInfo + message).c_str());
    }
  }

  /**
   * @brief DEBUGレベルのログを出力する
   *
   * @param format フォーマット文字列
   * @param args フォーマット引数
   */
  template <typename... Args>
  void debug(const char* format, Args&&... args) {
    if (AERO_LOG_LEVEL <= LogLevel::DEBUG) {
      log(LogLevel::DEBUG, format, std::forward<Args>(args)...);
    }
  }

  /**
   * @brief ソース位置情報付きDEBUGレベルログを出力
   */
  template <typename... Args>
  void debug_with_pos(const char* file, int line, const char* format, Args&&... args) {
    if (AERO_LOG_LEVEL <= LogLevel::DEBUG) {
      std::string message = detail::format_string(format, std::forward<Args>(args)...);
      std::string sourceInfo = detail::format_string("%s:%d ", file, line);
      log_internal(LogLevel::DEBUG, (sourceInfo + message).c_str());
    }
  }

  /**
   * @brief INFOレベルのログを出力する
   *
   * @param format フォーマット文字列
   * @param args フォーマット引数
   */
  template <typename... Args>
  void info(const char* format, Args&&... args) {
    log(LogLevel::INFO, format, std::forward<Args>(args)...);
  }

  /**
   * @brief WARNINGレベルのログを出力する
   *
   * @param format フォーマット文字列
   * @param args フォーマット引数
   */
  template <typename... Args>
  void warning(const char* format, Args&&... args) {
    log(LogLevel::WARNING, format, std::forward<Args>(args)...);
  }

  /**
   * @brief ERRORレベルのログを出力する
   *
   * @param format フォーマット文字列
   * @param args フォーマット引数
   */
  template <typename... Args>
  void error(const char* format, Args&&... args) {
    log(LogLevel::ERROR, format, std::forward<Args>(args)...);
  }

  /**
   * @brief CRITICALレベルのログを出力する
   *
   * @param format フォーマット文字列
   * @param args フォーマット引数
   */
  template <typename... Args>
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
  template <typename... Args>
  void log(LogLevel level, const char* format, Args&&... args) {
    if (level < m_options.level || m_options.level == LogLevel::OFF) {
      return;
    }

    // 最適化された文字列フォーマット処理
    std::string message = detail::format_string(format, std::forward<Args>(args)...);
    log_internal(level, message.c_str());
  }

 private:
  /**
   * @brief 内部ログ処理関数
   */
  void log_internal(LogLevel level, const char* message) {
    std::string formattedMessage = formatLogMessage(level, m_category, message);

    // 非同期ロギングが有効な場合はキューに追加
    if (m_options.asyncLogging && m_asyncLoggingActive) {
      enqueueAsyncLog(level, message, formattedMessage);
    } else {
      // 同期ロギング
      outputLog(level, message, formattedMessage);
    }
  }

  /**
   * @brief コンストラクタ
   *
   * @param category ログカテゴリ
   */
  Logger(const std::string& category)
      : m_category(category), m_asyncLoggingActive(false) {
  }

  /**
   * @brief デストラクタ
   */
  ~Logger() {
    // 非同期ロギングを停止
    if (m_asyncLoggingActive) {
      stopAsyncLogging();
    }
    
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
   * @brief 非同期ロギングを開始
   */
  void startAsyncLogging(size_t queueSize) {
    if (m_asyncLoggingActive) {
      return;
    }
    
    m_asyncLoggingActive = true;
    m_stopAsyncLogging = false;
    m_asyncQueue.reserve(queueSize);
    
    // ロギングスレッドを起動
    m_asyncLoggingThread = std::thread([this]() {
      this->processAsyncLogs();
    });
  }
  
  /**
   * @brief 非同期ロギングを停止
   */
  void stopAsyncLogging() {
    if (!m_asyncLoggingActive) {
      return;
    }
    
    {
      std::lock_guard<std::mutex> lock(m_asyncQueueMutex);
      m_stopAsyncLogging = true;
      m_asyncQueueCV.notify_one();
    }
    
    if (m_asyncLoggingThread.joinable()) {
      m_asyncLoggingThread.join();
    }
    
    m_asyncLoggingActive = false;
  }
  
  /**
   * @brief 非同期ログキューにメッセージを追加
   */
  void enqueueAsyncLog(LogLevel level, const std::string& rawMessage, const std::string& formattedMessage) {
    {
      std::lock_guard<std::mutex> lock(m_asyncQueueMutex);
      
      // ロギングコアレシング（似たメッセージの統合）
      if (m_options.logMessageCoalescing && !m_asyncQueue.empty()) {
        auto& last = m_asyncQueue.back();
        if (last.level == level && last.rawMessage == rawMessage) {
          last.repeatCount++;
          return;
        }
      }
      
      // キューがいっぱいなら古いメッセージを削除
      if (m_asyncQueue.size() >= m_options.asyncQueueSize) {
        m_asyncQueue.erase(m_asyncQueue.begin());
      }
      
      // 新しいメッセージを追加
      m_asyncQueue.push_back({level, m_category, rawMessage, formattedMessage, 1});
    }
    
    // 処理スレッドに通知
    m_asyncQueueCV.notify_one();
  }
  
  /**
   * @brief 非同期ログ処理スレッドのメイン関数
   */
  void processAsyncLogs() {
    while (true) {
      std::vector<AsyncLogEntry> logsToProcess;
      
      {
        std::unique_lock<std::mutex> lock(m_asyncQueueMutex);
        
        // キューが空でストップフラグが立っていなければ待機
        m_asyncQueueCV.wait(lock, [this] {
          return !m_asyncQueue.empty() || m_stopAsyncLogging;
        });
        
        // 終了条件をチェック
        if (m_asyncQueue.empty() && m_stopAsyncLogging) {
          break;
        }
        
        // 現在のキューを処理用にスワップ（ロック時間を最小化）
        logsToProcess.swap(m_asyncQueue);
      }
      
      // ロックを解放してからログを処理
      for (const auto& entry : logsToProcess) {
        std::string repeatedMessage = entry.formattedMessage;
        if (entry.repeatCount > 1) {
          repeatedMessage += detail::format_string(" (repeated %d times)", entry.repeatCount);
        }
        
        outputLog(entry.level, entry.rawMessage.c_str(), repeatedMessage.c_str());
      }
    }
  }

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
    
    // バッファリングを設定（パフォーマンス向上）
    if (m_logFile.is_open()) {
      char* buffer = new char[m_options.bufferSize];
      m_logFile.rdbuf()->pubsetbuf(buffer, m_options.bufferSize);
      m_fileBuffers.reset(buffer); // バッファの自動解放
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
    // 必要なサイズを見積もり、一度の割り当てで済むようにする
    size_t estimatedSize = message.size() + category.size() + 64;
    std::string result;
    result.reserve(estimatedSize);
    
    thread_local char timestampBuffer[64]; // スレッドローカルでタイムスタンプバッファを再利用

    // タイムスタンプ
    if (m_options.showTimestamp) {
      auto now = std::chrono::system_clock::now();
      auto nowTime = std::chrono::system_clock::to_time_t(now);
      std::strftime(timestampBuffer, sizeof(timestampBuffer), 
                    m_options.dateTimeFormat.c_str(), std::localtime(&nowTime));
      result.append(timestampBuffer).append(" ");
    }

    // ログレベル
    if (m_options.showLevel) {
      if (m_options.useColors) {
        result.append(getColorCode(level));
      }

      result.append("[").append(std::string(logLevelToString(level))).append("]");

      if (m_options.useColors) {
        result.append("\033[0m");
      }

      result.append(" ");
    }

    // カテゴリ
    if (m_options.showCategory && !category.empty()) {
      result.append("[").append(category).append("] ");
    }

    // メッセージ
    result.append(message);

    return result;
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
      std::ostream& out = (level >= LogLevel::ERROR) ? std::cerr : std::cout;
      out << formattedMessage << std::endl;
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

    // システムログ出力（新規追加）
    if (isTargetEnabled(LogTarget::SYSLOG)) {
      outputToSyslog(level, formattedMessage);
    }
  }

  /**
   * @brief システムログに出力（プラットフォーム依存）
   */
  void outputToSyslog(LogLevel level, const std::string& message) {
    // プラットフォーム依存の実装
    #ifdef _WIN32
      // Windows Event Log
      // 実装省略
    #elif defined(__APPLE__) || defined(__linux__)
      // syslog
      // 実装省略
    #endif
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
   * @return std::string_view 色コード
   */
  std::string_view getColorCode(LogLevel level) const {
    // 静的配列で色コードを管理（高速アクセス）
    static constexpr std::array<std::string_view, 7> colorCodes = {
      "\033[90m",  // TRACE: 灰色
      "\033[36m",  // DEBUG: シアン
      "\033[32m",  // INFO: 緑
      "\033[33m",  // WARNING: 黄
      "\033[31m",  // ERROR: 赤
      "\033[35m",  // CRITICAL: マゼンタ
      ""           // OFF: なし
    };
    
    return colorCodes[static_cast<size_t>(level)];
  }

  // 非同期ログエントリ構造体
  struct AsyncLogEntry {
    LogLevel level;
    std::string category;
    std::string rawMessage;
    std::string formattedMessage;
    size_t repeatCount;
  };

  std::string m_category;
  LoggerOptions m_options;
  std::mutex m_mutex;
  std::ofstream m_logFile;
  std::function<void(LogLevel, const std::string&, const std::string&)> m_logCallback;
  
  // ファイルバッファ管理
  std::unique_ptr<char[]> m_fileBuffers;
  
  // 非同期ロギング関連
  std::atomic<bool> m_asyncLoggingActive;
  std::atomic<bool> m_stopAsyncLogging;
  std::thread m_asyncLoggingThread;
  std::mutex m_asyncQueueMutex;
  std::condition_variable m_asyncQueueCV;
  std::vector<AsyncLogEntry> m_asyncQueue;
};

}  // namespace aero

#endif  // AERO_LOGGER_H