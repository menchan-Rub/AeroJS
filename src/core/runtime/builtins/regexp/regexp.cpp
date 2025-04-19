/**
 * @file regexp.cpp
 * @brief JavaScript の正規表現オブジェクトの実装
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#include "regexp.h"
#include "core/runtime/execution_context.h"
#include "core/runtime/value.h"
#include "core/runtime/property_descriptor.h"
#include "core/runtime/symbol.h"
#include "core/runtime/error.h"
#include "core/runtime/array_object.h"
#include "core/runtime/string_object.h"
#include <regex>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace aero {

// 前方宣言
RegExpObject* createRegExpObject(ExecutionContext* ctx, const std::vector<Value>& args);

// RegExpObject の実装

RegExpObject::RegExpObject(const std::string& pattern, const std::string& flags)
    : Object(nullptr) // プロトタイプは後で設定される
    , m_pattern(pattern)
    , m_flags(flags)
    , m_lastIndex(0)
{
    // フラグの検証
    validateRegExp(pattern, flags);
    
    // 正規表現オブジェクトを作成
    try {
        auto options = createRegexOptions(flags);
        m_regex = std::regex(pattern, options);
    } catch (const std::regex_error& e) {
        // 無効な正規表現パターンの場合は例外を投げる
        throw std::invalid_argument("無効な正規表現パターンです: " + std::string(e.what()));
    }
    
    // プロパティの初期化
    defineProperty("lastIndex", PropertyDescriptor(Value(static_cast<double>(m_lastIndex)), false, true, true));
    defineProperty("source", PropertyDescriptor(Value(m_pattern), true, false, false));
    defineProperty("flags", PropertyDescriptor(Value(m_flags), true, false, false));
    
    // フラグプロパティを設定
    defineProperty("global", PropertyDescriptor(Value(global()), true, false, false));
    defineProperty("ignoreCase", PropertyDescriptor(Value(ignoreCase()), true, false, false));
    defineProperty("multiline", PropertyDescriptor(Value(multiline()), true, false, false));
    defineProperty("sticky", PropertyDescriptor(Value(sticky()), true, false, false));
    defineProperty("dotAll", PropertyDescriptor(Value(dotAll()), true, false, false));
    defineProperty("unicode", PropertyDescriptor(Value(unicode()), true, false, false));
}

RegExpObject::~RegExpObject() {
    // リソースのクリーンアップ（必要に応じて）
}

const std::string& RegExpObject::getPattern() const {
    return m_pattern;
}

const std::string& RegExpObject::getFlags() const {
    return m_flags;
}

Object* RegExpObject::exec(const std::string& str) {
    // グローバルまたはスティッキーフラグがある場合、lastIndexから検索を開始
    size_t startPos = (global() || sticky()) ? m_lastIndex : 0;
    
    // 検索範囲を超えている場合はnullを返す
    if (startPos > str.length()) {
        if (global() || sticky()) {
            setLastIndex(0);
        }
        return nullptr;
    }
    
    std::smatch matches;
    std::string searchStr = str;
    
    // 検索開始位置を設定
    bool found = false;
    if (startPos == 0) {
        found = std::regex_search(searchStr, matches, m_regex);
    } else {
        // 部分文字列を使用して検索
        std::string substr = searchStr.substr(startPos);
        found = std::regex_search(substr, matches, m_regex);
        
        // マッチングの位置を調整
        if (found) {
            for (auto& match : matches) {
                // オフセットを調整（C++の正規表現ライブラリは部分文字列に対するマッチング位置を返すため）
                match.first += startPos;
                match.second += startPos;
            }
        }
    }
    
    // マッチングしない場合
    if (!found || (sticky() && matches.position() != 0)) {
        if (global() || sticky()) {
            setLastIndex(0);
        }
        return nullptr;
    }
    
    // lastIndexを更新
    if (global() || sticky()) {
        setLastIndex(matches.position() + matches.length());
    }
    
    // マッチング結果を配列として返す
    auto* ctx = ExecutionContext::current();
    auto* result = ctx->createArray();
    
    // マッチした文字列を配列に追加
    for (size_t i = 0; i < matches.size(); ++i) {
        std::string matchStr = matches[i].str();
        result->defineProperty(std::to_string(i), PropertyDescriptor(Value(ctx->createString(matchStr)), true, true, true));
    }
    
    // インデックス情報を追加
    result->defineProperty("index", PropertyDescriptor(Value(static_cast<double>(matches.position())), true, true, true));
    result->defineProperty("input", PropertyDescriptor(Value(ctx->createString(str)), true, true, true));
    
    // グループ名があれば追加（C++17以降）
    if (!matches.empty()) {
        auto* groups = ctx->createObject();
        for (const auto& name : m_regex.mark_count()) {
            if (name > 0 && name < matches.size()) {
                std::string groupName = "group" + std::to_string(name);
                groups->defineProperty(groupName, PropertyDescriptor(Value(ctx->createString(matches[name].str())), true, true, true));
            }
        }
        result->defineProperty("groups", PropertyDescriptor(Value(groups), true, true, true));
    }
    
    return result;
}

bool RegExpObject::test(const std::string& str) {
    // execメソッドを呼び出し、結果がnullでなければtrueを返す
    return exec(str) != nullptr;
}

std::string RegExpObject::toString() const {
    std::ostringstream oss;
    oss << "/" << m_pattern << "/" << m_flags;
    return oss.str();
}

bool RegExpObject::global() const {
    return m_flags.find('g') != std::string::npos;
}

bool RegExpObject::ignoreCase() const {
    return m_flags.find('i') != std::string::npos;
}

bool RegExpObject::multiline() const {
    return m_flags.find('m') != std::string::npos;
}

bool RegExpObject::sticky() const {
    return m_flags.find('y') != std::string::npos;
}

bool RegExpObject::dotAll() const {
    return m_flags.find('s') != std::string::npos;
}

bool RegExpObject::unicode() const {
    return m_flags.find('u') != std::string::npos;
}

size_t RegExpObject::lastIndex() const {
    return m_lastIndex;
}

void RegExpObject::setLastIndex(size_t index) {
    m_lastIndex = index;
    // プロパティも更新
    defineProperty("lastIndex", PropertyDescriptor(Value(static_cast<double>(m_lastIndex)), false, true, true));
}

std::regex::flag_type RegExpObject::createRegexOptions(const std::string& flags) {
    std::regex::flag_type options = std::regex::ECMAScript;
    
    // フラグに基づいてオプションを設定
    if (flags.find('i') != std::string::npos) {
        options |= std::regex::icase;
    }
    if (flags.find('m') != std::string::npos) {
        options |= std::regex::multiline;
    }
    if (flags.find('s') != std::string::npos) {
        // C++の正規表現では、ECMAScriptモードでのdotAllフラグは直接サポートされていないため、
        // パターンを書き換える必要がある場合がある。現在の実装ではそのまま使用。
    }
    
    return options;
}

void RegExpObject::validateRegExp(const std::string& pattern, const std::string& flags) {
    // フラグの重複チェック
    std::string uniqueFlags;
    for (char flag : flags) {
        if (uniqueFlags.find(flag) != std::string::npos) {
            throw std::invalid_argument("正規表現フラグが重複しています: " + std::string(1, flag));
        }
        
        // 有効なフラグかチェック
        if (std::string("gimysu").find(flag) == std::string::npos) {
            throw std::invalid_argument("無効な正規表現フラグです: " + std::string(1, flag));
        }
        
        uniqueFlags += flag;
    }
    
    // パターンの構文チェックは、std::regexのコンストラクタで行われる
}

// RegExp関連の関数の実装

Value regexpConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // thisValueがRegExpオブジェクトでない場合、新しいRegExpオブジェクトを作成
    if (!thisValue.isObject() || !thisValue.asObject()->isRegExp()) {
        auto* proto = ctx->regexpPrototype();
        auto* regexp = createRegExpObject(ctx, args);
        regexp->setPrototype(proto);
        return Value(regexp);
    }
    
    // thisValueがRegExpオブジェクトの場合、引数に応じて処理
    auto* regexp = static_cast<RegExpObject*>(thisValue.asObject());
    
    if (args.empty()) {
        // 引数がない場合は空のパターンで作成
        return Value(new RegExpObject("", ""));
    }
    
    if (args[0].isObject() && args[0].asObject()->isRegExp()) {
        // 最初の引数がRegExpオブジェクトの場合
        auto* sourceRegExp = static_cast<RegExpObject*>(args[0].asObject());
        std::string pattern = sourceRegExp->getPattern();
        std::string flags = args.size() > 1 ? args[1].toString()->value() : sourceRegExp->getFlags();
        
        return Value(new RegExpObject(pattern, flags));
    }
    
    // 通常のパターンと（オプションの）フラグで作成
    std::string pattern = args[0].isUndefined() ? "" : args[0].toString()->value();
    std::string flags = args.size() > 1 ? args[1].toString()->value() : "";
    
    return Value(new RegExpObject(pattern, flags));
}

// ヘルパー関数: RegExpオブジェクトの作成
RegExpObject* createRegExpObject(ExecutionContext* ctx, const std::vector<Value>& args) {
    if (args.empty()) {
        // 引数がない場合は空のパターンで作成
        return new RegExpObject("", "");
    }
    
    if (args[0].isObject() && args[0].asObject()->isRegExp()) {
        // 最初の引数がRegExpオブジェクトの場合
        auto* sourceRegExp = static_cast<RegExpObject*>(args[0].asObject());
        std::string pattern = sourceRegExp->getPattern();
        std::string flags = args.size() > 1 ? args[1].toString()->value() : sourceRegExp->getFlags();
        
        return new RegExpObject(pattern, flags);
    }
    
    // 通常のパターンと（オプションの）フラグで作成
    std::string pattern = args[0].isUndefined() ? "" : args[0].toString()->value();
    std::string flags = args.size() > 1 ? args[1].toString()->value() : "";
    
    return new RegExpObject(pattern, flags);
}

Value regexpExec(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // thisValueがRegExpオブジェクトでない場合はエラー
    if (!thisValue.isObject() || !thisValue.asObject()->isRegExp()) {
        return ctx->throwTypeError("RegExp.prototype.exec called on incompatible receiver");
    }
    
    auto* regexp = static_cast<RegExpObject*>(thisValue.asObject());
    
    // 引数がない場合はundefinedをデフォルト値として使用
    std::string str = args.empty() || args[0].isUndefined() ? "undefined" : args[0].toString()->value();
    
    // execを実行
    auto* result = regexp->exec(str);
    
    // 結果がnullの場合はnullを返す
    if (!result) {
        return Value::null();
    }
    
    return Value(result);
}

Value regexpTest(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // thisValueがRegExpオブジェクトでない場合はエラー
    if (!thisValue.isObject() || !thisValue.asObject()->isRegExp()) {
        return ctx->throwTypeError("RegExp.prototype.test called on incompatible receiver");
    }
    
    auto* regexp = static_cast<RegExpObject*>(thisValue.asObject());
    
    // 引数がない場合はundefinedをデフォルト値として使用
    std::string str = args.empty() || args[0].isUndefined() ? "undefined" : args[0].toString()->value();
    
    // testを実行
    bool result = regexp->test(str);
    
    return Value(result);
}

Value regexpToString(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // thisValueがRegExpオブジェクトでない場合はエラー
    if (!thisValue.isObject() || !thisValue.asObject()->isRegExp()) {
        return ctx->throwTypeError("RegExp.prototype.toString called on incompatible receiver");
    }
    
    auto* regexp = static_cast<RegExpObject*>(thisValue.asObject());
    
    // toStringを実行
    std::string result = regexp->toString();
    
    return Value(ctx->createString(result));
}

Value regexpMatch(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // thisValueがRegExpオブジェクトでない場合はエラー
    if (!thisValue.isObject() || !thisValue.asObject()->isRegExp()) {
        return ctx->throwTypeError("RegExp.prototype[Symbol.match] called on incompatible receiver");
    }
    
    auto* regexp = static_cast<RegExpObject*>(thisValue.asObject());
    
    // 引数がない場合はundefinedをデフォルト値として使用
    std::string str = args.empty() || args[0].isUndefined() ? "undefined" : args[0].toString()->value();
    
    // グローバルフラグがある場合は、すべてのマッチを配列として返す
    if (regexp->global()) {
        // グローバル検索の場合、結果を配列に格納
        auto* result = ctx->createArray();
        size_t index = 0;
        
        // lastIndexを0に設定
        regexp->setLastIndex(0);
        
        // すべてのマッチを検索
        while (true) {
            auto* match = regexp->exec(str);
            if (!match) {
                break;
            }
            
            // マッチした文字列を配列に追加
            Value matchStr = match->get("0");
            result->defineProperty(std::to_string(index++), PropertyDescriptor(matchStr, true, true, true));
        }
        
        // マッチがない場合はnullを返す
        if (index == 0) {
            return Value::null();
        }
        
        return Value(result);
    } else {
        // 非グローバル検索の場合、最初のマッチのみを返す
        auto* match = regexp->exec(str);
        
        // マッチがない場合はnullを返す
        if (!match) {
            return Value::null();
        }
        
        return Value(match);
    }
}

Value regexpMatchAll(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // thisValueがRegExpオブジェクトでない場合はエラー
    if (!thisValue.isObject() || !thisValue.asObject()->isRegExp()) {
        return ctx->throwTypeError("RegExp.prototype[Symbol.matchAll] called on incompatible receiver");
    }
    
    auto* regexp = static_cast<RegExpObject*>(thisValue.asObject());
    
    // 引数がない場合はundefinedをデフォルト値として使用
    std::string str = args.empty() || args[0].isUndefined() ? "undefined" : args[0].toString()->value();
    
    // 新しいRegExpオブジェクトを作成（フラグにgを追加）
    std::string flags = regexp->getFlags();
    if (flags.find('g') == std::string::npos) {
        flags += "g";
    }
    
    auto* newRegexp = new RegExpObject(regexp->getPattern(), flags);
    newRegexp->setPrototype(ctx->regexpPrototype());
    
    // マッチ結果を反復処理するイテレータオブジェクトを作成
    // 注: 実際の実装ではイテレータオブジェクトを作成する必要があります
    // ここではダミーの配列を返しています
    
    auto* result = ctx->createArray();
    size_t index = 0;
    
    // lastIndexを0に設定
    newRegexp->setLastIndex(0);
    
    // すべてのマッチを検索
    while (true) {
        auto* match = newRegexp->exec(str);
        if (!match) {
            break;
        }
        
        // マッチ結果を配列に追加
        result->defineProperty(std::to_string(index++), PropertyDescriptor(Value(match), true, true, true));
    }
    
    return Value(result);
}

Value regexpReplace(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // thisValueがRegExpオブジェクトでない場合はエラー
    if (!thisValue.isObject() || !thisValue.asObject()->isRegExp()) {
        return ctx->throwTypeError("RegExp.prototype[Symbol.replace] called on incompatible receiver");
    }
    
    auto* regexp = static_cast<RegExpObject*>(thisValue.asObject());
    
    // 引数チェック
    if (args.size() < 2) {
        return Value(ctx->createString(""));
    }
    
    // 文字列の取得
    std::string str = args[0].isUndefined() ? "undefined" : args[0].toString()->value();
    
    // 置換値の取得
    std::string replacement;
    if (args[1].isFunction()) {
        // 関数を使用した置換（詳細な実装は省略）
        // 実際の実装では、マッチごとに関数を呼び出し、返り値で置換する必要があります
        replacement = "$&";
    } else {
        replacement = args[1].toString()->value();
    }
    
    // グローバルフラグがある場合は、すべてのマッチを置換
    if (regexp->global()) {
        // lastIndexを0に設定
        regexp->setLastIndex(0);
        
        std::string result = str;
        size_t lastEnd = 0;
        
        // すべてのマッチを検索して置換
        while (true) {
            auto* match = regexp->exec(str);
            if (!match) {
                break;
            }
            
            // マッチ情報を取得
            size_t matchIndex = static_cast<size_t>(match->get("index").toNumber());
            std::string matchStr = match->get("0").toString()->value();
            
            // 置換処理
            // 実際の実装では、$&, $`, $', $n などの特殊文字を処理する必要があります
            std::string currentReplacement = replacement;
            
            // 置換を適用
            result = result.substr(0, matchIndex) + currentReplacement + result.substr(matchIndex + matchStr.length());
            
            // 次の検索位置を更新
            lastEnd = matchIndex + currentReplacement.length();
        }
        
        return Value(ctx->createString(result));
    } else {
        // 非グローバル検索の場合、最初のマッチのみを置換
        auto* match = regexp->exec(str);
        
        // マッチがない場合は元の文字列を返す
        if (!match) {
            return Value(ctx->createString(str));
        }
        
        // マッチ情報を取得
        size_t matchIndex = static_cast<size_t>(match->get("index").toNumber());
        std::string matchStr = match->get("0").toString()->value();
        
        // 置換処理
        // 実際の実装では、$&, $`, $', $n などの特殊文字を処理する必要があります
        std::string currentReplacement = replacement;
        
        // 置換を適用
        std::string result = str.substr(0, matchIndex) + currentReplacement + str.substr(matchIndex + matchStr.length());
        
        return Value(ctx->createString(result));
    }
}

Value regexpSearch(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // thisValueがRegExpオブジェクトでない場合はエラー
    if (!thisValue.isObject() || !thisValue.asObject()->isRegExp()) {
        return ctx->throwTypeError("RegExp.prototype[Symbol.search] called on incompatible receiver");
    }
    
    auto* regexp = static_cast<RegExpObject*>(thisValue.asObject());
    
    // 引数がない場合はundefinedをデフォルト値として使用
    std::string str = args.empty() || args[0].isUndefined() ? "undefined" : args[0].toString()->value();
    
    // lastIndexの元の値を保存
    size_t originalLastIndex = regexp->lastIndex();
    
    // lastIndexを0に設定
    regexp->setLastIndex(0);
    
    // 検索を実行
    auto* match = regexp->exec(str);
    
    // lastIndexを元の値に戻す
    regexp->setLastIndex(originalLastIndex);
    
    // マッチがない場合は-1を返す
    if (!match) {
        return Value(-1);
    }
    
    // マッチしたインデックスを返す
    return Value(match->get("index").toNumber());
}

Value regexpSplit(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
    // thisValueがRegExpオブジェクトでない場合はエラー
    if (!thisValue.isObject() || !thisValue.asObject()->isRegExp()) {
        return ctx->throwTypeError("RegExp.prototype[Symbol.split] called on incompatible receiver");
    }
    
    auto* regexp = static_cast<RegExpObject*>(thisValue.asObject());
    
    // 引数がない場合はundefinedをデフォルト値として使用
    std::string str = args.empty() || args[0].isUndefined() ? "undefined" : args[0].toString()->value();
    
    // 制限数（省略可能）
    size_t limit = std::numeric_limits<size_t>::max();
    if (args.size() > 1 && !args[1].isUndefined()) {
        double limitDouble = args[1].toNumber();
        if (limitDouble >= 0) {
            limit = static_cast<size_t>(limitDouble);
        }
    }
    
    // 結果配列を作成
    auto* result = ctx->createArray();
    size_t resultIndex = 0;
    
    // 空文字列の場合は特別処理
    if (str.empty()) {
        auto* match = regexp->exec("");
        if (!match) {
            result->defineProperty("0", PropertyDescriptor(Value(ctx->createString("")), true, true, true));
        }
        return Value(result);
    }
    
    // 分割処理
    size_t lastIndex = 0;
    size_t nextSearchIndex = 0;
    
    while (nextSearchIndex < str.length()) {
        // lastIndexを設定
        regexp->setLastIndex(nextSearchIndex);
        
        // マッチを検索
        auto* match = regexp->exec(str);
        
        // マッチがない場合、残りの文字列を追加して終了
        if (!match) {
            if (resultIndex < limit) {
                result->defineProperty(std::to_string(resultIndex++), 
                                      PropertyDescriptor(Value(ctx->createString(str.substr(lastIndex))), 
                                                       true, true, true));
            }
            break;
        }
        
        // マッチ情報を取得
        size_t matchIndex = static_cast<size_t>(match->get("index").toNumber());
        
        // マッチが前回のマッチと同じ位置の場合、1文字進める
        if (matchIndex == lastIndex) {
            nextSearchIndex = lastIndex + 1;
            continue;
        }
        
        // マッチ前の文字列を追加
        if (resultIndex < limit) {
            result->defineProperty(std::to_string(resultIndex++), 
                                  PropertyDescriptor(Value(ctx->createString(str.substr(lastIndex, matchIndex - lastIndex))), 
                                                   true, true, true));
        }
        
        // 制限に達した場合は終了
        if (resultIndex >= limit) {
            break;
        }
        
        // キャプチャグループを追加
        size_t groupCount = static_cast<size_t>(match->get("length").toNumber()) - 1;
        for (size_t i = 1; i <= groupCount; ++i) {
            if (resultIndex < limit) {
                result->defineProperty(std::to_string(resultIndex++), 
                                      PropertyDescriptor(match->get(std::to_string(i)), 
                                                       true, true, true));
            } else {
                break;
            }
        }
        
        // 次の検索位置を更新
        lastIndex = regexp->lastIndex();
        nextSearchIndex = lastIndex;
    }
    
    return Value(result);
}

// RegExpプロトタイプの初期化
void initializeRegExpPrototype(ExecutionContext* ctx, Object* prototype) {
    // プロトタイププロパティの設定
    prototype->defineProperty("constructor", PropertyDescriptor(Value::undefined(), true, false, true));
    
    // メソッドの追加
    auto* execFunc = ctx->createFunction(regexpExec, "exec", 1);
    prototype->defineProperty("exec", PropertyDescriptor(Value(execFunc), true, false, true));
    
    auto* testFunc = ctx->createFunction(regexpTest, "test", 1);
    prototype->defineProperty("test", PropertyDescriptor(Value(testFunc), true, false, true));
    
    auto* toStringFunc = ctx->createFunction(regexpToString, "toString", 0);
    prototype->defineProperty("toString", PropertyDescriptor(Value(toStringFunc), true, false, true));
    
    // Symbolメソッドの追加
    auto* matchFunc = ctx->createFunction(regexpMatch, "[Symbol.match]", 1);
    prototype->defineProperty(Symbol::for_("match").description(), PropertyDescriptor(Value(matchFunc), true, false, true));
    
    auto* matchAllFunc = ctx->createFunction(regexpMatchAll, "[Symbol.matchAll]", 1);
    prototype->defineProperty(Symbol::for_("matchAll").description(), PropertyDescriptor(Value(matchAllFunc), true, false, true));
    
    auto* replaceFunc = ctx->createFunction(regexpReplace, "[Symbol.replace]", 2);
    prototype->defineProperty(Symbol::for_("replace").description(), PropertyDescriptor(Value(replaceFunc), true, false, true));
    
    auto* searchFunc = ctx->createFunction(regexpSearch, "[Symbol.search]", 1);
    prototype->defineProperty(Symbol::for_("search").description(), PropertyDescriptor(Value(searchFunc), true, false, true));
    
    auto* splitFunc = ctx->createFunction(regexpSplit, "[Symbol.split]", 2);
    prototype->defineProperty(Symbol::for_("split").description(), PropertyDescriptor(Value(splitFunc), true, false, true));
}

// RegExpオブジェクトの登録
void registerRegExpObject(ExecutionContext* ctx, Object* global) {
    // RegExp.prototypeを作成
    auto* regexpProto = ctx->createObject();
    
    // RegExp コンストラクタを作成
    auto* regexpConstructorObj = ctx->createFunction(regexpConstructor, "RegExp", 2);
    
    // プロトタイプとコンストラクタを初期化
    initializeRegExpPrototype(ctx, regexpProto);
    
    // RegExp.prototypeの設定
    regexpConstructorObj->defineProperty("prototype", PropertyDescriptor(Value(regexpProto), false, false, false));
    
    // プロトタイプのconstructorを設定
    regexpProto->defineProperty("constructor", PropertyDescriptor(Value(regexpConstructorObj), true, false, true));
    
    // グローバルオブジェクトに登録
    global->defineProperty("RegExp", PropertyDescriptor(Value(regexpConstructorObj), true, false, true));
    
    // コンテキストに保存
    ctx->setRegexpPrototype(regexpProto);
    ctx->setRegexpConstructor(regexpConstructorObj);
}

} // namespace aero 