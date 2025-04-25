/**
 * @file reflect.h
 * @brief JavaScript の Reflect オブジェクトの定義
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#ifndef AERO_REFLECT_H
#define AERO_REFLECT_H

#include <vector>

#include "core/runtime/object.h"

namespace aero {

// 前方宣言
class ExecutionContext;
class Value;

/**
 * @brief Reflect.apply メソッド
 *
 * 指定した引数で、指定したthisを使用して関数を呼び出します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値（無視される）
 * @param args 引数リスト [target, thisArg, argumentsList]
 * @return 関数呼び出しの結果
 */
Value reflectApply(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.construct メソッド
 *
 * 指定した引数と、オプションのnewTargetでコンストラクタを呼び出します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値（無視される）
 * @param args 引数リスト [target, argumentsList, [newTarget]]
 * @return 新しく構築されたオブジェクト
 */
Value reflectConstruct(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.defineProperty メソッド
 *
 * オブジェクトにプロパティを定義します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値（無視される）
 * @param args 引数リスト [target, propertyKey, attributes]
 * @return 操作が成功した場合はtrue、そうでない場合はfalse
 */
Value reflectDefineProperty(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.deleteProperty メソッド
 *
 * オブジェクトから指定したプロパティを削除します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値（無視される）
 * @param args 引数リスト [target, propertyKey]
 * @return 操作が成功した場合はtrue、そうでない場合はfalse
 */
Value reflectDeleteProperty(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.get メソッド
 *
 * オブジェクトのプロパティを取得します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値（無視される）
 * @param args 引数リスト [target, propertyKey, [receiver]]
 * @return プロパティの値
 */
Value reflectGet(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.getOwnPropertyDescriptor メソッド
 *
 * オブジェクトの自身のプロパティディスクリプタを取得します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値（無視される）
 * @param args 引数リスト [target, propertyKey]
 * @return プロパティディスクリプタオブジェクト、またはundefined
 */
Value reflectGetOwnPropertyDescriptor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.getPrototypeOf メソッド
 *
 * オブジェクトのプロトタイプを取得します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値（無視される）
 * @param args 引数リスト [target]
 * @return オブジェクトのプロトタイプ、またはnull
 */
Value reflectGetPrototypeOf(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.has メソッド
 *
 * オブジェクトが指定したプロパティを持つかどうかを判定します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値（無視される）
 * @param args 引数リスト [target, propertyKey]
 * @return プロパティが存在する場合はtrue、そうでない場合はfalse
 */
Value reflectHas(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.isExtensible メソッド
 *
 * オブジェクトが拡張可能かどうかを判定します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値（無視される）
 * @param args 引数リスト [target]
 * @return オブジェクトが拡張可能な場合はtrue、そうでない場合はfalse
 */
Value reflectIsExtensible(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.ownKeys メソッド
 *
 * オブジェクトの自身のプロパティキーの配列を返します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値（無視される）
 * @param args 引数リスト [target]
 * @return プロパティキーの配列
 */
Value reflectOwnKeys(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.preventExtensions メソッド
 *
 * オブジェクトの拡張を禁止します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値（無視される）
 * @param args 引数リスト [target]
 * @return 操作が成功した場合はtrue、そうでない場合はfalse
 */
Value reflectPreventExtensions(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.set メソッド
 *
 * オブジェクトのプロパティを設定します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値（無視される）
 * @param args 引数リスト [target, propertyKey, value, [receiver]]
 * @return 操作が成功した場合はtrue、そうでない場合はfalse
 */
Value reflectSet(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.setPrototypeOf メソッド
 *
 * オブジェクトのプロトタイプを設定します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値（無視される）
 * @param args 引数リスト [target, prototype]
 * @return 操作が成功した場合はtrue、そうでない場合はfalse
 */
Value reflectSetPrototypeOf(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflectオブジェクトをグローバルオブジェクトに登録します
 *
 * @param ctx 実行コンテキスト
 * @param global グローバルオブジェクト
 */
void registerReflectObject(ExecutionContext* ctx, Object* global);

}  // namespace aero

#endif  // AERO_REFLECT_H