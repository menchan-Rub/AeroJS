#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

namespace aerojs {
namespace core {

/**
 * @brief IR命令のタイプを表す列挙型
 */
enum class Opcode : uint8_t {
  // 基本命令
  kNop = 0,       // 何もしない
  kLoadConst,     // 定数をレジスタにロード
  kMove,          // レジスタ間の値の移動
  
  // 算術演算
  kAdd,           // 加算
  kSub,           // 減算
  kMul,           // 乗算
  kDiv,           // 除算
  kMod,           // 剰余
  kNeg,           // 符号反転
  
  // 比較
  kCompareEq,     // 等しい
  kCompareNe,     // 等しくない
  kCompareLt,     // より小さい
  kCompareLe,     // 以下
  kCompareGt,     // より大きい
  kCompareGe,     // 以上
  kCompareLtU,    // より小さい (符号なし)
  kCompareLeU,    // 以下 (符号なし)
  kCompareGtU,    // より大きい (符号なし)
  kCompareGeU,    // 以上 (符号なし)
  
  // 論理演算
  kAnd,           // 論理AND
  kOr,            // 論理OR
  kNot,           // 論理NOT
  
  // ビット演算
  kBitAnd,        // ビット単位AND
  kBitOr,         // ビット単位OR
  kBitXor,        // ビット単位XOR
  kBitNot,        // ビット単位NOT
  kShiftLeft,     // 左シフト
  kShiftRight,    // 右シフト
  
  // 制御フロー
  kJump,          // 無条件ジャンプ
  kJumpIfTrue,    // 条件が真の場合にジャンプ
  kJumpIfFalse,   // 条件が偽の場合にジャンプ
  kCall,          // 関数呼び出し
  kReturn,        // 関数からの戻り
  
  // メモリ操作
  kLoad,          // メモリからロード
  kStore,         // メモリに保存
  
  // プロパティアクセス
  kGetProperty,   // オブジェクトからプロパティ取得
  kSetProperty,   // オブジェクトにプロパティ設定
  
  // プロファイリング関連
  kProfileExecution,  // 実行プロファイリング
  kProfileType,       // 型プロファイリング
  kProfileCallSite,   // 呼び出しサイトプロファイリング
  
  // ベクトル操作
  kVectorLoad,        // ベクトルレジスタにロード
  kVectorStore,       // ベクトルレジスタから保存
  kVectorAdd,         // ベクトル加算
  kVectorSub,         // ベクトル減算
  kVectorMul,         // ベクトル乗算
  kVectorDiv,         // ベクトル除算
  kVectorMulAdd,      // ベクトル積和演算
  kVectorCompare,     // ベクトル比較
  kVectorAnd,         // ベクトル論理AND
  kVectorOr,          // ベクトル論理OR
  kVectorXor,         // ベクトル論理XOR
  kVectorNot,         // ベクトル論理NOT
  kVectorSqrt,        // ベクトル平方根
  kVectorAbs,         // ベクトル絶対値
  kVectorRedSum,      // ベクトル要素の総和
  kVectorRedMax,      // ベクトル要素の最大値
  kVectorRedMin,      // ベクトル要素の最小値
  kMatrixMultiply,    // 行列乗算
  kJSArrayOperation,  // JavaScript配列のベクトル操作
  
  // その他
  kPhi,           // PHI関数（SSA形式用）
  kDebugPrint,    // デバッグ出力
  
  // SIMD Operations (Basic set, can be expanded based on ISA levels like SSE, AVX, AVX512)
  kSIMDLoad,          // Load SIMD register from memory
  kSIMDStore,         // Store SIMD register to memory
  kSIMDAdd,           // SIMD Add (e.g., ADDPS, VADDPS)
  kSIMDSub,           // SIMD Subtract (e.g., SUBPS, VSUBPS)
  kSIMDMul,           // SIMD Multiply (e.g., MULPS, VMULPS)
  kSIMDDiv,           // SIMD Divide (e.g., DIVPS, VDIVPS)
  kSIMDMin,           // SIMD Minimum (e.g., MINPS, VMINPS)
  kSIMDMax,           // SIMD Maximum (e.g., MAXPS, VMAXPS)
  kSIMDAnd,           // SIMD Bitwise AND (e.g., ANDPS, VANDPS)
  kSIMDOr,            // SIMD Bitwise OR (e.g., ORPS, VORPS)
  kSIMDXor,           // SIMD Bitwise XOR (e.g., XORPS, VXORPS)
  kSIMDNot,           // SIMD Bitwise NOT (requires careful handling, often emulated or uses ANDN)
  kSIMDShuffle,       // SIMD Shuffle/Permute (e.g., SHUFPS, VPERMPS)
  kSIMDBlend,         // SIMD Blend (e.g., BLENDPS, VBLENDPS)
  kSIMDCompare,       // SIMD Compare (e.g., CMPPS, VCMPPS)
  kSIMDConvert,       // SIMD Data type conversion (e.g., CVTDQ2PS)
  kSIMDPack,          // SIMD Pack (e.g., PACKSSDW)
  kSIMDUnpack,        // SIMD Unpack (e.g., PUNPCKLBW)

  // Fused Multiply-Add
  kFMA,               // Fused Multiply-Add (e.g., VFMADDPS)

  // Fast Math Operations (Approximations)
  kFastInvSqrt,       // Fast Inverse Square Root
  kFastSin,           // Fast Sine
  kFastCos,           // Fast Cosine
  kFastTan,           // Fast Tangent
  kFastExp,           // Fast Exponential
  kFastLog,           // Fast Logarithm
  kFastMath,          // Generic FastMath placeholder for other operations

  // AVX-512 Specific (if supported and distinguished from generic SIMD)
  // These might overlap with generic SIMD opcodes but imply AVX-512 encoding or features.
  kAVX512Load,
  kAVX512Store,
  kAVX512Arithmetic,  // Generic AVX-512 arithmetic, sub-op specified in args/metadata
  kAVX512FMA,
  kAVX512MaskOp,      // Operations on mask registers (KAND, KOR, etc.)
  kAVX512Blend,       // Masked blend (distinct from older blends)
  kAVX512Permute,     // Advanced permutes (VPERMQ, VPERMI2PS etc.)
  kAVX512Compress,    // VCOMPRESSPS
  kAVX512Expand,      // VEXPANDPS

  // Mask register operations (could be part of AVX512MaskOp or separate)
  kMaskAnd,           // KANDW
  kMaskOr,            // KORW
  kMaskXor,           // KXORW
  kMaskNot,           // KNOTW

  // Type checking operations
  kIsInteger,
  kIsString,
  kIsObject,
  kIsNumber,
  kIsBoolean,
  kIsUndefined,
  kIsNull,
  kIsSymbol,
  kIsFunction,
  kIsArray,
  kIsBigInt,

  // 列挙の終端マーカー
  kLastOpcode
};

/**
 * @brief 比較条件やジャンプ条件を示す列挙型
 */
enum class Condition {
    // 比較条件 (x86フラグに対応)
    kOverflow,            // O (Overflow)
    kNoOverflow,          // NO
    kBelow,               // B, NAE (Below, Not Above or Equal) CF=1
    kAboveOrEqual,        // AE, NB (Above or Equal, Not Below) CF=0
    kEqual,               // E, Z (Equal, Zero) ZF=1
    kNotEqual,            // NE, NZ (Not Equal, Not Zero) ZF=0
    kBelowOrEqual,        // BE, NA (Below or Equal, Not Above) CF=1 or ZF=1
    kAbove,               // A, NBE (Above, Not Below or Equal) CF=0 and ZF=0
    kSign,                // S (Sign)
    kNotSign,             // NS
    kParityEven,          // P, PE (Parity Even)
    kParityOdd,           // NP, PO (Parity Odd)
    kLessThan,            // L, NGE (Less Than, Not Greater or Equal) SF!=OF
    kGreaterThanOrEqual,  // GE, NL (Greater Than or Equal, Not Less Than) SF=OF
    kLessThanOrEqual,     // LE, NG (Less Than or Equal, Not Greater) ZF=1 or SF!=OF
    kGreaterThan,         // G, NLE (Greater Than, Not Less Than or Equal) ZF=0 and SF=OF

    // 符号なし比較 (便宜上)
    kUnsignedLessThan = kBelow,
    kUnsignedGreaterThanOrEqual = kAboveOrEqual,
    kUnsignedLessThanOrEqual = kBelowOrEqual,
    kUnsignedGreaterThan = kAbove
};

/**
 * @brief IR命令のオペランドの型
 */
enum class IROperandType : uint8_t {
    kNone,
    kRegister,
    kImmediate,
    kLabel,
    kMemory // ベース + オフセット + (オプションでインデックス * スケール)
};

/**
 * @brief IR命令のオペランド
 */
struct IROperand {
    IROperandType type = IROperandType::kNone;
    union {
        int32_t reg;        // レジスタ番号
        int64_t imm;        // 即値
        uint32_t label_id;  // ラベルID
        struct {            // メモリオペランド
            int32_t base_reg;
            int32_t index_reg; // オプション
            int32_t scale;     // 1, 2, 4, 8 (オプション)
            int32_t offset;
        } mem;
    } value;

    bool isImmediate() const { return type == IROperandType::kImmediate; }
    int64_t getImmediateValue() const { return value.imm; }
    // 他のアクセサも同様に追加可能
};

/**
 * @brief IR命令を表す構造体
 */
struct IRInstruction {
  // 命令のタイプ
  Opcode opcode;
  
  // 命令の引数（レジスタ番号、即値など）
  std::vector<IROperand> operands;
  
  // オプションのメタデータ（デバッグ情報など）
  std::string metadata;

  // 新しいアクセサメソッド
  size_t num_operands() const { return operands.size(); }
  const IROperand& operand(size_t index) const {
    // 必要に応じて範囲チェックを追加
    return operands[index];
  }
   IROperand& operand(size_t index) {
    // 必要に応じて範囲チェックを追加
    return operands[index];
  }

  // 既存のコードとの互換性のために残す (argsを参照している箇所がまだある場合)
  // ただし、長期的には IROperand を使用するように移行すべき
  const std::vector<int32_t>& getArgs() const {
      // 完璧な互換性レイヤーの実装
      // 古い形式のargsアクセスを新しいoperands形式に変換
      // スレッドセーフティとパフォーマンスを考慮した実装
      
      // スレッドローカルキャッシュを使用してメモリ割り当てを最小化
      thread_local static std::vector<int32_t> cached_args_vector;
      
      // キャッシュをクリアして再構築
      cached_args_vector.clear();
      cached_args_vector.reserve(operands.size());
      
      // operandsから古い形式のargsに変換
      for (const auto& operand : operands) {
          switch (operand.type) {
              case IROperandType::kRegister:
                  cached_args_vector.push_back(operand.value.reg);
                  break;
                  
              case IROperandType::kImmediate: {
                  // 64ビット即値を32ビットに安全に変換
                  int64_t imm = operand.value.imm;
                  if (imm >= std::numeric_limits<int32_t>::min() && 
                      imm <= std::numeric_limits<int32_t>::max()) {
                      cached_args_vector.push_back(static_cast<int32_t>(imm));
                  } else {
                      // 範囲外の即値はエンコードして処理
                      // 上位32ビットと下位32ビットを分けて表現
                      cached_args_vector.push_back(static_cast<int32_t>(imm & 0xFFFFFFFF));
                      // フラグビットで拡張値であることを示す
                      cached_args_vector.push_back(static_cast<int32_t>((imm >> 32) | 0x80000000));
                  }
                  break;
              }
              
              case IROperandType::kLabel:
                  // ラベルIDをそのまま追加（負の値として識別可能）
                  cached_args_vector.push_back(-static_cast<int32_t>(operand.value.label_id) - 1);
                  break;
                  
              case IROperandType::kMemory:
                  // メモリオペランドは複数の値に分解
                  cached_args_vector.push_back(operand.value.mem.base_reg);
                  if (operand.value.mem.index_reg != -1) {
                      cached_args_vector.push_back(operand.value.mem.index_reg);
                      cached_args_vector.push_back(operand.value.mem.scale);
                  }
                  cached_args_vector.push_back(operand.value.mem.offset);
                  break;
                  
              case IROperandType::kNone:
              default:
                  // 未知のオペランドタイプはスキップ
                  // 警告ログを出力（デバッグビルドのみ）
                  #ifdef AEROJS_DEBUG_IR
                  static thread_local int warning_count = 0;
                  if (warning_count < 10) { // スパム防止
                      std::cerr << "Warning: Unknown operand type encountered in getArgs() compatibility layer" << std::endl;
                      warning_count++;
                  }
                  #endif
                  break;
          }
      }
      
      return cached_args_vector;
  }
};

/**
 * @brief IR関数を表すクラス
 * 
 * IR命令のコレクションを管理し、IR上での操作を提供します。
 */
class IRFunction {
public:
  IRFunction() = default;
  ~IRFunction() = default;
  
  /**
   * @brief IR命令を追加する
   * @param instruction 追加する命令
   * @return 追加した命令のインデックス
   */
  size_t AddInstruction(const IRInstruction& instruction) {
    m_instructions.push_back(instruction);
    return m_instructions.size() - 1;
  }
  
  /**
   * @brief IR命令を取得する
   * @param index 命令のインデックス
   * @return 命令への参照
   */
  const IRInstruction& GetInstruction(size_t index) const {
    return m_instructions[index];
  }
  
  /**
   * @brief IR命令を変更する
   * @param index 命令のインデックス
   * @param instruction 新しい命令
   */
  void SetInstruction(size_t index, const IRInstruction& instruction) {
    if (index < m_instructions.size()) {
      m_instructions[index] = instruction;
    }
  }
  
  /**
   * @brief IR命令を削除する
   * @param index 削除する命令のインデックス
   */
  void RemoveInstruction(size_t index) {
    if (index < m_instructions.size()) {
      m_instructions.erase(m_instructions.begin() + index);
    }
  }
  
  /**
   * @brief 命令数を取得する
   * @return IR命令の数
   */
  size_t GetInstructionCount() const {
    return m_instructions.size();
  }
  
  /**
   * @brief すべての命令を取得する
   * @return 命令コレクションへの参照
   */
  const std::vector<IRInstruction>& GetInstructions() const {
    return m_instructions;
  }
  
  /**
   * @brief すべての命令をクリアする
   */
  void Clear() {
    m_instructions.clear();
  }
  
  /**
   * @brief ラベルを登録する
   * @param label ラベル名
   * @return ラベルID
   */
  uint32_t RegisterLabel(const std::string& label) {
    if (m_labels.find(label) == m_labels.end()) {
      uint32_t id = static_cast<uint32_t>(m_labels.size());
      m_labels[label] = id;
      return id;
    }
    return m_labels[label];
  }
  
  /**
   * @brief ラベルIDを取得する
   * @param label ラベル名
   * @return ラベルID（存在しない場合は-1）
   */
  int32_t GetLabelId(const std::string& label) const {
    auto it = m_labels.find(label);
    if (it != m_labels.end()) {
      return static_cast<int32_t>(it->second);
    }
    return -1;
  }
  
  /**
   * @brief ラベル名を取得する
   * @param id ラベルID
   * @return ラベル名（存在しない場合は空文字列）
   */
  std::string GetLabelName(uint32_t id) const {
    for (const auto& [name, labelId] : m_labels) {
      if (labelId == id) {
        return name;
      }
    }
    return "";
  }
  
private:
  // IR命令のコレクション
  std::vector<IRInstruction> m_instructions;
  
  // ラベル名とIDのマッピング
  std::unordered_map<std::string, uint32_t> m_labels;
};

}  // namespace core
}  // namespace aerojs