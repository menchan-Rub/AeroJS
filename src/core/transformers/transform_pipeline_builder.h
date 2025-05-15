/**
 * @file transform_pipeline_builder.h
 * @version 1.0.0
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 * @brief トランスフォーマーパイプラインの構築ユーティリティ
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "transformer.h"
#include "parallel_array_optimization.h"
#include "constant_folding.h"
#include "arch/riscv_array_optimizations.h"
#include "src/utils/platform/cpu_features.h"

namespace aerojs::transformers {

/**
 * @enum OptimizationProfile
 * @brief 最適化プロファイルの種類
 */
enum class OptimizationProfile {
  Debug,          ///< デバッグ用（最小限の最適化）
  Default,        ///< デフォルト（バランス重視）
  Performance,    ///< 高性能重視
  Size,           ///< サイズ重視
  Custom          ///< カスタム設定
};

/**
 * @struct TransformPipelineConfig
 * @brief パイプライン設定
 */
struct TransformPipelineConfig {
  OptimizationProfile profile = OptimizationProfile::Default;
  bool enableSIMD = true;
  bool enableMultithreading = true;
  bool enableProfiling = false;
  bool detectHardware = true;
  size_t threadCount = 0;  // 0 = 自動検出
  std::string configFilePath;
  std::unordered_map<std::string, bool> transformerEnabledMap;
};

/**
 * @class TransformPipelineBuilder
 * @brief トランスフォーマーパイプラインを構築するビルダークラス
 * 
 * @details
 * 最適化プロファイルに基づいて適切なトランスフォーマー群を構成し、
 * 最適化パイプラインを構築します。ハードウェア検出と自動設定機能を提供します。
 */
class TransformPipelineBuilder {
public:
  /**
   * @brief デフォルト設定でビルダーを作成
   */
  TransformPipelineBuilder();
  
  /**
   * @brief 指定した設定でビルダーを作成
   * 
   * @param config パイプライン設定
   */
  explicit TransformPipelineBuilder(const TransformPipelineConfig& config);
  
  /**
   * @brief 設定をセット
   * 
   * @param config パイプライン設定
   * @return *this （チェーン呼び出し用）
   */
  TransformPipelineBuilder& SetConfig(const TransformPipelineConfig& config);
  
  /**
   * @brief 最適化プロファイルを設定
   * 
   * @param profile 最適化プロファイル
   * @return *this （チェーン呼び出し用）
   */
  TransformPipelineBuilder& WithProfile(OptimizationProfile profile);
  
  /**
   * @brief マルチスレッド設定
   * 
   * @param enable 有効にするかどうか
   * @param threadCount スレッド数（0=自動）
   * @return *this （チェーン呼び出し用）
   */
  TransformPipelineBuilder& WithMultithreading(bool enable, size_t threadCount = 0);
  
  /**
   * @brief SIMD設定
   * 
   * @param enable 有効にするかどうか
   * @return *this （チェーン呼び出し用）
   */
  TransformPipelineBuilder& WithSIMD(bool enable);
  
  /**
   * @brief プロファイリング設定
   * 
   * @param enable 有効にするかどうか
   * @return *this （チェーン呼び出し用）
   */
  TransformPipelineBuilder& WithProfiling(bool enable);
  
  /**
   * @brief 特定のトランスフォーマーの有効/無効を設定
   * 
   * @param name トランスフォーマー名
   * @param enabled 有効にするかどうか
   * @return *this （チェーン呼び出し用）
   */
  TransformPipelineBuilder& SetTransformerEnabled(const std::string& name, bool enabled);
  
  /**
   * @brief 設定ファイルを読み込む
   * 
   * @param filePath JSONファイルのパス
   * @return *this （チェーン呼び出し用）
   */
  TransformPipelineBuilder& LoadConfigFile(const std::string& filePath);
  
  /**
   * @brief トランスフォーマーパイプラインを構築
   * 
   * @return トランスフォーマーのベクトル
   */
  std::vector<std::unique_ptr<Transformer>> Build();
  
  /**
   * @brief 利用可能なトランスフォーマー名の一覧を取得
   * 
   * @return トランスフォーマー名のベクトル
   */
  std::vector<std::string> GetAvailableTransformers() const;

private:
  TransformPipelineConfig m_config;
  std::unique_ptr<utils::platform::CPUFeatures> m_cpuFeatures;
  bool m_isInitialized;
  
  /**
   * @brief ハードウェア機能を検出
   */
  void DetectHardware();
  
  /**
   * @brief 設定ファイルを解析
   * 
   * @param filePath ファイルパス
   * @return 設定ファイル解析成功ならtrue
   */
  bool ParseConfigFile(const std::string& filePath);
  
  /**
   * @brief プロファイルに基づいてデフォルトのトランスフォーマーセットを構築
   * 
   * @param transformers 追加先のトランスフォーマーコンテナ
   */
  void BuildDefaultTransformers(std::vector<std::unique_ptr<Transformer>>& transformers);
  
  /**
   * @brief アーキテクチャ固有の最適化を追加
   * 
   * @param transformers 追加先のトランスフォーマーコンテナ
   */
  void AddArchitectureSpecificTransformers(std::vector<std::unique_ptr<Transformer>>& transformers);
  
  /**
   * @brief パフォーマンスプロファイル用のトランスフォーマーを追加
   * 
   * @param transformers 追加先のトランスフォーマーコンテナ
   */
  void AddPerformanceTransformers(std::vector<std::unique_ptr<Transformer>>& transformers);
  
  /**
   * @brief サイズ最適化プロファイル用のトランスフォーマーを追加
   * 
   * @param transformers 追加先のトランスフォーマーコンテナ
   */
  void AddSizeOptimizationTransformers(std::vector<std::unique_ptr<Transformer>>& transformers);
  
  /**
   * @brief デバッグプロファイル用のトランスフォーマーを追加
   * 
   * @param transformers 追加先のトランスフォーマーコンテナ
   */
  void AddDebugTransformers(std::vector<std::unique_ptr<Transformer>>& transformers);
  
  /**
   * @brief カスタム設定用のトランスフォーマーを追加
   * 
   * @param transformers 追加先のトランスフォーマーコンテナ
   */
  void AddCustomTransformers(std::vector<std::unique_ptr<Transformer>>& transformers);
};

} // namespace aerojs::transformers 