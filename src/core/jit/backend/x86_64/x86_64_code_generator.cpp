#include "x86_64_code_generator.h"
#include <cassert>
#include <cstring>

namespace aerojs {
namespace core {

X86_64CodeGenerator::X86_64CodeGenerator() noexcept {
}

X86_64CodeGenerator::~X86_64CodeGenerator() noexcept {
}

bool X86_64CodeGenerator::Generate(const IRFunction& function, std::vector<uint8_t>& outCode) noexcept {
    // 出力バッファのサイズを事前に確保（命令あたり平均10バイトと仮定）
    outCode.reserve(function.GetInstructionCount() * 10);
    
    // プロローグコードを生成
    EncodePrologue(outCode);
    
    // 各IR命令をx86_64コードに変換
    const auto& instructions = function.GetInstructions();
    for (const auto& inst : instructions) {
        switch (inst.opcode) {
            case Opcode::kNop:
                // NOPは何もしない
                break;
            case Opcode::kLoadConst:
                EncodeLoadConst(inst, outCode);
                break;
            case Opcode::kMove:
                EncodeMove(inst, outCode);
                break;
            case Opcode::kAdd:
            case Opcode::kSub:
            case Opcode::kMul:
            case Opcode::kDiv:
            case Opcode::kMod:
                // 算術演算
                switch (inst.opcode) {
                    case Opcode::kAdd: EncodeAdd(inst, outCode); break;
                    case Opcode::kSub: EncodeSub(inst, outCode); break;
                    case Opcode::kMul: EncodeMul(inst, outCode); break;
                    case Opcode::kDiv: EncodeDiv(inst, outCode); break;
                    case Opcode::kMod: EncodeMod(inst, outCode); break;
                    default: break;
                }
                break;
            case Opcode::kNeg:
                EncodeNeg(inst, outCode);
                break;
            case Opcode::kCompareEq:
            case Opcode::kCompareNe:
            case Opcode::kCompareLt:
            case Opcode::kCompareLe:
            case Opcode::kCompareGt:
            case Opcode::kCompareGe:
                // 比較演算
                EncodeCompare(inst, outCode);
                break;
            case Opcode::kAnd:
            case Opcode::kOr:
            case Opcode::kNot:
                // 論理演算
                EncodeLogical(inst, outCode);
                break;
            case Opcode::kBitAnd:
            case Opcode::kBitOr:
            case Opcode::kBitXor:
            case Opcode::kBitNot:
            case Opcode::kShiftLeft:
            case Opcode::kShiftRight:
                // ビット演算
                EncodeBitwise(inst, outCode);
                break;
            case Opcode::kJump:
            case Opcode::kJumpIfTrue:
            case Opcode::kJumpIfFalse:
                // ジャンプ命令
                EncodeJump(inst, outCode);
                break;
            case Opcode::kCall:
                // 関数呼び出し
                EncodeCall(inst, outCode);
                break;
            case Opcode::kReturn:
                // 関数からの戻り
                EncodeReturn(inst, outCode);
                break;
            case Opcode::kSIMDLoad:
                // SIMD読み込み - CPU機能に基づいて最適な実装を使用
                if (HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX512)) {
                    EncodeAVX512Load(inst, outCode);
                } else {
                    EncodeSIMDLoad(inst, outCode);
                }
                break;
            case Opcode::kSIMDStore:
                // SIMD書き込み - CPU機能に基づいて最適な実装を使用
                if (HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX512)) {
                    EncodeAVX512Store(inst, outCode);
                } else {
                    EncodeSIMDStore(inst, outCode);
                }
                break;
            case Opcode::kSIMDArithmetic:
                // SIMD算術演算 - CPU機能に基づいて最適な実装を使用
                if (HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX512)) {
                    EncodeAVX512Arithmetic(inst, outCode);
                } else {
                    EncodeSIMDArithmetic(inst, outCode);
                }
                break;
            case Opcode::kFMA:
                // FMA演算 - CPU機能に基づいて最適な実装を使用
                if (HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX512) && 
                    HasFlag(m_optimizationFlags, CodeGenOptFlags::UseFMA)) {
                    EncodeAVX512FMA(inst, outCode);
                } else {
                    EncodeFMA(inst, outCode);
                }
                break;
            case Opcode::kFastMath:
                EncodeFastMath(inst, outCode);
                break;
            case Opcode::kAVX512Compress:
                EncodeAVX512Compress(inst, outCode);
                break;
            case Opcode::kAVX512Expand:
                EncodeAVX512Expand(inst, outCode);
                break;
            case Opcode::kAVX512Blend:
                EncodeAVX512Blend(inst, outCode);
                break;
            case Opcode::kAVX512Permute:
                EncodeAVX512Permute(inst, outCode);
                break;
            default:
                // 未サポートの命令
                // エラーログを出力などの処理を入れるとよい
                return false;
        }
    }
    
    // キャッシュライン最適化を適用（オプション）
    if (HasFlag(m_optimizationFlags, CodeGenOptFlags::CacheAware)) {
        OptimizeForCacheLine(outCode);
    }
    
    // エピローグコードを生成
    EncodeEpilogue(outCode);
    
    return true;
}

void X86_64CodeGenerator::SetRegisterMapping(int32_t virtualReg, X86_64Register physicalReg) noexcept {
    m_registerMapping[virtualReg] = physicalReg;
}

X86_64Register X86_64CodeGenerator::GetPhysicalReg(int32_t virtualReg) const noexcept {
    auto it = m_registerMapping.find(virtualReg);
    if (it != m_registerMapping.end()) {
        return it->second;
    }
    
    // マッピングがない場合は、仮想レジスタ番号に基づいて物理レジスタを推測
    // この実装はとても単純で、実際には適切なレジスタアロケーションが必要
    int32_t regIndex = virtualReg % 14; // RSP, RBPを除く14個のGPRに対応
    
    // RSP, RBPを避けるための調整
    if (regIndex >= 4) regIndex += 1;
    if (regIndex >= 5) regIndex += 1;
    
    return static_cast<X86_64Register>(regIndex);
}

// ヘルパー関数
void X86_64CodeGenerator::AppendImmediate32(std::vector<uint8_t>& code, int32_t value) noexcept {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);
    for (int i = 0; i < 4; ++i) {
        code.push_back(bytes[i]);
    }
}

void X86_64CodeGenerator::AppendImmediate64(std::vector<uint8_t>& code, int64_t value) noexcept {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);
    for (int i = 0; i < 8; ++i) {
        code.push_back(bytes[i]);
    }
}

void X86_64CodeGenerator::AppendREXPrefix(std::vector<uint8_t>& code, bool w, bool r, bool x, bool b) noexcept {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08; // REX.W = 1: 64-bit operand
    if (r) rex |= 0x04; // REX.R = 1: Extension of the ModR/M reg field
    if (x) rex |= 0x02; // REX.X = 1: Extension of the SIB index field
    if (b) rex |= 0x01; // REX.B = 1: Extension of the ModR/M r/m field, SIB base, or Opcode reg
    
    code.push_back(rex);
}

void X86_64CodeGenerator::AppendModRM(std::vector<uint8_t>& code, uint8_t mod, uint8_t reg, uint8_t rm) noexcept {
    // mod(2) | reg(3) | rm(3)
    uint8_t modrm = (mod << 6) | ((reg & 0x7) << 3) | (rm & 0x7);
    code.push_back(modrm);
}

// 基本命令エンコード実装
void X86_64CodeGenerator::EncodeMove(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 2) return;
    
    int32_t destReg = inst.args[0];
    int32_t srcReg = inst.args[1];
    
    X86_64Register physDest = GetPhysicalReg(destReg);
    X86_64Register physSrc = GetPhysicalReg(srcReg);
    
    // MOV dst, src
    // REXプレフィックス: W=1 (64-bit), R=拡張destReg, B=拡張srcReg
    bool rExt = static_cast<uint8_t>(physDest) >= 8;
    bool bExt = static_cast<uint8_t>(physSrc) >= 8;
    AppendREXPrefix(code, true, rExt, false, bExt);
    
    code.push_back(0x89); // MOV r/m64, r64
    
    // ModR/Mバイト: mod=11 (レジスタ直接), reg=srcReg, r/m=destReg
    AppendModRM(code, 0b11, static_cast<uint8_t>(physSrc) & 0x7, static_cast<uint8_t>(physDest) & 0x7);
}

void X86_64CodeGenerator::EncodeLoadConst(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 2) return;
    
    int32_t destReg = inst.args[0];
    int32_t constValue = inst.args[1];
    
    X86_64Register physDest = GetPhysicalReg(destReg);
    
    // MOV dst, imm32
    // REXプレフィックス: W=1 (64-bit), B=拡張destReg
    bool bExt = static_cast<uint8_t>(physDest) >= 8;
    AppendREXPrefix(code, true, false, false, bExt);
    
    // オペコード: 0xB8 + レジスタ番号 (MOV r64, imm64)
    code.push_back(0xB8 + (static_cast<uint8_t>(physDest) & 0x7));
    
    // 64ビット即値 (64ビットモードでも32ビット即値でゼロ拡張される)
    AppendImmediate32(code, constValue);
    AppendImmediate32(code, 0); // 上位32ビットはゼロ
}

void X86_64CodeGenerator::EncodeAdd(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t destReg = inst.args[0];
    int32_t leftReg = inst.args[1];
    int32_t rightReg = inst.args[2];
    
    X86_64Register physDest = GetPhysicalReg(destReg);
    X86_64Register physLeft = GetPhysicalReg(leftReg);
    X86_64Register physRight = GetPhysicalReg(rightReg);
    
    // destとleftが異なる場合、まずleftをdestにコピー
    if (physDest != physLeft) {
        // MOV dest, left
        bool rExt = static_cast<uint8_t>(physLeft) >= 8;
        bool bExt = static_cast<uint8_t>(physDest) >= 8;
        AppendREXPrefix(code, true, rExt, false, bExt);
        code.push_back(0x89); // MOV r/m64, r64
        AppendModRM(code, 0b11, static_cast<uint8_t>(physLeft) & 0x7, static_cast<uint8_t>(physDest) & 0x7);
    }
    
    // ADD dest, right
    bool rExt = static_cast<uint8_t>(physRight) >= 8;
    bool bExt = static_cast<uint8_t>(physDest) >= 8;
    AppendREXPrefix(code, true, rExt, false, bExt);
    code.push_back(0x01); // ADD r/m64, r64
    AppendModRM(code, 0b11, static_cast<uint8_t>(physRight) & 0x7, static_cast<uint8_t>(physDest) & 0x7);
}

void X86_64CodeGenerator::EncodeSub(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t destReg = inst.args[0];
    int32_t leftReg = inst.args[1];
    int32_t rightReg = inst.args[2];
    
    X86_64Register physDest = GetPhysicalReg(destReg);
    X86_64Register physLeft = GetPhysicalReg(leftReg);
    X86_64Register physRight = GetPhysicalReg(rightReg);
    
    // destとleftが異なる場合、まずleftをdestにコピー
    if (physDest != physLeft) {
        // MOV dest, left
        bool rExt = static_cast<uint8_t>(physLeft) >= 8;
        bool bExt = static_cast<uint8_t>(physDest) >= 8;
        AppendREXPrefix(code, true, rExt, false, bExt);
        code.push_back(0x89); // MOV r/m64, r64
        AppendModRM(code, 0b11, static_cast<uint8_t>(physLeft) & 0x7, static_cast<uint8_t>(physDest) & 0x7);
    }
    
    // SUB dest, right
    bool rExt = static_cast<uint8_t>(physRight) >= 8;
    bool bExt = static_cast<uint8_t>(physDest) >= 8;
    AppendREXPrefix(code, true, rExt, false, bExt);
    code.push_back(0x29); // SUB r/m64, r64
    AppendModRM(code, 0b11, static_cast<uint8_t>(physRight) & 0x7, static_cast<uint8_t>(physDest) & 0x7);
}

void X86_64CodeGenerator::EncodeMul(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t destReg = inst.args[0];
    int32_t leftReg = inst.args[1];
    int32_t rightReg = inst.args[2];
    
    X86_64Register physDest = GetPhysicalReg(destReg);
    X86_64Register physLeft = GetPhysicalReg(leftReg);
    X86_64Register physRight = GetPhysicalReg(rightReg);
    
    // destとleftが異なる場合、まずleftをdestにコピー
    if (physDest != physLeft) {
        // MOV dest, left
        bool rExt = static_cast<uint8_t>(physLeft) >= 8;
        bool bExt = static_cast<uint8_t>(physDest) >= 8;
        AppendREXPrefix(code, true, rExt, false, bExt);
        code.push_back(0x89); // MOV r/m64, r64
        AppendModRM(code, 0b11, static_cast<uint8_t>(physLeft) & 0x7, static_cast<uint8_t>(physDest) & 0x7);
    }
    
    // IMUL dest, right
    bool rExt = static_cast<uint8_t>(physDest) >= 8;
    bool bExt = static_cast<uint8_t>(physRight) >= 8;
    AppendREXPrefix(code, true, rExt, false, bExt);
    code.push_back(0x0F);
    code.push_back(0xAF); // IMUL r64, r/m64
    AppendModRM(code, 0b11, static_cast<uint8_t>(physDest) & 0x7, static_cast<uint8_t>(physRight) & 0x7);
}

void X86_64CodeGenerator::EncodeDiv(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t destReg = inst.args[0];
    int32_t leftReg = inst.args[1];
    int32_t rightReg = inst.args[2];
    
    X86_64Register physDest = GetPhysicalReg(destReg);
    X86_64Register physLeft = GetPhysicalReg(leftReg);
    X86_64Register physRight = GetPhysicalReg(rightReg);
    
    // 除算は複雑なので、まずRAXにleftを、RDXを0にセットし、
    // IDIVで割り、結果をdestに移動する必要がある
    
    // MOV RAX, left
    bool bExt = static_cast<uint8_t>(physLeft) >= 8;
    AppendREXPrefix(code, true, false, false, bExt);
    code.push_back(0x89); // MOV r/m64, r64
    AppendModRM(code, 0b11, static_cast<uint8_t>(physLeft) & 0x7, 0); // RAX = 0
    
    // XOR RDX, RDX (RDXをゼロクリア)
    AppendREXPrefix(code, true, false, false, false);
    code.push_back(0x31); // XOR r/m64, r64
    AppendModRM(code, 0b11, 2, 2); // RDX = 2
    
    // IDIV right
    bExt = static_cast<uint8_t>(physRight) >= 8;
    AppendREXPrefix(code, true, false, false, bExt);
    code.push_back(0xF7); // IDIV r/m64
    AppendModRM(code, 0b11, 7, static_cast<uint8_t>(physRight) & 0x7);
    
    // 商はRAXに入っているので、これをdestに移動（RAXでない場合）
    if (physDest != X86_64Register::RAX) {
        bool bExt = static_cast<uint8_t>(physDest) >= 8;
        AppendREXPrefix(code, true, false, false, bExt);
        code.push_back(0x89); // MOV r/m64, r64
        AppendModRM(code, 0b11, 0, static_cast<uint8_t>(physDest) & 0x7);
    }
}

void X86_64CodeGenerator::EncodeMod(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t destReg = inst.args[0];
    int32_t leftReg = inst.args[1];
    int32_t rightReg = inst.args[2];
    
    X86_64Register physDest = GetPhysicalReg(destReg);
    X86_64Register physLeft = GetPhysicalReg(leftReg);
    X86_64Register physRight = GetPhysicalReg(rightReg);
    
    // 剰余も除算と同様に複雑
    
    // MOV RAX, left
    bool bExt = static_cast<uint8_t>(physLeft) >= 8;
    AppendREXPrefix(code, true, false, false, bExt);
    code.push_back(0x89); // MOV r/m64, r64
    AppendModRM(code, 0b11, static_cast<uint8_t>(physLeft) & 0x7, 0); // RAX = 0
    
    // XOR RDX, RDX (RDXをゼロクリア)
    AppendREXPrefix(code, true, false, false, false);
    code.push_back(0x31); // XOR r/m64, r64
    AppendModRM(code, 0b11, 2, 2); // RDX = 2
    
    // IDIV right
    bExt = static_cast<uint8_t>(physRight) >= 8;
    AppendREXPrefix(code, true, false, false, bExt);
    code.push_back(0xF7); // IDIV r/m64
    AppendModRM(code, 0b11, 7, static_cast<uint8_t>(physRight) & 0x7);
    
    // 剰余はRDXに入っているので、これをdestに移動（RDXでない場合）
    if (physDest != X86_64Register::RDX) {
        bool bExt = static_cast<uint8_t>(physDest) >= 8;
        AppendREXPrefix(code, true, false, false, bExt);
        code.push_back(0x89); // MOV r/m64, r64
        AppendModRM(code, 0b11, 2, static_cast<uint8_t>(physDest) & 0x7);
    }
}

void X86_64CodeGenerator::EncodeNeg(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 2) return;
    
    int32_t destReg = inst.args[0];
    int32_t srcReg = inst.args[1];
    
    X86_64Register physDest = GetPhysicalReg(destReg);
    X86_64Register physSrc = GetPhysicalReg(srcReg);
    
    // destとsrcが異なる場合、まずsrcをdestにコピー
    if (physDest != physSrc) {
        // MOV dest, src
        bool rExt = static_cast<uint8_t>(physSrc) >= 8;
        bool bExt = static_cast<uint8_t>(physDest) >= 8;
        AppendREXPrefix(code, true, rExt, false, bExt);
        code.push_back(0x89); // MOV r/m64, r64
        AppendModRM(code, 0b11, static_cast<uint8_t>(physSrc) & 0x7, static_cast<uint8_t>(physDest) & 0x7);
    }
    
    // NEG dest
    bool bExt = static_cast<uint8_t>(physDest) >= 8;
    AppendREXPrefix(code, true, false, false, bExt);
    code.push_back(0xF7); // NEG r/m64
    AppendModRM(code, 0b11, 3, static_cast<uint8_t>(physDest) & 0x7);
}

// 実際の実装ではもっと多くの命令を実装する必要があります
// この例では基本的な算術演算のみ実装しています

void X86_64CodeGenerator::EncodeCompare(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // 比較命令の実装
    // 簡略化のため、実装は省略
}

void X86_64CodeGenerator::EncodeLogical(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // 論理演算の実装
    // 簡略化のため、実装は省略
}

void X86_64CodeGenerator::EncodeBitwise(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // ビット演算の実装
    // 簡略化のため、実装は省略
}

void X86_64CodeGenerator::EncodeJump(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // ジャンプ命令の実装
    // 簡略化のため、実装は省略
}

void X86_64CodeGenerator::EncodeCall(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // 関数呼び出しの実装
    // 簡略化のため、実装は省略
}

void X86_64CodeGenerator::EncodeReturn(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    // 関数リターンの実装
    // 簡略化のため、実装は省略
}

// SIMD操作
void X86_64CodeGenerator::EncodeSIMDLoad(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t destReg = inst.args[0];
    int32_t addrReg = inst.args[1];
    int32_t offset = inst.args[2];
    
    auto simdReg = GetSIMDReg(destReg);
    if (!simdReg.has_value()) return;
    
    X86_64Register addrRegPhysical = GetPhysicalReg(addrReg);
    
    // AVX使用可能時に最適化
    bool useAVX = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX);
    
    if (useAVX) {
        // VEX.256.0F.WIG 10 /r VMOVUPS ymm1, m256
        // 16バイトアラインメント済みのデータに対してはVMOVAPS (aligned)が高速
        AppendVEXPrefix(code, 0b01, 0b00, 1, 0, 0b1111, 0, 0, 
                         static_cast<uint8_t>(addrRegPhysical) >= 8);
        code.push_back(0x10); // MOVUPS opcode
        
        // ModRM: [addrReg + offset]
        if (offset == 0) {
            // レジスタ間接アドレッシング
            AppendModRM(code, 0b00, static_cast<uint8_t>(*simdReg) & 0x7, 
                        static_cast<uint8_t>(addrRegPhysical) & 0x7);
        } else if (offset >= -128 && offset <= 127) {
            // 1バイト変位付きアドレッシング
            AppendModRM(code, 0b01, static_cast<uint8_t>(*simdReg) & 0x7, 
                        static_cast<uint8_t>(addrRegPhysical) & 0x7);
            code.push_back(static_cast<uint8_t>(offset));
        } else {
            // 4バイト変位付きアドレッシング
            AppendModRM(code, 0b10, static_cast<uint8_t>(*simdReg) & 0x7, 
                        static_cast<uint8_t>(addrRegPhysical) & 0x7);
            AppendImmediate32(code, offset);
        }
    } else {
        // 従来のSSE命令を使用
        bool rExt = static_cast<uint8_t>(*simdReg) >= 8;
        bool bExt = static_cast<uint8_t>(addrRegPhysical) >= 8;
        
        // REX.R = SIMD Reg extension, REX.B = Base address reg extension
        AppendREXPrefix(code, false, rExt, false, bExt);
        
        code.push_back(0x0F); // Two-byte opcode prefix
        code.push_back(0x10); // MOVUPS opcode
        
        // ModRM: [addrReg + offset]
        if (offset == 0) {
            AppendModRM(code, 0b00, static_cast<uint8_t>(*simdReg) & 0x7, 
                      static_cast<uint8_t>(addrRegPhysical) & 0x7);
        } else if (offset >= -128 && offset <= 127) {
            AppendModRM(code, 0b01, static_cast<uint8_t>(*simdReg) & 0x7, 
                      static_cast<uint8_t>(addrRegPhysical) & 0x7);
            code.push_back(static_cast<uint8_t>(offset));
        } else {
            AppendModRM(code, 0b10, static_cast<uint8_t>(*simdReg) & 0x7, 
                      static_cast<uint8_t>(addrRegPhysical) & 0x7);
            AppendImmediate32(code, offset);
        }
    }
}

void X86_64CodeGenerator::EncodeSIMDStore(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t srcReg = inst.args[0];
    int32_t addrReg = inst.args[1];
    int32_t offset = inst.args[2];
    
    auto simdReg = GetSIMDReg(srcReg);
    if (!simdReg.has_value()) return;
    
    X86_64Register addrRegPhysical = GetPhysicalReg(addrReg);
    
    // AVX使用可能時に最適化
    bool useAVX = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX);
    
    if (useAVX) {
        // VEX.256.0F.WIG 11 /r VMOVUPS m256, ymm1
        AppendVEXPrefix(code, 0b01, 0b00, 1, 0, 0b1111, 0, 0, 
                       static_cast<uint8_t>(addrRegPhysical) >= 8);
        code.push_back(0x11); // MOVUPS opcode (store)
        
        // ModRM: [addrReg + offset]
        if (offset == 0) {
            AppendModRM(code, 0b00, static_cast<uint8_t>(*simdReg) & 0x7, 
                      static_cast<uint8_t>(addrRegPhysical) & 0x7);
        } else if (offset >= -128 && offset <= 127) {
            AppendModRM(code, 0b01, static_cast<uint8_t>(*simdReg) & 0x7, 
                      static_cast<uint8_t>(addrRegPhysical) & 0x7);
            code.push_back(static_cast<uint8_t>(offset));
        } else {
            AppendModRM(code, 0b10, static_cast<uint8_t>(*simdReg) & 0x7, 
                      static_cast<uint8_t>(addrRegPhysical) & 0x7);
            AppendImmediate32(code, offset);
        }
    } else {
        // 従来のSSE命令を使用
        bool rExt = static_cast<uint8_t>(*simdReg) >= 8;
        bool bExt = static_cast<uint8_t>(addrRegPhysical) >= 8;
        
        AppendREXPrefix(code, false, rExt, false, bExt);
        
        code.push_back(0x0F); // Two-byte opcode prefix
        code.push_back(0x11); // MOVUPS opcode (store)
        
        // ModRM: [addrReg + offset]
        if (offset == 0) {
            AppendModRM(code, 0b00, static_cast<uint8_t>(*simdReg) & 0x7, 
                      static_cast<uint8_t>(addrRegPhysical) & 0x7);
        } else if (offset >= -128 && offset <= 127) {
            AppendModRM(code, 0b01, static_cast<uint8_t>(*simdReg) & 0x7, 
                      static_cast<uint8_t>(addrRegPhysical) & 0x7);
            code.push_back(static_cast<uint8_t>(offset));
        } else {
            AppendModRM(code, 0b10, static_cast<uint8_t>(*simdReg) & 0x7, 
                      static_cast<uint8_t>(addrRegPhysical) & 0x7);
            AppendImmediate32(code, offset);
        }
    }
}

void X86_64CodeGenerator::EncodeSIMDArithmetic(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t destReg = inst.args[0];
    int32_t src1Reg = inst.args[1];
    int32_t src2Reg = inst.args[2];
    
    auto destSIMD = GetSIMDReg(destReg);
    auto src1SIMD = GetSIMDReg(src1Reg);
    auto src2SIMD = GetSIMDReg(src2Reg);
    
    if (!destSIMD.has_value() || !src1SIMD.has_value() || !src2SIMD.has_value()) return;
    
    bool useAVX = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX);
    bool useFMA = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseFMA);
    
    uint8_t opcode = 0;
    uint8_t prefix = 0;
    
    // 命令タイプを決定
    switch (inst.opcode) {
        case Opcode::kSIMDAdd:
            opcode = 0x58; // ADDPS
            break;
        case Opcode::kSIMDSub:
            opcode = 0x5C; // SUBPS
            break;
        case Opcode::kSIMDMul:
            opcode = 0x59; // MULPS
            break;
        case Opcode::kSIMDDiv:
            opcode = 0x5E; // DIVPS
            break;
        case Opcode::kSIMDMin:
            opcode = 0x5D; // MINPS
            break;
        case Opcode::kSIMDMax:
            opcode = 0x5F; // MAXPS
            break;
        case Opcode::kSIMDAnd:
            opcode = 0x54; // ANDPS
            break;
        case Opcode::kSIMDOr:
            opcode = 0x56; // ORPS
            break;
        case Opcode::kSIMDXor:
            opcode = 0x57; // XORPS
            break;
        default:
            return; // サポートされていない操作
    }
    
    if (useAVX) {
        // AVX命令は3オペランド形式をサポート: dest = src1 op src2
        // VEX.256.0F.WIG opcode /r VADDPS ymm1, ymm2, ymm3/m256
        AppendVEXPrefix(code, 0b01, 0b00, 1, 0,
                        static_cast<uint8_t>(*src1SIMD) ^ 0xF, // VEX.vvvv = src1 (反転)
                        static_cast<uint8_t>(*destSIMD) >= 8,
                        false,
                        static_cast<uint8_t>(*src2SIMD) >= 8);
        
        code.push_back(opcode);
        
        // ModRM
        AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                    static_cast<uint8_t>(*src2SIMD) & 0x7);
    } else {
        // 従来のSSE命令は2オペランド形式: dest = dest op src
        // まずsrc1をdestにコピーする必要がある場合
        if (*destSIMD != *src1SIMD) {
            // MOVAPS dest, src1
            bool rExt = static_cast<uint8_t>(*destSIMD) >= 8;
            bool bExt = static_cast<uint8_t>(*src1SIMD) >= 8;
            
            AppendREXPrefix(code, false, rExt, false, bExt);
            
            code.push_back(0x0F);
            code.push_back(0x28); // MOVAPS
            
            AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                        static_cast<uint8_t>(*src1SIMD) & 0x7);
        }
        
        // 演算を実行: dest = dest op src2
        bool rExt = static_cast<uint8_t>(*destSIMD) >= 8;
        bool bExt = static_cast<uint8_t>(*src2SIMD) >= 8;
        
        AppendREXPrefix(code, false, rExt, false, bExt);
        
        code.push_back(0x0F);
        code.push_back(opcode);
        
        AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                    static_cast<uint8_t>(*src2SIMD) & 0x7);
    }
}

void X86_64CodeGenerator::EncodeFMA(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 4) return;
    
    int32_t destReg = inst.args[0];
    int32_t src1Reg = inst.args[1];
    int32_t src2Reg = inst.args[2];
    int32_t src3Reg = inst.args[3];
    
    auto destSIMD = GetSIMDReg(destReg);
    auto src1SIMD = GetSIMDReg(src1Reg);
    auto src2SIMD = GetSIMDReg(src2Reg);
    auto src3SIMD = GetSIMDReg(src3Reg);
    
    if (!destSIMD.has_value() || !src1SIMD.has_value() || 
        !src2SIMD.has_value() || !src3SIMD.has_value()) return;
    
    bool useFMA = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseFMA);
    if (!useFMA) {
        // FMA命令をサポートしていない場合、通常の乗算と加算に分解
        // dest = src1 * src2
        IRInstruction mulInst;
        mulInst.opcode = Opcode::kSIMDMul;
        mulInst.args.push_back(destReg);
        mulInst.args.push_back(src1Reg);
        mulInst.args.push_back(src2Reg);
        
        // dest = dest + src3
        IRInstruction addInst;
        addInst.opcode = Opcode::kSIMDAdd;
        addInst.args.push_back(destReg);
        addInst.args.push_back(destReg);
        addInst.args.push_back(src3Reg);
        
        EncodeSIMDArithmetic(mulInst, code);
        EncodeSIMDArithmetic(addInst, code);
        return;
    }
    
    // VFMADD132PS/VFMADD213PS/VFMADD231PS
    // 様々なオペランド順序に対応するFMA命令をサポート
    
    // VFMADD213PS: dest = src2 * dest + src3
    // VEX.DDS.256.66.0F38.W0 A8 /r VFMADD213PS ymm1, ymm2, ymm3/m256
    AppendVEXPrefix(code, 0b10, 0b01, 1, 0,
                    static_cast<uint8_t>(*src2SIMD) ^ 0xF, // VEX.vvvv = src2 (反転)
                    static_cast<uint8_t>(*destSIMD) >= 8,
                    false,
                    static_cast<uint8_t>(*src3SIMD) >= 8);
    
    code.push_back(0xA8); // VFMADD213PS opcode
    
    // ModRM
    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                static_cast<uint8_t>(*src3SIMD) & 0x7);
}

// VEXプレフィックスの追加ヘルパー
void X86_64CodeGenerator::AppendVEXPrefix(std::vector<uint8_t>& code, uint8_t m, uint8_t p, uint8_t l, uint8_t w, uint8_t vvvv, uint8_t r, uint8_t x, uint8_t b) noexcept {
    // m: 0F, 0F38, 0F3A のいずれか
    // p: none, 66, F3, F2 のいずれか
    // l: vector length (0 = 128-bit, 1 = 256-bit)
    // w: 64-bit operand
    // vvvv: 高次のSSEレジスタ指定（反転）
    // r, x, b: REXプレフィックスのビット
    
    if (r || x || b || m != 1) {
        // 3バイトVEXプレフィックス
        code.push_back(0xC4);
        
        // RXB反転ビットとmapSelect
        code.push_back((~r & 0x1) << 7 | (~x & 0x1) << 6 | (~b & 0x1) << 5 | (m & 0x1F));
        
        // W, vvvv反転, L, pp
        code.push_back((w & 0x1) << 7 | ((~vvvv) & 0xF) << 3 | (l & 0x1) << 2 | (p & 0x3));
    } else {
        // 2バイトVEXプレフィックス（0Fマッピングのみ）
        code.push_back(0xC5);
        
        // R反転ビット, vvvv反転, L, pp
        code.push_back((~r & 0x1) << 7 | ((~vvvv) & 0xF) << 3 | (l & 0x1) << 2 | (p & 0x3));
    }
}

// SIBバイトの追加ヘルパー
void X86_64CodeGenerator::AppendSIB(std::vector<uint8_t>& code, uint8_t scale, uint8_t index, uint8_t base) noexcept {
    // scale: 0=1, 1=2, 2=4, 3=8
    // index: インデックスレジスタ（0-7）
    // base: ベースレジスタ（0-7）
    
    uint8_t sib = (scale & 0x3) << 6 | (index & 0x7) << 3 | (base & 0x7);
    code.push_back(sib);
}

// SIMDレジスタの取得
std::optional<X86_64FloatRegister> X86_64CodeGenerator::GetSIMDReg(int32_t virtualReg) const noexcept {
    auto it = m_simdRegisterMapping.find(virtualReg);
    if (it != m_simdRegisterMapping.end()) {
        return it->second;
    }
    
    // 仮想レジスタ番号に基づいて物理SIMDレジスタを推測
    // 実際にはレジスタアロケータによって適切に割り当てられるべき
    int32_t regIndex = virtualReg % 16; // 16個のXMMレジスタを想定
    
    return static_cast<X86_64FloatRegister>(regIndex);
}

// キャッシュライン最適化
void X86_64CodeGenerator::OptimizeForCacheLine(std::vector<uint8_t>& code) noexcept {
    if (!HasFlag(m_optimizationFlags, CodeGenOptFlags::CacheAware)) {
        return;
    }
    
    // 64バイトのキャッシュラインを仮定
    const size_t cacheLine = 64;
    
    // 1. ホットコードパスのアライメント
    // ジャンプターゲットやループ開始位置をキャッシュライン境界にアライン
    std::vector<size_t> hotSpots;
    
    // ジャンプターゲットを特定 (単純実装)
    for (size_t i = 0; i < code.size() - 5; ++i) {
        // 無条件ジャンプまたは条件付きジャンプの命令を検出
        if (code[i] == 0xE9 || // JMP rel32
            (code[i] == 0x0F && (code[i+1] >= 0x80 && code[i+1] <= 0x8F))) { // Jcc rel32
            
            // ジャンプターゲットを計算
            int32_t offset;
            if (code[i] == 0xE9) {
                offset = *reinterpret_cast<int32_t*>(&code[i+1]);
                size_t target = i + 5 + offset;
                hotSpots.push_back(target);
            } else {
                offset = *reinterpret_cast<int32_t*>(&code[i+2]);
                size_t target = i + 6 + offset;
                hotSpots.push_back(target);
            }
        }
    }
    
    // ホットスポットをキャッシュライン境界にアライン
    std::vector<uint8_t> optimizedCode;
    optimizedCode.reserve(code.size() + hotSpots.size() * 64); // 最大で各ホットスポットに64バイトのパディングが必要
    
    size_t currentPos = 0;
    for (size_t i = 0; i < code.size(); ++i) {
        // ホットスポットの位置か確認
        auto it = std::find(hotSpots.begin(), hotSpots.end(), i);
        if (it != hotSpots.end()) {
            // キャッシュライン境界までパディングを追加
            size_t currentOffset = optimizedCode.size() % cacheLine;
            if (currentOffset != 0) {
                size_t paddingNeeded = cacheLine - currentOffset;
                
                // NOPによるパディング (0x90)
                // 長いNOPシーケンスは複数バイトNOPを使用
                while (paddingNeeded > 0) {
                    if (paddingNeeded >= 9) {
                        // 9バイトNOP: 66 0F 1F 84 00 00 00 00 00
                        optimizedCode.push_back(0x66);
                        optimizedCode.push_back(0x0F);
                        optimizedCode.push_back(0x1F);
                        optimizedCode.push_back(0x84);
                        optimizedCode.push_back(0x00);
                        optimizedCode.push_back(0x00);
                        optimizedCode.push_back(0x00);
                        optimizedCode.push_back(0x00);
                        optimizedCode.push_back(0x00);
                        paddingNeeded -= 9;
                    } else if (paddingNeeded >= 7) {
                        // 7バイトNOP: 0F 1F 80 00 00 00 00
                        optimizedCode.push_back(0x0F);
                        optimizedCode.push_back(0x1F);
                        optimizedCode.push_back(0x80);
                        optimizedCode.push_back(0x00);
                        optimizedCode.push_back(0x00);
                        optimizedCode.push_back(0x00);
                        optimizedCode.push_back(0x00);
                        paddingNeeded -= 7;
                    } else if (paddingNeeded >= 4) {
                        // 4バイトNOP: 0F 1F 40 00
                        optimizedCode.push_back(0x0F);
                        optimizedCode.push_back(0x1F);
                        optimizedCode.push_back(0x40);
                        optimizedCode.push_back(0x00);
                        paddingNeeded -= 4;
                    } else if (paddingNeeded >= 3) {
                        // 3バイトNOP: 0F 1F 00
                        optimizedCode.push_back(0x0F);
                        optimizedCode.push_back(0x1F);
                        optimizedCode.push_back(0x00);
                        paddingNeeded -= 3;
                    } else if (paddingNeeded >= 2) {
                        // 2バイトNOP: 66 90
                        optimizedCode.push_back(0x66);
                        optimizedCode.push_back(0x90);
                        paddingNeeded -= 2;
                    } else {
                        // 1バイトNOP: 90
                        optimizedCode.push_back(0x90);
                        paddingNeeded -= 1;
                    }
                }
            }
        }
        
        // 現在のバイトを追加
        optimizedCode.push_back(code[i]);
        currentPos++;
    }
    
    // 2. 関数間の間隔を調整
    // 複数の関数を隣接させる場合、キャッシュライン境界で分離
    
    // 最適化したコードで元のコードを置き換え
    code.swap(optimizedCode);
}

// 高速数学関数の特殊ケース
void X86_64CodeGenerator::EncodeFastMath(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 2) return;
    
    int32_t destReg = inst.args[0];
    int32_t srcReg = inst.args[1];
    
    X86_64Register physDest = GetPhysicalReg(destReg);
    X86_64Register physSrc = GetPhysicalReg(srcReg);
    
    // SSEが利用可能な場合は、SIMD命令を使用して高速化
    auto destSIMD = GetSIMDReg(destReg);
    auto srcSIMD = GetSIMDReg(srcReg);
    
    bool hasSIMD = destSIMD.has_value() && srcSIMD.has_value();
    bool useAVX = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX);
    
    switch (inst.opcode) {
        case Opcode::kFastInvSqrt: {
            // 高速な逆平方根近似 (Q_rsqrt from Quake III)
            // float Q_rsqrt(float number) {
            //     long i;
            //     float x2, y;
            //     const float threehalfs = 1.5F;
            //
            //     x2 = number * 0.5F;
            //     y = number;
            //     i = *(long*)&y;
            //     i = 0x5f3759df - (i >> 1);
            //     y = *(float*)&i;
            //     y = y * (threehalfs - (x2 * y * y));
            //     return y;
            // }

            // SIMD版 (SSE/AVX)
            if (hasSIMD) {
                if (useAVX) {
                    // AVX最適化版
                    // VMOVSS xmm0, xmm0, [srcSIMD]
                    AppendVEXPrefix(code, 0b01, 0b11, 0, 0, 0xF, 0, 0, 
                                  static_cast<uint8_t>(*srcSIMD) >= 8);
                    code.push_back(0x10);
                    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                               static_cast<uint8_t>(*srcSIMD) & 0x7);
                    
                    // 定数0.5をロード
                    // VMOVSS xmm1, [0.5]
                    // 定数格納領域へのポインタを取得...
                    
                    // VMULSS xmm1, xmm1, xmm0
                    AppendVEXPrefix(code, 0b01, 0b11, 0, 0, 0xF, 0, 0, 0);
                    code.push_back(0x59);
                    AppendModRM(code, 0b11, 1, 0);
                    
                    // RSQRTSSを使用して逆平方根近似を取得
                    // VRSQRTSS xmm0, xmm0, xmm0
                    AppendVEXPrefix(code, 0b01, 0b11, 0, 0, 0xF, 0, 0, 0);
                    code.push_back(0x52);
                    AppendModRM(code, 0b11, 0, 0);
                    
                    // 精度を改善するためにニュートン-ラフソン法を適用
                    // ...
                } else {
                    // SSE最適化版
                    // MOVSS xmm0, [srcSIMD]
                    AppendREXPrefix(code, false, false, false, 
                                  static_cast<uint8_t>(*srcSIMD) >= 8);
                    code.push_back(0xF3); // MOVSS prefix
                    code.push_back(0x0F);
                    code.push_back(0x10);
                    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                               static_cast<uint8_t>(*srcSIMD) & 0x7);
                    
                    // RSQRTSSを使用して逆平方根近似を取得
                    // RSQRTSS xmm0, xmm0
                    AppendREXPrefix(code, false, false, false, false);
                    code.push_back(0xF3); // RSQRTSS prefix
                    code.push_back(0x0F);
                    code.push_back(0x52);
                    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                               static_cast<uint8_t>(*destSIMD) & 0x7);
                    
                    // 精度を改善するためにニュートン-ラフソン法を適用
                    // ...
                }
            } else {
                // 非SIMD版（より多くの命令が必要）
                // ...
            }
            break;
        }
        
        case Opcode::kFastSin:
        case Opcode::kFastCos:
        case Opcode::kFastTan:
        case Opcode::kFastExp:
        case Opcode::kFastLog:
            // 他の高速数学関数も同様に実装
            // テイラー級数展開やチェビシェフ多項式近似などの数値計算手法を使用
            break;
            
        default:
            // サポートされていない操作
            break;
    }
}

// 自動CPU機能検出
void X86_64CodeGenerator::AutoDetectOptimalFlags() noexcept {
    // 基本の最適化フラグを設定
    m_optimizationFlags = CodeGenOptFlags::PeepholeOptimize | 
                         CodeGenOptFlags::AlignLoops | 
                         CodeGenOptFlags::OptimizeJumps | 
                         CodeGenOptFlags::CacheAware;
    
    // AVXサポートをチェック
    if (DetectCPUFeature("avx")) {
        m_optimizationFlags = m_optimizationFlags | CodeGenOptFlags::UseAVX;
    }
    
    // FMAサポートをチェック
    if (DetectCPUFeature("fma")) {
        m_optimizationFlags = m_optimizationFlags | CodeGenOptFlags::UseFMA;
    }
}

// CPUの機能を検出
bool X86_64CodeGenerator::DetectCPUFeature(const std::string& feature) noexcept {
#ifdef _MSC_VER
    // Windows環境での実装
    int cpuInfo[4] = {0};
    
    // CPUID命令を使用して機能をチェック
    __cpuid(cpuInfo, 1);
    
    if (feature == "sse") return (cpuInfo[3] & (1 << 25)) != 0;
    else if (feature == "sse2") return (cpuInfo[3] & (1 << 26)) != 0;
    else if (feature == "sse3") return (cpuInfo[2] & (1 << 0)) != 0;
    else if (feature == "ssse3") return (cpuInfo[2] & (1 << 9)) != 0;
    else if (feature == "sse4.1") return (cpuInfo[2] & (1 << 19)) != 0;
    else if (feature == "sse4.2") return (cpuInfo[2] & (1 << 20)) != 0;
    else if (feature == "avx") return (cpuInfo[2] & (1 << 28)) != 0;
    
    // AVX2, FMA, AVX-512などの拡張命令セットを検出
    if (feature == "avx2" || feature == "fma" || feature == "avx512f") {
        __cpuid(cpuInfo, 7);
        if (feature == "avx2") return (cpuInfo[1] & (1 << 5)) != 0;
        else if (feature == "avx512f") return (cpuInfo[1] & (1 << 16)) != 0;
    }
    
    if (feature == "fma") {
        __cpuid(cpuInfo, 1);
        return (cpuInfo[2] & (1 << 12)) != 0;
    }
#else
    // 非Windows環境（Linux/Macなど）での実装
    // ここでは簡易的な実装
    return false;
#endif

    return false;
}

// EVEX プレフィックスの追加ヘルパー
void X86_64CodeGenerator::AppendEVEXPrefix(std::vector<uint8_t>& code, uint8_t m, uint8_t p, uint8_t l, uint8_t w, 
                                          uint8_t vvvv, uint8_t aaa, uint8_t z, uint8_t b, 
                                          uint8_t v2, uint8_t k, bool broadcast) noexcept {
    // EVEX プレフィックス (4バイト)
    // 62 R X B R' 00 mm vvvv W v2 0 pp z L' b V' aaa
    
    // 1バイト目: 常に 0x62
    code.push_back(0x62);
    
    // 2バイト目: R, X, B, R' と mm フィールド
    // R' = 0 (AVX-512でのみ使用)
    uint8_t byte2 = (~b & 1) << 5 | (~x & 1) << 6 | (~r & 1) << 7;
    byte2 |= (0 & 1) << 4 | m;
    code.push_back(byte2);
    
    // 3バイト目: vvvv, W, v2と予約ビット
    uint8_t byte3 = (w & 1) << 7 | ((~vvvv) & 0xF) << 3 | (v2 & 1) << 2;
    code.push_back(byte3);
    
    // 4バイト目: z, L, b, V, aaa (マスクレジスタ)
    // V' = 0 (AVX-512でのみ使用)
    uint8_t byte4 = (z & 1) << 7 | (l & 3) << 5 | (broadcast ? 1 : 0) << 4;
    byte4 |= (0 & 1) << 3 | (aaa & 7);
    code.push_back(byte4);
}

// AVX-512算術演算
void X86_64CodeGenerator::EncodeAVX512Arithmetic(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t destReg = inst.args[0];
    int32_t src1Reg = inst.args[1];
    int32_t src2Reg = inst.args[2];
    
    // マスクレジスタ (オプション)
    uint8_t mask = 0; // k0 = no masking
    bool isZeroMasking = false;
    if (inst.args.size() >= 4) {
        mask = static_cast<uint8_t>(inst.args[3] & 0x7); // k1-k7
        if (inst.args.size() >= 5) {
            isZeroMasking = inst.args[4] != 0;
        }
    }
    
    auto destSIMD = GetSIMDReg(destReg);
    auto src1SIMD = GetSIMDReg(src1Reg);
    auto src2SIMD = GetSIMDReg(src2Reg);
    
    if (!destSIMD.has_value() || !src1SIMD.has_value() || !src2SIMD.has_value()) return;
    
    // AVX-512が使用可能かチェック
    bool useAVX512 = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX512);
    if (!useAVX512) {
        // AVX-512がサポートされていない場合は通常のAVXを使用
        EncodeSIMDArithmetic(inst, code);
        return;
    }
    
    uint8_t opcode = 0;
    uint8_t prefix = 0;
    
    // 命令タイプを決定
    switch (inst.opcode) {
        case Opcode::kSIMDAdd:
            opcode = 0x58; // ADDPS
            break;
        case Opcode::kSIMDSub:
            opcode = 0x5C; // SUBPS
            break;
        case Opcode::kSIMDMul:
            opcode = 0x59; // MULPS
            break;
        case Opcode::kSIMDDiv:
            opcode = 0x5E; // DIVPS
            break;
        case Opcode::kSIMDMin:
            opcode = 0x5D; // MINPS
            break;
        case Opcode::kSIMDMax:
            opcode = 0x5F; // MAXPS
            break;
        case Opcode::kSIMDAnd:
            opcode = 0x54; // ANDPS
            break;
        case Opcode::kSIMDOr:
            opcode = 0x56; // ORPS
            break;
        case Opcode::kSIMDXor:
            opcode = 0x57; // XORPS
            break;
        default:
            return; // サポートされていない操作
    }
    
    // EVEX.512.0F.W0 58 /r VADDPS zmm1 {k1}{z}, zmm2, zmm3/m512/m32bcst
    AppendEVEXPrefix(code, 0b01, 0b00, 0b10, 0,
                     static_cast<uint8_t>(*src1SIMD) ^ 0xF, // EVEX.vvvv = src1 (反転)
                     mask, isZeroMasking ? 1 : 0,
                     static_cast<uint8_t>(*src2SIMD) >= 8,
                     static_cast<uint8_t>(*destSIMD) >= 16 ? 1 : 0, 
                     mask, false);
    
    code.push_back(opcode);
    
    // ModRM
    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                static_cast<uint8_t>(*src2SIMD) & 0x7);
}

// AVX-512 FMA操作
void X86_64CodeGenerator::EncodeAVX512FMA(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 4) return;
    
    int32_t destReg = inst.args[0];
    int32_t src1Reg = inst.args[1];
    int32_t src2Reg = inst.args[2];
    int32_t src3Reg = inst.args[3];
    
    // マスクレジスタ (オプション)
    uint8_t mask = 0; // k0 = no masking
    bool isZeroMasking = false;
    if (inst.args.size() >= 5) {
        mask = static_cast<uint8_t>(inst.args[4] & 0x7); // k1-k7
        if (inst.args.size() >= 6) {
            isZeroMasking = inst.args[5] != 0;
        }
    }
    
    auto destSIMD = GetSIMDReg(destReg);
    auto src1SIMD = GetSIMDReg(src1Reg);
    auto src2SIMD = GetSIMDReg(src2Reg);
    auto src3SIMD = GetSIMDReg(src3Reg);
    
    if (!destSIMD.has_value() || !src1SIMD.has_value() || 
        !src2SIMD.has_value() || !src3SIMD.has_value()) return;
    
    // AVX-512とFMAが使用可能かチェック
    bool useAVX512 = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX512);
    bool useFMA = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseFMA);
    
    if (!useAVX512 || !useFMA) {
        // AVX-512またはFMAがサポートされていない場合は通常のFMAを使用
        EncodeFMA(inst, code);
        return;
    }
    
    // EVEX.DDS.512.66.0F38.W0 B8 /r VFMADD231PS zmm1 {k1}{z}, zmm2, zmm3/m512/m32bcst
    AppendEVEXPrefix(code, 0b10, 0b01, 0b10, 0,
                     static_cast<uint8_t>(*src2SIMD) ^ 0xF, // EVEX.vvvv = src2 (反転)
                     mask, isZeroMasking ? 1 : 0,
                     static_cast<uint8_t>(*src3SIMD) >= 8,
                     static_cast<uint8_t>(*destSIMD) >= 16 ? 1 : 0,
                     mask, false);
    
    code.push_back(0xB8); // VFMADD231PS opcode
    
    // ModRM
    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                static_cast<uint8_t>(*src3SIMD) & 0x7);
}

// AVX-512ロード操作
void X86_64CodeGenerator::EncodeAVX512Load(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t destReg = inst.args[0];
    int32_t addrReg = inst.args[1];
    int32_t offset = inst.args[2];
    
    // マスクレジスタ (オプション)
    uint8_t mask = 0; // k0 = no masking
    bool isZeroMasking = false;
    if (inst.args.size() >= 4) {
        mask = static_cast<uint8_t>(inst.args[3] & 0x7); // k1-k7
        if (inst.args.size() >= 5) {
            isZeroMasking = inst.args[4] != 0;
        }
    }
    
    auto simdReg = GetSIMDReg(destReg);
    if (!simdReg.has_value()) return;
    
    X86_64Register addrRegPhysical = GetPhysicalReg(addrReg);
    
    // AVX-512が使用可能かチェック
    bool useAVX512 = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX512);
    if (!useAVX512) {
        // AVX-512がサポートされていない場合は通常のAVXを使用
        EncodeSIMDLoad(inst, code);
        return;
    }
    
    // EVEX.512.0F.W0 10 /r VMOVUPS zmm1 {k1}{z}, m512
    AppendEVEXPrefix(code, 0b01, 0b00, 0b10, 0, 0b1111, 
                    mask, isZeroMasking ? 1 : 0, 
                    static_cast<uint8_t>(addrRegPhysical) >= 8,
                    static_cast<uint8_t>(*simdReg) >= 16 ? 1 : 0, 
                    mask, false);
    
    code.push_back(0x10); // MOVUPS opcode
    
    // ModRM: [addrReg + offset]
    if (offset == 0) {
        // レジスタ間接アドレッシング
        AppendModRM(code, 0b00, static_cast<uint8_t>(*simdReg) & 0x7, 
                    static_cast<uint8_t>(addrRegPhysical) & 0x7);
    } else if (offset >= -128 && offset <= 127) {
        // 1バイト変位付きアドレッシング
        AppendModRM(code, 0b01, static_cast<uint8_t>(*simdReg) & 0x7, 
                    static_cast<uint8_t>(addrRegPhysical) & 0x7);
        code.push_back(static_cast<uint8_t>(offset));
    } else {
        // 4バイト変位付きアドレッシング
        AppendModRM(code, 0b10, static_cast<uint8_t>(*simdReg) & 0x7, 
                    static_cast<uint8_t>(addrRegPhysical) & 0x7);
        AppendImmediate32(code, offset);
    }
}

// AVX-512ストア操作
void X86_64CodeGenerator::EncodeAVX512Store(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t srcReg = inst.args[0];
    int32_t addrReg = inst.args[1];
    int32_t offset = inst.args[2];
    
    // マスクレジスタ (オプション)
    uint8_t mask = 0; // k0 = no masking
    bool isZeroMasking = false;
    if (inst.args.size() >= 4) {
        mask = static_cast<uint8_t>(inst.args[3] & 0x7); // k1-k7
    }
    
    auto simdReg = GetSIMDReg(srcReg);
    if (!simdReg.has_value()) return;
    
    X86_64Register addrRegPhysical = GetPhysicalReg(addrReg);
    
    // AVX-512が使用可能かチェック
    bool useAVX512 = HasFlag(m_optimizationFlags, CodeGenOptFlags::UseAVX512);
    if (!useAVX512) {
        // AVX-512がサポートされていない場合は通常のAVXを使用
        EncodeSIMDStore(inst, code);
        return;
    }
    
    // EVEX.512.0F.W0 11 /r VMOVUPS m512 {k1}, zmm1
    AppendEVEXPrefix(code, 0b01, 0b00, 0b10, 0, 0b1111, 
                    mask, 0, // ストア操作にはゼロマスキングなし
                    static_cast<uint8_t>(addrRegPhysical) >= 8,
                    static_cast<uint8_t>(*simdReg) >= 16 ? 1 : 0, 
                    mask, false);
    
    code.push_back(0x11); // MOVUPS opcode (store)
    
    // ModRM: [addrReg + offset]
    if (offset == 0) {
        AppendModRM(code, 0b00, static_cast<uint8_t>(*simdReg) & 0x7, 
                    static_cast<uint8_t>(addrRegPhysical) & 0x7);
    } else if (offset >= -128 && offset <= 127) {
        AppendModRM(code, 0b01, static_cast<uint8_t>(*simdReg) & 0x7, 
                    static_cast<uint8_t>(addrRegPhysical) & 0x7);
        code.push_back(static_cast<uint8_t>(offset));
    } else {
        AppendModRM(code, 0b10, static_cast<uint8_t>(*simdReg) & 0x7, 
                    static_cast<uint8_t>(addrRegPhysical) & 0x7);
        AppendImmediate32(code, offset);
    }
}

// AVX-512マスク操作
void X86_64CodeGenerator::EncodeAVX512MaskOp(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t destMaskReg = inst.args[0]; // k1-k7
    int32_t src1MaskReg = inst.args[1]; // k1-k7
    int32_t src2MaskReg = inst.args[2]; // k1-k7
    
    uint8_t dest = static_cast<uint8_t>(destMaskReg & 0x7);
    uint8_t src1 = static_cast<uint8_t>(src1MaskReg & 0x7);
    uint8_t src2 = static_cast<uint8_t>(src2MaskReg & 0x7);
    
    uint8_t opcode = 0;
    
    // 命令タイプを決定
    switch (inst.opcode) {
        case Opcode::kMaskAnd:
            opcode = 0x42; // KANDW
            break;
        case Opcode::kMaskOr:
            opcode = 0x45; // KORW
            break;
        case Opcode::kMaskXor:
            opcode = 0x46; // KXORW
            break;
        case Opcode::kMaskNot:
            opcode = 0x44; // KNOTW
            break;
        default:
            return; // サポートされていない操作
    }
    
    // VEX.L0.0F.W0 42 /r KANDW k1, k2, k3
    AppendVEXPrefix(code, 0b10, 0b00, 0, 0, 
                   src1 ^ 0xF, // VEX.vvvv = src1 (反転)
                   0, 0, 0);
    
    code.push_back(opcode);
    
    // ModRM
    AppendModRM(code, 0b11, dest, src2);
}

// AVX-512マスクを使用したブレンド操作
void X86_64CodeGenerator::EncodeAVX512Blend(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 4) return;
    
    int32_t destReg = inst.args[0];
    int32_t src1Reg = inst.args[1];
    int32_t src2Reg = inst.args[2];
    int32_t maskReg = inst.args[3]; // k1-k7
    
    auto destSIMD = GetSIMDReg(destReg);
    auto src1SIMD = GetSIMDReg(src1Reg);
    auto src2SIMD = GetSIMDReg(src2Reg);
    
    if (!destSIMD.has_value() || !src1SIMD.has_value() || !src2SIMD.has_value()) return;
    
    uint8_t mask = static_cast<uint8_t>(maskReg & 0x7);
    
    // EVEX.NDS.512.0F.W0 C6 /r VSHUFPS zmm1 {k1}, zmm2, zmm3/m512/m32bcst, imm8
    AppendEVEXPrefix(code, 0b01, 0b00, 0b10, 0,
                     static_cast<uint8_t>(*src1SIMD) ^ 0xF, // EVEX.vvvv = src1 (反転)
                     mask, 0,
                     static_cast<uint8_t>(*src2SIMD) >= 8,
                     static_cast<uint8_t>(*destSIMD) >= 16 ? 1 : 0,
                     mask, false);
    
    code.push_back(0xC4); // VPSHUFB opcode
    
    // ModRM
    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                static_cast<uint8_t>(*src2SIMD) & 0x7);
}

// AVX-512データパーミュテーション
void X86_64CodeGenerator::EncodeAVX512Permute(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t destReg = inst.args[0];
    int32_t srcReg = inst.args[1];
    int32_t immValue = inst.args[2]; // インデックステーブル
    
    // マスクレジスタ (オプション)
    uint8_t mask = 0; // k0 = no masking
    bool isZeroMasking = false;
    if (inst.args.size() >= 4) {
        mask = static_cast<uint8_t>(inst.args[3] & 0x7); // k1-k7
        if (inst.args.size() >= 5) {
            isZeroMasking = inst.args[4] != 0;
        }
    }
    
    auto destSIMD = GetSIMDReg(destReg);
    auto srcSIMD = GetSIMDReg(srcReg);
    
    if (!destSIMD.has_value() || !srcSIMD.has_value()) return;
    
    // EVEX.512.66.0F3A.W0 00 /r VPERMQ zmm1 {k1}{z}, zmm2/m512/m64bcst, imm8
    AppendEVEXPrefix(code, 0b11, 0b01, 0b10, 0,
                     0b1111, // EVEX.vvvv = 0xF (未使用)
                     mask, isZeroMasking ? 1 : 0,
                     static_cast<uint8_t>(*srcSIMD) >= 8,
                     static_cast<uint8_t>(*destSIMD) >= 16 ? 1 : 0,
                     mask, false);
    
    code.push_back(0x00); // VPERMQ opcode
    
    // ModRM
    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                static_cast<uint8_t>(*srcSIMD) & 0x7);
    
    // インデックステーブル (8ビット即値)
    code.push_back(static_cast<uint8_t>(immValue & 0xFF));
}

// AVX-512データ圧縮
void X86_64CodeGenerator::EncodeAVX512Compress(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t destReg = inst.args[0];
    int32_t srcReg = inst.args[1];
    int32_t maskReg = inst.args[2]; // k1-k7
    
    auto destSIMD = GetSIMDReg(destReg);
    auto srcSIMD = GetSIMDReg(srcReg);
    
    if (!destSIMD.has_value() || !srcSIMD.has_value()) return;
    
    uint8_t mask = static_cast<uint8_t>(maskReg & 0x7);
    
    // EVEX.512.66.0F38.W0 8B /r VCOMPRESSPS zmm1 {k1}{z}, zmm2
    AppendEVEXPrefix(code, 0b10, 0b01, 0b10, 0,
                     0b1111, // EVEX.vvvv = 0xF (未使用)
                     mask, 0, // Zマスキングなし
                     static_cast<uint8_t>(*srcSIMD) >= 8,
                     static_cast<uint8_t>(*destSIMD) >= 16 ? 1 : 0,
                     mask, false);
    
    code.push_back(0x8B); // VCOMPRESSPS opcode
    
    // ModRM
    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                static_cast<uint8_t>(*srcSIMD) & 0x7);
}

// AVX-512データ展開
void X86_64CodeGenerator::EncodeAVX512Expand(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept {
    if (inst.args.size() < 3) return;
    
    int32_t destReg = inst.args[0];
    int32_t srcReg = inst.args[1];
    int32_t maskReg = inst.args[2]; // k1-k7
    
    auto destSIMD = GetSIMDReg(destReg);
    auto srcSIMD = GetSIMDReg(srcReg);
    
    if (!destSIMD.has_value() || !srcSIMD.has_value()) return;
    
    uint8_t mask = static_cast<uint8_t>(maskReg & 0x7);
    bool isZeroMasking = true; // 展開では通常ゼロマスキングを使用
    
    // EVEX.512.66.0F38.W0 89 /r VEXPANDPS zmm1 {k1}{z}, zmm2
    AppendEVEXPrefix(code, 0b10, 0b01, 0b10, 0,
                     0b1111, // EVEX.vvvv = 0xF (未使用)
                     mask, isZeroMasking ? 1 : 0,
                     static_cast<uint8_t>(*srcSIMD) >= 8,
                     static_cast<uint8_t>(*destSIMD) >= 16 ? 1 : 0,
                     mask, false);
    
    code.push_back(0x89); // VEXPANDPS opcode
    
    // ModRM
    AppendModRM(code, 0b11, static_cast<uint8_t>(*destSIMD) & 0x7, 
                static_cast<uint8_t>(*srcSIMD) & 0x7);
}

}  // namespace core
}  // namespace aerojs