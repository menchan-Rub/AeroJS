/**
 * @file bytecode_instruction.cpp
 * @brief バイトコード命令の実装
 * 
 * このファイルはバイトコード命令の実装を提供します。
 * 特にオペコード名の文字列表現やデバッグ情報の生成などを実装します。
 */

#include "bytecode_instruction.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <unordered_map>

namespace aerojs {
namespace core {

namespace {
  // オペコード名のマッピング
  const std::unordered_map<Opcode, std::string> kOpcodeNames = {
    // スタック操作
    {Opcode::kPush, "PUSH"},
    {Opcode::kPop, "POP"},
    {Opcode::kDuplicate, "DUP"},
    {Opcode::kSwap, "SWAP"},

    // 算術演算
    {Opcode::kAdd, "ADD"},
    {Opcode::kSub, "SUB"},
    {Opcode::kMul, "MUL"},
    {Opcode::kDiv, "DIV"},
    {Opcode::kMod, "MOD"},
    {Opcode::kPow, "POW"},
    {Opcode::kNeg, "NEG"},
    {Opcode::kInc, "INC"},
    {Opcode::kDec, "DEC"},

    // ビット演算
    {Opcode::kBitAnd, "BIT_AND"},
    {Opcode::kBitOr, "BIT_OR"},
    {Opcode::kBitXor, "BIT_XOR"},
    {Opcode::kBitNot, "BIT_NOT"},
    {Opcode::kLeftShift, "LEFT_SHIFT"},
    {Opcode::kRightShift, "RIGHT_SHIFT"},
    {Opcode::kUnsignedRightShift, "UNSIGNED_RIGHT_SHIFT"},

    // 論理演算
    {Opcode::kLogicalAnd, "LOGICAL_AND"},
    {Opcode::kLogicalOr, "LOGICAL_OR"},
    {Opcode::kLogicalNot, "LOGICAL_NOT"},

    // 比較演算
    {Opcode::kEqual, "EQUAL"},
    {Opcode::kStrictEqual, "STRICT_EQUAL"},
    {Opcode::kNotEqual, "NOT_EQUAL"},
    {Opcode::kStrictNotEqual, "STRICT_NOT_EQUAL"},
    {Opcode::kLessThan, "LESS_THAN"},
    {Opcode::kLessThanOrEqual, "LESS_THAN_OR_EQUAL"},
    {Opcode::kGreaterThan, "GREATER_THAN"},
    {Opcode::kGreaterThanOrEqual, "GREATER_THAN_OR_EQUAL"},
    {Opcode::kInstanceOf, "INSTANCE_OF"},
    {Opcode::kIn, "IN"},

    // 制御フロー
    {Opcode::kJump, "JUMP"},
    {Opcode::kJumpIfTrue, "JUMP_IF_TRUE"},
    {Opcode::kJumpIfFalse, "JUMP_IF_FALSE"},
    {Opcode::kCall, "CALL"},
    {Opcode::kReturn, "RETURN"},
    {Opcode::kThrow, "THROW"},
    {Opcode::kEnterTry, "ENTER_TRY"},
    {Opcode::kLeaveTry, "LEAVE_TRY"},
    {Opcode::kEnterCatch, "ENTER_CATCH"},
    {Opcode::kLeaveCatch, "LEAVE_CATCH"},
    {Opcode::kEnterFinally, "ENTER_FINALLY"},
    {Opcode::kLeaveFinally, "LEAVE_FINALLY"},

    // 変数操作
    {Opcode::kGetLocal, "GET_LOCAL"},
    {Opcode::kSetLocal, "SET_LOCAL"},
    {Opcode::kGetGlobal, "GET_GLOBAL"},
    {Opcode::kSetGlobal, "SET_GLOBAL"},
    {Opcode::kGetUpvalue, "GET_UPVALUE"},
    {Opcode::kSetUpvalue, "SET_UPVALUE"},
    {Opcode::kDeclareVar, "DECLARE_VAR"},
    {Opcode::kDeclareConst, "DECLARE_CONST"},
    {Opcode::kDeclareLet, "DECLARE_LET"},

    // オブジェクト操作
    {Opcode::kNewObject, "NEW_OBJECT"},
    {Opcode::kNewArray, "NEW_ARRAY"},
    {Opcode::kGetProperty, "GET_PROPERTY"},
    {Opcode::kSetProperty, "SET_PROPERTY"},
    {Opcode::kDeleteProperty, "DELETE_PROPERTY"},
    {Opcode::kGetElement, "GET_ELEMENT"},
    {Opcode::kSetElement, "SET_ELEMENT"},
    {Opcode::kDeleteElement, "DELETE_ELEMENT"},
    {Opcode::kNewFunction, "NEW_FUNCTION"},
    {Opcode::kNewClass, "NEW_CLASS"},
    {Opcode::kGetSuperProperty, "GET_SUPER_PROPERTY"},
    {Opcode::kSetSuperProperty, "SET_SUPER_PROPERTY"},

    // イテレータ操作
    {Opcode::kIteratorInit, "ITERATOR_INIT"},
    {Opcode::kIteratorNext, "ITERATOR_NEXT"},
    {Opcode::kIteratorClose, "ITERATOR_CLOSE"},

    // 非同期操作
    {Opcode::kAwait, "AWAIT"},
    {Opcode::kYield, "YIELD"},
    {Opcode::kYieldStar, "YIELD_STAR"},

    // その他
    {Opcode::kNop, "NOP"},
    {Opcode::kDebugger, "DEBUGGER"},
    {Opcode::kTypeOf, "TYPE_OF"},
    {Opcode::kVoid, "VOID"},
    {Opcode::kDelete, "DELETE"},
    {Opcode::kImport, "IMPORT"},
    {Opcode::kExport, "EXPORT"}
  };

  /**
   * @brief オペコードの名前を取得
   * 
   * @param opcode 取得するオペコード
   * @return std::string オペコードの文字列表現
   */
  std::string getOpcodeName(Opcode opcode) {
    auto it = kOpcodeNames.find(opcode);
    if (it != kOpcodeNames.end()) {
      return it->second;
    }
    return "UNKNOWN(" + std::to_string(static_cast<int>(opcode)) + ")";
  }
}

/**
 * @brief 命令の文字列表現を取得
 * 
 * @return std::string 命令の文字列表現
 */
std::string BytecodeInstruction::toString() const {
  std::ostringstream oss;
  
  // オペコード名を取得
  Opcode opcode = getOpcode();
  std::string opcodeName = getOpcodeName(opcode);
  
  // オペコード名を出力
  oss << std::left << std::setw(25) << opcodeName;
  
  // オペランドがあれば出力
  if (m_operandCount > 0) {
    oss << " ";
    for (size_t i = 0; i < m_operandCount; ++i) {
      if (i > 0) {
        oss << ", ";
      }
      oss << m_operands[i];
    }
  }
  
  // ソース位置情報を出力（デバッグ情報）
  if (m_sourceLine > 0) {
    oss << " // Line " << m_sourceLine;
    if (m_sourceColumn > 0) {
      oss << ", Column " << m_sourceColumn;
    }
  }
  
  return oss.str();
}

} // namespace core
} // namespace aerojs 