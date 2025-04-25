#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include "bytecode_decoder.h"
#include "../ir/ir.h"
#include "../ir/ir_builder.h"
#include "register_allocator.h"

namespace aerojs {
namespace core {

/**
 * @class IRGenerator
 * 
 * @brief バイトコードからIR（中間表現）を生成するクラス
 * 
 * このクラスは、バイトコードを解析して、内部表現（IR）を生成します。
 * 生成されたIRは、後続のコード生成フェーズで使用されます。
 */
class IRGenerator {
public:
    /**
     * @brief コンストラクタ
     */
    IRGenerator();
    
    /**
     * @brief デストラクタ
     */
    ~IRGenerator();
    
    /**
     * @brief バイトコードからIRを生成する
     * 
     * @param bytecodes バイトコードバッファ
     * @param length バイトコードの長さ
     * @return 生成されたIR関数へのポインタ
     */
    std::unique_ptr<IRFunction> Generate(const uint8_t* bytecodes, size_t length);
    
    /**
     * @brief 内部状態をリセットする
     */
    void Reset();

private:
    /**
     * @brief バイトコード命令をIR命令に変換する
     * 
     * @param bytecode 変換するバイトコード命令
     * @return 変換が成功したかどうか
     */
    bool EmitIRForBytecode(const Bytecode& bytecode);
    
    /**
     * @brief Nop命令のIRを生成
     */
    void EmitNop();
    
    /**
     * @brief LoadConst命令のIRを生成
     * 
     * @param value 定数値
     * @return 結果の仮想レジスタID
     */
    uint32_t EmitLoadConst(int64_t value);
    
    /**
     * @brief LoadVar命令のIRを生成
     * 
     * @param varIndex 変数インデックス
     * @return 結果の仮想レジスタID
     */
    uint32_t EmitLoadVar(uint32_t varIndex);
    
    /**
     * @brief StoreVar命令のIRを生成
     * 
     * @param varIndex 変数インデックス
     * @param valueReg 値の仮想レジスタID
     */
    void EmitStoreVar(uint32_t varIndex, uint32_t valueReg);
    
    /**
     * @brief 算術命令のIRを生成
     * 
     * @param opcode IR命令のオペコード
     * @param leftReg 左オペランドの仮想レジスタID
     * @param rightReg 右オペランドの仮想レジスタID
     * @return 結果の仮想レジスタID
     */
    uint32_t EmitBinaryOp(Opcode opcode, uint32_t leftReg, uint32_t rightReg);
    
    /**
     * @brief 比較命令のIRを生成
     * 
     * @param opcode IR命令のオペコード
     * @param leftReg 左オペランドの仮想レジスタID
     * @param rightReg 右オペランドの仮想レジスタID
     * @return 結果の仮想レジスタID
     */
    uint32_t EmitCompare(Opcode opcode, uint32_t leftReg, uint32_t rightReg);
    
    /**
     * @brief ジャンプ命令のIRを生成
     * 
     * @param targetOffset ジャンプ先オフセット
     */
    void EmitJump(uint32_t targetOffset);
    
    /**
     * @brief 条件付きジャンプ命令のIRを生成
     * 
     * @param condReg 条件の仮想レジスタID
     * @param targetOffset ジャンプ先オフセット
     * @param jumpIfTrue 真の場合にジャンプするかどうか
     */
    void EmitCondJump(uint32_t condReg, uint32_t targetOffset, bool jumpIfTrue);
    
    /**
     * @brief 関数呼び出し命令のIRを生成
     * 
     * @param funcReg 関数の仮想レジスタID
     * @param args 引数の仮想レジスタIDリスト
     * @return 結果の仮想レジスタID
     */
    uint32_t EmitCall(uint32_t funcReg, const std::vector<uint32_t>& args);
    
    /**
     * @brief リターン命令のIRを生成
     * 
     * @param valueReg 戻り値の仮想レジスタID（存在する場合）
     */
    void EmitReturn(uint32_t valueReg = 0xFFFFFFFF);
    
    // バイトコードデコーダー
    BytecodeDecoder m_decoder;
    
    // IR関数
    std::unique_ptr<IRFunction> m_irFunction;
    
    // IRビルダー
    IRBuilder m_irBuilder;
    
    // レジスタアロケータ
    RegisterAllocator m_regAllocator;
    
    // 命令オフセットからIR命令へのマッピング
    std::unordered_map<uint32_t, size_t> m_offsetToIRMap;
    
    // スタック上の値を表す仮想レジスタのマッピング
    std::vector<uint32_t> m_stackRegs;
    
    // 変数を表す仮想レジスタのマッピング
    std::vector<uint32_t> m_varRegs;
    
    // 次の一時レジスタID
    uint32_t m_nextTempReg;
    
    // 現在処理中のバイトコードオフセット
    uint32_t m_currentOffset;
};

} // namespace core
} // namespace aerojs 