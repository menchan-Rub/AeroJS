/**
 * @file aerojs.h
 * @brief AeroJS JavaScriptエンジンのメインヘッダーファイル
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_H
#define AEROJS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * バージョン情報
 */
#define AEROJS_VERSION_MAJOR 0
#define AEROJS_VERSION_MINOR 1
#define AEROJS_VERSION_PATCH 0
#define AEROJS_VERSION_STRING "0.1.0"

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
  AEROJS_ERROR_NOT_IMPLEMENTED = -8
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
  AerojsSize initialHeapSize;         /* 初期ヒープサイズ（バイト単位） */
  AerojsSize maximumHeapSize;         /* 最大ヒープサイズ（バイト単位） */
  AerojsSize stackSize;               /* スタックサイズ（バイト単位） */
  AerojsBool enableJIT;               /* JITコンパイルを有効にするかどうか */
  AerojsBool enableGC;                /* GCを有効にするかどうか */
  AerojsBool enableDebugger;          /* デバッガを有効にするかどうか */
  AerojsUInt32 gcThreshold;           /* GCが発生するしきい値（バイト単位） */
  AerojsUInt32 gcFrequency;           /* GCの頻度（ミリ秒単位） */
  const char* scriptCacheDir;         /* スクリプトキャッシュディレクトリ */
} AerojsEngineConfig;

/* デフォルト設定の取得 */
AEROJS_EXPORT void AerojsGetDefaultEngineConfig(AerojsEngineConfig* config);

/* 設定を使用したエンジンの作成 */
AEROJS_EXPORT AerojsEngine* AerojsCreateEngineWithConfig(const AerojsEngineConfig* config);

#ifdef __cplusplus
}
#endif

#endif /* AEROJS_H */ 