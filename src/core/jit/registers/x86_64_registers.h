#pragma once

#include <array>
#include <string>
#include <vector>
#include "register_allocator.h"

namespace aerojs {
namespace core {

// x86_64の汎用レジスタ
enum class X86_64Register : uint8_t {
    RAX = 0,  // 戻り値レジスタ
    RCX = 1,  // 4番目の引数、カウンタレジスタ
    RDX = 2,  // 3番目の引数、2番目の戻り値レジスタ
    RBX = 3,  // ベースレジスタ（callee-saved）
    RSP = 4,  // スタックポインタ（予約済み）
    RBP = 5,  // フレームポインタ（callee-saved、予約済み）
    RSI = 6,  // 2番目の引数、ソースインデックス
    RDI = 7,  // 1番目の引数、デスティネーションインデックス
    R8  = 8,  // 5番目の引数
    R9  = 9,  // 6番目の引数
    R10 = 10, // 一時レジスタ
    R11 = 11, // 一時レジスタ
    R12 = 12, // callee-saved
    R13 = 13, // callee-saved
    R14 = 14, // callee-saved
    R15 = 15, // callee-saved
    
    // 特別なレジスタ識別子
    NONE = 0xFF
};

// x86_64の浮動小数点レジスタ（SSE/AVX）
enum class X86_64FloatRegister : uint8_t {
    XMM0 = 0,   // 浮動小数点引数1、戻り値
    XMM1 = 1,   // 浮動小数点引数2
    XMM2 = 2,   // 浮動小数点引数3
    XMM3 = 3,   // 浮動小数点引数4
    XMM4 = 4,   // 浮動小数点引数5
    XMM5 = 5,   // 浮動小数点引数6
    XMM6 = 6,   // 浮動小数点引数7
    XMM7 = 7,   // 浮動小数点引数8
    XMM8 = 8,   // 一時レジスタ
    XMM9 = 9,   // 一時レジスタ
    XMM10 = 10, // 一時レジスタ
    XMM11 = 11, // 一時レジスタ
    XMM12 = 12, // 一時レジスタ
    XMM13 = 13, // 一時レジスタ
    XMM14 = 14, // 一時レジスタ
    XMM15 = 15, // 一時レジスタ
    
    // 特別なレジスタ識別子
    NONE = 0xFF
};

// x86_64レジスタに関する定数と関数を提供するクラス
class X86_64Registers {
public:
    // 汎用レジスタの名前を取得
    static const char* GetRegisterName(X86_64Register reg) {
        static const std::array<const char*, 16> names = {
            "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
            "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
        };
        
        if (reg == X86_64Register::NONE || static_cast<uint8_t>(reg) >= names.size()) {
            return "none";
        }
        
        return names[static_cast<uint8_t>(reg)];
    }
    
    // 浮動小数点レジスタの名前を取得
    static const char* GetRegisterName(X86_64FloatRegister reg) {
        static const std::array<const char*, 16> names = {
            "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
            "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"
        };
        
        if (reg == X86_64FloatRegister::NONE || static_cast<uint8_t>(reg) >= names.size()) {
            return "none";
        }
        
        return names[static_cast<uint8_t>(reg)];
    }
    
    // 32ビットモードでのレジスタ名を取得
    static const char* GetRegister32Name(X86_64Register reg) {
        static const std::array<const char*, 16> names = {
            "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
            "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
        };
        
        if (reg == X86_64Register::NONE || static_cast<uint8_t>(reg) >= names.size()) {
            return "none";
        }
        
        return names[static_cast<uint8_t>(reg)];
    }
    
    // 16ビットモードでのレジスタ名を取得
    static const char* GetRegister16Name(X86_64Register reg) {
        static const std::array<const char*, 16> names = {
            "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
            "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"
        };
        
        if (reg == X86_64Register::NONE || static_cast<uint8_t>(reg) >= names.size()) {
            return "none";
        }
        
        return names[static_cast<uint8_t>(reg)];
    }
    
    // 8ビットモードでのレジスタ名を取得
    static const char* GetRegister8Name(X86_64Register reg) {
        static const std::array<const char*, 16> names = {
            "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
            "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"
        };
        
        if (reg == X86_64Register::NONE || static_cast<uint8_t>(reg) >= names.size()) {
            return "none";
        }
        
        return names[static_cast<uint8_t>(reg)];
    }
    
    // 呼び出し側保存レジスタかどうかを確認
    static bool IsCallerSaved(X86_64Register reg) {
        switch (reg) {
            case X86_64Register::RAX:
            case X86_64Register::RCX:
            case X86_64Register::RDX:
            case X86_64Register::RSI:
            case X86_64Register::RDI:
            case X86_64Register::R8:
            case X86_64Register::R9:
            case X86_64Register::R10:
            case X86_64Register::R11:
                return true;
            default:
                return false;
        }
    }
    
    // 呼び出し先保存レジスタかどうかを確認
    static bool IsCalleeSaved(X86_64Register reg) {
        switch (reg) {
            case X86_64Register::RBX:
            case X86_64Register::RBP:
            case X86_64Register::R12:
            case X86_64Register::R13:
            case X86_64Register::R14:
            case X86_64Register::R15:
                return true;
            default:
                return false;
        }
    }
    
    // 予約済みレジスタかどうかを確認
    static bool IsReserved(X86_64Register reg) {
        switch (reg) {
            case X86_64Register::RSP:
            case X86_64Register::RBP:
                return true;
            default:
                return false;
        }
    }
    
    // 引数レジスタかどうかを確認
    static bool IsArgRegister(X86_64Register reg) {
        switch (reg) {
            case X86_64Register::RDI:  // 1番目の引数
            case X86_64Register::RSI:  // 2番目の引数
            case X86_64Register::RDX:  // 3番目の引数
            case X86_64Register::RCX:  // 4番目の引数
            case X86_64Register::R8:   // 5番目の引数
            case X86_64Register::R9:   // 6番目の引数
                return true;
            default:
                return false;
        }
    }
    
    // n番目の引数レジスタを取得
    static X86_64Register GetArgRegister(uint8_t index) {
        static const std::array<X86_64Register, 6> argRegs = {
            X86_64Register::RDI,  // 1番目の引数
            X86_64Register::RSI,  // 2番目の引数
            X86_64Register::RDX,  // 3番目の引数
            X86_64Register::RCX,  // 4番目の引数
            X86_64Register::R8,   // 5番目の引数
            X86_64Register::R9    // 6番目の引数
        };
        
        if (index < argRegs.size()) {
            return argRegs[index];
        }
        
        return X86_64Register::NONE;
    }
    
    // n番目の浮動小数点引数レジスタを取得
    static X86_64FloatRegister GetFloatArgRegister(uint8_t index) {
        if (index < 8) {
            return static_cast<X86_64FloatRegister>(index);
        }
        
        return X86_64FloatRegister::NONE;
    }
    
    // 一般的な割り当て可能レジスタのリストを取得
    static std::vector<X86_64Register> GetAllocatableRegisters() {
        return {
            X86_64Register::RAX,
            X86_64Register::RCX,
            X86_64Register::RDX,
            X86_64Register::RBX,
            X86_64Register::RSI,
            X86_64Register::RDI,
            X86_64Register::R8,
            X86_64Register::R9,
            X86_64Register::R10,
            X86_64Register::R11,
            X86_64Register::R12,
            X86_64Register::R13,
            X86_64Register::R14,
            X86_64Register::R15
        };
    }
    
    // レジスタアロケータ用の物理レジスタ定義を取得
    static std::vector<PhysicalRegister> GetPhysicalRegisters() {
        std::vector<PhysicalRegister> result;
        
        // 汎用レジスタの登録
        for (uint8_t i = 0; i < 16; i++) {
            X86_64Register reg = static_cast<X86_64Register>(i);
            if (reg == X86_64Register::RSP) {
                // スタックポインタは予約済み
                result.emplace_back(i, RegisterType::GPR, RegisterClass::INT64, false, true);
            } else if (reg == X86_64Register::RBP) {
                // フレームポインタも予約済み
                result.emplace_back(i, RegisterType::GPR, RegisterClass::INT64, true, true);
            } else {
                result.emplace_back(i, RegisterType::GPR, RegisterClass::INT64, IsCalleeSaved(reg), false);
            }
        }
        
        // 浮動小数点レジスタの登録
        for (uint8_t i = 0; i < 16; i++) {
            // x86_64では全ての浮動小数点レジスタは呼び出し側保存
            result.emplace_back(i + 16, RegisterType::FPR, RegisterClass::FLOAT64, false, false);
        }
        
        return result;
    }
};

}  // namespace core
}  // namespace aerojs 