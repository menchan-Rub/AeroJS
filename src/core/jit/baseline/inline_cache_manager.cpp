#include "inline_cache_manager.h"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <iomanip>

namespace aerojs {
namespace core {

//=============================================================================
// InlineCache の実装
//=============================================================================

InlineCache::InlineCache(const std::string& cacheId, ICType type, size_t maxEntries)
    : m_id(cacheId)
    , m_type(type)
    , m_maxEntries(maxEntries)
{
    m_stats.lookups = 0;
    m_stats.hits = 0;
    m_stats.misses = 0;
    m_stats.invalidations = 0;
    m_stats.typeErrors = 0;
    
    // 初期容量を確保
    m_entries.reserve(maxEntries);
}

InlineCache::~InlineCache() = default;

const std::string& InlineCache::GetId() const {
    return m_id;
}

ICType InlineCache::GetType() const {
    return m_type;
}

ICAccessResult InlineCache::Lookup(uint64_t key, uint64_t& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 統計を更新
    m_stats.lookups++;
    
    // キーでエントリを検索
    for (auto& entry : m_entries) {
        if (entry.key == key) {
            // ヒット
            m_stats.hits++;
            entry.hitCount++;
            value = entry.value;
            return ICAccessResult::kHit;
        }
    }
    
    // ミス
    m_stats.misses++;
    return ICAccessResult::kMiss;
}

void InlineCache::Add(uint64_t key, uint64_t value, uint32_t flags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // キーがすでに存在するか確認
    for (auto& entry : m_entries) {
        if (entry.key == key) {
            // 既存のエントリを更新
            entry.value = value;
            entry.flags = flags;
            return;
        }
    }
    
    // キャッシュがいっぱいの場合、最もヒット回数の少ないエントリを削除
    if (m_entries.size() >= m_maxEntries && !m_entries.empty()) {
        auto minHitEntry = std::min_element(m_entries.begin(), m_entries.end(),
            [](const ICEntry& a, const ICEntry& b) {
                return a.hitCount < b.hitCount;
            });
        if (minHitEntry != m_entries.end()) {
            *minHitEntry = ICEntry(key, value, flags);
        }
    } else {
        // 新しいエントリを追加
        m_entries.emplace_back(key, value, flags);
    }
}

bool InlineCache::Invalidate(uint64_t key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // キーでエントリを検索して削除
    auto it = std::find_if(m_entries.begin(), m_entries.end(),
        [key](const ICEntry& entry) {
            return entry.key == key;
        });
    
    if (it != m_entries.end()) {
        m_entries.erase(it);
        m_stats.invalidations++;
        return true;
    }
    
    return false;
}

void InlineCache::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

double InlineCache::GetHitRate() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    uint64_t totalLookups = m_stats.lookups;
    if (totalLookups == 0) {
        return 0.0;
    }
    
    return static_cast<double>(m_stats.hits) / totalLookups;
}

void InlineCache::SetMaxEntries(size_t maxEntries) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_maxEntries = maxEntries;
    
    // 容量を超えるエントリを削除
    if (m_entries.size() > m_maxEntries) {
        // ヒット回数の少ない順にソート
        std::sort(m_entries.begin(), m_entries.end(),
            [](const ICEntry& a, const ICEntry& b) {
                return a.hitCount < b.hitCount;
            });
        
        // 余分なエントリを削除
        m_entries.resize(m_maxEntries);
    }
}

size_t InlineCache::GetEntryCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.size();
}

std::vector<ICEntry> InlineCache::GetEntries() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries;
}

void InlineCache::ResetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_stats.lookups = 0;
    m_stats.hits = 0;
    m_stats.misses = 0;
    m_stats.invalidations = 0;
    m_stats.typeErrors = 0;
    
    // エントリのヒットカウントもリセット
    for (auto& entry : m_entries) {
        entry.hitCount = 0;
    }
}

//=============================================================================
// InlineCacheManager の実装
//=============================================================================

// シングルトンインスタンス取得
InlineCacheManager& InlineCacheManager::Instance() {
    static InlineCacheManager instance;
    return instance;
}

InlineCacheManager::InlineCacheManager()
    : m_maxCacheCount(1000)  // デフォルトは最大1000キャッシュ
    , m_enabled(true)
{
    m_globalStats.lookups = 0;
    m_globalStats.hits = 0;
    m_globalStats.misses = 0;
    m_globalStats.invalidations = 0;
    m_globalStats.typeErrors = 0;
}

InlineCacheManager::~InlineCacheManager() = default;

InlineCache* InlineCacheManager::GetOrCreateCache(
    const std::string& cacheId, ICType type, size_t maxEntries) {
    
    if (!m_enabled) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 既存のキャッシュを探す
    auto it = m_caches.find(cacheId);
    if (it != m_caches.end()) {
        return it->second.get();
    }
    
    // キャッシュ数が最大に達している場合は古いキャッシュを削除
    if (m_caches.size() >= m_maxCacheCount) {
        PruneCache();
    }
    
    // 新しいキャッシュを作成
    auto newCache = std::make_unique<InlineCache>(cacheId, type, maxEntries);
    InlineCache* result = newCache.get();
    m_caches[cacheId] = std::move(newCache);
    
    return result;
}

bool InlineCacheManager::RemoveCache(const std::string& cacheId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_caches.find(cacheId);
    if (it != m_caches.end()) {
        m_caches.erase(it);
        return true;
    }
    
    return false;
}

InlineCache* InlineCacheManager::GetCache(const std::string& cacheId) {
    if (!m_enabled) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_caches.find(cacheId);
    if (it != m_caches.end()) {
        return it->second.get();
    }
    
    return nullptr;
}

std::vector<InlineCache*> InlineCacheManager::GetCachesByType(ICType type) {
    std::vector<InlineCache*> result;
    
    if (!m_enabled) {
        return result;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    result.reserve(m_caches.size());
    for (auto& pair : m_caches) {
        if (pair.second->GetType() == type) {
            result.push_back(pair.second.get());
        }
    }
    
    return result;
}

std::unordered_map<std::string, InlineCache*> InlineCacheManager::GetAllCaches() {
    std::unordered_map<std::string, InlineCache*> result;
    
    if (!m_enabled) {
        return result;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& pair : m_caches) {
        result[pair.first] = pair.second.get();
    }
    
    return result;
}

void InlineCacheManager::ClearAllCaches() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& pair : m_caches) {
        pair.second->Clear();
    }
}

void InlineCacheManager::ClearCachesByType(ICType type) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& pair : m_caches) {
        if (pair.second->GetType() == type) {
            pair.second->Clear();
        }
    }
}

size_t InlineCacheManager::GetMaxCacheCount() const {
    return m_maxCacheCount;
}

void InlineCacheManager::SetMaxCacheCount(size_t maxCaches) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_maxCacheCount = maxCaches;
    
    // 制限を超えた場合は古いキャッシュを削除
    if (m_caches.size() > m_maxCacheCount) {
        PruneCache();
    }
}

bool InlineCacheManager::IsEnabled() const {
    return m_enabled;
}

void InlineCacheManager::SetEnabled(bool enabled) {
    m_enabled = enabled;
    
    if (!enabled) {
        // 無効化時にすべてのキャッシュをクリア
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& pair : m_caches) {
            pair.second->Clear();
        }
    }
}

void InlineCacheManager::ResetAllStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_globalStats.lookups = 0;
    m_globalStats.hits = 0;
    m_globalStats.misses = 0;
    m_globalStats.invalidations = 0;
    m_globalStats.typeErrors = 0;
    
    for (auto& pair : m_caches) {
        pair.second->ResetStats();
    }
}

double InlineCacheManager::GetGlobalHitRate() const {
    uint64_t totalLookups = m_globalStats.lookups;
    if (totalLookups == 0) {
        return 0.0;
    }
    
    return static_cast<double>(m_globalStats.hits) / totalLookups;
}

std::string InlineCacheManager::GenerateReport(bool detailed) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::ostringstream report;
    
    // 現在時刻
    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    
    report << "==========================================\n";
    report << "  インラインキャッシュパフォーマンスレポート\n";
    report << "  生成時間: " << std::put_time(std::localtime(&nowTime), "%Y-%m-%d %H:%M:%S") << "\n";
    report << "==========================================\n\n";
    
    report << "グローバル統計:\n";
    report << "  キャッシュ数: " << m_caches.size() << "/" << m_maxCacheCount << "\n";
    report << "  有効状態: " << (m_enabled ? "有効" : "無効") << "\n";
    report << "  総ルックアップ: " << m_globalStats.lookups << "\n";
    report << "  ヒット: " << m_globalStats.hits << " (" 
           << std::fixed << std::setprecision(2) << (GetGlobalHitRate() * 100) << "%)\n";
    report << "  ミス: " << m_globalStats.misses << " (" 
           << std::fixed << std::setprecision(2) 
           << (m_globalStats.lookups > 0 ? static_cast<double>(m_globalStats.misses) / m_globalStats.lookups * 100 : 0) << "%)\n";
    report << "  無効化: " << m_globalStats.invalidations << "\n";
    report << "  型エラー: " << m_globalStats.typeErrors << "\n\n";
    
    // タイプ別のキャッシュ数
    std::unordered_map<ICType, size_t> typeCounts;
    for (auto& pair : m_caches) {
        typeCounts[pair.second->GetType()]++;
    }
    
    report << "キャッシュタイプ分布:\n";
    for (int i = 0; i < static_cast<int>(ICType::kTypeCheck) + 1; ++i) {
        ICType type = static_cast<ICType>(i);
        report << "  " << ICTypeToString(type) << ": " << typeCounts[type] << "\n";
    }
    report << "\n";
    
    // 詳細レポートが要求された場合
    if (detailed && !m_caches.empty()) {
        report << "キャッシュ詳細:\n";
        
        // パフォーマンス順にソート
        std::vector<std::pair<std::string, InlineCache*>> sortedCaches;
        for (const auto& pair : m_caches) {
            sortedCaches.emplace_back(pair.first, pair.second.get());
        }
        
        std::sort(sortedCaches.begin(), sortedCaches.end(),
            [](const auto& a, const auto& b) {
                return a.second->GetHitRate() > b.second->GetHitRate();
            });
        
        for (const auto& pair : sortedCaches) {
            const auto& cache = pair.second;
            report << "  キャッシュID: " << pair.first << "\n";
            report << "    タイプ: " << ICTypeToString(cache->GetType()) << "\n";
            report << "    エントリ数: " << cache->GetEntryCount() << "\n";
            report << "    ヒット率: " << std::fixed << std::setprecision(2) 
                   << (cache->GetHitRate() * 100) << "%\n\n";
        }
    }
    
    return report.str();
}

void InlineCacheManager::PruneCache() {
    // 最も使用頻度の低いキャッシュを削除
    // ここでは単純に、削除するキャッシュ数の計算例を示します
    size_t targetSize = static_cast<size_t>(m_maxCacheCount * 0.8); // 20%削減
    
    if (m_caches.size() <= targetSize) {
        return;
    }
    
    // ヒット率でソート
    std::vector<std::pair<std::string, double>> cacheHitRates;
    for (const auto& pair : m_caches) {
        cacheHitRates.emplace_back(pair.first, pair.second->GetHitRate());
    }
    
    std::sort(cacheHitRates.begin(), cacheHitRates.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second; // ヒット率の低い順
        });
    
    // 削除する数
    size_t toRemove = m_caches.size() - targetSize;
    
    // ヒット率の低いキャッシュから削除
    for (size_t i = 0; i < toRemove && i < cacheHitRates.size(); ++i) {
        m_caches.erase(cacheHitRates[i].first);
    }
}

std::string ICTypeToString(ICType type) {
    switch (type) {
        case ICType::kProperty:    return "プロパティ";
        case ICType::kMethod:      return "メソッド";
        case ICType::kConstructor: return "コンストラクタ";
        case ICType::kPrototype:   return "プロトタイプ";
        case ICType::kComparison:  return "比較";
        case ICType::kBinaryOp:    return "二項演算";
        case ICType::kUnaryOp:     return "単項演算";
        case ICType::kTypeCheck:   return "型チェック";
        default:                   return "不明";
    }
}

std::string ICAccessResultToString(ICAccessResult result) {
    switch (result) {
        case ICAccessResult::kHit:        return "ヒット";
        case ICAccessResult::kMiss:       return "ミス";
        case ICAccessResult::kTypeError:  return "型エラー";
        case ICAccessResult::kInvalidated: return "無効化";
        case ICAccessResult::kOverflow:   return "オーバーフロー";
        default:                         return "不明";
    }
}

} // namespace core
} // namespace aerojs 