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
    if (!m_enabled || !m_profilingEnabled) return;
    
    std::lock_guard<std::mutex> lock(m_profiler_mutex);
    if (m_currentFunctionId == 0) return;

    auto it = m_profileData.find(m_currentFunctionId);
    if (it == m_profileData.end() || !it->second) return;
    auto& profile = *(it->second);

    auto& feedback = profile.typeFeedback[bytecodeOffset];
    feedback.totalObservations++;
    TypeFeedbackRecord::TypeCategory observedCategory = TypeFeedbackRecord::TypeCategory::Unknown;

    if (value.isInt32()) {
        observedCategory = TypeFeedbackRecord::TypeCategory::Integer;
    } else if (value.isNumber()) {
        observedCategory = TypeFeedbackRecord::TypeCategory::Double;
        if (value.toNumber() == 0.0 && std::signbit(value.toNumber())) {
            feedback.hasNegativeZero = true;
        }
        if (std::isnan(value.toNumber())) {
            feedback.hasNaN = true;
        }
    } else if (value.isBoolean()) {
        observedCategory = TypeFeedbackRecord::TypeCategory::Boolean;
    } else if (value.isString()) {
        observedCategory = TypeFeedbackRecord::TypeCategory::String;
    } else if (value.isNull()) {
        observedCategory = TypeFeedbackRecord::TypeCategory::Null;
    } else if (value.isUndefined()) {
        observedCategory = TypeFeedbackRecord::TypeCategory::Undefined;
    } else if (value.isArray()) {
        observedCategory = TypeFeedbackRecord::TypeCategory::Array;
    } else if (value.isFunction()) {
        observedCategory = TypeFeedbackRecord::TypeCategory::Function;
    } else if (value.isObject()) {
        observedCategory = TypeFeedbackRecord::TypeCategory::Object;
    }

    if (feedback.category == TypeFeedbackRecord::TypeCategory::Unknown || feedback.category == observedCategory) {
        feedback.category = observedCategory;
        feedback.observationCount++;
    } else {
        feedback.category = TypeFeedbackRecord::TypeCategory::Mixed;
    }
    feedback.confidence = static_cast<float>(feedback.observationCount) / feedback.totalObservations;
}

void JITProfiler::RecordBranch(uint32_t bytecodeOffset, bool taken) {
    if (!m_enabled || !m_profilingEnabled) return;
    
    std::lock_guard<std::mutex> lock(m_profiler_mutex);
    if (m_currentFunctionId == 0) return;

    auto it = m_profileData.find(m_currentFunctionId);
    if (it == m_profileData.end() || !it->second) return;
    auto& profile = *(it->second);
    
    auto& branchRecord = profile.branchBias[bytecodeOffset];
    
    branchRecord.totalObservations++;
    if (taken) {
        branchRecord.takenCount++;
    } else {
        branchRecord.notTakenCount++;
    }
}

void JITProfiler::RecordLoopIteration(uint32_t loopHeaderOffset, uint32_t iterationCount) {
    if (!m_enabled || !m_profilingEnabled) return;
    
    std::lock_guard<std::mutex> lock(m_profiler_mutex);
    if (m_currentFunctionId == 0) return;

    auto it = m_profileData.find(m_currentFunctionId);
    if (it == m_profileData.end() || !it->second) return;
    auto& profile = *(it->second);
    
    auto& loopProfile = profile.loopExecutionCounts[loopHeaderOffset];
    
    loopProfile.recordIteration(iterationCount);
}

void JITProfiler::RecordPropertyAccess(uint32_t bytecodeOffset, const std::string& propertyName, Value value) {
    if (!m_enabled || !m_profilingEnabled) return;

    std::lock_guard<std::mutex> lock(m_profiler_mutex);
    if (m_currentFunctionId == 0) return;

    auto it = m_profileData.find(m_currentFunctionId);
    if (it == m_profileData.end() || !it->second) return;
    auto& profile = *(it->second);

    auto& accessProfile = profile.propertyAccesses[bytecodeOffset];
    accessProfile.accessCount++;
    if (!propertyName.empty()) {
        accessProfile.propertyName = propertyName;
    }

    uint32_t currentShapeId = 0;
    if (value.isObject() && value.toObject()) {
        // currentShapeId = value.toObject()->getShapeId();
    }

    if (accessProfile.shapeObservationCount == 0) {
        accessProfile.mostCommonShapeId = currentShapeId;
        accessProfile.isMonomorphic = true;
    } else if (accessProfile.mostCommonShapeId != currentShapeId) {
        accessProfile.isMonomorphic = false;
        accessProfile.isPolymorphic = true;
    }
    accessProfile.shapeObservationCount++;
    if (accessProfile.shapeObservationCount > 0) {
        if (accessProfile.isMonomorphic) accessProfile.shapeConsistency = 1.0f;
    }
}

const FunctionProfile* JITProfiler::getProfileFor(uint32_t functionId) const {
    std::lock_guard<std::mutex> lock(m_profiler_mutex);
    auto it = m_profileData.find(functionId);
    if (it != m_profileData.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::string JITProfiler::GetProfileSummary() const {
    std::lock_guard<std::mutex> lock(m_profiler_mutex);
    std::string summary = "JIT Profiler Summary:\n";
    summary += "Tracked Functions: " + std::to_string(m_profileData.size()) + "\n";
    for (const auto& pair : m_profileData) {
        const auto& funcProfile = pair.second;
        if (!funcProfile) continue;

        summary += "  Function ID: " + std::to_string(funcProfile->functionId);
        if (!funcProfile->functionName.empty()) {
            summary += " (Name: " + funcProfile->functionName + ")";
        }
        summary += ", Total Executions: " + std::to_string(funcProfile->totalExecutions) + "\n";

        if (!funcProfile->typeFeedback.empty()) {
            summary += "    Type Feedback Samples (first few):
";
            for (size_t i = 0; i < std::min(funcProfile->typeFeedback.size(), static_cast<size_t>(3)); ++i) {
                const auto& tf = funcProfile->typeFeedback[i];
                 summary += "      Offset " + std::to_string(i) +
                           ": Category=" + tf.GetCategoryName() +
                           ", Count=" + std::to_string(tf.observationCount) +
                           ", Total=" + std::to_string(tf.totalObservations) +
                           ", Confidence=" + std::to_string(tf.confidence) + "\n";
            }
        }
        if (!funcProfile->branchBias.empty()) {
            summary += "    Branch Bias Samples (first few):
";
            size_t count = 0;
            for (const auto& branch_pair : funcProfile->branchBias) {
                if (count++ >= 3) break;
                summary += "      Offset " + std::to_string(branch_pair.first) +
                           ": Taken=" + std::to_string(branch_pair.second.takenCount) +
                           ", NotTaken=" + std::to_string(branch_pair.second.notTakenCount) +
                           ", Total=" + std::to_string(branch_pair.second.totalObservations) + "\n";
            }
        }
    }
    return summary;
}

std::string TypeFeedbackRecord::GetCategoryName() const {
    switch (category) {
        case TypeCategory::Unknown: return "Unknown";
        case TypeCategory::Integer: return "Integer";
        case TypeCategory::Double: return "Double";
        case TypeCategory::Boolean: return "Boolean";
        case TypeCategory::String: return "String";
        case TypeCategory::Object: return "Object";
        case TypeCategory::Array: return "Array";
        case TypeCategory::Function: return "Function";
        case TypeCategory::Null: return "Null";
        case TypeCategory::Undefined: return "Undefined";
        case TypeCategory::Mixed: return "Mixed";
        default: return "InvalidCategory";
    }
}

} // namespace aerojs::core 