/**
 * @file array_search.cpp
 * @brief JavaScript の Array オブジェクトの検索メソッド実装
 */

#include <cmath>
#include <stdexcept>

#include "../../../object.h"
#include "../../../value.h"
#include "../function/function.h"
#include "array.h"

namespace aerojs {
namespace core {

ValuePtr Array::find(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.find called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // コールバック関数が提供されていない場合はエラー
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isFunction()) {
    throw std::runtime_error("Array.prototype.find: callback must be a function");
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
      ValuePtr result = callback->call(thisArg, callbackArgs);

      // コールバックが true を返した場合、現在の要素を返す
      if (result->toBoolean()) {
        return currentValue;
      }
    }
  }

  // 見つからなかった場合は undefined を返す
  return Value::undefined();
}

ValuePtr Array::findIndex(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.findIndex called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // コールバック関数が提供されていない場合はエラー
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isFunction()) {
    throw std::runtime_error("Array.prototype.findIndex: callback must be a function");
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
      ValuePtr result = callback->call(thisArg, callbackArgs);

      // コールバックが true を返した場合、現在のインデックスを返す
      if (result->toBoolean()) {
        return Value::fromNumber(static_cast<double>(i));
      }
    }
  }

  // 見つからなかった場合は -1 を返す
  return Value::fromNumber(-1.0);
}

ValuePtr Array::indexOf(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.indexOf called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // 検索する要素が提供されていない場合は undefined を使用
  ValuePtr searchElement = Value::undefined();
  if (arguments.size() > 1) {
    searchElement = arguments[1];
  }

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 検索開始インデックスを取得（オプション）
  int32_t fromIndex = 0;
  if (arguments.size() > 2) {
    double index = arguments[2]->toNumber();

    // NaN の場合は 0 とする
    if (std::isnan(index)) {
      fromIndex = 0;
    } else {
      // インデックスを整数に変換
      fromIndex = static_cast<int32_t>(index);
    }
  }

  // 負のインデックスは配列の末尾からの相対位置
  if (fromIndex < 0) {
    fromIndex = static_cast<int32_t>(length) + fromIndex;

    // 負の結果の場合は 0 から開始
    if (fromIndex < 0) {
      fromIndex = 0;
    }
  }

  // fromIndex が配列の長さ以上の場合は要素は見つからない
  if (static_cast<uint32_t>(fromIndex) >= length) {
    return Value::fromNumber(-1.0);
  }

  // 各要素を検索
  for (uint32_t i = static_cast<uint32_t>(fromIndex); i < length; ++i) {
    std::string key = std::to_string(i);

    // プロパティが存在する場合のみ処理
    if (obj->hasProperty(key)) {
      ValuePtr currentValue = obj->get(key);

      // 厳密等価で比較
      if (currentValue->strictEquals(searchElement)) {
        return Value::fromNumber(static_cast<double>(i));
      }
    }
  }

  // 見つからなかった場合は -1 を返す
  return Value::fromNumber(-1.0);
}

ValuePtr Array::lastIndexOf(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.lastIndexOf called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // 検索する要素が提供されていない場合は undefined を使用
  ValuePtr searchElement = Value::undefined();
  if (arguments.size() > 1) {
    searchElement = arguments[1];
  }

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 空の配列の場合は -1 を返す
  if (length == 0) {
    return Value::fromNumber(-1.0);
  }

  // 検索開始インデックスを取得（オプション）
  int32_t fromIndex = static_cast<int32_t>(length) - 1;
  if (arguments.size() > 2) {
    double index = arguments[2]->toNumber();

    // NaN の場合は length - 1 とする
    if (std::isnan(index)) {
      fromIndex = static_cast<int32_t>(length) - 1;
    } else {
      // インデックスを整数に変換
      fromIndex = static_cast<int32_t>(index);
    }
  }

  // 負のインデックスは配列の末尾からの相対位置
  if (fromIndex < 0) {
    fromIndex = static_cast<int32_t>(length) + fromIndex;
  }

  // fromIndex が負の場合は要素は見つからない
  if (fromIndex < 0) {
    return Value::fromNumber(-1.0);
  }

  // fromIndex が配列の長さ以上の場合は length - 1 から開始
  if (static_cast<uint32_t>(fromIndex) >= length) {
    fromIndex = static_cast<int32_t>(length) - 1;
  }

  // 各要素を逆順に検索
  for (int32_t i = fromIndex; i >= 0; --i) {
    std::string key = std::to_string(i);

    // プロパティが存在する場合のみ処理
    if (obj->hasProperty(key)) {
      ValuePtr currentValue = obj->get(key);

      // 厳密等価で比較
      if (currentValue->strictEquals(searchElement)) {
        return Value::fromNumber(static_cast<double>(i));
      }
    }
  }

  // 見つからなかった場合は -1 を返す
  return Value::fromNumber(-1.0);
}

ValuePtr Array::includes(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.includes called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // 検索する要素が提供されていない場合は undefined を使用
  ValuePtr searchElement = Value::undefined();
  if (arguments.size() > 1) {
    searchElement = arguments[1];
  }

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 検索開始インデックスを取得（オプション）
  int32_t fromIndex = 0;
  if (arguments.size() > 2) {
    double index = arguments[2]->toNumber();

    // NaN の場合は 0 とする
    if (std::isnan(index)) {
      fromIndex = 0;
    } else {
      // インデックスを整数に変換
      fromIndex = static_cast<int32_t>(index);
    }
  }

  // 負のインデックスは配列の末尾からの相対位置
  if (fromIndex < 0) {
    fromIndex = static_cast<int32_t>(length) + fromIndex;

    // 負の結果の場合は 0 から開始
    if (fromIndex < 0) {
      fromIndex = 0;
    }
  }

  // fromIndex が配列の長さ以上の場合は要素は見つからない
  if (static_cast<uint32_t>(fromIndex) >= length) {
    return Value::fromBoolean(false);
  }

  // 各要素を検索
  for (uint32_t i = static_cast<uint32_t>(fromIndex); i < length; ++i) {
    std::string key = std::to_string(i);

    // プロパティが存在する場合のみ処理
    if (obj->hasProperty(key)) {
      ValuePtr currentValue = obj->get(key);

      // +0 と -0 を同一視するために特殊処理
      if (currentValue->isNumber() && searchElement->isNumber()) {
        double currentNum = currentValue->toNumber();
        double searchNum = searchElement->toNumber();

        if (currentNum == 0 && searchNum == 0) {
          return Value::fromBoolean(true);
        }
      }

      // NaN は NaN と等しいと考える
      if (currentValue->isNumber() && searchElement->isNumber() &&
          std::isnan(currentValue->toNumber()) && std::isnan(searchElement->toNumber())) {
        return Value::fromBoolean(true);
      }

      // 同値等価で比較
      if (currentValue->sameValueZero(searchElement)) {
        return Value::fromBoolean(true);
      }
    }
  }

  // 見つからなかった場合は false を返す
  return Value::fromBoolean(false);
}

ValuePtr Array::some(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.some called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // コールバック関数が提供されていない場合はエラー
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isFunction()) {
    throw std::runtime_error("Array.prototype.some: callback must be a function");
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

  // 配列が空の場合は false を返す
  if (length == 0) {
    return Value::fromBoolean(false);
  }

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

      // コールバックが true を返した場合、true を返す
      if (result->toBoolean()) {
        return Value::fromBoolean(true);
      }
    }
  }

  // どの要素もコールバックが true を返さなかった場合は false を返す
  return Value::fromBoolean(false);
}

ValuePtr Array::every(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.every called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // コールバック関数が提供されていない場合はエラー
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isFunction()) {
    throw std::runtime_error("Array.prototype.every: callback must be a function");
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

  // 配列が空の場合は true を返す
  if (length == 0) {
    return Value::fromBoolean(true);
  }

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

      // コールバックが false を返した場合、false を返す
      if (!result->toBoolean()) {
        return Value::fromBoolean(false);
      }
    }
  }

  // すべての要素がコールバックが true を返した場合は true を返す
  return Value::fromBoolean(true);
}

}  // namespace core
}  // namespace aerojs