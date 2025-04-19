/**
 * @file context.h
 * @brief AeroJS JavaScript実行コンテキストを管理するためのAPI
 * @version 0.1.0
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

#ifdef __cplusplus
}
#endif

#endif /* AEROJS_CONTEXT_H */ 