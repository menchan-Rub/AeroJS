#ifndef AEROJS_CORE_JIT_BYTECODE_BYTECODE_STRUCTURE_H
#define AEROJS_CORE_JIT_BYTECODE_BYTECODE_STRUCTURE_H

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include "bytecode_opcodes.h"

namespace aerojs {
namespace core {

// バイトコードにおけるオペランドの型
enum class OperandType : uint8_t {
    Int8,       // 8ビット整数
    Int16,      // 16ビット整数
    Int32,      // 32ビット整数
    Int64,      // 64ビット整数
    UInt8,      // 8ビット符号なし整数
    UInt16,     // 16ビット符号なし整数
    UInt32,     // 32ビット符号なし整数
    UInt64,     // 64ビット符号なし整数
    Float32,    // 32ビット浮動小数点数
    Float64,    // 64ビット浮動小数点数
    Boolean,    // 真偽値
    String,     // 文字列インデックス
    Function,   // 関数インデックス
    Variable,   // 変数インデックス
    Register,   // レジスタ番号
    Offset,     // コードオフセット（ジャンプ先など）
};

// オペランドの型のサイズを取得する関数
constexpr size_t GetOperandTypeSize(OperandType type) {
    switch (type) {
        case OperandType::Int8:
        case OperandType::UInt8:
        case OperandType::Boolean:
            return 1;
        case OperandType::Int16:
        case OperandType::UInt16:
            return 2;
        case OperandType::Int32:
        case OperandType::UInt32:
        case OperandType::Float32:
        case OperandType::String:
        case OperandType::Function:
        case OperandType::Variable:
        case OperandType::Register:
        case OperandType::Offset:
            return 4;
        case OperandType::Int64:
        case OperandType::UInt64:
        case OperandType::Float64:
            return 8;
        default:
            return 0;
    }
}

// 定数プールに格納する値の型
enum class ConstantType {
    Undefined,
    Null,
    Boolean,
    Number,
    String,
    BigInt,
    Object,
    Array,
    Function,
    RegExp
};

// 定数値を表す構造体
struct ConstantValue {
    ConstantType type;
    
    union {
        bool booleanValue;
        double numberValue;
        uint32_t stringIndex;  // 文字列テーブル内のインデックス
        uint32_t objectIndex;  // オブジェクト定義のインデックス
        uint32_t arrayIndex;   // 配列定義のインデックス
        uint32_t functionIndex; // 関数定義のインデックス
        uint32_t regexpIndex;  // 正規表現定義のインデックス
    };
    
    // 種々のコンストラクタ
    ConstantValue() : type(ConstantType::Undefined) {}
    
    explicit ConstantValue(bool value) 
        : type(ConstantType::Boolean), booleanValue(value) {}
    
    explicit ConstantValue(double value) 
        : type(ConstantType::Number), numberValue(value) {}
    
    static ConstantValue CreateString(uint32_t index) {
        ConstantValue value;
        value.type = ConstantType::String;
        value.stringIndex = index;
        return value;
    }
    
    static ConstantValue CreateNull() {
        ConstantValue value;
        value.type = ConstantType::Null;
        return value;
    }
    
    static ConstantValue CreateUndefined() {
        ConstantValue value;
        value.type = ConstantType::Undefined;
        return value;
    }
};

// バイトコード命令を表す構造体
struct BytecodeInstruction {
    BytecodeOpcode opcode;
    uint32_t operand;  // オペランド（必要な場合）
    
    BytecodeInstruction(BytecodeOpcode op, uint32_t op_val = 0)
        : opcode(op), operand(op_val) {}
};

// バイトコードの例外ハンドラ情報
struct ExceptionHandler {
    uint32_t tryStartOffset;      // try節開始オフセット
    uint32_t tryEndOffset;        // try節終了オフセット
    uint32_t handlerOffset;       // ハンドラ（catch節）開始オフセット
    uint32_t finallyOffset;       // finally節開始オフセット（存在しない場合は0xFFFFFFFF）
    uint32_t catchVariableIndex;  // catch節で例外を受け取る変数のインデックス
    bool hasFinallyBlock;         // finally節があるか

    // コンストラクタ
    ExceptionHandler()
        : tryStartOffset(0),
          tryEndOffset(0),
          handlerOffset(0),
          finallyOffset(0xFFFFFFFF),
          catchVariableIndex(0),
          hasFinallyBlock(false) {}

    // コンストラクタ
    ExceptionHandler(uint32_t tryStart, uint32_t tryEnd,
                    uint32_t handler, uint32_t finally,
                    uint32_t varIndex, bool hasFinally)
        : tryStartOffset(tryStart),
          tryEndOffset(tryEnd),
          handlerOffset(handler),
          finallyOffset(finally),
          catchVariableIndex(varIndex),
          hasFinallyBlock(hasFinally) {}
};

// バイトコード関数
class BytecodeFunction {
public:
    BytecodeFunction(uint32_t functionIndex, const std::string& name)
        : m_functionIndex(functionIndex),
          m_name(name),
          m_argCount(0),
          m_localVarCount(0),
          m_maxStackDepth(0),
          m_isStrictMode(false) {}

    // 関数インデックスを取得
    uint32_t GetFunctionIndex() const {
        return m_functionIndex;
    }

    // 関数名を取得
    const std::string& GetName() const {
        return m_name;
    }

    // 引数の数を取得・設定
    uint32_t GetArgCount() const {
        return m_argCount;
    }
    void SetArgCount(uint32_t count) {
        m_argCount = count;
    }

    // ローカル変数の数を取得・設定
    uint32_t GetLocalVarCount() const {
        return m_localVarCount;
    }
    void SetLocalVarCount(uint32_t count) {
        m_localVarCount = count;
    }

    // 最大スタック深度を取得・設定
    uint32_t GetMaxStackDepth() const {
        return m_maxStackDepth;
    }
    void SetMaxStackDepth(uint32_t depth) {
        m_maxStackDepth = depth;
    }

    // Strict Modeかどうかを取得・設定
    bool IsStrictMode() const {
        return m_isStrictMode;
    }
    void SetStrictMode(bool strictMode) {
        m_isStrictMode = strictMode;
    }

    // バイトコード命令を追加
    void AddInstruction(const BytecodeInstruction& instruction) {
        m_instructions.push_back(instruction);
    }

    // バイトコード命令を取得
    const std::vector<BytecodeInstruction>& GetInstructions() const {
        return m_instructions;
    }

    // バイトコード命令を変更可能な形で取得
    std::vector<BytecodeInstruction>& GetInstructions() {
        return m_instructions;
    }

    // 例外ハンドラを追加
    void AddExceptionHandler(const ExceptionHandler& handler) {
        m_exceptionHandlers.push_back(handler);
    }

    // 例外ハンドラを取得
    const std::vector<ExceptionHandler>& GetExceptionHandlers() const {
        return m_exceptionHandlers;
    }

    // 例外ハンドラを変更可能な形で取得
    std::vector<ExceptionHandler>& GetExceptionHandlers() {
        return m_exceptionHandlers;
    }

    // ローカル変数名を設定
    void SetLocalVariableName(uint32_t index, const std::string& name) {
        m_localVariableNames[index] = name;
    }

    // ローカル変数名を取得
    std::string GetLocalVariableName(uint32_t index) const {
        auto it = m_localVariableNames.find(index);
        if (it != m_localVariableNames.end()) {
            return it->second;
        }
        return "";
    }

private:
    uint32_t m_functionIndex;                                  // 関数インデックス
    std::string m_name;                                        // 関数名
    uint32_t m_argCount;                                       // 引数の数
    uint32_t m_localVarCount;                                  // ローカル変数の数
    uint32_t m_maxStackDepth;                                  // 最大スタック深度
    bool m_isStrictMode;                                       // Strict Modeかどうか
    std::vector<BytecodeInstruction> m_instructions;           // バイトコード命令
    std::vector<ExceptionHandler> m_exceptionHandlers;         // 例外ハンドラ
    std::unordered_map<uint32_t, std::string> m_localVariableNames;  // ローカル変数名
};

// バイトコードモジュール（複数の関数を含む）
class BytecodeModule {
public:
    BytecodeModule() : m_mainFunctionIndex(0) {}

    // 関数を追加
    void AddFunction(std::unique_ptr<BytecodeFunction> function) {
        uint32_t index = function->GetFunctionIndex();
        m_functions[index] = std::move(function);
    }

    // 関数を取得
    BytecodeFunction* GetFunction(uint32_t index) const {
        auto it = m_functions.find(index);
        if (it != m_functions.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    // メイン関数のインデックスを取得・設定
    uint32_t GetMainFunctionIndex() const {
        return m_mainFunctionIndex;
    }
    void SetMainFunctionIndex(uint32_t index) {
        m_mainFunctionIndex = index;
    }

    // メイン関数を取得
    BytecodeFunction* GetMainFunction() const {
        return GetFunction(m_mainFunctionIndex);
    }

    // 文字列を追加し、そのインデックスを返す
    uint32_t AddString(const std::string& str) {
        auto it = m_stringIndexMap.find(str);
        if (it != m_stringIndexMap.end()) {
            return it->second;
        }

        uint32_t index = static_cast<uint32_t>(m_stringTable.size());
        m_stringTable.push_back(str);
        m_stringIndexMap[str] = index;
        return index;
    }

    // 文字列を取得
    const std::string& GetString(uint32_t index) const {
        static const std::string emptyString;
        if (index >= m_stringTable.size()) {
            return emptyString;
        }
        return m_stringTable[index];
    }

    // 定数を追加し、そのインデックスを返す
    uint32_t AddConstant(const ConstantValue& value) {
        uint32_t index = static_cast<uint32_t>(m_constantPool.size());
        m_constantPool.push_back(value);
        return index;
    }

    // 定数を取得
    const ConstantValue& GetConstant(uint32_t index) const {
        static const ConstantValue emptyValue;
        if (index >= m_constantPool.size()) {
            return emptyValue;
        }
        return m_constantPool[index];
    }

    // 全ての関数を取得
    const std::unordered_map<uint32_t, std::unique_ptr<BytecodeFunction>>& GetFunctions() const {
        return m_functions;
    }

    // 文字列テーブルを取得
    const std::vector<std::string>& GetStringTable() const {
        return m_stringTable;
    }

    // 定数プールを取得
    const std::vector<ConstantValue>& GetConstantPool() const {
        return m_constantPool;
    }

private:
    std::unordered_map<uint32_t, std::unique_ptr<BytecodeFunction>> m_functions;  // 関数テーブル
    uint32_t m_mainFunctionIndex;                                        // メイン関数のインデックス
    std::vector<std::string> m_stringTable;                              // 文字列テーブル
    std::unordered_map<std::string, uint32_t> m_stringIndexMap;          // 文字列→インデックスのマップ
    std::vector<ConstantValue> m_constantPool;                           // 定数プール
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_JIT_BYTECODE_BYTECODE_STRUCTURE_H 