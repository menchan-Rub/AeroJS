/**
 * @file source_location.h
 * @brief ソースコードの位置情報を表すクラス
 *
 * このファイルはソースコード内の位置（ファイル名、行番号、列番号など）を表す
 * クラスを定義します。AST ノードやトークンの位置情報に使用されます。
 */

#ifndef AEROJS_CORE_PARSER_SOURCEMAP_SOURCE_LOCATION_H_
#define AEROJS_CORE_PARSER_SOURCEMAP_SOURCE_LOCATION_H_

#include <string>

namespace aerojs {
namespace core {

/**
 * @brief ソースコード内の位置を表す構造体
 *
 * ファイル名、行番号、列番号、オフセットなどのソースコード内の位置情報を保持します。
 * この情報はエラーメッセージの表示やソースマップの生成に使用されます。
 */
struct SourceLocation {
  /** @brief ソースファイル名 */
  std::string filename;

  /** @brief 行番号（1から始まる） */
  int line;

  /** @brief 列番号（1から始まる） */
  int column;

  /** @brief ファイル先頭からのバイトオフセット */
  int offset;

  /** @brief ソースコード範囲の長さ（バイト数） */
  int length;

  /**
   * @brief デフォルトコンストラクタ
   */
  SourceLocation()
      : filename(""), line(0), column(0), offset(0), length(0) {
  }

  /**
   * @brief 詳細情報を指定して位置情報を構築
   *
   * @param filename ソースファイル名
   * @param line 行番号（1から始まる）
   * @param column 列番号（1から始まる）
   * @param offset ファイル先頭からのバイトオフセット（オプション）
   * @param length ソースコード範囲の長さ（オプション）
   */
  SourceLocation(const std::string& filename, int line, int column, int offset = 0, int length = 0)
      : filename(filename), line(line), column(column), offset(offset), length(length) {
  }

  /**
   * @brief 位置情報の文字列表現を取得
   *
   * @return ファイル名:行番号:列番号 の形式の文字列
   */
  std::string toString() const {
    std::string result = filename;
    if (line > 0) {
      result += ":" + std::to_string(line);
      if (column > 0) {
        result += ":" + std::to_string(column);
      }
    }
    return result;
  }

  /**
   * @brief 2つの位置情報が等しいかを比較
   *
   * @param other 比較する位置情報
   * @return 等しい場合は true、そうでなければ false
   */
  bool operator==(const SourceLocation& other) const {
    return filename == other.filename &&
           line == other.line &&
           column == other.column &&
           offset == other.offset &&
           length == other.length;
  }

  /**
   * @brief 2つの位置情報が異なるかを比較
   *
   * @param other 比較する位置情報
   * @return 異なる場合は true、そうでなければ false
   */
  bool operator!=(const SourceLocation& other) const {
    return !(*this == other);
  }

  /**
   * @brief 位置情報が設定されているかをチェック
   *
   * @return 有効な位置情報がある場合はtrue
   */
  bool isValid() const {
    return !filename.empty() && line > 0;
  }
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_PARSER_SOURCEMAP_SOURCE_LOCATION_H_