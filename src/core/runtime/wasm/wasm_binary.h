/**
 * @file wasm_binary.h
 * @brief WebAssemblyバイナリパーサー
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include "wasm_module.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace aerojs {
namespace core {
namespace runtime {
namespace wasm {

/**
 * @brief WebAssemblyバイナリパーサー
 * 
 * WebAssemblyバイナリフォーマットを解析し、内部表現に変換します。
 */
class WasmBinaryParser {
public:
    /**
     * @brief コンストラクタ
     * @param binary WebAssemblyバイナリデータ
     */
    explicit WasmBinaryParser(const std::vector<uint8_t>& binary);
    
    /**
     * @brief バイナリの解析
     * @return 解析が成功した場合はtrue
     */
    bool parse();
    
    // 解析結果取得メソッド
    const std::vector<WasmFunctionType>& getTypes() const { return m_types; }
    const std::vector<WasmImportDescriptor>& getImports() const { return m_imports; }
    const std::vector<uint32_t>& getFunctions() const { return m_functions; }
    
    const std::vector<WasmModule::ModuleImpl::TableType>& getTables() const { return m_tables; }
    const std::vector<WasmModule::ModuleImpl::MemoryType>& getMemories() const { return m_memories; }
    const std::vector<WasmModule::ModuleImpl::GlobalType>& getGlobals() const { return m_globals; }
    
    const std::vector<WasmExportDescriptor>& getExports() const { return m_exports; }
    std::optional<uint32_t> getStartFunction() const { return m_startFunction; }
    
    const std::vector<WasmModule::ModuleImpl::ElementSegment>& getElements() const { return m_elements; }
    const std::vector<WasmModule::ModuleImpl::DataSegment>& getDataSegments() const { return m_dataSegments; }
    const std::vector<WasmModule::ModuleImpl::FunctionBody>& getFunctionBodies() const { return m_functionBodies; }
    
    const std::unordered_map<std::string, std::vector<uint8_t>>& getCustomSections() const { return m_customSections; }
    
private:
    // バイナリデータ
    const std::vector<uint8_t>& m_binary;
    
    // 現在の解析位置
    size_t m_position;
    
    // 解析結果
    std::vector<WasmFunctionType> m_types;
    std::vector<WasmImportDescriptor> m_imports;
    std::vector<uint32_t> m_functions;
    std::vector<WasmModule::ModuleImpl::TableType> m_tables;
    std::vector<WasmModule::ModuleImpl::MemoryType> m_memories;
    std::vector<WasmModule::ModuleImpl::GlobalType> m_globals;
    std::vector<WasmExportDescriptor> m_exports;
    std::optional<uint32_t> m_startFunction;
    std::vector<WasmModule::ModuleImpl::ElementSegment> m_elements;
    std::vector<WasmModule::ModuleImpl::DataSegment> m_dataSegments;
    std::vector<WasmModule::ModuleImpl::FunctionBody> m_functionBodies;
    std::unordered_map<std::string, std::vector<uint8_t>> m_customSections;
    
    // ヘルパーメソッド
    bool checkHeader();
    bool parseSection();
    bool parseCustomSection(uint32_t sectionSize);
    bool parseTypeSection(uint32_t sectionSize);
    bool parseImportSection(uint32_t sectionSize);
    bool parseFunctionSection(uint32_t sectionSize);
    bool parseTableSection(uint32_t sectionSize);
    bool parseMemorySection(uint32_t sectionSize);
    bool parseGlobalSection(uint32_t sectionSize);
    bool parseExportSection(uint32_t sectionSize);
    bool parseStartSection(uint32_t sectionSize);
    bool parseElementSection(uint32_t sectionSize);
    bool parseCodeSection(uint32_t sectionSize);
    bool parseDataSection(uint32_t sectionSize);
    bool parseDataCountSection(uint32_t sectionSize);
    
    // データ読み込みヘルパー
    uint8_t readByte();
    uint32_t readUint32();
    uint64_t readUint64();
    int32_t readVarInt32();
    int64_t readVarInt64();
    uint32_t readVarUint32();
    uint64_t readVarUint64();
    float readFloat32();
    double readFloat64();
    std::string readName();
    std::vector<uint8_t> readBytes(uint32_t size);
    
    // 型関連ヘルパー
    WasmValueType readValueType();
    WasmFunctionType readFunctionType();
    
    // 式解析ヘルパー
    std::vector<uint8_t> readInitExpression();
};

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs 