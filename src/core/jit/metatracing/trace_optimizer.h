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

#ifndef AEROJS_CORE_JIT_METATRACING_TRACE_OPTIMIZER_H
#define AEROJS_CORE_JIT_METATRACING_TRACE_OPTIMIZER_H

#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <optional>
#include <bitset>
#include <array>

#include "../ir/ir_node.h"
#include "../ir/ir_builder.h"
#include "../../runtime/values/value.h"
#include "trace_recorder.h"

namespace aerojs {
namespace core {
namespace jit {
namespace metatracing {

// 前方宣言
class TracingJIT;

/**
 * @brief 最適化パスを表す列挙型
 */
enum class OptimizationPass {
    // 基本的な最適化
    ConstantFolding,        ///< 定数畳み込み
    DeadCodeElimination,    ///< 不要コード削除
    CommonSubexprElimination, ///< 共通部分式の削除
    InstructionCombining,   ///< 命令結合
    
    // 型特化の最適化
    TypeSpecialization,     ///< 型に基づく特殊化
    GuardElimination,       ///< 不要なガード命令の削除
    BoxingElimination,      ///< 不要なボクシング/アンボクシングの削除
    
    // メモリ関連の最適化
    EscapeAnalysis,         ///< エスケープ解析
    AllocationSinking,      ///< アロケーション沈下
    LoadElimination,        ///< 冗長なロード除去
    
    // ループ最適化
    LoopInvariantCodeMotion, ///< ループ不変コード移動
    LoopUnrolling,          ///< ループ展開
    VectorizationAnalysis,  ///< ベクトル化解析
    SIMDTransformation,     ///< SIMD変換
    
    // 高度な最適化
    InliningAnalysis,       ///< インライン化解析
    TailCallOptimization,   ///< 末尾呼び出し最適化
    PartialEvaluation,      ///< 部分評価
    SpeculativeDevirtualization, ///< 投機的な仮想関数の具体化
    
    // メタトレース特有の最適化
    TraceSegmentMerging,    ///< トレースセグメントの結合
    ContinuationSpecialization, ///< 継続特化
    HotPathDuplication,     ///< ホットパス複製
    ContextSpecialization,  ///< コンテキスト特化
    
    // バックエンド最適化
    RegisterAllocation,     ///< レジスタ割り当て
    InstructionScheduling,  ///< 命令スケジューリング
    PeepholeOptimization,   ///< 覗き穴最適化
    
    NumPasses               ///< パスの総数（常に列挙型の最後に置く）
};

/**
 * @brief 最適化パスの依存関係を表すビットセット
 */
using PassDependencySet = std::bitset<static_cast<size_t>(OptimizationPass::NumPasses)>;

/**
 * @brief 最適化パスの設定を表す構造体
 */
struct OptimizationPassConfig {
    OptimizationPass pass;
    bool enabled;
    int priority;
    PassDependencySet dependencies;
    std::function<void(ir::IRNode*)> optimizeFunc;
};

/**
 * @brief 最適化の適用レベルを表す列挙型
 */
enum class OptimizationLevel {
    O0,  ///< 最適化なし
    O1,  ///< 基本的な最適化
    O2,  ///< 中レベルの最適化
    O3,  ///< 高度な最適化
    Os,  ///< サイズ優先の最適化
    Oz   ///< 超サイズ優先の最適化
};

/**
 * @brief トレース最適化器クラス
 * 
 * メタトレーシングJITのトレース最適化を担当するクラスです。
 * 記録されたトレースを解析し、様々な最適化を適用します。
 */
class TraceOptimizer {
public:
    /**
     * @brief コンストラクタ
     * @param jit 親となるTracingJITへの参照
     */
    explicit TraceOptimizer(TracingJIT& jit);
    
    /**
     * @brief デストラクタ
     */
    ~TraceOptimizer();
    
    /**
     * @brief 最適化レベルを設定
     * @param level 適用する最適化レベル
     */
    void setOptimizationLevel(OptimizationLevel level);
    
    /**
     * @brief 特定の最適化パスの有効/無効を設定
     * @param pass 設定する最適化パス
     * @param enabled 有効にする場合はtrue
     */
    void setPassEnabled(OptimizationPass pass, bool enabled);
    
    /**
     * @brief トレースを最適化
     * @param trace 最適化対象のトレース
     * @return 最適化されたIRノードのルート
     * 
     * 記録されたトレースを解析し、設定された最適化パスを適用して
     * 最適化されたIRノードを返します。
     */
    std::unique_ptr<ir::IRNode> optimizeTrace(const TraceRecorder::Trace& trace);
    
    /**
     * @brief 最適化されたトレースのパフォーマンス予測を行う
     * @param optimizedIR 最適化されたIRノード
     * @return 予測される実行時間（ナノ秒単位）
     * 
     * 最適化されたIRノードの実行時間を予測します。
     * この情報はJITコンパイラのヒューリスティクスに使用されます。
     */
    uint64_t predictPerformance(const ir::IRNode* optimizedIR) const;
    
    /**
     * @brief トレースの型情報を推論
     * @param trace 対象のトレース
     * @return 各命令ノードに対する型情報のマップ
     * 
     * トレースの各命令に対する型情報を推論し、マップとして返します。
     */
    std::unordered_map<uint32_t, runtime::ValueType> inferTypes(const TraceRecorder::Trace& trace);
    
    /**
     * @brief トレースを検証
     * @param trace 検証対象のトレース
     * @return トレースが有効な場合はtrue
     * 
     * トレースの整合性を検証し、最適化に適しているかどうかを判断します。
     */
    bool validateTrace(const TraceRecorder::Trace& trace);
    
    /**
     * @brief 最適化統計を取得
     * @return 各最適化パスの適用回数と効果のマップ
     */
    std::unordered_map<OptimizationPass, std::pair<uint32_t, double>> getOptimizationStats() const;
    
private:
    // 親となるJITエンジンへの参照
    TracingJIT& m_jit;
    
    // 現在の最適化レベル
    OptimizationLevel m_optimizationLevel;
    
    // 最適化パスの設定マップ
    std::array<OptimizationPassConfig, static_cast<size_t>(OptimizationPass::NumPasses)> m_passConfigs;
    
    // 型情報キャッシュ
    std::unordered_map<uint32_t, runtime::ValueType> m_typeCache;
    
    // 最適化統計
    std::unordered_map<OptimizationPass, std::pair<uint32_t, double>> m_optimizationStats;
    
    /**
     * @brief トレースをIRに変換
     * @param trace 変換対象のトレース
     * @return IRノードのルート
     */
    std::unique_ptr<ir::IRNode> traceToIR(const TraceRecorder::Trace& trace);
    
    /**
     * @brief 最適化パスの順序を決定
     * @return 実行すべき最適化パスのリスト
     */
    std::vector<OptimizationPass> determinePassOrder() const;
    
    /**
     * @brief IRに特定の最適化パスを適用
     * @param ir 最適化対象のIRノード
     * @param pass 適用する最適化パス
     * @return 最適化が適用された場合はtrue
     */
    bool applyPass(ir::IRNode* ir, OptimizationPass pass);
    
    /**
     * @brief 最適化パスの初期化
     * 各最適化パスの設定を初期化します。
     */
    void initializePasses();
    
    /**
     * @brief 特定の最適化レベルに基づいてパスを設定
     * @param level 設定する最適化レベル
     */
    void configurePassesForLevel(OptimizationLevel level);
    
    // 各最適化パスの実装
    void applyConstantFolding(ir::IRNode* ir);
    void applyDeadCodeElimination(ir::IRNode* ir);
    void applyCommonSubexprElimination(ir::IRNode* ir);
    void applyInstructionCombining(ir::IRNode* ir);
    void applyTypeSpecialization(ir::IRNode* ir);
    void applyGuardElimination(ir::IRNode* ir);
    void applyBoxingElimination(ir::IRNode* ir);
    void applyEscapeAnalysis(ir::IRNode* ir);
    void applyAllocationSinking(ir::IRNode* ir);
    void applyLoadElimination(ir::IRNode* ir);
    void applyLoopInvariantCodeMotion(ir::IRNode* ir);
    void applyLoopUnrolling(ir::IRNode* ir);
    void applyVectorizationAnalysis(ir::IRNode* ir);
    void applySIMDTransformation(ir::IRNode* ir);
    void applyInliningAnalysis(ir::IRNode* ir);
    void applyTailCallOptimization(ir::IRNode* ir);
    void applyPartialEvaluation(ir::IRNode* ir);
    void applySpeculativeDevirtualization(ir::IRNode* ir);
    void applyTraceSegmentMerging(ir::IRNode* ir);
    void applyContinuationSpecialization(ir::IRNode* ir);
    void applyHotPathDuplication(ir::IRNode* ir);
    void applyContextSpecialization(ir::IRNode* ir);
    
    // その他の内部ヘルパーメソッド
    bool isEligibleForOptimization(const TraceRecorder::Trace& trace) const;
    bool hasExceptionHandlers(const TraceRecorder::Trace& trace) const;
    bool containsUnsafeOperations(const TraceRecorder::Trace& trace) const;
    double estimateOptimizationBenefit(const ir::IRNode* before, const ir::IRNode* after) const;
};

} // namespace metatracing
} // namespace jit
} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_METATRACING_TRACE_OPTIMIZER_H 