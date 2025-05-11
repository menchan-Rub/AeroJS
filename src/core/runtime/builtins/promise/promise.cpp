/**
 * @file promise.cpp
 * @brief JavaScriptのPromiseオブジェクトの実装
 * @copyright 2023 AeroJS プロジェクト
 */

#include "promise.h"
#include <algorithm>
#include <functional>
#include <mutex>
#include <vector>
#include "../../context/execution_context.h"
#include "../../exception/exception.h"
#include "../../global_object.h"
#include "../../object.h"
#include "../../value.h"
#include "../array/array.h"
#include "../error/error.h"
#include "../function/function.h"

namespace aero {

// 静的メンバ変数の初期化
std::vector<std::function<void()>> PromiseObject::s_microtaskQueue;
std::mutex PromiseObject::s_microtaskMutex;
Object* PromiseObject::s_prototype = nullptr;

// PromiseObjectの実装
PromiseObject::PromiseObject(Value executor, GlobalObject* globalObj)
    : Object(globalObj->getPromisePrototype()),
      m_state(PromiseState::Pending),
      m_result(Value::undefined()),
      m_globalObject(globalObj) {
  // エグゼキュータ関数が関数オブジェクトでない場合、例外をスロー
  if (!executor.isFunction()) {
    throw TypeException("Promise constructor requires a function argument");
  }

  // resolve関数を作成
  auto resolveFunc = [this](const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
    if (args.size() > 0) {
      this->resolve(args[0]);
    } else {
      this->resolve(Value::undefined());
    }
    return Value::undefined();
  };

  // reject関数を作成
  auto rejectFunc = [this](const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
    if (args.size() > 0) {
      this->reject(args[0]);
    } else {
      this->reject(Value::undefined());
    }
    return Value::undefined();
  };

  // resolve関数と、reject関数を作成
  Value resolveFunction = FunctionObject::create(resolveFunc, "resolve", 1, globalObj);
  Value rejectFunction = FunctionObject::create(rejectFunc, "reject", 1, globalObj);

  // executor関数を実行
  try {
    std::vector<Value> args = {resolveFunction, rejectFunction};
    executor.call(Value::undefined(), args, globalObj);
  } catch (const Exception& e) {
    // エグゼキュータ関数内で例外が発生した場合、Promiseを拒否状態にする
    reject(e.getValue());
  }
}

PromiseObject::~PromiseObject() = default;

// Promiseの状態を取得
PromiseState PromiseObject::getState() const {
  return m_state;
}

// Promiseの結果値を取得
Value PromiseObject::getResult() const {
  return m_result;
}

// Promiseを解決（成功）する
void PromiseObject::resolve(const Value& value) {
  std::vector<PromiseReaction> reactions;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 既に解決または拒否されている場合は何もしない
    if (m_state != PromiseState::Pending) {
      return;
    }

    // もし解決値が現在のPromiseオブジェクト自身である場合、TypeError例外で拒否する
    if (value.isObject() && value.asObject() == this) {
      reject(TypeException("Promise resolved with itself").getValue());
      return;
    }

    // 解決値がthenableオブジェクトである場合、それに従う
    if (value.isObject()) {
      Object* obj = value.asObject();
      Value thenMethod;
      
      try {
        thenMethod = obj->get("then");
      } catch (const Exception& e) {
        // thenプロパティへのアクセス中に例外が発生した場合、その例外でPromiseを拒否
        reject(e.getValue());
        return;
      }
      
      // thenメソッドが関数であれば、呼び出す
      if (thenMethod.isFunction()) {
        auto resolvePromiseFunc = [this](const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
          if (args.size() > 0) {
            this->resolve(args[0]);
          } else {
            this->resolve(Value::undefined());
          }
          return Value::undefined();
        };

        auto rejectPromiseFunc = [this](const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
          if (args.size() > 0) {
            this->reject(args[0]);
          } else {
            this->reject(Value::undefined());
          }
          return Value::undefined();
        };

        Value resolvePromise = FunctionObject::create(resolvePromiseFunc, "resolvePromise", 1, m_globalObject);
        Value rejectPromise = FunctionObject::create(rejectPromiseFunc, "rejectPromise", 1, m_globalObject);

        // thenメソッドの呼び出しをマイクロタスクとしてキューに追加
        enqueueMicrotask([thenMethod, value, resolvePromise, rejectPromise, this]() {
          try {
            std::vector<Value> args = {resolvePromise, rejectPromise};
            thenMethod.call(value, args, m_globalObject);
          } catch (const Exception& e) {
            // thenメソッドの呼び出し中に例外が発生した場合、その例外でPromiseを拒否
            this->reject(e.getValue());
          }
        });
        
        return;
      }
    }

    // 単純な値またはthenメソッドを持たないオブジェクトの場合は、直接解決
    m_state = PromiseState::Fulfilled;
    m_result = value;
    
    // 既存のリアクションをローカルにコピー
    reactions = std::move(m_reactions);
  }
  
  // リアクションを実行
  fulfillReactions(reactions, value);
}

// Promiseを拒否（失敗）する
void PromiseObject::reject(const Value& reason) {
  std::vector<PromiseReaction> reactions;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 既に解決または拒否されている場合は何もしない
    if (m_state != PromiseState::Pending) {
      return;
    }

    // Promiseを拒否状態としてマーク
    m_state = PromiseState::Rejected;
    m_result = reason;
    
    // 既存のリアクションをローカルにコピー
    reactions = std::move(m_reactions);
  }
  
  // リアクションを実行
  rejectReactions(reactions, reason);
}

// thenハンドラを登録する
PromiseObject* PromiseObject::then(const Value& onFulfilled, const Value& onRejected) {
  // 新しいPromiseを作成するためのエグゼキュータ関数
  auto executor = [](const std::vector<Value>& /*args*/, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
    return Value::undefined();
  };
  
  // 新しいPromiseを作成
  PromiseObject* resultPromise = new PromiseObject(
      FunctionObject::create(executor, "thenExecutor", 2, m_globalObject),
      m_globalObject);
  
  // 現在のPromiseの状態に応じたハンドラの登録
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state == PromiseState::Pending) {
      // Promiseがまだ保留中の場合、リアクションに追加
      addReaction(onFulfilled, onRejected, resultPromise);
    } else if (m_state == PromiseState::Fulfilled) {
      // 既に解決済みの場合、成功ハンドラをマイクロタスクとして追加
      enqueueMicrotask([this, onFulfilled, resultPromise]() {
        this->handleFulfilled(onFulfilled, resultPromise);
      });
    } else if (m_state == PromiseState::Rejected) {
      // 既に拒否済みの場合、失敗ハンドラをマイクロタスクとして追加
      enqueueMicrotask([this, onRejected, resultPromise]() {
        this->handleRejected(onRejected, resultPromise);
      });
    }
  }
  
  return resultPromise;
}

// catchハンドラを登録する
PromiseObject* PromiseObject::catch_(const Value& onRejected) {
  // catchは基本的にthen(undefined, onRejected)と同じ
  return then(Value::undefined(), onRejected);
}

// finallyハンドラを登録する
PromiseObject* PromiseObject::finally_(const Value& onFinally) {
  if (!onFinally.isFunction()) {
    // onFinallyが関数でない場合は、単純にthen(identity, identity)と同じ
    return then(Value::undefined(), Value::undefined());
  }
  
  // onFinallyWrapperはPromiseの状態に関わらず同じ関数を呼び出す
  auto onFulfilledWrapper = [onFinally, this](const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) -> Value {
    // onFinallyを呼び出し
    Value result = onFinally.call(Value::undefined(), {}, globalObj);
    
    // 結果がPromiseの場合、それを待機してから元の値を返す
    if (result.isObject() && result.asObject()->isPromise()) {
      PromiseObject* resultPromise = static_cast<PromiseObject*>(result.asObject());
      
      // 新しいPromiseを作成するためのエグゼキュータ
      auto executor = [args, thisObj, globalObj](const std::vector<Value>& executorArgs, Object* /*executorThis*/, GlobalObject* /*executorGlobal*/) -> Value {
        Value resolve = executorArgs[0];
        Value originalValue = args.size() > 0 ? args[0] : Value::undefined();
        
        // 元の値を解決
        std::vector<Value> resolveArgs = {originalValue};
        resolve.call(Value::undefined(), resolveArgs, globalObj);
        return Value::undefined();
      };
      
      // 新しいPromiseを作成して返す
      return Value(new PromiseObject(
          FunctionObject::create(executor, "finallyExecutor", 2, globalObj),
          globalObj));
    }
    
    // onFinallyの結果がPromiseでなければ、元の値をそのまま返す
    return args.size() > 0 ? args[0] : Value::undefined();
  };
  
  // onRejectedWrapperはエラーを再スローする
  auto onRejectedWrapper = [onFinally, this](const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) -> Value {
    // onFinallyを呼び出し
    Value result = onFinally.call(Value::undefined(), {}, globalObj);
    
    // 結果がPromiseの場合、それを待機してからエラーをスロー
    if (result.isObject() && result.asObject()->isPromise()) {
      PromiseObject* resultPromise = static_cast<PromiseObject*>(result.asObject());
      
      // 新しいPromiseを作成するためのエグゼキュータ
      auto executor = [args, thisObj, globalObj](const std::vector<Value>& executorArgs, Object* /*executorThis*/, GlobalObject* /*executorGlobal*/) -> Value {
        Value reject = executorArgs[1];
        Value originalReason = args.size() > 0 ? args[0] : Value::undefined();
        
        // 元のエラーを再スロー
        std::vector<Value> rejectArgs = {originalReason};
        reject.call(Value::undefined(), rejectArgs, globalObj);
        return Value::undefined();
      };
      
      // 新しいPromiseを作成して返す
      return Value(new PromiseObject(
          FunctionObject::create(executor, "finallyExecutor", 2, globalObj),
          globalObj));
    }
    
    // onFinallyの結果がPromiseでなければ、元のエラーを再スロー
    throw Exception(args.size() > 0 ? args[0] : Value::undefined());
  };
  
  // onFulfilledWrapperとonRejectedWrapperをthenに渡す
  return then(
      FunctionObject::create(onFulfilledWrapper, "fulfillFinally", 1, m_globalObject),
      FunctionObject::create(onRejectedWrapper, "rejectFinally", 1, m_globalObject));
}

// Promiseリアクションを登録
void PromiseObject::addReaction(const Value& onFulfilled, const Value& onRejected, PromiseObject* resultPromise) {
  PromiseReaction fulfillReaction;
  fulfillReaction.handler = onFulfilled;
  fulfillReaction.resultPromise = resultPromise;
  fulfillReaction.isReject = false;
  
  PromiseReaction rejectReaction;
  rejectReaction.handler = onRejected;
  rejectReaction.resultPromise = resultPromise;
  rejectReaction.isReject = true;
  
  m_reactions.push_back(fulfillReaction);
  m_reactions.push_back(rejectReaction);
}

// 成功リアクションを処理
void PromiseObject::handleFulfilled(const Value& onFulfilled, PromiseObject* resultPromise) {
  // onFulfilledが関数でない場合、結果をそのまま次のPromiseに渡す
  if (!onFulfilled.isFunction()) {
    resultPromise->resolve(m_result);
    return;
  }
  
  try {
    // onFulfilledハンドラを呼び出し
    std::vector<Value> args = {m_result};
    Value handlerResult = onFulfilled.call(Value::undefined(), args, m_globalObject);
    
    // 結果で次のPromiseを解決
    resultPromise->resolve(handlerResult);
  } catch (const Exception& e) {
    // ハンドラ内で例外が発生した場合、次のPromiseを拒否
    resultPromise->reject(e.getValue());
  }
}

// 失敗リアクションを処理
void PromiseObject::handleRejected(const Value& onRejected, PromiseObject* resultPromise) {
  // onRejectedが関数でない場合、エラーをそのまま次のPromiseに渡す
  if (!onRejected.isFunction()) {
    resultPromise->reject(m_result);
    return;
  }
  
  try {
    // onRejectedハンドラを呼び出し
    std::vector<Value> args = {m_result};
    Value handlerResult = onRejected.call(Value::undefined(), args, m_globalObject);
    
    // 結果で次のPromiseを解決
    resultPromise->resolve(handlerResult);
  } catch (const Exception& e) {
    // ハンドラ内で例外が発生した場合、次のPromiseを拒否
    resultPromise->reject(e.getValue());
  }
}

// リアクションを実行（解決時）
void PromiseObject::fulfillReactions(std::vector<PromiseReaction> reactions, const Value& value) {
  for (const auto& reaction : reactions) {
    if (!reaction.isReject) {
      // 成功リアクションのみを処理
      enqueueMicrotask([this, reaction, value]() {
        this->handleFulfilled(reaction.handler, reaction.resultPromise);
      });
    }
  }
}

// リアクションを実行（拒否時）
void PromiseObject::rejectReactions(std::vector<PromiseReaction> reactions, const Value& reason) {
  for (const auto& reaction : reactions) {
    if (reaction.isReject) {
      // 失敗リアクションのみを処理
      enqueueMicrotask([this, reaction, reason]() {
        this->handleRejected(reaction.handler, reaction.resultPromise);
      });
    }
  }
}

// マイクロタスクキューにタスクを追加
void PromiseObject::enqueueMicrotask(std::function<void()> task) {
  std::lock_guard<std::mutex> lock(s_microtaskMutex);
  s_microtaskQueue.push_back(std::move(task));
}

// 登録されたマイクロタスクを処理
void PromiseObject::processMicrotasks() {
  std::vector<std::function<void()>> currentTasks;
  
  // 現在のマイクロタスクのみを処理し、処理中に追加されたタスクは次回に回す
  {
    std::lock_guard<std::mutex> lock(s_microtaskMutex);
    currentTasks = std::move(s_microtaskQueue);
  }
  
  for (const auto& task : currentTasks) {
    task();
  }
}

// Promiseコンストラクタ関数
Value promiseConstructor(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがPromiseコンストラクタでない場合はエラー
  if (thisObj == nullptr || !thisObj->isConstructor()) {
    throw TypeException("Promise constructor called on an object that is not a constructor");
  }
  
  // 引数が与えられていない場合はエラー
  if (args.empty() || !args[0].isFunction()) {
    throw TypeException("Promise constructor requires a function argument");
  }
  
  // 新しいPromiseオブジェクトを作成
  PromiseObject* promise = new PromiseObject(args[0], globalObj);
  return Value(promise);
}

// Promise.resolve 静的メソッド
Value promiseResolve(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがPromiseコンストラクタでない場合はエラー
  if (thisObj == nullptr || !thisObj->isConstructor()) {
    throw TypeException("Promise.resolve called on an object that is not a constructor");
  }
  
  Value value = args.empty() ? Value::undefined() : args[0];
  
  // 引数が既にPromiseオブジェクトで、thisがPromiseコンストラクタの場合、そのままPromiseを返す
  if (value.isObject() && value.asObject()->isPromise() && thisObj == globalObj->getPromiseConstructor()) {
    return value;
  }
  
  // エグゼキュータ関数
  auto executor = [value](const std::vector<Value>& executorArgs, Object* /*executorThis*/, GlobalObject* /*executorGlobal*/) -> Value {
    Value resolve = executorArgs[0];
    
    // 値でPromiseを解決
    std::vector<Value> resolveArgs = {value};
    resolve.call(Value::undefined(), resolveArgs, nullptr);
    return Value::undefined();
  };
  
  // 新しいPromiseを作成
  PromiseObject* promise = new PromiseObject(
      FunctionObject::create(executor, "resolveExecutor", 2, globalObj),
      globalObj);
  return Value(promise);
}

// Promise.reject 静的メソッド
Value promiseReject(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがPromiseコンストラクタでない場合はエラー
  if (thisObj == nullptr || !thisObj->isConstructor()) {
    throw TypeException("Promise.reject called on an object that is not a constructor");
  }
  
  Value reason = args.empty() ? Value::undefined() : args[0];
  
  // エグゼキュータ関数
  auto executor = [reason](const std::vector<Value>& executorArgs, Object* /*executorThis*/, GlobalObject* /*executorGlobal*/) -> Value {
    Value reject = executorArgs[1];
    
    // 理由でPromiseを拒否
    std::vector<Value> rejectArgs = {reason};
    reject.call(Value::undefined(), rejectArgs, nullptr);
    return Value::undefined();
  };
  
  // 新しいPromiseを作成
  PromiseObject* promise = new PromiseObject(
      FunctionObject::create(executor, "rejectExecutor", 2, globalObj),
      globalObj);
  return Value(promise);
}

// Promise.prototype.then メソッド
Value promiseThen(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがPromiseオブジェクトでない場合はエラー
  if (thisObj == nullptr || !thisObj->isPromise()) {
    throw TypeException("Promise.prototype.then called on an object that is not a Promise");
  }
  
  PromiseObject* promise = static_cast<PromiseObject*>(thisObj);
  
  Value onFulfilled = args.size() > 0 && args[0].isFunction() ? args[0] : Value::undefined();
  Value onRejected = args.size() > 1 && args[1].isFunction() ? args[1] : Value::undefined();
  
  // then呼び出し
  PromiseObject* resultPromise = promise->then(onFulfilled, onRejected);
  return Value(resultPromise);
}

// Promise.prototype.catch メソッド
Value promiseCatch(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがPromiseオブジェクトでない場合はエラー
  if (thisObj == nullptr || !thisObj->isPromise()) {
    throw TypeException("Promise.prototype.catch called on an object that is not a Promise");
  }
  
  PromiseObject* promise = static_cast<PromiseObject*>(thisObj);
  
  Value onRejected = args.size() > 0 && args[0].isFunction() ? args[0] : Value::undefined();
  
  // catch呼び出し
  PromiseObject* resultPromise = promise->catch_(onRejected);
  return Value(resultPromise);
}

// Promise.prototype.finally メソッド
Value promiseFinally(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがPromiseオブジェクトでない場合はエラー
  if (thisObj == nullptr || !thisObj->isPromise()) {
    throw TypeException("Promise.prototype.finally called on an object that is not a Promise");
  }
  
  PromiseObject* promise = static_cast<PromiseObject*>(thisObj);
  
  Value onFinally = args.size() > 0 && args[0].isFunction() ? args[0] : Value::undefined();
  
  // finally呼び出し
  PromiseObject* resultPromise = promise->finally_(onFinally);
  return Value(resultPromise);
}

// Promiseプロトタイプを初期化
void initPromisePrototype(GlobalObject* globalObj) {
  // Promiseプロトタイプオブジェクトがまだ存在しない場合、作成
  if (PromiseObject::s_prototype == nullptr) {
    PromiseObject::s_prototype = new Object(globalObj->getObjectPrototype());
    
    // プロトタイプメソッドを設定
    PromiseObject::s_prototype->defineNativeFunction("then", promiseThen, 2);
    PromiseObject::s_prototype->defineNativeFunction("catch", promiseCatch, 1);
    PromiseObject::s_prototype->defineNativeFunction("finally", promiseFinally, 1);
    
    // Symbol.speciesゲッターを設定
    PromiseObject::s_prototype->defineProperty(
        globalObj->getSymbolRegistry()->getSymbol("species"),
        PropertyDescriptor(
            FunctionObject::create([](const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) -> Value {
              return Value(globalObj->getPromiseConstructor());
            }, "get [Symbol.species]", 0, globalObj),
            nullptr, false, false, true));
    
    // プロトタイプにPromiseを設定
    PromiseObject::s_prototype->defineProperty(
        "constructor",
        PropertyDescriptor(Value(globalObj->getPromiseConstructor()), nullptr, false, false, true));
  }
}

// Promiseオブジェクトを初期化
void initPromiseObject(GlobalObject* globalObj) {
  // Promiseプロトタイプを初期化
  initPromisePrototype(globalObj);
  
  // Promiseコンストラクタを作成
  Object* promiseConstructorObj = new Object(globalObj->getFunctionPrototype());
  promiseConstructorObj->setIsConstructor(true);
  
  // 静的メソッドを設定
  promiseConstructorObj->defineNativeFunction("resolve", promiseResolve, 1);
  promiseConstructorObj->defineNativeFunction("reject", promiseReject, 1);
  promiseConstructorObj->defineNativeFunction("all", promiseAll, 1);
  promiseConstructorObj->defineNativeFunction("race", promiseRace, 1);
  promiseConstructorObj->defineNativeFunction("allSettled", promiseAllSettled, 1);
  promiseConstructorObj->defineNativeFunction("any", promiseAny, 1);
  
  // Symbol.speciesを設定
  promiseConstructorObj->defineProperty(
      globalObj->getSymbolRegistry()->getSymbol("species"),
      PropertyDescriptor(Value(promiseConstructorObj), nullptr, false, false, true));
  
  // Promiseプロトタイプを設定
  promiseConstructorObj->defineProperty(
      "prototype",
      PropertyDescriptor(Value(PromiseObject::s_prototype), nullptr, false, false, false));
  
  // グローバルオブジェクトにPromiseコンストラクタを設定
  globalObj->defineProperty(
      "Promise",
      PropertyDescriptor(Value(promiseConstructorObj), nullptr, false, false, true));
}

// Promise.all 静的メソッド
Value promiseAll(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがPromiseコンストラクタでない場合はエラー
  if (thisObj == nullptr || !thisObj->isConstructor()) {
    throw TypeException("Promise.all called on an object that is not a constructor");
  }

  // 引数が与えられていない場合は空配列を使用
  Value iterable = args.empty() ? ArrayObject::create(globalObj) : args[0];

  // イテレーターを取得
  Value iterator;
  try {
    Value getIterator = globalObj->getSymbolRegistry()->getIteratorMethod(iterable);
    if (!getIterator.isFunction()) {
      throw TypeException("Symbol.iterator is not a function");
    }
    iterator = getIterator.call(iterable, {}, globalObj);
  } catch (const Exception& e) {
    return promiseReject({e.getValue()}, thisObj, globalObj);
  }

  // Promise.allを実行するエグゼキュータ関数
  auto executor = [thisObj, globalObj, iterator](const std::vector<Value>& executorArgs, Object* /*executorThis*/, GlobalObject* /*executorGlobal*/) -> Value {
    Value resolve = executorArgs[0];
    Value reject = executorArgs[1];

    // 結果配列
    ArrayObject* results = new ArrayObject(globalObj);
    size_t remainingCount = 0;
    bool isDone = false;

    // イテレーターから値を順に取得
    while (true) {
      try {
        Value next;
        try {
          Value nextMethod = iterator.asObject()->get("next");
          next = nextMethod.call(iterator, {}, globalObj);
        } catch (const Exception& e) {
          // イテレーターのnextメソッドで例外が発生した場合、Promiseを拒否
          std::vector<Value> rejectArgs = {e.getValue()};
          reject.call(Value::undefined(), rejectArgs, globalObj);
          isDone = true;
          break;
        }

        // イテレーションが終了したか確認
        Value done = next.asObject()->get("done");
        if (done.toBoolean()) {
          break;
        }

        // 次の値を取得
        Value nextValue = next.asObject()->get("value");
        
        // インデックスを保存
        size_t index = remainingCount;
        remainingCount++;
        
        // thisに対してresolveメソッドを呼び出す
        Value resolveMethod = thisObj->get("resolve");
        Value nextPromise = resolveMethod.call(thisObj, {nextValue}, globalObj);
        
        // thenメソッドを使用して結果を処理
        auto onFulfilled = [results, index, remainingCount, resolve, globalObj](const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
          // 結果を配列に格納
          Value value = args.empty() ? Value::undefined() : args[0];
          results->set(index, value);
          
          // すべての結果が揃ったらPromiseを解決
          if (--remainingCount == 0) {
            std::vector<Value> resolveArgs = {Value(results)};
            resolve.call(Value::undefined(), resolveArgs, globalObj);
          }
          
          return Value::undefined();
        };
        
        Value thenMethod = nextPromise.asObject()->get("then");
        thenMethod.call(nextPromise, {
          FunctionObject::create(onFulfilled, "promiseAllFulfilled", 1, globalObj),
          reject
        }, globalObj);
        
      } catch (const Exception& e) {
        // イテレーション中に例外が発生した場合、Promiseを拒否
        if (!isDone) {
          std::vector<Value> rejectArgs = {e.getValue()};
          reject.call(Value::undefined(), rejectArgs, globalObj);
          isDone = true;
        }
        break;
      }
    }
    
    // イテレーションが空だった場合、即座に空配列で解決
    if (remainingCount == 0 && !isDone) {
      std::vector<Value> resolveArgs = {Value(results)};
      resolve.call(Value::undefined(), resolveArgs, globalObj);
    }
    
    return Value::undefined();
  };
  
  // 新しいPromiseを作成
  PromiseObject* promise = new PromiseObject(
      FunctionObject::create(executor, "allExecutor", 2, globalObj),
      globalObj);
  return Value(promise);
}

// Promise.race 静的メソッド
Value promiseRace(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがPromiseコンストラクタでない場合はエラー
  if (thisObj == nullptr || !thisObj->isConstructor()) {
    throw TypeException("Promise.race called on an object that is not a constructor");
  }

  // 引数が与えられていない場合は空配列を使用
  Value iterable = args.empty() ? ArrayObject::create(globalObj) : args[0];

  // イテレーターを取得
  Value iterator;
  try {
    Value getIterator = globalObj->getSymbolRegistry()->getIteratorMethod(iterable);
    if (!getIterator.isFunction()) {
      throw TypeException("Symbol.iterator is not a function");
    }
    iterator = getIterator.call(iterable, {}, globalObj);
  } catch (const Exception& e) {
    return promiseReject({e.getValue()}, thisObj, globalObj);
  }

  // Promise.raceを実行するエグゼキュータ関数
  auto executor = [thisObj, globalObj, iterator](const std::vector<Value>& executorArgs, Object* /*executorThis*/, GlobalObject* /*executorGlobal*/) -> Value {
    Value resolve = executorArgs[0];
    Value reject = executorArgs[1];
    bool isDone = false;

    // イテレーターから値を順に取得
    while (true) {
      try {
        Value next;
        try {
          Value nextMethod = iterator.asObject()->get("next");
          next = nextMethod.call(iterator, {}, globalObj);
        } catch (const Exception& e) {
          // イテレーターのnextメソッドで例外が発生した場合、Promiseを拒否（まだ決定されていない場合）
          if (!isDone) {
            std::vector<Value> rejectArgs = {e.getValue()};
            reject.call(Value::undefined(), rejectArgs, globalObj);
            isDone = true;
          }
          break;
        }

        // イテレーションが終了したか確認
        Value done = next.asObject()->get("done");
        if (done.toBoolean()) {
          break;
        }

        // 次の値を取得
        Value nextValue = next.asObject()->get("value");
        
        // thisに対してresolveメソッドを呼び出す
        Value resolveMethod = thisObj->get("resolve");
        Value nextPromise = resolveMethod.call(thisObj, {nextValue}, globalObj);
        
        // thenメソッドを使用して結果を処理
        Value thenMethod = nextPromise.asObject()->get("then");
        thenMethod.call(nextPromise, {resolve, reject}, globalObj);
        
      } catch (const Exception& e) {
        // イテレーション中に例外が発生した場合、Promiseを拒否（まだ決定されていない場合）
        if (!isDone) {
          std::vector<Value> rejectArgs = {e.getValue()};
          reject.call(Value::undefined(), rejectArgs, globalObj);
          isDone = true;
        }
        break;
      }
    }
    
    return Value::undefined();
  };
  
  // 新しいPromiseを作成
  PromiseObject* promise = new PromiseObject(
      FunctionObject::create(executor, "raceExecutor", 2, globalObj),
      globalObj);
  return Value(promise);
}

// Promise.allSettled 静的メソッド
Value promiseAllSettled(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがPromiseコンストラクタでない場合はエラー
  if (thisObj == nullptr || !thisObj->isConstructor()) {
    throw TypeException("Promise.allSettled called on an object that is not a constructor");
  }

  // 引数が与えられていない場合は空配列を使用
  Value iterable = args.empty() ? ArrayObject::create(globalObj) : args[0];

  // イテレーターを取得
  Value iterator;
  try {
    Value getIterator = globalObj->getSymbolRegistry()->getIteratorMethod(iterable);
    if (!getIterator.isFunction()) {
      throw TypeException("Symbol.iterator is not a function");
    }
    iterator = getIterator.call(iterable, {}, globalObj);
  } catch (const Exception& e) {
    return promiseReject({e.getValue()}, thisObj, globalObj);
  }

  // Promise.allSettledを実行するエグゼキュータ関数
  auto executor = [thisObj, globalObj, iterator](const std::vector<Value>& executorArgs, Object* /*executorThis*/, GlobalObject* /*executorGlobal*/) -> Value {
    Value resolve = executorArgs[0];
    Value reject = executorArgs[1];

    // 結果配列
    ArrayObject* results = new ArrayObject(globalObj);
    size_t remainingCount = 0;
    bool isDone = false;

    // イテレーターから値を順に取得
    while (true) {
      try {
        Value next;
        try {
          Value nextMethod = iterator.asObject()->get("next");
          next = nextMethod.call(iterator, {}, globalObj);
        } catch (const Exception& e) {
          // イテレーターのnextメソッドで例外が発生した場合、Promiseを拒否
          std::vector<Value> rejectArgs = {e.getValue()};
          reject.call(Value::undefined(), rejectArgs, globalObj);
          isDone = true;
          break;
        }

        // イテレーションが終了したか確認
        Value done = next.asObject()->get("done");
        if (done.toBoolean()) {
          break;
        }

        // 次の値を取得
        Value nextValue = next.asObject()->get("value");
        
        // インデックスを保存
        size_t index = remainingCount;
        remainingCount++;
        
        // thisに対してresolveメソッドを呼び出す
        Value resolveMethod = thisObj->get("resolve");
        Value nextPromise = resolveMethod.call(thisObj, {nextValue}, globalObj);
        
        // 成功ハンドラ
        auto onFulfilled = [results, index, remainingCount, resolve, globalObj](const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
          // 成功結果オブジェクトを作成
          Object* resultObj = new Object(globalObj->getObjectPrototype());
          Value value = args.empty() ? Value::undefined() : args[0];
          
          resultObj->set("status", Value("fulfilled"));
          resultObj->set("value", value);
          
          // 結果を配列に格納
          results->set(index, Value(resultObj));
          
          // すべての結果が揃ったらPromiseを解決
          if (--remainingCount == 0) {
            std::vector<Value> resolveArgs = {Value(results)};
            resolve.call(Value::undefined(), resolveArgs, globalObj);
          }
          
          return Value::undefined();
        };
        
        // 失敗ハンドラ
        auto onRejected = [results, index, remainingCount, resolve, globalObj](const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
          // 失敗結果オブジェクトを作成
          Object* resultObj = new Object(globalObj->getObjectPrototype());
          Value reason = args.empty() ? Value::undefined() : args[0];
          
          resultObj->set("status", Value("rejected"));
          resultObj->set("reason", reason);
          
          // 結果を配列に格納
          results->set(index, Value(resultObj));
          
          // すべての結果が揃ったらPromiseを解決
          if (--remainingCount == 0) {
            std::vector<Value> resolveArgs = {Value(results)};
            resolve.call(Value::undefined(), resolveArgs, globalObj);
          }
          
          return Value::undefined();
        };
        
        Value thenMethod = nextPromise.asObject()->get("then");
        thenMethod.call(nextPromise, {
          FunctionObject::create(onFulfilled, "allSettledFulfilled", 1, globalObj),
          FunctionObject::create(onRejected, "allSettledRejected", 1, globalObj)
        }, globalObj);
        
      } catch (const Exception& e) {
        // イテレーション中に例外が発生した場合、Promiseを拒否
        if (!isDone) {
          std::vector<Value> rejectArgs = {e.getValue()};
          reject.call(Value::undefined(), rejectArgs, globalObj);
          isDone = true;
        }
        break;
      }
    }
    
    // イテレーションが空だった場合、即座に空配列で解決
    if (remainingCount == 0 && !isDone) {
      std::vector<Value> resolveArgs = {Value(results)};
      resolve.call(Value::undefined(), resolveArgs, globalObj);
    }
    
    return Value::undefined();
  };
  
  // 新しいPromiseを作成
  PromiseObject* promise = new PromiseObject(
      FunctionObject::create(executor, "allSettledExecutor", 2, globalObj),
      globalObj);
  return Value(promise);
}

// Promise.any 静的メソッド
Value promiseAny(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがPromiseコンストラクタでない場合はエラー
  if (thisObj == nullptr || !thisObj->isConstructor()) {
    throw TypeException("Promise.any called on an object that is not a constructor");
  }

  // 引数が与えられていない場合は空配列を使用
  Value iterable = args.empty() ? ArrayObject::create(globalObj) : args[0];

  // イテレーターを取得
  Value iterator;
  try {
    Value getIterator = globalObj->getSymbolRegistry()->getIteratorMethod(iterable);
    if (!getIterator.isFunction()) {
      throw TypeException("Symbol.iterator is not a function");
    }
    iterator = getIterator.call(iterable, {}, globalObj);
  } catch (const Exception& e) {
    return promiseReject({e.getValue()}, thisObj, globalObj);
  }

  // Promise.anyを実行するエグゼキュータ関数
  auto executor = [thisObj, globalObj, iterator](const std::vector<Value>& executorArgs, Object* /*executorThis*/, GlobalObject* /*executorGlobal*/) -> Value {
    Value resolve = executorArgs[0];
    Value reject = executorArgs[1];

    // エラー配列
    ArrayObject* errors = new ArrayObject(globalObj);
    size_t remainingCount = 0;
    bool isDone = false;

    // イテレーターから値を順に取得
    while (true) {
      try {
        Value next;
        try {
          Value nextMethod = iterator.asObject()->get("next");
          next = nextMethod.call(iterator, {}, globalObj);
        } catch (const Exception& e) {
          // イテレーターのnextメソッドで例外が発生した場合、すでに完了していなければエラー
          if (!isDone) {
            // まだPromiseがない場合のみAggregateErrorを作成
            if (remainingCount == 0) {
              // AggregateErrorを作成
              Object* aggregateError = new Object(globalObj->getErrorPrototype());
              aggregateError->set("name", Value("AggregateError"));
              aggregateError->set("message", Value("All promises were rejected"));
              aggregateError->set("errors", Value(errors));
              
              // 拒否
              std::vector<Value> rejectArgs = {Value(aggregateError)};
              reject.call(Value::undefined(), rejectArgs, globalObj);
            }
            isDone = true;
          }
          break;
        }

        // イテレーションが終了したか確認
        Value done = next.asObject()->get("done");
        if (done.toBoolean()) {
          break;
        }

        // 次の値を取得
        Value nextValue = next.asObject()->get("value");
        
        // インデックスを保存
        size_t index = remainingCount;
        remainingCount++;
        
        // thisに対してresolveメソッドを呼び出す
        Value resolveMethod = thisObj->get("resolve");
        Value nextPromise = resolveMethod.call(thisObj, {nextValue}, globalObj);
        
        // 成功ハンドラはそのまま解決に使用
        
        // 失敗ハンドラ
        auto onRejected = [errors, index, remainingCount, reject, globalObj](const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
          // エラーを配列に格納
          Value reason = args.empty() ? Value::undefined() : args[0];
          errors->set(index, reason);
          
          // すべてのPromiseが拒否された場合、AggregateErrorを作成して拒否
          if (--remainingCount == 0) {
            // AggregateErrorを作成
            Object* aggregateError = new Object(globalObj->getErrorPrototype());
            aggregateError->set("name", Value("AggregateError"));
            aggregateError->set("message", Value("All promises were rejected"));
            aggregateError->set("errors", Value(errors));
            
            // 拒否
            std::vector<Value> rejectArgs = {Value(aggregateError)};
            reject.call(Value::undefined(), rejectArgs, globalObj);
          }
          
          return Value::undefined();
        };
        
        Value thenMethod = nextPromise.asObject()->get("then");
        thenMethod.call(nextPromise, {
          resolve,  // いずれかのPromiseが解決されたら、全体のPromiseを解決
          FunctionObject::create(onRejected, "anyRejected", 1, globalObj)
        }, globalObj);
        
      } catch (const Exception& e) {
        // イテレーション中に例外が発生した場合
        if (!isDone) {
          // まだPromiseがない場合のみAggregateErrorを作成
          if (remainingCount == 0) {
            // AggregateErrorを作成
            Object* aggregateError = new Object(globalObj->getErrorPrototype());
            aggregateError->set("name", Value("AggregateError"));
            aggregateError->set("message", Value("All promises were rejected"));
            aggregateError->set("errors", Value(errors));
            
            // 拒否
            std::vector<Value> rejectArgs = {Value(aggregateError)};
            reject.call(Value::undefined(), rejectArgs, globalObj);
          }
          isDone = true;
        }
        break;
      }
    }
    
    // イテレーションが空だった場合、AggregateErrorで拒否
    if (remainingCount == 0 && !isDone) {
      // AggregateErrorを作成
      Object* aggregateError = new Object(globalObj->getErrorPrototype());
      aggregateError->set("name", Value("AggregateError"));
      aggregateError->set("message", Value("All promises were rejected"));
      aggregateError->set("errors", Value(errors));
      
      // 拒否
      std::vector<Value> rejectArgs = {Value(aggregateError)};
      reject.call(Value::undefined(), rejectArgs, globalObj);
    }
    
    return Value::undefined();
  };
  
  // 新しいPromiseを作成
  PromiseObject* promise = new PromiseObject(
      FunctionObject::create(executor, "anyExecutor", 2, globalObj),
      globalObj);
  return Value(promise);
}

} // namespace aero 