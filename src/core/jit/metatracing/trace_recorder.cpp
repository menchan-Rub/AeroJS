#include "trace_recorder.h"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace aerojs {
namespace core {

TraceRecorder::TraceRecorder()
    : isRecording_(false),
      recordingStartPc_(0),
      recordingExitPc_(0),
      lastInstructionIndex_(0) {
}

TraceRecorder::~TraceRecorder() = default;

bool TraceRecorder::beginRecording(uint64_t pc) {
    if (isRecording_) {
        // 既に記録中
        return false;
    }
    
    // 最適化済みトレースが既に存在する場合は記録開始しない
    if (optimizedTraces_.find(pc) != optimizedTraces_.end()) {
        return false;
    }
    
    isRecording_ = true;
    recordingStartPc_ = pc;
    currentTrace_.clear();
    lastInstructionIndex_ = 0;
    
    return true;
}

std::unique_ptr<IRFunction> TraceRecorder::endRecording(uint64_t exitPc) {
    if (!isRecording_) {
        return nullptr;
    }
    
    isRecording_ = false;
    recordingExitPc_ = exitPc;
    
    // トレースが短すぎる場合は最適化しない
    if (currentTrace_.size() < 3) {
        return nullptr;
    }
    
    // 現在のトレースを最適化
    auto& profile = traceProfiles_[recordingStartPc_];
    auto optimizedTrace = optimizeTrace(currentTrace_, profile);
    
    // 最適化されたトレースを保存
    optimizedTraces_[recordingStartPc_] = std::move(optimizedTrace);
    
    return std::make_unique<IRFunction>(*(optimizedTraces_[recordingStartPc_]));
}

void TraceRecorder::recordExecution(uint64_t pc, uint32_t opcode, const std::vector<Value>& args, const Value& result) {
    // まずプロファイルを更新
    auto& profile = getOrCreateProfile(pc);
    profile.incrementExecutionCount();
    
    // 型情報を記録
    if (!args.empty()) {
        for (size_t i = 0; i < args.size(); ++i) {
            profile.recordTypeInfo(lastInstructionIndex_, args[i]);
        }
    }
    
    // 結果の型情報も記録
    profile.recordTypeInfo(lastInstructionIndex_ + 1, result);
    
    // トレースの記録中であれば命令を追加
    if (isRecording_) {
        IRInstruction instruction;
        instruction.opcode = static_cast<IROpcode>(opcode);
        
        // 引数をコピー
        for (const auto& arg : args) {
            instruction.args.push_back(arg.toInt64());
        }
        
        currentTrace_.push_back(instruction);
        lastInstructionIndex_++;
    }
    
    // 実行回数が閾値を超えたら記録開始
    if (!isRecording_ && profile.shouldOptimize()) {
        beginRecording(pc);
    }
}

TraceProfile& TraceRecorder::getOrCreateProfile(uint64_t pc) {
    auto it = traceProfiles_.find(pc);
    if (it == traceProfiles_.end()) {
        auto [newIt, _] = traceProfiles_.emplace(pc, TraceProfile(pc));
        return newIt->second;
    }
    return it->second;
}

void TraceRecorder::recordResult(uint64_t pc, const Value& result) {
    auto& profile = getOrCreateProfile(pc);
    profile.recordTypeInfo(lastInstructionIndex_, result);
}

const IRFunction* TraceRecorder::getOptimizedTrace(uint64_t pc) const {
    auto it = optimizedTraces_.find(pc);
    if (it != optimizedTraces_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::unique_ptr<IRFunction> TraceRecorder::optimizeTrace(
    const std::vector<IRInstruction>& trace,
    const TraceProfile& profile) {
    
    // 型情報に基づく投機的最適化を適用
    auto optimizedTrace = applySpeculativeOptimizations(trace, profile);
    
    // 脱最適化ポイントを生成
    generateDeoptimizationPoints(optimizedTrace, profile);
    
    // 最適化されたトレースからIR関数を生成
    auto result = std::make_unique<IRFunction>();
    result->SetInstructions(optimizedTrace);
    result->SetEntryPC(profile.getStartPc());
    
    return result;
}

std::vector<IRInstruction> TraceRecorder::applySpeculativeOptimizations(
    const std::vector<IRInstruction>& trace,
    const TraceProfile& profile) {
    
    std::vector<IRInstruction> optimized = trace;
    
    // 1. 定数伝播
    for (size_t i = 0; i < optimized.size(); ++i) {
        // パターンマッチングによる定数伝播
        if (i > 0 && optimized[i-1].opcode == IROpcode::LoadConst) {
            int64_t constValue = optimized[i-1].args[0];
            
            // 定数畳み込み: 定数同士の演算を事前計算
            if (i > 1 && optimized[i-2].opcode == IROpcode::LoadConst) {
                int64_t constValue2 = optimized[i-2].args[0];
                
                if (optimized[i].opcode == IROpcode::Add) {
                    // 加算の定数畳み込み
                    optimized[i].opcode = IROpcode::LoadConst;
                    optimized[i].args.clear();
                    optimized[i].args.push_back(constValue + constValue2);
                    // 元の定数ロード命令を削除
                    optimized[i-1].opcode = IROpcode::Nop;
                    optimized[i-2].opcode = IROpcode::Nop;
                } else if (optimized[i].opcode == IROpcode::Sub) {
                    // 減算の定数畳み込み
                    optimized[i].opcode = IROpcode::LoadConst;
                    optimized[i].args.clear();
                    optimized[i].args.push_back(constValue2 - constValue);
                    // 元の定数ロード命令を削除
                    optimized[i-1].opcode = IROpcode::Nop;
                    optimized[i-2].opcode = IROpcode::Nop;
                } else if (optimized[i].opcode == IROpcode::Mul) {
                    // 乗算の定数畳み込み
                    optimized[i].opcode = IROpcode::LoadConst;
                    optimized[i].args.clear();
                    optimized[i].args.push_back(constValue * constValue2);
                    // 元の定数ロード命令を削除
                    optimized[i-1].opcode = IROpcode::Nop;
                    optimized[i-2].opcode = IROpcode::Nop;
                } else if (optimized[i].opcode == IROpcode::Div && constValue != 0) {
                    // 除算の定数畳み込み (ゼロ除算回避)
                    optimized[i].opcode = IROpcode::LoadConst;
                    optimized[i].args.clear();
                    optimized[i].args.push_back(constValue2 / constValue);
                    // 元の定数ロード命令を削除
                    optimized[i-1].opcode = IROpcode::Nop;
                    optimized[i-2].opcode = IROpcode::Nop;
                }
            }
        }
    }
    
    // 2. 不要コード削除
    std::vector<IRInstruction> result;
    for (const auto& inst : optimized) {
        if (inst.opcode != IROpcode::Nop) {
            result.push_back(inst);
        }
    }
    
    // 3. 型特化最適化
    for (size_t i = 0; i < result.size(); ++i) {
        const auto& typeFeedback = profile.getTypeFeedback(i);
        
        // 単相的な呼び出しを特殊化
        if (typeFeedback.isMonomorphic()) {
            uint32_t dominantType = typeFeedback.getDominantType();
            
            // 特定の型に特化した命令に変換
            if (result[i].opcode == IROpcode::Add) {
                // 数値型に特化した加算命令に変換
                if (dominantType == ValueType::Number) {
                    result[i].opcode = IROpcode::AddInt;
                }
            } else if (result[i].opcode == IROpcode::Call) {
                // 型情報に基づいてインライン化
                if (dominantType == ValueType::Function) {
                    // この特定関数呼び出しの特化命令はユースケースに応じて実装
                }
            }
        }
    }
    
    return result;
}

void TraceRecorder::generateDeoptimizationPoints(
    std::vector<IRInstruction>& optimizedTrace,
    const TraceProfile& profile) {
    
    // 全てのガードに対して脱最適化ポイントを生成
    for (const auto& guard : profile.getGuards()) {
        uint32_t point = guard.getDeoptPoint();
        
        // ガードの種類に応じた脱最適化チェックを追加
        if (point < optimizedTrace.size()) {
            // 型チェックガードの追加例
            if (guard.getType() == TraceGuard::TYPE_GUARD) {
                IRInstruction checkInst;
                checkInst.opcode = IROpcode::GuardType;
                checkInst.args.push_back(static_cast<int64_t>(guard.getOperandIndex()));
                
                // ガード失敗時の脱最適化ポイントを指定
                checkInst.args.push_back(static_cast<int64_t>(point));
                
                // ガードのための型情報を取得
                const auto& typeFeedback = profile.getTypeFeedback(point);
                checkInst.args.push_back(static_cast<int64_t>(typeFeedback.getDominantType()));
                
                // チェック命令を対象位置の前に挿入
                optimizedTrace.insert(optimizedTrace.begin() + point, checkInst);
            }
        }
    }
}

} // namespace core
} // namespace aerojs 