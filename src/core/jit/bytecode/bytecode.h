#ifndef AEROJS_CORE_JIT_BYTECODE_BYTECODE_H
#define AEROJS_CORE_JIT_BYTECODE_BYTECODE_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include "bytecode_opcodes.h"

namespace aerojs {
namespace core {

// バイトコードの操作コード
enum class BytecodeOpcode : uint8_t {
    // スタック操作
    Nop = 0,            // 何もしない
    Pop,                // スタックから値をポップ
    Dup,                // スタックトップの値を複製
    Swap,               // スタックトップの2つの値を交換

    // 定数ロード
    LoadUndefined,      // undefined をプッシュ
    LoadNull,           // null をプッシュ
    LoadTrue,           // true をプッシュ
    LoadFalse,          // false をプッシュ
    LoadZero,           // 0 をプッシュ
    LoadOne,            // 1 をプッシュ
    LoadConst,          // 定数プールから定数をロード

    // 変数操作
    LoadVar,            // 変数をロード
    StoreVar,           // 変数を保存
    LoadGlobal,         // グローバル変数をロード
    StoreGlobal,        // グローバル変数を保存
    LoadThis,           // this をロード
    StoreThis,          // this を保存
    LoadClosureVar,     // クロージャ変数をロード
    StoreClosureVar,    // クロージャ変数を保存

    // 算術演算
    Add,                // 加算
    Sub,                // 減算
    Mul,                // 乗算
    Div,                // 除算
    Mod,                // 剰余
    Pow,                // べき乗
    Inc,                // インクリメント
    Dec,                // デクリメント
    Neg,                // 単項マイナス
    BitAnd,             // ビット論理積
    BitOr,              // ビット論理和
    BitXor,             // ビット排他的論理和
    BitNot,             // ビット否定
    ShiftLeft,          // 左シフト
    ShiftRight,         // 右シフト
    ShiftRightUnsigned, // 符号なし右シフト

    // 比較演算
    Equal,              // 等価比較
    NotEqual,           // 非等価比較
    StrictEqual,        // 厳密等価比較
    StrictNotEqual,     // 厳密非等価比較
    LessThan,           // より小さい
    LessEqual,          // 以下
    GreaterThan,        // より大きい
    GreaterEqual,       // 以上
    In,                 // in 演算子
    InstanceOf,         // instanceof 演算子

    // 論理演算
    LogicalAnd,         // 論理AND
    LogicalOr,          // 論理OR
    LogicalNot,         // 論理NOT

    // 制御フロー
    Jump,               // 無条件ジャンプ
    JumpIfTrue,         // 条件付きジャンプ（真の場合）
    JumpIfFalse,        // 条件付きジャンプ（偽の場合）
    Call,               // 関数呼び出し
    CallMethod,         // メソッド呼び出し
    Return,             // 関数からリターン
    Throw,              // 例外をスロー

    // オブジェクト操作
    CreateObject,       // オブジェクトを作成
    CreateArray,        // 配列を作成
    GetProperty,        // プロパティ取得
    SetProperty,        // プロパティ設定
    DeleteProperty,     // プロパティ削除
    HasProperty,        // プロパティ存在チェック

    // その他
    TypeOf,             // typeof 演算子
    Debugger,           // デバッガーステートメント
    
    // 境界値（有効なオペコードの数）
    End
};

// オペコードのオペランド数を取得する関数
inline uint8_t GetBytecodecOpcodeOperandCount(BytecodeOpcode opcode) {
    switch (opcode) {
        case BytecodeOpcode::Nop:
        case BytecodeOpcode::Pop:
        case BytecodeOpcode::Dup:
        case BytecodeOpcode::Swap:
        case BytecodeOpcode::LoadUndefined:
        case BytecodeOpcode::LoadNull:
        case BytecodeOpcode::LoadTrue:
        case BytecodeOpcode::LoadFalse:
        case BytecodeOpcode::LoadZero:
        case BytecodeOpcode::LoadOne:
        case BytecodeOpcode::LoadThis:
        case BytecodeOpcode::StoreThis:
        case BytecodeOpcode::Add:
        case BytecodeOpcode::Sub:
        case BytecodeOpcode::Mul:
        case BytecodeOpcode::Div:
        case BytecodeOpcode::Mod:
        case BytecodeOpcode::Pow:
        case BytecodeOpcode::Inc:
        case BytecodeOpcode::Dec:
        case BytecodeOpcode::Neg:
        case BytecodeOpcode::BitAnd:
        case BytecodeOpcode::BitOr:
        case BytecodeOpcode::BitXor:
        case BytecodeOpcode::BitNot:
        case BytecodeOpcode::ShiftLeft:
        case BytecodeOpcode::ShiftRight:
        case BytecodeOpcode::ShiftRightUnsigned:
        case BytecodeOpcode::Equal:
        case BytecodeOpcode::NotEqual:
        case BytecodeOpcode::StrictEqual:
        case BytecodeOpcode::StrictNotEqual:
        case BytecodeOpcode::LessThan:
        case BytecodeOpcode::LessEqual:
        case BytecodeOpcode::GreaterThan:
        case BytecodeOpcode::GreaterEqual:
        case BytecodeOpcode::In:
        case BytecodeOpcode::InstanceOf:
        case BytecodeOpcode::LogicalAnd:
        case BytecodeOpcode::LogicalOr:
        case BytecodeOpcode::LogicalNot:
        case BytecodeOpcode::Return:
        case BytecodeOpcode::Throw:
        case BytecodeOpcode::TypeOf:
        case BytecodeOpcode::Debugger:
            return 0;

        case BytecodeOpcode::LoadConst:
        case BytecodeOpcode::LoadVar:
        case BytecodeOpcode::StoreVar:
        case BytecodeOpcode::LoadGlobal:
        case BytecodeOpcode::StoreGlobal:
        case BytecodeOpcode::LoadClosureVar:
        case BytecodeOpcode::StoreClosureVar:
        case BytecodeOpcode::Jump:
        case BytecodeOpcode::JumpIfTrue:
        case BytecodeOpcode::JumpIfFalse:
        case BytecodeOpcode::CreateObject:
        case BytecodeOpcode::CreateArray:
        case BytecodeOpcode::DeleteProperty:
        case BytecodeOpcode::HasProperty:
            return 1;

        case BytecodeOpcode::Call:
        case BytecodeOpcode::GetProperty:
        case BytecodeOpcode::SetProperty:
            return 2;

        case BytecodeOpcode::CallMethod:
            return 3;

        default:
            return 0;
    }
}

// 定数の型
enum class ConstantType : uint8_t {
    Undefined = 0,
    Null,
    Boolean,
    Number,
    String,
    RegExp
};

// 基底定数クラス
class Constant {
public:
    virtual ~Constant() = default;
    virtual ConstantType GetType() const = 0;
};

// Undefined定数
class UndefinedConstant : public Constant {
public:
    ConstantType GetType() const override { return ConstantType::Undefined; }
};

// Null定数
class NullConstant : public Constant {
public:
    ConstantType GetType() const override { return ConstantType::Null; }
};

// 真偽値定数
class BooleanConstant : public Constant {
public:
    explicit BooleanConstant(bool value) : m_value(value) {}
    
    ConstantType GetType() const override { return ConstantType::Boolean; }
    bool GetValue() const { return m_value; }
    
private:
    bool m_value;
};

// 数値定数
class NumberConstant : public Constant {
public:
    explicit NumberConstant(double value) : m_value(value) {}
    
    ConstantType GetType() const override { return ConstantType::Number; }
    double GetValue() const { return m_value; }
    
private:
    double m_value;
};

// 文字列定数
class StringConstant : public Constant {
public:
    explicit StringConstant(std::string value) : m_value(std::move(value)) {}
    
    ConstantType GetType() const override { return ConstantType::String; }
    const std::string& GetValue() const { return m_value; }
    
private:
    std::string m_value;
};

// 正規表現定数
class RegExpConstant : public Constant {
public:
    RegExpConstant(std::string pattern, std::string flags)
        : m_pattern(std::move(pattern)), m_flags(std::move(flags)) {}
    
    ConstantType GetType() const override { return ConstantType::RegExp; }
    const std::string& GetPattern() const { return m_pattern; }
    const std::string& GetFlags() const { return m_flags; }
    
private:
    std::string m_pattern;
    std::string m_flags;
};

// バイトコード命令クラス
class BytecodeInstruction {
public:
    BytecodeInstruction(BytecodeOpcode opcode, std::vector<uint32_t> operands,
                       uint32_t offset, uint32_t line, uint32_t column)
        : m_opcode(opcode), m_operands(std::move(operands)),
          m_offset(offset), m_line(line), m_column(column) {}
    
    BytecodeOpcode GetOpcode() const { return m_opcode; }
    const std::vector<uint32_t>& GetOperands() const { return m_operands; }
    
    uint32_t GetOffset() const { return m_offset; }
    uint32_t GetLine() const { return m_line; }
    uint32_t GetColumn() const { return m_column; }
    
    // オペランド取得ヘルパー
    uint32_t GetOperand(size_t index) const {
        if (index >= m_operands.size()) {
            return 0;
        }
        return m_operands[index];
    }
    
private:
    BytecodeOpcode m_opcode;
    std::vector<uint32_t> m_operands;
    uint32_t m_offset;
    uint32_t m_line;
    uint32_t m_column;
};

// 例外ハンドラの種類
enum class HandlerType : uint8_t {
    Catch = 0,
    Finally,
    CatchFinally
};

// 例外ハンドラクラス
class ExceptionHandler {
public:
    ExceptionHandler(HandlerType type, uint32_t tryStartOffset, uint32_t tryEndOffset,
                    uint32_t handlerOffset, uint32_t handlerEndOffset, uint32_t finallyOffset)
        : m_type(type), m_tryStartOffset(tryStartOffset), m_tryEndOffset(tryEndOffset),
          m_handlerOffset(handlerOffset), m_handlerEndOffset(handlerEndOffset),
          m_finallyOffset(finallyOffset) {}
    
    HandlerType GetType() const { return m_type; }
    uint32_t GetTryStartOffset() const { return m_tryStartOffset; }
    uint32_t GetTryEndOffset() const { return m_tryEndOffset; }
    uint32_t GetHandlerOffset() const { return m_handlerOffset; }
    uint32_t GetHandlerEndOffset() const { return m_handlerEndOffset; }
    uint32_t GetFinallyOffset() const { return m_finallyOffset; }
    
private:
    HandlerType m_type;
    uint32_t m_tryStartOffset;
    uint32_t m_tryEndOffset;
    uint32_t m_handlerOffset;
    uint32_t m_handlerEndOffset;
    uint32_t m_finallyOffset;
};

// バイトコード関数クラス
class BytecodeFunction {
public:
    BytecodeFunction(std::string name, uint32_t paramCount)
        : m_name(std::move(name)), m_paramCount(paramCount) {}

    const std::string& GetName() const { return m_name; }
    uint32_t GetParamCount() const { return m_paramCount; }
    
    // 定数プール操作
    void AddConstant(std::unique_ptr<Constant> constant) {
        m_constants.push_back(std::move(constant));
    }
    
    const Constant* GetConstant(uint32_t index) const {
        if (index >= m_constants.size()) {
            return nullptr;
        }
        return m_constants[index].get();
    }
    
    size_t GetConstantCount() const { return m_constants.size(); }
    
    // 命令操作
    void AddInstruction(const BytecodeInstruction& instruction) {
        m_instructions.push_back(instruction);
    }
    
    const BytecodeInstruction* GetInstruction(uint32_t index) const {
        if (index >= m_instructions.size()) {
            return nullptr;
        }
        return &m_instructions[index];
    }
    
    size_t GetInstructionCount() const { return m_instructions.size(); }
    
    const std::vector<BytecodeInstruction>& GetInstructions() const {
        return m_instructions;
    }
    
    // 例外ハンドラ操作
    void AddExceptionHandler(const ExceptionHandler& handler) {
        m_exceptionHandlers.push_back(handler);
    }
    
    const ExceptionHandler* GetExceptionHandlerForOffset(uint32_t offset) const {
        for (const auto& handler : m_exceptionHandlers) {
            if (offset >= handler.GetTryStartOffset() && offset < handler.GetTryEndOffset()) {
                return &handler;
            }
        }
        return nullptr;
    }
    
    const std::vector<ExceptionHandler>& GetExceptionHandlers() const {
        return m_exceptionHandlers;
    }
    
private:
    std::string m_name;
    uint32_t m_paramCount;
    std::vector<std::unique_ptr<Constant>> m_constants;
    std::vector<BytecodeInstruction> m_instructions;
    std::vector<ExceptionHandler> m_exceptionHandlers;
};

// バイトコードモジュールクラス
class BytecodeModule {
public:
    explicit BytecodeModule(std::string filename)
        : m_filename(std::move(filename)) {}
    
    const std::string& GetFilename() const { return m_filename; }
    
    // 関数操作
    void AddFunction(std::unique_ptr<BytecodeFunction> function) {
        std::string name = function->GetName();
        m_functions.push_back(std::move(function));
        m_functionMap[name] = m_functions.size() - 1;
    }
    
    const BytecodeFunction* GetFunction(uint32_t index) const {
        if (index >= m_functions.size()) {
            return nullptr;
        }
        return m_functions[index].get();
    }
    
    const BytecodeFunction* GetFunction(const std::string& name) const {
        auto it = m_functionMap.find(name);
        if (it == m_functionMap.end()) {
            return nullptr;
        }
        return m_functions[it->second].get();
    }
    
    size_t GetFunctionCount() const { return m_functions.size(); }
    
    const std::vector<std::unique_ptr<BytecodeFunction>>& GetFunctions() const {
        return m_functions;
    }
    
private:
    std::string m_filename;
    std::vector<std::unique_ptr<BytecodeFunction>> m_functions;
    std::unordered_map<std::string, size_t> m_functionMap;
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_BYTECODE_BYTECODE_H 