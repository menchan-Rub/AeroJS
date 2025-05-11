#ifndef AEROJS_CORE_JIT_METATRACING_TRACE_RECORDER_H
#define AEROJS_CORE_JIT_METATRACING_TRACE_RECORDER_H

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "../ir/ir_function.h"
#include "../ir/ir_instruction.h"
#include "../../runtime/values/value.h"

namespace aerojs {
namespace core {

/**
 * @brief 実行トレースの型情報とガード条件を保持するクラス
 */
class TraceGuard {
public:
    enum GuardType {
        TYPE_GUARD,        // 型チェック
        SHAPE_GUARD,       // オブジェクト形状チェック
        VALUE_GUARD,       // 具体値チェック
        BOUND_CHECK_GUARD  // 配列境界チェック
    };

    TraceGuard(GuardType type, uint32_t instructionIndex, uint32_t operandIndex)
        : type_(type), instructionIndex_(instructionIndex), operandIndex_(operandIndex) {}

    // ガードフェイルハンドラの実行位置
    uint32_t getDeoptPoint() const { return instructionIndex_; }
    
    // ガードの種類を返却
    GuardType getType() const { return type_; }
    
    // ガード対象のオペランドインデックス
    uint32_t getOperandIndex() const { return operandIndex_; }

private:
    GuardType type_;
    uint32_t instructionIndex_;
    uint32_t operandIndex_;
};

/**
 * @brief 型フィードバックと実行頻度情報
 */
class TypeFeedback {
public:
    void recordType(const Value& value) {
        uint32_t typeId = value.getTypeId();
        if (observedTypes_.count(typeId) == 0) {
            observedTypes_[typeId] = 1;
        } else {
            observedTypes_[typeId]++;
        }
        totalObservations_++;
    }
    
    bool isMonomorphic() const {
        return observedTypes_.size() == 1;
    }
    
    bool isPolymorphic() const {
        return observedTypes_.size() > 1 && observedTypes_.size() <= 4;
    }
    
    bool isMegamorphic() const {
        return observedTypes_.size() > 4;
    }
    
    // 観測された型の分布情報を取得
    const std::unordered_map<uint32_t, uint32_t>& getTypeDistribution() const {
        return observedTypes_;
    }
    
    // 最も頻繁に観測された型を取得
    uint32_t getDominantType() const {
        uint32_t maxCount = 0;
        uint32_t dominantType = 0;
        
        for (const auto& [typeId, count] : observedTypes_) {
            if (count > maxCount) {
                maxCount = count;
                dominantType = typeId;
            }
        }
        
        return dominantType;
    }

private:
    std::unordered_map<uint32_t, uint32_t> observedTypes_;
    uint32_t totalObservations_ = 0;
};

/**
 * @brief 実行トレース（ホットパス）の情報を格納するクラス
 */
class TraceProfile {
public:
    TraceProfile(uint64_t startPc) : startPc_(startPc), executionCount_(0) {}
    
    void incrementExecutionCount() {
        executionCount_++;
    }
    
    uint64_t getExecutionCount() const {
        return executionCount_;
    }
    
    void recordTypeInfo(uint32_t instructionIndex, const Value& value) {
        typeFeedback_[instructionIndex].recordType(value);
    }
    
    bool shouldOptimize() const {
        // 実行回数が閾値を超えたらトレース最適化を実行
        return executionCount_ >= kOptimizationThreshold;
    }
    
    void addGuard(TraceGuard::GuardType type, uint32_t instructionIndex, uint32_t operandIndex) {
        guards_.emplace_back(type, instructionIndex, operandIndex);
    }
    
    const std::vector<TraceGuard>& getGuards() const {
        return guards_;
    }
    
    const TypeFeedback& getTypeFeedback(uint32_t instructionIndex) const {
        static const TypeFeedback emptyFeedback;
        auto it = typeFeedback_.find(instructionIndex);
        if (it != typeFeedback_.end()) {
            return it->second;
        }
        return emptyFeedback;
    }
    
    uint64_t getStartPc() const {
        return startPc_;
    }

private:
    static constexpr uint64_t kOptimizationThreshold = 100; // 最適化の閾値
    
    uint64_t startPc_;
    uint64_t executionCount_;
    std::unordered_map<uint32_t, TypeFeedback> typeFeedback_;
    std::vector<TraceGuard> guards_;
};

/**
 * @brief メタトレーシングJITのトレース収集と最適化を行うクラス
 */
class TraceRecorder {
public:
    TraceRecorder();
    ~TraceRecorder();
    
    /**
     * @brief 実行箇所の記録を開始
     * 
     * @param pc 現在の実行位置
     * @return トレース記録が開始されたかどうか
     */
    bool beginRecording(uint64_t pc);
    
    /**
     * @brief トレース記録を終了
     * 
     * @param exitPc 終了位置のPC
     * @return 最適化されたIR関数オブジェクト
     */
    std::unique_ptr<IRFunction> endRecording(uint64_t exitPc);
    
    /**
     * @brief 命令実行を記録
     * 
     * @param pc 命令位置
     * @param opcode 命令コード
     * @param args 引数配列
     * @param result 実行結果
     */
    void recordExecution(uint64_t pc, uint32_t opcode, const std::vector<Value>& args, const Value& result);
    
    /**
     * @brief トレースプロファイルを取得または作成
     * 
     * @param pc プログラムカウンタ
     * @return トレースプロファイル
     */
    TraceProfile& getOrCreateProfile(uint64_t pc);
    
    /**
     * @brief トレース実行結果を記録
     * 
     * @param pc プログラムカウンタ
     * @param result 実行結果
     */
    void recordResult(uint64_t pc, const Value& result);
    
    /**
     * @brief 実行中かどうかを確認
     * 
     * @return 実行中の場合true
     */
    bool isRecording() const { return isRecording_; }
    
    /**
     * @brief 最適化されたトレースを取得
     * 
     * @param pc 開始PC
     * @return IRFunctionへのポインタ、存在しない場合はnullptr
     */
    const IRFunction* getOptimizedTrace(uint64_t pc) const;

private:
    // トレース記録状態
    bool isRecording_;
    
    // 現在記録中のトレース
    std::vector<IRInstruction> currentTrace_;
    
    // トレース開始位置
    uint64_t recordingStartPc_;
    
    // トレース終了位置
    uint64_t recordingExitPc_;
    
    // 命令位置からトレースプロファイルへのマッピング
    std::unordered_map<uint64_t, TraceProfile> traceProfiles_;
    
    // 最適化済みトレース
    std::unordered_map<uint64_t, std::unique_ptr<IRFunction>> optimizedTraces_;
    
    // 最後に記録した命令のインデックス
    uint32_t lastInstructionIndex_;
    
    /**
     * @brief トレースを最適化
     * 
     * @param trace トレース命令列
     * @param profile トレースプロファイル
     * @return 最適化されたIR関数
     */
    std::unique_ptr<IRFunction> optimizeTrace(
        const std::vector<IRInstruction>& trace,
        const TraceProfile& profile);
        
    /**
     * @brief 型情報に基づくスペキュレーション最適化
     * 
     * @param trace トレース命令列
     * @param profile プロファイル情報
     * @return 最適化されたトレース
     */
    std::vector<IRInstruction> applySpeculativeOptimizations(
        const std::vector<IRInstruction>& trace,
        const TraceProfile& profile);
        
    /**
     * @brief 脱最適化ポイントの生成
     * 
     * @param optimizedTrace 最適化済みトレース
     * @param profile トレースプロファイル
     */
    void generateDeoptimizationPoints(
        std::vector<IRInstruction>& optimizedTrace,
        const TraceProfile& profile);
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_METATRACING_TRACE_RECORDER_H 