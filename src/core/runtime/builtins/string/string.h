/**
 * @file string.h
 * @brief JavaScript の String オブジェクトの定義
 */

#ifndef AEROJS_CORE_STRING_H
#define AEROJS_CORE_STRING_H

#include "../../../object.h"
#include "../../../value.h"
#include <memory>
#include <mutex>
#include <string>

namespace aerojs {
namespace core {

class Function;

/**
 * @class String
 * @brief JavaScript の String オブジェクトを表すクラス
 * 
 * ECMAScript 仕様に準拠した String オブジェクトの実装。
 * プリミティブな文字列値のラッパーとして機能します。
 */
class String : public Object {
public:
    /**
     * @brief デフォルトコンストラクタ
     * 
     * 空の文字列を作成します。
     */
    String();
    
    /**
     * @brief 文字列を指定して作成するコンストラクタ
     * 
     * @param value 初期値となる文字列
     */
    explicit String(const std::string& value);
    
    /**
     * @brief デストラクタ
     */
    virtual ~String();
    
    /**
     * @brief 内部文字列値を取得
     * 
     * @return 内部に保持する文字列値
     */
    const std::string& getValue() const;
    
    /**
     * @brief String コンストラクタ関数
     * 
     * new String() または String() として呼び出されたときに実行される関数。
     * 
     * @param arguments 引数リスト
     * @return 新しく作成された String オブジェクト
     */
    static ValuePtr construct(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief String.fromCharCode 静的メソッド
     * 
     * 指定されたUTF-16コードユニットから文字列を作成します。
     * 
     * @param arguments 引数リスト
     * @return 作成された文字列
     */
    static ValuePtr fromCharCode(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief String.fromCodePoint 静的メソッド
     * 
     * 指定されたコードポイントから文字列を作成します。
     * 
     * @param arguments 引数リスト
     * @return 作成された文字列
     */
    static ValuePtr fromCodePoint(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief String.raw 静的メソッド
     * 
     * テンプレートリテラルの生の文字列形式を返します。
     * 
     * @param arguments 引数リスト
     * @return 加工されていない文字列
     */
    static ValuePtr raw(const std::vector<ValuePtr>& arguments);
    
    // String.prototype メソッド
    
    /**
     * @brief charAt メソッド
     * 
     * 指定された位置の文字を返します。
     * 
     * @param arguments 引数リスト
     * @return 指定位置の文字
     */
    static ValuePtr charAt(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief charCodeAt メソッド
     * 
     * 指定された位置の文字のUTF-16コード単位を返します。
     * 
     * @param arguments 引数リスト
     * @return 文字のコード単位
     */
    static ValuePtr charCodeAt(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief codePointAt メソッド
     * 
     * 指定された位置から始まるUTF-16エンコードされたコードポイントを返します。
     * 
     * @param arguments 引数リスト
     * @return コードポイント
     */
    static ValuePtr codePointAt(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief concat メソッド
     * 
     * 現在の文字列に引数で指定された文字列を結合した新しい文字列を返します。
     * 
     * @param arguments 引数リスト
     * @return 結合された新しい文字列
     */
    static ValuePtr concat(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief includes メソッド
     * 
     * 文字列が別の文字列を含むかどうかを判定します。
     * 
     * @param arguments 引数リスト
     * @return 判定結果（ブール値）
     */
    static ValuePtr includes(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief endsWith メソッド
     * 
     * 文字列が指定された文字列で終わるかどうかを判定します。
     * 
     * @param arguments 引数リスト
     * @return 判定結果（ブール値）
     */
    static ValuePtr endsWith(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief startsWith メソッド
     * 
     * 文字列が指定された文字列で始まるかどうかを判定します。
     * 
     * @param arguments 引数リスト
     * @return 判定結果（ブール値）
     */
    static ValuePtr startsWith(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief indexOf メソッド
     * 
     * 指定された文字列が最初に現れる位置を返します。
     * 
     * @param arguments 引数リスト
     * @return 見つかった位置、または -1
     */
    static ValuePtr indexOf(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief lastIndexOf メソッド
     * 
     * 指定された文字列が最後に現れる位置を返します。
     * 
     * @param arguments 引数リスト
     * @return 見つかった位置、または -1
     */
    static ValuePtr lastIndexOf(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief localeCompare メソッド
     * 
     * 現在の文字列と別の文字列を現在のロケールに従って比較します。
     * 
     * @param arguments 引数リスト
     * @return 比較結果
     */
    static ValuePtr localeCompare(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief match メソッド
     * 
     * 正規表現に対する文字列の一致を取得します。
     * 
     * @param arguments 引数リスト
     * @return 一致した結果の配列
     */
    static ValuePtr match(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief matchAll メソッド
     * 
     * 正規表現に対する文字列のすべての一致を取得します。
     * 
     * @param arguments 引数リスト
     * @return 一致したすべての結果のイテレータ
     */
    static ValuePtr matchAll(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief normalize メソッド
     * 
     * Unicode正規化形式を適用します。
     * 
     * @param arguments 引数リスト
     * @return 正規化された文字列
     */
    static ValuePtr normalize(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief padEnd メソッド
     * 
     * 現在の文字列の末尾を別の文字列で埋めて、指定された長さに達するまで文字列を拡張します。
     * 
     * @param arguments 引数リスト
     * @return パディングされた文字列
     */
    static ValuePtr padEnd(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief padStart メソッド
     * 
     * 現在の文字列の先頭を別の文字列で埋めて、指定された長さに達するまで文字列を拡張します。
     * 
     * @param arguments 引数リスト
     * @return パディングされた文字列
     */
    static ValuePtr padStart(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief repeat メソッド
     * 
     * 文字列を指定された回数だけ繰り返した新しい文字列を作成します。
     * 
     * @param arguments 引数リスト
     * @return 繰り返された文字列
     */
    static ValuePtr repeat(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief replace メソッド
     * 
     * 文字列内の一部を別の文字列で置き換えます。
     * 
     * @param arguments 引数リスト
     * @return 置換された文字列
     */
    static ValuePtr replace(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief replaceAll メソッド
     * 
     * 文字列内のすべての一致を別の文字列で置き換えます。
     * 
     * @param arguments 引数リスト
     * @return 置換された文字列
     */
    static ValuePtr replaceAll(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief search メソッド
     * 
     * 正規表現に一致する文字列内の位置を検索します。
     * 
     * @param arguments 引数リスト
     * @return 見つかった位置、または -1
     */
    static ValuePtr search(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief slice メソッド
     * 
     * 文字列の一部を抽出して新しい文字列として返します。
     * 
     * @param arguments 引数リスト
     * @return 抽出された文字列
     */
    static ValuePtr slice(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief split メソッド
     * 
     * 文字列を区切り文字で分割して配列として返します。
     * 
     * @param arguments 引数リスト
     * @return 分割された文字列の配列
     */
    static ValuePtr split(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief substring メソッド
     * 
     * 文字列の一部を返します。
     * 
     * @param arguments 引数リスト
     * @return 部分文字列
     */
    static ValuePtr substring(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief toLowerCase メソッド
     * 
     * 文字列を小文字に変換します。
     * 
     * @param arguments 引数リスト
     * @return 小文字に変換された文字列
     */
    static ValuePtr toLowerCase(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief toUpperCase メソッド
     * 
     * 文字列を大文字に変換します。
     * 
     * @param arguments 引数リスト
     * @return 大文字に変換された文字列
     */
    static ValuePtr toUpperCase(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief trim メソッド
     * 
     * 文字列の両端の空白を削除します。
     * 
     * @param arguments 引数リスト
     * @return トリムされた文字列
     */
    static ValuePtr trim(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief trimStart メソッド
     * 
     * 文字列の先頭の空白を削除します。
     * 
     * @param arguments 引数リスト
     * @return 先頭がトリムされた文字列
     */
    static ValuePtr trimStart(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief trimEnd メソッド
     * 
     * 文字列の末尾の空白を削除します。
     * 
     * @param arguments 引数リスト
     * @return 末尾がトリムされた文字列
     */
    static ValuePtr trimEnd(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief valueOf メソッド
     * 
     * String オブジェクトのプリミティブ値を返します。
     * 
     * @param arguments 引数リスト
     * @return プリミティブ文字列値
     */
    static ValuePtr valueOf(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief toString メソッド
     * 
     * String オブジェクトを文字列表現に変換します。
     * 
     * @param arguments 引数リスト
     * @return 文字列表現
     */
    static ValuePtr toString(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief String プロトタイプオブジェクトを取得
     * 
     * @return String プロトタイプオブジェクト
     */
    static std::shared_ptr<Object> getPrototype();
    
    /**
     * @brief String コンストラクタ関数を取得
     * 
     * @return String コンストラクタ関数
     */
    static std::shared_ptr<Function> getConstructor();
    
private:
    /**
     * @brief String オブジェクトの初期化
     * 
     * プロトタイプとコンストラクタの設定を行います。
     */
    static void initialize();
    
    /** @brief 内部文字列値 */
    std::string m_value;
    
    /** @brief String プロトタイプオブジェクト */
    static std::shared_ptr<String> s_prototype;
    
    /** @brief String コンストラクタ関数 */
    static std::shared_ptr<Function> s_constructor;
    
    /** @brief スレッドセーフのためのミューテックス */
    static std::mutex s_mutex;
};

// スマートポインタの型エイリアス
using StringPtr = std::shared_ptr<String>;

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_STRING_H 