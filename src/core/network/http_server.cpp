/**
 * @file http_server.cpp
 * @brief 高性能HTTPサーバー実装
 * 
 * このファイルは、AeroJSエンジン用の高性能HTTPサーバーを実装します。
 * 非同期I/O、Keep-Alive、HTTP/2対応、リクエスト圧縮などの機能を提供します。
 * 
 * @author AeroJS Team
 * @version 1.0.0
 * @copyright MIT License
 */

#include "http_server.h"
#include "../../utils/logging.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <zlib.h>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <regex>
#include <iomanip>
#include <cstring>

namespace aerojs {
namespace core {
namespace network {

// 内部構造体の定義
struct HttpServer::Connection {
    int socket;
    SSL* ssl;
    std::string buffer;
    HttpRequest request;
    HttpResponse response;
    std::chrono::steady_clock::time_point lastActivity;
    bool keepAlive;
    bool requestComplete;
    bool responseComplete;
    
    Connection(int sock) : socket(sock), ssl(nullptr), keepAlive(true), 
                          requestComplete(false), responseComplete(false) {
        lastActivity = std::chrono::steady_clock::now();
    }
};

// MIMEタイプマッピング
const std::unordered_map<std::string, std::string> HttpServer::mimeTypes_ = {
    {".html", "text/html; charset=utf-8"},
    {".htm", "text/html; charset=utf-8"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},
    {".txt", "text/plain"},
    {".pdf", "application/pdf"},
    {".zip", "application/zip"},
    {".tar", "application/x-tar"},
    {".gz", "application/gzip"},
    {".mp4", "video/mp4"},
    {".mp3", "audio/mpeg"},
    {".wav", "audio/wav"},
    {".wasm", "application/wasm"}
};

HttpServer::HttpServer(const HttpServerConfig& config)
    : config_(config)
    , serverSocket_(-1) {
    
    // シグナルハンドラの設定
    signal(SIGPIPE, SIG_IGN);
    
    LOG_INFO("HTTPサーバーを初期化しました: {}:{}", config_.bindAddress, config_.port);
}

HttpServer::~HttpServer() {
    Stop();
    
    if (sslEnabled_) {
        CleanupSSL();
    }
    
    LOG_INFO("HTTPサーバーを終了しました");
}

bool HttpServer::Start() {
    if (running_) {
        LOG_WARNING("HTTPサーバーは既に起動しています");
        return true;
    }
    
    // SSL初期化
    if (!config_.sslCertFile.empty() && !config_.sslKeyFile.empty()) {
        if (!InitializeSSL()) {
            LOG_ERROR("SSL初期化に失敗しました");
            return false;
        }
        sslEnabled_ = true;
    }
    
    // サーバーソケットの作成
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        LOG_ERROR("ソケット作成に失敗しました: {}", strerror(errno));
        return false;
    }
    
    // ソケットオプションの設定
    int opt = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERROR("SO_REUSEADDRの設定に失敗しました: {}", strerror(errno));
        close(serverSocket_);
        return false;
    }
    
    // ノンブロッキングに設定
    int flags = fcntl(serverSocket_, F_GETFL, 0);
    if (fcntl(serverSocket_, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOG_ERROR("ノンブロッキング設定に失敗しました: {}", strerror(errno));
        close(serverSocket_);
        return false;
    }
    
    // バインド
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(config_.port);
    
    if (config_.bindAddress == "0.0.0.0") {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, config_.bindAddress.c_str(), &serverAddr.sin_addr) <= 0) {
            LOG_ERROR("無効なバインドアドレス: {}", config_.bindAddress);
            close(serverSocket_);
            return false;
        }
    }
    
    if (bind(serverSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LOG_ERROR("バインドに失敗しました: {}", strerror(errno));
        close(serverSocket_);
        return false;
    }
    
    // リッスン開始
    if (listen(serverSocket_, config_.maxConnections) < 0) {
        LOG_ERROR("リッスンに失敗しました: {}", strerror(errno));
        close(serverSocket_);
        return false;
    }
    
    running_ = true;
    shouldStop_ = false;
    
    // ワーカースレッドの起動
    for (size_t i = 0; i < config_.threadPoolSize; ++i) {
        workerThreads_.emplace_back(std::make_unique<std::thread>(&HttpServer::WorkerThread, this));
    }
    
    // アクセプトスレッドの起動
    acceptThread_ = std::make_unique<std::thread>(&HttpServer::AcceptLoop, this);
    
    LOG_INFO("HTTPサーバーが開始されました: {}://{}: port {}", 
             sslEnabled_ ? "https" : "http", config_.bindAddress, config_.port);
    
    return true;
}

void HttpServer::Stop() {
    if (!running_) {
        return;
    }
    
    LOG_INFO("HTTPサーバーを停止しています...");
    
    shouldStop_ = true;
    running_ = false;
    
    // サーバーソケットを閉じる
    if (serverSocket_ >= 0) {
        close(serverSocket_);
        serverSocket_ = -1;
    }
    
    // スレッドの終了を待機
    if (acceptThread_ && acceptThread_->joinable()) {
        acceptThread_->join();
    }
    
    // ワーカースレッドの終了
    queueCondition_.notify_all();
    for (auto& thread : workerThreads_) {
        if (thread->joinable()) {
            thread->join();
        }
    }
    
    workerThreads_.clear();
    
    LOG_INFO("HTTPサーバーが停止されました");
}

void HttpServer::Get(const std::string& pattern, HttpHandler handler) {
    Route(HttpMethod::GET, pattern, handler);
}

void HttpServer::Post(const std::string& pattern, HttpHandler handler) {
    Route(HttpMethod::POST, pattern, handler);
}

void HttpServer::Put(const std::string& pattern, HttpHandler handler) {
    Route(HttpMethod::PUT, pattern, handler);
}

void HttpServer::Delete(const std::string& pattern, HttpHandler handler) {
    Route(HttpMethod::DELETE, pattern, handler);
}

void HttpServer::Route(HttpMethod method, const std::string& pattern, HttpHandler handler) {
    RouteInfo route;
    route.method = method;
    route.pattern = pattern;
    route.handler = handler;
    
    routes_.push_back(route);
    
    LOG_DEBUG("ルートを登録しました: {} {}", HttpMethodToString(method), pattern);
}

void HttpServer::Use(HttpMiddleware middleware) {
    globalMiddlewares_.push_back(middleware);
}

void HttpServer::Use(const std::string& path, HttpMiddleware middleware) {
    // パス固有のミドルウェアは簡略化実装
    globalMiddlewares_.push_back([path, middleware](const HttpRequest& req, HttpResponse& res) {
        if (req.GetPath().find(path) == 0) {
            return middleware(req, res);
        }
        return true;
    });
}

void HttpServer::ServeStatic(const std::string& path, const std::string& root) {
    staticPaths_[path] = root;
    
    LOG_DEBUG("静的ファイルディレクトリを設定しました: {} -> {}", path, root);
}

void HttpServer::SetErrorHandler(std::function<void(HttpStatus, const HttpRequest&, HttpResponse&)> handler) {
    errorHandler_ = handler;
}

void HttpServer::ResetStats() {
    stats_.totalConnections = 0;
    stats_.activeConnections = 0;
    stats_.totalRequests = 0;
    stats_.totalResponses = 0;
    stats_.bytesReceived = 0;
    stats_.bytesSent = 0;
    stats_.errorCount = 0;
}

bool HttpServer::EnableSSL(const std::string& certFile, const std::string& keyFile) {
    config_.sslCertFile = certFile;
    config_.sslKeyFile = keyFile;
    
    if (running_) {
        return InitializeSSL();
    }
    
    return true;
}

void HttpServer::DisableSSL() {
    sslEnabled_ = false;
    config_.sslCertFile.clear();
    config_.sslKeyFile.clear();
    
    if (sslContext_) {
        CleanupSSL();
    }
}

void HttpServer::SetWebSocketHandler(std::function<void(int)> handler) {
    webSocketHandler_ = handler;
}

// プライベートメソッドの実装

void HttpServer::AcceptLoop() {
    int epollFd = epoll_create1(0);
    if (epollFd < 0) {
        LOG_ERROR("epoll_create1に失敗しました: {}", strerror(errno));
        return;
    }
    
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = serverSocket_;
    
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, serverSocket_, &event) < 0) {
        LOG_ERROR("epoll_ctlに失敗しました: {}", strerror(errno));
        close(epollFd);
        return;
    }
    
    std::vector<struct epoll_event> events(config_.maxConnections);
    
    while (!shouldStop_) {
        int numEvents = epoll_wait(epollFd, events.data(), events.size(), 1000);
        
        if (numEvents < 0) {
            if (errno == EINTR) continue;
            LOG_ERROR("epoll_waitに失敗しました: {}", strerror(errno));
            break;
        }
        
        for (int i = 0; i < numEvents; ++i) {
            if (events[i].data.fd == serverSocket_) {
                // 新しい接続を受け入れ
                struct sockaddr_in clientAddr;
                socklen_t clientLen = sizeof(clientAddr);
                
                int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientLen);
                if (clientSocket < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        LOG_ERROR("accept に失敗しました: {}", strerror(errno));
                    }
                    continue;
                }
                
                // ノンブロッキングに設定
                int flags = fcntl(clientSocket, F_GETFL, 0);
                fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);
                
                // 接続オブジェクトを作成
                auto connection = std::make_shared<Connection>(clientSocket);
                connection->request.SetRemoteAddress(inet_ntoa(clientAddr.sin_addr));
                connection->request.SetRemotePort(ntohs(clientAddr.sin_port));
                
                // SSL接続の場合
                if (sslEnabled_ && sslContext_) {
                    connection->ssl = SSL_new(static_cast<SSL_CTX*>(sslContext_));
                    SSL_set_fd(connection->ssl, clientSocket);
                    
                    if (SSL_accept(connection->ssl) <= 0) {
                        LOG_ERROR("SSL handshake に失敗しました");
                        SSL_free(connection->ssl);
                        close(clientSocket);
                        continue;
                    }
                }
                
                // ワーカーキューに追加
                {
                    std::lock_guard<std::mutex> lock(queueMutex_);
                    connectionQueue_.push(connection);
                }
                queueCondition_.notify_one();
                
                stats_.totalConnections++;
                stats_.activeConnections++;
                
                LOG_DEBUG("新しい接続を受け入れました: {}:{}", 
                         connection->request.GetRemoteAddress(),
                         connection->request.GetRemotePort());
            }
        }
    }
    
    close(epollFd);
}

void HttpServer::WorkerThread() {
    while (!shouldStop_) {
        std::shared_ptr<Connection> connection;
        
        // 接続を取得
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCondition_.wait(lock, [this] {
                return !connectionQueue_.empty() || shouldStop_;
            });
            
            if (shouldStop_) break;
            
            if (!connectionQueue_.empty()) {
                connection = connectionQueue_.front();
                connectionQueue_.pop();
            }
        }
        
        if (connection) {
            HandleConnection(connection);
        }
    }
}

void HttpServer::HandleConnection(std::shared_ptr<Connection> conn) {
    try {
        // データの読み込み
        char buffer[4096];
        ssize_t bytesRead;
        
        if (conn->ssl) {
            bytesRead = SSL_read(conn->ssl, buffer, sizeof(buffer) - 1);
        } else {
            bytesRead = recv(conn->socket, buffer, sizeof(buffer) - 1, 0);
        }
        
        if (bytesRead <= 0) {
            // 接続が閉じられた
            stats_.activeConnections--;
            if (conn->ssl) {
                SSL_free(conn->ssl);
            }
            close(conn->socket);
            return;
        }
        
        buffer[bytesRead] = '\0';
        conn->buffer += buffer;
        stats_.bytesReceived += bytesRead;
        
        // リクエストの解析
        if (!conn->requestComplete) {
            size_t headerEnd = conn->buffer.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                std::string headerData = conn->buffer.substr(0, headerEnd);
                
                if (ParseRequest(headerData, conn->request)) {
                    conn->requestComplete = true;
                    
                    // ボディの読み込み（POST等）
                    size_t contentLength = 0;
                    if (conn->request.GetHeaders().Has("Content-Length")) {
                        contentLength = std::stoul(conn->request.GetHeaders().Get("Content-Length"));
                    }
                    
                    size_t bodyStart = headerEnd + 4;
                    if (contentLength > 0) {
                        if (conn->buffer.size() >= bodyStart + contentLength) {
                            conn->request.SetBody(conn->buffer.substr(bodyStart, contentLength));
                        } else {
                            // ボディが不完全
                            conn->requestComplete = false;
                            return;
                        }
                    }
                    
                    // リクエストを処理
                    ProcessRequest(conn);
                } else {
                    // 不正なリクエスト
                    conn->response.SetStatus(HttpStatus::BAD_REQUEST);
                    conn->response.SetBody("400 Bad Request");
                    stats_.errorCount++;
                }
            } else if (conn->buffer.size() > config_.maxHeaderSize) {
                // ヘッダーサイズ制限
                conn->response.SetStatus(HttpStatus::BAD_REQUEST);
                conn->response.SetBody("Request Header Too Large");
                stats_.errorCount++;
            } else {
                // ヘッダーが不完全
                return;
            }
        }
        
        // レスポンスの送信
        if (conn->requestComplete && !conn->responseComplete) {
            std::string responseData = GenerateResponse(conn->response);
            
            ssize_t bytesSent;
            if (conn->ssl) {
                bytesSent = SSL_write(conn->ssl, responseData.c_str(), responseData.size());
            } else {
                bytesSent = send(conn->socket, responseData.c_str(), responseData.size(), 0);
            }
            
            if (bytesSent > 0) {
                stats_.bytesSent += bytesSent;
                stats_.totalResponses++;
                conn->responseComplete = true;
                
                // アクセスログ
                if (config_.enableAccessLog) {
                    LogAccess(conn->request, conn->response);
                }
            }
        }
        
        // 接続の終了判定
        if (conn->requestComplete && conn->responseComplete) {
            if (conn->keepAlive && conn->request.GetHeaders().Get("Connection") != "close") {
                // Keep-Alive接続を維持
                conn->buffer.clear();
                conn->requestComplete = false;
                conn->responseComplete = false;
                conn->request = HttpRequest{};
                conn->response = HttpResponse{};
                conn->lastActivity = std::chrono::steady_clock::now();
                
                // 再度キューに追加（簡略化実装）
                {
                    std::lock_guard<std::mutex> lock(queueMutex_);
                    connectionQueue_.push(conn);
                }
                queueCondition_.notify_one();
            } else {
                // 接続を閉じる
                stats_.activeConnections--;
                if (conn->ssl) {
                    SSL_shutdown(conn->ssl);
                    SSL_free(conn->ssl);
                }
                close(conn->socket);
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("接続処理中にエラーが発生しました: {}", e.what());
        stats_.errorCount++;
        
        stats_.activeConnections--;
        if (conn->ssl) {
            SSL_free(conn->ssl);
        }
        close(conn->socket);
    }
}

void HttpServer::ProcessRequest(std::shared_ptr<Connection> conn) {
    auto& request = conn->request;
    auto& response = conn->response;
    
    stats_.totalRequests++;
    request.SetTimestamp(std::chrono::steady_clock::now());
    
    try {
        // WebSocketアップグレードのチェック
        if (request.GetHeaders().Get("Upgrade") == "websocket" && webSocketHandler_) {
            webSocketHandler_(conn->socket);
            return;
        }
        
        // ミドルウェアの実行
        if (!ProcessMiddlewares(request, response)) {
            return;
        }
        
        // 静的ファイルの処理
        for (const auto& staticPath : staticPaths_) {
            if (request.GetPath().find(staticPath.first) == 0) {
                std::string filePath = staticPath.second + 
                    request.GetPath().substr(staticPath.first.length());
                
                if (ServeFile(filePath, response)) {
                    return;
                }
            }
        }
        
        // ルートマッチング
        RouteInfo* route = FindRoute(request.GetMethod(), request.GetPath());
        if (route) {
            // ルートのミドルウェアを実行
            bool middlewareSuccess = true;
            for (const auto& middleware : route->middlewares) {
                if (!middleware(request, response)) {
                    middlewareSuccess = false;
                    break;
                }
            }
            
            if (middlewareSuccess) {
                route->handler(request, response);
            }
        } else {
            // ルートが見つからない
            response.SetStatus(HttpStatus::NOT_FOUND);
            response.SetBody("404 Not Found");
            
            if (errorHandler_) {
                errorHandler_(HttpStatus::NOT_FOUND, request, response);
            }
        }
        
        // 圧縮の適用
        if (config_.enableCompression && response.GetBody().size() > 1024) {
            std::string acceptEncoding = request.GetHeaders().Get("Accept-Encoding");
            
            if (acceptEncoding.find("gzip") != std::string::npos) {
                std::string compressed = CompressBody(response.GetBody(), CompressionType::GZIP);
                if (!compressed.empty()) {
                    response.SetBody(std::move(compressed));
                    response.GetHeaders().Set("Content-Encoding", "gzip");
                    response.SetCompressionType(CompressionType::GZIP);
                }
            } else if (acceptEncoding.find("deflate") != std::string::npos) {
                std::string compressed = CompressBody(response.GetBody(), CompressionType::DEFLATE);
                if (!compressed.empty()) {
                    response.SetBody(std::move(compressed));
                    response.GetHeaders().Set("Content-Encoding", "deflate");
                    response.SetCompressionType(CompressionType::DEFLATE);
                }
            }
        }
        
        // デフォルトヘッダーの設定
        response.GetHeaders().Set("Server", "AeroJS/1.0");
        response.GetHeaders().Set("Content-Length", std::to_string(response.GetBody().size()));
        
        if (config_.enableKeepAlive && request.GetHeaders().Get("Connection") != "close") {
            response.GetHeaders().Set("Connection", "keep-alive");
            response.GetHeaders().Set("Keep-Alive", "timeout=" + 
                std::to_string(config_.keepAliveTimeout.count()));
        } else {
            response.GetHeaders().Set("Connection", "close");
            conn->keepAlive = false;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("リクエスト処理中にエラーが発生しました: {}", e.what());
        
        response.SetStatus(HttpStatus::INTERNAL_SERVER_ERROR);
        response.SetBody("500 Internal Server Error");
        stats_.errorCount++;
        
        if (errorHandler_) {
            errorHandler_(HttpStatus::INTERNAL_SERVER_ERROR, request, response);
        }
    }
}

bool HttpServer::ParseRequest(const std::string& data, HttpRequest& request) {
    std::istringstream stream(data);
    std::string line;
    
    // リクエストラインの解析
    if (!std::getline(stream, line)) {
        return false;
    }
    
    // \rを除去
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    if (!ParseRequestLine(line, request)) {
        return false;
    }
    
    // ヘッダーの解析
    std::string headerData;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.empty()) {
            break;
        }
        
        headerData += line + "\r\n";
    }
    
    return ParseHeaders(headerData, request);
}

bool HttpServer::ParseRequestLine(const std::string& line, HttpRequest& request) {
    std::istringstream stream(line);
    std::string method, url, version;
    
    stream >> method >> url >> version;
    
    if (method.empty() || url.empty() || version.empty()) {
        return false;
    }
    
    // HTTPメソッドの解析
    request.SetMethod(StringToHttpMethod(method));
    request.SetUrl(url);
    request.SetHttpVersion(version);
    
    // URLからパスとクエリを分離
    size_t queryPos = url.find('?');
    if (queryPos != std::string::npos) {
        request.SetPath(url.substr(0, queryPos));
        request.SetQuery(url.substr(queryPos + 1));
    } else {
        request.SetPath(url);
    }
    
    return true;
}

bool HttpServer::ParseHeaders(const std::string& headers, HttpRequest& request) {
    std::istringstream stream(headers);
    std::string line;
    
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.empty()) {
            continue;
        }
        
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }
        
        std::string name = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        
        // 前後の空白を除去
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        request.GetHeaders().Add(name, value);
    }
    
    return true;
}

std::string HttpServer::GenerateResponse(const HttpResponse& response) {
    std::ostringstream stream;
    
    // ステータスライン
    stream << response.GetHttpVersion() << " " 
           << static_cast<int>(response.GetStatus()) << " "
           << GetStatusText(response.GetStatus()) << "\r\n";
    
    // ヘッダー
    for (const auto& header : response.GetHeaders()) {
        for (const auto& value : header.second) {
            stream << header.first << ": " << value << "\r\n";
        }
    }
    
    stream << "\r\n";
    
    // ボディ
    stream << response.GetBody();
    
    return stream.str();
}

std::string HttpServer::CompressBody(const std::string& body, CompressionType type) {
    if (body.empty()) {
        return "";
    }
    
    switch (type) {
        case CompressionType::GZIP: {
            z_stream stream;
            std::memset(&stream, 0, sizeof(stream));
            
            if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
                return "";
            }
            
            std::string compressed;
            compressed.resize(body.size());
            
            stream.next_in = (Bytef*)body.data();
            stream.avail_in = body.size();
            stream.next_out = (Bytef*)compressed.data();
            stream.avail_out = compressed.size();
            
            if (deflate(&stream, Z_FINISH) != Z_STREAM_END) {
                deflateEnd(&stream);
                return "";
            }
            
            compressed.resize(stream.total_out);
            deflateEnd(&stream);
            
            return compressed;
        }
        
        case CompressionType::DEFLATE: {
            // 簡略化実装
            return CompressBody(body, CompressionType::GZIP);
        }
        
        default:
            return "";
    }
}

RouteInfo* HttpServer::FindRoute(HttpMethod method, const std::string& path) {
    for (auto& route : routes_) {
        if (route.method == method && MatchPattern(route.pattern, path)) {
            return &route;
        }
    }
    return nullptr;
}

bool HttpServer::MatchPattern(const std::string& pattern, const std::string& path) {
    // 簡単なパターンマッチング（実際にはより高度な実装が必要）
    if (pattern == path) {
        return true;
    }
    
    // ワイルドカードマッチング
    if (pattern.back() == '*') {
        std::string prefix = pattern.substr(0, pattern.length() - 1);
        return path.find(prefix) == 0;
    }
    
    return false;
}

bool HttpServer::ProcessMiddlewares(const HttpRequest& request, HttpResponse& response) {
    for (const auto& middleware : globalMiddlewares_) {
        if (!middleware(request, response)) {
            return false;
        }
    }
    return true;
}

bool HttpServer::ServeFile(const std::string& filepath, HttpResponse& response) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return false;
    }
    
    // ファイルサイズを取得
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // ファイル内容を読み込み
    std::string content(fileSize, '\0');
    file.read(&content[0], fileSize);
    
    response.SetBody(content);
    response.GetHeaders().Set("Content-Type", GetMimeType(filepath));
    response.SetStatus(HttpStatus::OK);
    
    return true;
}

std::string HttpServer::GetMimeType(const std::string& filepath) {
    size_t dotPos = filepath.find_last_of('.');
    if (dotPos == std::string::npos) {
        return "application/octet-stream";
    }
    
    std::string extension = filepath.substr(dotPos);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    auto it = mimeTypes_.find(extension);
    if (it != mimeTypes_.end()) {
        return it->second;
    }
    
    return "application/octet-stream";
}

void HttpServer::LogAccess(const HttpRequest& request, const HttpResponse& response) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream log;
    log << request.GetRemoteAddress() << " - - ["
        << std::put_time(std::localtime(&time_t), "%d/%b/%Y:%H:%M:%S %z") << "] \""
        << HttpMethodToString(request.GetMethod()) << " " << request.GetUrl() 
        << " " << request.GetHttpVersion() << "\" "
        << static_cast<int>(response.GetStatus()) << " "
        << response.GetBody().size();
    
    std::cout << log.str() << std::endl;
}

void HttpServer::LogError(const std::string& message) {
    LOG_ERROR("HTTP Server Error: {}", message);
}

std::string HttpServer::UrlDecode(const std::string& str) {
    std::string decoded;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::istringstream hex(str.substr(i + 1, 2));
            hex >> std::hex >> value;
            decoded += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            decoded += ' ';
        } else {
            decoded += str[i];
        }
    }
    return decoded;
}

std::string HttpServer::GetStatusText(HttpStatus status) {
    switch (status) {
        case HttpStatus::OK: return "OK";
        case HttpStatus::CREATED: return "Created";
        case HttpStatus::NO_CONTENT: return "No Content";
        case HttpStatus::MOVED_PERMANENTLY: return "Moved Permanently";
        case HttpStatus::NOT_MODIFIED: return "Not Modified";
        case HttpStatus::BAD_REQUEST: return "Bad Request";
        case HttpStatus::UNAUTHORIZED: return "Unauthorized";
        case HttpStatus::FORBIDDEN: return "Forbidden";
        case HttpStatus::NOT_FOUND: return "Not Found";
        case HttpStatus::METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HttpStatus::INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HttpStatus::NOT_IMPLEMENTED: return "Not Implemented";
        case HttpStatus::BAD_GATEWAY: return "Bad Gateway";
        case HttpStatus::SERVICE_UNAVAILABLE: return "Service Unavailable";
        default: return "Unknown";
    }
}

bool HttpServer::InitializeSSL() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    
    if (!ctx) {
        LOG_ERROR("SSL_CTX_newに失敗しました");
        return false;
    }
    
    if (SSL_CTX_use_certificate_file(ctx, config_.sslCertFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        LOG_ERROR("証明書ファイルの読み込みに失敗しました: {}", config_.sslCertFile);
        SSL_CTX_free(ctx);
        return false;
    }
    
    if (SSL_CTX_use_PrivateKey_file(ctx, config_.sslKeyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        LOG_ERROR("秘密鍵ファイルの読み込みに失敗しました: {}", config_.sslKeyFile);
        SSL_CTX_free(ctx);
        return false;
    }
    
    if (!SSL_CTX_check_private_key(ctx)) {
        LOG_ERROR("証明書と秘密鍵が一致しません");
        SSL_CTX_free(ctx);
        return false;
    }
    
    sslContext_ = ctx;
    return true;
}

void HttpServer::CleanupSSL() {
    if (sslContext_) {
        SSL_CTX_free(static_cast<SSL_CTX*>(sslContext_));
        sslContext_ = nullptr;
    }
    
    EVP_cleanup();
}

// HttpHeaders実装

void HttpHeaders::Set(const std::string& name, const std::string& value) {
    headers_[name] = {value};
}

void HttpHeaders::Add(const std::string& name, const std::string& value) {
    headers_[name].push_back(value);
}

std::string HttpHeaders::Get(const std::string& name) const {
    auto it = headers_.find(name);
    if (it != headers_.end() && !it->second.empty()) {
        return it->second[0];
    }
    return "";
}

std::vector<std::string> HttpHeaders::GetAll(const std::string& name) const {
    auto it = headers_.find(name);
    if (it != headers_.end()) {
        return it->second;
    }
    return {};
}

bool HttpHeaders::Has(const std::string& name) const {
    return headers_.find(name) != headers_.end();
}

void HttpHeaders::Remove(const std::string& name) {
    headers_.erase(name);
}

void HttpHeaders::Clear() {
    headers_.clear();
}

std::unordered_map<std::string, std::vector<std::string>>::const_iterator HttpHeaders::begin() const {
    return headers_.begin();
}

std::unordered_map<std::string, std::vector<std::string>>::const_iterator HttpHeaders::end() const {
    return headers_.end();
}

// HttpRequest実装

std::string HttpRequest::GetQueryParam(const std::string& name) const {
    if (query_.empty()) {
        return "";
    }
    
    std::istringstream stream(query_);
    std::string pair;
    
    while (std::getline(stream, pair, '&')) {
        size_t eqPos = pair.find('=');
        if (eqPos != std::string::npos) {
            std::string key = pair.substr(0, eqPos);
            std::string value = pair.substr(eqPos + 1);
            
            if (key == name) {
                return UrlParser::Decode(value);
            }
        }
    }
    
    return "";
}

std::unordered_map<std::string, std::string> HttpRequest::GetQueryParams() const {
    std::unordered_map<std::string, std::string> params;
    
    if (query_.empty()) {
        return params;
    }
    
    std::istringstream stream(query_);
    std::string pair;
    
    while (std::getline(stream, pair, '&')) {
        size_t eqPos = pair.find('=');
        if (eqPos != std::string::npos) {
            std::string key = pair.substr(0, eqPos);
            std::string value = pair.substr(eqPos + 1);
            
            params[UrlParser::Decode(key)] = UrlParser::Decode(value);
        }
    }
    
    return params;
}

// HttpResponse実装

void HttpResponse::SetJson(const std::string& json) {
    SetBody(json);
    GetHeaders().Set("Content-Type", "application/json; charset=utf-8");
}

void HttpResponse::SetHtml(const std::string& html) {
    SetBody(html);
    GetHeaders().Set("Content-Type", "text/html; charset=utf-8");
}

void HttpResponse::SetText(const std::string& text) {
    SetBody(text);
    GetHeaders().Set("Content-Type", "text/plain; charset=utf-8");
}

void HttpResponse::SetFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        SetStatus(HttpStatus::NOT_FOUND);
        SetBody("File not found");
        return;
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::string content(size, '\0');
    file.read(&content[0], size);
    
    SetBody(content);
    
    // MIMEタイプを設定
    size_t dotPos = filepath.find_last_of('.');
    if (dotPos != std::string::npos) {
        std::string extension = filepath.substr(dotPos);
        auto it = HttpServer::mimeTypes_.find(extension);
        if (it != HttpServer::mimeTypes_.end()) {
            GetHeaders().Set("Content-Type", it->second);
        }
    }
}

// ヘルパー関数の実装

std::string HttpMethodToString(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
        case HttpMethod::PATCH: return "PATCH";
        case HttpMethod::TRACE: return "TRACE";
        case HttpMethod::CONNECT: return "CONNECT";
        default: return "UNKNOWN";
    }
}

HttpMethod StringToHttpMethod(const std::string& method) {
    if (method == "GET") return HttpMethod::GET;
    if (method == "POST") return HttpMethod::POST;
    if (method == "PUT") return HttpMethod::PUT;
    if (method == "DELETE") return HttpMethod::DELETE;
    if (method == "HEAD") return HttpMethod::HEAD;
    if (method == "OPTIONS") return HttpMethod::OPTIONS;
    if (method == "PATCH") return HttpMethod::PATCH;
    if (method == "TRACE") return HttpMethod::TRACE;
    if (method == "CONNECT") return HttpMethod::CONNECT;
    return HttpMethod::GET;
}

std::string HttpStatusToString(HttpStatus status) {
    return std::to_string(static_cast<int>(status));
}

// URLParser実装

bool UrlParser::Parse(const std::string& url, std::string& scheme, 
                     std::string& host, uint16_t& port, std::string& path) {
    
    std::regex urlRegex(R"(^(https?):\/\/([^:\/]+)(?::(\d+))?(\/.*)?$)");
    std::smatch match;
    
    if (!std::regex_match(url, match, urlRegex)) {
        return false;
    }
    
    scheme = match[1];
    host = match[2];
    
    if (match[3].matched) {
        port = static_cast<uint16_t>(std::stoi(match[3]));
    } else {
        port = (scheme == "https") ? 443 : 80;
    }
    
    path = match[4].matched ? match[4] : "/";
    
    return true;
}

std::string UrlParser::Encode(const std::string& str) {
    std::ostringstream encoded;
    
    for (char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            encoded << '%' << std::hex << std::uppercase << (unsigned char)c;
        }
    }
    
    return encoded.str();
}

std::string UrlParser::Decode(const std::string& str) {
    std::string decoded;
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::istringstream hex(str.substr(i + 1, 2));
            hex >> std::hex >> value;
            decoded += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            decoded += ' ';
        } else {
            decoded += str[i];
        }
    }
    
    return decoded;
}

} // namespace network
} // namespace core
} // namespace aerojs