/**
 * @file module.cpp
 * @brief JavaScript の Set 組み込みオブジェクトのモジュール登録
 * @copyright Copyright (c) 2023 AeroJS project authors
 */

#include "../../global_object.h"
#include "set.h"

namespace aero {

/**
 * @brief Set 組み込みオブジェクトをグローバルオブジェクトに登録
 *
 * @param global グローバルオブジェクト
 */
void registerSetBuiltin(GlobalObject* global) {
  if (!global) {
    return;
  }

  // 実行コンテキストを取得
  Context* context = global->context();
  if (!context) {
    return;
  }

  // Setオブジェクトを初期化
  Value setConstructor = initializeSet(context);

  // SetコンストラクタをグローバルオブジェクトにSetという名前で登録
  global->defineOwnProperty(
      context->staticStrings()->Set,
      Object::PropertyDescriptor(
          setConstructor,
          Object::PropertyDescriptor::Writable | Object::PropertyDescriptor::Configurable));
}

}  // namespace aero