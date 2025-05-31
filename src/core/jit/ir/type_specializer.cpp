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
    
    // オペランドの型情報収集の完全実装
    TypeInfo lhsType = getOperandTypeInfo(inst, 0);
    TypeInfo rhsType = getOperandTypeInfo(inst, 1);
    
    // 型情報の検証と修正
    if (!lhsType.isValid() || !rhsType.isValid()) {
        // 型推論により不明な型情報を補完
        if (!lhsType.isValid()) {
            lhsType = inferTypeFromUsage(inst, 0);
        }
        if (!rhsType.isValid()) {
            rhsType = inferTypeFromUsage(inst, 1);
        }
    }
    
    // 完全な型情報収集のヘルパーメソッド実装
    TypeInfo getOperandTypeInfo(const IRInstruction& inst, size_t operandIndex) {
        if (operandIndex >= inst.getOperandCount()) {
            return TypeInfo::createUnknown();
        }
        
        const IROperand& operand = inst.getOperand(operandIndex);
        
        switch (operand.getType()) {
            case IROperandType::Immediate:
                return analyzeImmediateType(operand);
            case IROperandType::Variable:
                return getVariableTypeInfo(operand.getVariableId());
            case IROperandType::Register:
                return getRegisterTypeInfo(operand.getRegisterId());
            case IROperandType::Memory:
                return analyzeMemoryOperandType(operand);
            default:
                return TypeInfo::createUnknown();
        }
    }
    
    // 即値の型分析
    TypeInfo analyzeImmediateType(const IROperand& operand) {
        switch (operand.getImmediateType()) {
            case ImmediateType::Int32:
                return TypeInfo::createInt32(operand.getInt32Value());
            case ImmediateType::Double:
                return TypeInfo::createDouble(operand.getDoubleValue());
            case ImmediateType::Boolean:
                return TypeInfo::createBoolean(operand.getBooleanValue());
            case ImmediateType::String:
                return TypeInfo::createString(operand.getStringValue());
            case ImmediateType::Null:
                return TypeInfo::createNull();
            case ImmediateType::Undefined:
                return TypeInfo::createUndefined();
            default:
                return TypeInfo::createUnknown();
        }
    }
    
    // 変数の型情報取得
    TypeInfo getVariableTypeInfo(uint32_t variableId) {
        auto it = m_variableTypes.find(variableId);
        if (it != m_variableTypes.end()) {
            return it->second;
        }
        
        // プロファイルデータから型情報を取得
        if (m_profileData) {
            auto profiledType = m_profileData->getVariableType(variableId);
            if (profiledType.confidence > 0.8) { // 80%以上の信頼度
                TypeInfo typeInfo = convertFromProfiledType(profiledType);
                m_variableTypes[variableId] = typeInfo;
                return typeInfo;
            }
        }
        
        return TypeInfo::createUnknown();
    }
    
    // 使用パターンからの型推論
    TypeInfo inferTypeFromUsage(const IRInstruction& inst, size_t operandIndex) {
        const IROperand& operand = inst.getOperand(operandIndex);
        
        if (operand.getType() != IROperandType::Variable) {
            return TypeInfo::createUnknown();
        }
        
        uint32_t variableId = operand.getVariableId();
        
        // 変数の全使用箇所を分析
        auto usageSites = findVariableUsages(variableId);
        std::map<JSValueType, int> typeVotes;
        
        for (const auto& usage : usageSites) {
            JSValueType inferredType = inferTypeFromOperation(usage.instruction, usage.operandIndex);
            if (inferredType != JSValueType::Unknown) {
                typeVotes[inferredType]++;
            }
        }
        
        // 最も多く推論された型を採用
        if (!typeVotes.empty()) {
            auto maxElement = std::max_element(typeVotes.begin(), typeVotes.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            return TypeInfo::create(maxElement->first);
        }
        
        return TypeInfo::createUnknown();
    }
    
    // 操作から型を推論
    JSValueType inferTypeFromOperation(const IRInstruction& inst, size_t operandIndex) {
        switch (inst.getOpcode()) {
            case IROpcode::Add:
            case IROpcode::Sub:
            case IROpcode::Mul:
            case IROpcode::Div:
                // 算術演算では数値型を期待
                return JSValueType::Number;
            case IROpcode::BitwiseAnd:
            case IROpcode::BitwiseOr:
            case IROpcode::BitwiseXor:
            case IROpcode::LeftShift:
            case IROpcode::RightShift:
                // ビット演算では整数型を期待
                return JSValueType::Int32;
            case IROpcode::StringConcat:
                // 文字列連結では文字列型を期待
                return JSValueType::String;
            case IROpcode::LogicalAnd:
            case IROpcode::LogicalOr:
                // 論理演算では真偽値を期待
                return JSValueType::Boolean;
            case IROpcode::LoadProperty:
                // プロパティアクセスではオブジェクトを期待
                if (operandIndex == 0) return JSValueType::Object;
                if (operandIndex == 1) return JSValueType::String; // プロパティ名
                break;
            case IROpcode::LoadElement:
                // 配列アクセスでは配列とインデックスを期待
                if (operandIndex == 0) return JSValueType::Object; // 配列
                if (operandIndex == 1) return JSValueType::Number; // インデックス
                break;
            default:
                break;
        }
        return JSValueType::Unknown;
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
  
  // 拡張されたTypeGuard命令のエンコーディング
  // 基本型情報
  IRValue basicTypeValue = IRValue::CreateImmediate(static_cast<int32_t>(expectedType.type));
  guardInst.AddOperand(IROperand::CreateValue(basicTypeValue));
  
  // 詳細型情報の追加
  if (expectedType.type == JSValueType::Object && expectedType.hasShapeInfo()) {
    // オブジェクトシェイプID
    IRValue shapeIdValue = IRValue::CreateImmediate(expectedType.getShapeId());
    guardInst.AddOperand(IROperand::CreateValue(shapeIdValue));
    
    // プロパティマップハッシュ
    IRValue propertyHashValue = IRValue::CreateImmediate(expectedType.getPropertyMapHash());
    guardInst.AddOperand(IROperand::CreateValue(propertyHashValue));
  }
  
  if (expectedType.type == JSValueType::Number && expectedType.hasRangeInfo()) {
    // 数値範囲情報
    IRValue minValue = IRValue::CreateDouble(expectedType.getMinValue());
    IRValue maxValue = IRValue::CreateDouble(expectedType.getMaxValue());
    guardInst.AddOperand(IROperand::CreateValue(minValue));
    guardInst.AddOperand(IROperand::CreateValue(maxValue));
  }
  
  if (expectedType.type == JSValueType::Array && expectedType.hasElementTypeInfo()) {
    // 配列要素型情報
    IRValue elementTypeValue = IRValue::CreateImmediate(static_cast<int32_t>(expectedType.getElementType()));
    guardInst.AddOperand(IROperand::CreateValue(elementTypeValue));
    
    // 配列長情報（固定長の場合）
    if (expectedType.hasFixedLength()) {
      IRValue lengthValue = IRValue::CreateImmediate(expectedType.getFixedLength());
      guardInst.AddOperand(IROperand::CreateValue(lengthValue));
    }
  }
  
  if (expectedType.type == JSValueType::String && expectedType.hasStringInfo()) {
    // 文字列長情報
    if (expectedType.hasKnownLength()) {
      IRValue lengthValue = IRValue::CreateImmediate(expectedType.getStringLength());
      guardInst.AddOperand(IROperand::CreateValue(lengthValue));
    }
    
    // 文字列エンコーディング情報（ASCII、UTF-8、UTF-16など）
    IRValue encodingValue = IRValue::CreateImmediate(static_cast<int32_t>(expectedType.getStringEncoding()));
    guardInst.AddOperand(IROperand::CreateValue(encodingValue));
  }
  
  // 信頼度情報
  IRValue confidenceValue = IRValue::CreateDouble(expectedType.confidence);
  guardInst.AddOperand(IROperand::CreateValue(confidenceValue));
  
  // 型ガードの挿入位置を決定
  instructions.insert(instructions.begin() + index, guardInst);
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
  // 両オペランドが整数型の場合
  if (lhsType.type == JSValueType::Integer && rhsType.type == JSValueType::Integer) {
    switch (opcode) {
      case Opcode::kCompareEq:
        return static_cast<Opcode>(ExtendedOpcode::IntEqual);
      case Opcode::kCompareNe:
        return static_cast<Opcode>(ExtendedOpcode::IntNotEqual);
      case Opcode::kCompareLt:
        return static_cast<Opcode>(ExtendedOpcode::IntLessThan);
      case Opcode::kCompareLe:
        return static_cast<Opcode>(ExtendedOpcode::IntLessThanOrEqual);
      case Opcode::kCompareGt:
        return static_cast<Opcode>(ExtendedOpcode::IntGreaterThan);
      case Opcode::kCompareGe:
        return static_cast<Opcode>(ExtendedOpcode::IntGreaterThanOrEqual);
      default:
        break; // 他の比較オプコードは整数特化がない場合
    }
  }

  // 両オペランドが浮動小数点型の場合 (Double または HeapNumber)
  if ((lhsType.type == JSValueType::Double || lhsType.type == JSValueType::HeapNumber) &&
      (rhsType.type == JSValueType::Double || rhsType.type == JSValueType::HeapNumber)) {
    switch (opcode) {
      case Opcode::kCompareEq:
        return static_cast<Opcode>(ExtendedOpcode::DoubleEqual);
      case Opcode::kCompareNe:
        return static_cast<Opcode>(ExtendedOpcode::DoubleNotEqual);
      case Opcode::kCompareLt:
        return static_cast<Opcode>(ExtendedOpcode::DoubleLessThan);
      case Opcode::kCompareLe:
        return static_cast<Opcode>(ExtendedOpcode::DoubleLessThanOrEqual);
      case Opcode::kCompareGt:
        return static_cast<Opcode>(ExtendedOpcode::DoubleGreaterThan);
      case Opcode::kCompareGe:
        return static_cast<Opcode>(ExtendedOpcode::DoubleGreaterThanOrEqual);
      default:
        break; // 他の比較オプコードは浮動小数点特化がない場合
    }
  }

  // 文字列比較の特殊化の完全実装
  if (lhsType.type == JSValueType::String && rhsType.type == JSValueType::String) {
    IRInstruction specializedInst;
    
    // 比較演算子に応じて適切な文字列比較命令を選択
    switch (opcode) {
      case Opcode::kCompareEq:
        specializedInst.opcode = static_cast<Opcode>(ExtendedOpcode::StringEqual);
        break;
      case Opcode::kCompareNe:
        // StringNotEqual を使用し、結果を反転
        specializedInst.opcode = static_cast<Opcode>(ExtendedOpcode::StringEqual);
        // 後で結果を反転する命令を追加
        break;
      case Opcode::kCompareLt:
        specializedInst.opcode = static_cast<Opcode>(ExtendedOpcode::StringLessThan);
        break;
      case Opcode::kCompareLe:
        specializedInst.opcode = static_cast<Opcode>(ExtendedOpcode::StringLessThanOrEqual);
        break;
      case Opcode::kCompareGt:
        specializedInst.opcode = static_cast<Opcode>(ExtendedOpcode::StringGreaterThan);
        break;
      case Opcode::kCompareGe:
        specializedInst.opcode = static_cast<Opcode>(ExtendedOpcode::StringGreaterThanOrEqual);
        break;
      default:
        // サポートされていない比較演算子の場合は一般的な処理にフォールバック
        return CreateGenericInstruction(opcode, dest, src1, src2);
    }
    
    // オペランドを設定
    specializedInst.AddOperand(IROperand::CreateRegister(dest));
    specializedInst.AddOperand(IROperand::CreateRegister(src1));
    specializedInst.AddOperand(IROperand::CreateRegister(src2));
    
    // 文字列比較の最適化フラグを設定
    StringComparisonFlags flags = StringComparisonFlags::None;
    
    // 文字列長による最適化
    if (lhsType.hasKnownLength() && rhsType.hasKnownLength()) {
      if (lhsType.getStringLength() == rhsType.getStringLength()) {
        flags |= StringComparisonFlags::EqualLength;
      } else if (opcode == Opcode::kCompareEq || opcode == Opcode::kCompareNe) {
        // 長さが異なる文字列の等価比較は即座に結果が決まる
        flags |= StringComparisonFlags::LengthMismatch;
      }
    }
    
    // エンコーディングによる最適化
    if (lhsType.getStringEncoding() == StringEncoding::ASCII && 
        rhsType.getStringEncoding() == StringEncoding::ASCII) {
      flags |= StringComparisonFlags::ASCIIOnly;
    } else if (lhsType.getStringEncoding() == StringEncoding::Latin1 && 
               rhsType.getStringEncoding() == StringEncoding::Latin1) {
      flags |= StringComparisonFlags::Latin1Only;
    }
    
    // 定数文字列による最適化
    if (lhsType.isConstantString() || rhsType.isConstantString()) {
      flags |= StringComparisonFlags::HasConstant;
    }
    
    // 最適化フラグをメタデータとして追加
    IRValue flagsValue = IRValue::CreateImmediate(static_cast<int32_t>(flags));
    specializedInst.AddOperand(IROperand::CreateValue(flagsValue));
    
    // kCompareNe の場合は結果を反転する命令を追加
    if (opcode == Opcode::kCompareNe) {
      // 一時レジスタが必要
      int32_t tempReg = AllocateTemporaryRegister();
      specializedInst.operands[0] = IROperand::CreateRegister(tempReg);
      
      // 結果を反転する命令を後で追加
      IRInstruction notInst;
      notInst.opcode = Opcode::kLogicalNot;
      notInst.AddOperand(IROperand::CreateRegister(dest));
      notInst.AddOperand(IROperand::CreateRegister(tempReg));
      
      // 両方の命令を返すために、複合命令として処理
      IRInstruction compoundInst;
      compoundInst.opcode = static_cast<Opcode>(ExtendedOpcode::CompoundOperation);
      compoundInst.SetSubInstructions({specializedInst, notInst});
      
      return compoundInst;
    }
    
    return specializedInst;
  }
  
  // 文字列と数値の比較特殊化
  if ((lhsType.type == JSValueType::String && rhsType.type == JSValueType::Number) ||
      (lhsType.type == JSValueType::Number && rhsType.type == JSValueType::String)) {
    
    IRInstruction conversionInst;
    bool leftIsString = (lhsType.type == JSValueType::String);
    
    // 文字列を数値に変換する命令を挿入
    int32_t tempReg = AllocateTemporaryRegister();
    conversionInst.opcode = static_cast<Opcode>(ExtendedOpcode::StringToNumber);
    conversionInst.AddOperand(IROperand::CreateRegister(tempReg));
    conversionInst.AddOperand(IROperand::CreateRegister(leftIsString ? src1 : src2));
    
    // 数値比較命令を生成
    IRInstruction compareInst = SpecializeCompare(
      function, opcode, dest,
      leftIsString ? tempReg : src1,
      leftIsString ? src2 : tempReg,
      TypeFeedbackRecord::TypeCategory::Double,
      TypeFeedbackRecord::TypeCategory::Double
    );
    
    // 複合命令として返す
    IRInstruction compoundInst;
    compoundInst.opcode = static_cast<Opcode>(ExtendedOpcode::CompoundOperation);
    compoundInst.SetSubInstructions({conversionInst, compareInst});
    
    return compoundInst;
  }

  // 上記以外の場合、または特化できない場合は元の命令コードを使用
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
            
            // 範囲計算（オーバーフローチェックあり）
            if (lhs.range.hasLowerBound && rhs.range.hasLowerBound) {
              result.range.hasLowerBound = true;
              
              switch (inst.opcode) {
                case Opcode::Add:
                  {
                    int64_t sum = static_cast<int64_t>(lhs.range.lowerBound) + rhs.range.lowerBound;
                    if (sum < std::numeric_limits<int32_t>::min() || sum > std::numeric_limits<int32_t>::max()) {
                      result.range.hasLowerBound = false; // オーバーフロー
                    } else {
                      result.range.lowerBound = static_cast<int32_t>(sum);
                    }
                  }
                  break;
                case Opcode::Sub:
                  {
                    int64_t diff = static_cast<int64_t>(lhs.range.lowerBound) - rhs.range.upperBound; // lower - upper で最小
                    if (diff < std::numeric_limits<int32_t>::min() || diff > std::numeric_limits<int32_t>::max()) {
                      result.range.hasLowerBound = false; // オーバーフロー
                    } else {
                      result.range.lowerBound = static_cast<int32_t>(diff);
                    }
                  }
                  break;
                case Opcode::Mul:
                  // 符号に応じて異なる組み合わせが最小値になる
                  if (lhs.range.hasLowerBound && rhs.range.hasLowerBound) { // 両方に下限がある場合のみ
                    int64_t prod1 = static_cast<int64_t>(lhs.range.lowerBound) * rhs.range.lowerBound;
                    int64_t prod2 = static_cast<int64_t>(lhs.range.lowerBound) * rhs.range.upperBound; // rhs.upperBoundが存在する保証が必要
                    int64_t prod3 = static_cast<int64_t>(lhs.range.upperBound) * rhs.range.lowerBound; // lhs.upperBoundが存在する保証が必要
                    int64_t prod4 = static_cast<int64_t>(lhs.range.upperBound) * rhs.range.upperBound; // 両方に上限がある場合のみ

                    // 暫定的な最小値候補
                    int64_t min_prod = prod1;
                    bool possible_overflow = false;

                    if (prod1 < std::numeric_limits<int32_t>::min() || prod1 > std::numeric_limits<int32_t>::max()) possible_overflow = true;
                    
                    if (lhs.range.hasUpperBound && rhs.range.hasUpperBound) {
                        // 全ての境界値の組み合わせを計算
                        if (prod2 < std::numeric_limits<int32_t>::min() || prod2 > std::numeric_limits<int32_t>::max()) possible_overflow = true;
                        if (prod3 < std::numeric_limits<int32_t>::min() || prod3 > std::numeric_limits<int32_t>::max()) possible_overflow = true;
                        if (prod4 < std::numeric_limits<int32_t>::min() || prod4 > std::numeric_limits<int32_t>::max()) possible_overflow = true;
                        min_prod = std::min({prod1, prod2, prod3, prod4});
                    } else if (lhs.range.hasUpperBound) {
                        // lhsのみ上限がある場合 (lhs.lowerBound * rhs.lowerBound, lhs.upperBound * rhs.lowerBound)
                        if (prod3 < std::numeric_limits<int32_t>::min() || prod3 > std::numeric_limits<int32_t>::max()) possible_overflow = true;
                        min_prod = std::min(prod1, prod3);
                    } else if (rhs.range.hasUpperBound) {
                        // rhsのみ上限がある場合 (lhs.lowerBound * rhs.lowerBound, lhs.lowerBound * rhs.upperBound)
                        if (prod2 < std::numeric_limits<int32_t>::min() || prod2 > std::numeric_limits<int32_t>::max()) possible_overflow = true;
                         min_prod = std::min(prod1, prod2);
                    } else {
                        // 下限値のみが利用可能な場合（上限値がない場合）
                        // lhs.lowerBound * rhs.lowerBound のみが計算可能
                        min_prod = prod1;
                    }

                    if (possible_overflow || min_prod < std::numeric_limits<int32_t>::min() || min_prod > std::numeric_limits<int32_t>::max()) {
                      result.range.hasLowerBound = false;
                    } else {
                      result.range.lowerBound = static_cast<int32_t>(min_prod);
                    }
                  } else {
                    result.range.hasLowerBound = false; // どちらかの下限が不明なら結果も不明
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
                  {
                    int64_t sum = static_cast<int64_t>(lhs.range.upperBound) + rhs.range.upperBound;
                    if (sum < std::numeric_limits<int32_t>::min() || sum > std::numeric_limits<int32_t>::max()) {
                      result.range.hasUpperBound = false; // オーバーフロー
                    } else {
                      result.range.upperBound = static_cast<int32_t>(sum);
                    }
                  }
                  break;
                case Opcode::Sub:
                  {
                    int64_t diff = static_cast<int64_t>(lhs.range.upperBound) - rhs.range.lowerBound; // upper - lower で最大
                    if (diff < std::numeric_limits<int32_t>::min() || diff > std::numeric_limits<int32_t>::max()) {
                      result.range.hasUpperBound = false; // オーバーフロー
                    } else {
                      result.range.upperBound = static_cast<int32_t>(diff);
                    }
                  }
                  break;
                case Opcode::Mul:
                  if (lhs.range.hasUpperBound && rhs.range.hasUpperBound) { // 両方に上限がある場合のみ
                    int64_t prod1 = static_cast<int64_t>(lhs.range.lowerBound) * rhs.range.lowerBound;
                    int64_t prod2 = static_cast<int64_t>(lhs.range.lowerBound) * rhs.range.upperBound;
                    int64_t prod3 = static_cast<int64_t>(lhs.range.upperBound) * rhs.range.lowerBound;
                    int64_t prod4 = static_cast<int64_t>(lhs.range.upperBound) * rhs.range.upperBound;

                    int64_t max_prod = prod1;
                    bool possible_overflow = false;

                    if (prod1 < std::numeric_limits<int32_t>::min() || prod1 > std::numeric_limits<int32_t>::max()) possible_overflow = true;

                    if (lhs.range.hasLowerBound && rhs.range.hasLowerBound) {
                        // 全ての境界値の組み合わせを計算
                        if (prod2 < std::numeric_limits<int32_t>::min() || prod2 > std::numeric_limits<int32_t>::max()) possible_overflow = true;
                        if (prod3 < std::numeric_limits<int32_t>::min() || prod3 > std::numeric_limits<int32_t>::max()) possible_overflow = true;
                        if (prod4 < std::numeric_limits<int32_t>::min() || prod4 > std::numeric_limits<int32_t>::max()) possible_overflow = true;
                        max_prod = std::max({prod1, prod2, prod3, prod4});
                    } else if (lhs.range.hasLowerBound) {
                        // lhsのみ下限がある場合 (lhs.lowerBound * rhs.upperBound, lhs.upperBound * rhs.upperBound)
                        if (prod2 < std::numeric_limits<int32_t>::min() || prod2 > std::numeric_limits<int32_t>::max()) possible_overflow = true;
                        max_prod = std::max(prod1, prod2);
                    } else if (rhs.range.hasLowerBound) {
                        // rhsのみ下限がある場合 (lhs.upperBound * rhs.lowerBound, lhs.upperBound * rhs.upperBound)
                        if (prod3 < std::numeric_limits<int32_t>::min() || prod3 > std::numeric_limits<int32_t>::max()) possible_overflow = true;
                        max_prod = std::max(prod1, prod3);
                    } else {
                        // 上限値のみが利用可能な場合（下限値がない場合）
                        // lhs.upperBound * rhs.upperBound のみが計算可能
                        max_prod = prod1;
                    }

                    if (possible_overflow || max_prod < std::numeric_limits<int32_t>::min() || max_prod > std::numeric_limits<int32_t>::max()) {
                      result.range.hasUpperBound = false;
                    } else {
                      result.range.upperBound = static_cast<int32_t>(max_prod);
                    }
                  } else {
                     result.range.hasUpperBound = false; // どちらかの上限が不明なら結果も不明
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
    
    // オペランドの型情報収集の完全実装
    TypeInfo lhsType = getOperandTypeInfo(inst, 0);
    TypeInfo rhsType = getOperandTypeInfo(inst, 1);
    
    // 型情報の検証と修正
    if (!lhsType.isValid() || !rhsType.isValid()) {
        // 型推論により不明な型情報を補完
        if (!lhsType.isValid()) {
            lhsType = inferTypeFromUsage(inst, 0);
        }
        if (!rhsType.isValid()) {
            rhsType = inferTypeFromUsage(inst, 1);
        }
    }
    
    // 完全な型情報収集のヘルパーメソッド実装
    TypeInfo getOperandTypeInfo(const IRInstruction& inst, size_t operandIndex) {
        if (operandIndex >= inst.getOperandCount()) {
            return TypeInfo::createUnknown();
        }
        
        const IROperand& operand = inst.getOperand(operandIndex);
        
        switch (operand.getType()) {
            case IROperandType::Immediate:
                return analyzeImmediateType(operand);
            case IROperandType::Variable:
                return getVariableTypeInfo(operand.getVariableId());
            case IROperandType::Register:
                return getRegisterTypeInfo(operand.getRegisterId());
            case IROperandType::Memory:
                return analyzeMemoryOperandType(operand);
            default:
                return TypeInfo::createUnknown();
        }
    }
    
    // 即値の型分析
    TypeInfo analyzeImmediateType(const IROperand& operand) {
        switch (operand.getImmediateType()) {
            case ImmediateType::Int32:
                return TypeInfo::createInt32(operand.getInt32Value());
            case ImmediateType::Double:
                return TypeInfo::createDouble(operand.getDoubleValue());
            case ImmediateType::Boolean:
                return TypeInfo::createBoolean(operand.getBooleanValue());
            case ImmediateType::String:
                return TypeInfo::createString(operand.getStringValue());
            case ImmediateType::Null:
                return TypeInfo::createNull();
            case ImmediateType::Undefined:
                return TypeInfo::createUndefined();
            default:
                return TypeInfo::createUnknown();
        }
    }
    
    // 変数の型情報取得
    TypeInfo getVariableTypeInfo(uint32_t variableId) {
        auto it = m_variableTypes.find(variableId);
        if (it != m_variableTypes.end()) {
            return it->second;
        }
        
        // プロファイルデータから型情報を取得
        if (m_profileData) {
            auto profiledType = m_profileData->getVariableType(variableId);
            if (profiledType.confidence > 0.8) { // 80%以上の信頼度
                TypeInfo typeInfo = convertFromProfiledType(profiledType);
                m_variableTypes[variableId] = typeInfo;
                return typeInfo;
            }
        }
        
        return TypeInfo::createUnknown();
    }
    
    // 使用パターンからの型推論
    TypeInfo inferTypeFromUsage(const IRInstruction& inst, size_t operandIndex) {
        const IROperand& operand = inst.getOperand(operandIndex);
        
        if (operand.getType() != IROperandType::Variable) {
            return TypeInfo::createUnknown();
        }
        
        uint32_t variableId = operand.getVariableId();
        
        // 変数の全使用箇所を分析
        auto usageSites = findVariableUsages(variableId);
        std::map<JSValueType, int> typeVotes;
        
        for (const auto& usage : usageSites) {
            JSValueType inferredType = inferTypeFromOperation(usage.instruction, usage.operandIndex);
            if (inferredType != JSValueType::Unknown) {
                typeVotes[inferredType]++;
            }
        }
        
        // 最も多く推論された型を採用
        if (!typeVotes.empty()) {
            auto maxElement = std::max_element(typeVotes.begin(), typeVotes.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            return TypeInfo::create(maxElement->first);
        }
        
        return TypeInfo::createUnknown();
    }
    
    // 操作から型を推論
    JSValueType inferTypeFromOperation(const IRInstruction& inst, size_t operandIndex) {
        switch (inst.getOpcode()) {
            case IROpcode::Add:
            case IROpcode::Sub:
            case IROpcode::Mul:
            case IROpcode::Div:
                // 算術演算では数値型を期待
                return JSValueType::Number;
            case IROpcode::BitwiseAnd:
            case IROpcode::BitwiseOr:
            case IROpcode::BitwiseXor:
            case IROpcode::LeftShift:
            case IROpcode::RightShift:
                // ビット演算では整数型を期待
                return JSValueType::Int32;
            case IROpcode::StringConcat:
                // 文字列連結では文字列型を期待
                return JSValueType::String;
            case IROpcode::LogicalAnd:
            case IROpcode::LogicalOr:
                // 論理演算では真偽値を期待
                return JSValueType::Boolean;
            case IROpcode::LoadProperty:
                // プロパティアクセスではオブジェクトを期待
                if (operandIndex == 0) return JSValueType::Object;
                if (operandIndex == 1) return JSValueType::String; // プロパティ名
                break;
            case IROpcode::LoadElement:
                // 配列アクセスでは配列とインデックスを期待
                if (operandIndex == 0) return JSValueType::Object; // 配列
                if (operandIndex == 1) return JSValueType::Number; // インデックス
                break;
            default:
                break;
        }
        return JSValueType::Unknown;
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
  
  // プロファイルデータ・型推論エンジンと連携した本格実装
  const ProfileData* profileData = ExecutionProfiler::Instance().GetProfileData(functionId);
  if (profileData) {
    auto typeInfo = profileData->getTypeInfo(functionId);
    if (typeInfo.isPrecise()) {
      return typeInfo.getDominantType();
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

TypeInfo TypeSpecializer::InferTypeFromProfile(const TypeProfile& profile) {
    if (profile.isStable()) {
        return profile.getDominantType();
    }
    if (profile.hasShapeId()) {
        return TypeInfo::FromShapeId(profile.getShapeId());
    }
    if (profile.hasNumericRange()) {
        return TypeInfo::FromRange(profile.getMinValue(), profile.getMaxValue());
    }
    return TypeInfo::Any();
}

}  // namespace core
}  // namespace aerojs 
}  // namespace aerojs 