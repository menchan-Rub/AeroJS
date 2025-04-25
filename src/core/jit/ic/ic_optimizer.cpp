#include "ic_optimizer.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>
#include <thread>
#include <json.hpp>

namespace aerojs {
namespace core {

//==============================================================================
// ICOptimizer シングルトン実装
//==============================================================================

ICOptimizer& ICOptimizer::Instance() {
    static ICOptimizer instance;
    return instance;
}

ICOptimizer::ICOptimizer()
    : m_backgroundRunning(false)
    , m_totalOptimizationCount(0)
{
    ICLogger::Instance().Info("ICOptimizer initialized", "ICOptimizer");
}

ICOptimizer::~ICOptimizer() {
    // バックグラウンド最適化が実行中の場合は停止
    StopBackgroundOptimization();
    
    ICLogger::Instance().Info("ICOptimizer destroyed", "ICOptimizer");
}

//==============================================================================
// メイン最適化メソッド
//==============================================================================

ICOptimizationResult ICOptimizer::OptimizeCache(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager)
{
    if (!cacheManager) {
        ICOptimizationResult result;
        result.success = false;
        result.errorMessage = "Cache manager is null";
        return result;
    }
    
    // 開始時間を記録
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 最適化が必要かどうか確認
    if (!NeedsOptimization(cacheId, type, options.thresholds)) {
        ICOptimizationResult result;
        result.success = true;
        result.optimizationId = "no-op-" + cacheId;
        result.state = ICOptimizationState::FullyOptimized;
        result.errorMessage = "Optimization not needed";
        
        // 開始時間と終了時間の差分を計算
        auto endTime = std::chrono::high_resolution_clock::now();
        result.optimizationTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        
        return result;
    }
    
    // 最適化前のヒット率を記録
    double hitRateBefore = ICPerformanceAnalyzer::Instance().CalculateHitRate(cacheId);
    
    // 選択された最適化戦略に基づいて最適化を実行
    ICOptimizationResult result;
    switch (options.strategy) {
        case ICOptimizationStrategy::FrequencyBased:
            result = OptimizeByFrequency(cacheId, type, options, cacheManager);
            break;
        case ICOptimizationStrategy::PatternBased:
            result = OptimizeByPattern(cacheId, type, options, cacheManager);
            break;
        case ICOptimizationStrategy::ProfileBased:
            result = OptimizeByProfile(cacheId, type, options, cacheManager);
            break;
        case ICOptimizationStrategy::HeuristicBased:
            result = OptimizeByHeuristic(cacheId, type, options, cacheManager);
            break;
        case ICOptimizationStrategy::AdaptiveBased:
            result = OptimizeByAdaptive(cacheId, type, options, cacheManager);
            break;
        case ICOptimizationStrategy::Custom:
            {
                // カスタムハンドラを探す
                std::lock_guard<std::mutex> lock(m_handlersMutex);
                auto it = m_customHandlers.find(type);
                if (it != m_customHandlers.end()) {
                    result = it->second(cacheId, type, options, cacheManager);
                } else {
                    // カスタムハンドラが見つからない場合、デフォルトでFrequencyBasedを使用
                    result = OptimizeByFrequency(cacheId, type, options, cacheManager);
                }
            }
            break;
        case ICOptimizationStrategy::None:
        default:
            // 最適化なし
            result.success = true;
            result.optimizationId = "no-op-" + cacheId;
            result.state = ICOptimizationState::NotOptimized;
            break;
    }
    
    // 最適化後のヒット率を記録
    double hitRateAfter = ICPerformanceAnalyzer::Instance().CalculateHitRate(cacheId);
    result.hitRateBeforeOptimization = hitRateBefore;
    result.hitRateAfterOptimization = hitRateAfter;
    
    // パフォーマンス改善率を計算
    if (hitRateBefore > 0) {
        result.performanceImprovement = (hitRateAfter - hitRateBefore) / hitRateBefore;
    } else if (hitRateAfter > 0) {
        result.performanceImprovement = 1.0; // 100%の改善（0から改善された）
    } else {
        result.performanceImprovement = 0.0;
    }
    
    // 最適化時間を計算
    auto endTime = std::chrono::high_resolution_clock::now();
    result.optimizationTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);
    
    // 最適化カウンタをインクリメント
    m_totalOptimizationCount++;
    
    // 最適化履歴に追加
    AddToOptimizationHistory(cacheId, result);
    
    // 最適化結果をログに記録
    if (result.success) {
        std::stringstream ss;
        ss << "Optimized cache '" << cacheId << "' - Hit rate: " 
           << std::fixed << std::setprecision(2) << (hitRateBefore * 100.0) << "% -> " 
           << std::fixed << std::setprecision(2) << (hitRateAfter * 100.0) << "% "
           << "(Improvement: " << std::fixed << std::setprecision(2) 
           << (result.performanceImprovement * 100.0) << "%)";
        
        ICLogger::Instance().Info(ss.str(), "ICOptimizer");
    } else {
        std::stringstream ss;
        ss << "Failed to optimize cache '" << cacheId << "' - Error: " << result.errorMessage;
        ICLogger::Instance().Error(ss.str(), "ICOptimizer");
    }
    
    return result;
}

std::unordered_map<std::string, ICOptimizationResult> ICOptimizer::OptimizeCachesByType(
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager)
{
    std::unordered_map<std::string, ICOptimizationResult> results;
    
    if (!cacheManager) {
        ICLogger::Instance().Error("Cache manager is null", "ICOptimizer");
        return results;
    }
    
    // パフォーマンスアナライザからキャッシュIDのリストを取得
    std::vector<std::string> cacheIds = ICPerformanceAnalyzer::Instance().GetAllCacheIds();
    
    // 優先度に基づいてキャッシュをソート
    std::sort(cacheIds.begin(), cacheIds.end(), [this](const std::string& a, const std::string& b) {
        return static_cast<int>(GetCachePriority(a)) < static_cast<int>(GetCachePriority(b));
    });
    
    // 各キャッシュIDに対して最適化を実行
    for (const std::string& cacheId : cacheIds) {
        // このキャッシュが指定されたタイプであるかどうかを確認する必要がある
        // （この実装では簡単のために、すべてのキャッシュを処理）
        ICOptimizationResult result = OptimizeCache(cacheId, type, options, cacheManager);
        results[cacheId] = result;
    }
    
    // 結果をログに記録
    std::stringstream ss;
    ss << "Optimized " << results.size() << " caches of type " << ICPerformanceAnalyzer::ICTypeToString(type);
    ICLogger::Instance().Info(ss.str(), "ICOptimizer");
    
    return results;
}

std::unordered_map<std::string, ICOptimizationResult> ICOptimizer::OptimizeAllCaches(
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager)
{
    std::unordered_map<std::string, ICOptimizationResult> results;
    
    if (!cacheManager) {
        ICLogger::Instance().Error("Cache manager is null", "ICOptimizer");
        return results;
    }
    
    // パフォーマンスアナライザからキャッシュIDのリストを取得
    std::vector<std::string> cacheIds = ICPerformanceAnalyzer::Instance().GetAllCacheIds();
    
    // 優先度に基づいてキャッシュをソート
    std::sort(cacheIds.begin(), cacheIds.end(), [this](const std::string& a, const std::string& b) {
        return static_cast<int>(GetCachePriority(a)) < static_cast<int>(GetCachePriority(b));
    });
    
    // 各キャッシュIDに対して最適化を実行
    for (const std::string& cacheId : cacheIds) {
        // キャッシュタイプを取得（実際の実装ではキャッシュマネージャから取得する必要がある）
        // この例では、Property タイプを仮定
        ICType type = ICType::Property;
        
        ICOptimizationResult result = OptimizeCache(cacheId, type, options, cacheManager);
        results[cacheId] = result;
    }
    
    // 結果をログに記録
    std::stringstream ss;
    ss << "Optimized " << results.size() << " caches";
    ICLogger::Instance().Info(ss.str(), "ICOptimizer");
    
    return results;
}

//==============================================================================
// 優先度管理メソッド
//==============================================================================

void ICOptimizer::SetCachePriority(const std::string& cacheId, ICPriorityLevel priority) {
    std::lock_guard<std::mutex> lock(m_priorityMutex);
    m_cachePriorities[cacheId] = priority;
    
    // ログに記録
    std::stringstream ss;
    ss << "Set priority of cache '" << cacheId << "' to ";
    
    switch (priority) {
        case ICPriorityLevel::Critical:
            ss << "Critical";
            break;
        case ICPriorityLevel::High:
            ss << "High";
            break;
        case ICPriorityLevel::Medium:
            ss << "Medium";
            break;
        case ICPriorityLevel::Low:
            ss << "Low";
            break;
        case ICPriorityLevel::Background:
            ss << "Background";
            break;
        default:
            ss << "Unknown";
            break;
    }
    
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
}

ICPriorityLevel ICOptimizer::GetCachePriority(const std::string& cacheId) const {
    std::lock_guard<std::mutex> lock(m_priorityMutex);
    auto it = m_cachePriorities.find(cacheId);
    if (it != m_cachePriorities.end()) {
        return it->second;
    }
    
    // デフォルトはMedium
    return ICPriorityLevel::Medium;
}

//==============================================================================
// 分析・判断メソッド
//==============================================================================

bool ICOptimizer::NeedsOptimization(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationThresholds& thresholds) const
{
    // パフォーマンスアナライザからキャッシュの統計情報を取得
    ICAccessStats stats = ICPerformanceAnalyzer::Instance().GetStatsForCache(cacheId);
    
    // アクセス数が閾値未満の場合は最適化不要
    if (stats.accessCount < thresholds.minAccessCount) {
        return false;
    }
    
    // ヒット率を計算
    double hitRate = 0.0;
    if (stats.hits + stats.misses > 0) {
        hitRate = static_cast<double>(stats.hits) / (stats.hits + stats.misses);
    }
    
    // ヒット率が閾値未満の場合は最適化が必要
    if (hitRate < thresholds.minHitRate) {
        return true;
    }
    
    // 型エラー率を計算
    double typeErrorRate = 0.0;
    if (stats.accessCount > 0) {
        typeErrorRate = static_cast<double>(stats.typeErrors) / stats.accessCount;
    }
    
    // 型エラー率が閾値を超える場合は最適化が必要
    if (typeErrorRate > thresholds.maxTypeErrorRate) {
        return true;
    }
    
    // 無効化率を計算
    double invalidationRate = 0.0;
    if (stats.accessCount > 0) {
        invalidationRate = static_cast<double>(stats.invalidations) / stats.accessCount;
    }
    
    // 無効化率が閾値を超える場合は最適化が必要
    if (invalidationRate > thresholds.maxInvalidationRate) {
        return true;
    }
    
    // すべての条件を満たしている場合は最適化不要
    return false;
}

std::vector<ICOptimizationResult> ICOptimizer::GetOptimizationHistory(
    const std::string& cacheId) const
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    auto it = m_optimizationHistory.find(cacheId);
    if (it != m_optimizationHistory.end()) {
        return it->second;
    }
    
    return std::vector<ICOptimizationResult>();
}

//==============================================================================
// バックグラウンド最適化メソッド
//==============================================================================

void ICOptimizer::StartBackgroundOptimization(
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager,
    uint64_t intervalMs)
{
    // 既に実行中の場合は何もしない
    if (m_backgroundRunning) {
        ICLogger::Instance().Warning("Background optimization is already running", "ICOptimizer");
        return;
    }
    
    if (!cacheManager) {
        ICLogger::Instance().Error("Cannot start background optimization: Cache manager is null", "ICOptimizer");
        return;
    }
    
    // フラグを設定
    m_backgroundRunning = true;
    
    // バックグラウンドスレッドを開始
    m_backgroundThread = std::thread(&ICOptimizer::BackgroundOptimizationWorker, 
                                    this, options, cacheManager, intervalMs);
    
    std::stringstream ss;
    ss << "Started background optimization with interval " << intervalMs << " ms";
    ICLogger::Instance().Info(ss.str(), "ICOptimizer");
}

void ICOptimizer::StopBackgroundOptimization() {
    // 実行中でない場合は何もしない
    if (!m_backgroundRunning) {
        return;
    }
    
    // フラグを解除
    m_backgroundRunning = false;
    
    // スレッドが終了するのを待つ
    if (m_backgroundThread.joinable()) {
        m_backgroundThread.join();
    }
    
    ICLogger::Instance().Info("Stopped background optimization", "ICOptimizer");
}

void ICOptimizer::BackgroundOptimizationWorker(
    ICOptimizationOptions options,
    InlineCacheManager* cacheManager,
    uint64_t intervalMs)
{
    // 前回の実行時刻を初期化
    auto lastRunTime = std::chrono::steady_clock::now();
    
    // バックグラウンドが有効である限り実行
    while (m_backgroundRunning) {
        // 次の実行時刻を計算
        auto nextRunTime = lastRunTime + std::chrono::milliseconds(intervalMs);
        
        // 次の実行時刻まで待機
        std::this_thread::sleep_until(nextRunTime);
        
        // 待機中にバックグラウンド最適化が停止された場合はループを抜ける
        if (!m_backgroundRunning) {
            break;
        }
        
        try {
            // パフォーマンスアナライザからキャッシュIDのリストを取得
            std::vector<std::string> cacheIds = ICPerformanceAnalyzer::Instance().GetAllCacheIds();
            
            // 優先度に基づいてキャッシュをソート
            std::sort(cacheIds.begin(), cacheIds.end(), [this](const std::string& a, const std::string& b) {
                return static_cast<int>(GetCachePriority(a)) < static_cast<int>(GetCachePriority(b));
            });
            
            // 各キャッシュIDに対して最適化が必要かどうか確認
            for (const std::string& cacheId : cacheIds) {
                // 優先度がBackgroundのキャッシュのみを処理
                if (GetCachePriority(cacheId) == ICPriorityLevel::Background) {
                    // キャッシュタイプを取得（実際の実装ではキャッシュマネージャから取得する必要がある）
                    // この例では、Property タイプを仮定
                    ICType type = ICType::Property;
                    
                    // 最適化が必要かどうか確認
                    if (NeedsOptimization(cacheId, type, options.thresholds)) {
                        // 最適化を実行
                        OptimizeCache(cacheId, type, options, cacheManager);
                    }
                }
            }
        } catch (const std::exception& e) {
            // 例外をログに記録
            std::stringstream ss;
            ss << "Exception in background optimization: " << e.what();
            ICLogger::Instance().Error(ss.str(), "ICOptimizer");
        }
        
        // 現在の時刻を記録
        lastRunTime = std::chrono::steady_clock::now();
    }
}

//==============================================================================
// カスタムハンドラ管理メソッド
//==============================================================================

void ICOptimizer::RegisterCustomOptimizationHandler(
    ICType type,
    ICCustomOptimizationHandler handler)
{
    if (!handler) {
        ICLogger::Instance().Warning("Attempted to register null custom optimization handler", "ICOptimizer");
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_handlersMutex);
    m_customHandlers[type] = handler;
    
    std::stringstream ss;
    ss << "Registered custom optimization handler for type " << ICPerformanceAnalyzer::ICTypeToString(type);
    ICLogger::Instance().Info(ss.str(), "ICOptimizer");
}

//==============================================================================
// ヘルパーメソッド
//==============================================================================

void ICOptimizer::AddToOptimizationHistory(
    const std::string& cacheId,
    const ICOptimizationResult& result)
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    
    // 履歴に追加
    m_optimizationHistory[cacheId].push_back(result);
    
    // 履歴が長すぎる場合は古いエントリを削除
    constexpr size_t MAX_HISTORY_ENTRIES = 100;
    if (m_optimizationHistory[cacheId].size() > MAX_HISTORY_ENTRIES) {
        m_optimizationHistory[cacheId].erase(m_optimizationHistory[cacheId].begin());
    }
}

//==============================================================================
// 最適化戦略の実装
//==============================================================================

ICOptimizationResult ICOptimizer::OptimizeByFrequency(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager)
{
    ICOptimizationResult result;
    result.optimizationId = "freq-" + cacheId + "-" + std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());
    
    // パフォーマンスアナライザからアクセス履歴を取得
    std::vector<ICAccessHistoryEntry> history = 
        ICPerformanceAnalyzer::Instance().GetAccessHistory(cacheId);
    
    if (history.empty()) {
        result.success = false;
        result.errorMessage = "No access history available for optimization";
        return result;
    }
    
    try {
        // 最適化の初期状態を設定
        result.success = true;
        result.state = ICOptimizationState::Optimizing;
        
        // ヒット・ミスの統計を分析
        ICAccessStats stats = ICPerformanceAnalyzer::Instance().GetStatsForCache(cacheId);
        double hitRate = 0.0;
        if (stats.hits + stats.misses > 0) {
            hitRate = static_cast<double>(stats.hits) / (stats.hits + stats.misses);
        }
        
        // ヒット率に基づいて最適化操作を選択
        if (hitRate < 0.5) {
            // ヒット率が低い場合は、刈り込みとおそらく拡大が必要
            if (PerformPruneOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
            }
            
            if (PerformExpandOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
            }
        } else if (hitRate < options.thresholds.minHitRate) {
            // 中程度のヒット率の場合は、整理と特化が必要かもしれない
            if (PerformReorganizeOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
            }
            
            if (PerformSpecializeOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
            }
        } else {
            // ヒット率が高い場合は、最適化はあまり必要ないが、
            // 縮小してメモリ使用量を減らすことができるかもしれない
            if (options.enableMemoryConstraints && 
                PerformContractOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
            }
        }
        
        // 型エラー率が高い場合は、特化が必要かもしれない
        double typeErrorRate = 0.0;
        if (stats.accessCount > 0) {
            typeErrorRate = static_cast<double>(stats.typeErrors) / stats.accessCount;
        }
        
        if (typeErrorRate > options.thresholds.maxTypeErrorRate) {
            if (PerformSpecializeOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
            }
        }
        
        // 最適化の最終状態を設定
        result.state = result.modifiedCacheCount > 0 ? 
            ICOptimizationState::FullyOptimized : ICOptimizationState::NotOptimized;
        
        // 操作回数を記録
        result.optimizationCount = result.modifiedCacheCount;
    } catch (const std::exception& e) {
        result.success = false;
        result.state = ICOptimizationState::Failed;
        result.errorMessage = "Exception during frequency-based optimization: " + std::string(e.what());
    }
    
    return result;
}

ICOptimizationResult ICOptimizer::OptimizeByPattern(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager)
{
    ICOptimizationResult result;
    result.optimizationId = "pattern-" + cacheId + "-" + std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());
    
    // パフォーマンスアナライザからアクセス履歴を取得
    std::vector<ICAccessHistoryEntry> history = 
        ICPerformanceAnalyzer::Instance().GetAccessHistory(cacheId);
    
    if (history.empty()) {
        result.success = false;
        result.errorMessage = "No access history available for optimization";
        return result;
    }
    
    try {
        // 最適化の初期状態を設定
        result.success = true;
        result.state = ICOptimizationState::Optimizing;
        
        // アクセスパターンを分析
        // （実際の実装では、もっと複雑なパターン検出アルゴリズムが必要）
        
        // ヒットとミスのパターンを調べる簡易版
        std::vector<ICAccessResult> resultPattern;
        for (const auto& entry : history) {
            resultPattern.push_back(entry.result);
        }
        
        // パターン：連続するミスが多い場合は、キャッシュを拡大または特化する
        size_t consecutiveMissCount = 0;
        size_t maxConsecutiveMisses = 0;
        
        for (const auto& result : resultPattern) {
            if (result == ICAccessResult::Miss) {
                consecutiveMissCount++;
                maxConsecutiveMisses = std::max(maxConsecutiveMisses, consecutiveMissCount);
            } else {
                consecutiveMissCount = 0;
            }
        }
        
        if (maxConsecutiveMisses > 5) {  // 閾値は調整可能
            // 連続するミスが多い場合は拡大操作
            if (PerformExpandOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
            }
        }
        
        // パターン：ヒットとミスが交互に続く場合は、多態性の問題かもしれない
        bool alternatingPattern = true;
        for (size_t i = 2; i < resultPattern.size(); i++) {
            if ((resultPattern[i] == ICAccessResult::Hit && resultPattern[i-1] == ICAccessResult::Hit) ||
                (resultPattern[i] == ICAccessResult::Miss && resultPattern[i-1] == ICAccessResult::Miss)) {
                alternatingPattern = false;
                break;
            }
        }
        
        if (alternatingPattern && resultPattern.size() > 10) {
            // 特化操作を実行
            if (PerformSpecializeOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
            }
        }
        
        // パターン：無効化が多い場合は、プルーニングが必要かもしれない
        size_t invalidationCount = 0;
        for (const auto& result : resultPattern) {
            if (result == ICAccessResult::Invalidated) {
                invalidationCount++;
            }
        }
        
        if (static_cast<double>(invalidationCount) / resultPattern.size() > 0.1) {  // 10%以上が無効化
            if (PerformPruneOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
            }
        }
        
        // 最適化の最終状態を設定
        result.state = result.modifiedCacheCount > 0 ? 
            ICOptimizationState::FullyOptimized : ICOptimizationState::NotOptimized;
        
        // 操作回数を記録
        result.optimizationCount = result.modifiedCacheCount;
    } catch (const std::exception& e) {
        result.success = false;
        result.state = ICOptimizationState::Failed;
        result.errorMessage = "Exception during pattern-based optimization: " + std::string(e.what());
    }
    
    return result;
}

ICOptimizationResult ICOptimizer::OptimizeByProfile(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager)
{
    ICOptimizationResult result;
    result.optimizationId = "profile-" + cacheId + "-" + std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());
    
    try {
        // 最適化の初期状態を設定
        result.success = true;
        result.state = ICOptimizationState::Optimizing;
        
        // パフォーマンスアドバイスを取得
        std::vector<ICPerformanceAdvice> advice = 
            ICPerformanceAnalyzer::Instance().GeneratePerformanceAdviceForCache(cacheId);
        
        if (advice.empty()) {
            result.success = true;
            result.state = ICOptimizationState::NotOptimized;
            result.errorMessage = "No performance advice available for optimization";
            return result;
        }
        
        // アドバイスに基づいて最適化操作を選択
        for (const auto& adv : advice) {
            if (adv.impact < 0.1) {  // 影響が小さいアドバイスはスキップ
                continue;
            }
            
            if (adv.advice.find("ヒット率が低い") != std::string::npos) {
                // ヒット率が低いアドバイスに対しては拡大または特化
                if (PerformExpandOperation(cacheId, type, options, cacheManager, result)) {
                    result.modifiedCacheCount++;
                }
                
                if (PerformSpecializeOperation(cacheId, type, options, cacheManager, result)) {
                    result.modifiedCacheCount++;
                }
            } else if (adv.advice.find("無効化率が高い") != std::string::npos) {
                // 無効化率が高いアドバイスに対してはプルーニング
                if (PerformPruneOperation(cacheId, type, options, cacheManager, result)) {
                    result.modifiedCacheCount++;
                }
            } else if (adv.advice.find("型エラー率が高い") != std::string::npos) {
                // 型エラー率が高いアドバイスに対しては特化
                if (PerformSpecializeOperation(cacheId, type, options, cacheManager, result)) {
                    result.modifiedCacheCount++;
                }
            }
        }
        
        // 最適化の最終状態を設定
        result.state = result.modifiedCacheCount > 0 ? 
            ICOptimizationState::FullyOptimized : ICOptimizationState::NotOptimized;
        
        // 操作回数を記録
        result.optimizationCount = result.modifiedCacheCount;
    } catch (const std::exception& e) {
        result.success = false;
        result.state = ICOptimizationState::Failed;
        result.errorMessage = "Exception during profile-based optimization: " + std::string(e.what());
    }
    
    return result;
}

ICOptimizationResult ICOptimizer::OptimizeByHeuristic(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager)
{
    ICOptimizationResult result;
    result.optimizationId = "heuristic-" + cacheId + "-" + std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());
    
    try {
        // 最適化の初期状態を設定
        result.success = true;
        result.state = ICOptimizationState::Optimizing;
        
        // キャッシュの統計情報を取得
        ICAccessStats stats = ICPerformanceAnalyzer::Instance().GetStatsForCache(cacheId);
        
        // ヒューリスティックルール1: アクセス数が多く、ヒット率が低い場合は特化
        if (stats.accessCount > 1000 && 
            stats.hits < stats.misses && 
            stats.misses > 0) {
            if (PerformSpecializeOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
            }
        }
        
        // ヒューリスティックルール2: 型エラーが多い場合は一般化
        if (stats.typeErrors > stats.accessCount * 0.2) {  // 20%以上が型エラー
            if (PerformGeneralizeOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
            }
        }
        
        // ヒューリスティックルール3: 無効化が多く、アクセス数が少ない場合は縮小
        if (stats.invalidations > stats.accessCount * 0.3 && stats.accessCount < 500) {
            if (PerformContractOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
            }
        }
        
        // ヒューリスティックルール4: 型エラーが少なく、ミスが多い場合は拡大
        if (stats.typeErrors < stats.accessCount * 0.05 && 
            stats.misses > stats.hits && 
            stats.hits > 0) {
            if (PerformExpandOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
            }
        }
        
        // ヒューリスティックルール5: アクセス数が多く、全体的に良好なパフォーマンスの場合は整理
        if (stats.accessCount > 5000 && 
            static_cast<double>(stats.hits) / (stats.hits + stats.misses) > 0.8) {
            if (PerformReorganizeOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
            }
        }
        
        // 最適化の最終状態を設定
        result.state = result.modifiedCacheCount > 0 ? 
            ICOptimizationState::FullyOptimized : ICOptimizationState::NotOptimized;
        
        // 操作回数を記録
        result.optimizationCount = result.modifiedCacheCount;
    } catch (const std::exception& e) {
        result.success = false;
        result.state = ICOptimizationState::Failed;
        result.errorMessage = "Exception during heuristic-based optimization: " + std::string(e.what());
    }
    
    return result;
}

ICOptimizationResult ICOptimizer::OptimizeByAdaptive(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager)
{
    ICOptimizationResult result;
    result.optimizationId = "adaptive-" + cacheId + "-" + std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());
    
    try {
        // 最適化の初期状態を設定
        result.success = true;
        result.state = ICOptimizationState::Optimizing;
        
        // 過去の最適化履歴を取得
        std::vector<ICOptimizationResult> history = GetOptimizationHistory(cacheId);
        
        // 過去に最適化を行っていない場合は、頻度ベースの最適化を使用
        if (history.empty()) {
            return OptimizeByFrequency(cacheId, type, options, cacheManager);
        }
        
        // 直近の最適化結果を取得
        const ICOptimizationResult& lastOptimization = history.back();
        
        // 最後の最適化が失敗した場合、別の戦略を試す
        if (!lastOptimization.success) {
            // 最後に使われたIDからどの戦略が使われたかを推測
            if (lastOptimization.optimizationId.find("freq-") != std::string::npos) {
                return OptimizeByPattern(cacheId, type, options, cacheManager);
            } else if (lastOptimization.optimizationId.find("pattern-") != std::string::npos) {
                return OptimizeByProfile(cacheId, type, options, cacheManager);
            } else if (lastOptimization.optimizationId.find("profile-") != std::string::npos) {
                return OptimizeByHeuristic(cacheId, type, options, cacheManager);
            } else {
                return OptimizeByFrequency(cacheId, type, options, cacheManager);
            }
        }
        
        // 最後の最適化が成功したが、改善が小さい場合は別の戦略を試す
        if (lastOptimization.performanceImprovement < 0.05) {  // 5%未満の改善
            // 最後に使われたIDからどの戦略が使われたかを推測
            if (lastOptimization.optimizationId.find("freq-") != std::string::npos) {
                return OptimizeByPattern(cacheId, type, options, cacheManager);
            } else if (lastOptimization.optimizationId.find("pattern-") != std::string::npos) {
                return OptimizeByProfile(cacheId, type, options, cacheManager);
            } else if (lastOptimization.optimizationId.find("profile-") != std::string::npos) {
                return OptimizeByHeuristic(cacheId, type, options, cacheManager);
            } else if (lastOptimization.optimizationId.find("heuristic-") != std::string::npos) {
                // すべての戦略を試した場合は、最も成功した戦略を再度使用
                double bestImprovement = 0.0;
                std::string bestStrategy = "freq";
                
                for (const auto& opt : history) {
                    if (opt.performanceImprovement > bestImprovement) {
                        bestImprovement = opt.performanceImprovement;
                        
                        if (opt.optimizationId.find("freq-") != std::string::npos) {
                            bestStrategy = "freq";
                        } else if (opt.optimizationId.find("pattern-") != std::string::npos) {
                            bestStrategy = "pattern";
                        } else if (opt.optimizationId.find("profile-") != std::string::npos) {
                            bestStrategy = "profile";
                        } else if (opt.optimizationId.find("heuristic-") != std::string::npos) {
                            bestStrategy = "heuristic";
                        }
                    }
                }
                
                if (bestStrategy == "freq") {
                    return OptimizeByFrequency(cacheId, type, options, cacheManager);
                } else if (bestStrategy == "pattern") {
                    return OptimizeByPattern(cacheId, type, options, cacheManager);
                } else if (bestStrategy == "profile") {
                    return OptimizeByProfile(cacheId, type, options, cacheManager);
                } else {
                    return OptimizeByHeuristic(cacheId, type, options, cacheManager);
                }
            } else {
                return OptimizeByFrequency(cacheId, type, options, cacheManager);
            }
        }
        
        // 最後の最適化が成功し、良い改善があった場合は同じ戦略を使う
        if (lastOptimization.optimizationId.find("freq-") != std::string::npos) {
            return OptimizeByFrequency(cacheId, type, options, cacheManager);
        } else if (lastOptimization.optimizationId.find("pattern-") != std::string::npos) {
            return OptimizeByPattern(cacheId, type, options, cacheManager);
        } else if (lastOptimization.optimizationId.find("profile-") != std::string::npos) {
            return OptimizeByProfile(cacheId, type, options, cacheManager);
        } else if (lastOptimization.optimizationId.find("heuristic-") != std::string::npos) {
            return OptimizeByHeuristic(cacheId, type, options, cacheManager);
        } else {
            return OptimizeByFrequency(cacheId, type, options, cacheManager);
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.state = ICOptimizationState::Failed;
        result.errorMessage = "Exception during adaptive optimization: " + std::string(e.what());
    }
    
    return result;
}

//==============================================================================
// 最適化操作の実装
//==============================================================================

bool ICOptimizer::PerformSpecializeOperation(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager,
    ICOptimizationResult& result)
{
    // 特化操作のログを出力
    std::stringstream ss;
    ss << "Performing specialization operation on cache '" << cacheId << "'";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    
    try {
        // 実際の実装では、キャッシュマネージャを使用して特化操作を実行する
        // この例では、操作が成功したと仮定して結果を更新するだけ
        
        // キャッシュエントリの特化の成功を記録
        result.specializedEntryCount++;
        
        // 操作が成功したことを記録
        return true;
    } catch (const std::exception& e) {
        // エラーをログに記録
        std::stringstream errorSs;
        errorSs << "Error during specialization operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
        
        // 操作が失敗したことを記録
        return false;
    }
}

bool ICOptimizer::PerformGeneralizeOperation(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager,
    ICOptimizationResult& result)
{
    // 一般化操作のログを出力
    std::stringstream ss;
    ss << "Performing generalization operation on cache '" << cacheId << "'";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    
    try {
        // 実際の実装では、キャッシュマネージャを使用して一般化操作を実行する
        // この例では、操作が成功したと仮定して結果を更新するだけ
        
        // エントリの削除と追加を記録（一般化では通常、より具体的なエントリが削除され、より一般的なエントリが追加される）
        result.deletedEntryCount += 2;  // 仮の値
        result.addedEntryCount += 1;    // 仮の値
        
        // 操作が成功したことを記録
        return true;
    } catch (const std::exception& e) {
        // エラーをログに記録
        std::stringstream errorSs;
        errorSs << "Error during generalization operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
        
        // 操作が失敗したことを記録
        return false;
    }
}

bool ICOptimizer::PerformExpandOperation(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager,
    ICOptimizationResult& result)
{
    // 拡大操作のログを出力
    std::stringstream ss;
    ss << "Performing expand operation on cache '" << cacheId << "'";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    
    try {
        // 実際の実装では、キャッシュマネージャを使用して拡大操作を実行する
        // この例では、操作が成功したと仮定して結果を更新するだけ
        
        // エントリの追加を記録（拡大では通常、より多くのエントリが追加される）
        result.addedEntryCount += 3;  // 仮の値
        
        // 操作が成功したことを記録
        return true;
    } catch (const std::exception& e) {
        // エラーをログに記録
        std::stringstream errorSs;
        errorSs << "Error during expand operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
        
        // 操作が失敗したことを記録
        return false;
    }
}

bool ICOptimizer::PerformContractOperation(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager,
    ICOptimizationResult& result)
{
    // 縮小操作のログを出力
    std::stringstream ss;
    ss << "Performing contract operation on cache '" << cacheId << "'";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    
    try {
        // 実際の実装では、キャッシュマネージャを使用して縮小操作を実行する
        // この例では、操作が成功したと仮定して結果を更新するだけ
        
        // エントリの削除を記録（縮小では通常、エントリが削除される）
        result.deletedEntryCount += 2;  // 仮の値
        
        // 操作が成功したことを記録
        return true;
    } catch (const std::exception& e) {
        // エラーをログに記録
        std::stringstream errorSs;
        errorSs << "Error during contract operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
        
        // 操作が失敗したことを記録
        return false;
    }
}

bool ICOptimizer::PerformMergeOperation(
    const std::vector<std::string>& cacheIds,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager,
    ICOptimizationResult& result)
{
    // マージ操作のログを出力
    std::stringstream ss;
    ss << "Performing merge operation on " << cacheIds.size() << " caches";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    
    try {
        // 実際の実装では、キャッシュマネージャを使用してマージ操作を実行する
        // この例では、操作が成功したと仮定して結果を更新するだけ
        
        // エントリの削除と追加を記録（マージでは通常、複数のキャッシュからエントリが削除され、
        // 新しいマージされたキャッシュにエントリが追加される）
        result.deletedEntryCount += cacheIds.size() * 2;  // 仮の値
        result.addedEntryCount += cacheIds.size();       // 仮の値
        
        // 操作が成功したことを記録
        return true;
    } catch (const std::exception& e) {
        // エラーをログに記録
        std::stringstream errorSs;
        errorSs << "Error during merge operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
        
        // 操作が失敗したことを記録
        return false;
    }
}

bool ICOptimizer::PerformSplitOperation(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager,
    ICOptimizationResult& result)
{
    // 分割操作のログを出力
    std::stringstream ss;
    ss << "Performing split operation on cache '" << cacheId << "'";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    
    try {
        // 実際の実装では、キャッシュマネージャを使用して分割操作を実行する
        // この例では、操作が成功したと仮定して結果を更新するだけ
        
        // エントリの削除と追加を記録（分割では通常、1つのキャッシュからエントリが削除され、
        // 複数の新しいキャッシュにエントリが追加される）
        result.deletedEntryCount += 1;  // 仮の値
        result.addedEntryCount += 2;    // 仮の値
        
        // 操作が成功したことを記録
        return true;
    } catch (const std::exception& e) {
        // エラーをログに記録
        std::stringstream errorSs;
        errorSs << "Error during split operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
        
        // 操作が失敗したことを記録
        return false;
    }
}

bool ICOptimizer::PerformReorganizeOperation(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager,
    ICOptimizationResult& result)
{
    // 整理操作のログを出力
    std::stringstream ss;
    ss << "Performing reorganize operation on cache '" << cacheId << "'";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    
    try {
        // 実際の実装では、キャッシュマネージャを使用して整理操作を実行する
        // この例では、操作が成功したと仮定して結果を更新するだけ
        
        // エントリの削除と追加を記録（整理では通常、エントリの順序が変更されるだけで、
        // 数は変わらないが、内部的には削除と追加のように見える場合がある）
        result.deletedEntryCount += 1;  // 仮の値
        result.addedEntryCount += 1;    // 仮の値
        
        // 操作が成功したことを記録
        return true;
    } catch (const std::exception& e) {
        // エラーをログに記録
        std::stringstream errorSs;
        errorSs << "Error during reorganize operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
        
        // 操作が失敗したことを記録
        return false;
    }
}

bool ICOptimizer::PerformPruneOperation(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager,
    ICOptimizationResult& result)
{
    // 刈り込み操作のログを出力
    std::stringstream ss;
    ss << "Performing prune operation on cache '" << cacheId << "'";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    
    try {
        // 実際の実装では、キャッシュマネージャを使用して刈り込み操作を実行する
        // この例では、操作が成功したと仮定して結果を更新するだけ
        
        // エントリの削除を記録（刈り込みでは通常、不要なエントリが削除される）
        result.deletedEntryCount += 3;  // 仮の値
        
        // 操作が成功したことを記録
        return true;
    } catch (const std::exception& e) {
        // エラーをログに記録
        std::stringstream errorSs;
        errorSs << "Error during prune operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
        
        // 操作が失敗したことを記録
        return false;
    }
}

bool ICOptimizer::PerformPreloadOperation(
    const std::vector<std::string>& cacheIds,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager,
    ICOptimizationResult& result)
{
    // 先読み操作のログを出力
    std::stringstream ss;
    ss << "Performing preload operation on " << cacheIds.size() << " caches";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    
    try {
        // 実際の実装では、キャッシュマネージャを使用して先読み操作を実行する
        // この例では、操作が成功したと仮定して結果を更新するだけ
        
        // エントリの追加を記録（先読みでは通常、予測されるエントリが事前に追加される）
        result.addedEntryCount += cacheIds.size() * 2;  // 仮の値
        
        // 操作が成功したことを記録
        return true;
    } catch (const std::exception& e) {
        // エラーをログに記録
        std::stringstream errorSs;
        errorSs << "Error during preload operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
        
        // 操作が失敗したことを記録
        return false;
    }
}

//==============================================================================
// レポート生成メソッド
//==============================================================================

std::string ICOptimizer::GenerateOptimizationReport(bool detailed) const {
    std::stringstream ss;
    
    // レポートのヘッダー
    ss << "===================================================================\n";
    ss << "             インラインキャッシュ最適化レポート                    \n";
    ss << "===================================================================\n\n";
    
    // 現在の時刻を取得してレポートに含める
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    ss << "生成日時: " << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S") << "\n\n";
    
    // 全体の最適化統計
    ss << "全体の最適化統計:\n";
    ss << "  - 合計最適化実行回数: " << m_totalOptimizationCount.load() << "\n";
    
    // キャッシュの優先度分布
    std::unordered_map<ICPriorityLevel, size_t> priorityCounts;
    {
        std::lock_guard<std::mutex> lock(m_priorityMutex);
        for (const auto& pair : m_cachePriorities) {
            priorityCounts[pair.second]++;
        }
    }
    
    ss << "  - キャッシュ優先度分布:\n";
    ss << "    - Critical: " << priorityCounts[ICPriorityLevel::Critical] << "\n";
    ss << "    - High: " << priorityCounts[ICPriorityLevel::High] << "\n";
    ss << "    - Medium: " << priorityCounts[ICPriorityLevel::Medium] << "\n";
    ss << "    - Low: " << priorityCounts[ICPriorityLevel::Low] << "\n";
    ss << "    - Background: " << priorityCounts[ICPriorityLevel::Background] << "\n\n";
    
    // 詳細レポートの場合、個々のキャッシュの最適化履歴を含める
    if (detailed) {
        ss << "キャッシュ別の最適化履歴:\n";
        
        std::lock_guard<std::mutex> lock(m_historyMutex);
        for (const auto& pair : m_optimizationHistory) {
            const std::string& cacheId = pair.first;
            const std::vector<ICOptimizationResult>& history = pair.second;
            
            ss << "  - Cache ID: " << cacheId << "\n";
            ss << "    - 最適化回数: " << history.size() << "\n";
            
            // 最新の最適化結果を詳細に表示
            if (!history.empty()) {
                const ICOptimizationResult& latest = history.back();
                
                ss << "    - 最新の最適化結果:\n";
                ss << "      - 最適化ID: " << latest.optimizationId << "\n";
                ss << "      - 成功: " << (latest.success ? "はい" : "いいえ") << "\n";
                ss << "      - 状態: ";
                
                switch (latest.state) {
                    case ICOptimizationState::NotOptimized:
                        ss << "最適化なし";
                        break;
                    case ICOptimizationState::Optimizing:
                        ss << "最適化中";
                        break;
                    case ICOptimizationState::PartiallyOptimized:
                        ss << "部分的に最適化済み";
                        break;
                    case ICOptimizationState::FullyOptimized:
                        ss << "完全に最適化済み";
                        break;
                    case ICOptimizationState::Failed:
                        ss << "失敗";
                        break;
                    default:
                        ss << "不明";
                        break;
                }
                ss << "\n";
                
                ss << "      - 変更されたキャッシュ数: " << latest.modifiedCacheCount << "\n";
                ss << "      - 削除されたエントリ数: " << latest.deletedEntryCount << "\n";
                ss << "      - 追加されたエントリ数: " << latest.addedEntryCount << "\n";
                ss << "      - 特化されたエントリ数: " << latest.specializedEntryCount << "\n";
                ss << "      - ヒット率変化: " 
                   << std::fixed << std::setprecision(2) << (latest.hitRateBeforeOptimization * 100.0) << "% -> " 
                   << std::fixed << std::setprecision(2) << (latest.hitRateAfterOptimization * 100.0) << "%\n";
                ss << "      - パフォーマンス改善率: " 
                   << std::fixed << std::setprecision(2) << (latest.performanceImprovement * 100.0) << "%\n";
                ss << "      - 最適化時間: " << latest.optimizationTime.count() << " ms\n";
                
                if (!latest.success && !latest.errorMessage.empty()) {
                    ss << "      - エラーメッセージ: " << latest.errorMessage << "\n";
                }
            }
            
            // すべての最適化結果の概要を表示
            if (history.size() > 1) {
                ss << "    - 過去の最適化履歴:\n";
                
                for (size_t i = 0; i < history.size() - 1; ++i) {
                    const ICOptimizationResult& result = history[i];
                    
                    ss << "      - [" << i + 1 << "] "
                       << (result.success ? "成功" : "失敗") << ", ";
                    
                    switch (result.state) {
                        case ICOptimizationState::NotOptimized:
                            ss << "最適化なし";
                            break;
                        case ICOptimizationState::Optimizing:
                            ss << "最適化中";
                            break;
                        case ICOptimizationState::PartiallyOptimized:
                            ss << "部分的に最適化済み";
                            break;
                        case ICOptimizationState::FullyOptimized:
                            ss << "完全に最適化済み";
                            break;
                        case ICOptimizationState::Failed:
                            ss << "失敗";
                            break;
                        default:
                            ss << "不明";
                            break;
                    }
                    
                    ss << ", パフォーマンス改善率: " 
                       << std::fixed << std::setprecision(2) << (result.performanceImprovement * 100.0) << "%\n";
                }
            }
            
            ss << "\n";
        }
    }
    
    ss << "===================================================================\n";
    
    return ss.str();
}

std::string ICOptimizer::GenerateJsonOptimizationReport() const {
    using json = nlohmann::json;
    
    json report;
    
    // 現在の時刻を取得してレポートに含める
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream timeStream;
    timeStream << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
    report["timestamp"] = timeStream.str();
    
    // 全体の最適化統計
    report["total_optimization_count"] = m_totalOptimizationCount.load();
    
    // キャッシュの優先度分布
    json priorityDistribution;
    {
        std::lock_guard<std::mutex> lock(m_priorityMutex);
        std::unordered_map<ICPriorityLevel, size_t> priorityCounts;
        
        for (const auto& pair : m_cachePriorities) {
            priorityCounts[pair.second]++;
        }
        
        priorityDistribution["critical"] = priorityCounts[ICPriorityLevel::Critical];
        priorityDistribution["high"] = priorityCounts[ICPriorityLevel::High];
        priorityDistribution["medium"] = priorityCounts[ICPriorityLevel::Medium];
        priorityDistribution["low"] = priorityCounts[ICPriorityLevel::Low];
        priorityDistribution["background"] = priorityCounts[ICPriorityLevel::Background];
    }
    report["priority_distribution"] = priorityDistribution;
    
    // キャッシュ別の最適化履歴
    json cacheHistories = json::array();
    
    std::lock_guard<std::mutex> lock(m_historyMutex);
    for (const auto& pair : m_optimizationHistory) {
        const std::string& cacheId = pair.first;
        const std::vector<ICOptimizationResult>& history = pair.second;
        
        json cacheHistory;
        cacheHistory["cache_id"] = cacheId;
        cacheHistory["optimization_count"] = history.size();
        
        json optimizationResults = json::array();
        for (const auto& result : history) {
            json resultJson;
            resultJson["optimization_id"] = result.optimizationId;
            resultJson["success"] = result.success;
            
            switch (result.state) {
                case ICOptimizationState::NotOptimized:
                    resultJson["state"] = "not_optimized";
                    break;
                case ICOptimizationState::Optimizing:
                    resultJson["state"] = "optimizing";
                    break;
                case ICOptimizationState::PartiallyOptimized:
                    resultJson["state"] = "partially_optimized";
                    break;
                case ICOptimizationState::FullyOptimized:
                    resultJson["state"] = "fully_optimized";
                    break;
                case ICOptimizationState::Failed:
                    resultJson["state"] = "failed";
                    break;
                default:
                    resultJson["state"] = "unknown";
                    break;
            }
            
            resultJson["optimization_count"] = result.optimizationCount;
            resultJson["modified_cache_count"] = result.modifiedCacheCount;
            resultJson["deleted_entry_count"] = result.deletedEntryCount;
            resultJson["added_entry_count"] = result.addedEntryCount;
            resultJson["specialized_entry_count"] = result.specializedEntryCount;
            resultJson["hit_rate_before"] = result.hitRateBeforeOptimization;
            resultJson["hit_rate_after"] = result.hitRateAfterOptimization;
            resultJson["performance_improvement"] = result.performanceImprovement;
            resultJson["optimization_time_ms"] = result.optimizationTime.count();
            
            if (!result.success && !result.errorMessage.empty()) {
                resultJson["error_message"] = result.errorMessage;
            }
            
            optimizationResults.push_back(resultJson);
        }
        
        cacheHistory["optimization_results"] = optimizationResults;
        cacheHistories.push_back(cacheHistory);
    }
    
    report["cache_histories"] = cacheHistories;
    
    return report.dump(4); // インデント付きでJSON文字列を生成
}

} // namespace core
} // namespace aerojs 