/**
 * @file array.h
 * @brief JavaScript の Array オブジェクトの定義
 */

#ifndef AEROJS_CORE_ARRAY_H
#define AEROJS_CORE_ARRAY_H

#include "../../../object.h"
#include "../../../value.h"
#include <memory>
#include <mutex>
#include <vector>

namespace aerojs {
namespace core {

class Function;

/**
 * @class Array
 * @brief JavaScript の Array オブジェクトを表すクラス
 * 
 * ECMAScript 仕様に準拠した Array オブジェクトの実装。
 * 配列の要素は数値インデックスをプロパティとして持つ Object として実装されています。
 */
class Array : public Object {
public:
    /**
     * @brief デフォルトコンストラクタ
     * 
     * 空の配列を作成します。
     */
    Array();
    
    /**
     * @brief デストラクタ
     */
    virtual ~Array();
    
    /**
     * @brief Array コンストラクタ関数
     * 
     * new Array() または Array() として呼び出されたときに実行される関数。
     * 
     * @param arguments 引数リスト
     * @return 新しく作成された Array オブジェクト
     */
    static ValuePtr construct(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief Array.isArray 静的メソッド
     * 
     * 指定されたオブジェクトが Array かどうかを判定します。
     * 
     * @param arguments 引数リスト
     * @return 判定結果（ブール値）
     */
    static ValuePtr isArray(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief Array.from 静的メソッド
     * 
     * 反復可能オブジェクトやArray-likeオブジェクトから新しい配列を作成します。
     * 
     * @param arguments 引数リスト
     * @return 新しく作成された Array オブジェクト
     */
    static ValuePtr from(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief Array.of 静的メソッド
     * 
     * 任意の数の引数から新しい配列を作成します。
     * 
     * @param arguments 引数リスト
     * @return 新しく作成された Array オブジェクト
     */
    static ValuePtr of(const std::vector<ValuePtr>& arguments);
    
    // Array.prototype メソッド
    
    /**
     * @brief push メソッド
     * 
     * 配列の末尾に要素を追加し、新しい配列の長さを返します。
     * 
     * @param arguments 引数リスト
     * @return 配列の新しい長さ
     */
    static ValuePtr push(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief pop メソッド
     * 
     * 配列の末尾から要素を削除し、その要素を返します。
     * 
     * @param arguments 引数リスト
     * @return 削除された要素
     */
    static ValuePtr pop(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief shift メソッド
     * 
     * 配列の先頭から要素を削除し、その要素を返します。
     * 
     * @param arguments 引数リスト
     * @return 削除された要素
     */
    static ValuePtr shift(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief unshift メソッド
     * 
     * 配列の先頭に要素を追加し、新しい配列の長さを返します。
     * 
     * @param arguments 引数リスト
     * @return 配列の新しい長さ
     */
    static ValuePtr unshift(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief splice メソッド
     * 
     * 配列の一部を削除または置換し、削除された要素を含む配列を返します。
     * 
     * @param arguments 引数リスト
     * @return 削除された要素を含む配列
     */
    static ValuePtr splice(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief concat メソッド
     * 
     * 現在の配列に引数で指定された配列や値を結合した新しい配列を返します。
     * 
     * @param arguments 引数リスト
     * @return 結合された新しい配列
     */
    static ValuePtr concat(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief slice メソッド
     * 
     * 配列の一部を抽出して新しい配列として返します。
     * 
     * @param arguments 引数リスト
     * @return 抽出された新しい配列
     */
    static ValuePtr slice(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief reverse メソッド
     * 
     * 配列の要素の順序を反転します。
     * 
     * @param arguments 引数リスト
     * @return 反転された配列
     */
    static ValuePtr reverse(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief sort メソッド
     * 
     * 配列の要素をソートします。
     * 
     * @param arguments 引数リスト
     * @return ソートされた配列
     */
    static ValuePtr sort(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief indexOf メソッド
     * 
     * 指定された要素が配列内で最初に見つかったインデックスを返します。
     * 
     * @param arguments 引数リスト
     * @return 見つかったインデックス、または -1
     */
    static ValuePtr indexOf(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief lastIndexOf メソッド
     * 
     * 指定された要素が配列内で最後に見つかったインデックスを返します。
     * 
     * @param arguments 引数リスト
     * @return 見つかったインデックス、または -1
     */
    static ValuePtr lastIndexOf(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief includes メソッド
     * 
     * 配列が特定の要素を含むかどうかを判定します。
     * 
     * @param arguments 引数リスト
     * @return 判定結果（ブール値）
     */
    static ValuePtr includes(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief find メソッド
     * 
     * 指定された条件を満たす最初の要素の値を返します。
     * 
     * @param arguments 引数リスト
     * @return 見つかった要素または undefined
     */
    static ValuePtr find(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief findIndex メソッド
     * 
     * 指定された条件を満たす最初の要素のインデックスを返します。
     * 
     * @param arguments 引数リスト
     * @return 見つかったインデックス、または -1
     */
    static ValuePtr findIndex(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief some メソッド
     * 
     * 配列内の少なくとも1つの要素が指定された条件を満たすかどうかをテストします。
     * 
     * @param arguments 引数リスト
     * @return 判定結果（ブール値）
     */
    static ValuePtr some(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief every メソッド
     * 
     * 配列内のすべての要素が指定された条件を満たすかどうかをテストします。
     * 
     * @param arguments 引数リスト
     * @return 判定結果（ブール値）
     */
    static ValuePtr every(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief forEach メソッド
     * 
     * 配列内の各要素に対して指定された関数を実行します。
     * 
     * @param arguments 引数リスト
     * @return undefined
     */
    static ValuePtr forEach(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief map メソッド
     * 
     * 配列内のすべての要素に対して指定された関数を呼び出し、その結果から新しい配列を作成します。
     * 
     * @param arguments 引数リスト
     * @return 新しい配列
     */
    static ValuePtr map(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief filter メソッド
     * 
     * 指定された条件を満たすすべての要素を含む新しい配列を作成します。
     * 
     * @param arguments 引数リスト
     * @return フィルタリングされた新しい配列
     */
    static ValuePtr filter(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief reduce メソッド
     * 
     * 配列の各要素に対して指定された関数を適用し、単一の値にします。
     * 
     * @param arguments 引数リスト
     * @return 累積結果
     */
    static ValuePtr reduce(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief reduceRight メソッド
     * 
     * 配列の各要素に対して指定された関数を右から左へ適用し、単一の値にします。
     * 
     * @param arguments 引数リスト
     * @return 累積結果
     */
    static ValuePtr reduceRight(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief join メソッド
     * 
     * 配列のすべての要素を指定された区切り文字で連結した文字列を返します。
     * 
     * @param arguments 引数リスト
     * @return 連結された文字列
     */
    static ValuePtr join(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief toString メソッド
     * 
     * 配列を文字列に変換します。
     * 
     * @param arguments 引数リスト
     * @return 文字列表現
     */
    static ValuePtr toString(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief toLocaleString メソッド
     * 
     * 配列の要素をローカライズされた文字列に変換します。
     * 
     * @param arguments 引数リスト
     * @return ローカライズされた文字列表現
     */
    static ValuePtr toLocaleString(const std::vector<ValuePtr>& arguments);
    
    /**
     * @brief Array プロトタイプオブジェクトを取得
     * 
     * @return Array プロトタイプオブジェクト
     */
    static std::shared_ptr<Object> getPrototype();
    
    /**
     * @brief Array コンストラクタ関数を取得
     * 
     * @return Array コンストラクタ関数
     */
    static std::shared_ptr<Function> getConstructor();
    
private:
    /**
     * @brief Array オブジェクトの初期化
     * 
     * プロトタイプとコンストラクタの設定を行います。
     */
    static void initialize();
    
    /** @brief Array プロトタイプオブジェクト */
    static std::shared_ptr<Array> s_prototype;
    
    /** @brief Array コンストラクタ関数 */
    static std::shared_ptr<Function> s_constructor;
    
    /** @brief スレッドセーフのためのミューテックス */
    static std::mutex s_mutex;
};

// スマートポインタの型エイリアス
using ArrayPtr = std::shared_ptr<Array>;

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_ARRAY_H 