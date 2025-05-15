/**
 * @file value_test.cpp
 * @brief Value型のNaN-boxing実装テスト
 * @version 1.0.0
 * @license MIT
 */

#include "../../../../src/core/runtime/values/value.h"
#include <gtest/gtest.h>
#include <limits>
#include <vector>
#include <memory>
#include <cmath>

using namespace aerojs::core;

class ValueTest : public ::testing::Test {
protected:
  void SetUp() override {
    // テスト用セットアップ
  }

  void TearDown() override {
    // テスト用クリーンアップ
  }
};

// 基本初期化テスト
TEST_F(ValueTest, DefaultInitialization) {
  Value value;
  EXPECT_TRUE(value.isUndefined());
  EXPECT_FALSE(value.isNull());
  EXPECT_FALSE(value.isNumber());
  EXPECT_FALSE(value.isBoolean());
  EXPECT_FALSE(value.isString());
  EXPECT_FALSE(value.isObject());
}

// 特殊値の生成テスト
TEST_F(ValueTest, SpecialValueCreation) {
  auto undefinedVal = Value::createUndefined();
  auto nullVal = Value::createNull();
  
  EXPECT_TRUE(undefinedVal.isUndefined());
  EXPECT_TRUE(nullVal.isNull());
  EXPECT_FALSE(nullVal.isUndefined());
  EXPECT_FALSE(undefinedVal.isNull());
  
  EXPECT_TRUE(undefinedVal.isNullOrUndefined());
  EXPECT_TRUE(nullVal.isNullOrUndefined());
}

// ブール値テスト
TEST_F(ValueTest, BooleanValues) {
  auto trueVal = Value::createBoolean(true);
  auto falseVal = Value::createBoolean(false);
  
  EXPECT_TRUE(trueVal.isBoolean());
  EXPECT_TRUE(falseVal.isBoolean());
  
  EXPECT_TRUE(trueVal.toBoolean());
  EXPECT_FALSE(falseVal.toBoolean());
  
  // 特殊値のブール変換
  EXPECT_FALSE(Value::createUndefined().toBoolean());
  EXPECT_FALSE(Value::createNull().toBoolean());
  EXPECT_FALSE(Value::createNumber(0.0).toBoolean());
  EXPECT_TRUE(Value::createNumber(1.0).toBoolean());
  EXPECT_TRUE(Value::createNumber(-1.0).toBoolean());
  EXPECT_FALSE(Value::createNumber(std::numeric_limits<double>::quiet_NaN()).toBoolean());
}

// 数値テスト
TEST_F(ValueTest, NumberValues) {
  // 整数テスト
  auto zero = Value::createNumber(0.0);
  auto one = Value::createNumber(1.0);
  auto negOne = Value::createNumber(-1.0);
  auto maxInt32 = Value::createNumber(2147483647.0);
  auto minInt32 = Value::createNumber(-2147483648.0);
  
  EXPECT_TRUE(zero.isNumber());
  EXPECT_TRUE(one.isNumber());
  EXPECT_TRUE(negOne.isNumber());
  EXPECT_TRUE(maxInt32.isNumber());
  EXPECT_TRUE(minInt32.isNumber());
  
  EXPECT_TRUE(zero.isInteger());
  EXPECT_TRUE(one.isInteger());
  EXPECT_TRUE(negOne.isInteger());
  EXPECT_TRUE(maxInt32.isInteger());
  EXPECT_TRUE(minInt32.isInteger());
  
  EXPECT_TRUE(zero.isInt32());
  EXPECT_TRUE(one.isInt32());
  EXPECT_TRUE(negOne.isInt32());
  EXPECT_TRUE(maxInt32.isInt32());
  EXPECT_TRUE(minInt32.isInt32());
  
  EXPECT_EQ(0.0, zero.toNumber());
  EXPECT_EQ(1.0, one.toNumber());
  EXPECT_EQ(-1.0, negOne.toNumber());
  EXPECT_EQ(2147483647.0, maxInt32.toNumber());
  EXPECT_EQ(-2147483648.0, minInt32.toNumber());
  
  // 浮動小数点テスト
  auto pi = Value::createNumber(3.14159);
  auto halfPi = Value::createNumber(1.57079);
  
  EXPECT_TRUE(pi.isNumber());
  EXPECT_TRUE(halfPi.isNumber());
  
  EXPECT_FALSE(pi.isInteger());
  EXPECT_FALSE(halfPi.isInteger());
  
  EXPECT_FALSE(pi.isInt32());
  EXPECT_FALSE(halfPi.isInt32());
  
  EXPECT_DOUBLE_EQ(3.14159, pi.toNumber());
  EXPECT_DOUBLE_EQ(1.57079, halfPi.toNumber());
  
  // 特殊値テスト
  auto nan = Value::createNumber(std::numeric_limits<double>::quiet_NaN());
  auto infPos = Value::createNumber(std::numeric_limits<double>::infinity());
  auto infNeg = Value::createNumber(-std::numeric_limits<double>::infinity());
  
  EXPECT_TRUE(nan.isNumber());
  EXPECT_TRUE(infPos.isNumber());
  EXPECT_TRUE(infNeg.isNumber());
  
  EXPECT_FALSE(nan.isInteger());
  EXPECT_FALSE(infPos.isInteger());
  EXPECT_FALSE(infNeg.isInteger());
  
  EXPECT_FALSE(nan.isInt32());
  EXPECT_FALSE(infPos.isInt32());
  EXPECT_FALSE(infNeg.isInt32());
  
  EXPECT_TRUE(std::isnan(nan.toNumber()));
  EXPECT_TRUE(std::isinf(infPos.toNumber()));
  EXPECT_TRUE(std::isinf(infNeg.toNumber()));
  EXPECT_GT(infPos.toNumber(), 0.0);
  EXPECT_LT(infNeg.toNumber(), 0.0);
}

// int32変換テスト
TEST_F(ValueTest, Int32Conversion) {
  // 通常のケース
  EXPECT_EQ(0, Value::createNumber(0.0).toInt32());
  EXPECT_EQ(1, Value::createNumber(1.0).toInt32());
  EXPECT_EQ(-1, Value::createNumber(-1.0).toInt32());
  EXPECT_EQ(2147483647, Value::createNumber(2147483647.0).toInt32());
  EXPECT_EQ(-2147483648, Value::createNumber(-2147483648.0).toInt32());
  
  // 小数部分の切り捨て
  EXPECT_EQ(3, Value::createNumber(3.14159).toInt32());
  EXPECT_EQ(-3, Value::createNumber(-3.14159).toInt32());
  
  // モジュロ2^32の処理
  EXPECT_EQ(0, Value::createNumber(4294967296.0).toInt32()); // 2^32
  EXPECT_EQ(1, Value::createNumber(4294967297.0).toInt32()); // 2^32 + 1
  EXPECT_EQ(-1, Value::createNumber(4294967295.0).toInt32()); // 2^32 - 1
  
  // 負数のモジュロ2^32処理
  EXPECT_EQ(-2, Value::createNumber(-2.0).toInt32());
  EXPECT_EQ(-1, Value::createNumber(-1.0).toInt32());
  EXPECT_EQ(0, Value::createNumber(-4294967296.0).toInt32()); // -2^32
  EXPECT_EQ(-1, Value::createNumber(-4294967295.0).toInt32()); // -(2^32 - 1)
  
  // 特殊値の処理
  EXPECT_EQ(0, Value::createNumber(std::numeric_limits<double>::quiet_NaN()).toInt32());
  EXPECT_EQ(0, Value::createNumber(std::numeric_limits<double>::infinity()).toInt32());
  EXPECT_EQ(0, Value::createNumber(-std::numeric_limits<double>::infinity()).toInt32());
  EXPECT_EQ(0, Value::createNumber(0.0).toInt32());
  EXPECT_EQ(0, Value::createNumber(-0.0).toInt32());
}

// プリミティブ型判定テスト
TEST_F(ValueTest, PrimitiveTypeChecks) {
  // 各種プリミティブ値
  auto undefinedVal = Value::createUndefined();
  auto nullVal = Value::createNull();
  auto boolVal = Value::createBoolean(true);
  auto numVal = Value::createNumber(42.0);
  
  // プリミティブ判定
  EXPECT_TRUE(undefinedVal.isPrimitive());
  EXPECT_TRUE(nullVal.isPrimitive());
  EXPECT_TRUE(boolVal.isPrimitive());
  EXPECT_TRUE(numVal.isPrimitive());
}

// 高速アクセステスト
TEST_F(ValueTest, FastAccess) {
  auto intVal = Value::createNumber(123);
  auto doubleVal = Value::createNumber(3.14159);
  
  EXPECT_EQ(123, intVal.asInt32());
  EXPECT_DOUBLE_EQ(3.14159, doubleVal.asNumber());
}

// 特殊値グローバルインスタンステスト
TEST_F(ValueTest, GlobalInstances) {
  EXPECT_TRUE(undefined().isUndefined());
  EXPECT_TRUE(null().isNull());
  EXPECT_TRUE(trueValue().toBoolean());
  EXPECT_FALSE(falseValue().toBoolean());
  EXPECT_DOUBLE_EQ(0.0, zero().toNumber());
  EXPECT_DOUBLE_EQ(1.0, one().toNumber());
  EXPECT_TRUE(std::isnan(nan().toNumber()));
  EXPECT_TRUE(std::isinf(infinity().toNumber()));
  EXPECT_TRUE(std::isinf(negativeInfinity().toNumber()));
  EXPECT_GT(infinity().toNumber(), 0.0);
  EXPECT_LT(negativeInfinity().toNumber(), 0.0);
}

// NaN-Boxingビットパターンテスト
TEST_F(ValueTest, RawBitsRepresentation) {
  // 内部ビット表現の検証
  auto undefinedVal = Value::createUndefined();
  auto nullVal = Value::createNull();
  auto trueVal = Value::createBoolean(true);
  auto falseVal = Value::createBoolean(false);
  
  // NaN-Boxingパターンの検証
  EXPECT_EQ(detail::QUIET_NAN_MASK | detail::TAG_UNDEFINED, undefinedVal.getRawBits());
  EXPECT_EQ(detail::QUIET_NAN_MASK | detail::TAG_NULL, nullVal.getRawBits());
  EXPECT_EQ(detail::QUIET_NAN_MASK | detail::TAG_BOOLEAN | detail::BOOLEAN_TRUE, trueVal.getRawBits());
  EXPECT_EQ(detail::QUIET_NAN_MASK | detail::TAG_BOOLEAN, falseVal.getRawBits());
  
  // 数値はIEEE-754倍精度形式でビットが保存される
  union {
    double d;
    uint64_t bits;
  } u;
  u.d = 3.14159;
  auto piVal = Value::createNumber(3.14159);
  EXPECT_EQ(u.bits, piVal.getRawBits());
}

// 型情報取得テスト
TEST_F(ValueTest, TypeInformation) {
  EXPECT_EQ(ValueType::Undefined, Value::createUndefined().getType());
  EXPECT_EQ(ValueType::Null, Value::createNull().getType());
  EXPECT_EQ(ValueType::Boolean, Value::createBoolean(true).getType());
  EXPECT_EQ(ValueType::Number, Value::createNumber(123).getType());
}

// パフォーマンステスト（高速な型チェック）
TEST_F(ValueTest, TypeCheckPerformance) {
  std::vector<Value> values;
  constexpr size_t COUNT = 10000;
  
  // テスト用の値配列を生成
  for (size_t i = 0; i < COUNT; ++i) {
    switch (i % 5) {
      case 0: values.push_back(Value::createUndefined()); break;
      case 1: values.push_back(Value::createNull()); break;
      case 2: values.push_back(Value::createBoolean(i % 2 == 0)); break;
      case 3: values.push_back(Value::createNumber(static_cast<double>(i))); break;
      case 4: values.push_back(Value::createInteger(static_cast<int32_t>(i))); break;
    }
  }
  
  // 型チェックのカウンタ
  size_t undefinedCount = 0;
  size_t nullCount = 0;
  size_t booleanCount = 0;
  size_t numberCount = 0;
  size_t integerCount = 0;
  
  // 高速な型チェックを実行
  for (const auto& val : values) {
    if (val.isUndefined()) ++undefinedCount;
    if (val.isNull()) ++nullCount;
    if (val.isBoolean()) ++booleanCount;
    if (val.isNumber()) ++numberCount;
    if (val.isInteger()) ++integerCount;
  }
  
  // 結果検証
  EXPECT_EQ(COUNT / 5, undefinedCount);
  EXPECT_EQ(COUNT / 5, nullCount);
  EXPECT_EQ(COUNT / 5, booleanCount);
  EXPECT_EQ(COUNT * 2 / 5, numberCount); // createNumberとcreateIntegerの両方
  EXPECT_EQ(COUNT / 5, integerCount);
}

// エッジケーステスト
TEST_F(ValueTest, EdgeCases) {
  // 数値型のエッジケース
  auto maxSafeInt = Value::createNumber(9007199254740991.0); // 2^53 - 1
  auto minSafeInt = Value::createNumber(-9007199254740991.0); // -(2^53 - 1)
  auto beyondMaxSafe = Value::createNumber(9007199254740992.0); // 2^53
  auto beyondMinSafe = Value::createNumber(-9007199254740992.0); // -(2^53)
  
  EXPECT_TRUE(maxSafeInt.isInteger());
  EXPECT_TRUE(minSafeInt.isInteger());
  EXPECT_TRUE(beyondMaxSafe.isInteger()); // 厳密にはJSのセーフ整数を超えるが、IEEE-754では整数として表現可能
  EXPECT_TRUE(beyondMinSafe.isInteger());
  
  // 最大と最小のInt32範囲
  auto maxInt32Plus1 = Value::createNumber(2147483648.0); // 2^31
  auto minInt32Minus1 = Value::createNumber(-2147483649.0); // -2^31 - 1
  
  EXPECT_TRUE(maxInt32Plus1.isInteger());
  EXPECT_FALSE(maxInt32Plus1.isInt32());
  EXPECT_TRUE(minInt32Minus1.isInteger());
  EXPECT_FALSE(minInt32Minus1.isInt32());
}

// メインテスト実行
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
} 