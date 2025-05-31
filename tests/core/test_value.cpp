/**
 * @file test_value.cpp
 * @brief AeroJS Value クラスのテスト
 * @version 0.1.0
 * @license MIT
 */

#include <gtest/gtest.h>
#include "core/value.h"
#include <limits>
#include <cmath>

using namespace aerojs::core;

class ValueTest : public ::testing::Test {
protected:
    void SetUp() override {
        // テスト用のセットアップ
    }

    void TearDown() override {
        // テスト用のクリーンアップ
    }
};

TEST_F(ValueTest, UndefinedValueTest) {
    Value value = Value::undefined();
    
    EXPECT_TRUE(value.isUndefined());
    EXPECT_FALSE(value.isNull());
    EXPECT_FALSE(value.isBoolean());
    EXPECT_FALSE(value.isNumber());
    EXPECT_FALSE(value.isString());
    EXPECT_FALSE(value.isObject());
    
    EXPECT_EQ(value.getType(), ValueType::Undefined);
    EXPECT_EQ(value.toString(), "undefined");
    EXPECT_FALSE(value.toBoolean());
    EXPECT_DOUBLE_EQ(value.toNumber(), 0.0);
    EXPECT_EQ(value.toObject(), nullptr);
}

TEST_F(ValueTest, NullValueTest) {
    Value value = Value::null();
    
    EXPECT_FALSE(value.isUndefined());
    EXPECT_TRUE(value.isNull());
    EXPECT_FALSE(value.isBoolean());
    EXPECT_FALSE(value.isNumber());
    EXPECT_FALSE(value.isString());
    EXPECT_FALSE(value.isObject());
    
    EXPECT_EQ(value.getType(), ValueType::Null);
    EXPECT_EQ(value.toString(), "null");
    EXPECT_FALSE(value.toBoolean());
    EXPECT_DOUBLE_EQ(value.toNumber(), 0.0);
    EXPECT_EQ(value.toObject(), nullptr);
}

TEST_F(ValueTest, BooleanValueTest) {
    // true値のテスト
    Value trueValue = Value::fromBoolean(true);
    
    EXPECT_FALSE(trueValue.isUndefined());
    EXPECT_FALSE(trueValue.isNull());
    EXPECT_TRUE(trueValue.isBoolean());
    EXPECT_FALSE(trueValue.isNumber());
    EXPECT_FALSE(trueValue.isString());
    EXPECT_FALSE(trueValue.isObject());
    
    EXPECT_EQ(trueValue.getType(), ValueType::Boolean);
    EXPECT_EQ(trueValue.toString(), "true");
    EXPECT_TRUE(trueValue.toBoolean());
    EXPECT_DOUBLE_EQ(trueValue.toNumber(), 1.0);
    
    // false値のテスト
    Value falseValue = Value::fromBoolean(false);
    
    EXPECT_TRUE(falseValue.isBoolean());
    EXPECT_EQ(falseValue.toString(), "false");
    EXPECT_FALSE(falseValue.toBoolean());
    EXPECT_DOUBLE_EQ(falseValue.toNumber(), 0.0);
}

TEST_F(ValueTest, NumberValueTest) {
    // 整数のテスト
    Value intValue = Value::fromNumber(42);
    
    EXPECT_FALSE(intValue.isUndefined());
    EXPECT_FALSE(intValue.isNull());
    EXPECT_FALSE(intValue.isBoolean());
    EXPECT_TRUE(intValue.isNumber());
    EXPECT_FALSE(intValue.isString());
    EXPECT_FALSE(intValue.isObject());
    
    EXPECT_EQ(intValue.getType(), ValueType::Number);
    EXPECT_DOUBLE_EQ(intValue.toNumber(), 42.0);
    EXPECT_TRUE(intValue.toBoolean());
    EXPECT_EQ(intValue.toString(), "42.000000");
    
    // 浮動小数点数のテスト
    Value floatValue = Value::fromNumber(3.14159);
    EXPECT_DOUBLE_EQ(floatValue.toNumber(), 3.14159);
    EXPECT_TRUE(floatValue.toBoolean());
    
    // ゼロのテスト
    Value zeroValue = Value::fromNumber(0.0);
    EXPECT_DOUBLE_EQ(zeroValue.toNumber(), 0.0);
    EXPECT_FALSE(zeroValue.toBoolean());
    
    // 負の数のテスト
    Value negativeValue = Value::fromNumber(-123.456);
    EXPECT_DOUBLE_EQ(negativeValue.toNumber(), -123.456);
    EXPECT_TRUE(negativeValue.toBoolean());
    
    // 無限大のテスト
    Value infinityValue = Value::fromNumber(std::numeric_limits<double>::infinity());
    EXPECT_TRUE(std::isinf(infinityValue.toNumber()));
    EXPECT_TRUE(infinityValue.toBoolean());
    
    // NaNのテスト
    Value nanValue = Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(std::isnan(nanValue.toNumber()));
    EXPECT_FALSE(nanValue.toBoolean());
}

TEST_F(ValueTest, StringValueTest) {
    // 通常の文字列のテスト
    Value stringValue = Value::fromString("Hello, World!");
    
    EXPECT_FALSE(stringValue.isUndefined());
    EXPECT_FALSE(stringValue.isNull());
    EXPECT_FALSE(stringValue.isBoolean());
    EXPECT_FALSE(stringValue.isNumber());
    EXPECT_TRUE(stringValue.isString());
    EXPECT_FALSE(stringValue.isObject());
    
    EXPECT_EQ(stringValue.getType(), ValueType::String);
    EXPECT_EQ(stringValue.toString(), "Hello, World!");
    EXPECT_TRUE(stringValue.toBoolean());
    EXPECT_DOUBLE_EQ(stringValue.toNumber(), 0.0); // 数値に変換できない文字列
    
    // 空文字列のテスト
    Value emptyString = Value::fromString("");
    EXPECT_EQ(emptyString.toString(), "");
    EXPECT_FALSE(emptyString.toBoolean());
    EXPECT_DOUBLE_EQ(emptyString.toNumber(), 0.0);
    
    // 数値文字列のテスト
    Value numericString = Value::fromString("123.45");
    EXPECT_EQ(numericString.toString(), "123.45");
    EXPECT_TRUE(numericString.toBoolean());
    EXPECT_DOUBLE_EQ(numericString.toNumber(), 123.45);
    
    // 日本語文字列のテスト
    Value japaneseString = Value::fromString("こんにちは");
    EXPECT_EQ(japaneseString.toString(), "こんにちは");
    EXPECT_TRUE(japaneseString.toBoolean());
}

TEST_F(ValueTest, ObjectValueTest) {
    // nullptrオブジェクトのテスト
    Value nullObject = Value::fromObject(nullptr);
    
    EXPECT_FALSE(nullObject.isUndefined());
    EXPECT_FALSE(nullObject.isNull());
    EXPECT_FALSE(nullObject.isBoolean());
    EXPECT_FALSE(nullObject.isNumber());
    EXPECT_FALSE(nullObject.isString());
    EXPECT_TRUE(nullObject.isObject());
    
    EXPECT_EQ(nullObject.getType(), ValueType::Object);
    EXPECT_EQ(nullObject.toObject(), nullptr);
    EXPECT_FALSE(nullObject.toBoolean());
    EXPECT_EQ(nullObject.toString(), "[object Object]");
    
    // 有効なオブジェクトのテスト
    int dummy = 42;
    Value objectValue = Value::fromObject(&dummy);
    
    EXPECT_TRUE(objectValue.isObject());
    EXPECT_EQ(objectValue.toObject(), &dummy);
    EXPECT_TRUE(objectValue.toBoolean());
    EXPECT_EQ(objectValue.toString(), "[object Object]");
}

TEST_F(ValueTest, TypeConversionTest) {
    // Boolean to Number
    EXPECT_DOUBLE_EQ(Value::fromBoolean(true).toNumber(), 1.0);
    EXPECT_DOUBLE_EQ(Value::fromBoolean(false).toNumber(), 0.0);
    
    // Number to Boolean
    EXPECT_TRUE(Value::fromNumber(1.0).toBoolean());
    EXPECT_TRUE(Value::fromNumber(-1.0).toBoolean());
    EXPECT_FALSE(Value::fromNumber(0.0).toBoolean());
    EXPECT_FALSE(Value::fromNumber(std::numeric_limits<double>::quiet_NaN()).toBoolean());
    
    // String to Number
    EXPECT_DOUBLE_EQ(Value::fromString("42").toNumber(), 42.0);
    EXPECT_DOUBLE_EQ(Value::fromString("3.14").toNumber(), 3.14);
    EXPECT_DOUBLE_EQ(Value::fromString("invalid").toNumber(), 0.0);
    EXPECT_DOUBLE_EQ(Value::fromString("").toNumber(), 0.0);
    
    // String to Boolean
    EXPECT_TRUE(Value::fromString("hello").toBoolean());
    EXPECT_FALSE(Value::fromString("").toBoolean());
    
    // Number to String
    Value num = Value::fromNumber(42.0);
    std::string numStr = num.toString();
    EXPECT_FALSE(numStr.empty());
    
    // Boolean to String
    EXPECT_EQ(Value::fromBoolean(true).toString(), "true");
    EXPECT_EQ(Value::fromBoolean(false).toString(), "false");
}

TEST_F(ValueTest, DefaultConstructorTest) {
    Value value;
    
    EXPECT_TRUE(value.isUndefined());
    EXPECT_EQ(value.getType(), ValueType::Undefined);
    EXPECT_EQ(value.toString(), "undefined");
    EXPECT_FALSE(value.toBoolean());
    EXPECT_DOUBLE_EQ(value.toNumber(), 0.0);
}

TEST_F(ValueTest, CopyAndAssignmentTest) {
    Value original = Value::fromString("test");
    
    // コピーコンストラクタのテスト
    Value copied = original;
    EXPECT_TRUE(copied.isString());
    EXPECT_EQ(copied.toString(), "test");
    
    // 代入演算子のテスト
    Value assigned;
    assigned = original;
    EXPECT_TRUE(assigned.isString());
    EXPECT_EQ(assigned.toString(), "test");
}

TEST_F(ValueTest, EdgeCasesTest) {
    // 非常に大きな数値
    Value largeNumber = Value::fromNumber(1e308);
    EXPECT_TRUE(largeNumber.isNumber());
    EXPECT_TRUE(std::isfinite(largeNumber.toNumber()));
    
    // 非常に小さな数値
    Value smallNumber = Value::fromNumber(1e-308);
    EXPECT_TRUE(smallNumber.isNumber());
    EXPECT_TRUE(std::isfinite(smallNumber.toNumber()));
    
    // 非常に長い文字列
    std::string longString(10000, 'a');
    Value longStringValue = Value::fromString(longString);
    EXPECT_TRUE(longStringValue.isString());
    EXPECT_EQ(longStringValue.toString().length(), 10000);
}

TEST_F(ValueTest, PerformanceTest) {
    // 大量の値作成と変換のパフォーマンステスト
    const int iterations = 100000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        Value value = Value::fromNumber(i);
        double num = value.toNumber();
        bool boolean = value.toBoolean();
        std::string str = value.toString();
        
        // 使用されていない変数の警告を避ける
        (void)num;
        (void)boolean;
        (void)str;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 1秒以内に完了することを期待
    EXPECT_LT(duration.count(), 1000);
} 