/**
 * @file promise.h
 * @brief Promise実装（ES6+）
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include "../../values/value.h"
#include "../../context/execution_context.h"
#include <functional>
#include <vector>
#include <mutex>
#include <memory>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <thread>

namespace aerojs {
namespace core {
namespace runtime {
namespace builtins {

// Promise状態
enum class PromiseState {
  Pending,   // 保留中
  Fulfilled, // 成功
  Rejected   // 失敗
};

// Promise操作種別
enum class PromiseTaskType {
  Resolve,
  Reject,
  Then,
  Catch,
  Finally
};

// Promise操作
struct PromiseTask {
  PromiseTaskType type;
  Value value;
  Value onFulfilled;
  Value onRejected;
  class Promise* promise;
};

// Promiseハンドラ型定義
using PromiseCallback = std::function<Value(Value)>;

// Promiseマイクロタスクキュー
class PromiseJobQueue {
public:
  static PromiseJobQueue& getInstance();
  
  void enqueue(PromiseTask task);
  bool dequeue(PromiseTask& task);
  void processPendingJobs(execution::ExecutionContext* context);
  
private:
  PromiseJobQueue() = default;
  std::queue<PromiseTask> tasks;
  std::mutex queueMutex;
};

// Promise実装
class Promise {
public:
  Promise(execution::ExecutionContext* context);
  ~Promise();
  
  // 静的メソッド
  static Promise* resolve(execution::ExecutionContext* context, const Value& value);
  static Promise* reject(execution::ExecutionContext* context, const Value& reason);
  static Promise* all(execution::ExecutionContext* context, const std::vector<Value>& promises);
  static Promise* race(execution::ExecutionContext* context, const std::vector<Value>& promises);
  static Promise* allSettled(execution::ExecutionContext* context, const std::vector<Value>& promises);
  static Promise* any(execution::ExecutionContext* context, const std::vector<Value>& promises);
  
  // インスタンスメソッド
  Promise* then(const Value& onFulfilled, const Value& onRejected = Value::createUndefined());
  Promise* catchError(const Value& onRejected);
  Promise* finally(const Value& onFinally);
  
  // 内部メソッド
  void resolve(const Value& value);
  void reject(const Value& reason);
  
  // 状態アクセサ
  PromiseState getState() const;
  Value getValue() const;
  Value getReason() const;
  
  // プライベートメソッド
private:
  void transitionToFulfilled(const Value& value);
  void transitionToRejected(const Value& reason);
  void executeHandlers();
  Promise* createResolutionPromise(const Value& onFulfilled, const Value& onRejected);
  
  // プロパティ
private:
  PromiseState state;
  Value result;
  std::vector<std::pair<Value, Value>> handlers; // onFulfilled, onRejected のペア
  std::vector<Promise*> dependentPromises;
  execution::ExecutionContext* context;
  std::mutex mutex;
  std::atomic<bool> settled;
};

// Promise初期化関数
void initPromisePrototype(execution::ExecutionContext* context);

} // namespace builtins
} // namespace runtime
} // namespace core
} // namespace aerojs