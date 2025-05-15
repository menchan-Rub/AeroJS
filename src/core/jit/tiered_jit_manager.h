/**
 * @file tiered_jit_manager.h
 * @brief 世界最高性能の階層型JITコンパイルシステム
 * @version 2.0.0
 * @license MIT
 */

#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <string>
#include <functional>
#include <future>
#include <atomic>
#include <chrono>
#include <queue>
#include <thread>
#include <condition_variable>

#include "jit_compiler.h"
#include "baseline/baseline_jit.h"
#include "profiler/jit_profiler.h"

namespace aerojs::core {

// 前方宣言
class Context;
class Function;
class BytecodeCompiler;
class CodeCache;
class TraceRecorder;

/**
 * @brief JIT最適化階層
 */
enum class JITTier : uint8_t {
    Interpreter = 0,    // インタプリタ実行
    Baseline    = 1,    // ベースラインJIT (単純な命令レベルコンパイル)
    Optimizing  = 2,    // 最適化JIT (中間段階の最適化)
    SuperTier   = 3,    // 超最適化JIT (完全最適化)
    MetaTracing = 4     // メタトレーシングJIT (実行パス特化)
};

/**
 * @brief コンパイル状態
 */
enum class CompileState : uint8_t {
    None           = 0,  // 未コンパイル
    Queued         = 1,  // コンパイルキュー投入済み
    Compiling      = 2,  // コンパイル中
    Completed      = 3,  // コンパイル完了
    Failed         = 4,  // コンパイル失敗
    Invalidated    = 5   // 無効化済み
};

/**
 * @brief オンスタックリプレイスメント (OSR) 状態
 */
struct OSRData {
    uint32_t loopHeaderOffset;  // ループヘッダオフセット
    uint64_t osrEntryPoint;     // OSRエントリポイント
    void* osrData;              // OSR固有データ
    
    OSRData() : loopHeaderOffset(0), osrEntryPoint(0), osrData(nullptr) {}
    
    OSRData(uint32_t offset, uint64_t entry, void* data)
        : loopHeaderOffset(offset), osrEntryPoint(entry), osrData(data) {}
};

/**
 * @brief コンパイル対象関数の情報
 */
struct CompileTask {
    uint64_t functionId;        // 関数ID
    JITTier targetTier;         // 対象JIT階層
    uint32_t priority;          // 優先度
    bool isOSR;                 // OSRコンパイルか
    uint32_t osrOffset;         // OSRの場合のバイトコードオフセット
    std::chrono::steady_clock::time_point enqueueTime;  // キュー投入時刻
    
    CompileTask() : functionId(0), targetTier(JITTier::Baseline), 
                    priority(0), isOSR(false), osrOffset(0) {}
                    
    CompileTask(uint64_t id, JITTier tier, uint32_t prio = 0)
        : functionId(id), targetTier(tier), priority(0), isOSR(false), osrOffset(0) {
        enqueueTime = std::chrono::steady_clock::now();
    }
    
    CompileTask(uint64_t id, JITTier tier, uint32_t prio, uint32_t osr_offset)
        : functionId(id), targetTier(tier), priority(prio), isOSR(true), osrOffset(osr_offset) {
        enqueueTime = std::chrono::steady_clock::now();
    }
    
    // 優先度比較演算子
    bool operator<(const CompileTask& other) const {
        // 高優先度が先に処理される
        return priority < other.priority;
    }
};

/**
 * @brief 関数ごとのJIT状態管理
 */
struct FunctionJITState {
    CompileState states[5];      // 各JIT階層の状態 (階層数に対応)
    void* compiledCode[5];       // 各JIT階層のコンパイル済みコード
    uint64_t compilationTime[5]; // 各JIT階層のコンパイル時間 (ナノ秒)
    uint64_t codeSize[5];        // 各JIT階層のコードサイズ
    int32_t executeCount;        // 実行回数
    int32_t entryBackedgeCount;  // バックエッジカウント
    int32_t tierUpCounter;       // 階層昇格カウンタ
    bool hasInlinedCalls;        // インライン展開された呼び出しがあるか
    std::vector<OSRData> osrEntries;  // OSRエントリポイント
    std::vector<uint64_t> inlinedFunctions;  // インライン展開された関数のID
    std::atomic<bool> pendingDeoptimization;  // 最適化解除待ちフラグ
    
    FunctionJITState() : executeCount(0), entryBackedgeCount(0), 
                         tierUpCounter(0), hasInlinedCalls(false),
                         pendingDeoptimization(false) {
        for (int i = 0; i < 5; i++) {
            states[i] = CompileState::None;
            compiledCode[i] = nullptr;
            compilationTime[i] = 0;
            codeSize[i] = 0;
        }
    }
};

/**
 * @brief 階層型JIT最適化マネージャ
 * 
 * 複数の最適化レベルを持つJITコンパイラの管理と制御を行います。
 * 実行統計に基づいて最適なJIT階層を選択し、
 * 必要に応じて階層間の移行を行います。
 */
class TieredJITManager {
public:
    // コンストラクタ
    explicit TieredJITManager(Context* context);
    
    // デストラクタ
    ~TieredJITManager();
    
    // 対象関数の呼び出し実装
    void* getCodeForFunction(Function* function);
    
    // OSR実行
    void* getOSREntryPoint(Function* function, uint32_t bytecodeOffset);
    bool enterOSR(Function* function, uint32_t bytecodeOffset, void* framePtr);
    
    // ホットコード判定
    bool isHotFunction(Function* function) const;
    bool isHotLoop(Function* function, uint32_t bytecodeOffset) const;
    
    // 実行統計更新
    void recordExecution(Function* function);
    void recordBackEdge(Function* function, uint32_t loopHeaderOffset);
    
    // 階層の各種制御
    JITTier getCurrentTier(Function* function) const;
    bool promoteTier(Function* function, JITTier targetTier);
    
    // 最適化解除
    bool deoptimizeFunction(Function* function);
    bool invalidateCode(Function* function, JITTier tier);
    
    // コンパイルキュー制御
    bool queueForCompilation(Function* function, JITTier targetTier, 
                            uint32_t priority = 0);
    bool queueForOSRCompilation(Function* function, uint32_t bytecodeOffset, 
                               JITTier targetTier, uint32_t priority = 0);
    
    // 非同期コンパイルの制御
    void startCompilerThreads(uint32_t threadCount = 0);
    void stopCompilerThreads();
    
    // インライン展開制御
    bool canInlineFunction(Function* caller, Function* callee) const;
    void markFunctionInlined(Function* caller, Function* callee);
    
    // 設定
    struct TieredJITConfig {
        // 階層昇格閾値
        int32_t baselineTierUpThreshold;   // ベースライン→最適化JITへの閾値
        int32_t optimizingTierUpThreshold; // 最適化→超最適化JITへの閾値
        int32_t tracingTierThreshold;      // トレーシングJIT起動閾値
        
        // OSR閾値
        int32_t osrEntryThreshold;         // OSR開始閾値
        int32_t osrMinLoopCount;           // OSR最小ループ回数
        
        // インライン展開制限
        uint32_t maxInlineDepth;           // 最大インライン展開深度
        uint32_t maxInlineSize;            // 最大インライン展開サイズ
        
        // コンパイル制限
        uint32_t maxCompileThreads;        // 最大コンパイルスレッド数
        uint32_t maxCompileQueueSize;      // 最大キューサイズ
        uint32_t compileBudgetMs;          // コンパイル予算(ミリ秒)
        
        // 動的調整
        bool enableDynamicThresholds;      // 動的閾値調整
        bool enableSpeculativeCompilation; // 投機的コンパイル
        bool enableHeuristicInlining;      // 発見的インライン展開
        
        // キャッシュ設定
        size_t codeCacheMaxSize;           // コードキャッシュ最大サイズ
        
        TieredJITConfig()
            : baselineTierUpThreshold(100),
              optimizingTierUpThreshold(10000),
              tracingTierThreshold(1000),
              osrEntryThreshold(1000),
              osrMinLoopCount(50),
              maxInlineDepth(5),
              maxInlineSize(1000),
              maxCompileThreads(4),
              maxCompileQueueSize(1000),
              compileBudgetMs(50),
              enableDynamicThresholds(true),
              enableSpeculativeCompilation(true),
              enableHeuristicInlining(true),
              codeCacheMaxSize(64 * 1024 * 1024) {}
    };
    
    void setConfig(const TieredJITConfig& config);
    const TieredJITConfig& getConfig() const;

private:
    // コンテキスト
    Context* _context;
    
    // 各階層のJITコンパイラ
    std::unique_ptr<JITCompiler> _baselineJIT;
    std::unique_ptr<JITCompiler> _optimizingJIT;
    std::unique_ptr<JITCompiler> _superTierJIT;
    std::unique_ptr<JITCompiler> _metaTracingJIT;
    
    // プロファイラ
    std::unique_ptr<JITProfiler> _profiler;
    
    // バイトコードコンパイラ
    std::unique_ptr<BytecodeCompiler> _bytecodeCompiler;
    
    // トレースレコーダー
    std::unique_ptr<TraceRecorder> _traceRecorder;
    
    // 関数ごとのJIT状態
    std::unordered_map<uint64_t, FunctionJITState> _jitStates;
    
    // コンパイルキュー
    std::priority_queue<CompileTask> _compileQueue;
    
    // 同期オブジェクト
    mutable std::mutex _jitMutex;
    std::mutex _queueMutex;
    std::condition_variable _queueCondition;
    
    // コンパイルスレッド
    std::vector<std::thread> _compilerThreads;
    std::atomic<bool> _isCompilerRunning;
    
    // 設定
    TieredJITConfig _config;
    
    // 統計情報
    struct Stats {
        uint64_t totalCompilations;
        uint64_t totalOSRCompilations;
        uint64_t totalCompilationTimeNs;
        uint64_t totalExecutions;
        uint64_t totalDeoptimizations;
        
        Stats() : totalCompilations(0), totalOSRCompilations(0),
                  totalCompilationTimeNs(0), totalExecutions(0),
                  totalDeoptimizations(0) {}
    };
    
    Stats _stats;
    
    // コンパイル履歴（機械学習用）
    struct CompilationEvent {
        uint64_t functionId;
        JITTier tier;
        uint64_t compilationTimeNs;
        uint64_t codeSize;
        bool success;
    };
    
    std::vector<CompilationEvent> _compilationHistory;
    
    // ホットスポット検出用のヒートマップ
    std::unordered_map<uint64_t, std::unordered_map<uint32_t, int32_t>> _loopHeatMap;
    
    // スレッド管理
    void compilerThreadMain();
    bool processNextTask();
    
    // 内部実装
    bool compileFunction(Function* function, JITTier targetTier);
    bool compileOSRFunction(Function* function, uint32_t bytecodeOffset, JITTier targetTier);
    
    // 階層選択ロジック
    JITTier selectOptimalTier(Function* function) const;
    
    // インライン展開ヒューリスティクス
    bool shouldInline(Function* caller, Function* callee) const;
    
    // 動的閾値調整
    void adjustThresholds();
    
    // 最適化解除サポート
    void prepareForDeoptimization(Function* function);
    
    // パフォーマンス追跡
    void trackCompilationPerformance(uint64_t functionId, JITTier tier, 
                                    uint64_t timeNs, uint64_t codeSize, bool success);
    
    // 統計分析と自己調整
    void analyzePerfData();
    
    // OSRサポート
    void registerOSREntry(Function* function, uint32_t offset, uint64_t entryPoint, void* data);
    
    // 対応するJITコンパイラを取得
    JITCompiler* getCompilerForTier(JITTier tier);
    
    // 投機的コンパイル
    void performSpeculativeCompilation();
    
    // 実行パスフィードバック
    void collectPathFeedback(Function* function);
    
    // メガモーフィック呼び出しサイト検出
    void identifyMegamorphicCallsites(Function* function);
    
    // SIMD最適化機会の検出
    bool detectSIMDOpportunities(Function* function);
    
    // 学習ベースの最適化器選択
    JITTier predictBestTier(Function* function) const;
};

}  // namespace aerojs::core 