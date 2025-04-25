/**
 * @file boolean.h
 * @brief JavaScriptのBoolean組み込みオブジェクトの定義
 * @copyright Copyright (c) 2023 AeroJS project authors
 */

#ifndef AERO_BOOLEAN_H
#define AERO_BOOLEAN_H

#include <vector>

#include "core/runtime/object.h"
#include "core/runtime/value.h"

namespace aero {

class Context;
class ExecutionState;
class GlobalObject;

/**
 * @class BooleanObject
 * @brief JavaScriptのBooleanオブジェクトを表すクラス
 *
 * BooleanObjectはプリミティブなbool値をラップし、オブジェクトとしてのメソッドと
 * プロパティにアクセスできるようにします。ECMAScript仕様に準拠して実装されています。
 */
class BooleanObject : public Object {
 public:
  /**
   * @brief コンストラクタ
   * @param value ラップするブール値
   */
  explicit BooleanObject(bool value);

  /**
   * @brief デストラクタ
   */
  ~BooleanObject() override;

  /**
   * @brief 内部のブール値を取得
   * @return ラップされているブール値
   */
  bool value() const;

  /**
   * @brief BooleanオブジェクトのプロトタイプとBooleanコンストラクタを初期化
   * @param context 現在の実行コンテキスト
   * @return BooleanコンストラクタのValue
   */
  static Value initializePrototype(Context* context);

 private:
  bool m_value;  // ラップされたプリミティブなブール値
};

/**
 * @brief Booleanコンストラクタ関数
 * @param context 現在の実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 新しいBooleanオブジェクトかプリミティブブール値
 */
Value booleanConstructor(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Boolean.prototypeのtoString実装
 * @param context 現在の実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return ブール値を表す文字列 ("true" または "false")
 */
Value booleanToString(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Boolean.prototypeのvalueOf実装
 * @param context 現在の実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return ラップされたプリミティブブール値
 */
Value booleanValueOf(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Booleanオブジェクトを初期化
 * @param context 現在の実行コンテキスト
 * @return Booleanコンストラクタ関数のValue
 */
Value initializeBoolean(Context* context);

/**
 * @brief Boolean組み込みオブジェクトをグローバルオブジェクトに登録
 * @param global グローバルオブジェクト
 */
void registerBooleanBuiltin(GlobalObject* global);

}  // namespace aero

#endif  // AERO_BOOLEAN_H