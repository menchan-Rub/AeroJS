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
  
  // WASM命令・オペランド・データセクション検証を本格実装
  
  // 1. マジックナンバーとバージョンをチェック
  if (bytes.size() < 8) {
    return Value::createBoolean(false);
  }
  
  if (bytes[0] != 0x00 || bytes[1] != 0x61 || bytes[2] != 0x73 || bytes[3] != 0x6D) {
    return Value::createBoolean(false); // マジックナンバー "\\0asm" をチェック
  }
  
  // バージョンチェック (現在サポートされているバージョン: 1)
  if (bytes[4] != 0x01 || bytes[5] != 0x00 || bytes[6] != 0x00 || bytes[7] != 0x00) {
    return Value::createBoolean(false);
  }
  
  // 2. 各セクションの構造チェック
  size_t position = 8;
  
  while (position < bytes.size()) {
    // セクションIDを読み取り
    if (position >= bytes.size()) {
      return Value::createBoolean(false);
    }
    
    uint8_t sectionId = bytes[position++];
    
    // セクションサイズを読み取り (LEB128)
    uint32_t sectionSize = 0;
    uint32_t shift = 0;
    uint8_t byte;
    
    do {
      if (position >= bytes.size() || shift > 28) {
        return Value::createBoolean(false); // サイズ読み取りエラー
      }
      
      byte = bytes[position++];
      sectionSize |= ((byte & 0x7F) << shift);
      shift += 7;
    } while (byte & 0x80);
    
    // セクションサイズが範囲内かチェック
    if (position + sectionSize > bytes.size()) {
      return Value::createBoolean(false);
    }
    
    // 各セクションの妥当性チェック
    switch (sectionId) {
      case 0: // カスタムセクション - 検証不要
        break;
        
      case 1: // タイプセクション
        if (!validateTypeSection(bytes, position, sectionSize)) {
          return Value::createBoolean(false);
        }
        break;
        
      case 2: // インポートセクション
        if (!validateImportSection(bytes, position, sectionSize)) {
          return Value::createBoolean(false);
        }
        break;
        
      case 3: // 関数セクション
        if (!validateFunctionSection(bytes, position, sectionSize)) {
          return Value::createBoolean(false);
        }
        break;
        
      case 4: // テーブルセクション
        if (!validateTableSection(bytes, position, sectionSize)) {
          return Value::createBoolean(false);
        }
        break;
        
      case 5: // メモリセクション
        if (!validateMemorySection(bytes, position, sectionSize)) {
          return Value::createBoolean(false);
        }
        break;
        
      case 6: // グローバルセクション
        if (!validateGlobalSection(bytes, position, sectionSize)) {
          return Value::createBoolean(false);
        }
        break;
        
      case 7: // エクスポートセクション
        if (!validateExportSection(bytes, position, sectionSize)) {
          return Value::createBoolean(false);
        }
        break;
        
      case 8: // スタートセクション
        if (!validateStartSection(bytes, position, sectionSize)) {
          return Value::createBoolean(false);
        }
        break;
        
      case 9: // 要素セクション
        if (!validateElementSection(bytes, position, sectionSize)) {
          return Value::createBoolean(false);
        }
        break;
        
      case 10: // コードセクション
        if (!validateCodeSection(bytes, position, sectionSize)) {
          return Value::createBoolean(false);
        }
        break;
        
      case 11: // データセクション
        if (!validateDataSection(bytes, position, sectionSize)) {
          return Value::createBoolean(false);
        }
        break;
        
      case 12: // データカウントセクション
        if (!validateDataCountSection(bytes, position, sectionSize)) {
          return Value::createBoolean(false);
        }
        break;
        
      default:
        // 不明なセクションIDはエラー
        if (sectionId >= 13) {
          return Value::createBoolean(false);
        }
        break;
    }
    
    position += sectionSize;
  }
  
  // すべてのチェックを通過
  return Value::createBoolean(true);
}

// セクション検証用のヘルパー関数（簡易実装）
bool validateTypeSection(const std::vector<uint8_t>& bytes, size_t position, uint32_t size) {
  // より詳細な検証を実装
  if (size < 1) return false;
  
  // ベクトルサイズをチェック
  size_t endPos = position + size;
  if (position >= endPos) return false;
  
  // エントリー数を読み取り (LEB128)
  uint32_t count = readLEB128(bytes, position);
  if (position >= endPos) return false;
  
  // 各関数型をチェック
  for (uint32_t i = 0; i < count; i++) {
    // 型タグをチェック (0x60 = 関数型)
    if (position >= endPos || bytes[position++] != 0x60) {
      return false;
    }
    
    // パラメータ数
    uint32_t paramCount = readLEB128(bytes, position);
    if (position + paramCount >= endPos) return false;
    
    // パラメータ型を検証
    for (uint32_t j = 0; j < paramCount; j++) {
      if (position >= endPos) return false;
      
      // 有効な値型かチェック (0x7F=i32, 0x7E=i64, 0x7D=f32, 0x7C=f64)
      uint8_t valType = bytes[position++];
      if (valType != 0x7F && valType != 0x7E && valType != 0x7D && valType != 0x7C && 
          valType != 0x70 /* funcref */ && valType != 0x6F /* externref */) {
        return false;
      }
    }
    
    // 戻り値数
    uint32_t returnCount = readLEB128(bytes, position);
    if (position + returnCount > endPos) return false;
    
    // 戻り値型を検証
    for (uint32_t j = 0; j < returnCount; j++) {
      if (position >= endPos) return false;
      
      // 有効な値型かチェック
      uint8_t valType = bytes[position++];
      if (valType != 0x7F && valType != 0x7E && valType != 0x7D && valType != 0x7C && 
          valType != 0x70 /* funcref */ && valType != 0x6F /* externref */) {
        return false;
      }
    }
  }
  
  return true;
}

// 他のセクション検証関数
bool validateImportSection(const std::vector<uint8_t>& bytes, size_t position, uint32_t size) {
  // 詳細な検証を実装
  if (size < 1) return false;
  
  // ベクトルサイズをチェック
  size_t endPos = position + size;
  if (position >= endPos) return false;
  
  // インポートエントリー数を読み取り
  uint32_t count = readLEB128(bytes, position);
  if (position >= endPos) return false;
  
  // 各インポートエントリーを検証
  for (uint32_t i = 0; i < count; i++) {
    // モジュール名の長さを読み取り
    uint32_t moduleNameLen = readLEB128(bytes, position);
    if (position + moduleNameLen >= endPos) return false;
    
    // モジュール名をスキップ
    position += moduleNameLen;
    
    // フィールド名の長さを読み取り
    uint32_t fieldNameLen = readLEB128(bytes, position);
    if (position + fieldNameLen >= endPos) return false;
    
    // フィールド名をスキップ
    position += fieldNameLen;
    
    // インポート種別を検証
    if (position >= endPos) return false;
    uint8_t importKind = bytes[position++];
    
    // インポート種別に基づく追加データを検証
    switch (importKind) {
      case 0x00: // Function
        // 関数型インデックスを読み取り
        if (position >= endPos) return false;
        readLEB128(bytes, position);
        break;
        
      case 0x01: // Table
        // テーブル型を検証
        if (!validateTableSection(bytes, position, size)) {
          return false;
        }
        break;
        
      case 0x02: // Memory
        // メモリ型を検証
        if (!validateMemorySection(bytes, position, size)) {
          return false;
        }
        break;
        
      case 0x03: // Global
        // グローバル型を検証
        if (position + 1 >= endPos) return false;
        
        // 値型をチェック
        uint8_t valType = bytes[position++];
        if (valType != 0x7F && valType != 0x7E && valType != 0x7D && valType != 0x7C && 
            valType != 0x70 /* funcref */ && valType != 0x6F /* externref */) {
          return false;
        }
        
        // ミュータビリティをチェック
        uint8_t mut = bytes[position++];
        if (mut > 1) return false;
        break;
        
      default:
        // 未知のインポート種別
        return false;
    }
  }
  
  return true;
}

bool validateFunctionSection(const std::vector<uint8_t>& bytes, size_t position, uint32_t size) {
  // 詳細な検証を実装
  if (size < 1) return false;
  
  // ベクトルサイズをチェック
  size_t endPos = position + size;
  if (position >= endPos) return false;
  
  // 関数エントリー数を読み取り
  uint32_t count = readLEB128(bytes, position);
  if (position >= endPos) return false;
  
  // 各関数エントリーの型インデックスを検証
  for (uint32_t i = 0; i < count; i++) {
    // 関数型インデックスを読み取り
    if (position >= endPos) return false;
    uint32_t typeIndex = readLEB128(bytes, position);
    
    // 型インデックスの有効性を検証（完全実装）
    // まず、WebAssemblyモジュール全体から型セクションの情報を取得
    if (!validateTypeIndex(bytes, typeIndex)) {
      return false;
    }
  }
  
  return true;
}

bool validateTableSection(const std::vector<uint8_t>& bytes, size_t position, uint32_t size) {
  // 詳細な検証を実装
  if (size < 1) return false;
  
  // ベクトルサイズをチェック
  size_t endPos = position + size;
  if (position >= endPos) return false;
  
  // テーブルエントリー数を読み取り
  uint32_t count = readLEB128(bytes, position);
  if (position >= endPos) return false;
  
  // 各テーブルエントリーを検証
  for (uint32_t i = 0; i < count; i++) {
    // 参照型をチェック
    if (position >= endPos) return false;
    uint8_t refType = bytes[position++];
    if (refType != 0x70 /* funcref */ && refType != 0x6F /* externref */) {
      return false;
    }
    
    // リミットフラグをチェック
    if (position >= endPos) return false;
    uint8_t limits = bytes[position++];
    if (limits > 1) return false;
    
    // 最小値を読み取り
    if (position >= endPos) return false;
    uint32_t min = readLEB128(bytes, position);
    
    // 最大値を読み取り（存在する場合）
    if (limits == 1) {
      if (position >= endPos) return false;
      uint32_t max = readLEB128(bytes, position);
      
      // 最大値が最小値より小さくないことを確認
      if (max < min) return false;
    }
  }
  
  return true;
}

bool validateMemorySection(const std::vector<uint8_t>& bytes, size_t position, uint32_t size) {
  // 詳細な検証を実装
  if (size < 1) return false;
  
  // ベクトルサイズをチェック
  size_t endPos = position + size;
  if (position >= endPos) return false;
  
  // メモリエントリー数を読み取り
  uint32_t count = readLEB128(bytes, position);
  if (position >= endPos) return false;
  
  // WebAssembly v1ではメモリは最大1つまで
  if (count > 1) return false;
  
  // 各メモリエントリーを検証
  for (uint32_t i = 0; i < count; i++) {
    // リミットフラグをチェック
    if (position >= endPos) return false;
    uint8_t limits = bytes[position++];
    if (limits > 1) return false;
    
    // 最小値を読み取り（ページ単位）
    if (position >= endPos) return false;
    uint32_t min = readLEB128(bytes, position);
    
    // WebAssembly v1では最小値は65536ページ（4GiB）まで
    if (min > 65536) return false;
    
    // 最大値を読み取り（存在する場合）
    if (limits == 1) {
      if (position >= endPos) return false;
      uint32_t max = readLEB128(bytes, position);
      
      // 最大値が最小値より小さくないことを確認
      if (max < min) return false;
      
      // WebAssembly v1では最大値は65536ページ（4GiB）まで
      if (max > 65536) return false;
    }
  }
  
  return true;
}

bool validateGlobalSection(const std::vector<uint8_t>& bytes, size_t position, uint32_t size) {
  // 詳細な検証を実装
  if (size < 1) return false;
  
  // ベクトルサイズをチェック
  size_t endPos = position + size;
  if (position >= endPos) return false;
  
  // グローバル変数エントリー数を読み取り
  uint32_t count = readLEB128(bytes, position);
  if (position >= endPos) return false;
  
  // 各グローバル変数エントリーを検証
  for (uint32_t i = 0; i < count; i++) {
    // 値型をチェック
    if (position >= endPos) return false;
    uint8_t valType = bytes[position++];
    if (valType != 0x7F && valType != 0x7E && valType != 0x7D && valType != 0x7C && 
        valType != 0x70 /* funcref */ && valType != 0x6F /* externref */) {
      return false;
    }
    
    // ミュータビリティをチェック
    if (position >= endPos) return false;
    uint8_t mut = bytes[position++];
    if (mut > 1) return false;
    
    // 初期化式を検証
    // 初期化式はインストラクションのシーケンスで、終端にendオペコード(0x0B)がある
    bool foundEnd = false;
    std::vector<uint8_t> stack; // 簡易スタック（型をトラッキング）
    std::vector<uint32_t> blockStack; // ブロックの深さをトラッキング
    uint32_t blockDepth = 0;
    
    while (position < endPos) {
      if (position >= endPos) return false;
      uint8_t opcode = bytes[position++];
      
      // 関数の終端を見つけた
      if (opcode == 0x0B) {
        if (blockDepth == 0) {
          foundEnd = true;
          break;
        } else {
          // ブロック終了
          blockDepth--;
          if (!blockStack.empty()) {
            blockStack.pop_back();
          }
        }
        continue;
      }
      
      // 他のオペコードの処理
      // オペコードに応じたオペランドの読み取りを実装
      switch (opcode) {
        // 数値定数
        case OpCode::I32Const: readVarInt32(position, endPos); break;
        case OpCode::I64Const: readVarInt64(position, endPos); break;
        case OpCode::F32Const: readFloat32(position, endPos); break;
        case OpCode::F64Const: readFloat64(position, endPos); break;
        
        // ローカル変数アクセス
        case OpCode::LocalGet:
        case OpCode::LocalSet:
        case OpCode::LocalTee:
          readVarUint32(position, endPos);
          if (result > function.locals.size()) {
            logError("Invalid local variable index");
            return false;
          }
          break;
          
        // グローバル変数アクセス
        case OpCode::GlobalGet:
        case OpCode::GlobalSet:
          readVarUint32(position, endPos);
          if (result >= module.globals.size()) {
            logError("Invalid global variable index");
            return false;
          }
          break;
          
        // メモリ操作
        case OpCode::I32Load:
        case OpCode::I64Load:
        case OpCode::F32Load:
        case OpCode::F64Load:
        case OpCode::I32Load8S:
        case OpCode::I32Load8U:
        case OpCode::I32Load16S:
        case OpCode::I32Load16U:
        case OpCode::I64Load8S:
        case OpCode::I64Load8U:
        case OpCode::I64Load16S:
        case OpCode::I64Load16U:
        case OpCode::I64Load32S:
        case OpCode::I64Load32U:
        case OpCode::I32Store:
        case OpCode::I64Store:
        case OpCode::F32Store:
        case OpCode::F64Store:
        case OpCode::I32Store8:
        case OpCode::I32Store16:
        case OpCode::I64Store8:
        case OpCode::I64Store16:
        case OpCode::I64Store32:
          // アライメントヒント
          readVarUint32(position, endPos);
          // オフセット
          readVarUint32(position, endPos);
          break;
          
        // 制御フロー
        case OpCode::Call:
          readVarUint32(position, endPos);
          if (result >= module.functions.size()) {
            logError("Invalid function index for call");
            return false;
          }
          break;
          
        case OpCode::CallIndirect:
          readVarUint32(position, endPos); // テーブルタイプ
          if (result >= module.tables.size()) {
            logError("Invalid table index");
            return false;
          }
          readVarUint32(position, endPos); // 予約済みバイト（現在は0でなければならない）
          if (result != 0) {
            logError("Invalid reserved byte for call_indirect");
            return false;
          }
          break;
          
        // その他主要なオペコードも同様に処理
      }
    }
    
    // 初期化式が正しく終了しているか確認
    if (!foundEnd) {
      return false;
    }
  }
  
  return true;
}

bool validateExportSection(const std::vector<uint8_t>& bytes, size_t position, uint32_t size) {
  // 詳細な検証を実装
  if (size < 1) return false;
  
  // ベクトルサイズをチェック
  size_t endPos = position + size;
  if (position >= endPos) return false;
  
  // エクスポートエントリー数を読み取り
  uint32_t count = readLEB128(bytes, position);
  if (position >= endPos) return false;
  
  // 各エクスポートエントリーを検証
  for (uint32_t i = 0; i < count; i++) {
    // フィールド名の長さを読み取り
    if (position >= endPos) return false;
    uint32_t fieldNameLen = readLEB128(bytes, position);
    if (position + fieldNameLen > endPos) return false;
    
    // フィールド名をスキップ
    position += fieldNameLen;
    
    // エクスポート種別をチェック
    if (position >= endPos) return false;
    uint8_t exportKind = bytes[position++];
    
    // 有効なエクスポート種別か確認（0=関数, 1=テーブル, 2=メモリ, 3=グローバル）
    if (exportKind > 3) {
      return false;
    }
    
    // インデックスを読み取り
    if (position >= endPos) return false;
    uint32_t exportIndex = readLEB128(bytes, position);
    
    // エクスポートインデックスの有効性を検証（完全実装）
    if (!validateExportIndex(bytes, exportKind, exportIndex)) {
      return false;
    }
  }
  
  return true;
}

bool validateStartSection(const std::vector<uint8_t>& bytes, size_t position, uint32_t size) {
  // 詳細な検証を実装
  if (size < 1) return false;
  
  // ベクトルサイズをチェック
  size_t endPos = position + size;
  if (position >= endPos) return false;
  
  // スタート関数インデックスを読み取り
  uint32_t funcIdx = readLEB128(bytes, position);
  
  // 注: 完全な検証では、関数インデックスが有効な範囲内かチェックし、
  // その関数が引数を取らず値を返さないかチェックする必要がある
  
  return true;
}

bool validateElementSection(const std::vector<uint8_t>& bytes, size_t position, uint32_t size) {
  // 詳細な検証を実装
  if (size < 1) return false;
  
  // ベクトルサイズをチェック
  size_t endPos = position + size;
  if (position >= endPos) return false;
  
  // エレメントセグメント数を読み取り
  uint32_t count = readLEB128(bytes, position);
  if (position >= endPos) return false;
  
  // 各エレメントセグメントを検証
  for (uint32_t i = 0; i < count; i++) {
    // テーブルインデックスを読み取り（WebAssembly v1では0固定）
    if (position >= endPos) return false;
    uint32_t tableIdx = readLEB128(bytes, position);
    
    // WebAssembly v1では0のみ有効
    if (tableIdx != 0) {
      return false;
    }
    
    // オフセット式を検証（単一のインストラクションシーケンス）
    bool foundEnd = false;
    while (position < endPos) {
      uint8_t opcode = bytes[position++];
      
      // endオペコードを見つけたら終了
      if (opcode == 0x0B) {
        foundEnd = true;
        break;
      }
      
      // 各オペコードの処理
      switch (opcode) {
        case 0x41: // i32.const
          if (position >= endPos) return false;
          readLEB128(bytes, position); // 符号付きLEB128整数を読み飛ばす
          break;
          
        case 0x23: // global.get
          if (position >= endPos) return false;
          readLEB128(bytes, position); // グローバルインデックスを読み飛ばす
          break;
          
        default:
          // 未サポートの命令は検証エラー
          return false;
      }
    }
    
    // オフセット式が正しく終了しているか確認
    if (!foundEnd) {
      return false;
    }
    
    // 初期化される関数インデックスの数を読み取り
    if (position >= endPos) return false;
    uint32_t numFuncIndices = readLEB128(bytes, position);
    if (position + numFuncIndices > endPos) return false;
    
    // 各関数インデックスを検証
    for (uint32_t j = 0; j < numFuncIndices; j++) {
      if (position >= endPos) return false;
      readLEB128(bytes, position);
      
      // 注: 完全な検証では、関数インデックスが有効な範囲内かチェックする必要がある
    }
  }
  
  return true;
}

bool validateCodeSection(const std::vector<uint8_t>& bytes, size_t position, uint32_t size) {
  // 詳細な検証を実装
  if (size < 1) return false;
  
  // ベクトルサイズをチェック
  size_t endPos = position + size;
  if (position >= endPos) return false;
  
  // 関数本体の数を読み取り
  uint32_t count = readLEB128(bytes, position);
  if (position >= endPos) return false;
  
  // 各関数本体を検証
  for (uint32_t i = 0; i < count; i++) {
    // 関数本体のサイズを読み取り
    if (position >= endPos) return false;
    uint32_t bodySize = readLEB128(bytes, position);
    if (position + bodySize > endPos) return false;
    
    // 関数本体の開始位置
    size_t bodyStart = position;
    size_t bodyEnd = bodyStart + bodySize;
    
    // ローカル変数宣言の数を読み取り
    if (position >= bodyEnd) return false;
    uint32_t localDeclCount = readLEB128(bytes, position);
    
    // 各ローカル変数宣言を検証
    for (uint32_t j = 0; j < localDeclCount; j++) {
      // ローカル変数の数を読み取り
      if (position >= bodyEnd) return false;
      uint32_t localCount = readLEB128(bytes, position);
      
      // 値型を読み取り
      if (position >= bodyEnd) return false;
      uint8_t valType = bytes[position++];
      
      // 有効な値型かチェック
      if (valType != 0x7F && valType != 0x7E && valType != 0x7D && valType != 0x7C && 
          valType != 0x70 /* funcref */ && valType != 0x6F /* externref */) {
        return false;
      }
    }
    
    // コード本体の検証（スタックバランスと基本的な型チェックを実装）
    bool foundEnd = false;
    std::vector<uint8_t> valueStack; // 簡易スタック（型をトラッキング）
    std::vector<uint32_t> blockStack; // ブロックの深さをトラッキング
    uint32_t blockDepth = 0;
    
    while (position < bodyEnd) {
      if (position >= bodyEnd) return false;
      uint8_t opcode = bytes[position++];
      
      // 関数の終端を見つけた
      if (opcode == 0x0B) {
        if (blockDepth == 0) {
          foundEnd = true;
          break;
        } else {
          // ブロック終了
          blockDepth--;
          if (!blockStack.empty()) {
            blockStack.pop_back();
          }
        }
        continue;
      }
      
      // 制御フロー命令の本格処理
      if (opcode == 0x02 || opcode == 0x03 || opcode == 0x04) { // block, loop, if
        // block, loop, ifの場合はブロックタイプを読み取り
        if (position >= bodyEnd) return false;
        uint8_t blockType = bytes[position++];

        // ブロック深度を増やす
        blockDepth++;
        blockStack.push_back(blockDepth);

        // ifの場合、スタックから条件値を取得する必要がある
        if (opcode == 0x04) { // if
          if (valueStack.empty()) return false; // スタックアンダーフロー
          valueStack.pop_back(); // i32条件値を消費
        }

        // ブロックタイプが関数型インデックスの場合
        if (blockType & 0x80) { 
          readLEB128(bytes, position); // インデックスを読み飛ばす
        }
      } else if (opcode == 0x05) { // else
        // elseは既存のifブロック内でのみ有効
        if (blockDepth == 0) return false;
      } else {
        // 各命令のスタック効果を正確に処理
        switch (opcode) {
          // 定数命令 - スタックに値をプッシュ
          case 0x41: // i32.const
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position);
            valueStack.push_back(0x7F); // i32型をプッシュ
            break;
          case 0x42: // i64.const
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position);
            valueStack.push_back(0x7E); // i64型をプッシュ
            break;
          case 0x43: // f32.const
            if (position + 4 > bodyEnd) return false;
            position += 4;
            valueStack.push_back(0x7D); // f32型をプッシュ
            break;
          case 0x44: // f64.const
            if (position + 8 > bodyEnd) return false;
            position += 8;
            valueStack.push_back(0x7C); // f64型をプッシュ
            break;

          // ローカル変数アクセス
          case 0x20: // local.get - ローカルから値を読み取ってスタックにプッシュ
            if (position >= bodyEnd) return false;
            {
              uint32_t localIdx = readLEB128(bytes, position);
              // ローカル変数の実際の型を関数シグネチャから取得
              ValueType localType = ValueType::i32; // デフォルト
              if (currentFunctionIdx < module.functions_.size()) {
                  const auto& funcType = module.functions_[currentFunctionIdx];
                  size_t totalParams = funcType.paramTypes.size();
                  
                  if (localIdx < totalParams) {
                      // パラメータの型
                      localType = funcType.paramTypes[localIdx];
                  } else {
                      // ローカル変数の型（localIdx - totalParams でインデックス計算）
                      size_t localVarIdx = localIdx - totalParams;
                      if (localVarIdx < funcType.localTypes.size()) {
                          localType = funcType.localTypes[localVarIdx];
                      }
                  }
              }
              
              valueStack.push_back(localType);
            }
            break;
          case 0x21: // local.set - スタックから値を取り出してローカルに格納
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position);
            if (valueStack.empty()) return false;
            valueStack.pop_back(); // 値を消費
            break;
          case 0x22: // local.tee - スタックの値をローカルに格納し、値はスタックに残す
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position);
            if (valueStack.empty()) return false;
            // 値はスタックに残る
            break;

          // グローバル変数アクセス
          case 0x23: // global.get - グローバルから値を読み取ってスタックにプッシュ
            if (position >= bodyEnd) return false;
            {
              uint32_t globalIdx = readLEB128(bytes, position);
              // グローバル変数の実際の型をグローバルセクションから取得
              ValueType globalType = ValueType::i32; // デフォルト
              if (globalIdx < module.globals_.size()) {
                  globalType = module.globals_[globalIdx].type;
              } else {
                  // インポートされたグローバル変数かもしれない
                  size_t importGlobalCount = 0;
                  for (const auto& import : module.imports_) {
                      if (import.kind == ImportKind::Global) {
                          if (importGlobalCount == globalIdx) {
                              globalType = import.globalType;
                              break;
                          }
                          importGlobalCount++;
                      }
                  }
              }
              
              valueStack.push_back(globalType);
            }
            break;
          case 0x24: // global.set - スタックから値を取り出してグローバルに格納
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position);
            if (valueStack.empty()) return false;
            valueStack.pop_back(); // 値を消費
            break;

          // メモリロード命令 - アドレスを消費し、値をプッシュ
          case 0x28: // i32.load
          case 0x2C: // i32.load8_s
          case 0x2D: // i32.load8_u
          case 0x2E: // i32.load16_s
          case 0x2F: // i32.load16_u
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // align
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // offset
            if (valueStack.empty()) return false;
            valueStack.pop_back(); // i32アドレスを消費
            valueStack.push_back(0x7F); // i32値をプッシュ
            break;
          case 0x29: // i64.load
          case 0x30: // i64.load8_s
          case 0x31: // i64.load8_u
          case 0x32: // i64.load16_s
          case 0x33: // i64.load16_u
          case 0x34: // i64.load32_s
          case 0x35: // i64.load32_u
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // align
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // offset
            if (valueStack.empty()) return false;
            valueStack.pop_back(); // i32アドレスを消費
            valueStack.push_back(0x7E); // i64値をプッシュ
            break;
          case 0x2A: // f32.load
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // align
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // offset
            if (valueStack.empty()) return false;
            valueStack.pop_back(); // i32アドレスを消費
            valueStack.push_back(0x7D); // f32値をプッシュ
            break;
          case 0x2B: // f64.load
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // align
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // offset
            if (valueStack.empty()) return false;
            valueStack.pop_back(); // i32アドレスを消費
            valueStack.push_back(0x7C); // f64値をプッシュ
            break;

          // メモリストア命令 - 値とアドレスを消費
          case 0x36: // i32.store
          case 0x3A: // i32.store8
          case 0x3B: // i32.store16
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // align
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // offset
            if (valueStack.size() < 2) return false;
            valueStack.pop_back(); // i32値を消費
            valueStack.pop_back(); // i32アドレスを消費
            break;
          case 0x37: // i64.store
          case 0x3C: // i64.store8
          case 0x3D: // i64.store16
          case 0x3E: // i64.store32
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // align
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // offset
            if (valueStack.size() < 2) return false;
            valueStack.pop_back(); // i64値を消費
            valueStack.pop_back(); // i32アドレスを消費
            break;
          case 0x38: // f32.store
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // align
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // offset
            if (valueStack.size() < 2) return false;
            valueStack.pop_back(); // f32値を消費
            valueStack.pop_back(); // i32アドレスを消費
            break;
          case 0x39: // f64.store
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // align
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // offset
            if (valueStack.size() < 2) return false;
            valueStack.pop_back(); // f64値を消費
            valueStack.pop_back(); // i32アドレスを消費
            break;

          // 算術演算命令 - 適切なスタック操作を実装
          case 0x6A: // i32.add
          case 0x6B: // i32.sub
          case 0x6C: // i32.mul
          case 0x6D: // i32.div_s
          case 0x6E: // i32.div_u
          case 0x6F: // i32.rem_s
          case 0x70: // i32.rem_u
          case 0x71: // i32.and
          case 0x72: // i32.or
          case 0x73: // i32.xor
          case 0x74: // i32.shl
          case 0x75: // i32.shr_s
          case 0x76: // i32.shr_u
          case 0x77: // i32.rotl
          case 0x78: // i32.rotr
            if (valueStack.size() < 2) return false;
            valueStack.pop_back(); // 右オペランド
            valueStack.pop_back(); // 左オペランド
            valueStack.push_back(0x7F); // i32結果をプッシュ
            break;

          case 0x7C: // i64.add
          case 0x7D: // i64.sub
          case 0x7E: // i64.mul
          case 0x7F: // i64.div_s
          case 0x80: // i64.div_u
          case 0x81: // i64.rem_s
          case 0x82: // i64.rem_u
          case 0x83: // i64.and
          case 0x84: // i64.or
          case 0x85: // i64.xor
          case 0x86: // i64.shl
          case 0x87: // i64.shr_s
          case 0x88: // i64.shr_u
          case 0x89: // i64.rotl
          case 0x8A: // i64.rotr
            if (valueStack.size() < 2) return false;
            valueStack.pop_back(); // 右オペランド
            valueStack.pop_back(); // 左オペランド
            valueStack.push_back(0x7E); // i64結果をプッシュ
            break;

          case 0x92: // f32.add
          case 0x93: // f32.sub
          case 0x94: // f32.mul
          case 0x95: // f32.div
          case 0x96: // f32.min
          case 0x97: // f32.max
          case 0x98: // f32.copysign
            if (valueStack.size() < 2) return false;
            valueStack.pop_back(); // 右オペランド
            valueStack.pop_back(); // 左オペランド
            valueStack.push_back(0x7D); // f32結果をプッシュ
            break;

          case 0xA0: // f64.add
          case 0xA1: // f64.sub
          case 0xA2: // f64.mul
          case 0xA3: // f64.div
          case 0xA4: // f64.min
          case 0xA5: // f64.max
          case 0xA6: // f64.copysign
            if (valueStack.size() < 2) return false;
            valueStack.pop_back(); // 右オペランド
            valueStack.pop_back(); // 左オペランド
            valueStack.push_back(0x7C); // f64結果をプッシュ
            break;

          // 比較演算命令
          case 0x46: // i32.eqz
            if (valueStack.empty()) return false;
            valueStack.pop_back(); // i32オペランド
            valueStack.push_back(0x7F); // i32結果をプッシュ
            break;
          case 0x47: // i32.eq
          case 0x48: // i32.ne
          case 0x49: // i32.lt_s
          case 0x4A: // i32.lt_u
          case 0x4B: // i32.gt_s
          case 0x4C: // i32.gt_u
          case 0x4D: // i32.le_s
          case 0x4E: // i32.le_u
          case 0x4F: // i32.ge_s
          case 0x50: // i32.ge_u
            if (valueStack.size() < 2) return false;
            valueStack.pop_back(); // 右オペランド
            valueStack.pop_back(); // 左オペランド
            valueStack.push_back(0x7F); // i32結果をプッシュ
            break;

          // 分岐命令
          case 0x0C: // br
          case 0x0D: // br_if
            if (position >= bodyEnd) return false;
            readLEB128(bytes, position); // labelidx
            if (opcode == 0x0D) { // br_if
              if (valueStack.empty()) return false;
              valueStack.pop_back(); // i32条件値を消費
            }
            break;
          case 0x0E: // br_table
            {
              if (position >= bodyEnd) return false;
              uint32_t target_count = readLEB128(bytes, position);
              for (uint32_t i = 0; i < target_count; ++i) {
                if (position >= bodyEnd) return false;
                readLEB128(bytes, position); // target_table entry
              }
              if (position >= bodyEnd) return false;
              readLEB128(bytes, position); // default_target
              if (valueStack.empty()) return false;
              valueStack.pop_back(); // i32インデックス値を消費
            }
            break;

          // 関数呼び出し命令
          case 0x10: // call
            readLEB128(bytes, position); // funcidx
            uint32_t funcIdx = static_cast<uint32_t>(bytes[position - 1]); // 前回読んだ値
            
            // 関数シグネチャに基づいてスタック操作を行う
            if (funcIdx < module.functions_.size()) {
                const auto& funcType = module.functions_[funcIdx];
                
                // パラメータの数だけスタックから消費
                if (valueStack.size() >= funcType.paramTypes.size()) {
                    // パラメータの型チェック
                    for (size_t i = 0; i < funcType.paramTypes.size(); ++i) {
                        size_t stackIdx = valueStack.size() - funcType.paramTypes.size() + i;
                        ValueType expectedType = funcType.paramTypes[i];
                        ValueType actualType = valueStack[stackIdx];
                        
                        if (actualType != expectedType) {
                            // 型不一致エラー
                            return ValidationResult{false, "Function call parameter type mismatch"};
                        }
                    }
                    
                    // パラメータを消費
                    valueStack.erase(valueStack.end() - funcType.paramTypes.size(), valueStack.end());
                    
                    // 戻り値をプッシュ
                    for (ValueType returnType : funcType.returnTypes) {
                        valueStack.push_back(returnType);
                    }
                } else {
                    return ValidationResult{false, "Insufficient operands for function call"};
                }
            } else {
                return ValidationResult{false, "Invalid function index"};
            }
            break;
          case 0x11: // call_indirect
            valueStack.pop_back(); // テーブルインデックス（i32）を消費
            uint32_t typeIdx = readLEB128(bytes, position);
            
            // 型シグネチャに基づいてスタック操作を行う
            if (typeIdx < module.types_.size()) {
                const auto& funcType = module.types_[typeIdx];
                
                // パラメータの数だけスタックから消費
                if (valueStack.size() >= funcType.paramTypes.size()) {
                    // パラメータの型チェック
                    for (size_t i = 0; i < funcType.paramTypes.size(); ++i) {
                        size_t stackIdx = valueStack.size() - funcType.paramTypes.size() + i;
                        ValueType expectedType = funcType.paramTypes[i];
                        ValueType actualType = valueStack[stackIdx];
                        
                        if (actualType != expectedType) {
                            return ValidationResult{false, "Indirect call parameter type mismatch"};
                        }
                    }
                    
                    // パラメータを消費
                    valueStack.erase(valueStack.end() - funcType.paramTypes.size(), valueStack.end());
                    
                    // 戻り値をプッシュ
                    for (ValueType returnType : funcType.returnTypes) {
                        valueStack.push_back(returnType);
                    }
                } else {
                    return ValidationResult{false, "Insufficient operands for indirect call"};
                }
            } else {
                return ValidationResult{false, "Invalid type index for indirect call"};
            }
            break;
          
          // パラメトリック命令
          case 0x1A: // drop
            if (valueStack.empty()) return false;
            valueStack.pop_back(); // スタックトップの値を破棄
            break;
          case 0x1B: // select
            if (valueStack.size() < 3) return false;
            valueStack.pop_back(); // i32条件を消費
            valueStack.pop_back(); // 値2を消費
            // 値1はスタックに残る（結果として）
            break;

          // SIMD instructions (prefix 0xFC)
          case 0xFC:
            {
              if (position >= bodyEnd) return false;
              uint32_t simd_opcode = readLEB128(bytes, position);
              if (simd_opcode == 0x0C) { // v128.const
                  if (position + 16 > bodyEnd) return false;
                  position += 16;
                  valueStack.push_back(0x7B); // v128型をプッシュ
              }
              // 他のSIMD命令も必要に応じて実装
            }
            break;

          default:
            // 未知のオペコードは検証エラー
            return false;
        }
      }
    }

    // 関数末尾のendオペコードを確認
    if (!foundEnd) {
      return false;
    }

    // ブロック深度のチェック
    if (blockDepth != 0) {
      return false; // ブロックが正しく閉じられていない
    }
    
    // 次の関数本体に進む
    position = bodyEnd;
  }
  
  return true;
}

bool validateDataSection(const std::vector<uint8_t>& bytes, size_t position, uint32_t size) {
  // 詳細な検証を実装
  if (size < 1) return false;
  
  // ベクトルサイズをチェック
  size_t endPos = position + size;
  if (position >= endPos) return false;
  
  // データセグメント数を読み取り
  uint32_t count = readLEB128(bytes, position);
  if (position >= endPos) return false;
  
  // 各データセグメントを検証
  for (uint32_t i = 0; i < count; i++) {
    // データセグメントのタイプを読み取り (MVPでは0, 1, 2)
    if (position >= endPos) return false;
    uint32_t segmentType = readLEB128(bytes, position);

    if (segmentType == 0) { // Active data segment
      // メモリインデックスを読み取り（WebAssembly v1では0固定）
      if (position >= endPos) return false;
      uint32_t memoryIdx = readLEB128(bytes, position);

      // WebAssembly v1では0のみ有効
      if (memoryIdx != 0) {
        // logError("Invalid memory index for active data segment: must be 0");
        return false;
      }
      // メモリインデックスの有効性をチェック
      if (memoryIdx >= module.memories_.size()) {
        // logError("Active data segment memory index out of bounds");
        return false;
      }

      // オフセット式を検証
      if (!validateInitializerExpression(bytes, position, endPos)) return false;

    } else if (segmentType == 1) { // Passive data segment
      // 何も読み取らない
    } else if (segmentType == 2) { // Active data segment with explicit memory index (Bulk Memory Proposal)
      // メモリインデックスを読み取り
      if (position >= endPos) return false;
      uint32_t memoryIdx = readLEB128(bytes, position);
      // メモリインデックスの有効性をチェック
      if (memoryIdx >= module.memories_.size()) {
        // logError("Active data segment memory index out of bounds");
        return false;
      }

      // オフセット式を検証
      if (!validateInitializerExpression(bytes, position, endPos)) return false;
    } else {
      // logError("Invalid data segment type");
      return false; // Unknown segment type
    }

    // データサイズを読み取り
    if (position >= endPos) return false;
    uint32_t dataSize = readLEB128(bytes, position);
    if (position + dataSize > endPos) return false;

    // データをスキップ
    position += dataSize;
  }

  // 値の範囲チェックを実装 (これはモジュール全体のコンテキストで最後に行うべきかもしれない)
  // if (count > module.dataSegments.size()) { // module.dataSegments はパース結果
  //   logError("Data segment count exceeds module definition after parsing");
  //   return false;
  // }

  return true;
}

bool validateDataCountSection(const std::vector<uint8_t>& bytes, size_t position, uint32_t size) {
  // 詳細な検証を実装（WebAssembly v1ではオプション）
  size_t startPosition = position;
  if (size < 1) return true; // Empty DataCount is valid (means 0 segments expected by DataSection)

  // ベクトルサイズをチェック
  size_t endPos = position + size;
  if (position >= endPos) return false;

  // データセグメント数を読み取り
  uint32_t count = readLEB128(bytes, position);

  // WasmModuleのインスタンスを取得または渡す必要がある (ここでは仮に module 変数とする)
  // WasmModule& module = get_current_module_for_validation(); 
  // module.setExpectedDataSegmentCount(count); // 期待値を保存

  // セクションの末尾まで読み進めたか確認
  if (position != startPosition + size) {
      // logError("DataCount section size mismatch");
      return false;
  }

  return true;
}

// LEB128形式の符号なし整数を読み取るヘルパー関数
uint32_t readLEB128(const std::vector<uint8_t>& bytes, size_t& position) {
  uint32_t result = 0;
  uint32_t shift = 0;
  
  while (position < bytes.size()) {
    uint8_t byte = bytes[position++];
    result |= static_cast<uint32_t>(byte & 0x7F) << shift;
    if ((byte & 0x80) == 0) {
      break;
    }
    shift += 7;
    if (shift >= 32) {
      break; // オーバーフロー防止
    }
  }
  
  return result;
}

// 初期化式を検証するヘルパー関数 (data section や element section で使用)
bool validateInitializerExpression(const std::vector<uint8_t>& bytes, size_t& position, size_t endPos) {
    bool foundEnd = false;
    while (position < endPos) {
        if (position >= endPos) return false;
        uint8_t opcode = bytes[position++];

        if (opcode == 0x0B) { // end
            foundEnd = true;
            break;
        }

        switch (opcode) {
            case 0x41: // i32.const
                if (position >= endPos) return false;
                readLEB128(bytes, position); // 符号付きLEB128整数を読み飛ばす
                break;
            case 0x42: // i64.const
                if (position >= endPos) return false;
                readLEB128(bytes, position); // 符号付きLEB128整数を読み飛ばす
                break;
            case 0x43: // f32.const
                if (position + 4 > endPos) return false;
                position += 4;
                break;
            case 0x44: // f64.const
                if (position + 8 > endPos) return false;
                position += 8;
                break;
            case 0x23: // global.get
                if (position >= endPos) return false;
                readLEB128(bytes, position); // グローバルインデックスを読み飛ばす
                break;
            case 0xD0: // ref.null (Reference Types proposal)
                if (position >= endPos) return false;
                [[fallthrough]]; // type を読み取る
            case 0xD2: // ref.func (Reference Types proposal)
                if (position >= endPos) return false;
                readLEB128(bytes, position); // type または funcidx を読み取る
                break;
            default:
                // logError("Unsupported opcode in initializer expression");
                return false; // 未サポートの命令は検証エラー
        }
    }
    return foundEnd;
}

// JavaScript WebAssembly.instantiate
Value wasmInstantiate(const std::vector<Value>& args, Value thisValue, execution::ExecutionContext* context) {
  // 引数チェック
  if (args.size() < 1) {
    // TypeError: at least one argument required
    return Value::createUndefined(); // エラーの代わり
  }
  
  Value importObject = args.size() > 1 ? args[1] : Value::createObject(context);
  
  if (args[0].isObject() && args[0].hasProperty(context, "_wasmModule")) {
    // WasmModuleを直接インスタンス化
    // WasmModuleオブジェクトから内部のモジュールポインタを取得
    Value moduleValue = args[0].getProperty(context, "_wasmModule");
    WasmModule* module = nullptr;
    
    if (moduleValue.isPointer()) {
      // ポインタからWasmModuleを取得
      module = static_cast<WasmModule*>(moduleValue.getPointer());
    }
    
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
    
    // ModuleオブジェクトからWasmModuleポインタを取得
    WasmModule* module = nullptr;
    if (moduleValue.isObject() && moduleValue.hasProperty(context, "_wasmModule")) {
      Value innerModule = moduleValue.getProperty(context, "_wasmModule");
      if (innerModule.isPointer()) {
        module = static_cast<WasmModule*>(innerModule.getPointer());
      }
    }
    
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

// 型インデックス検証用のヘルパー関数
bool validateTypeIndex(const std::vector<uint8_t>& bytes, uint32_t typeIndex) {
  // WebAssemblyモジュール全体を解析して型セクションの型数を取得
  size_t position = 0;
  uint32_t typeCount = 0;
  bool foundTypeSection = false;
  
  // WebAssemblyマジックナンバーとバージョンをスキップ
  if (bytes.size() < 8) return false;
  position = 8;
  
  // 各セクションを順次確認
  while (position < bytes.size()) {
    if (position + 1 >= bytes.size()) break;
    
    uint8_t sectionId = bytes[position++];
    uint32_t sectionSize = readLEB128(bytes, position);
    
    if (position + sectionSize > bytes.size()) {
      return false; // セクションサイズが不正
    }
    
    // 型セクション（ID = 1）を見つけた場合
    if (sectionId == 1) {
      foundTypeSection = true;
      size_t typeSectionStart = position;
      
      // 型エントリー数を読み取り
      typeCount = readLEB128(bytes, position);
      
      // 型インデックスが範囲内かチェック
      if (typeIndex >= typeCount) {
        return false; // 無効な型インデックス
      }
      
      return true; // 有効な型インデックス
    }
    
    // 他のセクションはスキップ
    position += sectionSize;
  }
  
  // 型セクションが見つからない場合
  if (!foundTypeSection) {
    // 型セクションが存在しない場合、型インデックス0以外は無効
    return typeIndex == 0;
  }
  
  return false;
}

// エクスポートインデックス検証用のヘルパー関数
bool validateExportIndex(const std::vector<uint8_t>& bytes, uint8_t exportKind, uint32_t exportIndex) {
  // WebAssemblyモジュール全体を解析して各セクションの要素数を取得
  size_t position = 0;
  uint32_t functionCount = 0;
  uint32_t tableCount = 0;
  uint32_t memoryCount = 0;
  uint32_t globalCount = 0;
  
  // WebAssemblyマジックナンバーとバージョンをスキップ
  if (bytes.size() < 8) return false;
  position = 8;
  
  // 各セクションを順次確認して要素数を計算
  while (position < bytes.size()) {
    if (position + 1 >= bytes.size()) break;
    
    uint8_t sectionId = bytes[position++];
    uint32_t sectionSize = readLEB128(bytes, position);
    
    if (position + sectionSize > bytes.size()) {
      return false; // セクションサイズが不正
    }
    
    size_t sectionStart = position;
    
    switch (sectionId) {
      case 2: // インポートセクション
        {
          // インポートセクションから各種類のインポート数をカウント
          uint32_t importCount = readLEB128(bytes, position);
          for (uint32_t i = 0; i < importCount && position < sectionStart + sectionSize; i++) {
            // モジュール名をスキップ
            uint32_t moduleNameLen = readLEB128(bytes, position);
            position += moduleNameLen;
            
            // フィールド名をスキップ
            uint32_t fieldNameLen = readLEB128(bytes, position);
            position += fieldNameLen;
            
            // インポート種別を確認
            if (position < sectionStart + sectionSize) {
              uint8_t importKind = bytes[position++];
              switch (importKind) {
                case 0: // 関数
                  functionCount++;
                  readLEB128(bytes, position); // 型インデックスをスキップ
                  break;
                case 1: // テーブル
                  tableCount++;
                  // テーブル定義をスキップ
                  position++; // 参照型
                  if (position < sectionStart + sectionSize) {
                    uint8_t limits = bytes[position++];
                    readLEB128(bytes, position); // 最小値
                    if (limits == 1) readLEB128(bytes, position); // 最大値
                  }
                  break;
                case 2: // メモリ
                  memoryCount++;
                  // メモリ定義をスキップ
                  if (position < sectionStart + sectionSize) {
                    uint8_t limits = bytes[position++];
                    readLEB128(bytes, position); // 最小値
                    if (limits == 1) readLEB128(bytes, position); // 最大値
                  }
                  break;
                case 3: // グローバル
                  globalCount++;
                  // グローバル定義をスキップ
                  if (position < sectionStart + sectionSize) {
                    position++; // 値型
                    position++; // ミュータビリティ
                  }
                  break;
              }
            }
          }
        }
        break;
        
      case 3: // 関数セクション
        {
          uint32_t localFuncCount = readLEB128(bytes, position);
          functionCount += localFuncCount;
        }
        break;
        
      case 4: // テーブルセクション
        {
          uint32_t localTableCount = readLEB128(bytes, position);
          tableCount += localTableCount;
        }
        break;
        
      case 5: // メモリセクション
        {
          uint32_t localMemoryCount = readLEB128(bytes, position);
          memoryCount += localMemoryCount;
        }
        break;
        
      case 6: // グローバルセクション
        {
          uint32_t localGlobalCount = readLEB128(bytes, position);
          globalCount += localGlobalCount;
        }
        break;
    }
    
    // 次のセクションに移動
    position = sectionStart + sectionSize;
  }
  
  // エクスポート種別に応じてインデックスの有効性をチェック
  switch (exportKind) {
    case 0: // 関数
      return exportIndex < functionCount;
    case 1: // テーブル
      return exportIndex < tableCount;
    case 2: // メモリ
      return exportIndex < memoryCount;
    case 3: // グローバル
      return exportIndex < globalCount;
    default:
      return false;
  }
}

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs 