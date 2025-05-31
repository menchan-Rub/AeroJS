/**
 * @file context.h
 * @brief AeroJS JavaScript エンジンのコンテキストヘッダー
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_CORE_CONTEXT_H
#define AEROJS_CORE_CONTEXT_H

#include "value.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace aerojs {
namespace core {

// 前方宣言
class Engine;

/**
 * @brief グローバルオブジェクト
 */
struct GlobalObject {
    // 簡易実装
    int dummy;
};

/**
 * @brief JavaScript実行コンテキスト
 */
class Context {
public:
    explicit Context(Engine* engine);
    ~Context();

    // グローバルオブジェクト
    Value getGlobalObject() const;

    // 変数の管理
    void setVariable(const std::string& name, const Value& value);
    Value getVariable(const std::string& name) const;
    bool hasVariable(const std::string& name) const;
    void removeVariable(const std::string& name);

    // コードの評価
    Value evaluate(const std::string& source);

    // エンジンへのアクセス
    Engine* getEngine() const;

    // ガベージコレクション
    void collectGarbage();

    // 統計情報
    size_t getVariableCount() const;
    std::vector<std::string> getVariableNames() const;

    // リセット
    void clearVariables();

private:
    Engine* engine_;
    Value globalObject_;
    std::unique_ptr<std::unordered_map<std::string, Value>> variables_;

    void initializeGlobalObject();
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_CONTEXT_H