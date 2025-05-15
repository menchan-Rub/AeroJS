/**
 * @file arm64_assembler.cpp
 * @brief AeroJS JavaScriptエンジン用の世界最高性能ARM64アセンブラの実装
 * @version 1.0.0
 * @license MIT
 */

#include "arm64_assembler.h"
#include <cstring>
#include <algorithm>
#include <bit>

namespace aerojs {
namespace core {
namespace arm64 {

// 最適化された命令定数
namespace InstructionConstants {
    // ブランチ命令
    constexpr uint32_t B_MASK = 0xFC000000;
    constexpr uint32_t B = 0x14000000;
    constexpr uint32_t BL = 0x94000000;
    constexpr uint32_t BR = 0xD61F0000;
    constexpr uint32_t BLR = 0xD63F0000;
    constexpr uint32_t RET = 0xD65F0000;
    constexpr uint32_t BCOND = 0x54000000;
    constexpr uint32_t CBZ = 0x34000000;
    constexpr uint32_t CBNZ = 0x35000000;
    constexpr uint32_t TBZ = 0x36000000;
    constexpr uint32_t TBNZ = 0x37000000;
    
    // データ処理命令
    constexpr uint32_t MOV_SP = 0x91000000;
    constexpr uint32_t MOV = 0xAA0003E0;
    constexpr uint32_t MOVZ = 0x52800000;
    constexpr uint32_t MOVN = 0x12800000;
    constexpr uint32_t MOVK = 0x72800000;
    
    // 算術命令
    constexpr uint32_t ADD = 0x8B000000;
    constexpr uint32_t ADDS = 0xAB000000;
    constexpr uint32_t SUB = 0xCB000000;
    constexpr uint32_t SUBS = 0xEB000000;
    constexpr uint32_t ADC = 0x9A000000;
    constexpr uint32_t SBC = 0xDA000000;
    
    // 論理命令
    constexpr uint32_t AND = 0x8A000000;
    constexpr uint32_t ORR = 0xAA000000;
    constexpr uint32_t EOR = 0xCA000000;
    constexpr uint32_t ANDS = 0xEA000000;
    constexpr uint32_t BIC = 0x8A200000;
    
    // ARMv8.1以降の拡張命令
    constexpr uint32_t LRCPC = 0xD9800000;   // LDAPR
    constexpr uint32_t LDAPR = 0xD9800000;   // Load-Acquire RCpc Register
    constexpr uint32_t STAPR = 0xD9000000;   // Store-Release RCpc Register
    
    // ARMv8.2 拡張命令
    constexpr uint32_t PRFM = 0xF9800000;    // プリフェッチメモリ
    
    // ARMv8.3 拡張命令
    constexpr uint32_t FJCVTZS = 0x1E7E0000; // 浮動小数点JavaScriptの変換
    
    // ARMv8.4 拡張命令
    constexpr uint32_t SVE_ADD = 0x04200000; // SVE加算
    
    // ARMv8.5 拡張命令
    constexpr uint32_t BTI = 0xD503241F;     // ブランチターゲット識別
    constexpr uint32_t BTP = 0xD503243F;     // ブランチターゲット識別
    
    // SIMD命令
    constexpr uint32_t MOVI = 0x0F000400;    // ベクトル即値移動
    constexpr uint32_t FADD_V = 0x4E20D400;  // ベクトル浮動小数点加算
    constexpr uint32_t FMUL_V = 0x6E20DC00;  // ベクトル浮動小数点乗算
    
    // NOP
    constexpr uint32_t NOP = 0xD503201F;
    
    // メモリ最適化
    constexpr uint32_t PRFM_PLDL1KEEP = 0xF9800000 | (0 << 5);
    constexpr uint32_t PRFM_PLDL1STRM = 0xF9800000 | (1 << 5);
    constexpr uint32_t PRFM_PLDL2KEEP = 0xF9800000 | (2 << 5);
    constexpr uint32_t PRFM_PLDL2STRM = 0xF9800000 | (3 << 5);
    constexpr uint32_t PRFM_PLDL3KEEP = 0xF9800000 | (4 << 5);
    constexpr uint32_t PRFM_PLDL3STRM = 0xF9800000 | (5 << 5);
}

// コンストラクタ - バッファ初期サイズを最適化
ARM64Assembler::ARM64Assembler() {
    _buffer.reserve(16384);  // 一般的なJIT関数サイズに最適化
}

// デストラクタ
ARM64Assembler::~ARM64Assembler() {}

// バッファリセット
void ARM64Assembler::reset() {
    _buffer.clear();
    _buffer.reserve(16384);  // 最適なパフォーマンスのためにキャパシティを維持
}

// 命令エンコード - 高速バージョン
void ARM64Assembler::emit(uint32_t instruction) {
    // 単一追加操作でパフォーマンス向上
    size_t pos = _buffer.size();
    _buffer.resize(pos + 4);
    std::memcpy(_buffer.data() + pos, &instruction, 4);
}

// ラベルバインド - パフォーマンス最適化
void ARM64Assembler::bind(Label* label) {
    assert(label != nullptr);
    int position = static_cast<int>(_buffer.size());
    label->bind(position);
}

// アライメント調整 - キャッシュラインを考慮
void ARM64Assembler::align(int alignment) {
    // CPUキャッシュライン（通常64バイト）を考慮した最適アライメント
    // 16バイトアライメントはARM64で最も効率的
    static const int OPTIMAL_ALIGNMENT = 16;
    alignment = std::max(alignment, OPTIMAL_ALIGNMENT);
    
    // アライメント処理
    size_t currentPos = _buffer.size();
    size_t alignedPos = (currentPos + alignment - 1) & ~(alignment - 1);
    size_t padding = alignedPos - currentPos;
    
    // NOPでパディング
    for (size_t i = 0; i < padding / 4; i++) {
        emit(InstructionConstants::NOP);
    }
}

// 最適化されたレジスタエンコード
uint32_t ARM64Assembler::encodeReg(Register reg) {
    return static_cast<uint32_t>(reg) & 0x1F;
}

// 最適化された浮動小数点レジスタエンコード
uint32_t ARM64Assembler::encodeSIMDReg(FloatRegister reg) {
    return static_cast<uint32_t>(reg) & 0x1F;
}

// 最適化された移動命令
void ARM64Assembler::mov(Register rd, Register rm) {
    uint32_t rdCode = encodeReg(rd);
    uint32_t rmCode = encodeReg(rm);
    
    // 同一レジスタへの移動を最適化
    if (rdCode == rmCode) return;
    
    // 64ビットレジスタかチェック
    bool is64Bit = static_cast<uint32_t>(rd) < 32 || static_cast<uint32_t>(rd) >= 64;
    uint32_t sf = is64Bit ? 0x80000000 : 0;
    
    // レジスタ移動をエンコード
    emit(InstructionConstants::ORR | sf | (rmCode << 16) | (rdCode));
}

// 最適化された即値移動
void ARM64Assembler::mov(Register rd, uint64_t imm) {
    // ゼロの場合はXZRを使用して最適化
    if (imm == 0) {
        uint32_t rdCode = encodeReg(rd);
        bool is64Bit = static_cast<uint32_t>(rd) < 32 || static_cast<uint32_t>(rd) >= 64;
        uint32_t sf = is64Bit ? 0x80000000 : 0;
        emit(InstructionConstants::ORR | sf | (31 << 5) | rdCode);
        return;
    }
    
    // ビットパターン解析による最適化
    if (imm <= 0xFFFF) {
        // 16ビット以下ならMOVZ
        movz(rd, static_cast<uint16_t>(imm));
    } else if ((~imm) <= 0xFFFF) {
        // ビット反転が16ビット以下ならMOVN
        movn(rd, static_cast<uint16_t>(~imm));
    } else if ((imm & 0xFFFF) == 0 && (imm <= 0xFFFF0000)) {
        // 上位16ビットのみならシフト付きMOVZ
        movz(rd, static_cast<uint16_t>(imm >> 16), 16);
    } else if ((imm & 0xFFFF0000) == 0xFFFF0000) {
        // 上位16ビットが全て1ならMOVN+シフト
        movn(rd, static_cast<uint16_t>(~imm), 0);
    } else {
        // 複数命令が必要な場合の最適パターン検出
        uint16_t imm0 = imm & 0xFFFF;
        uint16_t imm1 = (imm >> 16) & 0xFFFF;
        uint16_t imm2 = (imm >> 32) & 0xFFFF;
        uint16_t imm3 = (imm >> 48) & 0xFFFF;
        
        // ビットパターン解析
        int zeroCount = (imm0 == 0) + (imm1 == 0) + (imm2 == 0) + (imm3 == 0);
        int ffCount = (imm0 == 0xFFFF) + (imm1 == 0xFFFF) + (imm2 == 0xFFFF) + (imm3 == 0xFFFF);
        
        bool is64Bit = static_cast<uint32_t>(rd) < 32 || static_cast<uint32_t>(rd) >= 64;
        
        if (zeroCount >= ffCount) {
            // MOVZを使用したシーケンス
            bool first = true;
            if (imm0 != 0) {
                movz(rd, imm0, 0);
                first = false;
            }
            if (imm1 != 0) {
                if (first) {
                    movz(rd, imm1, 16);
                    first = false;
                } else {
                    movk(rd, imm1, 16);
                }
            }
            if (imm2 != 0 && is64Bit) {
                if (first) {
                    movz(rd, imm2, 32);
                    first = false;
                } else {
                    movk(rd, imm2, 32);
                }
            }
            if (imm3 != 0 && is64Bit) {
                if (first) {
                    movz(rd, imm3, 48);
                } else {
                    movk(rd, imm3, 48);
                }
            }
        } else {
            // MOVNを使用したシーケンス
            bool first = true;
            if (imm0 != 0xFFFF) {
                movn(rd, ~imm0 & 0xFFFF, 0);
                first = false;
            }
            if (imm1 != 0xFFFF) {
                if (first) {
                    movn(rd, ~imm1 & 0xFFFF, 16);
                    first = false;
                } else {
                    movk(rd, imm1, 16);
                }
            }
            if (imm2 != 0xFFFF && is64Bit) {
                if (first) {
                    movn(rd, ~imm2 & 0xFFFF, 32);
                    first = false;
                } else {
                    movk(rd, imm2, 32);
                }
            }
            if (imm3 != 0xFFFF && is64Bit) {
                if (first) {
                    movn(rd, ~imm3 & 0xFFFF, 48);
                } else {
                    movk(rd, imm3, 48);
                }
            }
        }
    }
}

// 分岐コード最適化 - ヒントフラグ付き
void ARM64Assembler::b(Label* label) {
    int offset = static_cast<int>(_buffer.size());
    
    // 最適化されたリンク記録
    if (label->isBound()) {
        // ラベルがすでにバインドされている場合、相対オフセットを計算
        int target = label->position();
        int delta = target - offset;
        
        // 範囲チェック
        assert(delta >= -128 * 1024 * 1024 && delta < 128 * 1024 * 1024 && "分岐オフセットが範囲外です");
        
        // 命令エンコード
        uint32_t instr = InstructionConstants::B | ((delta >> 2) & 0x3FFFFFF);
        emit(instr);
    } else {
        // フォワード参照
        emit(InstructionConstants::B);
        
        // 解決関数の最適化
        label->addReference(offset, [this, offset](int target) {
            int delta = target - offset;
            assert(delta >= -128 * 1024 * 1024 && delta < 128 * 1024 * 1024 && "分岐オフセットが範囲外です");
            
            uint32_t instr;
            std::memcpy(&instr, &_buffer[offset], 4);
            instr |= ((delta >> 2) & 0x3FFFFFF);
            std::memcpy(&_buffer[offset], &instr, 4);
        });
    }
}

// 最適化された条件分岐
void ARM64Assembler::bCond(Condition cond, Label* label) {
    int offset = static_cast<int>(_buffer.size());
    uint32_t condCode = static_cast<uint32_t>(cond);
    
    if (label->isBound()) {
        // 既知のターゲットへの分岐
        int target = label->position();
        int delta = target - offset;
        
        // 条件分岐の範囲は±1MB
        assert(delta >= -1 * 1024 * 1024 && delta < 1 * 1024 * 1024 && "条件分岐のオフセットが範囲外です");
        
        uint32_t instr = InstructionConstants::BCOND | (condCode << 0) | ((delta >> 2) & 0x7FFFF) << 5;
        emit(instr);
    } else {
        // 未解決のフォワード参照
        emit(InstructionConstants::BCOND | (condCode << 0));
        
        label->addReference(offset, [this, offset, condCode](int target) {
            int delta = target - offset;
            assert(delta >= -1 * 1024 * 1024 && delta < 1 * 1024 * 1024 && "条件分岐のオフセットが範囲外です");
            
            uint32_t instr;
            std::memcpy(&instr, &_buffer[offset], 4);
            instr |= ((delta >> 2) & 0x7FFFF) << 5;
            std::memcpy(&_buffer[offset], &instr, 4);
        });
    }
}

// メモリアクセス最適化
void ARM64Assembler::ldr(Register rt, const MemOperand& operand) {
    bool is64Bit = static_cast<uint32_t>(rt) < 32 || static_cast<uint32_t>(rt) >= 64;
    uint8_t size = is64Bit ? 3 : 2;
    
    // アライメント最適化
    if (operand.isRegisterOffset()) {
        // レジスタオフセット形式
        uint32_t rtCode = encodeReg(rt);
        uint32_t rnCode = encodeReg(operand.getBase());
        uint32_t rmCode = encodeReg(operand.getIndex());
        
        uint32_t option = 0;
        uint32_t shifted = 0;
        
        if (operand.isShiftExtend()) {
            // 拡張形式
            option = static_cast<uint32_t>(operand.getExtend()) << 13;
            shifted = operand.getShiftAmount() ? (1 << 12) : 0;
        } else {
            // シフト形式
            option = static_cast<uint32_t>(operand.getShift()) << 22;
            shifted = operand.getShiftAmount() << 12;
        }
        
        uint32_t instr = 0xB8600800 | // LDRベース
                        (size << 30) |
                        option |
                        shifted |
                        (rmCode << 16) |
                        (rnCode << 5) |
                        rtCode;
        
        emit(instr);
    } else {
        // 即値オフセット形式
        uint32_t rtCode = encodeReg(rt);
        uint32_t rnCode = encodeReg(operand.getBase());
        int32_t offset = operand.getOffset();
        
        if (operand.isPreIndex()) {
            // プレインデックス
            uint32_t instr = 0xB8400C00 | // LDR Pre-indexベース
                            (size << 30) |
                            ((offset & 0x1FF) << 12) |
                            (rnCode << 5) |
                            rtCode;
            emit(instr);
        } else if (operand.isPostIndex()) {
            // ポストインデックス
            uint32_t instr = 0xB8400400 | // LDR Post-indexベース
                            (size << 30) |
                            ((offset & 0x1FF) << 12) |
                            (rnCode << 5) |
                            rtCode;
            emit(instr);
        } else {
            // 通常オフセット
            if (is64Bit) {
                // スケーリングされたオフセット
                uint32_t scaledOffset = static_cast<uint32_t>(offset / 8) & 0xFFF;
                uint32_t instr = 0xF9400000 | // LDR 64ビットベース
                                (scaledOffset << 10) |
                                (rnCode << 5) |
                                rtCode;
                emit(instr);
            } else {
                // 32ビットロード
                uint32_t scaledOffset = static_cast<uint32_t>(offset / 4) & 0xFFF;
                uint32_t instr = 0xB9400000 | // LDR 32ビットベース
                                (scaledOffset << 10) |
                                (rnCode << 5) |
                                rtCode;
                emit(instr);
            }
        }
    }
}

// メモリプリフェッチ最適化
void ARM64Assembler::prfm(PrefetchType type, const MemOperand& operand) {
    uint32_t typeCode;
    switch (type) {
        case PrefetchType::PLDL1KEEP: typeCode = 0; break;
        case PrefetchType::PLDL1STRM: typeCode = 1; break;
        case PrefetchType::PLDL2KEEP: typeCode = 2; break;
        case PrefetchType::PLDL2STRM: typeCode = 3; break;
        case PrefetchType::PLDL3KEEP: typeCode = 4; break;
        case PrefetchType::PLDL3STRM: typeCode = 5; break;
        case PrefetchType::PSTL1KEEP: typeCode = 8; break;
        case PrefetchType::PSTL1STRM: typeCode = 9; break;
        case PrefetchType::PSTL2KEEP: typeCode = 10; break;
        case PrefetchType::PSTL2STRM: typeCode = 11; break;
        case PrefetchType::PSTL3KEEP: typeCode = 12; break;
        case PrefetchType::PSTL3STRM: typeCode = 13; break;
        default: typeCode = 0; break;
    }
    
    uint32_t rnCode = encodeReg(operand.getBase());
    
    if (operand.isRegisterOffset()) {
        // レジスタオフセット形式
        uint32_t rmCode = encodeReg(operand.getIndex());
        uint32_t option = static_cast<uint32_t>(operand.getShift()) << 22;
        uint32_t amount = operand.getShiftAmount() << 12;
        
        uint32_t instr = 0xF8A00800 | option | amount | (rmCode << 16) | (rnCode << 5) | typeCode;
        emit(instr);
    } else {
        // 即値オフセット
        int32_t offset = operand.getOffset();
        uint32_t scaledOffset = static_cast<uint32_t>(offset / 8) & 0xFFF;
        
        uint32_t instr = 0xF9800000 | (scaledOffset << 10) | (rnCode << 5) | typeCode;
        emit(instr);
    }
}

// BTI命令の追加 (Branch Target Identification)
void ARM64Assembler::bti(BranchTargetType type) {
    uint32_t hint;
    switch (type) {
        case BranchTargetType::C:  hint = 0x1; break;
        case BranchTargetType::J:  hint = 0x2; break;
        case BranchTargetType::JC: hint = 0x3; break;
        default: hint = 0x0; break;
    }
    
    emit(InstructionConstants::BTI | (hint << 5));
}

// データエミット
void ARM64Assembler::emitInt8(int8_t value) {
    _buffer.push_back(static_cast<uint8_t>(value));
}

void ARM64Assembler::emitInt16(int16_t value) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);
    for (int i = 0; i < 2; i++) {
        _buffer.push_back(bytes[i]);
    }
}

void ARM64Assembler::emitInt32(int32_t value) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);
    for (int i = 0; i < 4; i++) {
        _buffer.push_back(bytes[i]);
    }
}

void ARM64Assembler::emitInt64(int64_t value) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);
    for (int i = 0; i < 8; i++) {
        _buffer.push_back(bytes[i]);
    }
}

void ARM64Assembler::emitFloat(float value) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);
    for (int i = 0; i < 4; i++) {
        _buffer.push_back(bytes[i]);
    }
}

void ARM64Assembler::emitDouble(double value) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);
    for (int i = 0; i < 8; i++) {
        _buffer.push_back(bytes[i]);
    }
}

void ARM64Assembler::emitString(const char* str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        _buffer.push_back(static_cast<uint8_t>(str[i]));
    }
}

// 算術命令実装
void ARM64Assembler::add(Register rd, Register rn, const Operand& operand) {
    uint32_t rdCode = encodeReg(rd);
    uint32_t rnCode = encodeReg(rn);
    uint32_t rmCode = encodeReg(operand.getReg());
    bool is64Bit = static_cast<uint32_t>(rd) < 32 || static_cast<uint32_t>(rd) >= 64;
    uint32_t sf = is64Bit ? 0x80000000 : 0;
    
    if (operand.isExtended()) {
        // 拡張付きADD
        uint32_t option = static_cast<uint32_t>(operand.getExtend()) << 13;
        uint32_t amount = operand.getAmount() << 10;
        emit(InstructionConstants::ADD | sf | option | amount | (1 << 21) | (rmCode << 16) | (rnCode << 5) | rdCode);
    } else {
        // シフト付きADD
        uint32_t shift = static_cast<uint32_t>(operand.getShift()) << 22;
        uint32_t amount = operand.getAmount() << 10;
        emit(InstructionConstants::ADD | sf | shift | amount | (rmCode << 16) | (rnCode << 5) | rdCode);
    }
}

void ARM64Assembler::add(Register rd, Register rn, uint64_t imm) {
    // 即値ADD（12ビット + シフト）
    uint32_t rdCode = encodeReg(rd);
    uint32_t rnCode = encodeReg(rn);
    bool is64Bit = static_cast<uint32_t>(rd) < 32 || static_cast<uint32_t>(rd) >= 64;
    uint32_t sf = is64Bit ? 0x80000000 : 0;
    
    if (imm < 4096) {
        // 通常の12ビット即値
        emit(InstructionConstants::ADD | sf | (0 << 22) | (static_cast<uint32_t>(imm) << 10) | (rnCode << 5) | rdCode);
    } else if (imm < (4096 << 12) && (imm & 0xFFF) == 0) {
        // 12ビットシフト付き即値
        emit(InstructionConstants::ADD | sf | (1 << 22) | ((static_cast<uint32_t>(imm) >> 12) << 10) | (rnCode << 5) | rdCode);
    } else {
        // 大きな即値の場合、レジスタを経由
        // SCRATCH_REG0を一時的に使用
        mov(Register::SCRATCH_REG0, imm);
        add(rd, rn, Operand(Register::SCRATCH_REG0));
    }
}

void ARM64Assembler::sub(Register rd, Register rn, const Operand& operand) {
    uint32_t rdCode = encodeReg(rd);
    uint32_t rnCode = encodeReg(rn);
    uint32_t rmCode = encodeReg(operand.getReg());
    bool is64Bit = static_cast<uint32_t>(rd) < 32 || static_cast<uint32_t>(rd) >= 64;
    uint32_t sf = is64Bit ? 0x80000000 : 0;
    
    if (operand.isExtended()) {
        // 拡張付きSUB
        uint32_t option = static_cast<uint32_t>(operand.getExtend()) << 13;
        uint32_t amount = operand.getAmount() << 10;
        emit(InstructionConstants::SUB | sf | option | amount | (1 << 21) | (rmCode << 16) | (rnCode << 5) | rdCode);
    } else {
        // シフト付きSUB
        uint32_t shift = static_cast<uint32_t>(operand.getShift()) << 22;
        uint32_t amount = operand.getAmount() << 10;
        emit(InstructionConstants::SUB | sf | shift | amount | (rmCode << 16) | (rnCode << 5) | rdCode);
    }
}

void ARM64Assembler::sub(Register rd, Register rn, uint64_t imm) {
    // 即値SUB（12ビット + シフト）
    uint32_t rdCode = encodeReg(rd);
    uint32_t rnCode = encodeReg(rn);
    bool is64Bit = static_cast<uint32_t>(rd) < 32 || static_cast<uint32_t>(rd) >= 64;
    uint32_t sf = is64Bit ? 0x80000000 : 0;
    
    if (imm < 4096) {
        // 通常の12ビット即値
        emit(InstructionConstants::SUB | sf | (0 << 22) | (static_cast<uint32_t>(imm) << 10) | (rnCode << 5) | rdCode);
    } else if (imm < (4096 << 12) && (imm & 0xFFF) == 0) {
        // 12ビットシフト付き即値
        emit(InstructionConstants::SUB | sf | (1 << 22) | ((static_cast<uint32_t>(imm) >> 12) << 10) | (rnCode << 5) | rdCode);
    } else {
        // 大きな即値の場合、レジスタを経由
        // SCRATCH_REG0を一時的に使用
        mov(Register::SCRATCH_REG0, imm);
        sub(rd, rn, Operand(Register::SCRATCH_REG0));
    }
}

// 比較命令
void ARM64Assembler::cmp(Register rn, const Operand& operand) {
    // CMPはSUBSのエイリアス（結果は破棄）
    uint32_t rnCode = encodeReg(rn);
    uint32_t rmCode = encodeReg(operand.getReg());
    bool is64Bit = static_cast<uint32_t>(rn) < 32 || static_cast<uint32_t>(rn) >= 64;
    uint32_t sf = is64Bit ? 0x80000000 : 0;
    
    if (operand.isExtended()) {
        // 拡張付きCMP
        uint32_t option = static_cast<uint32_t>(operand.getExtend()) << 13;
        uint32_t amount = operand.getAmount() << 10;
        emit(InstructionConstants::SUBS | sf | option | amount | (1 << 21) | (rmCode << 16) | (rnCode << 5) | 0x1F);
    } else {
        // シフト付きCMP
        uint32_t shift = static_cast<uint32_t>(operand.getShift()) << 22;
        uint32_t amount = operand.getAmount() << 10;
        emit(InstructionConstants::SUBS | sf | shift | amount | (rmCode << 16) | (rnCode << 5) | 0x1F);
    }
}

void ARM64Assembler::cmp(Register rn, uint64_t imm) {
    // 即値CMP（SUBSのエイリアス）
    uint32_t rnCode = encodeReg(rn);
    bool is64Bit = static_cast<uint32_t>(rn) < 32 || static_cast<uint32_t>(rn) >= 64;
    uint32_t sf = is64Bit ? 0x80000000 : 0;
    
    if (imm < 4096) {
        // 通常の12ビット即値
        emit(InstructionConstants::SUBS | sf | (0 << 22) | (static_cast<uint32_t>(imm) << 10) | (rnCode << 5) | 0x1F);
    } else if (imm < (4096 << 12) && (imm & 0xFFF) == 0) {
        // 12ビットシフト付き即値
        emit(InstructionConstants::SUBS | sf | (1 << 22) | ((static_cast<uint32_t>(imm) >> 12) << 10) | (rnCode << 5) | 0x1F);
    } else {
        // 大きな即値の場合、レジスタを経由
        mov(Register::SCRATCH_REG0, imm);
        cmp(rn, Operand(Register::SCRATCH_REG0));
    }
}

// 分岐命令
void ARM64Assembler::bl(Label* label) {
    int offset = _buffer.size();
    emit(InstructionConstants::BL);
    
    // ラベルリンク記録
    if (label->isBound()) {
        // ラベルがすでにバインドされている場合は直接埋める
        int target = label->position();
        int delta = target - offset;
        uint32_t instr = _buffer[offset] | ((delta >> 2) & 0x3FFFFFF);
        
        // バッファに書き戻す
        uint8_t* bytes = reinterpret_cast<uint8_t*>(&instr);
        for (int i = 0; i < 4; i++) {
            _buffer[offset + i] = bytes[i];
        }
    } else {
        // ラベルがまだバインドされていない場合は参照を記録
        label->addReference(offset, [this, offset](int target) {
            int delta = target - offset;
            uint32_t instr;
            memcpy(&instr, &_buffer[offset], 4);
            instr |= ((delta >> 2) & 0x3FFFFFF);
            
            // バッファに書き戻す
            uint8_t* bytes = reinterpret_cast<uint8_t*>(&instr);
            for (int i = 0; i < 4; i++) {
                _buffer[offset + i] = bytes[i];
            }
        });
    }
}

void ARM64Assembler::br(Register rn) {
    uint32_t rnCode = encodeReg(rn);
    emit(InstructionConstants::BR | (rnCode << 5));
}

void ARM64Assembler::blr(Register rn) {
    uint32_t rnCode = encodeReg(rn);
    emit(InstructionConstants::BLR | (rnCode << 5));
}

void ARM64Assembler::ret(Register rn) {
    uint32_t rnCode = encodeReg(rn);
    emit(InstructionConstants::RET | (rnCode << 5));
}

// メモリアクセス
uint32_t ARM64Assembler::encodeLoadStore(uint8_t size, uint8_t op, Register rt, const MemOperand& operand) {
    uint32_t rtCode = encodeReg(rt);
    uint32_t rnCode = encodeReg(operand.getBase());
    
    if (operand.isRegisterOffset()) {
        // レジスタオフセット
        uint32_t rmCode = encodeReg(operand.getIndex());
        uint32_t option = 0;
        
        if (operand.isShiftExtend()) {
            // 拡張付きインデックス
            option = (static_cast<uint32_t>(operand.getExtend()) & 0x7) << 13;
        } else {
            // シフト付きインデックス
            option = (static_cast<uint32_t>(operand.getShift()) & 0x3) << 22;
        }
        
        uint32_t amount = operand.getShiftAmount() << 12;
        return (op | (size << 30) | option | amount | (1 << 21) | (operand.isShiftExtend() ? (1 << 11) : 0) | (rmCode << 16) | (rnCode << 5) | rtCode);
    } else {
        // 即値オフセット
        int32_t offset = operand.getOffset();
        
        if (operand.isPreIndex()) {
            // プレインデックス
            return (op | (size << 30) | (0b11 << 10) | ((offset & 0x1FF) << 12) | (rnCode << 5) | rtCode);
        } else if (operand.isPostIndex()) {
            // ポストインデックス
            return (op | (size << 30) | (0b01 << 10) | ((offset & 0x1FF) << 12) | (rnCode << 5) | rtCode);
        } else {
            // 通常のオフセット
            return (op | (size << 30) | (0b10 << 10) | ((offset & 0xFFF) << 10) | (rnCode << 5) | rtCode);
        }
    }
}

void ARM64Assembler::str(Register rt, const MemOperand& operand) {
    bool is64Bit = static_cast<uint32_t>(rt) < 32 || static_cast<uint32_t>(rt) >= 64;
    uint8_t size = is64Bit ? 3 : 2;
    emit(encodeLoadStore(size, InstructionConstants::STR_IMM, rt, operand));
}

// その他の一般的な命令実装...

}  // namespace arm64
}  // namespace core
}  // namespace aerojs 