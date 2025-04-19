/**
 * @file constant_pool.cpp
 * @brief 定数プールの実装ファイル
 * 
 * このファイルは、バイトコードモジュールで使用される定数プールクラスを実装します。
 * 定数プールは文字列、数値、真偽値などのリテラル値を管理します。
 */

#include "constant_pool.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <limits>

namespace aerojs {
namespace core {

ConstantPool::ConstantPool(bool enable_deduplication)
    : m_enableDeduplication(enable_deduplication) {
}

ConstantPool::~ConstantPool() {
    // 特に何もしない、スマートポインタが自動的にメモリを解放
}

uint32_t ConstantPool::addConstant(const Value& value) {
    // 重複排除が有効な場合は、既存の同じ値を検索
    if (m_enableDeduplication) {
        uint32_t existing_index = findDuplicateConstant(value);
        if (existing_index != std::numeric_limits<uint32_t>::max()) {
            return existing_index;
        }
    }
    
    // 新しい定数を追加
    uint32_t index = static_cast<uint32_t>(m_constants.size());
    m_constants.push_back(value);
    
    // 重複排除が有効な場合は、マップにも追加
    if (m_enableDeduplication) {
        m_map[value] = index;
    }
    
    return index;
}

uint32_t ConstantPool::addString(const std::string& str) {
    Value value;
    value.setString(str);
    return addConstant(value);
}

uint32_t ConstantPool::addNumber(double number) {
    Value value;
    value.setNumber(number);
    return addConstant(value);
}

uint32_t ConstantPool::addInteger(int32_t integer) {
    Value value;
    value.setNumber(static_cast<double>(integer));
    return addConstant(value);
}

uint32_t ConstantPool::addBigInt(const std::string& bigint) {
    Value value;
    value.setBigInt(bigint);
    return addConstant(value);
}

uint32_t ConstantPool::addBoolean(bool boolean) {
    Value value;
    value.setBoolean(boolean);
    return addConstant(value);
}

uint32_t ConstantPool::addNull() {
    Value value;
    value.setNull();
    return addConstant(value);
}

uint32_t ConstantPool::addUndefined() {
    Value value;
    value.setUndefined();
    return addConstant(value);
}

uint32_t ConstantPool::addRegExp(const std::string& pattern, const std::string& flags) {
    Value value;
    value.setRegExp(pattern, flags);
    return addConstant(value);
}

uint32_t ConstantPool::addFunction(uint32_t function_index) {
    Value value;
    value.setFunction(function_index);
    return addConstant(value);
}

const Value& ConstantPool::getConstant(uint32_t index) const {
    if (index >= m_constants.size()) {
        throw std::out_of_range("Constant index out of range");
    }
    return m_constants[index];
}

uint32_t ConstantPool::findConstant(const Value& value) const {
    // 重複排除マップから検索
    if (m_enableDeduplication) {
        auto it = m_map.find(value);
        if (it != m_map.end()) {
            return it->second;
        }
    } else {
        // 重複排除が無効の場合は線形検索
        auto it = std::find(m_constants.begin(), m_constants.end(), value);
        if (it != m_constants.end()) {
            return static_cast<uint32_t>(std::distance(m_constants.begin(), it));
        }
    }
    
    return std::numeric_limits<uint32_t>::max();
}

const std::vector<Value>& ConstantPool::getConstants() const {
    return m_constants;
}

size_t ConstantPool::size() const {
    return m_constants.size();
}

std::vector<uint8_t> ConstantPool::serialize() const {
    // 単純なバイナリ形式でシリアライズする
    // 実際の実装では、より効率的なフォーマットを使用するべき
    
    std::vector<uint8_t> data;
    
    // ここにシリアライズのロジックを実装...
    
    return data;
}

std::unique_ptr<ConstantPool> ConstantPool::deserialize(const std::vector<uint8_t>& data) {
    // バイナリデータから定数プールを再構築する
    
    // ここでデシリアライズのロジックを実装...
    
    return std::make_unique<ConstantPool>();
}

void ConstantPool::dump(std::ostream& output) const {
    for (size_t i = 0; i < m_constants.size(); ++i) {
        const auto& value = m_constants[i];
        output << std::setw(4) << i << ": " << value.toString() << std::endl;
    }
}

uint32_t ConstantPool::findDuplicateConstant(const Value& value) const {
    if (!m_enableDeduplication) {
        return std::numeric_limits<uint32_t>::max();
    }
    
    auto it = m_map.find(value);
    if (it != m_map.end()) {
        return it->second;
    }
    
    return std::numeric_limits<uint32_t>::max();
}

} // namespace core
} // namespace aerojs 