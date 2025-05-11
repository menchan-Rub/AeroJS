/**
 * @file finalization_registry.h
 * @brief JavaScriptのFinalizationRegistryオブジェクトの定義
 * @copyright 2023 AeroJS プロジェクト
 */

#ifndef AERO_FINALIZATION_REGISTRY_H
#define AERO_FINALIZATION_REGISTRY_H

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <queue>

#include "../../global_object.h"
#include "../../object.h"
#include "../../value.h"
#include "../../../utils/memory/smart_ptr/weak_handle.h"

namespace aero {

// 前方宣言
class GarbageCollector;

/**
 * @brief FinalizationRegistryのエントリ情報
 */
struct RegistryEntry {
  WeakHandle<Object> target;         ///< 弱参照によるターゲットオブジェクト
  Value heldValue;                   ///< 保持する値
  Value unregisterToken;             ///< 登録解除トークン（オプション）
  std::atomic<bool> isTargetAlive;   ///< ターゲットの生存状態

  RegistryEntry() : isTargetAlive(false) {}
};

/**
 * @brief FinalizationRegistryオブジェクトを表すクラス
 *
 * ECMAScript仕様に準拠したFinalizationRegistryオブジェクトの実装。
 * オブジェクトがガベージコレクトされた後にクリーンアップコードを実行するための仕組み。
 */
class FinalizationRegistryObject : public Object {
 public:
  /**
   * @brief コンストラクタ
   * @param cleanupCallback クリーンアップコールバック関数
   * @param globalObj グローバルオブジェクト
   */
  FinalizationRegistryObject(Value cleanupCallback, GlobalObject* globalObj);

  /**
   * @brief デストラクタ
   */
  ~FinalizationRegistryObject() override;

  /**
   * @brief オブジェクトの種類を示す名前を取得
   * @return クラス名
   */
  virtual std::string getClassName() const override {
    return "FinalizationRegistry";
  }

  /**
   * @brief FinalizationRegistryのインスタンスかどうかを確認
   * @return FinalizationRegistryインスタンスの場合はtrue
   */
  virtual bool isFinalizationRegistry() const override {
    return true;
  }

  /**
   * @brief オブジェクトを登録する
   * @param target 監視対象オブジェクト
   * @param heldValue クリーンアップコールバックに渡される値
   * @param unregisterToken 登録解除のためのトークン（オプション）
   * @return 登録が成功したかどうか
   */
  bool register_(Value target, Value heldValue, Value unregisterToken = Value::undefined());

  /**
   * @brief 登録を解除する
   * @param unregisterToken 登録解除トークン
   * @return 登録解除が成功したかどうか
   */
  bool unregister(Value unregisterToken);

  /**
   * @brief クリーンアップ処理を実行する
   * ガベージコレクションの後に呼ばれる
   * @param callback オプションのコールバック関数
   */
  void cleanupSome(Value callback = Value::undefined());

  /**
   * @brief 監視しているターゲットのいずれかがGCされたか確認
   * @return クリーンアップが必要な場合はtrue
   */
  bool hasDeadTargets() const;

  /**
   * @brief クリーンアップコールバックを取得する
   * @return クリーンアップコールバック関数
   */
  Value getCleanupCallback() const;

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
   * @brief クリーンアップコールバック関数
   */
  Value m_cleanupCallback;

  /**
   * @brief グローバルオブジェクト
   */
  GlobalObject* m_globalObject;

  /**
   * @brief 登録されたエントリのリスト
   */
  std::vector<RegistryEntry> m_entries;

  /**
   * @brief クリーンアップ待ちのエントリのインデックスキュー
   */
  std::queue<size_t> m_cleanupQueue;

  /**
   * @brief unregisterTokenによるエントリの検索用マップ
   */
  std::unordered_map<Value, size_t> m_tokenMap;

  /**
   * @brief スレッドセーフ操作のための読み書きロック
   * 複数の読み取りを並列に行えるように共有ミューテックスを使用
   */
  mutable std::shared_mutex m_mutex;

  /**
   * @brief クリーンアップ処理中かどうかのフラグ
   */
  std::atomic<bool> m_isCleanupInProgress;

  /**
   * @brief 無効なエントリをリストから削除する
   * @return 削除されたエントリの数
   */
  size_t removeInvalidEntries();

  /**
   * @brief クリーンアップキューにエントリを追加
   * @param index エントリのインデックス
   */
  void addToCleanupQueue(size_t index);

  /**
   * @brief エントリを安全に削除
   * @param index 削除するエントリのインデックス
   */
  void safeRemoveEntry(size_t index);
};

/**
 * @brief FinalizationRegistryコンストラクタ関数
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 新しいFinalizationRegistryオブジェクト
 */
Value finalizationRegistryConstructor(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief FinalizationRegistry.prototype.register メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return undefined
 */
Value finalizationRegistryRegister(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief FinalizationRegistry.prototype.unregister メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 登録解除が成功したかどうか
 */
Value finalizationRegistryUnregister(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief FinalizationRegistry.prototype.cleanupSome メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return undefined
 */
Value finalizationRegistryCleanupSome(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief FinalizationRegistryプロトタイプを初期化
 * @param globalObj グローバルオブジェクト
 */
void initFinalizationRegistryPrototype(GlobalObject* globalObj);

/**
 * @brief FinalizationRegistryオブジェクトを初期化
 * @param globalObj グローバルオブジェクト
 */
void initFinalizationRegistryObject(GlobalObject* globalObj);

} // namespace aero

#endif // AERO_FINALIZATION_REGISTRY_H 