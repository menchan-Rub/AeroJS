/**
 * @file function.h
 * @brief JavaScript 関数型の定義
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_FUNCTION_H
#define AEROJS_FUNCTION_H

#include <functional>
#include <string>
#include <vector>

#include "src/core/runtime/values/object.h"

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class Value;
class String;
class Symbol;

/**
 * @brief ネイティブ関数のシグネチャ
 *
 * thisValue: 関数内でのthis値
 * args: 関数に渡された引数
 * context: 実行コンテキスト
 * 戻り値: 関数の戻り値
 */
using NativeFunction = std::function<Value(const Value& thisValue,
                                           const std::vector<Value>& args,
                                           Context* context)>;

/**
 * @brief JavaScript関数のコンストラクタのシグネチャ
 *
 * args: コンストラクタに渡された引数
 * context: 実行コンテキスト
 * 戻り値: 作成されたオブジェクト
 */
using NativeConstructor = std::function<Object*(const std::vector<Value>& args,
                                                Context* context)>;

/**
 * @brief JavaScript の関数型を表現するクラス
 *
 * このクラスはJavaScriptの関数とメソッドを表現します。
 */
class Function : public Object {
 public:
  /**
   * @brief ネイティブ関数のコンストラクタ
   * @param context 実行コンテキスト
   * @param name 関数名
   * @param func ネイティブ関数の実装
   * @param argCount 期待される引数の数（-1は可変長）
   * @param prototype 関数のプロトタイプ（nullptrの場合はデフォルト）
   */
  Function(Context* context, const String* name, NativeFunction func,
           int argCount = -1, Object* prototype = nullptr);

  /**
   * @brief ネイティブコンストラクタのコンストラクタ
   * @param context 実行コンテキスト
   * @param name 関数名
   * @param constructor ネイティブコンストラクタの実装
   * @param argCount 期待される引数の数（-1は可変長）
   * @param instancePrototype インスタンスのプロトタイプ
   * @param prototype 関数のプロトタイプ（nullptrの場合はデフォルト）
   */
  Function(Context* context, const String* name, NativeConstructor constructor,
           int argCount = -1, Object* instancePrototype = nullptr, Object* prototype = nullptr);

  /**
   * @brief デストラクタ
   */
  ~Function() override;

  /**
   * @brief 関数名を取得
   * @return 関数名
   */
  String* getName() const;

  /**
   * @brief 関数の期待される引数の数を取得
   * @return 引数の数（-1は可変長）
   */
  int getArgCount() const;

  /**
   * @brief 関数がコンストラクタかどうかを確認
   * @return コンストラクタの場合はtrue
   */
  bool isConstructor() const;

  /**
   * @brief 関数をネイティブ関数として実行
   * @param thisValue 関数内でのthis値
   * @param args 関数に渡された引数
   * @param context 実行コンテキスト
   * @return 関数の戻り値
   */
  Value call(const Value& thisValue, const std::vector<Value>& args, Context* context);

  /**
   * @brief 関数をコンストラクタとして実行
   * @param args コンストラクタに渡された引数
   * @param context 実行コンテキスト
   * @return 作成されたオブジェクト
   */
  Object* construct(const std::vector<Value>& args, Context* context);

  /**
   * @brief 関数のスコープを設定
   * @param scope スコープオブジェクト
   */
  void setScope(Object* scope);

  /**
   * @brief 関数のスコープを取得
   * @return スコープオブジェクト
   */
  Object* getScope() const;

  /**
   * @brief 関数のプロトタイププロパティを取得
   * @return プロトタイププロパティ（コンストラクタの場合）
   */
  Object* getPrototypeProperty() const;

  /**
   * @brief 関数のプロトタイププロパティを設定
   * @param prototype 新しいプロトタイプ
   */
  void setPrototypeProperty(Object* prototype);

  /**
   * @brief 関数をバインド
   * @param thisArg バインドするthis値
   * @param boundArgs 束縛される引数
   * @return 新しいバインドされた関数
   */
  Function* bind(const Value& thisArg, const std::vector<Value>& boundArgs);

  /**
   * @brief 関数の文字列表現を取得
   * @return 関数の文字列表現
   */
  std::string toString() const override;

  /**
   * @brief ネイティブ関数を作成
   * @param context 実行コンテキスト
   * @param name 関数名
   * @param func ネイティブ関数の実装
   * @param argCount 期待される引数の数（-1は可変長）
   * @return 新しい関数オブジェクト
   */
  static Function* createNativeFunction(Context* context, const String* name,
                                        NativeFunction func, int argCount = -1);

  /**
   * @brief ネイティブコンストラクタを作成
   * @param context 実行コンテキスト
   * @param name 関数名
   * @param constructor ネイティブコンストラクタの実装
   * @param argCount 期待される引数の数（-1は可変長）
   * @param instancePrototype インスタンスのプロトタイプ
   * @return 新しい関数オブジェクト
   */
  static Function* createNativeConstructor(Context* context, const String* name,
                                           NativeConstructor constructor, int argCount = -1,
                                           Object* instancePrototype = nullptr);

 private:
  // 関数名
  String* name_;

  // 期待される引数の数
  int argCount_;

  // ネイティブ関数の実装
  NativeFunction nativeFunction_;

  // ネイティブコンストラクタの実装
  NativeConstructor nativeConstructor_;

  // スコープオブジェクト
  Object* scope_;

  // コンストラクタのインスタンスプロトタイプ
  Object* instancePrototype_;

  // バインドされたthis値
  Value boundThis_;

  // バインドされた引数
  std::vector<Value> boundArgs_;

  // 関数がバインド関数かどうか
  bool isBound_;

  // 関数がコンストラクタかどうか
  bool isConstructor_;

  // バインド関数用のプライベートコンストラクタ
  Function(Context* context, Function* targetFunction, const Value& thisArg,
           const std::vector<Value>& boundArgs);

  // バインド関数のラッパー
  static Value boundFunctionWrapper(const Value& thisValue,
                                    const std::vector<Value>& args,
                                    Context* context);

  // コピーおよびムーブの無効化
  Function(const Function&) = delete;
  Function& operator=(const Function&) = delete;
  Function(Function&&) = delete;
  Function& operator=(Function&&) = delete;
};

/**
 * @brief アロー関数を表現するクラス
 *
 * アロー関数は通常の関数と異なり、独自のthisバインディングを持たない
 */
class ArrowFunction : public Function {
 public:
  /**
   * @brief アロー関数のコンストラクタ
   * @param context 実行コンテキスト
   * @param name 関数名
   * @param func ネイティブ関数の実装
   * @param argCount 期待される引数の数（-1は可変長）
   * @param lexicalThis レキシカルなthis値
   * @param prototype 関数のプロトタイプ（nullptrの場合はデフォルト）
   */
  ArrowFunction(Context* context, const String* name, NativeFunction func,
                int argCount = -1, const Value& lexicalThis = Value(),
                Object* prototype = nullptr);

  /**
   * @brief デストラクタ
   */
  ~ArrowFunction() override;

  /**
   * @brief アロー関数をネイティブ関数として実行
   * @param thisValue 無視される（アロー関数はレキシカルthisを使用）
   * @param args 関数に渡された引数
   * @param context 実行コンテキスト
   * @return 関数の戻り値
   */
  Value call(const Value& thisValue, const std::vector<Value>& args, Context* context);

  /**
   * @brief アロー関数をコンストラクタとして実行（エラー）
   * @param args コンストラクタに渡された引数
   * @param context 実行コンテキスト
   * @return 常にnullptr（アロー関数はコンストラクタにできない）
   */
  Object* construct(const std::vector<Value>& args, Context* context);

  /**
   * @brief アロー関数をバインド（必要なし、そのまま返す）
   * @param thisArg 無視される
   * @param boundArgs 無視される
   * @return this（アロー関数は既にバインドされている）
   */
  Function* bind(const Value& thisArg, const std::vector<Value>& boundArgs);

  /**
   * @brief アロー関数の文字列表現を取得
   * @return アロー関数の文字列表現
   */
  std::string toString() const override;

  /**
   * @brief アロー関数を作成
   * @param context 実行コンテキスト
   * @param name 関数名
   * @param func ネイティブ関数の実装
   * @param argCount 期待される引数の数（-1は可変長）
   * @param lexicalThis レキシカルなthis値
   * @return 新しいアロー関数オブジェクト
   */
  static ArrowFunction* createArrowFunction(Context* context, const String* name,
                                            NativeFunction func, int argCount = -1,
                                            const Value& lexicalThis = Value());

 private:
  // レキシカルなthis値
  Value lexicalThis_;

  // コピーおよびムーブの無効化
  ArrowFunction(const ArrowFunction&) = delete;
  ArrowFunction& operator=(const ArrowFunction&) = delete;
  ArrowFunction(ArrowFunction&&) = delete;
  ArrowFunction& operator=(ArrowFunction&&) = delete;
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_FUNCTION_H