#include "bytecode_parser.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace aerojs {
namespace core {

// ファイルからバイトコードをロードする実装
std::unique_ptr<BytecodeModule> BytecodeParser::LoadFromFile(const std::string& filename) {
    try {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) {
            throw BytecodeParseError("ファイルを開けませんでした: " + filename);
        }

        // ファイルサイズを取得
        auto size = file.tellg();
        if (size <= 0) {
            throw BytecodeParseError("ファイルが空です: " + filename);
        }
        
        file.seekg(0, std::ios::beg);

        // データを読み込む
        std::vector<uint8_t> data(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
            throw BytecodeParseError("ファイル読み込みに失敗しました: " + filename);
        }

        return LoadFromMemory(data, filename);
    } catch (const BytecodeParseError& e) {
        // 元の例外をそのまま再スロー
        throw;
    } catch (const std::exception& e) {
        // 標準例外をBytecodeParseErrorに変換
        throw BytecodeParseError(std::string("ファイル読み込み中に例外が発生しました: ") + e.what());
    }
}

// メモリからバイトコードをロードする実装
std::unique_ptr<BytecodeModule> BytecodeParser::LoadFromMemory(
        const std::vector<uint8_t>& data, const std::string& filename) {
    try {
        BinaryReader reader(data);
        return Parse(reader, filename);
    } catch (const BytecodeParseError& e) {
        // 元の例外をそのまま再スロー
        throw;
    } catch (const std::exception& e) {
        // 標準例外をBytecodeParseErrorに変換
        throw BytecodeParseError(std::string("バイトコードパース中に例外が発生しました: ") + e.what());
    }
}

// バイトコードのパース
std::unique_ptr<BytecodeModule> BytecodeParser::Parse(BinaryReader& reader, const std::string& filename) {
    // ヘッダーの読み取り
    BytecodeFileHeader header;
    
    // マジックナンバーの検証
    try {
        header.magic = reader.ReadUInt32();
    } catch (const BytecodeParseError&) {
        throw BytecodeParseError("バイトコードファイルが短すぎます");
    }
    
    if (header.magic != BYTECODE_MAGIC) {
        throw BytecodeParseError("無効なバイトコードファイル: マジックナンバーが一致しません");
    }

    // バージョン情報の読み取り
    header.versionMajor = reader.ReadUInt16();
    header.versionMinor = reader.ReadUInt16();
    
    // バージョンチェック
    if (header.versionMajor > BYTECODE_VERSION_MAJOR ||
        (header.versionMajor == BYTECODE_VERSION_MAJOR && 
         header.versionMinor > BYTECODE_VERSION_MINOR)) {
        std::ostringstream oss;
        oss << "サポートされていないバイトコードバージョン: " 
            << header.versionMajor << "." << header.versionMinor
            << " (サポートバージョン: " << BYTECODE_VERSION_MAJOR 
            << "." << BYTECODE_VERSION_MINOR << " 以下)";
        throw BytecodeParseError(oss.str());
    }

    // その他のヘッダー情報の読み取り
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
        try {
            stringTable.push_back(reader.ReadString());
        } catch (const BytecodeParseError& e) {
            throw BytecodeParseError(std::string("文字列テーブルの読み取りエラー: ") + e.what());
        }
    }

    // 関数の読み取り
    for (uint32_t i = 0; i < header.functionCount; ++i) {
        try {
            module->AddFunction(ParseFunction(reader, stringTable));
        } catch (const BytecodeParseError& e) {
            throw BytecodeParseError(std::string("関数#") + std::to_string(i) + 
                                    "のパースエラー: " + e.what());
        }
    }

    // 残りのデータがあるか確認（警告として出力）
    if (!reader.IsEof()) {
        std::cerr << "警告: バイトコードファイルに余分なデータがあります（" 
                  << reader.GetRemainingSize() << "バイト）" << std::endl;
    }

    return module;
}

// 関数のパース
std::unique_ptr<BytecodeFunction> BytecodeParser::ParseFunction(
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
void BytecodeParser::ParseConstants(BinaryReader& reader, BytecodeFunction* function,
                          uint32_t constantCount, const std::vector<std::string>& stringTable) {
    for (uint32_t i = 0; i < constantCount; ++i) {
        auto type = static_cast<ConstantType>(reader.ReadByte());
        std::unique_ptr<Constant> constant;

        try {
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
                    throw BytecodeParseError("未知の定数タイプ: " + std::to_string(static_cast<int>(type)));
            }
        } catch (const BytecodeParseError& e) {
            throw BytecodeParseError(std::string("定数#") + std::to_string(i) + 
                                   "のパースエラー: " + e.what());
        }

        function->AddConstant(std::move(constant));
    }
}

// 命令のパース
void BytecodeParser::ParseInstructions(BinaryReader& reader, BytecodeFunction* function, uint32_t instructionCount) {
    for (uint32_t i = 0; i < instructionCount; ++i) {
        try {
            // オペコードと行・列情報の読み取り
            auto opcode = static_cast<BytecodeOpcode>(reader.ReadByte());
            
            // オペコードの範囲チェック
            if (static_cast<int>(opcode) < 0 || opcode >= BytecodeOpcode::End) {
                throw BytecodeParseError("無効なオペコード: " + std::to_string(static_cast<int>(opcode)));
            }
            
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
        } catch (const BytecodeParseError& e) {
            throw BytecodeParseError(std::string("命令#") + std::to_string(i) + 
                                   "のパースエラー: " + e.what());
        }
    }
}

// 例外ハンドラのパース
void BytecodeParser::ParseExceptionHandlers(BinaryReader& reader, BytecodeFunction* function, uint32_t handlerCount) {
    for (uint32_t i = 0; i < handlerCount; ++i) {
        try {
            auto type = static_cast<HandlerType>(reader.ReadByte());
            
            // ハンドラタイプの範囲チェック
            if (static_cast<int>(type) < 0 || static_cast<int>(type) > 2) {
                throw BytecodeParseError("無効な例外ハンドラタイプ: " + std::to_string(static_cast<int>(type)));
            }
            
            uint32_t tryStartOffset = reader.ReadUInt32();
            uint32_t tryEndOffset = reader.ReadUInt32();
            uint32_t handlerOffset = reader.ReadUInt32();
            uint32_t handlerEndOffset = reader.ReadUInt32();
            uint32_t finallyOffset = reader.ReadUInt32();
            
            // オフセットの整合性チェック
            if (tryEndOffset < tryStartOffset) {
                throw BytecodeParseError("無効なtryブロック範囲");
            }
            
            if (handlerEndOffset < handlerOffset) {
                throw BytecodeParseError("無効なハンドラブロック範囲");
            }

            ExceptionHandler handler(type, tryStartOffset, tryEndOffset,
                                   handlerOffset, handlerEndOffset, finallyOffset);
            function->AddExceptionHandler(handler);
        } catch (const BytecodeParseError& e) {
            throw BytecodeParseError(std::string("例外ハンドラ#") + std::to_string(i) + 
                                   "のパースエラー: " + e.what());
        }
    }
}

// 文字列テーブルからの取得（インデックスチェック付き）
std::string BytecodeParser::GetStringFromTable(const std::vector<std::string>& stringTable, uint32_t index) {
    if (index >= stringTable.size()) {
        throw BytecodeParseError("文字列テーブルインデックスが範囲外です: " + std::to_string(index));
    }
    return stringTable[index];
}

} // namespace core
} // namespace aerojs 