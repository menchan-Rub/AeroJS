#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aerojs {
namespace core {

/**
 * @brief バイトコード命令の操作コード
 * 
 * 各バイトコード命令の種類を表す列挙型です。
 */
enum class BytecodeOp : uint8_t {
    // スタック操作
    Nop,            // 何もしない
    Push,           // 定数をスタックにプッシュ
    Pop,            // スタックから値をポップ
    Dup,            // スタックトップを複製
    Swap,           // スタックの上位2つの値を交換
    
    // 定数ロード
    LoadConst,      // 定数をロード
    LoadNull,       // null値をロード
    LoadUndefined,  // undefined値をロード
    LoadTrue,       // true値をロード
    LoadFalse,      // false値をロード
    LoadZero,       // 数値0をロード
    LoadOne,        // 数値1をロード
    
    // ローカル変数操作
    GetLocal,       // ローカル変数の値を取得
    SetLocal,       // ローカル変数に値を設定
    
    // グローバル変数操作
    GetGlobal,      // グローバル変数の値を取得
    SetGlobal,      // グローバル変数に値を設定
    
    // プロパティ操作
    GetProperty,    // オブジェクトのプロパティを取得
    SetProperty,    // オブジェクトのプロパティを設定
    DeleteProperty, // オブジェクトのプロパティを削除
    
    // 配列操作
    GetElement,     // 配列の要素を取得
    SetElement,     // 配列の要素を設定
    DeleteElement,  // 配列の要素を削除
    
    // 算術演算
    Add,            // 加算
    Sub,            // 減算
    Mul,            // 乗算
    Div,            // 除算
    Mod,            // 剰余
    Neg,            // 符号反転
    Inc,            // インクリメント
    Dec,            // デクリメント
    
    // ビット演算
    BitAnd,         // ビットAND
    BitOr,          // ビットOR
    BitXor,         // ビットXOR
    BitNot,         // ビット反転
    ShiftLeft,      // 左シフト
    ShiftRight,     // 右シフト
    ShiftRightUnsigned, // 符号なし右シフト
    
    // 論理演算
    LogicalAnd,     // 論理AND
    LogicalOr,      // 論理OR
    LogicalNot,     // 論理NOT
    
    // 比較演算
    Equal,          // 等価
    NotEqual,       // 非等価
    StrictEqual,    // 厳密等価
    StrictNotEqual, // 厳密非等価
    LessThan,       // 未満
    LessThanOrEqual, // 以下
    GreaterThan,    // 超過
    GreaterThanOrEqual, // 以上
    
    // 制御フロー
    Jump,           // 無条件ジャンプ
    JumpIfTrue,     // 条件が真の場合ジャンプ
    JumpIfFalse,    // 条件が偽の場合ジャンプ
    Call,           // 関数呼び出し
    Return,         // 関数からのリターン
    
    // 例外処理
    Throw,          // 例外をスロー
    EnterTry,       // try範囲に入る
    ExitTry,        // try範囲を出る
    
    // オブジェクト操作
    CreateObject,   // 新しいオブジェクトを作成
    CreateArray,    // 新しい配列を作成
    
    // その他
    TypeOf,         // 値の型を取得
    InstanceOf,     // インスタンス関係をチェック
    In,             // inオペレータ
    
    // デバッグ用
    DebugBreak,     // デバッグブレークポイント
};

/**
 * @brief オペランドの最大数
 */
constexpr uint8_t MAX_OPERANDS = 3;

/**
 * @brief バイトコード命令クラス
 * 
 * 単一のバイトコード命令を表します。
 * 操作コードと最大3つのオペランドを持ちます。
 */
class BytecodeInstruction {
public:
    /**
     * @brief デフォルトコンストラクタ
     * 
     * Nop命令を作成します。
     */
    BytecodeInstruction();
    
    /**
     * @brief オペランドなしの命令を作成
     * 
     * @param opcode 操作コード
     */
    explicit BytecodeInstruction(BytecodeOp opcode);
    
    /**
     * @brief 1つのオペランドを持つ命令を作成
     * 
     * @param opcode 操作コード
     * @param op1 オペランド1
     */
    BytecodeInstruction(BytecodeOp opcode, uint32_t op1);
    
    /**
     * @brief 2つのオペランドを持つ命令を作成
     * 
     * @param opcode 操作コード
     * @param op1 オペランド1
     * @param op2 オペランド2
     */
    BytecodeInstruction(BytecodeOp opcode, uint32_t op1, uint32_t op2);
    
    /**
     * @brief 3つのオペランドを持つ命令を作成
     * 
     * @param opcode 操作コード
     * @param op1 オペランド1
     * @param op2 オペランド2
     * @param op3 オペランド3
     */
    BytecodeInstruction(BytecodeOp opcode, uint32_t op1, uint32_t op2, uint32_t op3);
    
    /**
     * @brief 操作コードを取得
     * 
     * @return BytecodeOp 操作コード
     */
    BytecodeOp GetOpcode() const { return m_opcode; }
    
    /**
     * @brief オペランドの数を取得
     * 
     * @return uint8_t オペランドの数
     */
    uint8_t GetOperandCount() const;
    
    /**
     * @brief オペランドを取得
     * 
     * @param index オペランドのインデックス（0-2）
     * @return uint32_t オペランドの値
     * @throws std::out_of_range インデックスが範囲外の場合
     */
    uint32_t GetOperand(uint8_t index) const;
    
    /**
     * @brief 命令のバイトサイズを取得
     * 
     * @return uint8_t 命令のバイトサイズ
     */
    uint8_t GetSize() const;
    
    /**
     * @brief 命令の文字列表現を取得
     * 
     * @return std::string 命令の文字列表現
     */
    std::string ToString() const;
    
    /**
     * @brief 指定された操作コードに必要なオペランド数を取得
     * 
     * @param opcode 操作コード
     * @return uint8_t オペランド数
     */
    static uint8_t GetOperandCountForOpcode(BytecodeOp opcode);
    
    /**
     * @brief 操作コードの文字列表現を取得
     * 
     * @param opcode 操作コード
     * @return const char* 操作コードの文字列表現
     */
    static const char* GetOpcodeString(BytecodeOp opcode);

private:
    // 操作コード
    BytecodeOp m_opcode;
    
    // オペランド配列
    std::vector<uint32_t> m_operands;
};

/**
 * @brief 例外ハンドラクラス
 * 
 * try-catch ブロックの例外ハンドラ情報を表します。
 */
class ExceptionHandler {
public:
    /**
     * @brief コンストラクタ
     * 
     * @param try_start try節の開始オフセット
     * @param try_end try節の終了オフセット
     * @param handler_offset ハンドラのオフセット
     */
    ExceptionHandler(uint32_t try_start, uint32_t try_end, uint32_t handler_offset)
        : m_try_start(try_start), m_try_end(try_end), m_handler_offset(handler_offset) {}
    
    /**
     * @brief try節の開始オフセットを取得
     * 
     * @return uint32_t try節の開始オフセット
     */
    uint32_t GetTryStart() const { return m_try_start; }
    
    /**
     * @brief try節の終了オフセットを取得
     * 
     * @return uint32_t try節の終了オフセット
     */
    uint32_t GetTryEnd() const { return m_try_end; }
    
    /**
     * @brief ハンドラのオフセットを取得
     * 
     * @return uint32_t ハンドラのオフセット
     */
    uint32_t GetHandlerOffset() const { return m_handler_offset; }
    
    /**
     * @brief 指定したオフセットがtry節内にあるか確認
     * 
     * @param offset 確認するオフセット
     * @return bool try節内にある場合はtrue
     */
    bool IsInTryBlock(uint32_t offset) const;

private:
    // try節の開始オフセット
    uint32_t m_try_start;
    
    // try節の終了オフセット
    uint32_t m_try_end;
    
    // ハンドラのオフセット
    uint32_t m_handler_offset;
};

} // namespace core
} // namespace aerojs 