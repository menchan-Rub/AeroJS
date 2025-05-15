/**
 * @file jit_profiler.h
 * @brief AeroJSのJIT最適化のための高性能プロファイラ
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <atomic>
#include <mutex>

namespace aerojs {

// 前方宣言
class Function;
class IRNode;
class Value;

// 型観測データ
struct TypeObservation {
    uint32_t primaryType = 0;   // 最も頻繁に観測された型
    uint32_t observationCount = 0; // 観測回数
    float confidence = 0.0f;    // 信頼度 (0.0-1.0)
    bool hasNaN = false;        // NaN値が観測されたか
    bool hasNegativeZero = false; // -0値が観測されたか
};

// 形状観測データ
struct ShapeObservation {
    uint64_t primaryShapeId = 0;  // 最も頻繁に観測された形状ID
    uint32_t observationCount = 0; // 観測回数
    uint32_t uniqueShapes = 0;    // ユニークな形状の数
    bool isMonomorphic = false;   // 単一形状のみか
};

// 関数プロファイルデータ
struct FunctionProfileData {
    // 型観測結果
    std::unordered_map<uint64_t, TypeObservation> typeObservations;
    
    // 形状観測結果
    std::unordered_map<uint64_t, ShapeObservation> shapeObservations;
    
    // 呼び出し回数
    uint64_t callCount = 0;
    
    // ホットパス情報
    std::vector<uint64_t> hotNodes;
    
    // ループ情報
    std::vector<uint64_t> numericLoops;
    
    // 文字列操作情報
    std::vector<uint64_t> stringOperations;
    
    // 実行時間情報 (ナノ秒)
    uint64_t totalExecutionTimeNs = 0;
    uint64_t averageExecutionTimeNs = 0;
};

// 呼び出しサイト情報
struct CallSiteInfo {
    uint64_t nodeId;            // 呼び出しノードID
    uint64_t calleeId;          // 呼び出された関数のID
    uint32_t callCount;         // 呼び出し回数
    bool isPolymorphic;         // 複数の関数が呼び出されたか
    std::vector<uint64_t> callees; // 呼び出された全関数のID
};

// JITプロファイラクラス
class JITProfiler {
public:
    JITProfiler();
    ~JITProfiler();
    
    // プロファイリング開始/停止
    void startProfiling(Function* function);
    void stopProfiling(Function* function);
    
    // データ記録
    void recordCall(uint64_t functionId);
    void recordType(uint64_t functionId, uint64_t nodeId, uint32_t type);
    void recordShape(uint64_t functionId, uint64_t nodeId, uint64_t shapeId);
    void recordCallSite(uint64_t callerId, uint64_t nodeId, uint64_t calleeId);
    void recordExecutionTime(uint64_t functionId, uint64_t timeNs);
    
    // データ取得
    const FunctionProfileData& getFunctionTypeInfo(uint64_t functionId) const;
    uint64_t getCallCount(uint64_t functionId) const;
    bool isOnHotPath(uint64_t nodeId) const;
    std::vector<CallSiteInfo> getCallSites(uint64_t functionId) const;
    
    // 分析
    void analyzeProfiles();
    bool shouldOptimize(uint64_t functionId) const;
    bool shouldDeoptimize(uint64_t functionId) const;
    
    // データリセット
    void resetProfileData(uint64_t functionId);
    void resetAllProfiles();
    
private:
    // プロファイルデータストレージ
    std::unordered_map<uint64_t, FunctionProfileData> _profiles;
    
    // ホットノードのマップ
    std::unordered_map<uint64_t, bool> _hotNodes;
    
    // 呼び出しサイト情報
    std::unordered_map<uint64_t, std::vector<CallSiteInfo>> _callSites;
    
    // 同期
    mutable std::mutex _profileMutex;
    
    // 最適化閾値
    static constexpr uint32_t OPTIMIZE_CALL_THRESHOLD = 100;
    static constexpr uint32_t TYPE_STABILITY_THRESHOLD = 10;
    
    // ヘルパーメソッド
    FunctionProfileData& getOrCreateProfile(uint64_t functionId);
};

} // namespace aerojs 