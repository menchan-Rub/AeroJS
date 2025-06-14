/**
 * @file trace_optimizer.cpp
 * @brief メタトレーシングJITのトレース最適化フレームワーク実装
 * 
 * このファイルは、実行時にホットパスを検出して最適化するための
 * メタトレーシングJITの最適化エンジンを実装します。
 * 
 * @author AeroJS Team
 * @version 3.0.0
 * @copyright MIT License
 */

#include "trace_optimizer.h"
#include "../ir/ir_instruction.h"
#include "../ir/ir_function.h"
#include "../ir/execution_trace.h"
#include "../ir/ir_builder.h"
#include "../../runtime/values/value.h"
#include "../../utils/logging.h"
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <immintrin.h>  // SIMD命令用

namespace aerojs {
namespace core {

std::unique_ptr<IRFunction> TraceOptimizer::OptimizeTrace(const ExecutionTrace& trace) {
    // トレースからIR関数を生成
    auto irBuilder = std::make_unique<IRBuilder>();
    auto function = irBuilder->BuildFromTrace(trace);
    
    if (!function) {
        return nullptr;
    }
    
    // 最適化を適用
    OptimizationResult result = OptimizeIR(*function);
    
    LOG_DEBUG("トレース最適化完了: 削除命令数={}, 型特殊化数={}, ベクトル化ループ数={}, インライン関数数={}",
              result.eliminatedInstructions, result.specializedTypes, 
              result.vectorizedLoops, result.inlinedFunctions);
    
    return function;
}

OptimizationResult TraceOptimizer::OptimizeIR(IRFunction& function) {
    OptimizationResult totalResult;
    optimizationPassCount_ = 0;
    
    // 前処理：制御フロー解析
    AnalyzeDataFlow(function);
    BuildDependencyGraph(function);
    DetectLoops(function);
    InferTypes(function);
    
    bool changed = true;
    while (changed && optimizationPassCount_ < MAX_OPTIMIZATION_PASSES) {
        changed = false;
        optimizationPassCount_++;
        
        // 最適化フェーズを順番に適用
        if (enabledPhases_[static_cast<int>(OptimizationPhase::REDUNDANCY_ELIMINATION)]) {
            auto result = EliminateRedundancy(function);
            changed |= result.changed;
            totalResult.eliminatedInstructions += result.eliminatedInstructions;
        }
        
        if (enabledPhases_[static_cast<int>(OptimizationPhase::CONSTANT_FOLDING)]) {
            auto result = FoldConstants(function);
            changed |= result.changed;
            totalResult.eliminatedInstructions += result.eliminatedInstructions;
        }
        
        if (enabledPhases_[static_cast<int>(OptimizationPhase::DEAD_CODE_ELIMINATION)]) {
            auto result = EliminateDeadCode(function);
            changed |= result.changed;
            totalResult.eliminatedInstructions += result.eliminatedInstructions;
        }
        
        if (enabledPhases_[static_cast<int>(OptimizationPhase::TYPE_SPECIALIZATION)]) {
            auto result = SpecializeTypes(function);
            changed |= result.changed;
            totalResult.specializedTypes += result.specializedTypes;
        }
        
        if (enabledPhases_[static_cast<int>(OptimizationPhase::LOOP_INVARIANT_HOISTING)]) {
            auto result = HoistLoopInvariants(function);
            changed |= result.changed;
        }
        
        if (enabledPhases_[static_cast<int>(OptimizationPhase::COMMON_SUBEXPRESSION)]) {
            auto result = EliminateCommonSubexpressions(function);
            changed |= result.changed;
            totalResult.eliminatedInstructions += result.eliminatedInstructions;
        }
        
        if (enabledPhases_[static_cast<int>(OptimizationPhase::STRENGTH_REDUCTION)]) {
            auto result = ReduceStrength(function);
            changed |= result.changed;
        }
        
        if (enabledPhases_[static_cast<int>(OptimizationPhase::VECTORIZATION)]) {
            auto result = Vectorize(function);
            changed |= result.changed;
            totalResult.vectorizedLoops += result.vectorizedLoops;
        }
        
        // データフロー解析を再実行（最適化により変更されたため）
        if (changed) {
            AnalyzeDataFlow(function);
        }
    }
    
    // 後処理最適化
    if (enabledPhases_[static_cast<int>(OptimizationPhase::INLINING)]) {
        auto result = InlineFunctions(function);
        totalResult.inlinedFunctions += result.inlinedFunctions;
    }
    
    if (enabledPhases_[static_cast<int>(OptimizationPhase::ESCAPE_ANALYSIS)]) {
        AnalyzeEscape(function);
    }
    
    if (enabledPhases_[static_cast<int>(OptimizationPhase::TAIL_CALL_OPTIMIZATION)]) {
        OptimizeTailCalls(function);
    }
    
    totalResult.changed = optimizationPassCount_ > 1;
    
    return totalResult;
}

OptimizationResult TraceOptimizer::EliminateRedundancy(IRFunction& function) {
    OptimizationResult result;
    auto& instructions = function.GetInstructions();
    
    // 値番号付けによる冗長命令の検出
    std::unordered_map<std::string, size_t> valueNumbers;
    std::vector<bool> toRemove(instructions.size(), false);
    
    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto& instr = instructions[i];
        
        // 副作用のない命令のみ対象
        if (instr.HasSideEffects()) {
            continue;
        }
        
        // 命令のハッシュ値を計算
        std::string instrHash = instr.ComputeHash();
        
        auto it = valueNumbers.find(instrHash);
        if (it != valueNumbers.end()) {
            // 冗長命令を発見
            size_t originalIndex = it->second;
            
            // 使用箇所を置き換え
            function.ReplaceAllUsesWith(i, originalIndex);
            toRemove[i] = true;
            result.eliminatedInstructions++;
            result.changed = true;
        } else {
            valueNumbers[instrHash] = i;
        }
    }
    
    // 冗長命令を削除
    function.RemoveInstructions(toRemove);
    
    return result;
}

OptimizationResult TraceOptimizer::FoldConstants(IRFunction& function) {
    OptimizationResult result;
    auto& instructions = function.GetInstructions();
    
    for (size_t i = 0; i < instructions.size(); ++i) {
        auto& instr = instructions[i];
        
        // 定数オペランドのみの場合、コンパイル時に計算
        if (instr.AllOperandsAreConstants()) {
            Value constantResult = instr.EvaluateConstant();
            
            if (!constantResult.IsUndefined()) {
                // 定数命令に置き換え
                instr = IRInstruction::CreateConstant(constantResult);
                result.eliminatedInstructions++;
                result.changed = true;
            }
        }
        
        // 特定パターンの最適化
        switch (instr.GetOpcode()) {
            case IROpcode::ADD:
                if (instr.GetOperand(1).IsConstant() && instr.GetOperand(1).AsNumber() == 0.0) {
                    // x + 0 = x
                    function.ReplaceAllUsesWith(i, instr.GetOperand(0).GetSSAIndex());
                    instr.MarkAsDeleted();
                    result.changed = true;
                }
                break;
                
            case IROpcode::MUL:
                if (instr.GetOperand(1).IsConstant()) {
                    double val = instr.GetOperand(1).AsNumber();
                    if (val == 1.0) {
                        // x * 1 = x
                        function.ReplaceAllUsesWith(i, instr.GetOperand(0).GetSSAIndex());
                        instr.MarkAsDeleted();
                        result.changed = true;
                    } else if (val == 0.0) {
                        // x * 0 = 0
                        instr = IRInstruction::CreateConstant(Value(0.0));
                        result.changed = true;
                    }
                }
                break;
                
            case IROpcode::DIV:
                if (instr.GetOperand(1).IsConstant() && instr.GetOperand(1).AsNumber() == 1.0) {
                    // x / 1 = x
                    function.ReplaceAllUsesWith(i, instr.GetOperand(0).GetSSAIndex());
                    instr.MarkAsDeleted();
                    result.changed = true;
                }
                break;
                
            default:
                break;
        }
    }
    
    return result;
}

OptimizationResult TraceOptimizer::EliminateDeadCode(IRFunction& function) {
    OptimizationResult result;
    auto& instructions = function.GetInstructions();
    
    // 到達可能性解析
    reachableInstructions_.assign(instructions.size(), false);
    std::queue<size_t> workList;
    
    // エントリポイントをマーク
    if (!instructions.empty()) {
        reachableInstructions_[0] = true;
        workList.push(0);
    }
    
    // 到達可能な命令をマーク
    while (!workList.empty()) {
        size_t current = workList.front();
        workList.pop();
        
        const auto& instr = instructions[current];
        
        // 後続命令をマーク
        for (size_t successor : successors_[current]) {
            if (!reachableInstructions_[successor]) {
                reachableInstructions_[successor] = true;
                workList.push(successor);
            }
        }
        
        // 使用されている命令をマーク
        for (const auto& operand : instr.GetOperands()) {
            if (operand.IsSSAValue()) {
                size_t defIndex = operand.GetSSAIndex();
                if (!reachableInstructions_[defIndex]) {
                    reachableInstructions_[defIndex] = true;
                    workList.push(defIndex);
                }
            }
        }
    }
    
    // 到達不可能な命令を削除
    std::vector<bool> toRemove(instructions.size(), false);
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (!reachableInstructions_[i] && !instructions[i].HasSideEffects()) {
            toRemove[i] = true;
            result.eliminatedInstructions++;
            result.changed = true;
        }
    }
    
    function.RemoveInstructions(toRemove);
    
    return result;
}

OptimizationResult TraceOptimizer::SpecializeTypes(IRFunction& function) {
    OptimizationResult result;
    auto& instructions = function.GetInstructions();
    
    for (size_t i = 0; i < instructions.size(); ++i) {
        auto& instr = instructions[i];
        ValueType inferredType = inferredTypes_[i];
        
        // 型特殊化ハンドラが登録されている場合
        auto it = typeHandlers_.find(inferredType);
        if (it != typeHandlers_.end()) {
            IRInstruction specialized = it->second(instr, inferredType);
            if (specialized != instr) {
                instr = specialized;
                result.specializedTypes++;
                result.changed = true;
            }
        }
        
        // 組み込み型特殊化
        switch (instr.GetOpcode()) {
            case IROpcode::ADD:
                if (inferredType == ValueType::INTEGER) {
                    instr.SetOpcode(IROpcode::IADD);
                    result.specializedTypes++;
                    result.changed = true;
                } else if (inferredType == ValueType::DOUBLE) {
                    instr.SetOpcode(IROpcode::FADD);
                    result.specializedTypes++;
                    result.changed = true;
                }
                break;
                
            case IROpcode::MUL:
                if (inferredType == ValueType::INTEGER) {
                    instr.SetOpcode(IROpcode::IMUL);
                    result.specializedTypes++;
                    result.changed = true;
                } else if (inferredType == ValueType::DOUBLE) {
                    instr.SetOpcode(IROpcode::FMUL);
                    result.specializedTypes++;
                    result.changed = true;
                }
                break;
                
            case IROpcode::LOAD_PROPERTY:
                if (inferredType != ValueType::UNKNOWN) {
                    // 型が判明している場合は型チェックを省略
                    instr.AddFlag(IRInstructionFlag::SKIP_TYPE_CHECK);
                    result.specializedTypes++;
                    result.changed = true;
                }
                break;
                
            default:
                break;
        }
    }
    
    return result;
}

OptimizationResult TraceOptimizer::HoistLoopInvariants(IRFunction& function) {
    OptimizationResult result;
    auto& instructions = function.GetInstructions();
    
    for (size_t loopHeader : loopHeaders_) {
        // ループ内の不変式を検出
        std::vector<size_t> invariants;
        
        for (size_t i = loopHeader; i < instructions.size(); ++i) {
            if (loopNestDepth_[i] == 0) break;  // ループを抜けた
            
            const auto& instr = instructions[i];
            
            // 副作用がなく、オペランドがすべてループ外で定義されている
            if (!instr.HasSideEffects() && AreAllOperandsLoopInvariant(instr, loopHeader)) {
                invariants.push_back(i);
            }
        }
        
        // 不変式をループ前に移動
        for (size_t invariantIndex : invariants) {
            function.MoveInstructionBefore(invariantIndex, loopHeader);
            result.changed = true;
        }
    }
    
    return result;
}

OptimizationResult TraceOptimizer::EliminateCommonSubexpressions(IRFunction& function) {
    OptimizationResult result;
    auto& instructions = function.GetInstructions();
    
    // 利用可能式解析
    std::unordered_map<std::string, size_t> availableExpressions;
    std::vector<bool> toRemove(instructions.size(), false);
    
    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto& instr = instructions[i];
        
        if (instr.HasSideEffects()) {
            // 副作用のある命令は利用可能式を無効化
            availableExpressions.clear();
            continue;
        }
        
        std::string exprHash = instr.ComputeExpressionHash();
        
        auto it = availableExpressions.find(exprHash);
        if (it != availableExpressions.end()) {
            // 共通部分式を発見
            size_t originalIndex = it->second;
            
            // オペランドが変更されていないことを確認
            if (AreOperandsUnchanged(originalIndex, i, function)) {
                function.ReplaceAllUsesWith(i, originalIndex);
                toRemove[i] = true;
                result.eliminatedInstructions++;
                result.changed = true;
            }
        } else {
            availableExpressions[exprHash] = i;
        }
    }
    
    function.RemoveInstructions(toRemove);
    
    return result;
}

OptimizationResult TraceOptimizer::ReduceStrength(IRFunction& function) {
    OptimizationResult result;
    auto& instructions = function.GetInstructions();
    
    for (size_t i = 0; i < instructions.size(); ++i) {
        auto& instr = instructions[i];
        
        switch (instr.GetOpcode()) {
            case IROpcode::MUL:
                // x * 2^n -> x << n
                if (instr.GetOperand(1).IsConstant()) {
                    double val = instr.GetOperand(1).AsNumber();
                    if (IsPowerOfTwo(val)) {
                        int shift = static_cast<int>(std::log2(val));
                        instr.SetOpcode(IROpcode::SHL);
                        instr.SetOperand(1, IROperand::CreateConstant(Value(shift)));
                        result.changed = true;
                    }
                }
                break;
                
            case IROpcode::DIV:
                // x / 2^n -> x >> n (符号なし除算の場合)
                if (instr.GetOperand(1).IsConstant()) {
                    double val = instr.GetOperand(1).AsNumber();
                    if (IsPowerOfTwo(val) && inferredTypes_[i] == ValueType::INTEGER) {
                        int shift = static_cast<int>(std::log2(val));
                        instr.SetOpcode(IROpcode::SHR);
                        instr.SetOperand(1, IROperand::CreateConstant(Value(shift)));
                        result.changed = true;
                    }
                }
                break;
                
            case IROpcode::MOD:
                // x % 2^n -> x & (2^n - 1)
                if (instr.GetOperand(1).IsConstant()) {
                    double val = instr.GetOperand(1).AsNumber();
                    if (IsPowerOfTwo(val)) {
                        int mask = static_cast<int>(val) - 1;
                        instr.SetOpcode(IROpcode::AND);
                        instr.SetOperand(1, IROperand::CreateConstant(Value(mask)));
                        result.changed = true;
                    }
                }
                break;
                
            default:
                break;
        }
    }
    
    return result;
}

OptimizationResult TraceOptimizer::InlineFunctions(IRFunction& function) {
    OptimizationResult result;
    auto& instructions = function.GetInstructions();
    
    for (size_t i = 0; i < instructions.size(); ++i) {
        auto& instr = instructions[i];
        
        if (instr.GetOpcode() == IROpcode::CALL) {
            // インライン候補かどうか判定
            const auto& target = instr.GetOperand(0);
            
            if (target.IsFunction() && ShouldInlineFunction(target.AsFunction())) {
                // 関数をインライン化
                auto inlinedInstructions = target.AsFunction().GetInstructions();
                
                // パラメータをマッピング
                std::unordered_map<size_t, size_t> parameterMapping;
                for (size_t j = 1; j < instr.GetOperandCount(); ++j) {
                    parameterMapping[j - 1] = instr.GetOperand(j).GetSSAIndex();
                }
                
                // インライン化された命令を挿入
                function.InlineInstructions(i, inlinedInstructions, parameterMapping);
                
                result.inlinedFunctions++;
                result.changed = true;
            }
        }
    }
    
    return result;
}

OptimizationResult TraceOptimizer::AnalyzeEscape(IRFunction& function) {
    OptimizationResult result;
    auto& instructions = function.GetInstructions();
    
    // エスケープ解析：ヒープ割り当てをスタック割り当てに変更できるかを判定
    for (size_t i = 0; i < instructions.size(); ++i) {
        auto& instr = instructions[i];
        
        if (instr.GetOpcode() == IROpcode::NEW_OBJECT) {
            bool escapes = false;
            
            // オブジェクトが関数外に漏れるかチェック
            for (size_t useIndex : uses_[i]) {
                const auto& useInstr = instructions[useIndex];
                
                if (useInstr.GetOpcode() == IROpcode::RETURN ||
                    useInstr.GetOpcode() == IROpcode::STORE_GLOBAL ||
                    useInstr.GetOpcode() == IROpcode::CALL) {
                    escapes = true;
                    break;
                }
            }
            
            if (!escapes) {
                // スタック割り当てに変更
                instr.SetOpcode(IROpcode::ALLOCA);
                instr.AddFlag(IRInstructionFlag::STACK_ALLOCATED);
                result.changed = true;
            }
        }
    }
    
    return result;
}

OptimizationResult TraceOptimizer::OptimizeTailCalls(IRFunction& function) {
    OptimizationResult result;
    auto& instructions = function.GetInstructions();
    
    // 末尾再帰の検出と最適化
    if (!instructions.empty()) {
        const auto& lastInstr = instructions.back();
        
        if (lastInstr.GetOpcode() == IROpcode::RETURN && lastInstr.GetOperandCount() > 0) {
            const auto& returnValue = lastInstr.GetOperand(0);
            
            if (returnValue.IsSSAValue()) {
                size_t defIndex = returnValue.GetSSAIndex();
                const auto& defInstr = instructions[defIndex];
                
                if (defInstr.GetOpcode() == IROpcode::CALL) {
                    const auto& target = defInstr.GetOperand(0);
                    
                    // 自分自身への呼び出しかチェック
                    if (target.IsFunction() && target.AsFunction().GetName() == function.GetName()) {
                        // 末尾再帰をループに変換
                        ConvertTailRecursionToLoop(function, defIndex);
                        result.changed = true;
                    }
                }
            }
        }
    }
    
    return result;
}

OptimizationResult TraceOptimizer::Vectorize(IRFunction& function) {
    OptimizationResult result;
    auto& instructions = function.GetInstructions();
    
    // ループのベクトル化
    for (size_t loopHeader : loopHeaders_) {
        if (CanVectorizeLoop(function, loopHeader)) {
            VectorizeLoop(function, loopHeader);
            result.vectorizedLoops++;
            result.changed = true;
        }
    }
    
    return result;
}

void TraceOptimizer::AnalyzeDataFlow(IRFunction& function) {
    auto& instructions = function.GetInstructions();
    size_t numInstructions = instructions.size();
    
    // 初期化
    uses_.assign(numInstructions, std::vector<size_t>());
    defs_.assign(numInstructions, std::vector<size_t>());
    
    // def-use チェーンの構築
    for (size_t i = 0; i < numInstructions; ++i) {
        const auto& instr = instructions[i];
        
        // 定義
        if (instr.DefinesValue()) {
            defs_[i].push_back(i);
        }
        
        // 使用
        for (const auto& operand : instr.GetOperands()) {
            if (operand.IsSSAValue()) {
                size_t defIndex = operand.GetSSAIndex();
                uses_[defIndex].push_back(i);
            }
        }
    }
}

void TraceOptimizer::BuildDependencyGraph(IRFunction& function) {
    auto& instructions = function.GetInstructions();
    size_t numInstructions = instructions.size();
    
    predecessors_.assign(numInstructions, std::vector<size_t>());
    successors_.assign(numInstructions, std::vector<size_t>());
    
    // 制御フロー依存の構築
    for (size_t i = 0; i < numInstructions - 1; ++i) {
        const auto& instr = instructions[i];
        
        if (instr.IsTerminator()) {
            // 分岐命令の場合
            for (size_t target : instr.GetBranchTargets()) {
                successors_[i].push_back(target);
                predecessors_[target].push_back(i);
            }
        } else {
            // 通常の順次実行
            successors_[i].push_back(i + 1);
            predecessors_[i + 1].push_back(i);
        }
    }
}

void TraceOptimizer::DetectLoops(IRFunction& function) {
    auto& instructions = function.GetInstructions();
    size_t numInstructions = instructions.size();
    
    loopHeaders_.clear();
    loopNestDepth_.assign(numInstructions, 0);
    
    // バックエッジの検出によるループ検出
    std::vector<bool> visited(numInstructions, false);
    std::vector<bool> inStack(numInstructions, false);
    std::vector<size_t> dfsStack;
    
    std::function<void(size_t)> dfs = [&](size_t node) {
        visited[node] = true;
        inStack[node] = true;
        dfsStack.push_back(node);
        
        for (size_t successor : successors_[node]) {
            if (inStack[successor]) {
                // バックエッジを発見
                loopHeaders_.insert(successor);
                
                // ループ内の命令にネスト深度を設定
                for (auto it = std::find(dfsStack.begin(), dfsStack.end(), successor);
                     it != dfsStack.end(); ++it) {
                    loopNestDepth_[*it]++;
                }
            } else if (!visited[successor]) {
                dfs(successor);
            }
        }
        
        inStack[node] = false;
        dfsStack.pop_back();
    };
    
    if (!instructions.empty()) {
        dfs(0);
    }
}

void TraceOptimizer::InferTypes(IRFunction& function) {
    auto& instructions = function.GetInstructions();
    size_t numInstructions = instructions.size();
    
    inferredTypes_.assign(numInstructions, ValueType::UNKNOWN);
    
    // データフロー解析による型推論
    bool changed = true;
    while (changed) {
        changed = false;
        
        for (size_t i = 0; i < numInstructions; ++i) {
            const auto& instr = instructions[i];
            ValueType newType = InferInstructionType(instr, i);
            
            if (newType != inferredTypes_[i]) {
                inferredTypes_[i] = newType;
                changed = true;
            }
        }
    }
}

void TraceOptimizer::PropagateTypes(IRFunction& function) {
    // 型情報を前方伝播
    InferTypes(function);
    
    // 後方伝播による精密化
    auto& instructions = function.GetInstructions();
    
    for (int i = static_cast<int>(instructions.size()) - 1; i >= 0; --i) {
        const auto& instr = instructions[i];
        
        // 使用箇所から型制約を逆算
        for (size_t useIndex : uses_[i]) {
            ValueType constraintType = GetTypeConstraintFromUse(instructions[useIndex], i);
            if (constraintType != ValueType::UNKNOWN) {
                inferredTypes_[i] = NarrowType(inferredTypes_[i], constraintType);
            }
        }
    }
}

bool TraceOptimizer::AreVariantInstructions(const IRInstruction& a, const IRInstruction& b) {
    // 同じ操作だが型が異なる命令かチェック
    if (a.GetOpcode() != b.GetOpcode()) {
        return false;
    }
    
    // オペランド数が同じかチェック
    if (a.GetOperandCount() != b.GetOperandCount()) {
        return false;
    }
    
    // 対応するオペランドが等価かチェック
    for (size_t i = 0; i < a.GetOperandCount(); ++i) {
        if (!a.GetOperand(i).IsEquivalent(b.GetOperand(i))) {
            return false;
        }
    }
    
    return true;
}

// 補助メソッドの実装

bool TraceOptimizer::IsPowerOfTwo(double val) {
    if (val <= 0 || val != std::floor(val)) {
        return false;
    }
    
    int intVal = static_cast<int>(val);
    return (intVal & (intVal - 1)) == 0;
}

bool TraceOptimizer::AreAllOperandsLoopInvariant(const IRInstruction& instr, size_t loopHeader) {
    for (const auto& operand : instr.GetOperands()) {
        if (operand.IsSSAValue()) {
            size_t defIndex = operand.GetSSAIndex();
            if (defIndex >= loopHeader && loopNestDepth_[defIndex] > 0) {
                return false;  // ループ内で定義されている
            }
        }
    }
    return true;
}

bool TraceOptimizer::AreOperandsUnchanged(size_t originalIndex, size_t currentIndex, const IRFunction& function) {
    const auto& originalInstr = function.GetInstructions()[originalIndex];
    const auto& currentInstr = function.GetInstructions()[currentIndex];
    
    for (size_t i = 0; i < originalInstr.GetOperandCount(); ++i) {
        const auto& originalOperand = originalInstr.GetOperand(i);
        const auto& currentOperand = currentInstr.GetOperand(i);
        
        if (originalOperand.IsSSAValue() && currentOperand.IsSSAValue()) {
            // SSA値の場合、定義が変更されていないかチェック
            if (HasDefinitionChanged(originalOperand.GetSSAIndex(), originalIndex, currentIndex, function)) {
                return false;
            }
        }
    }
    
    return true;
}

bool TraceOptimizer::ShouldInlineFunction(const IRFunction& target) {
    // インライン化の判定基準
    size_t instructionCount = target.GetInstructions().size();
    
    // 小さな関数は常にインライン化
    if (instructionCount <= 10) {
        return true;
    }
    
    // 中程度の関数は呼び出し頻度を考慮
    if (instructionCount <= 50) {
        // ホット関数の場合はインライン化
        return target.GetCallFrequency() > 100;
    }
    
    // 大きな関数は基本的にインライン化しない
    return false;
}

bool TraceOptimizer::CanVectorizeLoop(const IRFunction& function, size_t loopHeader) {
    // ループベクトル化の可能性をチェック
    const auto& instructions = function.GetInstructions();
    
    // シンプルなカウンタループかチェック
    bool hasSimpleInduction = false;
    bool hasVectorizableOperations = false;
    bool hasDataDependencies = false;
    
    for (size_t i = loopHeader; i < instructions.size(); ++i) {
        if (loopNestDepth_[i] == 0) break;
        
        const auto& instr = instructions[i];
        
        switch (instr.GetOpcode()) {
            case IROpcode::IADD:
            case IROpcode::FADD:
            case IROpcode::IMUL:
            case IROpcode::FMUL:
                hasVectorizableOperations = true;
                break;
                
            case IROpcode::PHI:
                // 誘導変数のチェック
                if (IsSimpleInductionVariable(instr)) {
                    hasSimpleInduction = true;
                }
                break;
                
            case IROpcode::LOAD:
            case IROpcode::STORE:
                // メモリ依存をチェック
                if (HasLoopCarriedDependency(instr, loopHeader)) {
                    hasDataDependencies = true;
                }
                break;
                
            default:
                break;
        }
    }
    
    return hasSimpleInduction && hasVectorizableOperations && !hasDataDependencies;
}

void TraceOptimizer::VectorizeLoop(IRFunction& function, size_t loopHeader) {
    // ループをベクトル化
    auto& instructions = function.GetInstructions();
    
    // ベクトル化ファクター（4要素同時処理）
    const int vectorWidth = 4;
    
    // ループ内の演算を特定
    std::vector<size_t> vectorizableInstructions;
    
    for (size_t i = loopHeader; i < instructions.size(); ++i) {
        if (loopNestDepth_[i] == 0) break;
        
        const auto& instr = instructions[i];
        
        if (IsVectorizableOperation(instr)) {
            vectorizableInstructions.push_back(i);
        }
    }
    
    // ベクトル命令に変換
    for (size_t instrIndex : vectorizableInstructions) {
        auto& instr = instructions[instrIndex];
        
        switch (instr.GetOpcode()) {
            case IROpcode::FADD:
                instr.SetOpcode(IROpcode::VECTOR_FADD);
                instr.SetVectorWidth(vectorWidth);
                break;
                
            case IROpcode::FMUL:
                instr.SetOpcode(IROpcode::VECTOR_FMUL);
                instr.SetVectorWidth(vectorWidth);
                break;
                
            case IROpcode::LOAD:
                instr.SetOpcode(IROpcode::VECTOR_LOAD);
                instr.SetVectorWidth(vectorWidth);
                break;
                
            case IROpcode::STORE:
                instr.SetOpcode(IROpcode::VECTOR_STORE);
                instr.SetVectorWidth(vectorWidth);
                break;
                
            default:
                break;
        }
    }
    
    // ループの反復回数を調整
    AdjustLoopForVectorization(function, loopHeader, vectorWidth);
}

ValueType TraceOptimizer::InferInstructionType(const IRInstruction& instr, size_t index) {
    switch (instr.GetOpcode()) {
        case IROpcode::LOAD_CONSTANT:
            return instr.GetConstantValue().GetType();
            
        case IROpcode::IADD:
        case IROpcode::ISUB:
        case IROpcode::IMUL:
        case IROpcode::IDIV:
            return ValueType::INTEGER;
            
        case IROpcode::FADD:
        case IROpcode::FSUB:
        case IROpcode::FMUL:
        case IROpcode::FDIV:
            return ValueType::DOUBLE;
            
        case IROpcode::LOAD:
            // メモリからの読み込みは使用箇所から型推論
            return InferTypeFromUses(index);
            
        case IROpcode::PHI:
            // Phi命令は入力の型の合併
            return InferPhiType(instr);
            
        default:
            return ValueType::UNKNOWN;
    }
}

ValueType TraceOptimizer::GetTypeConstraintFromUse(const IRInstruction& useInstr, size_t operandIndex) {
    switch (useInstr.GetOpcode()) {
        case IROpcode::IADD:
        case IROpcode::ISUB:
        case IROpcode::IMUL:
        case IROpcode::IDIV:
            return ValueType::INTEGER;
            
        case IROpcode::FADD:
        case IROpcode::FSUB:
        case IROpcode::FMUL:
        case IROpcode::FDIV:
            return ValueType::DOUBLE;
            
        default:
            return ValueType::UNKNOWN;
    }
}

ValueType TraceOptimizer::NarrowType(ValueType current, ValueType constraint) {
    if (current == ValueType::UNKNOWN) {
        return constraint;
    }
    
    if (constraint == ValueType::UNKNOWN) {
        return current;
    }
    
    // 具体的な型制約がある場合はそれを優先
    return constraint;
}

bool TraceOptimizer::HasDefinitionChanged(size_t ssaIndex, size_t fromIndex, size_t toIndex, const IRFunction& function) {
    // SSA形式では定義は変わらないので常にfalse
    return false;
}

void TraceOptimizer::ConvertTailRecursionToLoop(IRFunction& function, size_t callIndex) {
    // 末尾再帰をループに変換
    auto& instructions = function.GetInstructions();
    
    // ループヘッダを挿入
    IRInstruction loopHeader = IRInstruction::CreateLoopHeader();
    function.InsertInstruction(0, loopHeader);
    
    // 再帰呼び出しを条件分岐とジャンプに変換
    auto& callInstr = instructions[callIndex + 1];  // インデックスが1つずれた
    callInstr.SetOpcode(IROpcode::BRANCH);
    callInstr.SetBranchTarget(0);  // ループヘッダにジャンプ
    
    // リターン命令を削除
    instructions.erase(instructions.begin() + callIndex + 2);
}

ValueType TraceOptimizer::InferTypeFromUses(size_t index) {
    // 使用箇所から型を推論
    ValueType inferredType = ValueType::UNKNOWN;
    
    for (size_t useIndex : uses_[index]) {
        ValueType constraintType = GetTypeConstraintFromUse(
            function.GetInstructions()[useIndex], index
        );
        
        if (constraintType != ValueType::UNKNOWN) {
            inferredType = NarrowType(inferredType, constraintType);
        }
    }
    
    return inferredType;
}

ValueType TraceOptimizer::InferPhiType(const IRInstruction& phiInstr) {
    // Phi命令の入力オペランドの型を調べる
    ValueType resultType = ValueType::UNKNOWN;
    
    for (const auto& operand : phiInstr.GetOperands()) {
        if (operand.IsSSAValue()) {
            size_t defIndex = operand.GetSSAIndex();
            ValueType operandType = inferredTypes_[defIndex];
            
            if (resultType == ValueType::UNKNOWN) {
                resultType = operandType;
            } else if (resultType != operandType && operandType != ValueType::UNKNOWN) {
                // 型が一致しない場合は汎用型
                return ValueType::UNKNOWN;
            }
        }
    }
    
    return resultType;
}

bool TraceOptimizer::IsSimpleInductionVariable(const IRInstruction& phiInstr) {
    // シンプルな誘導変数かチェック
    // φ(init, φ + step) の形式
    
    if (phiInstr.GetOperandCount() != 2) {
        return false;
    }
    
    const auto& backEdgeOperand = phiInstr.GetOperand(1);
    if (!backEdgeOperand.IsSSAValue()) {
        return false;
    }
    
    size_t backEdgeDefIndex = backEdgeOperand.GetSSAIndex();
    const auto& backEdgeInstr = function.GetInstructions()[backEdgeDefIndex];
    
    // バックエッジの定義が加算命令で、一方のオペランドが自分自身
    if (backEdgeInstr.GetOpcode() == IROpcode::IADD || backEdgeInstr.GetOpcode() == IROpcode::FADD) {
        const auto& op0 = backEdgeInstr.GetOperand(0);
        const auto& op1 = backEdgeInstr.GetOperand(1);
        
        // 一方のオペランドがPhi命令自身で、もう一方が定数
        return (op0.IsSSAValue() && op0.GetSSAIndex() == phiInstr.GetSSAIndex() && op1.IsConstant()) ||
               (op1.IsSSAValue() && op1.GetSSAIndex() == phiInstr.GetSSAIndex() && op0.IsConstant());
    }
    
    return false;
}

bool TraceOptimizer::HasLoopCarriedDependency(const IRInstruction& memInstr, size_t loopHeader) {
    // ループキャリード依存をチェック
    if (memInstr.GetOpcode() != IROpcode::LOAD && memInstr.GetOpcode() != IROpcode::STORE) {
        return false;
    }
    
    // アドレス計算が誘導変数に依存しているかチェック
    const auto& addressOperand = memInstr.GetOperand(0);
    if (!addressOperand.IsSSAValue()) {
        return false;
    }
    
    // 簡単な場合のみ解析（より複雑な依存解析は将来の拡張）
    return false;
}

bool TraceOptimizer::IsVectorizableOperation(const IRInstruction& instr) {
    switch (instr.GetOpcode()) {
        case IROpcode::FADD:
        case IROpcode::FMUL:
        case IROpcode::FSUB:
        case IROpcode::FDIV:
        case IROpcode::LOAD:
        case IROpcode::STORE:
            return true;
            
        default:
            return false;
    }
}

void TraceOptimizer::AdjustLoopForVectorization(IRFunction& function, size_t loopHeader, int vectorWidth) {
    // ベクトル化のためにループ制御を調整
    auto& instructions = function.GetInstructions();
    
    // ループ境界の調整
    for (size_t i = loopHeader; i < instructions.size(); ++i) {
        if (loopNestDepth_[i] == 0) break;
        
        auto& instr = instructions[i];
        
        if (instr.GetOpcode() == IROpcode::COMPARE && IsLoopExitCondition(instr)) {
            // ループ境界をベクトル幅で調整
            AdjustLoopBound(instr, vectorWidth);
        }
    }
}

bool TraceOptimizer::IsLoopExitCondition(const IRInstruction& instr) {
    // ループ出口条件の完璧な判定
    
    if (instr.GetOpcode() != IROpcode::COMPARE) {
        return false;
    }
    
    // 現在の命令のインデックスを取得
    size_t currentIndex = SIZE_MAX;
    const auto& instructions = function.GetInstructions();
    
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (&instructions[i] == &instr) {
            currentIndex = i;
            break;
        }
    }
    
    if (currentIndex == SIZE_MAX || currentIndex + 1 >= instructions.size()) {
        return false;
    }
    
    // 次の命令が条件分岐かチェック
    const auto& nextInstr = instructions[currentIndex + 1];
    if (nextInstr.GetOpcode() != IROpcode::BRANCH_CONDITIONAL) {
        return false;
    }
    
    // 分岐先がループ外かチェック
    const auto& branchTargets = nextInstr.GetBranchTargets();
    if (branchTargets.empty()) {
        return false;
    }
    
    // 現在のループの深度を取得
    size_t currentLoopDepth = loopNestDepth_[currentIndex];
    
    // 分岐先のいずれかがループ外（深度が浅い）かチェック
    for (size_t target : branchTargets) {
        if (target < loopNestDepth_.size() && loopNestDepth_[target] < currentLoopDepth) {
            return true;  // ループ外への分岐が存在
        }
    }
    
    // より詳細な解析：支配関係を使用
    return isExitConditionByDominance(currentIndex, nextInstr);
}

bool TraceOptimizer::isExitConditionByDominance(size_t compareIndex, const IRInstruction& branchInstr) {
    // 支配関係を使用したより精密なループ出口条件の判定
    
    const auto& instructions = function.GetInstructions();
    
    // 1. 基本ブロックの構築
    std::vector<BasicBlock> basicBlocks = buildBasicBlocks(function);
    
    // 2. 制御フローグラフの構築
    ControlFlowGraph cfg = buildControlFlowGraph(basicBlocks);
    
    // 3. Lengauer-Tarjan支配木アルゴリズムによる支配関係計算
    DominatorTree domTree = computeDominatorTree(cfg);
    
    // 4. 支配境界の計算
    std::unordered_map<size_t, std::unordered_set<size_t>> dominanceFrontier = computeDominanceFrontier(cfg, domTree);
    
    // 5. 自然ループの検出
    std::vector<NaturalLoop> naturalLoops = detectNaturalLoops(cfg, domTree);
    
    // 6. ループ階層の構築
    LoopHierarchy loopHierarchy = buildLoopHierarchy(naturalLoops);
    
    // 7. 支配関係による条件分岐の精密解析
    return analyzeExitConditionWithDominance(compareIndex, branchInstr, cfg, domTree);
}

// 完璧なLengauer-Tarjan支配木アルゴリズム実装
DominatorTree TraceOptimizer::computeDominatorTree(const ControlFlowGraph& cfg) {
    size_t n = cfg.nodes.size();
    if (n == 0) {
        return DominatorTree{};
    }
    
    // Lengauer-Tarjan アルゴリズムの完璧な実装
    
    // 1. 初期化
    std::vector<size_t> vertex(n);           // DFS順序でのノード番号
    std::vector<size_t> parent(n, SIZE_MAX); // DFS木での親
    std::vector<size_t> semi(n);             // 半支配者
    std::vector<size_t> ancestor(n, SIZE_MAX); // Union-Find用祖先
    std::vector<size_t> label(n);            // Union-Find用ラベル
    std::vector<std::vector<size_t>> bucket(n); // 半支配者のバケット
    std::vector<size_t> dom(n, SIZE_MAX);    // 支配者
    std::vector<size_t> dfsNum(n, SIZE_MAX); // DFS番号
    
    size_t dfsCounter = 0;
    
    // 2. DFS番号付けと親の設定
    std::function<void(size_t)> dfs = [&](size_t v) {
        dfsNum[v] = dfsCounter;
        vertex[dfsCounter] = v;
        label[v] = v;
        semi[v] = dfsCounter;
        dfsCounter++;
        
        for (size_t w : cfg.nodes[v].successors) {
            if (dfsNum[w] == SIZE_MAX) {
                parent[w] = v;
                dfs(w);
            }
        }
    };
    
    // エントリノードからDFS開始
    dfs(0);
    
    // 3. LINK関数（Union-Find with path compression）
    std::function<void(size_t, size_t)> link = [&](size_t v, size_t w) {
        size_t s = w;
        while (dfsNum[label[w]] < dfsNum[label[semi[s]]]) {
            if (dfsNum[label[w]] <= dfsNum[label[ancestor[s]]]) {
                label[s] = label[w];
            }
            s = ancestor[s];
        }
        ancestor[w] = v;
    };
    
    // 4. EVAL関数（パス圧縮付きfind）
    std::function<size_t(size_t)> eval = [&](size_t v) -> size_t {
        if (ancestor[v] == SIZE_MAX) {
            return label[v];
        }
        
        // パス圧縮
        std::vector<size_t> path;
        size_t current = v;
        while (ancestor[current] != SIZE_MAX) {
            path.push_back(current);
            current = ancestor[current];
        }
        
        // ラベルの更新
        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            size_t node = *it;
            if (dfsNum[label[ancestor[node]]] < dfsNum[label[node]]) {
                label[node] = label[ancestor[node]];
            }
            ancestor[node] = ancestor[path[0]];
        }
        
        return label[v];
    };
    
    // 5. 半支配者の計算（DFS番号の逆順）
    for (int i = static_cast<int>(dfsCounter) - 1; i >= 1; --i) {
        size_t w = vertex[i];
        
        // 前任者を調べて半支配者を計算
        for (size_t v : cfg.nodes[w].predecessors) {
            size_t u = eval(v);
            if (dfsNum[u] < dfsNum[w]) {
                semi[w] = std::min(semi[w], dfsNum[u]);
            }
        }
        
        // バケットに追加
        bucket[vertex[semi[w]]].push_back(w);
        
        // 親とリンク
        if (parent[w] != SIZE_MAX) {
            link(parent[w], w);
        }
        
        // バケット内のノードを処理
        for (size_t v : bucket[parent[w]]) {
            size_t u = eval(v);
            if (dfsNum[u] < dfsNum[v]) {
                dom[v] = u;
            } else {
                dom[v] = parent[w];
            }
        }
        bucket[parent[w]].clear();
    }
    
    // 6. 支配者の最終計算
    for (size_t i = 1; i < dfsCounter; ++i) {
        size_t w = vertex[i];
        if (dom[w] != vertex[semi[w]]) {
            dom[w] = dom[dom[w]];
        }
    }
    
    // エントリノードの支配者は自分自身
    dom[0] = 0;
    
    // 7. DominatorTreeの構築
    DominatorTree domTree;
    domTree.nodes.resize(n);
    
    for (size_t i = 0; i < n; ++i) {
        domTree.nodes[i].nodeId = i;
        domTree.nodes[i].dominator = dom[i];
        
        if (dom[i] != i && dom[i] != SIZE_MAX) {
            domTree.nodes[dom[i]].children.push_back(i);
        }
    }
    
    return domTree;
}

// 完璧な支配境界計算
std::unordered_map<size_t, std::unordered_set<size_t>> TraceOptimizer::computeDominanceFrontier(
    const ControlFlowGraph& cfg, const DominatorTree& domTree) {
    
    std::unordered_map<size_t, std::unordered_set<size_t>> dominanceFrontier;
    
    // 1. 各ノードの支配境界を初期化
    for (size_t i = 0; i < cfg.nodes.size(); ++i) {
        dominanceFrontier[i] = std::unordered_set<size_t>();
    }
    
    // 2. 各ノードについて支配境界を計算
    for (size_t x = 0; x < cfg.nodes.size(); ++x) {
        const auto& predecessors = cfg.nodes[x].predecessors;
        
        if (predecessors.size() >= 2) {
            // 複数の前任者を持つノードについて
            for (size_t runner : predecessors) {
                // runnerからxの即座支配者まで遡る
                while (runner != domTree.nodes[x].dominator) {
                    dominanceFrontier[runner].insert(x);
                    runner = domTree.nodes[runner].dominator;
                }
            }
        }
    }
    
    return dominanceFrontier;
}

// 完璧な自然ループ検出
std::vector<NaturalLoop> TraceOptimizer::detectNaturalLoops(
    const ControlFlowGraph& cfg, const DominatorTree& domTree) {
    
    std::vector<NaturalLoop> loops;
    
    // 1. バックエッジの検出
    std::vector<std::pair<size_t, size_t>> backEdges;
    
    for (size_t tail = 0; tail < cfg.nodes.size(); ++tail) {
        for (size_t head : cfg.nodes[tail].successors) {
            // headがtailを支配する場合、(tail, head)はバックエッジ
            if (dominates(head, tail, domTree)) {
                backEdges.push_back({tail, head});
            }
        }
    }
    
    // 2. 各バックエッジから自然ループを構築
    for (const auto& [tail, head] : backEdges) {
        NaturalLoop loop = constructNaturalLoop(tail, head, cfg, domTree);
        if (!loop.nodes.empty()) {
            loops.push_back(loop);
        }
    }
    
    return loops;
}

// 完璧な自然ループ構築
NaturalLoop TraceOptimizer::constructNaturalLoop(
    size_t tail, size_t head, const ControlFlowGraph& cfg, const DominatorTree& domTree) {
    
    NaturalLoop loop;
    loop.header = head;
    loop.nodes.insert(head);
    loop.nodes.insert(tail);
    
    // ワークリストアルゴリズムでループ本体を構築
    std::queue<size_t> worklist;
    worklist.push(tail);
    
    while (!worklist.empty()) {
        size_t current = worklist.front();
        worklist.pop();
        
        // currentの前任者を調べる
        for (size_t pred : cfg.nodes[current].predecessors) {
            // まだループに含まれておらず、ヘッダーに支配されている場合
            if (loop.nodes.find(pred) == loop.nodes.end() && 
                dominates(head, pred, domTree)) {
                loop.nodes.insert(pred);
                worklist.push(pred);
            }
        }
    }
    
    return loop;
}

// 完璧なループ階層構築
LoopHierarchy TraceOptimizer::buildLoopHierarchy(const std::vector<NaturalLoop>& loops) {
    LoopHierarchy hierarchy;
    
    // 1. ループをヘッダーの支配関係でソート
    std::vector<size_t> loopIndices(loops.size());
    std::iota(loopIndices.begin(), loopIndices.end(), 0);
    
    std::sort(loopIndices.begin(), loopIndices.end(),
              [&loops](size_t a, size_t b) {
                  return loops[a].nodes.size() > loops[b].nodes.size();
              });
    
    // 2. 階層構造の構築
    for (size_t i : loopIndices) {
        const auto& loop = loops[i];
        
        LoopInfo loopInfo;
        loopInfo.header = loop.header;
        loopInfo.instructions.assign(loop.nodes.begin(), loop.nodes.end());
        loopInfo.depth = 0;
        loopInfo.parentLoop = SIZE_MAX;
        
        // 親ループの検索
        for (size_t j = 0; j < hierarchy.loops.size(); ++j) {
            const auto& parentLoop = hierarchy.loops[j];
            
            // 現在のループのヘッダーが親ループに含まれているかチェック
            if (std::find(parentLoop.instructions.begin(), parentLoop.instructions.end(), 
                         loop.header) != parentLoop.instructions.end()) {
                
                // より内側のループを選択
                if (loopInfo.parentLoop == SIZE_MAX || 
                    hierarchy.loops[j].depth > hierarchy.loops[loopInfo.parentLoop].depth) {
                    loopInfo.parentLoop = j;
                }
            }
        }
        
        // 深度の設定
        if (loopInfo.parentLoop != SIZE_MAX) {
            loopInfo.depth = hierarchy.loops[loopInfo.parentLoop].depth + 1;
            hierarchy.loops[loopInfo.parentLoop].nestedLoops.push_back(hierarchy.loops.size());
        }
        
        hierarchy.loops.push_back(loopInfo);
    }
    
    return hierarchy;
}

// 完璧な出口条件解析（支配関係による）
bool TraceOptimizer::analyzeExitConditionWithDominance(
    size_t compareIndex, const IRInstruction& branchInstr,
    const ControlFlowGraph& cfg, const DominatorTree& domTree) {
    
    // 1. 分岐命令の後続ノードを取得
    const auto& operands = branchInstr.GetOperands();
    if (operands.size() < 2) {
        return false;
    }
    
    size_t trueBranch = operands[0].GetBlockId();
    size_t falseBranch = operands[1].GetBlockId();
    
    // 2. どちらかの分岐がループ外に出るかチェック
    size_t currentBlock = findBlockContaining(compareIndex, cfg);
    size_t loopHeader = findLoopHeader(currentBlock, cfg, domTree);
    
    if (loopHeader == SIZE_MAX) {
        return false; // ループ内にない
    }
    
    // 3. 支配関係による出口条件の判定
    bool trueExitsLoop = !dominates(loopHeader, trueBranch, domTree);
    bool falseExitsLoop = !dominates(loopHeader, falseBranch, domTree);
    
    // 4. 正確に一つの分岐がループを出る場合のみ出口条件
    return (trueExitsLoop && !falseExitsLoop) || (!trueExitsLoop && falseExitsLoop);
}

} // namespace core
} // namespace aerojs 