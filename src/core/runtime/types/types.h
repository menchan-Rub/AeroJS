/**
 * @file types.h
 * @brief JavaScriptの型システムの定義
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#ifndef AERO_CORE_RUNTIME_TYPES_H
#define AERO_CORE_RUNTIME_TYPES_H

#include <cstdint>
#include <memory>
#include <string>

#include "../context/context.h"
#include "value_type.h"

namespace aero {

/**
 * @brief 値の型を判定するユーティリティ関数
 */
class TypeChecking {
 public:
  /**
   * @brief 値がundefinedかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return undefinedの場合はtrue、それ以外の場合はfalse
   */
  static bool isUndefined(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がnullかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return nullの場合はtrue、それ以外の場合はfalse
   */
  static bool isNull(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がnullまたはundefinedかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return nullまたはundefinedの場合はtrue、それ以外の場合はfalse
   */
  static bool isNullOrUndefined(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がブール値かどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return ブール値の場合はtrue、それ以外の場合はfalse
   */
  static bool isBoolean(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値が数値かどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 数値の場合はtrue、それ以外の場合はfalse
   */
  static bool isNumber(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値が整数かどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 整数の場合はtrue、それ以外の場合はfalse
   */
  static bool isInteger(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値が文字列かどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 文字列の場合はtrue、それ以外の場合はfalse
   */
  static bool isString(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がシンボルかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return シンボルの場合はtrue、それ以外の場合はfalse
   */
  static bool isSymbol(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がBigIntかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return BigIntの場合はtrue、それ以外の場合はfalse
   */
  static bool isBigInt(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がオブジェクトかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return オブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isObject(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値が関数かどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 関数の場合はtrue、それ以外の場合はfalse
   */
  static bool isFunction(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値が配列かどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 配列の場合はtrue、それ以外の場合はfalse
   */
  static bool isArray(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値が日付オブジェクトかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 日付オブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isDate(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値が正規表現オブジェクトかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 正規表現オブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isRegExp(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がエラーオブジェクトかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return エラーオブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isError(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がMapオブジェクトかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return Mapオブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isMap(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がSetオブジェクトかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return Setオブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isSet(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がWeakMapオブジェクトかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return WeakMapオブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isWeakMap(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がWeakSetオブジェクトかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return WeakSetオブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isWeakSet(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がArrayBufferオブジェクトかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return ArrayBufferオブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isArrayBuffer(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がSharedArrayBufferオブジェクトかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return SharedArrayBufferオブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isSharedArrayBuffer(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がDataViewオブジェクトかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return DataViewオブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isDataView(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がTypedArrayオブジェクトかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return TypedArrayオブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isTypedArray(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がPromiseオブジェクトかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return Promiseオブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isPromise(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がProxyオブジェクトかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return Proxyオブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isProxy(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がコンストラクタかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return コンストラクタの場合はtrue、それ以外の場合はfalse
   */
  static bool isConstructor(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値が原始型（プリミティブ）かどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 原始型の場合はtrue、それ以外の場合はfalse
   */
  static bool isPrimitive(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値がイテレーブルかどうか判定
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return イテレーブルの場合はtrue、それ以外の場合はfalse
   */
  static bool isIterable(ExecutionContext* ctx, const Value& value);
};

/**
 * @brief 型変換ユーティリティ関数
 */
class TypeConversion {
 public:
  /**
   * @brief 値をブール値に変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後のブール値
   */
  static bool toBoolean(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値を数値に変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後の数値
   */
  static double toNumber(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値を整数に変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後の整数
   */
  static int64_t toInteger(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値を32ビット整数に変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後の32ビット整数
   */
  static int32_t toInt32(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値を符号なし32ビット整数に変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後の符号なし32ビット整数
   */
  static uint32_t toUint32(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値を16ビット整数に変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後の16ビット整数
   */
  static int16_t toInt16(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値を符号なし16ビット整数に変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後の符号なし16ビット整数
   */
  static uint16_t toUint16(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値を8ビット整数に変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後の8ビット整数
   */
  static int8_t toInt8(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値を符号なし8ビット整数に変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後の符号なし8ビット整数
   */
  static uint8_t toUint8(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値を符号なし8ビット整数（クランプ処理）に変換
   *
   * ECMAScript仕様に従った型変換を行います。
   * 結果が0未満の場合は0、255より大きい場合は255に丸められます。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後の符号なし8ビット整数（クランプ処理）
   */
  static uint8_t toUint8Clamp(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値を文字列に変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後の文字列
   */
  static std::string toString(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値をオブジェクトに変換
   *
   * ECMAScript仕様に従った型変換を行います。
   * nullまたはundefinedの場合は例外をスローします。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後のオブジェクト
   */
  static Object* toObject(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値を原始型（プリミティブ）に変換
   *
   * ECMAScript仕様に従った型変換を行います。
   * 既に原始型の場合はそのまま返します。
   * オブジェクトの場合はToPrimitiveアルゴリズムに従って変換します。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @param preferredType 変換の優先型ヒント（"default", "number", "string"）
   * @return 変換後の原始値
   */
  static Value toPrimitive(ExecutionContext* ctx, const Value& value, const std::string& preferredType = "default");

  /**
   * @brief 値をBigIntに変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後のBigInt
   */
  static BigInt* toBigInt(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値をBigInt64に変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後の64ビットBigInt
   */
  static int64_t toBigInt64(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値をBigUint64に変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後の符号なし64ビットBigInt
   */
  static uint64_t toBigUint64(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値をプロパティキーに変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後のプロパティキー
   */
  static Value toPropertyKey(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値を長さに変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後の長さ値
   */
  static uint32_t toLength(ExecutionContext* ctx, const Value& value);

  /**
   * @brief 値を配列インデックスに変換
   *
   * ECMAScript仕様に従った型変換を行います。
   *
   * @param ctx 実行コンテキスト
   * @param value 対象の値
   * @return 変換後の配列インデックス
   */
  static uint32_t toIndex(ExecutionContext* ctx, const Value& value);
};

/**
 * @brief 型比較ユーティリティ関数
 */
class TypeComparison {
 public:
  /**
   * @brief 値の等価性を比較（==演算子）
   *
   * ECMAScript仕様に従った緩い等価性比較を行います。
   *
   * @param ctx 実行コンテキスト
   * @param x 比較対象の値1
   * @param y 比較対象の値2
   * @return 等価の場合はtrue、そうでなければfalse
   */
  static bool abstractEquals(ExecutionContext* ctx, const Value& x, const Value& y);

  /**
   * @brief 値の厳密等価性を比較（===演算子）
   *
   * ECMAScript仕様に従った厳密等価性比較を行います。
   *
   * @param ctx 実行コンテキスト
   * @param x 比較対象の値1
   * @param y 比較対象の値2
   * @return 厳密に等価の場合はtrue、そうでなければfalse
   */
  static bool strictEquals(ExecutionContext* ctx, const Value& x, const Value& y);

  /**
   * @brief SameValue比較
   *
   * ECMAScript仕様に従ったSameValue比較を行います。
   * Object.is()メソッドの実装に使用されます。
   *
   * @param ctx 実行コンテキスト
   * @param x 比較対象の値1
   * @param y 比較対象の値2
   * @return 同じ値の場合はtrue、そうでなければfalse
   */
  static bool sameValue(ExecutionContext* ctx, const Value& x, const Value& y);

  /**
   * @brief SameValueZero比較
   *
   * ECMAScript仕様に従ったSameValueZero比較を行います。
   * Mapなどのキー比較に使用されます。
   *
   * @param ctx 実行コンテキスト
   * @param x 比較対象の値1
   * @param y 比較対象の値2
   * @return 同じ値の場合はtrue、そうでなければfalse
   */
  static bool sameValueZero(ExecutionContext* ctx, const Value& x, const Value& y);

  /**
   * @brief 値の大小を比較（<演算子）
   *
   * ECMAScript仕様に従った比較を行います。
   *
   * @param ctx 実行コンテキスト
   * @param x 比較対象の値1
   * @param y 比較対象の値2
   * @return x < yの場合はtrue、そうでなければfalse
   */
  static bool lessThan(ExecutionContext* ctx, const Value& x, const Value& y);

  /**
   * @brief 値の大小を比較（<=演算子）
   *
   * ECMAScript仕様に従った比較を行います。
   *
   * @param ctx 実行コンテキスト
   * @param x 比較対象の値1
   * @param y 比較対象の値2
   * @return x <= yの場合はtrue、そうでなければfalse
   */
  static bool lessThanOrEqual(ExecutionContext* ctx, const Value& x, const Value& y);

  /**
   * @brief 値の大小を比較（>演算子）
   *
   * ECMAScript仕様に従った比較を行います。
   *
   * @param ctx 実行コンテキスト
   * @param x 比較対象の値1
   * @param y 比較対象の値2
   * @return x > yの場合はtrue、そうでなければfalse
   */
  static bool greaterThan(ExecutionContext* ctx, const Value& x, const Value& y);

  /**
   * @brief 値の大小を比較（>=演算子）
   *
   * ECMAScript仕様に従った比較を行います。
   *
   * @param ctx 実行コンテキスト
   * @param x 比較対象の値1
   * @param y 比較対象の値2
   * @return x >= yの場合はtrue、そうでなければfalse
   */
  static bool greaterThanOrEqual(ExecutionContext* ctx, const Value& x, const Value& y);
};

/**
 * @brief 型システム初期化
 *
 * @param ctx 実行コンテキスト
 * @param globalObj グローバルオブジェクト
 */
void initializeTypeSystem(ExecutionContext* ctx, Object* globalObj);

}  // namespace aero

#endif  // AERO_CORE_RUNTIME_TYPES_H