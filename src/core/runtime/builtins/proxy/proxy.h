/**
 * @file proxy.h
 * @brief Proxy実装（ES6+）
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include "../../values/value.h"
#include "../../context/execution_context.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace aerojs {
namespace core {
namespace runtime {
namespace builtins {

// プロパティキー
enum class PropertyKeyType {
  String,
  Symbol
};

struct PropertyKey {
  PropertyKeyType type;
  std::string stringKey;
  uint32_t symbolId;
  
  static PropertyKey fromString(const std::string& key);
  static PropertyKey fromSymbol(uint32_t id);
  
  bool operator==(const PropertyKey& other) const;
  std::string toString() const;
};

// Proxyハンドラトラップ
struct ProxyHandler {
  Value getPrototypeOf;
  Value setPrototypeOf;
  Value isExtensible;
  Value preventExtensions;
  Value getOwnPropertyDescriptor;
  Value defineProperty;
  Value has;
  Value get;
  Value set;
  Value deleteProperty;
  Value ownKeys;
  Value apply;
  Value construct;
  
  // デフォルトハンドラの作成
  static ProxyHandler createDefault(execution::ExecutionContext* context);
};

// ProxyオブジェクトクラスID（内部識別用）
constexpr uint32_t PROXY_CLASS_ID = 0x01000007;

// Proxyオブジェクト
class ProxyObject {
public:
  ProxyObject(const Value& target, const Value& handler, execution::ExecutionContext* context);
  ~ProxyObject();
  
  // 内部メソッド
  Value getPrototypeOf();
  bool setPrototypeOf(const Value& prototype);
  bool isExtensible();
  bool preventExtensions();
  Value getOwnPropertyDescriptor(const PropertyKey& key);
  bool defineProperty(const PropertyKey& key, const Value& descriptor);
  bool has(const PropertyKey& key);
  Value get(const PropertyKey& key, const Value& receiver = Value::createUndefined());
  bool set(const PropertyKey& key, const Value& value, const Value& receiver = Value::createUndefined());
  bool deleteProperty(const PropertyKey& key);
  Value ownKeys();
  Value apply(const Value& thisArg, const std::vector<Value>& args);
  Value construct(const std::vector<Value>& args, const Value& newTarget);
  
  // ユーティリティ
  bool isRevoked() const;
  void revoke();
  
  // アクセサ
  const Value& getTarget() const;
  const Value& getHandler() const;
  
private:
  Value target;
  Value handler;
  ProxyHandler traps;
  execution::ExecutionContext* context;
  bool revoked;
  
  // トラップ呼び出しヘルパー
  template<typename... Args>
  Value callTrap(const Value& trap, Args&&... args);
  
  // バリデーション
  void validateTrapResult(const Value& trapResult, const PropertyKey& key);
  void ensureNotRevoked() const;
};

// Proxy API
Value createProxy(const Value& target, const Value& handler, execution::ExecutionContext* context);
Value createRevocableProxy(const Value& target, const Value& handler, execution::ExecutionContext* context);

// Proxy初期化関数
void initProxyPrototype(execution::ExecutionContext* context);

} // namespace builtins
} // namespace runtime
} // namespace core
} // namespace aerojs 