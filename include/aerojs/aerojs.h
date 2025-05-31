/**
 * @file aerojs.h
 * @brief AeroJS 世界最高性能JavaScriptエンジンのメインヘッダーファイル
 * @version 2.0.0
 * @license MIT
 */

#ifndef AEROJS_H
#define AEROJS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * バージョン情報
 */
#define AEROJS_VERSION_MAJOR 2
#define AEROJS_VERSION_MINOR 0
#define AEROJS_VERSION_PATCH 0
#define AEROJS_VERSION_STRING "2.0.0"

/**
 * ビルド設定マクロ
 */
#if defined(_WIN32) || defined(_WIN64)
#define AEROJS_PLATFORM_WINDOWS
#if defined(_MSC_VER)
#define AEROJS_COMPILER_MSVC
#ifdef AEROJS_BUILDING_DLL
#define AEROJS_EXPORT __declspec(dllexport)
#else
#define AEROJS_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define AEROJS_COMPILER_GCC
#define AEROJS_EXPORT __attribute__((visibility("default")))
#endif
#elif defined(__APPLE__)
#define AEROJS_PLATFORM_MACOS
#define AEROJS_EXPORT __attribute__((visibility("default")))
#define AEROJS_COMPILER_CLANG
#elif defined(__linux__)
#define AEROJS_PLATFORM_LINUX
#define AEROJS_EXPORT __attribute__((visibility("default")))
#if defined(__GNUC__)
#define AEROJS_COMPILER_GCC
#elif defined(__clang__)
#define AEROJS_COMPILER_CLANG
#endif
#elif defined(__FreeBSD__)
#define AEROJS_PLATFORM_FREEBSD
#define AEROJS_EXPORT __attribute__((visibility("default")))
#if defined(__GNUC__)
#define AEROJS_COMPILER_GCC
#elif defined(__clang__)
#define AEROJS_COMPILER_CLANG
#endif
#else
#error "未サポートのプラットフォームです"
#endif

#if !defined(AEROJS_EXPORT)
#define AEROJS_EXPORT
#endif

/**
 * コンパイラ固有の属性
 */
#if defined(AEROJS_COMPILER_GCC) || defined(AEROJS_COMPILER_CLANG)
#define AEROJS_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#define AEROJS_NOINLINE __attribute__((noinline))
#define AEROJS_ALWAYS_INLINE __attribute__((always_inline))
#define AEROJS_NORETURN __attribute__((noreturn))
#define AEROJS_LIKELY(x) __builtin_expect(!!(x), 1)
#define AEROJS_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define AEROJS_PRINTF_FORMAT(fmt, args) __attribute__((format(printf, fmt, args)))
#elif defined(AEROJS_COMPILER_MSVC)
#define AEROJS_WARN_UNUSED_RESULT
#define AEROJS_NOINLINE __declspec(noinline)
#define AEROJS_ALWAYS_INLINE __forceinline
#define AEROJS_NORETURN __declspec(noreturn)
#define AEROJS_LIKELY(x) (x)
#define AEROJS_UNLIKELY(x) (x)
#define AEROJS_PRINTF_FORMAT(fmt, args)
#else
#define AEROJS_WARN_UNUSED_RESULT
#define AEROJS_NOINLINE
#define AEROJS_ALWAYS_INLINE
#define AEROJS_NORETURN
#define AEROJS_LIKELY(x) (x)
#define AEROJS_UNLIKELY(x) (x)
#define AEROJS_PRINTF_FORMAT(fmt, args)
#endif

/**
 * プロセッサアーキテクチャ検出
 */
#if defined(__x86_64__) || defined(_M_X64)
#define AEROJS_ARCH_X86_64
#elif defined(__i386__) || defined(_M_IX86)
#define AEROJS_ARCH_X86
#elif defined(__aarch64__) || defined(_M_ARM64)
#define AEROJS_ARCH_ARM64
#elif defined(__arm__) || defined(_M_ARM)
#define AEROJS_ARCH_ARM32
#elif defined(__riscv) && (__riscv_xlen == 64)
#define AEROJS_ARCH_RISCV64
#elif defined(__powerpc64__)
#define AEROJS_ARCH_PPC64
#else
#define AEROJS_ARCH_UNKNOWN
#endif

/**
 * アーキテクチャ固有の機能検出
 */
#if defined(AEROJS_ARCH_ARM64)
#if defined(__ARM_FEATURE_SVE)
#define AEROJS_FEATURE_SVE
#endif
#if defined(__ARM_FEATURE_DOTPROD)
#define AEROJS_FEATURE_DOTPROD
#endif
#if defined(__ARM_FEATURE_JCVT)
#define AEROJS_FEATURE_JSCVT
#endif
#if defined(__ARM_FEATURE_CRC32)
#define AEROJS_FEATURE_CRC32
#endif
#if defined(__ARM_FEATURE_CRYPTO)
#define AEROJS_FEATURE_CRYPTO
#endif
#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
#define AEROJS_FEATURE_FP16
#endif
#if defined(__ARM_FEATURE_BF16_VECTOR_ARITHMETIC)
#define AEROJS_FEATURE_BF16
#endif
#endif

/**
 * 基本型定義
 */
typedef int32_t AerojsInt32;
typedef uint32_t AerojsUInt32;
typedef int64_t AerojsInt64;
typedef uint64_t AerojsUInt64;
typedef float AerojsFloat32;
typedef double AerojsFloat64;
typedef uint8_t AerojsByte;
typedef uint16_t AerojsUInt16;
typedef int16_t AerojsInt16;
typedef size_t AerojsSize;
typedef int AerojsBool;
typedef void* AerojsPtr;

#define AEROJS_TRUE 1
#define AEROJS_FALSE 0
#define AEROJS_NULL NULL

/**
 * ステータスコード
 */
typedef enum {
  AEROJS_SUCCESS = 0,
  AEROJS_ERROR_INVALID_ARGUMENT = -1,
  AEROJS_ERROR_OUT_OF_MEMORY = -2,
  AEROJS_ERROR_SYNTAX = -3,
  AEROJS_ERROR_REFERENCE = -4,
  AEROJS_ERROR_TYPE = -5,
  AEROJS_ERROR_RANGE = -6,
  AEROJS_ERROR_INTERNAL = -7,
  AEROJS_ERROR_NOT_IMPLEMENTED = -8,
  AEROJS_ERROR_JIT_COMPILATION = -9,
  AEROJS_ERROR_STACK_OVERFLOW = -10,
  AEROJS_ERROR_RUNTIME_LIMIT = -11,
  AEROJS_ERROR_SECURITY = -12,
  AEROJS_ERROR_NETWORK = -13,
  AEROJS_ERROR_IO = -14,
  AEROJS_ERROR_MODULE_NOT_FOUND = -15
} AerojsStatus;

/**
 * 前方宣言
 */
typedef struct AerojsEngine AerojsEngine;
typedef struct AerojsContext AerojsContext;
typedef struct AerojsValue AerojsValue;
typedef struct AerojsObject AerojsObject;
typedef struct AerojsFunction AerojsFunction;
typedef struct AerojsException AerojsException;
typedef struct AerojsArray AerojsArray;
typedef struct AerojsString AerojsString;

typedef AerojsValue* AerojsValueRef;

/**
 * エンジン作成と破棄
 */
AEROJS_EXPORT AerojsEngine* AerojsCreateEngine(void);
AEROJS_EXPORT void AerojsDestroyEngine(AerojsEngine* engine);

/**
 * バージョン情報取得
 */
AEROJS_EXPORT const char* AerojsGetVersion(void);
AEROJS_EXPORT void AerojsGetVersionInfo(AerojsUInt32* major, AerojsUInt32* minor, AerojsUInt32* patch);

/**
 * エンジン初期化時の設定オプション
 */
typedef struct {
  AerojsSize initialHeapSize;      /* 初期ヒープサイズ（バイト単位） */
  AerojsSize maximumHeapSize;      /* 最大ヒープサイズ（バイト単位） */
  AerojsSize stackSize;            /* スタックサイズ（バイト単位） */
  AerojsBool enableJIT;            /* JITコンパイルを有効にするかどうか */
  AerojsBool enableGC;             /* GCを有効にするかどうか */
  AerojsBool enableDebugger;       /* デバッガを有効にするかどうか */
  AerojsUInt32 gcThreshold;        /* GCが発生するしきい値（バイト単位） */
  AerojsUInt32 gcFrequency;        /* GCの頻度（ミリ秒単位） */
  const char* scriptCacheDir;      /* スクリプトキャッシュディレクトリ */
  AerojsUInt32 optimizationLevel;  /* 最適化レベル（0-3） */
  AerojsUInt32 jitThreshold;       /* JITコンパイルが発生するしきい値（関数実行回数） */
  AerojsBool enableWasm;           /* WebAssemblyサポートを有効にするかどうか */
  AerojsUInt32 maxCompilationThreads; /* JITコンパイル用スレッド数（0=自動） */
  AerojsBool enableSuperOptimizer; /* 超最適化を有効にするかどうか */
  AerojsUInt32 superOptimizationLevel; /* 超最適化レベル（0-4） */
  AerojsBool enableHardwareOptimizations; /* ハードウェア固有の最適化を有効にするかどうか */
  AerojsBool enableMetatracing;    /* メタトレーシング最適化を有効にするかどうか */
  AerojsBool enableSpeculativeOpts; /* スペキュレーティブ最適化を有効にするかどうか */
  AerojsBool enableProfileGuidedOpts; /* プロファイル誘導型最適化を有効にするかどうか */
  AerojsSize codeCacheSize;        /* JITコードキャッシュサイズの最大値（バイト単位） */
} AerojsEngineConfig;

/* デフォルト設定の取得 */
AEROJS_EXPORT void AerojsGetDefaultEngineConfig(AerojsEngineConfig* config);

/* 設定を使用したエンジンの作成 */
AEROJS_EXPORT AerojsEngine* AerojsCreateEngineWithConfig(const AerojsEngineConfig* config);

/**
 * プロセッサベンダー
 */
typedef enum {
  AEROJS_CPU_VENDOR_UNKNOWN = 0,
  AEROJS_CPU_VENDOR_INTEL,
  AEROJS_CPU_VENDOR_AMD,
  AEROJS_CPU_VENDOR_ARM,
  AEROJS_CPU_VENDOR_APPLE,
  AEROJS_CPU_VENDOR_QUALCOMM,
  AEROJS_CPU_VENDOR_AMPERE,
  AEROJS_CPU_VENDOR_NVIDIA,
  AEROJS_CPU_VENDOR_SAMSUNG,
  AEROJS_CPU_VENDOR_HUAWEI,
  AEROJS_CPU_VENDOR_FUJITSU,
  AEROJS_CPU_VENDOR_MARVELL
} AerojsCPUVendor;

/**
 * プロセッサ情報
 */
typedef struct {
  AerojsCPUVendor vendor;          /* プロセッサベンダー */
  char vendorString[64];           /* ベンダー文字列 */
  char modelName[128];             /* モデル名 */
  AerojsUInt32 coreCount;          /* 物理コア数 */
  AerojsUInt32 threadCount;        /* スレッド数 */
  AerojsUInt32 cacheL1I;           /* L1命令キャッシュサイズ（KB） */
  AerojsUInt32 cacheL1D;           /* L1データキャッシュサイズ（KB） */
  AerojsUInt32 cacheL2;            /* L2キャッシュサイズ（KB） */
  AerojsUInt32 cacheL3;            /* L3キャッシュサイズ（KB） */
  AerojsBool hasAVX;               /* AVXサポート（x86_64のみ） */
  AerojsBool hasAVX2;              /* AVX2サポート（x86_64のみ） */
  AerojsBool hasAVX512;            /* AVX-512サポート（x86_64のみ） */
  AerojsBool hasSVE;               /* SVEサポート（ARM64のみ） */
  AerojsUInt32 sveLength;          /* SVEベクトル長（ビット単位、ARM64のみ） */
  AerojsBool hasDotProd;           /* ドット積命令サポート（ARM64のみ） */
  AerojsBool hasJSCVT;             /* JavaScript変換命令サポート（ARM64のみ） */
  AerojsBool hasCrypto;            /* 暗号化命令サポート */
  AerojsBool hasBF16;              /* BF16命令サポート（ARM64のみ） */
} AerojsCPUInfo;

/**
 * プロセッサ情報の取得
 */
AEROJS_EXPORT AerojsStatus AerojsGetCPUInfo(AerojsCPUInfo* info);

#ifdef __cplusplus
}
#endif

#endif /* AEROJS_H */