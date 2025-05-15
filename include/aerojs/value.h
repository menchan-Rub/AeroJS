/**
 * @file value.h
 * @brief AeroJS 世界最高性能JavaScriptエンジンの値システムAPI
 * @version 2.0.0
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
  AEROJS_VALUE_TYPE_BIGINT,
  AEROJS_VALUE_TYPE_MAP,
  AEROJS_VALUE_TYPE_SET,
  AEROJS_VALUE_TYPE_WEAKMAP,
  AEROJS_VALUE_TYPE_WEAKSET,
  AEROJS_VALUE_TYPE_ARRAYBUFFER,
  AEROJS_VALUE_TYPE_DATAVIEW,
  AEROJS_VALUE_TYPE_TYPED_ARRAY,
  AEROJS_VALUE_TYPE_PROMISE,
  AEROJS_VALUE_TYPE_PROXY,
  AEROJS_VALUE_TYPE_WEAKREF,
  AEROJS_VALUE_TYPE_FINALIZATION_REGISTRY
} AerojsValueType;

/**
 * @brief 型付き配列の型を表す列挙型
 */
typedef enum {
  AEROJS_TYPED_ARRAY_INT8 = 0,
  AEROJS_TYPED_ARRAY_UINT8,
  AEROJS_TYPED_ARRAY_UINT8_CLAMPED,
  AEROJS_TYPED_ARRAY_INT16,
  AEROJS_TYPED_ARRAY_UINT16,
  AEROJS_TYPED_ARRAY_INT32,
  AEROJS_TYPED_ARRAY_UINT32,
  AEROJS_TYPED_ARRAY_FLOAT32,
  AEROJS_TYPED_ARRAY_FLOAT64,
  AEROJS_TYPED_ARRAY_BIGINT64,
  AEROJS_TYPED_ARRAY_BIGUINT64
} AerojsTypedArrayType;

/**
 * @brief JavaScript値ハンドル
 *
 * 不透明な値ハンドルで、JavaScriptエンジン内の値を参照します。
 * このハンドルはGCによって管理され、適切に解放する必要があります。
 */
typedef struct AerojsValue* AerojsValueRef;

/**
 * @brief 値の作成関数
 */

/** @brief undefined値を作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateUndefined(AerojsContext* ctx);

/** @brief null値を作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateNull(AerojsContext* ctx);

/** @brief 真偽値を作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateBoolean(AerojsContext* ctx, AerojsBool value);

/** @brief 数値を作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateNumber(AerojsContext* ctx, AerojsFloat64 value);

/** @brief 整数値を作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateInt32(AerojsContext* ctx, AerojsInt32 value);

/** @brief 文字列を作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateString(AerojsContext* ctx, const char* str);

/** @brief 指定した長さの文字列を作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateStringWithLength(AerojsContext* ctx, const char* str, AerojsSize length);

/** @brief シンボルを作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateSymbol(AerojsContext* ctx, const char* description);

/** @brief 空のオブジェクトを作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateObject(AerojsContext* ctx);

/** @brief プロトタイプを指定してオブジェクトを作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateObjectWithPrototype(AerojsContext* ctx, AerojsValueRef prototype);

/** @brief 空の配列を作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateArray(AerojsContext* ctx);

/** @brief 指定した長さの配列を作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateArrayWithLength(AerojsContext* ctx, AerojsSize length);

/** @brief 要素を指定して配列を作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateArrayFromElements(AerojsContext* ctx, const AerojsValueRef* elements, AerojsSize count);

/** @brief エラーオブジェクトを作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateError(AerojsContext* ctx, const char* message);

/** @brief 型を指定してエラーオブジェクトを作成 (SyntaxError, TypeError等) */
AEROJS_EXPORT AerojsValueRef AerojsCreateErrorWithType(AerojsContext* ctx, const char* errorType, const char* message);

/** @brief BigIntを作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateBigInt(AerojsContext* ctx, const char* value);

/** @brief 64ビット整数からBigIntを作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateBigIntFromInt64(AerojsContext* ctx, AerojsInt64 value);

/** @brief Map オブジェクトを作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateMap(AerojsContext* ctx);

/** @brief Set オブジェクトを作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateSet(AerojsContext* ctx);

/** @brief ArrayBuffer オブジェクトを作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateArrayBuffer(AerojsContext* ctx, AerojsSize byteLength);

/** @brief 外部メモリを使用した ArrayBuffer を作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateArrayBufferExternal(AerojsContext* ctx, void* data, AerojsSize byteLength, 
                                                             void (*freeCallback)(void* data, void* info), void* info);

/** @brief TypedArray を作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateTypedArray(AerojsContext* ctx, AerojsTypedArrayType type, 
                                                    AerojsValueRef buffer, AerojsSize byteOffset, AerojsSize length);

/** @brief Promise を作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreatePromise(AerojsContext* ctx, 
                                                 AerojsValueRef* resolveFunc, 
                                                 AerojsValueRef* rejectFunc);

/** @brief DataView を作成 */
AEROJS_EXPORT AerojsValueRef AerojsCreateDataView(AerojsContext* ctx, 
                                                 AerojsValueRef buffer, 
                                                 AerojsSize byteOffset, 
                                                 AerojsSize byteLength);

/**
 * @brief 高性能値変換
 */

/** @brief 値が特定の型かどうかを確認 (高速パス) */
AEROJS_EXPORT AerojsBool AerojsValueIsType(AerojsContext* ctx, AerojsValueRef value, AerojsValueType type);

/** @brief 値をブール値として取得 (高速パス) */
AEROJS_EXPORT AerojsBool AerojsValueToBoolean(AerojsContext* ctx, AerojsValueRef value);

/** @brief 値を数値として取得 (高速パス) */
AEROJS_EXPORT AerojsFloat64 AerojsValueToNumber(AerojsContext* ctx, AerojsValueRef value);

/** @brief 値を整数として取得 (高速パス) */
AEROJS_EXPORT AerojsInt32 AerojsValueToInt32(AerojsContext* ctx, AerojsValueRef value);

/** @brief 値を文字列として取得 */
AEROJS_EXPORT char* AerojsValueToString(AerojsContext* ctx, AerojsValueRef value);

/** @brief 値を文字列に変換し、バッファにコピー (ゼロコピー最適化) */
AEROJS_EXPORT AerojsSize AerojsValueStringCopy(AerojsContext* ctx, AerojsValueRef value,
                                               char* buffer, AerojsSize maxSize);

/** @brief 値の型を取得 */
AEROJS_EXPORT AerojsValueType AerojsGetValueType(AerojsContext* ctx, AerojsValueRef value);

/** @brief TypedArray の型を取得 */
AEROJS_EXPORT AerojsTypedArrayType AerojsGetTypedArrayType(AerojsContext* ctx, AerojsValueRef typedArray);

/** @brief 値の同値性を比較 */
AEROJS_EXPORT AerojsBool AerojsValueEquals(AerojsContext* ctx, AerojsValueRef a, AerojsValueRef b);

/** @brief 値の厳密な同値性を比較 (===) */
AEROJS_EXPORT AerojsBool AerojsValueStrictEquals(AerojsContext* ctx, AerojsValueRef a, AerojsValueRef b);

/**
 * @brief オブジェクト操作 (高性能実装)
 */

/** @brief オブジェクトのプロパティを取得 */
AEROJS_EXPORT AerojsValueRef AerojsObjectGetProperty(AerojsContext* ctx, AerojsValueRef object, const char* propertyName);

/** @brief オブジェクトのプロパティをインデックスを使って取得 (高速パス) */
AEROJS_EXPORT AerojsValueRef AerojsObjectGetPropertyAtIndex(AerojsContext* ctx, AerojsValueRef object, AerojsUInt32 index);

/** @brief オブジェクトのプロパティを設定 */
AEROJS_EXPORT AerojsStatus AerojsObjectSetProperty(AerojsContext* ctx, AerojsValueRef object,
                                                   const char* propertyName, AerojsValueRef value);

/** @brief オブジェクトのプロパティをインデックスを使って設定 (高速パス) */
AEROJS_EXPORT AerojsStatus AerojsObjectSetPropertyAtIndex(AerojsContext* ctx, AerojsValueRef object,
                                                         AerojsUInt32 index, AerojsValueRef value);

/** @brief オブジェクトのプロパティを詳細に設定 */
AEROJS_EXPORT AerojsStatus AerojsObjectDefineProperty(AerojsContext* ctx, AerojsValueRef object,
                                                     const char* propertyName, AerojsValueRef descriptor);

/** @brief オブジェクトのプロパティが存在するかを確認 */
AEROJS_EXPORT AerojsBool AerojsObjectHasProperty(AerojsContext* ctx, AerojsValueRef object, const char* propertyName);

/** @brief オブジェクトのプロパティを削除 */
AEROJS_EXPORT AerojsStatus AerojsObjectDeleteProperty(AerojsContext* ctx, AerojsValueRef object, const char* propertyName);

/** @brief オブジェクトの全プロパティ名を取得 */
AEROJS_EXPORT AerojsValueRef AerojsObjectGetPropertyNames(AerojsContext* ctx, AerojsValueRef object);

/** @brief オブジェクトの自身のプロパティのみを取得 */
AEROJS_EXPORT AerojsValueRef AerojsObjectGetOwnPropertyNames(AerojsContext* ctx, AerojsValueRef object);

/** @brief オブジェクトのプロトタイプを取得 */
AEROJS_EXPORT AerojsValueRef AerojsObjectGetPrototype(AerojsContext* ctx, AerojsValueRef object);

/** @brief オブジェクトのプロトタイプを設定 */
AEROJS_EXPORT AerojsStatus AerojsObjectSetPrototype(AerojsContext* ctx, AerojsValueRef object, AerojsValueRef prototype);

/**
 * @brief 値のGC管理
 */

/** @brief 値の参照をインクリメント (GC保護) */
AEROJS_EXPORT void AerojsValueProtect(AerojsContext* ctx, AerojsValueRef value);

/** @brief 値の参照をデクリメント (GC保護解除) */
AEROJS_EXPORT void AerojsValueUnprotect(AerojsContext* ctx, AerojsValueRef value);

/**
 * @brief 値の直接メモリアクセス（高性能操作用）
 */

/** @brief ArrayBuffer のバッキングストアを取得 */
AEROJS_EXPORT void* AerojsArrayBufferGetData(AerojsContext* ctx, AerojsValueRef arrayBuffer);

/** @brief ArrayBuffer のバイト長を取得 */
AEROJS_EXPORT AerojsSize AerojsArrayBufferGetByteLength(AerojsContext* ctx, AerojsValueRef arrayBuffer);

/** @brief TypedArray のバッキングストアを取得 */
AEROJS_EXPORT void* AerojsTypedArrayGetData(AerojsContext* ctx, AerojsValueRef typedArray);

/** @brief TypedArray の要素数を取得 */
AEROJS_EXPORT AerojsSize AerojsTypedArrayGetLength(AerojsContext* ctx, AerojsValueRef typedArray);

/** @brief TypedArray のバイト長を取得 */
AEROJS_EXPORT AerojsSize AerojsTypedArrayGetByteLength(AerojsContext* ctx, AerojsValueRef typedArray);

/** @brief TypedArray のバイトオフセットを取得 */
AEROJS_EXPORT AerojsSize AerojsTypedArrayGetByteOffset(AerojsContext* ctx, AerojsValueRef typedArray);

/** @brief 文字列の内部UTF-8データを取得（ゼロコピー） */
AEROJS_EXPORT const char* AerojsStringGetUTF8Data(AerojsContext* ctx, AerojsValueRef string, AerojsSize* length);

#ifdef __cplusplus
}
#endif

#endif /* AEROJS_VALUE_H */