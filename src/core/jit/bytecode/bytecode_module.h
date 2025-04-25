#pragma once

#include "bytecode_instruction.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace aerojs {
namespace core {

// 定数プールの項目タイプ
enum class ConstantType {
    Null,
    Undefined,
    Boolean,
    Number,
    String,
    Function,
    Object
};

// 定数プール内の項目
class ConstantPoolItem {
public:
    ConstantPoolItem(ConstantType type);
    ConstantPoolItem(bool value);
    ConstantPoolItem(double value);
    ConstantPoolItem(const std::string& value);
    
    ConstantType GetType() const { return m_type; }
    bool GetBooleanValue() const;
    double GetNumberValue() const;
    const std::string& GetStringValue() const;
    
    std::string ToString() const;

private:
    ConstantType m_type;
    union {
        bool m_boolean_value;
        double m_number_value;
    };
    std::string m_string_value; // ユニオンに含めない（デストラクタがあるため）
};

// バイトコード関数
class BytecodeFunction {
public:
    BytecodeFunction(const std::string& name, uint32_t arg_count);
    
    const std::string& GetName() const { return m_name; }
    uint32_t GetArgumentCount() const { return m_arg_count; }
    
    // 命令の追加と取得
    void AddInstruction(const BytecodeInstruction& instruction);
    const BytecodeInstruction& GetInstruction(uint32_t index) const;
    uint32_t GetInstructionCount() const;
    
    // 定数プールの操作
    uint32_t AddConstant(const ConstantPoolItem& constant);
    const ConstantPoolItem& GetConstant(uint32_t index) const;
    uint32_t GetConstantCount() const;
    
    // 例外ハンドラの操作
    void AddExceptionHandler(const ExceptionHandler& handler);
    const ExceptionHandler& GetExceptionHandler(uint32_t index) const;
    uint32_t GetExceptionHandlerCount() const;
    
    // 関数内でのローカル変数の数
    void SetLocalCount(uint32_t count) { m_local_count = count; }
    uint32_t GetLocalCount() const { return m_local_count; }
    
    // デバッグ情報
    void SetSourceMap(const std::string& file, uint32_t line);
    std::string GetSourceFile() const { return m_source_file; }
    uint32_t GetSourceLine() const { return m_source_line; }
    
    std::string ToString() const;

private:
    std::string m_name;
    uint32_t m_arg_count;
    uint32_t m_local_count;
    std::vector<BytecodeInstruction> m_instructions;
    std::vector<ConstantPoolItem> m_constant_pool;
    std::vector<ExceptionHandler> m_exception_handlers;
    std::string m_source_file;
    uint32_t m_source_line;
};

// バイトコードモジュール
class BytecodeModule {
public:
    /**
     * @brief コンストラクタ
     * 
     * @param name モジュールの名前
     */
    BytecodeModule(const std::string& name);

    /**
     * @brief デストラクタ
     */
    ~BytecodeModule() = default;

    /**
     * @brief 関数をモジュールに追加
     * 
     * @param function 追加する関数
     * @return uint32_t 追加された関数のインデックス
     */
    uint32_t AddFunction(std::unique_ptr<BytecodeFunction> function);

    /**
     * @brief 関数の取得
     * 
     * @param index 関数のインデックス
     * @return BytecodeFunction* 関数へのポインタ、存在しない場合はnullptr
     */
    BytecodeFunction* GetFunction(uint32_t index);

    /**
     * @brief 関数の取得（const版）
     * 
     * @param index 関数のインデックス
     * @return const BytecodeFunction* 関数へのポインタ、存在しない場合はnullptr
     */
    const BytecodeFunction* GetFunction(uint32_t index) const;

    /**
     * @brief メイン関数の取得
     * 
     * @return BytecodeFunction* メイン関数へのポインタ、存在しない場合はnullptr
     */
    BytecodeFunction* GetMainFunction();

    /**
     * @brief メイン関数の取得（const版）
     * 
     * @return const BytecodeFunction* メイン関数へのポインタ、存在しない場合はnullptr
     */
    const BytecodeFunction* GetMainFunction() const;

    /**
     * @brief 関数の総数を取得
     * 
     * @return uint32_t 関数の数
     */
    uint32_t GetFunctionCount() const { return m_functions.size(); }

    /**
     * @brief 関数名からインデックスを取得
     * 
     * @param name 関数名
     * @return int32_t 見つかった関数のインデックス、存在しない場合は-1
     */
    int32_t GetFunctionIndex(const std::string& name) const;

    /**
     * @brief モジュールの名前を取得
     * 
     * @return const std::string& モジュールの名前
     */
    const std::string& GetName() const { return m_name; }

    /**
     * @brief モジュールの文字列表現を取得
     * 
     * @return std::string モジュールの文字列表現
     */
    std::string ToString() const;

private:
    std::string m_name;
    std::vector<std::unique_ptr<BytecodeFunction>> m_functions;
    std::unordered_map<std::string, uint32_t> m_function_map;
    uint32_t m_main_function_index;
};

} // namespace core
} // namespace aerojs 