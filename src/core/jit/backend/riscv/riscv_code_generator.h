#pragma once

#include <cstdint>
#include <vector>

#include "../../ir/ir.h"

namespace aerojs {
namespace core {

/**
 * @brief RISC‑V 向け機械語生成器（骨子）
 *
 * 中間表現 IRFunction を受け取り、RV64I 命令をバイトバッファに emit します。
 */
class RISCVCodeGenerator {
 public:
  RISCVCodeGenerator() noexcept = default;
  ~RISCVCodeGenerator() noexcept = default;

  /**
   * @brief IRFunction からマシンコードを生成する
   */
  void Generate(const IRFunction& ir, std::vector<uint8_t>& out) noexcept;

 private:
  void EmitPrologue(std::vector<uint8_t>& out) noexcept;
  void EmitEpilogue(std::vector<uint8_t>& out) noexcept;
  void EmitInstruction(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept;
};

}  // namespace core
}  // namespace aerojs