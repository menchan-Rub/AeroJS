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
#include <future>
#include <condition_variable>
#include <shared_mutex>

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
  GENERIC,       ///< 汎用型ツール
  WEB_SEARCH,    ///< ウェブ検索ツール
  CODE_EXECUTION ///< コード実行ツール
};

/**
 * @brief ツールメタデータを表す構造体
 */
struct ToolMetadata {
  std::string name;         ///< ツール名
  std::string description;  ///< ツールの説明
  std::string version;      ///< ツールのバージョン
  ToolType type;            ///< ツールの種類
  std::string inputSchema;  ///< 入力スキーマ（JSON形式）
  std::string outputSchema; ///< 出力スキーマ（JSON形式）
  std::vector<std::string> tags; ///< タグリスト
  json additionalInfo;      ///< 追加情報
  bool isAuthenticated;     ///< 認証が必要かどうか
};

/**
 * @brief ツールを表す構造体
 */
struct Tool : public ToolMetadata {
  std::function<std::string(const std::string&)> handler;   ///< ツールハンドラー関数
  std::function<void(const std::string&, std::function<void(const std::string&, bool)>)> streamHandler; ///< ストリーミングハンドラー
};

/**
 * @brief スレッドプールの設定
 */
struct ThreadPoolConfig {
  size_t minThreads = 4;          ///< 最小スレッド数
  size_t maxThreads = 16;         ///< 最大スレッド数
  size_t idleTimeout = 60000;     ///< アイドルタイムアウト（ミリ秒）
  size_t queueSize = 1000;        ///< キューサイズ
  bool dynamicScaling = true;     ///< 動的スケーリングを有効にする
};

/**
 * @brief MCPサーバーのオプション
 */
struct MCPServerOptions {
  std::string serverName = "AeroJS-MCP-Server";        ///< サーバー名
  std::string version = "1.0.0";                       ///< サーバーバージョン
  std::string host = "0.0.0.0";                        ///< 待ち受けホスト
  int port = 8080;                                     ///< 待ち受けポート
  bool requireAuthentication = false;                  ///< 認証を要求するか
  std::string authSecret = "";                         ///< 認証シークレット
  bool enableLogging = true;                           ///< ロギングを有効にする
  std::string logLevel = "info";                       ///< ログレベル
  size_t maxConcurrentConnections = 1000;              ///< 最大同時接続数
  size_t maxRequestSize = 10 * 1024 * 1024;            ///< 最大リクエストサイズ（バイト）
  size_t maxResponseSize = 10 * 1024 * 1024;           ///< 最大レスポンスサイズ（バイト）
  size_t messageQueueSize = 10000;                     ///< メッセージキューの最大サイズ
  std::chrono::milliseconds pingInterval{30000};       ///< pingの送信間隔（ミリ秒）
  std::chrono::seconds clientTimeout{120};             ///< クライアントのタイムアウト時間（秒）
  bool enableCompression = true;                       ///< 圧縮を有効にする
  std::vector<std::string> allowedOrigins;             ///< 許可されたオリジン（CORS）
  bool enableHTTPS = false;                            ///< HTTPSを有効にする
  std::string certPath = "";                           ///< 証明書ファイルのパス
  std::string keyPath = "";                            ///< 秘密鍵ファイルのパス
  ThreadPoolConfig threadPool;                         ///< スレッドプールの設定
  bool enableProfiling = false;                        ///< プロファイリングを有効にする
};

/**
 * @brief クライアント認証状態を表す列挙型
 */
enum class AuthState {
  NONE,           ///< 未認証
  PENDING,        ///< 認証中
  AUTHENTICATED,  ///< 認証済み
  FAILED          ///< 認証失敗
};

/**
 * @brief クライアント情報を表す構造体
 */
struct ClientInfo {
  std::string id;                                      ///< クライアントID
  WebSocketConnection* connection = nullptr;           ///< WebSocket接続
  std::chrono::system_clock::time_point connectionTime; ///< 接続時刻
  std::chrono::system_clock::time_point lastActivityTime; ///< 最後のアクティビティ時刻
  AuthState authState = AuthState::NONE;               ///< 認証状態
  std::string engineId;                                ///< 関連付けられたエンジンID
  std::atomic<uint64_t> messageCount{0};               ///< 受信メッセージ数
  std::atomic<uint64_t> bytesSent{0};                  ///< 送信バイト数
  std::atomic<uint64_t> bytesReceived{0};              ///< 受信バイト数
  std::string userAgent;                               ///< ユーザーエージェント
  std::unordered_map<std::string, std::string> metadata; ///< メタデータ
  
  /**
   * @brief アクティビティを更新する
   */
  void updateActivity() {
    lastActivityTime = std::chrono::system_clock::now();
  }
  
  /**
   * @brief アイドル時間を取得する（秒）
   * @return アイドル時間（秒）
   */
  int64_t getIdleTimeSec() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - lastActivityTime).count();
  }
};

/**
 * @brief メッセージ情報を表す構造体
 */
struct MessageInfo {
  std::string clientId;                             ///< クライアントID
  std::string message;                              ///< メッセージ内容
  std::chrono::system_clock::time_point timestamp;  ///< タイムスタンプ
  bool requiresResponse = true;                     ///< レスポンスが必要かどうか
};

/**
 * @brief メッセージハンドラーの結果
 */
struct MessageResult {
  bool success = false;                 ///< 成功したかどうか
  std::string response;                 ///< レスポンスメッセージ
  std::string error;                    ///< エラーメッセージ
  std::chrono::microseconds duration;   ///< 処理時間
};

/**
 * @brief タスク優先度を表す列挙型
 */
enum class TaskPriority {
  LOW,      ///< 低優先度
  NORMAL,   ///< 通常優先度
  HIGH,     ///< 高優先度
  CRITICAL  ///< 重要優先度
};

/**
 * @brief MCPサーバー統計情報を表す構造体
 */
struct ServerStats {
  std::atomic<uint64_t> connectionCount{0};     ///< 現在の接続数
  std::atomic<uint64_t> totalConnections{0};    ///< 累計接続数
  std::atomic<uint64_t> messageCount{0};        ///< 処理済みメッセージ数
  std::atomic<uint64_t> errorCount{0};          ///< エラー数
  std::atomic<uint64_t> bytesSent{0};           ///< 送信バイト数
  std::atomic<uint64_t> bytesReceived{0};       ///< 受信バイト数
  std::atomic<uint64_t> authSuccess{0};         ///< 認証成功数
  std::atomic<uint64_t> authFailure{0};         ///< 認証失敗数
  std::atomic<uint64_t> toolCalls{0};           ///< ツール呼び出し数
  std::chrono::system_clock::time_point startTime; ///< 開始時刻
  
  /**
   * @brief 統計情報をJSONとして取得する
   * @return JSON形式の統計情報
   */
  json toJson() const;
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
   * @param options サーバー設定オプション
   */
  explicit MCPServer(const MCPServerOptions& options = MCPServerOptions());

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
   * @brief 指定されたIDのクライアントを取得する
   *
   * @param clientId クライアントID
   * @return std::shared_ptr<ClientInfo> クライアント情報（存在しない場合はnullptr）
   */
  std::shared_ptr<ClientInfo> getClient(const std::string& clientId) const;

  /**
   * @brief すべてのクライアントを取得する
   *
   * @return std::vector<std::shared_ptr<ClientInfo>> クライアント情報のリスト
   */
  std::vector<std::shared_ptr<ClientInfo>> getAllClients() const;

  /**
   * @brief 指定されたクライアントにメッセージを送信する
   *
   * @param clientId クライアントID
   * @param message メッセージ
   * @return bool 送信成功時はtrue、失敗時はfalse
   */
  bool sendMessage(const std::string& clientId, const std::string& message);

  /**
   * @brief 指定されたクライアントにJSON形式のメッセージを送信する
   *
   * @param clientId クライアントID
   * @param jsonMessage JSONメッセージ
   * @return bool 送信成功時はtrue、失敗時はfalse
   */
  bool sendJsonMessage(const std::string& clientId, const json& jsonMessage);

  /**
   * @brief すべてのクライアントにメッセージをブロードキャストする
   *
   * @param message メッセージ
   * @param excludeClientId 除外するクライアントID（省略可）
   */
  void broadcastMessage(const std::string& message, const std::string& excludeClientId = "");

  /**
   * @brief すべてのクライアントにJSON形式のメッセージをブロードキャストする
   *
   * @param jsonMessage JSONメッセージ
   * @param excludeClientId 除外するクライアントID（省略可）
   */
  void broadcastJsonMessage(const json& jsonMessage, const std::string& excludeClientId = "");

  /**
   * @brief クライアントを切断する
   *
   * @param clientId クライアントID
   * @param reason 切断理由
   * @return bool 切断成功時はtrue、失敗時はfalse
   */
  bool disconnectClient(const std::string& clientId, const std::string& reason = "Server closed connection");

  /**
   * @brief ツールを登録する
   *
   * @param tool ツール
   * @return bool 登録成功時はtrue、失敗時はfalse
   */
  bool registerTool(const Tool& tool);

  /**
   * @brief 指定された名前のツールを登録解除する
   *
   * @param toolName ツール名
   * @return bool 登録解除成功時はtrue、失敗時はfalse
   */
  bool unregisterTool(const std::string& toolName);

  /**
   * @brief 指定された名前のツールを取得する
   *
   * @param toolName ツール名
   * @return const Tool* ツール（存在しない場合はnullptr）
   */
  const Tool* getTool(const std::string& toolName) const;

  /**
   * @brief すべてのツールを取得する
   *
   * @return std::vector<Tool> ツールのリスト
   */
  std::vector<Tool> getAllTools() const;

  /**
   * @brief サーバーオプションを取得する
   *
   * @return const MCPServerOptions& サーバーオプション
   */
  const MCPServerOptions& getOptions() const;

  /**
   * @brief サーバー統計情報を取得する
   *
   * @return const ServerStats& サーバー統計情報
   */
  const ServerStats& getStats() const;

  /**
   * @brief サーバー情報を取得する
   *
   * @return json サーバー情報
   */
  json getServerInfo() const;

  /**
   * @brief APIバージョンを取得する
   *
   * @return std::string APIバージョン
   */
  std::string getApiVersion() const;

  /**
   * @brief ツールマネージャーを取得する
   *
   * @return std::shared_ptr<MCPToolManager> ツールマネージャーへの共有ポインタ
   */
  std::shared_ptr<MCPToolManager> getToolManager() const {
    return m_toolManager;
  }

 private:
  /**
   * @brief サーバー初期化
   * @return bool 初期化成功時はtrue、失敗時はfalse
   */
  bool initialize();

  /**
   * @brief クリーンアップ処理
   */
  void cleanup();

  /**
   * @brief WebSocketサーバーを初期化する
   * @return bool 初期化成功時はtrue、失敗時はfalse
   */
  bool initWebSocketServer();

  /**
   * @brief クライアントコネクションを処理する
   * @param connection WebSocket接続
   */
  void handleNewConnection(WebSocketConnection* connection);

  /**
   * @brief クライアント切断を処理する
   * @param connection WebSocket接続
   */
  void handleDisconnection(WebSocketConnection* connection);

  /**
   * @brief クライアントからのメッセージを処理する
   * @param connection WebSocket接続
   * @param message 受信メッセージ
   */
  void handleMessage(WebSocketConnection* connection, const std::string& message);

  /**
   * @brief バイナリメッセージを処理する
   * @param connection WebSocket接続
   * @param data バイナリデータ
   * @param size データサイズ
   */
  void handleBinaryMessage(WebSocketConnection* connection, const void* data, size_t size);

  /**
   * @brief メッセージキュー処理スレッド
   */
  void processMessageQueue();

  /**
   * @brief 監視スレッド
   */
  void monitorThread();

  /**
   * @brief クライアント認証を処理する
   * @param clientId クライアントID
   * @param authToken 認証トークン
   * @return bool 認証成功時はtrue、失敗時はfalse
   */
  bool authenticateClient(const std::string& clientId, const std::string& authToken);

  /**
   * @brief JSON形式のメッセージを解析する
   * @param message メッセージ
   * @param clientId クライアントID
   * @return std::optional<json> 解析結果（失敗時はnullopt）
   */
  std::optional<json> parseJsonMessage(const std::string& message, const std::string& clientId);

  /**
   * @brief リクエストを処理する
   * @param jsonRequest JSONリクエスト
   * @param clientId クライアントID
   * @return json JSONレスポンス
   */
  json processRequest(const json& jsonRequest, const std::string& clientId);

  /**
   * @brief ツールリクエストを処理する
   * @param request リクエスト
   * @param clientId クライアントID
   * @return MessageResult 処理結果
   */
  MessageResult processToolRequest(const json& request, const std::string& clientId);

  /**
   * @brief エラーレスポンスを作成する
   * @param code エラーコード
   * @param message エラーメッセージ
   * @param requestId リクエストID
   * @return json エラーレスポンス
   */
  json createErrorResponse(int code, const std::string& message, const std::string& requestId = "");

  /**
   * @brief 成功レスポンスを作成する
   * @param data レスポンスデータ
   * @param requestId リクエストID
   * @return json 成功レスポンス
   */
  json createSuccessResponse(const json& data, const std::string& requestId = "");

  /**
   * @brief 新しいクライアントIDを生成する
   * @return std::string クライアントID
   */
  std::string generateClientId() const;

  MCPServerOptions m_options;                                      ///< サーバー設定オプション
  std::atomic<bool> m_isRunning;                                   ///< サーバー実行中フラグ
  std::unique_ptr<WebSocketServer> m_webSocketServer;              ///< WebSocketサーバー
  std::shared_ptr<MCPToolManager> m_toolManager;                   ///< ツールマネージャー
  
  mutable std::shared_mutex m_clientsMutex;                        ///< クライアント情報アクセス用ミューテックス
  std::unordered_map<std::string, std::shared_ptr<ClientInfo>> m_clients; ///< クライアント情報マップ
  std::unordered_map<WebSocketConnection*, std::string> m_connectionToClientId; ///< 接続からクライアントIDへのマップ

  mutable std::shared_mutex m_toolsMutex;                          ///< ツールアクセス用ミューテックス
  std::unordered_map<std::string, Tool> m_tools;                   ///< ツールマップ

  std::mutex m_queueMutex;                                         ///< メッセージキュー用ミューテックス
  std::condition_variable m_queueCondition;                        ///< メッセージキュー条件変数
  std::queue<MessageInfo> m_messageQueue;                          ///< メッセージキュー
  std::vector<std::thread> m_workerThreads;                        ///< ワーカースレッド
  std::unique_ptr<std::thread> m_monitorThread;                    ///< 監視スレッド

  ServerStats m_stats;                                             ///< サーバー統計情報
  std::atomic<uint64_t> m_clientIdCounter;                         ///< クライアントIDカウンター
};

}  // namespace mcp
}  // namespace aero

#endif  // AEROJS_MCP_SERVER_H