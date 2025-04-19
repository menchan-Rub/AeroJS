/**
 * @file bytecode_instruction.h
 * @brief バイトコード命令の定義
 * 
 * このファイルはインタプリタで実行されるバイトコード命令を定義します。
 * 各命令はオペコードとオペランドで構成されます。
 */

#ifndef AEROJS_CORE_VM_INTERPRETER_BYTECODE_INSTRUCTION_H_
#define AEROJS_CORE_VM_INTERPRETER_BYTECODE_INSTRUCTION_H_

#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace aerojs {
namespace core {

/**
 * @brief バイトコード命令のオペコード
 * 
 * JavaScriptエンジンの仮想マシンで使用される全ての命令のオペコードを定義します。
 */
enum class Opcode : uint8_t {
  // スタック操作
  kPush = 0x01,           // 定数をスタックにプッシュ
  kPop = 0x02,            // スタックから値をポップ
  kDuplicate = 0x03,      // スタックの一番上の値を複製
  kSwap = 0x04,           // スタックの上位2つの値を入れ替え

  // 算術演算
  kAdd = 0x10,            // 加算
  kSub = 0x11,            // 減算
  kMul = 0x12,            // 乗算
  kDiv = 0x13,            // 除算
  kMod = 0x14,            // 剰余
  kPow = 0x15,            // べき乗
  kNeg = 0x16,            // 単項マイナス
  kInc = 0x17,            // インクリメント
  kDec = 0x18,            // デクリメント

  // ビット演算
  kBitAnd = 0x20,         // ビット論理積
  kBitOr = 0x21,          // ビット論理和
  kBitXor = 0x22,         // ビット排他的論理和
  kBitNot = 0x23,         // ビット否定
  kLeftShift = 0x24,      // 左シフト
  kRightShift = 0x25,     // 右シフト
  kUnsignedRightShift = 0x26, // 符号なし右シフト

  // 論理演算
  kLogicalAnd = 0x30,     // 論理積
  kLogicalOr = 0x31,      // 論理和
  kLogicalNot = 0x32,     // 論理否定

  // 比較演算
  kEqual = 0x40,          // 等価 (==)
  kStrictEqual = 0x41,    // 厳密等価 (===)
  kNotEqual = 0x42,       // 非等価 (!=)
  kStrictNotEqual = 0x43, // 厳密非等価 (!==)
  kLessThan = 0x44,       // 未満 (<)
  kLessThanOrEqual = 0x45, // 以下 (<=)
  kGreaterThan = 0x46,    // 超過 (>)
  kGreaterThanOrEqual = 0x47, // 以上 (>=)
  kInstanceOf = 0x48,     // instanceof演算子
  kIn = 0x49,             // in演算子

  // 制御フロー
  kJump = 0x50,           // 無条件ジャンプ
  kJumpIfTrue = 0x51,     // 条件付きジャンプ（真の場合）
  kJumpIfFalse = 0x52,    // 条件付きジャンプ（偽の場合）
  kCall = 0x53,           // 関数呼び出し
  kReturn = 0x54,         // 関数からの戻り
  kThrow = 0x55,          // 例外をスロー
  kEnterTry = 0x56,       // try ブロックに入る
  kLeaveTry = 0x57,       // try ブロックから出る
  kEnterCatch = 0x58,     // catch ブロックに入る
  kLeaveCatch = 0x59,     // catch ブロックから出る
  kEnterFinally = 0x5A,   // finally ブロックに入る
  kLeaveFinally = 0x5B,   // finally ブロックから出る

  // 変数操作
  kGetLocal = 0x60,       // ローカル変数の値を取得
  kSetLocal = 0x61,       // ローカル変数に値を設定
  kGetGlobal = 0x62,      // グローバル変数の値を取得
  kSetGlobal = 0x63,      // グローバル変数に値を設定
  kGetUpvalue = 0x64,     // アップバリューの値を取得
  kSetUpvalue = 0x65,     // アップバリューに値を設定
  kDeclareVar = 0x66,     // 変数宣言
  kDeclareConst = 0x67,   // 定数宣言
  kDeclareLet = 0x68,     // let宣言

  // オブジェクト操作
  kNewObject = 0x70,      // 新しいオブジェクトを作成
  kNewArray = 0x71,       // 新しい配列を作成
  kGetProperty = 0x72,    // プロパティの値を取得
  kSetProperty = 0x73,    // プロパティに値を設定
  kDeleteProperty = 0x74, // プロパティを削除
  kGetElement = 0x75,     // 配列要素の値を取得
  kSetElement = 0x76,     // 配列要素に値を設定
  kDeleteElement = 0x77,  // 配列要素を削除
  kNewFunction = 0x78,    // 新しい関数を作成
  kNewClass = 0x79,       // 新しいクラスを作成
  kGetSuperProperty = 0x7A, // スーパークラスのプロパティを取得
  kSetSuperProperty = 0x7B, // スーパークラスのプロパティを設定

  // イテレータ操作
  kIteratorInit = 0x80,   // イテレータの初期化
  kIteratorNext = 0x81,   // イテレータの次の値を取得
  kIteratorClose = 0x82,  // イテレータのクローズ

  // 非同期操作
  kAwait = 0x90,          // Promise の解決を待機
  kYield = 0x91,          // Generatorから値を返す
  kYieldStar = 0x92,      // Generator委譲

  // その他
  kNop = 0xF0,            // 何もしない
  kDebugger = 0xF1,       // デバッグポイント
  kTypeOf = 0xF2,         // typeof演算子
  kVoid = 0xF3,           // void演算子
  kDelete = 0xF4,         // delete演算子
  kImport = 0xF5,         // モジュールインポート
  kExport = 0xF6,         // モジュールエクスポート
};

/**
 * @brief バイトコード命令クラス
 * 
 * バイトコード命令を表すクラスです。オペコードと最大4つのオペランドを持ちます。
 */
class BytecodeInstruction {
public:
  // 最大オペランド数
  static constexpr size_t kMaxOperands = 4;

  /**
   * @brief デフォルトコンストラクタ
   */
  BytecodeInstruction()
    : m_opcode(static_cast<uint8_t>(Opcode::kNop))
    , m_operandCount(0)
    , m_operands{0, 0, 0, 0}
    , m_sourceLine(0)
    , m_sourceColumn(0) {}

  /**
   * @brief オペコードを指定するコンストラクタ
   * 
   * @param opcode 命令のオペコード
   */
  explicit BytecodeInstruction(Opcode opcode)
    : m_opcode(static_cast<uint8_t>(opcode))
    , m_operandCount(0)
    , m_operands{0, 0, 0, 0}
    , m_sourceLine(0)
    , m_sourceColumn(0) {}

  /**
   * @brief オペコードとオペランド1つを指定するコンストラクタ
   * 
   * @param opcode 命令のオペコード
   * @param operand1 1つ目のオペランド
   */
  BytecodeInstruction(Opcode opcode, int32_t operand1)
    : m_opcode(static_cast<uint8_t>(opcode))
    , m_operandCount(1)
    , m_operands{operand1, 0, 0, 0}
    , m_sourceLine(0)
    , m_sourceColumn(0) {}

  /**
   * @brief オペコードとオペランド2つを指定するコンストラクタ
   * 
   * @param opcode 命令のオペコード
   * @param operand1 1つ目のオペランド
   * @param operand2 2つ目のオペランド
   */
  BytecodeInstruction(Opcode opcode, int32_t operand1, int32_t operand2)
    : m_opcode(static_cast<uint8_t>(opcode))
    , m_operandCount(2)
    , m_operands{operand1, operand2, 0, 0}
    , m_sourceLine(0)
    , m_sourceColumn(0) {}

  /**
   * @brief オペコードとオペランド3つを指定するコンストラクタ
   * 
   * @param opcode 命令のオペコード
   * @param operand1 1つ目のオペランド
   * @param operand2 2つ目のオペランド
   * @param operand3 3つ目のオペランド
   */
  BytecodeInstruction(Opcode opcode, int32_t operand1, int32_t operand2, int32_t operand3)
    : m_opcode(static_cast<uint8_t>(opcode))
    , m_operandCount(3)
    , m_operands{operand1, operand2, operand3, 0}
    , m_sourceLine(0)
    , m_sourceColumn(0) {}

  /**
   * @brief オペコードとオペランド4つを指定するコンストラクタ
   * 
   * @param opcode 命令のオペコード
   * @param operand1 1つ目のオペランド
   * @param operand2 2つ目のオペランド
   * @param operand3 3つ目のオペランド
   * @param operand4 4つ目のオペランド
   */
  BytecodeInstruction(Opcode opcode, int32_t operand1, int32_t operand2, int32_t operand3, int32_t operand4)
    : m_opcode(static_cast<uint8_t>(opcode))
    , m_operandCount(4)
    , m_operands{operand1, operand2, operand3, operand4}
    , m_sourceLine(0)
    , m_sourceColumn(0) {}

  /**
   * @brief ソース位置情報を設定
   * 
   * @param line ソースコードの行番号
   * @param column ソースコードの列番号
   */
  void setSourcePosition(int line, int column) {
    m_sourceLine = line;
    m_sourceColumn = column;
  }

  /**
   * @brief オペコードを取得
   * 
   * @return Opcode 命令のオペコード
   */
  Opcode getOpcode() const {
    return static_cast<Opcode>(m_opcode);
  }

  /**
   * @brief オペランド数を取得
   * 
   * @return size_t オペランドの数
   */
  size_t getOperandCount() const {
    return m_operandCount;
  }

  /**
   * @brief 指定したインデックスのオペランドを取得
   * 
   * @param index オペランドのインデックス
   * @return int32_t オペランドの値
   * @throws std::out_of_range インデックスが範囲外の場合
   */
  int32_t getOperand(size_t index) const {
    if (index >= m_operandCount) {
      throw std::out_of_range("オペランドインデックスが範囲外です");
    }
    return m_operands[index];
  }

  /**
   * @brief ソースの行番号を取得
   * 
   * @return int ソースコードの行番号
   */
  int getSourceLine() const {
    return m_sourceLine;
  }

  /**
   * @brief ソースの列番号を取得
   * 
   * @return int ソースコードの列番号
   */
  int getSourceColumn() const {
    return m_sourceColumn;
  }

  /**
   * @brief 命令の文字列表現を取得
   * 
   * @return std::string 命令の文字列表現
   */
  std::string toString() const;

private:
  uint8_t m_opcode;                   // オペコード
  size_t m_operandCount;              // オペランド数
  std::array<int32_t, kMaxOperands> m_operands;  // オペランド
  int m_sourceLine;                   // ソースコードの行番号
  int m_sourceColumn;                 // ソースコードの列番号
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_VM_INTERPRETER_BYTECODE_INSTRUCTION_H_ 