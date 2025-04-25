/**
 * @file exception.cpp
 * @brief AeroJS JavaScript エンジンの例外クラスの実装
 * @version 0.1.0
 * @license MIT
 */

#include "exception.h"

#include <sstream>

#include "context.h"
#include "value.h"

namespace aerojs {
namespace core {

Exception::Exception(Context* ctx, const std::string& message, ErrorType type)
    : m_context(ctx), m_message(message), m_type(type) {
  // スタックトレースは初期状態では空
}

Exception* Exception::create(Context* ctx, const std::string& message, ErrorType type) {
  Exception* exception = new Exception(ctx, message, type);

  // 現在のコールスタックからスタックトレースを取得
  // 実際の実装では、コールスタックを取得するロジックを実装する必要がある
  // ここでは仮実装として現在地点のみを追加
  exception->addStackTraceElement(StackTraceElement(
      "<anonymous>",
      "<native>",
      1,
      1));

  return exception;
}

Exception* Exception::fromErrorObject(Context* ctx, Value* errorValue) {
  if (!errorValue || !errorValue->isObject()) {
    // エラーオブジェクトでない場合はデフォルトのエラーを返す
    return create(ctx, "Unknown error", ErrorType::Error);
  }

  // エラーオブジェクトからプロパティを抽出
  std::string message = "Unknown error";
  ErrorType type = ErrorType::Error;

  // messageプロパティの取得
  Value* messageValue = errorValue->getProperty("message");
  if (messageValue && messageValue->isString()) {
    message = messageValue->toString();
  }

  // nameプロパティからエラー種別を判別
  Value* nameValue = errorValue->getProperty("name");
  if (nameValue && nameValue->isString()) {
    std::string name = nameValue->toString();

    if (name == "EvalError") {
      type = ErrorType::EvalError;
    } else if (name == "RangeError") {
      type = ErrorType::RangeError;
    } else if (name == "ReferenceError") {
      type = ErrorType::ReferenceError;
    } else if (name == "SyntaxError") {
      type = ErrorType::SyntaxError;
    } else if (name == "TypeError") {
      type = ErrorType::TypeError;
    } else if (name == "URIError") {
      type = ErrorType::URIError;
    } else if (name == "AggregateError") {
      type = ErrorType::AggregateError;
    } else if (name == "InternalError") {
      type = ErrorType::InternalError;
    }
  }

  // 例外オブジェクトを作成
  Exception* exception = new Exception(ctx, message, type);

  // スタックトレースの取得（stackプロパティがある場合）
  Value* stackValue = errorValue->getProperty("stack");
  if (stackValue && stackValue->isString()) {
    // スタック文字列からスタックトレース要素をパース
    // 実際の実装ではスタック文字列の形式に合わせてパースする必要がある
    std::string stackStr = stackValue->toString();
    // ここでは簡単な実装として、スタック文字列を行ごとに分割
    size_t pos = 0;
    std::string line;
    while ((pos = stackStr.find('\n')) != std::string::npos) {
      line = stackStr.substr(0, pos);
      // スタックトレース行のパース処理（フォーマットに合わせて実装）
      // 例: "at functionName (fileName:lineNumber:columnNumber)"
      // 簡易的な実装として、最初の例外エントリを追加
      if (!line.empty() && line.find("at ") != std::string::npos) {
        exception->addStackTraceElement(StackTraceElement(
            line.substr(line.find("at ") + 3),
            "",
            -1,
            -1));
      }
      stackStr.erase(0, pos + 1);
    }
  }

  return exception;
}

Value* Exception::toErrorObject() const {
  // コンテキストから適切なエラーコンストラクタを取得
  Value* constructor = nullptr;
  std::string constructorName = getErrorConstructorName(m_type);

  // グローバルオブジェクトからコンストラクタを取得
  Value* global = m_context->getGlobalObject();
  constructor = global->getProperty(constructorName);

  // コンストラクタが見つからない場合はErrorコンストラクタを使用
  if (!constructor || !constructor->isFunction()) {
    constructor = global->getProperty("Error");
  }

  // エラーオブジェクトの作成
  Value* errorObj = nullptr;
  if (constructor && constructor->isFunction()) {
    // コンストラクタを呼び出してエラーオブジェクトを作成
    Value* messageValue = Value::createString(m_context, m_message);
    Value* args[] = {messageValue};
    errorObj = constructor->call(nullptr, 1, args);
  }

  // コンストラクタが見つからないか呼び出しに失敗した場合は、手動でエラーオブジェクトを作成
  if (!errorObj) {
    errorObj = Value::createObject(m_context);
    errorObj->setProperty("name", Value::createString(m_context, constructorName));
    errorObj->setProperty("message", Value::createString(m_context, m_message));
  }

  // スタックトレースの設定
  if (!m_stackTrace.empty()) {
    std::string stackStr = toString();
    errorObj->setProperty("stack", Value::createString(m_context, stackStr));
  }

  return errorObj;
}

std::string Exception::toString() const {
  std::stringstream ss;

  // エラー種別とメッセージ
  ss << getErrorConstructorName(m_type) << ": " << m_message << "\n";

  // スタックトレース
  for (const auto& element : m_stackTrace) {
    ss << "    at ";
    if (!element.functionName.empty()) {
      ss << element.functionName;
    } else {
      ss << "<anonymous>";
    }

    if (!element.fileName.empty()) {
      ss << " (" << element.fileName;
      if (element.lineNumber >= 0) {
        ss << ":" << element.lineNumber;
        if (element.columnNumber >= 0) {
          ss << ":" << element.columnNumber;
        }
      }
      ss << ")";
    }
    ss << "\n";
  }

  return ss.str();
}

std::string Exception::getErrorConstructorName(ErrorType type) {
  switch (type) {
    case ErrorType::EvalError:
      return "EvalError";
    case ErrorType::RangeError:
      return "RangeError";
    case ErrorType::ReferenceError:
      return "ReferenceError";
    case ErrorType::SyntaxError:
      return "SyntaxError";
    case ErrorType::TypeError:
      return "TypeError";
    case ErrorType::URIError:
      return "URIError";
    case ErrorType::AggregateError:
      return "AggregateError";
    case ErrorType::InternalError:
      return "InternalError";
    case ErrorType::Error:
    default:
      return "Error";
  }
}

}  // namespace core
}  // namespace aerojs