/**
 * @file memory_leak_detector.h
 * @brief メモリリーク検出システム
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <functional>

namespace aerojs {
namespace utils {
namespace memory {

// メモリ割り当て追跡情報
struct AllocationRecord {
  void* address;                  // 割り当てアドレス
  size_t size;                    // サイズ（バイト単位）
  std::string type;               // オブジェクト型
  std::chrono::steady_clock::time_point allocationTime; // 割り当て時刻
  std::string stackTrace;         // スタックトレース
  unsigned int generation;        // 世代カウント（GC生存回数）
  bool marked;                    // 現在のGCサイクルでマーク済みか
};

// リーク検出設定
struct LeakDetectorConfig {
  bool enabled = false;           // リーク検出有効フラグ
  bool captureStackTrace = true;  // スタックトレース取得フラグ
  size_t maxStackFrames = 20;     // 最大スタックフレーム数
  bool trackSizes = true;         // サイズ追跡フラグ
  bool autoCheck = true;          // 自動チェックフラグ
  std::chrono::seconds checkInterval{60}; // 自動チェック間隔（秒）
  size_t suspiciousAgeThreshold = 10; // 怪しいと見なす世代閾値
};

// リーク検出レポート
struct LeakReport {
  std::vector<AllocationRecord> possibleLeaks;  // リークの可能性がある割り当て
  size_t totalLeakSize;                         // 合計リークサイズ
  size_t leakCount;                             // リーク数
  std::unordered_map<std::string, size_t> leaksByType; // 型別リーク数
};

// メモリリーク検出クラス
class MemoryLeakDetector {
public:
  MemoryLeakDetector(const LeakDetectorConfig& config = LeakDetectorConfig());
  ~MemoryLeakDetector();
  
  // 割り当て/解放の記録
  void recordAllocation(void* address, size_t size, const std::string& type);
  void recordDeallocation(void* address);
  
  // GCサイクル通知
  void onGCStart();
  void onGCMarkObject(void* address);
  void onGCEnd();
  
  // リーク検出実行
  LeakReport detectLeaks();
  LeakReport detectLeaksOfType(const std::string& type);
  
  // 設定
  void enable(bool enabled);
  void setCaptureStackTrace(bool capture);
  void setAutoCheck(bool autoCheck);
  void setCheckInterval(std::chrono::seconds interval);
  
  // 統計情報
  size_t getCurrentMemoryUsage() const;
  size_t getPeakMemoryUsage() const;
  size_t getAllocationCount() const;
  
  // コールバック設定
  using LeakDetectedCallback = std::function<void(const LeakReport&)>;
  void setLeakDetectedCallback(LeakDetectedCallback callback);
  
private:
  // スタックトレース取得
  std::string captureStackTrace();
  
  // 自動チェックスレッド
  void autoCheckThread();
  
  // メンバ変数
  LeakDetectorConfig config;
  std::unordered_map<void*, AllocationRecord> allocations;
  std::mutex mutex;
  
  size_t currentMemoryUsage;
  size_t peakMemoryUsage;
  size_t allocationCount;
  
  LeakDetectedCallback leakCallback;
  
  std::thread autoCheckWorker;
  std::atomic<bool> shouldStop;
};

} // namespace memory
} // namespace utils
} // namespace aerojs 