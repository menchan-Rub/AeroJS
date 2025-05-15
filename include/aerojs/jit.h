/**
 * @file jit.h
 * @brief AeroJS 世界最高性能JITコンパイラの制御APIヘッダ
 * @version 2.0.0
 * @license MIT
 */

#ifndef AEROJS_JIT_H
#define AEROJS_JIT_H

#include "aerojs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief JITコンパイルティア
 */
typedef enum {
  AEROJS_JIT_TIER_INTERPRETER = 0, /* インタープリタ実行 */
  AEROJS_JIT_TIER_BASELINE = 1,    /* ベースラインJIT */
  AEROJS_JIT_TIER_OPTIMIZING = 2,  /* 最適化JIT */
  AEROJS_JIT_TIER_SUPER = 3        /* 超最適化JIT */
} AerojsJITTier;

/**
 * @brief JIT命令セット機能フラグ
 */
typedef enum {
  AEROJS_JIT_FEATURE_NONE = 0,
  AEROJS_JIT_FEATURE_SVE = 1 << 0,       /* Scalable Vector Extensions */
  AEROJS_JIT_FEATURE_DOTPROD = 1 << 1,   /* ドット積命令 */
  AEROJS_JIT_FEATURE_JSCVT = 1 << 2,     /* JavaScript変換命令 */
  AEROJS_JIT_FEATURE_CRC32 = 1 << 3,     /* CRC32命令 */
  AEROJS_JIT_FEATURE_CRYPTO = 1 << 4,    /* 暗号化命令 */
  AEROJS_JIT_FEATURE_FP16 = 1 << 5,      /* FP16命令 */
  AEROJS_JIT_FEATURE_BF16 = 1 << 6,      /* BF16命令 */
  AEROJS_JIT_FEATURE_LSE = 1 << 7,       /* Large System Extensions */
  AEROJS_JIT_FEATURE_PAUTH = 1 << 8,     /* ポインタ認証 */
  AEROJS_JIT_FEATURE_BTI = 1 << 9,       /* ブランチターゲット識別 */
  AEROJS_JIT_FEATURE_MTE = 1 << 10       /* メモリタグ拡張 */
} AerojsJITFeatureFlags;

/**
 * @brief JITの最適化パイプライン設定
 */
typedef struct {
  AerojsBool enableFastMath;          /* 高速数学最適化 */
  AerojsBool enableSIMDization;       /* SIMDベクトル化 */
  AerojsBool enableRegisterCoalescing; /* レジスタ併合 */
  AerojsBool enableAdvancedCSE;       /* 高度な共通部分式削除 */
  AerojsBool enableGVN;               /* 大域的値番号付け */
  AerojsBool enableLICM;              /* ループ不変コード移動 */
  AerojsBool enableLoopUnrolling;     /* ループ展開 */
  AerojsBool enableInlining;          /* インライン化 */
  AerojsBool enableSpecialization;    /* 型特化 */
  AerojsBool enableEscapeAnalysis;    /* エスケープ解析 */
} AerojsJITOptimizationPipeline;

/**
 * @brief 特定の関数のJITコンパイルを強制
 * @param engine エンジンインスタンス
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @param tier コンパイルティア
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsForceJITCompile(AerojsEngine* engine,
                                                AerojsContext* ctx,
                                                const char* functionName,
                                                AerojsJITTier tier);

/**
 * @brief 特定の関数のJITコードを破棄
 * @param engine エンジンインスタンス
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsInvalidateJITCode(AerojsEngine* engine,
                                                  AerojsContext* ctx,
                                                  const char* functionName);

/**
 * @brief 利用可能なJIT機能フラグを取得
 * @param engine エンジンインスタンス
 * @param features 機能フラグを格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetAvailableJITFeatures(AerojsEngine* engine,
                                                        AerojsUInt32* features);

/**
 * @brief JIT最適化パイプラインを設定
 * @param engine エンジンインスタンス
 * @param pipeline パイプライン設定
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetJITOptimizationPipeline(AerojsEngine* engine,
                                                          const AerojsJITOptimizationPipeline* pipeline);

/**
 * @brief JIT最適化パイプラインを取得
 * @param engine エンジンインスタンス
 * @param pipeline パイプライン設定を格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetJITOptimizationPipeline(AerojsEngine* engine,
                                                          AerojsJITOptimizationPipeline* pipeline);

/**
 * @brief 関数の最適化情報を取得
 * @param engine エンジンインスタンス
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @param buffer 情報を格納するバッファ
 * @param bufferSize バッファサイズ
 * @param actualSize 実際のサイズを格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetFunctionOptimizationInfo(AerojsEngine* engine,
                                                           AerojsContext* ctx,
                                                           const char* functionName,
                                                           char* buffer,
                                                           AerojsSize bufferSize,
                                                           AerojsSize* actualSize);

/**
 * @brief コンパイル済み関数のIRグラフをダンプ
 * @param engine エンジンインスタンス
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @param buffer 情報を格納するバッファ
 * @param bufferSize バッファサイズ
 * @param actualSize 実際のサイズを格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsDumpFunctionIRGraph(AerojsEngine* engine,
                                                   AerojsContext* ctx,
                                                   const char* functionName,
                                                   char* buffer,
                                                   AerojsSize bufferSize,
                                                   AerojsSize* actualSize);

/**
 * @brief ARM64アーキテクチャ固有の設定を構成
 * @param engine エンジンインスタンス
 * @param vendorOptimizations ベンダー固有の最適化を有効にするかどうか
 * @param enabledFeatures 有効にする機能フラグの組み合わせ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsConfigureARM64JIT(AerojsEngine* engine,
                                                 AerojsBool vendorOptimizations,
                                                 AerojsUInt32 enabledFeatures);

/**
 * @brief コールバック関数型：JITコンパイル時
 * @param engine エンジンインスタンス
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @param tier コンパイルティア
 * @param success 成功したかどうか
 * @param userData ユーザーデータ
 */
typedef void (*AerojsJITCompileCallback)(AerojsEngine* engine,
                                         AerojsContext* ctx,
                                         const char* functionName,
                                         AerojsJITTier tier,
                                         AerojsBool success,
                                         void* userData);

/**
 * @brief コールバック関数型：デオプティマイゼーション時
 * @param engine エンジンインスタンス
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @param reason デオプティマイゼーションの理由
 * @param userData ユーザーデータ
 */
typedef void (*AerojsDeoptimizationCallback)(AerojsEngine* engine,
                                            AerojsContext* ctx,
                                            const char* functionName,
                                            const char* reason,
                                            void* userData);

/**
 * @brief JITコンパイル時のコールバックを設定
 * @param engine エンジンインスタンス
 * @param callback コールバック関数
 * @param userData ユーザーデータ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetJITCompileCallback(AerojsEngine* engine,
                                                     AerojsJITCompileCallback callback,
                                                     void* userData);

/**
 * @brief デオプティマイゼーション時のコールバックを設定
 * @param engine エンジンインスタンス
 * @param callback コールバック関数
 * @param userData ユーザーデータ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetDeoptimizationCallback(AerojsEngine* engine,
                                                         AerojsDeoptimizationCallback callback,
                                                         void* userData);

#ifdef __cplusplus
}
#endif

#endif /* AEROJS_JIT_H */ 