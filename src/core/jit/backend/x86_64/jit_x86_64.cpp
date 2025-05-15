#include "jit_x86_64.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <system_error>

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
    // （ここでは簡略化のため1:1マッピングを想定）
    const auto& instructions = function.GetInstructions();
    for (size_t i = 0; i < instructions.size(); ++i) {
      // 仮の対応付け（実際には命令長や最適化によって異なる）
      size_t nativeOffset = i * 4; // 仮に4バイト/命令と想定
      if (nativeOffset < nativeCode.size()) {
        debugInfo.irToNativeMap[i] = nativeOffset;
        debugInfo.nativeToIrMap[nativeOffset] = i;
      }
    }
    
    m_debugInfoMap[executableCode] = std::move(debugInfo);
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
  // 実際のレジスタアロケーションアルゴリズムを実装する
  // ここでは、線形スキャンアロケーション方式の簡略版を実装
  
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
    
    // 利用可能な物理レジスタがなければスピル（簡略化のため実装省略）
    if (availableRegs.empty()) {
      // エラー処理または一時変数へのスピル実装
      continue;
    }
    
    // 物理レジスタを割り当て
    X86_64Register physReg = availableRegs.back();
    availableRegs.pop_back();
    
    // マッピングを記録
    registerMapping[range.regId] = physReg;
    activeRegisters[physReg] = range.end;
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
  // 実際の実装では、libcapstoneなどの逆アセンブラを使用するとよい
  // ここでは簡易的なバイトダンプを返す
  std::vector<std::string> result;
  
  for (size_t i = 0; i < code.size(); i += 4) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    
    for (size_t j = 0; j < 4 && i + j < code.size(); ++j) {
      oss << std::setw(2) << static_cast<int>(code[i + j]) << " ";
    }
    
    result.push_back(oss.str());
  }
  
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

} // namespace core
} // namespace aerojs 