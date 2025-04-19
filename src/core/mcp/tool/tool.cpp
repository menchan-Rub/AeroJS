/**
 * @file tool.cpp
 * @brief MCPツールの基本インターフェース実装
 * @version 0.1.0
 * @copyright 2023 AeroJS Project
 * @license MIT
 */

#include "tool.h"
#include "tool_result.h"
#include "../../../utils/json/json_parser.h"
#include "../../../utils/logging/logger.h"

namespace aerojs {
namespace core {
namespace mcp {
namespace tool {

utils::JSONValue* Tool::parseParams(const std::string& params) const {
    if (params.empty()) {
        return nullptr;
    }
    
    try {
        utils::JSONParser parser;
        return parser.parse(params);
    } catch (const std::exception& e) {
        AEROJS_LOG_ERROR("JSONパースエラー: %s", e.what());
        return nullptr;
    }
}

ToolResult* Tool::createError(int code, const std::string& message) const {
    return ToolResult::createError(code, message);
}

ToolResult* Tool::createSuccess(const std::string& content) const {
    return ToolResult::createSuccess(content);
}

} // namespace tool
} // namespace mcp
} // namespace core
} // namespace aerojs 