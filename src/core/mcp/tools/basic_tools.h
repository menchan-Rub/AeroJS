/**
 * @file basic_tools.h
 * @brief Model Context Protocol (MCP) 基本ツールの定義
 *
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../server/mcp_server.h"

namespace aero {
namespace mcp {
namespace tools {

/**
 * @brief 基本的なツール提供クラス
 *
 * MCPサーバーに登録可能な標準ツールを提供します。
 * このクラスを使用して、標準的な操作（エンジン制御、スクリプト実行、評価など）を
 * MCPサーバーを介して利用できるようにします。
 */
class BasicTools {
 public:
  /**
   * @brief コンストラクタ
   *
   * @param server 登録先のMCPサーバー
   */
  explicit BasicTools(MCPServer* server);

  /**
   * @brief デストラクタ
   */
  ~BasicTools();

  /**
   * @brief 全ての基本ツールを登録する
   *
   * @return bool 登録成功時はtrue、失敗時はfalse
   */
  bool registerAll();

  /**
   * @brief エンジン制御ツールを登録する
   *
   * @return bool 登録成功時はtrue、失敗時はfalse
   */
  bool registerEngineTools();

  /**
   * @brief スクリプト実行ツールを登録する
   *
   * @return bool 登録成功時はtrue、失敗時はfalse
   */
  bool registerScriptTools();

  /**
   * @brief モジュール関連ツールを登録する
   *
   * @return bool 登録成功時はtrue、失敗時はfalse
   */
  bool registerModuleTools();

  /**
   * @brief デバッグツールを登録する
   *
   * @return bool 登録成功時はtrue、失敗時はfalse
   */
  bool registerDebugTools();

  /**
   * @brief メモリ管理ツールを登録する
   *
   * @return bool 登録成功時はtrue、失敗時はfalse
   */
  bool registerMemoryTools();

  /**
   * @brief パフォーマンス計測ツールを登録する
   *
   * @return bool 登録成功時はtrue、失敗時はfalse
   */
  bool registerPerformanceTools();

  /**
   * @brief ファイルシステム操作ツールを登録する
   *
   * @return bool 登録成功時はtrue、失敗時はfalse
   */
  bool registerFileSystemTools();

  /**
   * @brief 環境情報ツールを登録する
   *
   * @return bool 登録成功時はtrue、失敗時はfalse
   */
  bool registerEnvironmentTools();

 private:
  MCPServer* m_server;                         ///< MCPサーバーへの参照
  std::vector<std::string> m_registeredTools;  ///< 登録済みツール名

  /**
   * @brief ツール登録を試みる
   *
   * @param tool 登録するツール
   * @return bool 登録成功時はtrue、失敗時はfalse
   */
  bool tryRegisterTool(const Tool& tool);

  // 各ツールの実装ハンドラー

  /**
   * @brief エンジン開始ツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleEngineStart(const std::string& args);

  /**
   * @brief エンジン停止ツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleEngineStop(const std::string& args);

  /**
   * @brief エンジン再起動ツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleEngineRestart(const std::string& args);

  /**
   * @brief エンジンステータス取得ツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleEngineStatus(const std::string& args);

  /**
   * @brief スクリプト実行ツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleExecuteScript(const std::string& args);

  /**
   * @brief スクリプト評価ツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleEvaluateExpression(const std::string& args);

  /**
   * @brief モジュールインポートツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleImportModule(const std::string& args);

  /**
   * @brief デバッグ情報取得ツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleGetDebugInfo(const std::string& args);

  /**
   * @brief ブレークポイント設定ツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleSetBreakpoint(const std::string& args);

  /**
   * @brief メモリ使用状況取得ツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleGetMemoryUsage(const std::string& args);

  /**
   * @brief ガベージコレクション実行ツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleRunGarbageCollection(const std::string& args);

  /**
   * @brief パフォーマンス計測開始ツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleStartProfiling(const std::string& args);

  /**
   * @brief パフォーマンス計測終了ツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleStopProfiling(const std::string& args);

  /**
   * @brief ファイル読み込みツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleReadFile(const std::string& args);

  /**
   * @brief ファイル書き込みツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleWriteFile(const std::string& args);

  /**
   * @brief 環境情報取得ツール
   *
   * @param args 引数（JSON形式）
   * @return std::string 実行結果（JSON形式）
   */
  std::string handleGetEnvironmentInfo(const std::string& args);
};

}  // namespace tools
}  // namespace mcp
}  // namespace aero