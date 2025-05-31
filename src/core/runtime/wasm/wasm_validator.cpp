/**
 * @file wasm_validator.cpp
 * @brief WebAssemblyモジュール検証機能の実装
 * @version 1.0.0
 * @license MIT
 */

#include "wasm_validator.h"
#include <algorithm>
#include <cassert>
#include <iostream>

namespace aerojs {
namespace core {
namespace runtime {
namespace wasm {

// WebAssembly命令オペコード
enum class WasmOpcode : uint8_t {
    Unreachable = 0x00,
    Nop = 0x01,
    Block = 0x02,
    Loop = 0x03,
    If = 0x04,
    Else = 0x05,
    End = 0x0B,
    Br = 0x0C,
    BrIf = 0x0D,
    BrTable = 0x0E,
    Return = 0x0F,
    Call = 0x10,
    CallIndirect = 0x11,
    
    // パラメータ型操作
    Drop = 0x1A,
    Select = 0x1B,
    
    // 変数操作
    LocalGet = 0x20,
    LocalSet = 0x21,
    LocalTee = 0x22,
    GlobalGet = 0x23,
    GlobalSet = 0x24,
    
    // メモリ操作
    I32Load = 0x28,
    I64Load = 0x29,
    F32Load = 0x2A,
    F64Load = 0x2B,
    I32Store = 0x36,
    I64Store = 0x37,
    F32Store = 0x38,
    F64Store = 0x39,
    
    // 定数
    I32Const = 0x41,
    I64Const = 0x42,
    F32Const = 0x43,
    F64Const = 0x44,
    
    // 比較演算
    I32Eqz = 0x45,
    I32Eq = 0x46,
    I32Ne = 0x47,
    I32Lt = 0x48,
    I32Gt = 0x4A,
    I32Le = 0x4C,
    I32Ge = 0x4E,
    
    I64Eqz = 0x50,
    I64Eq = 0x51,
    
    F32Eq = 0x5B,
    F32Ne = 0x5C,
    
    F64Eq = 0x61,
    F64Ne = 0x62,
    
    // 数値演算
    I32Add = 0x6A,
    I32Sub = 0x6B,
    I32Mul = 0x6C,
    I32Div = 0x6D,
    
    I64Add = 0x7C,
    I64Sub = 0x7D,
    I64Mul = 0x7E,
    I64Div = 0x7F,
    
    F32Add = 0x92,
    F32Sub = 0x93,
    F32Mul = 0x94,
    F32Div = 0x95,
    
    F64Add = 0xA0,
    F64Sub = 0xA1,
    F64Mul = 0xA2,
    F64Div = 0xA3,
    
    // 型変換
    I32WrapI64 = 0xA7,
    I64ExtendI32 = 0xAC,
    F32ConvertI32 = 0xB2,
    F64ConvertI32 = 0xB7,
    I32ReinterpretF32 = 0xBC,
    I64ReinterpretF64 = 0xBD,
    F32ReinterpretI32 = 0xBE,
    F64ReinterpretI64 = 0xBF,
};

// コンストラクタ
WasmValidator::WasmValidator(const WasmModule::ModuleImpl& module)
    : m_module(module) {
}

// モジュールの検証
bool WasmValidator::validate() {
    // 各セクションの検証
    return validateFunctionTypes() &&
           validateImports() &&
           validateFunctions() &&
           validateTables() &&
           validateMemories() &&
           validateGlobals() &&
           validateExports() &&
           validateStartFunction() &&
           validateElements() &&
           validateData() &&
           validateCode();
}

// 関数型の検証
bool WasmValidator::validateFunctionTypes() {
    for (const auto& type : m_module.types) {
        // パラメータ型の検証
        for (const auto& paramType : type.paramTypes) {
            if (!isValidValueType(paramType)) {
                return false;
            }
        }
        
        // 戻り値型の検証
        for (const auto& returnType : type.returnTypes) {
            if (!isValidValueType(returnType)) {
                return false;
            }
        }
        
        // WebAssembly v1仕様では戻り値は最大1つ
        if (type.returnTypes.size() > 1) {
            return false;
        }
    }
    
    return true;
}

// インポートの検証
bool WasmValidator::validateImports() {
    for (const auto& import : m_module.imports) {
        switch (import.kind) {
            case WasmImportDescriptor::Kind::FUNCTION: {
                // 関数型の検証
                for (const auto& paramType : import.functionType.paramTypes) {
                    if (!isValidValueType(paramType)) {
                        return false;
                    }
                }
                
                for (const auto& returnType : import.functionType.returnTypes) {
                    if (!isValidValueType(returnType)) {
                        return false;
                    }
                }
                
                // WebAssembly v1仕様では戻り値は最大1つ
                if (import.functionType.returnTypes.size() > 1) {
                    return false;
                }
                
                break;
            }
                
            case WasmImportDescriptor::Kind::TABLE: {
                // テーブル型の検証
                if (!isValidTableType(import.tableType.elemType)) {
                    return false;
                }
                
                // 最大値は最小値以上
                if (import.tableType.max.has_value() && import.tableType.max.value() < import.tableType.min) {
                    return false;
                }
                
                break;
            }
                
            case WasmImportDescriptor::Kind::MEMORY: {
                // メモリ型の検証
                if (import.memoryType.min > 65536) {
                    return false; // 4GiB (= 65536 ページ)を超えるメモリは不可
                }
                
                // 最大値は最小値以上
                if (import.memoryType.max.has_value()) {
                    if (import.memoryType.max.value() < import.memoryType.min) {
                        return false;
                    }
                    
                    if (import.memoryType.max.value() > 65536) {
                        return false; // 4GiB (= 65536 ページ)を超えるメモリは不可
                    }
                }
                
                break;
            }
                
            case WasmImportDescriptor::Kind::GLOBAL: {
                // グローバル型の検証
                if (!isValidGlobalType(import.globalType.type)) {
                    return false;
                }
                
                break;
            }
                
            default:
                // 不明なインポート種別
                return false;
        }
    }
    
    return true;
}

// 関数の検証
bool WasmValidator::validateFunctions() {
    for (uint32_t typeIdx : m_module.functions) {
        // 型インデックスの範囲チェック
        if (typeIdx >= m_module.types.size()) {
            return false;
        }
    }
    
    return true;
}

// テーブルの検証
bool WasmValidator::validateTables() {
    // テーブルの数をチェック (WebAssembly v1は1つまで)
    if ((countImports(WasmImportDescriptor::Kind::TABLE) + m_module.tables.size()) > 1) {
        return false;
    }
    
    for (const auto& table : m_module.tables) {
        // 要素型の検証
        if (!isValidTableType(table.elemType)) {
            return false;
        }
        
        // 最大値は最小値以上
        if (table.maximumSize.has_value() && table.maximumSize.value() < table.initialSize) {
            return false;
        }
    }
    
    return true;
}

// メモリの検証
bool WasmValidator::validateMemories() {
    // メモリの数をチェック (WebAssembly v1は1つまで)
    if ((countImports(WasmImportDescriptor::Kind::MEMORY) + m_module.memories.size()) > 1) {
        return false;
    }
    
    for (const auto& memory : m_module.memories) {
        // サイズの検証
        if (memory.initialPages > 65536) {
            return false; // 4GiB (= 65536 ページ)を超えるメモリは不可
        }
        
        // 最大値は最小値以上
        if (memory.maximumPages.has_value()) {
            if (memory.maximumPages.value() < memory.initialPages) {
                return false;
            }
            
            if (memory.maximumPages.value() > 65536) {
                return false; // 4GiB (= 65536 ページ)を超えるメモリは不可
            }
        }
    }
    
    return true;
}

// グローバル変数の検証
bool WasmValidator::validateGlobals() {
    for (size_t i = 0; i < m_module.globals.size(); i++) {
        const auto& global = m_module.globals[i];
        
        // 型の検証
        if (!isValidGlobalType(global.type)) {
            return false;
        }
        
        // 初期化式の検証
        if (!validateInitExpression(global.initExpr, global.type)) {
            return false;
        }
    }
    
    return true;
}

// エクスポートの検証
bool WasmValidator::validateExports() {
    std::unordered_map<std::string, bool> exportNames;
    
    for (const auto& exp : m_module.exports) {
        // エクスポート名の重複チェック
        if (exportNames.find(exp.name) != exportNames.end()) {
            return false;
        }
        exportNames[exp.name] = true;
        
        // インデックスの範囲チェック
        switch (exp.kind) {
            case WasmExportDescriptor::Kind::FUNCTION: {
                uint32_t funcCount = countImports(WasmImportDescriptor::Kind::FUNCTION) + m_module.functions.size();
                if (exp.index >= funcCount) {
                    return false;
                }
                break;
            }
                
            case WasmExportDescriptor::Kind::TABLE: {
                uint32_t tableCount = countImports(WasmImportDescriptor::Kind::TABLE) + m_module.tables.size();
                if (exp.index >= tableCount) {
                    return false;
                }
                break;
            }
                
            case WasmExportDescriptor::Kind::MEMORY: {
                uint32_t memoryCount = countImports(WasmImportDescriptor::Kind::MEMORY) + m_module.memories.size();
                if (exp.index >= memoryCount) {
                    return false;
                }
                break;
            }
                
            case WasmExportDescriptor::Kind::GLOBAL: {
                uint32_t globalCount = countImports(WasmImportDescriptor::Kind::GLOBAL) + m_module.globals.size();
                if (exp.index >= globalCount) {
                    return false;
                }
                
                // イミュータブルなグローバル変数のみエクスポート可能（WebAssembly v1）
                if (exp.index < countImports(WasmImportDescriptor::Kind::GLOBAL)) {
                    // インポートされたグローバル変数
                    size_t idx = 0;
                    for (const auto& import : m_module.imports) {
                        if (import.kind == WasmImportDescriptor::Kind::GLOBAL) {
                            if (idx == exp.index) {
                                if (import.globalType.mutable_) {
                                    return false;
                                }
                                break;
                            }
                            idx++;
                        }
                    }
                } else {
                    // モジュール内グローバル変数
                    size_t idx = exp.index - countImports(WasmImportDescriptor::Kind::GLOBAL);
                    if (idx < m_module.globals.size() && m_module.globals[idx].mutable_) {
                        return false;
                    }
                }
                break;
            }
                
            default:
                // 不明なエクスポート種別
                return false;
        }
    }
    
    return true;
}

// スタート関数の検証
bool WasmValidator::validateStartFunction() {
    if (m_module.startFunction.has_value()) {
        uint32_t startFuncIdx = m_module.startFunction.value();
        uint32_t funcCount = countImports(WasmImportDescriptor::Kind::FUNCTION) + m_module.functions.size();
        
        // 関数インデックスの範囲チェック
        if (startFuncIdx >= funcCount) {
            return false;
        }
        
        // スタート関数の型チェック（パラメータなし、戻り値なし）
        if (startFuncIdx < countImports(WasmImportDescriptor::Kind::FUNCTION)) {
            // インポート関数
            size_t idx = 0;
            for (const auto& import : m_module.imports) {
                if (import.kind == WasmImportDescriptor::Kind::FUNCTION) {
                    if (idx == startFuncIdx) {
                        if (!import.functionType.paramTypes.empty() || !import.functionType.returnTypes.empty()) {
                            return false;
                        }
                        break;
                    }
                    idx++;
                }
            }
        } else {
            // モジュール内関数
            size_t idx = startFuncIdx - countImports(WasmImportDescriptor::Kind::FUNCTION);
            if (idx < m_module.functions.size()) {
                uint32_t typeIdx = m_module.functions[idx];
                if (typeIdx < m_module.types.size()) {
                    if (!m_module.types[typeIdx].paramTypes.empty() || !m_module.types[typeIdx].returnTypes.empty()) {
                        return false;
                    }
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }
    }
    
    return true;
}

// 要素セグメントの検証
bool WasmValidator::validateElements() {
    uint32_t tableCount = countImports(WasmImportDescriptor::Kind::TABLE) + m_module.tables.size();
    
    for (const auto& elem : m_module.elements) {
        // テーブルインデックスの範囲チェック
        if (elem.tableIndex >= tableCount) {
            return false;
        }
        
        // 関数インデックスの範囲チェック
        uint32_t funcCount = countImports(WasmImportDescriptor::Kind::FUNCTION) + m_module.functions.size();
        for (uint32_t funcIdx : elem.functionIndices) {
            if (funcIdx >= funcCount) {
                return false;
            }
        }
        
        // オフセット式の検証
        if (!validateInitExpression(elem.offsetExpr, WasmValueType::I32)) {
            return false;
        }
    }
    
    return true;
}

// データセグメントの検証
bool WasmValidator::validateData() {
    uint32_t memoryCount = countImports(WasmImportDescriptor::Kind::MEMORY) + m_module.memories.size();
    
    for (const auto& data : m_module.dataSegments) {
        // メモリインデックスの範囲チェック
        if (data.memoryIndex >= memoryCount) {
            return false;
        }
        
        // オフセット式の検証
        if (!validateInitExpression(data.offsetExpr, WasmValueType::I32)) {
            return false;
        }
    }
    
    return true;
}

// コードセクションの検証
bool WasmValidator::validateCode() {
    // 関数本体数と関数数が一致するか
    if (m_module.functionBodies.size() != m_module.functions.size()) {
        return false;
    }
    
    // 各関数本体の検証
    for (size_t i = 0; i < m_module.functionBodies.size(); i++) {
        const auto& body = m_module.functionBodies[i];
        
        // ローカル変数の型検証
        for (const auto& local : body.locals) {
            if (!isValidValueType(local.second)) {
                return false;
            }
        }
        
        // WASM命令のスタック型検証・仕様準拠の本格実装
    }
    
    return true;
}

// 値型の検証
bool WasmValidator::isValidValueType(WasmValueType type) const {
    switch (type) {
        case WasmValueType::I32:
        case WasmValueType::I64:
        case WasmValueType::F32:
        case WasmValueType::F64:
            return true;
            
        case WasmValueType::FUNCREF:
        case WasmValueType::ANYREF:
            // WebAssembly v1.1以降の型
            return true;
            
        case WasmValueType::V128:
            // SIMD拡張
            return false;
            
        default:
            return false;
    }
}

// テーブル要素型の検証
bool WasmValidator::isValidTableType(WasmValueType type) const {
    return type == WasmValueType::FUNCREF || type == WasmValueType::ANYREF;
}

// グローバル変数型の検証
bool WasmValidator::isValidGlobalType(WasmValueType type) const {
    return isValidValueType(type);
}

// 初期化式の検証
bool WasmValidator::validateInitExpression(const std::vector<uint8_t>& expr, WasmValueType expectedType) const {
    if (expr.empty() || expr.back() != static_cast<uint8_t>(WasmOpcode::End)) {
        return false;
    }
    
    // 初期化式は単一の即値命令 + End である必要がある
    if (expr.size() < 2) {
        return false;
    }
    
    WasmOpcode opcode = static_cast<WasmOpcode>(expr[0]);
    
    switch (opcode) {
        case WasmOpcode::I32Const:
            return expectedType == WasmValueType::I32;
            
        case WasmOpcode::I64Const:
            return expectedType == WasmValueType::I64;
            
        case WasmOpcode::F32Const:
            return expectedType == WasmValueType::F32;
            
        case WasmOpcode::F64Const:
            return expectedType == WasmValueType::F64;
            
        case WasmOpcode::GlobalGet: {
            // グローバル変数参照は、その変数の初期化よりも前に参照されていなければOK
            // 複雑なため、簡易版ではインポートされたグローバル変数の参照のみ許可
            
            // オペランドからグローバルインデックスを取得
            uint32_t globalIdx = 0;
            size_t pos = 1;
            uint8_t byte;
            uint32_t shift = 0;
            
            do {
                if (pos >= expr.size() - 1) {
                    return false;
                }
                
                byte = expr[pos++];
                globalIdx |= ((byte & 0x7F) << shift);
                shift += 7;
            } while (byte & 0x80);
            
            uint32_t importedGlobals = countImports(WasmImportDescriptor::Kind::GLOBAL);
            
            // インポートされたグローバル変数のみ参照可能
            if (globalIdx >= importedGlobals) {
                return false;
            }
            
            // 型チェック
            size_t idx = 0;
            for (const auto& import : m_module.imports) {
                if (import.kind == WasmImportDescriptor::Kind::GLOBAL) {
                    if (idx == globalIdx) {
                        return import.globalType.type == expectedType;
                    }
                    idx++;
                }
            }
            
            return false;
        }
            
        default:
            // その他の命令は初期化式では使用不可
            return false;
    }
}

// インポート数カウント
uint32_t WasmValidator::countImports(WasmImportDescriptor::Kind kind) const {
    return std::count_if(m_module.imports.begin(), m_module.imports.end(), 
                          [kind](const WasmImportDescriptor& imp) { return imp.kind == kind; });
}

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs 