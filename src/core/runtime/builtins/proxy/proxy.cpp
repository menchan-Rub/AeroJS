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
      context->throwTypeError("getOwnPropertyDescriptor trap returned non-object and non-undefined");
      return Value::createUndefined();
    }
    
    // ターゲットのプロパティディスクリプタを取得
    Value targetDesc = target.getOwnPropertyDescriptor(context, keyValue);
    
    // 不変性チェック
    if (trapResult.isUndefined()) {
      // トラップがundefinedを返した場合
      if (targetDesc.isObject()) {
        // ターゲットに存在するプロパティが設定不可の場合は違反
        bool configurable = targetDesc.getProperty(context, "configurable").toBoolean();
        if (!configurable) {
          context->throwTypeError("getOwnPropertyDescriptor trap violated invariant: cannot report non-configurable property as non-existent");
          return targetDesc;
        }
        
        // 非拡張オブジェクトの場合も違反
        if (!target.isExtensible(context)) {
          context->throwTypeError("getOwnPropertyDescriptor trap violated invariant: cannot report existing property as non-existent on non-extensible object");
          return targetDesc;
        }
      }
    } else {
      // トラップがオブジェクトを返した場合
      if (targetDesc.isObject()) {
        // 設定可能性のチェック
        bool targetConfigurable = targetDesc.getProperty(context, "configurable").toBoolean();
        bool resultConfigurable = trapResult.getProperty(context, "configurable").toBoolean();
        
        if (!targetConfigurable && resultConfigurable) {
          context->throwTypeError("getOwnPropertyDescriptor trap violated invariant: cannot report non-configurable property as configurable");
          return targetDesc;
        }
        
        // 書き込み可能性のチェック（データプロパティの場合）
        if (targetDesc.hasProperty(context, "value")) {
          bool targetWritable = targetDesc.getProperty(context, "writable").toBoolean();
          
          if (!targetConfigurable && !targetWritable) {
            // 設定不可で書き込み不可の場合
            if (trapResult.hasProperty(context, "writable") && 
                trapResult.getProperty(context, "writable").toBoolean()) {
              context->throwTypeError("getOwnPropertyDescriptor trap violated invariant: cannot report non-configurable, non-writable property as writable");
              return targetDesc;
            }
            
            // 値の一致もチェック
            Value targetValue = targetDesc.getProperty(context, "value");
            Value resultValue = trapResult.getProperty(context, "value");
            
            if (!targetValue.equals(resultValue)) {
              context->throwTypeError("getOwnPropertyDescriptor trap violated invariant: non-configurable, non-writable property value must match");
              return targetDesc;
            }
          }
        }
        
        // アクセサプロパティのチェック
        if (targetDesc.hasProperty(context, "get") || targetDesc.hasProperty(context, "set")) {
          if (!targetConfigurable) {
            // getter, setterの一致チェック
            Value targetGetter = targetDesc.getProperty(context, "get");
            Value resultGetter = trapResult.getProperty(context, "get");
            Value targetSetter = targetDesc.getProperty(context, "set");
            Value resultSetter = trapResult.getProperty(context, "set");
            
            if (!targetGetter.equals(resultGetter) || !targetSetter.equals(resultSetter)) {
              context->throwTypeError("getOwnPropertyDescriptor trap violated invariant: non-configurable accessor property must report same accessors");
              return targetDesc;
            }
          }
        }
      } else {
        // ターゲットに存在しないプロパティの場合
        
        // 非拡張オブジェクトには新しいプロパティを報告できない
        if (!target.isExtensible(context)) {
          context->throwTypeError("getOwnPropertyDescriptor trap violated invariant: cannot report new property on non-extensible object");
          return Value::createUndefined();
        }
      }
    }
    
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
    Value targetDesc = target.getOwnPropertyDescriptor(context, keyValue);
    
    // 設定不可プロパティの存在チェック
    if (targetDesc.isObject()) {
      bool targetConfigurable = targetDesc.getProperty(context, "configurable").toBoolean();
      
      if (!targetConfigurable) {
        // 既存の設定不可プロパティを変更できない
        
        // ディスクリプタ（引数）から情報を取得
        bool descConfigurable = descriptor.hasProperty(context, "configurable") ? 
                              descriptor.getProperty(context, "configurable").toBoolean() : true;
                              
        // 設定不可プロパティを設定可能に変更することはできない
        if (descConfigurable) {
          context->throwTypeError("defineProperty trap violated invariant: cannot change configurable attribute of non-configurable property");
          return false;
        }
        
        // データプロパティのチェック
        if (targetDesc.hasProperty(context, "value")) {
          bool targetWritable = targetDesc.getProperty(context, "writable").toBoolean();
          
          if (!targetWritable) {
            // 書き込み不可プロパティを書き込み可能に変更できない
            bool descWritable = descriptor.hasProperty(context, "writable") ?
                              descriptor.getProperty(context, "writable").toBoolean() : false;
                              
            if (descWritable) {
              context->throwTypeError("defineProperty trap violated invariant: cannot change writable attribute of non-writable property");
              return false;
            }
            
            // 値の変更も不可
            if (descriptor.hasProperty(context, "value")) {
              Value targetValue = targetDesc.getProperty(context, "value");
              Value descValue = descriptor.getProperty(context, "value");
              
              if (!targetValue.equals(descValue)) {
                context->throwTypeError("defineProperty trap violated invariant: cannot change value of non-writable property");
                return false;
              }
            }
          }
        }
        
        // アクセサプロパティのチェック
        if (targetDesc.hasProperty(context, "get") || targetDesc.hasProperty(context, "set")) {
          // getter, setterの変更不可
          if (descriptor.hasProperty(context, "get")) {
            Value targetGetter = targetDesc.getProperty(context, "get");
            Value descGetter = descriptor.getProperty(context, "get");
            
            if (!targetGetter.equals(descGetter)) {
              context->throwTypeError("defineProperty trap violated invariant: cannot change getter of non-configurable property");
              return false;
            }
          }
          
          if (descriptor.hasProperty(context, "set")) {
            Value targetSetter = targetDesc.getProperty(context, "set");
            Value descSetter = descriptor.getProperty(context, "set");
            
            if (!targetSetter.equals(descSetter)) {
              context->throwTypeError("defineProperty trap violated invariant: cannot change setter of non-configurable property");
              return false;
            }
          }
        }
      }
    }
    
    // 非拡張オブジェクトの新規プロパティ定義チェック
    if (!target.isExtensible(context) && targetDesc.isUndefined()) {
      context->throwTypeError("defineProperty trap violated invariant: cannot define property on non-extensible object");
      return false;
    }
    
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
      context->throwTypeError("ownKeys trap returned non-object");
      return Value::createArray(context);
    }
    
    // 配列に変換
    Value resultArray = Value::createArray(context);
    
    // trapResultが配列の場合
    if (trapResult.isArray()) {
      uint32_t length = trapResult.getArrayLength(context);
      
      // 各要素をチェック
      for (uint32_t i = 0; i < length; i++) {
        Value element = trapResult.getProperty(context, std::to_string(i));
        
        // 要素が文字列またはシンボルであることを確認
        if (!element.isString() && !element.isSymbol()) {
          context->throwTypeError("ownKeys trap result element must be a string or symbol");
          continue;
        }
        
        // 結果配列に追加
        resultArray.setProperty(context, std::to_string(i), element);
      }
    } else {
      // イテラブルな場合の処理（省略）
      // JavaScriptのオブジェクト列挙順序に従って処理する必要がある
      
      // この実装では単純にプロパティを列挙
      Value keys = trapResult.getOwnPropertyKeys(context);
      if (keys.isArray()) {
        uint32_t length = keys.getArrayLength(context);
        
        uint32_t resultIndex = 0;
        for (uint32_t i = 0; i < length; i++) {
          Value key = keys.getProperty(context, std::to_string(i));
          Value element = trapResult.getProperty(context, key);
          
          if (!element.isString() && !element.isSymbol()) {
            continue;
          }
          
          resultArray.setProperty(context, std::to_string(resultIndex++), element);
        }
      }
    }
    
    // 不変性チェック
    // 1. 非拡張オブジェクトの場合、すべての非設定可能プロパティが結果に含まれる必要がある
    if (!target.isExtensible(context)) {
      Value targetKeys = target.getOwnPropertyKeys(context);
      uint32_t targetLength = targetKeys.getArrayLength(context);
      
      // ターゲットの各プロパティについてチェック
      for (uint32_t i = 0; i < targetLength; i++) {
        Value targetKey = targetKeys.getProperty(context, std::to_string(i));
        Value targetDesc = target.getOwnPropertyDescriptor(context, targetKey);
        
        // 非設定可能プロパティの場合
        if (targetDesc.isObject() && !targetDesc.getProperty(context, "configurable").toBoolean()) {
          // 結果に含まれているか確認
          bool found = false;
          uint32_t resultLength = resultArray.getArrayLength(context);
          
          for (uint32_t j = 0; j < resultLength; j++) {
            Value resultKey = resultArray.getProperty(context, std::to_string(j));
            if (targetKey.equals(resultKey)) {
              found = true;
              break;
            }
          }
          
          if (!found) {
            context->throwTypeError("ownKeys trap violated invariant: non-configurable property must be included");
            break;
          }
        }
      }
      
      // 2. 非拡張オブジェクトの場合、結果に新しいプロパティを含めることはできない
      uint32_t resultLength = resultArray.getArrayLength(context);
      
      for (uint32_t i = 0; i < resultLength; i++) {
        Value resultKey = resultArray.getProperty(context, std::to_string(i));
        
        // ターゲットに存在するかチェック
        bool found = false;
        for (uint32_t j = 0; j < targetLength; j++) {
          Value targetKey = targetKeys.getProperty(context, std::to_string(j));
          if (resultKey.equals(targetKey)) {
            found = true;
            break;
          }
        }
        
        if (!found) {
          context->throwTypeError("ownKeys trap violated invariant: cannot report new property on non-extensible object");
          break;
        }
      }
    }
    
    return resultArray;
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