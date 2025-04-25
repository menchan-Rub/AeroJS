/**
 * @file promise.h
 * @brief JavaScriptのPromiseオブジェクトの定義
 * @copyright 2023 AeroJS プロジェクト
 */

#ifndef AERO_PROMISE_H
#define AERO_PROMISE_H

#include <functional>
#include <mutex>
#include <vector>

#include "../../global_object.h"
#include "../../object.h"
#include "../../value.h"

namespace aero {

/**
 * @brief Promiseの状態を表す列挙型
 */
enum class PromiseState {
  Pending,    ///< 保留中
  Fulfilled,  ///< 成功
  Rejected    ///< 失敗
};

/**
 * @brief Promiseオブジェクトを表すクラス
 *
 * ECMAScript仕様に準拠したPromiseオブジェクトの実装
 */
class PromiseObject : public Object {
 public:
  /**
   * @brief コンストラクタ
   * @param executor Promiseのexecutor関数 (function(resolve, reject) {...})
   * @param globalObj グローバルオブジェクト
   */
  PromiseObject(Value executor, GlobalObject* globalObj);

  /**
   * @brief デストラクタ
   */
  ~PromiseObject() override;

  /**
   * @brief Promiseの状態を取得
   * @return 現在のPromiseの状態
   */
  PromiseState getState() const;

  /**
   * @brief Promiseの結果値を取得
   * @return Promiseの結果値
   */
  Value getResult() const;

  /**
   * @brief Promiseを解決（成功）する
   * @param value 解決値
   */
  void resolve(const Value& value);

  /**
   * @brief Promiseを拒否（失敗）する
   * @param reason 拒否理由
   */
  void reject(const Value& reason);

  /**
   * @brief thenハンドラを登録する
   * @param onFulfilled 成功時のコールバック
   * @param onRejected 失敗時のコールバック
   * @return 新しいPromiseオブジェクト
   */
  PromiseObject* then(const Value& onFulfilled, const Value& onRejected);

  /**
   * @brief catchハンドラを登録する
   * @param onRejected 失敗時のコールバック
   * @return 新しいPromiseオブジェクト
   */
  PromiseObject* catch_(const Value& onRejected);

  /**
   * @brief finallyハンドラを登録する
   * @param onFinally 完了時のコールバック
   * @return 新しいPromiseオブジェクト
   */
  PromiseObject* finally_(const Value& onFinally);

  /**
   * @brief Promiseが解決または拒否されたときに実行する関数を登録
   * @param onFulfilled 成功時のコールバック
   * @param onRejected 失敗時のコールバック
   * @param resultPromise 新しいPromiseオブジェクト
   */
  void addReaction(const Value& onFulfilled, const Value& onRejected, PromiseObject* resultPromise);

  /**
   * @brief マイクロタスクキューにタスクを追加
   * @param task 実行するタスク
   */
  static void enqueueMicrotask(std::function<void()> task);

  /**
   * @brief 登録されたマイクロタスクを処理
   */
  static void processMicrotasks();

  /**
   * @brief 静的なプロトタイプオブジェクト
   */
  static Object* s_prototype;

 private:
  /**
   * @brief PromiseCapabilityオブジェクト
   *
   * Promiseの解決/拒否関数を保持する
   */
  struct PromiseCapability {
    PromiseObject* promise;  ///< Promiseオブジェクト
    Value resolveFunction;   ///< resolve関数
    Value rejectFunction;    ///< reject関数
  };

  /**
   * @brief Promiseリアクションオブジェクト
   *
   * then/catch/finallyで登録されたコールバックを保持する
   */
  struct PromiseReaction {
    Value handler;                 ///< ハンドラ関数
    PromiseObject* resultPromise;  ///< 結果Promise
    bool isReject;                 ///< 拒否ハンドラかどうか
  };

  PromiseState m_state;                      ///< 現在の状態
  Value m_result;                            ///< 結果値または拒否理由
  std::vector<PromiseReaction> m_reactions;  ///< 登録されたリアクション
  GlobalObject* m_globalObject;              ///< グローバルオブジェクト
  std::mutex m_mutex;                        ///< スレッドセーフのためのミューテックス

  /**
   * @brief Promiseリアクションを完了する
   * @param reactions 完了するリアクション
   * @param value 値または理由
   */
  void fulfillReactions(std::vector<PromiseReaction> reactions, const Value& value);

  /**
   * @brief Promiseリアクションを拒否する
   * @param reactions 拒否するリアクション
   * @param reason 拒否理由
   */
  void rejectReactions(std::vector<PromiseReaction> reactions, const Value& reason);

  /**
   * @brief 新しいPromise機能を作成
   * @param constructor コンストラクタ
   * @return PromiseCapabilityオブジェクト
   */
  static PromiseCapability newPromiseCapability(Value constructor);

  /**
   * @brief マイクロタスクキュー
   */
  static std::vector<std::function<void()>> s_microtaskQueue;

  /**
   * @brief マイクロタスクキュー用のミューテックス
   */
  static std::mutex s_microtaskMutex;
};

/**
 * @brief Promiseコンストラクタ関数
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 新しいPromiseオブジェクト
 */
Value promiseConstructor(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Promise.resolve 静的メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 値で解決されたPromiseオブジェクト
 */
Value promiseResolve(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Promise.reject 静的メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 理由で拒否されたPromiseオブジェクト
 */
Value promiseReject(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Promise.all 静的メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 全てのPromiseが解決されると解決されるPromiseオブジェクト
 */
Value promiseAll(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Promise.race 静的メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return いずれかのPromiseが解決または拒否されると決定されるPromiseオブジェクト
 */
Value promiseRace(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Promise.allSettled 静的メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 全てのPromiseが完了するとそれぞれの結果を含むPromiseオブジェクト
 */
Value promiseAllSettled(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Promise.any 静的メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return いずれかのPromiseが成功すると解決されるPromiseオブジェクト
 */
Value promiseAny(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Promise.prototype.then メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 新しいPromiseオブジェクト
 */
Value promiseThen(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Promise.prototype.catch メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 新しいPromiseオブジェクト
 */
Value promiseCatch(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Promise.prototype.finally メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 新しいPromiseオブジェクト
 */
Value promiseFinally(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Promiseプロトタイプの初期化
 * @param globalObj グローバルオブジェクト
 */
void initPromisePrototype(GlobalObject* globalObj);

/**
 * @brief Promiseオブジェクトの初期化
 * @param globalObj グローバルオブジェクト
 */
void initPromiseObject(GlobalObject* globalObj);

}  // namespace aero

#endif  // AERO_PROMISE_H