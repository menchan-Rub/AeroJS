/**
 * @file function.h
 * @brief JavaScript Functionクラスの定義
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_FUNCTION_H
#define AEROJS_FUNCTION_H

#include "object.h"
#include "value.h"

#include <functional>
#include <vector>
#include <string>

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class Array;

/**
 * @brief ネイティブ関数の型定義
 */
using NativeFunction = std::function<Value*(Context*, const std::vector<Value*>&)>;

/**
 * @brief JavaScript Function型を表現するクラス
 *
 * ECMAScript仕様に準拠したFunctionオブジェクトの実装。
 * ネイティブ関数、ユーザー定義関数、アロー関数、コンストラクタをサポート。
 */
class Function : public Object {
 public:
  /**
   * @brief 関数の種類を表す列挙型
   */
  enum class FunctionType {
    Native,      // ネイティブ関数
    UserDefined, // ユーザー定義関数
    Arrow,       // アロー関数
    Bound,       // bind()されたバウンド関数
    Generator,   // ジェネレータ関数
    AsyncFunction, // 非同期関数
    AsyncGenerator // 非同期ジェネレータ関数
  };

  /**
   * @brief ネイティブ関数を作成
   * @param name 関数名
   * @param nativeFunc ネイティブ関数の実装
   * @param length 仮引数の数
   */
  Function(const std::string& name, NativeFunction nativeFunc, uint32_t length = 0);

  /**
   * @brief ユーザー定義関数を作成
   * @param name 関数名
   * @param params 仮引数名のリスト
   * @param body 関数本体のコード
   * @param closure クロージャ環境
   */
  Function(const std::string& name, const std::vector<std::string>& params, 
           const std::string& body, Context* closure = nullptr);

  /**
   * @brief バウンド関数を作成
   * @param targetFunction バインド対象の関数
   * @param thisArg thisとして使用する値
   * @param boundArgs 事前に束縛する引数
   */
  Function(Function* targetFunction, Value* thisArg, const std::vector<Value*>& boundArgs);

  /**
   * @brief デストラクタ
   */
  ~Function() override;

  /**
   * @brief オブジェクトタイプを取得
   * @return Type::Function
   */
  Type getType() const override;

  /**
   * @brief 関数の種類を取得
   * @return 関数の種類
   */
  FunctionType getFunctionType() const {
    return functionType_;
  }

  /**
   * @brief 関数名を取得
   * @return 関数名
   */
  const std::string& getName() const {
    return name_;
  }

  /**
   * @brief 仮引数の数を取得
   * @return 仮引数の数
   */
  uint32_t getLength() const {
    return length_;
  }

  /**
   * @brief 関数を呼び出す
   * @param context 実行コンテキスト
   * @param thisArg this として使用する値
   * @param args 引数リスト
   * @return 戻り値
   */
  Value* call(Context* context, Value* thisArg, const std::vector<Value*>& args);

  /**
   * @brief 関数をコンストラクタとして呼び出す
   * @param context 実行コンテキスト
   * @param args 引数リスト
   * @return 作成されたオブジェクト
   */
  Value* construct(Context* context, const std::vector<Value*>& args);

  /**
   * @brief apply()メソッドの実装
   * @param context 実行コンテキスト
   * @param thisArg thisとして使用する値
   * @param argsArray 引数の配列
   * @return 戻り値
   */
  Value* apply(Context* context, Value* thisArg, Array* argsArray);

  /**
   * @brief bind()メソッドの実装
   * @param thisArg thisとして束縛する値
   * @param args 事前に束縛する引数
   * @return 新しいバウンド関数
   */
  Function* bind(Value* thisArg, const std::vector<Value*>& args);

  /**
   * @brief 関数が呼び出し可能かを確認
   * @return 呼び出し可能な場合はtrue
   */
  bool isCallable() const {
    return true; // 関数は常に呼び出し可能
  }

  /**
   * @brief 関数がコンストラクタとして使用可能かを確認
   * @return コンストラクタとして使用可能な場合はtrue
   */
  bool isConstructor() const;

  /**
   * @brief アロー関数かどうかを確認
   * @return アロー関数の場合はtrue
   */
  bool isArrowFunction() const {
    return functionType_ == FunctionType::Arrow;
  }

  /**
   * @brief ネイティブ関数かどうかを確認
   * @return ネイティブ関数の場合はtrue
   */
  bool isNativeFunction() const {
    return functionType_ == FunctionType::Native;
  }

  /**
   * @brief バウンド関数かどうかを確認
   * @return バウンド関数の場合はtrue
   */
  bool isBoundFunction() const {
    return functionType_ == FunctionType::Bound;
  }

  /**
   * @brief ジェネレータ関数かどうかを確認
   * @return ジェネレータ関数の場合はtrue
   */
  bool isGeneratorFunction() const {
    return functionType_ == FunctionType::Generator;
  }

  /**
   * @brief 非同期関数かどうかを確認
   * @return 非同期関数の場合はtrue
   */
  bool isAsyncFunction() const {
    return functionType_ == FunctionType::AsyncFunction || 
           functionType_ == FunctionType::AsyncGenerator;
  }

  /**
   * @brief 関数のプロトタイプオブジェクトを取得
   * @return プロトタイプオブジェクト
   */
  Object* getPrototypeProperty() const;

  /**
   * @brief 関数のプロトタイプオブジェクトを設定
   * @param prototype 新しいプロトタイプオブジェクト
   */
  void setPrototypeProperty(Object* prototype);

  /**
   * @brief 関数の文字列表現を取得
   * @return "[object Function]"
   */
  std::string toString() const override;

  /**
   * @brief 関数のソースコードを取得
   * @return ソースコード文字列
   */
  std::string getSourceCode() const;

  // 静的ファクトリ関数
  static Function* createNative(const std::string& name, NativeFunction func, uint32_t length = 0);
  static Function* createUserDefined(const std::string& name, const std::vector<std::string>& params, 
                                   const std::string& body, Context* closure = nullptr);
  static Function* createArrow(const std::vector<std::string>& params, const std::string& body, 
                             Context* closure);
  static Function* createGenerator(const std::string& name, const std::vector<std::string>& params, 
                                 const std::string& body, Context* closure = nullptr);
  static Function* createAsync(const std::string& name, const std::vector<std::string>& params, 
                             const std::string& body, Context* closure = nullptr);

 private:
  // 関数の基本プロパティ
  FunctionType functionType_;
  std::string name_;
  uint32_t length_;
  
  // ネイティブ関数用
  NativeFunction nativeFunction_;
  
  // ユーザー定義関数用
  std::vector<std::string> parameters_;
  std::string functionBody_;
  Context* closure_;
  
  // バウンド関数用
  Function* targetFunction_;
  Value* boundThisArg_;
  std::vector<Value*> boundArguments_;
  
  // プロトタイプオブジェクト
  Object* prototypeProperty_;
  
  // 内部ヘルパー関数
  Value* callNativeFunction(Context* context, Value* thisArg, const std::vector<Value*>& args);
  Value* callUserDefinedFunction(Context* context, Value* thisArg, const std::vector<Value*>& args);
  Value* callBoundFunction(Context* context, Value* thisArg, const std::vector<Value*>& args);
  Value* callArrowFunction(Context* context, Value* thisArg, const std::vector<Value*>& args);
  
  Object* constructUserDefinedFunction(Context* context, const std::vector<Value*>& args);
  Object* constructNativeFunction(Context* context, const std::vector<Value*>& args);
  
  void initializePrototypeProperty();
  Context* createExecutionContext(Context* parentContext, Value* thisArg, const std::vector<Value*>& args);
  
  // 引数を仮引数にバインド
  void bindArguments(Context* executionContext, const std::vector<Value*>& args);
  
  // 関数実行時の引数オブジェクトを作成
  Object* createArgumentsObject(const std::vector<Value*>& args);
};

/**
 * @brief JavaScript の Arguments オブジェクト
 */
class ArgumentsObject : public Object {
 public:
  ArgumentsObject(const std::vector<Value*>& args);
  ~ArgumentsObject() override;
  
  Type getType() const override {
    return Type::Object; // Argumentsは特別なオブジェクト
  }
  
  std::string toString() const override {
    return "[object Arguments]";
  }
  
 private:
  std::vector<Value*> arguments_;
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_FUNCTION_H