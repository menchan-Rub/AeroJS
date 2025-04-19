/**
 * @file iteration.h
 * @brief JavaScriptのイテレーションプロトコルの定義
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#ifndef AERO_RUNTIME_ITERATION_H
#define AERO_RUNTIME_ITERATION_H

#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include "../context/context.h"
#include "../values/value.h"
#include "../object.h"

namespace aero {

// 前方宣言
class Iterator;
class IteratorResult;
class GeneratorObject;
class AsyncIterator;

/**
 * @brief イテレーター結果オブジェクト
 * 
 * イテレーターのnextメソッドから返されるオブジェクト
 */
class IteratorResult {
public:
  /**
   * @brief イテレーター結果オブジェクトを作成
   * 
   * @param ctx 実行コンテキスト
   * @param value 値
   * @param done 完了フラグ
   * @return イテレーター結果オブジェクト
   */
  static Object* create(ExecutionContext* ctx, Value value, bool done);

  /**
   * @brief イテレーター結果オブジェクトをラップする
   * 
   * @param ctx 実行コンテキスト
   * @param obj オブジェクト
   * @param value 値（取得できない場合）
   * @param done 完了フラグ（取得できない場合）
   * @return イテレーター結果オブジェクト
   */
  static Object* wrap(ExecutionContext* ctx, Object* obj, Value value = Value::createUndefined(), bool done = false);

  /**
   * @brief オブジェクトがイテレーター結果オブジェクトかどうか判定
   * 
   * @param ctx 実行コンテキスト
   * @param obj オブジェクト
   * @return イテレーター結果オブジェクトの場合はtrue、それ以外の場合はfalse
   */
  static bool isIteratorResult(ExecutionContext* ctx, Object* obj);

  /**
   * @brief イテレーター結果オブジェクトから値を取得
   * 
   * @param ctx 実行コンテキスト
   * @param obj イテレーター結果オブジェクト
   * @return 値
   */
  static Value getValue(ExecutionContext* ctx, Object* obj);

  /**
   * @brief イテレーター結果オブジェクトから完了フラグを取得
   * 
   * @param ctx 実行コンテキスト
   * @param obj イテレーター結果オブジェクト
   * @return 完了フラグ
   */
  static bool getDone(ExecutionContext* ctx, Object* obj);
};

/**
 * @brief イテレーブルオブジェクト
 * 
 * Symbol.iteratorメソッドを持つオブジェクト
 */
class Iterable {
public:
  /**
   * @brief オブジェクトからイテレーターを取得
   * 
   * Symbol.iteratorメソッドを呼び出してイテレーターを取得します。
   * 
   * @param ctx 実行コンテキスト
   * @param obj オブジェクト
   * @return イテレーターオブジェクト
   */
  static Object* getIterator(ExecutionContext* ctx, Value obj);

  /**
   * @brief オブジェクトがイテレーブルかどうか判定
   * 
   * Symbol.iteratorメソッドを持つかどうかを確認します。
   * 
   * @param ctx 実行コンテキスト
   * @param obj オブジェクト
   * @return イテレーブルの場合はtrue、それ以外の場合はfalse
   */
  static bool isIterable(ExecutionContext* ctx, Value obj);

  /**
   * @brief 非同期イテレーターを取得
   * 
   * Symbol.asyncIteratorメソッドを呼び出して非同期イテレーターを取得します。
   * 
   * @param ctx 実行コンテキスト
   * @param obj オブジェクト
   * @return 非同期イテレーターオブジェクト
   */
  static Object* getAsyncIterator(ExecutionContext* ctx, Value obj);

  /**
   * @brief オブジェクトが非同期イテレーブルかどうか判定
   * 
   * Symbol.asyncIteratorメソッドを持つかどうかを確認します。
   * 
   * @param ctx 実行コンテキスト
   * @param obj オブジェクト
   * @return 非同期イテレーブルの場合はtrue、それ以外の場合はfalse
   */
  static bool isAsyncIterable(ExecutionContext* ctx, Value obj);
};

/**
 * @brief イテレーターオブジェクト
 * 
 * nextメソッドを持つオブジェクト
 */
class Iterator {
public:
  /**
   * @brief イテレーターオブジェクトを作成
   * 
   * @param ctx 実行コンテキスト
   * @param nextMethod next関数
   * @param returnMethod return関数（オプション）
   * @param throwMethod throw関数（オプション）
   * @return イテレーターオブジェクト
   */
  static Object* create(ExecutionContext* ctx, 
                        Function* nextMethod, 
                        Function* returnMethod = nullptr, 
                        Function* throwMethod = nullptr);

  /**
   * @brief イテレーターオブジェクトをラップする
   * 
   * @param ctx 実行コンテキスト
   * @param obj オブジェクト
   * @return イテレーターオブジェクト
   */
  static Object* wrap(ExecutionContext* ctx, Object* obj);

  /**
   * @brief オブジェクトがイテレーターかどうか判定
   * 
   * @param ctx 実行コンテキスト
   * @param obj オブジェクト
   * @return イテレーターの場合はtrue、それ以外の場合はfalse
   */
  static bool isIterator(ExecutionContext* ctx, Object* obj);

  /**
   * @brief イテレーターのnextメソッドを呼び出す
   * 
   * @param ctx 実行コンテキスト
   * @param iterator イテレーターオブジェクト
   * @param value 引数値（オプション）
   * @return イテレーター結果オブジェクト
   */
  static Object* next(ExecutionContext* ctx, Object* iterator, Value value = Value::createUndefined());

  /**
   * @brief イテレーターのreturnメソッドを呼び出す
   * 
   * @param ctx 実行コンテキスト
   * @param iterator イテレーターオブジェクト
   * @param value 引数値（オプション）
   * @return イテレーター結果オブジェクト、メソッドが存在しない場合はnullptr
   */
  static Object* returnIterator(ExecutionContext* ctx, Object* iterator, Value value = Value::createUndefined());

  /**
   * @brief イテレーターのthrowメソッドを呼び出す
   * 
   * @param ctx 実行コンテキスト
   * @param iterator イテレーターオブジェクト
   * @param value 引数値（オプション）
   * @return イテレーター結果オブジェクト、メソッドが存在しない場合はnullptr
   */
  static Object* throwIterator(ExecutionContext* ctx, Object* iterator, Value value = Value::createUndefined());

  /**
   * @brief イテレーターを完了まで実行し、値を配列に収集
   * 
   * @param ctx 実行コンテキスト
   * @param iterator イテレーターオブジェクト
   * @return 値の配列
   */
  static Array* collectToArray(ExecutionContext* ctx, Object* iterator);
};

/**
 * @brief 非同期イテレーターオブジェクト
 * 
 * next、return、throwメソッドがPromiseを返すイテレーター
 */
class AsyncIterator {
public:
  /**
   * @brief 非同期イテレーターオブジェクトを作成
   * 
   * @param ctx 実行コンテキスト
   * @param nextMethod next関数
   * @param returnMethod return関数（オプション）
   * @param throwMethod throw関数（オプション）
   * @return 非同期イテレーターオブジェクト
   */
  static Object* create(ExecutionContext* ctx, 
                        Function* nextMethod, 
                        Function* returnMethod = nullptr, 
                        Function* throwMethod = nullptr);

  /**
   * @brief オブジェクトが非同期イテレーターかどうか判定
   * 
   * @param ctx 実行コンテキスト
   * @param obj オブジェクト
   * @return 非同期イテレーターの場合はtrue、それ以外の場合はfalse
   */
  static bool isAsyncIterator(ExecutionContext* ctx, Object* obj);

  /**
   * @brief 非同期イテレーターのnextメソッドを呼び出す
   * 
   * @param ctx 実行コンテキスト
   * @param iterator 非同期イテレーターオブジェクト
   * @param value 引数値（オプション）
   * @return Promiseオブジェクト
   */
  static Object* next(ExecutionContext* ctx, Object* iterator, Value value = Value::createUndefined());

  /**
   * @brief 非同期イテレーターのreturnメソッドを呼び出す
   * 
   * @param ctx 実行コンテキスト
   * @param iterator 非同期イテレーターオブジェクト
   * @param value 引数値（オプション）
   * @return Promiseオブジェクト、メソッドが存在しない場合はnullptr
   */
  static Object* returnIterator(ExecutionContext* ctx, Object* iterator, Value value = Value::createUndefined());

  /**
   * @brief 非同期イテレーターのthrowメソッドを呼び出す
   * 
   * @param ctx 実行コンテキスト
   * @param iterator 非同期イテレーターオブジェクト
   * @param value 引数値（オプション）
   * @return Promiseオブジェクト、メソッドが存在しない場合はnullptr
   */
  static Object* throwIterator(ExecutionContext* ctx, Object* iterator, Value value = Value::createUndefined());
};

/**
 * @brief ジェネレーターオブジェクト
 * 
 * ジェネレーター関数から返されるオブジェクト
 */
class GeneratorObject {
public:
  /**
   * @brief ジェネレーターステート
   */
  enum class State {
    Suspended,      ///< 中断状態
    Executing,      ///< 実行中
    Completed,      ///< 完了
    Closing         ///< クローズ中
  };

  /**
   * @brief ジェネレーターオブジェクトを作成
   * 
   * @param ctx 実行コンテキスト
   * @param generatorFunction ジェネレーター関数
   * @return ジェネレーターオブジェクト
   */
  static Object* create(ExecutionContext* ctx, Function* generatorFunction);

  /**
   * @brief オブジェクトがジェネレーターかどうか判定
   * 
   * @param ctx 実行コンテキスト
   * @param obj オブジェクト
   * @return ジェネレーターの場合はtrue、それ以外の場合はfalse
   */
  static bool isGenerator(ExecutionContext* ctx, Object* obj);

  /**
   * @brief ジェネレーターのnextメソッドを実装
   * 
   * @param ctx 実行コンテキスト
   * @param thisValue thisの値
   * @param args 引数リスト
   * @return イテレーター結果オブジェクト
   */
  static Value next(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief ジェネレーターのreturnメソッドを実装
   * 
   * @param ctx 実行コンテキスト
   * @param thisValue thisの値
   * @param args 引数リスト
   * @return イテレーター結果オブジェクト
   */
  static Value returnGenerator(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief ジェネレーターのthrowメソッドを実装
   * 
   * @param ctx 実行コンテキスト
   * @param thisValue thisの値
   * @param args 引数リスト
   * @return イテレーター結果オブジェクト
   */
  static Value throwGenerator(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);
};

/**
 * @brief イテレーションプロトコルの初期化
 * 
 * シンボルやプロトタイプの初期化を行います。
 * 
 * @param ctx 実行コンテキスト
 * @param globalObj グローバルオブジェクト
 */
void initializeIterationProtocol(ExecutionContext* ctx, Object* globalObj);

/**
 * @brief ArrayイテレーターとStringイテレーターのプロトタイプを作成
 * 
 * @param ctx 実行コンテキスト
 * @param objectPrototype Objectプロトタイプ
 * @return ArrayイテレータープロトタイプとStringイテレータープロトタイプのペア
 */
std::pair<Object*, Object*> createIteratorPrototypes(ExecutionContext* ctx, Object* objectPrototype);

/**
 * @brief ジェネレータープロトタイプとジェネレーターオブジェクトプロトタイプを作成
 * 
 * @param ctx 実行コンテキスト
 * @param objectPrototype Objectプロトタイプ
 * @return ジェネレータープロトタイプとジェネレーターオブジェクトプロトタイプのペア
 */
std::pair<Object*, Object*> createGeneratorPrototypes(ExecutionContext* ctx, Object* objectPrototype);

/**
 * @brief 非同期イテレータープロトタイプと非同期ジェネレータープロトタイプを作成
 * 
 * @param ctx 実行コンテキスト
 * @param objectPrototype Objectプロトタイプ
 * @param promisePrototype Promiseプロトタイプ
 * @return 非同期イテレータープロトタイプと非同期ジェネレータープロトタイプのペア
 */
std::pair<Object*, Object*> createAsyncIteratorPrototypes(ExecutionContext* ctx, Object* objectPrototype, Object* promisePrototype);

/**
 * @brief ヘルパー関数: イテレーターを完了する
 * 
 * @param ctx 実行コンテキスト
 * @param iterator イテレーターオブジェクト
 * @param value 返す値
 * @return イテレーターの戻り値
 */
Value completeIterator(ExecutionContext* ctx, Object* iterator, Value value);

} // namespace aero

#endif // AERO_RUNTIME_ITERATION_H 