/**
 * @file iteration.cpp
 * @brief JavaScriptのイテレーションプロトコルの実装
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#include "iteration.h"
#include <algorithm>
#include <array>
#include "../values/value.h"
#include "../builtins/array/array.h"
#include "../builtins/promise/promise.h"
#include "../error/error.h"
#include "../symbols/symbols.h"

namespace aero {

//-----------------------------------------------------------------------------
// IteratorResult クラスの実装
//-----------------------------------------------------------------------------

Object* IteratorResult::create(ExecutionContext* ctx, Value value, bool done) {
  Object* result = Object::create(ctx);
  
  // valueとdoneプロパティを設定
  result->defineProperty(ctx, "value", value, 
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Enumerable | 
          PropertyDescriptor::Configurable
      )
  );
  
  result->defineProperty(ctx, "done", Value::createBoolean(done), 
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Enumerable | 
          PropertyDescriptor::Configurable
      )
  );
  
  return result;
}

Object* IteratorResult::wrap(ExecutionContext* ctx, Object* obj, Value defaultValue, bool defaultDone) {
  // オブジェクトがnullの場合、新しいオブジェクトを作成
  if (!obj) {
    return create(ctx, defaultValue, defaultDone);
  }
  
  // valueプロパティが存在しない場合は追加
  if (!obj->hasProperty(ctx, "value")) {
    obj->defineProperty(ctx, "value", defaultValue, 
        PropertyDescriptor::createDataDescriptorFlags(
            PropertyDescriptor::Writable | 
            PropertyDescriptor::Enumerable | 
            PropertyDescriptor::Configurable
        )
    );
  }
  
  // doneプロパティが存在しない場合は追加
  if (!obj->hasProperty(ctx, "done")) {
    obj->defineProperty(ctx, "done", Value::createBoolean(defaultDone), 
        PropertyDescriptor::createDataDescriptorFlags(
            PropertyDescriptor::Writable | 
            PropertyDescriptor::Enumerable | 
            PropertyDescriptor::Configurable
        )
    );
  }
  
  return obj;
}

bool IteratorResult::isIteratorResult(ExecutionContext* ctx, Object* obj) {
  if (!obj) {
    return false;
  }
  
  return obj->hasProperty(ctx, "value") && obj->hasProperty(ctx, "done");
}

Value IteratorResult::getValue(ExecutionContext* ctx, Object* obj) {
  if (!obj) {
    return Value::createUndefined();
  }
  
  return obj->get(ctx, "value");
}

bool IteratorResult::getDone(ExecutionContext* ctx, Object* obj) {
  if (!obj) {
    return false;
  }
  
  Value done = obj->get(ctx, "done");
  return done.toBoolean();
}

//-----------------------------------------------------------------------------
// Iterable クラスの実装
//-----------------------------------------------------------------------------

Object* Iterable::getIterator(ExecutionContext* ctx, Value obj) {
  if (!obj.isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Object is not iterable"));
    return nullptr;
  }
  
  // Symbol.iteratorメソッドを取得
  Value iteratorMethod = obj.asObject()->get(ctx, Symbol::iterator);
  
  if (!iteratorMethod.isCallable()) {
    ctx->throwError(Error::createTypeError(ctx, "Object is not iterable"));
    return nullptr;
  }
  
  // Symbol.iteratorメソッドを呼び出してイテレーターを取得
  std::vector<Value> args;
  Value iteratorObj = iteratorMethod.asFunction()->call(ctx, obj, args);
  
  if (!iteratorObj.isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Result of Symbol.iterator is not an object"));
    return nullptr;
  }
  
  return iteratorObj.asObject();
}

bool Iterable::isIterable(ExecutionContext* ctx, Value obj) {
  if (!obj.isObject()) {
    return false;
  }
  
  // Symbol.iteratorメソッドがあるかどうかを確認
  Value iteratorMethod = obj.asObject()->get(ctx, Symbol::iterator);
  return iteratorMethod.isCallable();
}

Object* Iterable::getAsyncIterator(ExecutionContext* ctx, Value obj) {
  if (!obj.isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Object is not async iterable"));
    return nullptr;
  }
  
  // Symbol.asyncIteratorメソッドを取得
  Value iteratorMethod = obj.asObject()->get(ctx, Symbol::asyncIterator);
  
  if (!iteratorMethod.isCallable()) {
    ctx->throwError(Error::createTypeError(ctx, "Object is not async iterable"));
    return nullptr;
  }
  
  // Symbol.asyncIteratorメソッドを呼び出して非同期イテレーターを取得
  std::vector<Value> args;
  Value iteratorObj = iteratorMethod.asFunction()->call(ctx, obj, args);
  
  if (!iteratorObj.isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Result of Symbol.asyncIterator is not an object"));
    return nullptr;
  }
  
  return iteratorObj.asObject();
}

bool Iterable::isAsyncIterable(ExecutionContext* ctx, Value obj) {
  if (!obj.isObject()) {
    return false;
  }
  
  // Symbol.asyncIteratorメソッドがあるかどうかを確認
  Value iteratorMethod = obj.asObject()->get(ctx, Symbol::asyncIterator);
  return iteratorMethod.isCallable();
}

//-----------------------------------------------------------------------------
// Iterator クラスの実装
//-----------------------------------------------------------------------------

Object* Iterator::create(ExecutionContext* ctx, 
                        Function* nextMethod, 
                        Function* returnMethod, 
                        Function* throwMethod) {
  Object* iterator = Object::create(ctx);
  
  // nextメソッドを設定（必須）
  iterator->defineProperty(ctx, "next", Value(nextMethod), 
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  // returnメソッドを設定（オプション）
  if (returnMethod) {
    iterator->defineProperty(ctx, "return", Value(returnMethod), 
        PropertyDescriptor::createDataDescriptorFlags(
            PropertyDescriptor::Writable | 
            PropertyDescriptor::Configurable
        )
    );
  }
  
  // throwメソッドを設定（オプション）
  if (throwMethod) {
    iterator->defineProperty(ctx, "throw", Value(throwMethod), 
        PropertyDescriptor::createDataDescriptorFlags(
            PropertyDescriptor::Writable | 
            PropertyDescriptor::Configurable
        )
    );
  }
  
  return iterator;
}

Object* Iterator::wrap(ExecutionContext* ctx, Object* obj) {
  // オブジェクトがnullまたはnextメソッドがない場合はエラー
  if (!obj || !obj->hasProperty(ctx, "next")) {
    ctx->throwError(Error::createTypeError(ctx, "Object is not an iterator"));
    return nullptr;
  }
  
  // nextメソッドが関数かどうか確認
  Value nextMethod = obj->get(ctx, "next");
  if (!nextMethod.isCallable()) {
    ctx->throwError(Error::createTypeError(ctx, "Iterator.next is not callable"));
    return nullptr;
  }
  
  return obj;
}

bool Iterator::isIterator(ExecutionContext* ctx, Object* obj) {
  if (!obj) {
    return false;
  }
  
  // nextメソッドが存在するかどうかを確認
  Value nextMethod = obj->get(ctx, "next");
  return nextMethod.isCallable();
}

Object* Iterator::next(ExecutionContext* ctx, Object* iterator, Value value) {
  if (!iterator) {
    ctx->throwError(Error::createTypeError(ctx, "Iterator is null or undefined"));
    return nullptr;
  }
  
  // nextメソッドを取得
  Value nextMethod = iterator->get(ctx, "next");
  if (!nextMethod.isCallable()) {
    ctx->throwError(Error::createTypeError(ctx, "Iterator.next is not callable"));
    return nullptr;
  }
  
  // nextメソッドを呼び出す
  std::vector<Value> args;
  if (!value.isUndefined()) {
    args.push_back(value);
  }
  
  Value result = nextMethod.asFunction()->call(ctx, Value(iterator), args);
  
  // 結果がオブジェクトかどうかを確認
  if (!result.isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Iterator result is not an object"));
    return nullptr;
  }
  
  // イテレーター結果オブジェクトをラップして返す
  return IteratorResult::wrap(ctx, result.asObject());
}

Object* Iterator::returnIterator(ExecutionContext* ctx, Object* iterator, Value value) {
  if (!iterator) {
    return nullptr;
  }
  
  // returnメソッドがあるかどうかを確認
  Value returnMethod = iterator->get(ctx, "return");
  if (!returnMethod.isCallable()) {
    return nullptr;
  }
  
  // returnメソッドを呼び出す
  std::vector<Value> args;
  if (!value.isUndefined()) {
    args.push_back(value);
  }
  
  Value result = returnMethod.asFunction()->call(ctx, Value(iterator), args);
  
  // 結果がオブジェクトかどうかを確認
  if (!result.isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Iterator result is not an object"));
    return nullptr;
  }
  
  // イテレーター結果オブジェクトをラップして返す
  return IteratorResult::wrap(ctx, result.asObject());
}

Object* Iterator::throwIterator(ExecutionContext* ctx, Object* iterator, Value value) {
  if (!iterator) {
    return nullptr;
  }
  
  // throwメソッドがあるかどうかを確認
  Value throwMethod = iterator->get(ctx, "throw");
  if (!throwMethod.isCallable()) {
    return nullptr;
  }
  
  // throwメソッドを呼び出す
  std::vector<Value> args;
  if (!value.isUndefined()) {
    args.push_back(value);
  }
  
  Value result = throwMethod.asFunction()->call(ctx, Value(iterator), args);
  
  // 結果がオブジェクトかどうかを確認
  if (!result.isObject()) {
    ctx->throwError(Error::createTypeError(ctx, "Iterator result is not an object"));
    return nullptr;
  }
  
  // イテレーター結果オブジェクトをラップして返す
  return IteratorResult::wrap(ctx, result.asObject());
}

Array* Iterator::collectToArray(ExecutionContext* ctx, Object* iterator) {
  if (!iterator) {
    ctx->throwError(Error::createTypeError(ctx, "Iterator is null or undefined"));
    return nullptr;
  }
  
  // 配列を作成
  Array* array = Array::create(ctx);
  uint32_t index = 0;
  
  // イテレーターを完了まで実行
  while (true) {
    // nextメソッドを呼び出す
    Object* result = next(ctx, iterator);
    if (!result) {
      break;
    }
    
    // doneフラグを確認
    bool done = IteratorResult::getDone(ctx, result);
    if (done) {
      break;
    }
    
    // 値を配列に追加
    Value value = IteratorResult::getValue(ctx, result);
    array->defineProperty(ctx, std::to_string(index), value, 
        PropertyDescriptor::createDataDescriptorFlags(
            PropertyDescriptor::Writable | 
            PropertyDescriptor::Enumerable | 
            PropertyDescriptor::Configurable
        )
    );
    
    index++;
  }
  
  // 配列の長さを設定
  array->setLength(ctx, index);
  
  return array;
}

//-----------------------------------------------------------------------------
// AsyncIterator クラスの実装
//-----------------------------------------------------------------------------

Object* AsyncIterator::create(ExecutionContext* ctx, 
                            Function* nextMethod, 
                            Function* returnMethod, 
                            Function* throwMethod) {
  Object* iterator = Object::create(ctx);
  
  // nextメソッドを設定（必須）
  iterator->defineProperty(ctx, "next", Value(nextMethod), 
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  // returnメソッドを設定（オプション）
  if (returnMethod) {
    iterator->defineProperty(ctx, "return", Value(returnMethod), 
        PropertyDescriptor::createDataDescriptorFlags(
            PropertyDescriptor::Writable | 
            PropertyDescriptor::Configurable
        )
    );
  }
  
  // throwメソッドを設定（オプション）
  if (throwMethod) {
    iterator->defineProperty(ctx, "throw", Value(throwMethod), 
        PropertyDescriptor::createDataDescriptorFlags(
            PropertyDescriptor::Writable | 
            PropertyDescriptor::Configurable
        )
    );
  }
  
  return iterator;
}

bool AsyncIterator::isAsyncIterator(ExecutionContext* ctx, Object* obj) {
  if (!obj) {
    return false;
  }
  
  // nextメソッドが存在するかどうかを確認
  Value nextMethod = obj->get(ctx, "next");
  if (!nextMethod.isCallable()) {
    return false;
  }
  
  // 他のメソッド（returnやthrow）が存在する場合、それらも関数であることを確認
  Value returnMethod = obj->get(ctx, "return");
  if (!returnMethod.isUndefined() && !returnMethod.isCallable()) {
    return false;
  }
  
  Value throwMethod = obj->get(ctx, "throw");
  if (!throwMethod.isUndefined() && !throwMethod.isCallable()) {
    return false;
  }
  
  return true;
}

Object* AsyncIterator::next(ExecutionContext* ctx, Object* iterator, Value value) {
  if (!iterator) {
    ctx->throwError(Error::createTypeError(ctx, "AsyncIterator is null or undefined"));
    return nullptr;
  }
  
  // nextメソッドを取得
  Value nextMethod = iterator->get(ctx, "next");
  if (!nextMethod.isCallable()) {
    ctx->throwError(Error::createTypeError(ctx, "AsyncIterator.next is not callable"));
    return nullptr;
  }
  
  // nextメソッドを呼び出す
  std::vector<Value> args;
  if (!value.isUndefined()) {
    args.push_back(value);
  }
  
  Value result = nextMethod.asFunction()->call(ctx, Value(iterator), args);
  
  // 結果がPromiseでない場合はPromiseでラップ
  if (!result.isObject() || !result.asObject()->isPromise(ctx)) {
    // Promiseでラップして返す
    return builtins::promise::createResolved(ctx, result);
  }
  
  return result.asObject();
}

Object* AsyncIterator::returnIterator(ExecutionContext* ctx, Object* iterator, Value value) {
  if (!iterator) {
    return nullptr;
  }
  
  // returnメソッドがあるかどうかを確認
  Value returnMethod = iterator->get(ctx, "return");
  if (!returnMethod.isCallable()) {
    // returnメソッドがない場合は完了したPromiseを返す
    Object* iterResult = IteratorResult::create(ctx, value, true);
    return builtins::promise::createResolved(ctx, Value(iterResult));
  }
  
  // returnメソッドを呼び出す
  std::vector<Value> args;
  if (!value.isUndefined()) {
    args.push_back(value);
  }
  
  Value result = returnMethod.asFunction()->call(ctx, Value(iterator), args);
  
  // 結果がPromiseでない場合はPromiseでラップ
  if (!result.isObject() || !result.asObject()->isPromise(ctx)) {
    // Promiseでラップして返す
    return builtins::promise::createResolved(ctx, result);
  }
  
  return result.asObject();
}

Object* AsyncIterator::throwIterator(ExecutionContext* ctx, Object* iterator, Value value) {
  if (!iterator) {
    return nullptr;
  }
  
  // throwメソッドがあるかどうかを確認
  Value throwMethod = iterator->get(ctx, "throw");
  if (!throwMethod.isCallable()) {
    // throwメソッドがない場合は拒否されたPromiseを返す
    return builtins::promise::createRejected(ctx, value);
  }
  
  // throwメソッドを呼び出す
  std::vector<Value> args;
  if (!value.isUndefined()) {
    args.push_back(value);
  }
  
  Value result = throwMethod.asFunction()->call(ctx, Value(iterator), args);
  
  // 結果がPromiseでない場合はPromiseでラップ
  if (!result.isObject() || !result.asObject()->isPromise(ctx)) {
    // Promiseでラップして返す
    return builtins::promise::createResolved(ctx, result);
  }
  
  return result.asObject();
}

//-----------------------------------------------------------------------------
// GeneratorObject クラスの実装
//-----------------------------------------------------------------------------

// GeneratorオブジェクトのためのInternal slot名
static const char* kGeneratorStateSlot = "__generator_state__";
static const char* kGeneratorContextSlot = "__generator_context__";
static const char* kGeneratorFunctionSlot = "__generator_function__";
static const char* kGeneratorReceiverSlot = "__generator_receiver__";

Object* GeneratorObject::create(ExecutionContext* ctx, Function* generatorFunction) {
  Object* generator = Object::create(ctx);
  
  // 内部スロットを設定
  generator->setInternalSlot(kGeneratorStateSlot, static_cast<int>(State::Suspended));
  generator->setInternalSlot(kGeneratorFunctionSlot, generatorFunction);
  generator->setInternalSlot(kGeneratorContextSlot, ctx);
  
  // nextメソッドの定義
  generator->defineProperty(ctx, "next", Value::createFunction(ctx, next, 1, "next"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  // returnメソッドの定義
  generator->defineProperty(ctx, "return", Value::createFunction(ctx, returnGenerator, 1, "return"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  // throwメソッドの定義
  generator->defineProperty(ctx, "throw", Value::createFunction(ctx, throwGenerator, 1, "throw"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  // Symbol.iteratorメソッドの定義（自分自身を返す関数）
  auto selfIterator = [](ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) -> Value {
    return thisValue;
  };
  
  generator->defineProperty(ctx, Symbol::iterator, Value::createFunction(ctx, selfIterator, 0, "[Symbol.iterator]"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  return generator;
}

bool GeneratorObject::isGenerator(ExecutionContext* ctx, Object* obj) {
  if (!obj) {
    return false;
  }
  
  // 内部スロットでジェネレーターかどうかを判定
  return obj->hasInternalSlot(kGeneratorStateSlot) && 
         obj->hasInternalSlot(kGeneratorFunctionSlot) &&
         obj->hasInternalSlot(kGeneratorContextSlot);
}

Value GeneratorObject::next(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisValueがジェネレーターオブジェクトでなければエラー
  if (!thisValue.isObject() || !isGenerator(ctx, thisValue.asObject())) {
    ctx->throwError(Error::createTypeError(ctx, "Generator.prototype.next called on non-generator"));
    return Value::createUndefined();
  }
  
  Object* generator = thisValue.asObject();
  
  // ジェネレーターの状態を取得
  int stateValue = 0;
  generator->getInternalSlot(kGeneratorStateSlot, stateValue);
  State state = static_cast<State>(stateValue);
  
  // 完了している場合は{value: undefined, done: true}を返す
  if (state == State::Completed) {
    return Value(IteratorResult::create(ctx, Value::createUndefined(), true));
  }
  
  // 引数を取得
  Value input = args.empty() ? Value::createUndefined() : args[0];
  
  // ジェネレーター関数と実行コンテキストを取得
  Function* generatorFunction = nullptr;
  ExecutionContext* generatorCtx = nullptr;
  
  generator->getInternalSlot(kGeneratorFunctionSlot, generatorFunction);
  generator->getInternalSlot(kGeneratorContextSlot, generatorCtx);
  
  if (!generatorFunction || !generatorCtx) {
    ctx->throwError(Error::createTypeError(ctx, "Invalid generator state"));
    return Value::createUndefined();
  }
  
  // 状態を実行中に変更
  generator->setInternalSlot(kGeneratorStateSlot, static_cast<int>(State::Executing));
  
  try {
    // ジェネレーター関数を再開
    // 注意: 実際の実装ではジェネレーターの再開には特別な処理が必要です
    std::vector<Value> callArgs;
    callArgs.push_back(input);
    
    // thisValueを取得
    Value receiver = Value::createUndefined();
    generator->getInternalSlot(kGeneratorReceiverSlot, receiver);
    
    Value result = generatorFunction->call(generatorCtx, receiver, callArgs);
    
    // 状態を完了に変更
    generator->setInternalSlot(kGeneratorStateSlot, static_cast<int>(State::Completed));
    
    // 結果がイテレーター結果オブジェクトでない場合は変換
    if (!result.isObject() || !IteratorResult::isIteratorResult(ctx, result.asObject())) {
      return Value(IteratorResult::create(ctx, result, true));
    }
    
    return result;
  } catch (Error& e) {
    // 状態を完了に変更
    generator->setInternalSlot(kGeneratorStateSlot, static_cast<int>(State::Completed));
    throw;
  }
}

Value GeneratorObject::returnGenerator(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisValueがジェネレーターオブジェクトでなければエラー
  if (!thisValue.isObject() || !isGenerator(ctx, thisValue.asObject())) {
    ctx->throwError(Error::createTypeError(ctx, "Generator.prototype.return called on non-generator"));
    return Value::createUndefined();
  }
  
  Object* generator = thisValue.asObject();
  
  // ジェネレーターの状態を取得
  int stateValue = 0;
  generator->getInternalSlot(kGeneratorStateSlot, stateValue);
  State state = static_cast<State>(stateValue);
  
  // 完了している場合は{value: undefined, done: true}を返す
  if (state == State::Completed) {
    Value returnValue = args.empty() ? Value::createUndefined() : args[0];
    return Value(IteratorResult::create(ctx, returnValue, true));
  }
  
  // 状態をClosingに変更
  generator->setInternalSlot(kGeneratorStateSlot, static_cast<int>(State::Closing));
  
  try {
    // returnメソッドの実装
    // 注意: 実際の実装ではジェネレーターの早期終了処理が必要です
    Value returnValue = args.empty() ? Value::createUndefined() : args[0];
    
    // 状態を完了に変更
    generator->setInternalSlot(kGeneratorStateSlot, static_cast<int>(State::Completed));
    
    return Value(IteratorResult::create(ctx, returnValue, true));
  } catch (Error& e) {
    // 状態を完了に変更
    generator->setInternalSlot(kGeneratorStateSlot, static_cast<int>(State::Completed));
    throw;
  }
}

Value GeneratorObject::throwGenerator(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisValueがジェネレーターオブジェクトでなければエラー
  if (!thisValue.isObject() || !isGenerator(ctx, thisValue.asObject())) {
    ctx->throwError(Error::createTypeError(ctx, "Generator.prototype.throw called on non-generator"));
    return Value::createUndefined();
  }
  
  Object* generator = thisValue.asObject();
  
  // ジェネレーターの状態を取得
  int stateValue = 0;
  generator->getInternalSlot(kGeneratorStateSlot, stateValue);
  State state = static_cast<State>(stateValue);
  
  // 完了している場合は例外をスローして終了
  if (state == State::Completed) {
    Value throwValue = args.empty() ? Value::createUndefined() : args[0];
    ctx->throwError(Error::create(ctx, throwValue));
    return Value::createUndefined();
  }
  
  // 引数を取得
  Value throwValue = args.empty() ? Value::createUndefined() : args[0];
  
  // ジェネレーター関数と実行コンテキストを取得
  Function* generatorFunction = nullptr;
  ExecutionContext* generatorCtx = nullptr;
  
  generator->getInternalSlot(kGeneratorFunctionSlot, generatorFunction);
  generator->getInternalSlot(kGeneratorContextSlot, generatorCtx);
  
  if (!generatorFunction || !generatorCtx) {
    ctx->throwError(Error::createTypeError(ctx, "Invalid generator state"));
    return Value::createUndefined();
  }
  
  // 状態を実行中に変更
  generator->setInternalSlot(kGeneratorStateSlot, static_cast<int>(State::Executing));
  
  try {
    // ジェネレーター関数に例外をスロー
    // 注意: 実際の実装ではジェネレーターへの例外注入に特別な処理が必要です
    ctx->throwError(Error::create(ctx, throwValue));
    return Value::createUndefined();
  } catch (Error& e) {
    // 状態を完了に変更
    generator->setInternalSlot(kGeneratorStateSlot, static_cast<int>(State::Completed));
    throw;
  }
}

//-----------------------------------------------------------------------------
// グローバル関数の実装
//-----------------------------------------------------------------------------

void initializeIterationProtocol(ExecutionContext* ctx, Object* globalObj) {
  // Symbol.iteratorとSymbol.asyncIteratorの初期化はすでに
  // Symbolクラスの初期化時に行われているはずです
  
  // 必要に応じてイテレーターのプロトタイプの初期化を行います
  Object* objectPrototype = globalObj->get(ctx, "Object").asObject()->get(ctx, "prototype").asObject();
  Object* promisePrototype = globalObj->get(ctx, "Promise").asObject()->get(ctx, "prototype").asObject();
  
  // イテレータープロトタイプを作成
  auto [arrayIteratorProto, stringIteratorProto] = createIteratorPrototypes(ctx, objectPrototype);
  
  // ジェネレータープロトタイプを作成
  auto [generatorProto, generatorObjectProto] = createGeneratorPrototypes(ctx, objectPrototype);
  
  // 非同期イテレータープロトタイプを作成
  auto [asyncIteratorProto, asyncGeneratorProto] = createAsyncIteratorPrototypes(ctx, objectPrototype, promisePrototype);
  
  // プロトタイプをグローバルオブジェクトに保存しておくとデバッグしやすくなります
  // （実際のJavaScriptエンジンではこれらは通常アクセスできません）
  globalObj->setInternalSlot("ArrayIteratorPrototype", arrayIteratorProto);
  globalObj->setInternalSlot("StringIteratorPrototype", stringIteratorProto);
  globalObj->setInternalSlot("GeneratorPrototype", generatorProto);
  globalObj->setInternalSlot("GeneratorObjectPrototype", generatorObjectProto);
  globalObj->setInternalSlot("AsyncIteratorPrototype", asyncIteratorProto);
  globalObj->setInternalSlot("AsyncGeneratorPrototype", asyncGeneratorProto);
}

std::pair<Object*, Object*> createIteratorPrototypes(ExecutionContext* ctx, Object* objectPrototype) {
  // イテレータープロトタイプ（ベース）の作成
  Object* iteratorPrototype = Object::create(ctx, objectPrototype);
  
  // Symbol.iteratorメソッドの定義（自分自身を返す関数）
  auto selfIterator = [](ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) -> Value {
    return thisValue;
  };
  
  iteratorPrototype->defineProperty(ctx, Symbol::iterator, Value::createFunction(ctx, selfIterator, 0, "[Symbol.iterator]"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  // ArrayイテレータープロトタイプとStringイテレータープロトタイプの作成
  Object* arrayIteratorPrototype = Object::create(ctx, iteratorPrototype);
  Object* stringIteratorPrototype = Object::create(ctx, iteratorPrototype);
  
  // toStringTagの定義
  arrayIteratorPrototype->defineProperty(ctx, Symbol::toStringTag, Value::createString(ctx, "Array Iterator"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Configurable
      )
  );
  
  stringIteratorPrototype->defineProperty(ctx, Symbol::toStringTag, Value::createString(ctx, "String Iterator"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Configurable
      )
  );
  
  return {arrayIteratorPrototype, stringIteratorPrototype};
}

std::pair<Object*, Object*> createGeneratorPrototypes(ExecutionContext* ctx, Object* objectPrototype) {
  // イテレータープロトタイプとGeneratorプロトタイプの作成
  auto [arrayIteratorProto, _] = createIteratorPrototypes(ctx, objectPrototype);
  Object* generatorPrototype = Object::create(ctx, arrayIteratorProto);
  
  // ジェネレーターオブジェクトプロトタイプの作成
  Object* generatorObjectPrototype = Object::create(ctx, generatorPrototype);
  
  // toStringTagの定義
  generatorPrototype->defineProperty(ctx, Symbol::toStringTag, Value::createString(ctx, "Generator"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Configurable
      )
  );
  
  generatorObjectPrototype->defineProperty(ctx, Symbol::toStringTag, Value::createString(ctx, "Generator"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Configurable
      )
  );
  
  // nextメソッドの定義
  generatorPrototype->defineProperty(ctx, "next", Value::createFunction(ctx, GeneratorObject::next, 1, "next"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  // returnメソッドの定義
  generatorPrototype->defineProperty(ctx, "return", Value::createFunction(ctx, GeneratorObject::returnGenerator, 1, "return"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  // throwメソッドの定義
  generatorPrototype->defineProperty(ctx, "throw", Value::createFunction(ctx, GeneratorObject::throwGenerator, 1, "throw"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  return {generatorPrototype, generatorObjectPrototype};
}

std::pair<Object*, Object*> createAsyncIteratorPrototypes(ExecutionContext* ctx, Object* objectPrototype, Object* promisePrototype) {
  // 非同期イテレータープロトタイプの作成
  Object* asyncIteratorPrototype = Object::create(ctx, objectPrototype);
  
  // Symbol.asyncIteratorメソッドの定義（自分自身を返す関数）
  auto selfAsyncIterator = [](ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) -> Value {
    return thisValue;
  };
  
  asyncIteratorPrototype->defineProperty(ctx, Symbol::asyncIterator, Value::createFunction(ctx, selfAsyncIterator, 0, "[Symbol.asyncIterator]"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  // 非同期ジェネレータープロトタイプの作成
  Object* asyncGeneratorPrototype = Object::create(ctx, asyncIteratorPrototype);
  
  // toStringTagの定義
  asyncIteratorPrototype->defineProperty(ctx, Symbol::toStringTag, Value::createString(ctx, "Async Iterator"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Configurable
      )
  );
  
  asyncGeneratorPrototype->defineProperty(ctx, Symbol::toStringTag, Value::createString(ctx, "Async Generator"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Configurable
      )
  );
  
  return {asyncIteratorPrototype, asyncGeneratorPrototype};
}

Value completeIterator(ExecutionContext* ctx, Object* iterator, Value value) {
  // イテレーターのreturnメソッドがあれば呼び出す
  Object* result = Iterator::returnIterator(ctx, iterator, value);
  
  // returnメソッドがない場合や、エラーが発生した場合はundefinedを返す
  if (!result) {
    return Value::createUndefined();
  }
  
  return Value(result);
}

} // namespace aero 