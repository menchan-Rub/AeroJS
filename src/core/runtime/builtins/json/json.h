/**
 * @file json.h
 * @brief JavaScript の JSON 組み込みオブジェクトの定義
 * @copyright 2023 AeroJS Project
 */

#ifndef AERO_JSON_H
#define AERO_JSON_H

#include "core/runtime/object.h"
#include <vector>
#include <string>

namespace aero {

class Context;
class GlobalObject;

/**
 * @brief JSON.parse 実装
 * JSONテキストを解析して値に変換します
 * 
 * @param thisValue this値
 * @param arguments 引数リスト（text [, reviver]）
 * @param context 実行コンテキスト
 * @return 解析されたJavaScript値
 */
Value jsonParse(Value thisValue, const std::vector<Value>& arguments, Context* context);

/**
 * @brief JSON.stringify 実装
 * JavaScript値をJSON文字列に変換します
 * 
 * @param thisValue this値
 * @param arguments 引数リスト（value [, replacer [, space]]）
 * @param context 実行コンテキスト
 * @return JSON文字列
 */
Value jsonStringify(Value thisValue, const std::vector<Value>& arguments, Context* context);

/**
 * @brief JSONパーサーのためのクラス
 */
class JSONParser {
public:
    /**
     * @brief JSONテキストを解析してValueに変換
     * @param text 解析するJSONテキスト
     * @param context 実行コンテキスト
     * @return 解析された値
     */
    static Value parse(const std::string& text, Context* context);

private:
    /**
     * @brief コンストラクタ
     * @param text 解析するJSONテキスト
     * @param context 実行コンテキスト
     */
    JSONParser(const std::string& text, Context* context);
    
    /**
     * @brief 解析処理を実行
     * @return 解析された値
     */
    Value doParse();
    
    /**
     * @brief JSONの値を解析
     * @return 解析された値
     */
    Value parseValue();
    
    /**
     * @brief JSONのオブジェクトを解析
     * @return オブジェクト
     */
    Value parseObject();
    
    /**
     * @brief JSONの配列を解析
     * @return 配列
     */
    Value parseArray();
    
    /**
     * @brief JSON文字列を解析
     * @return 文字列
     */
    Value parseString();
    
    /**
     * @brief JSON数値を解析
     * @return 数値
     */
    Value parseNumber();
    
    /**
     * @brief JSONキーワード（true, false, null）を解析
     * @return 対応する値
     */
    Value parseKeyword();
    
    /**
     * @brief 空白文字をスキップ
     */
    void skipWhitespace();
    
    /**
     * @brief 次の文字を取得し、ポインタを進める
     * @return 次の文字
     */
    char nextChar();
    
    /**
     * @brief 現在位置の文字をピーク（ポインタを進めない）
     * @return 現在位置の文字
     */
    char peekChar();
    
    /**
     * @brief ポインタを1つ戻す
     */
    void backChar();
    
    /**
     * @brief 構文エラーを投げる
     * @param message エラーメッセージ
     */
    void throwSyntaxError(const std::string& message);

    // 解析対象のJSONテキスト
    std::string m_text;
    
    // テキストの長さ
    size_t m_length;
    
    // 現在の解析位置
    size_t m_pos;
    
    // 実行コンテキスト
    Context* m_context;
};

/**
 * @brief JSON文字列化のためのクラス
 */
class JSONStringifier {
public:
    /**
     * @brief 値をJSON文字列に変換
     * @param value 変換する値
     * @param replacer 置換関数またはフィルタ配列
     * @param space インデント文字列または数値
     * @param context 実行コンテキスト
     * @return JSON文字列
     */
    static Value stringify(Value value, Value replacer, Value space, Context* context);

private:
    /**
     * @brief コンストラクタ
     * @param replacer 置換関数またはフィルタ配列
     * @param space インデント文字列または数値
     * @param context 実行コンテキスト
     */
    JSONStringifier(Value replacer, Value space, Context* context);
    
    /**
     * @brief 文字列化を実行
     * @param value 変換する値
     * @param holder ホルダーオブジェクト
     * @param key プロパティキー
     * @return JSON文字列
     */
    std::string doStringify(Value value, Value holder, const std::string& key);
    
    /**
     * @brief オブジェクトをJSON表現に変換
     * @param value オブジェクト
     * @return JSON文字列
     */
    std::string stringifyObject(Value value);
    
    /**
     * @brief 配列をJSON表現に変換
     * @param value 配列
     * @return JSON文字列
     */
    std::string stringifyArray(Value value);
    
    /**
     * @brief 文字列をJSON表現に変換（エスケープ処理を含む）
     * @param str 文字列
     * @return エスケープされたJSON文字列
     */
    std::string escapeString(const std::string& str);
    
    /**
     * @brief 指定レベルのインデントを生成
     * @param level インデントレベル
     * @return インデント文字列
     */
    std::string makeIndent(int level);
    
    /**
     * @brief 置換関数を適用
     * @param holder ホルダーオブジェクト
     * @param key プロパティキー
     * @param value 変換する値
     * @return 置換後の値
     */
    Value applyReplacer(Value holder, const std::string& key, Value value);
    
    // インデント文字列
    std::string m_indent;
    
    // 置換関数
    Value m_replacerFunction;
    
    // プロパティフィルタ
    std::vector<std::string> m_propertyList;
    
    // 実行コンテキスト
    Context* m_context;
    
    // 循環参照検出のためのオブジェクトセット
    std::vector<Object*> m_stack;
};

/**
 * @brief JSON組み込みオブジェクトをグローバルオブジェクトに登録
 * @param global グローバルオブジェクト
 */
void registerJSONBuiltin(GlobalObject* global);

} // namespace aero

#endif // AERO_JSON_H 