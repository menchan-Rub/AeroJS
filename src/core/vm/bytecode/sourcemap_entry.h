/**
 * @file sourcemap_entry.h
 * @brief ソースマップエントリのヘッダーファイル
 *
 * このファイルは、バイトコード命令とソースコードの位置との対応関係を表す
 * ソースマップエントリクラスを定義します。デバッグや例外処理時に
 * バイトコードの位置からソースコードの位置を特定するために使用されます。
 */

#ifndef AEROJS_CORE_VM_BYTECODE_SOURCEMAP_ENTRY_H_
#define AEROJS_CORE_VM_BYTECODE_SOURCEMAP_ENTRY_H_

#include <cstdint>
#include <string>

namespace aerojs {
namespace core {

/**
 * @brief ソースマップエントリ構造体
 *
 * バイトコード命令とソースコードの位置との対応関係を表します。
 */
struct SourceMapEntry {
  uint32_t bytecode_offset;  ///< バイトコード内の命令オフセット
  uint32_t line;             ///< ソースコードの行番号
  uint32_t column;           ///< ソースコードの列番号
  std::string source_file;   ///< ソースファイル名（複数ファイルがある場合）

  /**
   * @brief コンストラクタ
   *
   * @param bytecode_offset バイトコード内の命令オフセット
   * @param line ソースコードの行番号
   * @param column ソースコードの列番号
   * @param source_file ソースファイル名
   */
  SourceMapEntry(uint32_t bytecode_offset = 0,
                 uint32_t line = 0,
                 uint32_t column = 0,
                 const std::string& source_file = "")
      : bytecode_offset(bytecode_offset),
        line(line),
        column(column),
        source_file(source_file) {
  }

  /**
   * @brief ソースマップエントリの文字列表現を取得
   *
   * @return 文字列表現
   */
  std::string toString() const {
    return "BytecodeOffset: " + std::to_string(bytecode_offset) +
           ", Line: " + std::to_string(line) +
           ", Column: " + std::to_string(column) +
           ", SourceFile: " + (source_file.empty() ? "<unknown>" : source_file);
  }

  /**
   * @brief 等価演算子
   *
   * @param other 比較するソースマップエントリ
   * @return 等しい場合はtrue、そうでない場合はfalse
   */
  bool operator==(const SourceMapEntry& other) const {
    return bytecode_offset == other.bytecode_offset &&
           line == other.line &&
           column == other.column &&
           source_file == other.source_file;
  }

  /**
   * @brief 不等価演算子
   *
   * @param other 比較するソースマップエントリ
   * @return 等しくない場合はtrue、そうでない場合はfalse
   */
  bool operator!=(const SourceMapEntry& other) const {
    return !(*this == other);
  }
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_VM_BYTECODE_SOURCEMAP_ENTRY_H_