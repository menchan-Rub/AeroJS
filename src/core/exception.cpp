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
#include "execution_context.h"

namespace aerojs {
namespace core {

Exception::Exception(Context* ctx, const std::string& message, ErrorType type)
    : m_context(ctx), m_message(message), m_type(type) {
  // スタックトレースは初期状態では空
}

Exception* Exception::create(Context* ctx, const std::string& message, ErrorType type) {
  Exception* exception = new Exception(ctx, message, type);

  // 現在のコールスタックからスタックトレースを取得
  if (ctx) {
    ExecutionContext* execContext = ctx->currentExecutionContext();
    if (execContext) {
      // 現在の実行コンテキストからコールスタックを取得
      const CallStack& callStack = execContext->getCallStack();
      for (const CallFrame& frame : callStack.getFrames()) {
        // 各フレーム情報からスタックトレース要素を作成
        std::string functionName = frame.getFunctionName();
        if (functionName.empty()) {
          functionName = "<anonymous>";
        }
        
        std::string fileName = frame.getFileName();
        if (fileName.empty()) {
          fileName = "<native>";
        }
        
        exception->addStackTraceElement(StackTraceElement(
            functionName,
            fileName,
            frame.getLineNumber(),
            frame.getColumnNumber()));
      }
    } else {
      // 実行コンテキストが取得できない場合は最低限の情報だけ追加
      exception->addStackTraceElement(StackTraceElement(
          "<anonymous>",
          "<native>",
          0,
          0));
    }
  }

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
    std::string stackStr = stackValue->toString();
    // スタックトレース文字列のパース実装
    std::vector<std::string> lines;
    std::istringstream ss(stackStr);
    std::string line;
    
    // 行ごとに分割
    while (std::getline(ss, line)) {
      lines.push_back(line);
    }
    
    // 最初の行はエラーメッセージのため無視する可能性がある
    bool skipFirstLine = false;
    if (!lines.empty()) {
      // エラーメッセージが含まれている場合は最初の行をスキップ
      if (lines[0].find(":") != std::string::npos && 
          lines[0].find("Error") != std::string::npos) {
        skipFirstLine = true;
      }
    }
    
    // 各行をパース
    for (size_t i = skipFirstLine ? 1 : 0; i < lines.size(); i++) {
      const auto& line = lines[i];
      
      // V8形式: "    at functionName (filename:line:column)"
      // SpiderMonkey形式: "functionName@filename:line:column"
      // JavaScriptCore形式: "functionName@filename:line:column"
      
      size_t atPos = line.find("at ");
      size_t atSymbolPos = line.find("@");
      std::string funcName;
      std::string location;
      
      if (atPos != std::string::npos) {
        // V8形式
        std::string entry = line.substr(atPos + 3);
        
        // 関数名と位置情報を分離
        size_t openParenPos = entry.find(" (");
        size_t closeParenPos = entry.find(")");
        
        if (openParenPos != std::string::npos && closeParenPos != std::string::npos) {
          funcName = entry.substr(0, openParenPos);
          location = entry.substr(openParenPos + 2, closeParenPos - openParenPos - 2);
        } else {
          // 括弧がない場合は全体を関数名とする
          funcName = entry;
          location = "";
        }
      } else if (atSymbolPos != std::string::npos) {
        // SpiderMonkey/JavaScriptCore形式
        funcName = line.substr(0, atSymbolPos);
        location = line.substr(atSymbolPos + 1);
      } else {
        // 形式が認識できない場合はスキップ
        continue;
      }
      
      // 関数名のトリミング
      funcName = std::regex_replace(funcName, std::regex("^\\s+|\\s+$"), "");
      
      // 位置情報（ファイル名:行:列）を分解
      std::string fileName;
      int lineNumber = -1;
      int columnNumber = -1;
      
      if (!location.empty()) {
        std::vector<std::string> parts;
        std::istringstream locStream(location);
        std::string part;
        
        while (std::getline(locStream, part, ':')) {
          parts.push_back(part);
        }
        
        if (parts.size() >= 1) {
          fileName = parts[0];
        }
        
        if (parts.size() >= 2) {
          try {
            lineNumber = std::stoi(parts[1]);
          } catch (const std::exception&) {
            // 数値変換エラーは無視
          }
        }
        
        if (parts.size() >= 3) {
          try {
            columnNumber = std::stoi(parts[2]);
          } catch (const std::exception&) {
            // 数値変換エラーは無視
          }
        }
      }
      
      // スタックトレースに追加
      exception->addStackTraceElement(StackTraceElement(
          funcName.empty() ? "<anonymous>" : funcName,
          fileName.empty() ? "<unknown>" : fileName,
          lineNumber,
          columnNumber));
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