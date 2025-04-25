#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace aerojs {
namespace core {

/**
 * @brief ログレベルの列挙型
 */
enum class ICLogLevel {
    Debug,      // デバッグ情報（詳細な追跡）
    Info,       // 情報メッセージ（通常の動作）
    Warning,    // 警告メッセージ（潜在的な問題）
    Error,      // エラーメッセージ（回復可能な問題）
    Critical    // 重大エラー（回復不可能な問題）
};

/**
 * @brief ログエントリの構造体
 */
struct ICLogEntry {
    ICLogLevel level;                              // ログレベル
    std::string message;                           // ログメッセージ
    std::string category;                          // ログカテゴリ
    std::string source;                            // ログソース (ファイル名、関数名など)
    std::string threadId;                          // スレッドID
    std::chrono::system_clock::time_point timestamp; // タイムスタンプ

    /**
     * @brief コンストラクタ
     * @param lvl ログレベル
     * @param msg ログメッセージ
     * @param cat ログカテゴリ
     * @param src ログソース
     */
    ICLogEntry(
        ICLogLevel lvl = ICLogLevel::Info,
        const std::string& msg = "",
        const std::string& cat = "",
        const std::string& src = "")
        : level(lvl)
        , message(msg)
        , category(cat)
        , source(src)
        , timestamp(std::chrono::system_clock::now()) {
        
        // スレッドIDを文字列に変換
        std::stringstream ss;
        ss << std::this_thread::get_id();
        threadId = ss.str();
    }
};

/**
 * @brief ログシンクのインターフェース
 * 
 * ログシンクは、ログエントリを特定の出力先（コンソール、ファイル、メモリなど）に書き込むための
 * 抽象インターフェースです。
 */
class ICLogSink {
public:
    /**
     * @brief 仮想デストラクタ
     */
    virtual ~ICLogSink() = default;
    
    /**
     * @brief ログエントリを書き込む
     * @param entry 書き込むログエントリ
     */
    virtual void Write(const ICLogEntry& entry) = 0;
    
    /**
     * @brief バッファをフラッシュする
     */
    virtual void Flush() = 0;
    
    /**
     * @brief シンクの名前を取得する
     * @return シンクの名前
     */
    virtual std::string GetName() const = 0;
};

// コンソールへのログ出力用のANSIカラーコード
constexpr const char* RESET_COLOR = "\033[0m";

/**
 * @brief コンソール出力用のログシンク
 */
class ICConsoleLogSink : public ICLogSink {
public:
    /**
     * @brief コンストラクタ
     * @param useColors 色付き出力を使用するかどうか
     */
    explicit ICConsoleLogSink(bool useColors = true);
    
    /**
     * @brief ログエントリをコンソールに書き込む
     * @param entry 書き込むログエントリ
     */
    void Write(const ICLogEntry& entry) override;
    
    /**
     * @brief コンソール出力をフラッシュする
     */
    void Flush() override;
    
    /**
     * @brief シンクの名前を取得する
     * @return シンクの名前
     */
    std::string GetName() const override;
    
    /**
     * @brief 色付き出力の設定を変更する
     * @param useColors 色付き出力を使用するかどうか
     */
    void SetUseColors(bool useColors);
    
private:
    /**
     * @brief ログレベルに応じたカラーコードを取得する
     * @param level ログレベル
     * @return ANSIカラーコード
     */
    std::string GetColorCode(ICLogLevel level) const;
    
    bool m_useColors;           // 色付き出力を使用するかどうか
    mutable std::mutex m_consoleMutex; // コンソール出力の排他制御用ミューテックス
};

/**
 * @brief ファイル出力用のログシンク
 */
class ICFileLogSink : public ICLogSink {
public:
    /**
     * @brief コンストラクタ
     * @param filePath ログファイルのパス
     * @param append 既存ファイルに追記するかどうか
     */
    ICFileLogSink(const std::string& filePath, bool append = true);
    
    /**
     * @brief デストラクタ
     */
    ~ICFileLogSink() override;
    
    /**
     * @brief ログエントリをファイルに書き込む
     * @param entry 書き込むログエントリ
     */
    void Write(const ICLogEntry& entry) override;
    
    /**
     * @brief ファイル出力をフラッシュする
     */
    void Flush() override;
    
    /**
     * @brief シンクの名前を取得する
     * @return シンクの名前
     */
    std::string GetName() const override;
    
    /**
     * @brief 出力先ファイルを切り替える
     * @param newFilePath 新しいファイルパス
     * @param append 既存ファイルに追記するかどうか
     * @return 成功したかどうか
     */
    bool SwitchFile(const std::string& newFilePath, bool append = true);
    
    /**
     * @brief 現在のファイルパスを取得する
     * @return ファイルパス
     */
    std::string GetFilePath() const;
    
    /**
     * @brief 現在のファイルサイズを取得する
     * @return ファイルサイズ（バイト単位）
     */
    size_t GetFileSize() const;
    
    /**
     * @brief ファイルローテーションのサイズを設定する
     * @param maxSizeBytes 最大サイズ（バイト単位）
     */
    void SetRotationSize(size_t maxSizeBytes);
    
    /**
     * @brief 保持する最大ローテーションファイル数を設定する
     * @param maxFiles 保持する最大ファイル数
     */
    void SetMaxRotationFiles(size_t maxFiles);
    
private:
    /**
     * @brief 必要に応じてファイルローテーションを実行する
     */
    void RotateLogFileIfNeeded();
    
    /**
     * @brief 古いローテーションファイルをクリーンアップする
     */
    void CleanupRotatedFiles();
    
    std::string m_filePath;         // ログファイルのパス
    std::ofstream m_fileStream;      // ファイルストリーム
    size_t m_rotationSizeBytes;     // ローテーションを行うサイズの閾値
    size_t m_maxRotationFiles;      // 保持する最大ローテーションファイル数
    mutable std::mutex m_fileMutex; // ファイル操作の排他制御用ミューテックス
};

/**
 * @brief カスタムコールバック用のログシンク
 */
class ICCustomLogSink : public ICLogSink {
public:
    using LogCallback = std::function<void(const ICLogEntry&)>;
    using FlushCallback = std::function<void()>;
    
    /**
     * @brief コンストラクタ
     * @param name シンクの名前
     * @param logCallback ログエントリを処理するコールバック関数
     * @param flushCallback フラッシュ時に呼び出されるコールバック関数
     */
    ICCustomLogSink(
        const std::string& name,
        LogCallback logCallback,
        FlushCallback flushCallback = nullptr);
    
    /**
     * @brief ログエントリをコールバック関数に渡す
     * @param entry 処理するログエントリ
     */
    void Write(const ICLogEntry& entry) override;
    
    /**
     * @brief フラッシュコールバックを呼び出す
     */
    void Flush() override;
    
    /**
     * @brief シンクの名前を取得する
     * @return シンクの名前
     */
    std::string GetName() const override;
    
private:
    std::string m_name;              // シンクの名前
    LogCallback m_logCallback;       // ログ処理コールバック
    FlushCallback m_flushCallback;   // フラッシュコールバック
    mutable std::mutex m_callbackMutex; // コールバック呼び出しの排他制御用ミューテックス
};

/**
 * @brief メモリ保持用のログシンク
 */
class ICMemoryLogSink : public ICLogSink {
public:
    /**
     * @brief コンストラクタ
     * @param maxEntries 保持する最大エントリ数
     */
    explicit ICMemoryLogSink(size_t maxEntries = 1000);
    
    /**
     * @brief ログエントリをメモリに保存する
     * @param entry 保存するログエントリ
     */
    void Write(const ICLogEntry& entry) override;
    
    /**
     * @brief メモリシンクのフラッシュ（何もしない）
     */
    void Flush() override;
    
    /**
     * @brief シンクの名前を取得する
     * @return シンクの名前
     */
    std::string GetName() const override;
    
    /**
     * @brief 保持する最大エントリ数を設定する
     * @param maxEntries 最大エントリ数
     */
    void SetMaxEntries(size_t maxEntries);
    
    /**
     * @brief 保持しているすべてのエントリを取得する
     * @return エントリのベクター
     */
    std::vector<ICLogEntry> GetEntries() const;
    
    /**
     * @brief 指定したレベル以上のエントリを取得する
     * @param minLevel 最小ログレベル
     * @return エントリのベクター
     */
    std::vector<ICLogEntry> GetEntriesByLevel(ICLogLevel minLevel) const;
    
    /**
     * @brief 指定したカテゴリのエントリを取得する
     * @param category カテゴリ
     * @return エントリのベクター
     */
    std::vector<ICLogEntry> GetEntriesByCategory(const std::string& category) const;
    
    /**
     * @brief すべてのエントリをクリアする
     */
    void Clear();
    
private:
    size_t m_maxEntries;                 // 保持する最大エントリ数
    std::vector<ICLogEntry> m_entries;   // 保存されたエントリ
    mutable std::mutex m_bufferMutex;    // バッファアクセスの排他制御用ミューテックス
};

/**
 * @brief 非同期ロギング用のキュークラス
 */
class ICLogQueue {
public:
    /**
     * @brief コンストラクタ
     */
    ICLogQueue();
    
    /**
     * @brief デストラクタ
     */
    ~ICLogQueue();
    
    /**
     * @brief ログエントリをキューに追加する
     * @param entry 追加するエントリ
     */
    void EnqueueEntry(const ICLogEntry& entry);
    
    /**
     * @brief キューからエントリを取得する
     * @param entry 取得したエントリを格納する変数
     * @return エントリが取得できたかどうか
     */
    bool DequeueEntry(ICLogEntry& entry);
    
    /**
     * @brief キュー処理を開始する
     */
    void Start();
    
    /**
     * @brief キュー処理を停止する
     */
    void Stop();
    
    /**
     * @brief キューが空かどうかを確認する
     * @return キューが空の場合true
     */
    bool IsEmpty() const;
    
    /**
     * @brief キューのサイズを取得する
     * @return キュー内のエントリ数
     */
    size_t GetSize() const;
    
private:
    /**
     * @brief キューを処理するワーカースレッド関数
     */
    void ProcessQueue();
    
    std::queue<ICLogEntry> m_queue;           // ログエントリのキュー
    std::thread m_workerThread;               // ワーカースレッド
    std::condition_variable m_condition;      // 条件変数
    std::mutex m_queueMutex;                  // キューアクセスの排他制御用ミューテックス
    bool m_running;                           // スレッド実行中フラグ
};

/**
 * @brief インラインキャッシュ用ロガーシングルトンクラス
 */
class ICLogger {
public:
    /**
     * @brief インスタンスを取得する
     * @return ロガーのシングルトンインスタンス
     */
    static ICLogger& Instance();
    
    /**
     * @brief デストラクタ
     */
    ~ICLogger();
    
    /**
     * @brief ログをスレッドセーフに出力する
     * @param level ログレベル
     * @param message ログメッセージ
     * @param category ログカテゴリ
     * @param source ログソース
     */
    void Log(
        ICLogLevel level,
        const std::string& message,
        const std::string& category = "",
        const std::string& source = "");
    
    /**
     * @brief デバッグレベルのログを出力する
     * @param message ログメッセージ
     * @param category ログカテゴリ
     * @param source ログソース
     */
    void Debug(
        const std::string& message,
        const std::string& category = "",
        const std::string& source = "");
    
    /**
     * @brief 情報レベルのログを出力する
     * @param message ログメッセージ
     * @param category ログカテゴリ
     * @param source ログソース
     */
    void Info(
        const std::string& message,
        const std::string& category = "",
        const std::string& source = "");
    
    /**
     * @brief 警告レベルのログを出力する
     * @param message ログメッセージ
     * @param category ログカテゴリ
     * @param source ログソース
     */
    void Warning(
        const std::string& message,
        const std::string& category = "",
        const std::string& source = "");
    
    /**
     * @brief エラーレベルのログを出力する
     * @param message ログメッセージ
     * @param category ログカテゴリ
     * @param source ログソース
     */
    void Error(
        const std::string& message,
        const std::string& category = "",
        const std::string& source = "");
    
    /**
     * @brief 重大エラーレベルのログを出力する
     * @param message ログメッセージ
     * @param category ログカテゴリ
     * @param source ログソース
     */
    void Critical(
        const std::string& message,
        const std::string& category = "",
        const std::string& source = "");
    
    /**
     * @brief ログシンクを追加する
     * @param sink 追加するシンク
     */
    void AddSink(std::shared_ptr<ICLogSink> sink);
    
    /**
     * @brief ログシンクを削除する
     * @param sinkName 削除するシンクの名前
     * @return 削除が成功したかどうか
     */
    bool RemoveSink(const std::string& sinkName);
    
    /**
     * @brief 指定した名前のログシンクを取得する
     * @param sinkName シンクの名前
     * @return シンクのポインタ（見つからない場合はnullptr）
     */
    std::shared_ptr<ICLogSink> GetSink(const std::string& sinkName);
    
    /**
     * @brief すべてのログシンクの名前を取得する
     * @return シンク名のベクター
     */
    std::vector<std::string> GetSinkNames() const;
    
    /**
     * @brief すべてのログシンクをフラッシュする
     */
    void FlushAllSinks();
    
    /**
     * @brief 最小ログレベルを設定する
     * @param level 最小ログレベル
     */
    void SetMinLogLevel(ICLogLevel level);
    
    /**
     * @brief 現在の最小ログレベルを取得する
     * @return 最小ログレベル
     */
    ICLogLevel GetMinLogLevel() const;
    
    /**
     * @brief カテゴリフィルタを追加する
     * @param category 追加するカテゴリ
     */
    void AddCategoryFilter(const std::string& category);
    
    /**
     * @brief カテゴリフィルタを削除する
     * @param category 削除するカテゴリ
     */
    void RemoveCategoryFilter(const std::string& category);
    
    /**
     * @brief すべてのカテゴリフィルタをクリアする
     */
    void ClearCategoryFilters();
    
    /**
     * @brief カテゴリが有効かどうかを確認する
     * @param category 確認するカテゴリ
     * @return カテゴリが有効な場合true
     */
    bool IsCategoryEnabled(const std::string& category) const;
    
    /**
     * @brief 非同期ロギングを有効/無効にする
     * @param enable 有効にする場合true
     */
    void EnableAsyncLogging(bool enable);
    
    /**
     * @brief 非同期ロギングが有効かどうかを確認する
     * @return 非同期ロギングが有効な場合true
     */
    bool IsAsyncLoggingEnabled() const;
    
    /**
     * @brief デフォルトのログシンクをセットアップする
     * @param console コンソール出力を有効にするかどうか
     * @param logFilePath ログファイルのパス（空文字列の場合はファイル出力を無効にする）
     */
    void SetupDefaultSinks(bool console = true, const std::string& logFilePath = "");
    
    /**
     * @brief ログフォーマットを設定する
     * @param format ログフォーマット
     */
    void SetLogFormat(const std::string& format);
    
    /**
     * @brief 現在のログフォーマットを取得する
     * @return ログフォーマット
     */
    std::string GetLogFormat() const;
    
    /**
     * @brief ログメッセージをフォーマットする
     * @param entry フォーマットするログエントリ
     * @return フォーマットされたメッセージ
     */
    std::string FormatLogMessage(const ICLogEntry& entry) const;
    
    /**
     * @brief ログエントリを処理する
     * @param entry 処理するログエントリ
     */
    void ProcessLogEntry(const ICLogEntry& entry);
    
    /**
     * @brief ログレベルを文字列に変換する
     * @param level 変換するログレベル
     * @return ログレベルの文字列表現
     */
    static std::string LogLevelToString(ICLogLevel level);
    
    /**
     * @brief 文字列をログレベルに変換する
     * @param levelStr 変換する文字列
     * @return ログレベル
     */
    static ICLogLevel StringToLogLevel(const std::string& levelStr);
    
private:
    /**
     * @brief コンストラクタ（シングルトン）
     */
    ICLogger();
    
    /**
     * @brief タイムスタンプをフォーマットする
     * @param timestamp フォーマットするタイムスタンプ
     * @return フォーマットされた文字列
     */
    std::string FormatTimestamp(const std::chrono::system_clock::time_point& timestamp) const;
    
    // コピー・ムーブを禁止
    ICLogger(const ICLogger&) = delete;
    ICLogger& operator=(const ICLogger&) = delete;
    ICLogger(ICLogger&&) = delete;
    ICLogger& operator=(ICLogger&&) = delete;
    
    ICLogLevel m_minLogLevel;                       // 最小ログレベル
    std::vector<std::shared_ptr<ICLogSink>> m_sinks; // ログシンク
    std::map<std::string, bool> m_categoryFilters;  // カテゴリフィルタ
    std::string m_logFormat;                        // ログフォーマット
    bool m_asyncLoggingEnabled;                     // 非同期ロギングが有効かどうか
    std::unique_ptr<ICLogQueue> m_logQueue;         // 非同期ログキュー
    
    mutable std::mutex m_sinksMutex;    // シンクアクセスの排他制御用ミューテックス
    mutable std::mutex m_filtersMutex;  // フィルタアクセスの排他制御用ミューテックス
    mutable std::mutex m_formatMutex;   // フォーマットアクセスの排他制御用ミューテックス
};

} // namespace core
} // namespace aerojs