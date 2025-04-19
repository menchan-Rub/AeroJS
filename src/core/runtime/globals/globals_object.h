/**
 * @file globals_object.h
 * @brief JavaScriptのグローバルオブジェクトの定義
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#ifndef AERO_RUNTIME_GLOBALS_OBJECT_H
#define AERO_RUNTIME_GLOBALS_OBJECT_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "../context/context.h"
#include "../values/value.h"
#include "../object.h"

namespace aero {

// 前方宣言
class Function;

/**
 * @brief JavaScriptのグローバルオブジェクト
 * 
 * JavaScriptのグローバルスコープを表すオブジェクトです。
 * すべてのグローバル変数やグローバル関数はこのオブジェクトのプロパティとして定義されます。
 */
class GlobalsObject {
public:
  /**
   * @brief グローバルオブジェクトのインスタンスを作成
   * 
   * @param ctx 実行コンテキスト
   * @return グローバルオブジェクトのインスタンス
   */
  static GlobalsObject* create(ExecutionContext* ctx);

  /**
   * @brief コンストラクタ
   * 
   * @param ctx 実行コンテキスト
   */
  explicit GlobalsObject(ExecutionContext* ctx);

  /**
   * @brief デストラクタ
   */
  ~GlobalsObject();

  /**
   * @brief グローバルオブジェクトを初期化
   * 
   * グローバル関数、オブジェクト、プロパティを設定します。
   */
  void initialize();

  /**
   * @brief 実行コンテキストを取得
   * 
   * @return 実行コンテキスト
   */
  ExecutionContext* getContext() const { return m_context; }

  /**
   * @brief グローバルオブジェクトを取得
   * 
   * @return グローバルオブジェクト
   */
  Object* getObject() const { return m_globalObject; }

  /**
   * @brief グローバル変数を設定
   * 
   * @param name 変数名
   * @param value 値
   * @param writable 書き込み可能かどうか
   * @param enumerable 列挙可能かどうか
   * @param configurable 設定変更可能かどうか
   */
  void setGlobal(const std::string& name, Value value, 
                 bool writable = true, 
                 bool enumerable = false, 
                 bool configurable = true);

  /**
   * @brief グローバル変数を取得
   * 
   * @param name 変数名
   * @return 変数の値
   */
  Value getGlobal(const std::string& name);

  /**
   * @brief グローバル関数を定義
   * 
   * @param name 関数名
   * @param callback コールバック関数
   * @param length 引数の数
   */
  void defineFunction(const std::string& name, FunctionCallback callback, uint32_t length);

  /**
   * @brief グローバルオブジェクトを定義
   * 
   * @param name オブジェクト名
   * @param obj オブジェクト
   */
  void defineObject(const std::string& name, Object* obj);

private:
  /**
   * @brief プロトタイプオブジェクトを初期化
   */
  void initializePrototypes();

  /**
   * @brief コンストラクタを初期化
   */
  void initializeConstructors();

  /**
   * @brief 組み込みオブジェクトを初期化
   */
  void initializeBuiltins();

  /**
   * @brief グローバルプロパティを初期化
   */
  void initializeProperties();

  ExecutionContext* m_context;        ///< 実行コンテキスト
  Object* m_globalObject;             ///< グローバルオブジェクト
  std::mutex m_mutex;                 ///< スレッド同期用ミューテックス
  bool m_isInitialized;               ///< 初期化済みかどうか

  // プロトタイプオブジェクト
  Object* m_objectPrototype;          ///< Objectプロトタイプ
  Object* m_functionPrototype;        ///< Functionプロトタイプ
  Object* m_arrayPrototype;           ///< Arrayプロトタイプ
  Object* m_stringPrototype;          ///< Stringプロトタイプ
  Object* m_numberPrototype;          ///< Numberプロトタイプ
  Object* m_booleanPrototype;         ///< Booleanプロトタイプ
  Object* m_datePrototype;            ///< Dateプロトタイプ
  Object* m_regexpPrototype;          ///< RegExpプロトタイプ
  std::unordered_map<std::string, Object*> m_errorPrototypes; ///< Errorプロトタイプマップ
  Object* m_symbolPrototype;          ///< Symbolプロトタイプ
  Object* m_promisePrototype;         ///< Promiseプロトタイプ
  Object* m_mapPrototype;             ///< Mapプロトタイプ
  Object* m_setPrototype;             ///< Setプロトタイプ
  Object* m_weakMapPrototype;         ///< WeakMapプロトタイプ
  Object* m_weakSetPrototype;         ///< WeakSetプロトタイプ

  // コンストラクタオブジェクト
  Object* m_objectConstructor;        ///< Objectコンストラクタ
  Object* m_functionConstructor;      ///< Functionコンストラクタ
  Object* m_arrayConstructor;         ///< Arrayコンストラクタ
  Object* m_stringConstructor;        ///< Stringコンストラクタ
  Object* m_numberConstructor;        ///< Numberコンストラクタ
  Object* m_booleanConstructor;       ///< Booleanコンストラクタ
  Object* m_dateConstructor;          ///< Dateコンストラクタ
  Object* m_regexpConstructor;        ///< RegExpコンストラクタ
  std::unordered_map<std::string, Object*> m_errorConstructors; ///< Errorコンストラクタマップ
  Object* m_symbolConstructor;        ///< Symbolコンストラクタ
  Object* m_promiseConstructor;       ///< Promiseコンストラクタ
  Object* m_mapConstructor;           ///< Mapコンストラクタ
  Object* m_setConstructor;           ///< Setコンストラクタ
  Object* m_weakMapConstructor;       ///< WeakMapコンストラクタ
  Object* m_weakSetConstructor;       ///< WeakSetコンストラクタ
};

} // namespace aero

#endif // AERO_RUNTIME_GLOBALS_OBJECT_H 