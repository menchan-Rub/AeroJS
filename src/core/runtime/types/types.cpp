/**
 * @file types.cpp
 * @brief JavaScriptの型システムの実装
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#include "types.h"

#include <cmath>
#include <limits>

#include "../error/error.h"
#include "../iteration/iteration.h"
#include "../symbols/symbols.h"

namespace aero {

//-----------------------------------------------------------------------------
// TypeChecking クラスの実装
//-----------------------------------------------------------------------------

bool TypeChecking::isUndefined(ExecutionContext* ctx, const Value& value) {
  return value.isUndefined();
}

bool TypeChecking::isNull(ExecutionContext* ctx, const Value& value) {
  return value.isNull();
}

bool TypeChecking::isNullOrUndefined(ExecutionContext* ctx, const Value& value) {
  return value.isNull() || value.isUndefined();
}

bool TypeChecking::isBoolean(ExecutionContext* ctx, const Value& value) {
  return value.isBoolean();
}

bool TypeChecking::isNumber(ExecutionContext* ctx, const Value& value) {
  return value.isNumber();
}

bool TypeChecking::isInteger(ExecutionContext* ctx, const Value& value) {
  if (!value.isNumber()) {
    return false;
  }

  double num = value.asNumber();

  // NaNやInfinityはfalse
  if (std::isnan(num) || std::isinf(num)) {
    return false;
  }

  // 整数かどうか確認
  return std::floor(num) == num;
}

bool TypeChecking::isString(ExecutionContext* ctx, const Value& value) {
  return value.isString();
}

bool TypeChecking::isSymbol(ExecutionContext* ctx, const Value& value) {
  return value.isSymbol();
}

bool TypeChecking::isBigInt(ExecutionContext* ctx, const Value& value) {
  return value.isBigInt();
}

bool TypeChecking::isObject(ExecutionContext* ctx, const Value& value) {
  return value.isObject();
}

bool TypeChecking::isFunction(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isCallable();
}

bool TypeChecking::isArray(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  // Array.isArray()と同等の動作
  return value.asObject()->isArray();
}

bool TypeChecking::isDate(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isDate();
}

bool TypeChecking::isRegExp(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isRegExp(ctx);
}

bool TypeChecking::isError(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isError();
}

bool TypeChecking::isMap(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isMap();
}

bool TypeChecking::isSet(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isSet();
}

bool TypeChecking::isWeakMap(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isWeakMap();
}

bool TypeChecking::isWeakSet(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isWeakSet();
}

bool TypeChecking::isArrayBuffer(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isArrayBuffer();
}

bool TypeChecking::isSharedArrayBuffer(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isSharedArrayBuffer();
}

bool TypeChecking::isDataView(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isDataView();
}

bool TypeChecking::isTypedArray(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isTypedArray();
}

bool TypeChecking::isPromise(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isPromise(ctx);
}

bool TypeChecking::isProxy(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isProxy();
}

bool TypeChecking::isConstructor(ExecutionContext* ctx, const Value& value) {
  if (!value.isObject()) {
    return false;
  }

  return value.asObject()->isConstructor();
}

bool TypeChecking::isPrimitive(ExecutionContext* ctx, const Value& value) {
  // オブジェクト以外の全ての値は原始型
  return !value.isObject();
}

bool TypeChecking::isIterable(ExecutionContext* ctx, const Value& value) {
  // Symbol.iteratorメソッドを持つオブジェクトがイテレーブル
  return Iterable::isIterable(ctx, value);
}

//-----------------------------------------------------------------------------
// TypeConversion クラスの実装
//-----------------------------------------------------------------------------

bool TypeConversion::toBoolean(ExecutionContext* ctx, const Value& value) {
  // ECMAScript仕様に従って変換
  switch (value.getType()) {
    case ValueType::Undefined:
    case ValueType::Null:
      return false;
    case ValueType::Boolean:
      return value.asBoolean();
    case ValueType::Number: {
      double num = value.asNumber();
      return num != 0 && !std::isnan(num);
    }
    case ValueType::String:
      return !value.asString().empty();
    case ValueType::Symbol:
    case ValueType::BigInt:
    case ValueType::Object:
      return true;
    default:
      return false;
  }
}

double TypeConversion::toNumber(ExecutionContext* ctx, const Value& value) {
  // ECMAScript仕様に従って変換
  switch (value.getType()) {
    case ValueType::Undefined:
      return std::numeric_limits<double>::quiet_NaN();
    case ValueType::Null:
      return 0.0;
    case ValueType::Boolean:
      return value.asBoolean() ? 1.0 : 0.0;
    case ValueType::Number:
      return value.asNumber();
    case ValueType::String: {
      // 文字列を数値に変換（実際の実装ではもっと複雑）
      try {
        return std::stod(value.asString());
      } catch (...) {
        return std::numeric_limits<double>::quiet_NaN();
      }
    }
    case ValueType::Symbol:
      // Symbolは数値に変換できないのでNaN
      return std::numeric_limits<double>::quiet_NaN();
    case ValueType::BigInt: {
      // BigIntは数値に変換可能だが精度が失われる可能性がある
      try {
        return static_cast<double>(value.asBigInt()->toInt64());
      } catch (...) {
        return std::numeric_limits<double>::quiet_NaN();
      }
    }
    case ValueType::Object: {
      // オブジェクトは原始値に変換してから処理
      Value primValue = toPrimitive(ctx, value, "number");
      return toNumber(ctx, primValue);
    }
    default:
      return 0.0;
  }
}

int64_t TypeConversion::toInteger(ExecutionContext* ctx, const Value& value) {
  // まず数値に変換
  double num = toNumber(ctx, value);

  // NaNの場合は0
  if (std::isnan(num)) {
    return 0;
  }

  // ±0の場合は0
  if (num == 0) {
    return 0;
  }

  // 無限大の場合はそのまま
  if (std::isinf(num)) {
    return num > 0 ? std::numeric_limits<int64_t>::max() : std::numeric_limits<int64_t>::min();
  }

  // 整数部分を抽出
  return static_cast<int64_t>(std::trunc(num));
}

int32_t TypeConversion::toInt32(ExecutionContext* ctx, const Value& value) {
  // まず数値に変換
  double num = toNumber(ctx, value);

  // NaN、±0、±Infinityの場合は0
  if (std::isnan(num) || num == 0 || std::isinf(num)) {
    return 0;
  }

  // モジュロ2^32を計算
  double modulo = std::fmod(std::trunc(num), 4294967296.0);

  // 負数の場合は2^32を加算
  if (modulo < 0) {
    modulo += 4294967296.0;
  }

  // 符号付き32ビット整数に変換
  if (modulo >= 2147483648.0) {
    return static_cast<int32_t>(modulo - 4294967296.0);
  } else {
    return static_cast<int32_t>(modulo);
  }
}

uint32_t TypeConversion::toUint32(ExecutionContext* ctx, const Value& value) {
  // まず数値に変換
  double num = toNumber(ctx, value);

  // NaN、±0、±Infinityの場合は0
  if (std::isnan(num) || num == 0 || std::isinf(num)) {
    return 0;
  }

  // モジュロ2^32を計算
  double modulo = std::fmod(std::trunc(num), 4294967296.0);

  // 負数の場合は2^32を加算
  if (modulo < 0) {
    modulo += 4294967296.0;
  }

  return static_cast<uint32_t>(modulo);
}

std::string TypeConversion::toString(ExecutionContext* ctx, const Value& value) {
  // ECMAScript仕様に従って変換
  switch (value.getType()) {
    case ValueType::Undefined:
      return "undefined";
    case ValueType::Null:
      return "null";
    case ValueType::Boolean:
      return value.asBoolean() ? "true" : "false";
    case ValueType::Number: {
      double num = value.asNumber();

      // 特殊な処理
      if (std::isnan(num)) {
        return "NaN";
      }
      if (num == 0) {
        return "0";
      }
      if (std::isinf(num)) {
        return num > 0 ? "Infinity" : "-Infinity";
      }

      // 数値を文字列に変換
      return std::to_string(num);
    }
    case ValueType::String:
      return value.asString();
    case ValueType::Symbol:
      // シンボルは文字列に直接変換できないが、デバッグ用にtoString()を呼び出す
      ctx->throwError(Error::createTypeError(ctx, "Cannot convert a Symbol to a string"));
      return "";
    case ValueType::BigInt:
      // BigIntは数値 + nの形式
      return value.asBigInt()->toString() + "n";
    case ValueType::Object: {
      // オブジェクトは原始値に変換してから処理
      Value primValue = toPrimitive(ctx, value, "string");
      return toString(ctx, primValue);
    }
    default:
      return "";
  }
}

Object* TypeConversion::toObject(ExecutionContext* ctx, const Value& value) {
  // ECMAScript仕様に従って変換
  switch (value.getType()) {
    case ValueType::Undefined:
    case ValueType::Null:
      ctx->throwError(Error::createTypeError(ctx, "Cannot convert undefined or null to object"));
      return nullptr;
    case ValueType::Boolean:
      // ブール値をBooleanオブジェクトにラップ
      return ctx->createBooleanObject(value.asBoolean());
    case ValueType::Number:
      // 数値をNumberオブジェクトにラップ
      return ctx->createNumberObject(value.asNumber());
    case ValueType::String:
      // 文字列をStringオブジェクトにラップ
      return ctx->createStringObject(value.asString());
    case ValueType::Symbol:
      // シンボルをSymbolオブジェクトにラップ
      return ctx->createSymbolObject(value.asSymbol());
    case ValueType::BigInt:
      // BigIntをBigIntオブジェクトにラップ
      return ctx->createBigIntObject(value.asBigInt());
    case ValueType::Object:
      // すでにオブジェクトなのでそのまま返す
      return value.asObject();
    default:
      ctx->throwError(Error::createTypeError(ctx, "Cannot convert to object"));
      return nullptr;
  }
}

Value TypeConversion::toPrimitive(ExecutionContext* ctx, const Value& value, const std::string& preferredType) {
  // 既に原始型ならそのまま返す
  if (!value.isObject()) {
    return value;
  }

  Object* obj = value.asObject();

  // Symbol.toPrimitiveメソッドを確認
  Value toPrimitiveFn = obj->get(ctx, Symbol::toPrimitive());
  if (toPrimitiveFn.isCallable()) {
    // ヒントを文字列として渡す
    std::vector<Value> args;
    args.push_back(Value::createString(preferredType));
    Value result = toPrimitiveFn.asFunction()->call(ctx, value, args);

    // 結果がオブジェクトでなければ返す
    if (!result.isObject()) {
      return result;
    }

    // オブジェクトが返された場合はTypeError
    ctx->throwError(Error::createTypeError(ctx, "Cannot convert object to primitive value"));
    return Value::createUndefined();
  }

  // デフォルトの変換アルゴリズム
  std::vector<std::string> methods;
  if (preferredType == "string") {
    methods = {"toString", "valueOf"};
  } else {
    methods = {"valueOf", "toString"};
  }

  for (const auto& method : methods) {
    Value fn = obj->get(ctx, method);
    if (fn.isCallable()) {
      std::vector<Value> args;
      Value result = fn.asFunction()->call(ctx, value, args);

      // 結果がオブジェクトでなければ返す
      if (!result.isObject()) {
        return result;
      }
    }
  }

  // 変換できなかった場合はTypeError
  ctx->throwError(Error::createTypeError(ctx, "Cannot convert object to primitive value"));
  return Value::createUndefined();
}

Value TypeConversion::toPropertyKey(ExecutionContext* ctx, const Value& value) {
  // まず原始値に変換
  Value key = toPrimitive(ctx, value, "string");

  // シンボルかどうかを確認
  if (key.isSymbol()) {
    return key;
  }

  // 文字列に変換
  return Value::createString(toString(ctx, key));
}

uint32_t TypeConversion::toLength(ExecutionContext* ctx, const Value& value) {
  // 整数に変換
  int64_t len = toInteger(ctx, value);

  // 負数や大きすぎる値を調整
  if (len <= 0) {
    return 0;
  }

  // 2^53-1を超える場合は上限を適用
  const int64_t MAX_SAFE_INTEGER = 9007199254740991;  // 2^53-1
  if (len > MAX_SAFE_INTEGER) {
    return MAX_SAFE_INTEGER;
  }

  return static_cast<uint32_t>(len);
}

uint32_t TypeConversion::toIndex(ExecutionContext* ctx, const Value& value) {
  // undefinedの場合は0を返す
  if (value.isUndefined()) {
    return 0;
  }

  // 整数に変換
  int64_t index = toInteger(ctx, value);

  // 負数や大きすぎる値をチェック
  if (index < 0 || index > std::numeric_limits<uint32_t>::max()) {
    ctx->throwError(Error::createRangeError(ctx, "Invalid array index"));
    return 0;
  }

  return static_cast<uint32_t>(index);
}

//-----------------------------------------------------------------------------
// TypeComparison クラスの実装
//-----------------------------------------------------------------------------

bool TypeComparison::strictEquals(ExecutionContext* ctx, const Value& x, const Value& y) {
  // 型が異なる場合は等しくない
  if (x.getType() != y.getType()) {
    return false;
  }

  // 型ごとの比較
  switch (x.getType()) {
    case ValueType::Undefined:
    case ValueType::Null:
      return true;
    case ValueType::Boolean:
      return x.asBoolean() == y.asBoolean();
    case ValueType::Number: {
      double numX = x.asNumber();
      double numY = y.asNumber();

      // NaNは自身と等しくない
      if (std::isnan(numX) || std::isnan(numY)) {
        return false;
      }

      // 0と-0は等しい
      if (numX == 0 && numY == 0) {
        return true;
      }

      return numX == numY;
    }
    case ValueType::String:
      return x.asString() == y.asString();
    case ValueType::Symbol:
      return x.asSymbol()->equals(y.asSymbol());
    case ValueType::BigInt:
      return x.asBigInt()->equals(y.asBigInt());
    case ValueType::Object:
      // オブジェクトは参照比較
      return x.asObject() == y.asObject();
    default:
      return false;
  }
}

bool TypeComparison::sameValue(ExecutionContext* ctx, const Value& x, const Value& y) {
  // 型が異なる場合は等しくない
  if (x.getType() != y.getType()) {
    return false;
  }

  // 型ごとの比較
  switch (x.getType()) {
    case ValueType::Undefined:
    case ValueType::Null:
      return true;
    case ValueType::Boolean:
      return x.asBoolean() == y.asBoolean();
    case ValueType::Number: {
      double numX = x.asNumber();
      double numY = y.asNumber();

      // NaNは自身と等しい（strictEqualsと違い）
      if (std::isnan(numX) && std::isnan(numY)) {
        return true;
      }

      // 0と-0は区別する（strictEqualsと違い）
      if (numX == 0 && numY == 0) {
        return std::signbit(numX) == std::signbit(numY);
      }

      return numX == numY;
    }
    case ValueType::String:
      return x.asString() == y.asString();
    case ValueType::Symbol:
      return x.asSymbol()->equals(y.asSymbol());
    case ValueType::BigInt:
      return x.asBigInt()->equals(y.asBigInt());
    case ValueType::Object:
      // オブジェクトは参照比較
      return x.asObject() == y.asObject();
    default:
      return false;
  }
}

bool TypeComparison::sameValueZero(ExecutionContext* ctx, const Value& x, const Value& y) {
  // 型が異なる場合は等しくない
  if (x.getType() != y.getType()) {
    return false;
  }

  // 型ごとの比較
  switch (x.getType()) {
    case ValueType::Undefined:
    case ValueType::Null:
      return true;
    case ValueType::Boolean:
      return x.asBoolean() == y.asBoolean();
    case ValueType::Number: {
      double numX = x.asNumber();
      double numY = y.asNumber();

      // NaNは自身と等しい（strictEqualsと違い）
      if (std::isnan(numX) && std::isnan(numY)) {
        return true;
      }

      // 0と-0は等しい（sameValueと違い）
      if (numX == 0 && numY == 0) {
        return true;
      }

      return numX == numY;
    }
    case ValueType::String:
      return x.asString() == y.asString();
    case ValueType::Symbol:
      return x.asSymbol()->equals(y.asSymbol());
    case ValueType::BigInt:
      return x.asBigInt()->equals(y.asBigInt());
    case ValueType::Object:
      // オブジェクトは参照比較
      return x.asObject() == y.asObject();
    default:
      return false;
  }
}

//-----------------------------------------------------------------------------
// 初期化関数
//-----------------------------------------------------------------------------

void initializeTypeSystem(ExecutionContext* ctx, Object* globalObj) {
  // 必要なら型システムの初期化処理を実装
}

}  // namespace aero