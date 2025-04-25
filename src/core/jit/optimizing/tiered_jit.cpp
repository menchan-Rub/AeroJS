#include "tiered_jit.h"

#include <algorithm>
#include <thread>
#include <chrono>
#include <xxhash.h>

#include "../memory_manager.h"
#include "../code_cache.h"
#include "optimizing_jit.h"
#include "super_optimizing_jit.h"
#include "../backend/x86_64/x86_64_code_generator.h"
#include "../deoptimizer/deoptimizer.h"

namespace aerojs {
namespace core {

TieredJIT::TieredJIT() noexcept 
  : m_irBuilder(),
    m_irOptimizer() {
  // プロファイラの初期化
  ExecutionProfiler::Instance().Reset();
  
  // デオプティマイザのコールバック設定
  Deoptimizer::Instance().SetCallback([](const DeoptimizationInfo& info, DeoptimizationReason reason) {
    // デオプティマイズ時にインタープリタモードに切り替え
    VMContext::Instance().SwitchToInterpreterMode(info.functionId, info.bytecodeOffset);
    
    // ホットパスの再プロファイリングをトリガー
    if (reason == DeoptimizationReason::TypeInstability) {
      ExecutionProfiler::Instance().MarkForReoptimization(info.functionId);
    }
  });
}

TieredJIT::~TieredJIT() noexcept {
  m_stop.store(true);
  
  // 最適化スレッドの終了を待機
  if (m_optimizationThread.joinable()) {
    m_optimizationThread.join();
  }
}

std::unique_ptr<uint8_t[]> TieredJIT::Compile(const std::vector<uint8_t>& bytecodes,
                                             size_t& outCodeSize) noexcept {
  // バイトコード長チェック
  if (bytecodes.empty()) {
    outCodeSize = 0;
    return nullptr;
  }
  
  // ハッシュ計算
  size_t key = ComputeHash(bytecodes);
  
  // 関数IDを取得または生成
  uint32_t functionId = GetOrCreateFunctionId(bytecodes);
  
  // プロファイリング情報の取得
  const ProfileData* profileData = ExecutionProfiler::Instance().GetProfileData(functionId);
  
  // キャッシュから最適化版を取得
  if (auto cached = CodeCache::Instance().Lookup(key)) {
    const auto& buf = *cached;
    outCodeSize = buf.size();
    auto codeBuf = std::make_unique<uint8_t[]>(outCodeSize);
    std::copy(buf.begin(), buf.end(), codeBuf.get());
    
    // 実行開始をマーク
    ExecutionProfiler::Instance().RecordFunctionEntry(functionId);
    
    return codeBuf;
  }
  
  // プロファイリング情報に基づいて最適化レベルを選択
  bool needsOptimization = false;
  if (profileData && profileData->executionCount > OPTIMIZATION_THRESHOLD) {
    needsOptimization = true;
  }
  
  // 最適化判断
  if (needsOptimization) {
    // バックグラウンドで最適化を開始
    StartOptimizationThread(bytecodes, key, functionId);
  }
  
  // 最適化版が利用できるまでのフォールバックとしてベースライン版を返却
  auto baseCode = m_baseline.Compile(bytecodes, outCodeSize);
  
  // 実行開始をマーク
  ExecutionProfiler::Instance().RecordFunctionEntry(functionId);
  
  return baseCode;
}

void TieredJIT::Reset() noexcept {
  m_stop.store(true);
  
  // 最適化スレッドの終了を待機
  if (m_optimizationThread.joinable()) {
    m_optimizationThread.join();
  }
  
  // 各コンポーネントのリセット
  m_baseline.Reset();
  m_irBuilder.Reset();
  m_irOptimizer.Reset();
  
  // 関数IDマップのクリア
  std::lock_guard<std::mutex> lock(m_mapMutex);
  m_bytecodeToFunctionId.clear();
  m_nextFunctionId.store(1);
  
  // プロファイラとデオプティマイザのリセット
  ExecutionProfiler::Instance().Reset();
  Deoptimizer::Instance().ClearAllDeoptPoints();
  
  // 最適化スレッドの再起動フラグをリセット
  m_stop.store(false);
}

size_t TieredJIT::ComputeHash(const std::vector<uint8_t>& bytecodes) const noexcept {
  // xxHash を使用した高速なハッシュ計算
  return XXH64(bytecodes.data(), bytecodes.size(), 0x42);
}

uint32_t TieredJIT::GetOrCreateFunctionId(const std::vector<uint8_t>& bytecodes) noexcept {
  size_t hash = ComputeHash(bytecodes);
  
  {
    std::lock_guard<std::mutex> lock(m_mapMutex);
    
    auto it = m_bytecodeToFunctionId.find(hash);
    if (it != m_bytecodeToFunctionId.end()) {
      return it->second;
    }
    
    // 新しい関数IDを生成
    uint32_t newId = m_nextFunctionId.fetch_add(1);
    m_bytecodeToFunctionId[hash] = newId;
    return newId;
  }
}

void TieredJIT::StartOptimizationThread(const std::vector<uint8_t>& bytecodes, 
                                       size_t key, 
                                       uint32_t functionId) noexcept {
  // コピーを作成してスレッドに渡す
  std::vector<uint8_t> bcCopy = bytecodes;
  
  // 既存の最適化スレッドが実行中なら終了を待機
  if (m_optimizationThread.joinable()) {
    m_optimizationThread.join();
  }
  
  // 最適化スレッドを開始
  m_optimizationThread = std::thread([this, bc = std::move(bcCopy), key, functionId]() {
    // スレッド停止フラグのチェック
    if (m_stop.load()) return;
    
    // プロファイルデータの取得
    const ProfileData* profileData = ExecutionProfiler::Instance().GetProfileData(functionId);
    
    // 最適化レベルの決定
    IROptimizer::OptimizationLevel level = IROptimizer::OptimizationLevel::Basic;
    
    if (profileData) {
      if (profileData->executionCount > SUPER_OPT_THRESHOLD) {
        level = IROptimizer::OptimizationLevel::Aggressive;
      } else if (profileData->executionCount > OPTIMIZATION_THRESHOLD) {
        level = IROptimizer::OptimizationLevel::Medium;
      }
    }
    
    try {
      // IRの生成
      auto irFunction = m_irBuilder.BuildIR(bc, functionId);
      
      // IRの最適化
      auto optimizedIR = m_irOptimizer.Optimize(std::move(irFunction), functionId, level);
      
      // 最適化がとても積極的な場合はSuperOptimizingJITを使用
      if (level == IROptimizer::OptimizationLevel::Aggressive && 
          profileData && profileData->isTypeStable) {
        SuperOptimizingJIT superJit;
        size_t optSize;
        auto optPtr = superJit.Compile(bc, optSize);
        
        if (optPtr && optSize > 0) {
          std::vector<uint8_t> optBuf(optPtr.get(), optPtr.get() + optSize);
          CodeCache::Instance().Insert(key, optBuf);
          
          // 超最適化コードの生成を記録
          ExecutionProfiler::Instance().RecordSuperOptimization(functionId);
        }
      } else {
        // バックエンドコード生成
        X86_64CodeGenerator codeGen;
        std::vector<uint8_t> machinecode;
        codeGen.Generate(*optimizedIR, machinecode);
        
        // キャッシュに登録
        if (!machinecode.empty()) {
          CodeCache::Instance().Insert(key, machinecode);
          
          // 最適化コードの生成を記録
          ExecutionProfiler::Instance().RecordOptimization(functionId, level);
        }
      }
    } catch (const std::exception& e) {
      // 最適化エラーをログに記録
      Logger::Error("最適化エラー（関数ID: %u）: %s", functionId, e.what());
    } catch (...) {
      // 未知のエラーをログに記録
      Logger::Error("未知の最適化エラー（関数ID: %u）", functionId);
    }
  });
}

}  // namespace core
}  // namespace aerojs