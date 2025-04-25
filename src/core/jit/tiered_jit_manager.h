#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <string>

#include "jit_compiler.h"
#include "baseline/baseline_jit.h"
#include "jit_profiler.h"

namespace aerojs::core {

// JIT階層の種類
enum class JITTier {
    Interpreter,    // インタープリタ実行（JITなし）
    Baseline,       // 基本的なJIT（最低限の最適化）
    Optimized,      // 最適化JIT（プロファイリングデータを利用した最適化）
    SuperOptimized  // 超最適化JIT（特殊なケースに特化した最適化）
};

// 関数の実行状態を表す構造体
struct FunctionExecutionState {
    uint32_t functionId;      // 関数のID
    JITTier currentTier;      // 現在のJIT階層
    uint32_t entryPoint;      // 機械語コードのエントリポイント
    uint32_t codeSize;        // 機械語コードのサイズ
    bool isCompiling;         // コンパイル中フラグ
    bool needsRecompilation;  // 再コンパイルが必要かどうか
    
    // 各階層の状態
    struct TierState {
        bool isCompiled;              // コンパイル済みかどうか
        uint32_t entryPoint;          // 機械語コードのエントリポイント
        uint32_t codeSize;            // 機械語コードのサイズ
        uint64_t compilationTimestamp; // コンパイル時のタイムスタンプ
    };
    
    std::unordered_map<JITTier, TierState> tierStates;
    
    // デフォルトコンストラクタ
    FunctionExecutionState()
        : functionId(0)
        , currentTier(JITTier::Interpreter)
        , entryPoint(0)
        , codeSize(0)
        , isCompiling(false)
        , needsRecompilation(false)
    {
    }
};

// 階層型JITコンパイラマネージャークラス
class TieredJITManager {
public:
    TieredJITManager();
    ~TieredJITManager();
    
    // 初期化・終了
    bool Initialize();
    void Shutdown();
    
    // コンパイル関連
    uint32_t CompileFunction(uint32_t functionId, const std::vector<uint8_t>& bytecodes, JITTier tier = JITTier::Baseline);
    void TriggerTierUpCompilation(uint32_t functionId, JITTier targetTier);
    void TriggerTierDownCompilation(uint32_t functionId, JITTier targetTier, const std::string& reason);
    
    // 実行関連
    uint32_t GetEntryPoint(uint32_t functionId) const;
    JITTier GetCurrentTier(uint32_t functionId) const;
    
    // プロファイリング関連
    void RecordExecution(uint32_t functionId, uint32_t bytecodeOffset);
    void RecordCallSite(uint32_t callerFunctionId, uint32_t callSiteOffset, uint32_t calleeFunctionId, uint32_t executionTimeNs);
    void RecordTypeObservation(uint32_t functionId, uint32_t bytecodeOffset, TypeFeedbackRecord::TypeCategory type);
    void RecordDeoptimization(uint32_t functionId, uint32_t bytecodeOffset, const std::string& reason);
    
    // 管理関連
    void EnableTieredCompilation(bool enable);
    void InvalidateFunction(uint32_t functionId);
    void ResetAllCompilations();
    
    // デバッグ・統計情報
    std::string GetCompilationStatistics() const;
    std::string GetProfileSummary() const;
    
    // 最適化ポリシー設定
    void SetHotFunctionThreshold(uint32_t threshold);
    void SetTierUpThreshold(uint32_t threshold);
    void SetTierDownThreshold(uint32_t threshold);
    
private:
    // JITコンパイラインスタンス
    std::unique_ptr<BaselineJIT> m_baselineJIT;
    std::unique_ptr<JITCompiler> m_optimizedJIT;
    std::unique_ptr<JITCompiler> m_superOptimizedJIT;
    
    // プロファイラ
    std::unique_ptr<JITProfiler> m_profiler;
    
    // 関数実行状態の管理
    mutable std::mutex m_stateMutex;
    std::unordered_map<uint32_t, FunctionExecutionState> m_functionStates;
    
    // 設定
    bool m_tieredCompilationEnabled;
    uint32_t m_tierUpThreshold;
    uint32_t m_tierDownThreshold;
    
    // 内部ヘルパーメソッド
    bool ShouldTierUp(uint32_t functionId) const;
    bool ShouldTierDown(uint32_t functionId) const;
    JITCompiler* GetCompilerForTier(JITTier tier) const;
    uint64_t GetCurrentTimestamp() const;
    void UpdateFunctionState(uint32_t functionId, JITTier tier, uint32_t entryPoint, uint32_t codeSize);
    FunctionExecutionState* GetOrCreateFunctionState(uint32_t functionId);
    const FunctionExecutionState* GetFunctionState(uint32_t functionId) const;
};

} // namespace aerojs::core 