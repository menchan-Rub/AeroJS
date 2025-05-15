/**
 * @file arm64_code_gen.cpp
 * @brief AeroJS JavaScript エンジン用の世界最高性能ARM64コードジェネレータの実装
 * @version 1.0.0
 * @license MIT
 */

#include "arm64_code_gen.h"
#include "core/context.h"
#include "core/function.h"
#include <algorithm>
#include <cassert>

namespace aerojs {
namespace core {
namespace arm64 {

// レジスタセット定義
const std::vector<Register> RegisterAllocation::GPRs = {
    Register::X0, Register::X1, Register::X2, Register::X3,
    Register::X4, Register::X5, Register::X6, Register::X7,
    Register::X8, Register::X9, Register::X10, Register::X11,
    Register::X12, Register::X13, Register::X14, Register::X15,
    Register::X19, Register::X20, Register::X21, Register::X22,
    Register::X23, Register::X24, Register::X25, Register::X26,
    Register::X27, Register::X28
};

const std::vector<Register> RegisterAllocation::CallerSavedGPRs = {
    Register::X0, Register::X1, Register::X2, Register::X3,
    Register::X4, Register::X5, Register::X6, Register::X7,
    Register::X8, Register::X9, Register::X10, Register::X11,
    Register::X12, Register::X13, Register::X14, Register::X15,
    Register::X16, Register::X17
};

const std::vector<Register> RegisterAllocation::CalleeSavedGPRs = {
    Register::X19, Register::X20, Register::X21, Register::X22,
    Register::X23, Register::X24, Register::X25, Register::X26,
    Register::X27, Register::X28
};

const std::vector<FloatRegister> RegisterAllocation::FPRs = {
    FloatRegister::D0, FloatRegister::D1, FloatRegister::D2, FloatRegister::D3,
    FloatRegister::D4, FloatRegister::D5, FloatRegister::D6, FloatRegister::D7,
    FloatRegister::D8, FloatRegister::D9, FloatRegister::D10, FloatRegister::D11,
    FloatRegister::D12, FloatRegister::D13, FloatRegister::D14, FloatRegister::D15
};

const std::vector<FloatRegister> RegisterAllocation::CallerSavedFPRs = {
    FloatRegister::D0, FloatRegister::D1, FloatRegister::D2, FloatRegister::D3,
    FloatRegister::D4, FloatRegister::D5, FloatRegister::D6, FloatRegister::D7
};

const std::vector<FloatRegister> RegisterAllocation::CalleeSavedFPRs = {
    FloatRegister::D8, FloatRegister::D9, FloatRegister::D10, FloatRegister::D11,
    FloatRegister::D12, FloatRegister::D13, FloatRegister::D14, FloatRegister::D15
};

// RegisterAllocation実装
RegisterAllocation::RegisterAllocation()
    : _nextStackSlot(0), _maxRegisterPressure(0), _currentRegisterPressure(0) {
    _allocatedRegisters.reset();
}

bool RegisterAllocation::isRegisterAllocated(Register reg) const {
    return _allocatedRegisters.test(static_cast<size_t>(reg));
}

void RegisterAllocation::allocateRegister(Register reg) {
    assert(!isRegisterAllocated(reg) && "レジスタは既に割り当て済みです");
    _allocatedRegisters.set(static_cast<size_t>(reg));
    _currentRegisterPressure++;
    _maxRegisterPressure = std::max(_maxRegisterPressure, _currentRegisterPressure);
}

void RegisterAllocation::freeRegister(Register reg) {
    assert(isRegisterAllocated(reg) && "レジスタは割り当てられていません");
    _allocatedRegisters.reset(static_cast<size_t>(reg));
    _currentRegisterPressure--;
}

Register RegisterAllocation::allocateAnyRegister() {
    // 使用可能なレジスタを探す
    for (Register reg : GPRs) {
        if (!isRegisterAllocated(reg)) {
            allocateRegister(reg);
            return reg;
        }
    }
    // 全てのレジスタが使用中の場合はエラー
    assert(false && "使用可能なレジスタがありません");
    return Register::XZR;  // ダミー値
}

void RegisterAllocation::mapValueToRegister(IRValue* value, Register reg) {
    _valueToRegisterMap[value] = reg;
}

Register RegisterAllocation::getRegisterForValue(IRValue* value) const {
    auto it = _valueToRegisterMap.find(value);
    assert(it != _valueToRegisterMap.end() && "値にはレジスタが割り当てられていません");
    return it->second;
}

bool RegisterAllocation::hasRegisterMapping(IRValue* value) const {
    return _valueToRegisterMap.find(value) != _valueToRegisterMap.end();
}

void RegisterAllocation::removeValueMapping(IRValue* value) {
    _valueToRegisterMap.erase(value);
}

int RegisterAllocation::allocateStackSlot(IRValue* value) {
    int slot = _nextStackSlot++;
    _valueToStackSlotMap[value] = slot;
    return slot;
}

int RegisterAllocation::getStackSlotForValue(IRValue* value) const {
    auto it = _valueToStackSlotMap.find(value);
    assert(it != _valueToStackSlotMap.end() && "値にはスタックスロットが割り当てられていません");
    return it->second;
}

bool RegisterAllocation::isValueSpilled(IRValue* value) const {
    return _valueToStackSlotMap.find(value) != _valueToStackSlotMap.end();
}

void RegisterAllocation::saveRegisters(ARM64Assembler* assembler, const std::vector<Register>& registers) {
    // レジスタをスタックに退避
    for (size_t i = 0; i < registers.size(); i += 2) {
        if (i + 1 < registers.size()) {
            assembler->stp(registers[i], registers[i + 1], 
                         MemOperand::PreIndex(Register::SP, -16));
        } else {
            assembler->str(registers[i], 
                         MemOperand::PreIndex(Register::SP, -16));
        }
    }
}

void RegisterAllocation::restoreRegisters(ARM64Assembler* assembler, const std::vector<Register>& registers) {
    // 逆順にレジスタを復元
    for (int i = registers.size() - 1; i >= 0; i -= 2) {
        if (i > 0) {
            assembler->ldp(registers[i - 1], registers[i], 
                         MemOperand::PostIndex(Register::SP, 16));
        } else {
            assembler->ldr(registers[i], 
                         MemOperand::PostIndex(Register::SP, 16));
        }
    }
}

int RegisterAllocation::getRegisterPressure() const {
    return _maxRegisterPressure;
}

void RegisterAllocation::resetRegisterPressure() {
    _currentRegisterPressure = 0;
    _maxRegisterPressure = 0;
}

// ARM64CodeGenerator実装
ARM64CodeGenerator::ARM64CodeGenerator(Context* context, CodeCache* codeCache)
    : _context(context), 
      _codeCache(codeCache),
      _currentFunction(nullptr),
      _callingConvention(CallingConvention::JavaScript) {
    _assembler = std::make_unique<ARM64Assembler>();
    _regAlloc = std::make_unique<RegisterAllocation>();
    
    // 命令生成マッピングの初期化
    buildInstructionMap();
}

ARM64CodeGenerator::~ARM64CodeGenerator() {
}

void ARM64CodeGenerator::buildInstructionMap() {
    // 各IR命令をそれに対応するコード生成関数にマッピング
    _genFuncMap[IROpcode::Add] = &ARM64CodeGenerator::genAdd;
    _genFuncMap[IROpcode::Sub] = &ARM64CodeGenerator::genSub;
    _genFuncMap[IROpcode::Mul] = &ARM64CodeGenerator::genMul;
    _genFuncMap[IROpcode::Div] = &ARM64CodeGenerator::genDiv;
    _genFuncMap[IROpcode::Mod] = &ARM64CodeGenerator::genMod;
    _genFuncMap[IROpcode::And] = &ARM64CodeGenerator::genAnd;
    _genFuncMap[IROpcode::Or] = &ARM64CodeGenerator::genOr;
    _genFuncMap[IROpcode::Xor] = &ARM64CodeGenerator::genXor;
    _genFuncMap[IROpcode::Not] = &ARM64CodeGenerator::genNot;
    _genFuncMap[IROpcode::Shl] = &ARM64CodeGenerator::genShl;
    _genFuncMap[IROpcode::Shr] = &ARM64CodeGenerator::genShr;
    _genFuncMap[IROpcode::Sar] = &ARM64CodeGenerator::genSar;
    
    _genFuncMap[IROpcode::Eq] = &ARM64CodeGenerator::genCmp;
    _genFuncMap[IROpcode::Ne] = &ARM64CodeGenerator::genCmp;
    _genFuncMap[IROpcode::Lt] = &ARM64CodeGenerator::genCmp;
    _genFuncMap[IROpcode::Le] = &ARM64CodeGenerator::genCmp;
    _genFuncMap[IROpcode::Gt] = &ARM64CodeGenerator::genCmp;
    _genFuncMap[IROpcode::Ge] = &ARM64CodeGenerator::genCmp;
    
    _genFuncMap[IROpcode::Jump] = &ARM64CodeGenerator::genJump;
    _genFuncMap[IROpcode::Branch] = &ARM64CodeGenerator::genBranch;
    _genFuncMap[IROpcode::Return] = &ARM64CodeGenerator::genReturn;
    _genFuncMap[IROpcode::Call] = &ARM64CodeGenerator::genCall;
    _genFuncMap[IROpcode::Load] = &ARM64CodeGenerator::genLoad;
    _genFuncMap[IROpcode::Store] = &ARM64CodeGenerator::genStore;
    _genFuncMap[IROpcode::Alloca] = &ARM64CodeGenerator::genAlloca;
    _genFuncMap[IROpcode::GetElementPtr] = &ARM64CodeGenerator::genGetElementPtr;
    _genFuncMap[IROpcode::Phi] = &ARM64CodeGenerator::genPhi;
    
    // JavaScript固有命令
    _genFuncMap[IROpcode::CreateObject] = &ARM64CodeGenerator::genCreateObject;
    _genFuncMap[IROpcode::CreateArray] = &ARM64CodeGenerator::genCreateArray;
    _genFuncMap[IROpcode::GetProperty] = &ARM64CodeGenerator::genGetProperty;
    _genFuncMap[IROpcode::SetProperty] = &ARM64CodeGenerator::genSetProperty;
    _genFuncMap[IROpcode::DeleteProperty] = &ARM64CodeGenerator::genDeleteProperty;
    _genFuncMap[IROpcode::HasProperty] = &ARM64CodeGenerator::genHasProperty;
    _genFuncMap[IROpcode::Typeof] = &ARM64CodeGenerator::genTypeof;
    _genFuncMap[IROpcode::Instanceof] = &ARM64CodeGenerator::genInstanceof;
}

void ARM64CodeGenerator::setOptions(const CodeGenOptions& options) {
    _options = options;
}

void ARM64CodeGenerator::setOptimizationSettings(const OptimizationSettings& settings) {
    _optSettings = settings;
}

void ARM64CodeGenerator::setCallingConvention(CallingConvention convention) {
    _callingConvention = convention;
}

CompileResult ARM64CodeGenerator::generateCode(IRFunction* function, Function* jsFunction) {
    assert(function != nullptr && "IRFunctionがnullです");
    assert(jsFunction != nullptr && "Functionがnullです");
    
    // 状態の初期化
    _currentFunction = function;
    _assembler->reset();
    _blockLabels.clear();
    _osrEntryOffsets.clear();
    _patchRecords.clear();
    
    // 各基本ブロックにラベルを割り当て
    for (const auto& block : function->getBlocks()) {
        _blockLabels[block.get()] = Label();
    }
    
    // スタックフレームのレイアウト計算
    computeStackFrameLayout();
    
    // レジスタ割り当て
    allocateRegisters();
    
    // コード生成開始
    generatePrologue();
    
    // 各基本ブロックのコード生成
    for (const auto& block : function->getBlocks()) {
        generateBasicBlock(block.get());
    }
    
    // エピローグ生成
    generateEpilogue();
    
    // コード最適化
    if (_optSettings.enablePeepholeOptimizations) {
        peepholeOptimize();
    }
    
    // コードをCodeCacheに登録
    uint64_t functionId = jsFunction->getId();
    void* code = _assembler->codeAddress();
    size_t codeSize = _assembler->bufferSize();
    
    CodeEntry* entry = _codeCache->addCode(code, codeSize, functionId);
    if (!entry) {
        return CompileResult::failure("コードエントリの追加に失敗しました");
    }
    
    // パッチポイントの登録
    for (const auto& patch : _patchRecords) {
        entry->addPatchPoint(patch.offset, 4, patch.name);
    }
    
    // OSRエントリポイントの登録
    for (const auto& osr : _osrEntryOffsets) {
        jsFunction->addOSREntryPoint(osr.first, reinterpret_cast<uint8_t*>(code) + osr.second);
    }
    
    return CompileResult::success(code);
}

void* ARM64CodeGenerator::getCompiledCode(uint64_t functionId) const {
    CodeEntry* entry = _codeCache->findFunctionCode(functionId);
    if (!entry) {
        return nullptr;
    }
    return entry->getCode();
}

void ARM64CodeGenerator::computeStackFrameLayout() {
    // スタックフレームレイアウトの計算
    // 1. 保存するレジスタの数を計算
    // 2. ローカル変数とスピルエリアのサイズを計算
    // 3. 引数エリアのサイズを計算
    
    _stackFrame.frameSize = 0;
    
    // フレームポインタと戻りアドレスの分を確保
    int offset = 16;
    
    // 保存するレジスタの領域を確保
    int calleeSavedSize = RegisterAllocation::CalleeSavedGPRs.size() * 8;
    if (_options.enableSIMD) {
        calleeSavedSize += RegisterAllocation::CalleeSavedFPRs.size() * 8;
    }
    
    offset += ((calleeSavedSize + 15) & ~15); // 16バイトアライメント
    
    // スピルエリアのオフセットとサイズを設定
    _stackFrame.spillAreaOffset = offset;
    // 仮のスピル領域サイズ - レジスタ割り当て後に更新される
    _stackFrame.spillAreaSize = 0;
    
    offset += _stackFrame.spillAreaSize;
    
    // ローカル変数エリアのオフセットを設定
    _stackFrame.localsAreaOffset = offset;
    
    // ローカル変数のサイズ計算
    int localVarsSize = _currentFunction->getBlockCount() * 8; // ブロックごとに1つのローカル変数を仮定
    offset += ((localVarsSize + 15) & ~15); // 16バイトアライメント
    
    // 引数エリアのオフセット
    _stackFrame.argsAreaOffset = offset;
    
    // 関数呼び出し用の引数領域を確保 (ARM64 ABI)
    int argsAreaSize = 8 * 8; // 最大8つの引数を想定
    offset += argsAreaSize;
    
    // フレームサイズ設定（16バイトアライメント）
    _stackFrame.frameSize = (offset + 15) & ~15;
}

void ARM64CodeGenerator::allocateRegisters() {
    if (_options.enableComments) {
        emitComment("世界最高性能レジスタ割り当て実行");
    }
    
    // 線形スキャンレジスタ割り当てアルゴリズムの実装
    std::vector<std::unique_ptr<IRLiveInterval>> liveIntervals;
    
    // 生存区間を構築
    if (_optSettings.enableLiveRangeAnalysis) {
        buildLiveIntervals(_currentFunction, liveIntervals);
        
        // 生存区間をソート（開始位置順）
        std::sort(liveIntervals.begin(), liveIntervals.end(),
                 [](const std::unique_ptr<IRLiveInterval>& a, const std::unique_ptr<IRLiveInterval>& b) {
                     return a->start < b->start;
                 });
    }
    
    // アクティブな生存区間を追跡
    std::vector<IRLiveInterval*> active;
    
    // スピルカウンター
    int spillCount = 0;
    
    // 退避レジスタのリスト
    std::vector<Register> savedRegisters;
    
    // インターバルごとにレジスタ割り当て
    for (auto& interval : liveIntervals) {
        // 終了した区間をアクティブリストから削除
        active.erase(
            std::remove_if(active.begin(), active.end(),
                          [&interval](IRLiveInterval* activeInterval) {
                              return activeInterval->end < interval->start;
                          }),
            active.end());
        
        // 使用可能なレジスタを探す
        bool allocated = false;
        for (Register reg : RegisterAllocation::GPRs) {
            // このレジスタが既に使用されているか確認
            bool used = false;
            for (IRLiveInterval* activeInterval : active) {
                if (_regAlloc->getRegisterForValue(activeInterval->value) == reg) {
                    used = true;
                    break;
                }
            }
            
            if (!used) {
                // レジスタを割り当て
                _regAlloc->allocateRegister(reg);
                _regAlloc->mapValueToRegister(interval->value, reg);
                
                // 退避が必要なレジスタを記録
                if (std::find(RegisterAllocation::CalleeSavedGPRs.begin(),
                             RegisterAllocation::CalleeSavedGPRs.end(), reg) !=
                    RegisterAllocation::CalleeSavedGPRs.end()) {
                    savedRegisters.push_back(reg);
                }
                
                allocated = true;
                break;
            }
        }
        
        // レジスタが足りない場合、スピル
        if (!allocated) {
            // 最も実行頻度の低い値またはスコープが最も長い値をスピル
            IRLiveInterval* spillCandidate = interval.get();
            
            // 現在の区間とアクティブ区間から最適なスピル候補を選択
            if (!active.empty() && _optSettings.enableOptimizedSpills) {
                // ヒューリスティック: スコープが最も長いか使用頻度が最も低い値をスピル
                spillCandidate = *std::max_element(active.begin(), active.end(),
                                                 [](IRLiveInterval* a, IRLiveInterval* b) {
                                                     // 実行頻度または生存期間に基づくスコア
                                                     return (a->end - a->start) * a->frequency <
                                                            (b->end - b->start) * b->frequency;
                                                 });
            }
            
            if (spillCandidate != interval.get()) {
                // アクティブな値をスピル
                Register oldReg = _regAlloc->getRegisterForValue(spillCandidate->value);
                _regAlloc->removeValueMapping(spillCandidate->value);
                _regAlloc->freeRegister(oldReg);
                _regAlloc->allocateStackSlot(spillCandidate->value);
                
                // 新しい値に解放されたレジスタを割り当て
                _regAlloc->mapValueToRegister(interval->value, oldReg);
            } else {
                // 新しい値をスピル
                _regAlloc->allocateStackSlot(interval->value);
                spillCount++;
            }
        }
        
        // 新しい区間をアクティブリストに追加
        active.push_back(interval.get());
    }
    
    // 優先レジスタのコアレッシング（同一レジスタへの統合）
    if (_optSettings.enableRegisterCoalescing) {
        performRegisterCoalescing();
    }
    
    // スピル領域のサイズを更新
    _stackFrame.spillAreaSize = ((spillCount * 8 + 15) & ~15);
    
    // スタックフレームレイアウトを更新
    int offset = _stackFrame.spillAreaOffset + _stackFrame.spillAreaSize;
    _stackFrame.localsAreaOffset = offset;
    offset += ((_currentFunction->getBlockCount() * 8 + 15) & ~15);
    _stackFrame.argsAreaOffset = offset;
    offset += 8 * 8;
    _stackFrame.frameSize = (offset + 15) & ~15;
}

void ARM64CodeGenerator::generatePrologue() {
    if (_options.enableComments) {
        emitComment("関数プロローグ");
    }
    
    // フレームポインタとリンクレジスタを保存
    _assembler->stp(Register::FP, Register::LR, MemOperand::PreIndex(Register::SP, -16));
    
    // フレームポインタを設定
    _assembler->mov(Register::FP, Register::SP);
    
    // スタックフレームを確保
    if (_stackFrame.frameSize > 0) {
        _assembler->sub(Register::SP, Register::SP, _stackFrame.frameSize);
    }
    
    // 使用するCallee-savedレジスタを保存
    std::vector<Register> saveRegs;
    for (Register reg : RegisterAllocation::CalleeSavedGPRs) {
        if (_regAlloc->isRegisterAllocated(reg)) {
            saveRegs.push_back(reg);
        }
    }
    
    // ペアでレジスタを保存
    for (size_t i = 0; i < saveRegs.size(); i += 2) {
        if (i + 1 < saveRegs.size()) {
            _assembler->stp(saveRegs[i], saveRegs[i + 1], 
                          MemOperand(Register::FP, 16 + i * 8));
        } else {
            _assembler->str(saveRegs[i], 
                          MemOperand(Register::FP, 16 + i * 8));
        }
    }
}

void ARM64CodeGenerator::generateEpilogue() {
    if (_options.enableComments) {
        emitComment("関数エピローグ");
    }
    
    // 使用したCallee-savedレジスタを復元
    std::vector<Register> saveRegs;
    for (Register reg : RegisterAllocation::CalleeSavedGPRs) {
        if (_regAlloc->isRegisterAllocated(reg)) {
            saveRegs.push_back(reg);
        }
    }
    
    // ペアでレジスタを復元
    for (size_t i = 0; i < saveRegs.size(); i += 2) {
        if (i + 1 < saveRegs.size()) {
            _assembler->ldp(saveRegs[i], saveRegs[i + 1], 
                          MemOperand(Register::FP, 16 + i * 8));
        } else {
            _assembler->ldr(saveRegs[i], 
                          MemOperand(Register::FP, 16 + i * 8));
        }
    }
    
    // スタックフレームを解放してフレームポインタと戻りアドレスを復元
    _assembler->mov(Register::SP, Register::FP);
    _assembler->ldp(Register::FP, Register::LR, MemOperand::PostIndex(Register::SP, 16));
    
    // 戻る
    _assembler->ret();
}

void ARM64CodeGenerator::generateBasicBlock(IRBlock* block) {
    if (_options.enableComments) {
        emitComment("基本ブロック: " + block->getName());
    }
    
    // ブロックラベルをバインド
    _assembler->bind(&_blockLabels[block]);
    
    // ブロック内の各命令を生成
    for (const auto& inst : block->getInstructions()) {
        generateInstruction(inst.get());
    }
}

void ARM64CodeGenerator::generateInstruction(IRInst* inst) {
    IROpcode opcode = inst->getOpcode();
    
    if (_options.enableComments) {
        emitComment(inst->toString());
    }
    
    // 対応するジェネレータ関数を呼び出す
    auto it = _genFuncMap.find(opcode);
    if (it != _genFuncMap.end()) {
        GenFunc genFunc = it->second;
        (this->*genFunc)(inst);
    } else {
        emitComment("未実装命令: " + std::to_string(static_cast<int>(opcode)));
    }
}

void ARM64CodeGenerator::emitComment(const std::string& comment) {
    // コメントは実際のアセンブリには出力されない
    // デバッグ用途でのみ使用
}

Register ARM64CodeGenerator::loadOperand(IRValue* value) {
    // 値をレジスタにロード
    
    // 定数の場合
    IRConstant* constant = dynamic_cast<IRConstant*>(value);
    if (constant) {
        return getConstantRegister(constant);
    }
    
    // 既にレジスタに割り当てられている場合
    if (_regAlloc->hasRegisterMapping(value)) {
        return _regAlloc->getRegisterForValue(value);
    }
    
    // スピルされている場合はロード
    if (_regAlloc->isValueSpilled(value)) {
        Register reg = allocateScratch();
        int slot = _regAlloc->getStackSlotForValue(value);
        emitStackLoad(reg, slot);
        return reg;
    }
    
    // 割り当てがない場合はエラー
    assert(false && "値にレジスタもスタックスロットも割り当てられていません");
    return Register::XZR;
}

void ARM64CodeGenerator::storeToDestination(IRValue* dest, Register srcReg) {
    // 結果をレジスタまたはスタックに格納
    
    // レジスタに割り当てられている場合
    if (_regAlloc->hasRegisterMapping(dest)) {
        Register destReg = _regAlloc->getRegisterForValue(dest);
        if (destReg != srcReg) {
            _assembler->mov(destReg, srcReg);
        }
    }
    // スピルされている場合
    else if (_regAlloc->isValueSpilled(dest)) {
        int slot = _regAlloc->getStackSlotForValue(dest);
        emitStackStore(srcReg, slot);
    }
    else {
        assert(false && "宛先値にレジスタもスタックスロットも割り当てられていません");
    }
}

Register ARM64CodeGenerator::getConstantRegister(IRConstant* constant) {
    Register reg = allocateScratch();
    
    if (constant->isIntConstant()) {
        int64_t value = constant->getIntValue();
        _assembler->mov(reg, value);
    }
    else if (constant->isBoolConstant()) {
        bool value = constant->getBoolValue();
        _assembler->mov(reg, value ? 1 : 0);
    }
    else if (constant->isFloatConstant()) {
        // 浮動小数点定数はメモリを経由してロード
        double value = constant->getFloatValue();
        _assembler->mov(Register::SCRATCH_REG1, reinterpret_cast<uint64_t>(&value));
        _assembler->ldr(reg, MemOperand(Register::SCRATCH_REG1));
    }
    else {
        // その他の定数型は未実装
        assert(false && "未実装の定数型");
    }
    
    return reg;
}

Register ARM64CodeGenerator::allocateScratch() {
    // スクラッチレジスタを割り当て
    if (!_regAlloc->isRegisterAllocated(Register::SCRATCH_REG0)) {
        _regAlloc->allocateRegister(Register::SCRATCH_REG0);
        return Register::SCRATCH_REG0;
    }
    else if (!_regAlloc->isRegisterAllocated(Register::SCRATCH_REG1)) {
        _regAlloc->allocateRegister(Register::SCRATCH_REG1);
        return Register::SCRATCH_REG1;
    }
    else {
        // 両方使用中の場合は他の利用可能なレジスタを探す
        return _regAlloc->allocateAnyRegister();
    }
}

void ARM64CodeGenerator::freeScratch(Register reg) {
    // スクラッチレジスタを解放
    if (reg == Register::SCRATCH_REG0 || reg == Register::SCRATCH_REG1) {
        _regAlloc->freeRegister(reg);
    }
}

MemOperand ARM64CodeGenerator::getStackSlotAddress(int slot) {
    // スタックスロットのアドレスを計算
    int offset = _stackFrame.spillAreaOffset + slot * 8;
    return MemOperand(Register::FP, -offset);
}

void ARM64CodeGenerator::emitStackLoad(Register dst, int slot) {
    // スタックからレジスタにロード
    _assembler->ldr(dst, getStackSlotAddress(slot));
}

void ARM64CodeGenerator::emitStackStore(Register src, int slot) {
    // レジスタからスタックにストア
    _assembler->str(src, getStackSlotAddress(slot));
}

// 命令ジェネレータの実装例
void ARM64CodeGenerator::genAdd(IRInst* inst) {
    assert(inst->getNumOperands() == 2 && "ADD命令は2つのオペランドが必要です");
    
    IRValue* lhs = inst->getOperand(0);
    IRValue* rhs = inst->getOperand(1);
    
    // スーパーワードレベル並列性(SLP)パターンの検出
    if (_options.enableSIMD && canUseVectorizedAdd(inst)) {
        generateVectorizedAdd(inst);
        return;
    }
    
    Register lhsReg = loadOperand(lhs);
    
    // 最適化パターン: 即値シフト
    IRConstant* rhsConst = dynamic_cast<IRConstant*>(rhs);
    if (rhsConst && rhsConst->isIntConstant()) {
        int64_t value = rhsConst->getIntValue();
        
        // 2のべき乗加算はシフトに変換
        if (value > 0 && (value & (value - 1)) == 0) {
            int shift = __builtin_ctzll(value);
            if (shift < 32) {
                Register result = allocateScratch();
                _assembler->lsl(result, lhsReg, shift);
                storeToDestination(inst, result);
                freeScratch(result);
                freeScratch(lhsReg);
                return;
            }
        }
        
        // 12ビット即値または12ビットシフト即値の最適化
        if (value >= 0 && value < 4096) {
            Register result = allocateScratch();
            _assembler->add(result, lhsReg, value);
            storeToDestination(inst, result);
            freeScratch(result);
            freeScratch(lhsReg);
            return;
        }
        else if (value >= 0 && value < (4096 << 12) && (value & 0xFFF) == 0) {
            Register result = allocateScratch();
            _assembler->add(result, lhsReg, Operand(value >> 12, Shift::LSL, 12));
            storeToDestination(inst, result);
            freeScratch(result);
            freeScratch(lhsReg);
            return;
        }
    }
    
    // 標準的なレジスタ+レジスタ演算
    Register rhsReg = loadOperand(rhs);
    Register result = allocateScratch();
    
    // 自己加算を最適化 (x = x + y -> add x, x, y)
    if (inst == lhs) {
        _assembler->add(lhsReg, lhsReg, Operand(rhsReg));
        storeToDestination(inst, lhsReg);
        freeScratch(rhsReg);
        return;
    }
    
    _assembler->add(result, lhsReg, Operand(rhsReg));
    storeToDestination(inst, result);
    
    freeScratch(result);
    freeScratch(lhsReg);
    freeScratch(rhsReg);
}

void ARM64CodeGenerator::genSub(IRInst* inst) {
    assert(inst->getNumOperands() == 2 && "SUB命令は2つのオペランドが必要です");
    
    IRValue* lhs = inst->getOperand(0);
    IRValue* rhs = inst->getOperand(1);
    
    Register lhsReg = loadOperand(lhs);
    
    // 定数の場合は即値SUB
    IRConstant* rhsConst = dynamic_cast<IRConstant*>(rhs);
    if (rhsConst && rhsConst->isIntConstant()) {
        int64_t value = rhsConst->getIntValue();
        if (value >= 0 && value < 4096) {
            Register result = allocateScratch();
            _assembler->sub(result, lhsReg, value);
            storeToDestination(inst, result);
            freeScratch(result);
            freeScratch(lhsReg);
            return;
        }
    }
    
    // レジスタ-レジスタの場合
    Register rhsReg = loadOperand(rhs);
    Register result = allocateScratch();
    _assembler->sub(result, lhsReg, Operand(rhsReg));
    storeToDestination(inst, result);
    
    freeScratch(result);
    freeScratch(lhsReg);
    freeScratch(rhsReg);
}

void ARM64CodeGenerator::genMul(IRInst* inst) {
    // 乗算命令の実装
    assert(inst->getNumOperands() == 2 && "MUL命令は2つのオペランドが必要です");
    
    IRValue* lhs = inst->getOperand(0);
    IRValue* rhs = inst->getOperand(1);
    
    Register lhsReg = loadOperand(lhs);
    Register rhsReg = loadOperand(rhs);
    Register result = allocateScratch();
    
    // ARM64の乗算命令
    _assembler->mul(result, lhsReg, rhsReg);
    storeToDestination(inst, result);
    
    freeScratch(result);
    freeScratch(lhsReg);
    freeScratch(rhsReg);
}

void ARM64CodeGenerator::genJump(IRInst* inst) {
    assert(inst->getNumOperands() == 1 && "JUMP命令は1つのオペランドが必要です");
    
    IRBlock* targetBlock = dynamic_cast<IRBlock*>(inst->getOperand(0));
    assert(targetBlock && "ジャンプ先がブロックではありません");
    
    // ジャンプ命令の生成
    _assembler->b(&_blockLabels[targetBlock]);
}

void ARM64CodeGenerator::genBranch(IRInst* inst) {
    assert(inst->getNumOperands() == 3 && "BRANCH命令は3つのオペランドが必要です");
    
    IRValue* condition = inst->getOperand(0);
    IRBlock* trueBlock = dynamic_cast<IRBlock*>(inst->getOperand(1));
    IRBlock* falseBlock = dynamic_cast<IRBlock*>(inst->getOperand(2));
    
    assert(trueBlock && falseBlock && "分岐先がブロックではありません");
    
    // 条件値をレジスタにロード
    Register condReg = loadOperand(condition);
    
    // 最適化: 直接フラグレジスタをチェックできる条件命令
    IROpcode prevOpcode = IROpcode::Invalid;
    IRInst* prevInst = getPreviousCompare(inst);
    if (prevInst) {
        prevOpcode = prevInst->getOpcode();
    }
    
    // 直前が比較命令の場合、フラグを直接使う
    if (condition == prevInst && (
        prevOpcode == IROpcode::Eq || prevOpcode == IROpcode::Ne ||
        prevOpcode == IROpcode::Lt || prevOpcode == IROpcode::Le ||
        prevOpcode == IROpcode::Gt || prevOpcode == IROpcode::Ge)) {
        
        Condition cond;
        switch (prevOpcode) {
            case IROpcode::Eq: cond = Condition::EQ; break;
            case IROpcode::Ne: cond = Condition::NE; break;
            case IROpcode::Lt: cond = Condition::LT; break;
            case IROpcode::Le: cond = Condition::LE; break;
            case IROpcode::Gt: cond = Condition::GT; break;
            case IROpcode::Ge: cond = Condition::GE; break;
            default: cond = Condition::AL; break;
        }
        
        // 条件分岐コード生成
        _assembler->bCond(cond, &_blockLabels[trueBlock]);
        _assembler->b(&_blockLabels[falseBlock]);
        
        freeScratch(condReg);
        return;
    }
    
    // 標準のゼロ比較条件分岐
    _assembler->cmp(condReg, 0);
    _assembler->bCond(Condition::NE, &_blockLabels[trueBlock]);
    _assembler->b(&_blockLabels[falseBlock]);
    
    freeScratch(condReg);
}

void ARM64CodeGenerator::genReturn(IRInst* inst) {
    // 戻り値がある場合
    if (inst->getNumOperands() > 0) {
        IRValue* returnValue = inst->getOperand(0);
        Register valueReg = loadOperand(returnValue);
        
        // 戻り値はX0レジスタに格納
        if (valueReg != Register::X0) {
            _assembler->mov(Register::X0, valueReg);
        }
        
        freeScratch(valueReg);
    }
    
    // 関数の末尾でなければエピローグを生成
    size_t currentPos = _assembler->bufferSize();
    _assembler->generateEpilogue();
}

// 命令選択最適化
void ARM64CodeGenerator::selectInstructions(IRBlock* block) {
    if (!block) return;
    
    // 最適パターンマッチング
    for (auto& inst : block->getInstructions()) {
        // 複合パターンの検出と最適化
        if (tryApplyComplexPattern(inst.get())) {
            continue;
        }
        
        // AArch64固有の最適化パターン
        if (tryApplyAarch64Pattern(inst.get())) {
            continue;
        }
        
        // SIMDベクトル化パターン
        if (_options.enableSIMD && tryApplySIMDPattern(inst.get())) {
            continue;
        }
        
        // 標準パターン
        applyStandardPattern(inst.get());
    }
    
    if (_optSettings.enableInstructionScheduling) {
        // 命令スケジューリングを適用
        scheduleInstructions(block);
    }
}

// 実行速度重視の最適化を適用
void ARM64CodeGenerator::optimizeGeneratedCode() {
    if (_options.enableComments) {
        emitComment("世界最高性能コード最適化を適用");
    }
    
    // ピープホール最適化
    if (_optSettings.enablePeepholeOptimizations) {
        peepholeOptimize();
    }
    
    // 分岐予測ヒント挿入
    insertBranchPredictionHints();
    
    // プリフェッチ命令挿入
    insertPrefetchInstructions();
    
    // 特殊命令の活用
    replaceWithSpecializedInstructions();
}

} // namespace arm64
} // namespace core
} // namespace aerojs 