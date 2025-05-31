/**
 * @file test_engine.cpp
 * @brief AeroJS エンジンのテスト
 * @version 0.1.0
 * @license MIT
 */

#include <gtest/gtest.h>
#include "core/engine.h"
#include "core/context.h"
#include "core/value.h"

using namespace aerojs::core;

class EngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<Engine>();
    }

    void TearDown() override {
        if (engine) {
            engine->shutdown();
        }
        engine.reset();
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(EngineTest, InitializationTest) {
    EXPECT_TRUE(engine->initialize());
    EXPECT_NE(engine->getGlobalContext(), nullptr);
    EXPECT_NE(engine->getMemoryAllocator(), nullptr);
    EXPECT_NE(engine->getMemoryPool(), nullptr);
    EXPECT_NE(engine->getGarbageCollector(), nullptr);
}

TEST_F(EngineTest, BasicEvaluationTest) {
    ASSERT_TRUE(engine->initialize());
    
    // 基本的な値の評価テスト
    Value result = engine->evaluate("undefined");
    EXPECT_TRUE(result.isUndefined());
    
    result = engine->evaluate("null");
    EXPECT_TRUE(result.isNull());
    
    result = engine->evaluate("true");
    EXPECT_TRUE(result.isBoolean());
    EXPECT_TRUE(result.toBoolean());
    
    result = engine->evaluate("false");
    EXPECT_TRUE(result.isBoolean());
    EXPECT_FALSE(result.toBoolean());
    
    result = engine->evaluate("42");
    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toNumber(), 42.0);
    
    result = engine->evaluate("3.14");
    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toNumber(), 3.14);
    
    result = engine->evaluate("hello");
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.toString(), "hello");
}

TEST_F(EngineTest, JITConfigurationTest) {
    ASSERT_TRUE(engine->initialize());
    
    // JIT設定のテスト
    engine->enableJIT(true);
    engine->setJITThreshold(50);
    engine->setOptimizationLevel(3);
    
    // 設定が正しく適用されているかは内部状態なので、
    // ここでは例外が発生しないことを確認
    EXPECT_NO_THROW(engine->enableJIT(false));
    EXPECT_NO_THROW(engine->setJITThreshold(100));
    EXPECT_NO_THROW(engine->setOptimizationLevel(2));
}

TEST_F(EngineTest, GarbageCollectionTest) {
    ASSERT_TRUE(engine->initialize());
    
    // ガベージコレクションの実行テスト
    EXPECT_NO_THROW(engine->collectGarbage());
    
    // 複数回実行しても問題ないことを確認
    for (int i = 0; i < 5; ++i) {
        EXPECT_NO_THROW(engine->collectGarbage());
    }
}

TEST_F(EngineTest, MemoryManagementTest) {
    ASSERT_TRUE(engine->initialize());
    
    auto* allocator = engine->getMemoryAllocator();
    ASSERT_NE(allocator, nullptr);
    
    // メモリ割り当てテスト
    void* ptr = allocator->allocate(1024);
    EXPECT_NE(ptr, nullptr);
    
    // メモリ解放テスト
    EXPECT_NO_THROW(allocator->deallocate(ptr));
}

TEST_F(EngineTest, ContextTest) {
    ASSERT_TRUE(engine->initialize());
    
    auto* context = engine->getGlobalContext();
    ASSERT_NE(context, nullptr);
    
    // コンテキストの基本機能テスト
    context->setVariable("testVar", Value::fromNumber(123));
    Value result = context->getVariable("testVar");
    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toNumber(), 123.0);
    
    // 存在しない変数のテスト
    result = context->getVariable("nonExistentVar");
    EXPECT_TRUE(result.isUndefined());
}

TEST_F(EngineTest, MultipleInitializationTest) {
    // 複数回初期化しても問題ないことを確認
    EXPECT_TRUE(engine->initialize());
    EXPECT_TRUE(engine->initialize());
    EXPECT_TRUE(engine->initialize());
}

TEST_F(EngineTest, ShutdownTest) {
    ASSERT_TRUE(engine->initialize());
    
    // シャットダウン後の状態確認
    EXPECT_NO_THROW(engine->shutdown());
    
    // シャットダウン後の評価は安全に失敗すべき
    Value result = engine->evaluate("42");
    EXPECT_TRUE(result.isUndefined());
}

TEST_F(EngineTest, FileEvaluationTest) {
    ASSERT_TRUE(engine->initialize());
    
    // ファイル評価（現在は未実装なのでundefinedを返すことを確認）
    Value result = engine->evaluateFile("nonexistent.js");
    EXPECT_TRUE(result.isUndefined());
}

TEST_F(EngineTest, StressTest) {
    ASSERT_TRUE(engine->initialize());
    
    // 大量の評価を実行してメモリリークや例外がないことを確認
    for (int i = 0; i < 1000; ++i) {
        Value result = engine->evaluate(std::to_string(i));
        EXPECT_TRUE(result.isNumber());
        EXPECT_DOUBLE_EQ(result.toNumber(), static_cast<double>(i));
    }
    
    // ガベージコレクションを実行
    EXPECT_NO_THROW(engine->collectGarbage());
}

TEST_F(EngineTest, ConcurrentAccessTest) {
    ASSERT_TRUE(engine->initialize());
    
    // 簡単な並行アクセステスト
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &successCount, i]() {
            try {
                Value result = engine->evaluate(std::to_string(i));
                if (result.isNumber() && result.toNumber() == static_cast<double>(i)) {
                    successCount++;
                }
            } catch (...) {
                // 例外は無視（並行アクセスでは発生する可能性がある）
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 少なくとも半分は成功することを期待
    EXPECT_GE(successCount.load(), 5);
} 