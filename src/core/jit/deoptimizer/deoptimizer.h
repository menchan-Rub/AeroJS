#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>

namespace aerojs {
namespace core {

/**
 * @brief JIT最適化コードからインタープリタに戻るための情報
 */
struct DeoptimizationInfo {
  uint32_t functionId;                   ///< 関数ID
  uint32_t bytecodeOffset;               ///< バイトコードオフセット
  uint32_t stackDepth;                   ///< スタック深度
  std::vector<uint32_t> liveVariables;   ///< 生存変数のリスト
};

/**
 * @brief デオプティマイズ理由の列挙型
 */
enum class DeoptimizationReason {
  TypeFeedback,     ///< 型フィードバックによる失敗
  Overflow,         ///< 数値オーバーフロー
  BailoutRequest,   ///< 明示的なベイルアウト要求
  DebuggerAttached, ///< デバッガが接続された
  TypeCheck,        ///< ガード条件による型チェック失敗
  Unknown           ///< 未知の理由
};

/**
 * @brief デオプティマイザのコールバック関数型
 */
using DeoptCallback = std::function<void(const DeoptimizationInfo&, DeoptimizationReason)>;

/**
 * @brief JITコードからインタープリタへの遷移を管理するクラス
 */
class Deoptimizer {
public:
  /**
   * @brief シングルトンインスタンスを取得
   * @return デオプティマイザのインスタンス
   */
  static Deoptimizer& Instance() noexcept {
    static Deoptimizer instance;
    return instance;
  }
  
  /**
   * @brief デオプティマイズポイントを登録
   * @param codeAddress デオプティマイズポイントのコードアドレス
   * @param info デオプティマイズ情報
   */
  void RegisterDeoptPoint(void* codeAddress, const DeoptimizationInfo& info) noexcept;
  
  /**
   * @brief デオプティマイズを実行
   * @param codeAddress デオプティマイズするポイントのコードアドレス
   * @param reason デオプティマイズの理由
   * @return デオプティマイズが成功したかどうか
   */
  bool PerformDeoptimization(void* codeAddress, DeoptimizationReason reason) noexcept;
  
  /**
   * @brief コールバック関数を設定
   * @param callback デオプティマイズ時に呼び出されるコールバック関数
   */
  void SetCallback(DeoptCallback callback) noexcept;
  
  /**
   * @brief デオプティマイズポイントの登録を解除
   * @param codeAddress 登録解除するコードアドレス
   */
  void UnregisterDeoptPoint(void* codeAddress) noexcept;
  
  /**
   * @brief すべてのデオプティマイズポイントの登録を解除
   */
  void ClearAllDeoptPoints() noexcept;

private:
  Deoptimizer() noexcept = default;
  ~Deoptimizer() noexcept = default;
  Deoptimizer(const Deoptimizer&) = delete;
  Deoptimizer& operator=(const Deoptimizer&) = delete;
  
  // コードアドレスからデオプティマイズ情報へのマップ
  std::unordered_map<void*, DeoptimizationInfo> m_deoptInfoMap;
  
  // デオプティマイズ時に呼び出されるコールバック
  DeoptCallback m_callback;
};

}  // namespace core
}  // namespace aerojs 