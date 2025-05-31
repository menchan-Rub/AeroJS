/**
 * @file parallel_array_optimization.h
 * @version 1.0.0
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 * @brief 並列処理に対応した高度な配列最適化トランスフォーマー
 * 
 * @details
 * このファイルは、AeroJSエンジンの配列操作を最適化する並列処理対応トランスフォーマーを定義します。
 * SIMD命令、マルチスレッド処理、メモリアクセスパターン最適化などの最新技術を活用し、
 * 配列操作の実行速度を大幅に向上させます。
 */

#ifndef AEROJS_TRANSFORMERS_PARALLEL_ARRAY_OPTIMIZATION_H
#define AEROJS_TRANSFORMERS_PARALLEL_ARRAY_OPTIMIZATION_H

#include <string>
#include <unordered_map>
#include <bitset>
#include <vector>
#include <algorithm>
#include <iostream>
#include <thread>

#include "../parser/ast/node.h"
#include "../parser/ast/node_visitor.h"
#include "../parser/ast/statements.h"
#include "../parser/ast/expressions.h"

namespace aerojs::transformers {

/**
 * 配列操作の並列化・ベクトル化最適化を行うトランスフォーマー
 * 
 * このクラスは以下の最適化を適用します：
 * 1. SIMD命令セットを使った順次アクセスループのベクトル化
 * 2. SIMD命令セットを使ったストライドアクセスループのベクトル化
 * 3. スレッドプールを使ったループの並列化
 * 4. for...ofループの並列化
 * 5. キャッシュ効率を考慮したループタイリング
 * 6. ストライドアクセスの最適化
 * 7. ギャザー/スキャッターアクセスの最適化
 */
class ParallelArrayOptimizationTransformer : public parser::ast::NodeVisitor {
public:
  // サポートしているSIMD拡張機能のビットマスク
  enum class SIMDSupport {
    SSE4_2,   // x86-64: SSE4.2
    AVX,      // x86-64: AVX
    AVX2,     // x86-64: AVX2
    AVX512,   // x86-64: AVX-512
    NEON,     // ARM: NEON / AArch64 Advanced SIMD
    SVE,      // ARM: Scalable Vector Extension
    RVV,      // RISC-V Vector Extension
    WASM_SIMD // WebAssembly SIMD
  };

  // コンストラクタ
  ParallelArrayOptimizationTransformer(const std::bitset<8>& supportedSimdFeatures = 0, bool debugMode = false);

  // AST訪問メソッド
  virtual bool Visit(parser::ast::ForStatement* node) override;
  virtual bool Visit(parser::ast::ForOfStatement* node) override;

  // 最適化結果の統計情報を取得
  const std::unordered_map<std::string, int>& getOptimizationStats() const;

private:
  // 依存関係解析
  class DependencyAnalysisVisitor;
  
  // 複雑性解析
  class ComplexityVisitor;

  // 変換関数
  parser::ast::NodePtr generateSIMDSequentialLoopCode(parser::ast::ForStatement* forStmt, const std::string& arrayName);
  parser::ast::NodePtr generateSIMDStridedLoopCode(parser::ast::ForStatement* forStmt, const std::string& arrayName);
  parser::ast::NodePtr generateParallelLoopCode(parser::ast::ForStatement* forStmt, const std::string& arrayName);
  parser::ast::NodePtr generateParallelForOfCode(parser::ast::ForOfStatement* forOfStmt, const std::string& arrayName);
  parser::ast::NodePtr generateCacheOptimizedLoopCode(parser::ast::ForStatement* forStmt, const std::string& arrayName);
  parser::ast::NodePtr generateStrideOptimizedLoopCode(parser::ast::ForStatement* forStmt, const std::string& arrayName);
  parser::ast::NodePtr generateGatherScatterOptimizedCode(parser::ast::ForStatement* forStmt, const std::string& arrayName);

  // ユーティリティ関数
  std::string extractLoopIndexVariable(parser::ast::Node* init);
  std::string extractLoopUpperBound(parser::ast::Node* test);
  std::string getSIMDFunctionName(const std::string& opType);
  std::string getAVXOperationFunction(const std::string& opType);
  std::string getNEONOperationFunction(const std::string& opType);
  std::string getRVVOperationFunction(const std::string& opType);
  parser::ast::Node* clone(parser::ast::Node* node);
  parser::ast::Expression* createPropertyAccess(parser::ast::Expression* object, const std::string& propertyName);
  void replaceNodeInParent(parser::ast::Node* oldNode, parser::ast::NodePtr newNode);
  int GetOptimalThreadCount();
  void UpdateStatistics(const std::string& optimizationType);

  // メンバ変数
  std::bitset<8> m_supportedSimdFeatures;
  bool m_debugMode;
  std::unordered_map<std::string, int> m_optimizationStats;
};

} // namespace aerojs::transformers

#endif // AEROJS_TRANSFORMERS_PARALLEL_ARRAY_OPTIMIZATION_H 