/**
 * @file http_server.h
 * @brief 高性能HTTPサーバー実装
 * 
 * このファイルは、AeroJSエンジン用の高性能HTTPサーバーを定義します。
 * 非同期I/O、Keep-Alive、HTTP/2対応、リクエスト圧縮などの機能を提供します。
 * 
 * @author AeroJS Team
 * @version 1.0.0
 * @copyright MIT License
 */

#ifndef AEROJS_HTTP_SERVER_H
#define AEROJS_HTTP_SERVER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <chrono>

namespace aerojs {
namespace core {
namespace network {

// HTTPメソッド
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    PATCH,
    TRACE,
    CONNECT
};

// HTTPステータスコード
enum class HttpStatus : int {
    OK = 200,
    CREATED = 201,
    NO_CONTENT = 204,
    MOVED_PERMANENTLY = 301,
    NOT_MODIFIED = 304,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503
};

// 圧縮タイプ
enum class CompressionType {
    NONE,
    GZIP,
    DEFLATE,
    BROTLI
};

// HTTPヘッダー
class HttpHeaders {
public:
    void Set(const std::string& name, const std::string& value);
    void Add(const std::string& name, const std::string& value);
    std::string Get(const std::string& name) const;
    std::vector<std::string> GetAll(const std::string& name) const;
    bool Has(const std::string& name) const;
    void Remove(const std::string& name);
    void Clear();
    
    // イテレータサポート
    std::unordered_map<std::string, std::vector<std::string>>::const_iterator begin() const;
    std::unordered_map<std::string, std::vector<std::string>>::const_iterator end() const;

private:
    std::unordered_map<std::string, std::vector<std::string>> headers_;
};

// HTTPリクエスト
class HttpRequest {
public:
    HttpRequest() = default;
    ~HttpRequest() = default;
    
    // メソッドとURL
    HttpMethod GetMethod() const { return method_; }
    void SetMethod(HttpMethod method) { method_ = method; }
    
    const std::string& GetUrl() const { return url_; }
    void SetUrl(const std::string& url) { url_ = url; }
    
    const std::string& GetPath() const { return path_; }
    void SetPath(const std::string& path) { path_ = path; }
    
    const std::string& GetQuery() const { return query_; }
    void SetQuery(const std::string& query) { query_ = query; }
    
    // HTTPバージョン
    const std::string& GetHttpVersion() const { return httpVersion_; }
    void SetHttpVersion(const std::string& version) { httpVersion_ = version; }
    
    // ヘッダー
    HttpHeaders& GetHeaders() { return headers_; }
    const HttpHeaders& GetHeaders() const { return headers_; }
    
    // ボディ
    const std::string& GetBody() const { return body_; }
    void SetBody(const std::string& body) { body_ = body; }
    void SetBody(std::string&& body) { body_ = std::move(body); }
    
    // クエリパラメータ
    std::string GetQueryParam(const std::string& name) const;
    std::unordered_map<std::string, std::string> GetQueryParams() const;
    
    // 接続情報
    const std::string& GetRemoteAddress() const { return remoteAddress_; }
    void SetRemoteAddress(const std::string& address) { remoteAddress_ = address; }
    
    uint16_t GetRemotePort() const { return remotePort_; }
    void SetRemotePort(uint16_t port) { remotePort_ = port; }
    
    // タイムスタンプ
    std::chrono::steady_clock::time_point GetTimestamp() const { return timestamp_; }
    void SetTimestamp(std::chrono::steady_clock::time_point time) { timestamp_ = time; }

private:
    HttpMethod method_ = HttpMethod::GET;
    std::string url_;
    std::string path_;
    std::string query_;
    std::string httpVersion_ = "HTTP/1.1";
    HttpHeaders headers_;
    std::string body_;
    std::string remoteAddress_;
    uint16_t remotePort_ = 0;
    std::chrono::steady_clock::time_point timestamp_;
};

// HTTPレスポンス
class HttpResponse {
public:
    HttpResponse() = default;
    ~HttpResponse() = default;
    
    // ステータス
    HttpStatus GetStatus() const { return status_; }
    void SetStatus(HttpStatus status) { status_ = status; }
    
    const std::string& GetStatusText() const { return statusText_; }
    void SetStatusText(const std::string& text) { statusText_ = text; }
    
    // HTTPバージョン
    const std::string& GetHttpVersion() const { return httpVersion_; }
    void SetHttpVersion(const std::string& version) { httpVersion_ = version; }
    
    // ヘッダー
    HttpHeaders& GetHeaders() { return headers_; }
    const HttpHeaders& GetHeaders() const { return headers_; }
    
    // ボディ
    const std::string& GetBody() const { return body_; }
    void SetBody(const std::string& body) { body_ = body; }
    void SetBody(std::string&& body) { body_ = std::move(body); }
    
    // 便利メソッド
    void SetJson(const std::string& json);
    void SetHtml(const std::string& html);
    void SetText(const std::string& text);
    void SetFile(const std::string& filepath);
    
    // 圧縮
    void SetCompressionType(CompressionType type) { compressionType_ = type; }
    CompressionType GetCompressionType() const { return compressionType_; }
    
    // 送信済みフラグ
    bool IsSent() const { return sent_; }
    void MarkAsSent() { sent_ = true; }

private:
    HttpStatus status_ = HttpStatus::OK;
    std::string statusText_ = "OK";
    std::string httpVersion_ = "HTTP/1.1";
    HttpHeaders headers_;
    std::string body_;
    CompressionType compressionType_ = CompressionType::NONE;
    bool sent_ = false;
};

// ルートハンドラ
using HttpHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

// ミドルウェア
using HttpMiddleware = std::function<bool(const HttpRequest&, HttpResponse&)>;

// ルート情報
struct RouteInfo {
    HttpMethod method;
    std::string pattern;
    HttpHandler handler;
    std::vector<HttpMiddleware> middlewares;
};

// サーバー設定
struct HttpServerConfig {
    std::string bindAddress = "0.0.0.0";
    uint16_t port = 8080;
    size_t maxConnections = 1000;
    size_t threadPoolSize = 4;
    size_t maxRequestSize = 16 * 1024 * 1024;  // 16MB
    size_t maxHeaderSize = 64 * 1024;          // 64KB
    std::chrono::seconds keepAliveTimeout{30};
    std::chrono::seconds requestTimeout{30};
    bool enableCompression = true;
    bool enableKeepAlive = true;
    bool enableHttp2 = false;
    std::string sslCertFile;
    std::string sslKeyFile;
    bool enableAccessLog = true;
    std::string accessLogFormat = "%h %l %u %t \"%r\" %>s %b";
};

// 接続統計
struct ConnectionStats {
    std::atomic<size_t> totalConnections{0};
    std::atomic<size_t> activeConnections{0};
    std::atomic<size_t> totalRequests{0};
    std::atomic<size_t> totalResponses{0};
    std::atomic<size_t> bytesReceived{0};
    std::atomic<size_t> bytesSent{0};
    std::atomic<size_t> errorCount{0};
};

// HTTPサーバークラス
class HttpServer {
public:
    explicit HttpServer(const HttpServerConfig& config = HttpServerConfig{});
    ~HttpServer();
    
    // サーバー制御
    bool Start();
    void Stop();
    bool IsRunning() const { return running_; }
    
    // ルート登録
    void Get(const std::string& pattern, HttpHandler handler);
    void Post(const std::string& pattern, HttpHandler handler);
    void Put(const std::string& pattern, HttpHandler handler);
    void Delete(const std::string& pattern, HttpHandler handler);
    void Route(HttpMethod method, const std::string& pattern, HttpHandler handler);
    
    // ミドルウェア
    void Use(HttpMiddleware middleware);
    void Use(const std::string& path, HttpMiddleware middleware);
    
    // 静的ファイル配信
    void ServeStatic(const std::string& path, const std::string& root);
    
    // エラーハンドラ
    void SetErrorHandler(std::function<void(HttpStatus, const HttpRequest&, HttpResponse&)> handler);
    
    // 設定
    void SetConfig(const HttpServerConfig& config) { config_ = config; }
    const HttpServerConfig& GetConfig() const { return config_; }
    
    // 統計情報
    const ConnectionStats& GetStats() const { return stats_; }
    void ResetStats();
    
    // SSL/TLS
    bool EnableSSL(const std::string& certFile, const std::string& keyFile);
    void DisableSSL();
    
    // WebSocket アップグレード
    void SetWebSocketHandler(std::function<void(int socket)> handler);

private:
    // 内部構造
    struct Connection;
    struct SocketData;
    
    // サーバー処理
    void AcceptLoop();
    void WorkerThread();
    void HandleConnection(std::shared_ptr<Connection> conn);
    void ProcessRequest(std::shared_ptr<Connection> conn);
    
    // リクエスト解析
    bool ParseRequest(const std::string& data, HttpRequest& request);
    bool ParseRequestLine(const std::string& line, HttpRequest& request);
    bool ParseHeaders(const std::string& headers, HttpRequest& request);
    
    // レスポンス生成
    std::string GenerateResponse(const HttpResponse& response);
    std::string CompressBody(const std::string& body, CompressionType type);
    
    // ルーティング
    RouteInfo* FindRoute(HttpMethod method, const std::string& path);
    bool MatchPattern(const std::string& pattern, const std::string& path);
    
    // ミドルウェア処理
    bool ProcessMiddlewares(const HttpRequest& request, HttpResponse& response);
    
    // ファイル処理
    bool ServeFile(const std::string& filepath, HttpResponse& response);
    std::string GetMimeType(const std::string& filepath);
    
    // ログ
    void LogAccess(const HttpRequest& request, const HttpResponse& response);
    void LogError(const std::string& message);
    
    // ユーティリティ
    std::string UrlDecode(const std::string& str);
    std::string GetStatusText(HttpStatus status);
    
    // SSL関連
    bool InitializeSSL();
    void CleanupSSL();

private:
    HttpServerConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> shouldStop_{false};
    
    // ネットワーク
    int serverSocket_ = -1;
    std::unique_ptr<std::thread> acceptThread_;
    std::vector<std::unique_ptr<std::thread>> workerThreads_;
    
    // ルーティング
    std::vector<RouteInfo> routes_;
    std::vector<HttpMiddleware> globalMiddlewares_;
    std::unordered_map<std::string, std::string> staticPaths_;
    
    // エラーハンドリング
    std::function<void(HttpStatus, const HttpRequest&, HttpResponse&)> errorHandler_;
    
    // 統計
    ConnectionStats stats_;
    
    // 接続管理
    std::queue<std::shared_ptr<Connection>> connectionQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    
    // SSL/TLS
    void* sslContext_ = nullptr;  // SSL_CTX*
    bool sslEnabled_ = false;
    
    // WebSocket
    std::function<void(int)> webSocketHandler_;
    
    // MIME タイプマッピング
    static const std::unordered_map<std::string, std::string> mimeTypes_;
    
    // アクセスログ
    mutable std::mutex logMutex_;
    std::ofstream accessLogFile_;
};

// HTTPクライアント（簡易版）
class HttpClient {
public:
    HttpClient();
    ~HttpClient();
    
    // リクエスト送信
    HttpResponse Get(const std::string& url);
    HttpResponse Post(const std::string& url, const std::string& body);
    HttpResponse Put(const std::string& url, const std::string& body);
    HttpResponse Delete(const std::string& url);
    
    // カスタムリクエスト
    HttpResponse Request(HttpMethod method, const std::string& url, 
                        const HttpHeaders& headers = HttpHeaders{},
                        const std::string& body = "");
    
    // 設定
    void SetTimeout(std::chrono::seconds timeout) { timeout_ = timeout; }
    void SetUserAgent(const std::string& userAgent) { userAgent_ = userAgent; }
    void SetFollowRedirects(bool follow) { followRedirects_ = follow; }

private:
    std::chrono::seconds timeout_{30};
    std::string userAgent_ = "AeroJS HttpClient/1.0";
    bool followRedirects_ = true;
};

// ヘルパー関数
std::string HttpMethodToString(HttpMethod method);
HttpMethod StringToHttpMethod(const std::string& method);
std::string HttpStatusToString(HttpStatus status);

// URL操作
class UrlParser {
public:
    static bool Parse(const std::string& url, std::string& scheme, 
                     std::string& host, uint16_t& port, std::string& path);
    static std::string Encode(const std::string& str);
    static std::string Decode(const std::string& str);
};

} // namespace network
} // namespace core
} // namespace aerojs

#endif // AEROJS_HTTP_SERVER_H 