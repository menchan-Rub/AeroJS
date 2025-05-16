/**
 * @file garbage_collector.h
 * @brief ガベージコレクタのインターフェース
 * @copyright 2023 AeroJS プロジェクト
 */

#ifndef AERO_GARBAGE_COLLECTOR_H
#define AERO_GARBAGE_COLLECTOR_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_set>

namespace aero {

// 前方宣言
class Object;
template <typename T> class WeakHandle;

/**
 * @brief ガベージコレクタクラス
 * 
 * JavaScriptオブジェクトのメモリ管理を行うガベージコレクタ。
 * WeakRefやFinalizationRegistryとの連携機能を持つ。
 */
class GarbageCollector {
public:
  /**
   * @brief シングルトンインスタンスを取得
   * @return GarbageCollectorのインスタンス
   */
  static GarbageCollector* getInstance() {
    static GarbageCollector instance;
    return &instance;
  }

  /**
   * @brief コピーコンストラクタを削除（シングルトン）
   */
  GarbageCollector(const GarbageCollector&) = delete;

  /**
   * @brief 代入演算子を削除（シングルトン）
   */
  GarbageCollector& operator=(const GarbageCollector&) = delete;

  /**
   * @brief WeakHandleを登録
   * @param handle 登録するWeakHandleへのポインタ
   */
  template <typename T>
  void registerWeakHandle(WeakHandle<T>* handle) {
    if (handle) {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_registeredWeakHandles.insert(static_cast<void*>(handle));
    }
  }

  /**
   * @brief WeakRefのプロバイダコールバックを登録
   * @param provider プロバイダコールバック
   */
  void registerWeakRefProvider(std::function<void()> provider);

  /**
   * @brief Finalizationコールバックを登録
   * @param callback Finalizationコールバック
   */
  void registerFinalizationCallback(std::function<void()> callback);

  /**
   * @brief GCをトリガー
   * @param force 強制的にGCを実行する場合はtrue
   */
  void triggerGC(bool force = false);

  /**
   * @brief FinalizationRegistryの処理
   * ガベージコレクション後にFinalizationRegistryのコールバックを実行
   */
  void processFinalizationRegistries();

  /**
   * @brief 並列コレクションの設定
   * @param enabled 有効にする場合はtrue
   */
  void setParallelCollection(bool enabled);

  /**
   * @brief デバッグモードの設定
   * @param enabled 有効にする場合はtrue
   */
  void setDebugMode(bool enabled);

  /**
   * @brief オブジェクトをGC管理対象として登録
   * @param obj 管理対象のオブジェクト
   */
  void registerObject(Object* obj) {
    if (obj) {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_managedObjects.insert(obj);
    }
  }

  /**
   * @brief ルートオブジェクトを登録
   * @param root ルートオブジェクトへのポインタ
   */
  void addRoot(Object** root) {
    if (root) {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_roots.push_back(root);
    }
  }

  /**
   * @brief ルートオブジェクトの登録解除
   * @param root 登録解除するルートオブジェクトへのポインタ
   */
  void removeRoot(Object** root) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_roots.erase(
      std::remove(m_roots.begin(), m_roots.end(), root),
      m_roots.end()
    );
  }

  /**
   * @brief グローバルハンドルを登録
   * @param handle グローバルハンドルへのポインタ
   */
  void addGlobalHandle(Object** handle) {
    if (handle) {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_globalHandles.push_back(handle);
    }
  }

  /**
   * @brief グローバルハンドルの登録解除
   * @param handle 登録解除するグローバルハンドルへのポインタ
   */
  void removeGlobalHandle(Object** handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_globalHandles.erase(
      std::remove(m_globalHandles.begin(), m_globalHandles.end(), handle),
      m_globalHandles.end()
    );
  }

private:
  /**
   * @brief コンストラクタ（シングルトン）
   */
  GarbageCollector() 
    : m_isCollecting(false),
      m_parallelCollection(true),
      m_debugMode(false) {}

  /**
   * @brief デストラクタ
   */
  ~GarbageCollector() = default;

  /**
   * @brief ガベージコレクション処理
   * オブジェクトの収集と解放を行う
   */
  void collectGarbage();

  /**
   * @brief マーク処理
   * 到達可能なオブジェクトをマークする
   */
  void markPhase();

  /**
   * @brief スイープ処理
   * マークされていないオブジェクトを解放する
   * @return 収集されたオブジェクトのリスト
   */
  std::vector<Object*> sweepPhase();

  /**
   * @brief WeakRefの更新
   * GC後にWeakRefオブジェクトを更新する
   */
  void updateWeakRefs();

  /**
   * @brief WeakRefオブジェクトの処理
   * 生存しているオブジェクトを参照するWeakRefのみ有効にする
   */
  void processWeakReferences();

  /**
   * @brief WeakHandleの無効化
   * @param obj 無効化の対象となるオブジェクト
   */
  void invalidateWeakHandles(Object* obj);

  /**
   * @brief ミューテックス
   */
  std::mutex m_mutex;

  /**
   * @brief GC実行中フラグ
   */
  std::atomic<bool> m_isCollecting;

  /**
   * @brief 並列コレクションフラグ
   */
  bool m_parallelCollection;

  /**
   * @brief デバッグモードフラグ
   */
  bool m_debugMode;

  /**
   * @brief 登録されたWeakHandleのセット
   */
  std::unordered_set<void*> m_registeredWeakHandles;

  /**
   * @brief WeakRefプロバイダのリスト
   */
  std::vector<std::function<void()>> m_weakRefProviders;

  /**
   * @brief Finalizationコールバックのリスト
   */
  std::vector<std::function<void()>> m_finalizationCallbacks;

  /**
   * @brief 管理対象オブジェクトのセット
   */
  std::unordered_set<Object*> m_managedObjects;

  /**
   * @brief ルートオブジェクトリスト
   */
  std::vector<Object**> m_roots;

  /**
   * @brief グローバルハンドルリスト
   */
  std::vector<Object**> m_globalHandles;
};

} // namespace aero

#endif // AERO_GARBAGE_COLLECTOR_H 