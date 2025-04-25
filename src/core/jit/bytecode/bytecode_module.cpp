#include "bytecode_module.h"
#include <sstream>
#include <stdexcept>

namespace aerojs {
namespace core {

// ConstantPoolItemの実装
ConstantPoolItem::ConstantPoolItem(ConstantType type) : m_type(type) {
    if (type == ConstantType::Boolean) {
        m_boolean_value = false;
    } else if (type == ConstantType::Number) {
        m_number_value = 0.0;
    }
}

ConstantPoolItem::ConstantPoolItem(bool value) : m_type(ConstantType::Boolean), m_boolean_value(value) {}

ConstantPoolItem::ConstantPoolItem(double value) : m_type(ConstantType::Number), m_number_value(value) {}

ConstantPoolItem::ConstantPoolItem(const std::string& value) : m_type(ConstantType::String), m_string_value(value) {}

bool ConstantPoolItem::GetBooleanValue() const {
    if (m_type != ConstantType::Boolean) {
        throw std::runtime_error("Constant is not a boolean");
    }
    return m_boolean_value;
}

double ConstantPoolItem::GetNumberValue() const {
    if (m_type != ConstantType::Number) {
        throw std::runtime_error("Constant is not a number");
    }
    return m_number_value;
}

const std::string& ConstantPoolItem::GetStringValue() const {
    if (m_type != ConstantType::String) {
        throw std::runtime_error("Constant is not a string");
    }
    return m_string_value;
}

std::string ConstantPoolItem::ToString() const {
    std::stringstream ss;
    switch (m_type) {
        case ConstantType::Null:
            ss << "null";
            break;
        case ConstantType::Undefined:
            ss << "undefined";
            break;
        case ConstantType::Boolean:
            ss << (m_boolean_value ? "true" : "false");
            break;
        case ConstantType::Number:
            ss << m_number_value;
            break;
        case ConstantType::String:
            ss << "\"" << m_string_value << "\"";
            break;
        case ConstantType::Function:
            ss << "function";
            break;
        case ConstantType::Object:
            ss << "object";
            break;
        default:
            ss << "unknown";
            break;
    }
    return ss.str();
}

// BytecodeFunctionの実装
BytecodeFunction::BytecodeFunction(const std::string& name, uint32_t arg_count)
    : m_name(name), m_arg_count(arg_count), m_local_count(0), m_source_line(0) {}

void BytecodeFunction::AddInstruction(const BytecodeInstruction& instruction) {
    m_instructions.push_back(instruction);
}

const BytecodeInstruction& BytecodeFunction::GetInstruction(uint32_t index) const {
    if (index >= m_instructions.size()) {
        throw std::out_of_range("Instruction index out of range");
    }
    return m_instructions[index];
}

uint32_t BytecodeFunction::GetInstructionCount() const {
    return static_cast<uint32_t>(m_instructions.size());
}

uint32_t BytecodeFunction::AddConstant(const ConstantPoolItem& constant) {
    m_constant_pool.push_back(constant);
    return static_cast<uint32_t>(m_constant_pool.size() - 1);
}

const ConstantPoolItem& BytecodeFunction::GetConstant(uint32_t index) const {
    if (index >= m_constant_pool.size()) {
        throw std::out_of_range("Constant index out of range");
    }
    return m_constant_pool[index];
}

uint32_t BytecodeFunction::GetConstantCount() const {
    return static_cast<uint32_t>(m_constant_pool.size());
}

void BytecodeFunction::AddExceptionHandler(const ExceptionHandler& handler) {
    m_exception_handlers.push_back(handler);
}

const ExceptionHandler& BytecodeFunction::GetExceptionHandler(uint32_t index) const {
    if (index >= m_exception_handlers.size()) {
        throw std::out_of_range("Exception handler index out of range");
    }
    return m_exception_handlers[index];
}

uint32_t BytecodeFunction::GetExceptionHandlerCount() const {
    return static_cast<uint32_t>(m_exception_handlers.size());
}

void BytecodeFunction::SetSourceMap(const std::string& file, uint32_t line) {
    m_source_file = file;
    m_source_line = line;
}

std::string BytecodeFunction::ToString() const {
    std::stringstream ss;
    ss << "Function: " << m_name << ", Args: " << m_arg_count << ", Locals: " << m_local_count << "\n";
    
    // 定数プールの情報
    ss << "Constants (" << m_constant_pool.size() << "):\n";
    for (size_t i = 0; i < m_constant_pool.size(); ++i) {
        ss << "  [" << i << "] " << m_constant_pool[i].ToString() << "\n";
    }
    
    // 命令の情報
    ss << "Instructions (" << m_instructions.size() << "):\n";
    for (size_t i = 0; i < m_instructions.size(); ++i) {
        ss << "  " << i << ": " << m_instructions[i].ToString() << "\n";
    }
    
    // 例外ハンドラの情報
    if (!m_exception_handlers.empty()) {
        ss << "Exception Handlers (" << m_exception_handlers.size() << "):\n";
        for (size_t i = 0; i < m_exception_handlers.size(); ++i) {
            auto& handler = m_exception_handlers[i];
            ss << "  [" << i << "] Try: " 
               << handler.GetTryStart() << "-" << handler.GetTryEnd()
               << ", Handler: " << handler.GetHandlerOffset() << "\n";
        }
    }
    
    return ss.str();
}

// BytecodeModuleの実装
BytecodeModule::BytecodeModule(const std::string& name)
    : m_name(name), m_main_function_index(0) {}

uint32_t BytecodeModule::AddFunction(std::unique_ptr<BytecodeFunction> function) {
    const std::string& function_name = function->GetName();
    
    // 関数名の重複チェック
    auto it = m_function_map.find(function_name);
    if (it != m_function_map.end()) {
        throw std::runtime_error("Function with name '" + function_name + "' already exists");
    }
    
    uint32_t index = static_cast<uint32_t>(m_functions.size());
    m_function_map[function_name] = index;
    m_functions.push_back(std::move(function));
    return index;
}

BytecodeFunction* BytecodeModule::GetFunction(uint32_t index) {
    if (index >= m_functions.size()) {
        return nullptr;
    }
    return m_functions[index].get();
}

const BytecodeFunction* BytecodeModule::GetFunction(uint32_t index) const {
    if (index >= m_functions.size()) {
        return nullptr;
    }
    return m_functions[index].get();
}

BytecodeFunction* BytecodeModule::GetMainFunction() {
    return GetFunction(m_main_function_index);
}

const BytecodeFunction* BytecodeModule::GetMainFunction() const {
    return GetFunction(m_main_function_index);
}

int32_t BytecodeModule::GetFunctionIndex(const std::string& name) const {
    auto it = m_function_map.find(name);
    if (it == m_function_map.end()) {
        return -1;
    }
    return static_cast<int32_t>(it->second);
}

std::string BytecodeModule::ToString() const {
    std::stringstream ss;
    ss << "Module: " << m_name << "\n";
    ss << "Functions (" << m_functions.size() << "):\n";
    
    for (size_t i = 0; i < m_functions.size(); ++i) {
        ss << "--- Function " << i << " ---\n";
        ss << m_functions[i]->ToString();
        ss << "\n";
    }
    
    return ss.str();
}

} // namespace core
} // namespace aerojs 