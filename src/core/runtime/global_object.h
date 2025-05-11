/**
 * @file global_object.h
 * @brief JavaScript のグローバルオブジェクトの定義
 * @copyright Copyright (c) 2023 AeroJS project authors
 */

#ifndef AERO_GLOBAL_OBJECT_H
#define AERO_GLOBAL_OBJECT_H

#include <memory>
#include <mutex>

#include "object.h"

namespace aero {

class Context;
class FunctionObject;

/**
 * @class GlobalObject
 * @brief JavaScriptのグローバルオブジェクトを表すクラス
 *
 * グローバルオブジェクトはJavaScriptのグローバルスコープにおけるオブジェクトで、
 * 組み込みオブジェクトやコンストラクタ、グローバル関数などを提供します。
 */
class GlobalObject : public Object {
 public:
  /**
   * @brief コンストラクタ
   * @param context 実行コンテキスト
   */
  explicit GlobalObject(Context* context);

  /**
   * @brief デストラクタ
   */
  ~GlobalObject() override;

  /**
   * @brief グローバルオブジェクトを初期化
   * 組み込みオブジェクトやプロトタイプなどの設定を行います。
   */
  void initialize();

  /**
   * @brief オブジェクトプロトタイプを取得
   * @return オブジェクトプロトタイプ
   */
  Object* objectPrototype() const {
    return m_objectPrototype;
  }

  /**
   * @brief 関数プロトタイプを取得
   * @return 関数プロトタイプ
   */
  Object* functionPrototype() const {
    return m_functionPrototype;
  }

  /**
   * @brief 配列プロトタイプを取得
   * @return 配列プロトタイプ
   */
  Object* arrayPrototype() const {
    return m_arrayPrototype;
  }

  /**
   * @brief 文字列プロトタイプを取得
   * @return 文字列プロトタイプ
   */
  Object* stringPrototype() const {
    return m_stringPrototype;
  }

  /**
   * @brief 数値プロトタイプを取得
   * @return 数値プロトタイプ
   */
  Object* numberPrototype() const {
    return m_numberPrototype;
  }

  /**
   * @brief ブールプロトタイプを取得
   * @return ブールプロトタイプ
   */
  Object* booleanPrototype() const {
    return m_booleanPrototype;
  }

  /**
   * @brief 日付プロトタイプを取得
   * @return 日付プロトタイプ
   */
  Object* datePrototype() const {
    return m_datePrototype;
  }

  /**
   * @brief 正規表現プロトタイプを取得
   * @return 正規表現プロトタイプ
   */
  Object* regExpPrototype() const {
    return m_regExpPrototype;
  }

  /**
   * @brief エラープロトタイプを取得
   * @return エラープロトタイプ
   */
  Object* errorPrototype() const {
    return m_errorPrototype;
  }

  /**
   * @brief Setプロトタイプを取得
   * @return Setプロトタイプ
   */
  Object* setPrototype() const {
    return m_setPrototype;
  }

  /**
   * @brief WeakMapプロトタイプを取得
   * @return WeakMapプロトタイプ
   */
  Object* weakMapPrototype() const {
    return m_weakMapPrototype;
  }

  /**
   * @brief WeakRefプロトタイプを取得
   * @return WeakRefプロトタイプ
   */
  Object* weakRefPrototype() const {
    return m_weakRefPrototype;
  }

  /**
   * @brief FinalizationRegistryプロトタイプを取得
   * @return FinalizationRegistryプロトタイプ
   */
  Object* finalizationRegistryPrototype() const {
    return m_finalizationRegistryPrototype;
  }

  /**
   * @brief オブジェクトコンストラクタを取得
   * @return オブジェクトコンストラクタ
   */
  FunctionObject* objectConstructor() const {
    return m_objectConstructor;
  }

  /**
   * @brief 関数コンストラクタを取得
   * @return 関数コンストラクタ
   */
  FunctionObject* functionConstructor() const {
    return m_functionConstructor;
  }

  /**
   * @brief 配列コンストラクタを取得
   * @return 配列コンストラクタ
   */
  FunctionObject* arrayConstructor() const {
    return m_arrayConstructor;
  }

  /**
   * @brief 文字列コンストラクタを取得
   * @return 文字列コンストラクタ
   */
  FunctionObject* stringConstructor() const {
    return m_stringConstructor;
  }

  /**
   * @brief 数値コンストラクタを取得
   * @return 数値コンストラクタ
   */
  FunctionObject* numberConstructor() const {
    return m_numberConstructor;
  }

  /**
   * @brief ブールコンストラクタを取得
   * @return ブールコンストラクタ
   */
  FunctionObject* booleanConstructor() const {
    return m_booleanConstructor;
  }

  /**
   * @brief 日付コンストラクタを取得
   * @return 日付コンストラクタ
   */
  FunctionObject* dateConstructor() const {
    return m_dateConstructor;
  }

  /**
   * @brief 正規表現コンストラクタを取得
   * @return 正規表現コンストラクタ
   */
  FunctionObject* regExpConstructor() const {
    return m_regExpConstructor;
  }

  /**
   * @brief エラーコンストラクタを取得
   * @return エラーコンストラクタ
   */
  FunctionObject* errorConstructor() const {
    return m_errorConstructor;
  }

  /**
   * @brief Setコンストラクタを取得
   * @return Setコンストラクタ
   */
  FunctionObject* setConstructor() const {
    return m_setConstructor;
  }

  /**
   * @brief WeakMapコンストラクタを取得
   * @return WeakMapコンストラクタ
   */
  FunctionObject* weakMapConstructor() const {
    return m_weakMapConstructor;
  }

  /**
   * @brief WeakRefコンストラクタを取得
   * @return WeakRefコンストラクタ
   */
  FunctionObject* weakRefConstructor() const {
    return m_weakRefConstructor;
  }

  /**
   * @brief FinalizationRegistryコンストラクタを取得
   * @return FinalizationRegistryコンストラクタ
   */
  FunctionObject* finalizationRegistryConstructor() const {
    return m_finalizationRegistryConstructor;
  }

  /**
   * @brief オブジェクトプロトタイプを設定
   * @param prototype オブジェクトプロトタイプ
   */
  void setObjectPrototype(Object* prototype) {
    m_objectPrototype = prototype;
  }

  /**
   * @brief 関数プロトタイプを設定
   * @param prototype 関数プロトタイプ
   */
  void setFunctionPrototype(Object* prototype) {
    m_functionPrototype = prototype;
  }

  /**
   * @brief 配列プロトタイプを設定
   * @param prototype 配列プロトタイプ
   */
  void setArrayPrototype(Object* prototype) {
    m_arrayPrototype = prototype;
  }

  /**
   * @brief 文字列プロトタイプを設定
   * @param prototype 文字列プロトタイプ
   */
  void setStringPrototype(Object* prototype) {
    m_stringPrototype = prototype;
  }

  /**
   * @brief 数値プロトタイプを設定
   * @param prototype 数値プロトタイプ
   */
  void setNumberPrototype(Object* prototype) {
    m_numberPrototype = prototype;
  }

  /**
   * @brief ブールプロトタイプを設定
   * @param prototype ブールプロトタイプ
   */
  void setBooleanPrototype(Object* prototype) {
    m_booleanPrototype = prototype;
  }

  /**
   * @brief 日付プロトタイプを設定
   * @param prototype 日付プロトタイプ
   */
  void setDatePrototype(Object* prototype) {
    m_datePrototype = prototype;
  }

  /**
   * @brief 正規表現プロトタイプを設定
   * @param prototype 正規表現プロトタイプ
   */
  void setRegExpPrototype(Object* prototype) {
    m_regExpPrototype = prototype;
  }

  /**
   * @brief エラープロトタイプを設定
   * @param prototype エラープロトタイプ
   */
  void setErrorPrototype(Object* prototype) {
    m_errorPrototype = prototype;
  }

  /**
   * @brief Setプロトタイプを設定
   * @param prototype Setプロトタイプ
   */
  void setSetPrototype(Object* prototype) {
    m_setPrototype = prototype;
  }

  /**
   * @brief WeakMapプロトタイプを設定
   * @param prototype WeakMapプロトタイプ
   */
  void setWeakMapPrototype(Object* prototype) {
    m_weakMapPrototype = prototype;
  }

  /**
   * @brief WeakRefプロトタイプを設定
   * @param prototype WeakRefプロトタイプ
   */
  void setWeakRefPrototype(Object* prototype) {
    m_weakRefPrototype = prototype;
  }

  /**
   * @brief FinalizationRegistryプロトタイプを設定
   * @param prototype FinalizationRegistryプロトタイプ
   */
  void setFinalizationRegistryPrototype(Object* prototype) {
    m_finalizationRegistryPrototype = prototype;
  }

  /**
   * @brief オブジェクトコンストラクタを設定
   * @param constructor オブジェクトコンストラクタ
   */
  void setObjectConstructor(FunctionObject* constructor) {
    m_objectConstructor = constructor;
  }

  /**
   * @brief 関数コンストラクタを設定
   * @param constructor 関数コンストラクタ
   */
  void setFunctionConstructor(FunctionObject* constructor) {
    m_functionConstructor = constructor;
  }

  /**
   * @brief 配列コンストラクタを設定
   * @param constructor 配列コンストラクタ
   */
  void setArrayConstructor(FunctionObject* constructor) {
    m_arrayConstructor = constructor;
  }

  /**
   * @brief 文字列コンストラクタを設定
   * @param constructor 文字列コンストラクタ
   */
  void setStringConstructor(FunctionObject* constructor) {
    m_stringConstructor = constructor;
  }

  /**
   * @brief 数値コンストラクタを設定
   * @param constructor 数値コンストラクタ
   */
  void setNumberConstructor(FunctionObject* constructor) {
    m_numberConstructor = constructor;
  }

  /**
   * @brief ブールコンストラクタを設定
   * @param constructor ブールコンストラクタ
   */
  void setBooleanConstructor(FunctionObject* constructor) {
    m_booleanConstructor = constructor;
  }

  /**
   * @brief 日付コンストラクタを設定
   * @param constructor 日付コンストラクタ
   */
  void setDateConstructor(FunctionObject* constructor) {
    m_dateConstructor = constructor;
  }

  /**
   * @brief 正規表現コンストラクタを設定
   * @param constructor 正規表現コンストラクタ
   */
  void setRegExpConstructor(FunctionObject* constructor) {
    m_regExpConstructor = constructor;
  }

  /**
   * @brief エラーコンストラクタを設定
   * @param constructor エラーコンストラクタ
   */
  void setErrorConstructor(FunctionObject* constructor) {
    m_errorConstructor = constructor;
  }

  /**
   * @brief Setコンストラクタを設定
   * @param constructor Setコンストラクタ
   */
  void setSetConstructor(FunctionObject* constructor) {
    m_setConstructor = constructor;
  }

  /**
   * @brief WeakMapコンストラクタを設定
   * @param constructor WeakMapコンストラクタ
   */
  void setWeakMapConstructor(FunctionObject* constructor) {
    m_weakMapConstructor = constructor;
  }

  /**
   * @brief WeakRefコンストラクタを設定
   * @param constructor WeakRefコンストラクタ
   */
  void setWeakRefConstructor(FunctionObject* constructor) {
    m_weakRefConstructor = constructor;
  }

  /**
   * @brief FinalizationRegistryコンストラクタを設定
   * @param constructor FinalizationRegistryコンストラクタ
   */
  void setFinalizationRegistryConstructor(FunctionObject* constructor) {
    m_finalizationRegistryConstructor = constructor;
  }

  /**
   * @brief 実行コンテキストを取得
   * @return 実行コンテキスト
   */
  Context* context() const {
    return m_context;
  }

 private:
  // 各組み込みオブジェクトのプロトタイプ
  Object* m_objectPrototype;
  Object* m_functionPrototype;
  Object* m_arrayPrototype;
  Object* m_stringPrototype;
  Object* m_numberPrototype;
  Object* m_booleanPrototype;
  Object* m_datePrototype;
  Object* m_regExpPrototype;
  Object* m_errorPrototype;
  Object* m_setPrototype;
  Object* m_weakMapPrototype;
  Object* m_weakRefPrototype;
  Object* m_finalizationRegistryPrototype;

  // 各組み込みオブジェクトのコンストラクタ
  FunctionObject* m_objectConstructor;
  FunctionObject* m_functionConstructor;
  FunctionObject* m_arrayConstructor;
  FunctionObject* m_stringConstructor;
  FunctionObject* m_numberConstructor;
  FunctionObject* m_booleanConstructor;
  FunctionObject* m_dateConstructor;
  FunctionObject* m_regExpConstructor;
  FunctionObject* m_errorConstructor;
  FunctionObject* m_setConstructor;
  FunctionObject* m_weakMapConstructor;
  FunctionObject* m_weakRefConstructor;
  FunctionObject* m_finalizationRegistryConstructor;

  // 実行コンテキスト
  Context* m_context;

  // 初期化ロックのためのミューテックス
  std::mutex m_initMutex;

  // 初期化済みフラグ
  bool m_initialized;

  // 組み込みオブジェクトの初期化
  void initializeBuiltins();
};

}  // namespace aero

#endif  // AERO_GLOBAL_OBJECT_H