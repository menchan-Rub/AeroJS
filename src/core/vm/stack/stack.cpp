/**
 * @file stack.cpp
 * @brief 仮想マシンのスタック実装
 * 
 * このファイルはJavaScriptエンジンの仮想マシンで使用されるスタッククラスの実装を提供します。
 */

#include "stack.h"
#include <sstream>

namespace aerojs {
namespace core {

Stack::Stack()
    : Stack(kDefaultInitialCapacity) {
}

Stack::Stack(size_t initialCapacity)
    : m_maxCapacity(kDefaultMaxCapacity) {
    // スタックが無限に拡大するのを防ぐため、初期容量を確保
    m_values.reserve(initialCapacity);
}

Stack::~Stack() {
    // スマートポインタで管理しているので特別な後処理は不要
    clear();
}

void Stack::push(ValuePtr value) {
    // スタックオーバーフローをチェック
    if (m_values.size() >= m_maxCapacity) {
        throw std::runtime_error("スタックオーバーフロー");
    }
    
    // 値をスタックにプッシュ
    m_values.push_back(value);
}

ValuePtr Stack::pop() {
    if (isEmpty()) {
        throw std::runtime_error("空のスタックからポップしようとしました");
    }
    
    // 最上位の値を取得
    ValuePtr value = m_values.back();
    
    // スタックから削除
    m_values.pop_back();
    
    return value;
}

ValuePtr Stack::peek() const {
    if (isEmpty()) {
        throw std::runtime_error("空のスタックをピークしようとしました");
    }
    
    return m_values.back();
}

ValuePtr Stack::peekAt(size_t index) const {
    if (index >= m_values.size()) {
        throw std::out_of_range("スタックインデックスが範囲外です");
    }
    
    // スタックの上からのインデックスに変換
    size_t actualIndex = m_values.size() - 1 - index;
    return m_values[actualIndex];
}

void Stack::setAt(size_t index, ValuePtr value) {
    if (index >= m_values.size()) {
        throw std::out_of_range("スタックインデックスが範囲外です");
    }
    
    // スタックの上からのインデックスに変換
    size_t actualIndex = m_values.size() - 1 - index;
    m_values[actualIndex] = value;
}

size_t Stack::size() const {
    return m_values.size();
}

bool Stack::isEmpty() const {
    return m_values.empty();
}

void Stack::clear() {
    m_values.clear();
}

void Stack::popMultiple(size_t count) {
    if (count > m_values.size()) {
        throw std::runtime_error("スタックから十分な値をポップできません");
    }
    
    // 複数の値をポップ
    m_values.resize(m_values.size() - count);
}

std::string Stack::dump(size_t maxItems) const {
    std::ostringstream oss;
    
    // スタックの内容を文字列化
    oss << "スタック（" << size() << "項目）:" << std::endl;
    
    // 出力する項目数を決定
    size_t itemsToShow = maxItems > 0 && maxItems < size() ? maxItems : size();
    
    // スタックの最上位の項目から出力
    for (size_t i = 0; i < itemsToShow; ++i) {
        ValuePtr value = peekAt(i);
        oss << "  " << i << ": ";
        
        if (value) {
            oss << value->toString();
        } else {
            oss << "null";
        }
        
        oss << std::endl;
    }
    
    // 出力しなかった項目数を表示
    if (itemsToShow < size()) {
        oss << "  ... 他 " << (size() - itemsToShow) << " 項目" << std::endl;
    }
    
    return oss.str();
}

} // namespace core
} // namespace aerojs 