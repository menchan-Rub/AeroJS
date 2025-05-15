/**
 * @file trace_recorder_test.cpp
 * @brief トレース記録機能のテスト
 * @version 1.0.0
 * @license MIT
 */

#include "../../../src/core/jit/metatracing/trace_recorder.h"
#include "../../../src/core/jit/metatracing/tracing_jit.h"
#include "../../../src/core/runtime/context/execution_context.h"
#include "../../../src/core/vm/bytecode/bytecode.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace aerojs::core;
using namespace aerojs::core::jit;
using namespace aerojs::core::jit::metatracing;
using namespace aerojs::core::runtime;
using namespace aerojs::core::vm::bytecode;

// モックオブジェクト
class MockContext : public Context {
public:
  // モックコンテキスト実装
};

class MockBytecodeFunction : public BytecodeFunction {
public:
  MockBytecodeFunction(uint32_t id, const std::vector<uint8_t>& code) 
    : m_id(id), m_code(code) {}
  
  uint32_t getId() const { return m_id; }
  const std::vector<uint8_t>& getCode() const { return m_code; }
  
private:
  uint32_t m_id;
  std::vector<uint8_t> m_code;
};

// テストフィクスチャ
class TraceRecorderTest : public ::testing::Test {
protected:
  void SetUp() override {
    context = std::make_unique<MockContext>();
    
    // トレース記録設定
    config = TraceRecorderConfig();
    config.maxTraceLength = 1000;
    config.maxLoopIterations = 10;
    
    // トレースレコーダ作成
    recorder = std::make_unique<TraceRecorder>(context.get(), config);
  }
  
  void TearDown() override {
    recorder.reset();
    context.reset();
  }
  
  // テスト用のバイトコード関数を作成
  std::unique_ptr<MockBytecodeFunction> createLoopFunction() {
    // シンプルなループを含むバイトコード
    // ループ: 0から9まで変数をインクリメント
    std::vector<uint8_t> bytecode = {
      0x01, 0x00, 0x00, 0x00,  // 定数ロード (0) -> 変数0
      0x02, 0x00, 0x00, 0x00,  // 変数0をスタックにプッシュ
      0x03, 0x00, 0x00, 0x00,  // 定数ロード (10) -> 比較用
      0x04, 0x00, 0x00, 0x00,  // 比較 (変数0 < 10)
      0x05, 0x1C, 0x00, 0x00,  // 条件分岐 (falseなら終了へジャンプ)
      0x06, 0x00, 0x00, 0x00,  // 変数0をスタックにプッシュ
      0x07, 0x00, 0x00, 0x00,  // 定数ロード (1)
      0x08, 0x00, 0x00, 0x00,  // 加算 (変数0 + 1)
      0x09, 0x00, 0x00, 0x00,  // 変数0に格納
      0x0A, 0x04, 0x00, 0x00,  // ジャンプ (ループ先頭へ)
      0x0B, 0x00, 0x00, 0x00,  // 変数0をスタックにプッシュ (戻り値)
      0x0C, 0x00, 0x00, 0x00   // リターン
    };
    
    return std::make_unique<MockBytecodeFunction>(1, bytecode);
  }
  
  std::unique_ptr<Context> context;
  TraceRecorderConfig config;
  std::unique_ptr<TraceRecorder> recorder;
};

// 基本的なトレース記録テスト
TEST_F(TraceRecorderTest, BasicRecording) {
  auto function = createLoopFunction();
  
  // トレース記録開始
  bool started = recorder->startRecording(function.get(), 4, TraceReason::HotLoop);
  EXPECT_TRUE(started);
  EXPECT_TRUE(recorder->isRecording());
  
  // 基本的な命令記録
  for (uint32_t i = 0; i < 5; ++i) {
    // ループを1回分記録
    recorder->recordInstruction(0x01, {});  // 定数ロード
    recorder->recordInstruction(0x02, {});  // 変数ロード
    recorder->recordInstruction(0x03, {});  // 定数ロード (比較用)
    recorder->recordInstruction(0x04, {});  // 比較
    recorder->recordInstruction(0x05, {});  // 条件分岐
    recorder->recordInstruction(0x06, {});  // 変数プッシュ
    recorder->recordInstruction(0x07, {});  // 定数ロード
    recorder->recordInstruction(0x08, {});  // 加算
    recorder->recordInstruction(0x09, {});  // 変数格納
    recorder->recordInstruction(0x0A, {});  // ジャンプ
  }
  
  // トレース記録終了
  auto trace = recorder->stopRecording(TraceExitReason::Complete);
  EXPECT_NE(nullptr, trace);
  EXPECT_FALSE(recorder->isRecording());
  
  // 記録されたトレースの検証
  EXPECT_EQ(TraceType::Loop, trace->getType());
  EXPECT_EQ(function->getId(), trace->getFunctionId());
  EXPECT_EQ(4, trace->getStartOffset());
  EXPECT_EQ(50, trace->getInstructions().size());  // 5反復 x 10命令
  EXPECT_EQ(5, trace->getIterationCount());
  EXPECT_EQ(TraceExitReason::Complete, trace->getExitReason());
}

// ループ検出テスト
TEST_F(TraceRecorderTest, LoopDetection) {
  auto function = createLoopFunction();
  
  // トレース記録開始
  recorder->startRecording(function.get(), 4, TraceReason::HotLoop);
  
  // ループ先頭記録
  TracePoint loopHead(function->getId(), 4);
  
  // 命令記録 (1回目)
  for (int i = 0; i < 10; ++i) {
    recorder->recordInstruction(0x01 + i, {});
  }
  
  // ループ先頭に戻る
  bool loopDetected = recorder->recordBackEdge(loopHead);
  EXPECT_TRUE(loopDetected);
  
  // 2回目のループ
  for (int i = 0; i < 10; ++i) {
    recorder->recordInstruction(0x01 + i, {});
  }
  
  // トレース記録終了
  auto trace = recorder->stopRecording(TraceExitReason::Complete);
  EXPECT_NE(nullptr, trace);
  
  // 記録されたトレースの検証
  EXPECT_EQ(TraceType::Loop, trace->getType());
  EXPECT_EQ(20, trace->getInstructions().size());  // 2反復 x 10命令
  EXPECT_EQ(2, trace->getIterationCount());
}

// 条件分岐記録テスト
TEST_F(TraceRecorderTest, BranchRecording) {
  auto function = createLoopFunction();
  
  // トレース記録開始
  recorder->startRecording(function.get(), 0, TraceReason::HotFunction);
  
  // 条件分岐の記録
  recorder->recordInstruction(0x01, {});  // 前の命令
  recorder->recordBranch(true, 12, 20);   // 条件分岐: 条件true、trueターゲット=12, falseターゲット=20
  recorder->recordInstruction(0x02, {});  // 次の命令
  
  // トレース記録終了
  auto trace = recorder->stopRecording(TraceExitReason::Complete);
  EXPECT_NE(nullptr, trace);
  
  // 記録された分岐の検証
  const auto& instructions = trace->getInstructions();
  EXPECT_EQ(3, instructions.size());
  
  // 中央の命令が分岐命令か確認
  EXPECT_EQ(TraceInstructionType::Branch, instructions[1].type);
  EXPECT_TRUE(instructions[1].branchTaken);
  EXPECT_EQ(12u, instructions[1].branchTargetTrue);
  EXPECT_EQ(20u, instructions[1].branchTargetFalse);
}

// サイド出口記録テスト
TEST_F(TraceRecorderTest, SideExitRecording) {
  auto function = createLoopFunction();
  
  // トレース記録開始
  recorder->startRecording(function.get(), 0, TraceReason::HotFunction);
  
  // 命令記録
  recorder->recordInstruction(0x01, {});
  recorder->recordInstruction(0x02, {});
  
  // ガード条件の記録 (型チェックなど)
  GuardCondition guard;
  guard.type = GuardType::TypeCheck;
  guard.expectedType = ValueType::Number;
  guard.valueIndex = 0;
  
  recorder->recordGuard(guard, 16);  // 16はガード失敗時の出口オフセット
  
  // さらに命令記録
  recorder->recordInstruction(0x03, {});
  
  // トレース記録終了
  auto trace = recorder->stopRecording(TraceExitReason::Complete);
  EXPECT_NE(nullptr, trace);
  
  // 記録されたガードの検証
  const auto& guards = trace->getGuards();
  EXPECT_EQ(1, guards.size());
  EXPECT_EQ(GuardType::TypeCheck, guards[0].condition.type);
  EXPECT_EQ(ValueType::Number, guards[0].condition.expectedType);
  EXPECT_EQ(0, guards[0].condition.valueIndex);
  EXPECT_EQ(16u, guards[0].exitOffset);
  
  // サイド出口の検証
  const auto& sideExits = trace->getSideExits();
  EXPECT_EQ(1, sideExits.size());
  EXPECT_EQ(16u, sideExits[0].bytecodeOffset);
}

// 最大トレース長テスト
TEST_F(TraceRecorderTest, MaxTraceLength) {
  auto function = createLoopFunction();
  
  // 最大トレース長を小さく設定
  config.maxTraceLength = 20;
  recorder = std::make_unique<TraceRecorder>(context.get(), config);
  
  // トレース記録開始
  recorder->startRecording(function.get(), 0, TraceReason::HotFunction);
  
  // 最大長を超える命令記録
  for (int i = 0; i < 25; ++i) {
    recorder->recordInstruction(0x01, {});
  }
  
  // 自動的に記録が終了しているはず
  EXPECT_FALSE(recorder->isRecording());
  
  // トレースを取得
  auto trace = recorder->getTrace();
  EXPECT_NE(nullptr, trace);
  EXPECT_EQ(TraceExitReason::TooLong, trace->getExitReason());
  EXPECT_LE(trace->getInstructions().size(), 20u);
}

// 最大ループ反復回数テスト
TEST_F(TraceRecorderTest, MaxLoopIterations) {
  auto function = createLoopFunction();
  
  // 最大ループ反復回数を小さく設定
  config.maxLoopIterations = 3;
  recorder = std::make_unique<TraceRecorder>(context.get(), config);
  
  // トレース記録開始
  recorder->startRecording(function.get(), 4, TraceReason::HotLoop);
  
  // ループ先頭記録
  TracePoint loopHead(function->getId(), 4);
  
  // 最大反復回数を超えるループ記録
  for (int iter = 0; iter < 5; ++iter) {
    // 各反復で10命令
    for (int i = 0; i < 10; ++i) {
      recorder->recordInstruction(0x01 + i, {});
    }
    
    // ループ先頭に戻る
    recorder->recordBackEdge(loopHead);
    
    // 4回目以降は記録が終了しているはず
    if (iter >= 3) {
      EXPECT_FALSE(recorder->isRecording());
    } else {
      EXPECT_TRUE(recorder->isRecording());
    }
  }
  
  // トレースを取得
  auto trace = recorder->getTrace();
  EXPECT_NE(nullptr, trace);
  EXPECT_EQ(TraceExitReason::TooManyIterations, trace->getExitReason());
  EXPECT_EQ(3, trace->getIterationCount());
  EXPECT_EQ(30, trace->getInstructions().size());  // 3反復 x 10命令
}

// 型プロファイル記録テスト
TEST_F(TraceRecorderTest, TypeProfiling) {
  auto function = createLoopFunction();
  
  // 型プロファイリングを有効化
  config.enableTypeProfiling = true;
  recorder = std::make_unique<TraceRecorder>(context.get(), config);
  
  // トレース記録開始
  recorder->startRecording(function.get(), 0, TraceReason::HotFunction);
  
  // 値の型を記録
  recorder->recordTypeInfo(0, runtime::Value::createNumber(42.0));
  recorder->recordTypeInfo(1, runtime::Value::createBoolean(true));
  recorder->recordTypeInfo(2, runtime::Value::createString(nullptr)); // スタブ: 実際はStringオブジェクトが必要
  
  // 命令記録
  recorder->recordInstruction(0x01, {});
  
  // トレース記録終了
  auto trace = recorder->stopRecording(TraceExitReason::Complete);
  EXPECT_NE(nullptr, trace);
  
  // 記録された型情報の検証
  const auto& typeProfile = trace->getTypeProfile();
  EXPECT_EQ(3, typeProfile.size());
  EXPECT_EQ(ValueType::Number, typeProfile.at(0).getMostCommonType());
  EXPECT_EQ(ValueType::Boolean, typeProfile.at(1).getMostCommonType());
  EXPECT_EQ(ValueType::String, typeProfile.at(2).getMostCommonType());
}

// メインテスト実行
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
} 