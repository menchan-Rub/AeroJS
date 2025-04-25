#include "ic_performance_analyzer.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <chrono>
#include <json.hpp>

namespace aerojs {
namespace core {

//==============================================================================
// ICPerformanceAnalyzer シングルトン実装
//==============================================================================

ICPerformanceAnalyzer& ICPerformanceAnalyzer::Instance() {
    static ICPerformanceAnalyzer instance;
    return instance;
}

ICPerformanceAnalyzer::ICPerformanceAnalyzer()
    : m_historyTrackingEnabled(true)
    , m_maxHistoryEntries(1000)
    , m_loggingEnabled(true)
    , m_performanceSamplingInterval(1000)  // デフォルトは1秒
    , m_minHitRateThreshold(0.8)          // 80%未満のヒット率でアラート
    , m_maxAvgAccessTimeThreshold(500.0)  // 500ナノ秒以上でアラート
{
    // ロガーの設定
    ICLogger::Instance().SetupDefaultSinks(true);
    ICLogger::Instance().SetMinLogLevel(ICLogLevel::Info);
    ICLogger::Instance().Debug("ICPerformanceAnalyzer initialized", "ICPerformance");
}

ICPerformanceAnalyzer::~ICPerformanceAnalyzer() {
    // クリーンアップ処理
    std::lock_guard<std::mutex> statsLock(m_statsMutex);
    std::lock_guard<std::mutex> historyLock(m_historyMutex);
    
    m_cacheStats.clear();
    m_typeStats.clear();
    m_accessHistory.clear();
    
    if (m_loggingEnabled) {
        ICLogger::Instance().Info("ICPerformanceAnalyzer destroyed", "ICPerformance");
    }
}

//==============================================================================
// データ記録メソッド
//==============================================================================

void ICPerformanceAnalyzer::RecordAccess(
    const std::string& cacheId,
    ICType type,
    ICAccessResult result,
    uint64_t accessTime,
    const std::string& locationInfo) 
{
    // 統計情報の更新
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        
        // キャッシュIDごとの統計
        auto& stats = m_cacheStats[cacheId];
        stats.accessCount++;
        stats.totalTime += accessTime;
        
        // キャッシュタイプごとの統計
        auto& typeStats = m_typeStats[type];
        typeStats.accessCount++;
        typeStats.totalTime += accessTime;
        
        // 結果に基づいて適切なカウンタを増加
        switch (result) {
            case ICAccessResult::Hit:
                stats.hits++;
                typeStats.hits++;
                break;
            case ICAccessResult::Miss:
                stats.misses++;
                typeStats.misses++;
                break;
            case ICAccessResult::Invalidated:
                stats.invalidations++;
                typeStats.invalidations++;
                break;
            case ICAccessResult::Overflow:
                stats.overflows++;
                typeStats.overflows++;
                break;
            case ICAccessResult::TypeError:
                stats.typeErrors++;
                typeStats.typeErrors++;
                break;
            case ICAccessResult::Unknown:
                stats.unknownErrors++;
                typeStats.unknownErrors++;
                break;
        }
    }
    
    // 履歴の記録（有効な場合）
    if (m_historyTrackingEnabled) {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        
        auto& history = m_accessHistory[cacheId];
        history.emplace_back(result, accessTime, locationInfo);
        
        // 最大エントリ数を超えた場合、古いエントリを削除
        if (history.size() > m_maxHistoryEntries) {
            history.erase(history.begin());
        }
    }
    
    // パフォーマンス問題の検出とロギング
    if (m_loggingEnabled) {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        DetectPerformanceIssues(cacheId, m_cacheStats[cacheId]);
    }
}

//==============================================================================
// 統計情報取得メソッド
//==============================================================================

ICAccessStats ICPerformanceAnalyzer::GetStatsForCache(const std::string& cacheId) const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    auto it = m_cacheStats.find(cacheId);
    if (it != m_cacheStats.end()) {
        return it->second;
    }
    
    return ICAccessStats(); // 該当するキャッシュIDが見つからない場合は空の統計を返す
}

ICAccessStats ICPerformanceAnalyzer::GetAggregateStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    ICAccessStats aggregateStats;
    for (const auto& pair : m_cacheStats) {
        aggregateStats += pair.second;
    }
    
    return aggregateStats;
}

std::vector<std::string> ICPerformanceAnalyzer::GetAllCacheIds() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    std::vector<std::string> cacheIds;
    cacheIds.reserve(m_cacheStats.size());
    
    for (const auto& pair : m_cacheStats) {
        cacheIds.push_back(pair.first);
    }
    
    return cacheIds;
}

ICAccessStats ICPerformanceAnalyzer::GetStatsByType(ICType type) const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    auto it = m_typeStats.find(type);
    if (it != m_typeStats.end()) {
        return it->second;
    }
    
    return ICAccessStats(); // 該当するタイプが見つからない場合は空の統計を返す
}

std::unordered_map<ICType, ICAccessStats> ICPerformanceAnalyzer::GetStatsByAllTypes() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_typeStats;
}

//==============================================================================
// 計算メソッド
//==============================================================================

double ICPerformanceAnalyzer::CalculateHitRate(const std::string& cacheId) const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    auto it = m_cacheStats.find(cacheId);
    if (it != m_cacheStats.end()) {
        const ICAccessStats& stats = it->second;
        uint64_t totalAccesses = stats.hits + stats.misses;
        if (totalAccesses > 0) {
            return static_cast<double>(stats.hits) / totalAccesses;
        }
    }
    
    return 0.0; // アクセスがない場合は0%のヒット率
}

double ICPerformanceAnalyzer::CalculateOverallHitRate() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    uint64_t totalHits = 0;
    uint64_t totalAccesses = 0;
    
    for (const auto& pair : m_cacheStats) {
        const ICAccessStats& stats = pair.second;
        totalHits += stats.hits;
        totalAccesses += stats.hits + stats.misses;
    }
    
    if (totalAccesses > 0) {
        return static_cast<double>(totalHits) / totalAccesses;
    }
    
    return 0.0; // アクセスがない場合は0%のヒット率
}

double ICPerformanceAnalyzer::CalculateAverageAccessTime(const std::string& cacheId) const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    auto it = m_cacheStats.find(cacheId);
    if (it != m_cacheStats.end()) {
        const ICAccessStats& stats = it->second;
        if (stats.accessCount > 0) {
            return static_cast<double>(stats.totalTime) / stats.accessCount;
        }
    }
    
    return 0.0; // アクセスがない場合は0ナノ秒
}

double ICPerformanceAnalyzer::CalculateOverallAverageAccessTime() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    uint64_t totalTime = 0;
    uint64_t totalAccesses = 0;
    
    for (const auto& pair : m_cacheStats) {
        const ICAccessStats& stats = pair.second;
        totalTime += stats.totalTime;
        totalAccesses += stats.accessCount;
    }
    
    if (totalAccesses > 0) {
        return static_cast<double>(totalTime) / totalAccesses;
    }
    
    return 0.0; // アクセスがない場合は0ナノ秒
}

//==============================================================================
// 履歴アクセスメソッド
//==============================================================================

std::vector<ICAccessHistoryEntry> ICPerformanceAnalyzer::GetAccessHistory(
    const std::string& cacheId,
    size_t maxEntries) const 
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    
    std::vector<ICAccessHistoryEntry> result;
    
    auto it = m_accessHistory.find(cacheId);
    if (it != m_accessHistory.end()) {
        const auto& history = it->second;
        
        size_t count = std::min(maxEntries, history.size());
        result.reserve(count);
        
        // 最新のエントリから指定数を取得
        auto startIt = history.end() - count;
        result.assign(startIt, history.end());
    }
    
    return result;
}

//==============================================================================
// 設定メソッド
//==============================================================================

void ICPerformanceAnalyzer::ResetStats() {
    std::lock_guard<std::mutex> statsLock(m_statsMutex);
    std::lock_guard<std::mutex> historyLock(m_historyMutex);
    
    // 全キャッシュの統計情報をリセット
    for (auto& pair : m_cacheStats) {
        pair.second.Reset();
    }
    
    // タイプ別統計情報をリセット
    for (auto& pair : m_typeStats) {
        pair.second.Reset();
    }
    
    // 履歴もクリア
    m_accessHistory.clear();
    
    if (m_loggingEnabled) {
        ICLogger::Instance().Info("All performance statistics have been reset", "ICPerformance");
    }
}

void ICPerformanceAnalyzer::ResetStatsForCache(const std::string& cacheId) {
    std::lock_guard<std::mutex> statsLock(m_statsMutex);
    std::lock_guard<std::mutex> historyLock(m_historyMutex);
    
    // 指定キャッシュの統計情報をリセット
    auto statsIt = m_cacheStats.find(cacheId);
    if (statsIt != m_cacheStats.end()) {
        statsIt->second.Reset();
    }
    
    // 指定キャッシュの履歴もクリア
    auto historyIt = m_accessHistory.find(cacheId);
    if (historyIt != m_accessHistory.end()) {
        historyIt->second.clear();
    }
    
    if (m_loggingEnabled) {
        ICLogger::Instance().Info("Reset statistics for cache: " + cacheId, "ICPerformance");
    }
}

void ICPerformanceAnalyzer::EnableHistoryTracking(bool enable) {
    m_historyTrackingEnabled = enable;
    
    if (m_loggingEnabled) {
        std::string status = enable ? "enabled" : "disabled";
        ICLogger::Instance().Info("History tracking " + status, "ICPerformance");
    }
    
    // 無効化する場合、既存の履歴をクリア
    if (!enable) {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        m_accessHistory.clear();
    }
}

void ICPerformanceAnalyzer::SetMaxHistoryEntries(size_t maxEntries) {
    m_maxHistoryEntries = maxEntries;
    
    if (m_loggingEnabled) {
        ICLogger::Instance().Info(
            "Max history entries set to " + std::to_string(maxEntries), 
            "ICPerformance");
    }
    
    // 現在の履歴エントリを調整
    if (m_historyTrackingEnabled) {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        for (auto& pair : m_accessHistory) {
            auto& history = pair.second;
            if (history.size() > maxEntries) {
                history.erase(history.begin(), history.begin() + (history.size() - maxEntries));
            }
        }
    }
}

void ICPerformanceAnalyzer::EnableLogging(bool enable) {
    m_loggingEnabled = enable;
    
    // ロギングを有効にしたときだけログを出力
    if (enable) {
        ICLogger::Instance().Info("Performance logging enabled", "ICPerformance");
    }
}

void ICPerformanceAnalyzer::SetPerformanceSamplingInterval(uint64_t intervalMs) {
    m_performanceSamplingInterval = intervalMs;
    
    if (m_loggingEnabled) {
        ICLogger::Instance().Info(
            "Performance sampling interval set to " + std::to_string(intervalMs) + " ms",
            "ICPerformance");
    }
}

void ICPerformanceAnalyzer::SetAlertThresholds(double minHitRate, double maxAvgAccessTime) {
    m_minHitRateThreshold = minHitRate;
    m_maxAvgAccessTimeThreshold = maxAvgAccessTime;
    
    if (m_loggingEnabled) {
        std::stringstream ss;
        ss << "Alert thresholds set - Min hit rate: " << std::fixed << std::setprecision(2) 
           << (minHitRate * 100.0) << "%, Max avg access time: " 
           << std::fixed << std::setprecision(2) << maxAvgAccessTime << " ns";
        
        ICLogger::Instance().Info(ss.str(), "ICPerformance");
    }
}

//==============================================================================
// コールバック関連メソッド
//==============================================================================

void ICPerformanceAnalyzer::RegisterPerformanceThresholdCallback(
    std::function<void(const std::string&, double)> callback,
    double hitRateThreshold) 
{
    m_performanceCallbacks.emplace_back(callback, hitRateThreshold);
    
    if (m_loggingEnabled) {
        std::stringstream ss;
        ss << "Registered performance callback with threshold: " 
           << std::fixed << std::setprecision(2) << (hitRateThreshold * 100.0) << "%";
        
        ICLogger::Instance().Info(ss.str(), "ICPerformance");
    }
}

//==============================================================================
// 変換ユーティリティメソッド
//==============================================================================

std::string ICPerformanceAnalyzer::ICTypeToString(ICType type) {
    switch (type) {
        case ICType::Property:
            return "Property";
        case ICType::Method:
            return "Method";
        case ICType::Constructor:
            return "Constructor";
        case ICType::Prototype:
            return "Prototype";
        case ICType::Polymorphic:
            return "Polymorphic";
        case ICType::Megamorphic:
            return "Megamorphic";
        case ICType::Global:
            return "Global";
        case ICType::Builtin:
            return "Builtin";
        case ICType::Other:
            return "Other";
        default:
            return "Unknown";
    }
}

std::string ICPerformanceAnalyzer::ICAccessResultToString(ICAccessResult result) {
    switch (result) {
        case ICAccessResult::Hit:
            return "Hit";
        case ICAccessResult::Miss:
            return "Miss";
        case ICAccessResult::Invalidated:
            return "Invalidated";
        case ICAccessResult::Overflow:
            return "Overflow";
        case ICAccessResult::TypeError:
            return "TypeError";
        case ICAccessResult::Unknown:
            return "Unknown";
        default:
            return "Undefined";
    }
}

//==============================================================================
// 内部ヘルパーメソッド
//==============================================================================

void ICPerformanceAnalyzer::LogPerformanceIssue(
    const std::string& cacheId,
    const std::string& message) 
{
    if (m_loggingEnabled) {
        std::stringstream ss;
        ss << "Cache ID: " << cacheId << " - " << message;
        ICLogger::Instance().Warning(ss.str(), "ICPerformance");
    }
}

void ICPerformanceAnalyzer::DetectPerformanceIssues(
    const std::string& cacheId,
    const ICAccessStats& stats) 
{
    // 十分なサンプル数がない場合はスキップ
    constexpr uint64_t MIN_SAMPLE_SIZE = 100;
    if (stats.accessCount < MIN_SAMPLE_SIZE) {
        return;
    }
    
    // ヒット率の計算
    uint64_t totalAccesses = stats.hits + stats.misses;
    if (totalAccesses > 0) {
        double hitRate = static_cast<double>(stats.hits) / totalAccesses;
        
        // ヒット率が閾値を下回る場合
        if (hitRate < m_minHitRateThreshold) {
            std::stringstream ss;
            ss << "Low hit rate: " << std::fixed << std::setprecision(2) 
               << (hitRate * 100.0) << "% (threshold: " 
               << (m_minHitRateThreshold * 100.0) << "%)";
            
            LogPerformanceIssue(cacheId, ss.str());
            
            // 登録されたコールバックを呼び出し
            for (const auto& callbackPair : m_performanceCallbacks) {
                if (hitRate < callbackPair.second) {
                    callbackPair.first(cacheId, hitRate);
                }
            }
        }
    }
    
    // 平均アクセス時間の計算
    if (stats.accessCount > 0) {
        double avgAccessTime = static_cast<double>(stats.totalTime) / stats.accessCount;
        
        // 平均アクセス時間が閾値を上回る場合
        if (avgAccessTime > m_maxAvgAccessTimeThreshold) {
            std::stringstream ss;
            ss << "High average access time: " << std::fixed << std::setprecision(2) 
               << avgAccessTime << " ns (threshold: " 
               << m_maxAvgAccessTimeThreshold << " ns)";
            
            LogPerformanceIssue(cacheId, ss.str());
        }
    }
    
    // 無効化率の計算
    if (totalAccesses > 0) {
        double invalidationRate = static_cast<double>(stats.invalidations) / totalAccesses;
        constexpr double HIGH_INVALIDATION_THRESHOLD = 0.05; // 5%
        
        if (invalidationRate > HIGH_INVALIDATION_THRESHOLD) {
            std::stringstream ss;
            ss << "High invalidation rate: " << std::fixed << std::setprecision(2) 
               << (invalidationRate * 100.0) << "%";
            
            LogPerformanceIssue(cacheId, ss.str());
        }
    }
    
    // 型エラー率の計算
    if (totalAccesses > 0) {
        double typeErrorRate = static_cast<double>(stats.typeErrors) / totalAccesses;
        constexpr double HIGH_TYPE_ERROR_THRESHOLD = 0.03; // 3%
        
        if (typeErrorRate > HIGH_TYPE_ERROR_THRESHOLD) {
            std::stringstream ss;
            ss << "High type error rate: " << std::fixed << std::setprecision(2) 
               << (typeErrorRate * 100.0) << "%";
            
            LogPerformanceIssue(cacheId, ss.str());
        }
    }
}

//==============================================================================
// パフォーマンスアドバイス生成メソッド
//==============================================================================

std::vector<ICPerformanceAdvice> ICPerformanceAnalyzer::GeneratePerformanceAdvice() const {
    std::vector<ICPerformanceAdvice> allAdvice;
    
    // すべてのキャッシュIDに対してアドバイスを生成
    std::lock_guard<std::mutex> lock(m_statsMutex);
    for (const auto& pair : m_cacheStats) {
        const std::string& cacheId = pair.first;
        std::vector<ICPerformanceAdvice> cacheAdvice = GeneratePerformanceAdviceForCache(cacheId);
        allAdvice.insert(allAdvice.end(), cacheAdvice.begin(), cacheAdvice.end());
    }
    
    // 重要度でソート
    std::sort(allAdvice.begin(), allAdvice.end());
    
    return allAdvice;
}

std::vector<ICPerformanceAdvice> ICPerformanceAnalyzer::GeneratePerformanceAdviceForCache(
    const std::string& cacheId) const 
{
    std::vector<ICPerformanceAdvice> advice;
    
    // キャッシュIDの統計情報を取得
    std::lock_guard<std::mutex> lock(m_statsMutex);
    auto it = m_cacheStats.find(cacheId);
    if (it == m_cacheStats.end()) {
        return advice;
    }
    
    const ICAccessStats& stats = it->second;
    
    // 十分なサンプル数があるか確認
    constexpr uint64_t MIN_SAMPLE_SIZE = 100;
    if (stats.accessCount < MIN_SAMPLE_SIZE) {
        return advice;
    }
    
    // 最後のアクセス履歴エントリからロケーション情報を取得
    std::string locationInfo;
    {
        std::lock_guard<std::mutex> historyLock(m_historyMutex);
        auto historyIt = m_accessHistory.find(cacheId);
        if (historyIt != m_accessHistory.end() && !historyIt->second.empty()) {
            locationInfo = historyIt->second.back().locationInfo;
        }
    }
    
    // ヒット率が低い場合のアドバイス
    uint64_t totalAccesses = stats.hits + stats.misses;
    if (totalAccesses > 0) {
        double hitRate = static_cast<double>(stats.hits) / totalAccesses;
        
        if (hitRate < m_minHitRateThreshold) {
            std::vector<ICPerformanceAdvice> lowHitRateAdvice = 
                GenerateAdviceForLowHitRate(cacheId, hitRate, locationInfo);
            
            advice.insert(advice.end(), lowHitRateAdvice.begin(), lowHitRateAdvice.end());
        }
    }
    
    // 無効化率が高い場合のアドバイス
    if (totalAccesses > 0) {
        double invalidationRate = static_cast<double>(stats.invalidations) / totalAccesses;
        constexpr double HIGH_INVALIDATION_THRESHOLD = 0.05; // 5%
        
        if (invalidationRate > HIGH_INVALIDATION_THRESHOLD) {
            std::vector<ICPerformanceAdvice> highInvalidationAdvice = 
                GenerateAdviceForHighInvalidationRate(cacheId, invalidationRate, locationInfo);
            
            advice.insert(advice.end(), highInvalidationAdvice.begin(), highInvalidationAdvice.end());
        }
    }
    
    // 型エラー率が高い場合のアドバイス
    if (totalAccesses > 0) {
        double typeErrorRate = static_cast<double>(stats.typeErrors) / totalAccesses;
        constexpr double HIGH_TYPE_ERROR_THRESHOLD = 0.03; // 3%
        
        if (typeErrorRate > HIGH_TYPE_ERROR_THRESHOLD) {
            std::vector<ICPerformanceAdvice> typeErrorAdvice = 
                GenerateAdviceForTypeErrors(cacheId, typeErrorRate, locationInfo);
            
            advice.insert(advice.end(), typeErrorAdvice.begin(), typeErrorAdvice.end());
        }
    }
    
    return advice;
}

std::vector<ICPerformanceAdvice> ICPerformanceAnalyzer::GeneratePerformanceAdviceByType(
    ICType type) const 
{
    std::vector<ICPerformanceAdvice> advice;
    
    // タイプに基づいて関連するキャッシュIDを収集
    std::vector<std::string> relevantCacheIds;
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        for (const auto& pair : m_cacheStats) {
            // 履歴からキャッシュタイプを検索
            std::lock_guard<std::mutex> historyLock(m_historyMutex);
            auto historyIt = m_accessHistory.find(pair.first);
            if (historyIt != m_accessHistory.end() && !historyIt->second.empty()) {
                // ここではタイプ情報を持っていないため、外部から提供される必要がある
                // 実際の実装では、キャッシュエントリにタイプ情報を含める必要がある
                relevantCacheIds.push_back(pair.first);
            }
        }
    }
    
    // 関連するキャッシュIDに対してアドバイスを生成
    for (const std::string& cacheId : relevantCacheIds) {
        std::vector<ICPerformanceAdvice> cacheAdvice = GeneratePerformanceAdviceForCache(cacheId);
        advice.insert(advice.end(), cacheAdvice.begin(), cacheAdvice.end());
    }
    
    // 重要度でソート
    std::sort(advice.begin(), advice.end());
    
    return advice;
}

double ICPerformanceAnalyzer::CalculateCacheImportance(const std::string& cacheId) const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    auto it = m_cacheStats.find(cacheId);
    if (it == m_cacheStats.end()) {
        return 0.0;
    }
    
    const ICAccessStats& stats = it->second;
    
    // 重要度の要素:
    // 1. アクセス頻度（全体に対する割合）
    double accessFrequency = 0.0;
    uint64_t totalAccessesAcrossAllCaches = 0;
    for (const auto& pair : m_cacheStats) {
        totalAccessesAcrossAllCaches += pair.second.accessCount;
    }
    
    if (totalAccessesAcrossAllCaches > 0) {
        accessFrequency = static_cast<double>(stats.accessCount) / totalAccessesAcrossAllCaches;
    }
    
    // 2. ミス率
    double missRate = 0.0;
    uint64_t totalAccesses = stats.hits + stats.misses;
    if (totalAccesses > 0) {
        missRate = static_cast<double>(stats.misses) / totalAccesses;
    }
    
    // 3. 型エラー率
    double typeErrorRate = 0.0;
    if (totalAccesses > 0) {
        typeErrorRate = static_cast<double>(stats.typeErrors) / totalAccesses;
    }
    
    // 4. アクセス時間（全体の平均との比較）
    double avgAccessTime = 0.0;
    if (stats.accessCount > 0) {
        avgAccessTime = static_cast<double>(stats.totalTime) / stats.accessCount;
    }
    
    double overallAvgAccessTime = CalculateOverallAverageAccessTime();
    double relativeAccessTime = 0.0;
    if (overallAvgAccessTime > 0) {
        relativeAccessTime = avgAccessTime / overallAvgAccessTime;
    }
    
    // 重要度のスコア計算
    // アクセス頻度が高く、ミス率や型エラー率が高く、アクセス時間が平均より長いほど重要
    double importance = accessFrequency * (1.0 + missRate * 5.0 + typeErrorRate * 3.0 + relativeAccessTime);
    
    return std::min(1.0, importance); // 0.0 - 1.0 の範囲に正規化
}

//==============================================================================
// レポート生成メソッド
//==============================================================================

std::string ICPerformanceAnalyzer::GenerateReport(bool detailed) const {
    std::stringstream ss;
    
    // レポートのヘッダー
    ss << "===================================================================\n";
    ss << "             インラインキャッシュパフォーマンスレポート            \n";
    ss << "===================================================================\n\n";
    
    // 現在の時刻を取得してレポートに含める
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    ss << "生成日時: " << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S") << "\n\n";
    
    // 全体の統計情報
    ICAccessStats aggregateStats = GetAggregateStats();
    double overallHitRate = CalculateOverallHitRate();
    double overallAvgAccessTime = CalculateOverallAverageAccessTime();
    
    ss << "全体の統計情報:\n";
    ss << "  - 総アクセス数: " << aggregateStats.accessCount << "\n";
    ss << "  - ヒット数: " << aggregateStats.hits << "\n";
    ss << "  - ミス数: " << aggregateStats.misses << "\n";
    ss << "  - 無効化数: " << aggregateStats.invalidations << "\n";
    ss << "  - 型エラー数: " << aggregateStats.typeErrors << "\n";
    ss << "  - オーバーフロー数: " << aggregateStats.overflows << "\n";
    ss << "  - その他エラー数: " << aggregateStats.unknownErrors << "\n";
    ss << "  - ヒット率: " << std::fixed << std::setprecision(2) << (overallHitRate * 100.0) << "%\n";
    ss << "  - 平均アクセス時間: " << std::fixed << std::setprecision(2) << overallAvgAccessTime << " ns\n\n";
    
    // キャッシュタイプ別の統計情報
    ss << "キャッシュタイプ別の統計情報:\n";
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        
        for (const auto& pair : m_typeStats) {
            ICType type = pair.first;
            const ICAccessStats& stats = pair.second;
            
            double typeHitRate = 0.0;
            double typeAvgAccessTime = 0.0;
            
            if (stats.hits + stats.misses > 0) {
                typeHitRate = static_cast<double>(stats.hits) / (stats.hits + stats.misses);
            }
            
            if (stats.accessCount > 0) {
                typeAvgAccessTime = static_cast<double>(stats.totalTime) / stats.accessCount;
            }
            
            ss << "  - " << ICTypeToString(type) << ":\n";
            ss << "    - アクセス数: " << stats.accessCount << "\n";
            ss << "    - ヒット率: " << std::fixed << std::setprecision(2) << (typeHitRate * 100.0) << "%\n";
            ss << "    - 平均アクセス時間: " << std::fixed << std::setprecision(2) << typeAvgAccessTime << " ns\n";
        }
    }
    ss << "\n";
    
    // 詳細レポートが要求された場合、キャッシュID別の統計情報も含める
    if (detailed) {
        ss << "キャッシュID別の詳細統計情報:\n";
        
        // キャッシュIDのリストを取得
        std::vector<std::string> cacheIds = GetAllCacheIds();
        
        // 重要度に基づいてソート
        std::sort(cacheIds.begin(), cacheIds.end(), [this](const std::string& a, const std::string& b) {
            return CalculateCacheImportance(a) > CalculateCacheImportance(b);
        });
        
        for (const std::string& cacheId : cacheIds) {
            ICAccessStats stats = GetStatsForCache(cacheId);
            double hitRate = CalculateHitRate(cacheId);
            double avgAccessTime = CalculateAverageAccessTime(cacheId);
            double importance = CalculateCacheImportance(cacheId);
            
            ss << "  - Cache ID: " << cacheId << " (重要度: " << std::fixed << std::setprecision(2) << (importance * 100.0) << "%)\n";
            ss << "    - アクセス数: " << stats.accessCount << "\n";
            ss << "    - ヒット数: " << stats.hits << "\n";
            ss << "    - ミス数: " << stats.misses << "\n";
            ss << "    - 無効化数: " << stats.invalidations << "\n";
            ss << "    - ヒット率: " << std::fixed << std::setprecision(2) << (hitRate * 100.0) << "%\n";
            ss << "    - 平均アクセス時間: " << std::fixed << std::setprecision(2) << avgAccessTime << " ns\n";
            
            if (stats.typeErrors > 0) {
                double typeErrorRate = static_cast<double>(stats.typeErrors) / stats.accessCount;
                ss << "    - 型エラー率: " << std::fixed << std::setprecision(2) << (typeErrorRate * 100.0) << "%\n";
            }
            
            if (stats.overflows > 0) {
                double overflowRate = static_cast<double>(stats.overflows) / stats.accessCount;
                ss << "    - オーバーフロー率: " << std::fixed << std::setprecision(2) << (overflowRate * 100.0) << "%\n";
            }
            
            // パフォーマンスアドバイスを追加
            std::vector<ICPerformanceAdvice> cacheAdvice = GeneratePerformanceAdviceForCache(cacheId);
            if (!cacheAdvice.empty()) {
                ss << "    - パフォーマンスアドバイス:\n";
                for (const auto& advice : cacheAdvice) {
                    ss << "      * " << advice.advice << " (影響度: " << std::fixed << std::setprecision(2) << (advice.impact * 100.0) << "%)\n";
                    if (!advice.explanation.empty()) {
                        ss << "        " << advice.explanation << "\n";
                    }
                }
            }
            
            ss << "\n";
        }
    }
    
    // 全体のパフォーマンスアドバイス
    std::vector<ICPerformanceAdvice> allAdvice = GeneratePerformanceAdvice();
    if (!allAdvice.empty()) {
        ss << "パフォーマンス改善のためのトップアドバイス:\n";
        
        size_t adviceCount = std::min(allAdvice.size(), static_cast<size_t>(5)); // 上位5つのアドバイスを表示
        for (size_t i = 0; i < adviceCount; ++i) {
            const auto& advice = allAdvice[i];
            ss << "  " << (i + 1) << ". " << advice.advice << "\n";
            ss << "     影響度: " << std::fixed << std::setprecision(2) << (advice.impact * 100.0) << "%\n";
            if (!advice.codeLocation.empty()) {
                ss << "     場所: " << advice.codeLocation << "\n";
            }
            if (!advice.explanation.empty()) {
                ss << "     説明: " << advice.explanation << "\n";
            }
            ss << "\n";
        }
    }
    
    ss << "===================================================================\n";
    
    return ss.str();
}

std::string ICPerformanceAnalyzer::GenerateJsonReport() const {
    using json = nlohmann::json;
    
    json report;
    
    // 現在の時刻を取得してレポートに含める
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream timeStream;
    timeStream << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
    report["timestamp"] = timeStream.str();
    
    // 全体の統計情報
    ICAccessStats aggregateStats = GetAggregateStats();
    double overallHitRate = CalculateOverallHitRate();
    double overallAvgAccessTime = CalculateOverallAverageAccessTime();
    
    report["overall_stats"] = {
        {"total_accesses", aggregateStats.accessCount},
        {"hits", aggregateStats.hits},
        {"misses", aggregateStats.misses},
        {"invalidations", aggregateStats.invalidations},
        {"type_errors", aggregateStats.typeErrors},
        {"overflows", aggregateStats.overflows},
        {"unknown_errors", aggregateStats.unknownErrors},
        {"hit_rate", overallHitRate},
        {"avg_access_time", overallAvgAccessTime}
    };
    
    // キャッシュタイプ別の統計情報
    json typeStats;
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        
        for (const auto& pair : m_typeStats) {
            ICType type = pair.first;
            const ICAccessStats& stats = pair.second;
            
            double typeHitRate = 0.0;
            double typeAvgAccessTime = 0.0;
            
            if (stats.hits + stats.misses > 0) {
                typeHitRate = static_cast<double>(stats.hits) / (stats.hits + stats.misses);
            }
            
            if (stats.accessCount > 0) {
                typeAvgAccessTime = static_cast<double>(stats.totalTime) / stats.accessCount;
            }
            
            json typeData = {
                {"type", ICTypeToString(type)},
                {"accesses", stats.accessCount},
                {"hits", stats.hits},
                {"misses", stats.misses},
                {"invalidations", stats.invalidations},
                {"type_errors", stats.typeErrors},
                {"overflows", stats.overflows},
                {"unknown_errors", stats.unknownErrors},
                {"hit_rate", typeHitRate},
                {"avg_access_time", typeAvgAccessTime}
            };
            
            typeStats.push_back(typeData);
        }
    }
    report["type_stats"] = typeStats;
    
    // キャッシュID別の統計情報
    json cacheStats;
    
    // キャッシュIDのリストを取得
    std::vector<std::string> cacheIds = GetAllCacheIds();
    
    // 重要度に基づいてソート
    std::sort(cacheIds.begin(), cacheIds.end(), [this](const std::string& a, const std::string& b) {
        return CalculateCacheImportance(a) > CalculateCacheImportance(b);
    });
    
    for (const std::string& cacheId : cacheIds) {
        ICAccessStats stats = GetStatsForCache(cacheId);
        double hitRate = CalculateHitRate(cacheId);
        double avgAccessTime = CalculateAverageAccessTime(cacheId);
        double importance = CalculateCacheImportance(cacheId);
        
        json cacheData = {
            {"cache_id", cacheId},
            {"importance", importance},
            {"accesses", stats.accessCount},
            {"hits", stats.hits},
            {"misses", stats.misses},
            {"invalidations", stats.invalidations},
            {"type_errors", stats.typeErrors},
            {"overflows", stats.overflows},
            {"unknown_errors", stats.unknownErrors},
            {"hit_rate", hitRate},
            {"avg_access_time", avgAccessTime}
        };
        
        // パフォーマンスアドバイスを追加
        json adviceArray;
        std::vector<ICPerformanceAdvice> cacheAdvice = GeneratePerformanceAdviceForCache(cacheId);
        for (const auto& advice : cacheAdvice) {
            json adviceData = {
                {"advice", advice.advice},
                {"impact", advice.impact},
                {"location", advice.codeLocation},
                {"explanation", advice.explanation}
            };
            adviceArray.push_back(adviceData);
        }
        cacheData["advice"] = adviceArray;
        
        cacheStats.push_back(cacheData);
    }
    report["cache_stats"] = cacheStats;
    
    // 全体のパフォーマンスアドバイス
    json adviceArray;
    std::vector<ICPerformanceAdvice> allAdvice = GeneratePerformanceAdvice();
    for (const auto& advice : allAdvice) {
        json adviceData = {
            {"advice", advice.advice},
            {"impact", advice.impact},
            {"location", advice.codeLocation},
            {"explanation", advice.explanation}
        };
        adviceArray.push_back(adviceData);
    }
    report["performance_advice"] = adviceArray;
    
    return report.dump(4); // インデント付きでJSON文字列を生成
}

//==============================================================================
// アドバイス生成ヘルパーメソッド
//==============================================================================

std::vector<ICPerformanceAdvice> ICPerformanceAnalyzer::GenerateAdviceForLowHitRate(
    const std::string& cacheId,
    double hitRate,
    const std::string& locationInfo) const 
{
    std::vector<ICPerformanceAdvice> advice;
    
    // ヒット率の逆数を影響度として使用（ヒット率が低いほど影響が大きい）
    double impact = 1.0 - hitRate;
    
    // 基本アドバイス
    std::stringstream ss;
    ss << "キャッシュ '" << cacheId << "' のヒット率が低い (" 
       << std::fixed << std::setprecision(2) << (hitRate * 100.0) << "%)";
    
    std::string basicAdvice = ss.str();
    std::string explanation = "ヒット率が低いインラインキャッシュは、パフォーマンスに悪影響を与えます。"
                              "キャッシュミスが多い原因を特定してください。";
    
    advice.emplace_back(basicAdvice, impact, locationInfo, explanation);
    
    // 詳細なアドバイス（キャッシュタイプや状況に応じて）
    // 例: shapeの変化が多いことが問題かもしれない
    std::string shapesAdvice = "オブジェクトのプロパティ構造（shape）が頻繁に変更されていないか確認してください。";
    std::string shapesExplanation = "JavaScriptでは、オブジェクトのプロパティ追加や削除によってshapeが変更され、"
                                   "インラインキャッシュが無効化される可能性があります。コードを修正して、"
                                   "オブジェクトの構造を一貫させることでヒット率を向上させることができます。";
    
    advice.emplace_back(shapesAdvice, impact * 0.9, locationInfo, shapesExplanation);
    
    // 多態性に関するアドバイス
    std::string polymorphismAdvice = "コードの多態性のレベルを確認し、可能であれば単一型または少数の型に制限してください。";
    std::string polymorphismExplanation = "多くの異なる型が同じコード位置で使用されると、インラインキャッシュのヒット率が低下します。"
                                        "型の数を減らすか、型ごとに別々のコードパスを用意することを検討してください。";
    
    advice.emplace_back(polymorphismAdvice, impact * 0.8, locationInfo, polymorphismExplanation);
    
    return advice;
}

std::vector<ICPerformanceAdvice> ICPerformanceAnalyzer::GenerateAdviceForHighInvalidationRate(
    const std::string& cacheId,
    double invalidationRate,
    const std::string& locationInfo) const 
{
    std::vector<ICPerformanceAdvice> advice;
    
    // 無効化率を影響度として使用
    double impact = std::min(1.0, invalidationRate * 10.0); // 10%の無効化率で最大影響度
    
    // 基本アドバイス
    std::stringstream ss;
    ss << "キャッシュ '" << cacheId << "' の無効化率が高い (" 
       << std::fixed << std::setprecision(2) << (invalidationRate * 100.0) << "%)";
    
    std::string basicAdvice = ss.str();
    std::string explanation = "キャッシュの頻繁な無効化は、パフォーマンスの低下を引き起こします。"
                              "オブジェクトやプロトタイプの変更頻度を確認してください。";
    
    advice.emplace_back(basicAdvice, impact, locationInfo, explanation);
    
    // プロトタイプに関するアドバイス
    std::string prototypeAdvice = "実行時にプロトタイプチェーンを変更していないか確認してください。";
    std::string prototypeExplanation = "プロトタイプの変更は、多くのインラインキャッシュエントリを無効化する可能性があります。"
                                     "可能であれば、アプリケーションの初期化段階でプロトタイプを設定し、"
                                     "その後は変更しないようにしてください。";
    
    advice.emplace_back(prototypeAdvice, impact * 0.9, locationInfo, prototypeExplanation);
    
    // シールドオブジェクトに関するアドバイス
    std::string sealedObjectsAdvice = "頻繁に使用されるオブジェクトをシールまたはフリーズすることを検討してください。";
    std::string sealedObjectsExplanation = "Object.seal()やObject.freeze()を使用すると、オブジェクトの構造が変更されなくなり、"
                                         "インラインキャッシュの無効化を減らすことができます。";
    
    advice.emplace_back(sealedObjectsAdvice, impact * 0.8, locationInfo, sealedObjectsExplanation);
    
    return advice;
}

std::vector<ICPerformanceAdvice> ICPerformanceAnalyzer::GenerateAdviceForTypeErrors(
    const std::string& cacheId,
    double typeErrorRate,
    const std::string& locationInfo) const 
{
    std::vector<ICPerformanceAdvice> advice;
    
    // 型エラー率を影響度として使用
    double impact = std::min(1.0, typeErrorRate * 20.0); // 5%の型エラー率で最大影響度
    
    // 基本アドバイス
    std::stringstream ss;
    ss << "キャッシュ '" << cacheId << "' の型エラー率が高い (" 
       << std::fixed << std::setprecision(2) << (typeErrorRate * 100.0) << "%)";
    
    std::string basicAdvice = ss.str();
    std::string explanation = "型エラーは、予期しない型のオブジェクトがキャッシュに渡されていることを示します。"
                              "コードの型の一貫性を確認してください。";
    
    advice.emplace_back(basicAdvice, impact, locationInfo, explanation);
    
    // 型チェックに関するアドバイス
    std::string typeCheckAdvice = "型ガードを追加するか、より厳格な型チェックを行ってください。";
    std::string typeCheckExplanation = "関数やメソッドの入力パラメータの型を一貫させるか、明示的な型チェックを追加することで、"
                                     "型エラーを減らし、キャッシュのヒット率を向上させることができます。";
    
    advice.emplace_back(typeCheckAdvice, impact * 0.9, locationInfo, typeCheckExplanation);
    
    // 型の一貫性に関するアドバイス
    std::string typeConsistencyAdvice = "同じプロパティや変数に対して一貫した型を使用するようにしてください。";
    std::string typeConsistencyExplanation = "プロパティや変数の型が変わると、インラインキャッシュが無効化されます。"
                                           "可能であれば、同じ名前のプロパティには常に同じ型の値を設定してください。";
    
    advice.emplace_back(typeConsistencyAdvice, impact * 0.8, locationInfo, typeConsistencyExplanation);
    
    return advice;
}

//==============================================================================
// ICAccessTimer 実装
//==============================================================================

ICAccessTimer::ICAccessTimer(
    const std::string& cacheId,
    ICType type,
    const std::string& locationInfo)
    : m_cacheId(cacheId)
    , m_type(type)
    , m_result(ICAccessResult::Unknown)
    , m_locationInfo(locationInfo)
    , m_startTime(std::chrono::high_resolution_clock::now())
    , m_stopped(false) 
{
}

ICAccessTimer::~ICAccessTimer() {
    if (!m_stopped) {
        // まだ停止していない場合、測定を終了して記録
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - m_startTime);
        
        // アクセス結果と時間をパフォーマンスアナライザに記録
        ICPerformanceAnalyzer::Instance().RecordAccess(
            m_cacheId,
            m_type,
            m_result,
            static_cast<uint64_t>(duration.count()),
            m_locationInfo);
    }
}

void ICAccessTimer::SetResult(ICAccessResult result) {
    m_result = result;
    m_stopped = true;
    
    // 測定を終了して記録
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - m_startTime);
    
    // アクセス結果と時間をパフォーマンスアナライザに記録
    ICPerformanceAnalyzer::Instance().RecordAccess(
        m_cacheId,
        m_type,
        result,
        static_cast<uint64_t>(duration.count()),
        m_locationInfo);
}

} // namespace core
} // namespace aerojs 