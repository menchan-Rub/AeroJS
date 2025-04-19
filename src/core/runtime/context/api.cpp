/**
 * @file api.cpp
 * @brief @context APIの実装
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#include "api.h"
#include "context.h"
#include "../values/value.h"
#include "../values/string.h"
#include "../values/object.h"
#include "../values/function.h"
#include "../values/error.h"
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace aero {

// コンテキストIDの生成用カウンタ
static std::atomic<int> s_nextContextId{1};

// コンテキスト管理用ハッシュマップとミューテックス
static std::unordered_map<int, ExecutionContext*> s_contexts;
static std::mutex s_contextsMutex;

// コンテキストオブジェクトのID格納用プロパティ名
static const std::string kContextIdProp = "__contextId";

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
        return it->second;
    }
    return nullptr;
}

/**
 * @brief thisValueからコンテキストIDを取得
 * 
 * @param ctx 現在の実行コンテキスト
 * @param thisValue コンテキストオブジェクト
 * @return int コンテキストID（-1は無効）
 */
static int getContextIdFromThis(ExecutionContext* ctx, Value thisValue) {
    if (!thisValue.isObject()) {
        return -1;
    }
    
    Object* obj = thisValue.asObject();
    Value idValue = obj->get(ctx, kContextIdProp);
    
    if (!idValue.isNumber()) {
        return -1;
    }
    
    return static_cast<int>(idValue.asNumber());
}

/**
 * @brief thisValueからExecutionContextを取得
 * 
 * @param ctx 現在の実行コンテキスト
 * @param thisValue コンテキストオブジェクト
 * @return ExecutionContext* 対応するコンテキスト（存在しない場合はnullptr）
 */
static ExecutionContext* getContextFromThis(ExecutionContext* ctx, Value thisValue) {
    int id = getContextIdFromThis(ctx, thisValue);
    if (id == -1) {
        return nullptr;
    }
    
    return getContextById(id);
}

Object* ContextAPI::create(ExecutionContext* ctx, const ContextOptions& options) {
    // 新しいコンテキストの作成
    ExecutionContext* newCtx = new ExecutionContext();
    
    // オプションの設定
    newCtx->setStrictMode(options.strictMode);
    newCtx->setSharedArrayBuffersEnabled(options.hasSharedArrayBuffer);
    
    if (!options.locale.empty()) {
        newCtx->setLocale(options.locale);
    }
    
    // グローバルオブジェクトの作成と初期化
    Object* globalObj = newCtx->createGlobalObject();
    newCtx->initializeBuiltins(globalObj, options.hasConsole, options.hasModules);
    
    // コンテキストIDの生成と登録
    int contextId = s_nextContextId.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(s_contextsMutex);
        s_contexts[contextId] = newCtx;
    }
    
    // コンテキストを参照するオブジェクトの作成
    Object* contextObj = Object::create(ctx);
    
    // 内部プロパティとしてコンテキストIDを保存
    contextObj->defineOwnProperty(ctx, kContextIdProp, {
        .value = Value(static_cast<double>(contextId)),
        .writable = false,
        .enumerable = false,
        .configurable = false
    });
    
    return contextObj;
}

Value ContextAPI::evaluate(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // コンテキストの取得
    ExecutionContext* targetCtx = getContextFromThis(ctx, thisValue);
    if (!targetCtx) {
        return ctx->throwTypeError("評価対象のコンテキストが無効です");
    }
    
    // 引数のチェック
    if (args.empty()) {
        return ctx->throwTypeError("evaluate: コード文字列が必要です");
    }
    
    // コード文字列の取得
    String* codeStr = args[0].toString(ctx);
    if (!codeStr) {
        return ctx->throwTypeError("evaluate: 最初の引数は文字列である必要があります");
    }
    
    std::string code = codeStr->value();
    std::string fileName = "<eval>";
    
    // オプション引数の処理
    if (args.size() > 1 && args[1].isObject()) {
        Object* options = args[1].asObject();
        
        // ファイル名のオプション
        Value fileNameValue = options->get(ctx, "fileName");
        if (fileNameValue.isString()) {
            fileName = fileNameValue.asString()->value();
        }
    }
    
    // コードの評価
    try {
        Value result = targetCtx->evaluateScript(code, fileName);
        return result;
    } catch (const std::exception& e) {
        return ctx->throwError(e.what());
    }
}

Value ContextAPI::setGlobal(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // コンテキストの取得
    ExecutionContext* targetCtx = getContextFromThis(ctx, thisValue);
    if (!targetCtx) {
        return ctx->throwTypeError("setGlobal: コンテキストが無効です");
    }
    
    // 引数のチェック
    if (args.size() < 2) {
        return ctx->throwTypeError("setGlobal: 名前と値の2つの引数が必要です");
    }
    
    // プロパティ名の取得
    String* nameStr = args[0].toString(ctx);
    if (!nameStr) {
        return ctx->throwTypeError("setGlobal: 最初の引数は文字列である必要があります");
    }
    
    std::string name = nameStr->value();
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
        return ctx->throwTypeError("getGlobal: コンテキストが無効です");
    }
    
    // 引数のチェック
    if (args.empty()) {
        return ctx->throwTypeError("getGlobal: プロパティ名が必要です");
    }
    
    // プロパティ名の取得
    String* nameStr = args[0].toString(ctx);
    if (!nameStr) {
        return ctx->throwTypeError("getGlobal: 引数は文字列である必要があります");
    }
    
    std::string name = nameStr->value();
    
    // グローバルオブジェクトからプロパティ取得
    Object* globalObj = targetCtx->globalObject();
    return globalObj->get(targetCtx, name);
}

Value ContextAPI::deleteGlobal(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // コンテキストの取得
    ExecutionContext* targetCtx = getContextFromThis(ctx, thisValue);
    if (!targetCtx) {
        return ctx->throwTypeError("deleteGlobal: コンテキストが無効です");
    }
    
    // 引数のチェック
    if (args.empty()) {
        return ctx->throwTypeError("deleteGlobal: プロパティ名が必要です");
    }
    
    // プロパティ名の取得
    String* nameStr = args[0].toString(ctx);
    if (!nameStr) {
        return ctx->throwTypeError("deleteGlobal: 引数は文字列である必要があります");
    }
    
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
        return ctx->throwTypeError("importModule: コンテキストが無効です");
    }
    
    // 引数のチェック
    if (args.empty()) {
        return ctx->throwTypeError("importModule: モジュール指定子が必要です");
    }
    
    // モジュール指定子の取得
    String* specifierStr = args[0].toString(ctx);
    if (!specifierStr) {
        return ctx->throwTypeError("importModule: 最初の引数は文字列である必要があります");
    }
    
    std::string specifier = specifierStr->value();
    
    // モジュールの読み込み
    try {
        Object* moduleNamespace = targetCtx->importModule(specifier);
        return Value(moduleNamespace);
    } catch (const std::exception& e) {
        return ctx->throwError(e.what());
    }
}

Value ContextAPI::getGlobalObject(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // コンテキストの取得
    ExecutionContext* targetCtx = getContextFromThis(ctx, thisValue);
    if (!targetCtx) {
        return ctx->throwTypeError("getGlobalObject: コンテキストが無効です");
    }
    
    // グローバルオブジェクトの取得
    Object* globalObj = targetCtx->globalObject();
    return Value(globalObj);
}

Value ContextAPI::getOptions(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // コンテキストの取得
    ExecutionContext* targetCtx = getContextFromThis(ctx, thisValue);
    if (!targetCtx) {
        return ctx->throwTypeError("getOptions: コンテキストが無効です");
    }
    
    // オプションオブジェクトの作成
    Object* options = Object::create(ctx);
    
    // 現在の設定値を取得して設定
    options->set(ctx, "strictMode", Value(targetCtx->isStrictMode()));
    options->set(ctx, "hasConsole", Value(targetCtx->hasConsole()));
    options->set(ctx, "hasModules", Value(targetCtx->hasModules()));
    options->set(ctx, "hasSharedArrayBuffer", Value(targetCtx->isSharedArrayBuffersEnabled()));
    options->set(ctx, "locale", Value(ctx->createString(targetCtx->getLocale())));
    
    return Value(options);
}

Value ContextAPI::destroy(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // thisValueからコンテキストIDを取得
    int contextId = getContextIdFromThis(ctx, thisValue);
    if (contextId == -1) {
        // すでに破棄されているか無効なコンテキストの場合は何もしない
        return Value::undefined();
    }
    
    // コンテキストをマップから削除
    ExecutionContext* targetCtx = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_contextsMutex);
        auto it = s_contexts.find(contextId);
        if (it != s_contexts.end()) {
            targetCtx = it->second;
            s_contexts.erase(it);
        }
    }
    
    // thisオブジェクトからIDプロパティを削除
    if (thisValue.isObject()) {
        Object* obj = thisValue.asObject();
        obj->deleteProperty(ctx, kContextIdProp);
    }
    
    // コンテキストオブジェクトを解放
    if (targetCtx) {
        delete targetCtx;
    }
    
    return Value::undefined();
}

void registerContextAPI(ExecutionContext* ctx, Object* globalObj) {
    // @contextオブジェクトの作成
    Object* contextObj = Object::create(ctx);
    
    // create関数の作成
    FunctionCallback createCallback = [](ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) -> Value {
        ContextOptions options;
        
        // オプションオブジェクトが渡された場合の処理
        if (!args.empty() && args[0].isObject()) {
            Object* optionsObj = args[0].asObject();
            
            // 厳格モード
            Value strictModeVal = optionsObj->get(ctx, "strictMode");
            if (strictModeVal.isBoolean()) {
                options.strictMode = strictModeVal.asBoolean();
            }
            
            // コンソールAPI
            Value hasConsoleVal = optionsObj->get(ctx, "hasConsole");
            if (hasConsoleVal.isBoolean()) {
                options.hasConsole = hasConsoleVal.asBoolean();
            }
            
            // ESモジュール
            Value hasModulesVal = optionsObj->get(ctx, "hasModules");
            if (hasModulesVal.isBoolean()) {
                options.hasModules = hasModulesVal.asBoolean();
            }
            
            // SharedArrayBuffer
            Value hasSharedArrayBufferVal = optionsObj->get(ctx, "hasSharedArrayBuffer");
            if (hasSharedArrayBufferVal.isBoolean()) {
                options.hasSharedArrayBuffer = hasSharedArrayBufferVal.asBoolean();
            }
            
            // ロケール
            Value localeVal = optionsObj->get(ctx, "locale");
            if (localeVal.isString()) {
                options.locale = localeVal.asString()->value();
            }
        }
        
        // コンテキストオブジェクトの作成
        Object* contextObj = ContextAPI::create(ctx, options);
        return Value(contextObj);
    };
    
    // create関数をcontextオブジェクトに追加
    Function* createFunc = Function::create(ctx, "create", createCallback, 1);
    contextObj->set(ctx, "create", Value(createFunc));
    
    // コンテキストインスタンスが持つメソッドを作成するためのプロトタイプ
    Object* prototype = Object::create(ctx);
    
    // 各メソッドをプロトタイプに追加
    struct MethodDef {
        const char* name;
        FunctionCallback callback;
        int paramCount;
    };
    
    const MethodDef methods[] = {
        { "evaluate", ContextAPI::evaluate, 1 },
        { "setGlobal", ContextAPI::setGlobal, 2 },
        { "getGlobal", ContextAPI::getGlobal, 1 },
        { "deleteGlobal", ContextAPI::deleteGlobal, 1 },
        { "importModule", ContextAPI::importModule, 1 },
        { "getGlobalObject", ContextAPI::getGlobalObject, 0 },
        { "getOptions", ContextAPI::getOptions, 0 },
        { "destroy", ContextAPI::destroy, 0 }
    };
    
    for (const auto& method : methods) {
        Function* func = Function::create(ctx, method.name, method.callback, method.paramCount);
        prototype->set(ctx, method.name, Value(func));
    }
    
    // プロトタイプをcontextオブジェクトに設定
    contextObj->set(ctx, "prototype", Value(prototype));
    
    // 完成した@contextオブジェクトをグローバルオブジェクトに登録
    globalObj->set(ctx, "@context", Value(contextObj));
}

} // namespace aero 