#include "symbol.h"

#include <atomic>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

namespace aerojs {
namespace core {

// シンボルIDの生成用カウンタ（スレッドセーフ）
static std::atomic<int> s_nextSymbolId{1};  // 1から開始（0は無効値として予約）

// コンストラクタ、デストラクタの実装

Symbol::Symbol(const std::string& description)
    : m_description(description),
      m_id(GenerateUniqueId()) {
}

Symbol::Symbol(Symbol&& other) noexcept
    : m_description(std::move(other.m_description)),
      m_id(other.m_id) {
  other.m_id = -1;  // 移動元を無効化
}

Symbol& Symbol::operator=(Symbol&& other) noexcept {
  if (this != &other) {
    m_description = std::move(other.m_description);
    m_id = other.m_id;
    other.m_id = -1;  // 移動元を無効化
  }
  return *this;
}

Symbol::~Symbol() = default;

// ファクトリメソッド

// static
SymbolPtr Symbol::create(const std::string& description) {
  return std::make_shared<Symbol>(description);
}

// public メソッドの実装

const std::string& Symbol::description() const noexcept {
  return m_description;
}

std::string_view Symbol::descriptionView() const noexcept {
  return std::string_view(m_description);
}

std::string Symbol::toString() const {
  // ECMAScript仕様に準拠した文字列表現
  return "Symbol(" + (m_description.empty() ? "" : m_description) + ")";
}

std::string Symbol::debugString() const {
  // デバッグ用の詳細表現
  return "Symbol@" + std::to_string(m_id) + "(" +
         (m_description.empty() ? "" : "\"" + m_description + "\"") +
         ")";
}

// グローバルレジストリ関連

namespace {
// シンボルレジストリとその保護用ミューテックス
std::unordered_map<std::string, SymbolWeakPtr> g_symbolRegistry;
std::unordered_map<int, SymbolWeakPtr> g_symbolIdRegistry;
std::unordered_map<Symbol*, std::string> g_symbolReverseRegistry;
std::mutex g_symbolRegistryMutex;
}  // namespace

// static
SymbolPtr Symbol::For(const std::string& key) {
  std::lock_guard<std::mutex> lock(g_symbolRegistryMutex);

  auto it = g_symbolRegistry.find(key);
  if (it != g_symbolRegistry.end()) {
    SymbolPtr existingSymbol = it->second.lock();
    if (existingSymbol) {
      return existingSymbol;
    } else {
      // 期限切れのエントリを削除
      auto revIt = g_symbolReverseRegistry.begin();
      int oldId = -1;
      
      // 対応するIDを検索して削除
      while (revIt != g_symbolReverseRegistry.end()) {
        if (revIt->second == key) {
          oldId = revIt->first->id();
          revIt = g_symbolReverseRegistry.erase(revIt);
        } else {
          ++revIt;
        }
      }
      
      if (oldId != -1) {
        g_symbolIdRegistry.erase(oldId);
      }
      g_symbolRegistry.erase(it);
    }
  }

  // 新しいシンボルを作成して登録
  SymbolPtr newSymbol = std::make_shared<Symbol>(key);

  g_symbolRegistry[key] = newSymbol;
  g_symbolIdRegistry[newSymbol->id()] = newSymbol;
  g_symbolReverseRegistry[newSymbol.get()] = key;

  return newSymbol;
}

// static
std::optional<std::string> Symbol::KeyFor(const SymbolPtr& symbol) {
  if (!symbol) {
    return std::nullopt;
  }

  std::lock_guard<std::mutex> lock(g_symbolRegistryMutex);

  auto it = g_symbolReverseRegistry.find(symbol.get());
  if (it != g_symbolReverseRegistry.end()) {
    // キーに対応するシンボルが有効か確認
    auto regIt = g_symbolRegistry.find(it->second);
    if (regIt != g_symbolRegistry.end()) {
      SymbolPtr existing = regIt->second.lock();
      if (existing && existing.get() == symbol.get()) {
        return it->second;
      }
    }
    // 不整合があれば逆引きマップから削除
    g_symbolReverseRegistry.erase(it);
  }

  return std::nullopt;
}

// static
std::optional<SymbolPtr> Symbol::getById(int id) {
  if (id < 0) {
    return std::nullopt;
  }
  std::lock_guard<std::mutex> lock(g_symbolRegistryMutex);

  auto it = g_symbolIdRegistry.find(id);
  if (it != g_symbolIdRegistry.end()) {
    SymbolPtr symbol = it->second.lock();
    if (symbol) {
      return symbol;
    } else {
      // 期限切れのエントリを完全に削除
      g_symbolIdRegistry.erase(it);
      
      // 対応する他のレジストリからも削除
      for (auto revIt = g_symbolReverseRegistry.begin(); revIt != g_symbolReverseRegistry.end();) {
        if (revIt->first->id() == id) {
          std::string key = revIt->second;
          revIt = g_symbolReverseRegistry.erase(revIt);
          g_symbolRegistry.erase(key);
        } else {
          ++revIt;
        }
      }
    }
  }
  return std::nullopt;
}

// static
size_t Symbol::registrySize() {
  std::lock_guard<std::mutex> lock(g_symbolRegistryMutex);
  size_t count = 0;
  
  // 期限切れのエントリを削除しながら有効なエントリをカウント
  for (auto it = g_symbolRegistry.begin(); it != g_symbolRegistry.end();) {
    if (it->second.expired()) {
      // 期限切れのエントリに関連する全てのレジストリをクリーンアップ
      std::string key = it->first;
      it = g_symbolRegistry.erase(it);
      
      // 対応するIDと逆引きエントリを検索して削除
      for (auto idIt = g_symbolIdRegistry.begin(); idIt != g_symbolIdRegistry.end();) {
        if (idIt->second.expired()) {
          int expiredId = idIt->first;
          idIt = g_symbolIdRegistry.erase(idIt);
          
          // 逆引きマップからも削除
          for (auto revIt = g_symbolReverseRegistry.begin(); revIt != g_symbolReverseRegistry.end();) {
            if (revIt->first->id() == expiredId || revIt->second == key) {
              revIt = g_symbolReverseRegistry.erase(revIt);
            } else {
              ++revIt;
            }
          }
        } else {
          ++idIt;
        }
      }
    } else {
      ++count;
      ++it;
    }
  }
  return count;
}

// static
std::string Symbol::dumpRegistry() {
  std::lock_guard<std::mutex> lock(g_symbolRegistryMutex);
  std::ostringstream oss;
  size_t activeCount = 0;
  
  // 有効なエントリをカウント
  for (const auto& pair : g_symbolRegistry) {
    if (!pair.second.expired()) {
      activeCount++;
    }
  }
  
  oss << "シンボルレジストリ (" << activeCount << " / " << g_symbolRegistry.size() << " エントリ有効):\n";

  for (const auto& pair : g_symbolRegistry) {
    SymbolPtr symbol = pair.second.lock();
    if (symbol) {
      oss << "  '" << pair.first << "' -> " << symbol->debugString() << "\n";
    } else {
      oss << "  '" << pair.first << "' -> (期限切れ)\n";
    }
  }
  return oss.str();
}

// static
void Symbol::resetRegistryForTesting() {
  std::lock_guard<std::mutex> lock(g_symbolRegistryMutex);
  g_symbolRegistry.clear();
  g_symbolIdRegistry.clear();
  g_symbolReverseRegistry.clear();
  s_nextSymbolId.store(1, std::memory_order_relaxed);
}

// Well Known Symbolの実装

// シンボル定義マクロ
#define DEFINE_WK_SYMBOL(name)                              \
  SymbolPtr Symbol::name() {                                \
    static SymbolPtr symbol = Symbol::For("Symbol." #name); \
    return symbol;                                          \
  }

DEFINE_WK_SYMBOL(hasInstance)
DEFINE_WK_SYMBOL(isConcatSpreadable)
DEFINE_WK_SYMBOL(iterator)
DEFINE_WK_SYMBOL(asyncIterator)
DEFINE_WK_SYMBOL(match)
DEFINE_WK_SYMBOL(matchAll)
DEFINE_WK_SYMBOL(replace)
DEFINE_WK_SYMBOL(search)
DEFINE_WK_SYMBOL(species)
DEFINE_WK_SYMBOL(split)
DEFINE_WK_SYMBOL(toPrimitive)
DEFINE_WK_SYMBOL(toStringTag)
DEFINE_WK_SYMBOL(unscopables)

// プライベートヘルパー関数

// static private
int Symbol::GenerateUniqueId() {
  return s_nextSymbolId.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace core
}  // namespace aerojs