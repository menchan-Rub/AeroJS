/**
 * @file mcp_utils.h
 * @brief Model Context Protocol (MCP) ユーティリティ関数
 * 
 * このファイルはMCPプロトコルサーバーで使用するユーティリティ関数を定義します。
 * JSON操作、ツール登録、認証などの汎用機能を提供します。
 * 
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#ifndef AERO_MCP_UTILS_H
#define AERO_MCP_UTILS_H

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <nlohmann/json.hpp>
#include "../server/mcp_server.h"

namespace aero {
namespace mcp {

/**
 * @brief JSONスキーマの検証結果
 */
struct SchemaValidationResult {
    bool valid;                  ///< スキーマが有効かどうか
    std::string errorMessage;    ///< エラーメッセージ（エラーがある場合）
    std::string errorPath;       ///< エラーが発生したパス
};

/**
 * @brief JSONスキーマバリデーター
 * 
 * JSONスキーマに基づいてJSONデータを検証するクラス
 */
class SchemaValidator {
public:
    /**
     * @brief コンストラクタ
     * @param schema JSONスキーマ文字列
     */
    explicit SchemaValidator(const std::string& schema);
    
    /**
     * @brief JSONデータを検証
     * @param data 検証するJSONデータ
     * @return 検証結果
     */
    SchemaValidationResult validate(const std::string& data) const;
    
    /**
     * @brief JSONデータを検証
     * @param data 検証するJSONオブジェクト
     * @return 検証結果
     */
    SchemaValidationResult validate(const nlohmann::json& data) const;
    
private:
    nlohmann::json m_schema;  ///< JSONスキーマ
};

/**
 * @brief ツール登録ヘルパー
 * 
 * MCPサーバーにツールを簡単に登録するためのヘルパー関数
 * 
 * @param server MCPサーバーインスタンス
 * @param name ツール名
 * @param description ツールの説明
 * @param type ツールの種類
 * @param inputSchema 入力スキーマJSON文字列
 * @param outputSchema 出力スキーマJSON文字列
 * @param handler ツールのハンドラ関数
 * @return 登録が成功したかどうか
 */
bool registerToolHelper(
    MCPServer& server,
    const std::string& name,
    const std::string& description,
    ToolType type,
    const std::string& inputSchema,
    const std::string& outputSchema,
    std::function<std::string(const std::string&)> handler
);

/**
 * @brief 認証トークンの生成
 * 
 * MCPサーバーの認証に使用するセキュアなトークンを生成
 * 
 * @param length トークンの長さ（デフォルト: 32）
 * @return 生成されたトークン
 */
std::string generateAuthToken(size_t length = 32);

/**
 * @brief 認証トークンの検証
 * 
 * 提供されたトークンが有効かどうかを検証
 * 
 * @param token 検証するトークン
 * @param validTokens 有効なトークンのリスト
 * @return トークンが有効かどうか
 */
bool validateAuthToken(const std::string& token, const std::vector<std::string>& validTokens);

/**
 * @brief JSONレスポンスの作成
 * 
 * 成功またはエラーのJSONレスポンスを作成
 * 
 * @param success 成功したかどうか
 * @param message メッセージ
 * @param data 追加データ（オプション）
 * @return JSON文字列
 */
std::string createJsonResponse(
    bool success,
    const std::string& message,
    const std::optional<nlohmann::json>& data = std::nullopt
);

/**
 * @brief JSONエラーレスポンスの作成
 * 
 * エラーコードとメッセージを含むJSONエラーレスポンスを作成
 * 
 * @param errorCode エラーコード
 * @param errorMessage エラーメッセージ
 * @param details エラーの詳細情報（オプション）
 * @return JSON文字列
 */
std::string createJsonErrorResponse(
    int errorCode,
    const std::string& errorMessage,
    const std::optional<nlohmann::json>& details = std::nullopt
);

/**
 * @brief JSONリクエストの検証
 * 
 * リクエストがJSON形式であり、必要なフィールドが含まれているかを検証
 * 
 * @param requestJson リクエストのJSON文字列
 * @param requiredFields 必要なフィールドのリスト
 * @return 検証結果と解析されたJSONオブジェクト
 */
std::pair<bool, nlohmann::json> validateJsonRequest(
    const std::string& requestJson,
    const std::vector<std::string>& requiredFields
);

/**
 * @brief バイナリデータをBase64にエンコード
 * 
 * @param data エンコードするバイナリデータ
 * @param length データの長さ
 * @return Base64エンコードされた文字列
 */
std::string base64Encode(const unsigned char* data, size_t length);

/**
 * @brief Base64文字列をバイナリデータにデコード
 * 
 * @param base64String デコードするBase64文字列
 * @return デコードされたバイナリデータ
 */
std::vector<unsigned char> base64Decode(const std::string& base64String);

} // namespace mcp
} // namespace aero

#endif // AERO_MCP_UTILS_H 