#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include "../../src/core/jit/backend/riscv/riscv_vector.h"

using namespace aerojs::core;

// RISCVVector実装のテスト
class RISCVVectorTest : public ::testing::Test {
protected:
    // テスト用バッファ
    std::vector<uint8_t> output;
    
    void SetUp() override {
        // 各テスト前にバッファをクリア
        output.clear();
    }
    
    // バイナリ命令の検証ヘルパー
    void verifyInstruction(uint32_t expectedInstruction, size_t index = 0) {
        ASSERT_GE(output.size(), index * 4 + 4) << "命令バッファのサイズが足りません";
        
        uint32_t actualInstruction = 
            static_cast<uint32_t>(output[index * 4]) |
            (static_cast<uint32_t>(output[index * 4 + 1]) << 8) |
            (static_cast<uint32_t>(output[index * 4 + 2]) << 16) |
            (static_cast<uint32_t>(output[index * 4 + 3]) << 24);
            
        EXPECT_EQ(expectedInstruction, actualInstruction) 
            << "命令が一致しません。インデックス: " << index;
    }
};

// VSET命令のテスト
TEST_F(RISCVVectorTest, EmitSetVL) {
    // vsetvli t0, zero, e32, m8
    RISCVVector::emitSetVL(output, 5, 0, RVVectorSEW::SEW_32, RVVectorLMUL::LMUL_8);
    
    // 命令フォーマット: vsetvli rd, rs1, vtypei
    // vtypei = sew[2:0] | lmul[2:0] | reserved | vma | vta
    // e32 (SEW=32) = 010, m8 (LMUL=8) = 011
    uint32_t expected = 0x57 | (5 << 7) | (0 << 15) | (7 << 12) | ((0b010 | (0b011 << 3)) << 20);
    verifyInstruction(expected);
}

// ベクトルロード命令のテスト
TEST_F(RISCVVectorTest, EmitVectorLoad) {
    // vle32.v v1, (a0)
    RISCVVector::emitVectorLoad(output, 1, 10, RVVectorMask::MASK_NONE, 4);
    
    // 命令フォーマット: vle32.v vd, (rs1)
    // opcode=0x07, vd=1, rs1=10, funct3=6 (32ビット), vm=0
    uint32_t expected = 0x07 | (1 << 7) | (10 << 15) | (6 << 12);
    verifyInstruction(expected);
}

// ベクトルストア命令のテスト
TEST_F(RISCVVectorTest, EmitVectorStore) {
    // vse64.v v2, (a1)
    RISCVVector::emitVectorStore(output, 2, 11, RVVectorMask::MASK_NONE, 8);
    
    // 命令フォーマット: vse64.v vs3, (rs1)
    // opcode=0x27, vs3=2, rs1=11, funct3=7 (64ビット), vm=0
    uint32_t expected = 0x27 | (2 << 7) | (11 << 15) | (7 << 12);
    verifyInstruction(expected);
}

// ベクトル算術演算のテスト
TEST_F(RISCVVectorTest, EmitVectorArithmetic) {
    // vadd.vv v3, v1, v2
    RISCVVector::emitVectorAdd(output, 3, 1, 2, RVVectorMask::MASK_NONE);
    
    // vsub.vv v4, v1, v2
    RISCVVector::emitVectorSub(output, 4, 1, 2, RVVectorMask::MASK_NONE);
    
    // vmul.vv v5, v1, v2
    RISCVVector::emitVectorMul(output, 5, 1, 2, RVVectorMask::MASK_NONE);
    
    // vdiv.vv v6, v1, v2
    RISCVVector::emitVectorDiv(output, 6, 1, 2, RVVectorMask::MASK_NONE);
    
    // 命令検証
    // vadd.vv v3, v1, v2
    uint32_t expected_add = 0x57 | (3 << 7) | (1 << 15) | (2 << 20);
    verifyInstruction(expected_add, 0);
    
    // vsub.vv v4, v1, v2
    uint32_t expected_sub = 0x57 | (4 << 7) | (1 << 15) | (2 << 20) | (0x08 << 26);
    verifyInstruction(expected_sub, 1);
    
    // vmul.vv v5, v1, v2
    uint32_t expected_mul = 0x57 | (5 << 7) | (1 << 15) | (2 << 20) | (0x24 << 26);
    verifyInstruction(expected_mul, 2);
    
    // vdiv.vv v6, v1, v2
    uint32_t expected_div = 0x57 | (6 << 7) | (1 << 15) | (2 << 20) | (0x28 << 26);
    verifyInstruction(expected_div, 3);
}

// 新しく実装したビット操作メソッドのテスト
TEST_F(RISCVVectorTest, EmitVectorBitOps) {
    // vand.vv v3, v1, v2
    RISCVVector::emitVectorAnd(output, 3, 1, 2, RVVectorMask::MASK_NONE);
    
    // vor.vv v4, v1, v2
    RISCVVector::emitVectorOr(output, 4, 1, 2, RVVectorMask::MASK_NONE);
    
    // vxor.vv v5, v1, v2
    RISCVVector::emitVectorXor(output, 5, 1, 2, RVVectorMask::MASK_NONE);
    
    // vnot.v v6, v2
    RISCVVector::emitVectorNot(output, 6, 2, RVVectorMask::MASK_NONE);
    
    // 命令検証
    // vand.vv
    uint32_t expected_and = 0x57 | (3 << 7) | (1 << 15) | (2 << 20) | (0x27 << 26);
    verifyInstruction(expected_and, 0);
    
    // vor.vv
    uint32_t expected_or = 0x57 | (4 << 7) | (1 << 15) | (2 << 20) | (0x25 << 26);
    verifyInstruction(expected_or, 1);
    
    // vxor.vv
    uint32_t expected_xor = 0x57 | (5 << 7) | (1 << 15) | (2 << 20) | (0x23 << 26);
    verifyInstruction(expected_xor, 2);
    
    // vnot.v
    uint32_t expected_not = 0x57 | (6 << 7) | (0 << 15) | (2 << 20) | (0x2F << 26);
    verifyInstruction(expected_not, 3);
}

// 新しく実装した数学関数のテスト
TEST_F(RISCVVectorTest, EmitVectorMathFuncs) {
    // vsqrt.v v3, v1
    RISCVVector::emitVectorSqrt(output, 3, 1, RVVectorMask::MASK_NONE);
    
    // vabs.v v4, v2
    RISCVVector::emitVectorAbs(output, 4, 2, RVVectorMask::MASK_NONE);
    
    // 命令検証
    // vsqrt.v
    uint32_t expected_sqrt = 0x57 | (3 << 7) | (0 << 15) | (1 << 20) | (0x4F << 26);
    verifyInstruction(expected_sqrt, 0);
    
    // vabs.v
    uint32_t expected_abs = 0x57 | (4 << 7) | (0 << 15) | (2 << 20) | (0x4B << 26);
    verifyInstruction(expected_abs, 1);
}

// 縮約操作のテスト
TEST_F(RISCVVectorTest, EmitVectorReduction) {
    // vredsum.vs v0, v1, v2
    RISCVVector::emitVectorRedSum(output, 0, 1, 2, RVVectorMask::MASK_NONE);
    
    // vredmax.vs v0, v1, v2
    RISCVVector::emitVectorRedMax(output, 0, 1, 2, RVVectorMask::MASK_NONE);
    
    // vredmin.vs v0, v1, v2
    RISCVVector::emitVectorRedMin(output, 0, 1, 2, RVVectorMask::MASK_NONE);
    
    // 命令検証
    // vredsum.vs
    uint32_t expected_sum = 0x57 | (0 << 7) | (1 << 15) | (2 << 20) | (0x03 << 26);
    verifyInstruction(expected_sum, 0);
    
    // vredmax.vs
    uint32_t expected_max = 0x57 | (0 << 7) | (1 << 15) | (2 << 20) | (0x07 << 26);
    verifyInstruction(expected_max, 1);
    
    // vredmin.vs
    uint32_t expected_min = 0x57 | (0 << 7) | (1 << 15) | (2 << 20) | (0x05 << 26);
    verifyInstruction(expected_min, 2);
}

// 行列乗算操作のテスト
TEST_F(RISCVVectorTest, EmitMatrixMultiply) {
    // 2x2行列の乗算
    RISCVVector::emitMatrixMultiply(output, 2, 2, 2);
    
    // 最低限のテスト：
    // 1. vsetivli命令が最初に出力されていること
    // 2. 行列乗算に十分な数の命令が生成されていること
    
    // 行列乗算は複雑なシーケンスなので、生成された命令数をチェック
    // 少なくとも以下の要素が含まれているはず:
    // - ループカウンタの初期化 (t0, t1, t2)
    // - ベクトル演算 (vxor, vle, vfmacc)
    // - 分岐命令 (blt)
    // - ストア命令 (vse)
    
    // 2x2行列乗算には少なくとも20命令は必要
    ASSERT_GE(output.size(), 20 * 4) << "行列乗算の命令数が不足しています";
    
    // vsetivli命令が最初に出力されていることを確認
    uint32_t first_instr = 
        static_cast<uint32_t>(output[0]) |
        (static_cast<uint32_t>(output[1]) << 8) |
        (static_cast<uint32_t>(output[2]) << 16) |
        (static_cast<uint32_t>(output[3]) << 24);
    
    // vsetivliのopcode (0x57) が含まれていることを確認
    EXPECT_EQ(0x57, first_instr & 0x7F) << "最初の命令がvsetivliではありません";
}

// JavaScript配列操作のテスト
TEST_F(RISCVVectorTest, EmitJSArrayOperation) {
    // 配列のmap操作
    RISCVVector::emitJSArrayOperation(output, 0, 10, 11, 10);
    
    // 最低限のテスト：
    // 1. vsetvli命令が最初に出力されていること
    // 2. 適切な数の命令が生成されていること
    
    // 必要な命令シーケンスが生成されていることを確認
    ASSERT_GE(output.size(), 6 * 4) << "配列操作の命令数が不足しています";
    
    // vsetvli命令が最初に出力されていることを確認
    uint32_t first_instr = 
        static_cast<uint32_t>(output[0]) |
        (static_cast<uint32_t>(output[1]) << 8) |
        (static_cast<uint32_t>(output[2]) << 16) |
        (static_cast<uint32_t>(output[3]) << 24);
    
    // vsetivliのopcode (0x57) が含まれていることを確認
    EXPECT_EQ(0x57, first_instr & 0x7F) << "最初の命令がvsetivliではありません";
    
    // レジスタ移動命令が次に出力されていることを確認
    uint32_t second_instr = 
        static_cast<uint32_t>(output[4]) |
        (static_cast<uint32_t>(output[5]) << 8) |
        (static_cast<uint32_t>(output[6]) << 16) |
        (static_cast<uint32_t>(output[7]) << 24);
    
    // mvのopcodeが含まれていることを確認
    EXPECT_EQ(0x33, second_instr & 0x7F) << "2番目の命令がmvではありません";
}

// エンコードヘルパーメソッドのテスト
TEST_F(RISCVVectorTest, EncodeHelpers) {
    // encodeVsetivli
    uint32_t vsetivli = RISCVVector::encodeVsetivli(5, 10, RVVectorSEW::SEW_32, RVVectorLMUL::LMUL_4);
    
    // vsetivli t0, 10, e32m4
    uint32_t expected_vsetivli = 0x57 | (5 << 7) | (7 << 12) | 
                               ((10 | ((static_cast<uint32_t>(RVVectorSEW::SEW_32) | 
                                     (static_cast<uint32_t>(RVVectorLMUL::LMUL_4) << 3)) << 5)) << 20);
    EXPECT_EQ(expected_vsetivli, vsetivli) << "encodeVsetivli の出力が不正です";
    
    // encodeVectorOp
    uint32_t vadd = RISCVVector::encodeVectorOp(0x57, 3, 1, 2, 0, 0);
    
    // vadd.vv v3, v1, v2
    uint32_t expected_vadd = 0x57 | (3 << 7) | (1 << 15) | (2 << 20);
    EXPECT_EQ(expected_vadd, vadd) << "encodeVectorOp の出力が不正です";
    
    // encodeRType
    uint32_t add = RISCVVector::encodeRType(0, 5, 6, 0, 7, 0x33);
    
    // add t2, t1, t0
    uint32_t expected_add = 0x33 | (7 << 7) | (0 << 12) | (6 << 15) | (5 << 20) | (0 << 25);
    EXPECT_EQ(expected_add, add) << "encodeRType の出力が不正です";
    
    // encodeBType
    uint32_t blt = RISCVVector::encodeBType(5, 6, 4, 0x63, -8);
    
    // blt t0, t1, -8
    uint32_t imm12 = (-8 >> 12) & 0x1;
    uint32_t imm10_5 = (-8 >> 5) & 0x3F;
    uint32_t imm4_1 = (-8 >> 1) & 0xF;
    uint32_t imm11 = (-8 >> 11) & 0x1;
    uint32_t expected_blt = 0x63 | (imm11 << 7) | (imm4_1 << 8) | (4 << 12) | (5 << 15) | (6 << 20) | (imm10_5 << 25) | (imm12 << 31);
    EXPECT_EQ(expected_blt, blt) << "encodeBType の出力が不正です";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 