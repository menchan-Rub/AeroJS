/**
 * @file weakmap.h
 * @brief JavaScript のWeakMap組み込みオブジェクトの定義
 * @copyright 2023 AeroJS Project
 */

#ifndef AERO_WEAKMAP_H
#define AERO_WEAKMAP_H

#include "core/runtime/object.h"
#include "core/runtime/value.h"
#include <unordered_map>
#include <memory>

namespace aero {

class Context;

/**
 * @class WeakMapObject
 * @brief JavaScript の WeakMap オブジェクトを実装するクラス
 * 
 * WeakMapはキーとしてオブジェクトのみを持ち、そのオブジェクトへの参照が弱参照となるコレクションです。
 * キーとなるオブジェクトが到達不能になると、対応する値もガベージコレクションの対象となります。
 */
class WeakMapObject : public Object {
public:
    /**
     * @brief コンストラクタ
     * @param prototype プロトタイプオブジェクト
     */
    explicit WeakMapObject(Object* prototype);
    
    /**
     * @brief イテレーブルから初期化するコンストラクタ
     * @param prototype プロトタイプオブジェクト
     * @param iterable 初期化に使用するイテレーブル
     */
    WeakMapObject(Object* prototype, Value iterable);
    
    /**
     * @brief デストラクタ
     */
    ~WeakMapObject() override;
    
    /**
     * @brief 指定されたキーに値を設定する
     * @param key キー（オブジェクトでなければならない）
     * @param value 設定する値
     * @return このWeakMapオブジェクト
     * @throw TypeError キーがオブジェクトでない場合
     */
    Value set(Value key, Value value);
    
    /**
     * @brief 指定されたキーに関連付けられた値を取得する
     * @param key キー
     * @return 関連付けられた値。キーが存在しない場合はundefined
     * @throw TypeError キーがオブジェクトでない場合
     */
    Value get(Value key) const;
    
    /**
     * @brief 指定されたキーが存在するかどうかを確認する
     * @param key キー
     * @return キーが存在する場合はtrue、そうでない場合はfalse
     * @throw TypeError キーがオブジェクトでない場合
     */
    bool has(Value key) const;
    
    /**
     * @brief 指定されたキーとそれに関連付けられた値を削除する
     * @param key キー
     * @return 値が削除された場合はtrue、キーが存在しなかった場合はfalse
     * @throw TypeError キーがオブジェクトでない場合
     */
    bool remove(Value key);
    
    /**
     * @brief このオブジェクトがWeakMapかどうかを確認する
     * @return 常にtrue
     */
    bool isWeakMapObject() const override { return true; }
    
    /**
     * @brief WeakMap コンストラクタ関数
     * @param callee 呼び出された関数オブジェクト
     * @param thisValue this値
     * @param arguments 引数リスト
     * @param context 実行コンテキスト
     * @return 新しいWeakMapオブジェクト
     */
    static Value weakMapConstructor(Value callee, Value thisValue, const std::vector<Value>& arguments, Context* context);
    
    /**
     * @brief WeakMapプロトタイプを初期化する
     * @param context 実行コンテキスト
     * @return 初期化されたプロトタイプオブジェクト
     */
    static Value initializePrototype(Context* context);

private:
    /**
     * @brief キーが有効かどうかを確認する（オブジェクトであること）
     * @param key 確認するキー
     * @return キーが有効な場合はtrue
     * @throw TypeError キーがオブジェクトでない場合
     */
    static bool validateKey(Value key);
    
    /**
     * @class ObjectWeakPtr
     * @brief オブジェクトへの弱参照を保持するためのラッパークラス
     */
    class ObjectWeakPtr {
    public:
        /**
         * @brief コンストラクタ
         * @param obj 弱参照として保持するオブジェクト
         */
        explicit ObjectWeakPtr(Object* obj) : m_ptr(obj) {}
        
        /**
         * @brief 保持しているオブジェクトを取得する
         * @return オブジェクトへのポインタ。オブジェクトが破棄されていればnullptr
         */
        Object* get() const { return m_ptr.lock().get(); }
        
        /**
         * @brief 保持しているオブジェクトのハッシュ値を取得する
         * @return ハッシュ値
         */
        size_t hash() const { return m_hash; }
        
        /**
         * @brief 等価比較演算子
         * @param other 比較対象
         * @return 同じオブジェクトを参照している場合はtrue
         */
        bool operator==(const ObjectWeakPtr& other) const {
            // ハッシュが異なれば確実に異なるオブジェクト
            if (m_hash != other.m_hash) {
                return false;
            }
            
            // どちらかがexpireしている場合は不一致
            auto thisPtr = m_ptr.lock();
            auto otherPtr = other.m_ptr.lock();
            if (!thisPtr || !otherPtr) {
                return false;
            }
            
            // 同じオブジェクトを指しているかチェック
            return thisPtr.get() == otherPtr.get();
        }
        
    private:
        std::weak_ptr<Object> m_ptr;  ///< オブジェクトへの弱参照
        size_t m_hash;  ///< オブジェクトのハッシュ値（固定）
    };
    
    /**
     * @struct WeakPtrHasher
     * @brief ObjectWeakPtrのハッシュ計算用関数オブジェクト
     */
    struct WeakPtrHasher {
        /**
         * @brief ハッシュ値を計算する
         * @param ptr ハッシュ値を計算するObjectWeakPtr
         * @return ハッシュ値
         */
        size_t operator()(const ObjectWeakPtr& ptr) const {
            return ptr.hash();
        }
    };
    
    /// キーとなるオブジェクトの弱参照と値のマップ
    std::unordered_map<ObjectWeakPtr, Value, WeakPtrHasher> m_entries;
};

// WeakMapプロトタイプメソッド

/**
 * @brief WeakMap.prototype.delete 実装
 * @param thisValue this値
 * @param arguments 引数リスト
 * @param context 実行コンテキスト
 * @return 削除が成功したかどうかを示す真偽値
 */
Value weakMapDelete(Value thisValue, const std::vector<Value>& arguments, Context* context);

/**
 * @brief WeakMap.prototype.get 実装
 * @param thisValue this値
 * @param arguments 引数リスト
 * @param context 実行コンテキスト
 * @return キーに関連付けられた値、またはundefined
 */
Value weakMapGet(Value thisValue, const std::vector<Value>& arguments, Context* context);

/**
 * @brief WeakMap.prototype.has 実装
 * @param thisValue this値
 * @param arguments 引数リスト
 * @param context 実行コンテキスト
 * @return キーが存在するかどうかを示す真偽値
 */
Value weakMapHas(Value thisValue, const std::vector<Value>& arguments, Context* context);

/**
 * @brief WeakMap.prototype.set 実装
 * @param thisValue this値
 * @param arguments 引数リスト
 * @param context 実行コンテキスト
 * @return WeakMapオブジェクト自身
 */
Value weakMapSet(Value thisValue, const std::vector<Value>& arguments, Context* context);

} // namespace aero

#endif // AERO_WEAKMAP_H 