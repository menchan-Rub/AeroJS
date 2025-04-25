#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <bitset>

#include "../ir/ir.h"

namespace aerojs {
namespace core {

/**
 * @brief 値の基本型を表す列挙型
 */
enum class ValueType : uint8_t {
  kUnknown,       // 不明な型
  kUndefined,     // undefined
  kNull,          // null
  kBoolean,       // 真偽値
  kInteger,       // 整数
  kNumber,        // 浮動小数点数
  kString,        // 文字列
  kObject,        // オブジェクト
  kArray,         // 配列
  kFunction,      // 関数
  kSymbol,        // シンボル
  kBigInt,        // BigInt
  kDate,          // Date
  kRegExp,        // 正規表現
  kMap,           // Map
  kSet,           // Set
  kPromise,       // Promise
  kArrayBuffer,   // ArrayBuffer
  kTypedArray,    // TypedArray
  kDataView,      // DataView
  kMax            // 最大値（型の数）
};

/**
 * @brief 型情報を表すクラス
 * 
 * 基本型とオプションの補足情報を含む
 */
class TypeInfo {
public:
  /**
   * @brief コンストラクタ
   * @param type 基本型
   */
  explicit TypeInfo(ValueType type = ValueType::kUnknown) noexcept;
  
  /**
   * @brief デストラクタ
   */
  ~TypeInfo() noexcept = default;
  
  /**
   * @brief 基本型を取得
   * @return 基本型
   */
  ValueType GetBaseType() const noexcept;
  
  /**
   * @brief 基本型を設定
   * @param type 設定する基本型
   */
  void SetBaseType(ValueType type) noexcept;
  
  /**
   * @brief 整数定数値を持つかどうかを取得
   * @return 整数定数値を持つ場合true
   */
  bool HasIntConstantValue() const noexcept;
  
  /**
   * @brief 整数定数値を取得
   * @return 整数定数値
   */
  int32_t GetIntConstantValue() const noexcept;
  
  /**
   * @brief 整数定数値を設定
   * @param value 設定する整数値
   */
  void SetIntConstantValue(int32_t value) noexcept;
  
  /**
   * @brief 浮動小数点定数値を持つかどうかを取得
   * @return 浮動小数点定数値を持つ場合true
   */
  bool HasDoubleConstantValue() const noexcept;
  
  /**
   * @brief 浮動小数点定数値を取得
   * @return 浮動小数点定数値
   */
  double GetDoubleConstantValue() const noexcept;
  
  /**
   * @brief 浮動小数点定数値を設定
   * @param value 設定する浮動小数点値
   */
  void SetDoubleConstantValue(double value) noexcept;
  
  /**
   * @brief 文字列定数値を持つかどうかを取得
   * @return 文字列定数値を持つ場合true
   */
  bool HasStringConstantValue() const noexcept;
  
  /**
   * @brief 文字列定数値を取得
   * @return 文字列定数値
   */
  const std::string& GetStringConstantValue() const noexcept;
  
  /**
   * @brief 文字列定数値を設定
   * @param value 設定する文字列値
   */
  void SetStringConstantValue(const std::string& value) noexcept;
  
  /**
   * @brief 型が未定義かどうかを取得
   * @return 未定義の場合true
   */
  bool IsUndefined() const noexcept;
  
  /**
   * @brief 型がnullかどうかを取得
   * @return nullの場合true
   */
  bool IsNull() const noexcept;
  
  /**
   * @brief 型が真偽値かどうかを取得
   * @return 真偽値の場合true
   */
  bool IsBoolean() const noexcept;
  
  /**
   * @brief 型が整数かどうかを取得
   * @return 整数の場合true
   */
  bool IsInteger() const noexcept;
  
  /**
   * @brief 型が数値かどうかを取得
   * @return 数値の場合true
   */
  bool IsNumber() const noexcept;
  
  /**
   * @brief 型が文字列かどうかを取得
   * @return 文字列の場合true
   */
  bool IsString() const noexcept;
  
  /**
   * @brief 型が関数かどうかを取得
   * @return 関数の場合true
   */
  bool IsFunction() const noexcept;
  
  /**
   * @brief 型がオブジェクトかどうかを取得
   * @return オブジェクトの場合true
   */
  bool IsObject() const noexcept;

  /**
   * @brief 型をマージする
   * @param other マージする型情報
   * @return マージ結果
   */
  TypeInfo Merge(const TypeInfo& other) const noexcept;
  
  /**
   * @brief 型情報を文字列形式で取得
   * @return 型情報の文字列表現
   */
  std::string ToString() const noexcept;

private:
  // 基本型
  ValueType m_baseType;
  
  // 整数定数値
  bool m_hasIntConstant;
  int32_t m_intConstantValue;
  
  // 浮動小数点定数値
  bool m_hasDoubleConstant;
  double m_doubleConstantValue;
  
  // 文字列定数値
  bool m_hasStringConstant;
  std::string m_stringConstantValue;
};

/**
 * @brief 型の推論結果を表す構造体
 */
struct TypeInferenceResult {
  // レジスタの型情報を格納
  std::unordered_map<uint32_t, TypeInfo> registerTypes;
  
  // 命令の型情報を格納
  std::vector<TypeInfo> instructionTypes;
  
  // 変数名の型情報を格納
  std::unordered_map<std::string, TypeInfo> variableTypes;
};

/**
 * @brief 型推論エンジンクラス
 * 
 * IR関数内のレジスタと命令の型を推論する責務を持ちます。
 */
class TypeInferenceEngine {
public:
  /**
   * @brief コンストラクタ
   */
  TypeInferenceEngine() noexcept;
  
  /**
   * @brief デストラクタ
   */
  ~TypeInferenceEngine() noexcept;
  
  /**
   * @brief IR関数の型推論を実行
   * @param function 対象のIR関数
   * @return 型推論結果
   */
  TypeInferenceResult InferTypes(const IRFunction& function) noexcept;
  
  /**
   * @brief 特定のレジスタの型情報を設定
   * @param regId レジスタID
   * @param type 設定する型情報
   */
  void SetRegisterType(uint32_t regId, const TypeInfo& type) noexcept;
  
  /**
   * @brief 特定の変数名の型情報を設定
   * @param varName 変数名
   * @param type 設定する型情報
   */
  void SetVariableType(const std::string& varName, const TypeInfo& type) noexcept;
  
  /**
   * @brief 型推論の最大繰り返し回数を設定
   * @param count 最大繰り返し回数
   */
  void SetMaxIterations(uint32_t count) noexcept;
  
  /**
   * @brief 推論エンジンをリセット
   */
  void Reset() noexcept;

private:
  /**
   * @brief 命令の型推論を実行
   * @param inst 対象の命令
   * @param result 型推論結果
   * @return 型情報が変更された場合true
   */
  bool InferInstruction(const IRInstruction& inst, TypeInferenceResult& result) noexcept;
  
  /**
   * @brief 算術演算命令の型推論を実行
   * @param opcode 命令コード
   * @param resultReg 結果レジスタ
   * @param op1Type 第1オペランドの型
   * @param op2Type 第2オペランドの型
   * @param result 型推論結果
   * @return 結果の型情報
   */
  TypeInfo InferArithmeticOp(Opcode opcode, uint32_t resultReg, 
                            const TypeInfo& op1Type, 
                            const TypeInfo& op2Type,
                            TypeInferenceResult& result) noexcept;
  
  /**
   * @brief 整数同士の算術演算を評価
   * @param opcode 命令コード
   * @param val1 第1オペランド値
   * @param val2 第2オペランド値
   * @return 結果の型情報
   */
  TypeInfo EvaluateIntegerOperation(Opcode opcode, int32_t val1, int32_t val2) noexcept;
  
  /**
   * @brief 浮動小数点同士の算術演算を評価
   * @param opcode 命令コード
   * @param val1 第1オペランド値
   * @param val2 第2オペランド値
   * @return 結果の型情報
   */
  TypeInfo EvaluateDoubleOperation(Opcode opcode, double val1, double val2) noexcept;

private:
  // 既知のレジスタ型
  std::unordered_map<uint32_t, TypeInfo> m_knownRegisterTypes;
  
  // 既知の変数型
  std::unordered_map<std::string, TypeInfo> m_knownVariableTypes;
  
  // 最大繰り返し回数
  uint32_t m_maxIterations;
};

/**
 * @brief 値の型を文字列に変換
 * @param type 変換する値の型
 * @return 型の文字列表現
 */
std::string ValueTypeToString(ValueType type);

} // namespace core
} // namespace aerojs 