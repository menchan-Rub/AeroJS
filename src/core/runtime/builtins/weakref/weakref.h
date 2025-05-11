/**
 * @file weakref.h
 * @brief JavaScriptのWeakRefオブジェクトの定義
 * @copyright 2023 AeroJS プロジェクト
 */

#ifndef AERO_WEAKREF_H
#define AERO_WEAKREF_H

#include <atomic>
#include <memory>
#include <mutex>

#include "../../global_object.h"
#include "../../object.h"
#include "../../value.h"
#include "../../../utils/memory/smart_ptr/weak_handle.h"

namespace aero {

// 前方宣言
class GarbageCollector;

/**
 * @brief WeakRefオブジェクトを表すクラス
 *
 * ECMAScript仕様に準拠したWeakRefオブジェクトの実装。
 * 参照先のオブジェクトはガベージコレクションの対象となることがある。
 */
class WeakRefObject : public Object {
 public:
  /**
   * @brief コンストラクタ
   * @param target 弱参照するターゲットオブジェクト
   * @param globalObj グローバルオブジェクト
   */
  WeakRefObject(Value target, GlobalObject* globalObj);

  /**
   * @brief デストラクタ
   */
  ~WeakRefObject() override;

  /**
   * @brief オブジェクトの種類を示す名前を取得
   * @return クラス名
   */
  virtual std::string getClassName() const override {
    return "WeakRef";
  }

  /**
   * @brief WeakRefのインスタンスかどうかを確認
   * @return WeakRefインスタンスの場合はtrue
   */
  virtual bool isWeakRef() const override {
    return true;
  }

  /**
   * @brief 弱参照しているターゲットを取得する
   * @return ターゲットオブジェクト（既に解放されている場合はundefined）
   */
  Value deref() const;

  /**
   * @brief ターゲットを設定する（内部使用）
   * @param target 新しいターゲットオブジェクト
   */
  void setTarget(Value target);

  /**
   * @brief ターゲットをクリアする（内部使用）
   * ガベージコレクタから呼ばれる
   */
  void clearTarget();

  /**
   * @brief ガベージコレクション開始前の処理
   * @param gc ガベージコレクタへの参照
   */
  virtual void preGCCallback(GarbageCollector* gc) override;

  /**
   * @brief ガベージコレクション終了後の処理
   * @param gc ガベージコレクタへの参照
   */
  virtual void postGCCallback(GarbageCollector* gc) override;

  /**
   * @brief 静的なプロトタイプオブジェクト
   */
  static Object* s_prototype;

 private:
  /**
   * @brief 弱参照によるターゲットオブジェクト
   * AeroJSのメモリ管理システムと統合するWeakHandleを使用
   */
  WeakHandle<Object> m_target;

  /**
   * @brief グローバルオブジェクト
   */
  GlobalObject* m_globalObject;

  /**
   * @brief ターゲットの生存状態
   * アトミックな操作で並列アクセスを安全に処理
   */
  std::atomic<bool> m_targetAlive;

  /**
   * @brief スレッドセーフ操作のためのミューテックス
   */
  mutable std::mutex m_mutex;
};

/**
 * @brief WeakRefコンストラクタ関数
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 新しいWeakRefオブジェクト
 */
Value weakRefConstructor(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief WeakRef.prototype.deref メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return ターゲットオブジェクトまたはundefined
 */
Value weakRefDeref(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief WeakRefプロトタイプを初期化
 * @param globalObj グローバルオブジェクト
 */
void initWeakRefPrototype(GlobalObject* globalObj);

/**
 * @brief WeakRefオブジェクトを初期化
 * @param globalObj グローバルオブジェクト
 */
void initWeakRefObject(GlobalObject* globalObj);

} // namespace aero

#endif // AERO_WEAKREF_H 