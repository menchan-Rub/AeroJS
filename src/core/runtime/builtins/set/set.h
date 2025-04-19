/**
 * @file set.h
 * @brief JavaScriptのSet組み込みオブジェクトの定義
 * @copyright Copyright (c) 2023 AeroJS project authors
 */

#ifndef AERO_SET_H
#define AERO_SET_H

#include "core/runtime/object.h"
#include "core/runtime/value.h"
#include <unordered_set>
#include <vector>

namespace aero {

class Context;
class GlobalObject;

/**
 * @brief 値のハッシュ計算を行うためのハッシュ関数オブジェクト
 * 異なるタイプの JavaScript 値に対して適切なハッシュ値を生成します
 */
class ValueHasher {
public:
    /**
     * @brief JavaScript の Value オブジェクトのハッシュ値を計算します
     * @param value ハッシュ値を計算する JavaScript の値
     * @return 計算されたハッシュ値
     */
    size_t operator()(const Value& value) const;
    
    /**
     * @brief 二つの JavaScript の値が等しいかを判定します
     * @param lhs 比較する左辺値
     * @param rhs 比較する右辺値
     * @return 二つの値が等しい場合は true、そうでない場合は false
     */
    bool operator()(const Value& lhs, const Value& rhs) const {
        return lhs.equals(rhs);
    }
};

/**
 * @class SetObject
 * @brief JavaScriptのSetオブジェクトを表すクラス
 * 
 * SetObjectはユニークな値のコレクションを提供します。
 * ECMAScript仕様に準拠した実装です。
 */
class SetObject : public Object {
public:
    /**
     * @brief コンストラクタ
     * @param context 実行コンテキスト
     * @param prototype プロトタイプオブジェクト
     */
    SetObject(Context* context, Object* prototype);

    /**
     * @brief デストラクタ
     */
    ~SetObject() override;
    
    /**
     * @brief Setに値を追加
     * @param value 追加する値
     * @return this
     */
    Value add(const Value& value);
    
    /**
     * @brief Setが指定値を持つか確認
     * @param value 確認する値
     * @return 含まれていればtrue
     */
    bool has(const Value& value) const;
    
    /**
     * @brief Setから値を削除
     * @param value 削除する値
     * @return 削除が成功したらtrue
     */
    bool remove(const Value& value);
    
    /**
     * @brief Set内のすべての値を削除
     */
    void clear();
    
    /**
     * @brief Set内の値の数を取得
     * @return 値の数
     */
    size_t size() const;
    
    /**
     * @brief Set内の全値を配列で取得
     * @return 値の配列
     */
    Value values(Context* context) const;
    
    /**
     * @brief Setのプロトタイプを初期化
     * @param context 実行コンテキスト
     * @return プロトタイプオブジェクト
     */
    static Value initializePrototype(Context* context);

    /**
     * @brief このオブジェクトがSetオブジェクトであるかを確認します
     * @return 常にtrue
     */
    bool isSetObject() const override { return true; }

private:
    /** @brief Setに格納されている値のコンテナ */
    std::unordered_set<Value, ValueHasher, ValueHasher> m_values;
};

/**
 * @brief Setコンストラクタ関数
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 新しいSetオブジェクト
 */
Value setConstructor(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Set.prototype.add実装
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return thisオブジェクト
 */
Value setAdd(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Set.prototype.clear実装
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return undefined
 */
Value setClear(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Set.prototype.delete実装
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 削除成功したらtrue
 */
Value setDelete(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Set.prototype.has実装
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 要素が含まれていればtrue
 */
Value setHas(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Set.prototype.forEach実装
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return undefined
 */
Value setForEach(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Set.prototype.values実装
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return イテレータオブジェクト
 */
Value setValues(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Set.prototype.keys実装（valuesと同じ）
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return イテレータオブジェクト
 */
Value setKeys(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Set.prototype.entries実装
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return イテレータオブジェクト
 */
Value setEntries(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Set.prototype[Symbol.iterator]実装（valuesと同じ）
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return イテレータオブジェクト
 */
Value setIterator(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Set.prototype.size取得関数
 * @param context 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 要素数
 */
Value setSize(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Setオブジェクトを初期化
 * @param context 実行コンテキスト
 * @return Setコンストラクタ
 */
Value initializeSet(Context* context);

/**
 * @brief Set組み込みオブジェクトをグローバルオブジェクトに登録
 * @param global グローバルオブジェクト
 */
void registerSetBuiltin(GlobalObject* global);

} // namespace aero

#endif // AERO_SET_H 