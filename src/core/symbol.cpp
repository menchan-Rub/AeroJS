/**
 * @file symbol.cpp
 * @brief AeroJS JavaScript エンジンのシンボルクラスの実装
 * @version 0.1.0
 * @license MIT
 */

#include "symbol.h"

#include <sstream>

#include "context.h"

namespace aerojs {
namespace core {

// 静的メンバ変数の初期化
size_t Symbol::s_nextId = 1;
std::unordered_map<std::string, Symbol*> Symbol::s_registry;
std::unordered_map<std::string, Symbol*> Symbol::s_wellKnownSymbols;

Symbol::Symbol(Context* ctx, const std::string& description)
    : m_context(ctx), m_description(description), m_id(s_nextId++) {
  // シンボルIDをインクリメント
}

std::string Symbol::toString() const {
  std::stringstream ss;
  ss << "Symbol(";
  if (!m_description.empty()) {
    ss << m_description;
  }
  ss << ")";
  return ss.str();
}

bool Symbol::equals(const Symbol* other) const {
  if (!other) {
    return false;
  }
  // シンボルは同一インスタンスの場合のみ等しい
  return this == other;
}

Symbol* Symbol::forKey(Context* ctx, const std::string& key) {
  // グローバルシンボルレジストリで検索
  auto it = s_registry.find(key);
  if (it != s_registry.end()) {
    // 既存のシンボルが見つかった場合はそれを返す
    return it->second;
  }

  // 新しいシンボルを作成し、レジストリに登録
  Symbol* symbol = new Symbol(ctx, key);
  s_registry[key] = symbol;
  return symbol;
}

Symbol* Symbol::wellKnown(Context* ctx, const std::string& name) {
  // 既知のシンボル名を検索
  auto it = s_wellKnownSymbols.find(name);
  if (it != s_wellKnownSymbols.end()) {
    return it->second;
  }

  // 見つからない場合は新規作成して保存
  Symbol* symbol = new Symbol(ctx, name);
  s_wellKnownSymbols[name] = symbol;

  return symbol;
}

/**
 * @brief Well-known Symbolsの初期化
 * @param ctx コンテキスト
 */
void initializeWellKnownSymbols(Context* ctx) {
  // ECMAScript Well-known Symbolsを初期化
  Symbol::wellKnown(ctx, "hasInstance");
  Symbol::wellKnown(ctx, "isConcatSpreadable");
  Symbol::wellKnown(ctx, "iterator");
  Symbol::wellKnown(ctx, "match");
  Symbol::wellKnown(ctx, "matchAll");
  Symbol::wellKnown(ctx, "replace");
  Symbol::wellKnown(ctx, "search");
  Symbol::wellKnown(ctx, "species");
  Symbol::wellKnown(ctx, "split");
  Symbol::wellKnown(ctx, "toPrimitive");
  Symbol::wellKnown(ctx, "toStringTag");
  Symbol::wellKnown(ctx, "unscopables");
}

}  // namespace core
}  // namespace aerojs