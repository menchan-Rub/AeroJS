#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <map>
#include <set>
#include "ir.h"
#include "value.h"

namespace aerojs::core {

/**
 * @brief JavaScript値の型を表す列挙型
 */
enum class ValueType {
    kUnknown,        ///< 不明な型
    kUndefined,      ///< undefined型
    kNull,           ///< null型
    kBoolean,        ///< Boolean型
    kInteger,        ///< 整数型
    kNumber,         ///< 浮動小数点数型
    kString,         ///< 文字列型
    kSymbol,         ///< Symbol型
    kObject,         ///< 一般オブジェクト型
    kArray,          ///< 配列型
    kFunction,       ///< 関数型
    kAny,            ///< どの型にもなりうる（型混合）
    kTypeCount       ///< 型の総数（列挙型の終端マーカー）
};

/**
 * @brief 型情報を表す構造体
 */
struct TypeInfo {
    ValueType primaryType;            ///< 最も可能性の高い型
    uint32_t possibleTypes;           ///< 可能性のある型をビットフラグで表現
    std::vector<double> probabilities; ///< 各型の確率（オプション）
    
    /**
     * @brief デフォルトコンストラクタ
     * 不明な型で初期化
     */
    TypeInfo() 
        : primaryType(ValueType::kUnknown), 
          possibleTypes(1 << static_cast<uint32_t>(ValueType::kUnknown)) {}
    
    /**
     * @brief 特定の型で初期化するコンストラクタ
     */
    explicit TypeInfo(ValueType type) 
        : primaryType(type), 
          possibleTypes(1 << static_cast<uint32_t>(type)) {}
    
    /**
     * @brief 型がセットに含まれているかどうかを確認
     */
    bool HasType(ValueType type) const {
        return (possibleTypes & (1 << static_cast<uint32_t>(type))) != 0;
    }
    
    /**
     * @brief 型をセットに追加
     */
    void AddType(ValueType type) {
        possibleTypes |= (1 << static_cast<uint32_t>(type));
    }
    
    /**
     * @brief 型をセットから削除
     */
    void RemoveType(ValueType type) {
        possibleTypes &= ~(1 << static_cast<uint32_t>(type));
    }
    
    /**
     * @brief この型が数値型かどうかを確認
     */
    bool IsNumeric() const {
        return HasType(ValueType::kInteger) || HasType(ValueType::kNumber);
    }
    
    /**
     * @brief この型がプリミティブ型かどうかを確認
     */
    bool IsPrimitive() const {
        return HasType(ValueType::kUndefined) ||
               HasType(ValueType::kNull) ||
               HasType(ValueType::kBoolean) ||
               HasType(ValueType::kInteger) ||
               HasType(ValueType::kNumber) ||
               HasType(ValueType::kString) ||
               HasType(ValueType::kSymbol);
    }
};

/**
 * @brief オブジェクト型の詳細情報
 */
struct ObjectTypeInfo : public TypeInfo {
    std::unordered_map<std::string, TypeInfo> properties; ///< プロパティの型情報
    
    /**
     * @brief デフォルトコンストラクタ
     */
    ObjectTypeInfo() : TypeInfo(ValueType::kObject) {}
};

/**
 * @brief 配列型の詳細情報
 */
struct ArrayTypeInfo : public TypeInfo {
    TypeInfo elementType;             ///< 要素の型情報
    uint32_t knownLength;             ///< 既知の配列長（0は不明）
    bool isHomogeneous;               ///< 同種の要素を持つかどうか
    
    /**
     * @brief デフォルトコンストラクタ
     */
    ArrayTypeInfo() 
        : TypeInfo(ValueType::kArray), 
          knownLength(0), 
          isHomogeneous(true) {}
};

/**
 * @brief 関数型の詳細情報
 */
struct FunctionTypeInfo : public TypeInfo {
    std::vector<TypeInfo> paramTypes;  ///< パラメータの型情報
    TypeInfo returnType;              ///< 戻り値の型情報
    bool isConstructor;               ///< コンストラクタ関数かどうか
    
    /**
     * @brief デフォルトコンストラクタ
     */
    FunctionTypeInfo() 
        : TypeInfo(ValueType::kFunction), 
          isConstructor(false) {}
};

/**
 * @brief ある型が別の型のサブタイプであるかどうかをチェック
 * 
 * @param subType サブタイプの候補
 * @param superType スーパータイプの候補
 * @return true subTypeがsuperTypeのサブタイプ
 * @return false subTypeがsuperTypeのサブタイプでない
 */
bool IsSubtype(ValueType subType, ValueType superType);

/**
 * @class TypeAnalyzer
 * @brief IRコード内の型情報を解析し、推論するためのクラス
 * 
 * このクラスはIR関数内の命令から型情報を解析し、
 * より詳細な型情報を推論します。フローに基づく型解析を行い、
 * 実行パスごとに異なる型情報を維持します。
 */
class TypeAnalyzer {
public:
    /**
     * @brief デフォルトコンストラクタ
     */
    TypeAnalyzer();

    /**
     * @brief デストラクタ
     */
    ~TypeAnalyzer();

    /**
     * @brief IR関数を解析し、型情報を収集・推論する
     * 
     * @param function 解析対象のIR関数
     * @return 解析が成功したかどうか
     */
    bool Analyze(IRFunction* function);

    /**
     * @brief 特定のレジスタの型情報を取得する
     * 
     * @param registerIndex レジスタインデックス
     * @param blockIndex オプション：基本ブロックインデックス（指定しない場合は最も精密な型情報）
     * @return レジスタの型情報
     */
    ValueType GetRegisterType(uint32_t registerIndex, uint32_t blockIndex = UINT32_MAX) const;

    /**
     * @brief レジスタの可能な型の集合を取得する（共用体型の場合）
     * 
     * @param registerIndex レジスタインデックス
     * @return 可能な型のビットマスク
     */
    uint32_t GetPossibleTypes(uint32_t registerIndex) const;

    /**
     * @brief 指定レジスタが特定の型である確率を取得する
     * 
     * @param registerIndex レジスタインデックス
     * @param type 問い合わせる型
     * @return 0.0〜1.0の確率値
     */
    double GetTypeProbability(uint32_t registerIndex, ValueType type) const;

    /**
     * @brief 型違反の可能性のある命令を検出する
     * 
     * @return 型違反の可能性のある命令のリスト
     */
    std::vector<const IRInstruction*> FindTypeViolations() const;

    /**
     * @brief 解析結果に基づいて型キャスト命令を挿入する
     * 
     * この方法は、型不一致を検出した場合に適切な型キャスト命令を
     * IR関数に挿入します。
     * 
     * @return 挿入したキャスト命令の数
     */
    size_t InsertTypeCasts();

    /**
     * @brief アナライザの状態をリセットする
     */
    void Reset();

    // 関数全体の型分析を行う
    void analyze(IRFunction* function);
    
    // 命令の結果型を取得
    TypeInfo getInstructionType(const IRInstruction* instruction) const;
    
    // 値の型を取得
    TypeInfo getValueType(const Value* value) const;

private:
    // 型情報構造体
    struct TypeInfo {
        ValueType primaryType;   // 主要な型
        uint32_t possibleTypes;  // 可能性のある型のビットマスク
        std::vector<double> probabilities;  // 各型の確率

        TypeInfo() : primaryType(ValueType::kAny), possibleTypes(0) {}
    };

    // 基本ブロックの型コンテキスト
    struct BlockTypeContext {
        std::vector<TypeInfo> registerTypes;  // このブロック内での各レジスタの型情報
        bool analyzed;                        // このブロックが解析済みかどうか
        
        BlockTypeContext() : analyzed(false) {}
    };

    // フロー解析状態
    struct FlowState {
        std::vector<TypeInfo> registerTypes;  // 現在の型情報
        std::vector<bool> visited;            // 訪問済みブロック
        std::vector<bool> inQueue;            // キュー内のブロック
    };

    // 型解析ヘルパーメソッド
    void AnalyzeInstruction(const IRInstruction* inst, TypeInfo& resultType);
    void PropagateTypes();
    void MergeTypes(TypeInfo& target, const TypeInfo& source);
    void NarrowType(TypeInfo& info, ValueType type);
    void RefineTypeFromConstraint(TypeInfo& info, ValueType constraint);
    bool IsSubtype(ValueType subType, ValueType superType) const;
    uint32_t TypeToMask(ValueType type) const;
    ValueType MaskToPrimaryType(uint32_t mask) const;
    bool HasCompatibleTypes(const TypeInfo& lhs, const TypeInfo& rhs) const;

    // ビットマスク操作ヘルパー
    static constexpr uint32_t BIT(ValueType type) { 
        return 1U << static_cast<uint32_t>(type); 
    }

    // インデックス検証ヘルパー
    bool IsValidRegisterIndex(uint32_t index) const;
    bool IsValidBlockIndex(uint32_t index) const;

    // 内部状態
    IRFunction* m_function;                      // 解析対象の関数
    std::vector<BlockTypeContext> m_blockTypes;  // ブロックごとの型コンテキスト
    std::vector<TypeInfo> m_finalTypes;          // 最終的な型情報
    bool m_fixedPointReached;                   // 不動点に到達したか
    size_t m_iterationCount;                    // 解析イテレーション回数
    static constexpr size_t kMaxIterations = 10; // 最大イテレーション数

    // 命令ごとの型推論
    void inferTypesForInstruction(IRInstruction* instruction);
    
    // 型情報を設定
    void setValueType(const Value* value, const TypeInfo& type);
    
    // 各命令タイプの型推論
    TypeInfo inferConstType(const IRInstruction* instruction);
    TypeInfo inferLoadType(const IRInstruction* instruction);
    TypeInfo inferStoreType(const IRInstruction* instruction);
    TypeInfo inferBinaryOpType(const IRInstruction* instruction);
    TypeInfo inferCompareOpType(const IRInstruction* instruction);
    TypeInfo inferCallType(const IRInstruction* instruction);
    TypeInfo inferPropertyAccessType(const IRInstruction* instruction);
    
    // 命令ID -> 型情報のマップ
    std::map<uint32_t, TypeInfo> m_valueTypes;
    
    // 型が変更された命令のセット
    std::set<uint32_t> m_changedInstructions;
    
    // 現在分析中の関数
    IRFunction* m_currentFunction;
};

} // namespace aerojs::core 