/**
 * @file wasm_module.cpp
 * @brief WebAssembly実装
 * @version 1.0.0
 * @license MIT
 */

#include "wasm_module.h"
#include <cstring>
#include <algorithm>
#include <iostream>

namespace aerojs {
namespace core {
namespace runtime {
namespace wasm {

// -------------- WasmValue 実装 --------------

WasmValue WasmValue::createI32(int32_t value) {
  WasmValue result;
  result.type = WasmValueType::I32;
  result.i32Value = value;
  return result;
}

WasmValue WasmValue::createI64(int64_t value) {
  WasmValue result;
  result.type = WasmValueType::I64;
  result.i64Value = value;
  return result;
}

WasmValue WasmValue::createF32(float value) {
  WasmValue result;
  result.type = WasmValueType::F32;
  result.f32Value = value;
  return result;
}

WasmValue WasmValue::createF64(double value) {
  WasmValue result;
  result.type = WasmValueType::F64;
  result.f64Value = value;
  return result;
}

WasmValue WasmValue::createExternRef(void* value) {
  WasmValue result;
  result.type = WasmValueType::ExternRef;
  result.externRef = value;
  return result;
}

WasmValue WasmValue::createFuncRef(uint32_t value) {
  WasmValue result;
  result.type = WasmValueType::FuncRef;
  result.funcRef = value;
  return result;
}

WasmValue WasmValue::createV128(const uint8_t* value) {
  WasmValue result;
  result.type = WasmValueType::V128;
  std::memcpy(result.v128Value, value, 16);
  return result;
}

Value WasmValue::toJSValue(execution::ExecutionContext* context) const {
  switch (type) {
    case WasmValueType::I32:
      return Value::createNumber(context, static_cast<double>(i32Value));
    
    case WasmValueType::I64:
      // JavaScript数値で表現できる安全な範囲を考慮
      if (i64Value >= -9007199254740991ll && i64Value <= 9007199254740991ll) {
        return Value::createNumber(context, static_cast<double>(i64Value));
      } else {
        // BigIntに変換
        return Value::createBigInt(context, i64Value);
      }
    
    case WasmValueType::F32:
      return Value::createNumber(context, static_cast<double>(f32Value));
    
    case WasmValueType::F64:
      return Value::createNumber(context, f64Value);
    
    case WasmValueType::ExternRef:
      // externRefはすでにJavaScriptオブジェクトへの参照である可能性がある
      if (externRef) {
        // ポインタはオブジェクトIDとして扱う
        return Value::createFromPointer(context, externRef);
      }
      return Value::createNull();
    
    case WasmValueType::FuncRef:
      // 関数参照 (実際の実装ではここでWasmFunctionを参照する必要がある)
      return Value::createFunction(context, [](const std::vector<Value>& args, Value thisValue) -> Value {
        // ダミー関数（実際の実装では適切なWasmFunction呼び出し）
        return Value::createUndefined();
      });
    
    case WasmValueType::V128:
      // SIMDは特殊なハンドリングが必要
      // ここではUint8Arrayとして返す例
      Value arrayBuffer = Value::createArrayBuffer(context, 16);
      uint8_t* data = static_cast<uint8_t*>(arrayBuffer.getArrayBufferData());
      std::memcpy(data, v128Value, 16);
      return arrayBuffer;
    
    default:
      return Value::createUndefined();
  }
}

WasmValue WasmValue::fromJSValue(const Value& value, WasmValueType targetType) {
  switch (targetType) {
    case WasmValueType::I32:
      if (value.isNumber()) {
        return createI32(static_cast<int32_t>(value.toNumber()));
      } else if (value.isBoolean()) {
        return createI32(value.toBoolean() ? 1 : 0);
      }
      return createI32(0);
    
    case WasmValueType::I64:
      if (value.isNumber()) {
        return createI64(static_cast<int64_t>(value.toNumber()));
      } else if (value.isBigInt()) {
        return createI64(value.toBigInt());
      }
      return createI64(0);
    
    case WasmValueType::F32:
      if (value.isNumber()) {
        return createF32(static_cast<float>(value.toNumber()));
      }
      return createF32(0.0f);
    
    case WasmValueType::F64:
      if (value.isNumber()) {
        return createF64(value.toNumber());
      }
      return createF64(0.0);
    
    case WasmValueType::ExternRef:
      if (value.isObject() || value.isFunction()) {
        // オブジェクトの内部ポインタを取得（実際の実装では適切な変換が必要）
        return createExternRef(const_cast<void*>(value.getPointer()));
      } else if (value.isNull()) {
        return createExternRef(nullptr);
      }
      return createExternRef(nullptr);
    
    case WasmValueType::FuncRef:
      if (value.isFunction()) {
        // 関数IDを取得（実際の実装では適切な変換が必要）
        return createFuncRef(0); // ダミー値
      }
      return createFuncRef(0);
    
    case WasmValueType::V128:
      if (value.isArrayBuffer() || value.isTypedArray()) {
        // TypedArrayからデータを取得
        uint8_t v128Data[16] = {0};
        
        if (value.isArrayBuffer() && value.getArrayBufferByteLength() >= 16) {
          const uint8_t* data = static_cast<const uint8_t*>(value.getArrayBufferData());
          std::memcpy(v128Data, data, 16);
        } else if (value.isTypedArray() && value.getTypedArrayByteLength() >= 16) {
          const uint8_t* data = static_cast<const uint8_t*>(value.getTypedArrayData());
          std::memcpy(v128Data, data, 16);
        }
        
        return createV128(v128Data);
      }
      // デフォルトは0で初期化
      uint8_t zeros[16] = {0};
      return createV128(zeros);
    
    default:
      return createI32(0);
  }
}

// -------------- WasmFunctionType 実装 --------------

bool WasmFunctionType::operator==(const WasmFunctionType& other) const {
  return params == other.params && results == other.results;
}

// -------------- WasmInstance 実装 --------------

WasmInstance::WasmInstance() 
  : initialized(false) {
}

WasmInstance::~WasmInstance() {
  // メンバーは自動的に解放される
}

WasmFunction* WasmInstance::getFunction(const std::string& name) const {
  auto it = functions.find(name);
  return it != functions.end() ? it->second.get() : nullptr;
}

WasmMemory* WasmInstance::getMemory(const std::string& name) const {
  auto it = memories.find(name);
  return it != memories.end() ? it->second.get() : nullptr;
}

WasmTable* WasmInstance::getTable(const std::string& name) const {
  auto it = tables.find(name);
  return it != tables.end() ? it->second.get() : nullptr;
}

WasmGlobal* WasmInstance::getGlobal(const std::string& name) const {
  auto it = globals.find(name);
  return it != globals.end() ? it->second.get() : nullptr;
}

void WasmInstance::addFunction(const std::string& name, std::unique_ptr<WasmFunction> function) {
  functions[name] = std::move(function);
}

void WasmInstance::addMemory(const std::string& name, std::unique_ptr<WasmMemory> memory) {
  memories[name] = std::move(memory);
}

void WasmInstance::addTable(const std::string& name, std::unique_ptr<WasmTable> table) {
  tables[name] = std::move(table);
}

void WasmInstance::addGlobal(const std::string& name, std::unique_ptr<WasmGlobal> global) {
  globals[name] = std::move(global);
}

void WasmInstance::initialize() {
  if (initialized) {
    return;
  }
  
  // 開始関数の実行など、必要な初期化を行う
  
  initialized = true;
}

std::vector<std::string> WasmInstance::getFunctionExports() const {
  std::vector<std::string> result;
  result.reserve(functions.size());
  
  for (const auto& pair : functions) {
    result.push_back(pair.first);
  }
  
  return result;
}

std::vector<std::string> WasmInstance::getMemoryExports() const {
  std::vector<std::string> result;
  result.reserve(memories.size());
  
  for (const auto& pair : memories) {
    result.push_back(pair.first);
  }
  
  return result;
}

std::vector<std::string> WasmInstance::getTableExports() const {
  std::vector<std::string> result;
  result.reserve(tables.size());
  
  for (const auto& pair : tables) {
    result.push_back(pair.first);
  }
  
  return result;
}

std::vector<std::string> WasmInstance::getGlobalExports() const {
  std::vector<std::string> result;
  result.reserve(globals.size());
  
  for (const auto& pair : globals) {
    result.push_back(pair.first);
  }
  
  return result;
}

// -------------- WasmMemory 実装 --------------
// 標準的なWasmMemory実装クラス
class StandardWasmMemory : public WasmMemory {
public:
  StandardWasmMemory(uint32_t initialPages, uint32_t maximumPages)
    : data(nullptr),
      sizeInPages(initialPages),
      maxPages(maximumPages) {
    
    // WebAssemblyのページサイズは64KiB固定
    constexpr uint32_t PAGE_SIZE = 65536;
    size_t byteLength = static_cast<size_t>(initialPages) * PAGE_SIZE;
    
    data = new uint8_t[byteLength];
    std::memset(data, 0, byteLength);
  }
  
  ~StandardWasmMemory() override {
    delete[] data;
  }
  
  uint8_t* getData() override {
    return data;
  }
  
  size_t getSize() const override {
    constexpr uint32_t PAGE_SIZE = 65536;
    return static_cast<size_t>(sizeInPages) * PAGE_SIZE;
  }
  
  bool grow(uint32_t pagesToAdd) override {
    constexpr uint32_t PAGE_SIZE = 65536;
    
    if (maxPages > 0 && sizeInPages + pagesToAdd > maxPages) {
      return false; // 最大ページ数を超える
    }
    
    size_t newByteLength = static_cast<size_t>(sizeInPages + pagesToAdd) * PAGE_SIZE;
    uint8_t* newData = new uint8_t[newByteLength];
    
    // 現在のデータをコピー
    size_t currentByteLength = static_cast<size_t>(sizeInPages) * PAGE_SIZE;
    std::memcpy(newData, data, currentByteLength);
    
    // 新しい領域を0で初期化
    std::memset(newData + currentByteLength, 0, newByteLength - currentByteLength);
    
    // データを入れ替え
    delete[] data;
    data = newData;
    sizeInPages += pagesToAdd;
    
    return true;
  }
  
  uint8_t getByte(uint32_t offset) const override {
    if (offset >= getSize()) {
      // 範囲外アクセス
      return 0;
    }
    return data[offset];
  }
  
  void setByte(uint32_t offset, uint8_t value) override {
    if (offset >= getSize()) {
      // 範囲外アクセス
      return;
    }
    data[offset] = value;
  }
  
  int8_t getInt8(uint32_t offset) const override {
    return static_cast<int8_t>(getByte(offset));
  }
  
  uint16_t getUint16(uint32_t offset) const override {
    if (offset + 1 >= getSize()) {
      // 範囲外アクセス
      return 0;
    }
    
    // リトルエンディアンで解釈
    return static_cast<uint16_t>(data[offset]) |
           (static_cast<uint16_t>(data[offset + 1]) << 8);
  }
  
  int32_t getInt32(uint32_t offset) const override {
    if (offset + 3 >= getSize()) {
      // 範囲外アクセス
      return 0;
    }
    
    // リトルエンディアンで解釈
    return static_cast<int32_t>(data[offset]) |
           (static_cast<int32_t>(data[offset + 1]) << 8) |
           (static_cast<int32_t>(data[offset + 2]) << 16) |
           (static_cast<int32_t>(data[offset + 3]) << 24);
  }
  
  float getFloat32(uint32_t offset) const override {
    int32_t bits = getInt32(offset);
    float result;
    std::memcpy(&result, &bits, sizeof(float));
    return result;
  }
  
  double getFloat64(uint32_t offset) const override {
    if (offset + 7 >= getSize()) {
      // 範囲外アクセス
      return 0.0;
    }
    
    // リトルエンディアンで解釈
    int64_t bits = static_cast<int64_t>(data[offset]) |
                   (static_cast<int64_t>(data[offset + 1]) << 8) |
                   (static_cast<int64_t>(data[offset + 2]) << 16) |
                   (static_cast<int64_t>(data[offset + 3]) << 24) |
                   (static_cast<int64_t>(data[offset + 4]) << 32) |
                   (static_cast<int64_t>(data[offset + 5]) << 40) |
                   (static_cast<int64_t>(data[offset + 6]) << 48) |
                   (static_cast<int64_t>(data[offset + 7]) << 56);
    
    double result;
    std::memcpy(&result, &bits, sizeof(double));
    return result;
  }
  
  void setInt8(uint32_t offset, int8_t value) override {
    setByte(offset, static_cast<uint8_t>(value));
  }
  
  void setUint16(uint32_t offset, uint16_t value) override {
    if (offset + 1 >= getSize()) {
      // 範囲外アクセス
      return;
    }
    
    // リトルエンディアンで格納
    data[offset] = static_cast<uint8_t>(value);
    data[offset + 1] = static_cast<uint8_t>(value >> 8);
  }
  
  void setInt32(uint32_t offset, int32_t value) override {
    if (offset + 3 >= getSize()) {
      // 範囲外アクセス
      return;
    }
    
    // リトルエンディアンで格納
    data[offset] = static_cast<uint8_t>(value);
    data[offset + 1] = static_cast<uint8_t>(value >> 8);
    data[offset + 2] = static_cast<uint8_t>(value >> 16);
    data[offset + 3] = static_cast<uint8_t>(value >> 24);
  }
  
  void setFloat32(uint32_t offset, float value) override {
    int32_t bits;
    std::memcpy(&bits, &value, sizeof(float));
    setInt32(offset, bits);
  }
  
  void setFloat64(uint32_t offset, double value) override {
    if (offset + 7 >= getSize()) {
      // 範囲外アクセス
      return;
    }
    
    int64_t bits;
    std::memcpy(&bits, &value, sizeof(double));
    
    // リトルエンディアンで格納
    data[offset] = static_cast<uint8_t>(bits);
    data[offset + 1] = static_cast<uint8_t>(bits >> 8);
    data[offset + 2] = static_cast<uint8_t>(bits >> 16);
    data[offset + 3] = static_cast<uint8_t>(bits >> 24);
    data[offset + 4] = static_cast<uint8_t>(bits >> 32);
    data[offset + 5] = static_cast<uint8_t>(bits >> 40);
    data[offset + 6] = static_cast<uint8_t>(bits >> 48);
    data[offset + 7] = static_cast<uint8_t>(bits >> 56);
  }
  
private:
  uint8_t* data;
  uint32_t sizeInPages;
  uint32_t maxPages;
};

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs 