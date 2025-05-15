/**
 * @file incremental_parser.cpp
 * @brief 高性能インクリメンタルJavaScriptパーサーの実装
 *
 * @copyright Copyright (c) 2024 AeroJS プロジェクト
 * @license MIT License
 */

#include "incremental_parser.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <stack>
#include <queue>
#include <nlohmann/json.hpp>

#include "../../utils/logger.h"
#include "lexer/scanner/scanner.h"
#include "parser_error.h"
#include "ast/node_visitor.h"

namespace aero {
namespace parser {

// JSONライブラリを使用
using json = nlohmann::json;

// ロガーの初期化
static Logger& logger = Logger::getInstance("IncrementalParser");

/**
 * @brief ASTノード訪問者クラス
 */
class NodeLocationVisitor : public ast::NodeVisitor {
public:
  /**
   * @brief コンストラクタ
   * @param start 編集開始位置
   * @param end 編集終了位置
   * @param delta 変更量
   */
  NodeLocationVisitor(size_t start, size_t end, int delta)
      : m_start(start), m_end(end), m_delta(delta), m_affectedNodes() {}

  /**
   * @brief 影響を受けるノードのリストを取得
   * @return std::vector<ast::Node*> 影響を受けるノードのリスト
   */
  const std::vector<ast::Node*>& getAffectedNodes() const {
    return m_affectedNodes;
  }

  /**
   * @brief ノードの訪問
   * @param node 訪問するノード
   * @return true 訪問を続ける場合
   */
  bool visit(ast::Node* node) override {
    if (!node) return true;

    const auto& loc = node->getLocation();
    
    // 編集位置がノードの範囲内またはノードが編集範囲をまたいでいる場合
    if ((loc.start.offset <= m_end && loc.end.offset >= m_start) ||
        (loc.start.offset >= m_start && loc.start.offset <= m_end) ||
        (loc.end.offset >= m_start && loc.end.offset <= m_end)) {
      m_affectedNodes.push_back(node);
    }
    
    // 編集位置より後のノードの位置情報を更新
    if (loc.start.offset > m_end) {
      // 編集位置より後のノードの位置を更新
      auto newStartOffset = static_cast<size_t>(static_cast<int>(loc.start.offset) + m_delta);
      auto newEndOffset = static_cast<size_t>(static_cast<int>(loc.end.offset) + m_delta);
      
      node->setLocation({
          {loc.start.line, loc.start.column, newStartOffset},
          {loc.end.line, loc.end.column, newEndOffset}
      });
    }
    
    return true;
  }

private:
  size_t m_start;                      ///< 編集開始位置
  size_t m_end;                        ///< 編集終了位置
  int m_delta;                         ///< 変更量
  std::vector<ast::Node*> m_affectedNodes; ///< 影響を受けるノード
};

IncrementalParser::IncrementalParser(const IncrementalParserOptions& options)
    : m_options(options), m_currentSource(), m_currentFilename(), m_currentAst(nullptr),
      m_currentTokens(), m_currentErrors(), m_cache(), m_stats() {
  m_logger = [](const std::string& msg) {
    logger.debug(msg);
  };
  
  m_logger("インクリメンタルパーサーが初期化されました");
}

IncrementalParser::~IncrementalParser() {
  clearCache();
  m_logger("インクリメンタルパーサーが破棄されました");
}

ParseResult IncrementalParser::parse(const std::string& source, const std::string& filename) {
  auto startTime = std::chrono::high_resolution_clock::now();
  
  // キャッシュが有効で、キャッシュにエントリがある場合はキャッシュから取得
  if (m_options.enableCaching) {
    auto cachedResult = getFromCache(source, filename);
    if (cachedResult) {
      m_stats.cacheHits++;
      m_currentSource = source;
      m_currentFilename = filename;
      m_currentAst = std::move(cachedResult->ast);
      m_currentTokens = cachedResult->tokens;
      m_currentErrors = cachedResult->errors;
      
      m_logger("キャッシュからパース結果を取得しました: " + filename);
      
      return *cachedResult;
    }
    m_stats.cacheMisses++;
  }
  
  // 新規にパース
  m_stats.totalParses++;
  m_stats.fullReparses++;
  
  // スキャナーを作成
  lexer::Scanner scanner;
  scanner.init(source, filename);
  
  // パーサーを作成
  Parser parser;
  ParserOptions parserOptions;
  parserOptions.strict_mode = false;
  parserOptions.module_mode = true;
  parserOptions.jsx_enabled = true;
  parserOptions.tolerant_mode = m_options.tolerantMode;
  parserOptions.collect_comments = m_options.collectComments;
  parser.setOptions(parserOptions);
  
  // パースを実行
  ParseResult result;
  result.parseTime = std::chrono::microseconds(0);
  
  try {
    auto parseStart = std::chrono::high_resolution_clock::now();
    
    // タイムアウト機能（将来的に実装）
    // 現在のスレッドでパースを実行
    result.ast = parser.parse(source, filename);
    result.tokens = scanner.getTokens();
    result.errors = parser.getErrors();
    
    auto parseEnd = std::chrono::high_resolution_clock::now();
    result.parseTime = std::chrono::duration_cast<std::chrono::microseconds>(parseEnd - parseStart);
  } catch (const std::exception& e) {
    m_logger("パース中に例外が発生しました: " + std::string(e.what()));
    result.errors.push_back({
      SourceLocation{{0, 0, 0}, {0, 0, 0}},
      "パースエラー: " + std::string(e.what()),
      parser_error::ErrorSeverity::Error
    });
  }
  
  // 現在の状態を更新
  m_currentSource = source;
  m_currentFilename = filename;
  m_currentAst = std::unique_ptr<ast::Node>(result.ast.get() ? result.ast->clone() : nullptr);
  m_currentTokens = result.tokens;
  m_currentErrors = result.errors;
  
  // キャッシュに追加
  if (m_options.enableCaching) {
    addToCache(source, filename, result);
  }
  
  // 統計情報を更新
  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  m_stats.totalTime += totalTime;
  
  m_logger("新規ファイルのパースが完了しました: " + filename + 
           " (" + std::to_string(result.parseTime.count()) + "µs)");
  
  return result;
}

ParseResult IncrementalParser::parseIncremental(const SourceEdit& edit, const std::string& filename) {
  auto startTime = std::chrono::high_resolution_clock::now();
  
  // 初回パースまたはファイルが異なる場合は完全再解析
  if (m_currentAst == nullptr || m_currentSource.empty() || m_currentFilename != filename) {
    // 編集を適用した新しいソースを作成
    std::string newSource;
    if (!m_currentSource.empty()) {
      newSource = m_currentSource;
    } else {
      newSource = std::string(edit.start, ' ');
    }
    
    // 編集範囲を更新
    if (edit.start <= newSource.size()) {
      size_t replaceEnd = std::min(edit.end, newSource.size());
      newSource.replace(edit.start, replaceEnd - edit.start, edit.newText);
    } else {
      // 範囲外の場合は末尾に追加
      newSource.append(std::string(edit.start - newSource.size(), ' '));
      newSource.append(edit.newText);
    }
    
    m_stats.totalEdits++;
    m_stats.largestEdit = std::max(m_stats.largestEdit, edit.newText.size());
    
    return parse(newSource, filename);
  }
  
  // 編集を適用
  m_stats.totalEdits++;
  m_stats.incrementalParses++;
  m_stats.largestEdit = std::max(m_stats.largestEdit, edit.newText.size());
  
  // 変更によって影響を受けるノードを特定
  auto affectedNodes = findAffectedNodes(edit);
  
  // 編集を適用した新しいソースを作成
  std::string newSource = m_currentSource;
  if (edit.start <= newSource.size()) {
    size_t replaceEnd = std::min(edit.end, newSource.size());
    newSource.replace(edit.start, replaceEnd - edit.start, edit.newText);
  } else {
    // 範囲外の場合は末尾に追加
    newSource.append(std::string(edit.start - newSource.size(), ' '));
    newSource.append(edit.newText);
  }
  
  // 変更が大きい場合または影響範囲が広すぎる場合は完全再解析
  if (edit.newText.size() > 1000 || affectedNodes.size() > 20) {
    m_logger("変更が大きいため、完全再解析を実行します");
    m_currentSource = newSource;
    return fullReparse();
  }
  
  ParseResult result;
  result.isPartial = true;
  
  // 部分的な再解析を試みる
  bool reparseSuccess = reparseNodes(affectedNodes, edit);
  
  // 部分的な再解析に失敗した場合は完全再解析
  if (!reparseSuccess) {
    m_logger("部分的な再解析に失敗したため、完全再解析を実行します");
    m_currentSource = newSource;
    return fullReparse();
  }
  
  // 部分的な再解析に成功した場合
  m_currentSource = newSource;
  
  result.ast = std::unique_ptr<ast::Node>(m_currentAst->clone());
  result.tokens = m_currentTokens;
  result.errors = m_currentErrors;
  
  // 統計情報を更新
  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  result.parseTime = totalTime;
  m_stats.totalTime += totalTime;
  
  m_logger("インクリメンタルパースが完了しました (" + std::to_string(totalTime.count()) + "µs)");
  
  return result;
}

ParseResult IncrementalParser::parseIncrementalBatch(const std::vector<SourceEdit>& edits, const std::string& filename) {
  if (edits.empty()) {
    // 編集がない場合は現在の状態を返す
    ParseResult result;
    result.ast = std::unique_ptr<ast::Node>(m_currentAst->clone());
    result.tokens = m_currentTokens;
    result.errors = m_currentErrors;
    result.parseTime = std::chrono::microseconds(0);
    return result;
  }
  
  if (edits.size() == 1) {
    // 編集が1つの場合は単一編集として処理
    return parseIncremental(edits[0], filename);
  }
  
  // バッチ内の最初の編集のみを考慮した新しいソースを作成
  std::string newSource = m_currentSource;
  size_t totalDelta = 0;
  
  for (const auto& edit : edits) {
    // 前の編集による位置の変化を考慮
    size_t adjustedStart = edit.start + totalDelta;
    size_t adjustedEnd = edit.end + totalDelta;
    
    // 範囲チェック
    if (adjustedStart > newSource.size()) {
      adjustedStart = newSource.size();
    }
    if (adjustedEnd > newSource.size()) {
      adjustedEnd = newSource.size();
    }
    
    // 編集を適用
    newSource.replace(adjustedStart, adjustedEnd - adjustedStart, edit.newText);
    
    // 総変化量を更新
    totalDelta += edit.newText.size() - (adjustedEnd - adjustedStart);
  }
  
  // 複数編集の場合は完全再解析を実行
  m_currentSource = newSource;
  m_currentFilename = filename;
  
  m_logger("複数の編集が適用されたため、完全再解析を実行します");
  return fullReparse();
}

const std::string& IncrementalParser::getCurrentSource() const {
  return m_currentSource;
}

const ast::Node* IncrementalParser::getCurrentAst() const {
  return m_currentAst.get();
}

void IncrementalParser::reset() {
  m_currentSource.clear();
  m_currentFilename.clear();
  m_currentAst.reset();
  m_currentTokens.clear();
  m_currentErrors.clear();
  
  m_logger("パーサー状態がリセットされました");
}

void IncrementalParser::clearCache() {
  m_cache.clear();
  m_logger("キャッシュがクリアされました");
}

void IncrementalParser::setOptions(const IncrementalParserOptions& options) {
  m_options = options;
}

const IncrementalParserOptions& IncrementalParser::getOptions() const {
  return m_options;
}

std::string IncrementalParser::getStats() const {
  json statsJson;
  statsJson["total_parses"] = m_stats.totalParses;
  statsJson["incremental_parses"] = m_stats.incrementalParses;
  statsJson["full_reparses"] = m_stats.fullReparses;
  statsJson["cache_hits"] = m_stats.cacheHits;
  statsJson["cache_misses"] = m_stats.cacheMisses;
  statsJson["total_time_us"] = m_stats.totalTime.count();
  statsJson["total_edits"] = m_stats.totalEdits;
  statsJson["largest_edit"] = m_stats.largestEdit;
  statsJson["cache_size"] = m_cache.size();
  statsJson["current_file"] = m_currentFilename;
  statsJson["has_ast"] = (m_currentAst != nullptr);
  statsJson["token_count"] = m_currentTokens.size();
  statsJson["error_count"] = m_currentErrors.size();
  
  return statsJson.dump(2);
}

std::string IncrementalParser::getNodeSource(const ast::Node* node) const {
  if (!node || m_currentSource.empty()) {
    return "";
  }
  
  const auto& loc = node->getLocation();
  if (loc.start.offset >= m_currentSource.size() || loc.end.offset > m_currentSource.size() || 
      loc.start.offset > loc.end.offset) {
    return "";
  }
  
  return m_currentSource.substr(loc.start.offset, loc.end.offset - loc.start.offset);
}

const ast::Node* IncrementalParser::getNodeAtOffset(size_t offset) const {
  if (!m_currentAst) {
    return nullptr;
  }
  
  std::stack<const ast::Node*> stack;
  stack.push(m_currentAst.get());
  
  const ast::Node* bestMatch = nullptr;
  
  while (!stack.empty()) {
    const ast::Node* current = stack.top();
    stack.pop();
    
    if (!current) {
      continue;
    }
    
    const auto& loc = current->getLocation();
    if (loc.start.offset <= offset && offset < loc.end.offset) {
      // 範囲内のノードが見つかった
      if (!bestMatch || (current->getLocation().end.offset - current->getLocation().start.offset < 
                         bestMatch->getLocation().end.offset - bestMatch->getLocation().start.offset)) {
        bestMatch = current;
      }
      
      // 子ノードも確認
      for (size_t i = 0; i < current->getChildCount(); ++i) {
        stack.push(current->getChild(i));
      }
    }
  }
  
  return bestMatch;
}

std::vector<const ast::Node*> IncrementalParser::getNodesInRange(size_t start, size_t end) const {
  std::vector<const ast::Node*> result;
  
  if (!m_currentAst || start >= end) {
    return result;
  }
  
  std::stack<const ast::Node*> stack;
  stack.push(m_currentAst.get());
  
  while (!stack.empty()) {
    const ast::Node* current = stack.top();
    stack.pop();
    
    if (!current) {
      continue;
    }
    
    const auto& loc = current->getLocation();
    if ((loc.start.offset <= end && loc.end.offset >= start)) {
      // ノードが範囲と重なっている
      result.push_back(current);
    }
    
    // 子ノードも確認
    for (size_t i = 0; i < current->getChildCount(); ++i) {
      stack.push(current->getChild(i));
    }
  }
  
  return result;
}

bool IncrementalParser::prepareIncremental(const SourceEdit& edit) {
  if (!m_currentAst || m_currentSource.empty()) {
    return false;
  }
  
  // 基本的な範囲チェック
  if (edit.start > m_currentSource.size() || edit.end > m_currentSource.size() || edit.start > edit.end) {
    m_logger("無効な編集範囲: start=" + std::to_string(edit.start) + 
             ", end=" + std::to_string(edit.end) + 
             ", sourceSize=" + std::to_string(m_currentSource.size()));
    return false;
  }
  
  return true;
}

std::vector<ast::Node*> IncrementalParser::findAffectedNodes(const SourceEdit& edit) {
  NodeLocationVisitor visitor(edit.start, edit.end, edit.getDelta());
  
  // ASTツリーを走査して影響を受けるノードを特定
  m_currentAst->accept(&visitor);
  
  return visitor.getAffectedNodes();
}

void IncrementalParser::updateNodeLocations(ast::Node* node, int delta, size_t editEnd) {
  std::queue<ast::Node*> queue;
  queue.push(node);
  
  while (!queue.empty()) {
    ast::Node* current = queue.front();
    queue.pop();
    
    if (!current) {
      continue;
    }
    
    const auto& loc = current->getLocation();
    
    // 編集位置より後のノードの位置情報を更新
    if (loc.start.offset > editEnd) {
      // 編集位置より後のノードの位置を更新
      auto newStartOffset = static_cast<size_t>(static_cast<int>(loc.start.offset) + delta);
      auto newEndOffset = static_cast<size_t>(static_cast<int>(loc.end.offset) + delta);
      
      current->setLocation({
          {loc.start.line, loc.start.column, newStartOffset},
          {loc.end.line, loc.end.column, newEndOffset}
      });
    }
    
    // 子ノードもキューに追加
    for (size_t i = 0; i < current->getChildCount(); ++i) {
      queue.push(current->getChild(i));
    }
  }
}

bool IncrementalParser::reparseNodes(const std::vector<ast::Node*>& affectedNodes, const SourceEdit& edit) {
  if (affectedNodes.empty()) {
    // 影響を受けるノードがない場合は位置情報のみ更新
    int delta = edit.getDelta();
    if (delta != 0) {
      updateNodeLocations(m_currentAst.get(), delta, edit.end);
    }
    return true;
  }
  
  // 現在は単純化のため、部分的な再解析はサポートしない
  // 将来的には、影響を受けるノードの親を特定し、その部分のみを再解析する
  return false;
}

ParseResult IncrementalParser::fullReparse() {
  return parse(m_currentSource, m_currentFilename);
}

std::optional<ParseResult> IncrementalParser::getFromCache(const std::string& source, const std::string& filename) {
  std::string key = filename + ":" + std::to_string(std::hash<std::string>{}(source));
  
  auto it = m_cache.find(key);
  if (it != m_cache.end()) {
    // キャッシュからコピーを作成して返す
    ParseResult result;
    result.ast = std::unique_ptr<ast::Node>(it->second.result.ast->clone());
    result.tokens = it->second.result.tokens;
    result.errors = it->second.result.errors;
    result.parseTime = it->second.result.parseTime;
    
    return result;
  }
  
  return std::nullopt;
}

void IncrementalParser::addToCache(const std::string& source, const std::string& filename, const ParseResult& result) {
  // キャッシュサイズが制限を超えている場合は古いエントリを削除
  if (m_cache.size() >= m_options.maxCacheSize) {
    // 最も古いエントリを見つける
    auto oldestIt = m_cache.begin();
    auto oldestTime = oldestIt->second.timestamp;
    
    for (auto it = std::next(m_cache.begin()); it != m_cache.end(); ++it) {
      if (it->second.timestamp < oldestTime) {
        oldestIt = it;
        oldestTime = it->second.timestamp;
      }
    }
    
    // 最も古いエントリを削除
    m_cache.erase(oldestIt);
  }
  
  // キャッシュに追加
  std::string key = filename + ":" + std::to_string(std::hash<std::string>{}(source));
  
  CacheEntry entry;
  entry.result.ast = std::unique_ptr<ast::Node>(result.ast->clone());
  entry.result.tokens = result.tokens;
  entry.result.errors = result.errors;
  entry.result.parseTime = result.parseTime;
  entry.timestamp = std::chrono::system_clock::now();
  
  m_cache[key] = std::move(entry);
}

} // namespace parser
} // namespace aero
 