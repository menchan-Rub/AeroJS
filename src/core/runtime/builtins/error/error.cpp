/**
 * @file error.cpp
 * @brief JavaScript のエラーオブジェクトの実装
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#include "error.h"

#include <sstream>
#include <unordered_map>

#include "core/runtime/array_object.h"
#include "core/runtime/execution_context.h"
#include "core/runtime/property_descriptor.h"
#include "core/runtime/value.h"

namespace aero {

namespace {
// エラータイプ名のマッピング
const std::unordered_map<ErrorType, std::string> s_errorTypeNames = {
    {ErrorType::Error, "Error"},
    {ErrorType::EvalError, "EvalError"},
    {ErrorType::RangeError, "RangeError"},
    {ErrorType::ReferenceError, "ReferenceError"},
    {ErrorType::SyntaxError, "SyntaxError"},
    {ErrorType::TypeError, "TypeError"},
    {ErrorType::URIError, "URIError"},
    {ErrorType::AggregateError, "AggregateError"}};
}  // namespace

// ErrorObject の実装

ErrorObject::ErrorObject(ErrorType type, const std::string& message, Object* cause)
    : Object(nullptr)  // プロトタイプは後で設定される
      ,
      m_type(type),
      m_message(message),
      m_cause(cause) {
  // スタックトレースを生成
  generateStackTrace();

  // プロパティの初期化
  defineProperty("message", PropertyDescriptor(Value(m_message), true, true, true));
  defineProperty("name", PropertyDescriptor(Value(getTypeName()), true, true, true));

  // ES2022のエラー原因（cause）プロパティ
  if (m_cause) {
    defineProperty("cause", PropertyDescriptor(Value(m_cause), true, true, true));
  }

  // スタックプロパティ（非標準だがほとんどの実装で提供）
  defineProperty("stack", PropertyDescriptor(Value(m_stack), true, true, true));
}

ErrorObject::~ErrorObject() {
  // リソースのクリーンアップ（必要に応じて）
}

const std::string& ErrorObject::getMessage() const {
  return m_message;
}

ErrorType ErrorObject::getType() const {
  return m_type;
}

std::string ErrorObject::getTypeName() const {
  auto it = s_errorTypeNames.find(m_type);
  if (it != s_errorTypeNames.end()) {
    return it->second;
  }
  return "Error";  // デフォルトは一般的なエラー
}

const std::string& ErrorObject::getStack() const {
  return m_stack;
}

Object* ErrorObject::getCause() const {
  return m_cause;
}

std::string ErrorObject::toString() const {
  std::ostringstream oss;
  if (m_message.empty()) {
    oss << getTypeName();
  } else {
    oss << getTypeName() << ": " << m_message;
  }
  return oss.str();
}

bool ErrorObject::isType(ErrorType type) const {
  return m_type == type;
}

void ErrorObject::generateStackTrace() {
  auto* ctx = ExecutionContext::current();
  std::ostringstream oss;

  // エラーメッセージをスタックトレースの先頭に追加
  oss << toString() << "\n";

  // 現在の実行コンテキストからコールスタック情報を取得
  const auto& callStack = ctx->getCallStack();

  // コールスタックが空の場合は最小限の情報を表示
  if (callStack.empty()) {
    oss << "    at <anonymous>:1:1\n";
  } else {
    // 最大表示するスタックフレーム数（設定可能）
    const size_t maxFrames = ctx->getConfig().maxStackTraceFrames;

    // 各コールフレームの情報を追加
    for (size_t i = 0; i < std::min(callStack.size(), maxFrames); ++i) {
      const auto& frame = callStack[i];

      // ファイル情報がある場合はそれを使用
      if (frame.sourceInfo) {
        oss << "    at " << (frame.functionName.empty() ? "<anonymous>" : frame.functionName);
        oss << " (" << frame.sourceInfo->fileName << ":"
            << frame.sourceInfo->line << ":" << frame.sourceInfo->column << ")\n";
      } else {
        // ソース情報がない場合は関数名のみ
        oss << "    at " << (frame.functionName.empty() ? "<anonymous>" : frame.functionName) << "\n";
      }

      // 非同期境界を示す（AsyncFunction、Promise等の場合）
      if (frame.isAsyncBoundary && i < callStack.size() - 1) {
        oss << "    --- async boundary ---\n";
      }
    }

    // スタックが切り詰められた場合はその旨を表示
    if (callStack.size() > maxFrames) {
      oss << "    ... " << (callStack.size() - maxFrames) << " more frames\n";
    }
  }

  // 最適化されたコードの場合は警告を表示
  if (ctx->isOptimizedCode()) {
    oss << "    (Note: Some frames may be omitted due to optimization)\n";
  }

  m_stack = oss.str();
}

// エラーコンストラクタ関数

ErrorConstructorFunction getErrorConstructor(ErrorType type) {
  switch (type) {
    case ErrorType::Error:
      return errorConstructor;
    case ErrorType::EvalError:
      return evalErrorConstructor;
    case ErrorType::RangeError:
      return rangeErrorConstructor;
    case ErrorType::ReferenceError:
      return referenceErrorConstructor;
    case ErrorType::SyntaxError:
      return syntaxErrorConstructor;
    case ErrorType::TypeError:
      return typeErrorConstructor;
    case ErrorType::URIError:
      return uriErrorConstructor;
    case ErrorType::AggregateError:
      return aggregateErrorConstructor;
    default:
      return errorConstructor;
  }
}

Value errorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisValueがErrorオブジェクトでない場合、新しいErrorオブジェクトを作成
  if (!thisValue.isObject() || !thisValue.asObject()->isError()) {
    auto* proto = ctx->errorPrototype();
    std::string message = args.empty() || args[0].isUndefined() ? "" : args[0].toString()->value();
    Object* cause = nullptr;

    // ES2022: エラー原因（cause）オプション
    if (args.size() > 1 && args[1].isObject() &&
        args[1].asObject()->has("cause")) {
      cause = args[1].asObject()->get("cause").asObject();
    }

    auto* error = new ErrorObject(ErrorType::Error, message, cause);
    error->setPrototype(proto);
    return Value(error);
  }

  // thisValueがErrorオブジェクトの場合、プロパティを設定
  auto* error = static_cast<ErrorObject*>(thisValue.asObject());

  // メッセージを設定
  if (!args.empty() && !args[0].isUndefined()) {
    error->defineProperty("message", PropertyDescriptor(args[0], true, true, true));
  }

  // ES2022: エラー原因（cause）オプション
  if (args.size() > 1 && args[1].isObject() &&
      args[1].asObject()->has("cause")) {
    error->defineProperty("cause", PropertyDescriptor(args[1].asObject()->get("cause"), true, true, true));
  }

  return thisValue;
}

Value evalErrorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisValueがEvalErrorオブジェクトでない場合、新しいEvalErrorオブジェクトを作成
  if (!thisValue.isObject() || !thisValue.asObject()->isError() ||
      !static_cast<ErrorObject*>(thisValue.asObject())->isType(ErrorType::EvalError)) {
    auto* proto = ctx->evalErrorPrototype();
    std::string message = args.empty() || args[0].isUndefined() ? "" : args[0].toString()->value();
    Object* cause = nullptr;

    // ES2022: エラー原因（cause）オプション
    if (args.size() > 1 && args[1].isObject() &&
        args[1].asObject()->has("cause")) {
      cause = args[1].asObject()->get("cause").asObject();
    }

    auto* error = new ErrorObject(ErrorType::EvalError, message, cause);
    error->setPrototype(proto);
    return Value(error);
  }

  // thisValueがEvalErrorオブジェクトの場合、プロパティを設定
  return errorConstructor(ctx, thisValue, args);
}

Value rangeErrorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisValueがRangeErrorオブジェクトでない場合、新しいRangeErrorオブジェクトを作成
  if (!thisValue.isObject() || !thisValue.asObject()->isError() ||
      !static_cast<ErrorObject*>(thisValue.asObject())->isType(ErrorType::RangeError)) {
    auto* proto = ctx->rangeErrorPrototype();
    std::string message = args.empty() || args[0].isUndefined() ? "" : args[0].toString()->value();
    Object* cause = nullptr;

    // ES2022: エラー原因（cause）オプション
    if (args.size() > 1 && args[1].isObject() &&
        args[1].asObject()->has("cause")) {
      cause = args[1].asObject()->get("cause").asObject();
    }

    auto* error = new ErrorObject(ErrorType::RangeError, message, cause);
    error->setPrototype(proto);
    return Value(error);
  }

  // thisValueがRangeErrorオブジェクトの場合、プロパティを設定
  return errorConstructor(ctx, thisValue, args);
}

Value referenceErrorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisValueがReferenceErrorオブジェクトでない場合、新しいReferenceErrorオブジェクトを作成
  if (!thisValue.isObject() || !thisValue.asObject()->isError() ||
      !static_cast<ErrorObject*>(thisValue.asObject())->isType(ErrorType::ReferenceError)) {
    auto* proto = ctx->referenceErrorPrototype();
    std::string message = args.empty() || args[0].isUndefined() ? "" : args[0].toString()->value();
    Object* cause = nullptr;

    // ES2022: エラー原因（cause）オプション
    if (args.size() > 1 && args[1].isObject() &&
        args[1].asObject()->has("cause")) {
      cause = args[1].asObject()->get("cause").asObject();
    }

    auto* error = new ErrorObject(ErrorType::ReferenceError, message, cause);
    error->setPrototype(proto);
    return Value(error);
  }

  // thisValueがReferenceErrorオブジェクトの場合、プロパティを設定
  return errorConstructor(ctx, thisValue, args);
}

Value syntaxErrorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisValueがSyntaxErrorオブジェクトでない場合、新しいSyntaxErrorオブジェクトを作成
  if (!thisValue.isObject() || !thisValue.asObject()->isError() ||
      !static_cast<ErrorObject*>(thisValue.asObject())->isType(ErrorType::SyntaxError)) {
    auto* proto = ctx->syntaxErrorPrototype();
    std::string message = args.empty() || args[0].isUndefined() ? "" : args[0].toString()->value();
    Object* cause = nullptr;

    // ES2022: エラー原因（cause）オプション
    if (args.size() > 1 && args[1].isObject() &&
        args[1].asObject()->has("cause")) {
      cause = args[1].asObject()->get("cause").asObject();
    }

    auto* error = new ErrorObject(ErrorType::SyntaxError, message, cause);
    error->setPrototype(proto);
    return Value(error);
  }

  // thisValueがSyntaxErrorオブジェクトの場合、プロパティを設定
  return errorConstructor(ctx, thisValue, args);
}

Value typeErrorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisValueがTypeErrorオブジェクトでない場合、新しいTypeErrorオブジェクトを作成
  if (!thisValue.isObject() || !thisValue.asObject()->isError() ||
      !static_cast<ErrorObject*>(thisValue.asObject())->isType(ErrorType::TypeError)) {
    auto* proto = ctx->typeErrorPrototype();
    std::string message = args.empty() || args[0].isUndefined() ? "" : args[0].toString()->value();
    Object* cause = nullptr;

    // ES2022: エラー原因（cause）オプション
    if (args.size() > 1 && args[1].isObject() &&
        args[1].asObject()->has("cause")) {
      cause = args[1].asObject()->get("cause").asObject();
    }

    auto* error = new ErrorObject(ErrorType::TypeError, message, cause);
    error->setPrototype(proto);
    return Value(error);
  }

  // thisValueがTypeErrorオブジェクトの場合、プロパティを設定
  return errorConstructor(ctx, thisValue, args);
}

Value uriErrorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisValueがURIErrorオブジェクトでない場合、新しいURIErrorオブジェクトを作成
  if (!thisValue.isObject() || !thisValue.asObject()->isError() ||
      !static_cast<ErrorObject*>(thisValue.asObject())->isType(ErrorType::URIError)) {
    auto* proto = ctx->uriErrorPrototype();
    std::string message = args.empty() || args[0].isUndefined() ? "" : args[0].toString()->value();
    Object* cause = nullptr;

    // ES2022: エラー原因（cause）オプション
    if (args.size() > 1 && args[1].isObject() &&
        args[1].asObject()->has("cause")) {
      cause = args[1].asObject()->get("cause").asObject();
    }

    auto* error = new ErrorObject(ErrorType::URIError, message, cause);
    error->setPrototype(proto);
    return Value(error);
  }

  // thisValueがURIErrorオブジェクトの場合、プロパティを設定
  return errorConstructor(ctx, thisValue, args);
}

Value aggregateErrorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisValueがAggregateErrorオブジェクトでない場合、新しいAggregateErrorオブジェクトを作成
  if (!thisValue.isObject() || !thisValue.asObject()->isError() ||
      !static_cast<ErrorObject*>(thisValue.asObject())->isType(ErrorType::AggregateError)) {
    auto* proto = ctx->aggregateErrorPrototype();
    std::string message = (args.size() > 1 && !args[1].isUndefined()) ? args[1].toString()->value() : "";
    Object* cause = nullptr;

    // ES2022: エラー原因（cause）オプション
    if (args.size() > 2 && args[2].isObject() &&
        args[2].asObject()->has("cause")) {
      cause = args[2].asObject()->get("cause").asObject();
    }

    auto* error = new ErrorObject(ErrorType::AggregateError, message, cause);
    error->setPrototype(proto);

    // エラー配列を設定
    if (!args.empty() && args[0].isObject()) {
      auto* errors = ctx->createArray();
      auto* iterator = ctx->getIterator(args[0]);

      // イテレータからエラーを収集
      size_t index = 0;
      while (true) {
        auto next = iterator->next();
        if (next.done) {
          break;
        }

        errors->defineProperty(std::to_string(index++),
                               PropertyDescriptor(next.value, true, true, true));
      }

      error->defineProperty("errors", PropertyDescriptor(Value(errors), true, true, true));
    }

    return Value(error);
  }

  // thisValueがAggregateErrorオブジェクトの場合、プロパティを設定
  if (!args.empty() && args[0].isObject()) {
    auto* errors = ctx->createArray();
    auto* iterator = ctx->getIterator(args[0]);

    // イテレータからエラーを収集
    size_t index = 0;
    while (true) {
      auto next = iterator->next();
      if (next.done) {
        break;
      }

      errors->defineProperty(std::to_string(index++),
                             PropertyDescriptor(next.value, true, true, true));
    }

    thisValue.asObject()->defineProperty("errors", PropertyDescriptor(Value(errors), true, true, true));
  }

  // メッセージとcauseの設定
  return errorConstructor(ctx, thisValue, std::vector<Value>(args.begin() + 1, args.end()));
}

// Error.prototype.toString
Value errorToString(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisValueがオブジェクトでない場合はエラー
  if (!thisValue.isObject()) {
    return ctx->throwTypeError("Error.prototype.toString called on non-object");
  }

  auto* obj = thisValue.asObject();

  // nameとmessageプロパティを取得
  Value name = obj->get("name");
  if (name.isUndefined()) {
    name = Value("Error");
  } else {
    name = name.toString();
  }

  Value message = obj->get("message");
  if (message.isUndefined()) {
    message = Value("");
  } else {
    message = message.toString();
  }

  // 結果の文字列を構築
  if (name.toString()->value().empty()) {
    return message;
  }

  if (message.toString()->value().empty()) {
    return name;
  }

  return Value(ctx->createString(name.toString()->value() + ": " + message.toString()->value()));
}

// エラープロトタイプの初期化
void initializeErrorPrototype(ExecutionContext* ctx, Object* prototype) {
  // プロトタイププロパティの設定
  prototype->defineProperty("constructor", PropertyDescriptor(Value::undefined(), true, false, true));
  prototype->defineProperty("name", PropertyDescriptor(Value("Error"), true, true, true));
  prototype->defineProperty("message", PropertyDescriptor(Value(""), true, true, true));

  // toStringメソッドの追加
  auto* toStringFunc = ctx->createFunction(errorToString, "toString", 0);
  prototype->defineProperty("toString", PropertyDescriptor(Value(toStringFunc), true, false, true));
}

// 特定のエラータイプのプロトタイプを初期化
void initializeSpecificErrorPrototype(ExecutionContext* ctx, ErrorType type, Object* prototype, Object* errorPrototype) {
  // 基本エラープロトタイプを継承
  prototype->setPrototype(errorPrototype);

  // 型特有の名前を設定
  std::string typeName;
  switch (type) {
    case ErrorType::EvalError:
      typeName = "EvalError";
      break;
    case ErrorType::RangeError:
      typeName = "RangeError";
      break;
    case ErrorType::ReferenceError:
      typeName = "ReferenceError";
      break;
    case ErrorType::SyntaxError:
      typeName = "SyntaxError";
      break;
    case ErrorType::TypeError:
      typeName = "TypeError";
      break;
    case ErrorType::URIError:
      typeName = "URIError";
      break;
    case ErrorType::AggregateError:
      typeName = "AggregateError";
      break;
    default:
      typeName = "Error";
      break;
  }

  prototype->defineProperty("name", PropertyDescriptor(Value(typeName), true, true, true));
  prototype->defineProperty("message", PropertyDescriptor(Value(""), true, true, true));

  // AggregateErrorの場合は追加のプロパティを設定
  if (type == ErrorType::AggregateError) {
    auto* errors = ctx->createArray();
    prototype->defineProperty("errors", PropertyDescriptor(Value(errors), true, true, true));
  }
}

// エラーオブジェクトの登録
void registerErrorObjects(ExecutionContext* ctx, Object* global) {
  // ベースのErrorプロトタイプとコンストラクタを作成
  auto* errorProto = ctx->createObject();
  auto* errorConstructorObj = ctx->createFunction(errorConstructor, "Error", 1);

  // Errorプロトタイプの初期化
  initializeErrorPrototype(ctx, errorProto);

  // Error.prototypeの設定
  errorConstructorObj->defineProperty("prototype", PropertyDescriptor(Value(errorProto), false, false, false));

  // プロトタイプのconstructorを設定
  errorProto->defineProperty("constructor", PropertyDescriptor(Value(errorConstructorObj), true, false, true));

  // グローバルオブジェクトに登録
  global->defineProperty("Error", PropertyDescriptor(Value(errorConstructorObj), true, false, true));

  // コンテキストに保存
  ctx->setErrorPrototype(errorProto);
  ctx->setErrorConstructor(errorConstructorObj);

  // 派生エラータイプを作成・登録

  // 1. EvalError
  auto* evalErrorProto = ctx->createObject();
  auto* evalErrorConstructorObj = ctx->createFunction(evalErrorConstructor, "EvalError", 1);

  initializeSpecificErrorPrototype(ctx, ErrorType::EvalError, evalErrorProto, errorProto);

  evalErrorConstructorObj->defineProperty("prototype", PropertyDescriptor(Value(evalErrorProto), false, false, false));
  evalErrorProto->defineProperty("constructor", PropertyDescriptor(Value(evalErrorConstructorObj), true, false, true));

  global->defineProperty("EvalError", PropertyDescriptor(Value(evalErrorConstructorObj), true, false, true));

  ctx->setEvalErrorPrototype(evalErrorProto);
  ctx->setEvalErrorConstructor(evalErrorConstructorObj);

  // 2. RangeError
  auto* rangeErrorProto = ctx->createObject();
  auto* rangeErrorConstructorObj = ctx->createFunction(rangeErrorConstructor, "RangeError", 1);

  initializeSpecificErrorPrototype(ctx, ErrorType::RangeError, rangeErrorProto, errorProto);

  rangeErrorConstructorObj->defineProperty("prototype", PropertyDescriptor(Value(rangeErrorProto), false, false, false));
  rangeErrorProto->defineProperty("constructor", PropertyDescriptor(Value(rangeErrorConstructorObj), true, false, true));

  global->defineProperty("RangeError", PropertyDescriptor(Value(rangeErrorConstructorObj), true, false, true));

  ctx->setRangeErrorPrototype(rangeErrorProto);
  ctx->setRangeErrorConstructor(rangeErrorConstructorObj);

  // 3. ReferenceError
  auto* refErrorProto = ctx->createObject();
  auto* refErrorConstructorObj = ctx->createFunction(referenceErrorConstructor, "ReferenceError", 1);

  initializeSpecificErrorPrototype(ctx, ErrorType::ReferenceError, refErrorProto, errorProto);

  refErrorConstructorObj->defineProperty("prototype", PropertyDescriptor(Value(refErrorProto), false, false, false));
  refErrorProto->defineProperty("constructor", PropertyDescriptor(Value(refErrorConstructorObj), true, false, true));

  global->defineProperty("ReferenceError", PropertyDescriptor(Value(refErrorConstructorObj), true, false, true));

  ctx->setReferenceErrorPrototype(refErrorProto);
  ctx->setReferenceErrorConstructor(refErrorConstructorObj);

  // 4. SyntaxError
  auto* syntaxErrorProto = ctx->createObject();
  auto* syntaxErrorConstructorObj = ctx->createFunction(syntaxErrorConstructor, "SyntaxError", 1);

  initializeSpecificErrorPrototype(ctx, ErrorType::SyntaxError, syntaxErrorProto, errorProto);

  syntaxErrorConstructorObj->defineProperty("prototype", PropertyDescriptor(Value(syntaxErrorProto), false, false, false));
  syntaxErrorProto->defineProperty("constructor", PropertyDescriptor(Value(syntaxErrorConstructorObj), true, false, true));

  global->defineProperty("SyntaxError", PropertyDescriptor(Value(syntaxErrorConstructorObj), true, false, true));

  ctx->setSyntaxErrorPrototype(syntaxErrorProto);
  ctx->setSyntaxErrorConstructor(syntaxErrorConstructorObj);

  // 5. TypeError
  auto* typeErrorProto = ctx->createObject();
  auto* typeErrorConstructorObj = ctx->createFunction(typeErrorConstructor, "TypeError", 1);

  initializeSpecificErrorPrototype(ctx, ErrorType::TypeError, typeErrorProto, errorProto);

  typeErrorConstructorObj->defineProperty("prototype", PropertyDescriptor(Value(typeErrorProto), false, false, false));
  typeErrorProto->defineProperty("constructor", PropertyDescriptor(Value(typeErrorConstructorObj), true, false, true));

  global->defineProperty("TypeError", PropertyDescriptor(Value(typeErrorConstructorObj), true, false, true));

  ctx->setTypeErrorPrototype(typeErrorProto);
  ctx->setTypeErrorConstructor(typeErrorConstructorObj);

  // 6. URIError
  auto* uriErrorProto = ctx->createObject();
  auto* uriErrorConstructorObj = ctx->createFunction(uriErrorConstructor, "URIError", 1);

  initializeSpecificErrorPrototype(ctx, ErrorType::URIError, uriErrorProto, errorProto);

  uriErrorConstructorObj->defineProperty("prototype", PropertyDescriptor(Value(uriErrorProto), false, false, false));
  uriErrorProto->defineProperty("constructor", PropertyDescriptor(Value(uriErrorConstructorObj), true, false, true));

  global->defineProperty("URIError", PropertyDescriptor(Value(uriErrorConstructorObj), true, false, true));

  ctx->setURIErrorPrototype(uriErrorProto);
  ctx->setURIErrorConstructor(uriErrorConstructorObj);

  // 7. AggregateError (ES2021)
  auto* aggErrorProto = ctx->createObject();
  auto* aggErrorConstructorObj = ctx->createFunction(aggregateErrorConstructor, "AggregateError", 2);

  initializeSpecificErrorPrototype(ctx, ErrorType::AggregateError, aggErrorProto, errorProto);

  aggErrorConstructorObj->defineProperty("prototype", PropertyDescriptor(Value(aggErrorProto), false, false, false));
  aggErrorProto->defineProperty("constructor", PropertyDescriptor(Value(aggErrorConstructorObj), true, false, true));

  global->defineProperty("AggregateError", PropertyDescriptor(Value(aggErrorConstructorObj), true, false, true));

  ctx->setAggregateErrorPrototype(aggErrorProto);
  ctx->setAggregateErrorConstructor(aggErrorConstructorObj);
}

}  // namespace aero
