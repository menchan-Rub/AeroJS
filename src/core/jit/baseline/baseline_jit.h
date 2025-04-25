#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

#include "../jit_compiler.h"
#include "../ir/ir.h"
#include "../ir/ir_builder.h"
#include "../jit_profiler.h"
#include "../../bytecode/bytecode.h"
#include "../../bytecode/bytecode_defs.h"

namespace aerojs {
namespace core {

// 前方宣言
class BytecodeDecoder;
class RegisterAllocator;

/**
 * @brief Baseline JIT 実装
 *
 * バイトコードを単純かつ高速にマシンコードに変換します。
 * 複雑な最適化は行わず、バイトコードからマシンコードへの直接的な変換を行います。
 */
class BaselineJIT : public JITCompiler {
 public:
  /**
   * @brief コンストラクタ
   * @param functionId 関数ID
   * @param enableProfiling プロファイリングを有効にするかどうか
   */
  BaselineJIT(uint32_t functionId = 0, bool enableProfiling = false) noexcept;
  ~BaselineJIT() noexcept override;

  /**
   * @brief バイトコードをコンパイルしてマシンコードを生成する
   * @param bytecodes 入力バイトコードのバイト列
   * @param outCodeSize 生成したマシンコードのサイズを返す
   * @return 生成したマシンコードのバイト配列
   */
  std::unique_ptr<uint8_t[]> Compile(const std::vector<uint8_t>& bytecodes,
                                   size_t& outCodeSize) noexcept override;
  
  /**
   * @brief JITコンパイラの内部状態をリセットする
   */
  void Reset() noexcept override;

  /**
   * @brief プロファイリングを有効または無効にする
   * @param enable プロファイリングを有効にするかどうか
   */
  void EnableProfiling(bool enable) noexcept { m_profilingEnabled = enable; }

  /**
   * @brief プロファイリングが有効かどうかを取得する
   * @return プロファイリングの有効/無効状態
   */
  bool IsProfilingEnabled() const noexcept { return m_profilingEnabled; }

  /**
   * @brief 関数IDを設定する
   * @param functionId 関数ID
   */
  void SetFunctionId(uint32_t functionId) noexcept { m_functionId = functionId; }

  /**
   * @brief 関数IDを取得する
   * @return 関数ID
   */
  uint32_t GetFunctionId() const noexcept { return m_functionId; }

  /**
   * @brief プロファイラーを取得する
   * @return プロファイラーへの参照
   */
  static JITProfiler& GetProfiler() noexcept { return m_profiler; }

 private:
  /**
   * @brief バイトコードをIR（中間表現）に変換する
   * @param bytecodes 入力バイトコードのバイト列
   * @return 生成したIR関数
   */
  std::unique_ptr<IRFunction> GenerateIR(const std::vector<uint8_t>& bytecodes) noexcept;

  /**
   * @brief IRをマシンコードに変換する
   * @param irFunction IR関数
   * @param outCodeSize 生成したマシンコードのサイズを返す
   * @return 生成したマシンコードのバイト配列
   */
  std::unique_ptr<uint8_t[]> GenerateMachineCode(IRFunction* irFunction, 
                                               size_t& outCodeSize) noexcept;

  /**
   * @brief ジャンプ命令のターゲットアドレスを解決する
   * @param bytecodes 入力バイトコードのバイト列
   * @return ラベル名からオフセットへのマッピング
   */
  std::unordered_map<std::string, size_t> ResolveJumpTargets(
      const std::vector<uint8_t>& bytecodes) noexcept;

  /**
   * @brief バイトコード実行をプロファイリングするためのフック
   * @param offset バイトコードオフセット
   */
  void ProfileExecution(uint32_t offset) noexcept;

  /**
   * @brief 型情報をプロファイリングするためのフック
   * @param offset バイトコードオフセット
   * @param type 型カテゴリ
   */
  void ProfileType(uint32_t offset, TypeFeedbackRecord::TypeCategory type) noexcept;

  /**
   * @brief 関数呼び出しをプロファイリングするためのフック
   * @param offset バイトコードオフセット
   * @param calleeFunctionId 呼び出される関数のID
   * @param executionTimeNs 実行時間（ナノ秒）
   */
  void ProfileCallSite(uint32_t offset, uint32_t calleeFunctionId, uint64_t executionTimeNs) noexcept;

 private:
  // バイトコードデコーダ
  std::unique_ptr<BytecodeDecoder> m_decoder;
  
  // レジスタアロケータ
  std::unique_ptr<RegisterAllocator> m_regAllocator;
  
  // IR生成器
  IRBuilder m_irBuilder;

  // 関数ID
  uint32_t m_functionId;

  // プロファイリングフラグ
  bool m_profilingEnabled;

  // JITプロファイラー（静的インスタンス）
  static JITProfiler m_profiler;
  
  // 現在のコンパイル状態
  struct {
    // バイトコードオフセットからIRインデックスへのマッピング
    std::unordered_map<size_t, size_t> offsetToIRIndex;
    
    // ラベル名からIRインデックスへのマッピング
    std::unordered_map<std::string, size_t> labelToIRIndex;
  } m_state;
};

}  // namespace core
}  // namespace aerojs