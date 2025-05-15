/**
 * @file arm64_backend.h
 * @brief AeroJS JavaScriptエンジン用ARM64バックエンド
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include "arm64_assembler.h"
#include "arm64_code_gen.h"
#include "arm64_jit_compiler.h"
#include "core/jit/backend/backend.h"

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class JITCompiler;
class JITProfiler;

namespace arm64 {

/**
 * @brief ARM64アーキテクチャ向けバックエンド実装
 *
 * JavaScriptエンジンのARM64アーキテクチャ向けバックエンドを提供します。
 * アセンブラ、コードジェネレータ、JITコンパイラを含め、ARM64固有の処理を実装します。
 */
class ARM64Backend : public Backend {
public:
    /**
     * @brief ARM64バックエンドを作成します
     * @param context JavaScriptコンテキスト
     * @param profiler JITプロファイラ（オプション）
     */
    ARM64Backend(Context* context, JITProfiler* profiler = nullptr);
    
    /**
     * @brief デストラクタ
     */
    virtual ~ARM64Backend();
    
    /**
     * @brief バックエンドの初期化
     * @return 初期化が成功したかどうか
     */
    virtual bool initialize() override;
    
    /**
     * @brief 使用しているアーキテクチャの名前を取得
     * @return アーキテクチャ名
     */
    virtual const char* getArchName() const override;
    
    /**
     * @brief JITコンパイラインスタンスを取得
     * @return JITコンパイラのインスタンス
     */
    virtual JITCompiler* getJITCompiler() override;
    
    /**
     * @brief バックエンドがサポートする機能を確認
     * @param feature 確認する機能
     * @return 機能がサポートされているかどうか
     */
    virtual bool supportsFeature(BackendFeature feature) const override;
    
    /**
     * @brief 実行中のCPUがARM64の拡張機能をサポートしているか確認
     * @param extension 確認する拡張機能
     * @return 拡張機能がサポートされているかどうか
     */
    bool supportsExtension(ARM64Extension extension) const;
    
    /**
     * @brief バックエンドが最適に動作するための設定を適用
     * @param context 設定を適用するコンテキスト
     */
    virtual void applyOptimalSettings(Context* context) override;
    
    /**
     * @brief パフォーマンスカウンタを取得
     * @return バックエンドの現在のパフォーマンスカウンタ
     */
    virtual const BackendPerfCounters& getPerfCounters() const override;
    
    /**
     * @brief パフォーマンスカウンタをリセット
     */
    virtual void resetPerfCounters() override;
    
    /**
     * @brief アセンブラインスタンスを取得
     * @return ARM64アセンブラのインスタンス
     */
    ARM64Assembler* getAssembler() { return _assembler.get(); }
    
    /**
     * @brief コードジェネレータインスタンスを取得
     * @return ARM64コードジェネレータのインスタンス
     */
    ARM64CodeGenerator* getCodeGenerator() { return _codeGenerator.get(); }
    
private:
    // CPUの機能を検出
    void detectCPUFeatures();
    
    // JITスタブの初期化
    void initializeJITStubs();
    
    Context* _context;
    JITProfiler* _profiler;
    std::unique_ptr<ARM64Assembler> _assembler;
    std::unique_ptr<ARM64CodeGenerator> _codeGenerator;
    std::unique_ptr<ARM64JITCompiler> _jitCompiler;
    
    // CPU機能フラグ
    struct CPUFeatures {
        bool hasSIMD;        // SIMD命令セット
        bool hasCrypto;      // 暗号拡張
        bool hasCRC32;       // CRC32命令
        bool hasAtomics;     // アトミック操作
        bool hasDotProduct;  // ドット積命令
        bool hasFP16;        // 半精度浮動小数点
        bool hasBF16;        // Brain浮動小数点
        bool hasJSCVT;       // JS変換命令
        bool hasLSE;         // LSE (Large System Extensions)
        bool hasSVE;         // SVE (Scalable Vector Extensions)
        
        CPUFeatures()
            : hasSIMD(false), hasCrypto(false), hasCRC32(false),
              hasAtomics(false), hasDotProduct(false), hasFP16(false),
              hasBF16(false), hasJSCVT(false), hasLSE(false), hasSVE(false) {}
    };
    
    CPUFeatures _cpuFeatures;
    BackendPerfCounters _perfCounters;
};

// ARM64拡張機能の列挙型
enum class ARM64Extension {
    SIMD,        // SIMD命令セット
    Crypto,      // 暗号拡張
    CRC32,       // CRC32命令
    Atomics,     // アトミック操作
    DotProduct,  // ドット積命令
    FP16,        // 半精度浮動小数点
    BF16,        // Brain浮動小数点
    JSCVT,       // JS変換命令
    LSE,         // LSE (Large System Extensions)
    SVE          // SVE (Scalable Vector Extensions)
};

} // namespace arm64
} // namespace core
} // namespace aerojs 