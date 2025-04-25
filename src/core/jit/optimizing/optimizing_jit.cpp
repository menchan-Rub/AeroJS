#include "optimizing_jit.h"
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <iostream>

#include "../ir/ir_optimizer.h"
#include "../ir/type_specializer.h"
#include "../deoptimizer/deoptimizer.h"

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

namespace aerojs::core {

OptimizingJIT::OptimizingJIT()
    : m_optimizationLevel(OptimizationLevel::O2)
    , m_profiler(nullptr)
    , m_irFunction(nullptr)
    , m_irBuilder(std::make_unique<IRBuilder>())
    , m_typeSpecializer(std::make_unique<TypeSpecializer>())
    , m_totalCompilationTimeMs(0)
    , m_irGenerationTimeMs(0)
    , m_optimizationTimeMs(0)
    , m_codeGenTimeMs(0)
    , m_inliningCount(0)
    , m_loopUnrollingCount(0)
    , m_deoptimizationCount(0)
    , m_irOptimizer()
    , m_functionId(0)
{
    // 最適化パスを初期化
    AddOptimizationPass("DeadCodeElimination");
    AddOptimizationPass("ConstantFolding");
    AddOptimizationPass("CommonSubexpressionElimination");
    AddOptimizationPass("Inlining");
    AddOptimizationPass("LoopInvariantCodeMotion");
    AddOptimizationPass("LoopUnrolling");
    AddOptimizationPass("GlobalValueNumbering");
    AddOptimizationPass("TypeSpecialization");
    AddOptimizationPass("RegisterAllocation");
    AddOptimizationPass("TailCallElimination");
    AddOptimizationPass("InstructionSelection");
    AddOptimizationPass("Peephole");
    
    // デフォルトではすべての最適化パスを有効にする
    for (const auto& pass : m_optimizationPasses) {
        m_enabledPasses[pass.name] = true;
    }
}

OptimizingJIT::~OptimizingJIT() = default;

std::unique_ptr<uint8_t[]> OptimizingJIT::Compile(const std::vector<uint8_t>& bytecodes, size_t* outCodeSize) {
    // デフォルトのオプションでコンパイル
    CompileOptions options;
    
    // プロファイルデータを取得（存在する場合）
    if (m_profiler && m_functionId != 0) {
        options.profileData = m_profiler->GetFunctionProfile(m_functionId);
    }
    
    // 最適化レベルに応じてオプションを調整
    switch (m_optimizationLevel) {
        case OptimizationLevel::O0: // 最小限の最適化
            options.enableSpeculation = false;
            options.enableInlining = false;
            options.enableLoopOptimization = false;
            options.enableDeadCodeElimination = false;
            options.enableTypeSpecialization = false;
            options.enableDeoptimizationSupport = false;
            break;
            
        case OptimizationLevel::O1: // 低い最適化
            options.enableSpeculation = false;
            options.enableInlining = true;
            options.enableLoopOptimization = false;
            options.enableTypeSpecialization = true;
            options.maxInliningDepth = 1;
            break;
            
        case OptimizationLevel::O2: // 中程度の最適化 (デフォルト)
            // デフォルト値を使用
            break;
            
        case OptimizationLevel::O3: // 高い最適化
            options.maxInliningDepth = 5;
            options.inliningCallCountThreshold = 5;
            options.maxInlinableFunctionSize = 200;
            break;
            
        default:
            // デフォルト値を使用
            break;
    }
    
    return CompileWithOptions(bytecodes, options, outCodeSize);
}

std::unique_ptr<uint8_t[]> OptimizingJIT::CompileWithOptions(const std::vector<uint8_t>& bytecodes, 
                                                          const CompileOptions& options, 
                                                          size_t* outCodeSize) {
    // コンパイル開始時間を記録
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        // 中間表現（IR）の生成
        m_irBuilder.Reset();
        std::unique_ptr<IRFunction> irFunction = m_irBuilder.BuildIR(bytecodes, m_functionId);
        
        if (!irFunction) {
            // IR生成に失敗
            if (outCodeSize) *outCodeSize = 0;
            return nullptr;
        }
        
        // プロファイルデータを使って最適化を適用
        std::unique_ptr<IRFunction> optimizedFunction = OptimizeIR(std::move(irFunction), options);
        
        if (!optimizedFunction) {
            // 最適化に失敗
            if (outCodeSize) *outCodeSize = 0;
            return nullptr;
        }
        
        // マシンコードの生成
        std::unique_ptr<uint8_t[]> machineCode;
        
#ifdef __x86_64__
        X86_64CodeGenerator codeGenerator(std::move(optimizedFunction));
        machineCode = codeGenerator.Generate(outCodeSize);
#elif defined(__aarch64__)
        ARM64CodeGenerator codeGenerator(std::move(optimizedFunction));
        machineCode = codeGenerator.Generate(outCodeSize);
#elif defined(__riscv)
        RISCVCodeGenerator codeGenerator(std::move(optimizedFunction));
        machineCode = codeGenerator.Generate(outCodeSize);
#else
        // サポートされていないアーキテクチャ
        if (outCodeSize) *outCodeSize = 0;
        return nullptr;
#endif

        // 統計情報の更新
        m_stats.totalCompilations++;
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
        m_stats.totalCompilationTimeNs += duration.count();
        
        return machineCode;
    } catch (const std::exception& e) {
        // コンパイル中にエラーが発生
        std::cerr << "OptimizingJIT::Compile error: " << e.what() << std::endl;
        if (outCodeSize) *outCodeSize = 0;
        return nullptr;
    }
}

void OptimizingJIT::Reset() {
    m_irFunction.reset();
    m_typeGuards.clear();
    
    // 統計情報はリセットしない
    m_irOptimizer.Reset();
    m_functionId = 0;
}

void OptimizingJIT::SetOptimizationLevel(OptimizationLevel level) {
    m_optimizationLevel = level;
    
    // 最適化レベルに応じて有効にするパスを調整
    switch (level) {
        case OptimizationLevel::O0:
            // 基本的な最適化のみ有効
            for (auto& [name, enabled] : m_enabledPasses) {
                enabled = (name == "DeadCodeElimination" || name == "RegisterAllocation");
            }
            break;
            
        case OptimizationLevel::O1:
            // 基本的な最適化と一部の追加最適化を有効
            for (auto& [name, enabled] : m_enabledPasses) {
                enabled = (name == "DeadCodeElimination" || 
                          name == "ConstantFolding" ||
                          name == "RegisterAllocation" ||
                          name == "Peephole");
            }
            break;
            
        case OptimizationLevel::O2:
            // デフォルト - ほとんどの最適化を有効
            for (auto& [name, enabled] : m_enabledPasses) {
                enabled = (name != "LoopUnrolling"); // ループアンローリングは無効
            }
            break;
            
        case OptimizationLevel::O3:
            // すべての最適化を有効
            for (auto& [name, enabled] : m_enabledPasses) {
                enabled = true;
            }
            break;
    }
}

OptimizationLevel OptimizingJIT::GetOptimizationLevel() const {
    return m_optimizationLevel;
}

void OptimizingJIT::SetProfiler(std::shared_ptr<JITProfiler> profiler) {
    m_profiler = profiler;
}

std::shared_ptr<JITProfiler> OptimizingJIT::GetProfiler() const {
    return m_profiler;
}

void OptimizingJIT::EnableOptimizationPass(const std::string& passName, bool enable) {
    if (m_enabledPasses.find(passName) != m_enabledPasses.end()) {
        m_enabledPasses[passName] = enable;
    }
}

bool OptimizingJIT::IsOptimizationPassEnabled(const std::string& passName) const {
    auto it = m_enabledPasses.find(passName);
    if (it != m_enabledPasses.end()) {
        return it->second;
    }
    return false;
}

void OptimizingJIT::AddOptimizationPass(const std::string& passName) {
    // 同名のパスが既に存在するか確認
    for (const auto& pass : m_optimizationPasses) {
        if (pass.name == passName) {
            return; // すでに追加済み
        }
    }
    
    // 新しいパスを追加
    OptimizationPassInfo newPass;
    newPass.name = passName;
    newPass.enabled = true;
    newPass.executionTimeMs = 0;
    newPass.bytesReduced = 0;
    newPass.instructionsEliminated = 0;
    
    m_optimizationPasses.push_back(newPass);
    m_enabledPasses[passName] = true;
}

std::vector<OptimizationPassInfo> OptimizingJIT::GetOptimizationPassInfo() const {
    return m_optimizationPasses;
}

void OptimizingJIT::AddTypeGuard(uint32_t bytecodeOffset, TypeFeedbackRecord::TypeCategory expectedType) {
    // 同じオフセットの既存のガードを検索
    for (auto& guard : m_typeGuards) {
        if (guard.offset == bytecodeOffset) {
            // 既存のガードを更新
            guard.expectedType = expectedType;
            guard.isValidated = false;
            return;
        }
    }
    
    // 新しいガードを追加
    OptimizationTypeFeedback guard;
    guard.offset = bytecodeOffset;
    guard.expectedType = expectedType;
    guard.isValidated = false;
    
    m_typeGuards.push_back(guard);
}

const std::vector<OptimizationTypeFeedback>& OptimizingJIT::GetTypeGuards() const {
    return m_typeGuards;
}

bool OptimizingJIT::HandleDeoptimization(uint32_t bytecodeOffset, const std::string& reason) {
    // 逆最適化情報を記録
    m_deoptimizationCount++;
    
    // 該当するオフセットの型ガードを検索
    for (auto& guard : m_typeGuards) {
        if (guard.offset == bytecodeOffset) {
            // ガードを無効化
            guard.isValidated = false;
        }
    }
    
    // 逆最適化情報を追加
    DeoptimizationInfo info;
    info.bytecodeOffset = bytecodeOffset;
    info.reason = reason;
    info.count = 1;
    
    // 既存の逆最適化情報を検索
    for (auto& existing : m_deoptimizationInfo) {
        if (existing.bytecodeOffset == bytecodeOffset && existing.reason == reason) {
            existing.count++;
            return true;
        }
    }
    
    // 新しい逆最適化情報を追加
    m_deoptimizationInfo.push_back(info);
    return true;
}

void OptimizingJIT::DumpGeneratedIR(std::ostream& stream) const {
    if (!m_irFunction) {
        stream << "IR関数が生成されていません。\n";
        return;
    }
    
    stream << "生成されたIR命令:\n";
    
    const auto& instructions = m_irFunction->GetInstructions();
    for (size_t i = 0; i < instructions.size(); i++) {
        const auto& inst = instructions[i];
        stream << i << ": " << static_cast<int>(inst.opcode) << " [";
        
        for (size_t j = 0; j < inst.args.size(); j++) {
            stream << inst.args[j];
            if (j < inst.args.size() - 1) {
                stream << ", ";
            }
        }
        
        stream << "]\n";
    }
}

void OptimizingJIT::DumpOptimizedIR(std::ostream& stream) const {
    DumpGeneratedIR(stream);
    
    // 最適化情報も出力
    stream << "\n最適化情報:\n";
    for (const auto& pass : m_optimizationPasses) {
        if (pass.enabled) {
            stream << pass.name << ": " 
                  << "実行時間 " << pass.executionTimeMs << "ms, "
                  << "削減バイト数 " << pass.bytesReduced << ", "
                  << "削減命令数 " << pass.instructionsEliminated << "\n";
        }
    }
}

void OptimizingJIT::DumpAssembly(std::ostream& stream) const {
    // 生成されたマシンコードのアセンブリを出力
    // この実装は省略
    stream << "アセンブリ出力機能は実装されていません。\n";
}

uint32_t OptimizingJIT::GetTotalCompilationTimeMs() const {
    return m_totalCompilationTimeMs;
}

uint32_t OptimizingJIT::GetTotalOptimizationTimeMs() const {
    return m_optimizationTimeMs;
}

uint32_t OptimizingJIT::GetTotalCodeGenTimeMs() const {
    return m_codeGenTimeMs;
}

uint32_t OptimizingJIT::GetTotalInliningCount() const {
    return m_inliningCount;
}

uint32_t OptimizingJIT::GetTotalLoopUnrollingCount() const {
    return m_loopUnrollingCount;
}

void OptimizingJIT::GenerateInitialIR(const std::vector<uint8_t>& bytecodes) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // IRビルダーを使用してバイトコードからIRを生成
    // 実際の実装では、ここでIRを構築する詳細なロジックを記述
    m_irFunction = m_irBuilder->BuildIR(bytecodes, 0);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    m_irGenerationTimeMs += static_cast<uint32_t>(duration);
}

void OptimizingJIT::RunOptimizationPasses() {
    if (!m_irFunction) return;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 有効な最適化パスを実行
    if (IsOptimizationPassEnabled("DeadCodeElimination")) OptimizeDeadCodeElimination();
    if (IsOptimizationPassEnabled("ConstantFolding")) OptimizeConstantFolding();
    if (IsOptimizationPassEnabled("CommonSubexpressionElimination")) OptimizeCommonSubexpressionElimination();
    if (IsOptimizationPassEnabled("Inlining")) OptimizeInlining();
    if (IsOptimizationPassEnabled("LoopInvariantCodeMotion")) OptimizeLoopInvariantCodeMotion();
    if (IsOptimizationPassEnabled("LoopUnrolling")) OptimizeLoopUnrolling();
    if (IsOptimizationPassEnabled("GlobalValueNumbering")) OptimizeGlobalValueNumbering();
    if (IsOptimizationPassEnabled("TypeSpecialization")) OptimizeTypeSpecialization();
    if (IsOptimizationPassEnabled("RegisterAllocation")) OptimizeRegisterAllocation();
    if (IsOptimizationPassEnabled("TailCallElimination")) OptimizeTailCallElimination();
    if (IsOptimizationPassEnabled("InstructionSelection")) OptimizeInstructionSelection();
    if (IsOptimizationPassEnabled("Peephole")) OptimizePeephole();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    m_optimizationTimeMs += static_cast<uint32_t>(duration);
}

void OptimizingJIT::GenerateMachineCode(std::unique_ptr<uint8_t[]>& outCode, size_t* outCodeSize) {
    if (!m_irFunction) {
        if (outCodeSize) *outCodeSize = 0;
        return;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // IR関数からターゲットアーキテクチャ用のマシンコードを生成
    // 実際の実装では、アーキテクチャ固有のコードジェネレータを使用
    
    // ダミーのコード生成（実装例）
    *outCodeSize = 100;  // 仮の値
    outCode = std::make_unique<uint8_t[]>(*outCodeSize);
    
    // コードを0で初期化
    std::fill_n(outCode.get(), *outCodeSize, 0);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    m_codeGenTimeMs += static_cast<uint32_t>(duration);
}

// 各最適化パスの実装
void OptimizingJIT::OptimizeDeadCodeElimination() {
    // 実装例：到達不可能なコードを削除
    if (!m_irFunction) return;
    
    // 開始時間を記録
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 命令数を記録
    size_t beforeCount = m_irFunction->GetInstructionCount();
    
    // 実際の最適化処理はここに実装
    // ...
    
    // 終了時間を記録
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    // 結果を記録
    size_t afterCount = m_irFunction->GetInstructionCount();
    size_t eliminatedCount = beforeCount > afterCount ? beforeCount - afterCount : 0;
    
    // 統計情報を更新
    for (auto& pass : m_optimizationPasses) {
        if (pass.name == "DeadCodeElimination") {
            pass.executionTimeMs += static_cast<uint32_t>(duration);
            pass.instructionsEliminated += static_cast<uint32_t>(eliminatedCount);
            break;
        }
    }
}

void OptimizingJIT::OptimizeConstantFolding() {
    // 定数畳み込み最適化の実装
    // コンパイル時に計算可能な式を畳み込む
}

void OptimizingJIT::OptimizeCommonSubexpressionElimination() {
    // 共通部分式の削除最適化の実装
    // 同じ計算を複数回行わないようにする
}

void OptimizingJIT::OptimizeInlining() {
    // インライン化最適化の実装
    // 小さな関数を呼び出し元に展開する
    
    // プロファイラからの情報を利用
    if (m_profiler) {
        // インライン化すべき関数を特定
        // ...
        
        // インライン化を実行
        // ...
        
        // インライン化カウントを増加
        m_inliningCount++;
    }
}

void OptimizingJIT::OptimizeLoopInvariantCodeMotion() {
    // ループ不変コード移動最適化の実装
    // ループ内の変化しない計算をループの外に移動
}

void OptimizingJIT::OptimizeLoopUnrolling() {
    // ループ展開最適化の実装
    // ループの反復を複数回展開してオーバーヘッドを減らす
    
    // プロファイラからのループ情報を利用
    if (m_profiler) {
        // 展開すべきループを特定
        // ...
        
        // ループ展開を実行
        // ...
        
        // ループ展開カウントを増加
        m_loopUnrollingCount++;
    }
}

void OptimizingJIT::OptimizeGlobalValueNumbering() {
    // グローバル値番号付け最適化の実装
    // 冗長な計算を広範囲で検出して削除
}

void OptimizingJIT::OptimizeTypeSpecialization() {
    // 型特化最適化の実装
    // 型情報を利用して特化したコードを生成
    
    // タイプガードを使用
    for (const auto& guard : m_typeGuards) {
        // IRに型チェックと特化コードを挿入
        m_typeSpecializer->AddTypeGuard(m_irFunction.get(), guard.offset, guard.expectedType);
    }
}

void OptimizingJIT::OptimizeRegisterAllocation() {
    // レジスタ割り当て最適化の実装
    // 変数をレジスタに効率的に割り当てる
}

void OptimizingJIT::OptimizeTailCallElimination() {
    // 末尾呼び出し最適化の実装
    // 関数の末尾での呼び出しをジャンプに変換
}

void OptimizingJIT::OptimizeInstructionSelection() {
    // 命令選択最適化の実装
    // ターゲットアーキテクチャに最適な命令を選択
}

void OptimizingJIT::OptimizePeephole() {
    // ピープホール最適化の実装
    // 局所的なパターンを最適なコードに置き換え
}

bool OptimizingJIT::ShouldInlineFunction(uint32_t functionId) const {
    // インライン化すべき関数かどうかを判断
    if (!m_profiler) return false;
    
    const FunctionProfile* profile = m_profiler->GetFunctionProfile(functionId);
    if (!profile) return false;
    
    // 小さな関数や頻繁に呼ばれる関数をインライン化
    return (profile->bytecodeSize < 100 || profile->executionCount > 1000);
}

bool OptimizingJIT::ShouldUnrollLoop(uint32_t loopHeaderOffset) const {
    // ループを展開すべきかどうかを判断
    // プロファイル情報を利用して決定
    return false;  // 実装なし
}

bool OptimizingJIT::IsHotCode(uint32_t bytecodeOffset) const {
    // ホットなコード（頻繁に実行される部分）かどうかを判断
    // プロファイル情報を利用して決定
    return false;  // 実装なし
}

std::unique_ptr<IRFunction> OptimizingJIT::OptimizeIR(
    std::unique_ptr<IRFunction> irFunction, 
    const CompileOptions& options) 
{
    if (!irFunction) {
        return nullptr;
    }
    
    // 元のIR関数のコピーを作成
    const IRFunction* originalIRFunction = irFunction.get();
    
    // 型特化の適用
    if (options.enableTypeSpecialization && options.profileData) {
        ApplyTypeSpecialization(irFunction.get(), options.profileData);
    }
    
    // ホットループに対する最適化
    if (options.enableLoopOptimization && options.profileData) {
        OptimizeHotLoops(irFunction.get(), options.profileData);
    }
    
    // インライン化の適用
    if (options.enableInlining && options.profileData) {
        ApplyInlining(irFunction.get(), options.profileData, options);
    }
    
    // 推測的最適化のためのガードを挿入
    if (options.enableSpeculation && options.enableDeoptimizationSupport && options.profileData) {
        InsertSpeculationGuards(irFunction.get(), options.profileData);
    }
    
    // 標準的な最適化パスを実行
    m_irOptimizer.SetFunction(irFunction.get());
    
    // デッドコード除去
    if (options.enableDeadCodeElimination) {
        m_irOptimizer.RunDeadCodeElimination();
    }
    
    // 定数畳み込み
    m_irOptimizer.RunConstantFolding();
    
    // 共通部分式の削除
    m_irOptimizer.RunCommonSubexpressionElimination();
    
    // ループ不変コード移動
    if (options.enableLoopOptimization) {
        m_irOptimizer.RunLoopInvariantCodeMotion();
    }
    
    // 命令選択の最適化
    m_irOptimizer.RunInstructionSelection();
    
    // 最後に再度デッドコード除去を実行
    if (options.enableDeadCodeElimination) {
        m_irOptimizer.RunDeadCodeElimination();
    }
    
    // 最適化統計情報を記録
    RecordOptimizationStats(originalIRFunction, irFunction.get(), options);
    
    return irFunction;
}

void OptimizingJIT::ApplyTypeSpecialization(
    IRFunction* irFunction,
    const FunctionProfile* profile) 
{
    if (!irFunction || !profile) {
        return;
    }
    
    uint32_t specializedTypes = 0;
    
    // すべての変数に対して型特化を試みる
    for (uint32_t variableIndex = 0; variableIndex < profile->typeFeedback.size(); ++variableIndex) {
        const auto& typeFeedback = profile->typeFeedback[variableIndex];
        
        // 型情報の信頼性が高い場合のみ特化を適用
        if (typeFeedback.category != TypeFeedbackRecord::TypeCategory::Unknown && 
            typeFeedback.observationCount > 10 && 
            typeFeedback.confidence > 0.9f) {
            
            bool specialized = m_typeSpecializer->SpecializeVariable(
                irFunction, 
                variableIndex, 
                typeFeedback.category,
                typeFeedback.hasNegativeZero,
                typeFeedback.hasNaN);
                
            if (specialized) {
                specializedTypes++;
            }
        }
    }
    
    // 算術演算の特化
    for (const auto& [offset, typeFeedback] : profile->arithmeticOperations) {
        if (typeFeedback.category != TypeFeedbackRecord::TypeCategory::Unknown && 
            typeFeedback.observationCount > 5 && 
            typeFeedback.confidence > 0.8f) {
            
            bool specialized = m_typeSpecializer->SpecializeArithmeticOperation(
                irFunction, 
                offset, 
                typeFeedback.category);
                
            if (specialized) {
                specializedTypes++;
            }
        }
    }
    
    // 比較演算の特化
    for (const auto& [offset, typeFeedback] : profile->comparisonOperations) {
        if (typeFeedback.category != TypeFeedbackRecord::TypeCategory::Unknown && 
            typeFeedback.observationCount > 5 && 
            typeFeedback.confidence > 0.8f) {
            
            bool specialized = m_typeSpecializer->SpecializeComparisonOperation(
                irFunction, 
                offset, 
                typeFeedback.category);
                
            if (specialized) {
                specializedTypes++;
            }
        }
    }
    
    m_stats.specializedTypes += specializedTypes;
}

void OptimizingJIT::OptimizeHotLoops(
    IRFunction* irFunction,
    const FunctionProfile* profile) 
{
    if (!irFunction || !profile) {
        return;
    }
    
    uint32_t optimizedLoops = 0;
    
    // ホットループを検出して最適化
    for (const auto& [loopHeader, counter] : profile->loopExecutionCounts) {
        // 実行回数が十分に多いループを最適化
        if (counter.executionCount > 100) {
            bool shouldUnroll = counter.averageIterations > 2 && counter.averageIterations < 10;
            
            if (shouldUnroll) {
                // ループ展開を適用
                bool unrolled = m_irOptimizer.UnrollLoop(irFunction, loopHeader, static_cast<uint32_t>(counter.averageIterations));
                if (unrolled) {
                    optimizedLoops++;
                }
            } else {
                // ループ不変コード移動を適用
                bool hoisted = m_irOptimizer.HoistLoopInvariants(irFunction, loopHeader);
                if (hoisted) {
                    optimizedLoops++;
                }
            }
        }
    }
    
    m_stats.optimizedLoops += optimizedLoops;
}

void OptimizingJIT::ApplyInlining(
    IRFunction* irFunction,
    const FunctionProfile* profile,
    const CompileOptions& options)
{
    if (!irFunction || !profile || !m_profiler) {
        return;
    }
    
    uint32_t inlinedFunctions = 0;
    
    // 呼び出しサイトを検討してインライン化
    for (const auto& [callSite, counter] : profile->callSiteExecutionCounts) {
        // 十分な実行回数の呼び出しサイトのみを考慮
        if (counter.executionCount > options.inliningCallCountThreshold) {
            const auto& targetInfo = counter.mostCommonTarget;
            
            // 最も頻繁に呼び出される関数のプロファイルを取得
            uint32_t targetFunctionId = targetInfo.first;
            uint32_t callCount = targetInfo.second;
            
            // 関数が十分な回数呼び出され、インライン化の深さが上限以下の場合
            if (callCount > options.inliningCallCountThreshold / 2 && 
                m_irBuilder.GetInliningDepth() < options.maxInliningDepth) {
                
                // 対象関数のバイトコードを取得
                auto targetBytecodes = m_profiler->GetFunctionBytecodes(targetFunctionId);
                
                // サイズが十分に小さい場合のみインライン化
                if (targetBytecodes && targetBytecodes->size() <= options.maxInlinableFunctionSize) {
                    bool inlined = m_irBuilder.InlineFunction(
                        irFunction, 
                        callSite, 
                        targetFunctionId, 
                        *targetBytecodes);
                        
                    if (inlined) {
                        inlinedFunctions++;
                    }
                }
            }
        }
    }
    
    m_stats.inlinedFunctions += inlinedFunctions;
}

void OptimizingJIT::InsertSpeculationGuards(
    IRFunction* irFunction, 
    const FunctionProfile* profile)
{
    if (!irFunction || !profile) {
        return;
    }
    
    uint32_t insertedGuards = 0;
    
    // 変数の型に関するガードを挿入
    for (uint32_t variableIndex = 0; variableIndex < profile->typeFeedback.size(); ++variableIndex) {
        const auto& typeFeedback = profile->typeFeedback[variableIndex];
        
        // 型情報の信頼性が中程度の場合は推測的に型ガードを挿入
        if (typeFeedback.category != TypeFeedbackRecord::TypeCategory::Unknown && 
            typeFeedback.observationCount > 5 && 
            typeFeedback.confidence > 0.7f && 
            typeFeedback.confidence < 0.95f) {
            
            bool inserted = m_typeSpecializer->InsertTypeGuard(
                irFunction, 
                variableIndex, 
                typeFeedback.category,
                typeFeedback.hasNegativeZero,
                typeFeedback.hasNaN);
                
            if (inserted) {
                insertedGuards++;
            }
        }
    }
    
    // プロパティアクセスのガードを挿入
    for (const auto& [offset, propertyAccess] : profile->propertyAccesses) {
        // プロパティアクセスの形状が一貫している場合
        if (propertyAccess.shapeObservationCount > 3 && 
            propertyAccess.shapeConsistency > 0.8f) {
            
            bool inserted = m_typeSpecializer->InsertPropertyShapeGuard(
                irFunction, 
                offset, 
                propertyAccess.mostCommonShapeId);
                
            if (inserted) {
                insertedGuards++;
            }
        }
    }
    
    m_stats.insertedGuards += insertedGuards;
}

void OptimizingJIT::RecordOptimizationStats(
    const IRFunction* irFunction,
    const IRFunction* optimizedFunction,
    const CompileOptions& options)
{
    // すでにベーシックな統計情報は各最適化メソッドで更新済み
    
    // デッドコード除去の効果を測定
    if (options.enableDeadCodeElimination && irFunction && optimizedFunction) {
        uint32_t originalInstructionCount = irFunction->GetInstructionCount();
        uint32_t optimizedInstructionCount = optimizedFunction->GetInstructionCount();
        
        if (originalInstructionCount > optimizedInstructionCount) {
            m_stats.eliminatedDeadCode += (originalInstructionCount - optimizedInstructionCount);
        }
    }
    
    // ここで詳細な統計情報をログに出力したり、外部システムに送信したりできる
    // デバッグビルドの場合のみ詳細ログを出力
#ifdef DEBUG
    std::cerr << "OptimizingJIT stats for function " << m_functionId << ":" << std::endl
              << "  Total compilations: " << m_stats.totalCompilations << std::endl
              << "  Inlined functions: " << m_stats.inlinedFunctions << std::endl
              << "  Specialized types: " << m_stats.specializedTypes << std::endl
              << "  Optimized loops: " << m_stats.optimizedLoops << std::endl
              << "  Eliminated dead code: " << m_stats.eliminatedDeadCode << std::endl
              << "  Inserted guards: " << m_stats.insertedGuards << std::endl
              << "  Total compilation time: " << (m_stats.totalCompilationTimeNs / 1000000.0) << " ms" << std::endl;
#endif
}

} // namespace aerojs::core