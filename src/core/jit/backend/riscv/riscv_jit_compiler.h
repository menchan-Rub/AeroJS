/**
 * @file riscv_jit_compiler.h
 * @brief RISC-V向けJITコンパイラ実装
 * 
 * このファイルは、RISC-Vアーキテクチャ向けのJITコンパイラクラスを定義します。
 * RV64GCV拡張セットに対応し、ベクトル処理やJavaScript特有の最適化を実装します。
 * 
 * @author AeroJS Team
 * @version 1.0.0
 * @copyright MIT License
 */

#ifndef AEROJS_RISCV_JIT_COMPILER_H
#define AEROJS_RISCV_JIT_COMPILER_H

#include "../../../jit_compiler.h"
#include "../../ir/ir_function.h"
#include "../../bytecode/bytecode.h"
#include "riscv_code_generator.h"
#include "riscv_vector.h"
#include "../../../runtime/values/value.h"
#include <vector>
#include <unordered_map>
#include <memory>

namespace aerojs {
namespace core {
namespace riscv {

// RISC-V拡張命令サポート
struct RISCVExtensions {
    bool hasRV64I = true;          // 基本整数命令
    bool hasRV64M = true;          // 乗除算拡張
    bool hasRV64A = true;          // アトミック拡張
    bool hasRV64F = true;          // 単精度浮動小数点
    bool hasRV64D = true;          // 倍精度浮動小数点
    bool hasRV64C = true;          // 圧縮命令
    bool hasRV64V = false;         // ベクトル拡張（検出必要）
    bool hasZba = false;           // アドレス生成拡張
    bool hasZbb = false;           // 基本ビット操作
    bool hasZbc = false;           // キャリーレス乗算
    bool hasZbs = false;           // 単一ビット操作
    bool hasZfh = false;           // 半精度浮動小数点
    bool hasZvfh = false;          // ベクトル半精度浮動小数点
};

// コンパイル結果
struct RISCVCompilationResult {
    std::vector<uint32_t> instructions;
    size_t codeSize = 0;
    void* executableCode = nullptr;
    std::vector<RelocationEntry> relocations;
    std::unordered_map<std::string, size_t> symbolTable;
    uint32_t entryPoint = 0;
    bool isOptimized = false;
    size_t registerPressure = 0;
    size_t vectorInstructions = 0;
};

// RISC-V JITコンパイラクラス
class RISCVJITCompiler : public JITCompiler {
public:
    explicit RISCVJITCompiler(Context* context);
    ~RISCVJITCompiler() override;

    // JITCompilerインターフェースの実装
    CompileResult Compile(const IRFunction& function) override;
    bool Execute(const CompileResult& result, Value* args, size_t argCount, Value& returnValue) override;
    void InvalidateCode(const CompileResult& result) override;
    size_t GetCodeSize(const CompileResult& result) const override;
    
    // RISC-V固有メソッド
    void SetOptimizationLevel(OptimizationLevel level);
    void EnableVectorization(bool enable) { vectorizationEnabled_ = enable; }
    void SetVectorLength(size_t vlen) { vectorLength_ = vlen; }
    bool SupportsExtension(const std::string& extension) const;
    
    // プロファイルデータに基づく最適化
    void SetProfileData(const ProfileData& profile) { profileData_ = profile; }
    void EnableProfilingOptimization(bool enable) { profilingOptimization_ = enable; }
    
    // デバッグサポート
    void EnableDebugInfo(bool enable) { debugInfoEnabled_ = enable; }
    void SetBreakpoint(size_t address) { breakpoints_.insert(address); }
    void RemoveBreakpoint(size_t address) { breakpoints_.erase(address); }

private:
    // コンパイルフェーズ
    RISCVCompilationResult CompileFunction(const IRFunction& function);
    void OptimizeIR(IRFunction& function);
    void AllocateRegisters(const IRFunction& function);
    void GenerateCode(const IRFunction& function, RISCVCompilationResult& result);
    void ApplyRelocations(RISCVCompilationResult& result);
    void* AllocateExecutableMemory(size_t size);
    void MakeMemoryExecutable(void* memory, size_t size);
    
    // 最適化パス
    void PerformConstantFolding(IRFunction& function);
    void PerformDeadCodeElimination(IRFunction& function);
    void PerformInstructionScheduling(IRFunction& function);
    void PerformVectorization(IRFunction& function);
    void PerformLoopOptimization(IRFunction& function);
    void PerformPeepholeOptimization(std::vector<uint32_t>& instructions);
    
    // レジスタ割り当て
    void PerformLinearScanRegisterAllocation(const IRFunction& function);
    void PerformGraphColoringRegisterAllocation(const IRFunction& function);
    int AllocateIntegerRegister(const IRValue& value);
    int AllocateFloatRegister(const IRValue& value);
    int AllocateVectorRegister(const IRValue& value);
    void SpillRegister(int reg, const IRValue& value);
    void RestoreRegister(int reg, const IRValue& value);
    
    // コード生成ヘルパー
    void EmitPrologue(RISCVCompilationResult& result);
    void EmitEpilogue(RISCVCompilationResult& result);
    void EmitInstruction(IROpcode opcode, const std::vector<IROperand>& operands, 
                        RISCVCompilationResult& result);
    void EmitBranch(IRBranchType type, int reg1, int reg2, int32_t offset,
                   RISCVCompilationResult& result);
    void EmitLoad(IRType type, int destReg, int baseReg, int32_t offset,
                 RISCVCompilationResult& result);
    void EmitStore(IRType type, int srcReg, int baseReg, int32_t offset,
                  RISCVCompilationResult& result);
    void EmitVectorOperation(VectorOpcode opcode, const std::vector<int>& operands,
                           RISCVCompilationResult& result);
    
    // 命令エンコーディング
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
    
    // JavaScript固有の最適化
    void OptimizePropertyAccess(IRFunction& function);
    void OptimizeArrayAccess(IRFunction& function);
    void OptimizeFunctionCalls(IRFunction& function);
    void OptimizeTypeChecks(IRFunction& function);
    void OptimizeGarbageCollection(IRFunction& function);
    
    // 最適化ヘルパーメソッド
    bool isArithmeticOperation(IROpcode opcode);
    bool isComparisonOperation(IROpcode opcode);
    bool isLogicalOperation(IROpcode opcode);
    bool isTypeConversion(IROpcode opcode);
    bool isBitwiseOperation(IROpcode opcode);
    bool isArrayAccess(IROpcode opcode);
    bool isStringOperation(IROpcode opcode);
    bool isConditionalBranch(IROpcode opcode);
    bool isTypeCheckInstruction(const IRInstruction& instr);
    
    // 定数畳み込みヘルパー
    bool foldArithmeticConstants(IRFunction& function, size_t index);
    bool foldComparisonConstants(IRFunction& function, size_t index);
    bool foldLogicalConstants(IRFunction& function, size_t index);
    bool foldTypeConversionConstants(IRFunction& function, size_t index);
    bool foldBitwiseConstants(IRFunction& function, size_t index);
    bool foldArrayAccessConstants(IRFunction& function, size_t index);
    bool foldStringConstants(IRFunction& function, size_t index);
    bool foldConditionalBranch(IRFunction& function, size_t index);
    bool performConstantPropagation(IRFunction& function);
    
    // デッドコード除去ヘルパー
    bool hasSideEffects(const IRInstruction& instr);
    bool isResultUsed(size_t index, const LivenessInfo& liveVariables);
    bool isUnreachableCode(size_t index, const IRFunction& function, 
                          const ReachabilityInfo& reachableBlocks);
    bool isRedundantAssignment(size_t index, const IRFunction& function,
                              const LivenessInfo& liveVariables);
    bool isUnusedPureCall(size_t index, const IRFunction& function,
                         const LivenessInfo& liveVariables);
    bool isDeadStore(size_t index, const IRFunction& function,
                    const LivenessInfo& liveVariables);
    bool removeEmptyBasicBlocks(IRFunction& function);
    bool removeRedundantBranches(IRFunction& function);
    bool isPureBuiltinFunction(const IRInstruction& instr);
    bool isNextInstructionTarget(const IRInstruction& instr, size_t index,
                                const IRFunction& function);
    
    // ベクトル化ヘルパー
    bool CanVectorizeLoop(const IRLoop& loop);
    void VectorizeLoop(IRLoop& loop);
    size_t GetOptimalVectorLength(ValueType type);
    void EmitVectorLoop(const IRLoop& loop, RISCVCompilationResult& result);
    
    // ランタイムサポート
    void EmitRuntimeCall(RuntimeFunction func, const std::vector<IROperand>& args,
                        RISCVCompilationResult& result);
    void EmitGarbageCollectionSafepoint(RISCVCompilationResult& result);
    void EmitProfilerHook(const IRInstruction& instr, RISCVCompilationResult& result);
    void EmitDebugInfo(const IRInstruction& instr, RISCVCompilationResult& result);
    
    // エラー処理
    void EmitExceptionHandler(ExceptionType type, RISCVCompilationResult& result);
    void EmitStackOverflowCheck(RISCVCompilationResult& result);
    void EmitNullPointerCheck(int reg, RISCVCompilationResult& result);
    void EmitBoundsCheck(int arrayReg, int indexReg, RISCVCompilationResult& result);
    
    // プロファイリング
    void CollectProfileData(const IRFunction& function);
    void UpdateHotspots(const CompileResult& result);
    bool IsHotFunction(const IRFunction& function) const;
    void ApplyProfileGuidedOptimizations(IRFunction& function);

private:
    Context* context_;
    RISCVExtensions extensions_;
    RISCVCodeGenerator codeGenerator_;
    RISCVVectorUnit vectorUnit_;
    
    // コンパイル設定
    OptimizationLevel optimizationLevel_ = OptimizationLevel::BALANCED;
    bool vectorizationEnabled_ = true;
    size_t vectorLength_ = 128;  // デフォルト128ビット
    bool debugInfoEnabled_ = false;
    bool profilingOptimization_ = false;
    
    // レジスタ情報
    std::vector<bool> integerRegistersUsed_;
    std::vector<bool> floatRegistersUsed_;
    std::vector<bool> vectorRegistersUsed_;
    std::unordered_map<IRValue, int> registerAssignments_;
    std::vector<IRValue> spilledValues_;
    
    // コンパイル状態
    size_t currentStackOffset_ = 0;
    std::vector<size_t> functionCalls_;
    std::unordered_map<std::string, size_t> labels_;
    std::vector<RelocationEntry> pendingRelocations_;
    
    // プロファイルデータ
    ProfileData profileData_;
    std::unordered_set<const IRFunction*> hotFunctions_;
    std::unordered_map<size_t, uint64_t> executionCounts_;
    
    // デバッグ情報
    std::unordered_set<size_t> breakpoints_;
    std::vector<DebugLineInfo> debugLines_;
    std::unordered_map<size_t, SourceLocation> sourceMap_;
    
    // メモリ管理
    std::vector<void*> allocatedMemory_;
    size_t totalAllocatedMemory_ = 0;
    static constexpr size_t MAX_CODE_SIZE = 64 * 1024 * 1024;  // 64MB
    
    // 統計情報
    struct CompilationStats {
        size_t functionsCompiled = 0;
        size_t instructionsGenerated = 0;
        size_t vectorInstructionsGenerated = 0;
        size_t registersSpilled = 0;
        double averageCompilationTime = 0.0;
        size_t codeSize = 0;
        
        // 最適化統計
        size_t constantFoldingCount = 0;
        size_t deadCodeEliminationCount = 0;
        size_t instructionSchedulingCount = 0;
        size_t vectorizedLoops = 0;
        size_t vectorizedStatements = 0;
        size_t slpVectorizedStatements = 0;
        size_t loopOptimizationCount = 0;
        size_t licmMovedInstructions = 0;
        size_t unrolledLoops = 0;
        size_t fusedLoops = 0;
        size_t peepholeOptimizationCount = 0;
        size_t scheduledBasicBlocks = 0;
        
        // JavaScript固有の最適化統計
        size_t optimizedPropertyAccesses = 0;
        size_t optimizedArrayAccesses = 0;
        size_t optimizedFunctionCalls = 0;
        size_t optimizedTypeChecks = 0;
        size_t optimizedGCPoints = 0;
    } stats_;
    
    // RISC-V固有の定数
    static constexpr int NUM_INTEGER_REGISTERS = 32;
    static constexpr int NUM_FLOAT_REGISTERS = 32;
    static constexpr int NUM_VECTOR_REGISTERS = 32;
    static constexpr int STACK_ALIGNMENT = 16;
    static constexpr size_t MAX_IMMEDIATE_VALUE = 2047;  // 12ビット符号付き
    static constexpr size_t PAGE_SIZE = 4096;
};

// RISC-V命令エンコーディング用の定数
namespace RISCVOpcodes {
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

// funct3フィールドの定数
namespace RISCVFunct3 {
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
}

// レジスタ番号の定数
namespace RISCVRegisters {
    // 整数レジスタ
    constexpr int ZERO = 0;   // ゼロレジスタ
    constexpr int RA = 1;     // リターンアドレス
    constexpr int SP = 2;     // スタックポインタ
    constexpr int GP = 3;     // グローバルポインタ
    constexpr int TP = 4;     // スレッドポインタ
    constexpr int T0 = 5;     // 一時レジスタ
    constexpr int T1 = 6;
    constexpr int T2 = 7;
    constexpr int S0 = 8;     // 保存レジスタ（フレームポインタ）
    constexpr int S1 = 9;
    constexpr int A0 = 10;    // 引数/戻り値レジスタ
    constexpr int A1 = 11;
    constexpr int A2 = 12;    // 引数レジスタ
    constexpr int A3 = 13;
    constexpr int A4 = 14;
    constexpr int A5 = 15;
    constexpr int A6 = 16;
    constexpr int A7 = 17;
    constexpr int S2 = 18;    // 保存レジスタ
    constexpr int S3 = 19;
    constexpr int S4 = 20;
    constexpr int S5 = 21;
    constexpr int S6 = 22;
    constexpr int S7 = 23;
    constexpr int S8 = 24;
    constexpr int S9 = 25;
    constexpr int S10 = 26;
    constexpr int S11 = 27;
    constexpr int T3 = 28;    // 一時レジスタ
    constexpr int T4 = 29;
    constexpr int T5 = 30;
    constexpr int T6 = 31;
    
    // 浮動小数点レジスタ
    constexpr int FT0 = 0;    // 一時レジスタ
    constexpr int FT1 = 1;
    constexpr int FT2 = 2;
    constexpr int FT3 = 3;
    constexpr int FT4 = 4;
    constexpr int FT5 = 5;
    constexpr int FT6 = 6;
    constexpr int FT7 = 7;
    constexpr int FS0 = 8;    // 保存レジスタ
    constexpr int FS1 = 9;
    constexpr int FA0 = 10;   // 引数/戻り値レジスタ
    constexpr int FA1 = 11;
    constexpr int FA2 = 12;   // 引数レジスタ
    constexpr int FA3 = 13;
    constexpr int FA4 = 14;
    constexpr int FA5 = 15;
    constexpr int FA6 = 16;
    constexpr int FA7 = 17;
    constexpr int FS2 = 18;   // 保存レジスタ
    constexpr int FS3 = 19;
    constexpr int FS4 = 20;
    constexpr int FS5 = 21;
    constexpr int FS6 = 22;
    constexpr int FS7 = 23;
    constexpr int FS8 = 24;
    constexpr int FS9 = 25;
    constexpr int FS10 = 26;
    constexpr int FS11 = 27;
    constexpr int FT8 = 28;   // 一時レジスタ
    constexpr int FT9 = 29;
    constexpr int FT10 = 30;
    constexpr int FT11 = 31;
}

} // namespace riscv
} // namespace core
} // namespace aerojs

#endif // AEROJS_RISCV_JIT_COMPILER_H 