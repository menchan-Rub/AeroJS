#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <memory>

namespace aerojs {
namespace core {

/**
 * @brief レジスタアロケーションの戦略を表す列挙型
 */
enum class RegisterAllocationStrategy {
  kLinearScan,   // 線形スキャン法
  kGreedy,       // 貪欲法
  kGraph         // グラフ彩色法
};

/**
 * @brief 物理レジスタの種類を表す列挙型
 */
enum class PhysicalRegisterType {
  kGeneral,      // 汎用レジスタ
  kFloat,        // 浮動小数点レジスタ
  kVector        // ベクトルレジスタ
};

/**
 * @brief 物理レジスタを表す構造体
 */
struct PhysicalRegister {
  uint32_t id;                   // レジスタID
  std::string name;              // レジスタ名
  PhysicalRegisterType type;     // レジスタタイプ
  bool isCallerSaved;            // 呼び出し元保存レジスタかどうか
  bool isCalleeSaved;            // 呼び出し先保存レジスタかどうか
  bool isReserved;               // 予約済みかどうか
};

/**
 * @brief 仮想レジスタを表す構造体
 */
struct VirtualRegister {
  uint32_t id;                   // 仮想レジスタID
  PhysicalRegisterType type;     // 必要なレジスタタイプ
  int liveRangeStart;            // 生存範囲の開始
  int liveRangeEnd;              // 生存範囲の終了
  uint32_t physicalRegId;        // 割り当てられた物理レジスタID
  bool isSpilled;                // スピルされたかどうか
  int spillSlot;                 // スピルスロット番号
};

/**
 * @brief レジスタアロケータクラス
 *
 * バイトコード内の仮想レジスタを物理レジスタに割り当てる責務を持ちます。
 * 複数のアロケーション戦略をサポートし、最適なレジスタ割り当てを行います。
 */
class RegisterAllocator {
 public:
  /**
   * @brief コンストラクタ
   * @param strategy 使用するレジスタ割り当て戦略
   */
  explicit RegisterAllocator(
      RegisterAllocationStrategy strategy = RegisterAllocationStrategy::kLinearScan) noexcept;
  
  /**
   * @brief デストラクタ
   */
  ~RegisterAllocator() noexcept;
  
  /**
   * @brief 新しい仮想レジスタを割り当てる
   * @param type 必要なレジスタタイプ
   * @return 割り当てられた仮想レジスタID
   */
  uint32_t AllocateVirtualRegister(PhysicalRegisterType type = PhysicalRegisterType::kGeneral) noexcept;
  
  /**
   * @brief 仮想レジスタに物理レジスタを割り当てる
   * @param instructions 命令列（生存範囲分析に使用）
   * @return 割り当てに成功した場合true
   */
  bool AllocateRegisters(const std::vector<uint32_t>& instructions) noexcept;
  
  /**
   * @brief 仮想レジスタに割り当てられた物理レジスタを取得する
   * @param virtualRegId 仮想レジスタID
   * @return 物理レジスタID（割り当てられていない場合は0xFFFFFFFF）
   */
  uint32_t GetPhysicalRegister(uint32_t virtualRegId) const noexcept;
  
  /**
   * @brief 仮想レジスタがスピルされているかどうかを取得する
   * @param virtualRegId 仮想レジスタID
   * @return スピルされている場合true
   */
  bool IsSpilled(uint32_t virtualRegId) const noexcept;
  
  /**
   * @brief スピルされた仮想レジスタのスピルスロット番号を取得する
   * @param virtualRegId 仮想レジスタID
   * @return スピルスロット番号（スピルされていない場合は-1）
   */
  int GetSpillSlot(uint32_t virtualRegId) const noexcept;
  
  /**
   * @brief レジスタアロケータの状態をリセットする
   */
  void Reset() noexcept;
  
  /**
   * @brief レジスタ割り当て戦略を設定する
   * @param strategy 使用する戦略
   */
  void SetStrategy(RegisterAllocationStrategy strategy) noexcept;

 private:
  /**
   * @brief 線形スキャン法でレジスタを割り当てる
   * @return 割り当てに成功した場合true
   */
  bool AllocateRegistersLinearScan() noexcept;
  
  /**
   * @brief 貪欲法でレジスタを割り当てる
   * @return 割り当てに成功した場合true
   */
  bool AllocateRegistersGreedy() noexcept;
  
  /**
   * @brief グラフ彩色法でレジスタを割り当てる
   * @return 割り当てに成功した場合true
   */
  bool AllocateRegistersGraph() noexcept;
  
  /**
   * @brief 生存範囲分析を行う
   * @param instructions 命令列
   */
  void AnalyzeLiveRanges(const std::vector<uint32_t>& instructions) noexcept;
  
  /**
   * @brief 物理レジスタの一覧を初期化する
   */
  void InitializePhysicalRegisters() noexcept;

 private:
  // 使用するレジスタ割り当て戦略
  RegisterAllocationStrategy m_strategy;
  
  // 次に割り当てる仮想レジスタID
  uint32_t m_nextVirtualRegId;
  
  // 仮想レジスタの一覧
  std::unordered_map<uint32_t, VirtualRegister> m_virtualRegisters;
  
  // 物理レジスタの一覧
  std::vector<PhysicalRegister> m_physicalRegisters;
  
  // 現在使用中の物理レジスタ
  std::unordered_set<uint32_t> m_usedPhysicalRegs;
  
  // 次に割り当てるスピルスロット番号
  int m_nextSpillSlot;
};

}  // namespace core
}  // namespace aerojs 