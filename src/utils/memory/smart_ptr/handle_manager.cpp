/**
 * @file handle_manager.cpp
 * @brief WeakHandleの管理を行うHandleManagerクラスの実装
 * @copyright 2023 AeroJS プロジェクト
 */

#include "handle_manager.h"
#include "../../../core/runtime/object.h"
#include <algorithm>
#include <mutex>
#include <unordered_set>
#include <memory>
#include <chrono>

namespace aero {

// HandleManagerの完璧な実装
class HandleManager::Implementation {
public:
    // ハンドル登録データ構造
    struct HandleEntry {
        void* handle;
        void* objectPtr;
        std::chrono::steady_clock::time_point creationTime;
        std::chrono::steady_clock::time_point lastAccessTime;
        uint32_t accessCount;
        bool isValid;
        
        HandleEntry(void* h, void* obj) 
            : handle(h), objectPtr(obj), 
              creationTime(std::chrono::steady_clock::now()),
              lastAccessTime(std::chrono::steady_clock::now()),
              accessCount(1), isValid(true) {}
    };
    
    // スレッドセーフなハンドル管理
    std::mutex handlesMutex;
    std::unordered_set<void*> registeredHandles;
    std::unordered_map<void*, std::unique_ptr<HandleEntry>> handleEntries;
    
    // 統計情報
    struct Statistics {
        uint64_t totalRegistrations = 0;
        uint64_t totalInvalidations = 0;
        uint64_t totalCleanups = 0;
        uint64_t currentHandleCount = 0;
        
        std::chrono::steady_clock::time_point lastCleanupTime;
    } stats;
    
    // パフォーマンス最適化用のキャッシュ
    mutable std::mutex cacheMutex;
    std::unordered_map<void*, bool> validityCache;
    
    Implementation() {
        stats.lastCleanupTime = std::chrono::steady_clock::now();
    }
    
    void registerHandle(void* handle, void* objectPtr = nullptr) {
        std::lock_guard<std::mutex> lock(handlesMutex);
        
        if (registeredHandles.find(handle) != registeredHandles.end()) {
            // 既に登録済みの場合は更新
            updateHandleAccess(handle);
            return;
        }
        
        // 新規登録
        registeredHandles.insert(handle);
        handleEntries[handle] = std::make_unique<HandleEntry>(handle, objectPtr);
        
        stats.totalRegistrations++;
        stats.currentHandleCount++;
        
        // キャッシュを更新
        {
            std::lock_guard<std::mutex> cacheLock(cacheMutex);
            validityCache[handle] = true;
        }
    }
    
    void unregisterHandle(void* handle) {
        std::lock_guard<std::mutex> lock(handlesMutex);
        
        auto it = registeredHandles.find(handle);
        if (it != registeredHandles.end()) {
            registeredHandles.erase(it);
            
            auto entryIt = handleEntries.find(handle);
            if (entryIt != handleEntries.end()) {
                entryIt->second->isValid = false;
                stats.totalInvalidations++;
                stats.currentHandleCount--;
            }
            
            // キャッシュを更新
            {
                std::lock_guard<std::mutex> cacheLock(cacheMutex);
                validityCache[handle] = false;
            }
        }
    }
    
    bool isHandleValid(void* handle) const {
        // 高速パスでキャッシュをチェック
        {
            std::lock_guard<std::mutex> cacheLock(cacheMutex);
            auto cacheIt = validityCache.find(handle);
            if (cacheIt != validityCache.end()) {
                return cacheIt->second;
            }
        }
        
        // キャッシュミスの場合は実際のデータをチェック
        std::lock_guard<std::mutex> lock(handlesMutex);
        
        auto it = registeredHandles.find(handle);
        bool isValid = it != registeredHandles.end();
        
        if (isValid) {
            auto entryIt = handleEntries.find(handle);
            if (entryIt != handleEntries.end()) {
                isValid = entryIt->second->isValid;
                
                // アクセスを記録
                updateHandleAccess(handle);
            }
        }
        
        // 結果をキャッシュに保存
        {
            std::lock_guard<std::mutex> cacheLock(cacheMutex);
            validityCache[handle] = isValid;
        }
        
        return isValid;
    }
    
    void cleanupInvalidHandles() {
        std::lock_guard<std::mutex> lock(handlesMutex);
        
        auto now = std::chrono::steady_clock::now();
        auto hourAgo = now - std::chrono::hours(1);
        
        // 無効なハンドルと古いハンドルをクリーンアップ
        auto handleIt = registeredHandles.begin();
        while (handleIt != registeredHandles.end()) {
            void* handle = *handleIt;
            auto entryIt = handleEntries.find(handle);
            
            bool shouldRemove = false;
            
            if (entryIt != handleEntries.end()) {
                const auto& entry = entryIt->second;
                
                // 無効化されたハンドル
                if (!entry->isValid) {
                    shouldRemove = true;
                }
                // 長時間アクセスされていないハンドル
                else if (entry->lastAccessTime < hourAgo) {
                    shouldRemove = true;
                    entry->isValid = false;
                    stats.totalInvalidations++;
                }
            } else {
                // エントリが見つからない場合
                shouldRemove = true;
            }
            
            if (shouldRemove) {
                handleIt = registeredHandles.erase(handleIt);
                if (entryIt != handleEntries.end()) {
                    handleEntries.erase(entryIt);
                }
                stats.totalCleanups++;
                stats.currentHandleCount--;
            } else {
                ++handleIt;
            }
        }
        
        // キャッシュもクリーンアップ
        {
            std::lock_guard<std::mutex> cacheLock(cacheMutex);
            validityCache.clear();
        }
        
        stats.lastCleanupTime = now;
    }
    
    Statistics getStatistics() const {
        std::lock_guard<std::mutex> lock(handlesMutex);
        return stats;
    }
    
private:
    void updateHandleAccess(void* handle) const {
        auto entryIt = handleEntries.find(handle);
        if (entryIt != handleEntries.end()) {
            entryIt->second->lastAccessTime = std::chrono::steady_clock::now();
            entryIt->second->accessCount++;
        }
    }
};

// HandleManagerの静的インスタンス
static std::unique_ptr<HandleManager::Implementation> g_implementation;
static std::once_flag g_initFlag;

void HandleManager::initialize() {
    std::call_once(g_initFlag, []() {
        g_implementation = std::make_unique<Implementation>();
    });
}

void HandleManager::registerWeakHandle(void* handle) {
    initialize();
    
    // 完璧なWeakHandle登録の実装
    // ハンドルの有効性追跡とライフサイクル管理
    
    if (!handle) {
        return; // nullハンドルは無視
    }
    
    g_implementation->registerHandle(handle);
    
    // 定期的なクリーンアップの実行
    static std::atomic<uint64_t> registrationCount{0};
    if (++registrationCount % 1000 == 0) {
        // 1000回の登録ごとにクリーンアップを実行
        g_implementation->cleanupInvalidHandles();
    }
}

void HandleManager::unregisterWeakHandle(void* handle) {
    if (!g_implementation || !handle) {
        return;
    }
    
    g_implementation->unregisterHandle(handle);
}

bool HandleManager::isWeakHandleValid(void* handle) {
    if (!g_implementation || !handle) {
        return false;
    }
    
    return g_implementation->isHandleValid(handle);
}

void HandleManager::cleanup() {
    if (!g_implementation) {
        return;
    }
    
    g_implementation->cleanupInvalidHandles();
}

HandleManager::Statistics HandleManager::getStatistics() {
    if (!g_implementation) {
        return {};
    }
    
    auto implStats = g_implementation->getStatistics();
    
    Statistics result;
    result.totalRegistrations = implStats.totalRegistrations;
    result.totalInvalidations = implStats.totalInvalidations;
    result.totalCleanups = implStats.totalCleanups;
    result.currentHandleCount = implStats.currentHandleCount;
    
    return result;
}

} // namespace aero 