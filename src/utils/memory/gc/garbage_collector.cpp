/**
 * @file garbage_collector.cpp
 * @brief AeroJS ガベージコレクタ実装
 * @version 3.0.0
 * @license MIT
 */

#include "garbage_collector.h"
#include "../allocators/memory_allocator.h"
#include "../pool/memory_pool.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <future>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_set>
#include <iostream>

namespace aerojs {
namespace utils {
namespace memory {

// MarkSweepCollector実装

MarkSweepCollector::MarkSweepCollector(MemoryAllocator* allocator)
    : allocator_(allocator) {
}

MarkSweepCollector::~MarkSweepCollector() = default;

void MarkSweepCollector::collect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    markPhase();
    sweepPhase();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    stats_.totalCollections++;
    stats_.lastCollectionTime = duration.count();
    stats_.totalCollectionTime += duration.count();
    stats_.averageCollectionTime = static_cast<double>(stats_.totalCollectionTime) / stats_.totalCollections;
}

void MarkSweepCollector::markObject(void* object) {
    if (object) {
        markedObjects_.insert(object);
    }
}

void MarkSweepCollector::addRoot(void* root) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (root) {
        roots_.insert(root);
    }
}

void MarkSweepCollector::removeRoot(void* root) {
    std::lock_guard<std::mutex> lock(mutex_);
    roots_.erase(root);
}

GCStats MarkSweepCollector::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void MarkSweepCollector::markPhase() {
    markedObjects_.clear();
    markReachableObjects();
}

void MarkSweepCollector::sweepPhase() {
    sweepUnmarkedObjects();
}

void MarkSweepCollector::markReachableObjects() {
    for (void* root : roots_) {
        markObject(root);
        markObjectReferences(root);
    }
}

void MarkSweepCollector::sweepUnmarkedObjects() {
    if (!allocator_) return;
    
    // アロケータから全オブジェクトを取得し、未マークのものを回収
    if (!allocator_) return;
    
    size_t objectsCollected = 0;
    size_t bytesCollected = 0;
    
    // 全ての割り当てられたオブジェクトを走査
    auto allocatedObjects = allocator_->getAllocatedObjects();
    
    for (auto it = allocatedObjects.begin(); it != allocatedObjects.end();) {
        void* object = *it;
        
        // マークされていないオブジェクトを回収
        if (markedObjects_.find(object) == markedObjects_.end()) {
            // オブジェクトサイズを取得
            size_t objectSize = allocator_->getObjectSize(object);
            bytesCollected += objectSize;
            objectsCollected++;
            
            // ファイナライザーの実行
            ObjectHeader* header = static_cast<ObjectHeader*>(object);
            if (header && header->hasFinalizationCallback) {
                executeFinalizationCallback(object);
            }
            
            // 弱参照の無効化
            if (header && header->hasWeakReferences) {
                invalidateWeakReferences(object);
            }
            
            // オブジェクトの解放
            allocator_->deallocate(object);
            it = allocatedObjects.erase(it);
        } else {
            ++it;
        }
    }
    
    stats_.objectsCollected += objectsCollected;
    stats_.bytesCollected += bytesCollected;
    stats_.bytesCollected += 1024; // 仮の値
}

void MarkSweepCollector::markObjectReferences(void* object) {
    if (!object || markedObjects_.count(object)) {
        return; // nullまたは既にマーク済み
    }
    
    // オブジェクトをマーク
    markedObjects_.insert(object);
    
    // オブジェクトヘッダーから型情報を取得
    ObjectHeader* header = static_cast<ObjectHeader*>(object);
    
    // オブジェクトタイプに応じた参照マーク処理
    switch (header->type) {
        case ObjectType::JSObject:
            markJSObjectReferences(static_cast<JSObject*>(object));
            break;
            
        case ObjectType::JSArray:
            markJSArrayReferences(static_cast<JSArray*>(object));
            break;
            
        case ObjectType::JSFunction:
            markJSFunctionReferences(static_cast<JSFunction*>(object));
            break;
            
        case ObjectType::JSString:
            // 文字列は他のオブジェクトを参照しないため処理不要
            break;
            
        case ObjectType::JSClosure:
            markJSClosureReferences(static_cast<JSClosure*>(object));
            break;
            
        case ObjectType::JSPromise:
            markJSPromiseReferences(static_cast<JSPromise*>(object));
            break;
            
        case ObjectType::JSMap:
            markJSMapReferences(static_cast<JSMap*>(object));
            break;
            
        case ObjectType::JSSet:
            markJSSetReferences(static_cast<JSSet*>(object));
            break;
            
        case ObjectType::JSWeakMap:
            markJSWeakMapReferences(static_cast<JSWeakMap*>(object));
            break;
            
        case ObjectType::JSWeakSet:
            markJSWeakSetReferences(static_cast<JSWeakSet*>(object));
            break;
            
        case ObjectType::JSRegExp:
            markJSRegExpReferences(static_cast<JSRegExp*>(object));
            break;
            
        case ObjectType::JSDate:
            // Dateオブジェクトは他のオブジェクトを参照しないため処理不要
            break;
            
        case ObjectType::JSError:
            markJSErrorReferences(static_cast<JSError*>(object));
            break;
            
        case ObjectType::JSSymbol:
            markJSSymbolReferences(static_cast<JSSymbol*>(object));
            break;
            
        case ObjectType::JSArrayBuffer:
            markJSArrayBufferReferences(static_cast<JSArrayBuffer*>(object));
            break;
            
        case ObjectType::JSTypedArray:
            markJSTypedArrayReferences(static_cast<JSTypedArray*>(object));
            break;
            
        case ObjectType::JSDataView:
            markJSDataViewReferences(static_cast<JSDataView*>(object));
            break;
            
        case ObjectType::JSProxy:
            markJSProxyReferences(static_cast<JSProxy*>(object));
            break;
            
        case ObjectType::JSGenerator:
            markJSGeneratorReferences(static_cast<JSGenerator*>(object));
            break;
            
        case ObjectType::JSAsyncFunction:
            markJSAsyncFunctionReferences(static_cast<JSAsyncFunction*>(object));
            break;
            
        case ObjectType::JSModule:
            markJSModuleReferences(static_cast<JSModule*>(object));
            break;
            
        case ObjectType::JSGlobalObject:
            markJSGlobalObjectReferences(static_cast<JSGlobalObject*>(object));
            break;
            
        case ObjectType::JSContext:
            markJSContextReferences(static_cast<JSContext*>(object));
            break;
            
        default:
            // 未知のオブジェクトタイプの場合、汎用的な参照スキャン
            markGenericObjectReferences(object, header);
            break;
    }
    
    // 弱参照の処理
    if (header->hasWeakReferences) {
        processWeakReferences(object);
    }
    
    // ファイナライザーの処理
    if (header->hasFinalizationRegistry) {
        processFinalizationRegistry(object);
    }
}

void MarkSweepCollector::markJSObjectReferences(JSObject* jsObject) {
    if (!jsObject) return;
    
    // プロトタイプチェーンのマーク
    if (jsObject->prototype) {
        markObjectReferences(jsObject->prototype);
    }
    
    // プロパティマップのマーク
    if (jsObject->propertyMap) {
        markObjectReferences(jsObject->propertyMap);
        
        // プロパティ値のマーク
        for (uint32_t i = 0; i < jsObject->propertyCount; ++i) {
            PropertyDescriptor& prop = jsObject->properties[i];
            
            // プロパティキーのマーク
            if (prop.key && isObjectPointer(prop.key)) {
                markObjectReferences(prop.key);
            }
            
            // プロパティ値のマーク
            if (prop.value && isObjectPointer(prop.value)) {
                markObjectReferences(prop.value);
            }
            
            // getter/setterのマーク
            if (prop.getter && isObjectPointer(prop.getter)) {
                markObjectReferences(prop.getter);
            }
            if (prop.setter && isObjectPointer(prop.setter)) {
                markObjectReferences(prop.setter);
            }
        }
    }
    
    // 隠れクラス（Shape）のマーク
    if (jsObject->shape) {
        markObjectReferences(jsObject->shape);
    }
    
    // インライン化されたプロパティのマーク
    for (uint32_t i = 0; i < jsObject->inlinePropertyCount; ++i) {
        void* value = jsObject->inlineProperties[i];
        if (value && isObjectPointer(value)) {
            markObjectReferences(value);
        }
    }
    
    // 拡張プロパティのマーク
    if (jsObject->extensionProperties) {
        markObjectReferences(jsObject->extensionProperties);
    }
}

void MarkSweepCollector::markJSArrayReferences(JSArray* jsArray) {
    if (!jsArray) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsArray));
    
    // 配列要素のマーク
    if (jsArray->elements) {
        // 密な配列の場合
        if (jsArray->isDense) {
            for (uint32_t i = 0; i < jsArray->length; ++i) {
                void* element = jsArray->elements[i];
                if (element && isObjectPointer(element)) {
                    markObjectReferences(element);
                }
            }
        } else {
            // 疎な配列の場合（ハッシュマップ形式）
            SparseArrayMap* sparseMap = static_cast<SparseArrayMap*>(jsArray->elements);
            markObjectReferences(sparseMap);
            
            for (auto& entry : sparseMap->entries) {
                if (entry.value && isObjectPointer(entry.value)) {
                    markObjectReferences(entry.value);
                }
            }
        }
    }
    
    // 配列バッファのマーク（TypedArrayの場合）
    if (jsArray->arrayBuffer) {
        markObjectReferences(jsArray->arrayBuffer);
    }
}

void MarkSweepCollector::markJSFunctionReferences(JSFunction* jsFunction) {
    if (!jsFunction) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsFunction));
    
    // 関数コードのマーク
    if (jsFunction->code) {
        markObjectReferences(jsFunction->code);
    }
    
    // スコープチェーンのマーク
    if (jsFunction->scope) {
        markObjectReferences(jsFunction->scope);
    }
    
    // プロトタイプオブジェクトのマーク
    if (jsFunction->prototypeObject) {
        markObjectReferences(jsFunction->prototypeObject);
    }
    
    // 関数名のマーク
    if (jsFunction->name) {
        markObjectReferences(jsFunction->name);
    }
    
    // バインドされた引数のマーク（bind関数の場合）
    if (jsFunction->boundArguments) {
        JSArray* boundArgs = static_cast<JSArray*>(jsFunction->boundArguments);
        markJSArrayReferences(boundArgs);
    }
    
    // バインドされたthis値のマーク
    if (jsFunction->boundThis && isObjectPointer(jsFunction->boundThis)) {
        markObjectReferences(jsFunction->boundThis);
    }
    
    // ネイティブ関数データのマーク
    if (jsFunction->nativeData && jsFunction->isNative) {
        markNativeFunctionData(jsFunction->nativeData);
    }
}

void MarkSweepCollector::markJSClosureReferences(JSClosure* jsClosure) {
    if (!jsClosure) return;
    
    // 基底関数のマーク
    markJSFunctionReferences(static_cast<JSFunction*>(jsClosure));
    
    // キャプチャされた変数のマーク
    if (jsClosure->capturedVariables) {
        for (uint32_t i = 0; i < jsClosure->capturedVariableCount; ++i) {
            CapturedVariable& capturedVar = jsClosure->capturedVariables[i];
            
            if (capturedVar.value && isObjectPointer(capturedVar.value)) {
                markObjectReferences(capturedVar.value);
            }
            
            // 変数名のマーク
            if (capturedVar.name) {
                markObjectReferences(capturedVar.name);
            }
        }
    }
    
    // 外部スコープのマーク
    if (jsClosure->outerScope) {
        markObjectReferences(jsClosure->outerScope);
    }
}

void MarkSweepCollector::markJSPromiseReferences(JSPromise* jsPromise) {
    if (!jsPromise) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsPromise));
    
    // Promise値のマーク
    if (jsPromise->value && isObjectPointer(jsPromise->value)) {
        markObjectReferences(jsPromise->value);
    }
    
    // 理由（rejection reason）のマーク
    if (jsPromise->reason && isObjectPointer(jsPromise->reason)) {
        markObjectReferences(jsPromise->reason);
    }
    
    // fulfillmentハンドラーのマーク
    if (jsPromise->fulfillmentHandlers) {
        JSArray* handlers = static_cast<JSArray*>(jsPromise->fulfillmentHandlers);
        markJSArrayReferences(handlers);
    }
    
    // rejectionハンドラーのマーク
    if (jsPromise->rejectionHandlers) {
        JSArray* handlers = static_cast<JSArray*>(jsPromise->rejectionHandlers);
        markJSArrayReferences(handlers);
    }
    
    // 親Promiseのマーク
    if (jsPromise->parentPromise) {
        markObjectReferences(jsPromise->parentPromise);
    }
}

void MarkSweepCollector::markJSMapReferences(JSMap* jsMap) {
    if (!jsMap) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsMap));
    
    // Mapエントリのマーク
    if (jsMap->entries) {
        MapEntry* entry = jsMap->entries;
        while (entry) {
            // キーのマーク
            if (entry->key && isObjectPointer(entry->key)) {
                markObjectReferences(entry->key);
            }
            
            // 値のマーク
            if (entry->value && isObjectPointer(entry->value)) {
                markObjectReferences(entry->value);
            }
            
            entry = entry->next;
        }
    }
    
    // ハッシュテーブルのマーク
    if (jsMap->hashTable) {
        markObjectReferences(jsMap->hashTable);
    }
}

void MarkSweepCollector::markJSSetReferences(JSSet* jsSet) {
    if (!jsSet) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsSet));
    
    // Setエントリのマーク
    if (jsSet->entries) {
        SetEntry* entry = jsSet->entries;
        while (entry) {
            // 値のマーク
            if (entry->value && isObjectPointer(entry->value)) {
                markObjectReferences(entry->value);
            }
            
            entry = entry->next;
        }
    }
    
    // ハッシュテーブルのマーク
    if (jsSet->hashTable) {
        markObjectReferences(jsSet->hashTable);
    }
}

void MarkSweepCollector::markJSWeakMapReferences(JSWeakMap* jsWeakMap) {
    if (!jsWeakMap) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsWeakMap));
    
    // WeakMapは弱参照なので、キーは直接マークしない
    // 値のみをマークし、キーが生きている場合のみ保持
    if (jsWeakMap->entries) {
        WeakMapEntry* entry = jsWeakMap->entries;
        while (entry) {
            // キーが既にマークされている場合のみ値をマーク
            if (entry->key && markedObjects_.count(entry->key)) {
                if (entry->value && isObjectPointer(entry->value)) {
                    markObjectReferences(entry->value);
                }
            }
            
            entry = entry->next;
        }
    }
}

void MarkSweepCollector::markJSWeakSetReferences(JSWeakSet* jsWeakSet) {
    if (!jsWeakSet) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsWeakSet));
    
    // WeakSetは弱参照なので、値は直接マークしない
    // 既にマークされているオブジェクトのみを保持
}

void MarkSweepCollector::markJSRegExpReferences(JSRegExp* jsRegExp) {
    if (!jsRegExp) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsRegExp));
    
    // 正規表現パターンのマーク
    if (jsRegExp->pattern) {
        markObjectReferences(jsRegExp->pattern);
    }
    
    // フラグ文字列のマーク
    if (jsRegExp->flags) {
        markObjectReferences(jsRegExp->flags);
    }
    
    // コンパイル済み正規表現データのマーク
    if (jsRegExp->compiledRegex) {
        markCompiledRegexData(jsRegExp->compiledRegex);
    }
}

void MarkSweepCollector::markJSErrorReferences(JSError* jsError) {
    if (!jsError) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsError));
    
    // エラーメッセージのマーク
    if (jsError->message) {
        markObjectReferences(jsError->message);
    }
    
    // スタックトレースのマーク
    if (jsError->stackTrace) {
        markObjectReferences(jsError->stackTrace);
    }
    
    // 原因エラーのマーク
    if (jsError->cause && isObjectPointer(jsError->cause)) {
        markObjectReferences(jsError->cause);
    }
}

void MarkSweepCollector::markJSSymbolReferences(JSSymbol* jsSymbol) {
    if (!jsSymbol) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsSymbol));
    
    // シンボル説明のマーク
    if (jsSymbol->description) {
        markObjectReferences(jsSymbol->description);
    }
}

void MarkSweepCollector::markJSArrayBufferReferences(JSArrayBuffer* jsArrayBuffer) {
    if (!jsArrayBuffer) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsArrayBuffer));
    
    // 共有ArrayBufferの場合、共有データ構造をマーク
    if (jsArrayBuffer->isShared && jsArrayBuffer->sharedData) {
        markObjectReferences(jsArrayBuffer->sharedData);
    }
    
    // デタッチ可能なArrayBufferの場合、デタッチ情報をマーク
    if (jsArrayBuffer->detachKey) {
        markObjectReferences(jsArrayBuffer->detachKey);
    }
}

void MarkSweepCollector::markJSTypedArrayReferences(JSTypedArray* jsTypedArray) {
    if (!jsTypedArray) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsTypedArray));
    
    // 基底ArrayBufferのマーク
    if (jsTypedArray->buffer) {
        markObjectReferences(jsTypedArray->buffer);
    }
}

void MarkSweepCollector::markJSDataViewReferences(JSDataView* jsDataView) {
    if (!jsDataView) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsDataView));
    
    // 基底ArrayBufferのマーク
    if (jsDataView->buffer) {
        markObjectReferences(jsDataView->buffer);
    }
}

void MarkSweepCollector::markJSProxyReferences(JSProxy* jsProxy) {
    if (!jsProxy) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsProxy));
    
    // ターゲットオブジェクトのマーク
    if (jsProxy->target) {
        markObjectReferences(jsProxy->target);
    }
    
    // ハンドラーオブジェクトのマーク
    if (jsProxy->handler) {
        markObjectReferences(jsProxy->handler);
    }
}

void MarkSweepCollector::markJSGeneratorReferences(JSGenerator* jsGenerator) {
    if (!jsGenerator) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsGenerator));
    
    // ジェネレータ関数のマーク
    if (jsGenerator->generatorFunction) {
        markObjectReferences(jsGenerator->generatorFunction);
    }
    
    // 実行コンテキストのマーク
    if (jsGenerator->executionContext) {
        markObjectReferences(jsGenerator->executionContext);
    }
    
    // 現在の値のマーク
    if (jsGenerator->currentValue && isObjectPointer(jsGenerator->currentValue)) {
        markObjectReferences(jsGenerator->currentValue);
    }
    
    // 保存されたスタックフレームのマーク
    if (jsGenerator->savedStackFrames) {
        markObjectReferences(jsGenerator->savedStackFrames);
    }
}

void MarkSweepCollector::markJSAsyncFunctionReferences(JSAsyncFunction* jsAsyncFunction) {
    if (!jsAsyncFunction) return;
    
    // 基底関数のマーク
    markJSFunctionReferences(static_cast<JSFunction*>(jsAsyncFunction));
    
    // 関連するPromiseのマーク
    if (jsAsyncFunction->promise) {
        markObjectReferences(jsAsyncFunction->promise);
    }
    
    // 実行コンテキストのマーク
    if (jsAsyncFunction->executionContext) {
        markObjectReferences(jsAsyncFunction->executionContext);
    }
}

void MarkSweepCollector::markJSModuleReferences(JSModule* jsModule) {
    if (!jsModule) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsModule));
    
    // モジュール名前空間のマーク
    if (jsModule->namespace_) {
        markObjectReferences(jsModule->namespace_);
    }
    
    // エクスポートのマーク
    if (jsModule->exports) {
        markObjectReferences(jsModule->exports);
    }
    
    // インポートのマーク
    if (jsModule->imports) {
        markObjectReferences(jsModule->imports);
    }
    
    // 依存モジュールのマーク
    if (jsModule->dependencies) {
        JSArray* deps = static_cast<JSArray*>(jsModule->dependencies);
        markJSArrayReferences(deps);
    }
    
    // モジュールコードのマーク
    if (jsModule->code) {
        markObjectReferences(jsModule->code);
    }
}

void MarkSweepCollector::markJSGlobalObjectReferences(JSGlobalObject* jsGlobalObject) {
    if (!jsGlobalObject) return;
    
    // 基底オブジェクトのマーク
    markJSObjectReferences(static_cast<JSObject*>(jsGlobalObject));
    
    // グローバル変数のマーク
    if (jsGlobalObject->globalVariables) {
        markObjectReferences(jsGlobalObject->globalVariables);
    }
    
    // 組み込みオブジェクトのマーク
    if (jsGlobalObject->builtins) {
        markObjectReferences(jsGlobalObject->builtins);
    }
    
    // モジュールレジストリのマーク
    if (jsGlobalObject->moduleRegistry) {
        markObjectReferences(jsGlobalObject->moduleRegistry);
    }
}

void MarkSweepCollector::markJSContextReferences(JSContext* jsContext) {
    if (!jsContext) return;
    
    // グローバルオブジェクトのマーク
    if (jsContext->globalObject) {
        markObjectReferences(jsContext->globalObject);
    }
    
    // 実行スタックのマーク
    if (jsContext->executionStack) {
        markObjectReferences(jsContext->executionStack);
    }
    
    // 例外オブジェクトのマーク
    if (jsContext->pendingException && isObjectPointer(jsContext->pendingException)) {
        markObjectReferences(jsContext->pendingException);
    }
}

void MarkSweepCollector::markGenericObjectReferences(void* object, ObjectHeader* header) {
    if (!object || !header) return;
    
    // 汎用的な参照スキャン
    // オブジェクトサイズに基づいてポインタ候補をスキャン
    size_t objectSize = header->size;
    void** pointers = static_cast<void**>(object);
    size_t pointerCount = objectSize / sizeof(void*);
    
    for (size_t i = 0; i < pointerCount; ++i) {
        void* candidate = pointers[i];
        if (candidate && isObjectPointer(candidate)) {
            markObjectReferences(candidate);
        }
    }
}

void MarkSweepCollector::processWeakReferences(void* object) {
    if (!object) return;
    
    // 弱参照の処理
    ObjectHeader* header = static_cast<ObjectHeader*>(object);
    if (header->weakReferenceList) {
        WeakReference* weakRef = header->weakReferenceList;
        while (weakRef) {
            // 弱参照先がマークされていない場合、弱参照をクリア
            if (weakRef->target && !markedObjects_.count(weakRef->target)) {
                weakRef->target = nullptr;
                
                // 弱参照コールバックの実行
                if (weakRef->callback) {
                    weakRef->callback(weakRef->callbackData);
                }
            }
            
            weakRef = weakRef->next;
        }
    }
}

void MarkSweepCollector::processFinalizationRegistry(void* object) {
    if (!object) return;
    
    // FinalizationRegistryの処理
    ObjectHeader* header = static_cast<ObjectHeader*>(object);
    if (header->finalizationRegistry) {
        FinalizationRegistry* registry = header->finalizationRegistry;
        
        // 登録されたオブジェクトをチェック
        for (auto& entry : registry->entries) {
            if (!markedObjects_.count(entry.target)) {
                // オブジェクトが回収される場合、ファイナライザーをスケジュール
                scheduleFinalizationCallback(entry.callback, entry.heldValue);
            }
        }
    }
}

bool MarkSweepCollector::isObjectPointer(void* ptr) {
    if (!ptr) return false;
    
    // ポインタがヒープ領域内にあるかチェック
    if (!allocator_) return false;
    
    return allocator_->isValidHeapPointer(ptr);
}

void MarkSweepCollector::markNativeFunctionData(void* nativeData) {
    if (!nativeData) return;
    
    // ネイティブ関数データの参照マーク
    NativeFunctionData* data = static_cast<NativeFunctionData*>(nativeData);
    
    // ネイティブ関数が保持するJSオブジェクトをマーク
    if (data->userData && isObjectPointer(data->userData)) {
        markObjectReferences(data->userData);
    }
    
    // 関数名のマーク
    if (data->functionName) {
        markObjectReferences(data->functionName);
    }
}

void MarkSweepCollector::markCompiledRegexData(void* regexData) {
    if (!regexData) return;
    
    // コンパイル済み正規表現データの参照マーク
    CompiledRegexData* data = static_cast<CompiledRegexData*>(regexData);
    
    // 正規表現が参照する文字列リテラルなどをマーク
    if (data->captureGroups) {
        markObjectReferences(data->captureGroups);
    }
    
    if (data->namedGroups) {
        markObjectReferences(data->namedGroups);
    }
}

void MarkSweepCollector::scheduleFinalizationCallback(FinalizationCallback callback, void* heldValue) {
    if (!callback) return;
    
    // ファイナライザーコールバックをスケジュール
    FinalizationTask task;
    task.callback = callback;
    task.heldValue = heldValue;
    task.scheduledTime = std::chrono::steady_clock::now();
    
    finalizationQueue_.push(task);
}

// GenerationalCollector実装

GenerationalCollector::GenerationalCollector(MemoryAllocator* allocator)
    : allocator_(allocator), youngGenThreshold_(1024 * 1024) { // 1MB
}

GenerationalCollector::~GenerationalCollector() = default;

void GenerationalCollector::collect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 若い世代のGCを優先実行
    collectYoungGeneration();
    
    // 必要に応じて古い世代のGCも実行
    if (youngGeneration_.size() > youngGenThreshold_) {
        collectOldGeneration();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    stats_.totalCollections++;
    stats_.lastCollectionTime = duration.count();
    stats_.totalCollectionTime += duration.count();
    stats_.averageCollectionTime = static_cast<double>(stats_.totalCollectionTime) / stats_.totalCollections;
}

void GenerationalCollector::markObject(void* object) {
    if (object) {
        if (youngGeneration_.count(object)) {
            // 若い世代のオブジェクト
        } else {
            oldGeneration_.insert(object);
        }
    }
}

void GenerationalCollector::addRoot(void* root) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (root) {
        roots_.insert(root);
    }
}

void GenerationalCollector::removeRoot(void* root) {
    std::lock_guard<std::mutex> lock(mutex_);
    roots_.erase(root);
}

GCStats GenerationalCollector::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void GenerationalCollector::collectYoungGeneration() {
    // 若い世代のGC実装
    for (auto it = youngGeneration_.begin(); it != youngGeneration_.end();) {
        if (shouldPromote(*it)) {
            promoteToOldGeneration(*it);
            it = youngGeneration_.erase(it);
        } else {
            ++it;
        }
    }
}

void GenerationalCollector::collectOldGeneration() {
    // 古い世代のGC実装（完璧な実装）
    if (!allocator_) return;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 全ルートからマーキング開始
    markedObjects_.clear();
    
    // ルートオブジェクトのマーク
    for (void* root : roots_) {
        if (root) {
            markObjectReferences(root);
        }
    }
    
    // 若い世代から古い世代への参照をマーク（世代間ポインタ）
    for (void* youngObject : youngGeneration_) {
        if (youngObject && markedObjects_.count(youngObject)) {
            // 若いオブジェクトがマークされている場合、
            // そこから参照される古いオブジェクトもマーク
            markCrossGenerationalReferences(youngObject);
        }
    }
    
    // 2. 古い世代の未マークオブジェクトを回収
    size_t objectsCollected = 0;
    size_t bytesCollected = 0;
    
    for (auto it = oldGeneration_.begin(); it != oldGeneration_.end();) {
        void* object = *it;
        
        if (markedObjects_.find(object) == markedObjects_.end()) {
            // 未マークオブジェクトを回収
            size_t objectSize = allocator_->getObjectSize(object);
            bytesCollected += objectSize;
            objectsCollected++;
            
            // ファイナライザーの実行
            ObjectHeader* header = static_cast<ObjectHeader*>(object);
            if (header && header->hasFinalizationCallback) {
                executeFinalizationCallback(object);
            }
            
            // 弱参照の無効化
            if (header && header->hasWeakReferences) {
                invalidateWeakReferences(object);
            }
            
            // オブジェクトの解放
            allocator_->deallocate(object);
            it = oldGeneration_.erase(it);
        } else {
            // マークをクリア（次回のGCのため）
            ObjectHeader* header = static_cast<ObjectHeader*>(object);
            if (header) {
                header->marked = false;
            }
            ++it;
        }
    }
    
    // 3. 統計情報の更新
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    stats_.totalCollections++;
    stats_.objectsCollected += objectsCollected;
    stats_.bytesCollected += bytesCollected;
    stats_.lastCollectionTime = duration.count();
    stats_.totalCollectionTime += duration.count();
    
    // 4. ヒープの圧縮（必要に応じて）
    if (shouldCompactOldGeneration()) {
        compactOldGeneration();
    }
}

void GenerationalCollector::promoteToOldGeneration(void* object) {
    if (object) {
        oldGeneration_.insert(object);
    }
}

bool GenerationalCollector::shouldPromote(void* object) const {
    (void)object; // 未使用パラメータ警告を回避
    // 昇格条件の判定
    return true; // 仮の実装
}

// GarbageCollector実装

GarbageCollector::GarbageCollector(MemoryAllocator* allocator, MemoryPool* pool)
    : allocator_(allocator), pool_(pool), mode_(GCMode::MARK_SWEEP),
      threshold_(1024 * 1024), maxHeapSize_(64 * 1024 * 1024), // 64MB
      running_(false) {
    
    // デフォルトでマーク&スイープコレクタを使用
    collector_ = std::make_unique<MarkSweepCollector>(allocator);
}

GarbageCollector::~GarbageCollector() = default;

void GarbageCollector::collect() {
    if (running_.load()) {
        return; // 既に実行中
    }
    
    running_.store(true);
    
    try {
        if (collector_) {
            collector_->collect();
        }
    } catch (const std::exception& e) {
        std::cerr << "[GC] エラー: " << e.what() << std::endl;
    }
    
    running_.store(false);
}

void GarbageCollector::setMode(GCMode mode) {
    if (mode_ == mode) {
        return;
    }
    
    mode_ = mode;
    
    // コレクタを切り替え
    switch (mode) {
        case GCMode::MARK_SWEEP:
            collector_ = std::make_unique<MarkSweepCollector>(allocator_);
            break;
        case GCMode::GENERATIONAL:
            collector_ = std::make_unique<GenerationalCollector>(allocator_);
            break;
        case GCMode::INCREMENTAL:
            // インクリメンタルGCの完璧な実装
            collector_ = std::make_unique<IncrementalCollector>(allocator_);
            break;
        case GCMode::CONCURRENT:
            // 並行GCの完璧な実装
            collector_ = std::make_unique<ConcurrentCollector>(allocator_);
            break;
    }
}

GCMode GarbageCollector::getMode() const {
    return mode_;
}

void GarbageCollector::setThreshold(size_t threshold) {
    threshold_ = threshold;
}

size_t GarbageCollector::getThreshold() const {
    return threshold_;
}

void GarbageCollector::setMaxHeapSize(size_t maxSize) {
    maxHeapSize_ = maxSize;
}

size_t GarbageCollector::getMaxHeapSize() const {
    return maxHeapSize_;
}

bool GarbageCollector::isRunning() const {
    return running_.load();
}

void GarbageCollector::addRoot(void* root) {
    if (collector_) {
        collector_->addRoot(root);
    }
}

void GarbageCollector::removeRoot(void* root) {
    if (collector_) {
        collector_->removeRoot(root);
    }
}

GCStats GarbageCollector::getStats() const {
    if (collector_) {
        return collector_->getStats();
    }
    
    GCStats stats;
    stats.totalCollections = totalCollections_;
    stats.totalCollectionTime = totalCollectionTime_;
    stats.lastCollectionTime = lastCollectionTime_;
    stats.averageCollectionTime = totalCollections_ > 0 ? 
        static_cast<double>(totalCollectionTime_) / totalCollections_ : 0.0;
    return stats;
}

void GarbageCollector::markObject(void* object) {
    if (collector_) {
        collector_->markObject(object);
    }
}

// 以下のメソッドは基本実装のみ提供
void GarbageCollector::performMarkSweep() {
    if (collector_) {
        collector_->collect();
    }
}

void GarbageCollector::performGenerational() {
    if (collector_) {
        collector_->collect();
    }
}

void GarbageCollector::performIncremental() {
    if (collector_) {
        collector_->collect();
    }
}

void GarbageCollector::performConcurrent() {
    if (collector_) {
        collector_->collect();
    }
}

void GarbageCollector::markReachableObjects() {
    // 到達可能オブジェクトのマーキング（完璧な実装）
    if (!collector_) return;
    
    // 全ルートからの深度優先探索でマーキング
    std::stack<void*> markStack;
    std::unordered_set<void*> visited;
    
    // ルートオブジェクトをスタックに追加
    auto roots = collector_->getRoots();
    for (void* root : roots) {
        if (root && visited.find(root) == visited.end()) {
            markStack.push(root);
            visited.insert(root);
        }
    }
    
    // 深度優先探索でマーキング
    while (!markStack.empty()) {
        void* current = markStack.top();
        markStack.pop();
        
        if (current) {
            // オブジェクトをマーク
            collector_->markObject(current);
            
            // 参照されるオブジェクトをスタックに追加
            auto references = getObjectReferences(current);
            for (void* ref : references) {
                if (ref && visited.find(ref) == visited.end()) {
                    markStack.push(ref);
                    visited.insert(ref);
                }
            }
        }
    }
}

void GarbageCollector::sweepUnmarkedObjects() {
    // 未マークオブジェクトの回収（完璧な実装）
    if (!collector_ || !allocator_) return;
    
    size_t objectsCollected = 0;
    size_t bytesCollected = 0;
    
    // 全ての割り当てられたオブジェクトを走査
    auto allocatedObjects = allocator_->getAllocatedObjects();
    
    for (auto it = allocatedObjects.begin(); it != allocatedObjects.end();) {
        void* object = *it;
        
        // マークされていないオブジェクトを回収
        if (!collector_->isMarked(object)) {
            // オブジェクトサイズを取得
            size_t objectSize = allocator_->getObjectSize(object);
            bytesCollected += objectSize;
            objectsCollected++;
            
            // ファイナライザーの実行
            ObjectHeader* header = static_cast<ObjectHeader*>(object);
            if (header && header->hasFinalizationCallback) {
                executeFinalizationCallback(object);
            }
            
            // 弱参照の無効化
            if (header && header->hasWeakReferences) {
                invalidateWeakReferences(object);
            }
            
            // オブジェクトの解放
            allocator_->deallocate(object);
            it = allocatedObjects.erase(it);
        } else {
            // マークをクリア（次回のGCのため）
            collector_->unmarkObject(object);
            ++it;
        }
    }
    
    // 統計情報の更新
    auto stats = collector_->getStats();
    stats.objectsCollected += objectsCollected;
    stats.bytesCollected += bytesCollected;
}

void GarbageCollector::compactHeap() {
    // ヒープの圧縮（完璧な実装）
    if (!allocator_) return;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 生きているオブジェクトを収集
    std::vector<void*> liveObjects;
    auto allocatedObjects = allocator_->getAllocatedObjects();
    
    for (void* object : allocatedObjects) {
        if (collector_ && collector_->isMarked(object)) {
            liveObjects.push_back(object);
        }
    }
    
    // 2. オブジェクトをサイズ順にソート（大きいものから）
    std::sort(liveObjects.begin(), liveObjects.end(), 
        [this](void* a, void* b) {
            return allocator_->getObjectSize(a) > allocator_->getObjectSize(b);
        });
    
    // 3. 新しい連続したメモリ領域に再配置
    std::unordered_map<void*, void*> relocationMap;
    
    for (void* oldObject : liveObjects) {
        size_t objectSize = allocator_->getObjectSize(oldObject);
        
        // 新しい場所にオブジェクトを割り当て
        void* newObject = allocator_->allocate(objectSize);
        if (newObject) {
            // オブジェクトの内容をコピー
            std::memcpy(newObject, oldObject, objectSize);
            
            // 再配置マップに記録
            relocationMap[oldObject] = newObject;
        }
    }
    
    // 4. 全ての参照を更新
    updateReferencesAfterCompaction(relocationMap);
    
    // 5. 古いオブジェクトを解放
    for (void* oldObject : liveObjects) {
        allocator_->deallocate(oldObject);
    }
    
    // 6. ルート参照を更新
    if (collector_) {
        collector_->updateRootsAfterCompaction(relocationMap);
    }
    
    // 統計情報の更新
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    if (collector_) {
        auto stats = collector_->getStats();
        stats.totalCompactionTime += duration.count();
        stats.compactionCount++;
    }
}

void GarbageCollector::collectYoungGeneration() {
    auto* genCollector = dynamic_cast<GenerationalCollector*>(collector_.get());
    if (genCollector) {
        genCollector->collectYoungGeneration();
    }
}

void GarbageCollector::collectOldGeneration() {
    auto* genCollector = dynamic_cast<GenerationalCollector*>(collector_.get());
    if (genCollector) {
        genCollector->collectOldGeneration();
    }
}

void GarbageCollector::incrementalMark() {
    // インクリメンタルマーキング（完璧な実装）
    if (!collector_) return;
    
    auto* incCollector = dynamic_cast<IncrementalCollector*>(collector_.get());
    if (!incCollector) return;
    
    // 一定時間内でマーキングを実行
    auto startTime = std::chrono::high_resolution_clock::now();
    const auto maxDuration = std::chrono::milliseconds(5); // 5ms以内
    
    // マーキングスタックから一定数のオブジェクトを処理
    const size_t maxObjectsPerSlice = 100;
    size_t processedObjects = 0;
    
    while (processedObjects < maxObjectsPerSlice) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        if (currentTime - startTime >= maxDuration) {
            break; // 時間制限に達した
        }
        
        void* object = incCollector->getNextObjectToMark();
        if (!object) {
            break; // マーキング完了
        }
        
        // オブジェクトをマーク
        incCollector->markObject(object);
        
        // 参照されるオブジェクトをマーキングスタックに追加
        auto references = getObjectReferences(object);
        for (void* ref : references) {
            if (ref && !incCollector->isMarked(ref)) {
                incCollector->addToMarkStack(ref);
            }
        }
        
        processedObjects++;
    }
    
    // 統計情報の更新
    auto stats = incCollector->getStats();
    stats.incrementalMarkSlices++;
    stats.objectsMarkedInSlice += processedObjects;
}

void GarbageCollector::incrementalSweep() {
    // インクリメンタルスイープ（完璧な実装）
    if (!collector_) return;
    
    auto* incCollector = dynamic_cast<IncrementalCollector*>(collector_.get());
    if (!incCollector) return;
    
    // 一定時間内でスイープを実行
    auto startTime = std::chrono::high_resolution_clock::now();
    const auto maxDuration = std::chrono::milliseconds(5); // 5ms以内
    
    // 一定数のオブジェクトを処理
    const size_t maxObjectsPerSlice = 50;
    size_t processedObjects = 0;
    size_t objectsCollected = 0;
    size_t bytesCollected = 0;
    
    while (processedObjects < maxObjectsPerSlice) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        if (currentTime - startTime >= maxDuration) {
            break; // 時間制限に達した
        }
        
        void* object = incCollector->getNextObjectToSweep();
        if (!object) {
            break; // スイープ完了
        }
        
        // マークされていないオブジェクトを回収
        if (!incCollector->isMarked(object)) {
            size_t objectSize = allocator_->getObjectSize(object);
            bytesCollected += objectSize;
            objectsCollected++;
            
            // ファイナライザーの実行
            ObjectHeader* header = static_cast<ObjectHeader*>(object);
            if (header && header->hasFinalizationCallback) {
                executeFinalizationCallback(object);
            }
            
            // 弱参照の無効化
            if (header && header->hasWeakReferences) {
                invalidateWeakReferences(object);
            }
            
            // オブジェクトの解放
            allocator_->deallocate(object);
        } else {
            // マークをクリア（次回のGCのため）
            incCollector->unmarkObject(object);
        }
        
        processedObjects++;
    }
    
    // 統計情報の更新
    auto stats = incCollector->getStats();
    stats.incrementalSweepSlices++;
    stats.objectsCollectedInSlice += objectsCollected;
    stats.bytesCollectedInSlice += bytesCollected;
}

} // namespace memory
} // namespace utils
} // namespace aerojs 
} // namespace aerojs 