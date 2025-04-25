#ifndef AEROJS_CORE_JIT_BYTECODE_BYTECODE_PARSER_H
#define AEROJS_CORE_JIT_BYTECODE_BYTECODE_PARSER_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <stdexcept>
#include <array>

#include "bytecode.h"

namespace aerojs {
namespace core {

// バイトコードファイルのマジックナンバー
constexpr uint32_t BYTECODE_MAGIC = 0x41455242; // 'AERO' (リトルエンディアン)

// バイトコードのバージョン
constexpr uint16_t BYTECODE_VERSION_MAJOR = 1;
constexpr uint16_t BYTECODE_VERSION_MINOR = 0;

// バイトコードファイルヘッダー
struct BytecodeFileHeader {
    uint32_t magic;          // マジックナンバー (BYTECODE_MAGIC)
    uint16_t versionMajor;   // メジャーバージョン
    uint16_t versionMinor;   // マイナーバージョン
    uint32_t timestamp;      // 生成タイムスタンプ
    uint32_t functionCount;  // 関数の数
    uint32_t stringCount;    // 文字列テーブルのエントリ数
    uint32_t flags;          // 追加フラグ (将来の拡張用)
};

// パース中に発生するエラー
class BytecodeParseError : public std::runtime_error {
public:
    explicit BytecodeParseError(const std::string& what)
        : std::runtime_error(what) {}
};

// バイナリデータからの読み取りを助けるクラス
class BinaryReader {
public:
    explicit BinaryReader(const std::vector<uint8_t>& data)
        : m_data(data), m_position(0) {}

    // 1バイト読み取り
    uint8_t ReadByte() {
        CheckBounds(1);
        return m_data[m_position++];
    }

    // 2バイト読み取り (リトルエンディアン)
    uint16_t ReadUInt16() {
        CheckBounds(2);
        uint16_t value = static_cast<uint16_t>(m_data[m_position]) |
                        (static_cast<uint16_t>(m_data[m_position + 1]) << 8);
        m_position += 2;
        return value;
    }

    // 4バイト読み取り (リトルエンディアン)
    uint32_t ReadUInt32() {
        CheckBounds(4);
        uint32_t value = static_cast<uint32_t>(m_data[m_position]) |
                        (static_cast<uint32_t>(m_data[m_position + 1]) << 8) |
                        (static_cast<uint32_t>(m_data[m_position + 2]) << 16) |
                        (static_cast<uint32_t>(m_data[m_position + 3]) << 24);
        m_position += 4;
        return value;
    }

    // 8バイト読み取り (リトルエンディアン)
    uint64_t ReadUInt64() {
        CheckBounds(8);
        uint64_t value = static_cast<uint64_t>(m_data[m_position]) |
                        (static_cast<uint64_t>(m_data[m_position + 1]) << 8) |
                        (static_cast<uint64_t>(m_data[m_position + 2]) << 16) |
                        (static_cast<uint64_t>(m_data[m_position + 3]) << 24) |
                        (static_cast<uint64_t>(m_data[m_position + 4]) << 32) |
                        (static_cast<uint64_t>(m_data[m_position + 5]) << 40) |
                        (static_cast<uint64_t>(m_data[m_position + 6]) << 48) |
                        (static_cast<uint64_t>(m_data[m_position + 7]) << 56);
        m_position += 8;
        return value;
    }

    // 4バイト浮動小数点数読み取り
    float ReadFloat() {
        union {
            uint32_t i;
            float f;
        } value;
        value.i = ReadUInt32();
        return value.f;
    }

    // 8バイト浮動小数点数読み取り
    double ReadDouble() {
        union {
            uint64_t i;
            double d;
        } value;
        value.i = ReadUInt64();
        return value.d;
    }

    // 文字列読み取り (長さ接頭バイト付き)
    std::string ReadString() {
        uint32_t length = ReadUInt32();
        CheckBounds(length);
        std::string result(reinterpret_cast<const char*>(&m_data[m_position]), length);
        m_position += length;
        return result;
    }

    // 現在位置を取得
    size_t GetPosition() const {
        return m_position;
    }

    // 特定の位置にシーク
    void Seek(size_t position) {
        if (position > m_data.size()) {
            throw BytecodeParseError("Seek position out of bounds");
        }
        m_position = position;
    }

    // データの終端かどうか
    bool IsEof() const {
        return m_position >= m_data.size();
    }

    // データの長さを取得
    size_t GetLength() const {
        return m_data.size();
    }

    // 残りのデータのサイズを取得
    size_t GetRemainingSize() const {
        return m_data.size() - m_position;
    }

private:
    // バウンドチェック
    void CheckBounds(size_t size) {
        if (m_position + size > m_data.size()) {
            throw BytecodeParseError("Read beyond end of data");
        }
    }

    const std::vector<uint8_t>& m_data;
    size_t m_position;
};

// バイトコードパーサー
class BytecodeParser {
public:
    // ファイルからバイトコードをロード
    static std::unique_ptr<BytecodeModule> LoadFromFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) {
            throw BytecodeParseError("Failed to open file: " + filename);
        }

        // ファイルサイズを取得
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        // データを読み込む
        std::vector<uint8_t> data(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
            throw BytecodeParseError("Failed to read file: " + filename);
        }

        return LoadFromMemory(data, filename);
    }

    // メモリからバイトコードをロード
    static std::unique_ptr<BytecodeModule> LoadFromMemory(
            const std::vector<uint8_t>& data, const std::string& filename = "<memory>") {
        BinaryReader reader(data);
        return Parse(reader, filename);
    }

private:
    // バイトコードのパース
    static std::unique_ptr<BytecodeModule> Parse(BinaryReader& reader, const std::string& filename) {
        // ヘッダーの読み取り
        BytecodeFileHeader header;
        header.magic = reader.ReadUInt32();
        if (header.magic != BYTECODE_MAGIC) {
            throw BytecodeParseError("Invalid bytecode magic number");
        }

        header.versionMajor = reader.ReadUInt16();
        header.versionMinor = reader.ReadUInt16();
        
        // バージョンチェック
        if (header.versionMajor > BYTECODE_VERSION_MAJOR ||
            (header.versionMajor == BYTECODE_VERSION_MAJOR && 
             header.versionMinor > BYTECODE_VERSION_MINOR)) {
            throw BytecodeParseError("Unsupported bytecode version: " + 
                                    std::to_string(header.versionMajor) + "." + 
                                    std::to_string(header.versionMinor));
        }

        header.timestamp = reader.ReadUInt32();
        header.functionCount = reader.ReadUInt32();
        header.stringCount = reader.ReadUInt32();
        header.flags = reader.ReadUInt32();

        // モジュールの作成
        auto module = std::make_unique<BytecodeModule>(filename);

        // 文字列テーブルの読み取り
        std::vector<std::string> stringTable;
        stringTable.reserve(header.stringCount);
        for (uint32_t i = 0; i < header.stringCount; ++i) {
            stringTable.push_back(reader.ReadString());
        }

        // 関数の読み取り
        for (uint32_t i = 0; i < header.functionCount; ++i) {
            module->AddFunction(ParseFunction(reader, stringTable));
        }

        return module;
    }

    // 関数のパース
    static std::unique_ptr<BytecodeFunction> ParseFunction(
            BinaryReader& reader, const std::vector<std::string>& stringTable) {
        // 関数名と引数の数を取得
        uint32_t nameIndex = reader.ReadUInt32();
        std::string name = GetStringFromTable(stringTable, nameIndex);
        uint32_t paramCount = reader.ReadUInt32();

        auto function = std::make_unique<BytecodeFunction>(name, paramCount);

        // 定数プールの読み取り
        uint32_t constantCount = reader.ReadUInt32();
        ParseConstants(reader, function.get(), constantCount, stringTable);

        // 命令の読み取り
        uint32_t instructionCount = reader.ReadUInt32();
        ParseInstructions(reader, function.get(), instructionCount);

        // 例外ハンドラの読み取り
        uint32_t handlerCount = reader.ReadUInt32();
        ParseExceptionHandlers(reader, function.get(), handlerCount);

        return function;
    }

    // 定数プールのパース
    static void ParseConstants(BinaryReader& reader, BytecodeFunction* function,
                              uint32_t constantCount, const std::vector<std::string>& stringTable) {
        for (uint32_t i = 0; i < constantCount; ++i) {
            auto type = static_cast<ConstantType>(reader.ReadByte());
            std::unique_ptr<Constant> constant;

            switch (type) {
                case ConstantType::Undefined:
                    constant = std::make_unique<UndefinedConstant>();
                    break;

                case ConstantType::Null:
                    constant = std::make_unique<NullConstant>();
                    break;

                case ConstantType::Boolean:
                    constant = std::make_unique<BooleanConstant>(reader.ReadByte() != 0);
                    break;

                case ConstantType::Number:
                    constant = std::make_unique<NumberConstant>(reader.ReadDouble());
                    break;

                case ConstantType::String: {
                    uint32_t stringIndex = reader.ReadUInt32();
                    constant = std::make_unique<StringConstant>(GetStringFromTable(stringTable, stringIndex));
                    break;
                }

                case ConstantType::RegExp: {
                    uint32_t patternIndex = reader.ReadUInt32();
                    uint32_t flagsIndex = reader.ReadUInt32();
                    std::string pattern = GetStringFromTable(stringTable, patternIndex);
                    std::string flags = GetStringFromTable(stringTable, flagsIndex);
                    constant = std::make_unique<RegExpConstant>(pattern, flags);
                    break;
                }

                default:
                    throw BytecodeParseError("Unknown constant type: " + std::to_string(static_cast<int>(type)));
            }

            function->AddConstant(std::move(constant));
        }
    }

    // 命令のパース
    static void ParseInstructions(BinaryReader& reader, BytecodeFunction* function, uint32_t instructionCount) {
        for (uint32_t i = 0; i < instructionCount; ++i) {
            // オペコードと行・列情報の読み取り
            auto opcode = static_cast<BytecodeOpcode>(reader.ReadByte());
            uint32_t offset = reader.ReadUInt32();
            uint32_t line = reader.ReadUInt32();
            uint32_t column = reader.ReadUInt32();

            // オペランドの読み取り
            uint8_t operandCount = GetBytecodecOpcodeOperandCount(opcode);
            std::vector<uint32_t> operands;
            operands.reserve(operandCount);

            for (uint8_t j = 0; j < operandCount; ++j) {
                operands.push_back(reader.ReadUInt32());
            }

            // 命令の追加
            BytecodeInstruction instruction(opcode, operands, offset, line, column);
            function->AddInstruction(instruction);
        }
    }

    // 例外ハンドラのパース
    static void ParseExceptionHandlers(BinaryReader& reader, BytecodeFunction* function, uint32_t handlerCount) {
        for (uint32_t i = 0; i < handlerCount; ++i) {
            auto type = static_cast<HandlerType>(reader.ReadByte());
            uint32_t tryStartOffset = reader.ReadUInt32();
            uint32_t tryEndOffset = reader.ReadUInt32();
            uint32_t handlerOffset = reader.ReadUInt32();
            uint32_t handlerEndOffset = reader.ReadUInt32();
            uint32_t finallyOffset = reader.ReadUInt32();

            ExceptionHandler handler(type, tryStartOffset, tryEndOffset,
                                    handlerOffset, handlerEndOffset, finallyOffset);
            function->AddExceptionHandler(handler);
        }
    }

    // 文字列テーブルからの取得（インデックスチェック付き）
    static std::string GetStringFromTable(const std::vector<std::string>& stringTable, uint32_t index) {
        if (index >= stringTable.size()) {
            throw BytecodeParseError("String table index out of bounds: " + std::to_string(index));
        }
        return stringTable[index];
    }
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_BYTECODE_BYTECODE_PARSER_H 