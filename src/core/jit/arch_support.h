/**
 * @file arch_support.h
 * @brief アーキテクチャサポート機能の定義
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include <cstdint>

namespace aerojs {
namespace core {

// ターゲットアーキテクチャの判定マクロ
#if defined(__x86_64__) || defined(_M_X64)
    #define AEROJS_TARGET_X86_64 1
#else
    #define AEROJS_TARGET_X86_64 0
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
    #define AEROJS_TARGET_ARM64 1
#else
    #define AEROJS_TARGET_ARM64 0
#endif

#if defined(__riscv)
    #define AEROJS_TARGET_RISCV 1
#else
    #define AEROJS_TARGET_RISCV 0
#endif

// システムのネイティブアーキテクチャを判定
#if AEROJS_TARGET_X86_64
    #define AEROJS_NATIVE_ARCH "x86_64"
#elif AEROJS_TARGET_ARM64
    #define AEROJS_NATIVE_ARCH "arm64"
#elif AEROJS_TARGET_RISCV
    #define AEROJS_NATIVE_ARCH "riscv"
#else
    #define AEROJS_NATIVE_ARCH "unknown"
    #error "Unsupported architecture"
#endif

/**
 * @brief アーキテクチャサポート機能の基本定義
 */
class ArchSupport {
public:
    /**
     * @brief サポートするアーキテクチャを確認
     * @return サポートされるアーキテクチャの名称リスト
     */
    static const char* getSupportedArchitectures() {
        return "x86_64, arm64, riscv";
    }
    
    /**
     * @brief 現在のネイティブアーキテクチャを取得
     * @return ネイティブアーキテクチャの名称
     */
    static const char* getNativeArchitecture() {
        return AEROJS_NATIVE_ARCH;
    }
    
    /**
     * @brief アーキテクチャがSIMD命令をサポートしているか確認
     * @param arch アーキテクチャ名
     * @return SIMDサポートの有無
     */
    static bool hasSIMDSupport(const char* arch) {
        if (strcmp(arch, "x86_64") == 0) {
            return true;  // SSE/AVX
        } else if (strcmp(arch, "arm64") == 0) {
            return true;  // NEON
        } else if (strcmp(arch, "riscv") == 0) {
            return true;  // Vector Extension
        }
        return false;
    }
    
    /**
     * @brief アーキテクチャのネイティブワードサイズを取得
     * @param arch アーキテクチャ名
     * @return ワードサイズ (ビット)
     */
    static int getWordSize(const char* arch) {
        if (strcmp(arch, "x86_64") == 0 || 
            strcmp(arch, "arm64") == 0 || 
            strcmp(arch, "riscv") == 0) {
            return 64;
        }
        return 32;  // デフォルト
    }
};

} // namespace core
} // namespace aerojs 