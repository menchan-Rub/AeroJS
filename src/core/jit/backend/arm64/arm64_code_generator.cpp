#include "arm64_code_generator.h"

#include <asmjit/asmjit.h>

namespace aerojs {
namespace core {

void ARM64CodeGenerator::Generate(const IRFunction& ir, std::vector<uint8_t>& outCode) noexcept {
  using namespace asmjit;
  using namespace asmjit::aarch64;

  // CodeHolder とアセンブラを初期化
  CodeHolder code;
  code.init(CodeInfo(ArchInfo::kIdAarch64));
  aarch64::Assembler a(&code);

  // プロローグ: push x29, x30; mov x29, sp; sub sp, sp, #16
  a.stp(x29, x30, Mem(sp, -16, 8));
  a.mov(x29, sp);
  a.sub(sp, sp, Imm(16));

  // Stack-machine: stack grows downward by 8 bytes per slot
  for (const auto& inst : ir.GetInstructions()) {
    switch (inst.opcode) {
      case Opcode::LoadConst: {
        int64_t val = inst.args.empty() ? 0 : inst.args[0];
        a.mov(x0, Imm(val));
        // push x0
        a.sub(sp, sp, Imm(8));
        a.str(x0, Mem(sp, 0, 8));
        break;
      }
      case Opcode::LoadVar: {
        int32_t idx = inst.args.empty() ? 0 : inst.args[0];
        int32_t disp = -16 - idx * 8;
        a.ldr(x0, Mem(x29, disp, 8));
        a.sub(sp, sp, Imm(8));
        a.str(x0, Mem(sp, 0, 8));
        break;
      }
      case Opcode::StoreVar: {
        int32_t idx = inst.args.empty() ? 0 : inst.args[0];
        int32_t disp = -16 - idx * 8;
        a.ldr(x0, Mem(sp, 0, 8));
        a.add(sp, sp, Imm(8));
        a.str(x0, Mem(x29, disp, 8));
        break;
      }
      case Opcode::Add:
      case Opcode::Sub:
      case Opcode::Mul:
      case Opcode::Div: {
        // pop into x1
        a.ldr(x1, Mem(sp, 0, 8));
        a.add(sp, sp, Imm(8));
        // pop into x0
        a.ldr(x0, Mem(sp, 0, 8));
        a.add(sp, sp, Imm(8));
        switch (inst.opcode) {
          case Opcode::Add:
            a.add(x0, x0, x1);
            break;
          case Opcode::Sub:
            a.sub(x0, x0, x1);
            break;
          case Opcode::Mul:
            a.mul(x0, x0, x1);
            break;
          case Opcode::Div:
            a.sdiv(x0, x0, x1);
            break;
          default:
            break;
        }
        // push x0
        a.sub(sp, sp, Imm(8));
        a.str(x0, Mem(sp, 0, 8));
        break;
      }
      case Opcode::Call: {
        // pop fn ptr into x0
        a.ldr(x0, Mem(sp, 0, 8));
        a.add(sp, sp, Imm(8));
        a.blr(x0);  // call x0
        // push return x0
        a.sub(sp, sp, Imm(8));
        a.str(x0, Mem(sp, 0, 8));
        break;
      }
      case Opcode::Return: {
        // pop return value into x0
        a.ldr(x0, Mem(sp, 0, 8));
        a.add(sp, sp, Imm(8));
        // epilogue
        a.add(sp, sp, Imm(16));
        a.ldp(x29, x30, Mem(sp, 0, 8));
        a.ret();
        break;
      }
      case Opcode::Nop:
      default:
        a.nop();
        break;
    }
  }

  // エピローグ (Return が最終命令で呼ばれる想定だが念のため)
  a.add(sp, sp, Imm(16));
  a.ldp(x29, x30, Mem(sp, 0, 8));
  a.ret();

  // コードバッファを取得
  const CodeBuffer& buf = code.sectionById(0)->buffer();
  outCode.insert(outCode.end(), buf.data(), buf.data() + buf.size());
}

}  // namespace core
}  // namespace aerojs