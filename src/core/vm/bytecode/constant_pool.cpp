/**
 * @file constant_pool.cpp
 * @brief 定数プールの実装ファイル
 *
 * このファイルは、バイトコードモジュールで使用される定数プールクラスを実装します。
 * 定数プールは文字列、数値、真偽値などのリテラル値を管理します。
 */

#include "constant_pool.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

namespace aerojs {
namespace core {

ConstantPool::ConstantPool(bool enable_deduplication)
    : m_enableDeduplication(enable_deduplication) {
}

ConstantPool::~ConstantPool() {
  // 特に何もしない、スマートポインタが自動的にメモリを解放
}

uint32_t ConstantPool::addConstant(const Value& value) {
  // 重複排除が有効な場合は、既存の同じ値を検索
  if (m_enableDeduplication) {
    uint32_t existing_index = findDuplicateConstant(value);
    if (existing_index != std::numeric_limits<uint32_t>::max()) {
      return existing_index;
    }
  }

  // 新しい定数を追加
  uint32_t index = static_cast<uint32_t>(m_constants.size());
  m_constants.push_back(value);

  // 重複排除が有効な場合は、マップにも追加
  if (m_enableDeduplication) {
    m_map[value] = index;
  }

  return index;
}

uint32_t ConstantPool::addString(const std::string& str) {
  Value value;
  value.setString(str);
  return addConstant(value);
}

uint32_t ConstantPool::addNumber(double number) {
  Value value;
  value.setNumber(number);
  return addConstant(value);
}

uint32_t ConstantPool::addInteger(int32_t integer) {
  Value value;
  value.setNumber(static_cast<double>(integer));
  return addConstant(value);
}

uint32_t ConstantPool::addBigInt(const std::string& bigint) {
  Value value;
  value.setBigInt(bigint);
  return addConstant(value);
}

uint32_t ConstantPool::addBoolean(bool boolean) {
  Value value;
  value.setBoolean(boolean);
  return addConstant(value);
}

uint32_t ConstantPool::addNull() {
  Value value;
  value.setNull();
  return addConstant(value);
}

uint32_t ConstantPool::addUndefined() {
  Value value;
  value.setUndefined();
  return addConstant(value);
}

uint32_t ConstantPool::addRegExp(const std::string& pattern, const std::string& flags) {
  Value value;
  value.setRegExp(pattern, flags);
  return addConstant(value);
}

uint32_t ConstantPool::addFunction(uint32_t function_index) {
  Value value;
  value.setFunction(function_index);
  return addConstant(value);
}

const Value& ConstantPool::getConstant(uint32_t index) const {
  if (index >= m_constants.size()) {
    throw std::out_of_range("Constant index out of range");
  }
  return m_constants[index];
}

uint32_t ConstantPool::findConstant(const Value& value) const {
  // 重複排除マップから検索
  if (m_enableDeduplication) {
    auto it = m_map.find(value);
    if (it != m_map.end()) {
      return it->second;
    }
  } else {
    // 重複排除が無効の場合は線形検索
    auto it = std::find(m_constants.begin(), m_constants.end(), value);
    if (it != m_constants.end()) {
      return static_cast<uint32_t>(std::distance(m_constants.begin(), it));
    }
  }

  return std::numeric_limits<uint32_t>::max();
}

const std::vector<Value>& ConstantPool::getConstants() const {
  return m_constants;
}

size_t ConstantPool::size() const {
  return m_constants.size();
}

std::vector<uint8_t> ConstantPool::serialize() const {
  // 効率的なバイナリフォーマットでシリアライズ
  std::vector<uint8_t> data;

  // ヘルパー関数：uint32_tをリトルエンディアンでデータに追加
  auto appendUint32 = [&data](uint32_t value) {
    data.push_back(static_cast<uint8_t>(value & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  };
  
  // ヘルパー関数：doubleをデータに追加
  auto appendDouble = [&data](double value) {
    union {
      double d;
      uint64_t i;
    } converter;
    converter.d = value;
    
    for (int i = 0; i < 8; i++) {
      data.push_back(static_cast<uint8_t>((converter.i >> (i * 8)) & 0xFF));
    }
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
  
  // 定数プールヘッダ
  // マジックナンバー "CPOOL"
  data.push_back('C');
  data.push_back('P');
  data.push_back('O');
  data.push_back('O');
  data.push_back('L');
  
  // バージョン
  data.push_back(1); // メジャーバージョン
  data.push_back(0); // マイナーバージョン
  
  // 定数の数
  appendUint32(static_cast<uint32_t>(m_constants.size()));
  
  // 定数データ
  for (const auto& value : m_constants) {
    // 値の型情報
    uint8_t type = static_cast<uint8_t>(value.getType());
    data.push_back(type);
    
    // 型に応じたデータのシリアライズ
    switch (value.getType()) {
      case ValueType::Undefined:
        // 型情報のみ
        break;
        
      case ValueType::Null:
        // 型情報のみ
        break;
        
      case ValueType::Boolean:
        // Boolean値（1バイト）
        data.push_back(value.getBoolean() ? 1 : 0);
        break;
        
      case ValueType::Number: {
        // 倍精度浮動小数点数（8バイト）
        appendDouble(value.getNumber());
        break;
      }
        
      case ValueType::String: {
        // 文字列
        appendString(value.getString());
        break;
      }
        
      case ValueType::BigInt: {
        // BigInt（文字列表現）
        appendString(value.getBigInt());
        break;
      }
        
      case ValueType::RegExp: {
        // 正規表現（パターンと flags の文字列）
        appendString(value.getRegExpPattern());
        appendString(value.getRegExpFlags());
        break;
      }
        
      case ValueType::Function: {
        // 関数インデックス
        appendUint32(value.getFunctionIndex());
        break;
      }
        
      default:
        // 未対応の型の場合は警告
        std::cerr << "警告: 未対応の値型 " << static_cast<int>(value.getType()) << " がシリアライズされました" << std::endl;
        break;
    }
  }
  
  // フッター（整合性チェック用）
  data.push_back('E'); // 終了マーカー
  data.push_back('N');
  data.push_back('D');
  
  // 単純なチェックサム
  uint32_t checksum = 0;
  for (size_t i = 0; i < data.size(); i++) {
    checksum = (checksum + data[i]) & 0xFFFFFFFF;
  }
  appendUint32(checksum);
  
  return data;
}

std::unique_ptr<ConstantPool> ConstantPool::deserialize(const std::vector<uint8_t>& data) {
  // バイナリデータから定数プールを復元
  if (data.size() < 16) { // 最小限必要なサイズ（ヘッダー、定数カウント、フッター）
    return nullptr;
  }
  
  // ヘッダーチェック
  if (data[0] != 'C' || data[1] != 'P' || data[2] != 'O' || data[3] != 'O' || data[4] != 'L') {
    return nullptr; // 無効なヘッダー
  }
  
  // バージョンチェック
  uint8_t majorVersion = data[5];
  uint8_t minorVersion = data[6];
  
  if (majorVersion > 1) {
    // 互換性のないバージョン
    return nullptr;
  }
  
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
  
  // ヘルパー関数：doubleを読み取り
  auto readDouble = [&data, &position]() -> double {
    if (position + 8 > data.size()) {
      throw std::runtime_error("データの境界外を読み取ろうとしています");
    }
    
    union {
      uint64_t i;
      double d;
    } converter;
    
    converter.i = 0;
    for (int i = 0; i < 8; i++) {
      converter.i |= static_cast<uint64_t>(data[position + i]) << (i * 8);
    }
    
    position += 8;
    return converter.d;
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
  
  // 定数数の読み取り
  uint32_t constantCount = readUint32();
  
  // 新しい定数プールの作成
  auto pool = std::make_unique<ConstantPool>(true); // 重複排除を有効化
  
  // 定数の読み取り
  for (uint32_t i = 0; i < constantCount; i++) {
    if (position >= data.size()) {
      throw std::runtime_error("予期せぬデータ終端");
    }
    
    // 型情報の読み取り
    uint8_t typeValue = data[position++];
    ValueType type = static_cast<ValueType>(typeValue);
    
    // 値の作成
    Value value;
    
    switch (type) {
      case ValueType::Undefined:
        value.setUndefined();
        break;
        
      case ValueType::Null:
        value.setNull();
        break;
        
      case ValueType::Boolean:
        if (position >= data.size()) {
          throw std::runtime_error("予期せぬデータ終端");
        }
        value.setBoolean(data[position++] != 0);
        break;
        
      case ValueType::Number:
        value.setNumber(readDouble());
        break;
        
      case ValueType::String:
        value.setString(readString());
        break;
        
      case ValueType::BigInt:
        value.setBigInt(readString());
        break;
        
      case ValueType::RegExp: {
        std::string pattern = readString();
        std::string flags = readString();
        value.setRegExp(pattern, flags);
        break;
      }
        
      case ValueType::Function:
        value.setFunction(readUint32());
        break;
        
      default:
        throw std::runtime_error("未対応の値型: " + std::to_string(typeValue));
    }
    
    // 定数プールに追加
    pool->m_constants.push_back(value);
    
    // 重複排除マップにも追加
    if (pool->m_enableDeduplication) {
      pool->m_map[value] = i;
    }
  }
  
  // フッターの検証（最低限のチェック）
  if (position + 7 > data.size() || 
      data[position] != 'E' || data[position + 1] != 'N' || data[position + 2] != 'D') {
    // フッターが無効な場合でも、ここまで読み取った定数は有効と判断して返す
    std::cerr << "警告: 定数プールのフッターが無効です" << std::endl;
  }
  
  return pool;
}

void ConstantPool::dump(std::ostream& output) const {
  for (size_t i = 0; i < m_constants.size(); ++i) {
    const auto& value = m_constants[i];
    output << std::setw(4) << i << ": " << value.toString() << std::endl;
  }
}

uint32_t ConstantPool::findDuplicateConstant(const Value& value) const {
  if (!m_enableDeduplication) {
    return std::numeric_limits<uint32_t>::max();
  }

  auto it = m_map.find(value);
  if (it != m_map.end()) {
    return it->second;
  }

  return std::numeric_limits<uint32_t>::max();
}

}  // namespace core
}  // namespace aerojs