/**
 * @file tracing_jit.cpp
 * @brief メタトレーシングJITコンパイラの実装
 * 
 * ホットなコードパスを検出し、実行時トレースに基づいて最適化コードを生成する
 * 最先端のJITコンパイラシステムです。
 * 
 * @author AeroJS Team
 * @version 3.0.0
 * @copyright MIT License
 */

#include "tracing_jit.h"
#include "trace_optimizer.h"
#include "trace_recorder.h"

#include "../../runtime/context/execution_context.h"
#include "../../vm/interpreter/interpreter.h"
#include "../backend/x86_64/code_generator.h"
#include "../ir/ir_builder.h"
#include "../registers/register_allocator.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <unordered_set>

namespace aerojs {
namespace core {
namespace jit {
namespace metatracing {

using namespace aerojs::core::runtime;
using namespace aerojs::core::vm;
using namespace aerojs::core::jit::registers;
using namespace aerojs::core::jit::backend;

/**
 * @brief コンストラクタ
 * @param context 実行コンテキスト
 */
TracingJIT::TracingJIT(runtime::Context* context)
    : m_context(context),
      m_recorder(std::make_unique<TraceRecorder>(*this)),
      m_optimizer(std::make_unique<TraceOptimizer>(*this)),
      m_enabled(true),
      m_optimizationLevel(OptimizationLevel::O2),
      m_hotThreshold(10),
      m_minTraceExecutions(3),
      m_maxTraceAttempts(5),
      m_maxCompiledTraces(1000),
      m_memoryLimit(100 * 1024 * 1024), // 100MB
      m_usedMemory(0),
      m_nextTraceId(1)
{
    // コンパイルバックエンドを初期化
    #if defined(__x86_64__) || defined(_M_X64)
    m_codeGenerator = std::make_unique<x86_64::CodeGenerator>();
    #else
    // 他のアーキテクチャのサポートは別途追加
    #error "Unsupported architecture"
    #endif
    
    // レジスタアロケータを初期化
    m_registerAllocator = std::make_unique<LinearScanRegisterAllocator>();
    
    // コード格納用の初期メモリを確保
    m_codeBuffer.reserve(1024 * 1024); // 1MB
    
    initializeStatistics();
}

/**
 * @brief デストラクタ
 */
TracingJIT::~TracingJIT() {
    // 確保したネイティブコードメモリを解放
    releaseCompiledCode();
}

/**
 * @brief JITコンパイルとトレース記録を有効/無効にする
 * @param enabled 有効にする場合はtrue
 */
void TracingJIT::setEnabled(bool enabled) {
    m_enabled = enabled;
}

/**
 * @brief JIT最適化レベルを設定
 * @param level 最適化レベル
 */
void TracingJIT::setOptimizationLevel(OptimizationLevel level) {
    m_optimizationLevel = level;
    m_optimizer->setOptimizationLevel(level);
}

/**
 * @brief ホットスポット検出閾値を設定
 * @param threshold 閾値（実行回数）
 */
void TracingJIT::setHotThreshold(uint32_t threshold) {
    m_hotThreshold = threshold;
}

/**
 * @brief バイトコード実行時のJITトレースを取得
 * @param context 実行コンテキスト
 * @param location バイトコード位置
 * @return JITトレース（利用可能な場合）
 */
TracingJIT::CompiledTrace* TracingJIT::getCompileTraceForLocation(
    runtime::execution::ExecutionContext* context,
    const bytecode::BytecodeAddress& location) 
{
    // JITが無効の場合はnullptrを返す
    if (!m_enabled) {
        return nullptr;
    }
    
    // 位置に対応するトレースを検索
    auto it = m_locationToTraceMap.find(location);
    if (it != m_locationToTraceMap.end()) {
        // 見つかったトレースIDに対応するコンパイル済みトレースを取得
        auto trace = getCompiledTrace(it->second);
        if (trace) {
            // トレース実行統計を更新
            trace->executionCount++;
            recordTraceHit(location);
            return trace;
        }
    }
    
    // トレースが見つからなかった場合、ホットスポット検出を行う
    auto entryCount = incrementAndGetEntryCount(location);
    
    // ホットスポットかどうかをチェック
    if (entryCount == m_hotThreshold) {
        // ホットスポットを検出、トレース記録を開始
        startTraceRecording(context, location);
    }
    
    return nullptr;
}

/**
 * @brief コンパイル済みトレースを取得
 * @param traceId トレースID
 * @return コンパイル済みトレース（存在しない場合はnullptr）
 */
TracingJIT::CompiledTrace* TracingJIT::getCompiledTrace(TraceId traceId) {
    auto it = m_compiledTraces.find(traceId);
    if (it != m_compiledTraces.end()) {
        return &it->second;
    }
    return nullptr;
}

/**
 * @brief バイトコード位置のエントリーカウントを増加し、現在値を取得
 * @param location バイトコード位置
 * @return 更新後のエントリーカウント
 */
uint32_t TracingJIT::incrementAndGetEntryCount(const bytecode::BytecodeAddress& location) {
    auto& count = m_entryCountMap[location];
    count++;
    return count;
}

/**
 * @brief トレース記録を開始
 * @param context 実行コンテキスト
 * @param location バイトコード位置
 */
void TracingJIT::startTraceRecording(
    runtime::execution::ExecutionContext* context,
    const bytecode::BytecodeAddress& location) 
{
    // 既に多すぎるトレース記録の試行が行われていないかチェック
    auto& attempts = m_traceAttemptMap[location];
    if (attempts >= m_maxTraceAttempts) {
        return; // 試行回数が多すぎる場合は記録しない
    }
    
    // 記録試行回数を増加
    attempts++;
    
    // トレース記録を開始
    m_recorder->startRecording(context, location);
    
    // 統計情報を更新
    m_statistics.totalTraceAttempts++;
}

/**
 * @brief トレース記録を完了し、最適化して実行可能コードを生成
 * @return 成功した場合はtrue
 */
bool TracingJIT::finishTraceRecording() {
    if (!m_recorder->isRecording()) {
        return false;
    }
    
    // トレース記録を完了
    auto trace = m_recorder->finishRecording();
    if (!trace) {
        return false;
    }
    
    // トレースを検証
    if (!m_optimizer->validateTrace(*trace)) {
        // 無効なトレース
        m_statistics.invalidTraces++;
        return false;
    }
    
    // トレースの最適化を実行
    auto irRoot = m_optimizer->optimizeTrace(*trace);
    if (!irRoot) {
        // 最適化に失敗
        m_statistics.optimizationFailures++;
        return false;
    }
    
    // レジスタ割り当てを実行
    auto allocatedIR = m_registerAllocator->allocateRegisters(irRoot.get());
    if (!allocatedIR) {
        // レジスタ割り当てに失敗
        m_statistics.registerAllocationFailures++;
        return false;
    }
    
    // ネイティブコードを生成
    CompiledTrace compiledTrace;
    compiledTrace.traceId = m_nextTraceId++;
    compiledTrace.entryPoint = trace->entryPoint;
    compiledTrace.exitPoint = trace->exitPoint;
    compiledTrace.executionCount = 0;
    compiledTrace.sideExits.resize(trace->sideExits.size());
    
    // コード生成
    size_t codeSize = 0;
    uint8_t* nativeCode = m_codeGenerator->generateCode(allocatedIR.get(), codeSize);
    
    if (!nativeCode || codeSize == 0) {
        // コード生成に失敗
        m_statistics.codeGenFailures++;
        return false;
    }
    
    // サイドエグジットのパッチポイント情報を収集
    std::vector<size_t> sideExitOffsets;
    for (size_t i = 0; i < trace->sideExits.size(); i++) {
        // IRノードのオフセットからネイティブコードでのオフセットを取得
        size_t offset = m_codeGenerator->getOffsetForLabel(trace->sideExits[i].label);
        sideExitOffsets.push_back(offset);
    }
    
    // ネイティブコードにサイドエグジットパッチを適用
    for (size_t i = 0; i < trace->sideExits.size(); i++) {
        const auto& srcExit = trace->sideExits[i];
        auto& destExit = compiledTrace.sideExits[i];
        
        // サイドエグジット情報を設定
        destExit.location = srcExit.location;
        destExit.type = srcExit.type;
        destExit.nativeOffset = sideExitOffsets[i];
        
        // パッチのためのトランポリンコードを生成
        // これにより、JITコードからインタプリタに戻る際の状態遷移を処理
        uint8_t* patchLocation = nativeCode + destExit.nativeOffset;
        
        // ターゲットアドレスをサイドエグジットハンドラとして設定
        uintptr_t handlerAddr = reinterpret_cast<uintptr_t>(handleSideExit);
        
        // サイドエグジットIDをレジスタに設定するコードを生成
        // mov rax, exit_index
        *patchLocation++ = 0x48;  // REX.W
        *patchLocation++ = 0xB8;  // MOV RAX, imm64
        *reinterpret_cast<uint64_t*>(patchLocation) = i;
        patchLocation += 8;
        
        // トレースIDをセット
        // mov rdx, trace_id
        *patchLocation++ = 0x48;  // REX.W
        *patchLocation++ = 0xBA;  // MOV RDX, imm64
        *reinterpret_cast<uint64_t*>(patchLocation) = compiledTrace.traceId;
        patchLocation += 8;
        
        // ジャンプ先をサイドエグジットハンドラに設定
        // jmp handler_addr
        *patchLocation++ = 0xFF;  // JMP r/m64
        *patchLocation++ = 0x25;  // RIP-relative
        *reinterpret_cast<uint32_t*>(patchLocation) = 0;  // オフセット
        patchLocation += 4;
        *reinterpret_cast<uint64_t*>(patchLocation) = handlerAddr;
    }
    
    // エントリーポイントのホットパッチングを設定
    // これにより、バイトコードの特定位置でトレースを開始できるように
    // インタプリタコードにパッチを適用
    if (trace->entryPoint) {
        auto bytecodeAddr = *trace->entryPoint;
        auto& interpreter = m_context->getVM()->getInterpreter();
        
        // バイトコードオペレーションのディスパッチテーブルに
        // トレースエントリポイントとしてネイティブコードを登録
        interpreter.registerTraceEntryPoint(
            bytecodeAddr,
            reinterpret_cast<void*>(nativeCode)
        );
    }
    
    // トレースの内部最適化情報を格納
    compiledTrace.optimizationInfo.inlinedCalls = trace->inlinedCalls.size();
    compiledTrace.optimizationInfo.guardCount = trace->guardPoints.size();
    compiledTrace.optimizationInfo.eliminatedBoundsChecks = trace->eliminatedBoundsChecks;
    compiledTrace.optimizationInfo.eliminatedNullChecks = trace->eliminatedNullChecks;
    compiledTrace.optimizationInfo.hoistedInstructions = trace->hoistedInstructions;
    
    // プロファイル情報を設定
    m_profileInfo[compiledTrace.traceId] = TraceProfileInfo{
        .compilationTimeUs = trace->compilationTimeUs,
        .originalInstructions = trace->originalInstructionCount,
        .optimizedInstructions = trace->optimizedInstructionCount,
        .irNodes = allocatedIR->getNodeCount(),
        .machineCodeSize = codeSize,
        .guardCount = trace->guardPoints.size(),
        .sideExitCount = trace->sideExits.size()
    };
    
    // 生成されたコードを保存
    compiledTrace.nativeCode = nativeCode;
    compiledTrace.codeSize = codeSize;
    compiledTrace.executionTime = trace->executionTimeNs;
    
    // 使用メモリを追跡
    m_usedMemory += codeSize;
    
    // メモリ制限をチェック
    if (m_usedMemory > m_memoryLimit) {
        // 古いトレースを削除してメモリを確保
        evictOldTraces();
    }
    
    // コンパイル済みトレースをマップに追加
    m_compiledTraces[compiledTrace.traceId] = std::move(compiledTrace);
    
    // バイトコード位置とトレースの対応関係を更新
    m_locationToTraceMap[*trace->entryPoint] = compiledTrace.traceId;
    
    // 統計情報を更新
    m_statistics.successfulCompilations++;
    
    return true;
}

/**
 * @brief 古いトレースを削除してメモリを解放
 */
void TracingJIT::evictOldTraces() {
    // 使用頻度が低いトレースを特定
    struct TraceUsage {
        TraceId id;
        uint32_t executionCount;
    };
    
    std::vector<TraceUsage> usages;
    usages.reserve(m_compiledTraces.size());
    
    for (const auto& pair : m_compiledTraces) {
        usages.push_back({pair.first, pair.second.executionCount});
    }
    
    // 実行回数の少ない順にソート
    std::sort(usages.begin(), usages.end(), 
              [](const TraceUsage& a, const TraceUsage& b) {
                  return a.executionCount < b.executionCount;
              });
    
    // 削除する数を決定（全体の20%）
    size_t removeCount = usages.size() / 5;
    if (removeCount == 0 && !usages.empty()) {
        removeCount = 1; // 少なくとも1つは削除
    }
    
    // 低頻度使用トレースを削除
    for (size_t i = 0; i < removeCount && i < usages.size(); i++) {
        auto traceId = usages[i].id;
        auto it = m_compiledTraces.find(traceId);
        if (it != m_compiledTraces.end()) {
            // メモリ使用量を更新
            m_usedMemory -= it->second.codeSize;
            
            // トレースに関連するエントリポイントのマッピングを削除
            for (auto& pair : m_locationToTraceMap) {
                if (pair.second == traceId) {
                    m_locationToTraceMap.erase(pair.first);
                    break;
                }
            }
            
            // トレースを削除
            m_compiledTraces.erase(it);
        }
    }
    
    // 統計情報を更新
    m_statistics.evictedTraces += removeCount;
}

/**
 * @brief すべてのコンパイル済みトレースを削除
 */
void TracingJIT::clearAllTraces() {
    // すべてのトレースを解放
    releaseCompiledCode();
    
    // マップをクリア
    m_compiledTraces.clear();
    m_locationToTraceMap.clear();
    m_entryCountMap.clear();
    m_traceAttemptMap.clear();
    
    // メモリ使用量をリセット
    m_usedMemory = 0;
    
    // 統計情報をリセット
    initializeStatistics();
}

/**
 * @brief コンパイル済みコードを解放
 */
void TracingJIT::releaseCompiledCode() {
    for (auto& pair : m_compiledTraces) {
        auto& trace = pair.second;
        if (trace.nativeCode) {
            m_codeGenerator->releaseCode(trace.nativeCode);
            trace.nativeCode = nullptr;
            trace.codeSize = 0;
        }
    }
}

/**
 * @brief トレース開始イベントを記録
 * @param location バイトコード位置
 */
void TracingJIT::recordTraceStartEvent(const bytecode::BytecodeAddress& location) {
    m_statistics.startedTraces++;
}

/**
 * @brief トレース中止イベントを記録
 * @param location バイトコード位置
 * @param reason 中止理由
 */
void TracingJIT::recordTraceAbortEvent(
    const bytecode::BytecodeAddress& location,
    ExitReason reason) 
{
    m_statistics.abortedTraces++;
    
    // 中止理由に基づいて詳細な統計情報を更新
    switch (reason) {
        case ExitReason::TraceTooLong:
            m_statistics.tooLongTraces++;
            break;
        case ExitReason::TooManyGuardFailures:
            m_statistics.tooManyGuardsTraces++;
            break;
        case ExitReason::TooManySideExits:
            m_statistics.tooManySideExitsTraces++;
            break;
        case ExitReason::Timeout:
            m_statistics.timeoutTraces++;
            break;
        default:
            m_statistics.otherAbortedTraces++;
            break;
    }
}

/**
 * @brief トレース完了イベントを記録
 * @param location バイトコード位置
 * @param instructionCount 命令数
 * @param executionTime 実行時間（ナノ秒）
 */
void TracingJIT::recordTraceCompletionEvent(
    const bytecode::BytecodeAddress& location,
    size_t instructionCount,
    uint64_t executionTime) 
{
    m_statistics.completedTraces++;
    m_statistics.totalTracedInstructions += instructionCount;
    m_statistics.totalExecutionTimeNs += executionTime;
}

/**
 * @brief トレースヒットイベントを記録
 * @param location バイトコード位置
 */
void TracingJIT::recordTraceHit(const bytecode::BytecodeAddress& location) {
    m_statistics.traceHits++;
}

/**
 * @brief トレースサイドエグジットイベントを記録
 * @param location バイトコード位置
 * @param exitType サイドエグジットタイプ
 */
void TracingJIT::recordSideExitEvent(
    const bytecode::BytecodeAddress& location,
    SideExitType exitType) 
{
    m_statistics.sideExits++;
    
    // サイドエグジットタイプに基づいて詳細な統計情報を更新
    switch (exitType) {
        case SideExitType::GuardFailure:
            m_statistics.guardFailures++;
            break;
        case SideExitType::UnexpectedType:
            m_statistics.unexpectedTypeExits++;
            break;
        case SideExitType::ExceptionThrown:
            m_statistics.exceptionExits++;
            break;
        default:
            m_statistics.otherSideExits++;
            break;
    }
}

/**
 * @brief 現在の統計情報を取得
 * @return JIT統計情報
 */
const TracingJIT::Statistics& TracingJIT::getStatistics() const {
    return m_statistics;
}

/**
 * @brief 統計情報を初期化
 */
void TracingJIT::initializeStatistics() {
    m_statistics = Statistics{};
    m_statistics.creationTime = std::chrono::system_clock::now();
}

/**
 * @brief 統計情報の概要を文字列で取得
 * @return 統計情報の概要
 */
std::string TracingJIT::getStatisticsSummary() const {
    std::string summary;
    
    // 現在時刻を取得
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        now - m_statistics.creationTime).count();
    
    summary += "AeroJS メタトレーシングJIT 統計情報\n";
    summary += "-----------------------------------\n";
    summary += "稼働時間: " + std::to_string(uptime) + " 秒\n";
    summary += "コンパイル済みトレース: " + std::to_string(m_compiledTraces.size()) + "\n";
    summary += "使用メモリ: " + std::to_string(m_usedMemory / 1024) + " KB\n\n";
    
    summary += "トレース統計:\n";
    summary += "  ホットスポット検出: " + std::to_string(m_statistics.totalTraceAttempts) + "\n";
    summary += "  トレース記録開始: " + std::to_string(m_statistics.startedTraces) + "\n";
    summary += "  トレース記録完了: " + std::to_string(m_statistics.completedTraces) + "\n";
    summary += "  トレース記録中止: " + std::to_string(m_statistics.abortedTraces) + "\n";
    summary += "  コンパイル成功: " + std::to_string(m_statistics.successfulCompilations) + "\n";
    summary += "  トレースヒット数: " + std::to_string(m_statistics.traceHits) + "\n";
    summary += "  サイドエグジット数: " + std::to_string(m_statistics.sideExits) + "\n\n";
    
    summary += "中止理由の内訳:\n";
    summary += "  トレース長過多: " + std::to_string(m_statistics.tooLongTraces) + "\n";
    summary += "  ガード多過ぎ: " + std::to_string(m_statistics.tooManyGuardsTraces) + "\n";
    summary += "  サイドエグジット多過ぎ: " + std::to_string(m_statistics.tooManySideExitsTraces) + "\n";
    summary += "  タイムアウト: " + std::to_string(m_statistics.timeoutTraces) + "\n";
    summary += "  その他: " + std::to_string(m_statistics.otherAbortedTraces) + "\n\n";
    
    summary += "サイドエグジットの内訳:\n";
    summary += "  ガード失敗: " + std::to_string(m_statistics.guardFailures) + "\n";
    summary += "  予期しない型: " + std::to_string(m_statistics.unexpectedTypeExits) + "\n";
    summary += "  例外発生: " + std::to_string(m_statistics.exceptionExits) + "\n";
    summary += "  その他: " + std::to_string(m_statistics.otherSideExits) + "\n\n";
    
    if (m_statistics.traceHits > 0) {
        float hitRatio = static_cast<float>(m_statistics.traceHits) / 
                         (m_statistics.traceHits + m_statistics.sideExits);
        summary += "ヒット率: " + std::to_string(hitRatio * 100.0f) + "%\n";
    }
    
    return summary;
}

// 最適化キューからのトレース最適化処理
bool TracingJIT::processOptimizationTask(const OptimizationTask& task) {
    // 対象トレースを取得
    auto it = m_traces.find(task.traceId);
    if (it == m_traces.end()) {
        return false; // トレースが見つからない
    }
    
    Trace* trace = it->second.get();
    if (!trace || trace->isAborted()) {
        return false; // 無効なトレース
    }
    
    // 既にコンパイル済みかチェック
    if (m_compiledTraces.find(task.traceId) != m_compiledTraces.end()) {
        return true; // 既にコンパイル済み
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        // トレースの検証
        if (!m_optimizer->validateTrace(*trace)) {
            m_stats.abortedTraces++;
            trace->setAbortReason(AbortReason::ValidationFailed);
            return false;
        }
        
        // トレース最適化の実行
        auto optimizedIR = m_optimizer->optimizeTrace(*trace);
        if (!optimizedIR) {
            m_stats.abortedTraces++;
            trace->setAbortReason(AbortReason::OptimizationFailed);
            return false;
        }
        
        // レジスタ割り当て
        auto registerAllocator = m_optimizingJIT->getRegisterAllocator();
        auto allocatedIR = registerAllocator->allocateRegisters(optimizedIR.get());
        if (!allocatedIR) {
            m_stats.abortedTraces++;
            trace->setAbortReason(AbortReason::RegisterAllocationFailed);
            return false;
        }
        
        // コード生成
        auto codeGenerator = m_optimizingJIT->getCodeGenerator();
        auto compiledTrace = std::make_unique<CompiledTrace>();
        
        compiledTrace->traceId = task.traceId;
        compiledTrace->nativeCode = codeGenerator->generateCode(allocatedIR.get(), compiledTrace->codeSize);
        
        if (!compiledTrace->nativeCode || compiledTrace->codeSize == 0) {
            m_stats.abortedTraces++;
            trace->setAbortReason(AbortReason::CodeGenerationFailed);
            return false;
        }
        
        // サイド出口の設定
        for (size_t i = 0; i < trace->getSideExits().size(); i++) {
            const auto& sideExit = trace->getSideExits()[i];
            size_t offset = codeGenerator->getOffsetForLabel(sideExit.label);
            compiledTrace->sideExitOffsets.push_back(offset);
            compiledTrace->guardToSideExitMap[sideExit.guardId] = offset;
        }
        
        // 最適化解除ポイントの設定
        for (const auto& deoptPoint : trace->getDeoptimizationPoints()) {
            size_t offset = codeGenerator->getOffsetForLabel(deoptPoint.label);
            compiledTrace->deoptPoints.push_back(offset);
        }
        
        // 実行可能メモリに配置
        compiledTrace->nativeCode = makeExecutable(compiledTrace->nativeCode, compiledTrace->codeSize);
        if (!compiledTrace->nativeCode) {
            m_stats.abortedTraces++;
            trace->setAbortReason(AbortReason::MemoryAllocationFailed);
            return false;
        }
        
        // トレースを実行可能として登録
        uint32_t traceId = compiledTrace->traceId;
        m_compiledTraces[traceId] = std::move(compiledTrace);
        trace->setCompiled(true);
        
        // 関数のエントリポイントマップに追加
        auto functionId = trace->getFunction()->getId();
        auto bytecodeOffset = trace->getStartOffset();
        m_entryMap[functionId][bytecodeOffset] = traceId;
        
        auto endTime = std::chrono::steady_clock::now();
        auto compilationTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // 統計情報を更新
        m_stats.compiledTraces++;
        m_stats.totalCompilationTimeMs += compilationTime.count();
        
        return true;
        
    } catch (const std::exception& e) {
        // コンパイルエラー
        m_stats.abortedTraces++;
        trace->setAbortReason(AbortReason::CompilerException);
        return false;
    }
}

// コンパイルスレッドメイン関数
void TracingJIT::compileThreadMain() {
    while (m_compileThreadRunning.load()) {
        std::unique_lock<std::mutex> lock(m_compileMutex);
        
        // 最適化タスクを待機
        m_compileCondition.wait(lock, [this] {
            return !m_optimizationQueue.empty() || !m_compileThreadRunning.load();
        });
        
        if (!m_compileThreadRunning.load()) {
            break;
        }
        
        // 最高優先度のタスクを取得
        if (!m_optimizationQueue.empty()) {
            OptimizationTask task = m_optimizationQueue.top();
            m_optimizationQueue.pop();
            lock.unlock();
            
            // タスクを処理
            processOptimizationTask(task);
        }
    }
}

// 実行可能メモリの確保
uint8_t* TracingJIT::makeExecutable(uint8_t* code, size_t size) {
    if (!code || size == 0) {
        return nullptr;
    }
    
#ifdef _WIN32
    // Windows: VirtualAlloc
    void* execMemory = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!execMemory) {
        return nullptr;
    }
    
    // コードをコピー
    memcpy(execMemory, code, size);
    
    // 実行可能にする
    DWORD oldProtect;
    if (!VirtualProtect(execMemory, size, PAGE_EXECUTE_READ, &oldProtect)) {
        VirtualFree(execMemory, 0, MEM_RELEASE);
        return nullptr;
    }
    
    return static_cast<uint8_t*>(execMemory);
    
#else
    // Unix系: mmap
    void* execMemory = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, 
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (execMemory == MAP_FAILED) {
        return nullptr;
    }
    
    // コードをコピー
    memcpy(execMemory, code, size);
    
    return static_cast<uint8_t*>(execMemory);
#endif
}

// サイド出口からのトレース継続処理
bool TracingJIT::handleSideExit(uint32_t traceId, uint32_t sideExitId, 
                                runtime::execution::ExecutionContext* context) {
    auto compiledTrace = findCompiledTrace(traceId);
    if (!compiledTrace) {
        return false;
    }
    
    // サイド出口のヒット数を増加
    auto it = compiledTrace->guardToSideExitMap.find(sideExitId);
    if (it == compiledTrace->guardToSideExitMap.end()) {
        return false;
    }
    
    // サイド出口が頻繁に実行される場合、サイドトレースを作成
    auto sideExitOffset = it->second;
    auto sideExitCount = ++m_sideExitCounts[{traceId, sideExitId}];
    
    if (sideExitCount >= m_config.sideExitThreshold) {
        // サイドトレースの記録を開始
        auto currentFunction = context->getCurrentFunction();
        auto currentOffset = context->getCurrentBytecodeOffset();
        
        return startTracing(currentFunction, currentOffset, TraceReason::SideExit);
    }
    
    // 統計情報を更新
    m_stats.sideExitCount++;
    
    return false;
}

// 最適化解除処理
void TracingJIT::handleDeoptimization(uint32_t traceId, uint32_t deoptId,
                                     runtime::execution::ExecutionContext* context) {
    auto compiledTrace = findCompiledTrace(traceId);
    if (!compiledTrace) {
        return;
    }
    
    // 最適化解除の理由を分析
    DeoptimizationReason reason = analyzeDeoptimizationReason(traceId, deoptId, context);
    
    // 統計情報を更新
    m_stats.deoptimizationCount++;
    compiledTrace->failCount++;
    
    // トレースの実行に失敗した回数が多い場合は無効化
    float failureRate = static_cast<float>(compiledTrace->failCount) / 
                       (compiledTrace->successCount + compiledTrace->failCount);
    
    if (failureRate > 0.5f && compiledTrace->executeCount > 10) {
        invalidateTrace(traceId);
    }
    
    // ベイルアウトハンドラを呼び出し
    if (m_bailoutHandler) {
        m_bailoutHandler(traceId, deoptId, context);
    }
}

// 最適化解除理由の分析
DeoptimizationReason TracingJIT::analyzeDeoptimizationReason(uint32_t traceId, uint32_t deoptId,
                                                            runtime::execution::ExecutionContext* context) {
    // 実際の実装では、デoptId、現在の実行状態、型情報などを分析して
    // 最適化解除の理由を特定する
    
    // 簡易実装として一般的な理由を返す
    return DeoptimizationReason::TypeMismatch;
}

// トレース無効化のパフォーマンス統計更新
void TracingJIT::updateInvalidationStatistics(uint32_t traceId, InvalidationReason reason) {
    m_stats.invalidatedTraces++;
    
    // 無効化理由に応じた詳細統計
    switch (reason) {
        case InvalidationReason::TooManyFailures:
            m_invalidationStats.tooManyFailures++;
            break;
        case InvalidationReason::FunctionChanged:
            m_invalidationStats.functionChanged++;
            break;
        case InvalidationReason::ShapeChanged:
            m_invalidationStats.shapeChanged++;
            break;
        case InvalidationReason::MemoryPressure:
            m_invalidationStats.memoryPressure++;
            break;
        default:
            m_invalidationStats.other++;
            break;
    }
}

} // namespace metatracing
} // namespace jit
} // namespace core
} // namespace aerojs 