#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <array>
#include <memory>
#include "../bytecode/bytecode.h"
#include "../value/value.h"
#include <chrono>
#include <functional>

namespace aerojs::core {

// 前方宣言
struct ProfiledTypeInfo;
class IRBuilder;
class JITCompiler;
class BytecodeFunction;

// プロファイリングデータ型
enum class ProfileDataType {
    CallCount,          // 関数呼び出し回数
    TypeFeedback,       // 型フィードバック情報
    BranchBias,         // 分岐バイアス情報
    LoopIterationCount, // ループ反復回数
    FieldAccess,        // フィールドアクセスパターン
    AllocationSite,     // オブジェクト割り当てサイト
    DeoptimizationPoint // 逆最適化が発生した地点
};

// プロファイリングポイント識別子
struct ProfilingPointId {
    uint32_t functionId;
    uint32_t bytecodeOffset;
    uint8_t slot;
    
    bool operator==(const ProfilingPointId& other) const {
        return functionId == other.functionId &&
               bytecodeOffset == other.bytecodeOffset &&
               slot == other.slot;
    }
};

// ハッシュ関数の特殊化
struct ProfilingPointIdHash {
    std::size_t operator()(const ProfilingPointId& id) const {
        return (std::hash<uint32_t>()(id.functionId) ^
                (std::hash<uint32_t>()(id.bytecodeOffset) << 1) ^
                (std::hash<uint8_t>()(id.slot) << 2));
    }
};

// プロファイリングデータ構造
// 型情報
struct ProfiledTypeInfo {
    uint8_t expectedType = 0;  // 予測される型ID
    uint32_t typeCheckFailures = 0;  // 型予測失敗回数
    bool isInlined = false;  // インライン化されたか
    
    bool isStable() const { return typeCheckFailures < 10; }
};

//==============================================================================
// 型フィードバックの記録用構造体
//==============================================================================

/**
 * 型フィードバックレコード
 */
struct TypeFeedbackRecord {
    // 型カテゴリ
    enum class TypeCategory {
        Unknown,    // 不明な型
        Integer,    // 整数型
        Double,     // 浮動小数点型
        Boolean,    // 真偽値型
        String,     // 文字列型
        Object,     // オブジェクト型
        Array,      // 配列型
        Function,   // 関数型
        Null,       // null
        Undefined,  // undefined
        Mixed       // 複数の型が混在
    };
    
    TypeCategory category;   // 観測された型
    uint32_t observationCount; // 観測回数
    uint32_t totalObservations; // 全観測回数
    bool hasNegativeZero;    // 負のゼロが観測されたか
    bool hasNaN;             // NaNが観測されたか
    float confidence;        // 型の信頼度 (0.0-1.0)
    
    // デフォルトコンストラクタ
    TypeFeedbackRecord()
        : category(TypeCategory::Unknown),
          observationCount(0),
          totalObservations(0),
          hasNegativeZero(false),
          hasNaN(false),
          confidence(0.0f) {}
    
    // カテゴリの名前を取得
    std::string GetCategoryName() const;
    
    // 型の安定性を計算
    float GetTypeStability() const {
        if (totalObservations == 0) return 0.0f;
        return static_cast<float>(observationCount) / totalObservations;
    }
};

//==============================================================================
// プロパティアクセスプロファイル
//==============================================================================

struct PropertyAccessProfile {
    uint32_t accessCount = 0;               // このプロパティへのアクセス回数
    uint32_t mostCommonShapeId = 0;         // 最も一般的なオブジェクト形状ID
    uint32_t shapeObservationCount = 0;     // 形状が観測された回数
    float shapeConsistency = 0.0f;          // 形状の一貫性（0.0〜1.0）
    bool isMonomorphic = false;             // 単一の形状のみが観測されたか
    bool isPolymorphic = false;             // 複数の形状が観測されたか（2-4）
    bool isMegamorphic = false;             // 多数の異なる形状が観測されたか（5+）
    
    // プロパティ名（利用可能な場合）
    std::string propertyName;
};

//==============================================================================
// 実行回数カウンター
//==============================================================================

struct ExecutionCounter {
    uint32_t executionCount = 0;            // 実行回数
    float averageIterations = 0.0f;         // ループの場合の平均反復回数
    
    // 呼び出しサイト用
    std::pair<uint32_t, uint32_t> mostCommonTarget = {0, 0};  // 最も一般的なターゲット関数IDとその呼び出し回数
};

//==============================================================================
// 関数プロファイル - 関数ごとの実行データを保持
//==============================================================================

struct FunctionProfile {
    uint32_t functionId = 0;                             // 関数のID
    std::string functionName;                            // 関数名（利用可能な場合）
    uint32_t totalExecutions = 0;                        // 総実行回数
    std::vector<TypeFeedbackRecord> typeFeedback;        // 変数ごとの型フィードバック
    
    // バイトコードオフセットごとのプロファイリングデータ
    std::unordered_map<uint32_t, TypeFeedbackRecord> arithmeticOperations;    // 算術演算の型フィードバック
    std::unordered_map<uint32_t, TypeFeedbackRecord> comparisonOperations;    // 比較演算の型フィードバック
    std::unordered_map<uint32_t, PropertyAccessProfile> propertyAccesses;     // プロパティアクセス情報
    std::unordered_map<uint32_t, ExecutionCounter> loopExecutionCounts;       // ループ実行回数
    std::unordered_map<uint32_t, ExecutionCounter> callSiteExecutionCounts;   // 呼び出しサイト実行回数
    
    // ホットスポット
    std::vector<uint32_t> hotSpots;                      // 実行頻度の高いバイトコードオフセット
    
    // バイトコードダンプ（解析用）
    std::shared_ptr<std::vector<uint8_t>> bytecodes;     // 関数のバイトコード
};

//==============================================================================
// JITプロファイラークラス - 実行データの収集と分析を担当
//==============================================================================

/**
 * @class JITProfiler
 * @brief JIT実行のプロファイリング情報を収集し、最適化の判断をサポートするクラス
 *
 * このクラスは関数の呼び出し回数、実行時間、実行パス（基本ブロック）の情報を収集し、
 * どの関数を最適化すべきかの判断材料を提供します。また、関数内のホットパス（頻繁に実行される経路）
 * を特定するための機能も提供します。
 */
class JITProfiler {
public:
    /**
     * @struct FunctionProfile
     * @brief 関数のプロファイリング情報を保持する構造体
     */
    struct FunctionProfile {
        uint32_t function_id;         ///< 関数のID
        uint32_t bytecode_size;       ///< バイトコードのサイズ
        uint32_t call_count;          ///< 呼び出し回数
        uint64_t execution_time_us;   ///< 累積実行時間（マイクロ秒）
        bool is_optimized;            ///< 最適化済みフラグ
        std::vector<uint32_t> hot_paths; ///< ホットパスのオフセットリスト

        FunctionProfile(uint32_t id, uint32_t size)
            : function_id(id)
            , bytecode_size(size)
            , call_count(0)
            , execution_time_us(0)
            , is_optimized(false)
        {}
    };

    /**
     * @struct BlockProfile
     * @brief 基本ブロックのプロファイリング情報を保持する構造体
     */
    struct BlockProfile {
        uint32_t block_id;            ///< ブロックID（通常はスタートオフセットを使用）
        uint32_t start_offset;        ///< ブロックの開始オフセット
        uint32_t end_offset;          ///< ブロックの終了オフセット
        uint32_t execution_count;     ///< 実行回数
        std::vector<uint32_t> target_blocks; ///< このブロックから分岐する先のブロックIDリスト

        BlockProfile(uint32_t id, uint32_t start, uint32_t end)
            : block_id(id)
            , start_offset(start)
            , end_offset(end)
            , execution_count(0)
        {}
    };

public:
    /**
     * @brief コンストラクタ
     */
    JITProfiler();

    /**
     * @brief デストラクタ
     */
    ~JITProfiler();

    /**
     * @brief 関数の実行開始を記録
     * @param function_id 関数ID
     * @param bytecode_size バイトコードサイズ
     */
    void FunctionEntry(uint32_t function_id, uint32_t bytecode_size);

    /**
     * @brief 関数の実行終了を記録
     * @param function_id 関数ID
     */
    void FunctionExit(uint32_t function_id);

    /**
     * @brief 基本ブロックの実行を記録
     * @param function_id 関数ID
     * @param block_offset ブロックの開始オフセット
     */
    void RecordBlockExecution(uint32_t function_id, uint32_t block_offset);

    /**
     * @brief 分岐命令の実行を記録
     * @param function_id 関数ID
     * @param source_offset 分岐元のオフセット
     * @param target_offset 分岐先のオフセット
     */
    void RecordBranchExecution(uint32_t function_id, uint32_t source_offset, uint32_t target_offset);

    /**
     * @brief 関数を最適化すべきかどうかを判断
     * @param function_id 関数ID
     * @return 最適化すべき場合はtrue、そうでない場合はfalse
     */
    bool ShouldOptimizeFunction(uint32_t function_id) const;

    /**
     * @brief 関数のプロファイル情報を取得
     * @param function_id 関数ID
     * @return プロファイル情報へのポインタ。見つからない場合はnullptr
     */
    const FunctionProfile* GetFunctionProfile(uint32_t function_id) const;

    /**
     * @brief ホット関数（頻繁に呼び出される関数）のリストを取得
     * @param threshold 閾値（この回数以上呼び出された関数を返す）
     * @return ホット関数のプロファイル情報のリスト
     */
    std::vector<const FunctionProfile*> GetHotFunctions(uint32_t threshold) const;

    /**
     * @brief 関数内のホットパス（頻繁に実行されるパス）を特定
     * @param function_id 関数ID
     * @return ホットパスの開始オフセットのリスト
     */
    std::vector<uint32_t> IdentifyHotPaths(uint32_t function_id) const;

    /**
     * @brief プロファイリング情報をリセット
     */
    void Reset();

    /**
     * @brief 関数を最適化済みとしてマーク
     * @param function_id 関数ID
     */
    void MarkFunctionAsOptimized(uint32_t function_id);

    /**
     * @brief 関数のバイトコードを取得
     * @param functionId 関数ID
     * @return バイトコードへの共有ポインタ
     */
    std::shared_ptr<std::vector<uint8_t>> GetFunctionBytecodes(uint32_t functionId) const;

    /**
     * @brief プロファイリング情報をダンプ（出力）
     */
    void DumpProfileInfo() const;

    /**
     * @brief プロファイリングの有効/無効を切り替え
     * @param enable trueで有効、falseで無効
     */
    void EnableProfiling(bool enable);

private:
    /**
     * @brief 現在時刻をマイクロ秒単位で取得
     * @return 現在時刻（マイクロ秒）
     */
    uint64_t CurrentTimeMicros() const;

    /**
     * @brief 関数プロファイルを取得または作成
     * @param function_id 関数ID
     * @param bytecode_size バイトコードサイズ
     * @return 関数プロファイルへのポインタ
     */
    FunctionProfile* GetOrCreateFunctionProfile(uint32_t function_id, uint32_t bytecode_size);

    /**
     * @brief ブロックプロファイルを取得または作成
     * @param function_id 関数ID
     * @param block_offset ブロックの開始オフセット
     * @param end_offset ブロックの終了オフセット
     * @return ブロックプロファイルへのポインタ
     */
    BlockProfile* GetOrCreateBlockProfile(uint32_t function_id, uint32_t block_offset, uint32_t end_offset);

private:
    mutable std::mutex m_mutex; ///< スレッドセーフ操作のためのミューテックス

    std::unordered_map<uint32_t, std::unique_ptr<FunctionProfile>> m_function_profiles; ///< 関数IDからプロファイルへのマップ
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::unique_ptr<BlockProfile>>> m_block_profiles; ///< 関数ID -> (ブロックオフセット -> プロファイル)のマップ
    std::unordered_map<uint32_t, uint64_t> m_function_start_times; ///< 関数の実行開始時間（関数ID -> 時間）

    // 閾値設定
    static constexpr uint32_t m_hot_function_threshold = 100; ///< ホット関数判定の閾値（呼び出し回数）
    static constexpr uint32_t m_hot_block_threshold = 1000;   ///< ホットブロック判定の閾値（実行回数）
};

} // namespace aerojs::core 