/**
 * @file type_info.cpp
 * @brief 高性能なJIT最適化のための型情報収集・保持クラスの実装
 * @version 1.0.0
 * @license MIT
 */

#include "type_info.h"
#include "../../function.h"
#include "../../value.h"
#include "../jit_profiler.h"

namespace aerojs {

TypeInfo::TypeInfo()
    : _hasNumericLoops(false)
    , _hasStringOperations(false)
    , _hasPropertyAccesses(false) {
}

TypeInfo::TypeInfo(Function* function)
    : _hasNumericLoops(false)
    , _hasStringOperations(false)
    , _hasPropertyAccesses(false) {
    
    if (function) {
        gatherTypeInfo(function);
    }
}

TypeInfo::~TypeInfo() = default;

TypeId TypeInfo::getTypeFor(uint64_t nodeId) const {
    auto it = _typeMap.find(nodeId);
    return (it != _typeMap.end()) ? it->second : TypeId();
}

ShapeInfo TypeInfo::getPropertyShapeInfo(uint64_t nodeId) const {
    auto it = _shapeMap.find(nodeId);
    return (it != _shapeMap.end()) ? it->second : ShapeInfo();
}

bool TypeInfo::hasNumericLoops() const {
    return _hasNumericLoops;
}

bool TypeInfo::isLoopNumeric(uint64_t loopId) const {
    auto it = _numericLoops.find(loopId);
    return (it != _numericLoops.end()) ? it->second : false;
}

bool TypeInfo::hasStringOperations() const {
    return _hasStringOperations;
}

bool TypeInfo::isStringOperationOptimizable(uint64_t nodeId) const {
    auto it = _stringOps.find(nodeId);
    return (it != _stringOps.end()) ? it->second : false;
}

bool TypeInfo::hasPropertyAccesses() const {
    return _hasPropertyAccesses;
}

void TypeInfo::recordType(uint64_t nodeId, ValueTypeId type) {
    auto it = _typeMap.find(nodeId);
    
    if (it == _typeMap.end()) {
        // 新しい型記録を作成
        TypeId typeId;
        typeId.setType(type);
        typeId.setConfidence(1.0f);
        _typeMap[nodeId] = typeId;
    } else {
        // 既存の記録を更新
        TypeId& typeId = it->second;
        
        if (typeId.getType() == type) {
            // 同じ型が繰り返し観測されたら信頼度を上げる
            float confidence = typeId.getConfidence();
            confidence = std::min(1.0f, confidence + 0.1f);
            typeId.setConfidence(confidence);
        } else {
            // 異なる型が観測されたら混合型として記録し、信頼度を下げる
            typeId.setType(ValueTypeId::Mixed);
            float confidence = typeId.getConfidence();
            confidence = std::max(0.0f, confidence - 0.3f);
            typeId.setConfidence(confidence);
        }
    }
}

void TypeInfo::recordShape(uint64_t nodeId, uint64_t shapeId) {
    auto it = _shapeMap.find(nodeId);
    
    if (it == _shapeMap.end()) {
        // 新しい形状記録を作成
        ShapeInfo info;
        info.setMonomorphic(true);
        info.setShapeCount(1);
        info.setPrimaryShapeId(shapeId);
        _shapeMap[nodeId] = info;
        _hasPropertyAccesses = true;
    } else {
        // 既存の記録を更新
        ShapeInfo& info = it->second;
        
        if (info.primaryShapeId() == shapeId) {
            // 同じ形状が繰り返し観測された場合
            info.setMonomorphic(true);
        } else {
            // 異なる形状が観測された場合
            info.setMonomorphic(false);
            info.setShapeCount(info.shapeCount() + 1);
        }
    }
}

void TypeInfo::recordNumericLoop(uint64_t loopId) {
    _numericLoops[loopId] = true;
    _hasNumericLoops = true;
}

void TypeInfo::recordStringOperation(uint64_t nodeId) {
    _stringOps[nodeId] = true;
    _hasStringOperations = true;
}

void TypeInfo::analyze() {
    // 型情報の全体的な分析
    
    // 数値ループの検出
    for (const auto& entry : _typeMap) {
        uint64_t nodeId = entry.first;
        const TypeId& typeId = entry.second;
        
        // ループのインデックス変数として使われる値を検出
        if (typeId.getType() == ValueTypeId::Int32 && 
            typeId.getConfidence() > 0.8f) {
            // ループ制御変数の可能性があるノードを記録
            // 実際のループ検出はJITコンパイラが行う
        }
    }
    
    // 文字列操作の検出
    for (const auto& entry : _typeMap) {
        uint64_t nodeId = entry.first;
        const TypeId& typeId = entry.second;
        
        if (typeId.getType() == ValueTypeId::String && 
            typeId.getConfidence() > 0.7f) {
            recordStringOperation(nodeId);
        }
    }
}

void TypeInfo::gatherTypeInfo(Function* function) {
    // 関数からプロファイル情報を取得
    if (!function) return;
    
    // 通常は関数のJITプロファイラから型情報を収集
    JITProfiler* profiler = function->getContext()->getJITProfiler();
    if (!profiler) return;
    
    // プロファイラから型情報を転送
    const auto& profileTypeInfo = profiler->getFunctionTypeInfo(function->id());
    
    // プロファイラの型情報をこのTypeInfoインスタンスにコピー
    for (const auto& entry : profileTypeInfo.typeObservations) {
        uint64_t nodeId = entry.first;
        ValueTypeId type = static_cast<ValueTypeId>(entry.second.primaryType);
        float confidence = entry.second.confidence;
        
        TypeId typeId;
        typeId.setType(type);
        typeId.setConfidence(confidence);
        _typeMap[nodeId] = typeId;
    }
    
    // 形状情報をコピー
    for (const auto& entry : profileTypeInfo.shapeObservations) {
        uint64_t nodeId = entry.first;
        const auto& shapeData = entry.second;
        
        ShapeInfo shapeInfo;
        shapeInfo.setMonomorphic(shapeData.isMonomorphic);
        shapeInfo.setShapeCount(shapeData.uniqueShapes);
        shapeInfo.setPrimaryShapeId(shapeData.primaryShapeId);
        _shapeMap[nodeId] = shapeInfo;
    }
    
    // ループ情報をコピー
    for (uint64_t loopId : profileTypeInfo.numericLoops) {
        recordNumericLoop(loopId);
    }
    
    // 文字列操作情報をコピー
    for (uint64_t nodeId : profileTypeInfo.stringOperations) {
        recordStringOperation(nodeId);
    }
    
    // 情報をさらに分析
    analyze();
}

} // namespace aerojs 