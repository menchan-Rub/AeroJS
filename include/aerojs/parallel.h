/**
 * @file parallel.h
 * @brief AeroJS 世界最高性能JavaScriptエンジンの並列処理API
 * @version 2.0.0
 * @license MIT
 */

#ifndef AEROJS_PARALLEL_H
#define AEROJS_PARALLEL_H

#include "aerojs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 並列処理モード
 */
typedef enum {
  AEROJS_PARALLEL_MODE_NONE = 0,        /* 並列処理なし */
  AEROJS_PARALLEL_MODE_THREADS = 1,     /* スレッドベース並列処理 */
  AEROJS_PARALLEL_MODE_WORKERS = 2,     /* ワーカーベース並列処理 */
  AEROJS_PARALLEL_MODE_SIMD = 3,        /* SIMDベクトル並列処理 */
  AEROJS_PARALLEL_MODE_TASKS = 4,       /* タスクベース並列処理 */
  AEROJS_PARALLEL_MODE_HYBRID = 5,      /* ハイブリッド並列処理 */
  AEROJS_PARALLEL_MODE_AUTO = 6         /* 自動選択並列処理 */
} AerojsParallelMode;

/**
 * @brief 並列ワーカータイプ
 */
typedef enum {
  AEROJS_WORKER_TYPE_DEDICATED = 0,     /* 専用ワーカー */
  AEROJS_WORKER_TYPE_SHARED = 1,        /* 共有ワーカー */
  AEROJS_WORKER_TYPE_SERVICE = 2        /* サービスワーカー */
} AerojsWorkerType;

/**
 * @brief スレッドプール優先度
 */
typedef enum {
  AEROJS_THREAD_PRIORITY_LOW = 0,       /* 低優先度 */
  AEROJS_THREAD_PRIORITY_NORMAL = 1,    /* 通常優先度 */
  AEROJS_THREAD_PRIORITY_HIGH = 2,      /* 高優先度 */
  AEROJS_THREAD_PRIORITY_CRITICAL = 3   /* クリティカル優先度 */
} AerojsThreadPriority;

/**
 * @brief メモリ共有モデル
 */
typedef enum {
  AEROJS_MEMORY_MODEL_COPY = 0,         /* コピーベース */
  AEROJS_MEMORY_MODEL_SHARED = 1,       /* 共有メモリ */
  AEROJS_MEMORY_MODEL_MIXED = 2         /* 混合モデル */
} AerojsMemoryModel;

/**
 * @brief 並列処理設定
 */
typedef struct {
  AerojsParallelMode mode;              /* 並列処理モード */
  AerojsUInt32 maxThreads;              /* 最大スレッド数（0=自動） */
  AerojsMemoryModel memoryModel;        /* メモリ共有モデル */
  AerojsBool enableWorkStealing;        /* ワークスティーリングを有効にする */
  AerojsBool enableAutoBalancing;       /* 自動負荷分散を有効にする */
  AerojsUInt32 minWorkItemSize;         /* 最小ワークアイテムサイズ */
  AerojsUInt32 threadPoolSize;          /* スレッドプールサイズ */
  AerojsUInt32 taskQueueSize;           /* タスクキューサイズ */
  AerojsBool enablePinning;             /* スレッドピニングを有効にする */
  AerojsThreadPriority defaultPriority; /* デフォルトのスレッド優先度 */
  AerojsUInt32 prefetchDistance;        /* プリフェッチ距離 */
  AerojsBool enableVectorization;       /* ベクトル化を有効にする */
  AerojsBool enableHardwareAcceleration; /* ハードウェアアクセラレーションを有効にする */
  AerojsUInt32 schedulerQuantum;        /* スケジューラクォンタム（ミリ秒） */
  AerojsBool enableAdaptiveScheduling;  /* 適応的スケジューリングを有効にする */
  AerojsUInt32 lockFreeThreshold;       /* ロックフリーしきい値 */
  AerojsBool enableAsyncProcessing;     /* 非同期処理を有効にする */
  AerojsUInt32 maxPendingTasks;         /* 最大保留タスク数 */
  AerojsBool enablePowerSaving;         /* 省電力モードを有効にする */
  AerojsBool enableAutomaticSIMD;       /* 自動SIMD最適化を有効にする */
  AerojsUInt32 workerIsolationLevel;    /* ワーカー分離レベル（0-3） */
  AerojsUInt32 dataPartitionStrategy;   /* データ分割戦略（0-3） */
} AerojsParallelConfig;

/**
 * @brief 並列ワーカーハンドル
 */
typedef struct AerojsWorker* AerojsWorkerRef;

/**
 * @brief 並列タスクハンドル
 */
typedef struct AerojsTask* AerojsTaskRef;

/**
 * @brief タスクスケジューラハンドル
 */
typedef struct AerojsTaskScheduler* AerojsTaskSchedulerRef;

/**
 * @brief スレッドプールハンドル
 */
typedef struct AerojsThreadPool* AerojsThreadPoolRef;

/**
 * @brief 並列処理統計情報
 */
typedef struct {
  AerojsUInt32 activeThreads;           /* アクティブスレッド数 */
  AerojsUInt32 pendingTasks;            /* 保留中タスク数 */
  AerojsUInt32 completedTasks;          /* 完了したタスク数 */
  AerojsUInt32 canceledTasks;           /* キャンセルされたタスク数 */
  AerojsUInt32 failedTasks;             /* 失敗したタスク数 */
  AerojsUInt64 totalTaskTime;           /* 総タスク実行時間（ナノ秒） */
  AerojsUInt64 avgTaskTime;             /* 平均タスク実行時間（ナノ秒） */
  AerojsUInt32 threadPoolUtilization;   /* スレッドプール使用率（0-100%） */
  AerojsUInt32 lockContentions;         /* ロックコンテンション数 */
  AerojsUInt32 cacheLineConflicts;      /* キャッシュライン競合数 */
  AerojsUInt64 totalDataTransferred;    /* 転送されたデータ量（バイト） */
  AerojsUInt32 loadImbalance;           /* 負荷不均衡度（0-100%） */
  AerojsUInt32 workerCount;             /* ワーカー数 */
  AerojsUInt32 maxParallelism;          /* 最大並列度 */
  AerojsUInt64 schedulingOverhead;      /* スケジューリングオーバーヘッド（ナノ秒） */
  AerojsUInt32 simdUtilization;         /* SIMD使用率（0-100%） */
  AerojsUInt32 taskTheftCount;          /* タスク強奪数 */
  AerojsUInt32 dynamicTaskCount;        /* 動的生成タスク数 */
  AerojsUInt32 contextSwitches;         /* コンテキストスイッチ数 */
} AerojsParallelStats;

/**
 * @brief 並列処理コールバック関数
 *
 * @param userData ユーザーデータ
 * @param threadIndex スレッドインデックス
 * @param threadCount スレッド総数
 */
typedef void (*AerojsParallelCallback)(void* userData, AerojsUInt32 threadIndex, AerojsUInt32 threadCount);

/**
 * @brief ワーカーメッセージハンドラ
 *
 * @param worker ワーカーハンドル
 * @param message メッセージ
 * @param userData ユーザーデータ
 */
typedef void (*AerojsWorkerMessageHandler)(AerojsWorkerRef worker, AerojsValueRef message, void* userData);

/**
 * @brief タスク完了コールバック
 *
 * @param taskId タスクID
 * @param result 結果
 * @param userData ユーザーデータ
 */
typedef void (*AerojsTaskCompletionCallback)(AerojsUInt32 taskId, AerojsValueRef result, void* userData);

/**
 * @brief タスク実行関数
 *
 * @param ctx コンテキスト
 * @param taskId タスクID
 * @param parameters パラメータ
 * @param userData ユーザーデータ
 * @return 結果値
 */
typedef AerojsValueRef (*AerojsTaskFunction)(AerojsContext* ctx, AerojsUInt32 taskId, AerojsValueRef parameters, void* userData);

/**
 * @brief 並列処理エンジンを初期化
 *
 * @param engine エンジンインスタンス
 * @param config 並列処理設定
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsInitializeParallel(AerojsEngine* engine, const AerojsParallelConfig* config);

/**
 * @brief 並列処理エンジンをシャットダウン
 *
 * @param engine エンジンインスタンス
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsShutdownParallel(AerojsEngine* engine);

/**
 * @brief 並列処理エンジンの統計を取得
 *
 * @param engine エンジンインスタンス
 * @param stats 統計情報を格納する構造体へのポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetParallelStats(AerojsEngine* engine, AerojsParallelStats* stats);

/**
 * @brief スレッドプールを作成
 *
 * @param engine エンジンインスタンス
 * @param threadCount スレッド数
 * @param priority 優先度
 * @return スレッドプールハンドル、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsThreadPoolRef AerojsCreateThreadPool(AerojsEngine* engine, 
                                                      AerojsUInt32 threadCount, 
                                                      AerojsThreadPriority priority);

/**
 * @brief スレッドプールを破棄
 *
 * @param pool スレッドプールハンドル
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsDestroyThreadPool(AerojsThreadPoolRef pool);

/**
 * @brief 並列ワーカーを作成
 *
 * @param ctx コンテキスト
 * @param scriptURL スクリプトURL
 * @param type ワーカータイプ
 * @return ワーカーハンドル、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsWorkerRef AerojsCreateWorker(AerojsContext* ctx,
                                              const char* scriptURL,
                                              AerojsWorkerType type);

/**
 * @brief ワーカーにメッセージを送信
 *
 * @param worker ワーカーハンドル
 * @param message メッセージ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsWorkerPostMessage(AerojsWorkerRef worker, AerojsValueRef message);

/**
 * @brief ワーカーメッセージハンドラを設定
 *
 * @param worker ワーカーハンドル
 * @param handler メッセージハンドラ
 * @param userData ユーザーデータ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsWorkerSetMessageHandler(AerojsWorkerRef worker,
                                                      AerojsWorkerMessageHandler handler,
                                                      void* userData);

/**
 * @brief ワーカーを終了
 *
 * @param worker ワーカーハンドル
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsWorkerTerminate(AerojsWorkerRef worker);

/**
 * @brief タスクスケジューラを作成
 *
 * @param engine エンジンインスタンス
 * @param threadPool スレッドプールハンドル
 * @return タスクスケジューラハンドル、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsTaskSchedulerRef AerojsCreateTaskScheduler(AerojsEngine* engine, AerojsThreadPoolRef threadPool);

/**
 * @brief タスクスケジューラを破棄
 *
 * @param scheduler タスクスケジューラハンドル
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsDestroyTaskScheduler(AerojsTaskSchedulerRef scheduler);

/**
 * @brief タスクをスケジュール
 *
 * @param scheduler タスクスケジューラハンドル
 * @param taskFunc タスク実行関数
 * @param ctx コンテキスト
 * @param parameters パラメータ
 * @param completionCallback 完了コールバック
 * @param userData ユーザーデータ
 * @return タスクID、失敗した場合は0
 */
AEROJS_EXPORT AerojsUInt32 AerojsScheduleTask(AerojsTaskSchedulerRef scheduler,
                                            AerojsTaskFunction taskFunc,
                                            AerojsContext* ctx,
                                            AerojsValueRef parameters,
                                            AerojsTaskCompletionCallback completionCallback,
                                            void* userData);

/**
 * @brief タスクをキャンセル
 *
 * @param scheduler タスクスケジューラハンドル
 * @param taskId タスクID
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsCancelTask(AerojsTaskSchedulerRef scheduler, AerojsUInt32 taskId);

/**
 * @brief タスク完了まで待機
 *
 * @param scheduler タスクスケジューラハンドル
 * @param taskId タスクID
 * @param timeoutMs タイムアウト（ミリ秒、0は無限）
 * @param result 結果値を格納するポインタ（NULL可）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsWaitForTask(AerojsTaskSchedulerRef scheduler,
                                          AerojsUInt32 taskId,
                                          AerojsUInt32 timeoutMs,
                                          AerojsValueRef* result);

/**
 * @brief すべてのタスク完了まで待機
 *
 * @param scheduler タスクスケジューラハンドル
 * @param timeoutMs タイムアウト（ミリ秒、0は無限）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsWaitForAllTasks(AerojsTaskSchedulerRef scheduler, AerojsUInt32 timeoutMs);

/**
 * @brief 配列を並列処理
 *
 * @param ctx コンテキスト
 * @param array 処理する配列
 * @param callback 並列コールバック関数
 * @param userData ユーザーデータ
 * @param threadCount スレッド数（0=自動）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsParallelForEach(AerojsContext* ctx,
                                               AerojsValueRef array,
                                               AerojsParallelCallback callback,
                                               void* userData,
                                               AerojsUInt32 threadCount);

/**
 * @brief 並列処理設定を取得
 *
 * @param engine エンジンインスタンス
 * @param config 設定を格納する構造体へのポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetParallelConfig(AerojsEngine* engine, AerojsParallelConfig* config);

/**
 * @brief 並列処理設定を更新
 *
 * @param engine エンジンインスタンス
 * @param config 設定構造体
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsUpdateParallelConfig(AerojsEngine* engine, const AerojsParallelConfig* config);

/**
 * @brief 共有メモリを作成
 *
 * @param ctx コンテキスト
 * @param sizeBytes サイズ（バイト）
 * @return 共有メモリオブジェクト、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateSharedMemory(AerojsContext* ctx, AerojsSize sizeBytes);

/**
 * @brief 共有配列を作成
 *
 * @param ctx コンテキスト
 * @param elementType 要素タイプ
 * @param length 配列長
 * @return 共有配列オブジェクト、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateSharedArray(AerojsContext* ctx, AerojsUInt32 elementType, AerojsSize length);

/**
 * @brief アトミックな操作を実行
 *
 * @param ctx コンテキスト
 * @param sharedArray 共有配列
 * @param index インデックス
 * @param operation 操作タイプ
 * @param value 操作値
 * @param result 結果値を格納するポインタ（NULL可）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsAtomicOperation(AerojsContext* ctx,
                                               AerojsValueRef sharedArray,
                                               AerojsUInt32 index,
                                               AerojsUInt32 operation,
                                               AerojsValueRef value,
                                               AerojsValueRef* result);

/**
 * @brief 並列処理能力を検出
 *
 * @param engine エンジンインスタンス
 * @param parallelCapabilities 並列処理能力を格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsDetectParallelCapabilities(AerojsEngine* engine, AerojsUInt32* parallelCapabilities);

/**
 * @brief 最適な並列度を検出
 *
 * @param engine エンジンインスタンス
 * @param workloadType ワークロードタイプ（0-5）
 * @param optimalThreads 最適スレッド数を格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsDetectOptimalParallelism(AerojsEngine* engine, 
                                                       AerojsUInt32 workloadType, 
                                                       AerojsUInt32* optimalThreads);

#ifdef __cplusplus
}
#endif

#endif /* AEROJS_PARALLEL_H */ 