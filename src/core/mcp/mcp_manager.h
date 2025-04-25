/**
 * @file mcp_manager.h
 * @brief Model Context Protocol (MCP) マネージャーの定義
 *
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "server/mcp_server.h"
#include "tools/basic_tools.h"

namespace aero {
namespace mcp {

/**
 * @brief MCPマネージャークラス
 *
 * AeroJS JavaScript エンジンのMCP対応を管理するクラスです。
 * サーバーの作成、ツールの登録、クライアント接続の管理などを行います。
 */
class MCPManager {
 public:
  /**
   * @brief シングルトンインスタンスを取得する
   *
   * @return MCPManager& シングルトンインスタンスへの参照
   */
  static MCPManager& getInstance();

  /**
   * @brief MCPサーバーを初期化する
   *
   * @param options サーバー設定オプション
   * @return bool 初期化成功時はtrue、失敗時はfalse
   */
  bool initialize(const MCPServerOptions& options = MCPServerOptions());

  /**
   * @brief MCPサーバーを終了する
   */
  void shutdown();

  /**
   * @brief 指定したポートでサーバーを起動する
   *
   * @param port ポート番号
   * @return bool 起動成功時はtrue、失敗時はfalse
   */
  bool startServer(int port = 8080);

  /**
   * @brief サーバーを停止する
   */
  void stopServer();

  /**
   * @brief サーバーが実行中かどうかを取得する
   *
   * @return bool サーバーが実行中の場合はtrue、そうでない場合はfalse
   */
  bool isServerRunning() const;

  /**
   * @brief サーバーインスタンスを取得する
   *
   * @return MCPServer* サーバーインスタンスへのポインタ
   */
  MCPServer* getServer();

  /**
   * @brief 基本ツールを登録する
   *
   * @return bool 登録成功時はtrue、失敗時はfalse
   */
  bool registerBasicTools();

 private:
  /**
   * @brief コンストラクタ（シングルトンパターン用）
   */
  MCPManager();

  /**
   * @brief デストラクタ
   */
  ~MCPManager();

  // コピーと代入を禁止
  MCPManager(const MCPManager&) = delete;
  MCPManager& operator=(const MCPManager&) = delete;

  std::unique_ptr<MCPServer> m_server;              ///< MCPサーバーインスタンス
  std::unique_ptr<tools::BasicTools> m_basicTools;  ///< 基本ツールインスタンス
  bool m_initialized;                               ///< 初期化済みフラグ
  mutable std::mutex m_mutex;                       ///< 同期用ミューテックス

  static MCPManager* s_instance;      ///< シングルトンインスタンス
  static std::mutex s_instanceMutex;  ///< インスタンス作成用ミューテックス
};

}  // namespace mcp
}  // namespace aero