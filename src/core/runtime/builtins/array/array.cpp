/**
 * @file array.cpp
 * @brief JavaScript の Array オブジェクトの実装
 */

#include "array.h"
#include "../../../object.h"
#include "../../../value.h"
#include "../function/function.h"
#include <stdexcept>
#include <algorithm>

namespace aerojs {
namespace core {

// 静的メンバ変数の初期化
std::shared_ptr<Array> Array::s_prototype = nullptr;
std::shared_ptr<Function> Array::s_constructor = nullptr;
std::mutex Array::s_mutex;

void Array::initialize() {
    std::unique_lock<std::mutex> lock(s_mutex);
    
    if (s_prototype != nullptr) {
        return; // すでに初期化済み
    }
    
    // Array.prototype の作成
    s_prototype = std::make_shared<Array>();
    s_prototype->set("length", Value::fromNumber(0));
    
    // Array コンストラクタの作成
    s_constructor = std::make_shared<Function>(Array::construct, "Array", 1);
    s_constructor->set("prototype", s_prototype);
    s_prototype->set("constructor", s_constructor);
    
    // プロトタイプメソッドの設定
    s_prototype->set("push", std::make_shared<Function>(Array::push, "push", 1));
    s_prototype->set("pop", std::make_shared<Function>(Array::pop, "pop", 0));
    s_prototype->set("shift", std::make_shared<Function>(Array::shift, "shift", 0));
    s_prototype->set("unshift", std::make_shared<Function>(Array::unshift, "unshift", 1));
    s_prototype->set("splice", std::make_shared<Function>(Array::splice, "splice", 2));
    s_prototype->set("concat", std::make_shared<Function>(Array::concat, "concat", 1));
    s_prototype->set("slice", std::make_shared<Function>(Array::slice, "slice", 2));
    s_prototype->set("reverse", std::make_shared<Function>(Array::reverse, "reverse", 0));
    s_prototype->set("sort", std::make_shared<Function>(Array::sort, "sort", 1));
    s_prototype->set("indexOf", std::make_shared<Function>(Array::indexOf, "indexOf", 1));
    s_prototype->set("lastIndexOf", std::make_shared<Function>(Array::lastIndexOf, "lastIndexOf", 1));
    s_prototype->set("includes", std::make_shared<Function>(Array::includes, "includes", 1));
    s_prototype->set("find", std::make_shared<Function>(Array::find, "find", 1));
    s_prototype->set("findIndex", std::make_shared<Function>(Array::findIndex, "findIndex", 1));
    s_prototype->set("some", std::make_shared<Function>(Array::some, "some", 1));
    s_prototype->set("every", std::make_shared<Function>(Array::every, "every", 1));
    s_prototype->set("forEach", std::make_shared<Function>(Array::forEach, "forEach", 1));
    s_prototype->set("map", std::make_shared<Function>(Array::map, "map", 1));
    s_prototype->set("filter", std::make_shared<Function>(Array::filter, "filter", 1));
    s_prototype->set("reduce", std::make_shared<Function>(Array::reduce, "reduce", 1));
    s_prototype->set("reduceRight", std::make_shared<Function>(Array::reduceRight, "reduceRight", 1));
    s_prototype->set("join", std::make_shared<Function>(Array::join, "join", 1));
    s_prototype->set("toString", std::make_shared<Function>(Array::toString, "toString", 0));
    s_prototype->set("toLocaleString", std::make_shared<Function>(Array::toLocaleString, "toLocaleString", 0));
    
    // 静的メソッドの設定
    s_constructor->set("isArray", std::make_shared<Function>(Array::isArray, "isArray", 1));
    s_constructor->set("from", std::make_shared<Function>(Array::from, "from", 1));
    s_constructor->set("of", std::make_shared<Function>(Array::of, "of", 0));
}

Array::Array() : Object(Array::getPrototype()) {
    set("length", Value::fromNumber(0));
}

Array::~Array() {
    // 特別な後処理が必要ない場合は空のままで問題ありません
}

ValuePtr Array::construct(const std::vector<ValuePtr>& arguments) {
    ArrayPtr array = std::make_shared<Array>();
    
    if (arguments.empty()) {
        // 引数なしの場合: new Array() -> 空の配列
        array->set("length", Value::fromNumber(0));
    } else if (arguments.size() == 1) {
        // 引数が1つの場合: new Array(length) -> 指定された長さの空の配列
        ValuePtr arg = arguments[0];
        if (arg->isNumber()) {
            double numArg = arg->toNumber();
            
            // 整数でない場合、または範囲外の場合はエラー
            if (numArg != std::floor(numArg) || numArg < 0 || numArg > UINT32_MAX) {
                throw std::runtime_error("Invalid array length");
            }
            
            array->set("length", Value::fromNumber(numArg));
        } else {
            // 数値でない場合は要素として扱う
            array->set("0", arg);
            array->set("length", Value::fromNumber(1));
        }
    } else {
        // 引数が複数の場合: new Array(element0, element1, ...) -> 要素を持つ配列
        for (size_t i = 0; i < arguments.size(); ++i) {
            array->set(std::to_string(i), arguments[i]);
        }
        array->set("length", Value::fromNumber(static_cast<double>(arguments.size())));
    }
    
    return array;
}

ValuePtr Array::isArray(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromBoolean(false);
    }
    
    ValuePtr arg = arguments[0];
    if (!arg || !arg->isObject()) {
        return Value::fromBoolean(false);
    }
    
    // objectが配列かどうかを判定
    return Value::fromBoolean(std::dynamic_pointer_cast<Array>(arg) != nullptr);
}

ValuePtr Array::from(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
        throw std::runtime_error("Cannot convert undefined or null to object");
    }
    
    ObjectPtr items = arguments[0]->toObject();
    FunctionPtr mapFn = nullptr;
    ValuePtr thisArg = Value::undefined();
    
    // マッピング関数が指定されているかチェック
    if (arguments.size() > 1 && arguments[1] && !arguments[1]->isUndefined()) {
        if (!arguments[1]->isFunction()) {
            throw std::runtime_error("Array.from: mapFn is not callable");
        }
        mapFn = std::dynamic_pointer_cast<Function>(arguments[1]);
        
        // thisArg が指定されているかチェック
        if (arguments.size() > 2) {
            thisArg = arguments[2];
        }
    }
    
    // items に length プロパティがあるかチェック
    if (!items->hasProperty("length")) {
        throw std::runtime_error("Object is not iterable");
    }
    
    // 配列のような長さを持つオブジェクトから新しい配列を作成
    ValuePtr lengthValue = items->get("length");
    uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());
    
    ArrayPtr result = std::make_shared<Array>();
    
    for (uint32_t i = 0; i < length; ++i) {
        std::string key = std::to_string(i);
        
        ValuePtr value = Value::undefined();
        if (items->hasProperty(key)) {
            value = items->get(key);
        }
        
        // マッピング関数が提供されている場合はそれを適用
        if (mapFn) {
            std::vector<ValuePtr> args = {value, Value::fromNumber(static_cast<double>(i))};
            value = mapFn->call(thisArg, args);
        }
        
        result->set(key, value);
    }
    
    result->set("length", Value::fromNumber(static_cast<double>(length)));
    
    return result;
}

ValuePtr Array::of(const std::vector<ValuePtr>& arguments) {
    // 全ての引数から新しい配列を作成
    ArrayPtr result = std::make_shared<Array>();
    
    for (size_t i = 0; i < arguments.size(); ++i) {
        result->set(std::to_string(i), arguments[i]);
    }
    
    result->set("length", Value::fromNumber(static_cast<double>(arguments.size())));
    
    return result;
}

ValuePtr Array::join(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
        throw std::runtime_error("Array.prototype.join called on null or undefined");
    }
    
    // this 値をオブジェクトに変換
    ObjectPtr obj = arguments[0]->toObject();
    
    // 配列の長さを取得
    ValuePtr lengthValue = obj->get("length");
    uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());
    
    // 区切り文字を取得 (デフォルトはカンマ)
    std::string separator = ",";
    if (arguments.size() > 1 && arguments[1] && !arguments[1]->isUndefined()) {
        separator = arguments[1]->toString();
    }
    
    std::string result;
    
    for (uint32_t i = 0; i < length; ++i) {
        if (i > 0) {
            result += separator;
        }
        
        std::string key = std::to_string(i);
        if (obj->hasProperty(key)) {
            ValuePtr element = obj->get(key);
            
            // null または undefined の場合は空文字列に
            if (!element || element->isNull() || element->isUndefined()) {
                // 何も追加しない
            } else {
                result += element->toString();
            }
        }
    }
    
    return Value::fromString(result);
}

ValuePtr Array::toString(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
        throw std::runtime_error("Array.prototype.toString called on null or undefined");
    }
    
    // join メソッドを使用して文字列に変換
    std::vector<ValuePtr> joinArgs = {arguments[0]};
    return Array::join(joinArgs);
}

ValuePtr Array::toLocaleString(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
        throw std::runtime_error("Array.prototype.toLocaleString called on null or undefined");
    }
    
    // this 値をオブジェクトに変換
    ObjectPtr obj = arguments[0]->toObject();
    
    // 配列の長さを取得
    ValuePtr lengthValue = obj->get("length");
    uint32_t length = static_cast<uint32_t>(lengthValue->toNumber());
    
    std::string result;
    
    for (uint32_t i = 0; i < length; ++i) {
        if (i > 0) {
            result += ",";
        }
        
        std::string key = std::to_string(i);
        if (obj->hasProperty(key)) {
            ValuePtr element = obj->get(key);
            
            // null または undefined の場合は空文字列に
            if (!element || element->isNull() || element->isUndefined()) {
                // 何も追加しない
            } else if (element->isObject()) {
                // オブジェクトの toLocaleString メソッドを呼び出し
                ObjectPtr elemObj = std::dynamic_pointer_cast<Object>(element);
                if (elemObj->hasProperty("toLocaleString")) {
                    ValuePtr toLocaleStringFn = elemObj->get("toLocaleString");
                    if (toLocaleStringFn && toLocaleStringFn->isFunction()) {
                        FunctionPtr fn = std::dynamic_pointer_cast<Function>(toLocaleStringFn);
                        std::vector<ValuePtr> callArgs = {element};
                        ValuePtr strValue = fn->call(element, callArgs);
                        result += strValue->toString();
                    } else {
                        result += element->toString();
                    }
                } else {
                    result += element->toString();
                }
            } else {
                result += element->toString();
            }
        }
    }
    
    return Value::fromString(result);
}

std::shared_ptr<Object> Array::getPrototype() {
    std::unique_lock<std::mutex> lock(s_mutex);
    
    if (!s_prototype) {
        initialize();
    }
    
    return s_prototype;
}

std::shared_ptr<Function> Array::getConstructor() {
    std::unique_lock<std::mutex> lock(s_mutex);
    
    if (!s_constructor) {
        initialize();
    }
    
    return s_constructor;
}

} // namespace core
} // namespace aerojs 