/**
 * @file constant_pool.h
 * @brief 定数プールのヘッダーファイル
 *
 * このファイルは、バイトコードモジュールで使用される定数値を管理する定数プールクラスを定義します。
 * 定数プールは文字列、数値、真偽値などのリテラル値を格納し、バイトコード内では
 * これらの値への参照をインデックスとして扱います。
 */

#ifndef AEROJS_CORE_VM_BYTECODE_CONSTANT_POOL_H_
#define AEROJS_CORE_VM_BYTECODE_CONSTANT_POOL_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "../../runtime/values/value.h"

namespace aerojs {
namespace core {

/**
 * @brief 定数プールクラス
 *
 * このクラスは、バイトコードモジュールで使用される定数値を管理します。
 * 重複する定数値を避けるための最適化機能も提供します。
 */
class ConstantPool {
 public:
  /**
   * @brief コンストラクタ
   *
   * @param enable_deduplication 定数の重複排除を有効にするかどうか
   */
  explicit ConstantPool(bool enable_deduplication = true);

  /**
   * @brief デストラクタ
   */
  ~ConstantPool();

  /**
   * @brief 定数値を追加
   *
   * @param value 追加する値
   * @return 定数のインデックス
   */
  uint32_t addConstant(const Value& value);

  /**
   * @brief 文字列定数を追加
   *
   * @param str 追加する文字列
   * @return 定数のインデックス
   */
  uint32_t addString(const std::string& str);

  /**
   * @brief 数値定数を追加
   *
   * @param number 追加する数値
   * @return 定数のインデックス
   */
  uint32_t addNumber(double number);

  /**
   * @brief 整数定数を追加
   *
   * @param integer 追加する整数
   * @return 定数のインデックス
   */
  uint32_t addInteger(int32_t integer);

  /**
   * @brief BigInt定数を追加
   *
   * @param bigint 追加するBigInt
   * @return 定数のインデックス
   */
  uint32_t addBigInt(const std::string& bigint);

  /**
   * @brief 真偽値定数を追加
   *
   * @param boolean 追加する真偽値
   * @return 定数のインデックス
   */
  uint32_t addBoolean(bool boolean);

  /**
   * @brief null定数を追加
   *
   * @return 定数のインデックス
   */
  uint32_t addNull();

  /**
   * @brief undefined定数を追加
   *
   * @return 定数のインデックス
   */
  uint32_t addUndefined();

  /**
   * @brief 正規表現定数を追加
   *
   * @param pattern 正規表現パターン
   * @param flags 正規表現フラグ
   * @return 定数のインデックス
   */
  uint32_t addRegExp(const std::string& pattern, const std::string& flags);

  /**
   * @brief 関数定数を追加
   *
   * @param function_index 関数のインデックス
   * @return 定数のインデックス
   */
  uint32_t addFunction(uint32_t function_index);

  /**
   * @brief 指定インデックスの定数値を取得
   *
   * @param index 取得する定数のインデックス
   * @return 定数値
   * @throws std::out_of_range インデックスが範囲外の場合
   */
  const Value& getConstant(uint32_t index) const;

  /**
   * @brief 定数値のインデックスを取得
   *
   * @param value 検索する値
   * @return 値が存在する場合はそのインデックス、存在しない場合はUINT32_MAX
   */
  uint32_t findConstant(const Value& value) const;

  /**
   * @brief 定数プールの全定数を取得
   *
   * @return 定数値の配列
   */
  const std::vector<Value>& getConstants() const;

  /**
   * @brief 定数プールの定数数を取得
   *
   * @return 定数の数
   */
  size_t size() const;

  /**
   * @brief 定数プールをシリアライズ
   *
   * @return シリアライズされたバイナリデータ
   */
  std::vector<uint8_t> serialize() const;

  /**
   * @brief バイナリデータから定数プールをデシリアライズ
   *
   * @param data デシリアライズするバイナリデータ
   * @return デシリアライズされた定数プール
   */
  static std::unique_ptr<ConstantPool> deserialize(const std::vector<uint8_t>& data);

  /**
   * @brief 定数プールの内容をダンプ（デバッグ用）
   *
   * @param output 出力ストリーム
   */
  void dump(std::ostream& output) const;

 private:
  /**
   * @brief 重複排除が有効な場合に、既存の定数のインデックスを探す
   *
   * @param value 検索する値
   * @return 値が存在する場合はそのインデックス、存在しない場合はUINT32_MAX
   */
  uint32_t findDuplicateConstant(const Value& value) const;

  std::vector<Value> m_constants;             ///< 定数値の配列
  std::unordered_map<Value, uint32_t> m_map;  ///< 値からインデックスへのマップ（重複排除用）
  bool m_enableDeduplication;                 ///< 重複排除を有効にするかどうか
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_VM_BYTECODE_CONSTANT_POOL_H_