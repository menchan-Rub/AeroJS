/**
 * @file identifier_lookup_optimization_test.cpp
 * @brief 識別子検索の最適化トランスフォーマーのテスト
 * @version 0.1
 * @date 2024-01-11
 *
 * @copyright Copyright (c) 2024 AeroJS Project
 *
 * @license
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../../src/core/transformers/identifier_lookup_optimization.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "../../src/core/ast/ast_node.h"
#include "../../src/core/parser/parser.h"

namespace aero {
namespace transformers {
namespace test {

/**
 * @brief 識別子検索の最適化トランスフォーマーのテスト
 */
class IdentifierLookupOptimizerTest : public ::testing::Test {
 protected:
  IdentifierLookupOptimizerTest()
      : transformer() {
  }

  void SetUp() override {
    transformer.resetCounters();
    transformer.enableStatistics(true);
  }

  /**
   * @brief コードをパースして最適化を適用する
   * @param code JavaScript コード
   * @return 最適化後のノード
   */
  ast::NodePtr parseAndTransform(const std::string& code) {
    parser::Parser parser;
    auto ast = parser.parse(code);
    EXPECT_TRUE(ast != nullptr) << "AST parsing failed";

    return transformer.transform(ast);
  }

  /**
   * @brief テスト用のIdentifierLookupOptimizerインスタンス
   */
  IdentifierLookupOptimizer transformer;
};

/**
 * @brief グローバルスコープでの識別子最適化をテスト
 */
TEST_F(IdentifierLookupOptimizerTest, GlobalScopeIdentifiers) {
  const std::string code = R"(
        const a = 1;
        let b = 2;
        var c = 3;
        
        function test() {
            console.log(a, b, c);
        }
    )";

  auto optimizedAst = parseAndTransform(code);
  EXPECT_TRUE(optimizedAst != nullptr);
  EXPECT_GT(transformer.getOptimizedIdentifiersCount(), 0);
}

/**
 * @brief ネストされたスコープでの識別子最適化をテスト
 */
TEST_F(IdentifierLookupOptimizerTest, NestedScopeIdentifiers) {
  const std::string code = R"(
        const outer = 10;
        
        function test() {
            const inner = 20;
            
            if (true) {
                const innerBlock = 30;
                console.log(outer, inner, innerBlock);
            }
            
            return () => {
                console.log(outer, inner);
            };
        }
    )";

  auto optimizedAst = parseAndTransform(code);
  EXPECT_TRUE(optimizedAst != nullptr);
  EXPECT_GT(transformer.getOptimizedIdentifiersCount(), 0);
  EXPECT_GT(transformer.getScopeHierarchyOptimizationsCount(), 0);
}

/**
 * @brief シャドウィング（変数の上書き）を含むスコープでの最適化をテスト
 */
TEST_F(IdentifierLookupOptimizerTest, VariableShadowing) {
  const std::string code = R"(
        const value = "outer";
        
        function test() {
            const value = "inner";
            console.log(value); // inner を参照
            
            function nested() {
                console.log(value); // inner を参照
            }
            
            return nested;
        }
        
        console.log(value); // outer を参照
    )";

  auto optimizedAst = parseAndTransform(code);
  EXPECT_TRUE(optimizedAst != nullptr);
  EXPECT_GT(transformer.getOptimizedIdentifiersCount(), 0);
}

/**
 * @brief クロージャー内の識別子参照の最適化をテスト
 */
TEST_F(IdentifierLookupOptimizerTest, ClosureIdentifiers) {
  const std::string code = R"(
        function createCounter() {
            let count = 0;
            
            return {
                increment: function() {
                    count++;
                    return count;
                },
                decrement: function() {
                    count--;
                    return count;
                },
                getCount: function() {
                    return count;
                }
            };
        }
    )";

  auto optimizedAst = parseAndTransform(code);
  EXPECT_TRUE(optimizedAst != nullptr);
  EXPECT_GT(transformer.getOptimizedIdentifiersCount(), 0);
}

/**
 * @brief 複合スコープと多数の識別子を含むコードの最適化をテスト
 */
TEST_F(IdentifierLookupOptimizerTest, ComplexScopeIdentifiers) {
  const std::string code = R"(
        const GLOBAL_CONST = "global";
        let globalVar = 100;
        
        function outer(param1, param2) {
            const outerConst = "outer";
            let outerVar = 200;
            
            function middle() {
                const middleConst = "middle";
                let middleVar = 300;
                
                return function inner() {
                    const innerConst = "inner";
                    let innerVar = 400;
                    
                    // 様々なスコープからの参照
                    console.log(
                        GLOBAL_CONST, globalVar,
                        param1, param2, outerConst, outerVar,
                        middleConst, middleVar,
                        innerConst, innerVar
                    );
                };
            }
            
            return middle;
        }
    )";

  auto optimizedAst = parseAndTransform(code);
  EXPECT_TRUE(optimizedAst != nullptr);
  EXPECT_GT(transformer.getOptimizedIdentifiersCount(), 10);
  EXPECT_GT(transformer.getScopeHierarchyOptimizationsCount(), 3);
}

/**
 * @brief 統計情報の収集機能をテスト
 */
TEST_F(IdentifierLookupOptimizerTest, StatisticsCollection) {
  // 統計情報を無効にしてテスト
  transformer.enableStatistics(false);
  const std::string code = "const a = 1; function test() { return a; }";

  auto optimizedAst = parseAndTransform(code);
  EXPECT_TRUE(optimizedAst != nullptr);
  EXPECT_EQ(transformer.getOptimizedIdentifiersCount(), 0);

  // 統計情報をリセットして有効にしてテスト
  transformer.resetCounters();
  transformer.enableStatistics(true);
  optimizedAst = parseAndTransform(code);
  EXPECT_TRUE(optimizedAst != nullptr);
  EXPECT_GT(transformer.getOptimizedIdentifiersCount(), 0);
}

}  // namespace test
}  // namespace transformers
}  // namespace aero