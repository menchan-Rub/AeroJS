/**
 * @file string_view.h
 * @brief 文字列を参照するためのクラス
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_STRING_VIEW_H
#define AEROJS_STRING_VIEW_H

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace aerojs {
namespace utils {

/**
 * @brief UTF-8文字列を参照するためのクラス
 *
 * このクラスは、所有権を持たない文字列ビューを提供します。
 * std::string_viewをラップし、UTF-8に特化した機能を追加しています。
 */
class StringView {
 public:
  // 型エイリアス
  using traits_type = std::char_traits<char>;
  using value_type = char;
  using pointer = const char*;
  using const_pointer = const char*;
  using reference = const char&;
  using const_reference = const char&;
  using const_iterator = std::string_view::const_iterator;
  using iterator = const_iterator;
  using const_reverse_iterator = std::string_view::const_reverse_iterator;
  using reverse_iterator = const_reverse_iterator;
  using size_type = std::string_view::size_type;
  using difference_type = std::string_view::difference_type;

  // 静的定数
  static constexpr size_type npos = static_cast<size_type>(-1);

  // デフォルトコンストラクタ
  constexpr StringView() noexcept
      : m_view() {
  }

  // 文字列とサイズからの構築
  constexpr StringView(const char* str, size_type len) noexcept
      : m_view(str, len) {
  }

  // null終端文字列からの構築
  constexpr StringView(const char* str) noexcept
      : m_view(str) {
  }

  // std::stringからの構築
  StringView(const std::string& str) noexcept
      : m_view(str) {
  }

  // std::string_viewからの構築
  constexpr StringView(std::string_view view) noexcept
      : m_view(view) {
  }

  // コピーコンストラクタ
  constexpr StringView(const StringView&) noexcept = default;

  // コピー代入演算子
  constexpr StringView& operator=(const StringView&) noexcept = default;

  // std::string_viewへの変換
  constexpr operator std::string_view() const noexcept {
    return m_view;
  }

  // 文字列データへのアクセス
  constexpr const char* data() const noexcept {
    return m_view.data();
  }
  constexpr size_type size() const noexcept {
    return m_view.size();
  }
  constexpr size_type length() const noexcept {
    return m_view.length();
  }
  constexpr bool empty() const noexcept {
    return m_view.empty();
  }

  // イテレータ
  constexpr const_iterator begin() const noexcept {
    return m_view.begin();
  }
  constexpr const_iterator end() const noexcept {
    return m_view.end();
  }
  constexpr const_iterator cbegin() const noexcept {
    return m_view.cbegin();
  }
  constexpr const_iterator cend() const noexcept {
    return m_view.cend();
  }
  constexpr const_reverse_iterator rbegin() const noexcept {
    return m_view.rbegin();
  }
  constexpr const_reverse_iterator rend() const noexcept {
    return m_view.rend();
  }
  constexpr const_reverse_iterator crbegin() const noexcept {
    return m_view.crbegin();
  }
  constexpr const_reverse_iterator crend() const noexcept {
    return m_view.crend();
  }

  // 要素アクセス
  constexpr char operator[](size_type pos) const noexcept {
    return m_view[pos];
  }

  constexpr char at(size_type pos) const {
    if (pos >= size()) {
      throw std::out_of_range("StringView::at: pos out of range");
    }
    return m_view[pos];
  }

  constexpr char front() const {
    if (empty()) {
      throw std::out_of_range("StringView::front: empty string");
    }
    return m_view.front();
  }

  constexpr char back() const {
    if (empty()) {
      throw std::out_of_range("StringView::back: empty string");
    }
    return m_view.back();
  }

  // 文字列の一部を取得
  constexpr StringView substr(size_type pos = 0, size_type count = npos) const {
    return m_view.substr(pos, count);
  }

  // 先頭・末尾からの文字削除
  constexpr void remove_prefix(size_type n) noexcept {
    m_view.remove_prefix(n);
  }

  constexpr void remove_suffix(size_type n) noexcept {
    m_view.remove_suffix(n);
  }

  // 比較
  constexpr int compare(StringView sv) const noexcept {
    return m_view.compare(sv.m_view);
  }

  constexpr int compare(size_type pos1, size_type count1, StringView sv) const {
    return m_view.compare(pos1, count1, sv.m_view);
  }

  constexpr int compare(size_type pos1, size_type count1, StringView sv,
                        size_type pos2, size_type count2) const {
    return m_view.compare(pos1, count1, sv.m_view, pos2, count2);
  }

  constexpr int compare(const char* s) const {
    return m_view.compare(s);
  }

  constexpr int compare(size_type pos1, size_type count1, const char* s) const {
    return m_view.compare(pos1, count1, s);
  }

  constexpr int compare(size_type pos1, size_type count1, const char* s, size_type count2) const {
    return m_view.compare(pos1, count1, s, count2);
  }

  // 検索
  constexpr bool starts_with(StringView sv) const noexcept {
    return size() >= sv.size() && compare(0, sv.size(), sv) == 0;
  }

  constexpr bool starts_with(char c) const noexcept {
    return !empty() && front() == c;
  }

  constexpr bool starts_with(const char* s) const {
    return starts_with(StringView(s));
  }

  constexpr bool ends_with(StringView sv) const noexcept {
    return size() >= sv.size() && compare(size() - sv.size(), npos, sv) == 0;
  }

  constexpr bool ends_with(char c) const noexcept {
    return !empty() && back() == c;
  }

  constexpr bool ends_with(const char* s) const {
    return ends_with(StringView(s));
  }

  constexpr bool contains(StringView sv) const noexcept {
    return find(sv) != npos;
  }

  constexpr bool contains(char c) const noexcept {
    return find(c) != npos;
  }

  constexpr bool contains(const char* s) const {
    return contains(StringView(s));
  }

  constexpr size_type find(StringView sv, size_type pos = 0) const noexcept {
    return m_view.find(sv.m_view, pos);
  }

  constexpr size_type find(char c, size_type pos = 0) const noexcept {
    return m_view.find(c, pos);
  }

  constexpr size_type find(const char* s, size_type pos, size_type count) const {
    return m_view.find(s, pos, count);
  }

  constexpr size_type find(const char* s, size_type pos = 0) const {
    return m_view.find(s, pos);
  }

  constexpr size_type rfind(StringView sv, size_type pos = npos) const noexcept {
    return m_view.rfind(sv.m_view, pos);
  }

  constexpr size_type rfind(char c, size_type pos = npos) const noexcept {
    return m_view.rfind(c, pos);
  }

  constexpr size_type rfind(const char* s, size_type pos, size_type count) const {
    return m_view.rfind(s, pos, count);
  }

  constexpr size_type rfind(const char* s, size_type pos = npos) const {
    return m_view.rfind(s, pos);
  }

  constexpr size_type find_first_of(StringView sv, size_type pos = 0) const noexcept {
    return m_view.find_first_of(sv.m_view, pos);
  }

  constexpr size_type find_first_of(char c, size_type pos = 0) const noexcept {
    return m_view.find_first_of(c, pos);
  }

  constexpr size_type find_first_of(const char* s, size_type pos, size_type count) const {
    return m_view.find_first_of(s, pos, count);
  }

  constexpr size_type find_first_of(const char* s, size_type pos = 0) const {
    return m_view.find_first_of(s, pos);
  }

  constexpr size_type find_last_of(StringView sv, size_type pos = npos) const noexcept {
    return m_view.find_last_of(sv.m_view, pos);
  }

  constexpr size_type find_last_of(char c, size_type pos = npos) const noexcept {
    return m_view.find_last_of(c, pos);
  }

  constexpr size_type find_last_of(const char* s, size_type pos, size_type count) const {
    return m_view.find_last_of(s, pos, count);
  }

  constexpr size_type find_last_of(const char* s, size_type pos = npos) const {
    return m_view.find_last_of(s, pos);
  }

  constexpr size_type find_first_not_of(StringView sv, size_type pos = 0) const noexcept {
    return m_view.find_first_not_of(sv.m_view, pos);
  }

  constexpr size_type find_first_not_of(char c, size_type pos = 0) const noexcept {
    return m_view.find_first_not_of(c, pos);
  }

  constexpr size_type find_first_not_of(const char* s, size_type pos, size_type count) const {
    return m_view.find_first_not_of(s, pos, count);
  }

  constexpr size_type find_first_not_of(const char* s, size_type pos = 0) const {
    return m_view.find_first_not_of(s, pos);
  }

  constexpr size_type find_last_not_of(StringView sv, size_type pos = npos) const noexcept {
    return m_view.find_last_not_of(sv.m_view, pos);
  }

  constexpr size_type find_last_not_of(char c, size_type pos = npos) const noexcept {
    return m_view.find_last_not_of(c, pos);
  }

  constexpr size_type find_last_not_of(const char* s, size_type pos, size_type count) const {
    return m_view.find_last_not_of(s, pos, count);
  }

  constexpr size_type find_last_not_of(const char* s, size_type pos = npos) const {
    return m_view.find_last_not_of(s, pos);
  }

  // std::stringに変換
  std::string to_string() const {
    return std::string(m_view);
  }

  // UTF-8関連機能 (基本的な実装)

  // UTF-8文字列の文字数 (コードポイント数)を取得
  size_type utf8_length() const noexcept {
    size_type len = 0;
    const char* str = data();
    const char* end = str + size();

    while (str < end) {
      // UTF-8の先頭バイトでカウント
      if ((*str & 0xC0) != 0x80) {
        ++len;
      }
      ++str;
    }

    return len;
  }

  // UTF-8のコードポイント数からバイトインデックスを取得
  size_type utf8_index_to_byte(size_type index) const {
    if (index == 0) return 0;
    if (index >= utf8_length()) return size();

    size_type count = 0;
    size_type byte_index = 0;
    const char* str = data();
    const char* end = str + size();

    while (str < end && count < index) {
      // UTF-8の先頭バイトを検出
      if ((*str & 0xC0) != 0x80) {
        ++count;
      }
      ++str;
      ++byte_index;
    }

    return byte_index;
  }

  // 位置indexのUTF-8文字の開始位置を取得
  size_type utf8_char_begin(size_type index) const {
    return utf8_index_to_byte(index);
  }

  // 位置indexのUTF-8文字のバイト長を取得
  size_type utf8_char_length(size_type index) const {
    size_type begin = utf8_char_begin(index);
    if (begin >= size()) return 0;

    const unsigned char* str = reinterpret_cast<const unsigned char*>(data() + begin);

    if (*str < 0x80)
      return 1;
    else if ((*str & 0xE0) == 0xC0)
      return 2;
    else if ((*str & 0xF0) == 0xE0)
      return 3;
    else if ((*str & 0xF8) == 0xF0)
      return 4;
    else
      return 1;  // 不正なUTF-8バイト
  }

  // UTF-8の部分文字列を取得
  StringView utf8_substr(size_type char_pos, size_type char_count = npos) const {
    if (char_pos >= utf8_length()) return StringView();

    size_type byte_pos = utf8_index_to_byte(char_pos);

    if (char_count == npos) {
      return substr(byte_pos);
    }

    size_type end_pos = utf8_index_to_byte(char_pos + char_count);
    return substr(byte_pos, end_pos - byte_pos);
  }

  // 指定位置の文字がUTF-8の先頭バイトかどうかを確認
  bool is_utf8_char_boundary(size_type index) const {
    if (index >= size()) return false;
    return (data()[index] & 0xC0) != 0x80;
  }

 private:
  std::string_view m_view;
};

// 非メンバ関数

// 比較演算子
inline bool operator==(StringView lhs, StringView rhs) noexcept {
  return lhs.compare(rhs) == 0;
}

inline bool operator!=(StringView lhs, StringView rhs) noexcept {
  return lhs.compare(rhs) != 0;
}

inline bool operator<(StringView lhs, StringView rhs) noexcept {
  return lhs.compare(rhs) < 0;
}

inline bool operator<=(StringView lhs, StringView rhs) noexcept {
  return lhs.compare(rhs) <= 0;
}

inline bool operator>(StringView lhs, StringView rhs) noexcept {
  return lhs.compare(rhs) > 0;
}

inline bool operator>=(StringView lhs, StringView rhs) noexcept {
  return lhs.compare(rhs) >= 0;
}

}  // namespace utils
}  // namespace aerojs

// std::hashの特殊化
namespace std {
template <>
struct hash<aerojs::utils::StringView> {
  size_t operator()(const aerojs::utils::StringView& sv) const noexcept {
    return hash<string_view>{}(static_cast<string_view>(sv));
  }
};
}  // namespace std

#endif  // AEROJS_STRING_VIEW_H