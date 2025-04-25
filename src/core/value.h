/**
 * @file value.h
 * @brief AeroJS JavaScript エンジンの値クラスの定義
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_CORE_VALUE_H
#define AEROJS_CORE_VALUE_H

#include <cstdint>
#include <string>

#include "../utils/containers/string/string_view.h"
#include "../utils/memory/smart_ptr/ref_counted.h"

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class Object;
class Function;
class Array;
class String;
class Symbol;
class BigInt;
class Error;
class RegExp;
class Date;

/**
 * @brief JavaScriptの値の型を表す列挙型
 */
enum class ValueType : uint8_t {
  Undefined,
  Null,
  Boolean,
  Number,
  String,
  Symbol,
  Object,
  Function,
  Array,
  Date,
  RegExp,
  Error,
  BigInt
};

/**
 * @brief JavaScriptの値を表すクラス
 *
 * このクラスはJavaScriptの値（プリミティブ型とオブジェクト型）を表現します。
 * NaNボクシングを使用して64ビット内にすべての値型を効率的に格納します。
 */
class Value {
 public:
  /**
   * @brief Undefined値を作成
   * @param ctx コンテキスト
   * @return Undefined値
   */
  static Value* createUndefined(Context* ctx);

  /**
   * @brief Null値を作成
   * @param ctx コンテキスト
   * @return Null値
   */
  static Value* createNull(Context* ctx);

  /**
   * @brief Boolean値を作成
   * @param ctx コンテキスト
   * @param value ブール値
   * @return Boolean値
   */
  static Value* createBoolean(Context* ctx, bool value);

  /**
   * @brief Number値を作成
   * @param ctx コンテキスト
   * @param value 数値
   * @return Number値
   */
  static Value* createNumber(Context* ctx, double value);

  /**
   * @brief String値を作成
   * @param ctx コンテキスト
   * @param value 文字列
   * @return String値
   */
  static Value* createString(Context* ctx, const std::string& value);

  /**
   * @brief String値を作成
   * @param ctx コンテキスト
   * @param value 文字列ビュー
   * @return String値
   */
  static Value* createString(Context* ctx, const utils::StringView& value);

  /**
   * @brief String値を作成
   * @param ctx コンテキスト
   * @param value C文字列
   * @return String値
   */
  static Value* createString(Context* ctx, const char* value);

  /**
   * @brief Symbol値を作成
   * @param ctx コンテキスト
   * @param description シンボルの説明
   * @return Symbol値
   */
  static Value* createSymbol(Context* ctx, const std::string& description);

  /**
   * @brief Object値を作成
   * @param ctx コンテキスト
   * @return Object値
   */
  static Value* createObject(Context* ctx);

  /**
   * @brief Array値を作成
   * @param ctx コンテキスト
   * @param length 配列の初期長さ
   * @return Array値
   */
  static Value* createArray(Context* ctx, uint32_t length = 0);

  /**
   * @brief Date値を作成
   * @param ctx コンテキスト
   * @param time ミリ秒単位のUNIX時間
   * @return Date値
   */
  static Value* createDate(Context* ctx, double time);

  /**
   * @brief RegExp値を作成
   * @param ctx コンテキスト
   * @param pattern 正規表現パターン
   * @param flags 正規表現フラグ
   * @return RegExp値
   */
  static Value* createRegExp(Context* ctx, const std::string& pattern, const std::string& flags);

  /**
   * @brief Error値を作成
   * @param ctx コンテキスト
   * @param message エラーメッセージ
   * @param name エラー名（省略可）
   * @return Error値
   */
  static Value* createError(Context* ctx, const std::string& message, const std::string& name = "Error");

  /**
   * @brief BigInt値を作成
   * @param ctx コンテキスト
   * @param value 64ビット整数値
   * @return BigInt値
   */
  static Value* createBigInt(Context* ctx, int64_t value);

  /**
   * @brief BigInt値を作成
   * @param ctx コンテキスト
   * @param str 数値を表す文字列
   * @return BigInt値
   */
  static Value* createBigIntFromString(Context* ctx, const std::string& str);

  /**
   * @brief オブジェクトからValue値を作成
   * @param obj オブジェクト
   * @return Object値
   */
  static Value* fromObject(Object* obj);

  /**
   * @brief 値の型を取得
   * @return 値の型
   */
  ValueType getType() const;

  /**
   * @brief 値がUndefinedかどうかを確認
   * @return Undefinedの場合はtrue
   */
  bool isUndefined() const;

  /**
   * @brief 値がNullかどうかを確認
   * @return Nullの場合はtrue
   */
  bool isNull() const;

  /**
   * @brief 値がBooleanかどうかを確認
   * @return Booleanの場合はtrue
   */
  bool isBoolean() const;

  /**
   * @brief 値がNumberかどうかを確認
   * @return Numberの場合はtrue
   */
  bool isNumber() const;

  /**
   * @brief 値がStringかどうかを確認
   * @return Stringの場合はtrue
   */
  bool isString() const;

  /**
   * @brief 値がSymbolかどうかを確認
   * @return Symbolの場合はtrue
   */
  bool isSymbol() const;

  /**
   * @brief 値がObjectかどうかを確認
   * @return Objectの場合はtrue
   */
  bool isObject() const;

  /**
   * @brief 値がFunctionかどうかを確認
   * @return Functionの場合はtrue
   */
  bool isFunction() const;

  /**
   * @brief 値がArrayかどうかを確認
   * @return Arrayの場合はtrue
   */
  bool isArray() const;

  /**
   * @brief 値がDateかどうかを確認
   * @return Dateの場合はtrue
   */
  bool isDate() const;

  /**
   * @brief 値がRegExpかどうかを確認
   * @return RegExpの場合はtrue
   */
  bool isRegExp() const;

  /**
   * @brief 値がErrorかどうかを確認
   * @return Errorの場合はtrue
   */
  bool isError() const;

  /**
   * @brief 値がBigIntかどうかを確認
   * @return BigIntの場合はtrue
   */
  bool isBigInt() const;

  /**
   * @brief 値がプリミティブ型かどうかを確認
   * @return プリミティブ型の場合はtrue
   */
  bool isPrimitive() const;

  /**
   * @brief 値が真と評価されるかどうかを確認
   * @return 真と評価される場合はtrue
   */
  bool toBoolean() const;

  /**
   * @brief 値を数値に変換
   * @return 数値
   */
  double toNumber() const;

  /**
   * @brief 値を整数に変換
   * @return 整数
   */
  int32_t toInt32() const;

  /**
   * @brief 値を符号なし整数に変換
   * @return 符号なし整数
   */
  uint32_t toUInt32() const;

  /**
   * @brief 値を文字列に変換
   * @return 文字列
   */
  std::string toString() const;

  /**
   * @brief 値をオブジェクトに変換
   * @return オブジェクト、変換できない場合はnullptr
   */
  Object* toObject() const;

  /**
   * @brief 値を関数として取得
   * @return 関数オブジェクト、関数でない場合はnullptr
   */
  Function* asFunction() const;

  /**
   * @brief 値を配列として取得
   * @return 配列オブジェクト、配列でない場合はnullptr
   */
  Array* asArray() const;

  /**
   * @brief 値を文字列オブジェクトとして取得
   * @return 文字列オブジェクト、文字列でない場合はnullptr
   */
  String* asString() const;

  /**
   * @brief 値をシンボルとして取得
   * @return シンボルオブジェクト、シンボルでない場合はnullptr
   */
  Symbol* asSymbol() const;

  /**
   * @brief 値をBigIntとして取得
   * @return BigIntオブジェクト、BigIntでない場合はnullptr
   */
  BigInt* asBigInt() const;

  /**
   * @brief 値を日付として取得
   * @return 日付オブジェクト、日付でない場合はnullptr
   */
  Date* asDate() const;

  /**
   * @brief 値を正規表現として取得
   * @return 正規表現オブジェクト、正規表現でない場合はnullptr
   */
  RegExp* asRegExp() const;

  /**
   * @brief 値をエラーとして取得
   * @return エラーオブジェクト、エラーでない場合はnullptr
   */
  Error* asError() const;

  /**
   * @brief 値を別の値と比較
   * @param other 比較対象の値
   * @return 等しい場合はtrue
   */
  bool equals(const Value* other) const;

  /**
   * @brief 値を別の値と厳密に比較
   * @param other 比較対象の値
   * @return 厳密に等しい場合はtrue
   */
  bool strictEquals(const Value* other) const;

  /**
   * @brief 値をマークして、GCから保護する
   */
  void mark();

  /**
   * @brief 値を保護する（GCから除外）
   */
  void protect();

  /**
   * @brief 保護を解除する（GCの対象に戻す）
   */
  void unprotect();

  /**
   * @brief 現在のコンテキストを取得
   * @return 現在のコンテキスト
   */
  Context* getContext() const;

 private:
  // 内部定数
  static constexpr uint64_t TAG_MASK = 0xFFFF000000000000ULL;
  static constexpr uint64_t TAG_PAYLOAD = 0x0000FFFFFFFFFFFFULL;
  static constexpr uint64_t TAG_NAN = 0x7FF8000000000000ULL;
  static constexpr uint64_t TAG_UNDEFINED = 0xFFFF000000000001ULL;
  static constexpr uint64_t TAG_NULL = 0xFFFF000000000002ULL;
  static constexpr uint64_t TAG_BOOLEAN = 0xFFFF000000000003ULL;
  static constexpr uint64_t TAG_STRING = 0xFFFF000000000004ULL;
  static constexpr uint64_t TAG_SYMBOL = 0xFFFF000000000005ULL;
  static constexpr uint64_t TAG_OBJECT = 0xFFFF000000000006ULL;

  // メンバ変数
  union {
    uint64_t m_bits;
    double m_number;
    void* m_ptr;
  } m_value;

  uint32_t m_flags;     // フラグ
  uint32_t m_refCount;  // 参照カウント
  Context* m_context;   // 所属コンテキスト

  // コンストラクタ
  explicit Value(Context* ctx);

  // 値の設定ヘルパー
  void setTag(uint64_t tag, void* payload = nullptr);

  // 数値の設定
  void setNumber(double num);

  // タグの取得
  uint64_t getTag() const;

  // ペイロードの取得
  void* getPayload() const;

  // フラグ定義
  enum ValueFlags : uint32_t {
    Flag_Marked = 1 << 0,     // GCによってマークされた
    Flag_Protected = 1 << 1,  // GCから保護されている
    Flag_Temporary = 1 << 2   // 一時的な値
  };

  // フラグ操作ヘルパー
  bool hasFlag(ValueFlags flag) const;
  void setFlag(ValueFlags flag);
  void clearFlag(ValueFlags flag);
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_VALUE_H