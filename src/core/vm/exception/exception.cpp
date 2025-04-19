/**
 * @file exception.cpp
 * @brief 仮想マシンの例外実装
 * 
 * このファイルはJavaScriptエンジンの仮想マシンで使用される例外クラスの実装を提供します。
 */

#include "exception.h"
#include <sstream>

namespace aerojs {
namespace core {

VMException::VMException(const std::string& message)
    : m_message(message) {
    // エラーオブジェクトを作成
    m_errorObject = Value::createError(message);
}

VMException::VMException(ValuePtr errorObject)
    : m_errorObject(errorObject) {
    // エラーオブジェクトからメッセージを取得
    if (errorObject) {
        m_message = errorObject->toString();
    } else {
        m_message = "Unknown error";
    }
}

VMException::~VMException() noexcept {
    // スマートポインタで管理しているので特別な後処理は不要
}

const char* VMException::what() const noexcept {
    return m_message.c_str();
}

ValuePtr VMException::getErrorObject() const {
    return m_errorObject;
}

const std::vector<std::string>& VMException::getStackTrace() const {
    return m_stackTrace;
}

void VMException::setStackTrace(const std::vector<std::string>& stackTrace) {
    m_stackTrace = stackTrace;

    if (!m_errorObject) {
        return;
    }

    // エラーオブジェクトの 'stack' プロパティに標準的なスタックトレース形式で設定
    std::ostringstream oss;
    auto typeName = m_errorObject->getTypeName();
    oss << typeName << ": " << m_message << "\n";
    for (const auto& frame : m_stackTrace) {
        oss << "  at " << frame << "\n";
    }

    m_errorObject->setProperty("stack", Value::createString(oss.str()));
}

void VMException::addStackFrame(const std::string& frameInfo) {
    m_stackTrace.push_back(frameInfo);

    if (!m_errorObject) {
        return;
    }

    // スタックフレーム追加後に 'stack' プロパティを更新
    std::ostringstream oss;
    auto typeName = m_errorObject->getTypeName();
    oss << typeName << ": " << m_message << "\n";
    for (const auto& frame : m_stackTrace) {
        oss << "  at " << frame << "\n";
    }

    m_errorObject->setProperty("stack", Value::createString(oss.str()));
}

std::string VMException::toString() const {
    std::ostringstream oss;
    
    // エラー型とメッセージを出力
    std::string errorType = "Error";
    if (m_errorObject) {
        // エラーオブジェクトから型名を取得
        errorType = m_errorObject->getTypeName();
    }
    
    oss << errorType << ": " << m_message << std::endl;
    
    // スタックトレースを出力
    if (!m_stackTrace.empty()) {
        oss << "スタックトレース:" << std::endl;
        for (const auto& frame : m_stackTrace) {
            oss << "  at " << frame << std::endl;
        }
    }
    
    return oss.str();
}

bool VMException::isInstanceOf(const std::string& typeName) const {
    if (!m_errorObject) {
        return false;
    }
    
    // エラーオブジェクトが指定した型のインスタンスかどうかを確認
    return m_errorObject->isInstanceOf(typeName);
}

} // namespace core
} // namespace aerojs 