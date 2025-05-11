/**
 * @file promise_static.cpp
 * @brief JavaScriptのPromiseオブジェクトの静的メソッド実装
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

// Promise.all 静的メソッド
Value promiseAll(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがPromiseコンストラクタでない場合はエラー
  if (thisObj == nullptr || !thisObj->isConstructor()) {
    throw TypeException("Promise.all called on an object that is not a constructor");
  }
  
  // 引数がない場合はデフォルトでundefined
  Value iterable = args.empty() ? Value::undefined() : args[0];
  
  // iterableがnullまたはundefinedの場合はTypeError
  if (iterable.isNull() || iterable.isUndefined()) {
    throw TypeException("Promise.all called with invalid iterable");
  }
  
  // Promiseエグゼキュータ関数
  auto executor = [iterable, thisObj, globalObj](const std::vector<Value>& executorArgs, Object* /*executorThis*/, GlobalObject* /*executorGlobal*/) -> Value {
    Value resolve = executorArgs[0];
    Value reject = executorArgs[1];
    
    try {
      // イテレータを取得
      Value iteratorMethod = iterable.get(globalObj->getSymbolRegistry()->getSymbol("iterator"));
      if (!iteratorMethod.isFunction()) {
        throw TypeException("Object is not iterable");
      }
      
      Value iterator = iteratorMethod.call(iterable, {}, globalObj);
      
      // 結果配列とペンディングカウンタ
      ArrayObject* resultArray = new ArrayObject(globalObj);
      std::shared_ptr<size_t> remainingCount = std::make_shared<size_t>(0);
      std::shared_ptr<bool> alreadyRejected = std::make_shared<bool>(false);
      
      // イテレータが空の場合は空配列で即時解決
      Value nextMethod = iterator.get("next");
      if (!nextMethod.isFunction()) {
        std::vector<Value> resolveArgs = {Value(resultArray)};
        resolve.call(Value::undefined(), resolveArgs, globalObj);
        return Value::undefined();
      }
      
      Value nextResult = nextMethod.call(iterator, {}, globalObj);
      if (nextResult.get("done").truthy()) {
        std::vector<Value> resolveArgs = {Value(resultArray)};
        resolve.call(Value::undefined(), resolveArgs, globalObj);
        return Value::undefined();
      }
      
      // イテレータから要素を処理
      size_t index = 0;
      
      do {
        Value currentValue = nextResult.get("value");
        resultArray->defineProperty(std::to_string(index), PropertyDescriptor(Value::undefined(), nullptr, true, true, true));
        
        // 各要素をPromiseで解決
        (*remainingCount)++;
        
        // 現在の要素をPromiseで解決
        // Promise.resolve(currentValue)を取得
        Value promiseResolveMethod = thisObj->get("resolve");
        if (!promiseResolveMethod.isFunction()) {
          throw TypeException("Promise.resolve is not a function");
        }
        
        Value currentPromise = promiseResolveMethod.call(thisObj, {currentValue}, globalObj);
        
        // thenメソッドを呼び出し
        Value thenMethod = currentPromise.get("then");
        if (!thenMethod.isFunction()) {
          throw TypeException("Promise.then is not a function");
        }
        
        // 要素を解決したら、結果配列に追加
        auto onFulfilled = [resultArray, index, remainingCount, resolve, alreadyRejected, globalObj](
            const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
          // 既に拒否されていたら何もしない
          if (*alreadyRejected) {
            return Value::undefined();
          }
          
          // 結果を配列に保存
          Value value = args.empty() ? Value::undefined() : args[0];
          resultArray->set(std::to_string(index), value);
          
          // すべての要素が解決されたかチェック
          (*remainingCount)--;
          if (*remainingCount == 0) {
            std::vector<Value> resolveArgs = {Value(resultArray)};
            resolve.call(Value::undefined(), resolveArgs, globalObj);
          }
          
          return Value::undefined();
        };
        
        // いずれかの要素が拒否されたら、全体を拒否
        auto onRejected = [reject, alreadyRejected, globalObj](
            const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
          // 既に拒否されていたら何もしない
          if (*alreadyRejected) {
            return Value::undefined();
          }
          
          *alreadyRejected = true;
          Value reason = args.empty() ? Value::undefined() : args[0];
          
          std::vector<Value> rejectArgs = {reason};
          reject.call(Value::undefined(), rejectArgs, globalObj);
          
          return Value::undefined();
        };
        
        // thenを呼び出し
        Value onFulfilledFunc = FunctionObject::create(onFulfilled, "allOnFulfilled", 1, globalObj);
        Value onRejectedFunc = FunctionObject::create(onRejected, "allOnRejected", 1, globalObj);
        thenMethod.call(currentPromise, {onFulfilledFunc, onRejectedFunc}, globalObj);
        
        // 次の要素を取得
        nextResult = nextMethod.call(iterator, {}, globalObj);
        index++;
        
      } while (!nextResult.get("done").truthy());
      
      // すべての要素が同期的に解決された場合
      if (*remainingCount == 0) {
        std::vector<Value> resolveArgs = {Value(resultArray)};
        resolve.call(Value::undefined(), resolveArgs, globalObj);
      }
      
    } catch (const Exception& e) {
      // イテレータの取得や処理中にエラーが発生した場合
      std::vector<Value> rejectArgs = {e.getValue()};
      reject.call(Value::undefined(), rejectArgs, globalObj);
    }
    
    return Value::undefined();
  };
  
  // Promiseを作成して返す
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
  
  // 引数がない場合はデフォルトでundefined
  Value iterable = args.empty() ? Value::undefined() : args[0];
  
  // iterableがnullまたはundefinedの場合はTypeError
  if (iterable.isNull() || iterable.isUndefined()) {
    throw TypeException("Promise.race called with invalid iterable");
  }
  
  // Promiseエグゼキュータ関数
  auto executor = [iterable, thisObj, globalObj](const std::vector<Value>& executorArgs, Object* /*executorThis*/, GlobalObject* /*executorGlobal*/) -> Value {
    Value resolve = executorArgs[0];
    Value reject = executorArgs[1];
    
    try {
      // イテレータを取得
      Value iteratorMethod = iterable.get(globalObj->getSymbolRegistry()->getSymbol("iterator"));
      if (!iteratorMethod.isFunction()) {
        throw TypeException("Object is not iterable");
      }
      
      Value iterator = iteratorMethod.call(iterable, {}, globalObj);
      
      // イテレータが空の場合はペンディングのままとなる（仕様通り）
      Value nextMethod = iterator.get("next");
      if (!nextMethod.isFunction()) {
        return Value::undefined();
      }
      
      // 既に決定済みかどうかを追跡するフラグ
      std::shared_ptr<bool> alreadySettled = std::make_shared<bool>(false);
      
      // イテレータから要素を処理
      Value nextResult = nextMethod.call(iterator, {}, globalObj);
      
      while (!nextResult.get("done").truthy()) {
        Value currentValue = nextResult.get("value");
        
        // Promise.resolve(currentValue)を取得
        Value promiseResolveMethod = thisObj->get("resolve");
        if (!promiseResolveMethod.isFunction()) {
          throw TypeException("Promise.resolve is not a function");
        }
        
        Value currentPromise = promiseResolveMethod.call(thisObj, {currentValue}, globalObj);
        
        // thenメソッドを呼び出し
        Value thenMethod = currentPromise.get("then");
        if (!thenMethod.isFunction()) {
          throw TypeException("Promise.then is not a function");
        }
        
        // 最初に決定された要素でPromiseを解決/拒否
        auto onFulfilled = [resolve, alreadySettled, globalObj](
            const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
          // 既に決定されていたら何もしない
          if (*alreadySettled) {
            return Value::undefined();
          }
          
          *alreadySettled = true;
          Value value = args.empty() ? Value::undefined() : args[0];
          
          std::vector<Value> resolveArgs = {value};
          resolve.call(Value::undefined(), resolveArgs, globalObj);
          
          return Value::undefined();
        };
        
        auto onRejected = [reject, alreadySettled, globalObj](
            const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
          // 既に決定されていたら何もしない
          if (*alreadySettled) {
            return Value::undefined();
          }
          
          *alreadySettled = true;
          Value reason = args.empty() ? Value::undefined() : args[0];
          
          std::vector<Value> rejectArgs = {reason};
          reject.call(Value::undefined(), rejectArgs, globalObj);
          
          return Value::undefined();
        };
        
        // thenを呼び出し
        Value onFulfilledFunc = FunctionObject::create(onFulfilled, "raceOnFulfilled", 1, globalObj);
        Value onRejectedFunc = FunctionObject::create(onRejected, "raceOnRejected", 1, globalObj);
        thenMethod.call(currentPromise, {onFulfilledFunc, onRejectedFunc}, globalObj);
        
        // 次の要素を取得
        nextResult = nextMethod.call(iterator, {}, globalObj);
      }
      
    } catch (const Exception& e) {
      // イテレータの取得や処理中にエラーが発生した場合
      std::vector<Value> rejectArgs = {e.getValue()};
      reject.call(Value::undefined(), rejectArgs, globalObj);
    }
    
    return Value::undefined();
  };
  
  // Promiseを作成して返す
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
  
  // 引数がない場合はデフォルトでundefined
  Value iterable = args.empty() ? Value::undefined() : args[0];
  
  // iterableがnullまたはundefinedの場合はTypeError
  if (iterable.isNull() || iterable.isUndefined()) {
    throw TypeException("Promise.allSettled called with invalid iterable");
  }
  
  // Promiseエグゼキュータ関数
  auto executor = [iterable, thisObj, globalObj](const std::vector<Value>& executorArgs, Object* /*executorThis*/, GlobalObject* /*executorGlobal*/) -> Value {
    Value resolve = executorArgs[0];
    Value reject = executorArgs[1];
    
    try {
      // イテレータを取得
      Value iteratorMethod = iterable.get(globalObj->getSymbolRegistry()->getSymbol("iterator"));
      if (!iteratorMethod.isFunction()) {
        throw TypeException("Object is not iterable");
      }
      
      Value iterator = iteratorMethod.call(iterable, {}, globalObj);
      
      // 結果配列とペンディングカウンタ
      ArrayObject* resultArray = new ArrayObject(globalObj);
      std::shared_ptr<size_t> remainingCount = std::make_shared<size_t>(0);
      
      // イテレータが空の場合は空配列で即時解決
      Value nextMethod = iterator.get("next");
      if (!nextMethod.isFunction()) {
        std::vector<Value> resolveArgs = {Value(resultArray)};
        resolve.call(Value::undefined(), resolveArgs, globalObj);
        return Value::undefined();
      }
      
      Value nextResult = nextMethod.call(iterator, {}, globalObj);
      if (nextResult.get("done").truthy()) {
        std::vector<Value> resolveArgs = {Value(resultArray)};
        resolve.call(Value::undefined(), resolveArgs, globalObj);
        return Value::undefined();
      }
      
      // イテレータから要素を処理
      size_t index = 0;
      
      do {
        Value currentValue = nextResult.get("value");
        resultArray->defineProperty(std::to_string(index), PropertyDescriptor(Value::undefined(), nullptr, true, true, true));
        
        // 各要素をPromiseで解決
        (*remainingCount)++;
        
        // Promise.resolve(currentValue)を取得
        Value promiseResolveMethod = thisObj->get("resolve");
        if (!promiseResolveMethod.isFunction()) {
          throw TypeException("Promise.resolve is not a function");
        }
        
        Value currentPromise = promiseResolveMethod.call(thisObj, {currentValue}, globalObj);
        
        // thenメソッドを呼び出し
        Value thenMethod = currentPromise.get("then");
        if (!thenMethod.isFunction()) {
          throw TypeException("Promise.then is not a function");
        }
        
        // 要素を解決したら、結果オブジェクトを配列に追加
        auto onFulfilled = [resultArray, index, remainingCount, resolve, globalObj](
            const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
          // status: 'fulfilled', value: valueの結果オブジェクトを作成
          Object* resultObj = new Object(globalObj->getObjectPrototype());
          resultObj->defineProperty("status", PropertyDescriptor(Value("fulfilled"), nullptr, true, true, true));
          
          Value value = args.empty() ? Value::undefined() : args[0];
          resultObj->defineProperty("value", PropertyDescriptor(value, nullptr, true, true, true));
          
          // 結果を配列に保存
          resultArray->set(std::to_string(index), Value(resultObj));
          
          // すべての要素が処理されたかチェック
          (*remainingCount)--;
          if (*remainingCount == 0) {
            std::vector<Value> resolveArgs = {Value(resultArray)};
            resolve.call(Value::undefined(), resolveArgs, globalObj);
          }
          
          return Value::undefined();
        };
        
        // 要素が拒否されても、結果オブジェクトを配列に追加
        auto onRejected = [resultArray, index, remainingCount, resolve, globalObj](
            const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
          // status: 'rejected', reason: reasonの結果オブジェクトを作成
          Object* resultObj = new Object(globalObj->getObjectPrototype());
          resultObj->defineProperty("status", PropertyDescriptor(Value("rejected"), nullptr, true, true, true));
          
          Value reason = args.empty() ? Value::undefined() : args[0];
          resultObj->defineProperty("reason", PropertyDescriptor(reason, nullptr, true, true, true));
          
          // 結果を配列に保存
          resultArray->set(std::to_string(index), Value(resultObj));
          
          // すべての要素が処理されたかチェック
          (*remainingCount)--;
          if (*remainingCount == 0) {
            std::vector<Value> resolveArgs = {Value(resultArray)};
            resolve.call(Value::undefined(), resolveArgs, globalObj);
          }
          
          return Value::undefined();
        };
        
        // thenを呼び出し
        Value onFulfilledFunc = FunctionObject::create(onFulfilled, "allSettledOnFulfilled", 1, globalObj);
        Value onRejectedFunc = FunctionObject::create(onRejected, "allSettledOnRejected", 1, globalObj);
        thenMethod.call(currentPromise, {onFulfilledFunc, onRejectedFunc}, globalObj);
        
        // 次の要素を取得
        nextResult = nextMethod.call(iterator, {}, globalObj);
        index++;
        
      } while (!nextResult.get("done").truthy());
      
      // すべての要素が同期的に処理された場合
      if (*remainingCount == 0) {
        std::vector<Value> resolveArgs = {Value(resultArray)};
        resolve.call(Value::undefined(), resolveArgs, globalObj);
      }
      
    } catch (const Exception& e) {
      // イテレータの取得や処理中にエラーが発生した場合
      std::vector<Value> rejectArgs = {e.getValue()};
      reject.call(Value::undefined(), rejectArgs, globalObj);
    }
    
    return Value::undefined();
  };
  
  // Promiseを作成して返す
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
  
  // 引数がない場合はデフォルトでundefined
  Value iterable = args.empty() ? Value::undefined() : args[0];
  
  // iterableがnullまたはundefinedの場合はTypeError
  if (iterable.isNull() || iterable.isUndefined()) {
    throw TypeException("Promise.any called with invalid iterable");
  }
  
  // Promiseエグゼキュータ関数
  auto executor = [iterable, thisObj, globalObj](const std::vector<Value>& executorArgs, Object* /*executorThis*/, GlobalObject* /*executorGlobal*/) -> Value {
    Value resolve = executorArgs[0];
    Value reject = executorArgs[1];
    
    try {
      // イテレータを取得
      Value iteratorMethod = iterable.get(globalObj->getSymbolRegistry()->getSymbol("iterator"));
      if (!iteratorMethod.isFunction()) {
        throw TypeException("Object is not iterable");
      }
      
      Value iterator = iteratorMethod.call(iterable, {}, globalObj);
      
      // 拒否理由配列とペンディングカウンタ
      ArrayObject* errorsArray = new ArrayObject(globalObj);
      std::shared_ptr<size_t> remainingCount = std::make_shared<size_t>(0);
      std::shared_ptr<bool> alreadyResolved = std::make_shared<bool>(false);
      
      // イテレータが空の場合はAggregateErrorで拒否
      Value nextMethod = iterator.get("next");
      if (!nextMethod.isFunction()) {
        // AggregateErrorを作成
        Object* aggregateError = createAggregateError(errorsArray, "All promises were rejected", globalObj);
        
        std::vector<Value> rejectArgs = {Value(aggregateError)};
        reject.call(Value::undefined(), rejectArgs, globalObj);
        return Value::undefined();
      }
      
      Value nextResult = nextMethod.call(iterator, {}, globalObj);
      if (nextResult.get("done").truthy()) {
        // AggregateErrorを作成
        Object* aggregateError = createAggregateError(errorsArray, "All promises were rejected", globalObj);
        
        std::vector<Value> rejectArgs = {Value(aggregateError)};
        reject.call(Value::undefined(), rejectArgs, globalObj);
        return Value::undefined();
      }
      
      // イテレータから要素を処理
      size_t index = 0;
      
      do {
        Value currentValue = nextResult.get("value");
        
        // 各要素をPromiseで解決
        (*remainingCount)++;
        
        // Promise.resolve(currentValue)を取得
        Value promiseResolveMethod = thisObj->get("resolve");
        if (!promiseResolveMethod.isFunction()) {
          throw TypeException("Promise.resolve is not a function");
        }
        
        Value currentPromise = promiseResolveMethod.call(thisObj, {currentValue}, globalObj);
        
        // thenメソッドを呼び出し
        Value thenMethod = currentPromise.get("then");
        if (!thenMethod.isFunction()) {
          throw TypeException("Promise.then is not a function");
        }
        
        // いずれかの要素が解決されたら、全体を解決
        auto onFulfilled = [resolve, alreadyResolved, globalObj](
            const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
          // 既に解決されていたら何もしない
          if (*alreadyResolved) {
            return Value::undefined();
          }
          
          *alreadyResolved = true;
          Value value = args.empty() ? Value::undefined() : args[0];
          
          std::vector<Value> resolveArgs = {value};
          resolve.call(Value::undefined(), resolveArgs, globalObj);
          
          return Value::undefined();
        };
        
        // すべての要素が拒否されたら、AggregateErrorで拒否
        auto onRejected = [errorsArray, index, remainingCount, reject, alreadyResolved, globalObj](
            const std::vector<Value>& args, Object* /*thisObj*/, GlobalObject* /*globalObj*/) -> Value {
          // 既に解決されていたら何もしない
          if (*alreadyResolved) {
            return Value::undefined();
          }
          
          // 拒否理由を配列に保存
          Value reason = args.empty() ? Value::undefined() : args[0];
          errorsArray->defineProperty(std::to_string(index), PropertyDescriptor(reason, nullptr, true, true, true));
          
          // すべての要素が拒否されたかチェック
          (*remainingCount)--;
          if (*remainingCount == 0) {
            *alreadyResolved = true;
            
            // AggregateErrorを作成
            Object* aggregateError = createAggregateError(errorsArray, "All promises were rejected", globalObj);
            
            std::vector<Value> rejectArgs = {Value(aggregateError)};
            reject.call(Value::undefined(), rejectArgs, globalObj);
          }
          
          return Value::undefined();
        };
        
        // thenを呼び出し
        Value onFulfilledFunc = FunctionObject::create(onFulfilled, "anyOnFulfilled", 1, globalObj);
        Value onRejectedFunc = FunctionObject::create(onRejected, "anyOnRejected", 1, globalObj);
        thenMethod.call(currentPromise, {onFulfilledFunc, onRejectedFunc}, globalObj);
        
        // 次の要素を取得
        nextResult = nextMethod.call(iterator, {}, globalObj);
        index++;
        
      } while (!nextResult.get("done").truthy());
      
      // すべての要素が同期的に拒否された場合
      if (*remainingCount == 0 && !(*alreadyResolved)) {
        *alreadyResolved = true;
        
        // AggregateErrorを作成
        Object* aggregateError = createAggregateError(errorsArray, "All promises were rejected", globalObj);
        
        std::vector<Value> rejectArgs = {Value(aggregateError)};
        reject.call(Value::undefined(), rejectArgs, globalObj);
      }
      
    } catch (const Exception& e) {
      // イテレータの取得や処理中にエラーが発生した場合
      std::vector<Value> rejectArgs = {e.getValue()};
      reject.call(Value::undefined(), rejectArgs, globalObj);
    }
    
    return Value::undefined();
  };
  
  // Promiseを作成して返す
  PromiseObject* promise = new PromiseObject(
      FunctionObject::create(executor, "anyExecutor", 2, globalObj),
      globalObj);
  return Value(promise);
}

// AggregateError作成ヘルパー関数
Object* createAggregateError(ArrayObject* errors, const std::string& message, GlobalObject* globalObj) {
  // エラーオブジェクトを作成
  Object* aggregateError = new Object(globalObj->getErrorPrototype("AggregateError"));
  
  // プロパティを設定
  aggregateError->defineProperty("message", PropertyDescriptor(Value(message), nullptr, true, true, true));
  aggregateError->defineProperty("errors", PropertyDescriptor(Value(errors), nullptr, true, true, true));
  aggregateError->defineProperty("name", PropertyDescriptor(Value("AggregateError"), nullptr, true, true, true));
  
  return aggregateError;
}

} // namespace aero 