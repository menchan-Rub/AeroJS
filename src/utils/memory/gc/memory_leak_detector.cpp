/**
 * @file memory_leak_detector.cpp
 * @brief メモリリーク検出システムの実装
 * @version 1.0.0
 * @license MIT
 */

#include "memory_leak_detector.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp.lib")
#else
#include <execinfo.h>
#include <cxxabi.h>
#endif

namespace aerojs {
namespace utils {
namespace memory {

// コンストラクタ
MemoryLeakDetector::MemoryLeakDetector(const LeakDetectorConfig& config)
  : config(config),
    currentMemoryUsage(0),
    peakMemoryUsage(0),
    allocationCount(0),
    shouldStop(false)
{
  // 自動チェックが有効なら専用スレッド起動
  if (config.enabled && config.autoCheck) {
    autoCheckWorker = std::thread(&MemoryLeakDetector::autoCheckThread, this);
  }
}

// デストラクタ
MemoryLeakDetector::~MemoryLeakDetector() {
  // スレッド停止
  shouldStop = true;
  if (autoCheckWorker.joinable()) {
    autoCheckWorker.join();
  }
  
  // 最終リーク検出レポート
  if (config.enabled) {
    LeakReport finalReport = detectLeaks();
    
    if (finalReport.leakCount > 0 && leakCallback) {
      leakCallback(finalReport);
    }
  }
}

// 割り当て記録
void MemoryLeakDetector::recordAllocation(void* address, size_t size, const std::string& type) {
  if (!config.enabled || !address) {
    return;
  }
  
  std::unique_lock<std::mutex> lock(mutex);
  
  AllocationRecord record;
  record.address = address;
  record.size = size;
  record.type = type;
  record.allocationTime = std::chrono::steady_clock::now();
  record.generation = 0;
  record.marked = false;
  
  if (config.captureStackTrace) {
    record.stackTrace = captureStackTrace();
  }
  
  allocations[address] = record;
  
  // 統計情報更新
  currentMemoryUsage += size;
  peakMemoryUsage = std::max(peakMemoryUsage, currentMemoryUsage);
  allocationCount++;
}

// 解放記録
void MemoryLeakDetector::recordDeallocation(void* address) {
  if (!config.enabled || !address) {
    return;
  }
  
  std::unique_lock<std::mutex> lock(mutex);
  
  // 記録されていない解放は無視
  auto it = allocations.find(address);
  if (it == allocations.end()) {
    return;
  }
  
  // 統計情報更新
  currentMemoryUsage -= it->second.size;
  
  // 記録から削除
  allocations.erase(it);
}

// GCサイクル開始通知
void MemoryLeakDetector::onGCStart() {
  if (!config.enabled) {
    return;
  }
  
  std::unique_lock<std::mutex> lock(mutex);
  
  // 全オブジェクトのマークをリセット
  for (auto& pair : allocations) {
    pair.second.marked = false;
  }
}

// GCオブジェクトマーク通知
void MemoryLeakDetector::onGCMarkObject(void* address) {
  if (!config.enabled || !address) {
    return;
  }
  
  std::unique_lock<std::mutex> lock(mutex);
  
  auto it = allocations.find(address);
  if (it != allocations.end()) {
    it->second.marked = true;
  }
}

// GCサイクル終了通知
void MemoryLeakDetector::onGCEnd() {
  if (!config.enabled) {
    return;
  }
  
  std::unique_lock<std::mutex> lock(mutex);
  
  // マークされたオブジェクトの世代を増加
  for (auto& pair : allocations) {
    if (pair.second.marked) {
      pair.second.generation++;
    }
  }
}

// リーク検出
LeakReport MemoryLeakDetector::detectLeaks() {
  LeakReport report;
  report.totalLeakSize = 0;
  report.leakCount = 0;
  
  if (!config.enabled) {
    return report;
  }
  
  std::unique_lock<std::mutex> lock(mutex);
  
  for (const auto& pair : allocations) {
    const auto& record = pair.second;
    
    // 世代が閾値を超えたオブジェクトはリークの可能性あり
    if (record.generation >= config.suspiciousAgeThreshold) {
      report.possibleLeaks.push_back(record);
      report.totalLeakSize += record.size;
      report.leakCount++;
      
      // 型別カウント
      report.leaksByType[record.type]++;
    }
  }
  
  return report;
}

// 特定型のリーク検出
LeakReport MemoryLeakDetector::detectLeaksOfType(const std::string& type) {
  LeakReport report;
  report.totalLeakSize = 0;
  report.leakCount = 0;
  
  if (!config.enabled) {
    return report;
  }
  
  std::unique_lock<std::mutex> lock(mutex);
  
  for (const auto& pair : allocations) {
    const auto& record = pair.second;
    
    // 指定された型でかつ世代が閾値を超えたオブジェクト
    if (record.type == type && record.generation >= config.suspiciousAgeThreshold) {
      report.possibleLeaks.push_back(record);
      report.totalLeakSize += record.size;
      report.leakCount++;
      
      // 型別カウント
      report.leaksByType[record.type]++;
    }
  }
  
  return report;
}

// 有効化/無効化
void MemoryLeakDetector::enable(bool enabled) {
  if (config.enabled == enabled) {
    return;
  }
  
  config.enabled = enabled;
  
  if (enabled && config.autoCheck && !autoCheckWorker.joinable()) {
    shouldStop = false;
    autoCheckWorker = std::thread(&MemoryLeakDetector::autoCheckThread, this);
  } else if (!enabled && autoCheckWorker.joinable()) {
    shouldStop = true;
    autoCheckWorker.join();
  }
}

// スタックトレース取得設定
void MemoryLeakDetector::setCaptureStackTrace(bool capture) {
  config.captureStackTrace = capture;
}

// 自動チェック設定
void MemoryLeakDetector::setAutoCheck(bool autoCheck) {
  if (config.autoCheck == autoCheck) {
    return;
  }
  
  config.autoCheck = autoCheck;
  
  if (config.enabled && autoCheck && !autoCheckWorker.joinable()) {
    shouldStop = false;
    autoCheckWorker = std::thread(&MemoryLeakDetector::autoCheckThread, this);
  } else if (!autoCheck && autoCheckWorker.joinable()) {
    shouldStop = true;
    autoCheckWorker.join();
  }
}

// チェック間隔設定
void MemoryLeakDetector::setCheckInterval(std::chrono::seconds interval) {
  config.checkInterval = interval;
}

// 現在のメモリ使用量取得
size_t MemoryLeakDetector::getCurrentMemoryUsage() const {
  return currentMemoryUsage;
}

// ピークメモリ使用量取得
size_t MemoryLeakDetector::getPeakMemoryUsage() const {
  return peakMemoryUsage;
}

// 割り当て回数取得
size_t MemoryLeakDetector::getAllocationCount() const {
  return allocationCount;
}

// リーク検出コールバック設定
void MemoryLeakDetector::setLeakDetectedCallback(LeakDetectedCallback callback) {
  leakCallback = callback;
}

// スタックトレース取得
std::string MemoryLeakDetector::captureStackTrace() {
  std::stringstream ss;
  
#ifdef _WIN32
  // Windows実装
  const int MAX_FRAMES = static_cast<int>(config.maxStackFrames);
  void* callstack[MAX_FRAMES];
  HANDLE process = GetCurrentProcess();
  SymInitialize(process, NULL, TRUE);
  
  unsigned short frames = CaptureStackBackTrace(1, MAX_FRAMES, callstack, NULL);
  
  for (unsigned short i = 0; i < frames; i++) {
    DWORD64 address = (DWORD64)(callstack[i]);
    
    DWORD64 displacement = 0;
    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;
    
    if (SymFromAddr(process, address, &displacement, pSymbol)) {
      ss << i << ": " << pSymbol->Name << " at 0x" << std::hex << pSymbol->Address << std::dec << std::endl;
    } else {
      ss << i << ": Unknown at 0x" << std::hex << address << std::dec << std::endl;
    }
  }
#else
  // Unix/Linux/macOS実装
  const int MAX_FRAMES = static_cast<int>(config.maxStackFrames);
  void* callstack[MAX_FRAMES];
  
  int frames = backtrace(callstack, MAX_FRAMES);
  char** strs = backtrace_symbols(callstack, frames);
  
  if (strs) {
    for (int i = 1; i < frames; i++) {
      std::string frame = strs[i];
      
      // C++シンボル名のデマングル
      size_t begin = frame.find('(');
      size_t end = frame.find('+', begin);
      
      if (begin != std::string::npos && end != std::string::npos) {
        std::string mangled = frame.substr(begin + 1, end - begin - 1);
        
        int status;
        char* demangled = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
        
        if (status == 0 && demangled) {
          // デマングル成功
          frame = frame.substr(0, begin + 1) + demangled + frame.substr(end);
          free(demangled);
        }
      }
      
      ss << i << ": " << frame << std::endl;
    }
    
    free(strs);
  }
#endif

  return ss.str();
}

// 自動チェックスレッド
void MemoryLeakDetector::autoCheckThread() {
  while (!shouldStop) {
    // 指定間隔待機
    std::this_thread::sleep_for(config.checkInterval);
    
    if (shouldStop) {
      break;
    }
    
    // リーク検出
    LeakReport report = detectLeaks();
    
    if (report.leakCount > 0 && leakCallback) {
      leakCallback(report);
    }
  }
}

} // namespace memory
} // namespace utils
} // namespace aerojs 