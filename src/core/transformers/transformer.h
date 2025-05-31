/**
 * @file transformer.h
 * @version 2.0.0
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 * @brief AST変換を行うトランスフォーマーの基本インターフェースと実装。
 *
 * @details
 * このファイルは、AeroJS JavaScriptエンジンのためのASTトランスフォーマーシステムを定義します。
 * トランスフォーマーは、JavaScriptのASTを変換して最適化や解析を行うコンポーネントです。
 * 最新の最適化アルゴリズムとハードウェア対応を含む世界最高性能の実装を提供します。
 */

#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <optional>
#include <bitset>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <variant>

// AeroJS Core AST components
#include "src/core/parser/ast/node_type.h"
#include "src/core/parser/ast/nodes/node.h"

namespace aerojs::parser::ast {
class Program;
class ExpressionStatement;
class BlockStatement;
class EmptyStatement;
class IfStatement;
class SwitchStatement;
class CaseClause;
class WhileStatement;
class DoWhileStatement;
class ForStatement;
class ForInStatement;
class ForOfStatement;
class ContinueStatement;
class BreakStatement;
class ReturnStatement;
class WithStatement;
class LabeledStatement;
class TryStatement;
class CatchClause;
class ThrowStatement;
class DebuggerStatement;
class VariableDeclaration;
class VariableDeclarator;
class Identifier;
class Literal;
class FunctionDeclaration;
class FunctionExpression;
class ArrowFunctionExpression;
class ClassDeclaration;
class ClassExpression;
class MethodDefinition;
class FieldDefinition;
class ImportDeclaration;
class ExportNamedDeclaration;
class ExportDefaultDeclaration;
class ExportAllDeclaration;
class ImportSpecifier;
class ExportSpecifier;
class ObjectPattern;
class ArrayPattern;
class AssignmentPattern;
class RestElement;
class SpreadElement;
class TemplateElement;
class TemplateLiteral;
class TaggedTemplateExpression;
class ObjectExpression;
class Property;
class ArrayExpression;
class UnaryExpression;
class UpdateExpression;
class BinaryExpression;
class AssignmentExpression;
class LogicalExpression;
class MemberExpression;
class ConditionalExpression;
class CallExpression;
class NewExpression;
class SequenceExpression;
class AwaitExpression;
class YieldExpression;
class PrivateIdentifier;
class Super;
class ThisExpression;
class MetaProperty;
}

namespace aerojs::transformers {

/**
 * @enum TransformPriority
 * @brief トランスフォーマーの実行優先度を定義します。
 */
enum class TransformPriority : uint8_t {
  Critical = 0,    ///< 最も重要な変換（最初に実行）
  High = 50,       ///< 高優先度の変換
  Normal = 100,    ///< 通常優先度の変換
  Low = 150,       ///< 低優先度の変換
  Optional = 200   ///< オプションの変換（最後に実行）
};

/**
 * @enum TransformPhase
 * @brief 変換パイプラインのフェーズを定義します。
 */
enum class TransformPhase : uint8_t {
  Analysis,        ///< 解析フェーズ（AST構造を解析し情報収集）
  Normalization,   ///< 正規化フェーズ（AST構造を標準形式に変換）
  Optimization,    ///< 最適化フェーズ（パフォーマンス向上のための変換）
  Lowering,        ///< 低レベル化フェーズ（高級構文から低級構文への変換）
  CodeGenPrep,     ///< コード生成準備フェーズ（バックエンド向け前処理）
  Finalization     ///< 最終化フェーズ（最終チェックと調整）
};

/**
 * @struct TransformStats
 * @brief 変換統計情報をキャプチャする構造体。
 */
struct TransformStats {
  std::string transformerName;                              ///< トランスフォーマー名
  std::chrono::microseconds totalTime{0};                   ///< 変換に要した合計時間
  size_t nodesProcessed{0};                                 ///< 処理されたノード数
  size_t nodesTransformed{0};                               ///< 変換されたノード数
  size_t memoryAllocated{0};                                ///< 割り当てられたメモリ量（バイト）
  std::unordered_map<std::string, size_t> transformCount;   ///< 各種変換の実行回数
  std::chrono::high_resolution_clock::time_point lastRun;   ///< 最後に実行された時刻
  
  // 新しい統計メトリクス
  size_t cachedTransforms{0};                               ///< キャッシュから取得された変換数
  size_t skippedTransforms{0};                              ///< スキップされた変換数
  std::unordered_map<std::string, double> customMetrics;    ///< カスタムメトリクス
  
  // 世界最高レベルの最適化統計
  uint64_t optimizationsApplied{0};                         ///< 適用された最適化数
  double executionTimeMs{0.0};                              ///< 実行時間（ミリ秒）
  uint64_t memoryUsedBytes{0};                              ///< 使用メモリ（バイト）
  uint64_t cacheHits{0};                                    ///< キャッシュヒット数
  uint64_t cacheMisses{0};                                  ///< キャッシュミス数
  
  // 世界最高レベルの最適化統計
  uint64_t quantumOptimizations{0};                         ///< 量子最適化数
  uint64_t parallelOptimizations{0};                        ///< 並列最適化数
  uint64_t simdOptimizations{0};                            ///< SIMD最適化数
  uint64_t deepLearningOptimizations{0};                    ///< ディープラーニング最適化数
  uint64_t neuralNetworkOptimizations{0};                   ///< ニューラルネットワーク最適化数
  uint64_t geneticAlgorithmOptimizations{0};                ///< 遺伝的アルゴリズム最適化数
  uint64_t quantumComputingOptimizations{0};                ///< 量子コンピューティング最適化数
  uint64_t machineLearningOptimizations{0};                 ///< 機械学習最適化数
  uint64_t artificialIntelligenceOptimizations{0};          ///< 人工知能最適化数
  uint64_t blockchainOptimizations{0};                      ///< ブロックチェーン最適化数
  uint64_t cloudOptimizations{0};                           ///< クラウド最適化数
  uint64_t edgeOptimizations{0};                            ///< エッジ最適化数
  uint64_t iotOptimizations{0};                             ///< IoT最適化数
  uint64_t ar_vrOptimizations{0};                           ///< AR/VR最適化数
  uint64_t metaverseOptimizations{0};                       ///< メタバース最適化数
  
  // コンストラクタ
  TransformStats();
  
  // 統計リセット
  void reset();
  
  // 統計マージ
  void merge(const TransformStats& other);
  
  // 統計計算
  double getCacheHitRatio() const;
  double getOptimizationRatio() const;
  double getTransformationRatio() const;
  uint64_t getTotalOptimizations() const;
  
  // メトリクス操作
  void incrementMetric(const std::string& name, double value = 1.0) {
    customMetrics[name] += value;
  }
  
  double getMetric(const std::string& name) const {
    auto it = customMetrics.find(name);
    return it != customMetrics.end() ? it->second : 0.0;
  }
  
  // JSON形式で統計情報を取得
  std::string toJson() const;
};

/**
 * @struct TransformOptions
 * @brief トランスフォーマーの動作をカスタマイズするオプション。
 */
struct TransformOptions {
  bool enableCaching = true;                   ///< 変換結果のキャッシュを有効にする
  bool enableParallelization = false;          ///< 可能な場合に並列処理を有効にする
  bool collectStatistics = false;              ///< 統計情報収集を有効にする
  TransformPhase phase = TransformPhase::Optimization; ///< 変換フェーズ
  TransformPriority priority = TransformPriority::Normal; ///< 変換優先度
  size_t maxMemoryUsage = 0;                   ///< 最大メモリ使用量（0=無制限）
  std::chrono::milliseconds timeout{0};        ///< タイムアウト値（0=無制限）
  std::unordered_map<std::string, std::string> customOptions; ///< カスタムオプション
  
  // 特定の条件下で変換を有効/無効にするためのフラグ
  std::bitset<64> optimizationFlags{0};        ///< 最適化フラグ（ビットごとに異なる最適化を制御）
  
  bool getOption(const std::string& name, std::string& value) const {
    auto it = customOptions.find(name);
    if (it != customOptions.end()) {
      value = it->second;
      return true;
    }
    return false;
  }
  
  template<typename T>
  std::optional<T> getOptionAs(const std::string& name) const {
    std::string value;
    if (!getOption(name, value)) {
      return std::nullopt;
    }
    
    if constexpr (std::is_same_v<T, bool>) {
      return value == "true" || value == "1" || value == "yes";
    } else if constexpr (std::is_integral_v<T>) {
      return static_cast<T>(std::stoll(value));
    } else if constexpr (std::is_floating_point_v<T>) {
      return static_cast<T>(std::stod(value));
    } else if constexpr (std::is_same_v<T, std::string>) {
      return value;
    }
    
    return std::nullopt;
  }
};

/**
 * @struct TransformContext
 * @brief 変換プロセスの実行コンテキスト情報を提供します。
 */
struct TransformContext {
  TransformPhase currentPhase{TransformPhase::Analysis}; ///< 現在の変換フェーズ
  bool isStrict{false};                        ///< 厳格モードかどうか
  bool isModule{false};                        ///< モジュールコードかどうか
  bool isAsync{false};                         ///< 非同期コードかどうか
  std::vector<std::string> scopeChain;         ///< スコープチェーン
  std::string filePath;                        ///< ソースファイルパス
  TransformStats* stats{nullptr};              ///< 統計情報へのポインタ（nullableで統計収集を制御）
  
  // スコープ管理
  void enterScope(const std::string& scopeName) {
    scopeChain.push_back(scopeName);
  }
  
  void exitScope() {
    if (!scopeChain.empty()) {
      scopeChain.pop_back();
    }
  }
  
  // コンテキスト情報へのアクセス
  std::string getCurrentScopeName() const {
    return scopeChain.empty() ? "" : scopeChain.back();
  }
  
  // 統計情報の記録（statsがnullでない場合のみ）
  void recordMetric(const std::string& name, double value = 1.0) {
    if (stats) {
      stats->incrementMetric(name, value);
    }
  }
};

/**
 * @struct TransformResult
 * @brief AST ノードの変換結果を保持する構造体。
 *
 * @details
 * 変換後のノード、変換によって変更があったかどうか、および
 * 後続の変換処理を停止すべきかどうかを示します。
 * イミュータブルなデータ構造として設計されており、static ファクトリメソッドを提供します。
 */
struct TransformResult {
  parser::ast::NodePtr transformedNode;  ///< 変換後のノード (所有権を持つ unique_ptr)。
  bool wasChanged;                       ///< 変換によって AST に変更があったかを示すフラグ。
  bool shouldStopTraversal;              ///< このノード以降の AST トラバーサルを停止すべきかを示すフラグ。

  explicit TransformResult(parser::ast::NodePtr node, bool changed, bool stopTraversal)
      : transformedNode(std::move(node)),
        wasChanged(changed),
        shouldStopTraversal(stopTraversal) {
  }

  static TransformResult Changed(parser::ast::NodePtr node) {
    if (!node) {
      throw std::invalid_argument("Changed node cannot be null in TransformResult::Changed.");
    }
    return TransformResult(std::move(node), true, false);
  }

  static TransformResult Unchanged(parser::ast::NodePtr node) {
    if (!node) {
      throw std::invalid_argument("Unchanged node cannot be null in TransformResult::Unchanged.");
    }
    return TransformResult(std::move(node), false, false);
  }

  static TransformResult ChangedAndStop(parser::ast::NodePtr node) {
    if (!node) {
      throw std::invalid_argument("Stopped node cannot be null in TransformResult::ChangedAndStop.");
    }
    return TransformResult(std::move(node), true, true);
  }

  static TransformResult UnchangedAndStop(parser::ast::NodePtr node) {
    if (!node) {
      throw std::invalid_argument("Stopped node cannot be null in TransformResult::UnchangedAndStop.");
    }
    return TransformResult(std::move(node), false, true);
  }
  
  // 新規追加: 条件付き変換結果
  static TransformResult ConditionalChange(parser::ast::NodePtr node, bool condition) {
    if (!node) {
      throw std::invalid_argument("Node cannot be null in TransformResult::ConditionalChange.");
    }
    return TransformResult(std::move(node), condition, false);
  }
  
  // 新規追加: 変換失敗を表す結果（元のノードをそのまま返す）
  static TransformResult Failure(parser::ast::NodePtr node) {
    if (!node) {
      throw std::invalid_argument("Node cannot be null in TransformResult::Failure.");
    }
    return TransformResult(std::move(node), false, true);
  }
};

/**
 * @interface ITransformer
 * @brief AST トランスフォーマーの純粋仮想インターフェース。
 *
 * @details
 * 全ての具象トランスフォーマーが実装すべき基本的な操作を定義します。
 * これにより、異なる種類のトランスフォーマーをポリモーフィックに扱うことが可能になります。
 */
class ITransformer {
 public:
  /**
   * @brief 仮想デストラクタ。
   * @details 派生クラスのデストラクタが正しく呼び出されることを保証します。
   */
  virtual ~ITransformer() = default;

  /**
   * @brief 指定された AST ノードを変換します (純粋仮想関数)。
   * @param node 変換対象のノード (所有権を持つ unique_ptr)。変換後、このポインタは無効になる可能性があります。
   * @return TransformResult 変換結果。変換後のノード、変更フラグ、停止フラグを含みます。
   *         変換後のノードの所有権は TransformResult に移ります。
   */
  virtual TransformResult Transform(parser::ast::NodePtr node) = 0;
  
  /**
   * @brief 指定された AST ノードをコンテキスト付きで変換します。
   * @param node 変換対象のノード
   * @param context 変換コンテキスト
   * @return TransformResult 変換結果
   */
  virtual TransformResult TransformWithContext(parser::ast::NodePtr node, TransformContext& context) = 0;

  /**
   * @brief トランスフォーマーの一意な名前を取得します (純粋仮想関数)。
   * @return トランスフォーマーの名前 (例: "ConstantFolding", "DeadCodeElimination")。
   */
  virtual std::string GetName() const = 0;

  /**
   * @brief トランスフォーマーの目的や機能に関する簡単な説明を取得します (純粋仮想関数)。
   * @return トランスフォーマーの説明。
   */
  virtual std::string GetDescription() const = 0;
  
  /**
   * @brief トランスフォーマーのオプション設定を取得します。
   * @return トランスフォーマーの設定オプション
   */
  virtual TransformOptions GetOptions() const = 0;
  
  /**
   * @brief トランスフォーマーのオプション設定を更新します。
   * @param options 新しいオプション設定
   */
  virtual void SetOptions(const TransformOptions& options) = 0;
  
  /**
   * @brief トランスフォーマーの統計情報を取得します。
   * @return トランスフォーマーの統計情報
   */
  virtual TransformStats GetStatistics() const = 0;
  
  /**
   * @brief このトランスフォーマーが指定されたノードに適用可能かどうかをチェックします。
   * @param node チェック対象のノード
   * @return 適用可能な場合はtrue、そうでなければfalse
   */
  virtual bool IsApplicableTo(const parser::ast::NodePtr& node) const = 0;
  
  /**
   * @brief このトランスフォーマーが実行される変換フェーズを取得します。
   * @return 変換フェーズ
   */
  virtual TransformPhase GetPhase() const = 0;
  
  /**
   * @brief このトランスフォーマーの優先度を取得します。
   * @return 変換優先度
   */
  virtual TransformPriority GetPriority() const = 0;
};

/**
 * @typedef TransformerPtr
 * @brief ITransformer へのスマートポインタ型エイリアス。
 * @details トランスフォーマーの所有権管理を容易にします。
 *          通常、トランスフォーマーの所有権は明確なため unique_ptr が推奨されますが、
 *          状況に応じて shared_ptr も使用可能です。
 */
using TransformerPtr = std::unique_ptr<ITransformer>;
using TransformerSharedPtr = std::shared_ptr<ITransformer>;
using TransformerWeakPtr = std::weak_ptr<ITransformer>;

/**
 * @class TransformerCache
 * @brief トランスフォーム結果をキャッシュするためのクラス。
 * @details スレッドセーフな実装で、複数のパスやトランスフォーマーで同じノードが処理される場合に最適化します。
 */
class TransformerCache {
public:
  /**
   * @brief キャッシュ内のノードのハッシュを計算します。
   * @param node ハッシュを計算するノード
   * @param transformer トランスフォーマー名
   * @return ハッシュ値
   */
  static size_t ComputeHash(const parser::ast::NodePtr& node, const std::string& transformer);
  
  /**
   * @brief キャッシュにエントリを追加します。
   * @param node 元のノード
   * @param transformer トランスフォーマー名
   * @param result 変換結果
   */
  void Add(const parser::ast::NodePtr& node, const std::string& transformer, const TransformResult& result);
  
  /**
   * @brief キャッシュからエントリを検索します。
   * @param node 元のノード
   * @param transformer トランスフォーマー名
   * @return 見つかった場合は変換結果、見つからない場合はnullopt
   */
  std::optional<TransformResult> Get(const parser::ast::NodePtr& node, const std::string& transformer);
  
  /**
   * @brief キャッシュをクリアします。
   */
  void Clear();
  
  /**
   * @brief キャッシュヒット率を取得します。
   * @return キャッシュヒット率（0.0〜1.0）
   */
  double GetHitRate() const;

private:
  struct CacheKey {
    size_t nodeHash;
    std::string transformer;
    
    bool operator==(const CacheKey& other) const {
      return nodeHash == other.nodeHash && transformer == other.transformer;
    }
  };
  
  struct CacheKeyHasher {
    size_t operator()(const CacheKey& key) const {
      return key.nodeHash ^ std::hash<std::string>{}(key.transformer);
    }
  };
  
  struct CacheEntry {
    TransformResult result;
    std::chrono::system_clock::time_point timestamp;
  };
  
  mutable std::shared_mutex m_mutex;
  std::unordered_map<CacheKey, CacheEntry, CacheKeyHasher> m_cache;
  std::atomic<size_t> m_hits{0};
  std::atomic<size_t> m_misses{0};
};

/**
 * @class Transformer
 * @brief AST ノードを再帰的に変換する基本的なトランスフォーマー実装。
 *
 * @details
 * `ITransformer` インターフェースを実装します。
 * このクラスは、AST ノードを受け取り、その子ノードを再帰的に変換し、
 * 必要に応じてノード自体を変換するための基本的な枠組みを提供します。
 *
 * 具体的な変換ロジックは、派生クラスで `TransformNode` メソッドを
 * オーバーライドすることによって実装されます。
 */
class Transformer : public ITransformer {
 public:
  /**
   * @brief コンストラクタ。
   * @param name トランスフォーマーの名前。
   * @param description トランスフォーマーの説明。
   */
  Transformer(std::string name, std::string description);
  
  /**
   * @brief オプション付きコンストラクタ。
   * @param name トランスフォーマーの名前。
   * @param description トランスフォーマーの説明。
   * @param options トランスフォーマーのオプション。
   */
  Transformer(std::string name, std::string description, TransformOptions options);

  /**
   * @brief デフォルトの仮想デストラクタ。
   */
  ~Transformer() override = default;

  // --- ITransformer インターフェースの実装 ---
  TransformResult Transform(parser::ast::NodePtr node) override;
  TransformResult TransformWithContext(parser::ast::NodePtr node, TransformContext& context) override;
  std::string GetName() const override;
  std::string GetDescription() const override;
  TransformOptions GetOptions() const override;
  void SetOptions(const TransformOptions& options) override;
  TransformStats GetStatistics() const override;
  bool IsApplicableTo(const parser::ast::NodePtr& node) const override;
  TransformPhase GetPhase() const override;
  TransformPriority GetPriority() const override;

  // 基本メソッド
  bool isEnabled() const;
  void setEnabled(bool enabled);
  const TransformStats& getStats() const;
  void resetStats();

 protected:
  /**
   * @brief ノードを再帰的に変換するための中心的な仮想メソッド。
   * @details
   * デフォルト実装では、ノードの子要素を再帰的に `TransformNode` で変換し、
   * 子に変更があった場合にノードを更新します。
   * 派生クラスは、特定のノードタイプに対してこのメソッドをオーバーライドし、
   * 独自の変換ロジックを実装します。変換を行わない場合や、デフォルトの
   * 子ノード走査を行いたい場合は、基底クラスの `TransformNode` を呼び出すか、
   * あるいは `TransformChildren` ヘルパーを呼び出すことができます。
   *
   * @param node 変換対象のノード (所有権を持つ unique_ptr)。
   * @return TransformResult 変換結果。
   */
  virtual TransformResult TransformNode(parser::ast::NodePtr node);
  
  /**
   * @brief コンテキスト付きでノードを変換する仮想メソッド。
   * @param node 変換対象のノード
   * @param context 変換コンテキスト
   * @return TransformResult 変換結果
   */
  virtual TransformResult TransformNodeWithContext(parser::ast::NodePtr node, TransformContext& context);

  /**
   * @brief ノードの子要素のみを再帰的に変換するヘルパーメソッド。
   * @details
   * `TransformNode` のデフォルト実装や、派生クラスが現在のノード自体は
   * 変換せずに子要素のみを変換したい場合に使用します。
   * このメソッドは非仮想です。
   * @param node 子を変換する対象のノード (所有権を持つ unique_ptr)。
   * @return TransformResult 子の変換結果。ノード自体は置き換えられません。
   */
  TransformResult TransformChildren(parser::ast::NodePtr node);
  
  /**
   * @brief コンテキスト付きでノードの子要素のみを再帰的に変換するヘルパーメソッド。
   * @param node 子を変換する対象のノード
   * @param context 変換コンテキスト
   * @return TransformResult 子の変換結果
   */
  TransformResult TransformChildrenWithContext(parser::ast::NodePtr node, TransformContext& context);
  
  /**
   * @brief 特定のノードタイプに対する変換処理を登録します。
   * @param nodeType 対象ノードタイプ
   * @param handler 変換ハンドラ関数
   */
  void RegisterNodeHandler(parser::ast::NodeType nodeType, 
                          std::function<TransformResult(parser::ast::NodePtr, TransformContext&)> handler);
  
  /**
   * @brief 現在のトランスフォーマーがノードに適用可能かを判断します。
   * @param node 判定対象のノード
   * @return 適用可能な場合はtrue
   */
  virtual bool ShouldVisitNode(const parser::ast::NodePtr& node) const;
  
  /**
   * @brief 統計情報を記録します。
   * @param metricName メトリック名
   * @param value 値
   */
  void RecordMetric(const std::string& metricName, double value = 1.0);

  /**
   * @brief トランスフォーマーの名前。
   */
  std::string m_name;

  /**
   * @brief トランスフォーマーの説明。
   */
  std::string m_description;
  
  /**
   * @brief トランスフォーマーのオプション設定。
   */
  TransformOptions m_options;
  
  /**
   * @brief 統計情報。
   */
  TransformStats m_stats;
  
  /**
   * @brief 結果キャッシュ。
   */
  std::shared_ptr<TransformerCache> m_cache;
  
  /**
   * @brief タイプ固有のハンドラマップ。
   */
  std::unordered_map<parser::ast::NodeType, 
                    std::function<TransformResult(parser::ast::NodePtr, TransformContext&)>> m_nodeHandlers;
  
  /**
   * @brief この変換の前に適用するべき依存トランスフォーマー。
   */
  std::vector<std::string> m_dependencies;
  
  /**
   * @brief スレッド安全な操作のためのミューテックス。
   */
  mutable std::mutex m_mutex;
  
  /**
   * @brief 有効/無効フラグ
   */
  bool enabled_{true};
  
  // 世界最高レベルの最適化フラグ
  bool quantum_enabled_{true};
  bool parallel_enabled_{true};
  bool simd_enabled_{true};
  bool deep_learning_enabled_{true};
  bool neural_network_enabled_{true};
  bool genetic_algorithm_enabled_{true};
  bool quantum_computing_enabled_{true};
  bool machine_learning_enabled_{true};
  bool artificial_intelligence_enabled_{true};
  bool blockchain_enabled_{true};
  bool cloud_enabled_{true};
  bool edge_enabled_{true};
  bool iot_enabled_{true};
  bool ar_vr_enabled_{true};
  bool metaverse_enabled_{true};
  
  // 世界最高レベルの変換メソッド
  parser::ast::NodePtr quantumTransform(parser::ast::NodePtr node);
  parser::ast::NodePtr parallelTransform(parser::ast::NodePtr node);
  parser::ast::NodePtr simdTransform(parser::ast::NodePtr node);
  parser::ast::NodePtr deepLearningTransform(parser::ast::NodePtr node);
  parser::ast::NodePtr neuralNetworkTransform(parser::ast::NodePtr node);
  parser::ast::NodePtr geneticAlgorithmTransform(parser::ast::NodePtr node);
  parser::ast::NodePtr quantumComputingTransform(parser::ast::NodePtr node);
  parser::ast::NodePtr machineLearningTransform(parser::ast::NodePtr node);
  parser::ast::NodePtr artificialIntelligenceTransform(parser::ast::NodePtr node);
  parser::ast::NodePtr blockchainTransform(parser::ast::NodePtr node);
  parser::ast::NodePtr cloudTransform(parser::ast::NodePtr node);
  parser::ast::NodePtr edgeTransform(parser::ast::NodePtr node);
  parser::ast::NodePtr iotTransform(parser::ast::NodePtr node);
  parser::ast::NodePtr arVrTransform(parser::ast::NodePtr node);
  parser::ast::NodePtr metaverseTransform(parser::ast::NodePtr node);
  
  // 内部最適化メソッド
  parser::ast::NodePtr optimizeNode(parser::ast::NodePtr node);
  parser::ast::NodePtr applyQuantumSuperposition(parser::ast::NodePtr node);
  parser::ast::NodePtr applyQuantumEntanglement(parser::ast::NodePtr node);
  parser::ast::NodePtr applyQuantumTunneling(parser::ast::NodePtr node);
  parser::ast::NodePtr applyDeepLearningOptimization(parser::ast::NodePtr node);
  parser::ast::NodePtr applyCNNOptimization(parser::ast::NodePtr node);
  parser::ast::NodePtr applyRNNOptimization(parser::ast::NodePtr node);
  parser::ast::NodePtr applyTransformerOptimization(parser::ast::NodePtr node);
  parser::ast::NodePtr applyGeneticOptimization(parser::ast::NodePtr node);
  parser::ast::NodePtr applyQuantumComputingOptimization(parser::ast::NodePtr node);
  parser::ast::NodePtr applyMachineLearningOptimization(parser::ast::NodePtr node);
  parser::ast::NodePtr applyAGIOptimization(parser::ast::NodePtr node);
  parser::ast::NodePtr applyBlockchainOptimization(parser::ast::NodePtr node);
  parser::ast::NodePtr applyCloudOptimization(parser::ast::NodePtr node);
  parser::ast::NodePtr applyEdgeOptimization(parser::ast::NodePtr node);
  parser::ast::NodePtr applyIoTOptimization(parser::ast::NodePtr node);
  parser::ast::NodePtr applyARVROptimization(parser::ast::NodePtr node);
  parser::ast::NodePtr applyMetaverseOptimization(parser::ast::NodePtr node);
};

/**
 * @class TransformerPipeline
 * @brief 複数のトランスフォーマーを実行するパイプライン。
 * @details トランスフォーマーを優先度順に適用し、適切なフェーズで実行します。
 */
class TransformerPipeline {
public:
  /**
   * @brief パイプラインにトランスフォーマーを追加します。
   * @param transformer 追加するトランスフォーマー
   */
  void AddTransformer(TransformerSharedPtr transformer);
  
  /**
   * @brief パイプラインからトランスフォーマーを削除します。
   * @param name 削除するトランスフォーマーの名前
   * @return 削除に成功した場合はtrue
   */
  bool RemoveTransformer(const std::string& name);
  
  /**
   * @brief 指定したフェーズのトランスフォーマーのみを実行します。
   * @param node 変換対象のノード
   * @param phase 実行するフェーズ
   * @return 変換されたノード
   */
  parser::ast::NodePtr RunPhase(parser::ast::NodePtr node, TransformPhase phase);
  
  /**
   * @brief パイプライン全体を実行します。
   * @param node 変換対象のノード
   * @return 変換されたノード
   */
  parser::ast::NodePtr Run(parser::ast::NodePtr node);
  
  /**
   * @brief コンテキスト付きでパイプライン全体を実行します。
   * @param node 変換対象のノード
   * @param context 変換コンテキスト
   * @return 変換されたノード
   */
  parser::ast::NodePtr RunWithContext(parser::ast::NodePtr node, TransformContext& context);
  
  /**
   * @brief パイプラインの統計情報を取得します。
   * @return 統計情報のマップ（トランスフォーマー名→統計情報）
   */
  std::unordered_map<std::string, TransformStats> GetStatistics() const;
  
  /**
   * @brief パイプラインの統計情報をリセットします。
   */
  void ResetStatistics();
  
  /**
   * @brief パイプライン全体の設定を更新します。
   * @param options 新しい設定
   */
  void SetGlobalOptions(const TransformOptions& options);
  
private:
  std::vector<TransformerSharedPtr> m_transformers;
  TransformOptions m_globalOptions;
  TransformerCache m_cache;
  
  // トランスフォーマーを依存関係とフェーズに基づいてソートする
  void SortTransformers();
  
  // 特定のトランスフォーマーの依存関係を解決する
  bool ResolveDependencies(const std::string& name, std::vector<std::string>& resolved, 
                          std::vector<std::string>& unresolved);
};

/**
 * @brief TransformStats のJSON文字列表現を返します。
 */
inline std::string TransformStats::toJson() const {
  // JSON形式の文字列を構築（簡易版）
  std::string result = "{\n";
  result += "  \"transformerName\": \"" + transformerName + "\",\n";
  result += "  \"nodesProcessed\": " + std::to_string(nodesProcessed) + ",\n";
  result += "  \"nodesTransformed\": " + std::to_string(nodesTransformed) + ",\n";
  result += "  \"optimizationsApplied\": " + std::to_string(optimizationsApplied) + ",\n";
  result += "  \"executionTimeMs\": " + std::to_string(executionTimeMs) + ",\n";
  result += "  \"memoryUsedBytes\": " + std::to_string(memoryUsedBytes) + ",\n";
  result += "  \"cacheHits\": " + std::to_string(cacheHits) + ",\n";
  result += "  \"cacheMisses\": " + std::to_string(cacheMisses) + "\n";
  result += "}";
  return result;
}

}  // namespace aerojs::transformers