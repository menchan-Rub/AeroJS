/**
 * @file bigint.cpp
 * @brief BigInt クラスの実装
 * @version 0.1.0
 * @license MIT
 */

#include "src/core/runtime/values/bigint.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace aerojs {
namespace core {

// デフォルトコンストラクタ (0)
BigInt::BigInt()
    : positive_(true), digits_(1, 0) {
}

// int64_t からのコンストラクタ
BigInt::BigInt(int64_t value)
    : positive_(value >= 0) {
  uint64_t absValue = value >= 0 ? value : -static_cast<uint64_t>(value);

  if (absValue == 0) {
    digits_.push_back(0);
  } else {
    while (absValue > 0) {
      digits_.push_back(static_cast<Digit>(absValue & (DIGIT_BASE - 1)));
      absValue >>= DIGIT_BITS;
    }
  }
}

// uint64_t からのコンストラクタ
BigInt::BigInt(uint64_t value)
    : positive_(true) {
  if (value == 0) {
    digits_.push_back(0);
  } else {
    while (value > 0) {
      digits_.push_back(static_cast<Digit>(value & (DIGIT_BASE - 1)));
      value >>= DIGIT_BITS;
    }
  }
}

// 符号と桁からのコンストラクタ
BigInt::BigInt(bool positive, std::vector<Digit> digits)
    : positive_(positive), digits_(std::move(digits)) {
  removeLeadingZeros();

  // 0の場合は常に正にする
  if (isZero()) {
    positive_ = true;
  }
}

// 文字列からのコンストラクタ
BigInt::BigInt(const std::string& str, int radix)
    : positive_(true), digits_(1, 0) {
  if (radix < 2 || radix > 36) {
    throw std::invalid_argument("Radix out of range");
  }

  if (str.empty()) {
    return;  // 0として扱う
  }

  size_t startIdx = 0;

  // 符号の処理
  if (str[0] == '+') {
    positive_ = true;
    startIdx = 1;
  } else if (str[0] == '-') {
    positive_ = false;
    startIdx = 1;
  }

  // 文字列を基数に応じて処理
  BigInt result(0);
  BigInt radixBigInt(static_cast<int64_t>(radix));

  for (size_t i = startIdx; i < str.length(); ++i) {
    int digitValue = charToDigit(str[i]);

    if (digitValue == -1 || digitValue >= radix) {
      throw std::invalid_argument("Invalid digit for given radix");
    }

    // result = result * radix + digitValue
    BigInt* temp = result.multiply(radixBigInt);
    BigInt* temp2 = temp->add(BigInt(static_cast<int64_t>(digitValue)));

    // 古い値を適切に削除
    delete temp;

    // 結果を保存
    result = *temp2;
    delete temp2;
  }

  positive_ = result.positive_;
  digits_ = std::move(result.digits_);

  // 0の場合は常に正にする
  if (isZero()) {
    positive_ = true;
  }
}

// コピーコンストラクタ
BigInt::BigInt(const BigInt& other)
    : positive_(other.positive_), digits_(other.digits_) {
}

// ムーブコンストラクタ
BigInt::BigInt(BigInt&& other) noexcept
    : positive_(other.positive_), digits_(std::move(other.digits_)) {
}

// コピー代入演算子
BigInt& BigInt::operator=(const BigInt& other) {
  if (this != &other) {
    positive_ = other.positive_;
    digits_ = other.digits_;
  }
  return *this;
}

// ムーブ代入演算子
BigInt& BigInt::operator=(BigInt&& other) noexcept {
  if (this != &other) {
    positive_ = other.positive_;
    digits_ = std::move(other.digits_);
  }
  return *this;
}

// 等値比較
bool BigInt::equals(const BigInt& other) const {
  // 0の場合は符号を無視
  if (isZero() && other.isZero()) {
    return true;
  }

  // 符号が異なる場合は等しくない
  if (positive_ != other.positive_) {
    return false;
  }

  // 桁数が異なる場合は等しくない
  if (digits_.size() != other.digits_.size()) {
    return false;
  }

  // 各桁を比較
  return digits_ == other.digits_;
}

// 比較演算子 <
bool BigInt::lessThan(const BigInt& other) const {
  // 符号が異なる場合
  if (positive_ != other.positive_) {
    return !positive_;  // 負の値は正の値より小さい
  }

  // 符号が同じ場合
  int cmp = compareAbsolute(*this, other);

  // 絶対値の比較結果を符号に応じて解釈
  if (positive_) {
    return cmp < 0;  // 正の値: 絶対値が小さいほど全体も小さい
  } else {
    return cmp > 0;  // 負の値: 絶対値が大きいほど全体は小さい
  }
}

// 比較演算子 <=
bool BigInt::lessThanOrEqual(const BigInt& other) const {
  return lessThan(other) || equals(other);
}

// 比較演算子 >
bool BigInt::greaterThan(const BigInt& other) const {
  return !lessThanOrEqual(other);
}

// 比較演算子 >=
bool BigInt::greaterThanOrEqual(const BigInt& other) const {
  return !lessThan(other);
}

// BigIntが0かどうかを確認
bool BigInt::isZero() const {
  return digits_.size() == 1 && digits_[0] == 0;
}

// BigIntが1かどうかを確認
bool BigInt::isOne() const {
  return positive_ && digits_.size() == 1 && digits_[0] == 1;
}

// BigIntがマイナスかどうかを確認
bool BigInt::isNegative() const {
  return !positive_ && !isZero();
}

// BigIntのビット長を取得
size_t BigInt::bitLength() const {
  if (isZero()) {
    return 0;
  }

  // 最上位桁のビット長 + 下位桁の合計ビット数
  size_t msd = mostSignificantDigit();
  Digit topDigit = digits_[msd];

  // 最上位桁のビット長を計算
  size_t bitLen = 0;
  while (topDigit > 0) {
    topDigit >>= 1;
    bitLen++;
  }

  // 下位桁のビット数を追加
  return bitLen + msd * DIGIT_BITS;
}

// 10進数での桁数を取得
size_t BigInt::digitLength() const {
  if (isZero()) {
    return 1;
  }

  // log10(n) + 1 で桁数を概算
  double approximateLog10 = bitLength() * std::log10(2);
  return static_cast<size_t>(approximateLog10) + 1;
}

// int64_tに変換 (範囲外の場合は飽和)
int64_t BigInt::toInt64() const {
  constexpr size_t INT64_BITS = sizeof(int64_t) * 8;

  if (isZero()) {
    return 0;
  }

  // ビット長がint64_tの範囲を超える場合は飽和
  if (bitLength() > INT64_BITS - 1) {
    if (positive_) {
      return std::numeric_limits<int64_t>::max();
    } else {
      return std::numeric_limits<int64_t>::min();
    }
  }

  // int64_tの範囲内の場合は正確に変換
  int64_t result = 0;
  for (size_t i = digits_.size(); i-- > 0;) {
    result = (result << DIGIT_BITS) | digits_[i];
  }

  return positive_ ? result : -result;
}

// uint64_tに変換 (範囲外の場合は飽和)
uint64_t BigInt::toUint64() const {
  constexpr size_t UINT64_BITS = sizeof(uint64_t) * 8;

  if (isZero()) {
    return 0;
  }

  // 負の値の場合は0
  if (!positive_) {
    return 0;
  }

  // ビット長がuint64_tの範囲を超える場合は飽和
  if (bitLength() > UINT64_BITS) {
    return std::numeric_limits<uint64_t>::max();
  }

  // uint64_tの範囲内の場合は正確に変換
  uint64_t result = 0;
  for (size_t i = digits_.size(); i-- > 0;) {
    result = (result << DIGIT_BITS) | digits_[i];
  }

  return result;
}

// doubleに変換 (精度が失われる可能性あり)
double BigInt::toDouble() const {
  if (isZero()) {
    return 0.0;
  }
  
  if (positive_) {
    // 正の値の場合
    double result = 0.0;
    double power = 1.0;
    
    // 各ブロックを順に変換（リトルエンディアン形式）
    for (size_t i = 0; i < digits_.size(); ++i) {
      result += static_cast<double>(digits_[i]) * power;
      power *= static_cast<double>(DIGIT_BASE);
      
      // 浮動小数点の精度を超えるとこれ以上の桁は意味がない
      if (power > DBL_MAX) {
        break;
      }
    }
    
    return result;
  } else {
    // 負の値の場合は正の値として計算してから符号を反転
    BigInt positive(*this);
    positive.positive_ = true;
    return -positive.toDouble();
  }
}

// 文字列に変換
std::string BigInt::toString(int radix) const {
  if (radix < 2 || radix > 36) {
    throw std::invalid_argument("Radix out of range");
  }

  if (isZero()) {
    return "0";
  }

  // 10進数の場合の簡易実装
  if (radix == 10) {
    std::string result;
    BigInt quotient = *this;
    bool isNeg = !quotient.positive_;

    if (isNeg) {
      quotient.positive_ = true;  // 絶対値を使用
    }

    BigInt radixBigInt(static_cast<int64_t>(radix));

    while (!quotient.isZero()) {
      BigInt* remainder;
      BigInt* newQuotient;
      quotient.divideAndRemainder(radixBigInt, &newQuotient, &remainder);

      int digit = remainder->toInt64();
      result.push_back(digitToChar(digit));

      delete remainder;
      BigInt tempQuotient = *newQuotient;
      delete newQuotient;
      quotient = std::move(tempQuotient);
    }

    if (isNeg) {
      result.push_back('-');
    }

    std::reverse(result.begin(), result.end());
    return result;
  }

  // その他の基数の場合も同様の処理
  std::string result;
  BigInt temp = *this;
  bool isNeg = !temp.positive_;

  if (isNeg) {
    temp.positive_ = true;  // 絶対値を使用
  }

  BigInt radixBigInt(static_cast<int64_t>(radix));

  while (!temp.isZero()) {
    BigInt* remainder;
    BigInt* newQuotient;
    temp.divideAndRemainder(radixBigInt, &newQuotient, &remainder);

    int digit = remainder->toInt64();
    result.push_back(digitToChar(digit));

    delete remainder;
    BigInt tempQuotient = *newQuotient;
    delete newQuotient;
    temp = std::move(tempQuotient);
  }

  if (isNeg) {
    result.push_back('-');
  }

  std::reverse(result.begin(), result.end());
  return result;
}

// 静的ファクトリメソッド
BigInt* BigInt::create(int64_t value) {
  return new BigInt(value);
}

BigInt* BigInt::create(uint64_t value) {
  return new BigInt(value);
}

BigInt* BigInt::create(const std::string& str, int radix) {
  return new BigInt(str, radix);
}

BigInt* BigInt::zero() {
  return new BigInt();
}

BigInt* BigInt::one() {
  return new BigInt(1);
}

BigInt* BigInt::negativeOne() {
  return new BigInt(-1);
}

// プライベートヘルパーメソッド

// ゼロでない最上位桁のインデックスを取得
size_t BigInt::mostSignificantDigit() const {
  if (isZero()) {
    return 0;
  }

  for (size_t i = digits_.size() - 1; i > 0; --i) {
    if (digits_[i] != 0) {
      return i;
    }
  }

  return 0;
}

// 冗長なゼロ桁を削除
void BigInt::removeLeadingZeros() {
  while (digits_.size() > 1 && digits_.back() == 0) {
    digits_.pop_back();
  }
}

// 数値文字を整数値に変換
int BigInt::charToDigit(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'z') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'Z') {
    return c - 'A' + 10;
  }
  return -1;  // 無効な文字
}

// 整数値を数値文字に変換
char BigInt::digitToChar(int d) {
  if (d >= 0 && d < 10) {
    return '0' + d;
  }
  if (d >= 10 && d < 36) {
    return 'a' + (d - 10);
  }
  return '?';  // 無効な値
}

// 2つのBigIntの絶対値を比較
int BigInt::compareAbsolute(const BigInt& a, const BigInt& b) {
  // 桁数の比較
  if (a.digits_.size() < b.digits_.size()) {
    return -1;
  }
  if (a.digits_.size() > b.digits_.size()) {
    return 1;
  }

  // 桁数が同じ場合は上の桁から比較
  for (size_t i = a.digits_.size(); i-- > 0;) {
    if (a.digits_[i] < b.digits_[i]) {
      return -1;
    }
    if (a.digits_[i] > b.digits_[i]) {
      return 1;
    }
  }

  // 全ての桁が等しい
  return 0;
}

}  // namespace core
}  // namespace aerojs