/**
 * @file x86_64_code_generator.h
 * @brief AeroJS JavaScript エンジンのx86_64コード生成ユーティリティ
 * @version 1.0.0
 * @license MIT
 */

#ifndef AEROJS_CORE_JIT_BACKEND_X86_64_CODE_GENERATOR_H
#define AEROJS_CORE_JIT_BACKEND_X86_64_CODE_GENERATOR_H

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include "x86_64_registers.h"
#include <optional>
#include "../../ir/ir.h"

namespace aerojs {
namespace core {
namespace jit {

enum class BranchHint {
  kNone,
  kLikelyTaken,
  kLikelyNotTaken
};

/**
 * @brief 型安全な最適化フラグ
 */
enum class CodeGenOptFlags : uint32_t {
  None = 0,
  UseAVX = 1 << 0,        // AVX命令を使用
  UseFMA = 1 << 1,        // FMA命令を使用
  UseAVX512 = 1 << 2,     // AVX-512命令を使用
  UseBMI = 1 << 3,        // BMI命令を使用
  UseCLMUL = 1 << 4,      // CLMUL命令を使用
  UseLZCNT = 1 << 5,      // LZCNT命令を使用
  UsePOPCNT = 1 << 6,     // POPCNT命令を使用
  CacheAware = 1 << 7,    // キャッシュラインを意識した最適化
  BranchHints = 1 << 8,   // 分岐予測ヒントを使用
  UseCPUSpecific = 1 << 9, // CPU固有の最適化を適用
  PeepholeOptimize = 1 << 10, // 覗き穴最適化
  AlignLoops = 1 << 11,     // ループのアライメント
  OptimizeJumps = 1 << 12   // ジャンプ最適化
};

// constexpr/inline化
constexpr inline bool HasFlag(uint32_t flags, CodeGenOptFlags flag) noexcept {
  return (flags & static_cast<uint32_t>(flag)) != 0;
}

/**
 * @brief コードの位置情報を保持する構造体
 */
struct CodeLocation {
  uint32_t offset;     // コード内のオフセット
  uint32_t size;       // 命令のサイズ
  std::string label;   // 関連するラベル（存在する場合）
  uint32_t line;       // ソースコードの行番号（デバッグ用）
  
  CodeLocation() : offset(0), size(0), line(0) {}
  CodeLocation(uint32_t offset, uint32_t size, const std::string& label = "", uint32_t line = 0)
    : offset(offset), size(size), label(label), line(line) {}
};

/**
 * @brief 未解決の参照を表す構造体
 */
struct PendingJump {
  uint32_t sourceOffset;     // ジャンプ命令の位置
  uint32_t jumpOpSize;       // ジャンプ命令のサイズ
  std::string targetLabel;   // ジャンプ先のラベル
  bool isShortJump;          // 短いジャンプかどうか
  
  PendingJump(uint32_t offset, uint32_t size, const std::string& label, bool isShort = false)
    : sourceOffset(offset), jumpOpSize(size), targetLabel(label), isShortJump(isShort) {}
};

class Context; // 前方宣言
class JITProfiler; // 前方宣言

/**
 * @brief x86_64向けマシンコードを生成するクラス
 */
class X86_64CodeGenerator {
public:
  /**
   * @brief コンストラクタ
   * @param context 実行コンテキスト (プロファイラ取得などに使用)
   * @param optimizationFlags 最適化フラグ
   */
  X86_64CodeGenerator(Context* context, uint32_t optimizationFlags = 0) noexcept;
  
  /**
   * @brief デストラクタ
   */
  ~X86_64CodeGenerator() noexcept;
  
  /**
   * @brief 生成されたマシンコードを取得
   * @return マシンコードのバイト列
   */
  const std::vector<uint8_t>& GetCode() const;
  
  /**
   * @brief 生成されたマシンコードのサイズを取得
   * @return マシンコードのサイズ（バイト単位）
   */
  size_t GetCodeSize() const;
  
  /**
   * @brief 現在のコード位置を取得
   * @return 現在のコード位置（バイト単位）
   */
  size_t GetCurrentPosition() const;
  
  /**
   * @brief コードバッファをクリア
   */
  void Clear();
  
  /**
   * @brief フレーム情報をリセット (新しい関数のコンパイル開始時)
   */
  void ResetFrameInfo();
  
  /**
   * @brief 仮想レジスタにスピルスロットを割り当て、そのオフセットを返す
   * @param virtualReg スピルする仮想レジスタID
   * @param size スピルするデータのサイズ (バイト単位、通常は8バイト)
   * @return スタックフレームベースからのオフセット
   */
  int32_t AllocateSpillSlot(int32_t virtualReg, uint32_t size = 8);
  
  /**
   * @brief 仮想レジスタのスピルスロットオフセットを取得
   * @param virtualReg 仮想レジスタID
   * @return オフセット (見つからない場合は std::nullopt)
   */
  std::optional<int32_t> GetSpillSlotOffset(int32_t virtualReg) const;
  
  /**
   * @brief 現在のスタックフレームサイズを取得
   * @return スタックフレームサイズ (バイト単位)
   */
  uint32_t GetCurrentFrameSize() const;
  
  /**
   * @brief 特定のバイトパターンでバッファを埋める
   * @param size 埋めるサイズ
   * @param value 埋める値
   */
  void Fill(size_t size, uint8_t value = 0x90 /* NOP */);
  
  /**
   * @brief コードバッファのアライメントを調整
   * @param alignment アライメント単位（バイト）
   * @param fillValue 埋める値（デフォルトはNOP）
   */
  void Align(size_t alignment, uint8_t fillValue = 0x90 /* NOP */) noexcept;
  
  /**
   * @brief ラベルを定義
   * @param label ラベル名
   */
  void DefineLabel(const std::string& label) noexcept;
  
  /**
   * @brief ラベルの位置を取得
   * @param label ラベル名
   * @return ラベルの位置（見つからない場合は-1）
   */
  int32_t GetLabelPosition(const std::string& label) const;
  
  /**
   * @brief すべてのラベル参照を解決
   * @return 解決に成功した場合はtrue
   */
  bool ResolveLabels() noexcept;
  
  //----------------------------------------------------------------------
  // マシンコード生成メソッド
  //----------------------------------------------------------------------
  
  // データエミット関数
  void EmitByte(uint8_t value);
  void EmitWord(uint16_t value);
  void EmitDWord(uint32_t value);
  void EmitQWord(uint64_t value);
  void EmitBytes(const std::vector<uint8_t>& bytes);
  
  // REX接頭辞
  uint8_t EncodeREX(bool w, bool r, bool x, bool b);
  void EmitREX(bool w, bool r, bool x, bool b);
  
  // ModR/Mエンコーディング
  uint8_t EncodeMod(uint8_t mod, uint8_t reg, uint8_t rm);
  void EmitModRM(uint8_t mod, uint8_t reg, uint8_t rm);
  
  // SIBエンコーディング
  uint8_t EncodeSIB(uint8_t scale, uint8_t index, uint8_t base);
  void EmitSIB(uint8_t scale, uint8_t index, uint8_t base);
  
  // 相対アドレス参照用の変位
  void EmitDisplacement(int32_t disp, uint8_t dispSize);
  
  // 命令エンコーディング - 移動命令
  void EmitMOV_Reg_Reg(X86_64Register dest, X86_64Register src, uint8_t size = 8);
  void EmitMOV_Reg_Imm(X86_64Register dest, int64_t imm, uint8_t size = 8);
  void EmitMOV_Reg_Mem(X86_64Register dest, X86_64Register base, int32_t disp = 0, uint8_t size = 8);
  void EmitMOV_Mem_Reg(X86_64Register base, int32_t disp, X86_64Register src, uint8_t size = 8);
  
  // 命令エンコーディング - 算術演算命令
  void EmitADD_Reg_Reg(X86_64Register dest, X86_64Register src, uint8_t size = 8);
  void EmitADD_Reg_Imm(X86_64Register dest, int32_t imm, uint8_t size = 8);
  void EmitSUB_Reg_Reg(X86_64Register dest, X86_64Register src, uint8_t size = 8);
  void EmitSUB_Reg_Imm(X86_64Register dest, int32_t imm, uint8_t size = 8);
  void EmitINC_Reg(X86_64Register reg, uint8_t size = 8);
  void EmitDEC_Reg(X86_64Register reg, uint8_t size = 8);
  void EmitIMUL_Reg_Reg(X86_64Register dest, X86_64Register src, uint8_t size = 8);
  void EmitIMUL_Reg_Imm(X86_64Register dest, int32_t imm, uint8_t size = 8);
  
  // 命令エンコーディング - 論理演算命令
  void EmitAND_Reg_Reg(X86_64Register dest, X86_64Register src, uint8_t size = 8);
  void EmitAND_Reg_Imm(X86_64Register dest, int32_t imm, uint8_t size = 8);
  void EmitOR_Reg_Reg(X86_64Register dest, X86_64Register src, uint8_t size = 8);
  void EmitOR_Reg_Imm(X86_64Register dest, int32_t imm, uint8_t size = 8);
  void EmitXOR_Reg_Reg(X86_64Register dest, X86_64Register src, uint8_t size = 8);
  void EmitXOR_Reg_Imm(X86_64Register dest, int32_t imm, uint8_t size = 8);
  void EmitNOT_Reg(X86_64Register reg, uint8_t size = 8);
  
  // 命令エンコーディング - シフト命令
  void EmitSHL_Reg_Imm(X86_64Register reg, uint8_t imm, uint8_t size = 8);
  void EmitSHL_Reg_CL(X86_64Register reg, uint8_t size = 8);
  void EmitSHR_Reg_Imm(X86_64Register reg, uint8_t imm, uint8_t size = 8);
  void EmitSHR_Reg_CL(X86_64Register reg, uint8_t size = 8);
  void EmitSAR_Reg_Imm(X86_64Register reg, uint8_t imm, uint8_t size = 8);
  void EmitSAR_Reg_CL(X86_64Register reg, uint8_t size = 8);
  
  // 命令エンコーディング - 比較命令
  void EmitCMP_Reg_Reg(X86_64Register left, X86_64Register right, uint8_t size = 8);
  void EmitCMP_Reg_Imm(X86_64Register reg, int32_t imm, uint8_t size = 8);
  void EmitTEST_Reg_Reg(X86_64Register left, X86_64Register right, uint8_t size = 8);
  void EmitTEST_Reg_Imm(X86_64Register reg, int32_t imm, uint8_t size = 8);
  
  // 命令エンコーディング - ジャンプ命令
  void EmitJMP_Label(const std::string& label, bool isShort = false);
  void EmitJMP_Reg(X86_64Register reg);
  void EmitJE_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);   // = / Zero
  void EmitJNE_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);  // != / Not Zero
  void EmitJG_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);   // >
  void EmitJGE_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);  // >=
  void EmitJL_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);   // <
  void EmitJLE_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);  // <=
  void EmitJO_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);   // Overflow
  void EmitJNO_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);  // Not Overflow
  void EmitJS_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);   // Sign (Negative)
  void EmitJNS_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);  // Not Sign (Positive)
  void EmitJP_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);   // Parity (Even)
  void EmitJNP_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);  // Not Parity (Odd)
  void EmitJB_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);   // Below (Unsigned <)
  void EmitJBE_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);  // Below or Equal (Unsigned <=)
  void EmitJA_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);   // Above (Unsigned >)
  void EmitJAE_Label(const std::string& label, BranchHint hint = BranchHint::kNone, bool isShort = true);  // Above or Equal (Unsigned >=)
  
  // 命令エンコーディング - 関数呼び出し
  void EmitCALL_Label(const std::string& label);
  void EmitCALL_Reg(X86_64Register reg);
  void EmitRET();
  
  // 命令エンコーディング - スタック操作
  void EmitPUSH_Reg(X86_64Register reg);
  void EmitPOP_Reg(X86_64Register reg);
  
  // 命令エンコーディング - 浮動小数点演算
  void EmitMOVSS_Reg_Reg(X86_64XMMRegister dest, X86_64XMMRegister src);
  void EmitMOVSD_Reg_Reg(X86_64XMMRegister dest, X86_64XMMRegister src);
  void EmitADDSS_Reg_Reg(X86_64XMMRegister dest, X86_64XMMRegister src);
  void EmitADDSD_Reg_Reg(X86_64XMMRegister dest, X86_64XMMRegister src);
  void EmitSUBSS_Reg_Reg(X86_64XMMRegister dest, X86_64XMMRegister src);
  void EmitSUBSD_Reg_Reg(X86_64XMMRegister dest, X86_64XMMRegister src);
  void EmitMULSS_Reg_Reg(X86_64XMMRegister dest, X86_64XMMRegister src);
  void EmitMULSD_Reg_Reg(X86_64XMMRegister dest, X86_64XMMRegister src);
  void EmitDIVSS_Reg_Reg(X86_64XMMRegister dest, X86_64XMMRegister src);
  void EmitDIVSD_Reg_Reg(X86_64XMMRegister dest, X86_64XMMRegister src);
  
  // 命令エンコーディング - 変換命令
  void EmitCVTSI2SS_Reg_Reg(X86_64XMMRegister dest, X86_64Register src, uint8_t size = 8);
  void EmitCVTSI2SD_Reg_Reg(X86_64XMMRegister dest, X86_64Register src, uint8_t size = 8);
  void EmitCVTSS2SI_Reg_Reg(X86_64Register dest, X86_64XMMRegister src, uint8_t size = 8);
  void EmitCVTSD2SI_Reg_Reg(X86_64Register dest, X86_64XMMRegister src, uint8_t size = 8);
  void EmitCVTSS2SD_Reg_Reg(X86_64XMMRegister dest, X86_64XMMRegister src);
  void EmitCVTSD2SS_Reg_Reg(X86_64XMMRegister dest, X86_64XMMRegister src);
  
  // 命令エンコーディング - その他
  void EmitNOP(uint8_t size = 1);
  void EmitINT3();
  
  // VEXプレフィックスの追加ヘルパー
  void AppendVEXPrefix(uint8_t m, uint8_t p, uint8_t l, uint8_t w, uint8_t vvvv, uint8_t r, uint8_t x, uint8_t b) noexcept;
  
  // AVX-512用EVEXプレフィックスの追加ヘルパー
  void AppendEVEXPrefix(uint8_t evex_mm, uint8_t evex_pp, uint8_t evex_W, uint8_t evex_vvvv, uint8_t evex_z, uint8_t evex_LL, bool evex_b, uint8_t evex_aaa, bool R, bool X, bool B, bool R_prime, bool V_prime) noexcept;
  
  // SIBバイトの追加ヘルパー
  void AppendSIB(uint8_t scale, uint8_t index, uint8_t base) noexcept;
  
  // SIMDレジスタの取得
  std::optional<X86_64XMMRegister> GetSIMDReg(int32_t virtualReg) const noexcept;
  
  // AVX-512命令のエンコード (m_code を使用, 戻り値をboolに)
  bool EncodeAVX512Load(const IRInstruction& inst) noexcept;
  bool EncodeAVX512Store(const IRInstruction& inst) noexcept;
  bool EncodeAVX512Arithmetic(const IRInstruction& inst) noexcept;
  bool EncodeAVX512FMA(const IRInstruction& inst) noexcept;
  bool EncodeAVX512MaskOp(const IRInstruction& inst) noexcept;
  bool EncodeAVX512Blend(const IRInstruction& inst) noexcept;
  bool EncodeAVX512Permute(const IRInstruction& inst) noexcept;
  bool EncodeAVX512Compress(const IRInstruction& inst) noexcept;
  bool EncodeAVX512Expand(const IRInstruction& inst) noexcept;
  
  void OptimizeForCacheLine() noexcept; // Removed code param
  
  bool Generate(const IRFunction& function, std::vector<uint8_t>& outCode) noexcept;
  
  void SetRegisterMapping(int32_t virtualReg, X86_64Register physicalReg) noexcept;
  std::optional<X86_64Register> GetPhysicalReg(int32_t virtualReg) const noexcept;
  
  // Type checking encoders (m_code を使用, 戻り値をboolに)
  bool EncodeIsInteger(const IRInstruction& inst) noexcept;
  bool EncodeIsString(const IRInstruction& inst) noexcept;
  bool EncodeIsObject(const IRInstruction& inst) noexcept;
  bool EncodeIsNumber(const IRInstruction& inst) noexcept;
  bool EncodeIsBoolean(const IRInstruction& inst) noexcept;
  bool EncodeIsUndefined(const IRInstruction& inst) noexcept;
  bool EncodeIsNull(const IRInstruction& inst) noexcept;
  bool EncodeIsSymbol(const IRInstruction& inst) noexcept;
  bool EncodeIsFunction(const IRInstruction& inst) noexcept;
  bool EncodeIsArray(const IRInstruction& inst) noexcept;
  bool EncodeIsBigInt(const IRInstruction& inst) noexcept;
  
  // Memory operations (m_code を使用, 戻り値をboolに)
  bool EncodeLoadMemory(const IRInstruction& inst) noexcept;
  bool EncodeStoreMemory(const IRInstruction& inst) noexcept;

  // Conditional jump encoding (m_code を使用, 戻り値をboolに)
  bool EncodeConditionalJump(const IRInstruction& inst, aerojs::core::jit::ir::Condition condition, int32_t target_offset) noexcept;

  // Prologue/Epilogue (m_code を使用, voidのまま)
  void EncodePrologue() noexcept;
  void EncodeEpilogue() noexcept;

  // Spill slot access helpers (m_code を使用, 戻り値boolは維持)
  bool EncodeLoadFromSpillSlot(X86_64Register destReg, int32_t virtualReg) noexcept;
  bool EncodeStoreToSpillSlot(int32_t virtualReg, X86_64Register srcReg) noexcept;

  // IRInstruction ベースのエンコーダ (m_code を使用, 戻り値boolは維持)
  bool EncodeLoadConst(const IRInstruction& inst) noexcept;
  bool EncodeMove(const IRInstruction& inst) noexcept;
  bool EncodeAdd(const IRInstruction& inst) noexcept;
  bool EncodeSub(const IRInstruction& inst) noexcept;
  bool EncodeMul(const IRInstruction& inst) noexcept;
  bool EncodeDiv(const IRInstruction& inst) noexcept;
  bool EncodeMod(const IRInstruction& inst) noexcept;
  bool EncodeNeg(const IRInstruction& inst) noexcept;
  bool EncodeCompare(const IRInstruction& inst) noexcept;
  bool EncodeJump(const IRInstruction& inst) noexcept;
  bool EncodeBranch(const IRInstruction& inst, BranchHint hint) noexcept;
  bool EncodeCall(const IRInstruction& inst) noexcept;
  bool EncodeReturn(const IRInstruction& inst) noexcept;
  bool EncodeShift(const IRInstruction& inst) noexcept;
  bool EncodeInc(const IRInstruction& inst) noexcept;
  bool EncodeDec(const IRInstruction& inst) noexcept;
  bool EncodeNop() noexcept;
  // bool EncodeLogical(const IRInstruction& inst) noexcept; // Example for further changes
  // bool EncodeBitwise(const IRInstruction& inst) noexcept; // Example for further changes

  // 命令エンコーディング ヘルパー (これらも m_code を使用する)
  void AppendImmediate32(int32_t value) noexcept;  // Removed code param
  void AppendImmediate64(int64_t value) noexcept;  // Removed code param
  void AppendREXPrefix(bool w, bool r, bool x, bool b) noexcept; // Removed code param
  void AppendModRM(uint8_t mod, uint8_t reg, uint8_t rm) noexcept; // Removed code param

private:
  void AutoDetectOptimalFlags() noexcept;
  bool DetectCPUFeature(const std::string& feature) noexcept;

  // 生成されたコード
  std::vector<uint8_t> m_code;
  
  // ラベル位置のマップ
  std::unordered_map<std::string, uint32_t> m_labels;
  
  // 未解決のジャンプ
  std::vector<PendingJump> m_pendingJumps;
  
  // コード位置情報
  std::vector<CodeLocation> m_codeLocations;
  
  // ヘルパーメソッド
  uint8_t GetModRMSIBDisp(X86_64Register rm, int32_t disp, 
                          bool* needSIB, uint8_t* dispSize);
  
  // SIMD処理のためのヘルパーメソッド
  void EmitSIMDVectorOperation(IRNode* node);
  
  // AVX命令のエミッター（単精度）
  void EmitAVXAddPS(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVXSubPS(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVXMulPS(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVXDivPS(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVXMinPS(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVXMaxPS(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVXSqrtPS(XMMRegister dest, XMMRegister src);
  
  // SSE命令のエミッター（単精度）
  void EmitSSEAddPS(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSESubPS(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSEMulPS(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSEDivPS(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSEMinPS(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSEMaxPS(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSESqrtPS(XMMRegister dest, XMMRegister src);
  
  // AVX命令のエミッター（倍精度）
  void EmitAVXAddPD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVXSubPD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVXMulPD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVXDivPD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVXMinPD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVXMaxPD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVXSqrtPD(XMMRegister dest, XMMRegister src);
  
  // SSE命令のエミッター（倍精度）
  void EmitSSEAddPD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSESubPD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSEMulPD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSEDivPD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSEMinPD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSEMaxPD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSESqrtPD(XMMRegister dest, XMMRegister src);
  
  // AVX2命令のエミッター（整数）
  void EmitAVX2PaddD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVX2PsubD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVX2PmulD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVX2PminD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitAVX2PmaxD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  
  // SSE命令のエミッター（整数）
  void EmitSSEPaddD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSEPsubD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSEPmulD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSE41PminD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSE41PmaxD(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSEPminDEmulation(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  void EmitSSEPmaxDEmulation(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  
  // 特殊なSIMD操作
  void EmitDotProductPS(XMMRegister dest, XMMRegister src1, XMMRegister src2);
  
  // 行列操作のためのヘルパーメソッド
  void EmitMatrixMultiplication(IRNode* node);
  void EmitSmallMatrixMultiplication(Register resultAddr, IRNode* matrixA, IRNode* matrixB, int m, int n, int p);
  void EmitBlockedMatrixMultiplication(Register resultAddr, IRNode* matrixA, IRNode* matrixB, int m, int n, int p);
  void EmitTileMatrixMultiplication(Register resultAddr, IRNode* matrixA, IRNode* matrixB, int m, int n, int p, Register iReg, Register jReg, Register kReg, int tileSize);
  
  // メモリアクセスヘルパー
  Register AllocateStackMemory(int size, int alignment);
  XMMRegister GetUnusedXMMRegister();
  XMMRegister GetXMMRegisterForNode(IRNode* node);
  void BindXMMRegisterToNode(XMMRegister reg, IRNode* node);
  void BindAddressToNode(Register addr, IRNode* node);
  
  // ラベル生成ヘルパー
  Label CreateLabel(const std::string& name);
  LabelRef CreateLabelRef(const std::string& name);
  
  // 補助エンコード関数
  void EncodePrologue(std::vector<uint8_t>& code);
  void EncodeEpilogue(std::vector<uint8_t>& code);
  void EncodeLoadConst(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeMove(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeAdd(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeSub(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeMul(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeDiv(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeMod(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeNeg(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeCompare(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeLogical(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeBitwise(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeJump(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeCall(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeReturn(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeSIMDLoad(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeSIMDStore(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeSIMDArithmetic(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeFMA(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeFastMath(const IRInstruction& inst, std::vector<uint8_t>& code);
  void EncodeAVX512Load(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512Store(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512Arithmetic(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512FMA(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512MaskOp(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512Blend(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512Permute(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512Compress(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512Expand(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeShift(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeInc(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeDec(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeNop(std::vector<uint8_t>& code) noexcept;
  void OptimizeForCacheLine(std::vector<uint8_t>& code) noexcept;
  void AppendImmediate32(int32_t value) noexcept;
  void AppendImmediate64(int64_t value) noexcept;
  void AppendREXPrefix(bool w, bool r, bool x, bool b) noexcept;
  void AppendModRM(uint8_t mod, uint8_t reg, uint8_t rm) noexcept;
  void AppendVEXPrefix(uint8_t m, uint8_t p, uint8_t l, uint8_t w, uint8_t vvvv, uint8_t r, uint8_t x, uint8_t b) noexcept;
  void AppendSIB(uint8_t scale, uint8_t index, uint8_t base) noexcept;
  void AppendEVEXPrefix(uint8_t evex_mm, uint8_t evex_pp, uint8_t evex_W, uint8_t evex_vvvv, uint8_t evex_z, uint8_t evex_LL, bool evex_b, uint8_t evex_aaa, bool R, bool X, bool B, bool R_prime, bool V_prime) noexcept;
  
  // 最適化フラグ
  uint32_t m_optimizationFlags;
  // 汎用レジスタ割り当て
  std::unordered_map<int32_t, X86_64Register> m_registerMapping;
  // SIMDレジスタ割り当て
  std::unordered_map<int32_t, X86_64XMMRegister> m_simdRegisterMapping;

  // スタックフレーム管理
  uint32_t m_currentFrameSize; // 現在の関数のスタックフレームサイズ (RBP基準)
  std::unordered_map<int32_t, int32_t> m_spillSlots; // 仮想レジスタID -> RBPからのオフセット
  uint32_t m_nextSpillOffset; // 次に割り当てるスピルスロットのRBPからの負のオフセット開始点

  Context* m_context; // プロファイラ等へのアクセス用
};

} // namespace jit
} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_BACKEND_X86_64_CODE_GENERATOR_H