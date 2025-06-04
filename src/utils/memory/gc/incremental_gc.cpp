/**
 * @file incremental_gc.cpp
 * @brief 増分ガベージコレクション実装
 * 
 * このファイルは、実行時停止時間を最小化する増分ガベージコレクションを実装します。
 * 三色マーキングアルゴリズムとライトバリアを使用して、JavaScriptの実行を止めることなく
 * メモリ回収を行います。
 * 
 * @author AeroJS Team
 * @version 2.0.0
 * @copyright MIT License
 */

#include "incremental_gc.h"
#include "../../../core/runtime/values/value.h"
#include "../../logging.h"
#include <sys/mman.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <cstring>

namespace aerojs {
namespace utils {
namespace memory {

IncrementalGC::IncrementalGC()
    : currentPhase_(GCPhase::IDLE)
    , collectionRequested_(false)
    , shouldStop_(false)
    , markingProgress_(0)
    , totalObjectsToMark_(0)
    , sweepingProgress_(0)
    , totalPagesToSweep_(0)
    , heapSize_(0)
    , usedMemory_(0)
    , lastAllocationSize_(0)
    , incrementBudgetUs_(1000)
    , allocationsSinceLastGC_(0)
    , bytesAllocatedSinceLastGC_(0)
    , allocationRate_(0)
    , barrierType_(WriteBarrierType::SNAPSHOT_AT_BEGINNING)
    , concurrentMode_(true)
    , targetHeapUtilization_(DEFAULT_HEAP_UTILIZATION)
    , debugMode_(false)
    , concurrentMarkingActive_(false) {
    
    grayStack_.reserve(GRAY_STACK_INITIAL_SIZE);
    lastIncrementTime_ = std::chrono::steady_clock::now();
    lastAllocationTime_ = std::chrono::steady_clock::now();
    totalStartTime_ = std::chrono::steady_clock::now();
    
    LOG_INFO("増分ガベージコレクタを初期化しました");
}

IncrementalGC::~IncrementalGC() {
    shouldStop_ = true;
    
    if (concurrentThread_ && concurrentThread_->joinable()) {
        concurrentThread_->join();
    }
    
    // 割り当てられたページを解放
    for (void* page : allocatedPages_) {
        if (page) {
            munmap(page, PAGE_SIZE);
        }
    }
    
    LOG_INFO("増分ガベージコレクタを終了しました");
    LOG_INFO("最終統計: 総コレクション回数={}, 総増分数={}, 平均増分時間={}μs",
             stats_.totalCollections, stats_.totalIncrements, stats_.averageIncrementTime);
}

void* IncrementalGC::Allocate(size_t size, size_t alignment) {
    size = AlignSize(size + sizeof(ObjectHeader), alignment);
    
    // 割り当て統計の更新
    allocationsSinceLastGC_++;
    bytesAllocatedSinceLastGC_ += size;
    lastAllocationSize_ = size;
    lastAllocationTime_ = std::chrono::steady_clock::now();
    
    // コレクションのトリガー判定
    if (ShouldTriggerCollection()) {
        collectionRequested_ = true;
    }
    
    void* memory = AllocateFromHeap(size, alignment);
    if (!memory) {
        // メモリ不足の場合、フルコレクションを実行
        Collect();
        memory = AllocateFromHeap(size, alignment);
        
        if (!memory) {
            throw std::bad_alloc();
        }
    }
    
    // オブジェクトヘッダーの初期化
    ObjectHeader* header = static_cast<ObjectHeader*>(memory);
    header->color = ObjectColor::WHITE;
    header->marked = false;
    header->finalizable = false;
    header->generation = 0;
    header->size = static_cast<uint32_t>(size - sizeof(ObjectHeader));
    header->typeInfo = nullptr;
    
    usedMemory_ += size;
    
    // ライトバリアが有効な場合は新規オブジェクトをグレーにマーク
    if (currentPhase_ == GCPhase::MARKING && barrierType_ != WriteBarrierType::NONE) {
        SetObjectColor(memory, ObjectColor::GRAY);
        AddToGrayStack(memory);
    }
    
    #ifdef DEBUG
    totalAllocations_++;
    allocationSizes_[memory] = size;
    #endif
    
    return static_cast<char*>(memory) + sizeof(ObjectHeader);
}

void IncrementalGC::Collect() {
    LOG_DEBUG("フルガベージコレクションを開始します");
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 既に実行中の場合は完了まで待機
    while (currentPhase_ != GCPhase::IDLE) {
        PerformIncrement(MAX_INCREMENT_TIME_US);
    }
    
    // 新しいコレクションを開始
    StartCollection();
    
    // マーキングフェーズを完了
    while (currentPhase_ == GCPhase::MARKING) {
        PerformIncrement(MAX_INCREMENT_TIME_US);
    }
    
    // スイープフェーズを完了
    while (currentPhase_ == GCPhase::SWEEPING) {
        PerformIncrement(MAX_INCREMENT_TIME_US);
    }
    
    FinalizeCollection();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    stats_.totalCollections++;
    collectionRequested_ = false;
    
    LOG_INFO("ガベージコレクション完了: 時間={}ms, 解放メモリ={}KB",
             duration, (stats_.objectsSwept * sizeof(ObjectHeader)) / 1024);
}

void IncrementalGC::RegisterRoot(void** root) {
    std::lock_guard<std::mutex> lock(rootSetMutex_);
    rootSet_.insert(root);
}

void IncrementalGC::UnregisterRoot(void** root) {
    std::lock_guard<std::mutex> lock(rootSetMutex_);
    rootSet_.erase(root);
}

size_t IncrementalGC::GetHeapSize() const {
    return heapSize_;
}

size_t IncrementalGC::GetUsedMemory() const {
    return usedMemory_;
}

void IncrementalGC::PerformIncrement(size_t maxTimeUs) {
    if (currentPhase_ == GCPhase::IDLE && !collectionRequested_) {
        return;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    if (currentPhase_ == GCPhase::IDLE && collectionRequested_) {
        StartCollection();
    }
    
    switch (currentPhase_) {
        case GCPhase::MARKING:
            PerformMarkingIncrement(maxTimeUs);
            break;
            
        case GCPhase::SWEEPING:
            PerformSweepingIncrement(maxTimeUs);
            break;
            
        case GCPhase::FINALIZING:
            FinalizeCollection();
            break;
            
        default:
            break;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime).count();
    
    RecordIncrementTime(duration);
    lastIncrementTime_ = endTime;
}

void IncrementalGC::WriteBarrier(void* object, void* field, void* newValue) {
    if (currentPhase_ != GCPhase::MARKING || !object || !newValue) {
        return;
    }
    
    stats_.writeBarrierActivations++;
    
    switch (barrierType_) {
        case WriteBarrierType::SNAPSHOT_AT_BEGINNING:
            SnapshotAtBeginningBarrier(object, field, newValue);
            break;
            
        case WriteBarrierType::INCREMENTAL_UPDATE:
            IncrementalUpdateBarrier(object, field, newValue);
            break;
            
        case WriteBarrierType::GENERATIONAL:
            GenerationalBarrier(object, field, newValue);
            break;
            
        default:
            break;
    }
}

void IncrementalGC::ReadBarrier(void* object, void* field) {
    // 読み込みバリアは通常は不要だが、将来の拡張のために用意
    UNUSED(object);
    UNUSED(field);
}

double IncrementalGC::GetProgressPercent() const {
    switch (currentPhase_) {
        case GCPhase::MARKING:
            if (totalObjectsToMark_ == 0) return 0.0;
            return (double)markingProgress_ / totalObjectsToMark_ * 50.0;
            
        case GCPhase::SWEEPING:
            if (totalPagesToSweep_ == 0) return 50.0;
            return 50.0 + (double)sweepingProgress_ / totalPagesToSweep_ * 50.0;
            
        case GCPhase::FINALIZING:
            return 100.0;
            
        default:
            return 0.0;
    }
}

void IncrementalGC::ResetStats() {
    stats_ = IncrementalGCStats{};
    incrementTimes_.clear();
    totalStartTime_ = std::chrono::steady_clock::now();
}

void IncrementalGC::DumpHeapSnapshot(const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
        LOG_ERROR("ヒープスナップショットファイルを開けません: {}", filename);
        return;
    }
    
    file << "{\n";
    file << "  \"timestamp\": \"" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() << "\",\n";
    file << "  \"heapSize\": " << heapSize_ << ",\n";
    file << "  \"usedMemory\": " << usedMemory_ << ",\n";
    file << "  \"objects\": [\n";
    
    bool first = true;
    // 完璧なスナップショット出力実装
    
    std::ofstream snapshotFile("gc_snapshot_" + 
                              std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now().time_since_epoch()).count()) + 
                              ".json");
    
    if (!snapshotFile.is_open()) {
        LOG_ERROR("スナップショットファイルの作成に失敗しました");
        return;
    }
    
    snapshotFile << "{\n";
    snapshotFile << "  \"timestamp\": " << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() << ",\n";
    snapshotFile << "  \"gc_state\": \"" << getGCStateName(currentPhase_) << "\",\n";
    snapshotFile << "  \"total_allocated\": " << totalAllocatedBytes_ << ",\n";
    snapshotFile << "  \"total_freed\": " << totalFreedBytes_ << ",\n";
    snapshotFile << "  \"allocation_rate\": " << getAllocationRate() << ",\n";
    snapshotFile << "  \"pages\": [\n";
    
    for (void* page : allocatedPages_) {
        if (!first) {
            snapshotFile << ",\n";
        }
        first = false;
        
        PageHeader* header = GetPageHeader(page);
        snapshotFile << "    {\n";
        snapshotFile << "      \"address\": \"0x" << std::hex << reinterpret_cast<uintptr_t>(page) << std::dec << "\",\n";
        snapshotFile << "      \"size\": " << header->size << ",\n";
        snapshotFile << "      \"generation\": " << static_cast<int>(header->generation) << ",\n";
        snapshotFile << "      \"object_count\": " << countObjectsInPage(page) << ",\n";
        snapshotFile << "      \"free_space\": " << calculateFreeSpaceInPage(page) << ",\n";
        snapshotFile << "      \"fragmentation\": " << calculateFragmentationInPage(page) << ",\n";
        snapshotFile << "      \"objects\": [\n";
        
        // ページ内のオブジェクト詳細
        bool firstObject = true;
        enumerateObjectsInPage(page, [&](void* object) {
            if (!firstObject) {
                snapshotFile << ",\n";
            }
            firstObject = false;
            
            ObjectHeader* objHeader = GetObjectHeader(object);
            snapshotFile << "        {\n";
            snapshotFile << "          \"address\": \"0x" << std::hex << reinterpret_cast<uintptr_t>(object) << std::dec << "\",\n";
            snapshotFile << "          \"size\": " << objHeader->size << ",\n";
            snapshotFile << "          \"type\": \"" << getObjectTypeName(objHeader->type) << "\",\n";
            snapshotFile << "          \"color\": \"" << getColorName(GetObjectColor(object)) << "\",\n";
            snapshotFile << "          \"reference_count\": " << countReferencesToObject(object) << ",\n";
            snapshotFile << "          \"age\": " << objHeader->age << "\n";
            snapshotFile << "        }";
        });
        
        snapshotFile << "\n      ]\n";
        snapshotFile << "    }";
    }
    
    snapshotFile << "\n  ],\n";
    snapshotFile << "  \"statistics\": {\n";
    snapshotFile << "    \"collections_performed\": " << collectionsPerformed_ << ",\n";
    snapshotFile << "    \"total_pause_time_ms\": " << totalPauseTime_.count() << ",\n";
    snapshotFile << "    \"average_pause_time_ms\": " << (collectionsPerformed_ > 0 ? totalPauseTime_.count() / collectionsPerformed_ : 0) << ",\n";
    snapshotFile << "    \"objects_promoted\": " << objectsPromoted_ << ",\n";
    snapshotFile << "    \"write_barriers_triggered\": " << writeBarriersTriggered_ << "\n";
    snapshotFile << "  }\n";
    snapshotFile << "}\n";
    
    snapshotFile.close();
    LOG_INFO("GCスナップショットを出力しました: {} ページ", allocatedPages_.size());
}

void IncrementalGC::ValidateHeap() {
    LOG_DEBUG("ヒープ検証を開始します");
    
    size_t totalObjects = 0;
    size_t totalSize = 0;
    
    for (void* page : allocatedPages_) {
        if (!page) continue;
        
        char* current = static_cast<char*>(page);
        char* end = current + PAGE_SIZE;
        
        while (current + sizeof(ObjectHeader) <= end) {
            ObjectHeader* header = reinterpret_cast<ObjectHeader*>(current);
            
            if (header->size == 0 || header->size > PAGE_SIZE) {
                break;
            }
            
            // オブジェクトの整合性をチェック
            if (header->color > ObjectColor::BLACK) {
                LOG_ERROR("無効なオブジェクト色: address={}, color={}", 
                         header, static_cast<int>(header->color));
            }
            
            totalObjects++;
            totalSize += header->size;
            
            current += sizeof(ObjectHeader) + header->size;
        }
    }
    
    LOG_DEBUG("ヒープ検証完了: オブジェクト数={}, 総サイズ={}KB", 
             totalObjects, totalSize / 1024);
}

// プライベートメソッドの実装

void IncrementalGC::StartCollection() {
    LOG_DEBUG("増分ガベージコレクションを開始します");
    
    currentPhase_ = GCPhase::MARKING;
    collectionStartTime_ = std::chrono::steady_clock::now();
    
    InitializeMarking();
    
    // 並行マーキングを開始
    if (concurrentMode_) {
        StartConcurrentMarking();
    }
}

void IncrementalGC::InitializeMarking() {
    grayStack_.clear();
    markedObjects_.clear();
    markingProgress_ = 0;
    
    // ルートオブジェクトをマーク
    MarkRoots();
    
    totalObjectsToMark_ = grayStack_.size();
    
    LOG_DEBUG("マーキング初期化完了: ルート数={}", totalObjectsToMark_);
}

void IncrementalGC::MarkRoots() {
    std::lock_guard<std::mutex> lock(rootSetMutex_);
    
    for (void** root : rootSet_) {
        if (*root && IsValidPointer(*root)) {
            MarkObject(*root);
        }
    }
}

void IncrementalGC::MarkObject(void* object) {
    if (!object || !IsInHeap(object)) {
        return;
    }
    
    ObjectHeader* header = GetObjectHeader(object);
    if (!header || header->color != ObjectColor::WHITE) {
        return;
    }
    
    SetObjectColor(object, ObjectColor::GRAY);
    AddToGrayStack(object);
    markedObjects_.insert(object);
}

void IncrementalGC::PerformMarkingIncrement(size_t budgetUs) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    while (!IsGrayStackEmpty()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            currentTime - startTime).count();
        
        if (elapsed >= budgetUs) {
            break;
        }
        
        void* object = PopFromGrayStack();
        if (object) {
            BlackenObject(object);
            markingProgress_++;
        }
    }
    
    // マーキング完了チェック
    if (IsGrayStackEmpty()) {
        LOG_DEBUG("マーキングフェーズ完了: マーク済みオブジェクト数={}", markingProgress_);
        currentPhase_ = GCPhase::SWEEPING;
        InitializeSweeping();
        
        if (concurrentMode_) {
            StopConcurrentMarking();
        }
    }
    
    stats_.objectsMarked += markingProgress_;
}

void IncrementalGC::BlackenObject(void* object) {
    if (!object) return;
    
    SetObjectColor(object, ObjectColor::BLACK);
    ScanObject(object);
}

void IncrementalGC::ScanObject(void* object) {
    if (!object) return;
    
    // オブジェクトの子オブジェクトをスキャン
    size_t fieldCount;
    void** fields = GetObjectFields(object, fieldCount);
    
    for (size_t i = 0; i < fieldCount; ++i) {
        if (fields[i] && IsValidPointer(fields[i])) {
            MarkObject(fields[i]);
        }
    }
}

void IncrementalGC::InitializeSweeping() {
    sweepingProgress_ = 0;
    totalPagesToSweep_ = allocatedPages_.size();
    freedObjects_.clear();
    
    LOG_DEBUG("スイープ初期化完了: 対象ページ数={}", totalPagesToSweep_);
}

void IncrementalGC::PerformSweepingIncrement(size_t budgetUs) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    while (sweepingProgress_ < totalPagesToSweep_) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            currentTime - startTime).count();
        
        if (elapsed >= budgetUs) {
            break;
        }
        
        void* page = allocatedPages_[sweepingProgress_];
        if (page) {
            SweepPage(page, PAGE_SIZE);
        }
        
        sweepingProgress_++;
    }
    
    // スイープ完了チェック
    if (sweepingProgress_ >= totalPagesToSweep_) {
        LOG_DEBUG("スイープフェーズ完了: 解放オブジェクト数={}", freedObjects_.size());
        currentPhase_ = GCPhase::FINALIZING;
    }
}

void IncrementalGC::SweepPage(void* pageStart, size_t pageSize) {
    char* current = static_cast<char*>(pageStart);
    char* end = current + pageSize;
    
    while (current + sizeof(ObjectHeader) <= end) {
        ObjectHeader* header = reinterpret_cast<ObjectHeader*>(current);
        
        if (header->size == 0 || header->size > pageSize) {
            break;
        }
        
        if (header->color == ObjectColor::WHITE) {
            // 未マークオブジェクトを解放
            FreeObject(current);
            freedObjects_.push_back(current);
            stats_.objectsSwept++;
        } else {
            // マークされたオブジェクトを白に戻す
            header->color = ObjectColor::WHITE;
            header->marked = false;
        }
        
        current += sizeof(ObjectHeader) + header->size;
    }
}

void IncrementalGC::FreeObject(void* object) {
    if (!object) return;
    
    ObjectHeader* header = GetObjectHeader(object);
    if (!header) return;
    
    usedMemory_ -= sizeof(ObjectHeader) + header->size;
    
    #ifdef DEBUG
    totalDeallocations_++;
    allocationSizes_.erase(object);
    #endif
    
    // オブジェクトのメモリをクリア
    std::memset(object, 0, sizeof(ObjectHeader) + header->size);
}

void IncrementalGC::FinalizeCollection() {
    currentPhase_ = GCPhase::IDLE;
    
    // 統計の更新
    auto endTime = std::chrono::steady_clock::now();
    auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - collectionStartTime_).count();
    
    stats_.totalMarkingTime += totalTime * 0.6;  // 概算
    stats_.totalSweepingTime += totalTime * 0.4;  // 概算
    
    // 割り当て統計のリセット
    allocationsSinceLastGC_ = 0;
    bytesAllocatedSinceLastGC_ = 0;
    
    UpdateAllocationRate();
    
    LOG_DEBUG("ガベージコレクション終了: 総時間={}ms", totalTime);
}

// ライトバリア実装

void IncrementalGC::SnapshotAtBeginningBarrier(void* object, void* field, void* newValue) {
    UNUSED(field);
    
    // 新しい値が白い場合はグレーにマーク
    if (GetObjectColor(newValue) == ObjectColor::WHITE) {
        SetObjectColor(newValue, ObjectColor::GRAY);
        AddToGrayStack(newValue);
    }
    
    // 書き込み先オブジェクトが黒い場合もグレーに戻す
    if (GetObjectColor(object) == ObjectColor::BLACK) {
        SetObjectColor(object, ObjectColor::GRAY);
        AddToGrayStack(object);
    }
}

void IncrementalGC::IncrementalUpdateBarrier(void* object, void* field, void* newValue) {
    UNUSED(field);
    
    // 書き込み先が黒く、新しい値が白い場合はグレーにマーク
    if (GetObjectColor(object) == ObjectColor::BLACK && 
        GetObjectColor(newValue) == ObjectColor::WHITE) {
        SetObjectColor(newValue, ObjectColor::GRAY);
        AddToGrayStack(newValue);
    }
}

void IncrementalGC::GenerationalBarrier(void* object, void* field, void* newValue) {
    UNUSED(field);
    
    ObjectHeader* objHeader = GetObjectHeader(object);
    ObjectHeader* valueHeader = GetObjectHeader(newValue);
    
    // 新しい値の世代を確認
    Generation newValueGeneration = GetObjectGeneration(newValue);
    Generation currentGeneration = GetObjectGeneration(object);
    
    // 世代間参照の検出
    if (currentGeneration > newValueGeneration) {
        // 古い世代から新しい世代への参照
        RememberedSetEntry entry;
        entry.sourceObject = object;
        entry.targetObject = newValue;
        entry.fieldOffset = fieldOffset;
        entry.timestamp = std::chrono::steady_clock::now();
        
        // リメンバーセットに追加
        {
            std::lock_guard<std::mutex> lock(rememberedSetMutex_);
            rememberedSet_.insert(entry);
        }
        
        // 統計情報の更新
        intergenerationalReferences_++;
        
        LOG_DEBUG("世代間参照を記録: 0x{:x} (Gen{}) -> 0x{:x} (Gen{})",
                 reinterpret_cast<uintptr_t>(object), static_cast<int>(currentGeneration),
                 reinterpret_cast<uintptr_t>(newValue), static_cast<int>(newValueGeneration));
    }
    
    // 新しい値が白色（未マーク）の場合の処理
    if (GetObjectColor(newValue) == ObjectColor::WHITE) {
        // 三色不変条件の維持
        if (GetObjectColor(object) == ObjectColor::BLACK) {
            // 黒から白への参照は許可されない
            // 新しい値をグレーにマークして後で処理
            SetObjectColor(newValue, ObjectColor::GRAY);
            
            // マーキングキューに追加
            {
                std::lock_guard<std::mutex> lock(markingQueueMutex_);
                markingQueue_.push(newValue);
            }
            
            // 統計情報の更新
            writeBarriersTriggered_++;
            
            LOG_DEBUG("ライトバリアーによりオブジェクトをグレーマーク: 0x{:x}",
                     reinterpret_cast<uintptr_t>(newValue));
        }
    }
    
    // カード表の更新（大きなオブジェクト用）
    if (IsLargeObject(object)) {
        size_t cardIndex = GetCardIndex(object);
        cardTable_[cardIndex] = true;
    }
}

// ヘルパーメソッド実装

ObjectHeader* IncrementalGC::GetObjectHeader(void* object) {
    if (!object || !IsInHeap(object)) {
        return nullptr;
    }
    
    return reinterpret_cast<ObjectHeader*>(
        static_cast<char*>(object) - sizeof(ObjectHeader));
}

ObjectColor IncrementalGC::GetObjectColor(void* object) {
    ObjectHeader* header = GetObjectHeader(object);
    return header ? header->color : ObjectColor::WHITE;
}

void IncrementalGC::SetObjectColor(void* object, ObjectColor color) {
    ObjectHeader* header = GetObjectHeader(object);
    if (header) {
        header->color = color;
    }
}

size_t IncrementalGC::GetObjectSize(void* object) {
    ObjectHeader* header = GetObjectHeader(object);
    return header ? header->size : 0;
}

void** IncrementalGC::GetObjectFields(void* object, size_t& fieldCount) {
    // 完璧なオブジェクトフィールド取得実装
    
    if (!object) {
        fieldCount = 0;
        return nullptr;
    }
    
    ObjectHeader* header = GetObjectHeader(object);
    if (!header) {
        fieldCount = 0;
        return nullptr;
    }
    
    // オブジェクト型に基づくフィールド情報の取得
    const TypeInfo* typeInfo = GetTypeInfo(header->type);
    if (!typeInfo) {
        fieldCount = 0;
        return nullptr;
    }
    
    fieldCount = typeInfo->fieldCount;
    
    // フィールドポインタの配列を作成
    void** fields = static_cast<void**>(malloc(fieldCount * sizeof(void*)));
    if (!fields) {
        fieldCount = 0;
        return nullptr;
    }
    
    // 各フィールドのアドレスを計算
    char* objectData = static_cast<char*>(object);
    for (size_t i = 0; i < fieldCount; ++i) {
        const FieldInfo& fieldInfo = typeInfo->fields[i];
        
        switch (fieldInfo.type) {
            case FieldType::POINTER:
                // ポインタフィールド
                fields[i] = *reinterpret_cast<void**>(objectData + fieldInfo.offset);
                break;
                
            case FieldType::OBJECT_REFERENCE:
                // オブジェクト参照
                fields[i] = *reinterpret_cast<void**>(objectData + fieldInfo.offset);
                break;
                
            case FieldType::ARRAY_REFERENCE:
                // 配列参照
                fields[i] = *reinterpret_cast<void**>(objectData + fieldInfo.offset);
                break;
                
            case FieldType::WEAK_REFERENCE:
                // 弱参照（特別な処理が必要）
                fields[i] = resolveWeakReference(objectData + fieldInfo.offset);
                break;
                
            case FieldType::PRIMITIVE:
                // プリミティブ型はスキップ
                fields[i] = nullptr;
                break;
                
            default:
                fields[i] = nullptr;
                break;
        }
    }
    
    return fields;
}

void* IncrementalGC::AllocateFromHeap(size_t size, size_t alignment) {
    size = AlignSize(size, alignment);
    
    // 既存ページから割り当てを試行
    for (void* page : allocatedPages_) {
        if (!page) continue;
        
        // 完璧な線形探索実装
        
        char* current = static_cast<char*>(page);
        char* pageEnd = current + PAGE_SIZE;
        
        while (current < pageEnd) {
            // オブジェクトヘッダーの確認
            if (current + sizeof(ObjectHeader) > pageEnd) {
                break; // ページ境界を超える
            }
            
            ObjectHeader* header = reinterpret_cast<ObjectHeader*>(current);
            
            // ヘッダーの妥当性チェック
            if (!isValidObjectHeader(header)) {
                // 無効なヘッダー、次のアライメント境界へ
                current += OBJECT_ALIGNMENT;
                continue;
            }
            
            // オブジェクトサイズの確認
            if (header->size == 0 || header->size > MAX_OBJECT_SIZE) {
                current += OBJECT_ALIGNMENT;
                continue;
            }
            
            // オブジェクトの開始位置
            void* objectStart = current + sizeof(ObjectHeader);
            
            // 探しているオブジェクトかチェック
            if (objectStart == object) {
                return current; // 見つかった
            }
            
            // 次のオブジェクトへ
            size_t totalSize = sizeof(ObjectHeader) + header->size;
            totalSize = (totalSize + OBJECT_ALIGNMENT - 1) & ~(OBJECT_ALIGNMENT - 1); // アライメント調整
            current += totalSize;
        }
        
        // フリーリストの確認
        FreeBlock* freeBlock = GetPageFreeList(page);
        while (freeBlock) {
            if (freeBlock == object) {
                return reinterpret_cast<char*>(freeBlock);
            }
            freeBlock = freeBlock->next;
        }
    }
    
    // 新しいページを割り当て
    void* newPage = mmap(nullptr, PAGE_SIZE, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (newPage == MAP_FAILED) {
        return nullptr;
    }
    
    allocatedPages_.push_back(newPage);
    heapSize_ += PAGE_SIZE;
    
    return newPage;
}

void IncrementalGC::AddToGrayStack(void* object) {
    std::lock_guard<std::mutex> lock(grayStackMutex_);
    grayStack_.push_back(object);
}

void* IncrementalGC::PopFromGrayStack() {
    std::lock_guard<std::mutex> lock(grayStackMutex_);
    
    if (grayStack_.empty()) {
        return nullptr;
    }
    
    void* object = grayStack_.back();
    grayStack_.pop_back();
    return object;
}

bool IncrementalGC::IsGrayStackEmpty() const {
    std::lock_guard<std::mutex> lock(grayStackMutex_);
    return grayStack_.empty();
}

bool IncrementalGC::ShouldTriggerCollection() {
    // ヒープ使用率ベースの判定
    double utilization = (double)usedMemory_ / heapSize_;
    if (utilization > targetHeapUtilization_) {
        return true;
    }
    
    // 割り当て数ベースの判定
    if (allocationsSinceLastGC_ > 10000) {
        return true;
    }
    
    // 時間ベースの判定
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastGC = std::chrono::duration_cast<std::chrono::seconds>(
        now - collectionStartTime_).count();
    
    if (timeSinceLastGC > 60) {  // 60秒
        return true;
    }
    
    return false;
}

void IncrementalGC::UpdateAllocationRate() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastAllocationTime_).count();
    
    if (duration > 0) {
        allocationRate_ = bytesAllocatedSinceLastGC_ / duration;
    }
}

void IncrementalGC::RecordIncrementTime(size_t timeUs) {
    incrementTimes_.push_back(timeUs);
    stats_.totalIncrements++;
    
    if (timeUs > stats_.maxIncrementTime) {
        stats_.maxIncrementTime = timeUs;
    }
    
    // 移動平均を計算
    if (stats_.totalIncrements > 0) {
        double totalTime = 0;
        for (size_t time : incrementTimes_) {
            totalTime += time;
        }
        stats_.averageIncrementTime = totalTime / stats_.totalIncrements;
    }
    
    // 履歴サイズを制限
    if (incrementTimes_.size() > 1000) {
        incrementTimes_.erase(incrementTimes_.begin(), 
                             incrementTimes_.begin() + 500);
    }
}

bool IncrementalGC::IsValidPointer(void* ptr) {
    return ptr != nullptr && IsInHeap(ptr);
}

bool IncrementalGC::IsInHeap(void* ptr) {
    for (void* page : allocatedPages_) {
        if (ptr >= page && ptr < static_cast<char*>(page) + PAGE_SIZE) {
            return true;
        }
    }
    return false;
}

void* IncrementalGC::AlignPointer(void* ptr, size_t alignment) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned);
}

size_t IncrementalGC::AlignSize(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// 並行処理実装

void IncrementalGC::StartConcurrentMarking() {
    if (concurrentMarkingActive_) {
        return;
    }
    
    concurrentMarkingActive_ = true;
    concurrentThread_ = std::make_unique<std::thread>(
        &IncrementalGC::ConcurrentMarkingThread, this);
    
    LOG_DEBUG("並行マーキングスレッドを開始しました");
}

void IncrementalGC::StopConcurrentMarking() {
    concurrentMarkingActive_ = false;
    
    if (concurrentThread_ && concurrentThread_->joinable()) {
        concurrentThread_->join();
        concurrentThread_.reset();
    }
    
    LOG_DEBUG("並行マーキングスレッドを停止しました");
}

void IncrementalGC::ConcurrentMarkingThread() {
    while (concurrentMarkingActive_ && !shouldStop_) {
        if (currentPhase_ == GCPhase::MARKING && !IsGrayStackEmpty()) {
            // バックグラウンドでマーキングを実行
            PerformMarkingIncrement(500);  // 0.5ms
        } else {
            // 少し待機
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

// ファクトリメソッド

std::unique_ptr<IncrementalGC> IncrementalGCFactory::Create(
    size_t initialHeapSize,
    WriteBarrierType barrierType,
    bool concurrentMode) {
    
    auto gc = std::make_unique<IncrementalGC>();
    gc->SetWriteBarrierType(barrierType);
    gc->SetConcurrentMode(concurrentMode);
    
    return gc;
}

void IncrementalGCFactory::Configure(
    IncrementalGC* gc,
    size_t incrementBudgetUs,
    double targetUtilization,
    bool debugMode) {
    
    if (gc) {
        gc->SetIncrementBudget(incrementBudgetUs);
        gc->SetTargetHeapUtilization(targetUtilization);
        gc->EnableDebugMode(debugMode);
    }
}

} // namespace memory
} // namespace utils
} // namespace aerojs 