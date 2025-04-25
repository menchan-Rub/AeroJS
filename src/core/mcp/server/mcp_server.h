/**
 * @file mcp_server.h
 * @brief Model Context Protocol (MCP) サーバーの実装
 *
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../../network/websocket_server.h"
#include "../tool/mcp_tool_manager.h"

namespace aero {
namespace mcp {

using json = nlohmann::json;
class WebSocketConnection;
class WebSocketServer;

/**
 * @brief ツールの種類を表す列挙型
 */
enum class ToolType {
  FUNCTION,      ///< 関数型ツール
  STREAM,        ///< ストリーム型ツール
  FILE_HANDLER,  ///< ファイルハンドラー型ツール
  GENERIC        ///< 汎用型ツール
};

/**
 * @brief ツールを表す構造体
 */
struct Tool {
  std::string name;                                        ///< ツール名
  std::string description;                                 ///< ツールの説明
  ToolType type;                                           ///< ツールの種類
  std::string inputSchema;                                 ///< 入力スキーマ（JSON形式）
  std::string outputSchema;                                ///< 出力スキーマ（JSON形式）
  std::function<std::string(const std::string&)> handler;  ///< ツールハンドラー関数
};

/**
 * @brief MCPサーバーのオプション
 */
struct MCPServerOptions {
  std::string serverName = "AeroJS-MCP-Server";   ///< サーバー名
  std::string version = "1.0.0";                  ///< サーバーバージョン
  bool requireAuthentication = false;             ///< 認証を要求するか
  std::string authSecret = "";                    ///< 認証シークレット
  bool enableLogging = true;                      ///< ロギングを有効にするか
  size_t maxConcurrentConnections = 10;           ///< 最大同時接続数
  size_t maxRequestSize = 1024 * 1024;            ///< 最大リクエストサイズ（バイト）
  size_t maxResponseSize = 1024 * 1024;           ///< 最大レスポンスサイズ（バイト）
  std::string logLevel = "info";                  ///< ログレベル
  size_t messageQueueSize = 100;                  ///< メッセージキューの最大サイズ
  std::chrono::milliseconds pingInterval{30000};  ///< pingの送信間隔（ミリ秒）
  std::chrono::seconds clientTimeout{120};        ///< クライアントのタイムアウト時間（秒）
};

/**
 * @brief クライアント情報を表す構造体
 */
struct ClientInfo {
  WebSocketConnection* connection = nullptr;               ///< WebSocket接続
  std::chrono::system_clock::time_point connectionTime;    ///< 接続時刻
  std::chrono::system_clock::time_point lastActivityTime;  ///< 最後のアクティビティ時刻
  bool isAuthenticated = false;                            ///< 認証済みフラグ
  std::string engineId;                                    ///< 関連付けられたエンジンID
  std::atomic<uint64_t> messageCount{0};                   ///< 受信メッセージ数
};

/**
 * @brief メッセージ情報を表す構造体
 */
struct MessageInfo {
  std::string clientId;                             ///< クライアントID
  std::string message;                              ///< メッセージ内容
  std::chrono::system_clock::time_point timestamp;  ///< タイムスタンプ
};

/**
 * @brief MCPサーバークラス
 *
 * Model Context Protocol (MCP) に準拠したサーバーの実装を提供するクラス。
 * WebSocketを使用してクライアントとの通信を行い、JavaScript実行エンジンとツールの管理を行います。
 */
class MCPServer {
 public:
  /**
   * @brief コンストラクタ
   *
   * @param host 待ち受けホスト
   * @param port 待ち受けポート番号
   * @param options サーバー設定オプション
   */
  explicit MCPServer(const std::string& host, int port, const MCPServerOptions& options = MCPServerOptions());

  /**
   * @brief デストラクタ
   */
  ~MCPServer();

  /**
   * @brief サーバーを起動する
   *
   * @return bool 起動成功時はtrue、失敗時はfalse
   */
  bool start();

  /**
   * @brief サーバーを停止する
   */
  void stop();

  /**
   * @brief サーバーが動作中かどうかを確認する
   *
   * @return bool 動作中の場合はtrue、それ以外はfalse
   */
  bool isRunning() const;

  /**
   * @brief ツールマネージャーを取得する
   *
   * @return std::shared_ptr<MCPToolManager> ツールマネージャーへの共有ポインタ
   */
  std::shared_ptr<MCPToolManager> getToolManager() const {
    return m_toolManager;
  }

  /**
   * @brief サーバー情報を取得する
   *
   * @return json サーバー情報
   */
  json getServerInfo() const;

  /**
   * @brief メトリクス情報を取得する
   *
   * @return json メトリクス情報
   */
  json getMetrics() const;

 private:
  std::string m_host;             ///< 待ち受けホスト
  int m_port;                     ///< 待ち受けポート
  MCPServerOptions m_options;     ///< サーバー設定オプション
  std::atomic<bool> m_isRunning;  ///< サーバー実行中フラグ

  std::unique_ptr<WebSocketServer> m_webSocketServer;  ///< WebSocketサーバー
  std::unique_ptr<std::thread> m_serverThread;         ///< サーバースレッド

  std::shared_ptr<MCPToolManager> m_toolManager;  ///< ツールマネージャー

  mutable std::mutex m_clientsMutex;                      ///< クライアント情報アクセス用ミューテックス
  std::unordered_map<std::string, ClientInfo> m_clients;  ///< クライアント情報

  mutable std::mutex m_messageQueueMutex;  ///< メッセージキューアクセス用ミューテックス
  std::queue<MessageInfo> m_messageQueue;  ///< メッセージキュー

  std::unordered_map<std::string,
                     std::function<void(const std::string&, WebSocketConnection*, const json&)>>
      m_messageHandlers;  ///< メッセージハンドラー

  // 統計情報
  struct {
    std::atomic<uint64_t> totalRequests{0};       ///< 総リクエスト数
    std::atomic<uint64_t> successfulRequests{0};  ///< 成功したリクエスト数
    std::atomic<uint64_t> failedRequests{0};      ///< 失敗したリクエスト数
    std::atomic<uint64_t> totalMessages{0};       ///< 総メッセージ数
    std::atomic<uint64_t> authAttempts{0};        ///< 認証試行回数
    std::atomic<uint64_t> successfulAuths{0};     ///< 成功した認証数
    std::atomic<uint64_t> failedAuths{0};         ///< 失敗した認証数
  } m_stats;

  /**
   * @brief 新しい接続の処理
   *
   * @param connection WebSocket接続
   */
  void handleNewConnection(WebSocketConnection* connection);

  /**
   * @brief 切断の処理
   *
   * @param connection WebSocket接続
   */
  void handleDisconnection(WebSocketConnection* connection);

  /**
   * @brief メッセージの処理
   *
   * @param connection WebSocket接続
   * @param message メッセージ
   */
  void handleMessage(WebSocketConnection* connection, const std::string& message);

  /**
   * @brief エラーの処理
   *
   * @param connection WebSocket接続
   * @param error エラーメッセージ
   */
  void handleError(WebSocketConnection* connection, const std::string& error);

  /**
   * @brief メッセージキューの処理
   */
  void processMessageQueue();

  /**
   * @brief メッセージの処理
   *
   * @param messageInfo メッセージ情報
   */
  void processMessage(const MessageInfo& messageInfo);

  /**
   * @brief メッセージハンドラーの初期化
   */
  void initializeMessageHandlers();

  /**
   * @brief 認証メッセージの処理
   *
   * @param clientId クライアントID
   * @param connection WebSocket接続
   * @param message メッセージ
   */
  void handleAuthenticateMessage(const std::string& clientId, WebSocketConnection* connection, const json& message);

  /**
   * @brief ツール呼び出しメッセージの処理
   *
   * @param clientId クライアントID
   * @param connection WebSocket接続
   * @param message メッセージ
   */
  void handleCallToolMessage(const std::string& clientId, WebSocketConnection* connection, const json& message);

  /**
   * @brief ツールリスト取得メッセージの処理
   *
   * @param clientId クライアントID
   * @param connection WebSocket接続
   * @param message メッセージ
   */
  void handleListToolsMessage(const std::string& clientId, WebSocketConnection* connection, const json& message);

  /**
   * @brief エンジン起動メッセージの処理
   *
   * @param clientId クライアントID
   * @param connection WebSocket接続
   * @param message メッセージ
   */
  void handleStartEngineMessage(const std::string& clientId, WebSocketConnection* connection, const json& message);

  /**
   * @brief エンジン停止メッセージの処理
   *
   * @param clientId クライアントID
   * @param connection WebSocket接続
   * @param message メッセージ
   */
  void handleStopEngineMessage(const std::string& clientId, WebSocketConnection* connection, const json& message);

  /**
   * @brief スクリプト実行メッセージの処理
   *
   * @param clientId クライアントID
   * @param connection WebSocket接続
   * @param message メッセージ
   */
  void handleExecuteScriptMessage(const std::string& clientId, WebSocketConnection* connection, const json& message);

  /**
   * @brief メモリ使用状況取得メッセージの処理
   *
   * @param clientId クライアントID
   * @param connection WebSocket接続
   * @param message メッセージ
   */
  void handleGetMemoryUsageMessage(const std::string& clientId, WebSocketConnection* connection, const json& message);

  /**
   * @brief pingメッセージの処理
   *
   * @param clientId クライアントID
   * @param connection WebSocket接続
   * @param message メッセージ
   */
  void handlePingMessage(const std::string& clientId, WebSocketConnection* connection, const json& message);

  /**
   * @brief クライアントIDの生成
   *
   * @return std::string 生成されたクライアントID
   */
  std::string generateClientId() const;

  /**
   * @brief 接続からクライアントIDを検索
   *
   * @param connection WebSocket接続
   * @return std::string クライアントID（見つからない場合は空文字列）
   */
  std::string findClientIdByConnection(WebSocketConnection* connection) const;
};

}  // namespace mcp
}  // namespace aero

#endif  // AEROJS_MCP_SERVER_H