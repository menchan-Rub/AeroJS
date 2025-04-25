/**
 * @file bigint.cpp
 * @brief JavaScriptのBigInt組み込みオブジェクトの実装
 * @copyright Copyright (c) 2023 AeroJS project authors
 */

#include "core/runtime/builtins/bigint/bigint.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "core/runtime/context.h"
#include "core/runtime/execution_state.h"
#include "core/runtime/value.h"

namespace aero {

// BigIntクラスの実装

BigInt::BigInt()
    : m_sign(0), m_digits() {
  // 0を表すBigIntは空のディジット配列
}

BigInt::BigInt(int64_t value)
    : m_digits() {
  if (value == 0) {
    m_sign = 0;
    return;
  }

  m_sign = (value < 0) ? -1 : 1;
  uint64_t absValue = (value < 0) ? static_cast<uint64_t>(-value) : static_cast<uint64_t>(value);

  // 32ビット単位でディジットに分解
  m_digits.push_back(static_cast<uint32_t>(absValue & 0xFFFFFFFF));
  if (absValue > 0xFFFFFFFF) {
    m_digits.push_back(static_cast<uint32_t>(absValue >> 32));
  }
}

BigInt::BigInt(const std::string& str, int radix)
    : m_sign(0), m_digits() {
  if (radix < 2 || radix > 36) {
    throw std::invalid_argument("基数は2から36の範囲内である必要があります");
  }

  std::string trimmed = str;
  // 先頭の空白を削除
  trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r\f\v"));

  // 符号の処理
  bool negative = false;
  if (!trimmed.empty() && trimmed[0] == '-') {
    negative = true;
    trimmed.erase(0, 1);
  } else if (!trimmed.empty() && trimmed[0] == '+') {
    trimmed.erase(0, 1);
  }

  if (trimmed.empty()) {
    throw std::invalid_argument("無効なBigInt文字列です");
  }

  // 末尾のnがある場合は削除（JavaScriptのBigInt表記）
  if (trimmed.back() == 'n') {
    trimmed.pop_back();
  }

  // 数値をパース
  *this = BigInt(0);
  BigInt base(radix);
  for (char c : trimmed) {
    int digit;
    if (c >= '0' && c <= '9') {
      digit = c - '0';
    } else if (c >= 'a' && c <= 'z') {
      digit = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'Z') {
      digit = c - 'A' + 10;
    } else {
      throw std::invalid_argument("無効な文字が含まれています");
    }

    if (digit >= radix) {
      throw std::invalid_argument("基数の範囲外の文字が含まれています");
    }

    // this = this * base + digit
    *this = multiply(base);
    *this = add(BigInt(digit));
  }

  if (negative && !isZero()) {
    m_sign = -1;
  }
}

BigInt::BigInt(const BigInt& other)
    : m_sign(other.m_sign), m_digits(other.m_digits) {
}

BigInt::BigInt(BigInt&& other) noexcept
    : m_sign(other.m_sign), m_digits(std::move(other.m_digits)) {
  other.m_sign = 0;
}

BigInt::~BigInt() {
}

BigInt& BigInt::operator=(const BigInt& other) {
  if (this != &other) {
    m_sign = other.m_sign;
    m_digits = other.m_digits;
  }
  return *this;
}

BigInt& BigInt::operator=(BigInt&& other) noexcept {
  if (this != &other) {
    m_sign = other.m_sign;
    m_digits = std::move(other.m_digits);
    other.m_sign = 0;
  }
  return *this;
}

int BigInt::sign() const {
  return m_sign;
}

const BigInt::Digits& BigInt::digits() const {
  return m_digits;
}

void BigInt::normalize() {
  // 先頭のゼロディジットを削除
  while (!m_digits.empty() && m_digits.back() == 0) {
    m_digits.pop_back();
  }

  // ディジットがすべて0の場合、符号も0に
  if (m_digits.empty()) {
    m_sign = 0;
  }
}

void BigInt::padZero(size_t numDigits) {
  size_t origSize = m_digits.size();
  if (numDigits > origSize) {
    m_digits.resize(numDigits, 0);
  }
}

std::string BigInt::toString() const {
  return toString(10);
}

std::string BigInt::toString(int radix) const {
  if (radix < 2 || radix > 36) {
    throw std::invalid_argument("基数は2から36の範囲内である必要があります");
  }

  if (isZero()) {
    return "0";
  }

  // 10進数表記で特に最適化
  if (radix == 10) {
    std::stringstream result;
    if (m_sign < 0) {
      result << "-";
    }

    // 10進法変換用の一時的なBigInt
    BigInt temp(*this);
    if (temp.m_sign < 0) {
      temp.m_sign = 1;  // 絶対値を使用
    }

    std::vector<uint32_t> decimalDigits;
    BigInt zero(0);
    BigInt ten(10);

    while (temp.compareTo(zero) > 0) {
      BigInt remainder = temp.remainder(ten);
      decimalDigits.push_back(remainder.isZero() ? 0 : remainder.m_digits[0]);
      temp = temp.divide(ten);
    }

    // 結果を逆順に
    for (auto it = decimalDigits.rbegin(); it != decimalDigits.rend(); ++it) {
      result << *it;
    }

    return result.str();
  }

  // その他の基数の場合
  std::string result;
  BigInt temp(*this);
  if (temp.m_sign < 0) {
    temp.m_sign = 1;  // 絶対値を使用
  }

  BigInt zero(0);
  BigInt base(radix);

  while (temp.compareTo(zero) > 0) {
    BigInt remainder = temp.remainder(base);
    int digit = remainder.isZero() ? 0 : remainder.m_digits[0];

    char digitChar;
    if (digit < 10) {
      digitChar = '0' + digit;
    } else {
      digitChar = 'a' + (digit - 10);
    }

    result.push_back(digitChar);
    temp = temp.divide(base);
  }

  // 符号を追加
  if (m_sign < 0) {
    result.push_back('-');
  }

  // 結果を逆順に
  std::reverse(result.begin(), result.end());

  return result;
}

bool BigInt::equals(const BigInt& other) const {
  if (m_sign != other.m_sign) {
    return false;
  }

  if (isZero() && other.isZero()) {
    return true;
  }

  if (m_digits.size() != other.m_digits.size()) {
    return false;
  }

  return std::equal(m_digits.begin(), m_digits.end(), other.m_digits.begin());
}

int BigInt::compareTo(const BigInt& other) const {
  // 符号が異なる場合
  if (m_sign < other.m_sign) {
    return -1;
  }
  if (m_sign > other.m_sign) {
    return 1;
  }

  // 両方ゼロの場合
  if (isZero() && other.isZero()) {
    return 0;
  }

  // 同じ符号の場合、絶対値を比較
  if (m_sign == 0) {
    return 0;
  }

  const size_t thisSize = m_digits.size();
  const size_t otherSize = other.m_digits.size();

  if (thisSize != otherSize) {
    return (thisSize > otherSize) ? m_sign : -m_sign;
  }

  // 同じ桁数の場合、各桁を上から比較
  for (int i = static_cast<int>(thisSize) - 1; i >= 0; --i) {
    if (m_digits[i] != other.m_digits[i]) {
      return (m_digits[i] > other.m_digits[i]) ? m_sign : -m_sign;
    }
  }

  return 0;  // 完全に同じ値
}

BigInt BigInt::add(const BigInt& other) const {
  // どちらかがゼロの場合
  if (isZero()) {
    return other;
  }
  if (other.isZero()) {
    return *this;
  }

  // 符号が異なる場合は減算に変換
  if (m_sign == -other.m_sign) {
    BigInt absThis(*this);
    BigInt absOther(other);
    absThis.m_sign = absOther.m_sign = 1;

    if (absThis.compareTo(absOther) >= 0) {
      // |this| >= |other|
      BigInt result = absThis.subtract(absOther);
      result.m_sign = isZero() ? 0 : m_sign;
      return result;
    } else {
      // |this| < |other|
      BigInt result = absOther.subtract(absThis);
      result.m_sign = other.isZero() ? 0 : other.m_sign;
      return result;
    }
  }

  // 同符号の加算
  BigInt result;
  result.m_sign = m_sign;  // 結果の符号は同じ

  const Digits& a = m_digits;
  const Digits& b = other.m_digits;
  const size_t maxSize = std::max(a.size(), b.size());

  result.m_digits.reserve(maxSize + 1);
  result.m_digits.resize(maxSize, 0);

  uint64_t carry = 0;
  for (size_t i = 0; i < maxSize; ++i) {
    uint64_t sum = carry;
    if (i < a.size()) {
      sum += a[i];
    }
    if (i < b.size()) {
      sum += b[i];
    }

    result.m_digits[i] = static_cast<uint32_t>(sum & 0xFFFFFFFF);
    carry = sum >> 32;
  }

  if (carry > 0) {
    result.m_digits.push_back(static_cast<uint32_t>(carry));
  }

  result.normalize();
  return result;
}

BigInt BigInt::subtract(const BigInt& other) const {
  // どちらかがゼロの場合
  if (other.isZero()) {
    return *this;
  }
  if (isZero()) {
    BigInt result(other);
    result.m_sign = -other.m_sign;
    return result;
  }

  // 符号が異なる場合は加算に変換
  if (m_sign == -other.m_sign) {
    BigInt result = add(BigInt(-other.m_sign, other.m_digits));
    return result;
  }

  // 同符号の減算
  int compareResult = compareTo(other);
  if (compareResult == 0) {
    return BigInt();  // 結果はゼロ
  }

  BigInt result;

  // |this| < |other| の場合、引く順序を逆にして符号を反転
  if ((m_sign == 1 && compareResult < 0) || (m_sign == -1 && compareResult > 0)) {
    // a - b = -(b - a)
    result = other.subtract(*this);
    result.m_sign = -result.m_sign;
    return result;
  }

  // |this| > |other| の場合、通常の減算
  const Digits& a = m_digits;
  const Digits& b = other.m_digits;

  result.m_digits.resize(a.size(), 0);
  result.m_sign = m_sign;

  int64_t borrow = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    int64_t diff = static_cast<int64_t>(a[i]) - borrow;
    if (i < b.size()) {
      diff -= b[i];
    }

    if (diff < 0) {
      diff += 0x100000000LL;  // 2^32を足す
      borrow = 1;
    } else {
      borrow = 0;
    }

    result.m_digits[i] = static_cast<uint32_t>(diff);
  }

  result.normalize();
  return result;
}

BigInt BigInt::multiply(const BigInt& other) const {
  // どちらかがゼロの場合
  if (isZero() || other.isZero()) {
    return BigInt();
  }

  // 結果の符号
  int resultSign = m_sign * other.m_sign;

  const Digits& a = m_digits;
  const Digits& b = other.m_digits;
  const size_t resultSize = a.size() + b.size();

  BigInt result;
  result.m_sign = resultSign;
  result.m_digits.resize(resultSize, 0);

  for (size_t i = 0; i < a.size(); ++i) {
    uint64_t carry = 0;
    for (size_t j = 0; j < b.size(); ++j) {
      uint64_t product = static_cast<uint64_t>(a[i]) * b[j] + result.m_digits[i + j] + carry;
      result.m_digits[i + j] = static_cast<uint32_t>(product & 0xFFFFFFFF);
      carry = product >> 32;
    }

    if (carry > 0) {
      result.m_digits[i + b.size()] += static_cast<uint32_t>(carry);
    }
  }

  result.normalize();
  return result;
}

// 引数のBigIntが0でないことを確認し、0の場合は例外をスロー
void checkDivisionByZero(const BigInt& divisor) {
  if (divisor.isZero()) {
    throw std::invalid_argument("ゼロによる除算はできません");
  }
}

// 除算と剰余の計算を行う関数
// 効率的な二分探索ベースのアルゴリズムを使用
std::pair<BigInt, BigInt> divideAndRemainder(const BigInt& dividend, const BigInt& divisor) {
  checkDivisionByZero(divisor);

  // ゼロの場合
  if (dividend.isZero()) {
    return {BigInt(), BigInt()};
  }

  // 符号の処理
  int quotientSign = dividend.sign() * divisor.sign();

  // 絶対値で計算
  BigInt absDividend(dividend);
  BigInt absDivisor(divisor);
  absDividend.m_sign = absDivisor.m_sign = 1;

  // |dividend| < |divisor| の場合
  if (absDividend.compareTo(absDivisor) < 0) {
    return {BigInt(), BigInt(dividend)};  // 商は0、余りは被除数
  }

  // 効率的な除算アルゴリズム（Knuth's Algorithm D の変形版）
  const size_t n = absDivisor.m_digits.size();
  const size_t m = absDividend.m_digits.size() - n;

  // 正規化（最上位桁を最大化するためのスケーリング）
  int normalizationShift = 0;
  uint32_t divisorMSB = absDivisor.m_digits.back();
  while ((divisorMSB & 0x80000000) == 0) {
    divisorMSB <<= 1;
    normalizationShift++;
  }

  BigInt normalizedDividend = absDividend.leftShift(normalizationShift);
  BigInt normalizedDivisor = absDivisor.leftShift(normalizationShift);

  // 商の初期化
  BigInt quotient;
  quotient.m_digits.resize(m + 1, 0);

  // 除数の最上位桁
  const uint32_t divisorHighDigit = normalizedDivisor.m_digits.back();
  const uint64_t divisorHighDigitPlusOne = (n > 1) ? normalizedDivisor.m_digits[n - 2] : 0;

  // 被除数から除数を引いていく
  BigInt remainder = normalizedDividend;

  for (int j = m; j >= 0; j--) {
    // 商の推定値を計算
    uint64_t dividend_high = 0;
    if (j + n < remainder.m_digits.size()) {
      dividend_high = static_cast<uint64_t>(remainder.m_digits[j + n]) << 32;
    }

    if (j + n - 1 < remainder.m_digits.size()) {
      dividend_high |= remainder.m_digits[j + n - 1];
    }

    uint64_t qEstimate = dividend_high / divisorHighDigit;
    uint64_t rEstimate = dividend_high % divisorHighDigit;

    // qEstimateの調整
    while (qEstimate >= (1ULL << 32) ||
           (qEstimate * divisorHighDigitPlusOne > ((rEstimate << 32) +
                                                   (j + n - 2 < remainder.m_digits.size() ? remainder.m_digits[j + n - 2] : 0)))) {
      qEstimate--;
      rEstimate += divisorHighDigit;
      if (rEstimate >= (1ULL << 32)) break;
    }

    if (qEstimate == 0) continue;

    // D4: 商の推定値を使って乗算と減算を行う
    BigInt product;
    product.m_sign = 1;
    product.m_digits.resize(n + 1, 0);

    uint64_t carry = 0;
    for (size_t i = 0; i < n; i++) {
      uint64_t p = qEstimate * normalizedDivisor.m_digits[i] + carry;
      product.m_digits[i] = static_cast<uint32_t>(p & 0xFFFFFFFF);
      carry = p >> 32;
    }
    product.m_digits[n] = static_cast<uint32_t>(carry);

    // 左にjだけシフト
    BigInt shiftedProduct;
    shiftedProduct.m_sign = 1;
    shiftedProduct.m_digits.resize(n + j + 1, 0);
    for (size_t i = 0; i < product.m_digits.size(); i++) {
      shiftedProduct.m_digits[i + j] = product.m_digits[i];
    }

    // 減算と調整
    if (remainder.compareTo(shiftedProduct) < 0) {
      qEstimate--;
      // 調整された商で再計算
      product.m_digits.clear();
      product.m_digits.resize(n + 1, 0);

      carry = 0;
      for (size_t i = 0; i < n; i++) {
        uint64_t p = qEstimate * normalizedDivisor.m_digits[i] + carry;
        product.m_digits[i] = static_cast<uint32_t>(p & 0xFFFFFFFF);
        carry = p >> 32;
      }
      product.m_digits[n] = static_cast<uint32_t>(carry);

      shiftedProduct.m_digits.clear();
      shiftedProduct.m_digits.resize(n + j + 1, 0);
      for (size_t i = 0; i < product.m_digits.size(); i++) {
        shiftedProduct.m_digits[i + j] = product.m_digits[i];
      }
    }

    // D5: 商の桁を設定
    quotient.m_digits[j] = static_cast<uint32_t>(qEstimate);

    // D6: 残りを計算
    remainder = remainder.subtract(shiftedProduct);
  }

  // 正規化を元に戻す
  remainder = remainder.rightShift(normalizationShift);

  // 不要な0を削除
  quotient.normalize();
  remainder.normalize();

  // 符号の設定
  quotient.m_sign = (quotient.isZero() ? 0 : quotientSign);
  remainder.m_sign = (remainder.isZero() ? 0 : dividend.sign());

  return {quotient, remainder};
}

BigInt BigInt::divide(const BigInt& other) const {
  return divideAndRemainder(*this, other).first;
}

BigInt BigInt::remainder(const BigInt& other) const {
  return divideAndRemainder(*this, other).second;
}

BigInt BigInt::leftShift(int64_t bits) const {
  if (bits < 0) {
    return rightShift(-bits);
  }

  if (isZero() || bits == 0) {
    return *this;
  }

  BigInt result(*this);

  // ビットシフトを32ビット単位のシフトと32ビット未満のシフトに分解
  uint64_t digitShift = bits / 32;
  uint64_t bitShift = bits % 32;

  if (bitShift == 0) {
    // 32ビット単位のシフトのみ
    result.m_digits.insert(result.m_digits.begin(), digitShift, 0);
  } else {
    // 32ビット未満のシフトがある場合
    uint64_t carry = 0;
    for (size_t i = 0; i < result.m_digits.size(); ++i) {
      uint64_t val = static_cast<uint64_t>(result.m_digits[i]) << bitShift;
      result.m_digits[i] = static_cast<uint32_t>((val & 0xFFFFFFFF) | carry);
      carry = val >> 32;
    }

    if (carry > 0) {
      result.m_digits.push_back(static_cast<uint32_t>(carry));
    }

    // 32ビット単位のシフト
    result.m_digits.insert(result.m_digits.begin(), digitShift, 0);
  }

  return result;
}

BigInt BigInt::rightShift(int64_t bits) const {
  if (bits < 0) {
    return leftShift(-bits);
  }

  if (isZero() || bits == 0) {
    return *this;
  }

  // 全体をシフトアウトする場合
  uint64_t digitShift = bits / 32;
  if (digitShift >= m_digits.size()) {
    // 符号を保持したゼロを返す
    BigInt result;
    result.m_sign = m_sign;
    return result;
  }

  BigInt result(*this);

  // 32ビット単位のシフト
  result.m_digits.erase(result.m_digits.begin(), result.m_digits.begin() + digitShift);

  // 32ビット未満のシフト
  uint64_t bitShift = bits % 32;
  if (bitShift > 0) {
    uint64_t mask = (1ULL << bitShift) - 1;
    uint64_t borrow = 0;

    for (int i = static_cast<int>(result.m_digits.size()) - 1; i >= 0; --i) {
      uint64_t val = result.m_digits[i];
      result.m_digits[i] = static_cast<uint32_t>((val >> bitShift) | (borrow << (32 - bitShift)));
      borrow = val & mask;
    }
  }

  result.normalize();
  return result;
}

BigInt BigInt::bitwiseAnd(const BigInt& other) const {
  if (isZero() || other.isZero()) {
    return BigInt();  // 0 AND x = 0
  }

  // 2の補数表現での処理
  if (m_sign < 0 || other.m_sign < 0) {
    // 負の数の2の補数表現を処理
    BigInt a = m_sign < 0 ? bitwiseNot().add(BigInt(1)) : *this;
    BigInt b = other.m_sign < 0 ? other.bitwiseNot().add(BigInt(1)) : other;

    BigInt result = a.bitwiseAndPositive(b);

    // 結果の符号を決定
    if (m_sign < 0 && other.m_sign < 0) {
      // 両方負の場合、結果も負
      result = result.bitwiseNot().add(BigInt(1));
      result.m_sign = -1;
    }

    return result;
  }

  return bitwiseAndPositive(other);
}

BigInt BigInt::bitwiseAndPositive(const BigInt& other) const {
  BigInt result;
  const size_t minSize = std::min(m_digits.size(), other.m_digits.size());
  result.m_digits.resize(minSize);

  for (size_t i = 0; i < minSize; ++i) {
    result.m_digits[i] = m_digits[i] & other.m_digits[i];
  }

  result.normalize();
  return result;
}

BigInt BigInt::bitwiseOr(const BigInt& other) const {
  if (isZero()) {
    return other;  // 0 OR x = x
  }
  if (other.isZero()) {
    return *this;  // x OR 0 = x
  }

  // 2の補数表現での処理
  if (m_sign < 0 || other.m_sign < 0) {
    // 負の数の2の補数表現を処理
    BigInt a = m_sign < 0 ? bitwiseNot().add(BigInt(1)) : *this;
    BigInt b = other.m_sign < 0 ? other.bitwiseNot().add(BigInt(1)) : other;

    BigInt result = a.bitwiseOrPositive(b);

    // 結果の符号を決定
    if (m_sign < 0 || other.m_sign < 0) {
      // どちらかが負の場合、結果も負
      result = result.bitwiseNot().add(BigInt(1));
      result.m_sign = -1;
    }

    return result;
  }

  return bitwiseOrPositive(other);
}

BigInt BigInt::bitwiseOrPositive(const BigInt& other) const {
  BigInt result;
  const size_t maxSize = std::max(m_digits.size(), other.m_digits.size());
  result.m_digits.resize(maxSize, 0);

  for (size_t i = 0; i < maxSize; ++i) {
    uint32_t a = (i < m_digits.size()) ? m_digits[i] : 0;
    uint32_t b = (i < other.m_digits.size()) ? other.m_digits[i] : 0;
    result.m_digits[i] = a | b;
  }

  result.normalize();
  return result;
}

BigInt BigInt::bitwiseXor(const BigInt& other) const {
  if (isZero()) {
    return other;  // 0 XOR x = x
  }
  if (other.isZero()) {
    return *this;  // x XOR 0 = x
  }

  // 2の補数表現での処理
  if (m_sign < 0 || other.m_sign < 0) {
    // 負の数の2の補数表現を処理
    BigInt a = m_sign < 0 ? bitwiseNot().add(BigInt(1)) : *this;
    BigInt b = other.m_sign < 0 ? other.bitwiseNot().add(BigInt(1)) : other;

    BigInt result = a.bitwiseXorPositive(b);

    // 結果の符号を決定
    if ((m_sign < 0) != (other.m_sign < 0)) {
      // 片方だけが負の場合、結果も負
      result = result.bitwiseNot().add(BigInt(1));
      result.m_sign = -1;
    }

    return result;
  }

  return bitwiseXorPositive(other);
}

BigInt BigInt::bitwiseXorPositive(const BigInt& other) const {
  BigInt result;
  const size_t maxSize = std::max(m_digits.size(), other.m_digits.size());
  result.m_digits.resize(maxSize, 0);

  for (size_t i = 0; i < maxSize; ++i) {
    uint32_t a = (i < m_digits.size()) ? m_digits[i] : 0;
    uint32_t b = (i < other.m_digits.size()) ? other.m_digits[i] : 0;
    result.m_digits[i] = a ^ b;
  }

  result.normalize();
  return result;
}

BigInt BigInt::bitwiseNot() const {
  // 2の補数表現での~x = -x - 1
  BigInt result(*this);
  result.m_sign = -result.m_sign;

  if (result.m_sign != 0) {
    // 絶対値を1減らす
    for (size_t i = 0; i < result.m_digits.size(); ++i) {
      if (result.m_digits[i] > 0) {
        result.m_digits[i]--;
        break;
      } else {
        result.m_digits[i] = 0xFFFFFFFF;
      }
    }
  } else {
    // 0の場合は-1を返す
    result.m_digits = {1};
    result.m_sign = -1;
  }

  result.normalize();
  return result;
}

bool BigInt::isZero() const {
  return m_digits.empty() || (m_digits.size() == 1 && m_digits[0] == 0);
}

// BigIntObjectクラスの実装

BigIntObject::BigIntObject(BigInt value)
    : Object(), m_value(std::move(value)) {
}

BigIntObject::~BigIntObject() {
}

const BigInt& BigIntObject::value() const {
  return m_value;
}

Value BigIntObject::initializePrototype(Context* context) {
  // BigInt.prototypeオブジェクトの作成
  Object* prototype = Object::create(context->objectPrototype());

  // プロトタイプメソッドの設定
  prototype->defineNativeFunction(context, "toString", bigIntToString, 0, PropertyAttributes::DontEnum);
  prototype->defineNativeFunction(context, "valueOf", bigIntValueOf, 0, PropertyAttributes::DontEnum);

  // @@toStringTagの設定
  Value toStringTag = Value(context->symbolRegistry().toStringTag());
  prototype->defineProperty(context, toStringTag, Value("BigInt"),
                            PropertyAttributes::ReadOnly | PropertyAttributes::DontEnum | PropertyAttributes::Configurable);

  return Value(prototype);
}

Value bigIntConstructor(Context* context, Value thisValue, const std::vector<Value>& args) {
  // 'new BigInt()' は許可されていない
  if (thisValue.isBigIntObject()) {
    context->throwTypeError("BigInt is not a constructor");
    return Value::undefined();
  }

  // 引数がない場合は0nを返す
  if (args.empty()) {
    return Value(BigInt(0));
  }

  Value arg = args[0];

  // 引数の型に基づいて変換
  if (arg.isNumber()) {
    // 数値からBigIntへの変換
    double number = arg.asNumber();

    // 非整数値はエラー
    if (!std::isfinite(number) || std::floor(number) != number) {
      context->throwRangeError("Cannot convert non-integer to BigInt");
      return Value::undefined();
    }

    return Value(BigInt(static_cast<int64_t>(number)));
  } else if (arg.isString()) {
    // 文字列からBigIntへの変換
    String* str = arg.asString();
    try {
      // 基数の取得（デフォルトは10）
      int radix = 10;
      if (args.size() > 1 && args[1].isNumber()) {
        radix = static_cast<int>(args[1].asNumber());
        if (radix < 2 || radix > 36) {
          context->throwRangeError("Invalid radix value");
          return Value::undefined();
        }
      }

      // 文字列をトリミングして解析
      std::string_view strView = str->view();
      // 先頭と末尾の空白を削除
      size_t start = strView.find_first_not_of(" \t\n\r\f\v");
      if (start == std::string_view::npos) {
        context->throwSyntaxError("Cannot convert empty string to BigInt");
        return Value::undefined();
      }
      size_t end = strView.find_last_not_of(" \t\n\r\f\v");
      strView = strView.substr(start, end - start + 1);

      // 接頭辞の処理
      bool isNegative = false;
      if (!strView.empty() && (strView[0] == '+' || strView[0] == '-')) {
        isNegative = strView[0] == '-';
        strView.remove_prefix(1);
      }

      // 0x, 0b, 0o接頭辞の処理
      if (strView.size() >= 2 && strView[0] == '0') {
        char prefix = strView[1];
        if ((prefix == 'x' || prefix == 'X') && radix == 10) {
          radix = 16;
          strView.remove_prefix(2);
        } else if ((prefix == 'b' || prefix == 'B') && radix == 10) {
          radix = 2;
          strView.remove_prefix(2);
        } else if ((prefix == 'o' || prefix == 'O') && radix == 10) {
          radix = 8;
          strView.remove_prefix(2);
        }
      }

      // 文字列を解析してBigIntを作成
      BigInt result = BigInt::fromString(std::string(strView), radix);
      if (isNegative) {
        result = result.negate();
      }

      return Value(std::move(result));
    } catch (const std::exception& e) {
      context->throwSyntaxError("Cannot convert string to BigInt");
      return Value::undefined();
    }
  } else if (arg.isBigInt()) {
    // BigIntはそのまま返す
    return arg;
  } else if (arg.isBoolean()) {
    // booleanからBigIntへの変換
    return Value(BigInt(arg.asBoolean() ? 1 : 0));
  } else if (arg.isObject()) {
    // オブジェクトの場合はtoPrimitiveを呼び出す
    Value primitive = arg.toPrimitive(context, Value::PreferredType::Number);
    if (primitive.isError()) {
      return primitive;
    }

    // 再帰的に処理
    std::vector<Value> newArgs = {primitive};
    return bigIntConstructor(context, thisValue, newArgs);
  }

  context->throwTypeError("Cannot convert to BigInt");
  return Value::undefined();
}

Value bigIntAsIntN(Context* context, Value thisValue, const std::vector<Value>& args) {
  if (args.size() < 2) {
    context->throwTypeError("BigInt.asIntN requires at least 2 arguments");
    return Value::undefined();
  }

  // ビット数の取得
  Value bitsValue = args[0];
  if (!bitsValue.isNumber()) {
    context->throwTypeError("First argument must be a number");
    return Value::undefined();
  }

  int64_t bits = static_cast<int64_t>(bitsValue.asNumber());
  if (bits < 0 || bits > 0x1FFFFFFFFFFFFF) {
    context->throwRangeError("Number of bits is out of range");
    return Value::undefined();
  }

  // BigIntの取得
  Value bigintValue = args[1];
  if (!bigintValue.isBigInt()) {
    try {
      // BigIntに変換を試みる
      std::vector<Value> conversionArgs = {bigintValue};
      bigintValue = bigIntConstructor(context, Value::undefined(), conversionArgs);
      if (bigintValue.isError() || !bigintValue.isBigInt()) {
        context->throwTypeError("Second argument cannot be converted to BigInt");
        return Value::undefined();
      }
    } catch (...) {
      context->throwTypeError("Second argument cannot be converted to BigInt");
      return Value::undefined();
    }
  }

  BigInt bigint = bigintValue.asBigInt();

  // ビット数が0の場合は常に0
  if (bits == 0) {
    return Value(BigInt(0));
  }

  // 符号付き整数としてビット数に収まるように切り詰める
  BigInt result = bigint.bitwiseAnd(BigInt::shiftLeft(BigInt(1), bits).subtract(BigInt(1)));

  // 最上位ビットが1の場合は負の値として扱う
  if (bits > 0 && result.testBit(bits - 1)) {
    // 2の補数表現で負の値に変換
    BigInt signBit = BigInt::shiftLeft(BigInt(1), bits - 1);
    result = result.subtract(BigInt::shiftLeft(BigInt(1), bits));
  }

  return Value(std::move(result));
}

Value bigIntAsUintN(Context* context, Value thisValue, const std::vector<Value>& args) {
  if (args.size() < 2) {
    context->throwTypeError("BigInt.asUintN requires at least 2 arguments");
    return Value::undefined();
  }

  // ビット数の取得
  Value bitsValue = args[0];
  if (!bitsValue.isNumber()) {
    context->throwTypeError("First argument must be a number");
    return Value::undefined();
  }

  int64_t bits = static_cast<int64_t>(bitsValue.asNumber());
  if (bits < 0 || bits > 0x1FFFFFFFFFFFFF) {
    context->throwRangeError("Number of bits is out of range");
    return Value::undefined();
  }

  // BigIntの取得
  Value bigintValue = args[1];
  if (!bigintValue.isBigInt()) {
    try {
      // BigIntに変換を試みる
      std::vector<Value> conversionArgs = {bigintValue};
      bigintValue = bigIntConstructor(context, Value::undefined(), conversionArgs);
      if (bigintValue.isError() || !bigintValue.isBigInt()) {
        context->throwTypeError("Second argument cannot be converted to BigInt");
        return Value::undefined();
      }
    } catch (...) {
      context->throwTypeError("Second argument cannot be converted to BigInt");
      return Value::undefined();
    }
  }

  BigInt bigint = bigintValue.asBigInt();

  // ビット数に収まるように切り詰める（符号なし）
  if (bits == 0) {
    return Value(BigInt(0));
  }

  BigInt mask = BigInt::shiftLeft(BigInt(1), bits).subtract(BigInt(1));
  BigInt result = bigint.bitwiseAnd(mask);

  return Value(std::move(result));
}

Value bigIntToString(Context* context, Value thisValue, const std::vector<Value>& args) {
  // thisValueがBigIntまたはBigIntObjectであることを確認
  BigInt bigint;

  if (thisValue.isBigInt()) {
    bigint = thisValue.asBigInt();
  } else if (thisValue.isBigIntObject()) {
    bigint = thisValue.asBigIntObject()->value();
  } else {
    context->throwTypeError("BigInt.prototype.toString requires a BigInt receiver");
    return Value::undefined();
  }

  // 基数の取得（デフォルトは10）
  int radix = 10;
  if (!args.empty() && !args[0].isUndefined()) {
    if (!args[0].isNumber()) {
      context->throwTypeError("Radix argument must be a number");
      return Value::undefined();
    }

    radix = static_cast<int>(args[0].asNumber());
    if (radix < 2 || radix > 36) {
      context->throwRangeError("Radix must be between 2 and 36");
      return Value::undefined();
    }
  }

  // BigIntを文字列に変換
  std::string result = bigint.toString(radix);
  return Value(context->createString(result));
}

Value bigIntValueOf(Context* context, Value thisValue, const std::vector<Value>& args) {
  // thisValueがBigIntObjectの場合はプリミティブ値を返す
  if (thisValue.isBigIntObject()) {
    return Value(thisValue.asBigIntObject()->value());
  }

  // thisValueがBigIntの場合はそのまま返す
  if (thisValue.isBigInt()) {
    return thisValue;
  }

  context->throwTypeError("BigInt.prototype.valueOf requires a BigInt receiver");
  return Value::undefined();
}

Value initializeBigInt(Context* context) {
  // BigInt関数オブジェクトの作成
  FunctionObject* bigIntConstructorObj = FunctionObject::create(context, "BigInt", bigIntConstructor, 1);

  // BigIntプロトタイプの初期化
  Value prototype = BigIntObject::initializePrototype(context);
  bigIntConstructorObj->defineProperty(context, "prototype", prototype,
                                       PropertyAttributes::ReadOnly | PropertyAttributes::DontEnum | PropertyAttributes::DontDelete);

  // BigIntオブジェクトのプロトタイプを設定
  Object* prototypeObj = prototype.asObject();
  prototypeObj->defineProperty(context, "constructor", Value(bigIntConstructorObj),
                               PropertyAttributes::Writable | PropertyAttributes::Configurable);

  // 静的メソッドの設定
  bigIntConstructorObj->defineNativeFunction(context, "asIntN", bigIntAsIntN, 2, PropertyAttributes::Writable | PropertyAttributes::Configurable);
  bigIntConstructorObj->defineNativeFunction(context, "asUintN", bigIntAsUintN, 2, PropertyAttributes::Writable | PropertyAttributes::Configurable);

  // グローバルオブジェクトにBigIntを設定
  context->globalObject()->defineProperty(context, "BigInt", Value(bigIntConstructorObj),
                                          PropertyAttributes::Writable | PropertyAttributes::Configurable);

  return Value(bigIntConstructorObj);
}

}  // namespace aero