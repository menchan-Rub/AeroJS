/**
 * @file bytecode.cpp
 * @brief JavaScript バイトコード実装
 * @version 0.1.0
 * @license MIT
 */

#include "bytecode.h"
#include <stdexcept>

namespace aerojs {
namespace core {

// Property デストラクタ
Property::~Property() {
  // 値の参照を解放
  if (value) {
    value->unref();
  }
  
  // ゲッター・セッター関数の参照を解放
  if (getter) {
    getter->unref();
  }
  if (setter) {
    setter->unref();
  }
}

// UndefinedConstant の実装
ConstantType UndefinedConstant::GetType() const {
  return ConstantType::Undefined;
}

// NullConstant の実装
ConstantType NullConstant::GetType() const {
  return ConstantType::Null;
}

// BooleanConstant の実装
BooleanConstant::BooleanConstant(bool value) : m_value(value) {
}

ConstantType BooleanConstant::GetType() const {
  return ConstantType::Boolean;
}

bool BooleanConstant::GetValue() const {
  return m_value;
}

// NumberConstant の実装
NumberConstant::NumberConstant(double value) : m_value(value) {
}

ConstantType NumberConstant::GetType() const {
  return ConstantType::Number;
}

double NumberConstant::GetValue() const {
  return m_value;
}

// StringConstant の実装
StringConstant::StringConstant(const std::string& value) : m_value(value) {
}

ConstantType StringConstant::GetType() const {
  return ConstantType::String;
}

const std::string& StringConstant::GetValue() const {
  return m_value;
}

// RegExpConstant の実装
RegExpConstant::RegExpConstant(const std::string& pattern, const std::string& flags)
    : m_pattern(pattern), m_flags(flags) {
}

ConstantType RegExpConstant::GetType() const {
  return ConstantType::RegExp;
}

const std::string& RegExpConstant::GetPattern() const {
  return m_pattern;
}

const std::string& RegExpConstant::GetFlags() const {
  return m_flags;
}

// BytecodeInstruction の実装
BytecodeInstruction::BytecodeInstruction(BytecodeOpcode opcode, 
                                       const std::vector<uint32_t>& operands,
                                       uint32_t offset, uint32_t line, uint32_t column)
    : m_opcode(opcode), m_operands(operands), m_offset(offset), 
      m_line(line), m_column(column) {
}

BytecodeOpcode BytecodeInstruction::GetOpcode() const {
  return m_opcode;
}

const std::vector<uint32_t>& BytecodeInstruction::GetOperands() const {
  return m_operands;
}

uint32_t BytecodeInstruction::GetOffset() const {
  return m_offset;
}

uint32_t BytecodeInstruction::GetLine() const {
  return m_line;
}

uint32_t BytecodeInstruction::GetColumn() const {
  return m_column;
}

uint32_t BytecodeInstruction::GetOperand(size_t index) const {
  if (index >= m_operands.size()) {
    throw std::out_of_range("Operand index out of range");
  }
  return m_operands[index];
}

// ExceptionHandler の実装
ExceptionHandler::ExceptionHandler(HandlerType type, 
                                 uint32_t tryStartOffset, uint32_t tryEndOffset,
                                 uint32_t handlerOffset, uint32_t handlerEndOffset,
                                 uint32_t finallyOffset)
    : m_type(type), m_tryStartOffset(tryStartOffset), m_tryEndOffset(tryEndOffset),
      m_handlerOffset(handlerOffset), m_handlerEndOffset(handlerEndOffset),
      m_finallyOffset(finallyOffset) {
}

HandlerType ExceptionHandler::GetType() const {
  return m_type;
}

uint32_t ExceptionHandler::GetTryStartOffset() const {
  return m_tryStartOffset;
}

uint32_t ExceptionHandler::GetTryEndOffset() const {
  return m_tryEndOffset;
}

uint32_t ExceptionHandler::GetHandlerOffset() const {
  return m_handlerOffset;
}

uint32_t ExceptionHandler::GetHandlerEndOffset() const {
  return m_handlerEndOffset;
}

uint32_t ExceptionHandler::GetFinallyOffset() const {
  return m_finallyOffset;
}

// BytecodeFunction の実装
BytecodeFunction::BytecodeFunction(const std::string& name, uint32_t paramCount)
    : m_name(name), m_paramCount(paramCount) {
}

const std::string& BytecodeFunction::GetName() const {
  return m_name;
}

uint32_t BytecodeFunction::GetParamCount() const {
  return m_paramCount;
}

void BytecodeFunction::AddConstant(std::unique_ptr<Constant> constant) {
  m_constants.push_back(std::move(constant));
}

const Constant* BytecodeFunction::GetConstant(uint32_t index) const {
  if (index >= m_constants.size()) {
    return nullptr;
  }
  return m_constants[index].get();
}

size_t BytecodeFunction::GetConstantCount() const {
  return m_constants.size();
}

void BytecodeFunction::AddInstruction(const BytecodeInstruction& instruction) {
  m_instructions.push_back(instruction);
}

const BytecodeInstruction* BytecodeFunction::GetInstruction(uint32_t index) const {
  if (index >= m_instructions.size()) {
    return nullptr;
  }
  return &m_instructions[index];
}

size_t BytecodeFunction::GetInstructionCount() const {
  return m_instructions.size();
}

const std::vector<BytecodeInstruction>& BytecodeFunction::GetInstructions() const {
  return m_instructions;
}

void BytecodeFunction::AddExceptionHandler(const ExceptionHandler& handler) {
  m_exceptionHandlers.push_back(handler);
}

const ExceptionHandler* BytecodeFunction::GetExceptionHandlerForOffset(uint32_t offset) const {
  for (const auto& handler : m_exceptionHandlers) {
    if (offset >= handler.GetTryStartOffset() && offset < handler.GetTryEndOffset()) {
      return &handler;
    }
  }
  return nullptr;
}

const std::vector<ExceptionHandler>& BytecodeFunction::GetExceptionHandlers() const {
  return m_exceptionHandlers;
}

// BytecodeModule の実装
BytecodeModule::BytecodeModule(const std::string& filename) : m_filename(filename) {
}

const std::string& BytecodeModule::GetFilename() const {
  return m_filename;
}

void BytecodeModule::AddFunction(std::unique_ptr<BytecodeFunction> function) {
  m_functionNameMap[function->GetName()] = m_functions.size();
  m_functions.push_back(std::move(function));
}

const BytecodeFunction* BytecodeModule::GetFunction(uint32_t index) const {
  if (index >= m_functions.size()) {
    return nullptr;
  }
  return m_functions[index].get();
}

const BytecodeFunction* BytecodeModule::GetFunction(const std::string& name) const {
  auto it = m_functionNameMap.find(name);
  if (it == m_functionNameMap.end()) {
    return nullptr;
  }
  return m_functions[it->second].get();
}

size_t BytecodeModule::GetFunctionCount() const {
  return m_functions.size();
}

const std::vector<std::unique_ptr<BytecodeFunction>>& BytecodeModule::GetFunctions() const {
  return m_functions;
}

// ヘルパー関数の実装

std::string BytecodeOpcodeToString(BytecodeOpcode opcode) {
  switch (opcode) {
    case BytecodeOpcode::Nop: return "Nop";
    case BytecodeOpcode::Pop: return "Pop";
    case BytecodeOpcode::Dup: return "Dup";
    case BytecodeOpcode::Swap: return "Swap";
    
    case BytecodeOpcode::LoadUndefined: return "LoadUndefined";
    case BytecodeOpcode::LoadNull: return "LoadNull";
    case BytecodeOpcode::LoadTrue: return "LoadTrue";
    case BytecodeOpcode::LoadFalse: return "LoadFalse";
    case BytecodeOpcode::LoadZero: return "LoadZero";
    case BytecodeOpcode::LoadOne: return "LoadOne";
    case BytecodeOpcode::LoadConst: return "LoadConst";
    
    case BytecodeOpcode::LoadVar: return "LoadVar";
    case BytecodeOpcode::StoreVar: return "StoreVar";
    case BytecodeOpcode::LoadGlobal: return "LoadGlobal";
    case BytecodeOpcode::StoreGlobal: return "StoreGlobal";
    case BytecodeOpcode::LoadThis: return "LoadThis";
    case BytecodeOpcode::StoreThis: return "StoreThis";
    case BytecodeOpcode::LoadClosureVar: return "LoadClosureVar";
    case BytecodeOpcode::StoreClosureVar: return "StoreClosureVar";
    
    case BytecodeOpcode::Add: return "Add";
    case BytecodeOpcode::Sub: return "Sub";
    case BytecodeOpcode::Mul: return "Mul";
    case BytecodeOpcode::Div: return "Div";
    case BytecodeOpcode::Mod: return "Mod";
    case BytecodeOpcode::Pow: return "Pow";
    case BytecodeOpcode::Inc: return "Inc";
    case BytecodeOpcode::Dec: return "Dec";
    case BytecodeOpcode::Neg: return "Neg";
    case BytecodeOpcode::BitAnd: return "BitAnd";
    case BytecodeOpcode::BitOr: return "BitOr";
    case BytecodeOpcode::BitXor: return "BitXor";
    case BytecodeOpcode::BitNot: return "BitNot";
    case BytecodeOpcode::ShiftLeft: return "ShiftLeft";
    case BytecodeOpcode::ShiftRight: return "ShiftRight";
    case BytecodeOpcode::ShiftRightUnsigned: return "ShiftRightUnsigned";
    
    case BytecodeOpcode::Equal: return "Equal";
    case BytecodeOpcode::NotEqual: return "NotEqual";
    case BytecodeOpcode::StrictEqual: return "StrictEqual";
    case BytecodeOpcode::StrictNotEqual: return "StrictNotEqual";
    case BytecodeOpcode::LessThan: return "LessThan";
    case BytecodeOpcode::LessEqual: return "LessEqual";
    case BytecodeOpcode::GreaterThan: return "GreaterThan";
    case BytecodeOpcode::GreaterEqual: return "GreaterEqual";
    case BytecodeOpcode::In: return "In";
    case BytecodeOpcode::InstanceOf: return "InstanceOf";
    
    case BytecodeOpcode::LogicalAnd: return "LogicalAnd";
    case BytecodeOpcode::LogicalOr: return "LogicalOr";
    case BytecodeOpcode::LogicalNot: return "LogicalNot";
    
    case BytecodeOpcode::Jump: return "Jump";
    case BytecodeOpcode::JumpIfTrue: return "JumpIfTrue";
    case BytecodeOpcode::JumpIfFalse: return "JumpIfFalse";
    case BytecodeOpcode::Call: return "Call";
    case BytecodeOpcode::CallMethod: return "CallMethod";
    case BytecodeOpcode::Return: return "Return";
    case BytecodeOpcode::Throw: return "Throw";
    
    case BytecodeOpcode::CreateObject: return "CreateObject";
    case BytecodeOpcode::CreateArray: return "CreateArray";
    case BytecodeOpcode::GetProperty: return "GetProperty";
    case BytecodeOpcode::SetProperty: return "SetProperty";
    case BytecodeOpcode::DeleteProperty: return "DeleteProperty";
    case BytecodeOpcode::HasProperty: return "HasProperty";
    
    case BytecodeOpcode::TypeOf: return "TypeOf";
    case BytecodeOpcode::Debugger: return "Debugger";
    
    default: return "Unknown";
  }
}

bool IsJumpInstruction(BytecodeOpcode opcode) {
  switch (opcode) {
    case BytecodeOpcode::Jump:
    case BytecodeOpcode::JumpIfTrue:
    case BytecodeOpcode::JumpIfFalse:
      return true;
    default:
      return false;
  }
}

bool IsCallInstruction(BytecodeOpcode opcode) {
  switch (opcode) {
    case BytecodeOpcode::Call:
    case BytecodeOpcode::CallMethod:
      return true;
    default:
      return false;
  }
}

bool IsTerminalInstruction(BytecodeOpcode opcode) {
  switch (opcode) {
    case BytecodeOpcode::Return:
    case BytecodeOpcode::Throw:
      return true;
    default:
      return false;
  }
}

// バイトコードの逆アセンブリ
std::string DisassembleBytecode(const BytecodeFunction* function) {
  if (!function) {
    return "";
  }
  
  std::string result;
  result += "Function: " + function->GetName() + "\n";
  result += "Parameters: " + std::to_string(function->GetParamCount()) + "\n\n";
  
  // 定数プール
  if (function->GetConstantCount() > 0) {
    result += "Constants:\n";
    for (size_t i = 0; i < function->GetConstantCount(); i++) {
      const Constant* constant = function->GetConstant(static_cast<uint32_t>(i));
      result += "  [" + std::to_string(i) + "] ";
      
      switch (constant->GetType()) {
        case ConstantType::Undefined:
          result += "undefined";
          break;
        case ConstantType::Null:
          result += "null";
          break;
        case ConstantType::Boolean:
          result += static_cast<const BooleanConstant*>(constant)->GetValue() ? "true" : "false";
          break;
        case ConstantType::Number:
          result += std::to_string(static_cast<const NumberConstant*>(constant)->GetValue());
          break;
        case ConstantType::String:
          result += "\"" + static_cast<const StringConstant*>(constant)->GetValue() + "\"";
          break;
        case ConstantType::RegExp:
          result += "/" + static_cast<const RegExpConstant*>(constant)->GetPattern() + "/" +
                   static_cast<const RegExpConstant*>(constant)->GetFlags();
          break;
      }
      result += "\n";
    }
    result += "\n";
  }
  
  // 命令列
  result += "Instructions:\n";
  const auto& instructions = function->GetInstructions();
  for (size_t i = 0; i < instructions.size(); i++) {
    const BytecodeInstruction& instruction = instructions[i];
    
    result += std::to_string(instruction.GetOffset()) + ": ";
    result += BytecodeOpcodeToString(instruction.GetOpcode());
    
    const auto& operands = instruction.GetOperands();
    for (size_t j = 0; j < operands.size(); j++) {
      result += " " + std::to_string(operands[j]);
    }
    
    result += "\n";
  }
  
  // 例外ハンドラ
  if (!function->GetExceptionHandlers().empty()) {
    result += "\nException Handlers:\n";
    const auto& handlers = function->GetExceptionHandlers();
    for (size_t i = 0; i < handlers.size(); i++) {
      const ExceptionHandler& handler = handlers[i];
      result += "  Handler " + std::to_string(i) + ": ";
      result += "try(" + std::to_string(handler.GetTryStartOffset()) + "-" + 
               std::to_string(handler.GetTryEndOffset()) + ") ";
      result += "catch(" + std::to_string(handler.GetHandlerOffset()) + "-" + 
               std::to_string(handler.GetHandlerEndOffset()) + ")";
      
      if (handler.GetFinallyOffset() != 0) {
        result += " finally(" + std::to_string(handler.GetFinallyOffset()) + ")";
      }
      result += "\n";
    }
  }
  
  return result;
}

}  // namespace core
}  // namespace aerojs 