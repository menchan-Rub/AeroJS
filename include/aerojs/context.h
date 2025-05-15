/**
 * @file context.h
 * @brief AeroJS 世界最高性能JavaScriptエンジンの実行コンテキスト管理API
 * @version 2.0.0
 * @license MIT
 */

#ifndef AEROJS_CONTEXT_H
#define AEROJS_CONTEXT_H

#include "aerojs.h"
#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief コンテキスト作成オプション
 */
typedef struct {
  AerojsSize maxStackSize;          /* 最大スタックサイズ（バイト単位） */
  AerojsBool enableExceptions;      /* 例外処理を有効にするかどうか */
  AerojsBool strictMode;            /* 厳格モードを有効にするかどうか */
  AerojsBool enableDebugger;        /* デバッガを有効にするかどうか */
  const char* timezone;             /* タイムゾーン設定 */
  const char* locale;               /* ロケール設定 */
  AerojsBool enableAsyncAwait;      /* async/awaitを有効にするかどうか */
  AerojsBool enableModules;         /* ES Modulesを有効にするかどうか */
  AerojsBool enableIntlAPI;         /* Intl APIを有効にするかどうか */
  AerojsBool enableBigIntAPI;       /* BigInt APIを有効にするかどうか */
  AerojsBool enableWeakRefs;        /* WeakRefsを有効にするかどうか */
  AerojsBool enableSharedArrayBuffer; /* SharedArrayBufferを有効にするかどうか */
  AerojsBool secureMode;            /* セキュアモードを有効にするかどうか */
  AerojsSize maxHeapSize;           /* コンテキスト最大ヒープサイズ（バイト単位） */
  AerojsUInt32 jitWarmupThreshold;  /* JITウォームアップのしきい値 */
  AerojsSize memoryLimit;           /* メモリ使用量制限（バイト単位） */
  AerojsBool enableSourceMaps;      /* ソースマップを有効にするかどうか */
  AerojsBool enablePrivateFields;   /* privateフィールドを有効にするかどうか */
  AerojsBool enableTopLevelAwait;   /* トップレベルawaitを有効にするかどうか */
  AerojsBool enableImportMeta;      /* import.metaを有効にするかどうか */
  AerojsBool enablePrivateMethods;  /* privateメソッドを有効にするかどうか */
  AerojsBool enableLogicalAssignment; /* 論理代入演算子を有効にするかどうか */
} AerojsContextOptions;

/**
 * @brief デフォルトのコンテキストオプションを取得
 * @param options 設定を格納する構造体へのポインタ
 */
AEROJS_EXPORT void AerojsGetDefaultContextOptions(AerojsContextOptions* options);

/**
 * @brief 新しいコンテキストを作成
 * @param engine エンジンインスタンス
 * @return 新しいコンテキストへのポインタ
 */
AEROJS_EXPORT AerojsContext* AerojsCreateContext(AerojsEngine* engine);

/**
 * @brief オプションを指定して新しいコンテキストを作成
 * @param engine エンジンインスタンス
 * @param options コンテキストオプション
 * @return 新しいコンテキストへのポインタ
 */
AEROJS_EXPORT AerojsContext* AerojsCreateContextWithOptions(AerojsEngine* engine,
                                                            const AerojsContextOptions* options);

/**
 * @brief コンテキストを破棄
 * @param ctx コンテキスト
 */
AEROJS_EXPORT void AerojsDestroyContext(AerojsContext* ctx);

/**
 * @brief コンテキストをリセット
 *
 * コンテキスト内のすべての変数と状態をクリアしますが、グローバルオブジェクトと組み込みオブジェクトは維持されます。
 *
 * @param ctx コンテキスト
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsResetContext(AerojsContext* ctx);

/**
 * @brief グローバルオブジェクトを取得
 * @param ctx コンテキスト
 * @return グローバルオブジェクトを表す値ハンドル
 */
AEROJS_EXPORT AerojsValueRef AerojsGetGlobalObject(AerojsContext* ctx);

/**
 * @brief コンテキストの有効性を確認
 * @param ctx コンテキスト
 * @return 有効な場合はAEROJS_TRUE、無効な場合はAEROJS_FALSE
 */
AEROJS_EXPORT AerojsBool AerojsContextIsValid(AerojsContext* ctx);

/**
 * @brief コンテキストの親エンジンを取得
 * @param ctx コンテキスト
 * @return 親エンジンのポインタ
 */
AEROJS_EXPORT AerojsEngine* AerojsContextGetEngine(AerojsContext* ctx);

/**
 * @brief 最後に発生した例外を取得
 * @param ctx コンテキスト
 * @return 例外オブジェクトを表す値ハンドル、例外が発生していない場合はNULL
 */
AEROJS_EXPORT AerojsValueRef AerojsGetLastException(AerojsContext* ctx);

/**
 * @brief 例外をクリア
 * @param ctx コンテキスト
 */
AEROJS_EXPORT void AerojsClearLastException(AerojsContext* ctx);

/**
 * @brief 例外を設定
 * @param ctx コンテキスト
 * @param exception 例外オブジェクト
 */
AEROJS_EXPORT void AerojsSetLastException(AerojsContext* ctx, AerojsValueRef exception);

/**
 * @brief 文字列からエラーオブジェクトを作成し、例外として設定
 * @param ctx コンテキスト
 * @param errorMessage エラーメッセージ
 */
AEROJS_EXPORT void AerojsSetErrorException(AerojsContext* ctx, const char* errorMessage);

/**
 * @brief 型エラーを例外として設定
 * @param ctx コンテキスト
 * @param errorMessage エラーメッセージ
 */
AEROJS_EXPORT void AerojsSetTypeErrorException(AerojsContext* ctx, const char* errorMessage);

/**
 * @brief 範囲外エラーを例外として設定
 * @param ctx コンテキスト
 * @param errorMessage エラーメッセージ
 */
AEROJS_EXPORT void AerojsSetRangeErrorException(AerojsContext* ctx, const char* errorMessage);

/**
 * @brief 構文エラーを例外として設定
 * @param ctx コンテキスト
 * @param errorMessage エラーメッセージ
 */
AEROJS_EXPORT void AerojsSetSyntaxErrorException(AerojsContext* ctx, const char* errorMessage);

/**
 * @brief プロキシが現在のコールスタックで例外をスローしているかどうかを確認
 * @param ctx コンテキスト
 * @return 例外がスローされている場合はAEROJS_TRUE、そうでない場合はAEROJS_FALSE
 */
AEROJS_EXPORT AerojsBool AerojsIsExceptionThrown(AerojsContext* ctx);

/**
 * @brief 詳細なスタックトレース情報を取得
 * @param ctx コンテキスト
 * @param exception 例外オブジェクト
 * @param buffer スタックトレースを格納するバッファ
 * @param bufferSize バッファサイズ
 * @param actualSize 実際のサイズを格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetDetailedStackTrace(AerojsContext* ctx, 
                                                      AerojsValueRef exception,
                                                      char* buffer, 
                                                      AerojsSize bufferSize,
                                                      AerojsSize* actualSize);

/**
 * @brief コールバック関数型
 *
 * JavaScriptから呼び出されるネイティブ関数の型定義
 *
 * @param ctx 実行コンテキスト
 * @param thisObject 'this'オブジェクト
 * @param arguments 引数の配列
 * @param argumentCount 引数の数
 * @return 戻り値
 */
typedef AerojsValueRef (*AerojsNativeFunction)(AerojsContext* ctx,
                                               AerojsValueRef thisObject,
                                               const AerojsValueRef* arguments,
                                               AerojsSize argumentCount);

/**
 * @brief ネイティブ関数を作成
 * @param ctx コンテキスト
 * @param name 関数名
 * @param function ネイティブ関数ポインタ
 * @param argumentCount 引数の数 (-1は可変長)
 * @return JavaScript関数オブジェクト
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateFunction(AerojsContext* ctx,
                                                  const char* name,
                                                  AerojsNativeFunction function,
                                                  AerojsInt32 argumentCount);

/**
 * @brief ネイティブ関数を作成（詳細設定）
 * @param ctx コンテキスト
 * @param name 関数名
 * @param function ネイティブ関数ポインタ
 * @param argumentCount 引数の数 (-1は可変長)
 * @param prototype プロトタイプオブジェクト（NULL可）
 * @param userData ユーザーデータ（NULL可）
 * @param finalizer ユーザーデータのファイナライザ（NULL可）
 * @return JavaScript関数オブジェクト
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateFunctionWithData(AerojsContext* ctx,
                                                         const char* name,
                                                         AerojsNativeFunction function,
                                                         AerojsInt32 argumentCount,
                                                         AerojsValueRef prototype,
                                                         void* userData,
                                                         void (*finalizer)(void*));

/**
 * @brief 関数からユーザーデータを取得
 * @param ctx コンテキスト
 * @param function 関数オブジェクト
 * @return ユーザーデータ
 */
AEROJS_EXPORT void* AerojsGetFunctionData(AerojsContext* ctx, AerojsValueRef function);

/**
 * @brief グローバルオブジェクトにネイティブ関数を登録
 * @param ctx コンテキスト
 * @param name 関数名
 * @param function ネイティブ関数ポインタ
 * @param argumentCount 引数の数 (-1は可変長)
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsRegisterGlobalFunction(AerojsContext* ctx,
                                                        const char* name,
                                                        AerojsNativeFunction function,
                                                        AerojsInt32 argumentCount);

/**
 * @brief グローバルオブジェクトに値を登録
 * @param ctx コンテキスト
 * @param name 変数名
 * @param value 設定する値
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsRegisterGlobalValue(AerojsContext* ctx,
                                                     const char* name,
                                                     AerojsValueRef value);

/**
 * @brief 名前空間にネイティブ関数を登録
 * @param ctx コンテキスト
 * @param namespace 名前空間オブジェクト
 * @param name 関数名
 * @param function ネイティブ関数ポインタ
 * @param argumentCount 引数の数 (-1は可変長)
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsRegisterNamespaceFunction(AerojsContext* ctx,
                                                          AerojsValueRef namespace,
                                                          const char* name,
                                                          AerojsNativeFunction function,
                                                          AerojsInt32 argumentCount);

/**
 * @brief スクリプト評価と実行
 */

/**
 * @brief 文字列をJavaScriptとして評価
 * @param ctx コンテキスト
 * @param script JavaScript文字列
 * @param sourceURL ソースURLまたはファイル名（デバッグ用、NULL可）
 * @param startLine 開始行番号（デバッグ用、0以上）
 * @param resultValue 結果値を格納するポインタ（NULL可）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsEvaluateScript(AerojsContext* ctx,
                                                const char* script,
                                                const char* sourceURL,
                                                AerojsInt32 startLine,
                                                AerojsValueRef* resultValue);

/**
 * @brief JavaScriptをコンパイルのみ実行（評価はしない）
 * @param ctx コンテキスト
 * @param script JavaScript文字列
 * @param sourceURL ソースURLまたはファイル名（デバッグ用、NULL可）
 * @param startLine 開始行番号（デバッグ用、0以上）
 * @param compiledScript コンパイル済みスクリプトを格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsCompileScript(AerojsContext* ctx,
                                              const char* script,
                                              const char* sourceURL,
                                              AerojsInt32 startLine,
                                              AerojsValueRef* compiledScript);

/**
 * @brief コンパイル済みスクリプトを評価
 * @param ctx コンテキスト
 * @param compiledScript コンパイル済みスクリプト
 * @param resultValue 結果値を格納するポインタ（NULL可）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsEvaluateCompiledScript(AerojsContext* ctx,
                                                       AerojsValueRef compiledScript,
                                                       AerojsValueRef* resultValue);

/**
 * @brief ファイルをJavaScriptとして評価
 * @param ctx コンテキスト
 * @param filePath ファイルパス
 * @param resultValue 結果値を格納するポインタ（NULL可）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsEvaluateScriptFile(AerojsContext* ctx,
                                                    const char* filePath,
                                                    AerojsValueRef* resultValue);

/**
 * @brief モジュールをインポート
 * @param ctx コンテキスト
 * @param moduleName モジュール名
 * @param resultValue 結果値を格納するポインタ（NULL可）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsImportModule(AerojsContext* ctx,
                                             const char* moduleName,
                                             AerojsValueRef* resultValue);

/**
 * @brief 非同期実行とPromise
 */

/**
 * @brief コールバック型：Promiseが解決されたとき
 */
typedef void (*AerojsPromiseResolveCallback)(AerojsContext* ctx, AerojsValueRef value, void* userData);

/**
 * @brief コールバック型：Promiseが拒否されたとき
 */
typedef void (*AerojsPromiseRejectCallback)(AerojsContext* ctx, AerojsValueRef reason, void* userData);

/**
 * @brief Promiseにコールバックを追加
 * @param ctx コンテキスト
 * @param promise Promiseオブジェクト
 * @param resolveCallback 解決時のコールバック
 * @param rejectCallback 拒否時のコールバック
 * @param userData ユーザーデータ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsPromiseThen(AerojsContext* ctx,
                                            AerojsValueRef promise,
                                            AerojsPromiseResolveCallback resolveCallback,
                                            AerojsPromiseRejectCallback rejectCallback,
                                            void* userData);

/**
 * @brief 関数呼び出し
 */

/**
 * @brief JavaScript関数を呼び出す
 * @param ctx コンテキスト
 * @param function 関数オブジェクト
 * @param thisObject 'this'オブジェクト（NULL可、その場合はグローバルオブジェクト）
 * @param arguments 引数の配列
 * @param argumentCount 引数の数
 * @param resultValue 結果値を格納するポインタ（NULL可）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsCallFunction(AerojsContext* ctx,
                                              AerojsValueRef function,
                                              AerojsValueRef thisObject,
                                              const AerojsValueRef* arguments,
                                              AerojsSize argumentCount,
                                              AerojsValueRef* resultValue);

/**
 * @brief オブジェクトのメソッドを呼び出す
 * @param ctx コンテキスト
 * @param object オブジェクト
 * @param methodName メソッド名
 * @param arguments 引数の配列
 * @param argumentCount 引数の数
 * @param resultValue 結果値を格納するポインタ（NULL可）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsCallMethod(AerojsContext* ctx,
                                            AerojsValueRef object,
                                            const char* methodName,
                                            const AerojsValueRef* arguments,
                                            AerojsSize argumentCount,
                                            AerojsValueRef* resultValue);

/**
 * @brief JavaScript関数を非同期で呼び出す
 * @param ctx コンテキスト
 * @param function 関数オブジェクト
 * @param thisObject 'this'オブジェクト（NULL可、その場合はグローバルオブジェクト）
 * @param arguments 引数の配列
 * @param argumentCount 引数の数
 * @return Promiseオブジェクト
 */
AEROJS_EXPORT AerojsValueRef AerojsCallFunctionAsync(AerojsContext* ctx,
                                                   AerojsValueRef function,
                                                   AerojsValueRef thisObject,
                                                   const AerojsValueRef* arguments,
                                                   AerojsSize argumentCount);

/**
 * @brief コンストラクタとしてJavaScript関数を呼び出す
 * @param ctx コンテキスト
 * @param constructor コンストラクタ関数
 * @param arguments 引数の配列
 * @param argumentCount 引数の数
 * @param resultValue 結果値を格納するポインタ（NULL可）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsCallConstructor(AerojsContext* ctx,
                                                AerojsValueRef constructor,
                                                const AerojsValueRef* arguments,
                                                AerojsSize argumentCount,
                                                AerojsValueRef* resultValue);

/**
 * @brief メモリ管理
 */

/**
 * @brief コンテキストのGCを実行
 * @param ctx コンテキスト
 */
AEROJS_EXPORT void AerojsCollectGarbage(AerojsContext* ctx);

/**
 * @brief コンテキストにカスタムデータを関連付ける
 * @param ctx コンテキスト
 * @param key キー
 * @param data データポインタ
 * @param destructor データ破棄用コールバック（NULL可）
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetContextData(AerojsContext* ctx,
                                                const char* key,
                                                void* data,
                                                void (*destructor)(void*));

/**
 * @brief コンテキストからカスタムデータを取得
 * @param ctx コンテキスト
 * @param key キー
 * @return データポインタ、キーが存在しない場合はNULL
 */
AEROJS_EXPORT void* AerojsGetContextData(AerojsContext* ctx, const char* key);

/**
 * @brief コンテキストからカスタムデータを削除
 * @param ctx コンテキスト
 * @param key キー
 * @return 成功した場合はAEROJS_TRUE、キーが存在しない場合はAEROJS_FALSE
 */
AEROJS_EXPORT AerojsBool AerojsRemoveContextData(AerojsContext* ctx, const char* key);

/**
 * @brief コンテキストの親エンジンを取得
 * @param ctx コンテキスト
 * @return 親エンジンのポインタ
 */
AEROJS_EXPORT AerojsEngine* AerojsGetContextEngine(AerojsContext* ctx);

/**
 * @brief コンテキストの現在のメモリ使用量を取得
 * @param ctx コンテキスト
 * @param usedBytes 使用バイト数を格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetContextMemoryUsage(AerojsContext* ctx, AerojsSize* usedBytes);

/**
 * @brief コンテキストの実行状態をスナップショット化
 * @param ctx コンテキスト
 * @param snapshotData スナップショットデータを格納するポインタ
 * @param snapshotSize スナップショットサイズを格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsCreateContextSnapshot(AerojsContext* ctx, void** snapshotData, AerojsSize* snapshotSize);

/**
 * @brief スナップショットからコンテキストを復元
 * @param engine エンジンインスタンス
 * @param snapshotData スナップショットデータ
 * @param snapshotSize スナップショットサイズ
 * @return 復元されたコンテキスト、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsContext* AerojsRestoreContextFromSnapshot(AerojsEngine* engine, const void* snapshotData, AerojsSize snapshotSize);

/**
 * @brief スナップショットデータを解放
 * @param snapshotData スナップショットデータ
 */
AEROJS_EXPORT void AerojsFreeContextSnapshot(void* snapshotData);

#ifdef __cplusplus
}
#endif

#endif /* AEROJS_CONTEXT_H */