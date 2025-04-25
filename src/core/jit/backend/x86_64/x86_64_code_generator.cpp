#include "x86_64_code_generator.h"

#include <algorithm>

namespace aerojs {
namespace core {

// 各Opcode向けのエミッタ関数
using EmitFn = void (*)(const IRInstruction&, std::vector<uint8_t>&);

static void EmitNop(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
  out.push_back(0x90);
}
static void EmitLoadConst(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept {
  out.push_back(0x48);
  out.push_back(0xC7);
  out.push_back(0xC0);
  int32_t val = inst.args.empty() ? 0 : inst.args[0];
  out.push_back(static_cast<uint8_t>(val & 0xFF));
  out.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
  out.push_back(0x50);
}
static void EmitLoadVar(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept {
  int32_t idx = inst.args.empty() ? 0 : inst.args[0];
  int32_t disp = -8 * (idx + 1);
  out.push_back(0x48);
  out.push_back(0x8B);
  out.push_back(0x85);
  out.push_back(static_cast<uint8_t>(disp & 0xFF));
  out.push_back(static_cast<uint8_t>((disp >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>((disp >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((disp >> 24) & 0xFF));
  out.push_back(0x50);
}
static void EmitStoreVar(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept {
  int32_t idx = inst.args.empty() ? 0 : inst.args[0];
  int32_t disp = -8 * (idx + 1);
  out.push_back(0x58);
  out.push_back(0x48);
  out.push_back(0x89);
  out.push_back(0x85);
  out.push_back(static_cast<uint8_t>(disp & 0xFF));
  out.push_back(static_cast<uint8_t>((disp >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>((disp >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((disp >> 24) & 0xFF));
}
static void EmitAdd(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
  out.push_back(0x58);
  out.push_back(0x5B);
  out.push_back(0x48);
  out.push_back(0x01);
  out.push_back(0xD8);
  out.push_back(0x50);
}
static void EmitSub(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
  out.push_back(0x5B);
  out.push_back(0x58);
  out.push_back(0x48);
  out.push_back(0x29);
  out.push_back(0xD8);
  out.push_back(0x50);
}
static void EmitMul(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
  out.push_back(0x5B);
  out.push_back(0x58);
  out.push_back(0x48);
  out.push_back(0x0F);
  out.push_back(0xAF);
  out.push_back(0xC3);
  out.push_back(0x50);
}
static void EmitDiv(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
  out.push_back(0x5B);
  out.push_back(0x58);
  out.push_back(0x48);
  out.push_back(0x99);
  out.push_back(0x48);
  out.push_back(0xF7);
  out.push_back(0xFB);
  out.push_back(0x50);
}
static void EmitCall(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
  out.push_back(0x58);
  out.push_back(0xFF);
  out.push_back(0xD0);
  out.push_back(0x50);
}
static void EmitReturn(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
  out.push_back(0xC9);
  out.push_back(0xC3);
}

static constexpr EmitFn kEmitTable[] = {
    EmitNop,
    EmitAdd,
    EmitSub,
    EmitMul,
    EmitDiv,
    EmitLoadConst,
    EmitLoadVar,
    EmitStoreVar,
    EmitCall,
    EmitReturn};

void X86_64CodeGenerator::Generate(const IRFunction& ir, std::vector<uint8_t>& outCode) noexcept {
  // プロローグ
  EmitPrologue(outCode);
  // IR 命令ごとにエミット
  for (const auto& inst : ir.GetInstructions()) {
    EmitInstruction(inst, outCode);
  }
  // エピローグ
  EmitEpilogue(outCode);
}

void X86_64CodeGenerator::EmitPrologue(std::vector<uint8_t>& out) noexcept {
  // push rbp; mov rbp, rsp
  out.push_back(0x55);
  out.push_back(0x48);
  out.push_back(0x89);
  out.push_back(0xE5);
}

void X86_64CodeGenerator::EmitEpilogue(std::vector<uint8_t>& out) noexcept {
  // leave; ret
  out.push_back(0xC9);  // leave
  out.push_back(0xC3);  // ret
}

void X86_64CodeGenerator::EmitInstruction(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept {
  size_t idx = static_cast<size_t>(inst.opcode);
  if (idx < sizeof(kEmitTable) / sizeof(EmitFn)) {
    kEmitTable[idx](inst, out);
  } else {
    EmitNop(inst, out);
  }
}

}  // namespace core
}  // namespace aerojs