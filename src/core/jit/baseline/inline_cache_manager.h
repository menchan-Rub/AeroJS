#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace aerojs {
namespace core {

/**
 * @brief インラインキャッシュのタイプを表す列挙型
 */
enum class ICType {
    kProperty,       // プロパティアクセス用
    kMethod,         // メソッド呼び出し用
    kConstructor,    // コンストラクタ呼び出し用
    kPrototype,      // プロトタイプチェーン用
    kComparison,     // 比較操作用
    kBinaryOp,       // 二項演算用
    kUnaryOp,        // 単項演算用
    kTypeCheck       // 型チェック用
};

/**
 * @brief インラインキャッシュのアクセス結果を表す列挙型
 */
enum class ICAccessResult {
    kHit,            // キャッシュヒット
    kMiss,           // キャッシュミス
    kTypeError,      // 型エラー
    kInvalidated,    // 無効化
    kOverflow        // オーバーフロー
};

/**
 * @brief インラインキャッシュエントリ
 */
struct ICEntry {
    uint64_t key;               // キャッシュキー
    uint64_t value;             // キャッシュ値
    uint32_t hitCount;          // ヒット回数
    uint32_t flags;             // フラグ
    
    ICEntry(uint64_t k = 0, uint64_t v = 0, uint32_t flags = 0)
        : key(k), value(v), hitCount(0), flags(flags) {}
};

/**
 * @brief インラインキャッシュ
 */
class InlineCache {
public:
    /**
     * @brief コンストラクタ
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param maxEntries 最大エントリ数
     */
    InlineCache(const std::string& cacheId, ICType type, size_t maxEntries = 4);
    
    /**
     * @brief デストラクタ
     */
    ~InlineCache();
    
    /**
     * @brief キャッシュIDを取得
     * @return キャッシュID
     */
    const std::string& GetId() const;
    
    /**
     * @brief キャッシュタイプを取得
     * @return キャッシュタイプ
     */
    ICType GetType() const;
    
    /**
     * @brief キャッシュからエントリを検索
     * @param key キャッシュキー
     * @param value [出力] 検索結果の値
     * @return アクセス結果
     */
    ICAccessResult Lookup(uint64_t key, uint64_t& value);
    
    /**
     * @brief キャッシュにエントリを追加
     * @param key キャッシュキー
     * @param value キャッシュ値
     * @param flags オプションフラグ
     */
    void Add(uint64_t key, uint64_t value, uint32_t flags = 0);
    
    /**
     * @brief キャッシュエントリを無効化
     * @param key 無効化するキャッシュのキー
     * @return 無効化成功の場合はtrue
     */
    bool Invalidate(uint64_t key);
    
    /**
     * @brief キャッシュ全体をクリア
     */
    void Clear();
    
    /**
     * @brief キャッシュヒット率を取得
     * @return ヒット率（0.0～1.0）
     */
    double GetHitRate() const;
    
    /**
     * @brief 最大エントリ数を設定
     * @param maxEntries 最大エントリ数
     */
    void SetMaxEntries(size_t maxEntries);
    
    /**
     * @brief 現在のエントリ数を取得
     * @return エントリ数
     */
    size_t GetEntryCount() const;
    
    /**
     * @brief キャッシュの全エントリを取得
     * @return エントリのベクトル
     */
    std::vector<ICEntry> GetEntries() const;
    
    /**
     * @brief パフォーマンス統計をリセット
     */
    void ResetStats();

private:
    // キャッシュID
    std::string m_id;
    
    // キャッシュタイプ
    ICType m_type;
    
    // エントリの配列
    std::vector<ICEntry> m_entries;
    
    // 最大エントリ数
    size_t m_maxEntries;
    
    // アクセス統計
    struct {
        std::atomic<uint64_t> lookups;     // 検索回数
        std::atomic<uint64_t> hits;        // ヒット数
        std::atomic<uint64_t> misses;      // ミス数
        std::atomic<uint64_t> invalidations; // 無効化数
        std::atomic<uint64_t> typeErrors;  // 型エラー数
    } m_stats;
    
    // ミューテックス
    mutable std::mutex m_mutex;
};

/**
 * @brief インラインキャッシュマネージャー
 * 
 * JITコンパイラ全体でインラインキャッシュを管理するクラス
 */
class InlineCacheManager {
public:
    /**
     * @brief シングルトンインスタンスを取得
     * @return InlineCacheManagerのインスタンス
     */
    static InlineCacheManager& Instance();
    
    /**
     * @brief デストラクタ
     */
    ~InlineCacheManager();
    
    /**
     * @brief キャッシュを作成または取得
     * @param cacheId キャッシュID
     * @param type キャッシュタイプ
     * @param maxEntries 最大エントリ数
     * @return インラインキャッシュへのポインタ
     */
    InlineCache* GetOrCreateCache(const std::string& cacheId, ICType type, size_t maxEntries = 4);
    
    /**
     * @brief キャッシュを削除
     * @param cacheId キャッシュID
     * @return 削除成功の場合はtrue
     */
    bool RemoveCache(const std::string& cacheId);
    
    /**
     * @brief キャッシュを取得
     * @param cacheId キャッシュID
     * @return インラインキャッシュへのポインタ（存在しない場合はnullptr）
     */
    InlineCache* GetCache(const std::string& cacheId);
    
    /**
     * @brief 指定されたタイプのすべてのキャッシュを取得
     * @param type キャッシュタイプ
     * @return インラインキャッシュのベクトル
     */
    std::vector<InlineCache*> GetCachesByType(ICType type);
    
    /**
     * @brief すべてのキャッシュを取得
     * @return キャッシュIDとインラインキャッシュのマップ
     */
    std::unordered_map<std::string, InlineCache*> GetAllCaches();
    
    /**
     * @brief すべてのキャッシュをクリア
     */
    void ClearAllCaches();
    
    /**
     * @brief 指定されたタイプのすべてのキャッシュをクリア
     * @param type キャッシュタイプ
     */
    void ClearCachesByType(ICType type);
    
    /**
     * @brief 設定されているキャッシュの最大数を取得
     * @return キャッシュの最大数
     */
    size_t GetMaxCacheCount() const;
    
    /**
     * @brief キャッシュの最大数を設定
     * @param maxCaches キャッシュの最大数
     */
    void SetMaxCacheCount(size_t maxCaches);
    
    /**
     * @brief インラインキャッシュが有効かどうかを取得
     * @return 有効な場合はtrue
     */
    bool IsEnabled() const;
    
    /**
     * @brief インラインキャッシュを有効または無効にする
     * @param enabled 有効にする場合はtrue
     */
    void SetEnabled(bool enabled);
    
    /**
     * @brief すべてのキャッシュの統計情報をリセット
     */
    void ResetAllStats();
    
    /**
     * @brief グローバルヒット率を取得
     * @return 全体のヒット率（0.0～1.0）
     */
    double GetGlobalHitRate() const;
    
    /**
     * @brief キャッシュに関する統計レポートを生成
     * @param detailed 詳細レポートを生成する場合はtrue
     * @return レポート文字列
     */
    std::string GenerateReport(bool detailed = false) const;

private:
    /**
     * @brief コンストラクタ（シングルトンパターン）
     */
    InlineCacheManager();
    
    // コピー禁止
    InlineCacheManager(const InlineCacheManager&) = delete;
    InlineCacheManager& operator=(const InlineCacheManager&) = delete;
    
    // キャッシュの最大数を超えた場合に古いキャッシュを削除
    void PruneCache();
    
    // キャッシュのマップ
    std::unordered_map<std::string, std::unique_ptr<InlineCache>> m_caches;
    
    // キャッシュの最大数
    size_t m_maxCacheCount;
    
    // インラインキャッシュが有効かどうか
    std::atomic<bool> m_enabled;
    
    // ミューテックス
    mutable std::mutex m_mutex;
    
    // グローバル統計
    struct {
        std::atomic<uint64_t> lookups;     // 検索回数
        std::atomic<uint64_t> hits;        // ヒット数
        std::atomic<uint64_t> misses;      // ミス数
        std::atomic<uint64_t> invalidations; // 無効化数
        std::atomic<uint64_t> typeErrors;  // 型エラー数
    } m_globalStats;
};

/**
 * @brief インラインキャッシュのICTypeを文字列に変換
 * @param type 変換するICType
 * @return タイプの文字列表現
 */
std::string ICTypeToString(ICType type);

/**
 * @brief インラインキャッシュのICAccessResultを文字列に変換
 * @param result 変換するICAccessResult
 * @return 結果の文字列表現
 */
std::string ICAccessResultToString(ICAccessResult result);

} // namespace core
} // namespace aerojs 