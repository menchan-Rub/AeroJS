/**
 * @file mcp_server.cpp
 * @brief Model Context Protocol (MCP) サーバーの実装
 *
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#include "mcp_server.h"

#include <chrono>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>

#include "utils/mcp_utils.h"

namespace aero {
namespace mcp {

using json = nlohmann::json;

// MCPサーバークラスのコンストラクタ
MCPServer::MCPServer(const MCPServerOptions& options)
    : m_options(options),
      m_isRunning(false),
      m_nextId(1),
      m_authenticatedClients(),
      m_requestHandlers(),
      m_messageQueue(),
      m_queueMutex(),
      m_queueCondition(),
      m_processingThread() {
  // リクエストハンドラの登録
  registerRequestHandlers();
}

// MCPサーバークラスのデストラクタ
MCPServer::~MCPServer() {
  stop();
}

// サーバーの起動
bool MCPServer::start(int port) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_isRunning) {
    std::cerr << "MCPサーバーはすでに実行中です" << std::endl;
    return false;
  }

  m_port = port;

  // WebSocketサーバーの初期化
  if (!initializeWebSocketServer()) {
    std::cerr << "WebSocketサーバーの初期化に失敗しました" << std::endl;
    return false;
  }

  // メッセージ処理スレッドの開始
  m_isRunning = true;
  m_processingThread = std::thread(&MCPServer::processMessageQueue, this);

  std::cout << "MCPサーバーが起動しました: " << m_options.serverName
            << " (v" << m_options.version << ") on port " << port << std::endl;

  return true;
}

// サーバーの停止
void MCPServer::stop() {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!m_isRunning) {
    return;
  }

  // WebSocketサーバーの停止
  stopWebSocketServer();

  // メッセージ処理スレッドの停止
  m_isRunning = false;
  m_queueCondition.notify_all();

  if (m_processingThread.joinable()) {
    m_processingThread.join();
  }

  std::cout << "MCPサーバーが停止しました: " << m_options.serverName << std::endl;
}

// WebSocketサーバーの初期化
bool MCPServer::initializeWebSocketServer() {
  try {
    m_server.init_asio();

    // WebSocketイベントハンドラの設定
    m_server.set_open_handler([this](websocketpp::connection_hdl hdl) {
      handleOpen(hdl);
    });

    m_server.set_close_handler([this](websocketpp::connection_hdl hdl) {
      handleClose(hdl);
    });

    m_server.set_message_handler([this](websocketpp::connection_hdl hdl,
                                        WebSocketServer::message_ptr msg) {
      handleMessage(hdl, msg);
    });

    // サーバーをリッスン状態に設定
    m_server.listen(m_port);

    // サーバーの起動
    m_server.start_accept();

    // 非同期にサーバーを実行
    m_wsThread = std::thread([this]() {
      try {
        m_server.run();
      } catch (const std::exception& e) {
        std::cerr << "WebSocketサーバー実行中にエラーが発生しました: " << e.what() << std::endl;
      }
    });

    return true;
  } catch (const std::exception& e) {
    std::cerr << "WebSocketサーバーの初期化中にエラーが発生しました: " << e.what() << std::endl;
    return false;
  }
}

// WebSocketサーバーの停止
void MCPServer::stopWebSocketServer() {
  if (m_wsThread.joinable()) {
    try {
      m_server.stop();
      m_wsThread.join();
    } catch (const std::exception& e) {
      std::cerr << "WebSocketサーバーの停止中にエラーが発生しました: " << e.what() << std::endl;
    }
  }
}

// クライアント接続時の処理
void MCPServer::handleOpen(websocketpp::connection_hdl hdl) {
  std::lock_guard<std::mutex> lock(m_clientsMutex);

  ConnectionInfo info;
  info.hdl = hdl;
  info.isAuthenticated = false;
  info.connectionTime = std::chrono::system_clock::now();

  auto conn = m_server.get_con_from_hdl(hdl);
  info.remoteEndpoint = conn->get_remote_endpoint();

  m_clients[hdl] = info;

  std::cout << "クライアント接続: " << info.remoteEndpoint << std::endl;

  // 認証が必要ない場合は認証済みとしてマーク
  if (!m_options.enableAuthentication) {
    info.isAuthenticated = true;
    m_clients[hdl] = info;
  }

  // 初期化メッセージを送信
  sendInitialization(hdl);
}

// クライアント切断時の処理
void MCPServer::handleClose(websocketpp::connection_hdl hdl) {
  std::lock_guard<std::mutex> lock(m_clientsMutex);

  auto it = m_clients.find(hdl);
  if (it != m_clients.end()) {
    std::cout << "クライアント切断: " << it->second.remoteEndpoint << std::endl;

    // クライアントが認証済みであれば、認証済みリストからも削除
    if (it->second.isAuthenticated) {
      auto authIt = m_authenticatedClients.find(it->second.clientId);
      if (authIt != m_authenticatedClients.end()) {
        m_authenticatedClients.erase(authIt);
      }
    }

    m_clients.erase(it);
  }
}

// メッセージ受信時の処理
void MCPServer::handleMessage(websocketpp::connection_hdl hdl, WebSocketServer::message_ptr msg) {
  // クライアント情報を確認
  std::unique_lock<std::mutex> lock(m_clientsMutex);

  auto it = m_clients.find(hdl);
  if (it == m_clients.end()) {
    sendErrorResponse(hdl, "UNKNOWN_CLIENT", "クライアントが見つかりません", "");
    return;
  }

  // メッセージをキューに追加
  std::string clientId = it->second.clientId;
  bool isAuthenticated = it->second.isAuthenticated;
  lock.unlock();

  // メッセージを処理キューに追加
  QueuedMessage queuedMsg;
  queuedMsg.hdl = hdl;
  queuedMsg.clientId = clientId;
  queuedMsg.isAuthenticated = isAuthenticated;
  queuedMsg.message = msg->get_payload();

  {
    std::lock_guard<std::mutex> queueLock(m_queueMutex);
    m_messageQueue.push(queuedMsg);
  }

  m_queueCondition.notify_one();
}

// メッセージキューの処理
void MCPServer::processMessageQueue() {
  while (m_isRunning) {
    // キューからメッセージを取得
    QueuedMessage msg;

    {
      std::unique_lock<std::mutex> lock(m_queueMutex);
      m_queueCondition.wait(lock, [this] {
        return !m_isRunning || !m_messageQueue.empty();
      });

      if (!m_isRunning && m_messageQueue.empty()) {
        break;
      }

      if (!m_messageQueue.empty()) {
        msg = m_messageQueue.front();
        m_messageQueue.pop();
      }
    }

    // メッセージを処理
    if (!msg.message.empty()) {
      processMessage(msg.hdl, msg.clientId, msg.isAuthenticated, msg.message);
    }
  }
}

// メッセージ処理
void MCPServer::processMessage(websocketpp::connection_hdl hdl,
                               const std::string& clientId,
                               bool isAuthenticated,
                               const std::string& message) {
  try {
    // JSONメッセージをパース
    json jsonMsg = json::parse(message);

    // メッセージIDの取得
    std::string msgId = "";
    if (jsonMsg.contains("id") && jsonMsg["id"].is_string()) {
      msgId = jsonMsg["id"].get<std::string>();
    }

    // メッセージタイプのバリデーション
    if (!jsonMsg.contains("type") || !jsonMsg["type"].is_string()) {
      sendErrorResponse(hdl, "INVALID_REQUEST", "無効なリクエストフォーマット: typeフィールドがありません", msgId);
      return;
    }

    std::string type = jsonMsg["type"].get<std::string>();

    // 認証が必要なメッセージの場合
    if (m_options.enableAuthentication && !isAuthenticated && type != "authenticate") {
      sendErrorResponse(hdl, "UNAUTHORIZED", "認証が必要です", msgId);
      return;
    }

    // パラメータのバリデーション
    if (!jsonMsg.contains("params") || !jsonMsg["params"].is_object()) {
      sendErrorResponse(hdl, "INVALID_REQUEST", "無効なリクエストフォーマット: paramsフィールドがありません", msgId);
      return;
    }

    // リクエストハンドラを呼び出し
    auto handlerIt = m_requestHandlers.find(type);
    if (handlerIt == m_requestHandlers.end()) {
      sendErrorResponse(hdl, "UNKNOWN_REQUEST", "不明なリクエストタイプ: " + type, msgId);
      return;
    }

    // リクエストを処理
    json params = jsonMsg["params"];
    json response = handlerIt->second(clientId, params);

    // レスポンスを送信
    sendResponse(hdl, msgId, response);

  } catch (json::parse_error& e) {
    sendErrorResponse(hdl, "PARSE_ERROR", "JSONパースエラー: " + std::string(e.what()), "");
  } catch (const std::exception& e) {
    sendErrorResponse(hdl, "INTERNAL_ERROR", "内部エラー: " + std::string(e.what()), "");
  }
}

// 初期化メッセージの送信
void MCPServer::sendInitialization(websocketpp::connection_hdl hdl) {
  json message = {
      {"type", "initialization"},
      {"id", generateId()},
      {"data", {{"serverName", m_options.serverName}, {"version", m_options.version}, {"requireAuth", m_options.enableAuthentication}, {"capabilities", getCapabilities()}}}};

  sendMessage(hdl, message.dump());
}

// サーバー機能の取得
json MCPServer::getCapabilities() const {
  return {
      {"tools", getAvailableTools()},
      {"features", {{"scriptExecution", true}, {"memoryTracking", true}, {"debugging", true}, {"asyncOperations", true}}}};
}

// 利用可能なツールの取得
json MCPServer::getAvailableTools() const {
  return m_tools->getToolDefinitions();
}

// リクエストハンドラの登録
void MCPServer::registerRequestHandlers() {
  // 認証ハンドラ
  m_requestHandlers["authenticate"] = [this](const std::string& clientId, const json& params) -> json {
    return handleAuthenticate(clientId, params);
  };

  // 一般リクエストハンドラ
  m_requestHandlers["engine.start"] = [this](const std::string& clientId, const json& params) -> json {
    return handleEngineStart(clientId, params);
  };

  m_requestHandlers["engine.stop"] = [this](const std::string& clientId, const json& params) -> json {
    return handleEngineStop(clientId, params);
  };

  m_requestHandlers["script.execute"] = [this](const std::string& clientId, const json& params) -> json {
    return handleScriptExecute(clientId, params);
  };

  m_requestHandlers["memory.getUsage"] = [this](const std::string& clientId, const json& params) -> json {
    return handleMemoryGetUsage(clientId, params);
  };

  m_requestHandlers["tools.list"] = [this](const std::string& clientId, const json& params) -> json {
    return handleToolsList(clientId, params);
  };

  m_requestHandlers["tools.call"] = [this](const std::string& clientId, const json& params) -> json {
    return handleToolsCall(clientId, params);
  };
}

// 認証リクエストの処理
json MCPServer::handleAuthenticate(const std::string& clientId, const json& params) {
  // 認証トークンの検証
  if (!params.contains("token") || !params["token"].is_string()) {
    return createErrorResponse("INVALID_AUTH", "認証トークンが無効です");
  }

  std::string token = params["token"].get<std::string>();

  // トークンの検証（実際の実装ではより安全な方法を使用）
  if (!validateAuthToken(token)) {
    return createErrorResponse("INVALID_AUTH", "認証トークンが無効です");
  }

  // クライアントを認証済みとしてマーク
  std::lock_guard<std::mutex> lock(m_clientsMutex);

  for (auto& client : m_clients) {
    if (client.second.clientId == clientId) {
      client.second.isAuthenticated = true;
      m_authenticatedClients[clientId] = client.second;

      return createSuccessResponse({{"authenticated", true},
                                    {"clientId", clientId}});
    }
  }

  return createErrorResponse("UNKNOWN_CLIENT", "クライアントが見つかりません");
}

// エンジン起動リクエストの処理
json MCPServer::handleEngineStart(const std::string& clientId, const json& params) {
  try {
    // オプションの取得
    json options = params.contains("options") ? params["options"] : json::object();

    // エンジンIDの生成
    std::string engineId = generateId();

    // エンジンの作成と初期化
    if (m_tools->startEngine(engineId, options)) {
      return createSuccessResponse({{"engineId", engineId},
                                    {"status", "running"}});
    } else {
      return createErrorResponse("ENGINE_START_FAILED", "エンジンの起動に失敗しました");
    }
  } catch (const std::exception& e) {
    return createErrorResponse("INTERNAL_ERROR", std::string("内部エラー: ") + e.what());
  }
}

// エンジン停止リクエストの処理
json MCPServer::handleEngineStop(const std::string& clientId, const json& params) {
  try {
    // エンジンIDのバリデーション
    if (!params.contains("engineId") || !params["engineId"].is_string()) {
      return createErrorResponse("INVALID_REQUEST", "engineIdは必須です");
    }

    std::string engineId = params["engineId"].get<std::string>();

    // エンジンの停止
    if (m_tools->stopEngine(engineId)) {
      return createSuccessResponse({{"engineId", engineId},
                                    {"status", "stopped"}});
    } else {
      return createErrorResponse("ENGINE_STOP_FAILED", "エンジンの停止に失敗しました");
    }
  } catch (const std::exception& e) {
    return createErrorResponse("INTERNAL_ERROR", std::string("内部エラー: ") + e.what());
  }
}

// スクリプト実行リクエストの処理
json MCPServer::handleScriptExecute(const std::string& clientId, const json& params) {
  try {
    // パラメータのバリデーション
    if (!params.contains("engineId") || !params["engineId"].is_string()) {
      return createErrorResponse("INVALID_REQUEST", "engineIdは必須です");
    }

    if (!params.contains("script") || !params["script"].is_string()) {
      return createErrorResponse("INVALID_REQUEST", "scriptは必須です");
    }

    std::string engineId = params["engineId"].get<std::string>();
    std::string script = params["script"].get<std::string>();
    std::string filename = params.contains("filename") && params["filename"].is_string()
                               ? params["filename"].get<std::string>()
                               : "script.js";

    json options = params.contains("options") ? params["options"] : json::object();

    // スクリプトの実行
    auto result = m_tools->executeScript(engineId, script, filename, options);

    if (result.contains("success") && result["success"].get<bool>()) {
      return createSuccessResponse({{"result", result["result"]},
                                    {"executionTime", result["executionTime"]}});
    } else {
      return createErrorResponse(
          result.contains("errorName") ? result["errorName"].get<std::string>() : "EXECUTION_ERROR",
          result.contains("errorMessage") ? result["errorMessage"].get<std::string>() : "スクリプト実行に失敗しました",
          {{"error", {{"name", result.contains("errorName") ? result["errorName"] : "Error"}, {"message", result.contains("errorMessage") ? result["errorMessage"] : "Unknown error"}, {"stack", result.contains("stack") ? result["stack"] : ""}}}});
    }
  } catch (const std::exception& e) {
    return createErrorResponse("INTERNAL_ERROR", std::string("内部エラー: ") + e.what());
  }
}

// メモリ使用状況取得リクエストの処理
json MCPServer::handleMemoryGetUsage(const std::string& clientId, const json& params) {
  try {
    // エンジンIDのバリデーション
    if (!params.contains("engineId") || !params["engineId"].is_string()) {
      return createErrorResponse("INVALID_REQUEST", "engineIdは必須です");
    }

    std::string engineId = params["engineId"].get<std::string>();
    bool detailed = params.contains("detailed") && params["detailed"].is_boolean()
                        ? params["detailed"].get<bool>()
                        : false;

    // メモリ使用状況の取得
    auto memoryInfo = m_tools->getMemoryUsage(engineId, detailed);

    if (memoryInfo.contains("success") && memoryInfo["success"].get<bool>()) {
      return createSuccessResponse({{"memory", memoryInfo["memory"]}});
    } else {
      return createErrorResponse(
          "MEMORY_INFO_FAILED",
          memoryInfo.contains("message") ? memoryInfo["message"].get<std::string>() : "メモリ情報の取得に失敗しました");
    }
  } catch (const std::exception& e) {
    return createErrorResponse("INTERNAL_ERROR", std::string("内部エラー: ") + e.what());
  }
}

// ツール一覧取得リクエストの処理
json MCPServer::handleToolsList(const std::string& clientId, const json& params) {
  try {
    return createSuccessResponse({{"tools", getAvailableTools()}});
  } catch (const std::exception& e) {
    return createErrorResponse("INTERNAL_ERROR", std::string("内部エラー: ") + e.what());
  }
}

// ツール呼び出しリクエストの処理
json MCPServer::handleToolsCall(const std::string& clientId, const json& params) {
  try {
    // パラメータのバリデーション
    if (!params.contains("name") || !params["name"].is_string()) {
      return createErrorResponse("INVALID_REQUEST", "nameは必須です");
    }

    if (!params.contains("arguments") || !params["arguments"].is_object()) {
      return createErrorResponse("INVALID_REQUEST", "argumentsは必須です");
    }

    std::string name = params["name"].get<std::string>();
    json arguments = params["arguments"];
    std::string engineId = "";

    if (params.contains("engineId") && params["engineId"].is_string()) {
      engineId = params["engineId"].get<std::string>();
    }

    // ツールの呼び出し
    auto result = m_tools->callTool(name, arguments, engineId);

    if (result.contains("success") && result["success"].get<bool>()) {
      return createSuccessResponse({{"result", result["result"]}});
    } else {
      return createErrorResponse(
          result.contains("errorName") ? result["errorName"].get<std::string>() : "TOOL_CALL_FAILED",
          result.contains("errorMessage") ? result["errorMessage"].get<std::string>() : "ツール呼び出しに失敗しました");
    }
  } catch (const std::exception& e) {
    return createErrorResponse("INTERNAL_ERROR", std::string("内部エラー: ") + e.what());
  }
}

// ツールマネージャの設定
void MCPServer::setToolManager(std::shared_ptr<MCPToolManager> tools) {
  m_tools = tools;
}

// レスポンス送信
void MCPServer::sendResponse(websocketpp::connection_hdl hdl, const std::string& id, const json& data) {
  json response = {
      {"type", "response"},
      {"id", id},
      {"data", data}};

  sendMessage(hdl, response.dump());
}

// エラーレスポンス送信
void MCPServer::sendErrorResponse(websocketpp::connection_hdl hdl,
                                  const std::string& errorCode,
                                  const std::string& errorMessage,
                                  const std::string& id) {
  json error = {
      {"type", "error"},
      {"id", id.empty() ? generateId() : id},
      {"error", {{"code", errorCode}, {"message", errorMessage}}}};

  sendMessage(hdl, error.dump());
}

// WebSocketメッセージ送信
void MCPServer::sendMessage(websocketpp::connection_hdl hdl, const std::string& message) {
  try {
    m_server.send(hdl, message, websocketpp::frame::opcode::text);
  } catch (const std::exception& e) {
    std::cerr << "メッセージ送信中にエラーが発生しました: " << e.what() << std::endl;
  }
}

// 成功レスポンスの作成
json MCPServer::createSuccessResponse(const json& data) {
  json response = {
      {"success", true}};

  // データフィールドをマージ
  for (auto& item : data.items()) {
    response[item.key()] = item.value();
  }

  return response;
}

// エラーレスポンスの作成
json MCPServer::createErrorResponse(const std::string& errorCode,
                                    const std::string& errorMessage,
                                    const json& additionalData) {
  json response = {
      {"success", false},
      {"errorName", errorCode},
      {"errorMessage", errorMessage}};

  // 追加データをマージ
  for (auto& item : additionalData.items()) {
    response[item.key()] = item.value();
  }

  return response;
}

// 一意のIDの生成
std::string MCPServer::generateId() {
  return utils::generateUUID();
}

// 認証トークンの検証
bool MCPServer::validateAuthToken(const std::string& token) {
  // 実際の実装ではトークンの正当性を検証するロジックを実装
  // このサンプルでは簡単な検証を行う
  return token.length() > 10 && token.find("auth_") == 0;
}

// リクエスト処理メソッド (外部から利用可能)
std::string MCPServer::handleRequest(const std::string& requestJson) {
  try {
    // JSONリクエストのパース
    json request = json::parse(requestJson);

    // リクエストの検証
    if (!request.contains("type") || !request["type"].is_string()) {
      json errorResponse = {
          {"type", "error"},
          {"id", request.contains("id") ? request["id"] : generateId()},
          {"error", {{"code", "INVALID_REQUEST"}, {"message", "無効なリクエスト: typeフィールドがありません"}}}};
      return errorResponse.dump();
    }

    std::string type = request["type"].get<std::string>();
    std::string id = request.contains("id") ? request["id"].get<std::string>() : generateId();

    // パラメータの検証
    if (!request.contains("params") || !request["params"].is_object()) {
      json errorResponse = {
          {"type", "error"},
          {"id", id},
          {"error", {{"code", "INVALID_REQUEST"}, {"message", "無効なリクエスト: paramsフィールドがありません"}}}};
      return errorResponse.dump();
    }

    json params = request["params"];

    // ハンドラの呼び出し
    auto handlerIt = m_requestHandlers.find(type);
    if (handlerIt == m_requestHandlers.end()) {
      json errorResponse = {
          {"type", "error"},
          {"id", id},
          {"error", {{"code", "UNKNOWN_REQUEST"}, {"message", "不明なリクエストタイプ: " + type}}}};
      return errorResponse.dump();
    }

    // リクエストの処理
    json responseData = handlerIt->second("direct_api", params);

    // レスポンスの作成
    json response = {
        {"type", "response"},
        {"id", id},
        {"data", responseData}};

    return response.dump();

  } catch (json::parse_error& e) {
    json errorResponse = {
        {"type", "error"},
        {"id", generateId()},
        {"error", {{"code", "PARSE_ERROR"}, {"message", std::string("JSONパースエラー: ") + e.what()}}}};
    return errorResponse.dump();
  } catch (const std::exception& e) {
    json errorResponse = {
        {"type", "error"},
        {"id", generateId()},
        {"error", {{"code", "INTERNAL_ERROR"}, {"message", std::string("内部エラー: ") + e.what()}}}};
    return errorResponse.dump();
  }
}

// ブロードキャストメッセージ送信
void MCPServer::broadcastMessage(const std::string& type, const json& data) {
  json message = {
      {"type", type},
      {"id", generateId()},
      {"data", data}};

  std::string messageStr = message.dump();

  std::lock_guard<std::mutex> lock(m_clientsMutex);

  for (auto& client : m_clients) {
    if (client.second.isAuthenticated) {
      sendMessage(client.second.hdl, messageStr);
    }
  }
}

}  // namespace mcp
}  // namespace aero