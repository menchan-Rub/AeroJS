/**
 * @file wasm_global.cpp
 * @brief WebAssemblyグローバル変数の完璧な実装
 * @version 1.0.0
 * @license MIT
 */

#include "wasm_global.h"
#include <algorithm>
#include <stdexcept>
#include <cassert>
#include <cmath>
#include <limits>

namespace aerojs {
namespace core {
namespace runtime {
namespace wasm {

StandardWasmGlobal::StandardWasmGlobal(WasmValueType type, bool mutable_, const WasmValue& initialValue)
    : m_type(type)
    , m_mutable(mutable_)
    , m_value(initialValue)
    , m_initialValue(initialValue)
    , m_readOperations(0)
    , m_writeOperations(0)
    , m_atomicOperations(0) {
    
    // 型と値の互換性チェック
    if (!isTypeCompatible(initialValue)) {
        throw std::invalid_argument("Initial value type does not match global variable type");
    }
}

StandardWasmGlobal::~StandardWasmGlobal() {
    // 自動的にクリーンアップされる
}

WasmValue StandardWasmGlobal::getValue() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_readOperations.fetch_add(1, std::memory_order_relaxed);
    return m_value;
}

void StandardWasmGlobal::setValue(const WasmValue& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 可変性チェック
    if (!m_mutable) {
        throw std::runtime_error("Cannot modify immutable global variable");
    }
    
    // 型互換性チェック
    if (!isTypeCompatible(value)) {
        throw std::invalid_argument("Value type does not match global variable type");
    }
    
    m_value = value;
    m_writeOperations.fetch_add(1, std::memory_order_relaxed);
}

bool StandardWasmGlobal::isMutable() const {
    return m_mutable;
}

WasmValueType StandardWasmGlobal::getType() const {
    return m_type;
}

bool StandardWasmGlobal::validateType(WasmValueType type, const WasmValue& value) const {
    return type == m_type && isTypeCompatible(value);
}

void StandardWasmGlobal::setMutable(bool mutable_) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mutable = mutable_;
}

void StandardWasmGlobal::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_mutable) {
        throw std::runtime_error("Cannot reset immutable global variable");
    }
    
    m_value = m_initialValue;
    m_writeOperations.fetch_add(1, std::memory_order_relaxed);
}

Value StandardWasmGlobal::toJSValue(ExecutionContext* context) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_value.toJSValue(context);
}

bool StandardWasmGlobal::fromJSValue(const Value& value, ExecutionContext* context) {
    if (!m_mutable) {
        return false;
    }
    
    try {
        WasmValue wasmValue = WasmValue::fromJSValue(value, m_type);
        setValue(wasmValue);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool StandardWasmGlobal::compareExchange(const WasmValue& expected, const WasmValue& desired) {
    if (!m_mutable || !supportsAtomicOperations()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_atomicOperations.fetch_add(1, std::memory_order_relaxed);
    
    // 期待値と現在値を比較
    bool valuesEqual = false;
    switch (m_type) {
        case WasmValueType::I32:
            valuesEqual = (m_value.i32Value == expected.i32Value);
            break;
        case WasmValueType::I64:
            valuesEqual = (m_value.i64Value == expected.i64Value);
            break;
        case WasmValueType::F32:
            valuesEqual = (m_value.f32Value == expected.f32Value);
            break;
        case WasmValueType::F64:
            valuesEqual = (m_value.f64Value == expected.f64Value);
            break;
        case WasmValueType::FuncRef:
            valuesEqual = (m_value.funcRef == expected.funcRef);
            break;
        case WasmValueType::ExternRef:
            valuesEqual = (m_value.externRef == expected.externRef);
            break;
        default:
            return false;
    }
    
    if (valuesEqual && isTypeCompatible(desired)) {
        m_value = desired;
        return true;
    }
    
    return false;
}

WasmValue StandardWasmGlobal::exchange(const WasmValue& newValue) {
    if (!m_mutable || !isTypeCompatible(newValue)) {
        return createDefaultValue();
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_atomicOperations.fetch_add(1, std::memory_order_relaxed);
    
    WasmValue oldValue = m_value;
    m_value = newValue;
    return oldValue;
}

WasmValue StandardWasmGlobal::fetchAdd(const WasmValue& value) {
    if (!m_mutable || !supportsAtomicOperations()) {
        return createDefaultValue();
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_atomicOperations.fetch_add(1, std::memory_order_relaxed);
    
    return performNumericOperation(m_value, value, [](const WasmValue& a, const WasmValue& b) -> WasmValue {
        switch (a.type()) {
            case WasmValueType::I32:
                return WasmValue::createI32(a.i32Value + b.i32Value);
            case WasmValueType::I64:
                return WasmValue::createI64(a.i64Value + b.i64Value);
            case WasmValueType::F32:
                return WasmValue::createF32(a.f32Value + b.f32Value);
            case WasmValueType::F64:
                return WasmValue::createF64(a.f64Value + b.f64Value);
            default:
                return a; // 加算できない型
        }
    });
}

WasmValue StandardWasmGlobal::fetchSub(const WasmValue& value) {
    if (!m_mutable || !supportsAtomicOperations()) {
        return createDefaultValue();
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_atomicOperations.fetch_add(1, std::memory_order_relaxed);
    
    return performNumericOperation(m_value, value, [](const WasmValue& a, const WasmValue& b) -> WasmValue {
        switch (a.type()) {
            case WasmValueType::I32:
                return WasmValue::createI32(a.i32Value - b.i32Value);
            case WasmValueType::I64:
                return WasmValue::createI64(a.i64Value - b.i64Value);
            case WasmValueType::F32:
                return WasmValue::createF32(a.f32Value - b.f32Value);
            case WasmValueType::F64:
                return WasmValue::createF64(a.f64Value - b.f64Value);
            default:
                return a; // 減算できない型
        }
    });
}

StandardWasmGlobal::GlobalStats StandardWasmGlobal::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    GlobalStats stats;
    stats.type = m_type;
    stats.isMutable = m_mutable;
    stats.readOperations = m_readOperations.load(std::memory_order_relaxed);
    stats.writeOperations = m_writeOperations.load(std::memory_order_relaxed);
    stats.atomicOperations = m_atomicOperations.load(std::memory_order_relaxed);
    stats.currentValue = m_value;
    stats.initialValue = m_initialValue;
    
    return stats;
}

bool StandardWasmGlobal::isInRange(const WasmValue& value) const {
    switch (m_type) {
        case WasmValueType::I32:
            return value.type() == WasmValueType::I32 &&
                   value.i32Value >= std::numeric_limits<int32_t>::min() &&
                   value.i32Value <= std::numeric_limits<int32_t>::max();
        case WasmValueType::I64:
            return value.type() == WasmValueType::I64 &&
                   value.i64Value >= std::numeric_limits<int64_t>::min() &&
                   value.i64Value <= std::numeric_limits<int64_t>::max();
        case WasmValueType::F32:
            return value.type() == WasmValueType::F32 &&
                   std::isfinite(value.f32Value);
        case WasmValueType::F64:
            return value.type() == WasmValueType::F64 &&
                   std::isfinite(value.f64Value);
        case WasmValueType::FuncRef:
            return value.type() == WasmValueType::FuncRef;
        case WasmValueType::ExternRef:
            return value.type() == WasmValueType::ExternRef;
        default:
            return false;
    }
}

std::unique_ptr<StandardWasmGlobal> StandardWasmGlobal::clone() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::make_unique<StandardWasmGlobal>(m_type, m_mutable, m_value);
}

bool StandardWasmGlobal::isTypeCompatible(const WasmValue& value) const {
    return value.type() == m_type;
}

bool StandardWasmGlobal::supportsAtomicOperations() const {
    // 数値型のみアトミック操作をサポート
    switch (m_type) {
        case WasmValueType::I32:
        case WasmValueType::I64:
        case WasmValueType::F32:
        case WasmValueType::F64:
            return true;
        default:
            return false;
    }
}

WasmValue StandardWasmGlobal::performNumericOperation(const WasmValue& current, const WasmValue& operand, 
                                                     std::function<WasmValue(const WasmValue&, const WasmValue&)> operation) const {
    if (!isTypeCompatible(operand)) {
        return current; // 型が合わない場合は元の値を返す
    }
    
    WasmValue oldValue = current;
    WasmValue newValue = operation(current, operand);
    
    // 範囲チェック
    if (isInRange(newValue)) {
        const_cast<StandardWasmGlobal*>(this)->m_value = newValue;
    }
    
    return oldValue;
}

WasmValue StandardWasmGlobal::createDefaultValue() const {
    switch (m_type) {
        case WasmValueType::I32:
            return WasmValue::createI32(0);
        case WasmValueType::I64:
            return WasmValue::createI64(0);
        case WasmValueType::F32:
            return WasmValue::createF32(0.0f);
        case WasmValueType::F64:
            return WasmValue::createF64(0.0);
        case WasmValueType::FuncRef:
            return WasmValue::createFuncRef(INVALID_FUNC_REF);
        case WasmValueType::ExternRef:
            return WasmValue::createExternRef(nullptr);
        default:
            return WasmValue::createI32(0);
    }
}

// -------------- WasmGlobalFactory 実装 --------------

std::unique_ptr<StandardWasmGlobal> WasmGlobalFactory::createI32Global(int32_t initialValue, bool mutable_) {
    WasmValue value = WasmValue::createI32(initialValue);
    return std::make_unique<StandardWasmGlobal>(WasmValueType::I32, mutable_, value);
}

std::unique_ptr<StandardWasmGlobal> WasmGlobalFactory::createI64Global(int64_t initialValue, bool mutable_) {
    WasmValue value = WasmValue::createI64(initialValue);
    return std::make_unique<StandardWasmGlobal>(WasmValueType::I64, mutable_, value);
}

std::unique_ptr<StandardWasmGlobal> WasmGlobalFactory::createF32Global(float initialValue, bool mutable_) {
    WasmValue value = WasmValue::createF32(initialValue);
    return std::make_unique<StandardWasmGlobal>(WasmValueType::F32, mutable_, value);
}

std::unique_ptr<StandardWasmGlobal> WasmGlobalFactory::createF64Global(double initialValue, bool mutable_) {
    WasmValue value = WasmValue::createF64(initialValue);
    return std::make_unique<StandardWasmGlobal>(WasmValueType::F64, mutable_, value);
}

std::unique_ptr<StandardWasmGlobal> WasmGlobalFactory::createFuncRefGlobal(uint32_t initialValue, bool mutable_) {
    WasmValue value = WasmValue::createFuncRef(initialValue);
    return std::make_unique<StandardWasmGlobal>(WasmValueType::FuncRef, mutable_, value);
}

std::unique_ptr<StandardWasmGlobal> WasmGlobalFactory::createExternRefGlobal(void* initialValue, bool mutable_) {
    WasmValue value = WasmValue::createExternRef(initialValue);
    return std::make_unique<StandardWasmGlobal>(WasmValueType::ExternRef, mutable_, value);
}

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs 