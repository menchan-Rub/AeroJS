/**
 * @file module_test.cpp
 * @brief JavaScript の Number 組み込みオブジェクトのモジュール登録テスト
 */

#include "src/core/runtime/builtins/number/module.h"

#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "src/core/function.h"
#include "src/core/global_object.h"
#include "src/core/runtime/builtins/number/number.h"
#include "src/core/value.h"

namespace aerojs {
namespace core {
namespace test {

class NumberModuleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_globalObject = std::make_shared<GlobalObject>();
  }

  std::shared_ptr<GlobalObject> m_globalObject;
};

// モジュール登録のテスト
TEST_F(NumberModuleTest, Registration) {
  // モジュール登録前はNumberが存在しないことを確認
  EXPECT_FALSE(m_globalObject->hasProperty("Number"));

  // モジュール登録
  registerNumberBuiltin(m_globalObject.get());

  // Numberが正しく登録されたことを確認
  EXPECT_TRUE(m_globalObject->hasProperty("Number"));

  // 登録されたオブジェクトがNumberコンストラクタであることを確認
  ValuePtr numberValue = m_globalObject->get("Number");
  EXPECT_TRUE(numberValue->isFunction());
  EXPECT_EQ("Number", std::static_pointer_cast<Function>(numberValue->toObject())->getName());

  // コンストラクタ関数として使用できることを確認
  FunctionPtr numberConstructor = std::static_pointer_cast<Function>(numberValue->toObject());

  // 静的メソッドと定数が存在することを確認
  ObjectPtr numberObj = numberValue->toObject();
  EXPECT_TRUE(numberObj->hasProperty("isFinite"));
  EXPECT_TRUE(numberObj->hasProperty("isInteger"));
  EXPECT_TRUE(numberObj->hasProperty("isNaN"));
  EXPECT_TRUE(numberObj->hasProperty("isSafeInteger"));
  EXPECT_TRUE(numberObj->hasProperty("parseFloat"));
  EXPECT_TRUE(numberObj->hasProperty("parseInt"));

  EXPECT_TRUE(numberObj->hasProperty("EPSILON"));
  EXPECT_TRUE(numberObj->hasProperty("MAX_VALUE"));
  EXPECT_TRUE(numberObj->hasProperty("MIN_VALUE"));
  EXPECT_TRUE(numberObj->hasProperty("MAX_SAFE_INTEGER"));
  EXPECT_TRUE(numberObj->hasProperty("MIN_SAFE_INTEGER"));
  EXPECT_TRUE(numberObj->hasProperty("POSITIVE_INFINITY"));
  EXPECT_TRUE(numberObj->hasProperty("NEGATIVE_INFINITY"));
  EXPECT_TRUE(numberObj->hasProperty("NaN"));

  // プロトタイプに必要なメソッドが存在することを確認
  ValuePtr prototypeValue = numberObj->get("prototype");
  EXPECT_TRUE(prototypeValue->isObject());

  ObjectPtr prototype = prototypeValue->toObject();
  EXPECT_TRUE(prototype->hasProperty("toExponential"));
  EXPECT_TRUE(prototype->hasProperty("toFixed"));
  EXPECT_TRUE(prototype->hasProperty("toLocaleString"));
  EXPECT_TRUE(prototype->hasProperty("toPrecision"));
  EXPECT_TRUE(prototype->hasProperty("toString"));
  EXPECT_TRUE(prototype->hasProperty("valueOf"));
}

// コンストラクタの機能テスト
TEST_F(NumberModuleTest, ConstructorFunctionality) {
  // モジュール登録
  registerNumberBuiltin(m_globalObject.get());

  // コンストラクタを取得
  ValuePtr numberValue = m_globalObject->get("Number");
  FunctionPtr numberConstructor = std::static_pointer_cast<Function>(numberValue->toObject());

  // 数値を作成
  std::vector<ValuePtr> emptyArgs;
  ValuePtr result1 = numberConstructor->call(Value::undefined(), emptyArgs);
  EXPECT_TRUE(result1->isNumber());
  EXPECT_DOUBLE_EQ(0.0, result1->toNumber());

  std::vector<ValuePtr> args = {Value::createNumber(42.5)};
  ValuePtr result2 = numberConstructor->call(Value::undefined(), args);
  EXPECT_TRUE(result2->isNumber());
  EXPECT_DOUBLE_EQ(42.5, result2->toNumber());

  // new演算子で呼び出し（オブジェクトを作成）
  ValuePtr result3 = numberConstructor->construct(args);
  EXPECT_TRUE(result3->isObject());
  EXPECT_TRUE(Number::isNumberObject(result3->toObject()));
  EXPECT_DOUBLE_EQ(42.5, result3->toNumber());
}

// モジュール登録エラーケースのテスト
TEST_F(NumberModuleTest, RegistrationErrorCases) {
  // nullグローバルオブジェクトでの登録
  EXPECT_NO_THROW(registerNumberBuiltin(nullptr));

  // 複数回の登録
  registerNumberBuiltin(m_globalObject.get());
  EXPECT_NO_THROW(registerNumberBuiltin(m_globalObject.get()));

  // 上書き登録後もNumberが機能することを確認
  ValuePtr numberValue = m_globalObject->get("Number");
  EXPECT_TRUE(numberValue->isFunction());
}

// 静的メソッドの機能テスト
TEST_F(NumberModuleTest, StaticMethodFunctionality) {
  // モジュール登録
  registerNumberBuiltin(m_globalObject.get());

  // 静的メソッドのテスト
  ValuePtr numberValue = m_globalObject->get("Number");
  ObjectPtr numberObj = numberValue->toObject();

  // Number.isFinite
  ValuePtr isFiniteMethod = numberObj->get("isFinite");
  EXPECT_TRUE(isFiniteMethod->isFunction());

  std::vector<ValuePtr> args = {Value::createNumber(42)};
  FunctionPtr isFiniteFn = std::static_pointer_cast<Function>(isFiniteMethod->toObject());
  ValuePtr result = isFiniteFn->call(numberValue, args);
  EXPECT_TRUE(result->isBoolean());
  EXPECT_TRUE(result->toBoolean());

  // Number.parseFloat
  ValuePtr parseFloatMethod = numberObj->get("parseFloat");
  EXPECT_TRUE(parseFloatMethod->isFunction());

  args = {Value::createString("42.5")};
  FunctionPtr parseFloatFn = std::static_pointer_cast<Function>(parseFloatMethod->toObject());
  result = parseFloatFn->call(numberValue, args);
  EXPECT_TRUE(result->isNumber());
  EXPECT_DOUBLE_EQ(42.5, result->toNumber());
}

// 定数の機能テスト
TEST_F(NumberModuleTest, ConstantsFunctionality) {
  // モジュール登録
  registerNumberBuiltin(m_globalObject.get());

  // 定数のテスト
  ValuePtr numberValue = m_globalObject->get("Number");
  ObjectPtr numberObj = numberValue->toObject();

  // EPSILON
  ValuePtr epsilon = numberObj->get("EPSILON");
  EXPECT_TRUE(epsilon->isNumber());
  EXPECT_DOUBLE_EQ(std::numeric_limits<double>::epsilon(), epsilon->toNumber());

  // MAX_SAFE_INTEGER
  ValuePtr maxSafeInteger = numberObj->get("MAX_SAFE_INTEGER");
  EXPECT_TRUE(maxSafeInteger->isNumber());
  EXPECT_DOUBLE_EQ(9007199254740991.0, maxSafeInteger->toNumber());

  // NaN
  ValuePtr nan = numberObj->get("NaN");
  EXPECT_TRUE(nan->isNumber());
  EXPECT_TRUE(std::isnan(nan->toNumber()));
}

}  // namespace test
}  // namespace core
}  // namespace aerojs