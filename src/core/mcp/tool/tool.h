/**
 * @file tool.h
 * @brief MCPツールの基本インターフェース定義
 * @version 0.1.0
 * @copyright 2023 AeroJS Project
 * @license MIT
 */

#ifndef AEROJS_CORE_MCP_TOOL_TOOL_H
#define AEROJS_CORE_MCP_TOOL_TOOL_H

#include <string>

#include "../../../utils/json/json_value.h"
#include "../../context.h"
#include "tool_result.h"

namespace aerojs {
namespace core {
namespace mcp {
namespace tool {

/**
 * @class Tool
 * @brief MCPツールの基本インターフェース
 *
 * すべてのMCPツールはこのインターフェースを実装する必要があります。
 * ツールは名前、カテゴリ、説明、パラメータスキーマ、実行ロジックを提供します。
 */
class Tool {
 public:
  virtual ~Tool() = default;

  /**
   * @brief ツール名を取得
   * @return std::string ツール名
   */
  virtual std::string getName() const = 0;

  /**
   * @brief ツールのカテゴリを取得
   * @return std::string カテゴリ名
   */
  virtual std::string getCategory() const = 0;

  /**
   * @brief ツールの説明を取得
   * @return std::string ツールの説明
   */
  virtual std::string getDescription() const = 0;

  /**
   * @brief パラメータなしで実行可能か確認
   * @return bool パラメータなしで実行可能な場合はtrue
   */
  virtual bool canExecuteWithoutParams() const = 0;

  /**
   * @brief パラメータを検証
   * @param params JSON形式のパラメータ
   * @return bool 検証成功時はtrue
   */
  virtual bool validateParams(const utils::JSONValue* params) const = 0;

  /**
   * @brief ツールを実行
   * @param ctx 実行コンテキスト
   * @param params JSON形式のパラメータ
   * @return ToolResult* 実行結果（呼び出し側で解放が必要）
   * @throws std::runtime_error 実行時エラーが発生した場合
   */
  virtual ToolResult* execute(Context* ctx, const std::string& params) = 0;

  /**
   * @brief ツールのスキーマを取得
   * @return std::string JSON形式のスキーマ定義
   */
  virtual std::string getSchema() const = 0;

 protected:
  /**
   * @brief パラメータをJSONとしてパース
   * @param params JSON文字列
   * @return utils::JSONValue* パース結果（nullptrの場合はパース失敗）
   */
  utils::JSONValue* parseParams(const std::string& params) const;

  /**
   * @brief エラー結果を生成
   * @param code エラーコード
   * @param message エラーメッセージ
   * @return ToolResult* エラー結果
   */
  ToolResult* createError(int code, const std::string& message) const;

  /**
   * @brief 成功結果を生成
   * @param content 結果内容（JSON形式）
   * @return ToolResult* 成功結果
   */
  ToolResult* createSuccess(const std::string& content) const;
};

}  // namespace tool
}  // namespace mcp
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_MCP_TOOL_TOOL_H