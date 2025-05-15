/**
 * @file simd_operations.h
 * @brief AeroJS JavaScript エンジンのSIMD操作ヘルパー
 * @version 1.0.0
 * @license MIT
 */

#ifndef AEROJS_UTILS_PLATFORM_SIMD_OPERATIONS_H
#define AEROJS_UTILS_PLATFORM_SIMD_OPERATIONS_H

#include <cstdint>
#include <array>
#include "cpu_features.h"

// アーキテクチャに基づくSIMDヘッダのインクルード
#if defined(_MSC_VER)
  #include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
  #include <x86intrin.h>
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  #include <arm_neon.h>
#endif

namespace aerojs {
namespace utils {

/**
 * @brief SIMD操作を提供するヘルパークラス
 *
 * x86-64向けの各種SIMD命令セット（SSE/AVX/AVX2/AVX-512）および
 * ARM向けNEONベクトル命令セットに対する統一インターフェイスを提供し、
 * 利用可能な最高性能のSIMD操作を実行時に自動選択します。
 */
class SIMDOperations {
public:
  /**
   * @brief SSE命令セットが利用可能か判定
   * @return 利用可能ならtrue
   */
  static bool isSSESupported() {
    return CPUFeatures::hasFeature(CPUFeatures::SSE);
  }
  
  /**
   * @brief AVX命令セットが利用可能か判定
   * @return 利用可能ならtrue
   */
  static bool isAVXSupported() {
    return CPUFeatures::hasFeature(CPUFeatures::AVX);
  }
  
  /**
   * @brief AVX2命令セットが利用可能か判定
   * @return 利用可能ならtrue
   */
  static bool isAVX2Supported() {
    return CPUFeatures::hasFeature(CPUFeatures::AVX2);
  }
  
  /**
   * @brief AVX-512命令セットが利用可能か判定
   * @return 利用可能ならtrue
   */
  static bool isAVX512Supported() {
    return CPUFeatures::hasFeature(CPUFeatures::AVX512F);
  }
  
  /**
   * @brief NEON命令セットが利用可能か判定（ARM）
   * @return 利用可能ならtrue
   */
  static bool isNEONSupported() {
    return CPUFeatures::hasFeature(CPUFeatures::NEON);
  }

  /**
   * @brief 4つの浮動小数点数の加算（SSE）
   * @param a 第1オペランド
   * @param b 第2オペランド
   * @return 結果の4要素配列
   */
  static std::array<float, 4> addFloat4(const std::array<float, 4>& a, 
                                       const std::array<float, 4>& b) {
    std::array<float, 4> result;
    
    if (isSSESupported()) {
      // SSE実装
      __m128 va = _mm_loadu_ps(a.data());
      __m128 vb = _mm_loadu_ps(b.data());
      __m128 vresult = _mm_add_ps(va, vb);
      _mm_storeu_ps(result.data(), vresult);
    } else {
      // フォールバック実装
      for (int i = 0; i < 4; ++i) {
        result[i] = a[i] + b[i];
      }
    }
    
    return result;
  }
  
  /**
   * @brief 8つの浮動小数点数の加算（AVX）
   * @param a 第1オペランド
   * @param b 第2オペランド
   * @return 結果の8要素配列
   */
  static std::array<float, 8> addFloat8(const std::array<float, 8>& a, 
                                       const std::array<float, 8>& b) {
    std::array<float, 8> result;
    
    if (isAVXSupported()) {
      // AVX実装
      __m256 va = _mm256_loadu_ps(a.data());
      __m256 vb = _mm256_loadu_ps(b.data());
      __m256 vresult = _mm256_add_ps(va, vb);
      _mm256_storeu_ps(result.data(), vresult);
    } else {
      // SSEにフォールバック
      for (int i = 0; i < 8; i += 4) {
        std::array<float, 4> subA, subB, subResult;
        for (int j = 0; j < 4; ++j) {
          subA[j] = a[i + j];
          subB[j] = b[i + j];
        }
        subResult = addFloat4(subA, subB);
        for (int j = 0; j < 4; ++j) {
          result[i + j] = subResult[j];
        }
      }
    }
    
    return result;
  }
  
  /**
   * @brief 4つの整数の加算（SSE2）
   * @param a 第1オペランド
   * @param b 第2オペランド
   * @return 結果の4要素配列
   */
  static std::array<int32_t, 4> addInt4(const std::array<int32_t, 4>& a, 
                                        const std::array<int32_t, 4>& b) {
    std::array<int32_t, 4> result;
    
    if (CPUFeatures::hasFeature(CPUFeatures::SSE2)) {
      // SSE2実装
      __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a.data()));
      __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b.data()));
      __m128i vresult = _mm_add_epi32(va, vb);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data()), vresult);
    } else {
      // フォールバック実装
      for (int i = 0; i < 4; ++i) {
        result[i] = a[i] + b[i];
      }
    }
    
    return result;
  }
  
  /**
   * @brief 8つの整数の加算（AVX2）
   * @param a 第1オペランド
   * @param b 第2オペランド
   * @return 結果の8要素配列
   */
  static std::array<int32_t, 8> addInt8(const std::array<int32_t, 8>& a, 
                                        const std::array<int32_t, 8>& b) {
    std::array<int32_t, 8> result;
    
    if (isAVX2Supported()) {
      // AVX2実装
      __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a.data()));
      __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b.data()));
      __m256i vresult = _mm256_add_epi32(va, vb);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data()), vresult);
    } else {
      // SSE2にフォールバック
      for (int i = 0; i < 8; i += 4) {
        std::array<int32_t, 4> subA, subB, subResult;
        for (int j = 0; j < 4; ++j) {
          subA[j] = a[i + j];
          subB[j] = b[i + j];
        }
        subResult = addInt4(subA, subB);
        for (int j = 0; j < 4; ++j) {
          result[i + j] = subResult[j];
        }
      }
    }
    
    return result;
  }
  
  /**
   * @brief 4つの浮動小数点数の乗算（SSE）
   * @param a 第1オペランド
   * @param b 第2オペランド
   * @return 結果の4要素配列
   */
  static std::array<float, 4> mulFloat4(const std::array<float, 4>& a, 
                                       const std::array<float, 4>& b) {
    std::array<float, 4> result;
    
    if (isSSESupported()) {
      // SSE実装
      __m128 va = _mm_loadu_ps(a.data());
      __m128 vb = _mm_loadu_ps(b.data());
      __m128 vresult = _mm_mul_ps(va, vb);
      _mm_storeu_ps(result.data(), vresult);
    } else {
      // フォールバック実装
      for (int i = 0; i < 4; ++i) {
        result[i] = a[i] * b[i];
      }
    }
    
    return result;
  }
  
  /**
   * @brief 乗算加算融合（FMA）演算：a * b + c
   * @param a 第1オペランド
   * @param b 第2オペランド
   * @param c 第3オペランド
   * @return 結果の4要素配列
   */
  static std::array<float, 4> fmaFloat4(const std::array<float, 4>& a,
                                       const std::array<float, 4>& b,
                                       const std::array<float, 4>& c) {
    std::array<float, 4> result;
    
    if (CPUFeatures::hasFeature(CPUFeatures::FMA)) {
      // FMA3実装
      __m128 va = _mm_loadu_ps(a.data());
      __m128 vb = _mm_loadu_ps(b.data());
      __m128 vc = _mm_loadu_ps(c.data());
      
      // a * b + c
      __m128 vresult = _mm_fmadd_ps(va, vb, vc);
      _mm_storeu_ps(result.data(), vresult);
    } else {
      // SSEフォールバック
      __m128 va = _mm_loadu_ps(a.data());
      __m128 vb = _mm_loadu_ps(b.data());
      __m128 vc = _mm_loadu_ps(c.data());
      
      // a * b + c を二つの命令で実行
      __m128 tmp = _mm_mul_ps(va, vb);
      __m128 vresult = _mm_add_ps(tmp, vc);
      _mm_storeu_ps(result.data(), vresult);
    }
    
    return result;
  }
  
  /**
   * @brief 4つの値の等価比較（SSE）
   * @param a 第1オペランド
   * @param b 第2オペランド
   * @return 結果の4要素配列（等しければtrue）
   */
  static std::array<bool, 4> compareEqFloat4(const std::array<float, 4>& a,
                                            const std::array<float, 4>& b) {
    std::array<bool, 4> result;
    
    if (isSSESupported()) {
      // SSE実装
      __m128 va = _mm_loadu_ps(a.data());
      __m128 vb = _mm_loadu_ps(b.data());
      __m128 vcmp = _mm_cmpeq_ps(va, vb);
      
      // マスクに変換
      int mask = _mm_movemask_ps(vcmp);
      for (int i = 0; i < 4; ++i) {
        result[i] = (mask & (1 << i)) != 0;
      }
    } else {
      // フォールバック実装
      for (int i = 0; i < 4; ++i) {
        result[i] = (a[i] == b[i]);
      }
    }
    
    return result;
  }
  
  /**
   * @brief 指定の浮動小数点値を持つレジスタを作成（AVX）
   * @param value 各要素に設定する値
   * @return 設定された8要素のレジスタ
   */
  static __m256 broadcastFloat8(float value) {
    if (isAVXSupported()) {
      return _mm256_set1_ps(value);
    } else {
      // コンパイルエラー回避のためのダミー
      return _mm256_setzero_ps();
    }
  }
  
  /**
   * @brief メモリアライメントを確認
   * @param ptr メモリポインタ
   * @param alignment アライメント要件（バイト単位、通常は16または32）
   * @return 指定のアライメントに準拠していればtrue
   */
  static bool isAligned(const void* ptr, size_t alignment) {
    return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
  }
  
  /**
   * @brief プリフェッチ操作（キャッシュパフォーマンス向上）
   * @param ptr プリフェッチするメモリ位置
   * @param locality 局所性ヒント（0=非時間的、3=高い時間的局所性）
   */
  static void prefetch(const void* ptr, int locality = 3) {
    // x86/x64環境
#if defined(_MSC_VER)
    switch (locality) {
      case 0: _mm_prefetch(static_cast<const char*>(ptr), _MM_HINT_NTA); break;
      case 1: _mm_prefetch(static_cast<const char*>(ptr), _MM_HINT_T2); break;
      case 2: _mm_prefetch(static_cast<const char*>(ptr), _MM_HINT_T1); break;
      case 3: _mm_prefetch(static_cast<const char*>(ptr), _MM_HINT_T0); break;
    }
#elif defined(__GNUC__) || defined(__clang__)
    switch (locality) {
      case 0: __builtin_prefetch(ptr, 0, 0); break;
      case 1: __builtin_prefetch(ptr, 0, 1); break;
      case 2: __builtin_prefetch(ptr, 0, 2); break;
      case 3: __builtin_prefetch(ptr, 0, 3); break;
    }
#endif
  }
};

}  // namespace utils
}  // namespace aerojs

#endif  // AEROJS_UTILS_PLATFORM_SIMD_OPERATIONS_H 