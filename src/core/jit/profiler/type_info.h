/**
 * @file type_info.h
 * @brief 最先端の型プロファイリングシステム
 * @version 2.0.0
 * @license MIT
 */

#ifndef AEROJS_TYPE_INFO_H
#define AEROJS_TYPE_INFO_H

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <bitset>
#include <optional>
#include <array>
#include <functional>

#include "src/core/runtime/values/value.h"
#include "src/core/runtime/types/value_type.h"

namespace aerojs {
namespace core {
namespace jit {
namespace profiler {

/**
 * @brief オブジェクト形状の型情報
 */
class ObjectShape {
public:
  /**
   * @brief プロパティ情報
   */
  struct Property {
    std::string name;                  // プロパティ名
    runtime::ValueType type;           // プロパティ型
    bool isConstant;                   // 定数プロパティか
    std::optional<runtime::Value> constantValue;  // 定数値（定数の場合）
    uint32_t offset;                   // オブジェクト内でのオフセット
    
    Property(const std::string& n, runtime::ValueType t, bool isConst = false)
      : name(n), type(t), isConstant(isConst), offset(0) {}
  };
  
  ObjectShape() : m_id(0), m_parentId(0), m_prototypeId(0), m_properties(), m_flags(0) {}
  
  // 形状ID
  uint32_t getId() const { return m_id; }
  void setId(uint32_t id) { m_id = id; }
  
  // 継承関係
  uint32_t getParentId() const { return m_parentId; }
  void setParentId(uint32_t id) { m_parentId = id; }
  
  uint32_t getPrototypeId() const { return m_prototypeId; }
  void setPrototypeId(uint32_t id) { m_prototypeId = id; }
  
  // プロパティ管理
  void addProperty(const Property& prop);
  const Property* getProperty(const std::string& name) const;
  const std::vector<Property>& getProperties() const { return m_properties; }
  bool hasProperty(const std::string& name) const;
  
  // フラグ
  enum Flags {
    IsArray          = 1 << 0,
    IsFunction       = 1 << 1,
    IsRegExp         = 1 << 2,
    IsDate           = 1 << 3,
    HasIndexedProps  = 1 << 4,
    HasNamedProps    = 1 << 5,
    IsFrozen         = 1 << 6,
    IsSealed         = 1 << 7,
    IsExtensible     = 1 << 8,
    HasTransition    = 1 << 9
  };
  
  bool hasFlag(Flags flag) const { return (m_flags & static_cast<uint32_t>(flag)) != 0; }
  void setFlag(Flags flag, bool value = true) {
    if (value) {
      m_flags |= static_cast<uint32_t>(flag);
    } else {
      m_flags &= ~static_cast<uint32_t>(flag);
    }
  }
  
  // 形状の互換性チェック
  bool isCompatibleWith(const ObjectShape& other) const;
  
  // 形状の類似度（0.0～1.0）
  float similarityWith(const ObjectShape& other) const;
  
private:
  uint32_t m_id;                     // 形状ID
  uint32_t m_parentId;               // 親形状ID
  uint32_t m_prototypeId;            // プロトタイプ形状ID
  std::vector<Property> m_properties; // プロパティリスト
  uint32_t m_flags;                  // フラグ
};

/**
 * @brief 型情報の分類
 */
enum class TypeCategory {
  Unknown,      // 不明
  Uninitialized,// 未初期化
  Monomorphic,  // 単一型（最も効率的）
  Polymorphic,  // 少数の型（2～4種類）
  MegaMorphic   // 多数の型（最適化困難）
};

/**
 * @brief 値の型プロファイル情報
 */
class TypeInfo {
public:
  /**
   * @brief 型の出現情報
   */
  struct TypeOccurrence {
    runtime::ValueType type;         // 値の型
    uint32_t count;                  // 出現回数
    std::optional<uint32_t> shapeId; // オブジェクト形状ID（オブジェクト型のみ）
    
    TypeOccurrence(runtime::ValueType t, uint32_t c = 1, 
                  std::optional<uint32_t> shape = std::nullopt)
      : type(t), count(c), shapeId(shape) {}
  };
  
  TypeInfo();
  
  // 型情報の記録
  void recordType(const runtime::Value& value);
  void recordType(runtime::ValueType type, std::optional<uint32_t> shapeId = std::nullopt);
  
  // 型情報の問い合わせ
  TypeCategory getCategory() const;
  uint32_t getTypeCount() const { return static_cast<uint32_t>(m_types.size()); }
  const std::vector<TypeOccurrence>& getTypes() const { return m_types; }
  
  // 最も一般的な型
  runtime::ValueType getMostCommonType() const;
  std::optional<uint32_t> getMostCommonShapeId() const;
  
  // 特定の型の比率（0.0～1.0）
  float getTypeRatio(runtime::ValueType type) const;
  
  // 型チェック
  bool isMonomorphic() const { return getCategory() == TypeCategory::Monomorphic; }
  bool isPolymorphic() const { return getCategory() == TypeCategory::Polymorphic; }
  bool isMegamorphic() const { return getCategory() == TypeCategory::MegaMorphic; }
  
  // 数値系特殊チェック
  bool isAlwaysInt32() const;
  bool isAlwaysNumber() const;
  bool isMostlyInt32() const;    // 95%以上がInt32
  bool isMostlyNumber() const;   // 95%以上が数値
  
  // オブジェクト系特殊チェック
  bool isAlwaysSameShape() const;
  bool isMostlySameShape() const; // 90%以上が同一形状
  
  // 型検出の信頼性（サンプル数による）
  float getConfidence() const;
  
  // 型安定性（型が変化しにくいか）
  float getStability() const { return m_stability; }
  
  // リセット
  void reset();
  
  // アルファブレンド
  void blend(const TypeInfo& other, float alpha);
  
  // 型のフィルタリング（特定の型だけを残す）
  void filterTypes(const std::function<bool(const TypeOccurrence&)>& predicate);
  
  // デバッグ用文字列表現
  std::string toString() const;
  
private:
  std::vector<TypeOccurrence> m_types;  // 記録された型
  uint32_t m_totalObservations;         // 総観測回数
  uint32_t m_typeTransitions;           // 型遷移回数
  float m_stability;                    // 型安定性（0.0～1.0）
  
  // 内部ヘルパー
  void updateStability();
  void consolidateTypes();
};

/**
 * @brief 関数呼び出しの型プロファイル情報
 */
class CallSiteTypeInfo {
public:
  CallSiteTypeInfo();
  
  // 引数の型情報
  void recordArgTypes(const std::vector<runtime::Value>& args);
  void recordReturnType(const runtime::Value& value);
  
  // タイプ情報取得
  const std::vector<TypeInfo>& getArgTypeInfos() const { return m_argTypeInfos; }
  const TypeInfo& getReturnTypeInfo() const { return m_returnTypeInfo; }
  
  // 呼び出し情報
  uint32_t getCallCount() const { return m_callCount; }
  bool isHot() const { return m_callCount >= 10; }
  
  // 成功率
  float getSuccessRatio() const { 
    return m_callCount > 0 ? 
      static_cast<float>(m_successCount) / m_callCount : 0.0f; 
  }
  
  // 例外発生率
  float getExceptionRatio() const { 
    return m_callCount > 0 ? 
      static_cast<float>(m_exceptionCount) / m_callCount : 0.0f; 
  }
  
  // 呼び出し結果の記録
  void recordSuccess() { m_successCount++; }
  void recordException() { m_exceptionCount++; }
  
  // リセット
  void reset();
  
  // デバッグ用文字列表現
  std::string toString() const;
  
private:
  std::vector<TypeInfo> m_argTypeInfos;  // 引数型情報
  TypeInfo m_returnTypeInfo;             // 戻り値型情報
  uint32_t m_callCount;                  // 呼び出し回数
  uint32_t m_successCount;               // 成功回数
  uint32_t m_exceptionCount;             // 例外発生回数
};

/**
 * @brief 型プロファイル管理
 * 
 * 実行時の型情報を収集・管理し、JIT最適化に提供するクラス
 */
class TypeProfiler {
public:
  TypeProfiler();
  ~TypeProfiler();
  
  // プロファイリング制御
  void enable();
  void disable();
  bool isEnabled() const { return m_enabled; }
  
  // オブジェクト形状
  ObjectShape* getOrCreateObjectShape(uint32_t shapeId);
  const ObjectShape* getObjectShape(uint32_t shapeId) const;
  void recordObjectShape(uint32_t shapeId, const ObjectShape& shape);
  
  // 変数型情報
  TypeInfo* getOrCreateVarTypeInfo(uint32_t functionId, uint32_t varIndex);
  const TypeInfo* getVarTypeInfo(uint32_t functionId, uint32_t varIndex) const;
  
  // 引数型情報
  TypeInfo* getOrCreateParamTypeInfo(uint32_t functionId, uint32_t paramIndex);
  const TypeInfo* getParamTypeInfo(uint32_t functionId, uint32_t paramIndex) const;
  
  // プロパティ型情報
  TypeInfo* getOrCreatePropertyTypeInfo(uint32_t shapeId, const std::string& propName);
  const TypeInfo* getPropertyTypeInfo(uint32_t shapeId, const std::string& propName) const;
  
  // 配列要素型情報
  TypeInfo* getOrCreateArrayElementTypeInfo(uint32_t arrayShapeId);
  const TypeInfo* getArrayElementTypeInfo(uint32_t arrayShapeId) const;
  
  // 呼び出し型情報
  CallSiteTypeInfo* getOrCreateCallSiteTypeInfo(uint32_t functionId, uint32_t callSiteOffset);
  const CallSiteTypeInfo* getCallSiteTypeInfo(uint32_t functionId, uint32_t callSiteOffset) const;
  
  // コレクションサイズ予測
  uint32_t predictCollectionSize(uint32_t functionId, uint32_t siteOffset);
  void recordCollectionSize(uint32_t functionId, uint32_t siteOffset, uint32_t size);
  
  // 統計
  uint32_t getTotalTypeObservations() const { return m_totalTypeObservations; }
  uint32_t getShapeCount() const { return static_cast<uint32_t>(m_objectShapes.size()); }
  uint32_t getHotFunctionCount() const;
  
  // ホット関数判定（頻繁に呼ばれる関数）
  bool isHotFunction(uint32_t functionId) const;
  
  // モノモーフィック関数判定（引数と戻り値の型が安定している関数）
  bool isMonomorphicFunction(uint32_t functionId) const;
  
  // 型情報のエクスポート/インポート
  std::string exportTypeProfile() const;
  bool importTypeProfile(const std::string& data);
  
  // プロファイルデータのクリア
  void clearAll();
  void clearFunction(uint32_t functionId);
  
  // デバッグ情報
  std::string dumpTypeProfile() const;
  
private:
  bool m_enabled;                    // 有効フラグ
  uint32_t m_totalTypeObservations;  // 総観測回数
  
  // オブジェクト形状
  std::unordered_map<uint32_t, std::unique_ptr<ObjectShape>> m_objectShapes;
  
  // 変数型情報
  std::unordered_map<uint32_t, 
                    std::unordered_map<uint32_t, std::unique_ptr<TypeInfo>>> m_varTypeInfos;
  
  // 引数型情報
  std::unordered_map<uint32_t, 
                    std::unordered_map<uint32_t, std::unique_ptr<TypeInfo>>> m_paramTypeInfos;
  
  // プロパティ型情報
  std::unordered_map<uint32_t, 
                    std::unordered_map<std::string, std::unique_ptr<TypeInfo>>> m_propertyTypeInfos;
  
  // 配列要素型情報
  std::unordered_map<uint32_t, std::unique_ptr<TypeInfo>> m_arrayElementTypeInfos;
  
  // 呼び出し型情報
  std::unordered_map<uint32_t, 
                    std::unordered_map<uint32_t, std::unique_ptr<CallSiteTypeInfo>>> m_callSiteTypeInfos;
  
  // コレクションサイズ予測
  std::unordered_map<uint32_t, 
                    std::unordered_map<uint32_t, std::vector<uint32_t>>> m_collectionSizeHistory;
};

} // namespace profiler
} // namespace jit
} // namespace core
} // namespace aerojs

#endif // AEROJS_TYPE_INFO_H 