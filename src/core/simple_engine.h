/**
 * @file simple_engine.h
 * @brief AeroJS 簡単なJavaScriptエンジン
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_CORE_SIMPLE_ENGINE_H
#define AEROJS_CORE_SIMPLE_ENGINE_H

#include "simple_value.h"
#include <string>
#include <unordered_map>

namespace aerojs {
namespace core {

/**
 * @brief 簡単なJavaScriptエンジンクラス
 */
class SimpleEngine {
public:
    SimpleEngine();
    ~SimpleEngine();

    // 基本的な評価機能
    SimpleValue evaluate(const std::string& code);
    
    // 変数の管理
    void setVariable(const std::string& name, const SimpleValue& value);
    SimpleValue getVariable(const std::string& name) const;
    bool hasVariable(const std::string& name) const;
    
    // エンジンの状態
    void reset();
    size_t getVariableCount() const;

private:
    std::unordered_map<std::string, SimpleValue> variables_;
    
    // 簡単な式の評価
    SimpleValue evaluateExpression(const std::string& expr);
    SimpleValue evaluateLiteral(const std::string& literal);
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_SIMPLE_ENGINE_H 