/**
 * @file array_iterator.cpp
 * @brief JavaScript の Array オブジェクトの反復処理メソッド実装
 */

#include <stdexcept>

#include "../../../function.h"
#include "../../../object.h"
#include "../../../value.h"
#include "array.h"

namespace aerojs {
namespace core {

ValuePtr Array::forEach(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.forEach called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // length を取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // コールバック関数が渡されていなければエラー
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isFunction()) {
    throw std::runtime_error("Array.prototype.forEach: callback must be a function");
  }

  ValuePtr callbackFn = arguments[1];
  ValuePtr thisArg = arguments.size() > 2 ? arguments[2] : Value::undefined();

  // 各要素に対してコールバックを実行
  for (uint32_t i = 0; i < length; ++i) {
    // 現在のインデックスにプロパティが存在するか確認
    if (!obj->hasProperty(std::to_string(i))) {
      continue;
    }

    ValuePtr element = obj->get(std::to_string(i));
    if (!element) {
      element = Value::undefined();
    }

    // コールバック関数を呼び出し
    std::vector<ValuePtr> callbackArgs = {
        element,                                    // 要素
        Value::fromNumber(static_cast<double>(i)),  // インデックス
        arguments[0]                                // 配列自体
    };

    callFunction(callbackFn, thisArg, callbackArgs);
  }

  // undefined を返す
  return Value::undefined();
}

ValuePtr Array::map(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.map called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // length を取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // コールバック関数が渡されていなければエラー
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isFunction()) {
    throw std::runtime_error("Array.prototype.map: callback must be a function");
  }

  ValuePtr callbackFn = arguments[1];
  ValuePtr thisArg = arguments.size() > 2 ? arguments[2] : Value::undefined();

  // 新しい配列を作成
  ArrayPtr resultArray = Array::create();
  resultArray->set("length", Value::fromNumber(static_cast<double>(length)));

  // 各要素に対してコールバックを実行し、結果を新しい配列に格納
  for (uint32_t i = 0; i < length; ++i) {
    // 現在のインデックスにプロパティが存在するか確認
    if (obj->hasProperty(std::to_string(i))) {
      ValuePtr element = obj->get(std::to_string(i));
      if (!element) {
        element = Value::undefined();
      }

      // コールバック関数を呼び出し
      std::vector<ValuePtr> callbackArgs = {
          element,                                    // 要素
          Value::fromNumber(static_cast<double>(i)),  // インデックス
          arguments[0]                                // 配列自体
      };

      ValuePtr mappedValue = callFunction(callbackFn, thisArg, callbackArgs);

      // 結果を新しい配列に格納
      resultArray->set(std::to_string(i), mappedValue);
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

  // length を取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // コールバック関数が渡されていなければエラー
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isFunction()) {
    throw std::runtime_error("Array.prototype.filter: callback must be a function");
  }

  ValuePtr callbackFn = arguments[1];
  ValuePtr thisArg = arguments.size() > 2 ? arguments[2] : Value::undefined();

  // 新しい配列を作成
  ArrayPtr resultArray = Array::create();
  uint32_t resultLength = 0;

  // 各要素に対してコールバックを実行し、条件を満たす要素を新しい配列に追加
  for (uint32_t i = 0; i < length; ++i) {
    // 現在のインデックスにプロパティが存在するか確認
    if (!obj->hasProperty(std::to_string(i))) {
      continue;
    }

    ValuePtr element = obj->get(std::to_string(i));
    if (!element) {
      element = Value::undefined();
    }

    // コールバック関数を呼び出し
    std::vector<ValuePtr> callbackArgs = {
        element,                                    // 要素
        Value::fromNumber(static_cast<double>(i)),  // インデックス
        arguments[0]                                // 配列自体
    };

    ValuePtr result = callFunction(callbackFn, thisArg, callbackArgs);

    // コールバックが true を返せば要素を新しい配列に追加
    if (result->toBoolean()) {
      resultArray->set(std::to_string(resultLength), element);
      resultLength++;
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

  // length を取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // コールバック関数が渡されていなければエラー
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isFunction()) {
    throw std::runtime_error("Array.prototype.reduce: callback must be a function");
  }

  ValuePtr callbackFn = arguments[1];

  // 空の配列で初期値がなければエラー
  if (length == 0 && arguments.size() <= 2) {
    throw std::runtime_error("Array.prototype.reduce of empty array with no initial value");
  }

  ValuePtr accumulator;
  uint32_t k = 0;

  // 初期値が指定されているかどうかで開始インデックスと初期値を決定
  if (arguments.size() > 2) {
    // 初期値が指定されている場合
    accumulator = arguments[2];
  } else {
    // 初期値が指定されていない場合、最初の要素を初期値にする
    bool foundInitialValue = false;
    while (k < length && !foundInitialValue) {
      if (obj->hasProperty(std::to_string(k))) {
        accumulator = obj->get(std::to_string(k));
        foundInitialValue = true;
      }
      k++;
    }

    if (!foundInitialValue) {
      throw std::runtime_error("Array.prototype.reduce of empty array with no initial value");
    }
  }

  // 残りの要素に対してコールバックを実行
  for (; k < length; ++k) {
    if (!obj->hasProperty(std::to_string(k))) {
      continue;
    }

    ValuePtr currentValue = obj->get(std::to_string(k));
    if (!currentValue) {
      currentValue = Value::undefined();
    }

    // コールバック関数を呼び出し
    std::vector<ValuePtr> callbackArgs = {
        accumulator,                                // アキュムレータ
        currentValue,                               // 現在の値
        Value::fromNumber(static_cast<double>(k)),  // インデックス
        arguments[0]                                // 配列自体
    };

    accumulator = callFunction(callbackFn, Value::undefined(), callbackArgs);
  }

  return accumulator;
}

ValuePtr Array::reduceRight(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.reduceRight called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // length を取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // コールバック関数が渡されていなければエラー
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isFunction()) {
    throw std::runtime_error("Array.prototype.reduceRight: callback must be a function");
  }

  ValuePtr callbackFn = arguments[1];

  // 空の配列で初期値がなければエラー
  if (length == 0 && arguments.size() <= 2) {
    throw std::runtime_error("Array.prototype.reduceRight of empty array with no initial value");
  }

  ValuePtr accumulator;
  int32_t k = length - 1;

  // 初期値が指定されているかどうかで開始インデックスと初期値を決定
  if (arguments.size() > 2) {
    // 初期値が指定されている場合
    accumulator = arguments[2];
  } else {
    // 初期値が指定されていない場合、最後の要素を初期値にする
    bool foundInitialValue = false;
    while (k >= 0 && !foundInitialValue) {
      if (obj->hasProperty(std::to_string(k))) {
        accumulator = obj->get(std::to_string(k));
        foundInitialValue = true;
      }
      k--;
    }

    if (!foundInitialValue) {
      throw std::runtime_error("Array.prototype.reduceRight of empty array with no initial value");
    }
  }

  // 残りの要素に対してコールバックを実行（逆順）
  for (; k >= 0; --k) {
    if (!obj->hasProperty(std::to_string(k))) {
      continue;
    }

    ValuePtr currentValue = obj->get(std::to_string(k));
    if (!currentValue) {
      currentValue = Value::undefined();
    }

    // コールバック関数を呼び出し
    std::vector<ValuePtr> callbackArgs = {
        accumulator,                                // アキュムレータ
        currentValue,                               // 現在の値
        Value::fromNumber(static_cast<double>(k)),  // インデックス
        arguments[0]                                // 配列自体
    };

    accumulator = callFunction(callbackFn, Value::undefined(), callbackArgs);
  }

  return accumulator;
}

}  // namespace core
}  // namespace aerojs