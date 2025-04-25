/**
 * @file mcp_tool_manager.cpp
 * @brief MCPツールマネージャークラスの実装
 * @version 1.0.0
 * @copyright 2024 AeroJS Project
 * @license MIT
 */

#include "mcp_tool_manager.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <future>
#include <iomanip>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "../../../utils/cache/lru_cache.h"
#include "../../../utils/json/json_builder.h"
#include "../../../utils/json/json_parser.h"
#include "../../../utils/logger/logger.h"
#include "../../../utils/memory/memory_pool.h"
#include "../../../utils/metrics/metrics_collector.h"
#include "../../../utils/profiler/profiler.h"
#include "../../../utils/security/security_validator.h"
#include "../../../utils/thread/thread_pool.h"

namespace aerojs::core::mcp::tool {

namespace {
// 定数定義
constexpr size_t DEFAULT_THREAD_POOL_SIZE = 16;
constexpr size_t DEFAULT_MEMORY_POOL_SIZE = 1024 * 1024;  // 1MB
constexpr std::chrono::seconds DEFAULT_CACHE_TTL = std::chrono::seconds(3600);
constexpr size_t DEFAULT_CACHE_SIZE = 1000;
constexpr uint32_t DEFAULT_RETRY_COUNT = 3;
constexpr std::chrono::milliseconds DEFAULT_RETRY_DELAY = std::chrono::milliseconds(100);
constexpr float DEFAULT_EVICTION_THRESHOLD = 0.9f;

// エラーメッセージ
const char* const ERROR_TOOL_NOT_FOUND = "指定されたツールが見つかりません: ";
const char* const ERROR_INVALID_PARAMS = "無効なパラメータです: ";
const char* const ERROR_EXECUTION_FAILED = "ツールの実行に失敗しました: ";
const char* const ERROR_VALIDATION_FAILED = "パラメータの検証に失敗しました: ";
const char* const ERROR_SECURITY_CHECK_FAILED = "セキュリティチェックに失敗しました: ";
const char* const ERROR_RESOURCE_LIMIT_EXCEEDED = "リソース制限を超過しました: ";
const char* const ERROR_TIMEOUT = "実行がタイムアウトしました: ";
const char* const ERROR_PLUGIN_LOAD_FAILED = "プラグインの読み込みに失敗しました: ";
}  // namespace

/**
 * @brief 実行コンテキストクラス
 */
class ToolManager::ExecutionContext {
 public:
  ExecutionContext(
      Context* ctx,
      std::string_view name,
      std::string_view params,
      const ExecutionOptions& options)
      : m_ctx(ctx), m_name(name), m_params(params), m_options(options), m_startTime(high_resolution_clock::now()) {
  }

  // ゲッター
  Context* getContext() const noexcept {
    return m_ctx;
  }
  std::string_view getName() const noexcept {
    return m_name;
  }
  std::string_view getParams() const noexcept {
    return m_params;
  }
  const ExecutionOptions& getOptions() const noexcept {
    return m_options;
  }

  // 実行時間の計測
  template <typename Duration = milliseconds>
  auto getElapsedTime() const {
    return duration_cast<Duration>(high_resolution_clock::now() - m_startTime);
  }

  // 実行結果の設定
  void setResult(ToolResult* result) noexcept {
    m_result = result;
    m_endTime = high_resolution_clock::now();
  }

  // 実行結果の取得
  ToolResult* getResult() const noexcept {
    return m_result;
  }

  // エラー情報の設定
  void setError(const std::exception& e) noexcept {
    m_error = std::current_exception();
  }

  // エラー情報の取得
  std::exception_ptr getError() const noexcept {
    return m_error;
  }

 private:
  Context* m_ctx;
  std::string m_name;
  std::string m_params;
  ExecutionOptions m_options;
  time_point<high_resolution_clock> m_startTime;
  time_point<high_resolution_clock> m_endTime;
  ToolResult* m_result{nullptr};
  std::exception_ptr m_error;
};

/**
 * @brief バッチ実行クラス
 */
class ToolManager::BatchExecutor {
 public:
  explicit BatchExecutor(ToolManager& manager)
      : m_manager(manager) {
  }

  void executeBatch(
      Context* ctx,
      const std::vector<std::pair<std::string, std::string>>& tasks,
      const ExecutionOptions& options) {
    std::vector<std::future<ToolResult*>> futures;
    futures.reserve(tasks.size());

    // タスクの並列実行
    for (const auto& [name, params] : tasks) {
      futures.push_back(
          m_manager.executeToolAsync(ctx, name, params, options));
    }

    // 結果の収集
    std::vector<ToolResult*> results;
    results.reserve(tasks.size());

    for (auto& future : futures) {
      try {
        results.push_back(future.get());
      } catch (const std::exception& e) {
        Logger::error("バッチ実行中にエラーが発生しました: %s", e.what());
        results.push_back(ToolResult::createError(-1, e.what()));
      }
    }

    // 結果の集計
    size_t successCount = std::count_if(results.begin(), results.end(),
                                        [](const ToolResult* r) { return r && r->isSuccess(); });

    Logger::info("バッチ実行完了: 成功=%zu, 失敗=%zu",
                 successCount, results.size() - successCount);
  }

 private:
  ToolManager& m_manager;
};

/**
 * @brief プラグイン管理クラス
 */
class ToolManager::PluginManager {
 public:
  explicit PluginManager(bool enabled = true)
      : m_enabled(enabled) {
  }

  bool loadPlugin(const std::string& path) {
    if (!m_enabled) return false;

    try {
      // プラグインのロード処理
      return true;
    } catch (const std::exception& e) {
      Logger::error("プラグインのロードに失敗しました: %s", e.what());
      return false;
    }
  }

  bool unloadPlugin(const std::string& name) {
    if (!m_enabled) return false;

    try {
      // プラグインのアンロード処理
      return true;
    } catch (const std::exception& e) {
      Logger::error("プラグインのアンロードに失敗しました: %s", e.what());
      return false;
    }
  }

  std::vector<std::string> getLoadedPlugins() const {
    if (!m_enabled) return {};

    std::vector<std::string> plugins;
    // プラグイン一覧の取得処理
    return plugins;
  }

 private:
  bool m_enabled;
  // プラグイン管理用のデータ構造
};

/**
 * @brief ヘルスモニタークラス
 */
class ToolManager::HealthMonitor {
 public:
  explicit HealthMonitor(ToolManager& manager)
      : m_manager(manager), m_running(true) {
    m_monitorThread = std::thread([this] { monitorLoop(); });
  }

  ~HealthMonitor() {
    m_running = false;
    if (m_monitorThread.joinable()) {
      m_monitorThread.join();
    }
  }

  std::string getStatus() const {
    JSONBuilder builder;
    builder.beginObject()
        .addProperty("status", "healthy")
        .addProperty("uptime", getUptime())
        .addProperty("memoryUsage", getMemoryUsage())
        .addProperty("cpuUsage", getCpuUsage())
        .endObject();
    return builder.toString();
  }

 private:
  void monitorLoop() {
    while (m_running) {
      // ヘルスチェック処理
      std::this_thread::sleep_for(seconds(1));
    }
  }

  std::string getUptime() const {
    auto uptime = duration_cast<seconds>(
        high_resolution_clock::now() - m_startTime);
    return std::format("{}s", uptime.count());
  }

  double getMemoryUsage() const {
    // メモリ使用量の取得処理
    return 0.0;
  }

  double getCpuUsage() const {
    // CPU使用率の取得処理
    return 0.0;
  }

  ToolManager& m_manager;
  std::atomic<bool> m_running;
  std::thread m_monitorThread;
  time_point<high_resolution_clock> m_startTime{high_resolution_clock::now()};
};

// ToolManagerの実装

ToolManager::ToolManager(const InitConfig& config)
    : m_config(config) {
  initializeComponents(config);
}

ToolManager::~ToolManager() {
  cleanupComponents();
}

void ToolManager::initializeComponents(const InitConfig& config) {
  // スレッドプールの初期化
  m_threadPool = std::make_unique<ThreadPool>(
      config.threadPoolSize ? config.threadPoolSize : DEFAULT_THREAD_POOL_SIZE);

  // メモリプールの初期化
  m_memoryPool = std::make_unique<MemoryPool>(
      config.memoryPoolSize ? config.memoryPoolSize : DEFAULT_MEMORY_POOL_SIZE);

  // キャッシュの初期化
  m_cache = std::make_unique<ToolCache>();

  // バリデータの初期化
  m_validator = std::make_unique<ToolValidator>();

  // メトリクスコレクタの初期化
  m_metricsCollector = std::make_unique<ToolMetricsCollector>(
      config.metricsConfig);

  // スケジューラの初期化
  m_scheduler = std::make_unique<ToolScheduler>();

  // プラグインマネージャの初期化
  m_pluginManager = std::make_unique<PluginManager>(
      config.enablePlugins);

  // ヘルスモニタの初期化
  m_healthMonitor = std::make_unique<HealthMonitor>(*this);

  Logger::info("ToolManagerの初期化が完了しました");
}

void ToolManager::cleanupComponents() noexcept {
  try {
    // コンポーネントの終了処理
    m_healthMonitor.reset();
    m_pluginManager.reset();
    m_scheduler.reset();
    m_metricsCollector.reset();
    m_validator.reset();
    m_cache.reset();
    m_memoryPool.reset();
    m_threadPool.reset();

    Logger::info("ToolManagerの終了処理が完了しました");
  } catch (const std::exception& e) {
    Logger::error("ToolManagerの終了処理中にエラーが発生しました: %s", e.what());
  }
}

bool ToolManager::registerTool(ToolPtr tool) {
  if (!tool) {
    throw std::invalid_argument("ツールがnullptrです");
  }

  std::unique_lock lock(m_mutex);
  const std::string& name = tool->getName();

  if (m_tools.find(name) != m_tools.end()) {
    Logger::warn("ツール '%s' は既に登録されています", name.c_str());
    return false;
  }

  m_tools[name] = std::move(tool);
  Logger::info("ツール '%s' を登録しました", name.c_str());
  return true;
}

bool ToolManager::unregisterTool(std::string_view name) {
  std::unique_lock lock(m_mutex);
  auto it = m_tools.find(std::string(name));

  if (it == m_tools.end()) {
    Logger::warn("ツール '%s' は登録されていません", name.data());
    return false;
  }

  m_tools.erase(it);
  Logger::info("ツール '%s' の登録を解除しました", name.data());
  return true;
}

std::optional<Tool*> ToolManager::getTool(std::string_view name) const {
  std::shared_lock lock(m_mutex);
  auto it = m_tools.find(std::string(name));
  return it != m_tools.end() ? std::make_optional(it->second.get()) : std::nullopt;
}

std::vector<Tool*> ToolManager::getTools() const {
  std::shared_lock lock(m_mutex);
  std::vector<Tool*> tools;
  tools.reserve(m_tools.size());

  for (const auto& [name, tool] : m_tools) {
    tools.push_back(tool.get());
  }

  return tools;
}

size_t ToolManager::getToolCount() const noexcept {
  std::shared_lock lock(m_mutex);
  return m_tools.size();
}

void ToolManager::clearTools() noexcept {
  std::unique_lock lock(m_mutex);
  m_tools.clear();
  Logger::info("すべてのツールをクリアしました");
}

ToolResult* ToolManager::executeTool(
    Context* ctx,
    std::string_view name,
    std::string_view params,
    const ExecutionOptions& options) {
  ExecutionContext execCtx(ctx, name, params, options);

  // キャッシュのチェック
  if (options.useCache) {
    if (auto cached = checkCache(name, params)) {
      m_metrics.cacheHits++;
      return *cached;
    }
    m_metrics.cacheMisses++;
  }

  // 実行の検証
  if (!validateExecution(execCtx)) {
    return ToolResult::createError(-1, "実行の検証に失敗しました");
  }

  // ツールの実行（リトライ機能付き）
  ToolResult* result = executeWithRetry(execCtx);

  // キャッシュの更新
  if (options.useCache && result && result->isSuccess()) {
    updateCache(name, params, result);
  }

  // メトリクスの更新
  updateMetrics(execCtx);

  // オブザーバーへの通知
  notifyObservers(execCtx);

  return result;
}

std::future<ToolResult*> ToolManager::executeToolAsync(
    Context* ctx,
    std::string_view name,
    std::string_view params,
    const ExecutionOptions& options) {
  return m_threadPool->enqueue([this, ctx, name = std::string(name),
                                params = std::string(params), options] {
    return executeTool(ctx, name, params, options);
  });
}

void ToolManager::executeToolBatch(
    Context* ctx,
    const std::vector<std::pair<std::string, std::string>>& tasks,
    const ExecutionOptions& options) {
  BatchExecutor executor(*this);
  executor.executeBatch(ctx, tasks, options);
}

bool ToolManager::validateToolParams(std::string_view name, std::string_view params) const {
  auto tool = getTool(name);
  if (!tool) {
    throw std::runtime_error(ERROR_TOOL_NOT_FOUND + std::string(name));
  }

  return m_validator->validateParams((*tool)->getParamsSchema(), std::string(params));
}

bool ToolManager::validateSecurity(std::string_view name, std::string_view params) const {
  return m_validator->validateSecurity(std::string(name), std::string(params));
}

bool ToolManager::validateResourceUsage(std::string_view name, std::string_view params) const {
  return m_validator->validateResourceUsage(std::string(name), std::string(params));
}

std::string ToolManager::getStatisticsAsJSON() const {
  JSONBuilder builder;

  builder.beginObject();
  builder.addProperty("totalExecutions", static_cast<int>(m_metrics.totalExecutions));
  builder.addProperty("successfulExecutions", static_cast<int>(m_metrics.successfulExecutions));
  builder.addProperty("failedExecutions", static_cast<int>(m_metrics.failedExecutions));
  builder.addProperty("totalExecutionTimeMs", static_cast<int>(m_metrics.totalExecutionTimeMs));
  builder.addProperty("cacheHits", static_cast<int>(m_metrics.cacheHits));
  builder.addProperty("cacheMisses", static_cast<int>(m_metrics.cacheMisses));

  return builder.endObject().toString();
}

std::string ToolManager::getMetricsAsJSON() const {
  return m_metricsCollector->getMetricsAsJSON();
}

std::string ToolManager::getHealthStatus() const {
  return m_healthMonitor->getStatus();
}

void ToolManager::resetStatistics() {
  m_metrics = ExecutionMetrics{};
  m_metricsCollector->reset();
}

void ToolManager::addObserver(std::shared_ptr<ToolObserver> observer) {
  std::unique_lock lock(m_mutex);
  m_observers.push_back(std::move(observer));
}

void ToolManager::removeObserver(std::shared_ptr<ToolObserver> observer) {
  std::unique_lock lock(m_mutex);
  m_observers.erase(
      std::remove(m_observers.begin(), m_observers.end(), observer),
      m_observers.end());
}

void ToolManager::updateConfig(const InitConfig& config) {
  std::unique_lock lock(m_mutex);
  m_config = config;

  // 各コンポーネントの設定を更新
  m_threadPool->resize(config.threadPoolSize);
  m_cache->updateConfig(config.cacheConfig);
  m_metricsCollector->updateConfig(config.metricsConfig);
}

void ToolManager::setExecutionCallback(ExecutionCallback callback) {
  std::unique_lock lock(m_mutex);
  m_executionCallback = std::move(callback);
}

void ToolManager::setErrorHandler(ErrorHandler handler) {
  std::unique_lock lock(m_mutex);
  m_errorHandler = std::move(handler);
}

void ToolManager::setValidationCallback(ValidationCallback callback) {
  std::unique_lock lock(m_mutex);
  m_validationCallback = std::move(callback);
}

void ToolManager::clearCache() {
  m_cache->clear();
}

void ToolManager::setCacheConfig(const CacheConfig& config) {
  m_cache->updateConfig(config);
}

ToolManager::CacheConfig ToolManager::getCacheConfig() const {
  return m_cache->getConfig();
}

bool ToolManager::loadPlugin(const std::string& path) {
  return m_pluginManager->loadPlugin(path);
}

bool ToolManager::unloadPlugin(const std::string& name) {
  return m_pluginManager->unloadPlugin(name);
}

std::vector<std::string> ToolManager::getLoadedPlugins() const {
  return m_pluginManager->getLoadedPlugins();
}

// 内部メソッド

void ToolManager::updateMetrics(const ExecutionContext& ctx) {
  auto result = ctx.getResult();
  auto executionTime = ctx.getElapsedTime();

  m_metrics.totalExecutions++;
  if (result && result->isSuccess()) {
    m_metrics.successfulExecutions++;
  } else {
    m_metrics.failedExecutions++;
  }
  m_metrics.totalExecutionTimeMs += executionTime.count();

  m_metricsCollector->recordExecution(ctx);
}

void ToolManager::notifyObservers(const ExecutionContext& ctx) {
  for (const auto& observer : m_observers) {
    try {
      observer->onExecutionComplete(ctx);
    } catch (const std::exception& e) {
      Logger::error("オブザーバーの通知中にエラーが発生しました: %s", e.what());
    }
  }
}

bool ToolManager::validateExecution(const ExecutionContext& ctx) const {
  const auto& name = ctx.getName();
  const auto& params = ctx.getParams();
  const auto& options = ctx.getOptions();

  // パラメータの検証
  if (!validateToolParams(name, params)) {
    return false;
  }

  // セキュリティチェック
  if (options.validateSecurity && !validateSecurity(name, params)) {
    return false;
  }

  // リソース使用量の検証
  if (!validateResourceUsage(name, params)) {
    return false;
  }

  return true;
}

ToolResult* ToolManager::executeWithRetry(const ExecutionContext& ctx) {
  const auto& options = ctx.getOptions();
  uint32_t retryCount = 0;

  while (retryCount <= options.retryCount) {
    try {
      auto tool = getTool(ctx.getName());
      if (!tool) {
        throw std::runtime_error(ERROR_TOOL_NOT_FOUND + std::string(ctx.getName()));
      }

      auto result = (*tool)->execute(ctx.getContext(), std::string(ctx.getParams()));

      if (result && result->isSuccess()) {
        return result;
      }

      retryCount++;
      if (retryCount <= options.retryCount) {
        std::this_thread::sleep_for(
            DEFAULT_RETRY_DELAY * static_cast<int>(std::pow(2, retryCount - 1)));
      }
    } catch (const std::exception& e) {
      handleExecutionError(ctx, e);
      retryCount++;
    }
  }

  return ToolResult::createError(-1, "最大リトライ回数を超過しました");
}

void ToolManager::handleExecutionError(const ExecutionContext& ctx, const std::exception& e) {
  Logger::error("ツール '%s' の実行中にエラーが発生しました: %s",
                std::string(ctx.getName()).c_str(), e.what());

  if (m_errorHandler) {
    m_errorHandler(e);
  }
}

std::optional<ToolResult*> ToolManager::checkCache(
    std::string_view name,
    std::string_view params) const {
  return m_cache->get(std::string(name), std::string(params));
}

void ToolManager::updateCache(
    std::string_view name,
    std::string_view params,
    const ToolResult* result) {
  m_cache->put(std::string(name), std::string(params), result);
}

}  // namespace aerojs::core::mcp::tool