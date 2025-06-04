/**
 * @file quantum_jit.h
 * @brief 量子JITコンパイラシステム
 */

#pragma once

#include <string>
#include <memory>
#include <future>
#include <unordered_map>

namespace aerojs {
namespace jit {

/**
 * @brief 量子最適化レベル
 */
enum class QuantumOptimizationLevel {
    None,
    Basic,
    Advanced,
    Extreme,
    Quantum,
    Transcendent
};

/**
 * @brief JIT統計
 */
struct JITStats {
    size_t compiledFunctions = 0;
    size_t optimizedFunctions = 0;
    size_t quantumOptimizedFunctions = 0;
    double compilationTime = 0.0;
    double optimizationTime = 0.0;
    size_t cacheHits = 0;
    size_t cacheMisses = 0;
};

/**
 * @brief プロファイル情報
 */
struct ProfileInfo {
    size_t executionCount = 0;
    double totalTime = 0.0;
    double averageTime = 0.0;
    bool isHotFunction = false;
};

/**
 * @brief JIT設定
 */
struct QuantumJITConfig {
    QuantumOptimizationLevel optimizationLevel = QuantumOptimizationLevel::Quantum;
    bool enableQuantumOptimization = true;
    bool enableParallelCompilation = true;
    bool enableAdaptiveOptimization = true;
    size_t maxCacheSize = 1024 * 1024 * 1024; // 1GB
    size_t hotThreshold = 100;
};

/**
 * @brief 量子JITコンパイラ
 */
class QuantumJIT {
public:
    explicit QuantumJIT(const QuantumJITConfig& config = QuantumJITConfig{});
    ~QuantumJIT();

    // 初期化・終了
    bool initialize();
    void shutdown();

    // コンパイル
    void* compileSync(const std::string& name, const std::string& source);
    std::future<void*> compileAsync(const std::string& name, const std::string& source);

    // 最適化
    bool optimizeFunction(const std::string& name, QuantumOptimizationLevel level);
    void performAdaptiveOptimization();
    void analyzeHotspots();
    void optimizeHotFunctions();

    // プロファイリング
    void recordExecution(const std::string& name, double executionTime);
    ProfileInfo* getProfile(const std::string& name);

    // キャッシュ
    size_t getCodeCacheSize() const;
    double getCodeCacheHitRate() const;
    void clearCache();

    // 統計・レポート
    const JITStats& getStats() const;
    std::string getCompilationReport() const;
    std::string getOptimizationReport() const;
    std::string getPerformanceReport() const;

private:
    QuantumJITConfig config_;
    JITStats stats_;
    std::unordered_map<std::string, ProfileInfo> profiles_;
    std::unordered_map<std::string, void*> codeCache_;
    bool initialized_ = false;
};

} // namespace jit
} // namespace aerojs 