/**
 * @file mcp_server.cpp
 * @brief Model Context Protocol (MCP) サーバー実装
 * @copyright 2023 AeroJS プロジェクト
 * @license Mozilla Public License 2.0
 */

#include "mcp_server.h"

#include <chrono>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_set>

#include "../../logger/logger.h"
#include "../utils/mcp_utils.h"

namespace aero {
namespace mcp {

// 静的メンバ変数の初期化
std::unordered_map<std::string, std::unique_ptr<ClientInfo>> MCPServer::s_clients;
std::mutex MCPServer::s_clientsMutex;
std::atomic<bool> MCPServer::s_isRunning(false);
std::thread MCPServer::s_serverThread;
WebSocketServer* MCPServer::s_wsServer = nullptr;

MCPServer::MCPServer(const MCPServerOptions& options)
    : m_options(options), m_tools(), m_connectionLimiter(), m_threadPool(options.threadPoolSize) {
  // ロガーの初期化
  initializeLogger(options.logLevel);

  LOG_INFO("MCPサーバーが初期化されました [サーバー名: {}, バージョン: {}]",
           options.serverName, options.serverVersion);
}

MCPServer::~MCPServer() {
  stop();
}

bool MCPServer::start() {
  std::lock_guard<std::mutex> lock(s_clientsMutex);

  if (s_isRunning.load()) {
    LOG_WARN("サーバーはすでに実行中です");
    return false;
  }

  // WebSocketサーバーの設定
  WebSocketServerConfig wsConfig;
  wsConfig.host = m_options.host;
  wsConfig.port = m_options.port;
  wsConfig.maxConnections = m_options.maxConnections;
  wsConfig.connectionTimeoutMs = m_options.connectionTimeoutMs;

  try {
    s_wsServer = new WebSocketServer(wsConfig);

    // WebSocketイベントハンドラーの設定
    s_wsServer->setConnectionHandler([this](WebSocketConnection* connection) {
      handleNewConnection(connection);
    });

    s_wsServer->setDisconnectionHandler([this](WebSocketConnection* connection) {
      handleDisconnection(connection);
    });

    s_wsServer->setMessageHandler([this](WebSocketConnection* connection, const std::string& message) {
      handleMessage(connection, message);
    });

    // サーバースレッドの開始
    s_serverThread = std::thread([this]() {
      s_isRunning.store(true);
      LOG_INFO("MCPサーバーが開始されました [{}:{}]", m_options.host, m_options.port);

      // サーバーループ
      while (s_isRunning.load()) {
        s_wsServer->update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      LOG_INFO("MCPサーバーが停止しました");
    });

    return true;
  } catch (const std::exception& e) {
    LOG_ERROR("サーバー起動エラー: {}", e.what());
    cleanup();
    return false;
  }
}

void MCPServer::stop() {
  std::lock_guard<std::mutex> lock(s_clientsMutex);

  if (!s_isRunning.load()) {
    return;
  }

  // サーバーの停止をマーク
  s_isRunning.store(false);

  // スレッドが終了するのを待つ
  if (s_serverThread.joinable()) {
    s_serverThread.join();
  }

  // 接続中のクライアントを切断
  if (s_wsServer) {
    s_wsServer->disconnectAllClients("サーバーがシャットダウンしました");
  }

  // リソースのクリーンアップ
  cleanup();

  LOG_INFO("MCPサーバーが正常に停止しました");
}

void MCPServer::cleanup() {
  // WebSocketサーバーのクリーンアップ
  if (s_wsServer) {
    delete s_wsServer;
    s_wsServer = nullptr;
  }

  // クライアント情報のクリーンアップ
  s_clients.clear();
}

void MCPServer::registerTool(const Tool& tool) {
  // ツール名が空でないことを確認
  if (tool.name.empty()) {
    LOG_ERROR("ツール登録エラー: ツール名が空です");
    return;
  }

  // 既存のツールと名前が衝突していないか確認
  if (m_tools.find(tool.name) != m_tools.end()) {
    LOG_WARN("ツール '{}' はすでに登録されています。上書きします", tool.name);
  }

  // ツールを登録
  m_tools[tool.name] = tool;
  LOG_INFO("ツール '{}' が正常に登録されました", tool.name);
}

void MCPServer::unregisterTool(const std::string& toolName) {
  auto it = m_tools.find(toolName);
  if (it != m_tools.end()) {
    m_tools.erase(it);
    LOG_INFO("ツール '{}' が登録解除されました", toolName);
  } else {
    LOG_WARN("ツール '{}' は登録されていません", toolName);
  }
}

const std::unordered_map<std::string, Tool>& MCPServer::getRegisteredTools() const {
  return m_tools;
}

void MCPServer::handleNewConnection(WebSocketConnection* connection) {
  // 接続制限のチェック
  if (!m_connectionLimiter.checkAndAddConnection(connection->getRemoteAddress())) {
    connection->close(1008, "接続制限に達しました");
    LOG_WARN("接続が拒否されました: 接続制限に達しました [{}]", connection->getRemoteAddress());
    return;
  }

  // クライアントIDの生成
  std::string clientId = generateClientId();

  // クライアント情報の作成
  auto clientInfo = std::make_unique<ClientInfo>();
  clientInfo->id = clientId;
  clientInfo->connection = connection;
  clientInfo->connectedAt = std::chrono::system_clock::now();
  clientInfo->remoteAddress = connection->getRemoteAddress();
  clientInfo->isAuthenticated = !m_options.requireAuthentication;

  // クライアント情報の登録
  {
    std::lock_guard<std::mutex> lock(s_clientsMutex);
    s_clients[clientId] = std::move(clientInfo);
  }

  LOG_INFO("新しいクライアント接続: [ID: {}, アドレス: {}]", clientId, connection->getRemoteAddress());

  // 認証が必要な場合は認証要求を送信
  if (m_options.requireAuthentication) {
    json authRequest = {
        {"type", "auth_request"},
        {"clientId", clientId}};

    connection->send(authRequest.dump());
    LOG_DEBUG("認証要求がクライアントに送信されました [ID: {}]", clientId);
  } else {
    // 認証が不要な場合は接続確認メッセージを送信
    json connectedMessage = {
        {"type", "connected"},
        {"clientId", clientId},
        {"serverInfo", {{"name", m_options.serverName}, {"version", m_options.serverVersion}}}};

    connection->send(connectedMessage.dump());
    LOG_DEBUG("接続確認メッセージがクライアントに送信されました [ID: {}]", clientId);
  }
}

void MCPServer::handleDisconnection(WebSocketConnection* connection) {
  std::string clientId = findClientIdByConnection(connection);
  if (!clientId.empty()) {
    LOG_INFO("クライアントが切断されました [ID: {}, アドレス: {}]",
             clientId, connection->getRemoteAddress());

    // 接続制限カウンターの更新
    m_connectionLimiter.removeConnection(connection->getRemoteAddress());

    // クライアント情報の削除
    std::lock_guard<std::mutex> lock(s_clientsMutex);
    s_clients.erase(clientId);
  }
}

void MCPServer::handleMessage(WebSocketConnection* connection, const std::string& message) {
  // 非同期処理
  m_threadPool.enqueue([this, connection, message]() {
    std::string clientId = findClientIdByConnection(connection);
    if (clientId.empty()) {
      LOG_WARN("不明な接続からのメッセージを受信しました");
      connection->close(1008, "未認識の接続");
      return;
    }

    // メッセージのJSONパース
    json jsonMessage;
    try {
      jsonMessage = json::parse(message);
    } catch (const json::parse_error& e) {
      LOG_ERROR("JSONパースエラー: {} [クライアントID: {}]", e.what(), clientId);
      sendErrorResponse(connection, "invalid_message", "無効なJSON形式", "");
      return;
    }

    // メッセージタイプの取得
    std::string messageType;
    try {
      messageType = jsonMessage.at("type").get<std::string>();
    } catch (const json::exception& e) {
      LOG_ERROR("メッセージタイプの取得に失敗: {} [クライアントID: {}]", e.what(), clientId);
      sendErrorResponse(connection, "invalid_message", "メッセージタイプが見つかりません", "");
      return;
    }

    // メッセージ情報を記録
    {
      std::lock_guard<std::mutex> lock(s_clientsMutex);
      if (s_clients.find(clientId) != s_clients.end()) {
        MessageInfo msgInfo;
        msgInfo.receivedAt = std::chrono::system_clock::now();
        msgInfo.type = messageType;
        msgInfo.content = message;
        s_clients[clientId]->messages.push_back(msgInfo);

        // メッセージ履歴の制限
        if (s_clients[clientId]->messages.size() > m_options.maxMessageHistory) {
          s_clients[clientId]->messages.pop_front();
        }
      }
    }

    LOG_DEBUG("メッセージを受信: タイプ = {} [クライアントID: {}]", messageType, clientId);

    // メッセージタイプに基づいて処理
    if (messageType == "auth") {
      handleAuthMessage(clientId, connection, jsonMessage);
    } else if (messageType == "call_tool") {
      // 認証済みかチェック
      if (!isClientAuthenticated(clientId)) {
        sendErrorResponse(connection, "auth_required", "認証が必要です", messageType);
        return;
      }

      handleCallToolMessage(clientId, connection, jsonMessage);
    } else if (messageType == "list_tools") {
      // 認証済みかチェック
      if (!isClientAuthenticated(clientId)) {
        sendErrorResponse(connection, "auth_required", "認証が必要です", messageType);
        return;
      }

      handleListToolsMessage(clientId, connection, jsonMessage);
    } else if (messageType == "start_engine") {
      // 認証済みかチェック
      if (!isClientAuthenticated(clientId)) {
        sendErrorResponse(connection, "auth_required", "認証が必要です", messageType);
        return;
      }

      handleStartEngineMessage(clientId, connection, jsonMessage);
    } else if (messageType == "stop_engine") {
      // 認証済みかチェック
      if (!isClientAuthenticated(clientId)) {
        sendErrorResponse(connection, "auth_required", "認証が必要です", messageType);
        return;
      }

      handleStopEngineMessage(clientId, connection, jsonMessage);
    } else if (messageType == "execute_script") {
      // 認証済みかチェック
      if (!isClientAuthenticated(clientId)) {
        sendErrorResponse(connection, "auth_required", "認証が必要です", messageType);
        return;
      }

      handleExecuteScriptMessage(clientId, connection, jsonMessage);
    } else if (messageType == "get_memory_usage") {
      // 認証済みかチェック
      if (!isClientAuthenticated(clientId)) {
        sendErrorResponse(connection, "auth_required", "認証が必要です", messageType);
        return;
      }

      handleGetMemoryUsageMessage(clientId, connection, jsonMessage);
    } else if (messageType == "ping") {
      handlePingMessage(clientId, connection, jsonMessage);
    } else {
      LOG_WARN("不明なメッセージタイプ: {} [クライアントID: {}]", messageType, clientId);
      sendErrorResponse(connection, "unknown_message_type", "不明なメッセージタイプ", messageType);
    }
  });
}

void MCPServer::handleAuthMessage(const std::string& clientId, WebSocketConnection* connection, const json& message) {
  // 認証トークンの取得
  std::string token;
  try {
    token = message.at("token").get<std::string>();
  } catch (const json::exception& e) {
    LOG_ERROR("認証トークンの取得に失敗: {} [クライアントID: {}]", e.what(), clientId);
    sendErrorResponse(connection, "auth_failed", "認証トークンが見つかりません", "auth");
    return;
  }

  // トークンの検証
  bool isValid = validateAuthToken(token, m_options.authSecret);

  if (isValid) {
    // クライアントの認証状態を更新
    {
      std::lock_guard<std::mutex> lock(s_clientsMutex);
      auto it = s_clients.find(clientId);
      if (it != s_clients.end()) {
        it->second->isAuthenticated = true;
        LOG_INFO("クライアントが認証されました [ID: {}]", clientId);
      }
    }

    // 認証成功レスポンスの送信
    json authResponse = {
        {"type", "auth_success"},
        {"clientId", clientId},
        {"serverInfo", {{"name", m_options.serverName}, {"version", m_options.serverVersion}}}};

    connection->send(authResponse.dump());
  } else {
    LOG_WARN("認証に失敗しました [クライアントID: {}]", clientId);
    sendErrorResponse(connection, "auth_failed", "無効な認証トークン", "auth");
  }
}

void MCPServer::handleCallToolMessage(const std::string& clientId, WebSocketConnection* connection, const json& message) {
  // ツール名とパラメータの取得
  std::string toolName;
  json toolParams;

  try {
    toolName = message.at("toolName").get<std::string>();
    toolParams = message.at("params");
  } catch (const json::exception& e) {
    LOG_ERROR("ツール呼び出しパラメータの取得に失敗: {} [クライアントID: {}]", e.what(), clientId);
    sendErrorResponse(connection, "invalid_params", "ツール名またはパラメータが見つかりません", "call_tool");
    return;
  }

  // ツールの検索
  auto toolIt = m_tools.find(toolName);
  if (toolIt == m_tools.end()) {
    LOG_WARN("不明なツールが呼び出されました: {} [クライアントID: {}]", toolName, clientId);
    sendErrorResponse(connection, "unknown_tool", "ツール '" + toolName + "' は登録されていません", "call_tool");
    return;
  }

  const Tool& tool = toolIt->second;

  // パラメータの検証
  SchemaValidator validator;
  if (!validator.validate(toolParams, tool.inputSchema)) {
    LOG_ERROR("ツールパラメータの検証に失敗: {} [クライアントID: {}]", validator.getLastError(), clientId);
    sendErrorResponse(connection, "invalid_params", "パラメータの検証に失敗: " + validator.getLastError(), "call_tool");
    return;
  }

  LOG_DEBUG("ツール呼び出し: {} [クライアントID: {}]", toolName, clientId);

  try {
    // ツール実行
    json result = tool.handler(toolParams);

    // レスポンスの送信
    json response = {
        {"type", "tool_result"},
        {"toolName", toolName},
        {"result", result},
        {"requestId", message.value("requestId", "")}};

    connection->send(response.dump());
    LOG_DEBUG("ツール結果を送信: {} [クライアントID: {}]", toolName, clientId);
  } catch (const std::exception& e) {
    LOG_ERROR("ツール実行エラー: {} [ツール: {}, クライアントID: {}]", e.what(), toolName, clientId);
    sendErrorResponse(connection, "tool_execution_error", "ツール実行エラー: " + std::string(e.what()), "call_tool", message.value("requestId", ""));
  }
}

void MCPServer::handleListToolsMessage(const std::string& clientId, WebSocketConnection* connection, const json& message) {
  LOG_DEBUG("ツールリスト要求 [クライアントID: {}]", clientId);

  // ツールリストの作成
  json toolList = json::array();
  for (const auto& toolEntry : m_tools) {
    const Tool& tool = toolEntry.second;

    json toolInfo = {
        {"name", tool.name},
        {"description", tool.description},
        {"inputSchema", tool.inputSchema}};

    toolList.push_back(toolInfo);
  }

  // レスポンスの送信
  json response = {
      {"type", "tools_list"},
      {"tools", toolList},
      {"requestId", message.value("requestId", "")}};

  connection->send(response.dump());
  LOG_DEBUG("ツールリストを送信しました [クライアントID: {}]", clientId);
}

void MCPServer::handleStartEngineMessage(const std::string& clientId, WebSocketConnection* connection, const json& message) {
  LOG_DEBUG("エンジン起動要求 [クライアントID: {}]", clientId);

  // エンジンオプションの取得
  json engineOptions;
  try {
    engineOptions = message.value("options", json::object());
  } catch (const json::exception& e) {
    LOG_ERROR("エンジンオプションの取得に失敗: {} [クライアントID: {}]", e.what(), clientId);
    engineOptions = json::object();
  }

  try {
    // エンジン起動処理
    // 注: ここに実際のエンジン起動コードを実装
    bool success = true;  // 仮の成功フラグ

    if (success) {
      // 成功レスポンスの送信
      json response = {
          {"type", "engine_started"},
          {"status", "success"},
          {"message", "JavaScriptエンジンが正常に起動しました"},
          {"requestId", message.value("requestId", "")}};

      connection->send(response.dump());
      LOG_INFO("JavaScriptエンジンが起動しました [クライアントID: {}]", clientId);
    } else {
      sendErrorResponse(connection, "engine_start_failed", "エンジンの起動に失敗しました", "start_engine", message.value("requestId", ""));
    }
  } catch (const std::exception& e) {
    LOG_ERROR("エンジン起動エラー: {} [クライアントID: {}]", e.what(), clientId);
    sendErrorResponse(connection, "engine_start_error", "エンジン起動エラー: " + std::string(e.what()), "start_engine", message.value("requestId", ""));
  }
}

void MCPServer::handleStopEngineMessage(const std::string& clientId, WebSocketConnection* connection, const json& message) {
  LOG_DEBUG("エンジン停止要求 [クライアントID: {}]", clientId);

  try {
    // エンジン停止処理
    // 注: ここに実際のエンジン停止コードを実装
    bool success = true;  // 仮の成功フラグ

    if (success) {
      // 成功レスポンスの送信
      json response = {
          {"type", "engine_stopped"},
          {"status", "success"},
          {"message", "JavaScriptエンジンが正常に停止しました"},
          {"requestId", message.value("requestId", "")}};

      connection->send(response.dump());
      LOG_INFO("JavaScriptエンジンが停止しました [クライアントID: {}]", clientId);
    } else {
      sendErrorResponse(connection, "engine_stop_failed", "エンジンの停止に失敗しました", "stop_engine", message.value("requestId", ""));
    }
  } catch (const std::exception& e) {
    LOG_ERROR("エンジン停止エラー: {} [クライアントID: {}]", e.what(), clientId);
    sendErrorResponse(connection, "engine_stop_error", "エンジン停止エラー: " + std::string(e.what()), "stop_engine", message.value("requestId", ""));
  }
}

void MCPServer::handleExecuteScriptMessage(const std::string& clientId, WebSocketConnection* connection, const json& message) {
  // スクリプトコードの取得
  std::string scriptCode;
  bool isModule = false;

  try {
    scriptCode = message.at("code").get<std::string>();
    isModule = message.value("isModule", false);
  } catch (const json::exception& e) {
    LOG_ERROR("スクリプトコードの取得に失敗: {} [クライアントID: {}]", e.what(), clientId);
    sendErrorResponse(connection, "invalid_params", "スクリプトコードが見つかりません", "execute_script", message.value("requestId", ""));
    return;
  }

  LOG_DEBUG("スクリプト実行要求 [クライアントID: {}, モジュール: {}]", clientId, isModule);

  try {
    // スクリプト実行処理
    // 注: ここに実際のスクリプト実行コードを実装
    json result = json::object();  // 仮の結果

    // 成功レスポンスの送信
    json response = {
        {"type", "script_result"},
        {"result", result},
        {"requestId", message.value("requestId", "")}};

    connection->send(response.dump());
    LOG_DEBUG("スクリプト実行結果を送信しました [クライアントID: {}]", clientId);
  } catch (const std::exception& e) {
    LOG_ERROR("スクリプト実行エラー: {} [クライアントID: {}]", e.what(), clientId);
    sendErrorResponse(connection, "script_execution_error", "スクリプト実行エラー: " + std::string(e.what()), "execute_script", message.value("requestId", ""));
  }
}

void MCPServer::handleGetMemoryUsageMessage(const std::string& clientId, WebSocketConnection* connection, const json& message) {
  LOG_DEBUG("メモリ使用状況要求 [クライアントID: {}]", clientId);

  try {
    // メモリ使用状況の取得
    // 注: ここに実際のメモリ使用状況取得コードを実装
    json memoryUsage = {
        {"heapTotal", 0},
        {"heapUsed", 0},
        {"external", 0}};

    // レスポンスの送信
    json response = {
        {"type", "memory_usage"},
        {"usage", memoryUsage},
        {"requestId", message.value("requestId", "")}};

    connection->send(response.dump());
    LOG_DEBUG("メモリ使用状況を送信しました [クライアントID: {}]", clientId);
  } catch (const std::exception& e) {
    LOG_ERROR("メモリ使用状況取得エラー: {} [クライアントID: {}]", e.what(), clientId);
    sendErrorResponse(connection, "memory_usage_error", "メモリ使用状況取得エラー: " + std::string(e.what()), "get_memory_usage", message.value("requestId", ""));
  }
}

void MCPServer::handlePingMessage(const std::string& clientId, WebSocketConnection* connection, const json& message) {
  // タイムスタンプの取得
  int64_t timestamp = message.value("timestamp", 0);

  // レスポンスの送信
  json response = {
      {"type", "pong"},
      {"timestamp", timestamp},
      {"serverTime", std::chrono::system_clock::now().time_since_epoch().count()},
      {"requestId", message.value("requestId", "")}};

  connection->send(response.dump());
  LOG_DEBUG("Pingに応答しました [クライアントID: {}]", clientId);
}

void MCPServer::sendErrorResponse(WebSocketConnection* connection, const std::string& errorCode,
                                  const std::string& errorMessage, const std::string& requestType,
                                  const std::string& requestId) {
  json response = {
      {"type", "error"},
      {"error", {{"code", errorCode}, {"message", errorMessage}, {"requestType", requestType}}}};

  if (!requestId.empty()) {
    response["requestId"] = requestId;
  }

  connection->send(response.dump());
}

bool MCPServer::isClientAuthenticated(const std::string& clientId) const {
  std::lock_guard<std::mutex> lock(s_clientsMutex);

  auto it = s_clients.find(clientId);
  if (it != s_clients.end()) {
    return it->second->isAuthenticated;
  }

  return false;
}

std::string MCPServer::generateClientId() const {
  // ランダムIDの生成
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);

  std::stringstream ss;
  ss << "cli_";

  const char* hex = "0123456789abcdef";
  for (int i = 0; i < 24; ++i) {
    ss << hex[dis(gen)];
  }

  return ss.str();
}

std::string MCPServer::findClientIdByConnection(WebSocketConnection* connection) const {
  std::lock_guard<std::mutex> lock(s_clientsMutex);

  for (const auto& clientEntry : s_clients) {
    if (clientEntry.second->connection == connection) {
      return clientEntry.first;
    }
  }

  return "";
}

}  // namespace mcp
}  // namespace aero
}  // namespace aero