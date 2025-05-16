/**
 * @file ir_builder.h
 * @brief AeroJS JavaScript エンジンの中間表現(IR)ビルダーの定義
 * @version 1.0.0
 * @license MIT
 */

#ifndef AEROJS_IR_BUILDER_H
#define AEROJS_IR_BUILDER_H

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <set>

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class Function;
class Value;
class JITProfiler;

/**
 * @brief IR操作コード
 */
enum class IROpcode {
    // 制御フロー
    NoOp,           ///< 何もしない
    Jump,           ///< 無条件ジャンプ
    Branch,         ///< 条件分岐
    Return,         ///< 関数から戻る
    Throw,          ///< 例外をスロー
    Call,           ///< 関数呼び出し
    TailCall,       ///< 末尾呼び出し
    
    // 算術演算
    Add,            ///< 加算
    Sub,            ///< 減算
    Mul,            ///< 乗算
    Div,            ///< 除算
    Mod,            ///< 剰余
    Neg,            ///< 符号反転
    Inc,            ///< インクリメント
    Dec,            ///< デクリメント
    
    // ビット演算
    BitAnd,         ///< ビットAND
    BitOr,          ///< ビットOR
    BitXor,         ///< ビットXOR
    BitNot,         ///< ビット反転
    LeftShift,      ///< 左シフト
    RightShift,     ///< 右シフト
    UnsignedRightShift, ///< 符号なし右シフト
    
    // 論理演算
    LogicalAnd,     ///< 論理AND
    LogicalOr,      ///< 論理OR
    LogicalNot,     ///< 論理NOT
    
    // 比較演算
    Equal,          ///< 等価
    NotEqual,       ///< 非等価
    StrictEqual,    ///< 厳密等価
    StrictNotEqual, ///< 厳密非等価
    LessThan,       ///< 未満
    LessThanOrEqual, ///< 以下
    GreaterThan,    ///< 超過
    GreaterThanOrEqual, ///< 以上
    
    // メモリ操作
    LoadConst,      ///< 定数をロード
    LoadGlobal,     ///< グローバル変数ロード
    StoreGlobal,    ///< グローバル変数ストア
    LoadLocal,      ///< ローカル変数ロード
    StoreLocal,     ///< ローカル変数ストア
    LoadArg,        ///< 引数ロード
    StoreArg,       ///< 引数ストア
    LoadProperty,   ///< プロパティロード
    StoreProperty,  ///< プロパティストア
    LoadElement,    ///< 配列要素ロード
    StoreElement,   ///< 配列要素ストア
    
    // オブジェクト操作
    CreateObject,   ///< オブジェクト生成
    CreateArray,    ///< 配列生成
    CreateFunction, ///< 関数生成
    
    // 型操作
    TypeOf,         ///< 型情報取得
    InstanceOf,     ///< インスタンス確認
    TypeGuard,      ///< 型ガード
    
    // メタ
    Phi,            ///< PHIノード
    DebugPrint,     ///< デバッグ出力
    Bailout,        ///< 最適化解除
    
    // マーカー
    BlockBegin,     ///< ブロック開始マーカー
    BlockEnd        ///< ブロック終了マーカー
};

/**
 * @brief IR 命令タイプ
 */
enum class IRType {
    Void,
    Boolean,
    Int32,
    Int64,
    Float64,
    String,
    Object,
    Array,
    Function,
    Any
};

/**
 * @brief IR値のフラグ
 */
enum IRValueFlags {
    None        = 0,
    Constant    = 1 << 0,
    Reusable    = 1 << 1,
    Spilled     = 1 << 2,
    LiveOut     = 1 << 3,
    Eliminated  = 1 << 4,
    MustCheck   = 1 << 5
};

/**
 * @brief IR値 (SSAフォーム)
 */
struct IRValue {
    uint32_t id;          ///< 一意のID
    IRType type;          ///< 値の型
    uint32_t flags;       ///< フラグ
    int32_t refCount;     ///< 参照カウント
    
    IRValue()
        : id(0)
        , type(IRType::Any)
        , flags(0)
        , refCount(0)
    {}
    
    IRValue(uint32_t valueId, IRType valueType)
        : id(valueId)
        , type(valueType)
        , flags(0)
        , refCount(0)
    {}
    
    bool isConstant() const { return (flags & IRValueFlags::Constant) != 0; }
    bool isReusable() const { return (flags & IRValueFlags::Reusable) != 0; }
    bool isSpilled() const { return (flags & IRValueFlags::Spilled) != 0; }
    bool isLiveOut() const { return (flags & IRValueFlags::LiveOut) != 0; }
    bool isEliminated() const { return (flags & IRValueFlags::Eliminated) != 0; }
    bool needsCheck() const { return (flags & IRValueFlags::MustCheck) != 0; }
    
    void setFlag(IRValueFlags flag) { flags |= flag; }
    void clearFlag(IRValueFlags flag) { flags &= ~flag; }
};

/**
 * @brief IR命令
 */
struct IRInstruction {
    IROpcode opcode;                 ///< 操作コード
    IRValue* result;                 ///< 結果値
    std::vector<IRValue*> operands;  ///< オペランド
    uint32_t bytecodeOffset;         ///< 対応するバイトコードオフセット
    uint32_t lineNumber;             ///< ソースコード行番号
    std::string debugInfo;           ///< デバッグ情報
    
    // デオプティマイズ情報
    uint32_t deoptIndex;             ///< デオプティマイズインデックス
    
    IRInstruction()
        : opcode(IROpcode::NoOp)
        , result(nullptr)
        , bytecodeOffset(0)
        , lineNumber(0)
        , deoptIndex(0)
    {}
    
    IRInstruction(IROpcode op, IRValue* res = nullptr)
        : opcode(op)
        , result(res)
        , bytecodeOffset(0)
        , lineNumber(0)
        , deoptIndex(0)
    {}
    
    /**
     * @brief オペランドを追加
     * @param operand 追加するオペランド
     */
    void addOperand(IRValue* operand) {
        if (operand) {
            operands.push_back(operand);
            operand->refCount++;
        }
    }
    
    /**
     * @brief オペランドをクリア
     */
    void clearOperands() {
        for (auto operand : operands) {
            if (operand) {
                operand->refCount--;
            }
        }
        operands.clear();
    }
};

/**
 * @brief 基本ブロック
 */
struct IRBlock {
    uint32_t id;                         ///< ブロックID
    std::vector<IRInstruction*> instructions; ///< 命令リスト
    std::vector<IRBlock*> predecessors;  ///< 前任ブロック
    std::vector<IRBlock*> successors;    ///< 後続ブロック
    std::vector<IRValue*> phiValues;     ///< PHI値
    bool isLoopHeader;                  ///< ループヘッダーフラグ
    bool isHandler;                     ///< 例外ハンドラフラグ
    
    IRBlock()
        : id(0)
        , isLoopHeader(false)
        , isHandler(false)
    {}
    
    explicit IRBlock(uint32_t blockId)
        : id(blockId)
        , isLoopHeader(false)
        , isHandler(false)
    {}
    
    /**
     * @brief 命令を追加
     * @param instruction 追加する命令
     */
    void addInstruction(IRInstruction* instruction) {
        if (instruction) {
            instructions.push_back(instruction);
        }
    }
    
    /**
     * @brief 前任ブロックを追加
     * @param pred 追加する前任ブロック
     */
    void addPredecessor(IRBlock* pred) {
        if (pred && std::find(predecessors.begin(), predecessors.end(), pred) == predecessors.end()) {
            predecessors.push_back(pred);
        }
    }
    
    /**
     * @brief 後続ブロックを追加
     * @param succ 追加する後続ブロック
     */
    void addSuccessor(IRBlock* succ) {
        if (succ && std::find(successors.begin(), successors.end(), succ) == successors.end()) {
            successors.push_back(succ);
            succ->addPredecessor(this);
        }
    }
    
    /**
     * @brief PHI値を追加
     * @param value 追加するPHI値
     */
    void addPhiValue(IRValue* value) {
        if (value) {
            phiValues.push_back(value);
        }
    }
};

/**
 * @brief IR関数
 */
struct IRFunction {
    std::string name;                     ///< 関数名
    uint64_t functionId;                  ///< 関数ID
    std::vector<IRBlock*> blocks;         ///< 基本ブロック
    IRBlock* entryBlock;                  ///< エントリブロック
    IRBlock* exitBlock;                   ///< 終了ブロック
    std::vector<IRValue*> values;         ///< 全ての値
    std::vector<IRInstruction*> allInstructions; ///< 全ての命令
    std::unordered_map<uint32_t, IRValue*> arguments; ///< 引数
    std::unordered_map<uint32_t, IRValue*> locals;   ///< ローカル変数
    
    IRFunction()
        : functionId(0)
        , entryBlock(nullptr)
        , exitBlock(nullptr)
    {}
    
    /**
     * @brief 基本ブロックを追加
     * @param block 追加するブロック
     */
    void addBlock(IRBlock* block) {
        if (block) {
            blocks.push_back(block);
        }
    }
    
    /**
     * @brief 値を追加
     * @param value 追加する値
     */
    void addValue(IRValue* value) {
        if (value) {
            values.push_back(value);
        }
    }
    
    /**
     * @brief 命令を追加
     * @param instruction 追加する命令
     */
    void addInstruction(IRInstruction* instruction) {
        if (instruction) {
            allInstructions.push_back(instruction);
        }
    }
    
    /**
     * @brief 引数を追加
     * @param index 引数インデックス
     * @param value 値
     */
    void addArgument(uint32_t index, IRValue* value) {
        if (value) {
            arguments[index] = value;
        }
    }
    
    /**
     * @brief ローカル変数を追加
     * @param index 変数インデックス
     * @param value 値
     */
    void addLocal(uint32_t index, IRValue* value) {
        if (value) {
            locals[index] = value;
        }
    }
};

/**
 * @brief IRビルダークラス
 */
class IRBuilder {
public:
    /**
     * @brief コンストラクタ
     * @param context コンテキスト
     * @param profiler プロファイラ
     */
    IRBuilder(Context* context, JITProfiler* profiler);
    
    /**
     * @brief デストラクタ
     */
    ~IRBuilder();
    
    /**
     * @brief 関数からIRを構築
     * @param function 対象関数
     * @return 構築されたIR関数
     */
    IRFunction* build(Function* function);
    
    /**
     * @brief IR関数のダンプ
     * @param irFunction IR関数
     * @return ダンプされた文字列
     */
    std::string dumpIR(IRFunction* irFunction) const;
    
private:
    Context* m_context;
    JITProfiler* m_profiler;
    uint32_t m_nextValueId;
    uint32_t m_nextBlockId;
    
    IRValue* m_currentValue;
    IRBlock* m_currentBlock;
    IRFunction* m_currentFunction;
    
    // バイトコード解析状態
    std::vector<uint8_t> m_bytecode;
    uint32_t m_bytecodeIndex;
    uint32_t m_bytecodeLength;
    
    // ブロック管理
    std::unordered_map<uint32_t, IRBlock*> m_blockMap;
    std::set<uint32_t> m_blockStarts;
    
    // 内部ヘルパーメソッド
    void scan(Function* function);
    void buildBlocks();
    void processFunction(Function* function);
    
    // 制御フローグラフ構築とSSA変換関連のメソッド
    void buildControlFlowGraph();
    void detectLoops();
    void detectLoopsRecursive(IRBlock* block, std::vector<bool>& visited, std::vector<bool>& inStack);
    void identifyExceptionHandlers(Function* function);
    void removeUnreachableBlocks();
    void insertPhiNodes();
    IRValue* getLastDefinition(IRBlock* block, uint32_t varId);
    
    IRBlock* createBlock();
    IRValue* createValue(IRType type);
    IRInstruction* createInstruction(IROpcode opcode, IRValue* result = nullptr);
    
    IRValue* loadConstant(const Value& value);
    IRValue* loadLocal(uint32_t index);
    void storeLocal(uint32_t index, IRValue* value);
    IRValue* loadArg(uint32_t index);
    void storeArg(uint32_t index, IRValue* value);
    
    void emitBranch(IRValue* condition, IRBlock* trueBlock, IRBlock* falseBlock);
    void emitJump(IRBlock* target);
    void emitReturn(IRValue* value);
    IRValue* emitCall(IRValue* callee, const std::vector<IRValue*>& args);
    
    // バイトコード命令処理
    void processNextInstruction();
    void processBinaryOp(IROpcode opcode);
    void processUnaryOp(IROpcode opcode);
    void processCompare(IROpcode opcode);
    void processJump(int32_t offset);
    void processConditionalJump(int32_t offset, bool condition);
    
    // ユーティリティ
    void markBlockStart(uint32_t bytecodeOffset);
    IRBlock* getOrCreateBlockAt(uint32_t bytecodeOffset);
    uint8_t readByte();
    uint16_t readWord();
    uint32_t readDWord();
    int32_t readSignedDWord();
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_IR_BUILDER_H 