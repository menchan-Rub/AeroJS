/**
 * @file riscv_array_optimizations.h
 * @version 1.0.0
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 * @brief RISC-Vアーキテクチャ向けの配列最適化ユーティリティ
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "src/core/parser/ast/nodes/node.h"
#include "src/utils/platform/cpu_features.h"

namespace aerojs::transformers::arch {

/**
 * @enum RISCVFeatures
 * @brief サポートするRISC-V拡張命令セット
 */
enum class RISCVFeatures : uint32_t {
  None         = 0,
  RV32I        = 1 << 0,  ///< 基本整数命令セット (32ビット)
  RV64I        = 1 << 1,  ///< 基本整数命令セット (64ビット)
  M            = 1 << 2,  ///< 整数乗算・除算
  A            = 1 << 3,  ///< アトミック操作
  F            = 1 << 4,  ///< 単精度浮動小数点
  D            = 1 << 5,  ///< 倍精度浮動小数点
  C            = 1 << 6,  ///< 圧縮命令
  V            = 1 << 7,  ///< ベクトル拡張
  B            = 1 << 8,  ///< ビット操作拡張
  P            = 1 << 9,  ///< パックドSIMD命令
  Zba          = 1 << 10, ///< アドレス計算拡張
  Zbb          = 1 << 11, ///< 基本ビット操作
  Zbc          = 1 << 12, ///< キャリーレスオペレーション
  Zbkb         = 1 << 13, ///< ビット操作（暗号用）
  Zbkc         = 1 << 14, ///< 暗号用クルプトサポート
  Zbkx         = 1 << 15, ///< 暗号用拡張命令
  Zbs          = 1 << 16, ///< シングルビット操作
  Zfh          = 1 << 17, ///< 半精度浮動小数点
  Zve32x       = 1 << 18, ///< ベクトル拡張（32ビット整数）
  Zve64x       = 1 << 19  ///< ベクトル拡張（64ビット整数）
};

/**
 * @struct RISCVVectorConfig
 * @brief RISC-V ベクトル拡張の設定
 */
struct RISCVVectorConfig {
  uint32_t vlen = 0;      ///< ベクトルレジスタ長 (ビット)
  uint32_t elen = 0;      ///< 最大要素サイズ (ビット)
  uint32_t slen = 0;      ///< ストライドビット長
  bool hardfloat = false; ///< ハードウェア浮動小数点サポート
  
  /// デバイスがRV Vectorをサポートしているか
  bool SupportsVector() const { return vlen > 0; }
  
  /// 8ビット要素で一度に何要素処理できるか
  uint32_t GetVectorElementCountI8() const { return vlen / 8; }
  
  /// 32ビット要素で一度に何要素処理できるか
  uint32_t GetVectorElementCountI32() const { return vlen / 32; }
  
  /// 64ビット要素で一度に何要素処理できるか
  uint32_t GetVectorElementCountI64() const { return vlen / 64; }
  
  /// 32ビット浮動小数点で一度に何要素処理できるか
  uint32_t GetVectorElementCountF32() const { 
    return hardfloat ? (vlen / 32) : 0; 
  }
  
  /// 64ビット浮動小数点で一度に何要素処理できるか
  uint32_t GetVectorElementCountF64() const { 
    return hardfloat ? (vlen / 64) : 0; 
  }
};

/**
 * @class RISCVArrayOptimizations
 * @brief RISC-V向けの配列操作最適化ユーティリティ
 */
class RISCVArrayOptimizations {
public:
  /**
   * @brief コンストラクタ
   */
  RISCVArrayOptimizations();
  
  /**
   * @brief 利用可能なRISCV機能を検出
   * 
   * @return 検出された機能のビットフラグ
   */
  uint32_t DetectFeatures();
  
  /**
   * @brief ベクトル拡張の設定を取得
   * 
   * @return ベクトル設定構造体
   */
  const RISCVVectorConfig& GetVectorConfig() const;
  
  /**
   * @brief 指定した機能がサポートされているか確認
   * 
   * @param feature 確認する機能
   * @return サポートされていればtrue
   */
  bool HasFeature(RISCVFeatures feature) const;
  
  /**
   * @brief ASTノードに対してRISC-Vベクトル最適化を適用できるか判定
   * 
   * @param node 評価対象のノード
   * @return 最適化可能ならtrue
   */
  bool CanApplyVectorization(const parser::ast::NodePtr& node) const;
  
  /**
   * @brief ASTノードに対してRISC-Vベクトル最適化を適用
   * 
   * @param node 対象ノード
   * @return 最適化されたノード（最適化できなければ元のノード）
   */
  parser::ast::NodePtr ApplyVectorization(const parser::ast::NodePtr& node);
  
  /**
   * @brief 配列メソッド向けのRISC-V最適化コードを生成
   * 
   * @param methodName メソッド名 ("map", "filter"など)
   * @param elementType 要素の型
   * @param isSimple 単純な最適化可能な操作かどうか
   * @return 最適化コード名 (例: "rvv_map_f64")
   */
  std::string GetOptimizedArrayMethodName(
      const std::string& methodName,
      const std::string& elementType,
      bool isSimple) const;
  
  /**
   * @brief RISC-Vが利用可能か確認
   * 
   * @return RISC-Vアーキテクチャならtrue
   */
  static bool IsRISCVArchitecture();

private:
  uint32_t m_features;
  RISCVVectorConfig m_vectorConfig;
  bool m_initialized;
  
  /**
   * @brief RISC-V ベクトル拡張の設定を検出
   */
  void DetectVectorConfig();
  
  /**
   * @brief ベクトル最適化可能なコードパターンを検出
   * 
   * @param node 検査対象ノード
   * @return パターンが検出されたらtrue
   */
  bool DetectVectorizablePattern(const parser::ast::NodePtr& node) const;
  
  /**
   * @brief ベクトル操作向けに配列要素のデータ型を判定
   * 
   * @param node 配列式または要素参照ノード
   * @return データ型名 ("i32", "f64"など)
   */
  std::string DetermineElementType(const parser::ast::NodePtr& node) const;
};

} // namespace aerojs::transformers::arch 