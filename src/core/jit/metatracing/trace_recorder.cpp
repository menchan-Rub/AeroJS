#include "trace_recorder.h"
#include "tracing_jit.h"
#include "../../runtime/context/execution_context.h"
#include "../../runtime/values/value.h"
#include "../../vm/bytecode/opcode.h"
#include "../../vm/interpreter/interpreter.h"
#include "../ir/ir_builder.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>

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

/**
 * @brief コンストラクタ
 */
TraceRecorder::TraceRecorder(TracingJIT& jit)
    : m_jit(jit),
      m_isRecording(false),
      m_currentTrace(),
      m_exitReason(ExitReason::None),
      m_guardFailureCount(0),
      m_recordingDepth(0),
      m_lastEntryPoint(nullptr),
      m_statisticsEnabled(true)
{
    initializeEmptyTrace();
}

/**
 * @brief デストラクタ
 */
TraceRecorder::~TraceRecorder() {
    // リソースクリーンアップ
    reset();
}

/**
 * @brief トレース記録を開始する
 * @param context 実行コンテキスト
 * @param entryPoint トレース開始位置
 * @return 記録開始に成功したらtrue
 */
bool TraceRecorder::startRecording(execution::ExecutionContext* context, const bytecode::BytecodeAddress& entryPoint) {
    if (m_isRecording) {
        // 既に記録中の場合、ネストしたトレースとして処理
        if (m_recordingDepth >= MAX_NESTED_TRACE_DEPTH) {
            // ネストの深さが限界を超えた場合は記録しない
        return false;
        }
        m_recordingDepth++;
        return true;
    }
    
    // 新しいトレースを初期化
    initializeEmptyTrace();
    
    m_isRecording = true;
    m_recordingDepth = 1;
    m_lastEntryPoint = entryPoint;
    m_currentTrace.entryPoint = entryPoint;
    m_currentTrace.startTimestamp = getCurrentTimestamp();
    m_currentTrace.contextSnapshot = std::make_unique<ContextSnapshot>(context);
    
    // トレース開始命令を記録
    TraceInstruction startInstr;
    startInstr.opcode = TraceOpcode::TraceStart;
    startInstr.location = entryPoint;
    startInstr.timestamp = getCurrentTimestamp();
    m_currentTrace.instructions.push_back(startInstr);
    
    // 統計情報を記録
    m_jit.recordTraceStartEvent(entryPoint);
    
    return true;
}

/**
 * @brief トレース記録を強制終了する
 * @param reason 終了理由
 */
void TraceRecorder::abortRecording(ExitReason reason) {
    if (!m_isRecording) {
        return;
    }
    
    // ネストしたトレースの場合、深さを減らすだけ
    if (m_recordingDepth > 1) {
        m_recordingDepth--;
        return;
    }
    
    m_exitReason = reason;
    m_isRecording = false;
    m_recordingDepth = 0;
    
    // 統計情報を記録
    if (m_statisticsEnabled) {
        m_jit.recordTraceAbortEvent(m_currentTrace.entryPoint, reason);
    }
    
    // トレースをクリア
    initializeEmptyTrace();
}

/**
 * @brief トレース記録を完了する
 * @return 記録されたトレース
 */
std::unique_ptr<TraceRecorder::Trace> TraceRecorder::finishRecording() {
    if (!m_isRecording) {
        return nullptr;
    }
    
    // ネストしたトレースの場合、深さを減らすだけ
    if (m_recordingDepth > 1) {
        m_recordingDepth--;
        return nullptr;
    }
    
    // トレース終了命令を記録
    TraceInstruction endInstr;
    endInstr.opcode = TraceOpcode::TraceEnd;
    endInstr.timestamp = getCurrentTimestamp();
    m_currentTrace.instructions.push_back(endInstr);
    
    // トレース実行時間を計算
    m_currentTrace.executionTimeNs = endInstr.timestamp - m_currentTrace.startTimestamp;
    
    // 統計情報を記録
    if (m_statisticsEnabled) {
        m_jit.recordTraceCompletionEvent(m_currentTrace.entryPoint, 
                                         m_currentTrace.instructions.size(),
                                         m_currentTrace.executionTimeNs);
    }
    
    // 結果を返すためにトレースのコピーを作成
    std::unique_ptr<Trace> result = std::make_unique<Trace>();
    result->entryPoint = m_currentTrace.entryPoint;
    result->exitPoint = m_currentTrace.exitPoint;
    result->startTimestamp = m_currentTrace.startTimestamp;
    result->executionTimeNs = m_currentTrace.executionTimeNs;
    result->exitReason = m_exitReason;
    
    // コンテキストスナップショットのコピー
    if (m_currentTrace.contextSnapshot) {
        result->contextSnapshot = std::make_unique<ContextSnapshot>(*m_currentTrace.contextSnapshot);
    }
    
    // 命令リストのコピー
    result->instructions.resize(m_currentTrace.instructions.size());
    optimizedCopy(m_currentTrace.instructions.data(), 
                  result->instructions.data(),
                  m_currentTrace.instructions.size());
    
    // サイドエグジットのコピー
    result->sideExits.resize(m_currentTrace.sideExits.size());
    for (size_t i = 0; i < m_currentTrace.sideExits.size(); i++) {
        result->sideExits[i] = m_currentTrace.sideExits[i];
    }
    
    // 統計情報のコピー
    result->executedBytecodes = m_currentTrace.executedBytecodes;
    
    // 現在のトレースをリセット
    m_isRecording = false;
    m_recordingDepth = 0;
    m_exitReason = ExitReason::None;
    initializeEmptyTrace();
    
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
    if (!m_isRecording) {
        return;
    }
    
    // 最大トレース長のチェック
    if (m_currentTrace.instructions.size() >= MAX_TRACE_LENGTH) {
        abortRecording(ExitReason::TraceTooLong);
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
    if (m_currentTrace.instructions.size() % STACK_SNAPSHOT_INTERVAL == 0) {
        instr.hasStackSnapshot = true;
        instr.stackSnapshot = captureStackSnapshot(context);
    }
    
    m_currentTrace.instructions.push_back(instr);
    m_currentTrace.executedBytecodes++;
    
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
    if (!m_isRecording) {
        return true; // 記録中でなければガードは常に成功したと見なす
    }
    
    // ガード命令を記録
    TraceInstruction instr;
    instr.opcode = TraceOpcode::Guard;
    instr.location = location;
    instr.guardCondition = condition;
    instr.expectedType = expectedType;
    instr.operands.push_back(actualValue);
    instr.timestamp = getCurrentTimestamp();
    
    m_currentTrace.instructions.push_back(instr);
    
    // ガードの実際の評価
    bool guardSuccess = evaluateGuard(condition, expectedType, actualValue);
    
    if (!guardSuccess) {
        // ガード失敗を記録
        recordGuardFailure(context, location, condition, expectedType, actualValue);
        m_guardFailureCount++;
        
        // ガード失敗が多すぎる場合はトレースを中止
        if (m_guardFailureCount > MAX_GUARD_FAILURES) {
            abortRecording(ExitReason::TooManyGuardFailures);
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
    if (!m_isRecording) {
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
    
    m_currentTrace.instructions.push_back(instr);
    
    // サイドエグジット情報を保存
    SideExit exit;
    exit.location = location;
    exit.type = exitType;
    exit.instructionIndex = m_currentTrace.instructions.size() - 1;
    exit.contextSnapshot = std::make_unique<ContextSnapshot>(context);
    
    m_currentTrace.sideExits.push_back(std::move(exit));
    
    // サイドエグジットが多すぎる場合はトレースを中止
    if (m_currentTrace.sideExits.size() > MAX_SIDE_EXITS) {
        abortRecording(ExitReason::TooManySideExits);
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
    if (!m_isRecording) {
        return;
    }
    
    // 最適化ヒント命令を記録
    TraceInstruction instr;
    instr.opcode = TraceOpcode::OptimizationHint;
    instr.location = location;
    instr.optimizationHint = hint;
    instr.operands.push_back(data);
    instr.timestamp = getCurrentTimestamp();
    
    m_currentTrace.instructions.push_back(instr);
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
    
    m_currentTrace.instructions.push_back(instr);
    
    // ガード失敗に対応するサイドエグジットを記録
    SideExit exit;
    exit.location = location;
    exit.type = SideExitType::GuardFailure;
    exit.instructionIndex = m_currentTrace.instructions.size() - 1;
    exit.contextSnapshot = std::make_unique<ContextSnapshot>(context);
    exit.guardCondition = condition;
    exit.expectedType = expectedType;
    exit.actualValue = actualValue;
    
    m_currentTrace.sideExits.push_back(std::move(exit));
}

/**
 * @brief トレースレコーダーをリセットする
 */
void TraceRecorder::reset() {
    m_isRecording = false;
    m_recordingDepth = 0;
    m_exitReason = ExitReason::None;
    m_guardFailureCount = 0;
    m_loopIterationCount = 0;
    m_lastEntryPoint = nullptr;
    
    initializeEmptyTrace();
}

/**
 * @brief トレース記録中かどうかを返す
 */
bool TraceRecorder::isRecording() const {
    return m_isRecording;
}

/**
 * @brief 現在の記録深度を返す
 */
int TraceRecorder::getRecordingDepth() const {
    return m_recordingDepth;
}

/**
 * @brief 最後のエントリーポイントを返す
 */
const bytecode::BytecodeAddress* TraceRecorder::getLastEntryPoint() const {
    return m_lastEntryPoint;
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
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
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
    m_currentTrace.instructions.clear();
    m_currentTrace.sideExits.clear();
    m_currentTrace.entryPoint = nullptr;
    m_currentTrace.exitPoint = nullptr;
    m_currentTrace.startTimestamp = 0;
    m_currentTrace.executionTimeNs = 0;
    m_currentTrace.executedBytecodes = 0;
    m_currentTrace.contextSnapshot.reset();
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

} // namespace metatracing
} // namespace jit
} // namespace core
} // namespace aerojs 