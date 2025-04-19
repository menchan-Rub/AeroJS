/**
 * @file symbol.cpp
 * @brief JavaScript のシンボル型の実装
 */

#include "symbol.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace aerojs {
namespace core {

// 静的メンバ変数の初期化
std::atomic<int> Symbol::s_nextId(1);
std::unordered_map<std::string, SymbolPtr> Symbol::s_registry;
std::unordered_map<int, SymbolPtr> Symbol::s_keyRegistry;
std::unordered_map<SymbolPtr, std::string, 
                  std::hash<SymbolPtr>, 
                  std::equal_to<SymbolPtr>> Symbol::s_reverseRegistry;
std::mutex Symbol::s_mutex;

// Well-known シンボルの静的インスタンス
SymbolPtr Symbol::s_hasInstanceSymbol;
SymbolPtr Symbol::s_isConcatSpreadableSymbol;
SymbolPtr Symbol::s_iteratorSymbol;
SymbolPtr Symbol::s_asyncIteratorSymbol;
SymbolPtr Symbol::s_matchSymbol;
SymbolPtr Symbol::s_matchAllSymbol;
SymbolPtr Symbol::s_replaceSymbol;
SymbolPtr Symbol::s_searchSymbol;
SymbolPtr Symbol::s_speciesSymbol;
SymbolPtr Symbol::s_splitSymbol;
SymbolPtr Symbol::s_toPrimitiveSymbol;
SymbolPtr Symbol::s_toStringTagSymbol;
SymbolPtr Symbol::s_unscopablesSymbol;

// コンストラクタ
Symbol::Symbol(const std::string& description)
    : m_description(description),
      m_id(s_nextId.fetch_add(1, std::memory_order_acq_rel)) // より適切なメモリオーダー
{
}

// キー登録付きコンストラクタ
Symbol::Symbol(const std::string& description, bool registerKey)
    : m_description(description),
      m_id(s_nextId.fetch_add(1, std::memory_order_acq_rel)) // より適切なメモリオーダー
{
    if (registerKey) {
        // shared_from_this()を使って自身のスマートポインタを取得
        try {
            auto self = shared_from_this();
            std::lock_guard<std::mutex> lock(s_mutex);
            s_keyRegistry[m_id] = self;
        } catch (const std::bad_weak_ptr&) {
            // まだshared_ptrが作成されていない場合の例外処理
            // この場合は後でcreateメソッドで登録される
        }
    }
}

// ムーブコンストラクタ
Symbol::Symbol(Symbol&& other) noexcept
    : m_description(std::move(other.m_description)),
      m_id(other.m_id)
{
    // 移動元のIDを無効化
    other.m_id = -1;
}

// ムーブ代入演算子
Symbol& Symbol::operator=(Symbol&& other) noexcept {
    if (this != &other) {
        // 自分自身のIDを持つシンボルをレジストリから削除
        if (m_id >= 0) {
            removeFromRegistry(m_id);
        }
        
        // 他方のデータを移動
        m_description = std::move(other.m_description);
        m_id = other.m_id;
        
        // 移動元のIDを無効化
        other.m_id = -1;
    }
    return *this;
}

// デストラクタ
Symbol::~Symbol() {
    // デストラクタではremoveFromRegistryを呼び出す
    if (m_id >= 0) {
        removeFromRegistry(m_id);
    }
}

// レジストリから特定のシンボルを安全に削除する
bool Symbol::removeFromRegistry(int symbolId) {
    if (symbolId < 0) {
        return false; // 無効なIDは削除しない
    }
    
    std::lock_guard<std::mutex> lock(s_mutex);
    bool removed = false;
    
    // IDレジストリから削除
    auto it = s_keyRegistry.find(symbolId);
    if (it != s_keyRegistry.end()) {
        SymbolPtr symbol = it->second;
        
        // 逆引きマップからも削除
        s_reverseRegistry.erase(symbol);
        
        // IDレジストリから削除
        s_keyRegistry.erase(it);
        removed = true;
        
        // 文字列キーによるレジストリからも削除
        for (auto regIt = s_registry.begin(); regIt != s_registry.end();) {
            if (regIt->second == symbol) {
                regIt = s_registry.erase(regIt);
            } else {
                ++regIt;
            }
        }
    }
    
    return removed;
}

// シンボルレジストリをリセットする（主にテスト用）
void Symbol::resetRegistry() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    // すべてのWell-knownシンボルをnullptrに設定
    s_hasInstanceSymbol = nullptr;
    s_isConcatSpreadableSymbol = nullptr;
    s_iteratorSymbol = nullptr;
    s_asyncIteratorSymbol = nullptr;
    s_matchSymbol = nullptr;
    s_matchAllSymbol = nullptr;
    s_replaceSymbol = nullptr;
    s_searchSymbol = nullptr;
    s_speciesSymbol = nullptr;
    s_splitSymbol = nullptr;
    s_toPrimitiveSymbol = nullptr;
    s_toStringTagSymbol = nullptr;
    s_unscopablesSymbol = nullptr;
    
    // レジストリをクリア
    s_registry.clear();
    s_keyRegistry.clear();
    s_reverseRegistry.clear();
    
    // IDカウンタをリセット
    s_nextId.store(1, std::memory_order_relaxed);
}

// ファクトリメソッド - 説明付きシンボル作成
SymbolPtr Symbol::create(const std::string& description) {
    return std::make_shared<Symbol>(description);
}

// ファクトリメソッド - キー登録付きシンボル作成
SymbolPtr Symbol::create(const std::string& description, bool registerKey) {
    auto symbol = std::make_shared<Symbol>(description);
    
    if (registerKey) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_keyRegistry[symbol->id()] = symbol;
    }
    
    return symbol;
}

// シンボルの説明を取得
const std::string& Symbol::description() const noexcept {
    return m_description;
}

// シンボルの説明を文字列ビューとして取得
std::string_view Symbol::descriptionView() const noexcept {
    return std::string_view(m_description);
}

// シンボルのIDを取得
int Symbol::id() const noexcept {
    return m_id;
}

// シンボルを文字列に変換
std::string Symbol::toString() const {
    std::ostringstream oss;
    oss << "Symbol(";
    if (!m_description.empty()) {
        oss << m_description;
    }
    oss << ")";
    return oss.str();
}

// デバッグ用の詳細な文字列表現
std::string Symbol::debugString() const {
    std::ostringstream oss;
    oss << "Symbol@" << m_id << "(";
    if (!m_description.empty()) {
        oss << "\"" << m_description << "\"";
    }
    oss << ")";
    return oss.str();
}

// 等価比較演算子
bool Symbol::operator==(const Symbol& other) const noexcept {
    return m_id == other.m_id;
}

// 非等価比較演算子
bool Symbol::operator!=(const Symbol& other) const noexcept {
    return m_id != other.m_id;
}

// スマートポインタ版等価比較
bool Symbol::equals(const SymbolPtr& lhs, const SymbolPtr& rhs) noexcept {
    // null チェックを含む安全な比較
    if (lhs == nullptr && rhs == nullptr) {
        return true;
    }
    if (lhs == nullptr || rhs == nullptr) {
        return false;
    }
    return *lhs == *rhs;
}

// グローバルシンボル取得または作成
SymbolPtr Symbol::for_(const std::string& key) {
    if (key.empty()) {
        throw std::invalid_argument("Symbol.for requires a non-empty string key");
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    
    // 既存のシンボルを検索
    auto it = s_registry.find(key);
    if (it != s_registry.end() && it->second) {
        return it->second;
    }
    
    // 新しいシンボルを作成して登録
    auto symbol = std::make_shared<Symbol>(key);
    s_registry[key] = symbol;
    s_keyRegistry[symbol->id()] = symbol;
    s_reverseRegistry[symbol] = key; // 逆引きマッピングも登録
    
    return symbol;
}

// シンボルに対応するキーを取得 (スマートポインタ版)
std::string Symbol::keyFor(const SymbolPtr& symbol) {
    if (!symbol) {
        throw std::invalid_argument("Symbol.keyFor requires a non-null symbol");
    }
    
    std::lock_guard<std::mutex> lock(s_mutex);
    
    // 効率的なO(1)の逆引き
    auto it = s_reverseRegistry.find(symbol);
    if (it != s_reverseRegistry.end()) {
        return it->second;
    }
    
    return ""; // レジストリに登録されていない場合は空文字列
}

// シンボルに対応するキーを取得 (生ポインタ版)
std::string Symbol::keyFor(Symbol* symbol) {
    if (!symbol) {
        throw std::invalid_argument("Symbol.keyFor requires a non-null symbol");
    }
    
    // シンボルのIDを取得し、それに対応するスマートポインタを取得
    const int id = symbol->id();
    
    std::lock_guard<std::mutex> lock(s_mutex);
    
    auto it = s_keyRegistry.find(id);
    if (it != s_keyRegistry.end()) {
        return keyFor(it->second);
    }
    
    return ""; // レジストリに登録されていない場合は空文字列
}

// 指定されたIDのシンボルを取得
std::optional<SymbolPtr> Symbol::getById(int id) {
    if (id < 0) {
        return std::nullopt;
    }
    
    std::lock_guard<std::mutex> lock(s_mutex);
    
    auto it = s_keyRegistry.find(id);
    if (it != s_keyRegistry.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

// レジストリサイズ取得
size_t Symbol::registrySize() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_registry.size();
}

// レジストリ内容をダンプ
std::string Symbol::dumpRegistry() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    std::ostringstream oss;
    oss << "Symbol Registry (" << s_registry.size() << " entries):" << std::endl;
    
    for (const auto& entry : s_registry) {
        oss << "  " << std::left << std::setw(30) << entry.first 
            << " -> " << entry.second->debugString() << std::endl;
    }
    
    return oss.str();
}

// カスタムシンボル登録
void Symbol::registerCustomSymbol(const std::string& name, SymbolPtr symbol) {
    if (name.empty()) {
        throw std::invalid_argument("Symbol name cannot be empty");
    }
    
    if (!symbol) {
        throw std::invalid_argument("Symbol cannot be null");
    }
    
    std::lock_guard<std::mutex> lock(s_mutex);
    
    // すでに同じ名前のシンボルが存在するかチェック
    auto it = s_registry.find(name);
    if (it != s_registry.end() && it->second != symbol) {
        throw std::invalid_argument("Symbol with name '" + name + "' already exists");
    }
    
    s_registry[name] = symbol;
    s_keyRegistry[symbol->id()] = symbol;
    s_reverseRegistry[symbol] = name;
}

// Well-known シンボルの初期化用ヘルパー関数
static SymbolPtr initWellKnownSymbol(SymbolPtr& symbolPtr, 
                                    const std::string& description, 
                                    std::mutex& mutex) {
    if (!symbolPtr) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!symbolPtr) {
            auto symbol = Symbol::create(description);
            
            // グローバルレジストリにwell-knownシンボルを登録
            try {
                std::lock_guard<std::mutex> regLock(Symbol::s_mutex);
                Symbol::s_keyRegistry[symbol->id()] = symbol;
                Symbol::s_reverseRegistry[symbol] = description;
            } catch (const std::exception& e) {
                // ロック取得に失敗した場合のエラーハンドリング
                // 通常は発生しないが、安全のため
            }
            
            symbolPtr = std::move(symbol);
        }
    }
    return symbolPtr;
}

// Well-known シンボル: hasInstance
SymbolPtr Symbol::hasInstance() {
    static std::mutex mutex;
    return initWellKnownSymbol(s_hasInstanceSymbol, "Symbol.hasInstance", mutex);
}

// Well-known シンボル: isConcatSpreadable
SymbolPtr Symbol::isConcatSpreadable() {
    static std::mutex mutex;
    return initWellKnownSymbol(s_isConcatSpreadableSymbol, "Symbol.isConcatSpreadable", mutex);
}

// Well-known シンボル: iterator
SymbolPtr Symbol::iterator() {
    static std::mutex mutex;
    return initWellKnownSymbol(s_iteratorSymbol, "Symbol.iterator", mutex);
}

// Well-known シンボル: asyncIterator
SymbolPtr Symbol::asyncIterator() {
    static std::mutex mutex;
    return initWellKnownSymbol(s_asyncIteratorSymbol, "Symbol.asyncIterator", mutex);
}

// Well-known シンボル: match
SymbolPtr Symbol::match() {
    static std::mutex mutex;
    return initWellKnownSymbol(s_matchSymbol, "Symbol.match", mutex);
}

// Well-known シンボル: matchAll
SymbolPtr Symbol::matchAll() {
    static std::mutex mutex;
    return initWellKnownSymbol(s_matchAllSymbol, "Symbol.matchAll", mutex);
}

// Well-known シンボル: replace
SymbolPtr Symbol::replace() {
    static std::mutex mutex;
    return initWellKnownSymbol(s_replaceSymbol, "Symbol.replace", mutex);
}

// Well-known シンボル: search
SymbolPtr Symbol::search() {
    static std::mutex mutex;
    return initWellKnownSymbol(s_searchSymbol, "Symbol.search", mutex);
}

// Well-known シンボル: species
SymbolPtr Symbol::species() {
    static std::mutex mutex;
    return initWellKnownSymbol(s_speciesSymbol, "Symbol.species", mutex);
}

// Well-known シンボル: split
SymbolPtr Symbol::split() {
    static std::mutex mutex;
    return initWellKnownSymbol(s_splitSymbol, "Symbol.split", mutex);
}

// Well-known シンボル: toPrimitive
SymbolPtr Symbol::toPrimitive() {
    static std::mutex mutex;
    return initWellKnownSymbol(s_toPrimitiveSymbol, "Symbol.toPrimitive", mutex);
}

// Well-known シンボル: toStringTag
SymbolPtr Symbol::toStringTag() {
    static std::mutex mutex;
    return initWellKnownSymbol(s_toStringTagSymbol, "Symbol.toStringTag", mutex);
}

// Well-known シンボル: unscopables
SymbolPtr Symbol::unscopables() {
    static std::mutex mutex;
    return initWellKnownSymbol(s_unscopablesSymbol, "Symbol.unscopables", mutex);
}

// Well-known シンボル アクセサメソッド実装
namespace well_known {

SymbolPtr getAsyncIterator() {
    return Symbol::asyncIterator();
}

SymbolPtr getHasInstance() {
    return Symbol::hasInstance();
}

SymbolPtr getIsConcatSpreadable() {
    return Symbol::isConcatSpreadable();
}

SymbolPtr getIterator() {
    return Symbol::iterator();
}

SymbolPtr getMatch() {
    return Symbol::match();
}

SymbolPtr getMatchAll() {
    return Symbol::matchAll();
}

SymbolPtr getReplace() {
    return Symbol::replace();
}

SymbolPtr getSearch() {
    return Symbol::search();
}

SymbolPtr getSpecies() {
    return Symbol::species();
}

SymbolPtr getSplit() {
    return Symbol::split();
}

SymbolPtr getToPrimitive() {
    return Symbol::toPrimitive();
}

SymbolPtr getToStringTag() {
    return Symbol::toStringTag();
}

SymbolPtr getUnscopables() {
    return Symbol::unscopables();
}

} // namespace well_known

} // namespace core
} // namespace aerojs