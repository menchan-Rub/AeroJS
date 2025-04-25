#include "deoptimizer.h"

#include <cassert>
#include <mutex>

namespace aerojs {
namespace core {

// スレッド安全のためのミューテックス
static std::mutex g_deoptMutex;

void Deoptimizer::RegisterDeoptPoint(void* codeAddress, const DeoptimizationInfo& info) noexcept {
  assert(codeAddress != nullptr && "Code address cannot be null");
  
  std::lock_guard<std::mutex> lock(g_deoptMutex);
  m_deoptInfoMap[codeAddress] = info;
}

bool Deoptimizer::PerformDeoptimization(void* codeAddress, DeoptimizationReason reason) noexcept {
  std::lock_guard<std::mutex> lock(g_deoptMutex);
  
  // デオプティマイズポイントの情報を取得
  auto it = m_deoptInfoMap.find(codeAddress);
  if (it == m_deoptInfoMap.end()) {
    // 登録されていないアドレスの場合、失敗
    return false;
  }
  
  // デオプティマイズ情報を取得
  const auto& info = it->second;
  
  // コールバックが設定されていれば呼び出す
  if (m_callback) {
    m_callback(info, reason);
  }
  
  return true;
}

void Deoptimizer::SetCallback(DeoptCallback callback) noexcept {
  std::lock_guard<std::mutex> lock(g_deoptMutex);
  m_callback = std::move(callback);
}

void Deoptimizer::UnregisterDeoptPoint(void* codeAddress) noexcept {
  std::lock_guard<std::mutex> lock(g_deoptMutex);
  m_deoptInfoMap.erase(codeAddress);
}

void Deoptimizer::ClearAllDeoptPoints() noexcept {
  std::lock_guard<std::mutex> lock(g_deoptMutex);
  m_deoptInfoMap.clear();
}

}  // namespace core
}  // namespace aerojs 