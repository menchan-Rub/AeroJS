/**
 * @file simd.h
 * @brief AeroJS 世界最高性能JavaScriptエンジンのSIMD処理API
 * @version 2.0.0
 * @license MIT
 */

#ifndef AEROJS_SIMD_H
#define AEROJS_SIMD_H

#include "aerojs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SIMDデータタイプ
 */
typedef enum {
  AEROJS_SIMD_INT8X16 = 0,      /* 16x 8ビット整数 */
  AEROJS_SIMD_INT16X8,          /* 8x 16ビット整数 */
  AEROJS_SIMD_INT32X4,          /* 4x 32ビット整数 */
  AEROJS_SIMD_INT64X2,          /* 2x 64ビット整数 */
  AEROJS_SIMD_UINT8X16,         /* 16x 8ビット符号なし整数 */
  AEROJS_SIMD_UINT16X8,         /* 8x 16ビット符号なし整数 */
  AEROJS_SIMD_UINT32X4,         /* 4x 32ビット符号なし整数 */
  AEROJS_SIMD_UINT64X2,         /* 2x 64ビット符号なし整数 */
  AEROJS_SIMD_FLOAT32X4,        /* 4x 32ビット浮動小数点 */
  AEROJS_SIMD_FLOAT64X2,        /* 2x 64ビット浮動小数点 */
  AEROJS_SIMD_BOOL8X16,         /* 16x 8ビットブール値 */
  AEROJS_SIMD_BOOL16X8,         /* 8x 16ビットブール値 */
  AEROJS_SIMD_BOOL32X4,         /* 4x 32ビットブール値 */
  AEROJS_SIMD_BOOL64X2,         /* 2x 64ビットブール値 */
  AEROJS_SIMD_FLOAT16X8,        /* 8x 16ビット浮動小数点 (FP16) */
  AEROJS_SIMD_BF16X8,           /* 8x ブレインフロート16 */
  AEROJS_SIMD_COMPLEX64X2,      /* 2x 複素数 (2x 32ビット) */
  /* 拡張SIMDタイプ - ARM専用 */
  AEROJS_SIMD_ARM_FLOAT32X2,    /* 2x 32ビット浮動小数点 (ARM NEON) */
  AEROJS_SIMD_ARM_FLOAT32X8,    /* 8x 32ビット浮動小数点 (ARM SVE) */
  AEROJS_SIMD_ARM_FLOAT64X4,    /* 4x 64ビット浮動小数点 (ARM SVE) */
  /* 拡張SIMDタイプ - x86専用 */
  AEROJS_SIMD_X86_FLOAT32X8,    /* 8x 32ビット浮動小数点 (AVX) */
  AEROJS_SIMD_X86_FLOAT64X4,    /* 4x 64ビット浮動小数点 (AVX) */
  AEROJS_SIMD_X86_FLOAT32X16,   /* 16x 32ビット浮動小数点 (AVX-512) */
  AEROJS_SIMD_X86_FLOAT64X8     /* 8x 64ビット浮動小数点 (AVX-512) */
} AerojsSIMDType;

/**
 * @brief SIMD演算タイプ
 */
typedef enum {
  /* 算術演算 */
  AEROJS_SIMD_OP_ADD = 0,       /* 加算 */
  AEROJS_SIMD_OP_SUB,           /* 減算 */
  AEROJS_SIMD_OP_MUL,           /* 乗算 */
  AEROJS_SIMD_OP_DIV,           /* 除算 */
  AEROJS_SIMD_OP_NEG,           /* 符号反転 */
  AEROJS_SIMD_OP_ABS,           /* 絶対値 */
  AEROJS_SIMD_OP_MIN,           /* 最小値 */
  AEROJS_SIMD_OP_MAX,           /* 最大値 */
  AEROJS_SIMD_OP_SQRT,          /* 平方根 */
  AEROJS_SIMD_OP_RECIP,         /* 逆数 */
  AEROJS_SIMD_OP_RECIP_SQRT,    /* 逆平方根 */
  AEROJS_SIMD_OP_ROUND,         /* 丸め */
  AEROJS_SIMD_OP_FLOOR,         /* 床関数 */
  AEROJS_SIMD_OP_CEIL,          /* 天井関数 */
  AEROJS_SIMD_OP_TRUNC,         /* 切り捨て */
  /* 論理演算 */
  AEROJS_SIMD_OP_AND,           /* ビット論理積 */
  AEROJS_SIMD_OP_OR,            /* ビット論理和 */
  AEROJS_SIMD_OP_XOR,           /* ビット排他的論理和 */
  AEROJS_SIMD_OP_NOT,           /* ビット否定 */
  AEROJS_SIMD_OP_SHL,           /* 左シフト */
  AEROJS_SIMD_OP_SHR,           /* 右シフト (論理) */
  AEROJS_SIMD_OP_SAR,           /* 右シフト (算術) */
  /* 比較演算 */
  AEROJS_SIMD_OP_EQ,            /* 等価 */
  AEROJS_SIMD_OP_NE,            /* 非等価 */
  AEROJS_SIMD_OP_LT,            /* 未満 */
  AEROJS_SIMD_OP_LE,            /* 以下 */
  AEROJS_SIMD_OP_GT,            /* 超過 */
  AEROJS_SIMD_OP_GE,            /* 以上 */
  /* データ操作 */
  AEROJS_SIMD_OP_SHUFFLE,       /* シャッフル */
  AEROJS_SIMD_OP_SWIZZLE,       /* スウィズル */
  AEROJS_SIMD_OP_SPLAT,         /* スプラット */
  AEROJS_SIMD_OP_SELECT,        /* 選択 */
  AEROJS_SIMD_OP_BLEND,         /* ブレンド */
  /* 高度な演算 */
  AEROJS_SIMD_OP_DOT,           /* ドット積 */
  AEROJS_SIMD_OP_CROSS,         /* クロス積 */
  AEROJS_SIMD_OP_FMA,           /* 積和融合 */
  AEROJS_SIMD_OP_SUM,           /* 水平加算 */
  AEROJS_SIMD_OP_PROD,          /* 水平乗算 */
  /* 変換演算 */
  AEROJS_SIMD_OP_CONVERT,       /* 型変換 */
  AEROJS_SIMD_OP_CAST,          /* キャスト */
  AEROJS_SIMD_OP_REINTERPRET,   /* 再解釈 */
  /* 複素数演算 */
  AEROJS_SIMD_OP_COMPLEX_MUL,   /* 複素数乗算 */
  AEROJS_SIMD_OP_COMPLEX_DIV    /* 複素数除算 */
} AerojsSIMDOperation;

/**
 * @brief SIMD実装機能フラグ
 */
typedef enum {
  AEROJS_SIMD_FEATURE_BASIC = 1 << 0,         /* 基本SIMD機能 */
  AEROJS_SIMD_FEATURE_FP16 = 1 << 1,          /* FP16サポート */
  AEROJS_SIMD_FEATURE_BF16 = 1 << 2,          /* BF16サポート */
  AEROJS_SIMD_FEATURE_INT64 = 1 << 3,         /* 64ビット整数演算 */
  AEROJS_SIMD_FEATURE_DOTPROD = 1 << 4,       /* ドット積 */
  AEROJS_SIMD_FEATURE_FMA = 1 << 5,           /* 積和融合 */
  AEROJS_SIMD_FEATURE_COMPLEX = 1 << 6,       /* 複素数演算 */
  AEROJS_SIMD_FEATURE_MATRIX = 1 << 7,        /* 行列演算 */
  AEROJS_SIMD_FEATURE_GATHER_SCATTER = 1 << 8, /* ギャザー/スキャッター */
  AEROJS_SIMD_FEATURE_SVE = 1 << 9,           /* ARM SVE */
  AEROJS_SIMD_FEATURE_AVX = 1 << 10,          /* x86 AVX */
  AEROJS_SIMD_FEATURE_AVX2 = 1 << 11,         /* x86 AVX2 */
  AEROJS_SIMD_FEATURE_AVX512 = 1 << 12,       /* x86 AVX-512 */
  AEROJS_SIMD_FEATURE_NEON = 1 << 13,         /* ARM NEON */
  AEROJS_SIMD_FEATURE_CRYPTO = 1 << 14,       /* 暗号化命令 */
  AEROJS_SIMD_FEATURE_MASKING = 1 << 15       /* マスク処理 */
} AerojsSIMDFeatureFlags;

/**
 * @brief SIMDランタイム情報
 */
typedef struct {
  AerojsUInt32 supportedTypes;        /* サポートされるSIMDタイプのビットマスク */
  AerojsUInt32 supportedOperations;   /* サポートされる演算のビットマスク */
  AerojsUInt32 featureFlags;          /* 機能フラグ */
  AerojsUInt32 maxLaneWidth;          /* 最大レーン幅（ビット） */
  AerojsUInt32 preferredVectorSize;   /* 推奨ベクトルサイズ（バイト） */
  AerojsUInt32 vectorRegisterCount;   /* ベクトルレジスタ数 */
  AerojsUInt32 maskRegisterCount;     /* マスクレジスタ数 */
  AerojsBool hasFallbackImpl;         /* フォールバック実装あり */
  AerojsBool hasHardwareAccel;        /* ハードウェア高速化あり */
  AerojsBool hasJITSupport;           /* JITサポートあり */
  char processorSIMDExtName[64];      /* プロセッサのSIMD拡張名 */
  AerojsUInt32 sveLength;             /* ARM SVEベクトル長（SVEサポート時のみ有効） */
} AerojsSIMDRuntime;

/**
 * @brief SIMDベクタ構造体
 */
typedef struct AerojsSIMDVector* AerojsSIMDVectorRef;

/**
 * @brief SIMDマトリックスハンドル
 */
typedef struct AerojsSIMDMatrix* AerojsSIMDMatrixRef;

/**
 * @brief SIMDマスクハンドル
 */
typedef struct AerojsSIMDMask* AerojsSIMDMaskRef;

/**
 * @brief SIMD実装を初期化
 *
 * @param engine エンジンインスタンス
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsInitializeSIMD(AerojsEngine* engine);

/**
 * @brief SIMDサポートを検出
 *
 * @param engine エンジンインスタンス
 * @param runtime ランタイム情報を格納する構造体へのポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsDetectSIMDSupport(AerojsEngine* engine, AerojsSIMDRuntime* runtime);

/**
 * @brief SIMDベクタを作成
 *
 * @param ctx コンテキスト
 * @param type SIMDデータタイプ
 * @param values 初期値の配列 (NULL可)
 * @param count 値の数
 * @return SIMDベクタハンドル、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsSIMDVectorRef AerojsCreateSIMDVector(AerojsContext* ctx,
                                                       AerojsSIMDType type,
                                                       const void* values,
                                                       AerojsSize count);

/**
 * @brief SIMDベクタを値で初期化
 *
 * @param ctx コンテキスト
 * @param type SIMDデータタイプ
 * @param value 全レーンに設定する値
 * @return SIMDベクタハンドル、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsSIMDVectorRef AerojsCreateSIMDVectorSplat(AerojsContext* ctx,
                                                             AerojsSIMDType type,
                                                             double value);

/**
 * @brief SIMDベクタを解放
 *
 * @param ctx コンテキスト
 * @param vector SIMDベクタハンドル
 */
AEROJS_EXPORT void AerojsReleaseSIMDVector(AerojsContext* ctx, AerojsSIMDVectorRef vector);

/**
 * @brief SIMDベクタの要素を取得
 *
 * @param ctx コンテキスト
 * @param vector SIMDベクタハンドル
 * @param lane レーンインデックス
 * @param value 値を格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsGetSIMDVectorLane(AerojsContext* ctx,
                                                 AerojsSIMDVectorRef vector,
                                                 AerojsUInt32 lane,
                                                 void* value);

/**
 * @brief SIMDベクタの要素を設定
 *
 * @param ctx コンテキスト
 * @param vector SIMDベクタハンドル
 * @param lane レーンインデックス
 * @param value 設定する値のポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSetSIMDVectorLane(AerojsContext* ctx,
                                                 AerojsSIMDVectorRef vector,
                                                 AerojsUInt32 lane,
                                                 const void* value);

/**
 * @brief SIMDベクタを全てのレーン値で初期化
 *
 * @param ctx コンテキスト
 * @param vector SIMDベクタハンドル
 * @param values 値の配列
 * @param count 値の数
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsLoadSIMDVector(AerojsContext* ctx,
                                              AerojsSIMDVectorRef vector,
                                              const void* values,
                                              AerojsSize count);

/**
 * @brief SIMDベクタ値をメモリに格納
 *
 * @param ctx コンテキスト
 * @param vector SIMDベクタハンドル
 * @param values 値を格納するバッファ
 * @param count バッファの要素数
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsStoreSIMDVector(AerojsContext* ctx,
                                               AerojsSIMDVectorRef vector,
                                               void* values,
                                               AerojsSize count);

/**
 * @brief 単項SIMD演算を実行
 *
 * @param ctx コンテキスト
 * @param op 演算タイプ
 * @param a 入力SIMDベクタ
 * @param result 結果を格納するSIMDベクタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSIMDUnaryOp(AerojsContext* ctx,
                                           AerojsSIMDOperation op,
                                           AerojsSIMDVectorRef a,
                                           AerojsSIMDVectorRef result);

/**
 * @brief 二項SIMD演算を実行
 *
 * @param ctx コンテキスト
 * @param op 演算タイプ
 * @param a 第一入力SIMDベクタ
 * @param b 第二入力SIMDベクタ
 * @param result 結果を格納するSIMDベクタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSIMDBinaryOp(AerojsContext* ctx,
                                            AerojsSIMDOperation op,
                                            AerojsSIMDVectorRef a,
                                            AerojsSIMDVectorRef b,
                                            AerojsSIMDVectorRef result);

/**
 * @brief 三項SIMD演算を実行
 *
 * @param ctx コンテキスト
 * @param op 演算タイプ
 * @param a 第一入力SIMDベクタ
 * @param b 第二入力SIMDベクタ
 * @param c 第三入力SIMDベクタ
 * @param result 結果を格納するSIMDベクタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSIMDTernaryOp(AerojsContext* ctx,
                                             AerojsSIMDOperation op,
                                             AerojsSIMDVectorRef a,
                                             AerojsSIMDVectorRef b,
                                             AerojsSIMDVectorRef c,
                                             AerojsSIMDVectorRef result);

/**
 * @brief SIMDシャッフル操作を実行
 *
 * @param ctx コンテキスト
 * @param a 入力SIMDベクタ
 * @param indices レーンインデックスの配列
 * @param count インデックスの数
 * @param result 結果を格納するSIMDベクタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSIMDShuffle(AerojsContext* ctx,
                                           AerojsSIMDVectorRef a,
                                           const AerojsUInt32* indices,
                                           AerojsSize count,
                                           AerojsSIMDVectorRef result);

/**
 * @brief SIMDベクタを別のタイプに変換
 *
 * @param ctx コンテキスト
 * @param vector 入力SIMDベクタ
 * @param toType 変換先タイプ
 * @param result 結果を格納するSIMDベクタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsConvertSIMDVector(AerojsContext* ctx,
                                                 AerojsSIMDVectorRef vector,
                                                 AerojsSIMDType toType,
                                                 AerojsSIMDVectorRef result);

/**
 * @brief SIMDマスクを作成
 *
 * @param ctx コンテキスト
 * @param type SIMDデータタイプ
 * @param values 初期マスク値の配列 (NULLの場合すべて0)
 * @param count 値の数
 * @return SIMDマスクハンドル、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsSIMDMaskRef AerojsCreateSIMDMask(AerojsContext* ctx,
                                                    AerojsSIMDType type,
                                                    const AerojsBool* values,
                                                    AerojsSize count);

/**
 * @brief SIMDマスクを解放
 *
 * @param ctx コンテキスト
 * @param mask SIMDマスクハンドル
 */
AEROJS_EXPORT void AerojsReleaseSIMDMask(AerojsContext* ctx, AerojsSIMDMaskRef mask);

/**
 * @brief SIMDコンディショナル操作を実行
 *
 * @param ctx コンテキスト
 * @param mask 選択マスク
 * @param ifTrue マスクがtrueの場合に選択されるベクタ
 * @param ifFalse マスクがfalseの場合に選択されるベクタ
 * @param result 結果を格納するSIMDベクタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSIMDSelect(AerojsContext* ctx,
                                          AerojsSIMDMaskRef mask,
                                          AerojsSIMDVectorRef ifTrue,
                                          AerojsSIMDVectorRef ifFalse,
                                          AerojsSIMDVectorRef result);

/**
 * @brief SIMD行列を作成
 *
 * @param ctx コンテキスト
 * @param type SIMDデータタイプ
 * @param rows 行数
 * @param cols 列数
 * @param values 初期値の配列 (NULL可)
 * @return SIMD行列ハンドル、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsSIMDMatrixRef AerojsCreateSIMDMatrix(AerojsContext* ctx,
                                                       AerojsSIMDType type,
                                                       AerojsUInt32 rows,
                                                       AerojsUInt32 cols,
                                                       const void* values);

/**
 * @brief SIMD行列を解放
 *
 * @param ctx コンテキスト
 * @param matrix SIMD行列ハンドル
 */
AEROJS_EXPORT void AerojsReleaseSIMDMatrix(AerojsContext* ctx, AerojsSIMDMatrixRef matrix);

/**
 * @brief SIMD行列乗算を実行
 *
 * @param ctx コンテキスト
 * @param a 第一入力行列
 * @param b 第二入力行列
 * @param result 結果を格納する行列
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSIMDMatrixMultiply(AerojsContext* ctx,
                                                  AerojsSIMDMatrixRef a,
                                                  AerojsSIMDMatrixRef b,
                                                  AerojsSIMDMatrixRef result);

/**
 * @brief SIMDベクタをJavaScript配列に変換
 *
 * @param ctx コンテキスト
 * @param vector SIMDベクタハンドル
 * @return JavaScript配列オブジェクト、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsValueRef AerojsConvertSIMDVectorToArray(AerojsContext* ctx, AerojsSIMDVectorRef vector);

/**
 * @brief JavaScript配列からSIMDベクタを作成
 *
 * @param ctx コンテキスト
 * @param array JavaScript配列
 * @param type SIMDデータタイプ
 * @return SIMDベクタハンドル、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsSIMDVectorRef AerojsCreateSIMDVectorFromArray(AerojsContext* ctx,
                                                                AerojsValueRef array,
                                                                AerojsSIMDType type);

/**
 * @brief SIMDベクタをJavaScript TypedArrayに変換
 *
 * @param ctx コンテキスト
 * @param vector SIMDベクタハンドル
 * @return JavaScript TypedArrayオブジェクト、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsValueRef AerojsConvertSIMDVectorToTypedArray(AerojsContext* ctx, AerojsSIMDVectorRef vector);

/**
 * @brief JavaScript TypedArrayからSIMDベクタを作成
 *
 * @param ctx コンテキスト
 * @param typedArray JavaScript TypedArray
 * @param type SIMDデータタイプ
 * @return SIMDベクタハンドル、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsSIMDVectorRef AerojsCreateSIMDVectorFromTypedArray(AerojsContext* ctx,
                                                                     AerojsValueRef typedArray,
                                                                     AerojsSIMDType type);

/**
 * @brief FMA (Fused Multiply-Add) 演算を実行
 *
 * @param ctx コンテキスト
 * @param a 第一入力ベクタ
 * @param b 第二入力ベクタ
 * @param c 加算ベクタ
 * @param result 結果を格納するベクタ (a * b + c)
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSIMDFusedMultiplyAdd(AerojsContext* ctx,
                                                    AerojsSIMDVectorRef a,
                                                    AerojsSIMDVectorRef b,
                                                    AerojsSIMDVectorRef c,
                                                    AerojsSIMDVectorRef result);

/**
 * @brief SIMDドット積を計算
 *
 * @param ctx コンテキスト
 * @param a 第一入力ベクタ
 * @param b 第二入力ベクタ
 * @param value 結果値を格納するポインタ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSIMDDotProduct(AerojsContext* ctx,
                                              AerojsSIMDVectorRef a,
                                              AerojsSIMDVectorRef b,
                                              void* value);

/**
 * @brief ARM SVE特有のSIMDベクタを作成
 *
 * @param ctx コンテキスト
 * @param type SIMDデータタイプ
 * @param values 初期値の配列 (NULL可)
 * @return SIMDベクタハンドル、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsSIMDVectorRef AerojsCreateSVEVector(AerojsContext* ctx,
                                                      AerojsSIMDType type,
                                                      const void* values);

/**
 * @brief SVEプレディケートを作成
 *
 * @param ctx コンテキスト
 * @param elementType SIMDデータタイプ
 * @param values 初期値の配列 (NULL可)
 * @return SIMDマスクハンドル、失敗した場合はNULL
 */
AEROJS_EXPORT AerojsSIMDMaskRef AerojsCreateSVEPredicate(AerojsContext* ctx,
                                                       AerojsSIMDType elementType,
                                                       const AerojsBool* values);

/**
 * @brief SIMDベクトル命令をストリーミング処理する
 *
 * @param ctx コンテキスト
 * @param op 演算タイプ
 * @param input 入力データバッファ
 * @param inputSize 入力サイズ (バイト)
 * @param stride ストライド (バイト)
 * @param output 出力バッファ
 * @param outputSize 出力バッファサイズ (バイト)
 * @param elementType SIMDデータタイプ
 * @return 成功した場合はAEROJS_SUCCESS、失敗した場合はエラーコード
 */
AEROJS_EXPORT AerojsStatus AerojsSIMDStream(AerojsContext* ctx,
                                          AerojsSIMDOperation op,
                                          const void* input,
                                          AerojsSize inputSize,
                                          AerojsSize stride,
                                          void* output,
                                          AerojsSize outputSize,
                                          AerojsSIMDType elementType);

#ifdef __cplusplus
}
#endif

#endif /* AEROJS_SIMD_H */ 