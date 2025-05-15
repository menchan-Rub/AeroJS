/**
 * @file engine.h
 * @brief AeroJS 世界最高性能JavaScriptエンジンを管理するためのAPI
 * @version 2.0.0
 * @license MIT
 */

#ifndef AEROJS_ENGINE_H
#define AEROJS_ENGINE_H

#include "aerojs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief エンジンのパラメータを設定
 * @param engine エンジンインスタンス
 * @param paramName パラメータ名
 * @param value パラメータ値
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetEngineParameter(AerojsEngine* engine,
                                                    const char* paramName,
                                                    const char* value);

/**
 * @brief エンジンのパラメータを取得
 * @param engine エンジンインスタンス
 * @param paramName パラメータ名
 * @param value パラメータ値を格納するバッファ
 * @param maxSize バッファサイズ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetEngineParameter(AerojsEngine* engine,
                                                    const char* paramName,
                                                    char* value,
                                                    AerojsSize maxSize);

/**
 * @brief メモリ使用統計を取得
 */
typedef struct {
  AerojsSize totalHeapSize;      /* 合計ヒープサイズ（バイト単位） */
  AerojsSize usedHeapSize;       /* 使用中ヒープサイズ（バイト単位） */
  AerojsSize heapSizeLimit;      /* ヒープサイズ制限（バイト単位） */
  AerojsSize totalExternalSize;  /* 外部メモリ合計（バイト単位） */
  AerojsSize mallocedMemory;     /* malloc()で確保されたメモリ（バイト単位） */
  AerojsSize peakMallocedMemory; /* malloc()のピーク使用量（バイト単位） */
  AerojsUInt32 objectCount;      /* オブジェクト数 */
  AerojsUInt32 stringCount;      /* 文字列数 */
  AerojsUInt32 symbolCount;      /* シンボル数 */
  AerojsUInt32 contextCount;     /* コンテキスト数 */
  AerojsUInt32 gcCount;          /* GC実行回数 */
  AerojsUInt32 gcTime;           /* GC累積実行時間（ミリ秒） */
} AerojsMemoryStats;

/**
 * @brief エンジンのメモリ使用統計を取得
 * @param engine エンジンインスタンス
 * @param stats 統計情報を格納する構造体へのポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetMemoryStats(AerojsEngine* engine, AerojsMemoryStats* stats);

/**
 * @brief エンジンのグローバルGCを実行
 * @param engine エンジンインスタンス
 */
AEROJS_EXPORT void AerojsCollectGarbageGlobal(AerojsEngine* engine);

/**
 * @brief メモリ使用量の上限を設定
 * @param engine エンジンインスタンス
 * @param limit メモリ使用量の上限（バイト単位）、0は制限なし
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetMemoryLimit(AerojsEngine* engine, AerojsSize limit);

/**
 * @brief 既存のコンテキストを全て列挙
 * @param engine エンジンインスタンス
 * @param contexts コンテキストポインタの配列
 * @param maxContexts 配列の最大サイズ
 * @param contextCount 実際のコンテキスト数を格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsEnumerateContexts(AerojsEngine* engine,
                                                   AerojsContext** contexts,
                                                   AerojsSize maxContexts,
                                                   AerojsSize* contextCount);

/**
 * @brief JITコンパイラを有効/無効化
 * @param engine エンジンインスタンス
 * @param enable 有効にする場合はAEROJS_TRUE、無効にする場合はAEROJS_FALSE
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsEnableJIT(AerojsEngine* engine, AerojsBool enable);

/**
 * @brief JITコンパイルのしきい値を設定
 * @param engine エンジンインスタンス
 * @param threshold しきい値（関数の実行回数）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetJITThreshold(AerojsEngine* engine, AerojsUInt32 threshold);

/**
 * @brief 最適化レベルを設定
 * @param engine エンジンインスタンス
 * @param level 最適化レベル（0: なし、1: 低、2: 中、3: 高）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetOptimizationLevel(AerojsEngine* engine, AerojsUInt32 level);

/**
 * @brief JIT統計情報
 */
typedef struct {
  AerojsUInt32 compiledFunctions;    /* コンパイルされた関数数 */
  AerojsUInt32 optimizedFunctions;   /* 最適化された関数数 */
  AerojsUInt32 deoptimizedFunctions; /* 最適化解除された関数数 */
  AerojsUInt32 codeSize;             /* 生成されたコードサイズ（バイト単位） */
  AerojsUInt64 compileTime;          /* コンパイル累積時間（マイクロ秒） */
  AerojsUInt32 interpreterTime;      /* インタープリタ累積実行時間（ミリ秒） */
  AerojsUInt32 jitTime;              /* JITコード累積実行時間（ミリ秒） */
  AerojsUInt32 superOptimizedFunctions; /* 超最適化された関数数 */
  AerojsUInt32 speculativeCompilations; /* スペキュレーティブコンパイル数 */
  AerojsUInt32 inlinedFunctions;     /* インライン化された関数数 */
  AerojsUInt32 eliminatedDeadCode;   /* 除去されたデッドコード行数 */
  AerojsUInt32 hoistedInvariants;    /* ループ外に移動された不変コード数 */
  AerojsUInt32 vectorizedLoops;      /* ベクトル化されたループ数 */
  AerojsUInt32 specializationCount;  /* 型特化された関数数 */
  AerojsUInt32 simdInstructionsCount; /* 生成されたSIMD命令数 */
  AerojsUInt64 optimizationTimeNs;   /* 最適化に費やされた時間（ナノ秒） */
  AerojsUInt64 peakMemoryUsage;      /* ピークメモリ使用量（バイト単位） */
  AerojsUInt32 codeCacheHits;        /* コードキャッシュヒット数 */
  AerojsUInt32 codeCacheMisses;      /* コードキャッシュミス数 */
  AerojsUInt32 icHits;               /* インラインキャッシュヒット数 */
  AerojsUInt32 icMisses;             /* インラインキャッシュミス数 */
  AerojsUInt32 warmupTime;           /* ウォームアップ時間（ミリ秒） */
} AerojsJITStats;

/**
 * @brief JIT統計情報を取得
 * @param engine エンジンインスタンス
 * @param stats 統計情報を格納する構造体へのポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetJITStats(AerojsEngine* engine, AerojsJITStats* stats);

/**
 * @brief JIT統計情報をリセット
 * @param engine エンジンインスタンス
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsResetJITStats(AerojsEngine* engine);

/**
 * @brief デバッグフック型
 */
typedef enum {
  AEROJS_DEBUG_HOOK_STATEMENT = 0,  /* ステートメント実行時 */
  AEROJS_DEBUG_HOOK_FUNCTION_ENTRY, /* 関数開始時 */
  AEROJS_DEBUG_HOOK_FUNCTION_EXIT,  /* 関数終了時 */
  AEROJS_DEBUG_HOOK_EXCEPTION       /* 例外発生時 */
} AerojsDebugHookType;

/**
 * @brief デバッグ情報
 */
typedef struct {
  const char* sourceFile;    /* ソースファイル名（NULL可能） */
  AerojsUInt32 lineNumber;   /* 行番号 */
  AerojsUInt32 columnNumber; /* 列番号 */
  const char* functionName;  /* 関数名（NULL可能） */
  AerojsValueRef exception;  /* 例外オブジェクト（例外発生時のみ、それ以外はNULL） */
} AerojsDebugInfo;

/**
 * @brief デバッグフックコールバック型
 *
 * @param engine エンジンインスタンス
 * @param ctx 実行コンテキスト
 * @param hookType フックタイプ
 * @param debugInfo デバッグ情報
 * @param userData ユーザーデータ
 */
typedef void (*AerojsDebugHookCallback)(AerojsEngine* engine,
                                        AerojsContext* ctx,
                                        AerojsDebugHookType hookType,
                                        const AerojsDebugInfo* debugInfo,
                                        void* userData);

/**
 * @brief デバッグフックを設定
 * @param engine エンジンインスタンス
 * @param hookType フックタイプ
 * @param callback コールバック関数
 * @param userData ユーザーデータ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetDebugHook(AerojsEngine* engine,
                                              AerojsDebugHookType hookType,
                                              AerojsDebugHookCallback callback,
                                              void* userData);

/**
 * @brief エンジンにカスタムデータを関連付ける
 * @param engine エンジンインスタンス
 * @param key キー
 * @param data データポインタ
 * @param destructor データ破棄用コールバック（NULL可）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetEngineData(AerojsEngine* engine,
                                               const char* key,
                                               void* data,
                                               void (*destructor)(void*));

/**
 * @brief エンジンからカスタムデータを取得
 * @param engine エンジンインスタンス
 * @param key キー
 * @return データポインタ、キーが存在しない場合はNULL
 */
AEROJS_EXPORT void* AerojsGetEngineData(AerojsEngine* engine, const char* key);

/**
 * @brief エンジンからカスタムデータを削除
 * @param engine エンジンインスタンス
 * @param key キー
 * @return 成功した場合はAEROJS_TRUE、キーが存在しない場合はAEROJS_FALSE
 */
AEROJS_EXPORT AerojsBool AerojsRemoveEngineData(AerojsEngine* engine, const char* key);

/**
 * @brief スクリプトキャッシュを有効/無効化
 * @param engine エンジンインスタンス
 * @param enable 有効にする場合はAEROJS_TRUE、無効にする場合はAEROJS_FALSE
 * @param cacheDir キャッシュディレクトリのパス（enableがAEROJS_TRUEの場合は必須）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsEnableScriptCache(AerojsEngine* engine,
                                                   AerojsBool enable,
                                                   const char* cacheDir);

/**
 * @brief キャッシュをクリア
 * @param engine エンジンインスタンス
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsClearScriptCache(AerojsEngine* engine);

/**
 * @brief スーパー最適化レベル
 */
typedef enum {
  AEROJS_SUPER_OPT_LEVEL0 = 0,   /* 基本最適化 */
  AEROJS_SUPER_OPT_LEVEL1 = 1,   /* 高度最適化 */
  AEROJS_SUPER_OPT_LEVEL2 = 2,   /* 超最適化 */
  AEROJS_SUPER_OPT_LEVEL3 = 3,   /* 究極最適化 */
  AEROJS_SUPER_OPT_EXTREME = 4   /* 極限最適化（実験的） */
} AerojsSuperOptLevel;

/**
 * @brief 超最適化レベルを設定
 * @param engine エンジンインスタンス
 * @param level 超最適化レベル
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetSuperOptimizationLevel(AerojsEngine* engine, AerojsSuperOptLevel level);

/**
 * @brief 超最適化レベルを取得
 * @param engine エンジンインスタンス
 * @param level 超最適化レベルを格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetSuperOptimizationLevel(AerojsEngine* engine, AerojsSuperOptLevel* level);

/**
 * @brief 並列コンパイルスレッド数を設定
 * @param engine エンジンインスタンス
 * @param threads スレッド数（0=自動）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetCompilationThreads(AerojsEngine* engine, AerojsUInt32 threads);

/**
 * @brief ハードウェア固有の最適化を有効/無効化
 * @param engine エンジンインスタンス
 * @param enable 有効にする場合はAEROJS_TRUE、無効にする場合はAEROJS_FALSE
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsEnableHardwareOptimizations(AerojsEngine* engine, AerojsBool enable);

/**
 * @brief 拡張命令セットオプション
 */
typedef struct {
  AerojsBool useCryptoInstructions;      /* 暗号命令（AES, SHA） */
  AerojsBool useDotProductInstructions;  /* ドット積命令 */
  AerojsBool useBF16Instructions;        /* BF16命令 */
  AerojsBool useJSCVTInstructions;       /* JavaScript変換命令 */
  AerojsBool useLSEInstructions;         /* Large System Extensions */
  AerojsBool useSVEInstructions;         /* Scalable Vector Extensions */
  AerojsBool usePAuthInstructions;       /* ポインタ認証 */
  AerojsBool useBTIInstructions;         /* ブランチターゲット識別 */
  AerojsBool useMTEInstructions;         /* メモリタグ拡張 */
} AerojsAdvancedInstructionOptions;

/**
 * @brief 拡張命令セットオプションを設定
 * @param engine エンジンインスタンス
 * @param options オプション構造体
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetAdvancedInstructionOptions(AerojsEngine* engine, 
                                                           const AerojsAdvancedInstructionOptions* options);

/**
 * @brief 拡張命令セットオプションを取得
 * @param engine エンジンインスタンス
 * @param options オプション構造体を格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetAdvancedInstructionOptions(AerojsEngine* engine, 
                                                           AerojsAdvancedInstructionOptions* options);

/**
 * @brief メタトレーシング最適化を有効/無効化
 * @param engine エンジンインスタンス
 * @param enable 有効にする場合はAEROJS_TRUE、無効にする場合はAEROJS_FALSE
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsEnableMetaTracing(AerojsEngine* engine, AerojsBool enable);

/**
 * @brief スペキュレーティブ最適化を有効/無効化
 * @param engine エンジンインスタンス
 * @param enable 有効にする場合はAEROJS_TRUE、無効にする場合はAEROJS_FALSE
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsEnableSpeculativeOptimizations(AerojsEngine* engine, AerojsBool enable);

/**
 * @brief プロファイル誘導型最適化を有効/無効化
 * @param engine エンジンインスタンス
 * @param enable 有効にする場合はAEROJS_TRUE、無効にする場合はAEROJS_FALSE
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsEnableProfileGuidedOptimization(AerojsEngine* engine, AerojsBool enable);

/**
 * @brief JITコードキャッシュの最大サイズを設定
 * @param engine エンジンインスタンス
 * @param maxSize 最大サイズ（バイト単位）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetMaxCodeCacheSize(AerojsEngine* engine, AerojsSize maxSize);

/**
 * @brief パフォーマンスチューニングを自動実行
 * @param engine エンジンインスタンス
 * @param timeoutMs タイムアウト時間（ミリ秒、0=無制限）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsAutoTunePerformance(AerojsEngine* engine, AerojsUInt32 timeoutMs);

/**
 * @brief ホットスポットの最適化を実行
 * @param engine エンジンインスタンス
 * @param async 非同期実行する場合はAEROJS_TRUE、同期実行する場合はAEROJS_FALSE
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsOptimizeHotspots(AerojsEngine* engine, AerojsBool async);

/**
 * @brief JITコンパイラの詳細なデバッグ情報を取得
 * @param engine エンジンインスタンス
 * @param functionName 関数名（NULL=すべての関数）
 * @param buffer 情報を格納するバッファ
 * @param bufferSize バッファサイズ
 * @param actualSize 実際のサイズを格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetJITDebugInfo(AerojsEngine* engine, 
                                                const char* functionName,
                                                char* buffer, 
                                                AerojsSize bufferSize,
                                                AerojsSize* actualSize);

/**
 * @brief コンパイル済み関数の逆アセンブリコードを取得
 * @param engine エンジンインスタンス
 * @param functionName 関数名
 * @param buffer 情報を格納するバッファ
 * @param bufferSize バッファサイズ
 * @param actualSize 実際のサイズを格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsDisassembleFunction(AerojsEngine* engine, 
                                                   const char* functionName,
                                                   char* buffer, 
                                                   AerojsSize bufferSize,
                                                   AerojsSize* actualSize);

#ifdef __cplusplus
}
#endif

#endif /* AEROJS_ENGINE_H */