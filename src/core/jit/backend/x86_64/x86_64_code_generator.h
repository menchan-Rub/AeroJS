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