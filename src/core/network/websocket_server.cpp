/**
 * @file websocket_server.cpp
 * @brief WebSocketサーバーの実装
 *
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#include "websocket_server.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#include "../../utils/logger.h"

// WebSocketの実装にlibwebsocketsを使用
#include <libwebsockets.h>

namespace aero {

// ロガーの定義
static Logger& logger = Logger::getInstance("WebSocketServer");

/**
 * @brief WebSocket接続の実装クラス
 */
class WebSocketConnectionImpl {
 public:
  /**
   * @brief コンストラクタ
   *
   * @param wsi libwebsockets接続インスタンス
   * @param context libwebsockets コンテキスト
   * @param id 接続ID
   * @param remoteAddress リモートアドレス
   */
  WebSocketConnectionImpl(lws* wsi, lws_context* context, const std::string& id, const std::string& remoteAddress)
      : m_wsi(wsi), m_context(context), m_id(id), m_remoteAddress(remoteAddress), m_isOpen(true) {
    logger.debug("新しいWebSocket接続を作成: ID={}, アドレス={}", id, remoteAddress);
  }

  /**
   * @brief デストラクタ
   */
  ~WebSocketConnectionImpl() {
    if (m_isOpen) {
      close(1000, "Connection closed by server");
    }
    logger.debug("WebSocket接続を破棄: ID={}", m_id);
  }

  /**
   * @brief メッセージを送信する
   *
   * @param message 送信するメッセージ
   * @return bool 送信成功時はtrue、失敗時はfalse
   */
  bool send(const std::string& message) {
    if (!m_isOpen || !m_wsi) {
      logger.error("送信失敗: 接続が閉じられています: ID={}", m_id);
      return false;
    }

    // LWSでデータを送信するためのバッファを準備
    // LWS_PRE バイトのプリアンブルを確保
    std::vector<unsigned char> buffer(LWS_PRE + message.size());
    std::copy(message.begin(), message.end(), buffer.begin() + LWS_PRE);

    // 送信
    int result = lws_write(m_wsi, buffer.data() + LWS_PRE, message.size(), LWS_WRITE_TEXT);
    if (result < 0) {
      logger.error("メッセージ送信中にエラーが発生: ID={}, エラーコード={}", m_id, result);
      return false;
    } else if (static_cast<size_t>(result) != message.size()) {
      logger.warning("メッセージの一部のみが送信されました: ID={}, 送信={}, 予定={}", m_id, result, message.size());
      return false;
    }

    logger.debug("メッセージを送信: ID={}, サイズ={}", m_id, message.size());
    return true;
  }

  /**
   * @brief バイナリデータを送信する
   *
   * @param data 送信するデータ
   * @param size データのサイズ
   * @return bool 送信成功時はtrue、失敗時はfalse
   */
  bool sendBinary(const void* data, size_t size) {
    if (!m_isOpen || !m_wsi) {
      logger.error("バイナリ送信失敗: 接続が閉じられています: ID={}", m_id);
      return false;
    }

    // LWSでデータを送信するためのバッファを準備
    std::vector<unsigned char> buffer(LWS_PRE + size);
    std::copy(static_cast<const unsigned char*>(data),
              static_cast<const unsigned char*>(data) + size,
              buffer.begin() + LWS_PRE);

    // 送信
    int result = lws_write(m_wsi, buffer.data() + LWS_PRE, size, LWS_WRITE_BINARY);
    if (result < 0) {
      logger.error("バイナリデータ送信中にエラーが発生: ID={}, エラーコード={}", m_id, result);
      return false;
    } else if (static_cast<size_t>(result) != size) {
      logger.warning("バイナリデータの一部のみが送信されました: ID={}, 送信={}, 予定={}", m_id, result, size);
      return false;
    }

    logger.debug("バイナリデータを送信: ID={}, サイズ={}", m_id, size);
    return true;
  }

  /**
   * @brief 接続を閉じる
   *
   * @param code 終了コード
   * @param reason 終了理由
   */
  void close(int code, const std::string& reason) {
    if (!m_isOpen || !m_wsi) {
      return;
    }

    logger.debug("接続を閉じています: ID={}, コード={}, 理由={}", m_id, code, reason);

    // 切断コードと理由を設定
    unsigned char buffer[LWS_PRE + 2 + 125];  // 2バイトのコード + 最大125バイトの理由
    buffer[LWS_PRE] = (code >> 8) & 0xFF;
    buffer[LWS_PRE + 1] = code & 0xFF;

    size_t reasonLength = std::min(reason.size(), static_cast<size_t>(123));
    if (reasonLength > 0) {
      std::copy(reason.begin(), reason.begin() + reasonLength, buffer + LWS_PRE + 2);
    }

    // 切断メッセージを送信
    lws_write(m_wsi, buffer + LWS_PRE, 2 + reasonLength, LWS_WRITE_CLOSE);

    // コールバックで切断が処理されるが、状態を更新しておく
    m_isOpen = false;
  }

  /**
   * @brief 接続が開いているかどうかを確認する
   *
   * @return bool 接続が開いている場合はtrue、それ以外はfalse
   */
  bool isOpen() const {
    return m_isOpen;
  }

  /**
   * @brief 接続IDを取得する
   *
   * @return std::string 接続ID
   */
  std::string getId() const {
    return m_id;
  }

  /**
   * @brief リモートアドレスを取得する
   *
   * @return std::string リモートアドレス
   */
  std::string getRemoteAddress() const {
    return m_remoteAddress;
  }

  /**
   * @brief カスタムデータを設定する
   *
   * @param key データのキー
   * @param value 設定するデータ
   */
  void setUserData(const std::string& key, const std::string& value) {
    m_userData[key] = value;
  }

  /**
   * @brief カスタムデータを取得する
   *
   * @param key データのキー
   * @return std::string データの内容（存在しない場合は空文字列）
   */
  std::string getUserData(const std::string& key) const {
    auto it = m_userData.find(key);
    if (it != m_userData.end()) {
      return it->second;
    }
    return "";
  }

  /**
   * @brief libwebsocketsハンドルを設定する
   *
   * @param wsi 新しいlibwebsocketsハンドル
   */
  void setWSI(lws* wsi) {
    m_wsi = wsi;
  }

  /**
   * @brief 接続の状態を設定する
   *
   * @param isOpen 接続状態
   */
  void setOpen(bool isOpen) {
    m_isOpen = isOpen;
  }

  // WebSocketConnectionImpl::sendCompressed の実装
  bool sendCompressed(const std::string& message, WebSocketConnection::CompressionMethod method, int level) {
    if (!m_isOpen || !m_wsi) {
      logger.error("切断された接続にメッセージを送信しようとしました: ID={}", m_id);
      return false;
    }

    try {
      std::vector<uint8_t> compressedData;
      
      switch (method) {
        case WebSocketConnection::CompressionMethod::DEFLATE: {
          // zlib deflateを使用した圧縮
          compressedData = compressWithZlib(message, level);
          break;
        }
        case WebSocketConnection::CompressionMethod::BROTLI: {
          // Brotli圧縮（利用可能な場合）
  #ifdef HAVE_BROTLI
          compressedData = compressWithBrotli(message, level);
  #else
          logger.warn("Brotli圧縮が利用できないため、zlibにフォールバックします");
          compressedData = compressWithZlib(message, level);
  #endif
          break;
        }
        case WebSocketConnection::CompressionMethod::CUSTOM: {
          // カスタム圧縮（必要に応じて実装）
          logger.warn("カスタム圧縮は実装されていません。zlibにフォールバックします");
          compressedData = compressWithZlib(message, level);
          break;
        }
        default:
          // 圧縮なし
          return send(message);
      }

      // 圧縮データの送信
      if (compressedData.empty()) {
        logger.error("圧縮に失敗しました: ID={}", m_id);
        return false;
      }

      return sendRawData(compressedData.data(), compressedData.size(), LWS_WRITE_BINARY);
    } catch (const std::exception& e) {
      logger.error("圧縮メッセージの送信中にエラーが発生しました: {}", e.what());
      return false;
    }
  }

  // WebSocketConnectionImpl::sendFragmented の実装
  bool sendFragmented(const std::string& message, size_t fragmentSize) {
    if (!m_isOpen || !m_wsi) {
      logger.error("切断された接続にメッセージを送信しようとしました: ID={}", m_id);
      return false;
    }

    try {
      const size_t totalSize = message.size();
      if (totalSize <= fragmentSize) {
        // フラグメント化が不要な場合は通常送信
        return send(message);
      }

      size_t offset = 0;
      bool isFirst = true;
      bool success = true;

      while (offset < totalSize) {
        size_t chunkSize = std::min(fragmentSize, totalSize - offset);
        std::string_view chunk(message.data() + offset, chunkSize);
        
        // フラグメントの種類を決定
        int writeMode;
        if (isFirst) {
          writeMode = LWS_WRITE_TEXT_START;
          isFirst = false;
        } else if (offset + chunkSize >= totalSize) {
          writeMode = LWS_WRITE_TEXT_FINAL;
        } else {
          writeMode = LWS_WRITE_TEXT_CONTINUATION;
        }
        
        // フラグメントの送信
        if (!sendRawData(chunk.data(), chunk.size(), writeMode)) {
          logger.error("フラグメントの送信に失敗しました: ID={}, オフセット={}", m_id, offset);
          success = false;
          break;
        }
        
        offset += chunkSize;
      }
      
      return success;
    } catch (const std::exception& e) {
      logger.error("フラグメント化メッセージの送信中にエラーが発生しました: {}", e.what());
      return false;
    }
  }

  // WebSocketConnectionImpl::sendPing の実装
  bool sendPing(const std::string& data) {
    if (!m_isOpen || !m_wsi) {
      return false;
    }

    try {
      // データサイズを125バイト以下に制限（WebSocketの制約）
      std::string pingData = data;
      if (pingData.size() > 125) {
        pingData.resize(125);
        logger.warn("Pingデータが大きすぎるため切り詰めました: ID={}", m_id);
      }

      // PINGフレームの送信
      return sendRawData(pingData.data(), pingData.size(), LWS_WRITE_PING);
    } catch (const std::exception& e) {
      logger.error("PINGの送信中にエラーが発生しました: {}", e.what());
      return false;
    }
  }

  // WebSocketConnectionImpl::getProtocol の実装
  std::string getProtocol() const {
    if (!m_wsi) {
      return "";
    }

    return lws_get_protocol(m_wsi)->name;
  }

  // WebSocketConnectionImpl::getStats の実装
  std::string getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    json stats;
    stats["id"] = m_id;
    stats["remote_address"] = m_remoteAddress;
    stats["connection_time"] = m_connectionTimeStr;
    stats["is_open"] = m_isOpen;
    stats["bytes_sent"] = m_bytesSent;
    stats["bytes_received"] = m_bytesReceived;
    stats["messages_sent"] = m_messagesSent;
    stats["messages_received"] = m_messagesReceived;
    stats["last_activity_time"] = m_lastActivityTimeStr;
    
    return stats.dump();
  }

  // WebSocketConnection の setReceiveTimeout メソッドの実装
  void setReceiveTimeout(std::chrono::milliseconds timeout) {
    m_receiveTimeout = timeout;
  }

  // WebSocketConnection の sendAsync メソッドの実装
  void sendAsync(const std::string& message, std::function<void(bool)> callback) {
    m_asyncQueue.push_back({message, callback});
  }

 private:
  lws* m_wsi;                                               ///< libwebsockets接続インスタンス
  lws_context* m_context;                                   ///< libwebsockets コンテキスト
  std::string m_id;                                         ///< 接続ID
  std::string m_remoteAddress;                              ///< リモートアドレス
  std::atomic<bool> m_isOpen;                               ///< 接続状態
  std::unordered_map<std::string, std::string> m_userData;  ///< ユーザーデータ
  std::chrono::milliseconds m_receiveTimeout;                ///< 受信タイムアウト
  std::vector<std::pair<std::string, std::function<void(bool)>>> m_asyncQueue;  ///< 非同期メッセージキュー
};

/**
 * @brief libwebsockets用の接続データ構造体
 */
struct LwsConnectionData {
  WebSocketConnection* connection;
  std::string pendingMessage;
};

/**
 * @brief WebSocketサーバーの実装クラス
 */
class WebSocketServerImpl {
 public:
  /**
   * @brief コンストラクタ
   *
   * @param server WebSocketサーバーインスタンス
   * @param host ホスト名
   * @param port ポート番号
   */
  WebSocketServerImpl(WebSocketServer* server, const std::string& host, int port)
      : m_server(server), m_host(host), m_port(port), m_context(nullptr), m_exitFlag(false) {
  }

  /**
   * @brief デストラクタ
   */
  ~WebSocketServerImpl() {
    stop();
  }

  /**
   * @brief サーバーを起動する
   *
   * @return bool 起動成功時はtrue、失敗時はfalse
   */
  bool start() {
    // サーバーが既に動作中の場合は成功を返す
    if (m_context) {
      logger.info("サーバーは既に実行中です: {}:{}", m_host, m_port);
      return true;
    }

    logger.info("WebSocketサーバーを開始: {}:{}", m_host, m_port);

    // libwebsockets用のプロトコルハンドラーを設定
    static const lws_protocols protocols[] = {
        {"aerojs-protocol",
         &WebSocketServerImpl::callbackFunction,
         sizeof(LwsConnectionData),
         4096,  // rx buffer size
         0, nullptr, 0},
        {nullptr, nullptr, 0, 0, 0, nullptr, 0}  // 終端
    };

    // サーバー作成情報
    lws_context_creation_info info = {};
    info.port = m_port;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8 | LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
    info.user = this;

    if (!m_host.empty() && m_host != "0.0.0.0") {
      info.iface = m_host.c_str();
    }

    // コンテキストの作成
    m_context = lws_create_context(&info);
    if (!m_context) {
      logger.error("WebSocketサーバーコンテキストの作成に失敗しました: {}:{}", m_host, m_port);
      return false;
    }

    m_exitFlag = false;

    logger.info("WebSocketサーバーが開始されました: {}:{}", m_host, m_port);
    return true;
  }

  /**
   * @brief サーバーを停止する
   */
  void stop() {
    if (!m_context) {
      return;
    }

    logger.info("WebSocketサーバーを停止しています: {}:{}", m_host, m_port);

    m_exitFlag = true;

    // すべての接続を閉じる
    for (auto& pair : m_connections) {
      if (pair.second->isOpen()) {
        pair.second->close(1001, "Server shutting down");
      }
    }

    // コンテキストを破棄
    lws_context_destroy(m_context);
    m_context = nullptr;

    logger.info("WebSocketサーバーが停止しました: {}:{}", m_host, m_port);
  }

  /**
   * @brief サーバーのイベントループを実行する
   */
  void run() {
    while (!m_exitFlag && m_context) {
      lws_service(m_context, 50);  // 50ms待機
    }
  }

  /**
   * @brief 接続IDを生成する
   *
   * @return std::string 生成された接続ID
   */
  std::string generateConnectionId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << "ws-";

    // 接続IDを生成 (UUID形式)
    for (int i = 0; i < 8; ++i) {
      ss << std::hex << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 4; ++i) {
      ss << std::hex << dis(gen);
    }
    ss << "-4";  // Version 4 UUID
    for (int i = 0; i < 3; ++i) {
      ss << std::hex << dis(gen);
    }
    ss << "-";
    ss << std::hex << (dis(gen) & 0x3 | 0x8);  // Variant
    for (int i = 0; i < 3; ++i) {
      ss << std::hex << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 12; ++i) {
      ss << std::hex << dis(gen);
    }

    return ss.str();
  }

  /**
   * @brief 接続を追加する
   *
   * @param wsi libwebsockets接続インスタンス
   * @return WebSocketConnection* 作成された接続
   */
  WebSocketConnection* addConnection(lws* wsi) {
    char remoteAddrBuf[128];
    lws_get_peer_simple(wsi, remoteAddrBuf, sizeof(remoteAddrBuf));
    std::string remoteAddr(remoteAddrBuf);

    std::string connectionId = generateConnectionId();

    // ConnectionImplを作成
    auto impl = std::make_unique<WebSocketConnectionImpl>(wsi, m_context, connectionId, remoteAddr);

    // Connectionを作成
    auto connection = new WebSocketConnection(std::move(impl));

    // マップに追加
    m_connections[connectionId] = connection;
    m_wsiConnectionMap[wsi] = connection;

    // リクエストユーザーデータを設定
    LwsConnectionData* userData = static_cast<LwsConnectionData*>(lws_wsi_user(wsi));
    if (userData) {
      userData->connection = connection;
    }

    // 接続ハンドラの呼び出し
    if (m_server && m_onConnect) {
      m_onConnect(connection);
    }

    logger.info("新しい接続を確立: ID={}, アドレス={}", connectionId, remoteAddr);
    return connection;
  }

  /**
   * @brief 接続を削除する
   *
   * @param wsi libwebsockets接続インスタンス
   */
  void removeConnection(lws* wsi) {
    auto it = m_wsiConnectionMap.find(wsi);
    if (it == m_wsiConnectionMap.end()) {
      return;
    }

    WebSocketConnection* connection = it->second;
    std::string connectionId = connection->getId();

    // 切断ハンドラの呼び出し
    if (m_server && m_onDisconnect) {
      m_onDisconnect(connection);
    }

    // マップから削除
    m_wsiConnectionMap.erase(wsi);
    m_connections.erase(connectionId);

    logger.info("接続を切断: ID={}", connectionId);

    // 接続を削除
    delete connection;
  }

  /**
   * @brief 接続を取得する
   *
   * @param wsi libwebsockets接続インスタンス
   * @return WebSocketConnection* 見つかった接続（見つからない場合はnullptr）
   */
  WebSocketConnection* getConnection(lws* wsi) {
    auto it = m_wsiConnectionMap.find(wsi);
    if (it != m_wsiConnectionMap.end()) {
      return it->second;
    }
    return nullptr;
  }

  /**
   * @brief IDから接続を取得する
   *
   * @param id 接続ID
   * @return WebSocketConnection* 見つかった接続（見つからない場合はnullptr）
   */
  WebSocketConnection* getConnectionById(const std::string& id) {
    auto it = m_connections.find(id);
    if (it != m_connections.end()) {
      return it->second;
    }
    return nullptr;
  }

  /**
   * @brief 接続リストを取得する
   *
   * @return std::vector<WebSocketConnection*> 接続のリスト
   */
  std::vector<WebSocketConnection*> getAllConnections() const {
    std::vector<WebSocketConnection*> result;
    result.reserve(m_connections.size());

    for (const auto& pair : m_connections) {
      result.push_back(pair.second);
    }

    return result;
  }

  /**
   * @brief 接続数を取得する
   *
   * @return size_t 接続数
   */
  size_t getConnectionCount() const {
    return m_connections.size();
  }

  /**
   * @brief すべての接続にメッセージをブロードキャストする
   *
   * @param message メッセージ内容
   * @param excludeConnection 除外する接続
   */
  void broadcast(const std::string& message, WebSocketConnection* excludeConnection) {
    for (const auto& pair : m_connections) {
      WebSocketConnection* connection = pair.second;
      if (connection != excludeConnection && connection->isOpen()) {
        connection->send(message);
      }
    }
  }

  /**
   * @brief ハンドラを設定する
   *
   * @param onConnect 接続ハンドラ
   * @param onDisconnect 切断ハンドラ
   * @param onMessage メッセージハンドラ
   * @param onError エラーハンドラ
   */
  void setHandlers(
      WebSocketServer::ConnectionHandler onConnect,
      WebSocketServer::ConnectionHandler onDisconnect,
      WebSocketServer::MessageHandler onMessage,
      WebSocketServer::ErrorHandler onError) {
    m_onConnect = onConnect;
    m_onDisconnect = onDisconnect;
    m_onMessage = onMessage;
    m_onError = onError;
  }

  /**
   * @brief libwebsocketsコールバック関数
   *
   * @param wsi libwebsockets接続インスタンス
   * @param reason イベント理由
   * @param user ユーザーデータ
   * @param in 入力データ
   * @param len データ長
   * @return int libwebsocketsの結果コード
   */
  static int callbackFunction(lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {
    LwsConnectionData* userData = static_cast<LwsConnectionData*>(user);
    WebSocketServerImpl* impl = static_cast<WebSocketServerImpl*>(lws_context_user(lws_get_context(wsi)));

    if (!impl) {
      return 0;
    }

    switch (reason) {
      case LWS_CALLBACK_ESTABLISHED: {
        // 新しい接続
        WebSocketConnection* connection = impl->addConnection(wsi);
        userData->connection = connection;
        break;
      }

      case LWS_CALLBACK_CLOSED: {
        // 接続の切断
        if (userData && userData->connection) {
          // 接続状態を更新
          WebSocketConnectionImpl* connImpl = reinterpret_cast<WebSocketConnectionImpl*>(userData->connection);
          if (connImpl) {
            connImpl->setOpen(false);
          }

          impl->removeConnection(wsi);
          userData->connection = nullptr;
        }
        break;
      }

      case LWS_CALLBACK_RECEIVE: {
        // メッセージの受信
        if (userData && userData->connection) {
          std::string message(static_cast<char*>(in), len);

          // メッセージハンドラの呼び出し
          if (impl->m_onMessage) {
            impl->m_onMessage(userData->connection, message);
          }
        }
        break;
      }

      case LWS_CALLBACK_SERVER_WRITEABLE: {
        // 送信可能なイベント
        if (userData && !userData->pendingMessage.empty()) {
          // 保留中のメッセージを送信
          std::string message = userData->pendingMessage;
          userData->pendingMessage.clear();

          unsigned char* buffer = static_cast<unsigned char*>(
              malloc(LWS_PRE + message.size()));
          if (!buffer) {
            return -1;
          }

          memcpy(buffer + LWS_PRE, message.c_str(), message.size());

          int result = lws_write(wsi, buffer + LWS_PRE, message.size(), LWS_WRITE_TEXT);
          free(buffer);

          if (result < 0) {
            return -1;
          }
        }
        break;
      }

      case LWS_CALLBACK_WSI_DESTROY: {
        // WSIの破棄
        if (userData && userData->connection) {
          WebSocketConnectionImpl* connImpl = reinterpret_cast<WebSocketConnectionImpl*>(userData->connection);
          if (connImpl) {
            connImpl->setWSI(nullptr);
            connImpl->setOpen(false);
          }
        }
        break;
      }

      default:
        break;
    }

    return 0;
  }

 private:
  WebSocketServer* m_server;     ///< サーバーインスタンス
  std::string m_host;            ///< ホスト名
  int m_port;                    ///< ポート番号
  lws_context* m_context;        ///< libwebsockets コンテキスト
  std::atomic<bool> m_exitFlag;  ///< 終了フラグ

  std::unordered_map<std::string, WebSocketConnection*> m_connections;  ///< 接続マップ (ID -> Connection)
  std::unordered_map<lws*, WebSocketConnection*> m_wsiConnectionMap;    ///< WSI -> Connection マップ

  WebSocketServer::ConnectionHandler m_onConnect;     ///< 接続ハンドラ
  WebSocketServer::ConnectionHandler m_onDisconnect;  ///< 切断ハンドラ
  WebSocketServer::MessageHandler m_onMessage;        ///< メッセージハンドラ
  WebSocketServer::ErrorHandler m_onError;            ///< エラーハンドラ
};

//
// WebSocketConnection の実装
//

WebSocketConnection::WebSocketConnection(std::unique_ptr<WebSocketConnectionImpl> impl)
    : m_impl(std::move(impl)) {
}

WebSocketConnection::~WebSocketConnection() {
}

bool WebSocketConnection::send(const std::string& message) {
  return m_impl->send(message);
}

bool WebSocketConnection::sendBinary(const void* data, size_t size) {
  return m_impl->sendBinary(data, size);
}

void WebSocketConnection::close(int code, const std::string& reason) {
  m_impl->close(code, reason);
}

bool WebSocketConnection::isOpen() const {
  return m_impl->isOpen();
}

std::string WebSocketConnection::getId() const {
  return m_impl->getId();
}

std::string WebSocketConnection::getRemoteAddress() const {
  return m_impl->getRemoteAddress();
}

void WebSocketConnection::setUserData(const std::string& key, const std::string& data) {
  m_impl->setUserData(key, data);
}

std::string WebSocketConnection::getUserData(const std::string& key) const {
  return m_impl->getUserData(key);
}

//
// WebSocketServer の実装
//

WebSocketServer::WebSocketServer(const std::string& host, int port)
    : m_host(host), m_port(port), m_isRunning(false) {
  // 実装を作成
  m_impl = std::make_unique<WebSocketServerImpl>(this, host, port);
}

WebSocketServer::~WebSocketServer() {
  stop();
}

bool WebSocketServer::start() {
  if (m_isRunning) {
    return true;
  }

  // サーバーを開始
  if (!m_impl->start()) {
    return false;
  }

  // サーバースレッドを作成
  m_serverThread = std::make_unique<std::thread>([this]() {
    m_impl->run();
  });

  m_isRunning = true;
  logger.info("WebSocketサーバースレッドを開始: {}:{}", m_host, m_port);

  return true;
}

void WebSocketServer::stop() {
  if (!m_isRunning) {
    return;
  }

  logger.info("WebSocketサーバーを停止しています: {}:{}", m_host, m_port);

  // 実装を停止
  m_impl->stop();
  m_isRunning = false;

  // スレッドが存在する場合は終了を待機
  if (m_serverThread && m_serverThread->joinable()) {
    m_serverThread->join();
    m_serverThread.reset();
  }

  logger.info("WebSocketサーバーが停止しました: {}:{}", m_host, m_port);
}

bool WebSocketServer::isRunning() const {
  return m_isRunning;
}

void WebSocketServer::setConnectionHandler(ConnectionHandler onConnect) {
  m_onConnect = onConnect;
  m_impl->setHandlers(m_onConnect, m_onDisconnect, m_onMessage, m_onError);
}

void WebSocketServer::setDisconnectionHandler(ConnectionHandler onDisconnect) {
  m_onDisconnect = onDisconnect;
  m_impl->setHandlers(m_onConnect, m_onDisconnect, m_onMessage, m_onError);
}

void WebSocketServer::setMessageHandler(MessageHandler onMessage) {
  m_onMessage = onMessage;
  m_impl->setHandlers(m_onConnect, m_onDisconnect, m_onMessage, m_onError);
}

void WebSocketServer::setErrorHandler(ErrorHandler onError) {
  m_onError = onError;
  m_impl->setHandlers(m_onConnect, m_onDisconnect, m_onMessage, m_onError);
}

std::vector<WebSocketConnection*> WebSocketServer::getAllConnections() const {
  return m_impl->getAllConnections();
}

void WebSocketServer::broadcast(const std::string& message, WebSocketConnection* excludeConnection) {
  m_impl->broadcast(message, excludeConnection);
}

size_t WebSocketServer::getConnectionCount() const {
  return m_impl->getConnectionCount();
}

std::string WebSocketServer::getHost() const {
  return m_host;
}

int WebSocketServer::getPort() const {
  return m_port;
}

}  // namespace aero