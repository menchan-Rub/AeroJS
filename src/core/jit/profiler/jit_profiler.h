/**
 * @file jit_profiler.h
 * @brief AeroJSのJIT最適化のための高性能プロファイラ
 * @version 2.0.0
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
#include <deque>
#include <chrono>
#include <optional>
#include <functional>
#include <array>

namespace aerojs {

// 前方宣言
class Function;
class IRNode;
class Value;
class ObjectShape;
class ExecutionContext;

// 型の分類
enum class ValueTypeCategory : uint8_t {
    Unknown = 0,
    Integer = 1,
    Double = 2,
    Boolean = 3,
    String = 4,
    Object = 5,
    Array = 6,
    Function = 7,
    Null = 8,
    Undefined = 9,
    Symbol = 10,
    BigInt = 11,
    SmallInt = 12,  // 31ビット整数としてタグ付けできる整数
    HeapNumber = 13, // ヒープ上の数値（BigIntや大きな数値）
    TaggedInt = 14,  // タグ付き整数
    External = 15,   // 外部/ネイティブデータ
    Mixed = 255      // 複数の型が混在
};

// 詳細な型情報
struct TypeProfile {
    ValueTypeCategory category = ValueTypeCategory::Unknown;
    struct {
        // 数値データの場合
        double minValue = 0.0;
        double maxValue = 0.0;
        bool isInteger = false;
        bool isNegative = false;
        bool hasSpecialValues = false; // NaN, Infinity, -0など
        bool fitsSmallInt = false;     // 31ビット整数に収まるか
    } number;
    
    struct {
        // 文字列データの場合
        uint32_t minLength = 0;
        uint32_t maxLength = 0;
        bool isAscii = true;
        bool isConstant = false;
        bool isInterned = false;
    } string;
    
    struct {
        // オブジェクトデータの場合
        uint64_t primaryShapeId = 0;
        uint32_t shapeCount = 0;
        bool isArray = false;
        bool isTypedArray = false;
        bool hasDenseElements = false;
    } object;
    
    // 型の安定性
    float stability = 0.0f;       // 0.0-1.0 (1.0が完全に安定)
    uint32_t observationCount = 0; // 観測回数
    uint32_t transitionCount = 0;  // 型の遷移回数
};

// 型観測データ
struct TypeObservation {
    ValueTypeCategory primaryType = ValueTypeCategory::Unknown;  // 最も頻繁に観測された型
    std::array<uint32_t, 16> typeCounts = {}; // 各型の観測回数
    uint32_t observationCount = 0;            // 全観測回数
    float stability = 0.0f;                   // 型の安定性 (0.0-1.0)
    bool hasNaN = false;                      // NaN値が観測されたか
    bool hasNegativeZero = false;             // -0値が観測されたか
    bool hasSpecialValues = false;            // 無限大などの特殊値
    
    // 数値範囲情報
    struct {
        double min = std::numeric_limits<double>::max();
        double max = std::numeric_limits<double>::lowest();
        bool rangeKnown = false;
        bool fitsInt32 = false;
        bool fitsUint32 = false;
    } numberRange;
    
    // 最近の値履歴（機械学習のため）
    std::deque<uint64_t> recentValues;
    
    // 詳細な型プロファイル（オプショナル）
    std::optional<TypeProfile> detailedProfile;
};

// 形状観測データ
struct ShapeObservation {
    uint64_t primaryShapeId = 0;    // 最も頻繁に観測された形状ID
    uint32_t observationCount = 0;   // 観測回数
    uint32_t uniqueShapes = 0;       // ユニークな形状の数
    bool isMonomorphic = false;      // 単一形状のみか
    bool isPolymorphic = false;      // 複数形状だが少数か
    bool isMegamorphic = false;      // 非常に多数の形状か
    
    // 形状遷移グラフ（どの形状からどの形状に変わったか）
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint32_t>> transitions;
    
    // オブジェクトへの一般的なアクセスパターン
    struct {
        std::vector<std::string> hotProperties;  // 頻繁にアクセスされるプロパティ
        bool hasStableLayout = true;             // レイアウトが安定しているか
        bool hasDeletedProperties = false;       // プロパティが削除されたか
    } accessPattern;
    
    // 最近観測された形状の履歴
    std::deque<uint64_t> recentShapes;
};

// 分岐予測情報
struct BranchData {
    uint64_t totalExecutions = 0;
    uint64_t takenCount = 0;        // 分岐が取られた回数
    float takenRatio = 0.0f;        // 分岐が取られた割合
    bool predictable = false;       // 予測可能かどうか
    bool alwaysTaken = false;       // 常に取られるか
    bool alwaysNotTaken = false;    // 常に取られないか
    
    // 依存する値の情報
    struct {
        uint64_t valueNodeId = 0;        // 依存する値のノードID
        ValueTypeCategory valueType = ValueTypeCategory::Unknown;  // 値の型
    } condition;
    
    // 分岐パターン（最近の履歴、機械学習用）
    std::deque<bool> pattern;
};

// ループ実行情報
struct LoopProfile {
    uint64_t loopId = 0;              // ループID
    uint64_t headerNodeId = 0;        // ループヘッダノードID
    uint64_t iterationCount = 0;      // 合計反復回数
    uint64_t executionCount = 0;      // ループ実行回数
    double averageIterations = 0.0;   // 平均反復回数
    uint64_t maxIterations = 0;       // 最大反復回数
    uint64_t minIterations = 0;       // 最小反復回数
    bool hasEarlyExits = false;       // 早期終了があるか
    
    // 誘導変数の情報
    struct InductionVariable {
        uint64_t nodeId = 0;           // 誘導変数ノードID
        int64_t initialValue = 0;      // 初期値
        int64_t stride = 0;            // ストライド（増加量）
        bool isMonotonic = false;      // 単調増加/減少か
        bool isInBounds = false;       // 範囲内に収まるか
    };
    std::vector<InductionVariable> inductionVars;
    
    // 配列アクセスパターン
    struct {
        bool hasRegularAccess = false;  // 規則的なアクセスパターンがあるか
        bool hasStrideAccess = false;   // ストライドアクセスパターンがあるか
        bool isVectorizable = false;    // ベクトル化可能か
    } arrayAccess;
    
    // ループの最適化ヒント
    struct {
        bool canUnroll = false;         // アンロール可能か
        bool canHoist = false;          // ループ不変式の巻き上げ可能か
        bool canVectorize = false;      // ベクトル化可能か
        bool canParallelize = false;    // 並列化可能か
        uint32_t recommendedUnrollFactor = 0; // 推奨アンロール係数
    } optimizationHints;
};

// 関数プロファイルデータ
struct FunctionProfileData {
    // 型観測結果
    std::unordered_map<uint64_t, TypeObservation> typeObservations;
    
    // 形状観測結果
    std::unordered_map<uint64_t, ShapeObservation> shapeObservations;
    
    // 分岐予測情報
    std::unordered_map<uint64_t, BranchData> branchData;
    
    // ループプロファイル
    std::unordered_map<uint64_t, LoopProfile> loopProfiles;
    
    // 呼び出し回数と実行統計
    uint64_t callCount = 0;
    uint64_t warmupCalls = 0;    // ウォームアップ段階の呼び出し回数
    uint64_t totalExecutionTimeNs = 0;
    uint64_t minExecutionTimeNs = UINT64_MAX;
    uint64_t maxExecutionTimeNs = 0;
    double averageExecutionTimeNs = 0.0;
    
    // ホットパス情報
    std::vector<uint64_t> hotNodes;
    std::vector<uint64_t> coldNodes;
    float hotPathCoverage = 0.0f;  // ホットパスが実行時間に占める割合
    
    // メモリアクセスプロファイル
    struct {
        uint64_t totalAllocations = 0;
        uint64_t allocatedBytes = 0;
        uint64_t maxHeapSize = 0;
        bool hasEscapingAllocations = false;
    } memoryProfile;
    
    // 実行履歴（機械学習用）
    struct ExecutionRecord {
        uint64_t timestamp;
        uint64_t executionTimeNs;
        uint32_t tierLevel;
        bool hadBailout;
    };
    std::deque<ExecutionRecord> executionHistory;
    
    // インライン化ヒント
    struct {
        bool isHot = false;
        bool isSmall = false;
        bool hasRecursion = false;
        bool isStable = false;
        float benefit = 0.0f;  // 0.0-1.0 (高いほど有益)
    } inliningHint;
    
    // 安定性と予測可能性
    float overallStability = 0.0f;    // 0.0-1.0
    float typeStability = 0.0f;       // 0.0-1.0
    float shapeStability = 0.0f;      // 0.0-1.0
    float controlFlowStability = 0.0f; // 0.0-1.0
    
    // 最適化と最適化解除の履歴
    struct OptimizationRecord {
        std::chrono::system_clock::time_point timestamp;
        uint8_t fromTier;
        uint8_t toTier;
        std::string reason;
        bool success;
    };
    std::vector<OptimizationRecord> optimizationHistory;
};

// 呼び出しサイト情報
struct CallSiteInfo {
    uint64_t nodeId;            // 呼び出しノードID
    uint64_t calleeId;          // 呼び出された関数のID
    uint32_t callCount;         // 呼び出し回数
    bool isPolymorphic;         // 複数の関数が呼び出されたか
    bool isMegamorphic;         // 非常に多くの関数が呼び出されたか
    std::vector<uint64_t> callees; // 呼び出された全関数のID
    
    // 各呼び出し先の呼び出し回数
    std::unordered_map<uint64_t, uint32_t> calleeCounts;
    
    // 呼び出しコンテキスト情報
    struct CallContext {
        ValueTypeCategory thisType;      // 'this'の型
        std::vector<ValueTypeCategory> argTypes; // 引数の型
        uint64_t count;                 // このコンテキストでの呼び出し回数
    };
    std::vector<CallContext> contexts;
    
    // インライン展開の決定に関する情報
    struct {
        bool wasInlined = false;
        std::string inliningDecision;
        float inliningBenefit = 0.0f;
    } inliningInfo;
};

// マシンコード生成統計
struct CodeGenStats {
    uint64_t codeSizeBytes = 0;
    uint64_t compilationTimeNs = 0;
    uint32_t registerSpillCount = 0;
    uint32_t instructionCount = 0;
    float optimizationLevel = 0.0f;
    bool usedSIMD = false;
    bool usedSpecialInstructions = false;
};

// JITプロファイラクラス
class JITProfiler {
public:
    JITProfiler();
    ~JITProfiler();
    
    // プロファイリング開始/停止
    void StartProfiling(Function* function);
    void StopProfiling(Function* function);
    
    // データ記録
    void RecordCall(uint64_t functionId);
    void RecordType(uint64_t functionId, uint64_t nodeId, ValueTypeCategory type, Value* value = nullptr);
    void RecordShape(uint64_t functionId, uint64_t nodeId, uint64_t shapeId, ObjectShape* shape = nullptr);
    void RecordCallSite(uint64_t callerId, uint64_t nodeId, uint64_t calleeId, 
                       const std::vector<Value*>& args = {}, Value* thisValue = nullptr);
    void RecordBranch(uint64_t functionId, uint64_t nodeId, bool taken, Value* condition = nullptr);
    void RecordLoop(uint64_t functionId, uint64_t loopId, uint64_t headerNodeId, uint64_t iterations);
    void RecordExecutionTime(uint64_t functionId, uint64_t timeNs);
    void RecordBailout(uint64_t functionId, uint64_t nodeId, const std::string& reason);
    void RecordAllocation(uint64_t functionId, uint64_t nodeId, uint64_t size);
    void RecordCompilation(uint64_t functionId, uint8_t tier, uint64_t codeSizeBytes);
    void RecordDeoptimization(uint64_t functionId, uint64_t nodeId, const std::string& reason);
    
    // データ取得
    const FunctionProfileData& GetFunctionProfile(uint64_t functionId) const;
    const TypeObservation& GetTypeInfo(uint64_t functionId, uint64_t nodeId) const;
    const ShapeObservation& GetShapeInfo(uint64_t functionId, uint64_t nodeId) const;
    uint64_t GetCallCount(uint64_t functionId) const;
    bool IsHotFunction(uint64_t functionId) const;
    bool IsOnHotPath(uint64_t functionId, uint64_t nodeId) const;
    std::vector<CallSiteInfo> GetCallSites(uint64_t functionId) const;
    std::vector<uint64_t> GetHotLoops(uint64_t functionId) const;
    
    // 分析
    void AnalyzeProfiles();
    bool ShouldOptimize(uint64_t functionId) const;
    bool ShouldDeoptimize(uint64_t functionId) const;
    uint8_t RecommendTierLevel(uint64_t functionId) const;
    bool ShouldSpecializeForTypes(uint64_t functionId, uint64_t nodeId) const;
    bool ShouldInline(uint64_t callerId, uint64_t calleeId, uint64_t callSiteId) const;
    
    // 最適化のヒント
    float GetFunctionHotness(uint64_t functionId) const; // 0.0-1.0
    float GetNodeHotness(uint64_t functionId, uint64_t nodeId) const; // 0.0-1.0
    bool IsPolymorphicCallSite(uint64_t functionId, uint64_t callSiteId) const;
    bool CanUseDirectDispatch(uint64_t functionId, uint64_t callSiteId) const;
    bool CanUnrollLoop(uint64_t functionId, uint64_t loopId, uint32_t& recommendedFactor) const;
    
    // 動的コンパイル最適化のフィードバック
    void ProvideFeedback(uint64_t functionId, const std::string& category, 
                        const std::string& action, bool success);
    
    // 学習モデルによる最適化支援
    struct OptimizationAdvice {
        float confidence;        // 0.0-1.0
        uint8_t recommendedTier; // 推奨Tier
        bool shouldSpecialize;   // 特殊化すべきか
        bool shouldInline;       // インライン化すべきか
        std::vector<std::string> suggestedOptimizations;
    };
    OptimizationAdvice GetMLBasedAdvice(uint64_t functionId) const;
    
    // データリセット
    void ResetProfileData(uint64_t functionId);
    void ResetAllProfiles();
    
    // プロファイルデータのエクスポート/インポート
    std::string ExportProfileData(uint64_t functionId) const;
    bool ImportProfileData(uint64_t functionId, const std::string& data);
    
    // 実行コンテキスト統合
    void SetExecutionContext(ExecutionContext* context) { m_context = context; }
    
    // プロファイリングイベントのコールバック登録
    using ProfileEventCallback = std::function<void(uint64_t, const std::string&, const std::string&)>;
    void RegisterProfileEventCallback(ProfileEventCallback callback);
    
private:
    // プロファイルデータストレージ
    std::unordered_map<uint64_t, FunctionProfileData> m_profiles;
    
    // ホットノードのマップ
    std::unordered_map<uint64_t, float> m_nodeHotness;
    
    // 呼び出しサイト情報
    std::unordered_map<uint64_t, std::vector<CallSiteInfo>> m_callSites;
    
    // コード生成統計
    std::unordered_map<uint64_t, std::unordered_map<uint8_t, CodeGenStats>> m_codeGenStats;
    
    // 同期
    mutable std::mutex m_profileMutex;
    
    // 実行コンテキスト参照
    ExecutionContext* m_context = nullptr;
    
    // イベントコールバック
    std::vector<ProfileEventCallback> m_eventCallbacks;
    
    // 設定可能な閾値
    struct Thresholds {
        uint32_t hotFunctionCallCount = 100;    // ホット関数の呼び出し閾値
        uint32_t typeStabilityObservations = 10; // 型安定性の観測閾値
        float hotNodeCoverage = 0.8f;           // ホットノードカバレッジ閾値
        float typeStabilityThreshold = 0.9f;     // 型安定性閾値
        float inliningBenefitThreshold = 0.5f;   // インライン化利益閾値
        uint32_t loopHotnessThreshold = 20;      // ループのホットさ閾値
    } m_thresholds;
    
    // ヘルパーメソッド
    FunctionProfileData& GetOrCreateProfile(uint64_t functionId);
    void UpdateTypeStability(FunctionProfileData& profile);
    void IdentifyHotNodes(FunctionProfileData& profile);
    void FireProfileEvent(uint64_t functionId, const std::string& category, const std::string& action);
    void AnalyzeLoopOptimizationOpportunities(FunctionProfileData& profile);
    void AnalyzeInliningOpportunities();
    void UpdateOptimizationHistory(uint64_t functionId, uint8_t fromTier, uint8_t toTier, 
                                  const std::string& reason, bool success);
};

} // namespace aerojs 