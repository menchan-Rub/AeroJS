#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace aerojs {
namespace core {

/**
 * @brief JIT コンパイラの共通インターフェースクラス
 *
 * バイトコード列を受け取り、実行可能なマシンコードを生成します。
 */
class JITCompiler {
 public:
  /**
   * @brief 仮想デストラクタ
   */
  virtual ~JITCompiler() noexcept = default;

  /**
   * @brief バイトコードをコンパイルしてマシンコードを生成する
   * @param bytecodes 入力バイトコードのバイト列
   * @param outCodeSize 生成したマシンコードのサイズを返す
   * @return 生成したマシンコードのバイト配列 (nullptr でないことを保証)
   */
  [[nodiscard]] virtual std::unique_ptr<uint8_t[]> Compile(const std::vector<uint8_t>& bytecodes,
                                                           size_t& outCodeSize) noexcept = 0;

  /**
   * @brief 内部状態をリセットする
   */
  virtual void Reset() noexcept = 0;

 protected:
  JITCompiler() noexcept = default;

 private:
  // コピー/ムーブ禁止
  JITCompiler(const JITCompiler&) = delete;
  JITCompiler& operator=(const JITCompiler&) = delete;
};

}  // namespace core
}  // namespace aerojs