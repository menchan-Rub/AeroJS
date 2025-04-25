#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/core/bytecode.h"
#include "src/core/jit/baseline/baseline_jit.h"
#include "src/core/jit/optimizing/optimizing_jit.h"
#include "src/core/jit/super_optimizing/super_optimizing_jit.h"
#include "src/core/jit/jit_profiler.h"

namespace aerojs::core {

/**
 * @brief JITコンパイルの最適化レベル
 */
enum class JITOptimizationTier {
    None,           ///< JIT無効（インタプリタのみ）
    Baseline,       ///< ベースラインJIT（最速のコンパイル、最小限の最適化）
    Optimized,      ///< 最適化JIT（中程度の最適化）
    SuperOptimized  ///< 超最適化JIT（最大限の最適化）
};

/**
 * @brief JITコンパイル対象関数の状態
 */
struct FunctionCompilationState {
    uint32_t functionId;                  ///< 関数ID
    JITOptimizationTier currentTier;      ///< 現在の最適化レベル
    uint32_t executionCount;              ///< 実行回数
    uint32_t osrEntryCount;               ///< OSRエントリ回数
    uint64_t totalExecutionTime;          ///< 合計実行時間（ナノ秒）
    bool isHot;                           ///< ホット関数フラグ
    bool hasTypeInstability;              ///< 型不安定フラグ
    bool compilationInProgress;           ///< コンパイル中フラグ
    
    std::vector<ProfiledTypeInfo> argTypes;  ///< 引数の型情報
    std::vector<ProfiledTypeInfo> varTypes;  ///< ローカル変数の型情報
    
    FunctionCompilationState(uint32_t id)
        : functionId(id)
        , currentTier(JITOptimizationTier::None)
        , executionCount(0)
        , osrEntryCount(0)
        , totalExecutionTime(0)
        , isHot(false)
        , hasTypeInstability(false)
        , compilationInProgress(false) {}
};

/**
 * @brief JIT最適化ポリシー設定
 */
struct JITOptimizerPolicy {
    uint32_t baselineThreshold;          ///< ベースラインJITへの遷移閾値
    uint32_t optimizingThreshold;        ///< 最適化JITへの遷移閾値
    uint32_t superOptimizingThreshold;   ///< 超最適化JITへの遷移閾値
    uint32_t osrThreshold;               ///< OSR実行閾値
    uint32_t deoptThreshold;             ///< 逆最適化閾値
    bool enableConcurrentCompilation;    ///< 並列コンパイル有効フラグ
    bool enableSpeculativeOptimization;  ///< 投機的最適化有効フラグ
    bool enableInlining;                 ///< インライン化有効フラグ
    
    JITOptimizerPolicy()
        : baselineThreshold(10)            // 10回実行でベースラインJIT
        , optimizingThreshold(1000)        // 1000回実行でオプティマイジングJIT
        , superOptimizingThreshold(10000) // 10000回実行で超最適化
        , osrThreshold(1000)              // 1000イテレーションでOSR
        , deoptThreshold(5)               // 5回型チェック失敗で逆最適化
        , enableConcurrentCompilation(true)
        , enableSpeculativeOptimization(true)
        , enableInlining(true) {}
};

/**
 * @brief 型プロファイリング情報
 */
struct ProfiledTypeInfo {
    enum class ValueType {
        Unknown,
        Int32,
        Float64,
        String,
        Object,
        Boolean,
        Undefined,
        Null
    };
    
    ValueType expectedType = ValueType::Unknown;
    uint32_t typeCheckFailures = 0;
    
    // 型安定性の判断
    bool isStable() const { return typeCheckFailures < 3; }
};

/**
 * @brief コンパイル済みのコードを表すポインタ型
 */
using CompiledCodePtr = std::unique_ptr<uint8_t[]>;

/**
 * @brief JITコンパイラマネージャークラス
 * 
 * JITコンパイラの複数階層とその切り替えを管理する責務を持ちます。
 */
class JITManager {
public:
    /**
     * @brief コンストラクタ
     * @param policy 最適化ポリシー
     */
    explicit JITManager(const JITOptimizerPolicy& policy = JITOptimizerPolicy());
    
    /**
     * @brief デストラクタ
     */
    ~JITManager();
    
    /**
     * @brief 関数のコンパイルを要求または取得
     * @param functionId 関数ID
     * @param bytecodes バイトコード
     * @return コンパイル結果
     */
    CompiledCodePtr getOrCompileFunction(uint32_t functionId, const std::vector<Bytecode>& bytecodes);
    
    /**
     * @brief 関数の実行カウンタを更新
     * @param functionId 関数ID
     * @param incrementCount 増分値
     */
    void incrementExecutionCount(uint32_t functionId, uint32_t incrementCount = 1);
    
    /**
     * @brief 型フィードバック情報を記録
     * @param functionId 関数ID
     * @param varIndex 変数インデックス
     * @param typeInfo 型情報
     */
    void recordTypeInfo(uint32_t functionId, uint32_t varIndex, const ProfiledTypeInfo& typeInfo);
    
    /**
     * @brief 関数の型安定性をマーク
     * @param functionId 関数ID
     * @param stable 安定フラグ
     */
    void markTypeStability(uint32_t functionId, bool stable);
    
    /**
     * @brief 関数の強制再コンパイルを要求
     * @param functionId 関数ID
     * @param targetTier 目標最適化レベル
     */
    void recompileFunction(uint32_t functionId, JITOptimizationTier targetTier);
    
    /**
     * @brief OSRエントリーポイントを取得
     * @param functionId 関数ID
     * @param bytecodeOffset バイトコードオフセット
     * @param loopIteration ループの現在の反復回数
     * @return OSRエントリーポイント
     */
    CompiledCodePtr getOSREntryPoint(uint32_t functionId, uint32_t bytecodeOffset, uint32_t loopIteration);
    
    /**
     * @brief 最適化ポリシーを設定
     * @param policy 新しいポリシー
     */
    void setPolicy(const JITOptimizerPolicy& policy);
    
    /**
     * @brief 実行プロファイラを取得
     * @return プロファイラへの参照
     */
    JITProfiler& profiler() { return m_profiler; }
    
    /**
     * @brief コンパイル統計情報を取得
     * @return 統計情報文字列
     */
    std::string getCompilationStatistics() const;

private:
    /**
     * @brief 関数の最適化階層を決定
     * @param state 関数の状態
     * @return 最適化階層
     */
    JITOptimizationTier determineTargetTier(const FunctionCompilationState& state) const;
    
    /**
     * @brief 適切なJITコンパイラを使って関数をコンパイル
     * @param functionId 関数ID
     * @param bytecodes バイトコード
     * @param targetTier 目標最適化レベル
     * @return コンパイル結果
     */
    CompiledCodePtr compileFunction(uint32_t functionId, const std::vector<Bytecode>& bytecodes, JITOptimizationTier targetTier);
    
    /**
     * @brief 関数の状態を取得または作成
     * @param functionId 関数ID
     * @return 関数状態への参照
     */
    FunctionCompilationState& getOrCreateFunctionState(uint32_t functionId);
    
    /**
     * @brief デオプティマイズ時のコールバック
     * @param functionId 関数ID
     * @param reason 逆最適化理由
     */
    void onDeoptimization(uint32_t functionId, const std::string& reason);

private:
    // 各最適化階層のJITコンパイラ
    std::unique_ptr<BaselineJIT> m_baselineJIT;
    std::unique_ptr<OptimizingJIT> m_optimizingJIT;
    std::unique_ptr<SuperOptimizingJIT> m_superOptimizingJIT;
    
    // 実行プロファイラ
    JITProfiler m_profiler;
    
    // コンパイル済み関数のキャッシュ
    std::unordered_map<uint32_t, CompiledCodePtr> m_compiledFunctions;
    
    // 関数のコンパイル状態
    std::unordered_map<uint32_t, FunctionCompilationState> m_functionStates;
    
    // OSRエントリポイントのキャッシュ
    std::map<std::string, CompiledCodePtr> m_osrEntryPoints;
    
    // 最適化ポリシー
    JITOptimizerPolicy m_policy;
    
    // 統計情報
    struct Statistics {
        uint32_t baselineCompilations;
        uint32_t optimizedCompilations;
        uint32_t superOptimizedCompilations;
        uint32_t deoptimizations;
        uint32_t osrEntries;
        
        Statistics() 
            : baselineCompilations(0)
            , optimizedCompilations(0)
            , superOptimizedCompilations(0)
            , deoptimizations(0)
            , osrEntries(0) {}
    } m_stats;
};

} // namespace aerojs::core 