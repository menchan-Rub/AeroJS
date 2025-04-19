/**
 * @file engine.cpp
 * @brief AeroJS JavaScript エンジンのメインエンジンクラス実装
 * @version 0.1.0
 * @license MIT
 */

#include "engine.h"
#include "context.h"
#include "../utils/memory/allocators/memory_allocator.h"
#include "../utils/memory/pool/memory_pool.h"
#include "../utils/time/timer.h"
#include <algorithm>
#include <chrono>
#include <thread>

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
      memoryLimit_(1024 * 1024 * 1024) { // デフォルト1GB
    
    // 標準アロケータを作成
    memoryAllocator_ = new utils::memory::StandardAllocator();
    
    // メモリプールマネージャを初期化
    memoryPoolManager_ = new utils::memory::MemoryPoolManager(memoryAllocator_);
    
    // エンジン設定のデフォルト値を初期化
    parameters_["version"] = "0.1.0";
    parameters_["strictMode"] = "true";
    parameters_["enableDebugger"] = "false";
    parameters_["gcInterval"] = std::to_string(gcInterval_);
    parameters_["memoryLimit"] = std::to_string(memoryLimit_);
    parameters_["jitEnabled"] = std::to_string(jitEnabled_);
    parameters_["jitThreshold"] = std::to_string(jitThreshold_);
    parameters_["optimizationLevel"] = std::to_string(optimizationLevel_);
    
    // 最後のGC時間を現在の時間で初期化
    lastGcTime_ = utils::time::Timer::getCurrentTimeMillis();
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
    }
}

void Engine::unregisterContext(Context* context) {
    if (!context) return;
    
    std::lock_guard<std::mutex> lock(contextsMutex_);
    
    // コンテキストを検索して削除
    auto it = std::find(contexts_.begin(), contexts_.end(), context);
    if (it != contexts_.end()) {
        contexts_.erase(it);
    }
}

bool Engine::enableJIT(bool enable) {
    // JITコンパイラの有効/無効を切り替え
    jitEnabled_ = enable;
    parameters_["jitEnabled"] = enable ? "true" : "false";
    return true;
}

bool Engine::setJITThreshold(int threshold) {
    if (threshold < 0) {
        return false;
    }
    
    jitThreshold_ = threshold;
    parameters_["jitThreshold"] = std::to_string(threshold);
    return true;
}

bool Engine::setOptimizationLevel(int level) {
    if (level < 0 || level > 3) {
        return false;
    }
    
    optimizationLevel_ = level;
    parameters_["optimizationLevel"] = std::to_string(level);
    return true;
}

bool Engine::setMemoryLimit(size_t limit) {
    memoryLimit_ = limit;
    parameters_["memoryLimit"] = std::to_string(limit);
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
    stats->gcCount = 0;
    stats->fullGcCount = 0;
    stats->gcTime = 0;
    
    // メモリアロケータから統計情報を取得
    if (memoryAllocator_) {
        stats->totalHeapSize = memoryAllocator_->getTotalAllocatedSize();
        stats->usedHeapSize = memoryAllocator_->getCurrentAllocatedSize();
    }
    
    // コンテキスト毎の統計情報を集約
    std::lock_guard<std::mutex> lock(contextsMutex_);
    for (auto* context : contexts_) {
        // 実際のアプリケーションでは、各コンテキストから統計情報を取得して集約する
        // 例：context->getStatistics(&contextStats) など
        // この実装ではプレースホルダー
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
    EngineDataEntry entry = { data, cleaner };
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
    
    // GCの実行
    // 実際のアプリケーションでは、マーク&スイープまたは世代別GCなどを実装
    // ここではプレースホルダー
    
    // 1. マークフェーズ: 到達可能なオブジェクトをマーク
    markReachableObjects(context);
    
    // 2. スイープフェーズ: マークされていないオブジェクトを回収
    sweepUnmarkedObjects(context);
    
    // GC実行終了時間を記録
    auto endTime = utils::time::Timer::getCurrentTimeMillis();
    
    // GC統計情報を更新
    gcStats_.gcCount++;
    gcStats_.gcTime += (endTime - startTime);
}

void Engine::markReachableObjects(Context* context) {
    // 実際のアプリケーションでは、以下の手順でマークフェーズを実装:
    // 1. ルートセット（グローバルオブジェクト、現在のスコープチェーン、スタック上の値など）をマーク
    // 2. マークされたオブジェクトから参照されるオブジェクトを再帰的にマーク
    
    // この実装ではプレースホルダー
}

void Engine::sweepUnmarkedObjects(Context* context) {
    // 実際のアプリケーションでは、以下の手順でスイープフェーズを実装:
    // 1. 全てのオブジェクトを走査し、マークされていないオブジェクトを解放
    // 2. 解放したオブジェクトのメモリをメモリプールに返却
    
    // この実装ではプレースホルダー
}

void Engine::scheduleGarbageCollection() {
    // バックグラウンドスレッドでGCを実行するためのスケジューリング
    // 実際のアプリケーションでは、バックグラウンドスレッドプールと連携
    
    // この実装ではプレースホルダー
}

} // namespace core
} // namespace aerojs 