#include "ir_validator.h"
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cassert>
#include <queue>
#include <algorithm>
#include <sstream>
#include <stack>

namespace aerojs::core {

// 最大エラーメッセージ長
constexpr size_t kMaxErrorMessageLength = 1024;

IRValidator::IRValidator()
    : m_currentFunction(nullptr)
{
}

IRValidator::~IRValidator() 
{
}

bool IRValidator::Validate(const IRFunction* function, std::vector<ValidationError>& errors) 
{
    if (!function) {
        errors.push_back({ValidationResult::kInvalidInstruction, 0, 0, "無効なIR関数ポインタ"});
        return false;
    }

    // 内部状態のリセット
    Reset();

    // コントロールフローグラフの構築
    if (!BuildControlFlowGraph(function)) {
        errors.push_back({ValidationResult::kInvalidBlockStructure, 0, 0, "CFGの構築に失敗しました"});
        return false;
    }

    // レジスタ使用の検証
    if (!ValidateRegisterUsage(function, errors)) {
        return false;
    }

    // コントロールフローの検証
    if (!ValidateControlFlow(function, errors)) {
        return false;
    }

    // 各ブロックの各命令を検証
    uint32_t blockCount = function->GetBlockCount();
    for (uint32_t blockIdx = 0; blockIdx < blockCount; ++blockIdx) {
        const auto& instructions = function->GetInstructionsForBlock(blockIdx);
        uint32_t instCount = static_cast<uint32_t>(instructions.size());

        for (uint32_t instIdx = 0; instIdx < instCount; ++instIdx) {
            const IRInstruction* inst = instructions[instIdx];
            if (!inst) {
                errors.push_back({ValidationResult::kInvalidInstruction, blockIdx, instIdx, "命令がnullです"});
                return false;
            }

            if (!ValidateInstruction(inst, errors, blockIdx, instIdx)) {
                return false;
            }
        }

        // ブロックの終端命令を検証
        if (!instCount || 
            !instructions.back() || 
            !instructions.back()->IsTerminator()) {
            std::stringstream ss;
            ss << "ブロック " << blockIdx << " に有効な終端命令がありません";
            errors.push_back({ValidationResult::kMissingTerminator, blockIdx, instCount > 0 ? instCount - 1 : 0, ss.str()});
            return false;
        }
    }

    // 到達不能コードの検出
    std::unordered_set<uint32_t> reachableBlocks;
    PerformReachabilityAnalysis(function, reachableBlocks);

    // 到達不能ブロックを警告として報告
    for (uint32_t blockIdx = 0; blockIdx < blockCount; ++blockIdx) {
        if (reachableBlocks.find(blockIdx) == reachableBlocks.end()) {
            std::stringstream ss;
            ss << "ブロック " << blockIdx << " は到達不能です";
            errors.push_back({ValidationResult::kUnreachableCode, blockIdx, 0, ss.str()});
            // 到達不能はエラーではなく警告として扱うため、falseを返さない
        }
    }

    return errors.empty();
}

std::string IRValidator::GetErrorMessage() const 
{
    return m_errorMessage;
}

void IRValidator::Reset() 
{
    m_currentFunction = nullptr;
    m_errorMessage.clear();
    m_definedRegisters.clear();
    m_reachableBlocks.clear();
    m_cfg.clear();
    m_reverse_cfg.clear();
}

void IRValidator::LogError(ErrorCode code, const char* format, ...) 
{
    char buffer[kMaxErrorMessageLength];
    
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, kMaxErrorMessageLength, format, args);
    va_end(args);
    
    m_errorMessage = buffer;
    
    // エラーコードをプレフィックスとして追加
    switch (code) {
        case ErrorCode::kUndefinedRegister:
            m_errorMessage = "[未定義レジスタ] " + m_errorMessage;
            break;
        case ErrorCode::kInvalidRegisterType:
            m_errorMessage = "[無効なレジスタ型] " + m_errorMessage;
            break;
        case ErrorCode::kInvalidOperandCount:
            m_errorMessage = "[無効なオペランド数] " + m_errorMessage;
            break;
        case ErrorCode::kInvalidControlFlow:
            m_errorMessage = "[無効な制御フロー] " + m_errorMessage;
            break;
        case ErrorCode::kInvalidMemoryAccess:
            m_errorMessage = "[無効なメモリアクセス] " + m_errorMessage;
            break;
        case ErrorCode::kInvalidConstantIndex:
            m_errorMessage = "[無効な定数インデックス] " + m_errorMessage;
            break;
        case ErrorCode::kInconsistentTypes:
            m_errorMessage = "[型の不一致] " + m_errorMessage;
            break;
        case ErrorCode::kMissingReturnValue:
            m_errorMessage = "[戻り値がない] " + m_errorMessage;
            break;
        case ErrorCode::kUnreachableCode:
            m_errorMessage = "[到達不能コード] " + m_errorMessage;
            break;
        case ErrorCode::kStackImbalance:
            m_errorMessage = "[スタック不均衡] " + m_errorMessage;
            break;
        case ErrorCode::kInvalidJumpTarget:
            m_errorMessage = "[無効なジャンプ先] " + m_errorMessage;
            break;
        case ErrorCode::kInvalidPhiNode:
            m_errorMessage = "[無効なPHIノード] " + m_errorMessage;
            break;
        case ErrorCode::kOtherError:
        default:
            m_errorMessage = "[エラー] " + m_errorMessage;
            break;
    }
}

bool IRValidator::ValidateTypes(const IRInstruction* inst) 
{
    if (!inst) {
        LogError(ErrorCode::kOtherError, "無効な命令ポインタ");
        return false;
    }
    
    // オペコードに基づいて必要なオペランド数を検証
    const size_t expectedOperands = GetExpectedOperandCount(inst->opcode);
    if (inst->operands.size() != expectedOperands) {
        LogError(ErrorCode::kInvalidOperandCount, 
                "命令 %s には %zu 個のオペランドが必要ですが、%zu 個が指定されています",
                GetOpcodeName(inst->opcode), expectedOperands, inst->operands.size());
        return false;
    }
    
    // 命令の型整合性を検証
    switch (inst->opcode) {
        case Opcode::kAdd:
        case Opcode::kSub:
        case Opcode::kMul:
        case Opcode::kDiv:
        case Opcode::kMod: {
            // 算術演算は同じ型でなければならない
            if (inst->operands.size() >= 2) {
                ValueType type1 = m_currentFunction->GetRegisterType(inst->operands[0]);
                ValueType type2 = m_currentFunction->GetRegisterType(inst->operands[1]);
                
                if (!AreCompatibleTypes(type1, type2)) {
                    LogError(ErrorCode::kInconsistentTypes, 
                            "算術演算の型が一致しません: %s と %s",
                            GetTypeName(type1), GetTypeName(type2));
                    return false;
                }
            }
            break;
        }
        
        case Opcode::kCompareEQ:
        case Opcode::kCompareNE:
        case Opcode::kCompareLT:
        case Opcode::kCompareLE:
        case Opcode::kCompareGT:
        case Opcode::kCompareGE: {
            // 比較演算は互換性のある型でなければならない
            if (inst->operands.size() >= 2) {
                ValueType type1 = m_currentFunction->GetRegisterType(inst->operands[0]);
                ValueType type2 = m_currentFunction->GetRegisterType(inst->operands[1]);
                
                if (!AreComparableTypes(type1, type2)) {
                    LogError(ErrorCode::kInconsistentTypes, 
                            "比較演算の型が互換性を持ちません: %s と %s",
                            GetTypeName(type1), GetTypeName(type2));
                    return false;
                }
            }
            break;
        }
        
        // 他の命令も同様に検証
        // ...
    }
    
    return true;
}

bool IRValidator::ValidateDefinitionsBeforeUse() 
{
    const auto& blocks = m_currentFunction->GetBasicBlocks();
    
    for (size_t blockIdx = 0; blockIdx < blocks.size(); ++blockIdx) {
        const auto& block = blocks[blockIdx];
        
        std::vector<bool> localDefinedRegisters = m_definedRegisters;
        
        for (const auto& inst : block.instructions) {
            // オペランドが定義済みかチェック
            for (uint32_t operand : inst->operands) {
                if (operand >= localDefinedRegisters.size() || !localDefinedRegisters[operand]) {
                    LogError(ErrorCode::kUndefinedRegister, 
                            "ブロック %zu の命令 %s が未定義のレジスタ r%u を使用しています",
                            blockIdx, GetOpcodeName(inst->opcode), operand);
                    return false;
                }
            }
            
            // 結果レジスタを定義済みとしてマーク
            if (inst->HasResult()) {
                const uint32_t resultReg = inst->GetResult();
                if (resultReg < localDefinedRegisters.size()) {
                    localDefinedRegisters[resultReg] = true;
                }
            }
        }
    }
    
    return true;
}

bool IRValidator::ValidateControlFlow(const IRFunction* function, std::vector<ValidationError>& errors) 
{
    if (!function) {
        return false;
    }

    // コントロールフローグラフの妥当性をチェック
    uint32_t blockCount = function->GetBlockCount();
    
    // 各ブロックの終端命令をチェック
    for (uint32_t blockIdx = 0; blockIdx < blockCount; ++blockIdx) {
        const auto& instructions = function->GetInstructionsForBlock(blockIdx);
        if (instructions.empty()) {
            errors.push_back({ValidationResult::kInvalidBlockStructure, blockIdx, 0, "空のブロックは許可されません"});
            return false;
        }

        const IRInstruction* terminator = instructions.back();
        if (!terminator) {
            errors.push_back({ValidationResult::kMissingTerminator, blockIdx, static_cast<uint32_t>(instructions.size() - 1), 
                            "ブロック終端の命令がnullです"});
            return false;
        }

        if (!terminator->IsTerminator()) {
            errors.push_back({ValidationResult::kMissingTerminator, blockIdx, static_cast<uint32_t>(instructions.size() - 1), 
                            "ブロックが有効な終端命令で終わっていません"});
            return false;
        }

        // ジャンプターゲットが有効なブロックを指しているか確認
        if (terminator->GetOpcode() == Opcode::kJump || 
            terminator->GetOpcode() == Opcode::kBranchTrue || 
            terminator->GetOpcode() == Opcode::kBranchFalse) {
            
            uint32_t targetBlock = terminator->GetTargetBlockIndex();
            if (targetBlock >= blockCount) {
                std::stringstream ss;
                ss << "ジャンプターゲット " << targetBlock << " は有効なブロック範囲 [0, " 
                   << blockCount - 1 << "] 外です";
                
                errors.push_back({ValidationResult::kInvalidBlockStructure, blockIdx, 
                                static_cast<uint32_t>(instructions.size() - 1), ss.str()});
                return false;
            }
        }

        // 条件分岐命令の場合、フォールスルーが有効なブロックを指しているか確認
        if (terminator->GetOpcode() == Opcode::kBranchTrue || 
            terminator->GetOpcode() == Opcode::kBranchFalse) {
            
            uint32_t fallthroughBlock = blockIdx + 1;
            if (fallthroughBlock >= blockCount) {
                std::stringstream ss;
                ss << "条件分岐のフォールスルー " << fallthroughBlock 
                   << " は有効なブロック範囲外です（最後のブロックに条件分岐は使用できません）";
                
                errors.push_back({ValidationResult::kInvalidBlockStructure, blockIdx, 
                                static_cast<uint32_t>(instructions.size() - 1), ss.str()});
                return false;
            }
        }
    }

    // エントリブロック（ブロック0）に到達不能な先行ブロックがないことを確認
    if (!m_reverse_cfg[0].empty()) {
        errors.push_back({ValidationResult::kInvalidBlockStructure, 0, 0, 
                        "エントリブロック（ブロック0）に先行ブロックがあります"});
        return false;
    }

    return true;
}

bool IRValidator::ValidateMemoryAccess(const IRInstruction* inst) 
{
    // メモリアクセス命令の検証
    if (inst->opcode == Opcode::kLoad || inst->opcode == Opcode::kStore) {
        // アドレス計算とアライメントの検証
        // バウンダリチェックの検証
        // ...
    }
    
    return true;
}

bool IRValidator::ValidateSpecificInstruction(const IRInstruction* inst) 
{
    switch (inst->opcode) {
        case Opcode::kLoadConst: {
            // 定数インデックスが有効範囲内か
            if (inst->operands.size() >= 1) {
                uint32_t constIdx = inst->operands[0];
                if (constIdx >= m_currentFunction->GetConstantCount()) {
                    LogError(ErrorCode::kInvalidConstantIndex, 
                            "定数インデックス %u が範囲外です (定数プール数: %zu)",
                            constIdx, m_currentFunction->GetConstantCount());
                    return false;
                }
            }
            break;
        }
        
        case Opcode::kPhi: {
            // PHIノードの検証
            // PHIノードは基本ブロックの先頭にのみ存在することを確認
            // 対応する前任ブロックを検証
            // ...
            break;
        }
        
        // 他の命令タイプも同様に検証
        // ...
    }
    
    return true;
}

// ヘルパーメソッド

size_t IRValidator::GetExpectedOperandCount(Opcode opcode) const 
{
    switch (opcode) {
        case Opcode::kNop: return 0;
        case Opcode::kLoadConst: return 1;
        case Opcode::kLoad: return 1;
        case Opcode::kStore: return 2;
        case Opcode::kAdd:
        case Opcode::kSub:
        case Opcode::kMul:
        case Opcode::kDiv:
        case Opcode::kMod:
        case Opcode::kCompareEQ:
        case Opcode::kCompareNE:
        case Opcode::kCompareLT:
        case Opcode::kCompareLE:
        case Opcode::kCompareGT:
        case Opcode::kCompareGE:
            return 2;
        case Opcode::kNeg:
        case Opcode::kNot:
            return 1;
        case Opcode::kJump: return 0;  // ジャンプ先はoperandsではなく別途管理
        case Opcode::kBranchTrue:
        case Opcode::kBranchFalse:
            return 1;  // 条件レジスタ
        case Opcode::kReturn: return 1;  // 戻り値（voidの場合は0）
        case Opcode::kCall: return 1;  // 最低でも関数ポインタの1つ
        case Opcode::kPhi: return 2;  // 最低でも2つの入力パスが必要
        default: return 0;
    }
}

bool IRValidator::AreCompatibleTypes(ValueType type1, ValueType type2) const 
{
    // 同じ型は互換性がある
    if (type1 == type2) {
        return true;
    }
    
    // 数値型同士は互換性がある
    if (IsNumericType(type1) && IsNumericType(type2)) {
        return true;
    }
    
    // その他の型の互換性ルール
    // ...
    
    return false;
}

bool IRValidator::AreComparableTypes(ValueType type1, ValueType type2) const 
{
    // 互換性のある型は比較可能
    if (AreCompatibleTypes(type1, type2)) {
        return true;
    }
    
    // オブジェクト型と参照型は比較可能
    if (IsObjectType(type1) && IsObjectType(type2)) {
        return true;
    }
    
    // null/undefinedとの比較は常に可能
    if (type1 == ValueType::kNull || type1 == ValueType::kUndefined ||
        type2 == ValueType::kNull || type2 == ValueType::kUndefined) {
        return true;
    }
    
    return false;
}

bool IRValidator::IsNumericType(ValueType type) const 
{
    return type == ValueType::kInt32 || 
           type == ValueType::kInt64 || 
           type == ValueType::kFloat64;
}

bool IRValidator::IsObjectType(ValueType type) const 
{
    return type == ValueType::kObject || 
           type == ValueType::kArray || 
           type == ValueType::kString;
}

bool IRValidator::IsTerminator(Opcode opcode) const 
{
    return opcode == Opcode::kJump ||
           opcode == Opcode::kBranchTrue ||
           opcode == Opcode::kBranchFalse ||
           opcode == Opcode::kReturn ||
           opcode == Opcode::kThrow;
}

const char* IRValidator::GetOpcodeName(Opcode opcode) const 
{
    switch (opcode) {
        case Opcode::kNop: return "Nop";
        case Opcode::kLoadConst: return "LoadConst";
        case Opcode::kLoad: return "Load";
        case Opcode::kStore: return "Store";
        case Opcode::kAdd: return "Add";
        case Opcode::kSub: return "Sub";
        case Opcode::kMul: return "Mul";
        case Opcode::kDiv: return "Div";
        case Opcode::kMod: return "Mod";
        case Opcode::kNeg: return "Neg";
        case Opcode::kNot: return "Not";
        case Opcode::kCompareEQ: return "CompareEQ";
        case Opcode::kCompareNE: return "CompareNE";
        case Opcode::kCompareLT: return "CompareLT";
        case Opcode::kCompareLE: return "CompareLE";
        case Opcode::kCompareGT: return "CompareGT";
        case Opcode::kCompareGE: return "CompareGE";
        case Opcode::kJump: return "Jump";
        case Opcode::kBranchTrue: return "BranchTrue";
        case Opcode::kBranchFalse: return "BranchFalse";
        case Opcode::kReturn: return "Return";
        case Opcode::kCall: return "Call";
        case Opcode::kPhi: return "Phi";
        case Opcode::kThrow: return "Throw";
        default: return "不明なオペコード";
    }
}

const char* IRValidator::GetTypeName(ValueType type) const 
{
    switch (type) {
        case ValueType::kVoid: return "void";
        case ValueType::kBoolean: return "boolean";
        case ValueType::kInt32: return "int32";
        case ValueType::kInt64: return "int64";
        case ValueType::kFloat64: return "float64";
        case ValueType::kString: return "string";
        case ValueType::kObject: return "object";
        case ValueType::kArray: return "array";
        case ValueType::kFunction: return "function";
        case ValueType::kNull: return "null";
        case ValueType::kUndefined: return "undefined";
        case ValueType::kAny: return "any";
        default: return "不明な型";
    }
}

bool IRValidator::BuildControlFlowGraph(const IRFunction* function) {
    if (!function) {
        return false;
    }

    m_cfg.clear();
    m_reverse_cfg.clear();

    // 各ブロックと命令を走査してCFGを構築
    uint32_t blockCount = function->GetBlockCount();
    for (uint32_t blockIdx = 0; blockIdx < blockCount; ++blockIdx) {
        const auto& instructions = function->GetInstructionsForBlock(blockIdx);

        // 空のブロックの場合はエラー（全てのブロックには少なくとも終端命令が必要）
        if (instructions.empty()) {
            return false;
        }

        const IRInstruction* terminator = instructions.back();
        if (!terminator || !terminator->IsTerminator()) {
            return false;
        }

        // 終端命令の種類によって後続ブロックを決定
        switch (terminator->GetOpcode()) {
            case Opcode::kJump: {
                // 無条件ジャンプ
                uint32_t targetBlock = terminator->GetTargetBlockIndex();
                m_cfg[blockIdx].push_back(targetBlock);
                m_reverse_cfg[targetBlock].push_back(blockIdx);
                break;
            }
            case Opcode::kBranchTrue:
            case Opcode::kBranchFalse: {
                // 条件付きジャンプ
                uint32_t targetBlock = terminator->GetTargetBlockIndex();
                uint32_t fallthroughBlock = blockIdx + 1;  // フォールスルーは次のブロック
                
                // ターゲットブロックとフォールスルーブロックを後続に追加
                m_cfg[blockIdx].push_back(targetBlock);
                m_cfg[blockIdx].push_back(fallthroughBlock);
                
                // 逆エッジも更新
                m_reverse_cfg[targetBlock].push_back(blockIdx);
                m_reverse_cfg[fallthroughBlock].push_back(blockIdx);
                break;
            }
            case Opcode::kReturn:
            case Opcode::kReturnVoid:
            case Opcode::kThrow: {
                // 関数から制御が出るため、後続ブロックなし
                break;
            }
            default:
                // 不明な終端命令
                return false;
        }
    }

    return true;
}

bool IRValidator::ValidateRegisterUsage(const IRFunction* function, std::vector<ValidationError>& errors) {
    if (!function) {
        return false;
    }

    // 初期化済みレジスタの追跡
    std::unordered_set<uint32_t> initializedRegisters;
    
    // 引数として渡されるレジスタは初期化済みとする
    uint32_t paramCount = function->GetParamCount();
    for (uint32_t i = 0; i < paramCount; ++i) {
        initializedRegisters.insert(i);
    }

    // 関数内の各ブロック・命令を順に走査してレジスタ使用をチェック
    for (uint32_t blockIdx = 0; blockIdx < function->GetBlockCount(); ++blockIdx) {
        const auto& instructions = function->GetInstructionsForBlock(blockIdx);

        for (uint32_t instIdx = 0; instIdx < instructions.size(); ++instIdx) {
            const IRInstruction* inst = instructions[instIdx];
            if (!inst) continue;

            // 各ソースレジスタが初期化済みか確認
            for (uint32_t i = 0; i < inst->GetSourceRegisterCount(); ++i) {
                uint32_t reg = inst->GetSourceRegister(i);
                
                // レジスタ番号が有効範囲内か確認
                if (reg >= function->GetRegisterCount()) {
                    std::stringstream ss;
                    ss << "命令で使用されているレジスタ " << reg << " は有効範囲 [0, " 
                       << function->GetRegisterCount() - 1 << "] 外です";
                    
                    errors.push_back({ValidationResult::kInvalidRegisterUse, blockIdx, instIdx, ss.str()});
                    return false;
                }
                
                // レジスタが初期化済みか確認
                if (initializedRegisters.find(reg) == initializedRegisters.end()) {
                    std::stringstream ss;
                    ss << "命令がレジスタ " << reg << " を使用していますが、このレジスタは初期化されていません";
                    
                    errors.push_back({ValidationResult::kInvalidRegisterUse, blockIdx, instIdx, ss.str()});
                    return false;
                }
            }

            // 宛先レジスタを初期化済みにマーク
            if (inst->HasDestinationRegister()) {
                uint32_t destReg = inst->GetDestinationRegister();
                
                // レジスタ番号が有効範囲内か確認
                if (destReg >= function->GetRegisterCount()) {
                    std::stringstream ss;
                    ss << "命令の宛先レジスタ " << destReg << " は有効範囲 [0, " 
                       << function->GetRegisterCount() - 1 << "] 外です";
                    
                    errors.push_back({ValidationResult::kInvalidRegisterUse, blockIdx, instIdx, ss.str()});
                    return false;
                }
                
                initializedRegisters.insert(destReg);
            }
        }
    }

    return true;
}

void IRValidator::PerformReachabilityAnalysis(const IRFunction* function, std::unordered_set<uint32_t>& reachableBlocks) {
    if (!function || function->GetBlockCount() == 0) {
        return;
    }

    // 幅優先探索で到達可能なブロックを特定
    std::queue<uint32_t> workList;
    workList.push(0);  // エントリブロック（ブロック0）から開始
    reachableBlocks.insert(0);

    while (!workList.empty()) {
        uint32_t blockIdx = workList.front();
        workList.pop();

        // このブロックから到達可能な後続ブロックを探索
        auto it = m_cfg.find(blockIdx);
        if (it != m_cfg.end()) {
            for (uint32_t successor : it->second) {
                if (reachableBlocks.find(successor) == reachableBlocks.end()) {
                    reachableBlocks.insert(successor);
                    workList.push(successor);
                }
            }
        }
    }
}

bool IRValidator::ValidateInstruction(const IRInstruction* inst, std::vector<ValidationError>& errors, 
                                   uint32_t blockIdx, uint32_t instIdx) {
    if (!inst) {
        errors.push_back({ValidationResult::kInvalidInstruction, blockIdx, instIdx, "命令がnullです"});
        return false;
    }

    Opcode opcode = inst->GetOpcode();
    
    // オペコード固有のバリデーション
    switch (opcode) {
        case Opcode::kLoadConst: {
            // 定数ロードには宛先レジスタが必要
            if (!inst->HasDestinationRegister()) {
                errors.push_back({ValidationResult::kInvalidInstruction, blockIdx, instIdx, 
                                "LoadConst命令には宛先レジスタが必要です"});
                return false;
            }
            break;
        }
        case Opcode::kAdd:
        case Opcode::kSub:
        case Opcode::kMul:
        case Opcode::kDiv:
        case Opcode::kMod: {
            // 二項演算には2つのソースレジスタと1つの宛先レジスタが必要
            if (inst->GetSourceRegisterCount() != 2 || !inst->HasDestinationRegister()) {
                errors.push_back({ValidationResult::kInvalidInstruction, blockIdx, instIdx, 
                                "二項演算命令には2つのソースレジスタと1つの宛先レジスタが必要です"});
                return false;
            }
            break;
        }
        case Opcode::kNeg:
        case Opcode::kNot: {
            // 単項演算には1つのソースレジスタと1つの宛先レジスタが必要
            if (inst->GetSourceRegisterCount() != 1 || !inst->HasDestinationRegister()) {
                errors.push_back({ValidationResult::kInvalidInstruction, blockIdx, instIdx, 
                                "単項演算命令には1つのソースレジスタと1つの宛先レジスタが必要です"});
                return false;
            }
            break;
        }
        case Opcode::kBranchTrue:
        case Opcode::kBranchFalse: {
            // 条件分岐には1つのソースレジスタと有効なターゲットブロックインデックスが必要
            if (inst->GetSourceRegisterCount() != 1) {
                errors.push_back({ValidationResult::kInvalidInstruction, blockIdx, instIdx, 
                                "条件分岐命令には1つの条件レジスタが必要です"});
                return false;
            }
            break;
        }
        case Opcode::kJump: {
            // ジャンプには有効なターゲットブロックインデックスが必要（ソースレジスタは不要）
            if (inst->GetSourceRegisterCount() != 0) {
                errors.push_back({ValidationResult::kInvalidInstruction, blockIdx, instIdx, 
                                "ジャンプ命令にはソースレジスタが不要です"});
                return false;
            }
            break;
        }
        case Opcode::kReturn: {
            // 戻り命令には1つのソースレジスタが必要（宛先レジスタは不要）
            if (inst->GetSourceRegisterCount() != 1 || inst->HasDestinationRegister()) {
                errors.push_back({ValidationResult::kInvalidInstruction, blockIdx, instIdx, 
                                "Return命令には1つのソースレジスタが必要で、宛先レジスタは不要です"});
                return false;
            }
            break;
        }
        case Opcode::kReturnVoid: {
            // void戻り命令にはソースレジスタも宛先レジスタも不要
            if (inst->GetSourceRegisterCount() != 0 || inst->HasDestinationRegister()) {
                errors.push_back({ValidationResult::kInvalidInstruction, blockIdx, instIdx, 
                                "ReturnVoid命令にはソースレジスタも宛先レジスタも不要です"});
                return false;
            }
            break;
        }
        case Opcode::kPhi: {
            // Phi命令には少なくとも1つのソースレジスタと1つの宛先レジスタが必要
            if (inst->GetSourceRegisterCount() < 1 || !inst->HasDestinationRegister()) {
                errors.push_back({ValidationResult::kInvalidInstruction, blockIdx, instIdx, 
                                "Phi命令には少なくとも1つのソースレジスタと1つの宛先レジスタが必要です"});
                return false;
            }
            
            // PHI関数のソースレジスタ数と対応するブロックインデックスが一致するか確認
            if (inst->GetSourceRegisterCount() != inst->GetPhiBlockCount()) {
                errors.push_back({ValidationResult::kInvalidInstruction, blockIdx, instIdx, 
                                "Phi命令のソースレジスタ数と対応するブロックインデックス数が一致しません"});
                return false;
            }
            break;
        }
        case Opcode::kCall: {
            // 関数呼び出しには少なくとも1つのソースレジスタ（関数ポインタ）が必要
            if (inst->GetSourceRegisterCount() < 1) {
                errors.push_back({ValidationResult::kInvalidInstruction, blockIdx, instIdx, 
                                "Call命令には少なくとも1つのソースレジスタ（関数ポインタ）が必要です"});
                return false;
            }
            break;
        }
        case Opcode::kNop: {
            // NOP命令にはソースレジスタも宛先レジスタも不要
            if (inst->GetSourceRegisterCount() != 0 || inst->HasDestinationRegister()) {
                errors.push_back({ValidationResult::kInvalidInstruction, blockIdx, instIdx, 
                                "Nop命令にはソースレジスタも宛先レジスタも不要です"});
                return false;
            }
            break;
        }
        // 必要に応じて他のオペコードのバリデーションを追加
        default:
            // 未知のオペコードは警告として記録（エラーではない）
            std::stringstream ss;
            ss << "未知のオペコード: " << static_cast<int>(opcode);
            errors.push_back({ValidationResult::kInvalidInstruction, blockIdx, instIdx, ss.str()});
            break;
    }

    return true;
}

} // namespace aerojs::core 