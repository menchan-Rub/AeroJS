/**
 * @file value.h
 * @brief AeroJS JavaScript値を操作するためのAPI
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_VALUE_H
#define AEROJS_VALUE_H

#include "aerojs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief JavaScript値の型を表す列挙型
 */
typedef enum {
  AEROJS_VALUE_TYPE_UNDEFINED = 0,
  AEROJS_VALUE_TYPE_NULL,
  AEROJS_VALUE_TYPE_BOOLEAN,
  AEROJS_VALUE_TYPE_NUMBER,
  AEROJS_VALUE_TYPE_STRING,
  AEROJS_VALUE_TYPE_SYMBOL,
  AEROJS_VALUE_TYPE_OBJECT,
  AEROJS_VALUE_TYPE_FUNCTION,
  AEROJS_VALUE_TYPE_ARRAY,
  AEROJS_VALUE_TYPE_DATE,
  AEROJS_VALUE_TYPE_REGEXP,
  AEROJS_VALUE_TYPE_ERROR,
  AEROJS_VALUE_TYPE_BIGINT
} AerojsValueType;

/**
 * @brief JavaScript値ハンドル
 *
 * 不透明な値ハンドルで、JavaScriptエンジン内の値を参照します。
 * このハンドルはGCによって管理され、適切に解放する必要があります。
 */
typedef struct AerojsValue* AerojsValueRef;

/**
 * @brief AerojsValue作成関数
 */

/**
 * @brief undefined値を作成
 * @param ctx 実行コンテキスト
 * @return undefined値を表すハンドル
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateUndefined(AerojsContext* ctx);

/**
 * @brief null値を作成
 * @param ctx 実行コンテキスト
 * @return null値を表すハンドル
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateNull(AerojsContext* ctx);

/**
 * @brief 真偽値を作成
 * @param ctx 実行コンテキスト
 * @param value 真偽値
 * @return 真偽値を表すハンドル
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateBoolean(AerojsContext* ctx, AerojsBool value);

/**
 * @brief 数値を作成
 * @param ctx 実行コンテキスト
 * @param value 数値
 * @return 数値を表すハンドル
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateNumber(AerojsContext* ctx, AerojsFloat64 value);

/**
 * @brief 整数値を作成
 * @param ctx 実行コンテキスト
 * @param value 整数値
 * @return 整数値を表すハンドル
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateInt32(AerojsContext* ctx, AerojsInt32 value);

/**
 * @brief 文字列を作成
 * @param ctx 実行コンテキスト
 * @param str 文字列 (UTF-8でエンコードされたヌル終端文字列)
 * @return 文字列を表すハンドル
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateString(AerojsContext* ctx, const char* str);

/**
 * @brief 指定した長さの文字列を作成
 * @param ctx 実行コンテキスト
 * @param str 文字列 (UTF-8でエンコードされた文字列)
 * @param length 文字列の長さ (バイト単位)
 * @return 文字列を表すハンドル
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateStringWithLength(AerojsContext* ctx, const char* str, AerojsSize length);

/**
 * @brief シンボルを作成
 * @param ctx 実行コンテキスト
 * @param description シンボルの説明 (オプション、NULLの場合は説明なし)
 * @return シンボルを表すハンドル
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateSymbol(AerojsContext* ctx, const char* description);

/**
 * @brief 空のオブジェクトを作成
 * @param ctx 実行コンテキスト
 * @return オブジェクトを表すハンドル
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateObject(AerojsContext* ctx);

/**
 * @brief 空の配列を作成
 * @param ctx 実行コンテキスト
 * @return 配列を表すハンドル
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateArray(AerojsContext* ctx);

/**
 * @brief 指定した長さの配列を作成
 * @param ctx 実行コンテキスト
 * @param length 配列の長さ
 * @return 配列を表すハンドル
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateArrayWithLength(AerojsContext* ctx, AerojsSize length);

/**
 * @brief エラーオブジェクトを作成
 * @param ctx 実行コンテキスト
 * @param message エラーメッセージ
 * @return エラーオブジェクトを表すハンドル
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateError(AerojsContext* ctx, const char* message);

/**
 * @brief BigIntを作成
 * @param ctx 実行コンテキスト
 * @param value BigInt値を表す文字列
 * @return BigIntを表すハンドル
 */
AEROJS_EXPORT AerojsValueRef AerojsCreateBigInt(AerojsContext* ctx, const char* value);

/**
 * @brief 値の型を取得
 * @param ctx 実行コンテキスト
 * @param value 値ハンドル
 * @return 値の型
 */
AEROJS_EXPORT AerojsValueType AerojsGetValueType(AerojsContext* ctx, AerojsValueRef value);

/**
 * @brief 値が特定の型かどうかを確認
 * @param ctx 実行コンテキスト
 * @param value 値ハンドル
 * @param type 確認する型
 * @return 指定した型であればAEROJS_TRUE、そうでなければAEROJS_FALSE
 */
AEROJS_EXPORT AerojsBool AerojsValueIsType(AerojsContext* ctx, AerojsValueRef value, AerojsValueType type);

/**
 * @brief 値をブール値として取得
 * @param ctx 実行コンテキスト
 * @param value 値ハンドル
 * @return ブール値
 */
AEROJS_EXPORT AerojsBool AerojsValueToBoolean(AerojsContext* ctx, AerojsValueRef value);

/**
 * @brief 値を数値として取得
 * @param ctx 実行コンテキスト
 * @param value 値ハンドル
 * @return 数値
 */
AEROJS_EXPORT AerojsFloat64 AerojsValueToNumber(AerojsContext* ctx, AerojsValueRef value);

/**
 * @brief 値を整数として取得
 * @param ctx 実行コンテキスト
 * @param value 値ハンドル
 * @return 整数値
 */
AEROJS_EXPORT AerojsInt32 AerojsValueToInt32(AerojsContext* ctx, AerojsValueRef value);

/**
 * @brief 値を文字列として取得
 * @param ctx 実行コンテキスト
 * @param value 値ハンドル
 * @return 文字列 (呼び出し側で解放する必要がある)
 */
AEROJS_EXPORT char* AerojsValueToString(AerojsContext* ctx, AerojsValueRef value);

/**
 * @brief 値の文字列表現を指定したバッファにコピー
 * @param ctx 実行コンテキスト
 * @param value 値ハンドル
 * @param buffer 出力バッファ
 * @param maxSize バッファサイズ
 * @return コピーされたバイト数 (終端のNULLバイトを含む)
 */
AEROJS_EXPORT AerojsSize AerojsValueStringCopy(AerojsContext* ctx, AerojsValueRef value,
                                               char* buffer, AerojsSize maxSize);

/**
 * @brief 値の同値性を比較
 * @param ctx 実行コンテキスト
 * @param a 1つ目の値
 * @param b 2つ目の値
 * @return 同値であればAEROJS_TRUE、そうでなければAEROJS_FALSE
 */
AEROJS_EXPORT AerojsBool AerojsValueEquals(AerojsContext* ctx, AerojsValueRef a, AerojsValueRef b);

/**
 * @brief 値の厳密な同値性を比較 (===)
 * @param ctx 実行コンテキスト
 * @param a 1つ目の値
 * @param b 2つ目の値
 * @return 厳密に同値であればAEROJS_TRUE、そうでなければAEROJS_FALSE
 */
AEROJS_EXPORT AerojsBool AerojsValueStrictEquals(AerojsContext* ctx, AerojsValueRef a, AerojsValueRef b);

/**
 * @brief オブジェクトのプロパティを取得
 * @param ctx 実行コンテキスト
 * @param object オブジェクト
 * @param propertyName プロパティ名
 * @return プロパティ値
 */
AEROJS_EXPORT AerojsValueRef AerojsObjectGetProperty(AerojsContext* ctx, AerojsValueRef object, const char* propertyName);

/**
 * @brief オブジェクトのプロパティを設定
 * @param ctx 実行コンテキスト
 * @param object オブジェクト
 * @param propertyName プロパティ名
 * @param value 設定する値
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsObjectSetProperty(AerojsContext* ctx, AerojsValueRef object,
                                                   const char* propertyName, AerojsValueRef value);

/**
 * @brief 値の参照をインクリメント (GC保護)
 * @param ctx 実行コンテキスト
 * @param value 値ハンドル
 */
AEROJS_EXPORT void AerojsValueProtect(AerojsContext* ctx, AerojsValueRef value);

/**
 * @brief 値の参照をデクリメント (GC保護解除)
 * @param ctx 実行コンテキスト
 * @param value 値ハンドル
 */
AEROJS_EXPORT void AerojsValueUnprotect(AerojsContext* ctx, AerojsValueRef value);

#ifdef __cplusplus
}
#endif

#endif /* AEROJS_VALUE_H */