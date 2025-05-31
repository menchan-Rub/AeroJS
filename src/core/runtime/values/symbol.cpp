/**
 * @file symbol.cpp
 * @brief JavaScript Symbolクラスの実装
 * @version 0.1.0
 * @license MIT
 */

#include "symbol.h"
#include <sstream>

namespace aerojs {
namespace core {

// 静的メンバーの初期化
std::atomic<uint64_t> Symbol::nextId_{1};

Symbol::Symbol(const std::string& description)
    : description_(description), id_(nextId_.fetch_add(1)) {
}

std::string Symbol::toString() const {
  std::ostringstream oss;
  oss << "Symbol(";
  if (!description_.empty()) {
    oss << description_;
  }
  oss << ")";
  return oss.str();
}

Symbol* Symbol::create(const std::string& description) {
  return new Symbol(description);
}

// Well-known symbols の実装
Symbol* Symbol::iterator() {
  static Symbol* instance = new Symbol("Symbol.iterator");
  return instance;
}

Symbol* Symbol::hasInstance() {
  static Symbol* instance = new Symbol("Symbol.hasInstance");
  return instance;
}

Symbol* Symbol::toPrimitive() {
  static Symbol* instance = new Symbol("Symbol.toPrimitive");
  return instance;
}

Symbol* Symbol::toStringTag() {
  static Symbol* instance = new Symbol("Symbol.toStringTag");
  return instance;
}

}  // namespace core
}  // namespace aerojs