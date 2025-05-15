/**
 * @file websocket_server.h
 * @brief WebSocketサーバークラスの定義
 *
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#ifndef AERO_WEBSOCKET_SERVER_H
#define AERO_WEBSOCKET_SERVER_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <queue>
#include <optional>
#include <chrono>

namespace aero {

// 前方宣言
class WebSocketServerImpl;
class WebSocketConnectionImpl;

/**
 * @brief WebSocket接続設定
 */
struct WebSocketConfig {
  bool enableCompression = true;     ///< メッセージ圧縮を有効にする
  bool enablePerMessageDeflate = true; ///< per-message deflate拡張を有効にする
  size_t maxMessageSize = 10 * 1024 * 1024; ///< 最大メッセージサイズ（バイト）
  std::chrono::milliseconds pingInterval{30000}; ///< ping間隔（ミリ秒）
  std::chrono::milliseconds handshakeTimeout{10000}; ///< ハンドシェイクタイムアウト（ミリ秒）
  int fragmentationThreshold = 1 * 1024 * 1024; ///< フラグメント化閾値（バイト）
};

/**
 * @brief WebSocket接続クラス
 *
 * 個々のWebSocket接続を表すクラス。
 * メッセージの送受信、接続の管理などの機能を提供します。
 */
class WebSocketConnection {
 public:
  /**
   * @brief メッセージ圧縮方式
   */
  enum class CompressionMethod {
    NONE,       ///< 圧縮なし
    DEFLATE,    ///< zlib deflate
    BROTLI,     ///< Brotli
    CUSTOM      ///< カスタム圧縮
  };
  
  /**
   * @brief メッセージタイプ
   */
  enum class MessageType {
    TEXT,       ///< テキストメッセージ
    BINARY,     ///< バイナリメッセージ
    PING,       ///< Pingメッセージ
    PONG,       ///< Pongメッセージ
    CLOSE       ///< クローズメッセージ
  };

  /**
   * @brief コンストラクタ
   * @param impl 実装オブジェクト
   */
  explicit WebSocketConnection(std::unique_ptr<WebSocketConnectionImpl> impl);

  /**
   * @brief デストラクタ
   */
  ~WebSocketConnection();

  /**
   * @brief メッセージを送信する
   * @param message 送信するメッセージ
   * @return 送信成功時はtrue、失敗時はfalse
   */
  bool send(const std::string& message);

  /**
   * @brief バイナリデータを送信する
   * @param data 送信するデータ
   * @param size データのサイズ
   * @return 送信成功時はtrue、失敗時はfalse
   */
  bool sendBinary(const void* data, size_t size);

  /**
   * @brief 圧縮してメッセージを送信する
   * @param message 送信するメッセージ
   * @param method 圧縮方式
   * @param level 圧縮レベル（0-9、0は無圧縮、9は最高圧縮）
   * @return 送信成功時はtrue、失敗時はfalse
   */
  bool sendCompressed(const std::string& message, CompressionMethod method = CompressionMethod::DEFLATE, int level = 6);

  /**
   * @brief メッセージをフラグメント化して送信する
   * @param message 送信するメッセージ
   * @param fragmentSize フラグメントサイズ（バイト）
   * @return 送信成功時はtrue、失敗時はfalse
   */
  bool sendFragmented(const std::string& message, size_t fragmentSize = 16384);

  /**
   * @brief Pingメッセージを送信する
   * @param data 送信するデータ（オプション）
   * @return 送信成功時はtrue、失敗時はfalse
   */
  bool sendPing(const std::string& data = "");

  /**
   * @brief 接続を閉じる
   * @param code 終了コード
   * @param reason 終了理由
   */
  void close(int code = 1000, const std::string& reason = "Normal closure");

  /**
   * @brief 接続が開いているかどうかを確認する
   * @return 接続が開いている場合はtrue、それ以外はfalse
   */
  bool isOpen() const;

  /**
   * @brief 接続IDを取得する
   * @return 接続ID
   */
  std::string getId() const;

  /**
   * @brief リモートアドレスを取得する
   * @return リモートアドレス
   */
  std::string getRemoteAddress() const;

  /**
   * @brief プロトコルを取得する
   * @return 使用中のサブプロトコル
   */
  std::string getProtocol() const;

  /**
   * @brief 接続の統計情報を取得する
   * @return 統計情報（JSON形式）
   */
  std::string getStats() const;

  /**
   * @brief カスタムデータを設定する
   * @param key データのキー
   * @param data 設定するデータ
   */
  void setUserData(const std::string& key, const std::string& data);

  /**
   * @brief カスタムデータを取得する
   * @param key データのキー
   * @return データの内容（存在しない場合は空文字列）
   */
  std::string getUserData(const std::string& key) const;

  /**
   * @brief メッセージの受信待機タイムアウトを設定する
   * @param timeout タイムアウト時間（ミリ秒）
   */
  void setReceiveTimeout(std::chrono::milliseconds timeout);

  /**
   * @brief メッセージを非同期で送信する
   * @param message 送信するメッセージ
   * @param callback 送信完了時のコールバック
   */
  void sendAsync(const std::string& message, std::function<void(bool)> callback = nullptr);

 private:
  std::unique_ptr<WebSocketConnectionImpl> m_impl;  ///< 実装オブジェクト
};

/**
 * @brief WebSocketサーバークラス
 *
 * WebSocketサーバーの機能を提供するクラス。
 * サーバーの起動、停止、接続の管理などの機能を提供します。
 */
class WebSocketServer {
 public:
  /**
   * @brief 接続ハンドラ関数の型定義
   */
  using ConnectionHandler = std::function<void(WebSocketConnection*)>;

  /**
   * @brief メッセージハンドラ関数の型定義
   */
  using MessageHandler = std::function<void(WebSocketConnection*, const std::string&)>;

  /**
   * @brief バイナリメッセージハンドラ関数の型定義
   */
  using BinaryMessageHandler = std::function<void(WebSocketConnection*, const void*, size_t)>;

  /**
   * @brief エラーハンドラ関数の型定義
   */
  using ErrorHandler = std::function<void(WebSocketConnection*, const std::string&)>;

  /**
   * @brief HTTPアップグレードハンドラ関数の型定義
   */
  using HttpHandler = std::function<bool(const std::string& path, const std::unordered_map<std::string, std::string>& headers)>;

  /**
   * @brief サーバー設定オプション
   */
  struct ServerOptions {
    std::string host = "0.0.0.0";    ///< 待ち受けホスト
    int port = 8080;                  ///< 待ち受けポート
    WebSocketConfig wsConfig;         ///< WebSocket設定
    size_t maxConnections = 1000;     ///< 最大接続数
    size_t threadPoolSize = 4;        ///< スレッドプールサイズ
    bool enableSSL = false;           ///< SSL/TLS有効化
    std::string certPath = "";        ///< SSL証明書パス
    std::string keyPath = "";         ///< SSL秘密鍵パス
    std::vector<std::string> allowedOrigins; ///< 許可されたオリジン（CORS）
    std::vector<std::string> protocols;      ///< サポートするサブプロトコル
  };

  /**
   * @brief コンストラクタ
   * @param host ホスト名またはIPアドレス
   * @param port ポート番号
   */
  WebSocketServer(const std::string& host, int port);

  /**
   * @brief オプション付きコンストラクタ
   * @param options サーバー設定オプション
   */
  explicit WebSocketServer(const ServerOptions& options);

  /**
   * @brief デストラクタ
   */
  ~WebSocketServer();

  /**
   * @brief サーバーを起動する
   * @return 起動成功時はtrue、失敗時はfalse
   */
  bool start();

  /**
   * @brief サーバーを停止する
   */
  void stop();

  /**
   * @brief サーバーが動作中かどうかを確認する
   * @return 動作中の場合はtrue、それ以外はfalse
   */
  bool isRunning() const;

  /**
   * @brief サーバーイベントループを1回実行する
   * @param timeout タイムアウト時間（ミリ秒）
   * @return 処理されたイベント数
   */
  int update(std::chrono::milliseconds timeout = std::chrono::milliseconds(10));

  /**
   * @brief 新しい接続時のハンドラーを設定する
   * @param onConnect 接続ハンドラー
   */
  void setConnectionHandler(ConnectionHandler onConnect);

  /**
   * @brief 切断時のハンドラーを設定する
   * @param onDisconnect 切断ハンドラー
   */
  void setDisconnectionHandler(ConnectionHandler onDisconnect);

  /**
   * @brief メッセージ受信時のハンドラーを設定する
   * @param onMessage メッセージハンドラー
   */
  void setMessageHandler(MessageHandler onMessage);

  /**
   * @brief バイナリメッセージ受信時のハンドラーを設定する
   * @param onBinaryMessage バイナリメッセージハンドラー
   */
  void setBinaryMessageHandler(BinaryMessageHandler onBinaryMessage);

  /**
   * @brief エラー発生時のハンドラーを設定する
   * @param onError エラーハンドラー
   */
  void setErrorHandler(ErrorHandler onError);

  /**
   * @brief HTTPアップグレード時のハンドラーを設定する
   * @param onHttp HTTPハンドラー
   */
  void setHttpHandler(HttpHandler onHttp);

  /**
   * @brief すべての接続を取得する
   * @return 接続のリスト
   */
  std::vector<WebSocketConnection*> getAllConnections() const;

  /**
   * @brief すべての接続にメッセージをブロードキャストする
   * @param message 送信するメッセージ
   * @param excludeConnection 除外する接続（省略可）
   */
  void broadcast(const std::string& message, WebSocketConnection* excludeConnection = nullptr);

  /**
   * @brief すべての接続にバイナリデータをブロードキャストする
   * @param data 送信するデータ
   * @param size データのサイズ
   * @param excludeConnection 除外する接続（省略可）
   */
  void broadcastBinary(const void* data, size_t size, WebSocketConnection* excludeConnection = nullptr);

  /**
   * @brief 接続数を取得する
   * @return 現在の接続数
   */
  size_t getConnectionCount() const;

  /**
   * @brief ホスト名を取得する
   * @return ホスト名
   */
  std::string getHost() const;

  /**
   * @brief ポート番号を取得する
   * @return ポート番号
   */
  int getPort() const;

  /**
   * @brief サーバーの統計情報を取得する
   * @return 統計情報（JSON形式の文字列）
   */
  std::string getStatsJson() const;

  /**
   * @brief サブプロトコルを追加する
   * @param protocol サブプロトコル名
   */
  void addProtocol(const std::string& protocol);

  /**
   * @brief 許可されたオリジンを追加する
   * @param origin オリジンURL
   */
  void addAllowedOrigin(const std::string& origin);

  /**
   * @brief サーバー設定を取得する
   * @return 現在のサーバー設定
   */
  const ServerOptions& getOptions() const;

 private:
  std::unique_ptr<WebSocketServerImpl> m_impl;  ///< 実装オブジェクト
  std::string m_host;                          ///< ホスト名
  int m_port;                                  ///< ポート番号
  std::atomic<bool> m_isRunning;               ///< 動作中フラグ
  std::unique_ptr<std::thread> m_serverThread; ///< サーバースレッド

  // ハンドラ
  ConnectionHandler m_onConnect;            ///< 接続ハンドラ
  ConnectionHandler m_onDisconnect;         ///< 切断ハンドラ
  MessageHandler m_onMessage;               ///< メッセージハンドラ
  BinaryMessageHandler m_onBinaryMessage;   ///< バイナリメッセージハンドラ
  ErrorHandler m_onError;                   ///< エラーハンドラ
  
  // サーバー設定
  ServerOptions m_options;                  ///< サーバー設定オプション
};

}  // namespace aero

#endif  // AERO_WEBSOCKET_SERVER_H