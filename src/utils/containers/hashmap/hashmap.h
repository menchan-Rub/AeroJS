/**
 * @file hashmap.h
 * @brief 最先端のハッシュマップ実装
 * 
 * このハッシュマップはロビンフッドハッシング、SIMD命令、キャッシュ最適化技術を
 * 組み合わせて最高のパフォーマンスを実現します。
 * 
 * @author AeroJS Team
 * @version 2.0.0
 * @copyright MIT License
 */

#ifndef AEROJS_UTILS_CONTAINERS_HASHMAP_H
#define AEROJS_UTILS_CONTAINERS_HASHMAP_H

#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>
#include <immintrin.h> // SIMD命令用

namespace aerojs {
namespace utils {
namespace containers {

/**
 * @brief ハッシュ関数の特殊化を定義するためのテンプレート
 */
template <typename T, typename = void>
struct Hasher {
    size_t operator()(const T& key) const {
        return std::hash<T>{}(key);
    }
};

// 文字列特殊化（FNV-1a高速実装）
template <>
struct Hasher<std::string> {
    size_t operator()(const std::string& key) const {
        static constexpr size_t FNV_PRIME = 1099511628211ULL;
        static constexpr size_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
        
        size_t hash = FNV_OFFSET_BASIS;
        for (char c : key) {
            hash ^= static_cast<size_t>(c);
            hash *= FNV_PRIME;
        }
        return hash;
    }
};

// cstring特殊化
template <>
struct Hasher<const char*> {
    size_t operator()(const char* key) const {
        static constexpr size_t FNV_PRIME = 1099511628211ULL;
        static constexpr size_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
        
        size_t hash = FNV_OFFSET_BASIS;
        while (*key) {
            hash ^= static_cast<size_t>(*key);
            hash *= FNV_PRIME;
            ++key;
        }
        return hash;
    }
};

/**
 * @brief 等価比較関数の特殊化を定義するためのテンプレート
 */
template <typename T, typename = void>
struct KeyEqual {
    bool operator()(const T& lhs, const T& rhs) const {
        return lhs == rhs;
    }
};

// 文字列特殊化
template <>
struct KeyEqual<std::string> {
    bool operator()(const std::string& lhs, const std::string& rhs) const {
        return lhs == rhs;
    }
};

// cstring特殊化
template <>
struct KeyEqual<const char*> {
    bool operator()(const char* lhs, const char* rhs) const {
        return strcmp(lhs, rhs) == 0;
    }
};

// キーと値のペアを保存するエントリ構造体
template <typename Key, typename Value>
struct HashMapEntry {
    Key key;
    Value value;
    uint16_t distance = 0; // プローブ距離（ロビンフッドハッシング用）
    bool occupied = false; // 占有フラグ
    
    HashMapEntry() = default;
    
    HashMapEntry(const Key& k, const Value& v, uint16_t dist = 0)
        : key(k), value(v), distance(dist), occupied(true) {}
    
    HashMapEntry(Key&& k, Value&& v, uint16_t dist = 0)
        : key(std::move(k)), value(std::move(v)), distance(dist), occupied(true) {}
};

/**
 * @brief 超高性能ハッシュマップ実装
 * 
 * ロビンフッドハッシングとSIMD最適化を組み合わせた高性能ハッシュマップです。
 * キャッシュ効率、衝突回避、並列検索を最適化しています。
 * 
 * @tparam Key キーの型
 * @tparam Value 値の型
 * @tparam Hasher ハッシュ関数
 * @tparam KeyEqual キー比較関数
 */
template <
    typename Key,
    typename Value,
    typename Hash = Hasher<Key>,
    typename Equal = KeyEqual<Key>
>
class HashMap {
public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
    using size_type = size_t;
    using hasher = Hash;
    using key_equal = Equal;
    using entry_type = HashMapEntry<Key, Value>;
    
    /**
     * @brief イテレータクラス
     */
    class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const Key&, Value&>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type;
        
        Iterator(entry_type* entry, entry_type* end) : m_entry(entry), m_end(end) {
            // 最初の有効なエントリを見つける
            while (m_entry != m_end && !m_entry->occupied) {
                ++m_entry;
            }
        }
        
        // 前進演算子
        Iterator& operator++() {
            ++m_entry;
            // 次の有効なエントリを見つける
            while (m_entry != m_end && !m_entry->occupied) {
                ++m_entry;
            }
            return *this;
        }
        
        // 後置演算子
        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        // デリファレンス演算子
        reference operator*() const {
            return {m_entry->key, m_entry->value};
    }

    // 比較演算子
        bool operator==(const Iterator& other) const {
            return m_entry == other.m_entry;
        }
        
        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }
        
    private:
        entry_type* m_entry;
        entry_type* m_end;
    };
    
    // 定数イテレータ
    class ConstIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const Key&, const Value&>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type;
        
        ConstIterator(const entry_type* entry, const entry_type* end)
            : m_entry(entry), m_end(end) {
            // 最初の有効なエントリを見つける
            while (m_entry != m_end && !m_entry->occupied) {
                ++m_entry;
            }
        }
        
        // 前進演算子
        ConstIterator& operator++() {
            ++m_entry;
            // 次の有効なエントリを見つける
            while (m_entry != m_end && !m_entry->occupied) {
                ++m_entry;
            }
      return *this;
    }

        // 後置演算子
        ConstIterator operator++(int) {
            ConstIterator tmp = *this;
      ++(*this);
      return tmp;
    }

        // デリファレンス演算子
        reference operator*() const {
            return {m_entry->key, m_entry->value};
        }
        
        // 比較演算子
        bool operator==(const ConstIterator& other) const {
            return m_entry == other.m_entry;
        }
        
        bool operator!=(const ConstIterator& other) const {
            return !(*this == other);
        }
        
    private:
        const entry_type* m_entry;
        const entry_type* m_end;
    };
    
    using iterator = Iterator;
    using const_iterator = ConstIterator;
    
    /**
     * @brief デフォルトコンストラクタ
     * @param initial_capacity 初期容量
     * @param max_load_factor 最大負荷係数
     */
    explicit HashMap(
        size_type initial_capacity = 16,
        float max_load_factor = 0.75f
    )
        : m_max_load_factor(max_load_factor) {
        // 容量は2のべき乗に調整
        m_capacity = roundUpToPowerOfTwo(initial_capacity);
        m_entries.resize(m_capacity);
        m_size = 0;
        m_resize_threshold = static_cast<size_type>(m_capacity * m_max_load_factor);
    }
    
    /**
     * @brief 初期化リストコンストラクタ
     */
    HashMap(std::initializer_list<value_type> init, float max_load_factor = 0.75f)
        : HashMap(init.size(), max_load_factor) {
    for (const auto& pair : init) {
            insert(pair.first, pair.second);
    }
  }

    /**
     * @brief コピーコンストラクタ
     */
  HashMap(const HashMap& other)
      : m_entries(other.m_entries),
        m_size(other.m_size),
        m_capacity(other.m_capacity),
          m_max_load_factor(other.m_max_load_factor),
          m_resize_threshold(other.m_resize_threshold) {}
    
    /**
     * @brief ムーブコンストラクタ
     */
  HashMap(HashMap&& other) noexcept
      : m_entries(std::move(other.m_entries)),
        m_size(other.m_size),
        m_capacity(other.m_capacity),
          m_max_load_factor(other.m_max_load_factor),
          m_resize_threshold(other.m_resize_threshold) {
    other.m_size = 0;
    other.m_capacity = 0;
        other.m_resize_threshold = 0;
  }

    /**
     * @brief コピー代入演算子
     */
  HashMap& operator=(const HashMap& other) {
    if (this != &other) {
      m_entries = other.m_entries;
      m_size = other.m_size;
      m_capacity = other.m_capacity;
            m_max_load_factor = other.m_max_load_factor;
            m_resize_threshold = other.m_resize_threshold;
    }
    return *this;
  }

    /**
     * @brief ムーブ代入演算子
     */
  HashMap& operator=(HashMap&& other) noexcept {
    if (this != &other) {
      m_entries = std::move(other.m_entries);
      m_size = other.m_size;
      m_capacity = other.m_capacity;
            m_max_load_factor = other.m_max_load_factor;
            m_resize_threshold = other.m_resize_threshold;
            
      other.m_size = 0;
      other.m_capacity = 0;
            other.m_resize_threshold = 0;
    }
    return *this;
  }

    /**
     * @brief 要素の挿入
     * @param key キー
     * @param value 値
     * @return 挿入が成功したかどうか
     */
    bool insert(const Key& key, const Value& value) {
        return emplace(key, value);
    }
    
    /**
     * @brief 要素の挿入（ムーブ）
     */
    bool insert(Key&& key, Value&& value) {
        return emplace(std::move(key), std::move(value));
    }
    
    /**
     * @brief キーと値のペアを直接構築して挿入
     * @tparam Args 値のコンストラクタ引数型
     * @param key キー
     * @param args 値のコンストラクタに渡す引数
     * @return 挿入が成功したかどうか
     */
    template <typename... Args>
    bool emplace(const Key& key, Args&&... args) {
        // 負荷係数のチェックとリサイズ
        if (m_size >= m_resize_threshold) {
            rehash(m_capacity * 2);
        }
        
        size_type index = hash(key) & (m_capacity - 1);
        uint16_t distance = 0;
        entry_type new_entry(key, Value(std::forward<Args>(args)...), distance);
        
        // ロビンフッドハッシング
        while (true) {
            entry_type& entry = m_entries[index];
            
            // 空きスロットが見つかった場合
            if (!entry.occupied) {
                entry = std::move(new_entry);
                ++m_size;
                return true;
            }
            
            // 既存のキーと一致する場合
            if (m_equal(entry.key, key)) {
                return false; // 既存キーの場合は挿入失敗
            }
            
            // ロビンフッドハッシング: 新しいエントリの方が遠い場合、既存エントリと入れ替え
            if (new_entry.distance > entry.distance) {
                std::swap(new_entry, entry);
            }
            
            // 次のスロットへ
            new_entry.distance++;
            index = (index + 1) & (m_capacity - 1);
        }
    }
    
    /**
     * @brief キーに関連付けられた値を取得
     * @param key 検索するキー
     * @return キーに関連付けられた値への参照（キーが存在しない場合は例外）
     * @throws std::out_of_range キーが存在しない場合
     */
    Value& at(const Key& key) {
        auto entry = find_entry(key);
        if (!entry) {
            throw std::out_of_range("キーが見つかりません");
        }
        return entry->value;
    }
    
    /**
     * @brief キーに関連付けられた値を取得（const版）
     */
    const Value& at(const Key& key) const {
        auto entry = find_entry(key);
        if (!entry) {
            throw std::out_of_range("キーが見つかりません");
        }
        return entry->value;
    }
    
    /**
     * @brief 添字演算子によるアクセス
     * @param key 検索するキー
     * @return キーに関連付けられた値への参照
     * 
     * キーが存在しない場合、新しいエントリが作成される
     */
    Value& operator[](const Key& key) {
        auto entry = find_entry(key);
        if (entry) {
            return entry->value;
        }
        
        // キーが存在しない場合、デフォルト値で挿入
        insert(key, Value{});
        return find_entry(key)->value;
    }
    
    /**
     * @brief キーが存在するか確認
     * @param key 検索するキー
     * @return キーが存在すればtrue
     */
    bool contains(const Key& key) const {
        return find_entry(key) != nullptr;
    }
    
    /**
     * @brief キーに対応する値を取得
     * @param key 検索するキー
     * @return キーが存在すれば値を含むoptional、存在しなければnullopt
     */
    std::optional<std::reference_wrapper<Value>> get(const Key& key) {
        auto entry = find_entry(key);
        if (entry) {
            return std::optional<std::reference_wrapper<Value>>{entry->value};
        }
        return std::nullopt;
    }
    
    /**
     * @brief キーに対応する値を取得（const版）
     */
    std::optional<std::reference_wrapper<const Value>> get(const Key& key) const {
        auto entry = find_entry(key);
        if (entry) {
            return std::optional<std::reference_wrapper<const Value>>{entry->value};
        }
        return std::nullopt;
    }
    
    /**
     * @brief キーに対応するエントリを削除
     * @param key 削除するキー
     * @return 削除したエントリ数（0または1）
     */
    size_type erase(const Key& key) {
        auto index_opt = find_index(key);
        if (!index_opt) {
            return 0;
        }
        
        size_type index = *index_opt;
        
        // エントリを削除
        m_entries[index].occupied = false;
        --m_size;
        
        // 後続のエントリを再配置
        size_type next_index = (index + 1) & (m_capacity - 1);
        while (m_entries[next_index].occupied && m_entries[next_index].distance > 0) {
            // 前のスロットに移動
            m_entries[index] = std::move(m_entries[next_index]);
            m_entries[index].distance--; // 距離を1減らす
            
            // 次のスロットを空にする
            m_entries[next_index].occupied = false;
            
            // 次のペアに進む
            index = next_index;
            next_index = (next_index + 1) & (m_capacity - 1);
        }
        
        return 1;
    }
    
    /**
     * @brief 全てのエントリをクリア
     */
    void clear() {
        m_entries.clear();
        m_entries.resize(m_capacity);
        m_size = 0;
    }
    
    /**
     * @brief ハッシュマップのサイズ（要素数）を取得
     */
    size_type size() const noexcept {
        return m_size;
    }
    
    /**
     * @brief ハッシュマップが空かどうか
     */
    bool empty() const noexcept {
        return m_size == 0;
    }
    
    /**
     * @brief 現在の容量を取得
     */
    size_type capacity() const noexcept {
        return m_capacity;
    }
    
    /**
     * @brief イテレータを開始位置に設定
     */
    iterator begin() {
        return Iterator(m_entries.data(), m_entries.data() + m_capacity);
    }
    
    /**
     * @brief イテレータを終了位置に設定
     */
    iterator end() {
        return Iterator(m_entries.data() + m_capacity, m_entries.data() + m_capacity);
    }
    
    /**
     * @brief constイテレータを開始位置に設定
     */
    const_iterator begin() const {
        return ConstIterator(m_entries.data(), m_entries.data() + m_capacity);
    }
    
    /**
     * @brief constイテレータを終了位置に設定
     */
    const_iterator end() const {
        return ConstIterator(m_entries.data() + m_capacity, m_entries.data() + m_capacity);
    }
    
    /**
     * @brief constイテレータを開始位置に設定
     */
    const_iterator cbegin() const {
        return ConstIterator(m_entries.data(), m_entries.data() + m_capacity);
    }
    
    /**
     * @brief constイテレータを終了位置に設定
     */
    const_iterator cend() const {
        return ConstIterator(m_entries.data() + m_capacity, m_entries.data() + m_capacity);
    }
    
    /**
     * @brief 容量を指定してリハッシュ
     * @param new_capacity 新しい容量
     */
    void rehash(size_type new_capacity) {
        new_capacity = roundUpToPowerOfTwo(std::max(new_capacity, m_size * 2));
        
        // 古いエントリを保存
        std::vector<entry_type> old_entries = std::move(m_entries);
        size_type old_capacity = m_capacity;
        
        // 新しいベクトルを初期化
        m_capacity = new_capacity;
        m_resize_threshold = static_cast<size_type>(m_capacity * m_max_load_factor);
        m_entries.resize(m_capacity);
        m_size = 0;
        
        // 古いエントリを新しいテーブルに再挿入
        for (size_type i = 0; i < old_capacity; ++i) {
            if (old_entries[i].occupied) {
                emplace(
                    std::move(old_entries[i].key),
                    std::move(old_entries[i].value)
                );
            }
        }
    }
    
    /**
     * @brief 少なくとも指定された要素数を格納できるように容量を予約
     * @param count 予約する要素数
     */
    void reserve(size_type count) {
        // 負荷係数を考慮した必要容量を計算
        size_type required_capacity = static_cast<size_type>(count / m_max_load_factor) + 1;
        if (required_capacity > m_capacity) {
            rehash(required_capacity);
        }
    }
    
    /**
     * @brief 現在の負荷係数を取得
     */
    float load_factor() const {
        return static_cast<float>(m_size) / m_capacity;
    }
    
    /**
     * @brief 最大負荷係数を取得
     */
    float max_load_factor() const {
        return m_max_load_factor;
    }
    
    /**
     * @brief 最大負荷係数を設定
     */
    void max_load_factor(float ml) {
        m_max_load_factor = ml;
        m_resize_threshold = static_cast<size_type>(m_capacity * m_max_load_factor);
        if (load_factor() > m_max_load_factor) {
            rehash(m_capacity * 2);
        }
    }
    
private:
    std::vector<entry_type> m_entries;
    size_type m_size;
    size_type m_capacity;
    float m_max_load_factor;
    size_type m_resize_threshold;
    hasher m_hash{};
    key_equal m_equal{};
    
    /**
     * @brief キーに対応するエントリを検索
     * @param key 検索するキー
     * @return エントリへのポインタ（見つからない場合はnullptr）
     */
    entry_type* find_entry(const Key& key) {
        auto index_opt = find_index(key);
        if (!index_opt) {
            return nullptr;
        }
        return &m_entries[*index_opt];
    }
    
    /**
     * @brief キーに対応するエントリを検索（const版）
     */
    const entry_type* find_entry(const Key& key) const {
        auto index_opt = find_index(key);
        if (!index_opt) {
            return nullptr;
        }
        return &m_entries[*index_opt];
    }
    
    /**
     * @brief キーに対応するインデックスを検索
     * @param key 検索するキー
     * @return インデックスを含むoptional（見つからない場合はnullopt）
     */
    std::optional<size_type> find_index(const Key& key) const {
        if (m_size == 0) {
            return std::nullopt;
        }
        
        size_type index = hash(key) & (m_capacity - 1);
        uint16_t distance = 0;
        
        // SIMD最適化された検索（x86-64環境の場合）
        #if defined(__AVX2__)
        // 8個の連続したエントリを同時に検索
        if constexpr (std::is_same_v<Key, int32_t>) {
            return find_index_avx2(key, index);
        }
        #endif
        
        // 通常の線形探索
        while (true) {
            const entry_type& entry = m_entries[index];
            
            // スロットが空いているか、距離が長すぎる場合
            if (!entry.occupied || distance > entry.distance) {
                return std::nullopt;
            }
            
            // キーが一致する場合
            if (m_equal(entry.key, key)) {
                return index;
            }
            
            // 次のスロットへ
            index = (index + 1) & (m_capacity - 1);
            ++distance;
        }
    }
    
    #if defined(__AVX2__)
    /**
     * @brief AVX2命令を使用した高速なキー検索（int32_t型キー用）
     * @param key 検索するキー
     * @param start_index 開始インデックス
     * @return インデックスを含むoptional（見つからない場合はnullopt）
     */
    std::optional<size_type> find_index_avx2(int32_t key, size_type start_index) const {
        // 検索するキーを8回複製したベクトルを作成
        __m256i search_key = _mm256_set1_epi32(key);
        
        for (size_type i = 0; i < m_capacity; i += 8) {
            size_type index = (start_index + i) & (m_capacity - 1);
            
            // 連続する8つのエントリが確実に存在するか確認
            if (index + 7 < m_capacity) {
                // 8つのキーを同時にロード
                __m256i keys = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&m_entries[index].key));
                
                // 8つのoccupiedフラグをロード（バイト配列として）
                __m256i occupied_mask = _mm256_set_epi32(
                    m_entries[index + 7].occupied ? -1 : 0,
                    m_entries[index + 6].occupied ? -1 : 0,
                    m_entries[index + 5].occupied ? -1 : 0,
                    m_entries[index + 4].occupied ? -1 : 0,
                    m_entries[index + 3].occupied ? -1 : 0,
                    m_entries[index + 2].occupied ? -1 : 0,
                    m_entries[index + 1].occupied ? -1 : 0,
                    m_entries[index + 0].occupied ? -1 : 0
                );
                
                // キーの比較
                __m256i eq = _mm256_cmpeq_epi32(keys, search_key);
                
                // 比較結果とoccupiedマスクを論理積
                __m256i result = _mm256_and_si256(eq, occupied_mask);
                
                // 結果をマスクとして取得
                int mask = _mm256_movemask_ps(_mm256_castsi256_ps(result));
                
                // マスクが0でない場合、一致するキーが見つかった
                if (mask != 0) {
                    // 最下位ビットの位置を計算
                    unsigned long pos;
                    #ifdef _MSC_VER
                    _BitScanForward(&pos, mask);
                    #else
                    pos = __builtin_ctz(mask);
                    #endif
                    return index + pos;
                }
            } else {
                // 境界をまたぐ場合は通常の検索に切り替え
                for (size_type j = 0; j < 8 && index + j < m_capacity; ++j) {
                    const entry_type& entry = m_entries[index + j];
                    if (entry.occupied && entry.key == key) {
                        return index + j;
                    }
                }
            }
            
            // すべてのスロットが埋まっているか、距離が長すぎる場合
            bool all_occupied = true;
            for (size_type j = 0; j < 8 && index + j < m_capacity; ++j) {
                if (!m_entries[index + j].occupied) {
                    all_occupied = false;
                    break;
                }
            }
            
            if (!all_occupied) {
                return std::nullopt;
            }
        }
        
        return std::nullopt;
    }
    #endif
    
    /**
     * @brief キーのハッシュ値を計算
     * @param key ハッシュするキー
     * @return ハッシュ値
     */
    size_type hash(const Key& key) const {
        size_type h = m_hash(key);
        
        // セカンダリハッシュ（高品質な分布を保証）
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53ULL;
        h ^= h >> 33;
        
        return h;
    }
    
    /**
     * @brief 指定された数値を次の2のべき乗に切り上げる
     * @param n 切り上げる数値
     * @return 2のべき乗の数値
     */
    static size_type roundUpToPowerOfTwo(size_type n) {
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
      n |= n >> 32;
        ++n;
        return n;
    }
};

} // namespace containers
} // namespace utils
} // namespace aerojs

#endif // AEROJS_UTILS_CONTAINERS_HASHMAP_H