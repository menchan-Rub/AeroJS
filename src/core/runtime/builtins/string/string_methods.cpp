/**
 * @file string_methods.cpp
 * @brief JavaScript の String オブジェクトのメソッド実装（基本操作）
 */

#include <algorithm>
#include <codecvt>
#include <locale>
#include <sstream>

#include "../../../function.h"
#include "../../../object.h"
#include "../../../value.h"
#include "string.h"

namespace aerojs {
namespace core {

// this 値から文字列を取得するユーティリティ関数
static std::string getStringFromThis(const std::vector<ValuePtr>& arguments) {
  // this 値が undefined または null の場合はエラー
  if (arguments.empty() || !arguments[0] || arguments[0]->isUndefined() || arguments[0]->isNull()) {
    throw std::runtime_error("String.prototype method called on null or undefined");
  }

  // this 値を文字列に変換
  if (arguments[0]->isObject() && arguments[0]->toObject()->isStringObject()) {
    // StringObject の場合はプリミティブ値を取得
    auto stringObj = std::static_pointer_cast<StringObject>(arguments[0]->toObject());
    return stringObj->primitiveValue();
  }

  // その他の値は toString() で変換
  return arguments[0]->toString();
}

// 引数を数値インデックスに変換するユーティリティ関数
static int toIndex(ValuePtr value, int defaultValue, int max) {
  if (!value || value->isUndefined()) {
    return defaultValue;
  }

  double index = value->toNumber();

  if (std::isnan(index) || index == 0) {
    return 0;
  }

  // 符号から整数部分を計算
  int intIndex = static_cast<int>(index);
  if (index < 0) {
    intIndex = std::max(0, max + intIndex);
  } else if (intIndex > max) {
    intIndex = max;
  }

  return intIndex;
}

// String.prototype.charAt()
ValuePtr String::charAt(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // インデックスを取得（デフォルトは0）
    int index = 0;
    if (arguments.size() > 1 && arguments[1]) {
      index = static_cast<int>(arguments[1]->toNumber());

      // 範囲外のインデックスは空文字列を返す
      if (index < 0 || index >= static_cast<int>(thisStr.length())) {
        return Value::fromString("");
      }
    }

    // 指定されたインデックスの文字を返す
    if (index < static_cast<int>(thisStr.length())) {
      return Value::fromString(thisStr.substr(index, 1));
    }

    return Value::fromString("");
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.charAt: ") + e.what());
  }
}

// String.prototype.charCodeAt()
ValuePtr String::charCodeAt(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // インデックスを取得（デフォルトは0）
    int index = 0;
    if (arguments.size() > 1 && arguments[1]) {
      index = static_cast<int>(arguments[1]->toNumber());

      // 範囲外のインデックスはNaNを返す
      if (index < 0 || index >= static_cast<int>(thisStr.length())) {
        return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
      }
    }

    // 指定されたインデックスの文字のUTF-16コードを返す
    if (index < static_cast<int>(thisStr.length())) {
      unsigned char c = thisStr[index];
      return Value::fromNumber(static_cast<double>(c));
    }

    return Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.charCodeAt: ") + e.what());
  }
}

// String.prototype.codePointAt()
ValuePtr String::codePointAt(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // インデックスを取得（デフォルトは0）
    int index = 0;
    if (arguments.size() > 1 && arguments[1]) {
      index = static_cast<int>(arguments[1]->toNumber());

      // 範囲外のインデックスはundefinedを返す
      if (index < 0 || index >= static_cast<int>(thisStr.length())) {
        return Value::undefined();
      }
    }

    // 指定されたインデックスのコードポイントを返す
    if (index < static_cast<int>(thisStr.length())) {
      // UTF-8文字列からコードポイントを正確に抽出
      size_t i = index;
      uint32_t codePoint = 0;

      // UTF-8シーケンスの先頭バイトを解析
      unsigned char firstByte = static_cast<unsigned char>(thisStr[i]);

      // 1バイト文字 (ASCII): 0xxxxxxx
      if ((firstByte & 0x80) == 0) {
        codePoint = firstByte;
      }
      // 2バイト文字: 110xxxxx 10xxxxxx
      else if ((firstByte & 0xE0) == 0xC0 && i + 1 < thisStr.length()) {
        unsigned char secondByte = static_cast<unsigned char>(thisStr[i + 1]);
        if ((secondByte & 0xC0) == 0x80) {
          codePoint = ((firstByte & 0x1F) << 6) | (secondByte & 0x3F);
        } else {
          // 無効なUTF-8シーケンス、最初のバイトのみ使用
          codePoint = firstByte;
        }
      }
      // 3バイト文字: 1110xxxx 10xxxxxx 10xxxxxx
      else if ((firstByte & 0xF0) == 0xE0 && i + 2 < thisStr.length()) {
        unsigned char secondByte = static_cast<unsigned char>(thisStr[i + 1]);
        unsigned char thirdByte = static_cast<unsigned char>(thisStr[i + 2]);
        if ((secondByte & 0xC0) == 0x80 && (thirdByte & 0xC0) == 0x80) {
          codePoint = ((firstByte & 0x0F) << 12) |
                      ((secondByte & 0x3F) << 6) |
                      (thirdByte & 0x3F);
        } else {
          // 無効なUTF-8シーケンス、最初のバイトのみ使用
          codePoint = firstByte;
        }
      }
      // 4バイト文字: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
      else if ((firstByte & 0xF8) == 0xF0 && i + 3 < thisStr.length()) {
        unsigned char secondByte = static_cast<unsigned char>(thisStr[i + 1]);
        unsigned char thirdByte = static_cast<unsigned char>(thisStr[i + 2]);
        unsigned char fourthByte = static_cast<unsigned char>(thisStr[i + 3]);
        if ((secondByte & 0xC0) == 0x80 &&
            (thirdByte & 0xC0) == 0x80 &&
            (fourthByte & 0xC0) == 0x80) {
          codePoint = ((firstByte & 0x07) << 18) |
                      ((secondByte & 0x3F) << 12) |
                      ((thirdByte & 0x3F) << 6) |
                      (fourthByte & 0x3F);
        } else {
          // 無効なUTF-8シーケンス、最初のバイトのみ使用
          codePoint = firstByte;
        }
      }
      // 無効なUTF-8シーケンス
      else {
        codePoint = firstByte;
      }

      return Value::fromNumber(static_cast<double>(codePoint));
    }

    return Value::undefined();
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.codePointAt: ") + e.what());
  }
}

// String.prototype.concat()
ValuePtr String::concat(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // 引数がなければthisの値をそのまま返す
    if (arguments.size() <= 1) {
      return Value::fromString(thisStr);
    }

    // すべての引数を連結
    std::string result = thisStr;
    for (size_t i = 1; i < arguments.size(); ++i) {
      if (arguments[i]) {
        result += arguments[i]->toString();
      }
    }

    return Value::fromString(result);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.concat: ") + e.what());
  }
}

// String.prototype.endsWith()
ValuePtr String::endsWith(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // 検索文字列を取得
    if (arguments.size() <= 1 || !arguments[1]) {
      return Value::fromBoolean(true);  // 空文字列での検索は常にtrue
    }

    std::string searchString = arguments[1]->toString();

    // 検索位置を取得（デフォルトは文字列の長さ）
    int endPosition = static_cast<int>(thisStr.length());
    if (arguments.size() > 2 && arguments[2] && !arguments[2]->isUndefined()) {
      endPosition = static_cast<int>(arguments[2]->toNumber());
      if (endPosition < 0) {
        endPosition = 0;
      } else if (endPosition > static_cast<int>(thisStr.length())) {
        endPosition = static_cast<int>(thisStr.length());
      }
    }

    // 検索文字列が空の場合は常にtrue
    if (searchString.empty()) {
      return Value::fromBoolean(true);
    }

    // 検索文字列が対象文字列よりも長い場合はfalse
    if (searchString.length() > static_cast<size_t>(endPosition)) {
      return Value::fromBoolean(false);
    }

    // 部分文字列の末尾と検索文字列を比較
    std::string subStr = thisStr.substr(0, endPosition);
    return Value::fromBoolean(
        subStr.length() >= searchString.length() &&
        subStr.substr(subStr.length() - searchString.length()) == searchString);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.endsWith: ") + e.what());
  }
}

// String.prototype.includes()
ValuePtr String::includes(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // 検索文字列を取得
    if (arguments.size() <= 1 || !arguments[1]) {
      return Value::fromBoolean(true);  // 空文字列での検索は常にtrue
    }

    std::string searchString = arguments[1]->toString();

    // 検索開始位置を取得（デフォルトは0）
    int position = 0;
    if (arguments.size() > 2 && arguments[2] && !arguments[2]->isUndefined()) {
      position = static_cast<int>(arguments[2]->toNumber());
      if (position < 0) {
        position = 0;
      } else if (position > static_cast<int>(thisStr.length())) {
        position = static_cast<int>(thisStr.length());
      }
    }

    // 検索文字列が空の場合は常にtrue
    if (searchString.empty()) {
      return Value::fromBoolean(true);
    }

    // 部分文字列内で検索
    std::string subStr = thisStr.substr(position);
    return Value::fromBoolean(subStr.find(searchString) != std::string::npos);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.includes: ") + e.what());
  }
}

// String.prototype.indexOf()
ValuePtr String::indexOf(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // 検索文字列を取得
    std::string searchString = "";
    if (arguments.size() > 1 && arguments[1]) {
      searchString = arguments[1]->toString();
    }

    // 検索開始位置を取得（デフォルトは0）
    int position = 0;
    if (arguments.size() > 2 && arguments[2] && !arguments[2]->isUndefined()) {
      position = static_cast<int>(arguments[2]->toNumber());
      if (position < 0) {
        position = 0;
      } else if (position > static_cast<int>(thisStr.length())) {
        return Value::fromNumber(-1);
      }
    }

    // 文字列を検索
    size_t found = thisStr.find(searchString, position);
    if (found == std::string::npos) {
      return Value::fromNumber(-1);
    }

    return Value::fromNumber(static_cast<double>(found));
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.indexOf: ") + e.what());
  }
}

// String.prototype.lastIndexOf()
ValuePtr String::lastIndexOf(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // 検索文字列を取得
    std::string searchString = "";
    if (arguments.size() > 1 && arguments[1]) {
      searchString = arguments[1]->toString();
    }

    // 検索開始位置を取得（デフォルトは文字列の長さ）
    int position = static_cast<int>(thisStr.length());
    if (arguments.size() > 2 && arguments[2] && !arguments[2]->isUndefined()) {
      double pos = arguments[2]->toNumber();
      if (std::isnan(pos)) {
        position = static_cast<int>(thisStr.length());
      } else {
        position = static_cast<int>(pos);
        if (position < 0) {
          position = 0;
        } else if (position > static_cast<int>(thisStr.length())) {
          position = static_cast<int>(thisStr.length());
        }
      }
    }

    // 空の検索文字列の場合は特別処理
    if (searchString.empty()) {
      return Value::fromNumber(std::min(position, static_cast<int>(thisStr.length())));
    }

    // position以前の最後の出現位置を検索
    if (position + searchString.length() > thisStr.length()) {
      position = static_cast<int>(thisStr.length() - searchString.length() + 1);
    }

    for (int i = position; i >= 0; --i) {
      if (thisStr.substr(i, searchString.length()) == searchString) {
        return Value::fromNumber(i);
      }
    }

    return Value::fromNumber(-1);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.lastIndexOf: ") + e.what());
  }
}

// String.prototype.repeat()
ValuePtr String::repeat(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // 繰り返し回数を取得
    int count = 0;
    if (arguments.size() > 1 && arguments[1]) {
      double countValue = arguments[1]->toNumber();

      // 負の値やInfinityはエラー
      if (countValue < 0 || !std::isfinite(countValue)) {
        throw std::runtime_error("Invalid count value");
      }

      count = static_cast<int>(countValue);
    }

    // 0回または空文字列の場合は空文字列を返す
    if (count == 0 || thisStr.empty()) {
      return Value::fromString("");
    }

    // 文字列を繰り返し連結
    std::string result;
    result.reserve(thisStr.length() * count);
    for (int i = 0; i < count; ++i) {
      result += thisStr;
    }

    return Value::fromString(result);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.repeat: ") + e.what());
  }
}

// String.prototype.startsWith()
ValuePtr String::startsWith(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // 検索文字列を取得
    if (arguments.size() <= 1 || !arguments[1]) {
      return Value::fromBoolean(true);  // 空文字列での検索は常にtrue
    }

    std::string searchString = arguments[1]->toString();

    // 検索開始位置を取得（デフォルトは0）
    int position = 0;
    if (arguments.size() > 2 && arguments[2] && !arguments[2]->isUndefined()) {
      position = static_cast<int>(arguments[2]->toNumber());
      if (position < 0) {
        position = 0;
      } else if (position > static_cast<int>(thisStr.length())) {
        return Value::fromBoolean(false);
      }
    }

    // 検索文字列が空の場合は常にtrue
    if (searchString.empty()) {
      return Value::fromBoolean(true);
    }

    // 部分文字列の先頭と検索文字列を比較
    if (position + searchString.length() > thisStr.length()) {
      return Value::fromBoolean(false);
    }

    return Value::fromBoolean(thisStr.substr(position, searchString.length()) == searchString);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.startsWith: ") + e.what());
  }
}

}  // namespace core
}  // namespace aerojs