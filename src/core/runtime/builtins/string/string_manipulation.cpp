/**
 * @file string_manipulation.cpp
 * @brief JavaScript の String オブジェクトのメソッド実装（変換・加工操作）
 */

#include "string.h"
#include "../../../object.h"
#include "../../../value.h"
#include "../../../function.h"
#include <algorithm>
#include <locale>
#include <cctype>

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

// String.prototype.slice()
ValuePtr String::slice(const std::vector<ValuePtr>& arguments) {
    try {
        std::string thisStr = getStringFromThis(arguments);
        
        // 引数がなければthisの値をそのまま返す
        if (arguments.size() <= 1) {
            return Value::fromString(thisStr);
        }
        
        int length = static_cast<int>(thisStr.length());
        
        // 開始位置を取得
        int start = static_cast<int>(arguments[1]->toNumber());
        if (start < 0) {
            start = std::max(length + start, 0);
        } else {
            start = std::min(start, length);
        }
        
        // 終了位置を取得（デフォルトは文字列の長さ）
        int end = length;
        if (arguments.size() > 2 && arguments[2] && !arguments[2]->isUndefined()) {
            end = static_cast<int>(arguments[2]->toNumber());
            if (end < 0) {
                end = std::max(length + end, 0);
            } else {
                end = std::min(end, length);
            }
        }
        
        // 終了位置が開始位置より前なら空文字列を返す
        if (end <= start) {
            return Value::fromString("");
        }
        
        // 部分文字列を返す
        return Value::fromString(thisStr.substr(start, end - start));
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("String.prototype.slice: ") + e.what());
    }
}

// String.prototype.substring()
ValuePtr String::substring(const std::vector<ValuePtr>& arguments) {
    try {
        std::string thisStr = getStringFromThis(arguments);
        
        // 引数がなければthisの値をそのまま返す
        if (arguments.size() <= 1) {
            return Value::fromString(thisStr);
        }
        
        int length = static_cast<int>(thisStr.length());
        
        // 開始位置を取得
        int start = static_cast<int>(arguments[1]->toNumber());
        if (std::isnan(start) || start < 0) {
            start = 0;
        } else {
            start = std::min(start, length);
        }
        
        // 終了位置を取得（デフォルトは文字列の長さ）
        int end = length;
        if (arguments.size() > 2 && arguments[2] && !arguments[2]->isUndefined()) {
            end = static_cast<int>(arguments[2]->toNumber());
            if (std::isnan(end) || end < 0) {
                end = 0;
            } else {
                end = std::min(end, length);
            }
        }
        
        // スタートとエンドを入れ替える（subsstringの特殊処理）
        if (start > end) {
            std::swap(start, end);
        }
        
        // 部分文字列を返す
        return Value::fromString(thisStr.substr(start, end - start));
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("String.prototype.substring: ") + e.what());
    }
}

// String.prototype.toLowerCase()
ValuePtr String::toLowerCase(const std::vector<ValuePtr>& arguments) {
    try {
        std::string thisStr = getStringFromThis(arguments);
        
        // 空文字列はそのまま返す
        if (thisStr.empty()) {
            return Value::fromString("");
        }
        
        // 文字列を小文字に変換
        std::string result = thisStr;
        std::transform(result.begin(), result.end(), result.begin(),
                      [](unsigned char c){ return std::tolower(c); });
        
        return Value::fromString(result);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("String.prototype.toLowerCase: ") + e.what());
    }
}

// String.prototype.toUpperCase()
ValuePtr String::toUpperCase(const std::vector<ValuePtr>& arguments) {
    try {
        std::string thisStr = getStringFromThis(arguments);
        
        // 空文字列はそのまま返す
        if (thisStr.empty()) {
            return Value::fromString("");
        }
        
        // 文字列を大文字に変換
        std::string result = thisStr;
        std::transform(result.begin(), result.end(), result.begin(),
                      [](unsigned char c){ return std::toupper(c); });
        
        return Value::fromString(result);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("String.prototype.toUpperCase: ") + e.what());
    }
}

// String.prototype.trim()
ValuePtr String::trim(const std::vector<ValuePtr>& arguments) {
    try {
        std::string thisStr = getStringFromThis(arguments);
        
        // 空文字列はそのまま返す
        if (thisStr.empty()) {
            return Value::fromString("");
        }
        
        // 前後の空白を削除
        std::string result = thisStr;
        
        // 先頭の空白を削除
        result.erase(result.begin(), std::find_if(result.begin(), result.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        
        // 末尾の空白を削除
        result.erase(std::find_if(result.rbegin(), result.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), result.end());
        
        return Value::fromString(result);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("String.prototype.trim: ") + e.what());
    }
}

// String.prototype.trimStart()
ValuePtr String::trimStart(const std::vector<ValuePtr>& arguments) {
    try {
        std::string thisStr = getStringFromThis(arguments);
        
        // 空文字列はそのまま返す
        if (thisStr.empty()) {
            return Value::fromString("");
        }
        
        // 先頭の空白を削除
        std::string result = thisStr;
        result.erase(result.begin(), std::find_if(result.begin(), result.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        
        return Value::fromString(result);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("String.prototype.trimStart: ") + e.what());
    }
}

// String.prototype.trimEnd()
ValuePtr String::trimEnd(const std::vector<ValuePtr>& arguments) {
    try {
        std::string thisStr = getStringFromThis(arguments);
        
        // 空文字列はそのまま返す
        if (thisStr.empty()) {
            return Value::fromString("");
        }
        
        // 末尾の空白を削除
        std::string result = thisStr;
        result.erase(std::find_if(result.rbegin(), result.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), result.end());
        
        return Value::fromString(result);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("String.prototype.trimEnd: ") + e.what());
    }
}

// String.prototype.padStart()
ValuePtr String::padStart(const std::vector<ValuePtr>& arguments) {
    try {
        std::string thisStr = getStringFromThis(arguments);
        
        // パディング長を取得
        if (arguments.size() <= 1 || !arguments[1]) {
            return Value::fromString(thisStr);
        }
        
        int maxLength = static_cast<int>(arguments[1]->toNumber());
        if (maxLength <= static_cast<int>(thisStr.length())) {
            return Value::fromString(thisStr);
        }
        
        // パディング文字列を取得（デフォルトは空白）
        std::string padString = " ";
        if (arguments.size() > 2 && arguments[2] && !arguments[2]->isUndefined()) {
            padString = arguments[2]->toString();
            if (padString.empty()) {
                padString = " ";
            }
        }
        
        // パディングを適用
        int padLength = maxLength - static_cast<int>(thisStr.length());
        std::string padding;
        
        while (padding.length() < static_cast<size_t>(padLength)) {
            padding += padString;
        }
        
        // 余分な部分をカット
        if (padding.length() > static_cast<size_t>(padLength)) {
            padding = padding.substr(0, padLength);
        }
        
        return Value::fromString(padding + thisStr);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("String.prototype.padStart: ") + e.what());
    }
}

// String.prototype.padEnd()
ValuePtr String::padEnd(const std::vector<ValuePtr>& arguments) {
    try {
        std::string thisStr = getStringFromThis(arguments);
        
        // パディング長を取得
        if (arguments.size() <= 1 || !arguments[1]) {
            return Value::fromString(thisStr);
        }
        
        int maxLength = static_cast<int>(arguments[1]->toNumber());
        if (maxLength <= static_cast<int>(thisStr.length())) {
            return Value::fromString(thisStr);
        }
        
        // パディング文字列を取得（デフォルトは空白）
        std::string padString = " ";
        if (arguments.size() > 2 && arguments[2] && !arguments[2]->isUndefined()) {
            padString = arguments[2]->toString();
            if (padString.empty()) {
                padString = " ";
            }
        }
        
        // パディングを適用
        int padLength = maxLength - static_cast<int>(thisStr.length());
        std::string padding;
        
        while (padding.length() < static_cast<size_t>(padLength)) {
            padding += padString;
        }
        
        // 余分な部分をカット
        if (padding.length() > static_cast<size_t>(padLength)) {
            padding = padding.substr(0, padLength);
        }
        
        return Value::fromString(thisStr + padding);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("String.prototype.padEnd: ") + e.what());
    }
}

// String.prototype.localeCompare()
ValuePtr String::localeCompare(const std::vector<ValuePtr>& arguments) {
    try {
        std::string thisStr = getStringFromThis(arguments);
        
        // 比較対象の文字列を取得
        std::string compareString = "";
        if (arguments.size() > 1 && arguments[1]) {
            compareString = arguments[1]->toString();
        }
        
        // 単純な辞書順比較を実装（より高度な実装はICUなどのライブラリを使用すべき）
        int result = thisStr.compare(compareString);
        
        if (result < 0) {
            return Value::fromNumber(-1);
        } else if (result > 0) {
            return Value::fromNumber(1);
        } else {
            return Value::fromNumber(0);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("String.prototype.localeCompare: ") + e.what());
    }
}

// String.prototype.toLocaleLowerCase()
ValuePtr String::toLocaleLowerCase(const std::vector<ValuePtr>& arguments) {
    try {
        std::string thisStr = getStringFromThis(arguments);
        
        // ロケール情報を取得（オプショナル引数）
        std::string locale = "en-US";  // デフォルトロケール
        if (arguments.size() > 1 && arguments[1] && !arguments[1]->isUndefined()) {
            locale = arguments[1]->toString();
        }
        
        // ICUライブラリを使用してロケール対応の小文字変換を実行
        UErrorCode status = U_ZERO_ERROR;
        icu::Locale icuLocale(locale.c_str());
        icu::UnicodeString uniStr(thisStr.c_str(), "UTF-8");
        
        // ロケール固有の小文字変換を実行
        uniStr.toLower(icuLocale);
        
        // 結果をUTF-8文字列に変換
        std::string result;
        uniStr.toUTF8String(result);
        
        return Value::fromString(result);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("String.prototype.toLocaleLowerCase: ") + e.what());
    }
}

// String.prototype.toLocaleUpperCase()
ValuePtr String::toLocaleUpperCase(const std::vector<ValuePtr>& arguments) {
    try {
        std::string thisStr = getStringFromThis(arguments);
        
        // ロケール情報を取得（オプショナル引数）
        std::string locale = "en-US";  // デフォルトロケール
        if (arguments.size() > 1 && arguments[1] && !arguments[1]->isUndefined()) {
            locale = arguments[1]->toString();
        }
        
        // ICUライブラリを使用してロケール対応の大文字変換を実行
        UErrorCode status = U_ZERO_ERROR;
        icu::Locale icuLocale(locale.c_str());
        icu::UnicodeString uniStr(thisStr.c_str(), "UTF-8");
        
        // ロケール固有の大文字変換を実行
        uniStr.toUpper(icuLocale);
        
        // 結果をUTF-8文字列に変換
        std::string result;
        uniStr.toUTF8String(result);
        
        return Value::fromString(result);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("String.prototype.toLocaleUpperCase: ") + e.what());
    }
}

} // namespace core
} // namespace aerojs