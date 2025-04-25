#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>
#include <chrono>
#include <functional>
#include <atomic>

#include "ic_logger.h"
#include "ic_performance_analyzer.h"

namespace aerojs {
namespace core {

// 前方宣言
class InlineCacheManager;

/**
 * @brief オプティマイズ戦略の種類
 */
enum class ICOptimizationStrategy {
    None,               // 最適化なし
    FrequencyBased,     // 使用頻度に基づく最適化
    PatternBased,       // アクセスパターンに基づく最適化
    ProfileBased,       // プロファイリングデータに基づく最適化
    HeuristicBased,     // ヒューリスティックに基づく最適化
    AdaptiveBased,      // 実行時状況に適応する最適化
    Custom              // カスタム最適化
};

/**
 * @brief キャッシュエントリの優先度レベル
 */
enum class ICPriorityLevel {
    Critical,           // クリティカルパス上の重要なエントリ
    High,               // 高優先度（頻繁に使用される）
    Medium,             // 中優先度
    Low,                // 低優先度（まれに使用される）
    Background          // バックグラウンド（ほとんど使用されない）
};

/**
 * @brief キャッシュエントリの最適化状態
 */
enum class ICOptimizationState {
    NotOptimized,       // 最適化されていない
    Optimizing,         // 最適化中
    PartiallyOptimized, // 部分的に最適化済み
    FullyOptimized,     // 完全に最適化済み
    Failed              // 最適化に失敗
};

/**
 * @brief キャッシュの最適化範囲
 */
enum class ICOptimizationScope {
    Global,             // グローバル範囲の最適化
    Function,           // 関数スコープの最適化
    Block,              // ブロックスコープの最適化
    Property,           // 特定のプロパティに限定した最適化
    Method,             // 特定のメソッドに限定した最適化
    Shape               // 特定のオブジェクト形状に限定した最適化
};

/**
 * @brief 最適化操作の種類
 */
enum class ICOptimizationOperation {
    Specialize,         // 特定の型またはシェイプに特化
    Generalize,         // より一般的なキャッシュに変換
    Expand,             // キャッシュサイズを拡大
    Contract,           // キャッシュサイズを縮小
    Merge,              // 複数のキャッシュをマージ
    Split,              // 1つのキャッシュを複数に分割
    Reorganize,         // エントリを再編成
    Prune,              // 不要なエントリを削除
    Preload,            // よく使われるエントリを先読み
    Custom              // カスタム操作
};

/**
 * @brief 最適化の閾値設定
 */
struct ICOptimizationThresholds {
    double minHitRate;                  // 最小ヒット率
    double maxTypeErrorRate;            // 最大型エラー率
    double maxInvalidationRate;         // 最大無効化率
    uint64_t minAccessCount;            // 最小アクセス数
    uint64_t maxMemoryUsage;            // 最大メモリ使用量（バイト）
    std::chrono::milliseconds maxOptimizationTime; // 最大最適化時間
    
    // デフォルトコンストラクタ
    ICOptimizationThresholds()
        : minHitRate(0.8)
        , maxTypeErrorRate(0.05)
        , maxInvalidationRate(0.1)
        , minAccessCount(100)
        , maxMemoryUsage(1024 * 1024) // 1MB
        , maxOptimizationTime(std::chrono::milliseconds(100))
    {
    }
};

/**
 * @brief 最適化オプションの設定
 */
struct ICOptimizationOptions {
    ICOptimizationStrategy strategy;    // 最適化戦略
    ICOptimizationScope scope;          // 最適化範囲
    ICOptimizationThresholds thresholds; // 閾値設定
    bool enableAggressiveOptimization;  // 積極的な最適化の有効化
    bool enableFallbackOptions;         // フォールバックオプションの有効化
    bool enableLearning;                // 学習機能の有効化
    bool enableBackgroundOptimization;  // バックグラウンド最適化の有効化
    bool enableMemoryConstraints;       // メモリ制約の有効化
    
    // デフォルトコンストラクタ
    ICOptimizationOptions()
        : strategy(ICOptimizationStrategy::FrequencyBased)
        , scope(ICOptimizationScope::Global)
        , enableAggressiveOptimization(false)
        , enableFallbackOptions(true)
        , enableLearning(true)
        , enableBackgroundOptimization(true)
        , enableMemoryConstraints(true)
    {
    }
};

/**
 * @brief 最適化の結果
 */
struct ICOptimizationResult {
    bool success;                       // 成功したかどうか
    std::string optimizationId;         // 最適化ID
    ICOptimizationState state;          // 最適化状態
    uint32_t optimizationCount;         // 最適化実行回数
    uint32_t modifiedCacheCount;        // 変更されたキャッシュ数
    uint32_t deletedEntryCount;         // 削除されたエントリ数
    uint32_t addedEntryCount;           // 追加されたエントリ数
    uint32_t specializedEntryCount;     // 特化されたエントリ数
    double hitRateBeforeOptimization;   // 最適化前のヒット率
    double hitRateAfterOptimization;    // 最適化後のヒット率
    double performanceImprovement;      // パフォーマンス改善率
    std::chrono::milliseconds optimizationTime; // 最適化にかかった時間
    std::string errorMessage;           // エラーメッセージ（失敗時）
    
    // デフォルトコンストラクタ
    ICOptimizationResult()
        : success(false)
        , state(ICOptimizationState::NotOptimized)
        , optimizationCount(0)
        , modifiedCacheCount(0)
        , deletedEntryCount(0)
        , addedEntryCount(0)
        , specializedEntryCount(0)
        , hitRateBeforeOptimization(0.0)
        , hitRateAfterOptimization(0.0)
        , performanceImprovement(0.0)
        , optimizationTime(std::chrono::milliseconds(0))
    {
    }
};

/**
 * @brief カスタム最適化ハンドラの定義
 */
using ICCustomOptimizationHandler = std::function<ICOptimizationResult(
    const std::string& cacheId,
    ICType type,
    const ICOptimizationOptions& options,
    InlineCacheManager* cacheManager)>;

/**
 * @brief インラインキャッシュオプティマイザークラス
 * 
 * このクラスは、インラインキャッシュのパフォーマンスを向上させるために
 * キャッシュエントリの最適化を行います。
 */
class ICOptimizer {
public:
    /**
     * @brief シングルトンインスタンスの取得
     * @return ICOptimizerのシングルトンインスタンス
     */
    static ICOptimizer& Instance();
    
    /**
     * @brief デストラクタ
     */
    ~ICOptimizer();
    
    /**
     * @brief 指定したキャッシュIDのキャッシュを最適化する
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 最適化結果
     */
    ICOptimizationResult OptimizeCache(
        const std::string& cacheId,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief 指定したタイプのすべてのキャッシュを最適化する
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 最適化結果のマップ（キャッシュID -> 結果）
     */
    std::unordered_map<std::string, ICOptimizationResult> OptimizeCachesByType(
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief すべてのキャッシュを最適化する
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 最適化結果のマップ（キャッシュID -> 結果）
     */
    std::unordered_map<std::string, ICOptimizationResult> OptimizeAllCaches(
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief 指定したキャッシュIDの優先度を設定する
     * @param cacheId キャッシュID
     * @param priority 優先度レベル
     */
    void SetCachePriority(const std::string& cacheId, ICPriorityLevel priority);
    
    /**
     * @brief 指定したキャッシュIDの優先度を取得する
     * @param cacheId キャッシュID
     * @return 優先度レベル
     */
    ICPriorityLevel GetCachePriority(const std::string& cacheId) const;
    
    /**
     * @brief 特定のキャッシュの最適化が必要かどうかを判断する
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param thresholds 閾値設定
     * @return 最適化が必要な場合はtrue
     */
    bool NeedsOptimization(
        const std::string& cacheId,
        ICType type,
        const ICOptimizationThresholds& thresholds) const;
    
    /**
     * @brief 指定したキャッシュIDの最適化履歴を取得する
     * @param cacheId キャッシュID
     * @return 最適化結果の履歴
     */
    std::vector<ICOptimizationResult> GetOptimizationHistory(const std::string& cacheId) const;
    
    /**
     * @brief バックグラウンド最適化を開始する
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @param intervalMs 最適化間隔（ミリ秒）
     */
    void StartBackgroundOptimization(
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager,
        uint64_t intervalMs = 5000);
    
    /**
     * @brief バックグラウンド最適化を停止する
     */
    void StopBackgroundOptimization();
    
    /**
     * @brief カスタム最適化ハンドラを登録する
     * @param type キャッシュタイプ
     * @param handler カスタム最適化ハンドラ
     */
    void RegisterCustomOptimizationHandler(
        ICType type,
        ICCustomOptimizationHandler handler);
    
    /**
     * @brief 最適化レポートを生成する
     * @param detailed 詳細レポートを生成するかどうか
     * @return 最適化レポート
     */
    std::string GenerateOptimizationReport(bool detailed = false) const;
    
    /**
     * @brief JSON形式の最適化レポートを生成する
     * @return JSON形式の最適化レポート
     */
    std::string GenerateJsonOptimizationReport() const;

private:
    /**
     * @brief コンストラクタ（シングルトンパターン）
     */
    ICOptimizer();
    
    // コピー禁止
    ICOptimizer(const ICOptimizer&) = delete;
    ICOptimizer& operator=(const ICOptimizer&) = delete;
    
    /**
     * @brief 使用頻度に基づく最適化
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 最適化結果
     */
    ICOptimizationResult OptimizeByFrequency(
        const std::string& cacheId,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief アクセスパターンに基づく最適化
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 最適化結果
     */
    ICOptimizationResult OptimizeByPattern(
        const std::string& cacheId,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief プロファイリングデータに基づく最適化
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 最適化結果
     */
    ICOptimizationResult OptimizeByProfile(
        const std::string& cacheId,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief ヒューリスティックに基づく最適化
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 最適化結果
     */
    ICOptimizationResult OptimizeByHeuristic(
        const std::string& cacheId,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief 適応型最適化
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @return 最適化結果
     */
    ICOptimizationResult OptimizeByAdaptive(
        const std::string& cacheId,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager);
    
    /**
     * @brief バックグラウンド最適化ワーカー
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @param intervalMs 最適化間隔（ミリ秒）
     */
    void BackgroundOptimizationWorker(
        ICOptimizationOptions options,
        InlineCacheManager* cacheManager,
        uint64_t intervalMs);
    
    /**
     * @brief 最適化結果を履歴に追加する
     * @param cacheId キャッシュID
     * @param result 最適化結果
     */
    void AddToOptimizationHistory(
        const std::string& cacheId,
        const ICOptimizationResult& result);
    
    /**
     * @brief 特殊化の最適化操作
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @param result 最適化結果の引数
     * @return 操作が成功した場合はtrue
     */
    bool PerformSpecializeOperation(
        const std::string& cacheId,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager,
        ICOptimizationResult& result);
    
    /**
     * @brief 一般化の最適化操作
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @param result 最適化結果の引数
     * @return 操作が成功した場合はtrue
     */
    bool PerformGeneralizeOperation(
        const std::string& cacheId,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager,
        ICOptimizationResult& result);
    
    /**
     * @brief 拡大の最適化操作
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @param result 最適化結果の引数
     * @return 操作が成功した場合はtrue
     */
    bool PerformExpandOperation(
        const std::string& cacheId,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager,
        ICOptimizationResult& result);
    
    /**
     * @brief 縮小の最適化操作
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @param result 最適化結果の引数
     * @return 操作が成功した場合はtrue
     */
    bool PerformContractOperation(
        const std::string& cacheId,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager,
        ICOptimizationResult& result);
    
    /**
     * @brief マージの最適化操作
     * @param cacheIds マージ対象のキャッシュIDリスト
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @param result 最適化結果の引数
     * @return 操作が成功した場合はtrue
     */
    bool PerformMergeOperation(
        const std::vector<std::string>& cacheIds,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager,
        ICOptimizationResult& result);
    
    /**
     * @brief 分割の最適化操作
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @param result 最適化結果の引数
     * @return 操作が成功した場合はtrue
     */
    bool PerformSplitOperation(
        const std::string& cacheId,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager,
        ICOptimizationResult& result);
    
    /**
     * @brief 整理の最適化操作
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @param result 最適化結果の引数
     * @return 操作が成功した場合はtrue
     */
    bool PerformReorganizeOperation(
        const std::string& cacheId,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager,
        ICOptimizationResult& result);
    
    /**
     * @brief 刈り込みの最適化操作
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @param result 最適化結果の引数
     * @return 操作が成功した場合はtrue
     */
    bool PerformPruneOperation(
        const std::string& cacheId,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager,
        ICOptimizationResult& result);
    
    /**
     * @brief 先読みの最適化操作
     * @param cacheIds 先読み対象のキャッシュIDリスト
     * @param type キャッシュタイプ
     * @param options 最適化オプション
     * @param cacheManager キャッシュマネージャーへのポインタ
     * @param result 最適化結果の引数
     * @return 操作が成功した場合はtrue
     */
    bool PerformPreloadOperation(
        const std::vector<std::string>& cacheIds,
        ICType type,
        const ICOptimizationOptions& options,
        InlineCacheManager* cacheManager,
        ICOptimizationResult& result);
    
    // メンバ変数
    std::unordered_map<std::string, ICPriorityLevel> m_cachePriorities;  // キャッシュIDごとの優先度
    std::unordered_map<std::string, std::vector<ICOptimizationResult>> m_optimizationHistory;  // 最適化履歴
    std::unordered_map<ICType, ICCustomOptimizationHandler> m_customHandlers;  // カスタムハンドラ
    
    std::thread m_backgroundThread;  // バックグラウンド最適化スレッド
    std::atomic<bool> m_backgroundRunning;  // バックグラウンド最適化実行中フラグ
    std::atomic<uint64_t> m_totalOptimizationCount;  // 合計最適化実行回数
    
    mutable std::mutex m_priorityMutex;  // 優先度マップのミューテックス
    mutable std::mutex m_historyMutex;   // 履歴マップのミューテックス
    mutable std::mutex m_handlersMutex;  // ハンドラマップのミューテックス
};

} // namespace core
} // namespace aerojs 