#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include "../jit_compiler.h"
#include "../jit_profiler.h"
#include "../ir/ir.h"
#include "../ir/ir_builder.h"
#include "../ir/type_specializer.h"
#include "../../bytecode/bytecode.h"
#include <functional>

namespace aerojs::core {

/**
 * @brief Optimizing JIT 実装のスケルトン
 *
 * プロファイル情報を用いた最適化パスを適用する予定。
 */

// 最適化レベル
enum class OptimizationLevel {
    O0, // 最適化なし
    O1, // 基本的な最適化
    O2, // 積極的な最適化
    O3  // 超積極的な最適化
};

// 最適化パス情報
struct OptimizationPassInfo {
    std::string name;
    bool enabled;
    uint32_t executionTimeMs;
    uint32_t bytesReduced;
    uint32_t instructionsEliminated;
};

// 最適化タイプフィードバック
struct OptimizationTypeFeedback {
    uint32_t offset;
    TypeFeedbackRecord::TypeCategory expectedType;
    bool isValidated;
};

// 最適化JITコンパイラクラス
class OptimizingJIT : public JITCompiler {
public:
    /**
     * @brief 最適化コンパイルのためのオプション
     */
    struct CompileOptions {
        // プロファイリングデータ
        const FunctionProfile* profileData = nullptr;
        
        // 推測的最適化を有効にするかどうか
        bool enableSpeculation = true;
        
        // インライン化を有効にするかどうか
        bool enableInlining = true;
        
        // ループ最適化を有効にするかどうか
        bool enableLoopOptimization = true;
        
        // デッドコード除去を有効にするかどうか
        bool enableDeadCodeElimination = true;
        
        // 型特化を有効にするかどうか
        bool enableTypeSpecialization = true;
        
        // 逆最適化サポートを有効にするかどうか
        bool enableDeoptimizationSupport = true;
        
        // インライン化の最大深さ
        uint32_t maxInliningDepth = 3;
        
        // インライン化するための呼び出し回数の閾値
        uint32_t inliningCallCountThreshold = 10;
        
        // 関数サイズの上限（これより大きい関数はインライン化しない）
        uint32_t maxInlinableFunctionSize = 100;
    };
    
public:
    /**
     * @brief コンストラクタ
     */
    OptimizingJIT() noexcept;
    
    /**
     * @brief デストラクタ
     */
    ~OptimizingJIT() noexcept override;
    
    /**
     * @brief バイトコードを最適化してマシンコードにコンパイルする
     * 
     * @param bytecodes コンパイル対象のバイトコード
     * @param codeSize 出力された機械語コードのサイズ
     * @return 生成されたマシンコードへのポインタ
     */
    std::unique_ptr<uint8_t[]> Compile(const std::vector<uint8_t>& bytecodes, uint32_t& codeSize) override;
    
    /**
     * @brief 最適化オプションを指定してコンパイルを実行する
     * 
     * @param bytecodes コンパイル対象のバイトコード
     * @param options 最適化オプション
     * @param codeSize 出力された機械語コードのサイズ
     * @return 生成されたマシンコードへのポインタ
     */
    std::unique_ptr<uint8_t[]> CompileWithOptions(
        const std::vector<uint8_t>& bytecodes, 
        const CompileOptions& options,
        uint32_t& codeSize);
    
    /**
     * @brief コンパイラの内部状態をリセットする
     */
    void Reset() override;
    
    /**
     * @brief 関数IDを設定する
     * 
     * @param functionId 関数ID
     */
    void SetFunctionId(uint32_t functionId);
    
    /**
     * @brief 最適化レベルを設定する
     * 
     * @param level 最適化レベル (0-3)
     */
    void SetOptimizationLevel(uint32_t level);
    
    /**
     * @brief 逆最適化ハンドラーを設定する
     * 
     * @param handler 逆最適化時に呼び出されるハンドラー
     */
    void SetDeoptimizationHandler(std::function<void(uint32_t functionId, uint32_t bytecodeOffset)> handler);
    
    // 設定
    void SetProfiler(std::shared_ptr<JITProfiler> profiler);
    std::shared_ptr<JITProfiler> GetProfiler() const;
    
    void EnableOptimizationPass(const std::string& passName, bool enable);
    bool IsOptimizationPassEnabled(const std::string& passName) const;
    
    // 最適化パスの追加・取得
    void AddOptimizationPass(const std::string& passName);
    std::vector<OptimizationPassInfo> GetOptimizationPassInfo() const;
    
    // 型フィードバックとガード
    void AddTypeGuard(uint32_t bytecodeOffset, TypeFeedbackRecord::TypeCategory expectedType);
    const std::vector<OptimizationTypeFeedback>& GetTypeGuards() const;
    
    // 逆最適化処理
    bool HandleDeoptimization(uint32_t bytecodeOffset, const std::string& reason);
    
    // デバッグ＆診断
    void DumpGeneratedIR(std::ostream& stream) const;
    void DumpOptimizedIR(std::ostream& stream) const;
    void DumpAssembly(std::ostream& stream) const;
    
    // 統計情報
    uint32_t GetTotalCompilationTimeMs() const;
    uint32_t GetTotalOptimizationTimeMs() const;
    uint32_t GetTotalCodeGenTimeMs() const;
    uint32_t GetTotalInliningCount() const;
    uint32_t GetTotalLoopUnrollingCount() const;
    
private:
    /**
     * @brief プロファイルデータをもとにIRを最適化する
     * 
     * @param irFunction 最適化対象のIR関数
     * @param options 最適化オプション
     * @return 最適化されたIR関数
     */
    std::unique_ptr<IRFunction> OptimizeIR(
        std::unique_ptr<IRFunction> irFunction, 
        const CompileOptions& options);
    
    /**
     * @brief 型フィードバックに基づいて型特化を適用する
     * 
     * @param irFunction IR関数
     * @param profile 関数プロファイル
     */
    void ApplyTypeSpecialization(
        IRFunction* irFunction,
        const FunctionProfile* profile);
    
    /**
     * @brief ホットループに対して最適化を適用する
     * 
     * @param irFunction IR関数
     * @param profile 関数プロファイル
     */
    void OptimizeHotLoops(
        IRFunction* irFunction,
        const FunctionProfile* profile);
    
    /**
     * @brief ホット関数呼び出しに対してインライン化を適用する
     * 
     * @param irFunction IR関数
     * @param profile 関数プロファイル
     * @param options コンパイルオプション
     */
    void ApplyInlining(
        IRFunction* irFunction,
        const FunctionProfile* profile,
        const CompileOptions& options);
    
    /**
     * @brief 推測的実行に必要なガードを挿入する
     * 
     * @param irFunction IR関数
     * @param profile 関数プロファイル
     */
    void InsertSpeculationGuards(
        IRFunction* irFunction, 
        const FunctionProfile* profile);
    
    /**
     * @brief 最適化統計情報を記録する
     * 
     * @param irFunction 最適化前のIR関数
     * @param optimizedFunction 最適化後のIR関数
     * @param options 適用された最適化オプション
     */
    void RecordOptimizationStats(
        const IRFunction* irFunction,
        const IRFunction* optimizedFunction,
        const CompileOptions& options);

private:
    // IR構築用
    IRBuilder m_irBuilder;
    
    // IR最適化エンジン
    IROptimizer m_irOptimizer;
    
    // 現在処理中の関数ID
    uint32_t m_functionId;
    
    // 最適化レベル
    uint32_t m_optimizationLevel;
    
    // 逆最適化ハンドラー
    std::function<void(uint32_t, uint32_t)> m_deoptimizationHandler;
    
    // 最適化統計情報
    struct OptimizationStats {
        uint32_t totalCompilations;
        uint32_t inlinedFunctions;
        uint32_t specializedTypes;
        uint32_t optimizedLoops;
        uint32_t eliminatedDeadCode;
        uint32_t insertedGuards;
        uint64_t totalCompilationTimeNs;
        
        OptimizationStats() : 
            totalCompilations(0),
            inlinedFunctions(0),
            specializedTypes(0),
            optimizedLoops(0),
            eliminatedDeadCode(0),
            insertedGuards(0),
            totalCompilationTimeNs(0) {}
    };
    
    OptimizationStats m_stats;
    
    // メンバ変数
    std::shared_ptr<JITProfiler> m_profiler;
    std::unique_ptr<IRFunction> m_irFunction;
    std::unique_ptr<TypeSpecializer> m_typeSpecializer;
    
    std::vector<OptimizationPassInfo> m_optimizationPasses;
    std::unordered_map<std::string, bool> m_enabledPasses;
    std::vector<OptimizationTypeFeedback> m_typeGuards;
    
    // 統計情報
    uint32_t m_totalCompilationTimeMs;
    uint32_t m_irGenerationTimeMs;
    uint32_t m_optimizationTimeMs;
    uint32_t m_codeGenTimeMs;
    uint32_t m_inliningCount;
    uint32_t m_loopUnrollingCount;
    uint32_t m_deoptimizationCount;
    
    // 逆最適化データ
    struct DeoptimizationInfo {
        uint32_t bytecodeOffset;
        std::string reason;
        uint32_t count;
    };
    std::vector<DeoptimizationInfo> m_deoptimizationInfo;
};

} // namespace aerojs::core