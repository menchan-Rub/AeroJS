/**
 * @file bigint.h
 * @brief JavaScriptのBigInt組み込みオブジェクトの定義
 * @copyright Copyright (c) 2023 AeroJS project authors
 */

#pragma once

#include "core/runtime/object.h"
#include "core/runtime/value.h"
#include <string>
#include <memory>
#include <vector>

namespace aero {

/**
 * @class BigInt
 * @brief JavaScriptのBigInt組み込みオブジェクト
 *
 * 任意精度の整数を表現するためのクラスです。
 * ECMAScript仕様に準拠しています。
 */
class BigInt final {
public:
    /**
     * @brief BigIntのストレージ型
     * 数値は複数の32ビット整数の配列として内部的に格納されます。
     */
    using Digits = std::vector<uint32_t>;

    /**
     * @brief 0を表すBigIntを作成します
     */
    BigInt();

    /**
     * @brief 64ビット整数からBigIntを作成します
     * @param value 元となる整数値
     */
    explicit BigInt(int64_t value);

    /**
     * @brief 文字列からBigIntを作成します
     * @param str 数値を表す文字列
     * @param radix 基数（2〜36）
     * @throws std::invalid_argument 無効な文字列または基数の場合
     */
    BigInt(const std::string& str, int radix = 10);

    /**
     * @brief コピーコンストラクタ
     */
    BigInt(const BigInt& other);

    /**
     * @brief ムーブコンストラクタ
     */
    BigInt(BigInt&& other) noexcept;

    /**
     * @brief デストラクタ
     */
    ~BigInt();

    /**
     * @brief コピー代入演算子
     */
    BigInt& operator=(const BigInt& other);

    /**
     * @brief ムーブ代入演算子
     */
    BigInt& operator=(BigInt&& other) noexcept;

    /**
     * @brief 符号を返します
     * @return 負の場合は-1、正の場合は1、ゼロの場合は0
     */
    int sign() const;

    /**
     * @brief 内部ディジット配列への参照を返します
     * @return 内部ディジット配列
     */
    const Digits& digits() const;

    /**
     * @brief 10進数の文字列表現を返します
     * @return 10進数の文字列
     */
    std::string toString() const;

    /**
     * @brief 指定した基数での文字列表現を返します
     * @param radix 基数（2〜36）
     * @return 指定した基数での文字列
     * @throws std::invalid_argument 無効な基数の場合
     */
    std::string toString(int radix) const;

    /**
     * @brief 2つのBigIntが等しいかを比較します
     */
    bool equals(const BigInt& other) const;

    /**
     * @brief 2つのBigIntを比較します
     * @return このBigIntが他方より小さい場合は負の値、
     *         等しい場合は0、大きい場合は正の値
     */
    int compareTo(const BigInt& other) const;

    /**
     * @brief BigIntの値を別のBigIntに加えます
     * @param other 加える値
     * @return 結果を格納した新しいBigInt
     */
    BigInt add(const BigInt& other) const;

    /**
     * @brief BigIntの値から別のBigIntを引きます
     * @param other 引く値
     * @return 結果を格納した新しいBigInt
     */
    BigInt subtract(const BigInt& other) const;

    /**
     * @brief BigIntの値と別のBigIntを掛けます
     * @param other 掛ける値
     * @return 結果を格納した新しいBigInt
     */
    BigInt multiply(const BigInt& other) const;

    /**
     * @brief BigIntの値を別のBigIntで割ります
     * @param other 割る値
     * @return 結果を格納した新しいBigInt
     * @throws std::invalid_argument ゼロで割る場合
     */
    BigInt divide(const BigInt& other) const;

    /**
     * @brief BigIntの値を別のBigIntで割った余りを求めます
     * @param other 割る値
     * @return 余りを格納した新しいBigInt
     * @throws std::invalid_argument ゼロで割る場合
     */
    BigInt remainder(const BigInt& other) const;

    /**
     * @brief BigIntの値を指定したビット数だけ左にシフトします
     * @param bits シフトするビット数
     * @return 結果を格納した新しいBigInt
     */
    BigInt leftShift(int64_t bits) const;

    /**
     * @brief BigIntの値を指定したビット数だけ右にシフトします
     * @param bits シフトするビット数
     * @return 結果を格納した新しいBigInt
     */
    BigInt rightShift(int64_t bits) const;

    /**
     * @brief BigIntのビット単位の論理積を計算します
     * @param other 演算対象
     * @return 結果を格納した新しいBigInt
     */
    BigInt bitwiseAnd(const BigInt& other) const;

    /**
     * @brief BigIntのビット単位の論理和を計算します
     * @param other 演算対象
     * @return 結果を格納した新しいBigInt
     */
    BigInt bitwiseOr(const BigInt& other) const;

    /**
     * @brief BigIntのビット単位の排他的論理和を計算します
     * @param other 演算対象
     * @return 結果を格納した新しいBigInt
     */
    BigInt bitwiseXor(const BigInt& other) const;

    /**
     * @brief BigIntのビット単位の否定を計算します
     * @return 結果を格納した新しいBigInt
     */
    BigInt bitwiseNot() const;

    /**
     * @brief ゼロかどうかを判定します
     * @return ゼロの場合はtrue
     */
    bool isZero() const;

private:
    /**
     * @brief 符号（-1, 0, 1）
     */
    int m_sign;

    /**
     * @brief 絶対値の内部表現（32ビット単位）
     */
    Digits m_digits;

    /**
     * @brief 不要な先頭の0を削除
     */
    void normalize();

    /**
     * @brief 指定した桁数だけゼロ埋めします
     * @param numDigits 追加する桁数
     */
    void padZero(size_t numDigits);

    // フレンド関数
    friend class BigIntObject;
};

/**
 * @class BigIntObject
 * @brief BigIntをラップするJavaScriptオブジェクト
 *
 * BigIntプリミティブ値をラップするオブジェクトを提供します。
 */
class BigIntObject final : public Object {
public:
    /**
     * @brief コンストラクタ
     * @param value BigInt値
     */
    explicit BigIntObject(BigInt value);

    /**
     * @brief デストラクタ
     */
    ~BigIntObject() override;

    /**
     * @brief BigInt値を取得します
     * @return 内部のBigInt値
     */
    const BigInt& value() const;

    /**
     * @brief BigIntプロトタイプオブジェクトを初期化します
     * @param context 実行コンテキスト
     * @return プロトタイプオブジェクト
     */
    static Value initializePrototype(Context* context);

private:
    /**
     * @brief 内部のBigInt値
     */
    BigInt m_value;
};

/**
 * @brief BigIntコンストラクタ関数
 * @param context 実行コンテキスト
 * @param thisValue thisオブジェクト
 * @param args 引数配列
 * @return 新しいBigInt値
 */
Value bigIntConstructor(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief BigInt.asIntN - 指定ビット幅の2の補数整数としてBigIntをクランプします
 * @param context 実行コンテキスト
 * @param thisValue thisオブジェクト
 * @param args 引数配列
 * @return クランプされたBigInt値
 */
Value bigIntAsIntN(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief BigInt.asUintN - 指定ビット幅の符号なし整数としてBigIntをクランプします
 * @param context 実行コンテキスト
 * @param thisValue thisオブジェクト
 * @param args 引数配列
 * @return クランプされたBigInt値
 */
Value bigIntAsUintN(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief BigInt.prototype.toString - BigIntの文字列表現を返します
 * @param context 実行コンテキスト
 * @param thisValue thisオブジェクト
 * @param args 引数配列
 * @return 文字列表現
 */
Value bigIntToString(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief BigInt.prototype.valueOf - BigIntプリミティブ値を返します
 * @param context 実行コンテキスト
 * @param thisValue thisオブジェクト
 * @param args 引数配列
 * @return BigIntプリミティブ値
 */
Value bigIntValueOf(Context* context, Value thisValue, const std::vector<Value>& args);

/**
 * @brief BigIntオブジェクトを初期化します
 * @param context 実行コンテキスト
 * @return BigIntコンストラクタ
 */
Value initializeBigInt(Context* context);

} // namespace aero 