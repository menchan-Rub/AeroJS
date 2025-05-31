/**
 * @file builtins_manager.h
 * @brief AeroJS JavaScript エンジンの組み込み関数マネージャー
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_CORE_RUNTIME_BUILTINS_BUILTINS_MANAGER_H
#define AEROJS_CORE_RUNTIME_BUILTINS_BUILTINS_MANAGER_H

namespace aerojs {
namespace core {

// 前方宣言
class Context;

namespace runtime {
namespace builtins {

/**
 * @brief 組み込み関数マネージャー
 */
class BuiltinsManager {
public:
    BuiltinsManager();
    ~BuiltinsManager();

    // コンテキストの初期化
    void initializeContext(Context* context);
    void cleanupContext(Context* context);

private:
    // 組み込み関数の登録
    void registerGlobalFunctions(Context* context);
    void registerObjectConstructor(Context* context);
    void registerArrayConstructor(Context* context);
    void registerFunctionConstructor(Context* context);
    void registerStringConstructor(Context* context);
    void registerNumberConstructor(Context* context);
    void registerBooleanConstructor(Context* context);

    // オブジェクト作成ヘルパー
    void* createConsoleObject();
    void* createParseIntFunction();
    void* createParseFloatFunction();
    void* createIsNaNFunction();
    void* createIsFiniteFunction();
    void* createObjectConstructor();
    void* createArrayConstructor();
    void* createFunctionConstructor();
    void* createStringConstructor();
    void* createNumberConstructor();
    void* createBooleanConstructor();
};

} // namespace builtins
} // namespace runtime
} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_RUNTIME_BUILTINS_BUILTINS_MANAGER_H 