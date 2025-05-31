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
    // 最大サイズが設定されている場合はそれを確認
    if (m_maximumSize != 0 && newSize > m_maximumSize) {
        return false;
    }
    
    // システムメモリ制限の簡易チェック
    // 実際の実装ではより詳細なメモリ管理が必要
    constexpr uint32_t MAX_TABLE_SIZE = 0x10000; // 64K entries
    return newSize <= MAX_TABLE_SIZE;
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