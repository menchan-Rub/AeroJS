/**
 * @file engine.cpp
 * @brief AeroJS JavaScript エンジンのメインエンジンクラス実装
 * @version 0.1.0
 * @license MIT
 */

#include "engine.h"

#include <algorithm>
#include <chrono>
#include <thread>

#include "../utils/memory/allocators/memory_allocator.h"
#include "../utils/memory/pool/memory_pool.h"
#include "../utils/time/timer.h"
#include "context.h"
#include "gc/collector/mark_sweep/mark_sweep_collector.h"
#include "gc/collector/generational/generational_collector.h"
#include "runtime/types/object_type.h"
#include "runtime/builtins/builtins_manager.h"
#include "vm/interpreter/interpreter.h"
#include "jit/profiler/execution_profiler.h"

namespace aerojs {
namespace core {

// シングルトンインスタンス
Engine* Engine::instance_ = nullptr;
std::mutex Engine::instanceMutex_;

Engine* Engine::getInstance() {
  std::lock_guard<std::mutex> lock(instanceMutex_);
  if (!instance_) {
    // インスタンスが存在しない場合、新しく作成
    instance_ = new Engine();
  }
  return instance_;
}

void Engine::destroyInstance() {
  std::lock_guard<std::mutex> lock(instanceMutex_);
  if (instance_) {
    delete instance_;
    instance_ = nullptr;
  }
}

Engine::Engine()
    : memoryAllocator_(nullptr),
      memoryPoolManager_(nullptr),
      jitEnabled_(false),
      jitThreshold_(1000),
      optimizationLevel_(2),
      gcInterval_(1000),
      lastGcTime_(0),
      memoryLimit_(1024 * 1024 * 1024),  // デフォルト1GB
      gcCollector_(nullptr),
      profiler_(nullptr) {

  // 標準アロケータを作成
  memoryAllocator_ = new utils::memory::StandardAllocator();

  // メモリプールマネージャを初期化
  memoryPoolManager_ = new utils::memory::MemoryPoolManager(memoryAllocator_);
  
  // GCコレクタを初期化（デフォルトはマーク＆スイープ）
  gcCollector_ = new gc::collector::MarkSweepCollector(memoryAllocator_);
  
  // 実行プロファイラを初期化
  profiler_ = new jit::profiler::ExecutionProfiler();

  // エンジン設定のデフォルト値を初期化
  parameters_["version"] = "0.1.0";
  parameters_["strictMode"] = "true";
  parameters_["enableDebugger"] = "false";
  parameters_["gcInterval"] = std::to_string(gcInterval_);
  parameters_["memoryLimit"] = std::to_string(memoryLimit_);
  parameters_["jitEnabled"] = std::to_string(jitEnabled_);
  parameters_["jitThreshold"] = std::to_string(jitThreshold_);
  parameters_["optimizationLevel"] = std::to_string(optimizationLevel_);
  parameters_["gcType"] = "markSweep"; // デフォルトのGCタイプ

  // 最後のGC時間を現在の時間で初期化
  lastGcTime_ = utils::time::Timer::getCurrentTimeMillis();
  
  // 組み込みオブジェクトマネージャを初期化
  builtinsManager_ = new runtime::builtins::BuiltinsManager();
  
  // インタープリタを初期化
  interpreter_ = new vm::interpreter::Interpreter();
}

Engine::~Engine() {
  // 全てのコンテキストを破棄
  std::lock_guard<std::mutex> contextLock(contextsMutex_);
  while (!contexts_.empty()) {
    Context* context = contexts_.back();
    contexts_.pop_back();
    delete context;
  }

  // エンジンデータのクリーンアップ
  {
    std::lock_guard<std::mutex> dataLock(engineDataMutex_);
    for (auto& entry : engineData_) {
      if (entry.second.cleaner) {
        entry.second.cleaner(entry.second.data);
      }
    }
    engineData_.clear();
  }

  // インタープリタを解放
  delete interpreter_;
  
  // 組み込みオブジェクトマネージャを解放
  delete builtinsManager_;
  
  // プロファイラを解放
  delete profiler_;
  
  // GCコレクタを解放
  delete gcCollector_;

  // メモリプールマネージャを解放
  delete memoryPoolManager_;

  // メモリアロケータを解放
  delete memoryAllocator_;
}

void Engine::setParameter(const std::string& name, const std::string& value) {
  parameters_[name] = value;

  // 特定のパラメータは内部状態も更新
  if (name == "jitEnabled") {
    jitEnabled_ = (value == "true");
  } else if (name == "jitThreshold") {
    try {
      jitThreshold_ = std::stoi(value);
    } catch (...) {
      // 変換エラーは無視
    }
  } else if (name == "optimizationLevel") {
    try {
      optimizationLevel_ = std::stoi(value);
      // 最適化レベルに応じてJITの設定を調整
      if (optimizationLevel_ > 0) {
        enableJIT(true);
      }
    } catch (...) {
      // 変換エラーは無視
    }
  } else if (name == "gcInterval") {
    try {
      gcInterval_ = std::stoi(value);
    } catch (...) {
      // 変換エラーは無視
    }
  } else if (name == "memoryLimit") {
    try {
      memoryLimit_ = std::stoull(value);
    } catch (...) {
      // 変換エラーは無視
    }
  } else if (name == "gcType") {
    // GCタイプの変更
    if (value == "generational" && 
        dynamic_cast<gc::collector::GenerationalCollector*>(gcCollector_) == nullptr) {
      // 世代別GCに切り替え
      delete gcCollector_;
      gcCollector_ = new gc::collector::GenerationalCollector(memoryAllocator_);
    } else if (value == "markSweep" && 
               dynamic_cast<gc::collector::MarkSweepCollector*>(gcCollector_) == nullptr) {
      // マーク＆スイープGCに切り替え
      delete gcCollector_;
      gcCollector_ = new gc::collector::MarkSweepCollector(memoryAllocator_);
    }
  }
}

std::string Engine::getParameter(const std::string& name) const {
  auto it = parameters_.find(name);
  if (it != parameters_.end()) {
    return it->second;
  }
  return "";
}

void Engine::collectGarbage(Context* context, bool force) {
  // 直近のGC実行からの経過時間を確認
  uint64_t currentTime = utils::time::Timer::getCurrentTimeMillis();
  uint64_t elapsed = currentTime - lastGcTime_;

  // forceフラグがtrueまたはGC間隔を超えている場合のみGCを実行
  if (force || elapsed > gcInterval_) {
    // GCの実行
    performGarbageCollection(context);

    // 最後のGC実行時間を更新
    lastGcTime_ = currentTime;
  }
}

void Engine::collectGarbageGlobal(bool force) {
  std::lock_guard<std::mutex> lock(contextsMutex_);

  // 全てのコンテキストに対してGCを実行
  for (auto* context : contexts_) {
    collectGarbage(context, force);
  }
}

void Engine::enumerateContexts(ContextEnumerator callback, void* userData) {
  std::lock_guard<std::mutex> lock(contextsMutex_);

  for (auto* context : contexts_) {
    if (!callback(context, userData)) {
      break;
    }
  }
}

void Engine::registerContext(Context* context) {
  if (!context) return;

  std::lock_guard<std::mutex> lock(contextsMutex_);

  // 既に登録済みでないことを確認
  auto it = std::find(contexts_.begin(), contexts_.end(), context);
  if (it == contexts_.end()) {
    contexts_.push_back(context);
    
    // コンテキスト初期化時に組み込みオブジェクトを設定
    builtinsManager_->initializeContext(context);
  }
}

void Engine::unregisterContext(Context* context) {
  if (!context) return;

  std::lock_guard<std::mutex> lock(contextsMutex_);

  // コンテキストを検索して削除
  auto it = std::find(contexts_.begin(), contexts_.end(), context);
  if (it != contexts_.end()) {
    // コンテキスト解放前に必要なクリーンアップを実行
    builtinsManager_->cleanupContext(context);
    contexts_.erase(it);
  }
}

bool Engine::enableJIT(bool enable) {
  // JITコンパイラの有効/無効を切り替え
  jitEnabled_ = enable;
  parameters_["jitEnabled"] = enable ? "true" : "false";
  
  // JIT関連のリソースを初期化または解放
  if (enable) {
    // JITコンパイラの初期化処理
    profiler_->reset();
  } else {
    // JITコンパイルされたコードをクリア
    profiler_->clearCompiledCode();
  }
  
  return true;
}

bool Engine::setJITThreshold(int threshold) {
  if (threshold < 0) {
    return false;
  }

  jitThreshold_ = threshold;
  parameters_["jitThreshold"] = std::to_string(threshold);
  
  // プロファイラのしきい値も更新
  profiler_->setCompilationThreshold(threshold);
  
  return true;
}

bool Engine::setOptimizationLevel(int level) {
  if (level < 0 || level > 3) {
    return false;
  }

  optimizationLevel_ = level;
  parameters_["optimizationLevel"] = std::to_string(level);
  
  // 最適化レベルに応じてJITの設定を調整
  if (level == 0) {
    // 最適化なし
    enableJIT(false);
  } else if (level == 1) {
    // 基本的な最適化のみ
    enableJIT(true);
    setJITThreshold(2000); // 高めのしきい値
  } else if (level == 2) {
    // 標準的な最適化
    enableJIT(true);
    setJITThreshold(1000); // 標準的なしきい値
  } else if (level == 3) {
    // 積極的な最適化
    enableJIT(true);
    setJITThreshold(500); // 低めのしきい値
  }
  
  return true;
}

bool Engine::setMemoryLimit(size_t limit) {
  memoryLimit_ = limit;
  parameters_["memoryLimit"] = std::to_string(limit);
  
  // メモリアロケータに制限を設定
  memoryAllocator_->setMemoryLimit(limit);
  
  return true;
}

void Engine::getMemoryStats(MemoryStats* stats) {
  if (!stats) return;

  // 統計情報の初期化
  stats->totalHeapSize = 0;
  stats->usedHeapSize = 0;
  stats->heapSizeLimit = memoryLimit_;
  stats->externalMemorySize = 0;
  stats->objectCount = 0;
  stats->functionCount = 0;
  stats->arrayCount = 0;
  stats->stringCount = 0;
  stats->symbolCount = 0;
  stats->gcCount = gcStats_.gcCount;
  stats->fullGcCount = gcStats_.fullGcCount;
  stats->gcTime = gcStats_.gcTime;

  // メモリアロケータから統計情報を取得
  if (memoryAllocator_) {
    stats->totalHeapSize = memoryAllocator_->getTotalAllocatedSize();
    stats->usedHeapSize = memoryAllocator_->getCurrentAllocatedSize();
  }

  // コンテキスト毎の統計情報を集約
  std::lock_guard<std::mutex> lock(contextsMutex_);
  for (auto* context : contexts_) {
    // 各コンテキストから統計情報を取得
    ContextStats contextStats;
    context->getStatistics(&contextStats);
    
    // オブジェクト数を集計
    stats->objectCount += contextStats.objectCount;
    stats->functionCount += contextStats.functionCount;
    stats->arrayCount += contextStats.arrayCount;
    stats->stringCount += contextStats.stringCount;
    stats->symbolCount += contextStats.symbolCount;
    
    // 外部メモリサイズを加算
    stats->externalMemorySize += contextStats.externalMemorySize;
  }
}

bool Engine::setEngineData(const std::string& key, void* data, EngineDataCleaner cleaner) {
  std::lock_guard<std::mutex> lock(engineDataMutex_);

  // 既存のデータがある場合はクリーンアップ
  auto it = engineData_.find(key);
  if (it != engineData_.end()) {
    if (it->second.cleaner) {
      it->second.cleaner(it->second.data);
    }
  }

  // 新しいデータを登録
  EngineDataEntry entry = {data, cleaner};
  engineData_[key] = entry;
  return true;
}

void* Engine::getEngineData(const std::string& key) const {
  std::lock_guard<std::mutex> lock(engineDataMutex_);

  auto it = engineData_.find(key);
  if (it != engineData_.end()) {
    return it->second.data;
  }
  return nullptr;
}

bool Engine::removeEngineData(const std::string& key) {
  std::lock_guard<std::mutex> lock(engineDataMutex_);

  auto it = engineData_.find(key);
  if (it != engineData_.end()) {
    if (it->second.cleaner) {
      it->second.cleaner(it->second.data);
    }
    engineData_.erase(it);
    return true;
  }
  return false;
}

void Engine::performGarbageCollection(Context* context) {
  if (!context) return;

  // GC実行開始時間を記録
  auto startTime = utils::time::Timer::getCurrentTimeMillis();

  // GCの実行前にJITコンパイラに通知
  if (jitEnabled_) {
    profiler_->notifyGarbageCollectionStart();
  }

  // GCコレクタを使用してガベージコレクションを実行
  bool isFullGC = gcCollector_->collect(context);

  // GC実行後にJITコンパイラに通知
  if (jitEnabled_) {
    profiler_->notifyGarbageCollectionEnd();
  }

  // GC実行終了時間を記録
  auto endTime = utils::time::Timer::getCurrentTimeMillis();

  // GC統計情報を更新
  gcStats_.gcCount++;
  if (isFullGC) {
    gcStats_.fullGcCount++;
  }
  gcStats_.gcTime += (endTime - startTime);
  
  // メモリ使用量が制限に近づいている場合は、次回のGCを早めに実行
  size_t currentMemoryUsage = memoryAllocator_->getCurrentAllocatedSize();
  if (currentMemoryUsage > memoryLimit_ * 0.8) { // 80%以上使用している場合
    gcInterval_ = gcInterval_ / 2; // GC間隔を半分に
    if (gcInterval_ < 100) { // 最小値を設定
      gcInterval_ = 100;
    }
  } else if (currentMemoryUsage < memoryLimit_ * 0.5) { // 50%未満の場合
    // GC間隔を元に戻す（ただし最大値は設定値）
    int defaultInterval = std::stoi(parameters_["gcInterval"]);
    gcInterval_ = std::min(gcInterval_ * 2, defaultInterval);
  }
}

void Engine::markReachableObjects(Context* context) {
  // GCコレクタのマークフェーズを直接呼び出す
  gcCollector_->markPhase(context);
}

void Engine::sweepUnmarkedObjects(Context* context) {
  // GCコレクタのスイープフェーズを直接呼び出す
  gcCollector_->sweepPhase(context);
}

void Engine::scheduleGarbageCollection() {
  // バックグラウンドスレッドでGCを実行するためのスケジューリング
  std::thread gcThread([this]() {
    // 全コンテキストに対してGCを実行
    this->collectGarbageGlobal(true);
  });
  
  // デタッチしてバックグラウンドで実行
  gcThread.detach();
}

runtime::builtins::BuiltinsManager* Engine::getBuiltinsManager() {
  return builtinsManager_;
}

vm::interpreter::Interpreter* Engine::getInterpreter() {
  return interpreter_;
}

jit::profiler::ExecutionProfiler* Engine::getProfiler() {
  return profiler_;
}

gc::collector::GarbageCollector* Engine::getGarbageCollector() {
  return gcCollector_;
}

utils::memory::MemoryAllocator* Engine::getMemoryAllocator() {
  return memoryAllocator_;
}

utils::memory::MemoryPoolManager* Engine::getMemoryPoolManager() {
  return memoryPoolManager_;
}

}  // namespace core
}  // namespace aerojs