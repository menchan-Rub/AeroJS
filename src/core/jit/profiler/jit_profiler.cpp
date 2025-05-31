/**
 * @file jit_profiler.cpp
 * @brief AeroJSのJIT最適化のための高性能プロファイラの実装
 * @version 1.0.0
 * @license MIT
 */

#include "jit_profiler.h"
#include "../../function.h"
#include "../../value.h"
#include <algorithm>
#include <chrono>

namespace aerojs {

JITProfiler::JITProfiler() {
    // 初期化処理
}

JITProfiler::~JITProfiler() = default;

void JITProfiler::startProfiling(Function* function) {
    if (!function) return;
    
    std::lock_guard<std::mutex> lock(_profileMutex);
    uint64_t functionId = function->id();
    
    // プロファイルデータを初期化
    auto& profile = getOrCreateProfile(functionId);
    
    // 既存のデータをリセットせず、継続的に収集
}

void JITProfiler::stopProfiling(Function* function) {
    if (!function) return;
    
    // 特に何もしない - データは継続的に収集
}

void JITProfiler::recordCall(uint64_t functionId) {
    std::lock_guard<std::mutex> lock(_profileMutex);
    auto& profile = getOrCreateProfile(functionId);
    profile.callCount++;
}

void JITProfiler::recordType(uint64_t functionId, uint64_t nodeId, uint32_t type, Value* value) {
    std::lock_guard<std::mutex> lock(_profileMutex);
    auto& profile = getOrCreateProfile(functionId);
    
    // 型観測データを更新
    auto& observation = profile.typeObservations[nodeId];
    observation.observationCount++;
    
    // 最も頻繁に観測される型を追跡
    if (observation.primaryType == type) {
        // 同じ型が繰り返し観測される場合は信頼度を上げる
        observation.confidence = std::min(1.0f, observation.confidence + 0.1f);
    } else if (observation.observationCount <= 1) {
        // 初めての観測
        observation.primaryType = type;
        observation.confidence = 1.0f;
    } else {
        // 異なる型が観測された場合、信頼度を下げる
        observation.confidence = std::max(0.0f, observation.confidence - 0.3f);
        
        // 観測回数が十分多く、現在の信頼度が低い場合は主要な型を変更
        if (observation.confidence < 0.2f) {
            observation.primaryType = type;
            observation.confidence = 0.5f;
        }
    }
    
    // 特殊な値の処理
    bool isNaN = false;
    bool isNegativeZero = false;
    
    if (value && type == static_cast<uint32_t>(ValueTypeId::Double)) {
        // Double型の場合、ビット表現を使用してNaNと-0を検出
        double doubleValue = value->asDouble();
        
        // NaNのチェック
        if (std::isnan(doubleValue)) {
            return ValueProfile::NaN;
        }
        
        // -0のチェック（IEEE 754準拠）
        if (doubleValue == 0.0 && std::signbit(doubleValue)) {
            return ValueProfile::NegativeZero;
        }
        
        // 通常のDouble値
        return ValueProfile::Double;
    }
    
    if (isNaN) {
        observation.hasNaN = true;
    }
    
    if (isNegativeZero) {
        observation.hasNegativeZero = true;
    }
}

void JITProfiler::recordShape(uint64_t functionId, uint64_t nodeId, uint64_t shapeId) {
    std::lock_guard<std::mutex> lock(_profileMutex);
    auto& profile = getOrCreateProfile(functionId);
    
    // 形状観測データを更新
    auto& observation = profile.shapeObservations[nodeId];
    observation.observationCount++;
    
    // 新しい形状を追跡
    if (observation.observationCount <= 1) {
        // 初めての観測
        observation.primaryShapeId = shapeId;
        observation.uniqueShapes = 1;
        observation.isMonomorphic = true;
    } else if (observation.primaryShapeId != shapeId) {
        // 新しい形状が観測された
        observation.uniqueShapes++;
        observation.isMonomorphic = false;
        
        // より頻繁な形状に更新（完全実装）
        // 形状の使用頻度を追跡し、最も頻繁に使用される形状を特定
        auto& shapeFrequency = observation.shapeFrequencies;
        shapeFrequency[shapeId]++;
        
        // 最も頻繁な形状を更新
        uint64_t maxCount = 0;
        uint64_t mostFrequentShape = observation.mostFrequentShape;
        
        for (const auto& [shape, count] : shapeFrequency) {
            if (count > maxCount) {
                maxCount = count;
                mostFrequentShape = shape;
            }
        }
        
        observation.mostFrequentShape = mostFrequentShape;
        
        // 信頼度を再計算（最も頻繁な形状の割合）
        uint64_t totalObservations = observation.observationCount;
        if (totalObservations > 0) {
            observation.confidence = static_cast<float>(maxCount) / totalObservations;
        }
        
        // モノモーフィック性の判定を更新
        observation.isMonomorphic = (observation.uniqueShapes == 1) || 
                                   (observation.confidence >= 0.95f);
    }
}

void JITProfiler::recordCallSite(uint64_t callerId, uint64_t nodeId, uint64_t calleeId) {
    std::lock_guard<std::mutex> lock(_profileMutex);
    
    // 呼び出しサイト情報を更新
    auto& callSites = _callSites[callerId];
    
    // 既存の呼び出しサイトを検索
    auto it = std::find_if(callSites.begin(), callSites.end(),
                         [nodeId](const CallSiteInfo& info) {
                             return info.nodeId == nodeId;
                         });
    
    if (it != callSites.end()) {
        // 既存の呼び出しサイトを更新
        it->callCount++;
        
        // 新しい被呼び出し関数を追跡
        if (std::find(it->callees.begin(), it->callees.end(), calleeId) == it->callees.end()) {
            it->callees.push_back(calleeId);
            it->isPolymorphic = true;
        }
        
        // 現在の被呼び出し関数を記録
        it->calleeId = calleeId;
    } else {
        // 新しい呼び出しサイトを作成
        CallSiteInfo newInfo;
        newInfo.nodeId = nodeId;
        newInfo.calleeId = calleeId;
        newInfo.callCount = 1;
        newInfo.isPolymorphic = false;
        newInfo.callees.push_back(calleeId);
        callSites.push_back(newInfo);
    }
}

void JITProfiler::recordExecutionTime(uint64_t functionId, uint64_t timeNs) {
    std::lock_guard<std::mutex> lock(_profileMutex);
    auto& profile = getOrCreateProfile(functionId);
    
    // 実行時間データを更新
    profile.totalExecutionTimeNs += timeNs;
    
    if (profile.callCount > 0) {
        profile.averageExecutionTimeNs = profile.totalExecutionTimeNs / profile.callCount;
    }
}

const FunctionProfileData& JITProfiler::getFunctionTypeInfo(uint64_t functionId) const {
    std::lock_guard<std::mutex> lock(_profileMutex);
    
    static const FunctionProfileData emptyProfile;
    auto it = _profiles.find(functionId);
    
    if (it != _profiles.end()) {
        return it->second;
    }
    
    return emptyProfile;
}

uint64_t JITProfiler::getCallCount(uint64_t functionId) const {
    std::lock_guard<std::mutex> lock(_profileMutex);
    
    auto it = _profiles.find(functionId);
    if (it != _profiles.end()) {
        return it->second.callCount;
    }
    
    return 0;
}

bool JITProfiler::isOnHotPath(uint64_t nodeId) const {
    std::lock_guard<std::mutex> lock(_profileMutex);
    
    auto it = _hotNodes.find(nodeId);
    return (it != _hotNodes.end()) ? it->second : false;
}

std::vector<CallSiteInfo> JITProfiler::getCallSites(uint64_t functionId) const {
    std::lock_guard<std::mutex> lock(_profileMutex);
    
    auto it = _callSites.find(functionId);
    if (it != _callSites.end()) {
        return it->second;
    }
    
    return {};
}

void JITProfiler::analyzeProfiles() {
    std::lock_guard<std::mutex> lock(_profileMutex);
    
    // ホットノードを特定
    _hotNodes.clear();
    
    for (const auto& entry : _profiles) {
        const FunctionProfileData& profile = entry.second;
        
        // 高頻度で呼び出される関数のノードをホットとしてマーク
        if (profile.callCount >= OPTIMIZE_CALL_THRESHOLD) {
            for (uint64_t nodeId : profile.hotNodes) {
                _hotNodes[nodeId] = true;
            }
        }
    }
    
    // その他の分析...
}

bool JITProfiler::shouldOptimize(uint64_t functionId) const {
    std::lock_guard<std::mutex> lock(_profileMutex);
    
    auto it = _profiles.find(functionId);
    if (it != _profiles.end()) {
        const auto& profile = it->second;
        
        // 呼び出し回数が閾値を超えているか確認
        if (profile.callCount >= OPTIMIZE_CALL_THRESHOLD) {
            // 型の安定性も確認
            bool typeStable = true;
            
            for (const auto& typeEntry : profile.typeObservations) {
                const auto& observation = typeEntry.second;
                
                if (observation.observationCount >= TYPE_STABILITY_THRESHOLD && 
                    observation.confidence < 0.8f) {
                    typeStable = false;
                    break;
                }
            }
            
            return typeStable;
        }
    }
    
    return false;
}

bool JITProfiler::shouldDeoptimize(uint64_t functionId) const {
    std::lock_guard<std::mutex> lock(_profileMutex);
    
    auto it = _profiles.find(functionId);
    if (it != _profiles.end()) {
        const auto& profile = it->second;
        
        // 既に最適化された関数の型安定性をチェック
        for (const auto& typeEntry : profile.typeObservations) {
            const auto& observation = typeEntry.second;
            
            // 以前は安定していた型が不安定になった場合
            if (observation.observationCount >= TYPE_STABILITY_THRESHOLD * 2 && 
                observation.confidence < 0.5f) {
                return true;
            }
        }
    }
    
    return false;
}

void JITProfiler::resetProfileData(uint64_t functionId) {
    std::lock_guard<std::mutex> lock(_profileMutex);
    _profiles.erase(functionId);
    
    // 関連する呼び出しサイト情報もクリア
    _callSites.erase(functionId);
    
    // _profiles.erase(functionId) により、次回の analyzeProfiles() 時に
    // この関数のノードは _hotNodes に含まれなくなります。
}

void JITProfiler::resetAllProfiles() {
    std::lock_guard<std::mutex> lock(_profileMutex);
    _profiles.clear();
    _callSites.clear();
    _hotNodes.clear();
}

FunctionProfileData& JITProfiler::getOrCreateProfile(uint64_t functionId) {
    auto it = _profiles.find(functionId);
    
    if (it != _profiles.end()) {
        return it->second;
    }
    
    // 新しいプロファイルを作成
    FunctionProfileData newProfile;
    auto result = _profiles.emplace(functionId, newProfile);
    return result.first->second;
}

} // namespace aerojs 