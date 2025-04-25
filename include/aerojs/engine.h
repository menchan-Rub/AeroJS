/**
 * @file engine.h
 * @brief AeroJS JavaScriptエンジンを管理するためのAPI
 * @version 0.1.0
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

#ifdef __cplusplus
}
#endif

#endif /* AEROJS_ENGINE_H */