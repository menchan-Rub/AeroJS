/**
 * @file reflect.cpp
 * @brief JavaScript の Reflect オブジェクトの実装
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#include "reflect.h"

#include "core/runtime/execution_context.h"
#include "core/runtime/property_descriptor.h"
#include "core/runtime/value.h"

namespace aero {

Value reflectApply(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 3) {
    return ctx->throwTypeError("Reflect.apply requires at least 3 arguments");
  }

  Value target = args[0];
  Value thisArg = args[1];
  Value argumentsList = args[2];

  // targetが関数でない場合はエラー
  if (!target.isFunction()) {
    return ctx->throwTypeError("Reflect.apply: target is not callable");
  }

  // argumentsListが配列でない場合はエラー
  if (!argumentsList.isObject() || !argumentsList.asObject()->isArrayLike()) {
    return ctx->throwTypeError("Reflect.apply: argumentsList is not an array-like object");
  }

  // argumentsListから引数を展開
  std::vector<Value> functionArgs;
  auto* argsObj = argumentsList.asObject();
  uint32_t length = argsObj->length();

  for (uint32_t i = 0; i < length; ++i) {
    functionArgs.push_back(argsObj->get(std::to_string(i)));
  }

  // 関数を呼び出す
  return target.asObject()->call(ctx, thisArg, functionArgs);
}

Value reflectConstruct(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 2) {
    return ctx->throwTypeError("Reflect.construct requires at least 2 arguments");
  }

  Value target = args[0];
  Value argumentsList = args[1];
  Value newTarget = args.size() > 2 ? args[2] : target;

  // targetがコンストラクタでない場合はエラー
  if (!target.isObject() || !target.asObject()->isConstructor()) {
    return ctx->throwTypeError("Reflect.construct: target is not a constructor");
  }

  // newTargetがコンストラクタでない場合はエラー
  if (!newTarget.isObject() || !newTarget.asObject()->isConstructor()) {
    return ctx->throwTypeError("Reflect.construct: newTarget is not a constructor");
  }

  // argumentsListが配列でない場合はエラー
  if (!argumentsList.isObject() || !argumentsList.asObject()->isArrayLike()) {
    return ctx->throwTypeError("Reflect.construct: argumentsList is not an array-like object");
  }

  // argumentsListから引数を展開
  std::vector<Value> constructorArgs;
  auto* argsObj = argumentsList.asObject();
  uint32_t length = argsObj->length();

  for (uint32_t i = 0; i < length; ++i) {
    constructorArgs.push_back(argsObj->get(std::to_string(i)));
  }

  // コンストラクタを呼び出す
  return target.asObject()->construct(ctx, constructorArgs, newTarget.asObject());
}

Value reflectDefineProperty(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 3) {
    return ctx->throwTypeError("Reflect.defineProperty requires at least 3 arguments");
  }

  Value target = args[0];
  Value propertyKey = args[1];
  Value attributes = args[2];

  // targetがオブジェクトでない場合はエラー
  if (!target.isObject()) {
    return ctx->throwTypeError("Reflect.defineProperty: target is not an object");
  }

  // プロパティキーを文字列または記号に変換
  auto key = propertyKey.toPropertyKey();

  // 属性オブジェクトからプロパティディスクリプタを作成
  PropertyDescriptor descriptor = PropertyDescriptor::fromObject(ctx, attributes.asObject());

  // プロパティを定義
  bool success = target.asObject()->defineOwnProperty(key, descriptor);

  return Value(success);
}

Value reflectDeleteProperty(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 2) {
    return ctx->throwTypeError("Reflect.deleteProperty requires at least 2 arguments");
  }

  Value target = args[0];
  Value propertyKey = args[1];

  // targetがオブジェクトでない場合はエラー
  if (!target.isObject()) {
    return ctx->throwTypeError("Reflect.deleteProperty: target is not an object");
  }

  // プロパティキーを文字列または記号に変換
  auto key = propertyKey.toPropertyKey();

  // プロパティを削除
  bool success = target.asObject()->deleteProperty(key);

  return Value(success);
}

Value reflectGet(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 2) {
    return ctx->throwTypeError("Reflect.get requires at least 2 arguments");
  }

  Value target = args[0];
  Value propertyKey = args[1];
  Value receiver = args.size() > 2 ? args[2] : target;

  // targetがオブジェクトでない場合はエラー
  if (!target.isObject()) {
    return ctx->throwTypeError("Reflect.get: target is not an object");
  }

  // プロパティキーを文字列または記号に変換
  auto key = propertyKey.toPropertyKey();

  // プロパティを取得
  return target.asObject()->get(key, receiver);
}

Value reflectGetOwnPropertyDescriptor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 2) {
    return ctx->throwTypeError("Reflect.getOwnPropertyDescriptor requires at least 2 arguments");
  }

  Value target = args[0];
  Value propertyKey = args[1];

  // targetがオブジェクトでない場合はエラー
  if (!target.isObject()) {
    return ctx->throwTypeError("Reflect.getOwnPropertyDescriptor: target is not an object");
  }

  // プロパティキーを文字列または記号に変換
  auto key = propertyKey.toPropertyKey();

  // プロパティディスクリプタを取得
  auto descriptor = target.asObject()->getOwnProperty(key);

  // ディスクリプタが存在しない場合はundefinedを返す
  if (!descriptor.hasValue()) {
    return Value::undefined();
  }

  // ディスクリプタオブジェクトを作成して返す
  return Value(descriptor.toObject(ctx));
}

Value reflectGetPrototypeOf(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.empty()) {
    return ctx->throwTypeError("Reflect.getPrototypeOf requires at least 1 argument");
  }

  Value target = args[0];

  // targetがオブジェクトでない場合はエラー
  if (!target.isObject()) {
    return ctx->throwTypeError("Reflect.getPrototypeOf: target is not an object");
  }

  // プロトタイプを取得
  Object* prototype = target.asObject()->getPrototype();

  // プロトタイプがnullの場合はnullを返す
  if (!prototype) {
    return Value::null();
  }

  return Value(prototype);
}

Value reflectHas(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 2) {
    return ctx->throwTypeError("Reflect.has requires at least 2 arguments");
  }

  Value target = args[0];
  Value propertyKey = args[1];

  // targetがオブジェクトでない場合はエラー
  if (!target.isObject()) {
    return ctx->throwTypeError("Reflect.has: target is not an object");
  }

  // プロパティキーを文字列または記号に変換
  auto key = propertyKey.toPropertyKey();

  // プロパティの存在を確認
  bool exists = target.asObject()->has(key);

  return Value(exists);
}

Value reflectIsExtensible(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.empty()) {
    return ctx->throwTypeError("Reflect.isExtensible requires at least 1 argument");
  }

  Value target = args[0];

  // targetがオブジェクトでない場合はエラー
  if (!target.isObject()) {
    return ctx->throwTypeError("Reflect.isExtensible: target is not an object");
  }

  // 拡張可能かどうかを確認
  bool isExtensible = target.asObject()->isExtensible();

  return Value(isExtensible);
}

Value reflectOwnKeys(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.empty()) {
    return ctx->throwTypeError("Reflect.ownKeys requires at least 1 argument");
  }

  Value target = args[0];

  // targetがオブジェクトでない場合はエラー
  if (!target.isObject()) {
    return ctx->throwTypeError("Reflect.ownKeys: target is not an object");
  }

  // 自身のプロパティキーを取得
  auto keys = target.asObject()->ownPropertyKeys();

  // キーの配列を作成
  auto* result = ctx->createArray();
  for (size_t i = 0; i < keys.size(); ++i) {
    result->defineOwnProperty(std::to_string(i), PropertyDescriptor(keys[i], true, true, true));
  }

  return Value(result);
}

Value reflectPreventExtensions(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.empty()) {
    return ctx->throwTypeError("Reflect.preventExtensions requires at least 1 argument");
  }

  Value target = args[0];

  // targetがオブジェクトでない場合はエラー
  if (!target.isObject()) {
    return ctx->throwTypeError("Reflect.preventExtensions: target is not an object");
  }

  // 拡張を禁止
  bool success = target.asObject()->preventExtensions();

  return Value(success);
}

Value reflectSet(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 3) {
    return ctx->throwTypeError("Reflect.set requires at least 3 arguments");
  }

  Value target = args[0];
  Value propertyKey = args[1];
  Value value = args[2];
  Value receiver = args.size() > 3 ? args[3] : target;

  // targetがオブジェクトでない場合はエラー
  if (!target.isObject()) {
    return ctx->throwTypeError("Reflect.set: target is not an object");
  }

  // プロパティキーを文字列または記号に変換
  auto key = propertyKey.toPropertyKey();

  // プロパティを設定
  bool success = target.asObject()->set(key, value, receiver);

  return Value(success);
}

Value reflectSetPrototypeOf(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 引数の検証
  if (args.size() < 2) {
    return ctx->throwTypeError("Reflect.setPrototypeOf requires at least 2 arguments");
  }

  Value target = args[0];
  Value prototype = args[1];

  // targetがオブジェクトでない場合はエラー
  if (!target.isObject()) {
    return ctx->throwTypeError("Reflect.setPrototypeOf: target is not an object");
  }

  // prototypeがオブジェクトまたはnullでない場合はエラー
  if (!prototype.isObject() && !prototype.isNull()) {
    return ctx->throwTypeError("Reflect.setPrototypeOf: prototype must be an object or null");
  }

  // プロトタイプを設定
  bool success = target.asObject()->setPrototype(prototype.isNull() ? nullptr : prototype.asObject());

  return Value(success);
}

void registerReflectObject(ExecutionContext* ctx, Object* global) {
  // Reflectオブジェクトを作成
  auto* reflectObj = ctx->createObject();

  // Reflectのメソッドを定義
  reflectObj->defineProperty("apply",
                             PropertyDescriptor(Value(ctx->createFunction(reflectApply, "apply", 3)),
                                                true, false, true));

  reflectObj->defineProperty("construct",
                             PropertyDescriptor(Value(ctx->createFunction(reflectConstruct, "construct", 2)),
                                                true, false, true));

  reflectObj->defineProperty("defineProperty",
                             PropertyDescriptor(Value(ctx->createFunction(reflectDefineProperty, "defineProperty", 3)),
                                                true, false, true));

  reflectObj->defineProperty("deleteProperty",
                             PropertyDescriptor(Value(ctx->createFunction(reflectDeleteProperty, "deleteProperty", 2)),
                                                true, false, true));

  reflectObj->defineProperty("get",
                             PropertyDescriptor(Value(ctx->createFunction(reflectGet, "get", 2)),
                                                true, false, true));

  reflectObj->defineProperty("getOwnPropertyDescriptor",
                             PropertyDescriptor(Value(ctx->createFunction(reflectGetOwnPropertyDescriptor, "getOwnPropertyDescriptor", 2)),
                                                true, false, true));

  reflectObj->defineProperty("getPrototypeOf",
                             PropertyDescriptor(Value(ctx->createFunction(reflectGetPrototypeOf, "getPrototypeOf", 1)),
                                                true, false, true));

  reflectObj->defineProperty("has",
                             PropertyDescriptor(Value(ctx->createFunction(reflectHas, "has", 2)),
                                                true, false, true));

  reflectObj->defineProperty("isExtensible",
                             PropertyDescriptor(Value(ctx->createFunction(reflectIsExtensible, "isExtensible", 1)),
                                                true, false, true));

  reflectObj->defineProperty("ownKeys",
                             PropertyDescriptor(Value(ctx->createFunction(reflectOwnKeys, "ownKeys", 1)),
                                                true, false, true));

  reflectObj->defineProperty("preventExtensions",
                             PropertyDescriptor(Value(ctx->createFunction(reflectPreventExtensions, "preventExtensions", 1)),
                                                true, false, true));

  reflectObj->defineProperty("set",
                             PropertyDescriptor(Value(ctx->createFunction(reflectSet, "set", 3)),
                                                true, false, true));

  reflectObj->defineProperty("setPrototypeOf",
                             PropertyDescriptor(Value(ctx->createFunction(reflectSetPrototypeOf, "setPrototypeOf", 2)),
                                                true, false, true));

  // Reflectオブジェクトを不変に設定
  reflectObj->preventExtensions();

  // グローバルオブジェクトに登録
  global->defineProperty("Reflect", PropertyDescriptor(Value(reflectObj), true, false, true));

  // コンテキストに保存
  ctx->setReflectObject(reflectObj);
}

}  // namespace aero