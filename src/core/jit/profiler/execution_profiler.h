#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace aerojs {
namespace core {

/**
 * @brief 実行プロファイル情報を収集する構造体
 */
struct ProfileData {
  uint64_t executionCount{0};       ///< 実行回数
  uint64_t totalExecutionTime{0};   ///< 総実行時間（ナノ秒）
  uint32_t typeStability{0};        ///< 型安定性スコア（0-100）
  uint32_t branchBias{0};           ///< 分岐バイアス（0-100）
  bool isHot{false};                ///< ホットな関数かどうか
  bool isStable{false};             ///< 型安定かどうか
  bool hasBranchBias{false};        ///< 分岐バイアスがあるかどうか
  
  // 型情報履歴
  struct TypeInfo {
    uint32_t typeId;                ///< 型ID
    uint32_t frequency;             ///< 出現頻度
  };
  
  std::vector<TypeInfo> typeHistory;  ///< 引数と戻り値の型履歴
  
  // 分岐履歴
  struct BranchInfo {
    uint32_t branchId;              ///< 分岐ID
    uint32_t takenCount;            ///< 分岐が取られた回数
    uint32_t notTakenCount;         ///< 分岐が取られなかった回数
  };
  
  std::vector<BranchInfo> branchHistory;  ///< 分岐履歴
};

/**
 * @brief JavaScript実行プロファイラ
 *
 * 実行時の動作を監視し、JIT最適化の判断材料となる情報を収集します。
 * シングルトンとして実装されています。
 */
class ExecutionProfiler {
public:
  /**
   * @brief シングルトンインスタンスを取得
   * @return プロファイラのインスタンス
   */
  static ExecutionProfiler& Instance() noexcept {
    static ExecutionProfiler instance;
    return instance;
  }
  
  /**
   * @brief 関数の実行開始を記録
   * @param functionId 関数のID
   * @return 実行開始時刻（ナノ秒）
   */
  uint64_t RecordFunctionEntry(uint32_t functionId) noexcept;
  
  /**
   * @brief 関数の実行終了を記録
   * @param functionId 関数のID
   * @param entryTimestamp 実行開始時刻（RecordFunctionEntryの戻り値）
   * @param returnTypeId 戻り値の型ID
   */
  void RecordFunctionExit(uint32_t functionId, uint64_t entryTimestamp, uint32_t returnTypeId) noexcept;
  
  /**
   * @brief 引数の型情報を記録
   * @param functionId 関数のID
   * @param argIndex 引数のインデックス
   * @param typeId 型ID
   */
  void RecordArgumentType(uint32_t functionId, uint32_t argIndex, uint32_t typeId) noexcept;
  
  /**
   * @brief 分岐実行を記録
   * @param functionId 関数のID
   * @param branchId 分岐のID
   * @param taken 分岐が取られたかどうか
   */
  void RecordBranch(uint32_t functionId, uint32_t branchId, bool taken) noexcept;
  
  /**
   * @brief 関数のプロファイルデータを取得
   * @param functionId 関数のID
   * @return プロファイルデータ（存在しない場合はnullptr）
   */
  const ProfileData* GetProfileData(uint32_t functionId) const noexcept;
  
  /**
   * @brief 関数がホットかどうかを判定
   * @param functionId 関数のID
   * @return ホットな関数であればtrue
   */
  bool IsFunctionHot(uint32_t functionId) const noexcept;
  
  /**
   * @brief 関数が型安定かどうかを判定
   * @param functionId 関数のID
   * @return 型安定な関数であればtrue
   */
  bool IsFunctionTypeStable(uint32_t functionId) const noexcept;
  
  /**
   * @brief プロファイラをリセット
   */
  void Reset() noexcept;

private:
  ExecutionProfiler() noexcept = default;
  ~ExecutionProfiler() noexcept = default;
  ExecutionProfiler(const ExecutionProfiler&) = delete;
  ExecutionProfiler& operator=(const ExecutionProfiler&) = delete;
  
  // プロファイリングデータマップ（関数ID → プロファイルデータ）
  std::unordered_map<uint32_t, ProfileData> m_profileData;
  
  // プロファイリングしきい値
  static constexpr uint32_t HOT_FUNCTION_THRESHOLD = 10000;     ///< ホット関数の実行回数しきい値
  static constexpr uint32_t TYPE_STABILITY_THRESHOLD = 95;      ///< 型安定性しきい値（%）
  static constexpr uint32_t BRANCH_BIAS_THRESHOLD = 95;         ///< 分岐バイアスしきい値（%）
  
  // スレッド安全のためのミューテックス
  mutable std::mutex m_mutex;
  
  /**
   * @brief プロファイルデータを更新して最適化の候補かどうかを判定
   * @param functionId 関数のID
   */
  void UpdateOptimizationStatus(uint32_t functionId) noexcept;
};

}  // namespace core
}  // namespace aerojs 