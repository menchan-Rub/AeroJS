/**
 * @file cpu_features.h
 * @brief AeroJS JavaScript エンジンのCPU機能検出ヘルパー
 * @version 1.0.0
 * @license MIT
 */

#ifndef AEROJS_UTILS_PLATFORM_CPU_FEATURES_H
#define AEROJS_UTILS_PLATFORM_CPU_FEATURES_H

#include <cstdint>
#include <string>
#include <vector>

#if defined(_MSC_VER)
  #include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
  #include <cpuid.h>
  #include <x86intrin.h>
#endif

namespace aerojs {
namespace utils {

/**
 * @brief CPU機能検出クラス
 *
 * 実行時にCPUの機能（SIMD対応など）を検出し、
 * それに基づいて最適なコード実行経路を選択するためのヘルパークラス
 */
class CPUFeatures {
public:
  /**
   * @brief 利用可能なCPU機能のフラグ定義
   */
  enum Feature : uint64_t {
    SSE     = 1ULL << 0,
    SSE2    = 1ULL << 1,
    SSE3    = 1ULL << 2,
    SSSE3   = 1ULL << 3,
    SSE4_1  = 1ULL << 4,
    SSE4_2  = 1ULL << 5,
    AVX     = 1ULL << 6,
    AVX2    = 1ULL << 7,
    AVX512F = 1ULL << 8,
    FMA     = 1ULL << 9,
    AES     = 1ULL << 10,
    PCLMUL  = 1ULL << 11,
    POPCNT  = 1ULL << 12,
    BMI1    = 1ULL << 13,
    BMI2    = 1ULL << 14,
    LZCNT   = 1ULL << 15,
    F16C    = 1ULL << 16,
    MOVBE   = 1ULL << 17,
    NEON    = 1ULL << 20,  // ARM向け
    SVE     = 1ULL << 21,  // ARM SVE
    RV_V    = 1ULL << 22,  // RISC-V ベクトル拡張
  };

  /**
   * @brief CPU機能検出を実行（シングルトン）
   * @return 検出されたCPU機能フラグ
   */
  static uint64_t detect();

  /**
   * @brief 特定の機能が利用可能かどうかを検査
   * @param feature 検査する機能フラグ
   * @return 利用可能な場合はtrue
   */
  static bool hasFeature(Feature feature);

  /**
   * @brief 利用可能なCPU機能をリストで取得
   * @return 機能名の文字列リスト
   */
  static std::vector<std::string> getAvailableFeatures();

  /**
   * @brief CPU名を取得
   * @return CPU名
   */
  static std::string getCPUName();

  /**
   * @brief ハードウェアスレッド数を取得
   * @return 利用可能なハードウェアスレッド数
   */
  static int getNumHardwareThreads();

  /**
   * @brief 物理コア数を取得
   * @return 物理コア数
   */
  static int getNumPhysicalCores();

  /**
   * @brief キャッシュラインサイズを取得
   * @return キャッシュラインサイズ（バイト単位）
   */
  static int getCacheLineSize();

  /**
   * @brief L1キャッシュサイズを取得
   * @return L1キャッシュサイズ（バイト単位）
   */
  static int getL1CacheSize();

  /**
   * @brief L2キャッシュサイズを取得
   * @return L2キャッシュサイズ（バイト単位）
   */
  static int getL2CacheSize();

  /**
   * @brief L3キャッシュサイズを取得
   * @return L3キャッシュサイズ（バイト単位）
   */
  static int getL3CacheSize();

private:
  static uint64_t s_features;  // 検出された機能フラグ
  static bool s_initialized;   // 初期化済みフラグ

  // CPUID呼び出しヘルパー
  static void cpuidex(int result[4], int eax, int ecx);

  // 初期化メソッド
  static void initialize();

  // x86/x64向け機能検出
  static void detectX86Features();

  // ARM向け機能検出
  static void detectARMFeatures();

  // RISC-V向け機能検出
  static void detectRISCVFeatures();
};

// インライン実装

inline bool CPUFeatures::hasFeature(Feature feature) {
  if (!s_initialized) {
    initialize();
  }
  return (s_features & static_cast<uint64_t>(feature)) != 0;
}

inline uint64_t CPUFeatures::detect() {
  if (!s_initialized) {
    initialize();
  }
  return s_features;
}

}  // namespace utils
}  // namespace aerojs

#endif  // AEROJS_UTILS_PLATFORM_CPU_FEATURES_H 