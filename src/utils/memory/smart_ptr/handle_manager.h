/**
 * @file handle_manager.h
 * @brief WeakHandleの管理を行うHandleManagerクラスの定義
 * @copyright 2023 AeroJS プロジェクト
 */

#ifndef AERO_HANDLE_MANAGER_H
#define AERO_HANDLE_MANAGER_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "weak_handle.h"

namespace aero {

// 前方宣言
class Object;

/**
 * @brief WeakHandleの管理を行うシングルトンクラス
 * 
 * WeakRefおよびFinalizationRegistry実装で使用される、
 * WeakHandleの生成と管理を一元的に行うためのシングルトンクラス。
 * GCと連携して弱参照の状態を管理します。
 */
class HandleManager {
public:
  /**
   * @brief シングルトンインスタンスを取得
   * @return HandleManagerのインスタンス
   */
  static HandleManager* getInstance() {
    static HandleManager instance;
    return &instance;
  }

  /**
   * @brief コピーコンストラクタを削除（シングルトン）
   */
  HandleManager(const HandleManager&) = delete;

  /**
   * @brief 代入演算子を削除（シングルトン）
   */
  HandleManager& operator=(const HandleManager&) = delete;

  /**
   * @brief オブジェクトに対するWeakHandleを作成
   * @tparam T オブジェクトの型
   * @param obj 参照対象のオブジェクト
   * @return WeakHandle
   */
  template <typename T>
  WeakHandle<T> createWeakHandle(T* obj) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    WeakHandle<T> handle(obj);
    
    // 作成したWeakHandleを追跡リストに追加
    if (obj) {
      m_activeHandles++;
    }
    
    return handle;
  }

  /**
   * @brief 個別のWeakHandleをGCに登録
   * @param handle 登録するWeakHandleへのポインタ
   */
  void registerWeakHandle(void* handle);

  /**
   * @brief オブジェクト種別ごとのハンドルプロバイダを登録
   * @param typeName オブジェクト種別名
   * @param provider オブジェクト種別を判別する関数
   */
  void registerHandleProvider(const std::string& typeName, 
                             std::function<bool(Object*)> provider) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handleProviders[typeName] = provider;
  }

  /**
   * @brief GC前の準備処理
   */
  void prepareForGC() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingInvalidation = 0;
  }

  /**
   * @brief GC後の処理
   * @param invalidatedObjects GCで回収されたオブジェクトのリスト
   */
  void afterGC(const std::vector<Object*>& invalidatedObjects) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // GCで回収されたオブジェクトのハンドルを無効化
    for (Object* obj : invalidatedObjects) {
      // 各オブジェクト種別に対応するハンドラを呼び出す
      for (const auto& providerPair : m_handleProviders) {
        if (providerPair.second(obj)) {
          m_pendingInvalidation++;
          // 実際のハンドル無効化はGCが処理
        }
      }
    }
    
    // 統計情報の更新
    m_activeHandles -= m_pendingInvalidation;
    m_totalInvalidated += m_pendingInvalidation;
    
    // デバッグ情報
    if (m_pendingInvalidation > 0 && m_debugMode) {
      printf("HandleManager: Invalidated %zu handles, active: %zu, total invalidated: %zu\n",
             m_pendingInvalidation, m_activeHandles.load(), m_totalInvalidated.load());
    }
  }

  /**
   * @brief デバッグモードを設定
   * @param enabled 有効にする場合はtrue
   */
  void setDebugMode(bool enabled) {
    m_debugMode = enabled;
  }

  /**
   * @brief 有効なハンドル数を取得
   * @return 有効なハンドル数
   */
  size_t activeHandleCount() const {
    return m_activeHandles.load();
  }

  /**
   * @brief 無効化されたハンドルの累計数を取得
   * @return 無効化されたハンドルの累計数
   */
  size_t totalInvalidatedCount() const {
    return m_totalInvalidated.load();
  }

private:
  /**
   * @brief コンストラクタ（シングルトン）
   */
  HandleManager() 
    : m_activeHandles(0),
      m_pendingInvalidation(0),
      m_totalInvalidated(0),
      m_debugMode(false) {}

  /**
   * @brief デストラクタ
   */
  ~HandleManager() = default;

  /**
   * @brief スレッドセーフ操作のためのミューテックス
   */
  mutable std::mutex m_mutex;

  /**
   * @brief 現在有効なハンドル数
   */
  std::atomic<size_t> m_activeHandles;

  /**
   * @brief 次回GCで無効化予定のハンドル数
   */
  size_t m_pendingInvalidation;

  /**
   * @brief 累計で無効化されたハンドル数
   */
  std::atomic<size_t> m_totalInvalidated;

  /**
   * @brief オブジェクト種別ごとのハンドルプロバイダマップ
   */
  std::unordered_map<std::string, std::function<bool(Object*)>> m_handleProviders;

  /**
   * @brief デバッグモードフラグ
   */
  bool m_debugMode;
};

} // namespace aero

#endif // AERO_HANDLE_MANAGER_H 