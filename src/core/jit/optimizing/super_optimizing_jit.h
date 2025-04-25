#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

#include "../ir/ir.h"
#include "../profiler/profile_data.h"
#include "optimizing_jit.h"
#include "../../optimization/passes/pass_manager.h"

namespace aerojs {
namespace core {

/**
 * @brief 超最適化JITコンパイラ
 *
 * V8を超えるパフォーマンスを目指し、複数ステージの最適化パスと
 * プロファイル情報に基づく高度なコード生成を行います。
 */
class SuperOptimizingJIT : public OptimizingJIT {
 public:
  SuperOptimizingJIT() noexcept;
  ~SuperOptimizingJIT() noexcept override;

  std::unique_ptr<uint8_t[]> Compile(const std::vector<uint8_t>& bytecodes,
                                     size_t& outCodeSize) noexcept override;
  void Reset() noexcept override;

 private:
  /**
   * @brief 中間表現(IR)を構築する
   * @param bytecodes バイトコード列
   */
  void BuildIR(const std::vector<uint8_t>& bytecodes) noexcept;

  IRFunction ir_;  ///< コンパイル中の中間表現を保持
  ProfileData profileData_;  ///< 実行時プロファイル情報
  PassManager passManager_;  ///< 最適化パスマネージャー
  
  struct CompilationMetadata {
    std::unordered_map<uint32_t, uint32_t> bytecodeToIRMap;  ///< バイトコードオフセットからIR位置へのマップ
    std::unordered_map<std::string, uint32_t> symbolTable;   ///< シンボルテーブル
    std::vector<uint32_t> deoptPoints;                       ///< 最適化解除ポイント
    uint32_t hotLoopCount;                                   ///< ホットループ数
    uint32_t inlinedFunctions;                               ///< インライン展開された関数数
    
    CompilationMetadata() : hotLoopCount(0), inlinedFunctions(0) {}
  };
  
  CompilationMetadata metadata_;  ///< コンパイル時メタデータ

  /**
   * @brief 複数ステージの最適化パスを実行する
   */
  void RunOptimizationPasses() noexcept;

  /**
   * @brief コード生成を行う
   */
  void GenerateMachineCode(std::vector<uint8_t>& codeBuffer) noexcept;

  /**
   * @brief 型推論を実行する
   */
  void PerformTypeInference() noexcept;

  /**
   * @brief 関数のインライン展開を行う
   */
  void InlineFunctions() noexcept;

  /**
   * @brief ループ最適化を行う
   */
  void OptimizeLoops() noexcept;

  /**
   * @brief 最適化解除ポイントを設定する
   */
  void SetupDeoptimizationPoints() noexcept;

  static constexpr size_t kAverageInstSize = 8;            ///< 命令あたり平均バイト数の推定値
  static constexpr size_t kMaxLocalVars = 256;             ///< 最大ローカル変数数
  static constexpr size_t kMaxFunctionTableEntries = 256;  ///< 最大関数テーブルエントリ数
  static constexpr size_t kMaxBytecodeLength = 1 << 20;    ///< 最大バイトコード長 (1MB)
  
  /**
   * @brief バイトコードの妥当性を検証する
   * @param bytecodes 入力バイトコード
   * @return 合法的ならtrue, 不正な命令があればfalse
   */
  bool ValidateBytecode(const std::vector<uint8_t>& bytecodes) noexcept;
};

}  // namespace core
}  // namespace aerojs