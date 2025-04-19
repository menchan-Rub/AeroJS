/**
 * @file math.cpp
 * @brief JavaScript の Math オブジェクトの実装
 */

#include "math.h"
#include "../../../object.h"
#include "../../../value.h"
#include "../../../function.h"
#include <random>
#include <limits>
#include <algorithm>
#include <ctime>

namespace aerojs {
namespace core {

// 乱数生成のためのメルセンヌ・ツイスター
// スレッドセーフのためにスレッドローカル変数として宣言
thread_local std::mt19937_64 s_randomEngine(
    static_cast<unsigned long>(std::time(nullptr)));
thread_local std::uniform_real_distribution<double> s_uniformDist(0.0, 1.0);

ObjectPtr Math::initialize(ObjectPtr globalObject) {
    // Math オブジェクトの作成
    auto mathObject = Object::create();
    
    // 定数のインストール
    installConstants(mathObject);
    
    // 関数のインストール
    installFunctions(mathObject);
    
    // グローバルオブジェクトに Math プロパティとして追加
    globalObject->defineProperty("Math", Value::fromObject(mathObject), 
                                PropertyAttributes::DontEnum | 
                                PropertyAttributes::DontDelete | 
                                PropertyAttributes::ReadOnly);
    
    return mathObject;
}

ObjectPtr Math::createPrototype() {
    // Math はコンストラクタではなく、単なるオブジェクトなのでプロトタイプは不要
    return nullptr;
}

ObjectPtr Math::createConstructor(ObjectPtr prototype) {
    // Math はコンストラクタではないので不要
    return nullptr;
}

void Math::installConstants(ObjectPtr mathObject) {
    // Math.E - 自然対数の底
    mathObject->defineProperty("E", Value::fromNumber(2.7182818284590452354),
                              PropertyAttributes::DontEnum | 
                              PropertyAttributes::DontDelete | 
                              PropertyAttributes::ReadOnly);
    
    // Math.LN10 - 10の自然対数
    mathObject->defineProperty("LN10", Value::fromNumber(2.302585092994046),
                              PropertyAttributes::DontEnum | 
                              PropertyAttributes::DontDelete | 
                              PropertyAttributes::ReadOnly);
    
    // Math.LN2 - 2の自然対数
    mathObject->defineProperty("LN2", Value::fromNumber(0.6931471805599453),
                              PropertyAttributes::DontEnum | 
                              PropertyAttributes::DontDelete | 
                              PropertyAttributes::ReadOnly);
    
    // Math.LOG10E - 底が10の対数におけるeの値
    mathObject->defineProperty("LOG10E", Value::fromNumber(0.4342944819032518),
                              PropertyAttributes::DontEnum | 
                              PropertyAttributes::DontDelete | 
                              PropertyAttributes::ReadOnly);
    
    // Math.LOG2E - 底が2の対数におけるeの値
    mathObject->defineProperty("LOG2E", Value::fromNumber(1.4426950408889634),
                              PropertyAttributes::DontEnum | 
                              PropertyAttributes::DontDelete | 
                              PropertyAttributes::ReadOnly);
    
    // Math.PI - 円周率
    mathObject->defineProperty("PI", Value::fromNumber(3.1415926535897932),
                              PropertyAttributes::DontEnum | 
                              PropertyAttributes::DontDelete | 
                              PropertyAttributes::ReadOnly);
    
    // Math.SQRT1_2 - 1/2の平方根
    mathObject->defineProperty("SQRT1_2", Value::fromNumber(0.7071067811865476),
                              PropertyAttributes::DontEnum | 
                              PropertyAttributes::DontDelete | 
                              PropertyAttributes::ReadOnly);
    
    // Math.SQRT2 - 2の平方根
    mathObject->defineProperty("SQRT2", Value::fromNumber(1.4142135623730951),
                              PropertyAttributes::DontEnum | 
                              PropertyAttributes::DontDelete | 
                              PropertyAttributes::ReadOnly);
}

void Math::installFunctions(ObjectPtr mathObject) {
    // ECMAScript の Math オブジェクトのすべてのメソッド
    auto defineMethod = [&](const std::string& name, MathFunction func, int length) {
        auto function = Function::create(name, func, length);
        mathObject->defineProperty(name, Value::fromObject(function),
                                   PropertyAttributes::DontEnum);
    };
    
    defineMethod("abs", Math::abs, 1);
    defineMethod("acos", Math::acos, 1);
    defineMethod("acosh", Math::acosh, 1);
    defineMethod("asin", Math::asin, 1);
    defineMethod("asinh", Math::asinh, 1);
    defineMethod("atan", Math::atan, 1);
    defineMethod("atanh", Math::atanh, 1);
    defineMethod("atan2", Math::atan2, 2);
    defineMethod("cbrt", Math::cbrt, 1);
    defineMethod("ceil", Math::ceil, 1);
    defineMethod("clz32", Math::clz32, 1);
    defineMethod("cos", Math::cos, 1);
    defineMethod("cosh", Math::cosh, 1);
    defineMethod("exp", Math::exp, 1);
    defineMethod("expm1", Math::expm1, 1);
    defineMethod("floor", Math::floor, 1);
    defineMethod("fround", Math::fround, 1);
    defineMethod("hypot", Math::hypot, 2);
    defineMethod("imul", Math::imul, 2);
    defineMethod("log", Math::log, 1);
    defineMethod("log1p", Math::log1p, 1);
    defineMethod("log10", Math::log10, 1);
    defineMethod("log2", Math::log2, 1);
    defineMethod("max", Math::max, 2);
    defineMethod("min", Math::min, 2);
    defineMethod("pow", Math::pow, 2);
    defineMethod("random", Math::random, 0);
    defineMethod("round", Math::round, 1);
    defineMethod("sign", Math::sign, 1);
    defineMethod("sin", Math::sin, 1);
    defineMethod("sinh", Math::sinh, 1);
    defineMethod("sqrt", Math::sqrt, 1);
    defineMethod("tan", Math::tan, 1);
    defineMethod("tanh", Math::tanh, 1);
    defineMethod("trunc", Math::trunc, 1);
}

// 引数を数値に変換するユーティリティ関数
static double toNumber(ValuePtr value) {
    if (!value) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return value->toNumber();
}

// Math.abs(x)
ValuePtr Math::abs(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::abs(value));
}

// Math.acos(x)
ValuePtr Math::acos(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::acos(value));
}

// Math.acosh(x)
ValuePtr Math::acosh(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::acosh(value));
}

// Math.asin(x)
ValuePtr Math::asin(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::asin(value));
}

// Math.asinh(x)
ValuePtr Math::asinh(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::asinh(value));
}

// Math.atan(x)
ValuePtr Math::atan(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::atan(value));
}

// Math.atanh(x)
ValuePtr Math::atanh(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::atanh(value));
}

// Math.atan2(y, x)
ValuePtr Math::atan2(const std::vector<ValuePtr>& arguments) {
    if (arguments.size() < 2) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double y = toNumber(arguments[0]);
    double x = toNumber(arguments[1]);
    return Value::fromNumber(std::atan2(y, x));
}

// Math.cbrt(x)
ValuePtr Math::cbrt(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::cbrt(value));
}

// Math.ceil(x)
ValuePtr Math::ceil(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::ceil(value));
}

// Math.clz32(x)
ValuePtr Math::clz32(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(32);
    }
    
    double value = toNumber(arguments[0]);
    // NaN、Infinity、負数などは32に変換
    if (std::isnan(value) || std::isinf(value) || value <= 0) {
        return Value::fromNumber(32);
    }
    
    uint32_t n = static_cast<uint32_t>(value);
    if (n == 0) {
        return Value::fromNumber(32);
    }
    
    // 先行する0のビット数をカウント
    int count = 0;
    for (int i = 31; i >= 0; i--) {
        if ((n & (1 << i)) == 0) {
            count++;
        } else {
            break;
        }
    }
    
    return Value::fromNumber(count);
}

// Math.cos(x)
ValuePtr Math::cos(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::cos(value));
}

// Math.cosh(x)
ValuePtr Math::cosh(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::cosh(value));
}

// Math.exp(x)
ValuePtr Math::exp(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::exp(value));
}

// Math.expm1(x)
ValuePtr Math::expm1(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::expm1(value));
}

// Math.floor(x)
ValuePtr Math::floor(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::floor(value));
}

// Math.fround(x)
ValuePtr Math::fround(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    // 32ビット浮動小数点に変換してから戻す
    float f = static_cast<float>(value);
    return Value::fromNumber(static_cast<double>(f));
}

// Math.hypot(x, y, ...)
ValuePtr Math::hypot(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(0.0);
    }
    
    // Infinity があれば結果は Infinity
    // NaN があれば、他に Infinity がなければ結果は NaN
    bool hasNaN = false;
    
    std::vector<double> values;
    values.reserve(arguments.size());
    
    for (const auto& arg : arguments) {
        double value = toNumber(arg);
        if (std::isinf(value)) {
            return Value::fromNumber(std::numeric_limits<double>::infinity());
        }
        if (std::isnan(value)) {
            hasNaN = true;
        } else {
            values.push_back(value);
        }
    }
    
    if (hasNaN) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    if (values.empty()) {
        return Value::fromNumber(0.0);
    }
    
    // オーバーフロー/アンダーフローを避けるため、最大値でスケーリング
    double max = 0.0;
    for (double v : values) {
        max = std::max(max, std::abs(v));
    }
    
    if (max == 0.0) {
        return Value::fromNumber(0.0);
    }
    
    // スケーリングして計算
    double sum = 0.0;
    for (double v : values) {
        double scaled = v / max;
        sum += scaled * scaled;
    }
    
    return Value::fromNumber(max * std::sqrt(sum));
}

// Math.imul(x, y)
ValuePtr Math::imul(const std::vector<ValuePtr>& arguments) {
    if (arguments.size() < 2) {
        return Value::fromNumber(0);
    }
    
    uint32_t a = static_cast<uint32_t>(toNumber(arguments[0]));
    uint32_t b = static_cast<uint32_t>(toNumber(arguments[1]));
    
    // 32ビット整数の乗算
    return Value::fromNumber(static_cast<int32_t>(a * b));
}

// Math.log(x)
ValuePtr Math::log(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::log(value));
}

// Math.log1p(x)
ValuePtr Math::log1p(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::log1p(value));
}

// Math.log10(x)
ValuePtr Math::log10(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::log10(value));
}

// Math.log2(x)
ValuePtr Math::log2(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::log2(value));
}

// Math.max(x, y, ...)
ValuePtr Math::max(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(-std::numeric_limits<double>::infinity());
    }
    
    double maxValue = -std::numeric_limits<double>::infinity();
    bool hasNaN = false;
    
    for (const auto& arg : arguments) {
        double value = toNumber(arg);
        
        if (std::isnan(value)) {
            hasNaN = true;
            break;
        }
        
        maxValue = std::max(maxValue, value);
    }
    
    if (hasNaN) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    return Value::fromNumber(maxValue);
}

// Math.min(x, y, ...)
ValuePtr Math::min(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::infinity());
    }
    
    double minValue = std::numeric_limits<double>::infinity();
    bool hasNaN = false;
    
    for (const auto& arg : arguments) {
        double value = toNumber(arg);
        
        if (std::isnan(value)) {
            hasNaN = true;
            break;
        }
        
        minValue = std::min(minValue, value);
    }
    
    if (hasNaN) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    return Value::fromNumber(minValue);
}

// Math.pow(x, y)
ValuePtr Math::pow(const std::vector<ValuePtr>& arguments) {
    if (arguments.size() < 2) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double base = toNumber(arguments[0]);
    double exponent = toNumber(arguments[1]);
    
    return Value::fromNumber(std::pow(base, exponent));
}

// Math.random()
ValuePtr Math::random(const std::vector<ValuePtr>& arguments) {
    // 0以上1未満の乱数を生成
    return Value::fromNumber(s_uniformDist(s_randomEngine));
}

// Math.round(x)
ValuePtr Math::round(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    
    // 特殊ケース処理
    if (std::isnan(value)) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    if (value == 0.0 || value == -0.0) {
        return Value::fromNumber(value);
    }
    
    if (std::isinf(value)) {
        return Value::fromNumber(value);
    }
    
    // .5に対する特別な丸め処理
    if (value > 0.0 && value - std::floor(value) == 0.5) {
        return Value::fromNumber(std::ceil(value));
    }
    
    if (value < 0.0 && std::ceil(value) - value == 0.5) {
        return Value::fromNumber(std::floor(value));
    }
    
    // 通常の丸め
    return Value::fromNumber(std::round(value));
}

// Math.sign(x)
ValuePtr Math::sign(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    
    if (std::isnan(value) || value == 0.0) {
        return Value::fromNumber(value); // NaN または +/-0 をそのまま返す
    }
    
    return Value::fromNumber(value > 0.0 ? 1.0 : -1.0);
}

// Math.sin(x)
ValuePtr Math::sin(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::sin(value));
}

// Math.sinh(x)
ValuePtr Math::sinh(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::sinh(value));
}

// Math.sqrt(x)
ValuePtr Math::sqrt(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::sqrt(value));
}

// Math.tan(x)
ValuePtr Math::tan(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::tan(value));
}

// Math.tanh(x)
ValuePtr Math::tanh(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::tanh(value));
}

// Math.trunc(x)
ValuePtr Math::trunc(const std::vector<ValuePtr>& arguments) {
    if (arguments.empty()) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    }
    
    double value = toNumber(arguments[0]);
    return Value::fromNumber(std::trunc(value));
}

} // namespace core
} // namespace aerojs 