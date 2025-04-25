/**
 * @file string.cpp
 * @brief String クラスの実装
 * @version 0.1.0
 * @license MIT
 */

#include "src/core/runtime/values/string.h"

#include <algorithm>
#include <cctype>
#include <codecvt>
#include <cstring>
#include <functional>
#include <locale>

namespace aerojs {
namespace core {

// 静的メンバの初期化
std::unordered_map<std::string, String*> String::internedStrings_;
std::mutex String::internMutex_;

// コンストラクタ実装
String::String(const char* str) {
  if (!str) {
    // nullポインタの場合は空文字列を作成
    length_ = 0;
    utf8Length_ = 0;
    storageType_ = StorageType::Small;
    small_.data[0] = '\0';
    return;
  }

  size_t len = std::strlen(str);
  if (len <= SMALL_STRING_MAX_SIZE) {
    // スモールストリングとして保存
    std::memcpy(small_.data, str, len + 1);  // +1 for null terminator
    length_ = len;
    utf8Length_ = calculateUtf8Length(small_.data, len);
    storageType_ = StorageType::Small;
  } else {
    // 通常文字列として保存
    size_t capacity = len + 1;  // +1 for null terminator
    normal_.data = new char[capacity];
    std::memcpy(normal_.data, str, capacity);
    normal_.capacity = capacity;
    length_ = len;
    utf8Length_ = calculateUtf8Length(normal_.data, len);
    storageType_ = StorageType::Normal;
  }
}

String::String(const std::string& str) {
  size_t len = str.length();
  if (len <= SMALL_STRING_MAX_SIZE) {
    // スモールストリングとして保存
    std::memcpy(small_.data, str.c_str(), len + 1);  // +1 for null terminator
    length_ = len;
    utf8Length_ = calculateUtf8Length(small_.data, len);
    storageType_ = StorageType::Small;
  } else {
    // 通常文字列として保存
    size_t capacity = len + 1;  // +1 for null terminator
    normal_.data = new char[capacity];
    std::memcpy(normal_.data, str.c_str(), capacity);
    normal_.capacity = capacity;
    length_ = len;
    utf8Length_ = calculateUtf8Length(normal_.data, len);
    storageType_ = StorageType::Normal;
  }
}

String::String(const utils::StringView& view) {
  size_t len = view.size();
  if (len <= SMALL_STRING_MAX_SIZE) {
    // スモールストリングとして保存
    std::memcpy(small_.data, view.data(), len);
    small_.data[len] = '\0';  // null terminator
    length_ = len;
    utf8Length_ = calculateUtf8Length(small_.data, len);
    storageType_ = StorageType::Small;
  } else {
    // 通常文字列として保存
    size_t capacity = len + 1;  // +1 for null terminator
    normal_.data = new char[capacity];
    std::memcpy(normal_.data, view.data(), len);
    normal_.data[len] = '\0';  // null terminator
    normal_.capacity = capacity;
    length_ = len;
    utf8Length_ = calculateUtf8Length(normal_.data, len);
    storageType_ = StorageType::Normal;
  }
}

// 内部使用のコンストラクタ
String::String(const char* data, size_t length, StorageType type)
    : length_(length), storageType_(type) {
  switch (type) {
    case StorageType::Small:
      std::memcpy(small_.data, data, length);
      small_.data[length] = '\0';
      utf8Length_ = calculateUtf8Length(small_.data, length);
      break;

    case StorageType::Normal:
      normal_.data = new char[length + 1];
      std::memcpy(normal_.data, data, length);
      normal_.data[length] = '\0';
      normal_.capacity = length + 1;
      utf8Length_ = calculateUtf8Length(normal_.data, length);
      break;

    case StorageType::Static:
      static_.data = data;
      utf8Length_ = calculateUtf8Length(data, length);
      break;

    default:
      // 不正なタイプ
      length_ = 0;
      utf8Length_ = 0;
      storageType_ = StorageType::Small;
      small_.data[0] = '\0';
      break;
  }
}

// スライス文字列用コンストラクタ
String::String(String* source, size_t offset, size_t length)
    : length_(length), storageType_(StorageType::Sliced) {
  sliced_.source = source;
  sliced_.offset = offset;

  // ソース文字列を参照カウントでインクリメント
  if (source) {
    source->incRefCount();

    // UTF-8文字数の計算
    if (offset == 0 && length == source->length()) {
      // 完全なコピーの場合はUTF-8文字数を再利用
      utf8Length_ = source->utf8Length();
    } else {
      // 部分文字列の場合はUTF-8文字数を計算
      utf8Length_ = calculateUtf8Length(c_str(), length);
    }
  } else {
    // ソースが無効な場合は空文字列を作成
    length_ = 0;
    utf8Length_ = 0;
    storageType_ = StorageType::Small;
    small_.data[0] = '\0';
  }
}

// 連結文字列用コンストラクタ
String::String(String* left, String* right)
    : length_(left ? left->length() : 0) + (right ? right->length() : 0),
storageType_(StorageType::Concatenated) {
  concatenated_.left = left;
  concatenated_.right = right;

  // 両方の文字列を参照カウントでインクリメント
  if (left) {
    left->incRefCount();
  }

  if (right) {
    right->incRefCount();
  }

  // UTF-8文字数は両方の文字列のUTF-8文字数の合計
  utf8Length_ = (left ? left->utf8Length() : 0) + (right ? right->utf8Length() : 0);
}

// デストラクタ
String::~String() {
  switch (storageType_) {
    case StorageType::Normal:
      delete[] normal_.data;
      break;

    case StorageType::Sliced:
      // スライスソース文字列の参照カウントをデクリメント
      if (sliced_.source) {
        sliced_.source->decRefCount();
      }
      break;

    case StorageType::Concatenated:
      // 左右の文字列の参照カウントをデクリメント
      if (concatenated_.left) {
        concatenated_.left->decRefCount();
      }
      if (concatenated_.right) {
        concatenated_.right->decRefCount();
      }
      break;

    default:
      // スモールストリングと静的文字列は特別な解放は不要
      break;
  }
}

// C文字列への変換
const char* String::c_str() const {
  switch (storageType_) {
    case StorageType::Small:
      return small_.data;

    case StorageType::Normal:
      return normal_.data;

    case StorageType::Static:
      return static_.data;

    case StorageType::Sliced: {
      // スライス文字列用のスレッドローカルバッファを使用
      thread_local std::vector<char> tlsBuffer;

      if (!sliced_.source) {
        return "";
      }

      const char* sourceData = sliced_.source->c_str();
      size_t requiredSize = length_ + 1;  // null終端用に+1

      if (tlsBuffer.size() < requiredSize) {
        tlsBuffer.resize(requiredSize * 2);  // 余裕を持たせる
      }

      std::memcpy(tlsBuffer.data(), sourceData + sliced_.offset, length_);
      tlsBuffer[length_] = '\0';  // null終端

      return tlsBuffer.data();
    }

    case StorageType::Concatenated: {
      // 連結文字列用のスレッドローカルバッファを使用
      thread_local std::vector<char> tlsBuffer;
      size_t requiredSize = length_ + 1;  // null終端用に+1

      if (tlsBuffer.size() < requiredSize) {
        tlsBuffer.resize(requiredSize * 2);  // 余裕を持たせる
      }

      char* buffer = tlsBuffer.data();
      size_t offset = 0;

      // 左側の文字列をコピー
      if (concatenated_.left) {
        const char* leftData = concatenated_.left->c_str();
        size_t leftLen = concatenated_.left->length();
        std::memcpy(buffer + offset, leftData, leftLen);
        offset += leftLen;
      }

      // 右側の文字列をコピー
      if (concatenated_.right) {
        const char* rightData = concatenated_.right->c_str();
        size_t rightLen = concatenated_.right->length();
        std::memcpy(buffer + offset, rightData, rightLen);
        offset += rightLen;
      }

      buffer[offset] = '\0';  // null終端

      return buffer;
    }

    default:
      return "";
  }
}

// std::stringへの変換
std::string String::value() const {
  switch (storageType_) {
    case StorageType::Small:
      return std::string(small_.data, length_);

    case StorageType::Normal:
      return std::string(normal_.data, length_);

    case StorageType::Static:
      return std::string(static_.data, length_);

    case StorageType::Sliced: {
      if (!sliced_.source) {
        return "";
      }

      std::string sourceStr = sliced_.source->value();
      return sourceStr.substr(sliced_.offset, length_);
    }

    case StorageType::Concatenated: {
      std::string result;
      result.reserve(length_);

      if (concatenated_.left) {
        result += concatenated_.left->value();
      }

      if (concatenated_.right) {
        result += concatenated_.right->value();
      }

      return result;
    }

    default:
      return "";
  }
}

// 文字列ビュー取得
utils::StringView String::view() const {
  return utils::StringView(c_str(), length_);
}

// 部分文字列取得
String* String::substring(size_t start, size_t length) const {
  // 範囲チェック
  if (start >= length_ || length == 0) {
    // 空文字列を返す
    return new String();
  }

  // 長さの調整
  if (start + length > length_) {
    length = length_ - start;
  }

  // スタイルに応じた部分文字列作成
  switch (storageType_) {
    case StorageType::Small:
    case StorageType::Normal:
    case StorageType::Static:
      // スモールストリング、通常文字列、静的文字列の場合は直接スライス
      return new String(const_cast<String*>(this), start, length);

    case StorageType::Sliced: {
      // スライス文字列のスライスは元ソースからのスライスを作成
      if (!sliced_.source) {
        return new String();
      }
      return new String(sliced_.source, sliced_.offset + start, length);
    }

    case StorageType::Concatenated: {
      // 連結文字列は少し複雑
      size_t leftLength = concatenated_.left ? concatenated_.left->length() : 0;

      if (start < leftLength) {
        // 開始点が左側の文字列内
        if (start + length <= leftLength) {
          // 完全に左側の文字列内
          return concatenated_.left->substring(start, length);
        } else {
          // 左側と右側にまたがる
          String* leftPart = concatenated_.left->substring(start, leftLength - start);
          String* rightPart = concatenated_.right->substring(0, length - (leftLength - start));
          String* result = new String(leftPart, rightPart);
          leftPart->decRefCount();
          rightPart->decRefCount();
          return result;
        }
      } else {
        // 開始点が右側の文字列内
        return concatenated_.right->substring(start - leftLength, length);
      }
    }

    default:
      return new String();
  }
}

// 文字列結合
String* String::concat(const String* other) const {
  if (!other || other->length() == 0) {
    // otherが空の場合は自身のコピーを返す
    return new String(c_str());
  }

  if (length_ == 0) {
    // 自身が空の場合はotherのコピーを返す
    return new String(other->c_str());
  }

  // 合計の長さを計算
  size_t totalLength = length_ + other->length();

  if (totalLength <= SMALL_STRING_MAX_SIZE) {
    // 結果がスモールストリングに収まる場合は直接結合
    char buffer[SMALL_STRING_MAX_SIZE + 1];
    std::memcpy(buffer, c_str(), length_);
    std::memcpy(buffer + length_, other->c_str(), other->length());
    buffer[totalLength] = '\0';

    return new String(buffer);
  } else {
    // 連結文字列として作成
    return new String(const_cast<String*>(this), const_cast<String*>(other));
  }
}

// 文字検索
size_t String::indexOf(char ch, size_t fromIndex) const {
  if (fromIndex >= length_) {
    return static_cast<size_t>(-1);
  }

  // 文字列全体をstd::stringに変換して検索
  std::string str = value();
  size_t pos = str.find(ch, fromIndex);

  return pos == std::string::npos ? static_cast<size_t>(-1) : pos;
}

// 部分文字列検索
size_t String::indexOf(const String* str, size_t fromIndex) const {
  if (!str || str->length() == 0 || fromIndex >= length_) {
    return static_cast<size_t>(-1);
  }

  std::string haystack = value();
  std::string needle = str->value();

  size_t pos = haystack.find(needle, fromIndex);

  return pos == std::string::npos ? static_cast<size_t>(-1) : pos;
}

// 接頭辞チェック (文字)
bool String::startsWith(char ch) const {
  if (length_ == 0) {
    return false;
  }

  switch (storageType_) {
    case StorageType::Small:
      return small_.data[0] == ch;

    case StorageType::Normal:
      return normal_.data[0] == ch;

    case StorageType::Static:
      return static_.data[0] == ch;

    default:
      return c_str()[0] == ch;
  }
}

// 接頭辞チェック (文字列)
bool String::startsWith(const String* str) const {
  if (!str || str->length() == 0) {
    return true;  // 空文字列は全ての文字列の接頭辞
  }

  if (str->length() > length_) {
    return false;  // 自身より長い文字列は接頭辞になりえない
  }

  std::string haystack = value();
  std::string needle = str->value();

  return haystack.compare(0, needle.length(), needle) == 0;
}

// 接尾辞チェック (文字)
bool String::endsWith(char ch) const {
  if (length_ == 0) {
    return false;
  }

  switch (storageType_) {
    case StorageType::Small:
      return small_.data[length_ - 1] == ch;

    case StorageType::Normal:
      return normal_.data[length_ - 1] == ch;

    case StorageType::Static:
      return static_.data[length_ - 1] == ch;

    default:
      return c_str()[length_ - 1] == ch;
  }
}

// 接尾辞チェック (文字列)
bool String::endsWith(const String* str) const {
  if (!str || str->length() == 0) {
    return true;  // 空文字列は全ての文字列の接尾辞
  }

  if (str->length() > length_) {
    return false;  // 自身より長い文字列は接尾辞になりえない
  }

  std::string haystack = value();
  std::string needle = str->value();

  return haystack.compare(length_ - needle.length(), needle.length(), needle) == 0;
}

// 等値比較
bool String::equals(const String* other) const {
  if (!other) {
    return false;
  }

  if (this == other) {
    return true;  // 同じオブジェクト
  }

  if (length_ != other->length()) {
    return false;  // 長さが異なる
  }

  // スモールストリングとインターン文字列の場合は高速比較
  if (storageType_ == StorageType::Small && other->storageType_ == StorageType::Small) {
    return std::strcmp(small_.data, other->small_.data) == 0;
  }

  // その他の場合は文字列全体を比較
  return value() == other->value();
}

// 大文字小文字を区別しない等値比較
bool String::equalsIgnoreCase(const String* other) const {
  if (!other) {
    return false;
  }

  if (this == other) {
    return true;  // 同じオブジェクト
  }

  if (length_ != other->length()) {
    return false;  // 長さが異なる
  }

  std::string str1 = value();
  std::string str2 = other->value();

  // 大文字小文字を区別せずに比較
  return std::equal(str1.begin(), str1.end(), str2.begin(),
                    [](char a, char b) {
                      return std::tolower(a) == std::tolower(b);
                    });
}

// 文字列の正規化（フラット化）
String* String::flatten() const {
  switch (storageType_) {
    case StorageType::Small:
    case StorageType::Normal:
    case StorageType::Static:
      // すでにフラットなので自身のコピーを返す
      return new String(c_str());

    case StorageType::Sliced: {
      // スライス文字列を通常文字列に変換
      if (!sliced_.source) {
        return new String();
      }

      std::string sourceStr = sliced_.source->value();
      std::string slicedStr = sourceStr.substr(sliced_.offset, length_);

      return new String(slicedStr);
    }

    case StorageType::Concatenated: {
      // 連結文字列を通常文字列に変換
      return new String(value());
    }

    default:
      return new String();
  }
}

// 指定位置の文字を取得
std::string String::charAt(size_t index) const {
  // UTF-8文字列からインデックスでの文字を抽出
  // UTF-8は可変長エンコーディングなのでバイト位置の計算が必要

  if (index >= utf8Length_) {
    return "";  // インデックスが範囲外
  }

  const char* str = c_str();
  size_t bytePos = 0;
  size_t charPos = 0;

  // UTF-8文字を数えながらバイト位置を進める
  while (charPos < index && bytePos < length_) {
    // UTF-8の先頭バイトをチェック
    unsigned char byte = static_cast<unsigned char>(str[bytePos]);

    if ((byte & 0x80) == 0) {
      // 1バイト文字
      bytePos += 1;
    } else if ((byte & 0xE0) == 0xC0) {
      // 2バイト文字
      bytePos += 2;
    } else if ((byte & 0xF0) == 0xE0) {
      // 3バイト文字
      bytePos += 3;
    } else if ((byte & 0xF8) == 0xF0) {
      // 4バイト文字
      bytePos += 4;
    } else {
      // 不正なUTF-8シーケンス
      bytePos += 1;
    }

    charPos++;
  }

  if (bytePos >= length_) {
    return "";  // バイト位置が範囲外
  }

  // UTF-8文字の長さを決定
  unsigned char byte = static_cast<unsigned char>(str[bytePos]);
  size_t charLen = 1;

  if ((byte & 0x80) == 0) {
    // 1バイト文字
    charLen = 1;
  } else if ((byte & 0xE0) == 0xC0) {
    // 2バイト文字
    charLen = 2;
  } else if ((byte & 0xF0) == 0xE0) {
    // 3バイト文字
    charLen = 3;
  } else if ((byte & 0xF8) == 0xF0) {
    // 4バイト文字
    charLen = 4;
  }

  // UTF-8文字を抽出
  return std::string(str + bytePos, charLen);
}

// 指定位置のコードポイントを取得
uint32_t String::charCodeAt(size_t index) const {
  std::string ch = charAt(index);

  if (ch.empty()) {
    return 0;  // インデックスが範囲外
  }

  // UTF-8バイト列をUnicode コードポイントに変換
  const unsigned char* bytes = reinterpret_cast<const unsigned char*>(ch.c_str());

  if ((bytes[0] & 0x80) == 0) {
    // 1バイト文字
    return bytes[0];
  } else if ((bytes[0] & 0xE0) == 0xC0) {
    // 2バイト文字
    return ((bytes[0] & 0x1F) << 6) | (bytes[1] & 0x3F);
  } else if ((bytes[0] & 0xF0) == 0xE0) {
    // 3バイト文字
    return ((bytes[0] & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) | (bytes[2] & 0x3F);
  } else if ((bytes[0] & 0xF8) == 0xF0) {
    // 4バイト文字
    return ((bytes[0] & 0x07) << 18) | ((bytes[1] & 0x3F) << 12) |
           ((bytes[2] & 0x3F) << 6) | (bytes[3] & 0x3F);
  }

  return 0;  // 不正なUTF-8シーケンス
}

// 小文字変換
String* String::toLowerCase() const {
  std::string str = value();
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  return new String(str);
}

// 大文字変換
String* String::toUpperCase() const {
  std::string str = value();
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  return new String(str);
}

// ハッシュコード計算
size_t String::hashCode() const {
  // FNV-1aハッシュを使用
  constexpr size_t FNV_PRIME = 16777619;
  constexpr size_t OFFSET_BASIS = 2166136261u;

  const char* str = c_str();
  size_t hash = OFFSET_BASIS;

  for (size_t i = 0; i < length_; ++i) {
    hash ^= static_cast<unsigned char>(str[i]);
    hash *= FNV_PRIME;
  }

  return hash;
}

// 静的文字列の作成
String* String::createStatic(const char* str) {
  if (!str) {
    return new String();
  }

  size_t length = std::strlen(str);
  String* staticStr = new String();

  staticStr->storageType_ = StorageType::Static;
  staticStr->static_.data = str;
  staticStr->length_ = length;
  staticStr->utf8Length_ = calculateUtf8Length(str, length);

  return staticStr;
}

// インターン文字列の取得または作成
String* String::intern(const char* str) {
  if (!str) {
    return new String();
  }

  std::string stdStr(str);
  return intern(stdStr);
}

String* String::intern(const std::string& str) {
  std::lock_guard<std::mutex> lock(internMutex_);

  auto it = internedStrings_.find(str);
  if (it != internedStrings_.end()) {
    // 既存のインターン文字列が見つかった
    it->second->incRefCount();
    return it->second;
  }

  // 新しいインターン文字列を作成
  String* newStr = new String(str);
  internedStrings_[str] = newStr;

  return newStr;
}

// 文字列の作成
String* String::create(const char* str) {
  return new String(str);
}

String* String::create(const std::string& str) {
  return new String(str);
}

String* String::create(const utils::StringView& view) {
  return new String(view);
}

// UTF-8文字数の計算
size_t String::calculateUtf8Length(const char* data, size_t byteLength) {
  if (!data || byteLength == 0) {
    return 0;
  }

  size_t count = 0;

  for (size_t i = 0; i < byteLength;) {
    unsigned char byte = static_cast<unsigned char>(data[i]);

    // UTF-8エンコーディングルールに従って文字数をカウント
    if ((byte & 0x80) == 0) {
      // 1バイト文字
      i += 1;
    } else if ((byte & 0xE0) == 0xC0) {
      // 2バイト文字
      i += 2;
    } else if ((byte & 0xF0) == 0xE0) {
      // 3バイト文字
      i += 3;
    } else if ((byte & 0xF8) == 0xF0) {
      // 4バイト文字
      i += 4;
    } else {
      // 不正なUTF-8シーケンス、1バイト進めて続行
      i += 1;
    }

    count++;
  }

  return count;
}

}  // namespace core
}  // namespace aerojs