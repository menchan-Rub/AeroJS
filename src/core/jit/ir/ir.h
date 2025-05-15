#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

namespace aerojs {
namespace core {

/**
 * @brief IR命令のタイプを表す列挙型
 */
enum class Opcode : uint8_t {
  // 基本命令
  kNop = 0,       // 何もしない
  kLoadConst,     // 定数をレジスタにロード
  kMove,          // レジスタ間の値の移動
  
  // 算術演算
  kAdd,           // 加算
  kSub,           // 減算
  kMul,           // 乗算
  kDiv,           // 除算
  kMod,           // 剰余
  kNeg,           // 符号反転
  
  // 比較
  kCompareEq,     // 等しい
  kCompareNe,     // 等しくない
  kCompareLt,     // より小さい
  kCompareLe,     // 以下
  kCompareGt,     // より大きい
  kCompareGe,     // 以上
  
  // 論理演算
  kAnd,           // 論理AND
  kOr,            // 論理OR
  kNot,           // 論理NOT
  
  // ビット演算
  kBitAnd,        // ビット単位AND
  kBitOr,         // ビット単位OR
  kBitXor,        // ビット単位XOR
  kBitNot,        // ビット単位NOT
  kShiftLeft,     // 左シフト
  kShiftRight,    // 右シフト
  
  // 制御フロー
  kJump,          // 無条件ジャンプ
  kJumpIfTrue,    // 条件が真の場合にジャンプ
  kJumpIfFalse,   // 条件が偽の場合にジャンプ
  kCall,          // 関数呼び出し
  kReturn,        // 関数からの戻り
  
  // メモリ操作
  kLoad,          // メモリからロード
  kStore,         // メモリに保存
  
  // プロパティアクセス
  kGetProperty,   // オブジェクトからプロパティ取得
  kSetProperty,   // オブジェクトにプロパティ設定
  
  // プロファイリング関連
  kProfileExecution,  // 実行プロファイリング
  kProfileType,       // 型プロファイリング
  kProfileCallSite,   // 呼び出しサイトプロファイリング
  
  // その他
  kPhi,           // PHI関数（SSA形式用）
  kDebugPrint,    // デバッグ出力
  
  // 列挙の終端マーカー
  kLastOpcode
};

/**
 * @brief IR命令を表す構造体
 */
struct IRInstruction {
  // 命令のタイプ
  Opcode opcode;
  
  // 命令の引数（レジスタ番号、即値など）
  std::vector<int32_t> args;
  
  // オプションのメタデータ（デバッグ情報など）
  std::string metadata;
};

/**
 * @brief IR関数を表すクラス
 * 
 * IR命令のコレクションを管理し、IR上での操作を提供します。
 */
class IRFunction {
public:
  IRFunction() = default;
  ~IRFunction() = default;
  
  /**
   * @brief IR命令を追加する
   * @param instruction 追加する命令
   * @return 追加した命令のインデックス
   */
  size_t AddInstruction(const IRInstruction& instruction) {
    m_instructions.push_back(instruction);
    return m_instructions.size() - 1;
  }
  
  /**
   * @brief IR命令を取得する
   * @param index 命令のインデックス
   * @return 命令への参照
   */
  const IRInstruction& GetInstruction(size_t index) const {
    return m_instructions[index];
  }
  
  /**
   * @brief IR命令を変更する
   * @param index 命令のインデックス
   * @param instruction 新しい命令
   */
  void SetInstruction(size_t index, const IRInstruction& instruction) {
    if (index < m_instructions.size()) {
      m_instructions[index] = instruction;
    }
  }
  
  /**
   * @brief IR命令を削除する
   * @param index 削除する命令のインデックス
   */
  void RemoveInstruction(size_t index) {
    if (index < m_instructions.size()) {
      m_instructions.erase(m_instructions.begin() + index);
    }
  }
  
  /**
   * @brief 命令数を取得する
   * @return IR命令の数
   */
  size_t GetInstructionCount() const {
    return m_instructions.size();
  }
  
  /**
   * @brief すべての命令を取得する
   * @return 命令コレクションへの参照
   */
  const std::vector<IRInstruction>& GetInstructions() const {
    return m_instructions;
  }
  
  /**
   * @brief すべての命令をクリアする
   */
  void Clear() {
    m_instructions.clear();
  }
  
  /**
   * @brief ラベルを登録する
   * @param label ラベル名
   * @return ラベルID
   */
  uint32_t RegisterLabel(const std::string& label) {
    if (m_labels.find(label) == m_labels.end()) {
      uint32_t id = static_cast<uint32_t>(m_labels.size());
      m_labels[label] = id;
      return id;
    }
    return m_labels[label];
  }
  
  /**
   * @brief ラベルIDを取得する
   * @param label ラベル名
   * @return ラベルID（存在しない場合は-1）
   */
  int32_t GetLabelId(const std::string& label) const {
    auto it = m_labels.find(label);
    if (it != m_labels.end()) {
      return static_cast<int32_t>(it->second);
    }
    return -1;
  }
  
  /**
   * @brief ラベル名を取得する
   * @param id ラベルID
   * @return ラベル名（存在しない場合は空文字列）
   */
  std::string GetLabelName(uint32_t id) const {
    for (const auto& [name, labelId] : m_labels) {
      if (labelId == id) {
        return name;
      }
    }
    return "";
  }
  
private:
  // IR命令のコレクション
  std::vector<IRInstruction> m_instructions;
  
  // ラベル名とIDのマッピング
  std::unordered_map<std::string, uint32_t> m_labels;
};

}  // namespace core
}  // namespace aerojs