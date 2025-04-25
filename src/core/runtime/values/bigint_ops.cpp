/**
 * @file bigint_ops.cpp
 * @brief BigInt クラスの演算操作の実装
 * @version 0.1.0
 * @license MIT
 */

#include <algorithm>
#include <stdexcept>

#include "src/core/runtime/values/bigint.h"

namespace aerojs {
namespace core {

// 加算
BigInt* BigInt::add(const BigInt& other) const {
  // 同符号の場合: |a| + |b|, 符号は共通
  if (positive_ == other.positive_) {
    std::vector<Digit> result = addAbsolute(digits_, other.digits_);
    return new BigInt(positive_, std::move(result));
  }

  // 異符号の場合: ||a| - |b||, 大きい方の符号
  int cmp = compareAbsolute(*this, other);

  if (cmp == 0) {
    // |a| = |b| -> 結果は0
    return new BigInt();
  } else if (cmp > 0) {
    // |a| > |b| -> 結果の符号はa
    std::vector<Digit> result = subtractAbsolute(digits_, other.digits_);
    return new BigInt(positive_, std::move(result));
  } else {
    // |a| < |b| -> 結果の符号はb
    std::vector<Digit> result = subtractAbsolute(other.digits_, digits_);
    return new BigInt(other.positive_, std::move(result));
  }
}

// 減算
BigInt* BigInt::subtract(const BigInt& other) const {
  // a - b = a + (-b)
  BigInt negated = other;
  negated.positive_ = !negated.positive_;

  return add(negated);
}

// 乗算
BigInt* BigInt::multiply(const BigInt& other) const {
  // ゼロとの乗算
  if (isZero() || other.isZero()) {
    return new BigInt();
  }

  // 絶対値の乗算
  std::vector<Digit> result = multiplyAbsolute(digits_, other.digits_);

  // 結果の符号を決定 (異符号なら負)
  bool resultPositive = (positive_ == other.positive_);

  return new BigInt(resultPositive, std::move(result));
}

// 除算
BigInt* BigInt::divide(const BigInt& other) const {
  // ゼロによる除算はエラー
  if (other.isZero()) {
    throw std::domain_error("Division by zero");
  }

  // ゼロの除算は常にゼロ
  if (isZero()) {
    return new BigInt();
  }

  // 絶対値で除算を行う
  std::vector<Digit> quotient, remainder;
  divRemAbsolute(digits_, other.digits_, quotient, remainder);

  // 結果の符号を決定 (異符号なら負)
  bool resultPositive = (positive_ == other.positive_);

  return new BigInt(resultPositive, std::move(quotient));
}

// 剰余
BigInt* BigInt::remainder(const BigInt& other) const {
  // ゼロによる除算はエラー
  if (other.isZero()) {
    throw std::domain_error("Division by zero");
  }

  // ゼロの剰余は常にゼロ
  if (isZero()) {
    return new BigInt();
  }

  // 絶対値で除算と剰余を行う
  std::vector<Digit> quotient, remainder;
  divRemAbsolute(digits_, other.digits_, quotient, remainder);

  // 結果の符号は被除数の符号
  bool resultPositive = positive_;

  // 剰余が0の場合は常に正
  if (remainder.size() == 1 && remainder[0] == 0) {
    resultPositive = true;
  }

  return new BigInt(resultPositive, std::move(remainder));
}

// 除算と剰余を同時に計算
void BigInt::divideAndRemainder(const BigInt& other, BigInt** quotient, BigInt** remainder) const {
  // ゼロによる除算はエラー
  if (other.isZero()) {
    throw std::domain_error("Division by zero");
  }

  // ゼロの除算は常にゼロ
  if (isZero()) {
    *quotient = new BigInt();
    *remainder = new BigInt();
    return;
  }

  // 絶対値で除算と剰余を行う
  std::vector<Digit> q, r;
  divRemAbsolute(digits_, other.digits_, q, r);

  // 商の符号を決定 (異符号なら負)
  bool quotientPositive = (positive_ == other.positive_);

  // 剰余の符号は被除数の符号
  bool remainderPositive = positive_;

  // 剰余が0の場合は常に正
  if (r.size() == 1 && r[0] == 0) {
    remainderPositive = true;
  }

  *quotient = new BigInt(quotientPositive, std::move(q));
  *remainder = new BigInt(remainderPositive, std::move(r));
}

// 累乗
BigInt* BigInt::pow(uint64_t exponent) const {
  // 0の0乗は言語仕様によって異なる (JavaScriptでは1)
  if (isZero() && exponent == 0) {
    return new BigInt(1);
  }

  // 0の正の累乗は0
  if (isZero()) {
    return new BigInt();
  }

  // 任意の数の0乗は1
  if (exponent == 0) {
    return new BigInt(1);
  }

  // 1または-1の累乗
  if (digits_.size() == 1 && digits_[0] == 1) {
    // -1の奇数乗は-1、偶数乗は1
    if (!positive_) {
      bool resultPositive = (exponent % 2 == 0);
      return new BigInt(resultPositive ? 1 : -1);
    }
    // 1の任意の累乗は1
    return new BigInt(1);
  }

  // 二分累乗法を使用
  BigInt result(1);
  BigInt base(*this);

  while (exponent > 0) {
    if (exponent & 1) {
      // 奇数の場合は結果に現在のベースを乗算
      BigInt* temp = result.multiply(base);
      result = *temp;
      delete temp;
    }

    // ベースを二乗
    BigInt* temp = base.multiply(base);
    base = *temp;
    delete temp;

    // 指数を半分にする
    exponent >>= 1;
  }

  return new BigInt(result);
}

// ビット単位AND演算
BigInt* BigInt::bitwiseAnd(const BigInt& other) const {
  // ゼロとのANDは常にゼロ
  if (isZero() || other.isZero()) {
    return new BigInt();
  }

  // 負の数の場合は2の補数表現で処理
  if (!positive_ || !other.positive_) {
    // 2の補数表現に変換
    BigInt* a_comp = positive_ ? new BigInt(*this) : twosComplement();
    BigInt* b_comp = other.positive_ ? new BigInt(other) : other.twosComplement();
    
    // AND演算を実行
    std::vector<Digit> result;
    size_t maxSize = std::max(a_comp->digits_.size(), b_comp->digits_.size());
    result.resize(maxSize);
    
    for (size_t i = 0; i < maxSize; ++i) {
      Digit a = (i < a_comp->digits_.size()) ? a_comp->digits_[i] : 0;
      Digit b = (i < b_comp->digits_.size()) ? b_comp->digits_[i] : 0;
      result[i] = a & b;
    }
    
    // 結果が負の場合は2の補数に戻す
    BigInt* res = new BigInt(true, std::move(result));
    if ((result.back() & (1ULL << (DIGIT_BITS - 1))) != 0) {
      BigInt* final_res = res->twosComplement();
      final_res->positive_ = false;
      delete res;
      delete a_comp;
      delete b_comp;
      return final_res;
    }
    
    delete a_comp;
    delete b_comp;
    return res;
  }

  // 正の数同士の場合は単純なビット演算
  size_t minSize = std::min(digits_.size(), other.digits_.size());
  std::vector<Digit> result(minSize);

  // 各桁をビット単位ANDで計算
  for (size_t i = 0; i < minSize; ++i) {
    result[i] = digits_[i] & other.digits_[i];
  }

  // 冗長なゼロ桁を削除
  while (result.size() > 1 && result.back() == 0) {
    result.pop_back();
  }

  return new BigInt(true, std::move(result));
}

// ビット単位OR演算
BigInt* BigInt::bitwiseOr(const BigInt& other) const {
  // ゼロとのORは相手の値
  if (isZero()) {
    return new BigInt(other);
  }
  if (other.isZero()) {
    return new BigInt(*this);
  }

  // 負の数の場合は2の補数表現で処理
  if (!positive_ || !other.positive_) {
    // 2の補数表現に変換
    BigInt* a_comp = positive_ ? new BigInt(*this) : twosComplement();
    BigInt* b_comp = other.positive_ ? new BigInt(other) : other.twosComplement();
    
    // OR演算を実行
    std::vector<Digit> result;
    size_t maxSize = std::max(a_comp->digits_.size(), b_comp->digits_.size());
    result.resize(maxSize);
    
    for (size_t i = 0; i < maxSize; ++i) {
      Digit a = (i < a_comp->digits_.size()) ? a_comp->digits_[i] : 0;
      Digit b = (i < b_comp->digits_.size()) ? b_comp->digits_[i] : 0;
      result[i] = a | b;
    }
    
    // 結果が負の場合は2の補数に戻す
    BigInt* res = new BigInt(true, std::move(result));
    if ((result.back() & (1ULL << (DIGIT_BITS - 1))) != 0) {
      BigInt* final_res = res->twosComplement();
      final_res->positive_ = false;
      delete res;
      delete a_comp;
      delete b_comp;
      return final_res;
    }
    
    delete a_comp;
    delete b_comp;
    return res;
  }

  // 正の数同士の場合は単純なビット演算
  size_t maxSize = std::max(digits_.size(), other.digits_.size());
  std::vector<Digit> result(maxSize);

  // 各桁をビット単位ORで計算
  for (size_t i = 0; i < maxSize; ++i) {
    Digit a = (i < digits_.size()) ? digits_[i] : 0;
    Digit b = (i < other.digits_.size()) ? other.digits_[i] : 0;
    result[i] = a | b;
  }

  return new BigInt(true, std::move(result));
}

// ビット単位XOR演算
BigInt* BigInt::bitwiseXor(const BigInt& other) const {
  // ゼロとのXORは相手の値
  if (isZero()) {
    return new BigInt(other);
  }
  if (other.isZero()) {
    return new BigInt(*this);
  }

  // 負の数の場合は2の補数表現で処理
  if (!positive_ || !other.positive_) {
    // 2の補数表現に変換
    BigInt* a_comp = positive_ ? new BigInt(*this) : twosComplement();
    BigInt* b_comp = other.positive_ ? new BigInt(other) : other.twosComplement();
    
    // XOR演算を実行
    std::vector<Digit> result;
    size_t maxSize = std::max(a_comp->digits_.size(), b_comp->digits_.size());
    result.resize(maxSize);
    
    for (size_t i = 0; i < maxSize; ++i) {
      Digit a = (i < a_comp->digits_.size()) ? a_comp->digits_[i] : 0;
      Digit b = (i < b_comp->digits_.size()) ? b_comp->digits_[i] : 0;
      result[i] = a ^ b;
    }
    
    // 結果が負の場合は2の補数に戻す
    BigInt* res = new BigInt(true, std::move(result));
    if ((result.back() & (1ULL << (DIGIT_BITS - 1))) != 0) {
      BigInt* final_res = res->twosComplement();
      final_res->positive_ = false;
      delete res;
      delete a_comp;
      delete b_comp;
      return final_res;
    }
    
    delete a_comp;
    delete b_comp;
    return res;
  }

  // 正の数同士の場合は単純なビット演算
  size_t maxSize = std::max(digits_.size(), other.digits_.size());
  std::vector<Digit> result(maxSize);

  // 各桁をビット単位XORで計算
  for (size_t i = 0; i < maxSize; ++i) {
    Digit a = (i < digits_.size()) ? digits_[i] : 0;
    Digit b = (i < other.digits_.size()) ? other.digits_[i] : 0;
    result[i] = a ^ b;
  }

  // 冗長なゼロ桁を削除
  while (result.size() > 1 && result.back() == 0) {
    result.pop_back();
  }

  return new BigInt(true, std::move(result));
}

// ビット単位NOT演算
BigInt* BigInt::bitwiseNot() const {
  // ビット単位NOTは「-this - 1」に等しい
  BigInt* minusThis = negate();
  BigInt* result = minusThis->subtract(BigInt(1));

  delete minusThis;
  return result;
}

// 左シフト
BigInt* BigInt::leftShift(uint64_t shift) const {
  if (isZero() || shift == 0) {
    return new BigInt(*this);
  }

  // 全桁シフト (DIGIT_BITSずつのシフト)
  size_t digitShift = shift / DIGIT_BITS;
  size_t bitShift = shift % DIGIT_BITS;

  std::vector<Digit> result;

  // 下位桁に0を追加
  result.resize(digitShift, 0);

  // 既存の桁をコピー
  for (auto digit : digits_) {
    result.push_back(digit);
  }

  // ビット単位のシフト処理
  if (bitShift > 0) {
    Digit carry = 0;
    for (size_t i = digitShift; i < result.size(); ++i) {
      Digit newCarry = result[i] >> (DIGIT_BITS - bitShift);
      result[i] = (result[i] << bitShift) | carry;
      carry = newCarry;
    }

    // 最後のキャリーがある場合は新しい桁を追加
    if (carry > 0) {
      result.push_back(carry);
    }
  }

  return new BigInt(positive_, std::move(result));
}

// 右シフト
BigInt* BigInt::rightShift(uint64_t shift) const {
  if (isZero() || shift == 0) {
    return new BigInt(*this);
  }

  // 負の数の場合は特別な処理（算術右シフト）
  if (!positive_) {
    // 2の補数表現に変換
    BigInt* comp = twosComplement();
    
    // 右シフト処理
    size_t digitShift = shift / DIGIT_BITS;
    size_t bitShift = shift % DIGIT_BITS;
    
    // シフト量が値のビット長以上の場合
    if (digitShift >= comp->digits_.size()) {
      delete comp;
      return new BigInt(-1); // 全て1のビットになる
    }
    
    std::vector<Digit> result;
    
    // 上位桁をコピー
    for (size_t i = digitShift; i < comp->digits_.size(); ++i) {
      result.push_back(comp->digits_[i]);
    }
    
    // ビット単位のシフト処理
    if (bitShift > 0) {
      Digit carry = ~0ULL; // 符号拡張のため全ビット1
      for (size_t i = result.size(); i-- > 0;) {
        Digit newCarry = result[i] & ((1ULL << bitShift) - 1);
        result[i] = (result[i] >> bitShift) | (carry << (DIGIT_BITS - bitShift));
        carry = newCarry;
      }
    }
    
    // 結果を2の補数に戻す
    BigInt* shifted = new BigInt(true, std::move(result));
    BigInt* final_result = shifted->twosComplement();
    final_result->positive_ = false;
    
    delete comp;
    delete shifted;
    return final_result;
  }

  // 全桁シフト (DIGIT_BITSずつのシフト)
  size_t digitShift = shift / DIGIT_BITS;
  size_t bitShift = shift % DIGIT_BITS;

  // シフト量が値のビット長以上の場合
  if (digitShift >= digits_.size()) {
    return new BigInt();
  }

  std::vector<Digit> result;

  // 下位桁を削除してから上位桁をコピー
  for (size_t i = digitShift; i < digits_.size(); ++i) {
    result.push_back(digits_[i]);
  }

  // ビット単位のシフト処理
  if (bitShift > 0) {
    Digit carry = 0;
    for (size_t i = result.size(); i-- > 0;) {
      Digit newCarry = result[i] & ((1ULL << bitShift) - 1);
      result[i] = (result[i] >> bitShift) | (carry << (DIGIT_BITS - bitShift));
      carry = newCarry;
    }
  }

  // 冗長なゼロ桁を削除
  while (result.size() > 1 && result.back() == 0) {
    result.pop_back();
  }

  return new BigInt(positive_, std::move(result));
}

// 符号反転
BigInt* BigInt::negate() const {
  if (isZero()) {
    return new BigInt();
  }

  BigInt result(*this);
  result.positive_ = !positive_;

  return new BigInt(result);
}

// 絶対値
BigInt* BigInt::abs() const {
  if (positive_ || isZero()) {
    return new BigInt(*this);
  }

  BigInt result(*this);
  result.positive_ = true;

  return new BigInt(result);
}

// 2の補数表現に変換（負の数のビット演算用）
BigInt* BigInt::twosComplement() const {
  // 全ビット反転
  std::vector<Digit> result(digits_.size());
  for (size_t i = 0; i < digits_.size(); ++i) {
    result[i] = ~digits_[i];
  }
  
  // 1を加算
  Digit carry = 1;
  for (size_t i = 0; i < result.size(); ++i) {
    Digit sum = result[i] + carry;
    carry = (sum < result[i]) ? 1 : 0;
    result[i] = sum;
  }
  
  // キャリーがある場合は新しい桁を追加
  if (carry > 0) {
    result.push_back(carry);
  }
  
  return new BigInt(true, std::move(result));
}

// 桁の絶対値の加算 (符号なし)
std::vector<BigInt::Digit> BigInt::addAbsolute(const std::vector<Digit>& a, const std::vector<Digit>& b) {
  size_t maxSize = std::max(a.size(), b.size());
  std::vector<Digit> result(maxSize + 1, 0);  // 桁上がりの可能性があるため1桁多く確保

  Digit carry = 0;
  for (size_t i = 0; i < maxSize; ++i) {
    Digit digitA = (i < a.size()) ? a[i] : 0;
    Digit digitB = (i < b.size()) ? b[i] : 0;

    Digit sum = digitA + digitB + carry;
    carry = (sum < digitA) || (sum == digitA && digitB > 0);  // 桁上がりの検出

    result[i] = sum;
  }

  // 最終桁の桁上がりがあれば設定
  if (carry > 0) {
    result[maxSize] = carry;
  } else {
    result.pop_back();  // 不要な最上位桁を削除
  }

  return result;
}

// 桁の絶対値の減算 (|a| >= |b| と仮定)
std::vector<BigInt::Digit> BigInt::subtractAbsolute(const std::vector<Digit>& a, const std::vector<Digit>& b) {
  std::vector<Digit> result(a.size());

  Digit borrow = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    Digit digitB = (i < b.size()) ? b[i] : 0;

    // 前の桁からの借り入れを考慮
    Digit diff;
    if (a[i] >= digitB + borrow) {
      diff = a[i] - digitB - borrow;
      borrow = 0;
    } else {
      diff = (a[i] + (DIGIT_BASE)) - digitB - borrow;
      borrow = 1;
    }

    result[i] = diff;
  }

  // 冗長なゼロ桁を削除
  while (result.size() > 1 && result.back() == 0) {
    result.pop_back();
  }

  return result;
}

// 桁の絶対値の乗算
std::vector<BigInt::Digit> BigInt::multiplyAbsolute(const std::vector<Digit>& a, const std::vector<Digit>& b) {
  std::vector<Digit> result(a.size() + b.size(), 0);

  for (size_t i = 0; i < a.size(); ++i) {
    Digit carry = 0;

    for (size_t j = 0; j < b.size(); ++j) {
      uint64_t product = static_cast<uint64_t>(a[i]) * static_cast<uint64_t>(b[j]) + static_cast<uint64_t>(result[i + j]) + static_cast<uint64_t>(carry);

      result[i + j] = static_cast<Digit>(product & (DIGIT_BASE - 1));
      carry = static_cast<Digit>((product >> DIGIT_BITS) & (DIGIT_BASE - 1));
    }

    // 最終桁の桁上がり
    if (carry > 0) {
      result[i + b.size()] = carry;
    }
  }

  // 冗長なゼロ桁を削除
  while (result.size() > 1 && result.back() == 0) {
    result.pop_back();
  }

  return result;
}

// 桁の絶対値の除算と剰余
void BigInt::divRemAbsolute(const std::vector<Digit>& a, const std::vector<Digit>& b,
                            std::vector<Digit>& quotient, std::vector<Digit>& remainder) {
  // 効率的な除算アルゴリズム（ロング除算）
  if (b.size() == 1) {
    // 単一桁の除数の場合は簡易的な処理
    divRemByDigit(a, b[0], quotient, remainder);
    return;
  }

  // 一般的な多桁除算
  size_t n = b.size();
  size_t m = a.size() - n + 1;

  // 被除数と除数の正規化（最上位桁を調整）
  Digit d = DIGIT_BASE / (b.back() + 1);
  std::vector<Digit> normalizedA(a.size() + 1, 0);
  std::vector<Digit> normalizedB(b);

  // 正規化
  if (d > 1) {
    Digit carry = 0;
    for (size_t i = 0; i < a.size(); ++i) {
      uint64_t product = static_cast<uint64_t>(a[i]) * d + carry;
      normalizedA[i] = static_cast<Digit>(product & (DIGIT_BASE - 1));
      carry = static_cast<Digit>(product >> DIGIT_BITS);
    }
    normalizedA[a.size()] = carry;

    carry = 0;
    for (size_t i = 0; i < b.size(); ++i) {
      uint64_t product = static_cast<uint64_t>(b[i]) * d + carry;
      normalizedB[i] = static_cast<Digit>(product & (DIGIT_BASE - 1));
      carry = static_cast<Digit>(product >> DIGIT_BITS);
    }
  } else {
    std::copy(a.begin(), a.end(), normalizedA.begin());
    normalizedA[a.size()] = 0;
  }

  // 商の初期化
  quotient.resize(m, 0);

  // 除算処理
  for (size_t j = m; j-- > 0;) {
    // 商の推定
    uint64_t qhat = (static_cast<uint64_t>(normalizedA[j + n]) * DIGIT_BASE + normalizedA[j + n - 1]) / normalizedB.back();
    uint64_t rhat = (static_cast<uint64_t>(normalizedA[j + n]) * DIGIT_BASE + normalizedA[j + n - 1]) % normalizedB.back();

    // qhatの調整
    while (qhat >= DIGIT_BASE || qhat * normalizedB[n - 2] > DIGIT_BASE * rhat + normalizedA[j + n - 2]) {
      qhat--;
      rhat += normalizedB.back();
      if (rhat >= DIGIT_BASE) break;
    }

    // 商による減算
    int64_t k = 0;
    int64_t t = 0;
    for (size_t i = 0; i < n; ++i) {
      uint64_t p = qhat * normalizedB[i];
      t = normalizedA[j + i] - k - (p & (DIGIT_BASE - 1));
      normalizedA[j + i] = static_cast<Digit>(t & (DIGIT_BASE - 1));
      k = (p >> DIGIT_BITS) - (t >> DIGIT_BITS);
    }
    t = normalizedA[j + n] - k;
    normalizedA[j + n] = static_cast<Digit>(t);

    // 商の設定
    quotient[j] = static_cast<Digit>(qhat);

    // 借り入れが発生した場合は調整
    if (t < 0) {
      quotient[j]--;
      k = 0;
      for (size_t i = 0; i < n; ++i) {
        t = static_cast<int64_t>(normalizedA[j + i]) + static_cast<int64_t>(normalizedB[i]) + k;
        normalizedA[j + i] = static_cast<Digit>(t & (DIGIT_BASE - 1));
        k = t >> DIGIT_BITS;
      }
      normalizedA[j + n] += static_cast<Digit>(k);
    }
  }

  // 剰余の正規化を戻す
  remainder.resize(n);
  if (d > 1) {
    Digit carry = 0;
    for (size_t i = n; i-- > 0;) {
      uint64_t dividend = static_cast<uint64_t>(carry) * DIGIT_BASE + normalizedA[i];
      remainder[i] = static_cast<Digit>(dividend / d);
  // 商を0で初期化
  quotient.clear();
  quotient.push_back(0);

  // 剰余を被除数で初期化
  remainder = a;

  // 除数より大きい間は繰り返し減算
  BigInt divisor(true, b);
  BigInt remainderBigInt(true, remainder);

  while (compareAbsolute(remainderBigInt, divisor) >= 0) {
    // 商を1増やす
    BigInt* incQuotient = BigInt(true, quotient).add(BigInt(1));
    quotient = incQuotient->digits_;
    delete incQuotient;

    // 剰余から除数を引く
    BigInt* subRemainder = remainderBigInt.subtract(divisor);
    remainder = subRemainder->digits_;
    delete subRemainder;

    remainderBigInt = BigInt(true, remainder);
  }
}

}  // namespace core
}  // namespace aerojs