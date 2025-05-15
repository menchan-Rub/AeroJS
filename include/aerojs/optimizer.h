/**
 * @file optimizer.h
 * @brief AeroJS 世界最高性能JavaScriptエンジンの最適化エンジンAPI
 * @version 2.0.0
 * @license MIT
 */

#ifndef AEROJS_OPTIMIZER_H
#define AEROJS_OPTIMIZER_H

#include "aerojs.h"
#include "jit.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 最適化レベル（標準）
 */
typedef enum {
  AEROJS_OPT_LEVEL_NONE = 0,    /* 最適化なし */
  AEROJS_OPT_LEVEL_LOW = 1,     /* 低レベル最適化 */
  AEROJS_OPT_LEVEL_MEDIUM = 2,  /* 中レベル最適化 */
  AEROJS_OPT_LEVEL_HIGH = 3     /* 高レベル最適化 */
} AerojsOptimizationLevel;

/**
 * @brief プロファイル収集モード
 */
typedef enum {
  AEROJS_PROFILE_MODE_NONE = 0,       /* プロファイルデータなし */
  AEROJS_PROFILE_MODE_COUNTS = 1,     /* 実行回数のカウントのみ */
  AEROJS_PROFILE_MODE_TYPES = 2,      /* 型情報も収集 */
  AEROJS_PROFILE_MODE_DETAILED = 3,   /* 詳細なプロファイル情報 */
  AEROJS_PROFILE_MODE_EXTREME = 4     /* すべての実行データを収集 */
} AerojsProfileMode;

/**
 * @brief 最適化オプション
 */
typedef struct {
  AerojsOptimizationLevel level;          /* 最適化レベル */
  AerojsProfileMode profileMode;          /* プロファイルモード */
  AerojsBool inlineSmallFunctions;        /* 小さな関数のインライン化 */
  AerojsBool eliminateDeadCode;           /* デッドコード除去 */
  AerojsBool optimizeLoops;               /* ループ最適化 */
  AerojsBool hoistInvariants;             /* ループ不変式の移動 */
  AerojsBool foldConstants;               /* 定数畳み込み */
  AerojsBool eliminateRedundancy;         /* 冗長な計算の除去 */
  AerojsBool specializedFastPaths;        /* 特殊ケースの高速パス */
  AerojsBool eliminateBoxing;             /* ボクシング/アンボクシングの除去 */
  AerojsBool propagateTypes;              /* 型伝播 */
  AerojsBool optimizePropertyAccess;      /* プロパティアクセスの最適化 */
  AerojsBool registerAllocation;          /* レジスタ割り当て最適化 */
  AerojsBool enableSIMD;                  /* SIMD最適化 */
  AerojsBool enableValueNumbering;        /* 値番号付け */
  AerojsBool optimizeControlFlow;         /* 制御フロー最適化 */
  AerojsBool optimizeMemoryAccess;        /* メモリアクセス最適化 */
  AerojsUInt32 inliningThreshold;         /* インライン化しきい値 */
  AerojsUInt32 loopUnrollFactor;          /* ループアンロール係数 */
  AerojsBool enableSpeculation;           /* 投機的最適化 */
  AerojsBool useOptimisticTypes;          /* 楽観的型最適化 */
  AerojsBool enableAutoVectorization;     /* 自動ベクトル化 */
  AerojsBool enableMetaTracing;           /* メタトレース最適化 */
  AerojsBool enablePredication;           /* 条件付き実行の最適化 */
  AerojsBool enableRegisterCoalescing;    /* レジスタ併合 */
  AerojsBool enableCommonSubexprElimination; /* 共通部分式の削除 */
  AerojsBool enableRangeAnalysis;         /* 範囲解析 */
  AerojsBool enableLICM;                  /* ループ不変コード移動 */
  AerojsBool enableGVN;                   /* 大域的値番号付け */
  AerojsBool enableFastMath;              /* 高速な数学演算 */
  AerojsBool enableEscapeAnalysis;        /* エスケープ解析 */
  AerojsBool enableDeadStoreElimination;  /* 不要ストア削除 */
  AerojsBool enableExactGC;               /* 正確なGC最適化 */
  AerojsBool enableStackAllocation;       /* スタック割り当て最適化 */
  AerojsBool enableUnboxedValues;         /* アンボックス値の最適化 */
  AerojsBool enablePIC;                   /* 多態性インラインキャッシュ */
  AerojsBool enableDCE;                   /* デッドコード除去 */
  AerojsBool enablePGO;                   /* プロファイル誘導型最適化 */
  AerojsBool enableThreads;               /* マルチスレッド最適化 */
  AerojsUInt32 bailoutThreshold;          /* 最適化解除のしきい値 */
  AerojsUInt32 specializationThreshold;   /* 特殊化のしきい値 */
  AerojsBool enableTailCallOptimization;  /* 末尾呼び出し最適化 */
  AerojsBool enableRegisterHints;         /* レジスタ割り当てヒント */
  AerojsBool enableSuperBlocks;           /* スーパーブロック形成 */
  AerojsBool enableDevirtualization;      /* 仮想メソッド解決 */
  AerojsBool enableCodeSplitting;         /* コード分割 */
  AerojsUInt32 inlineDepth;               /* インライン化の深さ制限 */
  AerojsBool enableFastArrayAccess;       /* 配列アクセス最適化 */
  AerojsBool enableStringOptimizations;   /* 文字列操作の最適化 */
  AerojsBool enableAutoParallelization;   /* 自動並列化 */
  AerojsBool enableProfileFeedback;       /* プロファイルフィードバック */
  AerojsUInt32 superOptimizationLevel;    /* 超最適化レベル */
} AerojsOptimizerOptions;

/**
 * @brief 最適化エンジンの状態
 */
typedef struct {
  AerojsUInt32 compiledFunctions;      /* コンパイルされた関数数 */
  AerojsUInt32 optimizedFunctions;     /* 最適化された関数数 */
  AerojsUInt32 inlinedFunctions;       /* インライン化された関数数 */
  AerojsUInt32 deoptimizedFunctions;   /* 最適化解除された関数数 */
  AerojsUInt32 speculativeFunctions;   /* 投機的に最適化された関数数 */
  AerojsUInt64 optimizationTime;       /* 最適化に費やした時間（ナノ秒） */
  AerojsUInt64 codeSize;               /* 生成されたコードのサイズ（バイト） */
  AerojsUInt32 typeSpecializations;    /* 型特殊化の数 */
  AerojsUInt32 hoistedExpressions;     /* ループ外に移動された式の数 */
  AerojsUInt32 eliminatedAllocations;  /* 除去されたアロケーション数 */
  AerojsUInt32 vectorizedLoops;        /* ベクトル化されたループ数 */
  AerojsUInt32 cacheHits;              /* インラインキャッシュヒット数 */
  AerojsUInt32 cacheMisses;            /* インラインキャッシュミス数 */
  AerojsUInt32 guardsInserted;         /* 挿入されたガード数 */
  AerojsUInt32 guardsFailed;           /* 失敗したガード数 */
  AerojsUInt32 tracesCompiled;         /* コンパイルされたトレース数 */
  AerojsUInt32 tracesAborted;          /* 中止されたトレース数 */
  AerojsUInt32 superOptimizedFunctions; /* 超最適化された関数数 */
  AerojsUInt32 parallelizedLoops;      /* 並列化されたループ数 */
  AerojsUInt32 unboxedValues;          /* アンボックスされた値の数 */
  AerojsUInt32 redundantChecksRemoved; /* 除去された冗長チェック数 */
  AerojsUInt32 constantsFolded;        /* 畳み込まれた定数数 */
  AerojsUInt32 deadStoresEliminated;   /* 除去された不要ストア数 */
  AerojsUInt32 tailCallsOptimized;     /* 最適化された末尾呼び出し数 */
} AerojsOptimizerState;

/**
 * @brief 最適化分析情報
 */
typedef struct {
  char functionName[256];              /* 関数名 */
  AerojsUInt32 bytecodeSizeOrig;       /* 元のバイトコードサイズ */
  AerojsUInt32 bytecodeSizeOpt;        /* 最適化後のバイトコードサイズ */
  AerojsUInt32 nativeSizeOrig;         /* 元のネイティブコードサイズ */
  AerojsUInt32 nativeSizeOpt;          /* 最適化後のネイティブコードサイズ */
  AerojsUInt32 executionCount;         /* 実行回数 */
  AerojsUInt32 deoptimizationCount;    /* 最適化解除回数 */
  AerojsUInt32 inlinedCount;           /* インライン化された関数数 */
  AerojsUInt32 specializationCount;    /* 型特殊化の数 */
  AerojsUInt32 loopOptimizationCount;  /* ループ最適化の数 */
  AerojsUInt32 guardCount;             /* ガード数 */
  AerojsUInt32 memoryAccesses;         /* メモリアクセス数 */
  AerojsUInt32 propertyAccesses;       /* プロパティアクセス数 */
  AerojsUInt32 redundanciesRemoved;    /* 除去された冗長コード数 */
  AerojsUInt64 totalExecutionTimeNs;   /* 総実行時間（ナノ秒） */
  AerojsUInt64 avgExecutionTimeNs;     /* 平均実行時間（ナノ秒） */
  AerojsUInt32 hotRegionCount;         /* ホットリージョン数 */
  AerojsUInt32 coldRegionCount;        /* コールドリージョン数 */
  AerojsUInt32 criticalPathLength;     /* クリティカルパス長 */
  float speedupRatio;                  /* 高速化率 */
  float memoryReductionRatio;          /* メモリ削減率 */
  AerojsUInt32 simdOperationCount;     /* SIMD操作数 */
  AerojsUInt32 parallelOperationCount; /* 並列操作数 */
  AerojsUInt32 successfulSpeculations; /* 成功した投機数 */
  AerojsUInt32 failedSpeculations;     /* 失敗した投機数 */
  AerojsOptimizationLevel optLevel;    /* 適用された最適化レベル */
  AerojsUInt32 superOptRegionCount;    /* 超最適化されたリージョン数 */
} AerojsOptimizationAnalysis;

/**
 * @brief 自動最適化設定
 */
typedef struct {
  AerojsBool enableAutoTuning;         /* 自動チューニングを有効にする */
  AerojsUInt32 feedbackInterval;       /* フィードバック間隔（実行回数） */
  AerojsUInt32 adaptationThreshold;    /* 適応しきい値 */
  AerojsUInt32 samplingRate;           /* サンプリングレート（0-100%） */
  AerojsBool enableReverting;          /* 最適化を元に戻す機能を有効にする */
  AerojsBool enableOnTheFly;           /* 実行中最適化を有効にする */
  AerojsUInt32 minExecutionCount;      /* 最小実行回数 */
  AerojsUInt32 maxOptimizationTime;    /* 最大最適化時間（ミリ秒） */
  AerojsBool preserveCriticalPaths;    /* クリティカルパスを優先する */
  AerojsBool preserveBattery;          /* バッテリーを保護する（モバイル） */
  AerojsBool adaptToThermal;           /* 熱状態に適応する */
  AerojsUInt32 autoVectorizationSize;  /* 自動ベクトル化の最小サイズ */
  AerojsBool enableCrossFunction;      /* 関数をまたいだ最適化を有効にする */
  AerojsUInt32 maxParallelThreads;     /* 最大並列スレッド数 */
  AerojsBool enableSchedulerHints;     /* スケジューラヒントを有効にする */
} AerojsAutoOptimizationConfig;

/**
 * @brief AIアシスト最適化設定
 */
typedef struct {
  AerojsBool enableAI;                 /* AIアシスト最適化を有効にする */
  AerojsUInt32 mlModelVersion;         /* 機械学習モデルのバージョン */
  AerojsUInt32 confidenceThreshold;    /* 信頼度しきい値（0-100） */
  AerojsBool enableOnlineTraining;     /* オンライントレーニングを有効にする */
  AerojsBool useHardwareAcceleration;  /* ハードウェアアクセラレーションを使用する */
  AerojsUInt32 inferenceTimeout;       /* 推論タイムアウト（ミリ秒） */
  AerojsBool prioritizeAccuracy;       /* 精度を優先する */
  AerojsUInt32 modelComplexity;        /* モデル複雑度（1-5） */
  AerojsBool enableSpecPatternMatching; /* 特殊パターンマッチングを有効にする */
  AerojsUInt32 patternLibraryVersion;  /* パターンライブラリバージョン */
  AerojsBool secureMode;               /* セキュアモードでAIを実行 */
  AerojsBool enableFeedback;           /* AIへのフィードバックを有効にする */
} AerojsAIOptimizationConfig;

/**
 * @brief デフォルトの最適化オプションを取得
 *
 * @param engine エンジンインスタンス
 * @param options 設定を格納する構造体へのポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetDefaultOptimizerOptions(AerojsEngine* engine, AerojsOptimizerOptions* options);

/**
 * @brief 最適化オプションを設定
 *
 * @param engine エンジンインスタンス
 * @param options 設定オプション
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetOptimizerOptions(AerojsEngine* engine, const AerojsOptimizerOptions* options);

/**
 * @brief 最適化エンジンの状態を取得
 *
 * @param engine エンジンインスタンス
 * @param state 状態を格納する構造体へのポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetOptimizerState(AerojsEngine* engine, AerojsOptimizerState* state);

/**
 * @brief 関数の最適化分析を取得
 *
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @param analysis 分析情報を格納する構造体へのポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetFunctionOptimizationAnalysis(AerojsContext* ctx, 
                                                               const char* functionName,
                                                               AerojsOptimizationAnalysis* analysis);

/**
 * @brief 関数を最適化
 *
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @param level 最適化レベル
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsOptimizeFunction(AerojsContext* ctx, 
                                                const char* functionName,
                                                AerojsOptimizationLevel level);

/**
 * @brief 関数の最適化を解除
 *
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsDeoptimizeFunction(AerojsContext* ctx, const char* functionName);

/**
 * @brief 関数の最適化をロック（変更を防止）
 *
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsLockFunctionOptimization(AerojsContext* ctx, const char* functionName);

/**
 * @brief 関数の最適化をロック解除
 *
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsUnlockFunctionOptimization(AerojsContext* ctx, const char* functionName);

/**
 * @brief 最適化エンジンの統計をリセット
 *
 * @param engine エンジンインスタンス
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsResetOptimizerStats(AerojsEngine* engine);

/**
 * @brief 最適化中間表現（IR）をダンプ
 *
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @param buffer 出力バッファ
 * @param bufferSize バッファサイズ
 * @param actualSize 実際のサイズを格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsDumpOptimizationIR(AerojsContext* ctx, 
                                                  const char* functionName,
                                                  char* buffer, 
                                                  AerojsSize bufferSize,
                                                  AerojsSize* actualSize);

/**
 * @brief 自動最適化設定を構成
 *
 * @param engine エンジンインスタンス
 * @param config 自動最適化設定
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsConfigureAutoOptimization(AerojsEngine* engine, 
                                                        const AerojsAutoOptimizationConfig* config);

/**
 * @brief AIアシスト最適化を構成
 *
 * @param engine エンジンインスタンス
 * @param config AIアシスト最適化設定
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsConfigureAIOptimization(AerojsEngine* engine, 
                                                      const AerojsAIOptimizationConfig* config);

/**
 * @brief 最適化プロファイルをエクスポート
 *
 * @param engine エンジンインスタンス
 * @param filename ファイル名
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsExportOptimizationProfile(AerojsEngine* engine, const char* filename);

/**
 * @brief 最適化プロファイルをインポート
 *
 * @param engine エンジンインスタンス
 * @param filename ファイル名
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsImportOptimizationProfile(AerojsEngine* engine, const char* filename);

/**
 * @brief ホットスポット関数を検出
 *
 * @param engine エンジンインスタンス
 * @param functionNames 関数名の配列
 * @param maxFunctions 配列の最大サイズ
 * @param actualCount 実際の数を格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsDetectHotspotFunctions(AerojsEngine* engine, 
                                                     char** functionNames,
                                                     AerojsUInt32 maxFunctions,
                                                     AerojsUInt32* actualCount);

/**
 * @brief スーパーブロック最適化を実行
 *
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsPerformSuperBlockOptimization(AerojsContext* ctx, const char* functionName);

/**
 * @brief メタトレース最適化を実行
 *
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsPerformMetaTraceOptimization(AerojsContext* ctx, const char* functionName);

/**
 * @brief コールバック関数型：最適化イベント
 *
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @param level 最適化レベル
 * @param successful 成功したかどうか
 * @param userData ユーザーデータ
 */
typedef void (*AerojsOptimizationCallback)(AerojsContext* ctx, 
                                         const char* functionName, 
                                         AerojsOptimizationLevel level,
                                         AerojsBool successful,
                                         void* userData);

/**
 * @brief 最適化イベントコールバックを設定
 *
 * @param engine エンジンインスタンス
 * @param callback コールバック関数
 * @param userData ユーザーデータ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetOptimizationCallback(AerojsEngine* engine, 
                                                       AerojsOptimizationCallback callback,
                                                       void* userData);

/**
 * @brief 超最適化を実行
 *
 * @param ctx コンテキスト
 * @param functionName 関数名
 * @param level 超最適化レベル
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsPerformSuperOptimization(AerojsContext* ctx, 
                                                        const char* functionName,
                                                        AerojsUInt32 level);

#ifdef __cplusplus
}
#endif

#endif /* AEROJS_OPTIMIZER_H */ 