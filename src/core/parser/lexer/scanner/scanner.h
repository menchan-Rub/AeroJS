/**
 * @file scanner.h
 * @brief ECMAScript用の文字スキャナーの定義
 * 
 * このファイルはソースコードの文字列を一文字ずつ解析するための
 * スキャナークラスを定義します。レキサーの内部で使用されます。
 */

#ifndef AEROJS_CORE_PARSER_LEXER_SCANNER_H_
#define AEROJS_CORE_PARSER_LEXER_SCANNER_H_

#include <string>
#include <string_view>
#include "../../sourcemap/source_location.h"

namespace aerojs {
namespace core {

/**
 * @class Scanner
 * @brief ソースコードの文字を一文字ずつスキャンするクラス
 * 
 * このクラスはソースコードの文字を一文字ずつ読み取り、
 * 位置情報を追跡し、文字に関する基本的な判定機能を提供します。
 */
class Scanner {
public:
  /**
   * @brief ソースコードを指定してスキャナーを構築
   * 
   * @param source スキャンするソースコード
   * @param filename ソースファイル名（エラーメッセージ用）
   * @param start_line 開始行番号（デフォルトは1）
   */
  Scanner(const std::string& source, const std::string& filename = "", int start_line = 1);
  
  /**
   * @brief ソース文字列への参照を指定してスキャナーを構築
   * 
   * @param source スキャンするソースコードへの参照
   * @param filename ソースファイル名（エラーメッセージ用）
   * @param start_line 開始行番号（デフォルトは1）
   */
  Scanner(const std::string_view& source, const std::string& filename = "", int start_line = 1);
  
  /**
   * @brief デストラクタ
   */
  ~Scanner() = default;
  
  /**
   * @brief 次の文字を取得して現在位置を進める
   * 
   * @return 次の文字、終端の場合は'\0'
   */
  char advance();
  
  /**
   * @brief 現在の文字を取得（位置は進めない）
   * 
   * @return 現在の文字、終端の場合は'\0'
   */
  char peek() const;
  
  /**
   * @brief 次の文字を取得（位置は進めない）
   * 
   * @return 次の文字、終端の場合は'\0'
   */
  char peekNext() const;
  
  /**
   * @brief n文字先の文字を取得（位置は進めない）
   * 
   * @param n 先読みする文字数
   * @return n文字先の文字、範囲外の場合は'\0'
   */
  char peekAhead(size_t n) const;
  
  /**
   * @brief 期待する文字と一致するか確認し、一致すれば位置を進める
   * 
   * @param expected 期待する文字
   * @return 一致した場合はtrue、それ以外はfalse
   */
  bool match(char expected);
  
  /**
   * @brief ソースの終端に達したかどうかを確認
   * 
   * @return 終端に達した場合はtrue
   */
  bool isAtEnd() const;
  
  /**
   * @brief 現在の位置情報を取得
   * 
   * @return 現在の位置情報（ファイル名、行、列）
   */
  SourceLocation getCurrentLocation() const;
  
  /**
   * @brief 現在の行番号を取得
   * 
   * @return 現在の行番号
   */
  int getLine() const;
  
  /**
   * @brief 現在の列番号を取得
   * 
   * @return 現在の列番号
   */
  int getColumn() const;
  
  /**
   * @brief 現在位置をマーク（後で戻るため）
   */
  void markPosition();
  
  /**
   * @brief マークした位置に戻る
   */
  void resetToMark();
  
  /**
   * @brief 現在位置からマークした位置までの文字列を取得
   * 
   * @return マークからの文字列
   */
  std::string getMarkedString() const;
  
  /**
   * @brief 指定された位置から現在位置までの文字列を取得
   * 
   * @param start 開始位置
   * @return 開始位置から現在位置までの文字列
   */
  std::string getStringFrom(size_t start) const;
  
  /**
   * @brief 現在の位置（インデックス）を取得
   * 
   * @return 現在のインデックス
   */
  size_t getCurrentIndex() const;
  
  /**
   * @brief 指定された文字が空白文字かどうかを判定
   * 
   * @param c 判定する文字
   * @return 空白文字の場合はtrue
   */
  static bool isWhitespace(char c);
  
  /**
   * @brief 指定された文字が数字かどうかを判定
   * 
   * @param c 判定する文字
   * @return 数字の場合はtrue
   */
  static bool isDigit(char c);
  
  /**
   * @brief 指定された文字が16進数の数字かどうかを判定
   * 
   * @param c 判定する文字
   * @return 16進数の数字の場合はtrue
   */
  static bool isHexDigit(char c);
  
  /**
   * @brief 指定された文字が8進数の数字かどうかを判定
   * 
   * @param c 判定する文字
   * @return 8進数の数字の場合はtrue
   */
  static bool isOctalDigit(char c);
  
  /**
   * @brief 指定された文字が2進数の数字かどうかを判定
   * 
   * @param c 判定する文字
   * @return 2進数の数字の場合はtrue
   */
  static bool isBinaryDigit(char c);
  
  /**
   * @brief 指定された文字が英字またはアンダースコアかどうかを判定
   * 
   * @param c 判定する文字
   * @return 英字またはアンダースコアの場合はtrue
   */
  static bool isAlpha(char c);
  
  /**
   * @brief 指定された文字が識別子の先頭に使用できる文字かどうかを判定
   * 
   * @param c 判定する文字
   * @return 識別子の先頭に使用できる場合はtrue
   */
  static bool isIdentifierStart(char c);
  
  /**
   * @brief 指定された文字が識別子に使用できる文字かどうかを判定
   * 
   * @param c 判定する文字
   * @return 識別子に使用できる場合はtrue
   */
  static bool isIdentifierPart(char c);
  
  /**
   * @brief 指定された文字が行終端文字かどうかを判定
   * 
   * @param c 判定する文字
   * @return 行終端文字の場合はtrue
   */
  static bool isLineTerminator(char c);

private:
  std::string_view m_source;      ///< スキャンするソースコード
  std::string m_filename;         ///< ソースファイル名
  size_t m_current;               ///< 現在の位置
  size_t m_marked_position;       ///< マークされた位置
  int m_line;                     ///< 現在の行番号
  int m_column;                   ///< 現在の列番号
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_PARSER_LEXER_SCANNER_H_ 