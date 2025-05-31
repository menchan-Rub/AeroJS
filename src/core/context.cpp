/**
 * @file context.cpp
 * @brief AeroJS JavaScript エンジンのコンテキスト実装
 * @version 0.1.0
 * @license MIT
 */

#include "context.h"
#include "engine.h"
#include "value.h"
#include <memory>
#include <unordered_map>
#include <limits>
#include <vector>

namespace aerojs {
namespace core {

Context::Context(Engine* engine) 
    : engine_(engine),
      globalObject_(Value::undefined()),
      variables_(std::make_unique<std::unordered_map<std::string, Value>>()) {
    
    // グローバルオブジェクトの初期化
    initializeGlobalObject();
}

Context::~Context() = default;

void Context::initializeGlobalObject() {
    // グローバルオブジェクトの作成は後で実装
    // 現在は組み込み変数のみ設定
    
    // 組み込み変数の設定
    setVariable("undefined", Value::undefined());
    setVariable("null", Value::null());
    setVariable("true", Value::fromBoolean(true));
    setVariable("false", Value::fromBoolean(false));
    setVariable("NaN", Value::fromNumber(std::numeric_limits<double>::quiet_NaN()));
    setVariable("Infinity", Value::fromNumber(std::numeric_limits<double>::infinity()));
}

Value Context::getGlobalObject() const {
    return globalObject_;
}

void Context::setVariable(const std::string& name, const Value& value) {
    if (variables_) {
        // emplaceを使用してコピーコンストラクタを呼び出す
        variables_->emplace(name, value);
    }
}

Value Context::getVariable(const std::string& name) const {
    if (variables_) {
        auto it = variables_->find(name);
        if (it != variables_->end()) {
            return it->second;
        }
    }
    return Value::undefined();
}

bool Context::hasVariable(const std::string& name) const {
    if (variables_) {
        return variables_->find(name) != variables_->end();
    }
    return false;
}

void Context::removeVariable(const std::string& name) {
    if (variables_) {
        variables_->erase(name);
    }
}

Value Context::evaluate(const std::string& source) {
    (void)source; // 未使用パラメータ警告を回避
    // 簡易実装：パーサーが実装されるまでundefinedを返す
    return Value::undefined();
}

Engine* Context::getEngine() const {
    return engine_;
}

void Context::collectGarbage() {
    // 簡易実装：エンジンのGCを呼び出す
    if (engine_) {
        engine_->collectGarbage();
    }
}

size_t Context::getVariableCount() const {
    return variables_ ? variables_->size() : 0;
}

std::vector<std::string> Context::getVariableNames() const {
    std::vector<std::string> names;
    if (variables_) {
        names.reserve(variables_->size());
        for (const auto& pair : *variables_) {
            names.push_back(pair.first);
        }
    }
    return names;
}

void Context::clearVariables() {
    if (variables_) {
        variables_->clear();
        // 組み込み変数を再設定
        initializeGlobalObject();
    }
}

} // namespace core
} // namespace aerojs