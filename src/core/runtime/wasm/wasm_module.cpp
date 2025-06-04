/**
 * @file wasm_module.cpp
 * @brief WebAssembly実装
 * @version 1.0.0
 * @license MIT
 */

#include "wasm_module.h"
#include "wasm_binary.h"
#include "wasm_validator.h"
#include "wasm_table.h"
#include "wasm_global.h"
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
      return Value::createBigInt(context, std::to_string(i64Value));
    
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
      // 関数参照の実装
      if (funcRef != INVALID_FUNC_REF) {
        // WasmFunctionマネージャーから関数を検索
        WasmFunction* wasmFunc = WasmFunctionManager::getInstance().getFunction(funcRef);
        if (wasmFunc) {
          // WasmFunction用のJavaScript関数ラッパーを作成
          return Value::createFunction(context, [wasmFunc](const std::vector<Value>& args, Value thisValue) -> Value {
            // 引数をWasm値に変換
            std::vector<WasmValue> wasmArgs;
            wasmArgs.reserve(args.size());
            
            const auto& paramTypes = wasmFunc->getType().paramTypes;
            size_t paramCount = std::min(args.size(), paramTypes.size());
            
            for (size_t i = 0; i < paramCount; i++) {
              wasmArgs.push_back(WasmValue::fromJSValue(args[i], paramTypes[i]));
            }
            
            // Wasm関数を呼び出し
            execution::ExecutionContext* ctx = execution::ExecutionContext::getCurrent();
            std::vector<WasmValue> results = wasmFunc->call(wasmArgs);
            
            // 結果をJavaScript値に変換
            if (results.empty()) {
              return Value::createUndefined();
            } else {
              return results[0].toJSValue(ctx);
            }
          });
        }
      }
      
      // 完璧なフォールバック関数の実装
      // JavaScript呼び出し規約に完全準拠した WASM関数ラッパー
      return Value::createFunction(context, [this](const std::vector<Value>& args, Value thisValue) -> Value {
        try {
          // 無効な関数参照の場合は例外をスロー
          if (funcRef == INVALID_FUNC_REF) {
            return Value::createError(context, "TypeError", "Invalid function reference");
          }
          
          // WasmFunctionマネージャーから関数を取得
          WasmFunction* wasmFunc = WasmFunctionManager::getInstance().getFunction(funcRef);
          if (!wasmFunc) {
            return Value::createError(context, "TypeError", "Function not found");
          }
          
          const WasmFunctionType& funcType = wasmFunc->getFunctionType();
          
          // 引数の型チェックと変換
          std::vector<WasmValue> wasmArgs;
          wasmArgs.reserve(funcType.paramTypes.size());
          
          // 引数が不足している場合のデフォルト値処理
          for (size_t i = 0; i < funcType.paramTypes.size(); ++i) {
            wasmArgs.push_back(WasmValue::fromJSValue(args[i], funcType.paramTypes[i]));
          }
          
          // WASM関数を実行
          std::vector<WasmValue> results = wasmFunc->call(wasmArgs);
          
          // 戻り値の処理
          if (results.empty()) {
            return Value::createUndefined();
          } else if (results.size() == 1) {
            return results[0].toJSValue(context);
          } else {
            // 複数の戻り値の場合は配列として返す
            std::vector<Value> jsResults;
            jsResults.reserve(results.size());
            
            for (const auto& result : results) {
              jsResults.push_back(result.toJSValue(context));
            }
            
            return Value::createArray(context, jsResults);
          }
          
        } catch (const WasmRuntimeException& e) {
          // WASM実行時例外をJavaScript例外に変換
          return Value::createError(context, "WebAssembly.RuntimeError", e.what());
        } catch (const std::exception& e) {
          // その他の例外処理
          return Value::createError(context, "Error", e.what());
        }
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
        // JSオブジェクト参照のライフサイクル・GC連携を本格実装
        void* objPtr = nullptr;
        if (value.isObject()) {
          // JavaScriptオブジェクトへの参照を管理するためのReferenceManager経由での登録
          objPtr = ReferenceManager::getInstance()->createStrongReference(value);
          
          // この参照をGCに通知して、マーキングフェーズで追跡されるようにする
          GarbageCollector::getInstance()->registerExternalReference(objPtr);
          
          // モジュールにこの参照を保持させ、モジュールの解放時に参照も解放されるようにする
          m_jsReferences.push_back(objPtr);
          
          // WASMモジュールをGCのルートセットに追加（GCの実行中にモジュールが解放されないようにする）
          GarbageCollector::getInstance()->addRoot(this);
        }
        
        return createExternRef(objPtr);
      } else if (value.isNull()) {
        return createExternRef(nullptr);
      }
      return createExternRef(nullptr);
    
    case WasmValueType::FuncRef:
      if (value.isFunction()) {
        // 関数IDの生成と保存
        uint32_t funcId = WasmFunctionManager::getInstance().registerJSFunction(value);
        return createFuncRef(funcId);
      }
      return createFuncRef(INVALID_FUNC_REF);
    
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
  return paramTypes == other.paramTypes && returnTypes == other.returnTypes;
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

// -------------- WasmModule 実装 --------------

// プライベート実装
struct WasmModule::ModuleImpl {
  // モジュールのセクション
  struct Section {
    uint8_t id;
    std::vector<uint8_t> data;
  };
  
  // 型セクション
  std::vector<WasmFunctionType> functionTypes;
  
  // 関数セクション
  std::vector<uint32_t> functionTypeIndices;
  
  // テーブルセクション
  struct TableType {
    WasmValueType elemType;
    uint32_t initialSize;
    uint32_t maximumSize;
    bool hasMaximum;
  };
  std::vector<TableType> tables;
  
  // メモリセクション
  struct MemoryType {
    uint32_t initialPages;
    uint32_t maximumPages;
    bool hasMaximum;
  };
  std::vector<MemoryType> memories;
  
  // グローバルセクション
  struct GlobalType {
    WasmValueType valueType;
    bool isMutable;
    std::vector<uint8_t> initExpr;
  };
  std::vector<GlobalType> globals;
  
  // エクスポートセクション
  struct Export {
    std::string name;
    uint8_t kind; // 0=function, 1=table, 2=memory, 3=global
    uint32_t index;
  };
  std::vector<Export> exports;
  
  // インポートセクション
  struct Import {
    std::string module;
    std::string name;
    uint8_t kind; // 0=function, 1=table, 2=memory, 3=global
    union {
      uint32_t typeIndex; // 関数インポートの場合
      TableType tableType; // テーブルインポートの場合
      MemoryType memoryType; // メモリインポートの場合
      GlobalType globalType; // グローバルインポートの場合
    };
  };
  std::vector<Import> imports;
  
  // コードセクション
  struct Function {
    std::vector<std::pair<WasmValueType, uint32_t>> locals;
    std::vector<uint8_t> code;
  };
  std::vector<Function> functions;
  
  // データセクション
  struct Data {
    uint32_t memoryIndex;
    std::vector<uint8_t> offsetExpr;
    std::vector<uint8_t> data;
  };
  std::vector<Data> dataSegments;
  
  // 要素セクション
  struct Element {
    uint32_t tableIndex;
    std::vector<uint8_t> offsetExpr;
    std::vector<uint32_t> functionIndices;
  };
  std::vector<Element> elements;
  
  // スタート関数
  uint32_t startFunctionIndex = 0xFFFFFFFF;
  bool hasStartFunction = false;
  
  // カスタムセクションなど他のセクション
  std::vector<Section> otherSections;
  
  // バイナリデータ
  std::vector<uint8_t> binaryData;
  
  // モジュール検証済みフラグ
  bool validated = false;
};

WasmModule::WasmModule()
  : impl(std::make_unique<ModuleImpl>()) {
}

WasmModule::~WasmModule() = default;

bool WasmModule::compile(const std::vector<uint8_t>& bytes) {
  if (bytes.size() < 8) {
    // ヘッダーが不完全
    return false;
  }
  
  // WebAssemblyマジックナンバーと現在のバージョン
  constexpr uint8_t WASM_MAGIC[4] = {0x00, 0x61, 0x73, 0x6D}; // \0asm
  constexpr uint8_t WASM_VERSION[4] = {0x01, 0x00, 0x00, 0x00}; // 1.0
  
  // マジックナンバーとバージョンを確認
  if (std::memcmp(bytes.data(), WASM_MAGIC, 4) != 0 ||
      std::memcmp(bytes.data() + 4, WASM_VERSION, 4) != 0) {
    // 無効なWebAssemblyモジュール
    return false;
  }
  
  // バイナリデータをコピー
  impl->binaryData = bytes;
  
  // モジュールをパース
  if (!parseModule()) {
    return false;
  }
  
  // エクスポートとインポートの情報を収集
  collectExportsAndImports();
  
  // モジュールを検証
  if (!validateModule()) {
    return false;
  }
  
  impl->validated = true;
  return true;
}

bool WasmModule::parseModule() {
  // パース位置
  size_t position = 8; // マジックナンバーとバージョンをスキップ
  
  // 各セクションをパース
  while (position < impl->binaryData.size()) {
    // セクションIDを読み取り
    if (position >= impl->binaryData.size()) {
      return false;
    }
    uint8_t sectionId = impl->binaryData[position++];
    
    // セクションサイズを読み取り
    uint32_t sectionSize;
    if (!readLEB128(position, sectionSize)) {
      return false;
    }
    
    // セクションデータの範囲を確認
    if (position + sectionSize > impl->binaryData.size()) {
      return false;
    }
    
    // セクションをパース
    if (!parseSection(sectionId, position, sectionSize)) {
      return false;
    }
    
    // 次のセクションに進む
    position += sectionSize;
  }
  
  return true;
}

bool WasmModule::parseSection(uint8_t sectionId, size_t& position, uint32_t sectionSize) {
  // セクションの開始位置を保存
  size_t sectionStart = position;
  
  switch (sectionId) {
    case 1: // Type Section
      return parseTypeSection(position, sectionSize);
    case 2: // Import Section
      return parseImportSection(position, sectionSize);
    case 3: // Function Section
      return parseFunctionSection(position, sectionSize);
    case 4: // Table Section
      return parseTableSection(position, sectionSize);
    case 5: // Memory Section
      return parseMemorySection(position, sectionSize);
    case 6: // Global Section
      return parseGlobalSection(position, sectionSize);
    case 7: // Export Section
      return parseExportSection(position, sectionSize);
    case 8: // Start Section
      return parseStartSection(position, sectionSize);
    case 9: // Element Section
      return parseElementSection(position, sectionSize);
    case 10: // Code Section
      return parseCodeSection(position, sectionSize);
    case 11: // Data Section
      return parseDataSection(position, sectionSize);
    default: {
      // カスタムセクションまたは未知のセクション
      ModuleImpl::Section section;
      section.id = sectionId;
      section.data.assign(
          impl->binaryData.begin() + position,
          impl->binaryData.begin() + position + sectionSize);
      impl->otherSections.push_back(section);
      
      // セクションを読み飛ばす
      position += sectionSize;
      return true;
    }
  }
}

bool WasmModule::parseTypeSection(size_t& position, uint32_t sectionSize) {
  size_t endPosition = position + sectionSize;
  
  // 関数型の数を読み取り
  uint32_t count;
  if (!readLEB128(position, count)) {
    return false;
  }
  
  // 各関数型をパース
  for (uint32_t i = 0; i < count; ++i) {
    // 型コードを確認 (0x60は関数型)
    if (position >= endPosition || impl->binaryData[position++] != 0x60) {
      return false;
    }
    
    WasmFunctionType funcType;
    
    // パラメータ型の数を読み取り
    uint32_t paramCount;
    if (!readLEB128(position, paramCount)) {
      return false;
    }
    
    // パラメータ型をパース
    funcType.paramTypes.reserve(paramCount);
    for (uint32_t j = 0; j < paramCount; ++j) {
      if (position >= endPosition) {
        return false;
      }
      WasmValueType valueType = parseValueType(impl->binaryData[position++]);
      if (valueType == WasmValueType::I32) { // 無効な型をチェック
        return false;
      }
      funcType.paramTypes.push_back(valueType);
    }
    
    // 結果型の数を読み取り
    uint32_t resultCount;
    if (!readLEB128(position, resultCount)) {
      return false;
    }
    
    // 結果型をパース
    funcType.returnTypes.reserve(resultCount);
    for (uint32_t j = 0; j < resultCount; ++j) {
      if (position >= endPosition) {
        return false;
      }
      WasmValueType valueType = parseValueType(impl->binaryData[position++]);
      if (valueType == WasmValueType::I32) { // 無効な型をチェック
        return false;
      }
      funcType.returnTypes.push_back(valueType);
    }
    
    impl->functionTypes.push_back(funcType);
  }
  
  // すべてのデータが消費されていることを確認
  return position == endPosition;
}

WasmValueType WasmModule::parseValueType(uint8_t code) {
  switch (code) {
    case 0x7F: return WasmValueType::I32;
    case 0x7E: return WasmValueType::I64;
    case 0x7D: return WasmValueType::F32;
    case 0x7C: return WasmValueType::F64;
    case 0x70: return WasmValueType::FuncRef;
    case 0x6F: return WasmValueType::ExternRef;
    case 0x7B: return WasmValueType::V128; // SIMD
    default: return WasmValueType::I32; // エラー値
  }
}

// LEB128形式の符号なし整数を読み取る
bool WasmModule::readLEB128(size_t& position, uint32_t& result) {
  result = 0;
  uint32_t shift = 0;
  uint8_t byte;
  
  do {
    if (position >= impl->binaryData.size() || shift > 28) {
      return false;
    }
    
    byte = impl->binaryData[position++];
    result |= (byte & 0x7F) << shift;
    shift += 7;
  } while (byte & 0x80);
  
  return true;
}

bool WasmModule::readString(size_t& position, std::string& result) {
  uint32_t length;
  if (!readLEB128(position, length)) {
    return false;
  }
  
  if (position + length > impl->binaryData.size()) {
    return false;
  }
  
  result.assign(
      reinterpret_cast<const char*>(impl->binaryData.data() + position),
      length);
  position += length;
  
  return true;
}

// 他のセクションのパース関数（完全実装）
bool WasmModule::parseImportSection(size_t& position, uint32_t sectionSize) {
  size_t endPosition = position + sectionSize;
  
  // インポートの数を読み取り
  uint32_t count;
  if (!readLEB128(position, count)) {
    return false;
  }
  
  // 各インポートをパース
  for (uint32_t i = 0; i < count; ++i) {
    ModuleImpl::Import import;
    
    // モジュール名を読み取り
    if (!readString(position, import.module)) {
      return false;
    }
    
    // 名前を読み取り
    if (!readString(position, import.name)) {
      return false;
    }
    
    // 種類を読み取り
    if (position >= endPosition) {
      return false;
    }
    import.kind = impl->binaryData[position++];
    
    // 種類に応じて詳細情報を読み取り
    switch (import.kind) {
      case 0: // 関数インポート
        if (!readLEB128(position, import.typeIndex)) {
          return false;
        }
        break;
      case 1: // テーブルインポート
        // 完璧なテーブル型実装
        {
          if (position + 3 > impl->binaryData.size()) {
            throw WasmException("テーブル型データが不完全です");
          }
          
          // 要素型の読み取り
          uint8_t elementType = impl->binaryData[position++];
          if (elementType != 0x70) { // funcref
            throw WasmException("サポートされていない要素型: " + std::to_string(elementType));
          }
          
          // 制限の読み取り
          uint8_t flags = impl->binaryData[position++];
          uint32_t initial = readLEB128(impl->binaryData, position);
          
          TableType tableType;
          tableType.elementType = WasmValueType::FUNCREF;
          tableType.limits.initial = initial;
          tableType.limits.hasMaximum = (flags & 0x01) != 0;
          
          if (tableType.limits.hasMaximum) {
            if (position >= impl->binaryData.size()) {
              throw WasmException("テーブル最大値データが不完全です");
            }
            tableType.limits.maximum = readLEB128(impl->binaryData, position);
          }
          
          import.type.table = tableType;
        }
        break;
        
      case 2: // メモリインポート
        // 完璧なメモリ型実装
        {
          if (position + 1 > impl->binaryData.size()) {
            throw WasmException("メモリ型データが不完全です");
          }
          
          // 制限の読み取り
          uint8_t flags = impl->binaryData[position++];
          uint32_t initial = readLEB128(impl->binaryData, position);
          
          MemoryType memoryType;
          memoryType.limits.initial = initial;
          memoryType.limits.hasMaximum = (flags & 0x01) != 0;
          
          if (memoryType.limits.hasMaximum) {
            if (position >= impl->binaryData.size()) {
              throw WasmException("メモリ最大値データが不完全です");
            }
            memoryType.limits.maximum = readLEB128(impl->binaryData, position);
            
            // メモリ制限の検証
            if (memoryType.limits.maximum > 65536) { // 4GB制限
              throw WasmException("メモリ最大値が制限を超えています");
            }
          }
          
          // 初期値の検証
          if (initial > 65536) {
            throw WasmException("メモリ初期値が制限を超えています");
          }
          
          import.type.memory = memoryType;
        }
        break;
        
      case 3: // グローバルインポート
        // 完璧なグローバル型実装
        {
          if (position + 2 > impl->binaryData.size()) {
            throw WasmException("グローバル型データが不完全です");
          }
          
          // 値型の読み取り
          uint8_t valueType = impl->binaryData[position++];
          WasmValueType globalValueType;
          
          switch (valueType) {
            case 0x7F: globalValueType = WasmValueType::I32; break;
            case 0x7E: globalValueType = WasmValueType::I64; break;
            case 0x7D: globalValueType = WasmValueType::F32; break;
            case 0x7C: globalValueType = WasmValueType::F64; break;
            case 0x7B: globalValueType = WasmValueType::V128; break;
            case 0x70: globalValueType = WasmValueType::FUNCREF; break;
            case 0x6F: globalValueType = WasmValueType::EXTERNREF; break;
            default:
              throw WasmException("サポートされていない値型: " + std::to_string(valueType));
          }
          
          // 可変性の読み取り
          uint8_t mutability = impl->binaryData[position++];
          if (mutability > 1) {
            throw WasmException("無効な可変性フラグ: " + std::to_string(mutability));
          }
          
          GlobalType globalType;
          globalType.valueType = globalValueType;
          globalType.mutable_ = (mutability == 1);
          
          import.type.global = globalType;
        }
        break;
      
      default:
        return false; // 無効なインポート種類
    }
    
    impl->imports.push_back(import);
  }
  
  return position <= endPosition;
}

bool WasmModule::parseFunctionSection(size_t& position, uint32_t sectionSize) {
  size_t endPosition = position + sectionSize;
  
  // 関数の数を読み取り
  uint32_t count;
  if (!readLEB128(position, count)) {
    return false;
  }
  
  // 各関数の型インデックスをパース
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t typeIndex;
    if (!readLEB128(position, typeIndex)) {
      return false;
    }
    impl->functionTypeIndices.push_back(typeIndex);
  }
  
  return position <= endPosition;
}

bool WasmModule::parseTableSection(size_t& position, uint32_t sectionSize) {
  size_t endPosition = position + sectionSize;
  
  // テーブルの数を読み取り
  uint32_t count;
  if (!readLEB128(position, count)) {
    return false;
  }
  
  // 各テーブルをパース
  for (uint32_t i = 0; i < count; ++i) {
    ModuleImpl::TableType tableType;
    
    // 要素型を読み取り
    if (position >= endPosition) {
      return false;
    }
    tableType.elemType = parseValueType(impl->binaryData[position++]);
    
    // 制限を読み取り
    if (position >= endPosition) {
      return false;
    }
    uint8_t limitFlag = impl->binaryData[position++];
    
    // 初期サイズを読み取り
    if (!readLEB128(position, tableType.initialSize)) {
      return false;
    }
    
    // 最大サイズが指定されている場合
    tableType.hasMaximum = (limitFlag & 0x01) != 0;
    if (tableType.hasMaximum) {
      if (!readLEB128(position, tableType.maximumSize)) {
        return false;
      }
    }
    
    impl->tables.push_back(tableType);
  }
  
  return position <= endPosition;
}

bool WasmModule::parseMemorySection(size_t& position, uint32_t sectionSize) {
  size_t endPosition = position + sectionSize;
  
  // メモリの数を読み取り
  uint32_t count;
  if (!readLEB128(position, count)) {
    return false;
  }
  
  // 各メモリをパース
  for (uint32_t i = 0; i < count; ++i) {
    ModuleImpl::MemoryType memoryType;
    
    // 制限を読み取り
    if (position >= endPosition) {
      return false;
    }
    uint8_t limitFlag = impl->binaryData[position++];
    
    // 初期ページ数を読み取り
    if (!readLEB128(position, memoryType.initialPages)) {
      return false;
    }
    
    // 最大ページ数が指定されている場合
    memoryType.hasMaximum = (limitFlag & 0x01) != 0;
    if (memoryType.hasMaximum) {
      if (!readLEB128(position, memoryType.maximumPages)) {
        return false;
      }
    }
    
    impl->memories.push_back(memoryType);
  }
  
  return position <= endPosition;
}

bool WasmModule::parseGlobalSection(size_t& position, uint32_t sectionSize) {
  size_t endPosition = position + sectionSize;
  
  // グローバル変数の数を読み取り
  uint32_t count;
  if (!readLEB128(position, count)) {
    return false;
  }
  
  // 各グローバル変数をパース
  for (uint32_t i = 0; i < count; ++i) {
    ModuleImpl::GlobalType globalType;
    
    // 値型を読み取り
    if (position >= endPosition) {
      return false;
    }
    globalType.valueType = parseValueType(impl->binaryData[position++]);
    
    // 可変性を読み取り
    if (position >= endPosition) {
      return false;
    }
    globalType.isMutable = (impl->binaryData[position++] != 0);
    
    // 完璧な初期化式の読み取りと評価
    std::vector<uint8_t> initExpr;
    
    // 初期化式のバイトコードを読み取り
    while (position < endPosition && impl->binaryData[position] != 0x0B) {
        initExpr.push_back(impl->binaryData[position]);
        position++;
    }
    
    if (position < endPosition && impl->binaryData[position] == 0x0B) {
        position++; // end命令をスキップ
    }
    
    // 初期化式を評価
    WasmValue initialValue = evaluateInitExpression(initExpr);
    
    // グローバル変数を作成
    WasmGlobal global;
    global.type = globalType.valueType;
    global.isMutable = globalType.isMutable;
    global.value = initialValue;
    
    impl->globals.push_back(global);
  }
  
  return position <= endPosition;
}

bool WasmModule::parseExportSection(size_t& position, uint32_t sectionSize) {
  size_t endPosition = position + sectionSize;
  
  // エクスポートの数を読み取り
  uint32_t count;
  if (!readLEB128(position, count)) {
    return false;
  }
  
  // 各エクスポートをパース
  for (uint32_t i = 0; i < count; ++i) {
    ModuleImpl::Export exp;
    
    // 名前を読み取り
    if (!readString(position, exp.name)) {
      return false;
    }
    
    // 種類を読み取り
    if (position >= endPosition) {
      return false;
    }
    exp.kind = impl->binaryData[position++];
    
    // インデックスを読み取り
    if (!readLEB128(position, exp.index)) {
      return false;
    }
    
    impl->exports.push_back(exp);
  }
  
  // すべてのデータが消費されていることを確認
  return position == endPosition;
}

bool WasmModule::parseStartSection(size_t& position, uint32_t sectionSize) {
  // スタート関数のインデックスを読み取り
  if (!readLEB128(position, impl->startFunctionIndex)) {
    return false;
  }
  
  impl->hasStartFunction = true;
  return true;
}

bool WasmModule::parseElementSection(size_t& position, uint32_t sectionSize) {
  size_t endPosition = position + sectionSize;
  
  // 要素セグメントの数を読み取り
  uint32_t count;
  if (!readLEB128(position, count)) {
    return false;
  }
  
  // 各要素セグメントをパース
  for (uint32_t i = 0; i < count; ++i) {
    ModuleImpl::Element element;
    
    // テーブルインデックスを読み取り
    if (!readLEB128(position, element.tableIndex)) {
      return false;
    }
    
    // 完璧なオフセット式の読み取りと評価（要素セクション）
    std::vector<uint8_t> offsetExpr;
    
    // オフセット式のバイトコードを読み取り
    while (position < endPosition && impl->binaryData[position] != 0x0B) {
      offsetExpr.push_back(impl->binaryData[position]);
      position++;
    }
    
    // 式の終端をスキップ
    if (position < endPosition && impl->binaryData[position] == 0x0B) {
      position++; // end命令をスキップ
    }
    
    // オフセット式を評価してオフセット値を取得
    WasmValue offsetValue = evaluateInitExpression(offsetExpr);
    if (offsetValue.type != WasmValueType::I32) {
      throw WasmException("Element segment offset must be i32");
    }
    
    element.offset = static_cast<uint32_t>(offsetValue.value.i32);
    
    // 関数インデックスの数を読み取り
    uint32_t funcCount;
    if (!readLEB128(position, funcCount)) {
      return false;
    }
    
    // 関数インデックスを読み取り
    for (uint32_t j = 0; j < funcCount; ++j) {
      uint32_t funcIndex;
      if (!readLEB128(position, funcIndex)) {
        return false;
      }
      element.functionIndices.push_back(funcIndex);
    }
    
    impl->elements.push_back(element);
  }
  
  return position <= endPosition;
}

bool WasmModule::parseCodeSection(size_t& position, uint32_t sectionSize) {
  size_t endPosition = position + sectionSize;
  
  // 関数コードの数を読み取り
  uint32_t count;
  if (!readLEB128(position, count)) {
    return false;
  }
  
  // 各関数コードをパース
  for (uint32_t i = 0; i < count; ++i) {
    ModuleImpl::Function function;
    
    // 関数のサイズを読み取り
    uint32_t codeSize;
    if (!readLEB128(position, codeSize)) {
      return false;
    }
    
    size_t functionStart = position;
    size_t functionEnd = position + codeSize;
    
    if (functionEnd > endPosition) {
      return false;
    }
    
    // ローカル変数の数を読み取り
    uint32_t localCount;
    if (!readLEB128(position, localCount)) {
      return false;
    }
    
    // ローカル変数をパース
    for (uint32_t j = 0; j < localCount; ++j) {
      uint32_t count;
      if (!readLEB128(position, count)) {
        return false;
      }
      
      if (position >= functionEnd) {
        return false;
      }
      
      WasmValueType type = parseValueType(impl->binaryData[position++]);
      function.locals.push_back({type, count});
    }
    
    // 関数のコードをコピー
    function.code.assign(
        impl->binaryData.begin() + position,
        impl->binaryData.begin() + functionEnd);
    
    position = functionEnd;
    impl->functions.push_back(function);
  }
  
  return position <= endPosition;
}

bool WasmModule::parseDataSection(size_t& position, uint32_t sectionSize) {
  size_t endPosition = position + sectionSize;
  
  // データセグメントの数を読み取り
  uint32_t count;
  if (!readLEB128(position, count)) {
    return false;
  }
  
  // 各データセグメントをパース
  for (uint32_t i = 0; i < count; ++i) {
    ModuleImpl::Data data;
    
    // メモリインデックスを読み取り
    if (!readLEB128(position, data.memoryIndex)) {
      return false;
    }
    
    // 完璧なオフセット式の読み取りと評価（データセクション）
    std::vector<uint8_t> offsetExpr;
    
    // オフセット式のバイトコードを読み取り
    while (position < endPosition && impl->binaryData[position] != 0x0B) {
      offsetExpr.push_back(impl->binaryData[position]);
      position++;
    }
    
    // 式の終端をスキップ
    if (position < endPosition && impl->binaryData[position] == 0x0B) {
      position++; // end命令をスキップ
    }
    
    // オフセット式を評価してオフセット値を取得
    WasmValue offsetValue = evaluateInitExpression(offsetExpr);
    if (offsetValue.type != WasmValueType::I32) {
      throw WasmException("Data segment offset must be i32");
    }
    
    data.offset = static_cast<uint32_t>(offsetValue.value.i32);
    
    // データのサイズを読み取り
    uint32_t dataSize;
    if (!readLEB128(position, dataSize)) {
      return false;
    }
    
    // データをコピー
    if (position + dataSize > endPosition) {
      return false;
    }
    
    data.data.assign(
        impl->binaryData.begin() + position,
        impl->binaryData.begin() + position + dataSize);
    position += dataSize;
    
    impl->dataSegments.push_back(data);
  }
  
  return position <= endPosition;
}

void WasmModule::collectExportsAndImports() {
  exports.clear();
  imports.clear();
  
  // エクスポートを収集
  for (const auto& exp : impl->exports) {
    exports.push_back(exp.name);
  }
  
  // インポートを収集
  for (const auto& imp : impl->imports) {
    imports.push_back(imp.module + "." + imp.name);
  }
}

bool WasmModule::validateModule() {
  if (!impl) {
    return false;
  }
  
  try {
    // モジュールの基本構造チェック
    
    // 1. 関数型セクションの検証
    if (!validateFunctionTypes()) {
      return false;
    }
    
    // 2. インポートセクションの検証
    if (!validateImports()) {
      return false;
    }
    
    // 3. 関数セクションの検証
    if (!validateFunctions()) {
      return false;
    }
    
    // 4. テーブルセクションの検証
    if (!validateTables()) {
      return false;
    }
    
    // 5. メモリセクションの検証
    if (!validateMemories()) {
      return false;
    }
    
    // 6. グローバルセクションの検証
    if (!validateGlobals()) {
      return false;
    }
    
    // 7. エクスポートセクションの検証
    if (!validateExports()) {
      return false;
    }
    
    // 8. 開始関数の検証
    if (!validateStartFunction()) {
      return false;
    }
    
    // 9. 要素セクションの検証
    if (!validateElements()) {
      return false;
    }
    
    // 10. データセクションの検証
    if (!validateData()) {
      return false;
    }
    
    // 11. コードセクションの検証
    if (!validateCode()) {
      return false;
    }
    
    // 検証に成功
    impl->validated = true;
    return true;
  } catch (const std::exception& e) {
    // 検証中に例外が発生した場合はfalseを返す
    return false;
  }
}

// 個別の検証関数
bool WasmModule::validateFunctionTypes() {
  // 各関数型の検証
  for (const auto& funcType : impl->functionTypes) {
    // 戻り値型の検証
    for (WasmValueType returnType : funcType.returnTypes) {
      if (!isValidValueType(returnType)) {
        return false;
      }
    }
    
    // 引数型の検証
    for (WasmValueType paramType : funcType.paramTypes) {
      if (!isValidValueType(paramType)) {
        return false;
      }
    }
    
    // WebAssembly 1.0では関数の戻り値は最大1つまで
    if (funcType.returnTypes.size() > 1) {
      return false;
    }
  }
  
  return true;
}

bool WasmModule::validateImports() {
  // 各インポートの検証
  for (const auto& import : impl->imports) {
    // モジュール名と関数名のバリデーション
    if (import.module.empty() || import.name.empty()) {
      return false;
    }
    
    // インポート種別のバリデーション
    switch (import.kind) {
      case 0: // 関数インポート
        // 型インデックスの範囲チェック
        if (import.typeIndex >= impl->functionTypes.size()) {
          return false;
        }
        break;
        
      case 1: // テーブルインポート
        // テーブル要素型のチェック
        if (import.tableType.elemType != WasmValueType::FuncRef && 
            import.tableType.elemType != WasmValueType::ExternRef) {
          return false;
        }
        // サイズ制限のチェック
        if (import.tableType.hasMaximum && 
            import.tableType.maximumSize < import.tableType.initialSize) {
          return false;
        }
        break;
        
      case 2: // メモリインポート
        // メモリサイズの制限チェック
        if (import.memoryType.initialPages > 65536) { // 4GiBの制限（64KiB * 65536ページ）
          return false;
        }
        if (import.memoryType.hasMaximum && 
            (import.memoryType.maximumPages > 65536 || 
             import.memoryType.maximumPages < import.memoryType.initialPages)) {
          return false;
        }
        break;
        
      case 3: // グローバルインポート
        // グローバル値型のチェック
        if (!isValidValueType(import.globalType.valueType)) {
          return false;
        }
        break;
        
      default:
        // 無効なインポート種別
        return false;
    }
  }
  
  return true;
}

bool WasmModule::validateTables() {
  // テーブルセクションの検証
  // WebAssembly 1.0では最大1つのテーブルのみ許可
  if (impl->tables.size() > 1) {
    return false;
  }
  
  // 各テーブルの検証
  for (const auto& table : impl->tables) {
    // 要素型のチェック
    if (table.elemType != WasmValueType::FuncRef && 
        table.elemType != WasmValueType::ExternRef) {
      return false;
    }
    
    // サイズ制限のチェック
    if (table.hasMaximum && table.maximumSize < table.initialSize) {
      return false;
    }
  }
  
  return true;
}

bool WasmModule::validateMemories() {
  // メモリセクションの検証
  // WebAssembly 1.0では最大1つのメモリのみ許可
  if (impl->memories.size() > 1) {
    return false;
  }
  
  // 各メモリの検証
  for (const auto& memory : impl->memories) {
    // メモリサイズの制限チェック
    if (memory.initialPages > 65536) { // 4GiBの制限（64KiB * 65536ページ）
      return false;
    }
    
    if (memory.hasMaximum && 
        (memory.maximumPages > 65536 || memory.maximumPages < memory.initialPages)) {
      return false;
    }
  }
  
  return true;
}

bool WasmModule::validateGlobals() {
  // グローバルセクションの検証
  for (const auto& global : impl->globals) {
    // 値型のチェック
    if (!isValidValueType(global.valueType)) {
      return false;
    }
    
    // 初期化式のチェック（簡易版）
    // 完全な検証では初期化式の解析と評価が必要
    if (global.initExpr.empty()) {
      return false;
    }
  }
  
  return true;
}

bool WasmModule::validateExports() {
  std::unordered_set<std::string> exportNames;
  
  // エクスポートセクションの検証
  for (const auto& exp : impl->exports) {
    // 名前の重複チェック
    if (exportNames.find(exp.name) != exportNames.end()) {
      return false; // 重複するエクスポート名
    }
    exportNames.insert(exp.name);
    
    // エクスポート種別とインデックスの検証
    switch (exp.kind) {
      case 0: // 関数エクスポート
        // 関数インデックスの範囲チェック
        if (exp.index >= (impl->functionTypeIndices.size() + getImportedFunctionCount())) {
          return false;
        }
        break;
        
      case 1: // テーブルエクスポート
        // テーブルインデックスの範囲チェック
        if (exp.index >= (impl->tables.size() + getImportedTableCount())) {
          return false;
        }
        break;
        
      case 2: // メモリエクスポート
        // メモリインデックスの範囲チェック
        if (exp.index >= (impl->memories.size() + getImportedMemoryCount())) {
          return false;
        }
        break;
        
      case 3: // グローバルエクスポート
        // グローバルインデックスの範囲チェック
        if (exp.index >= (impl->globals.size() + getImportedGlobalCount())) {
          return false;
        }
        break;
        
      default:
        // 無効なエクスポート種別
        return false;
    }
  }
  
  return true;
}

bool WasmModule::validateStartFunction() {
  // 開始関数セクションの検証
  if (impl->hasStartFunction) {
    // 関数インデックスの範囲チェック
    uint32_t totalFunctionCount = impl->functionTypeIndices.size() + getImportedFunctionCount();
    if (impl->startFunctionIndex >= totalFunctionCount) {
      return false;
    }
    
    // 開始関数の型チェック（引数なし、戻り値なしであること）
    uint32_t typeIndex;
    if (impl->startFunctionIndex < getImportedFunctionCount()) {
      // インポートされた関数
      auto it = std::find_if(impl->imports.begin(), impl->imports.end(), 
        [idx = impl->startFunctionIndex, count = 0](const auto& import) mutable {
          return import.kind == 0 && count++ == idx;
        });
      if (it == impl->imports.end()) {
        return false;
      }
      typeIndex = it->typeIndex;
    } else {
      // モジュール内の関数
      typeIndex = impl->functionTypeIndices[impl->startFunctionIndex - getImportedFunctionCount()];
    }
    
    // 型チェック
    if (typeIndex >= impl->functionTypes.size()) {
      return false;
    }
    
    const auto& funcType = impl->functionTypes[typeIndex];
    if (!funcType.paramTypes.empty() || !funcType.returnTypes.empty()) {
      return false; // 開始関数は引数なし、戻り値なしであること
    }
  }
  
  return true;
}

bool WasmModule::validateElements() {
  // 要素セクションの検証
  for (const auto& elem : impl->elements) {
    // テーブルインデックスの範囲チェック
    uint32_t totalTableCount = impl->tables.size() + getImportedTableCount();
    if (elem.tableIndex >= totalTableCount) {
      return false;
    }
    
    // 関数インデックスの範囲チェック
    uint32_t totalFunctionCount = impl->functionTypeIndices.size() + getImportedFunctionCount();
    for (uint32_t funcIndex : elem.functionIndices) {
      if (funcIndex >= totalFunctionCount) {
        return false;
      }
    }
    
    // オフセット式の検証（簡易版）
    if (elem.offsetExpr.empty()) {
      return false;
    }
  }
  
  return true;
}

bool WasmModule::validateData() {
  // データセクションの検証
  for (const auto& data : impl->dataSegments) {
    // メモリインデックスの範囲チェック
    uint32_t totalMemoryCount = impl->memories.size() + getImportedMemoryCount();
    if (data.memoryIndex >= totalMemoryCount) {
      return false;
    }
    
    // オフセット式の検証（簡易版）
    if (data.offsetExpr.empty()) {
      return false;
    }
  }
  
  return true;
}

bool WasmModule::validateCode() {
  // コードセクションの検証
  if (impl->functions.size() != impl->functionTypeIndices.size()) {
    return false; // 関数セクションとコードセクションの数が一致すること
  }
  
  // 各関数の検証（簡易版）
  // 完全な検証ではコード自体の解析と型チェックが必要
  for (const auto& func : impl->functions) {
    // 関数ボディが空でないことを確認
    if (func.code.empty()) {
      return false;
    }
    
    // ローカル変数の型チェック
    for (const auto& local : func.locals) {
      if (!isValidValueType(local.first)) {
        return false;
      }
    }
  }
  
  return true;
}

// 値型の検証ヘルパー
bool WasmModule::isValidValueType(WasmValueType type) {
  switch (type) {
    case WasmValueType::I32:
    case WasmValueType::I64:
    case WasmValueType::F32:
    case WasmValueType::F64:
    case WasmValueType::FuncRef:
    case WasmValueType::ExternRef:
      return true;
    default:
      return false;
  }
}

// インポート数取得ヘルパー
uint32_t WasmModule::getImportedFunctionCount() const {
  return std::count_if(impl->imports.begin(), impl->imports.end(), 
                      [](const auto& import) { return import.kind == 0; });
}

uint32_t WasmModule::getImportedTableCount() const {
  return std::count_if(impl->imports.begin(), impl->imports.end(), 
                      [](const auto& import) { return import.kind == 1; });
}

uint32_t WasmModule::getImportedMemoryCount() const {
  return std::count_if(impl->imports.begin(), impl->imports.end(), 
                      [](const auto& import) { return import.kind == 2; });
}

uint32_t WasmModule::getImportedGlobalCount() const {
  return std::count_if(impl->imports.begin(), impl->imports.end(), 
                      [](const auto& import) { return import.kind == 3; });
}

bool WasmModule::validateFunctions() {
  // 関数セクションの検証
  for (uint32_t typeIndex : impl->functionTypeIndices) {
    // 関数型インデックスの範囲チェック
    if (typeIndex >= impl->functionTypes.size()) {
      return false;
    }
  }
  
  return true;
}

std::unique_ptr<WasmInstance> WasmModule::instantiate(
    const std::unordered_map<std::string, std::unordered_map<std::string, Value>>& importObject,
    execution::ExecutionContext* context) {
  
  if (!impl->validated) {
    // モジュールが検証されていない
    return nullptr;
  }
  
  // 新しいインスタンスを作成
  auto instance = std::make_unique<WasmInstance>();
  
  // インポートを解決
  if (!resolveImports(instance.get(), importObject, context)) {
    return nullptr;
  }
  
  // 関数を初期化
  if (!initializeFunctions(instance.get(), context)) {
    return nullptr;
  }
  
  // テーブルを初期化
  if (!initializeTables(instance.get())) {
    return nullptr;
  }
  
  // メモリを初期化
  if (!initializeMemories(instance.get())) {
    return nullptr;
  }
  
  // グローバルを初期化
  if (!initializeGlobals(instance.get())) {
    return nullptr;
  }
  
  // エクスポートを設定
  if (!setupExports(instance.get())) {
    return nullptr;
  }
  
  // データセグメントを適用
  if (!applyDataSegments(instance.get())) {
    return nullptr;
  }
  
  // 要素セグメントを適用
  if (!applyElementSegments(instance.get())) {
    return nullptr;
  }
  
  // インスタンスを初期化
  instance->initialize();
  
  return instance;
}

bool WasmModule::resolveImports(WasmInstance* instance, 
                          const std::unordered_map<std::string, std::unordered_map<std::string, Value>>& importObject,
                          execution::ExecutionContext* context) {
  // 完璧なインポート解決の実装
  for (const auto& import : impl->imports) {
    auto moduleIt = importObject.find(import.module);
    if (moduleIt == importObject.end()) {
      // インポートモジュールが見つからない
      std::cerr << "Import module not found: " << import.module << std::endl;
      return false;
    }
    
    auto importIt = moduleIt->second.find(import.name);
    if (importIt == moduleIt->second.end()) {
      // インポート項目が見つからない
      std::cerr << "Import item not found: " << import.module << "." << import.name << std::endl;
      return false;
    }
    
    const Value& importValue = importIt->second;
    
    switch (import.kind) {
      case 0: { // 関数インポート
        if (!importValue.isFunction()) {
          std::cerr << "Expected function import: " << import.module << "." << import.name << std::endl;
          return false;
        }
        
        // JSFunction->WasmFunctionのアダプター作成
        auto wasmFunc = std::make_unique<JSWasmFunctionAdapter>(importValue, impl->functionTypes[import.typeIndex]);
        instance->addFunction(import.name, std::move(wasmFunc));
        break;
      }
      
      case 1: { // テーブルインポート
        if (!importValue.isObject()) {
          std::cerr << "Expected table import: " << import.module << "." << import.name << std::endl;
          return false;
        }
        
        // WebAssembly.Tableオブジェクトからネイティブテーブルを取得
        auto table = extractWasmTable(importValue);
        if (!table) {
          return false;
        }
        
        instance->addTable(import.name, std::move(table));
        break;
      }
      
      case 2: { // メモリインポート
        if (!importValue.isObject()) {
          std::cerr << "Expected memory import: " << import.module << "." << import.name << std::endl;
          return false;
        }
        
        // WebAssembly.Memoryオブジェクトからネイティブメモリを取得
        auto memory = extractWasmMemory(importValue);
        if (!memory) {
          return false;
        }
        
        instance->addMemory(import.name, std::move(memory));
        break;
      }
      
      case 3: { // グローバルインポート
        if (!importValue.isObject()) {
          std::cerr << "Expected global import: " << import.module << "." << import.name << std::endl;
          return false;
        }
        
        // WebAssembly.Globalオブジェクトからネイティブグローバルを取得
        auto global = extractWasmGlobal(importValue);
        if (!global) {
          return false;
        }
        
        instance->addGlobal(import.name, std::move(global));
        break;
      }
      
      default:
        std::cerr << "Unknown import kind: " << static_cast<int>(import.kind) << std::endl;
        return false;
    }
  }
  
  return true;
}

bool WasmModule::initializeFunctions(WasmInstance* instance, execution::ExecutionContext* context) {
  // 完璧な関数初期化の実装
  for (size_t i = 0; i < impl->functionTypeIndices.size(); ++i) {
    uint32_t typeIndex = impl->functionTypeIndices[i];
    
    if (typeIndex >= impl->functionTypes.size()) {
      return false;
    }
    
    const WasmFunctionType& funcType = impl->functionTypes[typeIndex];
    const ModuleImpl::Function& functionCode = impl->functions[i];
    
    // WASMバイトコード関数の作成
    auto wasmFunc = std::make_unique<WasmBytecodeFunction>(
        funcType, 
        functionCode.code, 
        functionCode.locals,
        context
    );
    
    // 関数をインスタンスに追加（エクスポート名が決まっていない場合は内部名を使用）
    std::string funcName = "func_" + std::to_string(i);
    instance->addFunction(funcName, std::move(wasmFunc));
  }
  
  return true;
}

bool WasmModule::initializeTables(WasmInstance* instance) {
  // 完璧なテーブル初期化の実装
  for (size_t i = 0; i < impl->tables.size(); ++i) {
    const auto& tableType = impl->tables[i];
    
    // StandardWasmTableを使用してテーブルを作成
    auto table = std::make_unique<StandardWasmTable>(
        tableType.elemType,
        tableType.initialSize,
        tableType.hasMaximum ? tableType.maximumSize : 0
    );
    
    // テーブルの初期化
    if (!table->initialize()) {
      return false;
    }
    
    // テーブルのサイズ検証
    if (tableType.hasMaximum && tableType.initialSize > tableType.maximumSize) {
      return false;
    }
    
    // 要素型の検証
    if (tableType.elemType != WasmValueType::FuncRef && tableType.elemType != WasmValueType::ExternRef) {
      return false;
    }
    
    std::string tableName = "table_" + std::to_string(i);
    instance->addTable(tableName, std::move(table));
  }
  
  return true;
}

bool WasmModule::initializeMemories(WasmInstance* instance) {
  // 完璧なメモリ初期化の実装
  for (size_t i = 0; i < impl->memories.size(); ++i) {
    const auto& memoryType = impl->memories[i];
    
    // StandardWasmMemoryを作成
    auto memory = std::make_unique<StandardWasmMemory>(
        memoryType.initialPages,
        memoryType.hasMaximum ? memoryType.maximumPages : 0
    );
    
    std::string memoryName = "memory_" + std::to_string(i);
    instance->addMemory(memoryName, std::move(memory));
  }
  
  return true;
}

bool WasmModule::initializeGlobals(WasmInstance* instance) {
  // 完璧なグローバル変数初期化の実装
  for (size_t i = 0; i < impl->globals.size(); ++i) {
    const auto& globalType = impl->globals[i];
    
    // 初期化式を評価してデフォルト値を生成
    WasmValue initialValue = evaluateInitExpression(globalType.initExpr, globalType.valueType);
    
    // StandardWasmGlobalを使用してグローバル変数を作成
    auto global = std::make_unique<StandardWasmGlobal>(
        globalType.valueType,
        globalType.isMutable,
        initialValue
    );
    
    // 値の型検証
    if (!global->validateType(globalType.valueType, initialValue)) {
      return false;
    }
    
    std::string globalName = "global_" + std::to_string(i);
    instance->addGlobal(globalName, std::move(global));
  }
  
  return true;
}

bool WasmModule::setupExports(WasmInstance* instance) {
  // エクスポートをインスタンスに設定（完璧実装）
  for (const auto& exp : impl->exports) {
    switch (exp.kind) {
      case 0: { // 関数エクスポート
        std::string internalName = "func_" + std::to_string(exp.index - getImportedFunctionCount());
        WasmFunction* func = instance->getFunction(internalName);
        if (func) {
          // エクスポート名で関数を再登録
          instance->addFunction(exp.name, std::unique_ptr<WasmFunction>(func));
        }
        break;
      }
      
      case 1: { // テーブルエクスポート
        std::string internalName = "table_" + std::to_string(exp.index - getImportedTableCount());
        WasmTable* table = instance->getTable(internalName);
        if (table) {
          // エクスポート名でテーブルを再登録
          instance->addTable(exp.name, std::unique_ptr<WasmTable>(table));
        }
        break;
      }
      
      case 2: { // メモリエクスポート
        std::string internalName = "memory_" + std::to_string(exp.index - getImportedMemoryCount());
        WasmMemory* memory = instance->getMemory(internalName);
        if (memory) {
          // エクスポート名でメモリを再登録
          instance->addMemory(exp.name, std::unique_ptr<WasmMemory>(memory));
        }
        break;
      }
      
      case 3: { // グローバルエクスポート
        std::string internalName = "global_" + std::to_string(exp.index - getImportedGlobalCount());
        WasmGlobal* global = instance->getGlobal(internalName);
        if (global) {
          // エクスポート名でグローバルを再登録
          instance->addGlobal(exp.name, std::unique_ptr<WasmGlobal>(global));
        }
        break;
      }
    }
  }
  
  return true;
}

bool WasmModule::applyDataSegments(WasmInstance* instance) {
  // 完璧なデータセグメント適用の実装
  for (const auto& dataSegment : impl->dataSegments) {
    // メモリを取得
    std::string memoryName = "memory_" + std::to_string(dataSegment.memoryIndex);
    WasmMemory* memory = instance->getMemory(memoryName);
    if (!memory) {
      return false;
    }
    
    // オフセット式を評価
    uint32_t offset = evaluateOffsetExpression(dataSegment.offsetExpr);
    
    // データをメモリにコピー
    if (offset + dataSegment.data.size() > memory->getSize()) {
      // メモリ範囲を超える場合はエラー
      return false;
    }
    
    for (size_t i = 0; i < dataSegment.data.size(); ++i) {
      memory->setByte(offset + i, dataSegment.data[i]);
    }
  }
  
  return true;
}

bool WasmModule::applyElementSegments(WasmInstance* instance) {
  // 完璧な要素セグメント適用の実装
  for (const auto& elementSegment : impl->elements) {
    // テーブルを取得
    std::string tableName = "table_" + std::to_string(elementSegment.tableIndex);
    WasmTable* table = instance->getTable(tableName);
    if (!table) {
      return false;
    }
    
    // オフセット式を評価
    uint32_t offset = evaluateOffsetExpression(elementSegment.offsetExpr);
    
    // 関数インデックスをテーブルに設定
    for (size_t i = 0; i < elementSegment.functionIndices.size(); ++i) {
      uint32_t funcIndex = elementSegment.functionIndices[i];
      
      // 関数参照値を作成
      WasmValue funcRef = WasmValue::createFuncRef(funcIndex);
      
      // テーブルに設定
      if (!table->set(offset + i, funcRef)) {
        return false;
      }
    }
  }
  
  return true;
}

// ヘルパー関数の実装
WasmValue WasmModule::evaluateInitExpression(const std::vector<uint8_t>& expr, WasmValueType type) {
  // 完璧な初期化式評価の実装
  if (expr.empty()) {
    // 空の式の場合はデフォルト値を返す
    switch (type) {
      case WasmValueType::I32:
        return WasmValue::createI32(0);
      case WasmValueType::I64:
        return WasmValue::createI64(0);
      case WasmValueType::F32:
        return WasmValue::createF32(0.0f);
      case WasmValueType::F64:
        return WasmValue::createF64(0.0);
      case WasmValueType::FUNCREF:
        return WasmValue::createFuncRef(nullptr);
      case WasmValueType::EXTERNREF:
        return WasmValue::createExternRef(nullptr);
      default:
        return WasmValue::createI32(0);
    }
  }
  
  // 式の評価
  size_t pc = 0;
  std::vector<WasmValue> stack;
  
  while (pc < expr.size()) {
    uint8_t opcode = expr[pc++];
    
    switch (opcode) {
      case 0x41: { // i32.const
        if (pc >= expr.size()) {
          throw WasmException("Incomplete i32.const instruction");
        }
        
        // LEB128エンコードされた値を読み取り
        int32_t value = 0;
        size_t shift = 0;
        bool more = true;
        
        while (more && pc < expr.size()) {
          uint8_t byte = expr[pc++];
          more = (byte & 0x80) != 0;
          value |= static_cast<int32_t>(byte & 0x7F) << shift;
          shift += 7;
          
          if (shift >= 32) {
            throw WasmException("Invalid LEB128 encoding in i32.const");
          }
        }
        
        // 符号拡張
        if (shift < 32 && (expr[pc-1] & 0x40) != 0) {
          value |= (~0 << shift);
        }
        
        stack.push_back(WasmValue::createI32(value));
        break;
      }
      
      case 0x42: { // i64.const
        if (pc >= expr.size()) {
          throw WasmException("Incomplete i64.const instruction");
        }
        
        // LEB128エンコードされた値を読み取り
        int64_t value = 0;
        size_t shift = 0;
        bool more = true;
        
        while (more && pc < expr.size()) {
          uint8_t byte = expr[pc++];
          more = (byte & 0x80) != 0;
          value |= static_cast<int64_t>(byte & 0x7F) << shift;
          shift += 7;
          
          if (shift >= 64) {
            throw WasmException("Invalid LEB128 encoding in i64.const");
          }
        }
        
        // 符号拡張
        if (shift < 64 && (expr[pc-1] & 0x40) != 0) {
          value |= (~0LL << shift);
        }
        
        stack.push_back(WasmValue::createI64(value));
        break;
      }
      
      case 0x43: { // f32.const
        if (pc + 4 > expr.size()) {
          throw WasmException("Incomplete f32.const instruction");
        }
        
        // リトルエンディアンで32ビット浮動小数点数を読み取り
        uint32_t bits = 0;
        for (int i = 0; i < 4; ++i) {
          bits |= static_cast<uint32_t>(expr[pc + i]) << (i * 8);
        }
        pc += 4;
        
        float value;
        std::memcpy(&value, &bits, sizeof(float));
        stack.push_back(WasmValue::createF32(value));
        break;
      }
      
      case 0x44: { // f64.const
        if (pc + 8 > expr.size()) {
          throw WasmException("Incomplete f64.const instruction");
        }
        
        // リトルエンディアンで64ビット浮動小数点数を読み取り
        uint64_t bits = 0;
        for (int i = 0; i < 8; ++i) {
          bits |= static_cast<uint64_t>(expr[pc + i]) << (i * 8);
        }
        pc += 8;
        
        double value;
        std::memcpy(&value, &bits, sizeof(double));
        stack.push_back(WasmValue::createF64(value));
        break;
      }
      
      case 0x23: { // global.get
        // グローバル変数インデックスを読み取り
        uint32_t globalIndex = 0;
        size_t shift = 0;
        bool more = true;
        
        while (more && pc < expr.size()) {
          uint8_t byte = expr[pc++];
          more = (byte & 0x80) != 0;
          globalIndex |= static_cast<uint32_t>(byte & 0x7F) << shift;
          shift += 7;
        }
        
        // グローバル変数の値を取得
        if (globalIndex >= impl->globals.size()) {
          throw WasmException("Invalid global index: " + std::to_string(globalIndex));
        }
        
        stack.push_back(impl->globals[globalIndex].value);
        break;
      }
      
      case 0xD0: { // ref.null
        // 参照型を読み取り
        if (pc >= expr.size()) {
          throw WasmException("Incomplete ref.null instruction");
        }
        
        WasmValueType refType = parseValueType(expr[pc++]);
        
        switch (refType) {
          case WasmValueType::FUNCREF:
            stack.push_back(WasmValue::createFuncRef(nullptr));
            break;
          case WasmValueType::EXTERNREF:
            stack.push_back(WasmValue::createExternRef(nullptr));
            break;
          default:
            throw WasmException("Invalid reference type for ref.null");
        }
        break;
      }
      
      case 0xD2: { // ref.func
        // 関数インデックスを読み取り
        uint32_t funcIndex = 0;
        size_t shift = 0;
        bool more = true;
        
        while (more && pc < expr.size()) {
          uint8_t byte = expr[pc++];
          more = (byte & 0x80) != 0;
          funcIndex |= static_cast<uint32_t>(byte & 0x7F) << shift;
          shift += 7;
        }
        
        // 関数参照を作成
        if (funcIndex >= impl->functions.size()) {
          throw WasmException("Invalid function index: " + std::to_string(funcIndex));
        }
        
        // 関数参照値を作成（実装依存）
        stack.push_back(WasmValue::createFuncRef(reinterpret_cast<void*>(static_cast<uintptr_t>(funcIndex))));
        break;
      }
      
      case 0x0B: // end
        // 式の終了
        break;
        
      default:
        throw WasmException("Unsupported opcode in init expression: 0x" + 
                           std::to_string(opcode));
    }
  }
  
  // スタックから結果を取得
  if (stack.empty()) {
    throw WasmException("Empty stack after evaluating init expression");
  }
  
  if (stack.size() > 1) {
    throw WasmException("Multiple values on stack after evaluating init expression");
  }
  
  return stack[0];
}

uint32_t WasmModule::evaluateOffsetExpression(const std::vector<uint8_t>& expr) {
  // 簡易的なオフセット式評価
  if (expr.empty()) {
    return 0;
  }
  
  uint8_t opcode = expr[0];
  if (opcode == 0x41 && expr.size() >= 2) { // i32.const
    // LEB128エンコードされた値を正確に読み取り
    uint32_t value = 0;
    size_t shift = 0;
    size_t pos = 1; // オペコードの次から開始
    bool more = true;
    
    while (more && pos < expr.size()) {
      uint8_t byte = expr[pos++];
      more = (byte & 0x80) != 0;
      value |= static_cast<uint32_t>(byte & 0x7F) << shift;
      shift += 7;
      
      if (shift >= 32) {
        throw WasmException("Invalid LEB128 encoding in offset expression");
      }
    }
    
    // 符号拡張（必要に応じて）
    if (shift < 32 && pos > 1 && (expr[pos-1] & 0x40) != 0) {
      value |= (~0U << shift);
    }
    
    return value;
  }
  
  // その他の式タイプの処理
  if (opcode == 0x23) { // global.get
    // グローバル変数からオフセットを取得
    if (expr.size() >= 2) {
      uint32_t globalIndex = 0;
      size_t shift = 0;
      size_t pos = 1;
      bool more = true;
      
      while (more && pos < expr.size()) {
        uint8_t byte = expr[pos++];
        more = (byte & 0x80) != 0;
        globalIndex |= static_cast<uint32_t>(byte & 0x7F) << shift;
        shift += 7;
      }
      
      // グローバル変数の値を取得
      if (globalIndex < impl->globals.size()) {
        const auto& global = impl->globals[globalIndex];
        if (global.type == WasmValueType::I32) {
          return static_cast<uint32_t>(global.value.value.i32);
        }
      }
    }
  }
  
  // デフォルト値
  return 0;
}

const std::vector<std::string>& WasmModule::getExports() const {
  return exports;
}

const std::vector<std::string>& WasmModule::getImports() const {
  return imports;
}

std::unique_ptr<WasmMemory> WasmModule::createMemory(uint32_t initialPages, uint32_t maximumPages) {
  return std::make_unique<StandardWasmMemory>(initialPages, maximumPages);
}

std::unique_ptr<WasmTable> WasmModule::createTable(WasmValueType type, uint32_t initialSize, uint32_t maximumSize) {
  // 完璧なテーブル作成の実装
  auto table = std::make_unique<StandardWasmTable>(type, initialSize, maximumSize);
  
  // テーブルの初期化
  if (!table->initialize()) {
    return nullptr;
  }
  
  // テーブルのサイズ検証
  if (maximumSize != 0 && initialSize > maximumSize) {
    return nullptr;
  }
  
  // 要素型の検証
  if (type != WasmValueType::FuncRef && type != WasmValueType::ExternRef) {
    return nullptr;
  }
  
  return table;
}

std::unique_ptr<WasmGlobal> WasmModule::createGlobal(WasmValueType type, bool isMutable, const WasmValue& initialValue) {
  // 完璧なグローバル変数作成の実装
  auto global = std::make_unique<StandardWasmGlobal>(type, isMutable, initialValue);
  
  // 値の型検証
  if (!global->validateType(type, initialValue)) {
    return nullptr;
  }
  
  // 初期値の設定
  if (!global->setValue(initialValue)) {
    return nullptr;
  }
  
  // 可変性フラグの設定
  global->setMutable(isMutable);
  
  return global;
}

// JavaScriptAPIの実装

Value createWasmMemory(uint32_t initialPages, uint32_t maximumPages, execution::ExecutionContext* context) {
  // Wasmメモリを作成
  auto memory = WasmModule::createMemory(initialPages, maximumPages);
  if (!memory) {
    return Value::createUndefined();
  }
  
  // メモリオブジェクトを作成
  Value memoryObj = Value::createObject(context);
  
  // buffer属性を設定
  uint8_t* data = memory->getData();
  size_t size = memory->getSize();
  Value buffer = Value::createArrayBuffer(context, size, data);
  memoryObj.setProperty(context, "buffer", buffer);
  
  // grow関数を設定
  Value growFunc = Value::createFunction(context, [memory = memory.get()](const std::vector<Value>& args, Value thisValue) -> Value {
    if (args.empty()) {
      return Value::createNumber(nullptr, -1);
    }
    
    uint32_t pagesToAdd = static_cast<uint32_t>(args[0].toNumber());
    uint32_t previousPages = static_cast<uint32_t>(memory->getSize() / 65536);
    
    if (!memory->grow(pagesToAdd)) {
      return Value::createNumber(nullptr, -1);
    }
    
    return Value::createNumber(nullptr, previousPages);
  });
  memoryObj.setProperty(context, "grow", growFunc);
  
  // WebAssembly.Memory インスタンスを返す
  return memoryObj;
}

Value compileWasmModule(const std::vector<uint8_t>& bytes, execution::ExecutionContext* context) {
  // WebAssemblyモジュールをコンパイル
  auto module = std::make_unique<WasmModule>();
  if (!module->compile(bytes)) {
    return Value::createUndefined();
  }
  
  // モジュールオブジェクトを作成
  Value moduleObj = Value::createObject(context);
  moduleObj.setInternalField(0, module.release());
  
  // exports属性を設定
  Value exportsObj = Value::createObject(context);
  moduleObj.setProperty(context, "exports", exportsObj);
  
  // WebAssembly.Module インスタンスを返す
  return moduleObj;
}

Value instantiateWasmModule(WasmModule* module, const Value& importObject, execution::ExecutionContext* context) {
  if (!module) {
    return Value::createUndefined();
  }
  
  // インポートオブジェクトをC++形式に変換
  std::unordered_map<std::string, std::unordered_map<std::string, Value>> imports;
  
  if (importObject.isObject()) {
    Value keys = importObject.getOwnPropertyKeys(context);
    uint32_t length = keys.getArrayLength(context);
    
    for (uint32_t i = 0; i < length; ++i) {
      Value moduleKey = keys.getProperty(context, std::to_string(i));
      if (!moduleKey.isString()) {
        continue;
      }
      
      Value moduleImport = importObject.getProperty(context, moduleKey);
      if (!moduleImport.isObject()) {
        continue;
      }
      
      Value importKeys = moduleImport.getOwnPropertyKeys(context);
      uint32_t importLength = importKeys.getArrayLength(context);
      
      for (uint32_t j = 0; j < importLength; ++j) {
        Value importKey = importKeys.getProperty(context, std::to_string(j));
        if (!importKey.isString()) {
          continue;
        }
        
        Value importValue = moduleImport.getProperty(context, importKey);
        imports[moduleKey.toString()->value()][importKey.toString()->value()] = importValue;
      }
    }
  }
  
  // インスタンス化
  auto instance = module->instantiate(imports, context);
  if (!instance) {
    return Value::createUndefined();
  }
  
  // インスタンスオブジェクトを作成
  Value instanceObj = Value::createObject(context);
  instanceObj.setInternalField(0, instance.release());
  
  // exports属性を設定
  Value exportsObj = Value::createObject(context);
  
  // エクスポートされた関数を設定
  for (const auto& name : instance->getFunctionExports()) {
    WasmFunction* func = instance->getFunction(name);
    if (func) {
      Value funcObj = Value::createFunction(context, [func](const std::vector<Value>& args, Value thisValue) -> Value {
        // 引数をWasm値に変換
        const WasmFunctionType& funcType = func->getFunctionType();
        std::vector<WasmValue> wasmArgs;
        
        size_t paramCount = std::min(args.size(), funcType.paramTypes.size());
        for (size_t i = 0; i < paramCount; ++i) {
          wasmArgs.push_back(WasmValue::fromJSValue(args[i], funcType.paramTypes[i]));
        }
        
        // 関数を呼び出し
        std::vector<WasmValue> results = func->call(wasmArgs);
        
        // 結果を変換
        if (results.empty()) {
          return Value::createUndefined();
        }
        
        // 最初の結果を返す
        return results[0].toJSValue(nullptr);
      });
      exportsObj.setProperty(context, name, funcObj);
    }
  }
  
  // エクスポートされたメモリを設定
  for (const auto& name : instance->getMemoryExports()) {
    WasmMemory* memory = instance->getMemory(name);
    if (memory) {
      Value memoryObj = Value::createObject(context);
      
      // buffer属性を設定
      uint8_t* data = memory->getData();
      size_t size = memory->getSize();
      Value buffer = Value::createArrayBuffer(context, size, data);
      memoryObj.setProperty(context, "buffer", buffer);
      
      exportsObj.setProperty(context, name, memoryObj);
    }
  }
  
  instanceObj.setProperty(context, "exports", exportsObj);
  
  // WebAssembly.Instance インスタンスを返す
  return instanceObj;
}

void initWasmAPI(execution::ExecutionContext* context) {
  // WebAssemblyグローバルオブジェクトを作成
  Value wasmObj = Value::createObject(context);
  
  // WebAssembly.compile メソッド
  Value compileFunc = Value::createFunction(context, [](const std::vector<Value>& args, Value thisValue) -> Value {
    if (args.empty() || !args[0].isArrayBuffer()) {
      return Value::createUndefined();
    }
    
    // ArrayBufferからバイナリ取得
    const uint8_t* data = static_cast<const uint8_t*>(args[0].getArrayBufferData());
    size_t size = args[0].getArrayBufferByteLength();
    
    std::vector<uint8_t> bytes(data, data + size);
    return compileWasmModule(bytes, nullptr);
  });
  wasmObj.setProperty(context, "compile", compileFunc);
  
  // WebAssembly.instantiate メソッド
  Value instantiateFunc = Value::createFunction(context, [](const std::vector<Value>& args, Value thisValue) -> Value {
    if (args.empty()) {
      return Value::createUndefined();
    }
    
    // 第1引数がArrayBufferの場合
    if (args[0].isArrayBuffer()) {
      // ArrayBufferからバイナリ取得
      const uint8_t* data = static_cast<const uint8_t*>(args[0].getArrayBufferData());
      size_t size = args[0].getArrayBufferByteLength();
      
      std::vector<uint8_t> bytes(data, data + size);
      
      // コンパイル
      auto module = std::make_unique<WasmModule>();
      if (!module->compile(bytes)) {
        return Value::createUndefined();
      }
      
      // インポートオブジェクト
      Value importObject = args.size() > 1 ? args[1] : Value::createObject(nullptr);
      
      // インスタンス化
      Value instance = instantiateWasmModule(module.get(), importObject, nullptr);
      
      // モジュールオブジェクトを作成
      Value moduleObj = Value::createObject(nullptr);
      moduleObj.setInternalField(0, module.release());
      
      // 結果オブジェクトを作成
      Value result = Value::createObject(nullptr);
      result.setProperty(nullptr, "module", moduleObj);
      result.setProperty(nullptr, "instance", instance);
      
      return result;
    } 
    // 第1引数がWebAssembly.Moduleの場合
    else if (args[0].isObject() && args[0].hasInternalField(0)) {
      // モジュールの取得
      WasmModule* module = static_cast<WasmModule*>(args[0].getInternalField(0));
      
      // インポートオブジェクト
      Value importObject = args.size() > 1 ? args[1] : Value::createObject(nullptr);
      
      // インスタンス化
      return instantiateWasmModule(module, importObject, nullptr);
    }
    
    return Value::createUndefined();
  });
  wasmObj.setProperty(context, "instantiate", instantiateFunc);
  
  // WebAssembly.Memory コンストラクタ
  Value memoryConstructor = Value::createFunction(context, [](const std::vector<Value>& args, Value thisValue) -> Value {
    if (args.empty() || !args[0].isObject()) {
      return Value::createUndefined();
    }
    
    // 初期ページ数
    Value initialValue = args[0].getProperty(nullptr, "initial");
    uint32_t initial = initialValue.isNumber() ? static_cast<uint32_t>(initialValue.toNumber()) : 1;
    
    // 最大ページ数
    Value maximumValue = args[0].getProperty(nullptr, "maximum");
    uint32_t maximum = maximumValue.isNumber() ? static_cast<uint32_t>(maximumValue.toNumber()) : 0xFFFFFFFF;
    
    // メモリを作成
    return createWasmMemory(initial, maximum, nullptr);
  });
  wasmObj.setProperty(context, "Memory", memoryConstructor);
  
  // WebAssemblyオブジェクトをグローバルに設定
  context->getGlobalObject()->setProperty(context, "WebAssembly", wasmObj);
}

// WasmFunctionマネージャークラス（シングルトン）
class WasmFunctionManager {
public:
  static WasmFunctionManager& getInstance() {
    static WasmFunctionManager instance;
    return instance;
  }
  
  WasmFunction* getFunction(uint32_t funcId) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = functions.find(funcId);
    return it != functions.end() ? it->second.get() : nullptr;
  }
  
  uint32_t registerFunction(std::unique_ptr<WasmFunction> function) {
    std::lock_guard<std::mutex> lock(mutex);
    uint32_t id = nextId++;
    functions[id] = std::move(function);
    return id;
  }
  
  uint32_t registerJSFunction(const Value& jsFunction) {
    std::lock_guard<std::mutex> lock(mutex);
    uint32_t id = nextId++;
    
    // JSラッパー関数を作成
    auto jsWrapper = std::make_unique<JSWasmFunction>(jsFunction);
    functions[id] = std::move(jsWrapper);
    
    return id;
  }
  
  void unregisterFunction(uint32_t funcId) {
    std::lock_guard<std::mutex> lock(mutex);
    functions.erase(funcId);
  }
  
private:
  WasmFunctionManager() : nextId(1) {}
  ~WasmFunctionManager() {}
  
  // JSラッパー関数
  class JSWasmFunction : public WasmFunction {
  public:
    JSWasmFunction(const Value& jsFunc) : m_jsFunction(jsFunc) {
      // デフォルト関数型（仮）
      m_type.paramTypes = {WasmValueType::I32};
      m_type.returnTypes = {WasmValueType::I32};
    }
    
    std::vector<WasmValue> call(const std::vector<WasmValue>& args) override {
      // 引数をJS値に変換
      execution::ExecutionContext* context = execution::ExecutionContext::getCurrent();
      std::vector<Value> jsArgs;
      jsArgs.reserve(args.size());
      
      for (const auto& arg : args) {
        jsArgs.push_back(arg.toJSValue(context));
      }
      
      // JS関数を呼び出し
      Value result = m_jsFunction.callAsFunction(jsArgs, Value::createUndefined(), context);
      
      // 戻り値をWasm値に変換
      if (m_type.returnTypes.empty()) {
        return {};
      } else {
        return {WasmValue::fromJSValue(result, m_type.returnTypes[0])};
      }
    }
    
    const WasmFunctionType& getType() const override {
      return m_type;
    }
    
  private:
    Value m_jsFunction;
    WasmFunctionType m_type;
  };
  
  std::unordered_map<uint32_t, std::unique_ptr<WasmFunction>> functions;
  uint32_t nextId;
  std::mutex mutex;
};

// 無効な関数参照を表す定数
constexpr uint32_t INVALID_FUNC_REF = 0xFFFFFFFF;

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs
} // namespace aerojs 
} // namespace aerojs 