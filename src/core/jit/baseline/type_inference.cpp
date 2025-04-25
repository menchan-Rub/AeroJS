#include "type_inference.h"
#include <sstream>

namespace aerojs {
namespace core {

//=============================================================================
// TypeInfo の実装
//=============================================================================

TypeInfo::TypeInfo(ValueType type) noexcept
    : m_baseType(type)
    , m_hasIntConstant(false)
    , m_intConstantValue(0)
    , m_hasDoubleConstant(false)
    , m_doubleConstantValue(0.0)
    , m_hasStringConstant(false)
    , m_stringConstantValue("") {
}

ValueType TypeInfo::GetBaseType() const noexcept {
  return m_baseType;
}

void TypeInfo::SetBaseType(ValueType type) noexcept {
  m_baseType = type;
}

bool TypeInfo::HasIntConstantValue() const noexcept {
  return m_hasIntConstant;
}

int32_t TypeInfo::GetIntConstantValue() const noexcept {
  return m_intConstantValue;
}

void TypeInfo::SetIntConstantValue(int32_t value) noexcept {
  m_hasIntConstant = true;
  m_intConstantValue = value;
  
  // 整数が設定された場合、基本型も整数に設定
  m_baseType = ValueType::kInteger;
  
  // 他の定数値をクリア
  m_hasDoubleConstant = false;
  m_hasStringConstant = false;
}

bool TypeInfo::HasDoubleConstantValue() const noexcept {
  return m_hasDoubleConstant;
}

double TypeInfo::GetDoubleConstantValue() const noexcept {
  return m_doubleConstantValue;
}

void TypeInfo::SetDoubleConstantValue(double value) noexcept {
  m_hasDoubleConstant = true;
  m_doubleConstantValue = value;
  
  // 浮動小数点数が設定された場合、基本型も数値に設定
  m_baseType = ValueType::kNumber;
  
  // 他の定数値をクリア
  m_hasIntConstant = false;
  m_hasStringConstant = false;
}

bool TypeInfo::HasStringConstantValue() const noexcept {
  return m_hasStringConstant;
}

const std::string& TypeInfo::GetStringConstantValue() const noexcept {
  return m_stringConstantValue;
}

void TypeInfo::SetStringConstantValue(const std::string& value) noexcept {
  m_hasStringConstant = true;
  m_stringConstantValue = value;
  
  // 文字列が設定された場合、基本型も文字列に設定
  m_baseType = ValueType::kString;
  
  // 他の定数値をクリア
  m_hasIntConstant = false;
  m_hasDoubleConstant = false;
}

bool TypeInfo::IsUndefined() const noexcept {
  return m_baseType == ValueType::kUndefined;
}

bool TypeInfo::IsNull() const noexcept {
  return m_baseType == ValueType::kNull;
}

bool TypeInfo::IsBoolean() const noexcept {
  return m_baseType == ValueType::kBoolean;
}

bool TypeInfo::IsInteger() const noexcept {
  return m_baseType == ValueType::kInteger;
}

bool TypeInfo::IsNumber() const noexcept {
  return m_baseType == ValueType::kNumber || m_baseType == ValueType::kInteger;
}

bool TypeInfo::IsString() const noexcept {
  return m_baseType == ValueType::kString;
}

bool TypeInfo::IsFunction() const noexcept {
  return m_baseType == ValueType::kFunction;
}

bool TypeInfo::IsObject() const noexcept {
  return m_baseType == ValueType::kObject ||
         m_baseType == ValueType::kArray ||
         m_baseType == ValueType::kFunction ||
         m_baseType == ValueType::kDate ||
         m_baseType == ValueType::kRegExp ||
         m_baseType == ValueType::kMap ||
         m_baseType == ValueType::kSet ||
         m_baseType == ValueType::kPromise ||
         m_baseType == ValueType::kArrayBuffer ||
         m_baseType == ValueType::kTypedArray ||
         m_baseType == ValueType::kDataView;
}

TypeInfo TypeInfo::Merge(const TypeInfo& other) const noexcept {
  // 同じ型の場合は単純
  if (m_baseType == other.m_baseType) {
    TypeInfo result(m_baseType);
    
    // 定数値は両方が同じ場合のみ保持
    if (m_hasIntConstant && other.m_hasIntConstant &&
        m_intConstantValue == other.m_intConstantValue) {
      result.m_hasIntConstant = true;
      result.m_intConstantValue = m_intConstantValue;
    }
    
    if (m_hasDoubleConstant && other.m_hasDoubleConstant &&
        m_doubleConstantValue == other.m_doubleConstantValue) {
      result.m_hasDoubleConstant = true;
      result.m_doubleConstantValue = m_doubleConstantValue;
    }
    
    if (m_hasStringConstant && other.m_hasStringConstant &&
        m_stringConstantValue == other.m_stringConstantValue) {
      result.m_hasStringConstant = true;
      result.m_stringConstantValue = m_stringConstantValue;
    }
    
    return result;
  }
  
  // 両方が数値型の場合、より広い型を選択
  if ((m_baseType == ValueType::kInteger || m_baseType == ValueType::kNumber) &&
      (other.m_baseType == ValueType::kInteger || other.m_baseType == ValueType::kNumber)) {
    return TypeInfo(ValueType::kNumber);
  }
  
  // どちらかが不明の場合、もう一方の型を選択
  if (m_baseType == ValueType::kUnknown) {
    return other;
  }
  if (other.m_baseType == ValueType::kUnknown) {
    return *this;
  }
  
  // それ以外の場合、型の結合は不明として扱う
  return TypeInfo(ValueType::kUnknown);
}

std::string TypeInfo::ToString() const noexcept {
  std::ostringstream oss;
  oss << ValueTypeToString(m_baseType);
  
  if (m_hasIntConstant) {
    oss << "(" << m_intConstantValue << ")";
  } else if (m_hasDoubleConstant) {
    oss << "(" << m_doubleConstantValue << ")";
  } else if (m_hasStringConstant) {
    oss << "(\"" << m_stringConstantValue << "\")";
  }
  
  return oss.str();
}

//=============================================================================
// TypeInferenceEngine の実装
//=============================================================================

TypeInferenceEngine::TypeInferenceEngine() noexcept
    : m_maxIterations(10) {
}

TypeInferenceEngine::~TypeInferenceEngine() noexcept = default;

TypeInferenceResult TypeInferenceEngine::InferTypes(const IRFunction& function) noexcept {
  TypeInferenceResult result;
  
  // 既知のレジスタ型を初期化
  result.registerTypes = m_knownRegisterTypes;
  
  // 既知の変数型を初期化
  result.variableTypes = m_knownVariableTypes;
  
  // 命令の型情報を初期化
  const auto& instructions = function.GetInstructions();
  result.instructionTypes.resize(instructions.size(), TypeInfo(ValueType::kUnknown));
  
  // 型推論ループ
  bool changed = true;
  uint32_t iteration = 0;
  
  while (changed && iteration < m_maxIterations) {
    changed = false;
    iteration++;
    
    // 各命令を順番に処理
    for (size_t i = 0; i < instructions.size(); ++i) {
      const auto& inst = instructions[i];
      
      // 命令の型を推論
      bool instChanged = InferInstruction(inst, result);
      changed = changed || instChanged;
    }
  }
  
  return result;
}

void TypeInferenceEngine::SetRegisterType(uint32_t regId, const TypeInfo& type) noexcept {
  m_knownRegisterTypes[regId] = type;
}

void TypeInferenceEngine::SetVariableType(const std::string& varName, const TypeInfo& type) noexcept {
  m_knownVariableTypes[varName] = type;
}

void TypeInferenceEngine::SetMaxIterations(uint32_t count) noexcept {
  m_maxIterations = count;
}

void TypeInferenceEngine::Reset() noexcept {
  m_knownRegisterTypes.clear();
  m_knownVariableTypes.clear();
}

bool TypeInferenceEngine::InferInstruction(const IRInstruction& inst, TypeInferenceResult& result) noexcept {
  bool changed = false;
  TypeInfo resultType(ValueType::kUnknown);
  
  switch (inst.opcode) {
    case Opcode::LoadConst: {
      // LoadConstの場合、定数値に基づいて型を推論
      if (inst.args.size() >= 2) {
        uint32_t resultReg = inst.args[0];
        int32_t constValue = inst.args[1];
        
        // 整数定数を設定
        resultType.SetIntConstantValue(constValue);
        
        // レジスタ型を更新
        auto it = result.registerTypes.find(resultReg);
        if (it == result.registerTypes.end() || !(it->second.IsInteger() && 
            it->second.HasIntConstantValue() && 
            it->second.GetIntConstantValue() == constValue)) {
          result.registerTypes[resultReg] = resultType;
          changed = true;
        }
      }
      break;
    }
    
    case Opcode::LoadVar: {
      // LoadVarの場合、ソースレジスタの型を結果レジスタにコピー
      if (inst.args.size() >= 2) {
        uint32_t resultReg = inst.args[0];
        uint32_t sourceReg = inst.args[1];
        
        auto it = result.registerTypes.find(sourceReg);
        if (it != result.registerTypes.end()) {
          resultType = it->second;
          
          // レジスタ型を更新
          auto destIt = result.registerTypes.find(resultReg);
          if (destIt == result.registerTypes.end() || !(destIt->second.GetBaseType() == resultType.GetBaseType())) {
            result.registerTypes[resultReg] = resultType;
            changed = true;
          }
        }
      }
      break;
    }
    
    case Opcode::StoreVar: {
      // StoreVarの場合、ソースレジスタの型を変数にコピー
      if (inst.args.size() >= 2) {
        // この実装では、変数名情報がないため、実際には何もしない
        // 実際の実装では、変数名テーブルなどから変数名を取得する必要がある
      }
      break;
    }
    
    case Opcode::Add:
    case Opcode::Sub:
    case Opcode::Mul:
    case Opcode::Div: {
      // 算術演算の場合、オペランドの型に基づいて結果型を推論
      if (inst.args.size() >= 3) {
        uint32_t resultReg = inst.args[0];
        uint32_t op1Reg = inst.args[1];
        uint32_t op2Reg = inst.args[2];
        
        TypeInfo op1Type(ValueType::kUnknown);
        TypeInfo op2Type(ValueType::kUnknown);
        
        // オペランドの型を取得
        auto it1 = result.registerTypes.find(op1Reg);
        if (it1 != result.registerTypes.end()) {
          op1Type = it1->second;
        }
        
        auto it2 = result.registerTypes.find(op2Reg);
        if (it2 != result.registerTypes.end()) {
          op2Type = it2->second;
        }
        
        // 算術演算の型推論
        resultType = InferArithmeticOp(inst.opcode, resultReg, op1Type, op2Type, result);
        
        // レジスタ型を更新
        auto destIt = result.registerTypes.find(resultReg);
        if (destIt == result.registerTypes.end() || !(destIt->second.GetBaseType() == resultType.GetBaseType())) {
          result.registerTypes[resultReg] = resultType;
          changed = true;
        }
      }
      break;
    }
    
    case Opcode::Call: {
      // 関数呼び出しの場合、関数の戻り値型に基づいて結果型を推論
      // この実装では単純化のため、すべての関数呼び出しの結果型を不明とする
      if (inst.args.size() >= 2) {
        uint32_t resultReg = inst.args[0];
        resultType = TypeInfo(ValueType::kUnknown);
        
        // レジスタ型を更新
        auto destIt = result.registerTypes.find(resultReg);
        if (destIt == result.registerTypes.end() || !(destIt->second.GetBaseType() == resultType.GetBaseType())) {
          result.registerTypes[resultReg] = resultType;
          changed = true;
        }
      }
      break;
    }
    
    case Opcode::Return: {
      // 戻り値の型は特に推論しない
      break;
    }
    
    case Opcode::Nop: {
      // 何もしない
      break;
    }
    
    default:
      // サポートされていない命令
      break;
  }
  
  return changed;
}

TypeInfo TypeInferenceEngine::InferArithmeticOp(Opcode opcode, uint32_t resultReg, 
                                              const TypeInfo& op1Type, 
                                              const TypeInfo& op2Type,
                                              TypeInferenceResult& result) noexcept {
  // どちらかのオペランドが不明な場合、結果も不明
  if (op1Type.GetBaseType() == ValueType::kUnknown || 
      op2Type.GetBaseType() == ValueType::kUnknown) {
    return TypeInfo(ValueType::kUnknown);
  }
  
  // 加算演算子の特殊ケース：文字列結合
  if (opcode == Opcode::Add && 
      (op1Type.IsString() || op2Type.IsString())) {
    
    // 両方が文字列定数の場合、結果も文字列定数
    if (op1Type.HasStringConstantValue() && op2Type.HasStringConstantValue()) {
      TypeInfo resultType(ValueType::kString);
      resultType.SetStringConstantValue(
          op1Type.GetStringConstantValue() + op2Type.GetStringConstantValue());
      return resultType;
    }
    
    // それ以外の場合は文字列型（定数ではない）
    return TypeInfo(ValueType::kString);
  }
  
  // 数値演算
  if (op1Type.IsNumber() && op2Type.IsNumber()) {
    // 両方が整数定数の場合、コンパイル時に計算
    if (op1Type.HasIntConstantValue() && op2Type.HasIntConstantValue()) {
      return EvaluateIntegerOperation(opcode, 
                                     op1Type.GetIntConstantValue(), 
                                     op2Type.GetIntConstantValue());
    }
    
    // 両方が浮動小数点定数の場合、コンパイル時に計算
    if (op1Type.HasDoubleConstantValue() && op2Type.HasDoubleConstantValue()) {
      return EvaluateDoubleOperation(opcode, 
                                    op1Type.GetDoubleConstantValue(), 
                                    op2Type.GetDoubleConstantValue());
    }
    
    // 整数演算の場合
    if (op1Type.IsInteger() && op2Type.IsInteger() && 
        opcode != Opcode::Div) {  // 除算は常に浮動小数点
      return TypeInfo(ValueType::kInteger);
    }
    
    // それ以外の数値演算は浮動小数点
    return TypeInfo(ValueType::kNumber);
  }
  
  // サポートされていない型の組み合わせ
  return TypeInfo(ValueType::kUnknown);
}

TypeInfo TypeInferenceEngine::EvaluateIntegerOperation(Opcode opcode, int32_t val1, int32_t val2) noexcept {
  TypeInfo result;
  
  switch (opcode) {
    case Opcode::Add:
      result.SetIntConstantValue(val1 + val2);
      break;
      
    case Opcode::Sub:
      result.SetIntConstantValue(val1 - val2);
      break;
      
    case Opcode::Mul:
      result.SetIntConstantValue(val1 * val2);
      break;
      
    case Opcode::Div:
      // 整数除算の結果が整数になるかどうかをチェック
      if (val2 != 0 && val1 % val2 == 0) {
        result.SetIntConstantValue(val1 / val2);
      } else {
        // 結果が整数にならない場合は浮動小数点数
        result.SetDoubleConstantValue(static_cast<double>(val1) / val2);
      }
      break;
      
    default:
      // サポートされていない演算
      result.SetBaseType(ValueType::kUnknown);
      break;
  }
  
  return result;
}

TypeInfo TypeInferenceEngine::EvaluateDoubleOperation(Opcode opcode, double val1, double val2) noexcept {
  TypeInfo result;
  
  switch (opcode) {
    case Opcode::Add:
      result.SetDoubleConstantValue(val1 + val2);
      break;
      
    case Opcode::Sub:
      result.SetDoubleConstantValue(val1 - val2);
      break;
      
    case Opcode::Mul:
      result.SetDoubleConstantValue(val1 * val2);
      break;
      
    case Opcode::Div:
      result.SetDoubleConstantValue(val1 / val2);
      break;
      
    default:
      // サポートされていない演算
      result.SetBaseType(ValueType::kUnknown);
      break;
  }
  
  return result;
}

std::string ValueTypeToString(ValueType type) {
  switch (type) {
    case ValueType::kUnknown:    return "不明";
    case ValueType::kUndefined:  return "undefined";
    case ValueType::kNull:       return "null";
    case ValueType::kBoolean:    return "真偽値";
    case ValueType::kInteger:    return "整数";
    case ValueType::kNumber:     return "数値";
    case ValueType::kString:     return "文字列";
    case ValueType::kObject:     return "オブジェクト";
    case ValueType::kArray:      return "配列";
    case ValueType::kFunction:   return "関数";
    case ValueType::kSymbol:     return "シンボル";
    case ValueType::kBigInt:     return "BigInt";
    case ValueType::kDate:       return "Date";
    case ValueType::kRegExp:     return "正規表現";
    case ValueType::kMap:        return "Map";
    case ValueType::kSet:        return "Set";
    case ValueType::kPromise:    return "Promise";
    case ValueType::kArrayBuffer: return "ArrayBuffer";
    case ValueType::kTypedArray: return "TypedArray";
    case ValueType::kDataView:   return "DataView";
    default:                    return "不明な型";
  }
}

} // namespace core
} // namespace aerojs 