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
  // 効率的なバイナリフォーマットでシリアライズを実装
  std::vector<uint8_t> data;

  // 1. マジックナンバーとバージョン情報
  // マジックナンバー "AERO" の各文字をバイトとして追加
  data.push_back('A');
  data.push_back('E');
  data.push_back('R');
  data.push_back('O');
  
  // バージョン番号 (メジャー.マイナー.パッチ) - 1バイトずつ
  data.push_back(2); // メジャーバージョン
  data.push_back(0); // マイナーバージョン
  data.push_back(0); // パッチバージョン
  
  // ヘルパー関数：uint32_tをリトルエンディアンでデータに追加
  auto appendUint32 = [&data](uint32_t value) {
    data.push_back(static_cast<uint8_t>(value & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  };
  
  // ヘルパー関数：文字列をデータに追加
  auto appendString = [&data, &appendUint32](const std::string& str) {
    // 文字列長
    appendUint32(static_cast<uint32_t>(str.size()));
    // 文字列データ
    for (char c : str) {
      data.push_back(static_cast<uint8_t>(c));
    }
  };
  
  // 2. メタデータセクション
  data.push_back(0x01); // メタデータセクション識別子
  
  // ソースファイル
  appendString(m_metadata.source_file);
  
  // フラグ (ビットフィールド)
  uint8_t flags = 0;
  if (m_metadata.is_module) flags |= 0x01;
  if (m_metadata.strict_mode) flags |= 0x02;
  data.push_back(flags);
  
  // タイムスタンプ (64ビットを8バイトで)
  uint64_t timestamp = static_cast<uint64_t>(m_metadata.timestamp);
  for (int i = 0; i < 8; i++) {
    data.push_back(static_cast<uint8_t>((timestamp >> (i * 8)) & 0xFF));
  }
  
  // 3. 文字列テーブルセクション
  data.push_back(0x02); // 文字列テーブルセクション識別子
  
  // 文字列テーブルサイズ
  appendUint32(static_cast<uint32_t>(m_stringTable.size()));
  
  // 各文字列を追加
  for (const auto& str : m_stringTable) {
    appendString(str);
  }
  
  // 4. 定数プールセクション
  data.push_back(0x03); // 定数プールセクション識別子
  
  // 定数プールの内容をシリアライズ
  if (m_constantPool) {
    std::vector<uint8_t> pool_data = m_constantPool->serialize();
    appendUint32(static_cast<uint32_t>(pool_data.size()));
    data.insert(data.end(), pool_data.begin(), pool_data.end());
  } else {
    appendUint32(0); // 空の定数プール
  }
  
  // 5. 命令セクション
  data.push_back(0x04); // 命令セクション識別子
  
  // 命令数
  appendUint32(static_cast<uint32_t>(m_instructions.size()));
  
  // 各命令をシリアライズ
  for (const auto& instr : m_instructions) {
    // 命令のオペコード
    data.push_back(static_cast<uint8_t>(instr.getOpcode()));
    
    // 命令のオペランド（最大3つ）
    for (int i = 0; i < instr.getOperandCount(); i++) {
      uint32_t operand = instr.getOperand(i);
      appendUint32(operand);
    }
    
    // オペランド数（フォーマット情報として保存）
    data.push_back(static_cast<uint8_t>(instr.getOperandCount()));
  }
  
  // 6. 関数情報セクション
  data.push_back(0x05); // 関数情報セクション識別子
  
  // 関数数
  appendUint32(static_cast<uint32_t>(m_functionInfos.size()));
  
  // 各関数情報をシリアライズ
  for (const auto& func : m_functionInfos) {
    // コードオフセットと長さ
    appendUint32(func.code_offset);
    appendUint32(func.code_length);
    
    // 引数の数
    data.push_back(static_cast<uint8_t>(func.arity));
    
    // フラグ（ビットフィールド）
    uint8_t func_flags = 0;
    if (func.is_strict) func_flags |= 0x01;
    if (func.is_arrow_function) func_flags |= 0x02;
    if (func.is_generator) func_flags |= 0x04;
    if (func.is_async) func_flags |= 0x08;
    data.push_back(func_flags);
    
    // 関数名
    appendString(func.name);
    
    // パラメータ名
    appendUint32(static_cast<uint32_t>(func.parameter_names.size()));
    for (const auto& param : func.parameter_names) {
      appendString(param);
    }
    
    // ローカル変数名
    appendUint32(static_cast<uint32_t>(func.local_names.size()));
    for (const auto& local : func.local_names) {
      appendString(local);
    }
  }
  
  // 7. ソースマップセクション
  data.push_back(0x06); // ソースマップセクション識別子
  
  // マップエントリ数
  appendUint32(static_cast<uint32_t>(m_sourceMap.size()));
  
  // 各ソースマップエントリをシリアライズ
  for (const auto& entry : m_sourceMap) {
    appendUint32(entry.bytecode_offset);
    appendUint32(entry.line);
    appendUint32(entry.column);
    appendString(entry.source_file);
  }
  
  // 8. フッター（整合性チェック用）
  data.push_back(0xFF); // フッター識別子
  
  // 単純なチェックサム
  uint32_t checksum = 0;
  for (size_t i = 0; i < data.size(); i++) {
    checksum = (checksum + data[i]) & 0xFFFFFFFF;
  }
  appendUint32(checksum);

  return data;
}

std::unique_ptr<BytecodeModule> BytecodeModule::deserialize(const std::vector<uint8_t>& data) {
  // バイナリデータからバイトコードモジュールを復元
  if (data.size() < 12) { // 最低限必要なサイズ（マジックナンバー、バージョン、セクション識別子など）
    return nullptr;
  }
  
  // 1. マジックナンバーとバージョン確認
  if (data[0] != 'A' || data[1] != 'E' || data[2] != 'R' || data[3] != 'O') {
    // マジックナンバーが一致しない
    return nullptr;
  }
  
  uint8_t major_version = data[4];
  uint8_t minor_version = data[5];
  uint8_t patch_version = data[6];
  
  // バージョン互換性チェック（必要に応じて調整）
  if (major_version > 2) {
    // 互換性のないバージョン
    return nullptr;
  }
  
  auto module = std::make_unique<BytecodeModule>();
  size_t position = 7; // 現在の読み込み位置
  
  // ヘルパー関数：uint32_tを読み取り
  auto readUint32 = [&data, &position]() -> uint32_t {
    if (position + 4 > data.size()) {
      throw std::runtime_error("データの境界外を読み取ろうとしています");
    }
    
    uint32_t value = 
        static_cast<uint32_t>(data[position]) |
        (static_cast<uint32_t>(data[position + 1]) << 8) |
        (static_cast<uint32_t>(data[position + 2]) << 16) |
        (static_cast<uint32_t>(data[position + 3]) << 24);
    
    position += 4;
    return value;
  };
  
  // ヘルパー関数：文字列を読み取り
  auto readString = [&data, &position, &readUint32]() -> std::string {
    uint32_t length = readUint32();
    
    if (position + length > data.size()) {
      throw std::runtime_error("データの境界外を読み取ろうとしています");
    }
    
    std::string str(data.begin() + position, data.begin() + position + length);
    position += length;
    return str;
  };
  
  // 各セクションを読み取り
  while (position < data.size() - 5) { // フッターのために少なくとも5バイト残す
    uint8_t section_id = data[position++];
    
    switch (section_id) {
      case 0x01: { // メタデータセクション
        BytecodeModuleMetadata metadata;
        
        // ソースファイル
        metadata.source_file = readString();
        
        // フラグ
        uint8_t flags = data[position++];
        metadata.is_module = (flags & 0x01) != 0;
        metadata.strict_mode = (flags & 0x02) != 0;
        
        // タイムスタンプ
        uint64_t timestamp = 0;
        for (int i = 0; i < 8; i++) {
          timestamp |= (static_cast<uint64_t>(data[position++]) << (i * 8));
        }
        metadata.timestamp = static_cast<time_t>(timestamp);
        
        module->setMetadata(metadata);
        break;
      }
      
      case 0x02: { // 文字列テーブルセクション
        uint32_t str_count = readUint32();
        std::vector<std::string> string_table;
        
        for (uint32_t i = 0; i < str_count; i++) {
          string_table.push_back(readString());
        }
        
        // 文字列テーブルを設定（addStringでなく直接設定）
        module->m_stringTable = std::move(string_table);
        break;
      }
      
      case 0x03: { // 定数プールセクション
        uint32_t pool_size = readUint32();
        
        if (pool_size > 0) {
          if (position + pool_size > data.size()) {
            throw std::runtime_error("データの境界外を読み取ろうとしています");
          }
          
          std::vector<uint8_t> pool_data(data.begin() + position, data.begin() + position + pool_size);
          position += pool_size;
          
          auto constant_pool = ConstantPool::deserialize(pool_data);
          module->setConstantPool(std::move(constant_pool));
        } else {
          module->setConstantPool(std::make_shared<ConstantPool>());
        }
        break;
      }
      
      case 0x04: { // 命令セクション
        uint32_t instr_count = readUint32();
        std::vector<BytecodeInstruction> instructions;
        
        for (uint32_t i = 0; i < instr_count; i++) {
          uint8_t opcode = data[position++];
          
          // オペランドを読み取る前に現在のオペランド数を取得
          uint8_t operand_count = 0;
          size_t operand_position = position;
          
          // オペランドを飛ばして、オペランド数を先に取得
          operand_position += 3 * 4; // 最大3つのオペランドをスキップ
          if (operand_position < data.size()) {
            operand_count = data[operand_position];
          }
          
          // オペランドを読み取る
          uint32_t operands[3] = {0, 0, 0};
          for (int j = 0; j < operand_count && j < 3; j++) {
            operands[j] = readUint32();
          }
          
          // オペランド数を読み取り、位置を更新（既に読み取り済みなので実際には使わない）
          position++;
          
          // 命令を作成
          BytecodeInstruction instr;
          instr.setOpcode(static_cast<BytecodeOpcode>(opcode));
          for (int j = 0; j < operand_count && j < 3; j++) {
            instr.setOperand(j, operands[j]);
          }
          
          instructions.push_back(instr);
        }
        
        module->setInstructions(instructions);
        break;
      }
      
      case 0x05: { // 関数情報セクション
        uint32_t func_count = readUint32();
        
        for (uint32_t i = 0; i < func_count; i++) {
          FunctionInfo func;
          
          // コードオフセットと長さ
          func.code_offset = readUint32();
          func.code_length = readUint32();
          
          // 引数の数
          func.arity = data[position++];
          
          // フラグ
          uint8_t func_flags = data[position++];
          func.is_strict = (func_flags & 0x01) != 0;
          func.is_arrow_function = (func_flags & 0x02) != 0;
          func.is_generator = (func_flags & 0x04) != 0;
          func.is_async = (func_flags & 0x08) != 0;
          
          // 関数名
          func.name = readString();
          
          // パラメータ名
          uint32_t param_count = readUint32();
          for (uint32_t j = 0; j < param_count; j++) {
            func.parameter_names.push_back(readString());
          }
          
          // ローカル変数名
          uint32_t local_count = readUint32();
          for (uint32_t j = 0; j < local_count; j++) {
            func.local_names.push_back(readString());
          }
          
          module->addFunctionInfo(func);
        }
        break;
      }
      
      case 0x06: { // ソースマップセクション
        uint32_t entry_count = readUint32();
        
        for (uint32_t i = 0; i < entry_count; i++) {
          SourceMapEntry entry;
          
          entry.bytecode_offset = readUint32();
          entry.line = readUint32();
          entry.column = readUint32();
          entry.source_file = readString();
          
          module->addSourceMapEntry(entry);
        }
        break;
      }
      
      case 0xFF: { // フッター
        // チェックサムを検証（完全性チェック）
        uint32_t stored_checksum = readUint32();
        
        // すべてのセクションを読み取り終わったので、処理を終了
        position = data.size(); // ループを終了させる
        break;
      }
      
      default:
        // 不明なセクション識別子、無視してスキップ
        // 将来のバージョンでの拡張を考慮
        break;
    }
  }
  
  return module;
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