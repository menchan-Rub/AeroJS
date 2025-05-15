/**
 * @file transformer.cpp
 * @version 2.0.0
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 * @brief AST変換を行うトランスフォーマーの基本実装。
 */

#include "transformer.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <queue>
#include <set>
#include <stack>
#include <thread>
#include <unordered_set>
#include <xxhash.h>

namespace aerojs::transformers {

//===----------------------------------------------------------------------===//
// ヘルパー関数
//===----------------------------------------------------------------------===//

namespace {

// AST ノードのディープコピーを作成する
parser::ast::NodePtr CloneNode(const parser::ast::NodePtr& node) {
  if (!node) return nullptr;
  return node->clone();
}

// 文字列用に安全なエスケープ処理
std::string EscapeString(const std::string& input) {
  std::string result;
  result.reserve(input.size());
  
  for (char c : input) {
    switch (c) {
      case '"': result += "\\\""; break;
      case '\\': result += "\\\\"; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default: result += c; break;
    }
  }
  
  return result;
}

// 高速ハッシュ計算
size_t FastHash(const void* data, size_t len) {
  return XXH64(data, len, 0);
}

} // namespace

//===----------------------------------------------------------------------===//
// TransformerCache 実装
//===----------------------------------------------------------------------===//

size_t TransformerCache::ComputeHash(const parser::ast::NodePtr& node, const std::string& transformer) {
  // ノードのハッシュを計算
  if (!node) return 0;
  
  // ノードの内容に基づくハッシュを計算
  std::string nodeContent = node->toString();
  size_t nodeHash = FastHash(nodeContent.data(), nodeContent.size());
  
  // transformerの名前も含める
  size_t transformerHash = std::hash<std::string>{}(transformer);
  
  // 組み合わせたハッシュを返す
  return nodeHash ^ (transformerHash << 1);
}

void TransformerCache::Add(const parser::ast::NodePtr& node, const std::string& transformer, const TransformResult& result) {
  if (!node) return;
  
  std::unique_lock<std::shared_mutex> lock(m_mutex);
  
  CacheKey key{ComputeHash(node, transformer), transformer};
  CacheEntry entry{
    TransformResult(CloneNode(result.transformedNode), result.wasChanged, result.shouldStopTraversal),
    std::chrono::system_clock::now()
  };
  
  m_cache[key] = std::move(entry);
}

std::optional<TransformResult> TransformerCache::Get(const parser::ast::NodePtr& node, const std::string& transformer) {
  if (!node) return std::nullopt;
  
  CacheKey key{ComputeHash(node, transformer), transformer};
  
  std::shared_lock<std::shared_mutex> lock(m_mutex);
  
  auto it = m_cache.find(key);
  if (it != m_cache.end()) {
    m_hits++;
    
    // キャッシュから結果をコピーして返す
    // 注意: ノードは独立したコピーを作成する必要がある
    return TransformResult(
      CloneNode(it->second.result.transformedNode),
      it->second.result.wasChanged,
      it->second.result.shouldStopTraversal
    );
  }
  
  m_misses++;
  return std::nullopt;
}

void TransformerCache::Clear() {
  std::unique_lock<std::shared_mutex> lock(m_mutex);
  m_cache.clear();
  m_hits = 0;
  m_misses = 0;
}

double TransformerCache::GetHitRate() const {
  size_t hits = m_hits.load();
  size_t total = hits + m_misses.load();
  
  if (total == 0) return 0.0;
  return static_cast<double>(hits) / total;
}

//===----------------------------------------------------------------------===//
// Transformer 実装
//===----------------------------------------------------------------------===//

Transformer::Transformer(std::string name, std::string description)
    : m_name(std::move(name)), m_description(std::move(description)) {
  m_cache = std::make_shared<TransformerCache>();
  m_stats.transformerName = m_name;
}

Transformer::Transformer(std::string name, std::string description, TransformOptions options)
    : m_name(std::move(name)), m_description(std::move(description)), m_options(std::move(options)) {
  m_cache = std::make_shared<TransformerCache>();
  m_stats.transformerName = m_name;
}

TransformResult Transformer::Transform(parser::ast::NodePtr node) {
  // 基本的なコンテキストを作成して委譲
  TransformContext context;
  context.stats = m_options.collectStatistics ? &m_stats : nullptr;
  
  return TransformWithContext(std::move(node), context);
}

TransformResult Transformer::TransformWithContext(parser::ast::NodePtr node, TransformContext& context) {
  if (!node) {
    return TransformResult::Unchanged(nullptr);
  }
  
  auto start = std::chrono::high_resolution_clock::now();
  
  // キャッシュが有効な場合はキャッシュを確認
  if (m_options.enableCaching) {
    auto cachedResult = m_cache->Get(node, m_name);
    if (cachedResult) {
      if (context.stats) {
        context.stats->cachedTransforms++;
      }
      return *cachedResult;
    }
  }
  
  // ノードに適用可能かを確認
  if (!ShouldVisitNode(node)) {
    if (context.stats) {
      context.stats->skippedTransforms++;
    }
    return TransformResult::Unchanged(node);
  }
  
  // 統計情報の更新
  if (context.stats) {
    context.stats->nodesProcessed++;
  }
  
  // タイムアウト設定がある場合は監視
  bool shouldTimeOut = false;
  std::chrono::time_point<std::chrono::high_resolution_clock> deadline;
  
  if (m_options.timeout.count() > 0) {
    deadline = start + m_options.timeout;
  }
  
  // 実際の変換実行
  TransformResult result;
  
  try {
    // ノードハンドラがある場合は使用
    auto it = m_nodeHandlers.find(node->getType());
    if (it != m_nodeHandlers.end()) {
      result = it->second(std::move(node), context);
    } else {
      // デフォルトの変換を実行
      result = TransformNodeWithContext(std::move(node), context);
    }
    
    // タイムアウトチェック
    if (m_options.timeout.count() > 0) {
      shouldTimeOut = std::chrono::high_resolution_clock::now() >= deadline;
    }
  } catch (const std::exception& e) {
    // 例外発生時はノードをそのまま返す
    std::cerr << "Error in transformer " << m_name << ": " << e.what() << std::endl;
    return TransformResult::Failure(std::move(node));
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  
  // 統計情報の更新
  if (context.stats) {
    context.stats->totalTime += duration;
    if (result.wasChanged) {
      context.stats->nodesTransformed++;
    }
  }
  
  // キャッシュ更新
  if (m_options.enableCaching && !shouldTimeOut) {
    m_cache->Add(node, m_name, result);
  }
  
  return result;
}

std::string Transformer::GetName() const {
  return m_name;
}

std::string Transformer::GetDescription() const {
  return m_description;
}

TransformOptions Transformer::GetOptions() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_options;
}

void Transformer::SetOptions(const TransformOptions& options) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_options = options;
}

TransformStats Transformer::GetStatistics() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_stats;
}

bool Transformer::IsApplicableTo(const parser::ast::NodePtr& node) const {
  if (!node) return false;
  return ShouldVisitNode(node);
}

TransformPhase Transformer::GetPhase() const {
  return m_options.phase;
}

TransformPriority Transformer::GetPriority() const {
  return m_options.priority;
}

TransformResult Transformer::TransformNode(parser::ast::NodePtr node) {
  // デフォルトでは子ノードのみを変換
  return TransformChildren(std::move(node));
}

TransformResult Transformer::TransformNodeWithContext(parser::ast::NodePtr node, TransformContext& context) {
  // デフォルトでは非コンテキスト版にフォールバック
  return TransformNode(std::move(node));
}

TransformResult Transformer::TransformChildren(parser::ast::NodePtr node) {
  if (!node) {
    return TransformResult::Unchanged(nullptr);
  }
  
  bool hasChanges = false;
  
  // 各子ノードに対して変換を実行
  for (size_t i = 0; i < node->getChildCount(); ++i) {
    auto child = node->getChild(i);
    if (!child) continue;
    
    auto childResult = Transform(std::move(child));
    
    if (childResult.shouldStopTraversal) {
      return TransformResult::UnchangedAndStop(std::move(node));
    }
    
    if (childResult.wasChanged) {
      hasChanges = true;
      node->setChild(i, std::move(childResult.transformedNode));
    }
  }
  
  return TransformResult::ConditionalChange(std::move(node), hasChanges);
}

TransformResult Transformer::TransformChildrenWithContext(parser::ast::NodePtr node, TransformContext& context) {
  if (!node) {
    return TransformResult::Unchanged(nullptr);
  }
  
  bool hasChanges = false;
  
  // 各子ノードに対して変換を実行
  for (size_t i = 0; i < node->getChildCount(); ++i) {
    auto child = node->getChild(i);
    if (!child) continue;
    
    auto childResult = TransformWithContext(std::move(child), context);
    
    if (childResult.shouldStopTraversal) {
      return TransformResult::UnchangedAndStop(std::move(node));
    }
    
    if (childResult.wasChanged) {
      hasChanges = true;
      node->setChild(i, std::move(childResult.transformedNode));
    }
  }
  
  return TransformResult::ConditionalChange(std::move(node), hasChanges);
}

void Transformer::RegisterNodeHandler(parser::ast::NodeType nodeType, 
                                    std::function<TransformResult(parser::ast::NodePtr, TransformContext&)> handler) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_nodeHandlers[nodeType] = std::move(handler);
}

bool Transformer::ShouldVisitNode(const parser::ast::NodePtr& node) const {
  // デフォルトではすべてのノードを訪問
  return true;
}

void Transformer::RecordMetric(const std::string& metricName, double value) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_stats.incrementMetric(metricName, value);
}

//===----------------------------------------------------------------------===//
// TransformerPipeline 実装
//===----------------------------------------------------------------------===//

void TransformerPipeline::AddTransformer(TransformerSharedPtr transformer) {
  if (!transformer) return;
  
  m_transformers.push_back(std::move(transformer));
  SortTransformers();
}

bool TransformerPipeline::RemoveTransformer(const std::string& name) {
  auto it = std::find_if(m_transformers.begin(), m_transformers.end(),
                        [&name](const TransformerSharedPtr& t) { return t->GetName() == name; });
  
  if (it != m_transformers.end()) {
    m_transformers.erase(it);
    return true;
  }
  
  return false;
}

parser::ast::NodePtr TransformerPipeline::RunPhase(parser::ast::NodePtr node, TransformPhase phase) {
  if (!node) return nullptr;
  
  TransformContext context;
  context.currentPhase = phase;
  
  for (const auto& transformer : m_transformers) {
    if (transformer->GetPhase() == phase) {
      auto result = transformer->TransformWithContext(std::move(node), context);
      node = std::move(result.transformedNode);
      
      if (result.shouldStopTraversal) {
        break;
      }
    }
  }
  
  return node;
}

parser::ast::NodePtr TransformerPipeline::Run(parser::ast::NodePtr node) {
  if (!node) return nullptr;
  
  // 各フェーズを順番に実行
  TransformContext context;
  
  // 解析フェーズ
  context.currentPhase = TransformPhase::Analysis;
  node = RunWithContext(std::move(node), context);
  
  // 正規化フェーズ
  context.currentPhase = TransformPhase::Normalization;
  node = RunWithContext(std::move(node), context);
  
  // 最適化フェーズ
  context.currentPhase = TransformPhase::Optimization;
  node = RunWithContext(std::move(node), context);
  
  // 低レベル化フェーズ
  context.currentPhase = TransformPhase::Lowering;
  node = RunWithContext(std::move(node), context);
  
  // コード生成準備フェーズ
  context.currentPhase = TransformPhase::CodeGenPrep;
  node = RunWithContext(std::move(node), context);
  
  // 最終化フェーズ
  context.currentPhase = TransformPhase::Finalization;
  node = RunWithContext(std::move(node), context);
  
  return node;
}

parser::ast::NodePtr TransformerPipeline::RunWithContext(parser::ast::NodePtr node, TransformContext& context) {
  if (!node) return nullptr;
  
  // フェーズに合致するトランスフォーマーのみを実行
  for (const auto& transformer : m_transformers) {
    if (transformer->GetPhase() == context.currentPhase) {
      auto result = transformer->TransformWithContext(std::move(node), context);
      node = std::move(result.transformedNode);
      
      if (result.shouldStopTraversal) {
        break;
      }
    }
  }
  
  return node;
}

std::unordered_map<std::string, TransformStats> TransformerPipeline::GetStatistics() const {
  std::unordered_map<std::string, TransformStats> stats;
  
  for (const auto& transformer : m_transformers) {
    stats[transformer->GetName()] = transformer->GetStatistics();
  }
  
  return stats;
}

void TransformerPipeline::ResetStatistics() {
  for (const auto& transformer : m_transformers) {
    // トランスフォーマーの統計情報をリセット
    // （現在の実装では直接リセットのAPIがないため、無効な操作）
  }
}

void TransformerPipeline::SetGlobalOptions(const TransformOptions& options) {
  m_globalOptions = options;
  
  for (const auto& transformer : m_transformers) {
    // グローバルオプションの一部を各トランスフォーマーに適用
    TransformOptions current = transformer->GetOptions();
    
    // 特定のオプションのみをオーバーライド
    current.enableCaching = options.enableCaching;
    current.enableParallelization = options.enableParallelization;
    current.collectStatistics = options.collectStatistics;
    current.maxMemoryUsage = options.maxMemoryUsage;
    current.timeout = options.timeout;
    
    transformer->SetOptions(current);
  }
}

void TransformerPipeline::SortTransformers() {
  // 依存関係に基づいたソート
  std::vector<std::string> resolved;
  std::vector<std::string> unresolved;
  
  // トポロジカルソート
  for (const auto& transformer : m_transformers) {
    if (std::find(resolved.begin(), resolved.end(), transformer->GetName()) == resolved.end()) {
      ResolveDependencies(transformer->GetName(), resolved, unresolved);
    }
  }
  
  // ソートされた順番でトランスフォーマーを再整理
  std::vector<TransformerSharedPtr> sorted;
  sorted.reserve(m_transformers.size());
  
  for (const auto& name : resolved) {
    auto it = std::find_if(m_transformers.begin(), m_transformers.end(),
                          [&name](const TransformerSharedPtr& t) { return t->GetName() == name; });
    
    if (it != m_transformers.end()) {
      sorted.push_back(*it);
    }
  }
  
  // 優先度でさらにソート
  std::stable_sort(sorted.begin(), sorted.end(),
                  [](const TransformerSharedPtr& a, const TransformerSharedPtr& b) {
                    // フェーズでまず並べ替え
                    if (a->GetPhase() != b->GetPhase()) {
                      return static_cast<int>(a->GetPhase()) < static_cast<int>(b->GetPhase());
                    }
                    
                    // 同じフェーズ内では優先度で並べ替え
                    return static_cast<int>(a->GetPriority()) < static_cast<int>(b->GetPriority());
                  });
  
  m_transformers = std::move(sorted);
}

bool TransformerPipeline::ResolveDependencies(const std::string& name, std::vector<std::string>& resolved, 
                                             std::vector<std::string>& unresolved) {
  // 循環依存検出
  if (std::find(unresolved.begin(), unresolved.end(), name) != unresolved.end()) {
    // 循環依存が見つかった
    std::cerr << "Circular dependency detected involving transformer: " << name << std::endl;
    return false;
  }
  
  // すでに解決済みならスキップ
  if (std::find(resolved.begin(), resolved.end(), name) != resolved.end()) {
    return true;
  }
  
  // トランスフォーマーを見つける
  auto it = std::find_if(m_transformers.begin(), m_transformers.end(),
                        [&name](const TransformerSharedPtr& t) { return t->GetName() == name; });
  
  if (it == m_transformers.end()) {
    // 存在しないトランスフォーマー
    std::cerr << "Unknown transformer dependency: " << name << std::endl;
    return false;
  }
  
  unresolved.push_back(name);
  
  // このトランスフォーマーの依存関係を再帰的に解決
  const auto& transformer = *it;
  // 注: 現在の実装では依存関係を取得する方法がないため、コメントアウト
  /*
  for (const auto& dep : transformer->GetDependencies()) {
    if (!ResolveDependencies(dep, resolved, unresolved)) {
      return false;
    }
  }
  */
  
  // 解決済みとしてマーク
  resolved.push_back(name);
  
  // 未解決リストから削除
  auto unresolvedIt = std::find(unresolved.begin(), unresolved.end(), name);
  if (unresolvedIt != unresolved.end()) {
    unresolved.erase(unresolvedIt);
  }
  
  return true;
}

} // namespace aerojs::transformers

