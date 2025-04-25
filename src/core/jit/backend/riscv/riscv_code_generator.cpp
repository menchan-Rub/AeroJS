#include "riscv_code_generator.h"

#include <cstring>  // for std::memcpy

namespace aerojs {
namespace core {

// RISC-V RV64I 形式エンコーディングヘルパー
namespace {
// R-Type: [funct7][rs2][rs1][funct3][rd][opcode]
uint32_t RType(int funct7, int rs2, int rs1, int funct3, int rd, int opcode) noexcept {
  return (uint32_t)((funct7 & 0x7F) << 25) | ((rs2 & 0x1F) << 20) | ((rs1 & 0x1F) << 15) |
         ((funct3 & 0x07) << 12) | ((rd & 0x1F) << 7) | (opcode & 0x7F);
}
// I-Type: [imm[11:0]][rs1][funct3][rd][opcode]
uint32_t IType(int imm, int rs1, int funct3, int rd, int opcode) noexcept {
  return (uint32_t)(((imm & 0xFFF) << 20) | ((rs1 & 0x1F) << 15) |
                    ((funct3 & 0x07) << 12) | ((rd & 0x1F) << 7) | (opcode & 0x7F));
}
// S-Type: [imm[11:5]][rs2][rs1][funct3][imm[4:0]][opcode]
uint32_t SType(int imm, int rs2, int rs1, int funct3, int opcode) noexcept {
  int imm11_5 = (imm >> 5) & 0x7F;
  int imm4_0 = imm & 0x1F;
  return (uint32_t)((imm11_5 << 25) | ((rs2 & 0x1F) << 20) | ((rs1 & 0x1F) << 15) |
                    ((funct3 & 0x07) << 12) | (imm4_0 << 7) | (opcode & 0x7F));
}
// U-Type: [imm[31:12]][rd][opcode]
uint32_t UType(int imm, int rd, int opcode) noexcept {
  return (uint32_t)(((imm & 0xFFFFF000)) | ((rd & 0x1F) << 7) | (opcode & 0x7F));
}
// J-Type: [imm[20]][imm[10:1]][imm[11]][imm[19:12]][rd][opcode]
uint32_t JType(int imm, int rd, int opcode) noexcept {
  int b20 = (imm >> 20) & 0x1;
  int b10_1 = (imm >> 1) & 0x3FF;
  int b11 = (imm >> 11) & 0x1;
  int b19_12 = (imm >> 12) & 0xFF;
  return (uint32_t)((b20 << 31) | (b10_1 << 21) | (b11 << 20) |
                    (b19_12 << 12) | ((rd & 0x1F) << 7) | (opcode & 0x7F));
}
}  // namespace

void RISCVCodeGenerator::Generate(const IRFunction& ir, std::vector<uint8_t>& out) noexcept {
  EmitPrologue(out);
  for (const auto& inst : ir.GetInstructions()) {
    EmitInstruction(inst, out);
  }
  EmitEpilogue(out);
}

void RISCVCodeGenerator::EmitPrologue(std::vector<uint8_t>& out) noexcept {
  // addi sp, sp, -16
  uint32_t insn = IType(-16, /*rs1=*/2, /*funct3=*/0, /*rd=*/2, /*opcode=*/0x13);
  out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
  // sd ra, 8(sp)
  insn = SType(/*imm=*/8, /*rs2=*/1, /*rs1=*/2, /*funct3=*/3, /*opcode=*/0x23);
  out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
  // sd fp, 0(sp)
  insn = SType(/*imm=*/0, /*rs2=*/8, /*rs1=*/2, /*funct3=*/3, /*opcode=*/0x23);
  out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
  // addi fp, sp, 16
  insn = IType(16, /*rs1=*/2, /*funct3=*/0, /*rd=*/8, /*opcode=*/0x13);
  out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
}

void RISCVCodeGenerator::EmitEpilogue(std::vector<uint8_t>& out) noexcept {
  // ld fp, 0(sp)
  uint32_t insn = IType(0, /*rs1=*/2, /*funct3=*/3, /*rd=*/8, /*opcode=*/0x03);
  out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
  // ld ra, 8(sp)
  insn = IType(8, /*rs1=*/2, /*funct3=*/3, /*rd=*/1, /*opcode=*/0x03);
  out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
  // addi sp, sp, 16
  insn = IType(16, /*rs1=*/2, /*funct3=*/0, /*rd=*/2, /*opcode=*/0x13);
  out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
  // ret (jalr x0, x1, 0) here simple: jalr x0, ra, 0
  insn = IType(0, /*rs1=*/1, /*funct3=*/0, /*rd=*/0, /*opcode=*/0x67);
  out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
}

void RISCVCodeGenerator::EmitInstruction(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept {
  uint32_t insn;
  switch (inst.opcode) {
    case Opcode::LoadConst: {
      int32_t val = inst.args.empty() ? 0 : inst.args[0];
      // lui + addi
      int32_t hi = (val + 0x800) >> 12;
      int32_t lo = val - (hi << 12);
      insn = UType(hi << 12, /*rd=*/10, /*opcode=*/0x37);
      out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
      insn = IType(lo, /*rs1=*/10, /*funct3=*/0, /*rd=*/10, /*opcode=*/0x13);
      out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
      // push x10
      insn = IType(-8, /*rs1=*/2, /*funct3=*/0, /*rd=*/2, /*opcode=*/0x13);
      out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
      insn = SType(/*imm=*/0, /*rs2=*/10, /*rs1=*/2, /*funct3=*/3, /*opcode=*/0x23);
      out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
      break;
    }
    case Opcode::Add:
    case Opcode::Sub:
    case Opcode::Mul:
    case Opcode::Div: {
      // pop x11, x10
      insn = IType(0, /*rs1=*/2, /*funct3=*/3, /*rd=*/11, /*opcode=*/0x03);
      out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
      insn = IType(0, /*rs1=*/2, /*funct3=*/3, /*rd=*/10, /*opcode=*/0x03);
      out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
      // addi sp, sp, 16
      insn = IType(16, /*rs1=*/2, /*funct3=*/0, /*rd=*/2, /*opcode=*/0x13);
      out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
      // compute
      switch (inst.opcode) {
        case Opcode::Add:
          insn = RType(0x00, /*rs2=*/11, /*rs1=*/10, /*funct3=*/0, /*rd=*/10, /*opcode=*/0x33);
          break;
        case Opcode::Sub:
          insn = RType(0x20, 11, 10, 0, 10, 0x33);
          break;
        case Opcode::Mul:
          insn = RType(0x01, 11, 10, 0, 10, 0x33);
          insn |= (0 << 12);
          break;
        case Opcode::Div:
          insn = RType(0x01, 11, 10, 4, 10, 0x33);
          break;
        default:
          insn = RType(0x00, 0, 0, 0, 0, 0x33);
          break;
      }
      out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
      // push x10
      insn = IType(-8, 2, 0, 2, 0x13);
      out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
      insn = SType(0, 10, 2, 3, 0x23);
      out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
      break;
    }
    default:
      // 未サポートはNOP (addi x0, x0, 0)
      insn = IType(0, 0, 0, 0, 0x13);
      out.insert(out.end(), reinterpret_cast<uint8_t*>(&insn), reinterpret_cast<uint8_t*>(&insn) + 4);
      break;
  }
}

}  // namespace core
}  // namespace aerojs