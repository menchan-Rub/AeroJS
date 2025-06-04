/**
 * @file world_class_engine.h
 * @brief AeroJS 世界最高レベルエンジン
 * @version 3.0.0
 * @license MIT
 */

#ifndef AEROJS_WORLD_CLASS_ENGINE_H
#define AEROJS_WORLD_CLASS_ENGINE_H

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <cstdint>
#include <functional>

// 前方宣言
namespace aerojs {
    namespace jit {
        class QuantumJIT;
        struct QuantumJITConfig;
    }
    namespace gc {
        class HyperGC;
        struct HyperGCConfig;
    }
    namespace parser {
        class UltraParser;
        struct UltraParserConfig;
    }
}

namespace aerojs {
namespace engine {

/**
 * @brief 実行結果
 */
struct ExecutionResult {
    bool success = false;
    std::string value;
    std::string error;
    double executionTime = 0.0;
    uint64_t memoryUsed = 0;
    std::string filename;
};

/**
 * @brief セキュリティ違反情報
 */
struct SecurityViolation {
    std::string type;
    std::string description;
    std::string location;
    uint64_t timestamp;
};

/**
 * @brief エンジン統計情報
 */
struct EngineStats {
    uint64_t totalExecutions = 0;
    uint64_t totalCompilations = 0;
    uint64_t totalGCCollections = 0;
    double averageExecutionTime = 0.0;
    uint64_t memoryUsage = 0;
    uint64_t cacheHits = 0;
    uint64_t cacheMisses = 0;
};

/**
 * @brief 世界最高レベルエンジン設定
 */
struct WorldClassEngineConfig {
    bool enableQuantumJIT = true;
    bool enableHyperGC = true;
    bool enableUltraParser = true;
    uint64_t maxMemory = 8ULL * 1024 * 1024 * 1024; // 8GB
    uint32_t maxThreads = 16;
    bool enableSandbox = true;
    bool enableProfiling = true;
    bool enableTracing = true;
    bool enableDebugMode = false;
};

/**
 * @brief 世界最高レベルJavaScriptエンジン
 */
class WorldClassEngine {
public:
    explicit WorldClassEngine(const WorldClassEngineConfig& config);
    ~WorldClassEngine();

    // 初期化・終了
    bool initialize();
    void shutdown();
    bool isInitialized() const;

    // 実行メソッド
    ExecutionResult execute(const std::string& code);
    ExecutionResult execute(const std::string& code, const std::string& filename);
    std::future<ExecutionResult> executeAsync(const std::string& code);
    ExecutionResult executeModule(const std::string& modulePath);
    std::vector<ExecutionResult> executeParallel(const std::vector<std::string>& codes);
    
    // ストリーミング実行
    void startStreamingExecution();
    void feedCode(const std::string& code);
    ExecutionResult finishStreamingExecution();
    
    // 最適化制御
    void enableQuantumOptimization(bool enable);
    void enableAdaptiveOptimization(bool enable);
    void enableSpeculativeOptimization(bool enable);
    void optimizeHotFunctions();
    
    // メモリ管理
    uint64_t getMemoryUsage() const;
    void collectGarbage();
    void optimizeMemory();
    double getMemoryEfficiency() const;
    
    // セキュリティ
    bool validateCode(const std::string& code) const;
    void enableSandbox(bool enable);
    void setExecutionLimits(uint64_t timeLimit, uint64_t memoryLimit);
    std::vector<SecurityViolation> getSecurityViolations() const;
    
    // デバッグ・プロファイリング
    void enableDebugMode(bool enable);
    void enableProfiling(bool enable);
    void enableTracing(bool enable);
    
    // レポート生成
    const EngineStats& getStats() const;
    std::string getPerformanceReport() const;
    std::string getDetailedReport() const;
    std::string getDebugInfo() const;
    std::string getProfilingReport() const;
    std::string getTraceReport() const;

private:
    WorldClassEngineConfig config_;
    
    // 統計情報
    mutable EngineStats stats_;
    
    // 内部状態
    bool initialized_;
    bool sandboxEnabled_;
    bool debugMode_;
    bool profilingEnabled_;
    bool tracingEnabled_;
    
    // 実行制限
    uint64_t timeLimit_;
    uint64_t memoryLimit_;
    
    // セキュリティ
    std::vector<SecurityViolation> securityViolations_;
    
    // 内部メソッド
    void initializeQuantumJIT();
    void initializeHyperGC();
    void initializeUltraParser();
    void updateStats();
};

/**
 * @brief 世界最高レベルエンジンファクトリ
 */
class WorldClassEngineFactory {
public:
    static WorldClassEngineConfig createQuantumConfig();
    static WorldClassEngineConfig createHighPerformanceConfig();
    static WorldClassEngineConfig createLowLatencyConfig();
    static WorldClassEngineConfig createMemoryOptimizedConfig();
    static WorldClassEngineConfig createSecureConfig();
};

} // namespace engine
} // namespace aerojs

#endif // AEROJS_WORLD_CLASS_ENGINE_H 