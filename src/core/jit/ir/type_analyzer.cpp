#include "type_analyzer.h"
#include <queue>
#include <algorithm>
#include <cassert>

namespace aerojs::core {

// TypeInfo::mergeの実装 - 2つの型を合成
TypeInfo TypeInfo::merge(const TypeInfo& a, const TypeInfo& b) {
    // 同じ型ならその型を返す
    if (a.type() == b.type()) {
        return a;
    }
    
    // どちらかが不明ならもう一方を返す
    if (a.isUnknown()) {
        return b;
    }
    if (b.isUnknown()) {
        return a;
    }
    
    // 数値型の場合、より大きな型に変換
    if (a.isNumeric() && b.isNumeric()) {
        if (a.isFloat64() || b.isFloat64()) {
            return TypeInfo(Type::Float64);
        }
        if (a.isInt64() || b.isInt64()) {
            return TypeInfo(Type::Int64);
        }
        return TypeInfo(Type::Int32);
    }
    
    // それ以外の場合は不明として扱う
    return TypeInfo(Type::Unknown);
}

TypeAnalyzer::TypeAnalyzer()
    : m_currentFunction(nullptr)
    , m_fixedPointReached(false)
    , m_iterationCount(0)
{
}

TypeAnalyzer::~TypeAnalyzer() {
    Reset();
}

bool TypeAnalyzer::Analyze(IRFunction* function) {
    if (!function) {
        return false;
    }

    Reset();
    m_currentFunction = function;

    // 基本ブロックの数を取得
    size_t blockCount = function->GetBasicBlockCount();
    if (blockCount == 0) {
        return false;
    }

    // ブロックコンテキストを初期化
    m_blockTypes.resize(blockCount);
    
    // レジスタ数を取得
    size_t registerCount = function->GetRegisterCount();
    
    // 各ブロックの型コンテキストを準備
    for (auto& blockContext : m_blockTypes) {
        blockContext.registerTypes.resize(registerCount);
        blockContext.analyzed = false;
    }
    
    // 最終型情報も初期化
    m_finalTypes.resize(registerCount);
    
    // フロー解析を実行
    PropagateTypes();
    
    // 最終型情報を構築
    for (size_t regIdx = 0; regIdx < registerCount; ++regIdx) {
        TypeInfo& finalInfo = m_finalTypes[regIdx];
        
        // すべてのブロックの型情報をマージ
        for (const auto& blockContext : m_blockTypes) {
            if (blockContext.analyzed) {
                MergeTypes(finalInfo, blockContext.registerTypes[regIdx]);
            }
        }
    }
    
    return true;
}

ValueType TypeAnalyzer::GetRegisterType(uint32_t registerIndex, uint32_t blockIndex) const {
    if (!IsValidRegisterIndex(registerIndex)) {
        return ValueType::kUnknown;
    }
    
    // 特定のブロックの型情報を要求された場合
    if (blockIndex != UINT32_MAX && IsValidBlockIndex(blockIndex)) {
        const auto& blockContext = m_blockTypes[blockIndex];
        if (blockContext.analyzed) {
            return blockContext.registerTypes[registerIndex].primaryType;
        }
        return ValueType::kUnknown;
    }
    
    // 最終型情報を返す
    return m_finalTypes[registerIndex].primaryType;
}

uint32_t TypeAnalyzer::GetPossibleTypes(uint32_t registerIndex) const {
    if (!IsValidRegisterIndex(registerIndex)) {
        return 0;
    }
    
    return m_finalTypes[registerIndex].possibleTypes;
}

double TypeAnalyzer::GetTypeProbability(uint32_t registerIndex, ValueType type) const {
    if (!IsValidRegisterIndex(registerIndex)) {
        return 0.0;
    }
    
    const auto& typeInfo = m_finalTypes[registerIndex];
    
    // 型の確率配列からの取得
    size_t typeIndex = static_cast<size_t>(type);
    if (typeIndex < typeInfo.probabilities.size()) {
        return typeInfo.probabilities[typeIndex];
    }
    
    // 可能性のある型に含まれているか確認
    if (typeInfo.possibleTypes & BIT(type)) {
        // 等確率と仮定
        int bitCount = __builtin_popcount(typeInfo.possibleTypes);
        return bitCount > 0 ? 1.0 / bitCount : 0.0;
    }
    
    return 0.0;
}

void TypeAnalyzer::PropagateTypes() {
    // キューを使用して反復的に型情報を伝搬
    std::queue<uint32_t> workQueue;
    
    // エントリーブロックの初期化
    workQueue.push(0);  // エントリーブロックは通常0
    
    // ブロック訪問管理
    std::vector<bool> inQueue(m_blockTypes.size(), false);
    inQueue[0] = true;
    
    // 反復処理
    m_iterationCount = 0;
    m_fixedPointReached = false;
    
    while (!workQueue.empty() && m_iterationCount < kMaxIterations) {
        uint32_t currentBlockIdx = workQueue.front();
        workQueue.pop();
        inQueue[currentBlockIdx] = false;
        
        // 現在のブロックを解析
        auto& blockContext = m_blockTypes[currentBlockIdx];
        bool changed = false;
        
        // ブロック内の各命令を解析
        const auto& instructions = m_currentFunction->GetInstructionsForBlock(currentBlockIdx);
        for (const auto* inst : instructions) {
            if (!inst) continue;
            
            // 命令の結果レジスタを特定
            uint32_t resultReg = inst->GetResultRegister();
            if (IsValidRegisterIndex(resultReg)) {
                // 以前の型情報を保存
                auto previousType = blockContext.registerTypes[resultReg].primaryType;
                auto previousMask = blockContext.registerTypes[resultReg].possibleTypes;
                
                // 命令を解析して型情報を更新
                AnalyzeInstruction(inst, blockContext.registerTypes[resultReg]);
                
                // 型情報が変更されたかチェック
                if (previousType != blockContext.registerTypes[resultReg].primaryType ||
                    previousMask != blockContext.registerTypes[resultReg].possibleTypes) {
                    changed = true;
                }
            }
        }
        
        blockContext.analyzed = true;
        
        // 型情報が変更された場合、後続ブロックをキューに追加
        if (changed) {
            const auto& successors = m_currentFunction->GetBlockSuccessors(currentBlockIdx);
            for (uint32_t succIdx : successors) {
                if (!inQueue[succIdx]) {
                    workQueue.push(succIdx);
                    inQueue[succIdx] = true;
                }
            }
        }
        
        // 繰り返し回数を更新
        if (workQueue.empty() && !m_fixedPointReached) {
            // 再度全ブロックを確認
            bool needAnotherPass = false;
            for (uint32_t i = 0; i < m_blockTypes.size(); ++i) {
                if (m_blockTypes[i].analyzed) {
                    workQueue.push(i);
                    inQueue[i] = true;
                    needAnotherPass = true;
                    break;
                }
            }
            
            if (!needAnotherPass) {
                m_fixedPointReached = true;
            }
            
            ++m_iterationCount;
        }
    }
}

void TypeAnalyzer::AnalyzeInstruction(const IRInstruction* inst, TypeInfo& resultType) {
    if (!inst) {
        resultType.primaryType = ValueType::kUnknown;
        resultType.possibleTypes = BIT(ValueType::kUnknown);
        return;
    }
    
    // オペコードに基づいて型を推定
    switch (inst->GetOpcode()) {
        // 定数ロード命令
        case Opcode::kLoadConst: {
            // 定数の型に基づいて型情報を設定
            const IRConstantValue& constant = inst->GetConstant();
            switch (constant.GetType()) {
                case IRConstantType::kInteger:
                    resultType.primaryType = ValueType::kInteger;
                    resultType.possibleTypes = BIT(ValueType::kInteger);
                    break;
                case IRConstantType::kDouble:
                    resultType.primaryType = ValueType::kNumber;
                    resultType.possibleTypes = BIT(ValueType::kNumber);
                    break;
                case IRConstantType::kBoolean:
                    resultType.primaryType = ValueType::kBoolean;
                    resultType.possibleTypes = BIT(ValueType::kBoolean);
                    break;
                case IRConstantType::kString:
                    resultType.primaryType = ValueType::kString;
                    resultType.possibleTypes = BIT(ValueType::kString);
                    break;
                case IRConstantType::kNull:
                    resultType.primaryType = ValueType::kNull;
                    resultType.possibleTypes = BIT(ValueType::kNull);
                    break;
                case IRConstantType::kUndefined:
                    resultType.primaryType = ValueType::kUndefined;
                    resultType.possibleTypes = BIT(ValueType::kUndefined);
                    break;
                default:
                    resultType.primaryType = ValueType::kUnknown;
                    resultType.possibleTypes = BIT(ValueType::kUnknown);
                    break;
            }
            break;
        }
        
        // 算術演算命令
        case Opcode::kAdd: {
            uint32_t lhsReg = inst->GetSourceRegister(0);
            uint32_t rhsReg = inst->GetSourceRegister(1);
            
            // ソースレジスタの型情報を取得
            ValueType lhsType = ValueType::kUnknown;
            ValueType rhsType = ValueType::kUnknown;
            
            if (IsValidRegisterIndex(lhsReg)) {
                // ブロック内のレジスタの型情報を使用
                lhsType = GetRegisterType(lhsReg);
            }
            
            if (IsValidRegisterIndex(rhsReg)) {
                // ブロック内のレジスタの型情報を使用
                rhsType = GetRegisterType(rhsReg);
            }
            
            // 加算の型推論ロジック
            if (lhsType == ValueType::kString || rhsType == ValueType::kString) {
                // 文字列連結
                resultType.primaryType = ValueType::kString;
                resultType.possibleTypes = BIT(ValueType::kString);
            } else if (lhsType == ValueType::kNumber || rhsType == ValueType::kNumber) {
                // 数値加算
                resultType.primaryType = ValueType::kNumber;
                resultType.possibleTypes = BIT(ValueType::kNumber);
            } else if (lhsType == ValueType::kInteger && rhsType == ValueType::kInteger) {
                // 整数加算（オーバーフローの可能性考慮）
                resultType.primaryType = ValueType::kInteger;
                resultType.possibleTypes = BIT(ValueType::kInteger) | BIT(ValueType::kNumber);
            } else {
                // その他のケース
                resultType.primaryType = ValueType::kNumber;
                resultType.possibleTypes = BIT(ValueType::kNumber) | BIT(ValueType::kString);
            }
            break;
        }
        
        // その他の算術演算
        case Opcode::kSub:
        case Opcode::kMul:
        case Opcode::kDiv:
        case Opcode::kMod: {
            // 通常は数値型を返す
            resultType.primaryType = ValueType::kNumber;
            resultType.possibleTypes = BIT(ValueType::kNumber);
            
            // 特定条件下では整数の可能性も
            if (inst->GetOpcode() != Opcode::kDiv && inst->GetOpcode() != Opcode::kMod) {
                uint32_t lhsReg = inst->GetSourceRegister(0);
                uint32_t rhsReg = inst->GetSourceRegister(1);
                
                if (IsValidRegisterIndex(lhsReg) && IsValidRegisterIndex(rhsReg)) {
                    ValueType lhsType = GetRegisterType(lhsReg);
                    ValueType rhsType = GetRegisterType(rhsReg);
                    
                    if (lhsType == ValueType::kInteger && rhsType == ValueType::kInteger) {
                        resultType.primaryType = ValueType::kInteger;
                        resultType.possibleTypes = BIT(ValueType::kInteger) | BIT(ValueType::kNumber);
                    }
                }
            }
            break;
        }
        
        // 比較演算
        case Opcode::kEqual:
        case Opcode::kStrictEqual:
        case Opcode::kLessThan:
        case Opcode::kLessThanOrEqual:
        case Opcode::kGreaterThan:
        case Opcode::kGreaterThanOrEqual: {
            resultType.primaryType = ValueType::kBoolean;
            resultType.possibleTypes = BIT(ValueType::kBoolean);
            break;
        }
        
        // 論理演算
        case Opcode::kAnd:
        case Opcode::kOr: {
            // 論理演算は任意の型を返す可能性がある
            uint32_t lhsReg = inst->GetSourceRegister(0);
            uint32_t rhsReg = inst->GetSourceRegister(1);
            
            if (IsValidRegisterIndex(lhsReg) && IsValidRegisterIndex(rhsReg)) {
                ValueType lhsType = GetRegisterType(lhsReg);
                ValueType rhsType = GetRegisterType(rhsReg);
                
                if (inst->GetOpcode() == Opcode::kAnd) {
                    // 論理AND：lhsがfalsyならlhs、そうでなければrhs
                    // 両方の型の可能性がある
                    resultType.possibleTypes = BIT(lhsType) | BIT(rhsType);
                    // より可能性の高い方を主要型とする
                    resultType.primaryType = rhsType;  // 単純化のため右側を優先
                } else {
                    // 論理OR：lhsがtruthyならlhs、そうでなければrhs
                    resultType.possibleTypes = BIT(lhsType) | BIT(rhsType);
                    resultType.primaryType = lhsType;  // 単純化のため左側を優先
                }
            } else {
                // 情報不足の場合は不明
                resultType.primaryType = ValueType::kAny;
                resultType.possibleTypes = ~0U;  // すべての型が可能
            }
            break;
        }
        
        // 型変換命令
        case Opcode::kToBoolean: {
            resultType.primaryType = ValueType::kBoolean;
            resultType.possibleTypes = BIT(ValueType::kBoolean);
            break;
        }
        case Opcode::kToNumber: {
            resultType.primaryType = ValueType::kNumber;
            resultType.possibleTypes = BIT(ValueType::kNumber);
            break;
        }
        case Opcode::kToString: {
            resultType.primaryType = ValueType::kString;
            resultType.possibleTypes = BIT(ValueType::kString);
            break;
        }
        
        // オブジェクト操作
        case Opcode::kCreateObject: {
            resultType.primaryType = ValueType::kObject;
            resultType.possibleTypes = BIT(ValueType::kObject);
            break;
        }
        case Opcode::kCreateArray: {
            resultType.primaryType = ValueType::kArray;
            resultType.possibleTypes = BIT(ValueType::kArray) | BIT(ValueType::kObject);
            break;
        }
        case Opcode::kGetProperty: {
            // プロパティの型は一般的に不明
            resultType.primaryType = ValueType::kAny;
            resultType.possibleTypes = ~0U;  // すべての型が可能
            break;
        }
        
        // 不明/未対応の命令
        default: {
            resultType.primaryType = ValueType::kAny;
            resultType.possibleTypes = ~0U;  // すべての型が可能
            break;
        }
    }
}

void TypeAnalyzer::MergeTypes(TypeInfo& target, const TypeInfo& source) {
    // 型ビットマスクをマージ
    target.possibleTypes |= source.possibleTypes;
    
    // 確率配列があれば統合
    if (!source.probabilities.empty()) {
        // 確率配列が初期化されていなければ初期化
        if (target.probabilities.empty()) {
            target.probabilities.resize(static_cast<size_t>(ValueType::kTypeCount), 0.0);
        }
        
        // 確率を更新（単純に最大値を採用）
        for (size_t i = 0; i < target.probabilities.size() && i < source.probabilities.size(); ++i) {
            target.probabilities[i] = std::max(target.probabilities[i], source.probabilities[i]);
        }
    }
    
    // プライマリ型の決定
    // より具体的な型を優先
    if (target.primaryType == ValueType::kUnknown || 
        target.primaryType == ValueType::kAny || 
        IsSubtype(source.primaryType, target.primaryType)) {
        target.primaryType = source.primaryType;
    }
}

void TypeAnalyzer::NarrowType(TypeInfo& info, ValueType type) {
    // 型ビットマスクの更新
    uint32_t typeMask = TypeToMask(type);
    
    // 新しい型との交差
    info.possibleTypes &= typeMask;
    
    // 交差の結果、可能な型がなくなった場合は指定された型を設定
    if (info.possibleTypes == 0) {
        info.possibleTypes = typeMask;
    }
    
    // プライマリ型の更新
    if (IsSubtype(type, info.primaryType) || 
        info.primaryType == ValueType::kAny || 
        info.primaryType == ValueType::kUnknown) {
        info.primaryType = type;
    } else {
        // 現在のプライマリ型が可能でなくなった場合
        if (!(info.possibleTypes & BIT(info.primaryType))) {
            info.primaryType = MaskToPrimaryType(info.possibleTypes);
        }
    }
    
    // 確率情報があれば更新
    if (!info.probabilities.empty()) {
        // 指定された型の確率を1.0に設定し、他を0に
        for (size_t i = 0; i < info.probabilities.size(); ++i) {
            info.probabilities[i] = (i == static_cast<size_t>(type)) ? 1.0 : 0.0;
        }
    }
}

void TypeAnalyzer::RefineTypeFromConstraint(TypeInfo& info, ValueType constraint) {
    // 制約型との互換性をチェック
    if (constraint == ValueType::kAny || constraint == ValueType::kUnknown) {
        return;  // 何も制約がない
    }
    
    // 制約型のビットマスク
    uint32_t constraintMask = TypeToMask(constraint);
    
    // 現在の可能な型との交差
    uint32_t refinedMask = info.possibleTypes & constraintMask;
    
    // 交差の結果が空でない場合は更新
    if (refinedMask != 0) {
        info.possibleTypes = refinedMask;
        
        // プライマリ型が制約とマッチしなくなった場合は更新
        if (!(refinedMask & BIT(info.primaryType))) {
            info.primaryType = MaskToPrimaryType(refinedMask);
        }
    }
    
    // 確率情報があれば更新
    if (!info.probabilities.empty()) {
        double totalProb = 0.0;
        
        // 制約にマッチしない型の確率を0に設定
        for (size_t i = 0; i < info.probabilities.size(); ++i) {
            ValueType t = static_cast<ValueType>(i);
            if (!(constraintMask & BIT(t))) {
                info.probabilities[i] = 0.0;
            } else {
                totalProb += info.probabilities[i];
            }
        }
        
        // 残りの確率を正規化
        if (totalProb > 0.0) {
            for (auto& prob : info.probabilities) {
                prob /= totalProb;
            }
        } else {
            // マッチする確率がゼロの場合、等確率分布に
            int bitCount = __builtin_popcount(refinedMask);
            double equalProb = bitCount > 0 ? 1.0 / bitCount : 0.0;
            
            for (size_t i = 0; i < info.probabilities.size(); ++i) {
                ValueType t = static_cast<ValueType>(i);
                info.probabilities[i] = (refinedMask & BIT(t)) ? equalProb : 0.0;
            }
        }
    }
}

bool TypeAnalyzer::IsSubtype(ValueType subType, ValueType superType) const {
    if (superType == ValueType::kAny) {
        return true;  // Any型はすべての型のスーパータイプ
    }
    
    if (subType == superType) {
        return true;  // 同じ型は常にサブタイプ関係
    }
    
    // 特定のサブタイプ関係
    switch (superType) {
        case ValueType::kObject:
            // オブジェクトの特殊型
            return subType == ValueType::kArray || 
                   subType == ValueType::kFunction ||
                   subType == ValueType::kRegExp;
        
        case ValueType::kNumber:
            // 数値型の特殊型
            return subType == ValueType::kInteger;
            
        default:
            return false;
    }
}

uint32_t TypeAnalyzer::TypeToMask(ValueType type) const {
    if (type == ValueType::kAny) {
        return ~0U;  // すべてのビットを設定
    }
    
    if (type == ValueType::kUnknown) {
        return 0;  // ビットなし
    }
    
    uint32_t mask = BIT(type);
    
    // 特定の型の場合はサブタイプも含める
    if (type == ValueType::kObject) {
        mask |= BIT(ValueType::kArray) | 
                BIT(ValueType::kFunction) | 
                BIT(ValueType::kRegExp);
    } else if (type == ValueType::kNumber) {
        mask |= BIT(ValueType::kInteger);
    }
    
    return mask;
}

ValueType TypeAnalyzer::MaskToPrimaryType(uint32_t mask) const {
    if (mask == 0) {
        return ValueType::kUnknown;
    }
    
    if (mask == ~0U) {
        return ValueType::kAny;
    }
    
    // 単一ビットの場合
    if ((mask & (mask - 1)) == 0) {
        // 最下位ビットの位置を計算
        unsigned long pos = 0;
#ifdef _MSC_VER
        _BitScanForward(&pos, mask);
#else
        pos = __builtin_ctz(mask);
#endif
        return static_cast<ValueType>(pos);
    }
    
    // 複数のビットがある場合、優先順位に基づいて選択
    static const ValueType priorityOrder[] = {
        ValueType::kInteger,
        ValueType::kNumber,
        ValueType::kString,
        ValueType::kBoolean,
        ValueType::kObject,
        ValueType::kArray,
        ValueType::kFunction,
        ValueType::kRegExp,
        ValueType::kNull,
        ValueType::kUndefined
    };
    
    for (ValueType type : priorityOrder) {
        if (mask & BIT(type)) {
            return type;
        }
    }
    
    return ValueType::kAny;
}

bool TypeAnalyzer::HasCompatibleTypes(const TypeInfo& lhs, const TypeInfo& rhs) const {
    // 型ビットマスクの交差をチェック
    return (lhs.possibleTypes & rhs.possibleTypes) != 0;
}

std::vector<const IRInstruction*> TypeAnalyzer::FindTypeViolations() const {
    std::vector<const IRInstruction*> violations;
    
    // 関数が設定されていない場合は空を返す
    if (!m_currentFunction) {
        return violations;
    }
    
    // 各ブロックの各命令をチェック
    for (uint32_t blockIdx = 0; blockIdx < m_blockTypes.size(); ++blockIdx) {
        const auto& instructions = m_currentFunction->GetInstructionsForBlock(blockIdx);
        
        for (const auto* inst : instructions) {
            if (!inst) continue;
            
            // オペランドの型チェック
            switch (inst->GetOpcode()) {
                // 算術演算子の型チェック
                case Opcode::kAdd:
                case Opcode::kSub:
                case Opcode::kMul:
                case Opcode::kDiv:
                case Opcode::kMod: {
                    uint32_t lhsReg = inst->GetSourceRegister(0);
                    uint32_t rhsReg = inst->GetSourceRegister(1);
                    
                    if (IsValidRegisterIndex(lhsReg) && IsValidRegisterIndex(rhsReg)) {
                        ValueType lhsType = GetRegisterType(lhsReg);
                        ValueType rhsType = GetRegisterType(rhsReg);
                        
                        // 加算の特別処理（文字列連結）
                        if (inst->GetOpcode() == Opcode::kAdd) {
                            // 文字列＋何か、または何か＋文字列は有効
                            if (lhsType == ValueType::kString || rhsType == ValueType::kString) {
                                continue;
                            }
                        }
                        
                        // 型不一致チェック
                        if (!IsNumericType(lhsType) || !IsNumericType(rhsType)) {
                            violations.push_back(inst);
                        }
                    }
                    break;
                }
                
                // 比較演算子の型チェック
                case Opcode::kLessThan:
                case Opcode::kLessThanOrEqual:
                case Opcode::kGreaterThan:
                case Opcode::kGreaterThanOrEqual: {
                    uint32_t lhsReg = inst->GetSourceRegister(0);
                    uint32_t rhsReg = inst->GetSourceRegister(1);
                    
                    if (IsValidRegisterIndex(lhsReg) && IsValidRegisterIndex(rhsReg)) {
                        ValueType lhsType = GetRegisterType(lhsReg);
                        ValueType rhsType = GetRegisterType(rhsReg);
                        
                        // 数値型または文字列型が必要
                        bool lhsValid = IsNumericType(lhsType) || lhsType == ValueType::kString;
                        bool rhsValid = IsNumericType(rhsType) || rhsType == ValueType::kString;
                        
                        // さらに、両方が文字列または両方が数値であるべき
                        bool typesCompatible = 
                            (IsNumericType(lhsType) && IsNumericType(rhsType)) ||
                            (lhsType == ValueType::kString && rhsType == ValueType::kString);
                        
                        if (!lhsValid || !rhsValid || !typesCompatible) {
                            violations.push_back(inst);
                        }
                    }
                    break;
                }
                
                // その他の型チェック（必要に応じて追加）
                // ...
                
                default:
                    break;
            }
        }
    }
    
    return violations;
}

size_t TypeAnalyzer::InsertTypeCasts() {
    size_t insertedCasts = 0;
    
    // 関数が設定されていない場合は0を返す
    if (!m_currentFunction) {
        return 0;
    }
    
    // 型違反を検出
    auto violations = FindTypeViolations();
    
    // 各違反に対して型キャストを挿入
    for (const auto* inst : violations) {
        // オペコードに基づいてキャスト挿入
        switch (inst->GetOpcode()) {
            // 算術演算子の型キャスト
            case Opcode::kAdd:
            case Opcode::kSub:
            case Opcode::kMul:
            case Opcode::kDiv:
            case Opcode::kMod: {
                uint32_t lhsReg = inst->GetSourceRegister(0);
                uint32_t rhsReg = inst->GetSourceRegister(1);
                
                if (IsValidRegisterIndex(lhsReg) && IsValidRegisterIndex(rhsReg)) {
                    ValueType lhsType = GetRegisterType(lhsReg);
                    ValueType rhsType = GetRegisterType(rhsReg);
                    
                    // 加算の特別処理
                    if (inst->GetOpcode() == Opcode::kAdd) {
                        // 少なくとも一方が文字列の場合
                        if (lhsType == ValueType::kString || rhsType == ValueType::kString) {
                            // 両方を文字列に変換
                            if (lhsType != ValueType::kString) {
                                m_currentFunction->InsertBefore(inst, Opcode::kToString, lhsReg, lhsReg);
                                insertedCasts++;
                            }
                            if (rhsType != ValueType::kString) {
                                m_currentFunction->InsertBefore(inst, Opcode::kToString, rhsReg, rhsReg);
                                insertedCasts++;
                            }
                        } else {
                            // 少なくとも一方が数値型でない場合は数値に変換
                            if (!IsNumericType(lhsType)) {
                                m_currentFunction->InsertBefore(inst, Opcode::kToNumber, lhsReg, lhsReg);
                                insertedCasts++;
                            }
                            if (!IsNumericType(rhsType)) {
                                m_currentFunction->InsertBefore(inst, Opcode::kToNumber, rhsReg, rhsReg);
                                insertedCasts++;
                            }
                        }
                    } else {
                        // その他の算術演算子は常に数値
                        if (!IsNumericType(lhsType)) {
                            m_currentFunction->InsertBefore(inst, Opcode::kToNumber, lhsReg, lhsReg);
                            insertedCasts++;
                        }
                        if (!IsNumericType(rhsType)) {
                            m_currentFunction->InsertBefore(inst, Opcode::kToNumber, rhsReg, rhsReg);
                            insertedCasts++;
                        }
                    }
                }
                break;
            }
            
            // 比較演算子の型キャスト
            case Opcode::kLessThan:
            case Opcode::kLessThanOrEqual:
            case Opcode::kGreaterThan:
            case Opcode::kGreaterThanOrEqual: {
                uint32_t lhsReg = inst->GetSourceRegister(0);
                uint32_t rhsReg = inst->GetSourceRegister(1);
                
                if (IsValidRegisterIndex(lhsReg) && IsValidRegisterIndex(rhsReg)) {
                    ValueType lhsType = GetRegisterType(lhsReg);
                    ValueType rhsType = GetRegisterType(rhsReg);
                    
                    // 文字列比較
                    if (lhsType == ValueType::kString || rhsType == ValueType::kString) {
                        // 両方を文字列に変換
                        if (lhsType != ValueType::kString) {
                            m_currentFunction->InsertBefore(inst, Opcode::kToString, lhsReg, lhsReg);
                            insertedCasts++;
                        }
                        if (rhsType != ValueType::kString) {
                            m_currentFunction->InsertBefore(inst, Opcode::kToString, rhsReg, rhsReg);
                            insertedCasts++;
                        }
                    } else {
                        // 数値比較
                        if (!IsNumericType(lhsType)) {
                            m_currentFunction->InsertBefore(inst, Opcode::kToNumber, lhsReg, lhsReg);
                            insertedCasts++;
                        }
                        if (!IsNumericType(rhsType)) {
                            m_currentFunction->InsertBefore(inst, Opcode::kToNumber, rhsReg, rhsReg);
                            insertedCasts++;
                        }
                    }
                }
                break;
            }
            
            // その他の必要なキャスト
            // ...
            
            default:
                break;
        }
    }
    
    return insertedCasts;
}

void TypeAnalyzer::Reset() {
    m_currentFunction = nullptr;
    m_blockTypes.clear();
    m_finalTypes.clear();
    m_fixedPointReached = false;
    m_iterationCount = 0;
}

bool TypeAnalyzer::IsValidRegisterIndex(uint32_t index) const {
    return m_currentFunction && index < m_currentFunction->GetRegisterCount();
}

bool TypeAnalyzer::IsValidBlockIndex(uint32_t index) const {
    return index < m_blockTypes.size();
}

// ヘルパーメソッド - 数値型かどうかをチェック
bool TypeAnalyzer::IsNumericType(ValueType type) const {
    return type == ValueType::kNumber || type == ValueType::kInteger;
}

} // namespace aerojs::core 