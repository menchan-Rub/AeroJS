/**
 * @file simple_engine.cpp
 * @brief AeroJS 簡単なJavaScriptエンジン実装
 * @version 0.1.0
 * @license MIT
 */

#include "simple_engine.h"
#include <algorithm>
#include <cctype>

namespace aerojs {
namespace core {

SimpleEngine::SimpleEngine() {
    // グローバル変数の初期化
    variables_["undefined"] = SimpleValue::undefined();
    variables_["null"] = SimpleValue::null();
    variables_["true"] = SimpleValue::fromBoolean(true);
    variables_["false"] = SimpleValue::fromBoolean(false);
}

SimpleEngine::~SimpleEngine() = default;

SimpleValue SimpleEngine::evaluate(const std::string& code) {
    // 簡単な評価実装
    std::string trimmed = code;
    
    // 前後の空白を削除
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), trimmed.end());
    
    if (trimmed.empty()) {
        return SimpleValue::undefined();
    }
    
    return evaluateExpression(trimmed);
}

void SimpleEngine::setVariable(const std::string& name, const SimpleValue& value) {
    variables_[name] = value;
}

SimpleValue SimpleEngine::getVariable(const std::string& name) const {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return it->second;
    }
    return SimpleValue::undefined();
}

bool SimpleEngine::hasVariable(const std::string& name) const {
    return variables_.find(name) != variables_.end();
}

void SimpleEngine::reset() {
    variables_.clear();
    // グローバル変数の再初期化
    variables_["undefined"] = SimpleValue::undefined();
    variables_["null"] = SimpleValue::null();
    variables_["true"] = SimpleValue::fromBoolean(true);
    variables_["false"] = SimpleValue::fromBoolean(false);
}

size_t SimpleEngine::getVariableCount() const {
    return variables_.size();
}

SimpleValue SimpleEngine::evaluateExpression(const std::string& expr) {
    // 変数の参照をチェック
    if (hasVariable(expr)) {
        return getVariable(expr);
    }
    
    // リテラルの評価
    return evaluateLiteral(expr);
}

SimpleValue SimpleEngine::evaluateLiteral(const std::string& literal) {
    // 文字列リテラル
    if (literal.size() >= 2 && literal.front() == '"' && literal.back() == '"') {
        return SimpleValue::fromString(literal.substr(1, literal.size() - 2));
    }
    
    if (literal.size() >= 2 && literal.front() == '\'' && literal.back() == '\'') {
        return SimpleValue::fromString(literal.substr(1, literal.size() - 2));
    }
    
    // 数値リテラル
    try {
        double num = std::stod(literal);
        return SimpleValue::fromNumber(num);
    } catch (...) {
        // 数値でない場合は何もしない
    }
    
    // ブール値リテラル
    if (literal == "true") {
        return SimpleValue::fromBoolean(true);
    }
    if (literal == "false") {
        return SimpleValue::fromBoolean(false);
    }
    
    // null
    if (literal == "null") {
        return SimpleValue::null();
    }
    
    // undefined
    if (literal == "undefined") {
        return SimpleValue::undefined();
    }
    
    // 不明なリテラル
    return SimpleValue::undefined();
}

} // namespace core
} // namespace aerojs 