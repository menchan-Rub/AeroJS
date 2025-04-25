/**
 * @file number_test.cpp
 * @brief JavaScript の Number 組み込みオブジェクトのテスト
 */

#include "src/core/runtime/builtins/number/number.h"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <memory>

#include "src/core/error.h"
#include "src/core/function.h"
#include "src/core/global_object.h"
#include "src/core/value.h"

namespace aerojs {
namespace core {
namespace test {

class NumberTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_globalObject = std::make_shared<GlobalObject>();

    // グローバルオブジェクトに Number を登録
    m_globalObject->set("Number", Number::getConstructor(),
                        PropertyAttribute::Writable | PropertyAttribute::Configurable);
  }

  ObjectPtr m_globalObject;
};

// コンストラクタのテスト
TEST_F(NumberTest, Constructor) {
  // 引数なしのコンストラクタ
  auto num0 = std::make_shared<Number>();
  EXPECT_DOUBLE_EQ(0.0, num0->getValue());

  // 数値引数のコンストラクタ
  auto num1 = std::make_shared<Number>(42.5);
  EXPECT_DOUBLE_EQ(42.5, num1->getValue());

  // 静的コンストラクタ関数
  std::vector<ValuePtr> emptyArgs;
  auto numResult1 = Number::construct(emptyArgs, true);
  EXPECT_TRUE(std::dynamic_pointer_cast<Number>(numResult1) != nullptr);
  EXPECT_DOUBLE_EQ(0.0, std::dynamic_pointer_cast<Number>(numResult1)->getValue());

  // 引数あり
  std::vector<ValuePtr> args = {Value::createNumber(123.45)};
  auto numResult2 = Number::construct(args, true);
  EXPECT_TRUE(std::dynamic_pointer_cast<Number>(numResult2) != nullptr);
  EXPECT_DOUBLE_EQ(123.45, std::dynamic_pointer_cast<Number>(numResult2)->getValue());

  // 関数として呼び出し
  auto numResult3 = Number::construct(args, false);
  EXPECT_FALSE(std::dynamic_pointer_cast<Number>(numResult3) != nullptr);
  EXPECT_DOUBLE_EQ(123.45, numResult3->toNumber());
}

// toNumber と toString のテスト
TEST_F(NumberTest, BasicMethods) {
  auto num1 = std::make_shared<Number>(42.5);

  // toNumber のテスト
  EXPECT_DOUBLE_EQ(42.5, num1->toNumber());

  // toString のテスト
  EXPECT_EQ("42.5", num1->toString());

  // 整数
  auto num2 = std::make_shared<Number>(123);
  EXPECT_EQ("123", num2->toString());

  // 0
  auto num3 = std::make_shared<Number>(0);
  EXPECT_EQ("0", num3->toString());

  // 負の数
  auto num4 = std::make_shared<Number>(-42.5);
  EXPECT_EQ("-42.5", num4->toString());

  // 非常に小さい数
  auto num5 = std::make_shared<Number>(0.0000001);
  EXPECT_EQ("1e-7", num5->toString());  // 指数表記または "0.0000001" も可

  // 非常に大きい数
  auto num6 = std::make_shared<Number>(1e20);
  EXPECT_EQ("1e+20", num6->toString());

  // 特殊値
  auto numNaN = std::make_shared<Number>(Number::NaN);
  EXPECT_EQ("NaN", numNaN->toString());

  auto numInf = std::make_shared<Number>(Number::POSITIVE_INFINITY);
  EXPECT_EQ("Infinity", numInf->toString());

  auto numNegInf = std::make_shared<Number>(Number::NEGATIVE_INFINITY);
  EXPECT_EQ("-Infinity", numNegInf->toString());
}

// 静的メソッドのテスト
TEST_F(NumberTest, StaticMethods) {
  // Number.isFinite
  {
    std::vector<ValuePtr> args = {Value::createNumber(42)};
    auto result = Number::isFinite(args);
    EXPECT_TRUE(result->toBoolean());

    args = {Value::createNumber(Number::POSITIVE_INFINITY)};
    result = Number::isFinite(args);
    EXPECT_FALSE(result->toBoolean());

    args = {Value::createNumber(Number::NaN)};
    result = Number::isFinite(args);
    EXPECT_FALSE(result->toBoolean());

    args = {Value::createString("42")};
    result = Number::isFinite(args);
    EXPECT_FALSE(result->toBoolean());  // Number.isFinite は型変換しない
  }

  // Number.isInteger
  {
    std::vector<ValuePtr> args = {Value::createNumber(42)};
    auto result = Number::isInteger(args);
    EXPECT_TRUE(result->toBoolean());

    args = {Value::createNumber(42.5)};
    result = Number::isInteger(args);
    EXPECT_FALSE(result->toBoolean());

    args = {Value::createNumber(Number::NaN)};
    result = Number::isInteger(args);
    EXPECT_FALSE(result->toBoolean());

    args = {Value::createString("42")};
    result = Number::isInteger(args);
    EXPECT_FALSE(result->toBoolean());  // Number.isInteger は型変換しない
  }

  // Number.isNaN
  {
    std::vector<ValuePtr> args = {Value::createNumber(Number::NaN)};
    auto result = Number::isNaN(args);
    EXPECT_TRUE(result->toBoolean());

    args = {Value::createNumber(42)};
    result = Number::isNaN(args);
    EXPECT_FALSE(result->toBoolean());

    args = {Value::createString("NaN")};
    result = Number::isNaN(args);
    EXPECT_FALSE(result->toBoolean());  // Number.isNaN は型変換しない
  }

  // Number.isSafeInteger
  {
    std::vector<ValuePtr> args = {Value::createNumber(42)};
    auto result = Number::isSafeInteger(args);
    EXPECT_TRUE(result->toBoolean());

    args = {Value::createNumber(Number::MAX_SAFE_INTEGER)};
    result = Number::isSafeInteger(args);
    EXPECT_TRUE(result->toBoolean());

    args = {Value::createNumber(Number::MAX_SAFE_INTEGER + .1)};
    result = Number::isSafeInteger(args);
    EXPECT_FALSE(result->toBoolean());

    args = {Value::createNumber(42.5)};
    result = Number::isSafeInteger(args);
    EXPECT_FALSE(result->toBoolean());
  }

  // Number.parseFloat
  {
    std::vector<ValuePtr> args = {Value::createString("42.5")};
    auto result = Number::parseFloat(args);
    EXPECT_DOUBLE_EQ(42.5, result->toNumber());

    args = {Value::createString("42.5abc")};
    result = Number::parseFloat(args);
    EXPECT_DOUBLE_EQ(42.5, result->toNumber());

    args = {Value::createString("abc")};
    result = Number::parseFloat(args);
    EXPECT_TRUE(std::isnan(result->toNumber()));

    args = {Value::createString("Infinity")};
    result = Number::parseFloat(args);
    EXPECT_TRUE(std::isinf(result->toNumber()) && result->toNumber() > 0);
  }

  // Number.parseInt
  {
    std::vector<ValuePtr> args = {Value::createString("42")};
    auto result = Number::parseInt(args);
    EXPECT_DOUBLE_EQ(42, result->toNumber());

    args = {Value::createString("42.5")};
    result = Number::parseInt(args);
    EXPECT_DOUBLE_EQ(42, result->toNumber());

    args = {Value::createString("42abc")};
    result = Number::parseInt(args);
    EXPECT_DOUBLE_EQ(42, result->toNumber());

    args = {Value::createString("abc")};
    result = Number::parseInt(args);
    EXPECT_TRUE(std::isnan(result->toNumber()));

    // 基数指定
    args = {Value::createString("1010"), Value::createNumber(2)};
    result = Number::parseInt(args);
    EXPECT_DOUBLE_EQ(10, result->toNumber());

    args = {Value::createString("FF"), Value::createNumber(16)};
    result = Number::parseInt(args);
    EXPECT_DOUBLE_EQ(255, result->toNumber());
  }
}

// インスタンスメソッドのテスト
TEST_F(NumberTest, InstanceMethods) {
  // Number.prototype.toExponential
  {
    auto numObj = std::make_shared<Number>(42.5);
    std::vector<ValuePtr> args = {numObj};
    auto result = Number::toExponential(args);
    EXPECT_EQ("4.25e+1", result->toString());

    // 桁数指定
    args = {numObj, Value::createNumber(2)};
    result = Number::toExponential(args);
    EXPECT_EQ("4.25e+1", result->toString());

    args = {numObj, Value::createNumber(4)};
    result = Number::toExponential(args);
    EXPECT_EQ("4.2500e+1", result->toString());

    // プリミティブ数値
    args = {Value::createNumber(12345)};
    result = Number::toExponential(args);
    EXPECT_EQ("1.2345e+4", result->toString());

    // 特殊値
    args = {Value::createNumber(Number::NaN)};
    result = Number::toExponential(args);
    EXPECT_EQ("NaN", result->toString());
  }

  // Number.prototype.toFixed
  {
    auto numObj = std::make_shared<Number>(42.5);
    std::vector<ValuePtr> args = {numObj};
    auto result = Number::toFixed(args);
    EXPECT_EQ("43", result->toString());  // デフォルトは0桁

    // 桁数指定
    args = {numObj, Value::createNumber(2)};
    result = Number::toFixed(args);
    EXPECT_EQ("42.50", result->toString());

    args = {numObj, Value::createNumber(4)};
    result = Number::toFixed(args);
    EXPECT_EQ("42.5000", result->toString());

    // プリミティブ数値
    args = {Value::createNumber(12345.6789)};
    result = Number::toFixed(args);
    EXPECT_EQ("12346", result->toString());  // 丸め

    args = {Value::createNumber(12345.6789), Value::createNumber(2)};
    result = Number::toFixed(args);
    EXPECT_EQ("12345.68", result->toString());  // 丸め

    // 特殊値
    args = {Value::createNumber(Number::NaN)};
    result = Number::toFixed(args);
    EXPECT_EQ("NaN", result->toString());
  }

  // Number.prototype.toPrecision
  {
    auto numObj = std::make_shared<Number>(42.5);
    std::vector<ValuePtr> args = {numObj, Value::createNumber(3)};
    auto result = Number::toPrecision(args);
    EXPECT_EQ("42.5", result->toString());  // 有効桁数3桁

    args = {numObj, Value::createNumber(5)};
    result = Number::toPrecision(args);
    EXPECT_EQ("42.500", result->toString());  // 有効桁数5桁

    // 大きな数値
    args = {Value::createNumber(12345), Value::createNumber(2)};
    result = Number::toPrecision(args);
    EXPECT_EQ("1.2e+4", result->toString());

    // 小さな数値
    args = {Value::createNumber(0.00123), Value::createNumber(2)};
    result = Number::toPrecision(args);
    EXPECT_EQ("0.0012", result->toString());

    // 特殊値
    args = {Value::createNumber(Number::NaN)};
    result = Number::toPrecision(args);
    EXPECT_EQ("NaN", result->toString());
  }

  // Number.prototype.toString
  {
    auto numObj = std::make_shared<Number>(42.5);
    std::vector<ValuePtr> args = {numObj};
    auto result = Number::toStringMethod(args);
    EXPECT_EQ("42.5", result->toString());  // デフォルトは10進数

    // 基数指定
    args = {numObj, Value::createNumber(2)};
    result = Number::toStringMethod(args);
    EXPECT_EQ("101010.1", result->toString());  // 2進数

    args = {numObj, Value::createNumber(16)};
    result = Number::toStringMethod(args);
    EXPECT_EQ("2a.8", result->toString());  // 16進数

    // プリミティブ数値
    args = {Value::createNumber(255)};
    result = Number::toStringMethod(args);
    EXPECT_EQ("255", result->toString());

    args = {Value::createNumber(255), Value::createNumber(16)};
    result = Number::toStringMethod(args);
    EXPECT_EQ("ff", result->toString());  // 16進数

    // 特殊値
    args = {Value::createNumber(Number::NaN)};
    result = Number::toStringMethod(args);
    EXPECT_EQ("NaN", result->toString());
  }

  // Number.prototype.valueOf
  {
    auto numObj = std::make_shared<Number>(42.5);
    std::vector<ValuePtr> args = {numObj};
    auto result = Number::valueOf(args);
    EXPECT_DOUBLE_EQ(42.5, result->toNumber());

    // プリミティブ数値
    args = {Value::createNumber(123)};
    result = Number::valueOf(args);
    EXPECT_DOUBLE_EQ(123, result->toNumber());

    // 特殊値
    args = {Value::createNumber(Number::NaN)};
    result = Number::valueOf(args);
    EXPECT_TRUE(std::isnan(result->toNumber()));
  }
}

// Number 定数のテスト
TEST_F(NumberTest, Constants) {
  auto constructor = Number::getConstructor();

  auto epsilon = constructor->get("EPSILON");
  EXPECT_DOUBLE_EQ(std::numeric_limits<double>::epsilon(), epsilon->toNumber());

  auto maxValue = constructor->get("MAX_VALUE");
  EXPECT_DOUBLE_EQ(std::numeric_limits<double>::max(), maxValue->toNumber());

  auto minValue = constructor->get("MIN_VALUE");
  EXPECT_DOUBLE_EQ(std::numeric_limits<double>::min(), minValue->toNumber());

  auto maxSafeInteger = constructor->get("MAX_SAFE_INTEGER");
  EXPECT_DOUBLE_EQ(9007199254740991.0, maxSafeInteger->toNumber());

  auto minSafeInteger = constructor->get("MIN_SAFE_INTEGER");
  EXPECT_DOUBLE_EQ(-9007199254740991.0, minSafeInteger->toNumber());

  auto positiveInfinity = constructor->get("POSITIVE_INFINITY");
  EXPECT_TRUE(std::isinf(positiveInfinity->toNumber()) && positiveInfinity->toNumber() > 0);

  auto negativeInfinity = constructor->get("NEGATIVE_INFINITY");
  EXPECT_TRUE(std::isinf(negativeInfinity->toNumber()) && negativeInfinity->toNumber() < 0);

  auto nan = constructor->get("NaN");
  EXPECT_TRUE(std::isnan(nan->toNumber()));
}

// エラーケースのテスト
TEST_F(NumberTest, ErrorCases) {
  // メソッドが無効なthisで呼ばれた場合
  {
    std::vector<ValuePtr> args = {Value::createString("not a number")};
    EXPECT_THROW(Number::toExponential(args), TypeError);
    EXPECT_THROW(Number::toFixed(args), TypeError);
    EXPECT_THROW(Number::toPrecision(args), TypeError);
    EXPECT_THROW(Number::toStringMethod(args), TypeError);
    EXPECT_THROW(Number::valueOf(args), TypeError);
  }

  // 引数が範囲外の場合
  {
    auto numObj = std::make_shared<Number>(42.5);

    std::vector<ValuePtr> args = {numObj, Value::createNumber(-1)};
    EXPECT_THROW(Number::toExponential(args), RangeError);
    EXPECT_THROW(Number::toFixed(args), RangeError);

    args = {numObj, Value::createNumber(21)};
    EXPECT_THROW(Number::toExponential(args), RangeError);
    EXPECT_THROW(Number::toFixed(args), RangeError);

    args = {numObj, Value::createNumber(0)};
    EXPECT_THROW(Number::toPrecision(args), RangeError);

    args = {numObj, Value::createNumber(22)};
    EXPECT_THROW(Number::toPrecision(args), RangeError);

    args = {numObj, Value::createNumber(1)};
    EXPECT_THROW(Number::toStringMethod(args), RangeError);

    args = {numObj, Value::createNumber(37)};
    EXPECT_THROW(Number::toStringMethod(args), RangeError);
  }

  // 引数が不足している場合
  {
    std::vector<ValuePtr> args;

    // 引数なしの場合はundefinedと同じ扱い
    auto result = Number::isFinite(args);
    EXPECT_FALSE(result->toBoolean());

    result = Number::isInteger(args);
    EXPECT_FALSE(result->toBoolean());

    result = Number::isNaN(args);
    EXPECT_FALSE(result->toBoolean());

    result = Number::isSafeInteger(args);
    EXPECT_FALSE(result->toBoolean());

    result = Number::parseFloat(args);
    EXPECT_TRUE(std::isnan(result->toNumber()));

    result = Number::parseInt(args);
    EXPECT_TRUE(std::isnan(result->toNumber()));

    // thisが必要なメソッドは例外を投げる
    EXPECT_THROW(Number::toExponential(args), TypeError);
  }
}

// 複雑なケースのテスト
TEST_F(NumberTest, EdgeCases) {
  // 非常に大きな数値
  {
    auto largeNum = std::make_shared<Number>(1e20);
    std::vector<ValuePtr> args = {largeNum};

    auto result = Number::toExponential(args);
    EXPECT_EQ("1e+20", result->toString());

    result = Number::toFixed(args);
    EXPECT_NE("Infinity", result->toString());  // 文字列表現は指数表記または大きな数

    args = {largeNum, Value::createNumber(2)};
    result = Number::toPrecision(args);
    EXPECT_EQ("1.0e+20", result->toString());
  }

  // 非常に小さな数値
  {
    auto smallNum = std::make_shared<Number>(1e-10);
    std::vector<ValuePtr> args = {smallNum};

    auto result = Number::toExponential(args);
    EXPECT_EQ("1e-10", result->toString());

    args = {smallNum, Value::createNumber(2)};
    result = Number::toFixed(args);
    EXPECT_EQ("0.00", result->toString());

    args = {smallNum, Value::createNumber(2)};
    result = Number::toPrecision(args);
    EXPECT_EQ("1.0e-10", result->toString());
  }

  // 精度の検証
  {
    auto num = std::make_shared<Number>(0.1 + 0.2);  // 0.30000000000000004
    std::vector<ValuePtr> args = {num};

    auto result = Number::toString(args);
    EXPECT_NE("0.3", result->toString());  // IEEE-754の制限により0.3にはならない

    args = {num, Value::createNumber(1)};
    result = Number::toFixed(args);
    EXPECT_EQ("0.3", result->toString());  // 丸めにより0.3になる
  }
}

}  // namespace test
}  // namespace core
}  // namespace aerojs