#include <atomic>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

#include "symbol.h"

namespace aerojs {
namespace core {

// シンボルIDの生成用カウンタ（スレッドセーフ）
static std::atomic<int> s_nextSymbolId{1};  // 1から開始（0は無効値として予約）

// コンストラクタ
Symbol::Symbol(const std::string& description)
    : m_description(description),
      m_id(GenerateUniqueId()) {
  // 初期化リストで全て完了
}

// ムーブコンストラクタ
Symbol::Symbol(Symbol&& other) noexcept
    : m_description(std::move(other.m_description)),
      m_id(other.m_id) {
  // ムーブ元のIDを無効化
  other.m_id = -1;
}

// ムーブ代入演算子
Symbol& Symbol::operator=(Symbol&& other) noexcept {
  if (this != &other) {
    m_description = std::move(other.m_description);
    m_id = other.m_id;
    other.m_id = -1;
  }
  return *this;
}

// デストラクタ
Symbol::~Symbol() {
  // 特別な後処理は不要
  // グローバルレジストリはweak_ptrで管理されているため
  // 自動的にクリーンアップされる
}

// シンボル生成ファクトリメソッド
SymbolPtr Symbol::create(const std::string& description) {
  // MakeSharedEnablerを使用して非公開コンストラクタにアクセス
  return std::make_shared<Symbol>(description);
}

// 説明文字列の取得
const std::string& Symbol::description() const noexcept {
  return m_description;
}

// 説明文字列をstring_viewとして取得（コピーなし）
std::string_view Symbol::descriptionView() const noexcept {
  return std::string_view(m_description);
}

// ECMAScript仕様に準拠した文字列表現
std::string Symbol::toString() const {
  return "Symbol(" + (m_description.empty() ? "" : m_description) + ")";
}

// デバッグ用の詳細な文字列表現
std::string Symbol::debugString() const {
  return "Symbol@" + std::to_string(m_id) + "(" +
         (m_description.empty() ? "" : "\"" + m_description + "\"") +
         ")";
}

// グローバルシンボルレジストリからシンボルを取得または作成
SymbolPtr Symbol::For(const std::string& key) {
  // スレッドセーフなレジストリ管理
  static std::unordered_map<std::string, SymbolWeakPtr> registry;
  static std::unordered_map<int, SymbolWeakPtr> idRegistry;
  static std::unordered_map<Symbol*, std::string> reverseRegistry;
  static std::mutex registryMutex;

  std::lock_guard<std::mutex> lock(registryMutex);

  // 既存のシンボルを検索
  auto it = registry.find(key);
  if (it != registry.end()) {
    SymbolPtr existingSymbol = it->second.lock();
    if (existingSymbol) {
      return existingSymbol;
    } else {
      // 実体が破棄されている場合はエントリを削除
      registry.erase(it);
    }
  }

  // 新しいシンボルを作成して登録
  SymbolPtr newSymbol = std::make_shared<Symbol>(key);

  registry[key] = newSymbol;
  idRegistry[newSymbol->id()] = newSymbol;
  reverseRegistry[newSymbol.get()] = key;

  return newSymbol;
}

// シンボルからグローバルレジストリのキーを逆引き
std::optional<std::string> Symbol::KeyFor(const SymbolPtr& symbol) {
  if (!symbol) {
    return std::nullopt;
  }

  static std::unordered_map<std::string, SymbolWeakPtr> registry;
  static std::unordered_map<int, SymbolWeakPtr> idRegistry;
  static std::unordered_map<Symbol*, std::string> reverseRegistry;
  static std::mutex registryMutex;

  std::lock_guard<std::mutex> lock(registryMutex);

  // ポインタベースで逆引き
  auto it = reverseRegistry.find(symbol.get());
  if (it != reverseRegistry.end()) {
    auto regIt = registry.find(it->second);
    if (regIt != registry.end()) {
      SymbolPtr existing = regIt->second.lock();
      if (existing && existing.get() == symbol.get()) {
        return it->second;
      }
    }
    // 不整合があれば逆引きマップからも削除
    reverseRegistry.erase(it);
  }

  return std::nullopt;
}

// IDからシンボルを取得
std::optional<SymbolPtr> Symbol::getById(int id) {
  if (id < 0) {
    return std::nullopt;
  }
  
  static std::unordered_map<std::string, SymbolWeakPtr> registry;
  static std::unordered_map<int, SymbolWeakPtr> idRegistry;
  static std::unordered_map<Symbol*, std::string> reverseRegistry;
  static std::mutex registryMutex;

  std::lock_guard<std::mutex> lock(registryMutex);

  auto it = idRegistry.find(id);
  if (it != idRegistry.end()) {
    SymbolPtr symbol = it->second.lock();
    if (symbol) {
      return symbol;
    } else {
      // 実体が破棄されているのでレジストリから削除
      idRegistry.erase(it);
      
      // 他のレジストリからも対応するエントリを削除
      for (auto revIt = reverseRegistry.begin(); revIt != reverseRegistry.end(); ) {
        if (revIt->second.empty() || registry.find(revIt->second) == registry.end() || 
            registry[revIt->second].expired()) {
          revIt = reverseRegistry.erase(revIt);
        } else {
          ++revIt;
        }
      }
    }
  }
  return std::nullopt;
}

// レジストリに登録されているアクティブなシンボルの数を取得
size_t Symbol::registrySize() {
  static std::unordered_map<std::string, SymbolWeakPtr> registry;
  static std::mutex registryMutex;
  std::lock_guard<std::mutex> lock(registryMutex);
  
  size_t count = 0;
  for (auto it = registry.begin(); it != registry.end(); ) {
    if (it->second.expired()) {
      it = registry.erase(it);
    } else {
      ++count;
      ++it;
    }
  }
  return count;
}

// レジストリの内容をデバッグ用に文字列化
std::string Symbol::dumpRegistry() {
  static std::unordered_map<std::string, SymbolWeakPtr> registry;
  static std::mutex registryMutex;
  std::lock_guard<std::mutex> lock(registryMutex);

  std::ostringstream oss;
  oss << "シンボルレジストリ（アクティブエントリ数: " << registrySize() << "）:\n";

  for (const auto& pair : registry) {
    SymbolPtr symbol = pair.second.lock();
    if (symbol) {
      oss << "  '" << pair.first << "' -> " << symbol->debugString() << "\n";
    } else {
      oss << "  '" << pair.first << "' -> (期限切れ)\n";
    }
  }
  return oss.str();
}

// Well Known Symbolの実装
#define INIT_WK_SYMBOL(name)                              \
  static SymbolPtr symbol = Symbol::For("Symbol." #name); \
  return symbol

SymbolPtr Symbol::hasInstance() {
  INIT_WK_SYMBOL(hasInstance);
}

SymbolPtr Symbol::isConcatSpreadable() {
  INIT_WK_SYMBOL(isConcatSpreadable);
}

SymbolPtr Symbol::iterator() {
  INIT_WK_SYMBOL(iterator);
}

SymbolPtr Symbol::asyncIterator() {
  INIT_WK_SYMBOL(asyncIterator);
}

SymbolPtr Symbol::match() {
  INIT_WK_SYMBOL(match);
}

SymbolPtr Symbol::matchAll() {
  INIT_WK_SYMBOL(matchAll);
}

SymbolPtr Symbol::replace() {
  INIT_WK_SYMBOL(replace);
}

SymbolPtr Symbol::search() {
  INIT_WK_SYMBOL(search);
}

SymbolPtr Symbol::species() {
  INIT_WK_SYMBOL(species);
}

SymbolPtr Symbol::split() {
  INIT_WK_SYMBOL(split);
}

SymbolPtr Symbol::toPrimitive() {
  INIT_WK_SYMBOL(toPrimitive);
}

SymbolPtr Symbol::toStringTag() {
  INIT_WK_SYMBOL(toStringTag);
}

SymbolPtr Symbol::unscopables() {
  INIT_WK_SYMBOL(unscopables);
}

// 一意なシンボルIDを生成
int Symbol::GenerateUniqueId() {
  return s_nextSymbolId.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace core
}  // namespace aerojs