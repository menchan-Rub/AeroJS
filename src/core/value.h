/**
 * @file value.h
 * @brief AeroJS JavaScript値シスチEヘッダー
 * @version 0.2.0
 * @license MIT
 */

#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include <functional>
#include <vector>
#include <unordered_map>

namespace aerojs {
namespace core {

/**
 * @brief JavaScript値の垁E
 */
enum class ValueType {
    Undefined,
    Null,
    Boolean,
    Number,
    String,
    Symbol,
    BigInt,
    Object,
    Array,
    Function,
    RegExp,
    Date,
    Error,
    Promise,
    Map,
    Set,
    WeakMap,
    WeakSet,
    ArrayBuffer,
    DataView,
    Int8Array,
    Uint8Array,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array,
    BigInt64Array,
    BigUint64Array
};

/**
 * @brief 値の比輁E果
 */
enum class ComparisonResult {
    LessThan,
    Equal,
    GreaterThan,
    Undefined  // NaN比輁Eど
};

/**
 * @brief JavaScript値を表現するクラス
 */
class Value {
public:
    // コンストラクタ
    Value();
    Value(const Value& other);
    Value(Value&& other) noexcept;
    explicit Value(bool value);
    explicit Value(int32_t value);
    explicit Value(double value);
    explicit Value(const std::string& value);
    explicit Value(const char* value);

    // チEトラクタ
    ~Value();

    // 代入演算子
    Value& operator=(const Value& other);
    Value& operator=(Value&& other) noexcept;

    // 型チェチE
    bool isUndefined() const;
    bool isNull() const;
    bool isNullish() const;  // null またE undefined
    bool isBoolean() const;
    bool isNumber() const;
    bool isInteger() const;
    bool isFinite() const;
    bool isNaN() const;
    bool isString() const;
    bool isSymbol() const;
    bool isBigInt() const;
    bool isObject() const;
    bool isArray() const;
    bool isFunction() const;
    bool isCallable() const;
    bool isPrimitive() const;
    bool isTruthy() const;
    bool isFalsy() const;

    // 型変換
    bool toBoolean() const;
    double toNumber() const;
    int32_t toInt32() const;
    uint32_t toUint32() const;
    int64_t toInt64() const;
    uint64_t toUint64() const;
    std::string toString() const;
    std::string toStringRepresentation() const;  // チEチE用
    void* toObject() const;

    // 安Eな型変換E例外なし！E
    bool tryToBoolean(bool& result) const;
    bool tryToNumber(double& result) const;
    bool tryToInt32(int32_t& result) const;
    bool tryToString(std::string& result) const;

    // アクセサ
    ValueType getType() const;
    const char* getTypeName() const;
    size_t getSize() const;  // メモリサイズ
    uint64_t getHash() const;  // ハッシュ値

    // 比輁E箁E
    bool equals(const Value& other) const;  // ==
    bool strictEquals(const Value& other) const;  // ===
    bool sameValue(const Value& other) const;  // Object.is
    ComparisonResult compare(const Value& other) const;  // <, >, <=, >=

    // 演算子オーバEローチE
    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const;
    bool operator<(const Value& other) const;
    bool operator<=(const Value& other) const;
    bool operator>(const Value& other) const;
    bool operator>=(const Value& other) const;

    // 配E操作（E列E場合EみE
    Value getElement(size_t index) const;
    void setElement(size_t index, const Value& value);
    size_t getLength() const;
    void push(const Value& value);
    Value pop();

    // オブジェクト操作（オブジェクトE場合EみE
    Value getProperty(const std::string& name) const;
    void setProperty(const std::string& name, const Value& value);
    bool hasProperty(const std::string& name) const;
    void deleteProperty(const std::string& name);
    std::vector<std::string> getPropertyNames() const;

    // 関数呼び出し（関数の場合EみE
    Value call(const std::vector<Value>& args) const;
    Value call(const Value& thisValue, const std::vector<Value>& args) const;

    // ユーチEリチE
    Value clone() const;
    void freeze();
    bool isFrozen() const;
    void seal();
    bool isSealed() const;
    bool isExtensible() const;
    void preventExtensions();

    // チEチE・診断
    std::string inspect() const;
    void dump() const;
    bool isValid() const;

    // メモリ管琁E
    void markForGC();
    void unmarkForGC();
    bool isMarkedForGC() const;
    void incrementRefCount();
    void decrementRefCount();
    size_t getRefCount() const;

    // 静的ファクトリメソッド
    static Value undefined();
    static Value null();
    static Value fromBoolean(bool value);
    static Value fromNumber(double value);
    static Value fromString(const std::string& value);
    static Value fromObject(void* object);
    static Value fromInteger(int32_t value);
    static Value fromArray(const std::vector<Value>& values);
    static Value fromFunction(void* function);
    static Value fromSymbol(void* symbol);

private:
    ValueType type_;
    
    // 値の格納用union
    union {
        bool boolValue_;
        double numberValue_;
        std::string* stringValue_;
        void* pointerValue_;
    };

    // プロパティ属性
    bool writable_ : 1;
    bool enumerable_ : 1;
    bool configurable_ : 1;
    
    // オブジェクト状慁E
    bool frozen_ : 1;
    bool sealed_ : 1;
    bool extensible_ : 1;
    
    // ガベEジコレクション
    bool markedForGC_ : 1;
    size_t refCount_;
    
    // ハッシュ値キャチEュ
    mutable size_t hashValue_;
    mutable bool hashComputed_;

    // 冁EヘルパEメソチE
    void cleanup();
    void copyFrom(const Value& other);
    void moveFrom(Value&& other);
    double stringToNumber(const std::string& str) const;
    std::string numberToString(double num) const;
    ComparisonResult abstractComparison(const Value& other) const;
    uint64_t computeHash() const;
};

} // namespace core
} // namespace aerojs

