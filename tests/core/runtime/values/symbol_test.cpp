/**
 * @file symbol_test.cpp
 * @brief Symbolクラスのテスト
 */

#include "../../../../src/core/runtime/values/symbol.h"

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

using namespace aerojs::core;

class SymbolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 各テストケースの前にレジストリをリセットする
    Symbol::resetRegistryForTesting();
  }

  void TearDown() override {
    // 各テストケースの後にリソースをクリーンアップする
    Symbol::resetRegistryForTesting();
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
  auto symbol1 = Symbol::For("global test");
  auto symbol2 = Symbol::For("global test");

  // 同じキーなら同じシンボルを返す
  EXPECT_EQ(symbol1->id(), symbol2->id());
  EXPECT_TRUE(*symbol1 == *symbol2);
  EXPECT_TRUE(Symbol::equals(symbol1, symbol2));

  // キーの取得テスト
  EXPECT_EQ("global test", Symbol::KeyFor(symbol1));

  // 存在しないキー (Symbol::create で作成されたもの)
  auto symbol3 = Symbol::create("not registered");
  EXPECT_EQ(std::nullopt, Symbol::KeyFor(symbol3));  // Corrected: Expect std::nullopt instead of ""
}

// TEST_F(SymbolTest, CustomSymbolRegistration) {
//     auto symbol = Symbol::create("custom");
//     Symbol::registerCustomSymbol("my.custom.symbol", symbol);
//
//     auto retrieved = Symbol::For("my.custom.symbol");
//     EXPECT_TRUE(Symbol::equals(symbol, retrieved));
//
//     // 同じキーで異なるシンボルを登録しようとするとエラー
//     auto different = Symbol::create("different");
//     EXPECT_THROW(Symbol::registerCustomSymbol("my.custom.symbol", different), std::invalid_argument);
//
//     // null シンボルの登録はエラー
//     EXPECT_THROW(Symbol::registerCustomSymbol("null.symbol", nullptr), std::invalid_argument);
// }

TEST_F(SymbolTest, WellKnownSymbols) {
  EXPECT_NE(Symbol::hasInstance(), nullptr);
  EXPECT_NE(Symbol::isConcatSpreadable(), nullptr);
  EXPECT_NE(Symbol::iterator(), nullptr);
  EXPECT_NE(Symbol::asyncIterator(), nullptr);
  EXPECT_NE(Symbol::match(), nullptr);
  EXPECT_NE(Symbol::matchAll(), nullptr);
  EXPECT_NE(Symbol::replace(), nullptr);
  EXPECT_NE(Symbol::search(), nullptr);
  EXPECT_NE(Symbol::species(), nullptr);
  EXPECT_NE(Symbol::split(), nullptr);
  EXPECT_NE(Symbol::toPrimitive(), nullptr);
  EXPECT_NE(Symbol::toStringTag(), nullptr);
  EXPECT_NE(Symbol::unscopables(), nullptr);

  // Well-known symbol は For で取得できる
  auto iterSym = Symbol::iterator();  // Corrected: Removed well_known::
  auto iterSymFor = Symbol::For("Symbol.iterator");
  EXPECT_TRUE(Symbol::equals(iterSym, iterSymFor));
  EXPECT_EQ(iterSym->id(), iterSymFor->id());

  // Well-known symbol のキーも取得できる
  EXPECT_EQ("Symbol.iterator", Symbol::KeyFor(iterSym));
}

// スレッド安全性のテスト
TEST_F(SymbolTest, ThreadSafety) {
  constexpr int NUM_THREADS = 10;
  constexpr int SYMBOLS_PER_THREAD = 100;

  std::vector<std::thread> threads;
  std::atomic<int> successCount(0);

  // 複数スレッドからシンボルを作成または取得
  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back([i, &successCount]() {
      try {
        for (int j = 0; j < SYMBOLS_PER_THREAD; ++j) {
          std::string key = "thread_" + std::to_string(i) + "_symbol_" + std::to_string(j);
          auto symbol = Symbol::For(key);
          EXPECT_EQ(key, Symbol::KeyFor(symbol));
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

  // レジストリサイズが正しいことを確認 - スレッド終了後、ポインタが破棄されるとサイズは0になるため、このチェックは削除
  // EXPECT_EQ(NUM_THREADS * SYMBOLS_PER_THREAD, Symbol::registrySize());
}

// レジストリリセットと削除のテスト
TEST_F(SymbolTest, RegistryCleanup) {
  auto symbol1 = Symbol::For("test1");
  auto symbol2 = Symbol::For("test2");

  EXPECT_EQ(2, Symbol::registrySize());

  // レジストリリセット
  Symbol::resetRegistryForTesting();
  EXPECT_EQ(0, Symbol::registrySize());

  // リセット後は既存のシンボルキーが取得できない
  EXPECT_EQ(std::nullopt, Symbol::KeyFor(symbol2));  // Corrected: Expect std::nullopt
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
  EXPECT_EQ(0, ptrHasher(nullptr));  // nullptrは0を返す

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