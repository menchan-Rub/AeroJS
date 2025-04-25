#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <string>

#include "ir.h"

namespace aerojs {
namespace core {

/**
 * @brief バイトコードからIR生成を行うクラス
 */
class IRBuilder {
public:
  IRBuilder() noexcept;
  ~IRBuilder() noexcept;
  
  /**
   * @brief バイトコードからIRを生成する
   * @param bytecodes バイトコードバイト列
   * @param functionId 関数ID（プロファイル情報取得用）
   * @return 生成されたIR関数オブジェクト
   */
  std::unique_ptr<IRFunction> BuildIR(const std::vector<uint8_t>& bytecodes, 
                                      uint32_t functionId) noexcept;
  
  /**
   * @brief バイトコード命令をIR命令に変換する関数の型定義
   */
  using BytecodeHandlerFn = std::function<void(IRBuilder&, uint8_t, const std::vector<uint8_t>&, size_t&)>;
  
  /**
   * @brief 内部状態をリセットする
   */
  void Reset() noexcept;
  
  /**
   * @brief 現在のIR関数を設定する
   * @param function IR関数ポインタ
   */
  void SetFunction(IRFunction* function) noexcept { m_function = function; }
  
  /**
   * @brief 空のNOP命令を生成する
   */
  void BuildNop() noexcept;
  
  /**
   * @brief 定数読み込み命令を生成する
   * @param dest 格納先レジスタ
   * @param value 定数値
   */
  void BuildLoadConst(uint32_t dest, uint32_t value) noexcept;
  
  /**
   * @brief Move命令を生成する
   * @param dest 格納先レジスタ
   * @param src 元レジスタ
   */
  void BuildMove(uint32_t dest, uint32_t src) noexcept;
  
  /**
   * @brief 加算命令を生成する
   * @param dest 格納先レジスタ
   * @param src1 元レジスタ1
   * @param src2 元レジスタ2
   */
  void BuildAdd(uint32_t dest, uint32_t src1, uint32_t src2) noexcept;
  
  /**
   * @brief 減算命令を生成する
   * @param dest 格納先レジスタ
   * @param src1 元レジスタ1
   * @param src2 元レジスタ2
   */
  void BuildSub(uint32_t dest, uint32_t src1, uint32_t src2) noexcept;
  
  /**
   * @brief 乗算命令を生成する
   * @param dest 格納先レジスタ
   * @param src1 元レジスタ1
   * @param src2 元レジスタ2
   */
  void BuildMul(uint32_t dest, uint32_t src1, uint32_t src2) noexcept;
  
  /**
   * @brief 除算命令を生成する
   * @param dest 格納先レジスタ
   * @param src1 元レジスタ1
   * @param src2 元レジスタ2
   */
  void BuildDiv(uint32_t dest, uint32_t src1, uint32_t src2) noexcept;
  
  /**
   * @brief 等価比較命令を生成する
   * @param dest 格納先レジスタ
   * @param src1 元レジスタ1
   * @param src2 元レジスタ2
   */
  void BuildCompareEq(uint32_t dest, uint32_t src1, uint32_t src2) noexcept;
  
  /**
   * @brief 非等価比較命令を生成する
   * @param dest 格納先レジスタ
   * @param src1 元レジスタ1
   * @param src2 元レジスタ2
   */
  void BuildCompareNe(uint32_t dest, uint32_t src1, uint32_t src2) noexcept;
  
  /**
   * @brief 小なり比較命令を生成する
   * @param dest 格納先レジスタ
   * @param src1 元レジスタ1
   * @param src2 元レジスタ2
   */
  void BuildCompareLt(uint32_t dest, uint32_t src1, uint32_t src2) noexcept;
  
  /**
   * @brief 以下比較命令を生成する
   * @param dest 格納先レジスタ
   * @param src1 元レジスタ1
   * @param src2 元レジスタ2
   */
  void BuildCompareLe(uint32_t dest, uint32_t src1, uint32_t src2) noexcept;
  
  /**
   * @brief 大なり比較命令を生成する
   * @param dest 格納先レジスタ
   * @param src1 元レジスタ1
   * @param src2 元レジスタ2
   */
  void BuildCompareGt(uint32_t dest, uint32_t src1, uint32_t src2) noexcept;
  
  /**
   * @brief 以上比較命令を生成する
   * @param dest 格納先レジスタ
   * @param src1 元レジスタ1
   * @param src2 元レジスタ2
   */
  void BuildCompareGe(uint32_t dest, uint32_t src1, uint32_t src2) noexcept;
  
  /**
   * @brief 無条件ジャンプ命令を生成する
   * @param label ジャンプ先ラベル名
   */
  void BuildJump(const std::string& label) noexcept;
  
  /**
   * @brief 条件付きジャンプ命令を生成する（条件が真の場合）
   * @param condReg 条件レジスタ
   * @param label ジャンプ先ラベル名
   */
  void BuildJumpIfTrue(uint32_t condReg, const std::string& label) noexcept;
  
  /**
   * @brief 条件付きジャンプ命令を生成する（条件が偽の場合）
   * @param condReg 条件レジスタ
   * @param label ジャンプ先ラベル名
   */
  void BuildJumpIfFalse(uint32_t condReg, const std::string& label) noexcept;
  
  /**
   * @brief 関数呼び出し命令を生成する
   * @param dest 戻り値格納先レジスタ
   * @param funcReg 関数ポインタを格納しているレジスタ
   * @param args 引数レジスタ配列
   */
  void BuildCall(uint32_t dest, uint32_t funcReg, const std::vector<uint32_t>& args) noexcept;
  
  /**
   * @brief 戻り値なしリターン命令を生成する
   */
  void BuildReturn() noexcept;
  
  /**
   * @brief 戻り値ありリターン命令を生成する
   * @param retReg 戻り値レジスタ
   */
  void BuildReturn(uint32_t retReg) noexcept;
  
  /**
   * @brief 実行プロファイリング命令を生成する
   * @param bytecodeOffset バイトコードオフセット
   */
  void BuildProfileExecution(uint32_t bytecodeOffset) noexcept;
  
  /**
   * @brief 型プロファイリング命令を生成する
   * @param bytecodeOffset バイトコードオフセット
   * @param typeCategory 型カテゴリ
   */
  void BuildProfileType(uint32_t bytecodeOffset, uint32_t typeCategory) noexcept;
  
  /**
   * @brief 呼び出しサイトプロファイリング命令を生成する
   * @param bytecodeOffset バイトコードオフセット
   */
  void BuildProfileCallSite(uint32_t bytecodeOffset) noexcept;

private:
  // 現在構築中のIR関数
  IRFunction* m_function;
  
  // バイトコード命令からIR命令への変換テーブル
  std::unordered_map<uint8_t, BytecodeHandlerFn> m_bytecodeHandlers;
  
  // バイトコード命令ハンドラを初期化
  void InitBytecodeHandlers() noexcept;
  
  // バイトコード命令ハンドラ（一部のみ実装例）
  void HandleNop(uint8_t opcode, const std::vector<uint8_t>& bytecodes, size_t& offset) noexcept;
  void HandleLoadConst(uint8_t opcode, const std::vector<uint8_t>& bytecodes, size_t& offset) noexcept;
  void HandleLoadVar(uint8_t opcode, const std::vector<uint8_t>& bytecodes, size_t& offset) noexcept;
  void HandleStoreVar(uint8_t opcode, const std::vector<uint8_t>& bytecodes, size_t& offset) noexcept;
  void HandleAdd(uint8_t opcode, const std::vector<uint8_t>& bytecodes, size_t& offset) noexcept;
  void HandleSub(uint8_t opcode, const std::vector<uint8_t>& bytecodes, size_t& offset) noexcept;
  void HandleMul(uint8_t opcode, const std::vector<uint8_t>& bytecodes, size_t& offset) noexcept;
  void HandleDiv(uint8_t opcode, const std::vector<uint8_t>& bytecodes, size_t& offset) noexcept;
  void HandleCall(uint8_t opcode, const std::vector<uint8_t>& bytecodes, size_t& offset) noexcept;
  void HandleReturn(uint8_t opcode, const std::vector<uint8_t>& bytecodes, size_t& offset) noexcept;
  
  // IRへの命令追加ヘルパーメソッド
  void AddInstruction(Opcode opcode, const std::vector<int32_t>& args = {}) noexcept;
};

}  // namespace core
}  // namespace aerojs 