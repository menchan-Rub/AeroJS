/**
 * @file number.cpp
 * @brief JavaScript の Number 組み込みオブジェクトの実装
 * 
 * ECMAScript 仕様に準拠したNumber組み込みオブジェクトの実装です。
 * IEEE 754倍精度浮動小数点数の処理とNumber関連の機能を提供します。
 */

#include "number.h"
#include "../../error.h"
#include "../../value.h"
#include "../../function.h"
#include "../../global_object.h"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <limits>
#include <locale>

namespace aerojs {
namespace core {

// 静的メンバの初期化
std::shared_ptr<Object> Number::s_prototype = nullptr;
std::shared_ptr<Function> Number::s_constructor = nullptr;
std::mutex Number::s_mutex;

Number::Number() : Object(), m_value(0.0) {
    setPrototype(getNumberPrototype());
}

Number::Number(double value) : Object(), m_value(value) {
    setPrototype(getNumberPrototype());
}

Number::~Number() = default;

double Number::getValue() const {
    return m_value;
}

double Number::toNumber() const {
    return m_value;
}

std::string Number::toString() const {
    std::ostringstream ss;
    
    // 特殊値の処理
    if (std::isnan(m_value)) {
        return "NaN";
    } else if (std::isinf(m_value)) {
        return m_value > 0 ? "Infinity" : "-Infinity";
    }
    
    // 整数部分と小数部分を取得
    double intPart;
    double fracPart = std::modf(m_value, &intPart);
    
    // 小数部分がない場合は整数として表示
    if (fracPart == 0.0) {
        ss << static_cast<int64_t>(intPart);
    } else {
        // 小数点以下の桁数を最適化して表示
        ss << std::setprecision(std::numeric_limits<double>::max_digits10) << m_value;
        
        // 末尾の不要な0を削除
        std::string result = ss.str();
        size_t pos = result.find_last_not_of('0');
        if (pos != std::string::npos && result[pos] == '.') {
            pos--;
        }
        return result.substr(0, pos + 1);
    }
    
    return ss.str();
}

std::shared_ptr<Object> Number::getNumberPrototype() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (!s_prototype) {
        initialize();
    }
    
    return s_prototype;
}

std::shared_ptr<Function> Number::getConstructor() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (!s_constructor) {
        initialize();
    }
    
    return s_constructor;
}

bool Number::isNumberObject(ObjectPtr obj) {
    if (!obj) {
        return false;
    }
    
    auto numObj = std::dynamic_pointer_cast<Number>(obj);
    return numObj != nullptr;
}

double Number::getNumberFromThis(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0]) {
        throw TypeError("Number.prototype method called on null or undefined");
    }
    
    if (arguments[0]->isNumber()) {
        return arguments[0]->toNumber();
    } else if (arguments[0]->isObject()) {
        ObjectPtr obj = std::static_pointer_cast<Object>(arguments[0]);
        if (isNumberObject(obj)) {
            return std::static_pointer_cast<Number>(obj)->getValue();
        }
    }
    
    throw TypeError("Number.prototype method called on non-Number object");
}

ValuePtr Number::construct(const std::vector<ValuePtr>& arguments, bool isConstructCall) {
    // 引数をNumber型に変換
    double value = 0.0;
    if (!arguments.empty() && arguments[0]) {
        value = arguments[0]->toNumber();
    }
    
    // コンストラクタとして呼び出された場合（new Number()）
    if (isConstructCall) {
        return std::make_shared<Number>(value);
    }
    
    // 関数として呼び出された場合（Number()）
    return Value::createNumber(value);
}

// 静的メソッドの実装

ValuePtr Number::isFinite(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0] || arguments[0]->getType() != ValueType::Number) {
        return Value::createBoolean(false);
    }
    
    double num = arguments[0]->toNumber();
    return Value::createBoolean(!std::isnan(num) && !std::isinf(num));
}

ValuePtr Number::isInteger(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0] || arguments[0]->getType() != ValueType::Number) {
        return Value::createBoolean(false);
    }
    
    double num = arguments[0]->toNumber();
    return Value::createBoolean(!std::isnan(num) && !std::isinf(num) && std::floor(num) == num);
}

ValuePtr Number::isNaN(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0] || arguments[0]->getType() != ValueType::Number) {
        return Value::createBoolean(false);
    }
    
    double num = arguments[0]->toNumber();
    return Value::createBoolean(std::isnan(num));
}

ValuePtr Number::isSafeInteger(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0] || arguments[0]->getType() != ValueType::Number) {
        return Value::createBoolean(false);
    }
    
    double num = arguments[0]->toNumber();
    return Value::createBoolean(
        !std::isnan(num) && 
        !std::isinf(num) && 
        std::floor(num) == num &&
        num >= MIN_SAFE_INTEGER && 
        num <= MAX_SAFE_INTEGER
    );
}

ValuePtr Number::parseFloat(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0]) {
        return Value::createNumber(NaN);
    }
    
    std::string str = arguments[0]->toString();
    
    // 先頭の空白を削除
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) {
        return Value::createNumber(NaN);
    }
    
    str = str.substr(start);
    
    // 特別な値の確認
    if (str.find("Infinity") == 0) {
        return Value::createNumber(POSITIVE_INFINITY);
    } else if (str.find("-Infinity") == 0) {
        return Value::createNumber(NEGATIVE_INFINITY);
    }
    
    try {
        size_t pos;
        double result = std::stod(str, &pos);
        return Value::createNumber(result);
    } catch (const std::exception&) {
        return Value::createNumber(NaN);
    }
}

ValuePtr Number::parseInt(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty() || !arguments[0]) {
        return Value::createNumber(NaN);
    }
    
    std::string str = arguments[0]->toString();
    
    // 先頭の空白を削除
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) {
        return Value::createNumber(NaN);
    }
    
    str = str.substr(start);
    
    // 基数を取得
    int radix = 10;
    if (arguments.size() > 1 && arguments[1]) {
        radix = static_cast<int>(arguments[1]->toNumber());
        if (radix == 0) {
            radix = 10;
        } else if (radix < 2 || radix > 36) {
            return Value::createNumber(NaN);
        }
    }
    
    // 0x または 0X で始まる場合、16進数として解析
    if ((radix == 0 || radix == 16) && str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        radix = 16;
        str = str.substr(2);
    }
    
    // 符号を抽出
    bool negative = false;
    if (!str.empty() && (str[0] == '+' || str[0] == '-')) {
        negative = (str[0] == '-');
        str = str.substr(1);
    }
    
    // 文字列が空になった場合
    if (str.empty()) {
        return Value::createNumber(NaN);
    }
    
    // 数値部分を解析
    try {
        size_t pos;
        double result = std::stoll(str, &pos, radix);
        return Value::createNumber(negative ? -result : result);
    } catch (const std::exception&) {
        // 数値部分がない場合はNaNを返す
        if (str.find_first_not_of("0123456789abcdefABCDEF") == 0) {
            return Value::createNumber(NaN);
        }
        
        // 部分的に解析できる場合は、解析できた部分まで
        std::string validPart;
        for (char c : str) {
            if ((c >= '0' && c <= '9') || 
                (radix > 10 && ((c >= 'a' && c <= ('a' + radix - 11)) || 
                                (c >= 'A' && c <= ('A' + radix - 11))))) {
                validPart += c;
            } else {
                break;
            }
        }
        
        if (validPart.empty()) {
            return Value::createNumber(NaN);
        }
        
        double result = std::stoll(validPart, nullptr, radix);
        return Value::createNumber(negative ? -result : result);
    }
}

// インスタンスメソッドの実装

ValuePtr Number::toExponential(const std::vector<ValuePtr>& arguments) {
    // this の値を取得
    double value;
    try {
        value = getNumberFromThis(arguments);
    } catch (const TypeError& e) {
        throw TypeError("Number.prototype.toExponential called on non-Number value");
    }
    
    // NaN, Infinity の処理
    if (std::isnan(value)) {
        return Value::createString("NaN");
    } else if (std::isinf(value)) {
        return Value::createString(value > 0 ? "Infinity" : "-Infinity");
    }
    
    // 桁数を取得
    int fractionDigits = -1;  // 未指定の場合は-1
    if (arguments.size() > 1 && arguments[1] && !arguments[1]->isUndefined()) {
        fractionDigits = static_cast<int>(arguments[1]->toNumber());
        
        // 有効範囲外の場合はエラー
        if (fractionDigits < 0 || fractionDigits > 20) {
            throw RangeError("fractionDigits out of range");
        }
    }
    
    std::ostringstream ss;
    if (fractionDigits >= 0) {
        // 指定された桁数
        ss << std::scientific << std::setprecision(fractionDigits) << value;
    } else {
        // 未指定の場合は必要な桁数
        ss << std::scientific << std::setprecision(std::numeric_limits<double>::max_digits10 - 6) << value;
    }
    
    // C++の指数表記（e+00）をJavaScript形式に変換
    std::string result = ss.str();
    size_t ePos = result.find('e');
    
    if (ePos != std::string::npos) {
        // 指数部分を修正
        size_t signPos = result.find_first_of("+-", ePos);
        if (signPos != std::string::npos) {
            std::string exponent = result.substr(signPos + 1);
            // 先頭の0を削除
            exponent = std::to_string(std::stoi(exponent));
            result = result.substr(0, signPos + 1) + exponent;
        }
    }
    
    return Value::createString(result);
}

ValuePtr Number::toFixed(const std::vector<ValuePtr>& arguments) {
    // this の値を取得
    double value;
    try {
        value = getNumberFromThis(arguments);
    } catch (const TypeError& e) {
        throw TypeError("Number.prototype.toFixed called on non-Number value");
    }
    
    // NaN, Infinity の処理
    if (std::isnan(value)) {
        return Value::createString("NaN");
    } else if (std::isinf(value)) {
        return Value::createString(value > 0 ? "Infinity" : "-Infinity");
    }
    
    // 桁数を取得
    int fractionDigits = 0;
    if (arguments.size() > 1 && arguments[1] && !arguments[1]->isUndefined()) {
        fractionDigits = static_cast<int>(arguments[1]->toNumber());
        
        // 有効範囲外の場合はエラー
        if (fractionDigits < 0 || fractionDigits > 20) {
            throw RangeError("fractionDigits out of range");
        }
    }
    
    // 値が大きすぎる場合は指数表記に
    if (std::abs(value) >= 1e21) {
        return Value::createString(std::to_string(value));
    }
    
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(fractionDigits) << value;
    
    return Value::createString(ss.str());
}

ValuePtr Number::toLocaleString(const std::vector<ValuePtr>& arguments) {
    // 現在の実装では標準のtoString()と同じ
    // 将来的にはロケールに基づいた書式設定を実装する
    return toStringMethod(arguments);
}

ValuePtr Number::toPrecision(const std::vector<ValuePtr>& arguments) {
    // this の値を取得
    double value;
    try {
        value = getNumberFromThis(arguments);
    } catch (const TypeError& e) {
        throw TypeError("Number.prototype.toPrecision called on non-Number value");
    }
    
    // 引数がなければtoString()を使用
    if (arguments.size() <= 1 || arguments[1]->isUndefined()) {
        return Value::createString(Number(value).toString());
    }
    
    // NaN, Infinity の処理
    if (std::isnan(value)) {
        return Value::createString("NaN");
    } else if (std::isinf(value)) {
        return Value::createString(value > 0 ? "Infinity" : "-Infinity");
    }
    
    // 精度を取得
    int precision = static_cast<int>(arguments[1]->toNumber());
    
    // 有効範囲外の場合はエラー
    if (precision < 1 || precision > 21) {
        throw RangeError("precision out of range");
    }
    
    // 値の大きさによって表記法を選択
    double absValue = std::abs(value);
    std::ostringstream ss;
    
    if (absValue >= 1e21 || (absValue > 0 && absValue < 1e-6)) {
        // 指数表記を使用
        ss << std::scientific << std::setprecision(precision - 1) << value;
    } else if (absValue < 1) {
        // 1未満の場合は小数点以下の桁数を調整
        int digits = 0;
        double tempValue = absValue;
        while (tempValue < 1 && digits < 21) {
            tempValue *= 10;
            digits++;
        }
        
        ss << std::fixed << std::setprecision(precision - 1 + digits) << value;
        
        // 末尾の0を削除
        std::string result = ss.str();
        size_t pos = result.find_last_not_of('0');
        if (pos != std::string::npos && result[pos] == '.') {
            pos--;
        }
        return Value::createString(result.substr(0, pos + 1));
    } else {
        // 整数部分の桁数を数える
        int digits = static_cast<int>(std::log10(absValue)) + 1;
        
        if (digits >= precision) {
            // 整数部分の桁数が指定精度以上の場合
            int round = precision - digits;
            double roundedValue = std::round(value * std::pow(10, round)) / std::pow(10, round);
            ss << std::fixed << std::setprecision(0) << roundedValue;
        } else {
            // 小数点以下の桁数を調整
            ss << std::fixed << std::setprecision(precision - digits) << value;
        }
    }
    
    // 指数表記のフォーマットを調整
    std::string result = ss.str();
    size_t ePos = result.find('e');
    
    if (ePos != std::string::npos) {
        // 指数部分を修正
        size_t signPos = result.find_first_of("+-", ePos);
        if (signPos != std::string::npos) {
            std::string exponent = result.substr(signPos + 1);
            // 先頭の0を削除
            exponent = std::to_string(std::stoi(exponent));
            result = result.substr(0, signPos + 1) + exponent;
        }
    }
    
    return Value::createString(result);
}

ValuePtr Number::toStringMethod(const std::vector<ValuePtr>& arguments) {
    // this の値を取得
    double value;
    try {
        value = getNumberFromThis(arguments);
    } catch (const TypeError& e) {
        throw TypeError("Number.prototype.toString called on non-Number value");
    }
    
    // 特殊値の処理
    if (std::isnan(value)) {
        return Value::createString("NaN");
    } else if (std::isinf(value)) {
        return Value::createString(value > 0 ? "Infinity" : "-Infinity");
    }
    
    // 基数を取得
    int radix = 10;
    if (arguments.size() > 1 && !arguments[1]->isUndefined()) {
        radix = static_cast<int>(arguments[1]->toNumber());
        
        // 有効範囲外の場合はエラー
        if (radix < 2 || radix > 36) {
            throw RangeError("toString() radix argument must be between 2 and 36");
        }
    }
    
    // 10進数の場合は標準の処理を使用
    if (radix == 10) {
        return Value::createString(Number(value).toString());
    }
    
    // 他の基数の場合
    std::string result;
    bool isNegative = (value < 0);
    double absValue = std::abs(value);
    int64_t intValue = static_cast<int64_t>(absValue);
    
    // 整数部分を変換
    if (intValue == 0) {
        result = "0";
    } else {
        static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
        while (intValue > 0) {
            result = digits[intValue % radix] + result;
            intValue /= radix;
        }
    }
    
    // 小数部分を変換
    double fracPart = absValue - std::floor(absValue);
    if (fracPart > 0) {
        result += ".";
        
        // 小数点以下最大20桁まで
        for (int i = 0; i < 20 && fracPart > 0; ++i) {
            fracPart *= radix;
            int digit = static_cast<int>(fracPart);
            result += digits[digit];
            fracPart -= digit;
            
            // 浮動小数点誤差を考慮
            if (fracPart < 1e-10) {
                break;
            }
        }
    }
    
    // 負の値の場合は符号を追加
    if (isNegative) {
        result = "-" + result;
    }
    
    return Value::createString(result);
}

ValuePtr Number::valueOf(const std::vector<ValuePtr>& arguments) {
    // this の値を取得
    double value;
    try {
        value = getNumberFromThis(arguments);
    } catch (const TypeError& e) {
        throw TypeError("Number.prototype.valueOf called on non-Number value");
    }
    
    return Value::createNumber(value);
}

void Number::initialize() {
    // ロックはgetNumberPrototype()またはgetConstructor()から取得済み
    
    // プロトタイプオブジェクトの作成
    s_prototype = std::make_shared<Object>();
    
    // コンストラクタ関数の作成
    s_constructor = std::make_shared<Function>("Number", 
        [](const std::vector<ValuePtr>& args, ValuePtr thisArg, bool isConstructCall) {
            return construct(args, isConstructCall);
        }, 
        1);
    
    // コンストラクタプロパティの設定
    s_prototype->set("constructor", s_constructor, PropertyAttribute::Writable | PropertyAttribute::Configurable);
    s_constructor->set("prototype", s_prototype, PropertyAttribute::None);
    
    // プロトタイプメソッドの登録
    s_prototype->set("toExponential", std::make_shared<Function>("toExponential", toExponential, 1), 
                    PropertyAttribute::Writable | PropertyAttribute::Configurable);
    s_prototype->set("toFixed", std::make_shared<Function>("toFixed", toFixed, 1), 
                    PropertyAttribute::Writable | PropertyAttribute::Configurable);
    s_prototype->set("toLocaleString", std::make_shared<Function>("toLocaleString", toLocaleString, 0), 
                    PropertyAttribute::Writable | PropertyAttribute::Configurable);
    s_prototype->set("toPrecision", std::make_shared<Function>("toPrecision", toPrecision, 1), 
                    PropertyAttribute::Writable | PropertyAttribute::Configurable);
    s_prototype->set("toString", std::make_shared<Function>("toString", toStringMethod, 1), 
                    PropertyAttribute::Writable | PropertyAttribute::Configurable);
    s_prototype->set("valueOf", std::make_shared<Function>("valueOf", valueOf, 0), 
                    PropertyAttribute::Writable | PropertyAttribute::Configurable);
    
    // 静的メソッドの登録
    s_constructor->set("isFinite", std::make_shared<Function>("isFinite", isFinite, 1), 
                      PropertyAttribute::Writable | PropertyAttribute::Configurable);
    s_constructor->set("isInteger", std::make_shared<Function>("isInteger", isInteger, 1), 
                      PropertyAttribute::Writable | PropertyAttribute::Configurable);
    s_constructor->set("isNaN", std::make_shared<Function>("isNaN", isNaN, 1), 
                      PropertyAttribute::Writable | PropertyAttribute::Configurable);
    s_constructor->set("isSafeInteger", std::make_shared<Function>("isSafeInteger", isSafeInteger, 1), 
                      PropertyAttribute::Writable | PropertyAttribute::Configurable);
    s_constructor->set("parseFloat", std::make_shared<Function>("parseFloat", parseFloat, 1), 
                      PropertyAttribute::Writable | PropertyAttribute::Configurable);
    s_constructor->set("parseInt", std::make_shared<Function>("parseInt", parseInt, 2), 
                      PropertyAttribute::Writable | PropertyAttribute::Configurable);
    
    // 定数の登録
    s_constructor->set("EPSILON", Value::createNumber(EPSILON), 
                      PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
    s_constructor->set("MAX_VALUE", Value::createNumber(MAX_VALUE), 
                      PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
    s_constructor->set("MIN_VALUE", Value::createNumber(MIN_VALUE), 
                      PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
    s_constructor->set("MAX_SAFE_INTEGER", Value::createNumber(MAX_SAFE_INTEGER), 
                      PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
    s_constructor->set("MIN_SAFE_INTEGER", Value::createNumber(MIN_SAFE_INTEGER), 
                      PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
    s_constructor->set("POSITIVE_INFINITY", Value::createNumber(POSITIVE_INFINITY), 
                      PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
    s_constructor->set("NEGATIVE_INFINITY", Value::createNumber(NEGATIVE_INFINITY), 
                      PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
    s_constructor->set("NaN", Value::createNumber(NaN), 
                      PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
}

} // namespace core
} // namespace aerojs 