/**
 * @file mcp_client_example.cpp
 * @brief Model Context Protocol (MCP) クライアントの使用例
 *
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "../mcp_manager.h"

using namespace aero::mcp;
using json = nlohmann::json;

// MCPサーバーのリクエストを作成するヘルパー関数
json createRequest(const std::string& type, const std::string& id, const json& params) {
  return {
      {"type", type},
      {"id", id},
      {"params", params}};
}

// エンジンを起動するヘルパー関数
std::string startEngine(MCPServer* server) {
  json params = {
      {"options", {{"enableJIT", true}, {"enableGC", true}, {"stackSize", 1024 * 1024}, {"heapSize", 16 * 1024 * 1024}, {"contextOptions", {{"strictMode", true}, {"enableConsole", true}, {"enableModules", true}}}}}};

  json request = createRequest("engine.start", "req-001", params);

  // リクエストを処理
  std::string response = server->handleRequest(request.dump());

  // レスポンスを解析
  json responseJson = json::parse(response);

  if (responseJson["data"]["success"].get<bool>()) {
    std::string engineId = responseJson["data"]["engineId"].get<std::string>();
    std::cout << "エンジンが起動しました: " << engineId << std::endl;
    return engineId;
  } else {
    std::cerr << "エンジン起動に失敗しました: " << responseJson["data"]["message"].get<std::string>() << std::endl;
    return "";
  }
}

// スクリプトを実行するヘルパー関数
void executeScript(MCPServer* server, const std::string& engineId, const std::string& script) {
  json params = {
      {"engineId", engineId},
      {"script", script},
      {"filename", "example.js"},
      {"options", {{"strictMode", true}, {"sourceType", "script"}}}};

  json request = createRequest("script.execute", "req-002", params);

  // リクエストを処理
  std::string response = server->handleRequest(request.dump());

  // レスポンスを解析
  json responseJson = json::parse(response);

  if (responseJson["data"]["success"].get<bool>()) {
    std::cout << "スクリプトが実行されました" << std::endl;
    std::cout << "結果: " << responseJson["data"]["result"].dump(2) << std::endl;
    std::cout << "実行時間: " << responseJson["data"]["executionTime"].get<int>() << "ms" << std::endl;
  } else {
    std::cerr << "スクリプト実行に失敗しました" << std::endl;
    if (responseJson["data"].contains("error")) {
      std::cerr << "エラー名: " << responseJson["data"]["error"]["name"].get<std::string>() << std::endl;
      std::cerr << "エラーメッセージ: " << responseJson["data"]["error"]["message"].get<std::string>() << std::endl;
    }
  }
}

// メモリ使用状況を取得するヘルパー関数
void getMemoryUsage(MCPServer* server, const std::string& engineId) {
  json params = {
      {"engineId", engineId},
      {"detailed", true}};

  json request = createRequest("memory.getUsage", "req-003", params);

  // リクエストを処理
  std::string response = server->handleRequest(request.dump());

  // レスポンスを解析
  json responseJson = json::parse(response);

  if (responseJson["data"]["success"].get<bool>()) {
    json memoryInfo = responseJson["data"]["memory"];
    std::cout << "メモリ使用状況:" << std::endl;
    std::cout << "  ヒープサイズ: " << memoryInfo["heapSize"].get<int>() / (1024 * 1024) << "MB" << std::endl;
    std::cout << "  使用メモリ: " << memoryInfo["heapUsed"].get<int>() / (1024 * 1024) << "MB" << std::endl;
    std::cout << "  オブジェクト数: " << memoryInfo["objectCount"].get<int>() << std::endl;
    std::cout << "  GC実行回数: " << memoryInfo["gcMetrics"]["gcCount"].get<int>() << std::endl;
  } else {
    std::cerr << "メモリ使用状況の取得に失敗しました" << std::endl;
  }
}

// エンジンを停止するヘルパー関数
void stopEngine(MCPServer* server, const std::string& engineId) {
  json params = {
      {"engineId", engineId}};

  json request = createRequest("engine.stop", "req-004", params);

  // リクエストを処理
  std::string response = server->handleRequest(request.dump());

  // レスポンスを解析
  json responseJson = json::parse(response);

  if (responseJson["data"]["success"].get<bool>()) {
    std::cout << "エンジンが停止しました: " << engineId << std::endl;
  } else {
    std::cerr << "エンジン停止に失敗しました: " << responseJson["data"]["message"].get<std::string>() << std::endl;
  }
}

int main() {
  try {
    // MCPマネージャーのインスタンスを取得
    MCPManager& manager = MCPManager::getInstance();

    // MCPサーバーを初期化
    MCPServerOptions options;
    options.serverName = "AeroJS-Example-Server";
    options.version = "1.0.0";
    options.enableAuthentication = false;  // 認証を無効化（例のため）

    if (!manager.initialize(options)) {
      std::cerr << "MCPマネージャーの初期化に失敗しました" << std::endl;
      return 1;
    }

    // サーバーを起動
    if (!manager.startServer(9229)) {
      std::cerr << "MCPサーバーの起動に失敗しました" << std::endl;
      return 1;
    }

    // サーバーインスタンスを取得
    MCPServer* server = manager.getServer();
    if (!server) {
      std::cerr << "MCPサーバーインスタンスの取得に失敗しました" << std::endl;
      return 1;
    }

    std::cout << "MCPサーバーが起動しました" << std::endl;

    // エンジンを起動
    std::string engineId = startEngine(server);
    if (engineId.empty()) {
      return 1;
    }

    // スクリプトを実行
    executeScript(server, engineId, "function greet(name) { return 'Hello, ' + name + '!'; } greet('AeroJS');");

    // メモリ使用状況を取得
    getMemoryUsage(server, engineId);

    // エンジンを停止
    stopEngine(server, engineId);

    // サーバーを停止
    manager.stopServer();

    // マネージャーをシャットダウン
    manager.shutdown();

    std::cout << "プログラムが正常に終了しました" << std::endl;
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "エラーが発生しました: " << e.what() << std::endl;
    return 1;
  }
}
