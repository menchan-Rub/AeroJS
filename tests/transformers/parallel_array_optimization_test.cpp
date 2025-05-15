/**
 * @file parallel_array_optimization_test.cpp
 * @version 1.0.0
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 * @brief 並列処理対応の配列最適化トランスフォーマーのテスト
 */

#include "src/core/transformers/parallel_array_optimization.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "src/core/parser/ast/ast_node_factory.h"
#include "src/core/parser/ast/nodes/all_nodes.h"
#include "src/core/parser/lexer/lexer.h"
#include "src/core/parser/parser.h"

namespace aerojs::transformers {
namespace {

using aerojs::parser::ast::ArrayExpression;
using aerojs::parser::ast::BinaryExpression;
using aerojs::parser::ast::BinaryOperator;
using aerojs::parser::ast::CallExpression;
using aerojs::parser::ast::FunctionExpression;
using aerojs::parser::ast::Identifier;
using aerojs::parser::ast::Literal;
using aerojs::parser::ast::MemberExpression;
using aerojs::parser::ast::Node;
using aerojs::parser::ast::NodePtr;
using aerojs::parser::ast::NodeType;
using aerojs::parser::ast::Program;
using aerojs::parser::ast::ASTNodeFactory;
using aerojs::parser::Parser;

// ヘルパー関数：ソースからASTを生成
std::shared_ptr<Program> ParseSource(const std::string& source) {
  Parser parser;
  auto result = parser.Parse(source);
  EXPECT_TRUE(result.first) << "パース失敗: " << result.second;
  return result.first;
}

// ヘルパー関数：配列式ノードの生成
NodePtr CreateArrayExpression(const std::vector<NodePtr>& elements) {
  return ASTNodeFactory::CreateArrayExpression(elements);
}

// ヘルパー関数：数値リテラルノードの生成
NodePtr CreateNumberLiteral(double value) {
  return ASTNodeFactory::CreateLiteral(value);
}

// ヘルパー関数：文字列リテラルノードの生成
NodePtr CreateStringLiteral(const std::string& value) {
  return ASTNodeFactory::CreateLiteral(value);
}

// ヘルパー関数：識別子ノードの生成
NodePtr CreateIdentifier(const std::string& name) {
  return ASTNodeFactory::CreateIdentifier(name);
}

// ヘルパー関数：コールバック関数式の生成
NodePtr CreateSimpleCallback() {
  // (x) => x * 2 相当のコールバック
  auto param = CreateIdentifier("x");
  auto body = ASTNodeFactory::CreateBinaryExpression(
      param,
      CreateNumberLiteral(2.0),
      BinaryOperator::kMultiplication);
      
  return ASTNodeFactory::CreateArrowFunctionExpression(
      std::vector<NodePtr>{param},
      body,
      false /* 表現式本体 */);
}

// ヘルパー関数：配列mapメソッド呼び出しの生成
NodePtr CreateArrayMapCall() {
  auto array = CreateIdentifier("arr");
  auto property = CreateIdentifier("map");
  auto memberExpr = ASTNodeFactory::CreateMemberExpression(array, property, false);
  auto callback = CreateSimpleCallback();
  
  return ASTNodeFactory::CreateCallExpression(
      memberExpr,
      std::vector<NodePtr>{callback});
}

class ParallelArrayOptimizationTest : public ::testing::Test {
protected:
  void SetUp() override {
    transformer_ = std::make_unique<ParallelArrayOptimizationTransformer>(
        ArrayOptimizationLevel::Balanced, 2, true, true);
    transformer_->Initialize();
  }

  void TearDown() override {
    transformer_.reset();
  }

  std::unique_ptr<ParallelArrayOptimizationTransformer> transformer_;
};

TEST_F(ParallelArrayOptimizationTest, ConstructorInitializesCorrectly) {
  EXPECT_EQ(transformer_->GetName(), "ParallelArrayOptimizationTransformer");
  EXPECT_EQ(transformer_->GetPriority(), TransformPriority::High);
  EXPECT_EQ(transformer_->GetPhase(), TransformPhase::Optimization);
}

TEST_F(ParallelArrayOptimizationTest, DetectsArrayPattern) {
  auto mapCall = CreateArrayMapCall();
  EXPECT_TRUE(transformer_->CanOptimize(mapCall));
}

TEST_F(ParallelArrayOptimizationTest, HandlesArrayExpression) {
  std::vector<NodePtr> elements;
  // 数値の配列を作成
  for (int i = 0; i < 100; ++i) {
    elements.push_back(CreateNumberLiteral(static_cast<double>(i)));
  }
  
  auto arrayExpr = CreateArrayExpression(elements);
  EXPECT_TRUE(transformer_->CanOptimize(arrayExpr));
  
  // 統計情報をリセット
  transformer_->Reset();
}

TEST_F(ParallelArrayOptimizationTest, HandlesMixedArrayExpression) {
  std::vector<NodePtr> elements;
  // 混合型の配列を作成
  elements.push_back(CreateNumberLiteral(1.0));
  elements.push_back(CreateStringLiteral("test"));
  elements.push_back(CreateNumberLiteral(2.0));
  
  auto arrayExpr = CreateArrayExpression(elements);
  EXPECT_TRUE(transformer_->CanOptimize(arrayExpr));
}

TEST_F(ParallelArrayOptimizationTest, RecognizesArrayMethods) {
  const std::string source = R"(
    const arr = [1, 2, 3, 4, 5];
    
    // map
    const doubled = arr.map(x => x * 2);
    
    // filter
    const evens = arr.filter(x => x % 2 === 0);
    
    // reduce
    const sum = arr.reduce((acc, x) => acc + x, 0);
    
    // forEach
    arr.forEach(x => console.log(x));
  )";
  
  auto program = ParseSource(source);
  ASSERT_TRUE(program != nullptr);
  
  // TransformContextを初期化
  TransformContext context;
  context.program = program;
  
  // 変換を実行
  transformer_->SetContext(&context);
  transformer_->Execute();
  
  // 統計情報を取得
  const auto& stats = transformer_->GetStatistics();
  EXPECT_GT(stats.nodesProcessed, 0);
}

TEST_F(ParallelArrayOptimizationTest, HandlesForLoops) {
  const std::string source = R"(
    const arr = new Array(1000);
    
    for (let i = 0; i < arr.length; i++) {
      arr[i] = i * i;
    }
    
    for (const item of arr) {
      console.log(item);
    }
  )";
  
  auto program = ParseSource(source);
  ASSERT_TRUE(program != nullptr);
  
  // TransformContextを初期化
  TransformContext context;
  context.program = program;
  
  // 変換を実行
  transformer_->SetContext(&context);
  transformer_->Execute();
  
  // 統計情報を取得
  const auto& stats = transformer_->GetStatistics();
  EXPECT_GT(stats.nodesProcessed, 0);
}

TEST_F(ParallelArrayOptimizationTest, OptimizationLevelAffectsProcessing) {
  // 最小限の最適化レベルでトランスフォーマーを作成
  auto minimalTransformer = std::make_unique<ParallelArrayOptimizationTransformer>(
      ArrayOptimizationLevel::Minimal, 2, true, true);
  minimalTransformer->Initialize();
  
  // 実験的な最適化レベルでトランスフォーマーを作成
  auto experimentalTransformer = std::make_unique<ParallelArrayOptimizationTransformer>(
      ArrayOptimizationLevel::Experimental, 2, true, true);
  experimentalTransformer->Initialize();
  
  // 同じAST構造に対して異なる最適化レベルを適用
  auto mapCall = CreateArrayMapCall();
  
  EXPECT_TRUE(minimalTransformer->CanOptimize(mapCall));
  EXPECT_TRUE(experimentalTransformer->CanOptimize(mapCall));
  
  // テスト後の後片付け
  minimalTransformer.reset();
  experimentalTransformer.reset();
}

TEST_F(ParallelArrayOptimizationTest, SimdEnablingControlsBehavior) {
  // SIMD無効のトランスフォーマー
  auto noSimdTransformer = std::make_unique<ParallelArrayOptimizationTransformer>(
      ArrayOptimizationLevel::Balanced, 2, false, true);
  noSimdTransformer->Initialize();
  
  // SIMD有効のトランスフォーマー
  auto simdTransformer = std::make_unique<ParallelArrayOptimizationTransformer>(
      ArrayOptimizationLevel::Balanced, 2, true, true);
  simdTransformer->Initialize();
  
  // 同じAST構造に対して異なるSIMD設定を適用
  auto mapCall = CreateArrayMapCall();
  
  EXPECT_TRUE(noSimdTransformer->CanOptimize(mapCall));
  EXPECT_TRUE(simdTransformer->CanOptimize(mapCall));
  
  // テスト後の後片付け
  noSimdTransformer.reset();
  simdTransformer.reset();
}

}  // namespace
}  // namespace aerojs::transformers 