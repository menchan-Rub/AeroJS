/**
 * @file main.cpp
 * @brief AeroJS JavaScriptエンジンのメインエントリーポイント
 * 
 * このファイルは、AeroJSエンジンの実行可能ファイルのメインエントリーポイントです。
 * コマンドライン引数の解析、エンジンの初期化、スクリプトの実行を行います。
 * 
 * @author AeroJS Team
 * @version 1.0.0
 * @copyright MIT License
 */

#include "core/engine.h"
#include "core/context.h"
#include "core/network/http_server.h"
#include "utils/memory/gc/incremental_gc.h"
#include "utils/logging.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>

namespace aerojs {

struct CommandLineArgs {
    std::string scriptFile;
    std::string module;
    bool isRepl = false;
    bool enableOptimization = true;
    bool enableProfiling = false;
    bool enableGCStats = false;
    bool enableHttpServer = false;
    std::string httpHost = "localhost";
    uint16_t httpPort = 8080;
    size_t heapSize = 64 * 1024 * 1024;  // 64MB
    std::string logLevel = "INFO";
    bool showHelp = false;
    bool showVersion = false;
    std::vector<std::string> scriptArgs;
};

void PrintUsage(const char* programName) {
    std::cout << "AeroJS - 世界最高性能のJavaScriptエンジン\n\n";
    std::cout << "使用方法:\n";
    std::cout << "  " << programName << " [オプション] [ファイル] [引数...]\n\n";
    std::cout << "オプション:\n";
    std::cout << "  -h, --help              このヘルプを表示\n";
    std::cout << "  -v, --version           バージョン情報を表示\n";
    std::cout << "  -i, --interactive       REPLモードで起動\n";
    std::cout << "  -m, --module <モジュール> モジュールとして実行\n";
    std::cout << "  --no-optimize           最適化を無効化\n";
    std::cout << "  --profile               プロファイリングを有効化\n";
    std::cout << "  --gc-stats              GC統計を表示\n";
    std::cout << "  --http-server           HTTPサーバーを起動\n";
    std::cout << "  --host <ホスト>         HTTPサーバーのホスト (デフォルト: localhost)\n";
    std::cout << "  --port <ポート>         HTTPサーバーのポート (デフォルト: 8080)\n";
    std::cout << "  --heap-size <サイズ>    ヒープサイズ (デフォルト: 64MB)\n";
    std::cout << "  --log-level <レベル>    ログレベル (DEBUG, INFO, WARNING, ERROR)\n\n";
    std::cout << "例:\n";
    std::cout << "  " << programName << " script.js              # ファイルを実行\n";
    std::cout << "  " << programName << " -i                     # REPLモード\n";
    std::cout << "  " << programName << " --http-server          # HTTPサーバー起動\n";
    std::cout << "  " << programName << " -m mymodule            # モジュール実行\n";
}

void PrintVersion() {
    std::cout << "AeroJS JavaScript Engine v1.0.0\n";
    std::cout << "Copyright (c) 2024 AeroJS Team\n";
    std::cout << "MIT License\n\n";
    std::cout << "対応アーキテクチャ:\n";
    std::cout << "- x86-64 (AVX, AVX2, AVX-512)\n";
    std::cout << "- ARM64 (Neon, SVE)\n";
    std::cout << "- RISC-V (RV64GCV)\n\n";
    std::cout << "特徴:\n";
    std::cout << "- 超高性能JITコンパイラ\n";
    std::cout << "- メタトレーシング最適化\n";
    std::cout << "- 増分ガベージコレクション\n";
    std::cout << "- 組み込みHTTPサーバー\n";
}

bool ParseCommandLine(int argc, char* argv[], CommandLineArgs& args) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            args.showHelp = true;
            return true;
        } else if (arg == "-v" || arg == "--version") {
            args.showVersion = true;
            return true;
        } else if (arg == "-i" || arg == "--interactive") {
            args.isRepl = true;
        } else if (arg == "-m" || arg == "--module") {
            if (i + 1 < argc) {
                args.module = argv[++i];
            } else {
                std::cerr << "エラー: --module にはモジュール名が必要です\n";
                return false;
            }
        } else if (arg == "--no-optimize") {
            args.enableOptimization = false;
        } else if (arg == "--profile") {
            args.enableProfiling = true;
        } else if (arg == "--gc-stats") {
            args.enableGCStats = true;
        } else if (arg == "--http-server") {
            args.enableHttpServer = true;
        } else if (arg == "--host") {
            if (i + 1 < argc) {
                args.httpHost = argv[++i];
            } else {
                std::cerr << "エラー: --host にはホスト名が必要です\n";
                return false;
            }
        } else if (arg == "--port") {
            if (i + 1 < argc) {
                args.httpPort = static_cast<uint16_t>(std::stoi(argv[++i]));
            } else {
                std::cerr << "エラー: --port にはポート番号が必要です\n";
                return false;
            }
        } else if (arg == "--heap-size") {
            if (i + 1 < argc) {
                std::string sizeStr = argv[++i];
                size_t multiplier = 1;
                if (sizeStr.back() == 'M' || sizeStr.back() == 'm') {
                    multiplier = 1024 * 1024;
                    sizeStr.pop_back();
                } else if (sizeStr.back() == 'G' || sizeStr.back() == 'g') {
                    multiplier = 1024 * 1024 * 1024;
                    sizeStr.pop_back();
                }
                args.heapSize = std::stoull(sizeStr) * multiplier;
            } else {
                std::cerr << "エラー: --heap-size にはサイズが必要です\n";
                return false;
            }
        } else if (arg == "--log-level") {
            if (i + 1 < argc) {
                args.logLevel = argv[++i];
            } else {
                std::cerr << "エラー: --log-level にはレベルが必要です\n";
                return false;
            }
        } else if (arg.front() == '-') {
            std::cerr << "エラー: 不明なオプション: " << arg << "\n";
            return false;
        } else {
            if (args.scriptFile.empty()) {
                args.scriptFile = arg;
            } else {
                args.scriptArgs.push_back(arg);
            }
        }
    }
    
    return true;
}

std::string ReadFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("ファイルを開けません: " + filename);
    }
    
    std::string content;
    std::string line;
    while (std::getline(file, line)) {
        content += line + '\n';
    }
    
    return content;
}

void RunRepl(Engine& engine) {
    std::cout << "AeroJS REPL v1.0.0\n";
    std::cout << "終了するには .exit を入力してください\n\n";
    
    std::string line;
    std::string multilineInput;
    int lineNumber = 1;
    
    while (true) {
        if (multilineInput.empty()) {
            std::cout << "aerojs:" << lineNumber << "> ";
        } else {
            std::cout << "... ";
        }
        
        if (!std::getline(std::cin, line)) {
            break;
        }
        
        if (line == ".exit") {
            break;
        } else if (line == ".help") {
            std::cout << "REPLコマンド:\n";
            std::cout << "  .exit     - 終了\n";
            std::cout << "  .help     - ヘルプ表示\n";
            std::cout << "  .clear    - 入力クリア\n";
            std::cout << "  .gc       - ガベージコレクション実行\n";
            std::cout << "  .stats    - エンジン統計表示\n";
            continue;
        } else if (line == ".clear") {
            multilineInput.clear();
            continue;
        } else if (line == ".gc") {
            engine.GetContext()->GetGarbageCollector()->Collect();
            std::cout << "ガベージコレクションを実行しました\n";
            continue;
        } else if (line == ".stats") {
            auto context = engine.GetContext();
            auto gc = context->GetGarbageCollector();
            std::cout << "エンジン統計:\n";
            std::cout << "  ヒープサイズ: " << gc->GetHeapSize() / (1024 * 1024) << " MB\n";
            std::cout << "  使用メモリ: " << gc->GetUsedMemory() / (1024 * 1024) << " MB\n";
            continue;
        }
        
        multilineInput += line + '\n';
        
        try {
            auto result = engine.Evaluate(multilineInput);
            if (result.IsException()) {
                std::cout << "エラー: " << result.ToString() << "\n";
            } else if (!result.IsUndefined()) {
                std::cout << result.ToString() << "\n";
            }
            multilineInput.clear();
            lineNumber++;
        } catch (const std::exception& e) {
            if (line.back() == '{' || line.back() == '(' || line.back() == '[') {
                // 継続入力
                continue;
            } else {
                std::cout << "エラー: " << e.what() << "\n";
                multilineInput.clear();
                lineNumber++;
            }
        }
    }
}

void SetupHttpServer(const CommandLineArgs& args, Engine& engine) {
    using namespace aerojs::core::network;
    
    HttpServerConfig config;
    config.bindAddress = args.httpHost;
    config.port = args.httpPort;
    config.enableCompression = true;
    config.enableKeepAlive = true;
    
    auto server = std::make_unique<HttpServer>(config);
    
    // JavaScriptエンジンAPIエンドポイント
    server->Post("/api/eval", [&engine](const HttpRequest& req, HttpResponse& res) {
        try {
            auto result = engine.Evaluate(req.GetBody());
            res.SetJson("{\"result\":\"" + result.ToString() + "\"}");
        } catch (const std::exception& e) {
            res.SetStatus(HttpStatus::BAD_REQUEST);
            res.SetJson("{\"error\":\"" + std::string(e.what()) + "\"}");
        }
    });
    
    // 静的ファイル配信
    server->ServeStatic("/", "./public");
    
    // 統計情報エンドポイント
    server->Get("/api/stats", [&engine](const HttpRequest& req, HttpResponse& res) {
        auto context = engine.GetContext();
        auto gc = context->GetGarbageCollector();
        
        std::string json = "{"
            "\"heapSize\":" + std::to_string(gc->GetHeapSize()) + ","
            "\"usedMemory\":" + std::to_string(gc->GetUsedMemory()) + ","
            "\"gcStats\":{}"
            "}";
        res.SetJson(json);
    });
    
    if (server->Start()) {
        LOG_INFO("HTTPサーバーが開始されました: http://{}:{}", args.httpHost, args.httpPort);
        
        // サーバーが停止されるまで待機
        std::cout << "HTTPサーバーが起動中です。Ctrl+Cで停止してください。\n";
        while (server->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else {
        LOG_ERROR("HTTPサーバーの開始に失敗しました");
    }
}

} // namespace aerojs

int main(int argc, char* argv[]) {
    using namespace aerojs;
    
    CommandLineArgs args;
    
    if (!ParseCommandLine(argc, argv, args)) {
        return 1;
    }
    
    if (args.showHelp) {
        PrintUsage(argv[0]);
        return 0;
    }
    
    if (args.showVersion) {
        PrintVersion();
        return 0;
    }
    
    // ログレベルの設定
    SetLogLevel(args.logLevel);
    
    try {
        // エンジンの初期化
        LOG_INFO("AeroJSエンジンを初期化しています...");
        
        EngineConfig engineConfig;
        engineConfig.enableOptimization = args.enableOptimization;
        engineConfig.enableProfiling = args.enableProfiling;
        engineConfig.heapSize = args.heapSize;
        
        Engine engine(engineConfig);
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        if (args.enableHttpServer) {
            SetupHttpServer(args, engine);
        } else if (args.isRepl) {
            RunRepl(engine);
        } else if (!args.module.empty()) {
            auto result = engine.LoadModule(args.module);
            if (result.IsException()) {
                std::cerr << "モジュール実行エラー: " << result.ToString() << "\n";
                return 1;
            }
        } else if (!args.scriptFile.empty()) {
            std::string script = ReadFile(args.scriptFile);
            auto result = engine.Evaluate(script, args.scriptFile);
            
            if (result.IsException()) {
                std::cerr << "スクリプト実行エラー: " << result.ToString() << "\n";
                return 1;
            }
        } else {
            // デフォルトはREPLモード
            RunRepl(engine);
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        if (args.enableGCStats) {
            auto gc = engine.GetContext()->GetGarbageCollector();
            std::cout << "\nGC統計:\n";
            std::cout << "  最終ヒープサイズ: " << gc->GetHeapSize() / (1024 * 1024) << " MB\n";
            std::cout << "  最終使用メモリ: " << gc->GetUsedMemory() / (1024 * 1024) << " MB\n";
        }
        
        if (args.enableProfiling) {
            std::cout << "\n実行時間: " << duration << " ms\n";
        }
        
        LOG_INFO("AeroJSエンジンが正常に終了しました");
        
    } catch (const std::exception& e) {
        std::cerr << "エラー: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
} 