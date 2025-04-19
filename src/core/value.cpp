/**
 * @file value.cpp
 * @brief AeroJS JavaScript エンジンの値クラスの実装
 * @version 0.1.0
 * @license MIT
 */

#include "value.h"
#include "context.h"
#include "object.h"
#include "function.h"
#include "array.h"
#include "string.h"
#include "symbol.h"
#include "bigint.h"
#include "error.h"
#include "regexp.h"
#include "date.h"
#include <cmath>
#include <limits>

namespace aerojs {
namespace core {

// NaNボクシングの実装の詳細
// IEEE-754倍精度浮動小数点数の特性を利用して、64ビット内にすべての値型を格納する
// NaNの場合、仮数部にタグと参照を格納できる

Value::Value(Context* ctx)
    : m_flags(0)
    , m_refCount(0)
    , m_context(ctx) {
    m_value.m_bits = 0;
}

Value* Value::createUndefined(Context* ctx) {
    auto* engine = ctx->getEngine();
    auto* allocator = engine->getMemoryAllocator();
    
    auto* value = new (allocator->allocate(sizeof(Value))) Value(ctx);
    value->setTag(TAG_UNDEFINED);
    return value;
}

Value* Value::createNull(Context* ctx) {
    auto* engine = ctx->getEngine();
    auto* allocator = engine->getMemoryAllocator();
    
    auto* value = new (allocator->allocate(sizeof(Value))) Value(ctx);
    value->setTag(TAG_NULL);
    return value;
}

Value* Value::createBoolean(Context* ctx, bool boolValue) {
    auto* engine = ctx->getEngine();
    auto* allocator = engine->getMemoryAllocator();
    
    auto* value = new (allocator->allocate(sizeof(Value))) Value(ctx);
    value->setTag(TAG_BOOLEAN, reinterpret_cast<void*>(boolValue ? 1ULL : 0ULL));
    return value;
}

Value* Value::createNumber(Context* ctx, double numValue) {
    // 数値がNaNの場合は特別な処理が必要
    if (std::isnan(numValue)) {
        numValue = std::numeric_limits<double>::quiet_NaN();
    }
    
    auto* engine = ctx->getEngine();
    auto* allocator = engine->getMemoryAllocator();
    
    auto* value = new (allocator->allocate(sizeof(Value))) Value(ctx);
    value->setNumber(numValue);
    return value;
}

Value* Value::createString(Context* ctx, const std::string& strValue) {
    auto* engine = ctx->getEngine();
    auto* allocator = engine->getMemoryAllocator();
    
    // String オブジェクトを作成
    auto* stringObj = new (allocator->allocate(sizeof(String))) String(ctx, strValue);
    
    // Value オブジェクトを作成して、String オブジェクトを参照
    auto* value = new (allocator->allocate(sizeof(Value))) Value(ctx);
    value->setTag(TAG_STRING, stringObj);
    return value;
}

Value* Value::createString(Context* ctx, const utils::StringView& strView) {
    return createString(ctx, strView.toString());
}

Value* Value::createString(Context* ctx, const char* strValue) {
    if (!strValue) {
        return createString(ctx, "");
    }
    return createString(ctx, std::string(strValue));
}

Value* Value::createSymbol(Context* ctx, const std::string& description) {
    auto* engine = ctx->getEngine();
    auto* allocator = engine->getMemoryAllocator();
    
    // Symbol オブジェクトを作成
    auto* symbolObj = new (allocator->allocate(sizeof(Symbol))) Symbol(ctx, description);
    
    // Value オブジェクトを作成して、Symbol オブジェクトを参照
    auto* value = new (allocator->allocate(sizeof(Value))) Value(ctx);
    value->setTag(TAG_SYMBOL, symbolObj);
    return value;
}

Value* Value::createObject(Context* ctx) {
    auto* engine = ctx->getEngine();
    auto* allocator = engine->getMemoryAllocator();
    
    // Object オブジェクトを作成
    auto* objectObj = new (allocator->allocate(sizeof(Object))) Object(ctx);
    
    // Value オブジェクトを作成して、Object オブジェクトを参照
    auto* value = new (allocator->allocate(sizeof(Value))) Value(ctx);
    value->setTag(TAG_OBJECT, objectObj);
    return value;
}

Value* Value::createArray(Context* ctx, uint32_t length) {
    auto* engine = ctx->getEngine();
    auto* allocator = engine->getMemoryAllocator();
    
    // Array オブジェクトを作成
    auto* arrayObj = new (allocator->allocate(sizeof(Array))) Array(ctx, length);
    
    // Value オブジェクトを作成して、Array オブジェクトを参照
    auto* value = new (allocator->allocate(sizeof(Value))) Value(ctx);
    value->setTag(TAG_OBJECT, arrayObj);
    return value;
}

Value* Value::createDate(Context* ctx, double time) {
    auto* engine = ctx->getEngine();
    auto* allocator = engine->getMemoryAllocator();
    
    // Date オブジェクトを作成
    auto* dateObj = new (allocator->allocate(sizeof(Date))) Date(ctx, time);
    
    // Value オブジェクトを作成して、Date オブジェクトを参照
    auto* value = new (allocator->allocate(sizeof(Value))) Value(ctx);
    value->setTag(TAG_OBJECT, dateObj);
    return value;
}

Value* Value::createRegExp(Context* ctx, const std::string& pattern, const std::string& flags) {
    auto* engine = ctx->getEngine();
    auto* allocator = engine->getMemoryAllocator();
    
    // RegExp オブジェクトを作成
    auto* regexpObj = new (allocator->allocate(sizeof(RegExp))) RegExp(ctx, pattern, flags);
    
    // Value オブジェクトを作成して、RegExp オブジェクトを参照
    auto* value = new (allocator->allocate(sizeof(Value))) Value(ctx);
    value->setTag(TAG_OBJECT, regexpObj);
    return value;
}

Value* Value::createError(Context* ctx, const std::string& message, const std::string& name) {
    auto* engine = ctx->getEngine();
    auto* allocator = engine->getMemoryAllocator();
    
    // Error オブジェクトを作成
    auto* errorObj = new (allocator->allocate(sizeof(Error))) Error(ctx, name, message);
    
    // Value オブジェクトを作成して、Error オブジェクトを参照
    auto* value = new (allocator->allocate(sizeof(Value))) Value(ctx);
    value->setTag(TAG_OBJECT, errorObj);
    return value;
}

Value* Value::createBigInt(Context* ctx, int64_t intValue) {
    auto* engine = ctx->getEngine();
    auto* allocator = engine->getMemoryAllocator();
    
    // BigInt オブジェクトを作成
    auto* bigintObj = new (allocator->allocate(sizeof(BigInt))) BigInt(ctx, intValue);
    
    // Value オブジェクトを作成して、BigInt オブジェクトを参照
    auto* value = new (allocator->allocate(sizeof(Value))) Value(ctx);
    value->setTag(TAG_OBJECT, bigintObj);
    return value;
}

Value* Value::createBigIntFromString(Context* ctx, const std::string& str) {
    auto* engine = ctx->getEngine();
    auto* allocator = engine->getMemoryAllocator();
    
    // BigInt オブジェクトを作成
    auto* bigintObj = new (allocator->allocate(sizeof(BigInt))) BigInt(ctx, str);
    
    // Value オブジェクトを作成して、BigInt オブジェクトを参照
    auto* value = new (allocator->allocate(sizeof(Value))) Value(ctx);
    value->setTag(TAG_OBJECT, bigintObj);
    return value;
}

Value* Value::fromObject(Object* obj) {
    if (!obj) return nullptr;
    
    auto* ctx = obj->getContext();
    auto* engine = ctx->getEngine();
    auto* allocator = engine->getMemoryAllocator();
    
    auto* value = new (allocator->allocate(sizeof(Value))) Value(ctx);
    value->setTag(TAG_OBJECT, obj);
    return value;
}

ValueType Value::getType() const {
    if (isNumber()) {
        return ValueType::Number;
    }
    
    uint64_t tag = getTag();
    
    switch (tag) {
        case TAG_UNDEFINED:
            return ValueType::Undefined;
        case TAG_NULL:
            return ValueType::Null;
        case TAG_BOOLEAN:
            return ValueType::Boolean;
        case TAG_STRING:
            return ValueType::String;
        case TAG_SYMBOL:
            return ValueType::Symbol;
        case TAG_OBJECT: {
            Object* obj = static_cast<Object*>(getPayload());
            if (!obj) return ValueType::Object;
            
            if (obj->isFunction()) return ValueType::Function;
            if (obj->isArray()) return ValueType::Array;
            if (obj->isDate()) return ValueType::Date;
            if (obj->isRegExp()) return ValueType::RegExp;
            if (obj->isError()) return ValueType::Error;
            if (obj->isBigInt()) return ValueType::BigInt;
            
            return ValueType::Object;
        }
        default:
            return ValueType::Object; // デフォルトとしてObject型を返す
    }
}

bool Value::isUndefined() const {
    return getTag() == TAG_UNDEFINED;
}

bool Value::isNull() const {
    return getTag() == TAG_NULL;
}

bool Value::isBoolean() const {
    return getTag() == TAG_BOOLEAN;
}

bool Value::isNumber() const {
    return (m_value.m_bits & QUIET_NAN_MASK) != QUIET_NAN_MASK;
}

bool Value::isString() const {
    return getTag() == TAG_STRING;
}

bool Value::isSymbol() const {
    return getTag() == TAG_SYMBOL;
}

bool Value::isObject() const {
    if (getTag() != TAG_OBJECT) return false;
    Object* obj = static_cast<Object*>(getPayload());
    return obj && !obj->isFunction() && !obj->isArray() && !obj->isDate() && 
           !obj->isRegExp() && !obj->isError() && !obj->isBigInt();
}

bool Value::isFunction() const {
    if (getTag() != TAG_OBJECT) return false;
    Object* obj = static_cast<Object*>(getPayload());
    return obj && obj->isFunction();
}

bool Value::isArray() const {
    if (getTag() != TAG_OBJECT) return false;
    Object* obj = static_cast<Object*>(getPayload());
    return obj && obj->isArray();
}

bool Value::isDate() const {
    if (getTag() != TAG_OBJECT) return false;
    Object* obj = static_cast<Object*>(getPayload());
    return obj && obj->isDate();
}

bool Value::isRegExp() const {
    if (getTag() != TAG_OBJECT) return false;
    Object* obj = static_cast<Object*>(getPayload());
    return obj && obj->isRegExp();
}

bool Value::isError() const {
    if (getTag() != TAG_OBJECT) return false;
    Object* obj = static_cast<Object*>(getPayload());
    return obj && obj->isError();
}

bool Value::isBigInt() const {
    if (getTag() != TAG_OBJECT) return false;
    Object* obj = static_cast<Object*>(getPayload());
    return obj && obj->isBigInt();
}

bool Value::isPrimitive() const {
    uint64_t tag = getTag();
    return isNumber() || tag == TAG_UNDEFINED || tag == TAG_NULL || 
           tag == TAG_BOOLEAN || tag == TAG_STRING || tag == TAG_SYMBOL;
}

bool Value::toBoolean() const {
    switch (getTag()) {
        case TAG_UNDEFINED:
        case TAG_NULL:
            return false;
        case TAG_BOOLEAN:
            return reinterpret_cast<uintptr_t>(getPayload()) != 0;
        case TAG_STRING: {
            String* str = static_cast<String*>(getPayload());
            return str && !str->isEmpty();
        }
        case TAG_OBJECT:
            return getPayload() != nullptr;
        default:
            if (isNumber()) {
                double num = getNumber();
                return num != 0 && !std::isnan(num);
            }
            return true;
    }
}

double Value::toNumber() const {
    switch (getTag()) {
        case TAG_UNDEFINED:
            return std::numeric_limits<double>::quiet_NaN();
        case TAG_NULL:
            return 0.0;
        case TAG_BOOLEAN:
            return reinterpret_cast<uintptr_t>(getPayload()) != 0 ? 1.0 : 0.0;
        case TAG_STRING: {
            String* str = static_cast<String*>(getPayload());
            if (!str) return 0.0;
            
            try {
                return std::stod(str->toString());
            } catch (...) {
                return std::numeric_limits<double>::quiet_NaN();
            }
        }
        case TAG_SYMBOL:
            return std::numeric_limits<double>::quiet_NaN();
        case TAG_OBJECT: {
            // オブジェクトの数値変換は複雑なので、ここでは基本実装のみ
            Object* obj = static_cast<Object*>(getPayload());
            if (!obj) return 0.0;
            
            if (obj->isBigInt()) {
                BigInt* bigint = static_cast<BigInt*>(obj);
                return bigint->toNumber();
            }
            
            if (obj->isDate()) {
                Date* date = static_cast<Date*>(obj);
                return date->getTime();
            }
            
            return std::numeric_limits<double>::quiet_NaN();
        }
        default:
            return isNumber() ? getNumber() : 0.0;
    }
}

int32_t Value::toInt32() const {
    double num = toNumber();
    
    if (std::isnan(num) || std::isinf(num)) {
        return 0;
    }
    
    if (num == 0.0) {
        return 0;
    }
    
    // IEEE-754変換アルゴリズムに従う
    num = std::fmod(num, std::pow(2, 32));
    if (num >= std::pow(2, 31)) {
        num -= std::pow(2, 32);
    } else if (num < -std::pow(2, 31)) {
        num += std::pow(2, 32);
    }
    
    return static_cast<int32_t>(num);
}

uint32_t Value::toUInt32() const {
    double num = toNumber();
    
    if (std::isnan(num) || std::isinf(num)) {
        return 0;
    }
    
    if (num == 0.0) {
        return 0;
    }
    
    // IEEE-754変換アルゴリズムに従う
    num = std::fmod(num, std::pow(2, 32));
    if (num < 0) {
        num += std::pow(2, 32);
    }
    
    return static_cast<uint32_t>(num);
}

std::string Value::toString() const {
    switch (getTag()) {
        case TAG_UNDEFINED:
            return "undefined";
        case TAG_NULL:
            return "null";
        case TAG_BOOLEAN:
            return reinterpret_cast<uintptr_t>(getPayload()) != 0 ? "true" : "false";
        case TAG_STRING: {
            String* str = static_cast<String*>(getPayload());
            return str ? str->toString() : "";
        }
        case TAG_SYMBOL: {
            Symbol* sym = static_cast<Symbol*>(getPayload());
            return sym ? sym->toString() : "Symbol()";
        }
        case TAG_OBJECT: {
            Object* obj = static_cast<Object*>(getPayload());
            if (!obj) return "[object Object]";
            
            if (obj->isFunction()) {
                Function* func = static_cast<Function*>(obj);
                return func->toString();
            }
            
            if (obj->isArray()) {
                Array* arr = static_cast<Array*>(obj);
                return arr->toString();
            }
            
            if (obj->isDate()) {
                Date* date = static_cast<Date*>(obj);
                return date->toString();
            }
            
            if (obj->isRegExp()) {
                RegExp* regexp = static_cast<RegExp*>(obj);
                return regexp->toString();
            }
            
            if (obj->isError()) {
                Error* error = static_cast<Error*>(obj);
                return error->toString();
            }
            
            if (obj->isBigInt()) {
                BigInt* bigint = static_cast<BigInt*>(obj);
                return bigint->toString();
            }
            
            return obj->toString();
        }
        default:
            if (isNumber()) {
                double num = getNumber();
                if (std::isnan(num)) return "NaN";
                if (std::isinf(num)) return num > 0 ? "Infinity" : "-Infinity";
                
                // 整数の場合は小数点以下を表示しない
                if (std::floor(num) == num && num <= std::pow(2, 53) && num >= -std::pow(2, 53)) {
                    return std::to_string(static_cast<int64_t>(num));
                }
                
                // 一般的なケース
                return std::to_string(num);
            }
            return "";
    }
}

Object* Value::toObject() const {
    switch (getTag()) {
        case TAG_OBJECT:
            return static_cast<Object*>(getPayload());
        default:
            // プリミティブ値からオブジェクトへの変換は複雑なので、ここでは単純化
            return nullptr;
    }
}

Function* Value::asFunction() const {
    if (!isFunction()) return nullptr;
    return static_cast<Function*>(getPayload());
}

Array* Value::asArray() const {
    if (!isArray()) return nullptr;
    return static_cast<Array*>(getPayload());
}

String* Value::asString() const {
    if (!isString()) return nullptr;
    return static_cast<String*>(getPayload());
}

Symbol* Value::asSymbol() const {
    if (!isSymbol()) return nullptr;
    return static_cast<Symbol*>(getPayload());
}

BigInt* Value::asBigInt() const {
    if (!isBigInt()) return nullptr;
    return static_cast<BigInt*>(getPayload());
}

Date* Value::asDate() const {
    if (!isDate()) return nullptr;
    return static_cast<Date*>(getPayload());
}

RegExp* Value::asRegExp() const {
    if (!isRegExp()) return nullptr;
    return static_cast<RegExp*>(getPayload());
}

Error* Value::asError() const {
    if (!isError()) return nullptr;
    return static_cast<Error*>(getPayload());
}

bool Value::equals(const Value* other) const {
    if (!other) return false;
    
    // 同一オブジェクト参照の場合
    if (this == other) return true;
    
    // 型が同じ場合は厳密等価で比較
    if (getType() == other->getType()) {
        return strictEquals(other);
    }
    
    // 型が異なる場合の比較ロジック（ECMAScript仕様に従う）
    // ここでは基本的な実装のみ提供
    
    // nullとundefinedは相等
    if ((isNull() && other->isUndefined()) || (isUndefined() && other->isNull())) {
        return true;
    }
    
    // 数値と文字列の比較
    if (isNumber() && other->isString()) {
        return getNumber() == other->toNumber();
    }
    
    if (isString() && other->isNumber()) {
        return toNumber() == other->getNumber();
    }
    
    // ブール値との比較
    if (isBoolean()) {
        return toNumber() == other->toNumber();
    }
    
    if (other->isBoolean()) {
        return toNumber() == other->toNumber();
    }
    
    // オブジェクトとプリミティブの比較
    if (isObject() && other->isPrimitive()) {
        // toPrimitive変換が必要だが、ここでは簡易実装
        return toString() == other->toString();
    }
    
    if (isPrimitive() && other->isObject()) {
        // toPrimitive変換が必要だが、ここでは簡易実装
        return toString() == other->toString();
    }
    
    return false;
}

bool Value::strictEquals(const Value* other) const {
    if (!other) return false;
    
    // 同一オブジェクト参照の場合
    if (this == other) return true;
    
    // 型が異なる場合は常にfalse
    if (getType() != other->getType()) {
        return false;
    }
    
    // 型ごとの厳密等価比較
    switch (getType()) {
        case ValueType::Undefined:
        case ValueType::Null:
            return true; // 同じ型のundefinedとnullは常に等しい
            
        case ValueType::Boolean:
            return (reinterpret_cast<uintptr_t>(getPayload()) != 0) == 
                   (reinterpret_cast<uintptr_t>(other->getPayload()) != 0);
            
        case ValueType::Number: {
            double a = getNumber();
            double b = other->getNumber();
            
            // +0と-0は厳密等価ではない
            if (a == 0 && b == 0) {
                return std::signbit(a) == std::signbit(b);
            }
            
            // NaNは自分自身と等しくない
            if (std::isnan(a) || std::isnan(b)) {
                return false;
            }
            
            return a == b;
        }
            
        case ValueType::String: {
            String* a = asString();
            String* b = other->asString();
            return (a && b) ? (a->toString() == b->toString()) : (a == b);
        }
            
        case ValueType::Symbol: {
            Symbol* a = asSymbol();
            Symbol* b = other->asSymbol();
            return a == b; // シンボルは同一性で比較
        }
            
        default: {
            // オブジェクト型は参照の同一性で比較
            return getPayload() == other->getPayload();
        }
    }
}

void Value::mark() {
    if (hasFlag(Flag_Marked)) {
        return; // 既にマークされている
    }
    
    setFlag(Flag_Marked);
    
    // 参照しているオブジェクトをマーク
    if (getTag() == TAG_OBJECT || getTag() == TAG_STRING || getTag() == TAG_SYMBOL) {
        void* payload = getPayload();
        if (payload) {
            // オブジェクトのマークメソッドを呼び出す
            // ここでは、単純化のためにコメントアウト
            // Object* obj = static_cast<Object*>(payload);
            // obj->mark();
        }
    }
}

void Value::protect() {
    setFlag(Flag_Protected);
}

void Value::unprotect() {
    clearFlag(Flag_Protected);
}

Context* Value::getContext() const {
    return m_context;
}

void Value::setTag(uint64_t tag, void* payload) {
    // NaNボクシングを使用してタグとペイロードを格納
    m_value.m_bits = TAG_NAN | tag;
    
    if (payload) {
        m_value.m_bits = (m_value.m_bits & TAG_MASK) | (reinterpret_cast<uint64_t>(payload) & TAG_PAYLOAD);
    }
}

uint64_t Value::getTag() const {
    // NaNボクシングからタグを抽出
    return m_value.m_bits & TAG_MASK;
}

void* Value::getPayload() const {
    // NaNボクシングからペイロードを抽出
    return reinterpret_cast<void*>(m_value.m_bits & TAG_PAYLOAD);
}

void Value::setNumber(double num) {
    m_value.m_number = num;
}

double Value::getNumber() const {
    return m_value.m_number;
}

bool Value::hasFlag(ValueFlags flag) const {
    return (m_flags & flag) != 0;
}

void Value::setFlag(ValueFlags flag) {
    m_flags |= flag;
}

void Value::clearFlag(ValueFlags flag) {
    m_flags &= ~flag;
}

} // namespace core
} // namespace aerojs 