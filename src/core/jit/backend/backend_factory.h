/**
 * @file backend_factory.h
 * @brief AeroJS JavaScriptエンジン用バックエンドファクトリ
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include "backend.h"
#include <memory>
#include <string>

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class JITProfiler;

/**
 * @brief JITバックエンドを作成するファクトリクラス
 *
 * 利用可能なJITバックエンド（x86_64, arm64, riscvなど）を検出し、
 * 現在の実行環境に最適なバックエンドを提供します。
 */
class BackendFactory {
public:
    /**
     * @brief 現在の実行環境に最適なJITバックエンドを作成
     * @param context JavaScriptコンテキスト
     * @param profiler JITプロファイラ（オプション）
     * @return バックエンドのインスタンス、作成できない場合はnullptr
     */
    static std::unique_ptr<Backend> createBackend(Context* context, JITProfiler* profiler = nullptr);
    
    /**
     * @brief 指定されたアーキテクチャ用のJITバックエンドを作成
     * @param archName アーキテクチャ名（"x86_64", "arm64", "riscv"など）
     * @param context JavaScriptコンテキスト
     * @param profiler JITプロファイラ（オプション）
     * @return バックエンドのインスタンス、作成できない場合はnullptr
     */
    static std::unique_ptr<Backend> createBackendForArchitecture(
        const std::string& archName,
        Context* context,
        JITProfiler* profiler = nullptr);
    
    /**
     * @brief 利用可能なバックエンドのリストを取得
     * @return 利用可能なバックエンドのアーキテクチャ名の配列
     */
    static std::vector<std::string> getAvailableBackends();
    
    /**
     * @brief 現在の実行環境のCPUアーキテクチャを検出
     * @return 検出されたCPUアーキテクチャ名
     */
    static std::string detectCPUArchitecture();
    
    /**
     * @brief 指定されたアーキテクチャがサポートされているか確認
     * @param archName 確認するアーキテクチャ名
     * @return サポートされている場合はtrue、それ以外はfalse
     */
    static bool isArchitectureSupported(const std::string& archName);
    
private:
    // x86_64バックエンドの作成
    static std::unique_ptr<Backend> createX86_64Backend(Context* context, JITProfiler* profiler);
    
    // ARM64バックエンドの作成
    static std::unique_ptr<Backend> createARM64Backend(Context* context, JITProfiler* profiler);
    
    // RISCVバックエンドの作成
    static std::unique_ptr<Backend> createRISCVBackend(Context* context, JITProfiler* profiler);
    
    // インタプリタバックエンドの作成（JITが利用できない場合のフォールバック）
    static std::unique_ptr<Backend> createInterpreterBackend(Context* context);
};

} // namespace core
} // namespace aerojs