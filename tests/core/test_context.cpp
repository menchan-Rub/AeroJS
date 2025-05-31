/**
 * @file test_context.cpp
 * @brief AeroJS Context クラスのテスト
 * @version 0.1.0
 * @license MIT
 */

#include <gtest/gtest.h>
#include "core/context.h"
#include "core/engine.h"
#include "core/value.h"
#include <memory>

using namespace aerojs::core;

class ContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<Engine>();
        ASSERT_TRUE(engine->initialize());
        context = engine->getGlobalContext();
        ASSERT_NE(context, nullptr);
    }

    void TearDown() override {
        if (engine) {
            engine->shutdown();
        }
        engine.reset();
        context = nullptr;
    }

    std::unique_ptr<Engine> engine;
    Context* context;
};

TEST_F(ContextTest, BasicVariableOperationsTest) {
    // 変数の設定と取得
    context->setVariable("testVar", Value::fromNumber(42));
    Value result = context->getVariable("testVar");
    
    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toNumber(), 42.0);
    
    // 存在しない変数の取得
    result = context->getVariable("nonExistentVar");
    EXPECT_TRUE(result.isUndefined());
    
    // 変数の存在確認
    EXPECT_TRUE(context->hasVariable("testVar"));
    EXPECT_FALSE(context->hasVariable("nonExistentVar"));
}

TEST_F(ContextTest, VariableTypesTest) {
    // 様々な型の変数をテスト
    context->setVariable("undefinedVar", Value::undefined());
    context->setVariable("nullVar", Value::null());
    context->setVariable("boolVar", Value::fromBoolean(true));
    context->setVariable("numberVar", Value::fromNumber(3.14));
    context->setVariable("stringVar", Value::fromString("hello"));
    
    EXPECT_TRUE(context->getVariable("undefinedVar").isUndefined());
    EXPECT_TRUE(context->getVariable("nullVar").isNull());
    EXPECT_TRUE(context->getVariable("boolVar").isBoolean());
    EXPECT_TRUE(context->getVariable("numberVar").isNumber());
    EXPECT_TRUE(context->getVariable("stringVar").isString());
    
    EXPECT_TRUE(context->getVariable("boolVar").toBoolean());
    EXPECT_DOUBLE_EQ(context->getVariable("numberVar").toNumber(), 3.14);
    EXPECT_EQ(context->getVariable("stringVar").toString(), "hello");
}

TEST_F(ContextTest, VariableOverwriteTest) {
    // 変数の上書きテスト
    context->setVariable("var", Value::fromNumber(1));
    EXPECT_DOUBLE_EQ(context->getVariable("var").toNumber(), 1.0);
    
    context->setVariable("var", Value::fromString("overwritten"));
    EXPECT_TRUE(context->getVariable("var").isString());
    EXPECT_EQ(context->getVariable("var").toString(), "overwritten");
    
    context->setVariable("var", Value::fromBoolean(false));
    EXPECT_TRUE(context->getVariable("var").isBoolean());
    EXPECT_FALSE(context->getVariable("var").toBoolean());
}

TEST_F(ContextTest, VariableRemovalTest) {
    // 変数の削除テスト
    context->setVariable("tempVar", Value::fromNumber(123));
    EXPECT_TRUE(context->hasVariable("tempVar"));
    
    context->removeVariable("tempVar");
    EXPECT_FALSE(context->hasVariable("tempVar"));
    
    Value result = context->getVariable("tempVar");
    EXPECT_TRUE(result.isUndefined());
    
    // 存在しない変数の削除（エラーにならないことを確認）
    EXPECT_NO_THROW(context->removeVariable("nonExistentVar"));
}

TEST_F(ContextTest, GlobalObjectTest) {
    // グローバルオブジェクトの取得
    Value globalObject = context->getGlobalObject();
    EXPECT_TRUE(globalObject.isObject());
    EXPECT_NE(globalObject.toObject(), nullptr);
}

TEST_F(ContextTest, BuiltinVariablesTest) {
    // 組み込み変数のテスト
    EXPECT_TRUE(context->hasVariable("undefined"));
    EXPECT_TRUE(context->hasVariable("null"));
    EXPECT_TRUE(context->hasVariable("true"));
    EXPECT_TRUE(context->hasVariable("false"));
    EXPECT_TRUE(context->hasVariable("NaN"));
    EXPECT_TRUE(context->hasVariable("Infinity"));
    
    // 組み込み変数の値確認
    EXPECT_TRUE(context->getVariable("undefined").isUndefined());
    EXPECT_TRUE(context->getVariable("null").isNull());
    EXPECT_TRUE(context->getVariable("true").toBoolean());
    EXPECT_FALSE(context->getVariable("false").toBoolean());
    
    Value nanValue = context->getVariable("NaN");
    EXPECT_TRUE(nanValue.isNumber());
    EXPECT_TRUE(std::isnan(nanValue.toNumber()));
    
    Value infinityValue = context->getVariable("Infinity");
    EXPECT_TRUE(infinityValue.isNumber());
    EXPECT_TRUE(std::isinf(infinityValue.toNumber()));
}

TEST_F(ContextTest, EvaluationTest) {
    // コンテキストでの評価テスト
    Value result = context->evaluate("42");
    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toNumber(), 42.0);
    
    result = context->evaluate("hello");
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.toString(), "hello");
}

TEST_F(ContextTest, EngineAccessTest) {
    // エンジンへのアクセステスト
    Engine* enginePtr = context->getEngine();
    EXPECT_EQ(enginePtr, engine.get());
    EXPECT_NE(enginePtr, nullptr);
}

TEST_F(ContextTest, GarbageCollectionTest) {
    // ガベージコレクションのテスト
    EXPECT_NO_THROW(context->collectGarbage());
    
    // GC後も変数が保持されていることを確認
    context->setVariable("persistentVar", Value::fromString("persistent"));
    context->collectGarbage();
    
    EXPECT_TRUE(context->hasVariable("persistentVar"));
    EXPECT_EQ(context->getVariable("persistentVar").toString(), "persistent");
}

TEST_F(ContextTest, VariableCountTest) {
    // 変数数の取得テスト
    size_t initialCount = context->getVariableCount();
    
    context->setVariable("var1", Value::fromNumber(1));
    context->setVariable("var2", Value::fromNumber(2));
    context->setVariable("var3", Value::fromNumber(3));
    
    EXPECT_EQ(context->getVariableCount(), initialCount + 3);
    
    context->removeVariable("var2");
    EXPECT_EQ(context->getVariableCount(), initialCount + 2);
}

TEST_F(ContextTest, VariableNamesTest) {
    // 変数名一覧の取得テスト
    context->setVariable("alpha", Value::fromNumber(1));
    context->setVariable("beta", Value::fromNumber(2));
    context->setVariable("gamma", Value::fromNumber(3));
    
    std::vector<std::string> names = context->getVariableNames();
    
    // 設定した変数が含まれていることを確認
    bool hasAlpha = std::find(names.begin(), names.end(), "alpha") != names.end();
    bool hasBeta = std::find(names.begin(), names.end(), "beta") != names.end();
    bool hasGamma = std::find(names.begin(), names.end(), "gamma") != names.end();
    
    EXPECT_TRUE(hasAlpha);
    EXPECT_TRUE(hasBeta);
    EXPECT_TRUE(hasGamma);
}

TEST_F(ContextTest, ClearVariablesTest) {
    // 変数のクリアテスト
    context->setVariable("temp1", Value::fromNumber(1));
    context->setVariable("temp2", Value::fromNumber(2));
    context->setVariable("temp3", Value::fromNumber(3));
    
    EXPECT_TRUE(context->hasVariable("temp1"));
    EXPECT_TRUE(context->hasVariable("temp2"));
    EXPECT_TRUE(context->hasVariable("temp3"));
    
    context->clearVariables();
    
    EXPECT_FALSE(context->hasVariable("temp1"));
    EXPECT_FALSE(context->hasVariable("temp2"));
    EXPECT_FALSE(context->hasVariable("temp3"));
    
    // 組み込み変数は復元されることを確認
    EXPECT_TRUE(context->hasVariable("undefined"));
    EXPECT_TRUE(context->hasVariable("null"));
}

TEST_F(ContextTest, LargeVariableSetTest) {
    // 大量の変数を扱うテスト
    const int variableCount = 1000;
    
    for (int i = 0; i < variableCount; ++i) {
        std::string varName = "var" + std::to_string(i);
        context->setVariable(varName, Value::fromNumber(i));
    }
    
    // 全ての変数が正しく設定されていることを確認
    for (int i = 0; i < variableCount; ++i) {
        std::string varName = "var" + std::to_string(i);
        EXPECT_TRUE(context->hasVariable(varName));
        
        Value value = context->getVariable(varName);
        EXPECT_TRUE(value.isNumber());
        EXPECT_DOUBLE_EQ(value.toNumber(), static_cast<double>(i));
    }
}

TEST_F(ContextTest, SpecialCharacterVariableNamesTest) {
    // 特殊文字を含む変数名のテスト
    context->setVariable("$special", Value::fromString("dollar"));
    context->setVariable("_underscore", Value::fromString("underscore"));
    context->setVariable("var123", Value::fromString("alphanumeric"));
    context->setVariable("日本語変数", Value::fromString("japanese"));
    
    EXPECT_TRUE(context->hasVariable("$special"));
    EXPECT_TRUE(context->hasVariable("_underscore"));
    EXPECT_TRUE(context->hasVariable("var123"));
    EXPECT_TRUE(context->hasVariable("日本語変数"));
    
    EXPECT_EQ(context->getVariable("$special").toString(), "dollar");
    EXPECT_EQ(context->getVariable("_underscore").toString(), "underscore");
    EXPECT_EQ(context->getVariable("var123").toString(), "alphanumeric");
    EXPECT_EQ(context->getVariable("日本語変数").toString(), "japanese");
}

TEST_F(ContextTest, EmptyVariableNameTest) {
    // 空の変数名のテスト
    context->setVariable("", Value::fromString("empty"));
    EXPECT_TRUE(context->hasVariable(""));
    EXPECT_EQ(context->getVariable("").toString(), "empty");
    
    context->removeVariable("");
    EXPECT_FALSE(context->hasVariable(""));
} 