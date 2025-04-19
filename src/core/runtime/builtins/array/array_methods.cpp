/**
 * @file array_methods.cpp
 * @brief JavaScript の Array オブジェクトのメソッド実装（基本操作）
 */

#include "array.h"
#include "../../../object.h"
#include "../../../value.h"
#include "../../../function.h"
#include <algorithm>
#include <cmath>

namespace aerojs {
namespace core {

ValuePtr Array::shift(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
        throw std::runtime_error("Array.prototype.shift called on null or undefined");
    }
    
    // this 値をオブジェクトに変換
    ObjectPtr obj = arguments[0]->toObject();
    
    // length を取得
    ValuePtr lengthValue = obj->get("length");
    uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());
    
    // 配列が空の場合は undefined を返す
    if (length == 0) {
        return Value::undefined();
    }
    
    // 最初の要素を取得
    ValuePtr first = obj->get("0");
    
    // 要素をシフト
    for (uint32_t i = 1; i < length; ++i) {
        ValuePtr element = obj->get(std::to_string(i));
        if (element) {
            obj->defineProperty(std::to_string(i - 1), element,
                               PropertyAttributes::Writable | PropertyAttributes::Enumerable | PropertyAttributes::Configurable);
        } else {
            obj->deleteProperty(std::to_string(i - 1));
        }
    }
    
    // 最後の要素を削除
    obj->deleteProperty(std::to_string(length - 1));
    
    // length を更新
    obj->defineProperty("length", Value::fromNumber(static_cast<double>(length - 1)),
                       PropertyAttributes::Writable);
    
    return first ? first : Value::undefined();
}

ValuePtr Array::unshift(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
        throw std::runtime_error("Array.prototype.unshift called on null or undefined");
    }
    
    // 追加する要素がなければ現在の長さを返す
    if (arguments.size() <= 1) {
        ValuePtr lengthValue = arguments[0]->toObject()->get("length");
        return lengthValue ? lengthValue : Value::fromNumber(0.0);
    }
    
    // this 値をオブジェクトに変換
    ObjectPtr obj = arguments[0]->toObject();
    
    // length を取得
    ValuePtr lengthValue = obj->get("length");
    uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());
    
    // 追加する要素数
    uint32_t argCount = arguments.size() - 1;
    
    // 要素を後ろにシフト
    for (int i = static_cast<int>(length) - 1; i >= 0; --i) {
        ValuePtr element = obj->get(std::to_string(i));
        if (element) {
            obj->defineProperty(std::to_string(i + argCount), element,
                               PropertyAttributes::Writable | PropertyAttributes::Enumerable | PropertyAttributes::Configurable);
        } else {
            obj->deleteProperty(std::to_string(i + argCount));
        }
    }
    
    // 新しい要素を先頭に追加
    for (uint32_t i = 0; i < argCount; ++i) {
        obj->defineProperty(std::to_string(i), arguments[i + 1],
                           PropertyAttributes::Writable | PropertyAttributes::Enumerable | PropertyAttributes::Configurable);
    }
    
    // length を更新
    uint32_t newLength = length + argCount;
    obj->defineProperty("length", Value::fromNumber(static_cast<double>(newLength)),
                       PropertyAttributes::Writable);
    
    return Value::fromNumber(static_cast<double>(newLength));
}

ValuePtr Array::slice(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
        throw std::runtime_error("Array.prototype.slice called on null or undefined");
    }
    
    // this 値をオブジェクトに変換
    ObjectPtr obj = arguments[0]->toObject();
    
    // length を取得
    ValuePtr lengthValue = obj->get("length");
    uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());
    
    // 開始位置を取得
    uint32_t start = 0;
    if (arguments.size() > 1 && arguments[1]) {
        double startDouble = arguments[1]->toNumber();
        if (startDouble < 0) {
            // 負の値は末尾からの相対位置
            start = static_cast<uint32_t>(std::max(static_cast<double>(length) + startDouble, 0.0));
        } else {
            start = static_cast<uint32_t>(std::min(startDouble, static_cast<double>(length)));
        }
    }
    
    // 終了位置を取得（デフォルトは配列の長さ）
    uint32_t end = length;
    if (arguments.size() > 2 && arguments[2] && !arguments[2]->isUndefined()) {
        double endDouble = arguments[2]->toNumber();
        if (endDouble < 0) {
            // 負の値は末尾からの相対位置
            end = static_cast<uint32_t>(std::max(static_cast<double>(length) + endDouble, 0.0));
        } else {
            end = static_cast<uint32_t>(std::min(endDouble, static_cast<double>(length)));
        }
    }
    
    // 新しい配列の長さを計算
    uint32_t newLength = end > start ? end - start : 0;
    
    // 新しい配列を作成
    std::vector<ValuePtr> elements;
    for (uint32_t i = 0; i < newLength; ++i) {
        ValuePtr element = obj->get(std::to_string(start + i));
        elements.push_back(element ? element : Value::undefined());
    }
    
    return Value::fromObject(createArrayFromValues(elements));
}

ValuePtr Array::splice(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
        throw std::runtime_error("Array.prototype.splice called on null or undefined");
    }
    
    // this 値をオブジェクトに変換
    ObjectPtr obj = arguments[0]->toObject();
    
    // length を取得
    ValuePtr lengthValue = obj->get("length");
    uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());
    
    // 引数がなければ空配列を返す
    if (arguments.size() <= 1) {
        return Value::fromObject(createArrayFromValues({}));
    }
    
    // 開始位置を取得
    uint32_t start = 0;
    if (arguments.size() > 1 && arguments[1]) {
        double startDouble = arguments[1]->toNumber();
        if (startDouble < 0) {
            // 負の値は末尾からの相対位置
            start = static_cast<uint32_t>(std::max(static_cast<double>(length) + startDouble, 0.0));
        } else {
            start = static_cast<uint32_t>(std::min(startDouble, static_cast<double>(length)));
        }
    }
    
    // 削除する要素数を取得
    uint32_t deleteCount = 0;
    if (arguments.size() > 2 && arguments[2]) {
        double deleteCountDouble = arguments[2]->toNumber();
        deleteCount = static_cast<uint32_t>(std::min(std::max(deleteCountDouble, 0.0), static_cast<double>(length - start)));
    } else {
        // 第2引数が省略された場合は、start以降のすべての要素を削除
        deleteCount = length - start;
    }
    
    // 削除される要素を保存
    std::vector<ValuePtr> deletedElements;
    for (uint32_t i = 0; i < deleteCount; ++i) {
        ValuePtr element = obj->get(std::to_string(start + i));
        deletedElements.push_back(element ? element : Value::undefined());
    }
    
    // 追加する要素数
    uint32_t insertCount = arguments.size() > 3 ? arguments.size() - 3 : 0;
    
    // 削除数と追加数の差
    int32_t diff = static_cast<int32_t>(insertCount) - static_cast<int32_t>(deleteCount);
    
    // 要素をシフト
    if (diff < 0) {
        // 削除する要素数の方が多い場合は要素を前にシフト
        for (uint32_t i = start + deleteCount; i < length; ++i) {
            ValuePtr element = obj->get(std::to_string(i));
            if (element) {
                obj->defineProperty(std::to_string(i + diff), element,
                                  PropertyAttributes::Writable | PropertyAttributes::Enumerable | PropertyAttributes::Configurable);
            } else {
                obj->deleteProperty(std::to_string(i + diff));
            }
        }
        
        // 余分な要素を削除
        for (uint32_t i = length; i > length + diff; --i) {
            obj->deleteProperty(std::to_string(i - 1));
        }
    } else if (diff > 0) {
        // 追加する要素数の方が多い場合は要素を後ろにシフト
        for (int32_t i = static_cast<int32_t>(length) - 1; i >= static_cast<int32_t>(start + deleteCount); --i) {
            ValuePtr element = obj->get(std::to_string(i));
            if (element) {
                obj->defineProperty(std::to_string(i + diff), element,
                                  PropertyAttributes::Writable | PropertyAttributes::Enumerable | PropertyAttributes::Configurable);
            } else {
                obj->deleteProperty(std::to_string(i + diff));
            }
        }
    }
    
    // 新しい要素を挿入
    for (uint32_t i = 0; i < insertCount; ++i) {
        obj->defineProperty(std::to_string(start + i), arguments[i + 3],
                          PropertyAttributes::Writable | PropertyAttributes::Enumerable | PropertyAttributes::Configurable);
    }
    
    // length を更新
    uint32_t newLength = length + diff;
    obj->defineProperty("length", Value::fromNumber(static_cast<double>(newLength)),
                       PropertyAttributes::Writable);
    
    // 削除された要素の配列を返す
    return Value::fromObject(createArrayFromValues(deletedElements));
}

ValuePtr Array::reverse(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
        throw std::runtime_error("Array.prototype.reverse called on null or undefined");
    }
    
    // this 値をオブジェクトに変換
    ObjectPtr obj = arguments[0]->toObject();
    
    // length を取得
    ValuePtr lengthValue = obj->get("length");
    uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());
    
    // 配列の要素を取得
    std::vector<ValuePtr> elements;
    for (uint32_t i = 0; i < length; ++i) {
        ValuePtr element = obj->get(std::to_string(i));
        elements.push_back(element ? element : Value::undefined());
    }
    
    // 要素を逆順に設定
    for (uint32_t i = 0; i < length; ++i) {
        uint32_t j = length - i - 1;
        ValuePtr element = elements[j];
        
        if (element->isUndefined()) {
            obj->deleteProperty(std::to_string(i));
        } else {
            obj->defineProperty(std::to_string(i), element,
                              PropertyAttributes::Writable | PropertyAttributes::Enumerable | PropertyAttributes::Configurable);
        }
    }
    
    // this オブジェクトを返す
    return arguments[0];
}

ValuePtr Array::concat(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
        throw std::runtime_error("Array.prototype.concat called on null or undefined");
    }
    
    // this 値をオブジェクトに変換
    ObjectPtr thisObj = arguments[0]->toObject();
    
    // 結果の配列
    std::vector<ValuePtr> resultElements;
    
    // this の要素を追加
    ValuePtr thisLengthValue = thisObj->get("length");
    uint32_t thisLength = static_cast<uint32_t>(thisLengthValue->toNumber());
    
    for (uint32_t i = 0; i < thisLength; ++i) {
        ValuePtr element = thisObj->get(std::to_string(i));
        resultElements.push_back(element ? element : Value::undefined());
    }
    
    // 引数の要素を追加
    for (size_t argIndex = 1; argIndex < arguments.size(); ++argIndex) {
        ValuePtr arg = arguments[argIndex];
        
        if (!arg || arg->isUndefined() || arg->isNull()) {
            // undefined や null はそのまま追加
            resultElements.push_back(arg);
            continue;
        }
        
        // 引数がArrayLike（Array または array-like）かどうかチェック
        bool isArrayLike = false;
        uint32_t argLength = 0;
        
        if (arg->isObject()) {
            ObjectPtr argObj = arg->toObject();
            isArrayLike = argObj->isArray();
            
            if (isArrayLike) {
                ValuePtr argLengthValue = argObj->get("length");
                argLength = static_cast<uint32_t>(argLengthValue->toNumber());
            }
        }
        
        if (isArrayLike) {
            // Array-like オブジェクトなら要素を順に追加
            ObjectPtr argObj = arg->toObject();
            for (uint32_t i = 0; i < argLength; ++i) {
                ValuePtr element = argObj->get(std::to_string(i));
                resultElements.push_back(element ? element : Value::undefined());
            }
        } else {
            // Array-like でないなら引数自体を追加
            resultElements.push_back(arg);
        }
    }
    
    // 新しい配列を作成して返す
    return Value::fromObject(createArrayFromValues(resultElements));
}

} // namespace core
} // namespace aerojs 