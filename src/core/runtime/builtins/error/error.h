/**
 * @file error.h
 * @brief JavaScript のエラーオブジェクトの定義
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#ifndef AERO_ERROR_H
#define AERO_ERROR_H

#include "core/runtime/object.h"
#include <string>
#include <memory>
#include <vector>

namespace aero {

// 前方宣言
class ExecutionContext;
class Value;

/**
 * @brief JavaScript のエラー型を表す列挙型
 */
enum class ErrorType {
  Error,          ///< 一般的なエラー
  EvalError,      ///< eval関数に関連するエラー
  RangeError,     ///< 値が所定の範囲内にないエラー
  ReferenceError, ///< 無効な参照に対するエラー
  SyntaxError,    ///< 構文エラー
  TypeError,      ///< 型エラー
  URIError,       ///< URIエンコード/デコード関連のエラー
  AggregateError  ///< 複数のエラーをまとめたエラー（ES2021）
};

/**
 * @brief JavaScript のエラーオブジェクトを表現するクラス
 * 
 * ECMAScript 仕様に基づいたエラーオブジェクトの実装です。
 * メッセージやスタックトレースなどの情報を保持します。
 */
class ErrorObject : public Object {
public:
  /**
   * @brief エラーオブジェクトを構築します
   * 
   * @param type エラーの種類
   * @param message エラーメッセージ（省略可能）
   * @param cause エラーの原因（省略可能）
   */
  ErrorObject(ErrorType type, const std::string& message = "", Object* cause = nullptr);
  
  /**
   * @brief デストラクタ
   */
  ~ErrorObject() override;
  
  /**
   * @brief エラーメッセージを取得します
   * 
   * @return エラーメッセージ
   */
  const std::string& getMessage() const;
  
  /**
   * @brief エラーの種類を取得します
   * 
   * @return エラーの種類
   */
  ErrorType getType() const;
  
  /**
   * @brief エラーの種類名を取得します
   * 
   * @return エラーの種類名
   */
  std::string getTypeName() const;
  
  /**
   * @brief スタックトレースを取得します
   * 
   * @return スタックトレースの文字列
   */
  const std::string& getStack() const;
  
  /**
   * @brief エラーの原因を取得します（ES2022）
   * 
   * @return エラーの原因オブジェクト
   */
  Object* getCause() const;
  
  /**
   * @brief エラーオブジェクトを文字列に変換します
   * 
   * @return エラーの文字列表現
   */
  std::string toString() const;
  
  /**
   * @brief エラーが特定の型かどうかをチェックします
   * 
   * @param type チェックするエラータイプ
   * @return 指定された型の場合はtrue
   */
  bool isType(ErrorType type) const;

private:
  ErrorType m_type;      ///< エラーの種類
  std::string m_message; ///< エラーメッセージ
  std::string m_stack;   ///< スタックトレース
  Object* m_cause;       ///< エラーの原因オブジェクト
  
  /**
   * @brief スタックトレースを生成します
   * 
   * 現在の実行コンテキストからスタックトレースを生成し、m_stackに格納します。
   */
  void generateStackTrace();
};

/**
 * @brief エラータイプに基づいて適切なエラーコンストラクタを返します
 * 
 * @param type エラータイプ
 * @return エラーコンストラクタ関数
 */
using ErrorConstructorFunction = Value(*)(ExecutionContext*, Value, const std::vector<Value>&);
ErrorConstructorFunction getErrorConstructor(ErrorType type);

/**
 * @brief Error コンストラクタ関数
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 新しいErrorオブジェクト
 */
Value errorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief EvalError コンストラクタ関数
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 新しいEvalErrorオブジェクト
 */
Value evalErrorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief RangeError コンストラクタ関数
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 新しいRangeErrorオブジェクト
 */
Value rangeErrorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief ReferenceError コンストラクタ関数
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 新しいReferenceErrorオブジェクト
 */
Value referenceErrorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief SyntaxError コンストラクタ関数
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 新しいSyntaxErrorオブジェクト
 */
Value syntaxErrorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief TypeError コンストラクタ関数
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 新しいTypeErrorオブジェクト
 */
Value typeErrorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief URIError コンストラクタ関数
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 新しいURIErrorオブジェクト
 */
Value uriErrorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief AggregateError コンストラクタ関数（ES2021）
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return 新しいAggregateErrorオブジェクト
 */
Value aggregateErrorConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief Error.prototype.toString メソッド
 * 
 * @param ctx 実行コンテキスト
 * @param thisValue thisの値
 * @param args 引数リスト
 * @return エラーの文字列表現
 */
Value errorToString(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

/**
 * @brief エラーオブジェクトのプロトタイプを初期化します
 * 
 * @param ctx 実行コンテキスト
 * @param prototype プロトタイプオブジェクト
 */
void initializeErrorPrototype(ExecutionContext* ctx, Object* prototype);

/**
 * @brief 特定のエラータイプのプロトタイプを初期化します
 * 
 * @param ctx 実行コンテキスト
 * @param type エラータイプ
 * @param prototype 特定のエラータイプのプロトタイプオブジェクト
 * @param errorPrototype 基本Errorプロトタイプオブジェクト
 */
void initializeSpecificErrorPrototype(ExecutionContext* ctx, ErrorType type, Object* prototype, Object* errorPrototype);

/**
 * @brief エラーオブジェクトをグローバルオブジェクトに登録します
 * 
 * @param ctx 実行コンテキスト
 * @param global グローバルオブジェクト
 */
void registerErrorObjects(ExecutionContext* ctx, Object* global);

} // namespace aero

#endif // AERO_ERROR_H 