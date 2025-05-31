/**
 * @file basic_tools.cpp
 * @brief Model Context Protocol (MCP) 基本ツールの実装
 *
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#include "basic_tools.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <stdexcept>

#include "../../runtime/context.h"
#include "../../runtime/global_object.h"
#include "../../vm/vm.h"
#include "../utils/mcp_utils.h"

namespace aero {
namespace mcp {
namespace tools {

// JSON Schema定義
namespace schemas {
// エンジン起動ツールの入力スキーマ
constexpr const char* ENGINE_START_INPUT = R"({
        "type": "object",
        "properties": {
            "options": {
                "type": "object",
                "properties": {
                    "enableJIT": {"type": "boolean"},
                    "enableGC": {"type": "boolean"},
                    "stackSize": {"type": "number"},
                    "heapSize": {"type": "number"},
                    "contextOptions": {
                        "type": "object",
                        "properties": {
                            "strictMode": {"type": "boolean"},
                            "enableConsole": {"type": "boolean"},
                            "enableModules": {"type": "boolean"}
                        }
                    }
                }
            }
        }
    })";

// エンジン起動ツールの出力スキーマ
constexpr const char* ENGINE_START_OUTPUT = R"({
        "type": "object",
        "properties": {
            "success": {"type": "boolean"},
            "engineId": {"type": "string"},
            "message": {"type": "string"}
        },
        "required": ["success"]
    })";

// エンジン停止ツールの入力スキーマ
constexpr const char* ENGINE_STOP_INPUT = R"({
        "type": "object",
        "properties": {
            "engineId": {"type": "string"}
        },
        "required": ["engineId"]
    })";

// エンジン停止ツールの出力スキーマ
constexpr const char* ENGINE_STOP_OUTPUT = R"({
        "type": "object",
        "properties": {
            "success": {"type": "boolean"},
            "message": {"type": "string"}
        },
        "required": ["success"]
    })";

// スクリプト実行ツールの入力スキーマ
constexpr const char* EXECUTE_SCRIPT_INPUT = R"({
        "type": "object",
        "properties": {
            "engineId": {"type": "string"},
            "script": {"type": "string"},
            "filename": {"type": "string"},
            "options": {
                "type": "object",
                "properties": {
                    "timeout": {"type": "number"},
                    "strictMode": {"type": "boolean"},
                    "sourceType": {"type": "string", "enum": ["script", "module"]}
                }
            }
        },
        "required": ["engineId", "script"]
    })";

// スクリプト実行ツールの出力スキーマ
constexpr const char* EXECUTE_SCRIPT_OUTPUT = R"({
        "type": "object",
        "properties": {
            "success": {"type": "boolean"},
            "result": {"type": "object"},
            "error": {
                "type": "object",
                "properties": {
                    "name": {"type": "string"},
                    "message": {"type": "string"},
                    "stack": {"type": "string"},
                    "lineNumber": {"type": "number"},
                    "columnNumber": {"type": "number"}
                }
            },
            "executionTime": {"type": "number"}
        },
        "required": ["success"]
    })";

// メモリ使用状況ツールの入力スキーマ
constexpr const char* GET_MEMORY_USAGE_INPUT = R"({
        "type": "object",
        "properties": {
            "engineId": {"type": "string"},
            "detailed": {"type": "boolean"}
        },
        "required": ["engineId"]
    })";

// メモリ使用状況ツールの出力スキーマ
constexpr const char* GET_MEMORY_USAGE_OUTPUT = R"({
        "type": "object",
        "properties": {
            "success": {"type": "boolean"},
            "memory": {
                "type": "object",
                "properties": {
                    "heapSize": {"type": "number"},
                    "heapUsed": {"type": "number"},
                    "heapAvailable": {"type": "number"},
                    "objectCount": {"type": "number"},
                    "stringCount": {"type": "number"},
                    "arrayCount": {"type": "number"},
                    "functionCount": {"type": "number"},
                    "gcMetrics": {
                        "type": "object",
                        "properties": {
                            "lastGCTime": {"type": "number"},
                            "totalGCTime": {"type": "number"},
                            "gcCount": {"type": "number"}
                        }
                    },
                    "details": {"type": "object"}
                }
            },
            "message": {"type": "string"}
        },
        "required": ["success"]
    })";

// ファイル読み込みツールの入力スキーマ
constexpr const char* READ_FILE_INPUT = R"({
        "type": "object",
        "properties": {
            "path": {"type": "string"},
            "encoding": {"type": "string", "enum": ["utf8", "binary"]}
        },
        "required": ["path"]
    })";

// ファイル読み込みツールの出力スキーマ
constexpr const char* READ_FILE_OUTPUT = R"({
        "type": "object",
        "properties": {
            "success": {"type": "boolean"},
            "content": {"type": "string"},
            "error": {"type": "string"},
            "size": {"type": "number"},
            "encoding": {"type": "string"}
        },
        "required": ["success"]
    })";

// プロファイリング開始ツールの入力スキーマ
constexpr const char* START_PROFILING_INPUT = R"({
        "type": "object",
        "properties": {
            "engineId": {"type": "string"},
            "options": {
                "type": "object",
                "properties": {
                    "sampleInterval": {"type": "number"},
                    "recordAllocations": {"type": "boolean"},
                    "recordGC": {"type": "boolean"}
                }
            }
        },
        "required": ["engineId"]
    })";

// プロファイリング開始ツールの出力スキーマ
constexpr const char* START_PROFILING_OUTPUT = R"({
        "type": "object",
        "properties": {
            "success": {"type": "boolean"},
            "profilingId": {"type": "string"},
            "message": {"type": "string"}
        },
        "required": ["success"]
    })";
}  // namespace schemas

// UUIDを生成するユーティリティ関数
static std::string generateUUID() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);
  std::uniform_int_distribution<> dis2(8, 11);

  std::stringstream ss;
  ss << std::hex;

  for (int i = 0; i < 8; i++) {
    ss << dis(gen);
  }
  ss << "-";

  for (int i = 0; i < 4; i++) {
    ss << dis(gen);
  }
  ss << "-4";

  for (int i = 0; i < 3; i++) {
    ss << dis(gen);
  }
  ss << "-";

  ss << dis2(gen);
  for (int i = 0; i < 3; i++) {
    ss << dis(gen);
  }
  ss << "-";

  for (int i = 0; i < 12; i++) {
    ss << dis(gen);
  }

  return ss.str();
}

BasicTools::BasicTools(MCPServer* server)
    : m_server(server) {
  if (!server) {
    throw std::invalid_argument("MCPサーバーがnullです");
  }
}

BasicTools::~BasicTools() {
  // 登録済みのツールを削除
  for (const auto& toolName : m_registeredTools) {
    m_server->unregisterTool(toolName);
  }
}

bool BasicTools::registerAll() {
  bool success = true;

  success &= registerEngineTools();
  success &= registerScriptTools();
  success &= registerModuleTools();
  success &= registerDebugTools();
  success &= registerMemoryTools();
  success &= registerPerformanceTools();
  success &= registerFileSystemTools();
  success &= registerEnvironmentTools();

  return success;
}

bool BasicTools::tryRegisterTool(const Tool& tool) {
  if (m_server->registerTool(tool)) {
    m_registeredTools.push_back(tool.name);
    return true;
  }
  return false;
}

bool BasicTools::registerEngineTools() {
  bool success = true;

  // エンジン起動ツール
  {
    Tool tool;
    tool.name = "engine.start";
    tool.description = "JavaScript エンジンを起動します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = schemas::ENGINE_START_INPUT;
    tool.outputSchema = schemas::ENGINE_START_OUTPUT;
    tool.handler = [this](const std::string& args) {
      return this->handleEngineStart(args);
    };

    success &= tryRegisterTool(tool);
  }

  // エンジン停止ツール
  {
    Tool tool;
    tool.name = "engine.stop";
    tool.description = "JavaScript エンジンを停止します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = schemas::ENGINE_STOP_INPUT;
    tool.outputSchema = schemas::ENGINE_STOP_OUTPUT;
    tool.handler = [this](const std::string& args) {
      return this->handleEngineStop(args);
    };

    success &= tryRegisterTool(tool);
  }

  // エンジン再起動ツール
  {
    Tool tool;
    tool.name = "engine.restart";
    tool.description = "JavaScript エンジンを再起動します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = schemas::ENGINE_STOP_INPUT;  // 同じスキーマを使用
    tool.outputSchema = schemas::ENGINE_START_OUTPUT;
    tool.handler = [this](const std::string& args) {
      return this->handleEngineRestart(args);
    };

    success &= tryRegisterTool(tool);
  }

  // エンジンステータス取得ツール
  {
    Tool tool;
    tool.name = "engine.status";
    tool.description = "JavaScript エンジンのステータスを取得します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = schemas::ENGINE_STOP_INPUT;  // 同じスキーマを使用
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "running": {"type": "boolean"},
                "uptime": {"type": "number"},
                "status": {"type": "string"},
                "stats": {"type": "object"}
            },
            "required": ["running"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleEngineStatus(args);
    };

    success &= tryRegisterTool(tool);
  }

  return success;
}

bool BasicTools::registerScriptTools() {
  bool success = true;

  // スクリプト実行ツール
  {
    Tool tool;
    tool.name = "script.execute";
    tool.description = "JavaScriptコードを実行します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = schemas::EXECUTE_SCRIPT_INPUT;
    tool.outputSchema = schemas::EXECUTE_SCRIPT_OUTPUT;
    tool.handler = [this](const std::string& args) {
      return this->handleExecuteScript(args);
    };

    success &= tryRegisterTool(tool);
  }

  // 式評価ツール
  {
    Tool tool;
    tool.name = "script.evaluate";
    tool.description = "JavaScript式を評価します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "engineId": {"type": "string"},
                "expression": {"type": "string"},
                "context": {"type": "object"}
            },
            "required": ["engineId", "expression"]
        })";
    tool.outputSchema = schemas::EXECUTE_SCRIPT_OUTPUT;
    tool.handler = [this](const std::string& args) {
      return this->handleEvaluateExpression(args);
    };

    success &= tryRegisterTool(tool);
  }

  return success;
}

bool BasicTools::registerModuleTools() {
  bool success = true;

  // モジュールインポートツール
  {
    Tool tool;
    tool.name = "module.import";
    tool.description = "JavaScriptモジュールをインポートします";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "engineId": {"type": "string"},
                "modulePath": {"type": "string"},
                "options": {
                    "type": "object",
                    "properties": {
                        "asType": {"type": "string", "enum": ["esm", "commonjs"]},
                        "timeout": {"type": "number"}
                    }
                }
            },
            "required": ["engineId", "modulePath"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "exports": {"type": "object"},
                "error": {
                    "type": "object",
                    "properties": {
                        "name": {"type": "string"},
                        "message": {"type": "string"},
                        "stack": {"type": "string"}
                    }
                }
            },
            "required": ["success"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleImportModule(args);
    };

    success &= tryRegisterTool(tool);
  }

  return success;
}

bool BasicTools::registerMemoryTools() {
  bool success = true;

  // メモリ使用状況取得ツール
  {
    Tool tool;
    tool.name = "memory.getUsage";
    tool.description = "JavaScript エンジンのメモリ使用状況を取得します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = schemas::GET_MEMORY_USAGE_INPUT;
    tool.outputSchema = schemas::GET_MEMORY_USAGE_OUTPUT;
    tool.handler = [this](const std::string& args) {
      return this->handleGetMemoryUsage(args);
    };

    success &= tryRegisterTool(tool);
  }

  // ガベージコレクション実行ツール
  {
    Tool tool;
    tool.name = "memory.runGC";
    tool.description = "ガベージコレクションを実行します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "engineId": {"type": "string"},
                "fullGC": {"type": "boolean"}
            },
            "required": ["engineId"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "freedBytes": {"type": "number"},
                "duration": {"type": "number"},
                "message": {"type": "string"}
            },
            "required": ["success"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleRunGarbageCollection(args);
    };

    success &= tryRegisterTool(tool);
  }

  return success;
}

bool BasicTools::registerDebugTools() {
  bool success = true;

  // デバッグ情報取得ツール
  {
    Tool tool;
    tool.name = "debug.getInfo";
    tool.description = "エンジンの詳細なデバッグ情報を取得します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "engineId": {"type": "string"},
                "detail": {"type": "string", "enum": ["basic", "full", "verbose"]},
                "sections": {
                    "type": "array",
                    "items": {"type": "string", "enum": ["memory", "execution", "compilation", "objects", "gc"]}
                }
            },
            "required": ["engineId"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "engineId": {"type": "string"},
                "debugInfo": {"type": "object"},
                "timestamp": {"type": "string"},
                "message": {"type": "string"}
            },
            "required": ["success", "engineId", "debugInfo"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleGetDebugInfo(args);
    };

    success &= tryRegisterTool(tool);
  }

  // ブレークポイント設定ツール
  {
    Tool tool;
    tool.name = "debug.setBreakpoint";
    tool.description = "スクリプト実行時のブレークポイントを設定します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "engineId": {"type": "string"},
                "scriptId": {"type": "string"},
                "lineNumber": {"type": "integer", "minimum": 0},
                "columnNumber": {"type": "integer", "minimum": 0},
                "condition": {"type": "string"},
                "enabled": {"type": "boolean"}
            },
            "required": ["engineId", "scriptId", "lineNumber"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "breakpointId": {"type": "string"},
                "actualLineNumber": {"type": "integer"},
                "actualColumnNumber": {"type": "integer"},
                "message": {"type": "string"}
            },
            "required": ["success"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleSetBreakpoint(args);
    };

    success &= tryRegisterTool(tool);
  }

  // デバッグセッション制御ツール
  {
    Tool tool;
    tool.name = "debug.controlSession";
    tool.description = "デバッグセッションの制御（開始/停止/一時停止/再開）を行います";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "engineId": {"type": "string"},
                "action": {"type": "string", "enum": ["start", "stop", "pause", "resume", "stepOver", "stepInto", "stepOut"]},
                "options": {"type": "object"}
            },
            "required": ["engineId", "action"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "sessionId": {"type": "string"},
                "state": {"type": "string", "enum": ["started", "stopped", "paused", "running"]},
                "position": {"type": "object"},
                "message": {"type": "string"}
            },
            "required": ["success", "state"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleDebugSessionControl(args);
    };

    success &= tryRegisterTool(tool);
  }

  // 変数評価ツール
  {
    Tool tool;
    tool.name = "debug.evaluate";
    tool.description = "デバッグコンテキストで式を評価します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "engineId": {"type": "string"},
                "sessionId": {"type": "string"},
                "frameId": {"type": "integer"},
                "expression": {"type": "string"},
                "returnByValue": {"type": "boolean"}
            },
            "required": ["engineId", "sessionId", "expression"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "result": {"type": "object"},
                "exceptionDetails": {"type": "object"},
                "message": {"type": "string"}
            },
            "required": ["success"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleDebugEvaluate(args);
    };

    success &= tryRegisterTool(tool);
  }

  return success;
}

bool BasicTools::registerPerformanceTools() {
  bool success = true;

  // パフォーマンスプロファイリング開始ツール
  {
    Tool tool;
    tool.name = "performance.startProfiling";
    tool.description = "パフォーマンスプロファイリングを開始します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "engineId": {"type": "string"},
                "profileName": {"type": "string"},
                "options": {
                    "type": "object",
                    "properties": {
                        "mode": {"type": "string", "enum": ["cpu", "memory", "gc", "full"]},
                        "samplingInterval": {"type": "integer", "minimum": 1},
                        "includeNative": {"type": "boolean"},
                        "timeLimit": {"type": "integer"}
                    }
                }
            },
            "required": ["engineId"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "profileId": {"type": "string"},
                "startTime": {"type": "string"},
                "message": {"type": "string"}
            },
            "required": ["success"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleStartProfiling(args);
    };

    success &= tryRegisterTool(tool);
  }

  // パフォーマンスプロファイリング停止ツール
  {
    Tool tool;
    tool.name = "performance.stopProfiling";
    tool.description = "パフォーマンスプロファイリングを停止し結果を取得します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "engineId": {"type": "string"},
                "profileId": {"type": "string"},
                "format": {"type": "string", "enum": ["json", "cpuprofile", "heapsnapshot", "flamegraph"]}
            },
            "required": ["engineId", "profileId"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "profileData": {"type": "object"},
                "summary": {"type": "object"},
                "duration": {"type": "number"},
                "message": {"type": "string"}
            },
            "required": ["success"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleStopProfiling(args);
    };

    success &= tryRegisterTool(tool);
  }

  // ヒープスナップショット取得ツール
  {
    Tool tool;
    tool.name = "performance.takeHeapSnapshot";
    tool.description = "現在のヒープのスナップショットを取得します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "engineId": {"type": "string"},
                "detailed": {"type": "boolean"},
                "includeObjects": {"type": "boolean"}
            },
            "required": ["engineId"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "snapshotId": {"type": "string"},
                "timestamp": {"type": "string"},
                "summary": {"type": "object"},
                "message": {"type": "string"}
            },
            "required": ["success", "snapshotId"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleTakeHeapSnapshot(args);
    };

    success &= tryRegisterTool(tool);
  }

  // パフォーマンスメトリクス取得ツール
  {
    Tool tool;
    tool.name = "performance.getMetrics";
    tool.description = "エンジンのパフォーマンスメトリクスを取得します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "engineId": {"type": "string"},
                "metrics": {
                    "type": "array",
                    "items": {"type": "string", "enum": ["cpu", "memory", "gc", "compilation", "execution", "all"]}
                }
            },
            "required": ["engineId"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "timestamp": {"type": "string"},
                "metrics": {"type": "object"},
                "message": {"type": "string"}
            },
            "required": ["success", "metrics"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleGetPerformanceMetrics(args);
    };

    success &= tryRegisterTool(tool);
  }

  return success;
}

bool BasicTools::registerFileSystemTools() {
  bool success = true;

  // ファイル読み込みツール
  {
    Tool tool;
    tool.name = "fs.readFile";
    tool.description = "ファイルの内容を読み込みます";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "path": {"type": "string"},
                "encoding": {"type": "string", "enum": ["utf8", "binary", "base64"]},
                "maxSize": {"type": "integer"}
            },
            "required": ["path"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "content": {"type": "string"},
                "size": {"type": "integer"},
                "encoding": {"type": "string"},
                "message": {"type": "string"}
            },
            "required": ["success"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleReadFile(args);
    };

    success &= tryRegisterTool(tool);
  }

  // ファイル書き込みツール
  {
    Tool tool;
    tool.name = "fs.writeFile";
    tool.description = "ファイルに内容を書き込みます";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "path": {"type": "string"},
                "content": {"type": "string"},
                "encoding": {"type": "string", "enum": ["utf8", "binary", "base64"]},
                "append": {"type": "boolean"}
            },
            "required": ["path", "content"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "bytesWritten": {"type": "integer"},
                "message": {"type": "string"}
            },
            "required": ["success"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleWriteFile(args);
    };

    success &= tryRegisterTool(tool);
  }

  // ディレクトリ操作ツール
  {
    Tool tool;
    tool.name = "fs.directory";
    tool.description = "ディレクトリの作成、読み取り、削除を行います";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "action": {"type": "string", "enum": ["list", "create", "delete", "exists"]},
                "path": {"type": "string"},
                "recursive": {"type": "boolean"},
                "filter": {"type": "string"}
            },
            "required": ["action", "path"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "entries": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "properties": {
                            "name": {"type": "string"},
                            "type": {"type": "string", "enum": ["file", "directory", "symlink", "other"]},
                            "size": {"type": "integer"},
                            "modifiedTime": {"type": "string"}
                        }
                    }
                },
                "exists": {"type": "boolean"},
                "message": {"type": "string"}
            },
            "required": ["success"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleDirectoryOperation(args);
    };

    success &= tryRegisterTool(tool);
  }

  // ファイルシステム監視ツール
  {
    Tool tool;
    tool.name = "fs.watch";
    tool.description = "ファイルやディレクトリの変更を監視します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "path": {"type": "string"},
                "recursive": {"type": "boolean"},
                "events": {
                    "type": "array",
                    "items": {"type": "string", "enum": ["create", "modify", "delete", "rename", "all"]}
                },
                "filter": {"type": "string"},
                "watchId": {"type": "string"}
            },
            "required": ["path"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "watchId": {"type": "string"},
                "message": {"type": "string"}
            },
            "required": ["success"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleFileSystemWatch(args);
    };

    success &= tryRegisterTool(tool);
  }

  return success;
}

bool BasicTools::registerEnvironmentTools() {
  bool success = true;

  // 環境変数操作ツール
  {
    Tool tool;
    tool.name = "env.variable";
    tool.description = "環境変数の取得、設定、削除を行います";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "action": {"type": "string", "enum": ["get", "set", "delete", "list"]},
                "name": {"type": "string"},
                "value": {"type": "string"},
                "engineId": {"type": "string"}
            },
            "required": ["action"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "value": {"type": "string"},
                "variables": {"type": "object"},
                "message": {"type": "string"}
            },
            "required": ["success"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleEnvironmentVariable(args);
    };

    success &= tryRegisterTool(tool);
  }

  // システム情報取得ツール
  {
    Tool tool;
    tool.name = "env.systemInfo";
    tool.description = "実行環境のシステム情報を取得します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "sections": {
                    "type": "array",
                    "items": {"type": "string", "enum": ["os", "cpu", "memory", "network", "all"]}
                }
            }
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "systemInfo": {"type": "object"},
                "timestamp": {"type": "string"},
                "message": {"type": "string"}
            },
            "required": ["success", "systemInfo"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleGetSystemInfo(args);
    };

    success &= tryRegisterTool(tool);
  }

  // エンジン設定ツール
  {
    Tool tool;
    tool.name = "env.engineConfig";
    tool.description = "JavaScript エンジンの設定を取得または変更します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "engineId": {"type": "string"},
                "action": {"type": "string", "enum": ["get", "set", "reset"]},
                "config": {"type": "object"},
                "path": {"type": "string"}
            },
            "required": ["engineId", "action"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "config": {"type": "object"},
                "message": {"type": "string"}
            },
            "required": ["success"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleEngineConfig(args);
    };

    success &= tryRegisterTool(tool);
  }

  // ロケール設定ツール
  {
    Tool tool;
    tool.name = "env.locale";
    tool.description = "エンジンのロケール設定を取得または変更します";
    tool.type = ToolType::FUNCTION;
    tool.inputSchema = R"({
            "type": "object",
            "properties": {
                "engineId": {"type": "string"},
                "action": {"type": "string", "enum": ["get", "set", "list"]},
                "locale": {"type": "string"},
                "options": {"type": "object"}
            },
            "required": ["action"]
        })";
    tool.outputSchema = R"({
            "type": "object",
            "properties": {
                "success": {"type": "boolean"},
                "locale": {"type": "string"},
                "availableLocales": {"type": "array", "items": {"type": "string"}},
                "message": {"type": "string"}
            },
            "required": ["success"]
        })";
    tool.handler = [this](const std::string& args) {
      return this->handleLocaleOperation(args);
    };

    success &= tryRegisterTool(tool);
  }

  return success;
}

// 以下、実装メソッド

std::string BasicTools::handleEngineStart(const std::string& args) {
  try {
    nlohmann::json argsJson = nlohmann::json::parse(args);
    nlohmann::json options = argsJson.contains("options") ? argsJson["options"] : nlohmann::json::object();

    // エンジン設定の検証と準備
    EngineConfig config;
    if (options.contains("memoryLimit")) {
      config.memoryLimit = options["memoryLimit"].get<size_t>();
    }

    if (options.contains("optimizationLevel")) {
      config.optimizationLevel = options["optimizationLevel"].get<int>();
    }

    if (options.contains("enableJIT")) {
      config.enableJIT = options["enableJIT"].get<bool>();
    }

    if (options.contains("gcMode")) {
      std::string gcMode = options["gcMode"].get<std::string>();
      if (gcMode == "aggressive") {
        config.gcConfig.mode = GCMode::AGGRESSIVE;
      } else if (gcMode == "conservative") {
        config.gcConfig.mode = GCMode::CONSERVATIVE;
      } else if (gcMode == "incremental") {
        config.gcConfig.mode = GCMode::INCREMENTAL;
      }
    }

    if (options.contains("locale")) {
      config.locale = options["locale"].get<std::string>();
    }

    if (options.contains("timezone")) {
      config.timezone = options["timezone"].get<std::string>();
    }

    // エンジンインスタンスの作成
    std::string engineId = generateUUID();

    // エンジンの初期化と起動
    bool engineStarted = initializeEngine(engineId, config);
    if (!engineStarted) {
      return nlohmann::json({{"success", false},
                             {"message", "エンジンの初期化に失敗しました"}})
          .dump();
    }

    // エンジンの状態を記録
    {
      std::lock_guard<std::mutex> lock(m_enginesMutex);
      m_engines[engineId] = {
          {"status", "running"},
          {"startTime", getCurrentTimestamp()},
          {"config", config.toJson()}};
    }

    // 初期メモリ使用量を取得
    MemoryStats initialMemory = getEngineMemoryStats(engineId);

    return nlohmann::json({{"success", true},
                           {"engineId", engineId},
                           {"status", "running"},
                           {"startTime", getCurrentTimestamp()},
                           {"initialMemory", {{"total", initialMemory.totalBytes}, {"used", initialMemory.usedBytes}, {"peak", initialMemory.peakBytes}}},
                           {"message", "エンジンが正常に起動しました"}})
        .dump();
  } catch (const std::exception& e) {
    return nlohmann::json({{"success", false},
                           {"message", std::string("エンジン起動中にエラーが発生しました: ") + e.what()}})
        .dump();
  }
}

std::string BasicTools::handleEngineStop(const std::string& args) {
  try {
    nlohmann::json argsJson = nlohmann::json::parse(args);

    if (!argsJson.contains("engineId")) {
      return nlohmann::json({{"success", false},
                             {"message", "engineIdが指定されていません"}})
          .dump();
    }

    std::string engineId = argsJson["engineId"];

    if (auto engine = EngineManager::getInstance()->getEngineById(engineId)) {
      engine->shutdown();
      return true;
    }
    return false;
  } catch (const std::exception& e) {
    return nlohmann::json({{"success", false},
                           {"message", std::string("エンジン停止中にエラーが発生しました: ") + e.what()}})
        .dump();
  }
}

std::string BasicTools::handleEngineRestart(const std::string& args) {
  try {
    // 一旦停止してから再起動
    nlohmann::json stopResult = nlohmann::json::parse(handleEngineStop(args));

    if (!stopResult["success"]) {
      return nlohmann::json({{"success", false},
                             {"message", "エンジン再起動中にエラーが発生しました: " + stopResult["message"].get<std::string>()}})
          .dump();
    }

    // 新しいエンジンを起動
    nlohmann::json argsJson = nlohmann::json::parse(args);
    argsJson["options"] = argsJson.contains("options") ? argsJson["options"] : nlohmann::json::object();

    return handleEngineStart(argsJson.dump());
  } catch (const std::exception& e) {
    return nlohmann::json({{"success", false},
                           {"message", std::string("エンジン再起動中にエラーが発生しました: ") + e.what()}})
        .dump();
  }
}

std::string BasicTools::handleEngineStatus(const std::string& args) {
  try {
    nlohmann::json argsJson = nlohmann::json::parse(args);

    if (!argsJson.contains("engineId")) {
      return nlohmann::json({{"success", false},
                             {"message", "engineIdが指定されていません"}})
          .dump();
    }

    std::string engineId = argsJson["engineId"];

    if (auto engine = EngineManager::getInstance()->getEngineById(engineId)) {
      return engine->getStatus();
    }
    return EngineStatus::Unknown;
  } catch (const std::exception& e) {
    return nlohmann::json({{"running", false},
                           {"message", std::string("エンジンステータス取得中にエラーが発生しました: ") + e.what()}})
        .dump();
  }
}

std::string BasicTools::handleExecuteScript(const std::string& args) {
  try {
    nlohmann::json argsJson = nlohmann::json::parse(args);

    if (!argsJson.contains("engineId") || !argsJson.contains("script")) {
      return nlohmann::json({{"success", false},
                             {"error", {{"name", "InvalidArgumentError"}, {"message", "engineIdまたはscriptが指定されていません"}}}})
          .dump();
    }

    std::string engineId = argsJson["engineId"];
    std::string script = argsJson["script"];
    std::string filename = argsJson.contains("filename") ? argsJson["filename"] : "<mcp-script>";

    nlohmann::json options = argsJson.contains("options") ? argsJson["options"] : nlohmann::json::object();
    bool strictMode = options.contains("strictMode") ? options["strictMode"].get<bool>() : false;
    std::string sourceType = options.contains("sourceType") ? options["sourceType"].get<std::string>() : "script";

    if (auto engine = EngineManager::getInstance()->getEngineById(engineId)) {
      return engine->executeScript(script);
    }
    return ScriptResult::Error;
  } catch (const std::exception& e) {
    return nlohmann::json({{"success", false},
                           {"error", {{"name", "ExecutionError"}, {"message", std::string("スクリプト実行中にエラーが発生しました: ") + e.what()}}}})
        .dump();
  }
}

std::string BasicTools::handleEvaluateExpression(const std::string& args) {
  try {
    nlohmann::json argsJson = nlohmann::json::parse(args);

    if (!argsJson.contains("engineId") || !argsJson.contains("expression")) {
      return nlohmann::json({{"success", false},
                             {"error", {{"name", "InvalidArgumentError"}, {"message", "engineIdまたはexpressionが指定されていません"}}}})
          .dump();
    }

    std::string engineId = argsJson["engineId"];
    std::string expression = argsJson["expression"];
    nlohmann::json context = argsJson.contains("context") ? argsJson["context"] : nlohmann::json::object();

    if (auto engine = EngineManager::getInstance()->getEngineById(engineId)) {
      return engine->evaluateExpression(expression);
    }
    return EvalResult::Error;
  } catch (const std::exception& e) {
    return nlohmann::json({{"success", false},
                           {"error", {{"name", "EvaluationError"}, {"message", std::string("式評価中にエラーが発生しました: ") + e.what()}}}})
        .dump();
  }
}

std::string BasicTools::handleImportModule(const std::string& args) {
  try {
    nlohmann::json argsJson = nlohmann::json::parse(args);

    if (!argsJson.contains("engineId") || !argsJson.contains("modulePath")) {
      return nlohmann::json({{"success", false},
                             {"error", {{"name", "InvalidArgumentError"}, {"message", "engineIdまたはmodulePathが指定されていません"}}}})
          .dump();
    }

    std::string engineId = argsJson["engineId"];
    std::string modulePath = argsJson["modulePath"];

    nlohmann::json options = argsJson.contains("options") ? argsJson["options"] : nlohmann::json::object();
    std::string asType = options.contains("asType") ? options["asType"].get<std::string>() : "esm";

    if (auto engine = EngineManager::getInstance()->getEngineById(engineId)) {
      return engine->importModule(modulePath);
    }
    return ImportResult::Error;
  } catch (const std::exception& e) {
    return nlohmann::json({{"success", false},
                           {"error", {{"name", "ImportError"}, {"message", std::string("モジュールインポート中にエラーが発生しました: ") + e.what()}, {"stack", "ImportError: " + std::string(e.what()) + "\n    at MCPServer.importModule (mcp_server.cpp:123:45)"}}}})
        .dump();
  }
}

std::string BasicTools::handleGetMemoryUsage(const std::string& args) {
  try {
    nlohmann::json argsJson = nlohmann::json::parse(args);

    if (!argsJson.contains("engineId")) {
      return nlohmann::json({{"success", false},
                             {"message", "engineIdが指定されていません"}})
          .dump();
    }

    std::string engineId = argsJson["engineId"];
    bool detailed = argsJson.contains("detailed") ? argsJson["detailed"].get<bool>() : false;

    if (auto engine = EngineManager::getInstance()->getEngineById(engineId)) {
      return engine->getMemoryUsage();
    }
    return MemoryUsageInfo{};
  } catch (const std::exception& e) {
    return nlohmann::json({{"success", false},
                           {"message", std::string("メモリ使用状況取得中にエラーが発生しました: ") + e.what()}})
        .dump();
  }
}

std::string BasicTools::handleRunGarbageCollection(const std::string& args) {
  try {
    nlohmann::json argsJson = nlohmann::json::parse(args);

    if (!argsJson.contains("engineId")) {
      return nlohmann::json({{"success", false},
                             {"message", "engineIdが指定されていません"}})
          .dump();
    }

    std::string engineId = argsJson["engineId"];
    bool fullGC = argsJson.contains("fullGC") ? argsJson["fullGC"].get<bool>() : false;

    if (auto engine = EngineManager::getInstance()->getEngineById(engineId)) {
      engine->runGC();
      return true;
    }
    return false;
  } catch (const std::exception& e) {
    return nlohmann::json({{"success", false},
                           {"message", std::string("ガベージコレクション実行中にエラーが発生しました: ") + e.what()}})
        .dump();
  }
}

std::string BasicTools::handleGetDebugInfo(const std::string& args) {
  try {
    nlohmann::json request = nlohmann::json::parse(args);
    nlohmann::json response;
    
    // デバッグ情報の収集
    response["runtime_info"] = {
      {"version", "AeroJS-1.0.0"},
      {"build_type", 
#ifdef _DEBUG
        "Debug"
#else
        "Release"
#endif
      },
      {"architecture", 
#if defined(__x86_64__) || defined(_M_X64)
        "x64"
#elif defined(__aarch64__) || defined(_M_ARM64)
        "arm64"
#else
        "unknown"
#endif
      },
      {"memory_usage", getMemoryUsage()},
      {"threads", getThreadCount()}
    };
    
    // JIT情報
    if (m_context && m_context->getJITManager()) {
      auto* jitManager = m_context->getJITManager();
      response["jit_info"] = {
        {"enabled", true},
        {"compilation_stats", jitManager->getCompilationStatistics()},
        {"optimization_level", static_cast<int>(jitManager->getOptimizationLevel())}
      };
    }
    
    // GC情報
    if (m_context && m_context->getGC()) {
      auto* gc = m_context->getGC();
      response["gc_info"] = {
        {"type", gc->getType()},
        {"heap_size", gc->getHeapSize()},
        {"used_memory", gc->getUsedMemory()},
        {"collection_count", gc->getCollectionCount()}
      };
    }
    
    return response.dump();
    
  } catch (const std::exception& e) {
    nlohmann::json error = {
      {"error", "Failed to get debug info"},
      {"message", e.what()}
    };
    return error.dump();
  }
}

std::string BasicTools::handleSetBreakpoint(const std::string& args) {
  try {
    nlohmann::json request = nlohmann::json::parse(args);
    nlohmann::json response;
    
    std::string filename = request.value("filename", "");
    int lineNumber = request.value("line", 0);
    bool enabled = request.value("enabled", true);
    std::string condition = request.value("condition", "");
    
    if (filename.empty() || lineNumber <= 0) {
      response["error"] = "Invalid filename or line number";
      return response.dump();
    }
    
    // デバッガーコンテキストの取得
    if (!m_context || !m_context->getDebugger()) {
      response["error"] = "Debugger not available";
      return response.dump();
    }
    
    auto* debugger = m_context->getDebugger();
    
    // ブレークポイントの設定
    BreakpointInfo breakpoint;
    breakpoint.filename = filename;
    breakpoint.lineNumber = lineNumber;
    breakpoint.enabled = enabled;
    breakpoint.condition = condition;
    breakpoint.hitCount = 0;
    
    uint32_t breakpointId = debugger->setBreakpoint(breakpoint);
    
    response["success"] = true;
    response["breakpoint_id"] = breakpointId;
    response["location"] = {
      {"filename", filename},
      {"line", lineNumber},
      {"enabled", enabled}
    };
    
    if (!condition.empty()) {
      response["condition"] = condition;
    }
    
    return response.dump();
    
  } catch (const std::exception& e) {
    nlohmann::json error = {
      {"error", "Failed to set breakpoint"},
      {"message", e.what()}
    };
    return error.dump();
  }
}

std::string BasicTools::handleStartProfiling(const std::string& args) {
  try {
    nlohmann::json request = nlohmann::json::parse(args);
    nlohmann::json response;
    
    std::string profileType = request.value("type", "cpu");
    int durationMs = request.value("duration", 30000);
    int sampleRate = request.value("sample_rate", 1000);
    bool includeNative = request.value("include_native", false);
    
    if (!m_context || !m_context->getProfiler()) {
      response["error"] = "Profiler not available";
      return response.dump();
    }
    
    auto* profiler = m_context->getProfiler();
    
    ProfilingConfig config;
    config.type = (profileType == "memory") ? ProfilingType::Memory : ProfilingType::CPU;
    config.durationMs = durationMs;
    config.sampleRateHz = sampleRate;
    config.includeNativeCode = includeNative;
    config.captureStackTraces = true;
    config.captureSourceInfo = true;
    
    uint32_t sessionId = profiler->startProfiling(config);
    
    response["success"] = true;
    response["session_id"] = sessionId;
    response["config"] = {
      {"type", profileType},
      {"duration_ms", durationMs},
      {"sample_rate_hz", sampleRate},
      {"include_native", includeNative}
    };
    
    return response.dump();
    
  } catch (const std::exception& e) {
    nlohmann::json error = {
      {"error", "Failed to start profiling"},
      {"message", e.what()}
    };
    return error.dump();
  }
}

std::string BasicTools::handleStopProfiling(const std::string& args) {
  try {
    nlohmann::json request = nlohmann::json::parse(args);
    nlohmann::json response;
    
    uint32_t sessionId = request.value("session_id", 0);
    std::string outputFormat = request.value("format", "json");
    std::string outputFile = request.value("output_file", "");
    
    if (!m_context || !m_context->getProfiler()) {
      response["error"] = "Profiler not available";
      return response.dump();
    }
    
    auto* profiler = m_context->getProfiler();
    
    if (sessionId == 0) {
      // 現在実行中のすべてのプロファイリングセッションを停止
      profiler->stopAllProfiling();
    } else {
      if (!profiler->stopProfiling(sessionId)) {
        response["error"] = "Invalid session ID or session not active";
        return response.dump();
      }
    }
    
    // プロファイル結果の取得
    ProfilingResult result = profiler->getProfilingResult(sessionId);
    
    response["success"] = true;
    response["session_id"] = sessionId;
    response["profile_data"] = {
      {"total_samples", result.totalSamples},
      {"duration_ms", result.durationMs},
      {"memory_usage", result.memoryUsage},
      {"function_stats", result.functionStats}
    };
    
    // ファイルへの出力
    if (!outputFile.empty()) {
      if (outputFormat == "flamegraph") {
        profiler->exportFlameGraph(sessionId, outputFile);
      } else if (outputFormat == "callgrind") {
        profiler->exportCallgrind(sessionId, outputFile);
      } else {
        profiler->exportJSON(sessionId, outputFile);
      }
      response["output_file"] = outputFile;
    }
    
    return response.dump();
    
  } catch (const std::exception& e) {
    nlohmann::json error = {
      {"error", "Failed to stop profiling"},
      {"message", e.what()}
    };
    return error.dump();
  }
}

std::string BasicTools::handleReadFile(const std::string& args) {
  try {
    nlohmann::json request = nlohmann::json::parse(args);
    nlohmann::json response;
    
    std::string filename = request.value("filename", "");
    std::string encoding = request.value("encoding", "utf8");
    int maxSize = request.value("max_size", 1024 * 1024); // 1MB default
    
    if (filename.empty()) {
      response["error"] = "Filename is required";
      return response.dump();
    }
    
    // セキュリティチェック: パスの正規化と制限
    std::filesystem::path filepath(filename);
    filepath = std::filesystem::canonical(filepath);
    
    // セキュリティ制限: 作業ディレクトリ外のアクセスを禁止
    std::filesystem::path workDir = std::filesystem::current_path();
    if (!filepath.string().starts_with(workDir.string())) {
      response["error"] = "Access denied: file outside working directory";
      return response.dump();
    }
    
    if (!std::filesystem::exists(filepath)) {
      response["error"] = "File not found";
      return response.dump();
    }
    
    auto fileSize = std::filesystem::file_size(filepath);
    if (fileSize > maxSize) {
      response["error"] = "File too large";
      response["file_size"] = fileSize;
      response["max_size"] = maxSize;
      return response.dump();
    }
    
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
      response["error"] = "Failed to open file";
      return response.dump();
    }
    
    std::string content;
    content.resize(fileSize);
    file.read(content.data(), fileSize);
    
    if (encoding == "base64") {
      content = base64Encode(content);
    } else if (encoding != "utf8" && encoding != "binary") {
      response["error"] = "Unsupported encoding";
      return response.dump();
    }
    
    response["success"] = true;
    response["filename"] = filename;
    response["content"] = content;
    response["size"] = fileSize;
    response["encoding"] = encoding;
    
    return response.dump();
    
  } catch (const std::exception& e) {
    nlohmann::json error = {
      {"error", "Failed to read file"},
      {"message", e.what()}
    };
    return error.dump();
  }
}

std::string BasicTools::handleWriteFile(const std::string& args) {
  try {
    nlohmann::json request = nlohmann::json::parse(args);
    nlohmann::json response;
    
    std::string filename = request.value("filename", "");
    std::string content = request.value("content", "");
    std::string encoding = request.value("encoding", "utf8");
    bool append = request.value("append", false);
    bool createDirs = request.value("create_dirs", false);
    
    if (filename.empty()) {
      response["error"] = "Filename is required";
      return response.dump();
    }
    
    // セキュリティチェック
    std::filesystem::path filepath(filename);
    if (createDirs) {
      std::filesystem::create_directories(filepath.parent_path());
    }
    
    // パスの正規化
    filepath = std::filesystem::absolute(filepath);
    
    // セキュリティ制限
    std::filesystem::path workDir = std::filesystem::current_path();
    if (!filepath.string().starts_with(workDir.string())) {
      response["error"] = "Access denied: file outside working directory";
      return response.dump();
    }
    
    // エンコーディング処理
    std::string decodedContent = content;
    if (encoding == "base64") {
      decodedContent = base64Decode(content);
    } else if (encoding != "utf8" && encoding != "binary") {
      response["error"] = "Unsupported encoding";
      return response.dump();
    }
    
    std::ios::openmode mode = std::ios::binary;
    if (append) {
      mode |= std::ios::app;
    } else {
      mode |= std::ios::trunc;
    }
    
    std::ofstream file(filepath, mode);
    if (!file.is_open()) {
      response["error"] = "Failed to open file for writing";
      return response.dump();
    }
    
    file.write(decodedContent.data(), decodedContent.size());
    file.close();
    
    if (file.fail()) {
      response["error"] = "Failed to write file";
      return response.dump();
    }
    
    response["success"] = true;
    response["filename"] = filename;
    response["bytes_written"] = decodedContent.size();
    response["encoding"] = encoding;
    response["append"] = append;
    
    return response.dump();
    
  } catch (const std::exception& e) {
    nlohmann::json error = {
      {"error", "Failed to write file"},
      {"message", e.what()}
    };
    return error.dump();
  }
}

std::string BasicTools::handleGetEnvironmentInfo(const std::string& args) {
  try {
    nlohmann::json response;
    
    // システム情報
    response["system"] = {
      {"platform", getPlatformName()},
      {"architecture", getArchitectureName()},
      {"cpu_count", std::thread::hardware_concurrency()},
      {"page_size", getPageSize()},
      {"endianness", isLittleEndian() ? "little" : "big"}
    };
    
    // メモリ情報
    response["memory"] = {
      {"total_physical", getTotalPhysicalMemory()},
      {"available_physical", getAvailablePhysicalMemory()},
      {"total_virtual", getTotalVirtualMemory()},
      {"process_working_set", getProcessWorkingSet()}
    };
    
    // 環境変数
    response["environment"] = getEnvironmentVariables();
    
    // プロセス情報
    response["process"] = {
      {"pid", getCurrentProcessId()},
      {"executable_path", getExecutablePath()},
      {"working_directory", std::filesystem::current_path().string()},
      {"command_line", getCommandLine()}
    };
    
    // AeroJS固有情報
    response["aerojs"] = {
      {"version", AEROJS_VERSION},
      {"build_date", __DATE__ " " __TIME__},
      {"jit_enabled", 
#ifdef AEROJS_JIT_ENABLED
        true
#else
        false
#endif
      },
      {"debug_build",
#ifdef _DEBUG
        true
#else
        false
#endif
      }
    };
    
    // ランタイム情報
    if (m_context) {
      response["runtime"] = {
        {"heap_size", m_context->getHeapSize()},
        {"stack_size", m_context->getStackSize()},
        {"global_objects", m_context->getGlobalObjectCount()},
        {"active_functions", m_context->getActiveFunctionCount()}
      };
    }
    
    return response.dump();
    
  } catch (const std::exception& e) {
    nlohmann::json error = {
      {"error", "Failed to get environment info"},
      {"message", e.what()}
    };
    return error.dump();
  }
}

}  // namespace tools
}  // namespace mcp
}  // namespace aero