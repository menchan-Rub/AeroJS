/**
 * @file constant_folding.cpp
 * @brief AeroJS JavaScript エンジンの定数畳み込み最適化の実装
 * @version 1.0.0
 * @license MIT
 */

#include "constant_folding.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <unordered_set>

namespace aerojs {
namespace core {
namespace jit {

int32_t ConstantFolding::Run(IRFunction& func) {
  int32_t foldCount = 0;
  std::vector<IRInstruction> newInstructions;
  
  // IRFunction内の全ての命令をスキャン
  for (const auto& inst : func.instructions()) {
    if (IsConstantExpression(inst)) {
      // 定数式の場合は畳み込み
      IRInstruction folded = FoldConstantExpression(inst);
      newInstructions.push_back(folded);
      foldCount++;
    } else {
      // 通常の命令はそのまま
      newInstructions.push_back(inst);
    }
  }
  
  // 命令リストを更新
  func.Clear();
  for (const auto& inst : newInstructions) {
    func.AddInstruction(inst);
  }
  
  return foldCount;
}

bool ConstantFolding::IsConstantExpression(const IRInstruction& inst) {
  // 単項演算子
  if (inst.IsSingleOperandOp()) {
    return inst.GetOperand(0).IsConstant();
  }
  
  // 二項演算子
  if (inst.IsBinaryOp()) {
    return inst.GetOperand(0).IsConstant() && inst.GetOperand(1).IsConstant();
  }

  // 特殊ケース: Moveの場合はソースが定数であればよい
  if (inst.GetOpcode() == IROpcode::Move) {
    return inst.GetOperand(0).IsConstant();
  }
  
  return false;
}

IRInstruction ConstantFolding::FoldConstantExpression(const IRInstruction& inst) {
  switch (inst.GetOpcode()) {
    // 算術演算
    case IROpcode::Add:
    case IROpcode::Sub:
    case IROpcode::Mul:
    case IROpcode::Div:
    case IROpcode::Mod:
    case IROpcode::Neg:
      return FoldArithmeticOp(inst);
    
    // 比較演算
    case IROpcode::Eq:
    case IROpcode::Ne:
    case IROpcode::Lt:
    case IROpcode::Le:
    case IROpcode::Gt:
    case IROpcode::Ge:
      return FoldComparisonOp(inst);
    
    // 論理演算
    case IROpcode::And:
    case IROpcode::Or:
    case IROpcode::Not:
      return FoldLogicalOp(inst);
    
    // ビット演算
    case IROpcode::BitAnd:
    case IROpcode::BitOr:
    case IROpcode::BitXor:
    case IROpcode::BitNot:
    case IROpcode::Shl:
    case IROpcode::Shr:
    case IROpcode::UShr:
      return FoldBitwiseOp(inst);
    
    // Move命令
    case IROpcode::Move:
      // 単純にソースの定数を返す
      return IRInstruction::CreateConstant(
          inst.GetDest(), 
          GetConstantValue(inst.GetOperand(0)));
    
    // その他の命令（未対応）
    default:
      return inst;
  }
}

IRInstruction ConstantFolding::FoldArithmeticOp(const IRInstruction& inst) {
  IROpcode op = inst.GetOpcode();
  IRRegister dest = inst.GetDest();
  
  // 単項演算子（Neg）
  if (op == IROpcode::Neg) {
    IRConstant val = GetConstantValue(inst.GetOperand(0));
    
    if (val.IsInt32()) {
      int32_t result = -val.AsInt32();
      return IRInstruction::CreateConstInt32(dest, result);
    } else {
      double result = -val.AsDouble();
      return IRInstruction::CreateConstDouble(dest, result);
    }
  }
  
  // 二項演算子
  IRConstant left = GetConstantValue(inst.GetOperand(0));
  IRConstant right = GetConstantValue(inst.GetOperand(1));
  
  // 整数演算の場合
  if (left.IsInt32() && right.IsInt32()) {
    int32_t a = left.AsInt32();
    int32_t b = right.AsInt32();
    int32_t result = 0;
    
    switch (op) {
      case IROpcode::Add:
        result = a + b;
        break;
      case IROpcode::Sub:
        result = a - b;
        break;
      case IROpcode::Mul:
        result = a * b;
        break;
      case IROpcode::Div:
        // ゼロ除算チェック
        if (b == 0) {
          // JavaScriptの仕様に合わせて、Infinity/-Infinityを返す
          double divResult = (a >= 0) ? std::numeric_limits<double>::infinity() 
                                      : -std::numeric_limits<double>::infinity();
          return IRInstruction::CreateConstDouble(dest, divResult);
        }
        result = a / b;
        break;
      case IROpcode::Mod:
        // ゼロによる剰余チェック
        if (b == 0) {
          return IRInstruction::CreateConstDouble(dest, std::numeric_limits<double>::quiet_NaN());
        }
        result = a % b;
        break;
      default:
        return inst;
    }
    
    return IRInstruction::CreateConstInt32(dest, result);
  }
  
  // 浮動小数点演算
  double a = left.IsInt32() ? static_cast<double>(left.AsInt32()) : left.AsDouble();
  double b = right.IsInt32() ? static_cast<double>(right.AsInt32()) : right.AsDouble();
  double result = 0.0;
  
  switch (op) {
    case IROpcode::Add:
      result = a + b;
      break;
    case IROpcode::Sub:
      result = a - b;
      break;
    case IROpcode::Mul:
      result = a * b;
      break;
    case IROpcode::Div:
      result = a / b;
      break;
    case IROpcode::Mod:
      result = std::fmod(a, b);
      break;
    default:
      return inst;
  }
  
  return IRInstruction::CreateConstDouble(dest, result);
}

IRInstruction ConstantFolding::FoldComparisonOp(const IRInstruction& inst) {
  IROpcode op = inst.GetOpcode();
  IRRegister dest = inst.GetDest();
  
  IRConstant left = GetConstantValue(inst.GetOperand(0));
  IRConstant right = GetConstantValue(inst.GetOperand(1));
  
  // 整数比較
  if (left.IsInt32() && right.IsInt32()) {
    int32_t a = left.AsInt32();
    int32_t b = right.AsInt32();
    bool result = false;
    
    switch (op) {
      case IROpcode::Eq:
        result = (a == b);
        break;
      case IROpcode::Ne:
        result = (a != b);
        break;
      case IROpcode::Lt:
        result = (a < b);
        break;
      case IROpcode::Le:
        result = (a <= b);
        break;
      case IROpcode::Gt:
        result = (a > b);
        break;
      case IROpcode::Ge:
        result = (a >= b);
        break;
      default:
        return inst;
    }
    
    return IRInstruction::CreateConstBool(dest, result);
  }
  
  // 浮動小数点比較
  double a = left.IsInt32() ? static_cast<double>(left.AsInt32()) : left.AsDouble();
  double b = right.IsInt32() ? static_cast<double>(right.AsInt32()) : right.AsDouble();
  bool result = false;
  
  switch (op) {
    case IROpcode::Eq:
      result = (a == b);
      break;
    case IROpcode::Ne:
      result = (a != b);
      break;
    case IROpcode::Lt:
      result = (a < b);
      break;
    case IROpcode::Le:
      result = (a <= b);
      break;
    case IROpcode::Gt:
      result = (a > b);
      break;
    case IROpcode::Ge:
      result = (a >= b);
      break;
    default:
      return inst;
  }
  
  return IRInstruction::CreateConstBool(dest, result);
}

IRInstruction ConstantFolding::FoldLogicalOp(const IRInstruction& inst) {
  IROpcode op = inst.GetOpcode();
  IRRegister dest = inst.GetDest();
  
  // 単項演算子（Not）
  if (op == IROpcode::Not) {
    IRConstant val = GetConstantValue(inst.GetOperand(0));
    bool result = !val.AsBoolean();
    return IRInstruction::CreateConstBool(dest, result);
  }
  
  // 二項演算子
  IRConstant left = GetConstantValue(inst.GetOperand(0));
  IRConstant right = GetConstantValue(inst.GetOperand(1));
  
  bool a = left.AsBoolean();
  bool b = right.AsBoolean();
  bool result = false;
  
  switch (op) {
    case IROpcode::And:
      result = a && b;
      break;
    case IROpcode::Or:
      result = a || b;
      break;
    default:
      return inst;
  }
  
  return IRInstruction::CreateConstBool(dest, result);
}

IRInstruction ConstantFolding::FoldBitwiseOp(const IRInstruction& inst) {
  IROpcode op = inst.GetOpcode();
  IRRegister dest = inst.GetDest();
  
  // 単項演算子（BitNot）
  if (op == IROpcode::BitNot) {
    IRConstant val = GetConstantValue(inst.GetOperand(0));
    int32_t result = ~val.AsInt32();
    return IRInstruction::CreateConstInt32(dest, result);
  }
  
  // 二項演算子
  IRConstant left = GetConstantValue(inst.GetOperand(0));
  IRConstant right = GetConstantValue(inst.GetOperand(1));
  
  int32_t a = left.AsInt32();
  int32_t b = right.AsInt32();
  int32_t result = 0;
  
  switch (op) {
    case IROpcode::BitAnd:
      result = a & b;
      break;
    case IROpcode::BitOr:
      result = a | b;
      break;
    case IROpcode::BitXor:
      result = a ^ b;
      break;
    case IROpcode::Shl:
      result = a << (b & 0x1F); // JavaScript仕様: 5ビットまでのシフト
      break;
    case IROpcode::Shr:
      result = a >> (b & 0x1F);
      break;
    case IROpcode::UShr:
      result = static_cast<uint32_t>(a) >> (b & 0x1F);
      break;
    default:
      return inst;
  }
  
  return IRInstruction::CreateConstInt32(dest, result);
}

IRConstant ConstantFolding::GetConstantValue(const IRValue& value) {
  // 定数値を取得
  if (value.IsConstant()) {
    return value.GetConstant();
  }
  
  // 定数でない場合（通常ここには来ないはず）
  return IRConstant::CreateInt32(0);
}

// 静的メンバ変数の初期化
int ConstantFolding::s_optimizationLevel = 2;
bool ConstantFolding::s_enableFloatingPointFolding = true;
bool ConstantFolding::s_enableAggressiveFolding = false;

void ConstantFolding::Initialize() {
    // デフォルト設定で初期化
    s_optimizationLevel = 2;
    s_enableFloatingPointFolding = true;
    s_enableAggressiveFolding = false;
}

void ConstantFolding::Shutdown() {
    // 特に何もしない
}

void ConstantFolding::SetOptimizationLevel(int level) {
    if (level >= 0 && level <= 3) {
        s_optimizationLevel = level;
    }
}

int ConstantFolding::GetOptimizationLevel() {
    return s_optimizationLevel;
}

void ConstantFolding::EnableFloatingPointFolding(bool enable) {
    s_enableFloatingPointFolding = enable;
}

void ConstantFolding::EnableAggressiveFolding(bool enable) {
    s_enableAggressiveFolding = enable;
}

bool ConstantFolding::FoldConstants(IRFunction& function) {
    bool changed = false;
    
    // 定数マップを構築
    std::unordered_map<int, int> intConstants;
    std::unordered_map<int, double> floatConstants;
    std::unordered_map<int, bool> boolConstants;
    
    // 定数を追跡
    TrackConstants(function, intConstants, floatConstants, boolConstants);
    
    // 最適化レベルに応じて適用する最適化を決定
    switch (s_optimizationLevel) {
        case 0:
            // 最適化なし
            break;
        case 1:
            // 基本的な算術演算のみ畳み込み
            changed |= FoldArithmeticOps(function);
            break;
        case 2:
            // 標準的な畳み込み
            changed |= FoldArithmeticOps(function);
            changed |= FoldComparisonOps(function);
            changed |= FoldLogicalOps(function);
            break;
        case 3:
            // 積極的な畳み込み
            changed |= FoldArithmeticOps(function);
            changed |= FoldComparisonOps(function);
            changed |= FoldLogicalOps(function);
            changed |= FoldConversions(function);
            if (s_enableAggressiveFolding) {
                changed |= FoldMemoryOps(function);
            }
            break;
    }
    
    return changed;
}

bool ConstantFolding::FoldArithmeticOps(IRFunction& function) {
    bool changed = false;
    std::vector<IRInstruction> newInstructions;
    
    // 算術演算の畳み込みを実行
    for (const auto& inst : function.instructions()) {
        if (inst.IsArithmeticOp() && IsConstantExpression(inst)) {
            IRInstruction folded = FoldArithmeticOp(inst);
            newInstructions.push_back(folded);
            changed = true;
        } else {
            newInstructions.push_back(inst);
        }
    }
    
    if (changed) {
        function.Clear();
        for (const auto& inst : newInstructions) {
            function.AddInstruction(inst);
        }
    }
    
    return changed;
}

bool ConstantFolding::FoldComparisonOps(IRFunction& function) {
    bool changed = false;
    std::vector<IRInstruction> newInstructions;
    
    // 比較演算の畳み込みを実行
    for (const auto& inst : function.instructions()) {
        if (inst.IsComparisonOp() && IsConstantExpression(inst)) {
            IRInstruction folded = FoldComparisonOp(inst);
            newInstructions.push_back(folded);
            changed = true;
        } else {
            newInstructions.push_back(inst);
        }
    }
    
    if (changed) {
        function.Clear();
        for (const auto& inst : newInstructions) {
            function.AddInstruction(inst);
        }
    }
    
    return changed;
}

bool ConstantFolding::FoldLogicalOps(IRFunction& function) {
    bool changed = false;
    std::vector<IRInstruction> newInstructions;
    
    // 論理演算の畳み込みを実行
    for (const auto& inst : function.instructions()) {
        if (inst.IsLogicalOp() && IsConstantExpression(inst)) {
            IRInstruction folded = FoldLogicalOp(inst);
            newInstructions.push_back(folded);
            changed = true;
        } else {
            newInstructions.push_back(inst);
        }
    }
    
    if (changed) {
        function.Clear();
        for (const auto& inst : newInstructions) {
            function.AddInstruction(inst);
        }
    }
    
    return changed;
}

bool ConstantFolding::FoldConversions(IRFunction& function) {
    bool changed = false;
    std::vector<IRInstruction> newInstructions;
    
    // 型変換命令の畳み込みを実行
    for (const auto& inst : function.instructions()) {
        if (inst.IsConversionOp() && inst.GetOperand(0).IsConstant()) {
            IRRegister dest = inst.GetDest();
            IRConstant val = GetConstantValue(inst.GetOperand(0));
            IRInstruction folded;
            
            switch (inst.GetOpcode()) {
                case IROpcode::Int32ToDouble: {
                    // Int32からDoubleへの変換の完全実装
                    if (val.GetType() == IRValueType::Int32) {
                        int32_t int32Val = val.AsInt32();
                        double doubleVal = static_cast<double>(int32Val);
                        IRValue doubleIRVal = IRValue::CreateDouble(doubleVal);
                        IRInstruction folded = IRInstruction::CreateConstDouble(dest, doubleVal);
                        folded.SetResult(dest);
                        
                        // 変換が精度を保持しているかチェック
                        if (static_cast<int32_t>(doubleVal) == int32Val) {
                            // 精度が保持されている場合のみ畳み込みを実行
                            return folded;
                        }
                    }
                    break;
                }
                case IROpcode::DoubleToInt32: {
                    // Doubleからint32への変換
                    if (val.GetType() == IRValueType::Double) {
                        double doubleVal = val.AsDouble();
                        
                        // NaN、Infinity、範囲外値のチェック
                        if (std::isnan(doubleVal) || std::isinf(doubleVal)) {
                            // NaNまたはInfinityの場合は0に変換
                            IRInstruction folded = IRInstruction::CreateConstInt32(dest, 0);
                            folded.SetResult(dest);
                            return folded;
                        }
                        
                        // 範囲チェック（32ビット整数の範囲内かどうか）
                        if (doubleVal >= static_cast<double>(INT32_MIN) && 
                            doubleVal <= static_cast<double>(INT32_MAX)) {
                            int32_t int32Val = static_cast<int32_t>(doubleVal);
                            IRInstruction folded = IRInstruction::CreateConstInt32(dest, int32Val);
                            folded.SetResult(dest);
                            return folded;
                        } else {
                            // 範囲外の場合はJavaScript式の変換ルールに従う
                            // ToInt32セマンティクスを実装
                            uint32_t uint32Val = static_cast<uint32_t>(doubleVal);
                            int32_t int32Val = static_cast<int32_t>(uint32Val);
                            IRInstruction folded = IRInstruction::CreateConstInt32(dest, int32Val);
                            folded.SetResult(dest);
                            return folded;
                        }
                    }
                    break;
                }
                case IROpcode::BooleanToInt32: {
                    // BooleanからInt32への変換
                    if (val.GetType() == IRValueType::Boolean) {
                        bool boolVal = val.AsBoolean();
                        int32_t int32Val = boolVal ? 1 : 0;
                        IRInstruction folded = IRInstruction::CreateConstInt32(dest, int32Val);
                        folded.SetResult(dest);
                        return folded;
                    }
                    break;
                }
                case IROpcode::StringToNumber: {
                    // 文字列から数値への変換
                    if (val.GetType() == IRValueType::String) {
                        const std::string& strVal = val.AsString();
                        
                        // 空文字列は0に変換
                        if (strVal.empty()) {
                            IRInstruction folded = IRInstruction::CreateConstDouble(dest, 0.0);
                            folded.SetResult(dest);
                            return folded;
                        }
                        
                        // 空白のみの文字列は0に変換
                        if (std::all_of(strVal.begin(), strVal.end(), ::isspace)) {
                            IRInstruction folded = IRInstruction::CreateConstDouble(dest, 0.0);
                            folded.SetResult(dest);
                            return folded;
                        }
                        
                        // 数値への変換を試行
                        try {
                            // 整数として解析可能かチェック
                            size_t pos;
                            long long intVal = std::stoll(strVal, &pos);
                            if (pos == strVal.length()) {
                                // 完全な整数として解析できた場合
                                if (intVal >= INT32_MIN && intVal <= INT32_MAX) {
                                    IRInstruction folded = IRInstruction::CreateConstInt32(dest, static_cast<int32_t>(intVal));
                                    folded.SetResult(dest);
                                    return folded;
                                } else {
                                    IRInstruction folded = IRInstruction::CreateConstDouble(dest, static_cast<double>(intVal));
                                    folded.SetResult(dest);
                                    return folded;
                                }
                            }
                            
                            // 浮動小数点として解析
                            double doubleVal = std::stod(strVal, &pos);
                            if (pos == strVal.length()) {
                                IRInstruction folded = IRInstruction::CreateConstDouble(dest, doubleVal);
                                folded.SetResult(dest);
                                return folded;
                            }
                        } catch (const std::exception&) {
                            // 変換失敗時はNaNに変換
                            IRInstruction folded = IRInstruction::CreateConstDouble(dest, std::numeric_limits<double>::quiet_NaN());
                            folded.SetResult(dest);
                            return folded;
                        }
                    }
                    break;
                }
                case IROpcode::NumberToString: {
                    // 数値から文字列への変換
                    std::string strResult;
                    if (val.GetType() == IRValueType::Int32) {
                        int32_t intVal = val.AsInt32();
                        strResult = std::to_string(intVal);
                    } else if (val.GetType() == IRValueType::Double) {
                        double doubleVal = val.AsDouble();
                        if (std::isnan(doubleVal)) {
                            strResult = "NaN";
                        } else if (std::isinf(doubleVal)) {
                            strResult = doubleVal > 0 ? "Infinity" : "-Infinity";
                        } else {
                            strResult = std::to_string(doubleVal);
                            // 不要な末尾の0を除去
                            if (strResult.find('.') != std::string::npos) {
                                strResult.erase(strResult.find_last_not_of('0') + 1, std::string::npos);
                                if (strResult.back() == '.') {
                                    strResult.pop_back();
                                }
                            }
                        }
                    }
                    
                    if (!strResult.empty()) {
                        IRInstruction folded = IRInstruction::CreateConstString(dest, strResult);
                        folded.SetResult(dest);
                        return folded;
                    }
                    break;
                }
                case IROpcode::BitwiseNot: {
                    // ビット反転演算
                    if (val.GetType() == IRValueType::Int32) {
                        int32_t intVal = val.AsInt32();
                        int32_t result = ~intVal;
                        IRInstruction folded = IRInstruction::CreateConstInt32(dest, result);
                        folded.SetResult(dest);
                        return folded;
                    }
                    break;
                }
                case IROpcode::LogicalNot: {
                    // 論理否定演算
                    if (val.GetType() == IRValueType::Boolean) {
                        bool boolVal = val.AsBoolean();
                        bool result = !boolVal;
                        IRInstruction folded = IRInstruction::CreateConstBoolean(dest, result);
                        folded.SetResult(dest);
                        return folded;
                    } else if (val.GetType() == IRValueType::Int32) {
                        int32_t intVal = val.AsInt32();
                        bool result = (intVal == 0);
                        IRInstruction folded = IRInstruction::CreateConstBoolean(dest, result);
                        folded.SetResult(dest);
                        return folded;
                    } else if (val.GetType() == IRValueType::Double) {
                        double doubleVal = val.AsDouble();
                        bool result = (doubleVal == 0.0 || std::isnan(doubleVal));
                        IRInstruction folded = IRInstruction::CreateConstBoolean(dest, result);
                        folded.SetResult(dest);
                        return folded;
                    }
                    break;
                }
                case IROpcode::Abs: {
                    // 絶対値演算
                    if (val.GetType() == IRValueType::Int32) {
                        int32_t intVal = val.AsInt32();
                        if (intVal == INT32_MIN) {
                            // INT32_MINの絶対値はオーバーフローするため、doubleに変換
                            IRInstruction folded = IRInstruction::CreateConstDouble(dest, static_cast<double>(INT32_MAX) + 1);
                            folded.SetResult(dest);
                            return folded;
                        } else {
                            int32_t result = std::abs(intVal);
                            IRInstruction folded = IRInstruction::CreateConstInt32(dest, result);
                            folded.SetResult(dest);
                            return folded;
                        }
                    } else if (val.GetType() == IRValueType::Double) {
                        double doubleVal = val.AsDouble();
                        double result = std::abs(doubleVal);
                        IRInstruction folded = IRInstruction::CreateConstDouble(dest, result);
                        folded.SetResult(dest);
                        return folded;
                    }
                    break;
                }
                default:
                    break;
            }
            
            newInstructions.push_back(folded);
            changed = true;
        } else {
            newInstructions.push_back(inst);
        }
    }
    
    if (changed) {
        function.Clear();
        for (const auto& inst : newInstructions) {
            function.AddInstruction(inst);
        }
    }
    
    return changed;
}

bool ConstantFolding::FoldMemoryOps(IRFunction& function) {
    // メモリ操作の畳み込みは複雑で副作用があるため、慎重に実装する必要がある
    bool changed = false;
    std::vector<IRInstruction> newInstructions;
    
    // メモリロード/ストア操作の定数アドレスを畳み込む
    for (const auto& inst : function.instructions()) {
        if ((inst.GetOpcode() == IROpcode::LoadMem || inst.GetOpcode() == IROpcode::StoreMem) &&
            inst.GetOperand(0).IsConstant()) {
            
            // アドレスが定数の場合、アドレス計算を事前に実行可能
            IRRegister dest = inst.GetDest();
            IRConstant addrVal = GetConstantValue(inst.GetOperand(0));
            
            // アドレス演算が含まれる場合、定数部分を事前計算
            if (inst.GetOpcode() == IROpcode::LoadMem && inst.GetOperandCount() > 2) {
                IRConstant offsetVal = GetConstantValue(inst.GetOperand(2));
                if (offsetVal.IsInt32()) {
                    int32_t baseAddr = addrVal.AsInt32();
                    int32_t offset = offsetVal.AsInt32();
                    
                    // 新しいロード命令を生成（事前計算されたアドレスを使用）
                    IRInstruction newLoad = inst;
                    newLoad.SetOperand(0, IRValue::CreateConstInt32(baseAddr + offset));
                    
                    newInstructions.push_back(newLoad);
                    changed = true;
                    continue;
                }
            }
        }
        
        // その他の命令はそのまま
        newInstructions.push_back(inst);
    }
    
    if (changed) {
        function.Clear();
        for (const auto& inst : newInstructions) {
            function.AddInstruction(inst);
        }
    }
    
    return changed;
}

std::optional<int> ConstantFolding::EvaluateIntExpression(const IRInstruction& inst, 
                                                        const std::unordered_map<int, int>& constMap) {
    switch (inst.GetOpcode()) {
        case IROpcode::LoadConst: {
            IRConstant val = inst.GetConstant();
            if (val.IsInt32()) {
                return val.AsInt32();
            }
            break;
        }
        
        case IROpcode::Add: {
            int regA = inst.GetOperand(0).GetRegister();
            int regB = inst.GetOperand(1).GetRegister();
            
            auto itA = constMap.find(regA);
            auto itB = constMap.find(regB);
            
            if (itA != constMap.end() && itB != constMap.end()) {
                return itA->second + itB->second;
            }
            break;
        }
        
        case IROpcode::Sub: {
            int regA = inst.GetOperand(0).GetRegister();
            int regB = inst.GetOperand(1).GetRegister();
            
            auto itA = constMap.find(regA);
            auto itB = constMap.find(regB);
            
            if (itA != constMap.end() && itB != constMap.end()) {
                return itA->second - itB->second;
            }
            break;
        }
        
        case IROpcode::Mul: {
            int regA = inst.GetOperand(0).GetRegister();
            int regB = inst.GetOperand(1).GetRegister();
            
            auto itA = constMap.find(regA);
            auto itB = constMap.find(regB);
            
            if (itA != constMap.end() && itB != constMap.end()) {
                return itA->second * itB->second;
            }
            break;
        }
        
        case IROpcode::Div: {
            int regA = inst.GetOperand(0).GetRegister();
            int regB = inst.GetOperand(1).GetRegister();
            
            auto itA = constMap.find(regA);
            auto itB = constMap.find(regB);
            
            if (itA != constMap.end() && itB != constMap.end() && itB->second != 0) {
                return itA->second / itB->second;
            }
            break;
        }
        
        // その他の演算子も同様に実装
        default:
            break;
    }
    
    return std::nullopt;
}

std::optional<double> ConstantFolding::EvaluateFloatExpression(const IRInstruction& inst, 
                                                             const std::unordered_map<int, double>& constMap) {
    switch (inst.GetOpcode()) {
        case IROpcode::LoadConst: {
            IRConstant val = inst.GetConstant();
            if (val.IsDouble()) {
                return val.AsDouble();
            } else if (val.IsInt32()) {
                return static_cast<double>(val.AsInt32());
            }
            break;
        }
        
        case IROpcode::Add: {
            int regA = inst.GetOperand(0).GetRegister();
            int regB = inst.GetOperand(1).GetRegister();
            
            auto itA = constMap.find(regA);
            auto itB = constMap.find(regB);
            
            if (itA != constMap.end() && itB != constMap.end()) {
                return itA->second + itB->second;
            }
            break;
        }
        
        case IROpcode::Sub: {
            int regA = inst.GetOperand(0).GetRegister();
            int regB = inst.GetOperand(1).GetRegister();
            
            auto itA = constMap.find(regA);
            auto itB = constMap.find(regB);
            
            if (itA != constMap.end() && itB != constMap.end()) {
                return itA->second - itB->second;
            }
            break;
        }
        
        case IROpcode::Mul: {
            int regA = inst.GetOperand(0).GetRegister();
            int regB = inst.GetOperand(1).GetRegister();
            
            auto itA = constMap.find(regA);
            auto itB = constMap.find(regB);
            
            if (itA != constMap.end() && itB != constMap.end()) {
                return itA->second * itB->second;
            }
            break;
        }
        
        case IROpcode::Div: {
            int regA = inst.GetOperand(0).GetRegister();
            int regB = inst.GetOperand(1).GetRegister();
            
            auto itA = constMap.find(regA);
            auto itB = constMap.find(regB);
            
            if (itA != constMap.end() && itB != constMap.end()) {
                // JavaScriptの仕様に従う（ゼロ除算は無限大）
                if (itB->second == 0.0) {
                    return itA->second >= 0.0 ? 
                        std::numeric_limits<double>::infinity() : 
                        -std::numeric_limits<double>::infinity();
                }
                return itA->second / itB->second;
            }
            break;
        }
        
        // その他の演算子も同様に実装
        default:
            break;
    }
    
    return std::nullopt;
}

std::optional<bool> ConstantFolding::EvaluateBoolExpression(const IRInstruction& inst, 
                                                          const std::unordered_map<int, int>& constMap) {
    switch (inst.GetOpcode()) {
        case IROpcode::LoadConst: {
            IRConstant val = inst.GetConstant();
            if (val.IsBool()) {
                return val.AsBoolean();
            } else if (val.IsInt32()) {
                return val.AsInt32() != 0;
            }
            break;
        }
        
        case IROpcode::Eq: {
            int regA = inst.GetOperand(0).GetRegister();
            int regB = inst.GetOperand(1).GetRegister();
            
            auto itA = constMap.find(regA);
            auto itB = constMap.find(regB);
            
            if (itA != constMap.end() && itB != constMap.end()) {
                return itA->second == itB->second;
            }
            break;
        }
        
        case IROpcode::Ne: {
            int regA = inst.GetOperand(0).GetRegister();
            int regB = inst.GetOperand(1).GetRegister();
            
            auto itA = constMap.find(regA);
            auto itB = constMap.find(regB);
            
            if (itA != constMap.end() && itB != constMap.end()) {
                return itA->second != itB->second;
            }
            break;
        }
        
        case IROpcode::Lt: {
            int regA = inst.GetOperand(0).GetRegister();
            int regB = inst.GetOperand(1).GetRegister();
            
            auto itA = constMap.find(regA);
            auto itB = constMap.find(regB);
            
            if (itA != constMap.end() && itB != constMap.end()) {
                return itA->second < itB->second;
            }
            break;
        }
        
        // その他の演算子も同様に実装
        default:
            break;
    }
    
    return std::nullopt;
}

void ConstantFolding::TrackConstants(const IRFunction& function, 
                                   std::unordered_map<int, int>& intConstants,
                                   std::unordered_map<int, double>& floatConstants,
                                   std::unordered_map<int, bool>& boolConstants) {
    // IR関数内の定数をすべて収集
    for (const auto& inst : function.instructions()) {
        IRRegister destReg = inst.GetDest();
        
        if (inst.GetOpcode() == IROpcode::LoadConst) {
            IRConstant val = inst.GetConstant();
            
            if (val.IsInt32()) {
                intConstants[destReg] = val.AsInt32();
            } else if (val.IsDouble()) {
                floatConstants[destReg] = val.AsDouble();
            } else if (val.IsBool()) {
                boolConstants[destReg] = val.AsBoolean();
            }
        } else if (IsConstantExpression(inst)) {
            // 定数式の評価結果も追跡
            if (inst.IsArithmeticOp()) {
                auto intResult = EvaluateIntExpression(inst, intConstants);
                if (intResult) {
                    intConstants[destReg] = *intResult;
                } else if (s_enableFloatingPointFolding) {
                    auto floatResult = EvaluateFloatExpression(inst, floatConstants);
                    if (floatResult) {
                        floatConstants[destReg] = *floatResult;
                    }
                }
            } else if (inst.IsComparisonOp() || inst.IsLogicalOp()) {
                auto boolResult = EvaluateBoolExpression(inst, intConstants);
                if (boolResult) {
                    boolConstants[destReg] = *boolResult;
                }
            }
        } else {
            // 非定数命令は既存の定数を無効化
            intConstants.erase(destReg);
            floatConstants.erase(destReg);
            boolConstants.erase(destReg);
        }
    }
}

} // namespace jit
} // namespace core
} // namespace aerojs 