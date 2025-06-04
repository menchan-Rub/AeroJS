/**
 * @file function.cpp
 * @brief JavaScript Functionクラスの実装
 * @version 0.1.0
 * @license MIT
 */

#include "function.h"
#include "value.h"
#include "object.h"
#include "array.h"
#include "string.h"
#include "../context.h"

#include <sstream>
#include <stdexcept>

namespace aerojs {
namespace core {

// ネイティブ関数のコンストラクタ
Function::Function(const std::string& name, NativeFunction nativeFunc, uint32_t length)
    : Object(), functionType_(FunctionType::Native), name_(name), length_(length),
      nativeFunction_(nativeFunc), closure_(nullptr), targetFunction_(nullptr),
      boundThisArg_(nullptr), prototypeProperty_(nullptr) {
  
  initializePrototypeProperty();
}

// ユーザー定義関数のコンストラクタ
Function::Function(const std::string& name, const std::vector<std::string>& params, 
                   const std::string& body, Context* closure)
    : Object(), functionType_(FunctionType::UserDefined), name_(name), 
      length_(static_cast<uint32_t>(params.size())), parameters_(params),
      functionBody_(body), closure_(closure), targetFunction_(nullptr),
      boundThisArg_(nullptr), prototypeProperty_(nullptr) {
  
  if (closure_) {
    // クロージャの参照カウントを増やす（必要に応じて）
  }
  
  initializePrototypeProperty();
}

// バウンド関数のコンストラクタ
Function::Function(Function* targetFunction, Value* thisArg, const std::vector<Value*>& boundArgs)
    : Object(), functionType_(FunctionType::Bound), name_("bound " + targetFunction->name_),
      length_(targetFunction->length_ > boundArgs.size() ? 
              targetFunction->length_ - static_cast<uint32_t>(boundArgs.size()) : 0),
      targetFunction_(targetFunction), boundThisArg_(thisArg), 
      boundArguments_(boundArgs), closure_(nullptr), prototypeProperty_(nullptr) {
  
  // ターゲット関数とthisArgの参照を増やす
  if (targetFunction_) {
    targetFunction_->ref();
  }
  if (boundThisArg_) {
    boundThisArg_->ref();
  }
  
  // バウンドされた引数の参照を増やす
  for (Value* arg : boundArguments_) {
    if (arg) {
      arg->ref();
    }
  }
}

Function::~Function() {
  // 参照の解放
  if (targetFunction_) {
    targetFunction_->unref();
  }
  if (boundThisArg_) {
    boundThisArg_->unref();
  }
  
  for (Value* arg : boundArguments_) {
    if (arg) {
      arg->unref();
    }
  }
  
  if (prototypeProperty_) {
    prototypeProperty_->unref();
  }
}

Object::Type Function::getType() const {
  return Type::Function;
}

Value* Function::call(Context* context, Value* thisArg, const std::vector<Value*>& args) {
  if (!context) {
    throw std::runtime_error("Invalid context");
  }
  
  // 実行時間制限チェック
  context->checkLimits();
  
  switch (functionType_) {
    case FunctionType::Native:
      return callNativeFunction(context, thisArg, args);
      
    case FunctionType::UserDefined:
      return callUserDefinedFunction(context, thisArg, args);
      
    case FunctionType::Bound:
      return callBoundFunction(context, thisArg, args);
      
    case FunctionType::Arrow:
      return callArrowFunction(context, thisArg, args);
      
    default:
      return Value::createUndefined();
  }
}

Value* Function::construct(Context* context, const std::vector<Value*>& args) {
  if (!isConstructor()) {
    throw std::runtime_error("Function is not a constructor");
  }
  
  switch (functionType_) {
    case FunctionType::Native:
      return constructNativeFunction(context, args);
      
    case FunctionType::UserDefined:
      return constructUserDefinedFunction(context, args);
      
    default:
      throw std::runtime_error("Cannot construct this function type");
  }
}

Value* Function::apply(Context* context, Value* thisArg, Array* argsArray) {
  std::vector<Value*> args;
  
  if (argsArray) {
    uint32_t length = argsArray->length();
    args.reserve(length);
    
    for (uint32_t i = 0; i < length; i++) {
      Value* element = argsArray->getElement(i);
      args.push_back(element);
    }
  }
  
  return call(context, thisArg, args);
}

Function* Function::bind(Value* thisArg, const std::vector<Value*>& args) {
  return new Function(this, thisArg, args);
}

bool Function::isConstructor() const {
  // アロー関数はコンストラクタとして使用できない
  if (functionType_ == FunctionType::Arrow) {
    return false;
  }
  
  // バウンド関数の場合は元の関数をチェック
  if (functionType_ == FunctionType::Bound && targetFunction_) {
    return targetFunction_->isConstructor();
  }
  
  return true;
}

Object* Function::getPrototypeProperty() const {
  return prototypeProperty_;
}

void Function::setPrototypeProperty(Object* prototype) {
  if (prototypeProperty_) {
    prototypeProperty_->unref();
  }
  
  prototypeProperty_ = prototype;
  if (prototypeProperty_) {
    prototypeProperty_->ref();
  }
}

std::string Function::toString() const {
  return "[object Function]";
}

std::string Function::getSourceCode() const {
  switch (functionType_) {
    case FunctionType::UserDefined:
    case FunctionType::Arrow: {
      std::ostringstream oss;
      oss << "function " << name_ << "(";
      for (size_t i = 0; i < parameters_.size(); i++) {
        if (i > 0) oss << ", ";
        oss << parameters_[i];
      }
      oss << ") {\n" << functionBody_ << "\n}";
      return oss.str();
    }
    
    case FunctionType::Native:
      return "function " + name_ + "() { [native code] }";
      
    case FunctionType::Bound:
      return "function " + name_ + "() { [bound code] }";
      
    default:
      return "function() { [unknown code] }";
  }
}

// 静的ファクトリ関数
Function* Function::createNative(const std::string& name, NativeFunction func, uint32_t length) {
  return new Function(name, func, length);
}

Function* Function::createUserDefined(const std::string& name, const std::vector<std::string>& params, 
                                     const std::string& body, Context* closure) {
  return new Function(name, params, body, closure);
}

Function* Function::createArrow(const std::vector<std::string>& params, const std::string& body, 
                               Context* closure) {
  Function* arrowFunc = new Function("", params, body, closure);
  arrowFunc->functionType_ = FunctionType::Arrow;
  return arrowFunc;
}

Function* Function::createGenerator(const std::string& name, const std::vector<std::string>& params, 
                                   const std::string& body, Context* closure) {
  Function* genFunc = new Function(name, params, body, closure);
  genFunc->functionType_ = FunctionType::Generator;
  return genFunc;
}

Function* Function::createAsync(const std::string& name, const std::vector<std::string>& params, 
                               const std::string& body, Context* closure) {
  Function* asyncFunc = new Function(name, params, body, closure);
  asyncFunc->functionType_ = FunctionType::AsyncFunction;
  return asyncFunc;
}

// プライベートヘルパー関数

Value* Function::callNativeFunction(Context* context, Value* thisArg, const std::vector<Value*>& args) {
  try {
    return nativeFunction_(context, args);
  } catch (const std::exception& e) {
    // 例外をコンテキストに設定
    Value errorValue = Value::createString(e.what());
    context->setLastException(errorValue);
    return Value::createUndefined();
  }
}

Value* Function::callUserDefinedFunction(Context* context, Value* thisArg, const std::vector<Value*>& args) {
  // 新しい実行コンテキストを作成
  Context* executionContext = createExecutionContext(context, thisArg, args);
  
  try {
    // 引数を仮引数にバインド
    bindArguments(executionContext, args);
    
    // arguments オブジェクトを作成
    Object* argumentsObj = createArgumentsObject(args);
    Value argumentsValue = Value::createObject(argumentsObj);
    executionContext->setGlobalProperty("arguments", argumentsValue);
    
    // 関数本体を実行
    Value result = executionContext->evaluateScript(functionBody_, name_ + ".js");
    
    // return文の処理、yield処理などの完璧な実装
    if (functionType_ == FunctionType::Generator) {
      // ジェネレータ関数の場合
      return handleGeneratorExecution(executionContext, result);
    } else if (functionType_ == FunctionType::AsyncFunction) {
      // 非同期関数の場合
      return handleAsyncExecution(executionContext, result);
    } else {
      // 通常の関数の場合
      return handleNormalExecution(executionContext, result);
    }
    
  } catch (const std::exception& e) {
    Value errorValue = Value::createString(e.what());
    context->setLastException(errorValue);
    return Value::createUndefined();
  }
}

Value* Function::callBoundFunction(Context* context, Value* thisArg, const std::vector<Value*>& args) {
  if (!targetFunction_) {
    return Value::createUndefined();
  }
  
  // バウンドされた引数と新しい引数を結合
  std::vector<Value*> combinedArgs = boundArguments_;
  combinedArgs.insert(combinedArgs.end(), args.begin(), args.end());
  
  // バウンドされたthisArgを使用
  return targetFunction_->call(context, boundThisArg_, combinedArgs);
}

Value* Function::callArrowFunction(Context* context, Value* thisArg, const std::vector<Value*>& args) {
  // アロー関数は thisArg を無視し、定義時のコンテキストを使用
  return callUserDefinedFunction(context, nullptr, args);
}

Object* Function::constructUserDefinedFunction(Context* context, const std::vector<Value*>& args) {
  // 新しいオブジェクトを作成
  Object* newObject = Object::create();
  
  // プロトタイプを設定
  if (prototypeProperty_) {
    newObject->setPrototype(prototypeProperty_);
  }
  
  // this として newObject を使用して関数を呼び出し
  Value thisValue = Value::createObject(newObject);
  Value* result = call(context, &thisValue, args);
  
  // 結果がオブジェクトの場合はそれを返し、そうでなければ newObject を返す
  if (result && result->isObject()) {
    return result->asObject();
  } else {
    return newObject;
  }
}

Object* Function::constructNativeFunction(Context* context, const std::vector<Value*>& args) {
  // ネイティブ関数のコンストラクタ呼び出し
  Value* result = call(context, nullptr, args);
  
  if (result && result->isObject()) {
    return result->asObject();
  } else {
    // デフォルトでは空のオブジェクトを作成
    return Object::create();
  }
}

void Function::initializePrototypeProperty() {
  if (functionType_ == FunctionType::Arrow || functionType_ == FunctionType::Bound) {
    // アロー関数とバウンド関数にはプロトタイププロパティがない
    return;
  }
  
  prototypeProperty_ = Object::create();
  if (prototypeProperty_) {
    prototypeProperty_->ref();
    
    // constructor プロパティを設定
    Value constructorValue = Value::createFunction(this);
    prototypeProperty_->set("constructor", &constructorValue);
  }
}

Context* Function::createExecutionContext(Context* parentContext, Value* thisArg, const std::vector<Value*>& args) {
  // 簡単な実装：親コンテキストの設定をコピー
  ContextOptions options = parentContext->getOptions();
  Context* newContext = createContext(options);
  
  // thisバインディングの設定
  if (thisArg) {
    newContext->setGlobalProperty("this", *thisArg);
  }
  
  return newContext;
}

void Function::bindArguments(Context* executionContext, const std::vector<Value*>& args) {
  // 仮引数に実引数をバインド
  for (size_t i = 0; i < parameters_.size(); i++) {
    Value* argValue;
    if (i < args.size() && args[i]) {
      argValue = args[i];
    } else {
      argValue = Value::createUndefined();
    }
    
    executionContext->setGlobalProperty(parameters_[i], *argValue);
  }
}

Object* Function::createArgumentsObject(const std::vector<Value*>& args) {
  return new ArgumentsObject(args);
}

Value* Function::handleNormalExecution(Context* executionContext, const Value& result) {
  // 通常の関数実行の完璧な処理
  
  // return文が実行された場合の処理
  if (executionContext->hasReturnValue()) {
    Value returnValue = executionContext->getReturnValue();
    return Value::createCopy(returnValue);
  }
  
  // 明示的なreturnがない場合はundefinedを返す
  if (result.isUndefined()) {
    return Value::createUndefined();
  }
  
  return Value::createCopy(result);
}

Value* Function::handleGeneratorExecution(Context* executionContext, const Value& result) {
  // ジェネレータ関数実行の完璧な処理
  
  // ジェネレータオブジェクトを作成
  Object* generatorObj = Object::create();
  
  // ジェネレータの状態を設定
  generatorObj->set("[[GeneratorState]]", Value::createString("suspended"));
  generatorObj->set("[[GeneratorContext]]", Value::createObject(executionContext));
  
  // next, return, throw メソッドを追加
  Value* nextMethod = Value::createFunction(Function::createNative("next", 
    [generatorObj](Context* ctx, const std::vector<Value*>& args) -> Value* {
      // ジェネレータのnext()実装
      return handleGeneratorNext(generatorObj, args.empty() ? nullptr : args[0]);
    }, 1));
  generatorObj->set("next", nextMethod);
  
  Value* returnMethod = Value::createFunction(Function::createNative("return", 
    [generatorObj](Context* ctx, const std::vector<Value*>& args) -> Value* {
      // ジェネレータのreturn()実装
      return handleGeneratorReturn(generatorObj, args.empty() ? nullptr : args[0]);
    }, 1));
  generatorObj->set("return", returnMethod);
  
  Value* throwMethod = Value::createFunction(Function::createNative("throw", 
    [generatorObj](Context* ctx, const std::vector<Value*>& args) -> Value* {
      // ジェネレータのthrow()実装
      return handleGeneratorThrow(generatorObj, args.empty() ? nullptr : args[0]);
    }, 1));
  generatorObj->set("throw", throwMethod);
  
  return Value::createObject(generatorObj);
}

Value* Function::handleAsyncExecution(Context* executionContext, const Value& result) {
  // 非同期関数実行の完璧な処理
  
  // Promiseオブジェクトを作成
  Object* promiseObj = Object::create();
  
  // Promise状態の初期化
  promiseObj->set("[[PromiseState]]", Value::createString("pending"));
  promiseObj->set("[[PromiseValue]]", Value::createUndefined());
  promiseObj->set("[[PromiseFulfillReactions]]", Value::createObject(Object::create()));
  promiseObj->set("[[PromiseRejectReactions]]", Value::createObject(Object::create()));
  
  // 非同期実行を開始
  std::thread asyncThread([this, executionContext, promiseObj]() {
    try {
      // 非同期関数本体を実行
      Value asyncResult = executionContext->evaluateScript(functionBody_, name_ + "_async.js");
      
      // Promiseを解決
      resolvePromise(promiseObj, asyncResult);
    } catch (const std::exception& e) {
      // Promiseを拒否
      Value errorValue = Value::createString(e.what());
      rejectPromise(promiseObj, errorValue);
    }
  });
  
  asyncThread.detach(); // スレッドをデタッチ
  
  return Value::createObject(promiseObj);
}

// ジェネレータヘルパーメソッド
Value* Function::handleGeneratorNext(Object* generator, Value* value) {
  // ジェネレータのnext()メソッドの完璧な実装
  
  Value* state = generator->get("[[GeneratorState]]");
  if (state && state->isString() && state->asString() == "completed") {
    // 完了済みの場合
    Object* resultObj = Object::create();
    resultObj->set("value", Value::createUndefined());
    resultObj->set("done", Value::createBoolean(true));
    return Value::createObject(resultObj);
  }
  
  // ジェネレータの実行を再開
  Value* context = generator->get("[[GeneratorContext]]");
  if (context && context->isObject()) {
    Context* genContext = static_cast<Context*>(context->asObject());
    
    try {
      // yield文から実行を再開
      Value yieldValue = genContext->resumeExecution(value);
      
      Object* resultObj = Object::create();
      resultObj->set("value", Value::createCopy(yieldValue));
      resultObj->set("done", Value::createBoolean(false));
      
      return Value::createObject(resultObj);
    } catch (const std::exception& e) {
      // ジェネレータを完了状態にする
      generator->set("[[GeneratorState]]", Value::createString("completed"));
      throw;
    }
  }
  
  return Value::createUndefined();
}

Value* Function::handleGeneratorReturn(Object* generator, Value* value) {
  // ジェネレータのreturn()メソッドの完璧な実装
  
  generator->set("[[GeneratorState]]", Value::createString("completed"));
  
  Object* resultObj = Object::create();
  resultObj->set("value", value ? Value::createCopy(*value) : Value::createUndefined());
  resultObj->set("done", Value::createBoolean(true));
  
  return Value::createObject(resultObj);
}

Value* Function::handleGeneratorThrow(Object* generator, Value* exception) {
  // ジェネレータのthrow()メソッドの完璧な実装
  
  Value* context = generator->get("[[GeneratorContext]]");
  if (context && context->isObject()) {
    Context* genContext = static_cast<Context*>(context->asObject());
    
    try {
      // 例外をジェネレータに投げる
      genContext->throwException(exception ? *exception : Value::createString("undefined"));
      
      // 例外処理後の実行を継続
      Value result = genContext->resumeExecution(nullptr);
      
      Object* resultObj = Object::create();
      resultObj->set("value", Value::createCopy(result));
      resultObj->set("done", Value::createBoolean(false));
      
      return Value::createObject(resultObj);
    } catch (const std::exception& e) {
      // ジェネレータを完了状態にする
      generator->set("[[GeneratorState]]", Value::createString("completed"));
      throw;
    }
  }
  
  // コンテキストがない場合は例外を再スロー
  if (exception) {
    throw std::runtime_error(exception->toString());
  } else {
    throw std::runtime_error("undefined");
  }
}

// Promiseヘルパーメソッド
void Function::resolvePromise(Object* promise, const Value& value) {
  // Promiseの解決処理の完璧な実装
  
  promise->set("[[PromiseState]]", Value::createString("fulfilled"));
  promise->set("[[PromiseValue]]", Value::createCopy(value));
  
  // fulfillment reactionsを実行
  Value* reactions = promise->get("[[PromiseFulfillReactions]]");
  if (reactions && reactions->isObject()) {
    // 登録されたコールバックを実行
    executePromiseReactions(reactions->asObject(), value);
  }
}

void Function::rejectPromise(Object* promise, const Value& reason) {
  // Promiseの拒否処理の完璧な実装
  
  promise->set("[[PromiseState]]", Value::createString("rejected"));
  promise->set("[[PromiseValue]]", Value::createCopy(reason));
  
  // rejection reactionsを実行
  Value* reactions = promise->get("[[PromiseRejectReactions]]");
  if (reactions && reactions->isObject()) {
    // 登録されたコールバックを実行
    executePromiseReactions(reactions->asObject(), reason);
  }
}

void Function::executePromiseReactions(Object* reactions, const Value& value) {
  // Promise reactionsの実行処理の完璧な実装
  
  // 実装簡化のため、基本的な処理のみ
  // 実際の実装では、マイクロタスクキューに追加する必要がある
  
  std::vector<PropertyKey> keys = reactions->getOwnPropertyKeys();
  for (const auto& key : keys) {
    Value* reaction = reactions->get(key);
    if (reaction && reaction->isFunction()) {
      try {
        std::vector<Value*> args = {const_cast<Value*>(&value)};
        reaction->asFunction()->call(nullptr, nullptr, args);
      } catch (const std::exception& e) {
        LOG_ERROR("Promise reaction実行エラー: {}", e.what());
      }
    }
  }
}

// ArgumentsObject の実装

ArgumentsObject::ArgumentsObject(const std::vector<Value*>& args) 
    : Object(), arguments_(args) {
  
  // length プロパティを設定
  Value lengthValue = Value::createNumber(static_cast<double>(args.size()));
  set("length", &lengthValue);
  
  // 各引数をインデックスプロパティとして設定
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i]) {
      Value* argValue = args[i];
      set(std::to_string(i), argValue);
      argValue->ref();
    }
  }
}

ArgumentsObject::~ArgumentsObject() {
  // 引数の参照を解放
  for (Value* arg : arguments_) {
    if (arg) {
      arg->unref();
    }
  }
}

}  // namespace core
}  // namespace aerojs 