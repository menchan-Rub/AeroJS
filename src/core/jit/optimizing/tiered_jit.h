#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>
#include <mutex>

#include "../jit_compiler.h"
#include "../baseline/baseline_jit.h"
#include "../profiler/execution_profiler.h"
#include "../ir/ir_builder.h"
#include "../ir/ir_optimizer.h"

namespace aerojs {
namespace core {

/**
 * @brief ティアードJITコンパイラ
 *
 * 複数階層のJIT最適化を実装:
 * 1. Baseline JIT: 初回呼び出し時に即座に生成される低コスト・高速な非最適化コード
 * 2. Optimizing JIT: 高頻度実行関数に対してバックグラウンドで最適化を実施
 * 3. Super Optimizing JIT: 非常に高頻度な関数やホットループに対して積極的な最適化を実施
 */
class TieredJIT : public JITCompiler {
 public:
  TieredJIT() noexcept;
  ~TieredJIT() noexcept override;

  /**
   * @brief バイトコードのコンパイル
   * @param bytecodes バイトコードバイト列
   * @param outCodeSize 出力されるコードのサイズ
   * @return 生成したマシンコードバッファ
   */
  [[nodiscard]] std::unique_ptr<uint8_t[]> Compile(const std::vector<uint8_t>& bytecodes,
                                                   size_t& outCodeSize) noexcept override;

  /**
   * @brief 内部状態のリセット
   */
  void Reset() noexcept override;

 private:
  // 基本コンポーネント
  BaselineJIT m_baseline;
  IRBuilder m_irBuilder;
  IROptimizer m_irOptimizer;
  
  // スレッド制御
  std::atomic<bool> m_stop{false};
  
  // バイトコードから関数IDへのマッピング
  std::unordered_map<size_t, uint32_t> m_bytecodeToFunctionId;
  std::mutex m_mapMutex;
  std::atomic<uint32_t> m_nextFunctionId{1};
  
  // 最適化用ユーティリティ
  size_t ComputeHash(const std::vector<uint8_t>& bytecodes) const noexcept;
  uint32_t GetOrCreateFunctionId(const std::vector<uint8_t>& bytecodes) noexcept;
  void StartOptimizationThread(const std::vector<uint8_t>& bytecodes, size_t key, uint32_t functionId) noexcept;
  
  // 最適化ヒューリスティックパラメータ
  static constexpr uint32_t OPTIMIZATION_THRESHOLD = 1000;  ///< 最適化を開始する実行回数の閾値
  static constexpr uint32_t SUPER_OPT_THRESHOLD = 10000;    ///< 超最適化を開始する実行回数の閾値
};

}  // namespace core
}  // namespace aerojs