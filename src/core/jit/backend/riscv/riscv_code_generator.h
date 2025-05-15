#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <bitset>
#include <functional>
#include <array>

#include "../../ir/ir.h"

namespace aerojs {
namespace core {

// RISC-V ISA拡張フラグ
enum class RiscVFeature : uint32_t {
    I           = 1 << 0,   // 基本整数命令セット (必須)
    M           = 1 << 1,   // 整数乗除算
    A           = 1 << 2,   // アトミック命令
    F           = 1 << 3,   // 単精度浮動小数点
    D           = 1 << 4,   // 倍精度浮動小数点
    G           = 1 << 5,   // IMAFD（汎用ISA）
    C           = 1 << 6,   // 圧縮命令
    B           = 1 << 7,   // ビット操作拡張
    V           = 1 << 8,   // ベクトル拡張
    P           = 1 << 9,   // パックド-SIMD拡張
    J           = 1 << 10,  // 動的言語向け拡張
    Zicbom      = 1 << 11,  // キャッシュブロック操作
    Zicsr       = 1 << 12,  // 制御・状態レジスタ
    Zifencei    = 1 << 13,  // 命令フェンス
    Zihintpause = 1 << 14,  // ポーズヒント
    Zba         = 1 << 15,  // アドレス操作拡張
    Zbb         = 1 << 16,  // 基本ビット操作
    Zbc         = 1 << 17,  // キャリーレス演算
    Zbs         = 1 << 18,  // シングルビット操作
    K           = 1 << 19,  // 暗号化拡張
    H           = 1 << 20,  // ハイパーバイザ拡張
    N           = 1 << 21,  // ユーザーレベル割り込み
    S           = 1 << 22,  // スーパーバイザモード
    Zfh         = 1 << 23,  // 半精度浮動小数点
    Zve32x      = 1 << 24,  // ベクトル拡張（整数のみ）
    Zve64x      = 1 << 25,  // ベクトル拡張（64ビット整数）
    Zvl128b     = 1 << 26,  // 最小ベクトル長128ビット
    Zvl256b     = 1 << 27,  // 最小ベクトル長256ビット
    Zvl512b     = 1 << 28,  // 最小ベクトル長512ビット
    Zfinx       = 1 << 29   // 整数レジスタ内浮動小数点
};
using RiscVFeatureSet = std::bitset<32>;

// コード生成のオプション
enum class CodeGenOptFlags : uint32_t {
    None              = 0,
    PeepholeOptimize  = 1 << 0,   // 命令単位の最適化
    AlignLoops        = 1 << 1,   // ループアライメント
    OptimizeJumps     = 1 << 2,   // ジャンプ最適化
    CacheAware        = 1 << 3,   // キャッシュを意識したコード配置
    VectorizeLoops    = 1 << 4,   // ループベクトル化
    UnrollLoops       = 1 << 5,   // ループアンローリング
    ScheduleInsts     = 1 << 6,   // 命令スケジューリング
    FullPowerOpt      = 0xFFFFFFFF // すべての最適化
};
inline CodeGenOptFlags operator|(CodeGenOptFlags a, CodeGenOptFlags b) {
    return static_cast<CodeGenOptFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline CodeGenOptFlags operator&(CodeGenOptFlags a, CodeGenOptFlags b) {
    return static_cast<CodeGenOptFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

// RISC-Vレジスタ
enum class RiscVRegister : uint8_t {
    zero = 0,  // ハードワイヤードゼロ
    ra   = 1,  // リターンアドレス
    sp   = 2,  // スタックポインタ
    gp   = 3,  // グローバルポインタ
    tp   = 4,  // スレッドポインタ
    t0   = 5,  // 一時レジスタ 0
    t1   = 6,  // 一時レジスタ 1
    t2   = 7,  // 一時レジスタ 2
    s0   = 8,  // 保存レジスタ 0 / フレームポインタ
    fp   = 8,  // フレームポインタ (s0の別名)
    s1   = 9,  // 保存レジスタ 1
    a0   = 10, // 引数/戻り値 0
    a1   = 11, // 引数/戻り値 1
    a2   = 12, // 引数 2
    a3   = 13, // 引数 3
    a4   = 14, // 引数 4
    a5   = 15, // 引数 5
    a6   = 16, // 引数 6
    a7   = 17, // 引数 7
    s2   = 18, // 保存レジスタ 2
    s3   = 19, // 保存レジスタ 3
    s4   = 20, // 保存レジスタ 4
    s5   = 21, // 保存レジスタ 5
    s6   = 22, // 保存レジスタ 6
    s7   = 23, // 保存レジスタ 7
    s8   = 24, // 保存レジスタ 8
    s9   = 25, // 保存レジスタ 9
    s10  = 26, // 保存レジスタ 10
    s11  = 27, // 保存レジスタ 11
    t3   = 28, // 一時レジスタ 3
    t4   = 29, // 一時レジスタ 4
    t5   = 30, // 一時レジスタ 5
    t6   = 31  // 一時レジスタ 6
};

// 浮動小数点レジスタ
enum class RiscVFPRegister : uint8_t {
    f0  = 0,   // FP一時レジスタ 0 / 戻り値
    f1  = 1,   // FP一時レジスタ 1 / 戻り値
    f2  = 2,   // FP一時レジスタ 2
    f3  = 3,   // FP一時レジスタ 3
    f4  = 4,   // FP一時レジスタ 4
    f5  = 5,   // FP一時レジスタ 5
    f6  = 6,   // FP一時レジスタ 6
    f7  = 7,   // FP一時レジスタ 7
    f8  = 8,   // FP保存レジスタ 0
    f9  = 9,   // FP保存レジスタ 1
    f10 = 10,  // FP引数 0
    f11 = 11,  // FP引数 1
    f12 = 12,  // FP引数 2
    f13 = 13,  // FP引数 3
    f14 = 14,  // FP引数 4
    f15 = 15,  // FP引数 5
    f16 = 16,  // FP引数 6
    f17 = 17,  // FP引数 7
    f18 = 18,  // FP保存レジスタ 2
    f19 = 19,  // FP保存レジスタ 3
    f20 = 20,  // FP保存レジスタ 4
    f21 = 21,  // FP保存レジスタ 5
    f22 = 22,  // FP保存レジスタ 6
    f23 = 23,  // FP保存レジスタ 7
    f24 = 24,  // FP保存レジスタ 8
    f25 = 25,  // FP保存レジスタ 9
    f26 = 26,  // FP保存レジスタ 10
    f27 = 27,  // FP保存レジスタ 11
    f28 = 28,  // FP一時レジスタ 8
    f29 = 29,  // FP一時レジスタ 9
    f30 = 30,  // FP一時レジスタ 10
    f31 = 31   // FP一時レジスタ 11
};

// コード生成の最適化オプション
struct RiscVJITCompileOptions {
    CodeGenOptFlags codeGenFlags = CodeGenOptFlags::PeepholeOptimize | 
                                   CodeGenOptFlags::AlignLoops | 
                                   CodeGenOptFlags::OptimizeJumps;
    bool enableSIMD = false;         // ベクトル命令を使用
    bool enableFastMath = true;      // 高速数学関数
    bool enableMicroarchOpt = true;  // マイクロアーキテクチャ最適化
    bool enableLoopUnrolling = true; // ループアンローリング
    uint8_t loopUnrollFactor = 4;    // アンローリング係数
    bool enableGCStackMapGen = true; // GCスタックマップ生成
    bool enableVectorization = false; // ベクトル化
    bool enableCompressedInsts = true; // 圧縮命令の使用
};

/**
 * @brief RISC-Vアーキテクチャ向けコードジェネレータ
 * 
 * IRからRISC-Vマシンコードを生成するクラス。
 * ベクトル拡張やビット操作拡張などの最新機能に対応。
 */
class RiscVCodeGenerator {
public:
    RiscVCodeGenerator();
    explicit RiscVCodeGenerator(RiscVJITCompileOptions options);
    ~RiscVCodeGenerator();
    
    /**
     * @brief コード生成を行う
     * 
     * @param function IR関数
     * @param output 出力コードバッファ
     * @return 生成が成功したかどうか
     */
    bool Generate(const IRFunction& function, std::vector<uint8_t>& output);
    
    /**
     * @brief オプションを設定する
     * 
     * @param options 最適化オプション
     */
    void SetOptions(const RiscVJITCompileOptions& options);
    
    /**
     * @brief 使用するISA拡張機能を設定する
     * 
     * @param feature 機能フラグ
     * @param enable 有効/無効
     */
    void SetFeature(RiscVFeature feature, bool enable);
    
    /**
     * @brief 現在のオプションを取得する
     */
    const RiscVJITCompileOptions& GetOptions() const;
    
    /**
     * @brief サポートされる機能を検出する
     */
    static RiscVFeatureSet DetectSupportedFeatures();
    
    /**
     * @brief 機能がサポートされているか確認する
     * 
     * @param feature 確認する機能
     */
    bool IsFeatureSupported(RiscVFeature feature) const;
    
private:
    // コード生成オプション
    RiscVJITCompileOptions m_options;
    
    // 有効なISA拡張
    RiscVFeatureSet m_enabledFeatures;
    
    // サポートされる機能
    RiscVFeatureSet m_supportedFeatures;
    
    // IR仮想レジスタからネイティブレジスタへのマッピング
    std::unordered_map<int32_t, RiscVRegister> m_registerMapping;
    std::unordered_map<int32_t, RiscVFPRegister> m_fpRegisterMapping;
    
    // レジスタ割り当てを行う
    void AllocateRegisters(const IRFunction& function);
    
    // 仮想レジスタをネイティブレジスタにマップする
    RiscVRegister MapRegister(int32_t virtualReg);
    RiscVFPRegister MapFPRegister(int32_t virtualReg);
    
    // 各種命令の生成メソッド
    void EmitProlog(std::vector<uint8_t>& code);
    void EmitEpilog(std::vector<uint8_t>& code);
    void EmitArithmeticInst(const IRInstruction& inst, std::vector<uint8_t>& code);
    void EmitLoadConstInst(const IRInstruction& inst, std::vector<uint8_t>& code);
    void EmitCompareInst(const IRInstruction& inst, std::vector<uint8_t>& code);
    void EmitJumpInst(const IRInstruction& inst, std::vector<uint8_t>& code);
    void EmitCallInst(const IRInstruction& inst, std::vector<uint8_t>& code);
    void EmitReturnInst(const IRInstruction& inst, std::vector<uint8_t>& code);
    
    // ベクトル命令の生成（V拡張）
    void EmitVectorInst(const IRInstruction& inst, std::vector<uint8_t>& code);
    
    // ビット操作命令の生成（B拡張）
    void EmitBitManipInst(const IRInstruction& inst, std::vector<uint8_t>& code);
    
    // 命令エンコード用ヘルパーメソッド
    uint32_t EncodeRTypeInst(uint32_t opcode, uint32_t rd, uint32_t rs1, uint32_t rs2, uint32_t funct3, uint32_t funct7);
    uint32_t EncodeITypeInst(uint32_t opcode, uint32_t rd, uint32_t rs1, uint32_t imm, uint32_t funct3);
    uint32_t EncodeSTypeInst(uint32_t opcode, uint32_t rs1, uint32_t rs2, uint32_t imm, uint32_t funct3);
    uint32_t EncodeBTypeInst(uint32_t opcode, uint32_t rs1, uint32_t rs2, uint32_t imm, uint32_t funct3);
    uint32_t EncodeUTypeInst(uint32_t opcode, uint32_t rd, uint32_t imm);
    uint32_t EncodeJTypeInst(uint32_t opcode, uint32_t rd, uint32_t imm);
    
    // 命令最適化
    void OptimizeCode(std::vector<uint8_t>& code);
};

} // namespace core
} // namespace aerojs