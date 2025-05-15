/**
 * @file ir_graph_test.cpp
 * @brief IRグラフシステムのテスト
 * @version 1.0.0
 * @license MIT
 */

#include "../../../src/core/jit/ir/ir_graph.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

using namespace aerojs::core;
using namespace aerojs::core::jit::ir;

// テストフィクスチャ
class IRGraphTest : public ::testing::Test {
protected:
  void SetUp() override {
    // 各テスト前にグラフを新規作成
    graph = std::make_unique<IRGraph>();
  }
  
  void TearDown() override {
    graph.reset();
  }
  
  // テスト用に単純なIRグラフを構築（x + 5 を計算して返す）
  void buildSimpleGraph() {
    // エントリブロック作成
    auto entry = graph->createBasicBlock("entry");
    graph->setEntryBlock(entry);
    
    // パラメータ x
    auto param = graph->createParameter(0, "x", IRType(IRType::Kind::Int32));
    graph->addParameter(param);
    
    // 定数 5
    auto constant = graph->createConstant(runtime::Value::createNumber(5.0));
    
    // 加算命令
    auto add = graph->createBinaryOp(NodeType::Add, param, constant);
    entry->addInstruction(add);
    
    // 戻り値命令
    auto ret = graph->createReturn(add);
    entry->addInstruction(ret);
  }
  
  // テスト用に条件分岐を含むIRグラフを構築（if (x > 10) return x; else return x * 2;）
  void buildBranchGraph() {
    // ブロック作成
    auto entry = graph->createBasicBlock("entry");
    auto thenBlock = graph->createBasicBlock("then");
    auto elseBlock = graph->createBasicBlock("else");
    auto exit = graph->createBasicBlock("exit");
    
    graph->setEntryBlock(entry);
    
    // パラメータ x
    auto param = graph->createParameter(0, "x", IRType(IRType::Kind::Int32));
    graph->addParameter(param);
    
    // 定数 10
    auto constant10 = graph->createConstant(runtime::Value::createNumber(10.0));
    
    // 比較命令: x > 10
    auto compare = graph->createBinaryOp(NodeType::GreaterThan, param, constant10);
    entry->addInstruction(compare);
    
    // 条件分岐
    auto branch = graph->createBranch(compare, thenBlock, elseBlock);
    entry->addInstruction(branch);
    
    // thenブロック: return x;
    auto returnX = graph->createReturn(param);
    thenBlock->addInstruction(returnX);
    
    // elseブロック: return x * 2;
    auto constant2 = graph->createConstant(runtime::Value::createNumber(2.0));
    auto mul = graph->createBinaryOp(NodeType::Mul, param, constant2);
    elseBlock->addInstruction(mul);
    auto returnMul = graph->createReturn(mul);
    elseBlock->addInstruction(returnMul);
  }
  
  // テスト用にループを含むIRグラフを構築（sum = 0; for(i=0; i<n; i++) sum += i; return sum;）
  void buildLoopGraph() {
    // ブロック作成
    auto entry = graph->createBasicBlock("entry");
    auto loopHeader = graph->createBasicBlock("loop_header");
    auto loopBody = graph->createBasicBlock("loop_body");
    auto exit = graph->createBasicBlock("exit");
    
    graph->setEntryBlock(entry);
    
    // パラメータ n
    auto paramN = graph->createParameter(0, "n", IRType(IRType::Kind::Int32));
    graph->addParameter(paramN);
    
    // 変数初期化: i = 0, sum = 0
    auto zero = graph->createConstant(runtime::Value::createNumber(0.0));
    auto varI = graph->createVariable(0, "i", IRType(IRType::Kind::Int32));
    auto varSum = graph->createVariable(1, "sum", IRType(IRType::Kind::Int32));
    
    // entry -> loopHeader
    auto jump = graph->createJump(loopHeader);
    entry->addInstruction(jump);
    
    // loopHeader: i < n ?
    auto compare = graph->createBinaryOp(NodeType::LessThan, varI, paramN);
    loopHeader->addInstruction(compare);
    auto branch = graph->createBranch(compare, loopBody, exit);
    loopHeader->addInstruction(branch);
    
    // loopBody: sum += i; i++;
    auto loadSum = graph->createBinaryOp(NodeType::Add, varSum, varI);
    loopBody->addInstruction(loadSum);
    
    auto one = graph->createConstant(runtime::Value::createNumber(1.0));
    auto incI = graph->createBinaryOp(NodeType::Add, varI, one);
    loopBody->addInstruction(incI);
    
    // ループ先頭に戻る
    auto backJump = graph->createJump(loopHeader);
    loopBody->addInstruction(backJump);
    
    // exit: return sum
    auto returnSum = graph->createReturn(varSum);
    exit->addInstruction(returnSum);
  }
  
  std::unique_ptr<IRGraph> graph;
};

// 基本的なグラフ構築テスト
TEST_F(IRGraphTest, BasicGraphConstruction) {
  buildSimpleGraph();
  
  // グラフの検証
  EXPECT_NE(nullptr, graph->getEntryBlock());
  EXPECT_EQ(1, graph->getParameterCount());
  EXPECT_EQ(1, graph->getBasicBlockCount());
  
  // エントリブロックの検証
  auto entry = graph->getEntryBlock();
  EXPECT_EQ("entry", entry->getLabel());
  EXPECT_EQ(2, entry->getInstructionCount());
  
  // 命令の検証
  const auto& instructions = entry->getInstructions();
  EXPECT_EQ(NodeType::Add, instructions[0]->getType());
  EXPECT_EQ(NodeType::Return, instructions[1]->getType());
  
  // バイナリ命令の検証
  auto binaryOp = dynamic_cast<BinaryInstruction*>(instructions[0]);
  EXPECT_NE(nullptr, binaryOp);
  EXPECT_EQ(NodeType::Parameter, binaryOp->getLeft()->getType());
  EXPECT_EQ(NodeType::Constant, binaryOp->getRight()->getType());
  
  // リターン命令の検証
  auto returnInst = dynamic_cast<ReturnInstruction*>(instructions[1]);
  EXPECT_NE(nullptr, returnInst);
  EXPECT_EQ(binaryOp, returnInst->getReturnValue());
}

// ノード作成テスト
TEST_F(IRGraphTest, NodeCreation) {
  // 定数作成
  auto constInt = graph->createConstant(runtime::Value::createNumber(42.0));
  auto constBool = graph->createConstant(runtime::Value::createBoolean(true));
  
  EXPECT_TRUE(constInt->isConstant());
  EXPECT_TRUE(constBool->isConstant());
  EXPECT_EQ(42.0, constInt->getValue().toNumber());
  EXPECT_TRUE(constBool->getValue().toBoolean());
  
  // 変数作成
  auto var = graph->createVariable(0, "testVar", IRType(IRType::Kind::Int32));
  EXPECT_TRUE(var->isVariable());
  EXPECT_EQ(0u, var->getIndex());
  EXPECT_EQ("testVar", var->getName());
  EXPECT_EQ(IRType::Kind::Int32, var->getValueType().kind);
  
  // パラメータ作成
  auto param = graph->createParameter(1, "testParam", IRType(IRType::Kind::Float64));
  EXPECT_EQ(NodeType::Parameter, param->getType());
  EXPECT_EQ(1u, param->getIndex());
  EXPECT_EQ("testParam", param->getName());
  EXPECT_EQ(IRType::Kind::Float64, param->getValueType().kind);
  
  // 基本ブロック作成
  auto block = graph->createBasicBlock("testBlock");
  EXPECT_TRUE(block->isBasicBlock());
  EXPECT_EQ("testBlock", block->getLabel());
  EXPECT_EQ(0, block->getInstructionCount());
}

// 条件分岐グラフテスト
TEST_F(IRGraphTest, BranchGraph) {
  buildBranchGraph();
  
  // グラフの検証
  EXPECT_NE(nullptr, graph->getEntryBlock());
  EXPECT_EQ(1, graph->getParameterCount());
  EXPECT_GE(graph->getBasicBlockCount(), 3); // 少なくともentry、then、elseブロック
  
  // エントリブロックの検証
  auto entry = graph->getEntryBlock();
  EXPECT_EQ(2, entry->getInstructionCount());
  EXPECT_EQ(NodeType::Branch, entry->getInstructions().back()->getType());
  
  // 分岐命令の検証
  auto branch = dynamic_cast<BranchInstruction*>(entry->getInstructions().back());
  EXPECT_NE(nullptr, branch);
  EXPECT_NE(nullptr, branch->getTrueBlock());
  EXPECT_NE(nullptr, branch->getFalseBlock());
  EXPECT_NE(branch->getTrueBlock(), branch->getFalseBlock());
  
  // trueブロックの検証
  auto thenBlock = branch->getTrueBlock();
  EXPECT_EQ(1, thenBlock->getInstructionCount());
  EXPECT_EQ(NodeType::Return, thenBlock->getInstructions()[0]->getType());
  
  // falseブロックの検証
  auto elseBlock = branch->getFalseBlock();
  EXPECT_EQ(2, elseBlock->getInstructionCount());
  EXPECT_EQ(NodeType::Mul, elseBlock->getInstructions()[0]->getType());
  EXPECT_EQ(NodeType::Return, elseBlock->getInstructions()[1]->getType());
}

// ループグラフテスト
TEST_F(IRGraphTest, LoopGraph) {
  buildLoopGraph();
  
  // グラフの検証
  EXPECT_NE(nullptr, graph->getEntryBlock());
  EXPECT_EQ(1, graph->getParameterCount());
  EXPECT_GE(graph->getBasicBlockCount(), 4); // entry, loopHeader, loopBody, exit
  
  // 制御フローの検証
  auto entry = graph->getEntryBlock();
  EXPECT_EQ(NodeType::Jump, entry->getTerminator()->getType());
  
  auto jumpInst = dynamic_cast<JumpInstruction*>(entry->getTerminator());
  EXPECT_NE(nullptr, jumpInst);
  
  auto loopHeader = jumpInst->getTargetBlock();
  EXPECT_EQ(NodeType::Branch, loopHeader->getTerminator()->getType());
  
  auto branch = dynamic_cast<BranchInstruction*>(loopHeader->getTerminator());
  EXPECT_NE(nullptr, branch);
  
  auto loopBody = branch->getTrueBlock();
  EXPECT_GE(loopBody->getInstructionCount(), 2);
}

// PHIノードテスト
TEST_F(IRGraphTest, PhiNodes) {
  // 2つの入力ブロックを持つ合流点でのPHIノードテスト
  auto entry = graph->createBasicBlock("entry");
  auto left = graph->createBasicBlock("left");
  auto right = graph->createBasicBlock("right");
  auto merge = graph->createBasicBlock("merge");
  
  graph->setEntryBlock(entry);
  
  // 条件変数
  auto condition = graph->createParameter(0, "condition");
  graph->addParameter(condition);
  
  // 分岐命令
  auto branch = graph->createBranch(condition, left, right);
  entry->addInstruction(branch);
  
  // left側: x = 1
  auto const1 = graph->createConstant(runtime::Value::createNumber(1.0));
  auto jumpFromLeft = graph->createJump(merge);
  left->addInstruction(jumpFromLeft);
  
  // right側: x = 2
  auto const2 = graph->createConstant(runtime::Value::createNumber(2.0));
  auto jumpFromRight = graph->createJump(merge);
  right->addInstruction(jumpFromRight);
  
  // PHIノード作成
  auto phi = graph->createPhi(IRType(IRType::Kind::Int32));
  phi->addIncoming(const1, left);
  phi->addIncoming(const2, right);
  merge->addInstruction(phi);
  
  // リターン命令
  auto ret = graph->createReturn(phi);
  merge->addInstruction(ret);
  
  // PHIノードの検証
  EXPECT_EQ(2, phi->getIncomingCount());
  EXPECT_EQ(const1, phi->getIncomingValue(0));
  EXPECT_EQ(left, phi->getIncomingBlock(0));
  EXPECT_EQ(const2, phi->getIncomingValue(1));
  EXPECT_EQ(right, phi->getIncomingBlock(1));
  
  // 特定ブロックに対する値の取得
  EXPECT_EQ(const1, phi->getIncomingValueForBlock(left));
  EXPECT_EQ(const2, phi->getIncomingValueForBlock(right));
}

// グラフクローンテスト
TEST_F(IRGraphTest, GraphCloning) {
  buildSimpleGraph();
  
  // グラフをクローン
  auto cloned = graph->clone();
  
  // クローンの検証
  EXPECT_NE(nullptr, cloned->getEntryBlock());
  EXPECT_EQ(graph->getParameterCount(), cloned->getParameterCount());
  EXPECT_EQ(graph->getBasicBlockCount(), cloned->getBasicBlockCount());
  
  // 元のグラフと別のインスタンスであることを確認
  EXPECT_NE(graph->getEntryBlock(), cloned->getEntryBlock());
  
  // クローンのエントリブロックの検証
  auto clonedEntry = cloned->getEntryBlock();
  EXPECT_EQ(2, clonedEntry->getInstructionCount());
  EXPECT_EQ(NodeType::Add, clonedEntry->getInstructions()[0]->getType());
  EXPECT_EQ(NodeType::Return, clonedEntry->getInstructions()[1]->getType());
}

// 命令削除テスト
TEST_F(IRGraphTest, InstructionRemoval) {
  // シンプルなグラフ: a = x + 5; b = a * 2; return b;
  auto entry = graph->createBasicBlock("entry");
  graph->setEntryBlock(entry);
  
  auto param = graph->createParameter(0, "x");
  graph->addParameter(param);
  
  auto const5 = graph->createConstant(runtime::Value::createNumber(5.0));
  auto add = graph->createBinaryOp(NodeType::Add, param, const5);
  entry->addInstruction(add);
  
  auto const2 = graph->createConstant(runtime::Value::createNumber(2.0));
  auto mul = graph->createBinaryOp(NodeType::Mul, add, const2);
  entry->addInstruction(mul);
  
  auto ret = graph->createReturn(mul);
  entry->addInstruction(ret);
  
  // 初期状態の検証
  EXPECT_EQ(3, entry->getInstructionCount());
  
  // 乗算命令を削除
  mul->remove();
  
  // 削除後の検証
  EXPECT_EQ(2, entry->getInstructionCount());
  EXPECT_EQ(NodeType::Add, entry->getInstructions()[0]->getType());
  EXPECT_EQ(NodeType::Return, entry->getInstructions()[1]->getType());
  
  // リターン命令のオペランドも更新されているはず
  auto retAfterRemove = dynamic_cast<ReturnInstruction*>(entry->getInstructions()[1]);
  EXPECT_NE(nullptr, retAfterRemove);
  EXPECT_NE(mul, retAfterRemove->getReturnValue());
}

// 支配関係計算テスト
TEST_F(IRGraphTest, DominatorCalculation) {
  // 次のようなCFGを構築:
  //    A
  //   / \
  //  B   C
  //   \ /
  //    D
  //    |
  //    E
  
  auto A = graph->createBasicBlock("A");
  auto B = graph->createBasicBlock("B");
  auto C = graph->createBasicBlock("C");
  auto D = graph->createBasicBlock("D");
  auto E = graph->createBasicBlock("E");
  
  graph->setEntryBlock(A);
  
  // A -> B, C
  auto condition = graph->createParameter(0, "cond");
  graph->addParameter(condition);
  auto branchA = graph->createBranch(condition, B, C);
  A->addInstruction(branchA);
  
  // B -> D
  auto jumpB = graph->createJump(D);
  B->addInstruction(jumpB);
  
  // C -> D
  auto jumpC = graph->createJump(D);
  C->addInstruction(jumpC);
  
  // D -> E
  auto jumpD = graph->createJump(E);
  D->addInstruction(jumpD);
  
  // E -> リターン
  auto constVal = graph->createConstant(runtime::Value::createNumber(42.0));
  auto retE = graph->createReturn(constVal);
  E->addInstruction(retE);
  
  // 支配関係計算
  graph->computeDominators();
  
  // 支配関係の検証
  EXPECT_EQ(nullptr, A->getDominator());      // Aはエントリ
  EXPECT_EQ(A, B->getDominator());            // AはBを支配
  EXPECT_EQ(A, C->getDominator());            // AはCを支配
  EXPECT_EQ(A, D->getDominator());            // AはDを支配
  EXPECT_EQ(A, E->getDominator());            // AはEを支配
  
  // A直接支配する（immediate dominate）ブロック
  const auto& ADominated = A->getImmediateDominated();
  EXPECT_EQ(4, ADominated.size());
  EXPECT_TRUE(std::find(ADominated.begin(), ADominated.end(), B) != ADominated.end());
  EXPECT_TRUE(std::find(ADominated.begin(), ADominated.end(), C) != ADominated.end());
  EXPECT_TRUE(std::find(ADominated.begin(), ADominated.end(), D) != ADominated.end());
  EXPECT_TRUE(std::find(ADominated.begin(), ADominated.end(), E) != ADominated.end());
  
  // DはBとCに支配されない
  EXPECT_FALSE(B->dominates(D));
  EXPECT_FALSE(C->dominates(D));
  
  // 各ブロックの支配関係
  EXPECT_TRUE(A->dominates(B));
  EXPECT_TRUE(A->dominates(C));
  EXPECT_TRUE(A->dominates(D));
  EXPECT_TRUE(A->dominates(E));
}

// ループ情報計算テスト
TEST_F(IRGraphTest, LoopInfoCalculation) {
  // シンプルなループCFGを構築:
  //    A
  //    |
  //    v
  //    B <--+
  //    |    |
  //    v    |
  //    C ---+
  //    |
  //    v
  //    D
  
  auto A = graph->createBasicBlock("A");
  auto B = graph->createBasicBlock("B");
  auto C = graph->createBasicBlock("C");
  auto D = graph->createBasicBlock("D");
  
  graph->setEntryBlock(A);
  
  // A -> B
  auto jumpA = graph->createJump(B);
  A->addInstruction(jumpA);
  
  // ループ条件用の変数
  auto loopVar = graph->createVariable(0, "i");
  auto loopCond = graph->createConstant(runtime::Value::createBoolean(true));
  
  // B -> C (条件評価)
  auto branchB = graph->createBranch(loopCond, C, D);
  B->addInstruction(branchB);
  
  // C -> B (ループバック)
  auto jumpC = graph->createJump(B);
  C->addInstruction(jumpC);
  
  // D -> リターン
  auto constVal = graph->createConstant(runtime::Value::createNumber(42.0));
  auto retD = graph->createReturn(constVal);
  D->addInstruction(retD);
  
  // ループ情報計算（内部的に支配関係計算も行われる）
  graph->computeLoopInfo();
  
  // ループヘッダーの検証
  EXPECT_TRUE(B->isLoopHeader());
  EXPECT_FALSE(A->isLoopHeader());
  EXPECT_FALSE(C->isLoopHeader());
  EXPECT_FALSE(D->isLoopHeader());
  
  // ループ深度の検証
  EXPECT_EQ(0u, A->getLoopDepth());
  EXPECT_EQ(1u, B->getLoopDepth());
  EXPECT_EQ(1u, C->getLoopDepth());
  EXPECT_EQ(0u, D->getLoopDepth());
}

// グラフ検証テスト
TEST_F(IRGraphTest, GraphVerification) {
  // 有効なグラフ
  buildSimpleGraph();
  EXPECT_TRUE(graph->verify());
  
  // 無効なグラフ（エントリブロックなし）
  auto invalidGraph = std::make_unique<IRGraph>();
  EXPECT_FALSE(invalidGraph->verify());
}

// メインテスト実行
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
} 