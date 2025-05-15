/**
 * @file x86_64_registers.h
 * @brief AeroJS JavaScript エンジンのx86_64レジスタ定義
 * @version 1.0.0
 * @license MIT
 */

#ifndef AEROJS_CORE_JIT_BACKEND_X86_64_REGISTERS_H
#define AEROJS_CORE_JIT_BACKEND_X86_64_REGISTERS_H

#include <cstdint>
#include <array>
#include <string>
#include <bitset>

namespace aerojs {
namespace core {
namespace jit {

/**
 * @brief x86_64の汎用レジスタを表す列挙型
 */
enum class X86_64Register : uint8_t {
  RAX = 0,  // 累積レジスタ、関数戻り値
  RCX = 1,  // カウントレジスタ、第4引数
  RDX = 2,  // データレジスタ、第3引数
  RBX = 3,  // ベースレジスタ、callee-saved
  RSP = 4,  // スタックポインタ
  RBP = 5,  // ベースポインタ、callee-saved
  RSI = 6,  // ソースインデックス、第2引数
  RDI = 7,  // デスティネーションインデックス、第1引数
  R8  = 8,  // 第5引数
  R9  = 9,  // 第6引数
  R10 = 10, // volatile
  R11 = 11, // volatile
  R12 = 12, // callee-saved
  R13 = 13, // callee-saved
  R14 = 14, // callee-saved
  R15 = 15, // callee-saved
  
  // 特殊値
  None = 0xFF
};

/**
 * @brief x86_64のXMMレジスタを表す列挙型 (浮動小数点演算用)
 */
enum class X86_64XMMRegister : uint8_t {
  XMM0 = 0,  // 浮動小数点戻り値、第1引数
  XMM1 = 1,  // 第2引数
  XMM2 = 2,  // 第3引数
  XMM3 = 3,  // 第4引数
  XMM4 = 4,  // 第5引数
  XMM5 = 5,  // 第6引数
  XMM6 = 6,  // 第7引数
  XMM7 = 7,  // 第8引数
  XMM8 = 8,  // volatile
  XMM9 = 9,  // volatile
  XMM10 = 10, // volatile
  XMM11 = 11, // volatile
  XMM12 = 12, // volatile
  XMM13 = 13, // volatile
  XMM14 = 14, // volatile
  XMM15 = 15, // volatile
  
  // 特殊値
  None = 0xFF
};

/**
 * @brief レジスタの種類を表す列挙型
 */
enum class X86_64RegType : uint8_t {
  GP,     // 汎用レジスタ
  XMM,    // XMMレジスタ
  Invalid // 無効
};

/**
 * @brief レジスタの使用状態を管理するクラス
 */
class X86_64RegisterAllocator {
public:
  /**
   * @brief コンストラクタ
   */
  X86_64RegisterAllocator() noexcept;
  
  /**
   * @brief デストラクタ
   */
  ~X86_64RegisterAllocator() noexcept = default;
  
  /**
   * @brief 全てのレジスタを解放する
   */
  void Reset() noexcept;
  
  /**
   * @brief 汎用レジスタを割り当てる
   * @param preferred 優先したいレジスタ (省略可)
   * @return 割り当てられたレジスタ、または割り当て失敗時にNone
   */
  X86_64Register AllocateGPRegister(X86_64Register preferred = X86_64Register::None) noexcept;
  
  /**
   * @brief XMMレジスタを割り当てる
   * @param preferred 優先したいレジスタ (省略可)
   * @return 割り当てられたレジスタ、または割り当て失敗時にNone
   */
  X86_64XMMRegister AllocateXMMRegister(X86_64XMMRegister preferred = X86_64XMMRegister::None) noexcept;
  
  /**
   * @brief 汎用レジスタを解放する
   * @param reg 解放するレジスタ
   */
  void FreeGPRegister(X86_64Register reg) noexcept;
  
  /**
   * @brief XMMレジスタを解放する
   * @param reg 解放するレジスタ
   */
  void FreeXMMRegister(X86_64XMMRegister reg) noexcept;
  
  /**
   * @brief 汎用レジスタが使用中かどうかを確認する
   * @param reg 確認するレジスタ
   * @return 使用中ならtrue
   */
  bool IsGPRegisterAllocated(X86_64Register reg) const noexcept;
  
  /**
   * @brief XMMレジスタが使用中かどうかを確認する
   * @param reg 確認するレジスタ
   * @return 使用中ならtrue
   */
  bool IsXMMRegisterAllocated(X86_64XMMRegister reg) const noexcept;
  
  /**
   * @brief 汎用レジスタが予約済みかどうかを確認する
   * @param reg 確認するレジスタ
   * @return 予約済みならtrue
   */
  bool IsGPRegisterReserved(X86_64Register reg) const noexcept;
  
  /**
   * @brief XMMレジスタが予約済みかどうかを確認する
   * @param reg 確認するレジスタ
   * @return 予約済みならtrue
   */
  bool IsXMMRegisterReserved(X86_64XMMRegister reg) const noexcept;
  
  /**
   * @brief 汎用レジスタを予約する
   * @param reg 予約するレジスタ
   */
  void ReserveGPRegister(X86_64Register reg) noexcept;
  
  /**
   * @brief XMMレジスタを予約する
   * @param reg 予約するレジスタ
   */
  void ReserveXMMRegister(X86_64XMMRegister reg) noexcept;
  
  /**
   * @brief 汎用レジスタを文字列として取得する
   * @param reg レジスタ
   * @param size レジスタサイズ (1=8bit, 2=16bit, 4=32bit, 8=64bit)
   * @return レジスタ名の文字列
   */
  static std::string GetGPRegisterName(X86_64Register reg, uint8_t size = 8) noexcept;
  
  /**
   * @brief XMMレジスタを文字列として取得する
   * @param reg レジスタ
   * @param isDouble 倍精度浮動小数点かどうか
   * @return レジスタ名の文字列
   */
  static std::string GetXMMRegisterName(X86_64XMMRegister reg, bool isDouble = true) noexcept;

  /**
   * @brief 呼び出し規約で保存すべきレジスタを取得
   * @return callee-savedレジスタのビットマスク
   */
  static uint16_t GetCalleeSavedRegistersMask() noexcept;

private:
  // 汎用レジスタの使用状態
  std::bitset<16> m_allocatedGPRegs;
  
  // XMMレジスタの使用状態
  std::bitset<16> m_allocatedXMMRegs;
  
  // 予約済み汎用レジスタ
  std::bitset<16> m_reservedGPRegs;
  
  // 予約済みXMMレジスタ
  std::bitset<16> m_reservedXMMRegs;
};

} // namespace jit
} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_BACKEND_X86_64_REGISTERS_H 