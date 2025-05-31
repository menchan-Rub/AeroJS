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
    // 関数・オブジェクトの実行パスや最適化ヒントを記録する本格実装
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
    // ValueクラスのAPI（getType()やisNumber()など）を使用して型を判別
    if (value.isNumber()) {
        // 数値の詳細型判定
        double d = value.asDouble();
        
        // NaNチェック
        if (std::isnan(d)) {
            return TypeProfile::ValueType::NaN;
        }
        
        // 整数値チェック
        if (std::trunc(d) == d) {
            if (d >= INT_MIN && d <= INT_MAX) {
                return TypeProfile::ValueType::Integer;
            }
            return TypeProfile::ValueType::BigInteger;
        }
        
        // 特殊浮動小数点値チェック
        if (std::isinf(d)) {
            return TypeProfile::ValueType::Infinity;
        }
        
        return TypeProfile::ValueType::Float;
    }
    
    // 文字列型
    if (value.isStringAny()) {
        if (value.isSmallString()) {
            // スモールストリング最適化されたもの
            uint8_t length = value.getSmallStringLength();
            if (length == 0) {
                return TypeProfile::ValueType::EmptyString;
            }
            return TypeProfile::ValueType::SmallString;
        } else if (value.isString()) {
            return TypeProfile::ValueType::String;
        }
    }
    
    // オブジェクト型の詳細分類
    if (value.isObjectAny()) {
        if (value.isSmallObject()) {
            return TypeProfile::ValueType::SmallObject;
        }
        
        // オブジェクトポインタを取得してより詳細な型情報を抽出
        JSObject* obj = value.asObject();
        if (!obj) {
            return TypeProfile::ValueType::Object;
        }
        
        // オブジェクトの型を判別
        uint32_t typeId = obj->getTypeId();
        switch (typeId) {
            case JSObject::TypeId::ARRAY:
                return TypeProfile::ValueType::Array;
            case JSObject::TypeId::FUNCTION:
                return TypeProfile::ValueType::Function;
            case JSObject::TypeId::REGEXP:
                return TypeProfile::ValueType::RegExp;
            case JSObject::TypeId::DATE:
                return TypeProfile::ValueType::Date;
            case JSObject::TypeId::PROMISE:
                return TypeProfile::ValueType::Promise;
            case JSObject::TypeId::MAP:
                return TypeProfile::ValueType::Map;
            case JSObject::TypeId::SET:
                return TypeProfile::ValueType::Set;
            case JSObject::TypeId::WEAK_MAP:
                return TypeProfile::ValueType::WeakMap;
            case JSObject::TypeId::WEAK_SET:
                return TypeProfile::ValueType::WeakSet;
            case JSObject::TypeId::ARRAY_BUFFER:
                return TypeProfile::ValueType::ArrayBuffer;
            case JSObject::TypeId::TYPED_ARRAY:
                return TypeProfile::ValueType::TypedArray;
            default:
                return TypeProfile::ValueType::Object;
        }
    }
    
    // その他のプリミティブ型
    if (value.isBoolean()) {
        return TypeProfile::ValueType::Boolean;
    }
    
    if (value.isNull()) {
        return TypeProfile::ValueType::Null;
    }
    
    if (value.isUndefined()) {
        return TypeProfile::ValueType::Undefined;
    }
    
    if (value.isSymbol()) {
        return TypeProfile::ValueType::Symbol;
    }
    
    if (value.isBigInt()) {
        return TypeProfile::ValueType::BigInt;
    }
    
    // 未知の型
    return TypeProfile::ValueType::Unknown;
}

} // namespace core
} // namespace aerojs 