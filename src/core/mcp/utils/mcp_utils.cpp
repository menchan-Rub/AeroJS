/**
 * @file mcp_utils.cpp
 * @brief Model Context Protocol (MCP) ユーティリティ関数の実装
 * 
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#include "mcp_utils.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

namespace aero {
namespace mcp {

// SchemaValidatorの実装
SchemaValidator::SchemaValidator(const std::string& schema) {
    try {
        m_schema = nlohmann::json::parse(schema);
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("Invalid JSON schema: " + std::string(e.what()));
    }
}

SchemaValidationResult SchemaValidator::validate(const std::string& data) const {
    try {
        auto jsonData = nlohmann::json::parse(data);
        return validate(jsonData);
    } catch (const nlohmann::json::exception& e) {
        return {false, "Invalid JSON data: " + std::string(e.what()), ""};
    }
}

SchemaValidationResult SchemaValidator::validate(const nlohmann::json& data) const {
    // 簡易的なスキーマ検証の実装
    // 注: 実際の実装では、完全なJSON Schemaバリデーションライブラリを使用することを推奨
    
    // ここでは基本的な型チェックのみを行う
    if (m_schema.contains("type")) {
        std::string schemaType = m_schema["type"];
        bool valid = false;
        
        if (schemaType == "object" && data.is_object()) {
            valid = true;
            
            // requiredフィールドのチェック
            if (m_schema.contains("required") && m_schema["required"].is_array()) {
                for (const auto& requiredField : m_schema["required"]) {
                    if (!data.contains(requiredField)) {
                        return {false, "Missing required field", requiredField};
                    }
                }
            }
            
            // propertiesのチェック
            if (m_schema.contains("properties") && m_schema["properties"].is_object()) {
                for (const auto& [propName, propSchema] : m_schema["properties"].items()) {
                    if (data.contains(propName)) {
                        // プロパティの存在を確認、型チェックなど
                        if (propSchema.contains("type")) {
                            std::string propType = propSchema["type"];
                            if (propType == "string" && !data[propName].is_string()) {
                                return {false, "Property type mismatch", propName};
                            } else if (propType == "number" && !data[propName].is_number()) {
                                return {false, "Property type mismatch", propName};
                            } else if (propType == "boolean" && !data[propName].is_boolean()) {
                                return {false, "Property type mismatch", propName};
                            } else if (propType == "array" && !data[propName].is_array()) {
                                return {false, "Property type mismatch", propName};
                            } else if (propType == "object" && !data[propName].is_object()) {
                                return {false, "Property type mismatch", propName};
                            }
                        }
                    }
                }
            }
        } else if (schemaType == "array" && data.is_array()) {
            valid = true;
            
            // items チェック
            if (m_schema.contains("items") && m_schema["items"].is_object() && !data.empty()) {
                for (size_t i = 0; i < data.size(); ++i) {
                    auto subValidator = SchemaValidator(m_schema["items"].dump());
                    auto subResult = subValidator.validate(data[i]);
                    if (!subResult.valid) {
                        return {false, "Item validation failed: " + subResult.errorMessage, "items[" + std::to_string(i) + "]." + subResult.errorPath};
                    }
                }
            }
        } else if (schemaType == "string" && data.is_string()) {
            valid = true;
        } else if (schemaType == "number" && data.is_number()) {
            valid = true;
        } else if (schemaType == "boolean" && data.is_boolean()) {
            valid = true;
        } else if (schemaType == "null" && data.is_null()) {
            valid = true;
        }
        
        if (!valid) {
            return {false, "Type mismatch: expected " + schemaType, ""};
        }
    }
    
    return {true, "", ""};
}

// ツール登録ヘルパー
bool registerToolHelper(
    MCPServer& server,
    const std::string& name,
    const std::string& description,
    ToolType type,
    const std::string& inputSchema,
    const std::string& outputSchema,
    std::function<std::string(const std::string&)> handler
) {
    try {
        Tool tool;
        tool.name = name;
        tool.description = description;
        tool.type = type;
        tool.inputSchema = inputSchema;
        tool.outputSchema = outputSchema;
        tool.handler = handler;
        
        server.registerTool(tool);
        return true;
    } catch (const std::exception& e) {
        // ログ出力
        std::cerr << "Failed to register tool: " << name << " - " << e.what() << std::endl;
        return false;
    }
}

// 認証トークン生成
std::string generateAuthToken(size_t length) {
    static const char charset[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    
    std::string token(length, 0);
    for (size_t i = 0; i < length; ++i) {
        token[i] = charset[dist(gen)];
    }
    
    return token;
}

// 認証トークン検証
bool validateAuthToken(const std::string& token, const std::vector<std::string>& validTokens) {
    return std::find(validTokens.begin(), validTokens.end(), token) != validTokens.end();
}

// JSONレスポンス作成
std::string createJsonResponse(
    bool success,
    const std::string& message,
    const std::optional<nlohmann::json>& data
) {
    nlohmann::json response = {
        {"success", success},
        {"message", message}
    };
    
    if (data.has_value()) {
        response["data"] = data.value();
    }
    
    return response.dump();
}

// JSONエラーレスポンス作成
std::string createJsonErrorResponse(
    int errorCode,
    const std::string& errorMessage,
    const std::optional<nlohmann::json>& details
) {
    nlohmann::json response = {
        {"error", {
            {"code", errorCode},
            {"message", errorMessage}
        }}
    };
    
    if (details.has_value()) {
        response["error"]["details"] = details.value();
    }
    
    return response.dump();
}

// JSONリクエスト検証
std::pair<bool, nlohmann::json> validateJsonRequest(
    const std::string& requestJson,
    const std::vector<std::string>& requiredFields
) {
    nlohmann::json jsonObj;
    
    try {
        jsonObj = nlohmann::json::parse(requestJson);
    } catch (const nlohmann::json::exception& e) {
        return {false, nlohmann::json()};
    }
    
    for (const auto& field : requiredFields) {
        if (!jsonObj.contains(field)) {
            return {false, jsonObj};
        }
    }
    
    return {true, jsonObj};
}

// Base64エンコード
std::string base64Encode(const unsigned char* data, size_t length) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, data, static_cast<int>(length));
    BIO_flush(b64);
    
    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);
    
    std::string result(bptr->data, bptr->length - 1);  // 最後の改行を除去
    BIO_free_all(b64);
    
    // 改行を削除
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
    
    return result;
}

// Base64デコード
std::vector<unsigned char> base64Decode(const std::string& base64String) {
    std::string input = base64String;
    
    // Base64文字列の末尾に不足しているパディング（=）を追加
    size_t padding = (4 - (input.length() % 4)) % 4;
    input.append(padding, '=');
    
    // BIOチェーン作成
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO* bmem = BIO_new_mem_buf(input.c_str(), static_cast<int>(input.length()));
    bmem = BIO_push(b64, bmem);
    
    // 出力バッファ
    std::vector<unsigned char> output(input.length());
    int decodedSize = BIO_read(bmem, output.data(), static_cast<int>(output.size()));
    
    BIO_free_all(bmem);
    
    if (decodedSize <= 0) {
        return {};
    }
    
    output.resize(decodedSize);
    return output;
}

} // namespace mcp
} // namespace aero 