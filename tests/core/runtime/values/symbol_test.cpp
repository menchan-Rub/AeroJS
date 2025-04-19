/**
 * @file symbol_test.cpp
 * @brief Symbolクラスのテスト
 */

#include <gtest/gtest.h>
#include "../../../../src/core/runtime/values/symbol.h"
#include <thread>
#include <vector>
#include <memory>

using namespace aerojs::core;

class SymbolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 各テストケースの前にレジストリをリセットする
        Symbol::resetRegistry();
    }

    void TearDown() override {
        // 各テストケースの後にリソースをクリーンアップする
        Symbol::resetRegistry();
    }
};

// 基本的な生成と説明のテスト
TEST_F(SymbolTest, BasicCreation) {
    auto symbol = Symbol::create("test symbol");
    EXPECT_EQ("test symbol", symbol->description());
    EXPECT_NE(0, symbol->id());
    EXPECT_EQ("Symbol(test symbol)", symbol->toString());
}

// 空の説明のテスト
TEST_F(SymbolTest, EmptyDescription) {
    auto symbol = Symbol::create();
    EXPECT_EQ("", symbol->description());
    EXPECT_EQ("Symbol()", symbol->toString());
}

// 同一性のテスト
TEST_F(SymbolTest, Identity) {
    auto symbol1 = Symbol::create("test");
    auto symbol2 = Symbol::create("test");
    
    // 同じ説明でも異なるシンボル
    EXPECT_NE(symbol1->id(), symbol2->id());
    EXPECT_FALSE(*symbol1 == *symbol2);
    EXPECT_TRUE(*symbol1 != *symbol2);
    EXPECT_FALSE(Symbol::equals(symbol1, symbol2));
}

// グローバルシンボルレジストリのテスト
TEST_F(SymbolTest, GlobalSymbolRegistry) {
    auto symbol1 = Symbol::for_("global test");
    auto symbol2 = Symbol::for_("global test");
    
    // 同じキーなら同じシンボルを返す
    EXPECT_EQ(symbol1->id(), symbol2->id());
    EXPECT_TRUE(*symbol1 == *symbol2);
    EXPECT_TRUE(Symbol::equals(symbol1, symbol2));
    
    // キーの取得テスト
    EXPECT_EQ("global test", Symbol::keyFor(symbol1));
    
    // 存在しないキー
    auto symbol3 = Symbol::create("not registered");
    EXPECT_EQ("", Symbol::keyFor(symbol3));
}

// カスタムシンボル登録のテスト
TEST_F(SymbolTest, CustomSymbolRegistration) {
    auto symbol = Symbol::create("custom symbol");
    Symbol::registerCustomSymbol("my.custom.symbol", symbol);
    
    auto retrieved = Symbol::for_("my.custom.symbol");
    EXPECT_TRUE(Symbol::equals(symbol, retrieved));
    
    // 重複登録テスト（同じシンボルなら成功）
    EXPECT_NO_THROW(Symbol::registerCustomSymbol("my.custom.symbol", symbol));
    
    // 異なるシンボルで同じ名前は例外
    auto different = Symbol::create("different");
    EXPECT_THROW(Symbol::registerCustomSymbol("my.custom.symbol", different), std::invalid_argument);
}

// Well-knownシンボルのテスト
TEST_F(SymbolTest, WellKnownSymbols) {
    // 同一インスタンスであることを確認
    EXPECT_EQ(Symbol::iterator()->id(), Symbol::iterator()->id());
    EXPECT_EQ(Symbol::hasInstance()->id(), Symbol::hasInstance()->id());
    
    // Well-known名前空間からのアクセス
    EXPECT_EQ(Symbol::iterator()->id(), well_known::getIterator()->id());
}

// スレッド安全性のテスト
TEST_F(SymbolTest, ThreadSafety) {
    constexpr int NUM_THREADS = 10;
    constexpr int SYMBOLS_PER_THREAD = 100;
    
    std::vector<std::thread> threads;
    std::atomic<int> successCount(0);
    
    // 複数スレッドからシンボルを作成
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([i, &successCount]() {
            try {
                for (int j = 0; j < SYMBOLS_PER_THREAD; ++j) {
                    std::string key = "thread_" + std::to_string(i) + "_symbol_" + std::to_string(j);
                    auto symbol = Symbol::for_(key);
                    EXPECT_EQ(key, Symbol::keyFor(symbol));
                    successCount++;
                }
            } catch (const std::exception& e) {
                // 例外が発生した場合はテスト失敗
                FAIL() << "Exception in thread " << i << ": " << e.what();
            }
        });
    }
    
    // すべてのスレッドが終了するのを待つ
    for (auto& thread : threads) {
        thread.join();
    }
    
    // すべての操作が成功したことを確認
    EXPECT_EQ(NUM_THREADS * SYMBOLS_PER_THREAD, successCount);
    
    // レジストリサイズが正しいことを確認
    EXPECT_EQ(NUM_THREADS * SYMBOLS_PER_THREAD, Symbol::registrySize());
}

// レジストリリセットと削除のテスト
TEST_F(SymbolTest, RegistryCleanup) {
    auto symbol1 = Symbol::for_("test1");
    auto symbol2 = Symbol::for_("test2");
    
    EXPECT_EQ(2, Symbol::registrySize());
    
    // 特定のシンボルを削除
    EXPECT_TRUE(Symbol::removeFromRegistry(symbol1->id()));
    EXPECT_EQ(1, Symbol::registrySize());
    EXPECT_EQ("", Symbol::keyFor(symbol1));
    
    // 存在しないIDの削除は失敗
    EXPECT_FALSE(Symbol::removeFromRegistry(9999));
    
    // レジストリリセット
    Symbol::resetRegistry();
    EXPECT_EQ(0, Symbol::registrySize());
    
    // リセット後は既存のシンボルキーが取得できない
    EXPECT_EQ("", Symbol::keyFor(symbol2));
}

// ハッシュサポートのテスト
TEST_F(SymbolTest, HashSupport) {
    auto symbol1 = Symbol::create("hash test 1");
    auto symbol2 = Symbol::create("hash test 2");
    
    // 直接のハッシュ計算
    std::hash<Symbol> hasher;
    EXPECT_EQ(static_cast<size_t>(symbol1->id()), hasher(*symbol1));
    
    // シンボルをキーとして使えることを確認
    std::unordered_map<Symbol*, std::string> symbolMap;
    symbolMap[symbol1.get()] = "Value 1";
    symbolMap[symbol2.get()] = "Value 2";
    
    EXPECT_EQ("Value 1", symbolMap[symbol1.get()]);
    EXPECT_EQ("Value 2", symbolMap[symbol2.get()]);
    
    // スマートポインタバージョン
    std::hash<SymbolPtr> ptrHasher;
    EXPECT_EQ(static_cast<size_t>(symbol1->id()), ptrHasher(symbol1));
    EXPECT_EQ(0, ptrHasher(nullptr)); // nullptrは0を返す
    
    // スマートポインタをキーとして使えることを確認
    std::unordered_map<SymbolPtr, std::string, std::hash<SymbolPtr>> ptrMap;
    ptrMap[symbol1] = "Smart 1";
    ptrMap[symbol2] = "Smart 2";
    
    EXPECT_EQ("Smart 1", ptrMap[symbol1]);
    EXPECT_EQ("Smart 2", ptrMap[symbol2]);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 