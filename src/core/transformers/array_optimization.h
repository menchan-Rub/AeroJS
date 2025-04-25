/**
 * @file array_optimization.h
 * @brief 配列操作最適化トランスフォーマーの定義
 * @version 0.2
 * @date 2024-01-15
 *
 * @copyright Copyright (c) 2024 AeroJS Project
 *
 * @license
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef AERO_TRANSFORMERS_ARRAY_OPTIMIZATION_H
#define AERO_TRANSFORMERS_ARRAY_OPTIMIZATION_H

#include <array>
#include <bitset>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../ast/ast.h"
#include "transformer.h"

namespace aero {
namespace transformers {

// 前方宣言
class HardwareCapabilityDetector;
class SIMDPatternMatcher;

/**
 * @brief メモリアクセスパターンを表す列挙型
 */
enum class MemoryAccessPattern {
  Unknown,          // 不明なパターン
  Sequential,       // 連続アクセス（配列を先頭から順にアクセス）
  Strided,          // ストライドアクセス（一定間隔でアクセス）
  Random,           // ランダムアクセス（予測不能なパターン）
  Gather,           // ギャザー（複数位置からデータ収集）
  Scatter,          // スキャッター（複数位置へデータ分散）
  BlockOriented,    // ブロック指向（キャッシュラインに最適化）
  CoalescedWrites,  // 集約書き込み（連続メモリへの書き込み集約）
  ZeroFill,         // ゼロ初期化（メモリをゼロで埋める）
  CopyMemory        // メモリコピー（一括コピー操作）
};

/**
 * @brief SIMD命令セットのサポート情報
 */
struct SIMDSupportInfo {
  bool sse = false;        // SSE (Streaming SIMD Extensions)
  bool sse2 = false;       // SSE2
  bool sse3 = false;       // SSE3
  bool ssse3 = false;      // SSSE3
  bool sse4_1 = false;     // SSE4.1
  bool sse4_2 = false;     // SSE4.2
  bool avx = false;        // AVX (Advanced Vector Extensions)
  bool avx2 = false;       // AVX2
  bool avx512 = false;     // AVX512
  bool neon = false;       // ARM NEON
  bool sve = false;        // ARM SVE (Scalable Vector Extension)
  bool wasm_simd = false;  // WebAssembly SIMD
};

/**
 * @brief ハードウェア固有の最適化設定
 */
struct HardwareOptimizationSettings {
  int cacheLine = 64;                   // キャッシュラインサイズ（バイト）
  int l1DataCache = 32 * 1024;          // L1データキャッシュサイズ
  int l2Cache = 256 * 1024;             // L2キャッシュサイズ
  int l3Cache = 8 * 1024 * 1024;        // L3キャッシュサイズ
  int prefetchDistance = 64;            // プリフェッチ距離
  SIMDSupportInfo simd;                 // SIMD対応状況
  int maxThreads = 4;                   // 並列処理の最大スレッド数
  bool supportsUnalignedAccess = true;  // アラインメントされていないアクセスをサポートするか
};

/**
 * @brief メモリアライメント戦略
 */
enum class MemoryAlignmentStrategy {
  None,          // アライメントなし（デフォルト）
  Natural,       // ナチュラルアライメント（型に基づく）
  CacheLine,     // キャッシュラインアライメント
  PageBoundary,  // ページ境界アライメント
  SIMDVector,    // SIMDベクトルアライメント
  Custom         // カスタムアライメント
};

/**
 * @brief 配列操作の種類を表す列挙型
 */
enum class ArrayOperationType {
  Unknown,      // 未知の操作
  Push,         // Array.prototype.push
  Pop,          // Array.prototype.pop
  Shift,        // Array.prototype.shift
  Unshift,      // Array.prototype.unshift
  Splice,       // Array.prototype.splice
  Slice,        // Array.prototype.slice
  Map,          // Array.prototype.map
  Filter,       // Array.prototype.filter
  Reduce,       // Array.prototype.reduce
  ForEach,      // Array.prototype.forEach
  Join,         // Array.prototype.join
  Concat,       // Array.prototype.concat
  Sort,         // Array.prototype.sort
  IndexAccess,  // 配列添字アクセス
  Set,          // TypedArray.prototype.set
  SubArray,     // TypedArray.prototype.subarray
  Fill,         // Array/TypedArray.prototype.fill
  CopyWithin,   // Array/TypedArray.prototype.copyWithin
  Reverse,      // Array/TypedArray.prototype.reverse
  Every,        // Array/TypedArray.prototype.every
  Some,         // Array/TypedArray.prototype.some
  Find,         // Array/TypedArray.prototype.find
  FindIndex,    // Array/TypedArray.prototype.findIndex
  Includes,     // Array/TypedArray.prototype.includes
  Keys,         // Array/TypedArray.prototype.keys
  Values,       // Array/TypedArray.prototype.values
  Entries,      // Array/TypedArray.prototype.entries
  Parallel,     // 並列処理操作
  SIMD          // SIMD最適化操作
};

/**
 * @brief 型付き配列の種類を表す列挙型
 */
enum class TypedArrayKind {
  NotTypedArray,      // 型付き配列ではない
  Int8Array,          // Int8Array
  Uint8Array,         // Uint8Array
  Uint8ClampedArray,  // Uint8ClampedArray
  Int16Array,         // Int16Array
  Uint16Array,        // Uint16Array
  Int32Array,         // Int32Array
  Uint32Array,        // Uint32Array
  Float32Array,       // Float32Array
  Float64Array,       // Float64Array
  BigInt64Array,      // BigInt64Array
  BigUint64Array      // BigUint64Array
};

/**
 * @brief 配列操作の情報を保持する構造体
 */
struct ArrayOperationInfo {
  ArrayOperationType type = ArrayOperationType::Unknown;
  ast::NodePtr arrayExpression = nullptr;                            // 配列式
  std::vector<ast::NodePtr> arguments;                               // 引数のリスト
  bool canOptimize = false;                                          // 最適化可能かどうか
  MemoryAccessPattern accessPattern = MemoryAccessPattern::Unknown;  // メモリアクセスパターン
  bool isHotPath = false;                                            // ホットパス上の操作かどうか
  bool isSafeForParallel = false;                                    // 並列実行が安全かどうか
  int estimatedComplexity = 0;                                       // 推定計算量（O(n)=1, O(n²)=2, ...）
  bool isPureOperation = false;                                      // 副作用のない操作かどうか
};

/**
 * @brief SIMD操作パターンを表す構造体
 */
struct SIMDOperation {
  enum class Type {
    None,            // SIMDなし
    Addition,        // 加算
    Subtraction,     // 減算
    Multiplication,  // 乗算
    Division,        // 除算
    Min,             // 最小値
    Max,             // 最大値
    And,             // ビット論理積
    Or,              // ビット論理和
    Xor,             // ビット排他的論理和
    Comparison,      // 比較
    Shuffle,         // シャッフル
    Blend,           // ブレンド
    LoadStore,       // ロード/ストア
    Custom           // カスタム操作
  };

  Type type = Type::None;
  int vectorSize = 0;               // ベクトルサイズ（要素数）
  int elementSize = 0;              // 要素サイズ（バイト）
  bool isVertical = false;          // 垂直演算か（要素ごと）
  bool isHorizontal = false;        // 水平演算か（ベクトル内）
  bool requiresMasking = false;     // マスキングが必要か
  bool canFallbackToScalar = true;  // スカラーにフォールバック可能か
};

/**
 * @brief 配列情報を保持する構造体
 */
struct ArrayInfo {
  size_t size = 0;                                                            // 配列のサイズ
  bool isFixed = false;                                                       // 固定サイズの配列かどうか
  bool isHomogeneous = false;                                                 // 同種の要素を持つ配列かどうか
  bool isTypedArray = false;                                                  // 型付き配列かどうか
  TypedArrayKind typedArrayKind = TypedArrayKind::NotTypedArray;              // 型付き配列の種類
  int elementByteSize = 0;                                                    // 要素1つあたりのバイトサイズ
  std::string elementType;                                                    // 要素の型
  int operationCount = 0;                                                     // 操作の回数
  ast::NodePtr initializer = nullptr;                                         // 初期化式
  bool isSharedBuffer = false;                                                // 共有バッファを使用しているか
  MemoryAlignmentStrategy alignmentStrategy = MemoryAlignmentStrategy::None;  // アライメント戦略
  int alignmentValue = 0;                                                     // アライメント値（バイト）
  bool isHotObject = false;                                                   // ホットパスで使用されるオブジェクトか
  int accessFrequency = 0;                                                    // アクセス頻度（推定）
  bool canUseIntrinsics = false;                                              // 組み込み関数が使用可能か
  bool needsBoundsCheck = true;                                               // 境界チェックが必要か
  std::bitset<32> optimizationFlags;                                          // 最適化フラグのビットセット
};

/**
 * @brief 配列変数の追跡情報を保持する構造体
 */
struct ArrayTrackingInfo {
  bool isFixedSize = false;                                       // 固定サイズの配列かどうか
  bool isHomogeneous = false;                                     // 同種の要素を持つ配列かどうか
  bool isTypedArray = false;                                      // 型付き配列かどうか
  TypedArrayKind typedArrayKind = TypedArrayKind::NotTypedArray;  // 型付き配列の種類
  int knownSize = -1;                                             // 既知のサイズ（-1は未知）
  bool hasHoles = false;                                          // スパースアレイかどうか
  std::string elementType;                                        // 要素の型（判明している場合）
  bool escapes = false;                                           // スコープ外に渡されるかどうか
  bool isLocal = true;                                            // ローカル変数かどうか
  int accessCount = 0;                                            // アクセス回数
  int modificationCount = 0;                                      // 変更回数
  std::vector<MemoryAccessPattern> observedPatterns;              // 観察されたアクセスパターン
  bool isOnHotPath = false;                                       // ホットパス上にあるか
  int loopNestingLevel = 0;                                       // ループのネストレベル
  std::unordered_map<std::string, int> methodCallFrequency;       // メソッド呼び出し頻度
};

/**
 * @brief SIMDベクトル化戦略を表す列挙型
 */
enum class SIMDStrategy {
  None,             ///< SIMD最適化なし
  Auto,             ///< 自動ベクトル化
  Explicit,         ///< 明示的ベクトル化
  FallbackAware,    ///< フォールバック対応型
  HardwareSpecific  ///< ハードウェア特化型
};

/**
 * @class ArrayOptimizationTransformer
 * @brief 配列操作を最適化するトランスフォーマー
 *
 * このトランスフォーマーは、配列操作を検出して最適化することでパフォーマンスを向上させます。
 * 特に、TypedArrayに対する操作や配列メソッドのインライン化、バッファリング最適化などを行います。
 */
class ArrayOptimizationTransformer : public Transformer {
 public:
  /**
   * @brief コンストラクタ
   * @param enableInlineForEach forEachメソッドのインライン化を有効にするかどうか
   * @param enableStatistics 統計情報収集を有効にするかどうか
   */
  ArrayOptimizationTransformer(bool enableInlineForEach = true, bool enableStatistics = false);

  /**
   * @brief デストラクタ
   */
  virtual ~ArrayOptimizationTransformer();

  /**
   * @brief 統計情報収集を有効/無効にする
   * @param enable 有効にする場合はtrue
   */
  void enableStatistics(bool enable);

  /**
   * @brief 最適化された操作の数を取得する
   * @return 最適化された操作の数
   */
  int getOptimizedOperationsCount() const;

  /**
   * @brief 検出された配列の数を取得する
   * @return 検出された配列の数
   */
  int getDetectedArraysCount() const;

  /**
   * @brief 最適化による推定メモリ節約量を取得する
   * @return 推定メモリ節約量（バイト単位）
   */
  int getEstimatedMemorySaved() const;

  /**
   * @brief 最適化によるSIMD利用回数を取得する
   * @return SIMD利用回数
   */
  int getSIMDOperationsCount() const;

  /**
   * @brief 最適化による並列処理回数を取得する
   * @return 並列処理回数
   */
  int getParallelProcessingCount() const;

  /**
   * @brief 処理カウンタをリセットする
   */
  void resetCounters();

  /**
   * @brief ASTノードを変換する
   * @param node 変換対象のノード
   * @return 変換結果
   */
  virtual TransformResult transform(ast::NodePtr node) override;

  /**
   * @brief ハードウェアSIMDサポートの検出を有効/無効にする
   * @param enable 有効にする場合はtrue
   */
  void enableHardwareDetection(bool enable);

  /**
   * @brief 並列処理最適化を有効/無効にする
   * @param enable 有効にする場合はtrue
   * @param threshold 並列処理を適用する配列サイズの閾値
   */
  void enableParallelProcessing(bool enable, int threshold = 1000);

  /**
   * @brief メモリアクセスパターン最適化を有効/無効にする
   * @param enable 有効にする場合はtrue
   */
  void enableMemoryPatternOptimization(bool enable);

  /**
   * @brief AOT(事前)コンパイル最適化を有効/無効にする
   * @param enable 有効にする場合はtrue
   */
  void enableAOTOptimization(bool enable);

  /**
   * @brief メモリアライメント最適化を有効/無効にする
   * @param enable 有効にする場合はtrue
   * @param alignment アライメント値（バイト単位、16、32、64など）
   */
  void enableMemoryAlignmentOptimization(bool enable, int alignment = 16);

 protected:
  /**
   * @brief プログラムノードを訪問する
   * @param node プログラムノード
   * @return 変換結果
   */
  virtual ast::NodePtr visitProgram(ast::NodePtr node) override;

  /**
   * @brief 変数宣言ノードを訪問する
   * @param node 変数宣言ノード
   * @return 変換結果
   */
  virtual ast::NodePtr visitVariableDeclaration(ast::NodePtr node) override;

  /**
   * @brief 関数宣言ノードを訪問する
   * @param node 関数宣言ノード
   * @return 変換結果
   */
  virtual ast::NodePtr visitFunctionDeclaration(ast::NodePtr node) override;

  /**
   * @brief for文ノードを訪問する
   * @param node for文ノード
   * @return 変換結果
   */
  virtual ast::NodePtr visitForStatement(ast::NodePtr node);

  /**
   * @brief for-of文ノードを訪問する
   * @param node for-of文ノード
   * @return 変換結果
   */
  virtual ast::NodePtr visitForOfStatement(ast::NodePtr node);

  /**
   * @brief 式文ノードを訪問する
   * @param node 式文ノード
   * @return 変換結果
   */
  virtual ast::NodePtr visitExpressionStatement(ast::NodePtr node) override;

  /**
   * @brief 呼び出し式ノードを訪問する
   * @param node 呼び出し式ノード
   * @return 変換結果
   */
  virtual ast::NodePtr visitCallExpression(ast::NodePtr node) override;

  /**
   * @brief メンバー式ノードを訪問する
   * @param node メンバー式ノード
   * @return 変換結果
   */
  virtual ast::NodePtr visitMemberExpression(ast::NodePtr node) override;

  /**
   * @brief 代入式ノードを訪問する
   * @param node 代入式ノード
   * @return 変換結果
   */
  virtual ast::NodePtr visitAssignmentExpression(ast::NodePtr node) override;

 private:
  typedef std::unordered_map<std::string, ast::NodePtr> ScopeMap;
  typedef std::unordered_map<std::string, ArrayInfo> ArrayVariableMap;

  /**
   * @brief 配列の使用状況を解析する
   * @param program プログラムノード
   */
  void analyzeArrayUsage(ast::NodePtr program);

  /**
   * @brief 配列変数を追跡する
   * @param name 変数名
   * @param initializer 初期化式
   */
  void trackArrayVariable(const std::string& name, ast::NodePtr initializer);

  /**
   * @brief 配列操作を識別する
   * @param node ノード
   * @return 配列操作情報
   */
  ArrayOperationInfo identifyArrayOperation(ast::NodePtr node) const;

  /**
   * @brief 操作をインライン化できるかどうかを判定する
   * @param info 配列操作情報
   * @return インライン化可能な場合はtrue
   */
  bool canInlineOperation(const ArrayOperationInfo& info) const;

  /**
   * @brief 配列操作を最適化する
   * @param node ノード
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeArrayOperation(ast::NodePtr node);

  /**
   * @brief ノードを変換する（内部用）
   * @param node ノード
   * @return 変換結果
   */
  TransformResult transformNode(ast::NodePtr node);

  /**
   * @brief スコープ内に入る
   */
  void enterScope();

  /**
   * @brief スコープから出る
   */
  void exitScope();

  /**
   * @brief 変数をスコープに追加する
   * @param name 変数名
   * @param initialValue 初期値
   */
  void addToScope(const std::string& name, ast::NodePtr initialValue);

  /**
   * @brief 配列変数かどうかを判定する
   * @param name 変数名
   * @return 配列変数の場合はtrue
   */
  bool isArrayVariable(const std::string& name) const;

  /**
   * @brief 固定サイズ配列かどうかを判定する
   * @param name 変数名
   * @return 固定サイズ配列の場合はtrue
   */
  bool isFixedSizeArray(const std::string& name) const;

  /**
   * @brief 同種要素の配列かどうかを判定する
   * @param name 変数名
   * @return 同種要素の配列の場合はtrue
   */
  bool isHomogeneousArray(const std::string& name) const;

  /**
   * @brief ノードが配列型かどうかを判定する
   * @param node ノード
   * @return 配列型の場合はtrue
   */
  bool isArrayType(ast::NodePtr node) const;

  /**
   * @brief 配列変数名を取得する
   * @param node ノード
   * @return 配列変数名（取得できない場合は空文字列）
   */
  std::string getArrayVariableName(ast::NodePtr node) const;

  /**
   * @brief 配列情報を更新する
   * @param name 変数名
   * @param opInfo 配列操作情報
   */
  void updateArrayInfo(const std::string& name, const ArrayOperationInfo& opInfo);

  /**
   * @brief pushメソッドを最適化する
   * @param info 配列操作情報
   * @return 最適化されたノード
   */
  ast::NodePtr optimizePush(const ArrayOperationInfo& info);

  /**
   * @brief popメソッドを最適化する
   * @param info 配列操作情報
   * @return 最適化されたノード
   */
  ast::NodePtr optimizePop(const ArrayOperationInfo& info);

  /**
   * @brief shiftメソッドを最適化する
   * @param info 配列操作情報
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeShift(const ArrayOperationInfo& info);

  /**
   * @brief unshiftメソッドを最適化する
   * @param info 配列操作情報
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeUnshift(const ArrayOperationInfo& info);

  /**
   * @brief forEachメソッドを最適化する
   * @param info 配列操作情報
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeForEach(const ArrayOperationInfo& info);

  /**
   * @brief mapメソッドを最適化する
   * @param info 配列操作情報
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeMap(const ArrayOperationInfo& info);

  /**
   * @brief filterメソッドを最適化する
   * @param info 配列操作情報
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeFilter(const ArrayOperationInfo& info);

  /**
   * @brief インデックスアクセスを最適化する
   * @param node ノード
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeIndexAccess(ast::NodePtr node);

  /**
   * @brief 配列バッファリングを最適化する
   * @param node ノード
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeArrayBuffering(ast::NodePtr node);

  /**
   * @brief MapとFilter操作を最適化する
   * @param node ノード
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeMapFilterOperations(ast::NodePtr node);

  /**
   * @brief 型付き配列のアライメント最適化を行う
   * @param node 型付き配列を生成するノード
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeTypedArrayAlignment(ast::NodePtr node);

  /**
   * @brief SIMD対応の最適化を行う
   * @param node 型付き配列を処理するノード
   * @param kind 型付き配列の種類
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeTypedArraySIMD(ast::NodePtr node, TypedArrayKind kind);

  /**
   * @brief 並列処理の最適化を行う
   * @param node 配列操作を含むノード
   * @param arrayName 配列変数名
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeParallelProcessing(ast::NodePtr node, const std::string& arrayName);

  /**
   * @brief 配列アクセスパターンを解析する
   * @param arrayName 配列変数名
   * @return アクセスパターン
   */
  MemoryAccessPattern analyzeAccessPattern(const std::string& arrayName);

  /**
   * @brief 型付き配列のループ最適化を行う
   * @param forNode forループノード
   * @param arrayName 配列変数名
   * @param kind 型付き配列の種類
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeTypedArrayLoop(ast::NodePtr forNode, const std::string& arrayName, TypedArrayKind kind);

  /**
   * @brief ハードウェアSIMD命令セットを検出する
   * @return SIMDサポート情報
   */
  SIMDSupportInfo detectHardwareSIMDSupport();

  /**
   * @brief 最適なSIMD戦略を選択する
   * @param kind TypedArrayの種類
   * @param opType 操作タイプ
   * @param simdInfo SIMDサポート情報
   * @return SIMD戦略
   */
  SIMDStrategy selectSIMDStrategy(TypedArrayKind kind, ArrayOperationType opType, const SIMDSupportInfo& simdInfo);

  /**
   * @brief AOT(事前)コンパイル最適化を適用する
   * @param node 操作ノード
   * @param arrayName 配列変数名
   * @return 最適化されたノード
   */
  ast::NodePtr applyAOTOptimization(ast::NodePtr node, const std::string& arrayName);

  /**
   * @brief GCプレッシャーを削減する最適化を適用する
   * @param node 操作ノード
   * @param arrayName 配列変数名
   * @return 最適化されたノード
   */
  ast::NodePtr applyGCPressureOptimization(ast::NodePtr node, const std::string& arrayName);

  /**
   * @brief バッファ共有最適化を適用する
   * @param node 操作ノード
   * @param arrayName 配列変数名
   * @return 最適化されたノード
   */
  ast::NodePtr applyBufferSharingOptimization(ast::NodePtr node, const std::string& arrayName);

  /**
   * @brief リテラルがゼロかどうかを判定する
   * @param node リテラルノード
   * @return ゼロの場合はtrue
   */
  bool isZeroLiteral(ast::NodePtr node) const;

  /**
   * @brief ノードの要素型を推測する
   * @param node 判定対象のノード
   * @return 推測された要素型
   */
  std::string inferArrayElementType(ast::NodePtr node) const;

  /**
   * @brief 型付き配列操作のエッジケースを処理する
   * @param node 型付き配列操作を含むノード
   * @param kind 型付き配列の種類
   * @return 処理されたノード
   */
  ast::NodePtr handleTypedArrayEdgeCases(ast::NodePtr node, TypedArrayKind kind);

  /**
   * @brief ノードがTypedArrayかどうかを判定する
   * @param node 判定対象のノード
   * @return TypedArrayの場合はtrue
   */
  bool isTypedArray(ast::NodePtr node) const;

  /**
   * @brief 型付き配列の種類を判定する
   * @param node 判定対象のノード
   * @return 型付き配列の種類
   */
  TypedArrayKind getTypedArrayKind(ast::NodePtr node) const;

  /**
   * @brief 型付き配列のコンストラクタ名から種類を判定する
   * @param name コンストラクタ名
   * @return 型付き配列の種類
   */
  TypedArrayKind getTypedArrayKindFromName(const std::string& name) const;

  /**
   * @brief 型付き配列のコンストラクタ名かどうかを判定する
   * @param name 判定対象の名前
   * @return コンストラクタ名の場合はtrue
   */
  bool isTypedArrayConstructor(const std::string& name) const;

  /**
   * @brief 型付き配列の要素サイズを取得する
   * @param kind 型付き配列の種類
   * @return 要素1つあたりのバイトサイズ
   */
  int getTypedArrayElementSize(TypedArrayKind kind) const;

  /**
   * @brief 型付き配列の要素型を表す文字列を取得する
   * @param kind 型付き配列の種類
   * @return 要素型を表す文字列
   */
  std::string getTypedArrayElementType(TypedArrayKind kind) const;

  /**
   * @brief TypedArray.setメソッドを最適化する
   * @param info 配列操作情報
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeTypedArraySet(const ArrayOperationInfo& info);

  /**
   * @brief TypedArray.subarrayメソッドを最適化する
   * @param info 配列操作情報
   * @return 最適化されたノード
   */
  ast::NodePtr optimizeTypedArraySubarray(const ArrayOperationInfo& info);

  /**
   * @brief Advanced SIMD最適化（高度なベクトル化）を適用する
   * @param node 操作ノード
   * @param arrayName 配列変数名
   * @param kind 型付き配列の種類
   * @return 最適化されたノード
   */
  ast::NodePtr applyAdvancedSIMDOptimization(ast::NodePtr node, const std::string& arrayName, TypedArrayKind kind);

  /**
   * @brief メモリアクセスパターン最適化を適用する
   * @param node 操作ノード
   * @param arrayName 配列変数名
   * @param pattern アクセスパターン
   * @return 最適化されたノード
   */
  ast::NodePtr applyMemoryAccessPatternOptimization(ast::NodePtr node, const std::string& arrayName, MemoryAccessPattern pattern);

  /**
   * @brief バッファ共有最適化を適用する
   * @param node 操作ノード
   * @param arrayName 配列変数名
   * @param targetOperation 対象操作タイプ
   * @return 最適化されたノード
   */
  ast::NodePtr applyBufferSharingOptimization(ast::NodePtr node, const std::string& arrayName, ArrayOperationType targetOperation);

  /**
   * @brief ハードウェア固有の最適化を適用する
   * @param node 操作ノード
   * @param arrayName 配列変数名
   * @param hwSettings ハードウェア設定
   * @return 最適化されたノード
   */
  ast::NodePtr applyHardwareSpecificOptimizations(ast::NodePtr node, const std::string& arrayName, const HardwareOptimizationSettings& hwSettings);

 private:
  bool m_enableInlineForEach;                ///< forEachメソッドのインライン化を有効にするかどうか
  bool m_enableStats;                        ///< 統計情報収集を有効にするかどうか
  bool m_changed;                            ///< 変更があったかどうか
  bool m_enableHardwareDetection;            ///< ハードウェア検出を有効にするかどうか
  bool m_enableParallelProcessing;           ///< 並列処理最適化を有効にするかどうか
  bool m_enableMemoryPatternOptimization;    ///< メモリアクセスパターン最適化を有効にするかどうか
  bool m_enableAOTOptimization;              ///< AOT最適化を有効にするかどうか
  bool m_enableMemoryAlignmentOptimization;  ///< メモリアライメント最適化を有効にするかどうか
  int m_parallelThreshold;                   ///< 並列処理適用閾値

  int m_optimizationThreshold;    ///< 最適化適用の閾値
  int m_arrayVariableCount;       ///< 検出された配列変数の数
  int m_typedArrayVariableCount;  ///< 検出された型付き配列の数
  int m_visitedNodesCount;        ///< 訪問したノードの数

  // 統計情報用カウンタ
  int m_optimizedOperationsCount;  ///< 最適化された操作の数
  int m_detectedArraysCount;       ///< 検出された配列の数
  int m_estimatedMemorySaved;      ///< 推定メモリ節約量
  int m_simdOperationsCount;       ///< SIMD操作の数
  int m_parallelProcessingCount;   ///< 並列処理回数

  // 操作タイプ別カウンタ
  int m_pushOptimizationCount;
  int m_popOptimizationCount;
  int m_shiftOptimizationCount;
  int m_unshiftOptimizationCount;
  int m_forEachOptimizationCount;
  int m_mapOptimizationCount;
  int m_filterOptimizationCount;
  int m_spliceOptimizationCount;
  int m_indexAccessOptimizationCount;

  // TypedArray専用カウンタ
  int m_typedArraySetOptimizationCount;
  int m_typedArraySubarrayOptimizationCount;
  int m_typedArrayCreationOptimizationCount;
  int m_typedArrayAlignmentOptimizationCount;
  int m_typedArraySIMDOptimizationCount;
  int m_typedArrayLoopOptimizationCount;
  int m_typedArrayForOfOptimizationCount;
  int m_typedArrayParallelOptimizationCount;
  int m_typedArrayBufferSharingOptimizationCount;
  int m_typedArrayGCPressureReductionCount;
  int m_typedArrayAOTOptimizationCount;
  int m_forOfOptimizationCount;

  SIMDSupportInfo m_simdSupportInfo;  ///< SIMDサポート情報

  ArrayVariableMap m_arrayVariables;                              ///< 配列変数の追跡情報
  std::unordered_map<ast::NodePtr, int> m_consecutiveOperations;  ///< 連続操作の追跡
  std::vector<ScopeMap> m_scopeStack;                             ///< スコープスタック

  ast::NodePtr m_result;                                                        ///< 変換結果
  std::unordered_map<std::string, std::vector<ast::NodePtr>> m_operationCache;  ///< 操作キャッシュ

  // 新しい最適化関連のカウンタ
  int m_advancedSIMDOptimizationCount;         ///< Advanced SIMD最適化カウント
  int m_memoryAccessPatternOptimizationCount;  ///< メモリアクセスパターン最適化カウント
  int m_bufferSharingOptimizationCount;        ///< バッファ共有最適化カウント
  int m_hardwareSpecificOptimizationCount;     ///< ハードウェア固有最適化カウント
};

}  // namespace transformers
}  // namespace aero

#endif  // AERO_TRANSFORMERS_ARRAY_OPTIMIZATION_H