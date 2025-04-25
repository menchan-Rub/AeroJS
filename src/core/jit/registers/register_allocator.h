#ifndef AEROJS_CORE_JIT_REGISTERS_REGISTER_ALLOCATOR_H
#define AEROJS_CORE_JIT_REGISTERS_REGISTER_ALLOCATOR_H

#include <bitset>
#include <vector>
#include <unordered_map>
#include <set>
#include <memory>
#include <array>
#include <optional>
#include <functional>
#include "src/core/jit/ir/ir.h"

namespace aerojs {
namespace core {

// レジスタの種類を表す列挙型
enum class RegisterType {
    GPR,    // 汎用レジスタ
    FPR,    // 浮動小数点レジスタ
    VR,     // 仮想レジスタ（物理レジスタに割り当て予定の一時的なレジスタ）
    SPILL   // スピルされた位置（スタック上に保存）
};

// レジスタクラス（アーキテクチャ固有のレジスタグループ）
enum class RegisterClass {
    INT32,      // 32ビット整数
    INT64,      // 64ビット整数
    FLOAT32,    // 単精度浮動小数点
    FLOAT64,    // 倍精度浮動小数点
    VECTOR,     // ベクトル
    SYSTEM      // システム専用レジスタ
};

// 物理レジスタを表すクラス
class PhysicalRegister {
public:
    PhysicalRegister(uint8_t id, RegisterType type, RegisterClass regClass, 
                     bool calleeSaved = false, bool reserved = false)
        : m_id(id), m_type(type), m_regClass(regClass), 
          m_calleeSaved(calleeSaved), m_reserved(reserved) {}

    uint8_t GetId() const { return m_id; }
    RegisterType GetType() const { return m_type; }
    RegisterClass GetRegClass() const { return m_regClass; }
    bool IsCalleeSaved() const { return m_calleeSaved; }
    bool IsReserved() const { return m_reserved; }
    void SetReserved(bool reserved) { m_reserved = reserved; }

private:
    uint8_t m_id;                // レジスタID
    RegisterType m_type;         // レジスタタイプ
    RegisterClass m_regClass;    // レジスタクラス
    bool m_calleeSaved;          // 呼び出し先保存レジスタか
    bool m_reserved;             // システム予約済みか
};

// 仮想レジスタを表すクラス
class VirtualRegister {
public:
    explicit VirtualRegister(uint32_t id, RegisterClass regClass)
        : m_id(id), m_regClass(regClass), m_allocatedReg(nullptr), m_spillOffset(-1) {}

    uint32_t GetId() const { return m_id; }
    RegisterClass GetRegClass() const { return m_regClass; }
    
    void AssignPhysicalRegister(PhysicalRegister* reg) { m_allocatedReg = reg; }
    PhysicalRegister* GetAssignedRegister() const { return m_allocatedReg; }
    bool IsAssigned() const { return m_allocatedReg != nullptr; }
    
    void SetSpillOffset(int32_t offset) { m_spillOffset = offset; }
    int32_t GetSpillOffset() const { return m_spillOffset; }
    bool IsSpilled() const { return m_spillOffset >= 0; }

private:
    uint32_t m_id;                   // 仮想レジスタID
    RegisterClass m_regClass;        // レジスタクラス
    PhysicalRegister* m_allocatedReg; // 割り当てられた物理レジスタ（nullptrの場合は未割り当て）
    int32_t m_spillOffset;           // スピルされた場合のスタックオフセット
};

// レジスタの生存区間を表すクラス
class LiveInterval {
public:
    // 区間の開始と終了を表す構造体
    struct Range {
        uint32_t start;
        uint32_t end;
        
        Range(uint32_t s, uint32_t e) : start(s), end(e) {}
        
        // 区間の比較演算子（ソート用）
        bool operator<(const Range& other) const {
            return start < other.start || (start == other.start && end < other.end);
        }
    };

    explicit LiveInterval(VirtualRegister* vreg)
        : m_virtualReg(vreg), m_spillWeight(0.0f) {}

    // 区間を追加
    void AddRange(uint32_t start, uint32_t end) {
        if (start > end) {
            return;  // 無効な区間
        }
        
        // 既存の区間と重複する場合はマージ
        std::vector<Range> newRanges;
        Range newRange(start, end);
        bool merged = false;
        
        for (const auto& range : m_ranges) {
            // 重複する区間がある場合
            if (DoRangesOverlap(range, newRange)) {
                newRange.start = std::min(range.start, newRange.start);
                newRange.end = std::max(range.end, newRange.end);
                merged = true;
            } else {
                newRanges.push_back(range);
            }
        }
        
        newRanges.push_back(newRange);
        
        // 区間をソート
        std::sort(newRanges.begin(), newRanges.end());
        
        // 隣接する区間をマージ
        m_ranges.clear();
        if (!newRanges.empty()) {
            Range current = newRanges[0];
            for (size_t i = 1; i < newRanges.size(); i++) {
                if (current.end + 1 >= newRanges[i].start) {
                    // 隣接する区間をマージ
                    current.end = std::max(current.end, newRanges[i].end);
                } else {
                    // 隣接しない場合は新しい区間を追加
                    m_ranges.push_back(current);
                    current = newRanges[i];
                }
            }
            m_ranges.push_back(current);
        }
    }

    // 生存区間のベクトルを取得
    const std::vector<Range>& GetRanges() const { return m_ranges; }
    
    // 生存区間が指定された位置を含むかチェック
    bool ContainsPosition(uint32_t position) const {
        for (const auto& range : m_ranges) {
            if (position >= range.start && position <= range.end) {
                return true;
            }
        }
        return false;
    }
    
    // 生存区間の開始位置を取得
    uint32_t GetStart() const {
        if (m_ranges.empty()) {
            return 0;
        }
        return m_ranges.front().start;
    }
    
    // 生存区間の終了位置を取得
    uint32_t GetEnd() const {
        if (m_ranges.empty()) {
            return 0;
        }
        uint32_t maxEnd = 0;
        for (const auto& range : m_ranges) {
            maxEnd = std::max(maxEnd, range.end);
        }
        return maxEnd;
    }
    
    // 関連付けられている仮想レジスタを取得
    VirtualRegister* GetVirtualRegister() const { return m_virtualReg; }
    
    // スピルの重みを設定
    void SetSpillWeight(float weight) { m_spillWeight = weight; }
    
    // スピルの重みを取得
    float GetSpillWeight() const { return m_spillWeight; }

private:
    // 2つの区間が重複するかチェック
    bool DoRangesOverlap(const Range& a, const Range& b) const {
        return (a.start <= b.end && b.start <= a.end);
    }

    VirtualRegister* m_virtualReg;   // この生存区間に関連付けられた仮想レジスタ
    std::vector<Range> m_ranges;     // 生存区間のリスト
    float m_spillWeight;             // スピルの重み（ループ内の使用頻度など）
};

// レジスタの干渉グラフを表すクラス
class InterferenceGraph {
public:
    InterferenceGraph() = default;

    // 仮想レジスタをグラフに追加
    void AddNode(VirtualRegister* vreg) {
        if (m_adjacencyList.find(vreg) == m_adjacencyList.end()) {
            m_adjacencyList[vreg] = std::set<VirtualRegister*>();
        }
    }

    // 干渉関係を追加
    void AddEdge(VirtualRegister* a, VirtualRegister* b) {
        // 自己ループは追加しない
        if (a == b) {
            return;
        }
        
        // 両方向に干渉関係を追加
        AddNode(a);
        AddNode(b);
        m_adjacencyList[a].insert(b);
        m_adjacencyList[b].insert(a);
    }

    // ノードaが持つすべての隣接ノードを取得
    const std::set<VirtualRegister*>& GetNeighbors(VirtualRegister* vreg) const {
        static const std::set<VirtualRegister*> emptySet;
        auto it = m_adjacencyList.find(vreg);
        if (it != m_adjacencyList.end()) {
            return it->second;
        }
        return emptySet;
    }

    // ノードの次数（隣接ノード数）を取得
    size_t GetDegree(VirtualRegister* vreg) const {
        auto it = m_adjacencyList.find(vreg);
        if (it != m_adjacencyList.end()) {
            return it->second.size();
        }
        return 0;
    }

    // 全ノードを取得
    std::vector<VirtualRegister*> GetAllNodes() const {
        std::vector<VirtualRegister*> nodes;
        for (const auto& pair : m_adjacencyList) {
            nodes.push_back(pair.first);
        }
        return nodes;
    }

private:
    std::unordered_map<VirtualRegister*, std::set<VirtualRegister*>> m_adjacencyList;
};

// レジスタアロケータのベースクラス
class RegisterAllocator {
public:
    RegisterAllocator() = default;
    virtual ~RegisterAllocator() = default;

    // 仮想レジスタを物理レジスタに割り当てる抽象メソッド
    virtual bool AllocateRegisters() = 0;

    // 使用可能な物理レジスタの初期化
    void InitializePhysicalRegisters(const std::vector<PhysicalRegister>& regs) {
        m_physicalRegisters = regs;
    }

    // 物理レジスタの数を取得
    size_t GetPhysicalRegisterCount() const {
        return m_physicalRegisters.size();
    }

    // 指定された種類のレジスタの数を取得
    size_t GetPhysicalRegisterCountByType(RegisterType type) const {
        size_t count = 0;
        for (const auto& reg : m_physicalRegisters) {
            if (reg.GetType() == type) {
                count++;
            }
        }
        return count;
    }

    // 指定されたクラスのレジスタの数を取得
    size_t GetPhysicalRegisterCountByClass(RegisterClass regClass) const {
        size_t count = 0;
        for (const auto& reg : m_physicalRegisters) {
            if (reg.GetRegClass() == regClass) {
                count++;
            }
        }
        return count;
    }

    // 仮想レジスタを作成
    VirtualRegister* CreateVirtualRegister(RegisterClass regClass) {
        auto vreg = std::make_unique<VirtualRegister>(
            static_cast<uint32_t>(m_virtualRegisters.size()), regClass);
        VirtualRegister* result = vreg.get();
        m_virtualRegisters.push_back(std::move(vreg));
        return result;
    }

    // スピルオフセットの生成
    int32_t AllocateSpillSlot(RegisterClass regClass) {
        int32_t size = 8;  // デフォルトサイズ
        switch (regClass) {
            case RegisterClass::INT32:
            case RegisterClass::FLOAT32:
                size = 4;
                break;
            case RegisterClass::INT64:
            case RegisterClass::FLOAT64:
                size = 8;
                break;
            case RegisterClass::VECTOR:
                size = 16;
                break;
            default:
                size = 8;
                break;
        }

        // アラインメントを考慮してスロットを割り当て
        int32_t offset = m_currentSpillOffset;
        m_currentSpillOffset += size;
        
        // 8バイトアラインメントを確保
        m_currentSpillOffset = (m_currentSpillOffset + 7) & ~7;
        
        return offset;
    }

    // 仮想レジスタの生存区間を設定
    void SetLiveInterval(VirtualRegister* vreg, std::unique_ptr<LiveInterval> interval) {
        m_liveIntervals[vreg] = std::move(interval);
    }

    // 仮想レジスタの生存区間を取得
    LiveInterval* GetLiveInterval(VirtualRegister* vreg) {
        auto it = m_liveIntervals.find(vreg);
        if (it != m_liveIntervals.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    // 干渉グラフを取得
    InterferenceGraph& GetInterferenceGraph() {
        return m_interferenceGraph;
    }

    // スピルスロットの総サイズを取得
    int32_t GetTotalSpillSize() const {
        return m_currentSpillOffset;
    }

protected:
    std::vector<PhysicalRegister> m_physicalRegisters;
    std::vector<std::unique_ptr<VirtualRegister>> m_virtualRegisters;
    std::unordered_map<VirtualRegister*, std::unique_ptr<LiveInterval>> m_liveIntervals;
    InterferenceGraph m_interferenceGraph;
    int32_t m_currentSpillOffset = 0;
};

// 線形走査アルゴリズムを使用するレジスタアロケータ
class LinearScanRegisterAllocator : public RegisterAllocator {
public:
    LinearScanRegisterAllocator() = default;
    ~LinearScanRegisterAllocator() override = default;

    bool AllocateRegisters() override;

private:
    // アクティブな区間（現在生きている区間）
    std::vector<LiveInterval*> m_active;

    // アロケーション処理用のヘルパーメソッド
    void ExpireOldIntervals(uint32_t position);
    void SpillAtInterval(LiveInterval* interval);
    PhysicalRegister* FindFreeRegister(VirtualRegister* vreg);
    void UpdateActiveIntervals(uint32_t position);
};

// グラフ彩色アルゴリズムを使用するレジスタアロケータ
class GraphColoringRegisterAllocator : public RegisterAllocator {
public:
    GraphColoringRegisterAllocator() = default;
    ~GraphColoringRegisterAllocator() override = default;

    bool AllocateRegisters() override;

private:
    // 単純化対象のノードのスタック
    std::vector<VirtualRegister*> m_simplifyStack;

    // ノードの単純化（低次数ノードの除去）
    void SimplifyGraph();
    
    // スピル候補の選択
    VirtualRegister* SelectSpillCandidate();
    
    // 彩色（レジスタ割り当て）
    bool AssignColors();
};

// レジスタアロケータファクトリー
class RegisterAllocatorFactory {
public:
    enum class AllocatorType {
        LINEAR_SCAN,
        GRAPH_COLORING
    };

    static std::unique_ptr<RegisterAllocator> Create(AllocatorType type) {
        switch (type) {
            case AllocatorType::LINEAR_SCAN:
                return std::make_unique<LinearScanRegisterAllocator>();
            case AllocatorType::GRAPH_COLORING:
                return std::make_unique<GraphColoringRegisterAllocator>();
            default:
                return std::make_unique<LinearScanRegisterAllocator>();
        }
    }
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_JIT_REGISTERS_REGISTER_ALLOCATOR_H 