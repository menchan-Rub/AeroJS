#include "ir_validator.h"
#include <sstream>
#include <queue>
#include <algorithm>
#include <numeric>

namespace aerojs {
namespace core {

IRValidator::IRValidator(const IRValidationOptions& options) noexcept
    : m_options(options) {
}

IRValidator::~IRValidator() noexcept = default;

bool IRValidator::Validate(const IRFunction& function) noexcept {
  // 検証前にエラーリストをクリア
  m_errors.clear();
  
  // 命令がない場合は正常終了
  const auto& instructions = function.GetInstructions();
  if (instructions.empty()) {
    return true;
  }
  
  // ラベルを収集
  std::unordered_map<std::string, size_t> labels;
  for (size_t i = 0; i < instructions.size(); ++i) {
    const auto& inst = instructions[i];
    
    // ラベル定義命令を探す
    if (inst.opcode == Opcode::Label && inst.args.size() >= 1) {
      std::string labelName = "L" + std::to_string(inst.args[0]);
      
      // 既に同じラベルが定義されていないか確認
      if (labels.find(labelName) != labels.end()) {
        AddError(ValidationErrorType::kDuplicateLabel, i, 
               "ラベル '" + labelName + "' が重複して定義されています");
      } else {
        labels[labelName] = i;
      }
    }
  }
  
  // 各命令の検証
  bool allValid = true;
  for (size_t i = 0; i < instructions.size(); ++i) {
    if (!ValidateInstruction(instructions[i], i)) {
      allValid = false;
    }
  }
  
  // レジスタの最大数をチェック
  if (!ValidateRegisterCount(function)) {
    allValid = false;
  }
  
  // レジスタの定義-使用関係を確認
  if (m_options.checkUndefinedRegisters && !ValidateRegisterUsage(function)) {
    allValid = false;
  }
  
  // ジャンプターゲットの有効性を確認
  if (!ValidateJumpTargets(function, labels)) {
    allValid = false;
  }
  
  // コードの到達可能性を確認
  if (m_options.checkUnreachableCode && !ValidateReachability(function, labels)) {
    allValid = false;
  }
  
  // スタックバランスをチェック
  if (m_options.checkStackBalance && !ValidateStackBalance(function)) {
    allValid = false;
  }
  
  return allValid;
}

const std::vector<ValidationError>& IRValidator::GetErrors() const noexcept {
  return m_errors;
}

void IRValidator::SetOptions(const IRValidationOptions& options) noexcept {
  m_options = options;
}

std::string IRValidator::GetErrorMessage() const noexcept {
  std::ostringstream oss;
  
  if (m_errors.empty()) {
    oss << "検証エラーはありません";
  } else {
    oss << m_errors.size() << "件の検証エラーが検出されました：\n";
    
    for (size_t i = 0; i < m_errors.size(); ++i) {
      const auto& error = m_errors[i];
      oss << (i + 1) << ". 命令 #" << error.instructionIndex << ": "
          << ValidationErrorTypeToString(error.type) << " - "
          << error.message << "\n";
    }
  }
  
  return oss.str();
}

void IRValidator::Reset() noexcept {
  m_errors.clear();
}

bool IRValidator::ValidateInstruction(const IRInstruction& inst, size_t index) noexcept {
  bool valid = true;
  
  // オペコードの範囲チェック
  if (static_cast<uint8_t>(inst.opcode) >= static_cast<uint8_t>(Opcode::Max)) {
    AddError(ValidationErrorType::kInvalidOpcode, index, 
           "無効なオペコード: " + std::to_string(static_cast<int>(inst.opcode)));
    valid = false;
  }
  
  // 命令ごとのオペランド数チェック
  size_t expectedArgs = 0;
  switch (inst.opcode) {
    case Opcode::Nop:
      expectedArgs = 0;  // 引数なし
      break;
      
    case Opcode::Label:
      expectedArgs = 1;  // ラベルID
      break;
      
    case Opcode::LoadConst:
      expectedArgs = 2;  // 結果レジスタ, 定数値
      break;
      
    case Opcode::LoadVar:
    case Opcode::StoreVar:
      expectedArgs = 2;  // 結果/ソースレジスタ, 変数インデックス
      break;
      
    case Opcode::Add:
    case Opcode::Sub:
    case Opcode::Mul:
    case Opcode::Div:
    case Opcode::Mod:
    case Opcode::BitAnd:
    case Opcode::BitOr:
    case Opcode::BitXor:
    case Opcode::ShiftLeft:
    case Opcode::ShiftRight:
    case Opcode::UShiftRight:
      expectedArgs = 3;  // 結果レジスタ, オペランド1, オペランド2
      break;
      
    case Opcode::BitNot:
    case Opcode::Neg:
      expectedArgs = 2;  // 結果レジスタ, オペランド
      break;
      
    case Opcode::Eq:
    case Opcode::Ne:
    case Opcode::Lt:
    case Opcode::Le:
    case Opcode::Gt:
    case Opcode::Ge:
      expectedArgs = 3;  // 結果レジスタ, オペランド1, オペランド2
      break;
      
    case Opcode::Jump:
      expectedArgs = 1;  // ジャンプ先ラベル
      break;
      
    case Opcode::JumpIf:
    case Opcode::JumpIfNot:
      expectedArgs = 2;  // 条件レジスタ, ジャンプ先ラベル
      break;
      
    case Opcode::Call:
      expectedArgs = 2;  // 結果レジスタ, 関数レジスタ (+ 可変長引数)
      // 可変長引数の場合は最小値をチェック
      if (inst.args.size() < expectedArgs) {
        AddError(ValidationErrorType::kInvalidOperandCount, index, 
               "引数の数が不足しています: 期待=" + std::to_string(expectedArgs) + 
               ", 実際=" + std::to_string(inst.args.size()));
        valid = false;
      }
      break;
      
    case Opcode::Return:
      // Returnは引数0または1（戻り値レジスタ）
      if (inst.args.size() > 1) {
        AddError(ValidationErrorType::kInvalidOperandCount, index, 
               "引数の数が過剰です: 期待=0または1, 実際=" + std::to_string(inst.args.size()));
        valid = false;
      }
      break;
      
    case Opcode::CreateObject:
      expectedArgs = 1;  // 結果レジスタ
      break;
      
    case Opcode::CreateArray:
      expectedArgs = 1;  // 結果レジスタ
      break;
      
    case Opcode::GetProperty:
      expectedArgs = 3;  // 結果レジスタ, オブジェクトレジスタ, プロパティ名レジスタ
      break;
      
    case Opcode::SetProperty:
      expectedArgs = 3;  // オブジェクトレジスタ, プロパティ名レジスタ, 値レジスタ
      break;
      
    case Opcode::GetElement:
      expectedArgs = 3;  // 結果レジスタ, 配列レジスタ, インデックスレジスタ
      break;
      
    case Opcode::SetElement:
      expectedArgs = 3;  // 配列レジスタ, インデックスレジスタ, 値レジスタ
      break;
      
    default:
      AddError(ValidationErrorType::kInvalidOpcode, index, 
             "サポートされていないオペコード: " + std::to_string(static_cast<int>(inst.opcode)));
      valid = false;
      break;
  }
  
  // Callを除く命令の引数の数をチェック
  if (inst.opcode != Opcode::Call && inst.opcode != Opcode::Return && inst.args.size() != expectedArgs) {
    AddError(ValidationErrorType::kInvalidOperandCount, index, 
           "引数の数が不正です: 期待=" + std::to_string(expectedArgs) + 
           ", 実際=" + std::to_string(inst.args.size()));
    valid = false;
  }
  
  // レジスタIDの有効性チェック
  for (size_t i = 0; i < inst.args.size(); ++i) {
    // ラベル参照やジャンプターゲットの場合はスキップ
    if ((inst.opcode == Opcode::Label && i == 0) ||
        (inst.opcode == Opcode::Jump && i == 0) ||
        (inst.opcode == Opcode::JumpIf && i == 1) ||
        (inst.opcode == Opcode::JumpIfNot && i == 1)) {
      continue;
    }
    
    // レジスタ番号のチェック
    if (inst.args[i] >= m_options.maxAllowedRegisters) {
      AddError(ValidationErrorType::kInvalidRegister, index, 
             "無効なレジスタID: " + std::to_string(inst.args[i]) + 
             ", 最大値=" + std::to_string(m_options.maxAllowedRegisters - 1));
      valid = false;
    }
  }
  
  return valid;
}

bool IRValidator::ValidateRegisterUsage(const IRFunction& function) noexcept {
  bool valid = true;
  const auto& instructions = function.GetInstructions();
  
  // レジスタの定義を追跡
  std::unordered_set<uint32_t> definedRegisters;
  
  // 関数引数は定義済みとみなす
  for (uint32_t i = 0; i < function.GetParameterCount(); ++i) {
    definedRegisters.insert(i);  // 引数はレジスタ0から順に割り当てられると仮定
  }
  
  // 各命令を順にチェック
  for (size_t i = 0; i < instructions.size(); ++i) {
    const auto& inst = instructions[i];
    
    // 命令の種類によって処理を分ける
    switch (inst.opcode) {
      case Opcode::LoadConst:
      case Opcode::LoadVar:
      case Opcode::Add:
      case Opcode::Sub:
      case Opcode::Mul:
      case Opcode::Div:
      case Opcode::Mod:
      case Opcode::BitAnd:
      case Opcode::BitOr:
      case Opcode::BitXor:
      case Opcode::ShiftLeft:
      case Opcode::ShiftRight:
      case Opcode::UShiftRight:
      case Opcode::BitNot:
      case Opcode::Neg:
      case Opcode::Eq:
      case Opcode::Ne:
      case Opcode::Lt:
      case Opcode::Le:
      case Opcode::Gt:
      case Opcode::Ge:
      case Opcode::Call:
      case Opcode::CreateObject:
      case Opcode::CreateArray:
      case Opcode::GetProperty:
      case Opcode::GetElement:
        // 結果レジスタを定義
        if (inst.args.size() > 0) {
          definedRegisters.insert(inst.args[0]);
        }
        break;
        
      default:
        // その他の命令は結果レジスタを定義しない
        break;
    }
    
    // 命令の種類によって使用レジスタをチェック
    switch (inst.opcode) {
      case Opcode::StoreVar:
        // 第1引数はソースレジスタ
        if (inst.args.size() >= 1) {
          if (definedRegisters.find(inst.args[0]) == definedRegisters.end()) {
            AddError(ValidationErrorType::kUndefinedRegister, i, 
                   "未定義のレジスタを使用しています: " + std::to_string(inst.args[0]));
            valid = false;
          }
        }
        break;
        
      case Opcode::Add:
      case Opcode::Sub:
      case Opcode::Mul:
      case Opcode::Div:
      case Opcode::Mod:
      case Opcode::BitAnd:
      case Opcode::BitOr:
      case Opcode::BitXor:
      case Opcode::ShiftLeft:
      case Opcode::ShiftRight:
      case Opcode::UShiftRight:
      case Opcode::Eq:
      case Opcode::Ne:
      case Opcode::Lt:
      case Opcode::Le:
      case Opcode::Gt:
      case Opcode::Ge:
        // 第2, 3引数はオペランドレジスタ
        if (inst.args.size() >= 3) {
          if (definedRegisters.find(inst.args[1]) == definedRegisters.end()) {
            AddError(ValidationErrorType::kUndefinedRegister, i, 
                   "未定義のレジスタを使用しています: " + std::to_string(inst.args[1]));
            valid = false;
          }
          if (definedRegisters.find(inst.args[2]) == definedRegisters.end()) {
            AddError(ValidationErrorType::kUndefinedRegister, i, 
                   "未定義のレジスタを使用しています: " + std::to_string(inst.args[2]));
            valid = false;
          }
        }
        break;
        
      case Opcode::BitNot:
      case Opcode::Neg:
        // 第2引数はオペランドレジスタ
        if (inst.args.size() >= 2) {
          if (definedRegisters.find(inst.args[1]) == definedRegisters.end()) {
            AddError(ValidationErrorType::kUndefinedRegister, i, 
                   "未定義のレジスタを使用しています: " + std::to_string(inst.args[1]));
            valid = false;
          }
        }
        break;
        
      case Opcode::JumpIf:
      case Opcode::JumpIfNot:
        // 第1引数は条件レジスタ
        if (inst.args.size() >= 1) {
          if (definedRegisters.find(inst.args[0]) == definedRegisters.end()) {
            AddError(ValidationErrorType::kUndefinedRegister, i, 
                   "未定義のレジスタを使用しています: " + std::to_string(inst.args[0]));
            valid = false;
          }
        }
        break;
        
      case Opcode::Call:
        // 第2引数は関数レジスタ、それ以降は引数レジスタ
        if (inst.args.size() >= 2) {
          if (definedRegisters.find(inst.args[1]) == definedRegisters.end()) {
            AddError(ValidationErrorType::kUndefinedRegister, i, 
                   "未定義のレジスタを使用しています: " + std::to_string(inst.args[1]));
            valid = false;
          }
          
          // 引数レジスタをチェック
          for (size_t j = 2; j < inst.args.size(); ++j) {
            if (definedRegisters.find(inst.args[j]) == definedRegisters.end()) {
              AddError(ValidationErrorType::kUndefinedRegister, i, 
                     "未定義のレジスタを使用しています: " + std::to_string(inst.args[j]));
              valid = false;
            }
          }
        }
        break;
        
      case Opcode::Return:
        // 戻り値レジスタをチェック
        if (inst.args.size() >= 1) {
          if (definedRegisters.find(inst.args[0]) == definedRegisters.end()) {
            AddError(ValidationErrorType::kUndefinedRegister, i, 
                   "未定義のレジスタを使用しています: " + std::to_string(inst.args[0]));
            valid = false;
          }
        }
        break;
        
      case Opcode::GetProperty:
      case Opcode::GetElement:
        // 第2, 3引数はオブジェクト/配列とプロパティ/インデックスレジスタ
        if (inst.args.size() >= 3) {
          if (definedRegisters.find(inst.args[1]) == definedRegisters.end()) {
            AddError(ValidationErrorType::kUndefinedRegister, i, 
                   "未定義のレジスタを使用しています: " + std::to_string(inst.args[1]));
            valid = false;
          }
          if (definedRegisters.find(inst.args[2]) == definedRegisters.end()) {
            AddError(ValidationErrorType::kUndefinedRegister, i, 
                   "未定義のレジスタを使用しています: " + std::to_string(inst.args[2]));
            valid = false;
          }
        }
        break;
        
      case Opcode::SetProperty:
      case Opcode::SetElement:
        // 全ての引数がソースレジスタ
        if (inst.args.size() >= 3) {
          for (size_t j = 0; j < 3; ++j) {
            if (definedRegisters.find(inst.args[j]) == definedRegisters.end()) {
              AddError(ValidationErrorType::kUndefinedRegister, i, 
                     "未定義のレジスタを使用しています: " + std::to_string(inst.args[j]));
              valid = false;
            }
          }
        }
        break;
        
      default:
        // その他の命令は引数をチェックしない
        break;
    }
  }
  
  return valid;
}

bool IRValidator::ValidateJumpTargets(const IRFunction& function, 
                                     const std::unordered_map<std::string, size_t>& labels) noexcept {
  bool valid = true;
  const auto& instructions = function.GetInstructions();
  
  for (size_t i = 0; i < instructions.size(); ++i) {
    const auto& inst = instructions[i];
    
    // ジャンプ命令のターゲットをチェック
    if (inst.opcode == Opcode::Jump || inst.opcode == Opcode::JumpIf || inst.opcode == Opcode::JumpIfNot) {
      size_t labelArgIndex = (inst.opcode == Opcode::Jump) ? 0 : 1;
      
      if (inst.args.size() > labelArgIndex) {
        std::string labelName = "L" + std::to_string(inst.args[labelArgIndex]);
        
        if (labels.find(labelName) == labels.end()) {
          AddError(ValidationErrorType::kUndefinedLabel, i, 
                 "未定義のラベルを参照しています: " + labelName);
          valid = false;
        }
      }
    }
  }
  
  return valid;
}

bool IRValidator::ValidateReachability(const IRFunction& function,
                                      const std::unordered_map<std::string, size_t>& labels) noexcept {
  bool valid = true;
  const auto& instructions = function.GetInstructions();
  
  // 到達可能な命令を追跡
  std::vector<bool> reachable(instructions.size(), false);
  
  // エントリーポイント（最初の命令）は到達可能
  if (!instructions.empty()) {
    reachable[0] = true;
  }
  
  // ジャンプターゲットも到達可能とマーク
  for (const auto& [label, index] : labels) {
    if (index < reachable.size()) {
      reachable[index] = true;
    }
  }
  
  // 到達可能性を伝播
  bool changed = true;
  while (changed) {
    changed = false;
    
    for (size_t i = 0; i < instructions.size(); ++i) {
      // 既に到達可能とマークされている場合
      if (reachable[i]) {
        const auto& inst = instructions[i];
        
        // フォールスルーする命令の場合、次の命令も到達可能
        if (inst.opcode != Opcode::Return && inst.opcode != Opcode::Jump && i + 1 < instructions.size()) {
          if (!reachable[i + 1]) {
            reachable[i + 1] = true;
            changed = true;
          }
        }
        
        // ジャンプ命令の場合、ターゲットも到達可能
        if (inst.opcode == Opcode::Jump || inst.opcode == Opcode::JumpIf || inst.opcode == Opcode::JumpIfNot) {
          size_t labelArgIndex = (inst.opcode == Opcode::Jump) ? 0 : 1;
          
          if (inst.args.size() > labelArgIndex) {
            std::string labelName = "L" + std::to_string(inst.args[labelArgIndex]);
            
            auto it = labels.find(labelName);
            if (it != labels.end() && !reachable[it->second]) {
              reachable[it->second] = true;
              changed = true;
            }
          }
        }
      }
    }
  }
  
  // 到達不能なコードをチェック
  for (size_t i = 0; i < instructions.size(); ++i) {
    if (!reachable[i]) {
      AddError(ValidationErrorType::kUnreachableCode, i, 
             "到達不能なコード");
      valid = false;
    }
  }
  
  return valid;
}

bool IRValidator::ValidateStackBalance(const IRFunction& function) noexcept {
  bool valid = true;
  const auto& instructions = function.GetInstructions();
  
  // 関数呼び出しのスタックバランスをチェック
  int stackBalance = 0;
  
  for (size_t i = 0; i < instructions.size(); ++i) {
    const auto& inst = instructions[i];
    
    if (inst.opcode == Opcode::Call) {
      // 引数の数（関数レジスタを除く）
      int argCount = static_cast<int>(inst.args.size() - 2);
      stackBalance -= argCount;
      
      // 戻り値があれば+1
      stackBalance += 1;
    }
  }
  
  // 最終的なスタックバランスが0でない場合はエラー
  if (stackBalance != 0) {
    AddError(ValidationErrorType::kStackImbalance, 0, 
           "関数終了時のスタックバランスが不正です: " + std::to_string(stackBalance));
    valid = false;
  }
  
  return valid;
}

void IRValidator::AddError(ValidationErrorType type, size_t index, const std::string& message) noexcept {
  m_errors.emplace_back(type, index, message);
}

bool IRValidator::ValidateRegisterCount(const IRFunction& function) noexcept {
  bool valid = true;
  const auto& instructions = function.GetInstructions();
  
  // 使用されている最大のレジスタIDを検出
  uint32_t maxRegId = 0;
  for (const auto& inst : instructions) {
    for (size_t i = 0; i < inst.args.size(); ++i) {
      // ラベル参照やジャンプターゲットの場合はスキップ
      if ((inst.opcode == Opcode::Label && i == 0) ||
          (inst.opcode == Opcode::Jump && i == 0) ||
          (inst.opcode == Opcode::JumpIf && i == 1) ||
          (inst.opcode == Opcode::JumpIfNot && i == 1)) {
        continue;
      }
      
      if (inst.args[i] > maxRegId) {
        maxRegId = inst.args[i];
      }
    }
  }
  
  // 最大レジスタ数を超えていないかチェック
  if (maxRegId >= m_options.maxAllowedRegisters) {
    AddError(ValidationErrorType::kMaxRegistersExceeded, 0, 
           "最大レジスタ数を超えています: " + std::to_string(maxRegId + 1) + 
           ", 許容値=" + std::to_string(m_options.maxAllowedRegisters));
    valid = false;
  }
  
  return valid;
}

std::string ValidationErrorTypeToString(ValidationErrorType type) {
  switch (type) {
    case ValidationErrorType::kInvalidOpcode:
      return "無効なオペコード";
    case ValidationErrorType::kInvalidOperandCount:
      return "無効なオペランド数";
    case ValidationErrorType::kInvalidRegister:
      return "無効なレジスタ";
    case ValidationErrorType::kUndefinedRegister:
      return "未定義のレジスタ使用";
    case ValidationErrorType::kInvalidJumpTarget:
      return "無効なジャンプターゲット";
    case ValidationErrorType::kUndefinedLabel:
      return "未定義のラベル使用";
    case ValidationErrorType::kInvalidConstantIndex:
      return "無効な定数インデックス";
    case ValidationErrorType::kInvalidFunctionIndex:
      return "無効な関数インデックス";
    case ValidationErrorType::kInvalidInstructionSequence:
      return "無効な命令シーケンス";
    case ValidationErrorType::kDuplicateLabel:
      return "ラベルの重複定義";
    case ValidationErrorType::kUnreachableCode:
      return "到達不能コード";
    case ValidationErrorType::kIncompatibleTypes:
      return "型の互換性エラー";
    case ValidationErrorType::kStackImbalance:
      return "スタック不均衡";
    case ValidationErrorType::kMaxRegistersExceeded:
      return "レジスタ数超過";
    case ValidationErrorType::kCyclicDependency:
      return "循環依存関係";
    case ValidationErrorType::kOther:
      return "その他のエラー";
    default:
      return "不明なエラー";
  }
}

} // namespace core
} // namespace aerojs 