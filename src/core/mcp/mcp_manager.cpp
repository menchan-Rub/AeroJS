/**
 * @file mcp_manager.cpp
 * @brief Model Context Protocol (MCP) マネージャーの実装
 * 
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#include "mcp_manager.h"
#include <stdexcept>
#include <iostream>

namespace aero {
namespace mcp {

// 静的メンバの初期化
MCPManager* MCPManager::s_instance = nullptr;
std::mutex MCPManager::s_instanceMutex;

MCPManager& MCPManager::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    
    if (s_instance == nullptr) {
        s_instance = new MCPManager();
    }
    
    return *s_instance;
}

MCPManager::MCPManager()
    : m_initialized(false) {
}

MCPManager::~MCPManager() {
    shutdown();
}

bool MCPManager::initialize(const MCPServerOptions& options) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        std::cerr << "MCPマネージャーは既に初期化されています" << std::endl;
        return false;
    }
    
    try {
        // MCPサーバーを作成
        m_server = std::make_unique<MCPServer>(options);
        
        // 基本ツールを登録
        registerBasicTools();
        
        m_initialized = true;
        std::cerr << "MCPマネージャーが初期化されました" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "MCPマネージャーの初期化中にエラーが発生しました: " << e.what() << std::endl;
        m_server.reset();
        m_basicTools.reset();
        m_initialized = false;
        return false;
    }
}

void MCPManager::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        return;
    }
    
    // サーバーが実行中なら停止
    if (m_server && isServerRunning()) {
        m_server->stop();
    }
    
    // リソースをクリーンアップ
    m_basicTools.reset();
    m_server.reset();
    
    m_initialized = false;
    std::cerr << "MCPマネージャーがシャットダウンされました" << std::endl;
}

bool MCPManager::startServer(int port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        std::cerr << "MCPマネージャーが初期化されていません" << std::endl;
        return false;
    }
    
    if (!m_server) {
        std::cerr << "MCPサーバーが存在しません" << std::endl;
        return false;
    }
    
    if (isServerRunning()) {
        std::cerr << "MCPサーバーは既に実行中です" << std::endl;
        return false;
    }
    
    return m_server->start(port);
}

void MCPManager::stopServer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized || !m_server || !isServerRunning()) {
        return;
    }
    
    m_server->stop();
}

bool MCPManager::isServerRunning() const {
    // この実装は単純化されています。実際の実装ではサーバーの状態をチェックすべきです。
    return m_initialized && m_server != nullptr;
}

MCPServer* MCPManager::getServer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        return nullptr;
    }
    
    return m_server.get();
}

bool MCPManager::registerBasicTools() {
    if (!m_initialized || !m_server) {
        std::cerr << "MCPマネージャーが初期化されていないため、基本ツールを登録できません" << std::endl;
        return false;
    }
    
    try {
        // 基本ツールクラスを初期化
        m_basicTools = std::make_unique<tools::BasicTools>(m_server.get());
        
        // 全てのツールを登録
        bool success = m_basicTools->registerAll();
        
        if (success) {
            std::cerr << "基本ツールが正常に登録されました" << std::endl;
        } else {
            std::cerr << "一部の基本ツールの登録に失敗しました" << std::endl;
        }
        
        return success;
    } catch (const std::exception& e) {
        std::cerr << "基本ツール登録中にエラーが発生しました: " << e.what() << std::endl;
        m_basicTools.reset();
        return false;
    }
}

} // namespace mcp
} // namespace aero 