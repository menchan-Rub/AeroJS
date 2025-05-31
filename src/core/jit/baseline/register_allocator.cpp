#include "register_allocator.h"
#include <algorithm>
#include <cassert>
#include <limits>
#include <iostream>

namespace aerojs {
namespace core {

// スピルされたレジスタや未割り当てのレジスタを表す定数
static constexpr uint8_t kInvalidRegister = 0xFF;

// コンストラクタ
RegisterAllocator::RegisterAllocator(RegisterAllocationStrategy strategy) noexcept
    : m_strategy(strategy),
      m_nextVirtualRegId(1),  // 0は無効な仮想レジスタとして予約
      m_nextSpillSlot(0) {
  // 物理レジスタの初期化
  InitializePhysicalRegisters();
}

// デストラクタ
RegisterAllocator::~RegisterAllocator() noexcept {
  // リソースの解放が必要な場合に実装
}

void RegisterAllocator::SetAvailableRegisters(const std::vector<Register>& registers) {
    m_availableRegisters = registers;
}

// 新しい仮想レジスタを割り当てる
uint32_t RegisterAllocator::AllocateVirtualRegister(PhysicalRegisterType type) noexcept {
  uint32_t newId = m_nextVirtualRegId++;
  
  // 新しい仮想レジスタを作成
  VirtualRegister vreg;
  vreg.id = newId;
  vreg.type = type;
  vreg.liveRangeStart = -1;  // 生存範囲は未設定
  vreg.liveRangeEnd = -1;    // 生存範囲は未設定
  vreg.physicalRegId = std::numeric_limits<uint32_t>::max();  // 未割り当て
  vreg.isSpilled = false;
  vreg.spillSlot = -1;
  
  // 仮想レジスタを登録
  m_virtualRegisters[newId] = vreg;
  
  return newId;
}

// 仮想レジスタに物理レジスタを割り当てる
bool RegisterAllocator::AllocateRegisters(const std::vector<uint32_t>& instructions) noexcept {
  // リセットして初期状態に戻す
  m_usedPhysicalRegs.clear();
  
  // 生存範囲分析を実行
  AnalyzeLiveRanges(instructions);
  
  // 選択された戦略に基づいてレジスタを割り当てる
  switch (m_strategy) {
    case RegisterAllocationStrategy::kLinearScan:
      return AllocateRegistersLinearScan();
    case RegisterAllocationStrategy::kGreedy:
      return AllocateRegistersGreedy();
    case RegisterAllocationStrategy::kGraph:
      return AllocateRegistersGraph();
    default:
      return false;
  }
}

// 仮想レジスタに割り当てられた物理レジスタを取得する
uint32_t RegisterAllocator::GetPhysicalRegister(uint32_t virtualRegId) const noexcept {
  auto it = m_virtualRegisters.find(virtualRegId);
  if (it == m_virtualRegisters.end()) {
    return std::numeric_limits<uint32_t>::max();  // 仮想レジスタが見つからない
  }
  
  if (it->second.isSpilled) {
    return std::numeric_limits<uint32_t>::max();  // スピルされているので物理レジスタなし
  }
  
  return it->second.physicalRegId;
}

// 仮想レジスタがスピルされているかどうかを取得する
bool RegisterAllocator::IsSpilled(uint32_t virtualRegId) const noexcept {
  auto it = m_virtualRegisters.find(virtualRegId);
  if (it == m_virtualRegisters.end()) {
    return false;  // 仮想レジスタが見つからない
  }
  
  return it->second.isSpilled;
}

// スピルされた仮想レジスタのスピルスロット番号を取得する
int RegisterAllocator::GetSpillSlot(uint32_t virtualRegId) const noexcept {
  auto it = m_virtualRegisters.find(virtualRegId);
  if (it == m_virtualRegisters.end() || !it->second.isSpilled) {
    return -1;  // 仮想レジスタが見つからないか、スピルされていない
  }
  
  return it->second.spillSlot;
}

// レジスタアロケータの状態をリセットする
void RegisterAllocator::Reset() noexcept {
  m_virtualRegisters.clear();
  m_usedPhysicalRegs.clear();
  m_nextVirtualRegId = 1;
  m_nextSpillSlot = 0;
}

// レジスタ割り当て戦略を設定する
void RegisterAllocator::SetStrategy(RegisterAllocationStrategy strategy) noexcept {
  m_strategy = strategy;
}

// 線形スキャン法でレジスタを割り当てる
bool RegisterAllocator::AllocateRegistersLinearScan() noexcept {
  // 既に割り当てられているレジスタをクリア
  for (auto& pair : m_virtualRegisters) {
    VirtualRegister& vreg = pair.second;
    vreg.physicalRegId = std::numeric_limits<uint32_t>::max();
    vreg.isSpilled = false;
    vreg.spillSlot = -1;
  }
  
  // 生存範囲の開始順にソートした仮想レジスタのリスト
  std::vector<VirtualRegister*> sortedVregs;
  for (auto& pair : m_virtualRegisters) {
    if (pair.second.liveRangeStart >= 0 && pair.second.liveRangeEnd >= 0) {
      sortedVregs.push_back(&pair.second);
    }
  }
  
  std::sort(sortedVregs.begin(), sortedVregs.end(), 
            [](const VirtualRegister* a, const VirtualRegister* b) {
              return a->liveRangeStart < b->liveRangeStart;
            });
  
  // アクティブなレジスタのリスト（生存範囲終了でソート）
  std::vector<VirtualRegister*> active;
  
  for (VirtualRegister* vreg : sortedVregs) {
    // アクティブリストから終了した生存範囲を除去
    active.erase(
      std::remove_if(active.begin(), active.end(),
                    [vreg](const VirtualRegister* a) {
                      return a->liveRangeEnd < vreg->liveRangeStart;
                    }),
      active.end());
    
    // 仮想レジスタの種類に応じた利用可能な物理レジスタを検索
    bool found = false;
    for (const PhysicalRegister& preg : m_physicalRegisters) {
      // タイプが一致し、予約されていないレジスタを探す
      if (preg.type == vreg->type && !preg.isReserved) {
        // このレジスタが既に使用されているか確認
        bool isUsed = false;
        for (const VirtualRegister* activeVreg : active) {
          if (activeVreg->physicalRegId == preg.id) {
            isUsed = true;
            break;
          }
        }
        
        if (!isUsed) {
          // 使用されていないレジスタを割り当て
          vreg->physicalRegId = preg.id;
          found = true;
          break;
        }
      }
    }
    
    // 利用可能なレジスタがない場合、スピル
    if (!found) {
      // 最も生存範囲が長いレジスタをスピル候補とする
      VirtualRegister* spillCandidate = vreg;
      int maxLiveRange = vreg->liveRangeEnd - vreg->liveRangeStart;
      
      for (VirtualRegister* activeVreg : active) {
        int activeRange = activeVreg->liveRangeEnd - activeVreg->liveRangeStart;
        if (activeRange > maxLiveRange) {
          maxLiveRange = activeRange;
          spillCandidate = activeVreg;
        }
      }
      
      // スピル候補がこの仮想レジスタ自体でない場合、スワップ
      if (spillCandidate != vreg) {
        vreg->physicalRegId = spillCandidate->physicalRegId;
        spillCandidate->physicalRegId = std::numeric_limits<uint32_t>::max();
        spillCandidate->isSpilled = true;
        spillCandidate->spillSlot = m_nextSpillSlot++;
      } else {
        // この仮想レジスタをスピル
        vreg->isSpilled = true;
        vreg->spillSlot = m_nextSpillSlot++;
      }
    }
    
    // アクティブリストに追加
    if (!vreg->isSpilled) {
      active.push_back(vreg);
    }
  }
  
  return true;
}

// 貪欲法でレジスタを割り当てる
bool RegisterAllocator::AllocateRegistersGreedy() noexcept {
  // 既に割り当てられているレジスタをクリア
  for (auto& pair : m_virtualRegisters) {
    VirtualRegister& vreg = pair.second;
    vreg.physicalRegId = std::numeric_limits<uint32_t>::max();
    vreg.isSpilled = false;
    vreg.spillSlot = -1;
  }
  
  // 生存範囲の開始順にソートした仮想レジスタのリスト
  std::vector<VirtualRegister*> sortedVregs;
  for (auto& pair : m_virtualRegisters) {
    if (pair.second.liveRangeStart >= 0 && pair.second.liveRangeEnd >= 0) {
      sortedVregs.push_back(&pair.second);
    }
  }
  
  // 利用度（使用回数）でソート
  std::sort(sortedVregs.begin(), sortedVregs.end(), 
            [](const VirtualRegister* a, const VirtualRegister* b) {
              int usageA = a->liveRangeEnd - a->liveRangeStart;
              int usageB = b->liveRangeEnd - b->liveRangeStart;
              return usageA > usageB;  // 使用頻度が高い順
            });
  
  // 各仮想レジスタに物理レジスタを割り当てる
  for (VirtualRegister* vreg : sortedVregs) {
    // 干渉するレジスタを特定
    std::unordered_set<uint32_t> interferingRegs;
    for (const auto& pair : m_virtualRegisters) {
      const VirtualRegister& otherVreg = pair.second;
      
      // 既に物理レジスタが割り当てられていて、生存範囲が重なる場合
      if (otherVreg.id != vreg->id && 
          otherVreg.physicalRegId != std::numeric_limits<uint32_t>::max() &&
          !otherVreg.isSpilled &&
          ((vreg->liveRangeStart <= otherVreg.liveRangeEnd && 
            vreg->liveRangeEnd >= otherVreg.liveRangeStart))) {
        interferingRegs.insert(otherVreg.physicalRegId);
      }
    }
    
    // 干渉しない物理レジスタを探す
    bool found = false;
    for (const PhysicalRegister& preg : m_physicalRegisters) {
      if (preg.type == vreg->type && !preg.isReserved && 
          interferingRegs.find(preg.id) == interferingRegs.end()) {
        vreg->physicalRegId = preg.id;
        found = true;
        break;
      }
    }
    
    // 利用可能なレジスタがない場合、スピル
    if (!found) {
      vreg->isSpilled = true;
      vreg->spillSlot = m_nextSpillSlot++;
    }
  }
  
  return true;
}

// グラフ彩色法でレジスタを割り当てる
bool RegisterAllocator::AllocateRegistersGraph() noexcept {
  // 既に割り当てられているレジスタをクリア
  for (auto& pair : m_virtualRegisters) {
    VirtualRegister& vreg = pair.second;
    vreg.physicalRegId = std::numeric_limits<uint32_t>::max();
    vreg.isSpilled = false;
    vreg.spillSlot = -1;
  }

  // 干渉グラフの構築
  std::unordered_map<uint32_t, std::unordered_set<uint32_t>> interferenceGraph;
  for (const auto& pair1 : m_virtualRegisters) {
    const VirtualRegister& vreg1 = pair1.second;
    for (const auto& pair2 : m_virtualRegisters) {
      const VirtualRegister& vreg2 = pair2.second;
      if (vreg1.id != vreg2.id && vreg1.type == vreg2.type &&
          (vreg1.liveRangeStart <= vreg2.liveRangeEnd && vreg1.liveRangeEnd >= vreg2.liveRangeStart)) {
        interferenceGraph[vreg1.id].insert(vreg2.id);
      }
    }
  }

  // Chaitin/Briggsアルゴリズム: ノードの次数がK未満のノードをスタックに積む
  const size_t K = m_physicalRegisters.size();
  std::vector<uint32_t> stack;
  std::unordered_map<uint32_t, std::unordered_set<uint32_t>> graphCopy = interferenceGraph;
  std::unordered_set<uint32_t> spilledNodes;

  while (!graphCopy.empty()) {
    bool removed = false;
    for (auto it = graphCopy.begin(); it != graphCopy.end(); ++it) {
      if (it->second.size() < K) {
        stack.push_back(it->first);
        // 隣接ノードからこのノードを除去
        for (uint32_t neighbor : it->second) {
          graphCopy[neighbor].erase(it->first);
        }
        graphCopy.erase(it);
        removed = true;
        break;
      }
    }
    if (!removed) {
      // すべてのノードの次数がK以上→スピル候補
      // スピルコスト最小のノードを選ぶ（ここではlive range最長を仮定）
      uint32_t spillId = 0;
      int maxRange = -1;
      for (const auto& pair : graphCopy) {
        const VirtualRegister& vreg = m_virtualRegisters[pair.first];
        int range = vreg.liveRangeEnd - vreg.liveRangeStart;
        if (range > maxRange) {
          maxRange = range;
          spillId = pair.first;
        }
      }
      spilledNodes.insert(spillId);
      // 隣接ノードからこのノードを除去
      for (uint32_t neighbor : graphCopy[spillId]) {
        graphCopy[neighbor].erase(spillId);
      }
      graphCopy.erase(spillId);
    }
  }

  // スタックから逆順に色（物理レジスタ）を割り当て
  std::reverse(stack.begin(), stack.end());
  for (uint32_t nodeId : stack) {
    auto& vreg = m_virtualRegisters[nodeId];
    std::unordered_set<uint32_t> usedColors;
    for (uint32_t neighborId : interferenceGraph[nodeId]) {
      auto it = m_virtualRegisters.find(neighborId);
      if (it != m_virtualRegisters.end() && !it->second.isSpilled) {
        usedColors.insert(it->second.physicalRegId);
      }
    }
    bool found = false;
    for (const PhysicalRegister& preg : m_physicalRegisters) {
      if (preg.type == vreg.type && !preg.isReserved && usedColors.find(preg.id) == usedColors.end()) {
        vreg.physicalRegId = preg.id;
        found = true;
        break;
      }
    }
    if (!found) {
      vreg.isSpilled = true;
      vreg.spillSlot = m_nextSpillSlot++;
    }
  }

  // スピルノードに対してはスピル処理
  for (uint32_t nodeId : spilledNodes) {
    auto& vreg = m_virtualRegisters[nodeId];
    vreg.isSpilled = true;
    vreg.spillSlot = m_nextSpillSlot++;
  }

  return true;
}

// 生存範囲分析を行う
void RegisterAllocator::AnalyzeLiveRanges(const std::vector<uint32_t>& instructions) noexcept {
  std::unordered_map<uint32_t, std::pair<size_t, size_t>> liveRanges;
  for (size_t i = 0; i < instructions.size(); ++i) {
    uint32_t reg = extractRegisterFromInstruction(instructions[i]);
    if (liveRanges.find(reg) == liveRanges.end()) {
      liveRanges[reg].first = i;
    }
    liveRanges[reg].second = i;
  }
  m_liveRanges = liveRanges;
}

// 物理レジスタの一覧を初期化する
void RegisterAllocator::InitializePhysicalRegisters() noexcept {
  // アーキテクチャに依存する実装
  // x86_64の場合の例
  
  // 汎用レジスタ
  m_physicalRegisters.push_back({0, "rax", PhysicalRegisterType::kGeneral, true, false, false});
  m_physicalRegisters.push_back({1, "rcx", PhysicalRegisterType::kGeneral, true, false, false});
  m_physicalRegisters.push_back({2, "rdx", PhysicalRegisterType::kGeneral, true, false, false});
  m_physicalRegisters.push_back({3, "rbx", PhysicalRegisterType::kGeneral, false, true, false});
  m_physicalRegisters.push_back({4, "rsp", PhysicalRegisterType::kGeneral, false, false, true});  // スタックポインタは予約済み
  m_physicalRegisters.push_back({5, "rbp", PhysicalRegisterType::kGeneral, false, true, false});
  m_physicalRegisters.push_back({6, "rsi", PhysicalRegisterType::kGeneral, true, false, false});
  m_physicalRegisters.push_back({7, "rdi", PhysicalRegisterType::kGeneral, true, false, false});
  m_physicalRegisters.push_back({8, "r8", PhysicalRegisterType::kGeneral, true, false, false});
  m_physicalRegisters.push_back({9, "r9", PhysicalRegisterType::kGeneral, true, false, false});
  m_physicalRegisters.push_back({10, "r10", PhysicalRegisterType::kGeneral, true, false, false});
  m_physicalRegisters.push_back({11, "r11", PhysicalRegisterType::kGeneral, true, false, false});
  m_physicalRegisters.push_back({12, "r12", PhysicalRegisterType::kGeneral, false, true, false});
  m_physicalRegisters.push_back({13, "r13", PhysicalRegisterType::kGeneral, false, true, false});
  m_physicalRegisters.push_back({14, "r14", PhysicalRegisterType::kGeneral, false, true, false});
  m_physicalRegisters.push_back({15, "r15", PhysicalRegisterType::kGeneral, false, true, false});
  
  // 浮動小数点レジスタ
  for (int i = 0; i < 16; i++) {
    m_physicalRegisters.push_back({
      static_cast<uint32_t>(16 + i),
      "xmm" + std::to_string(i),
      PhysicalRegisterType::kFloat,
      true,
      false,
      false
    });
  }
}

} // namespace core
} // namespace aerojs 