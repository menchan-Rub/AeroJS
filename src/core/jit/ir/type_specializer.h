#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <optional>
#include <string>

#include "ir.h"
#include "../profiler/execution_profiler.h"
#include "../jit_profiler.h"

namespace aerojs {
namespace core {

/**
 * @brief JavaScript値の型を表す列挙型
 */
enum class JSValueType : uint8_t {
  Unknown = 0,
  Integer,          ///< 32ビット整数
  Double,           ///< 64ビット浮動小数点数
  Boolean,          ///< 真偽値
  String,           ///< 文字列
  Object,           ///< オブジェクト（プロパティ集合）
  Array,            ///< 配列
  Function,         ///< 関数
  Symbol,           ///< シンボル
  BigInt,           ///< 任意精度整数
  Undefined,        ///< undefined値
  Null,             ///< null値
  SmallInt,         ///< SMI最適化用小整数（-2^31〜2^31-1）
  HeapNumber,       ///< ヒープ割り当て数値
  NaN,              ///< 非数
  StringObject,     ///< String オブジェクト
  NumberObject,     ///< Number オブジェクト
  BooleanObject,    ///< Boolean オブジェクト
  Date,             ///< Date オブジェクト
  RegExp,           ///< 正規表現オブジェクト
};

/**
 * @brief 命令が操作する値の型情報
 */
struct TypeInfo {
  JSValueType type = JSValueType::Unknown;  ///< 値の型
  bool nullable = false;                    ///< nullになり得るか
  bool maybeUndefined = false;              ///< undefinedになり得るか
  
  // 数値型の範囲（type がInteger/Double/SmallInt/HeapNumber の場合のみ有効）
  struct {
    bool hasLowerBound = false;
    bool hasUpperBound = false;
    double lowerBound = 0.0;
    double upperBound = 0.0;
  } range;
  
  // オブジェクト形状情報（type が Object の場合のみ有効）
  struct {
    uint32_t shapeId = 0;         ///< オブジェクト形状ID
    bool isMonomorphic = false;   ///< 単一形状か
    bool isPoly2 = false;         ///< 2種類の形状か
    bool isPoly3 = false;         ///< 3種類の形状か
    bool isPoly4 = false;         ///< 4種類の形状か
    bool isPolymorphic = false;   ///< 複数形状か
    bool isMegamorphic = false;   ///< 多数の形状か（インラインキャッシュ非効率）
  } shape;
  
  // 配列情報（type が Array の場合のみ有効）
  struct {
    bool isHomogeneous = false;   ///< 同種要素の配列か
    JSValueType elemType = JSValueType::Unknown;  ///< 要素の型
    bool isPacked = false;        ///< 稠密配列か
    bool hasHoles = false;        ///< スパース配列か
    bool isContinuous = false;    ///< 連続した配列か
  } array;
  
  /**
   * @brief 型情報をマージする
   * @param other マージする型情報
   * @return マージ後の型情報
   */
  TypeInfo Merge(const TypeInfo& other) const noexcept;
  
  /**
   * @brief 等価比較演算子
   */
  bool operator==(const TypeInfo& other) const noexcept;
  
  /**
   * @brief 非等価比較演算子
   */
  bool operator!=(const TypeInfo& other) const noexcept {
    return !(*this == other);
  }
};

/**
 * @brief 型に特化した最適化を行うクラス
 * 
 * プロファイリングデータを使用して、型に特化した最適化を行います。
 * 型ガードを挿入し、特定の型に対して最適化されたコードパスを生成します。
 */
class TypeSpecializer {
public:
    TypeSpecializer();
    ~TypeSpecializer();
    
    /**
     * @brief IRに型ガードを追加する
     * @param function 対象のIR関数
     * @param bytecodeOffset バイトコードオフセット
     * @param expectedType 期待される型
     * @return ガードが追加されたらtrue
     */
    bool AddTypeGuard(IRFunction* function, uint32_t bytecodeOffset, TypeFeedbackRecord::TypeCategory expectedType);
    
    /**
     * @brief IR命令を型特化する
     * @param function 対象のIR関数
     * @param inst 特化する命令
     * @param typeMap 型マッピング
     * @return 特化されたIR命令のインデックス
     */
    size_t SpecializeInstruction(IRFunction* function, const IRInstruction& inst, 
                               const std::unordered_map<int32_t, TypeFeedbackRecord::TypeCategory>& typeMap);
    
    /**
     * @brief 加算命令を型に特化する
     * @param function 対象のIR関数
     * @param dest 格納先レジスタ
     * @param src1 ソースレジスタ1
     * @param src2 ソースレジスタ2
     * @param type1 ソース1の型
     * @param type2 ソース2の型
     * @return 特化された命令のインデックス
     */
    size_t SpecializeAdd(IRFunction* function, int32_t dest, int32_t src1, int32_t src2,
                       TypeFeedbackRecord::TypeCategory type1, TypeFeedbackRecord::TypeCategory type2);
    
    /**
     * @brief 減算命令を型に特化する
     * @param function 対象のIR関数
     * @param dest 格納先レジスタ
     * @param src1 ソースレジスタ1
     * @param src2 ソースレジスタ2
     * @param type1 ソース1の型
     * @param type2 ソース2の型
     * @return 特化された命令のインデックス
     */
    size_t SpecializeSub(IRFunction* function, int32_t dest, int32_t src1, int32_t src2,
                       TypeFeedbackRecord::TypeCategory type1, TypeFeedbackRecord::TypeCategory type2);
    
    /**
     * @brief 乗算命令を型に特化する
     * @param function 対象のIR関数
     * @param dest 格納先レジスタ
     * @param src1 ソースレジスタ1
     * @param src2 ソースレジスタ2
     * @param type1 ソース1の型
     * @param type2 ソース2の型
     * @return 特化された命令のインデックス
     */
    size_t SpecializeMul(IRFunction* function, int32_t dest, int32_t src1, int32_t src2,
                       TypeFeedbackRecord::TypeCategory type1, TypeFeedbackRecord::TypeCategory type2);
    
    /**
     * @brief 除算命令を型に特化する
     * @param function 対象のIR関数
     * @param dest 格納先レジスタ
     * @param src1 ソースレジスタ1
     * @param src2 ソースレジスタ2
     * @param type1 ソース1の型
     * @param type2 ソース2の型
     * @return 特化された命令のインデックス
     */
    size_t SpecializeDiv(IRFunction* function, int32_t dest, int32_t src1, int32_t src2,
                       TypeFeedbackRecord::TypeCategory type1, TypeFeedbackRecord::TypeCategory type2);
    
    /**
     * @brief 比較命令を型に特化する
     * @param function 対象のIR関数
     * @param opcode 元の比較命令
     * @param dest 格納先レジスタ
     * @param src1 ソースレジスタ1
     * @param src2 ソースレジスタ2
     * @param type1 ソース1の型
     * @param type2 ソース2の型
     * @return 特化された命令のインデックス
     */
    size_t SpecializeCompare(IRFunction* function, Opcode opcode, int32_t dest, int32_t src1, int32_t src2,
                           TypeFeedbackRecord::TypeCategory type1, TypeFeedbackRecord::TypeCategory type2);
    
    /**
     * @brief バイトコードオフセットからIRインデックスへのマッピングを設定
     * @param bytecodeToIRMap オフセットからインデックスへのマップ
     */
    void SetBytecodeToIRMapping(const std::unordered_map<uint32_t, size_t>& bytecodeToIRMap) {
        m_bytecodeToIRMap = bytecodeToIRMap;
    }
    
    /**
     * @brief バイトコードオフセットに対応するIRインデックスを取得
     * @param bytecodeOffset バイトコードオフセット
     * @return IRインデックス（対応がない場合は-1）
     */
    int64_t GetIRIndexForBytecodeOffset(uint32_t bytecodeOffset) const {
        auto it = m_bytecodeToIRMap.find(bytecodeOffset);
        if (it != m_bytecodeToIRMap.end()) {
            return static_cast<int64_t>(it->second);
        }
        return -1;
    }
    
    /**
     * @brief 型ガードのカウントを取得
     * @return 追加された型ガードの数
     */
    size_t GetTypeGuardCount() const {
        return m_guardCount;
    }
    
    /**
     * @brief 型特化命令のカウントを取得
     * @return 型特化された命令の数
     */
    size_t GetSpecializedInstructionCount() const {
        return m_specializationCount;
    }
    
    /**
     * @brief 逆最適化トリガーのカウントを取得
     * @return 挿入された逆最適化トリガーの数
     */
    size_t GetDeoptimizationTriggerCount() const {
        return m_deoptCount;
    }
    
private:
    /**
     * @brief 型チェックのIR命令を生成
     * @param function 対象のIR関数
     * @param reg チェックするレジスタ
     * @param expectedType 期待される型
     * @return 生成された型チェック命令のインデックス
     */
    size_t InsertTypeCheck(IRFunction* function, int32_t reg, TypeFeedbackRecord::TypeCategory expectedType);
    
    /**
     * @brief 逆最適化トリガーのIR命令を生成
     * @param function 対象のIR関数
     * @param bytecodeOffset バイトコードオフセット
     * @param reason 逆最適化理由
     * @return 生成された逆最適化トリガー命令のインデックス
     */
    size_t InsertDeoptimizationTrigger(IRFunction* function, uint32_t bytecodeOffset, const std::string& reason);
    
    /**
     * @brief 整数特化パスにジャンプするIR命令を生成
     * @param function 対象のIR関数
     * @param labelName ジャンプ先ラベル名
     * @return 生成されたジャンプ命令のインデックス
     */
    size_t InsertJumpToIntegerPath(IRFunction* function, const std::string& labelName);
    
    /**
     * @brief 浮動小数点特化パスにジャンプするIR命令を生成
     * @param function 対象のIR関数
     * @param labelName ジャンプ先ラベル名
     * @return 生成されたジャンプ命令のインデックス
     */
    size_t InsertJumpToFloatPath(IRFunction* function, const std::string& labelName);
    
    /**
     * @brief 文字列特化パスにジャンプするIR命令を生成
     * @param function 対象のIR関数
     * @param labelName ジャンプ先ラベル名
     * @return 生成されたジャンプ命令のインデックス
     */
    size_t InsertJumpToStringPath(IRFunction* function, const std::string& labelName);
    
private:
    // バイトコードオフセットからIRインデックスへのマッピング
    std::unordered_map<uint32_t, size_t> m_bytecodeToIRMap;
    
    // 各レジスタに関連付けられた型情報
    std::unordered_map<int32_t, TypeFeedbackRecord::TypeCategory> m_regTypeMap;
    
    // 挿入されたガード命令インデックス
    std::vector<size_t> m_guardedInstructions;
    
    // 統計情報
    size_t m_guardCount;
    size_t m_specializationCount;
    size_t m_deoptCount;
};

}  // namespace core
}  // namespace aerojs 