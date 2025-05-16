/**
 * @file profiler.cpp
 * @brief AeroJS JavaScript エンジンのJITプロファイラの実装
 * @version 1.0.0
 * @license MIT
 */

#include "profiler.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace aerojs {
namespace core {

// コンストラクタ
JITProfiler::JITProfiler(Context* context)
    : m_context(context)
{
    // プロファイラ初期化処理
}

// デストラクタ
JITProfiler::~JITProfiler()
{
    // クリーンアップ処理
}

// 関数の実行を記録
void JITProfiler::recordFunctionExecution(uint64_t functionId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& profile = getOrCreateFunctionProfile(functionId);
    profile.executionCount++;
}

// 関数のJITコンパイルを記録
void JITProfiler::recordJITCompilation(uint64_t functionId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& profile = getOrCreateFunctionProfile(functionId);
    profile.jitCompilationCount++;
}

// 関数の最適化解除を記録
void JITProfiler::recordDeoptimization(uint64_t functionId, DeoptimizationReason reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& profile = getOrCreateFunctionProfile(functionId);
    profile.deoptimizationCount++;
    
    // 追加の統計処理やロギングをここで行うことができる
}

// バイトコードの実行を記録
void JITProfiler::recordBytecodeExecution(uint64_t functionId, uint32_t bytecodeOffset)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& profile = getOrCreateFunctionProfile(functionId);
    profile.bytecodeHeatmap[bytecodeOffset]++;
}

// 値の型を記録
void JITProfiler::recordValueType(uint64_t functionId, uint32_t variableId, const Value& value, bool isParameter)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& profile = getOrCreateFunctionProfile(functionId);
    
    auto& typeProfiles = isParameter ? profile.parameterProfiles : profile.variableProfiles;
    auto& typeProfile = typeProfiles[variableId];
    
    // サンプル数を増やす
    typeProfile.sampleCount++;
    
    // 値の型を決定
    auto valueType = determineValueType(value);
    
    // 型の安定性を更新
    if (typeProfile.sampleCount == 1) {
        // 最初のサンプルの場合、支配的な型として設定
        typeProfile.dominantType = valueType;
        typeProfile.stability = 1.0;
    } else {
        // サンプルが複数ある場合、安定性を更新
        if (typeProfile.dominantType == valueType) {
            // 同じ型が続く場合、安定性を向上
            typeProfile.stability = (typeProfile.stability * (typeProfile.sampleCount - 1) + 1.0) / typeProfile.sampleCount;
        } else {
            // 異なる型の場合、安定性を低下
            typeProfile.stability = (typeProfile.stability * (typeProfile.sampleCount - 1)) / typeProfile.sampleCount;
            
            // より頻度の高い型があれば、支配的な型を変更
            if (typeProfile.stability < 0.5) {
                typeProfile.dominantType = valueType;
            }
        }
    }
    
    // オブジェクト形状や関数ターゲットの安定性を更新
    // 実際の実装ではここに追加の処理が必要
}

// ホットループを記録
void JITProfiler::recordHotLoop(uint64_t functionId, uint32_t bytecodeOffset, uint32_t iterationCount)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& profile = getOrCreateFunctionProfile(functionId);
    
    // ループが既に記録されているか確認
    auto it = std::find(profile.hotLoops.begin(), profile.hotLoops.end(), bytecodeOffset);
    if (it == profile.hotLoops.end()) {
        // 新しいホットループとして追加
        profile.hotLoops.push_back(bytecodeOffset);
    }
    
    // ヒートマップも更新
    profile.bytecodeHeatmap[bytecodeOffset] += iterationCount;
}

// 関数バイトコードを記録
void JITProfiler::recordFunctionBytecodes(uint64_t functionId, const std::vector<uint8_t>& bytecodes)
{
    // このメソッドは必要に応じて実装
    // バイトコードの統計情報を収集する場合に使用
}

// 関数の実行回数を取得
uint32_t JITProfiler::getFunctionExecutionCount(uint64_t functionId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_functionProfiles.find(functionId);
    if (it != m_functionProfiles.end()) {
        return it->second.executionCount;
    }
    return 0;
}

// 関数のプロファイル情報を取得
void* JITProfiler::getFunctionProfile(uint64_t functionId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_functionProfiles.find(functionId);
    if (it != m_functionProfiles.end()) {
        return const_cast<FunctionProfile*>(&it->second);
    }
    return nullptr;
}

// プロファイル情報をリセット
void JITProfiler::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_functionProfiles.clear();
}

// プロファイル情報をダンプ
std::string JITProfiler::dumpProfiles() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::stringstream ss;
    
    ss << "=== JIT Profiler Statistics ===" << std::endl;
    ss << "Total profiled functions: " << m_functionProfiles.size() << std::endl;
    
    for (const auto& entry : m_functionProfiles) {
        const auto& profile = entry.second;
        ss << "Function ID: " << profile.functionId << std::endl;
        ss << "  Execution count: " << profile.executionCount << std::endl;
        ss << "  JIT compilation count: " << profile.jitCompilationCount << std::endl;
        ss << "  Deoptimization count: " << profile.deoptimizationCount << std::endl;
        
        ss << "  Parameter profiles: " << profile.parameterProfiles.size() << std::endl;
        for (const auto& param : profile.parameterProfiles) {
            ss << "    Param " << param.first << ": ";
            ss << "Type=" << static_cast<int>(param.second.dominantType);
            ss << ", Stability=" << std::fixed << std::setprecision(2) << param.second.stability;
            ss << ", Samples=" << param.second.sampleCount << std::endl;
        }
        
        ss << "  Variable profiles: " << profile.variableProfiles.size() << std::endl;
        for (const auto& var : profile.variableProfiles) {
            ss << "    Var " << var.first << ": ";
            ss << "Type=" << static_cast<int>(var.second.dominantType);
            ss << ", Stability=" << std::fixed << std::setprecision(2) << var.second.stability;
            ss << ", Samples=" << var.second.sampleCount << std::endl;
        }
        
        ss << "  Hot loops: " << profile.hotLoops.size() << std::endl;
        for (const auto& loop : profile.hotLoops) {
            ss << "    Offset " << loop << std::endl;
        }
        
        ss << "  Top bytecode hotspots: " << std::endl;
        std::vector<std::pair<uint32_t, uint32_t>> hotspots;
        for (const auto& bc : profile.bytecodeHeatmap) {
            hotspots.push_back(bc);
        }
        
        // ヒットカウントで降順ソート
        std::sort(hotspots.begin(), hotspots.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        // 上位5つのホットスポットを表示
        for (size_t i = 0; i < std::min(size_t(5), hotspots.size()); ++i) {
            ss << "    Offset " << hotspots[i].first << ": " << hotspots[i].second << " hits" << std::endl;
        }
        
        ss << std::endl;
    }
    
    return ss.str();
}

// 関数プロファイルを取得または作成
FunctionProfile& JITProfiler::getOrCreateFunctionProfile(uint64_t functionId)
{
    auto it = m_functionProfiles.find(functionId);
    if (it == m_functionProfiles.end()) {
        auto result = m_functionProfiles.emplace(functionId, FunctionProfile(functionId));
        return result.first->second;
    }
    return it->second;
}

// 値の型を決定
TypeProfile::ValueType JITProfiler::determineValueType(const Value& value) const
{
    // このメソッドでは、Valueオブジェクトの型を判定し、TypeProfile::ValueTypeに変換
    // 実際の実装では、ValueクラスのAPI（getType()やisNumber()など）を使用する
    
    // ダミー実装 - 実際の実装では適切なValue型チェックが必要
    return TypeProfile::ValueType::Unknown;
}

} // namespace core
} // namespace aerojs 