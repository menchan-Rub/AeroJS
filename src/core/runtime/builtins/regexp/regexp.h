/**
 * @file regexp.h
 * @brief JavaScript の正規表現オブジェクトの定義
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#ifndef AERO_REGEXP_H
#define AERO_REGEXP_H

#include "core/runtime/object.h"
#include <regex>
#include <string>
#include <memory>
#include <optional>

namespace aero {

/**
 * @brief JavaScript の RegExp オブジェクトを表現するクラス
 * 
 * ECMAScript 仕様に基づいた正規表現オブジェクトの実装です。
 * パターンとフラグを保持し、文字列に対するマッチングを行います。
 */
class RegExpObject : public Object {
public:
    /**
     * @brief 正規表現オブジェクトを構築します
     * 
     * @param pattern 正規表現のパターン文字列
     * @param flags 正規表現のフラグ文字列（省略可能）
     */
    RegExpObject(const std::string& pattern, const std::string& flags = "");
    
    /**
     * @brief デストラクタ
     */
    ~RegExpObject() override;

    /**
     * @brief 正規表現のパターン文字列を取得します
     * 
     * @return 正規表現のパターン文字列
     */
    const std::string& getPattern() const;
    
    /**
     * @brief 正規表現のフラグ文字列を取得します
     * 
     * @return 正規表現のフラグ文字列
     */
    const std::string& getFlags() const;
    
    /**
     * @brief 文字列に対して正規表現のマッチングを実行します
     * 
     * @param str マッチング対象の文字列
     * @return マッチング結果のオブジェクト
     */
    Object* exec(const std::string& str);
    
    /**
     * @brief 文字列に対して正規表現のテストを実行します
     * 
     * @param str テスト対象の文字列
     * @return マッチする場合は true、それ以外は false
     */
    bool test(const std::string& str);
    
    /**
     * @brief 正規表現オブジェクトを文字列に変換します
     * 
     * @return 正規表現の文字列表現
     */
    std::string toString() const;
    
    /**
     * @brief グローバルフラグが設定されているかを返します
     * 
     * @return グローバルフラグが設定されている場合は true
     */
    bool global() const;
    
    /**
     * @brief 大小文字を区別しないフラグが設定されているかを返します
     * 
     * @return 大小文字を区別しないフラグが設定されている場合は true
     */
    bool ignoreCase() const;
    
    /**
     * @brief 複数行モードフラグが設定されているかを返します
     * 
     * @return 複数行モードフラグが設定されている場合は true
     */
    bool multiline() const;
    
    /**
     * @brief スティッキーフラグが設定されているかを返します
     * 
     * @return スティッキーフラグが設定されている場合は true
     */
    bool sticky() const;
    
    /**
     * @brief ドット全マッチフラグが設定されているかを返します
     * 
     * @return ドット全マッチフラグが設定されている場合は true
     */
    bool dotAll() const;
    
    /**
     * @brief Unicodeフラグが設定されているかを返します
     * 
     * @return Unicodeフラグが設定されている場合は true
     */
    bool unicode() const;
    
    /**
     * @brief 最後のマッチのインデックスを取得します
     * 
     * @return 最後のマッチのインデックス
     */
    size_t lastIndex() const;
    
    /**
     * @brief 最後のマッチのインデックスを設定します
     * 
     * @param index 設定するインデックス
     */
    void setLastIndex(size_t index);
    
private:
    std::string m_pattern;        ///< 正規表現のパターン文字列
    std::string m_flags;          ///< 正規表現のフラグ文字列
    std::regex m_regex;           ///< 内部で使用するC++の正規表現オブジェクト
    size_t m_lastIndex{0};        ///< 最後のマッチのインデックス
    
    /**
     * @brief フラグからC++の正規表現オプションを構築します
     * 
     * @param flags 正規表現のフラグ文字列
     * @return 正規表現オプション
     */
    static std::regex::flag_type createRegexOptions(const std::string& flags);
    
    /**
     * @brief 正規表現の構文が有効かチェックします
     * 
     * @param pattern パターン文字列
     * @param flags フラグ文字列
     */
    static void validateRegExp(const std::string& pattern, const std::string& flags);
};

/**
 * @brief RegExp コンストラクタ関数
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 新しいRegExpオブジェクト
 */
Value regexpConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief RegExp.prototype.exec メソッド
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return マッチング結果の配列またはnull
 */
Value regexpExec(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief RegExp.prototype.test メソッド
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return マッチする場合はtrue、それ以外はfalse
 */
Value regexpTest(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief RegExp.prototype.toString メソッド
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 正規表現の文字列表現
 */
Value regexpToString(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief RegExp.prototype[Symbol.match] メソッド
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return マッチング結果
 */
Value regexpMatch(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief RegExp.prototype[Symbol.matchAll] メソッド
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return マッチングイテレータ
 */
Value regexpMatchAll(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief RegExp.prototype[Symbol.replace] メソッド
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 置換後の文字列
 */
Value regexpReplace(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief RegExp.prototype[Symbol.search] メソッド
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return マッチングの開始インデックスまたは-1
 */
Value regexpSearch(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief RegExp.prototype[Symbol.split] メソッド
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 分割された文字列の配列
 */
Value regexpSplit(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief RegExpオブジェクトのプロトタイプを初期化します
 * 
 * @param ctx 実行コンテキスト
 * @param prototype プロトタイプオブジェクト
 */
void initializeRegExpPrototype(ExecutionContext* ctx, Object* prototype);

/**
 * @brief RegExpオブジェクトをグローバルオブジェクトに登録します
 * 
 * @param ctx 実行コンテキスト
 * @param global グローバルオブジェクト
 */
void registerRegExpObject(ExecutionContext* ctx, Object* global);

} // namespace aero

#endif // AERO_REGEXP_H 