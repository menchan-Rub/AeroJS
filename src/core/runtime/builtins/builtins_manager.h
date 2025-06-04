/**
 * @file builtins_manager.h
 * @brief AeroJS 世界最高レベル組み込み関数マネージャー
 * @version 3.0.0
 * @license MIT
 */

#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class Value;

namespace runtime {
namespace builtins {

/**
 * @brief 世界最高レベル組み込み関数マネージャー
 * 
 * ECMAScript標準の全ての組み込みオブジェクトと関数を管理し、
 * 世界最高レベルのパフォーマンスと機能を提供します。
 */
class BuiltinsManager {
public:
    BuiltinsManager();
    ~BuiltinsManager();

    // コンテキスト管理
    void initializeContext(Context* context);
    void cleanupContext(Context* context);

    // 組み込み関数の取得
    void* getBuiltinFunction(const std::string& name) const;
    bool hasBuiltinFunction(const std::string& name) const;

    // 統計情報
    size_t getBuiltinFunctionCount() const;
    std::vector<std::string> getBuiltinFunctionNames() const;

private:
    // 初期化メソッド
    void initializeBuiltinFunctions();
    
    // 登録メソッド
    void registerBasicConstructors(Context* context);

    // コンストラクタ作成メソッド
    void* createObjectConstructor();
    void* createArrayConstructor();
    void* createFunctionConstructor();
    void* createStringConstructor();
    void* createNumberConstructor();
    void* createBooleanConstructor();

    // オブジェクト作成メソッド
    Value createMathObject();
    Value createJSONObject();

    // Console関数作成メソッド
    void* createConsoleObject();
    void* createConsoleLogFunction();

    // グローバル関数作成メソッド
    void* createParseIntFunction();
    void* createParseFloatFunction();
    void* createIsNaNFunction();
    void* createIsFiniteFunction();

private:
    // 組み込み関数のマップ
    std::unordered_map<std::string, void*> builtinFunctions_;
    
    // 統計情報
    mutable size_t accessCount_ = 0;
    mutable size_t cacheHits_ = 0;
    mutable size_t cacheMisses_ = 0;
};

} // namespace builtins
} // namespace runtime
} // namespace core
} // namespace aerojs 