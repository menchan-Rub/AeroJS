#ifndef AEROJS_CORE_JIT_BYTECODE_BYTECODE_OPCODES_H
#define AEROJS_CORE_JIT_BYTECODE_BYTECODE_OPCODES_H

#include <cstdint>
#include <string>
#include <unordered_map>

namespace aerojs {
namespace core {

// バイトコードの命令セット
enum class BytecodeOpcode : uint8_t {
    // スタック操作
    Nop,            // 何もしない
    Pop,            // スタックトップを捨てる
    Dup,            // スタックトップを複製
    Swap,           // スタックトップ2つの値を交換
    
    // 定数・リテラル
    PushUndefined,  // undefined をプッシュ
    PushNull,       // null をプッシュ
    PushTrue,       // true をプッシュ
    PushFalse,      // false をプッシュ
    PushZero,       // 0 をプッシュ
    PushOne,        // 1 をプッシュ
    PushConst,      // 定数プールからロード (operand: 定数プールインデックス)
    
    // 変数操作
    LoadLocal,      // ローカル変数をロード (operand: 変数インデックス)
    StoreLocal,     // ローカル変数に格納 (operand: 変数インデックス)
    LoadArg,        // 引数をロード (operand: 引数インデックス)
    StoreArg,       // 引数に格納 (operand: 引数インデックス)
    LoadGlobal,     // グローバル変数をロード (operand: 文字列テーブルインデックス)
    StoreGlobal,    // グローバル変数に格納 (operand: 文字列テーブルインデックス)
    
    // プロパティアクセス
    LoadProp,       // プロパティをロード (operand: 文字列テーブルインデックス)
    StoreProp,      // プロパティに格納 (operand: 文字列テーブルインデックス)
    DeleteProp,     // プロパティを削除 (operand: 文字列テーブルインデックス)
    HasProp,        // プロパティ存在チェック (operand: 文字列テーブルインデックス)
    LoadElem,       // 配列要素をロード
    StoreElem,      // 配列要素に格納
    DeleteElem,     // 配列要素を削除
    
    // 算術演算
    Add,            // 加算
    Sub,            // 減算
    Mul,            // 乗算
    Div,            // 除算
    Mod,            // 剰余
    Pow,            // 累乗
    Inc,            // インクリメント
    Dec,            // デクリメント
    Neg,            // 符号反転
    
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
    Equal,          // 等価 (==)
    NotEqual,       // 不等価 (!=)
    StrictEqual,    // 厳密等価 (===)
    StrictNotEqual, // 厳密不等価 (!==)
    LessThan,       // 未満 (<)
    LessEqual,      // 以下 (<=)
    GreaterThan,    // 超過 (>)
    GreaterEqual,   // 以上 (>=)
    
    // 制御フロー
    Jump,           // 無条件ジャンプ (operand: ジャンプ先オフセット)
    JumpIfTrue,     // 条件付きジャンプ (条件が真) (operand: ジャンプ先オフセット)
    JumpIfFalse,    // 条件付きジャンプ (条件が偽) (operand: ジャンプ先オフセット)
    
    // 関数操作
    Call,           // 関数呼び出し (operand: 引数の数)
    Return,         // 関数からリターン
    
    // オブジェクト操作
    CreateObject,   // 空のオブジェクトを作成
    CreateArray,    // 配列を作成 (operand: 要素数)
    
    // イテレーション
    StartForIn,     // for-in ループの開始
    ForInNext,      // for-in ループの次の要素
    
    // 例外操作
    Throw,          // 例外をスロー
    EnterTry,       // try ブロックに入る (operand: ハンドラのオフセット)
    LeaveTry,       // try ブロックを出る
    EnterCatch,     // catch ブロックに入る
    LeaveCatch,     // catch ブロックを出る
    EnterFinally,   // finally ブロックに入る
    LeaveFinally,   // finally ブロックを出る
    
    // その他
    TypeOf,         // typeof 演算子
    InstanceOf,     // instanceof 演算子
    In,             // in 演算子
    
    // デバッグ
    Debugger,       // デバッガーを起動
    
    // 終了
    End             // プログラム終了
};

// オペコードを文字列に変換する関数
inline std::string BytecodeOpcodeToString(BytecodeOpcode opcode) {
    switch (opcode) {
        case BytecodeOpcode::Nop: return "Nop";
        case BytecodeOpcode::Pop: return "Pop";
        case BytecodeOpcode::Dup: return "Dup";
        case BytecodeOpcode::Swap: return "Swap";
        
        case BytecodeOpcode::PushUndefined: return "PushUndefined";
        case BytecodeOpcode::PushNull: return "PushNull";
        case BytecodeOpcode::PushTrue: return "PushTrue";
        case BytecodeOpcode::PushFalse: return "PushFalse";
        case BytecodeOpcode::PushZero: return "PushZero";
        case BytecodeOpcode::PushOne: return "PushOne";
        case BytecodeOpcode::PushConst: return "PushConst";
        
        case BytecodeOpcode::LoadLocal: return "LoadLocal";
        case BytecodeOpcode::StoreLocal: return "StoreLocal";
        case BytecodeOpcode::LoadArg: return "LoadArg";
        case BytecodeOpcode::StoreArg: return "StoreArg";
        case BytecodeOpcode::LoadGlobal: return "LoadGlobal";
        case BytecodeOpcode::StoreGlobal: return "StoreGlobal";
        
        case BytecodeOpcode::LoadProp: return "LoadProp";
        case BytecodeOpcode::StoreProp: return "StoreProp";
        case BytecodeOpcode::DeleteProp: return "DeleteProp";
        case BytecodeOpcode::HasProp: return "HasProp";
        case BytecodeOpcode::LoadElem: return "LoadElem";
        case BytecodeOpcode::StoreElem: return "StoreElem";
        case BytecodeOpcode::DeleteElem: return "DeleteElem";
        
        case BytecodeOpcode::Add: return "Add";
        case BytecodeOpcode::Sub: return "Sub";
        case BytecodeOpcode::Mul: return "Mul";
        case BytecodeOpcode::Div: return "Div";
        case BytecodeOpcode::Mod: return "Mod";
        case BytecodeOpcode::Pow: return "Pow";
        case BytecodeOpcode::Inc: return "Inc";
        case BytecodeOpcode::Dec: return "Dec";
        case BytecodeOpcode::Neg: return "Neg";
        
        case BytecodeOpcode::BitAnd: return "BitAnd";
        case BytecodeOpcode::BitOr: return "BitOr";
        case BytecodeOpcode::BitXor: return "BitXor";
        case BytecodeOpcode::BitNot: return "BitNot";
        case BytecodeOpcode::ShiftLeft: return "ShiftLeft";
        case BytecodeOpcode::ShiftRight: return "ShiftRight";
        case BytecodeOpcode::ShiftRightUnsigned: return "ShiftRightUnsigned";
        
        case BytecodeOpcode::LogicalAnd: return "LogicalAnd";
        case BytecodeOpcode::LogicalOr: return "LogicalOr";
        case BytecodeOpcode::LogicalNot: return "LogicalNot";
        
        case BytecodeOpcode::Equal: return "Equal";
        case BytecodeOpcode::NotEqual: return "NotEqual";
        case BytecodeOpcode::StrictEqual: return "StrictEqual";
        case BytecodeOpcode::StrictNotEqual: return "StrictNotEqual";
        case BytecodeOpcode::LessThan: return "LessThan";
        case BytecodeOpcode::LessEqual: return "LessEqual";
        case BytecodeOpcode::GreaterThan: return "GreaterThan";
        case BytecodeOpcode::GreaterEqual: return "GreaterEqual";
        
        case BytecodeOpcode::Jump: return "Jump";
        case BytecodeOpcode::JumpIfTrue: return "JumpIfTrue";
        case BytecodeOpcode::JumpIfFalse: return "JumpIfFalse";
        
        case BytecodeOpcode::Call: return "Call";
        case BytecodeOpcode::Return: return "Return";
        
        case BytecodeOpcode::CreateObject: return "CreateObject";
        case BytecodeOpcode::CreateArray: return "CreateArray";
        
        case BytecodeOpcode::StartForIn: return "StartForIn";
        case BytecodeOpcode::ForInNext: return "ForInNext";
        
        case BytecodeOpcode::Throw: return "Throw";
        case BytecodeOpcode::EnterTry: return "EnterTry";
        case BytecodeOpcode::LeaveTry: return "LeaveTry";
        case BytecodeOpcode::EnterCatch: return "EnterCatch";
        case BytecodeOpcode::LeaveCatch: return "LeaveCatch";
        case BytecodeOpcode::EnterFinally: return "EnterFinally";
        case BytecodeOpcode::LeaveFinally: return "LeaveFinally";
        
        case BytecodeOpcode::TypeOf: return "TypeOf";
        case BytecodeOpcode::InstanceOf: return "InstanceOf";
        case BytecodeOpcode::In: return "In";
        
        case BytecodeOpcode::Debugger: return "Debugger";
        
        case BytecodeOpcode::End: return "End";
        
        default: return "Unknown";
    }
}

// オペコードのオペランド数を取得する関数
inline uint8_t GetBytecodecOpcodeOperandCount(BytecodeOpcode opcode) {
    switch (opcode) {
        // オペランドを持たない命令
        case BytecodeOpcode::Nop:
        case BytecodeOpcode::Pop:
        case BytecodeOpcode::Dup:
        case BytecodeOpcode::Swap:
        case BytecodeOpcode::PushUndefined:
        case BytecodeOpcode::PushNull:
        case BytecodeOpcode::PushTrue:
        case BytecodeOpcode::PushFalse:
        case BytecodeOpcode::PushZero:
        case BytecodeOpcode::PushOne:
        case BytecodeOpcode::Add:
        case BytecodeOpcode::Sub:
        case BytecodeOpcode::Mul:
        case BytecodeOpcode::Div:
        case BytecodeOpcode::Mod:
        case BytecodeOpcode::Pow:
        case BytecodeOpcode::Inc:
        case BytecodeOpcode::Dec:
        case BytecodeOpcode::Neg:
        case BytecodeOpcode::BitAnd:
        case BytecodeOpcode::BitOr:
        case BytecodeOpcode::BitXor:
        case BytecodeOpcode::BitNot:
        case BytecodeOpcode::ShiftLeft:
        case BytecodeOpcode::ShiftRight:
        case BytecodeOpcode::ShiftRightUnsigned:
        case BytecodeOpcode::LogicalAnd:
        case BytecodeOpcode::LogicalOr:
        case BytecodeOpcode::LogicalNot:
        case BytecodeOpcode::Equal:
        case BytecodeOpcode::NotEqual:
        case BytecodeOpcode::StrictEqual:
        case BytecodeOpcode::StrictNotEqual:
        case BytecodeOpcode::LessThan:
        case BytecodeOpcode::LessEqual:
        case BytecodeOpcode::GreaterThan:
        case BytecodeOpcode::GreaterEqual:
        case BytecodeOpcode::Return:
        case BytecodeOpcode::CreateObject:
        case BytecodeOpcode::LoadElem:
        case BytecodeOpcode::StoreElem:
        case BytecodeOpcode::DeleteElem:
        case BytecodeOpcode::Throw:
        case BytecodeOpcode::LeaveTry:
        case BytecodeOpcode::EnterCatch:
        case BytecodeOpcode::LeaveCatch:
        case BytecodeOpcode::EnterFinally:
        case BytecodeOpcode::LeaveFinally:
        case BytecodeOpcode::TypeOf:
        case BytecodeOpcode::InstanceOf:
        case BytecodeOpcode::In:
        case BytecodeOpcode::Debugger:
        case BytecodeOpcode::End:
        case BytecodeOpcode::ForInNext:
        case BytecodeOpcode::StartForIn:
            return 0;
            
        // 1つのオペランドを持つ命令
        case BytecodeOpcode::PushConst:
        case BytecodeOpcode::LoadLocal:
        case BytecodeOpcode::StoreLocal:
        case BytecodeOpcode::LoadArg:
        case BytecodeOpcode::StoreArg:
        case BytecodeOpcode::LoadGlobal:
        case BytecodeOpcode::StoreGlobal:
        case BytecodeOpcode::LoadProp:
        case BytecodeOpcode::StoreProp:
        case BytecodeOpcode::DeleteProp:
        case BytecodeOpcode::HasProp:
        case BytecodeOpcode::Jump:
        case BytecodeOpcode::JumpIfTrue:
        case BytecodeOpcode::JumpIfFalse:
        case BytecodeOpcode::Call:
        case BytecodeOpcode::CreateArray:
        case BytecodeOpcode::EnterTry:
            return 1;
            
        default:
            return 0; // 未知のオペコードの場合はデフォルト値
    }
}

// オペコードの情報を格納する構造体
struct BytecodeOpcodeInfo {
    std::string name;                // オペコードの名前
    uint8_t operandCount;            // オペランドの数
    bool hasStackEffect;             // スタックに影響を与えるか
    int8_t stackEffect;              // スタックの変化量（正: プッシュ、負: ポップ）
    bool isJump;                     // ジャンプ命令か
    bool isConditionalJump;          // 条件付きジャンプ命令か
    bool isTerminator;               // 制御フローの終端か（リターンなど）
    bool hasSideEffect;              // 副作用があるか
};

// オペコード情報のテーブルを提供するクラス
class BytecodeOpcodeTable {
public:
    static const BytecodeOpcodeInfo& GetInfo(BytecodeOpcode opcode) {
        static BytecodeOpcodeTable instance;
        return instance.m_opcodeInfoTable[static_cast<uint8_t>(opcode)];
    }

    static bool IsValid(BytecodeOpcode opcode) {
        return opcode < BytecodeOpcode::End;
    }

    static const char* GetName(BytecodeOpcode opcode) {
        if (!IsValid(opcode)) {
            return "INVALID";
        }
        return GetInfo(opcode).name.c_str();
    }

    static uint8_t GetOperandCount(BytecodeOpcode opcode) {
        if (!IsValid(opcode)) {
            return 0;
        }
        return GetInfo(opcode).operandCount;
    }

    static int8_t GetStackEffect(BytecodeOpcode opcode) {
        if (!IsValid(opcode)) {
            return 0;
        }
        return GetInfo(opcode).stackEffect;
    }

    static bool IsJump(BytecodeOpcode opcode) {
        if (!IsValid(opcode)) {
            return false;
        }
        return GetInfo(opcode).isJump;
    }

    static bool IsConditionalJump(BytecodeOpcode opcode) {
        if (!IsValid(opcode)) {
            return false;
        }
        return GetInfo(opcode).isConditionalJump;
    }

    static bool IsTerminator(BytecodeOpcode opcode) {
        if (!IsValid(opcode)) {
            return false;
        }
        return GetInfo(opcode).isTerminator;
    }

    static bool HasSideEffect(BytecodeOpcode opcode) {
        if (!IsValid(opcode)) {
            return false;
        }
        return GetInfo(opcode).hasSideEffect;
    }

private:
    BytecodeOpcodeTable() {
        InitializeOpcodeInfoTable();
    }

    void InitializeOpcodeInfoTable() {
        // デフォルト値で初期化
        m_opcodeInfoTable.resize(static_cast<size_t>(BytecodeOpcode::End));

        // 各オペコードの情報を設定
        // Nop
        SetOpcodeInfo(BytecodeOpcode::Nop, "Nop", 0, 0, false, false, false, false);

        // スタック操作
        SetOpcodeInfo(BytecodeOpcode::Pop, "Pop", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::Dup, "Dup", 0, 1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::Swap, "Swap", 0, 0, false, false, false, false);

        // 定数・リテラル
        SetOpcodeInfo(BytecodeOpcode::PushUndefined, "PushUndefined", 0, 1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::PushNull, "PushNull", 0, 1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::PushTrue, "PushTrue", 0, 1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::PushFalse, "PushFalse", 0, 1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::PushZero, "PushZero", 0, 1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::PushOne, "PushOne", 0, 1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::PushConst, "PushConst", 1, 1, false, false, false, false);

        // 変数操作
        SetOpcodeInfo(BytecodeOpcode::LoadLocal, "LoadLocal", 1, 1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::StoreLocal, "StoreLocal", 1, -1, false, false, false, true);
        SetOpcodeInfo(BytecodeOpcode::LoadArg, "LoadArg", 1, 1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::StoreArg, "StoreArg", 1, -1, false, false, false, true);
        SetOpcodeInfo(BytecodeOpcode::LoadGlobal, "LoadGlobal", 1, 1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::StoreGlobal, "StoreGlobal", 1, -1, false, false, false, true);

        // プロパティアクセス
        SetOpcodeInfo(BytecodeOpcode::LoadProp, "LoadProp", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::StoreProp, "StoreProp", 0, -3, false, false, false, true);
        SetOpcodeInfo(BytecodeOpcode::DeleteProp, "DeleteProp", 0, -1, false, false, false, true);
        SetOpcodeInfo(BytecodeOpcode::HasProp, "HasProp", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::LoadElem, "LoadElem", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::StoreElem, "StoreElem", 0, -3, false, false, false, true);
        SetOpcodeInfo(BytecodeOpcode::DeleteElem, "DeleteElem", 0, -1, false, false, false, true);

        // 算術演算
        SetOpcodeInfo(BytecodeOpcode::Add, "Add", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::Sub, "Sub", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::Mul, "Mul", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::Div, "Div", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::Mod, "Mod", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::Pow, "Pow", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::Inc, "Inc", 0, 0, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::Dec, "Dec", 0, 0, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::Neg, "Neg", 0, 0, false, false, false, false);

        // ビット演算
        SetOpcodeInfo(BytecodeOpcode::BitAnd, "BitAnd", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::BitOr, "BitOr", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::BitXor, "BitXor", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::BitNot, "BitNot", 0, 0, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::ShiftLeft, "ShiftLeft", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::ShiftRight, "ShiftRight", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::ShiftRightUnsigned, "ShiftRightUnsigned", 0, -1, false, false, false, false);

        // 論理演算
        SetOpcodeInfo(BytecodeOpcode::LogicalAnd, "LogicalAnd", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::LogicalOr, "LogicalOr", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::LogicalNot, "LogicalNot", 0, 0, false, false, false, false);

        // 比較演算
        SetOpcodeInfo(BytecodeOpcode::Equal, "Equal", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::NotEqual, "NotEqual", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::StrictEqual, "StrictEqual", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::StrictNotEqual, "StrictNotEqual", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::LessThan, "LessThan", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::LessEqual, "LessEqual", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::GreaterThan, "GreaterThan", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::GreaterEqual, "GreaterEqual", 0, -1, false, false, false, false);

        // 制御フロー
        SetOpcodeInfo(BytecodeOpcode::Jump, "Jump", 1, 0, true, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::JumpIfTrue, "JumpIfTrue", 1, -1, true, true, false, false);
        SetOpcodeInfo(BytecodeOpcode::JumpIfFalse, "JumpIfFalse", 1, -1, true, true, false, false);

        // 関数操作
        SetOpcodeInfo(BytecodeOpcode::Call, "Call", 2, -1, false, false, false, true);
        SetOpcodeInfo(BytecodeOpcode::Return, "Return", 0, -1, false, false, true, true);

        // オブジェクト操作
        SetOpcodeInfo(BytecodeOpcode::CreateObject, "CreateObject", 0, 1, false, false, false, true);
        SetOpcodeInfo(BytecodeOpcode::CreateArray, "CreateArray", 1, 0, false, false, false, true);

        // イテレーション
        SetOpcodeInfo(BytecodeOpcode::StartForIn, "StartForIn", 1, 0, true, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::ForInNext, "ForInNext", 1, 0, true, false, false, false);

        // 例外操作
        SetOpcodeInfo(BytecodeOpcode::Throw, "Throw", 0, -1, false, false, true, true);
        SetOpcodeInfo(BytecodeOpcode::EnterTry, "EnterTry", 2, 0, false, false, false, true);
        SetOpcodeInfo(BytecodeOpcode::LeaveTry, "LeaveTry", 0, 0, false, false, false, true);
        SetOpcodeInfo(BytecodeOpcode::EnterCatch, "EnterCatch", 0, 1, false, false, false, true);
        SetOpcodeInfo(BytecodeOpcode::LeaveCatch, "LeaveCatch", 0, 0, false, false, false, true);
        SetOpcodeInfo(BytecodeOpcode::EnterFinally, "EnterFinally", 0, 0, false, false, false, true);
        SetOpcodeInfo(BytecodeOpcode::LeaveFinally, "LeaveFinally", 0, 0, false, false, false, true);

        // その他
        SetOpcodeInfo(BytecodeOpcode::TypeOf, "TypeOf", 0, 0, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::InstanceOf, "InstanceOf", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::In, "In", 0, -1, false, false, false, false);
        SetOpcodeInfo(BytecodeOpcode::Debugger, "Debugger", 0, 0, false, false, false, false);
    }

    void SetOpcodeInfo(BytecodeOpcode opcode, const std::string& name, uint8_t operandCount,
                      int8_t stackEffect, bool isJump, bool isConditionalJump,
                      bool isTerminator, bool hasSideEffect) {
        size_t index = static_cast<size_t>(opcode);
        if (index >= m_opcodeInfoTable.size()) {
            return;
        }

        BytecodeOpcodeInfo& info = m_opcodeInfoTable[index];
        info.name = name;
        info.operandCount = operandCount;
        info.hasStackEffect = (stackEffect != 0);
        info.stackEffect = stackEffect;
        info.isJump = isJump;
        info.isConditionalJump = isConditionalJump;
        info.isTerminator = isTerminator;
        info.hasSideEffect = hasSideEffect;
    }

    std::vector<BytecodeOpcodeInfo> m_opcodeInfoTable;
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_JIT_BYTECODE_BYTECODE_OPCODES_H 