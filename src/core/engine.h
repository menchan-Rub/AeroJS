/**
 * @file engine.h
 * @brief AeroJS 世界最高レベル JavaScript エンジン
 * @version 1.0.0 - World Class Edition
 * @license MIT
 */

#ifndef AEROJS_CORE_ENGINE_H
#define AEROJS_CORE_ENGINE_H

#include "value.h"
#include "../utils/memory/allocators/memory_allocator.h"
#include "../utils/memory/pool/memory_pool.h"
#include "../utils/memory/gc/garbage_collector.h"
#include "../utils/time/timer.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <atomic>
#include <future>
#include <thread>
#include <unordered_map>

namespace aerojs {
namespace core {

// 前方宣言
class Context;

namespace runtime {
namespace builtins {
class BuiltinsManager;
}
}

/**
 * @brief エンジンエラーの種類（拡張版）
 */
enum class EngineError {
    None,
    InitializationFailed,
    OutOfMemory,
    InvalidScript,
    RuntimeError,
    CompilationError,
    JITError,
    GCError,
    ParserError,
    OptimizationError,
    SecurityError,
    NetworkError,
    ModuleError,
    QuantumError
};

/**
 * @brief エンジン統計情報
 */
struct EngineStats {
    size_t scriptsEvaluated = 0;
    size_t totalMemoryAllocated = 0;
    size_t currentMemoryUsage = 0;
    size_t gcCollections = 0;
    size_t jitCompilations = 0;
    std::chrono::milliseconds totalExecutionTime{0};
    std::chrono::milliseconds gcTime{0};
    std::chrono::milliseconds jitTime{0};
    
    // コピー代入演算子を削除
    EngineStats& operator=(const EngineStats&) = delete;
    EngineStats(const EngineStats&) = delete;
    EngineStats() = default;
    EngineStats(EngineStats&&) = default;
    EngineStats& operator=(EngineStats&&) = default;
};

/**
 * @brief エンジン設定
 */
struct EngineConfig {
    size_t maxMemoryLimit = 1024 * 1024 * 1024; // 1GB
    uint32_t jitThreshold = 100;
    uint32_t optimizationLevel = 2;
    uint32_t gcFrequency = 1000;
    bool enableJIT = true;
    bool enableProfiling = false;
    bool enableDebugging = false;
    bool strictMode = false;
    std::string engineName = "AeroJS";
    std::string version = "1.0.0";
};

/**
 * @brief エラーハンドラー関数型
 */
using ErrorHandler = std::function<void(EngineError, const std::string&)>;

/**
 * @brief AeroJS JavaScript エンジン
 */
class Engine {
public:
    Engine();
    explicit Engine(const EngineConfig& config);
    ~Engine();

    // エンジンの初期化と終了
    bool initialize();
    bool initialize(const EngineConfig& config);
    void shutdown();
    bool isInitialized() const;

    // コードの評価
    Value evaluate(const std::string& source);
    Value evaluate(const std::string& source, const std::string& filename);
    Value evaluateFile(const std::string& filename);
    
    // 非同期評価
    std::future<Value> evaluateAsync(const std::string& source);
    std::future<Value> evaluateAsync(const std::string& source, const std::string& filename);

    // ガベージコレクション
    void collectGarbage();
    size_t getGCFrequency() const;
    void setGCFrequency(size_t frequency);

    // JIT設定
    void enableJIT(bool enable);
    bool isJITEnabled() const;
    void setJITThreshold(uint32_t threshold);
    uint32_t getJITThreshold() const;
    void setOptimizationLevel(uint32_t level);
    uint32_t getOptimizationLevel() const;

    // メモリ管理
    void setMemoryLimit(size_t limit);
    size_t getMemoryLimit() const;
    size_t getCurrentMemoryUsage() const;
    size_t getTotalMemoryUsage() const;
    size_t getPeakMemoryUsage() const;
    void optimizeMemory();

    // エラーハンドリング
    void setErrorHandler(ErrorHandler handler);
    EngineError getLastError() const;
    std::string getLastErrorMessage() const;
    void clearError();

    // 統計情報
    const EngineStats& getStats() const;
    void resetStats();
    std::string getStatsReport() const;

    // 設定
    void setConfig(const EngineConfig& config);
    const EngineConfig& getConfig() const;

    // プロファイリング
    void enableProfiling(bool enable);
    bool isProfilingEnabled() const;
    std::string getProfilingReport() const;

    // アクセサ
    utils::memory::MemoryAllocator* getMemoryAllocator() const;
    utils::memory::MemoryPool* getMemoryPool() const;
    utils::memory::GarbageCollector* getGarbageCollector() const;
    Context* getGlobalContext() const;

    // ユーティリティ
    void warmup();  // エンジンのウォームアップ
    void cooldown();  // エンジンのクールダウン
    bool validateScript(const std::string& source);
    std::vector<std::string> getAvailableOptimizations() const;

private:
    std::unique_ptr<utils::memory::MemoryAllocator> memoryAllocator_;
    std::unique_ptr<utils::memory::MemoryPool> memoryPool_;
    std::unique_ptr<utils::Timer> timer_;
    std::unique_ptr<utils::memory::GarbageCollector> garbageCollector_;
    std::unique_ptr<runtime::builtins::BuiltinsManager> builtinsManager_;
    void* interpreter_;
    std::unique_ptr<Context> globalContext_;

    // 設定と状態
    EngineConfig config_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> jitEnabled_{false};
    std::atomic<uint32_t> jitThreshold_{100};
    std::atomic<uint32_t> optimizationLevel_{2};
    std::atomic<size_t> gcFrequency_{1000};

    // エラー管理
    std::atomic<EngineError> lastError_{EngineError::None};
    std::string lastErrorMessage_;
    ErrorHandler errorHandler_;
    mutable std::mutex errorMutex_;

    // 統計情報
    mutable EngineStats stats_;
    std::atomic<bool> profilingEnabled_{false};

    // 内部メソッド
    void* createString(const std::string& str);
    bool initializeMemorySystem();
    bool initializeRuntimeSystem();
    void handleError(EngineError error, const std::string& message);
    void updateStats(const std::string& operation, std::chrono::microseconds duration);
    void updateStats();  // 統計情報の更新
    Value evaluateInternal(const std::string& source, const std::string& filename = "");
    void performGCIfNeeded();
    void optimizeJIT();
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_ENGINE_H