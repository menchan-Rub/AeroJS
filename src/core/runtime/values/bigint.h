/**
 * @file bigint.h
 * @brief JavaScript BigInt 型の定義
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_BIGINT_H
#define AEROJS_BIGINT_H

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "src/utils/memory/smart_ptr/ref_counted.h"

namespace aerojs {
namespace core {

/**
 * @brief JavaScript BigInt 型を表現するクラス
 *
 * BigIntはJavaScriptの任意精度整数型です。
 * このクラスは内部的に符号と桁の配列で表現しています。
 */
class BigInt : public utils::RefCounted {
 public:
  // 内部的な基数 (メモリ効率と演算効率のバランスを考慮)
  static constexpr uint64_t DIGIT_BASE = UINT32_MAX;
  static constexpr size_t DIGIT_BITS = 32;

  // 桁を表す型
  using Digit = uint32_t;

  /**
   * @brief ゼロ値のBigIntを作成
   */
  BigInt();

  /**
   * @brief int64_t 値からBigIntを作成
   * @param value 整数値
   */
  explicit BigInt(int64_t value);

  /**
   * @brief uint64_t 値からBigIntを作成
   * @param value 符号なし整数値
   */
  explicit BigInt(uint64_t value);

  /**
   * @brief 文字列からBigIntを作成
   * @param str 整数を表す文字列
   * @param radix 基数 (2-36)
   */
  BigInt(const std::string& str, int radix = 10);

  /**
   * @brief コピーコンストラクタ
   * @param other コピー元のBigInt
   */
  BigInt(const BigInt& other);

  /**
   * @brief ムーブコンストラクタ
   * @param other ムーブ元のBigInt
   */
  BigInt(BigInt&& other) noexcept;

  /**
   * @brief コピー代入演算子
   * @param other コピー元のBigInt
   * @return このオブジェクトへの参照
   */
  BigInt& operator=(const BigInt& other);

  /**
   * @brief ムーブ代入演算子
   * @param other ムーブ元のBigInt
   * @return このオブジェクトへの参照
   */
  BigInt& operator=(BigInt&& other) noexcept;

  /**
   * @brief デストラクタ
   */
  ~BigInt() override = default;

  /**
   * @brief BigIntが等しいかどうかを比較
   * @param other 比較対象のBigInt
   * @return 等しい場合はtrue
   */
  bool equals(const BigInt& other) const;

  /**
   * @brief BigIntの比較 (<)
   * @param other 比較対象のBigInt
   * @return このBigIntがotherより小さい場合はtrue
   */
  bool lessThan(const BigInt& other) const;

  /**
   * @brief BigIntの比較 (<=)
   * @param other 比較対象のBigInt
   * @return このBigIntがother以下の場合はtrue
   */
  bool lessThanOrEqual(const BigInt& other) const;

  /**
   * @brief BigIntの比較 (>)
   * @param other 比較対象のBigInt
   * @return このBigIntがotherより大きい場合はtrue
   */
  bool greaterThan(const BigInt& other) const;

  /**
   * @brief BigIntの比較 (>=)
   * @param other 比較対象のBigInt
   * @return このBigIntがother以上の場合はtrue
   */
  bool greaterThanOrEqual(const BigInt& other) const;

  /**
   * @brief BigIntの加算
   * @param other 加算するBigInt
   * @return 新しいBigInt (this + other)
   */
  BigInt* add(const BigInt& other) const;

  /**
   * @brief BigIntの減算
   * @param other 減算するBigInt
   * @return 新しいBigInt (this - other)
   */
  BigInt* subtract(const BigInt& other) const;

  /**
   * @brief BigIntの乗算
   * @param other 乗算するBigInt
   * @return 新しいBigInt (this * other)
   */
  BigInt* multiply(const BigInt& other) const;

  /**
   * @brief BigIntの除算
   * @param other 除算するBigInt (0でないこと)
   * @return 新しいBigInt (this / other)
   */
  BigInt* divide(const BigInt& other) const;

  /**
   * @brief BigIntの剰余
   * @param other 除算するBigInt (0でないこと)
   * @return 新しいBigInt (this % other)
   */
  BigInt* remainder(const BigInt& other) const;

  /**
   * @brief BigIntの除算と剰余を同時に計算
   * @param other 除算するBigInt (0でないこと)
   * @param quotient 商を格納するポインタ
   * @param remainder 剰余を格納するポインタ
   */
  void divideAndRemainder(const BigInt& other, BigInt** quotient, BigInt** remainder) const;

  /**
   * @brief BigIntの累乗
   * @param exponent 累乗する数 (負でない整数)
   * @return 新しいBigInt (this ^ exponent)
   */
  BigInt* pow(uint64_t exponent) const;

  /**
   * @brief BigIntのビット単位AND演算
   * @param other AND演算するBigInt
   * @return 新しいBigInt (this & other)
   */
  BigInt* bitwiseAnd(const BigInt& other) const;

  /**
   * @brief BigIntのビット単位OR演算
   * @param other OR演算するBigInt
   * @return 新しいBigInt (this | other)
   */
  BigInt* bitwiseOr(const BigInt& other) const;

  /**
   * @brief BigIntのビット単位XOR演算
   * @param other XOR演算するBigInt
   * @return 新しいBigInt (this ^ other)
   */
  BigInt* bitwiseXor(const BigInt& other) const;

  /**
   * @brief BigIntのビット単位NOT演算
   * @return 新しいBigInt (~this)
   */
  BigInt* bitwiseNot() const;

  /**
   * @brief BigIntの左シフト
   * @param shift シフトするビット数
   * @return 新しいBigInt (this << shift)
   */
  BigInt* leftShift(uint64_t shift) const;

  /**
   * @brief BigIntの右シフト
   * @param shift シフトするビット数
   * @return 新しいBigInt (this >> shift)
   */
  BigInt* rightShift(uint64_t shift) const;

  /**
   * @brief BigIntの符号を反転
   * @return 新しいBigInt (-this)
   */
  BigInt* negate() const;

  /**
   * @brief BigIntの絶対値
   * @return 新しいBigInt (|this|)
   */
  BigInt* abs() const;

  /**
   * @brief BigIntを符号付き整数に変換
   * @return int64_t値 (値が範囲外の場合は飽和)
   */
  int64_t toInt64() const;

  /**
   * @brief BigIntを符号なし整数に変換
   * @return uint64_t値 (値が範囲外の場合は飽和)
   */
  uint64_t toUint64() const;

  /**
   * @brief BigIntを浮動小数点数に変換
   * @return double値 (精度が失われる可能性あり)
   */
  double toDouble() const;

  /**
   * @brief BigIntを指定の基数の文字列に変換
   * @param radix 基数 (2-36)
   * @return 文字列表現
   */
  std::string toString(int radix = 10) const;

  /**
   * @brief BigIntが0かどうかを確認
   * @return 0の場合はtrue
   */
  bool isZero() const;

  /**
   * @brief BigIntが1かどうかを確認
   * @return 1の場合はtrue
   */
  bool isOne() const;

  /**
   * @brief BigIntがマイナスかどうかを確認
   * @return 負の値の場合はtrue
   */
  bool isNegative() const;

  /**
   * @brief BigIntのビット長を取得
   * @return ビット長
   */
  size_t bitLength() const;

  /**
   * @brief BigIntの桁数を取得
   * @return 10進数での桁数
   */
  size_t digitLength() const;

  /**
   * @brief int64_t 値からBigIntを作成
   * @param value 整数値
   * @return 新しいBigInt
   */
  static BigInt* create(int64_t value);

  /**
   * @brief uint64_t 値からBigIntを作成
   * @param value 符号なし整数値
   * @return 新しいBigInt
   */
  static BigInt* create(uint64_t value);

  /**
   * @brief 文字列からBigIntを作成
   * @param str 整数を表す文字列
   * @param radix 基数 (2-36)
   * @return 新しいBigInt
   */
  static BigInt* create(const std::string& str, int radix = 10);

  /**
   * @brief BigIntのゼロ値を作成
   * @return ゼロのBigInt
   */
  static BigInt* zero();

  /**
   * @brief BigIntの1を作成
   * @return 1のBigInt
   */
  static BigInt* one();

  /**
   * @brief BigIntの-1を作成
   * @return -1のBigInt
   */
  static BigInt* negativeOne();

 private:
  // 符号 (正: true, 負: false)
  bool positive_;

  // 桁の配列 (各桁は0からDIGIT_BASE-1の値)
  // 最下位桁が先頭、最上位桁が末尾
  std::vector<Digit> digits_;

  // 内部操作のためのメソッド

  // ゼロでない最上位桁のインデックスを取得
  size_t mostSignificantDigit() const;

  // 冗長なゼロ桁を削除
  void removeLeadingZeros();

  // 符号と桁配列から構築
  BigInt(bool positive, std::vector<Digit> digits);

  // 2つのBigIntの絶対値を比較
  static int compareAbsolute(const BigInt& a, const BigInt& b);

  // 桁の絶対値の加算 (符号なし)
  static std::vector<Digit> addAbsolute(const std::vector<Digit>& a, const std::vector<Digit>& b);

  // 桁の絶対値の減算 (|a| >= |b| と仮定)
  static std::vector<Digit> subtractAbsolute(const std::vector<Digit>& a, const std::vector<Digit>& b);

  // 桁の絶対値の乗算
  static std::vector<Digit> multiplyAbsolute(const std::vector<Digit>& a, const std::vector<Digit>& b);

  // 桁の絶対値の除算と剰余
  static void divRemAbsolute(const std::vector<Digit>& a, const std::vector<Digit>& b,
                             std::vector<Digit>& quotient, std::vector<Digit>& remainder);

  // 数値文字を整数値に変換
  static int charToDigit(char c);

  // 整数値を数値文字に変換
  static char digitToChar(int d);
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_BIGINT_H