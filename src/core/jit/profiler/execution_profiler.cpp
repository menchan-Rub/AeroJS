#include "execution_profiler.h"

#include <algorithm>
#include <chrono>

namespace aerojs {
namespace core {

uint64_t ExecutionProfiler::RecordFunctionEntry(uint32_t functionId) noexcept {
  // 現在時刻をナノ秒単位で取得
  auto now = std::chrono::high_resolution_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                      now.time_since_epoch()).count();
  
  std::lock_guard<std::mutex> lock(m_mutex);
  
  // 必要に応じてプロファイルデータを初期化
  if (m_profileData.find(functionId) == m_profileData.end()) {
    m_profileData[functionId] = ProfileData{};
  }
  
  // 実行回数をインクリメント
  m_profileData[functionId].executionCount++;
  
  return timestamp;
}

void ExecutionProfiler::RecordFunctionExit(uint32_t functionId, 
                                         uint64_t entryTimestamp, 
                                         uint32_t returnTypeId) noexcept {
  // 現在時刻をナノ秒単位で取得
  auto now = std::chrono::high_resolution_clock::now();
  auto exitTimestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         now.time_since_epoch()).count();
  
  // 実行時間を計算
  uint64_t executionTime = exitTimestamp - entryTimestamp;
  
  std::lock_guard<std::mutex> lock(m_mutex);
  
  // データが存在することを確認（RecordFunctionEntryが先に呼ばれているはず）
  auto it = m_profileData.find(functionId);
  if (it == m_profileData.end()) {
    return;  // データが見つからない場合は何もしない
  }
  
  // 実行時間を累積
  it->second.totalExecutionTime += executionTime;
  
  // 戻り値の型を記録
  bool typeFound = false;
  for (auto& typeInfo : it->second.typeHistory) {
    if (typeInfo.typeId == returnTypeId) {
      typeInfo.frequency++;
      typeFound = true;
      break;
    }
  }
  
  if (!typeFound) {
    it->second.typeHistory.push_back({returnTypeId, 1});
  }
  
  // 最適化ステータスを更新
  UpdateOptimizationStatus(functionId);
}

void ExecutionProfiler::RecordArgumentType(uint32_t functionId, 
                                         uint32_t argIndex, 
                                         uint32_t typeId) noexcept {
  std::lock_guard<std::mutex> lock(m_mutex);
  
  // データが存在することを確認
  auto it = m_profileData.find(functionId);
  if (it == m_profileData.end()) {
    return;  // データが見つからない場合は何もしない
  }
  
  // 型履歴にユニークIDを生成（引数インデックスとタイプIDを組み合わせる）
  uint32_t uniqueTypeId = (argIndex << 16) | typeId;
  
  // 引数の型を記録
  bool typeFound = false;
  for (auto& typeInfo : it->second.typeHistory) {
    if (typeInfo.typeId == uniqueTypeId) {
      typeInfo.frequency++;
      typeFound = true;
      break;
    }
  }
  
  if (!typeFound) {
    it->second.typeHistory.push_back({uniqueTypeId, 1});
  }
}

void ExecutionProfiler::RecordBranch(uint32_t functionId, 
                                   uint32_t branchId, 
                                   bool taken) noexcept {
  std::lock_guard<std::mutex> lock(m_mutex);
  
  // データが存在することを確認
  auto it = m_profileData.find(functionId);
  if (it == m_profileData.end()) {
    return;  // データが見つからない場合は何もしない
  }
  
  // 分岐履歴を更新
  bool branchFound = false;
  for (auto& branchInfo : it->second.branchHistory) {
    if (branchInfo.branchId == branchId) {
      if (taken) {
        branchInfo.takenCount++;
      } else {
        branchInfo.notTakenCount++;
      }
      branchFound = true;
      break;
    }
  }
  
  if (!branchFound) {
    ProfileData::BranchInfo newBranch;
    newBranch.branchId = branchId;
    newBranch.takenCount = taken ? 1 : 0;
    newBranch.notTakenCount = taken ? 0 : 1;
    it->second.branchHistory.push_back(newBranch);
  }
  
  // 分岐バイアスを再計算
  UpdateOptimizationStatus(functionId);
}

const ProfileData* ExecutionProfiler::GetProfileData(uint32_t functionId) const noexcept {
  std::lock_guard<std::mutex> lock(m_mutex);
  
  auto it = m_profileData.find(functionId);
  if (it == m_profileData.end()) {
    return nullptr;
  }
  
  return &(it->second);
}

bool ExecutionProfiler::IsFunctionHot(uint32_t functionId) const noexcept {
  std::lock_guard<std::mutex> lock(m_mutex);
  
  auto it = m_profileData.find(functionId);
  if (it == m_profileData.end()) {
    return false;
  }
  
  return it->second.isHot;
}

bool ExecutionProfiler::IsFunctionTypeStable(uint32_t functionId) const noexcept {
  std::lock_guard<std::mutex> lock(m_mutex);
  
  auto it = m_profileData.find(functionId);
  if (it == m_profileData.end()) {
    return false;
  }
  
  return it->second.isStable;
}

void ExecutionProfiler::Reset() noexcept {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_profileData.clear();
}

void ExecutionProfiler::UpdateOptimizationStatus(uint32_t functionId) noexcept {
  auto it = m_profileData.find(functionId);
  if (it == m_profileData.end()) {
    return;
  }
  
  ProfileData& data = it->second;
  
  // ホット関数の判定
  data.isHot = (data.executionCount >= HOT_FUNCTION_THRESHOLD);
  
  // 型安定性の計算
  if (!data.typeHistory.empty()) {
    // 各型の出現頻度を合計
    uint32_t totalFreq = 0;
    uint32_t maxFreq = 0;
    uint32_t maxTypeId = 0;
    
    for (const auto& typeInfo : data.typeHistory) {
      totalFreq += typeInfo.frequency;
      if (typeInfo.frequency > maxFreq) {
        maxFreq = typeInfo.frequency;
        maxTypeId = typeInfo.typeId;
      }
    }
    
    // 最も頻度の高い型の割合を計算
    if (totalFreq > 0) {
      data.typeStability = (maxFreq * 100) / totalFreq;
    }
    
    // 型安定性の判定
    data.isStable = (data.typeStability >= TYPE_STABILITY_THRESHOLD);
  }
  
  // 分岐バイアスの計算
  if (!data.branchHistory.empty()) {
    // 各分岐のバイアスを計算し、平均を取る
    uint32_t totalBias = 0;
    uint32_t branchCount = 0;
    
    for (const auto& branchInfo : data.branchHistory) {
      uint32_t total = branchInfo.takenCount + branchInfo.notTakenCount;
      if (total > 0) {
        // どちらのケースが多いかを判断
        uint32_t majority = std::max(branchInfo.takenCount, branchInfo.notTakenCount);
        uint32_t bias = (majority * 100) / total;
        totalBias += bias;
        branchCount++;
      }
    }
    
    // 平均分岐バイアスを計算
    if (branchCount > 0) {
      data.branchBias = totalBias / branchCount;
    }
    
    // 分岐バイアスの判定
    data.hasBranchBias = (data.branchBias >= BRANCH_BIAS_THRESHOLD);
  }
}

}  // namespace core
}  // namespace aerojs 