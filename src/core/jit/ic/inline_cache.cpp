/**
 * @file inline_cache.cpp
 * @brief 高性能インラインキャッシュシステムの実装
 * @version 1.0.0
 * @license MIT
 */

#include "inline_cache.h"
#include "../../context.h"
#include "../../runtime/values/object.h"
#include "../../runtime/values/value.h"
#include "../code_cache.h"
#include <algorithm>
#include <cassert>

namespace aerojs {

//-----------------------------------------------------------------------------
// PropertyCache の実装
//-----------------------------------------------------------------------------

void PropertyCache::addEntry(uint64_t shapeId, uint32_t slotOffset, bool isInlineProperty) {
    // 既存エントリの検索
    for (auto& entry : _entries) {
        if (entry.shapeId == shapeId) {
            // 既存エントリの更新
            entry.slotOffset = slotOffset;
            entry.isInlineProperty = isInlineProperty;
            return;
        }
    }
    
    // 新しいエントリの追加
    Entry entry;
    entry.shapeId = shapeId;
    entry.slotOffset = slotOffset;
    entry.isInlineProperty = isInlineProperty;
    _entries.push_back(entry);
    
    // 状態の更新
    if (_entries.size() == 1) {
        _state = State::Monomorphic;
    } else if (_entries.size() < MEGAMORPHIC_THRESHOLD) {
        _state = State::Polymorphic;
    } else {
        _state = State::Megamorphic;
    }
}

bool PropertyCache::findEntry(uint64_t shapeId, uint32_t& outSlotOffset, bool& outIsInlineProperty) const {
    for (const auto& entry : _entries) {
        if (entry.shapeId == shapeId) {
            outSlotOffset = entry.slotOffset;
            outIsInlineProperty = entry.isInlineProperty;
            return true;
        }
    }
    
    return false;
}

void PropertyCache::clear() {
    _entries.clear();
    _state = State::Uninitialized;
    _missCount = 0;
}

//-----------------------------------------------------------------------------
// MethodCache の実装
//-----------------------------------------------------------------------------

void MethodCache::addEntry(uint64_t shapeId, uint64_t functionId, void* codeAddress) {
    // 既存エントリの検索
    for (auto& entry : _entries) {
        if (entry.shapeId == shapeId) {
            // 既存エントリの更新
            entry.functionId = functionId;
            entry.codeAddress = codeAddress;
            return;
        }
    }
    
    // 新しいエントリの追加
    Entry entry;
    entry.shapeId = shapeId;
    entry.functionId = functionId;
    entry.codeAddress = codeAddress;
    _entries.push_back(entry);
    
    // 状態の更新
    if (_entries.size() == 1) {
        _state = State::Monomorphic;
    } else if (_entries.size() < MEGAMORPHIC_THRESHOLD) {
        _state = State::Polymorphic;
    } else {
        _state = State::Megamorphic;
    }
}

bool MethodCache::findEntry(uint64_t shapeId, uint64_t& outFunctionId, void*& outCodeAddress) const {
    for (const auto& entry : _entries) {
        if (entry.shapeId == shapeId) {
            outFunctionId = entry.functionId;
            outCodeAddress = entry.codeAddress;
            return true;
        }
    }
    
    return false;
}

void MethodCache::clear() {
    _entries.clear();
    _state = State::Uninitialized;
    _missCount = 0;
}

//-----------------------------------------------------------------------------
// InlineCacheManager の実装
//-----------------------------------------------------------------------------

InlineCacheManager::InlineCacheManager(Context* context)
    : _context(context)
    , _codeGenerator(nullptr) { // 実際の実装では適切なコードジェネレータを初期化
}

PropertyCache* InlineCacheManager::getPropertyCache(uint64_t siteId) {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    
    auto it = _propertyCaches.find(siteId);
    if (it != _propertyCaches.end()) {
        return it->second.get();
    }
    
    return nullptr;
}

PropertyCache* InlineCacheManager::createPropertyCache(uint64_t siteId) {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    
    auto it = _propertyCaches.find(siteId);
    if (it != _propertyCaches.end()) {
        return it->second.get();
    }
    
    auto cache = std::make_unique<PropertyCache>();
    PropertyCache* result = cache.get();
    _propertyCaches[siteId] = std::move(cache);
    
    return result;
}

MethodCache* InlineCacheManager::getMethodCache(uint64_t siteId) {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    
    auto it = _methodCaches.find(siteId);
    if (it != _methodCaches.end()) {
        return it->second.get();
    }
    
    return nullptr;
}

MethodCache* InlineCacheManager::createMethodCache(uint64_t siteId) {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    
    auto it = _methodCaches.find(siteId);
    if (it != _methodCaches.end()) {
        return it->second.get();
    }
    
    auto cache = std::make_unique<MethodCache>();
    MethodCache* result = cache.get();
    _methodCaches[siteId] = std::move(cache);
    
    return result;
}

bool InlineCacheManager::handlePropertyAccess(uint64_t siteId, Object* obj, const std::string& propName, Value& result) {
    if (!obj) {
        return false;
    }
    
    PropertyCache* cache = getPropertyCache(siteId);
    if (!cache) {
        cache = createPropertyCache(siteId);
    }
    
    // オブジェクトの形状IDを取得
    uint64_t shapeId = obj->getShapeId();
    
    // キャッシュ内を検索
    uint32_t slotOffset;
    bool isInlineProperty;
    if (cache->findEntry(shapeId, slotOffset, isInlineProperty)) {
        // キャッシュヒット
        if (isInlineProperty) {
            // インラインプロパティの場合
            result = obj->getInlineProperty(slotOffset);
        } else {
            // アウトオブラインプロパティの場合
            result = obj->getProperty(slotOffset);
        }
    return true;
}

    // キャッシュミス - プロパティを検索
    uint32_t index;
    bool found = obj->findProperty(propName, index, isInlineProperty);
    
    if (found) {
        // プロパティが見つかった
        if (isInlineProperty) {
            result = obj->getInlineProperty(index);
        } else {
            result = obj->getProperty(index);
        }
        
        // キャッシュに追加
        cache->addEntry(shapeId, index, isInlineProperty);
        
        // キャッシュが成長したらスタブコードを生成
        if (cache->getState() == CacheEntry::State::Monomorphic) {
            // モノモーフィックスタブの生成
            generatePropertyStub(cache, siteId);
        } else if (cache->getState() == CacheEntry::State::Polymorphic) {
            // ポリモーフィックスタブの生成
            generatePropertyStub(cache, siteId);
        }
        
        // コードパッチの適用
        auto it = _propertySites.find(siteId);
        if (it != _propertySites.end()) {
            NativeCode* stubCode = _propertyStubs[siteId].get();
            for (const auto& site : it->second) {
                site.code->patchCode(siteId, &stubCode, sizeof(void*));
            }
        }
        
        return true;
    }
    
    // プロパティが見つからなかった
    cache->incrementMissCount();
    
    // ミスが閾値を超えたらメガモーフィックに遷移
    if (cache->getMissCount() > CacheEntry::MISS_THRESHOLD) {
        cache->setState(CacheEntry::State::Megamorphic);
        generatePropertyStub(cache, siteId);
    }
    
    return false;
}

bool InlineCacheManager::handleMethodCall(uint64_t siteId, Object* obj, const std::string& methodName, void*& outCodeAddress) {
    if (!obj) {
        return false;
    }
    
    MethodCache* cache = getMethodCache(siteId);
    if (!cache) {
        cache = createMethodCache(siteId);
    }
    
    // オブジェクトの形状IDを取得
    uint64_t shapeId = obj->getShapeId();
    
    // キャッシュ内を検索
    uint64_t functionId;
    if (cache->findEntry(shapeId, functionId, outCodeAddress)) {
        // キャッシュヒット
    return true;
}

    // キャッシュミス - メソッドを検索
    Value methodValue;
    bool found = obj->getMethod(methodName, methodValue);
    
    if (found && methodValue.isFunction()) {
        // メソッドが見つかった
        functionId = methodValue.asFunctionID();
        
        // コンパイル済みコードの取得
        outCodeAddress = _context->getCompiledCode(functionId);
        
        if (outCodeAddress) {
            // キャッシュに追加
            cache->addEntry(shapeId, functionId, outCodeAddress);
            
            // キャッシュが成長したらスタブコードを生成
            if (cache->getState() == CacheEntry::State::Monomorphic) {
                // モノモーフィックスタブの生成
                generateMethodStub(cache, siteId);
            } else if (cache->getState() == CacheEntry::State::Polymorphic) {
                // ポリモーフィックスタブの生成
                generateMethodStub(cache, siteId);
            }
            
            // コードパッチの適用
            auto it = _methodSites.find(siteId);
            if (it != _methodSites.end()) {
                NativeCode* stubCode = _methodStubs[siteId].get();
                for (const auto& site : it->second) {
                    site.code->patchCode(siteId, &stubCode, sizeof(void*));
                }
            }
            
            return true;
        }
    }
    
    // メソッドが見つからなかったか、コンパイル済みコードがない
    cache->incrementMissCount();
    
    // ミスが閾値を超えたらメガモーフィックに遷移
    if (cache->getMissCount() > CacheEntry::MISS_THRESHOLD) {
        cache->setState(CacheEntry::State::Megamorphic);
        generateMethodStub(cache, siteId);
    }
    
    return false;
}

void InlineCacheManager::patchPropertyAccess(uint64_t siteId, NativeCode* code, size_t patchOffset) {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    
    // パッチサイトの登録
    PatchSite site;
    site.code = code;
    site.offset = patchOffset;
    _propertySites[siteId].push_back(site);
    
    // 既存のスタブコードがあればパッチを適用
    auto it = _propertyStubs.find(siteId);
    if (it != _propertyStubs.end()) {
        NativeCode* stubCode = it->second.get();
        code->patchCode(siteId, &stubCode, sizeof(void*));
    }
}

void InlineCacheManager::patchMethodCall(uint64_t siteId, NativeCode* code, size_t patchOffset) {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    
    // パッチサイトの登録
    PatchSite site;
    site.code = code;
    site.offset = patchOffset;
    _methodSites[siteId].push_back(site);
    
    // 既存のスタブコードがあればパッチを適用
    auto it = _methodStubs.find(siteId);
    if (it != _methodStubs.end()) {
        NativeCode* stubCode = it->second.get();
        code->patchCode(siteId, &stubCode, sizeof(void*));
    }
}

size_t InlineCacheManager::getTotalCacheCount() const {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    return _propertyCaches.size() + _methodCaches.size();
}

size_t InlineCacheManager::getMonomorphicCacheCount() const {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    
    size_t count = 0;
    
    for (const auto& pair : _propertyCaches) {
        if (pair.second->getState() == CacheEntry::State::Monomorphic) {
            count++;
        }
    }
    
    for (const auto& pair : _methodCaches) {
        if (pair.second->getState() == CacheEntry::State::Monomorphic) {
            count++;
        }
    }
    
    return count;
}

size_t InlineCacheManager::getPolymorphicCacheCount() const {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    
    size_t count = 0;
    
    for (const auto& pair : _propertyCaches) {
        if (pair.second->getState() == CacheEntry::State::Polymorphic) {
            count++;
        }
    }
    
    for (const auto& pair : _methodCaches) {
        if (pair.second->getState() == CacheEntry::State::Polymorphic) {
            count++;
        }
    }
    
    return count;
}

size_t InlineCacheManager::getMegamorphicCacheCount() const {
    std::lock_guard<std::mutex> lock(_cacheMutex);
    
    size_t count = 0;
    
    for (const auto& pair : _propertyCaches) {
        if (pair.second->getState() == CacheEntry::State::Megamorphic) {
            count++;
        }
    }
    
    for (const auto& pair : _methodCaches) {
        if (pair.second->getState() == CacheEntry::State::Megamorphic) {
            count++;
        }
    }
    
    return count;
}

void InlineCacheManager::generatePropertyStub(PropertyCache* cache, uint64_t siteId) {
    // 実際の実装ではアーキテクチャ固有のスタブコードを生成
    if (!_codeGenerator || !cache) {
        return;
    }

    // キャッシュの状態に応じて異なるスタブを生成
    NativeCode* stubCode = new NativeCode();
    stubCode->buffer.reserve(256); // 十分なサイズを確保

#ifdef __x86_64__
    // x86_64アーキテクチャ用の実装
    switch (cache->getState()) {
        case CacheEntry::State::Monomorphic: {
            // モノモーフィックスタブの生成
            // - 単一形状に対する高速パス
            // - シェイプIDの比較
            // - プロパティオフセットからの直接ロード
            
            // シェイプIDをロード
            stubCode->buffer.emit8(0x48); // REX.W
            stubCode->buffer.emit8(0x8B); // mov
            stubCode->buffer.emit8(0x47); // rdi+offset
            stubCode->buffer.emit8(0x08); // シェイプIDのオフセット (仮)

            // キャッシュのシェイプIDと比較
            stubCode->buffer.emit8(0x48); // REX.W
            stubCode->buffer.emit8(0x3D); // cmp rax, imm32
            
            // モノモーフィックキャッシュから最初のエントリのシェイプIDを取得
            uint64_t expectedShapeId = 0;
            uint32_t slotOffset = 0;
            bool isInlineProperty = false;
            
            if (!cache->_entries.empty()) {
                expectedShapeId = cache->_entries[0].shapeId;
                slotOffset = cache->_entries[0].slotOffset;
                isInlineProperty = cache->_entries[0].isInlineProperty;
            }
            
            // 期待するシェイプIDを埋め込み
            stubCode->buffer.emit32(static_cast<uint32_t>(expectedShapeId & 0xFFFFFFFF));
            stubCode->buffer.emit32(static_cast<uint32_t>(expectedShapeId >> 32));
            
            // 不一致の場合はミスハンドラにジャンプ
            stubCode->buffer.emit8(0x75); // jne
            stubCode->buffer.emit8(0x15); // 相対オフセット

            // プロパティをロード
            stubCode->buffer.emit8(0x48); // REX.W
            
            if (isInlineProperty) {
                // インラインプロパティの場合
                stubCode->buffer.emit8(0x8B); // mov
                stubCode->buffer.emit8(0x47); // rdi+offset
                stubCode->buffer.emit8(static_cast<uint8_t>(0x10 + slotOffset * 8)); // プロパティオフセット
            } else {
                // アウトオブラインプロパティの場合
                stubCode->buffer.emit8(0x8B); // mov
                stubCode->buffer.emit8(0x8F); // rdi+offset
                stubCode->buffer.emit8(static_cast<uint8_t>(0x18)); // プロパティテーブルへのポインタオフセット
                
                // テーブルからプロパティをロード
                stubCode->buffer.emit8(0x48); // REX.W
                stubCode->buffer.emit8(0x8B); // mov
                stubCode->buffer.emit8(0x84); // rax+offset
                stubCode->buffer.emit8(0xC8); // rax+rcx*8
                stubCode->buffer.emit32(slotOffset * 8); // オフセット
            }
            
            // 戻り値設定
            stubCode->buffer.emit8(0xC3); // ret
            
            // ミスハンドラへのジャンプ
            void* missHandler = reinterpret_cast<void*>(&InlineCacheManager::handlePropertyMiss);
            stubCode->buffer.emit8(0x48); // REX.W
            stubCode->buffer.emit8(0xB8); // mov rax, imm64
            stubCode->buffer.emitPtr(missHandler);
            stubCode->buffer.emit8(0xFF); // jmp
            stubCode->buffer.emit8(0xE0); // rax
            break;
        }
        
        case CacheEntry::State::Polymorphic: {
            // ポリモーフィックスタブの生成
            // - 複数の形状に対するチェック連鎖
            // - 各形状IDの比較と対応するプロパティオフセットからのロード
            
            // シェイプIDをロード
            stubCode->buffer.emit8(0x48); // REX.W
            stubCode->buffer.emit8(0x8B); // mov
            stubCode->buffer.emit8(0x47); // rdi+offset
            stubCode->buffer.emit8(0x08); // シェイプIDのオフセット (仮)
            
            // 各エントリに対するチェック
            size_t entryCount = std::min(cache->_entries.size(), static_cast<size_t>(CacheEntry::MEGAMORPHIC_THRESHOLD));
            
            for (size_t i = 0; i < entryCount; i++) {
                uint64_t shapeId = cache->_entries[i].shapeId;
                uint32_t slotOffset = cache->_entries[i].slotOffset;
                bool isInlineProperty = cache->_entries[i].isInlineProperty;
                
                // シェイプIDの比較
                stubCode->buffer.emit8(0x48); // REX.W
                stubCode->buffer.emit8(0x3D); // cmp rax, imm32
                stubCode->buffer.emit32(static_cast<uint32_t>(shapeId & 0xFFFFFFFF));
                stubCode->buffer.emit32(static_cast<uint32_t>(shapeId >> 32));
                
                // 次のチェックにジャンプ（不一致の場合）
                stubCode->buffer.emit8(0x75); // jne
                
                // プロパティロードコードのサイズを計算
                size_t loadCodeSize = isInlineProperty ? 4 : 12;
                stubCode->buffer.emit8(static_cast<uint8_t>(loadCodeSize + 1)); // +1 for ret
                
                // プロパティをロード
                stubCode->buffer.emit8(0x48); // REX.W
                
                if (isInlineProperty) {
                    // インラインプロパティの場合
                    stubCode->buffer.emit8(0x8B); // mov
                    stubCode->buffer.emit8(0x47); // rdi+offset
                    stubCode->buffer.emit8(static_cast<uint8_t>(0x10 + slotOffset * 8)); // プロパティオフセット
                } else {
                    // アウトオブラインプロパティの場合
                    stubCode->buffer.emit8(0x8B); // mov
                    stubCode->buffer.emit8(0x8F); // rdi+offset
                    stubCode->buffer.emit8(static_cast<uint8_t>(0x18)); // プロパティテーブルへのポインタオフセット
                    
                    // テーブルからプロパティをロード
                    stubCode->buffer.emit8(0x48); // REX.W
                    stubCode->buffer.emit8(0x8B); // mov
                    stubCode->buffer.emit8(0x84); // rax+offset
                    stubCode->buffer.emit8(0xC8); // rax+rcx*8
                    stubCode->buffer.emit32(slotOffset * 8); // オフセット
                }
                
                // 戻り値設定
                stubCode->buffer.emit8(0xC3); // ret
            }
            
            // すべてのチェックが失敗した場合、ミスハンドラにジャンプ
            void* missHandler = reinterpret_cast<void*>(&InlineCacheManager::handlePropertyMiss);
            stubCode->buffer.emit8(0x48); // REX.W
            stubCode->buffer.emit8(0xB8); // mov rax, imm64
            stubCode->buffer.emitPtr(missHandler);
            stubCode->buffer.emit8(0xFF); // jmp
            stubCode->buffer.emit8(0xE0); // rax
            break;
        }
        
        case CacheEntry::State::Megamorphic:
        default: {
            // メガモーフィックスタブの生成
            // - 直接ハッシュテーブルを使用
            // - または単純にミスハンドラにジャンプ
            
            // ミスハンドラへのジャンプ
            void* missHandler = reinterpret_cast<void*>(&InlineCacheManager::handlePropertyMiss);
            stubCode->buffer.emit8(0x48); // REX.W
            stubCode->buffer.emit8(0xB8); // mov rax, imm64
            stubCode->buffer.emitPtr(missHandler);
            stubCode->buffer.emit8(0xFF); // jmp
            stubCode->buffer.emit8(0xE0); // rax
            break;
        }
    }
#elif defined(__aarch64__)
    // ARM64アーキテクチャ用の実装（省略）
#elif defined(__riscv)
    // RISC-Vアーキテクチャ用の実装（省略）
#else
    // 未サポートのアーキテクチャ
    delete stubCode;
    return;
#endif

    // スタブコードを実行可能にする
    stubCode->buffer.makeExecutable();
    
    // スタブコードを保存
    _propertyStubs[siteId] = std::unique_ptr<NativeCode>(stubCode);
}

void InlineCacheManager::generateMethodStub(MethodCache* cache, uint64_t siteId) {
    // 実際の実装ではアーキテクチャ固有のスタブコードを生成
    if (!_codeGenerator || !cache) {
        return;
    }

    // キャッシュの状態に応じて異なるスタブを生成
    NativeCode* stubCode = new NativeCode();
    stubCode->buffer.reserve(256); // 十分なサイズを確保

#ifdef __x86_64__
    // x86_64アーキテクチャ用の実装
    switch (cache->getState()) {
        case CacheEntry::State::Monomorphic: {
            // モノモーフィックスタブの生成
            // - 単一形状に対する高速パス
            // - シェイプIDの比較
            // - メソッド呼び出しの直接ジャンプ
            
            // シェイプIDをロード
            stubCode->buffer.emit8(0x48); // REX.W
            stubCode->buffer.emit8(0x8B); // mov
            stubCode->buffer.emit8(0x47); // rdi+offset
            stubCode->buffer.emit8(0x08); // シェイプIDのオフセット (仮)

            // キャッシュのシェイプIDと比較
            stubCode->buffer.emit8(0x48); // REX.W
            stubCode->buffer.emit8(0x3D); // cmp rax, imm32
            
            // モノモーフィックキャッシュから最初のエントリのシェイプIDを取得
            uint64_t expectedShapeId = 0;
            uint64_t functionId = 0;
            void* codeAddress = nullptr;
            
            if (!cache->_entries.empty()) {
                expectedShapeId = cache->_entries[0].shapeId;
                functionId = cache->_entries[0].functionId;
                codeAddress = cache->_entries[0].codeAddress;
            }
            
            // 期待するシェイプIDを埋め込み
            stubCode->buffer.emit32(static_cast<uint32_t>(expectedShapeId & 0xFFFFFFFF));
            stubCode->buffer.emit32(static_cast<uint32_t>(expectedShapeId >> 32));
            
            // 不一致の場合はミスハンドラにジャンプ
            stubCode->buffer.emit8(0x75); // jne
            stubCode->buffer.emit8(0x0A); // 相対オフセット

            // メソッドにジャンプ
            stubCode->buffer.emit8(0x48); // REX.W
            stubCode->buffer.emit8(0xB8); // mov rax, imm64
            stubCode->buffer.emitPtr(codeAddress);
            stubCode->buffer.emit8(0xFF); // jmp
            stubCode->buffer.emit8(0xE0); // rax
            
            // ミスハンドラへのジャンプ
            void* missHandler = reinterpret_cast<void*>(&InlineCacheManager::handleMethodMiss);
            stubCode->buffer.emit8(0x48); // REX.W
            stubCode->buffer.emit8(0xB8); // mov rax, imm64
            stubCode->buffer.emitPtr(missHandler);
            stubCode->buffer.emit8(0xFF); // jmp
            stubCode->buffer.emit8(0xE0); // rax
            break;
        }
        
        case CacheEntry::State::Polymorphic: {
            // ポリモーフィックスタブの生成
            // - 複数の形状に対するチェック連鎖
            // - 各形状IDの比較と対応するメソッドへのジャンプ
            
            // シェイプIDをロード
            stubCode->buffer.emit8(0x48); // REX.W
            stubCode->buffer.emit8(0x8B); // mov
            stubCode->buffer.emit8(0x47); // rdi+offset
            stubCode->buffer.emit8(0x08); // シェイプIDのオフセット (仮)
            
            // 各エントリに対するチェック
            size_t entryCount = std::min(cache->_entries.size(), static_cast<size_t>(CacheEntry::MEGAMORPHIC_THRESHOLD));
            
            for (size_t i = 0; i < entryCount; i++) {
                uint64_t shapeId = cache->_entries[i].shapeId;
                void* codeAddress = cache->_entries[i].codeAddress;
                
                // シェイプIDの比較
                stubCode->buffer.emit8(0x48); // REX.W
                stubCode->buffer.emit8(0x3D); // cmp rax, imm32
                stubCode->buffer.emit32(static_cast<uint32_t>(shapeId & 0xFFFFFFFF));
                stubCode->buffer.emit32(static_cast<uint32_t>(shapeId >> 32));
                
                // 次のチェックにジャンプ（不一致の場合）
                stubCode->buffer.emit8(0x75); // jne
                stubCode->buffer.emit8(0x0C); // 相対オフセット (ジャンプコードサイズ)
                
                // メソッドにジャンプ
                stubCode->buffer.emit8(0x48); // REX.W
                stubCode->buffer.emit8(0xB8); // mov rax, imm64
                stubCode->buffer.emitPtr(codeAddress);
                stubCode->buffer.emit8(0xFF); // jmp
                stubCode->buffer.emit8(0xE0); // rax
            }
            
            // すべてのチェックが失敗した場合、ミスハンドラにジャンプ
            void* missHandler = reinterpret_cast<void*>(&InlineCacheManager::handleMethodMiss);
            stubCode->buffer.emit8(0x48); // REX.W
            stubCode->buffer.emit8(0xB8); // mov rax, imm64
            stubCode->buffer.emitPtr(missHandler);
            stubCode->buffer.emit8(0xFF); // jmp
            stubCode->buffer.emit8(0xE0); // rax
            break;
        }
        
        case CacheEntry::State::Megamorphic:
        default: {
            // メガモーフィックスタブの生成
            // - 直接ハッシュテーブルを使用
            // - または単純にミスハンドラにジャンプ
            
            // ミスハンドラへのジャンプ
            void* missHandler = reinterpret_cast<void*>(&InlineCacheManager::handleMethodMiss);
            stubCode->buffer.emit8(0x48); // REX.W
            stubCode->buffer.emit8(0xB8); // mov rax, imm64
            stubCode->buffer.emitPtr(missHandler);
            stubCode->buffer.emit8(0xFF); // jmp
            stubCode->buffer.emit8(0xE0); // rax
            break;
        }
    }
#elif defined(__aarch64__)
    // ARM64アーキテクチャ用の実装（省略）
#elif defined(__riscv)
    // RISC-Vアーキテクチャ用の実装（省略）
#else
    // 未サポートのアーキテクチャ
    delete stubCode;
    return;
#endif

    // スタブコードを実行可能にする
    stubCode->buffer.makeExecutable();
    
    // スタブコードを保存
    _methodStubs[siteId] = std::unique_ptr<NativeCode>(stubCode);
}

// キャッシュミスハンドラ（スタブが呼び出す関数）
void* InlineCacheManager::handlePropertyMiss(uint64_t siteId, Object* obj, const std::string& propName) {
    // インスタンスを取得
    InlineCacheManager* manager = Context::getCurrentContext()->getInlineCacheManager();
    if (!manager) {
        return nullptr;
    }
    
    // プロパティアクセスを処理
    Value result;
    if (manager->handlePropertyAccess(siteId, obj, propName, result)) {
        // 成功した場合は結果を返す
        return &result;
    }
    
    // 失敗した場合はnullを返す
    return nullptr;
}

void* InlineCacheManager::handleMethodMiss(uint64_t siteId, Object* obj, const std::string& methodName) {
    // インスタンスを取得
    InlineCacheManager* manager = Context::getCurrentContext()->getInlineCacheManager();
    if (!manager) {
        return nullptr;
    }
    
    // メソッド呼び出しを処理
    void* codeAddress;
    if (manager->handleMethodCall(siteId, obj, methodName, codeAddress)) {
        // 成功した場合はコードアドレスを返す
        return codeAddress;
    }
    
    // 失敗した場合はnullを返す
    return nullptr;
}

} // namespace aerojs 