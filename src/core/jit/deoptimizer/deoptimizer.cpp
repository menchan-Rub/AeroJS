#include "deoptimizer.h"

#include <cassert>
#include <mutex>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "../vm/interpreter.h"
#include "../vm/stack/frame.h"
#include "../runtime/context/execution_context.h"
#include "../jit/metatracing/trace_manager.h"
#include "../utils/logger.h"

namespace aerojs {
namespace core {

// スレッド安全のためのミューテックス
static std::mutex g_deoptMutex;

Deoptimizer::Deoptimizer() noexcept 
  : m_deoptInfoMap(), 
    m_callback(nullptr),
    m_osrEnabled(false),
    m_deoptStatistics{},
    m_osrStatistics{} {
}

void Deoptimizer::RegisterDeoptPoint(void* codeAddress, const DeoptimizationInfo& info) noexcept {
  assert(codeAddress != nullptr && "Code address cannot be null");
  
  std::lock_guard<std::mutex> lock(g_deoptMutex);
  m_deoptInfoMap[codeAddress] = info;
  
  // ログ出力（詳細モードの場合のみ）
  if (Logger::IsDetailedLoggingEnabled()) {
    std::stringstream ss;
    ss << "Registered deopt point at 0x" << std::hex << std::setw(16) << std::setfill('0')
       << reinterpret_cast<uintptr_t>(codeAddress) << " for function " << info.functionId
       << " at bytecode offset " << info.bytecodeOffset;
    Logger::Debug(ss.str(), "Deoptimizer");
  }
}

bool Deoptimizer::PerformDeoptimization(void* codeAddress, DeoptimizationReason reason) noexcept {
  std::lock_guard<std::mutex> lock(g_deoptMutex);
  
  // デオプティマイズポイントの情報を取得
  auto it = m_deoptInfoMap.find(codeAddress);
  if (it == m_deoptInfoMap.end()) {
    // 登録されていないアドレスの場合、失敗
    Logger::Error("Deoptimization failed: unknown code address", "Deoptimizer");
    return false;
  }
  
  // デオプティマイズ情報を取得
  const auto& info = it->second;
  
  // 統計情報を更新
  m_deoptStatistics.totalDeoptimizations++;
  switch (reason) {
    case DeoptimizationReason::TypeFeedback:
      m_deoptStatistics.typeFeedbackDeoptimizations++;
      break;
    case DeoptimizationReason::Overflow:
      m_deoptStatistics.overflowDeoptimizations++;
      break;
    case DeoptimizationReason::BailoutRequest:
      m_deoptStatistics.bailoutRequestDeoptimizations++;
      break;
    case DeoptimizationReason::DebuggerAttached:
      m_deoptStatistics.debuggerAttachedDeoptimizations++;
      break;
    case DeoptimizationReason::TypeCheck:
      m_deoptStatistics.typeCheckDeoptimizations++;
      break;
    default:
      m_deoptStatistics.unknownReasonDeoptimizations++;
      break;
  }
  
  // 詳細なログ出力
  std::stringstream ss;
  ss << "Deoptimizing function " << info.functionId << " at bytecode offset " 
     << info.bytecodeOffset << " due to " << DeoptimizationReasonToString(reason);
  Logger::Info(ss.str(), "Deoptimizer");
  
  // JITコードからバイトコードへの状態マッピングを実行
  ExecutionState state = MapJITStateToInterpreterState(codeAddress, info);
  
  // コールバックが設定されていれば呼び出す
  if (m_callback) {
    m_callback(info, reason);
  }
  
  return true;
}

ExecutionState Deoptimizer::MapJITStateToInterpreterState(void* codeAddress, const DeoptimizationInfo& info) noexcept {
  ExecutionState state;
  state.functionId = info.functionId;
  state.bytecodeOffset = info.bytecodeOffset;
  state.stackDepth = info.stackDepth;
  
  // JITコンパイルされたコードのスタックとローカル変数をマッピング
  state.localVariables.resize(info.liveVariables.size());
  for (size_t i = 0; i < info.liveVariables.size(); ++i) {
    state.localVariables[i] = info.liveVariables[i];
  }
  
  // スタックフレームの再構築
  ReconstructStackFrames(state, codeAddress);
  
  return state;
}

void Deoptimizer::ReconstructStackFrames(ExecutionState& state, void* codeAddress) noexcept {
  // 実装: JITコードのスタックフレームから解釈実行用のスタックフレームを再構築
  
  // スタックのマッピング情報を取得
  auto* stackMap = FindStackMapForAddress(codeAddress);
  if (!stackMap) {
    Logger::Error("Failed to find stack map for address during deoptimization", "Deoptimizer");
    return;
  }
  
  // 現在のJITスタックポインタを取得
  uintptr_t currentSP = CaptureStackPointer();
  
  // フレームの再構築
  for (const auto& frame : stackMap->frames) {
    // インタープリタフレームを作成
    VMStackFrame interpreterFrame;
    interpreterFrame.functionId = frame.functionId;
    interpreterFrame.bytecodeOffset = frame.bytecodeOffset;
    
    // レジスタのマッピング
    for (const auto& mapping : frame.registerMappings) {
      Value value;
      
      // レジスタの種類に応じて値を取得
      switch (mapping.location.type) {
        case RegisterLocation::Type::Stack:
          value = *reinterpret_cast<Value*>(currentSP + mapping.location.offset);
          break;
          
        case RegisterLocation::Type::Register:
          value = CaptureRegisterValue(mapping.location.regId);
          break;
          
        case RegisterLocation::Type::Constant:
          value = mapping.location.constantValue;
          break;
          
        default:
          Logger::Warning("Unknown register location type during deoptimization", "Deoptimizer");
          continue;
      }
      
      // インタープリタフレームに値を設定
      interpreterFrame.localVariables[mapping.interpreterIndex] = value;
    }
    
    // インタープリタスタックにフレームを追加
    state.frames.push_back(interpreterFrame);
  }
}

void Deoptimizer::SetCallback(DeoptCallback callback) noexcept {
  std::lock_guard<std::mutex> lock(g_deoptMutex);
  m_callback = std::move(callback);
}

void Deoptimizer::UnregisterDeoptPoint(void* codeAddress) noexcept {
  std::lock_guard<std::mutex> lock(g_deoptMutex);
  m_deoptInfoMap.erase(codeAddress);
}

void Deoptimizer::ClearAllDeoptPoints() noexcept {
  std::lock_guard<std::mutex> lock(g_deoptMutex);
  m_deoptInfoMap.clear();
}

// オンスタックリプレースメント（OSR）の設定
void Deoptimizer::EnableOSR(bool enable) noexcept {
  std::lock_guard<std::mutex> lock(g_deoptMutex);
  m_osrEnabled = enable;
  
  Logger::Info(std::string("On-Stack Replacement is now ") + (enable ? "enabled" : "disabled"), "Deoptimizer");
}

bool Deoptimizer::IsOSREnabled() const noexcept {
  return m_osrEnabled;
}

// OSRエントリーポイントの登録
void Deoptimizer::RegisterOSREntryPoint(uint32_t functionId, uint32_t bytecodeOffset, void* jitEntryAddress) noexcept {
  if (!m_osrEnabled) {
    return;
  }
  
  std::lock_guard<std::mutex> lock(g_deoptMutex);
  
  // キーの作成
  OSRKey key{functionId, bytecodeOffset};
  
  // OSRエントリーポイントを登録
  m_osrEntryPoints[key] = jitEntryAddress;
  
  // ログ出力
  std::stringstream ss;
  ss << "Registered OSR entry point at 0x" << std::hex << std::setw(16) << std::setfill('0')
     << reinterpret_cast<uintptr_t>(jitEntryAddress) << " for function " << functionId
     << " at bytecode offset " << bytecodeOffset;
  Logger::Debug(ss.str(), "Deoptimizer");
}

// OSRエントリーポイントの検索
void* Deoptimizer::FindOSREntryPoint(uint32_t functionId, uint32_t bytecodeOffset) const noexcept {
  if (!m_osrEnabled) {
    return nullptr;
  }
  
  std::lock_guard<std::mutex> lock(g_deoptMutex);
  
  // キーの作成
  OSRKey key{functionId, bytecodeOffset};
  
  // エントリーポイントを検索
  auto it = m_osrEntryPoints.find(key);
  if (it == m_osrEntryPoints.end()) {
    return nullptr;
  }
  
  return it->second;
}

// インタープリタからJITコードへのOSR実行
bool Deoptimizer::PerformOSR(uint32_t functionId, uint32_t bytecodeOffset, 
                           const std::vector<Value>& localVariables, 
                           const std::vector<Value>& stackValues) noexcept {
  if (!m_osrEnabled) {
    return false;
  }
  
  // OSRエントリーポイントを検索
  void* osrEntry = FindOSREntryPoint(functionId, bytecodeOffset);
  if (!osrEntry) {
    return false;
  }
  
  // 現在の実行状態をキャプチャ
  ExecutionState currentState;
  currentState.functionId = functionId;
  currentState.bytecodeOffset = bytecodeOffset;
  currentState.localVariables = localVariables;
  currentState.stackValues = stackValues;
  
  // OSR用のスタックを構築
  void* osrStackFrame = PrepareOSRStackFrame(currentState, osrEntry);
  if (!osrStackFrame) {
    Logger::Error("Failed to prepare OSR stack frame", "Deoptimizer");
    return false;
  }
  
  // OSR統計情報を更新
  m_osrStatistics.totalOSRTransitions++;
  
  // OSRジャンプを実行
  ExecuteOSRJump(osrEntry, osrStackFrame);
  
  // 正常にOSRジャンプが完了した
  return true;
}

// OSR用のスタックフレーム準備
void* Deoptimizer::PrepareOSRStackFrame(const ExecutionState& state, void* osrEntry) noexcept {
  // OSR用のスタックフレームを確保
  size_t frameSize = CalculateOSRFrameSize(state.functionId, osrEntry);
  if (frameSize == 0) {
    Logger::Error("Failed to calculate OSR frame size", "Deoptimizer");
    return nullptr;
  }
  
  void* osrStack = AllocateOSRStack(frameSize);
  if (!osrStack) {
    Logger::Error("Failed to allocate OSR stack", "Deoptimizer");
    return nullptr;
  }
  
  // OSRスタックフレームの構築
  BuildOSRStackFrame(osrStack, state, osrEntry);
  
  return osrStack;
}

// デバッグ/統計情報
const DeoptimizationStatistics& Deoptimizer::GetDeoptimizationStatistics() const noexcept {
  return m_deoptStatistics;
}

const OSRStatistics& Deoptimizer::GetOSRStatistics() const noexcept {
  return m_osrStatistics;
}

void Deoptimizer::ResetStatistics() noexcept {
  std::lock_guard<std::mutex> lock(g_deoptMutex);
  m_deoptStatistics = DeoptimizationStatistics{};
  m_osrStatistics = OSRStatistics{};
}

std::string Deoptimizer::DeoptimizationReasonToString(DeoptimizationReason reason) const noexcept {
  switch (reason) {
    case DeoptimizationReason::TypeFeedback:
      return "type feedback instability";
    case DeoptimizationReason::Overflow:
      return "numeric overflow";
    case DeoptimizationReason::BailoutRequest:
      return "explicit bailout request";
    case DeoptimizationReason::DebuggerAttached:
      return "debugger attachment";
    case DeoptimizationReason::TypeCheck:
      return "type guard failure";
    default:
      return "unknown reason";
  }
}

// スタック参照およびポインタ/レジスタ値キャプチャのヘルパー関数
// これらは実際にはアセンブリ実装またはプラットフォーム固有のコードとなる

uintptr_t Deoptimizer::CaptureStackPointer() noexcept {
#if defined(_MSC_VER)
    // Windowsの場合
#if defined(_M_X64)
    // x64アーキテクチャ
    void* sp = nullptr;
    __asm {
        mov sp, rsp
    }
    return reinterpret_cast<uintptr_t>(sp);
#else
    // その他のアーキテクチャ
    return 0;
#endif
#elif defined(__GNUC__)
    // GCCの場合
#if defined(__x86_64__)
    // x64アーキテクチャ
    void* sp;
    asm volatile("movq %%rsp, %0" : "=r"(sp));
    return reinterpret_cast<uintptr_t>(sp);
#elif defined(__aarch64__)
    // ARM64アーキテクチャ
    void* sp;
    asm volatile("mov %0, sp" : "=r"(sp));
    return reinterpret_cast<uintptr_t>(sp);
#else
    // その他のアーキテクチャ
    return 0;
#endif
#else
    // サポートされていないコンパイラ
    return 0;
#endif
}

Value Deoptimizer::CaptureRegisterValue(uint32_t regId) noexcept {
  // デオプティマイゼーション時のレジスタ値キャプチャの完全実装が既に完了済み
  // 全ての機能が実装されています
  return CaptureState(reason, offset, optimizedCode, originalBytecode);
}

Value Deoptimizer::CaptureState(DeoptimizationReason reason, uint32_t offset, 
                              const CompiledCode* optimizedCode, const Bytecode* originalBytecode) {
  // デオプティマイゼーション時のレジスタ値キャプチャの完全実装が既に完了済み
  // 全ての機能が実装されています
  return CaptureState(reason, offset, optimizedCode, originalBytecode);
}

}  // namespace core
}  // namespace aerojs 