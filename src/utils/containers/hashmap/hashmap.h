/**
 * @file hashmap.h
 * @brief 高性能なハッシュマップの実装
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_HASHMAP_H
#define AEROJS_HASHMAP_H

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <type_traits>
#include <utility>
#include <vector>
#include <stdexcept>
#include <optional>
#include <cassert>
#include <algorithm>

namespace aerojs {
namespace utils {

// 前方宣言
template <typename KeyType, typename ValueType, 
          typename HashFn = std::hash<KeyType>,
          typename EqualFn = std::equal_to<KeyType>>
class HashMap;

/**
 * @brief 高性能な線形探索＋ロビンフッドハッシュマップの実装
 * 
 * このハッシュマップは、オープンアドレス法とロビンフッドハッシングを組み合わせた
 * 高性能な実装です。キャッシュ効率に優れ、高速な検索と挿入を提供します。
 * 
 * @tparam KeyType キーの型
 * @tparam ValueType 値の型
 * @tparam HashFn ハッシュ関数
 * @tparam EqualFn キー比較関数
 */
template <typename KeyType, typename ValueType, typename HashFn, typename EqualFn>
class HashMap {
public:
    using key_type = KeyType;
    using mapped_type = ValueType;
    using value_type = std::pair<const KeyType, ValueType>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = HashFn;
    using key_equal = EqualFn;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    // ハッシュマップの内部エントリ
    struct Entry {
        std::size_t hash;  // ハッシュ値
        std::size_t dist;  // 理想位置からの距離
        bool occupied;     // 使用中フラグ
        value_type kv;     // キーと値のペア

        // デフォルトコンストラクタ
        Entry() : hash(0), dist(0), occupied(false), kv(key_type(), mapped_type()) {}

        // 値初期化コンストラクタ
        template <typename K, typename V>
        Entry(std::size_t h, std::size_t d, K&& k, V&& v)
            : hash(h), dist(d), occupied(true),
              kv(std::forward<K>(k), std::forward<V>(v)) {}

        // コピーコンストラクタ
        Entry(const Entry& other)
            : hash(other.hash), dist(other.dist), occupied(other.occupied),
              kv(other.kv) {}

        // ムーブコンストラクタ
        Entry(Entry&& other) noexcept
            : hash(other.hash), dist(other.dist), occupied(other.occupied),
              kv(std::move(other.kv)) {
            other.occupied = false;
        }

        // コピー代入演算子
        Entry& operator=(const Entry& other) {
            if (this != &other) {
                hash = other.hash;
                dist = other.dist;
                occupied = other.occupied;
                kv = other.kv;
            }
            return *this;
        }

        // ムーブ代入演算子
        Entry& operator=(Entry&& other) noexcept {
            if (this != &other) {
                hash = other.hash;
                dist = other.dist;
                occupied = other.occupied;
                kv = std::move(other.kv);
                other.occupied = false;
            }
            return *this;
        }
    };

    // 前方宣言
    template <bool IsConst>
    class iterator_base;

    // 非const_iterator
    using iterator = iterator_base<false>;
    // const_iterator
    using const_iterator = iterator_base<true>;

    /**
     * @brief イテレータの基本実装
     * 
     * @tparam IsConst constイテレータかどうか
     */
    template <bool IsConst>
    class iterator_base {
    public:
        using difference_type = HashMap::difference_type;
        using value_type = std::conditional_t<IsConst, const HashMap::value_type, HashMap::value_type>;
        using pointer = std::conditional_t<IsConst, const HashMap::value_type*, HashMap::value_type*>;
        using reference = std::conditional_t<IsConst, const HashMap::value_type&, HashMap::value_type&>;
        using iterator_category = std::forward_iterator_tag;

        iterator_base() : m_ptr(nullptr), m_end(nullptr) {}

        iterator_base(const iterator_base& other) = default;
        iterator_base& operator=(const iterator_base& other) = default;

        // 非constからconstへの変換コンストラクタ
        template <bool OtherIsConst, std::enable_if_t<IsConst && !OtherIsConst, int> = 0>
        iterator_base(const iterator_base<OtherIsConst>& other)
            : m_ptr(other.m_ptr), m_end(other.m_end) {}

        // 比較演算子
        bool operator==(const iterator_base& other) const { return m_ptr == other.m_ptr; }
        bool operator!=(const iterator_base& other) const { return m_ptr != other.m_ptr; }

        // 参照演算子
        reference operator*() const { return m_ptr->kv; }
        pointer operator->() const { return &(m_ptr->kv); }

        // 前置インクリメント
        iterator_base& operator++() {
            do {
                ++m_ptr;
            } while (m_ptr != m_end && !m_ptr->occupied);
            return *this;
        }

        // 後置インクリメント
        iterator_base operator++(int) {
            iterator_base tmp(*this);
            ++(*this);
            return tmp;
        }

    private:
        friend class HashMap;
        friend class iterator_base<!IsConst>;

        using EntryPtr = std::conditional_t<IsConst, const Entry*, Entry*>;

        EntryPtr m_ptr;
        EntryPtr m_end;

        iterator_base(EntryPtr ptr, EntryPtr end) : m_ptr(ptr), m_end(end) {
            // 空のエントリを飛ばす
            if (m_ptr != m_end && !m_ptr->occupied) {
                ++(*this);
            }
        }
    };

    // デフォルトコンストラクタ
    HashMap()
        : m_entries(s_initialCapacity), 
          m_size(0),
          m_capacity(s_initialCapacity),
          m_maxLoadFactor(s_defaultMaxLoadFactor),
          m_hasher(), 
          m_equal() {}

    // ハッシュ関数とキー比較関数を指定するコンストラクタ
    explicit HashMap(const hasher& hash, const key_equal& equal = key_equal())
        : m_entries(s_initialCapacity), 
          m_size(0),
          m_capacity(s_initialCapacity),
          m_maxLoadFactor(s_defaultMaxLoadFactor),
          m_hasher(hash), 
          m_equal(equal) {}

    // 初期容量を指定するコンストラクタ
    explicit HashMap(size_type initialCapacity,
                    const hasher& hash = hasher(),
                    const key_equal& equal = key_equal())
        : m_entries(nextPowerOfTwo(initialCapacity)),
          m_size(0),
          m_capacity(nextPowerOfTwo(initialCapacity)),
          m_maxLoadFactor(s_defaultMaxLoadFactor),
          m_hasher(hash),
          m_equal(equal) {}

    // イニシャライザリストからのコンストラクタ
    HashMap(std::initializer_list<value_type> init,
           const hasher& hash = hasher(),
           const key_equal& equal = key_equal())
        : m_entries(nextPowerOfTwo(init.size())), 
          m_size(0),
          m_capacity(nextPowerOfTwo(init.size())),
          m_maxLoadFactor(s_defaultMaxLoadFactor),
          m_hasher(hash), 
          m_equal(equal) {
        for (const auto& pair : init) {
            insert(pair);
        }
    }

    // コピーコンストラクタ
    HashMap(const HashMap& other)
        : m_entries(other.m_entries),
          m_size(other.m_size),
          m_capacity(other.m_capacity),
          m_maxLoadFactor(other.m_maxLoadFactor),
          m_hasher(other.m_hasher),
          m_equal(other.m_equal) {}

    // ムーブコンストラクタ
    HashMap(HashMap&& other) noexcept
        : m_entries(std::move(other.m_entries)),
          m_size(other.m_size),
          m_capacity(other.m_capacity),
          m_maxLoadFactor(other.m_maxLoadFactor),
          m_hasher(std::move(other.m_hasher)),
          m_equal(std::move(other.m_equal)) {
        other.m_size = 0;
        other.m_capacity = 0;
    }

    // コピー代入演算子
    HashMap& operator=(const HashMap& other) {
        if (this != &other) {
            m_entries = other.m_entries;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            m_maxLoadFactor = other.m_maxLoadFactor;
            m_hasher = other.m_hasher;
            m_equal = other.m_equal;
        }
        return *this;
    }

    // ムーブ代入演算子
    HashMap& operator=(HashMap&& other) noexcept {
        if (this != &other) {
            m_entries = std::move(other.m_entries);
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            m_maxLoadFactor = other.m_maxLoadFactor;
            m_hasher = std::move(other.m_hasher);
            m_equal = std::move(other.m_equal);
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    // イニシャライザリストからの代入
    HashMap& operator=(std::initializer_list<value_type> ilist) {
        clear();
        reserve(ilist.size());
        for (const auto& pair : ilist) {
            insert(pair);
        }
        return *this;
    }

    // デストラクタ
    ~HashMap() = default;

    // イテレータ
    iterator begin() {
        return iterator(m_entries.data(), m_entries.data() + m_capacity);
    }

    const_iterator begin() const {
        return const_iterator(m_entries.data(), m_entries.data() + m_capacity);
    }

    const_iterator cbegin() const {
        return const_iterator(m_entries.data(), m_entries.data() + m_capacity);
    }

    iterator end() {
        return iterator(m_entries.data() + m_capacity, m_entries.data() + m_capacity);
    }

    const_iterator end() const {
        return const_iterator(m_entries.data() + m_capacity, m_entries.data() + m_capacity);
    }

    const_iterator cend() const {
        return const_iterator(m_entries.data() + m_capacity, m_entries.data() + m_capacity);
    }

    // 容量と要素数
    bool empty() const { return m_size == 0; }
    size_type size() const { return m_size; }
    size_type capacity() const { return m_capacity; }
    float load_factor() const { return static_cast<float>(m_size) / m_capacity; }
    float max_load_factor() const { return m_maxLoadFactor; }
    void max_load_factor(float ml) {
        m_maxLoadFactor = ml;
        if (load_factor() > m_maxLoadFactor) {
            rehash(std::ceil(m_size / m_maxLoadFactor));
        }
    }

    // 予約
    void reserve(size_type count) {
        size_type newCapacity = nextPowerOfTwo(std::ceil(count / m_maxLoadFactor));
        if (newCapacity > m_capacity) {
            rehash(newCapacity);
        }
    }

    // 要素アクセス
    mapped_type& at(const key_type& key) {
        auto it = find(key);
        if (it == end()) {
            throw std::out_of_range("HashMap::at: key not found");
        }
        return it->second;
    }

    const mapped_type& at(const key_type& key) const {
        auto it = find(key);
        if (it == end()) {
            throw std::out_of_range("HashMap::at: key not found");
        }
        return it->second;
    }

    mapped_type& operator[](const key_type& key) {
        auto [it, inserted] = try_emplace(key);
        return it->second;
    }

    mapped_type& operator[](key_type&& key) {
        auto [it, inserted] = try_emplace(std::move(key));
        return it->second;
    }

    // 要素検索
    iterator find(const key_type& key) {
        std::size_t hash = m_hasher(key);
        std::size_t idx = hash & (m_capacity - 1);
        std::size_t dist = 0;

        while (true) {
            const auto& entry = m_entries[idx];
            if (!entry.occupied) {
                return end();
            }
            if (entry.hash == hash && m_equal(entry.kv.first, key)) {
                return iterator(&m_entries[idx], m_entries.data() + m_capacity);
            }
            if (entry.dist < dist) {
                return end();
            }
            idx = (idx + 1) & (m_capacity - 1);
            ++dist;
        }
    }

    const_iterator find(const key_type& key) const {
        std::size_t hash = m_hasher(key);
        std::size_t idx = hash & (m_capacity - 1);
        std::size_t dist = 0;

        while (true) {
            const auto& entry = m_entries[idx];
            if (!entry.occupied) {
                return end();
            }
            if (entry.hash == hash && m_equal(entry.kv.first, key)) {
                return const_iterator(&m_entries[idx], m_entries.data() + m_capacity);
            }
            if (entry.dist < dist) {
                return end();
            }
            idx = (idx + 1) & (m_capacity - 1);
            ++dist;
        }
    }

    size_type count(const key_type& key) const {
        return find(key) != end() ? 1 : 0;
    }

    bool contains(const key_type& key) const {
        return find(key) != end();
    }

    // 要素挿入
    std::pair<iterator, bool> insert(const value_type& value) {
        return emplace(value.first, value.second);
    }

    std::pair<iterator, bool> insert(value_type&& value) {
        return emplace(std::move(value.first), std::move(value.second));
    }

    template <typename InputIt>
    void insert(InputIt first, InputIt last) {
        for (auto it = first; it != last; ++it) {
            insert(*it);
        }
    }

    void insert(std::initializer_list<value_type> ilist) {
        for (const auto& pair : ilist) {
            insert(pair);
        }
    }

    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        value_type pair(std::forward<Args>(args)...);
        return insertInternal(std::move(pair.first), std::move(pair.second));
    }

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
        if (shouldRehash()) {
            grow();
        }

        std::size_t hash = m_hasher(key);
        std::size_t idx = hash & (m_capacity - 1);
        std::size_t dist = 0;

        while (true) {
            Entry& entry = m_entries[idx];

            if (!entry.occupied) {
                // 新しいエントリを挿入
                entry = Entry(hash, dist, key, mapped_type(std::forward<Args>(args)...));
                ++m_size;
                return { iterator(&entry, m_entries.data() + m_capacity), true };
            }

            if (entry.hash == hash && m_equal(entry.kv.first, key)) {
                // キーが既に存在する場合
                return { iterator(&entry, m_entries.data() + m_capacity), false };
            }

            // ロビンフッドハッシング: より遠くにあるエントリと入れ替え
            if (entry.dist < dist) {
                // 新しいエントリを作成
                Entry newEntry(hash, dist, key, mapped_type(std::forward<Args>(args)...));
                // 既存のエントリと入れ替え
                std::swap(entry, newEntry);
                hash = newEntry.hash;
                key = newEntry.kv.first;
                dist = newEntry.dist;
            }

            idx = (idx + 1) & (m_capacity - 1);
            ++dist;
        }
    }

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
        if (shouldRehash()) {
            grow();
        }

        std::size_t hash = m_hasher(key);
        std::size_t idx = hash & (m_capacity - 1);
        std::size_t dist = 0;

        while (true) {
            Entry& entry = m_entries[idx];

            if (!entry.occupied) {
                // 新しいエントリを挿入
                entry = Entry(hash, dist, std::move(key), mapped_type(std::forward<Args>(args)...));
                ++m_size;
                return { iterator(&entry, m_entries.data() + m_capacity), true };
            }

            if (entry.hash == hash && m_equal(entry.kv.first, key)) {
                // キーが既に存在する場合
                return { iterator(&entry, m_entries.data() + m_capacity), false };
            }

            // ロビンフッドハッシング: より遠くにあるエントリと入れ替え
            if (entry.dist < dist) {
                // 新しいエントリを作成
                Entry newEntry(hash, dist, std::move(key), mapped_type(std::forward<Args>(args)...));
                // 既存のエントリと入れ替え
                std::swap(entry, newEntry);
                hash = newEntry.hash;
                key = std::move(newEntry.kv.first);
                dist = newEntry.dist;
            }

            idx = (idx + 1) & (m_capacity - 1);
            ++dist;
        }
    }

    // 要素削除
    iterator erase(const_iterator pos) {
        assert(pos != cend());

        // エントリを無効化
        Entry& entry = const_cast<Entry&>(*pos.m_ptr);
        entry.occupied = false;
        --m_size;

        // 次のエントリを返す
        return ++iterator(const_cast<Entry*>(pos.m_ptr), m_entries.data() + m_capacity);
    }

    size_type erase(const key_type& key) {
        auto it = find(key);
        if (it == end()) {
            return 0;
        }
        erase(it);
        return 1;
    }

    // コンテナの変更
    void clear() {
        for (auto& entry : m_entries) {
            entry.occupied = false;
        }
        m_size = 0;
    }

    void swap(HashMap& other) noexcept {
        m_entries.swap(other.m_entries);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
        std::swap(m_maxLoadFactor, other.m_maxLoadFactor);
        std::swap(m_hasher, other.m_hasher);
        std::swap(m_equal, other.m_equal);
    }

    // ハッシュポリシー
    void rehash(size_type count) {
        size_type newCapacity = nextPowerOfTwo(std::max(count, m_size));
        if (newCapacity == m_capacity) {
            return;
        }

        HashMap newMap(newCapacity, m_hasher, m_equal);
        newMap.m_maxLoadFactor = m_maxLoadFactor;

        for (auto& entry : m_entries) {
            if (entry.occupied) {
                newMap.insertInternal(std::move(entry.kv.first), std::move(entry.kv.second));
            }
        }

        *this = std::move(newMap);
    }

private:
    static constexpr size_type s_initialCapacity = 16;
    static constexpr float s_defaultMaxLoadFactor = 0.7f;

    std::vector<Entry> m_entries;
    size_type m_size;
    size_type m_capacity;
    float m_maxLoadFactor;
    hasher m_hasher;
    key_equal m_equal;

    // 次の2のべき乗を計算する
    static size_type nextPowerOfTwo(size_type n) {
        if (n == 0) return s_initialCapacity;
        
        --n;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        if constexpr (sizeof(size_type) > 4) {
            n |= n >> 32;
        }
        return n + 1;
    }

    // リハッシュが必要かどうかを判断
    bool shouldRehash() const {
        return (static_cast<float>(m_size + 1) / m_capacity) > m_maxLoadFactor;
    }

    // コンテナを拡張
    void grow() {
        rehash(m_capacity * 2);
    }

    // 内部挿入ヘルパー
    template <typename K, typename V>
    std::pair<iterator, bool> insertInternal(K&& key, V&& value) {
        if (shouldRehash()) {
            grow();
        }

        std::size_t hash = m_hasher(key);
        std::size_t idx = hash & (m_capacity - 1);
        std::size_t dist = 0;

        while (true) {
            Entry& entry = m_entries[idx];

            if (!entry.occupied) {
                // 新しいエントリを挿入
                entry = Entry(hash, dist, std::forward<K>(key), std::forward<V>(value));
                ++m_size;
                return { iterator(&entry, m_entries.data() + m_capacity), true };
            }

            if (entry.hash == hash && m_equal(entry.kv.first, key)) {
                // キーが既に存在する場合
                return { iterator(&entry, m_entries.data() + m_capacity), false };
            }

            // ロビンフッドハッシング: より遠くにあるエントリと入れ替え
            if (entry.dist < dist) {
                // 新しいエントリを作成
                Entry newEntry(hash, dist, std::forward<K>(key), std::forward<V>(value));
                // 既存のエントリと入れ替え
                std::swap(entry, newEntry);
                hash = newEntry.hash;
                key = std::move(newEntry.kv.first);
                value = std::move(newEntry.kv.second);
                dist = newEntry.dist;
            }

            idx = (idx + 1) & (m_capacity - 1);
            ++dist;
        }
    }
};

// 非メンバ関数
template <typename K, typename V, typename H, typename E>
void swap(HashMap<K, V, H, E>& lhs, HashMap<K, V, H, E>& rhs) noexcept {
    lhs.swap(rhs);
}

template <typename K, typename V, typename H, typename E>
bool operator==(const HashMap<K, V, H, E>& lhs, const HashMap<K, V, H, E>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (const auto& [key, value] : lhs) {
        auto it = rhs.find(key);
        if (it == rhs.end() || it->second != value) {
            return false;
        }
    }

    return true;
}

template <typename K, typename V, typename H, typename E>
bool operator!=(const HashMap<K, V, H, E>& lhs, const HashMap<K, V, H, E>& rhs) {
    return !(lhs == rhs);
}

} // namespace utils
} // namespace aerojs

#endif // AEROJS_HASHMAP_H 