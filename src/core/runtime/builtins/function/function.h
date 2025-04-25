/**
 * @file function.h
 * @brief JavaScript の Function オブジェクトの定義
 * @copyright 2023 AeroJS Project
 */

#pragma once

#include <string>
#include <vector>

#include "core/runtime/object.h"
#include "core/runtime/value.h"

namespace aero {

class Context;
class GlobalObject;

/**
 * @class FunctionObject
 * @brief JavaScript の Function オブジェクトの実装
 *
 * ECMAScriptの仕様に準拠したFunctionオブジェクトを提供します。
 * 関数の作成、呼び出し、バインディングなどの機能を実装しています。
 */
class FunctionObject : public Object {
 public:
  using NativeFunction = Value (*)(Context*, Value, const std::vector<Value>&);

  /**
   * @brief ネイティブ関数からFunctionオブジェクトを作成
   * @param proto プロトタイプオブジェクト
   * @param func ネイティブ関数ポインタ
   * @param length 引数の数（デフォルトはECMAScript上での関数の長さ）
   * @param name 関数名（デフォルトは空文字列）
   */
  FunctionObject(Object* proto, NativeFunction func, int length = 0, const std::string& name = "");

  /**
   * @brief 名前とソースコードからFunctionオブジェクトを作成
   * @param proto プロトタイプオブジェクト
   * @param name 関数名
   * @param parameterList パラメータリスト
   * @param body 関数本体
   * @param scope スコープチェーン
   */
  FunctionObject(Object* proto, const std::string& name, const std::vector<std::string>& parameterList,
                 const std::string& body, Object* scope);

  /**
   * @brief デストラクタ
   */
  ~FunctionObject() override;

  /**
   * @brief 関数かどうかを判定
   * @return 常にtrue
   */
  bool isFunction() const override {
    return true;
  }

  /**
   * @brief 関数を呼び出す
   * @param context 実行コンテキスト
   * @param thisValue thisの値
   * @param args 引数リスト
   * @return 関数の戻り値
   */
  Value call(Context* context, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief コンストラクタとして関数を呼び出す
   * @param context 実行コンテキスト
   * @param args 引数リスト
   * @return 新しく作成されたオブジェクト
   */
  Value construct(Context* context, const std::vector<Value>& args);

  /**
   * @brief 関数の引数の数を取得
   * @return 引数の数
   */
  int length() const {
    return m_length;
  }

  /**
   * @brief 関数名を取得
   * @return 関数名
   */
  const std::string& name() const {
    return m_name;
  }

  /**
   * @brief 関数のプロトタイプオブジェクトを取得
   * @return プロトタイプオブジェクト
   */
  Object* prototype() const {
    return m_prototype;
  }

  /**
   * @brief 関数のプロトタイプオブジェクトを設定
   * @param prototype 新しいプロトタイプオブジェクト
   */
  void setPrototype(Object* prototype) {
    m_prototype = prototype;
  }

 private:
  NativeFunction m_nativeFunction;           // ネイティブ関数ポインタ
  int m_length;                              // 関数の引数の数
  std::string m_name;                        // 関数名
  Object* m_prototype;                       // 関数のプロトタイプオブジェクト
  std::vector<std::string> m_parameterList;  // パラメータリスト
  std::string m_body;                        // 関数本体
  Object* m_scope;                           // スコープチェーン
  bool m_isNative;                           // ネイティブ関数かどうか
};

/**
 * @brief Function コンストラクタ
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 新しく作成されたFunctionオブジェクト
 */
Value functionConstructor(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Function.prototype.toString
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 関数の文字列表現
 */
Value functionToString(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Function.prototype.apply
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 関数の実行結果
 */
Value functionApply(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Function.prototype.call
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 関数の実行結果
 */
Value functionCall(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Function.prototype.bind
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return バインドされた新しい関数
 */
Value functionBind(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Function プロトタイプの初期化
 * @param context 実行コンテキスト
 * @param proto Function.prototype オブジェクト
 */
void initFunctionPrototype(Context* context, Object* proto);

/**
 * @brief Function コンストラクタの初期化
 * @param context 実行コンテキスト
 * @param constructor Function コンストラクタ
 * @param proto Function.prototype オブジェクト
 */
void initFunctionConstructor(Context* context, Object* constructor, Object* proto);

/**
 * @brief Function 組み込みオブジェクトの登録
 * @param global グローバルオブジェクト
 */
void registerFunctionBuiltin(GlobalObject* global);

}  // namespace aero

#endif  // AERO_FUNCTION_BUILTIN_H