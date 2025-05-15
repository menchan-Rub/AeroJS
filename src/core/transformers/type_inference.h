/**
 * @file type_inference.h
 * @brief 最先端の型推論トランスフォーマーシステム
 * @version 2.0.0
 * @date 2024-05-10
 *
 * @copyright Copyright (c) 2024 AeroJS Project
 *
 * @license
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef AERO_TYPE_INFERENCE_H
#define AERO_TYPE_INFERENCE_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <optional>
#include <set>
#include <bitset>
#include <functional>
#include <type_traits>
#include <ranges>
#include <algorithm>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <shared_mutex>
#include <atomic>

#include "../ast/ast.h"
#include "transformer.h"

namespace aero {
namespace transformers {

/**
 * @brief 型の種類を表す列挙型（詳細な型システム）
 */
enum class PreciseType : uint16_t {
  // 基本型
  Unknown             = 0,      ///< 不明な型
  Undefined           = 1 << 0, ///< undefined値
  Null                = 1 << 1, ///< null値
  Boolean             = 1 << 2, ///< ブール値
  Integer             = 1 << 3, ///< 整数値
  Float               = 1 << 4, ///< 浮動小数点数値
  NaN                 = 1 << 5, ///< NaN値
  Infinity            = 1 << 6, ///< 無限大値
  String              = 1 << 7, ///< 文字列
  
  // 複合型
  Object              = 1 << 8, ///< オブジェクト
  Array               = 1 << 9, ///< 配列
  Function            = 1 << 10, ///< 関数
  Date                = 1 << 11, ///< Date
  RegExp              = 1 << 12, ///< 正規表現
  Map                 = 1 << 13, ///< Map
  Set                 = 1 << 14, ///< Set
  Promise             = 1 << 15, ///< Promise
  
  // 特殊型
  TypedArray          = 1 << 16, ///< 型付き配列
  Symbol              = 1 << 17, ///< シンボル
  BigInt              = 1 << 18, ///< BigInt
  WeakReference       = 1 << 19, ///< 弱参照
  Iterator            = 1 << 20, ///< イテレータ
  
  // 集合型
  Number = Integer | Float | NaN | Infinity,  ///< すべての数値型
  Primitive = Undefined | Null | Boolean | Number | String | Symbol | BigInt, ///< プリミティブ型
  Any = 0xFFFFFFFF,  ///< すべての型（Unknown以外）
  
  // データ転送表現型
  JSON = Null | Boolean | Number | String | Object | Array, ///< JSON互換型
  
  // オブジェクト型の意味的区分
  Callable = Function | Constructor, ///< 呼び出し可能型
  Indexable = Array | String | TypedArray | Object,  ///< インデックスアクセス可能型
  Constructor = 1 << 21, ///< コンストラクタ
  Iterable = Array | Map | Set | String | Iterator | Object ///< 反復処理可能型
};

// PreciseTypeのビット演算操作をサポート
inline PreciseType operator|(PreciseType a, PreciseType b) {
  return static_cast<PreciseType>(static_cast<std::underlying_type_t<PreciseType>>(a) |
                                 static_cast<std::underlying_type_t<PreciseType>>(b));
}

inline PreciseType operator&(PreciseType a, PreciseType b) {
  return static_cast<PreciseType>(static_cast<std::underlying_type_t<PreciseType>>(a) &
                                 static_cast<std::underlying_type_t<PreciseType>>(b));
}

inline PreciseType operator~(PreciseType a) {
  return static_cast<PreciseType>(~static_cast<std::underlying_type_t<PreciseType>>(a));
}

inline bool operator!(PreciseType a) {
  return static_cast<std::underlying_type_t<PreciseType>>(a) == 0;
}

// 簡易用の型表現（基本的な型区分）
enum class TypeKind : uint8_t {
  Unknown,    ///< 未知の型
  Null,       ///< null値
  Undefined,  ///< undefined値
  Boolean,    ///< ブール値
  Number,     ///< 数値
  String,     ///< 文字列
  Object,     ///< オブジェクト
  Array,      ///< 配列
  Function,   ///< 関数
  BigInt,     ///< BigInt
  Symbol,     ///< シンボル
  Union       ///< 複数の型の組み合わせ
};

/**
 * @struct NumericRangeInfo
 * @brief 数値の範囲情報を表す構造体
 */
struct NumericRangeInfo {
  bool hasMin = false;             ///< 最小値が設定されているか
  bool hasMax = false;             ///< 最大値が設定されているか
  bool includesNaN = false;        ///< NaNを含むか
  bool includesInfinity = false;   ///< 無限大を含むか
  bool isInteger = false;          ///< 整数のみか
  double minValue = 0.0;           ///< 最小値
  double maxValue = 0.0;           ///< 最大値
  
  // 範囲の交差操作
  NumericRangeInfo intersect(const NumericRangeInfo& other) const;
  
  // 範囲の合成操作
  NumericRangeInfo merge(const NumericRangeInfo& other) const;
  
  // 文字列表現
  std::string toString() const;
  
  // 値が範囲内かチェック
  bool contains(double value) const;
};

/**
 * @struct TypeInfo
 * @brief 拡張型情報を表す構造体
 * @details フロー解析と型プロパゲーションをサポートする詳細な型情報
 */
struct TypeInfo {
  // 基本型情報
  PreciseType type = PreciseType::Unknown;  ///< 型の種類
  std::string name;                     ///< 型名（クラス名など）
  
  // 数値型の詳細情報
  std::optional<NumericRangeInfo> numericRange; ///< 数値の範囲情報
  
  // 文字列型の詳細情報
  std::optional<std::string> stringPattern;   ///< 想定される文字列パターン（正規表現）
  bool isStringLiteral = false;               ///< リテラル文字列（値が確定）か
  std::optional<std::string> literalValue;    ///< リテラル値（設定されている場合）
  std::optional<size_t> stringLengthRange;    ///< 文字列長の範囲
  
  // 関数型の詳細情報
  std::shared_ptr<TypeInfo> returnType;  ///< 戻り値の型
  std::vector<std::shared_ptr<TypeInfo>> paramTypes;  ///< 引数の型リスト
  bool isAsync = false;                  ///< 非同期関数か
  bool isGenerator = false;              ///< ジェネレータ関数か
  bool isPure = false;                   ///< 純粋関数か（副作用なし）
  bool isVarArg = false;                 ///< 可変長引数をサポートするか
  
  // オブジェクト型の詳細情報
  std::unordered_map<std::string, std::shared_ptr<TypeInfo>> properties;  ///< プロパティの型マップ
  std::shared_ptr<TypeInfo> indexType;   ///< インデックスの型（ディクショナリの場合）
  std::vector<std::string> methods;      ///< メソッド名リスト
  
  // 配列型の詳細情報
  std::shared_ptr<TypeInfo> elementType; ///< 要素の型
  bool isHomogeneous = false;            ///< 均質な配列か（全要素が同じ型）
  std::optional<size_t> arrayLength;     ///< 配列長（静的に判明する場合）
  
  // ユニオン型の詳細情報
  std::vector<std::shared_ptr<TypeInfo>> unionTypes;  ///< ユニオン型の構成要素
  
  // 型の制約条件
  std::unordered_map<std::string, std::string> constraints;  ///< 型制約条件
  
  // 型の確度情報
  enum class ConfidenceLevel : uint8_t {
    Inferred,     ///< 推論による（低確度）
    Probable,     ///< おそらく正しい（中確度）
    Annotated,    ///< 注釈あり（高確度）
    Proven        ///< 証明済み（最高確度）
  } confidence = ConfidenceLevel::Inferred;
  
  // 型のライフスパン
  struct Location {
    std::string filename;
    int line;
    int column;
  };
  std::optional<Location> definedAt;  ///< 型が定義された場所
  
  // エスケープ解析情報
  bool escapes = false;  ///< この型の変数がスコープ外に渡されるか
  
  // メタデータ
  std::unordered_map<std::string, std::string> metadata;  ///< 追加メタデータ
  
  // 型操作メソッド
  
  /**
   * @brief TypeKindへ変換（下位互換性のため）
   * @return 基本的な型分類
   */
  TypeKind toTypeKind() const;
  
  /**
   * @brief ユニオン型を作成
   * @param other 合成する型
   * @return 新しいユニオン型
   */
  static std::shared_ptr<TypeInfo> createUnion(const TypeInfo& a, const TypeInfo& b);
  
  /**
   * @brief 2つの型の交差型を取得
   * @param other 交差する型
   * @return 交差型（共通部分）
   */
  std::shared_ptr<TypeInfo> intersect(const TypeInfo& other) const;
  
  /**
   * @brief 型のサブタイプ関係をチェック
   * @param other 比較対象の型
   * @return thisがotherのサブタイプならtrue
   */
  bool isSubtypeOf(const TypeInfo& other) const;
  
  /**
   * @brief 型の互換性をチェック
   * @param other 比較対象の型
   * @return 型が互換性を持つ場合true
   */
  bool isCompatibleWith(const TypeInfo& other) const;
  
  /**
   * @brief 型の文字列表現を取得
   * @param detailed 詳細表示フラグ
   * @return 型の文字列表現
   */
  std::string toString(bool detailed = false) const;
  
  /**
   * @brief 型がプリミティブかチェック
   * @return プリミティブ型の場合true
   */
  bool isPrimitive() const;
  
  /**
   * @brief 型がオブジェクト系かチェック
   * @return オブジェクト系の場合true
   */
  bool isObjectType() const;
  
  /**
   * @brief 型がコール可能かチェック
   * @return 関数やコンストラクタの場合true
   */
  bool isCallable() const;
  
  /**
   * @brief 型が数値系かチェック
   * @return 数値型の場合true
   */
  bool isNumeric() const;
};

/**
 * @class TypeConstraintSolver
 * @brief 型制約を解決するソルバークラス
 */
class TypeConstraintSolver {
public:
  /**
   * @brief 型制約を追加
   * @param variableName 変数名
   * @param constraint 制約条件
   */
  void addConstraint(const std::string& variableName, const std::string& constraint);
  
  /**
   * @brief 制約条件から型を推論
   * @param variableName 変数名
   * @return 推論された型（解決できない場合はnullptr）
   */
  std::shared_ptr<TypeInfo> solveConstraints(const std::string& variableName);
  
  /**
   * @brief すべての制約を解決し、矛盾がないか検証
   * @return 矛盾がなければtrue
   */
  bool validateAllConstraints();
  
private:
  std::unordered_map<std::string, std::vector<std::string>> m_constraints;
  std::unordered_map<std::string, std::shared_ptr<TypeInfo>> m_solutions;
};

/**
 * @class FlowTypeContext
 * @brief フロー解析に基づく型コンテキスト
 * @details 条件分岐に基づく型の絞り込みを管理
 */
class FlowTypeContext {
public:
  /**
   * @brief コンストラクタ
   */
  FlowTypeContext() = default;
  
  /**
   * @brief 新しいフローコンテキストを作成
   * @return 新しいコンテキストのスマートポインタ
   */
  static std::shared_ptr<FlowTypeContext> create();
  
  /**
   * @brief コンテキストを分岐
   * @param condition 分岐条件のノード
   * @return 真・偽ブランチのコンテキストペア
   */
  std::pair<std::shared_ptr<FlowTypeContext>, std::shared_ptr<FlowTypeContext>> branch(const ast::NodePtr& condition);
  
  /**
   * @brief コンテキストをマージ
   * @param other マージするコンテキスト
   * @return マージされた新しいコンテキスト
   */
  std::shared_ptr<FlowTypeContext> merge(const FlowTypeContext& other);
  
  /**
   * @brief 条件に基づく型の絞り込み
   * @param name 変数名
   * @param narrowedType 絞り込まれた型
   */
  void narrowType(const std::string& name, const std::shared_ptr<TypeInfo>& narrowedType);
  
  /**
   * @brief 変数の型を取得
   * @param name 変数名
   * @return 現在のコンテキストでの型（存在しない場合はnullptr）
   */
  std::shared_ptr<TypeInfo> getType(const std::string& name) const;
  
private:
  std::unordered_map<std::string, std::shared_ptr<TypeInfo>> m_narrowedTypes;
  std::weak_ptr<FlowTypeContext> m_parent;
};

/**
 * @struct TypeAnalysisResult
 * @brief 型解析の結果を格納する構造体
 */
struct TypeAnalysisResult {
  std::unordered_map<std::string, std::shared_ptr<TypeInfo>> globalTypes;  ///< グローバル変数の型
  std::unordered_map<std::string, std::shared_ptr<TypeInfo>> functionTypes;  ///< 関数の型
  std::unordered_map<std::string, std::shared_ptr<TypeInfo>> classTypes;  ///< クラスの型
  std::vector<std::string> typeMismatchWarnings;  ///< 型の不一致警告
  std::vector<std::string> typeRecommendations;  ///< 型の推奨事項
  size_t totalNodesAnalyzed = 0;  ///< 解析されたノードの総数
  
  /**
   * @brief 結果をJSONとして出力
   * @return JSON形式の結果
   */
  std::string toJson() const;
  
  /**
   * @brief HTMLレポートとして出力
   * @return HTML形式のレポート
   */
  std::string toHtml() const;
};

/**
 * @class TypeInferenceEngine
 * @brief 高度な型推論エンジン
 * @details フロー解析、制約ソルバー、パターン認識を組み合わせた型推論を提供
 */
class TypeInferenceEngine {
public:
  /**
   * @brief コンストラクタ
   */
  TypeInferenceEngine();
  
  /**
   * @brief ノードの型を推論
   * @param node 推論対象のノード
   * @return 推論された型
   */
  std::shared_ptr<TypeInfo> inferType(const ast::NodePtr& node);
  
  /**
   * @brief ノードの型を推論（コンテキスト付き）
   * @param node 推論対象のノード
   * @param context フロータイピングコンテキスト
   * @return 推論された型
   */
  std::shared_ptr<TypeInfo> inferTypeWithContext(const ast::NodePtr& node, const std::shared_ptr<FlowTypeContext>& context);
  
  /**
   * @brief 変数の型を設定
   * @param name 変数名
   * @param type 型情報
   */
  void setVariableType(const std::string& name, const std::shared_ptr<TypeInfo>& type);
  
  /**
   * @brief 変数の型を取得
   * @param name 変数名
   * @return 型情報（存在しない場合はnullptr）
   */
  std::shared_ptr<TypeInfo> getVariableType(const std::string& name) const;
  
  /**
   * @brief スコープを開始
   * @param scopeName スコープ名（関数名やブロック識別子）
   */
  void enterScope(const std::string& scopeName);
  
  /**
   * @brief スコープを終了
   */
  void exitScope();
  
  /**
   * @brief 型の不一致を記録
   * @param nodeWithError エラーのあるノード
   * @param expected 期待される型
   * @param actual 実際の型
   * @param message エラーメッセージ
   */
  void recordTypeMismatch(const ast::NodePtr& nodeWithError, 
                         const std::shared_ptr<TypeInfo>& expected,
                         const std::shared_ptr<TypeInfo>& actual,
                         const std::string& message);
  
  /**
   * @brief 型に関する推奨事項を記録
   * @param node 対象ノード
   * @param recommendation 推奨事項
   */
  void recordRecommendation(const ast::NodePtr& node, const std::string& recommendation);
  
  /**
   * @brief 解析結果を取得
   * @return 型解析結果
   */
  TypeAnalysisResult getResults() const;
  
  /**
   * @brief 組み込み型定義を登録
   */
  void registerBuiltinTypes();
  
  /**
   * @brief 型定義ファイルから型情報をロード
   * @param filename 型定義ファイルのパス
   * @return 成功した場合true
   */
  bool loadTypeDefinitions(const std::string& filename);
  
private:
  // 型推論メソッド（各ノード型専用）
  std::shared_ptr<TypeInfo> inferBinaryExpressionType(const ast::NodePtr& node, const std::shared_ptr<FlowTypeContext>& context);
  std::shared_ptr<TypeInfo> inferUnaryExpressionType(const ast::NodePtr& node, const std::shared_ptr<FlowTypeContext>& context);
  std::shared_ptr<TypeInfo> inferCallExpressionType(const ast::NodePtr& node, const std::shared_ptr<FlowTypeContext>& context);
  std::shared_ptr<TypeInfo> inferObjectExpressionType(const ast::NodePtr& node, const std::shared_ptr<FlowTypeContext>& context);
  std::shared_ptr<TypeInfo> inferArrayExpressionType(const ast::NodePtr& node, const std::shared_ptr<FlowTypeContext>& context);
  std::shared_ptr<TypeInfo> inferFunctionType(const ast::NodePtr& node, const std::shared_ptr<FlowTypeContext>& context);
  std::shared_ptr<TypeInfo> inferIdentifierType(const ast::NodePtr& node, const std::shared_ptr<FlowTypeContext>& context);
  std::shared_ptr<TypeInfo> inferMemberExpressionType(const ast::NodePtr& node, const std::shared_ptr<FlowTypeContext>& context);
  std::shared_ptr<TypeInfo> inferConditionalExpressionType(const ast::NodePtr& node, const std::shared_ptr<FlowTypeContext>& context);
  std::shared_ptr<TypeInfo> inferAssignmentExpressionType(const ast::NodePtr& node, const std::shared_ptr<FlowTypeContext>& context);

  // ヘルパーメソッド
  std::shared_ptr<TypeInfo> createPrimitiveType(PreciseType type);
  std::shared_ptr<TypeInfo> createFunctionType(const ast::NodePtr& functionNode);
  std::shared_ptr<TypeInfo> createObjectType(const std::unordered_map<std::string, std::shared_ptr<TypeInfo>>& properties);
  std::shared_ptr<TypeInfo> createArrayType(const std::shared_ptr<TypeInfo>& elementType, std::optional<size_t> length = std::nullopt);
  
  // 型の互換性チェック
  bool areTypesCompatible(const std::shared_ptr<TypeInfo>& source, const std::shared_ptr<TypeInfo>& target);
  
  // 型パターンマッチング（型推論の精度向上用）
  void applyTypePatternMatching(const ast::NodePtr& node, std::shared_ptr<TypeInfo>& inferredType);
  
  // メンバースタック（スコープ管理）
  struct Scope {
    std::string name;
    std::unordered_map<std::string, std::shared_ptr<TypeInfo>> variables;
  };
  std::vector<Scope> m_scopeStack;
  
  // 解析結果
  TypeAnalysisResult m_results;
  
  // 制約ソルバー
  TypeConstraintSolver m_constraintSolver;
  
  // 型キャッシュ（パフォーマンス向上用）
  std::unordered_map<size_t, std::shared_ptr<TypeInfo>> m_typeCache;
  
  // グローバル型定義
  std::unordered_map<std::string, std::shared_ptr<TypeInfo>> m_builtinTypes;
  
  // 型パターンマッチング定義
  struct TypePattern {
    std::function<bool(const ast::NodePtr&)> matcher;
    std::function<void(const ast::NodePtr&, std::shared_ptr<TypeInfo>&)> enhancer;
  };
  std::vector<TypePattern> m_typePatterns;
  
  // 型パターン初期化
  void initializeTypePatterns();
  
  // フローの追跡コンテキスト
  std::shared_ptr<FlowTypeContext> m_currentFlowContext;
};

/**
 * @struct TypeInferenceOptions
 * @brief 型推論の挙動を制御するオプション
 */
struct TypeInferenceOptions {
  bool enableFlowAnalysis = true;              ///< フロー解析を有効にする
  bool enableConstraintSolving = true;         ///< 制約ソルバーを有効にする
  bool enableTypePatternMatching = true;       ///< パターンマッチングを有効にする
  bool enableStrictNullChecks = false;         ///< 厳格なnullチェックを有効にする
  bool reportImplicitAny = true;               ///< any型の暗黙的使用を報告
  bool inferReturnTypes = true;                ///< 関数の戻り値型を推論
  bool inferObjectLiteralTypes = true;         ///< オブジェクトリテラルの型を推論
  bool inferTypeFromUsage = true;              ///< 使用方法から型を推論
  bool preserveTypeAssertions = true;          ///< 型アサーションを維持
  bool inferConstantTypes = true;              ///< 定数から型を推論
  bool generateTypeRecommendations = true;     ///< 型の推奨事項を生成
  bool detectUndefinedProperties = true;       ///< 未定義プロパティアクセスを検出
  bool optimizeBasedOnTypes = true;            ///< 型に基づく最適化を有効化
  int maxRecursionDepth = 5;                   ///< 再帰型推論の最大深度
  int maxTypeUnionSize = 5;                    ///< ユニオン型の最大サイズ
  size_t maxTypeProcessingTime = 100;          ///< 型処理の最大時間（ミリ秒）
  
  /**
   * @brief オプションの文字列表現
   * @return JSONフォーマットのオプション
   */
  std::string toString() const;
  
  /**
   * @brief 文字列からオプションを復元
   * @param str JSONフォーマットのオプション
   * @return 設定されたオプション
   */
  static TypeInferenceOptions fromString(const std::string& str);
};

/**
 * @class TypeInferenceTransformer
 * @brief JavaScriptコード内の変数や式の型を推論する高度なトランスフォーマークラス
 */
class TypeInferenceTransformer : public Transformer {
 public:
  /**
   * @brief コンストラクタ
   */
  TypeInferenceTransformer();
  
  /**
   * @brief オプション付きコンストラクタ
   * @param options 型推論オプション
   */
  explicit TypeInferenceTransformer(const TypeInferenceOptions& options);

  /**
   * @brief デストラクタ
   */
  ~TypeInferenceTransformer() override;

  /**
   * @brief 型推論オプションを設定
   * @param options 新しいオプション
   */
  void setOptions(const TypeInferenceOptions& options);
  
  /**
   * @brief 現在のオプションを取得
   * @return 現在の型推論オプション
   */
  const TypeInferenceOptions& getOptions() const;

  /**
   * @brief 推論された型の総数を取得
   * @return 推論された型の数
   */
  size_t getInferredTypeCount() const;

  /**
   * @brief 型推論された変数の総数を取得
   * @return 型推論された変数の数
   */
  size_t getInferredVariableCount() const;

  /**
   * @brief 型の不一致警告の総数を取得
   * @return 型の不一致警告の数
   */
  size_t getTypeMismatchWarningCount() const;

  /**
   * @brief 型推論結果を取得
   * @return 型解析結果
   */
  TypeAnalysisResult getAnalysisResults() const;
  
  /**
   * @brief 型推論結果を外部ファイルに保存
   * @param filename 出力ファイル名
   * @param format 出力形式（"json"または"html"）
   * @return 成功した場合true
   */
  bool saveResults(const std::string& filename, const std::string& format = "json") const;

protected:
  /**
   * @brief コンテキスト付きのノード変換
   * @param node 変換対象のノード
   * @param context 変換コンテキスト
   * @return 変換結果
   */
  TransformResult TransformNodeWithContext(parser::ast::NodePtr node, TransformContext& context) override;

 private:
  // 各ノードタイプの処理
  TransformResult processProgram(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processBlockStatement(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processExpressionStatement(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processIfStatement(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processReturnStatement(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processFunctionDeclaration(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processVariableDeclaration(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processAssignmentExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processBinaryExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processUnaryExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processCallExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processObjectExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processArrayExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processMemberExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processIdentifier(parser::ast::NodePtr node, TransformContext& context);
  TransformResult processLiteral(parser::ast::NodePtr node, TransformContext& context);
  
  // 型に基づく最適化機会を特定
  void identifyOptimizationOpportunities(const ast::NodePtr& node, const std::shared_ptr<TypeInfo>& type);
  
  // 型アノテーションと型チェックの挿入
  parser::ast::NodePtr insertTypeAnnotation(parser::ast::NodePtr node, const std::shared_ptr<TypeInfo>& type);
  parser::ast::NodePtr insertTypeCheck(parser::ast::NodePtr node, const std::shared_ptr<TypeInfo>& expectedType);
  
  // ノード間のデータフロー解析
  void analyzeDataFlow(const ast::NodePtr& program);
  
  // 初期宣言のスキャン
  void scanDeclarations(const ast::NodePtr& program);

  // 型エラーの診断
  void diagnoseTypeErrors();
  
  // 型ベースの最適化
  void applyTypeBasedOptimizations(ast::NodePtr& node);
  
  // 型推論エンジン
  std::unique_ptr<TypeInferenceEngine> m_engine;
  
  // 型推論設定
  TypeInferenceOptions m_options;
  
  // 型タグ付けキャッシュ（ノードID -> 型情報）
  std::unordered_map<size_t, std::shared_ptr<TypeInfo>> m_nodeTypes;
  
  // 統計情報
  std::atomic<size_t> m_inferredTypes{0};
  std::atomic<size_t> m_inferredVariables{0};
  std::atomic<size_t> m_typeMismatchWarnings{0};
  std::atomic<size_t> m_optimizationsApplied{0};
  
  // スレッド安全な操作のためのミューテックス
  mutable std::shared_mutex m_mutex;
};

}  // namespace transformers
}  // namespace aero

#endif  // AERO_TYPE_INFERENCE_H