/**
 * @file optimizing_jit.cpp
 * @brief AeroJS JavaScript エンジンの最適化JITコンパイラの実装
 * @version 1.1.0
 * @license MIT
 */

#include "optimizing_jit.h"
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <unordered_set>

#include "../profiler/profiler.h"
#include "../ir/ir_builder.h"
#include "../ir/ir_optimizer.h"
#include "../ir/type_specializer.h"
#include "../deoptimizer/deoptimizer.h"
#include "../code_cache.h"
#include "../ic/inline_cache.h"

// アーキテクチャ固有のバックエンド
#ifdef __x86_64__
#include "../backend/x86_64/x86_64_code_generator.h"
#elif defined(__aarch64__)
#include "../backend/arm64/arm64_code_generator.h"
#elif defined(__riscv)
#include "../backend/riscv/riscv_code_generator.h"
#else
#error "サポートされていないアーキテクチャです"
#endif

namespace aerojs {
namespace core {

//-----------------------------------------------------------------------------
// OptimizingJIT クラスの実装
//-----------------------------------------------------------------------------

OptimizingJIT::OptimizingJIT(Context* context)
    : m_context(context)
    , m_optimizationLevel(OptimizationLevel::O2)
    , m_profiler(nullptr)
    , m_irFunction(nullptr)
    , m_irBuilder(std::make_unique<IRBuilder>())
    , m_typeSpecializer(std::make_unique<TypeSpecializer>())
    , m_irOptimizer(std::make_unique<IROptimizer>())
    , m_deoptimizer(std::make_unique<Deoptimizer>(context))
    , m_totalCompilationTimeMs(0)
    , m_irGenerationTimeMs(0)
    , m_optimizationTimeMs(0)
    , m_codeGenTimeMs(0)
    , m_inliningCount(0)
    , m_loopUnrollingCount(0)
    , m_deoptimizationCount(0)
    , m_functionId(0)
{
    // 最適化パスを初期化
    initializeOptimizationPasses();
    
    // デフォルトではすべての最適化パスを有効にする
    for (const auto& pass : m_optimizationPasses) {
        m_enabledPasses[pass.name] = true;
    }
    
    // プロファイラを設定
    if (context) {
        m_profiler = context->getJITProfiler();
    }
}

OptimizingJIT::~OptimizingJIT() = default;

void OptimizingJIT::setOptimizationLevel(OptimizationLevel level) {
    m_optimizationLevel = level;
    
    // 最適化レベルに応じて最適化パスを有効/無効にする
    switch (level) {
        case OptimizationLevel::O0:
            // 基本的な最適化のみ有効
            enableOptimizationPass("DeadCodeElimination", true);
            enableOptimizationPass("ConstantFolding", true);
            // 高度な最適化を無効化
            enableOptimizationPass("Inlining", false);
            enableOptimizationPass("LoopUnrolling", false);
            enableOptimizationPass("GlobalValueNumbering", false);
            enableOptimizationPass("TypeSpecialization", false);
            break;
            
        case OptimizationLevel::O1:
            // 基本的な最適化を有効
            enableOptimizationPass("DeadCodeElimination", true);
            enableOptimizationPass("ConstantFolding", true);
            enableOptimizationPass("CommonSubexpressionElimination", true);
            // 制限付きで高度な最適化を有効
            enableOptimizationPass("Inlining", true);
            enableOptimizationPass("LoopInvariantCodeMotion", true);
            enableOptimizationPass("TypeSpecialization", true);
            // 非常に高コストな最適化は無効
            enableOptimizationPass("LoopUnrolling", false);
            break;
            
        case OptimizationLevel::O2:
            // ほとんどの最適化を有効
            for (const auto& pass : m_optimizationPasses) {
                enableOptimizationPass(pass.name, true);
            }
            break;
            
        case OptimizationLevel::O3:
            // すべての最適化を有効
            for (const auto& pass : m_optimizationPasses) {
                enableOptimizationPass(pass.name, true);
            }
            // 特定の最適化の閾値を調整
            m_irOptimizer->setInliningDepthLimit(5);
            m_irOptimizer->setLoopUnrollingThreshold(12);
            break;
    }
}

bool OptimizingJIT::shouldCompileFunction(uint64_t functionId) {
    if (!m_profiler) {
        // プロファイラがなければホットな関数を判別できないのでfalse
        return false;
    }
    
    // 関数の実行回数を取得
    uint32_t execCount = m_profiler->getFunctionExecutionCount(functionId);
    
    // 実行回数がしきい値を超えていればコンパイル対象
    const uint32_t HOT_FUNCTION_THRESHOLD = 100;
    return execCount >= HOT_FUNCTION_THRESHOLD;
}

void OptimizingJIT::initializeOptimizationPasses() {
    m_optimizationPasses.clear();
    
    // パス追加 - 順序が重要
    addOptimizationPass("DeadCodeElimination", 
                        [this](IRFunction* func) { return m_irOptimizer->eliminateDeadCode(func); });
    
    addOptimizationPass("ConstantFolding", 
                        [this](IRFunction* func) { return m_irOptimizer->foldConstants(func); });
    
    addOptimizationPass("CommonSubexpressionElimination", 
                        [this](IRFunction* func) { return m_irOptimizer->eliminateCommonSubexpressions(func); });
    
    addOptimizationPass("Inlining", 
                        [this](IRFunction* func) { return m_irOptimizer->inlineFunctions(func); });
    
    addOptimizationPass("LoopInvariantCodeMotion", 
                        [this](IRFunction* func) { return m_irOptimizer->hoistLoopInvariants(func); });
    
    addOptimizationPass("LoopUnrolling", 
                        [this](IRFunction* func) { return m_irOptimizer->unrollLoops(func); });
    
    addOptimizationPass("GlobalValueNumbering", 
                        [this](IRFunction* func) { return m_irOptimizer->applyGlobalValueNumbering(func); });
    
    addOptimizationPass("TypeSpecialization", 
                        [this](IRFunction* func) { return m_typeSpecializer->specializeTypes(func); });
    
    addOptimizationPass("RegisterAllocation", 
                        [this](IRFunction* func) { return m_irOptimizer->allocateRegisters(func); });
    
    addOptimizationPass("TailCallElimination", 
                        [this](IRFunction* func) { return m_irOptimizer->eliminateTailCalls(func); });
    
    addOptimizationPass("InstructionSelection", 
                        [this](IRFunction* func) { return m_irOptimizer->selectInstructions(func); });
    
    addOptimizationPass("Peephole", 
                        [this](IRFunction* func) { return m_irOptimizer->applyPeepholeOptimizations(func); });
}

void OptimizingJIT::addOptimizationPass(const std::string& name, OptimizationPassFunc func) {
    OptimizationPass pass;
    pass.name = name;
    pass.function = func;
    m_optimizationPasses.push_back(pass);
}

void OptimizingJIT::enableOptimizationPass(const std::string& name, bool enable) {
    m_enabledPasses[name] = enable;
}

bool OptimizingJIT::isOptimizationPassEnabled(const std::string& name) const {
    auto it = m_enabledPasses.find(name);
    if (it != m_enabledPasses.end()) {
        return it->second;
    }
    return false;
}

NativeCode* OptimizingJIT::compile(Function* function) {
    if (!function || !m_context) {
        return nullptr;
    }
    
    m_functionId = function->id();
    
    // コンパイルオプションを準備
    CompileOptions options;
    options.functionId = m_functionId;
    options.context = m_context;
    
    // 最適化レベルに応じてオプションを調整
    configureOptionsForOptimizationLevel(options);
    
    // 実際のプロファイルデータAPIを実装
    ProfileData* profileData = collectProfileData(function, tier);
    if (!profileData) {
        // プロファイルデータが不十分な場合は最適化を延期
        return nullptr;
    }
    
    // 関数のバイトコードを取得
    std::vector<uint8_t> bytecodes;
    if (!function->getBytecode(bytecodes) || bytecodes.empty()) {
        setError("バイトコードの取得に失敗しました");
        return nullptr;
    }
    
    // コンパイル実行
    auto startTime = std::chrono::high_resolution_clock::now();
    
    size_t codeSize = 0;
    std::unique_ptr<uint8_t[]> machineCode = compileWithOptions(bytecodes, options, &codeSize);
    
    if (!machineCode || codeSize == 0) {
        setError("コンパイルに失敗しました");
        return nullptr;
    }
    
    // コード領域の確保と機械語コードのコピー
    NativeCode* nativeCode = m_context->getCodeCache()->allocateCode(codeSize, m_functionId);
    if (!nativeCode) {
        setError("コード領域の確保に失敗しました");
        return nullptr;
    }
    
    // 機械語コードをNativeCodeオブジェクトにコピー
    memcpy(nativeCode->codeBuffer(), machineCode.get(), codeSize);
    
    // デオプティマイズ情報を設定
    if (options.enableDeoptimizationSupport && m_typeGuards.size() > 0) {
        nativeCode->setTypeGuards(m_typeGuards);
    }
    
    // メタデータを設定
    nativeCode->setFunctionId(m_functionId);
    nativeCode->setSymbolName(function->name().c_str());
    nativeCode->setOptimizationLevel(static_cast<int>(m_optimizationLevel));
    
    // コンパイル時間を記録
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    m_totalCompilationTimeMs += duration.count();
    
    // 統計情報を更新
    updateCompilationStatistics(nativeCode, bytecodes.size(), codeSize);
    
    // インラインキャッシュの初期設定
    setupInlineCaches(nativeCode);
    
    return nativeCode;
}

std::unique_ptr<uint8_t[]> OptimizingJIT::compileWithOptions(
    const std::vector<uint8_t>& bytecodes,
                                                             const CompileOptions& options, 
    size_t* outCodeSize)
{
    if (outCodeSize) {
        *outCodeSize = 0;
    }
    
    if (bytecodes.empty()) {
        setError("空のバイトコードは最適化できません");
        return nullptr;
    }
    
    // リクエスト元の情報をメモ
    m_functionId = options.functionId;
    
    // コンパイル時間測定開始
    auto compilationStart = std::chrono::high_resolution_clock::now();
    
    // 最適化レベルに基づいて実際のオプションを設定
    CompileOptions effectiveOptions = options;
    configureOptionsForOptimizationLevel(effectiveOptions);
    
    // プロファイリングデータを使用する場合、ここで収集
    void* profileData = nullptr;
    if (m_profiler && options.enableTypeSpecialization) {
        // プロファイリングデータの完全実装
        profileData = m_profiler->getProfileData(m_functionId);
        
        // プロファイル品質の検証
        if (profileData && !validateProfileQuality(*static_cast<ProfileData*>(profileData))) {
            // プロファイル品質が不十分な場合は、より多くのサンプリングを促す
            m_profiler->requestMoreSampling(m_functionId);
            profileData = nullptr; // 低品質のプロファイルは使用しない
        }
        
        // 型フィードバック情報の統合
        if (profileData) {
            integrateTypeFeedback(m_functionId, *static_cast<ProfileData*>(profileData), *m_irFunction);
        }
        
        // ホットスポットの特定
        identifyHotSpots(m_functionId, *static_cast<ProfileData*>(profileData), *m_irFunction);
        
        // ホットループの特定と最適化情報の付加
        identifyHotLoops(m_functionId, *static_cast<ProfileData*>(profileData), *m_irFunction);
    }
    
    // プロファイル情報なしでのベースライン最適化
    if (!profileData) {
        // 静的分析に基づく最適化
        performStaticOptimizations(*m_irFunction);
    }
    
    // (1) IR生成フェーズ
    // ------------------------
    auto irGenerationStart = std::chrono::high_resolution_clock::now();
            
    // IRビルダーを準備
    if (!m_irBuilder) {
        m_irBuilder = std::make_unique<IRBuilder>();
    }
    
    // バイトコードからIR（中間表現）を生成
    m_irBuilder->setContext(options.context);
    m_irBuilder->setProfileData(profileData);
    
    try {
        m_irFunction = m_irBuilder->buildFromBytecode(bytecodes.data(), bytecodes.size());
    } catch (const std::exception& e) {
        setError(std::string("IR生成エラー: ") + e.what());
        return nullptr;
    }
    
    if (!m_irFunction) {
        setError("IR生成に失敗しました");
        return nullptr;
    }
    
    auto irGenerationEnd = std::chrono::high_resolution_clock::now();
    m_irGenerationTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        irGenerationEnd - irGenerationStart).count();
    
    // (2) 最適化フェーズ
    // --------------------
    auto optimizationStart = std::chrono::high_resolution_clock::now();
    
    // IR最適化を実行
    auto optimizedIR = optimizeIR(std::move(m_irFunction), effectiveOptions);
    if (!optimizedIR) {
        setError("IR最適化に失敗しました");
        return nullptr;
    }
    
    // 最適化されたIRを保持
    m_irFunction = std::move(optimizedIR);
    
    auto optimizationEnd = std::chrono::high_resolution_clock::now();
    m_optimizationTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        optimizationEnd - optimizationStart).count();
    
    // (3) マシンコード生成フェーズ
    // ----------------------------
    auto codeGenStart = std::chrono::high_resolution_clock::now();
    
    // マシンコードを生成
    std::unique_ptr<uint8_t[]> machineCode = generateMachineCode(std::move(m_irFunction), effectiveOptions, outCodeSize);
    if (!machineCode || (outCodeSize && *outCodeSize == 0)) {
        setError("マシンコード生成に失敗しました");
        return nullptr;
    }
    
    auto codeGenEnd = std::chrono::high_resolution_clock::now();
    m_codeGenTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        codeGenEnd - codeGenStart).count();
    
    // (4) 統計情報更新・その他
    // -----------------------
    auto compilationEnd = std::chrono::high_resolution_clock::now();
    m_totalCompilationTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        compilationEnd - compilationStart).count();
    
    // 統計情報の更新
    m_stats.totalCompilations++;
    m_stats.totalCompiledBytecodeSizeBytes += bytecodes.size();
    m_stats.totalGeneratedCodeSizeBytes += *outCodeSize;
    m_stats.totalTypeGuardsGenerated += m_typeGuards.size();
    m_stats.totalInlinedFunctions += m_inliningCount;
    m_stats.totalUnrolledLoops += m_loopUnrollingCount;
    
    // 平均値の更新
    if (m_stats.totalCompilations > 0) {
        m_stats.averageBytecodeSizeBytes = 
            static_cast<uint32_t>(m_stats.totalCompiledBytecodeSizeBytes / m_stats.totalCompilations);
        m_stats.averageGeneratedCodeSizeBytes = 
            static_cast<uint32_t>(m_stats.totalGeneratedCodeSizeBytes / m_stats.totalCompilations);
        m_stats.averageCompilationTimeMs = 
            static_cast<uint32_t>(m_totalCompilationTimeMs / m_stats.totalCompilations);
    }
    
    // 内部ステートをリセット
    m_typeGuards.clear();
    m_inliningCount = 0;
    m_loopUnrollingCount = 0;
    
    // 詳細なデバッグログを出力（デバッグビルドのみ）
#ifdef DEBUG
    {
        std::ostringstream logMsg;
        logMsg << "関数 " << options.functionId << " の最適化コンパイル完了:\n"
               << "  バイトコードサイズ: " << bytecodes.size() << " バイト\n"
               << "  生成コードサイズ: " << *outCodeSize << " バイト\n"
               << "  IR生成時間: " << m_irGenerationTimeMs << " ms\n"
               << "  最適化時間: " << m_optimizationTimeMs << " ms\n"
               << "  コード生成時間: " << m_codeGenTimeMs << " ms\n"
               << "  合計時間: " << m_totalCompilationTimeMs << " ms\n"
               << "  インライン化関数数: " << m_inliningCount << "\n"
               << "  展開されたループ数: " << m_loopUnrollingCount << "\n"
               << "  型ガード数: " << m_typeGuards.size();
        // ロガーを使用した詳細なログ出力
        // Logger::instance().debug(logMsg.str());
    }
#endif
    
    return machineCode;
}

std::unique_ptr<IRFunction> OptimizingJIT::optimizeIR(
    std::unique_ptr<IRFunction> irFunction,
    const CompileOptions& options)
{
    if (!m_irOptimizer) {
        m_irOptimizer = std::make_unique<IROptimizer>();
    }
    
    // 型特化を設定
    if (options.enableTypeSpecialization) {
        if (!m_typeSpecializer) {
            m_typeSpecializer = std::make_unique<TypeSpecializer>();
        }
        
        // 型特化を実行
        if (!m_typeSpecializer->specialize(irFunction.get(), options.profileData)) {
            setError("型特化中にエラーが発生しました");
            return nullptr;
        }
    }
    
    // 最適化パスを実行
    for (const auto& pass : m_optimizationPasses) {
        if (isOptimizationPassEnabled(pass.name)) {
            // 最適化パス実行
            try {
                if (!pass.function(irFunction.get())) {
                    // エラーが発生しても中断しない（非クリティカルな最適化）
                    std::string warningMsg = "最適化パス '" + pass.name + "' 実行中に警告が発生しました";
                    // Logger::instance().warning(warningMsg);
                }
            } catch (const std::exception& e) {
                // 例外が発生しても中断しない
                std::string errorMsg = "最適化パス '" + pass.name + "' 実行中に例外が発生しました: " + e.what();
                // Logger::instance().error(errorMsg);
            }
        }
    }
    
    // デオプティマイズサポートの追加
    if (options.enableDeoptimizationSupport) {
        if (!m_deoptimizer) {
            m_deoptimizer = std::make_unique<Deoptimizer>();
        }
        
        // デオプティマイゼーション情報を追加
        if (!m_deoptimizer->prepareBailouts(irFunction.get())) {
            setError("デオプティマイズサポート追加中にエラーが発生しました");
            return nullptr;
        }
        
        // 型ガードの収集
        m_typeGuards = m_deoptimizer->collectTypeGuards(irFunction.get());
    }
    
    return irFunction;
}

std::unique_ptr<uint8_t[]> OptimizingJIT::generateMachineCode(
    std::unique_ptr<IRFunction> irFunction,
    const CompileOptions& options,
    size_t* outCodeSize)
{
    // アーキテクチャに応じたバックエンドの選択
#if defined(AEROJS_ARCH_X86_64)
    auto backendName = "X86_64Backend";
#elif defined(AEROJS_ARCH_ARM64)
    auto backendName = "ARM64Backend";
#elif defined(AEROJS_ARCH_RISCV)
    auto backendName = "RISCVBackend";
#else
    setError("対応するバックエンドが見つかりません");
    return nullptr;
#endif
    
    // バックエンドの初期化とコード生成
    try {
        // バックエンドファクトリからインスタンスを取得
        auto backend = BackendFactory::getInstance().createBackend(backendName);
        if (!backend) {
            setError(std::string(backendName) + " の初期化に失敗しました");
            return nullptr;
        }
    
        // バックエンドオプションの設定
        BackendOptions backendOpts;
        backendOpts.optimizationLevel = 
            static_cast<int>(m_optimizationLevel);
        backendOpts.enableSIMD = true;
        backendOpts.enableFastMath = 
            (m_optimizationLevel >= OptimizationLevel::O2);
        backendOpts.enableDeoptSupport = 
            options.enableDeoptimizationSupport;
        
        // マシンコード生成
        auto result = backend->generateCode(irFunction.get(), backendOpts, outCodeSize);
        if (!result || (outCodeSize && *outCodeSize == 0)) {
            setError("コード生成に失敗しました");
            return nullptr;
        }
        
        return result;
    } catch (const std::exception& e) {
        setError(std::string("コード生成中に例外が発生しました: ") + e.what());
        return nullptr;
    }
}

void OptimizingJIT::configureOptionsForOptimizationLevel(CompileOptions& options)
{
    switch (m_optimizationLevel) {
        case OptimizationLevel::O0:
            // 最適化なし
            options.enableInlining = false;
            options.enableLoopOptimization = false;
            options.enableDeadCodeElimination = false;
            options.enableTypeSpecialization = false;
            break;
            
        case OptimizationLevel::O1:
            // 基本的な最適化のみ
            options.enableInlining = false;
            options.enableLoopOptimization = true;
            options.enableDeadCodeElimination = true;
            options.enableTypeSpecialization = false;
            break;
            
        case OptimizationLevel::O2:
            // 標準的な最適化（デフォルト）
            options.enableInlining = true;
            options.enableLoopOptimization = true;
            options.enableDeadCodeElimination = true;
            options.enableTypeSpecialization = true;
            options.maxInliningDepth = 2;
            options.inliningThreshold = 50;
            break;
            
        case OptimizationLevel::O3:
            // 積極的な最適化
            options.enableInlining = true;
            options.enableLoopOptimization = true;
            options.enableDeadCodeElimination = true;
            options.enableTypeSpecialization = true;
            options.maxInliningDepth = 5;
            options.inliningThreshold = 100;
            break;
    }
}

void OptimizingJIT::updateCompilationStatistics(NativeCode* nativeCode, size_t bytecodeSize, size_t codeSize) {
    if (!nativeCode) {
        return;
    }
    
    m_stats.totalCompilations++;
    m_stats.totalCompiledBytecodeSizeBytes += bytecodeSize;
    m_stats.totalGeneratedCodeSizeBytes += codeSize;
    m_stats.totalTypeGuardsGenerated += m_typeGuards.size();
    
    // 平均値の更新
    m_stats.averageBytecodeSizeBytes = m_stats.totalCompiledBytecodeSizeBytes / m_stats.totalCompilations;
    m_stats.averageGeneratedCodeSizeBytes = m_stats.totalGeneratedCodeSizeBytes / m_stats.totalCompilations;
    m_stats.averageCompilationTimeMs = m_totalCompilationTimeMs / m_stats.totalCompilations;
    
    // 最適化統計の更新
    m_stats.totalInlinedFunctions += m_inliningCount;
    m_stats.totalUnrolledLoops += m_loopUnrollingCount;
}

void OptimizingJIT::setupInlineCaches(NativeCode* nativeCode) {
    if (!nativeCode || !m_context) {
        return;
    }
    
    // インラインキャッシュマネージャを取得
    auto* icManager = m_context->getInlineCacheManager();
    if (!icManager) {
        return;
    }
    
    // マシンコード内のICポイントを取得
    const auto& icPoints = nativeCode->getInlineCachePoints();
    
    // 各ICポイントを初期化
    for (const auto& icPoint : icPoints) {
        switch (icPoint.type) {
            case InlineCacheType::PropertyAccess:
                icManager->initializePropertyCache(nativeCode, icPoint);
                break;
                
            case InlineCacheType::MethodCall:
                icManager->initializeMethodCache(nativeCode, icPoint);
                break;
                
            case InlineCacheType::Instanceof:
                icManager->initializeInstanceofCache(nativeCode, icPoint);
                break;
                
            case InlineCacheType::TypeCheck:
                icManager->initializeTypeCheckCache(nativeCode, icPoint);
                break;
                
            default:
                // 未知のICタイプは無視
                break;
        }
    }
}

void OptimizingJIT::reset() {
    m_irFunction.reset();
    m_typeGuards.clear();
    
    // 統計情報はリセットしない
    m_irOptimizer->reset();
    m_functionId = 0;
}

const OptimizingJIT::Statistics& OptimizingJIT::getStatistics() const {
    return m_stats;
}

void OptimizingJIT::setError(const std::string& message) {
    m_lastError = message;
    
    if (m_context) {
        // エラーログを残す
        m_context->logError("[OptimizingJIT] %s", message.c_str());
    }
}

const std::string& OptimizingJIT::getLastError() const {
    return m_lastError;
}

bool OptimizingJIT::handleDeoptimization(uint64_t functionId, const DeoptimizationInfo& info) {
    if (!m_context || !m_deoptimizer) {
        return false;
    }
    
    // デオプティマイズ回数を増加
    m_deoptimizationCount++;
    m_stats.totalDeoptimizations++;
    
    // デオプティマイザを使用して状態を復元
    bool result = m_deoptimizer->handleDeoptimization(functionId, info);
    
    // プロファイラにデオプティマイズを通知
    if (m_profiler) {
        m_profiler->recordDeoptimization(functionId, info.reason);
    }
    
    return result;
}

// プロファイルデータ収集の実装
ProfileData* OptimizingJIT::collectProfileData(FunctionObject* function, OptimizationTier tier) {
    if (!m_profiler) {
        return nullptr;
    }
    
    // 関数の実行回数と閾値をチェック
    uint32_t executionCount = m_profiler->getExecutionCount(function->getId());
    uint32_t requiredCount = getOptimizationThreshold(tier);
    
    if (executionCount < requiredCount) {
        // まだ最適化に必要な実行回数に達していない
        return nullptr;
    }
    
    // プロファイルデータを取得
    ProfileData* profileData = m_profiler->getFunctionProfile(function->getId());
    if (!profileData) {
        return nullptr;
    }
    
    // プロファイルデータの品質をチェック
    if (!validateProfileQuality(profileData, tier)) {
        // 品質が不十分な場合は収集継続
        return nullptr;
    }
    
    // タイプフィードバック情報の統合
    integrateTypeFeedback(profileData, function);
    
    // ホットスポット分析を実行
    identifyHotSpots(profileData);
    
    return profileData;
}

// プロファイルデータの品質検証
bool OptimizingJIT::validateProfileQuality(ProfileData* data, OptimizationTier tier) {
    // 最小サンプル数のチェック
    if (data->totalSamples < getMinimumSampleCount(tier)) {
        return false;
    }
    
    // ブランチ予測精度のチェック
    double branchPredictionAccuracy = data->correctBranchPredictions / 
                                     static_cast<double>(data->totalBranches);
    if (branchPredictionAccuracy < 0.7) { // 70%以上の精度が必要
        return false;
    }
    
    // 型情報の安定性チェック
    for (const auto& typeInfo : data->typeProfiles) {
        if (typeInfo.stabilityScore < 0.8) { // 80%以上の安定性が必要
            return false;
        }
    }
    
    return true;
}

// タイプフィードバック情報の統合
void OptimizingJIT::integrateTypeFeedback(ProfileData* data, FunctionObject* function) {
    // バイトコード命令ごとの型情報を収集
    const auto& bytecode = function->getBytecode();
    
    for (size_t pc = 0; pc < bytecode.size(); ) {
        Opcode opcode = static_cast<Opcode>(bytecode[pc]);
        
        // 型センシティブな命令の処理
        switch (opcode) {
            case Opcode::LoadProperty: {
                PropertyAccessProfile* profile = data->getPropertyProfile(pc);
                if (profile && profile->isMonomorphic()) {
                    // 単型アクセスの場合、インライン展開候補としてマーク
                    data->inlineCandidates.push_back({pc, InlineType::PropertyAccess});
                }
                break;
            }
            
            case Opcode::CallFunction: {
                CallSiteProfile* profile = data->getCallProfile(pc);
                if (profile && profile->isMonomorphic()) {
                    // 単型呼び出しの場合、インライン展開候補としてマーク
                    data->inlineCandidates.push_back({pc, InlineType::FunctionCall});
                }
                break;
            }
            
            case Opcode::BinaryOp: {
                ArithmeticProfile* profile = data->getArithmeticProfile(pc);
                if (profile && profile->hasStableTypes()) {
                    // 型が安定している算術演算は特殊化候補
                    data->specializationCandidates.push_back({pc, profile->getCommonTypes()});
                }
                break;
            }
        }
        
        pc += getInstructionLength(opcode);
    }
}

// ホットスポット分析
void OptimizingJIT::identifyHotSpots(ProfileData* data) {
    // 実行頻度の高い基本ブロックを特定
    std::vector<std::pair<uint32_t, double>> blockExecutionRates;
    
    for (const auto& [blockId, execCount] : data->blockExecutionCounts) {
        double rate = execCount / static_cast<double>(data->totalSamples);
        blockExecutionRates.push_back({blockId, rate});
    }
    
    // 実行頻度でソート
    std::sort(blockExecutionRates.begin(), blockExecutionRates.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // 上位20%をホットスポットとしてマーク
    size_t hotSpotCount = std::max(1UL, blockExecutionRates.size() / 5);
    for (size_t i = 0; i < hotSpotCount; ++i) {
        data->hotSpots.insert(blockExecutionRates[i].first);
    }
    
    // ホットループの特定
    identifyHotLoops(data);
}

// ホットループの特定
void OptimizingJIT::identifyHotLoops(ProfileData* data) {
    // ループ検出アルゴリズム（Tarjanのアルゴリズムベース）
    LoopDetector detector(data->controlFlowGraph);
    auto loops = detector.detectLoops();
    
    for (const auto& loop : loops) {
        // ループの実行頻度を計算
        double totalExecutions = 0;
        for (uint32_t blockId : loop.blocks) {
            totalExecutions += data->blockExecutionCounts[blockId];
        }
        
        double averageExecution = totalExecutions / loop.blocks.size();
        double executionRate = averageExecution / data->totalSamples;
        
        // 閾値以上の実行頻度を持つループをホットループとしてマーク
        if (executionRate > 0.1) { // 10%以上の実行時間を占める
            data->hotLoops.push_back({loop, executionRate});
        }
    }
}

// 静的分析による最適化
void OptimizingJIT::performStaticOptimizations(IRFunction& function) {
    // 定数伝播
    ConstantPropagation constProp;
    constProp.optimize(function);
    
    // デッドコード除去
    DeadCodeElimination dce;
    dce.optimize(function);
    
    // 共通部分式の除去
    CommonSubexpressionElimination cse;
    cse.optimize(function);
}

// 分岐の信頼性計算
double OptimizingJIT::calculateBranchConfidence(const BranchProfile& branch) {
    uint64_t total = branch.takenCount + branch.notTakenCount;
    if (total == 0) return 0.0;
    
    double ratio = std::max(branch.takenCount, branch.notTakenCount) / static_cast<double>(total);
    return ratio; // 0.5(完全にランダム) から 1.0(完全に予測可能) の範囲
}

// 型ガードの挿入
void OptimizingJIT::insertTypeGuard(IRFunction& function, uint32_t variableIndex, JSValueType expectedType) {
    // 変数の使用箇所を特定
    auto usageSites = function.findVariableUsageSites(variableIndex);
    
    for (auto site : usageSites) {
        // 型チェック命令を挿入
        IRInstruction typeCheck;
        typeCheck.opcode = IROpcode::TypeGuard;
        typeCheck.operands.push_back(IROperand::CreateVariable(variableIndex));
        typeCheck.operands.push_back(IROperand::CreateImmediate(static_cast<int32_t>(expectedType)));
        
        function.insertInstructionBefore(site, typeCheck);
    }
}

// ホットブロック最適化の適用
void OptimizingJIT::applyHotBlockOptimizations(IRFunction& function, uint32_t blockId) {
    auto* block = function.getBasicBlock(blockId);
    if (!block) return;
    
    // インライン展開の候補検討
    for (auto& instruction : block->instructions) {
        if (instruction->opcode == IROpcode::Call) {
            considerInlining(function, *instruction);
        }
    }
    
    // レジスタ割り当ての優先度向上
    block->setRegisterAllocationPriority(RegistrPriority::High);
}

// ループ最適化の適用
void OptimizingJIT::applyLoopOptimizations(IRFunction& function, const LoopProfile& loopProfile) {
    auto* loop = function.getLoop(loopProfile.loopId);
    if (!loop) return;
    
    // ループ不変式の移動
    LoopInvariantCodeMotion licm;
    licm.optimize(*loop);
    
    // ループ展開の検討
    if (loopProfile.averageIterationCount < m_maxUnrollIterations) {
        LoopUnrolling unroller;
        unroller.unroll(*loop, calculateUnrollFactor(loopProfile));
    }
    
    // ベクトル化の検討
    if (canVectorize(*loop)) {
        LoopVectorizer vectorizer;
        vectorizer.vectorize(*loop);
    }
}

// インライン展開の検討
void OptimizingJIT::considerInlining(IRFunction& function, IRInstruction& callInstruction) {
    // 呼び出し先関数の特定
    if (callInstruction.operands.empty()) return;
    
    auto* targetFunction = resolveCallTarget(callInstruction);
    if (!targetFunction) return;
    
    // インライン化の判断基準
    if (shouldInline(*targetFunction)) {
        InlineFunctionOptimization inliner;
        inliner.inlineFunction(function, callInstruction, *targetFunction);
    }
}

// 展開係数の計算
uint32_t OptimizingJIT::calculateUnrollFactor(const LoopProfile& loopProfile) {
    // 反復回数とコードサイズに基づいて展開係数を決定
    uint32_t avgIterations = loopProfile.averageIterationCount;
    uint32_t bodySize = loopProfile.bodyInstructionCount;
    
    if (bodySize < 10) {
        return std::min(avgIterations, 8u);
    } else if (bodySize < 20) {
        return std::min(avgIterations, 4u);
    } else {
        return std::min(avgIterations, 2u);
    }
}

// ベクトル化可能性の判定
bool OptimizingJIT::canVectorize(const IRLoop& loop) {
    // 依存性解析を実行
    DependencyAnalyzer analyzer;
    auto dependencies = analyzer.analyze(loop);
    
    // データ依存性がベクトル化を阻害しないかチェック
    return !dependencies.hasLoopCarriedDependencies();
}

// インライン化の判断
bool OptimizingJIT::shouldInline(const IRFunction& targetFunction) {
    // 関数サイズの制限
    if (targetFunction.getInstructionCount() > m_maxInlineInstructions) {
        return false;
    }
    
    // 再帰呼び出しは避ける
    if (targetFunction.isRecursive()) {
        return false;
    }
    
    // 呼び出し頻度が高い場合はインライン化
    return true;
}

// 呼び出し先の解決
IRFunction* OptimizingJIT::resolveCallTarget(const IRInstruction& callInstruction) {
    // 呼び出し先関数の解決ロジックを完璧に実装
    if (callInstruction.operands.empty()) {
        return nullptr;
    }
    
    // 直接関数呼び出しの場合
    if (callInstruction.operands[0].isFunction()) {
        uint64_t functionId = callInstruction.operands[0].getFunctionId();
        
        // JITコンパイル済み関数テーブルから検索
        auto it = m_compiledFunctions.find(functionId);
        if (it != m_compiledFunctions.end()) {
            return it->second->getIRFunction();
        }
        
        // モジュールの関数テーブルから検索
        if (m_module && m_module->hasFunction(functionId)) {
            auto* funcObj = m_module->getFunction(functionId);
            if (funcObj && funcObj->hasIRRepresentation()) {
                return funcObj->getIRFunction();
            }
        }
        
        // ランタイム関数テーブルから検索
        if (m_runtime && m_runtime->getFunctionTable()) {
            auto* funcTable = m_runtime->getFunctionTable();
            auto* funcObj = funcTable->getFunction(functionId);
            if (funcObj) {
                // IRが存在しない場合はバイトコードから生成
                if (!funcObj->hasIRRepresentation()) {
                    auto* bytecode = funcObj->getBytecode();
                    if (bytecode) {
                        auto irGen = std::make_unique<IRGenerator>();
                        auto ir = irGen->generateFromBytecode(*bytecode);
                        funcObj->setIRFunction(std::move(ir));
                    }
                }
                return funcObj->getIRFunction();
            }
        }
    }
    
    // 間接関数呼び出しの場合（関数ポインタ経由）
    else if (callInstruction.operands[0].isVariable()) {
        uint32_t varIndex = callInstruction.operands[0].getVariableIndex();
        
        // プロファイル情報から呼び出し先を推定
        if (m_profileData) {
            auto callSiteProfile = m_profileData->getCallSiteProfile(varIndex);
            if (callSiteProfile && callSiteProfile->hasMonomorphicTarget()) {
                uint64_t targetFunctionId = callSiteProfile->getMostFrequentTarget();
                return resolveDirectFunction(targetFunctionId);
            }
            
            // ポリモーフィック呼び出しの場合、最も頻度の高いターゲットを返す
            if (callSiteProfile && callSiteProfile->getTargetCount() <= 3) {
                auto targets = callSiteProfile->getFrequentTargets(3);
                if (!targets.empty()) {
                    return resolveDirectFunction(targets[0].functionId);
                }
            }
        }
        
        // 型情報から推定
        if (m_typeInference) {
            auto varType = m_typeInference->getVariableType(varIndex);
            if (varType.isFunctionType() && varType.hasKnownTarget()) {
                return resolveDirectFunction(varType.getTargetFunctionId());
            }
        }
    }
    
    // ビルトイン関数の場合
    else if (callInstruction.operands[0].isBuiltinFunction()) {
        auto builtinId = callInstruction.operands[0].getBuiltinId();
        return getBuiltinFunctionIR(builtinId);
    }
    
    // メソッド呼び出しの場合
    else if (callInstruction.operands[0].isMethodCall()) {
        auto* receiver = getReceiverObject(callInstruction);
        auto methodName = getMethodName(callInstruction);
        
        if (receiver && !methodName.empty()) {
            // オブジェクトの型からメソッドを解決
            auto* objectType = receiver->getType();
            if (objectType) {
                auto* method = objectType->getMethod(methodName);
                if (method && method->hasIRRepresentation()) {
                    return method->getIRFunction();
                }
            }
            
            // プロトタイプチェーンを辿ってメソッドを検索
            auto* prototype = receiver->getPrototype();
            while (prototype) {
                auto* method = prototype->getMethod(methodName);
                if (method && method->hasIRRepresentation()) {
                    return method->getIRFunction();
                }
                prototype = prototype->getPrototype();
            }
        }
    }
    
    return nullptr;
}

}  // namespace core
}  // namespace aerojs