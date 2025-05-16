/**
 * @file trace_optimizer.h
 * @brief メタトレーシングJITのトレース最適化フレームワーク
 * 
 * このファイルは、実行時にホットパスを検出して最適化するための
 * メタトレーシングJITの最適化エンジンを定義します。
 * 
 * @author AeroJS Team
 * @version 3.0.0
 * @copyright MIT License
 */

#ifndef AEROJS_TRACE_OPTIMIZER_H
#define AEROJS_TRACE_OPTIMIZER_H

#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include "../ir/ir_instruction.h"
#include "../../runtime/values/value.h"
#include "../../utils/bitset.h"

namespace aerojs {
namespace core {

class ExecutionTrace;
class IRBuilder;
class IRFunction;

// トレース最適化フェーズの種類
enum class OptimizationPhase {
    REDUNDANCY_ELIMINATION,    // 冗長命令の削除
    CONSTANT_FOLDING,          // 定数畳み込み
    DEAD_CODE_ELIMINATION,     // 不要コード削除
    TYPE_SPECIALIZATION,       // 型特殊化
    LOOP_INVARIANT_HOISTING,   // ループ不変式の移動
    COMMON_SUBEXPRESSION,      // 共通部分式の排除
    STRENGTH_REDUCTION,        // 強度削減
    INLINING,                  // インライン化
    ESCAPE_ANALYSIS,           // エスケープ解析
    TAIL_CALL_OPTIMIZATION,    // 末尾呼び出し最適化
    VECTORIZATION              // ベクトル化
};

// 最適化適用結果
struct OptimizationResult {
    bool changed = false;
    int eliminatedInstructions = 0;
    int specializedTypes = 0;
    int vectorizedLoops = 0;
    int inlinedFunctions = 0;
};

// トレース最適化クラス
class TraceOptimizer {
public:
    TraceOptimizer() = default;
    ~TraceOptimizer() = default;
    
    // 実行トレースを最適化
    std::unique_ptr<IRFunction> OptimizeTrace(const ExecutionTrace& trace);
    
    // 既存のIR関数を最適化
    OptimizationResult OptimizeIR(IRFunction& function);
    
    // 最適化フェーズの有効/無効を設定
    void EnableOptimization(OptimizationPhase phase, bool enable = true) {
        enabledPhases_[static_cast<int>(phase)] = enable;
    }
    
    // トレース中の型を専門化するハンドラの登録
    using TypeSpecializationHandler = std::function<IRInstruction(const IRInstruction&, ValueType)>;
    void RegisterTypeSpecialization(ValueType type, TypeSpecializationHandler handler) {
        typeHandlers_[type] = std::move(handler);
    }

private:
    // 冗長命令の削除
    OptimizationResult EliminateRedundancy(IRFunction& function);
    
    // 定数畳み込み
    OptimizationResult FoldConstants(IRFunction& function);
    
    // 不要コード削除
    OptimizationResult EliminateDeadCode(IRFunction& function);
    
    // 型特殊化
    OptimizationResult SpecializeTypes(IRFunction& function);
    
    // ループ不変式の移動
    OptimizationResult HoistLoopInvariants(IRFunction& function);
    
    // 共通部分式の排除
    OptimizationResult EliminateCommonSubexpressions(IRFunction& function);
    
    // 強度削減
    OptimizationResult ReduceStrength(IRFunction& function);
    
    // インライン化
    OptimizationResult InlineFunctions(IRFunction& function);
    
    // エスケープ解析
    OptimizationResult AnalyzeEscape(IRFunction& function);
    
    // 末尾呼び出し最適化
    OptimizationResult OptimizeTailCalls(IRFunction& function);
    
    // ベクトル化
    OptimizationResult Vectorize(IRFunction& function);
    
    // 命令のデータフロー解析
    void AnalyzeDataFlow(IRFunction& function);
    
    // 命令の依存関係グラフ構築
    void BuildDependencyGraph(IRFunction& function);
    
    // ループ検出
    void DetectLoops(IRFunction& function);
    
    // 型推論
    void InferTypes(IRFunction& function);
    
    // 型情報の伝播
    void PropagateTypes(IRFunction& function);
    
    // バリアント命令の検出（例：整数加算と浮動小数点加算）
    bool AreVariantInstructions(const IRInstruction& a, const IRInstruction& b);
    
    // 最適化フェーズの有効/無効フラグ
    std::array<bool, 11> enabledPhases_ = {true, true, true, true, true, true, true, true, true, true, true};
    
    // 型特殊化ハンドラのマップ
    std::unordered_map<ValueType, TypeSpecializationHandler> typeHandlers_;
    
    // 命令の到達可能性情報
    std::vector<bool> reachableInstructions_;
    
    // ループヘッダの集合
    std::unordered_set<size_t> loopHeaders_;
    
    // 各命令のループネスト深度
    std::vector<int> loopNestDepth_;
    
    // 制御フロー解析情報
    std::vector<std::vector<size_t>> predecessors_;
    std::vector<std::vector<size_t>> successors_;
    
    // データフロー解析情報
    std::vector<std::vector<size_t>> uses_;
    std::vector<std::vector<size_t>> defs_;
    
    // 命令の型情報
    std::vector<ValueType> inferredTypes_;
    
    // SSA形式の変数生存期間
    std::vector<std::pair<size_t, size_t>> liveRanges_;
    
    // メモリ依存解析情報
    std::vector<std::vector<size_t>> memoryDependencies_;
    
    // ホットパス情報
    std::vector<double> executionFrequencies_;
    
    // 仮想レジスタ割り当て情報
    std::vector<int> registerAssignments_;
    
    // 最適化適用回数の記録（無限ループ防止用）
    int optimizationPassCount_ = 0;
    static constexpr int MAX_OPTIMIZATION_PASSES = 20;
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_TRACE_OPTIMIZER_H 