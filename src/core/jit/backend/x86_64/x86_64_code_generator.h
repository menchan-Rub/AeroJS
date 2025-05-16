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

namespace aerojs {
namespace core {
namespace jit {

/**
 * @brief コード生成の最適化フラグ
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
  UseCPUSpecific = 1 << 9 // CPU固有の最適化を適用
};

// フラグチェックのためのヘルパー関数
inline bool HasFlag(uint32_t flags, CodeGenOptFlags flag) {
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

/**
 * @brief x86_64向けマシンコードを生成するクラス
 */
class X86_64CodeGenerator {
public:
  /**
   * @brief コンストラクタ
   */
  X86_64CodeGenerator();
  
  /**
   * @brief デストラクタ
   */
  ~X86_64CodeGenerator();
  
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
  void Align(size_t alignment, uint8_t fillValue = 0x90 /* NOP */);
  
  /**
   * @brief ラベルを定義
   * @param label ラベル名
   */
  void DefineLabel(const std::string& label);
  
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
  bool ResolveLabels();
  
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
  void EmitJE_Label(const std::string& label, bool isShort = true);   // = / Zero
  void EmitJNE_Label(const std::string& label, bool isShort = true);  // != / Not Zero
  void EmitJG_Label(const std::string& label, bool isShort = true);   // >
  void EmitJGE_Label(const std::string& label, bool isShort = true);  // >=
  void EmitJL_Label(const std::string& label, bool isShort = true);   // <
  void EmitJLE_Label(const std::string& label, bool isShort = true);  // <=
  
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
  void AppendVEXPrefix(std::vector<uint8_t>& code, uint8_t m, uint8_t p, uint8_t l, uint8_t w, uint8_t vvvv, uint8_t r, uint8_t x, uint8_t b) noexcept;
  
  // AVX-512用EVEXプレフィックスの追加ヘルパー
  void AppendEVEXPrefix(std::vector<uint8_t>& code, uint8_t m, uint8_t p, uint8_t l, uint8_t w, 
                        uint8_t vvvv, uint8_t aaa, uint8_t z, uint8_t b, 
                        uint8_t v2, uint8_t k, bool broadcast) noexcept;
  
  // SIBバイトの追加ヘルパー
  void AppendSIB(std::vector<uint8_t>& code, uint8_t scale, uint8_t index, uint8_t base) noexcept;
  
  // SIMDレジスタの取得
  std::optional<X86_64FloatRegister> GetSIMDReg(int32_t virtualReg) const noexcept;
  
  // AVX-512命令の追加メソッド
  void EncodeAVX512Load(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512Store(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512Arithmetic(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512FMA(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  
  // AVX-512用マスク操作
  void EncodeAVX512MaskOp(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512Blend(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512Permute(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512Compress(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  void EncodeAVX512Expand(const IRInstruction& inst, std::vector<uint8_t>& code) noexcept;
  
  // キャッシュライン最適化
  void OptimizeForCacheLine(std::vector<uint8_t>& code) noexcept;
  
private:
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
};

} // namespace jit
} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_BACKEND_X86_64_CODE_GENERATOR_H