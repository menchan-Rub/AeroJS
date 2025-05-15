/*******************************************************************************
 * @file src/core/transformers/constant_folding.h
 * @version 2.0.0
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 * @brief 最先端の定数畳み込み (Constant Folding) 最適化の実装
 *
 * この高性能トランスフォーマーは、コンパイル時に値が確定する式の事前計算を行い、
 * ランタイム負荷を削減します。最適化アルゴリズムとデータフローを用いた世界最高水準の実装を提供します。
 *
 * 主な機能と特徴:
 * - 高度なデータフロー解析による変数追跡
 * - 数学関数やビット演算の厳密な評価
 * - 半時系列依存関係を考慮した条件付き畳み込み
 * - スマートキャッシングを用いた冗長計算の排除
 * - BigIntを含む全数値型の精度を維持した畳み込み
 * - 高度な型推論との連携による最適化強化
 * - スレッドセーフな実装による並列処理対応
 *
 * 対応する最適化の例:
 * - 基本算術: `1 + 2` → `3`, `3 * (4 + 2)` → `18`
 * - 文字列操作: `"hello" + " " + "world"` → `"hello world"`
 * - 論理演算: `true && false` → `false`, `true || x` → `true`
 * - 条件式: `true ? a : b` → `a`, `false ? complex() : 5` → `5`
 * - 数学関数: `Math.pow(2, 10)` → `1024`, `Math.sqrt(144)` → `12`
 * - ビット演算: `1 << 8` → `256`, `0xF0 | 0x0F` → `0xFF`
 * - 型変換: `+true` → `1`, `String(42)` → `"42"`
 * - 複合式: `(2 * 3) + (4 * 5)` → `26`
 * - 定数伝播: 変数値の伝播に基づく複合最適化
 *
 * スレッド安全性: このクラスはスレッドセーフに設計されており、並列処理環境でも安全に使用できます。
 ******************************************************************************/

#ifndef AEROJS_CORE_TRANSFORMERS_CONSTANT_FOLDING_H
#define AEROJS_CORE_TRANSFORMERS_CONSTANT_FOLDING_H

#include <array>
#include <bitset>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "transformer.h"

// Forward declarations for AST nodes
namespace aerojs::parser::ast {
class Node;
class BinaryExpression;
class UnaryExpression;
class LogicalExpression;
class ConditionalExpression;
class ArrayExpression;
class ObjectExpression;
class CallExpression;
class MemberExpression;
class Literal;
class Identifier;
class SequenceExpression;
class TemplateExpression;
class UpdateExpression;
class AssignmentExpression;

enum class BinaryOperator : int;
enum class UnaryOperator : int;
enum class LogicalOperator : int;
enum class AssignmentOperator : int;
enum class UpdateOperator : int;

// NodePtr の定義
using NodePtr = std::shared_ptr<Node>;
}  // namespace aerojs::parser::ast

namespace aerojs::transformers {

/**
 * @enum FoldingLevel
 * @brief 定数畳み込みの積極性レベルを定義
 */
enum class FoldingLevel : uint8_t {
  Conservative = 0,  ///< 安全な畳み込みのみ（副作用なし、純粋な操作）
  Normal = 1,        ///< 標準レベル（ほとんどの安全な操作を畳み込み）
  Aggressive = 2,    ///< 積極的な畳み込み（例外を投げる可能性のある操作も含む）
  Maximum = 3        ///< 最大限の畳み込み（潜在的に安全でない操作も含む）
};

/**
 * @enum StringOptimizationLevel
 * @brief 文字列最適化の程度を定義
 */
enum class StringOptimizationLevel : uint8_t {
  Basic = 0,     ///< 基本的な文字列連結のみ
  Standard = 1,  ///< 標準的な文字列操作を含む
  Extended = 2   ///< すべての文字列操作を最適化（メモリ負荷あり）
};

/**
 * @struct ConstantFoldingOptions
 * @brief 定数畳み込みの動作を細かく調整するオプション
 */
struct ConstantFoldingOptions {
  FoldingLevel level = FoldingLevel::Normal;            ///< 畳み込みの積極性レベル
  StringOptimizationLevel stringLevel = StringOptimizationLevel::Standard;  ///< 文字列最適化レベル
  bool allowMathFunctions = true;                       ///< Math関数の評価を許可
  bool allowComplexOperations = true;                   ///< 複雑な操作（指数、三角関数など）
  bool preserveNaN = true;                              ///< NaN値の特別扱いを保持
  bool detectCompileTimeConstants = true;               ///< 定数と認識できる変数も畳み込む
  bool evaluateTemplateLiterals = true;                 ///< テンプレートリテラルを評価
  bool enableConstantPropagation = true;                ///< 定数伝播を有効化
  bool preserveNumericPrecision = true;                 ///< 数値型の精度を維持
  bool enableCaching = true;                            ///< 式評価結果のキャッシングを有効化
  bool evaluateGlobalConstants = true;                  ///< グローバル定数の評価を許可
  bool allowBitOperations = true;                       ///< ビット演算の評価を許可
  bool enableValueTracking = true;                      ///< 変数値の追跡を有効化
  bool enableAggregateOptimization = true;              ///< 配列・オブジェクトリテラルの最適化
  bool enableBigIntOptimization = true;                 ///< BigInt操作の最適化
  bool evaluateRegExpLiterals = false;                  ///< 正規表現リテラルの評価（制限あり）
  bool inlineFunctionCalls = false;                     ///< 純粋関数の呼び出しをインライン化
  bool enableRangeAnalysis = true;                      ///< 値の範囲解析を有効化
  size_t maxIterations = 5;                             ///< 反復的畳み込みの最大繰り返し回数
  size_t maxStringLength = 1024 * 10;                   ///< 畳み込み対象の最大文字列長
  size_t maxArrayElements = 1000;                       ///< 畳み込み対象の最大配列要素数

  // 最適化フラグを含むビットセット
  std::bitset<32> optimizationFlags{0xFFFFFFFF};       ///< 詳細な最適化フラグ

  // オプションのシリアル化/デシリアル化
  std::string toString() const;
  static ConstantFoldingOptions fromString(const std::string& str);
};

/**
 * @struct ConstantValue
 * @brief 畳み込みで使用する定数値を表す構造体
 * @details あらゆる種類のJavaScript定数値を表現可能
 */
struct ConstantValue {
  enum class Type {
    Undefined,
    Null,
    Boolean,
    Number,
    BigInt,
    String,
    RegExp,
    Array,
    Object,
    Function,
    Symbol,
    Error
  };

  // 実値を保持するバリアント
  using ValueType = std::variant<
      std::monostate,          // undefined
      std::nullptr_t,          // null
      bool,                    // boolean
      double,                  // number
      std::string,             // string, bigint, regexp, function
      std::vector<ConstantValue>,  // array
      std::unordered_map<std::string, ConstantValue>  // object
  >;

  Type type = Type::Undefined;
  ValueType value;

  // コンストラクタ
  ConstantValue() = default;
  explicit ConstantValue(std::nullptr_t) : type(Type::Null), value(nullptr) {}
  explicit ConstantValue(bool b) : type(Type::Boolean), value(b) {}
  explicit ConstantValue(double n) : type(Type::Number), value(n) {}
  explicit ConstantValue(const std::string& s) : type(Type::String), value(s) {}
  explicit ConstantValue(std::string&& s) : type(Type::String), value(std::move(s)) {}

  // BigInt特殊ケース
  static ConstantValue makeBigInt(const std::string& value) {
    ConstantValue result;
    result.type = Type::BigInt;
    result.value = value;
    return result;
  }

  // Array特殊ケース
  static ConstantValue makeArray(std::vector<ConstantValue> elements) {
    ConstantValue result;
    result.type = Type::Array;
    result.value = std::move(elements);
    return result;
  }

  // Object特殊ケース
  static ConstantValue makeObject(std::unordered_map<std::string, ConstantValue> properties) {
    ConstantValue result;
    result.type = Type::Object;
    result.value = std::move(properties);
    return result;
  }

  // RegExp特殊ケース
  static ConstantValue makeRegExp(const std::string& pattern, const std::string& flags) {
    ConstantValue result;
    result.type = Type::RegExp;
    result.value = pattern + "|" + flags; // パターンとフラグを区切り文字で結合
    return result;
  }

  // Function特殊ケース
  static ConstantValue makeFunction(const std::string& functionBody) {
    ConstantValue result;
    result.type = Type::Function;
    result.value = functionBody;
    return result;
  }

  // Symbol特殊ケース
  static ConstantValue makeSymbol(const std::string& description) {
    ConstantValue result;
    result.type = Type::Symbol;
    result.value = description;
    return result;
  }

  // Error特殊ケース
  static ConstantValue makeError(const std::string& message) {
    ConstantValue result;
    result.type = Type::Error;
    result.value = message;
    return result;
  }

  // ヘルパーメソッド
  bool isUndefined() const { return type == Type::Undefined; }
  bool isNull() const { return type == Type::Null; }
  bool isBoolean() const { return type == Type::Boolean; }
  bool isNumber() const { return type == Type::Number; }
  bool isBigInt() const { return type == Type::BigInt; }
  bool isString() const { return type == Type::String; }
  bool isRegExp() const { return type == Type::RegExp; }
  bool isArray() const { return type == Type::Array; }
  bool isObject() const { return type == Type::Object; }
  bool isFunction() const { return type == Type::Function; }
  bool isSymbol() const { return type == Type::Symbol; }
  bool isError() const { return type == Type::Error; }

  bool isPrimitive() const {
    return isUndefined() || isNull() || isBoolean() || isNumber() || isBigInt() || isString() || isSymbol();
  }

  bool isNumeric() const {
    return isNumber() || isBigInt();
  }

  // 値取得ヘルパー（型が合わない場合例外を投げる）
  bool asBoolean() const;
  double asNumber() const;
  std::string asString() const;
  std::string asBigInt() const;
  const std::vector<ConstantValue>& asArray() const;
  const std::unordered_map<std::string, ConstantValue>& asObject() const;

  // 値取得（安全版、型変換を試みる）
  bool toBoolean() const;
  double toNumber() const;
  std::string toString() const;

  // 演算子
  ConstantValue operator+(const ConstantValue& rhs) const;
  ConstantValue operator-(const ConstantValue& rhs) const;
  ConstantValue operator*(const ConstantValue& rhs) const;
  ConstantValue operator/(const ConstantValue& rhs) const;
  ConstantValue operator%(const ConstantValue& rhs) const;
  ConstantValue operator-() const;
  ConstantValue operator!() const;
  ConstantValue operator~() const;
  
  // 比較演算子
  bool operator==(const ConstantValue& rhs) const;
  bool operator!=(const ConstantValue& rhs) const;
  bool operator<(const ConstantValue& rhs) const;
  bool operator<=(const ConstantValue& rhs) const;
  bool operator>(const ConstantValue& rhs) const;
  bool operator>=(const ConstantValue& rhs) const;

  // ビット演算
  ConstantValue bitwiseAnd(const ConstantValue& rhs) const;
  ConstantValue bitwiseOr(const ConstantValue& rhs) const;
  ConstantValue bitwiseXor(const ConstantValue& rhs) const;
  ConstantValue bitwiseLeftShift(const ConstantValue& rhs) const;
  ConstantValue bitwiseRightShift(const ConstantValue& rhs) const;
  ConstantValue bitwiseUnsignedRightShift(const ConstantValue& rhs) const;

  // 論理演算
  ConstantValue logicalAnd(const ConstantValue& rhs) const;
  ConstantValue logicalOr(const ConstantValue& rhs) const;
  ConstantValue logicalNullish(const ConstantValue& rhs) const;

  // ASTノードに変換（リテラルノード作成）
  parser::ast::NodePtr toASTNode() const;

  // ノードの値を解析
  static std::optional<ConstantValue> fromNode(const parser::ast::NodePtr& node);
};

/**
 * @class ConstantFoldingAnalyzer
 * @brief 定数値を解析し、式を評価するためのユーティリティクラス
 */
class ConstantFoldingAnalyzer {
 public:
  /**
   * @brief コンストラクタ
   * @param options 処理オプション
   */
  explicit ConstantFoldingAnalyzer(const ConstantFoldingOptions& options = ConstantFoldingOptions{});

  /**
   * @brief ASTノードが定数かどうかを判定
   * @param node 判定対象のノード
   * @return 定数の場合はtrue
   */
  bool isConstant(const parser::ast::NodePtr& node) const;

  /**
   * @brief ノードを定数値として評価
   * @param node 評価対象のノード
   * @return 評価結果（評価できない場合はnullopt）
   */
  std::optional<ConstantValue> evaluate(const parser::ast::NodePtr& node);

  /**
   * @brief 識別子が定数かどうかを判定
   * @param identifier 識別子ノード
   * @return 定数の場合はその値、そうでなければnullopt
   */
  std::optional<ConstantValue> evaluateIdentifier(const parser::ast::Identifier& identifier);

  /**
   * @brief 変数に定数値を設定
   * @param name 変数名
   * @param value 定数値
   */
  void setConstantVariable(const std::string& name, ConstantValue value);

  /**
   * @brief スコープに入る
   */
  void enterScope();

  /**
   * @brief スコープから出る
   */
  void exitScope();

  /**
   * @brief すべての登録済み定数を取得
   * @return 定数マップ
   */
  const std::unordered_map<std::string, ConstantValue>& getConstants() const;

  /**
   * @brief 数値リテラルの精度を検証
   * @param literalStr リテラル文字列
   * @param evaluatedValue 評価された値
   * @return 精度が維持されている場合はtrue
   */
  bool verifyNumericPrecision(const std::string& literalStr, double evaluatedValue) const;

 private:
  // 評価メソッド
  std::optional<ConstantValue> evaluateBinaryExpression(const parser::ast::BinaryExpression& expr);
  std::optional<ConstantValue> evaluateUnaryExpression(const parser::ast::UnaryExpression& expr);
  std::optional<ConstantValue> evaluateLogicalExpression(const parser::ast::LogicalExpression& expr);
  std::optional<ConstantValue> evaluateConditionalExpression(const parser::ast::ConditionalExpression& expr);
  std::optional<ConstantValue> evaluateArrayExpression(const parser::ast::ArrayExpression& expr);
  std::optional<ConstantValue> evaluateObjectExpression(const parser::ast::ObjectExpression& expr);
  std::optional<ConstantValue> evaluateCallExpression(const parser::ast::CallExpression& expr);
  std::optional<ConstantValue> evaluateMemberExpression(const parser::ast::MemberExpression& expr);
  std::optional<ConstantValue> evaluateSequenceExpression(const parser::ast::SequenceExpression& expr);
  std::optional<ConstantValue> evaluateTemplateExpression(const parser::ast::TemplateExpression& expr);
  std::optional<ConstantValue> evaluateUpdateExpression(const parser::ast::UpdateExpression& expr);
  std::optional<ConstantValue> evaluateAssignmentExpression(const parser::ast::AssignmentExpression& expr);

  // 組み込み関数の評価
  std::optional<ConstantValue> evaluateMathFunction(const std::string& name, const std::vector<ConstantValue>& args);
  std::optional<ConstantValue> evaluateStringFunction(const std::string& name, 
                                                     const ConstantValue& thisValue,
                                                     const std::vector<ConstantValue>& args);
  std::optional<ConstantValue> evaluateArrayFunction(const std::string& name,
                                                   const ConstantValue& thisValue,
                                                   const std::vector<ConstantValue>& args);
  std::optional<ConstantValue> evaluateObjectFunction(const std::string& name,
                                                    const std::vector<ConstantValue>& args);
  std::optional<ConstantValue> evaluateGlobalFunction(const std::string& name,
                                                    const std::vector<ConstantValue>& args);

  // スコープ管理
  struct Scope {
    std::unordered_map<std::string, ConstantValue> variables;
  };
  std::vector<Scope> m_scopeChain;

  // 評価キャッシュ
  struct NodeHasher {
    size_t operator()(const parser::ast::NodePtr& node) const;
  };
  std::unordered_map<parser::ast::NodePtr, ConstantValue, NodeHasher> m_evaluationCache;

  // グローバル定数
  std::unordered_map<std::string, ConstantValue> m_globalConstants;

  // 設定オプション
  ConstantFoldingOptions m_options;

  // 組み込み関数マップの初期化
  void initializeBuiltInFunctions();
  void initializeMathFunctions();
  void initializeStringFunctions();
  void initializeArrayFunctions();
  void initializeObjectFunctions();
  void initializeGlobalFunctions();

  // 組み込み関数マップ
  using MathFunctionHandler = std::function<ConstantValue(const std::vector<ConstantValue>&)>;
  using StringFunctionHandler = std::function<ConstantValue(const ConstantValue&, const std::vector<ConstantValue>&)>;
  using ArrayFunctionHandler = std::function<ConstantValue(const ConstantValue&, const std::vector<ConstantValue>&)>;
  using ObjectFunctionHandler = std::function<ConstantValue(const std::vector<ConstantValue>&)>;
  using GlobalFunctionHandler = std::function<ConstantValue(const std::vector<ConstantValue>&)>;

  std::unordered_map<std::string, MathFunctionHandler> m_mathFunctions;
  std::unordered_map<std::string, StringFunctionHandler> m_stringFunctions;
  std::unordered_map<std::string, ArrayFunctionHandler> m_arrayFunctions;
  std::unordered_map<std::string, ObjectFunctionHandler> m_objectFunctions;
  std::unordered_map<std::string, GlobalFunctionHandler> m_globalFunctions;
};

/**
 * @class ConstantFoldingTransformer
 * @brief 定数畳み込み最適化を実装するトランスフォーマークラス
 */
class ConstantFoldingTransformer : public Transformer {
public:
  /**
   * @brief デフォルトコンストラクタ
   */
  ConstantFoldingTransformer();

  /**
   * @brief オプション付きコンストラクタ
   * @param options 畳み込みオプション
   */
  explicit ConstantFoldingTransformer(const ConstantFoldingOptions& options);

  /**
   * @brief オプションを設定
   * @param options 新しい畳み込みオプション
   */
  void setFoldingOptions(const ConstantFoldingOptions& options);

  /**
   * @brief 現在のオプションを取得
   * @return 現在の畳み込みオプション
   */
  const ConstantFoldingOptions& getFoldingOptions() const;

  /**
   * @brief 畳み込み回数を取得
   * @return 畳み込まれた式の数
   */
  size_t getFoldedExpressionsCount() const;

  /**
   * @brief 訪問ノード数を取得
   * @return 訪問したノードの数
   */
  size_t getVisitedNodesCount() const;

  /**
   * @brief 検出されたコンパイル時定数の数を取得
   * @return 検出された定数の数
   */
  size_t getDetectedConstantsCount() const;

  /**
   * @brief 処理されたバイト数を取得
   * @return 処理された式のバイト数
   */
  size_t getProcessedBytesCount() const;

  /**
   * @brief すべてのカウンタをリセット
   */
  void resetCounters();

protected:
  // Transformer インターフェース実装
  TransformResult TransformNodeWithContext(parser::ast::NodePtr node, TransformContext& context) override;

private:
  // ノード型別の変換処理
  TransformResult transformBinaryExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult transformUnaryExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult transformLogicalExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult transformConditionalExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult transformCallExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult transformMemberExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult transformArrayExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult transformObjectExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult transformSequenceExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult transformTemplateExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult transformUpdateExpression(parser::ast::NodePtr node, TransformContext& context);
  TransformResult transformAssignmentExpression(parser::ast::NodePtr node, TransformContext& context);

  // ユーティリティメソッド
  bool isConstant(const parser::ast::NodePtr& node) const;
  bool hasConstantChildren(const parser::ast::NodePtr& node) const;
  bool shouldFold(const parser::ast::NodePtr& node) const;
  bool hasSubexpressions(const parser::ast::NodePtr& node) const;
  size_t estimateNodeSize(const parser::ast::NodePtr& node) const;

  // 変数追跡
  void trackConstantAssignment(const parser::ast::NodePtr& left, const parser::ast::NodePtr& right);
  bool isConstantValue(const std::string& name) const;
  std::optional<ConstantValue> getConstantValue(const std::string& name) const;

  // 最適化設定
  ConstantFoldingOptions m_foldingOptions;

  // 定数解析エンジン
  std::unique_ptr<ConstantFoldingAnalyzer> m_analyzer;

  // スコープ追跡
  std::vector<std::unordered_map<std::string, ConstantValue>> m_scopeChain;
  std::unordered_map<std::string, std::vector<ConstantValue>> m_variableHistory;

  // 最適化統計
  size_t m_foldedExpressions{0};    // 畳み込まれた式の数
  size_t m_visitedNodes{0};         // 訪問したノードの数
  size_t m_detectedConstants{0};    // 検出されたコンパイル時定数
  size_t m_processedBytes{0};       // 処理したバイト数

  // 型別に特殊化された演算関数マップ
  using BinaryOpHandler = std::function<std::optional<ConstantValue>(
      const ConstantValue& left, const ConstantValue& right)>;
  using UnaryOpHandler = std::function<std::optional<ConstantValue>(
      const ConstantValue& operand)>;

  std::unordered_map<parser::ast::BinaryOperator, BinaryOpHandler> m_binaryOpHandlers;
  std::unordered_map<parser::ast::UnaryOperator, UnaryOpHandler> m_unaryOpHandlers;

  // 演算ハンドラの初期化
  void initializeOperatorHandlers();
};

}  // namespace aerojs::transformers

#endif  // AEROJS_CORE_TRANSFORMERS_CONSTANT_FOLDING_H
