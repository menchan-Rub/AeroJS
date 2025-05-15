/**
 * @file inline_cache.h
 * @brief 世界最高のポリモーフィックインラインキャッシュシステム
 * @version 2.0.0
 * @license MIT
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <array>
#include <bitset>
#include <mutex>
#include <atomic>

namespace aerojs {
namespace core {

// 前方宣言
class Value;
class Object;
class Context;
class Function;
class String;

/**
 * @brief キャッシュエントリ状態
 */
enum class CacheState : uint8_t {
    Uninitialized,   // 未初期化
    Monomorphic,     // 単一形状
    Polymorphic,     // 複数形状（2-4）
    Megamorphic,     // 多数形状（5以上）
    Generic          // 汎用（遅いパス）
};

/**
 * @brief キャッシュ操作タイプ
 */
enum class CacheOperation : uint8_t {
    PropertyLoad,     // プロパティ読み込み
    PropertyStore,    // プロパティ書き込み
    MethodCall,       // メソッド呼び出し
    ElementLoad,      // 配列要素読み込み
    ElementStore,     // 配列要素書き込み
    ConstantLoad,     // 定数読み込み
    ProtoLoad,        // プロトタイプチェーン読み込み
    HasProperty,      // プロパティ存在確認
    InstanceOf,       // インスタンス判定
    In,               // inオペレータ
    FastCall          // 高速呼び出し
};

/**
 * @brief キャッシュヒント
 */
enum class CacheHint : uint8_t {
    None,             // ヒントなし
    Likely,           // 高確率
    Unlikely,         // 低確率
    HotPath,          // ホットパス
    ColdPath,         // コールドパス
    PinnedProperty,   // ピン留めプロパティ
    ConstantProp,     // 定数伝播
    LeafFunction,     // リーフ関数
    NoSideEffect,     // 副作用なし
    FastAccess        // 高速アクセス
};

/**
 * @brief インラインキャッシュフラグ
 */
enum class ICFlags : uint32_t {
    None                    = 0,
    Optimized               = 1 << 0,    // 最適化済み
    TypeSpecialized         = 1 << 1,    // 型特殊化済み
    Inlined                 = 1 << 2,    // インライン展開済み
    Patched                 = 1 << 3,    // パッチ適用済み
    GuardInserted           = 1 << 4,    // ガード挿入済み
    SIMDOptimized           = 1 << 5,    // SIMD最適化済み
    UsesShapeGuard          = 1 << 6,    // シェイプガード使用
    UsesTypeGuard           = 1 << 7,    // 型ガード使用
    AccessesProtoChain      = 1 << 8,    // プロトタイプチェーンアクセス
    CanBeHoisted            = 1 << 9,    // ループ外に移動可能
    HasNegativeGuard        = 1 << 10,   // 否定ガードあり
    DirectAccess            = 1 << 11,   // 直接アクセス
    HasTransitionMap        = 1 << 12,   // 遷移マップあり
    RequiresTypeCheck       = 1 << 13,   // 型チェック必須
    AllowsSpecialization    = 1 << 14,   // 特殊化許可
    BlocksSIMD              = 1 << 15,   // SIMD妨げる
    IsMonomorphicFastPath   = 1 << 16,   // 単一形状高速パス
    IsPolymorphicFastPath   = 1 << 17,   // 複数形状高速パス
    IsMegamorphicSlowPath   = 1 << 18,   // 多形状低速パス
    IsGlobalIC              = 1 << 19,   // グローバルIC
    IsTransitionIC          = 1 << 20,   // 遷移IC
    AllowsShapeTransition   = 1 << 21,   // シェイプ遷移許可
    FeedsBackToJIT          = 1 << 22,   // JITにフィードバック
    AllowsStorageInlining   = 1 << 23    // ストレージインライン化許可
};

inline ICFlags operator|(ICFlags a, ICFlags b) {
    return static_cast<ICFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline ICFlags& operator|=(ICFlags& a, ICFlags b) {
    a = a | b;
    return a;
}

inline bool operator&(ICFlags a, ICFlags b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

/**
 * @brief オブジェクト形状ID型
 */
using ShapeID = uint64_t;

/**
 * @brief プロパティ位置情報
 */
struct PropertyLocation {
    enum class Type : uint8_t {
        Slot,              // 直接スロット
        Indexed,           // インデックス付き
        Named,             // 名前付き
        Accessor,          // アクセサプロパティ
        ConstantFunction,  // 定数関数
        Prototype,         // プロトタイプチェーン
        Dynamic,           // 動的計算
        Nonexistent        // 存在しない
    };
    
    Type type;
    uint32_t offset;       // オフセット（スロット/インデックス）
    uint32_t attributes;   // 属性フラグ
    Value* getter;         // ゲッター（アクセサの場合）
    Value* setter;         // セッター（アクセサの場合）
    ShapeID parentShape;   // 親シェイプ（プロトタイプの場合）
    
    PropertyLocation()
        : type(Type::Nonexistent), offset(0), attributes(0),
          getter(nullptr), setter(nullptr), parentShape(0) {}
          
    bool isAccessor() const { return type == Type::Accessor; }
    bool isSlot() const { return type == Type::Slot; }
    bool isIndexed() const { return type == Type::Indexed; }
    bool isNamed() const { return type == Type::Named; }
    bool isPrototype() const { return type == Type::Prototype; }
    bool exists() const { return type != Type::Nonexistent; }
};

/**
 * @brief キャッシュエントリ
 */
struct ICEntry {
    ShapeID shapeId;              // オブジェクトシェイプID
    uint32_t feedback;            // フィードバックカウンタ
    PropertyLocation location;    // プロパティ位置
    void* fastPath;               // 高速パスコード
    
    ICEntry() : shapeId(0), feedback(0), fastPath(nullptr) {}
    
    ICEntry(ShapeID id, const PropertyLocation& loc)
        : shapeId(id), feedback(1), location(loc), fastPath(nullptr) {}
};

/**
 * @brief インラインキャッシュの統計情報
 */
struct ICStats {
    uint32_t totalAccesses;       // 総アクセス数
    uint32_t cacheHits;           // キャッシュヒット数
    uint32_t cacheMisses;         // キャッシュミス数
    uint32_t transitions;         // 遷移回数
    double hitRatio;              // ヒット率
    
    ICStats() : totalAccesses(0), cacheHits(0), cacheMisses(0), transitions(0), hitRatio(0.0) {}
};

/**
 * @brief プロファイル駆動型ポリモーフィックインラインキャッシュ
 */
class InlineCache {
public:
    // コンテキスト情報を持つインスタンス化
    explicit InlineCache(Context* context, CacheOperation operation, const std::string& propertyName, uint32_t siteId = 0);
    ~InlineCache();
    
    // キャッシュ操作
    Value get(Value receiver);
    void put(Value receiver, Value value);
    Value call(Value receiver, const std::vector<Value>& args);
    
    // キャッシュ更新
    void update(ShapeID shapeId, const PropertyLocation& location);
    void invalidate();
    void transition(ShapeID oldShape, ShapeID newShape);
    
    // フィードバック管理
    void recordHit(ShapeID shapeId);
    void recordMiss();
    
    // エントリ管理
    void addEntry(const ICEntry& entry);
    void removeEntry(ShapeID shapeId);
    const ICEntry* findEntry(ShapeID shapeId) const;
    void optimizeEntries();
    
    // キャッシュ状態
    CacheState getState() const;
    size_t getEntryCount() const { return _entries.size(); }
    
    // フラグ管理
    bool hasFlag(ICFlags flag) const { return (_flags & flag); }
    void setFlag(ICFlags flag) { _flags |= flag; }
    void clearFlag(ICFlags flag) { _flags = static_cast<ICFlags>(static_cast<uint32_t>(_flags) & ~static_cast<uint32_t>(flag)); }
    
    // 統計情報
    const ICStats& getStats() const { return _stats; }
    void resetStats() { _stats = ICStats(); }
    
    // 高速パスコード管理
    void setFastPath(ShapeID shapeId, void* code);
    void* getFastPath(ShapeID shapeId) const;
    void* getGenericPath() const { return _genericPath; }
    void setGenericPath(void* code) { _genericPath = code; }
    
    // ヒント設定
    void setHint(CacheHint hint) { _hint = hint; }
    CacheHint getHint() const { return _hint; }
    
    // アクセサ
    uint32_t getSiteId() const { return _siteId; }
    const std::string& getPropertyName() const { return _propertyName; }
    CacheOperation getOperation() const { return _operation; }
    Context* getContext() const { return _context; }
    
    // 特殊キャッシュ構築
    static std::unique_ptr<InlineCache> createPropertyCache(Context* context, const std::string& name, uint32_t siteId = 0);
    static std::unique_ptr<InlineCache> createMethodCache(Context* context, const std::string& name, uint32_t siteId = 0);
    static std::unique_ptr<InlineCache> createElementCache(Context* context, uint32_t siteId = 0);
    
    // 複製
    std::unique_ptr<InlineCache> clone() const;
    
    // デバッグ情報
    std::string toString() const;
    
    // 動的生成キャッシュ用
    void generateSpecializedCode();

private:
    Context* _context;
    CacheOperation _operation;
    std::string _propertyName;
    uint32_t _siteId;
    
    std::vector<ICEntry> _entries;
    ICFlags _flags;
    CacheHint _hint;
    CacheState _state;
    
    void* _genericPath;
    mutable std::mutex _cacheMutex;
    std::atomic<uint32_t> _transitionCount;
    
    ICStats _stats;
    
    // シェイプID取得ヘルパー
    ShapeID getShapeId(Value value) const;
    
    // キャッシュ状態更新
    void updateState();
    
    // 内部実装ヘルパー
    Value getMonomorphic(Value receiver, ShapeID shapeId);
    Value getPolymorphic(Value receiver, ShapeID shapeId);
    Value getMegamorphic(Value receiver, ShapeID shapeId);
    Value getGeneric(Value receiver);
    
    void putMonomorphic(Value receiver, Value value, ShapeID shapeId);
    void putPolymorphic(Value receiver, Value value, ShapeID shapeId);
    void putMegamorphic(Value receiver, Value value, ShapeID shapeId);
    void putGeneric(Value receiver, Value value);
    
    Value callMonomorphic(Value receiver, const std::vector<Value>& args, ShapeID shapeId);
    Value callPolymorphic(Value receiver, const std::vector<Value>& args, ShapeID shapeId);
    Value callMegamorphic(Value receiver, const std::vector<Value>& args, ShapeID shapeId);
    Value callGeneric(Value receiver, const std::vector<Value>& args);
    
    // スペシャライズドコード生成
    void* generateGetterCode(const ICEntry& entry);
    void* generateSetterCode(const ICEntry& entry);
    void* generateMethodCode(const ICEntry& entry);
    
    // 型特殊化パス生成
    void* generateSpecializedPathForPrimitives();
    void* generateSpecializedPathForObjects();
    void* generateSpecializedPathForArrays();
    
    // インライン呼び出し最適化
    Value optimizeMethodCall(Function* method, const std::vector<Value>& args);
    
    // 最適化ヘルパー
    void applyInliningHeuristics();
    void mergeSimilarEntries();
    void sortEntriesByHitCount();
    
    // トランジションマップ
    struct TransitionMapEntry {
        ShapeID sourceShape;
        ShapeID targetShape;
        std::string propertyName;
        uint32_t count;
    };
    
    std::vector<TransitionMapEntry> _transitionMap;
    
    void recordTransition(ShapeID source, ShapeID target, const std::string& propName);
    void optimizeTransitions();
    bool predictNextTransition(ShapeID currentShape, std::string& outPropName, ShapeID& outTargetShape) const;
};

/**
 * @brief グローバルインラインキャッシュマネージャ
 */
class InlineCacheManager {
public:
    explicit InlineCacheManager(Context* context);
    ~InlineCacheManager();
    
    // キャッシュ管理
    InlineCache* getCache(uint32_t siteId) const;
    InlineCache* createCache(CacheOperation operation, const std::string& propertyName, uint32_t siteId = 0);
    void releaseCache(uint32_t siteId);
    
    // キャッシュ無効化
    void invalidateAll();
    void invalidateForShape(ShapeID shapeId);
    void invalidateForProperty(const std::string& propertyName);
    
    // キャッシュポリシー設定
    void setMaxCacheEntries(size_t maxEntries);
    void setCacheLifetime(uint32_t lifetimeMs);
    void setInliningThreshold(uint32_t threshold);
    void setTypeFeedbackThreshold(uint32_t threshold);
    
    // 統計
    ICStats getGlobalStats() const;
    void dumpCacheStatistics() const;
    
    // Site ID生成
    uint32_t generateSiteId();
    
    // コンテキスト
    Context* getContext() const { return _context; }

private:
    Context* _context;
    std::unordered_map<uint32_t, std::unique_ptr<InlineCache>> _caches;
    std::unordered_map<std::string, std::vector<uint32_t>> _propertyCaches;
    std::unordered_map<ShapeID, std::vector<uint32_t>> _shapeCaches;
    
    uint32_t _nextSiteId;
    size_t _maxCacheEntries;
    uint32_t _cacheLifetimeMs;
    uint32_t _inliningThreshold;
    uint32_t _typeFeedbackThreshold;
    
    mutable std::mutex _managerMutex;
    
    // キャッシュ最適化
    void optimizeCaches();
    void evictLeastUsedCaches();
    
    // キャッシュ使用統計
    struct CacheUsage {
        uint32_t siteId;
        uint32_t lastAccess;
        uint32_t hitCount;
        double hitRatio;
    };
    
    std::vector<CacheUsage> _cacheUsage;
    void updateCacheUsage(uint32_t siteId, const ICStats& stats);
};

} // namespace core
} // namespace aerojs 