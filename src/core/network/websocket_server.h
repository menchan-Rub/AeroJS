/**
 * @file websocket_server.h
 * @brief WebSocketサーバークラスの定義
 * 
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#ifndef AERO_WEBSOCKET_SERVER_H
#define AERO_WEBSOCKET_SERVER_H

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>

namespace aero {

// 前方宣言
class WebSocketServerImpl;
class WebSocketConnectionImpl;

/**
 * @brief WebSocket接続クラス
 * 
 * 個々のWebSocket接続を表すクラス。
 * メッセージの送受信、接続の管理などの機能を提供します。
 */
class WebSocketConnection {
public:
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

private:
    std::unique_ptr<WebSocketConnectionImpl> m_impl; ///< 実装オブジェクト
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
     * @brief エラーハンドラ関数の型定義
     */
    using ErrorHandler = std::function<void(WebSocketConnection*, const std::string&)>;
    
    /**
     * @brief コンストラクタ
     * @param host ホスト名またはIPアドレス
     * @param port ポート番号
     */
    WebSocketServer(const std::string& host, int port);
    
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
     * @brief 接続ハンドラを設定する
     * @param onConnect 新しい接続があった場合に呼び出される関数
     */
    void setConnectionHandler(ConnectionHandler onConnect);
    
    /**
     * @brief 切断ハンドラを設定する
     * @param onDisconnect 接続が切断された場合に呼び出される関数
     */
    void setDisconnectionHandler(ConnectionHandler onDisconnect);
    
    /**
     * @brief メッセージハンドラを設定する
     * @param onMessage メッセージを受信した場合に呼び出される関数
     */
    void setMessageHandler(MessageHandler onMessage);
    
    /**
     * @brief エラーハンドラを設定する
     * @param onError エラーが発生した場合に呼び出される関数
     */
    void setErrorHandler(ErrorHandler onError);
    
    /**
     * @brief すべての接続を取得する
     * @return 現在の接続のリスト
     */
    std::vector<WebSocketConnection*> getAllConnections() const;
    
    /**
     * @brief すべての接続にメッセージをブロードキャストする
     * @param message ブロードキャストするメッセージ
     * @param excludeConnection 除外する接続（オプション）
     */
    void broadcast(const std::string& message, WebSocketConnection* excludeConnection = nullptr);
    
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

private:
    std::unique_ptr<WebSocketServerImpl> m_impl; ///< 実装オブジェクト
    std::string m_host;                         ///< ホスト名
    int m_port;                                 ///< ポート番号
    std::atomic<bool> m_isRunning{false};       ///< サーバー動作中フラグ
    
    mutable std::mutex m_connectionsMutex;       ///< 接続リスト用ミューテックス
    std::unordered_map<std::string, WebSocketConnection*> m_connections; ///< 接続リスト
    
    ConnectionHandler m_onConnect;               ///< 接続ハンドラ
    ConnectionHandler m_onDisconnect;            ///< 切断ハンドラ
    MessageHandler m_onMessage;                  ///< メッセージハンドラ
    ErrorHandler m_onError;                      ///< エラーハンドラ
    
    std::unique_ptr<std::thread> m_serverThread; ///< サーバースレッド
};

} // namespace aero

#endif // AERO_WEBSOCKET_SERVER_H 