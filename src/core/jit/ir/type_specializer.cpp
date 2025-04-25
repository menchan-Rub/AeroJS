#include "type_specializer.h"

#include <cassert>
#include <algorithm>
#include <unordered_set>
#include <sstream>

namespace aerojs {
namespace core {

// 拡張Opcodeの定義（型特化された命令用）
// IRクラスに追加するべきだが、ここではシンプルに定義する
enum class ExtendedOpcode : uint8_t {
  // 整数特化命令
  IntAdd = 100,
  IntSub = 101,
  IntMul = 102,
  IntDiv = 103,
  
  // 浮動小数点特化命令
  DoubleAdd = 110,
  DoubleSub = 111,
  DoubleMul = 112,
  DoubleDiv = 113,
  
  // 比較演算特化命令
  IntEqual = 120,
  IntNotEqual = 121,
  IntLessThan = 122,
  IntLessThanOrEqual = 123,
  IntGreaterThan = 124,
  IntGreaterThanOrEqual = 125,
  
  DoubleEqual = 130,
  DoubleNotEqual = 131,
  DoubleLessThan = 132,
  DoubleLessThanOrEqual = 133,
  DoubleGreaterThan = 134,
  DoubleGreaterThanOrEqual = 135,
  
  // 型チェック・ガード命令
  TypeGuard = 140,
  TypeAssert = 141,
  
  // オブジェクト特化命令
  LoadProperty = 150,
  StoreProperty = 151,
  LoadElement = 152,
  StoreElement = 153,
  
  // 配列特化命令
  LoadTypedArrayElement = 160,
  StoreTypedArrayElement = 161,
  ArrayLength = 162,
  
  // 文字列特化命令
  StringConcat = 170,
  StringEqual = 171,
};

TypeSpecializer::TypeSpecializer()
    : m_guardCount(0)
    , m_specializationCount(0)
    , m_deoptCount(0)
{
}

TypeSpecializer::~TypeSpecializer() = default;

// 型情報の等価比較演算子
bool TypeInfo::operator==(const TypeInfo& other) const noexcept {
  // 基本型の比較
  if (type != other.type || 
      nullable != other.nullable || 
      maybeUndefined != other.maybeUndefined) {
    return false;
  }
  
  // 数値型の場合は範囲も比較
  if (type == JSValueType::Integer || 
      type == JSValueType::Double || 
      type == JSValueType::SmallInt ||
      type == JSValueType::HeapNumber) {
    
    if (range.hasLowerBound != other.range.hasLowerBound ||
        range.hasUpperBound != other.range.hasUpperBound) {
      return false;
    }
    
    if (range.hasLowerBound && 
        std::abs(range.lowerBound - other.range.lowerBound) > 1e-10) {
      return false;
    }
    
    if (range.hasUpperBound && 
        std::abs(range.upperBound - other.range.upperBound) > 1e-10) {
      return false;
    }
  }
  
  // オブジェクト型の場合は形状情報を比較
  if (type == JSValueType::Object) {
    if (shape.shapeId != other.shape.shapeId ||
        shape.isMonomorphic != other.shape.isMonomorphic ||
        shape.isPoly2 != other.shape.isPoly2 ||
        shape.isPoly3 != other.shape.isPoly3 ||
        shape.isPoly4 != other.shape.isPoly4 ||
        shape.isPolymorphic != other.shape.isPolymorphic ||
        shape.isMegamorphic != other.shape.isMegamorphic) {
      return false;
    }
  }
  
  // 配列型の場合は配列情報を比較
  if (type == JSValueType::Array) {
    if (array.isHomogeneous != other.array.isHomogeneous ||
        array.elemType != other.array.elemType ||
        array.isPacked != other.array.isPacked ||
        array.hasHoles != other.array.hasHoles ||
        array.isContinuous != other.array.isContinuous) {
      return false;
    }
  }
  
  return true;
}

// 型情報のマージ
TypeInfo TypeInfo::Merge(const TypeInfo& other) const noexcept {
  TypeInfo result;
  
  // 両方が同じ型の場合は型を維持、それ以外は Unknown になる
  if (type == other.type) {
    result.type = type;
  } else {
    result.type = JSValueType::Unknown;
  }
  
  // null と undefined の可能性はどちらかがtrue なら true
  result.nullable = nullable || other.nullable;
  result.maybeUndefined = maybeUndefined || other.maybeUndefined;
  
  // 数値型の場合は範囲をマージ
  if (result.type == JSValueType::Integer || 
      result.type == JSValueType::Double || 
      result.type == JSValueType::SmallInt || 
      result.type == JSValueType::HeapNumber) {
    
    // 下限のマージ
    if (range.hasLowerBound && other.range.hasLowerBound) {
      result.range.hasLowerBound = true;
      result.range.lowerBound = std::min(range.lowerBound, other.range.lowerBound);
    } else if (range.hasLowerBound) {
      result.range.hasLowerBound = true;
      result.range.lowerBound = range.lowerBound;
    } else if (other.range.hasLowerBound) {
      result.range.hasLowerBound = true;
      result.range.lowerBound = other.range.lowerBound;
    }
    
    // 上限のマージ
    if (range.hasUpperBound && other.range.hasUpperBound) {
      result.range.hasUpperBound = true;
      result.range.upperBound = std::max(range.upperBound, other.range.upperBound);
    } else if (range.hasUpperBound) {
      result.range.hasUpperBound = true;
      result.range.upperBound = range.upperBound;
    } else if (other.range.hasUpperBound) {
      result.range.hasUpperBound = true;
      result.range.upperBound = other.range.upperBound;
    }
  }
  
  // オブジェクト形状のマージ
  if (result.type == JSValueType::Object) {
    // 形状が同じ場合のみモノモーフィック
    result.shape.isMonomorphic = shape.isMonomorphic && other.shape.isMonomorphic &&
                                 shape.shapeId == other.shape.shapeId;
    
    if (result.shape.isMonomorphic) {
      result.shape.shapeId = shape.shapeId;
    } else {
      // 異なる形状の場合はポリモーフィック
      result.shape.isPolymorphic = true;
      
      // 2つの特定形状が既知の場合はPoly2
      if ((shape.isMonomorphic && other.isMonomorphic) ||
          (shape.isMonomorphic && other.isPoly2) ||
          (shape.isPoly2 && other.isMonomorphic)) {
        result.shape.isPoly2 = true;
      } 
      // 3つの特定形状が既知の場合はPoly3
      else if ((shape.isMonomorphic && other.isPoly3) ||
               (shape.isPoly2 && other.isMonomorphic) ||
               (shape.isPoly2 && other.isPoly2)) {
        result.shape.isPoly3 = true;
      }
      // 4つの特定形状が既知の場合はPoly4
      else if ((shape.isMonomorphic && other.isPoly4) ||
               (shape.isPoly2 && other.isPoly3) ||
               (shape.isPoly3 && other.isMonomorphic)) {
        result.shape.isPoly4 = true;
      }
      // それ以上はメガモーフィック（最適化の恩恵が低下）
      else {
        result.shape.isMegamorphic = true;
      }
    }
  }
  
  // 配列情報のマージ
  if (result.type == JSValueType::Array) {
    // 両方が同種の場合のみ同種配列
    result.array.isHomogeneous = array.isHomogeneous && other.array.isHomogeneous;
    
    // 要素型が同じ場合のみ要素型を維持
    if (array.elemType == other.array.elemType) {
      result.array.elemType = array.elemType;
    } else {
      result.array.elemType = JSValueType::Unknown;
    }
    
    // 稠密性と連続性は両方が true の場合のみ true
    result.array.isPacked = array.isPacked && other.array.isPacked;
    result.array.isContinuous = array.isContinuous && other.array.isContinuous;
    
    // 穴の有無はどちらかに穴があれば true
    result.array.hasHoles = array.hasHoles || other.array.hasHoles;
  }
  
  return result;
}

std::unique_ptr<IRFunction> TypeSpecializer::SpecializeTypes(
    const IRFunction& irFunction, 
    uint32_t functionId) noexcept {
  // まず命令列の型解析を行う
  const auto& originalInstructions = irFunction.GetInstructions();
  AnalyzeTypes(originalInstructions, functionId);
  
  // 型特化された新しい命令列を構築
  std::vector<IRInstruction> specializedInstructions;
  specializedInstructions.reserve(originalInstructions.size() * 2); // ガード命令も追加するのでサイズ増加
  
  // 各命令を型特化
  for (size_t i = 0; i < originalInstructions.size(); ++i) {
    const auto& inst = originalInstructions[i];
    
    // オペランドの型情報を収集
    std::vector<TypeInfo> operandTypes;
    
    // 通常、オペランドは前の命令の結果
    // この例では簡略化のため、引数は固定位置にあると仮定
    if (i >= 2 && (inst.opcode == Opcode::Add || 
                  inst.opcode == Opcode::Sub || 
                  inst.opcode == Opcode::Mul || 
                  inst.opcode == Opcode::Div)) {
      // 二項演算の場合、直前の2つの命令の結果型を取得
      auto it1 = m_valueTypes.find(i - 1);
      auto it2 = m_valueTypes.find(i - 2);
      
      if (it1 != m_valueTypes.end() && it2 != m_valueTypes.end()) {
        operandTypes.push_back(it2->second); // 左辺値
        operandTypes.push_back(it1->second); // 右辺値
      }
    }
    
    // オペランドの型情報に基づいて最適化された命令を生成
    IRInstruction optimizedInst = CreateOptimizedInstruction(inst, operandTypes);
    
    // 型ガードの挿入が必要な場合（型安定だが保証のため）
    if (!operandTypes.empty() && 
        (inst.opcode == Opcode::Add || 
         inst.opcode == Opcode::Sub || 
         inst.opcode == Opcode::Mul || 
         inst.opcode == Opcode::Div)) {
      
      // 左辺値の型が安定しているが、100%確実ではない場合
      if (operandTypes[0].type == JSValueType::Integer || 
          operandTypes[0].type == JSValueType::Double) {
        InsertTypeGuard(specializedInstructions, specializedInstructions.size(), operandTypes[0]);
      }
      
      // 右辺値の型が安定しているが、100%確実ではない場合
      if (operandTypes.size() > 1 && 
          (operandTypes[1].type == JSValueType::Integer || 
           operandTypes[1].type == JSValueType::Double)) {
        InsertTypeGuard(specializedInstructions, specializedInstructions.size(), operandTypes[1]);
      }
    }
    
    // 最適化された命令を追加
    specializedInstructions.push_back(optimizedInst);
  }
  
  // 新しいIR関数を作成して命令を設定
  auto specializedFunction = std::make_unique<IRFunction>();
  for (const auto& inst : specializedInstructions) {
    specializedFunction->AddInstruction(inst);
  }
  
  return specializedFunction;
}

void TypeSpecializer::InsertTypeGuard(
    std::vector<IRInstruction>& instructions,
    size_t index, 
    const TypeInfo& expectedType) noexcept {
  // TypeGuard命令を作成
  IRInstruction guardInst;
  guardInst.opcode = static_cast<Opcode>(ExtendedOpcode::TypeGuard);
  
  // 型情報をエンコード（実装の簡略化のため、ここでは型IDのみエンコード）
  int32_t typeCode = static_cast<int32_t>(expectedType.type);
  guardInst.args.push_back(typeCode);
  
  // デオプティマイズポイント情報（必要に応じて追加）
  
  // 指定位置に挿入
  if (index < instructions.size()) {
    instructions.insert(instructions.begin() + index, guardInst);
  } else {
    instructions.push_back(guardInst);
  }
}

std::optional<Opcode> TypeSpecializer::SpecializeArithmeticOp(
    Opcode opcode, 
    const TypeInfo& lhsType, 
    const TypeInfo& rhsType) noexcept {
  // 両方が整数型の場合は整数特化命令を使用
  if (lhsType.type == JSValueType::Integer && rhsType.type == JSValueType::Integer) {
    switch (opcode) {
      case Opcode::Add:
        return static_cast<Opcode>(ExtendedOpcode::IntAdd);
      case Opcode::Sub:
        return static_cast<Opcode>(ExtendedOpcode::IntSub);
      case Opcode::Mul:
        return static_cast<Opcode>(ExtendedOpcode::IntMul);
      case Opcode::Div:
        // 整数除算は割り切れない場合があるため、条件付きで最適化
        // 除数が0でないことを確認する必要がある
        if (rhsType.range.hasLowerBound && rhsType.range.hasUpperBound &&
            ((rhsType.range.lowerBound > 0 && rhsType.range.upperBound > 0) ||
             (rhsType.range.lowerBound < 0 && rhsType.range.upperBound < 0))) {
          return static_cast<Opcode>(ExtendedOpcode::IntDiv);
        }
        break;
      default:
        break;
    }
  }
  
  // 両方が浮動小数点型の場合は浮動小数点特化命令を使用
  if ((lhsType.type == JSValueType::Double || lhsType.type == JSValueType::HeapNumber) && 
      (rhsType.type == JSValueType::Double || rhsType.type == JSValueType::HeapNumber)) {
    switch (opcode) {
      case Opcode::Add:
        return static_cast<Opcode>(ExtendedOpcode::DoubleAdd);
      case Opcode::Sub:
        return static_cast<Opcode>(ExtendedOpcode::DoubleSub);
      case Opcode::Mul:
        return static_cast<Opcode>(ExtendedOpcode::DoubleMul);
      case Opcode::Div:
        // 0除算のチェックが必要
        if (rhsType.range.hasLowerBound && rhsType.range.hasUpperBound &&
            ((rhsType.range.lowerBound > 0 && rhsType.range.upperBound > 0) ||
             (rhsType.range.lowerBound < 0 && rhsType.range.upperBound < 0))) {
          return static_cast<Opcode>(ExtendedOpcode::DoubleDiv);
        }
        break;
      default:
        break;
    }
  }
  
  // 特化できない場合は、もとの命令コードを使用
  return std::nullopt;
}

std::optional<Opcode> TypeSpecializer::SpecializeComparisonOp(
    Opcode opcode, 
    const TypeInfo& lhsType, 
    const TypeInfo& rhsType) noexcept {
  // 実装の簡略化のため、常に元の命令コードを返す
  return std::nullopt;
}

void TypeSpecializer::Reset() noexcept {
  m_valueTypes.clear();
}

TypeInfo TypeSpecializer::InferType(
    const IRInstruction& inst, 
    const std::vector<TypeInfo>& operandTypes) noexcept {
  TypeInfo result;
  
  // 命令の種類に基づいて結果型を推論
  switch (inst.opcode) {
    case Opcode::LoadConst:
      // 定数値の型はロードする値に依存
      if (!inst.args.empty()) {
        int32_t value = inst.args[0];
        result.type = JSValueType::Integer;
        result.range.hasLowerBound = result.range.hasUpperBound = true;
        result.range.lowerBound = result.range.upperBound = value;
      }
      break;
      
    case Opcode::Add:
    case Opcode::Sub:
    case Opcode::Mul:
    case Opcode::Div:
      // 算術演算の結果型
      if (operandTypes.size() >= 2) {
        const auto& lhs = operandTypes[0];
        const auto& rhs = operandTypes[1];
        
        if (lhs.type == JSValueType::Integer && rhs.type == JSValueType::Integer) {
          // 整数同士の演算は整数（除算を除く）
          if (inst.opcode != Opcode::Div) {
            result.type = JSValueType::Integer;
            
            // 範囲計算（オーバーフローは考慮しない簡略版）
            if (lhs.range.hasLowerBound && rhs.range.hasLowerBound) {
              result.range.hasLowerBound = true;
              
              switch (inst.opcode) {
                case Opcode::Add:
                  result.range.lowerBound = lhs.range.lowerBound + rhs.range.lowerBound;
                  break;
                case Opcode::Sub:
                  result.range.lowerBound = lhs.range.lowerBound - rhs.range.upperBound;
                  break;
                case Opcode::Mul:
                  // 符号に応じて異なる組み合わせが最小値になる
                  if (lhs.range.lowerBound >= 0 && rhs.range.lowerBound >= 0) {
                    result.range.lowerBound = lhs.range.lowerBound * rhs.range.lowerBound;
                  } else if (lhs.range.lowerBound < 0 && rhs.range.lowerBound < 0) {
                    result.range.lowerBound = lhs.range.lowerBound * rhs.range.lowerBound;
                  } else {
                    // 符号が異なる場合、最小値計算は複雑なので省略
                    result.range.hasLowerBound = false;
                  }
                  break;
                default:
                  break;
              }
            }
            
            if (lhs.range.hasUpperBound && rhs.range.hasUpperBound) {
              result.range.hasUpperBound = true;
              
              switch (inst.opcode) {
                case Opcode::Add:
                  result.range.upperBound = lhs.range.upperBound + rhs.range.upperBound;
                  break;
                case Opcode::Sub:
                  result.range.upperBound = lhs.range.upperBound - rhs.range.lowerBound;
                  break;
                case Opcode::Mul:
                  // 符号に応じて異なる組み合わせが最大値になる
                  if (lhs.range.upperBound >= 0 && rhs.range.upperBound >= 0) {
                    result.range.upperBound = lhs.range.upperBound * rhs.range.upperBound;
                  } else if (lhs.range.upperBound < 0 && rhs.range.upperBound < 0) {
                    result.range.upperBound = lhs.range.upperBound * rhs.range.upperBound;
                  } else {
                    // 符号が異なる場合、最大値計算は複雑なので省略
                    result.range.hasUpperBound = false;
                  }
                  break;
                default:
                  break;
              }
            }
          } else {
            // 整数除算は浮動小数点になる可能性
            result.type = JSValueType::Double;
          }
        } else if (lhs.type == JSValueType::Double || rhs.type == JSValueType::Double) {
          // 浮動小数点を含む演算は浮動小数点
          result.type = JSValueType::Double;
        } else {
          // その他の型の場合は未知
          result.type = JSValueType::Unknown;
        }
      }
      break;
      
    case Opcode::LoadVar:
      // 変数の型は現時点では不明
      result.type = JSValueType::Unknown;
      break;
      
    case Opcode::StoreVar:
      // 格納する値の型
      if (!operandTypes.empty()) {
        result = operandTypes[0];
      }
      break;
      
    case Opcode::Call:
      // 関数呼び出しの結果型は現時点では不明
      result.type = JSValueType::Unknown;
      break;
      
    case Opcode::Return:
      // 戻り値の型
      if (!operandTypes.empty()) {
        result = operandTypes[0];
      }
      break;
      
    default:
      // その他の命令は不明
      result.type = JSValueType::Unknown;
      break;
  }
  
  return result;
}

void TypeSpecializer::AnalyzeTypes(
    const std::vector<IRInstruction>& instructions, 
    uint32_t functionId) noexcept {
  // 命令ごとに結果型を推論し、m_valueTypes に格納
  for (size_t i = 0; i < instructions.size(); ++i) {
    const auto& inst = instructions[i];
    
    // オペランドの型情報を収集
    std::vector<TypeInfo> operandTypes;
    
    // オペランドの型は直前の命令の結果を利用
    // 実際のコンパイラでは正確なデータフロー解析が必要
    if (i >= 2 && (inst.opcode == Opcode::Add || 
                   inst.opcode == Opcode::Sub || 
                   inst.opcode == Opcode::Mul || 
                   inst.opcode == Opcode::Div)) {
      // 二項演算の場合、直前の2つの命令の結果型を使用
      auto it1 = m_valueTypes.find(i - 1);
      auto it2 = m_valueTypes.find(i - 2);
      
      if (it1 != m_valueTypes.end() && it2 != m_valueTypes.end()) {
        operandTypes.push_back(it2->second); // 左辺値
        operandTypes.push_back(it1->second); // 右辺値
      }
    } else if (i >= 1 && inst.opcode == Opcode::Return) {
      // 戻り値の場合、直前の命令の結果型を使用
      auto it = m_valueTypes.find(i - 1);
      if (it != m_valueTypes.end()) {
        operandTypes.push_back(it->second);
      }
    }
    
    // プロファイル情報がある場合は利用
    const ProfileData* profileData = ExecutionProfiler::Instance().GetProfileData(functionId);
    if (profileData && i < instructions.size() && !profileData->typeHistory.empty()) {
      // プロファイル情報の利用法はプロファイラの実装に依存
      // ここでは単純化のため、各命令にプロファイル情報が関連付けられていると仮定
      for (const auto& typeInfo : profileData->typeHistory) {
        if (typeInfo.typeId == i) { // 命令インデックスと型IDが一致すると仮定
          TypeInfo inferredType = InferTypeFromProfile(functionId, typeInfo.typeId);
          
          // 既存のオペランド型情報と結合
          if (!operandTypes.empty()) {
            for (auto& opType : operandTypes) {
              opType = opType.Merge(inferredType);
            }
          } else {
            operandTypes.push_back(inferredType);
          }
          
          break;
        }
      }
    }
    
    // 命令の結果型を推論
    TypeInfo resultType = InferType(inst, operandTypes);
    
    // 型情報をマップに保存
    m_valueTypes[i] = resultType;
  }
}

TypeInfo TypeSpecializer::InferTypeFromProfile(
    uint32_t functionId, 
    uint32_t typeId) noexcept {
  TypeInfo result;
  
  // プロファイル情報からの型推定
  // 実際の実装では、プロファイルデータから詳細な型情報を取得
  const ProfileData* profileData = ExecutionProfiler::Instance().GetProfileData(functionId);
  if (!profileData) {
    return result;
  }
  
  // プロファイル内の型履歴から該当する型情報を検索
  for (const auto& typeInfo : profileData->typeHistory) {
    if (typeInfo.typeId == typeId) {
      // 高頻度の型を推定（この例では、単純に typeId から型を決定）
      if (typeInfo.frequency > 100) {
        // typeId の上位16ビットが引数インデックス、下位16ビットが型番号と仮定
        uint32_t typeNum = typeId & 0xFFFF;
        
        // 型番号に基づいて JSValueType を設定（単純化のため1:1対応と仮定）
        if (typeNum < 20) {
          result.type = static_cast<JSValueType>(typeNum);
          
          // 数値型の場合は範囲も設定（例）
          if (result.type == JSValueType::Integer) {
            result.range.hasLowerBound = true;
            result.range.hasUpperBound = true;
            result.range.lowerBound = -1000;
            result.range.upperBound = 1000;
          } else if (result.type == JSValueType::Double) {
            result.range.hasLowerBound = true;
            result.range.hasUpperBound = true;
            result.range.lowerBound = -1000.0;
            result.range.upperBound = 1000.0;
          }
        }
      }
      
      break;
    }
  }
  
  return result;
}

IRInstruction TypeSpecializer::CreateOptimizedInstruction(
    const IRInstruction& original,
    const std::vector<TypeInfo>& operandTypes) noexcept {
  IRInstruction optimized = original;
  
  // 算術演算の最適化
  if (original.opcode == Opcode::Add || 
      original.opcode == Opcode::Sub || 
      original.opcode == Opcode::Mul || 
      original.opcode == Opcode::Div) {
      
    if (operandTypes.size() >= 2) {
      const auto& lhs = operandTypes[0];
      const auto& rhs = operandTypes[1];
      
      auto specializedOp = SpecializeArithmeticOp(original.opcode, lhs, rhs);
      if (specializedOp) {
        optimized.opcode = *specializedOp;
      }
    }
  }
  
  // 拡張実装では以下の最適化も可能：
  // - 比較演算の最適化
  // - プロパティアクセスの最適化
  // - 文字列操作の最適化
  // - 配列操作の最適化
  
  return optimized;
}

bool TypeSpecializer::AddTypeGuard(IRFunction* function, uint32_t bytecodeOffset, TypeFeedbackRecord::TypeCategory expectedType)
{
    if (!function)
        return false;
        
    // バイトコードオフセットに対応するIR命令を検索
    int64_t irIndex = GetIRIndexForBytecodeOffset(bytecodeOffset);
    if (irIndex < 0)
        return false;
        
    // 対応するIR命令を取得
    const auto& inst = function->GetInstructions()[static_cast<size_t>(irIndex)];
    
    // 型チェック対象のレジスタを判断
    int32_t targetReg = -1;
    switch (inst.opcode) {
    case Opcode::Add:
    case Opcode::Sub:
    case Opcode::Mul:
    case Opcode::Div:
    case Opcode::Equal:
    case Opcode::NotEqual:
    case Opcode::LessThan:
    case Opcode::LessThanEqual:
    case Opcode::GreaterThan:
    case Opcode::GreaterThanEqual:
        // 二項演算の場合は両方のソースに対してガードを挿入
        targetReg = inst.args[1]; // 第1引数
        break;
        
    case Opcode::LoadProperty:
    case Opcode::StoreProperty:
        // プロパティアクセスの場合はオブジェクトに対してガードを挿入
        targetReg = inst.args[1]; // オブジェクトレジスタ
        break;
        
    case Opcode::Call:
        // 関数呼び出しの場合は関数オブジェクトに対してガードを挿入
        targetReg = inst.args[1]; // 関数レジスタ
        break;
        
    default:
        // その他の命令では型ガードは挿入しない
        return false;
    }
    
    if (targetReg < 0)
        return false;
        
    // 型チェック命令を挿入
    size_t checkIndex = InsertTypeCheck(function, targetReg, expectedType);
    
    // ガード命令を追跡
    m_guardedInstructions.push_back(checkIndex);
    m_guardCount++;
    
    // レジスタの型情報を記録
    m_regTypeMap[targetReg] = expectedType;
    
    return true;
}

size_t TypeSpecializer::SpecializeInstruction(IRFunction* function, const IRInstruction& inst, 
                                           const std::unordered_map<int32_t, TypeFeedbackRecord::TypeCategory>& typeMap)
{
    if (!function)
        return static_cast<size_t>(-1);
        
    // 型情報を使用して命令を特化
    m_specializationCount++;
    
    switch (inst.opcode) {
    case Opcode::Add: {
        // 加算命令の特化
        int32_t dest = inst.args[0];
        int32_t src1 = inst.args[1];
        int32_t src2 = inst.args[2];
        
        auto it1 = typeMap.find(src1);
        auto it2 = typeMap.find(src2);
        
        if (it1 != typeMap.end() && it2 != typeMap.end()) {
            return SpecializeAdd(function, dest, src1, src2, it1->second, it2->second);
        }
        break;
    }
    
    case Opcode::Sub: {
        // 減算命令の特化
        int32_t dest = inst.args[0];
        int32_t src1 = inst.args[1];
        int32_t src2 = inst.args[2];
        
        auto it1 = typeMap.find(src1);
        auto it2 = typeMap.find(src2);
        
        if (it1 != typeMap.end() && it2 != typeMap.end()) {
            return SpecializeSub(function, dest, src1, src2, it1->second, it2->second);
        }
        break;
    }
    
    case Opcode::Mul: {
        // 乗算命令の特化
        int32_t dest = inst.args[0];
        int32_t src1 = inst.args[1];
        int32_t src2 = inst.args[2];
        
        auto it1 = typeMap.find(src1);
        auto it2 = typeMap.find(src2);
        
        if (it1 != typeMap.end() && it2 != typeMap.end()) {
            return SpecializeMul(function, dest, src1, src2, it1->second, it2->second);
        }
        break;
    }
    
    case Opcode::Div: {
        // 除算命令の特化
        int32_t dest = inst.args[0];
        int32_t src1 = inst.args[1];
        int32_t src2 = inst.args[2];
        
        auto it1 = typeMap.find(src1);
        auto it2 = typeMap.find(src2);
        
        if (it1 != typeMap.end() && it2 != typeMap.end()) {
            return SpecializeDiv(function, dest, src1, src2, it1->second, it2->second);
        }
        break;
    }
    
    case Opcode::Equal:
    case Opcode::NotEqual:
    case Opcode::LessThan:
    case Opcode::LessThanEqual:
    case Opcode::GreaterThan:
    case Opcode::GreaterThanEqual: {
        // 比較命令の特化
        int32_t dest = inst.args[0];
        int32_t src1 = inst.args[1];
        int32_t src2 = inst.args[2];
        
        auto it1 = typeMap.find(src1);
        auto it2 = typeMap.find(src2);
        
        if (it1 != typeMap.end() && it2 != typeMap.end()) {
            return SpecializeCompare(function, inst.opcode, dest, src1, src2, it1->second, it2->second);
        }
        break;
    }
    
    default:
        // 特化が実装されていない命令の場合はそのまま追加
        function->AddInstruction(inst);
        return function->GetInstructions().size() - 1;
    }
    
    // 特化に失敗した場合は元の命令をそのまま追加
    function->AddInstruction(inst);
    return function->GetInstructions().size() - 1;
}

size_t TypeSpecializer::SpecializeAdd(IRFunction* function, int32_t dest, int32_t src1, int32_t src2,
                                   TypeFeedbackRecord::TypeCategory type1, TypeFeedbackRecord::TypeCategory type2)
{
    // 両方が整数の場合
    if (type1 == TypeFeedbackRecord::TypeCategory::Integer && 
        type2 == TypeFeedbackRecord::TypeCategory::Integer) {
        IRInstruction inst;
        inst.opcode = Opcode::AddInt;
        inst.args[0] = dest;
        inst.args[1] = src1;
        inst.args[2] = src2;
        function->AddInstruction(inst);
        return function->GetInstructions().size() - 1;
    }
    
    // 両方が浮動小数点の場合
    if (type1 == TypeFeedbackRecord::TypeCategory::Float && 
        type2 == TypeFeedbackRecord::TypeCategory::Float) {
        IRInstruction inst;
        inst.opcode = Opcode::AddFloat;
        inst.args[0] = dest;
        inst.args[1] = src1;
        inst.args[2] = src2;
        function->AddInstruction(inst);
        return function->GetInstructions().size() - 1;
    }
    
    // 少なくとも一方が文字列の場合
    if (type1 == TypeFeedbackRecord::TypeCategory::String || 
        type2 == TypeFeedbackRecord::TypeCategory::String) {
        IRInstruction inst;
        inst.opcode = Opcode::AddString;
        inst.args[0] = dest;
        inst.args[1] = src1;
        inst.args[2] = src2;
        function->AddInstruction(inst);
        return function->GetInstructions().size() - 1;
    }
    
    // その他の場合は通常の加算
    IRInstruction inst;
    inst.opcode = Opcode::Add;
    inst.args[0] = dest;
    inst.args[1] = src1;
    inst.args[2] = src2;
    function->AddInstruction(inst);
    return function->GetInstructions().size() - 1;
}

size_t TypeSpecializer::SpecializeSub(IRFunction* function, int32_t dest, int32_t src1, int32_t src2,
                                   TypeFeedbackRecord::TypeCategory type1, TypeFeedbackRecord::TypeCategory type2)
{
    // 両方が整数の場合
    if (type1 == TypeFeedbackRecord::TypeCategory::Integer && 
        type2 == TypeFeedbackRecord::TypeCategory::Integer) {
        IRInstruction inst;
        inst.opcode = Opcode::SubInt;
        inst.args[0] = dest;
        inst.args[1] = src1;
        inst.args[2] = src2;
        function->AddInstruction(inst);
        return function->GetInstructions().size() - 1;
    }
    
    // 両方が浮動小数点の場合
    if (type1 == TypeFeedbackRecord::TypeCategory::Float && 
        type2 == TypeFeedbackRecord::TypeCategory::Float) {
        IRInstruction inst;
        inst.opcode = Opcode::SubFloat;
        inst.args[0] = dest;
        inst.args[1] = src1;
        inst.args[2] = src2;
        function->AddInstruction(inst);
        return function->GetInstructions().size() - 1;
    }
    
    // その他の場合は通常の減算
    IRInstruction inst;
    inst.opcode = Opcode::Sub;
    inst.args[0] = dest;
    inst.args[1] = src1;
    inst.args[2] = src2;
    function->AddInstruction(inst);
    return function->GetInstructions().size() - 1;
}

size_t TypeSpecializer::SpecializeMul(IRFunction* function, int32_t dest, int32_t src1, int32_t src2,
                                   TypeFeedbackRecord::TypeCategory type1, TypeFeedbackRecord::TypeCategory type2)
{
    // 両方が整数の場合
    if (type1 == TypeFeedbackRecord::TypeCategory::Integer && 
        type2 == TypeFeedbackRecord::TypeCategory::Integer) {
        IRInstruction inst;
        inst.opcode = Opcode::MulInt;
        inst.args[0] = dest;
        inst.args[1] = src1;
        inst.args[2] = src2;
        function->AddInstruction(inst);
        return function->GetInstructions().size() - 1;
    }
    
    // 両方が浮動小数点の場合
    if (type1 == TypeFeedbackRecord::TypeCategory::Float && 
        type2 == TypeFeedbackRecord::TypeCategory::Float) {
        IRInstruction inst;
        inst.opcode = Opcode::MulFloat;
        inst.args[0] = dest;
        inst.args[1] = src1;
        inst.args[2] = src2;
        function->AddInstruction(inst);
        return function->GetInstructions().size() - 1;
    }
    
    // その他の場合は通常の乗算
    IRInstruction inst;
    inst.opcode = Opcode::Mul;
    inst.args[0] = dest;
    inst.args[1] = src1;
    inst.args[2] = src2;
    function->AddInstruction(inst);
    return function->GetInstructions().size() - 1;
}

size_t TypeSpecializer::SpecializeDiv(IRFunction* function, int32_t dest, int32_t src1, int32_t src2,
                                   TypeFeedbackRecord::TypeCategory type1, TypeFeedbackRecord::TypeCategory type2)
{
    // 両方が整数の場合
    if (type1 == TypeFeedbackRecord::TypeCategory::Integer && 
        type2 == TypeFeedbackRecord::TypeCategory::Integer) {
        IRInstruction inst;
        inst.opcode = Opcode::DivInt;
        inst.args[0] = dest;
        inst.args[1] = src1;
        inst.args[2] = src2;
        function->AddInstruction(inst);
        return function->GetInstructions().size() - 1;
    }
    
    // 両方が浮動小数点の場合
    if (type1 == TypeFeedbackRecord::TypeCategory::Float && 
        type2 == TypeFeedbackRecord::TypeCategory::Float) {
        IRInstruction inst;
        inst.opcode = Opcode::DivFloat;
        inst.args[0] = dest;
        inst.args[1] = src1;
        inst.args[2] = src2;
        function->AddInstruction(inst);
        return function->GetInstructions().size() - 1;
    }
    
    // その他の場合は通常の除算
    IRInstruction inst;
    inst.opcode = Opcode::Div;
    inst.args[0] = dest;
    inst.args[1] = src1;
    inst.args[2] = src2;
    function->AddInstruction(inst);
    return function->GetInstructions().size() - 1;
}

size_t TypeSpecializer::SpecializeCompare(IRFunction* function, Opcode opcode, int32_t dest, int32_t src1, int32_t src2,
                                       TypeFeedbackRecord::TypeCategory type1, TypeFeedbackRecord::TypeCategory type2)
{
    // 両方が整数の場合
    if (type1 == TypeFeedbackRecord::TypeCategory::Integer && 
        type2 == TypeFeedbackRecord::TypeCategory::Integer) {
        Opcode specializedOp;
        
        switch (opcode) {
        case Opcode::Equal:
            specializedOp = Opcode::EqualInt;
            break;
        case Opcode::NotEqual:
            specializedOp = Opcode::NotEqualInt;
            break;
        case Opcode::LessThan:
            specializedOp = Opcode::LessThanInt;
            break;
        case Opcode::LessThanEqual:
            specializedOp = Opcode::LessThanEqualInt;
            break;
        case Opcode::GreaterThan:
            specializedOp = Opcode::GreaterThanInt;
            break;
        case Opcode::GreaterThanEqual:
            specializedOp = Opcode::GreaterThanEqualInt;
            break;
        default:
            specializedOp = opcode;
        }
        
        IRInstruction inst;
        inst.opcode = specializedOp;
        inst.args[0] = dest;
        inst.args[1] = src1;
        inst.args[2] = src2;
        function->AddInstruction(inst);
        return function->GetInstructions().size() - 1;
    }
    
    // 両方が浮動小数点の場合
    if (type1 == TypeFeedbackRecord::TypeCategory::Float && 
        type2 == TypeFeedbackRecord::TypeCategory::Float) {
        Opcode specializedOp;
        
        switch (opcode) {
        case Opcode::Equal:
            specializedOp = Opcode::EqualFloat;
            break;
        case Opcode::NotEqual:
            specializedOp = Opcode::NotEqualFloat;
            break;
        case Opcode::LessThan:
            specializedOp = Opcode::LessThanFloat;
            break;
        case Opcode::LessThanEqual:
            specializedOp = Opcode::LessThanEqualFloat;
            break;
        case Opcode::GreaterThan:
            specializedOp = Opcode::GreaterThanFloat;
            break;
        case Opcode::GreaterThanEqual:
            specializedOp = Opcode::GreaterThanEqualFloat;
            break;
        default:
            specializedOp = opcode;
        }
        
        IRInstruction inst;
        inst.opcode = specializedOp;
        inst.args[0] = dest;
        inst.args[1] = src1;
        inst.args[2] = src2;
        function->AddInstruction(inst);
        return function->GetInstructions().size() - 1;
    }
    
    // 両方が文字列の場合
    if (type1 == TypeFeedbackRecord::TypeCategory::String && 
        type2 == TypeFeedbackRecord::TypeCategory::String) {
        Opcode specializedOp;
        
        switch (opcode) {
        case Opcode::Equal:
            specializedOp = Opcode::EqualString;
            break;
        case Opcode::NotEqual:
            specializedOp = Opcode::NotEqualString;
            break;
        case Opcode::LessThan:
            specializedOp = Opcode::LessThanString;
            break;
        case Opcode::LessThanEqual:
            specializedOp = Opcode::LessThanEqualString;
            break;
        case Opcode::GreaterThan:
            specializedOp = Opcode::GreaterThanString;
            break;
        case Opcode::GreaterThanEqual:
            specializedOp = Opcode::GreaterThanEqualString;
            break;
        default:
            specializedOp = opcode;
        }
        
        IRInstruction inst;
        inst.opcode = specializedOp;
        inst.args[0] = dest;
        inst.args[1] = src1;
        inst.args[2] = src2;
        function->AddInstruction(inst);
        return function->GetInstructions().size() - 1;
    }
    
    // その他の場合は通常の比較
    IRInstruction inst;
    inst.opcode = opcode;
    inst.args[0] = dest;
    inst.args[1] = src1;
    inst.args[2] = src2;
    function->AddInstruction(inst);
    return function->GetInstructions().size() - 1;
}

size_t TypeSpecializer::InsertTypeCheck(IRFunction* function, int32_t reg, TypeFeedbackRecord::TypeCategory expectedType)
{
    IRInstruction inst;
    
    switch (expectedType) {
    case TypeFeedbackRecord::TypeCategory::Integer:
        inst.opcode = Opcode::CheckInt;
        break;
    case TypeFeedbackRecord::TypeCategory::Float:
        inst.opcode = Opcode::CheckFloat;
        break;
    case TypeFeedbackRecord::TypeCategory::Boolean:
        inst.opcode = Opcode::CheckBoolean;
        break;
    case TypeFeedbackRecord::TypeCategory::String:
        inst.opcode = Opcode::CheckString;
        break;
    case TypeFeedbackRecord::TypeCategory::Object:
        inst.opcode = Opcode::CheckObject;
        break;
    case TypeFeedbackRecord::TypeCategory::Function:
        inst.opcode = Opcode::CheckFunction;
        break;
    case TypeFeedbackRecord::TypeCategory::Array:
        inst.opcode = Opcode::CheckArray;
        break;
    default:
        // 未知の型カテゴリの場合、型チェックを挿入しない
        return static_cast<size_t>(-1);
    }
    
    inst.args[0] = reg;  // チェック対象のレジスタ
    function->AddInstruction(inst);
    return function->GetInstructions().size() - 1;
}

size_t TypeSpecializer::InsertDeoptimizationTrigger(IRFunction* function, uint32_t bytecodeOffset, const std::string& reason)
{
    IRInstruction inst;
    inst.opcode = Opcode::Deoptimize;
    inst.args[0] = static_cast<int32_t>(bytecodeOffset);
    inst.args[1] = 0; // 理由コード（将来的に使用）
    
    function->AddInstruction(inst);
    m_deoptCount++;
    
    return function->GetInstructions().size() - 1;
}

size_t TypeSpecializer::InsertJumpToIntegerPath(IRFunction* function, const std::string& labelName)
{
    IRInstruction inst;
    inst.opcode = Opcode::JumpLabel;
    inst.stringArg = labelName;
    
    function->AddInstruction(inst);
    return function->GetInstructions().size() - 1;
}

size_t TypeSpecializer::InsertJumpToFloatPath(IRFunction* function, const std::string& labelName)
{
    IRInstruction inst;
    inst.opcode = Opcode::JumpLabel;
    inst.stringArg = labelName;
    
    function->AddInstruction(inst);
    return function->GetInstructions().size() - 1;
}

size_t TypeSpecializer::InsertJumpToStringPath(IRFunction* function, const std::string& labelName)
{
    IRInstruction inst;
    inst.opcode = Opcode::JumpLabel;
    inst.stringArg = labelName;
    
    function->AddInstruction(inst);
    return function->GetInstructions().size() - 1;
}

}  // namespace core
}  // namespace aerojs 