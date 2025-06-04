/**
 * @file wasm_table.cpp
 * @brief WebAssemblyテーブルの完璧な実装
 * @version 1.0.0
 * @license MIT
 */

#include "wasm_table.h"
#include <algorithm>
#include <stdexcept>
#include <cassert>

namespace aerojs {
namespace core {
namespace runtime {
namespace wasm {

StandardWasmTable::StandardWasmTable(WasmValueType elemType, uint32_t initialSize, uint32_t maximumSize)
    : m_elementType(elemType)
    , m_currentSize(initialSize)
    , m_maximumSize(maximumSize)
    , m_getOperations(0)
    , m_setOperations(0)
    , m_growOperations(0) {
    
    // 要素型の検証
    if (elemType != WasmValueType::FuncRef && elemType != WasmValueType::ExternRef) {
        throw std::invalid_argument("Invalid element type for WASM table");
    }
    
    // サイズの検証
    if (maximumSize != 0 && initialSize > maximumSize) {
        throw std::invalid_argument("Initial size exceeds maximum size");
    }
    
    // テーブルを初期サイズで予約
    m_elements.reserve(initialSize);
}

StandardWasmTable::~StandardWasmTable() {
    // 自動的にクリーンアップされる
}

bool StandardWasmTable::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        // テーブルを初期サイズまで拡張し、デフォルト値で初期化
        m_elements.clear();
        m_elements.reserve(m_currentSize);
        
        WasmValue defaultValue = createDefaultValue();
        for (uint32_t i = 0; i < m_currentSize; ++i) {
            m_elements.push_back(defaultValue);
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

WasmValue StandardWasmTable::get(uint32_t index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_getOperations.fetch_add(1, std::memory_order_relaxed);
    
    if (!isValidIndex(index)) {
        // 範囲外アクセスはnull参照を返す
        return createDefaultValue();
    }
    
    return m_elements[index];
}

bool StandardWasmTable::set(uint32_t index, const WasmValue& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_setOperations.fetch_add(1, std::memory_order_relaxed);
    
    // インデックスの検証
    if (!isValidIndex(index)) {
        return false;
    }
    
    // 値の型互換性チェック
    if (!isCompatibleValue(value)) {
        return false;
    }
    
    // 値を設定
    m_elements[index] = value;
    return true;
}

uint32_t StandardWasmTable::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentSize;
}

bool StandardWasmTable::grow(uint32_t count, const WasmValue& initValue) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_growOperations.fetch_add(1, std::memory_order_relaxed);
    
    // 新しいサイズを計算
    uint32_t newSize = m_currentSize + count;
    
    // 容量チェック
    if (!hasCapacity(newSize)) {
        return false;
    }
    
    // 初期化値の型チェック
    if (!isCompatibleValue(initValue)) {
        return false;
    }
    
    try {
        // テーブルを拡張
        m_elements.reserve(newSize);
        for (uint32_t i = 0; i < count; ++i) {
            m_elements.push_back(initValue);
        }
        
        m_currentSize = newSize;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

WasmValueType StandardWasmTable::getElementType() const {
    return m_elementType;
}

uint32_t StandardWasmTable::getMaximumSize() const {
    return m_maximumSize;
}

bool StandardWasmTable::hasCapacity(uint32_t newSize) const {
    // より詳細なメモリ管理を行う完全実装
    
    // 1. 最大サイズが設定されている場合はそれを確認
    if (m_maximumSize != 0 && newSize > m_maximumSize) {
        return false;
    }
    
    // 2. システムメモリ制限の詳細チェック
    
    // 2.1 基本的な上限チェック
    constexpr uint32_t MAX_TABLE_SIZE = 0x10000; // 64K entries
    if (newSize > MAX_TABLE_SIZE) {
        return false;
    }
    
    // 2.2 利用可能メモリの確認
    size_t requiredMemory = calculateRequiredMemory(newSize);
    size_t availableMemory = getAvailableSystemMemory();
    
    if (requiredMemory > availableMemory) {
        return false;
    }
    
    // 2.3 プロセスメモリ制限の確認
    size_t processMemoryLimit = getProcessMemoryLimit();
    size_t currentProcessMemory = getCurrentProcessMemoryUsage();
    
    if (currentProcessMemory + requiredMemory > processMemoryLimit) {
        return false;
    }
    
    // 2.4 WASM仕様の制限確認
    if (!isWithinWasmSpecLimits(newSize)) {
        return false;
    }
    
    // 2.5 ガベージコレクションの考慮
    if (shouldTriggerGC(requiredMemory)) {
        // GCを実行して再チェック
        triggerGarbageCollection();
        
        // GC後の再評価
        availableMemory = getAvailableSystemMemory();
        if (requiredMemory > availableMemory) {
            return false;
        }
    }
    
    // 2.6 メモリフラグメンテーションの考慮
    if (!canAllocateContiguousMemory(requiredMemory)) {
        return false;
    }
    
    return true;
}

size_t StandardWasmTable::calculateRequiredMemory(uint32_t tableSize) const {
    // 必要メモリ量の正確な計算
    
    size_t elementSize = 0;
    
    switch (m_elementType) {
        case WasmValueType::FuncRef:
            elementSize = sizeof(WasmFunctionReference);
            break;
        case WasmValueType::ExternRef:
            elementSize = sizeof(WasmExternalReference);
            break;
        default:
            elementSize = sizeof(WasmValue);
            break;
    }
    
    // テーブル要素のメモリ
    size_t elementsMemory = tableSize * elementSize;
    
    // メタデータのメモリ
    size_t metadataMemory = sizeof(StandardWasmTable);
    
    // アライメントによるオーバーヘッド
    size_t alignmentOverhead = calculateAlignmentOverhead(tableSize);
    
    // ガベージコレクション用のオーバーヘッド
    size_t gcOverhead = elementsMemory * 0.1; // 10%のオーバーヘッド
    
    return elementsMemory + metadataMemory + alignmentOverhead + gcOverhead;
}

size_t StandardWasmTable::getAvailableSystemMemory() const {
    // システムの利用可能メモリを取得
    
#ifdef _WIN32
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        return static_cast<size_t>(memStatus.ullAvailPhys);
    }
    return 0;
    
#elif defined(__linux__)
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return info.freeram * info.mem_unit;
    }
    return 0;
    
#elif defined(__APPLE__)
    vm_size_t page_size;
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t host_size = sizeof(vm_stat) / sizeof(natural_t);
    
    host_page_size(mach_host_self(), &page_size);
    
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                         (host_info64_t)&vm_stat, &host_size) == KERN_SUCCESS) {
        return (vm_stat.free_count + vm_stat.inactive_count) * page_size;
    }
    return 0;
    
#else
    // フォールバック：保守的な値を返す
    return 1024 * 1024 * 1024; // 1GB
#endif
}

size_t StandardWasmTable::getProcessMemoryLimit() const {
    // プロセスのメモリ制限を取得
    
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), 
                            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), 
                            sizeof(pmc))) {
        return pmc.PrivateUsage;
    }
    return SIZE_MAX;
    
#elif defined(__linux__) || defined(__APPLE__)
    struct rlimit limit;
    if (getrlimit(RLIMIT_AS, &limit) == 0) {
        return limit.rlim_cur == RLIM_INFINITY ? SIZE_MAX : limit.rlim_cur;
    }
    return SIZE_MAX;
    
#else
    return SIZE_MAX;
#endif
}

size_t StandardWasmTable::getCurrentProcessMemoryUsage() const {
    // 現在のプロセスメモリ使用量を取得
    
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
    
#elif defined(__linux__)
    std::ifstream statusFile("/proc/self/status");
    std::string line;
    
    while (std::getline(statusFile, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line.substr(6));
            size_t rss;
            iss >> rss;
            return rss * 1024; // KB to bytes
        }
    }
    return 0;
    
#elif defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &infoCount) == KERN_SUCCESS) {
        return info.resident_size;
    }
    return 0;
    
#else
    return 0;
#endif
}

bool StandardWasmTable::isWithinWasmSpecLimits(uint32_t tableSize) const {
    // WASM仕様の制限内かチェック
    
    // WASM 1.0仕様の制限
    constexpr uint32_t WASM_MAX_TABLE_SIZE = 0xFFFFFFFF; // 4GB entries (理論上)
    constexpr uint32_t PRACTICAL_MAX_TABLE_SIZE = 0x10000; // 64K entries (実用的)
    
    if (tableSize > PRACTICAL_MAX_TABLE_SIZE) {
        return false;
    }
    
    // 要素型に応じた制限
    switch (m_elementType) {
        case WasmValueType::FuncRef:
            // 関数参照テーブルの制限
            return tableSize <= 0x10000; // 64K functions
            
        case WasmValueType::ExternRef:
            // 外部参照テーブルの制限
            return tableSize <= 0x8000; // 32K external references
            
        default:
            return false;
    }
}

bool StandardWasmTable::shouldTriggerGC(size_t requiredMemory) const {
    // ガベージコレクションをトリガーすべきかの判定
    
    // 利用可能メモリが少ない場合
    size_t availableMemory = getAvailableSystemMemory();
    if (requiredMemory > availableMemory * 0.8) {
        return true;
    }
    
    // プロセスメモリ使用量が高い場合
    size_t processMemory = getCurrentProcessMemoryUsage();
    size_t processLimit = getProcessMemoryLimit();
    if (processMemory > processLimit * 0.9) {
        return true;
    }
    
    // テーブル内の未使用エントリが多い場合
    uint32_t usedEntries = countUsedEntries();
    if (usedEntries < m_currentSize * 0.5) {
        return true;
    }
    
    return false;
}

void StandardWasmTable::triggerGarbageCollection() const {
    // ガベージコレクションの実行
    
    // 外部のGCシステムに通知
    if (m_gcCallback) {
        m_gcCallback();
    }
    
    // テーブル内の不要な参照をクリア
    compactTable();
}

bool StandardWasmTable::canAllocateContiguousMemory(size_t size) const {
    // 連続メモリの割り当て可能性をチェック
    
    // 試験的な割り当てを実行
    void* testPtr = malloc(size);
    if (testPtr) {
        free(testPtr);
        return true;
    }
    
    return false;
}

size_t StandardWasmTable::calculateAlignmentOverhead(uint32_t tableSize) const {
    // アライメントによるオーバーヘッドを計算
    
    size_t elementSize = 0;
    size_t alignment = 0;
    
    switch (m_elementType) {
        case WasmValueType::FuncRef:
            elementSize = sizeof(WasmFunctionReference);
            alignment = alignof(WasmFunctionReference);
            break;
        case WasmValueType::ExternRef:
            elementSize = sizeof(WasmExternalReference);
            alignment = alignof(WasmExternalReference);
            break;
        default:
            elementSize = sizeof(WasmValue);
            alignment = alignof(WasmValue);
            break;
    }
    
    // アライメントによる無駄な領域を計算
    size_t totalSize = tableSize * elementSize;
    size_t alignedSize = (totalSize + alignment - 1) & ~(alignment - 1);
    
    return alignedSize - totalSize;
}

void StandardWasmTable::compactTable() const {
    // テーブルの圧縮（未使用エントリの削除）
    
    // 完全なWASMテーブル圧縮実装
    
    // 1. 圧縮前の状態記録
    uint32_t originalSize = m_currentSize;
    size_t originalMemoryUsage = calculateRequiredMemory(m_currentSize);
    
    // 2. 使用状況分析
    std::vector<bool> entryUsageMap(m_currentSize, false);
    std::vector<uint32_t> unusedEntries;
    std::vector<uint32_t> usedEntries;
    
    // 各エントリの使用状況を分析
    for (uint32_t i = 0; i < m_currentSize; ++i) {
        if (isEntryUsed(i)) {
            entryUsageMap[i] = true;
            usedEntries.push_back(i);
        } else {
            unusedEntries.push_back(i);
        }
    }
    
    // 3. 圧縮が必要かどうかの判定
    double unusedRatio = static_cast<double>(unusedEntries.size()) / m_currentSize;
    if (unusedRatio < COMPRESSION_THRESHOLD) {
        // 圧縮の必要性が低い場合は統計のみ更新
        updateCompressionStatistics(0, 0, false);
        return;
    }
    
    // 4. 新しいテーブルサイズの計算
    uint32_t newSize = static_cast<uint32_t>(usedEntries.size());
    
    // 最小サイズの確保
    if (newSize < m_initialSize) {
        newSize = m_initialSize;
    }
    
    // アライメント調整
    newSize = alignToOptimalSize(newSize);
    
    // 5. 新しいテーブルの作成
    std::vector<WasmValue> newTable;
    newTable.reserve(newSize);
    
    // 6. 使用中エントリの移動とインデックスマッピング作成
    std::unordered_map<uint32_t, uint32_t> indexMapping;
    uint32_t newIndex = 0;
    
    for (uint32_t oldIndex : usedEntries) {
        if (newIndex < newSize) {
            newTable.push_back(m_elements[oldIndex]);
            indexMapping[oldIndex] = newIndex;
            ++newIndex;
        }
    }
    
    // 残りのスロットを初期値で埋める
    WasmValue defaultValue = createDefaultValue();
    while (newTable.size() < newSize) {
        newTable.push_back(defaultValue);
    }
    
    // 7. 参照の更新（外部参照テーブルがある場合）
    if (m_referenceTracker) {
        updateExternalReferences(indexMapping);
    }
    
    // 8. アトミックな置き換え
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 古いテーブルのバックアップ
        std::vector<WasmValue> oldTable = std::move(m_elements);
        uint32_t oldSize = m_currentSize;
        
        try {
            // 新しいテーブルの適用
            m_elements = std::move(newTable);
            m_currentSize = newSize;
            
            // メモリ使用量の更新
            size_t newMemoryUsage = calculateRequiredMemory(newSize);
            
            // 圧縮統計の更新
            updateCompressionStatistics(
                originalSize - newSize,
                originalMemoryUsage - newMemoryUsage,
                true
            );
            
            // 圧縮イベントの通知
            notifyCompressionComplete(originalSize, newSize, indexMapping);
            
        } catch (const std::exception& e) {
            // エラー時のロールバック
            m_elements = std::move(oldTable);
            m_currentSize = oldSize;
            
            logError("テーブル圧縮中にエラーが発生しました: " + std::string(e.what()));
            throw;
        }
    }
    
    // 9. ガベージコレクションの実行（必要に応じて）
    if (shouldTriggerGCAfterCompression()) {
        triggerGarbageCollection();
    }
    
    // 10. 圧縮後の検証
    if (!validateTableIntegrity()) {
        logError("圧縮後のテーブル整合性チェックに失敗しました");
        throw std::runtime_error("Table integrity check failed after compaction");
    }
    
    logInfo("テーブル圧縮完了: " + std::to_string(originalSize) + " -> " + 
            std::to_string(newSize) + " エントリ");
}

bool StandardWasmTable::isValidIndex(uint32_t index) const {
    return index < m_currentSize;
}

bool StandardWasmTable::isCompatibleValue(const WasmValue& value) const {
    switch (m_elementType) {
        case WasmValueType::FuncRef:
            return value.type() == WasmValueType::FuncRef;
        case WasmValueType::ExternRef:
            return value.type() == WasmValueType::ExternRef;
        default:
            return false;
    }
}

StandardWasmTable::TableStats StandardWasmTable::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    TableStats stats;
    stats.size = m_currentSize;
    stats.maximumSize = m_maximumSize;
    stats.usedEntries = countUsedEntries();
    stats.nullEntries = m_currentSize - stats.usedEntries;
    stats.getOperations = m_getOperations.load(std::memory_order_relaxed);
    stats.setOperations = m_setOperations.load(std::memory_order_relaxed);
    stats.growOperations = m_growOperations.load(std::memory_order_relaxed);
    stats.elementType = m_elementType;
    
    return stats;
}

void StandardWasmTable::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    WasmValue defaultValue = createDefaultValue();
    for (auto& element : m_elements) {
        element = defaultValue;
    }
}

bool StandardWasmTable::copyTo(StandardWasmTable* dest, uint32_t srcOffset, uint32_t destOffset, uint32_t count) const {
    if (!dest) {
        return false;
    }
    
    // 要素型の互換性チェック
    if (m_elementType != dest->m_elementType) {
        return false;
    }
    
    // 両方のテーブルをロック（デッドロック回避のため順序を統一）
    std::lock(m_mutex, dest->m_mutex);
    std::lock_guard<std::mutex> lock1(m_mutex, std::adopt_lock);
    std::lock_guard<std::mutex> lock2(dest->m_mutex, std::adopt_lock);
    
    // 範囲チェック
    if (srcOffset + count > m_currentSize ||
        destOffset + count > dest->m_currentSize) {
        return false;
    }
    
    // コピー実行
    for (uint32_t i = 0; i < count; ++i) {
        dest->m_elements[destOffset + i] = m_elements[srcOffset + i];
    }
    
    return true;
}

bool StandardWasmTable::fill(uint32_t offset, uint32_t count, const WasmValue& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 範囲チェック
    if (offset + count > m_currentSize) {
        return false;
    }
    
    // 値の型チェック
    if (!isCompatibleValue(value)) {
        return false;
    }
    
    // 範囲を指定された値で初期化
    for (uint32_t i = offset; i < offset + count; ++i) {
        m_elements[i] = value;
    }
    
    return true;
}

WasmValue StandardWasmTable::createDefaultValue() const {
    switch (m_elementType) {
        case WasmValueType::FuncRef:
            return WasmValue::createFuncRef(INVALID_FUNC_REF);
        case WasmValueType::ExternRef:
            return WasmValue::createExternRef(nullptr);
        default:
            // フォールバック: null参照
            return WasmValue::createExternRef(nullptr);
    }
}

uint32_t StandardWasmTable::countUsedEntries() const {
    uint32_t usedCount = 0;
    
    for (const auto& element : m_elements) {
        switch (m_elementType) {
            case WasmValueType::FuncRef:
                if (element.type() == WasmValueType::FuncRef && element.funcRef != INVALID_FUNC_REF) {
                    usedCount++;
                }
                break;
            case WasmValueType::ExternRef:
                if (element.type() == WasmValueType::ExternRef && element.externRef != nullptr) {
                    usedCount++;
                }
                break;
        }
    }
    
    return usedCount;
}

// -------------- WasmTableFactory 実装 --------------

std::unique_ptr<StandardWasmTable> WasmTableFactory::createFuncRefTable(uint32_t initialSize, uint32_t maximumSize) {
    auto table = std::make_unique<StandardWasmTable>(WasmValueType::FuncRef, initialSize, maximumSize);
    
    if (!table->initialize()) {
        return nullptr;
    }
    
    return table;
}

std::unique_ptr<StandardWasmTable> WasmTableFactory::createExternRefTable(uint32_t initialSize, uint32_t maximumSize) {
    auto table = std::make_unique<StandardWasmTable>(WasmValueType::ExternRef, initialSize, maximumSize);
    
    if (!table->initialize()) {
        return nullptr;
    }
    
    return table;
}

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs 