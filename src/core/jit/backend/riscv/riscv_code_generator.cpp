#include "riscv_code_generator.h"

#include <algorithm>

namespace aerojs {
namespace core {

// RISC-Vの主要レジスタ定義
enum RVRegisters {
    X0 = 0,   // ゼロレジスタ (常に0)
    RA = 1,   // 戻りアドレス
    SP = 2,   // スタックポインタ
    GP = 3,   // グローバルポインタ
    TP = 4,   // スレッドポインタ
    T0 = 5,   // 一時レジスタ/代替リンクレジスタ
    T1 = 6,   // 一時レジスタ
    T2 = 7,   // 一時レジスタ
    S0 = 8,   // 保存レジスタ/フレームポインタ
    FP = 8,   // フレームポインタ (S0の別名)
    S1 = 9,   // 保存レジスタ
    A0 = 10,  // 関数引数/戻り値
    A1 = 11,  // 関数引数/戻り値
    A2 = 12,  // 関数引数
    A3 = 13,  // 関数引数
    A4 = 14,  // 関数引数
    A5 = 15,  // 関数引数
    A6 = 16,  // 関数引数
    A7 = 17,  // 関数引数
    S2 = 18,  // 保存レジスタ
    S3 = 19,  // 保存レジスタ
    S4 = 20,  // 保存レジスタ
    S5 = 21,  // 保存レジスタ
    S6 = 22,  // 保存レジスタ
    S7 = 23,  // 保存レジスタ
    S8 = 24,  // 保存レジスタ
    S9 = 25,  // 保存レジスタ
    S10 = 26, // 保存レジスタ
    S11 = 27, // 保存レジスタ
    T3 = 28,  // 一時レジスタ
    T4 = 29,  // 一時レジスタ
    T5 = 30,  // 一時レジスタ
    T6 = 31   // 一時レジスタ
};

// RISC-V命令フォーマット
// R-type: funct7[31:25] rs2[24:20] rs1[19:15] funct3[14:12] rd[11:7] opcode[6:0]
// I-type: imm[31:20] rs1[19:15] funct3[14:12] rd[11:7] opcode[6:0]
// S-type: imm[31:25] rs2[24:20] rs1[19:15] funct3[14:12] imm[11:7] opcode[6:0]
// U-type: imm[31:12] rd[11:7] opcode[6:0]
// J-type: imm[31:12] rd[11:7] opcode[6:0] (with special encoding for imm)

// RISC-Vオペコード
constexpr uint32_t RV_OP        = 0b0110011; // R-type算術命令
constexpr uint32_t RV_OP_IMM    = 0b0010011; // I-type即値算術命令
constexpr uint32_t RV_LOAD      = 0b0000011; // ロード命令
constexpr uint32_t RV_STORE     = 0b0100011; // ストア命令
constexpr uint32_t RV_BRANCH    = 0b1100011; // 分岐命令
constexpr uint32_t RV_JAL       = 0b1101111; // ジャンプ命令
constexpr uint32_t RV_JALR      = 0b1100111; // 間接ジャンプ命令
constexpr uint32_t RV_LUI       = 0b0110111; // 上位即値ロード
constexpr uint32_t RV_AUIPC     = 0b0010111; // PC相対上位即値ロード
constexpr uint32_t RV_OP_32     = 0b0111011; // 32ビット算術命令（RV64）
constexpr uint32_t RV_OP_IMM_32 = 0b0011011; // 32ビット即値算術命令（RV64）

// 関数型定義
using EmitFn = void (*)(const IRInstruction&, std::vector<uint8_t>&);

// RISC-V R-type命令のエンコード
static uint32_t encodeRType(uint32_t funct7, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

// RISC-V I-type命令のエンコード
static uint32_t encodeIType(int32_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
    // 即値は12ビットに制限
    uint32_t imm12 = static_cast<uint32_t>(imm & 0xFFF);
    return (imm12 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

// RISC-V S-type命令のエンコード
static uint32_t encodeSType(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
    // 即値は12ビットに制限し、上位7ビットと下位5ビットに分割
    uint32_t imm11_5 = static_cast<uint32_t>((imm & 0xFE0) >> 5);
    uint32_t imm4_0 = static_cast<uint32_t>(imm & 0x1F);
    return ((imm11_5 & 0x7F) << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | ((imm4_0 & 0x1F) << 7) | opcode;
}

// RISC-V U-type命令のエンコード
static uint32_t encodeUType(int32_t imm, uint32_t rd, uint32_t opcode) {
    // 上位20ビットの即値
    uint32_t imm31_12 = static_cast<uint32_t>(imm & 0xFFFFF000);
    return imm31_12 | (rd << 7) | opcode;
}

// 命令バイトをリトルエンディアンで出力バッファに追加
static void appendInstruction(uint32_t instr, std::vector<uint8_t>& out) {
    out.push_back(instr & 0xFF);
    out.push_back((instr >> 8) & 0xFF);
    out.push_back((instr >> 16) & 0xFF);
    out.push_back((instr >> 24) & 0xFF);
}

// NOP命令の生成（ADDI x0, x0, 0のエイリアス）
static void EmitNop(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
    uint32_t nop = encodeIType(0, X0, 0, X0, RV_OP_IMM);
    appendInstruction(nop, out);
}

// 定数ロード命令の生成
static void EmitLoadConst(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept {
    int64_t val = inst.args.empty() ? 0 : inst.args[0];
    
    if (val == 0) {
        // x0は常に0なので、単純にx0をA0にコピー
        uint32_t addi = encodeIType(0, X0, 0, A0, RV_OP_IMM);
        appendInstruction(addi, out);
    } else if (val >= -2048 && val < 2048) {
        // 12ビットの範囲内ならADDI命令一つで済む
        uint32_t addi = encodeIType(val, X0, 0, A0, RV_OP_IMM);
        appendInstruction(addi, out);
    } else {
        // 大きな値の場合はLUI+ADDIで上位と下位に分割
        int32_t hi = (val + 0x800) & 0xFFFFF000; // 上位20ビット（丸め付き）
        int32_t lo = val - hi;                   // 下位12ビット
        
        // LUI a0, hi
        uint32_t lui = encodeUType(hi, A0, RV_LUI);
        appendInstruction(lui, out);
        
        // ADDI a0, a0, lo
        uint32_t addi = encodeIType(lo, A0, 0, A0, RV_OP_IMM);
        appendInstruction(addi, out);
    }
    
    // スタックに値をプッシュ
    // ADDI sp, sp, -8
    uint32_t addi_sp = encodeIType(-8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp, out);
    
    // SD a0, 0(sp)
    uint32_t sd = encodeSType(0, A0, SP, 3, RV_STORE);
    appendInstruction(sd, out);
}

// 変数ロード命令の生成
static void EmitLoadVar(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept {
    int32_t idx = inst.args.empty() ? 0 : inst.args[0];
    int32_t offset = idx * 8;
    
    // LD a0, offset(fp)
    uint32_t ld = encodeIType(offset, FP, 3, A0, RV_LOAD);
    appendInstruction(ld, out);
    
    // スタックに値をプッシュ
    // ADDI sp, sp, -8
    uint32_t addi_sp = encodeIType(-8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp, out);
    
    // SD a0, 0(sp)
    uint32_t sd = encodeSType(0, A0, SP, 3, RV_STORE);
    appendInstruction(sd, out);
}

// 変数ストア命令の生成
static void EmitStoreVar(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept {
    int32_t idx = inst.args.empty() ? 0 : inst.args[0];
    int32_t offset = idx * 8;
    
    // スタックから値をポップ
    // LD a0, 0(sp)
    uint32_t ld_a0 = encodeIType(0, SP, 3, A0, RV_LOAD);
    appendInstruction(ld_a0, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp = encodeIType(8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp, out);
    
    // 変数に値を格納
    // SD a0, offset(fp)
    uint32_t sd = encodeSType(offset, A0, FP, 3, RV_STORE);
    appendInstruction(sd, out);
}

// 加算命令の生成
static void EmitAdd(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
    // スタックから2つの値をポップ
    // LD a1, 0(sp)
    uint32_t ld_a1 = encodeIType(0, SP, 3, A1, RV_LOAD);
    appendInstruction(ld_a1, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp1 = encodeIType(8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp1, out);
    
    // LD a0, 0(sp)
    uint32_t ld_a0 = encodeIType(0, SP, 3, A0, RV_LOAD);
    appendInstruction(ld_a0, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp2 = encodeIType(8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp2, out);
    
    // 加算実行
    // ADD a0, a0, a1
    uint32_t add = encodeRType(0, A1, A0, 0, A0, RV_OP);
    appendInstruction(add, out);
    
    // 結果をスタックにプッシュ
    // ADDI sp, sp, -8
    uint32_t addi_sp3 = encodeIType(-8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp3, out);
    
    // SD a0, 0(sp)
    uint32_t sd = encodeSType(0, A0, SP, 3, RV_STORE);
    appendInstruction(sd, out);
}

// 減算命令の生成
static void EmitSub(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
    // スタックから2つの値をポップ
    // LD a1, 0(sp)
    uint32_t ld_a1 = encodeIType(0, SP, 3, A1, RV_LOAD);
    appendInstruction(ld_a1, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp1 = encodeIType(8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp1, out);
    
    // LD a0, 0(sp)
    uint32_t ld_a0 = encodeIType(0, SP, 3, A0, RV_LOAD);
    appendInstruction(ld_a0, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp2 = encodeIType(8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp2, out);
    
    // 減算実行 (funct7=0x20はSUB命令を表す)
    // SUB a0, a0, a1
    uint32_t sub = encodeRType(0x20, A1, A0, 0, A0, RV_OP);
    appendInstruction(sub, out);
    
    // 結果をスタックにプッシュ
    // ADDI sp, sp, -8
    uint32_t addi_sp3 = encodeIType(-8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp3, out);
    
    // SD a0, 0(sp)
    uint32_t sd = encodeSType(0, A0, SP, 3, RV_STORE);
    appendInstruction(sd, out);
}

// 乗算命令の生成
static void EmitMul(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
    // スタックから2つの値をポップ
    // LD a1, 0(sp)
    uint32_t ld_a1 = encodeIType(0, SP, 3, A1, RV_LOAD);
    appendInstruction(ld_a1, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp1 = encodeIType(8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp1, out);
    
    // LD a0, 0(sp)
    uint32_t ld_a0 = encodeIType(0, SP, 3, A0, RV_LOAD);
    appendInstruction(ld_a0, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp2 = encodeIType(8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp2, out);
    
    // 乗算実行 (M拡張, funct7=1はMUL命令)
    // MUL a0, a0, a1
    uint32_t mul = encodeRType(1, A1, A0, 0, A0, RV_OP);
    appendInstruction(mul, out);
    
    // 結果をスタックにプッシュ
    // ADDI sp, sp, -8
    uint32_t addi_sp3 = encodeIType(-8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp3, out);
    
    // SD a0, 0(sp)
    uint32_t sd = encodeSType(0, A0, SP, 3, RV_STORE);
    appendInstruction(sd, out);
}

// 除算命令の生成
static void EmitDiv(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
    // スタックから2つの値をポップ
    // LD a1, 0(sp)
    uint32_t ld_a1 = encodeIType(0, SP, 3, A1, RV_LOAD);
    appendInstruction(ld_a1, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp1 = encodeIType(8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp1, out);
    
    // LD a0, 0(sp)
    uint32_t ld_a0 = encodeIType(0, SP, 3, A0, RV_LOAD);
    appendInstruction(ld_a0, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp2 = encodeIType(8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp2, out);
    
    // 除算実行 (M拡張, funct7=1, funct3=4はDIV命令)
    // DIV a0, a0, a1
    uint32_t div = encodeRType(1, A1, A0, 4, A0, RV_OP);
    appendInstruction(div, out);
    
    // 結果をスタックにプッシュ
    // ADDI sp, sp, -8
    uint32_t addi_sp3 = encodeIType(-8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp3, out);
    
    // SD a0, 0(sp)
    uint32_t sd = encodeSType(0, A0, SP, 3, RV_STORE);
    appendInstruction(sd, out);
}

// 関数呼び出し命令の生成
static void EmitCall(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
    // スタックから関数ポインタをポップ
    // LD a5, 0(sp)
    uint32_t ld_a5 = encodeIType(0, SP, 3, A5, RV_LOAD);
    appendInstruction(ld_a5, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp1 = encodeIType(8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp1, out);
    
    // 間接呼び出し
    // JALR ra, 0(a5)
    uint32_t jalr = encodeIType(0, A5, 0, RA, RV_JALR);
    appendInstruction(jalr, out);
    
    // 戻り値をスタックにプッシュ
    // ADDI sp, sp, -8
    uint32_t addi_sp2 = encodeIType(-8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp2, out);
    
    // SD a0, 0(sp)
    uint32_t sd = encodeSType(0, A0, SP, 3, RV_STORE);
    appendInstruction(sd, out);
}

// リターン命令の生成
static void EmitReturn(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
    // スタックから戻り値をポップ
    // LD a0, 0(sp)
    uint32_t ld_a0 = encodeIType(0, SP, 3, A0, RV_LOAD);
    appendInstruction(ld_a0, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp = encodeIType(8, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp, out);
    
    // フレームポインタを復元（エピローグで行う場合もある）
    // LD ra, 8(fp)
    uint32_t ld_ra = encodeIType(8, FP, 3, RA, RV_LOAD);
    appendInstruction(ld_ra, out);
    
    // LD fp, 0(fp)
    uint32_t ld_fp = encodeIType(0, FP, 3, FP, RV_LOAD);
    appendInstruction(ld_fp, out);
    
    // 戻る
    // JALR x0, 0(ra)
    uint32_t ret = encodeIType(0, RA, 0, X0, RV_JALR);
    appendInstruction(ret, out);
}

// 命令ディスパッチテーブル
static constexpr EmitFn kEmitTable[] = {
    EmitNop,        // Nop
    EmitAdd,        // Add
    EmitSub,        // Sub
    EmitMul,        // Mul
    EmitDiv,        // Div
    EmitLoadConst,  // LoadConst
    EmitLoadVar,    // LoadVar
    EmitStoreVar,   // StoreVar
    EmitCall,       // Call
    EmitReturn      // Return
};

void RISCVCodeGenerator::Generate(const IRFunction& ir, std::vector<uint8_t>& outCode) noexcept {
    // プロローグ
    EmitPrologue(outCode);
    
    // 各IR命令を処理
  for (const auto& inst : ir.GetInstructions()) {
        EmitInstruction(inst, outCode);
  }
    
    // エピローグ (Returnが呼ばれなかった場合のため)
    EmitEpilogue(outCode);
}

void RISCVCodeGenerator::EmitPrologue(std::vector<uint8_t>& out) noexcept {
    // フレームのセットアップ
    // フレームポインタ(s0/fp)と戻りアドレス(ra)を保存
    // ADDI sp, sp, -16
    uint32_t addi_sp = encodeIType(-16, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp, out);
    
    // SD ra, 8(sp)
    uint32_t sd_ra = encodeSType(8, RA, SP, 3, RV_STORE);
    appendInstruction(sd_ra, out);
    
    // SD fp, 0(sp)
    uint32_t sd_fp = encodeSType(0, FP, SP, 3, RV_STORE);
    appendInstruction(sd_fp, out);
    
    // ADDI fp, sp, 0
    uint32_t addi_fp = encodeIType(0, SP, 0, FP, RV_OP_IMM);
    appendInstruction(addi_fp, out);
}

void RISCVCodeGenerator::EmitEpilogue(std::vector<uint8_t>& out) noexcept {
    // フレームのクリーンアップ
    // LD ra, 8(fp)
    uint32_t ld_ra = encodeIType(8, FP, 3, RA, RV_LOAD);
    appendInstruction(ld_ra, out);
    
    // LD fp, 0(fp)
    uint32_t ld_fp = encodeIType(0, FP, 3, FP, RV_LOAD);
    appendInstruction(ld_fp, out);
    
    // ADDI sp, fp, 16
    uint32_t addi_sp = encodeIType(16, FP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp, out);
    
    // JALR x0, 0(ra)
    uint32_t ret = encodeIType(0, RA, 0, X0, RV_JALR);
    appendInstruction(ret, out);
}

void RISCVCodeGenerator::EmitInstruction(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept {
    size_t idx = static_cast<size_t>(inst.opcode);
    if (idx < sizeof(kEmitTable) / sizeof(EmitFn)) {
        kEmitTable[idx](inst, out);
    } else {
        EmitNop(inst, out);  // 未知の命令はNOPで代用
    }
}

void RISCVCodeGenerator::EmitLoadImmediate(int reg, int64_t value, std::vector<uint8_t>& out) noexcept {
    if (value == 0) {
        // ADDI reg, x0, 0
        uint32_t addi = encodeIType(0, X0, 0, reg, RV_OP_IMM);
        appendInstruction(addi, out);
    } else if (value >= -2048 && value < 2048) {
        // ADDI reg, x0, value
        uint32_t addi = encodeIType(value, X0, 0, reg, RV_OP_IMM);
        appendInstruction(addi, out);
    } else {
        // LUI+ADDI方式
        int32_t hi = (value + 0x800) & 0xFFFFF000; // 上位20ビット（丸め付き）
        int32_t lo = value - hi;                   // 下位12ビット
        
        // LUI reg, hi
        uint32_t lui = encodeUType(hi, reg, RV_LUI);
        appendInstruction(lui, out);
        
        // ADDI reg, reg, lo
        uint32_t addi = encodeIType(lo, reg, 0, reg, RV_OP_IMM);
        appendInstruction(addi, out);
    }
}

void RISCVCodeGenerator::EmitLoadMemory(int reg, int base, int offset, std::vector<uint8_t>& out) noexcept {
    if (offset >= -2048 && offset < 2048) {
        // LD reg, offset(base)
        uint32_t ld = encodeIType(offset, base, 3, reg, RV_LOAD);
        appendInstruction(ld, out);
    } else {
        // 大きなオフセットは複数命令に分割
        // まず一時レジスタにオフセットをロード
        EmitLoadImmediate(T0, offset, out);
        
        // ADD t0, base, t0
        uint32_t add = encodeRType(0, T0, base, 0, T0, RV_OP);
        appendInstruction(add, out);
        
        // LD reg, 0(t0)
        uint32_t ld = encodeIType(0, T0, 3, reg, RV_LOAD);
        appendInstruction(ld, out);
    }
}

void RISCVCodeGenerator::EmitStoreMemory(int reg, int base, int offset, std::vector<uint8_t>& out) noexcept {
    if (offset >= -2048 && offset < 2048) {
        // SD reg, offset(base)
        uint32_t sd = encodeSType(offset, reg, base, 3, RV_STORE);
        appendInstruction(sd, out);
    } else {
        // 大きなオフセットは複数命令に分割
        // まず一時レジスタにオフセットをロード
        EmitLoadImmediate(T0, offset, out);
        
        // ADD t0, base, t0
        uint32_t add = encodeRType(0, T0, base, 0, T0, RV_OP);
        appendInstruction(add, out);
        
        // SD reg, 0(t0)
        uint32_t sd = encodeSType(0, reg, T0, 3, RV_STORE);
        appendInstruction(sd, out);
    }
}

} // namespace core
} // namespace aerojs