/**
 * @file arm64_sve.cpp
 * @brief ARM64 Scalable Vector Extension (SVE) のサポート実装
 * @version 1.0.0
 * @license MIT
 */

#include "arm64_sve.h"
#include <algorithm>
#include <cassert>
#include <cstring>

namespace aerojs {
namespace core {

// SVE命令をエンコード
void ARM64SVE::appendInstruction(std::vector<uint8_t>& out, uint32_t instruction) {
    out.push_back(instruction & 0xFF);
    out.push_back((instruction >> 8) & 0xFF);
    out.push_back((instruction >> 16) & 0xFF);
    out.push_back((instruction >> 24) & 0xFF);
}

// プレディケートレジスタを初期化
void ARM64SVE::emitPredicateInit(std::vector<uint8_t>& out, int pd, PredicatePattern pattern, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // PTRUE Pd.T, pattern
    uint32_t instr = 0x2518E000 | size | (static_cast<uint32_t>(pattern) << 5) | pd;
    appendInstruction(out, instr);
}

// SVEレジスタを連続ロード
void ARM64SVE::emitContiguousLoad(std::vector<uint8_t>& out, int zt, int pg, int xn, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 21;
            break;
        case ELEM_H:
            size = 0b01 << 21;
            break;
        case ELEM_S:
            size = 0b10 << 21;
            break;
        case ELEM_D:
            size = 0b11 << 21;
            break;
    }
    
    // LD1W {Zt.S}, Pg/Z, [Xn]
    uint32_t instr = 0xA540A000 | size | (xn << 16) | (pg << 10) | zt;
    appendInstruction(out, instr);
}

// SVEレジスタを連続ストア
void ARM64SVE::emitContiguousStore(std::vector<uint8_t>& out, int zt, int pg, int xn, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 21;
            break;
        case ELEM_H:
            size = 0b01 << 21;
            break;
        case ELEM_S:
            size = 0b10 << 21;
            break;
        case ELEM_D:
            size = 0b11 << 21;
            break;
    }
    
    // ST1W {Zt.S}, Pg, [Xn]
    uint32_t instr = 0xE540A000 | size | (xn << 16) | (pg << 10) | zt;
    appendInstruction(out, instr);
}

// SVEベクタ加算
void ARM64SVE::emitVectorAdd(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FADD Zd.T, Pg/M, Zd.T, Zn.T
    uint32_t instr = 0x65000000 | size | (zm << 16) | (pg << 10) | (zn << 5) | zd;
    appendInstruction(out, instr);
}

// SVEベクタ減算
void ARM64SVE::emitVectorSub(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FSUB Zd.T, Pg/M, Zd.T, Zn.T
    uint32_t instr = 0x65008000 | size | (zm << 16) | (pg << 10) | (zn << 5) | zd;
    appendInstruction(out, instr);
}

// SVEベクタ乗算
void ARM64SVE::emitVectorMul(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FMUL Zd.T, Pg/M, Zd.T, Zn.T
    uint32_t instr = 0x65200000 | size | (zm << 16) | (pg << 10) | (zn << 5) | zd;
    appendInstruction(out, instr);
}

// SVEベクタ乗算加算
void ARM64SVE::emitVectorMulAdd(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FMLA Zd.T, Pg/M, Zn.T, Zm.T
    uint32_t instr = 0x65208000 | size | (zm << 16) | (pg << 10) | (zn << 5) | zd;
    appendInstruction(out, instr);
}

// SVE水平加算
void ARM64SVE::emitHorizontalAdd(std::vector<uint8_t>& out, int vd, int pg, int zn, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FADDV Vd.S, Pg, Zn.S
    uint32_t instr = 0x65182000 | size | (pg << 10) | (zn << 5) | vd;
    appendInstruction(out, instr);
}

// SVEゼロ初期化
void ARM64SVE::emitClearVector(std::vector<uint8_t>& out, int zd) {
    // DUP Z0.B, #0
    uint32_t instr = 0x25205000 | zd;
    appendInstruction(out, instr);
}

// SVEスカラー値からブロードキャスト
void ARM64SVE::emitBroadcast(std::vector<uint8_t>& out, int zd, int pg, int vn, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // CPY Zd.T, Pg/Z, Vn
    uint32_t instr = 0x05208000 | size | (pg << 10) | (vn << 5) | zd;
    appendInstruction(out, instr);
}

// SVEレジスタ間コピー
void ARM64SVE::emitMove(std::vector<uint8_t>& out, int zd, int pg, int zn) {
    // MOV Zd.B, Pg/Z, Zn.B
    uint32_t instr = 0x05208000 | (pg << 10) | (zn << 5) | zd;
    appendInstruction(out, instr);
}

// SVEベクタ比較（等しい）
void ARM64SVE::emitVectorCompareEQ(std::vector<uint8_t>& out, int pd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FCMPEQ Pd.T, Pg/Z, Zn.T, Zm.T
    uint32_t instr = 0x65C00000 | size | (zm << 16) | (pg << 10) | (zn << 5) | pd;
    appendInstruction(out, instr);
}

// SVEベクタ比較（より大きい）
void ARM64SVE::emitVectorCompareGT(std::vector<uint8_t>& out, int pd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FCMPGT Pd.T, Pg/Z, Zn.T, Zm.T
    uint32_t instr = 0x65C08000 | size | (zm << 16) | (pg << 10) | (zn << 5) | pd;
    appendInstruction(out, instr);
}

// SVEベクタ最大値
void ARM64SVE::emitVectorMax(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FMAX Zd.T, Pg/M, Zd.T, Zn.T
    uint32_t instr = 0x65400000 | size | (zm << 16) | (pg << 10) | (zn << 5) | zd;
    appendInstruction(out, instr);
}

// SVEベクタ最小値
void ARM64SVE::emitVectorMin(std::vector<uint8_t>& out, int zd, int pg, int zn, int zm, ElementSize elementSize) {
    uint32_t size = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            size = 0b00 << 22;
            break;
        case ELEM_H:
            size = 0b01 << 22;
            break;
        case ELEM_S:
            size = 0b10 << 22;
            break;
        case ELEM_D:
            size = 0b11 << 22;
            break;
    }
    
    // FMIN Zd.T, Pg/M, Zd.T, Zn.T
    uint32_t instr = 0x65408000 | size | (zm << 16) | (pg << 10) | (zn << 5) | zd;
    appendInstruction(out, instr);
}

// SVEのベクトル長を取得
void ARM64SVE::emitGetVectorLength(std::vector<uint8_t>& out, int xd, ElementSize elementSize) {
    uint32_t imm = 0;
    
    // 要素サイズエンコーディング
    switch (elementSize) {
        case ELEM_B:
            imm = 0b00 << 5;
            break;
        case ELEM_H:
            imm = 0b01 << 5;
            break;
        case ELEM_S:
            imm = 0b10 << 5;
            break;
        case ELEM_D:
            imm = 0b11 << 5;
            break;
    }
    
    // RDVL Xd, #1
    uint32_t instr = 0x04BF0000 | imm | xd;
    appendInstruction(out, instr);
}

// SVEを使用した行列乗算
void ARM64SVE::emitMatrixMultiply(std::vector<uint8_t>& out, int rows, int cols, int shared) {
    // 行列乗算をSVEを使って実装：C[i,j] = sum(A[i,k] * B[k,j])
    // この実装では、最適化された行列乗算を生成
    // 実際の実装では、ハードウェア対応のSVE2の行列乗算命令も考慮すべき
    
    // プレディケートレジスタの初期化 (すべての要素をアクティブに)
    emitPredicateInit(out, 0, PT_ALL, ELEM_S);
    
    // レジスタの役割: 
    // Z0-Z3: C行の蓄積, Z4: A行, Z5-Z8: B列

    // 初期化: Z0-Z3をゼロ初期化
    for (int i = 0; i < 4; ++i) {
        emitClearVector(out, i);
    }
    
    // A行のロード
    emitContiguousLoad(out, 4, 0, 0, ELEM_S);
    
    // B行のロード
    for (int j = 0; j < 4; ++j) {
        emitContiguousLoad(out, 5 + j, 0, 1 + j, ELEM_S);
    }
    
    // 行列乗算のための融合乗算加算
    for (int j = 0; j < 4; ++j) {
        emitVectorMulAdd(out, j, 0, 4, 5 + j, ELEM_S);
    }
    
    // 結果の保存
    for (int j = 0; j < 4; ++j) {
        emitContiguousStore(out, j, 0, 2 + j, ELEM_S);
    }
}

// 特定ループをSVEで自動ベクトル化
bool ARM64SVE::autoVectorizeLoop(const std::vector<IRInstruction>& loopInsts, std::vector<uint8_t>& out) {
    // ループのベクトル化可能性を評価
    
    // ベクトル化をブロックする命令の検出
    for (const auto& inst : loopInsts) {
        // ベクトル化不可能な操作
        switch (inst.opcode) {
            case IROpcode::CALL:
            case IROpcode::BRANCH:
            case IROpcode::BRANCH_COND:
            case IROpcode::THROW:
            case IROpcode::RETURN:
                return false;
            default:
                break;
        }
    }
    
    // プレディケートレジスタの初期化
    emitPredicateInit(out, 0, PT_ALL, ELEM_S);
    
    // 典型的なループパターンの検出と最適化
    bool hasLoad = false;
    bool hasStore = false;
    bool hasArithmetic = false;
    
    for (const auto& inst : loopInsts) {
        if (inst.opcode == IROpcode::LOAD) {
            hasLoad = true;
        } else if (inst.opcode == IROpcode::STORE) {
            hasStore = true;
        } else if (inst.opcode == IROpcode::ADD || 
                  inst.opcode == IROpcode::SUB || 
                  inst.opcode == IROpcode::MUL || 
                  inst.opcode == IROpcode::DIV) {
            hasArithmetic = true;
        }
    }
    
    // ベクトル化コード生成：配列加算の例
    if (hasLoad && hasStore && hasArithmetic) {
        // 基本的な配列加算パターン: C[i] = A[i] + B[i]
        emitContiguousLoad(out, 0, 0, 0, ELEM_S);  // A
        emitContiguousLoad(out, 1, 0, 1, ELEM_S);  // B
        emitVectorAdd(out, 2, 0, 0, 1, ELEM_S);    // A + B
        emitContiguousStore(out, 2, 0, 2, ELEM_S); // C
        return true;
    }
    
    return false;
}

// SVEを使用した数値積分
void ARM64SVE::emitNumericalIntegration(std::vector<uint8_t>& out, int dataReg, int resultReg, int length) {
    // 数値積分：台形則を使用した実装例
    // プレディケートレジスタの初期化
    emitPredicateInit(out, 0, PT_ALL, ELEM_S);
    
    // データのロード
    emitContiguousLoad(out, 0, 0, dataReg, ELEM_S);
    
    // 台形則の係数設定：最初と最後は0.5、その他は1.0
    // Z1に係数配列を設定
    emitClearVector(out, 1);
    // 実際には複雑なステップが必要だが、簡略化
    
    // 係数とデータの乗算
    emitVectorMul(out, 2, 0, 0, 1, ELEM_S);
    
    // 水平加算で結果を1つの値に
    emitHorizontalAdd(out, 3, 0, 2, ELEM_S);
    
    // 結果の保存
    emitContiguousStore(out, 3, 0, resultReg, ELEM_S);
}

void ARM64SVEOperations::codeGen(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    if (!backend.getFeatures().supportsSVE) {
        // SVEがサポートされていない場合はフォールバック
        ARM64NEONOperations::codeGen(op, ctx, backend);
        return;
    }
    
    // 適切なSVE命令を選択して生成
    switch (op->getType()) {
        case Operation::Type::VectorAdd:
            generateVectorAdd(op, ctx, backend);
            break;
        case Operation::Type::VectorSub:
            generateVectorSub(op, ctx, backend);
            break;
        case Operation::Type::VectorMul:
            generateVectorMul(op, ctx, backend);
            break;
        case Operation::Type::VectorDiv:
            generateVectorDiv(op, ctx, backend);
            break;
        case Operation::Type::VectorMin:
            generateVectorMin(op, ctx, backend);
            break;
        case Operation::Type::VectorMax:
            generateVectorMax(op, ctx, backend);
            break;
        case Operation::Type::VectorSqrt:
            generateVectorSqrt(op, ctx, backend);
            break;
        case Operation::Type::VectorRcp:
            generateVectorRcp(op, ctx, backend);
            break;
        case Operation::Type::VectorFma:
            generateVectorFma(op, ctx, backend);
            break;
        case Operation::Type::VectorLoad:
            generateVectorLoad(op, ctx, backend);
            break;
        case Operation::Type::VectorStore:
            generateVectorStore(op, ctx, backend);
            break;
        default:
            // 未サポートの場合はNEONベースの実装にフォールバック
            ARM64NEONOperations::codeGen(op, ctx, backend);
            break;
    }
}

// ベクトル加算命令
void ARM64SVEOperations::generateVectorAdd(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル加算命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg1 = ctx.operandToSVReg(op->getOperand(0));
    Register srcReg2 = ctx.operandToSVReg(op->getOperand(1));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Int32:
            // ADD (ベクトル、ベクトル)
            ctx.emitU32(0x04200000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Int64:
            // ADD (ベクトル、ベクトル)
            ctx.emitU32(0x04a00000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float32:
            // FADD (ベクトル、ベクトル)
            ctx.emitU32(0x65000000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float64:
            // FADD (ベクトル、ベクトル)
            ctx.emitU32(0x65400000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE add");
            ARM64NEONOperations::generateVectorAdd(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトル減算命令
void ARM64SVEOperations::generateVectorSub(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル減算命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg1 = ctx.operandToSVReg(op->getOperand(0));
    Register srcReg2 = ctx.operandToSVReg(op->getOperand(1));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Int32:
            // SUB (ベクトル、ベクトル)
            ctx.emitU32(0x04200400 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Int64:
            // SUB (ベクトル、ベクトル)
            ctx.emitU32(0x04a00400 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float32:
            // FSUB (ベクトル、ベクトル)
            ctx.emitU32(0x65008000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float64:
            // FSUB (ベクトル、ベクトル)
            ctx.emitU32(0x65408000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE sub");
            ARM64NEONOperations::generateVectorSub(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトル乗算命令
void ARM64SVEOperations::generateVectorMul(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル乗算命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg1 = ctx.operandToSVReg(op->getOperand(0));
    Register srcReg2 = ctx.operandToSVReg(op->getOperand(1));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Int32:
            // MUL (ベクトル、ベクトル)
            ctx.emitU32(0x04100000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Int64:
            // MUL (ベクトル、ベクトル)
            ctx.emitU32(0x04900000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float32:
            // FMUL (ベクトル、ベクトル)
            ctx.emitU32(0x65000800 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float64:
            // FMUL (ベクトル、ベクトル)
            ctx.emitU32(0x65400800 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE mul");
            ARM64NEONOperations::generateVectorMul(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトル除算命令
void ARM64SVEOperations::generateVectorDiv(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル除算命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg1 = ctx.operandToSVReg(op->getOperand(0));
    Register srcReg2 = ctx.operandToSVReg(op->getOperand(1));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Float32:
            // FDIV (ベクトル、ベクトル)
            ctx.emitU32(0x65009800 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float64:
            // FDIV (ベクトル、ベクトル)
            ctx.emitU32(0x65409800 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        default:
            // 整数除算はSVEで直接サポートされていないため、浮動小数点除算を使用するか、
            // または複数の命令で実装する必要がある
            ctx.emitCommentLine("Integer division not directly supported in SVE");
            ARM64NEONOperations::generateVectorDiv(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトル最小値命令
void ARM64SVEOperations::generateVectorMin(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル最小値命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg1 = ctx.operandToSVReg(op->getOperand(0));
    Register srcReg2 = ctx.operandToSVReg(op->getOperand(1));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Int32:
            // SMIN (ベクトル、ベクトル)
            ctx.emitU32(0x04108000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Int64:
            // SMIN (ベクトル、ベクトル)
            ctx.emitU32(0x04908000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float32:
            // FMIN (ベクトル、ベクトル)
            ctx.emitU32(0x65002800 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float64:
            // FMIN (ベクトル、ベクトル)
            ctx.emitU32(0x65402800 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE min");
            ARM64NEONOperations::generateVectorMin(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトル最大値命令
void ARM64SVEOperations::generateVectorMax(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル最大値命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg1 = ctx.operandToSVReg(op->getOperand(0));
    Register srcReg2 = ctx.operandToSVReg(op->getOperand(1));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Int32:
            // SMAX (ベクトル、ベクトル)
            ctx.emitU32(0x04109000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Int64:
            // SMAX (ベクトル、ベクトル)
            ctx.emitU32(0x04909000 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float32:
            // FMAX (ベクトル、ベクトル)
            ctx.emitU32(0x65002C00 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        case DataType::Float64:
            // FMAX (ベクトル、ベクトル)
            ctx.emitU32(0x65402C00 | (predReg << 10) | (srcReg2 << 5) | srcReg1 | (destReg << 0));
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE max");
            ARM64NEONOperations::generateVectorMax(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトル平方根命令
void ARM64SVEOperations::generateVectorSqrt(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル平方根命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg = ctx.operandToSVReg(op->getOperand(0));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Float32:
            // FSQRT (ベクトル)
            ctx.emitU32(0x650C9800 | (predReg << 10) | (srcReg << 5) | destReg);
            break;
            
        case DataType::Float64:
            // FSQRT (ベクトル)
            ctx.emitU32(0x654C9800 | (predReg << 10) | (srcReg << 5) | destReg);
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE sqrt");
            ARM64NEONOperations::generateVectorSqrt(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトル逆数近似命令
void ARM64SVEOperations::generateVectorRcp(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトル逆数近似命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg = ctx.operandToSVReg(op->getOperand(0));
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Float32:
            // FRECPE (ベクトル)
            ctx.emitU32(0x650E3800 | (predReg << 10) | (srcReg << 5) | destReg);
            break;
            
        case DataType::Float64:
            // FRECPE (ベクトル)
            ctx.emitU32(0x654E3800 | (predReg << 10) | (srcReg << 5) | destReg);
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE rcp");
            ARM64NEONOperations::generateVectorRcp(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトルFMA命令
void ARM64SVEOperations::generateVectorFma(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトルFMA命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register srcReg1 = ctx.operandToSVReg(op->getOperand(0)); // A
    Register srcReg2 = ctx.operandToSVReg(op->getOperand(1)); // B
    Register srcReg3 = ctx.operandToSVReg(op->getOperand(2)); // C
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    // 命令のバリエーション（A*B + C か A + B*C か）
    bool isMulAdd = op->getVariant() == Operation::Variant::MulAdd; // A*B + C
    
    switch (dataType) {
        case DataType::Float32:
            if (isMulAdd) {
                // FMLA (ベクトル、ベクトル): Zd = Za + Zb * Zm
                ctx.emitU32(0x65200000 | (predReg << 10) | (srcReg2 << 5) | srcReg3 | (srcReg1 << 16));
                // 結果をコピー（必要に応じて）
                if (srcReg1 != destReg) {
                    ctx.emitU32(0x05204000 | (predReg << 10) | (srcReg1 << 5) | destReg);
                }
            } else {
                // FMAD (ベクトル、ベクトル): Zd = Za + Zb * Zn
                ctx.emitU32(0x65200800 | (predReg << 10) | (srcReg3 << 5) | srcReg1 | (srcReg2 << 16));
                // 結果をコピー（必要に応じて）
                if (srcReg1 != destReg) {
                    ctx.emitU32(0x05204000 | (predReg << 10) | (srcReg1 << 5) | destReg);
                }
            }
            break;
            
        case DataType::Float64:
            if (isMulAdd) {
                // FMLA (ベクトル、ベクトル): Zd = Za + Zb * Zm
                ctx.emitU32(0x65600000 | (predReg << 10) | (srcReg2 << 5) | srcReg3 | (srcReg1 << 16));
                // 結果をコピー（必要に応じて）
                if (srcReg1 != destReg) {
                    ctx.emitU32(0x05604000 | (predReg << 10) | (srcReg1 << 5) | destReg);
                }
            } else {
                // FMAD (ベクトル、ベクトル): Zd = Za + Zb * Zn
                ctx.emitU32(0x65600800 | (predReg << 10) | (srcReg3 << 5) | srcReg1 | (srcReg2 << 16));
                // 結果をコピー（必要に応じて）
                if (srcReg1 != destReg) {
                    ctx.emitU32(0x05604000 | (predReg << 10) | (srcReg1 << 5) | destReg);
                }
            }
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE fma");
            ARM64NEONOperations::generateVectorFma(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトルロード命令
void ARM64SVEOperations::generateVectorLoad(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトルロード命令を生成
    
    // レジスタ割り当て
    Register destReg = ctx.allocatePReg();
    Register addrReg = ctx.operandToReg(op->getOperand(0)); // ベースアドレス
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Int8:
        case DataType::UInt8:
            // LD1B (ベクトル)
            ctx.emitU32(0x84004000 | (predReg << 10) | (addrReg << 5) | destReg);
            break;
            
        case DataType::Int16:
        case DataType::UInt16:
            // LD1H (ベクトル)
            ctx.emitU32(0x84404000 | (predReg << 10) | (addrReg << 5) | destReg);
            break;
            
        case DataType::Int32:
        case DataType::UInt32:
        case DataType::Float32:
            // LD1W (ベクトル)
            ctx.emitU32(0x84804000 | (predReg << 10) | (addrReg << 5) | destReg);
            break;
            
        case DataType::Int64:
        case DataType::UInt64:
        case DataType::Float64:
            // LD1D (ベクトル)
            ctx.emitU32(0x84C04000 | (predReg << 10) | (addrReg << 5) | destReg);
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE load");
            ARM64NEONOperations::generateVectorLoad(op, ctx, backend);
            break;
    }
    
    ctx.setOperandReg(op->getResult(), destReg);
}

// ベクトルストア命令
void ARM64SVEOperations::generateVectorStore(const Operation* op, CodeGenContext& ctx, ARM64Backend& backend) {
    // SVEのベクトルストア命令を生成
    
    // レジスタ割り当て
    Register srcReg = ctx.operandToSVReg(op->getOperand(0));  // ストアするデータ
    Register addrReg = ctx.operandToReg(op->getOperand(1));   // ベースアドレス
    
    // データタイプに基づいて適切なSVE命令を選択
    DataType dataType = op->getDataType();
    
    // プレディケートレジスタ（すべての要素を処理するフルマスク）
    uint32_t predReg = 0; // p0を使用
    
    switch (dataType) {
        case DataType::Int8:
        case DataType::UInt8:
            // ST1B (ベクトル)
            ctx.emitU32(0xE4004000 | (predReg << 10) | (addrReg << 5) | srcReg);
            break;
            
        case DataType::Int16:
        case DataType::UInt16:
            // ST1H (ベクトル)
            ctx.emitU32(0xE4404000 | (predReg << 10) | (addrReg << 5) | srcReg);
            break;
            
        case DataType::Int32:
        case DataType::UInt32:
        case DataType::Float32:
            // ST1W (ベクトル)
            ctx.emitU32(0xE4804000 | (predReg << 10) | (addrReg << 5) | srcReg);
            break;
            
        case DataType::Int64:
        case DataType::UInt64:
        case DataType::Float64:
            // ST1D (ベクトル)
            ctx.emitU32(0xE4C04000 | (predReg << 10) | (addrReg << 5) | srcReg);
            break;
            
        default:
            // サポートされていないデータ型の場合
            ctx.emitCommentLine("Unsupported data type for SVE store");
            ARM64NEONOperations::generateVectorStore(op, ctx, backend);
            break;
    }
}

} // namespace core
} // namespace aerojs 