/**
 * @file tool_result.h
 * @brief MCPツール実行結果クラスの定義
 * @version 0.1.0
 * @copyright 2023 AeroJS Project
 * @license MIT
 */

#ifndef AEROJS_CORE_MCP_TOOL_RESULT_H
#define AEROJS_CORE_MCP_TOOL_RESULT_H

#include <string>
#include <vector>
#include <memory>
#include "../../../utils/memory/smart_ptr/ref_counted.h"

namespace aerojs {
namespace core {
namespace mcp {
namespace tool {

/**
 * @class ToolResult
 * @brief ツール実行結果を表すクラス
 * 
 * ツールの実行結果を格納し、成功/エラー状態、メッセージ、
 * 実行結果のコンテンツを管理します。
 */
class ToolResult : public utils::RefCounted {
public:
    /**
     * @brief 成功結果を作成
     * @param content 結果内容（JSON形式）
     * @return ToolResult* 結果インスタンス
     */
    static ToolResult* createSuccess(const std::string& content);
    
    /**
     * @brief エラー結果を作成
     * @param code エラーコード
     * @param message エラーメッセージ
     * @return ToolResult* 結果インスタンス
     */
    static ToolResult* createError(int code, const std::string& message);
    
    /**
     * @brief 結果コードを取得
     * @return int 結果コード（0は成功）
     */
    int getCode() const { return m_code; }
    
    /**
     * @brief エラーメッセージを取得
     * @return const std::string& エラーメッセージ
     */
    const std::string& getMessage() const { return m_message; }
    
    /**
     * @brief 結果内容を取得
     * @return const std::string& 結果内容（JSON形式）
     */
    const std::string& getContent() const { return m_content; }
    
    /**
     * @brief 成功したかどうかを確認
     * @return bool 成功時はtrue
     */
    bool isSuccess() const { return m_code == 0; }
    
    /**
     * @brief 結果をJSON形式の文字列として取得
     * @return std::string JSON形式の結果
     */
    std::string toJSON() const;

private:
    /**
     * @brief コンストラクタ
     * @param code 結果コード
     * @param message メッセージ
     * @param content 結果内容
     */
    ToolResult(int code, const std::string& message, const std::string& content);
    
    int m_code;              ///< 結果コード（0は成功）
    std::string m_message;   ///< メッセージ
    std::string m_content;   ///< 結果内容（JSON形式）
};

// スマートポインタ型の定義
using ToolResultPtr = utils::RefPtr<ToolResult>;

} // namespace tool
} // namespace mcp
} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_MCP_TOOL_RESULT_H 