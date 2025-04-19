/**
 * @file stack.h
 * @brief 仮想マシンのスタック定義
 * 
 * このファイルはJavaScriptエンジンの仮想マシンで使用されるスタッククラスを定義します。
 * スタックはインタプリタが値を操作するための主要なデータ構造です。
 */

#ifndef AEROJS_CORE_VM_STACK_STACK_H_
#define AEROJS_CORE_VM_STACK_STACK_H_

#include <vector>
#include <memory>
#include <stdexcept>
#include "../../runtime/values/value.h"

namespace aerojs {
namespace core {

/**
 * @brief VMスタッククラス
 * 
 * JavaScriptエンジンの仮想マシンで使用される値のスタックを管理します。
 * インタプリタは命令実行中にこのスタックを使用して値を操作します。
 */
class Stack {
public:
  /**
   * @brief デフォルトコンストラクタ
   * 
   * デフォルトの初期容量でスタックを作成します。
   */
  Stack();

  /**
   * @brief 指定した初期容量でスタックを作成
   * 
   * @param initialCapacity スタックの初期容量
   */
  explicit Stack(size_t initialCapacity);

  /**
   * @brief デストラクタ
   */
  ~Stack();

  /**
   * @brief 値をスタックにプッシュする
   * 
   * @param value プッシュする値
   */
  void push(ValuePtr value);

  /**
   * @brief スタックから値をポップする
   * 
   * @return ValuePtr ポップされた値
   * @throws std::runtime_error スタックが空の場合
   */
  ValuePtr pop();

  /**
   * @brief スタックの最上位の値を確認する (ポップしない)
   * 
   * @return ValuePtr スタックの最上位の値
   * @throws std::runtime_error スタックが空の場合
   */
  ValuePtr peek() const;

  /**
   * @brief スタックの指定位置の値を確認する (ポップしない)
   * 
   * @param index スタックの上からのインデックス (0が最上位)
   * @return ValuePtr 指定位置の値
   * @throws std::out_of_range インデックスが範囲外の場合
   */
  ValuePtr peekAt(size_t index) const;

  /**
   * @brief スタックの指定位置の値を設定する
   * 
   * @param index スタックの上からのインデックス (0が最上位)
   * @param value 設定する値
   * @throws std::out_of_range インデックスが範囲外の場合
   */
  void setAt(size_t index, ValuePtr value);

  /**
   * @brief スタックのサイズを取得する
   * 
   * @return size_t スタックに格納されている値の数
   */
  size_t size() const;

  /**
   * @brief スタックが空かどうかを確認する
   * 
   * @return bool スタックが空の場合はtrue
   */
  bool isEmpty() const;

  /**
   * @brief スタックをクリアする
   */
  void clear();

  /**
   * @brief 指定した深さの値をポップする
   * 
   * スタックから指定した数の値をポップします。
   * 
   * @param count ポップする値の数
   * @throws std::runtime_error スタックに十分な値がない場合
   */
  void popMultiple(size_t count);

  /**
   * @brief スタックの内容をダンプする
   * 
   * デバッグ用にスタックの内容を文字列として取得します。
   * 
   * @param maxItems 出力する最大項目数 (0は制限なし)
   * @return std::string スタックの内容を表す文字列
   */
  std::string dump(size_t maxItems = 0) const;

private:
  /** @brief スタックの値を保持するベクター */
  std::vector<ValuePtr> m_values;
  
  /** @brief スタックの最大容量 */
  size_t m_maxCapacity;
  
  /** @brief スタックの初期容量 */
  static constexpr size_t kDefaultInitialCapacity = 1024;
  
  /** @brief スタックの最大容量のデフォルト値 */
  static constexpr size_t kDefaultMaxCapacity = 1024 * 1024;
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_VM_STACK_STACK_H_ 