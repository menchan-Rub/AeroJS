/**
 * @file arm64_jit.h
 * @brief AeroJS 世界最高性能ARM64 JITコンパイラの設定用C++ヘッダ
 * @version 2.0.0
 * @license MIT
 */

#ifndef AEROJS_ARM64_JIT_H
#define AEROJS_ARM64_JIT_H

#include "aerojs.h"
#include "jit.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace aerojs {

/**
 * @brief ARM64プロセッサモデルの詳細指定
 */
enum class ARM64ProcessorModel {
  Generic,          /* 汎用ARM64 */
  AppleM1,          /* Apple M1 */
  AppleM2,          /* Apple M2 */
  AppleM3,          /* Apple M3 */
  QualcommKryo,     /* Qualcomm Kryo */
  AmpereAltra,      /* Ampere Altra */
  NeoversN1,        /* ARM Neoverse N1 */
  NeoversN2,        /* ARM Neoverse N2 */
  NeoversV1,        /* ARM Neoverse V1 */
  NeoversV2,        /* ARM Neoverse V2 */
  CortexA55,        /* ARM Cortex-A55 */
  CortexA76,        /* ARM Cortex-A76 */
  CortexA77,        /* ARM Cortex-A77 */
  CortexA78,        /* ARM Cortex-A78 */
  CortexA710,       /* ARM Cortex-A710 */
  CortexA715,       /* ARM Cortex-A715 */
  CortexA720,       /* ARM Cortex-A720 */
  CortexX1,         /* ARM Cortex-X1 */
  CortexX2,         /* ARM Cortex-X2 */
  CortexX3,         /* ARM Cortex-X3 */
  CortexX4,         /* ARM Cortex-X4 */
  AWS_Graviton2,    /* AWS Graviton2 */
  AWS_Graviton3,    /* AWS Graviton3 */
  Fujitsu_A64FX     /* Fujitsu A64FX */
};

/**
 * @brief Advanced SIMD (NEON) 設定オプション
 */
struct ARM64SIMDOptions {
  bool useAdvancedSIMD;          /* 基本的なSIMD命令の使用 */
  bool useFP16SIMD;              /* FP16 SIMD命令の使用 */
  bool useDotProduct;            /* ドット積命令の使用 */
  bool useComplexNumbers;        /* 複素数SIMD処理の使用 */
  bool useMatrixMultiplication;  /* 行列乗算の最適化 */
  bool autovectorizeLoops;       /* ループの自動ベクトル化 */
  uint32_t preferredVectorWidth; /* 優先ベクトル幅（0=自動） */
  
  ARM64SIMDOptions() 
      : useAdvancedSIMD(true), 
        useFP16SIMD(false), 
        useDotProduct(false),
        useComplexNumbers(false),
        useMatrixMultiplication(false),
        autovectorizeLoops(true),
        preferredVectorWidth(0) {}
};

/**
 * @brief SVE (Scalable Vector Extension) 設定オプション
 */
struct ARM64SVEOptions {
  bool useSVE;                   /* SVE命令の使用 */
  bool useSVE2;                  /* SVE2命令の使用 */
  bool preferSVEOverNeon;        /* SVEをNEONより優先 */
  bool useSVEForMemoryOps;       /* メモリ操作にSVEを使用 */
  bool useSVEForLoopControl;     /* ループ制御にSVEを使用 */
  uint32_t minVectorLength;      /* 最小ベクトル長（0=無制限） */

  ARM64SVEOptions() 
      : useSVE(false), 
        useSVE2(false), 
        preferSVEOverNeon(false),
        useSVEForMemoryOps(false),
        useSVEForLoopControl(false),
        minVectorLength(0) {}
};

/**
 * @brief メモリ操作最適化オプション
 */
struct ARM64MemoryOptions {
  bool usePrefetch;              /* プリフェッチ命令の使用 */
  bool useLSE;                   /* Large System Extensionsの使用 */
  bool useMTE;                   /* Memory Tagging Extensionの使用 */
  bool useNontemporalHints;      /* 非時間的ヒントの使用 */
  bool useSpeculativeLoads;      /* 投機的ロードの使用 */
  
  ARM64MemoryOptions() 
      : usePrefetch(true), 
        useLSE(false), 
        useMTE(false),
        useNontemporalHints(true),
        useSpeculativeLoads(true) {}
};

/**
 * @brief 数値計算最適化オプション
 */
struct ARM64MathOptions {
  bool useFastMath;              /* 高速数学最適化 */
  bool useCryptography;          /* 暗号化命令の使用 */
  bool useJSCVT;                 /* JavaScript変換命令の使用 */
  bool useBF16;                  /* BF16命令の使用 */
  bool useRoundingOptimizations; /* 丸め処理の最適化 */
  
  ARM64MathOptions() 
      : useFastMath(true), 
        useCryptography(false), 
        useJSCVT(false),
        useBF16(false),
        useRoundingOptimizations(true) {}
};

/**
 * @brief 制御フロー最適化オプション
 */
struct ARM64ControlFlowOptions {
  bool useBTI;                   /* ブランチターゲット識別の使用 */
  bool usePAuth;                 /* ポインタ認証の使用 */
  bool useSpeculativeBranching;  /* 投機的分岐の使用 */
  bool useBranchHinting;         /* 分岐ヒントの使用 */
  
  ARM64ControlFlowOptions() 
      : useBTI(false), 
        usePAuth(false), 
        useSpeculativeBranching(true),
        useBranchHinting(true) {}
};

/**
 * @brief JITコンパイル詳細オプション
 */
struct ARM64JITCompilerOptions {
  ARM64ProcessorModel processorModel;       /* プロセッサモデル */
  ARM64SIMDOptions simdOptions;             /* SIMD設定 */
  ARM64SVEOptions sveOptions;               /* SVE設定 */
  ARM64MemoryOptions memoryOptions;         /* メモリ操作設定 */
  ARM64MathOptions mathOptions;             /* 数値計算設定 */
  ARM64ControlFlowOptions controlFlowOptions; /* 制御フロー設定 */
  bool enableVendorOptimizations;           /* ベンダー固有の最適化を有効にするかどうか */
  uint32_t maxCompilationThreads;           /* JITコンパイル用スレッド数（0=自動） */
  uint32_t superOptimizationLevel;          /* 超最適化レベル（0-4） */
  bool enableMetaTracing;                   /* メタトレーシング最適化を有効にするかどうか */
  bool enableSpeculativeOpts;               /* スペキュレーティブ最適化を有効にするかどうか */
  bool enablePGO;                           /* プロファイル誘導型最適化を有効にするかどうか */
  size_t codeCacheSize;                     /* JITコードキャッシュサイズの最大値（バイト単位） */
  std::string processorSpecificTuning;      /* プロセッサ特有の微調整パラメータ */
  
  ARM64JITCompilerOptions() 
      : processorModel(ARM64ProcessorModel::Generic),
        enableVendorOptimizations(true),
        maxCompilationThreads(0),
        superOptimizationLevel(1),
        enableMetaTracing(false),
        enableSpeculativeOpts(true),
        enablePGO(true),
        codeCacheSize(64 * 1024 * 1024) {}
};

/**
 * @brief ARM64 JITコンパイラの設定
 *
 * @param engine エンジンインスタンス
 * @param options 詳細設定オプション
 * @return 成功した場合は0、失敗した場合はエラーコード
 */
int ConfigureARM64JITCompiler(AerojsEngine* engine, const ARM64JITCompilerOptions& options);

/**
 * @brief 現在のシステムに最適なARM64 JITコンパイラの設定を自動取得
 *
 * @param engine エンジンインスタンス
 * @param options 設定を格納する構造体
 * @return 成功した場合は0、失敗した場合はエラーコード
 */
int GetOptimalARM64JITCompilerOptions(AerojsEngine* engine, ARM64JITCompilerOptions& options);

/**
 * @brief ARM64 JITデバッグ情報の取得
 *
 * @param engine エンジンインスタンス
 * @return デバッグ情報文字列
 */
std::string GetARM64JITDebugInfo(AerojsEngine* engine);

/**
 * @brief プロファイル情報から最適な設定を提案
 *
 * @param engine エンジンインスタンス
 * @param currentOptions 現在の設定
 * @return 提案された新しい設定
 */
ARM64JITCompilerOptions SuggestOptimizedOptions(AerojsEngine* engine, 
                                              const ARM64JITCompilerOptions& currentOptions);

} // namespace aerojs

#endif /* AEROJS_ARM64_JIT_H */ 