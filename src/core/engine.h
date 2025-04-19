/**
 * @file engine.h
 * @brief AeroJS JavaScript エンジンのメインエンジンクラスの定義
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_CORE_ENGINE_H
#define AEROJS_CORE_ENGINE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <cstdint>

namespace aerojs {

// 前方宣言
namespace utils {
namespace memory {
class MemoryAllocator;
class MemoryPoolManager;
} // namespace memory
} // namespace utils

namespace core {

// 前方宣言
class Context;

/**
 * @brief エンジンのメモリ統計情報
 */
struct MemoryStats {
    size_t totalHeapSize;       // 全ヒープサイズ（バイト）
    size_t usedHeapSize;        // 使用中のヒープサイズ（バイト）
    size_t heapSizeLimit;       // ヒープサイズの上限（バイト）
    size_t externalMemorySize;  // 外部メモリサイズ（バイト）
    size_t objectCount;         // オブジェクト数
    size_t functionCount;       // 関数オブジェクト数
    size_t arrayCount;          // 配列オブジェクト数
    size_t stringCount;         // 文字列オブジェクト数
    size_t symbolCount;         // シンボル数
    size_t gcCount;             // GC実行回数
    size_t fullGcCount;         // フルGC実行回数
    uint64_t gcTime;            // GC実行累積時間（ミリ秒）
};

/**
 * @brief コンテキスト列挙コールバック関数型
 * @param context 列挙されたコンテキスト
 * @param userData ユーザーデータ
 * @return 列挙を継続する場合はtrue、中止する場合はfalse
 */
typedef bool (*ContextEnumerator)(Context* context, void* userData);

/**
 * @brief エンジンデータのクリーンアップ関数型
 * @param data クリーンアップするデータ
 */
typedef void (*EngineDataCleaner)(void* data);

/**
 * @brief AeroJS JavaScript エンジンのメインクラス
 * 
 * エンジン全体の構成と動作を管理するシングルトンクラス。
 * コンテキストの作成、メモリ管理、ガベージコレクション、JITコンパイルの設定などを担当する。
 */
class Engine {
public:
    /**
     * @brief シングルトンインスタンスを取得
     * @return Engine* エンジンインスタンス
     */
    static Engine* getInstance();
    
    /**
     * @brief シングルトンインスタンスを破棄
     */
    static void destroyInstance();
    
    /**
     * @brief デストラクタ
     */
    ~Engine();
    
    /**
     * @brief エンジンパラメータを設定
     * @param name パラメータ名
     * @param value パラメータ値
     */
    void setParameter(const std::string& name, const std::string& value);
    
    /**
     * @brief エンジンパラメータを取得
     * @param name パラメータ名
     * @return std::string パラメータ値（存在しない場合は空文字列）
     */
    std::string getParameter(const std::string& name) const;
    
    /**
     * @brief ガベージコレクションを実行
     * @param context 対象のコンテキスト（nullの場合は全コンテキスト）
     * @param force 強制実行フラグ（trueの場合はGC間隔に関わらず実行）
     */
    void collectGarbage(Context* context = nullptr, bool force = false);
    
    /**
     * @brief グローバルガベージコレクションを実行
     * @param force 強制実行フラグ
     */
    void collectGarbageGlobal(bool force = false);
    
    /**
     * @brief 登録されているコンテキストを列挙
     * @param callback 各コンテキストに対して呼び出されるコールバック関数
     * @param userData コールバック関数に渡されるユーザーデータ
     */
    void enumerateContexts(ContextEnumerator callback, void* userData);
    
    /**
     * @brief コンテキストをエンジンに登録
     * @param context 登録するコンテキスト
     */
    void registerContext(Context* context);
    
    /**
     * @brief コンテキストの登録を解除
     * @param context 登録解除するコンテキスト
     */
    void unregisterContext(Context* context);
    
    /**
     * @brief JITコンパイラの有効/無効を設定
     * @param enable 有効にする場合はtrue
     * @return bool 設定に成功した場合はtrue
     */
    bool enableJIT(bool enable);
    
    /**
     * @brief JITコンパイルのしきい値を設定
     * @param threshold JITコンパイルを行う実行回数のしきい値
     * @return bool 設定に成功した場合はtrue
     */
    bool setJITThreshold(int threshold);
    
    /**
     * @brief 最適化レベルを設定
     * @param level 最適化レベル（0: なし、1: 低、2: 中、3: 高）
     * @return bool 設定に成功した場合はtrue
     */
    bool setOptimizationLevel(int level);
    
    /**
     * @brief メモリ使用上限を設定
     * @param limit メモリ使用上限（バイト）
     * @return bool 設定に成功した場合はtrue
     */
    bool setMemoryLimit(size_t limit);
    
    /**
     * @brief メモリ統計情報を取得
     * @param stats 統計情報の格納先
     */
    void getMemoryStats(MemoryStats* stats);
    
    /**
     * @brief メモリアロケータを取得
     * @return utils::memory::MemoryAllocator* メモリアロケータ
     */
    utils::memory::MemoryAllocator* getMemoryAllocator() const { return m_memoryAllocator; }
    
    /**
     * @brief メモリプールマネージャを取得
     * @return utils::memory::MemoryPoolManager* メモリプールマネージャ
     */
    utils::memory::MemoryPoolManager* getMemoryPoolManager() const { return m_memoryPoolManager; }
    
    /**
     * @brief エンジンにカスタムデータを関連付ける
     * @param key データの識別キー
     * @param data データポインタ
     * @param cleaner データ解放時に呼び出すクリーナー関数（省略可）
     * @return bool 設定に成功した場合はtrue
     */
    bool setEngineData(const std::string& key, void* data, EngineDataCleaner cleaner = nullptr);
    
    /**
     * @brief エンジンに関連付けられたカスタムデータを取得
     * @param key データの識別キー
     * @return void* データポインタ（存在しない場合はnull）
     */
    void* getEngineData(const std::string& key) const;
    
    /**
     * @brief エンジンに関連付けられたカスタムデータを削除
     * @param key データの識別キー
     * @return bool 削除に成功した場合はtrue
     */
    bool removeEngineData(const std::string& key);

private:
    /**
     * @brief コンストラクタ（private: シングルトンパターン）
     */
    Engine();
    
    // コピー禁止
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    
    /**
     * @brief ガベージコレクションの実行
     * @param context 対象のコンテキスト
     */
    void performGarbageCollection(Context* context);
    
    /**
     * @brief 到達可能なオブジェクトをマーク
     * @param context 対象のコンテキスト
     */
    void markReachableObjects(Context* context);
    
    /**
     * @brief マークされていないオブジェクトを回収
     * @param context 対象のコンテキスト
     */
    void sweepUnmarkedObjects(Context* context);
    
    /**
     * @brief ガベージコレクションをスケジュール
     */
    void scheduleGarbageCollection();
    
    // エンジン統計情報構造体
    struct GCStats {
        size_t gcCount = 0;      // GC実行回数
        size_t fullGcCount = 0;  // フルGC実行回数
        uint64_t gcTime = 0;     // GC実行累積時間（ミリ秒）
    };
    
    // エンジンデータエントリ構造体
    struct EngineDataEntry {
        void* data;                  // データポインタ
        EngineDataCleaner cleaner;   // クリーナー関数
        
        ~EngineDataEntry() {
            if (data && cleaner) {
                cleaner(data);
            }
        }
    };
    
    // シングルトンインスタンス
    static Engine* s_instance;
    
    // エンジンパラメータ
    std::unordered_map<std::string, std::string> m_parameters;
    
    // 登録されたコンテキスト
    std::vector<Context*> m_contexts;
    
    // メモリ関連
    utils::memory::MemoryAllocator* m_memoryAllocator;
    utils::memory::MemoryPoolManager* m_memoryPoolManager;
    size_t m_memoryLimit;
    
    // GC関連
    GCStats m_gcStats;
    uint64_t m_lastGCTime;
    bool m_gcRunning;
    
    // JIT関連
    bool m_jitEnabled;
    int m_jitThreshold;
    int m_optimizationLevel;
    
    // カスタムデータ
    std::unordered_map<std::string, EngineDataEntry> m_engineData;
    
    // 同期用
    mutable std::mutex m_mutex;
    mutable std::mutex m_gcMutex;
    mutable std::mutex m_dataMutex;
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_ENGINE_H 