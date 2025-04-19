/**
 * @file math.h
 * @brief JavaScript の Math オブジェクトの定義
 */

#pragma once

#include <cmath>
#include <memory>
#include <unordered_map>
#include <string>
#include "../../../value.h"

namespace aerojs {
namespace core {

// 前方宣言
class Value;
class Object;
using ValuePtr = std::shared_ptr<Value>;
using ObjectPtr = std::shared_ptr<Object>;

/**
 * @brief JavaScript の Math オブジェクトを表すクラス
 * 
 * Math クラスは JavaScript の Math オブジェクトを実装します。
 * ECMAScript 仕様に準拠した数学関数と定数を提供します。
 */
class Math {
public:
    /**
     * @brief グローバルオブジェクトに Math オブジェクトをインストールする
     * 
     * @param globalObject グローバルオブジェクト
     * @return Math オブジェクトのインスタンス
     */
    static ObjectPtr initialize(ObjectPtr globalObject);

    /**
     * @brief Math オブジェクトのプロトタイプを作成する
     * 
     * @return Math オブジェクトのプロトタイプ
     */
    static ObjectPtr createPrototype();

    /**
     * @brief Math オブジェクトのコンストラクタを作成する
     * 
     * @param prototype Math オブジェクトのプロトタイプ
     * @return Math オブジェクトのコンストラクタ
     */
    static ObjectPtr createConstructor(ObjectPtr prototype);

private:
    // Math オブジェクトの定数
    static void installConstants(ObjectPtr mathObject);

    // Math オブジェクトのメソッド
    static void installFunctions(ObjectPtr mathObject);

    // Math オブジェクトの各メソッド実装
    static ValuePtr abs(const std::vector<ValuePtr>& arguments);
    static ValuePtr acos(const std::vector<ValuePtr>& arguments);
    static ValuePtr acosh(const std::vector<ValuePtr>& arguments);
    static ValuePtr asin(const std::vector<ValuePtr>& arguments);
    static ValuePtr asinh(const std::vector<ValuePtr>& arguments);
    static ValuePtr atan(const std::vector<ValuePtr>& arguments);
    static ValuePtr atanh(const std::vector<ValuePtr>& arguments);
    static ValuePtr atan2(const std::vector<ValuePtr>& arguments);
    static ValuePtr cbrt(const std::vector<ValuePtr>& arguments);
    static ValuePtr ceil(const std::vector<ValuePtr>& arguments);
    static ValuePtr clz32(const std::vector<ValuePtr>& arguments);
    static ValuePtr cos(const std::vector<ValuePtr>& arguments);
    static ValuePtr cosh(const std::vector<ValuePtr>& arguments);
    static ValuePtr exp(const std::vector<ValuePtr>& arguments);
    static ValuePtr expm1(const std::vector<ValuePtr>& arguments);
    static ValuePtr floor(const std::vector<ValuePtr>& arguments);
    static ValuePtr fround(const std::vector<ValuePtr>& arguments);
    static ValuePtr hypot(const std::vector<ValuePtr>& arguments);
    static ValuePtr imul(const std::vector<ValuePtr>& arguments);
    static ValuePtr log(const std::vector<ValuePtr>& arguments);
    static ValuePtr log1p(const std::vector<ValuePtr>& arguments);
    static ValuePtr log10(const std::vector<ValuePtr>& arguments);
    static ValuePtr log2(const std::vector<ValuePtr>& arguments);
    static ValuePtr max(const std::vector<ValuePtr>& arguments);
    static ValuePtr min(const std::vector<ValuePtr>& arguments);
    static ValuePtr pow(const std::vector<ValuePtr>& arguments);
    static ValuePtr random(const std::vector<ValuePtr>& arguments);
    static ValuePtr round(const std::vector<ValuePtr>& arguments);
    static ValuePtr sign(const std::vector<ValuePtr>& arguments);
    static ValuePtr sin(const std::vector<ValuePtr>& arguments);
    static ValuePtr sinh(const std::vector<ValuePtr>& arguments);
    static ValuePtr sqrt(const std::vector<ValuePtr>& arguments);
    static ValuePtr tan(const std::vector<ValuePtr>& arguments);
    static ValuePtr tanh(const std::vector<ValuePtr>& arguments);
    static ValuePtr trunc(const std::vector<ValuePtr>& arguments);
};

} // namespace core
} // namespace aerojs 