/**
 * @file array.h
 * @brief JavaScript Arrayクラスの定義
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_ARRAY_H
#define AEROJS_ARRAY_H

#include "object.h"
#include "value.h"

#include <vector>
#include <algorithm>
#include <functional>

namespace aerojs {
namespace core {

/**
 * @brief JavaScript Array型を表現するクラス
 *
 * ECMAScript仕様に準拠したArrayオブジェクトの実装。
 * 動的サイズ変更、スパース配列、高度な配列メソッドをサポート。
 */
class Array : public Object {
 public:
  /**
   * @brief 空の配列を作成
   */
  Array();

  /**
   * @brief 指定したサイズの配列を作成
   * @param length 初期サイズ
   */
  explicit Array(uint32_t length);

  /**
   * @brief 初期要素を持つ配列を作成
   * @param elements 初期要素のリスト
   */
  Array(std::initializer_list<Value*> elements);

  /**
   * @brief デストラクタ
   */
  ~Array() override;

  /**
   * @brief オブジェクトタイプを取得
   * @return Type::Array
   */
  Type getType() const override;

  /**
   * @brief 配列の長さを取得
   * @return 配列の長さ
   */
  uint32_t length() const {
    return length_;
  }

  /**
   * @brief 配列の長さを設定
   * @param newLength 新しい長さ
   */
  void setLength(uint32_t newLength);

  /**
   * @brief 指定したインデックスの要素を取得
   * @param index インデックス
   * @return 要素の値、存在しない場合はundefined
   */
  Value* getElement(uint32_t index) const;

  /**
   * @brief 指定したインデックスに要素を設定
   * @param index インデックス
   * @param value 設定する値
   * @return 設定に成功した場合はtrue
   */
  bool setElement(uint32_t index, Value* value);

  /**
   * @brief 指定したインデックスの要素を削除
   * @param index インデックス
   * @return 削除に成功した場合はtrue
   */
  bool deleteElement(uint32_t index);

  /**
   * @brief 指定したインデックスに要素が存在するかを確認
   * @param index インデックス
   * @return 要素が存在する場合はtrue
   */
  bool hasElement(uint32_t index) const;

  /**
   * @brief 配列の末尾に要素を追加
   * @param value 追加する要素
   * @return 新しい配列の長さ
   */
  uint32_t push(Value* value);

  /**
   * @brief 配列の末尾から要素を削除
   * @return 削除された要素、配列が空の場合はundefined
   */
  Value* pop();

  /**
   * @brief 配列の先頭に要素を追加
   * @param value 追加する要素
   * @return 新しい配列の長さ
   */
  uint32_t unshift(Value* value);

  /**
   * @brief 配列の先頭から要素を削除
   * @return 削除された要素、配列が空の場合はundefined
   */
  Value* shift();

  /**
   * @brief 配列の一部を削除/置換
   * @param start 開始インデックス
   * @param deleteCount 削除する要素数
   * @param items 挿入する要素のリスト
   * @return 削除された要素の配列
   */
  Array* splice(int32_t start, uint32_t deleteCount, const std::vector<Value*>& items = {});

  /**
   * @brief 配列の一部を抽出
   * @param start 開始インデックス
   * @param end 終了インデックス（省略可能）
   * @return 抽出された要素の新しい配列
   */
  Array* slice(int32_t start = 0, int32_t end = -1) const;

  /**
   * @brief 他の配列または値と連結
   * @param values 連結する値のリスト
   * @return 連結された新しい配列
   */
  Array* concat(const std::vector<Value*>& values) const;

  /**
   * @brief 配列を指定した区切り文字で文字列に変換
   * @param separator 区切り文字（省略時はカンマ）
   * @return 連結された文字列
   */
  std::string join(const std::string& separator = ",") const;

  /**
   * @brief 配列を逆順にする
   * @return 逆順にされた配列（自身）
   */
  Array* reverse();

  /**
   * @brief 配列をソートする
   * @param compareFn 比較関数（省略可能）
   * @return ソートされた配列（自身）
   */
  Array* sort(std::function<int(Value*, Value*)> compareFn = nullptr);

  /**
   * @brief 要素を検索してインデックスを返す
   * @param searchElement 検索する要素
   * @param fromIndex 検索開始インデックス
   * @return 見つかったインデックス、見つからない場合は-1
   */
  int32_t indexOf(Value* searchElement, int32_t fromIndex = 0) const;

  /**
   * @brief 要素を後方から検索してインデックスを返す
   * @param searchElement 検索する要素
   * @param fromIndex 検索開始インデックス
   * @return 見つかったインデックス、見つからない場合は-1
   */
  int32_t lastIndexOf(Value* searchElement, int32_t fromIndex = -1) const;

  /**
   * @brief 指定した条件を満たす要素が存在するかを確認
   * @param predicate 条件関数
   * @return 条件を満たす要素が存在する場合はtrue
   */
  bool some(std::function<bool(Value*, uint32_t, Array*)> predicate) const;

  /**
   * @brief すべての要素が指定した条件を満たすかを確認
   * @param predicate 条件関数
   * @return すべての要素が条件を満たす場合はtrue
   */
  bool every(std::function<bool(Value*, uint32_t, Array*)> predicate) const;

  /**
   * @brief 各要素に対して関数を実行
   * @param callback 実行する関数
   */
  void forEach(std::function<void(Value*, uint32_t, Array*)> callback) const;

  /**
   * @brief 各要素を変換した新しい配列を作成
   * @param callback 変換関数
   * @return 変換された新しい配列
   */
  Array* map(std::function<Value*(Value*, uint32_t, Array*)> callback) const;

  /**
   * @brief 条件を満たす要素のみを含む新しい配列を作成
   * @param predicate 条件関数
   * @return フィルタされた新しい配列
   */
  Array* filter(std::function<bool(Value*, uint32_t, Array*)> predicate) const;

  /**
   * @brief 配列を単一の値に縮約
   * @param callback 縮約関数
   * @param initialValue 初期値
   * @return 縮約された値
   */
  Value* reduce(std::function<Value*(Value*, Value*, uint32_t, Array*)> callback, Value* initialValue = nullptr) const;

  /**
   * @brief 配列を右から左に単一の値に縮約
   * @param callback 縮約関数
   * @param initialValue 初期値
   * @return 縮約された値
   */
  Value* reduceRight(std::function<Value*(Value*, Value*, uint32_t, Array*)> callback, Value* initialValue = nullptr) const;

  /**
   * @brief 条件を満たす最初の要素を検索
   * @param predicate 条件関数
   * @return 見つかった要素、見つからない場合はundefined
   */
  Value* find(std::function<bool(Value*, uint32_t, Array*)> predicate) const;

  /**
   * @brief 条件を満たす最初の要素のインデックスを検索
   * @param predicate 条件関数
   * @return 見つかったインデックス、見つからない場合は-1
   */
  int32_t findIndex(std::function<bool(Value*, uint32_t, Array*)> predicate) const;

  /**
   * @brief 配列に指定した要素が含まれているかを確認
   * @param searchElement 検索する要素
   * @param fromIndex 検索開始インデックス
   * @return 含まれている場合はtrue
   */
  bool includes(Value* searchElement, int32_t fromIndex = 0) const;

  /**
   * @brief 配列を平坦化
   * @param depth 平坦化の深度（省略時は1）
   * @return 平坦化された新しい配列
   */
  Array* flat(uint32_t depth = 1) const;

  /**
   * @brief 各要素をマップして平坦化
   * @param callback マップ関数
   * @param depth 平坦化の深度
   * @return マップ・平坦化された新しい配列
   */
  Array* flatMap(std::function<Value*(Value*, uint32_t, Array*)> callback, uint32_t depth = 1) const;

  /**
   * @brief 配列のすべての有効なインデックスを取得
   * @return 有効なインデックスのベクター
   */
  std::vector<uint32_t> getValidIndices() const;

  /**
   * @brief 配列が密な配列かどうかを確認
   * @return 密な配列の場合はtrue
   */
  bool isDense() const;

  /**
   * @brief 配列を文字列表現に変換
   * @return "[object Array]"
   */
  std::string toString() const override;

  // 便利なファクトリ関数
  static Array* create();
  static Array* create(uint32_t length);
  static Array* from(const std::vector<Value*>& elements);

 private:
  // 配列の実際のデータ格納
  std::vector<Value*> elements_;  // 密な部分
  std::unordered_map<uint32_t, Value*> sparse_;  // スパースな部分
  uint32_t length_;  // 配列の長さ

  // 内部ヘルパー関数
  void ensureCapacity(uint32_t minCapacity);
  void compactIfNeeded();
  bool isValidArrayIndex(uint32_t index) const;
  int32_t normalizeIndex(int32_t index, uint32_t length) const;
  void flattenHelper(Array* result, Value* element, uint32_t depth) const;

  // デフォルトの比較関数
  static int defaultCompare(Value* a, Value* b);
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_ARRAY_H 