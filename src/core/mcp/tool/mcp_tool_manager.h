/**
 * @file mcp_tool_manager.h
 * @brief MCPツールマネージャークラスの定義
 * @version 1.0.0
 * @copyright 2024 AeroJS Project
 * @license MIT
 */

#ifndef AEROJS_CORE_MCP_TOOL_MANAGER_H
#define AEROJS_CORE_MCP_TOOL_MANAGER_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <optional>
#include <future>
#include <queue>
#include <chrono>
#include <atomic>
#include <concepts>
#include <type_traits>
#include <functional>
#include <variant>
#include <span>

#include "tool.h"
#include "tool_result.h"
#include "../../context.h"
#include "../../../utils/json/json_value.h"
#include "../../../utils/memory/smart_ptr/ref_counted.h"
#include "../../../utils/thread/thread_pool.h"
#include "../../../utils/memory/memory_pool.h"
#include "../../../utils/profiler/profiler.h"
#include "../../../utils/metrics/metrics_collector.h"
#include "../../../utils/cache/lru_cache.h"
#include "../../../utils/security/security_validator.h"

namespace aerojs::core::mcp::tool {

// 前方宣言
class ToolExecutionContext;
class ToolValidator;
class ToolMetricsCollector;
class ToolCache;
class ToolScheduler;
class ToolObserver;

/**
 * @class ToolManager
 * @brief 高性能なMCPツール管理システム
 * 
 * 特徴：
 * - 非同期実行とバッチ処理のサポート
 * - 高度なキャッシュシステム
 * - リアルタイムメトリクス収集
 * - プラグイン機構による拡張性
 * - セキュリティバリデーション
 * - 自動スケーリング
 * - 障害検知と自動リカバリ
 * - パフォーマンス最適化
 */
class ToolManager : public utils::RefCounted {
public:
    // 型定義
    using ToolPtr = std::unique_ptr<Tool>;
    using ToolMap = std::unordered_map<std::string, ToolPtr>;
    using ExecutionCallback = std::function<void(const ToolResult*)>;
    using ValidationCallback = std::function<bool(const std::string&, const std::string&)>;
    using ErrorHandler = std::function<void(const std::exception&)>;
    
    /**
     * @brief 実行オプション構造体
     */
    struct ExecutionOptions {
        bool async{false};                  ///< 非同期実行フラグ
        bool useCache{true};                ///< キャッシュ使用フラグ
        std::chrono::milliseconds timeout{5000}; ///< 実行タイムアウト
        uint32_t retryCount{3};             ///< リトライ回数
        uint32_t priority{0};               ///< 実行優先度
        bool validateSecurity{true};        ///< セキュリティ検証フラグ
        std::optional<std::string> tag;     ///< 実行タグ
    };

    /**
     * @brief キャッシュ設定構造体
     */
    struct CacheConfig {
        size_t maxSize{1000};              ///< 最大キャッシュサイズ
        std::chrono::seconds ttl{3600};    ///< キャッシュTTL
        bool useCompression{true};         ///< 圧縮使用フラグ
        float evictionThreshold{0.9f};     ///< 削除閾値
    };

    /**
     * @brief メトリクス設定構造体
     */
    struct MetricsConfig {
        bool enableDetailedStats{true};    ///< 詳細統計有効フラグ
        bool enableHistograms{true};       ///< ヒストグラム有効フラグ
        bool enableTracing{true};          ///< トレース有効フラグ
        uint32_t samplingRate{100};        ///< サンプリングレート
    };

    /**
     * @brief 初期化設定構造体
     */
    struct InitConfig {
        uint32_t threadPoolSize{16};       ///< スレッドプールサイズ
        size_t memoryPoolSize{1024*1024};  ///< メモリプールサイズ
        CacheConfig cacheConfig;           ///< キャッシュ設定
        MetricsConfig metricsConfig;       ///< メトリクス設定
        bool enablePlugins{true};          ///< プラグイン有効フラグ
        std::string configPath;            ///< 設定ファイルパス
    };

    // コンストラクタ/デストラクタ
    explicit ToolManager(const InitConfig& config = InitConfig{});
    ~ToolManager() override;

    // ツール管理
    bool registerTool(ToolPtr tool);
    bool unregisterTool(std::string_view name);
    std::optional<Tool*> getTool(std::string_view name) const;
    std::vector<Tool*> getTools() const;
    size_t getToolCount() const noexcept;
    void clearTools() noexcept;

    // ツール実行
    ToolResult* executeTool(
        Context* ctx,
        std::string_view name,
        std::string_view params,
        const ExecutionOptions& options = {}
    );

    std::future<ToolResult*> executeToolAsync(
        Context* ctx,
        std::string_view name,
        std::string_view params,
        const ExecutionOptions& options = {}
    );

    void executeToolBatch(
        Context* ctx,
        const std::vector<std::pair<std::string, std::string>>& tasks,
        const ExecutionOptions& options = {}
    );

    // バリデーション
    bool validateToolParams(std::string_view name, std::string_view params) const;
    bool validateSecurity(std::string_view name, std::string_view params) const;
    bool validateResourceUsage(std::string_view name, std::string_view params) const;

    // 統計・メトリクス
    std::string getStatisticsAsJSON() const;
    std::string getMetricsAsJSON() const;
    std::string getHealthStatus() const;
    void resetStatistics();

    // オブザーバー
    void addObserver(std::shared_ptr<ToolObserver> observer);
    void removeObserver(std::shared_ptr<ToolObserver> observer);

    // 設定
    void updateConfig(const InitConfig& config);
    void setExecutionCallback(ExecutionCallback callback);
    void setErrorHandler(ErrorHandler handler);
    void setValidationCallback(ValidationCallback callback);

    // キャッシュ制御
    void clearCache();
    void setCacheConfig(const CacheConfig& config);
    CacheConfig getCacheConfig() const;

    // プラグイン管理
    bool loadPlugin(const std::string& path);
    bool unloadPlugin(const std::string& name);
    std::vector<std::string> getLoadedPlugins() const;
    
private:
    // 内部クラス
    class ExecutionContext;
    class BatchExecutor;
    class PluginManager;
    class HealthMonitor;

    // 内部メソッド
    void initializeComponents(const InitConfig& config);
    void cleanupComponents() noexcept;
    void updateMetrics(const ExecutionContext& ctx);
    void notifyObservers(const ExecutionContext& ctx);
    bool validateExecution(const ExecutionContext& ctx) const;
    ToolResult* executeWithRetry(const ExecutionContext& ctx);
    void handleExecutionError(const ExecutionContext& ctx, const std::exception& e);
    std::optional<ToolResult*> checkCache(std::string_view name, std::string_view params) const;
    void updateCache(std::string_view name, std::string_view params, const ToolResult* result);

    // 内部データ構造
    struct ExecutionMetrics {
        std::atomic<uint64_t> totalExecutions{0};
        std::atomic<uint64_t> successfulExecutions{0};
        std::atomic<uint64_t> failedExecutions{0};
        std::atomic<uint64_t> totalExecutionTimeMs{0};
        std::atomic<uint64_t> cacheHits{0};
        std::atomic<uint64_t> cacheMisses{0};
    };

    // メンバー変数
    InitConfig m_config;
    ToolMap m_tools;
    mutable std::shared_mutex m_mutex;
    std::unique_ptr<ThreadPool> m_threadPool;
    std::unique_ptr<MemoryPool> m_memoryPool;
    std::unique_ptr<ToolCache> m_cache;
    std::unique_ptr<ToolValidator> m_validator;
    std::unique_ptr<ToolMetricsCollector> m_metricsCollector;
    std::unique_ptr<ToolScheduler> m_scheduler;
    std::unique_ptr<PluginManager> m_pluginManager;
    std::unique_ptr<HealthMonitor> m_healthMonitor;
    std::vector<std::shared_ptr<ToolObserver>> m_observers;
    ExecutionCallback m_executionCallback;
    ErrorHandler m_errorHandler;
    ValidationCallback m_validationCallback;
    ExecutionMetrics m_metrics;

    // 禁止操作
    ToolManager(const ToolManager&) = delete;
    ToolManager& operator=(const ToolManager&) = delete;
    ToolManager(ToolManager&&) = delete;
    ToolManager& operator=(ToolManager&&) = delete;
};

// スマートポインタ型の定義
using ToolManagerPtr = utils::RefPtr<ToolManager>;

} // namespace aerojs::core::mcp::tool

#endif // AEROJS_CORE_MCP_TOOL_MANAGER_H 