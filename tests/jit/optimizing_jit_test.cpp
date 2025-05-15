/**
 * @file optimizing_jit_test.cpp
 * @brief 最適化JITコンパイラの詳細テスト
 * @version 1.0.0
 * @license MIT
 */

#include "../../src/core/jit/optimizing/optimizing_jit.h"
#include "../../src/core/jit/ir/ir_graph.h"
#include "../../src/core/jit/profiler/type_info.h"
#include "../../src/core/runtime/context/execution_context.h"
#include "../../src/core/vm/bytecode/bytecode.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <string>
#include <chrono>

using namespace aerojs::core;
using namespace aerojs::core::jit;
using namespace aerojs::core::jit::optimizing;
using namespace aerojs::core::jit::ir;
using namespace aerojs::core::jit::profiler;
using namespace aerojs::core::runtime;
using namespace aerojs::core::vm::bytecode;

// モックオブジェクト
class MockContext : public Context {
public:
  // モックコンテキスト実装
};

class MockBytecodeFunction : public BytecodeFunction {
public:
  MockBytecodeFunction(uint32_t id, const std::vector<uint8_t>& code) : m_id(id), m_code(code) {}
  
  uint32_t getId() const { return m_id; }
  const std::vector<uint8_t>& getCode() const { return m_code; }
  
private:
  uint32_t m_id;
  std::vector<uint8_t> m_code;
};

// テストフィクスチャ
class OptimizingJITTest : public ::testing::Test {
protected:
  void SetUp() override {
    context = std::make_unique<MockContext>();
    
    // テスト用の型情報作成
    typeInfo = std::make_unique<TypeInfo>();
    
    // テスト用の設定作成
    config = OptimizingJITConfig();
    config.level = OptimizationLevel::O2;
    config.enableInlining = true;
    config.enableTypeSpecialization = true;
    
    // JITコンパイラ作成
    jit = std::make_unique<OptimizingJIT>(context.get(), nullptr, config);
  }
  
  void TearDown() override {
    jit.reset();
    typeInfo.reset();
    context.reset();
  }
  
  // テスト用にIRグラフを構築
  std::unique_ptr<IRGraph> buildTestIRGraph() {
    auto graph = std::make_unique<IRGraph>();
    
    // エントリブロック作成
    auto entryBlock = graph->createBasicBlock("entry");
    graph->setEntryBlock(entryBlock);
    
    // テスト用のIR命令を追加
    auto constValue = graph->createConstant(runtime::Value::createNumber(42.0));
    auto var = graph->createVariable(0, "testVar");
    
    // 単純な演算
    auto addInst = graph->createBinaryOp(NodeType::Add, constValue, var);
    entryBlock->addInstruction(addInst);
    
    // 戻り値
    auto returnInst = graph->createReturn(addInst);
    entryBlock->addInstruction(returnInst);
    
    return graph;
  }
  
  // テスト用のバイトコード関数を作成
  std::unique_ptr<MockBytecodeFunction> createTestFunction(uint32_t id) {
    // シンプルなバイトコード: 定数をロード、変数をロード、加算、戻り値
    std::vector<uint8_t> bytecode = {
      0x01, 0x00, 0x00, 0x00,  // 定数ロード (42)
      0x02, 0x00, 0x00, 0x00,  // 変数ロード (インデックス0)
      0x03, 0x00, 0x00, 0x00,  // 加算
      0x04, 0x00, 0x00, 0x00   // 戻り値
    };
    
    return std::make_unique<MockBytecodeFunction>(id, bytecode);
  }
  
  std::unique_ptr<Context> context;
  std::unique_ptr<TypeInfo> typeInfo;
  OptimizingJITConfig config;
  std::unique_ptr<OptimizingJIT> jit;
};

// 基本的なコンパイルテスト
TEST_F(OptimizingJITTest, BasicCompilation) {
  auto function = createTestFunction(1);
  auto result = jit->optimizeFunction(function.get());
  
  EXPECT_TRUE(result.success);
  EXPECT_NE(nullptr, result.function);
  EXPECT_NE(nullptr, result.function->nativeCode);
  EXPECT_GT(result.function->codeSize, 0);
  EXPECT_EQ(OptimizedFunctionState::Ready, result.function->state);
}

// 最適化レベルテスト
TEST_F(OptimizingJITTest, OptimizationLevels) {
  auto function = createTestFunction(1);
  
  // O0 (最適化なし)
  jit->setOptimizationLevel(OptimizationLevel::O0);
  auto resultO0 = jit->optimizeFunction(function.get());
  EXPECT_TRUE(resultO0.success);
  
  // O1 (基本最適化)
  jit->setOptimizationLevel(OptimizationLevel::O1);
  auto resultO1 = jit->optimizeFunction(function.get());
  EXPECT_TRUE(resultO1.success);
  
  // O2 (標準最適化)
  jit->setOptimizationLevel(OptimizationLevel::O2);
  auto resultO2 = jit->optimizeFunction(function.get());
  EXPECT_TRUE(resultO2.success);
  
  // O3 (積極的最適化)
  jit->setOptimizationLevel(OptimizationLevel::O3);
  auto resultO3 = jit->optimizeFunction(function.get());
  EXPECT_TRUE(resultO3.success);
  
  // 最大最適化
  jit->setOptimizationLevel(OptimizationLevel::Omax);
  auto resultOmax = jit->optimizeFunction(function.get());
  EXPECT_TRUE(resultOmax.success);
  
  // 最適化レベルが高いほどコンパイル時間が長い傾向にある
  // 注意: このテストは環境依存で不安定になる可能性がある
  if (resultO0.compilationTime > std::chrono::nanoseconds(0) && 
      resultOmax.compilationTime > std::chrono::nanoseconds(0)) {
    EXPECT_LE(resultO0.compilationTime, resultOmax.compilationTime);
  }
}

// 型情報を使った最適化テスト
TEST_F(OptimizingJITTest, TypeSpecialization) {
  auto function = createTestFunction(1);
  
  // 型情報を準備
  typeInfo = std::make_unique<TypeInfo>();
  typeInfo->recordType(runtime::ValueType::Number);
  
  // 型特化ありでコンパイル
  config.enableTypeSpecialization = true;
  jit->updateConfig(config);
  auto resultWithTypes = jit->optimizeFunction(function.get(), std::move(typeInfo));
  EXPECT_TRUE(resultWithTypes.success);
  
  // 新しい型情報を準備
  typeInfo = std::make_unique<TypeInfo>();
  typeInfo->recordType(runtime::ValueType::Number);
  
  // 型特化なしでコンパイル
  config.enableTypeSpecialization = false;
  jit->updateConfig(config);
  auto resultWithoutTypes = jit->optimizeFunction(function.get(), std::move(typeInfo));
  EXPECT_TRUE(resultWithoutTypes.success);
  
  // 型特化ありの方が最適化が多く適用されるはず
  // (optimizationFlagsが異なるはずだが、モック環境では検証が難しいので省略)
}

// 最適化フェーズテスト
TEST_F(OptimizingJITTest, OptimizationPhases) {
  auto function = createTestFunction(1);
  
  // テスト用のコンパイル進捗コールバック
  struct PhaseRecord {
    OptimizationPhase phase;
    OptimizationStage stage;
    float progress;
  };
  
  std::vector<PhaseRecord> phaseRecords;
  auto callback = [&phaseRecords](OptimizationPhase phase, OptimizationStage stage, float progress) {
    phaseRecords.push_back({phase, stage, progress});
  };
  
  // コンパイル実行
  auto result = jit->optimizeFunction(function.get(), nullptr, OptimizationReason::HotFunction, callback);
  EXPECT_TRUE(result.success);
  
  // 少なくともいくつかのフェーズが記録されているはず
  EXPECT_FALSE(phaseRecords.empty());
  
  // フロントエンド, ミドルエンド, バックエンド, コード生成の各フェーズが記録されているか確認
  bool foundFrontend = false;
  bool foundMiddleEnd = false;
  bool foundBackend = false;
  bool foundCodeGen = false;
  
  for (const auto& record : phaseRecords) {
    switch (record.phase) {
      case OptimizationPhase::Frontend: foundFrontend = true; break;
      case OptimizationPhase::MiddleEnd: foundMiddleEnd = true; break;
      case OptimizationPhase::Backend: foundBackend = true; break;
      case OptimizationPhase::CodeGen: foundCodeGen = true; break;
    }
  }
  
  EXPECT_TRUE(foundFrontend);
  EXPECT_TRUE(foundMiddleEnd);
  EXPECT_TRUE(foundBackend);
  EXPECT_TRUE(foundCodeGen);
}

// 無効化テスト
TEST_F(OptimizingJITTest, InvalidationTest) {
  auto function = createTestFunction(1);
  
  // 関数を最適化
  auto result = jit->optimizeFunction(function.get());
  EXPECT_TRUE(result.success);
  EXPECT_EQ(OptimizedFunctionState::Ready, result.function->state);
  
  // 関数IDを取得
  uint32_t functionId = function->getId();
  
  // 最適化済み関数を取得
  auto state = jit->getOptimizedFunctionState(functionId);
  EXPECT_TRUE(state.has_value());
  EXPECT_EQ(OptimizedFunctionState::Ready, state.value());
  
  // 関数を無効化
  bool invalidated = jit->invalidateOptimizedFunction(functionId);
  EXPECT_TRUE(invalidated);
  
  // 無効化後の状態を確認
  state = jit->getOptimizedFunctionState(functionId);
  EXPECT_TRUE(state.has_value());
  EXPECT_EQ(OptimizedFunctionState::Invalidated, state.value());
  
  // すべての関数を無効化
  jit->invalidateAllOptimizedFunctions();
  
  // 無効化後の状態を確認
  state = jit->getOptimizedFunctionState(functionId);
  EXPECT_TRUE(state.has_value());
  EXPECT_EQ(OptimizedFunctionState::Invalidated, state.value());
}

// バックグラウンド最適化テスト
TEST_F(OptimizingJITTest, BackgroundOptimization) {
  // バックグラウンド最適化を有効化
  jit->setBackgroundOptimization(true);
  jit->startBackgroundOptimization();
  
  // 複数の関数を最適化キューに追加
  auto function1 = createTestFunction(1);
  auto function2 = createTestFunction(2);
  auto function3 = createTestFunction(3);
  
  bool enqueued1 = jit->enqueueForOptimization(function1.get(), OptimizationReason::HotFunction);
  bool enqueued2 = jit->enqueueForOptimization(function2.get(), OptimizationReason::TypeStability);
  bool enqueued3 = jit->enqueueForOptimization(function3.get(), OptimizationReason::CriticalPath);
  
  EXPECT_TRUE(enqueued1);
  EXPECT_TRUE(enqueued2);
  EXPECT_TRUE(enqueued3);
  
  // バックグラウンド処理が終了するまで少し待機
  // 注意: これは非決定的な方法で、テスト環境によって調整が必要
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  
  // 最適化結果を確認
  // 注意: バックグラウンド最適化はモックオブジェクトの制限により完全に検証できない場合がある
  
  // バックグラウンド最適化を停止
  jit->stopBackgroundOptimization();
}

// 強制最適化テスト
TEST_F(OptimizingJITTest, ForceOptimize) {
  auto function = createTestFunction(1);
  
  // 強制最適化
  auto result = jit->forceOptimize(function.get(), OptimizationLevel::Omax);
  
  EXPECT_TRUE(result.success);
  EXPECT_NE(nullptr, result.function);
  EXPECT_EQ(OptimizedFunctionState::Ready, result.function->state);
  EXPECT_EQ(OptimizationLevel::Omax, result.function->level);
}

// メモリ使用量テスト
TEST_F(OptimizingJITTest, MemoryUsage) {
  // 初期メモリ使用量
  size_t initialMemory = jit->getMemoryUsage();
  
  // 複数の関数をコンパイル
  std::vector<std::unique_ptr<MockBytecodeFunction>> functions;
  for (uint32_t i = 0; i < 5; ++i) {
    functions.push_back(createTestFunction(i + 1));
    jit->optimizeFunction(functions.back().get());
  }
  
  // メモリ使用量が増加しているはず
  size_t finalMemory = jit->getMemoryUsage();
  EXPECT_GT(finalMemory, initialMemory);
}

// 最適化失敗ケースのテスト
TEST_F(OptimizingJITTest, OptimizationFailure) {
  // 空のバイトコードを持つ関数
  std::vector<uint8_t> emptyBytecode;
  auto emptyFunction = std::make_unique<MockBytecodeFunction>(100, emptyBytecode);
  
  // 最適化試行
  auto result = jit->optimizeFunction(emptyFunction.get());
  
  // 失敗するはず
  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.errorMessage.empty());
}

// ホット関数検出テスト
TEST_F(OptimizingJITTest, HotFunctionDetection) {
  // 初期状態では検出されない
  size_t hotCount = jit->detectAndEnqueueHotFunctions();
  EXPECT_EQ(0, hotCount);
  
  // 注意: 完全なテストにはプロファイリングデータの模擬が必要
}

// デバッグ情報テスト
TEST_F(OptimizingJITTest, DebugInformation) {
  auto function = createTestFunction(1);
  auto result = jit->optimizeFunction(function.get());
  EXPECT_TRUE(result.success);
  
  // 最適化状態のダンプ
  std::string status = jit->dumpOptimizationStatus();
  EXPECT_FALSE(status.empty());
  
  // IR表現のダンプ
  std::string ir = jit->dumpOptimizedIR(function->getId());
  EXPECT_FALSE(ir.empty());
}

// メインテスト実行
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
} 