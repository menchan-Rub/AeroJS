/**
 * @file bytecode_module.cpp
 * @brief バイトコードモジュールの実装ファイル
 *
 * このファイルは、VMが実行するバイトコードモジュールの実装を提供します。
 * バイトコードモジュールは命令列、定数プール、関数情報、ソースマップなどを管理します。
 */

#include "bytecode_module.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace aerojs {
namespace core {

BytecodeModule::BytecodeModule(const BytecodeModuleMetadata& metadata)
    : m_metadata(metadata) {
  // 現在のタイムスタンプを設定
  if (m_metadata.timestamp == 0) {
    auto now = std::chrono::system_clock::now();
    m_metadata.timestamp = std::chrono::system_clock::to_time_t(now);
  }

  // 定数プールを初期化
  m_constantPool = std::make_shared<ConstantPool>();
}

BytecodeModule::~BytecodeModule() {
  // 特に何もしない、スマートポインタが自動的にメモリを解放
}

uint32_t BytecodeModule::addInstruction(const BytecodeInstruction& instruction) {
  uint32_t index = static_cast<uint32_t>(m_instructions.size());
  m_instructions.push_back(instruction);
  return index;
}

void BytecodeModule::setInstructions(const std::vector<BytecodeInstruction>& instructions) {
  m_instructions = instructions;
}

void BytecodeModule::setConstantPool(std::shared_ptr<ConstantPool> constant_pool) {
  m_constantPool = constant_pool;
}

uint32_t BytecodeModule::addFunctionInfo(const FunctionInfo& function_info) {
  uint32_t index = static_cast<uint32_t>(m_functionInfos.size());
  m_functionInfos.push_back(function_info);
  return index;
}

void BytecodeModule::addSourceMapEntry(const SourceMapEntry& entry) {
  m_sourceMap.push_back(entry);
}

uint32_t BytecodeModule::addString(const std::string& str) {
  // すでに存在する文字列かどうかをチェック
  for (size_t i = 0; i < m_stringTable.size(); ++i) {
    if (m_stringTable[i] == str) {
      return static_cast<uint32_t>(i);
    }
  }

  // 新しい文字列を追加
  uint32_t index = static_cast<uint32_t>(m_stringTable.size());
  m_stringTable.push_back(str);
  return index;
}

const BytecodeModuleMetadata& BytecodeModule::getMetadata() const {
  return m_metadata;
}

void BytecodeModule::setMetadata(const BytecodeModuleMetadata& metadata) {
  m_metadata = metadata;
}

const std::vector<BytecodeInstruction>& BytecodeModule::getInstructions() const {
  return m_instructions;
}

const BytecodeInstruction& BytecodeModule::getInstruction(uint32_t index) const {
  if (index >= m_instructions.size()) {
    throw std::out_of_range("Instruction index out of range");
  }
  return m_instructions[index];
}

size_t BytecodeModule::getInstructionCount() const {
  return m_instructions.size();
}

std::shared_ptr<ConstantPool> BytecodeModule::getConstantPool() const {
  return m_constantPool;
}

const std::vector<FunctionInfo>& BytecodeModule::getFunctionInfos() const {
  return m_functionInfos;
}

const FunctionInfo& BytecodeModule::getFunctionInfo(uint32_t index) const {
  if (index >= m_functionInfos.size()) {
    throw std::out_of_range("Function info index out of range");
  }
  return m_functionInfos[index];
}

const std::vector<SourceMapEntry>& BytecodeModule::getSourceMap() const {
  return m_sourceMap;
}

const std::vector<std::string>& BytecodeModule::getStringTable() const {
  return m_stringTable;
}

const std::string& BytecodeModule::getString(uint32_t index) const {
  if (index >= m_stringTable.size()) {
    throw std::out_of_range("String index out of range");
  }
  return m_stringTable[index];
}

std::vector<uint8_t> BytecodeModule::serialize() const {
  // 単純なバイナリ形式でシリアライズする
  // 実際の実装では、より効率的なフォーマットを使用するべき

  std::vector<uint8_t> data;

  // ここにシリアライズのロジックを実装...
  // メタデータ、命令列、定数プール、関数情報、ソースマップなどをシリアライズ

  return data;
}

std::unique_ptr<BytecodeModule> BytecodeModule::deserialize(const std::vector<uint8_t>& data) {
  // バイナリデータからモジュールを再構築する

  // ここでデシリアライズのロジックを実装...
  // データからメタデータ、命令列、定数プール、関数情報、ソースマップなどを復元

  return std::make_unique<BytecodeModule>();
}

void BytecodeModule::dump(std::ostream& output, bool verbose) const {
  // メタデータの出力
  output << "=== Bytecode Module ===" << std::endl;
  output << "Source: " << m_metadata.source_file << std::endl;
  output << "Module: " << (m_metadata.is_module ? "true" : "false") << std::endl;
  output << "Strict Mode: " << (m_metadata.strict_mode ? "true" : "false") << std::endl;

  // タイムスタンプを人間が読める形式に変換
  char time_buf[100];
  std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&m_metadata.timestamp));
  output << "Generated: " << time_buf << std::endl;

  output << std::endl;

  // 命令列の出力
  output << "=== Instructions (" << m_instructions.size() << ") ===" << std::endl;
  for (size_t i = 0; i < m_instructions.size(); ++i) {
    const auto& instr = m_instructions[i];
    output << std::setw(4) << i << ": " << instr.toString();

    // ソースマップ情報を付加（詳細モードの場合）
    if (verbose) {
      // 該当する命令のソースマップエントリを探す
      for (const auto& entry : m_sourceMap) {
        if (entry.bytecode_offset == i) {
          output << " // " << entry.source_file << ":" << entry.line << ":" << entry.column;
          break;
        }
      }
    }

    output << std::endl;
  }

  output << std::endl;

  // 定数プールの出力
  output << "=== Constant Pool (" << m_constantPool->size() << ") ===" << std::endl;
  m_constantPool->dump(output);

  output << std::endl;

  // 関数情報の出力
  output << "=== Functions (" << m_functionInfos.size() << ") ===" << std::endl;
  for (size_t i = 0; i < m_functionInfos.size(); ++i) {
    const auto& func = m_functionInfos[i];
    output << std::setw(4) << i << ": " << func.getSignature() << std::endl;

    if (verbose) {
      output << "     Code: " << func.code_offset << "-" << (func.code_offset + func.code_length - 1) << std::endl;
      output << "     Arity: " << func.arity << std::endl;
      output << "     Strict: " << (func.is_strict ? "true" : "false") << std::endl;
      output << "     Arrow: " << (func.is_arrow_function ? "true" : "false") << std::endl;
      output << "     Generator: " << (func.is_generator ? "true" : "false") << std::endl;
      output << "     Async: " << (func.is_async ? "true" : "false") << std::endl;

      output << "     Parameters: ";
      for (size_t j = 0; j < func.parameter_names.size(); ++j) {
        if (j > 0) output << ", ";
        output << func.parameter_names[j];
      }
      output << std::endl;

      output << "     Locals: ";
      for (size_t j = 0; j < func.local_names.size(); ++j) {
        if (j > 0) output << ", ";
        output << func.local_names[j];
      }
      output << std::endl;
    }
  }

  output << std::endl;

  // 文字列テーブルの出力（詳細モードの場合）
  if (verbose) {
    output << "=== String Table (" << m_stringTable.size() << ") ===" << std::endl;
    for (size_t i = 0; i < m_stringTable.size(); ++i) {
      output << std::setw(4) << i << ": \"" << m_stringTable[i] << "\"" << std::endl;
    }

    output << std::endl;
  }
}

}  // namespace core
}  // namespace aerojs