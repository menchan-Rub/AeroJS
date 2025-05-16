/**
 * @file profiler.h
 * @brief AeroJS JavaScript エンジンのJITプロファイラの定義
 * @version 1.0.0
 * @license MIT
 */

#ifndef AEROJS_JIT_PROFILER_H
#define AEROJS_JIT_PROFILER_H

#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <memory>

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class Value;
class Function;

/**
 * @brief 値の型プロファイル情報
 */
struct TypeProfile {
    enum class ValueType {
        Unknown,
        Undefined,
        Null,
        Boolean,
        Number,
        Integer,
        String,
        Symbol,
        Object,
        Array,
        Function,
        BigInt
    };
    
    ValueType dominantType;           ///< 支配的な型
    uint32_t sampleCount;            ///< サンプル数
    double stability;                ///< 型の安定性 (0.0-1.0)
    bool hasStableShape;            ///< オブジェクト形状の安定性
    bool hasStableTarget;           ///< 関数呼び出しターゲットの安定性
    
    TypeProfile()
        : dominantType(ValueType::Unknown)
        , sampleCount(0)
        , stability(0.0)
        , hasStableShape(false)
        , hasStableTarget(false)
    {}
};

/**
 * @brief 関数プロファイル情報
 */
struct FunctionProfile {
    uint64_t functionId;                ///< 関数ID
    uint32_t executionCount;           ///< 実行回数
    uint32_t jitCompilationCount;      ///< JITコンパイル回数
    uint32_t deoptimizationCount;      ///< 最適化解除回数
    std::unordered_map<uint32_t, TypeProfile> parameterProfiles;  ///< パラメータの型プロファイル
    std::unordered_map<uint32_t, TypeProfile> variableProfiles;   ///< 変数の型プロファイル
    std::unordered_map<uint32_t, uint32_t> bytecodeHeatmap;      ///< バイトコード実行ヒートマップ
    std::vector<uint32_t> hotLoops;                             ///< ホットループの位置
    
    FunctionProfile()
        : functionId(0)
        , executionCount(0)
        , jitCompilationCount(0)
        , deoptimizationCount(0)
    {}
    
    explicit FunctionProfile(uint64_t id)
        : functionId(id)
        , executionCount(0)
        , jitCompilationCount(0)
        , deoptimizationCount(0)
    {}
};

/**
 * @brief 最適化解除の理由
 */
enum class DeoptimizationReason {
    TypeGuardFailure,     ///< 型ガード失敗
    InlineGuardFailure,   ///< インライン展開ガード失敗
    BailoutRequest,       ///< 明示的なバイルアウト要求
    StackOverflow,        ///< スタックオーバーフロー
    DebuggerAttached,     ///< デバッガ接続
    Unknown               ///< 不明な理由
};

/**
 * @brief JITプロファイラ
 * 
 * 関数実行の統計情報を収集し、JITコンパイラが最適化の判断に使用するデータを提供します。
 */
class JITProfiler {
public:
    /**
     * @brief コンストラクタ
     * @param context 実行コンテキスト
     */
    explicit JITProfiler(Context* context);
    
    /**
     * @brief デストラクタ
     */
    ~JITProfiler();
    
    /**
     * @brief 関数の実行を記録
     * @param functionId 関数ID
     */
    void recordFunctionExecution(uint64_t functionId);
    
    /**
     * @brief 関数のJITコンパイルを記録
     * @param functionId 関数ID
     */
    void recordJITCompilation(uint64_t functionId);
    
    /**
     * @brief 関数の最適化解除を記録
     * @param functionId 関数ID
     * @param reason 最適化解除の理由
     */
    void recordDeoptimization(uint64_t functionId, DeoptimizationReason reason);
    
    /**
     * @brief バイトコードの実行を記録
     * @param functionId 関数ID
     * @param bytecodeOffset バイトコードオフセット
     */
    void recordBytecodeExecution(uint64_t functionId, uint32_t bytecodeOffset);
    
    /**
     * @brief 値の型を記録
     * @param functionId 関数ID
     * @param variableId 変数ID
     * @param value 値
     * @param isParameter パラメータかどうか
     */
    void recordValueType(uint64_t functionId, uint32_t variableId, const Value& value, bool isParameter = false);
    
    /**
     * @brief ホットループを記録
     * @param functionId 関数ID
     * @param bytecodeOffset ループのバイトコードオフセット
     * @param iterationCount 反復回数
     */
    void recordHotLoop(uint64_t functionId, uint32_t bytecodeOffset, uint32_t iterationCount);
    
    /**
     * @brief 関数バイトコードを記録
     * @param functionId 関数ID
     * @param bytecodes バイトコード
     */
    void recordFunctionBytecodes(uint64_t functionId, const std::vector<uint8_t>& bytecodes);
    
    /**
     * @brief 関数の実行回数を取得
     * @param functionId 関数ID
     * @return 実行回数
     */
    uint32_t getFunctionExecutionCount(uint64_t functionId) const;
    
    /**
     * @brief 関数のプロファイル情報を取得
     * @param functionId 関数ID
     * @return プロファイル情報のポインタ（存在しない場合はnullptr）
     */
    void* getFunctionProfile(uint64_t functionId) const;
    
    /**
     * @brief プロファイル情報をリセット
     */
    void reset();
    
    /**
     * @brief プロファイル情報をダンプ
     * @return プロファイル情報の文字列表現
     */
    std::string dumpProfiles() const;
    
private:
    Context* m_context;
    std::unordered_map<uint64_t, FunctionProfile> m_functionProfiles;
    mutable std::mutex m_mutex;
    
    // 内部ヘルパーメソッド
    FunctionProfile& getOrCreateFunctionProfile(uint64_t functionId);
    TypeProfile::ValueType determineValueType(const Value& value) const;
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_JIT_PROFILER_H 