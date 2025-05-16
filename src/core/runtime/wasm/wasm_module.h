/**
 * @file wasm_module.h
 * @brief WebAssembly実装
 * @version 1.0.0
 * @license MIT
 */

#ifndef AEROJS_WASM_MODULE_H
#define AEROJS_WASM_MODULE_H

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <optional>
#include "../../utils/memory/smart_ptr.h"
#include "../values/value.h"
#include "../context/execution_context.h"

namespace aerojs {
namespace core {
namespace runtime {
namespace wasm {

// 前方宣言
class WasmModuleInstance;
class WasmMemory;
class WasmTable;
class WasmGlobal;
class ExecutionContext;

// WebAssembly値型
enum class WasmValueType {
  I32,
  I64,
  F32,
  F64,
  V128,
  ANYREF,
  FUNCREF
};

// WebAssembly値
class WasmValue {
public:
  // 各型のコンストラクタ
  static WasmValue createI32(int32_t value);
  static WasmValue createI64(int64_t value);
  static WasmValue createF32(float value);
  static WasmValue createF64(double value);
  static WasmValue createAnyRef(Value value);
  static WasmValue createFuncRef(Value func);
  static WasmValue createV128(const uint8_t* data);

  // 値の取得
  int32_t asI32() const;
  int64_t asI64() const;
  float asF32() const;
  double asF64() const;
  Value asAnyRef() const;
  Value asFuncRef() const;
  void getV128(uint8_t* dest) const;

  // 型の取得
  WasmValueType type() const;
  
  // JavaScriptの値に変換
  Value toJSValue(ExecutionContext* context) const;
  
  // JavaScriptの値から変換
  static WasmValue fromJSValue(const Value& value, WasmValueType targetType);

private:
  WasmValueType type_;
  union {
    int32_t i32Value;
    int64_t i64Value;
    float f32Value;
    double f64Value;
    uint64_t refValue; // 参照値のポインタ
    uint8_t v128Value[16];
  } value_;

  // プライベートコンストラクタ
  WasmValue(WasmValueType type);
};

// WebAssembly関数型
struct WasmFunctionType {
  std::vector<WasmValueType> params;
  std::vector<WasmValueType> results;
  
  bool operator==(const WasmFunctionType& other) const;
  size_t hash() const;
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
  
  // JavaScriptの関数に変換
  virtual Value toJSFunction(ExecutionContext* context) = 0;
};

// WebAssemblyモジュール記述子
struct WasmModuleDescriptor {
  std::vector<uint8_t> binary;        // WebAssemblyバイナリ
  std::string name;                   // モジュール名
  bool streaming;                     // ストリーミングコンパイル
  bool debug;                         // デバッグ情報あり
};

// WebAssemblyインポート記述子
struct WasmImportDescriptor {
  std::string module;                 // インポート元モジュール名
  std::string name;                   // インポート名
  enum Kind {
    FUNCTION,
    TABLE,
    MEMORY,
    GLOBAL
  } kind;                             // インポート種類
  
  // インポート情報（共用体の代わりに個別フィールドで管理）
  WasmFunctionType functionType;      // 関数の場合
  
  struct {
    uint32_t min;
    std::optional<uint32_t> max;
    WasmValueType elemType;
  } tableType;                        // テーブルの場合
  
  struct {
    uint32_t min;
    std::optional<uint32_t> max;
    bool shared;
  } memoryType;                       // メモリの場合
  
  struct {
    WasmValueType type;
    bool mutable_;
  } globalType;                       // グローバル変数の場合
};

// WebAssemblyエクスポート記述子
struct WasmExportDescriptor {
  std::string name;                   // エクスポート名
  enum Kind {
    FUNCTION,
    TABLE,
    MEMORY,
    GLOBAL
  } kind;                             // エクスポート種類
  uint32_t index;                     // エクスポート対象のインデックス
};

// WebAssemblyモジュールクラス
class WasmModule : public RefCounted {
public:
  // モジュール作成
  static RefPtr<WasmModule> compile(const WasmModuleDescriptor& descriptor);
  
  // インスタンス化
  RefPtr<WasmModuleInstance> instantiate(ExecutionContext* context, const std::unordered_map<std::string, Value>& importObject);
  
  // モジュール情報取得
  const std::vector<WasmImportDescriptor>& imports() const;
  const std::vector<WasmExportDescriptor>& exports() const;
  std::string name() const;
  size_t codeSize() const;
  bool hasDebugInfo() const;
  
  // カスタムセクション取得
  std::optional<std::vector<uint8_t>> customSection(const std::string& name) const;
  
  // 検証済みかどうか
  bool isValid() const;
  
  // シリアライズ
  std::vector<uint8_t> serialize() const;
  
  // デシリアライズ
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