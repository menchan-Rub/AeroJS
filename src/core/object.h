/**
 * @file object.h
 * @brief AeroJS JavaScript エンジンのオブジェクトクラスの定義
 * @version 0.1.0
 * @license MIT
 */

#pragma once

#include "../utils/containers/hashmap/hashmap.h"
#include "../utils/containers/string/string_view.h"
#include "../utils/memory/smart_ptr/ref_counted.h"
#include <string>
#include <vector>
#include <functional>

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class Value;
class Function;
class String;
class Symbol;
class Array;
class Date;
class RegExp;
class Error;
class BigInt;

/**
 * @brief オブジェクトのプロパティ属性を定義する列挙型
 */
enum PropertyAttribute {
    None        = 0,
    Writable    = 1 << 0, // 書き込み可能かどうか
    Enumerable  = 1 << 1, // 列挙可能かどうか
    Configurable = 1 << 2, // 設定変更可能かどうか
    Accessor    = 1 << 3, // アクセサプロパティかどうか
    Default     = Writable | Enumerable | Configurable, // デフォルト属性
};

/**
 * @brief プロパティディスクリプタ
 */
struct PropertyDescriptor {
    Value* value = nullptr;              // プロパティの値
    Value* get = nullptr;                // getter関数
    Value* set = nullptr;                // setter関数
    uint32_t attributes = PropertyAttribute::Default;  // 属性フラグ
    
    bool hasValue() const { return value != nullptr; }
    bool hasGet() const { return get != nullptr; }
    bool hasSet() const { return set != nullptr; }
    bool isAccessor() const { return (attributes & PropertyAttribute::Accessor) != 0; }
    bool isWritable() const { return (attributes & PropertyAttribute::Writable) != 0; }
    bool isEnumerable() const { return (attributes & PropertyAttribute::Enumerable) != 0; }
    bool isConfigurable() const { return (attributes & PropertyAttribute::Configurable) != 0; }
};

/**
 * @brief プロパティキー（文字列またはシンボル）
 */
class PropertyKey {
public:
    enum KeyType {
        String,
        Symbol,
        Integer
    };

    PropertyKey() : m_type(Integer), m_intKey(0) {}
    
    PropertyKey(uint32_t index) : m_type(Integer), m_intKey(index) {}
    
    PropertyKey(const std::string& str) : m_type(String), m_stringKey(str) {}
    
    PropertyKey(const utils::StringView& view) : m_type(String), m_stringKey(view.toString()) {}
    
    PropertyKey(class Symbol* sym) : m_type(Symbol), m_symbolPtr(sym) {}
    
    KeyType getType() const { return m_type; }
    
    bool isString() const { return m_type == String; }
    bool isSymbol() const { return m_type == Symbol; }
    bool isInteger() const { return m_type == Integer; }
    
    const std::string& asString() const { return m_stringKey; }
    class Symbol* asSymbol() const { return m_symbolPtr; }
    uint32_t asInteger() const { return m_intKey; }
    
    bool operator==(const PropertyKey& other) const;
    bool operator!=(const PropertyKey& other) const { return !(*this == other); }
    
    // ハッシュ関数のためのメソッド
    size_t hash() const;
    
    std::string toString() const;

private:
    KeyType m_type;
    
    // 共用体を使用して効率的にメモリを使用
    union {
        std::string m_stringKey;
        class Symbol* m_symbolPtr;
        uint32_t m_intKey;
    };
};

/**
 * @brief JavaScriptオブジェクトを表すクラス
 */
class Object : public utils::RefCounted {
public:
    explicit Object(Context* ctx);
    virtual ~Object();

    /**
     * @brief オブジェクトの型を判別するための列挙型
     */
    enum class Type {
        Object,
        Function,
        Array,
        String,
        Boolean,
        Number,
        Date,
        RegExp,
        Error,
        BigInt,
        Map,
        Set,
        Promise,
        Proxy,
        TypedArray,
        ArrayBuffer,
        DataView
    };

    /**
     * @brief オブジェクトの型を取得
     * @return オブジェクトの型
     */
    virtual Type getType() const { return Type::Object; }

    /**
     * @brief オブジェクトの型を判別するヘルパーメソッド
     */
    bool isFunction() const { return getType() == Type::Function; }
    bool isArray() const { return getType() == Type::Array; }
    bool isString() const { return getType() == Type::String; }
    bool isBoolean() const { return getType() == Type::Boolean; }
    bool isNumber() const { return getType() == Type::Number; }
    bool isDate() const { return getType() == Type::Date; }
    bool isRegExp() const { return getType() == Type::RegExp; }
    bool isError() const { return getType() == Type::Error; }
    bool isBigInt() const { return getType() == Type::BigInt; }
    bool isMap() const { return getType() == Type::Map; }
    bool isSet() const { return getType() == Type::Set; }
    bool isPromise() const { return getType() == Type::Promise; }
    bool isProxy() const { return getType() == Type::Proxy; }
    bool isTypedArray() const { return getType() == Type::TypedArray; }
    bool isArrayBuffer() const { return getType() == Type::ArrayBuffer; }
    bool isDataView() const { return getType() == Type::DataView; }

    /**
     * @brief コンテキストを取得
     * @return オブジェクトが属するコンテキスト
     */
    Context* getContext() const { return m_context; }

    /**
     * @brief プロトタイプを取得
     * @return プロトタイプオブジェクト
     */
    Object* getPrototype() const { return m_prototype; }

    /**
     * @brief プロトタイプを設定
     * @param proto 新しいプロトタイプオブジェクト
     */
    void setPrototype(Object* proto);

    /**
     * @brief プロパティの設定
     * @param key プロパティキー
     * @param value プロパティ値
     * @param attributes プロパティ属性
     * @return 成功したらtrue
     */
    bool setProperty(const PropertyKey& key, Value* value, uint32_t attributes = PropertyAttribute::Default);

    /**
     * @brief プロパティの設定（文字列キー）
     * @param key プロパティ名
     * @param value プロパティ値
     * @param attributes プロパティ属性
     * @return 成功したらtrue
     */
    bool setProperty(const std::string& key, Value* value, uint32_t attributes = PropertyAttribute::Default);

    /**
     * @brief プロパティの設定（数値インデックス）
     * @param index 配列インデックス
     * @param value プロパティ値
     * @param attributes プロパティ属性
     * @return 成功したらtrue
     */
    bool setProperty(uint32_t index, Value* value, uint32_t attributes = PropertyAttribute::Default);

    /**
     * @brief プロパティの設定（シンボルキー）
     * @param symbol シンボルキー
     * @param value プロパティ値
     * @param attributes プロパティ属性
     * @return 成功したらtrue
     */
    bool setProperty(Symbol* symbol, Value* value, uint32_t attributes = PropertyAttribute::Default);

    /**
     * @brief プロパティの取得
     * @param key プロパティキー
     * @return プロパティ値、存在しない場合はnullptr
     */
    Value* getProperty(const PropertyKey& key) const;

    /**
     * @brief プロパティの取得（文字列キー）
     * @param key プロパティ名
     * @return プロパティ値、存在しない場合はnullptr
     */
    Value* getProperty(const std::string& key) const;

    /**
     * @brief プロパティの取得（数値インデックス）
     * @param index 配列インデックス
     * @return プロパティ値、存在しない場合はnullptr
     */
    Value* getProperty(uint32_t index) const;

    /**
     * @brief プロパティの取得（シンボルキー）
     * @param symbol シンボルキー
     * @return プロパティ値、存在しない場合はnullptr
     */
    Value* getProperty(Symbol* symbol) const;

    /**
     * @brief プロパティの削除
     * @param key プロパティキー
     * @return 成功したらtrue
     */
    bool deleteProperty(const PropertyKey& key);

    /**
     * @brief プロパティの存在確認
     * @param key プロパティキー
     * @return 存在する場合はtrue
     */
    bool hasProperty(const PropertyKey& key) const;

    /**
     * @brief 自身のプロパティの存在確認
     * @param key プロパティキー
     * @return 自身に存在する場合はtrue
     */
    bool hasOwnProperty(const PropertyKey& key) const;

    /**
     * @brief プロパティディスクリプタの取得
     * @param key プロパティキー
     * @return プロパティディスクリプタ
     */
    PropertyDescriptor getOwnPropertyDescriptor(const PropertyKey& key) const;

    /**
     * @brief プロパティの定義
     * @param key プロパティキー
     * @param desc プロパティディスクリプタ
     * @return 成功したらtrue
     */
    bool defineProperty(const PropertyKey& key, const PropertyDescriptor& desc);

    /**
     * @brief 自身のプロパティキーの一覧取得
     * @param includeNonEnumerable 列挙不可のプロパティも含めるか
     * @return プロパティキーの配列
     */
    std::vector<PropertyKey> getOwnPropertyKeys(bool includeNonEnumerable = false) const;

    /**
     * @brief 列挙可能なプロパティキーの一覧取得
     * @return 列挙可能なプロパティキーの配列
     */
    std::vector<PropertyKey> getEnumerablePropertyKeys() const;

    /**
     * @brief 文字列への変換
     * @return オブジェクトの文字列表現
     */
    virtual std::string toString() const;

    /**
     * @brief 数値への変換
     * @return オブジェクトの数値表現
     */
    virtual double toNumber() const;

    /**
     * @brief プリミティブ値への変換
     * @param preferredType 優先する型のヒント ("default", "string", "number")
     * @return プリミティブ値
     */
    virtual Value* toPrimitive(const std::string& preferredType = "default") const;

    /**
     * @brief ガベージコレクションのためのマーク処理
     */
    virtual void mark();

    /**
     * @brief カスタムデータの設定
     * @param key カスタムデータのキー
     * @param data カスタムデータ
     * @param deleter データ削除用関数（オプション）
     */
    template <typename T>
    void setCustomData(const std::string& key, T* data, std::function<void(void*)> deleter = nullptr) {
        m_customData[key] = {data, deleter ? deleter : [](void* p) { delete static_cast<T*>(p); }};
    }

    /**
     * @brief カスタムデータの取得
     * @param key カスタムデータのキー
     * @return カスタムデータ、存在しない場合はnullptr
     */
    template <typename T>
    T* getCustomData(const std::string& key) const {
        auto it = m_customData.find(key);
        if (it != m_customData.end()) {
            return static_cast<T*>(it->second.data);
        }
        return nullptr;
    }

    /**
     * @brief カスタムデータの削除
     * @param key カスタムデータのキー
     * @return 成功したらtrue
     */
    bool removeCustomData(const std::string& key);

private:
    struct CustomDataEntry {
        void* data;
        std::function<void(void*)> deleter;
        
        ~CustomDataEntry() {
            if (data && deleter) {
                deleter(data);
            }
        }
    };

    // コンテキスト
    Context* m_context;
    
    // プロトタイプオブジェクト
    Object* m_prototype;
    
    // プロパティマップ
    utils::HashMap<PropertyKey, PropertyDescriptor> m_properties;
    
    // カスタムデータ
    std::unordered_map<std::string, CustomDataEntry> m_customData;
};

} // namespace core
} // namespace aerojs

// std名前空間で特殊化を定義
namespace std {

/**
 * @brief PropertyKeyのためのハッシュ関数特殊化
 */
template<>
struct hash<aerojs::core::PropertyKey> {
    size_t operator()(const aerojs::core::PropertyKey& key) const {
        return key.hash();
    }
};

} // namespace std 