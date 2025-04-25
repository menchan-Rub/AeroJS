#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <variant>

namespace aerojs {
namespace core {

// 前方宣言
class BytecodeInstruction;
class ExceptionHandler;

/**
 * @brief 定数値を表す型
 * 
 * バイトコード内で使用される様々な型の定数値を格納するための型です。
 * 数値、文字列、ブール値などの基本型をサポートします。
 */
using ConstantValue = std::variant<
    std::nullptr_t,    // null
    bool,              // ブール値
    int32_t,           // 整数
    double,            // 浮動小数点
    std::string        // 文字列
>;

/**
 * @brief バイトコード関数クラス
 * 
 * コンパイルされたJavaScript関数を表します。
 * 関数名、引数情報、定数プール、命令列、例外ハンドラなどの情報を保持します。
 */
class BytecodeFunction {
public:
    /**
     * @brief コンストラクタ
     * 
     * @param name 関数名
     * @param arity 引数の数
     */
    BytecodeFunction(const std::string& name, uint32_t arity);
    
    /**
     * @brief デストラクタ
     */
    ~BytecodeFunction();
    
    /**
     * @brief 関数名の取得
     * 
     * @return const std::string& 関数名
     */
    const std::string& GetName() const { return m_name; }
    
    /**
     * @brief 引数の数を取得
     * 
     * @return uint32_t 引数の数
     */
    uint32_t GetArity() const { return m_arity; }
    
    /**
     * @brief 定数を追加
     * 
     * @param value 追加する定数値
     * @return uint32_t 追加された定数のインデックス
     */
    uint32_t AddConstant(const ConstantValue& value);
    
    /**
     * @brief 定数の取得
     * 
     * @param index 定数のインデックス
     * @return const ConstantValue& 定数値
     * @throws std::out_of_range インデックスが範囲外の場合
     */
    const ConstantValue& GetConstant(uint32_t index) const;
    
    /**
     * @brief 命令を追加
     * 
     * @param instruction 追加する命令
     */
    void AddInstruction(const BytecodeInstruction& instruction);
    
    /**
     * @brief 命令の取得
     * 
     * @param offset 命令のオフセット
     * @return const BytecodeInstruction& 命令
     * @throws std::out_of_range オフセットが範囲外の場合
     */
    const BytecodeInstruction& GetInstruction(uint32_t offset) const;
    
    /**
     * @brief 命令列の取得
     * 
     * @return const std::vector<BytecodeInstruction>& 命令のベクター
     */
    const std::vector<BytecodeInstruction>& GetInstructions() const { return m_instructions; }
    
    /**
     * @brief 例外ハンドラを追加
     * 
     * @param handler 追加する例外ハンドラ
     */
    void AddExceptionHandler(const ExceptionHandler& handler);
    
    /**
     * @brief 例外ハンドラの取得
     * 
     * @return const std::vector<ExceptionHandler>& 例外ハンドラのベクター
     */
    const std::vector<ExceptionHandler>& GetExceptionHandlers() const { return m_exception_handlers; }
    
    /**
     * @brief 関数の命令サイズ（バイト単位）を取得
     * 
     * @return uint32_t 命令のサイズ
     */
    uint32_t GetCodeSize() const;
    
    /**
     * @brief デバッグ情報の追加
     * 
     * @param offset バイトコードオフセット
     * @param line ソースコードの行番号
     * @param column ソースコードの列番号
     */
    void AddDebugInfo(uint32_t offset, uint32_t line, uint32_t column);
    
    /**
     * @brief オフセットに対応するソースコードの行を取得
     * 
     * @param offset バイトコードオフセット
     * @return uint32_t ソースコードの行番号、見つからない場合は0
     */
    uint32_t GetSourceLine(uint32_t offset) const;

private:
    // 関数名
    std::string m_name;
    
    // 引数の数
    uint32_t m_arity;
    
    // 定数プール
    std::vector<ConstantValue> m_constants;
    
    // 命令列
    std::vector<BytecodeInstruction> m_instructions;
    
    // 例外ハンドラ
    std::vector<ExceptionHandler> m_exception_handlers;
    
    // ソースマップ情報 (offset -> {line, column})
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> m_debug_info;
};

} // namespace core
} // namespace aerojs 