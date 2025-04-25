#pragma once

#include <chrono>
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>
#include <atomic>
#include "ic_logger.h"

namespace aerojs {
namespace core {

// キャッシュアクセス結果
enum class ICAccessResult {
    Hit,             // キャッシュヒット
    Miss,            // キャッシュミス
    Invalidated,     // 無効化されたエントリ
    Overflow,        // キャッシュオーバーフロー
    TypeError,       // 型エラー
    Unknown          // 不明なエラー
};

// キャッシュのタイプ
enum class ICType {
    Property,        // プロパティアクセス用IC
    Method,          // メソッド呼び出し用IC
    Constructor,     // コンストラクタ呼び出し用IC
    Prototype,       // プロトタイプチェーンアクセス用IC
    Polymorphic,     // ポリモーフィックIC
    Megamorphic,     // メガモーフィックIC
    Global,          // グローバル変数アクセス用IC
    Builtin,         // ビルトイン関数用IC
    Other            // その他
};

// キャッシュアクセス統計情報
struct ICAccessStats {
    std::atomic<uint64_t> hits;          // ヒット数
    std::atomic<uint64_t> misses;        // ミス数
    std::atomic<uint64_t> invalidations; // 無効化回数
    std::atomic<uint64_t> overflows;     // オーバーフロー回数
    std::atomic<uint64_t> typeErrors;    // 型エラー回数
    std::atomic<uint64_t> unknownErrors; // 不明なエラー回数
    std::atomic<uint64_t> totalTime;     // 累積アクセス時間（ナノ秒）
    std::atomic<uint64_t> accessCount;   // 総アクセス回数
    
    ICAccessStats()
        : hits(0), misses(0), invalidations(0), overflows(0),
          typeErrors(0), unknownErrors(0), totalTime(0), accessCount(0) {}
    
    // 統計情報のリセット
    void Reset() {
        hits = 0;
        misses = 0;
        invalidations = 0;
        overflows = 0;
        typeErrors = 0;
        unknownErrors = 0;
        totalTime = 0;
        accessCount = 0;
    }
    
    // 統計情報の合算
    ICAccessStats& operator+=(const ICAccessStats& other) {
        hits += other.hits.load();
        misses += other.misses.load();
        invalidations += other.invalidations.load();
        overflows += other.overflows.load();
        typeErrors += other.typeErrors.load();
        unknownErrors += other.unknownErrors.load();
        totalTime += other.totalTime.load();
        accessCount += other.accessCount.load();
        return *this;
    }
};

// キャッシュアクセス履歴エントリ
struct ICAccessHistoryEntry {
    std::chrono::system_clock::time_point timestamp;
    ICAccessResult result;
    uint64_t accessTime;  // ナノ秒
    std::string locationInfo;
    
    ICAccessHistoryEntry(
        ICAccessResult res,
        uint64_t time,
        const std::string& location)
        : timestamp(std::chrono::system_clock::now()),
          result(res),
          accessTime(time),
          locationInfo(location) {}
};

// パフォーマンスアドバイスの項目
struct ICPerformanceAdvice {
    std::string advice;
    double impact;  // 0.0から1.0までの影響度
    std::string codeLocation;
    std::string explanation;
    
    ICPerformanceAdvice(
        const std::string& adv,
        double imp,
        const std::string& loc,
        const std::string& expl)
        : advice(adv),
          impact(imp),
          codeLocation(loc),
          explanation(expl) {}
    
    // 影響度によるソート用の比較演算子
    bool operator<(const ICPerformanceAdvice& other) const {
        return impact > other.impact;  // 高い影響度が先にくるように降順ソート
    }
};

// インラインキャッシュのパフォーマンス分析を行うクラス
class ICPerformanceAnalyzer {
public:
    // シングルトンインスタンスの取得
    static ICPerformanceAnalyzer& Instance();
    
    // デストラクタ
    ~ICPerformanceAnalyzer();
    
    // キャッシュアクセスの記録
    void RecordAccess(
        const std::string& cacheId,
        ICType type,
        ICAccessResult result,
        uint64_t accessTime,
        const std::string& locationInfo);
    
    // 即時の統計情報を返す
    ICAccessStats GetStatsForCache(const std::string& cacheId) const;
    
    // すべてのキャッシュの統計情報を統合して返す
    ICAccessStats GetAggregateStats() const;
    
    // すべてのキャッシュIDのリストを取得
    std::vector<std::string> GetAllCacheIds() const;
    
    // 特定のキャッシュタイプの統計情報を取得
    ICAccessStats GetStatsByType(ICType type) const;
    
    // すべてのキャッシュタイプの統計情報を取得
    std::unordered_map<ICType, ICAccessStats> GetStatsByAllTypes() const;
    
    // パフォーマンス分析レポートの生成
    std::string GenerateReport(bool detailed = false) const;
    
    // JSON形式のレポート生成
    std::string GenerateJsonReport() const;
    
    // キャッシュのヒット率を計算
    double CalculateHitRate(const std::string& cacheId) const;
    
    // 全体のヒット率を計算
    double CalculateOverallHitRate() const;
    
    // 平均アクセス時間を計算
    double CalculateAverageAccessTime(const std::string& cacheId) const;
    
    // 全体の平均アクセス時間を計算
    double CalculateOverallAverageAccessTime() const;
    
    // 特定のキャッシュIDのアクセス履歴を取得
    std::vector<ICAccessHistoryEntry> GetAccessHistory(
        const std::string& cacheId,
        size_t maxEntries = 100) const;
    
    // 統計情報のリセット
    void ResetStats();
    
    // 特定のキャッシュの統計情報をリセット
    void ResetStatsForCache(const std::string& cacheId);
    
    // 履歴保持の有効化/無効化
    void EnableHistoryTracking(bool enable);
    
    // 履歴保持の最大エントリ数を設定
    void SetMaxHistoryEntries(size_t maxEntries);
    
    // パフォーマンス問題の検出とアドバイスの生成
    std::vector<ICPerformanceAdvice> GeneratePerformanceAdvice() const;
    
    // 特定のキャッシュIDのパフォーマンスアドバイスを生成
    std::vector<ICPerformanceAdvice> GeneratePerformanceAdviceForCache(
        const std::string& cacheId) const;
    
    // キャッシュタイプ別のパフォーマンスアドバイスを生成
    std::vector<ICPerformanceAdvice> GeneratePerformanceAdviceByType(
        ICType type) const;
    
    // キャッシュの重要度を計算（アクセス頻度やミス率に基づく）
    double CalculateCacheImportance(const std::string& cacheId) const;
    
    // ロギングの有効化/無効化
    void EnableLogging(bool enable);
    
    // パフォーマンス測定間隔の設定（ミリ秒）
    void SetPerformanceSamplingInterval(uint64_t intervalMs);
    
    // コールバック関数の登録（パフォーマンス閾値を超えた場合に呼び出される）
    void RegisterPerformanceThresholdCallback(
        std::function<void(const std::string&, double)> callback,
        double hitRateThreshold);
    
    // アラート発生閾値の設定
    void SetAlertThresholds(double minHitRate, double maxAvgAccessTime);
    
    // ICタイプを文字列に変換
    static std::string ICTypeToString(ICType type);
    
    // ICアクセス結果を文字列に変換
    static std::string ICAccessResultToString(ICAccessResult result);

private:
    // シングルトンパターンのための非公開コンストラクタ
    ICPerformanceAnalyzer();
    
    // コピー禁止
    ICPerformanceAnalyzer(const ICPerformanceAnalyzer&) = delete;
    ICPerformanceAnalyzer& operator=(const ICPerformanceAnalyzer&) = delete;
    
    // キャッシュ別の統計情報
    mutable std::mutex m_statsMutex;
    std::unordered_map<std::string, ICAccessStats> m_cacheStats;
    
    // キャッシュタイプ別の統計情報
    std::unordered_map<ICType, ICAccessStats> m_typeStats;
    
    // キャッシュ別のアクセス履歴
    mutable std::mutex m_historyMutex;
    std::unordered_map<std::string, std::vector<ICAccessHistoryEntry>> m_accessHistory;
    
    // 設定
    bool m_historyTrackingEnabled;
    size_t m_maxHistoryEntries;
    bool m_loggingEnabled;
    uint64_t m_performanceSamplingInterval;
    
    // アラート閾値
    double m_minHitRateThreshold;
    double m_maxAvgAccessTimeThreshold;
    
    // パフォーマンスコールバック
    std::vector<std::pair<std::function<void(const std::string&, double)>, double>> m_performanceCallbacks;
    
    // ロギング
    void LogPerformanceIssue(const std::string& cacheId, const std::string& message);
    
    // パフォーマンス問題の検出
    void DetectPerformanceIssues(const std::string& cacheId, const ICAccessStats& stats);
    
    // アドバイス生成のためのヘルパーメソッド
    std::vector<ICPerformanceAdvice> GenerateAdviceForLowHitRate(
        const std::string& cacheId,
        double hitRate,
        const std::string& locationInfo) const;
    
    std::vector<ICPerformanceAdvice> GenerateAdviceForHighInvalidationRate(
        const std::string& cacheId,
        double invalidationRate,
        const std::string& locationInfo) const;
    
    std::vector<ICPerformanceAdvice> GenerateAdviceForTypeErrors(
        const std::string& cacheId,
        double typeErrorRate,
        const std::string& locationInfo) const;
};

// キャッシュアクセス時間測定のユーティリティクラス
class ICAccessTimer {
public:
    // コンストラクタ：測定開始
    ICAccessTimer(
        const std::string& cacheId,
        ICType type,
        const std::string& locationInfo);
    
    // デストラクタ：測定終了と記録
    ~ICAccessTimer();
    
    // アクセス結果の設定
    void SetResult(ICAccessResult result);

private:
    std::string m_cacheId;
    ICType m_type;
    ICAccessResult m_result;
    std::string m_locationInfo;
    std::chrono::high_resolution_clock::time_point m_startTime;
    bool m_stopped;
};

} // namespace core
} // namespace aerojs 