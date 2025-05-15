#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <array>
#include <functional>
#include <optional>
#include <bitset>
#include <mutex>
#include <atomic>

#include "../../ir/ir.h"
#include "../../jit_compiler.h"
#include "x86_64_code_generator.h"
#include "x86_64_registers.h"

namespace aerojs {
namespace core {

// CPU機能フラグ
enum class CPUFeature : uint32_t {
    SSE         = 1 << 0,
    SSE2        = 1 << 1,
    SSE3        = 1 << 2,
    SSSE3       = 1 << 3,
    SSE41       = 1 << 4,
    SSE42       = 1 << 5,
    AVX         = 1 << 6,
    AVX2        = 1 << 7,
    FMA         = 1 << 8,
    BMI1        = 1 << 9,
    BMI2        = 1 << 10,
    POPCNT      = 1 << 11,
    LZCNT       = 1 << 12,
    F16C        = 1 << 13,
    MOVBE       = 1 << 14,
    AES         = 1 << 15,
    PCLMUL      = 1 << 16,
    RDRAND      = 1 << 17,
    RDSEED      = 1 << 18,
    ADX         = 1 << 19,
    AVX512F     = 1 << 20,
    AVX512VL    = 1 << 21,
    AVX512BW    = 1 << 22,
    AVX512DQ    = 1 << 23,
    AVX512CD    = 1 << 24,
    AVX512IFMA  = 1 << 25,
    AVX512VBMI  = 1 << 26
};
using CPUFeatureSet = std::bitset<32>;

/**
 * @brief コード生成の最適化オプション
 */
struct JITCompileOptions {
    CodeGenOptFlags codeGenFlags = CodeGenOptFlags::PeepholeOptimize | 
                                  CodeGenOptFlags::AlignLoops | 
                                  CodeGenOptFlags::OptimizeJumps |
                                  CodeGenOptFlags::CacheAware;
    bool enableSIMD = true;          // SIMDベクトル化を有効にする
    bool enableFastMath = true;      // 高速数学関数（精度を犠牲に速度重視）
    bool enableTracing = false;      // トレース生成を有効にする
    bool enableCacheOpt = true;      // キャッシュ最適化を有効にする
    bool enableHotCodeInlining = true; // ホットコードパスのインライン化
    bool enableTailCallOpt = true;   // 末尾呼び出し最適化
    bool enableSpeculation = true;   // 投機的実行の最適化
    bool enableGCStackMapGen = true; // GCスタックマップの生成
    bool enableFunctionSplitting = true; // 関数分割最適化
    bool enableLoopUnrolling = true; // ループアンローリング
    bool enableRegisterHinting = true; // レジスタヒント
    bool enableMicroarchOpt = true;  // マイクロアーキテクチャ特化の最適化
    bool enableProfileGuidedOpt = false; // プロファイル誘導型最適化
    bool enableSafepointInsertion = true; // セーフポイント挿入
    uint8_t loopUnrollFactor = 4;     // ループアンローリング係数
    uint8_t inlineDepth = 3;          // インライン化の深さ
    uint32_t hotThreshold = 1000;     // ホットコード閾値
    uint32_t minInlineSize = 8;       // インライン化対象の最小サイズ
    uint32_t maxInlineSize = 64;      // インライン化対象の最大サイズ
};

/**
 * @brief 生成コードのメタデータ
 */
struct CodeMetadata {
    std::vector<std::pair<size_t, size_t>> safepointOffsets; // サイズとオフセットのペア
    std::vector<std::pair<size_t, std::string>> labelOffsets; // オフセットとラベル名のペア
    std::vector<std::tuple<size_t, size_t, uint32_t>> stackMapEntries; // オフセット、サイズ、インデックスのセット
    std::unordered_map<size_t, size_t> irToNativeMap; // IR命令インデックスからネイティブコードオフセットへのマッピング
    std::unordered_map<size_t, size_t> nativeToIrMap; // ネイティブコードオフセットからIR命令インデックスへのマッピング
    std::string disassembly;          // 逆アセンブル結果
    std::vector<std::string> annotations; // コード注釈
    uint32_t hotspotCount = 0;        // 推定ホットスポット数
    std::vector<uint8_t> jumpTable;   // ジャンプテーブルデータ
    uint32_t jumpTableOffset = 0;     // ジャンプテーブルのコード内オフセット
};

/**
 * @brief x86_64アーキテクチャ向け超高性能JITコンパイラ
 * 
 * IR関数をx86_64マシンコードに変換し、実行可能にします。
 * 高度なレジスタアロケーション、最適化、ハードウェア機能活用を提供します。
 */
class JITX86_64 : public JITCompiler {
public:
    JITX86_64() noexcept;
    explicit JITX86_64(JITCompileOptions options) noexcept;
    ~JITX86_64() noexcept override;
    
    /**
     * @brief IR関数をコンパイルし、実行可能コードを生成する
     * 
     * @param function コンパイル対象のIR関数
     * @param functionId 関数ID（プロファイリング用）
     * @return 実行可能コードのポインタ
     */
    void* Compile(const IRFunction& function, uint32_t functionId) noexcept override;
    
    /**
     * @brief 実行可能コードを解放する
     * 
     * @param codePtr 解放対象のコードポインタ
     */
    void ReleaseCode(void* codePtr) noexcept override;
    
    /**
     * @brief 最適化レベルを設定する
     * 
     * @param level 最適化レベル
     */
    void SetOptimizationLevel(OptimizationLevel level) noexcept override;
    
    /**
     * @brief デバッグ情報を設定する
     * 
     * @param enable デバッグ情報有効フラグ
     */
    void EnableDebugInfo(bool enable) noexcept override;
    
    /**
     * @brief コンパイルされた関数に関するデバッグ情報を取得する
     * 
     * @param codePtr コードポインタ
     * @return デバッグ情報の文字列
     */
    std::string GetDebugInfo(void* codePtr) const noexcept override;
    
    /**
     * @brief 内部状態をリセットする
     */
    void Reset() noexcept override;
    
    /**
     * @brief コンパイルオプションを設定する
     * 
     * @param options コンパイルオプション
     */
    void SetCompileOptions(const JITCompileOptions& options) noexcept;
    
    /**
     * @brief 現在のコンパイルオプションを取得する
     * 
     * @return 現在のコンパイルオプション
     */
    const JITCompileOptions& GetCompileOptions() const noexcept;
    
    /**
     * @brief CPU機能を検出する
     * 
     * @return 検出された機能セット
     */
    static CPUFeatureSet DetectCPUFeatures() noexcept;
    
    /**
     * @brief 特定のCPU機能がサポートされているか確認する
     * 
     * @param feature 確認する機能
     * @return サポートされているかどうか
     */
    static bool IsCPUFeatureSupported(CPUFeature feature) noexcept;
    
    /**
     * @brief コード生成に特定のCPU機能を使用する
     * 
     * @param feature 使用する機能
     * @param enable 有効にするかどうか
     */
    void UseCPUFeature(CPUFeature feature, bool enable) noexcept;
    
    /**
     * @brief コードのパフォーマンスカウンタを取得する
     * 
     * @param codePtr コードポインタ
     * @return パフォーマンスカウンタのマップ
     */
    std::unordered_map<std::string, uint64_t> GetPerformanceCounters(void* codePtr) const noexcept;
    
    /**
     * @brief バイトコードをコンパイルする
     * 
     * @param bytecodes バイトコード
     * @param outCodeSize 出力コードサイズ
     * @return コンパイル結果のバイト配列
     */
    [[nodiscard]] std::unique_ptr<uint8_t[]> Compile(const std::vector<uint8_t>& bytecodes,
                                                    size_t& outCodeSize) noexcept override;

private:
    // コード生成器
    X86_64CodeGenerator m_codeGenerator;
    
    // 最適化レベル
    OptimizationLevel m_optimizationLevel;
    
    // デバッグ情報有効フラグ
    bool m_debugInfoEnabled;
    
    // コンパイルオプション
    JITCompileOptions m_compileOptions;
    
    // CPU機能セット
    CPUFeatureSet m_enabledFeatures;
    
    // コードキャッシュ
    struct CachedCode {
        void* codePtr;
        size_t codeSize;
        uint32_t crc32;
        uint32_t functionId;
        uint64_t timestamp;
        uint32_t useCount;
        CodeMetadata metadata;
    };
    std::unordered_map<uint32_t, CachedCode> m_codeCache;
    
    // コードメモリプール
    struct CodeMemoryPool {
        void* baseAddress;
        size_t totalSize;
        size_t usedSize;
        std::vector<std::pair<void*, size_t>> freeBlocks;
    };
    CodeMemoryPool m_memoryPool;
    std::mutex m_memoryPoolMutex;
    
    // コンパイルされた関数のデバッグ情報
    struct DebugInfo {
        std::string functionName;
        std::vector<std::string> instructions;
        CodeMetadata metadata;
        std::unordered_map<std::string, int64_t> stats;
    };
    
    // コードポインタとデバッグ情報のマッピング
    std::unordered_map<void*, DebugInfo> m_debugInfoMap;
    
    // 各種内部カウンタ
    struct Stats {
        std::atomic<uint64_t> compiledFunctions{0};
        std::atomic<uint64_t> totalCodeSize{0};
        std::atomic<uint64_t> cacheHits{0};
        std::atomic<uint64_t> cacheMisses{0};
        std::atomic<uint64_t> totalCompilationTimeNs{0};
        std::atomic<uint64_t> totalRegAllocTimeNs{0};
        std::atomic<uint64_t> totalCodeGenTimeNs{0};
        std::atomic<uint64_t> totalOptimizationTimeNs{0};
    };
    Stats m_stats;
    
    // レジスタアロケーション処理
    void AllocateRegisters(const IRFunction& function, std::vector<uint8_t>& nativeCode) noexcept;
    
    // プロローグとエピローグの追加
    void AddPrologueAndEpilogue(std::vector<uint8_t>& nativeCode) noexcept;
    
    // 実行可能メモリの確保
    void* AllocateExecutableMemory(const std::vector<uint8_t>& code, size_t alignment = 16) noexcept;
    
    // 実行可能メモリの解放
    void FreeExecutableMemory(void* ptr, size_t size) noexcept;
    
    // バイトコードをデバッグ用に逆アセンブルする
    std::vector<std::string> DisassembleCode(const std::vector<uint8_t>& code) const noexcept;
    
    // コードキャッシュ管理
    void* GetCachedCode(uint32_t functionId, uint32_t crc32) noexcept;
    void AddToCache(uint32_t functionId, void* codePtr, size_t codeSize, uint32_t crc32, CodeMetadata metadata) noexcept;
    void CleanupCache() noexcept;
    
    // IR最適化パイプラインの実行
    std::unique_ptr<IRFunction> OptimizeIR(const IRFunction& function, uint32_t functionId) noexcept;
    
    // アーキテクチャ固有の最適化を適用
    void ApplyArchSpecificOptimizations(std::vector<uint8_t>& code) noexcept;
    
    // コード配置の最適化
    void OptimizeCodeLayout(std::vector<uint8_t>& code, const CodeMetadata& metadata) noexcept;
    
    // ホットパス検出
    std::vector<std::pair<size_t, size_t>> DetectHotPaths(const IRFunction& function) noexcept;
    
    // メモリプールの初期化
    bool InitMemoryPool(size_t initialSize = 4 * 1024 * 1024) noexcept;
    
    // メモリプールからメモリを確保
    void* AllocateFromPool(size_t size, size_t alignment) noexcept;
    
    // メモリプールにメモリを返却
    void ReturnToPool(void* ptr, size_t size) noexcept;
    
    // ガードページの設定（セキュリティ強化用）
    void SetupGuardPages(void* codePtr, size_t codeSize) noexcept;
    
    // マイクロアーキテクチャ固有の最適化適用
    void ApplyMicroarchOptimizations(std::vector<uint8_t>& code, CPUFeatureSet features) noexcept;
    
    // SIMD最適化が適用可能かどうかの判定
    bool CanApplySIMD(const IRFunction& function) noexcept;
    
    // SIMD命令テンプレートの生成
    std::vector<uint8_t> GenerateSIMDTemplate(
        const std::string& operation, 
        X86_64FloatRegister dest, 
        X86_64FloatRegister src1, 
        X86_64FloatRegister src2) noexcept;
    
    // CRC32計算
    uint32_t CalculateCRC32(const IRFunction& function) const noexcept;
    
    // プロファイリング情報に基づく最適化
    void ApplyProfileGuidedOptimizations(
        std::vector<uint8_t>& code, 
        const IRFunction& function, 
        uint32_t functionId) noexcept;
};

} // namespace core
} // namespace aerojs 