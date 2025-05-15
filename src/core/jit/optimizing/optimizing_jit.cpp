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
    
    // プロファイルデータを取得（存在する場合）
    if (m_profiler) {
        options.profileData = m_profiler->getFunctionProfile(m_functionId);
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

std::unique_ptr<uint8_t[]> OptimizingJIT::compileWithOptions(const std::vector<uint8_t>& bytecodes, 
                                                             const CompileOptions& options, 
                                                             size_t* outCodeSize) {
    try {
        // IR生成開始時間
        auto irStartTime = std::chrono::high_resolution_clock::now();
        
        // 中間表現（IR）の生成
        m_irBuilder->setContext(m_context);
        m_irBuilder->setProfileData(options.profileData);
        std::unique_ptr<IRFunction> irFunction = m_irBuilder->buildIR(bytecodes, options.functionId);
        
        if (!irFunction) {
            setError("IR生成に失敗しました");
            if (outCodeSize) *outCodeSize = 0;
            return nullptr;
        }
        
        // IR生成時間を記録
        auto irEndTime = std::chrono::high_resolution_clock::now();
        auto irDuration = std::chrono::duration_cast<std::chrono::milliseconds>(irEndTime - irStartTime);
        m_irGenerationTimeMs += irDuration.count();
        
        // 最適化開始時間
        auto optStartTime = std::chrono::high_resolution_clock::now();
        
        // 最適化を適用
        std::unique_ptr<IRFunction> optimizedFunction = optimizeIR(std::move(irFunction), options);
        
        if (!optimizedFunction) {
            setError("IR最適化に失敗しました");
            if (outCodeSize) *outCodeSize = 0;
            return nullptr;
        }
        
        // 最適化時間を記録
        auto optEndTime = std::chrono::high_resolution_clock::now();
        auto optDuration = std::chrono::duration_cast<std::chrono::milliseconds>(optEndTime - optStartTime);
        m_optimizationTimeMs += optDuration.count();
        
        // コード生成開始時間
        auto codeGenStartTime = std::chrono::high_resolution_clock::now();
        
        // マシンコードの生成
        std::unique_ptr<uint8_t[]> machineCode = generateMachineCode(std::move(optimizedFunction), options, outCodeSize);
        
        // コード生成時間を記録
        auto codeGenEndTime = std::chrono::high_resolution_clock::now();
        auto codeGenDuration = std::chrono::duration_cast<std::chrono::milliseconds>(codeGenEndTime - codeGenStartTime);
        m_codeGenTimeMs += codeGenDuration.count();
        
        return machineCode;
    } catch (const std::exception& e) {
        // コンパイル中にエラーが発生
        std::string errorMsg = "コンパイル中に例外が発生しました: ";
        errorMsg += e.what();
        setError(errorMsg);
        
        if (outCodeSize) *outCodeSize = 0;
        return nullptr;
    }
}

std::unique_ptr<IRFunction> OptimizingJIT::optimizeIR(std::unique_ptr<IRFunction> irFunction, 
                                                      const CompileOptions& options) {
    if (!irFunction) {
        return nullptr;
    }
    
    m_typeGuards.clear();
    
    // 最適化パスの実行を前処理パス・中間パス・後処理パスに分ける
    std::unordered_set<std::string> preOptimizationPasses = {
        "DeadCodeElimination",
        "ConstantFolding"
    };
    
    std::unordered_set<std::string> postOptimizationPasses = {
        "RegisterAllocation",
        "InstructionSelection",
        "Peephole"
    };
    
    // 前処理パスの実行
    for (const auto& pass : m_optimizationPasses) {
        if (preOptimizationPasses.find(pass.name) != preOptimizationPasses.end() && 
            isOptimizationPassEnabled(pass.name)) {
            if (!pass.function(irFunction.get())) {
                std::cerr << "前処理最適化パス " << pass.name << " の実行に失敗しました" << std::endl;
            }
        }
    }
    
    // メイン最適化パスの実行
    for (const auto& pass : m_optimizationPasses) {
        if (preOptimizationPasses.find(pass.name) == preOptimizationPasses.end() && 
            postOptimizationPasses.find(pass.name) == postOptimizationPasses.end() && 
            isOptimizationPassEnabled(pass.name)) {
            
            // パスの実行
            bool result = pass.function(irFunction.get());
            
            // 特定のパスの統計情報を更新
            if (pass.name == "Inlining" && result) {
                m_inliningCount += m_irOptimizer->getLastInliningCount();
            } else if (pass.name == "LoopUnrolling" && result) {
                m_loopUnrollingCount += m_irOptimizer->getLastLoopUnrollingCount();
            } else if (pass.name == "TypeSpecialization" && result) {
                // 型ガードをコピー
                const auto& typeGuards = m_typeSpecializer->getTypeGuards();
                m_typeGuards.insert(m_typeGuards.end(), typeGuards.begin(), typeGuards.end());
            }
            
            if (!result) {
                std::cerr << "最適化パス " << pass.name << " の実行に失敗しました" << std::endl;
            }
        }
    }
    
    // 後処理パスの実行
    for (const auto& pass : m_optimizationPasses) {
        if (postOptimizationPasses.find(pass.name) != postOptimizationPasses.end() && 
            isOptimizationPassEnabled(pass.name)) {
            if (!pass.function(irFunction.get())) {
                std::cerr << "後処理最適化パス " << pass.name << " の実行に失敗しました" << std::endl;
            }
        }
    }
    
    return irFunction;
}

std::unique_ptr<uint8_t[]> OptimizingJIT::generateMachineCode(std::unique_ptr<IRFunction> irFunction, 
                                                              const CompileOptions& options,
                                                              size_t* outCodeSize) {
    if (!irFunction || !outCodeSize) {
        if (outCodeSize) *outCodeSize = 0;
        return nullptr;
    }
    
    // ネイティブコードジェネレータの作成
    std::unique_ptr<BaseCodeGenerator> codeGenerator;
    
#ifdef __x86_64__
    codeGenerator = std::make_unique<X86_64CodeGenerator>(m_context, std::move(irFunction));
#elif defined(__aarch64__)
    codeGenerator = std::make_unique<ARM64CodeGenerator>(m_context, std::move(irFunction));
#elif defined(__riscv)
    codeGenerator = std::make_unique<RISCVCodeGenerator>(m_context, std::move(irFunction));
#else
    setError("サポートされていないターゲットアーキテクチャです");
    *outCodeSize = 0;
    return nullptr;
#endif
    
    // デオプティマイズサポートを設定
    if (options.enableDeoptimizationSupport) {
        codeGenerator->setDeoptimizationSupport(true);
        codeGenerator->setTypeGuards(m_typeGuards);
    }
    
    // 最適化レベルを設定
    codeGenerator->setOptimizationLevel(static_cast<int>(m_optimizationLevel));
    
    // マシンコードの生成
    return codeGenerator->generate(outCodeSize);
}

void OptimizingJIT::configureOptionsForOptimizationLevel(CompileOptions& options) {
    // 最適化レベルに応じてオプションを調整
    switch (m_optimizationLevel) {
        case OptimizationLevel::O0: // 最小限の最適化
            options.enableInlining = false;
            options.enableLoopOptimization = false;
            options.enableDeadCodeElimination = true;
            options.enableTypeSpecialization = false;
            options.enableDeoptimizationSupport = false;
            options.maxInliningDepth = 0;
            options.inliningThreshold = 0;
            break;
            
        case OptimizationLevel::O1: // 低い最適化
            options.enableInlining = true;
            options.enableLoopOptimization = false;
            options.enableDeadCodeElimination = true;
            options.enableTypeSpecialization = true;
            options.enableDeoptimizationSupport = true;
            options.maxInliningDepth = 1;
            options.inliningThreshold = 30;
            break;
            
        case OptimizationLevel::O2: // 中程度の最適化 (デフォルト)
            options.enableInlining = true;
            options.enableLoopOptimization = true;
            options.enableDeadCodeElimination = true;
            options.enableTypeSpecialization = true;
            options.enableDeoptimizationSupport = true;
            options.maxInliningDepth = 3;
            options.inliningThreshold = 50;
            break;
            
        case OptimizationLevel::O3: // 高い最適化
            options.enableInlining = true;
            options.enableLoopOptimization = true;
            options.enableDeadCodeElimination = true;
            options.enableTypeSpecialization = true;
            options.enableDeoptimizationSupport = true;
            options.maxInliningDepth = 5;
            options.inliningThreshold = 100;
            break;
            
        default:
            // O2と同じ設定
            options.enableInlining = true;
            options.enableLoopOptimization = true;
            options.enableDeadCodeElimination = true;
            options.enableTypeSpecialization = true;
            options.enableDeoptimizationSupport = true;
            options.maxInliningDepth = 3;
            options.inliningThreshold = 50;
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

}  // namespace core
}  // namespace aerojs