/**
 * @file array_iteration.cpp
 * @brief JavaScript の Array オブジェクトの反復処理メソッド実装
 */

#include <stdexcept>

#include "../../../object.h"
#include "../../../value.h"
#include "../function/function.h"
#include "array.h"

namespace aerojs {
namespace core {

ValuePtr Array::forEach(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.forEach called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // コールバック関数が提供されていない場合はエラー
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isFunction()) {
    throw std::runtime_error("Array.prototype.forEach: callback must be a function");
  }

  // コールバック関数を取得
  FunctionPtr callback = std::dynamic_pointer_cast<Function>(arguments[1]);

  // thisArg を取得（オプション）
  ValuePtr thisArg = Value::undefined();
  if (arguments.size() > 2) {
    thisArg = arguments[2];
  }

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 各要素に対してコールバックを実行
  for (uint32_t i = 0; i < length; ++i) {
    std::string key = std::to_string(i);

    // プロパティが存在する場合のみ処理
    if (obj->hasProperty(key)) {
      ValuePtr currentValue = obj->get(key);

      // コールバック関数の引数を準備
      std::vector<ValuePtr> callbackArgs = {
          currentValue,                               // 現在の値
          Value::fromNumber(static_cast<double>(i)),  // インデックス
          obj                                         // 元の配列
      };

      // コールバック関数を実行
      callback->call(thisArg, callbackArgs);
    }
  }

  // undefined を返す（forEach は値を返さない）
  return Value::undefined();
}

ValuePtr Array::map(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.map called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // コールバック関数が提供されていない場合はエラー
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isFunction()) {
    throw std::runtime_error("Array.prototype.map: callback must be a function");
  }

  // コールバック関数を取得
  FunctionPtr callback = std::dynamic_pointer_cast<Function>(arguments[1]);

  // thisArg を取得（オプション）
  ValuePtr thisArg = Value::undefined();
  if (arguments.size() > 2) {
    thisArg = arguments[2];
  }

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 結果を格納する新しい配列を作成
  ObjectPtr resultArray = Array::create();
  resultArray->set("length", Value::fromNumber(static_cast<double>(length)));

  // 各要素に対してコールバックを実行
  for (uint32_t i = 0; i < length; ++i) {
    std::string key = std::to_string(i);

    // プロパティが存在する場合のみ処理
    if (obj->hasProperty(key)) {
      ValuePtr currentValue = obj->get(key);

      // コールバック関数の引数を準備
      std::vector<ValuePtr> callbackArgs = {
          currentValue,                               // 現在の値
          Value::fromNumber(static_cast<double>(i)),  // インデックス
          obj                                         // 元の配列
      };

      // コールバック関数を実行し、結果を新しい配列に格納
      ValuePtr mappedValue = callback->call(thisArg, callbackArgs);
      resultArray->set(key, mappedValue);
    }
  }

  return resultArray;
}

ValuePtr Array::filter(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.filter called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // コールバック関数が提供されていない場合はエラー
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isFunction()) {
    throw std::runtime_error("Array.prototype.filter: callback must be a function");
  }

  // コールバック関数を取得
  FunctionPtr callback = std::dynamic_pointer_cast<Function>(arguments[1]);

  // thisArg を取得（オプション）
  ValuePtr thisArg = Value::undefined();
  if (arguments.size() > 2) {
    thisArg = arguments[2];
  }

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 結果を格納する新しい配列を作成
  ObjectPtr resultArray = Array::create();
  uint32_t resultLength = 0;

  // 各要素に対してコールバックを実行
  for (uint32_t i = 0; i < length; ++i) {
    std::string key = std::to_string(i);

    // プロパティが存在する場合のみ処理
    if (obj->hasProperty(key)) {
      ValuePtr currentValue = obj->get(key);

      // コールバック関数の引数を準備
      std::vector<ValuePtr> callbackArgs = {
          currentValue,                               // 現在の値
          Value::fromNumber(static_cast<double>(i)),  // インデックス
          obj                                         // 元の配列
      };

      // コールバック関数を実行
      ValuePtr result = callback->call(thisArg, callbackArgs);

      // コールバックが true を返した場合、要素を結果配列に追加
      if (result->toBoolean()) {
        resultArray->set(std::to_string(resultLength), currentValue);
        ++resultLength;
      }
    }
  }

  // 長さを設定
  resultArray->set("length", Value::fromNumber(static_cast<double>(resultLength)));

  return resultArray;
}

ValuePtr Array::reduce(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.reduce called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // コールバック関数が提供されていない場合はエラー
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isFunction()) {
    throw std::runtime_error("Array.prototype.reduce: callback must be a function");
  }

  // コールバック関数を取得
  FunctionPtr callback = std::dynamic_pointer_cast<Function>(arguments[1]);

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 空の配列で初期値がない場合はエラー
  if (length == 0 && arguments.size() <= 2) {
    throw std::runtime_error("Reduce of empty array with no initial value");
  }

  // アキュムレータの初期値を設定
  ValuePtr accumulator;
  uint32_t startIndex = 0;

  if (arguments.size() > 2) {
    // 初期値が提供されている場合
    accumulator = arguments[2];
  } else {
    // 初期値が提供されていない場合、最初の要素を使用
    bool foundFirstElement = false;
    for (uint32_t i = 0; i < length; ++i) {
      std::string key = std::to_string(i);
      if (obj->hasProperty(key)) {
        accumulator = obj->get(key);
        startIndex = i + 1;
        foundFirstElement = true;
        break;
      }
    }

    if (!foundFirstElement) {
      throw std::runtime_error("Reduce of empty array with no initial value");
    }
  }

  // 各要素に対してコールバックを実行
  for (uint32_t i = startIndex; i < length; ++i) {
    std::string key = std::to_string(i);

    // プロパティが存在する場合のみ処理
    if (obj->hasProperty(key)) {
      ValuePtr currentValue = obj->get(key);

      // コールバック関数の引数を準備
      std::vector<ValuePtr> callbackArgs = {
          accumulator,                                // アキュムレータ
          currentValue,                               // 現在の値
          Value::fromNumber(static_cast<double>(i)),  // インデックス
          obj                                         // 元の配列
      };

      // コールバック関数を実行し、結果をアキュムレータに格納
      accumulator = callback->call(Value::undefined(), callbackArgs);
    }
  }

  return accumulator;
}

ValuePtr Array::reduceRight(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.reduceRight called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // コールバック関数が提供されていない場合はエラー
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isFunction()) {
    throw std::runtime_error("Array.prototype.reduceRight: callback must be a function");
  }

  // コールバック関数を取得
  FunctionPtr callback = std::dynamic_pointer_cast<Function>(arguments[1]);

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 空の配列で初期値がない場合はエラー
  if (length == 0 && arguments.size() <= 2) {
    throw std::runtime_error("Reduce of empty array with no initial value");
  }

  // アキュムレータの初期値を設定
  ValuePtr accumulator;
  int32_t startIndex = static_cast<int32_t>(length) - 1;

  if (arguments.size() > 2) {
    // 初期値が提供されている場合
    accumulator = arguments[2];
  } else {
    // 初期値が提供されていない場合、最後の要素を使用
    bool foundLastElement = false;
    for (int32_t i = startIndex; i >= 0; --i) {
      std::string key = std::to_string(i);
      if (obj->hasProperty(key)) {
        accumulator = obj->get(key);
        startIndex = i - 1;
        foundLastElement = true;
        break;
      }
    }

    if (!foundLastElement) {
      throw std::runtime_error("Reduce of empty array with no initial value");
    }
  }

  // 各要素に対してコールバックを実行（後ろから前へ）
  for (int32_t i = startIndex; i >= 0; --i) {
    std::string key = std::to_string(i);

    // プロパティが存在する場合のみ処理
    if (obj->hasProperty(key)) {
      ValuePtr currentValue = obj->get(key);

      // コールバック関数の引数を準備
      std::vector<ValuePtr> callbackArgs = {
          accumulator,                                // アキュムレータ
          currentValue,                               // 現在の値
          Value::fromNumber(static_cast<double>(i)),  // インデックス
          obj                                         // 元の配列
      };

      // コールバック関数を実行し、結果をアキュムレータに格納
      accumulator = callback->call(Value::undefined(), callbackArgs);
    }
  }

  return accumulator;
}

}  // namespace core
}  // namespace aerojs