#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aerojs {
namespace core {

// バイトコードのオペコード一覧
enum class BytecodeOpcode {
    Nop = 0,        // 何もしない
    LoadConst,      // 定数をロード
    LoadVar,        // 変数をロード
    StoreVar,       // 変数に格納
    Add,            // 加算
    Sub,            // 減算
    Mul,            // 乗算
    Div,            // 除算
    Equal,          // 等値比較
    NotEqual,       // 不等値比較
    LessThan,       // 小なり比較
    LessThanOrEqual, // 以下比較
    GreaterThan,    // 大なり比較
    GreaterThanOrEqual, // 以上比較
    Jump,           // 無条件ジャンプ
    JumpIfTrue,     // 真の場合ジャンプ
    JumpIfFalse,    // 偽の場合ジャンプ
    Call,           // 関数呼び出し
    Return,         // 関数からの戻り
    Count,          // オペコードの総数（内部使用）
    Invalid = 0xFF  // 無効なオペコード
};

// オペランドのタイプ
enum class OperandType : uint8_t {
    None = 0,       // オペランドなし
    Uint8 = 1,      // 8ビット符号なし整数
    Uint16 = 2,     // 16ビット符号なし整数
    Uint32 = 3      // 32ビット符号なし整数
};

// 複数のオペランドタイプをエンコードする型
using OperandTypes = uint16_t;

// バイトコード命令の最大オペランド数
constexpr size_t kMaxBytecodeOperands = 4;

// バイトコード命令の構造体
struct Bytecode {
    BytecodeOpcode opcode;                       // オペコード
    size_t operandCount;                         // オペランドの数
    uint32_t operands[kMaxBytecodeOperands];     // オペランドの配列
};

/**
 * @class BytecodeDecoder
 * @brief バイトコードストリームからインストラクションとオペランドをデコードするためのクラス
 * 
 * このクラスはバイトコードを解析し、オペコードとオペランドを取得するメソッドを提供します。
 * また、特定のオフセットへのシークやデコーダのリセットも可能です。
 */
class BytecodeDecoder {
public:
    /**
     * @brief デフォルトコンストラクタ
     */
    BytecodeDecoder();
    
    /**
     * @brief デストラクタ
     */
    ~BytecodeDecoder();
    
    /**
     * @brief デコード対象のバイトコードを設定する
     * @param bytecodes デコードするバイトコード配列へのポインタ
     * @param length バイトコード配列の長さ
     */
    void SetBytecode(const uint8_t* bytecodes, size_t length);
    
    /**
     * @brief 次のインストラクションをデコードする
     * @param operands デコードされたオペランドを格納するベクター
     * @return デコードされたバイトコードオペコード
     */
    BytecodeOpcode DecodeNextInstruction(std::vector<uint32_t>& operands);
    
    /**
     * @brief 特定のオフセットにシークする
     * @param offset シーク先のオフセット
     */
    void Seek(size_t offset);
    
    /**
     * @brief 現在のオフセットを取得する
     * @return 現在のオフセット位置
     */
    size_t GetCurrentOffset() const;
    
    /**
     * @brief デコーダをリセットする
     */
    void Reset();
    
private:
    /**
     * @brief 次のオペランドを読み取る
     * @return 読み取られたオペランド値
     */
    uint32_t ReadOperand();
    
    /**
     * @brief 現在のオペランドのサイズを決定する
     * @return オペランドのサイズ（バイト単位）
     */
    uint8_t DetermineOperandSize();
    
    // バイトコード配列へのポインタ
    const uint8_t* m_bytecodes;
    
    // バイトコード配列の長さ
    size_t m_bytecodeLength;
    
    // 現在のデコード位置
    size_t m_currentOffset;
};

} // namespace core
} // namespace aerojs 