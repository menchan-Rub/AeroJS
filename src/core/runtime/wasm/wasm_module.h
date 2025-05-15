/**
 * @file wasm_module.h
 * @brief WebAssembly実装
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include "../values/value.h"
#include "../context/execution_context.h"

namespace aerojs {
namespace core {
namespace runtime {
namespace wasm {

// WebAssembly値型
enum class WasmValueType {
  I32,
  I64,
  F32,
  F64,
  ExternRef,
  FuncRef,
  V128  // SIMD
};

// WebAssembly値
struct WasmValue {
  WasmValueType type;
  union {
    int32_t i32Value;
    int64_t i64Value;
    float f32Value;
    double f64Value;
    void* externRef;
    uint32_t funcRef;
    uint8_t v128Value[16];
  };
  
  // コンストラクタ
  static WasmValue createI32(int32_t value);
  static WasmValue createI64(int64_t value);
  static WasmValue createF32(float value);
  static WasmValue createF64(double value);
  static WasmValue createExternRef(void* value);
  static WasmValue createFuncRef(uint32_t value);
  static WasmValue createV128(const uint8_t* value);
  
  // JavaScriptの値に変換
  Value toJSValue(execution::ExecutionContext* context) const;
  
  // JavaScriptの値から変換
  static WasmValue fromJSValue(const Value& value, WasmValueType targetType);
};

// WebAssembly関数型
struct WasmFunctionType {
  std::vector<WasmValueType> params;
  std::vector<WasmValueType> results;
  
  bool operator==(const WasmFunctionType& other) const;
};

// WebAssembly関数インターフェース
class WasmFunction {
public:
  virtual ~WasmFunction() = default;
  
  // 関数呼び出し
  virtual std::vector<WasmValue> call(const std::vector<WasmValue>& args) = 0;
  
  // 関数型取得
  virtual const WasmFunctionType& getFunctionType() const = 0;
  
  // 名前取得
  virtual const std::string& getName() const = 0;
};

// WebAssemblyメモリインターフェース
class WasmMemory {
public:
  virtual ~WasmMemory() = default;
  
  // メモリアクセス
  virtual uint8_t* getData() = 0;
  virtual size_t getSize() const = 0;
  virtual bool grow(uint32_t pages) = 0;
  
  // JavaScriptからのアクセス用
  virtual uint8_t getByte(uint32_t offset) const = 0;
  virtual void setByte(uint32_t offset, uint8_t value) = 0;
  
  // ヘルパー関数
  virtual int8_t getInt8(uint32_t offset) const = 0;
  virtual uint16_t getUint16(uint32_t offset) const = 0;
  virtual int32_t getInt32(uint32_t offset) const = 0;
  virtual float getFloat32(uint32_t offset) const = 0;
  virtual double getFloat64(uint32_t offset) const = 0;
  
  virtual void setInt8(uint32_t offset, int8_t value) = 0;
  virtual void setUint16(uint32_t offset, uint16_t value) = 0;
  virtual void setInt32(uint32_t offset, int32_t value) = 0;
  virtual void setFloat32(uint32_t offset, float value) = 0;
  virtual void setFloat64(uint32_t offset, double value) = 0;
};

// WebAssemblyテーブルインターフェース
class WasmTable {
public:
  virtual ~WasmTable() = default;
  
  // テーブルアクセス
  virtual WasmValue get(uint32_t index) const = 0;
  virtual bool set(uint32_t index, const WasmValue& value) = 0;
  virtual uint32_t size() const = 0;
  virtual bool grow(uint32_t count, const WasmValue& initValue) = 0;
};

// WebAssemblyグローバル変数インターフェース
class WasmGlobal {
public:
  virtual ~WasmGlobal() = default;
  
  // グローバル変数アクセス
  virtual WasmValue getValue() const = 0;
  virtual void setValue(const WasmValue& value) = 0;
  virtual bool isMutable() const = 0;
  virtual WasmValueType getType() const = 0;
};

// WebAssemblyインスタンス（モジュールインスタンス）
class WasmInstance {
public:
  WasmInstance();
  ~WasmInstance();
  
  // メンバーアクセス
  WasmFunction* getFunction(const std::string& name) const;
  WasmMemory* getMemory(const std::string& name) const;
  WasmTable* getTable(const std::string& name) const;
  WasmGlobal* getGlobal(const std::string& name) const;
  
  // メンバー設定
  void addFunction(const std::string& name, std::unique_ptr<WasmFunction> function);
  void addMemory(const std::string& name, std::unique_ptr<WasmMemory> memory);
  void addTable(const std::string& name, std::unique_ptr<WasmTable> table);
  void addGlobal(const std::string& name, std::unique_ptr<WasmGlobal> global);
  
  // 初期化
  void initialize();
  
  // エクスポート取得
  std::vector<std::string> getFunctionExports() const;
  std::vector<std::string> getMemoryExports() const;
  std::vector<std::string> getTableExports() const;
  std::vector<std::string> getGlobalExports() const;
  
private:
  std::unordered_map<std::string, std::unique_ptr<WasmFunction>> functions;
  std::unordered_map<std::string, std::unique_ptr<WasmMemory>> memories;
  std::unordered_map<std::string, std::unique_ptr<WasmTable>> tables;
  std::unordered_map<std::string, std::unique_ptr<WasmGlobal>> globals;
  
  bool initialized;
};

// WebAssemblyモジュール（コンパイルされたWasmコード）
class WasmModule {
public:
  WasmModule();
  ~WasmModule();
  
  // モジュール操作
  bool compile(const std::vector<uint8_t>& bytes);
  std::unique_ptr<WasmInstance> instantiate(
      const std::unordered_map<std::string, std::unordered_map<std::string, Value>>& importObject,
      execution::ExecutionContext* context);
  
  // モジュール情報
  const std::vector<std::string>& getExports() const;
  const std::vector<std::string>& getImports() const;
  
  // WebAssemblyメモリの作成
  static std::unique_ptr<WasmMemory> createMemory(uint32_t initialPages, uint32_t maximumPages);
  
  // WebAssemblyテーブルの作成
  static std::unique_ptr<WasmTable> createTable(WasmValueType type, uint32_t initialSize, uint32_t maximumSize);
  
  // WebAssemblyグローバルの作成
  static std::unique_ptr<WasmGlobal> createGlobal(WasmValueType type, bool isMutable, const WasmValue& initialValue);
  
private:
  // 実際の実装では、ここにWasmバイナリの詳細な解析ロジックが入る
  struct ModuleImpl;
  std::unique_ptr<ModuleImpl> impl;
  
  std::vector<std::string> exports;
  std::vector<std::string> imports;
};

// JavaScriptからアクセスするためのAPI
Value createWasmMemory(uint32_t initialPages, uint32_t maximumPages, execution::ExecutionContext* context);
Value createWasmTable(WasmValueType type, uint32_t initialSize, uint32_t maximumSize, execution::ExecutionContext* context);
Value createWasmGlobal(WasmValueType type, bool isMutable, const Value& initialValue, execution::ExecutionContext* context);
Value compileWasmModule(const std::vector<uint8_t>& bytes, execution::ExecutionContext* context);
Value instantiateWasmModule(WasmModule* module, const Value& importObject, execution::ExecutionContext* context);

// WebAssembly JavaScript APIの初期化
void initWasmAPI(execution::ExecutionContext* context);

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs 