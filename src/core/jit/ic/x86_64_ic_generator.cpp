/**
 * @file x86_64_ic_generator.cpp
 * @brief X86_64アーキテクチャ向けのインラインキャッシュコード生成の実装
 * @version 1.0.0
 * @license MIT
 */

#include "x86_64_ic_generator.h"
#include "inline_cache.h"
#include <cassert>

namespace aerojs {
namespace core {

// X86_64アーキテクチャの命令エンコーディング定数
namespace x86_64 {
    // レジスタ定義
    enum Register {
        RAX = 0,
        RCX = 1,
        RDX = 2,
        RBX = 3,
        RSP = 4,
        RBP = 5,
        RSI = 6,
        RDI = 7,
        R8 = 8,
        R9 = 9,
        R10 = 10,
        R11 = 11,
        R12 = 12,
        R13 = 13,
        R14 = 14,
        R15 = 15
    };

    // X86_64の呼び出し規約（System V AMD64 ABI）：
    // RDI, RSI, RDX, RCX, R8, R9: 引数レジスタ
    // RAX: 戻り値レジスタ
    // R10, R11: 一時レジスタ
    // RBX, RBP, R12-R15: 保存レジスタ
    // RSP: スタックポインタ

    // MODビットフィールド定義
    enum MOD {
        MOD_INDIRECT = 0,       // [reg]
        MOD_INDIRECT_DISP8 = 1, // [reg+disp8]
        MOD_INDIRECT_DISP32 = 2,// [reg+disp32]
        MOD_DIRECT = 3          // reg
    };

    // REXプレフィックス生成
    inline uint8_t encodeREX(bool w, bool r, bool x, bool b) {
        return 0x40 | (w ? 0x08 : 0) | (r ? 0x04 : 0) | (x ? 0x02 : 0) | (b ? 0x01 : 0);
    }

    // ModR/Mバイト生成
    inline uint8_t encodeModRM(uint8_t mod, uint8_t reg, uint8_t rm) {
        return (mod << 6) | ((reg & 0x7) << 3) | (rm & 0x7);
    }

    // SIBバイト生成
    inline uint8_t encodeSIB(uint8_t scale, uint8_t index, uint8_t base) {
        return (scale << 6) | ((index & 0x7) << 3) | (base & 0x7);
    }

    // レジスタ-レジスタ間のMOV命令エンコード
    inline void encodeMOV_reg_reg(CodeBuffer& buffer, Register dst, Register src) {
        // REXプレフィックス
        bool rex_w = true;  // 64ビット操作
        bool rex_r = (src & 0x8) != 0;  // 拡張先レジスタ
        bool rex_b = (dst & 0x8) != 0;  // 拡張元レジスタ
        buffer.emit8(encodeREX(rex_w, rex_r, false, rex_b));
        
        // MOV r64, r64 (0x89)
        buffer.emit8(0x89);
        
        // ModR/M
        buffer.emit8(encodeModRM(MOD_DIRECT, src & 0x7, dst & 0x7));
    }

    // メモリからレジスタへのロード命令
    inline void encodeMOV_reg_mem(CodeBuffer& buffer, Register dst, Register base, int32_t offset) {
        // REXプレフィックス
        bool rex_w = true;  // 64ビット操作
        bool rex_r = (dst & 0x8) != 0;  // 拡張先レジスタ
        bool rex_b = (base & 0x8) != 0;  // 拡張ベースレジスタ
        buffer.emit8(encodeREX(rex_w, rex_r, false, rex_b));
        
        // MOV r64, [r64+disp] (0x8B)
        buffer.emit8(0x8B);
        
        // ModR/Mとディスプレースメント
        if (offset == 0 && base != RBP && base != RSP) {
            // [reg]
            buffer.emit8(encodeModRM(MOD_INDIRECT, dst & 0x7, base & 0x7));
        } else if (offset >= -128 && offset <= 127) {
            // [reg+disp8]
            buffer.emit8(encodeModRM(MOD_INDIRECT_DISP8, dst & 0x7, base & 0x7));
            buffer.emit8(static_cast<uint8_t>(offset));
        } else {
            // [reg+disp32]
            buffer.emit8(encodeModRM(MOD_INDIRECT_DISP32, dst & 0x7, base & 0x7));
            buffer.emit32(static_cast<uint32_t>(offset));
        }
    }

    // 即値のロード命令
    inline void encodeMOV_reg_imm64(CodeBuffer& buffer, Register dst, uint64_t imm) {
        // REXプレフィックス
        bool rex_w = true;  // 64ビット操作
        bool rex_b = (dst & 0x8) != 0;  // 拡張レジスタ
        buffer.emit8(encodeREX(rex_w, false, false, rex_b));
        
        // MOV r64, imm64 (0xB8 + rd)
        buffer.emit8(0xB8 + (dst & 0x7));
        
        // 64ビット即値
        buffer.emit64(imm);
    }

    // 比較命令（レジスタと即値）
    inline void encodeCMP_reg_imm32(CodeBuffer& buffer, Register reg, uint32_t imm) {
        // REXプレフィックス
        bool rex_w = true;  // 64ビット操作
        bool rex_b = (reg & 0x8) != 0;  // 拡張レジスタ
        buffer.emit8(encodeREX(rex_w, false, false, rex_b));
        
        if (imm <= 127 && imm >= -128) {
            // CMP r64, imm8 (0x83)
            buffer.emit8(0x83);
            buffer.emit8(encodeModRM(MOD_DIRECT, 7, reg & 0x7));  // 7 = CMP opcode extension
            buffer.emit8(static_cast<uint8_t>(imm));
        } else {
            // CMP r64, imm32 (0x81)
            buffer.emit8(0x81);
            buffer.emit8(encodeModRM(MOD_DIRECT, 7, reg & 0x7));  // 7 = CMP opcode extension
            buffer.emit32(imm);
        }
    }

    // 条件分岐命令
    inline void encodeJcc(CodeBuffer& buffer, uint8_t condition, int32_t offset) {
        if (offset >= -128 && offset <= 127) {
            // 短い分岐
            buffer.emit8(0x70 + condition);  // Jcc short
            buffer.emit8(static_cast<uint8_t>(offset));
        } else {
            // 長い分岐
            buffer.emit8(0x0F);
            buffer.emit8(0x80 + condition);  // Jcc near
            buffer.emit32(static_cast<uint32_t>(offset));
        }
    }

    // 無条件ジャンプ命令
    inline void encodeJMP(CodeBuffer& buffer, int32_t offset) {
        if (offset >= -128 && offset <= 127) {
            // 短いジャンプ
            buffer.emit8(0xEB);
            buffer.emit8(static_cast<uint8_t>(offset));
        } else {
            // 長いジャンプ
            buffer.emit8(0xE9);
            buffer.emit32(static_cast<uint32_t>(offset));
        }
    }

    // 関数呼び出し命令（レジスタ間接）
    inline void encodeCALL_reg(CodeBuffer& buffer, Register reg) {
        // REXプレフィックス（必要な場合）
        if ((reg & 0x8) != 0) {
            buffer.emit8(encodeREX(false, false, false, true));
        }
        
        // CALL r/m64 (0xFF /2)
        buffer.emit8(0xFF);
        buffer.emit8(encodeModRM(MOD_DIRECT, 2, reg & 0x7));  // 2 = CALL opcode extension
    }

    // リターン命令
    inline void encodeRET(CodeBuffer& buffer) {
        buffer.emit8(0xC3);
    }

    // 64ビット即値をレジスタにロード（最適化版）
    inline void emitMOV_imm64(CodeBuffer& buffer, Register rd, uint64_t imm) {
        if (imm == 0) {
            // XOR reg, reg はゼロクリア用の一般的な最適化
            bool rex_w = true;
            bool rex_r = (rd & 0x8) != 0;
            bool rex_b = (rd & 0x8) != 0;
            buffer.emit8(encodeREX(rex_w, rex_r, false, rex_b));
            buffer.emit8(0x33);  // XOR
            buffer.emit8(encodeModRM(MOD_DIRECT, rd & 0x7, rd & 0x7));
        } else if (imm <= 0xFFFFFFFF) {
            // 32ビット即値をゼロ拡張
            bool rex_w = true;
            bool rex_b = (rd & 0x8) != 0;
            buffer.emit8(encodeREX(rex_w, false, false, rex_b));
            buffer.emit8(0xB8 + (rd & 0x7));  // MOV r32, imm32
            buffer.emit32(static_cast<uint32_t>(imm));
        } else {
            // 完全な64ビット即値をロード
            encodeMOV_reg_imm64(buffer, rd, imm);
        }
    }
}

// X86_64用のモノモーフィックプロパティアクセススタブ生成
std::unique_ptr<NativeCode> X86_64_ICGenerator::generateMonomorphicPropertyStub(void* cachePtr) {
    auto cache = static_cast<PropertyCache*>(cachePtr);
    if (!cache || cache->getEntries().empty()) {
        return nullptr;
    }
    
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 64KBの初期バッファを確保
    buffer.reserve(65536);
    
    // スタブは以下の引数を受け取る（X86_64呼出規約に従う）：
    // RDI: オブジェクトポインタ
    // RSI: プロパティ名（文字列ポインタ）- 今回は使用しない
    // このスタブは結果をRAXに返す
    
    // エントリ取得（モノモーフィックなので最初のエントリのみ使用）
    const auto& entry = cache->getEntries()[0];
    
    // 1. シェイプIDをチェック
    // オブジェクトのシェイプIDを取得（典型的にはオブジェクトヘッダの最初のフィールド）
    // MOV RAX, [RDI]       // オブジェクトからシェイプIDを取得
    x86_64::encodeMOV_reg_mem(buffer, x86_64::RAX, x86_64::RDI, 0);
    
    // シェイプIDと期待値を比較
    // CMP RAX, シェイプID
    x86_64::encodeCMP_reg_imm32(buffer, x86_64::RAX, static_cast<uint32_t>(entry.shapeId));
    
    // 不一致の場合は汎用パスにジャンプ
    // JNE miss
    size_t missJumpOffset = buffer.size();
    buffer.emit8(0x0F);
    buffer.emit8(0x85);
    buffer.emit32(0);  // プレースホルダー（後で埋める）
    
    // 2. キャッシュヒット時のプロパティアクセス
    if (entry.isInlineProperty) {
        // インラインプロパティの場合: オブジェクト+ 固定オフセットから直接ロード
        // MOV RAX, [RDI + offset]
        x86_64::encodeMOV_reg_mem(buffer, x86_64::RAX, x86_64::RDI, entry.slotOffset);
    } else {
        // アウトオブラインプロパティの場合: スロット配列からロード
        // MOV RAX, [RDI + 8]   // スロット配列ポインタ取得
        x86_64::encodeMOV_reg_mem(buffer, x86_64::RAX, x86_64::RDI, 8);
        
        // MOV RAX, [RAX + (slotOffset * 8)]  // スロットからValueを取得
        x86_64::encodeMOV_reg_mem(buffer, x86_64::RAX, x86_64::RAX, entry.slotOffset * 8);
    }
    
    // キャッシュヒット時は結果を返してリターン
    // RET
    x86_64::encodeRET(buffer);
    
    // 3. キャッシュミス時の処理
    // ミスハンドラ呼び出しのコードをここに配置
    size_t missOffset = buffer.size();
    
    // ジャンプオフセットを埋める
    int32_t displacement = static_cast<int32_t>(missOffset - (missJumpOffset + 6));  // +6 はジャンプ命令自体のサイズ
    *reinterpret_cast<uint32_t*>(buffer.data() + missJumpOffset + 2) = static_cast<uint32_t>(displacement);
    
    // キャッシュミスハンドラの呼び出し
    // 引数はすでにRDI（オブジェクト）とRSI（プロパティ名）にセットされている
    // 第3引数としてキャッシュIDをRDXに設定
    // MOV RDX, cacheId
    x86_64::emitMOV_imm64(buffer, x86_64::RDX, cache->getCacheId());
    
    // handlePropertyMiss関数のアドレスをRAXにロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handlePropertyMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // MOV RAX, missHandlerAddr
    x86_64::emitMOV_imm64(buffer, x86_64::RAX, missHandlerAddr);
    
    // CALL RAX
    x86_64::encodeCALL_reg(buffer, x86_64::RAX);
    
    // ミスハンドラから戻ったらリターン
    // RET
    x86_64::encodeRET(buffer);
    
    // 実行可能にする
    buffer.makeExecutable();
    
    return code;
}

// X86_64用のポリモーフィックプロパティアクセススタブ生成
std::unique_ptr<NativeCode> X86_64_ICGenerator::generatePolymorphicPropertyStub(void* cachePtr) {
    auto cache = static_cast<PropertyCache*>(cachePtr);
    if (!cache || cache->getEntries().size() <= 1) {
        return nullptr;
    }
    
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 64KBの初期バッファを確保
    buffer.reserve(65536);
    
    // スタブは以下の引数を受け取る（X86_64呼出規約に従う）：
    // RDI: オブジェクトポインタ
    // RSI: プロパティ名（文字列ポインタ）
    
    // 1. シェイプIDを取得
    // MOV RAX, [RDI]       // オブジェクトからシェイプIDを取得
    x86_64::encodeMOV_reg_mem(buffer, x86_64::RAX, x86_64::RDI, 0);
    
    // 2. 各エントリに対する分岐を生成
    std::vector<size_t> missJumpOffsets;
    
    for (const auto& entry : cache->getEntries()) {
        // シェイプIDと期待値を比較
        // CMP RAX, シェイプID
        x86_64::encodeCMP_reg_imm32(buffer, x86_64::RAX, static_cast<uint32_t>(entry.shapeId));
        
        // 一致しない場合は次のエントリをチェック
        // JNE next_check
        size_t jumpOffset = buffer.size();
        buffer.emit8(0x0F);
        buffer.emit8(0x85);
        buffer.emit32(0);  // プレースホルダー
        missJumpOffsets.push_back(jumpOffset);
        
        // プロパティアクセスコード
        if (entry.isInlineProperty) {
            // インラインプロパティの場合
            // MOV RAX, [RDI + offset]
            x86_64::encodeMOV_reg_mem(buffer, x86_64::RAX, x86_64::RDI, entry.slotOffset);
        } else {
            // アウトオブラインプロパティの場合
            // MOV RAX, [RDI + 8]   // スロット配列ポインタ取得
            x86_64::encodeMOV_reg_mem(buffer, x86_64::RAX, x86_64::RDI, 8);
            
            // MOV RAX, [RAX + (slotOffset * 8)]  // スロットからValueを取得
            x86_64::encodeMOV_reg_mem(buffer, x86_64::RAX, x86_64::RAX, entry.slotOffset * 8);
        }
        
        // リターン
        // RET
        x86_64::encodeRET(buffer);
        
        // 次のエントリのコードの開始位置
        size_t nextOffset = buffer.size();
        
        // 前のジャンプオフセットを埋める
        int32_t displacement = static_cast<int32_t>(nextOffset - (jumpOffset + 6));  // +6 はジャンプ命令自体のサイズ
        *reinterpret_cast<uint32_t*>(buffer.data() + jumpOffset + 2) = static_cast<uint32_t>(displacement);
    }
    
    // 3. キャッシュミス時の共通処理
    size_t missOffset = buffer.size();
    
    // キャッシュミスハンドラの呼び出し
    // MOV RDX, cacheId
    x86_64::emitMOV_imm64(buffer, x86_64::RDX, cache->getCacheId());
    
    // handlePropertyMiss関数のアドレスをRAXにロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handlePropertyMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // MOV RAX, missHandlerAddr
    x86_64::emitMOV_imm64(buffer, x86_64::RAX, missHandlerAddr);
    
    // CALL RAX
    x86_64::encodeCALL_reg(buffer, x86_64::RAX);
    
    // ミスハンドラから戻ったらリターン
    // RET
    x86_64::encodeRET(buffer);
    
    // 実行可能にする
    buffer.makeExecutable();
    
    return code;
}

// X86_64用のメガモーフィックプロパティアクセススタブ生成
std::unique_ptr<NativeCode> X86_64_ICGenerator::generateMegamorphicPropertyStub(uint64_t siteId) {
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 64KBの初期バッファを確保
    buffer.reserve(65536);
    
    // スタブは以下の引数を受け取る（X86_64呼出規約に従う）：
    // RDI: オブジェクトポインタ
    // RSI: プロパティ名（文字列ポインタ）
    
    // 直接キャッシュミスハンドラを呼び出す
    // メガモーフィックの場合、常にグローバルハンドラにリダイレクト
    
    // MOV RDX, siteId
    x86_64::emitMOV_imm64(buffer, x86_64::RDX, siteId);
    
    // handlePropertyMiss関数のアドレスをRAXにロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handlePropertyMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // MOV RAX, missHandlerAddr
    x86_64::emitMOV_imm64(buffer, x86_64::RAX, missHandlerAddr);
    
    // CALL RAX
    x86_64::encodeCALL_reg(buffer, x86_64::RAX);
    
    // ミスハンドラから戻ったらリターン
    // RET
    x86_64::encodeRET(buffer);
    
    // 実行可能にする
    buffer.makeExecutable();
    
    return code;
}

// X86_64用のモノモーフィックメソッド呼び出しスタブ生成
std::unique_ptr<NativeCode> X86_64_ICGenerator::generateMonomorphicMethodStub(void* cachePtr) {
    auto cache = static_cast<MethodCache*>(cachePtr);
    if (!cache || cache->getEntries().empty()) {
        return nullptr;
    }
    
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 64KBの初期バッファを確保
    buffer.reserve(65536);
    
    // スタブは以下の引数を受け取る（X86_64呼出規約に従う）：
    // RDI: オブジェクトポインタ（thisオブジェクト）
    // RSI: メソッド名（文字列ポインタ）
    // RDX: 引数配列ポインタ
    // RCX: 引数カウント
    
    // エントリ取得（モノモーフィックなので最初のエントリのみ使用）
    const auto& entry = cache->getEntries()[0];
    
    // 1. シェイプIDをチェック
    // オブジェクトのシェイプIDを取得
    // MOV RAX, [RDI]       // オブジェクトからシェイプIDを取得
    x86_64::encodeMOV_reg_mem(buffer, x86_64::RAX, x86_64::RDI, 0);
    
    // シェイプIDと期待値を比較
    // CMP RAX, シェイプID
    x86_64::encodeCMP_reg_imm32(buffer, x86_64::RAX, static_cast<uint32_t>(entry.shapeId));
    
    // 不一致の場合は汎用パスにジャンプ
    // JNE miss
    size_t missJumpOffset = buffer.size();
    buffer.emit8(0x0F);
    buffer.emit8(0x85);
    buffer.emit32(0);  // プレースホルダー
    
    // 2. キャッシュヒット時の処理
    // 関数を直接呼び出す
    
    // MOV RAX, codeAddress
    x86_64::emitMOV_imm64(buffer, x86_64::RAX, reinterpret_cast<uint64_t>(entry.codeAddress));
    
    // thisオブジェクトはRDIに既にある
    // 引数配列はRDXに既にある
    // 引数カウントはRCXに既にある
    
    // CALL RAX
    x86_64::encodeCALL_reg(buffer, x86_64::RAX);
    
    // メソッド呼び出し後はそのままリターン（結果はRAXに入っている）
    // RET
    x86_64::encodeRET(buffer);
    
    // 3. キャッシュミス時の処理
    size_t missOffset = buffer.size();
    
    // ジャンプオフセットを埋める
    int32_t displacement = static_cast<int32_t>(missOffset - (missJumpOffset + 6));  // +6 はジャンプ命令自体のサイズ
    *reinterpret_cast<uint32_t*>(buffer.data() + missJumpOffset + 2) = static_cast<uint32_t>(displacement);
    
    // キャッシュミスハンドラの呼び出し準備
    // System V AMD64 ABIでは第4引数はRCXにあるが、第5引数がR8になる
    // MOV R8, cacheId
    x86_64::emitMOV_imm64(buffer, x86_64::R8, cache->getCacheId());
    
    // handleMethodMiss関数のアドレスをRAXにロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handleMethodMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // MOV RAX, missHandlerAddr
    x86_64::emitMOV_imm64(buffer, x86_64::RAX, missHandlerAddr);
    
    // CALL RAX
    x86_64::encodeCALL_reg(buffer, x86_64::RAX);
    
    // ミスハンドラから戻ったらリターン
    // RET
    x86_64::encodeRET(buffer);
    
    // 実行可能にする
    buffer.makeExecutable();
    
    return code;
}

// X86_64用のポリモーフィックメソッド呼び出しスタブ生成
std::unique_ptr<NativeCode> X86_64_ICGenerator::generatePolymorphicMethodStub(void* cachePtr) {
    auto cache = static_cast<MethodCache*>(cachePtr);
    if (!cache || cache->getEntries().size() <= 1) {
        return nullptr;
    }
    
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 64KBの初期バッファを確保
    buffer.reserve(65536);
    
    // スタブは以下の引数を受け取る（X86_64呼出規約に従う）：
    // RDI: オブジェクトポインタ（thisオブジェクト）
    // RSI: メソッド名（文字列ポインタ）
    // RDX: 引数配列ポインタ
    // RCX: 引数カウント
    
    // 1. シェイプIDを取得
    // MOV RAX, [RDI]       // オブジェクトからシェイプIDを取得
    x86_64::encodeMOV_reg_mem(buffer, x86_64::RAX, x86_64::RDI, 0);
    
    // 2. 各エントリに対する分岐を生成
    std::vector<size_t> nextJumpOffsets;
    
    for (size_t i = 0; i < cache->getEntries().size(); i++) {
        const auto& entry = cache->getEntries()[i];
        
        // シェイプIDと期待値を比較
        // CMP RAX, シェイプID
        x86_64::encodeCMP_reg_imm32(buffer, x86_64::RAX, static_cast<uint32_t>(entry.shapeId));
        
        // 一致しない場合は次のエントリをチェック
        // JNE next_entry
        size_t jumpOffset = buffer.size();
        buffer.emit8(0x0F);
        buffer.emit8(0x85);
        buffer.emit32(0);  // プレースホルダー
        nextJumpOffsets.push_back(jumpOffset);
        
        // MOV RAX, codeAddress
        x86_64::emitMOV_imm64(buffer, x86_64::RAX, reinterpret_cast<uint64_t>(entry.codeAddress));
        
        // CALL RAX
        x86_64::encodeCALL_reg(buffer, x86_64::RAX);
        
        // RET
        x86_64::encodeRET(buffer);
        
        // 次のエントリの開始位置
        size_t nextOffset = buffer.size();
        
        // 前のジャンプオフセットを埋める
        int32_t displacement = static_cast<int32_t>(nextOffset - (jumpOffset + 6));  // +6 はジャンプ命令自体のサイズ
        *reinterpret_cast<uint32_t*>(buffer.data() + jumpOffset + 2) = static_cast<uint32_t>(displacement);
    }
    
    // 3. キャッシュミス時の共通処理
    size_t missOffset = buffer.size();
    
    // キャッシュミスハンドラの呼び出し
    // MOV R8, cacheId
    x86_64::emitMOV_imm64(buffer, x86_64::R8, cache->getCacheId());
    
    // handleMethodMiss関数のアドレスをRAXにロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handleMethodMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // MOV RAX, missHandlerAddr
    x86_64::emitMOV_imm64(buffer, x86_64::RAX, missHandlerAddr);
    
    // CALL RAX
    x86_64::encodeCALL_reg(buffer, x86_64::RAX);
    
    // ミスハンドラから戻ったらリターン
    // RET
    x86_64::encodeRET(buffer);
    
    // 実行可能にする
    buffer.makeExecutable();
    
    return code;
}

// X86_64用のメガモーフィックメソッド呼び出しスタブ生成
std::unique_ptr<NativeCode> X86_64_ICGenerator::generateMegamorphicMethodStub(uint64_t siteId) {
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 64KBの初期バッファを確保
    buffer.reserve(65536);
    
    // スタブは以下の引数を受け取る（X86_64呼出規約に従う）：
    // RDI: オブジェクトポインタ（thisオブジェクト）
    // RSI: メソッド名（文字列ポインタ）
    // RDX: 引数配列ポインタ
    // RCX: 引数カウント
    
    // 直接キャッシュミスハンドラを呼び出す
    // メガモーフィックの場合、常にグローバルハンドラにリダイレクト
    
    // MOV R8, siteId
    x86_64::emitMOV_imm64(buffer, x86_64::R8, siteId);
    
    // handleMethodMiss関数のアドレスをRAXにロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handleMethodMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // MOV RAX, missHandlerAddr
    x86_64::emitMOV_imm64(buffer, x86_64::RAX, missHandlerAddr);
    
    // CALL RAX
    x86_64::encodeCALL_reg(buffer, x86_64::RAX);
    
    // ミスハンドラから戻ったらリターン
    // RET
    x86_64::encodeRET(buffer);
    
    // 実行可能にする
    buffer.makeExecutable();
    
    return code;
}

} // namespace core
} // namespace aerojs 