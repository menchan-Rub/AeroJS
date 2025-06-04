/**
 * @file context.h
 * @brief AeroJS 世界最高レベルJavaScript実行コンテキスト
 * @version 3.0.0
 * @license MIT
 */

#pragma once

#include "value.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>

namespace aerojs {
namespace core {

// 前方宣言
class Engine;

/**
 * @brief 世界最高レベルグローバルオブジェクト
 */
struct GlobalObject {
    std::unordered_map<std::string, Value> properties;
    std::unordered_map<std::string, std::function<Value(const std::vector<Value>&)>> functions;
    mutable std::mutex mutex;
    
    GlobalObject() = default;
    ~GlobalObject() = default;
    
    // コピー・ムーブ操作
    GlobalObject(const GlobalObject& other) {
        std::lock_guard<std::mutex> lock(other.mutex);
        properties = other.properties;
        functions = other.functions;
    }
    
    GlobalObject& operator=(const GlobalObject& other) {
        if (this != &other) {
            std::lock(mutex, other.mutex);
            std::lock_guard<std::mutex> lock1(mutex, std::adopt_lock);
            std::lock_guard<std::mutex> lock2(other.mutex, std::adopt_lock);
            properties = other.properties;
            functions = other.functions;
        }
        return *this;
    }
    
    GlobalObject(GlobalObject&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.mutex);
        properties = std::move(other.properties);
        functions = std::move(other.functions);
    }
    
    GlobalObject& operator=(GlobalObject&& other) noexcept {
        if (this != &other) {
            std::lock(mutex, other.mutex);
            std::lock_guard<std::mutex> lock1(mutex, std::adopt_lock);
            std::lock_guard<std::mutex> lock2(other.mutex, std::adopt_lock);
            properties = std::move(other.properties);
            functions = std::move(other.functions);
        }
        return *this;
    }
};

/**
 * @brief 世界最高レベルJavaScript実行コンテキスト
 * 
 * 高度な変数管理、スコープチェーン、プロトタイプチェーン、
 * 非同期実行、セキュリティ機能を提供します。
 */
class Context {
public:
    explicit Context(Engine* engine);
    ~Context();

    // グローバルオブジェクト管理
    Value getGlobalObject() const;
    void setGlobalProperty(const std::string& name, const Value& value);
    Value getGlobalProperty(const std::string& name) const;
    bool hasGlobalProperty(const std::string& name) const;
    void removeGlobalProperty(const std::string& name);

    // 変数の管理
    void setVariable(const std::string& name, const Value& value);
    Value getVariable(const std::string& name) const;
    bool hasVariable(const std::string& name) const;
    void removeVariable(const std::string& name);

    // スコープ管理
    void pushScope();
    void popScope();
    size_t getScopeDepth() const;
    void clearCurrentScope();

    // 関数管理
    void setFunction(const std::string& name, std::function<Value(const std::vector<Value>&)> func);
    Value callFunction(const std::string& name, const std::vector<Value>& args);
    bool hasFunction(const std::string& name) const;
    void removeFunction(const std::string& name);

    // プロトタイプチェーン
    void setPrototype(const Value& object, const Value& prototype);
    Value getPrototype(const Value& object) const;
    bool hasPrototype(const Value& object) const;

    // プロパティ管理
    void setProperty(const Value& object, const std::string& name, const Value& value);
    Value getProperty(const Value& object, const std::string& name) const;
    bool hasProperty(const Value& object, const std::string& name) const;
    void removeProperty(const Value& object, const std::string& name);
    std::vector<std::string> getPropertyNames(const Value& object) const;

    // コードの評価
    Value evaluate(const std::string& source);
    Value evaluateAsync(const std::string& source);
    Value evaluateModule(const std::string& source, const std::string& moduleName);

    // エンジンへのアクセス
    Engine* getEngine() const;

    // ガベージコレクション
    void collectGarbage();
    void markReachableObjects();
    size_t getHeapSize() const;
    size_t getUsedHeapSize() const;

    // 統計情報
    size_t getVariableCount() const;
    size_t getGlobalPropertyCount() const;
    size_t getFunctionCount() const;
    std::vector<std::string> getVariableNames() const;
    std::vector<std::string> getGlobalPropertyNames() const;
    std::vector<std::string> getFunctionNames() const;

    // セキュリティ
    void enableSandbox(bool enable = true);
    bool isSandboxEnabled() const;
    void setExecutionLimit(size_t maxInstructions);
    size_t getExecutionLimit() const;
    size_t getExecutedInstructions() const;

    // デバッグ
    void enableDebugMode(bool enable = true);
    bool isDebugModeEnabled() const;
    void setBreakpoint(const std::string& source, size_t line);
    void removeBreakpoint(const std::string& source, size_t line);
    std::vector<std::pair<std::string, size_t>> getBreakpoints() const;

    // パフォーマンス
    void enableProfiling(bool enable = true);
    bool isProfilingEnabled() const;
    std::string getProfilingReport() const;
    void resetProfilingData();

    // 非同期実行
    void enableAsyncExecution(bool enable = true);
    bool isAsyncExecutionEnabled() const;
    void setEventLoop(std::function<void()> eventLoop);
    void runEventLoop();

    // モジュールシステム
    void registerModule(const std::string& name, const Value& module);
    Value getModule(const std::string& name) const;
    bool hasModule(const std::string& name) const;
    void removeModule(const std::string& name);
    std::vector<std::string> getModuleNames() const;

    // エラーハンドリング
    void setErrorHandler(std::function<void(const std::string&)> handler);
    void handleError(const std::string& error);
    std::vector<std::string> getErrorHistory() const;
    void clearErrorHistory();

    // リセット
    void clearVariables();
    void clearGlobalProperties();
    void clearFunctions();
    void clearModules();
    void reset();

    // 詳細情報
    std::string getDetailedReport() const;
    std::unordered_map<std::string, size_t> getStatistics() const;

private:
    Engine* engine_;
    Value globalObject_;
    std::unique_ptr<GlobalObject> globalObjectImpl_;
    
    // 変数とスコープ
    std::vector<std::unique_ptr<std::unordered_map<std::string, Value>>> scopes_;
    std::unique_ptr<std::unordered_map<std::string, Value>> variables_;
    
    // 関数
    std::unique_ptr<std::unordered_map<std::string, std::function<Value(const std::vector<Value>&)>>> functions_;
    
    // プロトタイプチェーン
    std::unique_ptr<std::unordered_map<size_t, Value>> prototypes_;
    
    // モジュール
    std::unique_ptr<std::unordered_map<std::string, Value>> modules_;
    
    // セキュリティ
    bool sandboxEnabled_;
    size_t executionLimit_;
    size_t executedInstructions_;
    
    // デバッグ
    bool debugModeEnabled_;
    std::unique_ptr<std::vector<std::pair<std::string, size_t>>> breakpoints_;
    
    // パフォーマンス
    bool profilingEnabled_;
    std::unique_ptr<std::unordered_map<std::string, size_t>> profilingData_;
    
    // 非同期実行
    bool asyncExecutionEnabled_;
    std::function<void()> eventLoop_;
    
    // エラーハンドリング
    std::function<void(const std::string&)> errorHandler_;
    std::unique_ptr<std::vector<std::string>> errorHistory_;
    
    // 統計
    mutable size_t accessCount_;
    mutable size_t evaluationCount_;
    mutable size_t gcCount_;
    
    // スレッドセーフティ
    mutable std::mutex mutex_;

    // 初期化メソッド
    void initializeGlobalObject();
    void initializeBuiltinFunctions();
    void initializeBuiltinObjects();
    void initializeSecuritySystem();
    void initializeDebugSystem();
    void initializeProfilingSystem();
    void initializeAsyncSystem();
    void initializeModuleSystem();
    void initializeErrorHandling();
    
    // ヘルパーメソッド
    Value* findVariable(const std::string& name);
    const Value* findVariable(const std::string& name) const;
    void updateStatistics() const;
    void checkExecutionLimit();
    void checkSandboxViolation(const std::string& operation);
    void logDebugInfo(const std::string& info);
    void updateProfilingData(const std::string& operation, size_t duration);
};

} // namespace core
} // namespace aerojs