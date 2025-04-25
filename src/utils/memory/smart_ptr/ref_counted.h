/**
 * @file ref_counted.h
 * @brief 参照カウント方式のスマートポインタの実装
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_REF_COUNTED_H
#define AEROJS_REF_COUNTED_H

#include <atomic>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace aerojs {
namespace utils {

/**
 * @brief 参照カウント可能なクラスのためのミックスイン
 *
 * このクラスをpublic継承することで、派生クラスは参照カウント機能を持ちます。
 * RefPtrと共に使用することを想定しています。
 */
class RefCounted {
 public:
  // デフォルトコンストラクタ
  RefCounted()
      : m_refCount(0) {
  }

  // コピー禁止
  RefCounted(const RefCounted&) = delete;
  RefCounted& operator=(const RefCounted&) = delete;

  // 参照カウント操作
  void ref() const {
    m_refCount.fetch_add(1, std::memory_order_relaxed);
  }

  void deref() const {
    // 参照カウントを1減らし、0になったら削除
    if (m_refCount.fetch_sub(1, std::memory_order_release) == 1) {
      std::atomic_thread_fence(std::memory_order_acquire);
      delete this;
    }
  }

  // 現在の参照カウントを取得（主にデバッグ用）
  std::size_t refCount() const {
    return m_refCount.load(std::memory_order_relaxed);
  }

 protected:
  // 派生クラスからのみデストラクタを呼べるように
  virtual ~RefCounted() = default;

 private:
  mutable std::atomic<std::size_t> m_refCount;
};

/**
 * @brief 参照カウント方式のスマートポインタ
 *
 * RefCountedを継承したクラスのインスタンスを安全に管理するためのスマートポインタです。
 *
 * @tparam T RefCountedを継承した型
 */
template <typename T>
class RefPtr {
  static_assert(std::is_base_of<RefCounted, T>::value, "T must inherit from RefCounted");

 public:
  // デフォルトコンストラクタ
  constexpr RefPtr()
      : m_ptr(nullptr) {
  }

  // nullptrからの構築
  constexpr RefPtr(std::nullptr_t)
      : m_ptr(nullptr) {
  }

  // 生ポインタからの構築
  explicit RefPtr(T* ptr)
      : m_ptr(ptr) {
    if (m_ptr)
      m_ptr->ref();
  }

  // コピーコンストラクタ
  RefPtr(const RefPtr& other)
      : m_ptr(other.m_ptr) {
    if (m_ptr)
      m_ptr->ref();
  }

  // ムーブコンストラクタ
  RefPtr(RefPtr&& other) noexcept
      : m_ptr(other.m_ptr) {
    other.m_ptr = nullptr;
  }

  // 変換コンストラクタ（派生クラスのRefPtrから基底クラスのRefPtrへの変換）
  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  RefPtr(const RefPtr<U>& other)
      : m_ptr(other.get()) {
    if (m_ptr)
      m_ptr->ref();
  }

  // デストラクタ
  ~RefPtr() {
    if (m_ptr)
      m_ptr->deref();
  }

  // コピー代入演算子
  RefPtr& operator=(const RefPtr& other) {
    RefPtr(other).swap(*this);
    return *this;
  }

  // ムーブ代入演算子
  RefPtr& operator=(RefPtr&& other) noexcept {
    RefPtr(std::move(other)).swap(*this);
    return *this;
  }

  // nullptr代入
  RefPtr& operator=(std::nullptr_t) {
    T* oldPtr = m_ptr;
    m_ptr = nullptr;
    if (oldPtr)
      oldPtr->deref();
    return *this;
  }

  // 生ポインタ代入
  RefPtr& operator=(T* ptr) {
    RefPtr(ptr).swap(*this);
    return *this;
  }

  // スワップ
  void swap(RefPtr& other) noexcept {
    std::swap(m_ptr, other.m_ptr);
  }

  // ポインタアクセス
  T* get() const {
    return m_ptr;
  }
  T& operator*() const {
    assert(m_ptr);
    return *m_ptr;
  }
  T* operator->() const {
    assert(m_ptr);
    return m_ptr;
  }

  // bool変換演算子
  explicit operator bool() const {
    return m_ptr != nullptr;
  }

  // リセット
  void reset() {
    *this = nullptr;
  }
  void reset(T* ptr) {
    *this = ptr;
  }

  // 現在の参照カウントを取得（主にデバッグ用）
  std::size_t refCount() const {
    return m_ptr ? m_ptr->refCount() : 0;
  }

  // 比較演算子
  friend bool operator==(const RefPtr& a, const RefPtr& b) {
    return a.m_ptr == b.m_ptr;
  }
  friend bool operator!=(const RefPtr& a, const RefPtr& b) {
    return a.m_ptr != b.m_ptr;
  }
  friend bool operator==(const RefPtr& a, std::nullptr_t) {
    return a.m_ptr == nullptr;
  }
  friend bool operator!=(const RefPtr& a, std::nullptr_t) {
    return a.m_ptr != nullptr;
  }
  friend bool operator==(std::nullptr_t, const RefPtr& b) {
    return nullptr == b.m_ptr;
  }
  friend bool operator!=(std::nullptr_t, const RefPtr& b) {
    return nullptr != b.m_ptr;
  }

  // RefPtr<U>から直接アクセスできるようにする
  template <typename U>
  friend class RefPtr;

 private:
  T* m_ptr;
};

// スワップ関数のオーバーロード
template <typename T>
inline void swap(RefPtr<T>& a, RefPtr<T>& b) noexcept {
  a.swap(b);
}

// ユーティリティ関数: makeRefPtr
template <typename T, typename... Args>
inline RefPtr<T> makeRefPtr(Args&&... args) {
  return RefPtr<T>(new T(std::forward<Args>(args)...));
}

}  // namespace utils
}  // namespace aerojs

#endif  // AEROJS_REF_COUNTED_H