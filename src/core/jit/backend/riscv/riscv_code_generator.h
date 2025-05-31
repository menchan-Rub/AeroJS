/**
 * @file riscv_code_generator.h
 * @brief RISC-V コード生成器
 * 
 * このファイルは、RISC-Vアーキテクチャ向けのコード生成器を定義します。
 * 機械語命令の生成、レジスタ管理、最適化を担当します。
 * 
 * @author AeroJS Team
 * @version 1.0.0
 * @copyright MIT License
 */

#ifndef AEROJS_RISCV_CODE_GENERATOR_H
#define AEROJS_RISCV_CODE_GENERATOR_H

#include "../../ir/ir_instruction.h"
#include "../../ir/ir_function.h"
#include "../../../context.h"
#include <vector>
#include <unordered_map>
#include <memory>

namespace aerojs {
namespace core {
namespace riscv {

// 命令エンコーディング情報
struct InstructionEncoding {
    uint32_t instruction;
    std::vector<size_t> relocations;
    bool needsPatching;
    
    InstructionEncoding() : instruction(0), needsPatching(false) {}
};

// 基本ブロック情報
struct BasicBlock {
    size_t startIndex;
    size_t endIndex;
    std::vector<size_t> predecessors;
    std::vector<size_t> successors;
    bool isLoopHeader;
    size_t loopDepth;
};

// RISC-Vコード生成器クラス
class RISCVCodeGenerator {
public:
    explicit RISCVCodeGenerator(Context* context);
    ~RISCVCodeGenerator();
    
    // コード生成メイン
    std::vector<uint32_t> GenerateCode(const IRFunction& function);
    
    // 命令生成
    void EmitInstruction(const IRInstruction& instr);
    void EmitArithmetic(IROpcode opcode, int rd, int rs1, int rs2);
    void EmitImmediate(IROpcode opcode, int rd, int rs1, int16_t imm);
    void EmitLoad(IRType type, int rd, int rs1, int16_t offset);
    void EmitStore(IRType type, int rs1, int rs2, int16_t offset);
    void EmitBranch(IRBranchType type, int rs1, int rs2, int32_t offset);
    void EmitJump(int rd, int32_t offset);
    void EmitCall(const std::string& functionName);
    void EmitReturn();
    
    // ベクトル命令生成
    void EmitVectorLoad(int vd, int rs1, int16_t offset);
    void EmitVectorStore(int vs3, int rs1, int16_t offset);
    void EmitVectorArithmetic(VectorOpcode opcode, int vd, int vs1, int vs2);
    void EmitVectorConfig(size_t vl, size_t vsew);
    
    // 基本ブロック管理
    void StartBasicBlock();
    void EndBasicBlock();
    size_t GetCurrentBlockIndex() const { return currentBlock_; }
    
    // ラベル管理
    void DefineLabel(const std::string& label);
    void EmitLabelReference(const std::string& label);
    size_t GetLabelAddress(const std::string& label);
    
    // リロケーション
    void AddRelocation(size_t instructionIndex, const std::string& symbol, int32_t addend = 0);
    void ResolveRelocations();
    
    // 最適化
    void OptimizeCode();
    void PerformPeepholeOptimization();
    void RemoveRedundantInstructions();
    void OptimizeBranches();
    
    // レジスタ情報
    void SetRegisterMapping(const std::unordered_map<IRValue, int>& mapping);
    int GetPhysicalRegister(const IRValue& value);
    
    // 統計情報
    size_t GetInstructionCount() const { return instructions_.size(); }
    size_t GetCodeSize() const { return instructions_.size() * sizeof(uint32_t); }
    
private:
    // 内部ヘルパー
    uint32_t EncodeRType(uint8_t opcode, uint8_t rd, uint8_t funct3, 
                        uint8_t rs1, uint8_t rs2, uint8_t funct7);
    uint32_t EncodeIType(uint8_t opcode, uint8_t rd, uint8_t funct3, 
                        uint8_t rs1, int16_t imm);
    uint32_t EncodeSType(uint8_t opcode, uint8_t funct3, uint8_t rs1, 
                        uint8_t rs2, int16_t imm);
    uint32_t EncodeBType(uint8_t opcode, uint8_t funct3, uint8_t rs1, 
                        uint8_t rs2, int16_t imm);
    uint32_t EncodeUType(uint8_t opcode, uint8_t rd, uint32_t imm);
    uint32_t EncodeJType(uint8_t opcode, uint8_t rd, int32_t imm);
    uint32_t EncodeVType(uint8_t opcode, uint8_t vd, uint8_t funct3,
                        uint8_t vs1, uint8_t vs2, uint8_t vm, uint8_t funct6);
    
    // 最適化ヘルパー
    bool IsRedundantInstruction(size_t index);
    bool CanMergeInstructions(size_t index1, size_t index2);
    void ReplaceInstruction(size_t index, uint32_t newInstruction);
    void RemoveInstruction(size_t index);
    
    // 分岐最適化
    void OptimizeConditionalBranches();
    void OptimizeUnconditionalBranches();
    bool CanInvertBranch(uint32_t instruction);
    uint32_t InvertBranchCondition(uint32_t instruction);
    
    // ループ最適化
    void DetectLoops();
    void OptimizeLoops();
    bool IsLoopInvariant(size_t instructionIndex, size_t loopStart, size_t loopEnd);
    
private:
    Context* context_;
    std::vector<InstructionEncoding> instructions_;
    std::vector<BasicBlock> basicBlocks_;
    size_t currentBlock_;
    
    // ラベルとシンボル
    std::unordered_map<std::string, size_t> labels_;
    std::unordered_map<size_t, std::string> pendingLabels_;
    
    // リロケーション情報
    struct RelocationEntry {
        size_t instructionIndex;
        std::string symbol;
        int32_t addend;
        RelocationType type;
    };
    std::vector<RelocationEntry> relocations_;
    
    // レジスタマッピング
    std::unordered_map<IRValue, int> registerMapping_;
    
    // コード生成状態
    bool inFunction_;
    size_t currentInstructionIndex_;
    std::vector<size_t> loopHeaders_;
    std::vector<size_t> loopEnds_;
};

// エンコーディング定数
namespace Opcodes {
    constexpr uint8_t LOAD        = 0x03;
    constexpr uint8_t LOAD_FP     = 0x07;
    constexpr uint8_t MISC_MEM    = 0x0F;
    constexpr uint8_t OP_IMM      = 0x13;
    constexpr uint8_t AUIPC       = 0x17;
    constexpr uint8_t OP_IMM_32   = 0x1B;
    constexpr uint8_t STORE       = 0x23;
    constexpr uint8_t STORE_FP    = 0x27;
    constexpr uint8_t AMO         = 0x2F;
    constexpr uint8_t OP          = 0x33;
    constexpr uint8_t LUI         = 0x37;
    constexpr uint8_t OP_32       = 0x3B;
    constexpr uint8_t MADD        = 0x43;
    constexpr uint8_t MSUB        = 0x47;
    constexpr uint8_t NMSUB       = 0x4B;
    constexpr uint8_t NMADD       = 0x4F;
    constexpr uint8_t OP_FP       = 0x53;
    constexpr uint8_t OP_V        = 0x57;
    constexpr uint8_t BRANCH      = 0x63;
    constexpr uint8_t JALR        = 0x67;
    constexpr uint8_t JAL         = 0x6F;
    constexpr uint8_t SYSTEM      = 0x73;
}

namespace Funct3 {
    // LOAD/STORE
    constexpr uint8_t LB = 0x0, LH = 0x1, LW = 0x2, LD = 0x3;
    constexpr uint8_t LBU = 0x4, LHU = 0x5, LWU = 0x6;
    constexpr uint8_t SB = 0x0, SH = 0x1, SW = 0x2, SD = 0x3;
    
    // OP_IMM
    constexpr uint8_t ADDI = 0x0, SLTI = 0x2, SLTIU = 0x3;
    constexpr uint8_t XORI = 0x4, ORI = 0x6, ANDI = 0x7;
    constexpr uint8_t SLLI = 0x1, SRLI = 0x5, SRAI = 0x5;
    
    // OP
    constexpr uint8_t ADD = 0x0, SLL = 0x1, SLT = 0x2, SLTU = 0x3;
    constexpr uint8_t XOR = 0x4, SRL = 0x5, OR = 0x6, AND = 0x7;
    
    // BRANCH
    constexpr uint8_t BEQ = 0x0, BNE = 0x1, BLT = 0x4, BGE = 0x5;
    constexpr uint8_t BLTU = 0x6, BGEU = 0x7;
    
    // SYSTEM
    constexpr uint8_t ECALL = 0x0, EBREAK = 0x0;
}

namespace Funct7 {
    constexpr uint8_t NORMAL = 0x00;
    constexpr uint8_t ALT    = 0x20;  // SUB, SRA等
    constexpr uint8_t MULDIV = 0x01;  // MUL, DIV等
}

} // namespace riscv
} // namespace core
} // namespace aerojs

#endif // AEROJS_RISCV_CODE_GENERATOR_H