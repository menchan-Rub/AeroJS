/**
 * @file array_manipulation.cpp
 * @brief JavaScript の Array オブジェクトの操作メソッド実装
 */

#include <algorithm>
#include <functional>
#include <stdexcept>

#include "../../../object.h"
#include "../../../value.h"
#include "../function/function.h"
#include "array.h"

namespace aerojs {
namespace core {

ValuePtr Array::push(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.push called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 追加する要素がない場合は現在の長さを返す
  if (arguments.size() <= 1) {
    return Value::fromNumber(static_cast<double>(length));
  }

  // 引数で渡された要素を配列の末尾に追加
  for (size_t i = 1; i < arguments.size(); ++i) {
    std::string key = std::to_string(length);
    obj->set(key, arguments[i]);
    ++length;
  }

  // 更新された長さをセット
  obj->set("length", Value::fromNumber(static_cast<double>(length)));

  // 新しい長さを返す
  return Value::fromNumber(static_cast<double>(length));
}

ValuePtr Array::pop(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.pop called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 配列が空の場合は undefined を返す
  if (length == 0) {
    obj->set("length", Value::fromNumber(0));
    return Value::undefined();
  }

  // 最後の要素のインデックス
  uint32_t lastIndex = length - 1;
  std::string lastKey = std::to_string(lastIndex);

  // 最後の要素を取得
  ValuePtr lastElement = Value::undefined();
  if (obj->hasProperty(lastKey)) {
    lastElement = obj->get(lastKey);
  }

  // 最後の要素を削除
  obj->deleteProperty(lastKey);

  // 長さを1減らす
  obj->set("length", Value::fromNumber(static_cast<double>(lastIndex)));

  // 削除した要素を返す
  return lastElement;
}

ValuePtr Array::shift(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.shift called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 配列が空の場合は undefined を返す
  if (length == 0) {
    return Value::undefined();
  }

  // 最初の要素を取得
  ValuePtr firstElement = Value::undefined();
  if (obj->hasProperty("0")) {
    firstElement = obj->get("0");
  }

  // 要素を前にシフト
  for (uint32_t i = 1; i < length; ++i) {
    std::string currentKey = std::to_string(i);
    std::string prevKey = std::to_string(i - 1);

    if (obj->hasProperty(currentKey)) {
      obj->set(prevKey, obj->get(currentKey));
    } else {
      obj->deleteProperty(prevKey);
    }
  }

  // 最後の要素を削除
  std::string lastKey = std::to_string(length - 1);
  obj->deleteProperty(lastKey);

  // 長さを1減らす
  obj->set("length", Value::fromNumber(static_cast<double>(length - 1)));

  // 削除した最初の要素を返す
  return firstElement;
}

ValuePtr Array::unshift(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.unshift called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 追加する要素の数
  uint32_t argCount = arguments.size() - 1;

  // 追加する要素がない場合は現在の長さを返す
  if (argCount == 0) {
    return Value::fromNumber(static_cast<double>(length));
  }

  // 既存の要素を後ろにシフト
  for (int32_t i = static_cast<int32_t>(length) - 1; i >= 0; --i) {
    std::string currentKey = std::to_string(i);
    std::string newKey = std::to_string(i + argCount);

    if (obj->hasProperty(currentKey)) {
      obj->set(newKey, obj->get(currentKey));
    }
  }

  // 引数で渡された要素を配列の先頭に追加
  for (uint32_t i = 0; i < argCount; ++i) {
    std::string key = std::to_string(i);
    obj->set(key, arguments[i + 1]);
  }

  // 更新された長さをセット
  uint32_t newLength = length + argCount;
  obj->set("length", Value::fromNumber(static_cast<double>(newLength)));

  // 新しい長さを返す
  return Value::fromNumber(static_cast<double>(newLength));
}

ValuePtr Array::splice(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.splice called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 開始インデックスが提供されていない場合はデフォルトで0
  int32_t start = 0;
  if (arguments.size() > 1) {
    start = static_cast<int32_t>(arguments[1]->toInteger());
  }

  // 負のインデックスは配列の末尾からの相対位置
  if (start < 0) {
    start = std::max(static_cast<int32_t>(length) + start, 0);
  } else {
    start = std::min(start, static_cast<int32_t>(length));
  }

  // 削除する要素数が提供されていない場合は残りすべて
  uint32_t deleteCount = length - start;
  if (arguments.size() > 2) {
    int32_t count = static_cast<int32_t>(arguments[2]->toInteger());
    deleteCount = std::max(0, std::min(count, static_cast<int32_t>(length - start)));
  }

  // 削除される要素を保持する新しい配列を作成
  ArrayPtr deletedElements = std::make_shared<Array>();
  deletedElements->set("length", Value::fromNumber(static_cast<double>(deleteCount)));

  // 削除される要素をコピー
  for (uint32_t i = 0; i < deleteCount; ++i) {
    std::string srcKey = std::to_string(start + i);
    std::string dstKey = std::to_string(i);

    if (obj->hasProperty(srcKey)) {
      deletedElements->set(dstKey, obj->get(srcKey));
    }
  }

  // 追加する要素の数
  uint32_t itemCount = arguments.size() > 3 ? arguments.size() - 3 : 0;

  // 要素をシフトする必要がある場合
  if (itemCount != deleteCount) {
    if (itemCount < deleteCount) {
      // 要素を前にシフト
      for (uint32_t i = start + deleteCount; i < length; ++i) {
        std::string srcKey = std::to_string(i);
        std::string dstKey = std::to_string(i - deleteCount + itemCount);

        if (obj->hasProperty(srcKey)) {
          obj->set(dstKey, obj->get(srcKey));
        } else {
          obj->deleteProperty(dstKey);
        }
      }

      // 不要な要素を削除
      for (uint32_t i = length - deleteCount + itemCount; i < length; ++i) {
        std::string key = std::to_string(i);
        obj->deleteProperty(key);
      }
    } else {
      // 要素を後ろにシフト
      for (int32_t i = static_cast<int32_t>(length) - 1; i >= static_cast<int32_t>(start + deleteCount); --i) {
        std::string srcKey = std::to_string(i);
        std::string dstKey = std::to_string(i + itemCount - deleteCount);

        if (obj->hasProperty(srcKey)) {
          obj->set(dstKey, obj->get(srcKey));
        } else {
          obj->deleteProperty(dstKey);
        }
      }
    }
  }

  // 新しい要素を挿入
  for (uint32_t i = 0; i < itemCount; ++i) {
    std::string key = std::to_string(start + i);
    obj->set(key, arguments[i + 3]);
  }

  // 新しい長さをセット
  uint32_t newLength = length - deleteCount + itemCount;
  obj->set("length", Value::fromNumber(static_cast<double>(newLength)));

  // 削除された要素の配列を返す
  return deletedElements;
}

ValuePtr Array::concat(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.concat called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // 新しい配列を作成
  ArrayPtr result = std::make_shared<Array>();
  uint32_t resultIndex = 0;

  // まず this の要素をコピー
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  for (uint32_t i = 0; i < length; ++i) {
    std::string key = std::to_string(i);

    if (obj->hasProperty(key)) {
      result->set(std::to_string(resultIndex), obj->get(key));
      ++resultIndex;
    }
  }

  // 引数で渡された配列や要素を連結
  for (size_t argIndex = 1; argIndex < arguments.size(); ++argIndex) {
    ValuePtr arg = arguments[argIndex];

    // null または undefined の場合はスキップ
    if (!arg || arg->isNull() || arg->isUndefined()) {
      continue;
    }

    // 引数が配列または配列のようなオブジェクトの場合
    if (arg->isObject() && arg->toObject()->hasProperty("length")) {
      ObjectPtr argObj = arg->toObject();
      ValuePtr argLengthValue = argObj->get("length");
      uint32_t argLength = static_cast<uint32_t>(argLengthValue->toNumber());

      for (uint32_t i = 0; i < argLength; ++i) {
        std::string key = std::to_string(i);

        if (argObj->hasProperty(key)) {
          result->set(std::to_string(resultIndex), argObj->get(key));
          ++resultIndex;
        }
      }
    } else {
      // 配列でない場合は単一の要素として追加
      result->set(std::to_string(resultIndex), arg);
      ++resultIndex;
    }
  }

  // 結果配列の長さをセット
  result->set("length", Value::fromNumber(static_cast<double>(resultIndex)));

  return result;
}

ValuePtr Array::slice(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.slice called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 開始インデックスが提供されていない場合はデフォルトで0
  int32_t start = 0;
  if (arguments.size() > 1) {
    start = static_cast<int32_t>(arguments[1]->toInteger());
  }

  // 負のインデックスは配列の末尾からの相対位置
  if (start < 0) {
    start = std::max(static_cast<int32_t>(length) + start, 0);
  } else {
    start = std::min(start, static_cast<int32_t>(length));
  }

  // 終了インデックスが提供されていない場合はデフォルトで配列の長さ
  int32_t end = static_cast<int32_t>(length);
  if (arguments.size() > 2 && !arguments[2]->isUndefined()) {
    end = static_cast<int32_t>(arguments[2]->toInteger());
  }

  // 負のインデックスは配列の末尾からの相対位置
  if (end < 0) {
    end = std::max(static_cast<int32_t>(length) + end, 0);
  } else {
    end = std::min(end, static_cast<int32_t>(length));
  }

  // 開始インデックスが終了インデックスより大きい場合は空の配列を返す
  if (start >= end) {
    ArrayPtr emptyArray = std::make_shared<Array>();
    emptyArray->set("length", Value::fromNumber(0));
    return emptyArray;
  }

  // 新しい配列を作成
  uint32_t count = static_cast<uint32_t>(end - start);
  ArrayPtr result = std::make_shared<Array>();

  // 指定範囲の要素をコピー
  for (uint32_t i = 0; i < count; ++i) {
    std::string srcKey = std::to_string(start + i);
    std::string dstKey = std::to_string(i);

    if (obj->hasProperty(srcKey)) {
      result->set(dstKey, obj->get(srcKey));
    }
  }

  // 結果配列の長さをセット
  result->set("length", Value::fromNumber(static_cast<double>(count)));

  return result;
}

ValuePtr Array::reverse(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.reverse called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 空の配列または単一要素の配列の場合は変更不要
  if (length <= 1) {
    return obj;
  }

  // 配列を反転
  for (uint32_t i = 0; i < length / 2; ++i) {
    uint32_t j = length - i - 1;
    std::string iKey = std::to_string(i);
    std::string jKey = std::to_string(j);

    // i および j の位置にプロパティが存在するかチェック
    bool hasI = obj->hasProperty(iKey);
    bool hasJ = obj->hasProperty(jKey);

    if (hasI && hasJ) {
      // 両方のプロパティが存在する場合は交換
      ValuePtr temp = obj->get(iKey);
      obj->set(iKey, obj->get(jKey));
      obj->set(jKey, temp);
    } else if (hasI) {
      // i のみにプロパティが存在する場合
      obj->set(jKey, obj->get(iKey));
      obj->deleteProperty(iKey);
    } else if (hasJ) {
      // j のみにプロパティが存在する場合
      obj->set(iKey, obj->get(jKey));
      obj->deleteProperty(jKey);
    }
    // 両方存在しない場合は何もしない
  }

  // this を返す
  return obj;
}

ValuePtr Array::sort(const std::vector<ValuePtr>& arguments) {
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("Array.prototype.sort called on null or undefined");
  }

  // this 値をオブジェクトに変換
  ObjectPtr obj = arguments[0]->toObject();

  // 配列の長さを取得
  ValuePtr lengthValue = obj->get("length");
  uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());

  // 空の配列または単一要素の配列の場合は変更不要
  if (length <= 1) {
    return obj;
  }

  // コンパレータ関数
  std::function<int(const ValuePtr&, const ValuePtr&)> compareFn;

  // 比較関数が提供されている場合
  if (arguments.size() > 1 && arguments[1] && arguments[1]->isFunction()) {
    FunctionPtr compareFunction = std::dynamic_pointer_cast<Function>(arguments[1]);

    compareFn = [&compareFunction](const ValuePtr& a, const ValuePtr& b) -> int {
      // undefined は常に末尾へ
      if (a->isUndefined() && b->isUndefined()) return 0;
      if (a->isUndefined()) return 1;
      if (b->isUndefined()) return -1;

      std::vector<ValuePtr> args = {a, b};
      ValuePtr result = compareFunction->call(Value::undefined(), args);
      double numResult = result->toNumber();

      if (std::isnan(numResult)) return 0;
      if (numResult < 0) return -1;
      if (numResult > 0) return 1;
      return 0;
    };
  } else {
    // デフォルトの比較関数（文字列の辞書順）
    compareFn = [](const ValuePtr& a, const ValuePtr& b) -> int {
      // undefined は常に末尾へ
      if (a->isUndefined() && b->isUndefined()) return 0;
      if (a->isUndefined()) return 1;
      if (b->isUndefined()) return -1;

      std::string aStr = a->toString();
      std::string bStr = b->toString();

      return aStr.compare(bStr);
    };
  }

  // 配列の要素を取得
  std::vector<ValuePtr> elements;
  for (uint32_t i = 0; i < length; ++i) {
    std::string key = std::to_string(i);
    if (obj->hasProperty(key)) {
      elements.push_back(obj->get(key));
    } else {
      elements.push_back(Value::undefined());
    }
  }

  // 要素をソート
  std::stable_sort(elements.begin(), elements.end(),
                   [&compareFn](const ValuePtr& a, const ValuePtr& b) {
                     return compareFn(a, b) < 0;
                   });

  // ソートされた要素を配列に戻す
  for (uint32_t i = 0; i < elements.size(); ++i) {
    std::string key = std::to_string(i);
    obj->set(key, elements[i]);
  }

  // this を返す
  return obj;
}

}  // namespace core
}  // namespace aerojs