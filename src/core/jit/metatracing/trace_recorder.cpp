#include "trace_recorder.h"
#include "tracing_jit.h"
#include "../../runtime/context/execution_context.h"
#include "../../runtime/values/value.h"
#include "../../vm/bytecode/opcode.h"
#include "../../vm/interpreter/interpreter.h"
#include "../ir/ir_builder.h"
#include "../profiler/execution_counter.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <unordered_set>
#include <numeric>

namespace aerojs {
namespace core {
namespace jit {
namespace metatracing {

using namespace aerojs::core::runtime;
using namespace aerojs::core::vm;

// SIMD操作をサポートする最適化されたメモリコピーユーティリティ
namespace {
    // メモリ操作のアラインメントサイズ
    constexpr size_t ALIGNMENT_SIZE = 16;
    
    template<typename T>
    inline void optimizedCopy(const T* src, T* dest, size_t count) {
        #if defined(__AVX2__)
        // AVX2が利用可能な場合、SIMD命令を使用
        if (count >= 8 && reinterpret_cast<uintptr_t>(src) % ALIGNMENT_SIZE == 0 && 
            reinterpret_cast<uintptr_t>(dest) % ALIGNMENT_SIZE == 0) {
            // アラインメントが合っている場合はSIMD最適化
            size_t simdCount = count / 8;
            for (size_t i = 0; i < simdCount; i++) {
                __m256i value = _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i * 8));
                _mm256_store_si256(reinterpret_cast<__m256i*>(dest + i * 8), value);
            }
            
            // 残りの要素をコピー
            size_t remaining = count % 8;
            size_t offset = count - remaining;
            for (size_t i = 0; i < remaining; i++) {
                dest[offset + i] = src[offset + i];
            }
        } else
        #endif
        {
            // SIMDが使えない場合は標準コピー
            for (size_t i = 0; i < count; i++) {
                dest[i] = src[i];
            }
        }
    }
}

// グローバル最適化レベル設定
enum class OptimizationLevel {
    None,
    Basic,
    Aggressive,
    Ultra // 最高レベルの最適化
};

/**
 * @brief コンストラクタ
 */
TraceRecorder::TraceRecorder(TracingJIT& jit)
    : m_jit(jit),
      m_recording(false),
      m_aborted(false),
      m_guardCount(0),
      m_loopLevel(0),
      m_instructionCount(0),
      m_nextValueId(1)
{
    // 初期ストレージの確保
    m_trace.instructions.reserve(1024);
    m_valueMap.reserve(256);
}

/**
 * @brief デストラクタ
 */
TraceRecorder::~TraceRecorder() {
    // トレースが完了していない場合はクリーンアップ
    if (m_recording) {
        abortTrace("Recorder destroyed while recording");
    }
}

/**
 * @brief トレース記録を開始する
 * @param context 実行コンテキスト
 * @param entryPoint トレース開始位置
 * @return 記録開始に成功したらtrue
 */
bool TraceRecorder::startRecording(execution::ExecutionContext* context, const bytecode::BytecodeAddress& entryPoint) {
    if (m_recording) {
        // 既に記録中の場合、ネストしたトレースとして処理
        if (m_loopLevel >= MAX_NESTED_TRACE_DEPTH) {
            // ネストの深さが限界を超えた場合は記録しない
            return false;
        }
        m_loopLevel++;
        return true;
    }
    
    m_recording = true;
    m_aborted = false;
    m_guardCount = 0;
    m_instructionCount = 0;
    m_trace.clear();
    m_valueMap.clear();
    m_nextValueId = 1;
    m_loopLevel = 1;
    
    // トレース開始情報を記録
    m_trace.startAddress = entryPoint.offset;
    m_trace.startContext = context;
    m_trace.startTimestamp = getCurrentTimestamp();
    
    // トレースヘッダを記録
    recordTraceHeader(context);
    
    // トレース開始命令を記録
    TraceInstruction startInstr;
    startInstr.opcode = TraceOpcode::TraceStart;
    startInstr.location = entryPoint;
    startInstr.timestamp = getCurrentTimestamp();
    m_trace.instructions.push_back(startInstr);
    
    // 統計情報を記録
    m_jit.recordTraceStartEvent(entryPoint);
    
    return true;
}

/**
 * @brief トレース記録を強制終了する
 * @param reason 終了理由
 */
void TraceRecorder::abortRecording(ExitReason reason) {
    if (!m_recording) {
        return;
    }
    
    m_aborted = true;
    m_trace.abortReason = reason;
    m_trace.endTimestamp = getCurrentTimestamp();
    
    // JITコンパイラにトレース中止を通知
    m_jit.onTraceAborted(m_trace);
    
    // トレース状態をリセット
    m_recording = false;
    m_loopLevel = 0;
    
    // 統計情報を記録
    if (m_statisticsEnabled) {
        m_jit.recordTraceAbortEvent(m_trace.startAddress, reason);
    }
    
    // トレースをクリア
    m_trace.clear();
}

/**
 * @brief トレース記録を完了する
 * @return 記録されたトレース
 */
std::unique_ptr<TraceRecorder::Trace> TraceRecorder::finishRecording() {
    if (!m_recording) {
        return nullptr;
    }
    
    // トレース終了情報を記録
    m_trace.endAddress = m_trace.instructions.back().location.offset;
    m_trace.endTimestamp = getCurrentTimestamp();
    
    // トレースフッタを記録
    recordTraceFooter();
    
    // 結果を返すためにトレースのコピーを作成
    std::unique_ptr<Trace> result = std::make_unique<Trace>();
    result->trace = m_trace;
    
    // 統計情報を記録
    if (m_statisticsEnabled) {
        m_jit.recordTraceCompletionEvent(m_trace.startAddress, 
                                         m_trace.instructions.size(),
                                         m_trace.endTimestamp - m_trace.startTimestamp);
    }
    
    // トレース状態をリセット
    m_recording = false;
    m_loopLevel = 0;
    
    return result;
}

/**
 * @brief バイトコード命令の実行を記録する
 * @param context 実行コンテキスト
 * @param location バイトコードの位置
 * @param opcode バイトコードのオペコード
 * @param operands オペランドリスト
 */
void TraceRecorder::recordBytecodeExecution(
    execution::ExecutionContext* context,
    const bytecode::BytecodeAddress& location,
    bytecode::Opcode opcode,
    const std::vector<Value>& operands) 
{
    if (!m_recording || m_aborted) {
        return;
    }
    
    // トレースサイズ制限をチェック
    if (++m_instructionCount > MAX_TRACE_INSTRUCTIONS) {
        abortTrace("Trace exceeded maximum instruction count");
        return;
    }
    
    // バイトコード実行命令を記録
    TraceInstruction instr;
    instr.opcode = TraceOpcode::ExecuteBytecode;
    instr.location = location;
    instr.bytecodeOp = opcode;
    instr.timestamp = getCurrentTimestamp();
    
    // オペランドのコピー
    instr.operands.resize(operands.size());
    for (size_t i = 0; i < operands.size(); i++) {
        instr.operands[i] = operands[i];
    }
    
    // スタックスナップショットの記録（定期的に）
    const size_t STACK_SNAPSHOT_INTERVAL = 10; // 10命令ごとにスタックをスナップショット
    if (m_trace.instructions.size() % STACK_SNAPSHOT_INTERVAL == 0) {
        instr.hasStackSnapshot = true;
        instr.stackSnapshot = captureStackSnapshot(context);
    }
    
    m_trace.instructions.push_back(instr);
    
    // バイトコードの種類に応じた特殊処理
    switch (opcode) {
        case bytecode::Opcode::CALL:
        case bytecode::Opcode::CALL_METHOD:
            // 関数呼び出しの場合、呼び出し深さをチェック
            if (context->getCallStackDepth() > MAX_INLINE_CALL_DEPTH) {
                // インライン展開の深さが制限を超えた場合
                recordSideExit(context, location, SideExitType::CallStackLimitReached);
            }
            break;
            
        case bytecode::Opcode::JMP_IF_FALSE:
        case bytecode::Opcode::JMP_IF_TRUE:
        case bytecode::Opcode::JMP:
            // ジャンプ命令の場合、トレースが大きくなりすぎないようにループ検出
            if (isBackwardJump(location, getBytecodeJumpTarget(location, opcode, operands))) {
                m_loopIterationCount++;
                
                if (m_loopIterationCount > MAX_LOOP_ITERATIONS) {
                    // ループ反復回数が多すぎる場合、サイドエグジット
                    recordSideExit(context, location, SideExitType::LoopIterationLimit);
                }
            }
            break;
            
        default:
            // その他のバイトコードは通常処理
            break;
    }
}

/**
 * @brief ガード条件を記録する
 * @param context 実行コンテキスト
 * @param location バイトコードの位置
 * @param condition ガード条件
 * @param valueType 期待される値の型
 * @param actualValue 実際の値
 * @return ガードが成功した場合はtrue
 */
bool TraceRecorder::recordGuardCondition(
    execution::ExecutionContext* context,
    const bytecode::BytecodeAddress& location,
    GuardCondition condition,
    ValueType expectedType,
    const Value& actualValue) 
{
    if (!m_recording || m_aborted) {
        return true; // 記録中でなければガードは常に成功したと見なす
    }
    
    // ガード数制限をチェック
    if (++m_guardCount > MAX_GUARDS_PER_TRACE) {
        abortTrace("Trace exceeded maximum guard count");
        return true;
    }
    
    // ガード命令を記録
    TraceInstruction instr;
    instr.opcode = TraceOpcode::Guard;
    instr.location = location;
    instr.guardCondition = condition;
    instr.expectedType = expectedType;
    instr.operands.push_back(actualValue);
    instr.timestamp = getCurrentTimestamp();
    
    m_trace.instructions.push_back(instr);
    
    // ガードの実際の評価
    bool guardSuccess = evaluateGuard(condition, expectedType, actualValue);
    
    if (!guardSuccess) {
        // ガード失敗を記録
        recordGuardFailure(context, location, condition, expectedType, actualValue);
        
        // ガード失敗が多すぎる場合はトレースを中止
        if (m_guardCount > MAX_GUARDS_PER_TRACE) {
            abortTrace("Too many guard failures");
        }
    }
    
    return guardSuccess;
}

/**
 * @brief サイドエグジットを記録する
 * @param context 実行コンテキスト
 * @param location バイトコードの位置
 * @param exitType サイドエグジットのタイプ
 */
void TraceRecorder::recordSideExit(
    execution::ExecutionContext* context,
    const bytecode::BytecodeAddress& location,
    SideExitType exitType) 
{
    if (!m_recording || m_aborted) {
        return;
    }
    
    // サイドエグジット命令を記録
    TraceInstruction instr;
    instr.opcode = TraceOpcode::SideExit;
    instr.location = location;
    instr.sideExitType = exitType;
    instr.timestamp = getCurrentTimestamp();
    
    // スタックスナップショットを記録
    instr.hasStackSnapshot = true;
    instr.stackSnapshot = captureStackSnapshot(context);
    
    m_trace.instructions.push_back(instr);
    
    // サイドエグジット情報を保存
    SideExit exit;
    exit.location = location;
    exit.type = exitType;
    exit.instructionIndex = m_trace.instructions.size() - 1;
    exit.contextSnapshot = std::make_unique<ContextSnapshot>(context);
    
    m_trace.sideExits.push_back(std::move(exit));
    
    // サイドエグジットが多すぎる場合はトレースを中止
    if (m_trace.sideExits.size() > MAX_SIDE_EXITS) {
        abortTrace("Too many side exits");
    }
}

/**
 * @brief 最適化条件のヒントを記録する
 * @param location バイトコードの位置
 * @param hint 最適化ヒント
 * @param data 追加データ
 */
void TraceRecorder::recordOptimizationHint(
    const bytecode::BytecodeAddress& location,
    OptimizationHint hint,
    const Value& data) 
{
    if (!m_recording || m_aborted) {
        return;
    }
    
    // 最適化ヒント命令を記録
    TraceInstruction instr;
    instr.opcode = TraceOpcode::OptimizationHint;
    instr.location = location;
    instr.optimizationHint = hint;
    instr.operands.push_back(data);
    instr.timestamp = getCurrentTimestamp();
    
    m_trace.instructions.push_back(instr);
}

/**
 * @brief ガード失敗を記録する
 * @param context 実行コンテキスト
 * @param location バイトコードの位置
 * @param condition ガード条件
 * @param expectedType 期待される値の型
 * @param actualValue 実際の値
 */
void TraceRecorder::recordGuardFailure(
    execution::ExecutionContext* context,
    const bytecode::BytecodeAddress& location,
    GuardCondition condition,
    ValueType expectedType,
    const Value& actualValue) 
{
    // ガード失敗命令を記録
    TraceInstruction instr;
    instr.opcode = TraceOpcode::GuardFailure;
    instr.location = location;
    instr.guardCondition = condition;
    instr.expectedType = expectedType;
    instr.operands.push_back(actualValue);
    instr.timestamp = getCurrentTimestamp();
    
    // スタックスナップショットを記録
    instr.hasStackSnapshot = true;
    instr.stackSnapshot = captureStackSnapshot(context);
    
    m_trace.instructions.push_back(instr);
    
    // ガード失敗に対応するサイドエグジットを記録
    SideExit exit;
    exit.location = location;
    exit.type = SideExitType::GuardFailure;
    exit.instructionIndex = m_trace.instructions.size() - 1;
    exit.contextSnapshot = std::make_unique<ContextSnapshot>(context);
    exit.guardCondition = condition;
    exit.expectedType = expectedType;
    exit.actualValue = actualValue;
    
    m_trace.sideExits.push_back(std::move(exit));
}

/**
 * @brief トレースレコーダーをリセットする
 */
void TraceRecorder::reset() {
    m_recording = false;
    m_aborted = false;
    m_guardCount = 0;
    m_instructionCount = 0;
    m_loopLevel = 0;
    m_loopIterationCount = 0;
    m_trace.clear();
    m_valueMap.clear();
    m_nextValueId = 1;
    
    initializeEmptyTrace();
}

/**
 * @brief トレース記録中かどうかを返す
 */
bool TraceRecorder::isRecording() const {
    return m_recording;
}

/**
 * @brief 現在の記録深度を返す
 */
int TraceRecorder::getRecordingDepth() const {
    return m_loopLevel;
}

/**
 * @brief 最後のエントリーポイントを返す
 */
const bytecode::BytecodeAddress* TraceRecorder::getLastEntryPoint() const {
    return &m_trace.instructions.back().location;
}

/**
 * @brief 統計情報の収集を有効化/無効化する
 */
void TraceRecorder::enableStatistics(bool enable) {
    m_statisticsEnabled = enable;
}

/**
 * @brief ガード条件を評価する
 * @param condition ガード条件
 * @param expectedType 期待される値の型
 * @param actualValue 実際の値
 * @return ガードが成功した場合はtrue
 */
bool TraceRecorder::evaluateGuard(
    GuardCondition condition,
    ValueType expectedType,
    const Value& actualValue) 
{
    switch (condition) {
        case GuardCondition::TypeCheck:
            return actualValue.getType() == expectedType;
            
        case GuardCondition::NonNull:
            return !actualValue.isNull() && !actualValue.isUndefined();
            
        case GuardCondition::IntegerInRange:
            if (actualValue.isInt32()) {
                int32_t value = actualValue.asInt32();
                int32_t min = static_cast<int32_t>(expectedType & 0xFFFFFFFF);
                int32_t max = static_cast<int32_t>(expectedType >> 32);
                return value >= min && value <= max;
            }
            return false;
            
        case GuardCondition::StringLength:
            if (actualValue.isString()) {
                size_t length = actualValue.asString()->length();
                return length == static_cast<size_t>(expectedType);
            }
            return false;
            
        case GuardCondition::ObjectShape:
            // オブジェクトシェイプの比較は複雑なため、実際の実装ではさらに詳細な処理が必要
            if (actualValue.isObject()) {
                return actualValue.asObject()->getShapeId() == static_cast<uint32_t>(expectedType);
            }
            return false;
            
        case GuardCondition::ArrayLength:
            if (actualValue.isArray()) {
                size_t length = actualValue.asArray()->length();
                return length == static_cast<size_t>(expectedType);
            }
            return false;
            
        default:
            // 未知のガード条件は安全のため失敗とする
            return false;
    }
}

/**
 * @brief 現在のタイムスタンプを取得する
 */
uint64_t TraceRecorder::getCurrentTimestamp() {
    // 現在のタイムスタンプを取得（高精度）
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count());
}

/**
 * @brief スタックスナップショットを取得する
 * @param context 実行コンテキスト
 * @return スタックスナップショット
 */
TraceRecorder::StackSnapshot TraceRecorder::captureStackSnapshot(execution::ExecutionContext* context) {
    StackSnapshot snapshot;
    
    // 現在のスタックをコピー（実際の実装ではエンジンの内部構造に依存）
    const auto& stack = context->getStack();
    snapshot.values.resize(stack.size());
    
    for (size_t i = 0; i < stack.size(); i++) {
        snapshot.values[i] = stack[i];
    }
    
    snapshot.stackPointer = context->getStackPointer();
    snapshot.framePointer = context->getFramePointer();
    
    return snapshot;
}

/**
 * @brief 空のトレースを初期化する
 */
void TraceRecorder::initializeEmptyTrace() {
    m_trace.instructions.clear();
    m_trace.sideExits.clear();
    m_trace.startAddress = 0;
    m_trace.endAddress = 0;
    m_trace.startTimestamp = 0;
    m_trace.endTimestamp = 0;
    m_trace.executedBytecodes = 0;
    m_trace.contextSnapshot.reset();
    m_loopIterationCount = 0;
}

/**
 * @brief 後方ジャンプかどうかをチェックする
 * @param current 現在の位置
 * @param target ジャンプ先
 * @return 後方ジャンプならtrue
 */
bool TraceRecorder::isBackwardJump(
    const bytecode::BytecodeAddress& current,
    const bytecode::BytecodeAddress& target) 
{
    // 通常、バイトコードアドレスは関数とオフセットで構成される
    // 同じ関数内で、オフセットが現在地より小さければ後方ジャンプ
    if (current.function == target.function) {
        return target.offset < current.offset;
    }
    
    // 異なる関数の場合は後方ジャンプとは見なさない
    return false;
}

/**
 * @brief バイトコードのジャンプターゲットを取得する
 * @param location 現在の位置
 * @param opcode バイトコードのオペコード
 * @param operands オペランドリスト
 * @return ジャンプ先アドレス
 */
bytecode::BytecodeAddress TraceRecorder::getBytecodeJumpTarget(
    const bytecode::BytecodeAddress& location,
    bytecode::Opcode opcode,
    const std::vector<Value>& operands) 
{
    // ジャンプ命令のターゲットは通常、相対オフセットとして指定される
    // 実際の実装ではバイトコードフォーマットに依存する
    
    // 簡略化のため、直接ジャンプオフセットが指定されていると仮定
    if (operands.size() > 0 && operands[0].isInt32()) {
        int32_t offset = operands[0].asInt32();
        
        bytecode::BytecodeAddress target;
        target.function = location.function;
        target.offset = location.offset + offset;
        
        return target;
    }
    
    // デフォルトでは現在位置を返す
    return location;
}

/**
 * @brief コンテキストスナップショットのコンストラクタ
 * @param context 実行コンテキスト
 */
TraceRecorder::ContextSnapshot::ContextSnapshot(execution::ExecutionContext* context) {
    if (context) {
        // 実行コンテキストの状態をキャプチャ
        instruction = context->getCurrentInstruction();
        stackPointer = context->getStackPointer();
        framePointer = context->getFramePointer();
        
        // 現在のスタックフレームをコピー
        const auto& stack = context->getStack();
        stackValues.resize(stack.size());
        
        for (size_t i = 0; i < stack.size(); i++) {
            stackValues[i] = stack[i];
        }
        
        // その他の必要な状態をキャプチャ
        // コンテキストの実装に応じて追加
    }
}

/**
 * @brief コンテキストスナップショットのコピーコンストラクタ
 * @param other コピー元のスナップショット
 */
TraceRecorder::ContextSnapshot::ContextSnapshot(const ContextSnapshot& other) {
    instruction = other.instruction;
    stackPointer = other.stackPointer;
    framePointer = other.framePointer;
    
    stackValues.resize(other.stackValues.size());
    optimizedCopy(other.stackValues.data(), stackValues.data(), other.stackValues.size());
}

/**
 * @brief コンテキストスナップショットのデストラクタ
 */
TraceRecorder::ContextSnapshot::~ContextSnapshot() {
    // 特に必要なリソース解放はない
}

void TraceRecorder::recordTraceHeader(ExecutionContext* context) {
    // 環境情報の記録
    m_trace.header.contextId = reinterpret_cast<uintptr_t>(context);
    m_trace.header.globalObjectId = reinterpret_cast<uintptr_t>(context->globalObject());
    m_trace.header.traceId = generateTraceId();
    m_trace.header.timestamp = getCurrentTimestamp();
    m_trace.header.threadId = getCurrentThreadId();
    
    // トレースの種類を設定
    if (m_jit.getExecutionCounter().isLoopHeader(m_trace.startAddress)) {
        m_trace.header.traceType = TraceType::LOOP_TRACE;
    } else if (m_jit.getExecutionCounter().isFunctionEntry(m_trace.startAddress)) {
        m_trace.header.traceType = TraceType::FUNCTION_TRACE;
    } else {
        m_trace.header.traceType = TraceType::GENERIC_TRACE;
    }
}

void TraceRecorder::recordTraceFooter() {
    // トレース統計情報を計算
    m_trace.footer.instructionCount = m_trace.instructions.size();
    m_trace.footer.guardCount = m_trace.guards.size();
    m_trace.footer.valueCount = m_trace.values.size();
    m_trace.footer.sideExitCount = m_trace.sideExits.size();
    m_trace.footer.functionEntryCount = m_trace.functionEntries.size();
    m_trace.footer.duration = m_trace.endTimestamp - m_trace.startTimestamp;
    
    // ループトレースの場合、ループの反復回数を記録
    if (m_trace.header.traceType == TraceType::LOOP_TRACE) {
        int loopIterations = 0;
        for (const auto& event : m_trace.events) {
            if (event.type == TraceEventType::LOOP_ENTRY) {
                loopIterations++;
            }
        }
        m_trace.footer.loopIterations = loopIterations;
    }
}

std::unique_ptr<IRFunction> TraceRecorder::convertTraceToIR() const {
    // IRビルダーを作成
    IRBuilder builder;
    
    // トレース命令を順番にIR命令に変換
    for (const auto& instr : m_trace.instructions) {
        // 入力値のベクトルを構築
        std::vector<int64_t> args;
        for (ValueId input : instr.inputs) {
            args.push_back(static_cast<int64_t>(input));
        }
        
        // 対応するIR命令を追加
        builder.addInstruction(instr.opcode, args, static_cast<int64_t>(instr.output));
    }
    
    // ガード命令を追加
    for (const auto& guard : m_trace.guards) {
        // ガードタイプに応じた命令を生成
        IROpcode guardOpcode;
        switch (guard.type) {
            case GuardType::TYPE_GUARD:
                guardOpcode = IROpcode::GUARD_TYPE;
                break;
            case GuardType::VALUE_GUARD:
                guardOpcode = IROpcode::GUARD_VALUE;
                break;
            case GuardType::SHAPE_GUARD:
                guardOpcode = IROpcode::GUARD_SHAPE;
                break;
            case GuardType::NULL_GUARD:
                guardOpcode = IROpcode::GUARD_NULL;
                break;
            case GuardType::UNDEFINED_GUARD:
                guardOpcode = IROpcode::GUARD_UNDEFINED;
                break;
            case GuardType::INTEGER_GUARD:
                guardOpcode = IROpcode::GUARD_INTEGER;
                break;
            default:
                guardOpcode = IROpcode::GUARD_GENERIC;
        }
        
        // ガード命令を追加
        builder.addInstruction(guardOpcode, { static_cast<int64_t>(guard.valueId) });
    }
    
    // IR関数を生成して返す
    std::string funcName = "trace_" + std::to_string(m_trace.header.traceId);
    IRFunction* func = builder.buildFunction(funcName);
    
    return std::unique_ptr<IRFunction>(func);
}

uint32_t TraceRecorder::getCurrentThreadId() {
    // 現在のスレッドIDを取得
    return static_cast<uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

uint64_t TraceRecorder::generateTraceId() {
    // ユニークなトレースIDを生成
    static std::atomic<uint64_t> nextTraceId(1);
    return nextTraceId++;
}

// 新しい並列処理機能実装
void TraceRecorder::optimizeTraceParallel(Trace* trace) {
    if (!trace) return;
    
    // 各最適化フェーズを並列実行
    std::vector<std::unique_ptr<OptimizationPhase>> phases;
    phases.push_back(std::make_unique<RedundantGuardElimination>());
    phases.push_back(std::make_unique<CommonSubexpressionElimination>());
    phases.push_back(std::make_unique<DeadCodeElimination>());
    phases.push_back(std::make_unique<LoopInvariantCodeMotion>());
    phases.push_back(std::make_unique<InstructionCombiner>());
    phases.push_back(std::make_unique<LoadStoreOptimizer>());
    phases.push_back(std::make_unique<VectorizationOptimizer>());
    phases.push_back(std::make_unique<TypeSpecializer>());
    
    // 並列実行用データ構造
    struct OptimizationResult {
        std::vector<std::unique_ptr<TraceNode>> newNodes;
        bool changed = false;
    };
    
    std::vector<OptimizationResult> results(phases.size());
    
    // 複数スレッドで各最適化を並列に実行
    #pragma omp parallel for
    for (size_t i = 0; i < phases.size(); ++i) {
        results[i].changed = phases[i]->optimize(trace, results[i].newNodes);
    }
    
    // 結果をマージ
    bool changed = false;
    for (auto& result : results) {
        if (result.changed) {
            changed = true;
            // 新しいノードの追加
            for (auto& node : result.newNodes) {
                trace->addNode(std::move(node));
            }
        }
    }
    
    // 最適化が適用された場合は依存関係を更新
    if (changed) {
        updateNodeDependencies(trace);
    }
}

// 先進的型推論とSpecialization
void TraceRecorder::inferTypesAdvanced(Trace* trace) {
    if (!trace) return;
    
    // 型情報伝播のための処理順序を決定
    std::vector<TraceNode*> processingOrder;
    buildProcessingOrder(trace, processingOrder);
    
    // 型情報の収集と伝播
    std::unordered_map<TraceNode*, profiler::TypeInfo> typeInfoMap;
    for (auto* node : processingOrder) {
        if (node->isOperation()) {
            auto* op = node->as<OperationNode>();
            
            // 入力ノードの型情報を取得
            std::vector<profiler::TypeInfo> inputTypes;
            for (auto* input : op->getInputs()) {
                auto it = typeInfoMap.find(input);
                if (it != typeInfoMap.end()) {
                    inputTypes.push_back(it->second);
                } else {
                    // デフォルトの型情報
                    inputTypes.push_back(profiler::TypeInfo());
                }
            }
            
            // 操作に基づいて型推論
            profiler::TypeInfo resultType = inferTypeForOperation(op->getOpKind(), inputTypes);
            
            // 型情報記録
            typeInfoMap[node] = resultType;
            op->setTypeInfo(resultType);
            
            // 型情報に基づく最適化ヒント
            if (op->getOpKind() == OpKind::Add && 
                resultType.isProbablyNumber() && resultType.getPreciseType() == runtime::ValueType::Number) {
                // 数値加算として専用パスを使用
                op->setOptimizationHint(OptimizationHint::UseSpecializedPath);
            } else if (op->getOpKind() == OpKind::PropertyLoad && 
                      resultType.isProbablyObject()) {
                // プロパティアクセスの最適化
                op->setOptimizationHint(OptimizationHint::InlinePropertyAccess);
            }
            
            // SIMD加速のヒント
            if (canVectorize(op, resultType)) {
                op->setOptimizationHint(OptimizationHint::Vectorize);
            }
        } else if (node->isGuard()) {
            auto* guard = node->as<GuardNode>();
            auto* input = guard->getInput();
            
            // 入力の型情報を利用して不要なガードを検出
            auto it = typeInfoMap.find(input);
            if (it != typeInfoMap.end() && 
                guard->getGuardKind() == GuardKind::TypeCheck) {
                const auto& inputType = it->second;
                if (inputType.isPrecise() && 
                    inputType.getPreciseType() == guard->getExpectedType()) {
                    // このガードは冗長なので除去候補としてマーク
                    guard->setOptimizationHint(OptimizationHint::EliminateRedundantGuard);
                }
            }
            
            // 型情報の更新（ガード通過後はより精密な型情報になる）
            if (guard->getGuardKind() == GuardKind::TypeCheck) {
                profiler::TypeInfo refinedType;
                refinedType.setPreciseType(guard->getExpectedType());
                typeInfoMap[node] = refinedType;
            }
        }
    }
}

// 超高速メタトレースバイナリコード生成
void TraceRecorder::generateOptimizedMachineCode(Trace* trace, const std::vector<profiler::TypeInfo>& typeInfos) {
    if (!trace || !trace->isLoop()) return;
    
    // IR生成
    IRGraph irGraph;
    std::unordered_map<TraceNode*, IRNode*> nodeMapping;
    
    // ノードをIRに変換
    for (const auto& node : trace->getNodes()) {
        IRNode* irNode = node->toIRNode(&irGraph);
        nodeMapping[node.get()] = irNode;
    }
    
    // ノード間の依存関係を構築
    for (const auto& node : trace->getNodes()) {
        if (node->isOperation()) {
            auto* op = node->as<OperationNode>();
            IRNode* irNode = nodeMapping[node.get()];
            
            for (auto* input : op->getInputs()) {
                auto it = nodeMapping.find(input);
                if (it != nodeMapping.end()) {
                    irNode->addDependency(it->second);
                }
            }
        } else if (node->isGuard()) {
            auto* guard = node->as<GuardNode>();
            IRNode* irNode = nodeMapping[node.get()];
            
            auto* input = guard->getInput();
            auto it = nodeMapping.find(input);
            if (it != nodeMapping.end()) {
                irNode->addDependency(it->second);
            }
        }
    }
    
    // IR最適化
    irGraph.optimize(OptimizationLevel::Ultra);
    
    // 最適化済みIRからバイナリコード生成
    auto nativeCodegen = m_jit.getBackend()->createNativeCodeGenerator(m_jit.getCPUFeatures());
    std::vector<uint8_t> machineCode;
    bool success = nativeCodegen->generate(irGraph, machineCode);
    
    if (success) {
        // 生成したコードをトレースにアタッチ
        trace->setNativeCode(std::move(machineCode));
        trace->setCompiled(true);
        
        // 実行統計の初期化
        trace->resetExecutionStats();
    }
}

// AIベース超最適化
void TraceRecorder::applyAIOptimization(Trace* trace) {
    if (!trace) return;
    
    // ループのパターンを認識
    if (trace->isLoop()) {
        auto* loopHead = trace->getLoopHead();
        auto* loopEnd = trace->getLoopEnd();
        
        if (loopHead && loopEnd) {
            // ループ内のノードを収集
            std::vector<TraceNode*> loopNodes;
            bool inLoop = false;
            for (const auto& node : trace->getNodes()) {
                if (node.get() == loopHead) {
                    inLoop = true;
                }
                if (inLoop) {
                    loopNodes.push_back(node.get());
                }
                if (node.get() == loopEnd) {
                    inLoop = false;
                }
            }
            
            // パターン認識と変換
            PatternMatcher matcher;
            if (matcher.detectArrayPattern(loopNodes)) {
                // 配列操作パターンを検出
                replaceWithVectorizedArrayOp(trace, loopNodes, matcher.getPatternInfo());
            } else if (matcher.detectStringPattern(loopNodes)) {
                // 文字列操作パターンを検出
                replaceWithOptimizedStringOp(trace, loopNodes, matcher.getPatternInfo());
            } else if (matcher.detectMathPattern(loopNodes)) {
                // 数学計算パターンを検出
                replaceWithIntrinsicMathOp(trace, loopNodes, matcher.getPatternInfo());
            }
        }
    }
}

// AIが検出したパターンに基づく最適化をサポートするクラス
class PatternMatcher {
public:
    enum class PatternType {
        None,
        ArrayIteration,
        ArrayMap,
        ArrayReduce,
        StringConcat,
        StringSearch,
        MathComputation
    };
    
    struct PatternInfo {
        PatternType type;
        std::vector<TraceNode*> keyNodes;
        std::unordered_map<std::string, TraceNode*> namedNodes;
        std::string description;
    };
    
    PatternMatcher() {}
    
    bool detectArrayPattern(const std::vector<TraceNode*>& nodes) {
        // 配列イテレーションパターンの検出
        if (matchArrayIteration(nodes)) {
            m_patternInfo.type = PatternType::ArrayIteration;
            m_patternInfo.description = "Array iteration pattern";
            return true;
        } else if (matchArrayMap(nodes)) {
            m_patternInfo.type = PatternType::ArrayMap;
            m_patternInfo.description = "Array map pattern";
            return true;
        } else if (matchArrayReduce(nodes)) {
            m_patternInfo.type = PatternType::ArrayReduce;
            m_patternInfo.description = "Array reduce pattern";
            return true;
        }
        return false;
    }
    
    bool detectStringPattern(const std::vector<TraceNode*>& nodes) {
        // 文字列操作パターンの検出
        if (matchStringConcat(nodes)) {
            m_patternInfo.type = PatternType::StringConcat;
            m_patternInfo.description = "String concatenation pattern";
            return true;
        } else if (matchStringSearch(nodes)) {
            m_patternInfo.type = PatternType::StringSearch;
            m_patternInfo.description = "String search pattern";
            return true;
        }
        return false;
    }
    
    bool detectMathPattern(const std::vector<TraceNode*>& nodes) {
        // 数学計算パターンの検出
        if (matchMathComputation(nodes)) {
            m_patternInfo.type = PatternType::MathComputation;
            m_patternInfo.description = "Mathematical computation pattern";
            return true;
        }
        return false;
    }
    
    const PatternInfo& getPatternInfo() const {
        return m_patternInfo;
    }
    
private:
    bool matchArrayIteration(const std::vector<TraceNode*>& nodes) {
        // 配列イテレーションパターンのマッチング
        // 典型的なfor(let i=0; i<arr.length; i++)パターンを検出
        
        // 必要なノードを探す
        TraceNode* loopCounter = nullptr;
        TraceNode* arrayLength = nullptr;
        TraceNode* arrayAccess = nullptr;
        TraceNode* comparison = nullptr;
        
        for (auto* node : nodes) {
            if (node->isOperation()) {
                auto* op = node->as<OperationNode>();
                if (op->getOpKind() == OpKind::ElementLoad) {
                    arrayAccess = node;
                } else if (op->getOpKind() == OpKind::PropertyLoad) {
                    // length プロパティのロードを検出
                    auto* propLoad = op->as<OperationNode>();
                    // ここではシンプル化のため省略
                    arrayLength = node;
                } else if (op->getOpKind() == OpKind::LessThan) {
                    comparison = node;
                }
            }
        }
        
        // パターンの検証
        if (arrayAccess && arrayLength && comparison) {
            // キーノードを記録
            m_patternInfo.keyNodes = {arrayAccess, arrayLength, comparison};
            m_patternInfo.namedNodes["arrayAccess"] = arrayAccess;
            m_patternInfo.namedNodes["arrayLength"] = arrayLength;
            m_patternInfo.namedNodes["comparison"] = comparison;
            return true;
        }
        
        return false;
    }
    
    bool matchArrayMap(const std::vector<TraceNode*>& nodes) {
        // Array.map()のようなパターンを検出
        // 実際の実装では、もっと複雑な検出ロジックが必要
        
        // 必要なノードを探す
        TraceNode* arrayAccess = nullptr;
        TraceNode* arrayIterator = nullptr;
        TraceNode* callbackInvocation = nullptr;
        TraceNode* newArrayPush = nullptr;
        
        // map処理のパターン検出
        // 1. 配列アクセス -> 要素取得
        // 2. コールバック呼び出し
        // 3. 新しい配列への結果追加
        
        for (auto* node : nodes) {
            if (node->isOperation()) {
                auto* op = node->as<OperationNode>();
                
                if (op->getOpKind() == OpKind::ElementLoad) {
                    arrayAccess = node;
                } else if (op->getOpKind() == OpKind::ElementStore) {
                    newArrayPush = node;
                } else if (op->getOpKind() == OpKind::Call) {
                    callbackInvocation = node;
                } else if (op->getOpKind() == OpKind::Add && 
                           op->getInputs().size() == 2 && 
                           op->getInputs()[1]->isConstantOne()) {
                    // インデックスの増加を検出
                    arrayIterator = node;
                }
            }
        }
        
        // パターンの検証
        if (arrayAccess && callbackInvocation && newArrayPush && arrayIterator) {
            // キーノードを記録
            m_patternInfo.keyNodes = {arrayAccess, callbackInvocation, newArrayPush, arrayIterator};
            m_patternInfo.namedNodes["arrayAccess"] = arrayAccess;
            m_patternInfo.namedNodes["callbackInvocation"] = callbackInvocation;
            m_patternInfo.namedNodes["newArrayPush"] = newArrayPush;
            m_patternInfo.namedNodes["arrayIterator"] = arrayIterator;
            m_patternInfo.description = "Array map pattern detected";
            return true;
        }
        
        return false;
    }
    
    bool matchArrayReduce(const std::vector<TraceNode*>& nodes) {
        // Array.reduce()のようなパターンを検出
        // 必要なノードを探す
        TraceNode* arrayAccess = nullptr;
        TraceNode* accumulator = nullptr;
        TraceNode* callbackInvocation = nullptr;
        TraceNode* accumulatorUpdate = nullptr;
        
        // reduce処理のパターン検出
        // 1. 配列アクセス -> 要素取得
        // 2. コールバック呼び出し（アキュムレータと現在値）
        // 3. アキュムレータの更新
        
        for (auto* node : nodes) {
            if (node->isOperation()) {
                auto* op = node->as<OperationNode>();
                
                if (op->getOpKind() == OpKind::ElementLoad) {
                    arrayAccess = node;
                } else if (op->getOpKind() == OpKind::Call) {
                    callbackInvocation = node;
                } else if (op->getOpKind() == OpKind::StoreLocal || 
                           op->getOpKind() == OpKind::StoreGlobal) {
                    accumulatorUpdate = node;
                } else if (op->getOpKind() == OpKind::LoadLocal || 
                           op->getOpKind() == OpKind::LoadGlobal) {
                    // アキュムレータのロードを検出
                    accumulator = node;
                }
            }
        }
        
        // パターンの検証
        if (arrayAccess && callbackInvocation && accumulatorUpdate && accumulator) {
            // キーノードを記録
            m_patternInfo.keyNodes = {arrayAccess, callbackInvocation, accumulatorUpdate, accumulator};
            m_patternInfo.namedNodes["arrayAccess"] = arrayAccess;
            m_patternInfo.namedNodes["callbackInvocation"] = callbackInvocation;
            m_patternInfo.namedNodes["accumulatorUpdate"] = accumulatorUpdate;
            m_patternInfo.namedNodes["accumulator"] = accumulator;
            m_patternInfo.description = "Array reduce pattern detected";
            return true;
        }
        
        return false;
    }
    
    bool matchStringConcat(const std::vector<TraceNode*>& nodes) {
        // 文字列連結パターンを検出
        // 必要なノードを探す
        std::vector<TraceNode*> stringLoads;
        TraceNode* concatOperation = nullptr;
        
        // 文字列連結パターン検出
        // 1. 複数の文字列ロード
        // 2. 連結操作
        
        for (auto* node : nodes) {
            if (node->isOperation()) {
                auto* op = node->as<OperationNode>();
                
                if (op->getOpKind() == OpKind::LoadString || 
                    op->getOpKind() == OpKind::LoadLocal || 
                    op->getOpKind() == OpKind::LoadGlobal) {
                    // このノードが文字列型を扱っている場合（型情報があれば検証）
                    stringLoads.push_back(node);
                } else if (op->getOpKind() == OpKind::StringConcat || 
                           op->getOpKind() == OpKind::Add) {
                    // 加算演算子も文字列連結として使われる可能性がある
                    concatOperation = node;
                }
            }
        }
        
        // パターンの検証 - 少なくとも2つの文字列と連結操作が必要
        if (stringLoads.size() >= 2 && concatOperation) {
            // キーノードを記録
            m_patternInfo.keyNodes = stringLoads;
            m_patternInfo.keyNodes.push_back(concatOperation);
            m_patternInfo.namedNodes["concatOperation"] = concatOperation;
            // 文字列ロードノードも名前付きで記録
            for (size_t i = 0; i < stringLoads.size(); ++i) {
                m_patternInfo.namedNodes["stringLoad" + std::to_string(i)] = stringLoads[i];
            }
            m_patternInfo.description = "String concatenation pattern detected";
            return true;
        }
        
        return false;
    }
    
    bool matchStringSearch(const std::vector<TraceNode*>& nodes) {
        // 文字列検索パターンを検出
        // 必要なノードを探す
        TraceNode* sourceString = nullptr;
        TraceNode* searchString = nullptr;
        TraceNode* searchOperation = nullptr;
        TraceNode* conditionCheck = nullptr;
        
        // 文字列検索パターン検出
        // 1. 元の文字列ロード
        // 2. 検索文字列ロード
        // 3. 検索操作（indexOf, includes, startsWith, endsWith等）
        // 4. 検索結果の条件チェック（オプション）
        
        for (auto* node : nodes) {
            if (node->isOperation()) {
                auto* op = node->as<OperationNode>();
                
                if (op->getOpKind() == OpKind::LoadString || 
                    op->getOpKind() == OpKind::LoadLocal || 
                    op->getOpKind() == OpKind::LoadGlobal) {
                    // このノードが文字列操作の入力として使われているか確認
                    bool isUsedInStringOp = false;
                    for (auto* other : nodes) {
                        if (other->isOperation() && 
                            (other->as<OperationNode>()->getOpKind() == OpKind::StringIndexOf ||
                             other->as<OperationNode>()->getOpKind() == OpKind::StringIncludes ||
                             other->as<OperationNode>()->getOpKind() == OpKind::Call)) {
                            auto inputs = other->as<OperationNode>()->getInputs();
                            if (std::find(inputs.begin(), inputs.end(), node) != inputs.end()) {
                                isUsedInStringOp = true;
                                if (!sourceString) {
                                    sourceString = node;
                                } else if (!searchString) {
                                    searchString = node;
                                }
                                break;
                            }
                        }
                    }
                } else if (op->getOpKind() == OpKind::StringIndexOf || 
                           op->getOpKind() == OpKind::StringIncludes ||
                           op->getOpKind() == OpKind::StringStartsWith ||
                           op->getOpKind() == OpKind::StringEndsWith) {
                    searchOperation = node;
                } else if (op->getOpKind() == OpKind::Equal || 
                           op->getOpKind() == OpKind::NotEqual ||
                           op->getOpKind() == OpKind::GreaterThan ||
                           op->getOpKind() == OpKind::LessThan) {
                    // 検索結果の比較（例：indexOf >= 0）
                    conditionCheck = node;
                }
            }
        }
        
        // パターンの検証
        if (sourceString && searchOperation) {
            // キーノードを記録
            m_patternInfo.keyNodes = {sourceString, searchOperation};
            if (searchString) m_patternInfo.keyNodes.push_back(searchString);
            if (conditionCheck) m_patternInfo.keyNodes.push_back(conditionCheck);
            
            m_patternInfo.namedNodes["sourceString"] = sourceString;
            m_patternInfo.namedNodes["searchOperation"] = searchOperation;
            if (searchString) m_patternInfo.namedNodes["searchString"] = searchString;
            if (conditionCheck) m_patternInfo.namedNodes["conditionCheck"] = conditionCheck;
            
            m_patternInfo.description = "String search pattern detected";
            return true;
        }
        
        return false;
    }
    
    bool matchMathComputation(const std::vector<TraceNode*>& nodes) {
        // 数学計算パターンを検出
        // 必要なノードを探す
        std::vector<TraceNode*> mathOps;
        std::vector<TraceNode*> numericInputs;
        bool hasTrigFunction = false;
        bool hasExponentiation = false;
        bool hasComplexArithmetic = false;
        
        // 数学計算パターン検出
        // 1. 複数の数値演算操作
        // 2. 特殊な数学関数呼び出し（sin, cos, exp等）
        
        for (auto* node : nodes) {
            if (node->isOperation()) {
                auto* op = node->as<OperationNode>();
                
                // 数値演算操作
                if (op->getOpKind() == OpKind::Add || 
                    op->getOpKind() == OpKind::Sub || 
                    op->getOpKind() == OpKind::Mul || 
                    op->getOpKind() == OpKind::Div ||
                    op->getOpKind() == OpKind::Pow ||
                    op->getOpKind() == OpKind::Mod) {
                    mathOps.push_back(node);
                    
                    if (op->getOpKind() == OpKind::Pow) {
                        hasExponentiation = true;
                    }
                    
                    // 複雑な演算かどうかを判定（入力が他の演算結果を使用）
                    for (auto* input : op->getInputs()) {
                        if (input->isOperation() && 
                            (input->as<OperationNode>()->getOpKind() == OpKind::Add ||
                             input->as<OperationNode>()->getOpKind() == OpKind::Sub ||
                             input->as<OperationNode>()->getOpKind() == OpKind::Mul ||
                             input->as<OperationNode>()->getOpKind() == OpKind::Div)) {
                            hasComplexArithmetic = true;
                        }
                    }
                } 
                // 特殊数学関数
                else if (op->getOpKind() == OpKind::Call) {
                    // 関数名をチェック（可能であれば）
                    auto* callNode = op->as<CallNode>();
                    if (callNode && callNode->getFunctionName()) {
                        std::string funcName = callNode->getFunctionName()->asString();
                        if (funcName == "Math.sin" || funcName == "Math.cos" || 
                            funcName == "Math.tan" || funcName == "Math.asin" || 
                            funcName == "Math.acos" || funcName == "Math.atan") {
                            hasTrigFunction = true;
                            mathOps.push_back(node);
                        }
                        else if (funcName == "Math.exp" || funcName == "Math.log" || 
                                 funcName == "Math.sqrt" || funcName == "Math.pow") {
                            hasExponentiation = true;
                            mathOps.push_back(node);
                        }
                    }
                }
                // 数値入力
                else if (op->getOpKind() == OpKind::LoadNumber || 
                         op->getOpKind() == OpKind::LoadInt32) {
                    numericInputs.push_back(node);
                }
            }
        }
        
        // パターンの検証 - 十分な数学演算があるか
        bool isMathPattern = (mathOps.size() >= 2) || hasTrigFunction || 
                             (hasExponentiation && mathOps.size() >= 1) ||
                             hasComplexArithmetic;
        
        if (isMathPattern) {
            // キーノードを記録
            m_patternInfo.keyNodes = mathOps;
            for (auto* input : numericInputs) {
                m_patternInfo.keyNodes.push_back(input);
            }
            
            // 演算ノードを名前付きで記録
            for (size_t i = 0; i < mathOps.size(); ++i) {
                m_patternInfo.namedNodes["mathOp" + std::to_string(i)] = mathOps[i];
            }
            
            // パターン詳細を設定
            std::string details = "Math computation pattern detected: ";
            if (hasTrigFunction) details += "trigonometric functions, ";
            if (hasExponentiation) details += "exponentiation, ";
            if (hasComplexArithmetic) details += "complex arithmetic, ";
            if (!details.empty()) details = details.substr(0, details.length() - 2); // 末尾の ", " を削除
            
            m_patternInfo.description = details;
            return true;
        }
        
        return false;
    }
    
    PatternInfo m_patternInfo;
};

// SIMD最適化のためのベクトル化判定
bool TraceRecorder::canVectorize(OperationNode* op, const profiler::TypeInfo& typeInfo) {
    // 基本的な条件：数値演算で、かつSIMD対応の演算
    if (!typeInfo.isProbablyNumber()) {
        return false;
    }
    
    // ベクトル化可能な演算の種類
    switch (op->getOpKind()) {
        case OpKind::Add:
        case OpKind::Sub:
        case OpKind::Mul:
        case OpKind::Div:
        case OpKind::Min:
        case OpKind::Max:
        case OpKind::BitAnd:
        case OpKind::BitOr:
        case OpKind::BitXor:
        case OpKind::ShiftLeft:
        case OpKind::ShiftRight:
            return true;
        default:
            return false;
    }
}

// パターン検出に基づく配列操作の最適化
void TraceRecorder::replaceWithVectorizedArrayOp(Trace* trace, 
                                               const std::vector<TraceNode*>& loopNodes, 
                                               const PatternMatcher::PatternInfo& patternInfo) {
    // 配列操作のベクトル化
    if (patternInfo.type == PatternMatcher::PatternType::ArrayIteration) {
        // ループノードをSIMD最適化バージョンで置き換え
        // ここでは新しいノードを生成して置き換える
        auto arrayAccess = patternInfo.namedNodes.at("arrayAccess");
        auto arrayLength = patternInfo.namedNodes.at("arrayLength");
        
        // 新しいベクトル化操作ノード作成
        auto* vectorizedOp = new OperationNode(
            OpKind::SIMDArrayOperation,
            trace->getNextNodeId(),
            arrayAccess->getBytecodeOffset()
        );
        
        // 入力設定
        vectorizedOp->addInput(arrayAccess->as<OperationNode>()->getInputs()[0]); // 配列
        
        // 最適化ヒント
        vectorizedOp->setOptimizationHint(OptimizationHint::UseSIMD);
        
        // トレースに追加
        trace->addNode(std::unique_ptr<TraceNode>(vectorizedOp));
        
        // 古いノードへの参照を新しいノードに更新
        updateReferences(trace, arrayAccess, vectorizedOp);
    }
}

void TraceRecorder::replaceWithOptimizedStringOp(Trace* trace, 
                                               const std::vector<TraceNode*>& loopNodes, 
                                               const PatternMatcher::PatternInfo& patternInfo) {
    // 文字列操作の最適化
    if (patternInfo.type == PatternMatcher::PatternType::StringConcat) {
        // 文字列連結の最適化
        auto concatOperation = patternInfo.namedNodes.at("concatOperation");
        
        // 新しい最適化ノード作成
        auto* optimizedConcat = new OperationNode(
            OpKind::OptimizedStringConcat,
            trace->getNextNodeId(),
            concatOperation->getBytecodeOffset()
        );
        
        // 入力文字列を全て追加
        auto& namedNodes = patternInfo.namedNodes;
        for (size_t i = 0; i < patternInfo.keyNodes.size() - 1; ++i) {
            std::string name = "stringLoad" + std::to_string(i);
            if (namedNodes.find(name) != namedNodes.end()) {
                optimizedConcat->addInput(namedNodes.at(name));
            }
        }
        
        // 最適化ヒント設定
        optimizedConcat->setOptimizationHint(OptimizationHint::InlineStringOperation);
        
        // トレースに追加
        trace->addNode(std::unique_ptr<TraceNode>(optimizedConcat));
        
        // 古いノードへの参照を新しいノードに更新
        updateReferences(trace, concatOperation, optimizedConcat);
    }
    else if (patternInfo.type == PatternMatcher::PatternType::StringSearch) {
        // 文字列検索の最適化
        auto searchOperation = patternInfo.namedNodes.at("searchOperation");
        auto sourceString = patternInfo.namedNodes.at("sourceString");
        
        // 新しい最適化ノード作成
        OpKind optimizedOpKind;
        
        // 適切な最適化オペコードを選択
        auto* searchOp = searchOperation->as<OperationNode>();
        switch (searchOp->getOpKind()) {
            case OpKind::StringIndexOf:
                optimizedOpKind = OpKind::OptimizedStringIndexOf;
                break;
            case OpKind::StringIncludes:
                optimizedOpKind = OpKind::OptimizedStringIncludes;
                break;
            case OpKind::StringStartsWith:
                optimizedOpKind = OpKind::OptimizedStringStartsWith;
                break;
            case OpKind::StringEndsWith:
                optimizedOpKind = OpKind::OptimizedStringEndsWith;
                break;
            default:
                optimizedOpKind = OpKind::OptimizedStringSearch;
                break;
        }
        
        auto* optimizedSearch = new OperationNode(
            optimizedOpKind,
            trace->getNextNodeId(),
            searchOperation->getBytecodeOffset()
        );
        
        // 基本入力設定
        optimizedSearch->addInput(sourceString);
        
        // 検索文字列があれば追加
        if (patternInfo.namedNodes.find("searchString") != patternInfo.namedNodes.end()) {
            optimizedSearch->addInput(patternInfo.namedNodes.at("searchString"));
        }
        
        // 最適化ヒント設定
        optimizedSearch->setOptimizationHint(OptimizationHint::UseStringSearchIntrinsic);
        
        // SIMD対応CPUの場合、さらに追加のヒント
        if (m_jit.getCPUFeatures().hasSIMD) {
            optimizedSearch->setOptimizationHint(OptimizationHint::UseSIMD);
        }
        
        // トレースに追加
        trace->addNode(std::unique_ptr<TraceNode>(optimizedSearch));
        
        // 古いノードへの参照を新しいノードに更新
        updateReferences(trace, searchOperation, optimizedSearch);
        
        // 条件チェックの最適化（例：indexOf >= 0 をより効率的な命令に）
        if (patternInfo.namedNodes.find("conditionCheck") != patternInfo.namedNodes.end()) {
            auto conditionCheck = patternInfo.namedNodes.at("conditionCheck");
            
            // 条件チェックが単純な「結果 >= 0」または「結果 !== -1」のパターンなら最適化
            auto* condOp = conditionCheck->as<OperationNode>();
            if ((condOp->getOpKind() == OpKind::GreaterThanOrEqual && 
                condOp->getInputs().size() == 2 && 
                condOp->getInputs()[1]->isConstantZero()) ||
                (condOp->getOpKind() == OpKind::NotEqual && 
                condOp->getInputs().size() == 2 && 
                condOp->getInputs()[1]->isConstantValue(-1))) {
                
                // 一般的な「含まれているか」チェックを最適化
                auto* optimizedCheck = new OperationNode(
                    OpKind::OptimizedStringContainsCheck,
                    trace->getNextNodeId(),
                    conditionCheck->getBytecodeOffset()
                );
                
                // 入力として最適化された検索操作を設定
                optimizedCheck->addInput(optimizedSearch);
                
                // トレースに追加
                trace->addNode(std::unique_ptr<TraceNode>(optimizedCheck));
                
                // 古いノードへの参照を新しいノードに更新
                updateReferences(trace, conditionCheck, optimizedCheck);
            }
        }
    }
}

void TraceRecorder::replaceWithIntrinsicMathOp(Trace* trace, 
                                             const std::vector<TraceNode*>& loopNodes, 
                                             const PatternMatcher::PatternInfo& patternInfo) {
    // 数学計算の最適化
    if (patternInfo.type == PatternMatcher::PatternType::MathComputation) {
        bool hasTrigFunction = patternInfo.description.find("trigonometric") != std::string::npos;
        bool hasExponentiation = patternInfo.description.find("exponentiation") != std::string::npos;
        bool hasComplexArithmetic = patternInfo.description.find("complex arithmetic") != std::string::npos;
        
        // 最適化の種類を決定
        OpKind optimizedOpKind;
        OptimizationHint optimizationHint;
        
        // パターンに基づいて最適なオペコードとヒントを選択
        if (hasTrigFunction) {
            optimizedOpKind = OpKind::OptimizedTrigFunction;
            optimizationHint = OptimizationHint::UseMathIntrinsic;
        } else if (hasExponentiation) {
            optimizedOpKind = OpKind::OptimizedExponentiation;
            optimizationHint = OptimizationHint::UseFastMath;
        } else {
            // 一般的な数学計算
            optimizedOpKind = OpKind::OptimizedMathOperation;
            optimizationHint = OptimizationHint::UseFastMath;
        }
        
        // 計算の複雑さに応じてベクトル化を検討
        bool canVectorize = false;
        
        // 同じ操作が複数のデータに適用される場合はベクトル化可能
        // これは実際には複雑な解析が必要だが、ここではシンプルな判定を使用
        size_t similarOperations = 0;
        OpKind dominantOpKind = OpKind::NoOperation;
        
        // 最も頻繁な操作を検出
        std::unordered_map<OpKind, int> opCounts;
        for (auto* node : loopNodes) {
            if (node->isOperation()) {
                auto* op = node->as<OperationNode>();
                OpKind kind = op->getOpKind();
                if (kind == OpKind::Add || kind == OpKind::Sub || 
                    kind == OpKind::Mul || kind == OpKind::Div) {
                    opCounts[kind]++;
                    if (opCounts[kind] > similarOperations) {
                        similarOperations = opCounts[kind];
                        dominantOpKind = kind;
                    }
                }
            }
        }
        
        // 同じ種類の操作が3つ以上あれば、ベクトル化を検討
        canVectorize = (similarOperations >= 3);
        
        // 数学関数の集約ノードを作成
        auto* firstMathOp = nullptr;
        for (size_t i = 0; i < patternInfo.keyNodes.size(); ++i) {
            std::string name = "mathOp" + std::to_string(i);
            if (patternInfo.namedNodes.find(name) != patternInfo.namedNodes.end()) {
                auto* node = patternInfo.namedNodes.at(name);
                if (!firstMathOp && node->isOperation()) {
                    firstMathOp = node;
                    break;
                }
            }
        }
        
        if (!firstMathOp) return; // 安全チェック
        
        // 最適化ノード作成
        auto* optimizedMath = new OperationNode(
            optimizedOpKind,
            trace->getNextNodeId(),
            firstMathOp->getBytecodeOffset()
        );
        
        // マスターノードとしてのヒント設定
        optimizedMath->setOptimizationHint(optimizationHint);
        
        // ベクトル化が可能な場合
        if (canVectorize && m_jit.getCPUFeatures().hasSIMD) {
            optimizedMath->setOptimizationHint(OptimizationHint::UseSIMD);
            
            // ARM SVEが利用可能な場合、特別なヒントを追加
            if (m_jit.getCPUFeatures().hasSVE) {
                optimizedMath->setOptimizationHint(OptimizationHint::UseSVE);
            }
        }
        
        // 数学操作の集約に必要な入力を追加
        for (auto* node : patternInfo.keyNodes) {
            // 入力として使われるノードを追加
            if (node->isOperation() && !node->as<OperationNode>()->hasInputs()) {
                optimizedMath->addInput(node);
            }
        }
        
        // トレースに追加
        trace->addNode(std::unique_ptr<TraceNode>(optimizedMath));
        
        // 個別の操作ノードを最適化されたノードに置き換え
        for (size_t i = 0; i < patternInfo.keyNodes.size(); ++i) {
            std::string name = "mathOp" + std::to_string(i);
            if (patternInfo.namedNodes.find(name) != patternInfo.namedNodes.end()) {
                auto* node = patternInfo.namedNodes.at(name);
                if (node->isOperation()) {
                    updateReferences(trace, node, optimizedMath);
                }
            }
        }
        
        // 特殊パターンの最適化
        if (hasTrigFunction) {
            optimizeTrigonometricFunctions(trace, loopNodes, optimizedMath);
        } else if (hasExponentiation) {
            optimizeExponentiationOperations(trace, loopNodes, optimizedMath);
        } else if (hasComplexArithmetic) {
            optimizeComplexArithmetic(trace, loopNodes, optimizedMath, dominantOpKind);
        }
    }
}

void TraceRecorder::optimizeTrigonometricFunctions(Trace* trace, 
                                                const std::vector<TraceNode*>& loopNodes, 
                                                OperationNode* masterNode) {
    // 三角関数の最適化
    // CPUの特殊命令（Intel FMA、ARM NEON/SVEのvfma等）を活用
    
    // この実装では以下を最適化:
    // - 複数の三角関数のバッチ計算
    // - sin/cos同時計算（多くのCPUで同時計算が効率的）
    
    // FMA（Fused Multiply-Add）サポートの検出
    bool hasFMA = m_jit.getCPUFeatures().hasFMA;
    
    if (hasFMA) {
        masterNode->setOptimizationHint(OptimizationHint::UseFMA);
    }
    
    // sin/cos同時計算の検出
    bool hasSin = false;
    bool hasCos = false;
    std::vector<TraceNode*> sinNodes;
    std::vector<TraceNode*> cosNodes;
    
    for (auto* node : loopNodes) {
        if (node->isOperation() && node->as<OperationNode>()->getOpKind() == OpKind::Call) {
            auto* callNode = node->as<CallNode>();
            if (callNode && callNode->getFunctionName()) {
                std::string funcName = callNode->getFunctionName()->asString();
                if (funcName == "Math.sin") {
                    hasSin = true;
                    sinNodes.push_back(node);
                } else if (funcName == "Math.cos") {
                    hasCos = true;
                    cosNodes.push_back(node);
                }
            }
        }
    }
    
    // sin/cos同時計算の最適化
    if (hasSin && hasCos) {
        // 同一引数でsin/cosを計算する場合、同時計算を使用
        for (auto* sinNode : sinNodes) {
            for (auto* cosNode : cosNodes) {
                auto* sinOp = sinNode->as<OperationNode>();
                auto* cosOp = cosNode->as<OperationNode>();
                
                // 引数が同一かどうかを確認
                if (sinOp->getInputs().size() > 0 && cosOp->getInputs().size() > 0 &&
                    sinOp->getInputs()[0] == cosOp->getInputs()[0]) {
                    
                    auto* sinCosNode = new OperationNode(
                        OpKind::OptimizedSinCos,
                        trace->getNextNodeId(),
                        sinNode->getBytecodeOffset()
                    );
                    
                    // 引数を設定
                    sinCosNode->addInput(sinOp->getInputs()[0]);
                    
                    // 最適化ヒント
                    sinCosNode->setOptimizationHint(OptimizationHint::UseTrigIntrinsic);
                    
                    // トレースに追加
                    trace->addNode(std::unique_ptr<TraceNode>(sinCosNode));
                    
                    // sinとcos両方の参照を新しいノードに更新
                    updateReferences(trace, sinNode, sinCosNode);
                    updateReferences(trace, cosNode, sinCosNode);
                }
            }
        }
    }
}

void TraceRecorder::optimizeExponentiationOperations(Trace* trace, 
                                                  const std::vector<TraceNode*>& loopNodes, 
                                                  OperationNode* masterNode) {
    // 指数関数と対数関数の最適化
    // 特に次の数学演算を最適化:
    // - pow: 特別なケース（2の累乗、整数累乗）
    // - exp/log: 特殊命令
    
    // 整数のべき乗最適化のための解析
    for (auto* node : loopNodes) {
        if (node->isOperation()) {
            auto* op = node->as<OperationNode>();
            
            if (op->getOpKind() == OpKind::Pow) {
                // x^2 の最適化 (乗算に変換)
                if (op->getInputs().size() > 1 && op->getInputs()[1]->isConstantValue(2.0)) {
                    auto* squareNode = new OperationNode(
                        OpKind::OptimizedSquare,
                        trace->getNextNodeId(),
                        node->getBytecodeOffset()
                    );
                    
                    // 引数を設定
                    squareNode->addInput(op->getInputs()[0]);
                    
                    // 最適化ヒント
                    squareNode->setOptimizationHint(OptimizationHint::UseFastMath);
                    
                    // トレースに追加
                    trace->addNode(std::unique_ptr<TraceNode>(squareNode));
                    
                    // 参照を新しいノードに更新
                    updateReferences(trace, node, squareNode);
                }
                // 2^x の最適化 (特殊命令使用)
                else if (op->getInputs().size() > 0 && op->getInputs()[0]->isConstantValue(2.0)) {
                    auto* powTwoNode = new OperationNode(
                        OpKind::OptimizedPowTwo,
                        trace->getNextNodeId(),
                        node->getBytecodeOffset()
                    );
                    
                    // 引数を設定
                    powTwoNode->addInput(op->getInputs()[1]);
                    
                    // 最適化ヒント
                    powTwoNode->setOptimizationHint(OptimizationHint::UseFastMath);
                    
                    // トレースに追加
                    trace->addNode(std::unique_ptr<TraceNode>(powTwoNode));
                    
                    // 参照を新しいノードに更新
                    updateReferences(trace, node, powTwoNode);
                }
            }
        }
    }
}

void TraceRecorder::optimizeComplexArithmetic(Trace* trace, 
                                           const std::vector<TraceNode*>& loopNodes, 
                                           OperationNode* masterNode,
                                           OpKind dominantOpKind) {
    // 複雑な算術演算の最適化
    
    // 行列演算のパターンを検出
    bool isMatrixLike = false;
    bool hasNestedLoops = false;
    size_t arrayLoads = 0;
    size_t arrayStores = 0;
    size_t multiplyAdds = 0;
    
    // 行列演算のような特徴を検出
    for (auto* node : loopNodes) {
        if (node->isOperation()) {
            auto* op = node->as<OperationNode>();
            
            if (op->getOpKind() == OpKind::ElementLoad) {
                arrayLoads++;
            } else if (op->getOpKind() == OpKind::ElementStore) {
                arrayStores++;
            } else if (op->getOpKind() == OpKind::Mul && 
                      op->getNext() && op->getNext()->isOperation() && 
                      op->getNext()->as<OperationNode>()->getOpKind() == OpKind::Add) {
                multiplyAdds++;
            }
        }
    }
    
    // 行列演算のパターン検出（単純な判定）
    isMatrixLike = (arrayLoads >= 2 && arrayStores >= 1 && multiplyAdds >= 2);
    
    if (isMatrixLike) {
        // 行列演算向け最適化
        auto* matrixOpNode = new OperationNode(
            OpKind::OptimizedMatrixOperation,
            trace->getNextNodeId(),
            masterNode->getBytecodeOffset()
        );
        
        // マスターノードの入力を引き継ぐ
        for (auto* input : masterNode->getInputs()) {
            matrixOpNode->addInput(input);
        }
        
        // 行列操作のためのヒント設定
        matrixOpNode->setOptimizationHint(OptimizationHint::UseMatrixIntrinsic);
        
        // SIMD対応
        if (m_jit.getCPUFeatures().hasSIMD) {
            matrixOpNode->setOptimizationHint(OptimizationHint::UseSIMD);
            
            // 行列特有のSIMD拡張
            if (m_jit.getCPUFeatures().hasFMA) {
                matrixOpNode->setOptimizationHint(OptimizationHint::UseFMA);
            }
            
            // ARM SVE使用可能な場合
            if (m_jit.getCPUFeatures().hasSVE) {
                matrixOpNode->setOptimizationHint(OptimizationHint::UseSVE);
                
                // SVE2利用可能ならさらに高度な最適化
                if (m_jit.getCPUFeatures().hasSVE2) {
                    matrixOpNode->setOptimizationHint(OptimizationHint::UseSVE2);
                }
            }
        }
        
        // トレースに追加
        trace->addNode(std::unique_ptr<TraceNode>(matrixOpNode));
        
        // マスターノードへの参照を行列操作ノードに更新
        updateReferences(trace, masterNode, matrixOpNode);
    }
    else {
        // 通常の数学演算の最適化（SIMD活用）
        if (dominantOpKind != OpKind::NoOperation && m_jit.getCPUFeatures().hasSIMD) {
            // 命令ごとに最適化ノードを作成
            OpKind optimizedKind;
            switch (dominantOpKind) {
                case OpKind::Add:
                    optimizedKind = OpKind::OptimizedVectorAdd;
                    break;
                case OpKind::Sub:
                    optimizedKind = OpKind::OptimizedVectorSub;
                    break;
                case OpKind::Mul:
                    optimizedKind = OpKind::OptimizedVectorMul;
                    break;
                case OpKind::Div:
                    optimizedKind = OpKind::OptimizedVectorDiv;
                    break;
                default:
                    optimizedKind = OpKind::OptimizedVectorOp;
                    break;
            }
            
            auto* vectorOpNode = new OperationNode(
                optimizedKind,
                trace->getNextNodeId(),
                masterNode->getBytecodeOffset()
            );
            
            // マスターノードの入力を引き継ぐ
            for (auto* input : masterNode->getInputs()) {
                vectorOpNode->addInput(input);
            }
            
            // ベクトル操作のヒント設定
            vectorOpNode->setOptimizationHint(OptimizationHint::UseSIMD);
            
            // トレースに追加
            trace->addNode(std::unique_ptr<TraceNode>(vectorOpNode));
            
            // マスターノードへの参照をベクトル操作ノードに更新
            updateReferences(trace, masterNode, vectorOpNode);
        }
    }
}

// ノード間の依存関係更新
void TraceRecorder::updateNodeDependencies(Trace* trace) {
    if (!trace) return;
    
    // ノード間の依存関係をクリア
    for (auto& node : trace->getNodes()) {
        node->clearDependencies();
    }
    
    // 依存関係の再構築
    for (auto& node : trace->getNodes()) {
        if (node->isOperation()) {
            auto* op = node->as<OperationNode>();
            for (auto* input : op->getInputs()) {
                op->addDependency(input);
            }
        } else if (node->isGuard()) {
            auto* guard = node->as<GuardNode>();
            auto* input = guard->getInput();
            guard->addDependency(input);
        }
    }
}

// ノード参照の更新
void TraceRecorder::updateReferences(Trace* trace, TraceNode* oldNode, TraceNode* newNode) {
    if (!trace || !oldNode || !newNode) return;
    
    // すべてのノードを走査して、古いノードへの参照を新しいノードに更新
    for (auto& node : trace->getNodes()) {
        if (node->isOperation()) {
            auto* op = node->as<OperationNode>();
            auto& inputs = op->getInputs();
            
            for (size_t i = 0; i < inputs.size(); ++i) {
                if (inputs[i] == oldNode) {
                    inputs[i] = newNode;
                }
            }
        } else if (node->isGuard()) {
            auto* guard = node->as<GuardNode>();
            if (guard->getInput() == oldNode) {
                guard->setInput(newNode);
            }
        }
    }
}

// 型推論のための処理順序構築
void TraceRecorder::buildProcessingOrder(Trace* trace, std::vector<TraceNode*>& order) {
    if (!trace) return;
    
    // 簡易的なトポロジカルソート
    std::unordered_set<TraceNode*> visited;
    std::vector<TraceNode*> tempOrder;
    
    // 全ノードを取得
    std::vector<TraceNode*> allNodes;
    for (const auto& node : trace->getNodes()) {
        allNodes.push_back(node.get());
    }
    
    // 入次数マップの構築
    std::unordered_map<TraceNode*, int> inDegree;
    for (auto* node : allNodes) {
        inDegree[node] = 0;
    }
    
    // 入次数の計算
    for (auto* node : allNodes) {
        if (node->isOperation()) {
            auto* op = node->as<OperationNode>();
            for (auto* input : op->getInputs()) {
                inDegree[node]++;
            }
        } else if (node->isGuard()) {
            auto* guard = node->as<GuardNode>();
            inDegree[node]++;
        }
    }
    
    // 入次数が0のノードをキューに追加
    std::queue<TraceNode*> queue;
    for (auto* node : allNodes) {
        if (inDegree[node] == 0) {
            queue.push(node);
        }
    }
    
    // トポロジカルソート
    while (!queue.empty()) {
        auto* node = queue.front();
        queue.pop();
        order.push_back(node);
        
        // このノードに依存するノードの入次数を減らす
        for (auto* dependent : getDependentNodes(trace, node)) {
            inDegree[dependent]--;
            if (inDegree[dependent] == 0) {
                queue.push(dependent);
            }
        }
    }
}

// あるノードに依存するノードを取得
std::vector<TraceNode*> TraceRecorder::getDependentNodes(Trace* trace, TraceNode* node) {
    std::vector<TraceNode*> dependents;
    
    for (const auto& other : trace->getNodes()) {
        if (other->isOperation()) {
            auto* op = other->as<OperationNode>();
            for (auto* input : op->getInputs()) {
                if (input == node) {
                    dependents.push_back(other.get());
                    break;
                }
            }
        } else if (other->isGuard()) {
            auto* guard = other->as<GuardNode>();
            if (guard->getInput() == node) {
                dependents.push_back(other.get());
            }
        }
    }
    
    return dependents;
}

} // namespace metatracing
} // namespace jit
} // namespace core
} // namespace aerojs 