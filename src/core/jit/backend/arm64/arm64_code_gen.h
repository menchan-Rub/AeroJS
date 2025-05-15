/**
 * @file arm64_code_gen.h
 * @brief AeroJS JavaScript エンジン用の世界最高性能ARM64コードジェネレータ
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include "arm64_assembler.h"
#include "core/jit/ir/ir_graph.h"
#include "core/jit/code_cache.h"
#include "core/jit/jit_compiler.h"
#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>
#include <bitset>
#include <stack>

namespace aerojs {
namespace core {
namespace arm64 {

/**
 * @brief レジスタ割り当て情報
 */
class RegisterAllocation {
public:
    RegisterAllocation();
    
    // レジスタ割り当て状態
    bool isRegisterAllocated(Register reg) const;
    void allocateRegister(Register reg);
    void freeRegister(Register reg);
    Register allocateAnyRegister();
    
    // 汎用レジスタセット
    static const std::vector<Register> GPRs;
    static const std::vector<Register> CallerSavedGPRs;
    static const std::vector<Register> CalleeSavedGPRs;
    
    // 浮動小数点レジスタセット
    static const std::vector<FloatRegister> FPRs;
    static const std::vector<FloatRegister> CallerSavedFPRs;
    static const std::vector<FloatRegister> CalleeSavedFPRs;
    
    // 値とレジスタの対応付け
    void mapValueToRegister(IRValue* value, Register reg);
    Register getRegisterForValue(IRValue* value) const;
    bool hasRegisterMapping(IRValue* value) const;
    void removeValueMapping(IRValue* value);
    
    // スピル管理
    int allocateStackSlot(IRValue* value);
    int getStackSlotForValue(IRValue* value) const;
    bool isValueSpilled(IRValue* value) const;
    
    // レジスタ退避・復元
    void saveRegisters(ARM64Assembler* assembler, const std::vector<Register>& registers);
    void restoreRegisters(ARM64Assembler* assembler, const std::vector<Register>& registers);
    
    // レジスタ使用統計
    int getRegisterPressure() const;
    void resetRegisterPressure();
    
private:
    std::bitset<static_cast<size_t>(Register::REGISTER_COUNT)> _allocatedRegisters;
    std::unordered_map<IRValue*, Register> _valueToRegisterMap;
    std::unordered_map<IRValue*, int> _valueToStackSlotMap;
    int _nextStackSlot;
    int _maxRegisterPressure;
    int _currentRegisterPressure;
};

/**
 * @brief ARM64アーキテクチャ向けコードジェネレータ
 * 
 * JIT IRからARM64ネイティブコードを生成し、最適化を適用します。
 */
class ARM64CodeGenerator {
public:
    ARM64CodeGenerator(Context* context, CodeCache* codeCache);
    ~ARM64CodeGenerator();
    
    // コード生成
    CompileResult generateCode(IRFunction* function, Function* jsFunction);
    void* getCompiledCode(uint64_t functionId) const;
    
    // 生成オプション
    struct CodeGenOptions {
        bool enableFastCalls;           // 高速呼び出し
        bool enableInlineCache;         // インラインキャッシュ
        bool enableExceptionHandling;   // 例外ハンドリング
        bool enableSIMD;                // SIMD命令
        bool enableOptimizedSpills;     // 最適化されたスピル
        bool enableComments;            // コメント出力（デバッグ用）
        bool enableFramePointer;        // フレームポインタ使用
        bool emitBoundsChecks;          // 境界チェック生成
        bool emitStackChecks;           // スタックチェック生成
        bool emitTypeChecks;            // 型チェック生成
        
        CodeGenOptions()
            : enableFastCalls(true),
              enableInlineCache(true),
              enableExceptionHandling(true),
              enableSIMD(true),
              enableOptimizedSpills(true),
              enableComments(false),
              enableFramePointer(true),
              emitBoundsChecks(true),
              emitStackChecks(true),
              emitTypeChecks(true) {}
    };
    
    // オプション設定
    void setOptions(const CodeGenOptions& options);
    const CodeGenOptions& getOptions() const { return _options; }
    
    // 最適化設定
    struct OptimizationSettings {
        bool enablePeepholeOptimizations;   // ピープホール最適化
        bool enableLiveRangeAnalysis;       // 生存区間解析
        bool enableRegisterCoalescing;      // レジスタ合体
        bool enableInstructionScheduling;   // 命令スケジューリング
        bool enableStackSlotCoalescing;     // スタックスロット合体
        bool enableConstantPropagation;     // 定数伝播
        bool enableDeadCodeElimination;     // デッドコード除去
        bool enableSoftwareUnrolling;       // ソフトウェアループ展開
        
        OptimizationSettings()
            : enablePeepholeOptimizations(true),
              enableLiveRangeAnalysis(true),
              enableRegisterCoalescing(true),
              enableInstructionScheduling(true),
              enableStackSlotCoalescing(true),
              enableConstantPropagation(true),
              enableDeadCodeElimination(true),
              enableSoftwareUnrolling(true) {}
    };
    
    // 最適化設定
    void setOptimizationSettings(const OptimizationSettings& settings);
    const OptimizationSettings& getOptimizationSettings() const { return _optSettings; }
    
    // ABI設定
    enum class CallingConvention {
        Standard,   // 標準呼び出し規約
        FastCall,   // 高速呼び出し規約
        JavaScript  // JavaScript特化規約
    };
    
    void setCallingConvention(CallingConvention convention);
    CallingConvention getCallingConvention() const { return _callingConvention; }
    
    // デバッグ情報
    std::string disassembleCode(void* code, size_t size) const;
    
private:
    Context* _context;
    CodeCache* _codeCache;
    CodeGenOptions _options;
    OptimizationSettings _optSettings;
    CallingConvention _callingConvention;
    
    // 現在処理中のIR関数とアセンブラ
    IRFunction* _currentFunction;
    std::unique_ptr<ARM64Assembler> _assembler;
    
    // レジスタ割り当て
    std::unique_ptr<RegisterAllocation> _regAlloc;
    
    // ラベル管理
    std::unordered_map<IRBlock*, Label> _blockLabels;
    
    // OSRサポート
    std::unordered_map<uint32_t, uint32_t> _osrEntryOffsets;
    
    // スタックフレーム情報
    struct StackFrame {
        int32_t frameSize;          // フレームサイズ（バイト）
        int32_t spillAreaOffset;    // スピルエリアオフセット
        int32_t spillAreaSize;      // スピルエリアサイズ
        int32_t localsAreaOffset;   // ローカル変数エリアオフセット
        int32_t argsAreaOffset;     // 引数エリアオフセット
        
        StackFrame() : frameSize(0), spillAreaOffset(0), 
                      spillAreaSize(0), localsAreaOffset(0),
                      argsAreaOffset(0) {}
    };
    
    StackFrame _stackFrame;
    
    // コールバック辞書（オペコード→実装メソッド）
    using GenFunc = void (ARM64CodeGenerator::*)(IRInst*);
    std::unordered_map<IROpcode, GenFunc> _genFuncMap;
    
    // コードジェネレーションメイン処理
    void generatePrologue();
    void generateEpilogue();
    void generateStackFrame();
    void generateBasicBlock(IRBlock* block);
    void generateInstruction(IRInst* inst);
    void generateOSREntry(IRInst* inst);
    
    // レジスタ割り当て
    void computeStackFrameLayout();
    void allocateRegisters();
    
    // 命令選択およびマッピング
    void buildInstructionMap();
    
    // 命令ジェネレータ - パターンから実装を呼び出す
    void selectInstructions(IRBlock* block);
    
    // 固有命令ジェネレータ
    void genMov(IRInst* inst);
    void genAdd(IRInst* inst);
    void genSub(IRInst* inst);
    void genMul(IRInst* inst);
    void genDiv(IRInst* inst);
    void genMod(IRInst* inst);
    void genAnd(IRInst* inst);
    void genOr(IRInst* inst);
    void genXor(IRInst* inst);
    void genNot(IRInst* inst);
    void genShl(IRInst* inst);
    void genShr(IRInst* inst);
    void genSar(IRInst* inst);
    void genCmp(IRInst* inst);
    void genJump(IRInst* inst);
    void genBranch(IRInst* inst);
    void genReturn(IRInst* inst);
    void genCall(IRInst* inst);
    void genLoad(IRInst* inst);
    void genStore(IRInst* inst);
    void genAlloca(IRInst* inst);
    void genGetElementPtr(IRInst* inst);
    void genPhi(IRInst* inst);
    
    // JavaScript固有操作のジェネレータ
    void genCreateObject(IRInst* inst);
    void genCreateArray(IRInst* inst);
    void genGetProperty(IRInst* inst);
    void genSetProperty(IRInst* inst);
    void genDeleteProperty(IRInst* inst);
    void genHasProperty(IRInst* inst);
    void genTypeof(IRInst* inst);
    void genInstanceof(IRInst* inst);
    
    // 浮動小数点/SIMD命令
    void genFAdd(IRInst* inst);
    void genFSub(IRInst* inst);
    void genFMul(IRInst* inst);
    void genFDiv(IRInst* inst);
    void genFCmp(IRInst* inst);
    void genVectorOp(IRInst* inst);
    
    // ユーティリティメソッド
    void emitComment(const std::string& comment);
    Register loadOperand(IRValue* value);
    void storeToDestination(IRValue* dest, Register srcReg);
    Register getConstantRegister(IRConstant* constant);
    Register allocateScratch();
    void freeScratch(Register reg);
    
    // スタックアクセス
    MemOperand getStackSlotAddress(int slot);
    void emitStackLoad(Register dst, int slot);
    void emitStackStore(Register src, int slot);
    
    // 最適化パス
    void optimizeGeneratedCode();
    void peepholeOptimize();
    void scheduleInstructions();
    
    // パッチポイント管理
    struct PatchRecord {
        std::string name;
        uint32_t offset;
        IRInst* instruction;
    };
    
    std::vector<PatchRecord> _patchRecords;
    void registerPatchPoint(const std::string& name, IRInst* instruction);
    void applyPatches();
};

} // namespace arm64
} // namespace core
} // namespace aerojs 