/**
 * @file code_cache.cpp
 * @brief 高速なネイティブコードキャッシュ管理システムの実装
 * @version 1.0.0
 * @license MIT
 */

#include "code_cache.h"
#include "../context.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <sys/mman.h>

namespace aerojs {

//-----------------------------------------------------------------------------
// NativeCode クラスの実装
//-----------------------------------------------------------------------------

NativeCode::NativeCode(void* code, size_t codeSize, void* entryPoint)
    : _code(code)
    , _codeSize(codeSize)
    , _entryPoint(entryPoint)
    , _symbolName(nullptr) {
}

NativeCode::~NativeCode() {
    // アロケートされたコードメモリを解放
    if (_code) {
        // 実行可能メモリを解放するため、munmapを使用
        munmap(_code, _codeSize);
        _code = nullptr;
    }
}

void NativeCode::setProtection(MemoryProtection protection) {
    if (!_code) return;
    
    int prot = 0;
    switch (protection) {
        case MemoryProtection::ReadWrite:
            prot = PROT_READ | PROT_WRITE;
            break;
        case MemoryProtection::ReadExecute:
            prot = PROT_READ | PROT_EXEC;
            break;
        case MemoryProtection::ReadWriteExecute:
            prot = PROT_READ | PROT_WRITE | PROT_EXEC;
            break;
    }
    
    // メモリ保護を設定
    mprotect(_code, _codeSize, prot);
}

void NativeCode::addPatchPoint(size_t offset, uint32_t patchId) {
    if (offset >= _codeSize) return;
    
    PatchPoint point;
    point.offset = offset;
    point.patchId = patchId;
    _patchPoints.push_back(point);
}

void NativeCode::patchCode(uint32_t patchId, const void* newCode, size_t newCodeSize) {
    // パッチポイントを検索
    for (const auto& point : _patchPoints) {
        if (point.patchId == patchId) {
            // パッチのサイズチェック
            if (point.offset + newCodeSize > _codeSize) {
                return; // 範囲外へのパッチは適用しない
            }
            
            // 現在のメモリ保護設定を記憶し、書き込み可能に設定
            setProtection(MemoryProtection::ReadWrite);
            
            // パッチを適用
            uint8_t* target = static_cast<uint8_t*>(_code) + point.offset;
            std::memcpy(target, newCode, newCodeSize);
            
            // 実行可能に戻す
            setProtection(MemoryProtection::ReadExecute);
            
            return;
        }
    }
}

void NativeCode::setSymbolName(const char* name) {
    _symbolName = name;
}

//-----------------------------------------------------------------------------
// CodeCache クラスの実装
//-----------------------------------------------------------------------------

CodeCache::CodeCache(Context* context)
    : _context(context)
    , _evictionPolicy(EvictionPolicy::LRU)
    , _maxCacheSize(32 * 1024 * 1024) // デフォルト32MB
    , _currentCacheSize(0) {
}

CodeCache::~CodeCache() {
    clear();
}

void CodeCache::store(uint64_t functionId, NativeCode* code) {
    if (!code) return;
    
    std::lock_guard<std::mutex> lock(_cacheMutex);
    
    // 既存のエントリを削除
    auto it = _codeMap.find(functionId);
    if (it != _codeMap.end()) {
        _currentCacheSize -= it->second.code->codeSize();
        delete it->second.code;
    }
    
    // 容量オーバーの場合は古いエントリを削除
    _currentCacheSize += code->codeSize();
    evictIfNeeded();
    
    // 新しいエントリを追加
    uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    CacheEntry entry(code);
    entry.lastAccessTime = currentTime;
    _codeMap[functionId] = entry;
}

NativeCode* CodeCache::retrieve(uint64_t functionId) const {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    
    auto it = _codeMap.find(functionId);
    if (it != _codeMap.end()) {
        // アクセス時間を更新
        uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        it->second.lastAccessTime = currentTime;
        
        return it->second.code;
    }
    
    return nullptr;
}

bool CodeCache::contains(uint64_t functionId) const {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    return _codeMap.find(functionId) != _codeMap.end();
}

void CodeCache::remove(uint64_t functionId) {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    
    auto it = _codeMap.find(functionId);
    if (it != _codeMap.end()) {
        _currentCacheSize -= it->second.code->codeSize();
        delete it->second.code;
        _codeMap.erase(it);
    }
}

NativeCode* CodeCache::allocateCode(size_t codeSize) {
    // コード用のメモリを確保（実行可能なメモリ領域）
    void* mem = mmap(nullptr, codeSize, 
                    PROT_READ | PROT_WRITE | PROT_EXEC, 
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                    
    if (mem == MAP_FAILED) {
        return nullptr;
    }
    
    // NativeCodeオブジェクトを作成
    return new NativeCode(mem, codeSize, mem);
}

void CodeCache::clear() {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    
    // すべてのコードエントリを削除
    for (auto& entry : _codeMap) {
        delete entry.second.code;
    }
    
    _codeMap.clear();
    _currentCacheSize = 0;
}

void CodeCache::flush() {
    // 実装例: すべてのキャッシュされたコードのCPUキャッシュをフラッシュ
    // この実装はアーキテクチャ固有のコードになります
}

size_t CodeCache::size() const {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    return _codeMap.size();
}

size_t CodeCache::totalCodeSize() const {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    return _currentCacheSize;
}

void CodeCache::setEvictionPolicy(EvictionPolicy policy) {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    _evictionPolicy = policy;
}

void CodeCache::setMaxCacheSize(size_t maxSizeBytes) {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    _maxCacheSize = maxSizeBytes;
    
    // サイズが縮小された場合、必要に応じて削除
    evictIfNeeded();
}

void CodeCache::evictIfNeeded() {
    // キャッシュサイズが上限を超えたら古いエントリを削除
    while (_currentCacheSize > _maxCacheSize && !_codeMap.empty()) {
        uint64_t idToRemove = 0;
        
        switch (_evictionPolicy) {
            case EvictionPolicy::LRU: {
                // 最も長く使われていないエントリを探す
                uint64_t oldestTime = UINT64_MAX;
                for (const auto& entry : _codeMap) {
                    if (entry.second.lastAccessTime < oldestTime) {
                        oldestTime = entry.second.lastAccessTime;
                        idToRemove = entry.first;
                    }
                }
                break;
            }
            
            case EvictionPolicy::Size: {
                // 最も大きいエントリを探す
                size_t largestSize = 0;
                for (const auto& entry : _codeMap) {
                    if (entry.second.code->codeSize() > largestSize) {
                        largestSize = entry.second.code->codeSize();
                        idToRemove = entry.first;
                    }
                }
                break;
            }
            
            case EvictionPolicy::Hybrid: {
                // 大きさとアクセス時間の両方を考慮したスコアでエントリを選択
                double worstScore = -1.0;
                uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                
                for (const auto& entry : _codeMap) {
                    uint64_t age = currentTime - entry.second.lastAccessTime;
                    size_t size = entry.second.code->codeSize();
                    
                    // スコア = サイズ * 経過時間（大きなサイズで長く使われていないものが優先的に削除）
                    double score = static_cast<double>(size) * static_cast<double>(age);
                    
                    if (score > worstScore) {
                        worstScore = score;
                        idToRemove = entry.first;
                    }
                }
                break;
            }
        }
        
        // 特定されたエントリを削除
        if (idToRemove != 0) {
            auto it = _codeMap.find(idToRemove);
            if (it != _codeMap.end()) {
                _currentCacheSize -= it->second.code->codeSize();
                delete it->second.code;
                _codeMap.erase(it);
            }
        } else {
            break; // 削除するエントリが見つからない場合、終了
        }
    }
}

} // namespace aerojs 