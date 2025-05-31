#include "tiered_jit_manager.h"
#include <chrono>
#include <sstream>

// このファイルに必要なヘッダーをグループ化
#include "jit_context.h"
#include "jit_optimizer.h"
#include "baseline/baseline_jit.h"
#include "optimizing/optimizing_jit.h"
#include "ir/ir_function.h" // IRFunction の定義
#include "ir/ir_optimizer.h"
#include "ir/constant_folding.h"
#include "bytecode/bytecode_parser.h" // BytecodeParser のヘッダー
#include "profiler/profiler.h"
#include "jit_compile_options.h"
#include "code_cache.h" // CodeCache のヘッダーをインクルード

// アーキテクチャ固有ヘッダー
#if defined(__x86_64__) || defined(_M_X64)
#define AEROJS_ARCH_X86_64 1
#include "backend/x86_64/jit_x86_64.h"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define AEROJS_ARCH_ARM64 1
// #include "backend/arm64/jit_arm64.h" // 対応する場合にコメント解除
#elif defined(__riscv)
#define AEROJS_ARCH_RISCV 1
// #include "backend/riscv/jit_riscv.h" // 対応する場合にコメント解除
#else
#define AEROJS_ARCH_UNKNOWN 1
#endif

namespace aerojs::core {

// Context の前方宣言 (ヘッダで行うのが望ましいが、cpp内で行う場合)
class Context; 

TieredJITManager::TieredJITManager()
    : m_baselineJIT(nullptr)
    , m_optimizedJIT(nullptr)
    , m_superOptimizedJIT(nullptr)
    , m_profiler(nullptr)
    , m_codeCache(nullptr) // m_codeCache を初期化
    , m_tieredCompilationEnabled(true)
    , m_tierUpThreshold(1000)
    , m_tierDownThreshold(5)
    // m_bytecodeStore はデフォルトコンストラクタで初期化される
{
}

TieredJITManager::~TieredJITManager() {
    Shutdown();
}

bool TieredJITManager::Initialize(Context* ctx) {
    // CodeCache を初期化
    // CodeCacheConfig はデフォルトコンストラクタで良いか、別途設定が必要か確認
    m_codeCache = std::make_unique<CodeCache>(ctx, CodeCacheConfig());

    // 各JITコンパイラの初期化
    m_baselineJIT = std::make_unique<BaselineJIT>();
    m_baselineJIT->EnableProfiling(true);
    
    // ターゲットアーキテクチャに応じたJITコンパイラを初期化
#if defined(AEROJS_ARCH_X86_64)
    // x86_64アーキテクチャ用JITコンパイラ
    m_optimizedJIT = std::make_unique<JITX86_64>();
    m_optimizedJIT->SetOptimizationLevel(OptimizationLevel::Medium);
    
    m_superOptimizedJIT = std::make_unique<JITX86_64>();
    m_superOptimizedJIT->SetOptimizationLevel(OptimizationLevel::Aggressive);
    
    // デバッグ情報の有効化（開発/テスト環境で便利）
#ifdef AEROJS_DEBUG
    m_optimizedJIT->EnableDebugInfo(true);
    m_superOptimizedJIT->EnableDebugInfo(true);
#endif

#elif defined(AEROJS_ARCH_ARM64)
    // ARM64アーキテクチャ用JITコンパイラ
    m_optimizedJIT = std::make_unique<JITArm64>();
    m_optimizedJIT->SetOptimizationLevel(OptimizationLevel::Medium);
    
    m_superOptimizedJIT = std::make_unique<JITArm64>();
    m_superOptimizedJIT->SetOptimizationLevel(OptimizationLevel::Aggressive);
    
    // ARM64固有の最適化設定
    if (auto arm64Compiler = dynamic_cast<JITArm64*>(m_optimizedJIT.get())) {
        // ハードウェア固有の機能を検出して有効化
        JITCompileOptions options;
        options.enableSIMD = true;  // ARM64はNEONをサポート
        options.enableFastMath = true;
        options.enableMicroarchOpt = true;
        
        arm64Compiler->SetCompileOptions(options);
        
        // ARMv8.2+の機能が利用可能かチェック
        if (arm64Compiler->IsCPUFeatureSupported(ARM64Feature::DotProduct)) {
            arm64Compiler->UseCPUFeature(ARM64Feature::DotProduct, true);
        }
        
        if (arm64Compiler->IsCPUFeatureSupported(ARM64Feature::SVE)) {
            arm64Compiler->UseCPUFeature(ARM64Feature::SVE, true);
        }
    }
    
    // 超最適化JITにも同様の設定を適用
    if (auto arm64SuperCompiler = dynamic_cast<JITArm64*>(m_superOptimizedJIT.get())) {
        JITCompileOptions options;
        options.enableSIMD = true;
        options.enableFastMath = true;
        options.enableMicroarchOpt = true;
        options.enableFunctionSplitting = true;
        options.enableLoopUnrolling = true;
        options.loopUnrollFactor = 8;  // より積極的なアンローリング
        
        arm64SuperCompiler->SetCompileOptions(options);
        
        // すべてのサポートされた機能を有効化
        if (arm64SuperCompiler->IsCPUFeatureSupported(ARM64Feature::DotProduct)) {
            arm64SuperCompiler->UseCPUFeature(ARM64Feature::DotProduct, true);
        }
        
        if (arm64SuperCompiler->IsCPUFeatureSupported(ARM64Feature::SVE)) {
            arm64SuperCompiler->UseCPUFeature(ARM64Feature::SVE, true);
        }
        
        if (arm64SuperCompiler->IsCPUFeatureSupported(ARM64Feature::CryptoExt)) {
            arm64SuperCompiler->UseCPUFeature(ARM64Feature::CryptoExt, true);
        }
    }
    
#ifdef AEROJS_DEBUG
    m_optimizedJIT->EnableDebugInfo(true);
    m_superOptimizedJIT->EnableDebugInfo(true);
#endif

#elif defined(AEROJS_ARCH_RISCV)
    // RISC-Vアーキテクチャ用JITコンパイラ
    m_optimizedJIT = std::make_unique<JITRiscV>();
    m_optimizedJIT->SetOptimizationLevel(OptimizationLevel::Medium);
    
    m_superOptimizedJIT = std::make_unique<JITRiscV>();
    m_superOptimizedJIT->SetOptimizationLevel(OptimizationLevel::Aggressive);
    
    // RISC-V固有の最適化設定
    if (auto riscvCompiler = dynamic_cast<JITRiscV*>(m_optimizedJIT.get())) {
        // RISC-V ISA拡張の検出と有効化
        JITCompileOptions options;
        options.enableMicroarchOpt = true;
        options.enableCacheOpt = true;
        
        riscvCompiler->SetCompileOptions(options);
        
        // 利用可能な拡張を検出
        if (riscvCompiler->IsCPUFeatureSupported(RiscVFeature::V)) {
            // ベクトル拡張を有効化
            riscvCompiler->UseCPUFeature(RiscVFeature::V, true);
            options.enableSIMD = true;
        }
        
        if (riscvCompiler->IsCPUFeatureSupported(RiscVFeature::B)) {
            // ビット操作拡張を有効化
            riscvCompiler->UseCPUFeature(RiscVFeature::B, true);
        }
    }
    
    // 超最適化JITにも同様の設定を適用
    if (auto riscvSuperCompiler = dynamic_cast<JITRiscV*>(m_superOptimizedJIT.get())) {
        JITCompileOptions options;
        options.enableSIMD = true;
        options.enableFastMath = true;
        options.enableMicroarchOpt = true;
        options.enableLoopUnrolling = true;
        
        riscvSuperCompiler->SetCompileOptions(options);
        
        // すべてのサポートされた機能を有効化
        if (riscvSuperCompiler->IsCPUFeatureSupported(RiscVFeature::V)) {
            riscvSuperCompiler->UseCPUFeature(RiscVFeature::V, true);
        }
        
        if (riscvSuperCompiler->IsCPUFeatureSupported(RiscVFeature::B)) {
            riscvSuperCompiler->UseCPUFeature(RiscVFeature::B, true);
        }
        
        if (riscvSuperCompiler->IsCPUFeatureSupported(RiscVFeature::G)) {
            riscvSuperCompiler->UseCPUFeature(RiscVFeature::G, true);
        }
    }
    
#ifdef AEROJS_DEBUG
    m_optimizedJIT->EnableDebugInfo(true);
    m_superOptimizedJIT->EnableDebugInfo(true);
#endif

#else
    // 未サポートアーキテクチャの場合は、最適化JITを無効化
    // ログに警告を出力
    std::cerr << "Warning: Unsupported architecture for optimizing JIT. Using baseline JIT only." << std::endl;
#endif
    
    // プロファイラを初期化
    m_profiler = std::make_unique<JITProfiler>();
    return m_profiler->Initialize() && m_codeCache != nullptr; // m_codeCache の初期化成功も確認
}

void TieredJITManager::Shutdown() {
    if (m_profiler) {
        m_profiler->Shutdown();
    }
    
    // m_codeCache の解放処理 (unique_ptr により自動でデストラクタが呼ばれるが明示的にresetも可能)
    if (m_codeCache) {
        m_codeCache.reset(); 
    }

    // 関数状態のクリーンアップ
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_functionStates.clear();
}

uint32_t TieredJITManager::CompileFunction(uint32_t functionId, const std::vector<uint8_t>& bytecodes, JITTier tier) {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    // バイトコードを保存 (TriggerTierUpCompilation で使用するため)
    // 本来はBytecodeModuleなどを管理する専用のマネージャがあるべき
    if (m_bytecodeStore.find(functionId) == m_bytecodeStore.end()) {
        m_bytecodeStore[functionId] = bytecodes;
    }
    
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
    
    // IRFunctionを生成（バイトコードパーサーから）
    std::unique_ptr<IRFunction> irFunction; // ここで宣言のみ
    
    // バイトコードをIRに変換する処理
    BytecodeParser parser(bytecodes.data(), bytecodes.size());
    irFunction = parser.Parse(functionId); // parser.Parse が std::unique_ptr<IRFunction> を返すと仮定
    if (!irFunction) {
        // パース失敗
        // エラーログなどを追加すると良い
        state->isCompiling = false;
        return 0;
    }
    
    // 定数畳み込み最適化パスを実行
    if (tier >= JITTier::Optimized) {
        // 定数畳み込みを適用
        jit::ConstantFolding::Run(*irFunction);
    }
    
    // IR最適化（ティアに応じた最適化レベル）
    IROptimizer optimizer;
    switch (tier) {
        case JITTier::Baseline:
            optimizer.SetOptimizationLevel(OptimizationLevel::Basic);
            break;
        case JITTier::Optimized:
            optimizer.SetOptimizationLevel(OptimizationLevel::Medium);
            break;
        case JITTier::SuperOptimized:
            optimizer.SetOptimizationLevel(OptimizationLevel::Aggressive);
            break;
        default:
            break;
    }
    
    irFunction = optimizer.Optimize(std::move(irFunction), functionId, optimizer.GetOptimizationLevel());
    
    // IRをネイティブコードにコンパイル
    void* codePtr = nullptr;
    
    if (tier == JITTier::Baseline && m_baselineJIT) {
        // BaselineJITの場合、関数IDを設定
        m_baselineJIT->SetFunctionId(functionId);
        
        // バイトコードから直接コンパイルする
        size_t codeSize = 0;
        auto generated_code = m_baselineJIT->Compile(bytecodes, codeSize);
        
        // 生成コードをメモリマネージャへ登録する本格実装
        if (generated_code && codeSize > 0 && m_codeCache) {
            void* executableMemory = m_memoryManager->allocateExecutableMemory(codeSize);
            if (!executableMemory) {
                std::cerr << "Error: Failed to allocate executable memory for functionId: " << functionId << std::endl;
                state->isCompiling = false;
                return 0;
            }
            std::memcpy(executableMemory, generated_code.get(), codeSize);
            if (!m_memoryManager->protectMemory(executableMemory, codeSize, MemoryProtection::ReadExecute)) {
                std::cerr << "Error: Failed to set memory protection for functionId: " << functionId << std::endl;
                m_memoryManager->freeMemory(executableMemory);
                state->isCompiling = false;
                return 0;
            }
            m_memoryManager->flushInstructionCache(executableMemory, codeSize);
            if (!m_codeCache->registerCode(functionId, executableMemory, codeSize, tier)) {
                std::cerr << "Error: Failed to register code in cache for functionId: " << functionId << std::endl;
                m_memoryManager->freeMemory(executableMemory);
                state->isCompiling = false;
                return 0;
            }
            m_profiler->RecordCompilation(functionId, tier, codeSize);
#ifdef AEROJS_DEBUG
            if (m_debugInfoRegistry && m_baselineJIT->HasDebugInfo()) {
                m_debugInfoRegistry->registerDebugInfo(functionId, tier, m_baselineJIT->GetDebugInfo());
            }
#endif
            return executableMemory;
        }
    } else {
        // 最適化JITの場合、IRからコンパイルする
        if ((tier == JITTier::Optimized && m_optimizedJIT) || 
            (tier == JITTier::SuperOptimized && m_superOptimizedJIT)) {
            
            // アーキテクチャ固有の最適化コードをここに追加
#if defined(AEROJS_ARCH_X86_64)
            // x86_64固有の最適化がある場合、ここで適用
            if (auto x86_64Compiler = dynamic_cast<JITX86_64*>(compiler)) {
                // x86_64固有の設定
                JITCompileOptions options;
                
                // SIMD最適化を有効にする（対応するCPU機能が利用可能な場合）
                if (JITX86_64::IsCPUFeatureSupported(CPUFeature::SSE2)) {
                    options.enableSIMD = true;
                    x86_64Compiler->UseCPUFeature(CPUFeature::SSE2, true);
                }
                
                // AVX対応
                if (JITX86_64::IsCPUFeatureSupported(CPUFeature::AVX)) {
                    x86_64Compiler->UseCPUFeature(CPUFeature::AVX, true);
                }
                
                // FMA対応
                if (JITX86_64::IsCPUFeatureSupported(CPUFeature::FMA)) {
                    x86_64Compiler->UseCPUFeature(CPUFeature::FMA, true);
                }
                
                // 階層に応じて最適化を調整
                if (tier == JITTier::SuperOptimized) {
                    options.enableLoopUnrolling = true;
                    options.loopUnrollFactor = 8;
                    options.enableFunctionSplitting = true;
                    options.enableHotCodeInlining = true;
                }
                
                // コンパイルオプションを設定
                x86_64Compiler->SetCompileOptions(options);
            }
#endif
            
            codePtr = compiler->Compile(*irFunction, functionId);
            
            if (codePtr && m_codeCache) {                // コードサイズの取得                size_t compiledCodeSize = compiler->GetCompiledCodeSize(functionId);                                if (compiledCodeSize == 0) {                    std::cerr << "Error: Could not determine compiled code size for functionId: " << functionId << std::endl;                    return 0;                }                                // メタデータを取得                JITCodeMetadata metadata;                compiler->GetCodeMetadata(functionId, &metadata);                                // 最適化されたコンパイル結果をメモリマネージャに登録                void* executableMemory = m_memoryManager->allocateExecutableMemory(compiledCodeSize);                if (!executableMemory) {                    std::cerr << "Error: Failed to allocate executable memory for optimized code, functionId: " << functionId << std::endl;                    return 0;                }                                // コンパイラから生成されたコードをコピー                std::memcpy(executableMemory, codePtr, compiledCodeSize);                                // メモリプロテクションを設定（読み取り可能・実行可能）                if (!m_memoryManager->protectMemory(executableMemory, compiledCodeSize, MemoryProtection::ReadExecute)) {                    std::cerr << "Error: Failed to set memory protection for optimized code, functionId: " << functionId << std::endl;                    m_memoryManager->freeMemory(executableMemory);                    return 0;                }                                // 命令キャッシュをフラッシュして最適化                m_memoryManager->flushInstructionCache(executableMemory, compiledCodeSize);                                // コードキャッシュに登録                CodeCacheEntry entry;                entry.codePtr = executableMemory;                entry.codeSize = compiledCodeSize;                entry.functionId = functionId;                entry.timestamp = std::chrono::steady_clock::now();                entry.tier = tier;                entry.metadata = metadata;                                if (m_codeCache->registerCode(entry)) {                    // エントリーポイントを取得                    uint32_t entryPoint = reinterpret_cast<uintptr_t>(executableMemory);                                        // 関数状態を更新                    UpdateFunctionState(functionId, tier, entryPoint, static_cast<uint32_t>(compiledCodeSize));                                        // プロファイリング情報を更新                    m_profiler->RecordCompilation(functionId, tier, compiledCodeSize);                                        // デバッグ情報の登録（開発モードのみ）                    #ifdef AEROJS_DEBUG                    if (metadata.debugInfo) {                        m_debugInfoRegistry->registerDebugInfo(functionId, tier, metadata.debugInfo);                    }                    #endif                                        // コンパイル完了                    state->isCompiling = false;                    return entryPoint;                } else {                    // CodeCache への登録失敗                    std::cerr << "Error: Failed to register optimized code to cache for functionId: " << functionId << std::endl;                    m_memoryManager->freeMemory(executableMemory);                    return 0;                }
            }
        }
    }
    
    // コンパイル失敗
    state->isCompiling = false;
    return 0;
}

void TieredJITManager::TriggerTierUpCompilation(uint32_t functionId, JITTier targetTier) {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    auto* state = GetFunctionState(functionId);
    if (!state || state->isCompiling || state->currentTier >= targetTier) {
        return;
    }
    
    // バイトコードを取得
    std::vector<uint8_t> bytecodes;
    // バイトコードマネージャからバイトコードを取得
    if (m_bytecodeManager) {
        // バイトコードマネージャAPIを使用
        BytecodeHandle handle = m_bytecodeManager->getBytecodeForFunction(functionId);
        if (handle.isValid()) {
            // バイトコードを取得
            if (!m_bytecodeManager->getBytecodeData(handle, bytecodes)) {
                std::cerr << "Error: Failed to get bytecode data for functionId: " << functionId << std::endl;
                state->isCompiling = false;
                return;
            }
        } else {
            // ソースコードからバイトコードを生成する必要がある場合
            if (m_functionRegistry) {
                FunctionInfo* funcInfo = m_functionRegistry->getFunctionInfo(functionId);
                if (funcInfo && funcInfo->sourceCode.length() > 0) {
                    // ソースからバイトコードをコンパイル
                    if (!m_bytecodeCompiler->compile(funcInfo->sourceCode, bytecodes)) {
                        std::cerr << "Error: Failed to compile bytecode for functionId: " << functionId << std::endl;
                        state->isCompiling = false;
                        return;
                    }
                    
                    // バイトコードをマネージャに登録
                    BytecodeHandle newHandle = m_bytecodeManager->registerBytecode(bytecodes, functionId);
                    if (!newHandle.isValid()) {
                        std::cerr << "Warning: Failed to register new bytecode for functionId: " << functionId << std::endl;
                    }
                } else {
                    std::cerr << "Error: No source code available for functionId: " << functionId << std::endl;
                    state->isCompiling = false;
                    return;
                }
            } else {
                std::cerr << "Error: Function registry not available" << std::endl;
                state->isCompiling = false;
                return;
            }
        }
    } else {
        // フォールバック: ローカルストレージから取得
        auto it = m_bytecodeStore.find(functionId);
        if (it != m_bytecodeStore.end()) {
            bytecodes = it->second;
        } else {
            // バイトコードが見つからない場合のエラー処理
            std::cerr << "Error: Bytecode not found for functionId: " << functionId << std::endl;
            state->isCompiling = false;
            return;
        }
    }
    
    // 非同期コンパイルを開始
    state->isCompiling = true;
    // スレッドプールでコンパイルを実行
    if (m_compilerThreadPool) {
        // タスクをスレッドプールに投入
        std::function<void()> compileTask = [this, functionId, bytecodes, targetTier]() {
            this->compileInBackground(functionId, bytecodes, targetTier);
        };
        m_compilerThreadPool->enqueue(compileTask);
    } else {
        // スレッドプールが利用できない場合は同期実行
        compileInBackground(functionId, bytecodes, targetTier);
    }
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
        // バイトコードを取得
        std::vector<uint8_t> bytecodes;
        auto it = m_bytecodeStore.find(functionId);
        if (it != m_bytecodeStore.end()) {
            bytecodes = it->second;
        } else {
            // バイトコードが見つからない場合のエラー処理 (ログ出力など)
            // ここではTier Downを中止 (あるいはInterpreterに戻すなど)
            // std::cerr << "Error: Bytecode not found for functionId during TierDown: " << functionId << std::endl;
            state->isCompiling = false; // isCompiling フラグをリセット
            return;
        }
        
        // 再コンパイル
        uint32_t newEntryPoint = CompileFunction(functionId, bytecodes, targetTier);
        
        if (newEntryPoint) {
            state->entryPoint = newEntryPoint;
        }
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
                // x86_64バックエンドが利用可能な場合のみ最適化JITに昇格
#if defined(AEROJS_ARCH_X86_64)
                nextTier = JITTier::Optimized;
#else
                nextTier = JITTier::Baseline; // 変更なし
#endif
                break;
            case JITTier::Optimized:
                nextTier = JITTier::SuperOptimized;
                break;
            default:
                return;
        }
        
        if (nextTier != JITTier::Interpreter && nextTier != currentTier) {
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
        // 各階層のコンパイル済みコードを解放
        for (auto& pair : it->second.tierStates) { // auto& pair に変更
            JITTier tier = pair.first;
            FunctionTierState& tierState = pair.second; // auto& tierState に変更

            if (tierState.isCompiled && m_codeCache) {
                JITCompiler* compiler = GetCompilerForTier(tier);
                if (compiler) {
                    // entryPoint (CodeEntry* のキャスト値) から CodeEntry を取得し、そこから codePtr を得る
                    CodeEntry* codeEntry = reinterpret_cast<CodeEntry*>(static_cast<uintptr_t>(tierState.entryPoint));
                    if (codeEntry) {
                        void* codePtr = codeEntry->getCode(); // CodeEntry に getCode() があると仮定
                        compiler->ReleaseCode(codePtr); // JITCompiler が解放処理を持つと仮定
                        m_codeCache->removeEntry(codeEntry->getId()); // CodeCache からも削除
                    } else {
                        // codeEntry が null の場合、不正な entryPoint の可能性 (ログ等)
                    }
                }
            }
        }
        
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
    
    // すべての関数のコンパイル済みコードを解放
    for (auto& state_pair : m_functionStates) { // auto& state_pair に変更
        FunctionExecutionState& state = state_pair.second; // auto& state に変更
        for (auto& tier_pair : state.tierStates) { // auto& tier_pair に変更
            JITTier tier = tier_pair.first;
            FunctionTierState& tierState = tier_pair.second; // auto& tierState に変更

            if (tierState.isCompiled && m_codeCache) {
                JITCompiler* compiler = GetCompilerForTier(tier);
                if (compiler) {
                    // entryPoint (CodeEntry* のキャスト値) から CodeEntry を取得し、そこから codePtr を得る
                    CodeEntry* codeEntry = reinterpret_cast<CodeEntry*>(static_cast<uintptr_t>(tierState.entryPoint));
                    if (codeEntry) {
                        void* codePtr = codeEntry->getCode(); // CodeEntry に getCode() があると仮定
                        compiler->ReleaseCode(codePtr); // JITCompiler が解放処理を持つと仮定
                        m_codeCache->removeEntry(codeEntry->getId()); // CodeCache からも削除
                    } else {
                        // codeEntry が null の場合、不正な entryPoint の可能性 (ログ等)
                    }
                }
            }
        }
    }
    
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
    
    // アーキテクチャ情報
#if defined(AEROJS_ARCH_X86_64)
    ss << "アーキテクチャ: x86_64\n";
#elif defined(AEROJS_ARCH_ARM64)
    ss << "アーキテクチャ: ARM64\n";
#elif defined(AEROJS_ARCH_RISCV)
    ss << "アーキテクチャ: RISC-V\n";
#else
    ss << "アーキテクチャ: 不明\n";
#endif
    
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
    m_tierUpThreshold = threshold;
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
        case JITTier::Interpreter:
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