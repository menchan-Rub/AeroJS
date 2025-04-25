/**
 * @file string_static.cpp
 * @brief JavaScript の String オブジェクトの静的メソッドとその他のメソッド実装
 */

#include <algorithm>
#include <vector>

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

// String.fromCharCode()
ValuePtr String::fromCharCode(const std::vector<ValuePtr>& arguments) {
  // 引数がなければ空文字列を返す
  if (arguments.size() <= 1) {
    return Value::fromString("");
  }

  std::u16string utf16Result;
  utf16Result.reserve(arguments.size() - 1);

  // 各引数を文字コードとして処理
  for (size_t i = 1; i < arguments.size(); ++i) {
    if (arguments[i]) {
      // 数値に変換して16ビットに制限（ECMAScript仕様に準拠）
      uint16_t charCode = static_cast<uint16_t>(arguments[i]->toNumber()) & 0xFFFF;
      utf16Result.push_back(charCode);
    }
  }

  // UTF-16からUTF-8への変換
  std::string result;
  result.reserve(utf16Result.length() * 3);  // 最悪の場合、各文字が3バイトになる

  for (char16_t c : utf16Result) {
    if (c <= 0x7F) {
      // ASCII範囲
      result.push_back(static_cast<char>(c));
    } else if (c <= 0x7FF) {
      // 2バイト文字
      result.push_back(static_cast<char>(0xC0 | (c >> 6)));
      result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else {
      // 3バイト文字
      result.push_back(static_cast<char>(0xE0 | (c >> 12)));
      result.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
  }

  return Value::fromString(result);
}

// String.fromCodePoint()
ValuePtr String::fromCodePoint(const std::vector<ValuePtr>& arguments) {
  // 引数がなければ空文字列を返す
  if (arguments.size() <= 1) {
    return Value::fromString("");
  }

  std::u16string utf16Result;
  utf16Result.reserve(arguments.size() - 1);

  // 各引数をコードポイントとして処理
  for (size_t i = 1; i < arguments.size(); ++i) {
    if (arguments[i]) {
      double codePoint = arguments[i]->toNumber();

      // コードポイントの範囲チェック (ECMAScript仕様に準拠)
      if (codePoint < 0 || codePoint > 0x10FFFF || std::floor(codePoint) != codePoint) {
        throw RuntimeError("Invalid code point: code points must be non-negative integers less than or equal to 0x10FFFF");
      }

      uint32_t cp = static_cast<uint32_t>(codePoint);

      // サロゲートペア処理
      if (cp < 0x10000) {
        // BMP (Basic Multilingual Plane) 内の文字
        utf16Result.push_back(static_cast<char16_t>(cp));
      } else {
        // サロゲートペアに変換
        cp -= 0x10000;
        char16_t highSurrogate = static_cast<char16_t>(0xD800 + (cp >> 10));
        char16_t lowSurrogate = static_cast<char16_t>(0xDC00 + (cp & 0x3FF));
        utf16Result.push_back(highSurrogate);
        utf16Result.push_back(lowSurrogate);
      }
    }
  }

  // UTF-16からUTF-8への変換
  std::string result;
  result.reserve(utf16Result.length() * 3);  // 最悪の場合、各文字が3バイトになる

  for (size_t i = 0; i < utf16Result.length(); ++i) {
    char16_t c = utf16Result[i];

    if (c <= 0x7F) {
      // ASCII範囲
      result.push_back(static_cast<char>(c));
    } else if (c <= 0x7FF) {
      // 2バイト文字
      result.push_back(static_cast<char>(0xC0 | (c >> 6)));
      result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else if (c >= 0xD800 && c <= 0xDBFF && i + 1 < utf16Result.length() &&
               utf16Result[i + 1] >= 0xDC00 && utf16Result[i + 1] <= 0xDFFF) {
      // サロゲートペア (4バイト文字)
      char16_t highSurrogate = c;
      char16_t lowSurrogate = utf16Result[++i];

      // サロゲートペアからコードポイントを復元
      uint32_t codePoint = 0x10000 + ((highSurrogate - 0xD800) << 10) + (lowSurrogate - 0xDC00);

      // 4バイトUTF-8エンコーディング
      result.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
      result.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
      // 3バイト文字 (または無効なサロゲート)
      result.push_back(static_cast<char>(0xE0 | (c >> 12)));
      result.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
  }

  return Value::fromString(result);
}

// String.raw()
ValuePtr String::raw(const std::vector<ValuePtr>& arguments) {
  // 引数がなければ空文字列を返す
  if (arguments.size() <= 1 || !arguments[1] || !arguments[1]->isObject()) {
    return Value::fromString("");
  }

  ObjectPtr template_ = arguments[1]->toObject();

  // raw配列を取得
  ValuePtr rawValue = template_->get("raw");
  if (!rawValue || !rawValue->isObject()) {
    throw std::runtime_error("String.raw: 'raw' property must be an array-like object");
  }

  ObjectPtr raw = rawValue->toObject();

  // rawの長さを取得
  ValuePtr lengthValue = raw->get("length");
  if (!lengthValue) {
    return Value::fromString("");
  }

  int length = static_cast<int>(lengthValue->toNumber());
  if (length <= 0) {
    return Value::fromString("");
  }

  // テンプレート部分を順に連結
  std::string result;
  for (int i = 0; i < length; ++i) {
    // i番目のraw部分を追加
    std::string cooked = raw->get(std::to_string(i))->toString();
    result += cooked;

    // 次の置換値を追加（あれば）
    if (i < length - 1 && arguments.size() > i + 2 && arguments[i + 2]) {
      result += arguments[i + 2]->toString();
    }
  }

  return Value::fromString(result);
}

// String.prototype.split()
ValuePtr String::split(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // 分割方法を取得
    std::string separator = "";
    bool separatorIsEmpty = true;
    bool isSeparatorRegExp = false;
    ObjectPtr regexObj = nullptr;

    if (arguments.size() > 1 && arguments[1] && !arguments[1]->isUndefined()) {
      // レギュラーエクスプレッションの場合は特別処理
      if (arguments[1]->isObject() && arguments[1]->toObject()->isRegExp()) {
        isSeparatorRegExp = true;
        regexObj = arguments[1]->toObject();
      } else {
        separator = arguments[1]->toString();
        separatorIsEmpty = separator.empty();
      }
    }

    // 制限を取得
    uint32_t limit = std::numeric_limits<uint32_t>::max();
    if (arguments.size() > 2 && arguments[2] && !arguments[2]->isUndefined()) {
      double limitNum = arguments[2]->toNumber();
      if (std::isfinite(limitNum) && limitNum >= 0) {
        limit = static_cast<uint32_t>(std::min(limitNum, static_cast<double>(std::numeric_limits<uint32_t>::max())));
      }
    }

    // 空文字列の場合の特別処理
    if (thisStr.empty()) {
      if (separatorIsEmpty) {
        return Value::fromArray({});  // 空配列を返す
      } else {
        return Value::fromArray({Value::fromString("")});  // [""]を返す
      }
    }

    // 結果配列
    std::vector<ValuePtr> result;

    // RegExpの場合
    if (isSeparatorRegExp) {
      if (limit == 0) {
        return Value::fromArray({});
      }

      // RegExpを使った分割処理
      size_t lastIndex = 0;
      size_t startIndex = 0;

      while (result.size() < limit) {
        auto matchResult = RegExp::exec(regexObj, thisStr, startIndex);
        if (matchResult->isNull()) {
          break;
        }

        ObjectPtr matchObj = matchResult->toObject();
        size_t matchIndex = static_cast<size_t>(matchObj->get("index")->toNumber());
        std::string matchStr = matchObj->get("0")->toString();

        // マッチした位置が前回と同じで、マッチした文字列が空の場合は1文字進める
        if (matchIndex == lastIndex && matchStr.empty()) {
          startIndex = matchIndex + 1;
          if (startIndex > thisStr.length()) {
            break;
          }
          continue;
        }

        // マッチした位置までの部分文字列を結果に追加
        result.push_back(Value::fromString(thisStr.substr(lastIndex, matchIndex - lastIndex)));

        // キャプチャグループがある場合は追加
        size_t groupCount = matchObj->get("length")->toNumber();
        for (size_t i = 1; i < groupCount && result.size() < limit; i++) {
          ValuePtr groupValue = matchObj->get(std::to_string(i));
          if (groupValue && !groupValue->isUndefined()) {
            result.push_back(groupValue);
          } else {
            result.push_back(Value::fromString(""));
          }
        }

        lastIndex = matchIndex + matchStr.length();
        startIndex = lastIndex;
      }

      // 最後の部分を追加（制限内なら）
      if (result.size() < limit) {
        result.push_back(Value::fromString(thisStr.substr(lastIndex)));
      }
    }
    // 空区切り文字の場合は1文字ずつ分割
    else if (separatorIsEmpty) {
      for (size_t i = 0; i < thisStr.length() && result.size() < limit; ++i) {
        // UTF-8対応のために、コードポイントごとに分割する必要がある
        size_t charLen = 1;
        if ((thisStr[i] & 0xC0) == 0xC0) {
          // マルチバイト文字の先頭バイトを検出
          if ((thisStr[i] & 0xE0) == 0xC0)
            charLen = 2;  // 2バイト文字
          else if ((thisStr[i] & 0xF0) == 0xE0)
            charLen = 3;  // 3バイト文字
          else if ((thisStr[i] & 0xF8) == 0xF0)
            charLen = 4;  // 4バイト文字
        }

        result.push_back(Value::fromString(thisStr.substr(i, charLen)));

        // マルチバイト文字の場合はインデックスを調整
        if (charLen > 1) {
          i += (charLen - 1);
        }
      }
    }
    // 通常の文字列分割
    else {
      size_t startPos = 0;
      size_t foundPos = thisStr.find(separator);

      while (foundPos != std::string::npos && result.size() < limit) {
        result.push_back(Value::fromString(thisStr.substr(startPos, foundPos - startPos)));
        startPos = foundPos + separator.length();
        foundPos = thisStr.find(separator, startPos);
      }

      // 最後の部分を追加（制限内なら）
      if (result.size() < limit) {
        result.push_back(Value::fromString(thisStr.substr(startPos)));
      }
    }

    return Value::fromArray(result);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.split: ") + e.what());
  }
}

// String.prototype.match()
ValuePtr String::match(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // 引数がなければnullを返す
    if (arguments.size() <= 1 || !arguments[1]) {
      return Value::null();
    }

    // 引数がRegExpでなければ、RegExpに変換
    ObjectPtr regex;
    if (arguments[1]->isObject() && arguments[1]->toObject()->isRegExp()) {
      regex = arguments[1]->toObject();
    } else {
      // 文字列やその他の値からRegExpオブジェクトを作成
      std::string pattern = arguments[1]->toString();
      regex = RegExp::create(pattern, "");
    }

    // グローバルフラグがあるかチェック
    bool global = regex->get("global")->toBoolean();

    if (!global) {
      // 非グローバルマッチの場合は最初のマッチのみ
      auto result = RegExp::exec(regex, thisStr);
      return result;
    } else {
      // グローバルマッチの場合は全マッチを配列で返す
      std::vector<ValuePtr> matches;
      regex->set("lastIndex", Value::fromNumber(0));

      while (true) {
        auto result = RegExp::exec(regex, thisStr);
        if (result->isNull()) {
          break;
        }

        // マッチした文字列を配列に追加
        matches.push_back(result->toObject()->get("0"));
      }

      return matches.empty() ? Value::null() : Value::fromArray(matches);
    }
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.match: ") + e.what());
  }
}

// String.prototype.matchAll()
ValuePtr String::matchAll(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // 引数がなければ型エラー
    if (arguments.size() <= 1 || !arguments[1]) {
      throw std::runtime_error("RegExp argument is required");
    }

    // RegExpオブジェクトを取得または作成
    ObjectPtr regex;
    if (arguments[1]->isObject() && arguments[1]->toObject()->isRegExp()) {
      // RegExpオブジェクトのコピーを作成（元のオブジェクトを変更しないため）
      std::string pattern = arguments[1]->toObject()->get("source")->toString();
      std::string flags = arguments[1]->toObject()->get("flags")->toString();

      // グローバルフラグが必要
      if (flags.find('g') == std::string::npos) {
        flags += "g";
      }

      regex = RegExp::create(pattern, flags);
    } else {
      // 文字列やその他の値からRegExpオブジェクトを作成（グローバルフラグ付き）
      std::string pattern = arguments[1]->toString();
      regex = RegExp::create(pattern, "g");
    }

    // イテレータオブジェクトを作成
    ObjectPtr iterator = Object::create();

    // クロージャで状態を保持
    struct IteratorState {
      std::string str;
      ObjectPtr regexp;
      bool done;
    };

    auto state = std::make_shared<IteratorState>();
    state->str = thisStr;
    state->regexp = regex;
    state->done = false;

    // next関数を実装
    auto nextFn = [state](const std::vector<ValuePtr>& args) -> ValuePtr {
      if (state->done) {
        return Object::createIteratorResult(Value::undefined(), true);
      }

      auto result = RegExp::exec(state->regexp, state->str);
      if (result->isNull()) {
        state->done = true;
        return Object::createIteratorResult(Value::undefined(), true);
      }

      return Object::createIteratorResult(result, false);
    };

    // イテレータオブジェクトにnextメソッドを設定
    iterator->set("next", Function::create(nextFn));

    // Symbol.iteratorメソッドを設定
    iterator->set(Symbol::iterator, Function::create([iterator](const std::vector<ValuePtr>&) {
                    return iterator;
                  }));

    return iterator;
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.matchAll: ") + e.what());
  }
}

// String.prototype.normalize()
ValuePtr String::normalize(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // 正規化形式を取得（デフォルトはNFC）
    std::string form = "NFC";
    if (arguments.size() > 1 && arguments[1] && !arguments[1]->isUndefined()) {
      form = arguments[1]->toString();
    }

    // 正規化形式の検証
    if (form != "NFC" && form != "NFD" && form != "NFKC" && form != "NFKD") {
      throw std::runtime_error("Invalid normalization form: " + form);
    }

    // ICUを使用してUnicode正規化を実行
    UErrorCode status = U_ZERO_ERROR;
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(thisStr);

    icu::Normalizer2 const* normalizer = nullptr;
    if (form == "NFC") {
      normalizer = icu::Normalizer2::getNFCInstance(status);
    } else if (form == "NFD") {
      normalizer = icu::Normalizer2::getNFDInstance(status);
    } else if (form == "NFKC") {
      normalizer = icu::Normalizer2::getNFKCInstance(status);
    } else if (form == "NFKD") {
      normalizer = icu::Normalizer2::getNFKDInstance(status);
    }

    if (U_FAILURE(status)) {
      throw std::runtime_error("Failed to get normalizer instance");
    }

    icu::UnicodeString normalized = normalizer->normalize(ustr, status);
    if (U_FAILURE(status)) {
      throw std::runtime_error("Normalization failed");
    }

    std::string result;
    normalized.toUTF8String(result);
    return Value::fromString(result);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.normalize: ") + e.what());
  }
}

// String.prototype.replace()
ValuePtr String::replace(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // 引数がなければそのまま返す
    if (arguments.size() <= 1 || !arguments[1]) {
      return Value::fromString(thisStr);
    }

    // 検索パターンと置換値を取得
    ValuePtr searchValue = arguments[1];
    ValuePtr replaceValue = arguments.size() > 2 ? arguments[2] : Value::fromString("");

    // パターンがRegExpの場合
    if (searchValue->isObject() && searchValue->toObject()->isRegExp()) {
      ObjectPtr regex = searchValue->toObject();
      return replaceWithRegExp(thisStr, regex, replaceValue);
    }

    // パターンが文字列の場合
    std::string searchString = searchValue->toString();

    // 検索文字列が見つからない場合はそのまま返す
    size_t pos = thisStr.find(searchString);
    if (pos == std::string::npos) {
      return Value::fromString(thisStr);
    }

    // 置換値が関数の場合
    if (replaceValue->isObject() && replaceValue->toObject()->isFunction()) {
      ObjectPtr replacerFn = replaceValue->toObject();

      // 関数を呼び出すための引数を準備
      std::vector<ValuePtr> fnArgs;
      fnArgs.push_back(Value::fromString(searchString));  // マッチした文字列
      fnArgs.push_back(Value::fromNumber(pos));           // マッチした位置
      fnArgs.push_back(Value::fromString(thisStr));       // 元の文字列

      // 関数を呼び出して置換値を取得
      ValuePtr result = Function::call(replacerFn, Value::undefined(), fnArgs);
      std::string replacement = result->toString();

      // 置換を実行
      std::string resultStr = thisStr;
      resultStr.replace(pos, searchString.length(), replacement);
      return Value::fromString(resultStr);
    }

    // 置換値が文字列の場合
    std::string replacement = replaceValue->toString();

    // $記号による特殊置換を処理
    replacement = processReplacementPattern(replacement, searchString, pos, thisStr);

    // 置換を実行
    std::string resultStr = thisStr;
    resultStr.replace(pos, searchString.length(), replacement);
    return Value::fromString(resultStr);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.replace: ") + e.what());
  }
}

// String.prototype.replaceAll()
ValuePtr String::replaceAll(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // 引数チェック
    if (arguments.size() <= 1 || !arguments[1]) {
      return Value::fromString(thisStr);
    }

    ValuePtr searchValue = arguments[1];
    ValuePtr replaceValue = arguments.size() > 2 && arguments[2] ? arguments[2] : Value::fromString("");

    // パターンがRegExpの場合
    if (searchValue->isObject() && searchValue->toObject()->isRegExp()) {
      ObjectPtr regexObj = searchValue->toObject();

      // グローバルフラグがない場合はエラー
      bool isGlobal = RegExp::getFlag(regexObj, "g");
      if (!isGlobal) {
        throw std::runtime_error("String.prototype.replaceAll called with a non-global RegExp argument");
      }

      return replaceWithRegExp(thisStr, regexObj, replaceValue, true);
    }

    // パターンが文字列の場合
    std::string searchString = searchValue->toString();

    // 置換値が関数の場合
    if (replaceValue->isObject() && replaceValue->toObject()->isFunction()) {
      ObjectPtr replacerFn = replaceValue->toObject();
      return replaceAllWithFunction(thisStr, searchString, replacerFn);
    }

    // 置換値が文字列の場合
    std::string replacement = replaceValue->toString();

    // すべての出現を置換
    std::string resultStr = thisStr;
    size_t startPos = 0;
    size_t pos;

    while ((pos = resultStr.find(searchString, startPos)) != std::string::npos) {
      // $記号による特殊置換を処理
      std::string processedReplacement = processReplacementPattern(replacement, searchString, pos, thisStr);

      // 置換を実行
      resultStr.replace(pos, searchString.length(), processedReplacement);
      startPos = pos + processedReplacement.length();
    }

    return Value::fromString(resultStr);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.replaceAll: ") + e.what());
  }
}

// 関数を使用した文字列の全置換を行う
ValuePtr String::replaceAllWithFunction(const std::string& str, const std::string& searchString, ObjectPtr replacerFn) {
  std::string resultStr = str;
  size_t startPos = 0;
  size_t pos;
  std::vector<size_t> positions;

  // まず全ての出現位置を記録（置換による位置ずれを避けるため）
  while ((pos = resultStr.find(searchString, startPos)) != std::string::npos) {
    positions.push_back(pos);
    startPos = pos + searchString.length();
  }

  // 後ろから置換していくことで位置ずれを回避
  for (auto it = positions.rbegin(); it != positions.rend(); ++it) {
    pos = *it;

    // 関数を呼び出すための引数を準備
    std::vector<ValuePtr> fnArgs;
    fnArgs.push_back(Value::fromString(searchString));  // マッチした文字列
    fnArgs.push_back(Value::fromNumber(pos));           // マッチした位置
    fnArgs.push_back(Value::fromString(str));           // 元の文字列

    // 関数を呼び出して置換値を取得
    ValuePtr result = Function::call(replacerFn, Value::undefined(), fnArgs);
    std::string replacement = result->toString();

    // 置換を実行
    resultStr.replace(pos, searchString.length(), replacement);
  }

  return Value::fromString(resultStr);
}

// String.prototype.search()
ValuePtr String::search(const std::vector<ValuePtr>& arguments) {
  try {
    std::string thisStr = getStringFromThis(arguments);

    // 引数がなければ空の正規表現で検索
    ValuePtr regexp = arguments.size() > 1 && arguments[1] ? arguments[1] : Value::fromString("");

    // 引数がRegExpでなければ、RegExpに変換
    ObjectPtr regexObj;
    if (regexp->isObject() && regexp->toObject()->isRegExp()) {
      regexObj = regexp->toObject();
    } else {
      // 新しいRegExpオブジェクトを作成
      std::vector<ValuePtr> regexpArgs = {regexp};
      regexObj = RegExp::constructor(regexpArgs);
    }

    // RegExpのexecメソッドを使用して検索
    ValuePtr execResult = RegExp::exec(regexObj, {Value::fromString(thisStr)});

    // 結果がnullの場合は-1を返す
    if (execResult->isNull()) {
      return Value::fromNumber(-1);
    }

    // マッチした位置を返す
    ObjectPtr resultArray = execResult->toObject();
    ValuePtr indexValue = Object::get(resultArray, "index");
    return indexValue;
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("String.prototype.search: ") + e.what());
  }
}

}  // namespace core
}  // namespace aerojs