#include "super_optimizing_jit.h"

#include <algorithm>  // std::copy
#include <chrono>     // パフォーマンス計測用

#include "../backend/arm64/arm64_code_generator.h"
#include "../backend/riscv/riscv_code_generator.h"
#include "../backend/x86_64/x86_64_code_generator.h"
#include "../code_cache.h"
#include "../ir/ir_optimizer.h"
#include "../memory/memory_manager.h"
#include "../profiler/execution_profiler.h"

namespace aerojs {
namespace core {

#ifndef LIKELY
#define LIKELY(x) __builtin_expect(!!(x), 1)
#endif
#ifndef UNLIKELY
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

SuperOptimizingJIT::SuperOptimizingJIT() noexcept
    : optimization_level_(OptimizationLevel::Maximum),
      profile_data_(std::make_unique<ProfileData>()),
      compilation_count_(0) {
  // プロファイル情報の初期化
  profile_data_->Initialize();

  // 最適化パスの登録
  optimization_passes_.emplace_back(std::make_unique<ConstantFoldingPass>());
  optimization_passes_.emplace_back(std::make_unique<DeadCodeEliminationPass>());
  optimization_passes_.emplace_back(std::make_unique<InliningPass>());
  optimization_passes_.emplace_back(std::make_unique<LoopOptimizationPass>());
  optimization_passes_.emplace_back(std::make_unique<RegisterAllocationPass>());

  // メモリ管理の初期化
  MemoryManager::Instance().RegisterJITInstance(this);

  // 統計情報の初期化
  stats_.total_compilation_time_ns = 0;
  stats_.total_bytecode_size = 0;
  stats_.total_machine_code_size = 0;
  stats_.cache_hits = 0;
  stats_.cache_misses = 0;
}

SuperOptimizingJIT::~SuperOptimizingJIT() noexcept {
  // プロファイル情報の永続化
  if (profile_data_) {
    profile_data_->Persist();
  }

  // メモリ管理からの登録解除
  MemoryManager::Instance().UnregisterJITInstance(this);

  // 最適化パスのクリーンアップ
  optimization_passes_.clear();

  // 統計情報のログ出力
  LogStatistics();
}

std::unique_ptr<uint8_t[]> SuperOptimizingJIT::Compile(const std::vector<uint8_t>& bytecodes,
                                                       size_t& outCodeSize) noexcept {
  auto start_time = std::chrono::high_resolution_clock::now();

  // バイトコード検証
  if (!ValidateBytecode(bytecodes) || bytecodes.size() > kMaxBytecodeLength) {
    outCodeSize = 0;
    return nullptr;
  }

  // ハッシュを計算 (FNV-1a)
  size_t key = 146527;
  for (uint8_t b : bytecodes) {
    key ^= b;
    key *= 1099511628211ULL;
  }

  // キャッシュから再利用
  if (auto cached = CodeCache::Instance().Lookup(key)) {
    const auto& buf = *cached;
    outCodeSize = buf.size();
    auto code = std::make_unique<uint8_t[]>(outCodeSize);
    std::copy(buf.begin(), buf.end(), code.get());

    // 統計情報の更新
    stats_.cache_hits++;

    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.total_compilation_time_ns +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();

    return code;
  }

  // 統計情報の更新
  stats_.cache_misses++;
  stats_.total_bytecode_size += bytecodes.size();

  // IR容量を事前予約
  ir_.Reserve(bytecodes.size() * 2);

  // IR生成 → 最適化 → コード生成
  BuildIR(bytecodes);
  RunOptimizationPasses();
  std::vector<uint8_t> codeBuffer;
  codeBuffer.reserve(ir_.GetInstructions().size() * kAverageInstSize);
  GenerateMachineCode(codeBuffer);
  outCodeSize = codeBuffer.size();

  if (outCodeSize == 0) {
    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.total_compilation_time_ns +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    return nullptr;
  }

  // 統計情報の更新
  stats_.total_machine_code_size += outCodeSize;
  compilation_count_++;

  // キャッシュに挿入
  CodeCache::Instance().Insert(key, codeBuffer);

  // 結果を返却
  auto code = std::make_unique<uint8_t[]>(outCodeSize);
  std::copy(codeBuffer.begin(), codeBuffer.end(), code.get());

  auto end_time = std::chrono::high_resolution_clock::now();
  stats_.total_compilation_time_ns +=
      std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();

  return code;
}

void SuperOptimizingJIT::Reset() noexcept {
  // IRのクリア
  ir_.Clear();

  // プロファイル情報のリセット
  if (profile_data_) {
    profile_data_->Reset();
  }

  // 統計情報の保存と初期化
  LogStatistics();
  stats_.total_compilation_time_ns = 0;
  stats_.total_bytecode_size = 0;
  stats_.total_machine_code_size = 0;
  stats_.cache_hits = 0;
  stats_.cache_misses = 0;
  compilation_count_ = 0;
}

void SuperOptimizingJIT::BuildIR(const std::vector<uint8_t>& bytecodes) noexcept {
  ir_.Clear();
  const uint8_t* ptr = bytecodes.data();
  const uint8_t* end = ptr + bytecodes.size();
  ir_.Reserve(bytecodes.size() * 2);

  // 基本ブロックの構築
  std::vector<size_t> block_starts;
  block_starts.push_back(0);

  // 第一パス: 基本ブロックの境界を特定
  size_t offset = 0;
  while (ptr < end) {
    uint8_t b = *ptr++;
    offset++;

    // 分岐命令の検出
    if (b == 0x90 || b == 0x91 || b == 0x92) {  // JMP, JZ, JNZ
      if (ptr < end - 1) {
        int16_t target = static_cast<int16_t>((*ptr << 8) | *(ptr + 1));
        size_t abs_target = offset + 2 + target;
        if (abs_target < bytecodes.size()) {
          block_starts.push_back(abs_target);
        }
        block_starts.push_back(offset + 2);
        ptr += 2;
        offset += 2;
      }
    }
  }

  // 重複を排除してソート
  std::sort(block_starts.begin(), block_starts.end());
  block_starts.erase(std::unique(block_starts.begin(), block_starts.end()), block_starts.end());

  // 第二パス: IR命令の生成
  ptr = bytecodes.data();
  offset = 0;

  while (ptr < end) {
    // 基本ブロックの開始位置をマーク
    auto it = std::find(block_starts.begin(), block_starts.end(), offset);
    if (it != block_starts.end()) {
      ir_.AddBlockMarker(offset);
    }

    uint8_t b = *ptr++;
    offset++;

    if (LIKELY(b <= 0x7F)) {
      // 定数ロード
      ir_.AddInstruction({Opcode::LoadConst, {static_cast<int32_t>(b)}});
    } else if (LIKELY(b >= 0x80 && b <= 0x83)) {
      // 算術演算
      static const Opcode ops[] = {Opcode::Add, Opcode::Sub, Opcode::Mul, Opcode::Div};
      ir_.AddInstruction({ops[b - 0x80], {}});
    } else if (b == 0x84) {
      // 変数ロード
      if (LIKELY(ptr < end)) {
        uint8_t var_idx = *ptr++;
        offset++;
        ir_.AddInstruction({Opcode::LoadVar, {static_cast<int32_t>(var_idx)}});

        // プロファイル情報の更新
        if (profile_data_) {
          profile_data_->RecordVarAccess(var_idx, true);
        }
      }
    } else if (b == 0x85) {
      // 変数ストア
      if (LIKELY(ptr < end)) {
        uint8_t var_idx = *ptr++;
        offset++;
        ir_.AddInstruction({Opcode::StoreVar, {static_cast<int32_t>(var_idx)}});

        // プロファイル情報の更新
        if (profile_data_) {
          profile_data_->RecordVarAccess(var_idx, false);
        }
      }
    } else if (b == 0x90) {
      // 無条件ジャンプ
      if (LIKELY(ptr < end - 1)) {
        int16_t target = static_cast<int16_t>((*ptr << 8) | *(ptr + 1));
        ptr += 2;
        offset += 2;
        ir_.AddInstruction({Opcode::Jump, {static_cast<int32_t>(target)}});
      }
    } else if (b == 0x91) {
      // ゼロジャンプ
      if (LIKELY(ptr < end - 1)) {
        int16_t target = static_cast<int16_t>((*ptr << 8) | *(ptr + 1));
        ptr += 2;
        offset += 2;
        ir_.AddInstruction({Opcode::JumpIfZero, {static_cast<int32_t>(target)}});
      }
    } else if (b == 0x92) {
      // 非ゼロジャンプ
      if (LIKELY(ptr < end - 1)) {
        int16_t target = static_cast<int16_t>((*ptr << 8) | *(ptr + 1));
        ptr += 2;
        offset += 2;
        ir_.AddInstruction({Opcode::JumpIfNotZero, {static_cast<int32_t>(target)}});
      }
    } else if (b == 0xF0) {
      // 関数呼び出し
      if (LIKELY(ptr < end)) {
        uint8_t func_idx = *ptr++;
        offset++;
        ir_.AddInstruction({Opcode::Call, {static_cast<int32_t>(func_idx)}});

        // プロファイル情報の更新
        if (profile_data_) {
          profile_data_->RecordFunctionCall(func_idx);
        }
      }
    } else if (b == 0xA0) {
      // 配列アクセス
      ir_.AddInstruction({Opcode::ArrayAccess, {}});
    } else if (b == 0xA1) {
      // プロパティアクセス
      if (LIKELY(ptr < end)) {
        uint8_t prop_idx = *ptr++;
        offset++;
        ir_.AddInstruction({Opcode::PropertyAccess, {static_cast<int32_t>(prop_idx)}});
      }
    } else if (b == 0xFF) {
      // リターン
      ir_.AddInstruction({Opcode::Return, {}});
    } else {
      // 未知の命令はNOPとして扱う
      ir_.AddInstruction({Opcode::Nop, {}});
    }
  }

  // 最後の命令がリターンでない場合は追加
  const auto& insts = ir_.GetInstructions();
  if (insts.empty() || insts.back().opcode != Opcode::Return) {
    ir_.AddInstruction({Opcode::Return, {}});
  }

  // 制御フローグラフの構築
  ir_.BuildCFG();

  // データフロー解析
  ir_.AnalyzeDataFlow();
}

void SuperOptimizingJIT::RunOptimizationPasses() noexcept {
  // 最適化レベルに応じたパスの実行
  if (optimization_level_ == OptimizationLevel::None) {
    return;
  }

  // 基本最適化パス
  for (auto& pass : optimization_passes_) {
    if (pass->GetLevel() <= static_cast<int>(optimization_level_)) {
      pass->Run(ir_, profile_data_.get());
    }
  }

  // 定数畳み込み最適化（基本実装）
  const auto& insts = ir_.GetInstructions();
  std::vector<IRInstruction> folded;
  folded.reserve(insts.size());
  const IRInstruction* p = insts.data();
  const IRInstruction* end = p + insts.size();

  while (p < end) {
    // 定数畳み込み: LoadConst + LoadConst + Add → LoadConst(sum)
    if ((end - p) >= 3 &&
        p[0].opcode == Opcode::LoadConst &&
        p[1].opcode == Opcode::LoadConst &&
        p[2].opcode == Opcode::Add) {
      int32_t sum = p[0].args[0] + p[1].args[0];
      folded.push_back({Opcode::LoadConst, {sum}});
      p += 3;
    }
    // 定数畳み込み: LoadConst + LoadConst + Mul → LoadConst(product)
    else if ((end - p) >= 3 &&
             p[0].opcode == Opcode::LoadConst &&
             p[1].opcode == Opcode::LoadConst &&
             p[2].opcode == Opcode::Mul) {
      int32_t product = p[0].args[0] * p[1].args[0];
      folded.push_back({Opcode::LoadConst, {product}});
      p += 3;
    }
    // 定数畳み込み: LoadConst + LoadConst + Sub → LoadConst(diff)
    else if ((end - p) >= 3 &&
             p[0].opcode == Opcode::LoadConst &&
             p[1].opcode == Opcode::LoadConst &&
             p[2].opcode == Opcode::Sub) {
      int32_t diff = p[0].args[0] - p[1].args[0];
      folded.push_back({Opcode::LoadConst, {diff}});
      p += 3;
    }
    // 定数畳み込み: LoadConst + LoadConst + Div → LoadConst(quotient)
    else if ((end - p) >= 3 &&
             p[0].opcode == Opcode::LoadConst &&
             p[1].opcode == Opcode::LoadConst &&
             p[2].opcode == Opcode::Div &&
             p[1].args[0] != 0) {  // ゼロ除算を回避
      int32_t quotient = p[0].args[0] / p[1].args[0];
      folded.push_back({Opcode::LoadConst, {quotient}});
      p += 3;
    }
    // その他の命令はそのまま追加
    else {
      folded.push_back(*p);
      ++p;
    }
  }

  // 最適化結果をIRに反映
  ir_.Clear();
  ir_.Reserve(folded.size());
  for (const auto& inst : folded) {
    ir_.AddInstruction(inst);
  }

  // 制御フローグラフの再構築
  ir_.RebuildCFG();
}

void SuperOptimizingJIT::GenerateMachineCode(std::vector<uint8_t>& codeBuffer) noexcept {
  // アーキテクチャ固有のコード生成を呼び出す
#if defined(__x86_64__) || defined(_M_X64)
  X86_64CodeGenerator codeGen;
  codeGen.SetProfileData(profile_data_.get());
  codeGen.Generate(ir_, codeBuffer);
#elif defined(__aarch64__)
  ARM64CodeGenerator codeGen;
  codeGen.SetProfileData(profile_data_.get());
  codeGen.Generate(ir_, codeBuffer);
#elif defined(__riscv) || defined(__riscv__)
  RISCVCodeGenerator codeGen;
  codeGen.SetProfileData(profile_data_.get());
  codeGen.Generate(ir_, codeBuffer);
#else
  static_assert(false, "サポートされていないアーキテクチャです");
#endif

  // 生成されたコードの実行保護設定
  if (!codeBuffer.empty()) {
    MemoryManager::Instance().ProtectCodeMemory(codeBuffer.data(), codeBuffer.size());
  }
}

bool SuperOptimizingJIT::ValidateBytecode(const std::vector<uint8_t>& bytecodes) noexcept {
  if (bytecodes.empty()) {
    return false;
  }

  size_t i = 0, n = bytecodes.size();
  while (i < n) {
    uint8_t op = bytecodes[i++];

    // 単一バイト命令
    if (LIKELY(op <= 0x7F || (op >= 0x80 && op <= 0x83) || op == 0xA0 || op == 0xFF)) {
      continue;
    }

    // 2バイト命令
    if (LIKELY(op == 0x84 || op == 0x85 || op == 0xF0 || op == 0xA1)) {
      if (i >= n) return false;  // 引数が不足
      ++i;
      continue;
    }

    // 3バイト命令（ジャンプ系）
    if (LIKELY(op == 0x90 || op == 0x91 || op == 0x92)) {
      if (i + 1 >= n) return false;  // 引数が不足
      i += 2;
      continue;
    }

    // 未定義命令
    return false;
  }

  return true;
}

void SuperOptimizingJIT::SetOptimizationLevel(OptimizationLevel level) noexcept {
  optimization_level_ = level;
}

OptimizationLevel SuperOptimizingJIT::GetOptimizationLevel() const noexcept {
  return optimization_level_;
}

const JITStatistics& SuperOptimizingJIT::GetStatistics() const noexcept {
  return stats_;
}

void SuperOptimizingJIT::LogStatistics() const noexcept {
  if (compilation_count_ == 0) {
    return;
  }

  // 統計情報のログ出力
  double avg_compilation_time_us = static_cast<double>(stats_.total_compilation_time_ns) /
                                   (compilation_count_ * 1000.0);
  double code_size_ratio = static_cast<double>(stats_.total_machine_code_size) /
                           static_cast<double>(stats_.total_bytecode_size);
  double cache_hit_ratio = static_cast<double>(stats_.cache_hits) /
                           static_cast<double>(stats_.cache_hits + stats_.cache_misses);

  Logger::Info(
      "SuperOptimizingJIT統計: コンパイル数=%d, 平均コンパイル時間=%.2fμs, "
      "コードサイズ比=%.2f, キャッシュヒット率=%.2f%%",
      compilation_count_, avg_compilation_time_us, code_size_ratio,
      cache_hit_ratio * 100.0);
}

}  // namespace core
}  // namespace aerojs