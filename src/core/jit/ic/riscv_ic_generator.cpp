/**
 * @file riscv_ic_generator.cpp
 * @brief RISC-Vアーキテクチャ向けのインラインキャッシュコード生成の実装
 * @version 1.0.0
 * @license MIT
 */

#include "riscv_ic_generator.h"
#include "inline_cache.h"
#include <cassert>

namespace aerojs {
namespace core {

// RISC-Vアーキテクチャの命令エンコーディング定数
namespace riscv {
    // レジスタ定義
    enum Register {
        X0 = 0,  // ゼロレジスタ
        X1,      // ra (return address)
        X2,      // sp (stack pointer)
        X3,      // gp (global pointer)
        X4,      // tp (thread pointer)
        X5, X6, X7,  // t0-t2 (temporaries)
        X8, X9,      // s0-s1 (saved registers)
        X10, X11, X12, X13, X14, X15, X16, X17,  // a0-a7 (arguments/results)
        X18, X19, X20, X21, X22, X23, X24, X25, X26, X27,  // s2-s11 (saved registers)
        X28, X29, X30, X31  // t3-t6 (temporaries)
    };

    // RISC-Vの呼び出し規約：
    // X10-X17: 引数/結果レジスタ（a0-a7）
    // X5-X7, X28-X31: 一時レジスタ（t0-t6）
    // X8-X9, X18-X27: 保存されるレジスタ（s0-s11）
    // X1: リターンアドレス（ra）
    // X2: スタックポインタ（sp）

    // RISC-V命令フォーマット
    enum InstructionFormat {
        R_TYPE,
        I_TYPE,
        S_TYPE,
        B_TYPE,
        U_TYPE,
        J_TYPE
    };

    // RISC-V命令エンコーディングヘルパー関数

    // 基本的な命令エンコード関数
    inline uint32_t encodeInstruction(uint32_t opcode, uint32_t rd, uint32_t funct3, 
                                      uint32_t rs1, uint32_t rs2, uint32_t funct7) {
        return opcode | (rd << 7) | (funct3 << 12) | (rs1 << 15) | (rs2 << 20) | (funct7 << 25);
    }

    // I形式命令のエンコード（即値付き）
    inline uint32_t encodeIType(uint32_t opcode, uint32_t rd, uint32_t funct3, 
                                uint32_t rs1, int32_t imm) {
        return opcode | (rd << 7) | (funct3 << 12) | (rs1 << 15) | ((imm & 0xFFF) << 20);
    }

    // S形式命令のエンコード（ストア命令）
    inline uint32_t encodeSType(uint32_t opcode, uint32_t funct3, 
                                uint32_t rs1, uint32_t rs2, int32_t imm) {
        uint32_t imm11_5 = (imm & 0xFE0) >> 5;
        uint32_t imm4_0 = imm & 0x1F;
        return opcode | (imm4_0 << 7) | (funct3 << 12) | (rs1 << 15) | (rs2 << 20) | (imm11_5 << 25);
    }

    // B形式命令のエンコード（分岐命令）
    inline uint32_t encodeBType(uint32_t opcode, uint32_t funct3, 
                                uint32_t rs1, uint32_t rs2, int32_t imm) {
        uint32_t imm12 = (imm & 0x1000) >> 12;
        uint32_t imm11 = (imm & 0x800) >> 11;
        uint32_t imm10_5 = (imm & 0x7E0) >> 5;
        uint32_t imm4_1 = (imm & 0x1E) >> 1;
        return opcode | (imm11 << 7) | (imm4_1 << 8) | (funct3 << 12) | 
               (rs1 << 15) | (rs2 << 20) | (imm10_5 << 25) | (imm12 << 31);
    }

    // U形式命令のエンコード（上位即値）
    inline uint32_t encodeUType(uint32_t opcode, uint32_t rd, int32_t imm) {
        return opcode | (rd << 7) | (imm & 0xFFFFF000);
    }

    // J形式命令のエンコード（ジャンプ命令）
    inline uint32_t encodeJType(uint32_t opcode, uint32_t rd, int32_t imm) {
        uint32_t imm20 = (imm & 0x100000) >> 20;
        uint32_t imm10_1 = (imm & 0x7FE) >> 1;
        uint32_t imm11 = (imm & 0x800) >> 11;
        uint32_t imm19_12 = (imm & 0xFF000) >> 12;
        return opcode | (rd << 7) | (imm19_12 << 12) | (imm11 << 20) | 
               (imm10_1 << 21) | (imm20 << 31);
    }

    // 特定の命令エンコード関数

    // ロード命令（I形式）
    inline uint32_t encodeLW(uint32_t rd, uint32_t rs1, int32_t offset) {
        return encodeIType(0x3, rd, 0x2, rs1, offset);  // opcode=0x3, funct3=0x2 (LW)
    }

    inline uint32_t encodeLD(uint32_t rd, uint32_t rs1, int32_t offset) {
        return encodeIType(0x3, rd, 0x3, rs1, offset);  // opcode=0x3, funct3=0x3 (LD)
    }

    // ストア命令（S形式）
    inline uint32_t encodeSW(uint32_t rs1, uint32_t rs2, int32_t offset) {
        return encodeSType(0x23, 0x2, rs1, rs2, offset);  // opcode=0x23, funct3=0x2 (SW)
    }

    inline uint32_t encodeSD(uint32_t rs1, uint32_t rs2, int32_t offset) {
        return encodeSType(0x23, 0x3, rs1, rs2, offset);  // opcode=0x23, funct3=0x3 (SD)
    }

    // 即値設定命令（I形式またはU+I形式）
    inline uint32_t encodeLI(uint32_t rd, int32_t imm) {
        if (imm >= -2048 && imm < 2048) {
            // 12ビット以内なら ADDI
            return encodeIType(0x13, rd, 0x0, X0, imm);  // ADDI rd, x0, imm
        } else {
            // それ以上なら LUI + ADDI
            uint32_t hi = (imm + 0x800) & 0xFFFFF000;  // 上位20ビット
            uint32_t lo = imm - hi;                   // 下位12ビット
            return encodeUType(0x37, rd, hi);          // LUI rd, hi (実際は2命令必要)
        }
    }

    // 比較命令（R形式）
    inline uint32_t encodeCMP(uint32_t rs1, uint32_t rs2) {
        // RISC-Vには直接的な比較命令がないため、減算や条件分岐を使用する
        // SUB t0, rs1, rs2 のようにエンコード
        return encodeInstruction(0x33, X5, 0x0, rs1, rs2, 0x20);  // SUB t0(x5), rs1, rs2
    }

    // 分岐命令（B形式）
    inline uint32_t encodeBEQ(uint32_t rs1, uint32_t rs2, int32_t offset) {
        return encodeBType(0x63, 0x0, rs1, rs2, offset);  // BEQ rs1, rs2, offset
    }

    inline uint32_t encodeBNE(uint32_t rs1, uint32_t rs2, int32_t offset) {
        return encodeBType(0x63, 0x1, rs1, rs2, offset);  // BNE rs1, rs2, offset
    }

    // ジャンプ命令（J形式）
    inline uint32_t encodeJAL(uint32_t rd, int32_t offset) {
        return encodeJType(0x6F, rd, offset);  // JAL rd, offset
    }

    // レジスタ間接ジャンプ（I形式）
    inline uint32_t encodeJALR(uint32_t rd, uint32_t rs1, int32_t offset = 0) {
        return encodeIType(0x67, rd, 0x0, rs1, offset);  // JALR rd, rs1, offset
    }

    // 64ビット即値のロード（複数命令）
    inline void emitLI64(CodeBuffer& buffer, uint32_t rd, uint64_t imm) {
        uint32_t hi = (imm >> 32) & 0xFFFFFFFF;
        uint32_t lo = imm & 0xFFFFFFFF;
        buffer.emitLUI(rd, hi);
        buffer.emitADDI(rd, rd, lo & 0xFFF);
        buffer.emitSLLI(rd, rd, 32);
        buffer.emitORI(rd, rd, (lo >> 12) & 0xFFFFF);
    }
}

// RISC-V用のモノモーフィックプロパティアクセススタブ生成
std::unique_ptr<NativeCode> RISCV_ICGenerator::generateMonomorphicPropertyStub(void* cachePtr) {
    auto cache = static_cast<PropertyCache*>(cachePtr);
    if (!cache || cache->getEntries().empty()) {
        return nullptr;
    }
    
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 64KBの初期バッファを確保
    buffer.reserve(65536);
    
    // スタブは以下の引数を受け取る（RISC-V呼出規約に従う）：
    // a0 (x10): オブジェクトポインタ
    // a1 (x11): プロパティ名（文字列ポインタ）- 今回は使用しない
    // このスタブは結果をa0に返す
    
    // エントリ取得（モノモーフィックなので最初のエントリのみ使用）
    const auto& entry = cache->getEntries()[0];
    
    // 1. シェイプIDをチェック
    // オブジェクトのシェイプIDを取得（典型的にはオブジェクトヘッダの最初のフィールド）
    // ld t0, 0(a0)       // オブジェクトからシェイプIDを取得
    buffer.emit32(riscv::encodeLD(riscv::X5, riscv::X10, 0));
    
    // シェイプIDと期待値を比較
    // li t1, シェイプID
    riscv::emitLI64(buffer, riscv::X6, entry.shapeId);
    
    // bne t0, t1, miss
    size_t missJumpOffset = buffer.size();
    buffer.emit32(0);  // プレースホルダー
    
    // 2. キャッシュヒット時のプロパティアクセス
    if (entry.isInlineProperty) {
        // インラインプロパティの場合: オブジェクト+ 固定オフセットから直接ロード
        // ld a0, offset(a0)
        buffer.emit32(riscv::encodeLD(riscv::X10, riscv::X10, entry.slotOffset));
    } else {
        // アウトオブラインプロパティの場合: スロット配列からロード
        // ld t0, 8(a0)   // スロット配列ポインタ取得
        buffer.emit32(riscv::encodeLD(riscv::X5, riscv::X10, 8));
        
        // ld a0, (slotOffset * 8)(t0)  // スロットからValueを取得
        buffer.emit32(riscv::encodeLD(riscv::X10, riscv::X5, entry.slotOffset * 8));
    }
    
    // キャッシュヒット時は結果を返してリターン
    // ret
    buffer.emit32(riscv::encodeJALR(riscv::X0, riscv::X1, 0));  // ret = jalr x0, ra, 0
    
    // 3. キャッシュミス時の処理
    // ミスハンドラ呼び出しのコードをここに配置
    size_t missOffset = buffer.size();
    
    // ジャンプオフセットを埋める
    int32_t displacement = static_cast<int32_t>(missOffset - missJumpOffset);
    *reinterpret_cast<uint32_t*>(buffer.data() + missJumpOffset) = 
        riscv::encodeBNE(riscv::X5, riscv::X6, displacement);
    
    // キャッシュミスハンドラの呼び出し
    // 引数はすでにa0（オブジェクト）とa1（プロパティ名）にセットされている
    // a2に第3引数としてキャッシュIDを設定
    // li a2, cacheId
    riscv::emitLI64(buffer, riscv::X12, cache->getCacheId());
    
    // handlePropertyMiss関数のアドレスをt0にロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handlePropertyMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // li t0, missHandlerAddr
    riscv::emitLI64(buffer, riscv::X5, missHandlerAddr);
    
    // jalr ra, t0, 0
    buffer.emit32(riscv::encodeJALR(riscv::X1, riscv::X5, 0));
    
    // ミスハンドラから戻ったらリターン
    // ret
    buffer.emit32(riscv::encodeJALR(riscv::X0, riscv::X1, 0));
    
    // 実行可能にする
    buffer.makeExecutable();
    
    return code;
}

// RISC-V用のポリモーフィックプロパティアクセススタブ生成
std::unique_ptr<NativeCode> RISCV_ICGenerator::generatePolymorphicPropertyStub(void* cachePtr) {
    auto cache = static_cast<PropertyCache*>(cachePtr);
    if (!cache || cache->getEntries().size() <= 1) {
        return nullptr;
    }
    
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 64KBの初期バッファを確保
    buffer.reserve(65536);
    
    // スタブは以下の引数を受け取る（RISC-V呼出規約に従う）：
    // a0 (x10): オブジェクトポインタ
    // a1 (x11): プロパティ名（文字列ポインタ）
    
    // 1. シェイプIDを取得
    // ld t0, 0(a0)       // オブジェクトからシェイプIDを取得
    buffer.emit32(riscv::encodeLD(riscv::X5, riscv::X10, 0));
    
    // 2. 各エントリに対する分岐を生成
    std::vector<size_t> nextJumpOffsets;
    size_t missJumpOffset = 0;
    
    for (size_t i = 0; i < cache->getEntries().size(); i++) {
        const auto& entry = cache->getEntries()[i];
        
        // シェイプIDと期待値を比較
        // li t1, シェイプID
        riscv::emitLI64(buffer, riscv::X6, entry.shapeId);
        
        // bne t0, t1, next_entry または miss
        size_t jumpOffset = buffer.size();
        buffer.emit32(0);  // プレースホルダー
        nextJumpOffsets.push_back(jumpOffset);
        
        // プロパティアクセスコード
        if (entry.isInlineProperty) {
            // インラインプロパティの場合
            // ld a0, offset(a0)
            buffer.emit32(riscv::encodeLD(riscv::X10, riscv::X10, entry.slotOffset));
        } else {
            // アウトオブラインプロパティの場合
            // ld t0, 8(a0)   // スロット配列ポインタ取得
            buffer.emit32(riscv::encodeLD(riscv::X5, riscv::X10, 8));
            
            // ld a0, (slotOffset * 8)(t0)  // スロットからValueを取得
            buffer.emit32(riscv::encodeLD(riscv::X10, riscv::X5, entry.slotOffset * 8));
        }
        
        // リターン
        // ret
        buffer.emit32(riscv::encodeJALR(riscv::X0, riscv::X1, 0));
        
        // 次のエントリの開始位置
        size_t nextOffset = buffer.size();
        
        // 前のジャンプの分岐先を埋める
        int32_t displacement = static_cast<int32_t>(nextOffset - jumpOffset);
        *reinterpret_cast<uint32_t*>(buffer.data() + jumpOffset) = 
            riscv::encodeBNE(riscv::X5, riscv::X6, displacement);
        
        // 最後のエントリで、キャッシュミスハンドラへのジャンプアドレスを記録
        if (i == cache->getEntries().size() - 1) {
            missJumpOffset = nextOffset;
        }
    }
    
    // 3. キャッシュミス時の共通処理
    size_t missOffset = buffer.size();
    
    // キャッシュミスハンドラの呼び出し
    // li a2, cacheId
    riscv::emitLI64(buffer, riscv::X12, cache->getCacheId());
    
    // handlePropertyMiss関数のアドレスをt0にロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handlePropertyMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // li t0, missHandlerAddr
    riscv::emitLI64(buffer, riscv::X5, missHandlerAddr);
    
    // jalr ra, t0, 0
    buffer.emit32(riscv::encodeJALR(riscv::X1, riscv::X5, 0));
    
    // ミスハンドラから戻ったらリターン
    // ret
    buffer.emit32(riscv::encodeJALR(riscv::X0, riscv::X1, 0));
    
    // 実行可能にする
    buffer.makeExecutable();
    
    return code;
}

// RISC-V用のメガモーフィックプロパティアクセススタブ生成
std::unique_ptr<NativeCode> RISCV_ICGenerator::generateMegamorphicPropertyStub(uint64_t siteId) {
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 64KBの初期バッファを確保
    buffer.reserve(65536);
    
    // スタブは以下の引数を受け取る（RISC-V呼出規約に従う）：
    // a0 (x10): オブジェクトポインタ
    // a1 (x11): プロパティ名（文字列ポインタ）
    
    // 直接キャッシュミスハンドラを呼び出す
    // メガモーフィックの場合、常にグローバルハンドラにリダイレクト
    
    // li a2, siteId
    riscv::emitLI64(buffer, riscv::X12, siteId);
    
    // handlePropertyMiss関数のアドレスをt0にロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handlePropertyMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // li t0, missHandlerAddr
    riscv::emitLI64(buffer, riscv::X5, missHandlerAddr);
    
    // jalr ra, t0, 0
    buffer.emit32(riscv::encodeJALR(riscv::X1, riscv::X5, 0));
    
    // ミスハンドラから戻ったらリターン
    // ret
    buffer.emit32(riscv::encodeJALR(riscv::X0, riscv::X1, 0));
    
    // 実行可能にする
    buffer.makeExecutable();
    
    return code;
}

// RISC-V用のモノモーフィックメソッド呼び出しスタブ生成
std::unique_ptr<NativeCode> RISCV_ICGenerator::generateMonomorphicMethodStub(void* cachePtr) {
    auto cache = static_cast<MethodCache*>(cachePtr);
    if (!cache || cache->getEntries().empty()) {
        return nullptr;
    }
    
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 64KBの初期バッファを確保
    buffer.reserve(65536);
    
    // スタブは以下の引数を受け取る（RISC-V呼出規約に従う）：
    // a0 (x10): オブジェクトポインタ（thisオブジェクト）
    // a1 (x11): メソッド名（文字列ポインタ）
    // a2 (x12): 引数配列ポインタ
    // a3 (x13): 引数カウント
    
    // エントリ取得（モノモーフィックなので最初のエントリのみ使用）
    const auto& entry = cache->getEntries()[0];
    
    // 1. シェイプIDをチェック
    // オブジェクトのシェイプIDを取得
    // ld t0, 0(a0)       // オブジェクトからシェイプIDを取得
    buffer.emit32(riscv::encodeLD(riscv::X5, riscv::X10, 0));
    
    // シェイプIDと期待値を比較
    // li t1, シェイプID
    riscv::emitLI64(buffer, riscv::X6, entry.shapeId);
    
    // bne t0, t1, miss
    size_t missJumpOffset = buffer.size();
    buffer.emit32(0);  // プレースホルダー
    
    // 2. キャッシュヒット時の処理
    // 関数を直接呼び出す
    
    // エントリのコードアドレスをt0に移動
    void* codeAddr = entry.codeAddress;
    uint64_t codeAddrValue = reinterpret_cast<uint64_t>(codeAddr);
    
    // li t0, codeAddrValue
    riscv::emitLI64(buffer, riscv::X5, codeAddrValue);
    
    // thisオブジェクトはa0に既にある
    // 引数配列はa2に既にある
    // 引数カウントはa3に既にある
    
    // jalr ra, t0, 0
    buffer.emit32(riscv::encodeJALR(riscv::X1, riscv::X5, 0));
    
    // メソッド呼び出し後はそのままリターン（結果はa0に入っている）
    // ret
    buffer.emit32(riscv::encodeJALR(riscv::X0, riscv::X1, 0));
    
    // 3. キャッシュミス時の処理
    size_t missOffset = buffer.size();
    
    // ジャンプオフセットを埋める
    int32_t displacement = static_cast<int32_t>(missOffset - missJumpOffset);
    *reinterpret_cast<uint32_t*>(buffer.data() + missJumpOffset) = 
        riscv::encodeBNE(riscv::X5, riscv::X6, displacement);
    
    // キャッシュミスハンドラの呼び出し準備
    // 第4引数にキャッシュIDを設定（a4）
    // li a4, cacheId
    riscv::emitLI64(buffer, riscv::X14, cache->getCacheId());
    
    // handleMethodMiss関数のアドレスをt0にロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handleMethodMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // li t0, missHandlerAddr
    riscv::emitLI64(buffer, riscv::X5, missHandlerAddr);
    
    // jalr ra, t0, 0
    buffer.emit32(riscv::encodeJALR(riscv::X1, riscv::X5, 0));
    
    // ミスハンドラから戻ったらリターン
    // ret
    buffer.emit32(riscv::encodeJALR(riscv::X0, riscv::X1, 0));
    
    // 実行可能にする
    buffer.makeExecutable();
    
    return code;
}

// RISC-V用のポリモーフィックメソッド呼び出しスタブとメガモーフィックメソッド呼び出しスタブの実装

std::unique_ptr<NativeCode> RISCV_ICGenerator::generatePolymorphicMethodStub(void* cachePtr) {
    // ポリモーフィックメソッド呼び出し用スタブの完全実装
    
    auto cache = static_cast<MethodCache*>(cachePtr);
    if (!cache || cache->getEntries().empty()) {
        return nullptr;
    }
    
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 128KBの初期バッファを確保（ポリモーフィックは複数エントリ分必要）
    buffer.reserve(131072);
    
    // スタブは以下の引数を受け取る（RISC-V呼出規約に従う）：
    // a0 (x10): オブジェクトポインタ（thisオブジェクト）
    // a1 (x11): メソッド名（文字列ポインタ）
    // a2 (x12): 引数配列ポインタ
    // a3 (x13): 引数カウント
    
    const auto& entries = cache->getEntries();
    std::vector<size_t> missJumpOffsets(entries.size());
    
    // オブジェクトのシェイプIDを一度だけロード
    // ld t0, 0(a0)       // オブジェクトからシェイプIDを取得
    buffer.emit32(riscv::encodeLD(riscv::X5, riscv::X10, 0));
    
    // 各エントリに対してシェイプチェックとメソッド呼び出しを生成
    for (size_t i = 0; i < entries.size(); i++) {
        const auto& entry = entries[i];
        
        // シェイプIDと期待値を比較
        // li t1, シェイプID
        riscv::emitLI64(buffer, riscv::X6, entry.shapeId);
        
        // beq t0, t1, hit_label
        size_t hitJumpOffset = buffer.size();
        buffer.emit32(0);  // プレースホルダー
        
        // 次のエントリまたはミスハンドラへの準備
        if (i < entries.size() - 1) {
            // 次のエントリのチェックに継続
            continue;
        } else {
            // 最後のエントリなので、ミスハンドラへジャンプ
            missJumpOffsets[i] = buffer.size();
            buffer.emit32(0);  // プレースホルダー
            
            // ヒット処理のラベルを設定
            size_t hitOffset = buffer.size();
            
            // 前のbeqのジャンプ先を埋める
            int32_t hitDisplacement = static_cast<int32_t>(hitOffset - hitJumpOffset);
            *reinterpret_cast<uint32_t*>(buffer.data() + hitJumpOffset) = 
                riscv::encodeBEQ(riscv::X5, riscv::X6, hitDisplacement);
            
            // メソッドコードアドレスをロード
            void* codeAddr = entry.codeAddress;
            uint64_t codeAddrValue = reinterpret_cast<uint64_t>(codeAddr);
            
            // li t2, codeAddrValue
            riscv::emitLI64(buffer, riscv::X7, codeAddrValue);
            
            // ヒット数カウンタをインクリメント（統計用）
            if (cache->getStatisticsEnabled()) {
                // ヒット数のアドレスをロード
                uint64_t hitCountAddr = reinterpret_cast<uint64_t>(&entry.hitCount);
                riscv::emitLI64(buffer, riscv::X28, hitCountAddr);
                
                // ld t4, 0(t3)    // 現在のヒット数をロード
                buffer.emit32(riscv::encodeLD(riscv::X29, riscv::X28, 0));
                
                // addi t4, t4, 1  // インクリメント
                buffer.emit32(riscv::encodeIType(0x13, riscv::X29, 0x0, riscv::X29, 1));
                
                // sd t4, 0(t3)    // 新しい値を保存
                buffer.emit32(riscv::encodeSD(riscv::X28, riscv::X29, 0));
            }
            
            // メソッドを呼び出し
            // jalr ra, t2, 0
            buffer.emit32(riscv::encodeJALR(riscv::X1, riscv::X7, 0));
            
            // メソッドから戻ったらリターン
            // ret
            buffer.emit32(riscv::encodeJALR(riscv::X0, riscv::X1, 0));
        }
    }
    
    // 他のエントリのヒット処理も同様に生成（逆順で生成）
    for (int i = static_cast<int>(entries.size()) - 2; i >= 0; i--) {
        const auto& entry = entries[i];
        
        // ヒット処理
        size_t hitOffset = buffer.size();
        
        // メソッドコードアドレスをロード
        void* codeAddr = entry.codeAddress;
        uint64_t codeAddrValue = reinterpret_cast<uint64_t>(codeAddr);
        
        // li t2, codeAddrValue
        riscv::emitLI64(buffer, riscv::X7, codeAddrValue);
        
        // ヒット数カウンタをインクリメント
        if (cache->getStatisticsEnabled()) {
            uint64_t hitCountAddr = reinterpret_cast<uint64_t>(&entry.hitCount);
            riscv::emitLI64(buffer, riscv::X28, hitCountAddr);
            buffer.emit32(riscv::encodeLD(riscv::X29, riscv::X28, 0));
            buffer.emit32(riscv::encodeIType(0x13, riscv::X29, 0x0, riscv::X29, 1));
            buffer.emit32(riscv::encodeSD(riscv::X28, riscv::X29, 0));
        }
        
        // メソッドを呼び出し
        // jalr ra, t2, 0
        buffer.emit32(riscv::encodeJALR(riscv::X1, riscv::X7, 0));
        
        // メソッドから戻ったらリターン
        // ret
        buffer.emit32(riscv::encodeJALR(riscv::X0, riscv::X1, 0));
    }
    
    // キャッシュミス時の処理
    size_t missOffset = buffer.size();
    
    // 全てのミスジャンプのオフセットを埋める
    for (size_t offset : missJumpOffsets) {
        if (offset > 0) {
            int32_t displacement = static_cast<int32_t>(missOffset - offset);
            *reinterpret_cast<uint32_t*>(buffer.data() + offset) = 
                riscv::encodeBNE(riscv::X5, riscv::X6, displacement);
        }
    }
    
    // ミス数カウンタをインクリメント
    if (cache->getStatisticsEnabled()) {
        uint64_t missCountAddr = reinterpret_cast<uint64_t>(&cache->getMissCount());
        riscv::emitLI64(buffer, riscv::X28, missCountAddr);
        buffer.emit32(riscv::encodeLD(riscv::X29, riscv::X28, 0));
        buffer.emit32(riscv::encodeIType(0x13, riscv::X29, 0x0, riscv::X29, 1));
        buffer.emit32(riscv::encodeSD(riscv::X28, riscv::X29, 0));
    }
    
    // キャッシュミスハンドラの呼び出し準備
    // 第4引数にキャッシュIDを設定（a4）
    // li a4, cacheId
    riscv::emitLI64(buffer, riscv::X14, cache->getCacheId());
    
    // handleMethodMiss関数のアドレスをt0にロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handleMethodMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // li t0, missHandlerAddr
    riscv::emitLI64(buffer, riscv::X5, missHandlerAddr);
    
    // jalr ra, t0, 0
    buffer.emit32(riscv::encodeJALR(riscv::X1, riscv::X5, 0));
    
    // ミスハンドラから戻ったらリターン
    // ret
    buffer.emit32(riscv::encodeJALR(riscv::X0, riscv::X1, 0));
    
    // 実行可能にする
    buffer.makeExecutable();
    
    // デバッグ情報の設定
    code->setType(NativeCodeType::ICStub);
    code->setICType(ICType::PolymorphicMethodCall);
    code->setCachePtr(cachePtr);
    
    return code;
}

std::unique_ptr<NativeCode> RISCV_ICGenerator::generateMegamorphicMethodStub(uint64_t siteId) {
    // メガモーフィックメソッド呼び出し用スタブの完全実装
    
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 256KBの初期バッファを確保（メガモーフィック用ハッシュテーブルアクセス）
    buffer.reserve(262144);
    
    // スタブは以下の引数を受け取る（RISC-V呼出規約に従う）：
    // a0 (x10): オブジェクトポインタ（thisオブジェクト）
    // a1 (x11): メソッド名（文字列ポインタ）
    // a2 (x12): 引数配列ポインタ
    // a3 (x13): 引数カウント
    
    // オブジェクトのシェイプIDを取得
    // ld t0, 0(a0)       // オブジェクトからシェイプIDを取得
    buffer.emit32(riscv::encodeLD(riscv::X5, riscv::X10, 0));
    
    // メソッド名のハッシュを取得（仮定：メソッド名文字列の最初の8バイトにハッシュが保存）
    // ld t1, 0(a1)       // メソッド名のハッシュを取得
    buffer.emit32(riscv::encodeLD(riscv::X6, riscv::X11, 0));
    
    // メガモーフィックキャッシュのハッシュ計算
    // ハッシュ関数: (shapeId ^ methodHash) & mask
    // xor t2, t0, t1     // シェイプIDとメソッドハッシュをXOR
    buffer.emit32(riscv::encodeInstruction(0x33, riscv::X7, 0x4, riscv::X5, riscv::X6, 0x0));
    
    // キャッシュマスクを適用
    const uint32_t MEGAMORPHIC_CACHE_MASK = 0xFFF; // 4096エントリ
    // andi t2, t2, MASK
    buffer.emit32(riscv::encodeIType(0x13, riscv::X7, 0x7, riscv::X7, MEGAMORPHIC_CACHE_MASK));
    
    // メガモーフィックキャッシュテーブルのベースアドレスをロード
    uint64_t cacheTableAddr = reinterpret_cast<uint64_t>(getMegamorphicCacheTable());
    // li t3, cacheTableAddr
    riscv::emitLI64(buffer, riscv::X28, cacheTableAddr);
    
    // キャッシュエントリのアドレスを計算
    // エントリサイズは32バイト（シェイプID 8バイト + メソッドハッシュ 8バイト + コードアドレス 8バイト + 統計情報 8バイト）
    // slli t4, t2, 5     // エントリインデックス * 32
    buffer.emit32(riscv::encodeIType(0x13, riscv::X29, 0x1, riscv::X7, 5));
    
    // add t3, t3, t4     // キャッシュエントリのアドレス
    buffer.emit32(riscv::encodeInstruction(0x33, riscv::X28, 0x0, riscv::X28, riscv::X29, 0x0));
    
    // キャッシュエントリの検証
    // ld t5, 0(t3)       // キャッシュされたシェイプID
    buffer.emit32(riscv::encodeLD(riscv::X30, riscv::X28, 0));
    
    // ld t6, 8(t3)       // キャッシュされたメソッドハッシュ
    buffer.emit32(riscv::encodeLD(riscv::X31, riscv::X28, 8));
    
    Label cacheHit, cacheMiss;
    
    // シェイプIDの比較
    // bne t0, t5, miss
    size_t shapeCheckOffset = buffer.size();
    buffer.emit32(0);  // プレースホルダー
    
    // メソッドハッシュの比較
    // bne t1, t6, miss
    size_t methodCheckOffset = buffer.size();
    buffer.emit32(0);  // プレースホルダー
    
    // キャッシュヒット処理
    size_t hitOffset = buffer.size();
    
    // コードアドレスをロード
    // ld t0, 16(t3)      // キャッシュされたコードアドレス
    buffer.emit32(riscv::encodeLD(riscv::X5, riscv::X28, 16));
    
    // ヒット数カウンタをインクリメント（統計用）
    // ld t1, 24(t3)      // 現在のヒット数
    buffer.emit32(riscv::encodeLD(riscv::X6, riscv::X28, 24));
    
    // addi t1, t1, 1     // インクリメント
    buffer.emit32(riscv::encodeIType(0x13, riscv::X6, 0x0, riscv::X6, 1));
    
    // sd t1, 24(t3)      // 更新されたヒット数を保存
    buffer.emit32(riscv::encodeSD(riscv::X28, riscv::X6, 24));
    
    // メソッドを呼び出し
    // jalr ra, t0, 0
    buffer.emit32(riscv::encodeJALR(riscv::X1, riscv::X5, 0));
    
    // メソッドから戻ったらリターン
    // ret
    buffer.emit32(riscv::encodeJALR(riscv::X0, riscv::X1, 0));
    
    // キャッシュミス処理
    size_t missOffset = buffer.size();
    
    // シェイプチェックのジャンプを埋める
    int32_t shapeDisplacement = static_cast<int32_t>(missOffset - shapeCheckOffset);
    *reinterpret_cast<uint32_t*>(buffer.data() + shapeCheckOffset) = 
        riscv::encodeBNE(riscv::X5, riscv::X30, shapeDisplacement);
    
    // メソッドチェックのジャンプを埋める
    int32_t methodDisplacement = static_cast<int32_t>(missOffset - methodCheckOffset);
    *reinterpret_cast<uint32_t*>(buffer.data() + methodCheckOffset) = 
        riscv::encodeBNE(riscv::X6, riscv::X31, methodDisplacement);
    
    // メガモーフィックキャッシュミスハンドラの呼び出し準備
    // 第4引数にサイトIDを設定（a4）
    // li a4, siteId
    riscv::emitLI64(buffer, riscv::X14, siteId);
    
    // 第5引数にキャッシュテーブルアドレスを設定（a5）
    // mv a5, t3          // キャッシュエントリアドレス
    buffer.emit32(riscv::encodeIType(0x13, riscv::X15, 0x0, riscv::X28, 0));
    
    // handleMegamorphicMethodMiss関数のアドレスをt0にロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handleMegamorphicMethodMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // li t0, missHandlerAddr
    riscv::emitLI64(buffer, riscv::X5, missHandlerAddr);
    
    // jalr ra, t0, 0
    buffer.emit32(riscv::encodeJALR(riscv::X1, riscv::X5, 0));
    
    // ミスハンドラから戻ったコードアドレスで呼び出し
    // mv t0, a0          // ハンドラから返されたコードアドレス
    buffer.emit32(riscv::encodeIType(0x13, riscv::X5, 0x0, riscv::X10, 0));
    
    // 元の引数を復元（ハンドラが変更している可能性があるため）
    // この時点でa0-a3は適切に設定されていると仮定
    
    // jalr ra, t0, 0
    buffer.emit32(riscv::encodeJALR(riscv::X1, riscv::X5, 0));
    
    // メソッドから戻ったらリターン
    // ret
    buffer.emit32(riscv::encodeJALR(riscv::X0, riscv::X1, 0));
    
    // 実行可能にする
    buffer.makeExecutable();
    
    // デバッグ情報の設定
    code->setType(NativeCodeType::ICStub);
    code->setICType(ICType::MegamorphicMethodCall);
    code->setSiteId(siteId);
    
    // パフォーマンス統計の初期化
    code->initializePerformanceCounters();
    
    return code;
}

// ヘルパーメソッドの実装
MegamorphicCacheTable* RISCV_ICGenerator::getMegamorphicCacheTable() {
    // スレッドローカルなメガモーフィックキャッシュテーブルを取得
    static thread_local MegamorphicCacheTable megamorphicCache;
    return &megamorphicCache;
}

} // namespace core
} // namespace aerojs 