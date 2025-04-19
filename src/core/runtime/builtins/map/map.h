/**
 * @file map.h
 * @brief JavaScript のMap組み込みオブジェクトの定義
 * @copyright 2023 AeroJS Project
 */

#pragma once

#include "core/runtime/object.h"
#include <unordered_map>
#include <vector>
#include <utility>
#include <memory>

namespace aero {

/**
 * @brief JavaScriptのMapオブジェクトを実装するクラス
 * 
 * JavaScriptのMapは任意の値をキーとして使用できる連想配列です。
 * 通常のオブジェクトとは異なり、プリミティブ値やオブジェクトをキーとして使用できます。
 */
class MapObject : public Object {
private:
    // キーと値のペアを格納するベクター（挿入順を保持するため）
    std::vector<std::pair<Value, Value>> m_entries;
    
    // キーから配列インデックスへのマッピング（高速検索用）
    std::unordered_map<Value, size_t, ValueHasher> m_keyMap;
    
    // Mapのサイズ
    size_t m_size;

public:
    /**
     * @brief 空のMapオブジェクトを構築します
     * @param prototype プロトタイプオブジェクト
     */
    explicit MapObject(Object* prototype);
    
    /**
     * @brief イテレーブルからMapオブジェクトを構築します
     * @param prototype プロトタイプオブジェクト
     * @param iterable キー/値のペアを持つイテレーブル
     */
    MapObject(Object* prototype, Value iterable);
    
    /**
     * @brief デストラクタ
     */
    ~MapObject() override;
    
    /**
     * @brief このオブジェクトの型名を返します
     * @return 型名 "Map"
     */
    const char* className() const override { return "Map"; }
    
    /**
     * @brief このオブジェクトがMapオブジェクトであるかを確認します
     * @return 常にtrue
     */
    bool isMapObject() const override { return true; }

    /**
     * @brief Mapに値を設定します
     * @param key キー
     * @param value 値
     * @return thisオブジェクト
     */
    Value set(Value key, Value value);
    
    /**
     * @brief 指定されたキーに関連付けられた値を取得します
     * @param key キー
     * @return 関連付けられた値、キーが存在しない場合はundefined
     */
    Value get(Value key) const;
    
    /**
     * @brief 指定されたキーがMapに存在するかを確認します
     * @param key キー
     * @return キーが存在する場合はtrue
     */
    bool has(Value key) const;
    
    /**
     * @brief 指定されたキーとその値をMapから削除します
     * @param key キー
     * @return 削除に成功した場合はtrue
     */
    bool remove(Value key);
    
    /**
     * @brief Mapの全てのエントリを削除します
     */
    void clear();
    
    /**
     * @brief Mapのサイズを取得します
     * @return Mapに含まれるエントリの数
     */
    size_t size() const { return m_size; }
    
    /**
     * @brief Mapのキーを配列として取得します
     * @return 全てのキーを含む配列
     */
    Value keys() const;
    
    /**
     * @brief Mapの値を配列として取得します
     * @return 全ての値を含む配列
     */
    Value values() const;
    
    /**
     * @brief Mapのエントリを配列として取得します
     * @return [key, value]ペアの配列
     */
    Value entries() const;
    
    /**
     * @brief Mapの全てのエントリに対してコールバック関数を実行します
     * @param callback 実行するコールバック関数
     * @param thisArg コールバック関数内でthisとして使用する値
     */
    void forEach(Value callback, Value thisArg);

    /**
     * @brief Map constructor
     * @param callee 呼び出し元の関数オブジェクト
     * @param thisValue thisの値
     * @param arguments 引数リスト
     * @param context 実行コンテキスト
     * @return 新しいMapオブジェクト
     */
    static Value mapConstructor(Value callee, Value thisValue, const std::vector<Value>& arguments, Context* context);
    
    /**
     * @brief Mapプロトタイプを初期化します
     * @param context 実行コンテキスト
     * @return Mapプロトタイプオブジェクト
     */
    static Value initializePrototype(Context* context);
};

/**
 * @brief Map.prototype.set実装
 */
Value mapSet(Value thisValue, const std::vector<Value>& arguments, Context* context);

/**
 * @brief Map.prototype.get実装
 */
Value mapGet(Value thisValue, const std::vector<Value>& arguments, Context* context);

/**
 * @brief Map.prototype.has実装
 */
Value mapHas(Value thisValue, const std::vector<Value>& arguments, Context* context);

/**
 * @brief Map.prototype.delete実装
 */
Value mapDelete(Value thisValue, const std::vector<Value>& arguments, Context* context);

/**
 * @brief Map.prototype.clear実装
 */
Value mapClear(Value thisValue, const std::vector<Value>& arguments, Context* context);

/**
 * @brief Map.prototype.size getter実装
 */
Value mapSize(Value thisValue, Context* context);

/**
 * @brief Map.prototype.forEach実装
 */
Value mapForEach(Value thisValue, const std::vector<Value>& arguments, Context* context);

/**
 * @brief Map.prototype.keys実装
 */
Value mapKeys(Value thisValue, const std::vector<Value>& arguments, Context* context);

/**
 * @brief Map.prototype.values実装
 */
Value mapValues(Value thisValue, const std::vector<Value>& arguments, Context* context);

/**
 * @brief Map.prototype.entries実装
 */
Value mapEntries(Value thisValue, const std::vector<Value>& arguments, Context* context);

/**
 * @brief MapオブジェクトのためのValueHasherクラス
 */
class ValueHasher {
public:
    size_t operator()(const Value& key) const;
};

} // namespace aero 