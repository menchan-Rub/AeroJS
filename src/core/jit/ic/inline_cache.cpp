#include "inline_cache.h"
#include "../../object/js_object.h"
#include "../../object/shape.h"
#include "../../value/js_value.h"
#include <cassert>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <unordered_set>
#include <sstream>

namespace aerojs {
namespace core {

//==============================================================================
// ICStatistics - キャッシュ統計情報
//==============================================================================

class ICStatistics {
public:
    static ICStatistics& Instance() {
        static ICStatistics instance;
        return instance;
    }

    void RecordCacheHit(uint32_t cacheId, bool isMethod) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        auto& stats = isMethod ? m_methodCacheStats[cacheId] : m_propertyCacheStats[cacheId];
        stats.hits++;
        stats.lastHitTime = std::chrono::system_clock::now();
        m_totalHits++;
    }

    void RecordCacheMiss(uint32_t cacheId, bool isMethod) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        auto& stats = isMethod ? m_methodCacheStats[cacheId] : m_propertyCacheStats[cacheId];
        stats.misses++;
        stats.lastMissTime = std::chrono::system_clock::now();
        m_totalMisses++;
    }

    double GetHitRate(uint32_t cacheId, bool isMethod) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        const auto& map = isMethod ? m_methodCacheStats : m_propertyCacheStats;
        auto it = map.find(cacheId);
        if (it == map.end()) {
            return 0.0;
        }
        const auto& stats = it->second;
        uint64_t total = stats.hits + stats.misses;
        return total > 0 ? static_cast<double>(stats.hits) / total : 0.0;
    }

    double GetGlobalHitRate() const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        uint64_t total = m_totalHits + m_totalMisses;
        return total > 0 ? static_cast<double>(m_totalHits) / total : 0.0;
    }

    void GetTopMissedProperties(std::vector<std::pair<uint32_t, uint64_t>>& result, size_t count) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        result.clear();
        for (const auto& pair : m_propertyCacheStats) {
            result.push_back(std::make_pair(pair.first, pair.second.misses));
        }
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
        if (result.size() > count) {
            result.resize(count);
        }
    }

    void Reset() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_propertyCacheStats.clear();
        m_methodCacheStats.clear();
        m_totalHits = 0;
        m_totalMisses = 0;
    }

    std::string GenerateStatisticsReport() const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        std::ostringstream oss;
        oss << "===== Inline Cache Statistics =====\n";
        oss << "Global hit rate: " << (GetGlobalHitRate() * 100.0) << "%\n";
        oss << "Total hits: " << m_totalHits << "\n";
        oss << "Total misses: " << m_totalMisses << "\n";
        oss << "Property caches: " << m_propertyCacheStats.size() << "\n";
        oss << "Method caches: " << m_methodCacheStats.size() << "\n";
        
        oss << "\nTop 5 property caches by hit rate:\n";
        std::vector<std::pair<uint32_t, double>> topHits;
        for (const auto& pair : m_propertyCacheStats) {
            double hitRate = pair.second.hits + pair.second.misses > 0
                ? static_cast<double>(pair.second.hits) / (pair.second.hits + pair.second.misses)
                : 0.0;
            topHits.push_back(std::make_pair(pair.first, hitRate));
        }
        std::sort(topHits.begin(), topHits.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
        
        for (size_t i = 0; i < std::min(topHits.size(), size_t(5)); ++i) {
            oss << "  Cache ID " << topHits[i].first << ": " 
                << (topHits[i].second * 100.0) << "% hit rate\n";
        }
        
        return oss.str();
    }

private:
    ICStatistics() : m_totalHits(0), m_totalMisses(0) {}
    
    struct CacheStats {
        uint64_t hits = 0;
        uint64_t misses = 0;
        std::chrono::system_clock::time_point lastHitTime;
        std::chrono::system_clock::time_point lastMissTime;
    };
    
    std::unordered_map<uint32_t, CacheStats> m_propertyCacheStats;
    std::unordered_map<uint32_t, CacheStats> m_methodCacheStats;
    uint64_t m_totalHits;
    uint64_t m_totalMisses;
    mutable std::shared_mutex m_mutex;
};

//==============================================================================
// ICLogger - キャッシュ操作のロギング
//==============================================================================

enum class ICLogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class ICLogger {
public:
    static ICLogger& Instance() {
        static ICLogger instance;
        return instance;
    }

    void SetLogLevel(ICLogLevel level) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_logLevel = level;
    }

    void Debug(const std::string& message) {
        Log(ICLogLevel::Debug, message);
    }
    
    void Info(const std::string& message) {
        Log(ICLogLevel::Info, message);
    }
    
    void Warning(const std::string& message) {
        Log(ICLogLevel::Warning, message);
    }
    
    void Error(const std::string& message) {
        Log(ICLogLevel::Error, message);
    }

private:
    ICLogger() : m_logLevel(ICLogLevel::Info), m_enabled(true) {}
    
    void Log(ICLogLevel level, const std::string& message) {
        if (!m_enabled || level < m_logLevel) {
            return;
        }
        
        std::unique_lock<std::mutex> lock(m_mutex);
        
        std::string levelStr;
        switch (level) {
            case ICLogLevel::Debug:   levelStr = "DEBUG"; break;
            case ICLogLevel::Info:    levelStr = "INFO"; break;
            case ICLogLevel::Warning: levelStr = "WARNING"; break;
            case ICLogLevel::Error:   levelStr = "ERROR"; break;
        }
        
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        
        std::tm tm_buf;
        localtime_r(&now_time_t, &tm_buf);
        
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_buf);
        
        std::cout << "[" << buffer << "] [IC:" << levelStr << "] " << message << std::endl;
    }
    
    ICLogLevel m_logLevel;
    bool m_enabled;
    std::mutex m_mutex;
};

//==============================================================================
// PropertyICEntry
//==============================================================================

PropertyICEntry::PropertyICEntry(uint32_t shapeId, uint32_t offset, bool isInline) noexcept
    : m_shapeId(shapeId), m_offset(offset), m_isInline(isInline), m_isValid(true) {
    ICLogger::Instance().Debug("Created PropertyICEntry for shape " + std::to_string(shapeId) + 
                              " at offset " + std::to_string(offset) + 
                              (isInline ? " (inline)" : " (out-of-line)"));
}

PropertyICEntry::~PropertyICEntry() {
    ICLogger::Instance().Debug("Destroyed PropertyICEntry for shape " + std::to_string(m_shapeId));
}

void PropertyICEntry::UpdateOffset(uint32_t newOffset) noexcept {
    ICLogger::Instance().Debug("Updating PropertyICEntry offset from " + 
                              std::to_string(m_offset) + " to " + 
                              std::to_string(newOffset) + " for shape " + 
                              std::to_string(m_shapeId));
    m_offset = newOffset;
}

uint32_t PropertyICEntry::GetAccessCount() const noexcept {
    return m_accessCount;
}

void PropertyICEntry::IncrementAccessCount() noexcept {
    m_accessCount++;
}

//==============================================================================
// MethodICEntry
//==============================================================================

MethodICEntry::MethodICEntry(uint32_t shapeId, uint32_t methodId, void* nativeCode) noexcept
    : m_shapeId(shapeId), m_methodId(methodId), m_nativeCode(nativeCode), m_isValid(true), m_accessCount(0) {
    ICLogger::Instance().Debug("Created MethodICEntry for shape " + std::to_string(shapeId) + 
                              " with method ID " + std::to_string(methodId));
}

MethodICEntry::~MethodICEntry() {
    ICLogger::Instance().Debug("Destroyed MethodICEntry for shape " + std::to_string(m_shapeId));
}

void MethodICEntry::UpdateNativeCode(void* newNativeCode) noexcept {
    ICLogger::Instance().Debug("Updating MethodICEntry native code for shape " + 
                              std::to_string(m_shapeId));
    m_nativeCode = newNativeCode;
}

uint32_t MethodICEntry::GetAccessCount() const noexcept {
    return m_accessCount;
}

void MethodICEntry::IncrementAccessCount() noexcept {
    m_accessCount++;
}

//==============================================================================
// ProtoICEntry
//==============================================================================

ProtoICEntry::ProtoICEntry(uint32_t shapeId, uint32_t protoShapeId, uint32_t offset) noexcept
    : m_shapeId(shapeId), m_protoShapeId(protoShapeId), m_offset(offset), m_isValid(true), m_accessCount(0) {
    ICLogger::Instance().Debug("Created ProtoICEntry for shape " + std::to_string(shapeId) + 
                              " with prototype shape " + std::to_string(protoShapeId) + 
                              " at offset " + std::to_string(offset));
}

ProtoICEntry::~ProtoICEntry() {
    ICLogger::Instance().Debug("Destroyed ProtoICEntry for shape " + std::to_string(m_shapeId));
}

void ProtoICEntry::UpdateOffset(uint32_t newOffset) noexcept {
    ICLogger::Instance().Debug("Updating ProtoICEntry offset from " + 
                              std::to_string(m_offset) + " to " + 
                              std::to_string(newOffset) + " for shape " + 
                              std::to_string(m_shapeId));
    m_offset = newOffset;
}

uint32_t ProtoICEntry::GetAccessCount() const noexcept {
    return m_accessCount;
}

void ProtoICEntry::IncrementAccessCount() noexcept {
    m_accessCount++;
}

//==============================================================================
// MegamorphicICEntry - 多態形状のキャッシュエントリ
//==============================================================================

MegamorphicICEntry::MegamorphicICEntry() noexcept
    : m_isValid(true), m_accessCount(0) {
    ICLogger::Instance().Debug("Created MegamorphicICEntry");
}

MegamorphicICEntry::~MegamorphicICEntry() {
    ICLogger::Instance().Debug("Destroyed MegamorphicICEntry with " + 
                             std::to_string(m_shapesEncountered.size()) + " shapes");
}

void MegamorphicICEntry::AddShapeId(uint32_t shapeId) noexcept {
    m_shapesEncountered.insert(shapeId);
}

bool MegamorphicICEntry::HasSeenShape(uint32_t shapeId) const noexcept {
    return m_shapesEncountered.find(shapeId) != m_shapesEncountered.end();
}

size_t MegamorphicICEntry::GetUniqueShapeCount() const noexcept {
    return m_shapesEncountered.size();
}

uint32_t MegamorphicICEntry::GetAccessCount() const noexcept {
    return m_accessCount;
}

void MegamorphicICEntry::IncrementAccessCount() noexcept {
    m_accessCount++;
}

uint32_t MegamorphicICEntry::GetShapeId() const noexcept {
    // 多態キャッシュには単一のシェイプIDがないのでダミー値を返す
    return 0xFFFFFFFF;
}

bool MegamorphicICEntry::IsValid() const noexcept {
    return m_isValid;
}

void MegamorphicICEntry::Invalidate() noexcept {
    ICLogger::Instance().Debug("Invalidating MegamorphicICEntry with " + 
                             std::to_string(m_shapesEncountered.size()) + " shapes");
    m_isValid = false;
}

//==============================================================================
// TransitionICEntry - 形状遷移を記録するキャッシュエントリ
//==============================================================================

TransitionICEntry::TransitionICEntry(uint32_t fromShapeId, uint32_t toShapeId,
                                    const std::string& propertyName) noexcept
    : m_fromShapeId(fromShapeId), m_toShapeId(toShapeId), m_propertyName(propertyName),
      m_isValid(true), m_accessCount(0) {
    ICLogger::Instance().Debug("Created TransitionICEntry from shape " + 
                              std::to_string(fromShapeId) + " to " + 
                              std::to_string(toShapeId) + " for property '" + 
                              propertyName + "'");
}

TransitionICEntry::~TransitionICEntry() {
    ICLogger::Instance().Debug("Destroyed TransitionICEntry");
}

uint32_t TransitionICEntry::GetShapeId() const noexcept {
    // このエントリは開始形状IDを返す
    return m_fromShapeId;
}

uint32_t TransitionICEntry::GetTargetShapeId() const noexcept {
    return m_toShapeId;
}

const std::string& TransitionICEntry::GetPropertyName() const noexcept {
    return m_propertyName;
}

uint32_t TransitionICEntry::GetAccessCount() const noexcept {
    return m_accessCount;
}

void TransitionICEntry::IncrementAccessCount() noexcept {
    m_accessCount++;
}

bool TransitionICEntry::IsValid() const noexcept {
    return m_isValid;
}

void TransitionICEntry::Invalidate() noexcept {
    ICLogger::Instance().Debug("Invalidating TransitionICEntry from shape " + 
                             std::to_string(m_fromShapeId) + " to " + 
                             std::to_string(m_toShapeId));
    m_isValid = false;
}

//==============================================================================
// MonomorphicIC
//==============================================================================

MonomorphicIC::MonomorphicIC() noexcept : m_type(ICType::None), m_entryCreationTime(std::chrono::system_clock::now()) {
    ICLogger::Instance().Debug("Created MonomorphicIC");
}

MonomorphicIC::~MonomorphicIC() {
    ICLogger::Instance().Debug("Destroyed MonomorphicIC with status " + GetStatusString());
}

void MonomorphicIC::Set(std::unique_ptr<PropertyICEntry> entry) noexcept {
    ICLogger::Instance().Debug("Setting PropertyICEntry in MonomorphicIC");
    m_type = ICType::Property;
    m_entry = std::move(entry);
    m_entryCreationTime = std::chrono::system_clock::now();
}

void MonomorphicIC::Set(std::unique_ptr<MethodICEntry> entry) noexcept {
    ICLogger::Instance().Debug("Setting MethodICEntry in MonomorphicIC");
    m_type = ICType::Method;
    m_entry = std::move(entry);
    m_entryCreationTime = std::chrono::system_clock::now();
}

void MonomorphicIC::Set(std::unique_ptr<ProtoICEntry> entry) noexcept {
    ICLogger::Instance().Debug("Setting ProtoICEntry in MonomorphicIC");
    m_type = ICType::Proto;
    m_entry = std::move(entry);
    m_entryCreationTime = std::chrono::system_clock::now();
}

void MonomorphicIC::Set(std::unique_ptr<MegamorphicICEntry> entry) noexcept {
    ICLogger::Instance().Debug("Setting MegamorphicICEntry in MonomorphicIC");
    m_type = ICType::Megamorphic;
    m_entry = std::move(entry);
    m_entryCreationTime = std::chrono::system_clock::now();
}

void MonomorphicIC::Set(std::unique_ptr<TransitionICEntry> entry) noexcept {
    ICLogger::Instance().Debug("Setting TransitionICEntry in MonomorphicIC");
    m_type = ICType::Transition;
    m_entry = std::move(entry);
    m_entryCreationTime = std::chrono::system_clock::now();
}

ICStatus MonomorphicIC::GetStatus() const noexcept {
    if (!m_entry) {
        return ICStatus::Uninitialized;
    }
    
    if (!m_entry->IsValid()) {
        return ICStatus::Invalid;
    }
    
    return m_type == ICType::Megamorphic ? ICStatus::Megamorphic : ICStatus::Monomorphic;
}

std::string MonomorphicIC::GetStatusString() const noexcept {
    switch (GetStatus()) {
        case ICStatus::Uninitialized: return "Uninitialized";
        case ICStatus::Monomorphic: return "Monomorphic";
        case ICStatus::Polymorphic: return "Polymorphic"; // 実際には返されない
        case ICStatus::Megamorphic: return "Megamorphic";
        case ICStatus::Invalid: return "Invalid";
        default: return "Unknown";
    }
}

bool MonomorphicIC::IsPropertyCache() const noexcept {
    return m_type == ICType::Property;
}

bool MonomorphicIC::IsMethodCache() const noexcept {
    return m_type == ICType::Method;
}

bool MonomorphicIC::IsProtoCache() const noexcept {
    return m_type == ICType::Proto;
}

bool MonomorphicIC::IsMegamorphicCache() const noexcept {
    return m_type == ICType::Megamorphic;
}

bool MonomorphicIC::IsTransitionCache() const noexcept {
    return m_type == ICType::Transition;
}

const ICEntry* MonomorphicIC::GetEntry() const noexcept {
    return m_entry.get();
}

void MonomorphicIC::Invalidate() noexcept {
    ICLogger::Instance().Debug("Invalidating MonomorphicIC");
    if (m_entry) {
        m_entry->Invalidate();
    }
}

void MonomorphicIC::Reset() noexcept {
    ICLogger::Instance().Debug("Resetting MonomorphicIC");
    m_entry.reset();
    m_type = ICType::None;
}

std::chrono::system_clock::time_point MonomorphicIC::GetEntryCreationTime() const noexcept {
    return m_entryCreationTime;
}

//==============================================================================
// PolymorphicIC
//==============================================================================

PolymorphicIC::PolymorphicIC() noexcept {
    ICLogger::Instance().Debug("Created PolymorphicIC");
}

PolymorphicIC::~PolymorphicIC() {
    ICLogger::Instance().Debug("Destroyed PolymorphicIC with " + 
                             std::to_string(m_entries.size()) + " entries");
}

bool PolymorphicIC::Add(std::unique_ptr<PropertyICEntry> entry) noexcept {
    if (!HasSpace()) {
        ICLogger::Instance().Warning("PolymorphicIC full, cannot add PropertyICEntry");
        return false;
    }
    
    // 同じシェイプIDのエントリがないかチェック
    uint32_t shapeId = entry->GetShapeId();
    for (const auto& existingEntry : m_entries) {
        if (existingEntry->GetShapeId() == shapeId) {
            ICLogger::Instance().Warning("PolymorphicIC already has entry for shape " + 
                                      std::to_string(shapeId));
            return false;
        }
    }
    
    ICLogger::Instance().Debug("Adding PropertyICEntry for shape " + 
                             std::to_string(shapeId) + " to PolymorphicIC");
    m_entries.push_back(std::move(entry));
    return true;
}

bool PolymorphicIC::Add(std::unique_ptr<MethodICEntry> entry) noexcept {
    if (!HasSpace()) {
        ICLogger::Instance().Warning("PolymorphicIC full, cannot add MethodICEntry");
        return false;
    }
    
    // 同じシェイプIDのエントリがないかチェック
    uint32_t shapeId = entry->GetShapeId();
    for (const auto& existingEntry : m_entries) {
        if (existingEntry->GetShapeId() == shapeId) {
            ICLogger::Instance().Warning("PolymorphicIC already has entry for shape " + 
                                      std::to_string(shapeId));
            return false;
        }
    }
    
    ICLogger::Instance().Debug("Adding MethodICEntry for shape " + 
                             std::to_string(shapeId) + " to PolymorphicIC");
    m_entries.push_back(std::move(entry));
    return true;
}

bool PolymorphicIC::Add(std::unique_ptr<ProtoICEntry> entry) noexcept {
    if (!HasSpace()) {
        ICLogger::Instance().Warning("PolymorphicIC full, cannot add ProtoICEntry");
        return false;
    }
    
    // 同じシェイプIDのエントリがないかチェック
    uint32_t shapeId = entry->GetShapeId();
    for (const auto& existingEntry : m_entries) {
        if (existingEntry->GetShapeId() == shapeId) {
            ICLogger::Instance().Warning("PolymorphicIC already has entry for shape " + 
                                      std::to_string(shapeId));
            return false;
        }
    }
    
    ICLogger::Instance().Debug("Adding ProtoICEntry for shape " + 
                             std::to_string(shapeId) + " to PolymorphicIC");
    m_entries.push_back(std::move(entry));
    return true;
}

bool PolymorphicIC::Add(std::unique_ptr<TransitionICEntry> entry) noexcept {
    if (!HasSpace()) {
        ICLogger::Instance().Warning("PolymorphicIC full, cannot add TransitionICEntry");
        return false;
    }
    
    // 同じシェイプIDのエントリがないかチェック
    uint32_t shapeId = entry->GetShapeId();
    for (const auto& existingEntry : m_entries) {
        if (existingEntry->GetShapeId() == shapeId) {
            ICLogger::Instance().Warning("PolymorphicIC already has entry for shape " + 
                                      std::to_string(shapeId));
            return false;
        }
    }
    
    ICLogger::Instance().Debug("Adding TransitionICEntry for shape " + 
                             std::to_string(shapeId) + " to PolymorphicIC");
    m_entries.push_back(std::move(entry));
    return true;
}

ICEntry* PolymorphicIC::Find(uint32_t shapeId) const noexcept {
    for (const auto& entry : m_entries) {
        if (entry->GetShapeId() == shapeId && entry->IsValid()) {
            return entry.get();
        }
    }
    return nullptr;
}

ICStatus PolymorphicIC::GetStatus() const noexcept {
    if (m_entries.empty()) {
        return ICStatus::Uninitialized;
    }
    
    // 全エントリが無効ならInvalidを返す
    bool allInvalid = true;
    for (const auto& entry : m_entries) {
        if (entry->IsValid()) {
            allInvalid = false;
            break;
        }
    }
    
    if (allInvalid) {
        return ICStatus::Invalid;
    }
    
    if (m_entries.size() >= kPolymorphicThreshold) {
        return ICStatus::Megamorphic;
    }
    
    return ICStatus::Polymorphic;
}

void PolymorphicIC::Invalidate() noexcept {
    ICLogger::Instance().Debug("Invalidating PolymorphicIC with " + 
                             std::to_string(m_entries.size()) + " entries");
    for (auto& entry : m_entries) {
        entry->Invalidate();
    }
}

void PolymorphicIC::Reset() noexcept {
    ICLogger::Instance().Debug("Resetting PolymorphicIC");
    m_entries.clear();
}

void PolymorphicIC::RemoveInvalidEntries() noexcept {
    size_t initialSize = m_entries.size();
    
    m_entries.erase(
        std::remove_if(m_entries.begin(), m_entries.end(),
                     [](const std::unique_ptr<ICEntry>& entry) {
                         return !entry->IsValid();
                     }),
        m_entries.end());
    
    size_t removedCount = initialSize - m_entries.size();
    if (removedCount > 0) {
        ICLogger::Instance().Debug("Removed " + std::to_string(removedCount) + 
                                 " invalid entries from PolymorphicIC");
    }
}

//==============================================================================
// InlineCacheManager
//==============================================================================

InlineCacheManager::InlineCacheManager() noexcept
    : m_nextCacheId(1) // 0は無効なIDとして予約
{
    ICLogger::Instance().Debug("Created InlineCacheManager");
}

InlineCacheManager::~InlineCacheManager() {
    ICLogger::Instance().Debug("Destroyed InlineCacheManager with " + 
                             std::to_string(m_propertyCaches.size()) + " property caches and " +
                             std::to_string(m_methodCaches.size()) + " method caches");
}

InlineCacheManager& InlineCacheManager::Instance() noexcept {
    static InlineCacheManager instance;
    return instance;
}

uint32_t InlineCacheManager::CreatePropertyCache(const std::string& propertyName) noexcept {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    uint32_t cacheId = GenerateCacheId();
    
    auto cache = std::make_unique<MonomorphicIC>();
    m_propertyCaches[cacheId] = std::move(cache);
    m_propertyCacheNames[cacheId] = propertyName;
    
    ICLogger::Instance().Debug("Created property cache with ID " + std::to_string(cacheId) + 
                             " for property '" + propertyName + "'");
    
    return cacheId;
}

uint32_t InlineCacheManager::CreateMethodCache(const std::string& methodName) noexcept {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    uint32_t cacheId = GenerateCacheId();
    
    auto cache = std::make_unique<MonomorphicIC>();
    m_methodCaches[cacheId] = std::move(cache);
    m_methodCacheNames[cacheId] = methodName;
    
    ICLogger::Instance().Debug("Created method cache with ID " + std::to_string(cacheId) + 
                             " for method '" + methodName + "'");
    
    return cacheId;
}

MonomorphicIC* InlineCacheManager::GetPropertyCache(uint32_t cacheId) noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_propertyCaches.find(cacheId);
    if (it == m_propertyCaches.end()) {
        ICLogger::Instance().Warning("Property cache with ID " + std::to_string(cacheId) + 
                                   " not found");
        return nullptr;
    }
    return it->second.get();
}

MonomorphicIC* InlineCacheManager::GetMethodCache(uint32_t cacheId) noexcept {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_methodCaches.find(cacheId);
    if (it == m_methodCaches.end()) {
        ICLogger::Instance().Warning("Method cache with ID " + std::to_string(cacheId) + 
                                   " not found");
        return nullptr;
    }
    return it->second.get();
}

PolymorphicIC* InlineCacheManager::UpgradePropertyCache(uint32_t cacheId) noexcept {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    auto it = m_propertyCaches.find(cacheId);
    if (it == m_propertyCaches.end()) {
        ICLogger::Instance().Warning("Cannot upgrade non-existent property cache with ID " + 
                                   std::to_string(cacheId));
        return nullptr;
    }
    
    MonomorphicIC* monoCache = it->second.get();
    if (monoCache->GetStatus() != ICStatus::Monomorphic) {
        ICLogger::Instance().Warning("Cannot upgrade property cache with ID " + 
                                   std::to_string(cacheId) + " because it is not monomorphic");
        return nullptr;
    }
    
    // モノモーフィックキャッシュからポリモーフィックキャッシュへ移行
    auto polyCache = std::make_unique<PolymorphicIC>();
    
    // モノモーフィックエントリをポリモーフィックキャッシュにコピー
    const ICEntry* entry = monoCache->GetEntry();
    if (entry) {
        // 型に応じて適切なエントリタイプを作成
        if (monoCache->IsPropertyCache()) {
            const PropertyICEntry* propEntry = static_cast<const PropertyICEntry*>(entry);
            auto newEntry = std::make_unique<PropertyICEntry>(
                propEntry->GetShapeId(),
                propEntry->GetOffset(),
                propEntry->IsInlineProperty()
            );
            polyCache->Add(std::move(newEntry));
        }
        else if (monoCache->IsMethodCache()) {
            const MethodICEntry* methodEntry = static_cast<const MethodICEntry*>(entry);
            auto newEntry = std::make_unique<MethodICEntry>(
                methodEntry->GetShapeId(),
                methodEntry->GetMethodId(),
                nullptr  // nativeCodeはコピーしない
            );
            polyCache->Add(std::move(newEntry));
        }
        else if (monoCache->IsProtoCache()) {
            const ProtoICEntry* protoEntry = static_cast<const ProtoICEntry*>(entry);
            auto newEntry = std::make_unique<ProtoICEntry>(
                protoEntry->GetShapeId(),
                protoEntry->GetProtoShapeId(),
                protoEntry->GetOffset()
            );
            polyCache->Add(std::move(newEntry));
        }
    }
    
    // ポリモーフィックキャッシュをマップに追加
    PolymorphicIC* result = polyCache.get();
    m_polymorphicPropertyCaches[cacheId] = std::move(polyCache);
    
    // モノモーフィックキャッシュをリセット（無効化）
    monoCache->Reset();
    
    ICLogger::Instance().Debug("Upgraded property cache with ID " + std::to_string(cacheId) + 
                             " from monomorphic to polymorphic");
    
    return result;
}

void InlineCacheManager::InvalidateCachesForShape(uint32_t shapeId) noexcept {
    auto it = m_shapeToCaches.find(shapeId);
    if (it == m_shapeToCaches.end()) {
        return;
    }
    
    // 該当する形状に関連するすべてのキャッシュを無効化
    for (uint32_t cacheId : it->second) {
        // プロパティキャッシュの無効化
        auto propIt = m_propertyCaches.find(cacheId);
        if (propIt != m_propertyCaches.end()) {
            propIt->second->Invalidate();
        }
        
        // メソッドキャッシュの無効化
        auto methodIt = m_methodCaches.find(cacheId);
        if (methodIt != m_methodCaches.end()) {
            methodIt->second->Invalidate();
        }
        
        // ポリモーフィックプロパティキャッシュの無効化
        auto polyPropIt = m_polymorphicPropertyCaches.find(cacheId);
        if (polyPropIt != m_polymorphicPropertyCaches.end()) {
            polyPropIt->second->Invalidate();
        }
        
        // ポリモーフィックメソッドキャッシュの無効化
        auto polyMethodIt = m_polymorphicMethodCaches.find(cacheId);
        if (polyMethodIt != m_polymorphicMethodCaches.end()) {
            polyMethodIt->second->Invalidate();
        }
    }
}

void InlineCacheManager::Reset() noexcept {
    m_propertyCaches.clear();
    m_methodCaches.clear();
    m_polymorphicPropertyCaches.clear();
    m_polymorphicMethodCaches.clear();
    m_shapeToCaches.clear();
    m_nextCacheId = 1;
}

//==============================================================================
// プロパティアクセス補助関数
//==============================================================================

JSValue GetPropertyCached(JSObject* obj, uint32_t cacheId, const std::string& propertyName) {
    assert(obj != nullptr);
    
    // キャッシュマネージャからキャッシュを取得
    InlineCacheManager& manager = InlineCacheManager::Instance();
    
    // オブジェクトの形状IDを取得
    uint32_t shapeId = obj->GetShape()->GetId();
    
    // モノモーフィックキャッシュのチェック
    MonomorphicIC* monoCache = manager.GetPropertyCache(cacheId);
    if (monoCache) {
        ICEntry* entry = monoCache->Get();
        if (entry && entry->IsValid() && entry->GetShapeId() == shapeId) {
            PropertyICEntry* propEntry = dynamic_cast<PropertyICEntry*>(entry);
            if (propEntry) {
                // インラインプロパティからの取得
                if (propEntry->IsInlineProperty()) {
                    return obj->GetInlinePropertyAt(propEntry->GetOffset());
                } else {
                    // 非インラインプロパティからの取得
                    return obj->GetOutOfLinePropertyAt(propEntry->GetOffset());
                }
            }
            
            ProtoICEntry* protoEntry = dynamic_cast<ProtoICEntry*>(entry);
            if (protoEntry) {
                // プロトタイプチェーンからの取得
                JSObject* proto = obj->GetPrototype();
                if (proto && proto->GetShape()->GetId() == protoEntry->GetProtoShapeId()) {
                    if (protoEntry->GetOffset() != UINT32_MAX) {
                        return proto->GetInlinePropertyAt(protoEntry->GetOffset());
                    }
                }
            }
        }
        
        // キャッシュミスの場合、通常の方法でプロパティを検索
        JSValue value = obj->GetProperty(propertyName);
        
        // キャッシュの更新
        if (value.IsDefined()) {
            // プロパティの場所に応じてキャッシュエントリを作成
            uint32_t offset = 0;
            bool isInline = false;
            JSObject* propOwner = nullptr;
            
            if (obj->HasOwnProperty(propertyName, &offset, &isInline)) {
                // 直接所有しているプロパティ
                auto entry = std::make_unique<PropertyICEntry>(shapeId, offset, isInline);
                monoCache->Set(std::move(entry));
            } else if (obj->HasPropertyInPrototypeChain(propertyName, &propOwner, &offset)) {
                // プロトタイプチェーン上のプロパティ
                if (propOwner) {
                    auto entry = std::make_unique<ProtoICEntry>(
                        shapeId, 
                        propOwner->GetShape()->GetId(), 
                        offset
                    );
                    monoCache->Set(std::move(entry));
                }
            }
        }
        
        return value;
    }
    
    // キャッシュが見つからない場合は通常の方法でプロパティを取得
    return obj->GetProperty(propertyName);
}

void SetPropertyCached(JSObject* obj, uint32_t cacheId, const std::string& propertyName, const JSValue& value) {
    assert(obj != nullptr);
    
    // キャッシュマネージャからキャッシュを取得
    InlineCacheManager& manager = InlineCacheManager::Instance();
    
    // オブジェクトの形状IDを取得
    uint32_t shapeId = obj->GetShape()->GetId();
    
    // モノモーフィックキャッシュのチェック
    MonomorphicIC* monoCache = manager.GetPropertyCache(cacheId);
    if (monoCache) {
        ICEntry* entry = monoCache->Get();
        if (entry && entry->IsValid() && entry->GetShapeId() == shapeId) {
            PropertyICEntry* propEntry = dynamic_cast<PropertyICEntry*>(entry);
            if (propEntry) {
                // キャッシュヒット - インラインプロパティの更新
                if (propEntry->IsInlineProperty()) {
                    obj->SetInlinePropertyAt(propEntry->GetOffset(), value);
                    return;
                } else {
                    // 非インラインプロパティの更新
                    obj->SetOutOfLinePropertyAt(propEntry->GetOffset(), value);
                    return;
                }
            }
        }
        
        // キャッシュミスまたは無効なエントリの場合
        uint32_t offset = 0;
        bool isInline = false;
        
        // プロパティを設定
        obj->SetProperty(propertyName, value);
        
        // 新しい形状IDを取得（プロパティ追加で変わった可能性あり）
        uint32_t newShapeId = obj->GetShape()->GetId();
        
        // 既存のプロパティだったかチェック
        if (obj->HasOwnProperty(propertyName, &offset, &isInline)) {
            auto entry = std::make_unique<PropertyICEntry>(newShapeId, offset, isInline);
            monoCache->Set(std::move(entry));
            
            // 形状IDとキャッシュIDのマッピングを更新
            manager.InvalidateCachesForShape(shapeId);
        }
        
        return;
    }
    
    // キャッシュが見つからない場合は通常の方法でプロパティを設定
    obj->SetProperty(propertyName, value);
}

//==============================================================================
// メソッド呼び出し補助関数
//==============================================================================

JSValue CallMethodCached(JSObject* obj, uint32_t cacheId, const std::string& methodName, const std::vector<JSValue>& args) {
    assert(obj != nullptr);
    
    // キャッシュマネージャからキャッシュを取得
    InlineCacheManager& manager = InlineCacheManager::Instance();
    
    // オブジェクトの形状IDを取得
    uint32_t shapeId = obj->GetShape()->GetId();
    
    // モノモーフィックキャッシュのチェック
    MonomorphicIC* monoCache = manager.GetMethodCache(cacheId);
    if (monoCache) {
        ICEntry* entry = monoCache->Get();
        if (entry && entry->IsValid() && entry->GetShapeId() == shapeId) {
            MethodICEntry* methodEntry = dynamic_cast<MethodICEntry*>(entry);
            if (methodEntry) {
                // キャッシュヒット - 最適化されたメソッド呼び出し
                void* nativeCode = methodEntry->GetNativeCode();
                if (nativeCode) {
                    // ネイティブコードが利用可能な場合、直接呼び出し
                    // 実際の呼び出しはエンジン固有の実装によって異なる
                    // ここでは簡略化のため、通常のメソッド呼び出しを行う
                    return obj->CallMethod(methodName, args);
                }
            }
        }
        
        // キャッシュミスの場合、通常の方法でメソッドを呼び出し
        JSValue methodValue = obj->GetProperty(methodName);
        
        // メソッドの呼び出し
        JSValue result = obj->CallMethod(methodName, args);
        
        // キャッシュの更新（ここでは簡略化）
        if (methodValue.IsFunction()) {
            // 実際のエンジンでは、ここでJITコンパイルしたネイティブコードへのポインタを保存する
            auto entry = std::make_unique<MethodICEntry>(shapeId, 0, nullptr);
            monoCache->Set(std::move(entry));
        }
        
        return result;
    }
    
    // キャッシュが見つからない場合は通常の方法でメソッドを呼び出し
    return obj->CallMethod(methodName, args);
}

}  // namespace core
}  // namespace aerojs 