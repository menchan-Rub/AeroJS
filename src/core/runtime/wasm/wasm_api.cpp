/**
 * @file wasm_api.cpp
 * @brief WebAssembly JavaScript API実装
 * @version 1.0.0
 * @license MIT
 */

#include "wasm_module.h"
#include "../builtins/function/function.h"
#include "../globals/global_object.h"
#include <iostream>
#include <fstream>

namespace aerojs {
namespace core {
namespace runtime {
namespace wasm {

// JavaScript WebAssembly.Memory コンストラクタ
Value wasmMemoryConstructor(const std::vector<Value>& args, Value thisValue, execution::ExecutionContext* context) {
  // 引数チェック
  if (args.size() < 1 || !args[0].isObject()) {
    // TypeError: first argument must be an object
    return Value::createUndefined(); // エラーの代わり
  }
  
  // 引数からパラメータを取得
  Value descriptor = args[0];
  uint32_t initial = descriptor.getProperty(context, "initial").toUint32();
  uint32_t maximum = 0;
  bool hasMaximum = descriptor.hasProperty(context, "maximum");
  
  if (hasMaximum) {
    maximum = descriptor.getProperty(context, "maximum").toUint32();
    
    if (maximum < initial) {
      // RangeError: maximum must not be less than initial
      return Value::createUndefined(); // エラーの代わり
    }
  }
  
  // WasmMemoryを作成
  return createWasmMemory(initial, hasMaximum ? maximum : 0, context);
}

// JavaScript WebAssembly.Table コンストラクタ
Value wasmTableConstructor(const std::vector<Value>& args, Value thisValue, execution::ExecutionContext* context) {
  // 引数チェック
  if (args.size() < 1 || !args[0].isObject()) {
    // TypeError: first argument must be an object
    return Value::createUndefined(); // エラーの代わり
  }
  
  // 引数からパラメータを取得
  Value descriptor = args[0];
  
  Value elementType = descriptor.getProperty(context, "element");
  WasmValueType type = WasmValueType::FuncRef; // デフォルト
  
  if (elementType.isString()) {
    std::string typeStr = elementType.toString();
    if (typeStr == "funcref") {
      type = WasmValueType::FuncRef;
    } else if (typeStr == "externref") {
      type = WasmValueType::ExternRef;
    } else {
      // TypeError: element type must be 'funcref' or 'externref'
      return Value::createUndefined(); // エラーの代わり
    }
  }
  
  uint32_t initial = descriptor.getProperty(context, "initial").toUint32();
  uint32_t maximum = 0;
  bool hasMaximum = descriptor.hasProperty(context, "maximum");
  
  if (hasMaximum) {
    maximum = descriptor.getProperty(context, "maximum").toUint32();
    
    if (maximum < initial) {
      // RangeError: maximum must not be less than initial
      return Value::createUndefined(); // エラーの代わり
    }
  }
  
  // WasmTableを作成
  return createWasmTable(type, initial, hasMaximum ? maximum : 0, context);
}

// JavaScript WebAssembly.Global コンストラクタ
Value wasmGlobalConstructor(const std::vector<Value>& args, Value thisValue, execution::ExecutionContext* context) {
  // 引数チェック
  if (args.size() < 1 || !args[0].isObject()) {
    // TypeError: first argument must be an object
    return Value::createUndefined(); // エラーの代わり
  }
  
  // 引数からパラメータを取得
  Value descriptor = args[0];
  
  Value valueType = descriptor.getProperty(context, "value");
  WasmValueType type = WasmValueType::I32; // デフォルト
  
  if (valueType.isString()) {
    std::string typeStr = valueType.toString();
    if (typeStr == "i32") {
      type = WasmValueType::I32;
    } else if (typeStr == "i64") {
      type = WasmValueType::I64;
    } else if (typeStr == "f32") {
      type = WasmValueType::F32;
    } else if (typeStr == "f64") {
      type = WasmValueType::F64;
    } else {
      // TypeError: value type must be one of 'i32', 'i64', 'f32', 'f64'
      return Value::createUndefined(); // エラーの代わり
    }
  }
  
  bool mutable_ = false;
  if (descriptor.hasProperty(context, "mutable")) {
    mutable_ = descriptor.getProperty(context, "mutable").toBoolean();
  }
  
  Value initialValue = args.size() > 1 ? args[1] : Value::createNumber(context, 0);
  
  // WasmGlobalを作成
  return createWasmGlobal(type, mutable_, initialValue, context);
}

// JavaScript WebAssembly.compile
Value wasmCompile(const std::vector<Value>& args, Value thisValue, execution::ExecutionContext* context) {
  // 引数チェック
  if (args.size() < 1 || (!args[0].isArrayBuffer() && !args[0].isTypedArray())) {
    // TypeError: first argument must be an ArrayBuffer or typed array
    return Value::createUndefined(); // エラーの代わり
  }
  
  std::vector<uint8_t> bytes;
  
  if (args[0].isArrayBuffer()) {
    // ArrayBufferからバイト列を取得
    const uint8_t* data = static_cast<const uint8_t*>(args[0].getArrayBufferData());
    size_t length = args[0].getArrayBufferByteLength();
    bytes.assign(data, data + length);
  } else if (args[0].isTypedArray()) {
    // TypedArrayからバイト列を取得
    const uint8_t* data = static_cast<const uint8_t*>(args[0].getTypedArrayData());
    size_t length = args[0].getTypedArrayByteLength();
    bytes.assign(data, data + length);
  }
  
  // WebAssemblyモジュールをコンパイル
  return compileWasmModule(bytes, context);
}

// JavaScript WebAssembly.instantiate
Value wasmInstantiate(const std::vector<Value>& args, Value thisValue, execution::ExecutionContext* context) {
  // 引数チェック
  if (args.size() < 1) {
    // TypeError: at least one argument required
    return Value::createUndefined(); // エラーの代わり
  }
  
  Value importObject = args.size() > 1 ? args[1] : Value::createObject(context);
  
  if (args[0].isObject() && /* args[0]がWasmModuleオブジェクトか確認 */) {
    // WasmModuleを直接インスタンス化
    WasmModule* module = nullptr; // 実際の実装ではargs[0]からWasmModuleを取得
    
    if (!module) {
      // TypeError: first argument must be a WebAssembly.Module
      return Value::createUndefined(); // エラーの代わり
    }
    
    return instantiateWasmModule(module, importObject, context);
  } else if (args[0].isArrayBuffer() || args[0].isTypedArray()) {
    // バイナリからコンパイルしてインスタンス化
    std::vector<uint8_t> bytes;
    
    if (args[0].isArrayBuffer()) {
      // ArrayBufferからバイト列を取得
      const uint8_t* data = static_cast<const uint8_t*>(args[0].getArrayBufferData());
      size_t length = args[0].getArrayBufferByteLength();
      bytes.assign(data, data + length);
    } else if (args[0].isTypedArray()) {
      // TypedArrayからバイト列を取得
      const uint8_t* data = static_cast<const uint8_t*>(args[0].getTypedArrayData());
      size_t length = args[0].getTypedArrayByteLength();
      bytes.assign(data, data + length);
    }
    
    // コンパイル
    Value moduleValue = compileWasmModule(bytes, context);
    WasmModule* module = nullptr; // 実際の実装ではmoduleValueからWasmModuleを取得
    
    if (!module) {
      // コンパイルエラー
      return Value::createUndefined(); // エラーの代わり
    }
    
    // インスタンス化
    Value instance = instantiateWasmModule(module, importObject, context);
    
    // { module, instance } オブジェクトを返す
    Value result = Value::createObject(context);
    result.setProperty(context, "module", moduleValue);
    result.setProperty(context, "instance", instance);
    
    return result;
  }
  
  // TypeError: first argument must be an ArrayBuffer, typed array, or WebAssembly.Module
  return Value::createUndefined(); // エラーの代わり
}

// JavaScript WebAssembly.validate
Value wasmValidate(const std::vector<Value>& args, Value thisValue, execution::ExecutionContext* context) {
  // 引数チェック
  if (args.size() < 1 || (!args[0].isArrayBuffer() && !args[0].isTypedArray())) {
    // TypeError: first argument must be an ArrayBuffer or typed array
    return Value::createBoolean(false);
  }
  
  std::vector<uint8_t> bytes;
  
  if (args[0].isArrayBuffer()) {
    // ArrayBufferからバイト列を取得
    const uint8_t* data = static_cast<const uint8_t*>(args[0].getArrayBufferData());
    size_t length = args[0].getArrayBufferByteLength();
    bytes.assign(data, data + length);
  } else if (args[0].isTypedArray()) {
    // TypedArrayからバイト列を取得
    const uint8_t* data = static_cast<const uint8_t*>(args[0].getTypedArrayData());
    size_t length = args[0].getTypedArrayByteLength();
    bytes.assign(data, data + length);
  }
  
  // 実際の実装ではWasmバイナリのバリデーション処理
  // ここではシンプルにマジックナンバーのみチェック
  if (bytes.size() >= 4 && bytes[0] == 0x00 && bytes[1] == 0x61 && bytes[2] == 0x73 && bytes[3] == 0x6D) {
    return Value::createBoolean(true);
  }
  
  return Value::createBoolean(false);
}

// WebAssembly JavaScript APIの初期化
void initWasmAPI(execution::ExecutionContext* context) {
  // WebAssemblyグローバルオブジェクトの作成
  Value webAssembly = Value::createObject(context);
  
  // コンストラクタの設定
  Value memoryConstructor = Value::createFunction(context, wasmMemoryConstructor);
  Value tableConstructor = Value::createFunction(context, wasmTableConstructor);
  Value globalConstructor = Value::createFunction(context, wasmGlobalConstructor);
  
  // メソッドの設定
  Value compileFunc = Value::createFunction(context, wasmCompile);
  Value instantiateFunc = Value::createFunction(context, wasmInstantiate);
  Value validateFunc = Value::createFunction(context, wasmValidate);
  
  // WebAssemblyオブジェクトにコンストラクタとメソッドを追加
  webAssembly.setProperty(context, "Memory", memoryConstructor);
  webAssembly.setProperty(context, "Table", tableConstructor);
  webAssembly.setProperty(context, "Global", globalConstructor);
  webAssembly.setProperty(context, "compile", compileFunc);
  webAssembly.setProperty(context, "instantiate", instantiateFunc);
  webAssembly.setProperty(context, "validate", validateFunc);
  
  // グローバルオブジェクトにWebAssemblyを設定
  context->getGlobalObject()->setProperty(context, "WebAssembly", webAssembly);
}

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs 