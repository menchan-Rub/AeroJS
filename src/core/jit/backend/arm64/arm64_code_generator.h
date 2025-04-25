#pragma once

#include <cstdint>
#include <vector>

#include "../../ir/ir.h"

namespace aerojs {
namespace core {

/**
 * @brief ARM64 向け機械語生成器（骨子）
 */
class ARM64CodeGenerator {
 public:
  ARM64CodeGenerator() noexcept = default;
  ~ARM64CodeGenerator() noexcept = default;

  void Generate(const IRFunction& ir, std::vector<uint8_t>& outCode) noexcept;

 private:
  void EmitPrologue(std::vector<uint8_t>& out) noexcept;
  void EmitEpilogue(std::vector<uint8_t>& out) noexcept;
  void EmitInstruction(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept;
};

}  // namespace core
}  // namespace aerojs