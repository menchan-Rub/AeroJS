/**
 * @file sourcemap_entry.cpp
 * @brief バイトコードとソースコードの位置マッピング実装
 *
 * このファイルは、バイトコード命令とソースコードの位置をマッピングする
 * SourceMapEntryクラスの実装を提供します。デバッグや例外処理に使用されます。
 */

#include "sourcemap_entry.h"

#include <algorithm>
#include <cassert>
#include <sstream>

namespace aerojs {
namespace core {

/**
 * @brief 新しいソースマップエントリを作成
 *
 * @param bc_offset バイトコードオフセット
 * @param bc_length バイトコード長
 * @param source_offset ソースコードオフセット
 * @param source_length ソースコード長
 * @param line_number 行番号
 * @param column_number 列番号
 * @return 作成されたSourceMapEntryオブジェクト
 */
SourceMapEntry SourceMapEntry::create(
    uint32_t bc_offset,
    uint32_t bc_length,
    uint32_t source_offset,
    uint32_t source_length,
    uint32_t line_number,
    uint32_t column_number) {
  SourceMapEntry entry;
  entry.bytecode_offset = bc_offset;
  entry.bytecode_length = bc_length;
  entry.source_offset = source_offset;
  entry.source_length = source_length;
  entry.line = line_number;
  entry.column = column_number;
  return entry;
}

/**
 * @brief 指定されたバイトコードオフセットがこのエントリ範囲内にあるかチェック
 *
 * @param offset チェックするバイトコードオフセット
 * @return オフセットがこのエントリの範囲内であればtrue
 */
bool SourceMapEntry::containsBytecodeOffset(uint32_t offset) const {
  return offset >= bytecode_offset && offset < bytecode_offset + bytecode_length;
}

/**
 * @brief 指定されたソースコードオフセットがこのエントリ範囲内にあるかチェック
 *
 * @param offset チェックするソースコードオフセット
 * @return オフセットがこのエントリの範囲内であればtrue
 */
bool SourceMapEntry::containsSourceOffset(uint32_t offset) const {
  return offset >= source_offset && offset < source_offset + source_length;
}

/**
 * @brief エントリの文字列表現を取得
 *
 * @return エントリの詳細を含む文字列
 */
std::string SourceMapEntry::toString() const {
  std::stringstream ss;
  ss << "BC[" << bytecode_offset << "-" << (bytecode_offset + bytecode_length - 1)
     << "] => Source[" << source_offset << "-" << (source_offset + source_length - 1)
     << "] (line " << line << ", col " << column << ")";
  return ss.str();
}

/**
 * @brief ソースマップエントリをシリアライズ
 *
 * @return シリアライズされたバイトデータ
 */
std::vector<uint8_t> SourceMapEntry::serialize() const {
  std::vector<uint8_t> result;
  result.reserve(24);  // 6つの32ビット整数 = 24バイト

  // バイトコードオフセットとサイズ
  appendUint32(result, bytecode_offset);
  appendUint32(result, bytecode_length);

  // ソースコードオフセットとサイズ
  appendUint32(result, source_offset);
  appendUint32(result, source_length);

  // 行と列の情報
  appendUint32(result, line);
  appendUint32(result, column);

  return result;
}

/**
 * @brief バイナリデータからソースマップエントリをデシリアライズ
 *
 * @param data シリアライズされたデータ
 * @param pos 現在の位置（更新される）
 * @return デシリアライズされたSourceMapEntry
 */
SourceMapEntry SourceMapEntry::deserialize(const std::vector<uint8_t>& data, size_t& pos) {
  SourceMapEntry entry;

  // バイトコードオフセットとサイズを読み込む
  entry.bytecode_offset = readUint32(data, pos);
  entry.bytecode_length = readUint32(data, pos);

  // ソースコードオフセットとサイズを読み込む
  entry.source_offset = readUint32(data, pos);
  entry.source_length = readUint32(data, pos);

  // 行と列の情報を読み込む
  entry.line = readUint32(data, pos);
  entry.column = readUint32(data, pos);

  return entry;
}

/**
 * @brief バイトコードオフセットでソースマップエントリを比較するための比較関数
 *
 * @param a 比較する最初のエントリ
 * @param b 比較する2番目のエントリ
 * @return aのバイトコードオフセットがbより小さい場合はtrue
 */
bool SourceMapEntry::compareByBytecodeOffset(const SourceMapEntry& a, const SourceMapEntry& b) {
  return a.bytecode_offset < b.bytecode_offset;
}

/**
 * @brief ソースコードオフセットでソースマップエントリを比較するための比較関数
 *
 * @param a 比較する最初のエントリ
 * @param b 比較する2番目のエントリ
 * @return aのソースコードオフセットがbより小さい場合はtrue
 */
bool SourceMapEntry::compareBySourceOffset(const SourceMapEntry& a, const SourceMapEntry& b) {
  return a.source_offset < b.source_offset;
}

/**
 * @brief バイナリデータへの32ビット符号なし整数の追加
 *
 * @param data 追加先のバイナリデータ
 * @param value 追加する値
 */
void SourceMapEntry::appendUint32(std::vector<uint8_t>& data, uint32_t value) const {
  data.push_back(static_cast<uint8_t>(value & 0xFF));
  data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

/**
 * @brief バイナリデータからの32ビット符号なし整数の読み取り
 *
 * @param data 読み取り元のバイナリデータ
 * @param pos 現在の位置（更新される）
 * @return 読み取られた32ビット符号なし整数
 */
uint32_t SourceMapEntry::readUint32(const std::vector<uint8_t>& data, size_t& pos) {
  assert(pos + 4 <= data.size());

  uint32_t result =
      static_cast<uint32_t>(data[pos]) |
      (static_cast<uint32_t>(data[pos + 1]) << 8) |
      (static_cast<uint32_t>(data[pos + 2]) << 16) |
      (static_cast<uint32_t>(data[pos + 3]) << 24);

  pos += 4;
  return result;
}

// SourceMapManagerの実装

/**
 * @brief ソースマップエントリを追加
 *
 * @param entry 追加するエントリ
 */
void SourceMapManager::addEntry(const SourceMapEntry& entry) {
  m_entries.push_back(entry);
  m_sorted = false;
}

/**
 * @brief バイトコードオフセットに対応するソースコード位置を検索
 *
 * @param bytecode_offset 検索するバイトコードオフセット
 * @return ソースコード位置情報、見つからない場合は無効なエントリ
 */
SourceMapEntry SourceMapManager::findEntryByBytecodeOffset(uint32_t bytecode_offset) {
  if (m_entries.empty()) {
    return SourceMapEntry();  // 無効なエントリ
  }

  if (!m_sorted) {
    sortByBytecodeOffset();
  }

  // バイナリサーチのための比較関数
  auto compare = [](const SourceMapEntry& entry, uint32_t offset) {
    return entry.bytecode_offset + entry.bytecode_length <= offset;
  };

  // オフセットを含む最初のエントリを検索
  auto it = std::lower_bound(
      m_entries.begin(),
      m_entries.end(),
      bytecode_offset,
      compare);

  if (it != m_entries.end() && it->containsBytecodeOffset(bytecode_offset)) {
    return *it;
  }

  // エントリが見つからなかった場合、最も近いエントリを返す
  if (!m_entries.empty()) {
    // 最も近い前のエントリを見つける
    auto closest = m_entries.begin();
    uint32_t min_distance = UINT32_MAX;

    for (auto iter = m_entries.begin(); iter != m_entries.end(); ++iter) {
      if (iter->bytecode_offset <= bytecode_offset) {
        uint32_t distance = bytecode_offset - iter->bytecode_offset;
        if (distance < min_distance) {
          min_distance = distance;
          closest = iter;
        }
      }
    }

    return *closest;
  }

  return SourceMapEntry();  // 無効なエントリ
}

/**
 * @brief ソースコードオフセットに対応するバイトコード位置を検索
 *
 * @param source_offset 検索するソースコードオフセット
 * @return バイトコード位置情報、見つからない場合は無効なエントリ
 */
SourceMapEntry SourceMapManager::findEntryBySourceOffset(uint32_t source_offset) {
  if (m_entries.empty()) {
    return SourceMapEntry();  // 無効なエントリ
  }

  // ソースオフセットでソートされていない場合はソート
  sortBySourceOffset();

  // バイナリサーチのための比較関数
  auto compare = [](const SourceMapEntry& entry, uint32_t offset) {
    return entry.source_offset + entry.source_length <= offset;
  };

  // オフセットを含む最初のエントリを検索
  auto it = std::lower_bound(
      m_entries.begin(),
      m_entries.end(),
      source_offset,
      compare);

  if (it != m_entries.end() && it->containsSourceOffset(source_offset)) {
    return *it;
  }

  // エントリが見つからなかった場合、最も近いエントリを返す
  if (!m_entries.empty()) {
    // 最も近い前のエントリを見つける
    auto closest = m_entries.begin();
    uint32_t min_distance = UINT32_MAX;

    for (auto iter = m_entries.begin(); iter != m_entries.end(); ++iter) {
      if (iter->source_offset <= source_offset) {
        uint32_t distance = source_offset - iter->source_offset;
        if (distance < min_distance) {
          min_distance = distance;
          closest = iter;
        }
      }
    }

    return *closest;
  }

  return SourceMapEntry();  // 無効なエントリ
}

/**
 * @brief 指定された行と列に最も近いソースマップエントリを見つける
 *
 * @param line 検索する行番号
 * @param column 検索する列番号
 * @return 最も近いエントリ、見つからない場合は無効なエントリ
 */
SourceMapEntry SourceMapManager::findEntryByLineColumn(uint32_t line, uint32_t column) {
  if (m_entries.empty()) {
    return SourceMapEntry();  // 無効なエントリ
  }

  SourceMapEntry* closest = nullptr;
  uint32_t min_distance = UINT32_MAX;

  for (auto& entry : m_entries) {
    // 行が一致しない場合は行の距離を計算
    uint32_t line_distance = std::abs(static_cast<int32_t>(entry.line) - static_cast<int32_t>(line));

    // 行が一致する場合は列の距離を計算
    uint32_t column_distance = 0;
    if (line_distance == 0) {
      column_distance = std::abs(static_cast<int32_t>(entry.column) - static_cast<int32_t>(column));
    }

    // 距離の重み付け（行の違いは列の違いよりも重要）
    uint32_t distance = (line_distance * 1000) + column_distance;

    if (distance < min_distance) {
      min_distance = distance;
      closest = &entry;
    }
  }

  return closest ? *closest : SourceMapEntry();
}

/**
 * @brief すべてのエントリをクリア
 */
void SourceMapManager::clear() {
  m_entries.clear();
  m_sorted = true;
}

/**
 * @brief すべてのソースマップエントリを取得
 *
 * @return エントリの配列
 */
const std::vector<SourceMapEntry>& SourceMapManager::getAllEntries() const {
  return m_entries;
}

/**
 * @brief エントリの数を取得
 *
 * @return エントリの数
 */
size_t SourceMapManager::size() const {
  return m_entries.size();
}

/**
 * @brief エントリを現在のラインとコラムでマージ
 *
 * 同じソース位置の連続するバイトコード命令をマージして、
 * ソースマップのサイズを削減します。
 */
void SourceMapManager::mergeEntriesByLineColumn() {
  if (m_entries.size() <= 1) {
    return;
  }

  // バイトコードオフセットでソート
  sortByBytecodeOffset();

  std::vector<SourceMapEntry> merged;
  merged.reserve(m_entries.size());

  SourceMapEntry current = m_entries[0];

  for (size_t i = 1; i < m_entries.size(); ++i) {
    const auto& entry = m_entries[i];

    // 同じ行と列で、連続したバイトコード範囲であれば、マージする
    if (entry.line == current.line &&
        entry.column == current.column &&
        entry.bytecode_offset == current.bytecode_offset + current.bytecode_length) {
      // 現在のエントリを拡張
      current.bytecode_length += entry.bytecode_length;

      // ソース長も拡張（必要に応じて）
      if (entry.source_offset + entry.source_length > current.source_offset + current.source_length) {
        current.source_length = (entry.source_offset + entry.source_length) - current.source_offset;
      }
    } else {
      // マージできないので、前のエントリを保存して新しいエントリを開始
      merged.push_back(current);
      current = entry;
    }
  }

  // 最後のエントリを追加
  merged.push_back(current);

  // マージされたエントリでリストを置き換え
  m_entries = std::move(merged);
  m_sorted = true;
}

/**
 * @brief バイトコードオフセットでエントリをソート
 */
void SourceMapManager::sortByBytecodeOffset() {
  if (!m_sorted) {
    std::sort(m_entries.begin(), m_entries.end(), SourceMapEntry::compareByBytecodeOffset);
    m_sorted = true;
  }
}

/**
 * @brief ソースコードオフセットでエントリをソート
 */
void SourceMapManager::sortBySourceOffset() {
  std::sort(m_entries.begin(), m_entries.end(), SourceMapEntry::compareBySourceOffset);
  // このソートでバイトコードオフセット順のソートは失われるため、フラグをリセット
  m_sorted = false;
}

/**
 * @brief ソースマップ全体をシリアライズ
 *
 * @return シリアライズされたバイナリデータ
 */
std::vector<uint8_t> SourceMapManager::serialize() const {
  std::vector<uint8_t> result;

  // エントリ数を書き込む
  uint32_t count = static_cast<uint32_t>(m_entries.size());
  result.push_back(static_cast<uint8_t>(count & 0xFF));
  result.push_back(static_cast<uint8_t>((count >> 8) & 0xFF));
  result.push_back(static_cast<uint8_t>((count >> 16) & 0xFF));
  result.push_back(static_cast<uint8_t>((count >> 24) & 0xFF));

  // 各エントリをシリアライズ
  for (const auto& entry : m_entries) {
    auto entry_data = entry.serialize();
    result.insert(result.end(), entry_data.begin(), entry_data.end());
  }

  return result;
}

/**
 * @brief バイナリデータからソースマップをデシリアライズ
 *
 * @param data シリアライズされたデータ
 * @return デシリアライズされたSourceMapManager
 */
SourceMapManager SourceMapManager::deserialize(const std::vector<uint8_t>& data) {
  SourceMapManager manager;
  size_t pos = 0;

  if (data.size() < 4) {
    return manager;  // データ不足
  }

  // エントリ数を読み込む
  uint32_t count =
      static_cast<uint32_t>(data[pos]) |
      (static_cast<uint32_t>(data[pos + 1]) << 8) |
      (static_cast<uint32_t>(data[pos + 2]) << 16) |
      (static_cast<uint32_t>(data[pos + 3]) << 24);
  pos += 4;

  // 各エントリをデシリアライズ
  for (uint32_t i = 0; i < count && pos < data.size(); ++i) {
    auto entry = SourceMapEntry::deserialize(data, pos);
    manager.addEntry(entry);
  }

  return manager;
}

}  // namespace core
}  // namespace aerojs