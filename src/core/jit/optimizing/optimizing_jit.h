/**
 * @file optimizing_jit.h
 * @brief AeroJS JavaScript エンジンの最適化JITコンパイラの定義
 * @version 2.0.0
 * @license MIT
 */

#ifndef AEROJS_OPTIMIZING_JIT_H
#define AEROJS_OPTIMIZING_JIT_H

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <chrono>
#include <functional>

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class Function;
class IRBuilder;
class TypeSpecializer;
class IROptimizer;
class IRFunction;
class Deoptimizer;
class NativeCode;

/**
 * @brief 最適化レベル定義
 */
enum class OptimizationLevel {
    O0,  // 最適化なし
    O1,  // 基本最適化のみ
    O2,  // 標準的な最適化（デフォルト）
    O3   // 積極的な最適化
};

/**
 * @brief 型ガード情報
 */
struct TypeGuard {
    uint32_t codeOffset;     ///< コード内のオフセット
    uint32_t typeCheckKind;  ///< チェックする型の種類
    uint32_t bailoutOffset;  ///< 失敗時のバイトコードオフセット
    
    TypeGuard(uint32_t offset, uint32_t kind, uint32_t bailout)
        : codeOffset(offset), typeCheckKind(kind), bailoutOffset(bailout) {}
};

/**
 * @brief デオプティマイゼーション情報
 */
struct DeoptimizationInfo {
    enum class Reason {
        TypeGuardFailure,
        InlineGuardFailure,
        StackOverflow,
        DebuggerAttached,
        Custom
    };
    
    Reason reason;
    uint32_t codeOffset;
    uint32_t bailoutId;
    
    DeoptimizationInfo(Reason r, uint32_t offset, uint32_t id)
        : reason(r), codeOffset(offset), bailoutId(id) {}
};

/**
 * @brief コンパイルオプション
 */
struct CompileOptions {
    uint64_t functionId;
    Context* context;
    void* profileData;
    
    bool enableInlining;
    bool enableLoopOptimization;
    bool enableDeadCodeElimination;
    bool enableTypeSpecialization;
    bool enableDeoptimizationSupport;
    
    uint32_t maxInliningDepth;
    uint32_t inliningThreshold;
    
    CompileOptions()
        : functionId(0)
        , context(nullptr)
        , profileData(nullptr)
        , enableInlining(true)
        , enableLoopOptimization(true)
        , enableDeadCodeElimination(true)
        , enableTypeSpecialization(true)
        , enableDeoptimizationSupport(true)
        , maxInliningDepth(3)
        , inliningThreshold(50)
    {}
};

/**
 * @brief 最適化JITコンパイラ
 */
class OptimizingJIT {
public:
    /**
     * @brief 統計情報
     */
    struct Statistics {
        uint32_t totalCompilations;             ///< 総コンパイル数
        uint32_t totalInlinedFunctions;         ///< インライン化された関数の数
        uint32_t totalUnrolledLoops;           ///< 展開されたループの数
        uint32_t totalDeoptimizations;         ///< デオプティマイゼーションの回数
        uint32_t totalTypeGuardsGenerated;     ///< 生成された型ガードの数
        uint64_t totalCompiledBytecodeSizeBytes; ///< コンパイルされたバイトコードの合計サイズ
        uint64_t totalGeneratedCodeSizeBytes;   ///< 生成されたコードの合計サイズ
        uint32_t averageBytecodeSizeBytes;     ///< 平均バイトコードサイズ
        uint32_t averageGeneratedCodeSizeBytes; ///< 平均生成コードサイズ
        uint32_t averageCompilationTimeMs;     ///< 平均コンパイル時間
        
        Statistics()
            : totalCompilations(0)
            , totalInlinedFunctions(0)
            , totalUnrolledLoops(0)
            , totalDeoptimizations(0)
            , totalTypeGuardsGenerated(0)
            , totalCompiledBytecodeSizeBytes(0)
            , totalGeneratedCodeSizeBytes(0)
            , averageBytecodeSizeBytes(0)
            , averageGeneratedCodeSizeBytes(0)
            , averageCompilationTimeMs(0)
        {}
    };
    
    /**
     * @brief 最適化パスの関数型
     */
    using OptimizationPassFunc = std::function<bool(IRFunction*)>;
    
    /**
     * @brief 最適化パス定義
     */
    struct OptimizationPass {
        std::string name;
        OptimizationPassFunc function;
    };
    
    /**
     * @brief コンストラクタ
     * @param context 実行コンテキスト
     */
    explicit OptimizingJIT(Context* context);
    
    /**
     * @brief デストラクタ
     */
    ~OptimizingJIT();
    
    /**
     * @brief 最適化レベルを設定する
     * @param level 最適化レベル
     */
    void setOptimizationLevel(OptimizationLevel level);
    
    /**
     * @brief 関数をコンパイルする
     * @param function コンパイル対象の関数
     * @return 生成されたネイティブコード（失敗時はnullptr）
     */
    NativeCode* compile(Function* function);
    
    /**
     * @brief オプション付きでバイトコードをコンパイルする
     * @param bytecodes バイトコード
     * @param options コンパイルオプション
     * @param outCodeSize 出力: 生成されたコードのサイズ
     * @return 生成されたマシンコードへのポインタ（失敗時はnullptr）
     */
    std::unique_ptr<uint8_t[]> compileWithOptions(
        const std::vector<uint8_t>& bytecodes,
        const CompileOptions& options,
        size_t* outCodeSize);
    
    /**
     * @brief 関数が最適化コンパイル対象かどうかをチェックする
     * @param functionId 関数ID
     * @return コンパイル対象であればtrue
     */
    bool shouldCompileFunction(uint64_t functionId);
    
    /**
     * @brief インスタンスをリセットする
     */
    void reset();
    
    /**
     * @brief 統計情報を取得する
     * @return 統計情報の参照
     */
    const Statistics& getStatistics() const;
    
    /**
     * @brief 最後のエラーメッセージを取得する
     * @return エラーメッセージ
     */
    const std::string& getLastError() const;
    
    /**
     * @brief デオプティマイゼーションを処理する
     * @param functionId 関数ID
     * @param info デオプティマイゼーション情報
     * @return 成功すればtrue
     */
    bool handleDeoptimization(uint64_t functionId, const DeoptimizationInfo& info);
    
private:
    // インスタンス変数
    Context* m_context;
    OptimizationLevel m_optimizationLevel;
    void* m_profiler;  // 実際のプロファイラ型に置き換え
    std::unique_ptr<IRFunction> m_irFunction;
    std::unique_ptr<IRBuilder> m_irBuilder;
    std::unique_ptr<TypeSpecializer> m_typeSpecializer;
    std::unique_ptr<IROptimizer> m_irOptimizer;
    std::unique_ptr<Deoptimizer> m_deoptimizer;
    
    std::vector<OptimizationPass> m_optimizationPasses;
    std::unordered_map<std::string, bool> m_enabledPasses;
    std::vector<TypeGuard> m_typeGuards;
    
    // 統計情報
    uint64_t m_totalCompilationTimeMs;
    uint64_t m_irGenerationTimeMs;
    uint64_t m_optimizationTimeMs;
    uint64_t m_codeGenTimeMs;
    uint32_t m_inliningCount;
    uint32_t m_loopUnrollingCount;
    uint32_t m_deoptimizationCount;
    Statistics m_stats;
    
    // バイトコード関連
    uint64_t m_functionId;
    
    // エラーハンドリング
    std::string m_lastError;
    
    // 内部メソッド
    void initializeOptimizationPasses();
    void addOptimizationPass(const std::string& name, OptimizationPassFunc func);
    void enableOptimizationPass(const std::string& name, bool enable);
    bool isOptimizationPassEnabled(const std::string& name) const;
    void setError(const std::string& message);
    
    std::unique_ptr<IRFunction> optimizeIR(std::unique_ptr<IRFunction> irFunction, const CompileOptions& options);
    std::unique_ptr<uint8_t[]> generateMachineCode(std::unique_ptr<IRFunction> irFunction, const CompileOptions& options, size_t* outCodeSize);
    void configureOptionsForOptimizationLevel(CompileOptions& options);
    void updateCompilationStatistics(NativeCode* nativeCode, size_t bytecodeSize, size_t codeSize);
    void setupInlineCaches(NativeCode* nativeCode);
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_OPTIMIZING_JIT_H