/**
 * @file globals.h
 * @brief JavaScriptのグローバルオブジェクトと関数のヘッダファイル
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#ifndef AERO_RUNTIME_GLOBALS_H
#define AERO_RUNTIME_GLOBALS_H

#include <memory>
#include <string>
#include <vector>

#include "../context/context.h"
#include "../object.h"
#include "../values/value.h"

namespace aero {

/**
 * @brief @globalsオブジェクトを取得
 *
 * 指定された実行コンテキストの@globalsオブジェクトを取得します。
 *
 * @param ctx 実行コンテキスト
 * @return @globalsオブジェクト
 */
class GlobalsObject;
GlobalsObject* getGlobalsObject(ExecutionContext* ctx);

/**
 * @brief グローバル関数の初期化
 *
 * JavaScriptのグローバルスコープに存在する関数を初期化します。
 *
 * @param ctx 実行コンテキスト
 * @param globalObj グローバルオブジェクト
 */
void initializeGlobalFunctions(ExecutionContext* ctx, Object* globalObj);

/**
 * @brief eval関数の実装
 *
 * 文字列として渡されたJavaScriptコードを評価し、その結果を返します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (code: string)
 * @return 評価結果
 */
Value globalEval(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief isFinite関数の実装
 *
 * 値が有限数かどうかを判定します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (value)
 * @return 有限数の場合はtrue、それ以外の場合はfalse
 */
Value globalIsFinite(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief isNaN関数の実装
 *
 * 値がNaNかどうかを判定します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (value)
 * @return NaNの場合はtrue、それ以外の場合はfalse
 */
Value globalIsNaN(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief parseInt関数の実装
 *
 * 文字列を指定された基数の整数に変換します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (string, radix = 10)
 * @return 変換された整数値
 */
Value globalParseInt(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief parseFloat関数の実装
 *
 * 文字列を浮動小数点数に変換します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (string)
 * @return 変換された浮動小数点数値
 */
Value globalParseFloat(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief encodeURI関数の実装
 *
 * URIをエンコードします。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (uri: string)
 * @return エンコードされたURI
 */
Value globalEncodeURI(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief decodeURI関数の実装
 *
 * エンコードされたURIをデコードします。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (encodedURI: string)
 * @return デコードされたURI
 */
Value globalDecodeURI(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief encodeURIComponent関数の実装
 *
 * URI構成要素をエンコードします。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (component: string)
 * @return エンコードされたURI構成要素
 */
Value globalEncodeURIComponent(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief decodeURIComponent関数の実装
 *
 * エンコードされたURI構成要素をデコードします。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (encodedComponent: string)
 * @return デコードされたURI構成要素
 */
Value globalDecodeURIComponent(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief escape関数の実装 (非推奨)
 *
 * 文字列をURLエンコードします。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (string: string)
 * @return エンコードされた文字列
 */
Value globalEscape(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief unescape関数の実装 (非推奨)
 *
 * URLエンコードされた文字列をデコードします。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (string: string)
 * @return デコードされた文字列
 */
Value globalUnescape(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief JSON名前空間
 *
 * JSON操作のためのオブジェクトとメソッドを提供します。
 */
namespace json {

/**
 * @brief JSONオブジェクトを初期化
 *
 * グローバルオブジェクトにJSONオブジェクトを追加します。
 *
 * @param ctx 実行コンテキスト
 * @param globalObj グローバルオブジェクト
 * @return JSONオブジェクト
 */
Object* initialize(ExecutionContext* ctx, Object* globalObj);

/**
 * @brief JSON.parse関数の実装
 *
 * JSON文字列をパースし、JavaScript値に変換します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (text: string, reviver?: function)
 * @return パースされた値
 */
Value parse(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief JSON.stringify関数の実装
 *
 * JavaScript値をJSON文字列に変換します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (value, replacer?: function|array, space?: number|string)
 * @return JSON文字列
 */
Value stringify(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

}  // namespace json

/**
 * @brief Math名前空間
 *
 * 数学関数と定数を提供します。
 */
namespace math {

/**
 * @brief Mathオブジェクトを初期化
 *
 * グローバルオブジェクトにMathオブジェクトを追加します。
 *
 * @param ctx 実行コンテキスト
 * @param globalObj グローバルオブジェクト
 * @return Mathオブジェクト
 */
Object* initialize(ExecutionContext* ctx, Object* globalObj);

/**
 * @brief Math.abs関数の実装
 *
 * 数値の絶対値を返します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (x: number)
 * @return 絶対値
 */
Value abs(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Math.random関数の実装
 *
 * 0以上1未満の疑似乱数を生成します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (未使用)
 * @return 乱数
 */
Value random(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Math.max関数の実装
 *
 * 指定された値の中で最大の値を返します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (...values)
 * @return 最大値
 */
Value max(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Math.min関数の実装
 *
 * 指定された値の中で最小の値を返します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (...values)
 * @return 最小値
 */
Value min(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Math.floor関数の実装
 *
 * 指定された数値以下の最大の整数を返します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (x: number)
 * @return 切り捨てられた値
 */
Value floor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Math.ceil関数の実装
 *
 * 指定された数値以上の最小の整数を返します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (x: number)
 * @return 切り上げられた値
 */
Value ceil(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Math.round関数の実装
 *
 * 指定された数値を最も近い整数に丸めます。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (x: number)
 * @return 丸められた値
 */
Value round(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Math.pow関数の実装
 *
 * 基数を指定された指数で累乗した値を返します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (base: number, exponent: number)
 * @return 累乗された値
 */
Value pow(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Math.sqrt関数の実装
 *
 * 数値の平方根を返します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (x: number)
 * @return 平方根
 */
Value sqrt(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

}  // namespace math

/**
 * @brief Reflect名前空間
 *
 * オブジェクトの操作に使用するメソッドを提供します。
 */
namespace reflect {

/**
 * @brief Reflectオブジェクトを初期化
 *
 * グローバルオブジェクトにReflectオブジェクトを追加します。
 *
 * @param ctx 実行コンテキスト
 * @param globalObj グローバルオブジェクト
 * @return Reflectオブジェクト
 */
Object* initialize(ExecutionContext* ctx, Object* globalObj);

/**
 * @brief Reflect.apply関数の実装
 *
 * 指定された引数リストで関数を呼び出します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (target: function, thisArg, args: array)
 * @return 関数の戻り値
 */
Value apply(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.construct関数の実装
 *
 * 指定されたコンストラクタ関数と引数リストで新しいオブジェクトを作成します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (target: function, args: array, newTarget?: function)
 * @return 新しいオブジェクト
 */
Value construct(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.defineProperty関数の実装
 *
 * オブジェクトにプロパティを定義または変更します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (target: object, propertyKey: string|symbol, attributes: object)
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
Value defineProperty(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.deleteProperty関数の実装
 *
 * オブジェクトからプロパティを削除します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (target: object, propertyKey: string|symbol)
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
Value deleteProperty(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.get関数の実装
 *
 * オブジェクトのプロパティの値を取得します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (target: object, propertyKey: string|symbol, receiver?: object)
 * @return プロパティの値
 */
Value get(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.getOwnPropertyDescriptor関数の実装
 *
 * オブジェクトの指定されたプロパティの記述子を取得します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (target: object, propertyKey: string|symbol)
 * @return プロパティ記述子
 */
Value getOwnPropertyDescriptor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.has関数の実装
 *
 * 指定されたプロパティがオブジェクトに存在するかどうかを確認します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (target: object, propertyKey: string|symbol)
 * @return プロパティが存在する場合はtrue、それ以外の場合はfalse
 */
Value has(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.ownKeys関数の実装
 *
 * オブジェクトの全ての直接プロパティのキーの配列を返します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (target: object)
 * @return プロパティキーの配列
 */
Value ownKeys(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.preventExtensions関数の実装
 *
 * オブジェクトに新しいプロパティが追加されるのを防ぎます。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (target: object)
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
Value preventExtensions(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.set関数の実装
 *
 * オブジェクトのプロパティに値を設定します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (target: object, propertyKey: string|symbol, value, receiver?: object)
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
Value set(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Reflect.setPrototypeOf関数の実装
 *
 * オブジェクトのプロトタイプを変更します。
 *
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト (target: object, prototype: object|null)
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
Value setPrototypeOf(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

}  // namespace reflect

}  // namespace aero

#endif  // AERO_RUNTIME_GLOBALS_H