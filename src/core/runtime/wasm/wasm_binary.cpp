/**
 * @file wasm_binary.cpp
 * @brief WebAssemblyバイナリパーサーの実装
 * @version 1.0.0
 * @license MIT
 */

#include "wasm_binary.h"
#include <cassert>
#include <cstring>
#include <iostream>

namespace aerojs {
namespace core {
namespace runtime {
namespace wasm {

// WebAssemblyセクション種別
enum class SectionId : uint8_t {
    Custom = 0,
    Type = 1,
    Import = 2,
    Function = 3,
    Table = 4,
    Memory = 5,
    Global = 6,
    Export = 7,
    Start = 8,
    Element = 9,
    Code = 10,
    Data = 11,
    DataCount = 12
};

// コンストラクタ
WasmBinaryParser::WasmBinaryParser(const std::vector<uint8_t>& binary)
    : m_binary(binary), m_position(0) {
}

// バイナリの解析
bool WasmBinaryParser::parse() {
    // ヘッダーチェック
    if (!checkHeader()) {
        return false;
    }
    
    // 各セクションを順に解析
    while (m_position < m_binary.size()) {
        if (!parseSection()) {
            return false;
        }
    }
    
    return true;
}

// ヘッダーチェック
bool WasmBinaryParser::checkHeader() {
    // バイナリサイズチェック
    if (m_binary.size() < 8) {
        return false;
    }
    
    // マジックナンバーチェック (0x00 'a' 's' 'm')
    if (m_binary[0] != 0x00 || m_binary[1] != 0x61 || m_binary[2] != 0x73 || m_binary[3] != 0x6D) {
        return false;
    }
    
    // バージョンチェック (現在は1.0のみサポート)
    if (m_binary[4] != 0x01 || m_binary[5] != 0x00 || m_binary[6] != 0x00 || m_binary[7] != 0x00) {
        return false;
    }
    
    // ヘッダー分だけ位置を進める
    m_position = 8;
    
    return true;
}

// セクション解析
bool WasmBinaryParser::parseSection() {
    // セクションIDの読み取り
    uint8_t sectionId = readByte();
    
    // セクションサイズの読み取り
    uint32_t sectionSize = readVarUint32();
    
    // 現在位置を保存（セクション終了位置の計算用）
    size_t startPos = m_position;
    
    // セクション種別に応じて処理
    bool result = false;
    switch (static_cast<SectionId>(sectionId)) {
        case SectionId::Custom:
            result = parseCustomSection(sectionSize);
            break;
            
        case SectionId::Type:
            result = parseTypeSection(sectionSize);
            break;
            
        case SectionId::Import:
            result = parseImportSection(sectionSize);
            break;
            
        case SectionId::Function:
            result = parseFunctionSection(sectionSize);
            break;
            
        case SectionId::Table:
            result = parseTableSection(sectionSize);
            break;
            
        case SectionId::Memory:
            result = parseMemorySection(sectionSize);
            break;
            
        case SectionId::Global:
            result = parseGlobalSection(sectionSize);
            break;
            
        case SectionId::Export:
            result = parseExportSection(sectionSize);
            break;
            
        case SectionId::Start:
            result = parseStartSection(sectionSize);
            break;
            
        case SectionId::Element:
            result = parseElementSection(sectionSize);
            break;
            
        case SectionId::Code:
            result = parseCodeSection(sectionSize);
            break;
            
        case SectionId::Data:
            result = parseDataSection(sectionSize);
            break;
            
        case SectionId::DataCount:
            result = parseDataCountSection(sectionSize);
            break;
            
        default:
            // 不明なセクションはスキップ
            m_position += sectionSize;
            result = true;
            break;
    }
    
    // セクションのサイズが正確であることを確認
    if (result && (m_position - startPos) != sectionSize) {
        // セクションサイズが不正
        return false;
    }
    
    return result;
}

// カスタムセクションの解析
bool WasmBinaryParser::parseCustomSection(uint32_t sectionSize) {
    size_t endPos = m_position + sectionSize;
    
    // セクション名の読み取り
    std::string sectionName = readName();
    
    // カスタムセクションのデータを取得
    size_t dataSize = endPos - m_position;
    std::vector<uint8_t> data = readBytes(dataSize);
    
    // カスタムセクションを登録
    m_customSections[sectionName] = data;
    
    return true;
}

// 型セクションの解析
bool WasmBinaryParser::parseTypeSection(uint32_t sectionSize) {
    // エントリ数を読み取り
    uint32_t count = readVarUint32();
    
    // 各型を読み取り
    for (uint32_t i = 0; i < count; i++) {
        // 関数型を読み取り
        WasmFunctionType funcType = readFunctionType();
        m_types.push_back(funcType);
    }
    
    return true;
}

// インポートセクションの解析
bool WasmBinaryParser::parseImportSection(uint32_t sectionSize) {
    // エントリ数を読み取り
    uint32_t count = readVarUint32();
    
    // 各インポートを読み取り
    for (uint32_t i = 0; i < count; i++) {
        // インポート記述子を作成
        WasmImportDescriptor import;
        
        // モジュール名とフィールド名を読み取り
        import.module = readName();
        import.name = readName();
        
        // インポート種別を読み取り
        uint8_t kind = readByte();
        import.kind = static_cast<WasmImportDescriptor::Kind>(kind);
        
        // 種別に応じた追加情報を読み取り
        switch (import.kind) {
            case WasmImportDescriptor::Kind::FUNCTION: {
                // 関数型インデックスを読み取り
                uint32_t typeIdx = readVarUint32();
                if (typeIdx >= m_types.size()) {
                    return false;
                }
                import.functionType = m_types[typeIdx];
                break;
            }
                
            case WasmImportDescriptor::Kind::TABLE: {
                // テーブル型を読み取り
                uint8_t elemType = readByte();
                import.tableType.elemType = static_cast<WasmValueType>(elemType);
                
                // リミットを読み取り
                uint8_t hasMax = readByte();
                import.tableType.min = readVarUint32();
                if (hasMax) {
                    import.tableType.max = readVarUint32();
                }
                break;
            }
                
            case WasmImportDescriptor::Kind::MEMORY: {
                // メモリ型を読み取り
                uint8_t flags = readByte();
                bool hasMax = (flags & 0x1) != 0;
                import.memoryType.shared = (flags & 0x2) != 0;
                
                import.memoryType.min = readVarUint32();
                if (hasMax) {
                    import.memoryType.max = readVarUint32();
                }
                break;
            }
                
            case WasmImportDescriptor::Kind::GLOBAL: {
                // グローバル型を読み取り
                import.globalType.type = readValueType();
                import.globalType.mutable_ = readByte() != 0;
                break;
            }
                
            default:
                // 不明なインポート種別
                return false;
        }
        
        m_imports.push_back(import);
    }
    
    return true;
}

// 関数セクションの解析
bool WasmBinaryParser::parseFunctionSection(uint32_t sectionSize) {
    // エントリ数を読み取り
    uint32_t count = readVarUint32();
    
    // 各関数の型インデックスを読み取り
    for (uint32_t i = 0; i < count; i++) {
        uint32_t typeIdx = readVarUint32();
        if (typeIdx >= m_types.size()) {
            return false;
        }
        m_functions.push_back(typeIdx);
    }
    
    return true;
}

// テーブルセクションの解析
bool WasmBinaryParser::parseTableSection(uint32_t sectionSize) {
    // エントリ数を読み取り
    uint32_t count = readVarUint32();
    
    // 各テーブルを読み取り
    for (uint32_t i = 0; i < count; i++) {
        WasmModule::ModuleImpl::TableType table;
        
        // 要素型を読み取り
        uint8_t elemType = readByte();
        table.elemType = static_cast<WasmValueType>(elemType);
        
        // リミットを読み取り
        uint8_t hasMax = readByte();
        table.initialSize = readVarUint32();
        if (hasMax) {
            table.maximumSize = readVarUint32();
        }
        
        m_tables.push_back(table);
    }
    
    return true;
}

// メモリセクションの解析
bool WasmBinaryParser::parseMemorySection(uint32_t sectionSize) {
    // エントリ数を読み取り
    uint32_t count = readVarUint32();
    
    // 各メモリを読み取り
    for (uint32_t i = 0; i < count; i++) {
        WasmModule::ModuleImpl::MemoryType memory;
        
        // フラグを読み取り
        uint8_t flags = readByte();
        bool hasMax = (flags & 0x1) != 0;
        memory.shared = (flags & 0x2) != 0;
        
        // ページ数を読み取り
        memory.initialPages = readVarUint32();
        if (hasMax) {
            memory.maximumPages = readVarUint32();
        }
        
        m_memories.push_back(memory);
    }
    
    return true;
}

// グローバルセクションの解析
bool WasmBinaryParser::parseGlobalSection(uint32_t sectionSize) {
    // エントリ数を読み取り
    uint32_t count = readVarUint32();
    
    // 各グローバル変数を読み取り
    for (uint32_t i = 0; i < count; i++) {
        WasmModule::ModuleImpl::GlobalType global;
        
        // 型とミュータビリティを読み取り
        global.type = readValueType();
        global.mutable_ = readByte() != 0;
        
        // 初期化式を読み取り
        global.initExpr = readInitExpression();
        
        m_globals.push_back(global);
    }
    
    return true;
}

// エクスポートセクションの解析
bool WasmBinaryParser::parseExportSection(uint32_t sectionSize) {
    // エントリ数を読み取り
    uint32_t count = readVarUint32();
    
    // 各エクスポートを読み取り
    for (uint32_t i = 0; i < count; i++) {
        WasmExportDescriptor exp;
        
        // 名前を読み取り
        exp.name = readName();
        
        // 種別とインデックスを読み取り
        uint8_t kind = readByte();
        exp.kind = static_cast<WasmExportDescriptor::Kind>(kind);
        exp.index = readVarUint32();
        
        m_exports.push_back(exp);
    }
    
    return true;
}

// スタートセクションの解析
bool WasmBinaryParser::parseStartSection(uint32_t sectionSize) {
    // スタート関数のインデックスを読み取り
    uint32_t funcIdx = readVarUint32();
    m_startFunction = funcIdx;
    
    return true;
}

// 要素セクションの解析
bool WasmBinaryParser::parseElementSection(uint32_t sectionSize) {
    // エントリ数を読み取り
    uint32_t count = readVarUint32();
    
    // 各要素セグメントを読み取り
    for (uint32_t i = 0; i < count; i++) {
        WasmModule::ModuleImpl::ElementSegment elem;
        
        // テーブルインデックスを読み取り
        elem.tableIndex = readVarUint32();
        
        // オフセット式を読み取り
        elem.offsetExpr = readInitExpression();
        
        // 関数インデックスの数を読み取り
        uint32_t numFuncs = readVarUint32();
        
        // 各関数インデックスを読み取り
        for (uint32_t j = 0; j < numFuncs; j++) {
            uint32_t funcIdx = readVarUint32();
            elem.functionIndices.push_back(funcIdx);
        }
        
        m_elements.push_back(elem);
    }
    
    return true;
}

// コードセクションの解析
bool WasmBinaryParser::parseCodeSection(uint32_t sectionSize) {
    // エントリ数を読み取り
    uint32_t count = readVarUint32();
    
    // 各関数本体を読み取り
    for (uint32_t i = 0; i < count; i++) {
        WasmModule::ModuleImpl::FunctionBody body;
        
        // 関数本体のサイズを読み取り
        uint32_t bodySize = readVarUint32();
        size_t bodyStart = m_position;
        
        // ローカル変数の数を読み取り
        uint32_t localCount = readVarUint32();
        
        // 各ローカル変数を読み取り
        for (uint32_t j = 0; j < localCount; j++) {
            uint32_t count = readVarUint32();
            WasmValueType type = readValueType();
            body.locals.push_back(std::make_pair(count, type));
        }
        
        // 関数本体のコードを読み取り
        size_t codeSize = bodySize - (m_position - bodyStart);
        body.code = readBytes(codeSize);
        
        m_functionBodies.push_back(body);
    }
    
    return true;
}

// データセクションの解析
bool WasmBinaryParser::parseDataSection(uint32_t sectionSize) {
    // エントリ数を読み取り
    uint32_t count = readVarUint32();
    
    // 各データセグメントを読み取り
    for (uint32_t i = 0; i < count; i++) {
        WasmModule::ModuleImpl::DataSegment data;
        
        // メモリインデックスを読み取り
        data.memoryIndex = readVarUint32();
        
        // オフセット式を読み取り
        data.offsetExpr = readInitExpression();
        
        // データサイズを読み取り
        uint32_t dataSize = readVarUint32();
        
        // データを読み取り
        data.data = readBytes(dataSize);
        
        m_dataSegments.push_back(data);
    }
    
    return true;
}

// データカウントセクションの解析
bool WasmBinaryParser::parseDataCountSection(uint32_t sectionSize) {
    // データセグメント数を読み取り
    uint32_t count = readVarUint32();
    
    // このセクションは検証用なので、特に何もしない
    return true;
}

// 1バイト読み取り
uint8_t WasmBinaryParser::readByte() {
    if (m_position >= m_binary.size()) {
        throw std::out_of_range("Binary read out of bounds");
    }
    return m_binary[m_position++];
}

// 4バイト整数読み取り (リトルエンディアン)
uint32_t WasmBinaryParser::readUint32() {
    uint32_t result = 0;
    result |= static_cast<uint32_t>(readByte());
    result |= static_cast<uint32_t>(readByte()) << 8;
    result |= static_cast<uint32_t>(readByte()) << 16;
    result |= static_cast<uint32_t>(readByte()) << 24;
    return result;
}

// 8バイト整数読み取り (リトルエンディアン)
uint64_t WasmBinaryParser::readUint64() {
    uint64_t result = 0;
    result |= static_cast<uint64_t>(readByte());
    result |= static_cast<uint64_t>(readByte()) << 8;
    result |= static_cast<uint64_t>(readByte()) << 16;
    result |= static_cast<uint64_t>(readByte()) << 24;
    result |= static_cast<uint64_t>(readByte()) << 32;
    result |= static_cast<uint64_t>(readByte()) << 40;
    result |= static_cast<uint64_t>(readByte()) << 48;
    result |= static_cast<uint64_t>(readByte()) << 56;
    return result;
}

// 可変長符号付き32ビット整数読み取り (LEB128)
int32_t WasmBinaryParser::readVarInt32() {
    uint32_t result = 0;
    uint32_t shift = 0;
    uint8_t byte;
    
    do {
        byte = readByte();
        result |= (byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    
    // 符号拡張
    if (shift < 32 && (byte & 0x40)) {
        result |= (~0u << shift);
    }
    
    return static_cast<int32_t>(result);
}

// 可変長符号付き64ビット整数読み取り (LEB128)
int64_t WasmBinaryParser::readVarInt64() {
    uint64_t result = 0;
    uint64_t shift = 0;
    uint8_t byte;
    
    do {
        byte = readByte();
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    
    // 符号拡張
    if (shift < 64 && (byte & 0x40)) {
        result |= (~0ull << shift);
    }
    
    return static_cast<int64_t>(result);
}

// 可変長符号なし32ビット整数読み取り (LEB128)
uint32_t WasmBinaryParser::readVarUint32() {
    uint32_t result = 0;
    uint32_t shift = 0;
    uint8_t byte;
    
    do {
        byte = readByte();
        result |= (byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    
    return result;
}

// 可変長符号なし64ビット整数読み取り (LEB128)
uint64_t WasmBinaryParser::readVarUint64() {
    uint64_t result = 0;
    uint64_t shift = 0;
    uint8_t byte;
    
    do {
        byte = readByte();
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    
    return result;
}

// 32ビット浮動小数点数読み取り
float WasmBinaryParser::readFloat32() {
    uint32_t bits = readUint32();
    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

// 64ビット浮動小数点数読み取り
double WasmBinaryParser::readFloat64() {
    uint64_t bits = readUint64();
    double result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

// 名前読み取り（長さ付き文字列）
std::string WasmBinaryParser::readName() {
    // 長さを読み取り
    uint32_t length = readVarUint32();
    
    // バイト列を読み取り
    std::vector<uint8_t> bytes = readBytes(length);
    
    // 文字列に変換
    return std::string(bytes.begin(), bytes.end());
}

// バイト列読み取り
std::vector<uint8_t> WasmBinaryParser::readBytes(uint32_t size) {
    if (m_position + size > m_binary.size()) {
        throw std::out_of_range("Binary read out of bounds");
    }
    
    std::vector<uint8_t> result(m_binary.begin() + m_position, m_binary.begin() + m_position + size);
    m_position += size;
    
    return result;
}

// 値型読み取り
WasmValueType WasmBinaryParser::readValueType() {
    uint8_t type = readByte();
    
    switch (type) {
        case 0x7F: return WasmValueType::I32;
        case 0x7E: return WasmValueType::I64;
        case 0x7D: return WasmValueType::F32;
        case 0x7C: return WasmValueType::F64;
        case 0x7B: return WasmValueType::V128;
        case 0x70: return WasmValueType::FUNCREF;
        case 0x6F: return WasmValueType::ANYREF;
        default:
            throw std::runtime_error("Invalid value type");
    }
}

// 関数型読み取り
WasmFunctionType WasmBinaryParser::readFunctionType() {
    WasmFunctionType result;
    
    // 関数型のタグを確認 (0x60)
    uint8_t tag = readByte();
    if (tag != 0x60) {
        throw std::runtime_error("Invalid function type tag");
    }
    
    // パラメータ数を読み取り
    uint32_t paramCount = readVarUint32();
    
    // パラメータ型を読み取り
    for (uint32_t i = 0; i < paramCount; i++) {
        result.paramTypes.push_back(readValueType());
    }
    
    // 戻り値数を読み取り
    uint32_t returnCount = readVarUint32();
    
    // 戻り値型を読み取り
    for (uint32_t i = 0; i < returnCount; i++) {
        result.returnTypes.push_back(readValueType());
    }
    
    return result;
}

// 初期化式読み取り
std::vector<uint8_t> WasmBinaryParser::readInitExpression() {
    std::vector<uint8_t> expr;
    uint8_t byte;
    
    // end命令 (0x0B) まで読み取り
    do {
        byte = readByte();
        expr.push_back(byte);
    } while (byte != 0x0B);
    
    return expr;
}

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs 