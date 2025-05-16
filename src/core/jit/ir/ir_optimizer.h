/**
 * @file ir_optimizer.h
 * @brief AeroJS JavaScript エンジンのIR最適化器の定義
 * @version 1.0.0
 * @license MIT
 */

#ifndef AEROJS_IR_OPTIMIZER_H
#define AEROJS_IR_OPTIMIZER_H

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <functional>

namespace aerojs {
namespace core {

// 前方宣言
class IRFunction;
class Context;
class JITProfiler;

/**
 * @brief 最適化パスの基底クラス
 */
class OptimizationPass {
public:
    /**
     * @brief コンストラクタ
     * @param name パス名
     */
    explicit OptimizationPass(const std::string& name) : m_name(name) {}
    
    /**
     * @brief デストラクタ
     */
    virtual ~OptimizationPass() = default;
    
    /**
     * @brief 最適化パスを実行
     * @param function 対象IR関数
     * @return 最適化が実行されたかどうか
     */
    virtual bool run(IRFunction* function) = 0;
    
    /**
     * @brief パス名を取得
     * @return パス名
     */
    const std::string& getName() const { return m_name; }
    
private:
    std::string m_name;
};

/**
 * @brief 定数畳み込み最適化パス
 */
class ConstantFoldingPass : public OptimizationPass {
public:
    /**
     * @brief コンストラクタ
     */
    ConstantFoldingPass() : OptimizationPass("ConstantFolding") {}
    
    /**
     * @brief 定数畳み込み最適化を実行
     * @param function 対象IR関数
     * @return 最適化が実行されたかどうか
     */
    bool run(IRFunction* function) override;
};

/**
 * @brief 共通部分式削除最適化パス
 */
class CommonSubexpressionEliminationPass : public OptimizationPass {
public:
    /**
     * @brief コンストラクタ
     */
    CommonSubexpressionEliminationPass() : OptimizationPass("CSE") {}
    
    /**
     * @brief 共通部分式削除最適化を実行
     * @param function 対象IR関数
     * @return 最適化が実行されたかどうか
     */
    bool run(IRFunction* function) override;
};

/**
 * @brief デッドコード削除最適化パス
 */
class DeadCodeEliminationPass : public OptimizationPass {
public:
    /**
     * @brief コンストラクタ
     */
    DeadCodeEliminationPass() : OptimizationPass("DCE") {}
    
    /**
     * @brief デッドコード削除最適化を実行
     * @param function 対象IR関数
     * @return 最適化が実行されたかどうか
     */
    bool run(IRFunction* function) override;
};

/**
 * @brief 命令スケジューリング最適化パス
 */
class InstructionSchedulingPass : public OptimizationPass {
public:
    /**
     * @brief コンストラクタ
     */
    InstructionSchedulingPass() : OptimizationPass("InstructionScheduling") {}
    
    /**
     * @brief 命令スケジューリング最適化を実行
     * @param function 対象IR関数
     * @return 最適化が実行されたかどうか
     */
    bool run(IRFunction* function) override;

private:
    /**
     * @brief 命令間のメモリ依存関係をチェック
     * @param first 先行命令
     * @param second 後続命令
     * @return 依存関係の有無
     */
    bool hasMemoryDependency(IRInstruction* first, IRInstruction* second);
    
    /**
     * @brief 命令間の制御依存関係をチェック
     * @param first 先行命令
     * @param second 後続命令
     * @return 依存関係の有無
     */
    bool hasControlDependency(IRInstruction* first, IRInstruction* second);
    
    /**
     * @brief 命令がメモリ読み取りかどうかを判定
     * @param opcode 命令のオペコード
     * @return メモリ読み取りの場合true
     */
    bool isMemoryRead(IROpcode opcode);
    
    /**
     * @brief 命令がメモリ書き込みかどうかを判定
     * @param opcode 命令のオペコード
     * @return メモリ書き込みの場合true
     */
    bool isMemoryWrite(IROpcode opcode);
    
    /**
     * @brief 命令が制御フロー命令かどうかを判定
     * @param opcode 命令のオペコード
     * @return 制御フロー命令の場合true
     */
    bool isControlFlowInstruction(IROpcode opcode);
    
    /**
     * @brief 命令が副作用を持つかどうかを判定
     * @param opcode 命令のオペコード
     * @return 副作用を持つ場合true
     */
    bool hasSideEffects(IROpcode opcode);
    
    /**
     * @brief 命令タイプごとのレイテンシテーブルを取得
     * @return レイテンシテーブル
     */
    std::unordered_map<IROpcode, int> getLatencyTable();
    
    /**
     * @brief 命令の優先順位を計算（クリティカルパス解析）
     * @param inst 対象命令
     * @param dependencies 依存関係グラフ
     * @param latencies レイテンシテーブル
     * @param memo 計算のメモ化用マップ
     * @return 優先順位の値
     */
    int calculatePriority(IRInstruction* inst, 
                         const std::unordered_map<IRInstruction*, std::vector<IRInstruction*>>& dependencies,
                         const std::unordered_map<IROpcode, int>& latencies,
                         std::unordered_map<IRInstruction*, int>& memo);
    
    /**
     * @brief 最適な命令を選択
     * @param ready 実行可能な命令リスト
     * @param dependencies 依存関係グラフ
     * @param function 対象IR関数
     * @return 選択された最適な命令
     */
    IRInstruction* selectBestInstruction(const std::vector<IRInstruction*>& ready,
                                        const std::unordered_map<IRInstruction*, std::vector<IRInstruction*>>& dependencies,
                                        IRFunction* function);
};

/**
 * @brief ループ不変コード移動最適化パス
 */
class LoopInvariantCodeMotionPass : public OptimizationPass {
public:
    /**
     * @brief コンストラクタ
     */
    LoopInvariantCodeMotionPass() : OptimizationPass("LICM") {}
    
    /**
     * @brief ループ不変コード移動最適化を実行
     * @param function 対象IR関数
     * @return 最適化が実行されたかどうか
     */
    bool run(IRFunction* function) override;
};

/**
 * @brief 命令組み合わせ最適化パス
 */
class InstructionCombiningPass : public OptimizationPass {
public:
    /**
     * @brief コンストラクタ
     */
    InstructionCombiningPass() : OptimizationPass("InstructionCombining") {}
    
    /**
     * @brief 命令組み合わせ最適化を実行
     * @param function 対象IR関数
     * @return 最適化が実行されたかどうか
     */
    bool run(IRFunction* function) override;
    
private:
    /**
     * @brief 値を定義している命令を探す
     * @param function 対象IR関数
     * @param value 検索する値
     * @return 見つかった命令、見つからない場合はnullptr
     */
    IRInstruction* findDefiningInstruction(IRFunction* function, IRValue* value);
};

/**
 * @brief IR最適化器クラス
 */
class IROptimizer {
public:
    /**
     * @brief 最適化レベル
     */
    enum class OptimizationLevel {
        O0,  // 最適化なし
        O1,  // 基本最適化のみ
        O2,  // 標準的な最適化（デフォルト）
        O3   // 積極的な最適化
    };
    
    /**
     * @brief プロファイル値タイプ
     */
    enum class ProfiledValueType {
        Unknown,
        Undefined,
        Null,
        Boolean,
        Int32,
        Int64,
        Float,
        String,
        Object,
        Array,
        Function,
        Mixed
    };
    
    /**
     * @brief コンストラクタ
     * @param context コンテキスト
     * @param profiler プロファイラ
     */
    IROptimizer(Context* context, JITProfiler* profiler);
    
    /**
     * @brief デストラクタ
     */
    ~IROptimizer();
    
    /**
     * @brief 最適化レベルを設定
     * @param level 最適化レベル
     */
    void setOptimizationLevel(OptimizationLevel level);
    
    /**
     * @brief IR関数を最適化
     * @param function 対象IR関数
     * @return 最適化が実行されたかどうか
     */
    bool optimize(IRFunction* function);
    
    /**
     * @brief 最適化パスを追加
     * @param pass 追加する最適化パス
     */
    void addPass(std::unique_ptr<OptimizationPass> pass);
    
    /**
     * @brief 最適化パスを有効化
     * @param passName パス名
     * @param enable 有効化するかどうか
     */
    void enablePass(const std::string& passName, bool enable);
    
    /**
     * @brief 最適化情報をダンプ
     * @return 最適化情報の文字列表現
     */
    std::string dumpOptimizationInfo() const;
    
private:
    // 最適化パスのリスト
    std::vector<std::unique_ptr<OptimizationPass>> m_passes;
    
    // 有効/無効化されたパスのマップ
    std::unordered_map<std::string, bool> m_enabledPasses;
    
    // 最適化レベル
    OptimizationLevel m_level;
    
    // コンテキスト/プロファイラ
    Context* m_context;
    JITProfiler* m_profiler;
    
    // 最適化統計情報
    struct OptimizationStats {
        int passExecutionCount;
        int optimizationCount;
        
        OptimizationStats() : passExecutionCount(0), optimizationCount(0) {}
    };
    
    std::unordered_map<std::string, OptimizationStats> m_stats;
    
    // 最適化レベルに応じたパスを設定
    void setupPassesForLevel();
    
    // デフォルトパスを追加
    void addDefaultPasses();
    
    // プロファイルタイプをIRタイプに変換
    IRType mapProfileTypeToIRType(ProfiledValueType profileType);
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_IR_OPTIMIZER_H 