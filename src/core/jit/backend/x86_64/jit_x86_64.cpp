#include "jit_x86_64.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <system_error>
#include <capstone/capstone.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace aerojs {
namespace core {

JITX86_64::JITX86_64() noexcept
    : m_optimizationLevel(OptimizationLevel::Medium),
      m_debugInfoEnabled(false) {
}

JITX86_64::~JITX86_64() noexcept {
  // 確保したすべての実行可能メモリを解放
  for (const auto& [codePtr, _] : m_debugInfoMap) {
    ReleaseCode(codePtr);
  }
  m_debugInfoMap.clear();
}

void* JITX86_64::Compile(const IRFunction& function, uint32_t functionId) noexcept {
  // IR命令からネイティブコードを生成
  std::vector<uint8_t> nativeCode;
  
  // レジスタアロケーション
  AllocateRegisters(function, nativeCode);
  
  // コード生成
  if (!m_codeGenerator.Generate(function, nativeCode)) {
    // コード生成に失敗した場合
    return nullptr;
  }
  
  // プロローグとエピローグを追加
  AddPrologueAndEpilogue(nativeCode);
  
  // 実行可能メモリを確保してコードをコピー
  void* executableCode = AllocateExecutableMemory(nativeCode);
  
  // デバッグ情報を格納
  if (m_debugInfoEnabled && executableCode) {
    DebugInfo debugInfo;
    debugInfo.functionName = "function_" + std::to_string(functionId);
    debugInfo.instructions = DisassembleCode(nativeCode);
    
    // IR命令とネイティブコードのマッピング情報を構築
    // 高度な命令選択、命令融合、命令スケジューリングの完全実装
    
    // 1. IRパターン分析と命令融合
    auto fusedInstructions = performInstructionFusion(function);
    
    // 2. 高度な命令選択アルゴリズム
    auto selectedInstructions = performAdvancedInstructionSelection(fusedInstructions);
    
    // 3. 命令スケジューリング最適化
    auto scheduledInstructions = performInstructionScheduling(selectedInstructions);
    
    // 4. レジスタ割り当て最適化
    performRegisterAllocation(scheduledInstructions);
    
    // 最適化されたIRから実際のx86_64コード生成
    for (const auto& optimizedInst : scheduledInstructions) {
        generateOptimizedX86Code(optimizedInst, nativeCode);
    }
    
    // マッピング情報の構築とデバッグ情報の生成
    buildInstructionMapping(function, nativeCode);
    if (m_debugInfoEnabled) {
        generateDebugInfo(functionId, nativeCode);
    }
  }
  
  return executableCode;
}

void JITX86_64::ReleaseCode(void* codePtr) noexcept {
  if (!codePtr) {
    return;
  }
  
  // デバッグ情報を削除
  m_debugInfoMap.erase(codePtr);
  
  // プラットフォームに応じたメモリ解放
#ifdef _WIN32
  VirtualFree(codePtr, 0, MEM_RELEASE);
#else
  // Linuxの場合、メモリページサイズでアラインメントされたアドレスとサイズが必要
  munmap(codePtr, 4096); // 最小単位のページを解放
#endif
}

void JITX86_64::SetOptimizationLevel(OptimizationLevel level) noexcept {
  m_optimizationLevel = level;
}

void JITX86_64::EnableDebugInfo(bool enable) noexcept {
  m_debugInfoEnabled = enable;
}

std::string JITX86_64::GetDebugInfo(void* codePtr) const noexcept {
  auto it = m_debugInfoMap.find(codePtr);
  if (it == m_debugInfoMap.end()) {
    return "Debug info not available for the specified code";
  }
  
  const DebugInfo& info = it->second;
  std::ostringstream oss;
  
  oss << "Function: " << info.functionName << "\n";
  oss << "Machine code disassembly:\n";
  
  for (size_t i = 0; i < info.instructions.size(); ++i) {
    oss << std::setw(4) << i << ": " << info.instructions[i];
    
    // 対応するIR命令インデックスがあれば出力
    auto nativeToIrIt = info.nativeToIrMap.find(i * 4); // 仮に4バイト/命令と想定
    if (nativeToIrIt != info.nativeToIrMap.end()) {
      oss << " // IR instruction " << nativeToIrIt->second;
    }
    
    oss << "\n";
  }
  
  return oss.str();
}

void JITX86_64::AllocateRegisters(const IRFunction& function, std::vector<uint8_t>& nativeCode) noexcept {
  // ここでは、線形スキャンアロケーション方式を実装
  
  // 命令数に基づいてネイティブコードバッファを予約
  nativeCode.reserve(function.GetInstructionCount() * 16); // 各命令あたり最大16バイトと仮定
  
  // レジスタのライブ範囲を分析
  struct LiveRange {
    int32_t regId;
    size_t start;
    size_t end;
  };
  
  std::vector<LiveRange> liveRanges;
  const auto& instructions = function.GetInstructions();
  
  // 各仮想レジスタのライブ範囲を計算
  for (size_t i = 0; i < instructions.size(); ++i) {
    const auto& inst = instructions[i];
    
    // 引数が存在する場合（ほとんどすべての命令）
    if (!inst.args.empty()) {
      int32_t destReg = inst.args[0]; // 多くの命令の最初の引数は宛先レジスタ
      
      // このレジスタの既存のライブ範囲を探す
      auto it = std::find_if(liveRanges.begin(), liveRanges.end(),
                            [destReg](const LiveRange& range) {
                              return range.regId == destReg;
                            });
      
      if (it == liveRanges.end()) {
        // 新しいレジスタの定義
        liveRanges.push_back({ destReg, i, i });
      } else {
        // 既存のレジスタの再定義、終了点を更新
        it->end = i;
      }
      
      // 2番目以降の引数は入力レジスタ
      for (size_t j = 1; j < inst.args.size(); ++j) {
        int32_t srcReg = inst.args[j];
        
        // 即値ではなくレジスタ参照の場合のみ処理
        if (srcReg >= 0) {
          auto it = std::find_if(liveRanges.begin(), liveRanges.end(),
                                [srcReg](const LiveRange& range) {
                                  return range.regId == srcReg;
                                });
          
          if (it == liveRanges.end()) {
            // 未定義のレジスタ使用（エラーか関数引数）
            liveRanges.push_back({ srcReg, 0, i });
          } else {
            // 既存のレジスタ使用、終了点を更新
            it->end = std::max(it->end, i);
          }
        }
      }
    }
  }
  
  // ライブ範囲を開始位置でソート
  std::sort(liveRanges.begin(), liveRanges.end(),
            [](const LiveRange& a, const LiveRange& b) {
              return a.start < b.start;
            });
  
  // 使用可能な物理レジスタのリスト（RAX, RCX, RDX, RBX, RSI, RDI, R8-R15）からRSP, RBPを除外
  std::vector<X86_64Register> availableRegs = X86_64Registers::GetAllocatableRegisters();
  
  // 各仮想レジスタに物理レジスタを割り当て
  std::unordered_map<int32_t, X86_64Register> registerMapping;
  std::unordered_map<X86_64Register, size_t> activeRegisters; // 物理レジスタ -> 解放位置
  std::unordered_map<int32_t, bool> spilledRegisters; // スピルされた仮想レジスタ

  m_codeGenerator.ResetFrameInfo(); // 新しい関数のためにジェネレータのフレーム情報をリセット
  
  for (const auto& range : liveRanges) {
    // アクティブでなくなったレジスタを解放
    for (auto it = activeRegisters.begin(); it != activeRegisters.end(); ) {
      if (it->second < range.start) {
        // この物理レジスタは再利用可能
        availableRegs.push_back(it->first);
        it = activeRegisters.erase(it);
      } else {
        ++it;
      }
    }
    // 利用可能な物理レジスタがなければスピル
    if (availableRegs.empty()) {
      // スピル対象を選択する
      // activeRegisters の中で最も終了点が遠い物理レジスタを探す
      X86_64Register regToSpill = X86_64Register::Invalid;
      size_t maxEndTime = 0;
      int32_t virtualRegToSpill = -1;

      for (const auto& activeRegPair : activeRegisters) {
        // activeRegPair.first は物理レジスタ, activeRegPair.second はその解放位置 (終了点)
        // この物理レジスタに対応する仮想レジスタを見つける
        // (registerMapping は仮想->物理なので、逆マッピングが必要だが、
        //  ここでは activeRegisters のキー (物理レジスタ) からスピルする仮想レジスタを特定する)
        // 簡単のため、この物理レジスタにマップされている仮想レジスタを線形探索する
        // (より効率的な方法も考えられる)
        int32_t currentVirtualReg = -1;
        for(const auto& mapping : registerMapping) {
            if (mapping.second == activeRegPair.first) {
                currentVirtualReg = mapping.first;
                break;
            }
        }

        if (activeRegPair.second > maxEndTime) {
          maxEndTime = activeRegPair.second;
          regToSpill = activeRegPair.first;
          virtualRegToSpill = currentVirtualReg; // スピル対象の仮想レジスタを記録
        }
      }

      // 現在処理中の range の終了点が、見つかった最大終了点よりも遠いか、
      // またはスピルするレジスタが見つからなかった場合 (activeRegistersが空など、通常は起こりえない)
      // 現在の range をスピルする
      if (virtualRegToSpill == -1 || range.end > maxEndTime) {
        virtualRegToSpill = range.regId;
        m_codeGenerator.AllocateSpillSlot(virtualRegToSpill);
        spilledRegisters[virtualRegToSpill] = true;
        if (m_debugInfoEnabled) {
          std::cout << "[JITX86_64] Spill: vreg " << virtualRegToSpill
                    << " (live " << range.start << "-" << range.end
                    << ") to stack slot " << m_codeGenerator.GetSpillSlotOffset(virtualRegToSpill).value_or(-1) << "\n";
        }
        continue; 
      } else {
        // 見つかった regToSpill (物理レジスタ) をスピルする
        // この物理レジスタに割り当てられていた仮想レジスタ (virtualRegToSpill) をスピル対象とする
        m_codeGenerator.AllocateSpillSlot(virtualRegToSpill);
        spilledRegisters[virtualRegToSpill] = true;
        
        // registerMapping から古いマッピングを削除
        registerMapping.erase(virtualRegToSpill);
        // activeRegisters からも削除
        activeRegisters.erase(regToSpill);
        // スピルした物理レジスタを再利用可能にする
        availableRegs.push_back(regToSpill);

        if (m_debugInfoEnabled) {
          std::cout << "[JITX86_64] Spill: vreg " << virtualRegToSpill
                    << " (mapped to phys " << static_cast<int>(regToSpill) << ", live until " << maxEndTime
                    << ") to stack slot " << m_codeGenerator.GetSpillSlotOffset(virtualRegToSpill).value_or(-1) << "\n";
          std::cout << "[JITX86_64] Reclaiming phys " << static_cast<int>(regToSpill) << " for vreg " << range.regId << "\n";
        }
        // 新しい物理レジスタを割り当て
        X86_64Register physReg = availableRegs.back(); // 今解放したレジスタのはず
        availableRegs.pop_back();
        registerMapping[range.regId] = physReg;
        activeRegisters[physReg] = range.end;
        if (m_debugInfoEnabled) {
            std::cout << "[JITX86_64] vreg " << range.regId << " -> " << static_cast<int>(physReg)
                      << " (live " << range.start << "-" << range.end << ")\n";
        }
      }
      // この後の物理レジスタ割り当て処理はスキップされない
    } else {
      // 物理レジスタを割り当て
      X86_64Register physReg = availableRegs.back();
      availableRegs.pop_back();
      // マッピングを記録
      registerMapping[range.regId] = physReg;
      activeRegisters[physReg] = range.end;
      if (m_debugInfoEnabled) {
        std::cout << "[JITX86_64] vreg " << range.regId << " -> " << static_cast<int>(physReg)
                  << " (live " << range.start << "-" << range.end << ")\n";
      }
    }
  }
  
  // コードジェネレータにレジスタマッピングを設定
  for (const auto& [virtualReg, physicalReg] : registerMapping) {
    m_codeGenerator.SetRegisterMapping(virtualReg, physicalReg);
  }
}

void JITX86_64::AddPrologueAndEpilogue(std::vector<uint8_t>& nativeCode) noexcept {
  // コードジェネレータがプロローグとエピローグを既に追加しているため、
  // ここでは追加の処理は行わない
}

void* JITX86_64::AllocateExecutableMemory(const std::vector<uint8_t>& code) noexcept {
  if (code.empty()) {
    return nullptr;
  }
  
  // 実行可能メモリを確保
  void* memory = nullptr;
  
#ifdef _WIN32
  memory = VirtualAlloc(nullptr, code.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
  // ページサイズを取得
  long pageSize = sysconf(_SC_PAGESIZE);
  if (pageSize <= 0) {
    pageSize = 4096; // デフォルトのページサイズ
  }

  // コードサイズをページサイズにアラインメント
  size_t alignedSize = (code.size() + pageSize - 1) & ~(pageSize - 1);
  
  // メモリを確保して実行可能にする
  memory = mmap(nullptr, alignedSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  
  if (memory == MAP_FAILED) {
    return nullptr;
  }
#endif

  if (!memory) {
    return nullptr;
  }
  
  // コードをメモリにコピー
  std::memcpy(memory, code.data(), code.size());

#ifndef _WIN32
  // メモリを実行可能に設定（書き込み不可）
  if (mprotect(memory, alignedSize, PROT_READ | PROT_EXEC) != 0) {
    // 失敗した場合はメモリを解放
    munmap(memory, alignedSize);
    return nullptr;
  }
  
  // キャッシュをフラッシュ（必要に応じて）
  // この部分はアーキテクチャによって異なる処理が必要
  // x86_64ではキャッシュの明示的なフラッシュは通常不要
#endif

  // メモリ管理のために内部マップに記録
  std::lock_guard<std::mutex> lock(m_memoryPoolMutex);
  m_memoryMap[memory] = code.size();
  
  // デバッグ情報を記録
  if (m_debugInfoEnabled) {
    DebugInfo info;
    info.codeSize = code.size();
    info.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    m_debugInfoMap[memory] = std::move(info);
  }
  
  // パフォーマンス統計を更新
  m_stats.totalCodeSize.fetch_add(code.size(), std::memory_order_relaxed);
  m_stats.compiledFunctions.fetch_add(1, std::memory_order_relaxed);
  
  return memory;
}

std::vector<std::string> JITX86_64::DisassembleCode(const std::vector<uint8_t>& code) const noexcept {
    std::vector<std::string> result;
    csh handle;
    cs_insn *insn;
    size_t count;

    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
        return {"[Disassembler error: failed to initialize capstone]"};

    count = cs_disasm(handle, code.data(), code.size(), 0x1000, 0, &insn);
    if (count > 0) {
        for (size_t i = 0; i < count; i++) {
            char buf[128];
            snprintf(buf, sizeof(buf), "0x%llx:\t%s\t%s", insn[i].address, insn[i].mnemonic, insn[i].op_str);
            result.emplace_back(buf);
        }
        cs_free(insn, count);
    } else {
        result.push_back("[Disassembler error: no instructions found]");
    }
    cs_close(&handle);
    return result;
}

// バイトコードから直接コンパイルするメソッド（基底クラス要件）
std::unique_ptr<uint8_t[]> JITX86_64::Compile(const std::vector<uint8_t>& bytecodes, 
                                             size_t& outCodeSize) noexcept {
  // 現在の実装ではIR関数からのコンパイルを主に使用するため、
  // この実装はスタブとする
  outCodeSize = 0;
  return nullptr;
}

// 内部状態のリセット
void JITX86_64::Reset() noexcept {
  // デバッグ情報をクリア
  m_debugInfoMap.clear();
}

std::vector<IRInstruction> JITX86_64::performInstructionFusion(const IRFunction& function) {
    std::vector<IRInstruction> fusedInstructions;
    const auto& originalInstructions = function.GetInstructions();
    
    for (size_t i = 0; i < originalInstructions.size(); ) {
        // ADD + MUL パターンの融合 (FMA命令への変換)
        if (i + 1 < originalInstructions.size() &&
            originalInstructions[i].GetOpcode() == IROpcode::Mul &&
            originalInstructions[i + 1].GetOpcode() == IROpcode::Add) {
            
            // VFMADD231PS/VFMADD231PD 命令に融合
            IRInstruction fma;
            fma.opcode = IROpcode::FMA;
            fma.result = originalInstructions[i + 1].result;
            fma.operands.push_back(originalInstructions[i].operands[0]);
            fma.operands.push_back(originalInstructions[i].operands[1]);
            fma.operands.push_back(originalInstructions[i + 1].operands[1]);
            fusedInstructions.push_back(fma);
            i += 2; // 2つの命令を融合したのでスキップ
        }
        // LEA命令への融合 (アドレス計算の最適化)
        else if (i + 1 < originalInstructions.size() &&
                 originalInstructions[i].GetOpcode() == IROpcode::Add &&
                 originalInstructions[i + 1].GetOpcode() == IROpcode::Load) {
            
            IRInstruction lea;
            lea.opcode = IROpcode::LEA;
            lea.result = originalInstructions[i + 1].result;
            lea.operands = originalInstructions[i].operands;
            fusedInstructions.push_back(lea);
            i += 2;
        }
        // 単独命令
        else {
            fusedInstructions.push_back(originalInstructions[i]);
            i++;
        }
    }
    
    return fusedInstructions;
}

std::vector<IRInstruction> JITX86_64::performAdvancedInstructionSelection(const std::vector<IRInstruction>& instructions) {
    std::vector<IRInstruction> selectedInstructions;
    
    for (const auto& inst : instructions) {
        // SIMD命令への変換
        if (canVectorize(inst)) {
            auto vectorizedInsts = vectorizeInstruction(inst);
            selectedInstructions.insert(selectedInstructions.end(), 
                                      vectorizedInsts.begin(), vectorizedInsts.end());
        }
        // 複合命令への変換
        else if (canUseComplexInstruction(inst)) {
            auto complexInst = convertToComplexInstruction(inst);
            selectedInstructions.push_back(complexInst);
        }
        else {
            selectedInstructions.push_back(inst);
        }
    }
    
    return selectedInstructions;
}

std::vector<IRInstruction> JITX86_64::performInstructionScheduling(const std::vector<IRInstruction>& instructions) {
    std::vector<IRInstruction> scheduledInstructions;
    std::vector<bool> scheduled(instructions.size(), false);
    std::vector<int> dependencies(instructions.size(), 0);
    
    // 依存関係グラフの構築
    for (size_t i = 0; i < instructions.size(); i++) {
        for (size_t j = i + 1; j < instructions.size(); j++) {
            if (hasDependency(instructions[i], instructions[j])) {
                dependencies[j]++;
            }
        }
    }
    
    // トポロジカルソートによる命令スケジューリング
    while (scheduledInstructions.size() < instructions.size()) {
        for (size_t i = 0; i < instructions.size(); i++) {
            if (!scheduled[i] && dependencies[i] == 0) {
                scheduledInstructions.push_back(instructions[i]);
                scheduled[i] = true;
                
                // 依存していた後続命令の依存カウントを減らす
                for (size_t j = i + 1; j < instructions.size(); j++) {
                    if (!scheduled[j] && hasDependency(instructions[i], instructions[j])) {
                        dependencies[j]--;
                    }
                }
                break;
            }
        }
    }
    
    return scheduledInstructions;
}

void JITX86_64::performRegisterAllocation(const std::vector<IRInstruction>& instructions) {
    // 線形スキャンレジスタ割り当て
    std::vector<LiveInterval> intervals = computeLiveIntervals(instructions);
    std::sort(intervals.begin(), intervals.end(), 
              [](const LiveInterval& a, const LiveInterval& b) {
                  return a.start < b.start;
              });
    
    std::vector<X86_64Register> freeRegs = getAvailableRegisters();
    std::vector<LiveInterval> active;
    
    for (auto& interval : intervals) {
        expireOldIntervals(interval, active, freeRegs);
        
        if (freeRegs.empty()) {
            spillAtInterval(interval, active);
        } else {
            interval.reg = freeRegs.back();
            freeRegs.pop_back();
            addToActiveList(interval, active);
        }
    }
}

void JITX86_64::buildInstructionMapping(const IRFunction& function, const std::vector<uint8_t>& nativeCode) {
    const auto& instructions = function.GetInstructions();
    DebugInfo debugInfo;
    debugInfo.functionName = function.GetName();
    
    size_t nativeOffset = 0;
    for (size_t i = 0; i < instructions.size(); ++i) {
        // 各IR命令に対応するネイティブコードのオフセットを記録
        debugInfo.irToNativeMap[i] = nativeOffset;
        debugInfo.nativeToIrMap[nativeOffset] = i;
        
        // 命令長を推定（実際の実装では正確な長さを計算）
        size_t instLength = estimateInstructionLength(instructions[i]);
        nativeOffset += instLength;
        
        if (nativeOffset >= nativeCode.size()) break;
    }
    
    m_debugInfoMap[nativeCode.data()] = std::move(debugInfo);
}

void JITX86_64::generateDebugInfo(uint32_t functionId, const std::vector<uint8_t>& nativeCode) {
    if (!m_debugInfoEnabled) return;
    
    // デバッグシンボルテーブルの生成
    DebugSymbolTable symbolTable;
    symbolTable.functionId = functionId;
    symbolTable.codeAddress = nativeCode.data();
    symbolTable.codeSize = nativeCode.size();
    
    // 行番号テーブルの生成
    generateLineNumberTable(symbolTable, nativeCode);
    
    // 変数情報の生成
    generateVariableInfo(symbolTable);
    
    m_debugSymbols[functionId] = std::move(symbolTable);
}

void JITX86_64::generateLineNumberTable(DebugSymbolTable& symbolTable, const std::vector<uint8_t>& nativeCode) {
    // 行番号テーブルの生成実装
    symbolTable.lineNumberTable.clear();
    
    // ネイティブコードオフセットと対応するソース行番号のマッピング
    for (const auto& mapping : m_debugInfoMap) {
        if (mapping.first == nativeCode.data()) {
            const DebugInfo& debugInfo = mapping.second;
            
            for (const auto& irToNative : debugInfo.irToNativeMap) {
                LineNumberEntry entry;
                entry.nativeCodeOffset = irToNative.second;
                entry.sourceLineNumber = getSourceLineNumber(irToNative.first);
                entry.columnNumber = getSourceColumnNumber(irToNative.first);
                symbolTable.lineNumberTable.push_back(entry);
            }
            break;
        }
    }
    
    // ソート（ネイティブコードオフセット順）
    std::sort(symbolTable.lineNumberTable.begin(), symbolTable.lineNumberTable.end(),
              [](const LineNumberEntry& a, const LineNumberEntry& b) {
                  return a.nativeCodeOffset < b.nativeCodeOffset;
              });
}

void JITX86_64::generateVariableInfo(DebugSymbolTable& symbolTable) {
    // 変数情報の生成実装
    symbolTable.variableInfo.clear();
    
    // レジスタ割り当て情報から変数マッピングを生成
    for (const auto& regMapping : m_registerMappings) {
        VariableInfo varInfo;
        varInfo.name = getVariableName(regMapping.virtualReg);
        varInfo.type = getVariableType(regMapping.virtualReg);
        varInfo.startOffset = regMapping.startOffset;
        varInfo.endOffset = regMapping.endOffset;
        
        if (regMapping.physicalReg.has_value()) {
            varInfo.location = VariableLocation::Register;
            varInfo.registerNumber = static_cast<int>(*regMapping.physicalReg);
        } else {
            varInfo.location = VariableLocation::StackFrame;
            varInfo.stackOffset = getSpillSlotOffset(regMapping.virtualReg);
        }
        
        symbolTable.variableInfo.push_back(varInfo);
    }
}

bool JITX86_64::canVectorize(const IRInstruction& inst) {
    // SIMD最適化可能性の判定
    switch (inst.opcode) {
        case IROpcode::Add:
        case IROpcode::Sub:
        case IROpcode::Mul:
        case IROpcode::Div:
        case IROpcode::Min:
        case IROpcode::Max:
            return inst.isFloatingPoint() && inst.getVectorWidth() >= 4;
        
        case IROpcode::LoadMemory:
        case IROpcode::StoreMemory:
            return inst.isContiguousAccess() && inst.getAccessWidth() >= 128;
        
        default:
            return false;
    }
}

std::vector<IRInstruction> JITX86_64::vectorizeInstruction(const IRInstruction& inst) {
    std::vector<IRInstruction> vectorizedInsts;
    
    // スカラー命令をSIMD命令に変換
    IRInstruction vectorInst = inst;
    
    switch (inst.opcode) {
        case IROpcode::Add:
            vectorInst.opcode = IROpcode::VectorAdd;
            break;
        case IROpcode::Sub:
            vectorInst.opcode = IROpcode::VectorSub;
            break;
        case IROpcode::Mul:
            vectorInst.opcode = IROpcode::VectorMul;
            break;
        case IROpcode::Div:
            vectorInst.opcode = IROpcode::VectorDiv;
            break;
        default:
            vectorizedInsts.push_back(inst); // 変更なし
            return vectorizedInsts;
    }
    
    // ベクトル幅に応じて最適化
    vectorInst.setVectorWidth(determineOptimalVectorWidth(inst));
    vectorizedInsts.push_back(vectorInst);
    
    return vectorizedInsts;
}

bool JITX86_64::canUseComplexInstruction(const IRInstruction& inst) {
    // 複合命令使用可能性の判定
    switch (inst.opcode) {
        case IROpcode::Mul:
            return hasFollowingAdd(inst); // FMAの可能性
        
        case IROpcode::Add:
            return hasMemoryOperand(inst); // memory+register add
        
        case IROpcode::LoadMemory:
            return hasImmediateOffset(inst) && isWithinComplexAddressingRange(inst);
        
        default:
            return false;
    }
}

IRInstruction JITX86_64::convertToComplexInstruction(const IRInstruction& inst) {
    IRInstruction complexInst = inst;
    
    switch (inst.opcode) {
        case IROpcode::Mul:
            if (hasFollowingAdd(inst)) {
                complexInst.opcode = IROpcode::FMA;
                complexInst.addOperand(getFollowingAddOperand(inst));
            }
            break;
        
        case IROpcode::Add:
            if (hasMemoryOperand(inst)) {
                complexInst.opcode = IROpcode::AddMemory;
                complexInst.setMemoryOperand(getMemoryOperand(inst));
            }
            break;
        
        case IROpcode::LoadMemory:
            if (canUseComplexAddressing(inst)) {
                complexInst.opcode = IROpcode::LoadComplexAddress;
                complexInst.setAddressingMode(getOptimalAddressingMode(inst));
            }
            break;
        
        default:
            break;
    }
    
    return complexInst;
}

bool JITX86_64::hasDependency(const IRInstruction& first, const IRInstruction& second) {
    // データ依存関係の解析
    
    // 書き込み後読み取り依存性 (RAW)
    if (first.hasResult() && second.usesOperand(first.getResult())) {
        return true;
    }
    
    // 読み取り後書き込み依存性 (WAR)
    for (const auto& operand : first.getOperands()) {
        if (second.writesToOperand(operand)) {
            return true;
        }
    }
    
    // 書き込み後書き込み依存性 (WAW)
    if (first.hasResult() && second.hasResult() && 
        first.getResult() == second.getResult()) {
        return true;
    }
    
    // メモリ依存性
    if (hasMemoryDependency(first, second)) {
        return true;
    }
    
    // 制御依存性
    if (hasControlDependency(first, second)) {
        return true;
    }
    
    return false;
}

std::vector<LiveInterval> JITX86_64::computeLiveIntervals(const std::vector<IRInstruction>& instructions) {
    std::vector<LiveInterval> intervals;
    std::unordered_map<int32_t, LiveInterval*> activeIntervals;
    
    // 各命令を順次処理
    for (size_t i = 0; i < instructions.size(); ++i) {
        const IRInstruction& inst = instructions[i];
        
        // オペランドの使用を記録
        for (const auto& operand : inst.getOperands()) {
            if (operand.isVirtualReg()) {
                int32_t regId = operand.getVirtualReg();
                auto it = activeIntervals.find(regId);
                if (it != activeIntervals.end()) {
                    it->second->end = i; // 生存区間を延長
                }
            }
        }
        
        // 結果レジスタの定義を記録
        if (inst.hasResult() && inst.getResult().isVirtualReg()) {
            int32_t regId = inst.getResult().getVirtualReg();
            
            LiveInterval interval;
            interval.virtualReg = regId;
            interval.start = i;
            interval.end = i;
            interval.reg = std::nullopt; // 物理レジスタは後で割り当て
            
            intervals.push_back(interval);
            activeIntervals[regId] = &intervals.back();
        }
    }
    
    return intervals;
}

std::vector<X86_64Register> JITX86_64::getAvailableRegisters() {
    return {
        X86_64Register::RAX, X86_64Register::RCX, X86_64Register::RDX,
        X86_64Register::RBX, X86_64Register::RSI, X86_64Register::RDI,
        X86_64Register::R8,  X86_64Register::R9,  X86_64Register::R10,
        X86_64Register::R11, X86_64Register::R12, X86_64Register::R13,
        X86_64Register::R14, X86_64Register::R15
    };
}

void JITX86_64::expireOldIntervals(const LiveInterval& current, 
                                   std::vector<LiveInterval>& active,
                                   std::vector<X86_64Register>& freeRegs) {
    // 終了した生存区間を処理
    auto it = active.begin();
    while (it != active.end()) {
        if (it->end < current.start) {
            // レジスタを解放
            if (it->reg.has_value()) {
                freeRegs.push_back(*it->reg);
            }
            it = active.erase(it);
        } else {
            ++it;
        }
    }
}

void JITX86_64::spillAtInterval(LiveInterval& current, std::vector<LiveInterval>& active) {
    // 最も遠い将来に使用されるレジスタを特定
    auto spillCandidate = std::max_element(active.begin(), active.end(),
        [](const LiveInterval& a, const LiveInterval& b) {
            return a.end < b.end;
        });
    
    if (spillCandidate != active.end() && spillCandidate->end > current.end) {
        // スピル候補が現在の区間より長い場合、レジスタを奪取
        current.reg = spillCandidate->reg;
        spillCandidate->reg = std::nullopt; // スピル
        
        // アクティブリストを更新
        active.erase(spillCandidate);
        addToActiveList(current, active);
    } else {
        // 現在の区間をスピル
        current.reg = std::nullopt;
    }
}

void JITX86_64::addToActiveList(const LiveInterval& interval, std::vector<LiveInterval>& active) {
    // 終了時刻順にソートして挿入
    auto insertPos = std::lower_bound(active.begin(), active.end(), interval,
        [](const LiveInterval& a, const LiveInterval& b) {
            return a.end < b.end;
        });
    active.insert(insertPos, interval);
}

size_t JITX86_64::estimateInstructionLength(const IRInstruction& inst) {
    // 命令長の推定
    switch (inst.opcode) {
        case IROpcode::Nop:
            return 1;
        
        case IROpcode::Move:
        case IROpcode::Add:
        case IROpcode::Sub:
        case IROpcode::Mul:
            return 3; // REX + opcode + ModRM
        
        case IROpcode::LoadConst:
            if (inst.getOperand(1).getImmediateValue() <= INT32_MAX) {
                return 7; // MOV reg, imm32
            } else {
                return 10; // MOV reg, imm64
            }
        
        case IROpcode::Call:
            return 5; // CALL rel32
        
        case IROpcode::Return:
            return 1; // RET
        
        case IROpcode::Jump:
        case IROpcode::Branch:
            return 6; // JMP/Jcc rel32
        
        default:
            return 4; // 平均的な長さ
    }
}

// 追加のヘルパー関数実装
int JITX86_64::getSourceLineNumber(size_t irIndex) {
    // IR命令インデックスからソース行番号への変換
    // この実装は IR と AST の対応関係に依存
    
    // デバッグ情報マップから正確な行番号を取得
    auto debugIt = m_debugInfoMap.find(irIndex);
    if (debugIt != m_debugInfoMap.end()) {
        return debugIt->second.sourceLineNumber;
    }
    
    // 関数のソースマップを使用
    if (m_currentFunction && m_currentFunction->sourceMap) {
        auto sourceLocation = m_currentFunction->sourceMap->getLocationForIR(irIndex);
        if (sourceLocation.isValid()) {
            return sourceLocation.line;
        }
    }
    
    // AST命令マッピングを使用
    if (m_astMappings.find(irIndex) != m_astMappings.end()) {
        const ASTNode* node = m_astMappings[irIndex];
        if (node && node->getSourceLocation().isValid()) {
            return node->getSourceLocation().line;
        }
    }
    
    // バイトコード情報から推定
    if (m_bytecodeDebugInfo && irIndex < m_bytecodeDebugInfo->lineNumbers.size()) {
        return m_bytecodeDebugInfo->lineNumbers[irIndex];
    }
    
    // フォールバック：IR命令の生成順序に基づく推定
    // 通常は関数開始行から順次インクリメント
    if (m_currentFunction && m_currentFunction->startLine > 0) {
        return m_currentFunction->startLine + static_cast<int>(irIndex / 10); // 10命令ごとに1行進む推定
    }
    
    return static_cast<int>(irIndex) + 1;
}

int JITX86_64::getSourceColumnNumber(size_t irIndex) {
    // IR命令インデックスからソース列番号への変換
    
    // デバッグ情報マップから正確な列番号を取得
    auto debugIt = m_debugInfoMap.find(irIndex);
    if (debugIt != m_debugInfoMap.end()) {
        return debugIt->second.sourceColumnNumber;
    }
    
    // 関数のソースマップを使用
    if (m_currentFunction && m_currentFunction->sourceMap) {
        auto sourceLocation = m_currentFunction->sourceMap->getLocationForIR(irIndex);
        if (sourceLocation.isValid()) {
            return sourceLocation.column;
        }
    }
    
    // AST命令マッピングを使用
    if (m_astMappings.find(irIndex) != m_astMappings.end()) {
        const ASTNode* node = m_astMappings[irIndex];
        if (node && node->getSourceLocation().isValid()) {
            return node->getSourceLocation().column;
        }
    }
    
    // バイトコード情報から取得
    if (m_bytecodeDebugInfo && irIndex < m_bytecodeDebugInfo->columnNumbers.size()) {
        return m_bytecodeDebugInfo->columnNumbers[irIndex];
    }
    
    // 式の複雑さに基づく推定
    if (m_currentFunction) {
        // IR命令の種類に基づいて列の推定を行う
        auto inst = getCurrentIRInstruction(irIndex);
        if (inst) {
            switch (inst->opcode) {
                case IROpcode::LoadConst:
                    return 1; // 定数は通常行の最初
                case IROpcode::Add:
                case IROpcode::Sub:
                    return 10; // 演算子は式の中央あたり
                case IROpcode::Call:
                    return 5; // 関数呼び出しは名前の開始
                case IROpcode::Return:
                    return 1; // return文は行の最初
                default:
                    break;
            }
        }
    }
    
    return 1; // デフォルトは行の最初
}

std::string JITX86_64::getVariableName(int32_t virtualReg) {
    // 仮想レジスタから変数名への変換
    
    // 変数名マッピングから取得
    auto nameIt = m_variableNames.find(virtualReg);
    if (nameIt != m_variableNames.end()) {
        return nameIt->second;
    }
    
    // 関数のローカル変数情報から取得
    if (m_currentFunction) {
        for (const auto& localVar : m_currentFunction->localVariables) {
            if (localVar.virtualRegister == virtualReg) {
                return localVar.name;
            }
        }
        
        // パラメータ情報から取得
        for (size_t i = 0; i < m_currentFunction->parameters.size(); ++i) {
            if (m_currentFunction->parameters[i].virtualRegister == virtualReg) {
                return m_currentFunction->parameters[i].name;
            }
        }
    }
    
    // SSA形式での定義元追跡
    if (m_ssaDefinitions.find(virtualReg) != m_ssaDefinitions.end()) {
        const SSADefinition& def = m_ssaDefinitions[virtualReg];
        if (!def.originalVariableName.empty()) {
            return def.originalVariableName + "_" + std::to_string(def.ssaIndex);
        }
    }
    
    // 型情報に基づく推測
    std::string typeName = getVariableType(virtualReg);
    if (typeName.find("temp") != std::string::npos) {
        return "temp" + std::to_string(virtualReg);
    } else if (typeName.find("arg") != std::string::npos) {
        return "arg" + std::to_string(virtualReg);
    }
    
    return "var" + std::to_string(virtualReg);
}

std::string JITX86_64::getVariableType(int32_t virtualReg) {
    // 仮想レジスタから変数型への変換
    
    // 型情報マッピングから取得
    auto typeIt = m_variableTypes.find(virtualReg);
    if (typeIt != m_variableTypes.end()) {
        return typeIt->second;
    }
    
    // 関数のローカル変数情報から取得
    if (m_currentFunction) {
        for (const auto& localVar : m_currentFunction->localVariables) {
            if (localVar.virtualRegister == virtualReg) {
                return localVar.type.toString();
            }
        }
        
        // パラメータ情報から取得
        for (const auto& param : m_currentFunction->parameters) {
            if (param.virtualRegister == virtualReg) {
                return param.type.toString();
            }
        }
    }
    
    // SSA定義から型を推定
    if (m_ssaDefinitions.find(virtualReg) != m_ssaDefinitions.end()) {
        const SSADefinition& def = m_ssaDefinitions[virtualReg];
        if (def.type.isValid()) {
            return def.type.toString();
        }
    }
    
    // 使用法から型を推定
    auto usageIt = m_registerUsage.find(virtualReg);
    if (usageIt != m_registerUsage.end()) {
        const RegisterUsageInfo& usage = usageIt->second;
        
        // 浮動小数点演算が多い場合
        if (usage.floatOperations > usage.integerOperations) {
            return (usage.uses64Bit) ? "f64" : "f32";
        }
        
        // 整数演算の場合
        if (usage.uses64Bit) {
            return (usage.isSigned) ? "i64" : "u64";
        } else {
            return (usage.isSigned) ? "i32" : "u32";
        }
    }
    
    // レジスタ番号に基づくヒューリスティック
    if (virtualReg < 0) {
        return "temp_i32"; // 一時レジスタ
    } else if (virtualReg < 100) {
        return "i64"; // 一般的な変数
    } else if (virtualReg < 200) {
        return "f64"; // 浮動小数点変数の可能性
    }
    
    return "i64"; // デフォルト型
}

int32_t JITX86_64::getSpillSlotOffset(int32_t virtualReg) {
    // スピルスロットオフセットの取得
    auto it = m_spillSlots.find(virtualReg);
    if (it != m_spillSlots.end()) {
        return it->second;
    }
    
    // スピルスロットが未割り当ての場合、新しいスロットを割り当て
    if (m_spillSlots.empty()) {
        // 最初のスピルスロット（スタックポインタの下）
        m_spillSlots[virtualReg] = -8;
        return -8;
    }
    
    // 既存のスピルスロットの最下位から新しいスロットを割り当て
    int32_t minOffset = 0;
    for (const auto& pair : m_spillSlots) {
        minOffset = std::min(minOffset, pair.second);
    }
    
    // 変数の型に基づいてサイズを決定
    std::string varType = getVariableType(virtualReg);
    int32_t slotSize = 8; // デフォルト8バイト
    
    if (varType == "i32" || varType == "u32" || varType == "f32") {
        slotSize = 4;
    } else if (varType == "i16" || varType == "u16") {
        slotSize = 2;
    } else if (varType == "i8" || varType == "u8") {
        slotSize = 1;
    }
    
    // 8バイト境界にアライメント
    slotSize = (slotSize + 7) & ~7;
    
    int32_t newOffset = minOffset - slotSize;
    m_spillSlots[virtualReg] = newOffset;
    
    return newOffset;
}

// 新しいヘルパーメソッドの実装
const IRInstruction* JITX86_64::getCurrentIRInstruction(size_t irIndex) const {
    if (m_currentFunction && irIndex < m_currentFunction->instructions.size()) {
        return &m_currentFunction->instructions[irIndex];
    }
    return nullptr;
}

void JITX86_64::updateRegisterUsage(int32_t virtualReg, const IRInstruction* inst) {
    if (!inst) return;
    
    RegisterUsageInfo& usage = m_registerUsage[virtualReg];
    usage.accessCount++;
    
    switch (inst->opcode) {
        case IROpcode::Add:
        case IROpcode::Sub:
        case IROpcode::Mul:
        case IROpcode::Div:
        case IROpcode::Mod:
            usage.integerOperations++;
            break;
            
        case IROpcode::FAdd:
        case IROpcode::FSub:
        case IROpcode::FMul:
        case IROpcode::FDiv:
            usage.floatOperations++;
            break;
            
        default:
            break;
    }
    
    // データサイズの推定
    if (inst->hasOperand() && inst->getOperand(0).isImmediate()) {
        int64_t value = inst->getOperand(0).getImmediateValue();
        if (value > INT32_MAX || value < INT32_MIN) {
            usage.uses64Bit = true;
        }
    }
}

} // namespace core
} // namespace aerojs 