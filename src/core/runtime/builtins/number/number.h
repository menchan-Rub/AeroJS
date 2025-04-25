/**
 * @file number.h
 * @brief JavaScript の Number 組み込みオブジェクトの定義
 *
 * ECMAScript 仕様に準拠したNumber組み込みオブジェクトのヘッダファイルです。
 * プリミティブな数値型と数値オブジェクトの機能を提供します。
 */

#ifndef AEROJS_CORE_RUNTIME_BUILTINS_NUMBER_H
#define AEROJS_CORE_RUNTIME_BUILTINS_NUMBER_H

#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../../error.h"
#include "../../function.h"
#include "../../object.h"
#include "../../property_attribute.h"
#include "../../value.h"

namespace aerojs {
namespace core {

// 前方宣言
class Value;
class Object;
class Function;
class Error;

// スマートポインタ型エイリアス
using ValuePtr = std::shared_ptr<Value>;
using ObjectPtr = std::shared_ptr<Object>;
using FunctionPtr = std::shared_ptr<Function>;
using ErrorPtr = std::shared_ptr<Error>;

/**
 * @class Number
 * @brief JavaScript の Number 組み込みオブジェクトを表すクラス
 *
 * ECMAScript 仕様に準拠した Number オブジェクトの実装。
 * 数値のラッパーオブジェクトとして機能し、静的メソッドと定数を提供します。
 * すべての数値操作はIEEE 754倍精度浮動小数点数として処理されます。
 *
 * スレッド安全性: すべての静的メソッドとイニシャライズメソッドはスレッドセーフです。
 * パフォーマンス特性: プリミティブ数値型に比べてボックス化されたNumberオブジェクトは
 *                   オーバーヘッドが大きいため、パフォーマンスクリティカルな操作では
 *                   プリミティブ型を使用することを推奨します。
 */
class Number : public Object {
 public:
  /**
   * @brief デフォルトコンストラクタ
   *
   * 値が 0.0 の Number オブジェクトを作成します。
   * プロトタイプは Number.prototype に設定されます。
   *
   * @throws std::runtime_error プロトタイプの初期化に失敗した場合
   */
  Number();

  /**
   * @brief 数値を指定して作成するコンストラクタ
   *
   * 指定された値を内部に保持する Number オブジェクトを作成します。
   * プロトタイプは Number.prototype に設定されます。
   *
   * @param value ラップする数値（IEEE 754倍精度浮動小数点数）
   * @throws std::runtime_error プロトタイプの初期化に失敗した場合
   */
  explicit Number(double value);

  /**
   * @brief デストラクタ
   *
   * 特別なリソース解放処理は行いません。
   */
  virtual ~Number() override;

  /**
   * @brief Number コンストラクタ関数
   *
   * new Number() または Number() として呼び出されたときに実行される関数。
   * new 演算子なしで呼び出された場合はプリミティブな数値を返し、
   * new 演算子付きで呼び出された場合は Number オブジェクトを返します。
   *
   * @param arguments 引数リスト
   * @param isConstructCall コンストラクタとして呼び出された場合は true
   * @return 新しく作成された Number オブジェクト、またはプリミティブな数値
   * @throws TypeError 無効な引数が渡された場合
   */
  static ValuePtr construct(const std::vector<ValuePtr>& arguments, bool isConstructCall = false);

  /**
   * @brief 数値を取得
   *
   * 内部に保持している数値を返します。
   *
   * @return 内部に保持している数値（IEEE 754倍精度浮動小数点数）
   */
  double getValue() const;

  /**
   * @brief オブジェクトの種類を判定
   *
   * ValueType::Object を返します。
   *
   * @return ValueType::Object
   */
  ValueType getType() const override {
    return ValueType::Object;
  }

  /**
   * @brief プリミティブ値の種類を判定
   *
   * ValueType::Number を返します。
   *
   * @return ValueType::Number
   */
  ValueType getPrimitiveType() const override {
    return ValueType::Number;
  }

  /**
   * @brief プリミティブな数値に変換
   *
   * 内部に保持している数値を返します。
   *
   * @return 数値（IEEE 754倍精度浮動小数点数）
   */
  double toNumber() const override;

  /**
   * @brief オブジェクトを文字列表現に変換
   *
   * 内部に保持している数値を文字列に変換します。
   * ECMAScript 仕様に従った文字列表現を返します。
   *
   * @return 文字列表現
   */
  std::string toString() const override;

  /**
   * @brief Number のプロトタイプオブジェクトを取得（静的メソッド）
   *
   * Number.prototype オブジェクトを返します。
   * まだ初期化されていない場合は初期化します。
   * このメソッドはスレッドセーフです。
   *
   * @return Number プロトタイプオブジェクト
   */
  static std::shared_ptr<Object> getNumberPrototype();

  /**
   * @brief Number のコンストラクタ関数を取得（静的メソッド）
   *
   * Number コンストラクタ関数を返します。
   * まだ初期化されていない場合は初期化します。
   * このメソッドはスレッドセーフです。
   *
   * @return Number コンストラクタ関数
   */
  static std::shared_ptr<Function> getConstructor();

  /**
   * @brief Number オブジェクトかどうかを判定
   *
   * 指定されたオブジェクトが Number オブジェクトか判定します。
   *
   * @param obj 判定するオブジェクト
   * @return Number オブジェクトの場合は true、そうでない場合は false
   */
  static bool isNumberObject(ObjectPtr obj);

  // Number 静的メソッド

  /**
   * @brief Number.isFinite 静的メソッド
   *
   * 引数が有限数かどうかを判定します。
   * グローバル isFinite() と異なり、型変換を行いません。
   *
   * @param arguments 引数リスト
   * @return 引数が数値型で有限数である場合は true、それ以外は false
   */
  static ValuePtr isFinite(const std::vector<ValuePtr>& arguments);

  /**
   * @brief Number.isInteger 静的メソッド
   *
   * 引数が整数かどうかを判定します。
   *
   * @param arguments 引数リスト
   * @return 引数が数値型で整数である場合は true、それ以外は false
   */
  static ValuePtr isInteger(const std::vector<ValuePtr>& arguments);

  /**
   * @brief Number.isNaN 静的メソッド
   *
   * 引数が NaN かどうかを判定します。
   * グローバル isNaN() と異なり、型変換を行いません。
   *
   * @param arguments 引数リスト
   * @return 引数が数値型で NaN である場合は true、それ以外は false
   */
  static ValuePtr isNaN(const std::vector<ValuePtr>& arguments);

  /**
   * @brief Number.isSafeInteger 静的メソッド
   *
   * 引数が安全な整数かどうかを判定します。
   * 安全な整数とは、IEEE 754倍精度浮動小数点数で正確に表現できる整数で、
   * -(2^53 - 1) から (2^53 - 1) の範囲内の整数です。
   *
   * @param arguments 引数リスト
   * @return 引数が数値型で安全な整数である場合は true、それ以外は false
   */
  static ValuePtr isSafeInteger(const std::vector<ValuePtr>& arguments);

  /**
   * @brief Number.parseFloat 静的メソッド
   *
   * 文字列を解析して浮動小数点数に変換します。
   * グローバル parseFloat() と同じ動作をします。
   *
   * @param arguments 引数リスト
   * @return 解析された浮動小数点数、解析できない場合は NaN
   */
  static ValuePtr parseFloat(const std::vector<ValuePtr>& arguments);

  /**
   * @brief Number.parseInt 静的メソッド
   *
   * 文字列を解析して整数に変換します。
   * グローバル parseInt() と同じ動作をします。
   *
   * @param arguments 引数リスト
   * @param[in] 1: 解析する文字列
   * @param[in] 2: 解析に使用する基数（2～36、デフォルトは10）
   * @return 解析された整数、解析できない場合は NaN
   */
  static ValuePtr parseInt(const std::vector<ValuePtr>& arguments);

  // Number.prototype メソッド

  /**
   * @brief toExponential メソッド
   *
   * 数値を指数表記の文字列に変換します。
   *
   * @param arguments 引数リスト
   * @param[in] 0: this 値（Number オブジェクトまたは数値）
   * @param[in] 1: 小数部の桁数（0～20、省略時は必要な桁数）
   * @return 指数表記の文字列
   * @throws TypeError this が Number でない場合
   * @throws RangeError 桁数が範囲外の場合
   */
  static ValuePtr toExponential(const std::vector<ValuePtr>& arguments);

  /**
   * @brief toFixed メソッド
   *
   * 数値を固定小数点表記の文字列に変換します。
   *
   * @param arguments 引数リスト
   * @param[in] 0: this 値（Number オブジェクトまたは数値）
   * @param[in] 1: 小数部の桁数（0～20、デフォルトは0）
   * @return 固定小数点表記の文字列
   * @throws TypeError this が Number でない場合
   * @throws RangeError 桁数が範囲外の場合
   */
  static ValuePtr toFixed(const std::vector<ValuePtr>& arguments);

  /**
   * @brief toLocaleString メソッド
   *
   * 数値をローカライズされた文字列に変換します。
   * 現在の実装では toString() と同じ動作をします。
   *
   * @param arguments 引数リスト
   * @param[in] 0: this 値（Number オブジェクトまたは数値）
   * @return ローカライズされた文字列表現
   * @throws TypeError this が Number でない場合
   */
  static ValuePtr toLocaleString(const std::vector<ValuePtr>& arguments);

  /**
   * @brief toPrecision メソッド
   *
   * 数値を指定された精度の文字列に変換します。
   *
   * @param arguments 引数リスト
   * @param[in] 0: this 値（Number オブジェクトまたは数値）
   * @param[in] 1: 有効桁数（1～21、省略時は toString() と同じ）
   * @return 指定された精度の文字列
   * @throws TypeError this が Number でない場合
   * @throws RangeError 精度が範囲外の場合
   */
  static ValuePtr toPrecision(const std::vector<ValuePtr>& arguments);

  /**
   * @brief toString メソッド
   *
   * 数値を文字列に変換します。
   *
   * @param arguments 引数リスト
   * @param[in] 0: this 値（Number オブジェクトまたは数値）
   * @param[in] 1: 基数（2～36、デフォルトは10）
   * @return 指定された基数での文字列表現
   * @throws TypeError this が Number でない場合
   * @throws RangeError 基数が範囲外の場合
   */
  static ValuePtr toStringMethod(const std::vector<ValuePtr>& arguments);

  /**
   * @brief valueOf メソッド
   *
   * Number オブジェクトからプリミティブな数値を取得します。
   *
   * @param arguments 引数リスト
   * @param[in] 0: this 値（Number オブジェクトまたは数値）
   * @return プリミティブな数値
   * @throws TypeError this が Number でない場合
   */
  static ValuePtr valueOf(const std::vector<ValuePtr>& arguments);

  // Number 定数

  /**
   * @brief 丸め誤差を表す最小の値
   * 1と1より大きい最小の浮動小数点数の差
   */
  static constexpr double EPSILON = std::numeric_limits<double>::epsilon();

  /**
   * @brief 表現可能な最大の数値
   * 約 1.7976931348623157e+308
   */
  static constexpr double MAX_VALUE = std::numeric_limits<double>::max();

  /**
   * @brief 表現可能な最小の正の数値
   * 約 5e-324
   */
  static constexpr double MIN_VALUE = std::numeric_limits<double>::min();

  /**
   * @brief 安全に表現できる最大の整数
   * 2^53 - 1 = 9007199254740991
   */
  static constexpr double MAX_SAFE_INTEGER = 9007199254740991.0;

  /**
   * @brief 安全に表現できる最小の整数
   * -(2^53 - 1) = -9007199254740991
   */
  static constexpr double MIN_SAFE_INTEGER = -9007199254740991.0;

  /**
   * @brief 正の無限大
   * Infinity
   */
  static constexpr double POSITIVE_INFINITY = std::numeric_limits<double>::infinity();

  /**
   * @brief 負の無限大
   * -Infinity
   */
  static constexpr double NEGATIVE_INFINITY = -std::numeric_limits<double>::infinity();

  /**
   * @brief 非数
   * NaN (Not a Number)
   */
  static constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

 private:
  /**
   * @brief 内部数値
   * IEEE 754倍精度浮動小数点数
   */
  double m_value;

  /**
   * @brief Number オブジェクトの初期化
   *
   * プロトタイプとコンストラクタの設定、定数の登録を行います。
   * このメソッドはスレッドセーフです。
   */
  static void initialize();

  /**
   * @brief this 値から Number オブジェクトまたは数値を取得
   *
   * 引数リストの最初の要素（this値）からNumberオブジェクトまたは数値を取得します。
   *
   * @param arguments 引数リスト（最初の要素が this 値）
   * @return 数値
   * @throws TypeError this が Number でない場合
   */
  static double getNumberFromThis(const std::vector<ValuePtr>& arguments);

  /**
   * @brief Number プロトタイプオブジェクト
   *
   * すべての Number インスタンスの prototype
   */
  static std::shared_ptr<Object> s_prototype;

  /**
   * @brief Number コンストラクタ関数
   *
   * グローバルオブジェクトの Number プロパティ
   */
  static std::shared_ptr<Function> s_constructor;

  /**
   * @brief スレッドセーフのためのミューテックス
   *
   * 静的メンバの初期化と取得を保護するためのミューテックス
   */
  static std::mutex s_mutex;
};

/**
 * @typedef NumberPtr
 * @brief Number クラスのスマートポインタ型
 */
using NumberPtr = std::shared_ptr<Number>;

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_RUNTIME_BUILTINS_NUMBER_H