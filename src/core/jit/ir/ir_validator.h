#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "src/core/jit/ir/ir.h"

namespace aerojs::core {

// IR検証の結果コード
enum class IRValidationResult {
    Valid,                  // 有効なIR
    InvalidInstruction,     // 無効な命令
    InvalidOperandCount,    // オペランド数が不正
    InvalidOperandType,     // オペランドの型が不正
    MissingOperand,         // 必要なオペランドが欠如
    InvalidControlFlow,     // 制御フローが不正
    DeadCode,               // 到達不能コード
    CriticalPathCycle,      // クリティカルパスに循環が存在
    TypeMismatch,           // 型の不一致
    UninitializedValue,     // 未初期化の値の使用
    InvalidMemoryAccess,    // 不正なメモリアクセス
    Other                   // その他のエラー
};

// 検証エラーを表す構造体
struct IRValidationError {
    IRValidationResult code;       // エラーコード
    uint32_t instructionId;        // エラーが発生した命令のID
    std::string message;           // エラーメッセージ
    
    IRValidationError(IRValidationResult code, uint32_t instructionId, const std::string& message)
        : code(code), instructionId(instructionId), message(message) {}
};

// IR検証器クラス
class IRValidator {
public:
    IRValidator();
    ~IRValidator();

    // IRを検証し、結果を返す
    bool validate(const IRFunction* function);
    
    // 検証エラーを取得
    const std::vector<IRValidationError>& errors() const { return m_errors; }
    
    // 最初のエラーメッセージを取得
    std::string firstErrorMessage() const;
    
    // 全てのエラーメッセージを取得
    std::string getAllErrorMessages() const;
    
    // エラーをクリア
    void clearErrors() { m_errors.clear(); }

private:
    // 様々な検証メソッド
    bool validateInstruction(const IRInstruction* instruction);
    bool validateOperands(const IRInstruction* instruction);
    bool validateControlFlow(const IRFunction* function);
    bool validateTypes(const IRFunction* function);
    bool validateDataFlow(const IRFunction* function);
    bool validateMemoryAccess(const IRFunction* function);
    
    // エラーを追加
    void addError(IRValidationResult code, uint32_t instructionId, const std::string& message);
    
    // 各命令タイプの検証
    bool validateArithmeticInstruction(const IRInstruction* instruction);
    bool validateComparisonInstruction(const IRInstruction* instruction);
    bool validateBranchInstruction(const IRInstruction* instruction);
    bool validateMemoryInstruction(const IRInstruction* instruction);
    bool validateCallInstruction(const IRInstruction* instruction);
    
    // 検証中に検出されたエラーのリスト
    std::vector<IRValidationError> m_errors;
    
    // 現在検証中の関数
    const IRFunction* m_currentFunction;
};

} // namespace aerojs::core