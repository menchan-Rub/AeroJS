/**
 * @file string.cpp
 * @brief JavaScript文字列クラスの実装
 * @version 0.1.0
 * @license MIT
 */

#include "string.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>
#include <sstream>

namespace aerojs {
namespace core {

// 静的メンバの定義
std::unordered_map<std::string, String*> String::internedStrings_;
std::mutex String::internMutex_;

// コンストラクタ
String::String(const char* str)
    : length_(str ? strlen(str) : 0), 
      utf8Length_(0), 
      storageType_(StorageType::Small) {
  
  if (!str || length_ == 0) {
    small_.data[0] = '\0';
    utf8Length_ = 0;
    return;
  }

  // スモールストリング最適化
  if (length_ <= SMALL_STRING_MAX_SIZE) {
    storageType_ = StorageType::Small;
    memcpy(small_.data, str, length_);
    small_.data[length_] = '\0';
  } else {
    // 通常の文字列として保存
    storageType_ = StorageType::Normal;
    normal_.capacity = length_ * 2; // 余裕を持ったサイズ
    normal_.data = new char[normal_.capacity + 1];
    memcpy(normal_.data, str, length_);
    normal_.data[length_] = '\0';
  }

  utf8Length_ = calculateUtf8Length(str, length_);
}

String::String(const std::string& str) 
    : String(str.c_str()) {
}

String::String(const utils::StringView& view)
    : length_(view.length()), 
      utf8Length_(0), 
      storageType_(StorageType::Small) {
  
  if (length_ == 0) {
    small_.data[0] = '\0';
    utf8Length_ = 0;
    return;
  }

  // スモールストリング最適化
  if (length_ <= SMALL_STRING_MAX_SIZE) {
    storageType_ = StorageType::Small;
    memcpy(small_.data, view.data(), length_);
    small_.data[length_] = '\0';
  } else {
    // 通常の文字列として保存
    storageType_ = StorageType::Normal;
    normal_.capacity = length_ * 2;
    normal_.data = new char[normal_.capacity + 1];
    memcpy(normal_.data, view.data(), length_);
    normal_.data[length_] = '\0';
  }

  utf8Length_ = calculateUtf8Length(view.data(), length_);
}

String::String(const char* data, size_t length, StorageType type)
    : length_(length), utf8Length_(0), storageType_(type) {
  
  switch (type) {
    case StorageType::Small:
      assert(length <= SMALL_STRING_MAX_SIZE);
      if (data && length > 0) {
        memcpy(small_.data, data, length);
      }
      small_.data[length] = '\0';
      break;
      
    case StorageType::Normal:
      normal_.capacity = length * 2;
      normal_.data = new char[normal_.capacity + 1];
      if (data && length > 0) {
        memcpy(normal_.data, data, length);
      }
      normal_.data[length] = '\0';
      break;
      
    case StorageType::Static:
      static_.data = data;
      break;
      
    default:
      assert(false && "Invalid storage type for this constructor");
      break;
  }

  if (data) {
    utf8Length_ = calculateUtf8Length(data, length);
  }
}

String::String(String* source, size_t offset, size_t length)
    : length_(length), utf8Length_(0), storageType_(StorageType::Sliced) {
  
  assert(source != nullptr);
  assert(offset + length <= source->length());
  
  sliced_.source = source;
  sliced_.offset = offset;
  source->ref(); // 参照元を保持
  
  // UTF-8文字数の計算は必要に応じて行う
  utf8Length_ = calculateUtf8Length(c_str(), length);
}

String::String(String* left, String* right)
    : length_(left->length() + right->length()), 
      utf8Length_(left->utf8Length() + right->utf8Length()), 
      storageType_(StorageType::Concatenated) {
  
  assert(left != nullptr);
  assert(right != nullptr);
  
  concatenated_.left = left;
  concatenated_.right = right;
  left->ref();
  right->ref();
}

// デストラクタ
String::~String() {
  switch (storageType_) {
    case StorageType::Normal:
      delete[] normal_.data;
      break;
      
    case StorageType::Sliced:
      sliced_.source->unref();
      break;
      
    case StorageType::Concatenated:
      concatenated_.left->unref();
      concatenated_.right->unref();
      break;
      
    default:
      break;
  }
}

// C文字列に変換
const char* String::c_str() const {
  switch (storageType_) {
    case StorageType::Small:
      return small_.data;
      
    case StorageType::Normal:
      return normal_.data;
      
    case StorageType::Static:
      return static_.data;
      
    case StorageType::Sliced:
      // スライス文字列の場合、一時的なバッファを作成する必要がある
      // 実装簡化のため、文字列を実体化する
      return value().c_str();
      
    case StorageType::Concatenated:
      // 連結文字列の場合も同様
      return value().c_str();
      
    default:
      return "";
  }
}

// std::stringに変換
std::string String::value() const {
  switch (storageType_) {
    case StorageType::Small:
      return std::string(small_.data, length_);
      
    case StorageType::Normal:
      return std::string(normal_.data, length_);
      
    case StorageType::Static:
      return std::string(static_.data, length_);
      
    case StorageType::Sliced: {
      const char* sourceData = sliced_.source->c_str();
      return std::string(sourceData + sliced_.offset, length_);
    }
    
    case StorageType::Concatenated: {
      std::string result;
      result.reserve(length_);
      result += concatenated_.left->value();
      result += concatenated_.right->value();
      return result;
    }
    
    default:
      return "";
  }
}

// 文字列ビューを取得
utils::StringView String::view() const {
  // 実装簡化のため、文字列を実体化してビューを返す
  static thread_local std::string temp;
  temp = value();
  return utils::StringView(temp.data(), temp.length());
}

// 部分文字列を取得
String* String::substring(size_t start, size_t length) const {
  if (start >= length_ || length == 0) {
    return new String(); // 空文字列
  }
  
  size_t actualLength = std::min(length, length_ - start);
  
  // 小さい部分文字列の場合は新しい文字列として作成
  if (actualLength <= SMALL_STRING_MAX_SIZE) {
    std::string sub = value().substr(start, actualLength);
    return new String(sub);
  }
  
  // スライス文字列として作成
  return new String(const_cast<String*>(this), start, actualLength);
}

// 文字列を連結
String* String::concat(const String* other) const {
  if (!other || other->length() == 0) {
    const_cast<String*>(this)->ref();
    return const_cast<String*>(this);
  }
  
  if (length_ == 0) {
    const_cast<String*>(other)->ref();
    return const_cast<String*>(other);
  }
  
  // 小さい文字列の場合は直接連結
  size_t totalLength = length_ + other->length();
  if (totalLength <= SMALL_STRING_MAX_SIZE) {
    std::string result = value() + other->value();
    return new String(result);
  }
  
  // 連結文字列として作成
  return new String(const_cast<String*>(this), const_cast<String*>(other));
}

// 文字検索
size_t String::indexOf(char ch, size_t fromIndex) const {
  if (fromIndex >= length_) {
    return static_cast<size_t>(-1);
  }
  
  std::string str = value();
  size_t pos = str.find(ch, fromIndex);
  return (pos == std::string::npos) ? static_cast<size_t>(-1) : pos;
}

// 部分文字列検索
size_t String::indexOf(const String* str, size_t fromIndex) const {
  if (!str || str->length() == 0 || fromIndex >= length_) {
    return static_cast<size_t>(-1);
  }
  
  std::string thisStr = value();
  std::string searchStr = str->value();
  size_t pos = thisStr.find(searchStr, fromIndex);
  return (pos == std::string::npos) ? static_cast<size_t>(-1) : pos;
}

// 文字列が指定した文字で始まるかを確認
bool String::startsWith(char ch) const {
  if (length_ == 0) return false;
  
  switch (storageType_) {
    case StorageType::Small:
      return small_.data[0] == ch;
    case StorageType::Normal:
      return normal_.data[0] == ch;
    case StorageType::Static:
      return static_.data[0] == ch;
    default:
      return value()[0] == ch;
  }
}

// 文字列が指定した部分文字列で始まるかを確認
bool String::startsWith(const String* str) const {
  if (!str || str->length() > length_) {
    return false;
  }
  
  if (str->length() == 0) {
    return true;
  }
  
  std::string thisStr = value();
  std::string searchStr = str->value();
  return thisStr.substr(0, searchStr.length()) == searchStr;
}

// 文字列が指定した文字で終わるかを確認
bool String::endsWith(char ch) const {
  if (length_ == 0) return false;
  
  switch (storageType_) {
    case StorageType::Small:
      return small_.data[length_ - 1] == ch;
    case StorageType::Normal:
      return normal_.data[length_ - 1] == ch;
    case StorageType::Static:
      return static_.data[length_ - 1] == ch;
    default:
      std::string str = value();
      return str[str.length() - 1] == ch;
  }
}

// 文字列が指定した部分文字列で終わるかを確認
bool String::endsWith(const String* str) const {
  if (!str || str->length() > length_) {
    return false;
  }
  
  if (str->length() == 0) {
    return true;
  }
  
  std::string thisStr = value();
  std::string searchStr = str->value();
  return thisStr.substr(thisStr.length() - searchStr.length()) == searchStr;
}

// 等価性比較
bool String::equals(const String* other) const {
  if (!other) return false;
  if (this == other) return true;
  if (length_ != other->length_) return false;
  
  return value() == other->value();
}

// 大文字小文字を無視した比較
bool String::equalsIgnoreCase(const String* other) const {
  if (!other) return false;
  if (this == other) return true;
  if (length_ != other->length_) return false;
  
  std::string str1 = value();
  std::string str2 = other->value();
  
  std::transform(str1.begin(), str1.end(), str1.begin(), ::tolower);
  std::transform(str2.begin(), str2.end(), str2.begin(), ::tolower);
  
  return str1 == str2;
}

// 指定した位置の文字を取得
std::string String::charAt(size_t index) const {
  if (index >= length_) {
    return "";
  }
  
  std::string str = value();
  return std::string(1, str[index]);
}

// 指定した位置の文字コードを取得
uint32_t String::charCodeAt(size_t index) const {
  if (index >= length_) {
    return 0; // JavaScriptのcharCodeAtはNaNを返すが、ここでは0を返す
  }
  
  std::string str = value();
  return static_cast<uint32_t>(static_cast<unsigned char>(str[index]));
}

// 大文字に変換
String* String::toUpperCase() const {
  std::string str = value();
  std::transform(str.begin(), str.end(), str.begin(), ::toupper);
  return new String(str);
}

// 小文字に変換
String* String::toLowerCase() const {
  std::string str = value();
  std::transform(str.begin(), str.end(), str.begin(), ::tolower);
  return new String(str);
}

// ハッシュコードを取得
size_t String::hashCode() const {
  std::string str = value();
  return std::hash<std::string>{}(str);
}

// 静的文字列を作成
String* String::createStatic(const char* str) {
  if (!str) {
    return new String();
  }
  
  size_t length = strlen(str);
  String* result = new String(str, length, StorageType::Static);
  result->length_ = length;
  result->utf8Length_ = calculateUtf8Length(str, length);
  return result;
}

// インターン文字列を取得または作成
String* String::intern(const char* str) {
  std::lock_guard<std::mutex> lock(internMutex_);
  
  std::string key(str ? str : "");
  auto it = internedStrings_.find(key);
  if (it != internedStrings_.end()) {
    it->second->ref();
    return it->second;
  }
  
  String* newString = new String(str);
  internedStrings_[key] = newString;
  newString->ref(); // インターンプールで保持
  newString->ref(); // 戻り値として保持
  return newString;
}

String* String::intern(const std::string& str) {
  return intern(str.c_str());
}

// 文字列を作成
String* String::create(const char* str) {
  return new String(str);
}

String* String::create(const std::string& str) {
  return new String(str);
}

String* String::create(const utils::StringView& view) {
  return new String(view);
}

// UTF-8文字数を計算
size_t String::calculateUtf8Length(const char* data, size_t byteLength) {
  if (!data || byteLength == 0) {
    return 0;
  }
  
  size_t charCount = 0;
  size_t i = 0;
  
  while (i < byteLength) {
    unsigned char c = static_cast<unsigned char>(data[i]);
    
    if (c < 0x80) {
      // 1バイト文字
      i += 1;
    } else if ((c & 0xE0) == 0xC0) {
      // 2バイト文字
      i += 2;
    } else if ((c & 0xF0) == 0xE0) {
      // 3バイト文字
      i += 3;
    } else if ((c & 0xF8) == 0xF0) {
      // 4バイト文字
      i += 4;
    } else {
      // 無効なUTF-8シーケンス
      i += 1;
    }
    
    charCount++;
  }
  
  return charCount;
}

}  // namespace core
}  // namespace aerojs