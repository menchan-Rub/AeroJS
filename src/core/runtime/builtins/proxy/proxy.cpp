/**
 * @file proxy.cpp
 * @brief Proxy実装（ES6+）
 * @version 1.0.0
 * @license MIT
 */

#include "proxy.h"
#include "../../globals/global_object.h"
#include <stdexcept>

namespace aerojs {
namespace core {
namespace runtime {
namespace builtins {

// PropertyKeyFromStringの実装
PropertyKey PropertyKey::fromString(const std::string& key) {
  PropertyKey result;
  result.type = PropertyKeyType::String;
  result.stringKey = key;
  return result;
}

// PropertyKeyFromSymbolの実装
PropertyKey PropertyKey::fromSymbol(uint32_t id) {
  PropertyKey result;
  result.type = PropertyKeyType::Symbol;
  result.symbolId = id;
  return result;
}

// PropertyKey等価演算子
bool PropertyKey::operator==(const PropertyKey& other) const {
  if (type != other.type) {
    return false;
  }
  
  if (type == PropertyKeyType::String) {
    return stringKey == other.stringKey;
  } else {
    return symbolId == other.symbolId;
  }
}

// PropertyKey文字列化
std::string PropertyKey::toString() const {
  if (type == PropertyKeyType::String) {
    return stringKey;
  } else {
    return "Symbol(" + std::to_string(symbolId) + ")";
  }
}

// デフォルトProxyHandlerの作成
ProxyHandler ProxyHandler::createDefault(execution::ExecutionContext* context) {
  ProxyHandler handler;
  // すべてのトラップにundefinedを設定
  handler.getPrototypeOf = Value::createUndefined();
  handler.setPrototypeOf = Value::createUndefined();
  handler.isExtensible = Value::createUndefined();
  handler.preventExtensions = Value::createUndefined();
  handler.getOwnPropertyDescriptor = Value::createUndefined();
  handler.defineProperty = Value::createUndefined();
  handler.has = Value::createUndefined();
  handler.get = Value::createUndefined();
  handler.set = Value::createUndefined();
  handler.deleteProperty = Value::createUndefined();
  handler.ownKeys = Value::createUndefined();
  handler.apply = Value::createUndefined();
  handler.construct = Value::createUndefined();
  
  return handler;
}

// ProxyObjectコンストラクタ
ProxyObject::ProxyObject(const Value& target, const Value& handler, execution::ExecutionContext* context)
  : target(target),
    handler(handler),
    context(context),
    revoked(false) {
  
  // ハンドラからトラップを抽出
  if (handler.isObject()) {
    // getPrototypeOf
    traps.getPrototypeOf = handler.getProperty(context, "getPrototypeOf");
    
    // setPrototypeOf
    traps.setPrototypeOf = handler.getProperty(context, "setPrototypeOf");
    
    // isExtensible
    traps.isExtensible = handler.getProperty(context, "isExtensible");
    
    // preventExtensions
    traps.preventExtensions = handler.getProperty(context, "preventExtensions");
    
    // getOwnPropertyDescriptor
    traps.getOwnPropertyDescriptor = handler.getProperty(context, "getOwnPropertyDescriptor");
    
    // defineProperty
    traps.defineProperty = handler.getProperty(context, "defineProperty");
    
    // has
    traps.has = handler.getProperty(context, "has");
    
    // get
    traps.get = handler.getProperty(context, "get");
    
    // set
    traps.set = handler.getProperty(context, "set");
    
    // deleteProperty
    traps.deleteProperty = handler.getProperty(context, "deleteProperty");
    
    // ownKeys
    traps.ownKeys = handler.getProperty(context, "ownKeys");
    
    // apply
    traps.apply = handler.getProperty(context, "apply");
    
    // construct
    traps.construct = handler.getProperty(context, "construct");
  } else {
    // 空のハンドラを作成
    traps = ProxyHandler::createDefault(context);
  }
}

// ProxyObjectデストラクタ
ProxyObject::~ProxyObject() {
  // 特にすることはない
}

// [[GetPrototypeOf]]
Value ProxyObject::getPrototypeOf() {
  ensureNotRevoked();
  
  if (traps.getPrototypeOf.isFunction()) {
    Value trapResult = callTrap(traps.getPrototypeOf, target);
    
    // 結果の検証
    if (!trapResult.isObject() && !trapResult.isNull()) {
      // TypeError: getPrototypeOf trap returned neither object nor null
      return Value::createUndefined(); // エラーの代わり
    }
    
    // 不変性チェック: targetがnon-extensibleなら、trapResultはtarget.[[GetPrototypeOf]]()と一致する必要がある
    if (!target.isExtensible(context)) {
      Value targetProto = target.getPrototype(context);
      if (!trapResult.equals(targetProto)) {
        // TypeError: getPrototypeOf trap violated invariant
        return Value::createUndefined(); // エラーの代わり
      }
    }
    
    return trapResult;
  }
  
  // デフォルト: ターゲットのプロトタイプを返す
  return target.getPrototype(context);
}

// [[SetPrototypeOf]]
bool ProxyObject::setPrototypeOf(const Value& prototype) {
  ensureNotRevoked();
  
  if (traps.setPrototypeOf.isFunction()) {
    Value trapResult = callTrap(traps.setPrototypeOf, target, prototype);
    
    // 結果をブール値に変換
    bool booleanTrapResult = trapResult.toBoolean();
    
    // falseを返した場合は早期リターン
    if (!booleanTrapResult) {
      return false;
    }
    
    // 不変性チェック: targetがnon-extensibleなら、prototypeはtarget.[[GetPrototypeOf]]()と一致する必要がある
    if (!target.isExtensible(context)) {
      Value targetProto = target.getPrototype(context);
      if (!prototype.equals(targetProto)) {
        // TypeError: setPrototypeOf trap violated invariant
        return false;
      }
    }
    
    return true;
  }
  
  // デフォルト: ターゲットのプロトタイプを設定
  return target.setPrototype(context, prototype);
}

// [[IsExtensible]]
bool ProxyObject::isExtensible() {
  ensureNotRevoked();
  
  if (traps.isExtensible.isFunction()) {
    Value trapResult = callTrap(traps.isExtensible, target);
    
    // 結果をブール値に変換
    bool booleanTrapResult = trapResult.toBoolean();
    
    // 不変性チェック: trapResultはtarget.[[IsExtensible]]()と一致する必要がある
    bool targetIsExtensible = target.isExtensible(context);
    if (booleanTrapResult != targetIsExtensible) {
      // TypeError: isExtensible trap violated invariant
      return false;
    }
    
    return booleanTrapResult;
  }
  
  // デフォルト: ターゲットの拡張可能性を返す
  return target.isExtensible(context);
}

// [[PreventExtensions]]
bool ProxyObject::preventExtensions() {
  ensureNotRevoked();
  
  if (traps.preventExtensions.isFunction()) {
    Value trapResult = callTrap(traps.preventExtensions, target);
    
    // 結果をブール値に変換
    bool booleanTrapResult = trapResult.toBoolean();
    
    // 不変性チェック: trueを返した場合、targetは非拡張にされている必要がある
    if (booleanTrapResult && target.isExtensible(context)) {
      // TypeError: preventExtensions trap violated invariant
      return false;
    }
    
    return booleanTrapResult;
  }
  
  // デフォルト: ターゲットの拡張を防止
  return target.preventExtensions(context);
}

// [[GetOwnProperty]]
Value ProxyObject::getOwnPropertyDescriptor(const PropertyKey& key) {
  ensureNotRevoked();
  
  if (traps.getOwnPropertyDescriptor.isFunction()) {
    // キーを文字列に変換
    Value keyValue;
    if (key.type == PropertyKeyType::String) {
      keyValue = Value::createString(context, key.stringKey);
    } else {
      keyValue = Value::createSymbol(context, key.symbolId);
    }
    
    Value trapResult = callTrap(traps.getOwnPropertyDescriptor, target, keyValue);
    
    // 結果の検証
    if (!trapResult.isObject() && !trapResult.isUndefined()) {
      // TypeError: getOwnPropertyDescriptor trap returned non-object and non-undefined
      return Value::createUndefined(); // エラーの代わり
    }
    
    // ターゲットのプロパティディスクリプタを取得
    Value targetDesc = target.getOwnPropertyDescriptor(context, keyValue);
    
    // 不変性チェック
    // 実際の実装では複雑な不変性チェックが必要
    
    return trapResult;
  }
  
  // デフォルト: ターゲットのプロパティディスクリプタを返す
  if (key.type == PropertyKeyType::String) {
    return target.getOwnPropertyDescriptor(context, Value::createString(context, key.stringKey));
  } else {
    return target.getOwnPropertyDescriptor(context, Value::createSymbol(context, key.symbolId));
  }
}

// [[DefineOwnProperty]]
bool ProxyObject::defineProperty(const PropertyKey& key, const Value& descriptor) {
  ensureNotRevoked();
  
  if (traps.defineProperty.isFunction()) {
    // キーを文字列に変換
    Value keyValue;
    if (key.type == PropertyKeyType::String) {
      keyValue = Value::createString(context, key.stringKey);
    } else {
      keyValue = Value::createSymbol(context, key.symbolId);
    }
    
    Value trapResult = callTrap(traps.defineProperty, target, keyValue, descriptor);
    
    // 結果をブール値に変換
    bool booleanTrapResult = trapResult.toBoolean();
    
    // falseを返した場合は早期リターン
    if (!booleanTrapResult) {
      return false;
    }
    
    // 不変性チェック
    // 実際の実装では複雑な不変性チェックが必要
    
    return true;
  }
  
  // デフォルト: ターゲットにプロパティを定義
  if (key.type == PropertyKeyType::String) {
    return target.defineProperty(context, Value::createString(context, key.stringKey), descriptor);
  } else {
    return target.defineProperty(context, Value::createSymbol(context, key.symbolId), descriptor);
  }
}

// [[HasProperty]]
bool ProxyObject::has(const PropertyKey& key) {
  ensureNotRevoked();
  
  if (traps.has.isFunction()) {
    // キーを文字列に変換
    Value keyValue;
    if (key.type == PropertyKeyType::String) {
      keyValue = Value::createString(context, key.stringKey);
    } else {
      keyValue = Value::createSymbol(context, key.symbolId);
    }
    
    Value trapResult = callTrap(traps.has, target, keyValue);
    
    // 結果をブール値に変換
    bool booleanTrapResult = trapResult.toBoolean();
    
    // 不変性チェック
    if (!booleanTrapResult) {
      // ターゲットのプロパティディスクリプタを取得
      Value targetDesc = target.getOwnPropertyDescriptor(context, keyValue);
      
      if (targetDesc.isObject()) {
        // プロパティが存在し、設定不可の場合は違反
        bool configurable = targetDesc.getProperty(context, "configurable").toBoolean();
        if (!configurable) {
          // TypeError: has trap violated invariant
          return true;
        }
        
        // 非拡張ターゲットの存在プロパティも違反
        if (!target.isExtensible(context)) {
          // TypeError: has trap violated invariant
          return true;
        }
      }
    }
    
    return booleanTrapResult;
  }
  
  // デフォルト: ターゲットのプロパティ存在チェック
  if (key.type == PropertyKeyType::String) {
    return target.hasProperty(context, Value::createString(context, key.stringKey));
  } else {
    return target.hasProperty(context, Value::createSymbol(context, key.symbolId));
  }
}

// [[Get]]
Value ProxyObject::get(const PropertyKey& key, const Value& receiver) {
  ensureNotRevoked();
  
  if (traps.get.isFunction()) {
    // キーを文字列に変換
    Value keyValue;
    if (key.type == PropertyKeyType::String) {
      keyValue = Value::createString(context, key.stringKey);
    } else {
      keyValue = Value::createSymbol(context, key.symbolId);
    }
    
    Value trapResult = callTrap(traps.get, target, keyValue, receiver);
    
    // 不変性チェック
    // ターゲットのプロパティディスクリプタを取得
    Value targetDesc = target.getOwnPropertyDescriptor(context, keyValue);
    
    if (targetDesc.isObject()) {
      // データプロパティのgettableチェック
      if (targetDesc.hasProperty(context, "value")) {
        bool writable = targetDesc.getProperty(context, "writable").toBoolean();
        bool configurable = targetDesc.getProperty(context, "configurable").toBoolean();
        
        if (!configurable && !writable && !trapResult.equals(targetDesc.getProperty(context, "value"))) {
          // TypeError: get trap violated invariant
          return Value::createUndefined(); // エラーの代わり
        }
      }
      
      // アクセサプロパティのgetterチェック
      if (targetDesc.hasProperty(context, "get")) {
        bool configurable = targetDesc.getProperty(context, "configurable").toBoolean();
        Value getter = targetDesc.getProperty(context, "get");
        
        if (!configurable && getter.isUndefined() && !trapResult.isUndefined()) {
          // TypeError: get trap violated invariant
          return Value::createUndefined(); // エラーの代わり
        }
      }
    }
    
    return trapResult;
  }
  
  // デフォルト: ターゲットからプロパティを取得
  if (key.type == PropertyKeyType::String) {
    return target.get(context, Value::createString(context, key.stringKey), receiver);
  } else {
    return target.get(context, Value::createSymbol(context, key.symbolId), receiver);
  }
}

// [[Set]]
bool ProxyObject::set(const PropertyKey& key, const Value& value, const Value& receiver) {
  ensureNotRevoked();
  
  if (traps.set.isFunction()) {
    // キーを文字列に変換
    Value keyValue;
    if (key.type == PropertyKeyType::String) {
      keyValue = Value::createString(context, key.stringKey);
    } else {
      keyValue = Value::createSymbol(context, key.symbolId);
    }
    
    Value trapResult = callTrap(traps.set, target, keyValue, value, receiver);
    
    // 結果をブール値に変換
    bool booleanTrapResult = trapResult.toBoolean();
    
    // falseを返した場合は早期リターン
    if (!booleanTrapResult) {
      return false;
    }
    
    // 不変性チェック
    // ターゲットのプロパティディスクリプタを取得
    Value targetDesc = target.getOwnPropertyDescriptor(context, keyValue);
    
    if (targetDesc.isObject()) {
      // データプロパティのwritableチェック
      if (targetDesc.hasProperty(context, "value")) {
        bool writable = targetDesc.getProperty(context, "writable").toBoolean();
        bool configurable = targetDesc.getProperty(context, "configurable").toBoolean();
        
        if (!configurable && !writable) {
          // TypeError: set trap violated invariant
          return false;
        }
      }
      
      // アクセサプロパティのsetterチェック
      if (targetDesc.hasProperty(context, "set")) {
        bool configurable = targetDesc.getProperty(context, "configurable").toBoolean();
        Value setter = targetDesc.getProperty(context, "set");
        
        if (!configurable && setter.isUndefined()) {
          // TypeError: set trap violated invariant
          return false;
        }
      }
    }
    
    return true;
  }
  
  // デフォルト: ターゲットにプロパティを設定
  if (key.type == PropertyKeyType::String) {
    return target.set(context, Value::createString(context, key.stringKey), value, receiver);
  } else {
    return target.set(context, Value::createSymbol(context, key.symbolId), value, receiver);
  }
}

// [[Delete]]
bool ProxyObject::deleteProperty(const PropertyKey& key) {
  ensureNotRevoked();
  
  if (traps.deleteProperty.isFunction()) {
    // キーを文字列に変換
    Value keyValue;
    if (key.type == PropertyKeyType::String) {
      keyValue = Value::createString(context, key.stringKey);
    } else {
      keyValue = Value::createSymbol(context, key.symbolId);
    }
    
    Value trapResult = callTrap(traps.deleteProperty, target, keyValue);
    
    // 結果をブール値に変換
    bool booleanTrapResult = trapResult.toBoolean();
    
    // falseを返した場合は早期リターン
    if (!booleanTrapResult) {
      return false;
    }
    
    // 不変性チェック
    // ターゲットのプロパティディスクリプタを取得
    Value targetDesc = target.getOwnPropertyDescriptor(context, keyValue);
    
    if (targetDesc.isObject()) {
      bool configurable = targetDesc.getProperty(context, "configurable").toBoolean();
      
      if (!configurable) {
        // TypeError: deleteProperty trap violated invariant
        return false;
      }
    }
    
    return true;
  }
  
  // デフォルト: ターゲットからプロパティを削除
  if (key.type == PropertyKeyType::String) {
    return target.deleteProperty(context, Value::createString(context, key.stringKey));
  } else {
    return target.deleteProperty(context, Value::createSymbol(context, key.symbolId));
  }
}

// [[OwnPropertyKeys]]
Value ProxyObject::ownKeys() {
  ensureNotRevoked();
  
  if (traps.ownKeys.isFunction()) {
    Value trapResult = callTrap(traps.ownKeys, target);
    
    // 結果の検証
    if (!trapResult.isObject()) {
      // TypeError: ownKeys trap returned non-object
      return Value::createArray(context); // エラーの代わり
    }
    
    // 配列に変換
    // 実際の実装では配列を適切に変換する必要がある
    
    // 不変性チェック
    // 実際の実装では複雑な不変性チェックが必要
    
    return trapResult;
  }
  
  // デフォルト: ターゲットのプロパティキーを返す
  return target.ownKeys(context);
}

// [[Call]]
Value ProxyObject::apply(const Value& thisArg, const std::vector<Value>& args) {
  ensureNotRevoked();
  
  if (traps.apply.isFunction()) {
    // 引数配列の作成
    Value argsArray = Value::createArray(context);
    for (size_t i = 0; i < args.size(); ++i) {
      argsArray.setProperty(context, std::to_string(i), args[i]);
    }
    
    return callTrap(traps.apply, target, thisArg, argsArray);
  }
  
  // デフォルト: ターゲットを関数として呼び出す
  return target.call(context, thisArg, args);
}

// [[Construct]]
Value ProxyObject::construct(const std::vector<Value>& args, const Value& newTarget) {
  ensureNotRevoked();
  
  if (traps.construct.isFunction()) {
    // 引数配列の作成
    Value argsArray = Value::createArray(context);
    for (size_t i = 0; i < args.size(); ++i) {
      argsArray.setProperty(context, std::to_string(i), args[i]);
    }
    
    Value constructResult = callTrap(traps.construct, target, argsArray, newTarget);
    
    // 結果の検証
    if (!constructResult.isObject()) {
      // TypeError: construct trap returned non-object
      return Value::createObject(context); // エラーの代わり
    }
    
    return constructResult;
  }
  
  // デフォルト: ターゲットをコンストラクタとして呼び出す
  return target.construct(context, args, newTarget);
}

// ユーティリティ: 無効化されているかチェック
bool ProxyObject::isRevoked() const {
  return revoked;
}

// ユーティリティ: 無効化する
void ProxyObject::revoke() {
  revoked = true;
}

// アクセサ: ターゲット取得
const Value& ProxyObject::getTarget() const {
  return target;
}

// アクセサ: ハンドラ取得
const Value& ProxyObject::getHandler() const {
  return handler;
}

// 内部: トラップ呼び出しヘルパー
template<typename... Args>
Value ProxyObject::callTrap(const Value& trap, Args&&... args) {
  if (!trap.isFunction()) {
    // TypeError: trap is not a function
    return Value::createUndefined(); // エラーの代わり
  }
  
  // トラップを関数として呼び出す
  std::vector<Value> trapArgs;
  (trapArgs.push_back(std::forward<Args>(args)), ...);
  
  return trap.call(context, handler, trapArgs);
}

// 内部: トラップ結果の検証
void ProxyObject::validateTrapResult(const Value& trapResult, const PropertyKey& key) {
  // 実際の実装では必要に応じてトラップ結果の検証を行う
}

// 内部: 無効化されていないことを確認
void ProxyObject::ensureNotRevoked() const {
  if (revoked) {
    // TypeError: Cannot perform operation on a revoked proxy
    throw std::runtime_error("Cannot perform operation on a revoked proxy");
  }
}

// Proxy作成 API
Value createProxy(const Value& target, const Value& handler, execution::ExecutionContext* context) {
  // targetとhandlerの検証
  if (!target.isObject()) {
    // TypeError: Proxy target must be an object
    return Value::createUndefined(); // エラーの代わり
  }
  
  if (!handler.isObject()) {
    // TypeError: Proxy handler must be an object
    return Value::createUndefined(); // エラーの代わり
  }
  
  // ProxyObject作成
  auto* proxy = new ProxyObject(target, handler, context);
  
  // Proxy型のValueオブジェクトを返す
  return Value::createObject(context, proxy, PROXY_CLASS_ID);
}

// Revocable Proxy作成 API
Value createRevocableProxy(const Value& target, const Value& handler, execution::ExecutionContext* context) {
  // targetとhandlerの検証
  if (!target.isObject()) {
    // TypeError: Proxy target must be an object
    return Value::createUndefined(); // エラーの代わり
  }
  
  if (!handler.isObject()) {
    // TypeError: Proxy handler must be an object
    return Value::createUndefined(); // エラーの代わり
  }
  
  // ProxyObject作成
  auto* proxy = new ProxyObject(target, handler, context);
  
  // Proxy型のValueオブジェクトを作成
  Value proxyValue = Value::createObject(context, proxy, PROXY_CLASS_ID);
  
  // revoke関数を作成
  Value revokeFunc = Value::createFunction(context, [proxy](const std::vector<Value>& args, Value thisValue) -> Value {
    proxy->revoke();
    return Value::createUndefined();
  });
  
  // 結果オブジェクトを作成
  Value result = Value::createObject(context);
  result.setProperty(context, "proxy", proxyValue);
  result.setProperty(context, "revoke", revokeFunc);
  
  return result;
}

// Proxyプロトタイプの初期化
void initProxyPrototype(execution::ExecutionContext* context) {
  // ここで実際のJavaScriptのProxyクラスを初期化
  // 実際の実装では、各メソッドを適切に設定
}

} // namespace builtins
} // namespace runtime
} // namespace core
} // namespace aerojs 