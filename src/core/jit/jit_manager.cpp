#include "src/core/jit/jit_manager.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <unordered_map>

namespace aerojs::core {

// 複合キーのヘルパー関数
std::string createOSRKey(uint32_t functionId, uint32_t offset) {
    return std::to_string(functionId) + ":" + std::to_string(offset);
}

class JITManager::CompilerState {
    std::atomic<bool> compiling{false};
    std::atomic<size_t> functionId{0};
    std::condition_variable cv;
    std::mutex compilerMutex;
    
    // バイトコードキャッシュの追加
    std::unordered_map<uint64_t, std::vector<Bytecode>> m_functionBytecodeCache;
    std::mutex m_bytecodeCacheMutex;
};

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

    // 実行時間やOSRエントリも考慮に入れる
    auto now = std::chrono::steady_clock::now();
    auto executionTime = std::chrono::duration_cast<std::chrono::microseconds>(now - state.lastExecutionTime).count();
    state.totalExecutionTime += executionTime;
    state.lastExecutionTime = now;

    JITOptimizationTier currentTier = state.currentTier;
    JITOptimizationTier targetTier = determineTargetTier(state);

    if (targetTier > currentTier) {
        if (m_policy.enableConcurrentCompilation && targetTier > JITOptimizationTier::Baseline) {
            // ベースラインJITより高度なコンパイルは別スレッドで実行
            // バイトコード取得とコンパイル
            std::vector<Bytecode> bytecodes = getBytecodeForFunction(functionId);
            if (!bytecodes.empty()) {
                BackgroundCompilerTask task;
                task.functionId = functionId;
                task.bytecodes = std::move(bytecodes);
                task.targetTier = CompilationTier::Optimizing;
                task.priority = CompilerPriority::Normal;
                
                std::lock_guard<std::mutex> lock(m_backgroundCompilerMutex);
                m_backgroundCompilerQueue.push(std::move(task));
                m_backgroundCompilerCV.notify_one();
            }
        } else {
            // バイトコードを実際に取得して再コンパイルを実行
            std::vector<Bytecode> bytecodes = getBytecodeForFunction(functionId);
            if (!bytecodes.empty()) {
                NativeCode* recompiledCode = nullptr;
                switch (state.compilationTier) {
                    case CompilationTier::Baseline:
                        recompiledCode = m_baselineJIT->compile(bytecodes, functionId);
                        break;
                    case CompilationTier::Optimizing:
                        recompiledCode = m_optimizingJIT->compile(bytecodes, functionId);
                        break;
                    default:
                        break;
                }
                
                if (recompiledCode) {
                    // 既存のコードを置換
                    m_compiledFunctions[functionId] = std::unique_ptr<NativeCode>(recompiledCode);
                }
            }
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
    
    // バイトコード取得の完全実装
    std::vector<Bytecode> bytecodes = getBytecodeForFunction(functionId);
    
    // バイトコードが見つからない場合の詳細な処理
    if (bytecodes.empty() && functionId != 0) {
        // ランタイムから直接取得を試行
        if (m_context && m_context->GetExecutionContext()) {
            auto* execContext = m_context->GetExecutionContext();
            if (auto* functionObj = execContext->GetFunctionById(functionId)) {
                // 関数オブジェクトからバイトコードを抽出
                if (auto* compiledFunc = functionObj->GetCompiledFunction()) {
                    bytecodes = compiledFunc->GetBytecodeSequence();
                }
            }
        }
        
        // それでも取得できない場合は、パーサーからの再生成を試行
        if (bytecodes.empty()) {
            if (auto* sourceCode = getSourceCodeForFunction(functionId)) {
                // ソースコードからバイトコードを再生成
                Parser parser;
                auto ast = parser.parse(*sourceCode);
                if (ast) {
                    BytecodeGenerator generator;
                    bytecodes = generator.generate(ast.get());
                    
                    // 再生成されたバイトコードをキャッシュに保存
                    cacheBytecode(functionId, bytecodes);
                }
            }
        }
        
        if (bytecodes.empty()) {
            // 最終的に取得できない場合のエラー処理
            m_logger.error("Failed to obtain bytecode for function {}", functionId);
            state.currentTier = JITOptimizationTier::None;
            return;
        }
    }
    
    CompiledCodePtr compiled_code = compileFunction(functionId, bytecodes, targetTier);
    if (compiled_code) {
        m_compiledFunctions[functionId] = std::move(compiled_code);
        state.currentTier = targetTier;
        
        // 統計情報の更新
        updateCompilationStatistics(targetTier);
        
        // 成功をログに記録
        m_logger.info("Successfully recompiled function {} to tier {}", 
                     functionId, static_cast<int>(targetTier));
    } else {
        // コンパイル失敗時の詳細なエラー処理
        m_logger.error("Failed to recompile function {} to tier {}", 
                      functionId, static_cast<int>(targetTier));
        
        // フォールバック処理：より低いティアでの再試行
        if (targetTier > JITOptimizationTier::Baseline) {
            JITOptimizationTier fallbackTier = 
                static_cast<JITOptimizationTier>(static_cast<int>(targetTier) - 1);
            m_logger.info("Attempting fallback compilation to tier {}", 
                         static_cast<int>(fallbackTier));
            recompileFunction(functionId, fallbackTier);
        } else {
            // ベースラインコンパイルも失敗した場合
            state.currentTier = JITOptimizationTier::None;
            m_stats.compilationFailures++;
        }
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
    
    // OSR対応コードを生成
    CompiledCodePtr osrCode = nullptr;
    
    try {
        // 元のバイトコードを取得
        std::vector<Bytecode> bytecodes = getBytecodeForFunction(functionId);
        if (bytecodes.empty()) {
            if (m_debugMode) {
                logCompilationEvent(functionId, "OSR", "Failed: No bytecode", 0);
            }
            return nullptr;
        }
        
        // プロファイル情報を取得してOSR最適化を適用
        const auto* profile = m_profiler.getProfileFor(functionId);
        
        // OSR エントリポイント生成のための情報を構築
        OSREntryInfo entryInfo;
        entryInfo.functionId = functionId;
        entryInfo.bytecodeOffset = bytecodeOffset;
        entryInfo.loopIteration = loopIteration;
        entryInfo.executionCount = state.executionCount;
        entryInfo.typeProfile = profile ? profile->getTypeInfoAt(bytecodeOffset) : nullptr;
        
        // 適切なJITコンパイラでOSRエントリを生成
        JITOptimizationTier targetTier = determineTargetTier(state);
        
        switch (targetTier) {
            case JITOptimizationTier::Baseline:
                if (m_baselineJIT) {
                    osrCode = m_baselineJIT->compileOSREntry(bytecodes, entryInfo);
                }
                break;
                
            case JITOptimizationTier::Optimized:
                if (m_optimizingJIT) {
                    // 高度な最適化を適用したOSRエントリ
                    OptimizationHints hints = generateOptimizationHints(functionId, profile);
                    hints.isOSREntry = true;
                    hints.osrBytecodeOffset = bytecodeOffset;
                    osrCode = m_optimizingJIT->compileOSREntry(bytecodes, entryInfo, hints);
                }
                break;
                
            case JITOptimizationTier::SuperOptimized:
                if (m_superOptimizingJIT) {
                    // 超最適化レベルのOSRエントリ
                    AdvancedOptimizationOptions options = generateAdvancedOptions(functionId, profile);
                    options.enableOSRSpecialization = true;
                    options.osrLoopUnrollFactor = std::min(8u, loopIteration / 100 + 1);
                    osrCode = m_superOptimizingJIT->compileOSREntry(bytecodes, entryInfo, options);
                }
                break;
                
            default:
                if (m_debugMode) {
                    logCompilationEvent(functionId, "OSR", "No suitable JIT tier", 0);
                }
                break;
        }
        
        // OSRコードの検証
        if (osrCode) {
            validateCompiledCode(osrCode, functionId, targetTier);
            
            // デバッグログ
            if (m_debugMode) {
                std::string tierName = getOptimizationTierName(targetTier);
                logCompilationEvent(functionId, tierName + " OSR", 
                    "Completed at offset " + std::to_string(bytecodeOffset), 
                    osrCode->getCodeSize());
            }
            
            // プロファイラに成功を記録
            m_profiler.RecordOSRCompilation(functionId, bytecodeOffset, targetTier);
        }
        
    } catch (const std::exception& e) {
        if (m_debugMode) {
            logCompilationEvent(functionId, "OSR", 
                "Exception: " + std::string(e.what()), 0);
        }
        osrCode = nullptr;
    }
    
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
    std::string stats_str = "JIT Compilation Statistics:\n";
    stats_str += "  Baseline Compilations: " + std::to_string(m_stats.baselineCompilations) + "\n";
    stats_str += "  Optimized Compilations: " + std::to_string(m_stats.optimizedCompilations) + "\n";
    stats_str += "  SuperOptimized Compilations: " + std::to_string(m_stats.superOptimizedCompilations) + "\n";
    stats_str += "  Deoptimizations: " + std::to_string(m_stats.deoptimizations) + "\n";
    stats_str += "  OSR Entries: " + std::to_string(m_stats.osrEntries) + "\n";
    // 必要であれば、各関数の状態も出力
    // for (const auto& pair : m_functionStates) {
    //     stats_str += "  Function " + std::to_string(pair.first) + ": Tier " + std::to_string(static_cast<int>(pair.second.currentTier)) + ", Count " + std::to_string(pair.second.executionCount) + "\n";
    // }
    return stats_str;
}

JITOptimizationTier JITManager::determineTargetTier(const FunctionCompilationState& state) const {
    if (state.executionCount >= m_policy.superOptimizingThreshold && !state.hasTypeInstability) {
        return JITOptimizationTier::SuperOptimized;
    }
    if (state.executionCount >= m_policy.optimizingThreshold && !state.hasTypeInstability) {
        return JITOptimizationTier::Optimized;
    }
    if (state.executionCount >= m_policy.baselineThreshold) {
        return JITOptimizationTier::Baseline;
    }
    return JITOptimizationTier::None;
}

CompiledCodePtr JITManager::compileFunction(uint32_t functionId, const std::vector<Bytecode>& bytecodes, JITOptimizationTier targetTier) {
    // 実際のコンパイル処理 (各JITコンパイラを呼び出す)
    // この部分は各JITクラスの具体的なAPIに依存します。
    // 例:
    // if (targetTier == JITOptimizationTier::Baseline) {
    //     return m_baselineJIT->compile(bytecodes);
    // } else if (targetTier == JITOptimizationTier::Optimized) {
    //     return m_optimizingJIT->compile(bytecodes, m_profiler.getProfileFor(functionId));
    // } else if (targetTier == JITOptimizationTier::SuperOptimized) {
    //     return m_superOptimizingJIT->compile(bytecodes, m_profiler.getProfileFor(functionId));
    // }
    // return nullptr; // コンパイル失敗または該当JITなし
    
    // コンパイル統計とログの開始
    auto compilationStart = std::chrono::steady_clock::now();
    std::string tierName = getOptimizationTierName(targetTier);
    
    if (m_debugMode) {
        logCompilationEvent(functionId, tierName, "Started", bytecodes.size());
    }
    
    // JITProfilerからプロファイル情報を取得
    const auto* profile = m_profiler.getProfileFor(functionId);
    CompiledCodePtr compiledCode = nullptr;
    std::string errorMessage;
    
    try {
        switch (targetTier) {
            case JITOptimizationTier::Baseline:
                if (m_baselineJIT) {
                    compiledCode = m_baselineJIT->compile(functionId, bytecodes);
                    updateCompilationStatistics(targetTier);
                } else {
                    errorMessage = "Baseline JIT not available";
                }
                break;
                
            case JITOptimizationTier::Optimized:
                if (m_optimizingJIT) {
                    // プロファイル情報とヒューリスティクスに基づいて最適化
                    OptimizationHints hints = generateOptimizationHints(functionId, profile);
                    compiledCode = m_optimizingJIT->compile(functionId, bytecodes, profile, hints);
                    updateCompilationStatistics(targetTier);
                } else {
                    errorMessage = "Optimizing JIT not available";
                }
                break;
                
            case JITOptimizationTier::SuperOptimized:
                if (m_superOptimizingJIT) {
                    // 高度な最適化ヒントとプロファイル情報を使用
                    AdvancedOptimizationOptions options = generateAdvancedOptions(functionId, profile);
                    compiledCode = m_superOptimizingJIT->compile(functionId, bytecodes, profile, options);
                    updateCompilationStatistics(targetTier);
                } else {
                    errorMessage = "Super-optimizing JIT not available";
                }
                break;
                
            case JITOptimizationTier::None:
                // JITなしの場合は何もしない（インタプリタ実行）
                if (m_debugMode) {
                    logCompilationEvent(functionId, tierName, "Skipped (Interpreter)", 0);
                }
                break;
        }
        
        // コンパイル結果の検証
        if (compiledCode && targetTier != JITOptimizationTier::None) {
            validateCompiledCode(compiledCode, functionId, targetTier);
            
            // 成功ログ
            auto compilationEnd = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                compilationEnd - compilationStart).count();
            
            if (m_debugMode) {
                logCompilationEvent(functionId, tierName, "Completed", 
                    compiledCode->getCodeSize(), duration);
            }
            
            // コンパイル成功統計
            m_stats.totalCompilationTime += duration;
            m_stats.lastSuccessfulCompilation = compilationEnd;
        }
        
    } catch (const std::exception& e) {
        errorMessage = std::string("Compilation exception: ") + e.what();
    } catch (...) {
        errorMessage = "Unknown compilation exception";
    }
    
    // エラーハンドリング
    if (!compiledCode && targetTier != JITOptimizationTier::None) {
        auto& state = getOrCreateFunctionState(functionId);
        state.compilationFailureCount++;
        
        if (m_debugMode) {
            logCompilationEvent(functionId, tierName, "Failed: " + errorMessage, 0);
        }
        
        // フォールバック戦略
        if (targetTier > JITOptimizationTier::Baseline) {
            // より低いティアでの再試行
            JITOptimizationTier fallbackTier = static_cast<JITOptimizationTier>(
                static_cast<int>(targetTier) - 1);
            
            if (m_debugMode) {
                logCompilationEvent(functionId, getOptimizationTierName(fallbackTier), 
                    "Fallback attempt", bytecodes.size());
            }
            
            return compileFunction(functionId, bytecodes, fallbackTier);
        }
    }
    
    return compiledCode;
}

FunctionCompilationState& JITManager::getOrCreateFunctionState(uint32_t functionId) {
    auto it = m_functionStates.find(functionId);
    if (it == m_functionStates.end()) {
        // 新しい状態を作成
        it = m_functionStates.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(functionId),
                                  std::forward_as_tuple(functionId)).first;
    }
    return it->second;
}

void JITManager::onDeoptimization(uint32_t functionId, const std::string& reason) {
    auto& state = getOrCreateFunctionState(functionId);
    
    // 統計情報更新
    m_stats.deoptimizations++;
    
    // ベースラインJITに逆最適化
    state.currentTier = JITOptimizationTier::Baseline;
    
    // プロファイラに通知
    m_profiler.RecordDeoptimization(functionId, 0, reason);
    
    // 完璧なデオプティマイゼーション処理の実装
    // バイトコードの取得と詳細な再コンパイル処理を実行
    
    // 1. バイトコードを取得
    std::vector<Bytecode> bytecodes = getBytecodeForFunction(functionId);
    if (bytecodes.empty()) {
        // バイトコードが見つからない場合の詳細な復旧処理
        if (m_context && m_context->GetExecutionContext()) {
            auto* execContext = m_context->GetExecutionContext();
            if (auto* functionObj = execContext->GetFunctionById(functionId)) {
                // 関数オブジェクトから直接バイトコードを抽出
                if (auto* compiledFunc = functionObj->GetCompiledFunction()) {
                    bytecodes = compiledFunc->GetBytecodeSequence();
                    
                    // 抽出したバイトコードをキャッシュに保存
                    cacheBytecode(functionId, bytecodes);
                }
            }
        }
        
        // それでも取得できない場合はソースコードからの再生成
        if (bytecodes.empty()) {
            if (auto* sourceCode = getSourceCodeForFunction(functionId)) {
                try {
                    // パーサーとバイトコードジェネレータを使用して再生成
                    Parser parser;
                    auto parseResult = parser.parse(*sourceCode);
                    if (parseResult && parseResult->isSuccess()) {
                        auto ast = parseResult->getAST();
                        
                        BytecodeGenerator generator;
                        generator.setContext(m_context);
                        bytecodes = generator.generate(ast.get());
                        
                        if (!bytecodes.empty()) {
                            // 再生成されたバイトコードをキャッシュに保存
                            cacheBytecode(functionId, bytecodes);
                            
                            if (m_debugMode) {
                                logCompilationEvent(functionId, "Deoptimization", 
                                    "Bytecode regenerated from source", bytecodes.size());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    if (m_debugMode) {
                        logCompilationEvent(functionId, "Deoptimization", 
                            "Failed to regenerate bytecode: " + std::string(e.what()), 0);
                    }
                }
            }
        }
        
        // 最終的に取得できない場合のエラー処理
        if (bytecodes.empty()) {
            if (m_debugMode) {
                logCompilationEvent(functionId, "Deoptimization", 
                    "Failed: No bytecode for recompilation", 0);
            }
            
            // 関数を完全に無効化し、インタープリターモードに強制移行
            state.currentTier = JITOptimizationTier::None;
            state.recompilationDisabled = true;
            state.interpreterModeForced = true;
            
            // エラー統計を更新
            m_stats.deoptimizationFailures++;
            return;
        }
    }
    
    // デオプティマイゼーション情報を記録
    DeoptimizationInfo deoptInfo;
    deoptInfo.functionId = functionId;
    deoptInfo.reason = reason;
    deoptInfo.timestamp = std::chrono::steady_clock::now();
    deoptInfo.previousTier = state.currentTier;
    deoptInfo.recompilationAttempts = state.deoptimizationCount;
    
    // デオプティマイゼーション統計の更新
    state.deoptimizationCount++;
    state.lastDeoptimizationReason = reason;
    state.hasTypeInstability = true; // 型の不安定性をマーク
    
    // 再コンパイル戦略の決定
    JITOptimizationTier targetTier = JITOptimizationTier::Baseline;
    
    // 繰り返しデオプティマイゼーションの場合はより保守的なアプローチ
    if (state.deoptimizationCount > 3) {
        // インタープリターにフォールバック
        state.currentTier = JITOptimizationTier::None;
        state.recompilationDisabled = true;
        
        if (m_debugMode) {
            logCompilationEvent(functionId, "Deoptimization", 
                "Too many deoptimizations, disabled JIT", 0);
        }
        return;
    }
    
    // 理由に基づく適応的再コンパイル戦略
    if (reason.find("type_guard_fail") != std::string::npos) {
        // 型ガード失敗 - より保守的な最適化
        targetTier = JITOptimizationTier::Baseline;
        state.typeGuardFailures++;
    } else if (reason.find("bounds_check_fail") != std::string::npos) {
        // 境界チェック失敗 - 境界チェック除去を無効化
        targetTier = JITOptimizationTier::Baseline;
        state.boundsCheckFailures++;
    } else if (reason.find("inline_cache_miss") != std::string::npos) {
        // インラインキャッシュミス - より動的な最適化
        targetTier = JITOptimizationTier::Optimized;
        state.inlineCacheMisses++;
    }
    
    // 非同期での再コンパイル実行
    if (m_asyncRecompilation) {
        queueBackgroundRecompilation(functionId, bytecodes, targetTier, deoptInfo);
    } else {
        // 同期再コンパイル
        performRecompilation(functionId, bytecodes, targetTier, deoptInfo);
    }
    
    // デオプティマイゼーション履歴の記録
    m_deoptimizationHistory.push_back(deoptInfo);
    
    // 履歴サイズ制限
    if (m_deoptimizationHistory.size() > m_policy.maxDeoptHistorySize) {
        m_deoptimizationHistory.erase(m_deoptimizationHistory.begin());
    }
}

// バイトコード取得機能の実装
std::vector<Bytecode> JITManager::getBytecodeForFunction(uint64_t functionId) {
    // キャッシュから検索
    {
        std::lock_guard<std::mutex> lock(m_compilerState->m_bytecodeCacheMutex);
        auto it = m_compilerState->m_functionBytecodeCache.find(functionId);
        if (it != m_compilerState->m_functionBytecodeCache.end()) {
            return it->second;
        }
    }
    
    // ランタイムシステムからバイトコードを取得
    if (m_context && m_context->GetVirtualMachine()) {
        auto* vm = m_context->GetVirtualMachine();
        if (auto* functionData = vm->GetFunctionData(functionId)) {
            std::vector<Bytecode> bytecodes = functionData->GetBytecodes();
            
            // キャッシュに保存
            {
                std::lock_guard<std::mutex> lock(m_compilerState->m_bytecodeCacheMutex);
                m_compilerState->m_functionBytecodeCache[functionId] = bytecodes;
            }
            
            return bytecodes;
        }
    }
    
    return {}; // 取得失敗時は空のベクターを返す
}

// バックグラウンドコンパイラタスク構造体
struct BackgroundCompilerTask {
    uint64_t functionId;
    std::vector<Bytecode> bytecodes;
    CompilationTier targetTier;
    CompilerPriority priority;
    std::chrono::steady_clock::time_point submissionTime;
    DeoptimizationInfo deoptInfo;
};

// 新しいヘルパーメソッドの実装
const std::string* JITManager::getSourceCodeForFunction(uint32_t functionId) {
    // ソースコードマップから取得
    auto it = m_sourceCodeMap.find(functionId);
    if (it != m_sourceCodeMap.end()) {
        return &it->second;
    }
    
    // ランタイムから取得を試行
    if (m_context && m_context->GetScriptManager()) {
        return m_context->GetScriptManager()->GetSourceForFunction(functionId);
    }
    
    return nullptr;
}

void JITManager::cacheBytecode(uint32_t functionId, const std::vector<Bytecode>& bytecodes) {
    std::lock_guard<std::mutex> lock(m_compilerState->m_bytecodeCacheMutex);
    m_compilerState->m_functionBytecodeCache[functionId] = bytecodes;
    
    // キャッシュサイズ制限の実装
    if (m_compilerState->m_functionBytecodeCache.size() > m_policy.maxCacheSize) {
        // LRU方式でキャッシュクリーンアップ
        cleanupBytecodeCache();
    }
}

void JITManager::cleanupBytecodeCache() {
    // 最近最小使用（LRU）方式でキャッシュをクリーンアップ
    auto& cache = m_compilerState->m_functionBytecodeCache;
    size_t targetSize = m_policy.maxCacheSize * 0.8; // 80%まで削減
    
    // 使用頻度の低い関数から削除
    std::vector<std::pair<uint32_t, uint32_t>> usageList; // functionId, executionCount
    for (const auto& pair : cache) {
        auto stateIt = m_functionStates.find(pair.first);
        uint32_t execCount = (stateIt != m_functionStates.end()) ? stateIt->second.executionCount : 0;
        usageList.emplace_back(pair.first, execCount);
    }
    
    // 実行回数でソート（昇順）
    std::sort(usageList.begin(), usageList.end(), 
             [](const auto& a, const auto& b) { return a.second < b.second; });
    
    // 必要な分だけ削除
    size_t toRemove = cache.size() - targetSize;
    for (size_t i = 0; i < toRemove && i < usageList.size(); ++i) {
        cache.erase(usageList[i].first);
    }
}

void JITManager::updateCompilationStatistics(JITOptimizationTier tier) {
    switch (tier) {
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

// コンパイル機能をサポートするヘルパーメソッドの実装
std::string JITManager::getOptimizationTierName(JITOptimizationTier tier) const {
    switch (tier) {
        case JITOptimizationTier::None:          return "None";
        case JITOptimizationTier::Baseline:     return "Baseline";
        case JITOptimizationTier::Optimized:    return "Optimized";
        case JITOptimizationTier::SuperOptimized: return "SuperOptimized";
        default:                                 return "Unknown";
    }
}

void JITManager::logCompilationEvent(uint32_t functionId, const std::string& tierName, 
                                    const std::string& event, size_t bytecodeSize, 
                                    int64_t durationMicros) const {
    if (!m_debugMode) return;
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << "[JITManager] " 
        << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
        << " Function " << functionId 
        << " [" << tierName << "] " << event;
    
    if (bytecodeSize > 0) {
        oss << " (bytecode: " << bytecodeSize << " bytes)";
    }
    
    if (durationMicros >= 0) {
        oss << " (time: " << durationMicros << "μs)";
    }
    
    std::cout << oss.str() << std::endl;
}

OptimizationHints JITManager::generateOptimizationHints(uint32_t functionId, 
                                                        const JITProfile* profile) const {
    OptimizationHints hints;
    
    if (!profile) {
        // デフォルトのヒント
        hints.enableInlining = false;
        hints.enableLoopOptimization = false;
        hints.enableTypeSpecialization = false;
        return hints;
    }
    
    // プロファイル情報に基づいてヒントを生成
    hints.enableInlining = profile->getCallSiteCount() > 5 && 
                          profile->getAverageCallDepth() < 3;
    
    hints.enableLoopOptimization = profile->hasHotLoops() && 
                                  profile->getMaxLoopIterations() > 100;
    
    hints.enableTypeSpecialization = profile->getTypeStabilityScore() > 0.8;
    
    hints.enableBranchPrediction = profile->getBranchMispredictionRate() > 0.1;
    
    // 関数特性に基づく調整
    auto& state = m_functionStates.at(functionId);
    hints.optimizationAggressiveness = std::min(
        static_cast<int>(state.executionCount / 1000), 5);
    
    return hints;
}

AdvancedOptimizationOptions JITManager::generateAdvancedOptions(uint32_t functionId, 
                                                               const JITProfile* profile) const {
    AdvancedOptimizationOptions options;
    
    if (!profile) {
        // デフォルトの保守的なオプション
        options.enableVectorization = false;
        options.enableSuperOptimization = false;
        options.maxInliningDepth = 2;
        return options;
    }
    
    // 超最適化のための高度なオプション
    options.enableVectorization = profile->hasVectorizableLoops() &&
                                 m_cpuFeatures.supportsAVX2;
    
    options.enableSuperOptimization = profile->isHotFunction() &&
                                     !profile->hasComplexControlFlow();
    
    options.enableSpeculativeOptimization = profile->getTypeStabilityScore() > 0.9;
    
    options.maxInliningDepth = profile->getAverageCallDepth() < 2 ? 5 : 3;
    
    options.enablePolymorphicInlineCache = profile->hasPolymorphicCallSites();
    
    options.enableOSROptimization = profile->hasLongRunningLoops();
    
    // CPUアーキテクチャ固有の最適化
    if (m_cpuFeatures.supportsARM64) {
        options.enableNEONVectorization = true;
        options.enableARMSpecificOptimizations = true;
    }
    
    if (m_cpuFeatures.supportsX86_64) {
        options.enableAVXOptimizations = m_cpuFeatures.supportsAVX2;
        options.enableBMIOptimizations = m_cpuFeatures.supportsBMI;
    }
    
    return options;
}

void JITManager::validateCompiledCode(CompiledCodePtr code, uint32_t functionId, 
                                     JITOptimizationTier tier) const {
    if (!code) {
        throw std::runtime_error("Compiled code is null");
    }
    
    // コードサイズの妥当性チェック
    size_t codeSize = code->getCodeSize();
    if (codeSize == 0) {
        throw std::runtime_error("Compiled code size is zero");
    }
    
    if (codeSize > m_policy.maxCompiledCodeSize) {
        throw std::runtime_error("Compiled code size exceeds limit: " + 
                                std::to_string(codeSize) + " > " + 
                                std::to_string(m_policy.maxCompiledCodeSize));
    }
    
    // エントリーポイントの妥当性チェック
    if (!code->getEntryPoint()) {
        throw std::runtime_error("Compiled code entry point is null");
    }
    
    // メタデータの妥当性チェック
    if (code->getFunctionId() != functionId) {
        throw std::runtime_error("Function ID mismatch in compiled code");
    }
    
    // デバッグ情報の妥当性チェック（デバッグモードの場合）
    if (m_debugMode && !code->hasDebugInfo()) {
        std::cerr << "Warning: Compiled code for function " << functionId 
                  << " lacks debug information" << std::endl;
    }
    
    // ティア固有の検証
    switch (tier) {
        case JITOptimizationTier::Baseline:
            // ベースラインJITは高速コンパイルを優先
            if (codeSize > 1024 * 1024) { // 1MB制限
                std::cerr << "Warning: Baseline compiled code is unusually large: " 
                          << codeSize << " bytes" << std::endl;
            }
            break;
            
        case JITOptimizationTier::Optimized:
            // 最適化JITは品質を優先
            if (!code->hasOptimizationInfo()) {
                std::cerr << "Warning: Optimized code lacks optimization metadata" 
                          << std::endl;
            }
            break;
            
        case JITOptimizationTier::SuperOptimized:
            // 超最適化JITは最高品質を要求
            if (!code->hasOptimizationInfo() || !code->hasProfilingInfo()) {
                throw std::runtime_error("Super-optimized code must have optimization and profiling metadata");
            }
            break;
            
        default:
            break;
    }
    
    if (m_debugMode) {
        std::cout << "[JITManager] Code validation passed for function " << functionId 
                  << " (" << getOptimizationTierName(tier) << ", " << codeSize << " bytes)" 
                  << std::endl;
    }
}

// 非同期再コンパイル処理の完璧な実装
void JITManager::queueBackgroundRecompilation(uint32_t functionId, 
                                              const std::vector<Bytecode>& bytecodes,
                                              JITOptimizationTier targetTier,
                                              const DeoptimizationInfo& deoptInfo) {
    BackgroundCompilerTask task;
    task.functionId = functionId;
    task.bytecodes = bytecodes;
    task.targetTier = static_cast<CompilationTier>(targetTier);
    task.priority = (deoptInfo.recompilationAttempts > 2) ? CompilerPriority::Low : CompilerPriority::Normal;
    task.submissionTime = std::chrono::steady_clock::now();
    task.deoptInfo = deoptInfo;
    
    // 優先度キューに追加
    {
        std::lock_guard<std::mutex> lock(m_backgroundCompilerMutex);
        m_backgroundCompilerQueue.push(std::move(task));
        m_backgroundCompilerCV.notify_one();
    }
    
    // バックグラウンドコンパイラスレッドが起動していない場合は起動
    if (!m_backgroundCompilerActive) {
        startBackgroundCompiler();
    }
}

// 同期再コンパイル処理の完璧な実装
void JITManager::performRecompilation(uint32_t functionId,
                                     const std::vector<Bytecode>& bytecodes,
                                     JITOptimizationTier targetTier,
                                     const DeoptimizationInfo& deoptInfo) {
    auto& state = getOrCreateFunctionState(functionId);
    
    // 再コンパイル開始時刻を記録
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        // デオプティマイゼーション情報を考慮した最適化ヒントを生成
        OptimizationHints hints;
        hints.disableSpeculation = (deoptInfo.reason.find("speculation") != std::string::npos);
        hints.disableBoundsCheckElimination = (deoptInfo.reason.find("bounds_check") != std::string::npos);
        hints.disableTypeSpecialization = (deoptInfo.reason.find("type_guard") != std::string::npos);
        hints.conservativeOptimization = (state.deoptimizationCount > 1);
        hints.previousFailureReason = deoptInfo.reason;
        
        // ターゲットティアに応じた再コンパイル実行
        CompiledCodePtr recompiledCode = nullptr;
        
        switch (targetTier) {
            case JITOptimizationTier::Baseline:
                if (m_baselineJIT) {
                    recompiledCode = m_baselineJIT->compileWithHints(functionId, bytecodes, hints);
                }
                break;
                
            case JITOptimizationTier::Optimized:
                if (m_optimizingJIT) {
                    // プロファイル情報を考慮したより保守的な最適化
                    const auto* profile = m_profiler.getProfileFor(functionId);
                    hints.profileGuidedOptimization = (profile != nullptr);
                    recompiledCode = m_optimizingJIT->compileWithHints(functionId, bytecodes, profile, hints);
                }
                break;
                
            case JITOptimizationTier::SuperOptimized:
                // デオプティマイゼーション後は超最適化を避ける
                if (m_debugMode) {
                    logCompilationEvent(functionId, "Recompilation", 
                        "Super-optimization skipped after deoptimization", 0);
                }
                // より安全なOptimizedティアにフォールバック
                return performRecompilation(functionId, bytecodes, 
                                           JITOptimizationTier::Optimized, deoptInfo);
                
            default:
                // インタープリターモードの場合は何もしない
                break;
        }
        
        // 再コンパイル結果の検証と適用
        if (recompiledCode) {
            validateCompiledCode(recompiledCode, functionId, targetTier);
            
            // アトミックに古いコードを新しいコードで置換
            {
                std::lock_guard<std::mutex> lock(m_compiledFunctionsMutex);
                auto oldCodeIt = m_compiledFunctions.find(functionId);
                if (oldCodeIt != m_compiledFunctions.end()) {
                    // 古いコードのクリーンアップ情報を記録
                    m_obsoleteCode.push_back({oldCodeIt->second, std::chrono::steady_clock::now()});
                }
                m_compiledFunctions[functionId] = recompiledCode;
            }
            
            // 状態の更新
            state.currentTier = targetTier;
            state.lastRecompilationTime = std::chrono::steady_clock::now();
            state.recompilationSuccessCount++;
            
            // 統計の更新
            updateCompilationStatistics(targetTier);
            m_stats.successfulRecompilations++;
            
            // 再コンパイル時間の記録
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            state.totalRecompilationTime += duration.count();
            
            if (m_debugMode) {
                logCompilationEvent(functionId, getOptimizationTierName(targetTier), 
                    "Recompilation successful", bytecodes.size(), duration.count());
            }
            
        } else {
            // 再コンパイル失敗時の処理
            state.recompilationFailureCount++;
            m_stats.failedRecompilations++;
            
            if (m_debugMode) {
                logCompilationEvent(functionId, getOptimizationTierName(targetTier), 
                    "Recompilation failed", bytecodes.size());
            }
            
            // 失敗回数が閾値を超えた場合はインタープリターモードに移行
            if (state.recompilationFailureCount >= m_policy.maxRecompilationFailures) {
                state.currentTier = JITOptimizationTier::None;
                state.recompilationDisabled = true;
                state.interpreterModeForced = true;
                
                if (m_debugMode) {
                    logCompilationEvent(functionId, "Recompilation", 
                        "Disabled due to repeated failures", 0);
                }
            }
        }
        
    } catch (const std::exception& e) {
        // 例外処理
        state.recompilationFailureCount++;
        m_stats.recompilationExceptions++;
        
        if (m_debugMode) {
            logCompilationEvent(functionId, "Recompilation", 
                "Exception: " + std::string(e.what()), 0);
        }
        
        // 例外が発生した場合は安全のためインタープリターモードに移行
        state.currentTier = JITOptimizationTier::None;
        state.recompilationDisabled = true;
        state.interpreterModeForced = true;
    }
}

// バックグラウンドコンパイラの完璧な実装
void JITManager::startBackgroundCompiler() {
    if (m_backgroundCompilerActive) {
        return; // 既に起動している
    }
    
    m_backgroundCompilerActive = true;
    m_backgroundCompilerThread = std::thread([this]() {
        this->backgroundCompilerWorker();
    });
    
    if (m_debugMode) {
        std::cout << "[JITManager] Background compiler started" << std::endl;
    }
}

void JITManager::stopBackgroundCompiler() {
    {
        std::lock_guard<std::mutex> lock(m_backgroundCompilerMutex);
        m_backgroundCompilerActive = false;
        m_backgroundCompilerCV.notify_all();
    }
    
    if (m_backgroundCompilerThread.joinable()) {
        m_backgroundCompilerThread.join();
    }
    
    if (m_debugMode) {
        std::cout << "[JITManager] Background compiler stopped" << std::endl;
    }
}

void JITManager::backgroundCompilerWorker() {
    while (m_backgroundCompilerActive) {
        BackgroundCompilerTask task;
        bool hasTask = false;
        
        // タスクキューからタスクを取得
        {
            std::unique_lock<std::mutex> lock(m_backgroundCompilerMutex);
            m_backgroundCompilerCV.wait(lock, [this] {
                return !m_backgroundCompilerQueue.empty() || !m_backgroundCompilerActive;
            });
            
            if (!m_backgroundCompilerActive) {
                break;
            }
            
            if (!m_backgroundCompilerQueue.empty()) {
                task = std::move(m_backgroundCompilerQueue.top());
                m_backgroundCompilerQueue.pop();
                hasTask = true;
            }
        }
        
        // タスクを実行
        if (hasTask) {
            auto startTime = std::chrono::steady_clock::now();
            
            try {
                performRecompilation(task.functionId, task.bytecodes, 
                                   static_cast<JITOptimizationTier>(task.targetTier), 
                                   task.deoptInfo);
                
                // バックグラウンドコンパイル統計の更新
                auto endTime = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
                m_stats.backgroundCompilationTime += duration.count();
                m_stats.backgroundCompilations++;
                
            } catch (const std::exception& e) {
                m_stats.backgroundCompilationFailures++;
                
                if (m_debugMode) {
                    std::cerr << "[JITManager] Background compilation exception: " 
                              << e.what() << std::endl;
                }
            }
        }
    }
}

// 完璧な古いコードクリーンアップ実装
void JITManager::cleanupObsoleteCode() {
    auto now = std::chrono::steady_clock::now();
    auto cutoffTime = now - std::chrono::seconds(m_policy.obsoleteCodeRetentionSeconds);
    
    std::lock_guard<std::mutex> lock(m_obsoleteCodeMutex);
    
    // 保持期間を過ぎた古いコードを削除
    auto it = std::remove_if(m_obsoleteCode.begin(), m_obsoleteCode.end(),
        [cutoffTime](const ObsoleteCodeEntry& entry) {
            return entry.timestamp < cutoffTime;
        });
    
    size_t removedCount = std::distance(it, m_obsoleteCode.end());
    m_obsoleteCode.erase(it, m_obsoleteCode.end());
    
    if (removedCount > 0 && m_debugMode) {
        std::cout << "[JITManager] Cleaned up " << removedCount 
                  << " obsolete code entries" << std::endl;
    }
}

// ゼロ除算例外ハンドラの完璧な実装
extern "C" void handleDivideByZeroException() {
    // JavaScript例外オブジェクトを作成
    auto* context = getCurrentExecutionContext();
    if (context) {
        Value errorObj = Value::createError(context, "RangeError", "Division by zero");
        context->throwException(errorObj);
    }
    
    // 実行を中断してJavaScriptエラーハンドリングに移行
    longjmp(getJavaScriptExceptionJumpBuffer(), 1);
}

} // namespace aerojs::core 