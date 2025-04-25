/**
 * @file context.h
 * @brief JavaScript実行コンテキストのヘッダファイル
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#ifndef AERO_RUNTIME_CONTEXT_H
#define AERO_RUNTIME_CONTEXT_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace aero {

// 前方宣言
class Object;
class Value;
class String;
class Symbol;
class Function;
class Error;

/**
 * @brief JavaScript実行コンテキストクラス
 *
 * JavaScript実行環境の状態を管理するクラス。
 * スコープチェーン、変数、this値などを含む実行コンテキストを表現します。
 */
class ExecutionContext {
 public:
  /**
   * @brief コンテキストの種類
   */
  enum class Type {
    Global,    ///< グローバルコンテキスト
    Function,  ///< 関数コンテキスト
    Eval,      ///< evalコンテキスト
    Module,    ///< モジュールコンテキスト
    Block      ///< ブロックコンテキスト
  };

  /**
   * @brief デフォルトコンストラクタ
   */
  ExecutionContext();

  /**
   * @brief 型指定コンストラクタ
   *
   * @param type コンテキストの種類
   */
  explicit ExecutionContext(Type type);

  /**
   * @brief デストラクタ
   */
  ~ExecutionContext();

  /**
   * @brief コンテキストの種類を取得
   *
   * @return コンテキストの種類
   */
  Type type() const;

  /**
   * @brief グローバルオブジェクトを取得
   *
   * @return グローバルオブジェクト
   */
  Object* globalObject() const;

  /**
   * @brief グローバルオブジェクトを初期化して取得
   *
   * @return 初期化されたグローバルオブジェクト
   */
  Object* createGlobalObject();

  /**
   * @brief 変数オブジェクトを取得
   *
   * @return 変数オブジェクト
   */
  Object* variableObject() const;

  /**
   * @brief 変数オブジェクトを設定
   *
   * @param obj 変数オブジェクト
   */
  void setVariableObject(Object* obj);

  /**
   * @brief スコープチェーンを取得
   *
   * @return スコープチェーン（オブジェクトの配列）
   */
  const std::vector<Object*>& scopeChain() const;

  /**
   * @brief スコープにオブジェクトを追加
   *
   * @param obj 追加するオブジェクト
   */
  void pushScope(Object* obj);

  /**
   * @brief スコープから最後に追加したオブジェクトを削除
   */
  void popScope();

  /**
   * @brief this値を取得
   *
   * @return this値
   */
  Value thisValue() const;

  /**
   * @brief this値を設定
   *
   * @param value 設定するthis値
   */
  void setThisValue(Value value);

  /**
   * @brief 新しいオブジェクトを作成
   *
   * @return 作成されたオブジェクト
   */
  Object* createObject();

  /**
   * @brief 文字列オブジェクトを作成
   *
   * @param value 文字列値
   * @return 作成された文字列オブジェクト
   */
  String* createString(const std::string& value);

  /**
   * @brief シンボルを作成
   *
   * @param description シンボルの説明
   * @return 作成されたシンボル
   */
  Symbol* createSymbol(const std::string& description);

  /**
   * @brief 関数オブジェクトを作成
   *
   * @param name 関数名
   * @param paramCount パラメータ数
   * @return 作成された関数オブジェクト
   */
  Function* createFunction(const std::string& name, int paramCount);

  /**
   * @brief JavaScript コードを評価
   *
   * @param code 評価するJavaScriptコード
   * @param fileName ファイル名（エラー表示用）
   * @return 評価結果
   */
  Value evaluateScript(const std::string& code, const std::string& fileName = "<eval>");

  /**
   * @brief モジュールを評価
   *
   * @param code モジュールコード
   * @param fileName モジュールファイル名
   * @return 評価結果
   */
  Value evaluateModule(const std::string& code, const std::string& fileName);

  /**
   * @brief モジュールをインポート
   *
   * @param specifier モジュール指定子
   * @return モジュールの名前空間オブジェクト
   */
  Object* importModule(const std::string& specifier);

  /**
   * @brief TypeErrorをスロー
   *
   * @param message エラーメッセージ
   * @return undefined値（通常は例外が発生するため返されない）
   */
  Value throwTypeError(const std::string& message);

  /**
   * @brief エラーをスロー
   *
   * @param message エラーメッセージ
   * @param name エラー名（デフォルトは "Error"）
   * @return undefined値（通常は例外が発生するため返されない）
   */
  Value throwError(const std::string& message, const std::string& name = "Error");

  /**
   * @brief 厳格モードかどうかを取得
   *
   * @return 厳格モードの場合true
   */
  bool isStrictMode() const;

  /**
   * @brief 厳格モードを設定
   *
   * @param strict 厳格モードにする場合true
   */
  void setStrictMode(bool strict);

  /**
   * @brief コンソールAPIが利用可能かどうかを取得
   *
   * @return コンソールAPIが利用可能な場合true
   */
  bool hasConsole() const;

  /**
   * @brief ESモジュールが利用可能かどうかを取得
   *
   * @return ESモジュールが利用可能な場合true
   */
  bool hasModules() const;

  /**
   * @brief 実行中かどうかを取得
   *
   * @return 実行中の場合true
   */
  bool isRunning() const;

  /**
   * @brief SharedArrayBufferが有効かどうかを取得
   *
   * @return SharedArrayBufferが有効な場合true
   */
  bool isSharedArrayBuffersEnabled() const;

  /**
   * @brief SharedArrayBufferの有効/無効を設定
   *
   * @param enabled 有効にする場合true
   */
  void setSharedArrayBuffersEnabled(bool enabled);

  /**
   * @brief ロケールを取得
   *
   * @return 現在のロケール
   */
  std::string getLocale() const;

  /**
   * @brief ロケールを設定
   *
   * @param locale 設定するロケール
   */
  void setLocale(const std::string& locale);

  /**
   * @brief ビルトインオブジェクトを初期化
   *
   * @param globalObj グローバルオブジェクト
   * @param hasConsole コンソールAPIを有効にするかどうか
   * @param hasModules ESモジュールを有効にするかどうか
   */
  void initializeBuiltins(Object* globalObj, bool hasConsole = true, bool hasModules = true);

  /**
   * @brief 値のクローンを作成
   *
   * @param value クローン元の値
   * @return クローンされた値
   */
  Value cloneValue(Value value);

 private:
  Type m_type;                        ///< コンテキストの種類
  Object* m_globalObject;             ///< グローバルオブジェクト
  Object* m_variableObject;           ///< 変数オブジェクト
  std::vector<Object*> m_scopeChain;  ///< スコープチェーン
  Value m_thisValue;                  ///< this値
  bool m_strictMode;                  ///< 厳格モードフラグ
  bool m_hasConsole;                  ///< コンソールAPI有効フラグ
  bool m_hasModules;                  ///< ESモジュール有効フラグ
  bool m_sharedArrayBuffersEnabled;   ///< SharedArrayBuffer有効フラグ
  std::string m_locale;               ///< ロケール
  bool m_isRunning;                   ///< 実行中フラグ

  // 内部実装用メソッド
  void initialize();
};

}  // namespace aero

#endif  // AERO_RUNTIME_CONTEXT_H