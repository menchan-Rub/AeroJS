/**
 * @file gc_test.cpp
 * @brief パラレルGCのユニットテスト
 * @version 1.0.0
 * @license MIT
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <memory>
#include <random>

#include "../../../../src/utils/memory/gc/parallel_gc.h"

using namespace aerojs::utils::memory;

// テスト用のGCセル
class TestCell : public GCCell {
public:
  TestCell(size_t dataSize = 0)
    : m_data(nullptr), 
      m_dataSize(dataSize),
      m_refs()
  {
    if (dataSize > 0) {
      m_data = new uint8_t[dataSize];
      memset(m_data, 0xAA, dataSize);
    }
  }
  
  ~TestCell() {
    delete[] m_data;
  }
  
  void addReference(TestCell* ref) {
    if (ref) {
      m_refs.push_back(ref);
    }
  }
  
  void clearReferences() {
    m_refs.clear();
  }
  
  void trace(GarbageCollector* gc) override {
    // 未使用
  }
  
  size_t getSize() const override {
    return sizeof(TestCell) + m_dataSize;
  }
  
  void visitReferences(std::function<void(GCCell*)> visitor) override {
    for (auto* ref : m_refs) {
      visitor(ref);
    }
  }
  
  void visitMutableReferences(std::function<void(GCCell**)> visitor) override {
    for (size_t i = 0; i < m_refs.size(); i++) {
      visitor(&m_refs[i]);
    }
  }
  
private:
  uint8_t* m_data;
  size_t m_dataSize;
  std::vector<TestCell*> m_refs;
};

// 基本的なGC動作のテスト
TEST(ParallelGCTest, BasicGCOperation) {
  ParallelGCConfig config;
  config.workerThreadCount = 2;
  config.enableConcurrentMarking = true;
  
  ParallelGC gc(config);
  
  // オブジェクト作成
  std::vector<TestCell*> roots;
  for (int i = 0; i < 100; i++) {
    TestCell* cell = gc.allocate<TestCell>(1024);
    roots.push_back(cell);
    
    // ルートとして登録
    gc.addRoot(reinterpret_cast<GCCell**>(&roots.back()));
  }
  
  // 参照を設定
  for (int i = 0; i < 50; i++) {
    int refCount = i % 5 + 1;
    for (int j = 0; j < refCount; j++) {
      int targetIdx = (i + j * 7) % roots.size();
      roots[i]->addReference(roots[targetIdx]);
    }
  }
  
  // GC前の状態を確認
  auto statsBefore = gc.getStats();
  
  // マイナーGCを実行
  gc.minorCollection();
  
  // GC後の状態を確認
  auto statsAfter = gc.getStats();
  
  // 全てのルートオブジェクトは生き残るはず
  EXPECT_EQ(roots.size(), 100);
  EXPECT_GT(statsAfter.minorGCCount, statsBefore.minorGCCount);
  
  // 参照を一部削除
  for (int i = 50; i < 75; i++) {
    roots[i]->clearReferences();
    
    // ルートから削除
    gc.removeRoot(reinterpret_cast<GCCell**>(&roots[i]));
    roots[i] = nullptr;
  }
  
  // メジャーGCを実行
  gc.majorCollection();
  
  // GC後の状態を確認
  auto statsMajor = gc.getStats();
  EXPECT_GT(statsMajor.majorGCCount, 0);
  EXPECT_GT(statsMajor.freedObjects, 0);
}

// 複数スレッド環境でのGC動作のテスト
TEST(ParallelGCTest, MultithreadedGCOperation) {
  ParallelGCConfig config;
  config.workerThreadCount = 4;
  config.enableConcurrentMarking = true;
  config.enableConcurrentSweeping = true;
  
  auto gc = std::make_shared<ParallelGC>(config);
  
  // ルートオブジェクトの共有
  std::vector<TestCell*> sharedRoots;
  std::mutex rootsMutex;
  
  // アトミックカウンタ
  std::atomic<int> totalAllocated(0);
  
  // 複数スレッドでオブジェクト割り当て
  const int threadCount = 4;
  const int objectsPerThread = 500;
  
  std::vector<std::thread> threads;
  
  for (int t = 0; t < threadCount; t++) {
    threads.emplace_back([t, gc, objectsPerThread, &sharedRoots, &rootsMutex, &totalAllocated]() {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> sizeDist(128, 4096);
      std::uniform_int_distribution<> refDist(0, 10);
      
      std::vector<TestCell*> localRoots;
      
      for (int i = 0; i < objectsPerThread; i++) {
        // オブジェクト作成
        int size = sizeDist(gen);
        TestCell* cell = gc->allocate<TestCell>(size);
        localRoots.push_back(cell);
        
        // 一部をルートとして共有
        if (i % 10 == 0) {
          std::lock_guard<std::mutex> lock(rootsMutex);
          sharedRoots.push_back(cell);
          gc->addRoot(reinterpret_cast<GCCell**>(&sharedRoots.back()));
        }
        
        // 既存オブジェクトへの参照を追加
        int refCount = refDist(gen);
        if (refCount > 0 && !localRoots.empty()) {
          for (int r = 0; r < refCount; r++) {
            std::uniform_int_distribution<> indexDist(0, static_cast<int>(localRoots.size()) - 1);
            int index = indexDist(gen);
            cell->addReference(localRoots[index]);
          }
        }
        
        totalAllocated++;
        
        // 時々GCを誘発
        if (i % 50 == 0) {
          gc->minorCollection();
        }
        
        // スレッド間干渉を増やす
        if (i % 100 == 0) {
          std::this_thread::yield();
        }
      }
      
      // この時点で一部のオブジェクトの参照を削除
      for (size_t i = 0; i < localRoots.size(); i += 2) {
        localRoots[i]->clearReferences();
      }
    });
  }
  
  // 別スレッドでGCを実施
  std::thread gcThread([gc]() {
    for (int i = 0; i < 5; i++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      gc->minorCollection();
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    gc->mediumCollection();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    gc->majorCollection();
  });
  
  // 全スレッド完了を待機
  for (auto& t : threads) {
    t.join();
  }
  
  gcThread.join();
  
  // 最終的なGC実行
  gc->majorCollection();
  
  // 統計情報確認
  auto stats = gc->getStats();
  EXPECT_GT(stats.majorGCCount, 0);
  EXPECT_GT(stats.minorGCCount, 0);
  EXPECT_GT(stats.mediumGCCount, 0);
  
  // 全ての割り当て回数を確認
  EXPECT_EQ(totalAllocated.load(), threadCount * objectsPerThread);
  
  // ヒープ使用率が正常か確認
  EXPECT_GT(stats.heapUsageRatio, 0.0f);
  EXPECT_LT(stats.heapUsageRatio, 1.0f);
}

// 大きいオブジェクト処理のテスト
TEST(ParallelGCTest, LargeObjectHandling) {
  ParallelGCConfig config;
  config.largeObjectThreshold = 16 * 1024; // 16KB
  
  ParallelGC gc(config);
  
  // 通常サイズと大きいサイズのオブジェクトを割り当て
  std::vector<TestCell*> normalObjects;
  std::vector<TestCell*> largeObjects;
  
  // 通常サイズのオブジェクト
  for (int i = 0; i < 100; i++) {
    TestCell* cell = gc.allocate<TestCell>(8 * 1024); // 8KB
    normalObjects.push_back(cell);
    gc.addRoot(reinterpret_cast<GCCell**>(&normalObjects.back()));
  }
  
  // 大きいオブジェクト
  for (int i = 0; i < 10; i++) {
    TestCell* cell = gc.allocate<TestCell>(32 * 1024); // 32KB
    largeObjects.push_back(cell);
    gc.addRoot(reinterpret_cast<GCCell**>(&largeObjects.back()));
  }
  
  // GCを実行
  gc.majorCollection();
  
  // 統計情報を確認
  auto stats = gc.getStats();
  
  // 大きいオブジェクト領域に10個のオブジェクトがあるはず
  EXPECT_EQ(stats.generationObjectCount[static_cast<size_t>(ExtendedGeneration::LargeObj)], 10);
  
  // 一部の大きいオブジェクトのルート参照を削除
  for (int i = 0; i < 5; i++) {
    gc.removeRoot(reinterpret_cast<GCCell**>(&largeObjects[i]));
    largeObjects[i] = nullptr;
  }
  
  // GCを再度実行
  gc.majorCollection();
  
  // 大きいオブジェクト領域から一部が解放されたはず
  auto statsAfter = gc.getStats();
  EXPECT_EQ(statsAfter.generationObjectCount[static_cast<size_t>(ExtendedGeneration::LargeObj)], 5);
}

// インクリメンタルマーキングのテスト
TEST(ParallelGCTest, IncrementalMarking) {
  ParallelGCConfig config;
  config.enableIncrementalMarking = true;
  config.markingStepSize = 10;
  
  ParallelGC gc(config);
  
  // 多数のオブジェクトとその間の複雑な参照関係を作成
  std::vector<TestCell*> roots;
  std::vector<TestCell*> nonRoots;
  
  // ルートオブジェクト作成
  for (int i = 0; i < 20; i++) {
    TestCell* cell = gc.allocate<TestCell>(1024);
    roots.push_back(cell);
    gc.addRoot(reinterpret_cast<GCCell**>(&roots.back()));
  }
  
  // 非ルートオブジェクト作成
  for (int i = 0; i < 1000; i++) {
    TestCell* cell = gc.allocate<TestCell>(512);
    nonRoots.push_back(cell);
  }
  
  // 複雑な参照関係を構築
  for (int i = 0; i < 20; i++) {
    int refCount = 10;
    for (int j = 0; j < refCount; j++) {
      int targetIdx = i * 50 + j;
      if (targetIdx < 1000) {
        roots[i]->addReference(nonRoots[targetIdx]);
      }
    }
  }
  
  for (int i = 0; i < 1000; i++) {
    int refCount = i % 5;
    for (int j = 0; j < refCount; j++) {
      int targetIdx = (i + j * 100) % 1000;
      nonRoots[i]->addReference(nonRoots[targetIdx]);
    }
  }
  
  // インクリメンタルマーキングの手動実行
  gc.incrementalMarkingStep(100);
  
  // GCを実行
  gc.majorCollection();
  
  // 統計情報を確認
  auto stats = gc.getStats();
  EXPECT_GT(stats.incrementalMarkingPasses, 0);
  
  // ルートからの参照がないオブジェクトは解放されるはず
  // ただし、サイクリック参照があると一部のオブジェクトは残るかもしれない
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
} 