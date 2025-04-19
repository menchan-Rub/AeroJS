/**
 * @file tool_result.cpp
 * @brief MCPツール実行結果クラスの実装
 * @version 0.1.0
 * @copyright 2023 AeroJS Project
 * @license MIT
 */

#include "tool_result.h"
#include "../../../utils/json/json_builder.h"
#include "../../../utils/logger/logger.h"

namespace aerojs {
namespace core {
namespace mcp {
namespace tool {

using namespace utils;

ToolResult::ToolResult(int code, const std::string& message, const std::string& content)
    : m_code(code)
    , m_message(message)
    , m_content(content) {
}

ToolResult* ToolResult::createSuccess(const std::string& content) {
    return new ToolResult(0, "", content);
}

ToolResult* ToolResult::createError(int code, const std::string& message) {
    if (code == 0) {
        Logger::error("ToolResult::createError: エラーコードは0以外である必要があります");
        code = -1;
    }
    return new ToolResult(code, message, "");
}

std::string ToolResult::toJSON() const {
    JSONBuilder builder;
    builder.beginObject()
        .addProperty("code", m_code)
        .addProperty("message", m_message);
    
    if (!m_content.empty()) {
        builder.addRawProperty("content", m_content);
    }
    
    return builder.endObject().toString();
}

} // namespace tool
} // namespace mcp
} // namespace core
} // namespace aerojs 