#include "tiered_jit_manager.h"
#include <chrono>
#include <sstream>

namespace aerojs::core {

TieredJITManager::TieredJITManager()
    : m_baselineJIT(nullptr)
    , m_optimizedJIT(nullptr)
    , m_superOptimizedJIT(nullptr)
    , m_profiler(nullptr)
    , m_tieredCompilationEnabled(true)
    , m_tierUpThreshold(1000)
    , m_tierDownThreshold(5)
{
}

TieredJITManager::~TieredJITManager() {
    Shutdown();
}

bool TieredJITManager::Initialize() {
    // 各JITコンパイラの初期化
    m_baselineJIT = std::make_unique<BaselineJIT>();
    m_baselineJIT->EnableProfiling(true);
    
    // 最適化JITとスーパー最適化JITは実際の実装に合わせて作成
    // m_optimizedJIT = std::make_unique<OptimizingJIT>();
    // m_superOptimizedJIT = std::make_unique<SuperOptimizingJIT>();
    
    // プロファイラを初期化
    m_profiler = std::make_unique<JITProfiler>();
    return m_profiler->Initialize();
}

void TieredJITManager::Shutdown() {
    if (m_profiler) {
        m_profiler->Shutdown();
    }
    
    // 関数状態のクリーンアップ
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_functionStates.clear();
}

uint32_t TieredJITManager::CompileFunction(uint32_t functionId, const std::vector<uint8_t>& bytecodes, JITTier tier) {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    // 関数の実行状態を取得または作成
    auto* state = GetOrCreateFunctionState(functionId);
    
    // すでにコンパイル済みで同じ階層なら再利用
    if (state->tierStates.find(tier) != state->tierStates.end() && 
        state->tierStates[tier].isCompiled) {
        return state->tierStates[tier].entryPoint;
    }
    
    // コンパイル中フラグをセット
    state->isCompiling = true;
    
    // プロファイラに関数を登録
    m_profiler->RegisterFunction(functionId, bytecodes.size());
    
    // 実際のコンパイル処理
    JITCompiler* compiler = GetCompilerForTier(tier);
    if (!compiler) {
        state->isCompiling = false;
        return 0;
    }
    
    // 関数IDをセット (BaselineJITの場合)
    if (tier == JITTier::Baseline && m_baselineJIT) {
        m_baselineJIT->SetFunctionId(functionId);
    }
    
    // コンパイル実行
    size_t codeSize = 0;
    std::unique_ptr<uint8_t[]> code = compiler->Compile(bytecodes, codeSize);
    
    // コンパイル結果の処理
    uint32_t entryPoint = 0;
    if (code && codeSize > 0) {
        // ここでは仮想的なエントリポイントとして単純なハッシュを使用
        entryPoint = functionId * 1000 + static_cast<uint32_t>(tier);
        
        // 実際にはコード管理クラスにコードを登録する必要あり
        // codeManager->RegisterCode(entryPoint, std::move(code), codeSize);
        
        // 関数状態を更新
        UpdateFunctionState(functionId, tier, entryPoint, static_cast<uint32_t>(codeSize));
    }
    
    state->isCompiling = false;
    return entryPoint;
}

void TieredJITManager::TriggerTierUpCompilation(uint32_t functionId, JITTier targetTier) {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    auto* state = GetFunctionState(functionId);
    if (!state || state->isCompiling || state->currentTier >= targetTier) {
        return;
    }
    
    // 実際の処理では、バイトコードを取得して再コンパイルを実行
    // ここでは簡略化のため、バイトコード取得とコンパイル実行は省略
    
    // コンパイル成功を仮定して状態を更新
    state->currentTier = targetTier;
}

void TieredJITManager::TriggerTierDownCompilation(uint32_t functionId, JITTier targetTier, const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    auto* state = GetFunctionState(functionId);
    if (!state || state->isCompiling || state->currentTier <= targetTier) {
        return;
    }
    
    // 逆最適化の理由をプロファイラに記録
    m_profiler->RecordDeoptimization(functionId, 0, reason);
    
    // 階層を下げる
    state->currentTier = targetTier;
    state->needsRecompilation = true;
    
    // エントリポイントを変更
    if (state->tierStates.find(targetTier) != state->tierStates.end() && 
        state->tierStates[targetTier].isCompiled) {
        state->entryPoint = state->tierStates[targetTier].entryPoint;
    } else {
        // 対象階層がコンパイル済みでない場合は再コンパイルが必要
        // 実際の実装では、この後に適切な処理を行う
    }
}

uint32_t TieredJITManager::GetEntryPoint(uint32_t functionId) const {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    auto* state = GetFunctionState(functionId);
    if (!state) {
        return 0;
    }
    
    return state->entryPoint;
}

JITTier TieredJITManager::GetCurrentTier(uint32_t functionId) const {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    auto* state = GetFunctionState(functionId);
    if (!state) {
        return JITTier::Interpreter;
    }
    
    return state->currentTier;
}

void TieredJITManager::RecordExecution(uint32_t functionId, uint32_t bytecodeOffset) {
    if (!m_profiler) return;
    
    m_profiler->RecordExecution(functionId, bytecodeOffset);
    
    // 実行カウントが閾値を超えたかチェックして階層アップを検討
    if (m_tieredCompilationEnabled && ShouldTierUp(functionId)) {
        auto currentTier = GetCurrentTier(functionId);
        JITTier nextTier = JITTier::Interpreter;
        
        switch (currentTier) {
            case JITTier::Interpreter:
                nextTier = JITTier::Baseline;
                break;
            case JITTier::Baseline:
                nextTier = JITTier::Optimized;
                break;
            case JITTier::Optimized:
                nextTier = JITTier::SuperOptimized;
                break;
            default:
                return;
        }
        
        if (nextTier != JITTier::Interpreter) {
            TriggerTierUpCompilation(functionId, nextTier);
        }
    }
}

void TieredJITManager::RecordCallSite(uint32_t callerFunctionId, uint32_t callSiteOffset, uint32_t calleeFunctionId, uint32_t executionTimeNs) {
    if (!m_profiler) return;
    
    m_profiler->RecordCallSite(callerFunctionId, callSiteOffset, calleeFunctionId, executionTimeNs);
}

void TieredJITManager::RecordTypeObservation(uint32_t functionId, uint32_t bytecodeOffset, TypeFeedbackRecord::TypeCategory type) {
    if (!m_profiler) return;
    
    m_profiler->RecordTypeObservation(functionId, bytecodeOffset, type);
    
    // 型不安定性が閾値を超えたかチェックして階層ダウンを検討
    if (m_tieredCompilationEnabled && ShouldTierDown(functionId)) {
        auto currentTier = GetCurrentTier(functionId);
        if (currentTier > JITTier::Baseline) {
            TriggerTierDownCompilation(functionId, JITTier::Baseline, "型不安定のため");
        }
    }
}

void TieredJITManager::RecordDeoptimization(uint32_t functionId, uint32_t bytecodeOffset, const std::string& reason) {
    if (!m_profiler) return;
    
    m_profiler->RecordDeoptimization(functionId, bytecodeOffset, reason);
}

void TieredJITManager::EnableTieredCompilation(bool enable) {
    m_tieredCompilationEnabled = enable;
}

void TieredJITManager::InvalidateFunction(uint32_t functionId) {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    auto it = m_functionStates.find(functionId);
    if (it != m_functionStates.end()) {
        // 関数の状態をリセット
        it->second.currentTier = JITTier::Interpreter;
        it->second.entryPoint = 0;
        it->second.isCompiling = false;
        it->second.needsRecompilation = false;
        it->second.tierStates.clear();
    }
    
    // プロファイラの状態もリセット
    if (m_profiler) {
        m_profiler->UnregisterFunction(functionId);
    }
}

void TieredJITManager::ResetAllCompilations() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    // すべての関数状態をクリア
    m_functionStates.clear();
    
    // プロファイラをリセット
    if (m_profiler) {
        m_profiler->ResetAllProfiles();
    }
}

std::string TieredJITManager::GetCompilationStatistics() const {
    std::stringstream ss;
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    // コンパイル統計情報
    size_t interpreterCount = 0;
    size_t baselineCount = 0;
    size_t optimizedCount = 0;
    size_t superOptimizedCount = 0;
    
    for (const auto& [id, state] : m_functionStates) {
        switch (state.currentTier) {
            case JITTier::Interpreter:
                interpreterCount++;
                break;
            case JITTier::Baseline:
                baselineCount++;
                break;
            case JITTier::Optimized:
                optimizedCount++;
                break;
            case JITTier::SuperOptimized:
                superOptimizedCount++;
                break;
        }
    }
    
    ss << "JIT階層別関数数:\n";
    ss << "  インタプリタ: " << interpreterCount << "\n";
    ss << "  ベースライン: " << baselineCount << "\n";
    ss << "  最適化: " << optimizedCount << "\n";
    ss << "  超最適化: " << superOptimizedCount << "\n";
    ss << "総関数数: " << m_functionStates.size() << "\n";
    
    return ss.str();
}

std::string TieredJITManager::GetProfileSummary() const {
    if (!m_profiler) {
        return "プロファイラが初期化されていません";
    }
    
    return m_profiler->GetProfileSummary();
}

void TieredJITManager::SetHotFunctionThreshold(uint32_t threshold) {
    // 実際は内部的に利用するしきい値
}

void TieredJITManager::SetTierUpThreshold(uint32_t threshold) {
    m_tierUpThreshold = threshold;
}

void TieredJITManager::SetTierDownThreshold(uint32_t threshold) {
    m_tierDownThreshold = threshold;
}

bool TieredJITManager::ShouldTierUp(uint32_t functionId) const {
    // 関数がホットかどうかの判定
    if (!m_profiler) return false;
    
    return m_profiler->IsFunctionHot(functionId, m_tierUpThreshold);
}

bool TieredJITManager::ShouldTierDown(uint32_t functionId) const {
    // 型不安定性の判定ロジック
    if (!m_profiler) return false;
    
    const FunctionProfile* profile = m_profiler->GetFunctionProfile(functionId);
    if (!profile) return false;
    
    // 型フィードバックレコードをチェック
    // 単純化のため、任意の型フィードバックの不安定性をチェック
    for (const auto& [offset, record] : profile->typeFeedback) {
        // 型安定性が低く、かつ十分な観測数がある場合
        if (record.GetTypeStability() < 0.8 && record.totalObservations > m_tierDownThreshold) {
            return true;
        }
    }
    
    return false;
}

JITCompiler* TieredJITManager::GetCompilerForTier(JITTier tier) const {
    switch (tier) {
        case JITTier::Baseline:
            return m_baselineJIT.get();
        case JITTier::Optimized:
            return m_optimizedJIT.get();
        case JITTier::SuperOptimized:
            return m_superOptimizedJIT.get();
        default:
            return nullptr;
    }
}

uint64_t TieredJITManager::GetCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void TieredJITManager::UpdateFunctionState(uint32_t functionId, JITTier tier, uint32_t entryPoint, uint32_t codeSize) {
    auto* state = GetOrCreateFunctionState(functionId);
    
    // 階層状態を更新
    auto& tierState = state->tierStates[tier];
    tierState.isCompiled = true;
    tierState.entryPoint = entryPoint;
    tierState.codeSize = codeSize;
    tierState.compilationTimestamp = GetCurrentTimestamp();
    
    // 現在の階層を更新（より高い階層に移行する場合のみ）
    if (tier > state->currentTier) {
        state->currentTier = tier;
    }
    
    // エントリポイントを更新
    state->entryPoint = entryPoint;
    state->codeSize = codeSize;
    state->needsRecompilation = false;
}

FunctionExecutionState* TieredJITManager::GetOrCreateFunctionState(uint32_t functionId) {
    auto it = m_functionStates.find(functionId);
    if (it != m_functionStates.end()) {
        return &it->second;
    }
    
    // 新しい状態を作成
    auto& newState = m_functionStates[functionId];
    newState.functionId = functionId;
    return &newState;
}

const FunctionExecutionState* TieredJITManager::GetFunctionState(uint32_t functionId) const {
    auto it = m_functionStates.find(functionId);
    if (it != m_functionStates.end()) {
        return &it->second;
    }
    
    return nullptr;
}

} // namespace aerojs::core 