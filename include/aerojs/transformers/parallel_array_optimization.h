/**
 * @file parallel_array_optimization.h
 * @brief AeroJS 並列配列最適化のパブリックAPI
 * @version 2.0.0
 * @license MIT
 */

#ifndef AEROJS_TRANSFORMERS_PUBLIC_PARALLEL_ARRAY_H
#define AEROJS_TRANSFORMERS_PUBLIC_PARALLEL_ARRAY_H

#include "../aerojs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 並列配列最適化器の設定
 */
typedef struct {
    /** サポートするSIMD拡張のビットマスク */
    AerojsUInt32 simdFeatures;
    
    /** 使用するスレッド数（0=自動） */
    AerojsUInt32 threadCount;
    
    /** 最小ベクトル化サイズ */
    AerojsUInt32 minVectorizationSize;
    
    /** 最小並列化サイズ */
    AerojsUInt32 minParallelizationSize;
    
    /** ループタイリングの使用 */
    AerojsBool enableTiling;
    
    /** ループ分割の使用 */
    AerojsBool enableLoopFission;
    
    /** ギャザー/スキャッター最適化の使用 */
    AerojsBool enableGatherScatter;
    
    /** データプリフェッチの使用 */
    AerojsBool enablePrefetching;
    
    /** 最適化レベル (0-3) */
    AerojsUInt32 optimizationLevel;
    
    /** デバッグ出力の有効化 */
    AerojsBool debugMode;
} AerojsParallelArrayConfig;

/**
 * @brief 並列配列最適化器のハンドル
 */
typedef struct AerojsParallelArrayOptimizer* AerojsParallelArrayOptimizerRef;

/**
 * @brief 並列配列最適化器の統計情報
 */
typedef struct {
    /** SIMD順次アクセス最適化の数 */
    AerojsUInt32 simdSequentialOpts;
    
    /** SIMDストライドアクセス最適化の数 */
    AerojsUInt32 simdStridedOpts;
    
    /** 並列ループ変換の数 */
    AerojsUInt32 parallelLoopOpts;
    
    /** 並列ForOf変換の数 */
    AerojsUInt32 parallelForOfOpts;
    
    /** キャッシュ最適化ループの数 */
    AerojsUInt32 cacheOptimizedOpts;
    
    /** ストライド最適化ループの数 */
    AerojsUInt32 strideOptimizedOpts;
    
    /** ギャザー/スキャッター最適化の数 */
    AerojsUInt32 gatherScatterOpts;
    
    /** 変換されたソースサイズの削減率 (0-100%) */
    AerojsUInt32 sizeReductionPercent;
    
    /** 最適化されたコードの推定スピードアップ比率 (100 = 元のコードと同じ速度) */
    AerojsUInt32 estimatedSpeedupPercent;
} AerojsParallelArrayStats;

/**
 * @brief 並列配列最適化器を作成する
 * 
 * @param ctx コンテキスト
 * @param config 最適化設定
 * @return 最適化器ハンドル、失敗時はNULL
 */
AEROJS_EXPORT AerojsParallelArrayOptimizerRef AerojsCreateParallelArrayOptimizer(
    AerojsContext* ctx,
    const AerojsParallelArrayConfig* config);

/**
 * @brief 並列配列最適化器を破棄する
 * 
 * @param optimizer 最適化器ハンドル
 */
AEROJS_EXPORT void AerojsDestroyParallelArrayOptimizer(
    AerojsParallelArrayOptimizerRef optimizer);

/**
 * @brief JavaScriptソースコードを最適化する
 * 
 * @param optimizer 最適化器ハンドル
 * @param source ソースコード
 * @param sourceLen ソースコードの長さ
 * @param filename ファイル名（エラーメッセージ用）
 * @param outputBuffer 出力バッファ（NULLの場合、必要なサイズを返す）
 * @param outputSize 出力バッファのサイズ（バイト単位）
 * @return 最適化されたコードの長さ、エラー時は0
 */
AEROJS_EXPORT AerojsSize AerojsOptimizeArrayCode(
    AerojsParallelArrayOptimizerRef optimizer,
    const char* source,
    AerojsSize sourceLen,
    const char* filename,
    char* outputBuffer,
    AerojsSize outputSize);

/**
 * @brief 最適化器の統計情報を取得する
 * 
 * @param optimizer 最適化器ハンドル
 * @param stats 統計情報を格納する構造体へのポインタ
 * @return 成功時はAEROJS_SUCCESS、失敗時はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetParallelArrayStats(
    AerojsParallelArrayOptimizerRef optimizer,
    AerojsParallelArrayStats* stats);

/**
 * @brief 最適化器の設定を更新する
 * 
 * @param optimizer 最適化器ハンドル
 * @param config 新しい設定
 * @return 成功時はAEROJS_SUCCESS、失敗時はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsUpdateParallelArrayConfig(
    AerojsParallelArrayOptimizerRef optimizer,
    const AerojsParallelArrayConfig* config);

/**
 * @brief 最適化器がSIMD機能をサポートしているかをチェックする
 * 
 * @param optimizer 最適化器ハンドル
 * @param feature チェックするSIMD機能
 * @return サポートしている場合はtrue、それ以外はfalse
 */
AEROJS_EXPORT AerojsBool AerojsParallelArrayHasSIMDSupport(
    AerojsParallelArrayOptimizerRef optimizer,
    AerojsUInt32 feature);

#ifdef __cplusplus
}
#endif

#endif // AEROJS_TRANSFORMERS_PUBLIC_PARALLEL_ARRAY_H 