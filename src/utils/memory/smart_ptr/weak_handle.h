/**
 * @file weak_handle.h
 * @brief オブジェクトへの弱参照を管理するWeakHandleクラスの定義
 * @copyright 2023 AeroJS プロジェクト
 */

#ifndef AERO_WEAK_HANDLE_H
#define AERO_WEAK_HANDLE_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

namespace aero {

/**
 * @brief オブジェクトへの弱参照を管理するクラス
 * 
 * WeakRefおよびFinalizationRegistryの実装で使用される、
 * オブジェクトへの弱参照を安全に管理するためのテンプレートクラス。
 * 参照先のオブジェクトがガベージコレクトされた場合、自動的に無効になります。
 * 
 * @tparam T 参照するオブジェクトの型
 */
template <typename T>
class WeakHandle {
public:
  /**
   * @brief デフォルトコンストラクタ
   */
  WeakHandle() : m_ptr(nullptr), m_isValid(false) {}

  /**
   * @brief コンストラクタ
   * @param ptr 弱参照するオブジェクトへのポインタ
   */
  explicit WeakHandle(T* ptr) : m_ptr(ptr), m_isValid(ptr != nullptr) {}

  /**
   * @brief コピーコンストラクタ
   * @param other コピー元のWeakHandle
   */
  WeakHandle(const WeakHandle& other) {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    m_ptr = other.m_ptr;
    m_isValid.store(other.m_isValid.load(std::memory_order_acquire), std::memory_order_release);
  }

  /**
   * @brief ムーブコンストラクタ
   * @param other ムーブ元のWeakHandle
   */
  WeakHandle(WeakHandle&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    m_ptr = other.m_ptr;
    m_isValid.store(other.m_isValid.load(std::memory_order_acquire), std::memory_order_release);
    other.m_ptr = nullptr;
    other.m_isValid.store(false, std::memory_order_release);
  }

  /**
   * @brief コピー代入演算子
   * @param other コピー元のWeakHandle
   * @return *this
   */
  WeakHandle& operator=(const WeakHandle& other) {
    if (this != &other) {
      std::lock_guard<std::mutex> lockThis(m_mutex);
      std::lock_guard<std::mutex> lockOther(other.m_mutex);
      m_ptr = other.m_ptr;
      m_isValid.store(other.m_isValid.load(std::memory_order_acquire), std::memory_order_release);
    }
    return *this;
  }

  /**
   * @brief ムーブ代入演算子
   * @param other ムーブ元のWeakHandle
   * @return *this
   */
  WeakHandle& operator=(WeakHandle&& other) noexcept {
    if (this != &other) {
      std::lock_guard<std::mutex> lockThis(m_mutex);
      std::lock_guard<std::mutex> lockOther(other.m_mutex);
      m_ptr = other.m_ptr;
      m_isValid.store(other.m_isValid.load(std::memory_order_acquire), std::memory_order_release);
      other.m_ptr = nullptr;
      other.m_isValid.store(false, std::memory_order_release);
    }
    return *this;
  }

  /**
   * @brief デストラクタ
   */
  ~WeakHandle() = default;

  /**
   * @brief 参照先のオブジェクトを取得
   * @return 参照先のオブジェクト（無効な場合はnullptr）
   */
  T* get() const {
    if (!m_isValid.load(std::memory_order_acquire)) {
      return nullptr;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isValid.load(std::memory_order_acquire) ? m_ptr : nullptr;
  }

  /**
   * @brief 参照先のオブジェクトをリセット
   */
  void reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ptr = nullptr;
    m_isValid.store(false, std::memory_order_release);
  }

  /**
   * @brief 参照先のオブジェクトを設定
   * @param ptr 新しい参照先のオブジェクト
   */
  void reset(T* ptr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ptr = ptr;
    m_isValid.store(ptr != nullptr, std::memory_order_release);
  }

  /**
   * @brief 参照が有効かどうかを確認
   * @return 参照が有効な場合はtrue
   */
  bool isValid() const {
    return m_isValid.load(std::memory_order_acquire);
  }

  /**
   * @brief 参照を無効化（GC用）
   */
  void invalidate() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isValid.store(false, std::memory_order_release);
    // ポインタ自体はクリアしない（デバッグ用）
  }

  /**
   * @brief 比較演算子
   * @param other 比較対象のWeakHandle
   * @return 参照先が同じならtrue
   */
  bool operator==(const WeakHandle& other) const {
    return get() == other.get();
  }

  /**
   * @brief 比較演算子
   * @param other 比較対象のWeakHandle
   * @return 参照先が異なるならtrue
   */
  bool operator!=(const WeakHandle& other) const {
    return !(*this == other);
  }

private:
  /**
   * @brief 参照先のオブジェクトへのポインタ
   */
  T* m_ptr;

  /**
   * @brief 参照の有効状態
   * アトミックな操作で並列アクセスを安全に処理
   */
  mutable std::atomic<bool> m_isValid;

  /**
   * @brief スレッドセーフ操作のためのミューテックス
   */
  mutable std::mutex m_mutex;
};

} // namespace aero

#endif // AERO_WEAK_HANDLE_H 