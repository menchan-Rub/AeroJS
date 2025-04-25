#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <bitset>
#include <array>

#include "../ir/ir.h"
#include "../ir/ir_builder.h"
#include "type_inference.h"

namespace aerojs {
namespace core {

/**
 * @brief ベクトル化可能なパターンの種類を表す列挙型
 */
enum class VectorizationPattern : uint8_t {
  kArrayElementWiseOp,     // 配列要素ごとの演算
  kScalarArrayOp,          // スカラと配列の演算
  kReductionOp,            // 配列の縮約演算（sum, max, minなど）
  kMatrixMultiplication,   // 行列乗算
  kConvolution,            // 畳み込み演算
  kComparisonOp,           // 比較演算
  kTransformOp,            // 変換操作（map操作など）
  kSearchOp,               // 検索操作
  kMax                     // 最大値（パターンの数）
};

/**
 * @brief SIMDの命令セットを表す列挙型
 */
enum class SIMDInstructionSet : uint8_t {
  kNone,     // SIMD命令なし
  kSSE,      // SSE命令セット
  kSSE2,     // SSE2命令セット
  kSSE3,     // SSE3命令セット
  kSSE4,     // SSE4命令セット
  kAVX,      // AVX命令セット
  kAVX2,     // AVX2命令セット
  kAVX512,   // AVX-512命令セット
  kNeon,     // ARM NEON命令セット
  kSVE,      // ARM SVE命令セット
  kWASM,     // WebAssembly SIMD命令セット
  kMax       // 最大値（命令セットの数）
};

/**
 * @brief ベクトル化候補を表す構造体
 */
struct VectorizationCandidate {
  size_t startIndex;                // 開始命令インデックス
  size_t endIndex;                  // 終了命令インデックス
  VectorizationPattern pattern;     // 検出されたパターン
  std::vector<uint32_t> registers;  // 関連するレジスタのリスト
  float speedupEstimate;            // 推定スピードアップ係数
  
  VectorizationCandidate(size_t start, size_t end, VectorizationPattern pat)
    : startIndex(start), endIndex(end), pattern(pat), speedupEstimate(1.0f) {}
};

/**
 * @brief SIMD最適化器のオプションを表す構造体
 */
struct SIMDOptimizerOptions {
  SIMDInstructionSet targetInstructionSet;  // ターゲットSIMD命令セット
  bool enableAutomaticVectorization;        // 自動ベクトル化を有効にするか
  bool enableLoopVectorization;             // ループのベクトル化を有効にするか
  bool enableIfConversion;                  // if文のベクトル化を有効にするか
  float minSpeedupThreshold;                // 最小スピードアップ閾値
  uint32_t maxVectorWidth;                  // 最大ベクトル幅（ビット）
  
  SIMDOptimizerOptions()
    : targetInstructionSet(SIMDInstructionSet::kSSE4)
    , enableAutomaticVectorization(true)
    , enableLoopVectorization(true)
    , enableIfConversion(true)
    , minSpeedupThreshold(1.5f)
    , maxVectorWidth(128) {}
};

/**
 * @brief SIMD命令の情報を表す構造体
 */
struct SIMDInstruction {
  std::string name;                        // 命令名
  uint32_t latency;                        // レイテンシ（サイクル）
  uint32_t throughput;                     // スループット（命令/サイクル）
  std::vector<SIMDInstructionSet> sets;    // サポートされる命令セット
};

/**
 * @brief SIMDベクトル化最適化器クラス
 *
 * IRに対してSIMD命令を活用した最適化を適用する責務を持ちます。
 * 配列処理や数値計算を高速化するため、自動ベクトル化を実行します。
 */
class SIMDOptimizer {
public:
  /**
   * @brief コンストラクタ
   * @param options 最適化オプション
   */
  explicit SIMDOptimizer(const SIMDOptimizerOptions& options = SIMDOptimizerOptions()) noexcept;
  
  /**
   * @brief デストラクタ
   */
  ~SIMDOptimizer() noexcept;
  
  /**
   * @brief IR関数に対してSIMD最適化を実行する
   * @param function 最適化対象のIR関数
   * @param typeInfo 型情報
   * @return 最適化によって変更があった場合true
   */
  bool Optimize(IRFunction& function, const TypeInferenceResult& typeInfo) noexcept;
  
  /**
   * @brief 最適化オプションを設定する
   * @param options 設定するオプション
   */
  void SetOptions(const SIMDOptimizerOptions& options) noexcept;
  
  /**
   * @brief サポートされているSIMD命令セットを検出する
   * @return 検出された最も高度な命令セット
   */
  static SIMDInstructionSet DetectSupportedInstructionSet() noexcept;
  
  /**
   * @brief ベクトル化された命令の統計を取得する
   * @return ベクトル化された命令の数
   */
  uint32_t GetVectorizedInstructionCount() const noexcept;
  
  /**
   * @brief 推定スピードアップ係数を取得する
   * @return 推定スピードアップ係数
   */
  float GetEstimatedSpeedup() const noexcept;

private:
  /**
   * @brief ベクトル化候補を検出する
   * @param function 対象関数
   * @param typeInfo 型情報
   * @return 検出されたベクトル化候補のリスト
   */
  std::vector<VectorizationCandidate> DetectVectorizationCandidates(
      const IRFunction& function, const TypeInferenceResult& typeInfo) noexcept;
  
  /**
   * @brief ループをベクトル化する
   * @param function 対象関数
   * @param loopInfo ループ情報
   * @param typeInfo 型情報
   * @return ベクトル化が成功した場合true
   */
  bool VectorizeLoop(IRFunction& function, 
                    const std::vector<size_t>& loopIndices,
                    const TypeInferenceResult& typeInfo) noexcept;
  
  /**
   * @brief 基本ブロック内の命令をベクトル化する
   * @param function 対象関数
   * @param blockIndices ブロックに含まれる命令のインデックス
   * @param typeInfo 型情報
   * @return ベクトル化が成功した場合true
   */
  bool VectorizeBlock(IRFunction& function,
                     const std::vector<size_t>& blockIndices,
                     const TypeInferenceResult& typeInfo) noexcept;
  
  /**
   * @brief 命令のSIMDバージョンを生成する
   * @param function 対象関数
   * @param instIndex 命令インデックス
   * @param targetSet ターゲット命令セット
   * @return 生成に成功した場合true
   */
  bool GenerateSIMDVersion(IRFunction& function, 
                          size_t instIndex,
                          SIMDInstructionSet targetSet) noexcept;
  
  /**
   * @brief ベクトル化の利益を評価する
   * @param candidate ベクトル化候補
   * @param function 対象関数
   * @param typeInfo 型情報
   * @return 推定スピードアップ係数
   */
  float EvaluateVectorizationBenefit(const VectorizationCandidate& candidate,
                                    const IRFunction& function,
                                    const TypeInferenceResult& typeInfo) noexcept;
  
  /**
   * @brief 命令のSIMD対応を確認する
   * @param opcode 命令コード
   * @param targetSet ターゲット命令セット
   * @return SIMD対応している場合true
   */
  bool IsSIMDCompatible(Opcode opcode, SIMDInstructionSet targetSet) const noexcept;

private:
  // 最適化オプション
  SIMDOptimizerOptions m_options;
  
  // SIMD命令のマッピング
  std::unordered_map<Opcode, std::vector<SIMDInstruction>> m_simdInstructions;
  
  // ベクトル化された命令の数
  uint32_t m_vectorizedInstructionCount;
  
  // 推定スピードアップ係数
  float m_estimatedSpeedup;
  
  // サポートされているSIMD命令セット（静的キャッシュ）
  static SIMDInstructionSet s_supportedInstructionSet;
};

/**
 * @brief SIMD命令セットを文字列に変換する
 * @param set 変換するSIMD命令セット
 * @return 命令セットの文字列表現
 */
std::string SIMDInstructionSetToString(SIMDInstructionSet set);

/**
 * @brief ベクトル化パターンを文字列に変換する
 * @param pattern 変換するベクトル化パターン
 * @return パターンの文字列表現
 */
std::string VectorizationPatternToString(VectorizationPattern pattern);

} // namespace core
} // namespace aerojs 