/**
 * @file context.cpp
 * @brief JavaScript の Context API の実装
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#include "context.h"
#include "core/runtime/execution_context.h"
#include "core/runtime/object.h"
#include "core/runtime/value.h"
#include "core/runtime/error.h"
#include "core/runtime/function.h"
#include "core/runtime/global_object.h"
#include "core/runtime/values/string.h"
#include "core/runtime/module/module_loader.h"
#include "core/vm/vm.h"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <memory>

namespace aero {

// コンテキストIDの生成に使用する静的カウンタ
static std::atomic<int> s_nextContextId{1};

// コンテキストの管理用ハッシュマップとミューテックス
static std::unordered_map<int, std::unique_ptr<ExecutionContext>> s_contexts;
static std::mutex s_contextsMutex;

// コンテキストオブジェクトのID格納用シンボル
static const char* const kContextIdSymbol = "__contextId";

/**
 * @brief コンテキストIDからExecutionContextを取得
 * 
 * @param id コンテキストID
 * @return ExecutionContext* 見つかったコンテキスト（存在しない場合はnullptr）
 */
static ExecutionContext* getContextById(int id) {
    std::lock_guard<std::mutex> lock(s_contextsMutex);
    auto it = s_contexts.find(id);
    if (it != s_contexts.end()) {
        return it->second.get();
    }
    return nullptr;
}

/**
 * @brief thisValueからコンテキストIDを取得
 * 
 * @param ctx 現在の実行コンテキスト
 * @param thisValue thisの値（コンテキストオブジェクト）
 * @param throwIfMissing IDが見つからない場合に例外をスローするかどうか
 * @return int コンテキストID（-1は無効）
 */
static int getContextIdFromThis(ExecutionContext* ctx, Value thisValue, bool throwIfMissing = true) {
    // thisValueがオブジェクトでない場合はエラー
    if (!thisValue.isObject()) {
        if (throwIfMissing) {
            Error::throwTypeError(ctx, "コンテキストオブジェクトではありません");
        }
        return -1;
    }
    
    Object* obj = thisValue.asObject();
    Value idValue = obj->get(ctx, kContextIdSymbol);
    
    // コンテキストIDが見つからない場合はエラー
    if (!idValue.isNumber()) {
        if (throwIfMissing) {
            Error::throwTypeError(ctx, "無効なコンテキストオブジェクトです");
        }
        return -1;
    }
    
    return static_cast<int>(idValue.asNumber());
}

/**
 * @brief thisValueから対応するExecutionContextを取得
 * 
 * @param ctx 現在の実行コンテキスト
 * @param thisValue thisの値（コンテキストオブジェクト）
 * @param throwIfMissing コンテキストが見つからない場合に例外をスローするかどうか
 * @return ExecutionContext* 見つかったコンテキスト（存在しない場合はnullptr）
 */
static ExecutionContext* getContextFromThis(ExecutionContext* ctx, Value thisValue, bool throwIfMissing = true) {
    int id = getContextIdFromThis(ctx, thisValue, throwIfMissing);
    if (id == -1) {
        return nullptr;
    }
    
    ExecutionContext* targetCtx = getContextById(id);
    if (!targetCtx && throwIfMissing) {
        Error::throwTypeError(ctx, "指定されたコンテキストは存在しないか破棄されています");
    }
    
    return targetCtx;
}

Object* ContextAPI::create(ExecutionContext* ctx, const ContextOptions& options) {
    // 新しいコンテキストIDを生成
    int contextId = s_nextContextId.fetch_add(1);
    
    // 新しい実行コンテキストを作成
    std::unique_ptr<ExecutionContext> newCtx = std::make_unique<ExecutionContext>();
    
    // グローバルオブジェクトを初期化
    Object* globalObj = newCtx->globalObject();
    
    // オプションに基づいて環境を設定
    newCtx->setStrictMode(options.strictMode);
    
    // コンテキストAPIを登録しないオプション追加も検討
    registerContextAPI(newCtx.get(), globalObj);
    
    // コンテキストをレジストリに登録
    {
        std::lock_guard<std::mutex> lock(s_contextsMutex);
        s_contexts[contextId] = std::move(newCtx);
    }
    
    // コンテキストオブジェクトを作成
    Object* contextObj = Object::create(ctx);
    
    // コンテキストIDをオブジェクトに格納
    contextObj->defineOwnProperty(
        ctx, 
        kContextIdSymbol, 
        {
            .value = Value(static_cast<double>(contextId)),
            .writable = false,
            .enumerable = false,
            .configurable = false
        }
    );
    
    return contextObj;
}

Value ContextAPI::evaluate(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // コンテキストの取得
    ExecutionContext* targetCtx = getContextFromThis(ctx, thisValue);
    if (!targetCtx) {
        return Value::undefined();
    }
    
    // 引数のチェック
    if (args.size() < 1) {
        Error::throwTypeError(ctx, "evaluate には少なくとも1つの引数が必要です");
        return Value::undefined();
    }
    
    // コード文字列の取得
    if (!args[0].isString()) {
        Error::throwTypeError(ctx, "evaluate の第1引数は文字列である必要があります");
        return Value::undefined();
    }
    
    String* codeStr = args[0].asString();
    std::string code = codeStr->value();
    
    // オプションの取得（将来的な拡張用）
    Object* options = nullptr;
    if (args.size() > 1 && args[1].isObject()) {
        options = args[1].asObject();
    }
    
    // コードの評価
    try {
        Value result = targetCtx->evaluateScript(code, "<context.evaluate>");
        return result;
    } catch (const std::exception& e) {
        // エラーをラップして返す
        Error::throwError(ctx, e.what());
        return Value::undefined();
    }
}

Value ContextAPI::setGlobal(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // コンテキストの取得
    ExecutionContext* targetCtx = getContextFromThis(ctx, thisValue);
    if (!targetCtx) {
        return Value::undefined();
    }
    
    // 引数のチェック
    if (args.size() < 2) {
        Error::throwTypeError(ctx, "setGlobal には少なくとも2つの引数が必要です");
        return Value::undefined();
    }
    
    // プロパティ名の取得
    String* nameStr = args[0].toString(ctx);
    std::string name = nameStr->value();
    
    // 値の取得
    Value value = args[1];
    
    // グローバルオブジェクトへのプロパティ設定
    Object* globalObj = targetCtx->globalObject();
    globalObj->set(targetCtx, name, value);
    
    return Value::undefined();
}

Value ContextAPI::getGlobal(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // コンテキストの取得
    ExecutionContext* targetCtx = getContextFromThis(ctx, thisValue);
    if (!targetCtx) {
        return Value::undefined();
    }
    
    // 引数のチェック
    if (args.size() < 1) {
        Error::throwTypeError(ctx, "getGlobal には少なくとも1つの引数が必要です");
        return Value::undefined();
    }
    
    // プロパティ名の取得
    String* nameStr = args[0].toString(ctx);
    std::string name = nameStr->value();
    
    // グローバルオブジェクトからプロパティ取得
    Object* globalObj = targetCtx->globalObject();
    return globalObj->get(targetCtx, name);
}

Value ContextAPI::deleteGlobal(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // コンテキストの取得
    ExecutionContext* targetCtx = getContextFromThis(ctx, thisValue);
    if (!targetCtx) {
        return Value::undefined();
    }
    
    // 引数のチェック
    if (args.size() < 1) {
        Error::throwTypeError(ctx, "deleteGlobal には少なくとも1つの引数が必要です");
        return Value::undefined();
    }
    
    // プロパティ名の取得
    String* nameStr = args[0].toString(ctx);
    std::string name = nameStr->value();
    
    // グローバルオブジェクトからプロパティ削除
    Object* globalObj = targetCtx->globalObject();
    bool result = globalObj->deleteProperty(targetCtx, name);
    
    return Value(result);
}

Value ContextAPI::importModule(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // コンテキストの取得
    ExecutionContext* targetCtx = getContextFromThis(ctx, thisValue);
    if (!targetCtx) {
        return Value::undefined();
    }
    
    // 引数のチェック
    if (args.size() < 1) {
        Error::throwTypeError(ctx, "importModule には少なくとも1つの引数が必要です");
        return Value::undefined();
    }
    
    // モジュール指定子の取得
    if (!args[0].isString()) {
        Error::throwTypeError(ctx, "importModule の第1引数は文字列である必要があります");
        return Value::undefined();
    }
    
    String* specifierStr = args[0].asString();
    std::string specifier = specifierStr->value();
    
    // モジュールの読み込み
    try {
        // ModuleLoaderを使用してモジュールを読み込む
        Object* moduleNamespace = ModuleLoader::importModule(targetCtx, specifier);
        if (!moduleNamespace) {
            Error::throwError(ctx, "モジュールの読み込みに失敗しました: " + specifier);
            return Value::undefined();
        }
        
        return Value(moduleNamespace);
    } catch (const std::exception& e) {
        // エラーをラップして返す
        Error::throwError(ctx, e.what());
        return Value::undefined();
    }
}

Value ContextAPI::getGlobalObject(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // コンテキストの取得
    ExecutionContext* targetCtx = getContextFromThis(ctx, thisValue);
    if (!targetCtx) {
        return Value::undefined();
    }
    
    // グローバルオブジェクトを返す
    return Value(targetCtx->globalObject());
}

Value ContextAPI::getOptions(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // コンテキストの取得
    ExecutionContext* targetCtx = getContextFromThis(ctx, thisValue);
    if (!targetCtx) {
        return Value::undefined();
    }
    
    // オプションオブジェクトを作成
    Object* optionsObj = Object::create(ctx);
    
    // 現在のコンテキスト設定をオブジェクトに設定
    optionsObj->set(ctx, "strictMode", Value(targetCtx->isStrictMode()));
    
    // 他のオプションも設定（実際の実装に応じて追加）
    // optionsObj->set(ctx, "hasConsole", Value(true));
    // optionsObj->set(ctx, "hasModules", Value(true));
    // optionsObj->set(ctx, "hasSharedArrayBuffer", Value(false));
    
    return Value(optionsObj);
}

Value ContextAPI::destroy(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // コンテキストIDの取得（エラーをスローしない）
    int contextId = getContextIdFromThis(ctx, thisValue, false);
    if (contextId == -1) {
        return Value::undefined();
    }
    
    // コンテキストをレジストリから削除
    {
        std::lock_guard<std::mutex> lock(s_contextsMutex);
        s_contexts.erase(contextId);
    }
    
    // thisオブジェクトからIDプロパティを削除
    if (thisValue.isObject()) {
        Object* obj = thisValue.asObject();
        obj->deleteProperty(ctx, kContextIdSymbol);
    }
    
    return Value::undefined();
}

// Context APIの関数定義用ヘルパー
struct ContextMethod {
    const char* name;
    FunctionCallback callback;
    int length;
};

// Context APIの関数定義
static const ContextMethod s_contextMethods[] = {
    { "evaluate", ContextAPI::evaluate, 1 },
    { "setGlobal", ContextAPI::setGlobal, 2 },
    { "getGlobal", ContextAPI::getGlobal, 1 },
    { "deleteGlobal", ContextAPI::deleteGlobal, 1 },
    { "importModule", ContextAPI::importModule, 1 },
    { "getGlobalObject", ContextAPI::getGlobalObject, 0 },
    { "getOptions", ContextAPI::getOptions, 0 },
    { "destroy", ContextAPI::destroy, 0 }
};

void registerContextAPI(ExecutionContext* ctx, Object* global) {
    // @contextオブジェクトを作成
    Object* contextObj = Object::create(ctx);
    
    // create関数を登録
    Function* createFn = Function::create(
        ctx,
        "create",
        [](ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) -> Value {
            ContextOptions options;
            
            // オプションオブジェクトが渡された場合に処理
            if (args.size() > 0 && args[0].isObject()) {
                Object* optionsObj = args[0].asObject();
                
                // 各オプションを取得
                Value strictModeVal = optionsObj->get(ctx, "strictMode");
                if (strictModeVal.isBoolean()) {
                    options.strictMode = strictModeVal.asBoolean();
                }
                
                Value hasConsoleVal = optionsObj->get(ctx, "hasConsole");
                if (hasConsoleVal.isBoolean()) {
                    options.hasConsole = hasConsoleVal.asBoolean();
                }
                
                Value hasModulesVal = optionsObj->get(ctx, "hasModules");
                if (hasModulesVal.isBoolean()) {
                    options.hasModules = hasModulesVal.asBoolean();
                }
                
                Value hasSharedArrayBufferVal = optionsObj->get(ctx, "hasSharedArrayBuffer");
                if (hasSharedArrayBufferVal.isBoolean()) {
                    options.hasSharedArrayBuffer = hasSharedArrayBufferVal.asBoolean();
                }
                
                Value localeVal = optionsObj->get(ctx, "locale");
                if (localeVal.isString()) {
                    options.locale = localeVal.asString()->value();
                }
            }
            
            // 新しいコンテキストを作成
            return Value(ContextAPI::create(ctx, options));
        },
        1
    );
    contextObj->set(ctx, "create", Value(createFn));
    
    // 各メソッドをコンテキストオブジェクトのプロトタイプに登録
    Object* prototype = Object::create(ctx);
    
    for (const auto& method : s_contextMethods) {
        Function* fn = Function::create(ctx, method.name, method.callback, method.length);
        prototype->set(ctx, method.name, Value(fn));
    }
    
    // プロトタイプを設定
    contextObj->set(ctx, "prototype", Value(prototype));
    
    // グローバルオブジェクトに登録
    global->set(ctx, "@context", Value(contextObj));
}

} // namespace aero 