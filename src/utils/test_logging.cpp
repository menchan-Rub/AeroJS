/**
 * @file test_logging.cpp
 * @brief ロギングシステムのテストプログラム
 * 
 * @author AeroJS Team
 * @version 1.0.0
 * @copyright MIT License
 */

#include "logging.h"
#include <iostream>

int main() {
    // ロギングシステムの初期化
    aerojs::utils::ConfigureDebugLogging();
    
    // 基本ログテスト
    AEROJS_LOG_INFO("AeroJS ロギングシステムのテストを開始します");
    AEROJS_LOG_DEBUG("デバッグレベルのログです");
    AEROJS_LOG_WARNING("警告レベルのログです");
    AEROJS_LOG_ERROR("エラーレベルのログです");
    
    // カテゴリ別ログテスト
    AEROJS_JIT_LOG_INFO("JITコンパイラのログです");
    AEROJS_PARSER_LOG_INFO("パーサーのログです");
    AEROJS_RUNTIME_LOG_INFO("ランタイムのログです");
    AEROJS_GC_LOG_INFO("ガベージコレクタのログです");
    AEROJS_NETWORK_LOG_INFO("ネットワークのログです");
    
    // パフォーマンス測定テスト
    {
        AEROJS_SCOPED_TIMER("テスト処理");
        
        // 何らかの処理をシミュレート
        for (int i = 0; i < 1000000; ++i) {
            volatile int x = i * i;
        }
        
        AEROJS_LOG_INFO("処理が完了しました: %d回の計算", 1000000);
    }
    
    // エラーコンテキスト付きログテスト
    AEROJS_LOG_ERROR_WITH_CONTEXT("コンテキスト情報付きのエラーログです");
    
    // アサーションテスト（コメントアウトして回避）
    // AEROJS_LOG_ASSERT(false, "このアサーションは失敗します");
    
    // メモリ使用量ログテスト
    AEROJS_LOG_MEMORY_USAGE("テスト終了時のメモリ使用量");
    
    // ログシステムの終了
    AEROJS_LOG_INFO("テストが完了しました");
    aerojs::utils::ShutdownLogging();
    
    return 0;
} 