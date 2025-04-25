#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>

namespace aerojs {
namespace core {

/**
 * @brief プロファイル対象の種類を表す列挙型
 */
enum class ProfileTargetType {
    kFunction,      // 関数
    kLoop,          // ループ
    kBranch,        // 分岐
    kCall,          // 関数呼び出し
    kBytecode,      // バイトコード命令
    kBlock          // コードブロック
};

/**
 * @brief 最適化フェーズの種類を表す列挙型
 */
enum class OptimizationPhase {
    kNone,               // 最適化なし
    kBaselineJIT,        // ベースラインJIT
    kInlineCaching,      // インラインキャッシュ
    kTypeSpecialization, // 型特化
    kInlining,           // インライン化
    kLoopOptimization,   // ループ最適化
    kDeadCodeElimination // デッドコード除去
};

/**
 * @brief プロファイル情報を格納する構造体
 */
struct ProfileInfo {
    uint64_t executionCount;           // 実行回数
    uint64_t totalExecutionTimeNs;     // 合計実行時間 (ナノ秒)
    uint64_t maxExecutionTimeNs;       // 最大実行時間 (ナノ秒)
    uint64_t minExecutionTimeNs;       // 最小実行時間 (ナノ秒)
    OptimizationPhase optimizationPhase; // 適用された最適化フェーズ
    std::chrono::system_clock::time_point lastExecutionTime; // 最後の実行時間
    
    // コンストラクタ
    ProfileInfo()
        : executionCount(0)
        , totalExecutionTimeNs(0)
        , maxExecutionTimeNs(0)
        , minExecutionTimeNs(UINT64_MAX)
        , optimizationPhase(OptimizationPhase::kNone)
        , lastExecutionTime(std::chrono::system_clock::now())
    {
    }
};

/**
 * @brief 実行トレース情報を格納する構造体
 */
struct TraceEntry {
    std::string targetId;              // 対象ID
    ProfileTargetType targetType;      // 対象タイプ
    uint64_t startTimeNs;              // 開始時間 (ナノ秒)
    uint64_t endTimeNs;                // 終了時間 (ナノ秒)
    uint64_t executionTimeNs;          // 実行時間 (ナノ秒)
    
    // コンストラクタ
    TraceEntry(const std::string& id, ProfileTargetType type, uint64_t start)
        : targetId(id)
        , targetType(type)
        , startTimeNs(start)
        , endTimeNs(0)
        , executionTimeNs(0)
    {
    }
};

/**
 * @brief 最適化判断のための閾値設定
 */
struct OptimizationThresholds {
    uint64_t executionCountThreshold;      // 実行回数の閾値
    uint64_t executionTimeThresholdNs;     // 実行時間の閾値 (ナノ秒)
    uint64_t hotLoopThreshold;             // ホットループの回数閾値
    double timePercentageThreshold;        // 全体実行時間に対する割合の閾値
    
    // デフォルトコンストラクタ
    OptimizationThresholds()
        : executionCountThreshold(1000)
        , executionTimeThresholdNs(1000000) // 1ms
        , hotLoopThreshold(100)
        , timePercentageThreshold(0.05) // 5%
    {
    }
};

/**
 * @brief コードプロファイラクラス
 * 
 * JITコンパイラの最適化のためのプロファイリング情報を収集・管理するクラス
 */
class CodeProfiler {
public:
    /**
     * @brief シングルトンインスタンスを取得
     * @return CodeProfilerのインスタンス
     */
    static CodeProfiler& Instance();
    
    /**
     * @brief デストラクタ
     */
    ~CodeProfiler();
    
    /**
     * @brief プロファイリングの開始
     * @param targetId 対象ID
     * @param targetType 対象タイプ
     * @return 生成されたトレースエントリのID
     */
    uint64_t StartProfiling(const std::string& targetId, ProfileTargetType targetType);
    
    /**
     * @brief プロファイリングの終了
     * @param traceId トレースエントリのID
     */
    void EndProfiling(uint64_t traceId);
    
    /**
     * @brief プロファイリング情報を記録
     * @param targetId 対象ID
     * @param targetType 対象タイプ
     * @param executionTimeNs 実行時間 (ナノ秒)
     */
    void RecordExecution(const std::string& targetId, ProfileTargetType targetType, uint64_t executionTimeNs);
    
    /**
     * @brief 対象のプロファイル情報を取得
     * @param targetId 対象ID
     * @return プロファイル情報
     */
    ProfileInfo GetProfileInfo(const std::string& targetId) const;
    
    /**
     * @brief 全てのプロファイル情報を取得
     * @return 対象IDとプロファイル情報のマップ
     */
    std::unordered_map<std::string, ProfileInfo> GetAllProfileInfo() const;
    
    /**
     * @brief 実行回数の多い順に対象を取得
     * @param limit 取得する対象の数
     * @return 対象IDとプロファイル情報のペアのベクトル
     */
    std::vector<std::pair<std::string, ProfileInfo>> GetHotTargets(size_t limit = 10) const;
    
    /**
     * @brief 実行時間の長い順に対象を取得
     * @param limit 取得する対象の数
     * @return 対象IDとプロファイル情報のペアのベクトル
     */
    std::vector<std::pair<std::string, ProfileInfo>> GetSlowTargets(size_t limit = 10) const;
    
    /**
     * @brief 最適化候補を取得
     * @param thresholds 最適化閾値
     * @return 最適化候補の対象IDとプロファイル情報のペアのベクトル
     */
    std::vector<std::pair<std::string, ProfileInfo>> GetOptimizationCandidates(
        const OptimizationThresholds& thresholds = OptimizationThresholds()) const;
    
    /**
     * @brief 特定タイプの対象のプロファイル情報を取得
     * @param targetType 対象タイプ
     * @return 対象IDとプロファイル情報のマップ
     */
    std::unordered_map<std::string, ProfileInfo> GetProfileInfoByType(ProfileTargetType targetType) const;
    
    /**
     * @brief 対象の最適化フェーズを設定
     * @param targetId 対象ID
     * @param phase 最適化フェーズ
     */
    void SetOptimizationPhase(const std::string& targetId, OptimizationPhase phase);
    
    /**
     * @brief 統計情報をリセット
     */
    void ResetStats();
    
    /**
     * @brief プロファイリングが有効かどうかを取得
     * @return 有効な場合はtrue
     */
    bool IsEnabled() const;
    
    /**
     * @brief プロファイリングを有効または無効にする
     * @param enabled 有効にする場合はtrue
     */
    void SetEnabled(bool enabled);
    
    /**
     * @brief トレース記録が有効かどうかを取得
     * @return 有効な場合はtrue
     */
    bool IsTraceEnabled() const;
    
    /**
     * @brief トレース記録を有効または無効にする
     * @param enabled 有効にする場合はtrue
     */
    void SetTraceEnabled(bool enabled);
    
    /**
     * @brief 最大トレースエントリ数を設定
     * @param maxEntries 最大エントリ数
     */
    void SetMaxTraceEntries(size_t maxEntries);
    
    /**
     * @brief トレース履歴を取得
     * @param limit 取得するエントリの数（0の場合は全て）
     * @return トレースエントリのベクトル
     */
    std::vector<TraceEntry> GetTraceHistory(size_t limit = 0) const;
    
    /**
     * @brief 特定対象のトレース履歴を取得
     * @param targetId 対象ID
     * @param limit 取得するエントリの数（0の場合は全て）
     * @return トレースエントリのベクトル
     */
    std::vector<TraceEntry> GetTraceHistoryForTarget(const std::string& targetId, size_t limit = 0) const;
    
    /**
     * @brief プロファイリングレポートを生成
     * @param detailed 詳細レポートを生成する場合はtrue
     * @return レポート文字列
     */
    std::string GenerateReport(bool detailed = false) const;
    
    /**
     * @brief JSONプロファイリングレポートを生成
     * @return JSON形式のレポート文字列
     */
    std::string GenerateJsonReport() const;

private:
    /**
     * @brief コンストラクタ（シングルトンパターン）
     */
    CodeProfiler();
    
    // コピー禁止
    CodeProfiler(const CodeProfiler&) = delete;
    CodeProfiler& operator=(const CodeProfiler&) = delete;
    
    // 実行中のトレースを完了
    void CompleteTrace(TraceEntry& trace, uint64_t endTimeNs);
    
    // トレースバッファの管理
    void ManageTraceBuffer();
    
    // プロファイル情報のマップ
    std::unordered_map<std::string, ProfileInfo> m_profileInfo;
    
    // 実行中のトレースのマップ
    std::unordered_map<uint64_t, TraceEntry> m_activeTraces;
    
    // トレース履歴
    std::vector<TraceEntry> m_traceHistory;
    
    // 次のトレースID
    std::atomic<uint64_t> m_nextTraceId;
    
    // プロファイリングが有効かどうか
    std::atomic<bool> m_enabled;
    
    // トレース記録が有効かどうか
    std::atomic<bool> m_traceEnabled;
    
    // 最大トレースエントリ数
    size_t m_maxTraceEntries;
    
    // 合計プロファイリング時間
    std::atomic<uint64_t> m_totalProfilingTimeNs;
    
    // ミューテックス
    mutable std::mutex m_profileMutex;
    mutable std::mutex m_traceMutex;
    
    // 開始時間の記録
    std::chrono::steady_clock::time_point m_startTime;
};

/**
 * @brief プロファイルターゲットタイプを文字列に変換
 * @param type 変換するProfileTargetType
 * @return タイプの文字列表現
 */
std::string ProfileTargetTypeToString(ProfileTargetType type);

/**
 * @brief 最適化フェーズを文字列に変換
 * @param phase 変換するOptimizationPhase
 * @return フェーズの文字列表現
 */
std::string OptimizationPhaseToString(OptimizationPhase phase);

/**
 * @brief スコープベースのプロファイリングヘルパークラス
 */
class ScopedProfiler {
public:
    /**
     * @brief コンストラクタ
     * @param targetId 対象ID
     * @param targetType 対象タイプ
     */
    ScopedProfiler(const std::string& targetId, ProfileTargetType targetType)
        : m_targetId(targetId)
        , m_targetType(targetType)
        , m_traceId(0)
    {
        auto& profiler = CodeProfiler::Instance();
        if (profiler.IsEnabled()) {
            m_traceId = profiler.StartProfiling(targetId, targetType);
        }
    }
    
    /**
     * @brief デストラクタ
     */
    ~ScopedProfiler() {
        auto& profiler = CodeProfiler::Instance();
        if (profiler.IsEnabled() && m_traceId > 0) {
            profiler.EndProfiling(m_traceId);
        }
    }
    
private:
    std::string m_targetId;
    ProfileTargetType m_targetType;
    uint64_t m_traceId;
};

} // namespace core
} // namespace aerojs 