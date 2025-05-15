/**
 * @file arm64_assembler.h
 * @brief AeroJS JavaScriptエンジン用の世界最高性能ARM64アセンブラ
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <array>
#include <bitset>
#include <cassert>

namespace aerojs {
namespace core {
namespace arm64 {

// ARM64レジスタ定義
enum class Register : uint8_t {
    // 汎用レジスタ (64ビット)
    X0 = 0, X1, X2, X3, X4, X5, X6, X7,
    X8, X9, X10, X11, X12, X13, X14, X15,
    X16, X17, X18, X19, X20, X21, X22, X23,
    X24, X25, X26, X27, X28, X29, X30, XZR,
    
    // スタックポインタとフレームポインタ
    SP = 31,
    FP = 29,  // X29
    LR = 30,  // X30
    
    // 汎用レジスタ (32ビット)
    W0 = 32, W1, W2, W3, W4, W5, W6, W7,
    W8, W9, W10, W11, W12, W13, W14, W15,
    W16, W17, W18, W19, W20, W21, W22, W23,
    W24, W25, W26, W27, W28, W29, W30, WZR,
    
    // SIMD/FPレジスタ (128ビット)
    V0 = 64, V1, V2, V3, V4, V5, V6, V7,
    V8, V9, V10, V11, V12, V13, V14, V15,
    V16, V17, V18, V19, V20, V21, V22, V23,
    V24, V25, V26, V27, V28, V29, V30, V31,
    
    // 規約上の名前
    PLATFORM_REGISTER = X18,   // プラットフォーム予約レジスタ
    SCRATCH_REG0 = X16,        // スクラッチレジスタ
    SCRATCH_REG1 = X17,        // スクラッチレジスタ
    
    // レジスタカウント
    REGISTER_COUNT = 96
};

// 浮動小数点レジスタの特殊形式
enum class FloatRegister : uint8_t {
    // 単精度 (32ビット)
    S0 = 0, S1, S2, S3, S4, S5, S6, S7,
    S8, S9, S10, S11, S12, S13, S14, S15,
    S16, S17, S18, S19, S20, S21, S22, S23,
    S24, S25, S26, S27, S28, S29, S30, S31,
    
    // 倍精度 (64ビット)
    D0 = 32, D1, D2, D3, D4, D5, D6, D7,
    D8, D9, D10, D11, D12, D13, D14, D15,
    D16, D17, D18, D19, D20, D21, D22, D23,
    D24, D25, D26, D27, D28, D29, D30, D31,
    
    // 四倍精度 (128ビット)
    Q0 = 64, Q1, Q2, Q3, Q4, Q5, Q6, Q7,
    Q8, Q9, Q10, Q11, Q12, Q13, Q14, Q15,
    Q16, Q17, Q18, Q19, Q20, Q21, Q22, Q23,
    Q24, Q25, Q26, Q27, Q28, Q29, Q30, Q31
};

// 条件コード
enum class Condition : uint8_t {
    EQ = 0,   // Equal
    NE = 1,   // Not Equal
    CS = 2,   // Carry Set / Higher or Same
    HS = 2,   // Same as CS
    CC = 3,   // Carry Clear / Lower
    LO = 3,   // Same as CC
    MI = 4,   // Minus / Negative
    PL = 5,   // Plus / Positive or Zero
    VS = 6,   // Overflow
    VC = 7,   // No Overflow
    HI = 8,   // Higher
    LS = 9,   // Lower or Same
    GE = 10,  // Greater than or Equal
    LT = 11,  // Less Than
    GT = 12,  // Greater Than
    LE = 13,  // Less than or Equal
    AL = 14,  // Always
    NV = 15   // Never
};

// シフト種別
enum class Shift : uint8_t {
    LSL = 0,  // 論理左シフト
    LSR = 1,  // 論理右シフト
    ASR = 2,  // 算術右シフト
    ROR = 3   // 右ローテート
};

// メモリ拡張
enum class Extend : uint8_t {
    UXTB = 0,  // 符号なし拡張バイト
    UXTH = 1,  // 符号なし拡張ハーフワード
    UXTW = 2,  // 符号なし拡張ワード
    UXTX = 3,  // 符号なし拡張ダブルワード
    SXTB = 4,  // 符号付き拡張バイト
    SXTH = 5,  // 符号付き拡張ハーフワード
    SXTW = 6,  // 符号付き拡張ワード
    SXTX = 7   // 符号付き拡張ダブルワード
};

// ラベル情報
class Label {
public:
    Label() : _bound(false), _position(0) {}
    
    bool isBound() const { return _bound; }
    int position() const { return _position; }
    
    void bind(int position) {
        _bound = true;
        _position = position;
        
        // バインド保留中のすべての参照を解決
        for (auto& ref : _references) {
            ref.resolve(position);
        }
        _references.clear();
    }
    
    // 参照情報の構造体
    struct Reference {
        int position;  // 命令位置
        std::function<void(int)> resolve;  // 解決コールバック
        
        Reference(int pos, std::function<void(int)> res)
            : position(pos), resolve(res) {}
    };
    
    void addReference(int position, std::function<void(int)> resolve) {
        if (_bound) {
            resolve(_position);
        } else {
            _references.emplace_back(position, resolve);
        }
    }
    
private:
    bool _bound;
    int _position;
    std::vector<Reference> _references;
};

// レジスタリスト
class RegisterList {
public:
    RegisterList() : _registers(0) {}
    
    explicit RegisterList(Register reg) : _registers(0) {
        add(reg);
    }
    
    RegisterList(std::initializer_list<Register> regs) : _registers(0) {
        for (auto reg : regs) {
            add(reg);
        }
    }
    
    void add(Register reg) {
        uint8_t regNum = static_cast<uint8_t>(reg);
        if (regNum < 32) {
            _registers |= (1ULL << regNum);
        }
    }
    
    void remove(Register reg) {
        uint8_t regNum = static_cast<uint8_t>(reg);
        if (regNum < 32) {
            _registers &= ~(1ULL << regNum);
        }
    }
    
    bool contains(Register reg) const {
        uint8_t regNum = static_cast<uint8_t>(reg);
        if (regNum < 32) {
            return (_registers & (1ULL << regNum)) != 0;
        }
        return false;
    }
    
    size_t count() const {
        return std::bitset<64>(_registers).count();
    }
    
    uint32_t bits() const {
        return static_cast<uint32_t>(_registers);
    }
    
private:
    uint64_t _registers;
};

// メモリアドレス指定
class MemOperand {
public:
    // ベースレジスタのみ
    explicit MemOperand(Register base, int32_t offset = 0, bool preIndex = false)
        : _base(base), _index(Register::XZR), _offset(offset),
          _shift(0), _extend(Extend::UXTX), _shiftExtend(false),
          _preIndex(preIndex), _postIndex(false) {}
    
    // ベース + インデックスレジスタ
    MemOperand(Register base, Register index, Shift shift = Shift::LSL, 
               uint8_t shiftAmount = 0)
        : _base(base), _index(index), _offset(0),
          _shift(static_cast<uint8_t>(shift)), _shiftAmount(shiftAmount),
          _extend(Extend::UXTX), _shiftExtend(false),
          _preIndex(false), _postIndex(false) {}
    
    // ベース + 拡張インデックス
    MemOperand(Register base, Register index, Extend extend, 
               uint8_t shiftAmount = 0)
        : _base(base), _index(index), _offset(0),
          _shift(0), _shiftAmount(shiftAmount),
          _extend(extend), _shiftExtend(true),
          _preIndex(false), _postIndex(false) {}
    
    // ポストインデックス
    static MemOperand PostIndex(Register base, int32_t offset) {
        MemOperand operand(base);
        operand._offset = offset;
        operand._postIndex = true;
        return operand;
    }
    
    // プレインデックス
    static MemOperand PreIndex(Register base, int32_t offset) {
        return MemOperand(base, offset, true);
    }
    
    // アクセサ
    Register getBase() const { return _base; }
    Register getIndex() const { return _index; }
    int32_t getOffset() const { return _offset; }
    uint8_t getShift() const { return _shift; }
    uint8_t getShiftAmount() const { return _shiftAmount; }
    Extend getExtend() const { return _extend; }
    bool isShiftExtend() const { return _shiftExtend; }
    bool isPreIndex() const { return _preIndex; }
    bool isPostIndex() const { return _postIndex; }
    bool isRegisterOffset() const { return _index != Register::XZR; }
    
private:
    Register _base;
    Register _index;
    int32_t _offset;
    uint8_t _shift;
    uint8_t _shiftAmount;
    Extend _extend;
    bool _shiftExtend;
    bool _preIndex;
    bool _postIndex;
};

// オペランド用シフト情報
class Operand {
public:
    explicit Operand(Register reg)
        : _reg(reg), _shift(Shift::LSL), _amount(0), _extended(false) {}
    
    Operand(Register reg, Shift shift, uint8_t amount)
        : _reg(reg), _shift(shift), _amount(amount), _extended(false) {}
    
    Operand(Register reg, Extend extend, uint8_t amount)
        : _reg(reg), _extend(extend), _amount(amount), _extended(true) {}
    
    Register getReg() const { return _reg; }
    Shift getShift() const { return _shift; }
    Extend getExtend() const { return _extend; }
    uint8_t getAmount() const { return _amount; }
    bool isExtended() const { return _extended; }
    
private:
    Register _reg;
    union {
        Shift _shift;
        Extend _extend;
    };
    uint8_t _amount;
    bool _extended;
};

// ARM64 アセンブラクラス
class ARM64Assembler {
public:
    ARM64Assembler();
    ~ARM64Assembler();
    
    // バッファ管理
    void reset();
    const uint8_t* buffer() const { return _buffer.data(); }
    size_t bufferSize() const { return _buffer.size(); }
    void* codeAddress() const { return const_cast<void*>(reinterpret_cast<const void*>(_buffer.data())); }
    
    // ラベル操作
    void bind(Label* label);
    
    // アライメント
    void align(int alignment);
    void nop(int count = 1);
    
    // データ埋め込み
    void emitInt8(int8_t value);
    void emitInt16(int16_t value);
    void emitInt32(int32_t value);
    void emitInt64(int64_t value);
    void emitFloat(float value);
    void emitDouble(double value);
    void emitString(const char* str, size_t len);
    
    // 移動命令
    void mov(Register rd, Register rm);
    void mov(Register rd, uint64_t imm);
    void movk(Register rd, uint16_t imm, int shift = 0);
    void movn(Register rd, uint16_t imm, int shift = 0);
    void movz(Register rd, uint16_t imm, int shift = 0);
    
    // 算術命令
    void add(Register rd, Register rn, const Operand& operand);
    void add(Register rd, Register rn, uint64_t imm);
    void sub(Register rd, Register rn, const Operand& operand);
    void sub(Register rd, Register rn, uint64_t imm);
    void mul(Register rd, Register rn, Register rm);
    void sdiv(Register rd, Register rn, Register rm);
    void udiv(Register rd, Register rn, Register rm);
    void msub(Register rd, Register rn, Register rm, Register ra);
    void madd(Register rd, Register rn, Register rm, Register ra);
    
    // 論理命令
    void andInst(Register rd, Register rn, const Operand& operand);
    void orr(Register rd, Register rn, const Operand& operand);
    void eor(Register rd, Register rn, const Operand& operand);
    void ands(Register rd, Register rn, const Operand& operand);
    void bic(Register rd, Register rn, const Operand& operand);
    
    // シフト命令
    void lsl(Register rd, Register rn, uint8_t shift);
    void lsr(Register rd, Register rn, uint8_t shift);
    void asr(Register rd, Register rn, uint8_t shift);
    void ror(Register rd, Register rn, uint8_t shift);
    
    // ビット操作
    void clz(Register rd, Register rn);
    void rbit(Register rd, Register rn);
    void rev(Register rd, Register rn);
    
    // 拡張命令
    void sxtb(Register rd, Register rn);
    void sxth(Register rd, Register rn);
    void sxtw(Register rd, Register rn);
    void uxtb(Register rd, Register rn);
    void uxth(Register rd, Register rn);
    void uxtw(Register rd, Register rn);
    
    // 比較命令
    void cmp(Register rn, const Operand& operand);
    void cmp(Register rn, uint64_t imm);
    void cmn(Register rn, const Operand& operand);
    void tst(Register rn, const Operand& operand);
    
    // 条件命令
    void csel(Register rd, Register rn, Register rm, Condition cond);
    void csinc(Register rd, Register rn, Register rm, Condition cond);
    void csinv(Register rd, Register rn, Register rm, Condition cond);
    void csneg(Register rd, Register rn, Register rm, Condition cond);
    
    // 分岐命令
    void b(Label* label);
    void b(int offset);
    void bl(Label* label);
    void bl(int offset);
    void br(Register rn);
    void blr(Register rn);
    void ret(Register rn = Register::X30);
    
    // 条件分岐命令
    void bCond(Condition cond, Label* label);
    void bCond(Condition cond, int offset);
    void cbz(Register rt, Label* label);
    void cbnz(Register rt, Label* label);
    void tbz(Register rt, unsigned bit, Label* label);
    void tbnz(Register rt, unsigned bit, Label* label);
    
    // メモリアクセス命令
    void ldr(Register rt, const MemOperand& operand);
    void ldrb(Register rt, const MemOperand& operand);
    void ldrh(Register rt, const MemOperand& operand);
    void ldrsb(Register rt, const MemOperand& operand);
    void ldrsh(Register rt, const MemOperand& operand);
    void ldrsw(Register rt, const MemOperand& operand);
    void str(Register rt, const MemOperand& operand);
    void strb(Register rt, const MemOperand& operand);
    void strh(Register rt, const MemOperand& operand);
    
    // SIMD/FP レジスタ用メモリアクセス
    void ldr(FloatRegister vt, const MemOperand& operand);
    void str(FloatRegister vt, const MemOperand& operand);
    
    // ペア操作
    void ldp(Register rt, Register rt2, const MemOperand& operand);
    void stp(Register rt, Register rt2, const MemOperand& operand);
    
    // メモリバリア命令
    void dmb(int option);
    void dsb(int option);
    void isb();
    
    // システム命令
    void mrs(Register rt, uint32_t systemReg);
    void msr(uint32_t systemReg, Register rt);
    
    // 浮動小数点算術命令
    void fadd(FloatRegister vd, FloatRegister vn, FloatRegister vm);
    void fsub(FloatRegister vd, FloatRegister vn, FloatRegister vm);
    void fmul(FloatRegister vd, FloatRegister vn, FloatRegister vm);
    void fdiv(FloatRegister vd, FloatRegister vn, FloatRegister vm);
    void fabs(FloatRegister vd, FloatRegister vn);
    void fneg(FloatRegister vd, FloatRegister vn);
    void fsqrt(FloatRegister vd, FloatRegister vn);
    
    // 浮動小数点比較
    void fcmp(FloatRegister vn, FloatRegister vm);
    void fcmp(FloatRegister vn, double imm);
    
    // 浮動小数点変換
    void fcvtzs(Register rd, FloatRegister vn);
    void fcvtzu(Register rd, FloatRegister vn);
    void scvtf(FloatRegister vd, Register rn);
    void ucvtf(FloatRegister vd, Register rn);
    
    // SIMD命令
    void ld1(FloatRegister vt, const MemOperand& operand);
    void st1(FloatRegister vt, const MemOperand& operand);
    void fmla(FloatRegister vd, FloatRegister vn, FloatRegister vm);
    void fmls(FloatRegister vd, FloatRegister vn, FloatRegister vm);
    
    // 特殊命令
    void adrp(Register rd, Label* label);
    void adr(Register rd, Label* label);
    void brkpt(uint16_t imm = 0);

private:
    std::vector<uint8_t> _buffer;
    
    // エンコーディングヘルパー
    void emit(uint32_t instruction);
    
    // レジスタエンコード
    static uint32_t encodeReg(Register reg);
    static uint32_t encodeSIMDReg(FloatRegister reg);
    static uint32_t encodeRegShift(Register reg, Shift shift, uint8_t amount);
    static uint32_t encodeRegExtend(Register reg, Extend extend, uint8_t amount);
    
    // 即値エンコード
    static uint32_t encodeBitMask(uint8_t size, uint64_t value);
    static uint32_t encodeLogical(uint64_t value, uint8_t size, uint8_t& n, uint8_t& imms, uint8_t& immr);
    
    // 命令形式エンコード
    uint32_t encodeDP1(uint8_t op, Register rd, Register rn);
    uint32_t encodeDP2(uint8_t op, Register rd, Register rn, Register rm);
    uint32_t encodeDP3(uint8_t op, Register rd, Register rn, Register rm, Register ra);
    uint32_t encodeMov(Register rd, uint16_t imm, uint8_t shift, uint8_t opcode);
    uint32_t encodeBranch(uint8_t op, int32_t offset);
    uint32_t encodeBranchCond(Condition cond, int32_t offset);
    uint32_t encodeBranchReg(uint8_t op, Register rn);
    uint32_t encodeLoadStore(uint8_t size, uint8_t op, Register rt, const MemOperand& operand);
    uint32_t encodeLoadStorePair(uint8_t op, Register rt, Register rt2, const MemOperand& operand);
    
    // パッチ管理
    void recordLabelLink(Label* label, int offset, int maxRange);
    void recordFarLabelLink(Label* label, int offset);
    void resolveLabel(Label* label, int destination);
};

} // namespace arm64
} // namespace core
} // namespace aerojs 