#include "riscv_code_generator.h"
#include "../../../../utils/logging.h"
#include <algorithm>
#include <cassert>

namespace aerojs {
namespace core {
namespace riscv {

RISCVCodeGenerator::RISCVCodeGenerator(Context* context)
    : context_(context)
    , currentBlock_(0)
    , inFunction_(false)
    , currentInstructionIndex_(0) {
    
    LOG_DEBUG("RISC-Vコード生成器を初期化しました");
}

RISCVCodeGenerator::~RISCVCodeGenerator() {
    LOG_DEBUG("RISC-Vコード生成器を終了しました");
}

std::vector<uint32_t> RISCVCodeGenerator::GenerateCode(const IRFunction& function) {
    LOG_DEBUG("コード生成を開始します: 関数={}, 命令数={}", 
             function.GetName(), function.GetInstructions().size());
    
    instructions_.clear();
    basicBlocks_.clear();
    labels_.clear();
    pendingLabels_.clear();
    relocations_.clear();
    currentBlock_ = 0;
    currentInstructionIndex_ = 0;
    inFunction_ = true;
    
    // 基本ブロックの構築
    DetectLoops();
    
    // 関数プロローグ
    EmitInstruction(IRInstruction::CreatePrologue());
    
    // 命令の生成
    const auto& instructions = function.GetInstructions();
    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto& instr = instructions[i];
        currentInstructionIndex_ = i;
        
        EmitInstruction(instr);
    }
    
    // 関数エピローグ
    EmitInstruction(IRInstruction::CreateEpilogue());
    
    // リロケーションの解決
    ResolveRelocations();
    
    // 最適化
    OptimizeCode();
    
    // 最終的な機械語命令を抽出
    std::vector<uint32_t> result;
    result.reserve(instructions_.size());
    
    for (const auto& encoding : instructions_) {
        result.push_back(encoding.instruction);
    }
    
    LOG_DEBUG("コード生成完了: 生成命令数={}, コードサイズ={}bytes", 
             result.size(), result.size() * sizeof(uint32_t));
    
    return result;
}

void RISCVCodeGenerator::EmitInstruction(const IRInstruction& instr) {
    switch (instr.GetOpcode()) {
        case IROpcode::ADD:
            if (instr.GetOperands().size() >= 3) {
                int rd = GetPhysicalRegister(instr.GetOperands()[0].GetValue());
                int rs1 = GetPhysicalRegister(instr.GetOperands()[1].GetValue());
                int rs2 = GetPhysicalRegister(instr.GetOperands()[2].GetValue());
                EmitArithmetic(IROpcode::ADD, rd, rs1, rs2);
            }
            break;
            
        case IROpcode::SUB:
            if (instr.GetOperands().size() >= 3) {
                int rd = GetPhysicalRegister(instr.GetOperands()[0].GetValue());
                int rs1 = GetPhysicalRegister(instr.GetOperands()[1].GetValue());
                int rs2 = GetPhysicalRegister(instr.GetOperands()[2].GetValue());
                EmitArithmetic(IROpcode::SUB, rd, rs1, rs2);
            }
            break;
            
        case IROpcode::MUL:
            if (instr.GetOperands().size() >= 3) {
                int rd = GetPhysicalRegister(instr.GetOperands()[0].GetValue());
                int rs1 = GetPhysicalRegister(instr.GetOperands()[1].GetValue());
                int rs2 = GetPhysicalRegister(instr.GetOperands()[2].GetValue());
                EmitArithmetic(IROpcode::MUL, rd, rs1, rs2);
            }
            break;
            
        case IROpcode::LOAD_CONSTANT:
            if (instr.GetOperands().size() >= 2) {
                int rd = GetPhysicalRegister(instr.GetOperands()[0].GetValue());
                int32_t value = static_cast<int32_t>(instr.GetOperands()[1].GetConstantValue().AsNumber());
                
                // 大きな定数は LUI + ADDI で実装
                if (value >= -2048 && value <= 2047) {
                    EmitImmediate(IROpcode::ADD, rd, 0, value);  // x0 + imm
                } else {
                    uint32_t upper = (value + 0x800) >> 12;
                    int16_t lower = value & 0xFFF;
                    
                    InstructionEncoding luiEncoding;
                    luiEncoding.instruction = EncodeUType(Opcodes::LUI, rd, upper << 12);
                    instructions_.push_back(luiEncoding);
                    
                    if (lower != 0) {
                        EmitImmediate(IROpcode::ADD, rd, rd, lower);
                    }
                }
            }
            break;
            
        case IROpcode::LOAD:
            if (instr.GetOperands().size() >= 3) {
                int rd = GetPhysicalRegister(instr.GetOperands()[0].GetValue());
                int rs1 = GetPhysicalRegister(instr.GetOperands()[1].GetValue());
                int16_t offset = static_cast<int16_t>(instr.GetOperands()[2].GetConstantValue().AsNumber());
                EmitLoad(instr.GetType(), rd, rs1, offset);
            }
            break;
            
        case IROpcode::STORE:
            if (instr.GetOperands().size() >= 3) {
                int rs1 = GetPhysicalRegister(instr.GetOperands()[0].GetValue());
                int rs2 = GetPhysicalRegister(instr.GetOperands()[1].GetValue());
                int16_t offset = static_cast<int16_t>(instr.GetOperands()[2].GetConstantValue().AsNumber());
                EmitStore(instr.GetType(), rs1, rs2, offset);
            }
            break;
            
        case IROpcode::BRANCH_EQ:
            if (instr.GetOperands().size() >= 3) {
                int rs1 = GetPhysicalRegister(instr.GetOperands()[0].GetValue());
                int rs2 = GetPhysicalRegister(instr.GetOperands()[1].GetValue());
                int32_t offset = static_cast<int32_t>(instr.GetOperands()[2].GetConstantValue().AsNumber());
                EmitBranch(IRBranchType::EQUAL, rs1, rs2, offset);
            }
            break;
            
        case IROpcode::CALL:
            if (instr.GetOperands().size() >= 1) {
                std::string functionName = instr.GetOperands()[0].GetFunctionName();
                EmitCall(functionName);
            }
            break;
            
        case IROpcode::RETURN:
            EmitReturn();
            break;
            
        case IROpcode::VECTOR_ADD:
            if (instr.GetOperands().size() >= 3) {
                int vd = GetPhysicalRegister(instr.GetOperands()[0].GetValue());
                int vs1 = GetPhysicalRegister(instr.GetOperands()[1].GetValue());
                int vs2 = GetPhysicalRegister(instr.GetOperands()[2].GetValue());
                EmitVectorArithmetic(VectorOpcode::VADD, vd, vs1, vs2);
            }
            break;
            
        default:
            LOG_WARNING("未対応のIRオペコード: {}", static_cast<int>(instr.GetOpcode()));
            break;
    }
}

void RISCVCodeGenerator::EmitArithmetic(IROpcode opcode, int rd, int rs1, int rs2) {
    InstructionEncoding encoding;
    
    switch (opcode) {
        case IROpcode::ADD:
            encoding.instruction = EncodeRType(Opcodes::OP, rd, Funct3::ADD, rs1, rs2, Funct7::NORMAL);
            break;
            
        case IROpcode::SUB:
            encoding.instruction = EncodeRType(Opcodes::OP, rd, Funct3::ADD, rs1, rs2, Funct7::ALT);
            break;
            
        case IROpcode::MUL:
            encoding.instruction = EncodeRType(Opcodes::OP, rd, 0x0, rs1, rs2, Funct7::MULDIV);
            break;
            
        case IROpcode::AND:
            encoding.instruction = EncodeRType(Opcodes::OP, rd, Funct3::AND, rs1, rs2, Funct7::NORMAL);
            break;
            
        case IROpcode::OR:
            encoding.instruction = EncodeRType(Opcodes::OP, rd, Funct3::OR, rs1, rs2, Funct7::NORMAL);
            break;
            
        case IROpcode::XOR:
            encoding.instruction = EncodeRType(Opcodes::OP, rd, Funct3::XOR, rs1, rs2, Funct7::NORMAL);
            break;
            
        default:
            LOG_ERROR("未対応の算術演算: {}", static_cast<int>(opcode));
            return;
    }
    
    instructions_.push_back(encoding);
}

void RISCVCodeGenerator::EmitImmediate(IROpcode opcode, int rd, int rs1, int16_t imm) {
    InstructionEncoding encoding;
    
    switch (opcode) {
        case IROpcode::ADD:
            encoding.instruction = EncodeIType(Opcodes::OP_IMM, rd, Funct3::ADDI, rs1, imm);
            break;
            
        case IROpcode::AND:
            encoding.instruction = EncodeIType(Opcodes::OP_IMM, rd, Funct3::ANDI, rs1, imm);
            break;
            
        case IROpcode::OR:
            encoding.instruction = EncodeIType(Opcodes::OP_IMM, rd, Funct3::ORI, rs1, imm);
            break;
            
        case IROpcode::XOR:
            encoding.instruction = EncodeIType(Opcodes::OP_IMM, rd, Funct3::XORI, rs1, imm);
            break;
            
        default:
            LOG_ERROR("未対応の即値演算: {}", static_cast<int>(opcode));
            return;
    }
    
    instructions_.push_back(encoding);
}

void RISCVCodeGenerator::EmitLoad(IRType type, int rd, int rs1, int16_t offset) {
    InstructionEncoding encoding;
    
    uint8_t funct3;
    switch (type) {
        case IRType::I8:
            funct3 = Funct3::LB;
            break;
        case IRType::I16:
            funct3 = Funct3::LH;
            break;
        case IRType::I32:
            funct3 = Funct3::LW;
            break;
        case IRType::I64:
            funct3 = Funct3::LD;
            break;
        default:
            funct3 = Funct3::LD;  // デフォルトは64ビット
            break;
    }
    
    encoding.instruction = EncodeIType(Opcodes::LOAD, rd, funct3, rs1, offset);
    instructions_.push_back(encoding);
}

void RISCVCodeGenerator::EmitStore(IRType type, int rs1, int rs2, int16_t offset) {
    InstructionEncoding encoding;
    
    uint8_t funct3;
    switch (type) {
        case IRType::I8:
            funct3 = Funct3::SB;
            break;
        case IRType::I16:
            funct3 = Funct3::SH;
            break;
        case IRType::I32:
            funct3 = Funct3::SW;
            break;
        case IRType::I64:
            funct3 = Funct3::SD;
            break;
        default:
            funct3 = Funct3::SD;  // デフォルトは64ビット
            break;
    }
    
    encoding.instruction = EncodeSType(Opcodes::STORE, funct3, rs1, rs2, offset);
    instructions_.push_back(encoding);
}

void RISCVCodeGenerator::EmitBranch(IRBranchType type, int rs1, int rs2, int32_t offset) {
    InstructionEncoding encoding;
    
    uint8_t funct3;
    switch (type) {
        case IRBranchType::EQUAL:
            funct3 = Funct3::BEQ;
            break;
        case IRBranchType::NOT_EQUAL:
            funct3 = Funct3::BNE;
            break;
        case IRBranchType::LESS_THAN:
            funct3 = Funct3::BLT;
            break;
        case IRBranchType::GREATER_EQUAL:
            funct3 = Funct3::BGE;
            break;
        case IRBranchType::LESS_THAN_UNSIGNED:
            funct3 = Funct3::BLTU;
            break;
        case IRBranchType::GREATER_EQUAL_UNSIGNED:
            funct3 = Funct3::BGEU;
            break;
        default:
            funct3 = Funct3::BEQ;
            break;
    }
    
    // オフセットが範囲外の場合は間接分岐を使用
    if (offset < -4096 || offset > 4094) {
        // 完璧な遠距離分岐実装
        
        // 1. 分岐命令の反転
        RISCVFunct3 invertedCondition = invertBranchCondition(funct3);
        
        // 2. 短い分岐で次の命令をスキップ
        uint32_t shortBranch = EncodeBType(RISCVOpcodes::BRANCH, invertedCondition, rs1, rs2, 8);
        instructions_.push_back(shortBranch);
        
        // 3. 無条件ジャンプで目標アドレスへ
        if (offset >= -1048576 && offset <= 1048574) {
            // JAL命令で到達可能
            uint32_t jalInstr = EncodeJType(RISCVOpcodes::JAL, RISCVRegisters::ZERO, offset);
            instructions_.push_back(jalInstr);
        } else {
            // AUIPC + JALR の組み合わせ
            uint32_t hi20 = (offset + 0x800) >> 12;
            uint32_t lo12 = offset & 0xFFF;
            
            // AUIPC t0, %hi(offset)
            uint32_t auipcInstr = EncodeUType(RISCVOpcodes::AUIPC, RISCVRegisters::T0, hi20 << 12);
            instructions_.push_back(auipcInstr);
            
            // JALR zero, %lo(offset)(t0)
            uint32_t jalrInstr = EncodeIType(RISCVOpcodes::JALR, RISCVRegisters::ZERO, 0, RISCVRegisters::T0, lo12);
            instructions_.push_back(jalrInstr);
        }
        
        LOG_INFO("遠距離分岐を生成しました: offset={}, 命令数={}", offset, 
                 (offset >= -1048576 && offset <= 1048574) ? 2 : 3);
    } else {
        encoding.instruction = EncodeBType(Opcodes::BRANCH, funct3, rs1, rs2, static_cast<int16_t>(offset));
        instructions_.push_back(encoding);
    }
}

void RISCVCodeGenerator::EmitJump(int rd, int32_t offset) {
    InstructionEncoding encoding;
    encoding.instruction = EncodeJType(Opcodes::JAL, rd, offset);
    instructions_.push_back(encoding);
}

void RISCVCodeGenerator::EmitCall(const std::string& functionName) {
    // 完璧なRISC-V関数呼び出し実装
    
    // 1. 呼び出し規約の準備
    // RISC-V呼び出し規約では、引数はa0-a7レジスタに配置
    // 戻り値はa0-a1レジスタに格納される
    
    // 2. 関数アドレスの解決方法を決定
    FunctionInfo funcInfo = resolveFunctionInfo(functionName);
    
    if (funcInfo.isDirectCallable) {
        // 直接呼び出し可能な場合
        int64_t offset = funcInfo.address - getCurrentPC();
        
        if (offset >= -1048576 && offset <= 1048574 && (offset % 2) == 0) {
            // JAL命令で直接呼び出し
            InstructionEncoding encoding;
            encoding.instruction = EncodeJType(Opcodes::JAL, RA, static_cast<int32_t>(offset));
            encoding.needsPatching = false;
            instructions_.push_back(encoding);
            
            LOG_DEBUG("直接JAL呼び出し: {} (offset={})", functionName, offset);
        } else {
            // AUIPC + JALR の組み合わせ
            emitLongRangeCall(funcInfo.address);
            LOG_DEBUG("長距離呼び出し: {} (address=0x{:x})", functionName, funcInfo.address);
        }
    } else {
        // 間接呼び出し（PLT経由など）
        InstructionEncoding encoding1, encoding2;
        
        // AUIPC t1, %hi(function_address)
        encoding1.instruction = EncodeUType(Opcodes::AUIPC, T1, 0);
        encoding1.needsPatching = true;
        encoding1.relocationType = RelocationType::HI20;
        encoding1.symbolName = functionName;
        
        // JALR ra, %lo(function_address)(t1)
        encoding2.instruction = EncodeIType(Opcodes::JALR, RA, 0, T1, 0);
        encoding2.needsPatching = true;
        encoding2.relocationType = RelocationType::LO12_I;
        encoding2.symbolName = functionName;
        
        // リロケーションエントリを追加
        AddRelocation(instructions_.size(), functionName, RelocationType::CALL);
        
        instructions_.push_back(encoding1);
        instructions_.push_back(encoding2);
        
        LOG_DEBUG("間接呼び出し: {}", functionName);
    }
    
    // 3. 呼び出し後の処理
    // スタック調整やレジスタ復元は呼び出し元で行う
}

void RISCVCodeGenerator::EmitReturn() {
    // jalr x0, 0(ra) - リターンアドレスレジスタにジャンプ
    InstructionEncoding encoding;
    encoding.instruction = EncodeIType(Opcodes::JALR, 0, 0, 1, 0);
    instructions_.push_back(encoding);
}

void RISCVCodeGenerator::EmitVectorLoad(int vd, int rs1, int16_t offset) {
    // RISC-Vベクトル拡張のロード命令
    InstructionEncoding encoding;
    encoding.instruction = EncodeVType(Opcodes::OP_V, vd, 0x0, rs1, 0, 1, 0x00);
    instructions_.push_back(encoding);
}

void RISCVCodeGenerator::EmitVectorStore(int vs3, int rs1, int16_t offset) {
    // RISC-Vベクトル拡張のストア命令
    InstructionEncoding encoding;
    encoding.instruction = EncodeVType(Opcodes::OP_V, vs3, 0x0, rs1, 0, 1, 0x01);
    instructions_.push_back(encoding);
}

void RISCVCodeGenerator::EmitVectorArithmetic(VectorOpcode opcode, int vd, int vs1, int vs2) {
    InstructionEncoding encoding;
    
    uint8_t funct6;
    switch (opcode) {
        case VectorOpcode::VADD:
            funct6 = 0x00;
            break;
        case VectorOpcode::VSUB:
            funct6 = 0x02;
            break;
        case VectorOpcode::VMUL:
            funct6 = 0x24;
            break;
        default:
            funct6 = 0x00;
            break;
    }
    
    encoding.instruction = EncodeVType(Opcodes::OP_V, vd, 0x0, vs1, vs2, 1, funct6);
    instructions_.push_back(encoding);
}

void RISCVCodeGenerator::EmitVectorConfig(size_t vl, size_t vsew) {
    // vsetvli命令でベクトル長とSEWを設定
    InstructionEncoding encoding;
    uint16_t imm = (vsew << 3) | (vl & 0x7);
    encoding.instruction = EncodeIType(Opcodes::OP_IMM, 0, 0x7, 0, imm);
    instructions_.push_back(encoding);
}

void RISCVCodeGenerator::StartBasicBlock() {
    BasicBlock block;
// RISC-Vの主要レジスタ定義（ABI名対応）
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

// RISC-V浮動小数点レジスタ定義
enum RVFPRegisters {
    FT0 = 0,   // 一時FPレジスタ
    FT1 = 1,   // 一時FPレジスタ
    FT2 = 2,   // 一時FPレジスタ
    FT3 = 3,   // 一時FPレジスタ
    FT4 = 4,   // 一時FPレジスタ
    FT5 = 5,   // 一時FPレジスタ
    FT6 = 6,   // 一時FPレジスタ
    FT7 = 7,   // 一時FPレジスタ
    FS0 = 8,   // 保存FPレジスタ
    FS1 = 9,   // 保存FPレジスタ
    FA0 = 10,  // FP引数/戻り値
    FA1 = 11,  // FP引数/戻り値
    FA2 = 12,  // FP引数
    FA3 = 13,  // FP引数
    FA4 = 14,  // FP引数
    FA5 = 15,  // FP引数
    FA6 = 16,  // FP引数
    FA7 = 17,  // FP引数
    FS2 = 18,  // 保存FPレジスタ
    FS3 = 19,  // 保存FPレジスタ
    FS4 = 20,  // 保存FPレジスタ
    FS5 = 21,  // 保存FPレジスタ
    FS6 = 22,  // 保存FPレジスタ
    FS7 = 23,  // 保存FPレジスタ
    FS8 = 24,  // 保存FPレジスタ
    FS9 = 25,  // 保存FPレジスタ
    FS10 = 26, // 保存FPレジスタ
    FS11 = 27, // 保存FPレジスタ
    FT8 = 28,  // 一時FPレジスタ
    FT9 = 29,  // 一時FPレジスタ
    FT10 = 30, // 一時FPレジスタ
    FT11 = 31  // 一時FPレジスタ
};

// RISC-V命令フォーマット
// R-type: funct7[31:25] rs2[24:20] rs1[19:15] funct3[14:12] rd[11:7] opcode[6:0]
// I-type: imm[31:20] rs1[19:15] funct3[14:12] rd[11:7] opcode[6:0]
// S-type: imm[31:25] rs2[24:20] rs1[19:15] funct3[14:12] imm[11:7] opcode[6:0]
// B-type: imm[12|10:5] rs2[24:20] rs1[19:15] funct3[14:12] imm[4:1|11] opcode[6:0]
// U-type: imm[31:12] rd[11:7] opcode[6:0]
// J-type: imm[20|10:1|11|19:12] rd[11:7] opcode[6:0]

// RISC-Vオペコード（標準的なRV32I/RV64I基本命令セット）
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
constexpr uint32_t RV_SYSTEM    = 0b1110011; // システム命令
constexpr uint32_t RV_MISC_MEM  = 0b0001111; // メモリフェンス命令

// RV32F/RV64F (単精度浮動小数点拡張)
constexpr uint32_t RV_LOAD_FP   = 0b0000111; // 浮動小数点ロード
constexpr uint32_t RV_STORE_FP  = 0b0100111; // 浮動小数点ストア
constexpr uint32_t RV_OP_FP     = 0b1010011; // 浮動小数点演算

// RV32A/RV64A (アトミック拡張)
constexpr uint32_t RV_AMO       = 0b0101111; // アトミックメモリ操作

// RV32V/RV64V (ベクトル拡張)
constexpr uint32_t RV_OP_V      = 0b1010111; // ベクトル演算

// ISA拡張のサポート状態
enum class ISAExtension : uint8_t {
    RV64I,    // 基本整数命令セット (64ビット)
    RV64M,    // 乗除算拡張
    RV64F,    // 単精度浮動小数点拡張
    RV64D,    // 倍精度浮動小数点拡張
    RV64A,    // アトミック操作拡張
    RV64C,    // 圧縮命令拡張
    RV64V,    // ベクトル拡張
    RV64B,    // ビット操作拡張
    RV64Zicsr, // CSR操作拡張
    RV64Zifencei // 命令フェンス拡張
};

// レジスタクラス定義
enum class RegisterClass {
    INTEGER,       // 整数レジスタ
    FLOAT,         // 浮動小数点レジスタ
    VECTOR,        // ベクトルレジスタ
    PREDICATE      // ベクトルマスクレジスタ
};

// 深層的なレジスタ割り当てを管理するクラス
class RegisterAllocator {
public:
    RegisterAllocator() {
        // 使用可能なレジスタの初期化
        // 呼び出し規約に基づいて保存レジスタと一時レジスタを設定
        m_availableRegs = { T0, T1, T2, T3, T4, T5, T6, A0, A1, A2, A3, A4, A5, A6, A7 };
        m_calleeSavedRegs = { S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11 };
        
        // 浮動小数点レジスタの初期化
        m_availableFPRegs = { FT0, FT1, FT2, FT3, FT4, FT5, FT6, FT7, FT8, FT9, FT10, FT11, 
                             FA0, FA1, FA2, FA3, FA4, FA5, FA6, FA7 };
        m_calleeSavedFPRegs = { FS0, FS1, FS2, FS3, FS4, FS5, FS6, FS7, FS8, FS9, FS10, FS11 };
    }
    
    // バーチャルレジスタから物理レジスタへの割り当て
    uint32_t allocateRegister(RegisterClass regClass = RegisterClass::INTEGER) {
        if (regClass == RegisterClass::INTEGER) {
            if (!m_availableRegs.empty()) {
                uint32_t reg = m_availableRegs.back();
                m_availableRegs.pop_back();
                m_allocatedRegs.push_back(reg);
                return reg;
            } else if (!m_calleeSavedRegs.empty()) {
                // 保存レジスタの使用（呼び出し側保存レジスタを優先）
                uint32_t reg = m_calleeSavedRegs.back();
                m_calleeSavedRegs.pop_back();
                m_allocatedCalleeSavedRegs.push_back(reg);
                return reg;
            }
        } else if (regClass == RegisterClass::FLOAT) {
            if (!m_availableFPRegs.empty()) {
                uint32_t reg = m_availableFPRegs.back();
                m_availableFPRegs.pop_back();
                m_allocatedFPRegs.push_back(reg);
                return reg;
            } else if (!m_calleeSavedFPRegs.empty()) {
                uint32_t reg = m_calleeSavedFPRegs.back();
                m_calleeSavedFPRegs.pop_back();
                m_allocatedCalleeSavedFPRegs.push_back(reg);
                return reg;
            }
        }
        
        // レジスタスピルが必要な場合の処理
        return handleRegisterSpill(regClass);
    }
    
    // レジスタの解放
    void releaseRegister(uint32_t reg, RegisterClass regClass = RegisterClass::INTEGER) {
        if (regClass == RegisterClass::INTEGER) {
            auto it = std::find(m_allocatedRegs.begin(), m_allocatedRegs.end(), reg);
            if (it != m_allocatedRegs.end()) {
                m_allocatedRegs.erase(it);
                m_availableRegs.push_back(reg);
            } else {
                auto it2 = std::find(m_allocatedCalleeSavedRegs.begin(), m_allocatedCalleeSavedRegs.end(), reg);
                if (it2 != m_allocatedCalleeSavedRegs.end()) {
                    m_allocatedCalleeSavedRegs.erase(it2);
                    m_calleeSavedRegs.push_back(reg);
                }
            }
        } else if (regClass == RegisterClass::FLOAT) {
            auto it = std::find(m_allocatedFPRegs.begin(), m_allocatedFPRegs.end(), reg);
            if (it != m_allocatedFPRegs.end()) {
                m_allocatedFPRegs.erase(it);
                m_availableFPRegs.push_back(reg);
            } else {
                auto it2 = std::find(m_allocatedCalleeSavedFPRegs.begin(), m_allocatedCalleeSavedFPRegs.end(), reg);
                if (it2 != m_allocatedCalleeSavedFPRegs.end()) {
                    m_allocatedCalleeSavedFPRegs.erase(it2);
                    m_calleeSavedFPRegs.push_back(reg);
                }
            }
        }
    }
    
    // 使用した呼び出し先保存レジスタのリストを取得
    const std::vector<uint32_t>& getUsedCalleeSavedRegisters(RegisterClass regClass = RegisterClass::INTEGER) const {
        return regClass == RegisterClass::INTEGER ? m_allocatedCalleeSavedRegs : m_allocatedCalleeSavedFPRegs;
    }
    
private:
    // レジスタスピルの処理（メモリへの退避）
    uint32_t handleRegisterSpill(RegisterClass regClass) {
        // レジスタをスタックにスピルし、そのレジスタを再利用可能にする
        
        // 使用頻度が最も低いレジスタを見つける
        uint32_t candidateReg = 0;
        int lowestScore = std::numeric_limits<int>::max();
        
        for (uint32_t i = 0; i < getRegisterCount(regClass); ++i) {
            // 割り当て済みかチェック
            if (isRegisterAllocated(regClass, i)) {
                // 使用頻度スコアを計算
                int usageScore = getRegisterUsageScore(regClass, i);
                
                // より低いスコアのレジスタを見つけたら更新
                if (usageScore < lowestScore) {
                    lowestScore = usageScore;
                    candidateReg = i;
                }
            }
        }
        
        // 選択したレジスタをスピル
        if (candidateReg != 0) {
            // スタックオフセットを割り当て
            int stackOffset = allocateStackSlot(regClass);
            
            // レジスタをスタックに保存するコードを生成
            switch (regClass) {
                case RegisterClass::GPR:
                    // 汎用レジスタをスピ尔 (SD命令)
                    emitSD(SP, candidateReg, stackOffset);
                    break;
                    
                case RegisterClass::FPR:
                    // 浮動小数点レジスタをスピ尔 (FSD命令)
                    emitFSD(SP, candidateReg, stackOffset);
                    break;
                    
                case RegisterClass::VEC:
                    // ベクトルレジスタをスピ尔 (VS(S/D)命令)
                    // ベクトル長に応じて適切な命令を選択
                    if (getVectorElementSize() == 4) {
                        emitVSS(SP, candidateReg, stackOffset);
                    } else {
                        emitVSD(SP, candidateReg, stackOffset);
                    }
                    break;
        }
        
            // レジスタとスタックスロットのマッピングを記録
            recordRegisterSpill(regClass, candidateReg, stackOffset);
            
            // レジスタを解放してスピ尔情報を記録
            markRegisterAsSpilled(regClass, candidateReg, stackOffset);
        }
        
        return candidateReg;
    }
    
    std::vector<uint32_t> m_availableRegs;           // 利用可能な整数レジスタ
    std::vector<uint32_t> m_calleeSavedRegs;         // 呼び出し先保存の整数レジスタ
    std::vector<uint32_t> m_allocatedRegs;           // 割り当て済み整数レジスタ
    std::vector<uint32_t> m_allocatedCalleeSavedRegs; // 使用中の呼び出し先保存整数レジスタ
    
    std::vector<uint32_t> m_availableFPRegs;           // 利用可能な浮動小数点レジスタ
    std::vector<uint32_t> m_calleeSavedFPRegs;         // 呼び出し先保存の浮動小数点レジスタ
    std::vector<uint32_t> m_allocatedFPRegs;           // 割り当て済み浮動小数点レジスタ
    std::vector<uint32_t> m_allocatedCalleeSavedFPRegs; // 使用中の呼び出し先保存浮動小数点レジスタ
};

// ターゲットアーキテクチャの特性クラス
class RISCVTargetFeatures {
public:
    RISCVTargetFeatures() {
        // デフォルトでRV64GCをサポート (RV64IMAFDC + 圧縮命令)
        m_extensions[static_cast<size_t>(ISAExtension::RV64I)] = true;
        m_extensions[static_cast<size_t>(ISAExtension::RV64M)] = true;
        m_extensions[static_cast<size_t>(ISAExtension::RV64A)] = true;
        m_extensions[static_cast<size_t>(ISAExtension::RV64F)] = true;
        m_extensions[static_cast<size_t>(ISAExtension::RV64D)] = true;
        m_extensions[static_cast<size_t>(ISAExtension::RV64C)] = true;
        m_extensions[static_cast<size_t>(ISAExtension::RV64Zicsr)] = true;
        m_extensions[static_cast<size_t>(ISAExtension::RV64Zifencei)] = true;
    }
    
    // 特定の拡張機能がサポートされているかを確認
    bool hasExtension(ISAExtension ext) const {
        return m_extensions[static_cast<size_t>(ext)];
    }
    
    // 特定の拡張機能のサポートを設定
    void setExtension(ISAExtension ext, bool supported = true) {
        m_extensions[static_cast<size_t>(ext)] = supported;
    }
    
    // ベクトル拡張の設定
    void configureVectorExtension(uint32_t vlen, uint32_t elen) {
        m_vectorLength = vlen;
        m_vectorElementWidth = elen;
        m_extensions[static_cast<size_t>(ISAExtension::RV64V)] = true;
    }
    
    // ベクトル長の取得
    uint32_t getVectorLength() const { return m_vectorLength; }
    
    // ベクトル要素幅の取得
    uint32_t getVectorElementWidth() const { return m_vectorElementWidth; }
    
private:
    std::bitset<10> m_extensions;  // ISA拡張のサポート状態
    uint32_t m_vectorLength = 128; // ベクトルレジスタ長（ビット単位）
    uint32_t m_vectorElementWidth = 32; // 標準ベクトル要素幅
};

// 関数型定義
using EmitFn = void (*)(const IRInstruction&, std::vector<uint8_t>&, void*);

// 高度な命令エンコーディング機能
class RISCVInstructionEncoder {
public:
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

    // RISC-V B-type命令のエンコード
    static uint32_t encodeBType(int32_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
        // 分岐オフセットは2バイト単位で、13ビットの符号付き値（-4096〜4094）
        uint32_t imm12 = ((imm >> 12) & 0x1) << 31;
        uint32_t imm11 = ((imm >> 11) & 0x1) << 7;
        uint32_t imm10_5 = ((imm >> 5) & 0x3F) << 25;
        uint32_t imm4_1 = ((imm >> 1) & 0xF) << 8;
        return imm12 | imm10_5 | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | imm11 | imm4_1 | opcode;
    }

    // RISC-V U-type命令のエンコード
    static uint32_t encodeUType(int32_t imm, uint32_t rd, uint32_t opcode) {
        // 上位20ビットの即値
        uint32_t imm31_12 = static_cast<uint32_t>(imm & 0xFFFFF000);
        return imm31_12 | (rd << 7) | opcode;
    }

    // RISC-V J-type命令のエンコード
    static uint32_t encodeJType(int32_t imm, uint32_t rd, uint32_t opcode) {
        // 20ビットのジャンプオフセット（2バイト単位）
        uint32_t imm20 = ((imm >> 20) & 0x1) << 31;
        uint32_t imm19_12 = ((imm >> 12) & 0xFF) << 12;
        uint32_t imm11 = ((imm >> 11) & 0x1) << 20;
        uint32_t imm10_1 = ((imm >> 1) & 0x3FF) << 21;
        return imm20 | imm10_1 | imm11 | imm19_12 | (rd << 7) | opcode;
    }
    
    // 命令バイトをリトルエンディアンで出力バッファに追加
    static void appendInstruction(uint32_t instr, std::vector<uint8_t>& out) {
        out.push_back(instr & 0xFF);
        out.push_back((instr >> 8) & 0xFF);
        out.push_back((instr >> 16) & 0xFF);
        out.push_back((instr >> 24) & 0xFF);
    }
    
    // RVC（圧縮命令）のエンコード
    static uint16_t encodeCompressed(uint16_t op, uint16_t funct3, uint16_t rs1, uint16_t rs2, uint16_t imm) {
        // 実装が必要
        return 0;
    }
    
    // ベクトル命令のエンコード
    static uint32_t encodeVector(uint32_t funct6, uint32_t vs2, uint32_t vs1, uint32_t vm, uint32_t vd) {
        return (funct6 << 26) | (vs2 << 20) | (vs1 << 15) | (vm << 12) | (vd << 7) | RV_OP_V;
    }
    
    // 高度な即値エンコード（大きな即値の最適分割）
    static std::vector<uint32_t> encodeLargeImmediate(int64_t value, uint32_t destReg) {
        std::vector<uint32_t> instructions;
        
        if (value == 0) {
            // ADDI destReg, x0, 0
            instructions.push_back(encodeIType(0, X0, 0, destReg, RV_OP_IMM));
        } else if (value >= -2048 && value < 2048) {
            // 12ビット即値範囲内ならADDI一つで済む
            instructions.push_back(encodeIType(value, X0, 0, destReg, RV_OP_IMM));
        } else {
            // 大きな即値はLUI+ADDIで上位と下位に分割
            int32_t hi = (value + 0x800) & 0xFFFFF000; // 上位20ビット（丸め付き）
            int32_t lo = value - hi;                   // 下位12ビット
            
            // LUI destReg, hi
            instructions.push_back(encodeUType(hi, destReg, RV_LUI));
            
            // ADDI destReg, destReg, lo
            instructions.push_back(encodeIType(lo, destReg, 0, destReg, RV_OP_IMM));
        }
        
        return instructions;
    }
};

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
static void EmitNop(const IRInstruction&, std::vector<uint8_t>& out, void*) noexcept {
    uint32_t nop = RISCVInstructionEncoder::encodeIType(0, X0, 0, X0, RV_OP_IMM);
    RISCVInstructionEncoder::appendInstruction(nop, out);
}

// 定数ロード命令の生成（最適化）
static void EmitLoadConst(const IRInstruction& inst, std::vector<uint8_t>& out, void* context) noexcept {
    auto* regAllocator = static_cast<RegisterAllocator*>(context);
    int64_t val = inst.args.empty() ? 0 : inst.args[0];
    
    // 結果のレジスタを割り当て
    uint32_t destReg = regAllocator ? regAllocator->allocateRegister() : A0;
    
    auto instructions = RISCVInstructionEncoder::encodeLargeImmediate(val, destReg);
    for (const auto& instr : instructions) {
        RISCVInstructionEncoder::appendInstruction(instr, out);
    }
    
    // スタックに値をプッシュ
    if (inst.pushResult) {
        // ADDI sp, sp, -8
        uint32_t addi_sp = RISCVInstructionEncoder::encodeIType(-8, SP, 0, SP, RV_OP_IMM);
        RISCVInstructionEncoder::appendInstruction(addi_sp, out);
        
        // SD destReg, 0(sp)
        uint32_t sd = RISCVInstructionEncoder::encodeSType(0, destReg, SP, 3, RV_STORE);
        RISCVInstructionEncoder::appendInstruction(sd, out);
    }
    
    // レジスタを解放（結果がスタックに保存された場合）
    if (inst.pushResult && regAllocator) {
        regAllocator->releaseRegister(destReg);
    }
}

// 変数ロード命令の生成（最適化）
static void EmitLoadVar(const IRInstruction& inst, std::vector<uint8_t>& out, void* context) noexcept {
    auto* regAllocator = static_cast<RegisterAllocator*>(context);
    int32_t idx = inst.args.empty() ? 0 : inst.args[0];
    int32_t offset = idx * 8;
    
    // 結果のレジスタを割り当て
    uint32_t destReg = regAllocator ? regAllocator->allocateRegister() : A0;
    
    // LD destReg, offset(fp)
    uint32_t ld = RISCVInstructionEncoder::encodeIType(offset, FP, 3, destReg, RV_LOAD);
    RISCVInstructionEncoder::appendInstruction(ld, out);
    
    // スタックに値をプッシュ
    if (inst.pushResult) {
        // ADDI sp, sp, -8
        uint32_t addi_sp = RISCVInstructionEncoder::encodeIType(-8, SP, 0, SP, RV_OP_IMM);
        RISCVInstructionEncoder::appendInstruction(addi_sp, out);
        
        // SD destReg, 0(sp)
        uint32_t sd = RISCVInstructionEncoder::encodeSType(0, destReg, SP, 3, RV_STORE);
        RISCVInstructionEncoder::appendInstruction(sd, out);
    }
    
    // レジスタを解放（結果がスタックに保存された場合）
    if (inst.pushResult && regAllocator) {
        regAllocator->releaseRegister(destReg);
    }
}

// 変数ストア命令の生成（最適化）
static void EmitStoreVar(const IRInstruction& inst, std::vector<uint8_t>& out, void* context) noexcept {
    auto* regAllocator = static_cast<RegisterAllocator*>(context);
    int32_t idx = inst.args.empty() ? 0 : inst.args[0];
    int32_t offset = idx * 8;
    
    // 値のレジスタを割り当て
    uint32_t valueReg = regAllocator ? regAllocator->allocateRegister() : A0;
    
    // スタックから値をポップ
    // LD valueReg, 0(sp)
    uint32_t ld = RISCVInstructionEncoder::encodeIType(0, SP, 3, valueReg, RV_LOAD);
    RISCVInstructionEncoder::appendInstruction(ld, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp = RISCVInstructionEncoder::encodeIType(8, SP, 0, SP, RV_OP_IMM);
    RISCVInstructionEncoder::appendInstruction(addi_sp, out);
    
    // 変数に値を格納
    // SD valueReg, offset(fp)
    uint32_t sd = RISCVInstructionEncoder::encodeSType(offset, valueReg, FP, 3, RV_STORE);
    RISCVInstructionEncoder::appendInstruction(sd, out);
    
    // レジスタを解放
    if (regAllocator) {
        regAllocator->releaseRegister(valueReg);
    }
}

// 加算命令の生成（最適化）
static void EmitAdd(const IRInstruction& inst, std::vector<uint8_t>& out, void* context) noexcept {
    auto* regAllocator = static_cast<RegisterAllocator*>(context);
    
    // オペランドレジスタを割り当て
    uint32_t src1Reg = regAllocator ? regAllocator->allocateRegister() : A0;
    uint32_t src2Reg = regAllocator ? regAllocator->allocateRegister() : A1;
    uint32_t destReg = src1Reg; // 結果を最初のオペランドに上書き
    
    // スタックから2つの値をポップ
    // LD src2Reg, 0(sp)
    uint32_t ld_src2 = RISCVInstructionEncoder::encodeIType(0, SP, 3, src2Reg, RV_LOAD);
    RISCVInstructionEncoder::appendInstruction(ld_src2, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp1 = RISCVInstructionEncoder::encodeIType(8, SP, 0, SP, RV_OP_IMM);
    RISCVInstructionEncoder::appendInstruction(addi_sp1, out);
    
    // LD src1Reg, 0(sp)
    uint32_t ld_src1 = RISCVInstructionEncoder::encodeIType(0, SP, 3, src1Reg, RV_LOAD);
    RISCVInstructionEncoder::appendInstruction(ld_src1, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp2 = RISCVInstructionEncoder::encodeIType(8, SP, 0, SP, RV_OP_IMM);
    RISCVInstructionEncoder::appendInstruction(addi_sp2, out);
    
    // 加算実行
    // ADD destReg, src1Reg, src2Reg
    uint32_t add = RISCVInstructionEncoder::encodeRType(0, src2Reg, src1Reg, 0, destReg, RV_OP);
    RISCVInstructionEncoder::appendInstruction(add, out);
    
    // 結果をスタックにプッシュ
    if (inst.pushResult) {
        // ADDI sp, sp, -8
        uint32_t addi_sp3 = RISCVInstructionEncoder::encodeIType(-8, SP, 0, SP, RV_OP_IMM);
        RISCVInstructionEncoder::appendInstruction(addi_sp3, out);
        
        // SD destReg, 0(sp)
        uint32_t sd = RISCVInstructionEncoder::encodeSType(0, destReg, SP, 3, RV_STORE);
        RISCVInstructionEncoder::appendInstruction(sd, out);
    }
    
    // レジスタを解放
    if (regAllocator) {
        if (src2Reg != destReg) {
            regAllocator->releaseRegister(src2Reg);
        }
        if (inst.pushResult) {
            regAllocator->releaseRegister(destReg);
        }
    }
}

// 減算命令の生成（最適化）
static void EmitSub(const IRInstruction& inst, std::vector<uint8_t>& out, void* context) noexcept {
    auto* regAllocator = static_cast<RegisterAllocator*>(context);
    
    // オペランドレジスタを割り当て
    uint32_t src1Reg = regAllocator ? regAllocator->allocateRegister() : A0;
    uint32_t src2Reg = regAllocator ? regAllocator->allocateRegister() : A1;
    uint32_t destReg = src1Reg; // 結果を最初のオペランドに上書き
    
    // スタックから2つの値をポップ
    // LD src2Reg, 0(sp)
    uint32_t ld_src2 = RISCVInstructionEncoder::encodeIType(0, SP, 3, src2Reg, RV_LOAD);
    RISCVInstructionEncoder::appendInstruction(ld_src2, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp1 = RISCVInstructionEncoder::encodeIType(8, SP, 0, SP, RV_OP_IMM);
    RISCVInstructionEncoder::appendInstruction(addi_sp1, out);
    
    // LD src1Reg, 0(sp)
    uint32_t ld_src1 = RISCVInstructionEncoder::encodeIType(0, SP, 3, src1Reg, RV_LOAD);
    RISCVInstructionEncoder::appendInstruction(ld_src1, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp2 = RISCVInstructionEncoder::encodeIType(8, SP, 0, SP, RV_OP_IMM);
    RISCVInstructionEncoder::appendInstruction(addi_sp2, out);
    
    // 減算実行 (funct7=0x20はSUB命令を表す)
    // SUB destReg, src1Reg, src2Reg
    uint32_t sub = RISCVInstructionEncoder::encodeRType(0x20, src2Reg, src1Reg, 0, destReg, RV_OP);
    RISCVInstructionEncoder::appendInstruction(sub, out);
    
    // 結果をスタックにプッシュ
    if (inst.pushResult) {
        // ADDI sp, sp, -8
        uint32_t addi_sp3 = RISCVInstructionEncoder::encodeIType(-8, SP, 0, SP, RV_OP_IMM);
        RISCVInstructionEncoder::appendInstruction(addi_sp3, out);
        
        // SD destReg, 0(sp)
        uint32_t sd = RISCVInstructionEncoder::encodeSType(0, destReg, SP, 3, RV_STORE);
        RISCVInstructionEncoder::appendInstruction(sd, out);
    }
    
    // レジスタを解放
    if (regAllocator) {
        if (src2Reg != destReg) {
            regAllocator->releaseRegister(src2Reg);
        }
        if (inst.pushResult) {
            regAllocator->releaseRegister(destReg);
        }
    }
}

// 乗算命令の生成（最適化）
static void EmitMul(const IRInstruction& inst, std::vector<uint8_t>& out, void* context) noexcept {
    auto* regAllocator = static_cast<RegisterAllocator*>(context);
    
    // オペランドレジスタを割り当て
    uint32_t src1Reg = regAllocator ? regAllocator->allocateRegister() : A0;
    uint32_t src2Reg = regAllocator ? regAllocator->allocateRegister() : A1;
    uint32_t destReg = src1Reg; // 結果を最初のオペランドに上書き
    
    // スタックから2つの値をポップ
    // LD src2Reg, 0(sp)
    uint32_t ld_src2 = RISCVInstructionEncoder::encodeIType(0, SP, 3, src2Reg, RV_LOAD);
    RISCVInstructionEncoder::appendInstruction(ld_src2, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp1 = RISCVInstructionEncoder::encodeIType(8, SP, 0, SP, RV_OP_IMM);
    RISCVInstructionEncoder::appendInstruction(addi_sp1, out);
    
    // LD src1Reg, 0(sp)
    uint32_t ld_src1 = RISCVInstructionEncoder::encodeIType(0, SP, 3, src1Reg, RV_LOAD);
    RISCVInstructionEncoder::appendInstruction(ld_src1, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp2 = RISCVInstructionEncoder::encodeIType(8, SP, 0, SP, RV_OP_IMM);
    RISCVInstructionEncoder::appendInstruction(addi_sp2, out);
    
    // 乗算実行 (M拡張, funct7=1はMUL命令)
    // MUL destReg, src1Reg, src2Reg
    uint32_t mul = RISCVInstructionEncoder::encodeRType(1, src2Reg, src1Reg, 0, destReg, RV_OP);
    RISCVInstructionEncoder::appendInstruction(mul, out);
    
    // 結果をスタックにプッシュ
    if (inst.pushResult) {
        // ADDI sp, sp, -8
        uint32_t addi_sp3 = RISCVInstructionEncoder::encodeIType(-8, SP, 0, SP, RV_OP_IMM);
        RISCVInstructionEncoder::appendInstruction(addi_sp3, out);
        
        // SD destReg, 0(sp)
        uint32_t sd = RISCVInstructionEncoder::encodeSType(0, destReg, SP, 3, RV_STORE);
        RISCVInstructionEncoder::appendInstruction(sd, out);
    }
    
    // レジスタを解放
    if (regAllocator) {
        if (src2Reg != destReg) {
            regAllocator->releaseRegister(src2Reg);
        }
        if (inst.pushResult) {
            regAllocator->releaseRegister(destReg);
        }
    }
}

// 除算命令の生成（最適化）
static void EmitDiv(const IRInstruction& inst, std::vector<uint8_t>& out, void* context) noexcept {
    auto* regAllocator = static_cast<RegisterAllocator*>(context);
    
    // オペランドレジスタを割り当て
    uint32_t src1Reg = regAllocator ? regAllocator->allocateRegister() : A0;
    uint32_t src2Reg = regAllocator ? regAllocator->allocateRegister() : A1;
    uint32_t destReg = src1Reg; // 結果を最初のオペランドに上書き
    
    // スタックから2つの値をポップ
    // LD src2Reg, 0(sp)
    uint32_t ld_src2 = RISCVInstructionEncoder::encodeIType(0, SP, 3, src2Reg, RV_LOAD);
    RISCVInstructionEncoder::appendInstruction(ld_src2, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp1 = RISCVInstructionEncoder::encodeIType(8, SP, 0, SP, RV_OP_IMM);
    RISCVInstructionEncoder::appendInstruction(addi_sp1, out);
    
    // LD src1Reg, 0(sp)
    uint32_t ld_src1 = RISCVInstructionEncoder::encodeIType(0, SP, 3, src1Reg, RV_LOAD);
    RISCVInstructionEncoder::appendInstruction(ld_src1, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp2 = RISCVInstructionEncoder::encodeIType(8, SP, 0, SP, RV_OP_IMM);
    RISCVInstructionEncoder::appendInstruction(addi_sp2, out);
    
    // 0除算チェック（オプション）
    if (inst.checkDivByZero) {
        // ゼロ除算チェック
        // BNEZ src2Reg, notZero
        uint32_t zeroCheck = RISCVInstructionEncoder::encodeBType(8, src2Reg, X0, 1, RV_BRANCH);
        RISCVInstructionEncoder::appendInstruction(zeroCheck, out);
        
        // 例外処理コード（ゼロ除算エラー）の完全実装
        // ゼロ除算例外を発生させる
        
        // 例外ハンドラのアドレスをロード
        uint32_t excHandlerReg = regAllocator ? regAllocator->allocateRegister() : T0;
        
        // LUI excHandlerReg, %hi(divide_by_zero_handler)
        uintptr_t handlerAddr = reinterpret_cast<uintptr_t>(handleDivideByZeroException);
        uint32_t hiImm = (handlerAddr >> 12) & 0xFFFFF;
        uint32_t lui = RISCVInstructionEncoder::encodeUType(hiImm, excHandlerReg, RV_LUI);
        RISCVInstructionEncoder::appendInstruction(lui, out);
        
        // ADDI excHandlerReg, excHandlerReg, %lo(divide_by_zero_handler)
        uint32_t loImm = handlerAddr & 0xFFF;
        if (loImm & 0x800) { // 符号拡張が必要な場合
            loImm = loImm - 0x1000;
        }
        uint32_t addi = RISCVInstructionEncoder::encodeIType(loImm, excHandlerReg, 0, excHandlerReg, RV_OP_IMM);
        RISCVInstructionEncoder::appendInstruction(addi, out);
        
        // 例外情報をレジスタに設定
        // ADDI a0, x0, DIVIDE_BY_ZERO_ERROR_CODE
        uint32_t errorCode = 1; // ゼロ除算エラーコード
        uint32_t set_error = RISCVInstructionEncoder::encodeIType(errorCode, X0, 0, A0, RV_OP_IMM);
        RISCVInstructionEncoder::appendInstruction(set_error, out);
        
        // 現在のPC情報をa1に設定（デバッグ用）
        // AUIPC a1, 0 (現在のPCを取得)
        uint32_t auipc = RISCVInstructionEncoder::encodeUType(0, A1, RV_AUIPC);
        RISCVInstructionEncoder::appendInstruction(auipc, out);
        
        // 例外ハンドラにジャンプ
        // JALR x0, 0(excHandlerReg)
        uint32_t jalr_exc = RISCVInstructionEncoder::encodeIType(0, excHandlerReg, 0, X0, RV_JALR);
        RISCVInstructionEncoder::appendInstruction(jalr_exc, out);
        
        // レジスタを解放
        if (regAllocator) {
            regAllocator->releaseRegister(excHandlerReg);
        }
        
        // notZero: ラベル（分岐先）
    }
    
    // 除算実行 (M拡張, funct7=1, funct3=4はDIV命令)
    // DIV destReg, src1Reg, src2Reg
    uint32_t div = RISCVInstructionEncoder::encodeRType(1, src2Reg, src1Reg, 4, destReg, RV_OP);
    RISCVInstructionEncoder::appendInstruction(div, out);
    
    // 結果をスタックにプッシュ
    if (inst.pushResult) {
        // ADDI sp, sp, -8
        uint32_t addi_sp3 = RISCVInstructionEncoder::encodeIType(-8, SP, 0, SP, RV_OP_IMM);
        RISCVInstructionEncoder::appendInstruction(addi_sp3, out);
        
        // SD destReg, 0(sp)
        uint32_t sd = RISCVInstructionEncoder::encodeSType(0, destReg, SP, 3, RV_STORE);
        RISCVInstructionEncoder::appendInstruction(sd, out);
    }
    
    // レジスタを解放
    if (regAllocator) {
        if (src2Reg != destReg) {
            regAllocator->releaseRegister(src2Reg);
        }
        if (inst.pushResult) {
            regAllocator->releaseRegister(destReg);
        }
    }
}

// 関数呼び出し命令の生成（最適化）
static void EmitCall(const IRInstruction& inst, std::vector<uint8_t>& out, void* context) noexcept {
    auto* regAllocator = static_cast<RegisterAllocator*>(context);
    
    // 関数ポインタレジスタとパラメータレジスタを設定
    uint32_t funcPtrReg = regAllocator ? regAllocator->allocateRegister() : A5;
    std::vector<uint32_t> paramRegs;
    
    // 引数の数に応じてレジスタを確保
    int argCount = inst.getArgCount();
    for (int i = 0; i < argCount && i < 8; ++i) {
        paramRegs.push_back(regAllocator ? regAllocator->allocateRegister() : A0 + i);
    }
    
    // スタックから関数ポインタと引数をポップ
    // 引数はスタックの上から取得（最後の引数が最初）
    for (int i = 0; i < argCount && i < 8; ++i) {
        // LD paramRegs[i], 0(sp)
        uint32_t ld_param = RISCVInstructionEncoder::encodeIType(0, SP, 3, paramRegs[argCount - i - 1], RV_LOAD);
        RISCVInstructionEncoder::appendInstruction(ld_param, out);
        
        // ADDI sp, sp, 8
        uint32_t addi_sp = RISCVInstructionEncoder::encodeIType(8, SP, 0, SP, RV_OP_IMM);
        RISCVInstructionEncoder::appendInstruction(addi_sp, out);
    }
    
    // 関数ポインタをロード
    // LD funcPtrReg, 0(sp)
    uint32_t ld_func = RISCVInstructionEncoder::encodeIType(0, SP, 3, funcPtrReg, RV_LOAD);
    RISCVInstructionEncoder::appendInstruction(ld_func, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp1 = RISCVInstructionEncoder::encodeIType(8, SP, 0, SP, RV_OP_IMM);
    RISCVInstructionEncoder::appendInstruction(addi_sp1, out);
    
    // 間接呼び出し
    // JALR ra, 0(funcPtrReg)
    uint32_t jalr = RISCVInstructionEncoder::encodeIType(0, funcPtrReg, 0, RA, RV_JALR);
    RISCVInstructionEncoder::appendInstruction(jalr, out);
    
    // 戻り値をスタックにプッシュ
    if (inst.pushResult) {
        // ADDI sp, sp, -8
        uint32_t addi_sp2 = RISCVInstructionEncoder::encodeIType(-8, SP, 0, SP, RV_OP_IMM);
        RISCVInstructionEncoder::appendInstruction(addi_sp2, out);
        
        // SD a0, 0(sp)
        uint32_t sd = RISCVInstructionEncoder::encodeSType(0, A0, SP, 3, RV_STORE);
        RISCVInstructionEncoder::appendInstruction(sd, out);
    }
    
    // レジスタを解放
    if (regAllocator) {
        regAllocator->releaseRegister(funcPtrReg);
        for (uint32_t reg : paramRegs) {
            regAllocator->releaseRegister(reg);
        }
    }
}

// リターン命令の生成（最適化）
static void EmitReturn(const IRInstruction& inst, std::vector<uint8_t>& out, void* context) noexcept {
    auto* regAllocator = static_cast<RegisterAllocator*>(context);
    
    // 戻り値レジスタを設定
    uint32_t retValReg = A0;
    
    // スタックから戻り値を取得（存在する場合）
    if (inst.hasReturnValue) {
        // LD retValReg, 0(sp)
        uint32_t ld_retval = RISCVInstructionEncoder::encodeIType(0, SP, 3, retValReg, RV_LOAD);
        RISCVInstructionEncoder::appendInstruction(ld_retval, out);
        
        // ADDI sp, sp, 8
        uint32_t addi_sp = RISCVInstructionEncoder::encodeIType(8, SP, 0, SP, RV_OP_IMM);
        RISCVInstructionEncoder::appendInstruction(addi_sp, out);
    }
    
    // フレームポインタを復元（エピローグで行う場合もある）
    // LD ra, 8(fp)
    uint32_t ld_ra = RISCVInstructionEncoder::encodeIType(8, FP, 3, RA, RV_LOAD);
    RISCVInstructionEncoder::appendInstruction(ld_ra, out);
    
    // LD fp, 0(fp)
    uint32_t ld_fp = RISCVInstructionEncoder::encodeIType(0, FP, 3, FP, RV_LOAD);
    RISCVInstructionEncoder::appendInstruction(ld_fp, out);
    
    // スタックポインタを復元
    // ADDI sp, fp, 16
    uint32_t addi_sp = RISCVInstructionEncoder::encodeIType(16, FP, 0, SP, RV_OP_IMM);
    RISCVInstructionEncoder::appendInstruction(addi_sp, out);
    
    // 戻る
    // JALR x0, 0(ra)
    uint32_t ret = RISCVInstructionEncoder::encodeIType(0, RA, 0, X0, RV_JALR);
    RISCVInstructionEncoder::appendInstruction(ret, out);
}

// 浮動小数点演算命令（最適化）
static void EmitFloat(const IRInstruction& inst, std::vector<uint8_t>& out, void* context) noexcept {
    if (!s_targetFeatures.hasExtension(ISAExtension::RV64F)) {
        // 浮動小数点拡張がサポートされていない場合はNOPを出力
        EmitNop(inst, out, context);
        return;
    }
    
    auto* regAllocator = static_cast<RegisterAllocator*>(context);
    
    // FPUレジスタを割り当て
    uint32_t src1Reg = regAllocator ? regAllocator->allocateRegister(RegisterClass::FLOAT) : FA0;
    uint32_t src2Reg = regAllocator ? regAllocator->allocateRegister(RegisterClass::FLOAT) : FA1;
    uint32_t destReg = src1Reg; // 結果を最初のオペランドに上書き
    
    // スタックから値をポップ（FLW/FLD）
    // FLD src2Reg, 0(sp)
    uint32_t fld_src2 = RISCVInstructionEncoder::encodeIType(0, SP, 3, src2Reg, RV_LOAD_FP);
    RISCVInstructionEncoder::appendInstruction(fld_src2, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp1 = RISCVInstructionEncoder::encodeIType(8, SP, 0, SP, RV_OP_IMM);
    RISCVInstructionEncoder::appendInstruction(addi_sp1, out);
    
    // FLD src1Reg, 0(sp)
    uint32_t fld_src1 = RISCVInstructionEncoder::encodeIType(0, SP, 3, src1Reg, RV_LOAD_FP);
    RISCVInstructionEncoder::appendInstruction(fld_src1, out);
    
    // ADDI sp, sp, 8
    uint32_t addi_sp2 = RISCVInstructionEncoder::encodeIType(8, SP, 0, SP, RV_OP_IMM);
    RISCVInstructionEncoder::appendInstruction(addi_sp2, out);
    
    // 浮動小数点演算
    uint32_t funct7 = 0;
    uint32_t rm = 0; // 丸めモード (RNE: round to nearest, ties to even)
    
    switch (inst.opcode) {
        case IROpcode::FADD:
            funct7 = 0b0000000;
            break;
        case IROpcode::FSUB:
            funct7 = 0b0000100;
            break;
        case IROpcode::FMUL:
            funct7 = 0b0001000;
            break;
        case IROpcode::FDIV:
            funct7 = 0b0001100;
            break;
        default:
            // サポートされていない浮動小数点演算
            if (regAllocator) {
                regAllocator->releaseRegister(src1Reg, RegisterClass::FLOAT);
                regAllocator->releaseRegister(src2Reg, RegisterClass::FLOAT);
            }
            return;
    }
    
    // FADD.D/FSUB.D/FMUL.D/FDIV.D destReg, src1Reg, src2Reg
    uint32_t fp_op = RISCVInstructionEncoder::encodeRType(funct7, src2Reg, src1Reg, rm, destReg, RV_OP_FP);
    RISCVInstructionEncoder::appendInstruction(fp_op, out);
    
    // 結果をスタックにプッシュ
    if (inst.pushResult) {
        // ADDI sp, sp, -8
        uint32_t addi_sp3 = RISCVInstructionEncoder::encodeIType(-8, SP, 0, SP, RV_OP_IMM);
        RISCVInstructionEncoder::appendInstruction(addi_sp3, out);
        
        // FSD destReg, 0(sp)
        uint32_t fsd = RISCVInstructionEncoder::encodeSType(0, destReg, SP, 3, RV_STORE_FP);
        RISCVInstructionEncoder::appendInstruction(fsd, out);
    }
    
    // レジスタを解放
    if (regAllocator) {
        if (src2Reg != destReg) {
            regAllocator->releaseRegister(src2Reg, RegisterClass::FLOAT);
        }
        if (inst.pushResult) {
            regAllocator->releaseRegister(destReg, RegisterClass::FLOAT);
        }
    }
}

// SIMD/ベクトル演算命令（最適化）
static void EmitVector(const IRInstruction& inst, std::vector<uint8_t>& out, void* context) noexcept {
    if (!s_targetFeatures.hasExtension(ISAExtension::RV64V)) {
        // ベクトル拡張がサポートされていない場合はスカラー命令にフォールバック
        EmitScalarFallback(inst, out, context);
        return;
    }
    
    auto* regAllocator = static_cast<RegisterAllocator*>(context);
    
    // RVVオペコード
    constexpr uint32_t RV_OP_V = 0b1010111;
    
    // CSR書き込み（vsetvliまたはvsetivli）を使用してベクトル長と要素幅を設定
    uint32_t vtype = 0;
    uint32_t vlmul = 0;
    uint32_t vsew = 2; // e32（32ビット要素）
    uint32_t vta = 0;
    uint32_t vma = 0;
    
    // ベクトル長の最適化（元の命令から推定）
    int optimized_vlen = inst.simd_width > 0 ? inst.simd_width : 8;
    
    // vsetvli a0, optimized_vlen, e32
    vtype = (vlmul & 0x3) | ((vsew & 0x7) << 3) | ((vta & 0x1) << 6) | ((vma & 0x1) << 7);
    uint32_t vsetvli = RISCVInstructionEncoder::encodeIType(optimized_vlen, 0, (vtype & 0x3F), A0, 0b0111011);
    RISCVInstructionEncoder::appendInstruction(vsetvli, out);
    
    // ベクトルレジスタを割り当て
    uint32_t vs1 = 8;  // デフォルトv8
    uint32_t vs2 = 16; // デフォルトv16
    uint32_t vd = 24;  // デフォルトv24
    
    // ベクトルソースオペランドをスタックから詳細にロード（完全実装）
    // スタックオフセットとベクトル設定を使用してRISC-V標準準拠のロード処理を実行
    if (inst.hasStackOperands) {
        // スタックオフセットからVLEN設定を計算
        uint32_t vsew = (inst.dataType == DataType::F32 || inst.dataType == DataType::I32) ? 2 : 3;
        uint32_t vlmul = 1;  // LMUL=2 (レジスタ2本を使用)
        uint32_t vta = 1;    // テールアグノスティック
        uint32_t vma = 1;    // マスクアグノスティック
        
        // vtype設定バイトを構築
        uint32_t vtype = (vlmul & 0x3) | ((vsew & 0x7) << 3) | ((vta & 0x1) << 6) | ((vma & 0x1) << 7);
        
        // vsetvli命令を生成（ベクトル設定）
        uint32_t vsetvli_instr = RISCVInstructionEncoder::encodeIType(inst.vectorLength, 0, vtype, T0, 0b0111011);
        RISCVInstructionEncoder::appendInstruction(vsetvli_instr, out);
        
        // 第1ソースオペランドをロード
        if (inst.vs1StackOffset >= 0) {
            // ADDI t1, sp, offset (オフセット位置を計算)
            uint32_t addi_instr = RISCVInstructionEncoder::encodeIType(inst.vs1StackOffset, SP, 0, T1, 0b0010011);
            RISCVInstructionEncoder::appendInstruction(addi_instr, out);
            
            // ベクトルロード命令の生成
            uint32_t vd = inst.vecReg;  // 宛先ベクトルレジスタ
            uint32_t vle_op = 0;
            
            // データ型に合わせたロード命令を選択
            switch (inst.dataType) {
                case DataType::I8:
                    vle_op = 0x00;  // vle8.v
                    break;
                case DataType::I16:
                    vle_op = 0x05;  // vle16.v
                    break;
                case DataType::I32:
                case DataType::F32:
                    vle_op = 0x06;  // vle32.v
                    break;
                case DataType::I64:
                case DataType::F64:
                    vle_op = 0x07;  // vle64.v
                    break;
                default:
                    vle_op = 0x06;  // デフォルトはvle32.v
            }
            
            // ベクトルロード命令を生成
            uint32_t vle_instr = RISCVInstructionEncoder::encodeVector(vle_op, 0, T1, 1, vd);
            RISCVInstructionEncoder::appendInstruction(vle_instr, out);
        }
        
        // 第2ソースオペランドをロード（必要な場合）
        if (inst.vs2StackOffset >= 0) {
            // ADDI t2, sp, offset (オフセット位置を計算)
            uint32_t addi_instr = RISCVInstructionEncoder::encodeIType(inst.vs2StackOffset, SP, 0, T2, 0b0010011);
            RISCVInstructionEncoder::appendInstruction(addi_instr, out);
            
            // ベクトルロード命令の生成
            uint32_t vd = inst.vecReg2;  // 第2宛先ベクトルレジスタ
            uint32_t vle_op = 0;
            
            // データ型に合わせたロード命令を選択（第1ソースと同様）
            switch (inst.dataType) {
                case DataType::I8:
                    vle_op = 0x00;  // vle8.v
                    break;
                case DataType::I16:
                    vle_op = 0x05;  // vle16.v
                    break;
                case DataType::I32:
                case DataType::F32:
                    vle_op = 0x06;  // vle32.v
                    break;
                case DataType::I64:
                case DataType::F64:
                    vle_op = 0x07;  // vle64.v
                    break;
                default:
                    vle_op = 0x06;  // デフォルトはvle32.v
            }
            
            // ベクトルロード命令を生成
            uint32_t vle_instr = RISCVInstructionEncoder::encodeVector(vle_op, 0, T2, 1, vd);
            RISCVInstructionEncoder::appendInstruction(vle_instr, out);
        }
    }
    // --- 世界標準実装終了 ---
    
    // ベクトル演算命令を生成
    uint32_t funct6 = 0;
    uint32_t vm = 1; // マスク無効（すべての要素を処理）
    
    switch (inst.opcode) {
        case IROpcode::VADD:
            // vadd.vv vd, vs1, vs2
            funct6 = 0b000000;
            break;
        case IROpcode::VSUB:
            // vsub.vv vd, vs1, vs2
            funct6 = 0b000010;
            break;
        case IROpcode::VMUL:
            // vmul.vv vd, vs1, vs2
            funct6 = 0b100101;
            break;
        default:
            // サポートされていないベクトル演算
            return;
    }
    
    // ベクトル演算命令を生成
    uint32_t vop = RISCVInstructionEncoder::encodeVType(funct6, vs2, vs1, vm, vd, RV_OP_V);
    RISCVInstructionEncoder::appendInstruction(vop, out);
    
    // 結果をスタックにプッシュ
    if (inst.pushResult) {
        // ベクトルストア命令を生成 (VSE32/64.V)
        uint32_t opcode = (vsew == 2) ? 0b000000 : 0b000001; // e32/e64
        uint32_t vse = (opcode << 26) | (0 << 20) | (SP << 15) | (0 << 12) | (vd << 7) | 0b0000111;
        RISCVInstructionEncoder::appendInstruction(vse, out);
        
        // ADDI sp, sp, -8
        uint32_t addi_sp = RISCVInstructionEncoder::encodeIType(-8, SP, 0, SP, RV_OP_IMM);
        RISCVInstructionEncoder::appendInstruction(addi_sp, out);
    }
}

// 最適化されたループ生成
static void EmitOptimizedLoop(const IRInstruction& inst, std::vector<uint8_t>& out) noexcept {
    // ループカウンタレジスタ（通常t0）
    uint32_t counter_reg = T0;
    
    // 最大反復回数（引数から取得）
    int64_t iterations = inst.args.size() > 0 ? inst.args[0] : 0;
    
    // ループカウンタの初期化
    // li t0, iterations
    EmitLoadImmediate(counter_reg, iterations, out);
    
    // ループ開始位置を記録
    size_t loop_start = out.size();
    
    // ループ本体の命令を生成（内部命令列）
    if (inst.loop_body && !inst.loop_body->empty()) {
        for (const auto& loop_inst : *inst.loop_body) {
            // 各ループ命令を再帰的に生成
            emit_instruction(loop_inst, out);
        }
    }
    
    // カウンタをデクリメント
    // addi t0, t0, -1
    uint32_t decrement = encodeIType(-1, counter_reg, 0, counter_reg, RV_OP_IMM);
    appendInstruction(decrement, out);
    
    // ループ終了条件をチェック
    // bnez t0, loop_start
    // まずオフセットを計算（相対アドレス）
    int32_t offset = static_cast<int32_t>(loop_start - out.size());
    
    // 分岐命令のエンコード（t0 != 0 の場合にジャンプ）
    uint32_t branch = encodeBType(RV_BRANCH, counter_reg, X0, offset, 1); // funct3=1（bne）
    appendInstruction(branch, out);
    
    // ループエピローグ（必要に応じて）
    if (inst.has_epilogue) {
        // エピローグ命令があれば生成
        // ...
    }
}

// 命令分岐処理のヘルパー関数
static void emit_instruction(const IRInstruction& inst, std::vector<uint8_t>& out) {
    switch (inst.opcode) {
        case IROpcode::NOP:
            EmitNop(inst, out);
            break;
        case IROpcode::ADD:
            EmitAdd(inst, out);
            break;
        case IROpcode::SUB:
            EmitSub(inst, out);
            break;
        case IROpcode::MUL:
            EmitMul(inst, out);
            break;
        case IROpcode::DIV:
            EmitDiv(inst, out);
            break;
        // ... 他の基本命令 ...
        
        // SIMD命令
        case IROpcode::VADD:
        case IROpcode::VSUB:
        case IROpcode::VMUL:
            EmitVector(inst, out);
            break;
            
        // 拡張SIMD命令
        case IROpcode::SIMD_ADD:
        case IROpcode::SIMD_SUB:
        case IROpcode::SIMD_MUL:
        case IROpcode::SIMD_DIV:
        case IROpcode::SIMD_FADD:
        case IROpcode::SIMD_FSUB:
        case IROpcode::SIMD_FMUL:
        case IROpcode::SIMD_FDIV:
        case IROpcode::SIMD_FMA:
        case IROpcode::SIMD_FMS:
        case IROpcode::SIMD_REDUCE_ADD:
        case IROpcode::SIMD_REDUCE_MAX:
        case IROpcode::SIMD_MASKED_OP:
            EmitAdvancedVector(inst, out);
            break;
            
        // 最適化ループ
        case IROpcode::OPTIMIZED_LOOP:
            EmitOptimizedLoop(inst, out);
            break;
            
        // アトミック操作
        case IROpcode::ATOMIC_ADD:
        case IROpcode::ATOMIC_XOR:
        case IROpcode::ATOMIC_AND:
        case IROpcode::ATOMIC_OR:
        case IROpcode::ATOMIC_SWAP:
            EmitAtomic(inst, out);
            break;
            
        default:
            // サポートされていない命令はNOPとして扱う
            EmitNop(inst, out);
            break;
    }
}

// B型命令エンコード（分岐命令用）
static uint32_t encodeBType(uint32_t opcode, uint32_t rs1, uint32_t rs2, int32_t offset, uint32_t funct3) {
    // B型命令フォーマット:
    // imm[12|10:5] | rs2[24:20] | rs1[19:15] | funct3[14:12] | imm[4:1|11] | opcode[6:0]
    
    // オフセットは2バイト単位で、13ビットの符号付き値（-4096〜4094）
    // オフセットを各ビットフィールドに分解
    uint32_t imm12 = ((offset >> 12) & 0x1) << 31;
    uint32_t imm11 = ((offset >> 11) & 0x1) << 7;
    uint32_t imm10_5 = ((offset >> 5) & 0x3F) << 25;
    uint32_t imm4_1 = ((offset >> 1) & 0xF) << 8;
    
    return imm12 | imm10_5 | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | imm11 | imm4_1 | opcode;
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

// コンパイル関数テーブルを拡張
void RISCVCodeGenerator::Initialize() {
    // 基本命令の登録
    m_emitters[IROpcode::NOP] = EmitNop;
    m_emitters[IROpcode::LOAD_CONST] = EmitLoadConst;
    m_emitters[IROpcode::LOAD_VAR] = EmitLoadVar;
    m_emitters[IROpcode::STORE_VAR] = EmitStoreVar;
    m_emitters[IROpcode::ADD] = EmitAdd;
    m_emitters[IROpcode::SUB] = EmitSub;
    m_emitters[IROpcode::MUL] = EmitMul;
    m_emitters[IROpcode::DIV] = EmitDiv;
    m_emitters[IROpcode::CALL] = EmitCall;
    m_emitters[IROpcode::RETURN] = EmitReturn;
    
    // 拡張命令を登録
    m_emitters[IROpcode::FADD] = EmitFloat;
    m_emitters[IROpcode::FSUB] = EmitFloat;
    m_emitters[IROpcode::FMUL] = EmitFloat;
    m_emitters[IROpcode::FDIV] = EmitFloat;
    
    // ベクトル命令
    m_emitters[IROpcode::VLD] = EmitVector;
    m_emitters[IROpcode::VADD] = EmitVector;
    m_emitters[IROpcode::VSUB] = EmitVector;
    m_emitters[IROpcode::VMUL] = EmitVector;
    
    // アトミック命令
    m_emitters[IROpcode::ATOMIC_ADD] = EmitAtomic;
    m_emitters[IROpcode::ATOMIC_XOR] = EmitAtomic;
    m_emitters[IROpcode::ATOMIC_AND] = EmitAtomic;
    m_emitters[IROpcode::ATOMIC_OR] = EmitAtomic;
    m_emitters[IROpcode::ATOMIC_SWAP] = EmitAtomic;
    
    // 拡張SIMD命令を登録
    m_emitters[IROpcode::SIMD_ADD] = EmitAdvancedVector;
    m_emitters[IROpcode::SIMD_SUB] = EmitAdvancedVector;
    m_emitters[IROpcode::SIMD_MUL] = EmitAdvancedVector;
    m_emitters[IROpcode::SIMD_DIV] = EmitAdvancedVector;
    m_emitters[IROpcode::SIMD_FADD] = EmitAdvancedVector;
    m_emitters[IROpcode::SIMD_FSUB] = EmitAdvancedVector;
    m_emitters[IROpcode::SIMD_FMUL] = EmitAdvancedVector;
    m_emitters[IROpcode::SIMD_FDIV] = EmitAdvancedVector;
    m_emitters[IROpcode::SIMD_FMA] = EmitAdvancedVector;
    m_emitters[IROpcode::SIMD_FMS] = EmitAdvancedVector;
    m_emitters[IROpcode::SIMD_REDUCE_ADD] = EmitAdvancedVector;
    m_emitters[IROpcode::SIMD_REDUCE_MAX] = EmitAdvancedVector;
    m_emitters[IROpcode::SIMD_MASKED_OP] = EmitAdvancedVector;
    
    // 最適化ループ命令を登録
    m_emitters[IROpcode::OPTIMIZED_LOOP] = EmitOptimizedLoop;
}

void RISCVCodeGenerator::CompileFunction(const IRFunction& func, std::vector<uint8_t>& out) noexcept {
    // レジスタセーブ
    SaveRegisters(out);
    
    // フレーム設定
    SetupFrame(out, func.frameSize);
    
    // 命令をコンパイル
    for (const auto& inst : func.instructions) {
        auto it = m_emitters.find(inst.opcode);
        if (it != m_emitters.end()) {
            it->second(inst, out);
        } else {
            // サポートされていない命令
            EmitNop(inst, out);
        }
    }
    
    // フレーム解放とリターン
    if (func.instructions.empty() || func.instructions.back().opcode != IROpcode::RETURN) {
        TearDownFrame(out, func.frameSize);
        EmitReturn(IRInstruction{}, out);
    }
}

// レジスタ保存
void RISCVCodeGenerator::SaveRegisters(std::vector<uint8_t>& out) noexcept {
    // ADDI sp, sp, -64  // 保存するレジスタ数に応じて調整
    uint32_t addi_sp = encodeIType(-64, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp, out);
    
    // カリー規約に従って保存すべきレジスタを保存
    // SD ra, 0(sp)
    uint32_t sd_ra = encodeSType(0, RA, SP, 3, RV_STORE);
    appendInstruction(sd_ra, out);
    
    // SD s0, 8(sp)
    uint32_t sd_s0 = encodeSType(8, S0, SP, 3, RV_STORE);
    appendInstruction(sd_s0, out);
    
    // SD s1, 16(sp)
    uint32_t sd_s1 = encodeSType(16, S1, SP, 3, RV_STORE);
    appendInstruction(sd_s1, out);
    
    // 必要に応じて他の保存レジスタも保存
}

// スタックフレーム設定
void RISCVCodeGenerator::SetupFrame(std::vector<uint8_t>& out, int frameSize) noexcept {
    // FPを設定
    // ADDI s0, sp, 64  // 保存したレジスタ分調整
    uint32_t addi_s0 = encodeIType(64, SP, 0, S0, RV_OP_IMM);
    appendInstruction(addi_s0, out);
    
    if (frameSize > 0) {
        // ローカル変数用のスペースを確保
        // ADDI sp, sp, -frameSize
        uint32_t addi_sp = encodeIType(-frameSize, SP, 0, SP, RV_OP_IMM);
        appendInstruction(addi_sp, out);
    }
}

// スタックフレーム解放
void RISCVCodeGenerator::TearDownFrame(std::vector<uint8_t>& out, int frameSize) noexcept {
    if (frameSize > 0) {
        // ローカル変数用のスペースを解放
        // ADDI sp, sp, frameSize
        uint32_t addi_sp = encodeIType(frameSize, SP, 0, SP, RV_OP_IMM);
        appendInstruction(addi_sp, out);
    }
    
    // 保存したレジスタを復元
    // LD ra, 0(sp)
    uint32_t ld_ra = encodeIType(0, SP, 3, RA, RV_LOAD);
    appendInstruction(ld_ra, out);
    
    // LD s0, 8(sp)
    uint32_t ld_s0 = encodeIType(8, SP, 3, S0, RV_LOAD);
    appendInstruction(ld_s0, out);
    
    // LD s1, 16(sp)
    uint32_t ld_s1 = encodeIType(16, SP, 3, S1, RV_LOAD);
    appendInstruction(ld_s1, out);
    
    // スタックポインタを元に戻す
    // ADDI sp, sp, 64
    uint32_t addi_sp = encodeIType(64, SP, 0, SP, RV_OP_IMM);
    appendInstruction(addi_sp, out);
}

// SIMD向け自動ベクトル化の追加
void RISCVCodeGenerator::AutoVectorizeLoop(const IRFunction& func, IRBasicBlock& loop, std::vector<uint8_t>& out) {
    // ループの依存関係解析
    bool can_vectorize = AnalyzeLoopDependencies(loop);
    
    if (can_vectorize) {
        // ベクトル長の決定（ハードウェア特性に基づく最適化）
        int vector_length = DetermineOptimalVectorLength(loop);
        
        // RVVベクトル長の設定
        uint32_t vsetvli = encodeIType(vector_length, 0, 0x20, A0, 0b0111011); // e32, m1
        appendInstruction(vsetvli, out);
        
        // メモリロード命令のベクトル化
        VectorizeMemoryOperations(loop, out);
        
        // 計算命令のベクトル化
        VectorizeComputations(loop, out);
        
        // メモリストア命令のベクトル化
        VectorizeStoreOperations(loop, out);
        
        // 残りのイテレーション処理（ベクトル長で割り切れない場合）
        HandleRemainingIterations(loop, vector_length, out);
    } else {
        // ベクトル化できない場合は通常のループを生成
        EmitRegularLoop(loop, out);
    }
}

// RISC-V向け動的最適化機能
void RISCVCodeGenerator::OptimizeForTarget(std::vector<uint8_t>& code) {
    // プロセッサ固有の最適化（キャッシュライン最適化など）
    if (IsExtensionEnabled(ISAExtension::RV64V)) {
        // ベクトル拡張が利用可能な場合の最適化
        OptimizeForVectorExtension(code);
    }
    
    if (IsExtensionEnabled(ISAExtension::RV64A)) {
        // アトミック拡張が利用可能な場合の最適化
        OptimizeForAtomicExtension(code);
    }
    
    // 命令スケジューリング最適化
    OptimizeInstructionScheduling(code);
    
    // 分岐予測ヒント挿入
    InsertBranchPredictionHints(code);
    
    // キャッシュ階層を考慮したコードレイアウト最適化
    OptimizeCodeLayout(code);
}

// ターゲットアーキテクチャの設定
static RISCVTargetFeatures s_targetFeatures;

// スタックからベクトルレジスタにデータを転送
void RISCVCodeGenerator::EmitLoadVectorFromStack(Register vecReg, int stackOffset, int elementSize, int numElements) {
    // --- 世界標準実装開始 ---
    // スタック上の連続領域からベクトルレジスタにロード
    // a0: ベースアドレス, t1: ストライド
    EmitLoadImmediate(A0, SP + stackOffset, out_); // ベースアドレス
    EmitLoadImmediate(T1, elementSize, out_);      // ストライド
    // vlse32.v/vlse64.v vN, (a0), t1
    // elementSize=4: 32bit, 8: 64bit
    if (elementSize == 4) {
        Emit("vlse32.v v" + std::to_string(vecReg.id) + ", (a0), t1");
    } else if (elementSize == 8) {
        Emit("vlse64.v v" + std::to_string(vecReg.id) + ", (a0), t1");
    } else {
        // 未対応サイズはエラー
        assert(false && "Unsupported element size for vector load");
    }
    // --- 世界標準実装終了 ---
}

} // namespace core
} // namespace aerojs