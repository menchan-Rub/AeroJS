/**
 * @file x86_64_registers.cpp
 * @brief AeroJS JavaScript エンジンのx86_64レジスタ定義の実装
 * @version 1.0.0
 * @license MIT
 */

#include "x86_64_registers.h"
#include <cassert>

namespace aerojs {
namespace core {
namespace jit {

// レジスタ名のテーブル（8, 16, 32, 64ビット）
static const std::array<std::array<const char*, 4>, 16> GP_REG_NAMES = {{
  {{"al", "ax", "eax", "rax"}},   // RAX
  {{"cl", "cx", "ecx", "rcx"}},   // RCX
  {{"dl", "dx", "edx", "rdx"}},   // RDX
  {{"bl", "bx", "ebx", "rbx"}},   // RBX
  {{"spl", "sp", "esp", "rsp"}},  // RSP
  {{"bpl", "bp", "ebp", "rbp"}},  // RBP
  {{"sil", "si", "esi", "rsi"}},  // RSI
  {{"dil", "di", "edi", "rdi"}},  // RDI
  {{"r8b", "r8w", "r8d", "r8"}},  // R8
  {{"r9b", "r9w", "r9d", "r9"}},  // R9
  {{"r10b", "r10w", "r10d", "r10"}}, // R10
  {{"r11b", "r11w", "r11d", "r11"}}, // R11
  {{"r12b", "r12w", "r12d", "r12"}}, // R12
  {{"r13b", "r13w", "r13d", "r13"}}, // R13
  {{"r14b", "r14w", "r14d", "r14"}}, // R14
  {{"r15b", "r15w", "r15d", "r15"}}  // R15
}};

// XMMレジスタ名のテーブル（float/double）
static const std::array<std::array<const char*, 2>, 16> XMM_REG_NAMES = {{
  {{"xmm0", "xmm0"}},  // XMM0
  {{"xmm1", "xmm1"}},  // XMM1
  {{"xmm2", "xmm2"}},  // XMM2
  {{"xmm3", "xmm3"}},  // XMM3
  {{"xmm4", "xmm4"}},  // XMM4
  {{"xmm5", "xmm5"}},  // XMM5
  {{"xmm6", "xmm6"}},  // XMM6
  {{"xmm7", "xmm7"}},  // XMM7
  {{"xmm8", "xmm8"}},  // XMM8
  {{"xmm9", "xmm9"}},  // XMM9
  {{"xmm10", "xmm10"}},  // XMM10
  {{"xmm11", "xmm11"}},  // XMM11
  {{"xmm12", "xmm12"}},  // XMM12
  {{"xmm13", "xmm13"}},  // XMM13
  {{"xmm14", "xmm14"}},  // XMM14
  {{"xmm15", "xmm15"}}   // XMM15
}};

X86_64RegisterAllocator::X86_64RegisterAllocator() noexcept {
  Reset();
  
  // システム予約レジスタ
  ReserveGPRegister(X86_64Register::RSP); // スタックポインタは常に予約
}

void X86_64RegisterAllocator::Reset() noexcept {
  m_allocatedGPRegs.reset();
  m_allocatedXMMRegs.reset();
  m_reservedGPRegs.reset();
  m_reservedXMMRegs.reset();
  
  // システム予約レジスタ
  ReserveGPRegister(X86_64Register::RSP); // スタックポインタは常に予約
}

X86_64Register X86_64RegisterAllocator::AllocateGPRegister(X86_64Register preferred) noexcept {
  // 優先レジスタが指定され、割り当て可能であれば使用
  if (preferred != X86_64Register::None && 
      !IsGPRegisterAllocated(preferred) && 
      !IsGPRegisterReserved(preferred)) {
    m_allocatedGPRegs.set(static_cast<size_t>(preferred));
    return preferred;
  }
  
  // 使用可能なレジスタを探す (使用頻度の低いレジスタを優先)
  // R8-R15, RDI, RSI, RDX, RCX, RAX, RBX, RBP の順に割り当て (R12-R15はcallee-saved)
  const std::array<X86_64Register, 15> allocationOrder = {
    X86_64Register::R8, X86_64Register::R9, X86_64Register::R10, X86_64Register::R11,
    X86_64Register::RAX, X86_64Register::RCX, X86_64Register::RDX,
    X86_64Register::RDI, X86_64Register::RSI, 
    X86_64Register::R12, X86_64Register::R13, X86_64Register::R14, X86_64Register::R15,
    X86_64Register::RBX, X86_64Register::RBP
  };
  
  for (auto reg : allocationOrder) {
    if (!IsGPRegisterAllocated(reg) && !IsGPRegisterReserved(reg)) {
      m_allocatedGPRegs.set(static_cast<size_t>(reg));
      return reg;
    }
  }
  
  // 割り当て失敗
  return X86_64Register::None;
}

X86_64XMMRegister X86_64RegisterAllocator::AllocateXMMRegister(X86_64XMMRegister preferred) noexcept {
  // 優先レジスタが指定され、割り当て可能であれば使用
  if (preferred != X86_64XMMRegister::None && 
      !IsXMMRegisterAllocated(preferred) && 
      !IsXMMRegisterReserved(preferred)) {
    m_allocatedXMMRegs.set(static_cast<size_t>(preferred));
    return preferred;
  }
  
  // 使用可能なレジスタを探す (XMM8-XMM15, XMM0-XMM7の順)
  for (uint8_t i = 8; i < 16; ++i) {
    auto reg = static_cast<X86_64XMMRegister>(i);
    if (!IsXMMRegisterAllocated(reg) && !IsXMMRegisterReserved(reg)) {
      m_allocatedXMMRegs.set(i);
      return reg;
    }
  }
  
  for (uint8_t i = 0; i < 8; ++i) {
    auto reg = static_cast<X86_64XMMRegister>(i);
    if (!IsXMMRegisterAllocated(reg) && !IsXMMRegisterReserved(reg)) {
      m_allocatedXMMRegs.set(i);
      return reg;
    }
  }
  
  // 割り当て失敗
  return X86_64XMMRegister::None;
}

void X86_64RegisterAllocator::FreeGPRegister(X86_64Register reg) noexcept {
  if (reg == X86_64Register::None) {
    return;
  }
  
  auto idx = static_cast<size_t>(reg);
  assert(idx < 16);
  m_allocatedGPRegs.reset(idx);
}

void X86_64RegisterAllocator::FreeXMMRegister(X86_64XMMRegister reg) noexcept {
  if (reg == X86_64XMMRegister::None) {
    return;
  }
  
  auto idx = static_cast<size_t>(reg);
  assert(idx < 16);
  m_allocatedXMMRegs.reset(idx);
}

bool X86_64RegisterAllocator::IsGPRegisterAllocated(X86_64Register reg) const noexcept {
  if (reg == X86_64Register::None) {
    return false;
  }
  
  auto idx = static_cast<size_t>(reg);
  assert(idx < 16);
  return m_allocatedGPRegs.test(idx);
}

bool X86_64RegisterAllocator::IsXMMRegisterAllocated(X86_64XMMRegister reg) const noexcept {
  if (reg == X86_64XMMRegister::None) {
    return false;
  }
  
  auto idx = static_cast<size_t>(reg);
  assert(idx < 16);
  return m_allocatedXMMRegs.test(idx);
}

bool X86_64RegisterAllocator::IsGPRegisterReserved(X86_64Register reg) const noexcept {
  if (reg == X86_64Register::None) {
    return false;
  }
  
  auto idx = static_cast<size_t>(reg);
  assert(idx < 16);
  return m_reservedGPRegs.test(idx);
}

bool X86_64RegisterAllocator::IsXMMRegisterReserved(X86_64XMMRegister reg) const noexcept {
  if (reg == X86_64XMMRegister::None) {
    return false;
  }
  
  auto idx = static_cast<size_t>(reg);
  assert(idx < 16);
  return m_reservedXMMRegs.test(idx);
}

void X86_64RegisterAllocator::ReserveGPRegister(X86_64Register reg) noexcept {
  if (reg == X86_64Register::None) {
    return;
  }
  
  auto idx = static_cast<size_t>(reg);
  assert(idx < 16);
  m_reservedGPRegs.set(idx);
}

void X86_64RegisterAllocator::ReserveXMMRegister(X86_64XMMRegister reg) noexcept {
  if (reg == X86_64XMMRegister::None) {
    return;
  }
  
  auto idx = static_cast<size_t>(reg);
  assert(idx < 16);
  m_reservedXMMRegs.set(idx);
}

std::string X86_64RegisterAllocator::GetGPRegisterName(X86_64Register reg, uint8_t size) noexcept {
  if (reg == X86_64Register::None) {
    return "none";
  }
  
  auto idx = static_cast<size_t>(reg);
  assert(idx < 16);
  
  // サイズのインデックスを計算 (0=8bit, 1=16bit, 2=32bit, 3=64bit)
  uint8_t sizeIdx = 0;
  switch (size) {
    case 1: sizeIdx = 0; break;
    case 2: sizeIdx = 1; break;
    case 4: sizeIdx = 2; break;
    case 8: sizeIdx = 3; break;
    default:
      assert(false && "Invalid register size");
      return "???";
  }
  
  return GP_REG_NAMES[idx][sizeIdx];
}

std::string X86_64RegisterAllocator::GetXMMRegisterName(X86_64XMMRegister reg, bool isDouble) noexcept {
  if (reg == X86_64XMMRegister::None) {
    return "none";
  }
  
  auto idx = static_cast<size_t>(reg);
  assert(idx < 16);
  
  // 現在のところ、float/doubleでXMMレジスタ名は同じ
  return XMM_REG_NAMES[idx][isDouble ? 1 : 0];
}

uint16_t X86_64RegisterAllocator::GetCalleeSavedRegistersMask() noexcept {
  // callee-savedレジスタのビットマスク
  // RBX, RBP, R12-R15 (ビット位置 3, 5, 12, 13, 14, 15)
  return (1 << 3) | (1 << 5) | (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15);
}

} // namespace jit
} // namespace core
} // namespace aerojs 