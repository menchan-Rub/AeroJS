/**
 * @file code_cache.h
 * @brief 世界最高性能のネイティブコードキャッシュシステム
 * @version 2.0.0
 * @license MIT
 */

#pragma once

#include <unordered_map>
#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include <string>
#include <functional>
#include <cstdint>
#include <optional>
#include <bitset>
#include <chrono>
#include <atomic>
#include <shared_mutex>
#include <list>

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class Function;

/**
 * @brief コードエントリの保持状態
 */
enum class CodeEntryState : uint8_t {
    Available,      // 利用可能
    Invalidated,    // 無効化済み
    StaleButUsable, // 古いが使用可能
    Evicted,        // 退避済み
    Relocating,     // 再配置中
    Deoptimizing,   // 最適化解除中
    Custom          // カスタム状態
};

/**
 * @brief コードフラグ
 */
enum class CodeFlags : uint32_t {
    None               = 0,
    NeedsFlush         = 1 << 0,   // フラッシュ必要
    IsHot              = 1 << 1,   // ホットコード
    SelfModifying      = 1 << 2,   // 自己修正コード
    UsesLargePages     = 1 << 3,   // 大きなページ使用
    IsOSRCode          = 1 << 4,   // OSRコード
    IsShared           = 1 << 5,   // 共有コード
    IsInline           = 1 << 6,   // インラインコード
    IsTrampoline       = 1 << 7,   // トランポリンコード
    HasAlignment       = 1 << 8,   // アライメントあり
    IsProtected        = 1 << 9,   // 保護済み
    TrackingPerformance = 1 << 10,  // パフォーマンストラッキング
    VirtualizeAccess   = 1 << 11,  // アクセス仮想化
    AllowsPatching     = 1 << 12,  // パッチング許可
    RequiresUnwindInfo = 1 << 13,  // アンワインド情報必要
    IsConstantData     = 1 << 14,  // 定数データ
    IsPinned           = 1 << 15,  // ピン留め状態
    IsPersistent       = 1 << 16,  // 永続的データ
    WasOptimized       = 1 << 17,  // 最適化済み
    HasJumpTable       = 1 << 18,  // ジャンプテーブル
    HasExceptionHandler = 1 << 19,  // 例外ハンドラ
    IsGuardStub        = 1 << 20,  // ガードスタブ
    HasDirectCalls     = 1 << 21,  // 直接呼び出し
    ContainsSIMD       = 1 << 22,  // SIMDコード
    HasPatchPoints     = 1 << 23   // パッチポイント
};

inline CodeFlags operator|(CodeFlags a, CodeFlags b) {
    return static_cast<CodeFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline CodeFlags& operator|=(CodeFlags& a, CodeFlags b) {
    a = a | b;
    return a;
}

inline bool operator&(CodeFlags a, CodeFlags b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

/**
 * @brief コードバッファパーミッション
 */
enum class CodePermissions : uint8_t {
    ReadOnly,       // 読み取り専用
    ReadWrite,      // 読み書き可能
    ReadExecute,    // 読み取り・実行可能
    ReadWriteExecute // 読み書き・実行可能
};

/**
 * @brief パッチポイント情報
 */
struct PatchPoint {
    uint32_t offset;          // オフセット
    uint32_t length;          // 長さ
    std::string name;         // 名前
    void* originalBytes;      // 元のバイト列
    bool isActive;            // アクティブか
    
    PatchPoint() : offset(0), length(0), originalBytes(nullptr), isActive(false) {}
    
    PatchPoint(uint32_t off, uint32_t len, const std::string& n)
        : offset(off), length(len), name(n), originalBytes(nullptr), isActive(false) {}
};

/**
 * @brief シンボル情報
 */
struct SymbolInfo {
    std::string name;         // シンボル名
    uint32_t offset;          // オフセット
    uint32_t size;            // サイズ
    bool isFunction;          // 関数か
    
    SymbolInfo() : offset(0), size(0), isFunction(false) {}
    
    SymbolInfo(const std::string& n, uint32_t off, uint32_t sz, bool func)
        : name(n), offset(off), size(sz), isFunction(func) {}
};

/**
 * @brief リロケーション情報
 */
struct RelocationInfo {
    uint32_t offset;          // オフセット
    uint32_t targetId;        // ターゲットID
    int32_t addend;           // アデンド
    bool isAbsolute;          // 絶対アドレスか
    
    RelocationInfo() : offset(0), targetId(0), addend(0), isAbsolute(false) {}
    
    RelocationInfo(uint32_t off, uint32_t target, int32_t add, bool abs)
        : offset(off), targetId(target), addend(add), isAbsolute(abs) {}
};

/**
 * @brief 実行統計情報
 */
struct ExecutionStats {
    uint64_t executionCount;          // 実行回数
    uint64_t totalCycles;             // 総サイクル数
    uint64_t totalTimeNs;             // 総実行時間（ナノ秒）
    uint32_t icMissCount;             // ICミス回数
    uint32_t branchMispredictCount;   // 分岐予測ミス回数
    uint32_t cacheMissCount;          // キャッシュミス回数
    double avgCyclesPerExecution;     // 実行あたり平均サイクル数
    
    ExecutionStats()
        : executionCount(0), totalCycles(0), totalTimeNs(0),
          icMissCount(0), branchMispredictCount(0), cacheMissCount(0),
          avgCyclesPerExecution(0.0) {}
    
    void recordExecution(uint64_t cycles, uint64_t timeNs) {
        executionCount++;
        totalCycles += cycles;
        totalTimeNs += timeNs;
        avgCyclesPerExecution = static_cast<double>(totalCycles) / executionCount;
    }
};

/**
 * @brief 最適化情報
 */
struct OptimizationInfo {
    uint32_t optimizationLevel;       // 最適化レベル
    bool inlined;                     // インライン展開済み
    bool loopOptimized;               // ループ最適化済み
    bool hasTypeSpecialization;       // 型特殊化あり
    bool hasSIMDOptimization;         // SIMD最適化あり
    bool hasGuardElimination;         // ガード除去あり
    std::vector<std::string> appliedOptimizations;  // 適用済み最適化
    
    OptimizationInfo()
        : optimizationLevel(0), inlined(false), loopOptimized(false),
          hasTypeSpecialization(false), hasSIMDOptimization(false),
          hasGuardElimination(false) {}
};

/**
 * @brief ネイティブコードエントリ
 */
class CodeEntry {
public:
    CodeEntry(uint64_t id, void* code, size_t size);
    ~CodeEntry();
    
    // 基本アクセス
    uint64_t getId() const { return _id; }
    void* getCode() const { return _code; }
    size_t getSize() const { return _size; }
    uint32_t getEntryOffset() const { return _entryOffset; }
    void setEntryOffset(uint32_t offset) { _entryOffset = offset; }
    
    // エントリポイント
    void* getEntryPoint() const {
        return reinterpret_cast<uint8_t*>(_code) + _entryOffset;
    }
    
    // 状態
    CodeEntryState getState() const { return _state; }
    void setState(CodeEntryState state) { _state = state; }
    bool isInvalidated() const { return _state == CodeEntryState::Invalidated; }
    bool isAvailable() const { return _state == CodeEntryState::Available; }
    
    // 権限管理
    CodePermissions getPermissions() const { return _permissions; }
    void setPermissions(CodePermissions perms);
    bool makeExecutable();
    bool makeWritable();
    
    // フラグ管理
    bool hasFlag(CodeFlags flag) const { return (_flags & flag); }
    void setFlag(CodeFlags flag) { _flags |= flag; }
    void clearFlag(CodeFlags flag) { _flags = static_cast<CodeFlags>(static_cast<uint32_t>(_flags) & ~static_cast<uint32_t>(flag)); }
    CodeFlags getFlags() const { return _flags; }
    
    // パッチ管理
    bool addPatchPoint(uint32_t offset, uint32_t length, const std::string& name);
    bool applyPatch(uint32_t offset, const void* data, uint32_t length);
    bool revertPatch(uint32_t offset);
    bool revertAllPatches();
    const std::vector<PatchPoint>& getPatchPoints() const { return _patchPoints; }
    
    // シンボル管理
    void addSymbol(const std::string& name, uint32_t offset, uint32_t size, bool isFunction);
    const SymbolInfo* findSymbol(const std::string& name) const;
    const SymbolInfo* findSymbolAt(uint32_t offset) const;
    const std::vector<SymbolInfo>& getSymbols() const { return _symbols; }
    
    // リロケーション管理
    void addRelocation(uint32_t offset, uint32_t targetId, int32_t addend, bool isAbsolute);
    bool applyRelocations(const std::unordered_map<uint32_t, void*>& targetMap);
    const std::vector<RelocationInfo>& getRelocations() const { return _relocations; }
    
    // メタデータ
    void setMetadata(const std::string& key, const std::string& value);
    std::string getMetadata(const std::string& key) const;
    bool hasMetadata(const std::string& key) const;
    const std::unordered_map<std::string, std::string>& getAllMetadata() const { return _metadata; }
    
    // 関連付け
    void setFunctionId(uint64_t functionId) { _functionId = functionId; }
    uint64_t getFunctionId() const { return _functionId; }
    
    // 使用統計
    void recordExecution(uint64_t cycles, uint64_t timeNs);
    const ExecutionStats& getExecutionStats() const { return _stats; }
    
    // 最適化情報
    OptimizationInfo& getOptimizationInfo() { return _optimizationInfo; }
    const OptimizationInfo& getOptimizationInfo() const { return _optimizationInfo; }
    
    // ヒープ割り当て
    bool wasAllocatedInHeap() const { return _heapAllocated; }
    
    // メモリ管理
    bool copyFrom(const void* src, size_t size);
    bool zeroMemory();
    
    // 実行サポート
    void* executeFromOffset(uint32_t offset, void* arg1 = nullptr, void* arg2 = nullptr);
    
    // コードダンプ
    std::string dumpCode() const;
    std::string dumpAsm() const;
    std::string dumpHex() const;
    
private:
    uint64_t _id;                 // コードエントリID
    void* _code;                  // コードポインタ
    size_t _size;                 // コードサイズ
    uint32_t _entryOffset;        // エントリポイントオフセット
    CodeEntryState _state;        // 状態
    CodeFlags _flags;             // フラグ
    CodePermissions _permissions; // 権限
    bool _heapAllocated;          // ヒープ割り当てか
    uint64_t _functionId;         // 関連関数ID
    
    std::vector<PatchPoint> _patchPoints;       // パッチポイント
    std::vector<SymbolInfo> _symbols;           // シンボル
    std::vector<RelocationInfo> _relocations;   // リロケーション
    
    std::unordered_map<std::string, std::string> _metadata;  // メタデータ
    
    ExecutionStats _stats;         // 実行統計
    OptimizationInfo _optimizationInfo;  // 最適化情報
    
    // メモリ保護変更
    bool changeProtection(CodePermissions newPerms);
    
    // パッチポイント検索
    PatchPoint* findPatchPoint(uint32_t offset);
};

/**
 * @brief コードキャッシュの設定
 */
struct CodeCacheConfig {
    size_t initialCapacity;          // 初期容量
    size_t maxCapacity;              // 最大容量
    size_t blockSize;                // ブロックサイズ
    bool useGuardPages;              // ガードページ使用
    bool useLargePages;              // 大きなページ使用
    bool enableEviction;             // 退避有効化
    size_t evictionThresholdBytes;   // 退避閾値（バイト）
    float evictionLoadFactor;        // 退避負荷係数
    bool enableSharing;              // 共有有効化
    bool trackPerformance;           // パフォーマンス追跡
    size_t codeAlignmentBytes;       // コードアライメント（バイト）
    bool preferContiguous;           // 連続アドレス空間を優先
    
    CodeCacheConfig()
        : initialCapacity(4 * 1024 * 1024),        // 4MB
          maxCapacity(256 * 1024 * 1024),          // 256MB
          blockSize(64 * 1024),                    // 64KB
          useGuardPages(true),
          useLargePages(false),
          enableEviction(true),
          evictionThresholdBytes(200 * 1024 * 1024), // 200MB
          evictionLoadFactor(0.75f),
          enableSharing(true),
          trackPerformance(true),
          codeAlignmentBytes(64),
          preferContiguous(true) {}
};

/**
 * @brief コードキャッシュ統計情報
 */
struct CodeCacheStats {
    size_t totalAllocatedBytes;     // 総割り当てバイト数
    size_t totalUsedBytes;          // 総使用バイト数
    size_t fragmentedBytes;         // 断片化バイト数
    size_t peakUsage;               // ピーク使用量
    uint64_t totalEntries;          // 総エントリ数
    uint64_t activeEntries;         // アクティブエントリ数
    uint64_t evictedEntries;        // 退避済みエントリ数
    uint64_t invalidatedEntries;    // 無効化済みエントリ数
    uint64_t allocationFailures;    // 割り当て失敗数
    uint64_t evictionCount;         // 退避回数
    double fragmentationRatio;      // 断片化比率
    double usageRatio;              // 使用率
    
    CodeCacheStats()
        : totalAllocatedBytes(0), totalUsedBytes(0), fragmentedBytes(0),
          peakUsage(0), totalEntries(0), activeEntries(0),
          evictedEntries(0), invalidatedEntries(0),
          allocationFailures(0), evictionCount(0),
          fragmentationRatio(0.0), usageRatio(0.0) {}
};

/**
 * @brief コードブロックアロケータ
 */
class CodeBlockAllocator {
public:
    CodeBlockAllocator(size_t blockSize, size_t initialCapacity);
    ~CodeBlockAllocator();
    
    // メモリ割り当て
    void* allocate(size_t size, size_t alignment = 1);
    bool deallocate(void* ptr, size_t size);
    
    // 容量管理
    bool expand(size_t additionalCapacity);
    void reset();
    
    // 使用情報
    size_t getBlockSize() const { return _blockSize; }
    size_t getCapacity() const { return _capacity; }
    size_t getUsedBytes() const { return _usedBytes; }
    size_t getFreeBytes() const { return _capacity - _usedBytes; }
    
    // メモリ管理
    bool changePermissions(void* ptr, size_t size, CodePermissions perms);
    bool flushICache(void* ptr, size_t size);
    
private:
    struct Block {
        void* memory;         // メモリ領域
        size_t size;          // サイズ
        size_t freeOffset;    // 空き領域オフセット
        bool isFull;          // フル状態か
        
        Block(void* mem, size_t sz) : memory(mem), size(sz), freeOffset(0), isFull(false) {}
    };
    
    std::vector<Block> _blocks;       // ブロック
    size_t _blockSize;                // ブロックサイズ
    size_t _capacity;                 // 総容量
    size_t _usedBytes;                // 使用バイト数
    
    // ユーティリティ
    Block* findBlockWithSpace(size_t size, size_t alignment);
    void* allocateInBlock(Block* block, size_t size, size_t alignment);
    void* allocateNewBlock(size_t size);
};

/**
 * @brief 世界最高性能のネイティブコードキャッシュ
 * 
 * JITコンパイラが生成したネイティブコードを効率的に管理します。
 * - 高速コード検索とアクセス
 * - メモリ最適化と再利用
 * - コードパッチングとリロケーション
 * - 実行統計の収集
 */
class CodeCache {
public:
    explicit CodeCache(Context* context, const CodeCacheConfig& config = CodeCacheConfig());
    ~CodeCache();
    
    // コードエントリの追加・検索
    CodeEntry* addCode(void* code, size_t size, uint64_t functionId = 0);
    CodeEntry* findCode(uint64_t id) const;
    CodeEntry* findFunctionCode(uint64_t functionId) const;
    
    // メモリ割り当て
    void* allocateCode(size_t size, size_t alignment = 16);
    bool deallocateCode(void* ptr, size_t size);
    
    // コードエントリ操作
    bool removeEntry(uint64_t id);
    bool invalidateEntry(uint64_t id);
    void invalidateAllEntries();
    
    // 関数関連付け操作
    bool linkFunctionToCode(uint64_t functionId, uint64_t codeId);
    bool unlinkFunction(uint64_t functionId);
    std::vector<CodeEntry*> findFunctionCodeEntries(uint64_t functionId) const;
    
    // イテレーション
    std::vector<CodeEntry*> getAllEntries() const;
    std::vector<CodeEntry*> getEntriesByState(CodeEntryState state) const;
    std::vector<CodeEntry*> getEntriesByFlags(CodeFlags flags) const;
    
    // メモリ管理
    size_t getTotalMemoryUsage() const;
    size_t getAvailableMemory() const;
    bool expandCapacity(size_t additionalCapacity);
    
    // 退避管理
    bool evictEntries(size_t bytesToFree);
    bool evictEntry(uint64_t id);
    void setEvictionPolicy(std::function<std::vector<uint64_t>(size_t)> policy);
    
    // バージング制御
    void setFlushPolicy(std::function<bool(const CodeEntry*)> policy);
    bool flushCode(uint64_t id);
    
    // 共有コード管理
    uint64_t addSharedCode(void* code, size_t size, const std::string& name);
    void* getSharedCode(const std::string& name) const;
    
    // 統計情報
    const CodeCacheStats& getStats() const { return _stats; }
    void resetStats();
    
    // デバッグサポート
    std::string dumpStats() const;
    std::string dumpLayout() const;
    bool verifyIntegrity() const;
    
    // 設定アクセス
    const CodeCacheConfig& getConfig() const { return _config; }
    
 private:
    Context* _context;
    CodeCacheConfig _config;
    std::unique_ptr<CodeBlockAllocator> _allocator;
    
    // コードエントリマップ
    std::unordered_map<uint64_t, std::unique_ptr<CodeEntry>> _entries;
    
    // 関数マッピング
    std::unordered_map<uint64_t, std::vector<uint64_t>> _functionToCodeMap;
    
    // 共有コード
    std::unordered_map<std::string, uint64_t> _sharedCodeMap;
    
    // LRUキャッシュ（最近使用したエントリ）
    struct LRUEntry {
        uint64_t id;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::list<LRUEntry> _lruList;
    std::unordered_map<uint64_t, std::list<LRUEntry>::iterator> _lruMap;
    
    // カスタムポリシー
    std::function<std::vector<uint64_t>(size_t)> _evictionPolicy;
    std::function<bool(const CodeEntry*)> _flushPolicy;
    
    // 統計情報
    CodeCacheStats _stats;
    
    // 同期オブジェクト
    mutable std::shared_mutex _cacheMutex;
    
    // ヘルパーメソッド
    void updateLRU(uint64_t id);
    std::vector<uint64_t> defaultEvictionPolicy(size_t bytesToFree);
    bool defaultFlushPolicy(const CodeEntry* entry);
    uint64_t nextEntryId();
    
    // ユーティリティ
    void updateStats();
};

} // namespace core
} // namespace aerojs