#include "arm64_code_generator.h"

#include <algorithm>

namespace aerojs {
namespace core {

// ARM64の主要レジスタ定義
enum ARM64Registers {
    X0 = 0,    // 第1引数/戻り値
    X1 = 1,    // 第2引数
    X2 = 2,    // 第3引数
    X3 = 3,    // 第4引数
    X4 = 4,    // 第5引数
    X5 = 5,    // 第6引数
    X6 = 6,    // 第7引数
    X7 = 7,    // 第8引数
    X8 = 8,    // インダイレクト結果レジスタ
    X9 = 9,    // 一時レジスタ
    X10 = 10,  // 一時レジスタ
    X11 = 11,  // 一時レジスタ
    X12 = 12,  // 一時レジスタ
    X13 = 13,  // 一時レジスタ
    X14 = 14,  // 一時レジスタ
    X15 = 15,  // 一時レジスタ
    X16 = 16,  // インダイレクトレジスタIP0
    X17 = 17,  // インダイレクトレジスタIP1
    X18 = 18,  // プラットフォーム固有
    X19 = 19,  // 保存レジスタ
    X20 = 20,  // 保存レジスタ
    X21 = 21,  // 保存レジスタ
    X22 = 22,  // 保存レジスタ
    X23 = 23,  // 保存レジスタ
    X24 = 24,  // 保存レジスタ
    X25 = 25,  // 保存レジスタ
    X26 = 26,  // 保存レジスタ
    X27 = 27,  // 保存レジスタ
    X28 = 28,  // 保存レジスタ
    X29 = 29,  // フレームポインタ（FP）
    X30 = 30,  // リンクレジスタ（LR）
    SP = 31    // スタックポインタ
};

// 各Opcode向けのエミッタ関数
using EmitFn = void (*)(const IRInstruction&, std::vector<uint8_t>&);

// NOP命令を生成
static void EmitNop(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
    // NOP (0xD503201F)
    out.push_back(0x1F);
    out.push_back(0x20);
    out.push_back(0x03);
    out.push_back(0xD5);
}

// 定数ロード命令を生成
static void EmitLoadConst(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept {
    int64_t val = inst.args.empty() ? 0 : inst.args[0];
    
    // MOV X0, #val - 単純な即値の場合
    if (val >= 0 && val < 0xFFFF) {
        // MOV X0, #val
        uint32_t instr = 0xD2800000 | (val << 5) | X0;
        out.push_back(instr & 0xFF);
        out.push_back((instr >> 8) & 0xFF);
        out.push_back((instr >> 16) & 0xFF);
        out.push_back((instr >> 24) & 0xFF);
    } else {
        // 大きな値の場合は複数命令に分割
        // MOVZ X0, #(val & 0xFFFF)
        uint32_t instr1 = 0xD2800000 | ((val & 0xFFFF) << 5) | X0;
        out.push_back(instr1 & 0xFF);
        out.push_back((instr1 >> 8) & 0xFF);
        out.push_back((instr1 >> 16) & 0xFF);
        out.push_back((instr1 >> 24) & 0xFF);
        
        // MOVK X0, #((val >> 16) & 0xFFFF), LSL #16
        if (val >= 0x10000) {
            uint32_t instr2 = 0xF2A00000 | (((val >> 16) & 0xFFFF) << 5) | X0;
            out.push_back(instr2 & 0xFF);
            out.push_back((instr2 >> 8) & 0xFF);
            out.push_back((instr2 >> 16) & 0xFF);
            out.push_back((instr2 >> 24) & 0xFF);
        }
    }
    
    // スタックにプッシュ (STR X0, [SP, #-16]!)
    uint32_t str_instr = 0xF81F0FE0;
    out.push_back(str_instr & 0xFF);
    out.push_back((str_instr >> 8) & 0xFF);
    out.push_back((str_instr >> 16) & 0xFF);
    out.push_back((str_instr >> 24) & 0xFF);
}

// 変数ロード命令を生成
static void EmitLoadVar(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept {
    int32_t idx = inst.args.empty() ? 0 : inst.args[0];
    int32_t offset = idx * 8;
    
    // LDR X0, [X29, #offset]
    uint32_t instr = 0xF9400000 | ((offset / 8) << 10) | (X29 << 5) | X0;
    out.push_back(instr & 0xFF);
    out.push_back((instr >> 8) & 0xFF);
    out.push_back((instr >> 16) & 0xFF);
    out.push_back((instr >> 24) & 0xFF);
    
    // スタックにプッシュ (STR X0, [SP, #-16]!)
    uint32_t str_instr = 0xF81F0FE0;
    out.push_back(str_instr & 0xFF);
    out.push_back((str_instr >> 8) & 0xFF);
    out.push_back((str_instr >> 16) & 0xFF);
    out.push_back((str_instr >> 24) & 0xFF);
}

// 変数ストア命令を生成
static void EmitStoreVar(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept {
    int32_t idx = inst.args.empty() ? 0 : inst.args[0];
    int32_t offset = idx * 8;
    
    // スタックからポップ (LDR X0, [SP], #16)
    uint32_t ldr_instr = 0xF84107E0;
    out.push_back(ldr_instr & 0xFF);
    out.push_back((ldr_instr >> 8) & 0xFF);
    out.push_back((ldr_instr >> 16) & 0xFF);
    out.push_back((ldr_instr >> 24) & 0xFF);
    
    // STR X0, [X29, #offset]
    uint32_t instr = 0xF9000000 | ((offset / 8) << 10) | (X29 << 5) | X0;
    out.push_back(instr & 0xFF);
    out.push_back((instr >> 8) & 0xFF);
    out.push_back((instr >> 16) & 0xFF);
    out.push_back((instr >> 24) & 0xFF);
}

// 加算命令を生成
static void EmitAdd(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
    // スタックから2つの値をポップ
    // LDR X1, [SP], #16
    uint32_t ldr_instr1 = 0xF84107E1;
    out.push_back(ldr_instr1 & 0xFF);
    out.push_back((ldr_instr1 >> 8) & 0xFF);
    out.push_back((ldr_instr1 >> 16) & 0xFF);
    out.push_back((ldr_instr1 >> 24) & 0xFF);
    
    // LDR X0, [SP], #16
    uint32_t ldr_instr2 = 0xF84107E0;
    out.push_back(ldr_instr2 & 0xFF);
    out.push_back((ldr_instr2 >> 8) & 0xFF);
    out.push_back((ldr_instr2 >> 16) & 0xFF);
    out.push_back((ldr_instr2 >> 24) & 0xFF);
    
    // ADD X0, X0, X1
    uint32_t add_instr = 0x8B010000;
    out.push_back(add_instr & 0xFF);
    out.push_back((add_instr >> 8) & 0xFF);
    out.push_back((add_instr >> 16) & 0xFF);
    out.push_back((add_instr >> 24) & 0xFF);
    
    // 結果をスタックにプッシュ (STR X0, [SP, #-16]!)
    uint32_t str_instr = 0xF81F0FE0;
    out.push_back(str_instr & 0xFF);
    out.push_back((str_instr >> 8) & 0xFF);
    out.push_back((str_instr >> 16) & 0xFF);
    out.push_back((str_instr >> 24) & 0xFF);
}

// 減算命令を生成
static void EmitSub(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
    // スタックから2つの値をポップ
    // LDR X1, [SP], #16
    uint32_t ldr_instr1 = 0xF84107E1;
    out.push_back(ldr_instr1 & 0xFF);
    out.push_back((ldr_instr1 >> 8) & 0xFF);
    out.push_back((ldr_instr1 >> 16) & 0xFF);
    out.push_back((ldr_instr1 >> 24) & 0xFF);
    
    // LDR X0, [SP], #16
    uint32_t ldr_instr2 = 0xF84107E0;
    out.push_back(ldr_instr2 & 0xFF);
    out.push_back((ldr_instr2 >> 8) & 0xFF);
    out.push_back((ldr_instr2 >> 16) & 0xFF);
    out.push_back((ldr_instr2 >> 24) & 0xFF);
    
    // SUB X0, X0, X1
    uint32_t sub_instr = 0xCB010000;
    out.push_back(sub_instr & 0xFF);
    out.push_back((sub_instr >> 8) & 0xFF);
    out.push_back((sub_instr >> 16) & 0xFF);
    out.push_back((sub_instr >> 24) & 0xFF);
    
    // 結果をスタックにプッシュ (STR X0, [SP, #-16]!)
    uint32_t str_instr = 0xF81F0FE0;
    out.push_back(str_instr & 0xFF);
    out.push_back((str_instr >> 8) & 0xFF);
    out.push_back((str_instr >> 16) & 0xFF);
    out.push_back((str_instr >> 24) & 0xFF);
}

// 乗算命令を生成
static void EmitMul(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
    // スタックから2つの値をポップ
    // LDR X1, [SP], #16
    uint32_t ldr_instr1 = 0xF84107E1;
    out.push_back(ldr_instr1 & 0xFF);
    out.push_back((ldr_instr1 >> 8) & 0xFF);
    out.push_back((ldr_instr1 >> 16) & 0xFF);
    out.push_back((ldr_instr1 >> 24) & 0xFF);
    
    // LDR X0, [SP], #16
    uint32_t ldr_instr2 = 0xF84107E0;
    out.push_back(ldr_instr2 & 0xFF);
    out.push_back((ldr_instr2 >> 8) & 0xFF);
    out.push_back((ldr_instr2 >> 16) & 0xFF);
    out.push_back((ldr_instr2 >> 24) & 0xFF);
    
    // MUL X0, X0, X1
    uint32_t mul_instr = 0x9B007C00;
    out.push_back(mul_instr & 0xFF);
    out.push_back((mul_instr >> 8) & 0xFF);
    out.push_back((mul_instr >> 16) & 0xFF);
    out.push_back((mul_instr >> 24) & 0xFF);
    
    // 結果をスタックにプッシュ (STR X0, [SP, #-16]!)
    uint32_t str_instr = 0xF81F0FE0;
    out.push_back(str_instr & 0xFF);
    out.push_back((str_instr >> 8) & 0xFF);
    out.push_back((str_instr >> 16) & 0xFF);
    out.push_back((str_instr >> 24) & 0xFF);
}

// 除算命令を生成
static void EmitDiv(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
    // スタックから2つの値をポップ
    // LDR X1, [SP], #16
    uint32_t ldr_instr1 = 0xF84107E1;
    out.push_back(ldr_instr1 & 0xFF);
    out.push_back((ldr_instr1 >> 8) & 0xFF);
    out.push_back((ldr_instr1 >> 16) & 0xFF);
    out.push_back((ldr_instr1 >> 24) & 0xFF);
    
    // LDR X0, [SP], #16
    uint32_t ldr_instr2 = 0xF84107E0;
    out.push_back(ldr_instr2 & 0xFF);
    out.push_back((ldr_instr2 >> 8) & 0xFF);
    out.push_back((ldr_instr2 >> 16) & 0xFF);
    out.push_back((ldr_instr2 >> 24) & 0xFF);
    
    // SDIV X0, X0, X1
    uint32_t div_instr = 0x9AC00C00;
    out.push_back(div_instr & 0xFF);
    out.push_back((div_instr >> 8) & 0xFF);
    out.push_back((div_instr >> 16) & 0xFF);
    out.push_back((div_instr >> 24) & 0xFF);
    
    // 結果をスタックにプッシュ (STR X0, [SP, #-16]!)
    uint32_t str_instr = 0xF81F0FE0;
    out.push_back(str_instr & 0xFF);
    out.push_back((str_instr >> 8) & 0xFF);
    out.push_back((str_instr >> 16) & 0xFF);
    out.push_back((str_instr >> 24) & 0xFF);
}

// 関数呼び出し命令を生成
static void EmitCall(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
    // スタックからアドレスをポップ
    // LDR X0, [SP], #16
    uint32_t ldr_instr = 0xF84107E0;
    out.push_back(ldr_instr & 0xFF);
    out.push_back((ldr_instr >> 8) & 0xFF);
    out.push_back((ldr_instr >> 16) & 0xFF);
    out.push_back((ldr_instr >> 24) & 0xFF);
    
    // BLR X0 (間接呼び出し)
    uint32_t blr_instr = 0xD63F0000;
    out.push_back(blr_instr & 0xFF);
    out.push_back((blr_instr >> 8) & 0xFF);
    out.push_back((blr_instr >> 16) & 0xFF);
    out.push_back((blr_instr >> 24) & 0xFF);
    
    // 戻り値をスタックにプッシュ (STR X0, [SP, #-16]!)
    uint32_t str_instr = 0xF81F0FE0;
    out.push_back(str_instr & 0xFF);
    out.push_back((str_instr >> 8) & 0xFF);
    out.push_back((str_instr >> 16) & 0xFF);
    out.push_back((str_instr >> 24) & 0xFF);
}

// リターン命令を生成
static void EmitReturn(const IRInstruction&, std::vector<uint8_t>& out) noexcept {
    // スタックから戻り値をポップ
    // LDR X0, [SP], #16
    uint32_t ldr_instr = 0xF84107E0;
    out.push_back(ldr_instr & 0xFF);
    out.push_back((ldr_instr >> 8) & 0xFF);
    out.push_back((ldr_instr >> 16) & 0xFF);
    out.push_back((ldr_instr >> 24) & 0xFF);
    
    // フレームを解放して戻る
    // RET (= RET LR = RET X30)
    uint32_t ret_instr = 0xD65F03C0;
    out.push_back(ret_instr & 0xFF);
    out.push_back((ret_instr >> 8) & 0xFF);
    out.push_back((ret_instr >> 16) & 0xFF);
    out.push_back((ret_instr >> 24) & 0xFF);
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

void ARM64CodeGenerator::Generate(const IRFunction& ir, std::vector<uint8_t>& outCode) noexcept {
    // プロローグ
    EmitPrologue(outCode);
    
    // IR 命令ごとにエミット
    for (const auto& inst : ir.GetInstructions()) {
        EmitInstruction(inst, outCode);
    }
    
    // エピローグ
    EmitEpilogue(outCode);
}

void ARM64CodeGenerator::EmitPrologue(std::vector<uint8_t>& out) noexcept {
    // フレーム設定
    // STP X29, X30, [SP, #-16]!
    uint32_t stp_instr = 0xA9BF7BFD;
    out.push_back(stp_instr & 0xFF);
    out.push_back((stp_instr >> 8) & 0xFF);
    out.push_back((stp_instr >> 16) & 0xFF);
    out.push_back((stp_instr >> 24) & 0xFF);
    
    // MOV X29, SP
    uint32_t mov_instr = 0x910003FD;
    out.push_back(mov_instr & 0xFF);
    out.push_back((mov_instr >> 8) & 0xFF);
    out.push_back((mov_instr >> 16) & 0xFF);
    out.push_back((mov_instr >> 24) & 0xFF);
}

void ARM64CodeGenerator::EmitEpilogue(std::vector<uint8_t>& out) noexcept {
    // フレーム解放
    // LDP X29, X30, [SP], #16
    uint32_t ldp_instr = 0xA8C17BFD;
    out.push_back(ldp_instr & 0xFF);
    out.push_back((ldp_instr >> 8) & 0xFF);
    out.push_back((ldp_instr >> 16) & 0xFF);
    out.push_back((ldp_instr >> 24) & 0xFF);
    
    // RET
    uint32_t ret_instr = 0xD65F03C0;
    out.push_back(ret_instr & 0xFF);
    out.push_back((ret_instr >> 8) & 0xFF);
    out.push_back((ret_instr >> 16) & 0xFF);
    out.push_back((ret_instr >> 24) & 0xFF);
}

void ARM64CodeGenerator::EmitInstruction(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept {
    size_t idx = static_cast<size_t>(inst.opcode);
    if (idx < sizeof(kEmitTable) / sizeof(EmitFn)) {
        kEmitTable[idx](inst, out);
    } else {
        EmitNop(inst, out);  // 未知の命令はNOPに置き換え
    }
}

void ARM64CodeGenerator::EmitLoadImmediate(int reg, int64_t value, std::vector<uint8_t>& out) noexcept {
    if (value >= 0 && value < 0xFFFF) {
        // MOV Xreg, #val (小さな値)
        uint32_t mov_instr = 0xD2800000 | ((value & 0xFFFF) << 5) | reg;
        out.push_back(mov_instr & 0xFF);
        out.push_back((mov_instr >> 8) & 0xFF);
        out.push_back((mov_instr >> 16) & 0xFF);
        out.push_back((mov_instr >> 24) & 0xFF);
    } else {
        // 大きな値の場合は複数命令に分割
        // MOVZ Xreg, #(val & 0xFFFF)
        uint32_t movz_instr = 0xD2800000 | ((value & 0xFFFF) << 5) | reg;
        out.push_back(movz_instr & 0xFF);
        out.push_back((movz_instr >> 8) & 0xFF);
        out.push_back((movz_instr >> 16) & 0xFF);
        out.push_back((movz_instr >> 24) & 0xFF);
        
        // MOVK Xreg, #((val >> 16) & 0xFFFF), LSL #16
        if ((value >> 16) != 0) {
            uint32_t movk_instr = 0xF2A00000 | (((value >> 16) & 0xFFFF) << 5) | reg;
            out.push_back(movk_instr & 0xFF);
            out.push_back((movk_instr >> 8) & 0xFF);
            out.push_back((movk_instr >> 16) & 0xFF);
            out.push_back((movk_instr >> 24) & 0xFF);
        }
        
        // MOVK Xreg, #((val >> 32) & 0xFFFF), LSL #32
        if ((value >> 32) != 0) {
            uint32_t movk_instr = 0xF2C00000 | (((value >> 32) & 0xFFFF) << 5) | reg;
            out.push_back(movk_instr & 0xFF);
            out.push_back((movk_instr >> 8) & 0xFF);
            out.push_back((movk_instr >> 16) & 0xFF);
            out.push_back((movk_instr >> 24) & 0xFF);
        }
        
        // MOVK Xreg, #((val >> 48) & 0xFFFF), LSL #48
        if ((value >> 48) != 0) {
            uint32_t movk_instr = 0xF2E00000 | (((value >> 48) & 0xFFFF) << 5) | reg;
            out.push_back(movk_instr & 0xFF);
            out.push_back((movk_instr >> 8) & 0xFF);
            out.push_back((movk_instr >> 16) & 0xFF);
            out.push_back((movk_instr >> 24) & 0xFF);
        }
    }
}

void ARM64CodeGenerator::EmitLoadMemory(int reg, int base, int offset, std::vector<uint8_t>& out) noexcept {
    // オフセットが12ビットに収まる場合
    if (offset >= 0 && offset < 4096) {
        // LDR Xreg, [Xbase, #offset]
        uint32_t ldr_instr = 0xF9400000 | ((offset / 8) << 10) | (base << 5) | reg;
        out.push_back(ldr_instr & 0xFF);
        out.push_back((ldr_instr >> 8) & 0xFF);
        out.push_back((ldr_instr >> 16) & 0xFF);
        out.push_back((ldr_instr >> 24) & 0xFF);
    } else {
        // 大きなオフセットの場合は、一時レジスタを使用
        // MOV X9, #offset
        EmitLoadImmediate(X9, offset, out);
        
        // LDR Xreg, [Xbase, X9]
        uint32_t ldr_instr = 0xF8696800 | (base << 5) | reg;
        out.push_back(ldr_instr & 0xFF);
        out.push_back((ldr_instr >> 8) & 0xFF);
        out.push_back((ldr_instr >> 16) & 0xFF);
        out.push_back((ldr_instr >> 24) & 0xFF);
    }
}

void ARM64CodeGenerator::EmitStoreMemory(int reg, int base, int offset, std::vector<uint8_t>& out) noexcept {
    // オフセットが12ビットに収まる場合
    if (offset >= 0 && offset < 4096) {
        // STR Xreg, [Xbase, #offset]
        uint32_t str_instr = 0xF9000000 | ((offset / 8) << 10) | (base << 5) | reg;
        out.push_back(str_instr & 0xFF);
        out.push_back((str_instr >> 8) & 0xFF);
        out.push_back((str_instr >> 16) & 0xFF);
        out.push_back((str_instr >> 24) & 0xFF);
    } else {
        // 大きなオフセットの場合は、一時レジスタを使用
        // MOV X9, #offset
        EmitLoadImmediate(X9, offset, out);
        
        // STR Xreg, [Xbase, X9]
        uint32_t str_instr = 0xF8296800 | (base << 5) | reg;
        out.push_back(str_instr & 0xFF);
        out.push_back((str_instr >> 8) & 0xFF);
        out.push_back((str_instr >> 16) & 0xFF);
        out.push_back((str_instr >> 24) & 0xFF);
    }
}

} // namespace core
} // namespace aerojs