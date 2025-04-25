#pragma once

#include <cstdint>
#include <vector>

#include "../../ir/ir.h"

namespace aerojs {
namespace core {

/**
 * @brief x86_64 向け機械語生成器
 *
 * 中間表現 (IRFunction) を受け取り、
 * ネイティブ機械語をバイトバッファとして生成します。
 */
class X86_64CodeGenerator {
 public:
  X86_64CodeGenerator() noexcept = default;
  ~X86_64CodeGenerator() noexcept = default;

  /**
   * @brief IRFunction からマシンコードを生成する
   * @param ir 中間表現関数
   * @param outCode 生成した機械語バイト列を格納する
   */
  void Generate(const IRFunction& ir, std::vector<uint8_t>& outCode) noexcept;

 private:
  void EmitPrologue(std::vector<uint8_t>& out) noexcept;
  void EmitEpilogue(std::vector<uint8_t>& out) noexcept;
  void EmitInstruction(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept;
};

}  // namespace core
}  // namespace aerojs