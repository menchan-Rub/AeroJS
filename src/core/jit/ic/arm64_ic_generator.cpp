/**
 * @file arm64_ic_generator.cpp
 * @brief ARM64アーキテクチャ向けのインラインキャッシュコード生成の実装
 * @version 1.0.0
 * @license MIT
 */

#include "arm64_ic_generator.h"
#include "inline_cache.h"
#include <cassert>

namespace aerojs {
namespace core {

// ARM64アーキテクチャの命令エンコーディング定数
namespace arm64 {
    // 条件フラグ
    enum Condition {
        EQ = 0x0,  // 等しい
        NE = 0x1,  // 等しくない
        CS = 0x2,  // キャリーセット
        CC = 0x3,  // キャリークリア
        MI = 0x4,  // マイナス
        PL = 0x5,  // プラスまたはゼロ
        VS = 0x6,  // オーバーフロー
        VC = 0x7,  // オーバーフローなし
        HI = 0x8,  // 符号なし大きい
        LS = 0x9,  // 符号なし小さいまたは等しい
        GE = 0xA,  // 符号付き大きいまたは等しい
        LT = 0xB,  // 符号付き小さい
        GT = 0xC,  // 符号付き大きい
        LE = 0xD,  // 符号付き小さいまたは等しい
        AL = 0xE,  // 常に
        NV = 0xF   // 未定義
    };

    // レジスタ定義
    enum Register {
        X0 = 0, X1, X2, X3, X4, X5, X6, X7, 
        X8, X9, X10, X11, X12, X13, X14, X15, 
        X16, X17, X18, X19, X20, X21, X22, X23, 
        X24, X25, X26, X27, X28, X29, X30, XZR
    };

    // ARM64の呼び出し規約：
    // X0-X7: 引数/結果レジスタ
    // X8: 間接結果レジスタ
    // X9-X15: 一時レジスタ
    // X16-X17: 特殊用途（IPレジスタ）
    // X18: プラットフォーム固有
    // X19-X28: 保存されるレジスタ
    // X29: フレームポインタ
    // X30: リンクレジスタ（リターンアドレス）
    // XZR/X31: ゼロレジスタ

    // ARM64命令エンコーディングヘルパー関数

    // 32ビット命令をエンコード
    inline uint32_t encodeInstruction(uint32_t opcode) {
        return opcode;
    }

    // ロード/ストア命令をエンコード（レジスタ+即値オフセット）
    inline uint32_t encodeLDR_STR_imm(bool isLoad, Register rt, Register rn, int32_t imm12, bool is64bit = true) {
        uint32_t size = is64bit ? 0x3 : 0x2;
        uint32_t opc = isLoad ? 0x1 : 0x0;
        uint32_t instruction = (size << 30) | (opc << 22) | (1 << 24) | ((imm12 & 0xFFF) << 10) | (rn << 5) | rt;
        return instruction;
    }

    // 比較命令をエンコード
    inline uint32_t encodeCMP_imm(Register rn, uint32_t imm12, bool is64bit = true) {
        uint32_t sf = is64bit ? 1 : 0;
        uint32_t op = 1;  // CMP (SUB with rd=XZR)
        uint32_t instruction = (sf << 31) | (op << 30) | (1 << 29) | (0b10001 << 24) | 
                             ((imm12 & 0xFFF) << 10) | (rn << 5) | XZR;
        return instruction;
    }

    // 条件付き分岐をエンコード
    inline uint32_t encodeB_cond(Condition cond, int32_t offset) {
        int32_t imm19 = (offset >> 2) & 0x7FFFF;  // 19ビットに変換、4バイト単位
        uint32_t instruction = (0b01010100 << 24) | (imm19 << 5) | (0 << 4) | cond;
        return instruction;
    }

    // 無条件分岐をエンコード
    inline uint32_t encodeB(int32_t offset) {
        int32_t imm26 = (offset >> 2) & 0x3FFFFFF;  // 26ビットに変換、4バイト単位
        uint32_t instruction = (0b000101 << 26) | imm26;
        return instruction;
    }

    // レジスタ移動命令をエンコード
    inline uint32_t encodeMOV_reg(Register rd, Register rn, bool is64bit = true) {
        uint32_t sf = is64bit ? 1 : 0;
        uint32_t instruction = (sf << 31) | (0b00101010000 << 20) | (rn << 16) | (0 << 10) | (XZR << 5) | rd;
        return instruction;
    }

    // 即値ロード命令をエンコード（64ビット値）
    inline void encodeMOV_imm64(aerojs::core::jit::CodeBuffer& buffer, Register rd, uint64_t imm64) {
        // MOVZ Rd, #imm16, LSL #(0|16|32|48)
        // MOVK Rd, #imm16, LSL #(0|16|32|48)
        // sf=1 (64-bit), opc (MOVZ:10, MOVK:11)
        // hw: 00 (LSL 0), 01 (LSL 16), 10 (LSL 32), 11 (LSL 48)

        uint32_t rd_val = static_cast<uint32_t>(rd);

        // MOVZ Xd, #(imm & 0xFFFF), LSL #0
        // opc=10 (MOVZ), hw=00
        uint16_t chunk0 = imm64 & 0xFFFF;
        buffer.emit32( (1U << 31) | (0b10U << 29) | (0b00U << 21) | (chunk0 << 5) | rd_val );

        // MOVK Xd, #((imm >> 16) & 0xFFFF), LSL #16
        // opc=11 (MOVK), hw=01
        if ((imm64 >> 16) != 0 || imm64 > 0xFFFF) { // 値が0でも上位ビットがゼロでない場合があるため、>0xFFFF もチェック
            uint16_t chunk1 = (imm64 >> 16) & 0xFFFF;
            buffer.emit32( (1U << 31) | (0b11U << 29) | (0b01U << 21) | (chunk1 << 5) | rd_val );
        }

        // MOVK Xd, #((imm >> 32) & 0xFFFF), LSL #32
        // opc=11 (MOVK), hw=10
        if ((imm64 >> 32) != 0 || imm64 > 0xFFFFFFFF) {
            uint16_t chunk2 = (imm64 >> 32) & 0xFFFF;
            buffer.emit32( (1U << 31) | (0b11U << 29) | (0b10U << 21) | (chunk2 << 5) | rd_val );
        }

        // MOVK Xd, #((imm >> 48) & 0xFFFF), LSL #48
        // opc=11 (MOVK), hw=11
        if ((imm64 >> 48) != 0 || imm64 > 0xFFFFFFFFFFFF) {
            uint16_t chunk3 = (imm64 >> 48) & 0xFFFF;
            buffer.emit32( (1U << 31) | (0b11U << 29) | (0b11U << 21) | (chunk3 << 5) | rd_val );
        }
    }

    // 関数呼び出し命令をエンコード
    inline uint32_t encodeBLR(Register rn) {
        uint32_t instruction = (0b1101011000111111000000 << 10) | (rn << 5) | 0;
        return instruction;
    }

    // リターン命令をエンコード
    inline uint32_t encodeRET() {
        // X30（リンクレジスタ）からのリターン
        uint32_t instruction = (0b1101011000111111000000 << 10) | (X30 << 5) | 0;
        return instruction;
    }
}

// ARM64用のモノモーフィックプロパティアクセススタブ生成
std::unique_ptr<NativeCode> ARM64_ICGenerator::generateMonomorphicPropertyStub(void* cachePtr) {
    auto cache = static_cast<PropertyCache*>(cachePtr);
    if (!cache || cache->getEntries().empty()) {
        return nullptr;
    }
    
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 64KBの初期バッファを確保
    buffer.reserve(65536);
    
    // スタブは以下の引数を受け取る（ARM64呼出規約に従う）：
    // X0: オブジェクトポインタ
    // X1: プロパティ名（文字列ポインタ）- 今回は使用しない
    // このスタブは結果をX0に返す
    
    // エントリ取得（モノモーフィックなので最初のエントリのみ使用）
    const auto& entry = cache->getEntries()[0];
    
    // 1. シェイプIDをチェック
    // オブジェクトのシェイプIDを取得（典型的にはオブジェクトヘッダの最初のフィールド）
    // LDR X9, [X0]       // オブジェクトからシェイプIDを取得
    buffer.emit32(arm64::encodeLDR_STR_imm(true, arm64::X9, arm64::X0, 0));
    
    // シェイプIDと期待値を比較
    // CMP X9, #シェイプID
    buffer.emit32(arm64::encodeCMP_imm(arm64::X9, static_cast<uint32_t>(entry.shapeId)));
    
    // 不一致の場合は汎用パスにジャンプ
    // B.NE miss
    // 注: 後でオフセットを埋める必要あり
    size_t missJumpOffset = buffer.size();
    buffer.emit32(0);  // プレースホルダー
    
    // 2. キャッシュヒット時のプロパティアクセス
    if (entry.isInlineProperty) {
        // インラインプロパティの場合: オブジェクト+ 固定オフセットから直接ロード
        // LDR X0, [X0, #offset]
        buffer.emit32(arm64::encodeLDR_STR_imm(true, arm64::X0, arm64::X0, entry.slotOffset));
    } else {
        // アウトオブラインプロパティの場合: スロット配列からロード
        // LDR X9, [X0, #8]   // スロット配列ポインタ取得
        buffer.emit32(arm64::encodeLDR_STR_imm(true, arm64::X9, arm64::X0, 8));
        
        // LDR X0, [X9, #(slotOffset * 8)]  // スロットからValueを取得
        buffer.emit32(arm64::encodeLDR_STR_imm(true, arm64::X0, arm64::X9, entry.slotOffset * 8));
    }
    
    // キャッシュヒット時は結果を返してリターン
    // RET
    buffer.emit32(arm64::encodeRET());
    
    // 3. キャッシュミス時の処理
    // ミスハンドラ呼び出しのコードをここに配置
    size_t missOffset = buffer.size();
    
    // ジャンプオフセットを埋める
    int32_t displacement = static_cast<int32_t>(missOffset - missJumpOffset);
    *reinterpret_cast<uint32_t*>(buffer.data() + missJumpOffset) = 
        arm64::encodeB_cond(arm64::NE, displacement);
    
    // キャッシュミスハンドラの呼び出し
    // 引数はすでにX0（オブジェクト）とX1（プロパティ名）にセットされている
    // X2に第3引数としてキャッシュIDを設定
    // MOV X2, #cacheId
    // 64ビット即値を単一命令でロードできないため、複数命令に分割
    uint64_t cacheId = cache->getCacheId();
    
    // MOVZ X2, #(cacheId & 0xFFFF)
    buffer.emit32(0xD2800002 | ((cacheId & 0xFFFF) << 5));
    
    // MOVK X2, #((cacheId >> 16) & 0xFFFF), LSL #16
    if (cacheId > 0xFFFF) {
        buffer.emit32(0xF2A00002 | (((cacheId >> 16) & 0xFFFF) << 5));
    }
    
    // MOVK X2, #((cacheId >> 32) & 0xFFFF), LSL #32
    if (cacheId > 0xFFFFFFFF) {
        buffer.emit32(0xF2C00002 | (((cacheId >> 32) & 0xFFFF) << 5));
    }
    
    // MOVK X2, #((cacheId >> 48) & 0xFFFF), LSL #48
    if (cacheId > 0xFFFFFFFFFFFF) {
        buffer.emit32(0xF2E00002 | (((cacheId >> 48) & 0xFFFF) << 5));
    }
    
    // handlePropertyMiss関数のアドレスをX9にロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handlePropertyMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // MOVZ X9, #(missHandlerAddr & 0xFFFF)
    buffer.emit32(0xD2800009 | ((missHandlerAddr & 0xFFFF) << 5));
    
    // MOVK X9, #((missHandlerAddr >> 16) & 0xFFFF), LSL #16
    if (missHandlerAddr > 0xFFFF) {
        buffer.emit32(0xF2A00009 | (((missHandlerAddr >> 16) & 0xFFFF) << 5));
    }
    
    // MOVK X9, #((missHandlerAddr >> 32) & 0xFFFF), LSL #32
    if (missHandlerAddr > 0xFFFFFFFF) {
        buffer.emit32(0xF2C00009 | (((missHandlerAddr >> 32) & 0xFFFF) << 5));
    }
    
    // MOVK X9, #((missHandlerAddr >> 48) & 0xFFFF), LSL #48
    if (missHandlerAddr > 0xFFFFFFFFFFFF) {
        buffer.emit32(0xF2E00009 | (((missHandlerAddr >> 48) & 0xFFFF) << 5));
    }
    
    // BLR X9
    buffer.emit32(arm64::encodeBLR(arm64::X9));
    
    // ミスハンドラから戻ったらリターン
    // RET
    buffer.emit32(arm64::encodeRET());
    
    // 実行可能にする
    buffer.makeExecutable();
    
    return code;
}

// ARM64用のポリモーフィックプロパティアクセススタブ生成
std::unique_ptr<NativeCode> ARM64_ICGenerator::generatePolymorphicPropertyStub(void* cachePtr) {
    auto cache = static_cast<PropertyCache*>(cachePtr);
    if (!cache || cache->getEntries().size() <= 1) {
        return nullptr;
    }
    
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 64KBの初期バッファを確保
    buffer.reserve(65536);
    
    // スタブは以下の引数を受け取る（ARM64呼出規約に従う）：
    // X0: オブジェクトポインタ
    // X1: プロパティ名（文字列ポインタ）
    
    // 1. シェイプIDを取得
    // LDR X9, [X0]       // オブジェクトからシェイプIDを取得
    buffer.emit32(arm64::encodeLDR_STR_imm(true, arm64::X9, arm64::X0, 0));
    
    // 2. 各エントリに対する分岐を生成
    std::vector<size_t> missJumpOffsets;
    
    for (const auto& entry : cache->getEntries()) {
        // シェイプIDと期待値を比較
        // CMP X9, #シェイプID
        buffer.emit32(arm64::encodeCMP_imm(arm64::X9, static_cast<uint32_t>(entry.shapeId)));
        
        // 一致しない場合は次のエントリをチェック
        // B.NE next_check
        size_t jumpOffset = buffer.size();
        buffer.emit32(0);  // プレースホルダー
        missJumpOffsets.push_back(jumpOffset);
        
        // プロパティアクセスコード
        if (entry.isInlineProperty) {
            // インラインプロパティの場合
            // LDR X0, [X0, #offset]
            buffer.emit32(arm64::encodeLDR_STR_imm(true, arm64::X0, arm64::X0, entry.slotOffset));
        } else {
            // アウトオブラインプロパティの場合
            // LDR X9, [X0, #8]   // スロット配列ポインタ取得
            buffer.emit32(arm64::encodeLDR_STR_imm(true, arm64::X9, arm64::X0, 8));
            
            // LDR X0, [X9, #(slotOffset * 8)]  // スロットからValueを取得
            buffer.emit32(arm64::encodeLDR_STR_imm(true, arm64::X0, arm64::X9, entry.slotOffset * 8));
        }
        
        // リターン
        // RET
        buffer.emit32(arm64::encodeRET());
        
        // 次のエントリのコードの開始位置
        size_t nextOffset = buffer.size();
        
        // 前のジャンプオフセットを埋める
        int32_t displacement = static_cast<int32_t>(nextOffset - jumpOffset);
        *reinterpret_cast<uint32_t*>(buffer.data() + jumpOffset) = 
            arm64::encodeB_cond(arm64::NE, displacement);
    }
    
    // 3. キャッシュミス時の共通処理
    size_t missOffset = buffer.size();
    
    // キャッシュミスハンドラの呼び出し
    // MOV X2, #cacheId
    uint64_t cacheId = cache->getCacheId();
    
    // MOVZ X2, #(cacheId & 0xFFFF)
    buffer.emit32(0xD2800002 | ((cacheId & 0xFFFF) << 5));
    
    // MOVK X2, #((cacheId >> 16) & 0xFFFF), LSL #16
    if (cacheId > 0xFFFF) {
        buffer.emit32(0xF2A00002 | (((cacheId >> 16) & 0xFFFF) << 5));
    }
    
    // MOVK X2, #((cacheId >> 32) & 0xFFFF), LSL #32
    if (cacheId > 0xFFFFFFFF) {
        buffer.emit32(0xF2C00002 | (((cacheId >> 32) & 0xFFFF) << 5));
    }
    
    // MOVK X2, #((cacheId >> 48) & 0xFFFF), LSL #48
    if (cacheId > 0xFFFFFFFFFFFF) {
        buffer.emit32(0xF2E00002 | (((cacheId >> 48) & 0xFFFF) << 5));
    }
    
    // handlePropertyMiss関数のアドレスをX9にロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handlePropertyMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // MOVZ X9, #(missHandlerAddr & 0xFFFF)
    buffer.emit32(0xD2800009 | ((missHandlerAddr & 0xFFFF) << 5));
    
    // MOVK X9, #((missHandlerAddr >> 16) & 0xFFFF), LSL #16
    if (missHandlerAddr > 0xFFFF) {
        buffer.emit32(0xF2A00009 | (((missHandlerAddr >> 16) & 0xFFFF) << 5));
    }
    
    // MOVK X9, #((missHandlerAddr >> 32) & 0xFFFF), LSL #32
    if (missHandlerAddr > 0xFFFFFFFF) {
        buffer.emit32(0xF2C00009 | (((missHandlerAddr >> 32) & 0xFFFF) << 5));
    }
    
    // MOVK X9, #((missHandlerAddr >> 48) & 0xFFFF), LSL #48
    if (missHandlerAddr > 0xFFFFFFFFFFFF) {
        buffer.emit32(0xF2E00009 | (((missHandlerAddr >> 48) & 0xFFFF) << 5));
    }
    
    // BLR X9
    buffer.emit32(arm64::encodeBLR(arm64::X9));
    
    // ミスハンドラから戻ったらリターン
    // RET
    buffer.emit32(arm64::encodeRET());
    
    // 実行可能にする
    buffer.makeExecutable();
    
    return code;
}

// ARM64用のメガモーフィックプロパティアクセススタブ生成
std::unique_ptr<NativeCode> ARM64_ICGenerator::generateMegamorphicPropertyStub(uint64_t siteId) {
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 64KBの初期バッファを確保
    buffer.reserve(65536);
    
    // スタブは以下の引数を受け取る（ARM64呼出規約に従う）：
    // X0: オブジェクトポインタ
    // X1: プロパティ名（文字列ポインタ）
    
    // 直接キャッシュミスハンドラを呼び出す
    // メガモーフィックの場合、常にグローバルハンドラにリダイレクト
    
    // MOV X2, #siteId
    // MOVZ X2, #(siteId & 0xFFFF)
    buffer.emit32(0xD2800002 | ((siteId & 0xFFFF) << 5));
    
    // MOVK X2, #((siteId >> 16) & 0xFFFF), LSL #16
    if (siteId > 0xFFFF) {
        buffer.emit32(0xF2A00002 | (((siteId >> 16) & 0xFFFF) << 5));
    }
    
    // MOVK X2, #((siteId >> 32) & 0xFFFF), LSL #32
    if (siteId > 0xFFFFFFFF) {
        buffer.emit32(0xF2C00002 | (((siteId >> 32) & 0xFFFF) << 5));
    }
    
    // MOVK X2, #((siteId >> 48) & 0xFFFF), LSL #48
    if (siteId > 0xFFFFFFFFFFFF) {
        buffer.emit32(0xF2E00002 | (((siteId >> 48) & 0xFFFF) << 5));
    }
    
    // handlePropertyMiss関数のアドレスをX9にロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handlePropertyMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // MOVZ X9, #(missHandlerAddr & 0xFFFF)
    buffer.emit32(0xD2800009 | ((missHandlerAddr & 0xFFFF) << 5));
    
    // MOVK X9, #((missHandlerAddr >> 16) & 0xFFFF), LSL #16
    if (missHandlerAddr > 0xFFFF) {
        buffer.emit32(0xF2A00009 | (((missHandlerAddr >> 16) & 0xFFFF) << 5));
    }
    
    // MOVK X9, #((missHandlerAddr >> 32) & 0xFFFF), LSL #32
    if (missHandlerAddr > 0xFFFFFFFF) {
        buffer.emit32(0xF2C00009 | (((missHandlerAddr >> 32) & 0xFFFF) << 5));
    }
    
    // MOVK X9, #((missHandlerAddr >> 48) & 0xFFFF), LSL #48
    if (missHandlerAddr > 0xFFFFFFFFFFFF) {
        buffer.emit32(0xF2E00009 | (((missHandlerAddr >> 48) & 0xFFFF) << 5));
    }
    
    // BLR X9
    buffer.emit32(arm64::encodeBLR(arm64::X9));
    
    // ミスハンドラから戻ったらリターン
    // RET
    buffer.emit32(arm64::encodeRET());
    
    // 実行可能にする
    buffer.makeExecutable();
    
    return code;
}

// ARM64用のモノモーフィックメソッド呼び出しスタブ生成
std::unique_ptr<NativeCode> ARM64_ICGenerator::generateMonomorphicMethodStub(void* cachePtr) {
    auto cache = static_cast<MethodCache*>(cachePtr);
    if (!cache || cache->getEntries().empty()) {
        return nullptr;
    }
    
    auto code = std::make_unique<NativeCode>();
    CodeBuffer& buffer = code->buffer;
    
    // 64KBの初期バッファを確保
    buffer.reserve(65536);
    
    // スタブは以下の引数を受け取る（ARM64呼出規約に従う）：
    // X0: オブジェクトポインタ（thisオブジェクト）
    // X1: メソッド名（文字列ポインタ）
    // X2: 引数配列ポインタ
    // X3: 引数カウント
    
    // エントリ取得（モノモーフィックなので最初のエントリのみ使用）
    const auto& entry = cache->getEntries()[0];
    
    // 1. シェイプIDをチェック
    // オブジェクトのシェイプIDを取得
    // LDR X9, [X0]       // オブジェクトからシェイプIDを取得
    buffer.emit32(arm64::encodeLDR_STR_imm(true, arm64::X9, arm64::X0, 0));
    
    // シェイプIDと期待値を比較
    // CMP X9, #シェイプID
    buffer.emit32(arm64::encodeCMP_imm(arm64::X9, static_cast<uint32_t>(entry.shapeId)));
    
    // 不一致の場合は汎用パスにジャンプ
    // B.NE miss
    size_t missJumpOffset = buffer.size();
    buffer.emit32(0);  // プレースホルダー
    
    // 2. キャッシュヒット時の処理
    // 関数を直接呼び出す
    
    // エントリのコードアドレスをX9に移動
    void* codeAddr = entry.codeAddress;
    uint64_t codeAddrValue = reinterpret_cast<uint64_t>(codeAddr);
    
    // MOVZ X9, #(codeAddrValue & 0xFFFF)
    buffer.emit32(0xD2800009 | ((codeAddrValue & 0xFFFF) << 5));
    
    // MOVK X9, #((codeAddrValue >> 16) & 0xFFFF), LSL #16
    if (codeAddrValue > 0xFFFF) {
        buffer.emit32(0xF2A00009 | (((codeAddrValue >> 16) & 0xFFFF) << 5));
    }
    
    // MOVK X9, #((codeAddrValue >> 32) & 0xFFFF), LSL #32
    if (codeAddrValue > 0xFFFFFFFF) {
        buffer.emit32(0xF2C00009 | (((codeAddrValue >> 32) & 0xFFFF) << 5));
    }
    
    // MOVK X9, #((codeAddrValue >> 48) & 0xFFFF), LSL #48
    if (codeAddrValue > 0xFFFFFFFFFFFF) {
        buffer.emit32(0xF2E00009 | (((codeAddrValue >> 48) & 0xFFFF) << 5));
    }
    
    // thisオブジェクトはX0に既にある
    // 引数配列はX2に既にある
    // 引数カウントはX3に既にある
    
    // BLR X9
    buffer.emit32(arm64::encodeBLR(arm64::X9));
    
    // メソッド呼び出し後はそのままリターン（結果はX0に入っている）
    // RET
    buffer.emit32(arm64::encodeRET());
    
    // 3. キャッシュミス時の処理
    size_t missOffset = buffer.size();
    
    // ジャンプオフセットを埋める
    int32_t displacement = static_cast<int32_t>(missOffset - missJumpOffset);
    *reinterpret_cast<uint32_t*>(buffer.data() + missJumpOffset) = 
        arm64::encodeB_cond(arm64::NE, displacement);
    
    // キャッシュミスハンドラの呼び出し準備
    // 第4引数にキャッシュIDを設定（X4）
    uint64_t cacheId = cache->getCacheId();
    
    // MOVZ X4, #(cacheId & 0xFFFF)
    buffer.emit32(0xD2800004 | ((cacheId & 0xFFFF) << 5));
    
    // MOVK X4, #((cacheId >> 16) & 0xFFFF), LSL #16
    if (cacheId > 0xFFFF) {
        buffer.emit32(0xF2A00004 | (((cacheId >> 16) & 0xFFFF) << 5));
    }
    
    // MOVK X4, #((cacheId >> 32) & 0xFFFF), LSL #32
    if (cacheId > 0xFFFFFFFF) {
        buffer.emit32(0xF2C00004 | (((cacheId >> 32) & 0xFFFF) << 5));
    }
    
    // MOVK X4, #((cacheId >> 48) & 0xFFFF), LSL #48
    if (cacheId > 0xFFFFFFFFFFFF) {
        buffer.emit32(0xF2E00004 | (((cacheId >> 48) & 0xFFFF) << 5));
    }
    
    // handleMethodMiss関数のアドレスをX9にロード
    void* missHandlerPtr = reinterpret_cast<void*>(&InlineCacheManager::handleMethodMiss);
    uint64_t missHandlerAddr = reinterpret_cast<uint64_t>(missHandlerPtr);
    
    // MOVZ X9, #(missHandlerAddr & 0xFFFF)
    buffer.emit32(0xD2800009 | ((missHandlerAddr & 0xFFFF) << 5));
    
    // MOVK X9, #((missHandlerAddr >> 16) & 0xFFFF), LSL #16
    if (missHandlerAddr > 0xFFFF) {
        buffer.emit32(0xF2A00009 | (((missHandlerAddr >> 16) & 0xFFFF) << 5));
    }
    
    // MOVK X9, #((missHandlerAddr >> 32) & 0xFFFF), LSL #32
    if (missHandlerAddr > 0xFFFFFFFF) {
        buffer.emit32(0xF2C00009 | (((missHandlerAddr >> 32) & 0xFFFF) << 5));
    }
    
    // MOVK X9, #((missHandlerAddr >> 48) & 0xFFFF), LSL #48
    if (missHandlerAddr > 0xFFFFFFFFFFFF) {
        buffer.emit32(0xF2E00009 | (((missHandlerAddr >> 48) & 0xFFFF) << 5));
    }
    
    // BLR X9
    buffer.emit32(arm64::encodeBLR(arm64::X9));
    
    // ミスハンドラから戻ったらリターン
    // RET
    buffer.emit32(arm64::encodeRET());
    
    // 実行可能にする
    buffer.makeExecutable();
    
    return code;
}

// ARM64用のポリモーフィックメソッド呼び出しスタブとメガモーフィックメソッド呼び出しスタブの実装

std::unique_ptr<NativeCode> ARM64_ICGenerator::generatePolymorphicMethodStub(void* cachePtr) {
    // ポリモーフィックメソッド呼び出し用スタブの完全実装
    
    auto assembler = std::make_unique<ARM64Assembler>();
    
    // スタブエントリポイント
    Label stubEntry;
    assembler->bind(&stubEntry);
    
    // BTI (Branch Target Identification) 保護 - ARMv8.5+
    if (m_cpuFeatures.supportsBTI) {
        assembler->bti(BranchTargetType::JC);
    }
    
    // レジスタ保存
    assembler->stp(ARM64Register::X29, ARM64Register::X30, MemOperand::PreIndex(ARM64Register::SP, -16));
    assembler->mov(ARM64Register::X29, ARM64Register::SP);
    
    // 引数とキャッシュ情報を保存
    assembler->stp(ARM64Register::X0, ARM64Register::X1, MemOperand::PreIndex(ARM64Register::SP, -16));
    assembler->stp(ARM64Register::X2, ARM64Register::X3, MemOperand::PreIndex(ARM64Register::SP, -16));
    
    // オブジェクトの型情報（Hidden Class）をロード
    // X0 = this オブジェクト, [X0] = Hidden Class ポインタ
    assembler->ldr(ARM64Register::X4, MemOperand(ARM64Register::X0, 0));
    
    // キャッシュエントリアドレスをロード
    assembler->mov(ARM64Register::X5, reinterpret_cast<uint64_t>(cachePtr));
    
    // ポリモーフィックキャッシュのチェック（最大4つのエントリ）
    const int maxPolymorphicEntries = 4;
    std::vector<Label> typeCheckLabels(maxPolymorphicEntries);
    std::vector<Label> callTargetLabels(maxPolymorphicEntries);
    Label cacheMissLabel;
    
    for (int i = 0; i < maxPolymorphicEntries; i++) {
        assembler->bind(&typeCheckLabels[i]);
        
        // キャッシュエントリ[i]の型をロード
        assembler->ldr(ARM64Register::X6, MemOperand(ARM64Register::X5, i * 16)); // 型ポインタ
        
        // 型の比較
        assembler->cmp(ARM64Register::X4, ARM64Register::X6);
        
        if (i < maxPolymorphicEntries - 1) {
            assembler->b_ne(&typeCheckLabels[i + 1]);
        } else {
            assembler->b_ne(&cacheMissLabel);
        }
        
        // 型が一致した場合、メソッドポインタをロード
        assembler->bind(&callTargetLabels[i]);
        assembler->ldr(ARM64Register::X7, MemOperand(ARM64Register::X5, i * 16 + 8)); // メソッドポインタ
        
        // 引数を復元
        assembler->ldp(ARM64Register::X2, ARM64Register::X3, MemOperand::PostIndex(ARM64Register::SP, 16));
        assembler->ldp(ARM64Register::X0, ARM64Register::X1, MemOperand::PostIndex(ARM64Register::SP, 16));
        
        // フレームを復元
        assembler->ldp(ARM64Register::X29, ARM64Register::X30, MemOperand::PostIndex(ARM64Register::SP, 16));
        
        // メソッドを呼び出し
        assembler->br(ARM64Register::X7);
    }
    
    // キャッシュミス処理
    assembler->bind(&cacheMissLabel);
    
    // 引数を復元
    assembler->ldp(ARM64Register::X2, ARM64Register::X3, MemOperand::PostIndex(ARM64Register::SP, 16));
    assembler->ldp(ARM64Register::X0, ARM64Register::X1, MemOperand::PostIndex(ARM64Register::SP, 16));
    
    // IC更新ルーチンを呼び出し
    assembler->mov(ARM64Register::X16, reinterpret_cast<uint64_t>(&handlePolymorphicCacheMiss));
    assembler->blr(ARM64Register::X16);
    
    // フレームを復元
    assembler->ldp(ARM64Register::X29, ARM64Register::X30, MemOperand::PostIndex(ARM64Register::SP, 16));
    
    // 更新されたメソッドポインタで再試行
    assembler->br(ARM64Register::X0);
    
    // コードを生成してNativeCodeオブジェクトを作成
    auto code = assembler->finalize();
    
    auto nativeCode = std::make_unique<NativeCode>();
    nativeCode->setCode(std::move(code));
    nativeCode->setEntryPoint(stubEntry.getAddress());
    nativeCode->setType(NativeCodeType::ICStub);
    nativeCode->setICType(ICType::PolymorphicMethodCall);
    
    return nativeCode;
}

std::unique_ptr<NativeCode> ARM64_ICGenerator::generateMegamorphicMethodStub(uint64_t siteId) {
    // メガモーフィックメソッド呼び出し用スタブの完全実装
    
    auto assembler = std::make_unique<ARM64Assembler>();
    
    // スタブエントリポイント
    Label stubEntry;
    assembler->bind(&stubEntry);
    
    // BTI 保護
    if (m_cpuFeatures.supportsBTI) {
        assembler->bti(BranchTargetType::JC);
    }
    
    // レジスタ保存
    assembler->stp(ARM64Register::X29, ARM64Register::X30, MemOperand::PreIndex(ARM64Register::SP, -16));
    assembler->mov(ARM64Register::X29, ARM64Register::SP);
    
    // 引数レジスタを保存
    assembler->stp(ARM64Register::X0, ARM64Register::X1, MemOperand::PreIndex(ARM64Register::SP, -16));
    assembler->stp(ARM64Register::X2, ARM64Register::X3, MemOperand::PreIndex(ARM64Register::SP, -16));
    assembler->stp(ARM64Register::X4, ARM64Register::X5, MemOperand::PreIndex(ARM64Register::SP, -16));
    
    // オブジェクトの型情報をロード
    assembler->ldr(ARM64Register::X6, MemOperand(ARM64Register::X0, 0)); // Hidden Class
    
    // プロパティ名のハッシュをロード (X1 に格納されていると仮定)
    assembler->mov(ARM64Register::X7, ARM64Register::X1);
    
    // メガモーフィックキャッシュのハッシュテーブルルックアップ
    // ハッシュ関数: (hiddenClass ^ propertyHash) & mask
    assembler->eor(ARM64Register::X8, ARM64Register::X6, ARM64Register::X7);
    assembler->and_(ARM64Register::X8, ARM64Register::X8, MEGAMORPHIC_CACHE_MASK);
    
    // キャッシュテーブルのベースアドレスをロード
    assembler->mov(ARM64Register::X9, reinterpret_cast<uint64_t>(getMegamorphicCacheTable()));
    
    // キャッシュエントリのアドレスを計算
    // エントリサイズは32バイト (型16バイト + メソッド8バイト + プロパティハッシュ8バイト)
    assembler->lsl(ARM64Register::X10, ARM64Register::X8, 5); // * 32
    assembler->add(ARM64Register::X11, ARM64Register::X9, ARM64Register::X10);
    
    // キャッシュエントリの検証
    assembler->ldr(ARM64Register::X12, MemOperand(ARM64Register::X11, 0));  // 型
    assembler->ldr(ARM64Register::X13, MemOperand(ARM64Register::X11, 16)); // プロパティハッシュ
    
    Label cacheHit, cacheMiss;
    
    // 型の比較
    assembler->cmp(ARM64Register::X6, ARM64Register::X12);
    assembler->b_ne(&cacheMiss);
    
    // プロパティハッシュの比較
    assembler->cmp(ARM64Register::X7, ARM64Register::X13);
    assembler->b_ne(&cacheMiss);
    
    // キャッシュヒット
    assembler->bind(&cacheHit);
    
    // メソッドポインタをロード
    assembler->ldr(ARM64Register::X14, MemOperand(ARM64Register::X11, 8));
    
    // 引数レジスタを復元
    assembler->ldp(ARM64Register::X4, ARM64Register::X5, MemOperand::PostIndex(ARM64Register::SP, 16));
    assembler->ldp(ARM64Register::X2, ARM64Register::X3, MemOperand::PostIndex(ARM64Register::SP, 16));
    assembler->ldp(ARM64Register::X0, ARM64Register::X1, MemOperand::PostIndex(ARM64Register::SP, 16));
    
    // フレームを復元
    assembler->ldp(ARM64Register::X29, ARM64Register::X30, MemOperand::PostIndex(ARM64Register::SP, 16));
    
    // メソッドを呼び出し
    assembler->br(ARM64Register::X14);
    
    // キャッシュミス
    assembler->bind(&cacheMiss);
    
    // 引数レジスタを復元（ハンドラ呼び出しのため）
    assembler->ldp(ARM64Register::X4, ARM64Register::X5, MemOperand::PostIndex(ARM64Register::SP, 16));
    assembler->ldp(ARM64Register::X2, ARM64Register::X3, MemOperand::PostIndex(ARM64Register::SP, 16));
    assembler->ldp(ARM64Register::X0, ARM64Register::X1, MemOperand::PostIndex(ARM64Register::SP, 16));
    
    // メガモーフィックキャッシュミスハンドラを呼び出し
    assembler->mov(ARM64Register::X16, reinterpret_cast<uint64_t>(&handleMegamorphicCacheMiss));
    assembler->mov(ARM64Register::X17, siteId); // サイトIDを引数に追加
    assembler->blr(ARM64Register::X16);
    
    // ハンドラから返されたメソッドポインタで呼び出し
    assembler->mov(ARM64Register::X16, ARM64Register::X0);
    
    // フレームを復元
    assembler->ldp(ARM64Register::X29, ARM64Register::X30, MemOperand::PostIndex(ARM64Register::SP, 16));
    
    // メソッドを呼び出し
    assembler->br(ARM64Register::X16);
    
    // コードを生成
    auto code = assembler->finalize();
    
    auto nativeCode = std::make_unique<NativeCode>();
    nativeCode->setCode(std::move(code));
    nativeCode->setEntryPoint(stubEntry.getAddress());
    nativeCode->setType(NativeCodeType::ICStub);
    nativeCode->setICType(ICType::MegamorphicMethodCall);
    nativeCode->setSiteId(siteId);
    
    // デバッグ情報の追加
    if (m_debugMode) {
        nativeCode->addDebugInfo("ARM64 Megamorphic Method Call Stub", siteId);
    }
    
    // パフォーマンス統計の初期化
    nativeCode->initializePerformanceCounters();
    
    return nativeCode;
}

// ヘルパーメソッドの実装
void* ARM64_ICGenerator::handlePolymorphicCacheMiss(void* thisObject, void* propertyName, void* cachePtr) {
    // ポリモーフィックキャッシュミスの処理
    
    // オブジェクトの型情報を取得
    JSObject* obj = static_cast<JSObject*>(thisObject);
    HiddenClass* hiddenClass = obj->getHiddenClass();
    
    // プロパティ名を取得
    JSString* propName = static_cast<JSString*>(propertyName);
    
    // メソッドを解決
    JSFunction* method = obj->lookupMethod(propName);
    if (!method) {
        // メソッドが見つからない場合のフォールバック
        return nullptr;
    }
    
    // キャッシュを更新
    PolymorphicCache* cache = static_cast<PolymorphicCache*>(cachePtr);
    cache->addEntry(hiddenClass, method);
    
    return method->getNativeCode();
}

void* ARM64_ICGenerator::handleMegamorphicCacheMiss(void* thisObject, void* propertyName, uint64_t siteId) {
    // メガモーフィックキャッシュミスの処理
    
    JSObject* obj = static_cast<JSObject*>(thisObject);
    JSString* propName = static_cast<JSString*>(propertyName);
    HiddenClass* hiddenClass = obj->getHiddenClass();
    
    // メソッドを解決
    JSFunction* method = obj->lookupMethod(propName);
    if (!method) {
        return nullptr;
    }
    
    // メガモーフィックキャッシュを更新
    MegamorphicCache* cache = getMegamorphicCacheTable();
    uint32_t hash = computeHash(hiddenClass, propName->getHash()) & MEGAMORPHIC_CACHE_MASK;
    
    cache->entries[hash] = {
        .hiddenClass = hiddenClass,
        .method = method,
        .propertyHash = propName->getHash(),
        .lastAccessed = getCurrentTimestamp()
    };
    
    // 統計情報を更新
    cache->updateStats(siteId, true); // cache miss
    
    return method->getNativeCode();
}

MegamorphicCache* ARM64_ICGenerator::getMegamorphicCacheTable() {
    // スレッドローカルなメガモーフィックキャッシュテーブルを取得
    static thread_local MegamorphicCache megamorphicCache;
    return &megamorphicCache;
}

uint32_t ARM64_ICGenerator::computeHash(HiddenClass* hiddenClass, uint32_t propertyHash) {
    // 型とプロパティハッシュからキャッシュキーを計算
    uint64_t classPtr = reinterpret_cast<uint64_t>(hiddenClass);
    return static_cast<uint32_t>((classPtr >> 3) ^ propertyHash);
}

uint64_t ARM64_ICGenerator::getCurrentTimestamp() {
    return std::chrono::steady_clock::now().time_since_epoch().count();
}

} // namespace core
} // namespace aerojs 