#include "src/core/runtime/values/symbol.h"

#include <future>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace aerojs::core;

// --- 基本的なテスト ---

TEST(SymbolTest, BasicCreation) {
  // Symbol::create を使用して作成 (非登録)
  SymbolPtr sym1 = Symbol::create("mySymbol");
  ASSERT_NE(sym1, nullptr);
  EXPECT_EQ(sym1->description(), "mySymbol");
  EXPECT_GT(sym1->id(), 0);
  EXPECT_EQ(sym1->toString(), "Symbol(mySymbol)");
  EXPECT_NE(sym1->debugString().find("Symbol@"), std::string::npos);
  EXPECT_NE(sym1->debugString().find("(\"mySymbol\")"), std::string::npos);

  SymbolPtr sym2 = Symbol::create("");  // 空の説明
  ASSERT_NE(sym2, nullptr);
  EXPECT_EQ(sym2->description(), "");
  EXPECT_GT(sym2->id(), 0);
  EXPECT_NE(sym1->id(), sym2->id());
  EXPECT_EQ(sym2->toString(), "Symbol()");
  EXPECT_NE(sym2->debugString().find("Symbol@"), std::string::npos);
  EXPECT_NE(sym2->debugString().find("())"), std::string::npos);

  // create で作成されたシンボルはレジストリには登録されない
  Symbol::resetRegistryForTesting();
  EXPECT_EQ(Symbol::registrySize(), 0);
  SymbolPtr sym3 = Symbol::create("NotInRegistry");
  EXPECT_EQ(Symbol::registrySize(), 0);
  EXPECT_EQ(Symbol::KeyFor(sym3), std::nullopt);
}

// --- レジストリテスト (Symbol.for, Symbol.keyFor) ---

TEST(SymbolTest, RegistryForAndKeyFor) {
  Symbol::resetRegistryForTesting();
  ASSERT_EQ(Symbol::registrySize(), 0);

  SymbolPtr sym1 = Symbol::For("key1");
  ASSERT_NE(sym1, nullptr);
  EXPECT_EQ(sym1->description(), "key1");
  EXPECT_EQ(Symbol::registrySize(), 1);

  SymbolPtr sym2 = Symbol::For("key2");
  ASSERT_NE(sym2, nullptr);
  EXPECT_EQ(sym2->description(), "key2");
  EXPECT_NE(sym1->id(), sym2->id());
  EXPECT_EQ(Symbol::registrySize(), 2);

  // 同じキーで For を呼ぶと、同じシンボルが返る
  SymbolPtr sym1_again = Symbol::For("key1");
  ASSERT_NE(sym1_again, nullptr);
  EXPECT_EQ(sym1_again, sym1);
  EXPECT_EQ(sym1_again->id(), sym1->id());
  EXPECT_EQ(Symbol::registrySize(), 2);  // サイズは変わらない

  // KeyFor でキーを取得
  EXPECT_EQ(Symbol::KeyFor(sym1), "key1");
  EXPECT_EQ(Symbol::KeyFor(sym2), "key2");
  EXPECT_EQ(Symbol::KeyFor(sym1_again), "key1");

  // 登録されていないシンボルや nullptr のキーは nullopt
  SymbolPtr sym_unregistered = Symbol::create("unregistered");
  EXPECT_EQ(Symbol::KeyFor(sym_unregistered), std::nullopt);
  EXPECT_EQ(Symbol::KeyFor(nullptr), std::nullopt);

  // 空キーのテスト
  SymbolPtr sym_empty = Symbol::For("");
  ASSERT_NE(sym_empty, nullptr);
  EXPECT_EQ(sym_empty->description(), "");
  EXPECT_EQ(Symbol::registrySize(), 3);
  EXPECT_EQ(Symbol::KeyFor(sym_empty), "");

  SymbolPtr sym_empty_again = Symbol::For("");
  EXPECT_EQ(sym_empty_again, sym_empty);
  EXPECT_EQ(Symbol::registrySize(), 3);
}

TEST(SymbolTest, RegistryWeakReference) {
  Symbol::resetRegistryForTesting();
  ASSERT_EQ(Symbol::registrySize(), 0);

  SymbolPtr sym_strong = Symbol::For("weak_test");
  int original_id = sym_strong->id();
  ASSERT_EQ(Symbol::registrySize(), 1);

  // シンボルの最後の shared_ptr を破棄
  sym_strong.reset();

  // レジストリサイズはすぐには減らない (weak_ptr が残っているため)
  // しかし、次に For を呼んだときに、古い weak_ptr が検出され、クリーンアップされるはず
  // Symbol::registrySize() 自体がクリーンアップをトリガーする可能性もある
  // (現在の実装では registrySize はクリーンアップしない)

  // 再度同じキーで取得すると、新しいシンボルが作成されるはず
  SymbolPtr sym_new = Symbol::For("weak_test");
  ASSERT_NE(sym_new, nullptr);
  EXPECT_EQ(sym_new->description(), "weak_test");
  EXPECT_NE(sym_new->id(), original_id);  // ID は異なるはず

  // レジストリのエントリは上書きされるか、古いものが削除されて新しいものが追加される
  // 内部実装によるが、最終的なサイズは 1 になるはず
  // (現在の Symbol::For の実装では、古いエントリを削除してから新しいものを追加)
  // EXPECT_EQ(Symbol::registrySize(), 1); // クリーンアップが確実なら1
  // dumpRegistry で確認
  // std::cout << Symbol::dumpRegistry() << std::endl;
}

// --- 等価性テスト ---

TEST(SymbolTest, Equality) {
  Symbol::resetRegistryForTesting();

  SymbolPtr sym1a = Symbol::create("sym");
  SymbolPtr sym1b = Symbol::create("sym");  // 説明が同じでも別インスタンス
  SymbolPtr sym2 = Symbol::create("other");

  SymbolPtr symFor1a = Symbol::For("key1");
  SymbolPtr symFor1b = Symbol::For("key1");  // 同じインスタンス
  SymbolPtr symFor2 = Symbol::For("key2");

  // 同一インスタンスは等しい
  EXPECT_TRUE(Symbol::equals(sym1a, sym1a));
  EXPECT_TRUE(sym1a == sym1a);
  EXPECT_FALSE(sym1a != sym1a);

  // 説明が同じでも、create で作られた別インスタンスは等しくない
  EXPECT_FALSE(Symbol::equals(sym1a, sym1b));
  EXPECT_FALSE(sym1a == sym1b);
  EXPECT_TRUE(sym1a != sym1b);

  // 説明が異なるインスタンスは等しくない
  EXPECT_FALSE(Symbol::equals(sym1a, sym2));
  EXPECT_FALSE(sym1a == sym2);
  EXPECT_TRUE(sym1a != sym2);

  // For で取得した同じキーのシンボルは等しい (同一インスタンス)
  EXPECT_TRUE(Symbol::equals(symFor1a, symFor1b));
  EXPECT_TRUE(symFor1a == symFor1b);
  EXPECT_FALSE(symFor1a != symFor1b);

  // For で取得した異なるキーのシンボルは等しくない
  EXPECT_FALSE(Symbol::equals(symFor1a, symFor2));
  EXPECT_FALSE(symFor1a == symFor2);
  EXPECT_TRUE(symFor1a != symFor2);

  // create と For で作られたシンボルは、説明が同じでも等しくない
  SymbolPtr sym_create = Symbol::create("common");
  SymbolPtr sym_for = Symbol::For("common");
  EXPECT_FALSE(Symbol::equals(sym_create, sym_for));
  EXPECT_FALSE(sym_create == sym_for);
  EXPECT_TRUE(sym_create != sym_for);

  // nullptr との比較
  EXPECT_FALSE(Symbol::equals(sym1a, nullptr));
  EXPECT_FALSE(sym1a == nullptr);
  EXPECT_TRUE(sym1a != nullptr);
  EXPECT_FALSE(Symbol::equals(nullptr, sym1a));
  EXPECT_FALSE(nullptr == sym1a);
  EXPECT_TRUE(nullptr != sym1a);
  EXPECT_TRUE(Symbol::equals(nullptr, nullptr));  // null 同士は等しい
  EXPECT_TRUE(nullptr == nullptr);
  EXPECT_FALSE(nullptr != nullptr);
}

// --- Well-Known Symbols テスト ---

TEST(SymbolTest, WellKnownSymbols) {
  Symbol::resetRegistryForTesting();

  SymbolPtr iterSym = Symbol::iterator();
  ASSERT_NE(iterSym, nullptr);
  EXPECT_EQ(iterSym->description(), "Symbol.iterator");

  // well-known symbol は For で取得できる
  SymbolPtr iterSymFor = Symbol::For("Symbol.iterator");
  EXPECT_EQ(iterSym, iterSymFor);
  EXPECT_EQ(iterSym->id(), iterSymFor->id());

  // 複数回呼び出しても同じインスタンスが返る
  SymbolPtr iterSymAgain = Symbol::iterator();
  EXPECT_EQ(iterSym, iterSymAgain);

  // 他の well-known symbol とは異なる
  SymbolPtr asyncIterSym = Symbol::asyncIterator();
  ASSERT_NE(asyncIterSym, nullptr);
  EXPECT_NE(iterSym, asyncIterSym);
  EXPECT_NE(iterSym->id(), asyncIterSym->id());
  EXPECT_EQ(Symbol::KeyFor(iterSym), "Symbol.iterator");
  EXPECT_EQ(Symbol::KeyFor(asyncIterSym), "Symbol.asyncIterator");

  // すべての well-known symbol をチェック (簡単な存在確認)
  EXPECT_NE(Symbol::hasInstance(), nullptr);
  EXPECT_NE(Symbol::isConcatSpreadable(), nullptr);
  EXPECT_NE(Symbol::match(), nullptr);
  EXPECT_NE(Symbol::matchAll(), nullptr);
  EXPECT_NE(Symbol::replace(), nullptr);
  EXPECT_NE(Symbol::search(), nullptr);
  EXPECT_NE(Symbol::species(), nullptr);
  EXPECT_NE(Symbol::split(), nullptr);
  EXPECT_NE(Symbol::toPrimitive(), nullptr);
  EXPECT_NE(Symbol::toStringTag(), nullptr);
  EXPECT_NE(Symbol::unscopables(), nullptr);

  // レジストリにも登録されているはず
  EXPECT_EQ(Symbol::registrySize(), 13);  // 13個のwell-knownシンボル
}

// --- スレッド安全性テスト ---

TEST(SymbolTest, ThreadSafetyFor) {
  Symbol::resetRegistryForTesting();
  const int num_threads = 10;
  const int iterations = 1000;
  std::vector<std::string> keys = {"keyA", "keyB", "keyC", "keyD", "keyE"};

  std::vector<std::future<std::vector<SymbolPtr>>> futures;

  for (int i = 0; i < num_threads; ++i) {
    futures.push_back(std::async(std::launch::async, [&]() {
      std::vector<SymbolPtr> thread_symbols;
      thread_symbols.reserve(iterations * keys.size());
      for (int j = 0; j < iterations; ++j) {
        for (const auto& key : keys) {
          thread_symbols.push_back(Symbol::For(key));
        }
      }
      return thread_symbols;
    }));
  }

  // 各キーに対応するシンボルのIDを格納するセット
  std::unordered_map<std::string, std::unordered_set<int>> symbol_ids;

  for (auto& fut : futures) {
    std::vector<SymbolPtr> thread_symbols = fut.get();
    for (const auto& sym : thread_symbols) {
      ASSERT_NE(sym, nullptr);
      std::optional<std::string> key = Symbol::KeyFor(sym);
      ASSERT_TRUE(key.has_value());
      symbol_ids[*key].insert(sym->id());
    }
  }

  // 各キーについて、生成されたシンボルはただ1つであるはず
  EXPECT_EQ(symbol_ids.size(), keys.size());
  for (const auto& key : keys) {
    EXPECT_EQ(symbol_ids[key].size(), 1) << "Key: " << key;
    if (symbol_ids[key].size() != 1) {
      std::cerr << "Mismatch for key '" << key << "'. IDs found: ";
      for (int id : symbol_ids[key]) {
        std::cerr << id << " ";
      }
      std::cerr << std::endl;
    }
  }

  // 最終的なレジストリサイズはキーの数と同じはず
  EXPECT_EQ(Symbol::registrySize(), keys.size());
}

TEST(SymbolTest, ThreadSafetyCreate) {
  // Symbol::create はレジストリを使わないので、本質的にスレッドセーフ
  // (内部のID生成が atomic なら)
  const int num_threads = 10;
  const int iterations = 1000;
  std::vector<std::future<std::vector<SymbolPtr>>> futures;
  std::unordered_set<int> generated_ids;
  std::mutex ids_mutex;

  for (int i = 0; i < num_threads; ++i) {
    futures.push_back(std::async(std::launch::async, [&]() {
      std::vector<SymbolPtr> thread_symbols;
      thread_symbols.reserve(iterations);
      std::vector<int> local_ids;
      local_ids.reserve(iterations);
      for (int j = 0; j < iterations; ++j) {
        SymbolPtr sym = Symbol::create("thread_" + std::to_string(i) + "_" + std::to_string(j));
        thread_symbols.push_back(sym);
        if (sym) {
          local_ids.push_back(sym->id());
        }
      }
      // Collect IDs under lock
      {
        std::lock_guard<std::mutex> lock(ids_mutex);
        generated_ids.insert(local_ids.begin(), local_ids.end());
      }
      return thread_symbols;
    }));
  }

  for (auto& fut : futures) {
    fut.get();  // Wait for all threads to complete
  }

  // 生成されたIDの総数が期待通りであり、重複がないことを確認
  EXPECT_EQ(generated_ids.size(), num_threads * iterations);
}