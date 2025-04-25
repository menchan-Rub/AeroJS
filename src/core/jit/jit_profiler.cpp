#include "src/core/jit/jit_profiler.h"
#include "src/core/jit/jit_manager.h"  // ProfiledTypeInfo定義用
#include "ir/ir_builder.h"
#include "jit_compiler.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iostream>
#include <chrono>

namespace aerojs::core {

// 静的デフォルト値の初期化
const TypeFeedbackRecord JITProfiler::s_defaultTypeFeedback = {
    TypeFeedbackRecord::TypeCategory::Unknown, 0, false, false
};

const ExecutionCounterRecord JITProfiler::s_defaultExecutionCounter = {
    0, 0, 0, false
};

JITProfiler::JITProfiler()
    : m_hotFunctionThreshold(1000)
    , m_hotLoopThreshold(100)
    , m_hotCallSiteThreshold(50)
    , m_profilingEnabled(true)
{
}

JITProfiler::~JITProfiler() = default;

void JITProfiler::Enable() {
    m_enabled = true;
}

void JITProfiler::Disable() {
    m_enabled = false;
}

bool JITProfiler::IsEnabled() const {
    return m_enabled;
}

void JITProfiler::incrementExecutionCount(uint32_t functionId, uint32_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_executionCounts[functionId] += count;
}

void JITProfiler::recordTypeInfo(uint32_t functionId, uint32_t varIndex, const ProfiledTypeInfo& typeInfo) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t key = makeTypeInfoKey(functionId, varIndex);
    auto it = m_typeInfos.find(key);
    
    if (it != m_typeInfos.end()) {
        // 既存の型情報を更新
        if (it->second.expectedType != typeInfo.expectedType) {
            it->second.typeCheckFailures++;
        }
        it->second.isInlined |= typeInfo.isInlined;
    } else {
        // 新しい型情報を追加
        m_typeInfos[key] = typeInfo;
    }
}

void JITProfiler::recordBranch(uint32_t functionId, uint32_t bytecodeOffset, bool taken) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t key = makeBytecodePointKey(functionId, bytecodeOffset);
    
    if (taken) {
        m_branchData[key].takenCount++;
    } else {
        m_branchData[key].notTakenCount++;
    }
}

void JITProfiler::recordCallSite(uint32_t functionId, uint32_t bytecodeOffset, uint32_t targetFunctionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t key = makeBytecodePointKey(functionId, bytecodeOffset);
    m_callSiteData[key].recordCall(targetFunctionId);
}

void JITProfiler::recordLoopIteration(uint32_t functionId, uint32_t bytecodeOffset, uint32_t iterations) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t key = makeBytecodePointKey(functionId, bytecodeOffset);
    LoopProfilingData& data = m_loopData[key];
    
    data.executionCount++;
    data.totalIterations += iterations;
    data.maxIterations = std::max(data.maxIterations, iterations);
}

void JITProfiler::recordValueRange(uint32_t functionId, uint32_t bytecodeOffset, uint8_t slot, int64_t value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ProfilingPointId id = {functionId, bytecodeOffset, slot};
    m_valueRangeData[id].update(value);
}

void JITProfiler::recordFloatValue(uint32_t functionId, uint32_t bytecodeOffset, uint8_t slot) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ProfilingPointId id = {functionId, bytecodeOffset, slot};
    m_valueRangeData[id].updateFloat();
}

uint32_t JITProfiler::getExecutionCount(uint32_t functionId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_executionCounts.find(functionId);
    return it != m_executionCounts.end() ? it->second : 0;
}

const ProfiledTypeInfo* JITProfiler::getTypeInfo(uint32_t functionId, uint32_t varIndex) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t key = makeTypeInfoKey(functionId, varIndex);
    auto it = m_typeInfos.find(key);
    return it != m_typeInfos.end() ? &it->second : nullptr;
}

const BranchProfilingData* JITProfiler::getBranchData(uint32_t functionId, uint32_t bytecodeOffset) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t key = makeBytecodePointKey(functionId, bytecodeOffset);
    auto it = m_branchData.find(key);
    return it != m_branchData.end() ? &it->second : nullptr;
}

const CallSiteProfilingData* JITProfiler::getCallSiteData(uint32_t functionId, uint32_t bytecodeOffset) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t key = makeBytecodePointKey(functionId, bytecodeOffset);
    auto it = m_callSiteData.find(key);
    return it != m_callSiteData.end() ? &it->second : nullptr;
}

const LoopProfilingData* JITProfiler::getLoopData(uint32_t functionId, uint32_t bytecodeOffset) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t key = makeBytecodePointKey(functionId, bytecodeOffset);
    auto it = m_loopData.find(key);
    return it != m_loopData.end() ? &it->second : nullptr;
}

const ValueRangeProfilingData* JITProfiler::getValueRangeData(uint32_t functionId, uint32_t bytecodeOffset, uint8_t slot) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    ProfilingPointId id = {functionId, bytecodeOffset, slot};
    auto it = m_valueRangeData.find(id);
    return it != m_valueRangeData.end() ? &it->second : nullptr;
}

void JITProfiler::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_executionCounts.clear();
    m_typeInfos.clear();
    m_branchData.clear();
    m_callSiteData.clear();
    m_loopData.clear();
    m_valueRangeData.clear();
}

void JITProfiler::resetFunction(uint32_t functionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 実行回数のリセット
    m_executionCounts.erase(functionId);
    
    // 関数IDに関連する型情報の削除
    auto typeIt = m_typeInfos.begin();
    while (typeIt != m_typeInfos.end()) {
        if ((typeIt->first >> 32) == functionId) {
            typeIt = m_typeInfos.erase(typeIt);
        } else {
            ++typeIt;
        }
    }
    
    // 関数IDに関連する分岐データの削除
    auto branchIt = m_branchData.begin();
    while (branchIt != m_branchData.end()) {
        if ((branchIt->first >> 32) == functionId) {
            branchIt = m_branchData.erase(branchIt);
        } else {
            ++branchIt;
        }
    }
    
    // 関数IDに関連する呼び出しサイトデータの削除
    auto callSiteIt = m_callSiteData.begin();
    while (callSiteIt != m_callSiteData.end()) {
        if ((callSiteIt->first >> 32) == functionId) {
            callSiteIt = m_callSiteData.erase(callSiteIt);
        } else {
            ++callSiteIt;
        }
    }
    
    // 関数IDに関連するループデータの削除
    auto loopIt = m_loopData.begin();
    while (loopIt != m_loopData.end()) {
        if ((loopIt->first >> 32) == functionId) {
            loopIt = m_loopData.erase(loopIt);
        } else {
            ++loopIt;
        }
    }
    
    // 関数IDに関連する値範囲データの削除
    auto valueRangeIt = m_valueRangeData.begin();
    while (valueRangeIt != m_valueRangeData.end()) {
        if (valueRangeIt->first.functionId == functionId) {
            valueRangeIt = m_valueRangeData.erase(valueRangeIt);
        } else {
            ++valueRangeIt;
        }
    }
}

std::string JITProfiler::dumpStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::stringstream ss;
    
    ss << "JITプロファイラー統計:\n";
    ss << "====================\n\n";
    
    // 関数実行統計
    ss << "関数実行カウント:\n";
    if (m_executionCounts.empty()) {
        ss << "  データなし\n";
    } else {
        for (const auto& [functionId, count] : m_executionCounts) {
            ss << "  関数 #" << functionId << ": " << count << "回実行\n";
        }
    }
    ss << "\n";
    
    // 型情報統計
    ss << "型プロファイリング情報:\n";
    if (m_typeInfos.empty()) {
        ss << "  データなし\n";
    } else {
        std::unordered_map<uint32_t, int> typeInfosPerFunction;
        for (const auto& [key, value] : m_typeInfos) {
            uint32_t functionId = static_cast<uint32_t>(key >> 32);
            typeInfosPerFunction[functionId]++;
        }
        
        for (const auto& [functionId, count] : typeInfosPerFunction) {
            ss << "  関数 #" << functionId << ": " << count << "箇所の型プロファイリングポイント\n";
        }
    }
    ss << "\n";
    
    // 分岐統計
    ss << "分岐プロファイリング情報:\n";
    if (m_branchData.empty()) {
        ss << "  データなし\n";
    } else {
        int predictableBranches = 0;
        int totalBranches = m_branchData.size();
        
        for (const auto& [key, data] : m_branchData) {
            if (data.isPredictable()) {
                predictableBranches++;
            }
        }
        
        ss << "  合計分岐箇所: " << totalBranches << "\n";
        ss << "  予測可能分岐: " << predictableBranches << " (" 
           << std::fixed << std::setprecision(1) 
           << (totalBranches > 0 ? (predictableBranches * 100.0 / totalBranches) : 0.0) 
           << "%)\n";
    }
    ss << "\n";
    
    // 呼び出しサイト統計
    ss << "呼び出しサイトプロファイリング情報:\n";
    if (m_callSiteData.empty()) {
        ss << "  データなし\n";
    } else {
        int monomorphicSites = 0;
        int polymorphicSites = 0;
        int megamorphicSites = 0;
        int totalSites = m_callSiteData.size();
        
        for (const auto& [key, data] : m_callSiteData) {
            if (data.isMonomorphic()) {
                monomorphicSites++;
            } else if (data.isPolymorphic()) {
                polymorphicSites++;
            } else if (data.isMegamorphic()) {
                megamorphicSites++;
            }
        }
        
        auto printPercentage = [&ss, totalSites](int count) {
            ss << " (" << std::fixed << std::setprecision(1) 
               << (totalSites > 0 ? (count * 100.0 / totalSites) : 0.0) 
               << "%)";
        };
        
        ss << "  合計呼び出しサイト: " << totalSites << "\n";
        ss << "  単形態(Monomorphic): " << monomorphicSites;
        printPercentage(monomorphicSites);
        ss << "\n";
        
        ss << "  多形態(Polymorphic): " << polymorphicSites;
        printPercentage(polymorphicSites);
        ss << "\n";
        
        ss << "  超多形態(Megamorphic): " << megamorphicSites;
        printPercentage(megamorphicSites);
        ss << "\n";
    }
    ss << "\n";
    
    // ループ統計
    ss << "ループプロファイリング情報:\n";
    if (m_loopData.empty()) {
        ss << "  データなし\n";
    } else {
        int unrollCandidates = 0;
        int osrCandidates = 0;
        int totalLoops = m_loopData.size();
        
        for (const auto& [key, data] : m_loopData) {
            if (data.isCandidateForUnrolling()) {
                unrollCandidates++;
            }
            if (data.isCandidateForOSR()) {
                osrCandidates++;
            }
        }
        
        auto printPercentage = [&ss, totalLoops](int count) {
            ss << " (" << std::fixed << std::setprecision(1) 
               << (totalLoops > 0 ? (count * 100.0 / totalLoops) : 0.0) 
               << "%)";
        };
        
        ss << "  合計ループ数: " << totalLoops << "\n";
        ss << "  アンロール候補: " << unrollCandidates;
        printPercentage(unrollCandidates);
        ss << "\n";
        
        ss << "  OSR候補: " << osrCandidates;
        printPercentage(osrCandidates);
        ss << "\n";
    }
    ss << "\n";
    
    // 値範囲統計
    ss << "値範囲プロファイリング情報:\n";
    if (m_valueRangeData.empty()) {
        ss << "  データなし\n";
    } else {
        int constantValues = 0;
        int smallIntValues = 0;
        int floatValues = 0;
        int totalValues = m_valueRangeData.size();
        
        for (const auto& [id, data] : m_valueRangeData) {
            if (data.isConstant()) {
                constantValues++;
            }
            if (data.isSmallInteger()) {
                smallIntValues++;
            }
            if (!data.isAllInteger) {
                floatValues++;
            }
        }
        
        auto printPercentage = [&ss, totalValues](int count) {
            ss << " (" << std::fixed << std::setprecision(1) 
               << (totalValues > 0 ? (count * 100.0 / totalValues) : 0.0) 
               << "%)";
        };
        
        ss << "  合計値プロファイリングポイント: " << totalValues << "\n";
        ss << "  定数値: " << constantValues;
        printPercentage(constantValues);
        ss << "\n";
        
        ss << "  小整数値: " << smallIntValues;
        printPercentage(smallIntValues);
        ss << "\n";
        
        ss << "  浮動小数点値: " << floatValues;
        printPercentage(floatValues);
        ss << "\n";
    }
    
    return ss.str();
}

void JITProfiler::RecordFunctionCall(uint32_t functionId) {
    if (!m_enabled) return;
    
    auto& profile = m_functionProfiles[functionId];
    profile.callCount++;
}

void JITProfiler::RecordTypeFeedback(uint32_t bytecodeOffset, Value value) {
    if (!m_enabled) return;
    
    // 値から型カテゴリを決定
    TypeFeedbackRecord::TypeCategory category = TypeFeedbackRecord::TypeCategory::Unknown;
    
    if (value.IsInt32()) {
        category = TypeFeedbackRecord::TypeCategory::Integer;
    } else if (value.IsDouble()) {
        category = TypeFeedbackRecord::TypeCategory::Double;
    } else if (value.IsBoolean()) {
        category = TypeFeedbackRecord::TypeCategory::Boolean;
    } else if (value.IsString()) {
        category = TypeFeedbackRecord::TypeCategory::String;
    } else if (value.IsObject()) {
        if (value.IsArray()) {
            category = TypeFeedbackRecord::TypeCategory::Array;
        } else if (value.IsFunction()) {
            category = TypeFeedbackRecord::TypeCategory::Function;
        } else {
            category = TypeFeedbackRecord::TypeCategory::Object;
        }
    }
    
    // 現在実行中の関数IDの取得（簡略化のため0を使用）
    uint32_t currentFunctionId = 0; // 実際には現在の実行コンテキストから取得する
    
    auto& profile = m_functionProfiles[currentFunctionId];
    auto& typeRecord = profile.typeFeedback[bytecodeOffset];
    
    if (typeRecord.observationCount == 0) {
        typeRecord.observedType = category;
    } else if (typeRecord.observedType != category) {
        typeRecord.observedType = TypeFeedbackRecord::TypeCategory::Mixed;
    }
    
    typeRecord.observationCount++;
}

void JITProfiler::RecordBranch(uint32_t bytecodeOffset, bool taken) {
    if (!m_enabled) return;
    
    // 現在実行中の関数IDの取得（簡略化のため0を使用）
    uint32_t currentFunctionId = 0; // 実際には現在の実行コンテキストから取得する
    
    auto& profile = m_functionProfiles[currentFunctionId];
    auto& branchRecord = profile.branchBias[bytecodeOffset];
    
    if (taken) {
        branchRecord.takenCount++;
    } else {
        branchRecord.notTakenCount++;
    }
}

void JITProfiler::RecordLoopIteration(uint32_t loopHeaderOffset, uint32_t iterationCount) {
    if (!m_enabled) return;
    
    // 現在実行中の関数IDの取得（簡略化のため0を使用）
    uint32_t currentFunctionId = 0; // 実際には現在の実行コンテキストから取得する
    
    auto& profile = m_functionProfiles[currentFunctionId];
    auto& loopProfile = profile.loops[loopHeaderOffset];
    
    loopProfile.recordIteration(iterationCount);
}

void JITProfiler::RecordPropertyAccess(uint32_t bytecodeOffset, const std::string& propertyName, Value value) {
    if (!m_enabled) return;
    
    // 値から型カテゴリを決定
    TypeFeedbackRecord::TypeCategory category = TypeFeedbackRecord::TypeCategory::Unknown;
    
    if (value.IsInt32()) {
        category = TypeFeedbackRecord::TypeCategory::Integer;
    } else if (value.IsDouble()) {
        category = TypeFeedbackRecord::TypeCategory::Double;
    } else if (value.IsBoolean()) {
        category = TypeFeedbackRecord::TypeCategory::Boolean;
    } else if (value.IsString()) {
        category = TypeFeedbackRecord::TypeCategory::String;
    } else if (value.IsObject()) {
        if (value.IsArray()) {
            category = TypeFeedbackRecord::TypeCategory::Array;
        } else if (value.IsFunction()) {
            category = TypeFeedbackRecord::TypeCategory::Function;
        } else {
            category = TypeFeedbackRecord::TypeCategory::Object;
        }
    }
    
    // 現在実行中の関数IDの取得（簡略化のため0を使用）
    uint32_t currentFunctionId = 0; // 実際には現在の実行コンテキストから取得する
    
    auto& profile = m_functionProfiles[currentFunctionId];
    auto& propertyProfile = profile.propertyAccesses[bytecodeOffset];
    
    propertyProfile.recordAccess(propertyName, category);
}

//==============================================================================
// TypeFeedbackRecord実装
//==============================================================================

std::string TypeFeedbackRecord::GetCategoryName() const {
    switch (category) {
        case TypeCategory::Unknown:    return "不明";
        case TypeCategory::Integer:    return "整数";
        case TypeCategory::Double:     return "浮動小数点";
        case TypeCategory::Boolean:    return "真偽値";
        case TypeCategory::String:     return "文字列";
        case TypeCategory::Object:     return "オブジェクト";
        case TypeCategory::Array:      return "配列";
        case TypeCategory::Function:   return "関数";
        case TypeCategory::Null:       return "null";
        case TypeCategory::Undefined:  return "undefined";
        default:                       return "不明な型";
    }
}

//==============================================================================
// JITProfiler実装
//==============================================================================

void JITProfiler::RecordFunctionEntry(uint32_t functionId, const std::string& functionName) {
    if (!m_profilingEnabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 関数プロファイルが存在しない場合は作成
    if (m_profileData.find(functionId) == m_profileData.end()) {
        auto profile = std::make_unique<FunctionProfile>();
        profile->functionId = functionId;
        profile->functionName = functionName;
        m_profileData[functionId] = std::move(profile);
    }
    
    // 実行回数を増加
    m_profileData[functionId]->totalExecutions++;
}

void JITProfiler::RecordFunctionExit(uint32_t functionId) {
    // 現在の実装では特別な処理は不要
}

void JITProfiler::RecordVariableType(uint32_t functionId, uint32_t variableIndex, 
                                  TypeFeedbackRecord::TypeCategory type,
                                  bool isNegativeZero, bool isNaN) {
    if (!m_profilingEnabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 関数プロファイルが存在することを確認
    auto it = m_profileData.find(functionId);
    if (it == m_profileData.end()) return;
    
    auto& profile = it->second;
    
    // 変数インデックスに対応する型フィードバックを確保
    if (variableIndex >= profile->typeFeedback.size()) {
        profile->typeFeedback.resize(variableIndex + 1);
    }
    
    // 型フィードバックを更新
    auto& record = profile->typeFeedback[variableIndex];
    record.observationCount++;
    
    // 特別な浮動小数点値の記録
    if (type == TypeFeedbackRecord::TypeCategory::Double) {
        record.hasNegativeZero |= isNegativeZero;
        record.hasNaN |= isNaN;
    }
    
    // 型の信頼度を更新
    UpdateTypeConfidence(record, type);
}

void JITProfiler::RecordArithmeticOperation(uint32_t functionId, uint32_t bytecodeOffset, 
                                        TypeFeedbackRecord::TypeCategory type) {
    if (!m_profilingEnabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 関数プロファイルが存在することを確認
    auto it = m_profileData.find(functionId);
    if (it == m_profileData.end()) return;
    
    // 算術演算の型フィードバックを更新
    auto& record = it->second->arithmeticOperations[bytecodeOffset];
    record.observationCount++;
    
    // 型の信頼度を更新
    UpdateTypeConfidence(record, type);
}

void JITProfiler::RecordComparisonOperation(uint32_t functionId, uint32_t bytecodeOffset, 
                                         TypeFeedbackRecord::TypeCategory type) {
    if (!m_profilingEnabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 関数プロファイルが存在することを確認
    auto it = m_profileData.find(functionId);
    if (it == m_profileData.end()) return;
    
    // 比較演算の型フィードバックを更新
    auto& record = it->second->comparisonOperations[bytecodeOffset];
    record.observationCount++;
    
    // 型の信頼度を更新
    UpdateTypeConfidence(record, type);
}

void JITProfiler::RecordPropertyAccess(uint32_t functionId, uint32_t bytecodeOffset, 
                                    uint32_t shapeId, const std::string& propertyName) {
    if (!m_profilingEnabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 関数プロファイルが存在することを確認
    auto it = m_profileData.find(functionId);
    if (it == m_profileData.end()) return;
    
    // プロパティアクセスプロファイルを更新
    auto& profile = it->second->propertyAccesses[bytecodeOffset];
    profile.accessCount++;
    
    // プロパティ名を設定（空でなければ）
    if (!propertyName.empty()) {
        profile.propertyName = propertyName;
    }
    
    // 形状IDの一貫性を更新
    UpdatePropertyAccessProfile(profile, shapeId);
}

void JITProfiler::RecordLoopIteration(uint32_t functionId, uint32_t loopHeaderOffset) {
    if (!m_profilingEnabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 関数プロファイルが存在することを確認
    auto it = m_profileData.find(functionId);
    if (it == m_profileData.end()) return;
    
    // ループ実行カウンターを更新
    auto& counter = it->second->loopExecutionCounts[loopHeaderOffset];
    counter.executionCount++;
    
    // 平均反復回数を更新（単純な加重平均）
    counter.averageIterations = (counter.averageIterations * (counter.executionCount - 1) + 1.0f) / counter.executionCount;
}

void JITProfiler::RecordFunctionCall(uint32_t callerFunctionId, uint32_t bytecodeOffset, 
                                  uint32_t calleeFunctionId) {
    if (!m_profilingEnabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 呼び出し元関数プロファイルが存在することを確認
    auto it = m_profileData.find(callerFunctionId);
    if (it == m_profileData.end()) return;
    
    // 呼び出しサイト実行カウンターを更新
    auto& counter = it->second->callSiteExecutionCounts[bytecodeOffset];
    counter.executionCount++;
    
    // 最も一般的なターゲットを追跡
    if (counter.mostCommonTarget.first == calleeFunctionId) {
        counter.mostCommonTarget.second++;
    } else if (counter.mostCommonTarget.second == 0 || 
              counter.executionCount / 2 > counter.mostCommonTarget.second) {
        // 新しいターゲットが多数派になった場合に更新
        counter.mostCommonTarget = {calleeFunctionId, 1};
    }
}

void JITProfiler::RecordFunctionBytecodes(uint32_t functionId, const std::vector<uint8_t>& bytecodes) {
    if (!m_profilingEnabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 関数プロファイルが存在することを確認
    auto it = m_profileData.find(functionId);
    if (it == m_profileData.end()) return;
    
    // バイトコードのコピーを保存
    it->second->bytecodes = std::make_shared<std::vector<uint8_t>>(bytecodes);
    
    // ホットスポットの識別
    IdentifyHotSpots(*it->second);
}

const FunctionProfile* JITProfiler::GetFunctionProfile(uint32_t functionId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_profileData.find(functionId);
    if (it == m_profileData.end()) return nullptr;
    
    return it->second.get();
}

std::shared_ptr<std::vector<uint8_t>> JITProfiler::GetFunctionBytecodes(uint32_t functionId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_profileData.find(functionId);
    if (it == m_profileData.end() || !it->second->bytecodes) {
        return nullptr;
    }
    
    return it->second->bytecodes;
}

std::vector<const FunctionProfile*> JITProfiler::GetHotFunctions(size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<const FunctionProfile*> hotFunctions;
    hotFunctions.reserve(std::min(count, m_profileData.size()));
    
    // 各関数プロファイルを実行回数でソートするための一時配列
    std::vector<std::pair<uint32_t, const FunctionProfile*>> sortedProfiles;
    sortedProfiles.reserve(m_profileData.size());
    
    for (const auto& pair : m_profileData) {
        sortedProfiles.emplace_back(pair.second->totalExecutions, pair.second.get());
    }
    
    // 実行回数で降順ソート
    std::sort(sortedProfiles.begin(), sortedProfiles.end(), 
             [](const auto& a, const auto& b) { return a.first > b.first; });
    
    // 上位count個を結果に追加
    for (size_t i = 0; i < std::min(count, sortedProfiles.size()); ++i) {
        hotFunctions.push_back(sortedProfiles[i].second);
    }
    
    return hotFunctions;
}

std::vector<uint32_t> JITProfiler::GetHotSpots(uint32_t functionId, size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_profileData.find(functionId);
    if (it == m_profileData.end()) {
        return {};
    }
    
    const auto& hotSpots = it->second->hotSpots;
    
    // 要求された数のホットスポットを返す（最大で利用可能な数まで）
    return std::vector<uint32_t>(
        hotSpots.begin(),
        hotSpots.begin() + std::min(count, hotSpots.size())
    );
}

void JITProfiler::EnableProfiling(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profilingEnabled = enable;
}

void JITProfiler::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profileData.clear();
}

void JITProfiler::DumpProfileData(const std::string& outputPath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::ofstream outFile;
    std::ostream* out = &std::cout;
    
    if (!outputPath.empty()) {
        outFile.open(outputPath);
        if (outFile.is_open()) {
            out = &outFile;
        } else {
            std::cerr << "警告: 出力ファイルを開けませんでした: " << outputPath << std::endl;
        }
    }
    
    *out << "====== JITプロファイリングデータダンプ ======" << std::endl;
    *out << "関数の総数: " << m_profileData.size() << std::endl;
    *out << std::endl;
    
    // ホット関数を10個取得
    auto hotFunctions = GetHotFunctions(10);
    
    *out << "***** ホット関数TOP10 *****" << std::endl;
    for (size_t i = 0; i < hotFunctions.size(); ++i) {
        const auto* profile = hotFunctions[i];
        *out << "#" << (i + 1) << ": ";
        if (!profile->functionName.empty()) {
            *out << profile->functionName;
        } else {
            *out << "関数ID " << profile->functionId;
        }
        *out << " (" << profile->totalExecutions << "回実行)" << std::endl;
    }
    
    *out << std::endl << "***** 関数詳細 *****" << std::endl;
    
    // 各関数のさらに詳細な情報をダンプ
    for (const auto& pair : m_profileData) {
        const auto& profile = pair.second;
        
        *out << "----- ";
        if (!profile->functionName.empty()) {
            *out << profile->functionName;
        } else {
            *out << "関数ID " << profile->functionId;
        }
        *out << " -----" << std::endl;
        *out << "総実行回数: " << profile->totalExecutions << std::endl;
        
        // 型フィードバック情報
        if (!profile->typeFeedback.empty()) {
            *out << "変数型フィードバック:" << std::endl;
            for (size_t i = 0; i < profile->typeFeedback.size(); ++i) {
                const auto& record = profile->typeFeedback[i];
                if (record.observationCount > 0) {
                    *out << "  変数#" << i << ": " << record.GetCategoryName()
                         << " (観測回数: " << record.observationCount
                         << ", 信頼度: " << std::fixed << std::setprecision(2) << record.confidence << ")" << std::endl;
                }
            }
        }
        
        // ホットスポット情報
        if (!profile->hotSpots.empty()) {
            *out << "ホットスポット（バイトコードオフセット）:" << std::endl;
            for (auto offset : profile->hotSpots) {
                *out << "  オフセット " << offset << std::endl;
            }
        }
        
        *out << std::endl;
    }
    
    *out << "====== ダンプ終了 ======" << std::endl;
}

void JITProfiler::DumpFunctionProfile(uint32_t functionId, const std::string& outputPath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_profileData.find(functionId);
    if (it == m_profileData.end()) {
        std::cerr << "エラー: 関数ID " << functionId << " のプロファイルが見つかりませんでした。" << std::endl;
        return;
    }
    
    std::ofstream outFile;
    std::ostream* out = &std::cout;
    
    if (!outputPath.empty()) {
        outFile.open(outputPath);
        if (outFile.is_open()) {
            out = &outFile;
        } else {
            std::cerr << "警告: 出力ファイルを開けませんでした: " << outputPath << std::endl;
        }
    }
    
    const auto& profile = it->second;
    
    *out << "====== 関数プロファイルダンプ ======" << std::endl;
    *out << "関数ID: " << profile->functionId << std::endl;
    if (!profile->functionName.empty()) {
        *out << "関数名: " << profile->functionName << std::endl;
    }
    *out << "総実行回数: " << profile->totalExecutions << std::endl;
    *out << std::endl;
    
    // 型フィードバック情報
    if (!profile->typeFeedback.empty()) {
        *out << "***** 変数型フィードバック *****" << std::endl;
        for (size_t i = 0; i < profile->typeFeedback.size(); ++i) {
            const auto& record = profile->typeFeedback[i];
            if (record.observationCount > 0) {
                *out << "変数#" << i << ":" << std::endl;
                *out << "  型: " << record.GetCategoryName() << std::endl;
                *out << "  観測回数: " << record.observationCount << std::endl;
                *out << "  信頼度: " << std::fixed << std::setprecision(4) << record.confidence << std::endl;
                if (record.category == TypeFeedbackRecord::TypeCategory::Double) {
                    *out << "  -0.0観測: " << (record.hasNegativeZero ? "あり" : "なし") << std::endl;
                    *out << "  NaN観測: " << (record.hasNaN ? "あり" : "なし") << std::endl;
                }
                *out << std::endl;
            }
        }
    }
    
    // 算術演算の型フィードバック
    if (!profile->arithmeticOperations.empty()) {
        *out << "***** 算術演算型フィードバック *****" << std::endl;
        for (const auto& pair : profile->arithmeticOperations) {
            const auto& offset = pair.first;
            const auto& record = pair.second;
            *out << "バイトコードオフセット " << offset << ":" << std::endl;
            *out << "  型: " << record.GetCategoryName() << std::endl;
            *out << "  観測回数: " << record.observationCount << std::endl;
            *out << "  信頼度: " << std::fixed << std::setprecision(4) << record.confidence << std::endl;
            *out << std::endl;
        }
    }
    
    // 比較演算の型フィードバック
    if (!profile->comparisonOperations.empty()) {
        *out << "***** 比較演算型フィードバック *****" << std::endl;
        for (const auto& pair : profile->comparisonOperations) {
            const auto& offset = pair.first;
            const auto& record = pair.second;
            *out << "バイトコードオフセット " << offset << ":" << std::endl;
            *out << "  型: " << record.GetCategoryName() << std::endl;
            *out << "  観測回数: " << record.observationCount << std::endl;
            *out << "  信頼度: " << std::fixed << std::setprecision(4) << record.confidence << std::endl;
            *out << std::endl;
        }
    }
    
    // プロパティアクセス情報
    if (!profile->propertyAccesses.empty()) {
        *out << "***** プロパティアクセス情報 *****" << std::endl;
        for (const auto& pair : profile->propertyAccesses) {
            const auto& offset = pair.first;
            const auto& accessProfile = pair.second;
            *out << "バイトコードオフセット " << offset << ":" << std::endl;
            if (!accessProfile.propertyName.empty()) {
                *out << "  プロパティ名: " << accessProfile.propertyName << std::endl;
            }
            *out << "  アクセス回数: " << accessProfile.accessCount << std::endl;
            *out << "  最も一般的な形状ID: " << accessProfile.mostCommonShapeId << std::endl;
            *out << "  形状一貫性: " << std::fixed << std::setprecision(4) << accessProfile.shapeConsistency << std::endl;
            *out << "  形式: ";
            if (accessProfile.isMonomorphic) *out << "単形式";
            else if (accessProfile.isPolymorphic) *out << "多形式";
            else if (accessProfile.isMegamorphic) *out << "超多形式";
            else *out << "未知";
            *out << std::endl << std::endl;
        }
    }
    
    // ループ実行情報
    if (!profile->loopExecutionCounts.empty()) {
        *out << "***** ループ実行情報 *****" << std::endl;
        for (const auto& pair : profile->loopExecutionCounts) {
            const auto& offset = pair.first;
            const auto& counter = pair.second;
            *out << "ループヘッダーオフセット " << offset << ":" << std::endl;
            *out << "  実行回数: " << counter.executionCount << std::endl;
            *out << "  平均反復回数: " << std::fixed << std::setprecision(2) << counter.averageIterations << std::endl;
            *out << std::endl;
        }
    }
    
    // 呼び出しサイト情報
    if (!profile->callSiteExecutionCounts.empty()) {
        *out << "***** 呼び出しサイト情報 *****" << std::endl;
        for (const auto& pair : profile->callSiteExecutionCounts) {
            const auto& offset = pair.first;
            const auto& counter = pair.second;
            *out << "バイトコードオフセット " << offset << ":" << std::endl;
            *out << "  呼び出し回数: " << counter.executionCount << std::endl;
            *out << "  最も一般的なターゲット: 関数ID " << counter.mostCommonTarget.first
                 << " (" << counter.mostCommonTarget.second << "回呼び出し)" << std::endl;
            *out << std::endl;
        }
    }
    
    // ホットスポット情報
    if (!profile->hotSpots.empty()) {
        *out << "***** ホットスポット *****" << std::endl;
        for (size_t i = 0; i < profile->hotSpots.size(); ++i) {
            *out << "#" << (i+1) << ": バイトコードオフセット " << profile->hotSpots[i] << std::endl;
        }
        *out << std::endl;
    }
    
    *out << "====== ダンプ終了 ======" << std::endl;
}

void JITProfiler::SetupIRBuilderHooks(IRBuilder* irBuilder) {
    // IRビルダーに必要なフックを設定
    if (!irBuilder) return;
    
    // IRビルダーがフックをサポートする場合の実装
    // TODO: フックのセットアップコード
    // irBuilder->SetProfiler(this);
}

void JITProfiler::AttachToCompiler(JITCompiler* compiler) {
    // コンパイラにプロファイラを接続
    if (!compiler) return;
    
    // コンパイラがプロファイリングをサポートする場合の実装
    // TODO: コンパイラへの接続コード
    // compiler->SetProfiler(this);
}

std::vector<uint32_t> JITProfiler::GetFunctionsForOptimization(uint32_t threshold) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<uint32_t> functionsToOptimize;
    
    for (const auto& pair : m_profileData) {
        const auto& functionId = pair.first;
        const auto& profile = pair.second;
        
        // 実行回数が閾値を超えている関数を候補に
        if (profile->totalExecutions >= threshold) {
            functionsToOptimize.push_back(functionId);
        }
    }
    
    return functionsToOptimize;
}

std::vector<uint32_t> JITProfiler::GetFunctionsForDeoptimization() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 脱最適化すべき関数を特定するロジック
    // 現在は仮実装
    std::vector<uint32_t> functionsToDeoptimize;
    
    // TODO: 型の不安定性や予測が外れた関数を特定するロジック
    
    return functionsToDeoptimize;
}

void JITProfiler::UpdateTypeConfidence(TypeFeedbackRecord& record, TypeFeedbackRecord::TypeCategory newType) {
    // 初めての観測の場合
    if (record.observationCount == 1) {
        record.category = newType;
        record.confidence = 1.0f;
        return;
    }
    
    // 同じ型が観測された場合は信頼度を上げる
    if (record.category == newType) {
        // 単純な加重平均で信頼度を更新
        record.confidence = (record.confidence * (record.observationCount - 1) + 1.0f) / record.observationCount;
    } else {
        // 異なる型が観測された場合は信頼度を下げる
        record.confidence = (record.confidence * (record.observationCount - 1)) / record.observationCount;
        
        // 信頼度が50%を下回った場合は型をUnknownに変更
        if (record.confidence < 0.5f) {
            record.category = TypeFeedbackRecord::TypeCategory::Unknown;
        }
    }
}

void JITProfiler::UpdatePropertyAccessProfile(PropertyAccessProfile& profile, uint32_t shapeId) {
    // アクセスカウンターは既にインクリメント済み
    
    // 同じ形状IDが観測された場合
    if (profile.mostCommonShapeId == shapeId) {
        profile.shapeObservationCount++;
    } 
    // 異なる形状IDが観測された場合
    else if (profile.shapeObservationCount == 0 || profile.accessCount / 2 > profile.shapeObservationCount) {
        // 新しい形状IDが多数派になった場合に更新
        profile.mostCommonShapeId = shapeId;
        profile.shapeObservationCount = 1;
    }
    
    // 形状の一貫性を計算（多数派の形状IDの比率）
    profile.shapeConsistency = static_cast<float>(profile.shapeObservationCount) / profile.accessCount;
    
    // 形式分類を更新
    profile.isMonomorphic = profile.shapeConsistency > 0.95f;  // 95%以上が同じ形状ID
    profile.isPolymorphic = !profile.isMonomorphic && profile.shapeConsistency > 0.7f;  // 70-95%が同じ形状ID
    profile.isMegamorphic = profile.shapeConsistency < 0.5f;  // 50%未満が同じ形状ID
}

void JITProfiler::IdentifyHotSpots(FunctionProfile& profile, size_t count) {
    // ホットスポットを識別するための評価対象
    struct HotSpotCandidate {
        uint32_t offset;
        uint32_t heatScore;  // 実行頻度を表すスコア
    };
    
    std::vector<HotSpotCandidate> candidates;
    
    // ループヘッダーを追加
    for (const auto& pair : profile.loopExecutionCounts) {
        uint32_t offset = pair.first;
        const auto& counter = pair.second;
        
        // スコア = 実行回数 * 平均反復回数
        uint32_t score = static_cast<uint32_t>(counter.executionCount * counter.averageIterations);
        candidates.push_back({offset, score});
    }
    
    // コールサイトを追加
    for (const auto& pair : profile.callSiteExecutionCounts) {
        uint32_t offset = pair.first;
        const auto& counter = pair.second;
        
        // スコア = 呼び出し回数
        candidates.push_back({offset, counter.executionCount});
    }
    
    // 算術演算を追加
    for (const auto& pair : profile.arithmeticOperations) {
        uint32_t offset = pair.first;
        const auto& record = pair.second;
        
        // スコア = 観測回数
        candidates.push_back({offset, record.observationCount});
    }
    
    // スコアで降順ソート
    std::sort(candidates.begin(), candidates.end(),
             [](const HotSpotCandidate& a, const HotSpotCandidate& b) {
                 return a.heatScore > b.heatScore;
             });
    
    // ホットスポットリストをクリア
    profile.hotSpots.clear();
    
    // 上位count個を結果に追加
    for (size_t i = 0; i < std::min(count, candidates.size()); ++i) {
        profile.hotSpots.push_back(candidates[i].offset);
    }
}

float JITProfiler::CalculateTypeStability(const TypeFeedbackRecord& record) const {
    // 型の安定性を計算（信頼度と観測回数の組み合わせ）
    if (record.observationCount < 10) {
        // サンプル数が少ない場合は信頼度を割り引く
        return record.confidence * (static_cast<float>(record.observationCount) / 10.0f);
    }
    return record.confidence;
}

} // namespace aerojs::core 