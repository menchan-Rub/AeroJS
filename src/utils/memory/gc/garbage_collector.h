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
  void registerWeakRefProvider(std::function<void()> provider) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_weakRefProviders.push_back(provider);
  }

  /**
   * @brief Finalizationコールバックを登録
   * @param callback Finalizationコールバック
   */
  void registerFinalizationCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_finalizationCallbacks.push_back(callback);
  }

  /**
   * @brief GCをトリガー
   * @param force 強制的にGCを実行する場合はtrue
   */
  void triggerGC(bool force = false) {
    // GCが既に実行中ならスキップ
    bool expected = false;
    if (!m_isCollecting.compare_exchange_strong(expected, true)) {
      return;
    }

    // 並列コレクションが設定されている場合は別スレッドで実行
    if (m_parallelCollection && !force) {
      std::thread collector([this]() {
        this->collectGarbage();
        m_isCollecting.store(false);
      });
      collector.detach();
    } else {
      // 直接実行
      collectGarbage();
      m_isCollecting.store(false);
    }
  }

  /**
   * @brief FinalizationRegistryの処理
   * ガベージコレクション後にFinalizationRegistryのコールバックを実行
   */
  void processFinalizationRegistries() {
    std::vector<std::function<void()>> callbacks;
    
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      callbacks = m_finalizationCallbacks;
    }
    
    // 各コールバックを実行
    for (const auto& callback : callbacks) {
      try {
        callback();
      } catch (...) {
        // エラーを無視（コールバックチェーンを中断しない）
      }
    }
  }

  /**
   * @brief 並列コレクションの設定
   * @param enabled 有効にする場合はtrue
   */
  void setParallelCollection(bool enabled) {
    m_parallelCollection = enabled;
  }

  /**
   * @brief デバッグモードの設定
   * @param enabled 有効にする場合はtrue
   */
  void setDebugMode(bool enabled) {
    m_debugMode = enabled;
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
  void collectGarbage() {
    if (m_debugMode) {
      printf("GC: Starting garbage collection\n");
    }

    // フェーズ1: マーク処理（WeakRefオブジェクトの特定を含む）
    markPhase();

    // フェーズ2: スイープ処理（オブジェクトの解放）
    std::vector<Object*> collectedObjects = sweepPhase();

    // フェーズ3: WeakRefの更新
    updateWeakRefs();

    // フェーズ4: Finalizationコールバックの実行
    if (!collectedObjects.empty()) {
      processFinalizationRegistries();
    }

    if (m_debugMode) {
      printf("GC: Completed garbage collection, collected %zu objects\n", 
             collectedObjects.size());
    }
  }

  /**
   * @brief マーク処理
   * 到達可能なオブジェクトをマークする
   */
  void markPhase() {
    // 実際の実装では、到達可能なオブジェクトをマークする処理が入る
    // ここではモックアップとして空の実装
  }

  /**
   * @brief スイープ処理
   * マークされていないオブジェクトを解放する
   * @return 収集されたオブジェクトのリスト
   */
  std::vector<Object*> sweepPhase() {
    // 実際の実装では、マークされていないオブジェクトを解放する処理が入る
    // ここではモックアップとして空のリストを返す
    return std::vector<Object*>();
  }

  /**
   * @brief WeakRefの更新
   * GC後にWeakRefオブジェクトを更新する
   */
  void updateWeakRefs() {
    std::vector<std::function<void()>> providers;
    
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      providers = m_weakRefProviders;
    }
    
    // 各プロバイダを実行
    for (const auto& provider : providers) {
      try {
        provider();
      } catch (...) {
        // エラーを無視
      }
    }
  }

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
};

} // namespace aero

#endif // AERO_GARBAGE_COLLECTOR_H 