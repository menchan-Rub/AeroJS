/**
 * @file exception.h
 * @brief 仮想マシンの例外定義
 * 
 * このファイルはJavaScriptエンジンの仮想マシンで使用される例外クラスを定義します。
 * VMExceptionはバイトコード実行中に発生するJavaScript例外を表します。
 */

#ifndef AEROJS_CORE_VM_EXCEPTION_EXCEPTION_H_
#define AEROJS_CORE_VM_EXCEPTION_EXCEPTION_H_

#include <exception>
#include <memory>
#include <string>
#include <vector>
#include "../../runtime/values/value.h"

namespace aerojs {
namespace core {

/**
 * @brief 仮想マシン例外クラス
 * 
 * JavaScriptエンジンの仮想マシン内で発生する例外を表します。
 * エラーオブジェクトを含み、スタックトレース情報も持ちます。
 */
class VMException : public std::exception {
public:
  /**
   * @brief エラーメッセージで例外を構築
   * 
   * @param message エラーメッセージ
   */
  explicit VMException(const std::string& message);

  /**
   * @brief エラーオブジェクトで例外を構築
   * 
   * @param errorObject JavaScript エラーオブジェクト
   */
  explicit VMException(ValuePtr errorObject);

  /**
   * @brief デストラクタ
   */
  ~VMException() noexcept override;

  /**
   * @brief エラーメッセージを取得
   * 
   * @return const char* エラーメッセージ
   */
  const char* what() const noexcept override;

  /**
   * @brief エラーオブジェクトを取得
   * 
   * @return ValuePtr エラーオブジェクト
   */
  ValuePtr getErrorObject() const;

  /**
   * @brief スタックトレース情報を取得
   * 
   * @return const std::vector<std::string>& スタックトレース情報
   */
  const std::vector<std::string>& getStackTrace() const;

  /**
   * @brief スタックトレース情報を設定
   * 
   * @param stackTrace スタックトレース情報
   */
  void setStackTrace(const std::vector<std::string>& stackTrace);

  /**
   * @brief スタックトレースにフレーム情報を追加
   * 
   * @param frameInfo フレーム情報
   */
  void addStackFrame(const std::string& frameInfo);

  /**
   * @brief 例外のフォーマット済み文字列表現を取得
   * 
   * エラーメッセージとスタックトレースを含む文字列表現を返します。
   * 
   * @return std::string 例外の文字列表現
   */
  std::string toString() const;

  /**
   * @brief 例外が特定の型かどうかを確認
   * 
   * @param typeName 確認する例外型の名前（"Error", "TypeError"など）
   * @return bool 例外が指定した型である場合はtrue
   */
  bool isInstanceOf(const std::string& typeName) const;

private:
  /** @brief エラーオブジェクト */
  ValuePtr m_errorObject;
  
  /** @brief エラーメッセージ */
  std::string m_message;
  
  /** @brief スタックトレース情報 */
  std::vector<std::string> m_stackTrace;
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_VM_EXCEPTION_EXCEPTION_H_ 