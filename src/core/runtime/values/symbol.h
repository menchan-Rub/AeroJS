#pragma once

#include <atomic>
#include <functional>  // for std::hash
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>  // より効率的な文字列操作のため
#include <unordered_map>

namespace aerojs {
namespace core {

// 前方宣言とスマートポインタの型エイリアス定義
class Symbol;
using SymbolPtr = std::shared_ptr<Symbol>;
using SymbolWeakPtr = std::weak_ptr<Symbol>;

/**
 * @brief JavaScript のシンボル型を表すクラス (修正版 2)
 */
class Symbol : public std::enable_shared_from_this<Symbol> {
 public:
  /**
   * @brief 説明付きシンボルを作成する (Public コンストラクタ)
   * @param description シンボルの説明（オプション）
   */
  explicit Symbol(const std::string& description = "");

  /**
   * @brief ムーブコンストラクタ
   */
  Symbol(Symbol&& other) noexcept;

  /**
   * @brief ムーブ代入演算子
   */
  Symbol& operator=(Symbol&& other) noexcept;

  // コピー操作は禁止
  Symbol(const Symbol&) = delete;
  Symbol& operator=(const Symbol&) = delete;

  /**
   * @brief デストラクタ
   */
  ~Symbol();

  /**
   * @brief 説明付きシンボルを作成する (ファクトリメソッド)
   */
  static SymbolPtr create(const std::string& description = "");

  /**
   * @brief シンボルの説明を取得する
   */
  const std::string& description() const noexcept;

  /**
   * @brief シンボルの説明を文字列ビューとして取得する (高効率)
   */
  std::string_view descriptionView() const noexcept;

  /**
   * @brief シンボルの一意な識別子を取得する
   */
  constexpr int id() const noexcept {
    return m_id;
  }

  /**
   * @brief シンボルの文字列表現を取得する
   */
  std::string toString() const;

  /**
   * @brief デバッグ用の詳細な文字列表現を取得する
   */
  std::string debugString() const;

  /**
   * @brief 等価比較演算子 (ID で比較)
   */
  constexpr bool operator==(const Symbol& other) const noexcept {
    // IDが有効(-1でない)かつ一致するか
    return m_id >= 0 && m_id == other.m_id;
  }

  /**
   * @brief 非等価比較演算子
   */
  constexpr bool operator!=(const Symbol& other) const noexcept {
    return !(*this == other);
  }

  /**
   * @brief 二つのシンボルポインタが同じシンボルを指しているか比較する (ID で比較)
   */
  static bool equals(const SymbolPtr& lhs, const SymbolPtr& rhs) noexcept;

  /**
   * @brief グローバルシンボルレジストリ内のキーに対応するシンボルを取得または作成する
   */
  static SymbolPtr For(const std::string& key);

  /**
   * @brief シンボルに対応するグローバルシンボルレジストリのキーを取得する
   */
  static std::optional<std::string> KeyFor(const SymbolPtr& symbol);

  /**
   * @brief 指定されたIDを持つシンボルを取得する (主に内部・デバッグ用)
   */
  static std::optional<SymbolPtr> getById(int id);

  /**
   * @brief グローバルシンボルレジストリのサイズを取得する
   */
  static size_t registrySize();

  /**
   * @brief グローバルシンボルレジストリの内容をデバッグ用に文字列化する
   */
  static std::string dumpRegistry();

  /**
   * @brief グローバルシンボルレジストリをクリアする（テスト用）
   * 注意: このメソッドはテスト以外で使用しないでください。
   */
  static void resetRegistryForTesting();

  // Well-known Symbols (Static getter methods)
  static SymbolPtr hasInstance();
  static SymbolPtr isConcatSpreadable();
  static SymbolPtr iterator();
  static SymbolPtr asyncIterator();
  static SymbolPtr match();
  static SymbolPtr matchAll();
  static SymbolPtr replace();
  static SymbolPtr search();
  static SymbolPtr species();
  static SymbolPtr split();
  static SymbolPtr toPrimitive();
  static SymbolPtr toStringTag();
  static SymbolPtr unscopables();

 private:
  std::string m_description;  ///< シンボルの説明
  int m_id;                   ///< シンボルの一意な識別子 (-1 は無効状態を示す)

  /**
   * @brief 一意なIDを生成する内部ヘルパー関数
   */
  static int GenerateUniqueId();
};

// シンボルポインタの等価比較（インライン実装）
inline bool Symbol::equals(const SymbolPtr& lhs, const SymbolPtr& rhs) noexcept {
  if (lhs == nullptr && rhs == nullptr) {
    return true;  // 両方 null なら等しい
  }
  if (lhs == nullptr || rhs == nullptr) {
    return false;  // 片方だけ null なら等しくない
  }
  // 同じ ID を持つか (Symbol::operator== を使う)
  return *lhs == *rhs;
}

}  // namespace core
}  // namespace aerojs

// std::hash の特殊化 (ヘッダーで定義)
namespace std {
/**
 * @brief Symbol クラスのためのハッシュ関数の特殊化
 */
template <>
struct hash<aerojs::core::Symbol> {
  // id() は constexpr だが、std::hash<int> は constexpr 保証がないため、全体も constexpr にしない
  size_t operator()(const aerojs::core::Symbol& s) const noexcept {  // constexpr を削除
    return std::hash<int>{}(s.id());
  }
};

/**
 * @brief SymbolPtr のためのハッシュ関数の特殊化
 */
template <>
struct hash<aerojs::core::SymbolPtr> {
  size_t operator()(const aerojs::core::SymbolPtr& s) const noexcept {
    return s ? std::hash<aerojs::core::Symbol>{}(*s) : 0;
  }
};
}  // namespace std