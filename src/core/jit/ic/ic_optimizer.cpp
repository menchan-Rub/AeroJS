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
        // キャッシュタイプをキャッシュマネージャから取得
        ICType type = ICType::Property;
        if (cacheManager) {
            auto cache = cacheManager->GetCache(cacheId);
            if (cache) {
                type = cache->GetType();
            }
        }
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
                    // キャッシュタイプを取得
                    ICType type = ICType::Property; // デフォルト値。取得失敗時に使用される。
                    if (cacheManager) {
                        auto cache = cacheManager->GetCache(cacheId);
                        if (cache) {
                            type = cache->GetType();
                        } else {
                            // キャッシュIDに対応するキャッシュが見つからない場合
                            std::stringstream ss;
                            ss << "Cache not found for ID: " << cacheId << ". Using default ICType::Property.";
                            ICLogger::Instance().Warning(ss.str(), "ICOptimizer");
                        }
                    } else {
                        // キャッシュマネージャが無効な場合
                        std::stringstream ss;
                        ss << "CacheManager is null. Cannot retrieve ICType for cache ID: " << cacheId << ". Using default ICType::Property.";
                        ICLogger::Instance().Warning(ss.str(), "ICOptimizer");
                    }
                    
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
        
        // より洗練されたパターン分析アルゴリズムの実装
        struct PatternFeatures {
            double hitRate;
            double missRate;
            double invalidationRate;
            double typeErrorRate;
            size_t maxConsecutiveHits;
            size_t maxConsecutiveMisses;
            double hitMissRatio;
            double patternEstropy; // Entropy of access results (Hit, Miss, etc.)
            bool hasAlternatingPattern;
            bool hasCyclicPattern;
            size_t patternPeriod;
            bool hasShapeConflict;
            double objectTypeVariety;
            bool hasPolymorphicAccess;

            // N-gram features (bigrams of ICAccessResult)
            size_t missMissCount; // Count of Miss -> Miss sequences
            size_t missHitCount;  // Count of Miss -> Hit sequences
            size_t hitMissCount;  // Count of Hit -> Miss sequences
            size_t hitHitCount;   // Count of Hit -> Hit sequences
            // Add more n-grams or other sequence features as needed
        };
        
        // パターン特徴量を抽出
        PatternFeatures features{}; // Initialize all members to zero/false/default
        
        // ヒットとミスの基本的な統計
        size_t hitCount = 0;
        size_t missCount = 0;
        size_t invalidationCount = 0;
        size_t typeErrorCount = 0;
        
        std::vector<ICAccessResult> resultPattern;
        std::vector<std::string> objectTypes;
        std::vector<uint64_t> shapeIds;
        
        // アクセス履歴から詳細情報を抽出
        for (const auto& entry : history) {
            resultPattern.push_back(entry.result);
            
            // カウンタを更新
            if (entry.result == ICAccessResult::Hit) {
                hitCount++;
            } else if (entry.result == ICAccessResult::Miss) {
                missCount++;
            } else if (entry.result == ICAccessResult::Invalidated) {
                invalidationCount++;
            } else if (entry.result == ICAccessResult::TypeError) {
                typeErrorCount++;
            }
            
            // オブジェクトタイプと形状情報を収集
            if (entry.objectType && !entry.objectType->empty()) {
                objectTypes.push_back(*entry.objectType);
            }
            
            if (entry.shapeId) {
                shapeIds.push_back(*entry.shapeId);
            }
        }
        
        const size_t totalAccesses = resultPattern.size();
        if (totalAccesses == 0) {
            result.success = false;
            result.state = ICOptimizationState::NotOptimized;
            result.errorMessage = "No access history available for analysis";
            return result;
        }
        
        // 基本的な統計特徴量
        features.hitRate = static_cast<double>(hitCount) / totalAccesses;
        features.missRate = static_cast<double>(missCount) / totalAccesses;
        features.invalidationRate = static_cast<double>(invalidationCount) / totalAccesses;
        features.typeErrorRate = static_cast<double>(typeErrorCount) / totalAccesses;
        features.hitMissRatio = missCount > 0 ? static_cast<double>(hitCount) / missCount : hitCount;
        
        // 連続する同じ結果の最大数を計算
        size_t consecutiveHits = 0;
        size_t consecutiveMisses = 0;
        features.maxConsecutiveHits = 0;
        features.maxConsecutiveMisses = 0;
        
        // Initialize N-gram counts
        features.missMissCount = 0;
        features.missHitCount = 0;
        features.hitMissCount = 0;
        features.hitHitCount = 0;

        ICAccessResult previousResult = ICAccessResult::Unknown; // Initialize with a value not part of typical sequence start

        for (size_t i = 0; i < resultPattern.size(); ++i) {
            const auto& currentResult = resultPattern[i];
            if (currentResult == ICAccessResult::Hit) {
                consecutiveHits++;
                consecutiveMisses = 0;
                features.maxConsecutiveHits = std::max(features.maxConsecutiveHits, consecutiveHits);
                if (i > 0 && previousResult == ICAccessResult::Hit) {
                    features.hitHitCount++;
                } else if (i > 0 && previousResult == ICAccessResult::Miss) {
                    features.missHitCount++;
                }
            } else if (currentResult == ICAccessResult::Miss) {
                consecutiveMisses++;
                consecutiveHits = 0;
                features.maxConsecutiveMisses = std::max(features.maxConsecutiveMisses, consecutiveMisses);
                if (i > 0 && previousResult == ICAccessResult::Miss) {
                    features.missMissCount++;
                } else if (i > 0 && previousResult == ICAccessResult::Hit) {
                    features.hitMissCount++;
                }
            } else { // For TypeError, Invalidated, etc.
                consecutiveHits = 0;
                consecutiveMisses = 0;
                // N-grams involving these could be added if deemed useful
            }
            previousResult = currentResult;
        }
        
        // パターンのエントロピー計算（パターンの複雑さの指標）
        std::unordered_map<ICAccessResult, size_t> resultCounts;
        for (const auto& result : resultPattern) {
            resultCounts[result]++;
        }
        
        double entropy = 0.0;
        for (const auto& pair : resultCounts) {
            double probability = static_cast<double>(pair.second) / totalAccesses;
            if (probability > 0) {
                entropy -= probability * log2(probability);
            }
        }
        features.patternEstropy = entropy;
        
        // 交互パターン検出（ヒット/ミスが交互に発生）
        features.hasAlternatingPattern = resultPattern.size() >= 4;
        for (size_t i = 2; i < resultPattern.size() && features.hasAlternatingPattern; i++) {
            if ((resultPattern[i] == resultPattern[i-2]) != (resultPattern[i-1] == resultPattern[i-3])) {
                features.hasAlternatingPattern = false;
            }
        }
        
        // 周期的パターン検出（周期的なアクセスパターンの有無と周期）
        features.hasCyclicPattern = false;
        features.patternPeriod = 0;
        
        // 周期は2から全体の1/3までの範囲で検索（短い周期を優先）
        for (size_t period = 2; period <= totalAccesses / 3 && !features.hasCyclicPattern; period++) {
            bool isPeriodic = true;
            // 少なくとも3周期分のデータがある場合のみ検討
            if (totalAccesses >= period * 3) {
                for (size_t i = 0; i < 2 * period && isPeriodic; i++) {
                    for (size_t j = 1; j < totalAccesses / period - 1 && isPeriodic; j++) {
                        if (i + j * period < totalAccesses && 
                            resultPattern[i] != resultPattern[i + j * period]) {
                            isPeriodic = false;
                        }
                    }
                }
                if (isPeriodic) {
                    features.hasCyclicPattern = true;
                    features.patternPeriod = period;
                    break;
                }
            }
        }
        
        // シェイプの競合検出（同じプロパティに対して異なるシェイプからのアクセス）
        std::unordered_set<uint64_t> uniqueShapes(shapeIds.begin(), shapeIds.end());
        features.hasShapeConflict = uniqueShapes.size() > 1;
        
        // オブジェクトタイプの多様性
        std::unordered_set<std::string> uniqueTypes(objectTypes.begin(), objectTypes.end());
        features.objectTypeVariety = static_cast<double>(uniqueTypes.size()) / std::max<size_t>(objectTypes.size(), 1);
        
        // 多態アクセスの検出（複数の異なるタイプによるアクセス）
        features.hasPolymorphicAccess = uniqueTypes.size() > 1;
        
        // パターン特徴量に基づいて最適化戦略を選択
        // コメント更新: 既存の統計的特徴に加え、アクセス結果のバイグラム（連続2要素のパターン）も考慮して最適化判断を行う。
        // さらなる改善として、より長いN-gram、隠れマルコフモデル、時系列分析などが考えられる。
        
        // パターン1: 連続するミスが多い、またはMiss-Missバイグラムが多い場合
        // （ポリモーフィック/メガモーフィックキャッシュへの拡大が必要）
        if (features.maxConsecutiveMisses > options.thresholds.maxConsecutiveMissesForExpansion || 
            (features.missRate > options.thresholds.missRateForExpansion && features.hasShapeConflict) ||
            (totalAccesses > 1 && static_cast<double>(features.missMissCount) / (totalAccesses -1) > options.thresholds.missMissBigramRateForExpansion) ) {
            if (PerformExpandOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
                result.recommendations.push_back("キャッシュを拡大して複数のシェイプや高頻度の連続ミスに対応");
            }
        }
        
        // パターン2: 周期的なアクセスパターン（特定の使用パターンに合わせた最適化）
        if (features.hasCyclicPattern && features.patternPeriod <= 5) {
            if (PerformSpecializeOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
                result.recommendations.push_back("周期的アクセスパターンに合わせた特化最適化");
            }
        }
        
        // パターン3: 高い無効化率（キャッシュエントリの不安定さ）
        if (features.invalidationRate > 0.15) {
            if (PerformPruneOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
                result.recommendations.push_back("不安定なエントリを削除してキャッシュを安定化");
            }
        }
        
        // パターン4: 型エラーが多い（より汎用的なキャッシュが必要）
        if (features.typeErrorRate > 0.2) {
            if (PerformGeneralizeOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
                result.recommendations.push_back("キャッシュを一般化して型エラーを減少");
            }
        }
        
        // パターン5: 多態アクセスがあるが型は安定している（特化最適化）
        if (features.hasPolymorphicAccess && features.objectTypeVariety < 0.3) {
            if (PerformSpecializeOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
                result.recommendations.push_back("複数タイプに対して特化したキャッシュを作成");
            }
        }
        
        // パターン6: 高いヒット率と安定したパターン（事前ロードの候補）
        if (features.hitRate > 0.8 && features.patternEstropy < 0.5) {
            if (PerformPreloadOperation(cacheId, type, options, cacheManager, result)) {
                result.modifiedCacheCount++;
                result.recommendations.push_back("高頻度アクセスパターンを事前にロード");
            }
        }
        
        // 複数のパターンが検出された場合は複合最適化を適用
        if (result.modifiedCacheCount > 1) {
            if (PerformMergeOperation(cacheId, type, options, cacheManager, result)) {
                result.recommendations.push_back("複数の最適化を組み合わせた複合最適化");
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
    std::stringstream ss;
    ss << "Performing specialization operation on cache '" << cacheId << "'";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    try {
        if (!cacheManager) {
            ICLogger::Instance().Error("Cache manager is null", "ICOptimizer");
            return false;
        }
        auto cache = cacheManager->GetCache(cacheId);
        if (!cache) {
            ICLogger::Instance().Error("Cache not found: " + cacheId, "ICOptimizer");
            return false;
        }
        // 全エントリを取得し、ヒット回数が閾値以上のものを特化
        auto entries = cache->GetEntries();
        size_t specialized = 0;
        for (auto& entry : entries) {
            if (entry.hitCount > options.specialization.minHitThreshold) {
                // 高度な特化処理の実装
                performAdvancedSpecialization(entry, type, options, cacheManager, result);
                specialized++;
            }
        }
        result.specializedEntryCount = specialized;
        cacheManager->GetCache(cacheId)->ResetStats();
        return specialized > 0;
    } catch (const std::exception& e) {
        std::stringstream errorSs;
        errorSs << "Error during specialization operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
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
    std::stringstream ss;
    ss << "Performing generalization operation on cache '" << cacheId << "'";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    try {
        if (!cacheManager) return false;
        auto cache = cacheManager->GetCache(cacheId);
        if (!cache) return false;
        // より具体的なエントリを削除し、より一般的なエントリを追加
        size_t deleted = cache->RemoveSpecializedEntries();
        size_t added = cache->AddGenericEntry();
        result.deletedEntryCount += deleted;
        result.addedEntryCount += added;
        return (deleted > 0 || added > 0);
    } catch (const std::exception& e) {
        std::stringstream errorSs;
        errorSs << "Error during generalization operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
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
        // キャッシュマネージャーの確認
        if (!cacheManager) {
            ICLogger::Instance().Error("キャッシュマネージャーがnullです", "ICOptimizer");
            return false;
        }
        
        // キャッシュを取得
        auto cache = cacheManager->GetCache(cacheId);
        if (!cache) {
            ICLogger::Instance().Error("キャッシュが見つかりません: " + cacheId, "ICOptimizer");
            return false;
        }

        // 現在のキャッシュ容量とエントリ数
        size_t currentCapacity = cache->GetCapacity();
        size_t currentEntryCount = cache->GetEntryCount();
        
        // 拡大が必要かどうかを判断
        if (currentEntryCount >= currentCapacity * 0.75) {  // 75%以上の使用率
            // キャッシュ容量を拡大するために必要な新しいサイズを計算
            size_t newCapacity = currentCapacity * 2;  // 2倍に拡大
            if (newCapacity <= currentCapacity) {  // オーバーフロー対策
                newCapacity = currentCapacity + 8;
            }
            newCapacity = std::min(newCapacity, options.expansion.maxCacheSize); // 上限を設定
            
            // 一定以上の拡大率の場合、警告をログに出力
            if (newCapacity > currentCapacity * 4) {
                std::stringstream warnSs;
                warnSs << "警告: キャッシュ '" << cacheId << "' の容量が大幅に増加しています。" 
                       << currentCapacity << " -> " << newCapacity;
                ICLogger::Instance().Warning(warnSs.str(), "ICOptimizer");
            }
            
            // 統計情報を収集
            auto stats = ICPerformanceAnalyzer::Instance().GetStatsForCache(cacheId);
            
            // 拡大操作の実行
            bool resizeSuccess = cache->Resize(newCapacity);
            if (!resizeSuccess) {
                ICLogger::Instance().Error("キャッシュのリサイズに失敗しました: " + cacheId, "ICOptimizer");
                return false;
            }
            
            // エントリ拡大のための戦略を選択
            size_t targetEntryCount = std::min(
                currentEntryCount + options.expansion.maxNewEntries,
                newCapacity - 1 // 1つの空きスロットを残す
            );
            
            // ミスパターンを分析して適切なエントリを追加
            std::vector<ICAccessHistoryEntry> missHistory = 
                ICPerformanceAnalyzer::Instance().GetMissHistory(cacheId, options.expansion.historyWindow);
            
            // 型情報に基づいてエントリを作成
            std::unordered_map<uint64_t, size_t> shapeFrequency;  // シェイプIDとその頻度
            std::unordered_map<std::string, size_t> typeFrequency; // 型名とその頻度
            
            // ミスパターンからシェイプと型の頻度を抽出
            for (const auto& entry : missHistory) {
                if (entry.shapeId) {
                    shapeFrequency[*entry.shapeId]++;
                }
                
                if (entry.objectType && !entry.objectType->empty()) {
                    typeFrequency[*entry.objectType]++;
                }
            }
            
            // エントリの作成数をカウント
            size_t addedEntries = 0;
            
            // 頻度に基づいて新しいシェイプエントリを追加
            if (!shapeFrequency.empty()) {
                // シェイプ頻度に基づいてソート
                std::vector<std::pair<uint64_t, size_t>> shapePairs(
                    shapeFrequency.begin(), shapeFrequency.end()
                );
                std::sort(shapePairs.begin(), shapePairs.end(), 
                    [](const auto& a, const auto& b) { return a.second > b.second; });
                
                // 最も頻度の高いシェイプから順に追加
                size_t maxShapeEntries = std::min(
                    shapePairs.size(), 
                    options.expansion.maxNewShapeEntries
                );
                
                for (size_t i = 0; i < maxShapeEntries && addedEntries < options.expansion.maxNewEntries; ++i) {
                    uint64_t shapeId = shapePairs[i].first;
                    
                    // シェイプ情報の取得
                    auto shapeInfo = m_runtime->getShapeRegistry()->getShapeById(shapeId);
                    if (!shapeInfo) continue;
                    
                    // コード生成器を取得
                    ICCodeGenerator* codeGen = cacheManager->GetCodeGenerator();
                    if (!codeGen) continue;
                    
                    // 新しいICエントリの作成とキャッシュへの追加
                    ICEntry* newEntry = new ICEntry();
                    newEntry->SetShapeId(shapeId);
                    
                    // シェイプに基づく専用コードの生成
                    void* specializedCode = codeGen->GenerateShapeSpecificCode(cache, shapeInfo);
                    if (specializedCode) {
                        newEntry->SetCode(specializedCode);
                        newEntry->SetPriority(ICEntryPriority::High); // 高優先度を設定
                        
                        // キャッシュに追加
                        if (cache->AddEntry(newEntry)) {
                            addedEntries++;
                            
                            std::stringstream debugSs;
                            debugSs << "シェイプID " << shapeId << " 用の新しいICエントリを追加しました";
                            ICLogger::Instance().Debug(debugSs.str(), "ICOptimizer");
                        } else {
                            delete newEntry; // 追加に失敗した場合、メモリリークを防ぐ
                        }
                    } else {
                        delete newEntry;
                    }
                }
            }
            
            // 型情報に基づいて汎用エントリを追加（シェイプが不足している場合）
            if (addedEntries < options.expansion.maxNewEntries && !typeFrequency.empty()) {
                // 型頻度でソート
                std::vector<std::pair<std::string, size_t>> typePairs(
                    typeFrequency.begin(), typeFrequency.end()
                );
                std::sort(typePairs.begin(), typePairs.end(), 
                    [](const auto& a, const auto& b) { return a.second > b.second; });
                
                // 最も頻度の高い型から順に追加
                size_t maxTypeEntries = std::min(
                    typePairs.size(), 
                    options.expansion.maxNewTypeEntries
                );
                
                for (size_t i = 0; i < maxTypeEntries && addedEntries < options.expansion.maxNewEntries; ++i) {
                    const std::string& typeName = typePairs[i].first;
                    
                    // コード生成器を取得
                    ICCodeGenerator* codeGen = cacheManager->GetCodeGenerator();
                    if (!codeGen) continue;
                    
                    // 新しい汎用ICエントリの作成
                    ICEntry* newEntry = new ICEntry();
                    newEntry->SetTypeName(typeName);
                    
                    // 型に基づく汎用コードの生成
                    void* genericCode = codeGen->GenerateTypeBasedCode(cache, typeName);
                    if (genericCode) {
                        newEntry->SetCode(genericCode);
                        newEntry->SetPriority(ICEntryPriority::Medium); // 中程度の優先度
                        
                        // キャッシュに追加
                        if (cache->AddEntry(newEntry)) {
                            addedEntries++;
                            
                            std::stringstream debugSs;
                            debugSs << "型 '" << typeName << "' 用の汎用ICエントリを追加しました";
                            ICLogger::Instance().Debug(debugSs.str(), "ICOptimizer");
                        } else {
                            delete newEntry; // 追加に失敗した場合、メモリリークを防ぐ
                        }
                    } else {
                        delete newEntry;
                    }
                }
            }
            
            // どんな型にも対応する汎用フォールバックエントリを追加
            if (addedEntries < options.expansion.maxNewEntries && 
                !cache->HasGenericFallback() && 
                options.expansion.addGenericFallback) {
                
                // コード生成器を取得
                ICCodeGenerator* codeGen = cacheManager->GetCodeGenerator();
                if (codeGen) {
                    // フォールバック用の汎用エントリを作成
                    ICEntry* fallbackEntry = new ICEntry();
                    fallbackEntry->SetGeneric(true);
                    
                    // 汎用コードの生成
                    void* fallbackCode = codeGen->GenerateGenericFallbackCode(cache);
                    if (fallbackCode) {
                        fallbackEntry->SetCode(fallbackCode);
                        fallbackEntry->SetPriority(ICEntryPriority::Low); // 低優先度
                        
                        // キャッシュに追加
                        if (cache->AddEntry(fallbackEntry)) {
                            addedEntries++;
                            
                            ICLogger::Instance().Debug(
                                "汎用フォールバックICエントリを追加しました", "ICOptimizer");
                        } else {
                            delete fallbackEntry; // 追加に失敗した場合、メモリリークを防ぐ
                        }
                    } else {
                        delete fallbackEntry;
                    }
                }
            }
            
            // 追加したエントリ数を記録
            result.addedEntryCount += addedEntries;
            
            // キャッシュの再編成を実行
            if (addedEntries > 0) {
                cacheManager->ReorganizeCache(cacheId);
                
                // 拡大後の統計情報をログに出力
                std::stringstream infoSs;
                infoSs << "キャッシュ '" << cacheId << "' を拡大しました: "
                       << currentCapacity << " -> " << newCapacity
                       << "、" << addedEntries << " エントリを追加";
                ICLogger::Instance().Info(infoSs.str(), "ICOptimizer");
                
                // 最適化イベントを記録
                cacheManager->RecordOptimizationEvent(
                    cacheId, "expand", true, 
                    {{"newCapacity", std::to_string(newCapacity)},
                     {"addedEntries", std::to_string(addedEntries)}}
                );
                
                // デバッグ情報の更新
                if (options.collectDebugInfo) {
                    std::stringstream debugInfo;
                    debugInfo << "キャッシュ '" << cacheId << "' を" 
                             << currentCapacity << "から" << newCapacity 
                             << "に拡大し、" << addedEntries << "エントリを追加しました";
                    result.debugInfo.push_back(debugInfo.str());
                }
                
                // フラグを設定（操作が成功したことを示す）
        return true;
            } else {
                ICLogger::Instance().Warning(
                    "キャッシュ '" + cacheId + "' の容量を拡大しましたが、新しいエントリは追加されませんでした", 
                    "ICOptimizer");
                return false;
            }
        } else {
            // 拡大が必要ない場合
            ICLogger::Instance().Info(
                "キャッシュ '" + cacheId + "' は十分な容量があります (" + 
                std::to_string(currentEntryCount) + "/" + std::to_string(currentCapacity) + ")", 
                "ICOptimizer");
            return false;
        }
    } catch (const std::exception& e) {
        // エラーをログに記録
        std::stringstream errorSs;
        errorSs << "拡大操作中にエラーが発生しました: " << e.what();
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
        // キャッシュマネージャーの確認
        if (!cacheManager) {
            ICLogger::Instance().Error("キャッシュマネージャーがnullです", "ICOptimizer");
            return false;
        }
        
        // キャッシュを取得
        auto cache = cacheManager->GetCache(cacheId);
        if (!cache) {
            ICLogger::Instance().Error("キャッシュが見つかりません: " + cacheId, "ICOptimizer");
            return false;
        }

        // 現在のキャッシュステータス
        size_t currentCapacity = cache->GetCapacity();
        size_t currentEntryCount = cache->GetEntryCount();
        
        // キャッシュエントリを取得
        std::vector<ICEntry*> entries;
        cache->GetAllEntries(entries);
        
        // 縮小が必要かどうかを判断
        if (entries.empty() || currentEntryCount < options.contraction.minEntryCount) {
            ICLogger::Instance().Info("キャッシュ '" + cacheId + "' は縮小する必要がありません", "ICOptimizer");
            return false;
        }
        
        // キャッシュ使用率が低すぎるかチェック
        double usageRatio = static_cast<double>(currentEntryCount) / currentCapacity;
        if (usageRatio >= options.contraction.minUsageRatio) {
            ICLogger::Instance().Info(
                "キャッシュ '" + cacheId + "' の使用率が十分高いため縮小は不要です (" + 
                std::to_string(usageRatio * 100.0) + "%)", 
                "ICOptimizer");
            return false;
        }
        
        // パフォーマンスアナライザからヒット統計を取得
        ICAccessStats stats = ICPerformanceAnalyzer::Instance().GetStatsForCache(cacheId);
        
        // ヒット統計に基づいてエントリをランク付け
        struct EntryStats {
            ICEntry* entry;
            uint32_t hits;
            uint32_t misses;
            double hitRatio;
            uint64_t lastUsed;
        };
        
        std::vector<EntryStats> entryStats;
        entryStats.reserve(entries.size());
        
        // 各エントリの統計情報を収集
        for (auto* entry : entries) {
            EntryStats es;
            es.entry = entry;
            es.hits = entry->GetHitCount();
            es.misses = entry->GetMissCount();
            es.lastUsed = entry->GetLastUsedTimestamp();
            
            // ヒット率を計算（ゼロ除算を回避）
            if (es.hits + es.misses > 0) {
                es.hitRatio = static_cast<double>(es.hits) / (es.hits + es.misses);
            } else {
                es.hitRatio = 0.0;
            }
            
            entryStats.push_back(es);
        }
        
        // 縮小アルゴリズムの選択
        switch (options.contraction.algorithm) {
            case ICContractionAlgorithm::LeastRecentlyUsed: {
                // 最後に使用された時刻でソート
                std::sort(entryStats.begin(), entryStats.end(), 
                    [](const EntryStats& a, const EntryStats& b) {
                        return a.lastUsed < b.lastUsed;  // 古いものが先頭
                    });
                break;
            }
            
            case ICContractionAlgorithm::LeastFrequentlyUsed: {
                // ヒット数でソート
                std::sort(entryStats.begin(), entryStats.end(), 
                    [](const EntryStats& a, const EntryStats& b) {
                        return a.hits < b.hits;  // ヒット数が少ないものが先頭
                    });
                break;
            }
            
            case ICContractionAlgorithm::WorstHitRatio: {
                // ヒット率でソート
                std::sort(entryStats.begin(), entryStats.end(), 
                    [](const EntryStats& a, const EntryStats& b) {
                        return a.hitRatio < b.hitRatio;  // ヒット率が低いものが先頭
                    });
                break;
            }
            
            case ICContractionAlgorithm::Hybrid:
            default: {
                // ヒット率と最終使用時刻の組み合わせでスコアリング
                const double hitRatioWeight = 0.7;  // ヒット率の重み
                const double recencyWeight = 0.3;   // 最新度の重み
                
                // 最大最終使用時刻を取得
                uint64_t maxLastUsed = 0;
                for (const auto& es : entryStats) {
                    maxLastUsed = std::max(maxLastUsed, es.lastUsed);
                }
                
                // ハイブリッドスコアでソート
                std::sort(entryStats.begin(), entryStats.end(), 
                    [hitRatioWeight, recencyWeight, maxLastUsed](const EntryStats& a, const EntryStats& b) {
                        // 最新度スコア（0〜1、1が最も新しい）
                        double recencyScoreA = maxLastUsed > 0 ? 
                            static_cast<double>(a.lastUsed) / maxLastUsed : 0;
                        double recencyScoreB = maxLastUsed > 0 ? 
                            static_cast<double>(b.lastUsed) / maxLastUsed : 0;
                        
                        // 総合スコア（高いほど良い）
                        double scoreA = hitRatioWeight * a.hitRatio + recencyWeight * recencyScoreA;
                        double scoreB = hitRatioWeight * b.hitRatio + recencyWeight * recencyScoreB;
                        
                        return scoreA < scoreB; // スコアが低いものが先頭
                    });
                break;
            }
        }
        
        // 削除する候補数を計算
        size_t idealCapacity = std::max(
            static_cast<size_t>(currentEntryCount / options.contraction.targetUsageRatio),
            options.contraction.minCapacity
        );
        
        // キャパシティはべき乗に調整するか、最小値を保証
        size_t newCapacity = idealCapacity;
        if (options.contraction.usePowerOfTwo) {
            // 次の2のべき乗を見つける
            newCapacity = 1;
            while (newCapacity < idealCapacity) {
                newCapacity *= 2;
            }
        }
        
        // 削除するエントリ数を計算
        size_t entriesToRemove = 0;
        
        if (newCapacity < currentCapacity) {
            // 新しいキャパシティのターゲットが現在より小さい場合
            // まずキャッシュをリサイズする
            bool resizeSuccess = cache->Resize(newCapacity);
            
            if (!resizeSuccess) {
                ICLogger::Instance().Warning(
                    "キャッシュ '" + cacheId + "' のリサイズに失敗しました", 
                    "ICOptimizer");
            } else {
                std::stringstream infoSs;
                infoSs << "キャッシュ '" << cacheId << "' のキャパシティを縮小しました: " 
                       << currentCapacity << " -> " << newCapacity;
                ICLogger::Instance().Info(infoSs.str(), "ICOptimizer");
            }
            
            // 削除するエントリ数を計算
            size_t maxEntriesAfterResize = static_cast<size_t>(newCapacity * options.contraction.targetUsageRatio);
            entriesToRemove = currentEntryCount > maxEntriesAfterResize ? 
                              currentEntryCount - maxEntriesAfterResize : 0;
        } else {
            // キャパシティの縮小が不要な場合でも、使用率を調整するためにエントリを削除
            size_t targetEntryCount = static_cast<size_t>(currentCapacity * options.contraction.targetUsageRatio);
            entriesToRemove = currentEntryCount > targetEntryCount ? 
                              currentEntryCount - targetEntryCount : 0;
        }
        
        // 維持すべき最小エントリ数をチェック
        entriesToRemove = std::min(
            entriesToRemove, 
            currentEntryCount > options.contraction.minEntryCount ? 
                currentEntryCount - options.contraction.minEntryCount : 0
        );
        
        // エントリを削除（最も価値の低いものから）
        size_t removedCount = 0;
        for (size_t i = 0; i < entryStats.size() && removedCount < entriesToRemove; i++) {
            auto* entry = entryStats[i].entry;
            
            // 汎用フォールバックエントリは保持する
            if (entry->IsGeneric() && options.contraction.preserveGenericFallback) {
                continue;
            }
            
            // 必須エントリは削除しない（例: グローバルオブジェクト用など）
            if (entry->IsCritical() && options.contraction.preserveCriticalEntries) {
                continue;
            }
            
            // エントリを削除
            if (cache->RemoveEntry(entry)) {
                removedCount++;
                
                // デバッグ情報
                std::stringstream debugSs;
                debugSs << "キャッシュ '" << cacheId << "' からエントリを削除しました "
                       << "(ヒット率: " << std::fixed << std::setprecision(2) << (entryStats[i].hitRatio * 100.0) 
                       << "%, ヒット数: " << entryStats[i].hits << ")";
                ICLogger::Instance().Debug(debugSs.str(), "ICOptimizer");
            }
        }
        
        // 操作結果を更新
        result.deletedEntryCount += removedCount;
        
        // キャッシュ再編成
        if (removedCount > 0) {
            cacheManager->ReorganizeCache(cacheId);
            
            // イベント記録
            cacheManager->RecordOptimizationEvent(
                cacheId, "contract", true,
                {{"removedEntries", std::to_string(removedCount)},
                 {"newCapacity", std::to_string(cache->GetCapacity())}}
            );
            
            // 詳細情報をログに出力
            std::stringstream infoSs;
            infoSs << "キャッシュ '" << cacheId << "' を縮小しました: "
                   << removedCount << " エントリを削除し、キャパシティは "
                   << currentCapacity << " -> " << cache->GetCapacity() << " になりました";
            ICLogger::Instance().Info(infoSs.str(), "ICOptimizer");
            
            // デバッグ情報
            if (options.collectDebugInfo) {
                std::stringstream debugInfo;
                debugInfo << "キャッシュ '" << cacheId << "' を縮小しました: "
                         << removedCount << " エントリを削除、キャパシティ " 
                         << currentCapacity << " -> " << cache->GetCapacity();
                result.debugInfo.push_back(debugInfo.str());
            }
            
        return true;
        } else {
            ICLogger::Instance().Info(
                "キャッシュ '" + cacheId + "' からのエントリ削除はできませんでした", 
                "ICOptimizer");
                
            return false;
        }
    } catch (const std::exception& e) {
        // エラーをログに記録
        std::stringstream errorSs;
        errorSs << "縮小操作中にエラーが発生しました: " << e.what();
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
    std::stringstream ss;
    ss << "Performing merge operation on " << cacheIds.size() << " caches";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    try {
        if (!cacheManager) return false;
        size_t deleted = 0, added = 0;
        for (const auto& id : cacheIds) {
            auto cache = cacheManager->GetCache(id);
            if (cache) {
                deleted += cache->RemoveRedundantEntries();
            }
        }
        added = cacheManager->MergeCaches(cacheIds);
        result.deletedEntryCount += deleted;
        result.addedEntryCount += added;
        return (deleted > 0 || added > 0);
    } catch (const std::exception& e) {
        std::stringstream errorSs;
        errorSs << "Error during merge operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
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
    std::stringstream ss;
    ss << "Performing split operation on cache '" << cacheId << "'";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    try {
        if (!cacheManager) return false;
        auto cache = cacheManager->GetCache(cacheId);
        if (!cache) return false;
        size_t deleted = cache->RemovePolymorphicEntries();
        size_t added = cacheManager->SplitCache(cacheId);
        result.deletedEntryCount += deleted;
        result.addedEntryCount += added;
        return (deleted > 0 || added > 0);
    } catch (const std::exception& e) {
        std::stringstream errorSs;
        errorSs << "Error during split operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
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
    std::stringstream ss;
    ss << "Performing reorganize operation on cache '" << cacheId << "'";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    try {
        if (!cacheManager) return false;
        auto cache = cacheManager->GetCache(cacheId);
        if (!cache) return false;
        size_t deleted = cache->ReorderEntries();
        size_t added = cache->AddOptimizedEntry();
        result.deletedEntryCount += deleted;
        result.addedEntryCount += added;
        return (deleted > 0 || added > 0);
    } catch (const std::exception& e) {
        std::stringstream errorSs;
        errorSs << "Error during reorganize operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
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
    std::stringstream ss;
    ss << "Performing prune operation on cache '" << cacheId << "'";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    try {
        if (!cacheManager) return false;
        auto cache = cacheManager->GetCache(cacheId);
        if (!cache) return false;
        size_t deleted = cache->PruneUnstableEntries();
        result.deletedEntryCount += deleted;
        return (deleted > 0);
    } catch (const std::exception& e) {
        std::stringstream errorSs;
        errorSs << "Error during prune operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
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
    std::stringstream ss;
    ss << "Performing preload operation on " << cacheIds.size() << " caches";
    ICLogger::Instance().Debug(ss.str(), "ICOptimizer");
    try {
        if (!cacheManager) return false;
        for (const auto& cacheId : cacheIds) {
            auto cache = cacheManager->GetCache(cacheId);
            if (!cache) continue;
            // 予測されるキー（例: 0〜N）を事前に追加
            for (uint64_t k = 0; k < options.preload.numPreloadKeys; ++k) {
                cache->Add(k, 0, 0);
                result.addedEntryCount++;
            }
        }
        return result.addedEntryCount > 0;
    } catch (const std::exception& e) {
        std::stringstream errorSs;
        errorSs << "Error during preload operation: " << e.what();
        ICLogger::Instance().Error(errorSs.str(), "ICOptimizer");
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

//==============================================================================
// 複雑なアクセスパターン検出
//==============================================================================

void ICOptimizer::AnalyzeAccessPatterns(const std::vector<ICAccess>& accesses) {
    std::unordered_map<std::string, int> histogram;
    for (const auto& access : accesses) {
        histogram[access.pattern]++;
    }
    for (const auto& pair : histogram) {
        if (pair.second > kPatternOptimizeThreshold) {
            OptimizePattern(pair.first);
        }
    }
}

ICType ICOptimizer::GetCacheType(const std::string& cacheId) const {
    return m_cacheManager->getTypeForCache(cacheId);
}

// 高度な特化処理の実装
void ICOptimizer::performAdvancedSpecialization(ICEntry& entry, ICType type,
                                               const ICOptimizationOptions& options,
                                               InlineCacheManager* cacheManager,
                                               ICOptimizationResult& result) {
    // 基本的な特化フラグの設定
    entry.flags |= ICEntryFlags::Specialized;
    
    // IC種別に応じた特化処理
    switch (type) {
        case ICType::Property: {
            // プロパティアクセスの特化
            if (entry.shapeInfo.isMonomorphic()) {
                // モノモーフィックな場合は直接オフセットアクセスに特化
                entry.specialized.propertyOffset = entry.shapeInfo.getPropertyOffset();
                entry.specialized.directAccess = true;
                
                // 境界チェックの除去
                if (options.eliminateBoundsChecks) {
                    entry.flags |= ICEntryFlags::BoundsCheckEliminated;
                }
                
                // 型チェックの最適化
                if (options.optimizeTypeChecks) {
                    entry.specialized.typeCheckCode = generateOptimizedTypeCheck(entry.shapeInfo);
                }
            } else if (entry.shapeInfo.isPolymorphic() && entry.shapeInfo.getShapeCount() <= 4) {
                // ポリモーフィック（4つまで）の場合はディスパッチテーブル特化
                entry.specialized.dispatchTable = generateDispatchTable(entry.shapeInfo.getShapes());
                entry.specialized.polymorphicOptimized = true;
            }
            break;
        }
        
        case ICType::Method: {
            // メソッド呼び出しの特化
            if (entry.callInfo.isDirectCall()) {
                // 直接呼び出し可能な場合
                entry.specialized.directCallTarget = entry.callInfo.getTargetFunction();
                entry.specialized.inlineCandidate = 
                    (entry.callInfo.getTargetSize() <= options.inlineThreshold);
                
                // インライン展開の実行
                if (entry.specialized.inlineCandidate && options.enableInlining) {
                    entry.specialized.inlinedCode = inlineFunction(entry.callInfo.getTargetFunction());
                    entry.flags |= ICEntryFlags::Inlined;
                }
            } else if (entry.callInfo.isVirtualCall()) {
                // 仮想呼び出しの場合は仮想テーブル最適化
                entry.specialized.vtableOffset = entry.callInfo.getVTableOffset();
                entry.specialized.vtableOptimized = true;
            }
            break;
        }
        
        case ICType::Constructor: {
            // コンストラクタ呼び出しの特化
            if (entry.constructorInfo.hasFixedShape()) {
                // 固定シェイプのオブジェクト作成
                entry.specialized.preAllocatedShape = entry.constructorInfo.getShape();
                entry.specialized.fastConstruction = true;
                
                // プロパティの事前初期化
                if (options.preInitializeProperties) {
                    entry.specialized.propertyInitCode = 
                        generatePropertyInitCode(entry.constructorInfo.getInitialProperties());
                }
            }
            break;
        }
        
        case ICType::BinaryOperation: {
            // 二項演算の特化
            auto& operandTypes = entry.operandTypes;
            if (operandTypes.size() >= 2) {
                auto lhsType = operandTypes[0];
                auto rhsType = operandTypes[1];
                
                // 数値演算の特化
                if (lhsType.isNumeric() && rhsType.isNumeric()) {
                    entry.specialized.numericOperation = true;
                    entry.specialized.operationType = determineNumericOperationType(lhsType, rhsType);
                    
                    // 整数オーバーフローチェックの最適化
                    if (options.optimizeIntegerOverflow) {
                        entry.specialized.overflowChecks = 
                            generateOptimizedOverflowChecks(entry.operation, lhsType, rhsType);
                    }
                }
                
                // 文字列連結の特化
                else if (lhsType.isString() && rhsType.isString()) {
                    entry.specialized.stringConcatenation = true;
                    entry.specialized.stringOptimizations = 
                        generateStringConcatenationOptimizations(options);
                }
            }
            break;
        }
        
        case ICType::Comparison: {
            // 比較演算の特化
            auto& operandTypes = entry.operandTypes;
            if (operandTypes.size() >= 2) {
                auto lhsType = operandTypes[0];
                auto rhsType = operandTypes[1];
                
                // 数値比較の特化
                if (lhsType.isNumeric() && rhsType.isNumeric()) {
                    entry.specialized.numericComparison = true;
                    entry.specialized.comparisonType = determineComparisonType(lhsType, rhsType);
                }
                
                // 文字列比較の特化
                else if (lhsType.isString() && rhsType.isString()) {
                    entry.specialized.stringComparison = true;
                    entry.specialized.stringComparisonOptimizations = 
                        generateStringComparisonOptimizations(options);
                }
                
                // オブジェクト参照比較の特化
                else if (lhsType.isObject() && rhsType.isObject()) {
                    entry.specialized.referenceComparison = true;
                }
            }
            break;
        }
        
        default:
            // その他のIC種別は基本的な特化のみ実行
            break;
    }
    
    // 共通の最適化処理
    
    // ガード条件の最適化
    if (options.optimizeGuards) {
        entry.specialized.optimizedGuards = generateOptimizedGuards(entry, type);
    }
    
    // プロファイルフィードバックベースの最適化
    if (options.useProfileFeedback && entry.profileData.isAvailable()) {
        applyProfileBasedOptimizations(entry, options);
    }
    
    // マシンコード特化の実行
    if (options.generateSpecializedCode) {
        entry.specialized.nativeCode = generateSpecializedNativeCode(entry, type, options);
        entry.specialized.nativeCodeSize = entry.specialized.nativeCode.size();
    }
    
    // 特化後の検証
    if (options.validateSpecialization) {
        bool isValid = validateSpecializedEntry(entry, type);
        if (!isValid) {
            // 特化に失敗した場合は元に戻す
            entry.flags &= ~ICEntryFlags::Specialized;
            result.specializedEntryCount--;
            return;
        }
    }
    
    // 統計情報の更新
    entry.specialized.creationTime = std::chrono::steady_clock::now();
    entry.specialized.hitCountAtSpecialization = entry.hitCount;
    
    // 結果に詳細情報を追加
    if (options.collectDebugInfo) {
        std::stringstream debugInfo;
        debugInfo << "エントリ " << entry.id << " を " << ICTypeToString(type) 
                  << " 用に特化しました (ヒット数: " << entry.hitCount << ")";
        result.debugInfo.push_back(debugInfo.str());
    }
}

} // namespace core
} // namespace aerojs 