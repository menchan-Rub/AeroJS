/**
 * @file wasm_validator.h
 * @brief WebAssemblyモジュール検証機能
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include "wasm_module.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace aerojs {
namespace core {
namespace runtime {
namespace wasm {

/**
 * @brief WebAssemblyモジュール検証クラス
 * 
 * WebAssemblyモジュールが仕様に準拠しているかを検証します。
 */
class WasmValidator {
public:
    /**
     * @brief コンストラクタ
     * @param module 検証対象のモジュール内部実装
     */
    explicit WasmValidator(const WasmModule::ModuleImpl& module);
    
    /**
     * @brief モジュールの検証を行う
     * @return 検証が成功した場合はtrue
     */
    bool validate();
    
private:
    // モジュール内部実装への参照
    const WasmModule::ModuleImpl& m_module;
    
    // 検証メソッド
    bool validateFunctionTypes();
    bool validateImports();
    bool validateFunctions();
    bool validateTables();
    bool validateMemories();
    bool validateGlobals();
    bool validateExports();
    bool validateStartFunction();
    bool validateElements();
    bool validateData();
    bool validateCode();
    
    // ヘルパーメソッド
    bool isValidValueType(WasmValueType type) const;
    bool isValidTableType(WasmValueType type) const;
    bool isValidGlobalType(WasmValueType type) const;
    bool validateInitExpression(const std::vector<uint8_t>& expr, WasmValueType expectedType) const;
    
    // インポート数カウント
    uint32_t countImports(WasmImportDescriptor::Kind kind) const;
};

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs 