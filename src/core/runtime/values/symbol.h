/**
 * @file symbol.h
 * @brief JavaScript Symbolクラスの定義
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_SYMBOL_H
#define AEROJS_SYMBOL_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace aerojs {
namespace core {

// 前方宣言
class Symbol;
using SymbolPtr = std::shared_ptr<Symbol>;

/**
 * @brief JavaScript Symbol型を表現するクラス
 */
class Symbol {
 public:
  /**
   * @brief 新しいSymbolを作成
   * @param description Symbolの説明文字列（省略可能）
   */
  explicit Symbol(const std::string& description = "");

  /**
   * @brief デストラクタ
   */
  virtual ~Symbol() = default;

  /**
   * @brief Symbolの説明文字列を取得
   * @return 説明文字列
   */
  const std::string& description() const {
    return description_;
  }

  /**
   * @brief Symbolの一意ID を取得
   * @return 一意ID
   */
  uint64_t id() const {
    return id_;
  }

  /**
   * @brief Symbolの文字列表現を取得
   * @return "Symbol(description)" 形式の文字列
   */
  std::string toString() const;

  /**
   * @brief 新しいSymbolを作成する
   * @param description 説明文字列
   * @return 新しいSymbol
   */
  static Symbol* create(const std::string& description = "");

  /**
   * @brief 等価比較演算子
   */
  bool operator==(const Symbol& other) const {
    return id_ == other.id_;
  }

  // Well-known symbols
  static Symbol* iterator();
  static Symbol* hasInstance();
  static Symbol* toPrimitive();
  static Symbol* toStringTag();

 private:
  // 一意ID生成用カウンタ
  static std::atomic<uint64_t> nextId_;

  // 内部プロパティ
  std::string description_;  // 説明文字列
  uint64_t id_;             // 一意ID

  // コピー・ムーブを禁止
  Symbol(const Symbol&) = delete;
  Symbol& operator=(const Symbol&) = delete;
  Symbol(Symbol&&) = delete;
  Symbol& operator=(Symbol&&) = delete;
};

}  // namespace core
}  // namespace aerojs

// std::hash の特殊化
namespace std {
template <>
struct hash<aerojs::core::Symbol> {
  size_t operator()(const aerojs::core::Symbol& s) const noexcept {
    return std::hash<uint64_t>{}(s.id());
  }
};
}  // namespace std

#endif  // AEROJS_SYMBOL_H