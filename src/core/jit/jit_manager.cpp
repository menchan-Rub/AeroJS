#include "src/core/jit/jit_manager.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

namespace aerojs::core {

// 複合キーのヘルパー関数
std::string createOSRKey(uint32_t functionId, uint32_t offset) {
    return std::to_string(functionId) + ":" + std::to_string(offset);
}

JITManager::JITManager(const JITOptimizerPolicy& policy)
    : m_policy(policy)
    , m_profiler() {
    // 各JITコンパイラの初期化
    m_baselineJIT = std::make_unique<BaselineJIT>();
    m_baselineJIT->EnableProfiling(true);
    
    m_optimizingJIT = std::make_unique<OptimizingJIT>();
    m_optimizingJIT->SetProfiler(std::make_shared<JITProfiler>(m_profiler));
    
    m_superOptimizingJIT = std::make_unique<SuperOptimizingJIT>();
    
    // プロファイラを初期化
    m_profiler.Initialize();
}

JITManager::~JITManager() {
    m_profiler.Shutdown();
}

CompiledCodePtr JITManager::getOrCompileFunction(uint32_t functionId, const std::vector<Bytecode>& bytecodes) {
    // コンパイル済みかチェック
    auto it = m_compiledFunctions.find(functionId);
    if (it != m_compiledFunctions.end()) {
        return it->second;
    }
    
    // 関数状態を取得または作成
    auto& state = getOrCreateFunctionState(functionId);
    
    // 目標階層を決定
    JITOptimizationTier targetTier = determineTargetTier(state);
    
    // コンパイル実行
    return compileFunction(functionId, bytecodes, targetTier);
}

void JITManager::incrementExecutionCount(uint32_t functionId, uint32_t incrementCount) {
    auto& state = getOrCreateFunctionState(functionId);
    state.executionCount += incrementCount;
    
    // プロファイラに実行カウント増加を記録
    m_profiler.RecordExecution(functionId, 0);
    
    // 現在のティアと目標ティアを比較
    JITOptimizationTier targetTier = determineTargetTier(state);
    
    // より高度な最適化が必要かつ可能なら再コンパイル
    if (targetTier > state.currentTier && !state.compilationInProgress) {
        // 再コンパイルフラグを設定
        state.compilationInProgress = true;
        
        if (m_policy.enableConcurrentCompilation) {
            // 非同期コンパイル（別スレッドで実行）
            std::thread([this, functionId, targetTier]() {
                // ここではバイトコードが必要なので、実際には何らかの方法で取得する必要がある
                // この例では簡略化のために、再コンパイルは未実装
                auto& state = getOrCreateFunctionState(functionId);
                state.compilationInProgress = false;
            }).detach();
        } else {
            // 同期コンパイル（非実装）
            state.compilationInProgress = false;
        }
    }
}

void JITManager::recordTypeInfo(uint32_t functionId, uint32_t varIndex, const ProfiledTypeInfo& typeInfo) {
    auto& state = getOrCreateFunctionState(functionId);
    
    // プロファイラに型情報を記録
    TypeFeedbackRecord::TypeCategory type;
    switch(typeInfo.expectedType) {
        case ProfiledTypeInfo::ValueType::Int32:
            type = TypeFeedbackRecord::TypeCategory::Integer;
            break;
        case ProfiledTypeInfo::ValueType::Float64:
            type = TypeFeedbackRecord::TypeCategory::Double;
            break;
        case ProfiledTypeInfo::ValueType::String:
            type = TypeFeedbackRecord::TypeCategory::String;
            break;
        case ProfiledTypeInfo::ValueType::Object:
            type = TypeFeedbackRecord::TypeCategory::Object;
            break;
        case ProfiledTypeInfo::ValueType::Boolean:
            type = TypeFeedbackRecord::TypeCategory::Boolean;
            break;
        case ProfiledTypeInfo::ValueType::Undefined:
            type = TypeFeedbackRecord::TypeCategory::Undefined;
            break;
        case ProfiledTypeInfo::ValueType::Null:
            type = TypeFeedbackRecord::TypeCategory::Null;
            break;
        default:
            type = TypeFeedbackRecord::TypeCategory::Mixed;
            break;
    }
    
    m_profiler.RecordTypeObservation(functionId, varIndex, type);
    
    // 変数インデックスが引数の場合
    if (varIndex < state.argTypes.size()) {
        auto& currentType = state.argTypes[varIndex];
        
        // 型に変更があるか確認
        if (currentType.expectedType != typeInfo.expectedType) {
            // 型不安定を記録
            if (!state.hasTypeInstability) {
                state.hasTypeInstability = true;
                
                // 逆最適化カウンターを増やす
                typeInfo.typeCheckFailures++;
                
                // 逆最適化閾値を超えた場合
                if (typeInfo.typeCheckFailures >= m_policy.deoptThreshold && 
                    state.currentTier > JITOptimizationTier::Baseline) {
                    // 逆最適化を実行
                    onDeoptimization(functionId, "型不安定のため");
                }
            }
        }
        
        // 型情報を更新
        state.argTypes[varIndex] = typeInfo;
    } else {
        // ローカル変数の場合
        uint32_t localVarIndex = varIndex - state.argTypes.size();
        
        // 必要に応じてvectorをリサイズ
        if (localVarIndex >= state.varTypes.size()) {
            state.varTypes.resize(localVarIndex + 1);
        }
        
        auto& currentType = state.varTypes[localVarIndex];
        
        // 型に変更があるか確認
        if (currentType.expectedType != typeInfo.expectedType) {
            // 型不安定を記録
            if (!state.hasTypeInstability) {
                state.hasTypeInstability = true;
                
                // 逆最適化閾値を超えた場合
                if (typeInfo.typeCheckFailures >= m_policy.deoptThreshold && 
                    state.currentTier > JITOptimizationTier::Baseline) {
                    // 逆最適化を実行
                    onDeoptimization(functionId, "型不安定のため");
                }
            }
        }
        
        // 型情報を更新
        state.varTypes[localVarIndex] = typeInfo;
    }
}

void JITManager::markTypeStability(uint32_t functionId, bool stable) {
    auto& state = getOrCreateFunctionState(functionId);
    
    // 型安定性フラグを更新
    state.hasTypeInstability = !stable;
    
    // 安定化されたなら、最適化を検討
    if (stable && state.currentTier < JITOptimizationTier::Optimized && 
        state.executionCount >= m_policy.optimizingThreshold) {
        // 再最適化を検討
        JITOptimizationTier targetTier = determineTargetTier(state);
        if (targetTier > state.currentTier) {
            recompileFunction(functionId, targetTier);
        }
    }
}

void JITManager::recompileFunction(uint32_t functionId, JITOptimizationTier targetTier) {
    auto& state = getOrCreateFunctionState(functionId);
    
    // すでに指定レベル以上にコンパイル済みなら何もしない
    if (state.currentTier >= targetTier) {
        return;
    }
    
    // コンパイル中フラグを設定
    state.compilationInProgress = true;
    
    // 実際にはここでバイトコードを取得して再コンパイルを実行
    // （この例では省略）
    
    // コンパイル完了
    state.compilationInProgress = false;
    state.currentTier = targetTier;
    
    // 統計情報更新
    switch (targetTier) {
        case JITOptimizationTier::Baseline:
            m_stats.baselineCompilations++;
            break;
        case JITOptimizationTier::Optimized:
            m_stats.optimizedCompilations++;
            break;
        case JITOptimizationTier::SuperOptimized:
            m_stats.superOptimizedCompilations++;
            break;
        default:
            break;
    }
}

CompiledCodePtr JITManager::getOSREntryPoint(uint32_t functionId, uint32_t bytecodeOffset, uint32_t loopIteration) {
    // OSRエントリーポイントのキーを作成
    std::string osrKey = createOSRKey(functionId, bytecodeOffset);
    
    // キャッシュをチェック
    auto it = m_osrEntryPoints.find(osrKey);
    if (it != m_osrEntryPoints.end()) {
        return it->second;
    }
    
    // ループの反復回数がOSR閾値を超えているか確認
    if (loopIteration < m_policy.osrThreshold) {
        return nullptr;  // 閾値未満なのでOSR実行しない
    }
    
    auto& state = getOrCreateFunctionState(functionId);
    state.osrEntryCount++;
    
    // プロファイラにOSR実行を記録
    m_profiler.RecordExecution(functionId, bytecodeOffset);
    
    // OSR対応コードを生成（実装省略）
    CompiledCodePtr osrCode = nullptr;  // 実際には適切なコードを生成
    
    // キャッシュに保存
    if (osrCode) {
        m_osrEntryPoints[osrKey] = osrCode;
        m_stats.osrEntries++;
    }
    
    return osrCode;
}

void JITManager::setPolicy(const JITOptimizerPolicy& policy) {
    m_policy = policy;
}

std::string JITManager::getCompilationStatistics() const {
    std::stringstream ss;
    ss << "JITコンパイル統計:\n";
    ss << "  ベースラインコンパイル: " << m_stats.baselineCompilations << "\n";
    ss << "  最適化コンパイル: " << m_stats.optimizedCompilations << "\n";
    ss << "  超最適化コンパイル: " << m_stats.superOptimizedCompilations << "\n";
    ss << "  逆最適化: " << m_stats.deoptimizations << "\n";
    ss << "  OSRエントリ: " << m_stats.osrEntries << "\n";
    
    // プロファイラ統計も追加
    ss << "\n" << m_profiler.GetProfileSummary();
    
    return ss.str();
}

JITOptimizationTier JITManager::determineTargetTier(const FunctionCompilationState& state) const {
    // 型不安定な関数はベースラインに留める
    if (state.hasTypeInstability && !m_policy.enableSpeculativeOptimization) {
        return JITOptimizationTier::Baseline;
    }
    
    // 実行回数に基づく階層決定
    if (state.executionCount >= m_policy.superOptimizingThreshold) {
        return JITOptimizationTier::SuperOptimized;
    } else if (state.executionCount >= m_policy.optimizingThreshold) {
        return JITOptimizationTier::Optimized;
    } else if (state.executionCount >= m_policy.baselineThreshold) {
        return JITOptimizationTier::Baseline;
    }
    
    return JITOptimizationTier::None;
}

CompiledCodePtr JITManager::compileFunction(uint32_t functionId, const std::vector<Bytecode>& bytecodes, JITOptimizationTier targetTier) {
    CompiledCodePtr result = nullptr;
    
    // 関数の状態を取得
    auto& state = getOrCreateFunctionState(functionId);
    
    // コンパイル中フラグを設定
    state.compilationInProgress = true;
    
    // バイトコードをuint8_tに変換
    std::vector<uint8_t> rawBytecodes;
    for (const auto& bytecode : bytecodes) {
        // バイトコードのシリアライズロジック（実際にはもっと複雑）
        rawBytecodes.push_back(static_cast<uint8_t>(bytecode.opcode));
        for (const auto& operand : bytecode.operands) {
            // 単純化のため、operandを8ビットずつ分解
            rawBytecodes.push_back((operand >> 24) & 0xFF);
            rawBytecodes.push_back((operand >> 16) & 0xFF);
            rawBytecodes.push_back((operand >> 8) & 0xFF);
            rawBytecodes.push_back(operand & 0xFF);
        }
    }
    
    // コンパイル開始前にプロファイラに関数を登録
    m_profiler.RegisterFunction(functionId, rawBytecodes.size());
    
    size_t codeSize = 0;
    
    // 各階層に応じたコンパイラを選択
    switch (targetTier) {
        case JITOptimizationTier::Baseline:
            m_baselineJIT->SetFunctionId(functionId);
            result = m_baselineJIT->Compile(rawBytecodes, codeSize);
            m_stats.baselineCompilations++;
            break;
            
        case JITOptimizationTier::Optimized:
            // プロファイル情報を取得して最適化JITに渡す
            {
                // 関数のプロファイル情報を取得
                const FunctionProfile* profile = m_profiler.GetFunctionProfile(functionId);
                
                if (profile) {
                    // 最適化オプションを設定
                    OptimizingJIT::CompileOptions options;
                    options.profileData = profile;
                    options.enableSpeculation = m_policy.enableSpeculativeOptimization;
                    options.enableInlining = m_policy.enableInlining;
                    
                    result = m_optimizingJIT->CompileWithOptions(rawBytecodes, options, codeSize);
                    m_stats.optimizedCompilations++;
                } else {
                    // プロファイルが無い場合はベースラインJITにフォールバック
                    m_baselineJIT->SetFunctionId(functionId);
                    result = m_baselineJIT->Compile(rawBytecodes, codeSize);
                    m_stats.baselineCompilations++;
                    targetTier = JITOptimizationTier::Baseline;
                }
            }
            break;
            
        case JITOptimizationTier::SuperOptimized:
            // 超最適化JITは全ての最適化を有効化
            {
                // 関数のプロファイル情報を取得
                const FunctionProfile* profile = m_profiler.GetFunctionProfile(functionId);
                
                if (profile && profile->executionCount > m_policy.superOptimizingThreshold) {
                    // 最適化オプションを設定
                    SuperOptimizingJIT::CompileOptions options;
                    options.profileData = profile;
                    options.optimizationLevel = SuperOptimizingJIT::OptimizationLevel::Maximum;
                    
                    result = m_superOptimizingJIT->CompileWithOptions(rawBytecodes, options, codeSize);
                    m_stats.superOptimizedCompilations++;
                } else {
                    // 条件を満たさない場合は最適化JITにフォールバック
                    OptimizingJIT::CompileOptions options;
                    options.profileData = profile;
                    options.enableSpeculation = m_policy.enableSpeculativeOptimization;
                    options.enableInlining = m_policy.enableInlining;
                    
                    result = m_optimizingJIT->CompileWithOptions(rawBytecodes, options, codeSize);
                    m_stats.optimizedCompilations++;
                    targetTier = JITOptimizationTier::Optimized;
                }
            }
            break;
            
        default:
            // インタプリタモードの場合はコンパイルしない
            break;
    }
    
    // コンパイル完了
    state.compilationInProgress = false;
    
    // 状態を更新
    if (result) {
        state.currentTier = targetTier;
        m_compiledFunctions[functionId] = result;
    }
    
    return result;
}

FunctionCompilationState& JITManager::getOrCreateFunctionState(uint32_t functionId) {
    auto it = m_functionStates.find(functionId);
    if (it != m_functionStates.end()) {
        return it->second;
    }
    
    // 新しい状態を作成
    return m_functionStates.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(functionId),
        std::forward_as_tuple(functionId)
    ).first->second;
}

void JITManager::onDeoptimization(uint32_t functionId, const std::string& reason) {
    auto& state = getOrCreateFunctionState(functionId);
    
    // 統計情報更新
    m_stats.deoptimizations++;
    
    // ベースラインJITに逆最適化
    state.currentTier = JITOptimizationTier::Baseline;
    
    // プロファイラに通知
    m_profiler.RecordDeoptimization(functionId, 0, reason);
    
    // バイトコードが必要なので、実際にはここでバイトコードを取得して再コンパイル
    // （この例では省略）
}

} // namespace aerojs::core 