#include "code_profiler.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace aerojs {
namespace core {

//=============================================================================
// CodeProfiler の実装
//=============================================================================

// シングルトンインスタンス取得
CodeProfiler& CodeProfiler::Instance() {
    static CodeProfiler instance;
    return instance;
}

CodeProfiler::CodeProfiler()
    : m_nextTraceId(1)
    , m_enabled(true)
    , m_traceEnabled(true)
    , m_maxTraceEntries(1000)  // デフォルトは最大1000エントリ
    , m_totalProfilingTimeNs(0)
    , m_startTime(std::chrono::steady_clock::now())
{
}

CodeProfiler::~CodeProfiler() = default;

uint64_t CodeProfiler::StartProfiling(const std::string& targetId, ProfileTargetType targetType) {
    if (!m_enabled) {
        return 0;
    }
    
    // 現在時刻を取得 (ナノ秒)
    auto now = std::chrono::steady_clock::now();
    auto startTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now - m_startTime).count();
    
    // 新しいトレースID生成
    uint64_t traceId = m_nextTraceId++;
    
    // トレース記録が有効の場合、アクティブトレースに追加
    if (m_traceEnabled) {
        std::lock_guard<std::mutex> lock(m_traceMutex);
        m_activeTraces.emplace(traceId, TraceEntry(targetId, targetType, startTimeNs));
    }
    
    return traceId;
}

void CodeProfiler::EndProfiling(uint64_t traceId) {
    if (!m_enabled || traceId == 0) {
        return;
    }
    
    // 現在時刻を取得 (ナノ秒)
    auto now = std::chrono::steady_clock::now();
    auto endTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now - m_startTime).count();
    
    TraceEntry trace("", ProfileTargetType::kFunction, 0);
    bool found = false;
    
    // トレース記録が有効の場合、アクティブトレースから検索
    if (m_traceEnabled) {
        std::lock_guard<std::mutex> lock(m_traceMutex);
        
        auto it = m_activeTraces.find(traceId);
        if (it != m_activeTraces.end()) {
            trace = it->second;
            found = true;
            m_activeTraces.erase(it);
        }
    }
    
    if (found) {
        // トレースを完了して統計を更新
        CompleteTrace(trace, endTimeNs);
        
        // トレース履歴にも追加
        if (m_traceEnabled) {
            std::lock_guard<std::mutex> lock(m_traceMutex);
            m_traceHistory.push_back(trace);
            ManageTraceBuffer();
        }
        
        // プロファイル情報を更新
        RecordExecution(trace.targetId, trace.targetType, trace.executionTimeNs);
    }
}

void CodeProfiler::RecordExecution(const std::string& targetId, 
                                  ProfileTargetType targetType,
                                  uint64_t executionTimeNs) {
    if (!m_enabled) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_profileMutex);
    
    // 対象のプロファイル情報を取得または作成
    auto& info = m_profileInfo[targetId];
    
    // 統計情報を更新
    info.executionCount++;
    info.totalExecutionTimeNs += executionTimeNs;
    info.maxExecutionTimeNs = std::max(info.maxExecutionTimeNs, executionTimeNs);
    info.minExecutionTimeNs = std::min(info.minExecutionTimeNs, executionTimeNs);
    info.lastExecutionTime = std::chrono::system_clock::now();
    
    // 合計プロファイリング時間を更新
    m_totalProfilingTimeNs += executionTimeNs;
}

ProfileInfo CodeProfiler::GetProfileInfo(const std::string& targetId) const {
    std::lock_guard<std::mutex> lock(m_profileMutex);
    
    auto it = m_profileInfo.find(targetId);
    if (it != m_profileInfo.end()) {
        return it->second;
    }
    
    // 見つからない場合はデフォルト値を返す
    return ProfileInfo();
}

std::unordered_map<std::string, ProfileInfo> CodeProfiler::GetAllProfileInfo() const {
    std::lock_guard<std::mutex> lock(m_profileMutex);
    return m_profileInfo;
}

std::vector<std::pair<std::string, ProfileInfo>> CodeProfiler::GetHotTargets(size_t limit) const {
    std::lock_guard<std::mutex> lock(m_profileMutex);
    
    // 実行回数が多い順にソートした結果を返す
    std::vector<std::pair<std::string, ProfileInfo>> sortedTargets(
        m_profileInfo.begin(), m_profileInfo.end());
    
    std::sort(sortedTargets.begin(), sortedTargets.end(),
        [](const auto& a, const auto& b) {
            return a.second.executionCount > b.second.executionCount;
        });
    
    // 指定された数まで切り詰め
    if (sortedTargets.size() > limit) {
        sortedTargets.resize(limit);
    }
    
    return sortedTargets;
}

std::vector<std::pair<std::string, ProfileInfo>> CodeProfiler::GetSlowTargets(size_t limit) const {
    std::lock_guard<std::mutex> lock(m_profileMutex);
    
    // 平均実行時間が長い順にソートした結果を返す
    std::vector<std::pair<std::string, ProfileInfo>> sortedTargets;
    for (const auto& pair : m_profileInfo) {
        if (pair.second.executionCount > 0) {
            sortedTargets.push_back(pair);
        }
    }
    
    std::sort(sortedTargets.begin(), sortedTargets.end(),
        [](const auto& a, const auto& b) {
            double avgA = a.second.executionCount > 0 ? 
                static_cast<double>(a.second.totalExecutionTimeNs) / a.second.executionCount : 0;
            double avgB = b.second.executionCount > 0 ? 
                static_cast<double>(b.second.totalExecutionTimeNs) / b.second.executionCount : 0;
            return avgA > avgB;
        });
    
    // 指定された数まで切り詰め
    if (sortedTargets.size() > limit) {
        sortedTargets.resize(limit);
    }
    
    return sortedTargets;
}

std::vector<std::pair<std::string, ProfileInfo>> 
CodeProfiler::GetOptimizationCandidates(const OptimizationThresholds& thresholds) const {
    std::lock_guard<std::mutex> lock(m_profileMutex);
    
    std::vector<std::pair<std::string, ProfileInfo>> candidates;
    
    for (const auto& pair : m_profileInfo) {
        const auto& info = pair.second;
        
        // 各閾値に基づいて最適化候補を選択
        bool isCandidate = false;
        
        // 実行回数の閾値を超えている
        if (info.executionCount >= thresholds.executionCountThreshold) {
            isCandidate = true;
        }
        
        // 平均実行時間の閾値を超えている
        if (info.executionCount > 0) {
            double avgTimeNs = static_cast<double>(info.totalExecutionTimeNs) / info.executionCount;
            if (avgTimeNs >= thresholds.executionTimeThresholdNs) {
                isCandidate = true;
            }
        }
        
        // 全体実行時間に対する割合の閾値を超えている
        if (m_totalProfilingTimeNs > 0) {
            double percentage = static_cast<double>(info.totalExecutionTimeNs) / m_totalProfilingTimeNs;
            if (percentage >= thresholds.timePercentageThreshold) {
                isCandidate = true;
            }
        }
        
        // 最適化済みでない対象のみを候補に追加
        if (isCandidate && info.optimizationPhase == OptimizationPhase::kNone) {
            candidates.emplace_back(pair);
        }
    }
    
    // 最適化による潜在的な効果が大きい順にソート
    std::sort(candidates.begin(), candidates.end(),
        [this](const auto& a, const auto& b) {
            double percA = static_cast<double>(a.second.totalExecutionTimeNs) / m_totalProfilingTimeNs;
            double percB = static_cast<double>(b.second.totalExecutionTimeNs) / m_totalProfilingTimeNs;
            return percA > percB;
        });
    
    return candidates;
}

std::unordered_map<std::string, ProfileInfo> 
CodeProfiler::GetProfileInfoByType(ProfileTargetType targetType) const {
    std::lock_guard<std::mutex> lock(m_profileMutex);
    std::lock_guard<std::mutex> traceLock(m_traceMutex);
    
    std::unordered_map<std::string, ProfileInfo> result;
    
    // トレース履歴から特定タイプのみ抽出
    std::unordered_set<std::string> targetIds;
    for (const auto& trace : m_traceHistory) {
        if (trace.targetType == targetType) {
            targetIds.insert(trace.targetId);
        }
    }
    
    // プロファイル情報から該当する対象のみ抽出
    for (const auto& id : targetIds) {
        auto it = m_profileInfo.find(id);
        if (it != m_profileInfo.end()) {
            result[id] = it->second;
        }
    }
    
    return result;
}

void CodeProfiler::SetOptimizationPhase(const std::string& targetId, OptimizationPhase phase) {
    if (!m_enabled) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_profileMutex);
    
    auto it = m_profileInfo.find(targetId);
    if (it != m_profileInfo.end()) {
        it->second.optimizationPhase = phase;
    }
}

void CodeProfiler::ResetStats() {
    std::lock_guard<std::mutex> profileLock(m_profileMutex);
    std::lock_guard<std::mutex> traceLock(m_traceMutex);
    
    m_profileInfo.clear();
    m_activeTraces.clear();
    m_traceHistory.clear();
    m_totalProfilingTimeNs = 0;
    m_startTime = std::chrono::steady_clock::now();
}

bool CodeProfiler::IsEnabled() const {
    return m_enabled;
}

void CodeProfiler::SetEnabled(bool enabled) {
    m_enabled = enabled;
}

bool CodeProfiler::IsTraceEnabled() const {
    return m_traceEnabled;
}

void CodeProfiler::SetTraceEnabled(bool enabled) {
    m_traceEnabled = enabled;
}

void CodeProfiler::SetMaxTraceEntries(size_t maxEntries) {
    std::lock_guard<std::mutex> lock(m_traceMutex);
    
    m_maxTraceEntries = maxEntries;
    ManageTraceBuffer();
}

std::vector<TraceEntry> CodeProfiler::GetTraceHistory(size_t limit) const {
    std::lock_guard<std::mutex> lock(m_traceMutex);
    
    if (limit == 0 || limit >= m_traceHistory.size()) {
        return m_traceHistory;
    }
    
    // 最新のトレースを指定数だけ返す
    return std::vector<TraceEntry>(
        m_traceHistory.end() - limit, m_traceHistory.end());
}

std::vector<TraceEntry> CodeProfiler::GetTraceHistoryForTarget(
    const std::string& targetId, size_t limit) const {
    
    std::lock_guard<std::mutex> lock(m_traceMutex);
    
    std::vector<TraceEntry> result;
    
    // 特定対象のトレースのみ抽出
    for (const auto& trace : m_traceHistory) {
        if (trace.targetId == targetId) {
            result.push_back(trace);
        }
    }
    
    // 指定された数まで切り詰め
    if (limit > 0 && result.size() > limit) {
        result.erase(result.begin(), result.end() - limit);
    }
    
    return result;
}

std::string CodeProfiler::GenerateReport(bool detailed) const {
    std::lock_guard<std::mutex> profileLock(m_profileMutex);
    
    std::ostringstream report;
    
    // 現在時刻
    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    
    report << "==========================================\n";
    report << "  コードプロファイリングレポート\n";
    report << "  生成時間: " << std::put_time(std::localtime(&nowTime), "%Y-%m-%d %H:%M:%S") << "\n";
    report << "==========================================\n\n";
    
    report << "グローバル統計:\n";
    report << "  プロファイリング対象数: " << m_profileInfo.size() << "\n";
    report << "  総プロファイリング時間: " << (m_totalProfilingTimeNs / 1000000.0) << "ms\n";
    report << "  有効状態: " << (m_enabled ? "有効" : "無効") << "\n";
    report << "  トレース記録: " << (m_traceEnabled ? "有効" : "無効") << "\n\n";
    
    // タイプ別の対象数
    std::unordered_map<ProfileTargetType, size_t> typeCounts;
    std::unordered_map<ProfileTargetType, uint64_t> typeTimes;
    
    for (const auto& pair : m_profileInfo) {
        // 特定の対象タイプを確認するために、トレース履歴から検索
        std::lock_guard<std::mutex> traceLock(m_traceMutex);
        ProfileTargetType type = ProfileTargetType::kFunction; // デフォルト
        
        for (const auto& trace : m_traceHistory) {
            if (trace.targetId == pair.first) {
                type = trace.targetType;
                break;
            }
        }
        
        typeCounts[type]++;
        typeTimes[type] += pair.second.totalExecutionTimeNs;
    }
    
    report << "プロファイルタイプ分布:\n";
    for (int i = 0; i < static_cast<int>(ProfileTargetType::kBlock) + 1; ++i) {
        ProfileTargetType type = static_cast<ProfileTargetType>(i);
        double percentage = m_totalProfilingTimeNs > 0 ? 
            (static_cast<double>(typeTimes[type]) / m_totalProfilingTimeNs * 100) : 0;
        
        report << "  " << ProfileTargetTypeToString(type) << ": " 
               << typeCounts[type] << " 対象 (" 
               << std::fixed << std::setprecision(2) << percentage << "% の時間)\n";
    }
    report << "\n";
    
    // 最適化フェーズ別の対象数
    std::unordered_map<OptimizationPhase, size_t> phaseCounts;
    for (const auto& pair : m_profileInfo) {
        phaseCounts[pair.second.optimizationPhase]++;
    }
    
    report << "最適化フェーズ分布:\n";
    for (int i = 0; i < static_cast<int>(OptimizationPhase::kDeadCodeElimination) + 1; ++i) {
        OptimizationPhase phase = static_cast<OptimizationPhase>(i);
        report << "  " << OptimizationPhaseToString(phase) << ": " 
               << phaseCounts[phase] << " 対象\n";
    }
    report << "\n";
    
    // 詳細レポートが要求された場合
    if (detailed && !m_profileInfo.empty()) {
        report << "ホットスポット (実行回数順):\n";
        
        auto hotTargets = GetHotTargets(10);
        for (const auto& pair : hotTargets) {
            const auto& info = pair.second;
            double avgTimeNs = info.executionCount > 0 ? 
                static_cast<double>(info.totalExecutionTimeNs) / info.executionCount : 0;
            
            report << "  " << pair.first << ":\n";
            report << "    実行回数: " << info.executionCount << "\n";
            report << "    合計時間: " << (info.totalExecutionTimeNs / 1000000.0) << "ms\n";
            report << "    平均時間: " << (avgTimeNs / 1000.0) << "μs\n";
            report << "    最大時間: " << (info.maxExecutionTimeNs / 1000.0) << "μs\n";
            report << "    最適化フェーズ: " 
                   << OptimizationPhaseToString(info.optimizationPhase) << "\n\n";
        }
        
        report << "最も遅い対象 (平均実行時間順):\n";
        
        auto slowTargets = GetSlowTargets(10);
        for (const auto& pair : slowTargets) {
            const auto& info = pair.second;
            double avgTimeNs = info.executionCount > 0 ? 
                static_cast<double>(info.totalExecutionTimeNs) / info.executionCount : 0;
            
            report << "  " << pair.first << ":\n";
            report << "    平均時間: " << (avgTimeNs / 1000.0) << "μs\n";
            report << "    実行回数: " << info.executionCount << "\n";
            report << "    合計時間: " << (info.totalExecutionTimeNs / 1000000.0) << "ms\n";
            report << "    最適化フェーズ: " 
                   << OptimizationPhaseToString(info.optimizationPhase) << "\n\n";
        }
    }
    
    return report.str();
}

std::string CodeProfiler::GenerateJsonReport() const {
    std::lock_guard<std::mutex> profileLock(m_profileMutex);
    
    std::ostringstream json;
    
    json << "{\n";
    json << "  \"timestamp\": " << std::chrono::system_clock::now().time_since_epoch().count() << ",\n";
    json << "  \"target_count\": " << m_profileInfo.size() << ",\n";
    json << "  \"total_time_ns\": " << m_totalProfilingTimeNs << ",\n";
    json << "  \"enabled\": " << (m_enabled ? "true" : "false") << ",\n";
    json << "  \"trace_enabled\": " << (m_traceEnabled ? "true" : "false") << ",\n";
    
    // タイプ別の統計
    json << "  \"type_stats\": {\n";
    std::unordered_map<ProfileTargetType, size_t> typeCounts;
    std::unordered_map<ProfileTargetType, uint64_t> typeTimes;
    
    for (const auto& pair : m_profileInfo) {
        // 特定の対象タイプを確認するために、トレース履歴から検索
        std::lock_guard<std::mutex> traceLock(m_traceMutex);
        ProfileTargetType type = ProfileTargetType::kFunction; // デフォルト
        
        for (const auto& trace : m_traceHistory) {
            if (trace.targetId == pair.first) {
                type = trace.targetType;
                break;
            }
        }
        
        typeCounts[type]++;
        typeTimes[type] += pair.second.totalExecutionTimeNs;
    }
    
    for (int i = 0; i < static_cast<int>(ProfileTargetType::kBlock) + 1; ++i) {
        ProfileTargetType type = static_cast<ProfileTargetType>(i);
        json << "    \"" << ProfileTargetTypeToString(type) << "\": {\n";
        json << "      \"count\": " << typeCounts[type] << ",\n";
        json << "      \"time_ns\": " << typeTimes[type] << "\n";
        
        if (i < static_cast<int>(ProfileTargetType::kBlock)) {
            json << "    },\n";
        } else {
            json << "    }\n";
        }
    }
    json << "  },\n";
    
    // 最適化フェーズ別の統計
    json << "  \"phase_stats\": {\n";
    std::unordered_map<OptimizationPhase, size_t> phaseCounts;
    for (const auto& pair : m_profileInfo) {
        phaseCounts[pair.second.optimizationPhase]++;
    }
    
    for (int i = 0; i < static_cast<int>(OptimizationPhase::kDeadCodeElimination) + 1; ++i) {
        OptimizationPhase phase = static_cast<OptimizationPhase>(i);
        json << "    \"" << OptimizationPhaseToString(phase) << "\": " << phaseCounts[phase];
        
        if (i < static_cast<int>(OptimizationPhase::kDeadCodeElimination)) {
            json << ",\n";
        } else {
            json << "\n";
        }
    }
    json << "  },\n";
    
    // ホットスポット
    json << "  \"hot_spots\": [\n";
    auto hotTargets = GetHotTargets(10);
    for (size_t i = 0; i < hotTargets.size(); ++i) {
        const auto& pair = hotTargets[i];
        const auto& info = pair.second;
        double avgTimeNs = info.executionCount > 0 ? 
            static_cast<double>(info.totalExecutionTimeNs) / info.executionCount : 0;
        
        json << "    {\n";
        json << "      \"id\": \"" << pair.first << "\",\n";
        json << "      \"execution_count\": " << info.executionCount << ",\n";
        json << "      \"total_time_ns\": " << info.totalExecutionTimeNs << ",\n";
        json << "      \"avg_time_ns\": " << avgTimeNs << ",\n";
        json << "      \"max_time_ns\": " << info.maxExecutionTimeNs << ",\n";
        json << "      \"min_time_ns\": " << info.minExecutionTimeNs << ",\n";
        json << "      \"optimization_phase\": \"" 
             << OptimizationPhaseToString(info.optimizationPhase) << "\"\n";
        
        if (i < hotTargets.size() - 1) {
            json << "    },\n";
        } else {
            json << "    }\n";
        }
    }
    json << "  ],\n";
    
    // 最も遅い対象
    json << "  \"slow_targets\": [\n";
    auto slowTargets = GetSlowTargets(10);
    for (size_t i = 0; i < slowTargets.size(); ++i) {
        const auto& pair = slowTargets[i];
        const auto& info = pair.second;
        double avgTimeNs = info.executionCount > 0 ? 
            static_cast<double>(info.totalExecutionTimeNs) / info.executionCount : 0;
        
        json << "    {\n";
        json << "      \"id\": \"" << pair.first << "\",\n";
        json << "      \"avg_time_ns\": " << avgTimeNs << ",\n";
        json << "      \"execution_count\": " << info.executionCount << ",\n";
        json << "      \"total_time_ns\": " << info.totalExecutionTimeNs << ",\n";
        json << "      \"optimization_phase\": \"" 
             << OptimizationPhaseToString(info.optimizationPhase) << "\"\n";
        
        if (i < slowTargets.size() - 1) {
            json << "    },\n";
        } else {
            json << "    }\n";
        }
    }
    json << "  ]\n";
    
    json << "}\n";
    
    return json.str();
}

void CodeProfiler::CompleteTrace(TraceEntry& trace, uint64_t endTimeNs) {
    trace.endTimeNs = endTimeNs;
    trace.executionTimeNs = endTimeNs - trace.startTimeNs;
}

void CodeProfiler::ManageTraceBuffer() {
    // トレース履歴が上限を超えたら古いものから削除
    if (m_traceHistory.size() > m_maxTraceEntries) {
        size_t toRemove = m_traceHistory.size() - m_maxTraceEntries;
        m_traceHistory.erase(m_traceHistory.begin(), m_traceHistory.begin() + toRemove);
    }
}

//=============================================================================
// ユーティリティ関数
//=============================================================================

std::string ProfileTargetTypeToString(ProfileTargetType type) {
    switch (type) {
        case ProfileTargetType::kFunction:   return "関数";
        case ProfileTargetType::kLoop:       return "ループ";
        case ProfileTargetType::kBranch:     return "分岐";
        case ProfileTargetType::kCall:       return "関数呼び出し";
        case ProfileTargetType::kBytecode:   return "バイトコード命令";
        case ProfileTargetType::kBlock:      return "コードブロック";
        default:                            return "不明";
    }
}

std::string OptimizationPhaseToString(OptimizationPhase phase) {
    switch (phase) {
        case OptimizationPhase::kNone:                return "最適化なし";
        case OptimizationPhase::kBaselineJIT:         return "ベースラインJIT";
        case OptimizationPhase::kInlineCaching:       return "インラインキャッシュ";
        case OptimizationPhase::kTypeSpecialization:  return "型特化";
        case OptimizationPhase::kInlining:            return "インライン化";
        case OptimizationPhase::kLoopOptimization:    return "ループ最適化";
        case OptimizationPhase::kDeadCodeElimination: return "デッドコード除去";
        default:                                     return "不明";
    }
}

} // namespace core
} // namespace aerojs 