/**
 * @file jit_compiler.h
 * @brief JITコンパイラのための基本インターフェース
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include "ir/ir.h"

namespace aerojs {
namespace core {

/**
 * @brief 最適化レベルの設定
 */
enum class OptimizationLevel {
  None,       ///< 最適化なし
  Basic,      ///< 基本的な最適化のみ（ベースラインJIT向け）
  Medium,     ///< 中程度の最適化（標準的な最適化JIT向け）
  Aggressive  ///< 積極的な最適化（超最適化JIT向け）
};

// 前方宣言
class Context;
class Function;
class JITProfiler;
struct TypeFeedbackRecord;
struct LoopProfile;
struct CallSiteProfile;

// JITコンパイルオプション
struct JITCompileOptions {
    bool enableOptimization = true;    // 最適化を有効にするか
    bool enableInlining = true;        // インライン展開を有効にするか
    bool enableSIMD = true;            // SIMDベクトル化を有効にするか
    bool enableAsyncCompilation = false; // 非同期コンパイルを有効にするか
    int optimizationLevel = 2;         // 最適化レベル (0-3)
    size_t maxInlineSize = 100;        // インライン展開の最大サイズ
    
    // デフォルトコンストラクタ
    JITCompileOptions() = default;
};

/**
 * @brief JIT コンパイラの共通インターフェースクラス
 *
 * バイトコード列やIR関数を受け取り、実行可能なマシンコードを生成します。
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
   * @brief IR関数をコンパイルして実行可能コードを生成する
   * @param function コンパイル対象のIR関数
   * @param functionId 関数ID（プロファイリング用）
   * @return 実行可能コードのポインタ
   */
  virtual void* Compile(const IRFunction& function, uint32_t functionId) noexcept = 0;

  /**
   * @brief 実行可能コードを解放する
   * @param codePtr 解放対象のコードポインタ
   */
  virtual void ReleaseCode(void* codePtr) noexcept = 0;

  /**
   * @brief 最適化レベルを設定する
   * @param level 最適化レベル
   */
  virtual void SetOptimizationLevel(OptimizationLevel level) noexcept = 0;

  /**
   * @brief デバッグ情報を有効/無効にする
   * @param enable デバッグ情報有効フラグ
   */
  virtual void EnableDebugInfo(bool enable) noexcept = 0;

  /**
   * @brief コンパイルされた関数に関するデバッグ情報を取得する
   * @param codePtr コードポインタ
   * @return デバッグ情報の文字列
   */
  virtual std::string GetDebugInfo(void* codePtr) const noexcept = 0;

  /**
   * @brief 内部状態をリセットする
   */
  virtual void Reset() noexcept = 0;

  // 関数コンパイル
  virtual bool compile(Function* function) = 0;
  
  // コンパイル済みコード取得
  virtual void* getCompiledCode(uint64_t functionId) = 0;
  
  // コンパイル済みかチェック
  virtual bool hasCompiledCode(uint64_t functionId) const = 0;
  
  // オプション設定
  virtual void setOptions(const JITCompileOptions& options) {
      _options = options;
  }
  
  // オプション取得
  virtual const JITCompileOptions& getOptions() const {
      return _options;
  }
  
  // エラー取得
  virtual std::string getLastError() const {
      return _lastError;
  }

  // ファクトリーメソッド - コンパイラ種別に応じたインスタンス作成
  enum class CompilerType {
      Baseline,    // 単純なJITコンパイラ
      Optimizing,  // 最適化JITコンパイラ
      Tracing      // トレーシングJITコンパイラ
  };
  
  static JITCompiler* create(Context* context, CompilerType type);

  /**
   * @brief 関数IDを設定する
   * @param functionId 関数ID
   */
  void SetFunctionId(uint32_t functionId) { m_functionId = functionId; }
  
  /**
   * @brief プロファイラを設定する
   * @param profiler プロファイラインスタンス
   */
  void SetProfiler(JITProfiler* profiler) { m_profiler = profiler; }
  
  /**
   * @brief ホットスポットコールバックを設定する
   * @param callback 関数IDからホットスポットを取得するコールバック
   */
  void SetHotspotCallback(std::function<std::vector<uint32_t>(uint32_t)> callback) {
      m_hotspotCallback = std::move(callback);
  }
  
  /**
   * @brief 型情報コールバックを設定する
   * @param callback 関数IDとバイトコードオフセットから型情報を取得するコールバック
   */
  void SetTypeInfoCallback(std::function<TypeFeedbackRecord(uint32_t, uint32_t)> callback) {
      m_typeInfoCallback = std::move(callback);
  }
  
  /**
   * @brief 分岐予測コールバックを設定する
   * @param callback 関数IDとバイトコードオフセットから分岐確率を取得するコールバック
   */
  void SetBranchPredictionCallback(std::function<double(uint32_t, uint32_t)> callback) {
      m_branchPredictionCallback = std::move(callback);
  }
  
  /**
   * @brief ループ情報コールバックを設定する
   * @param callback 関数IDとループヘッダーオフセットからループ情報を取得するコールバック
   */
  void SetLoopInfoCallback(std::function<LoopProfile(uint32_t, uint32_t)> callback) {
      m_loopInfoCallback = std::move(callback);
  }
  
  /**
   * @brief コールサイト情報コールバックを設定する
   * @param callback 関数IDとコールサイトオフセットからコール情報を取得するコールバック
   */
  void SetCallSiteInfoCallback(std::function<CallSiteProfile(uint32_t, uint32_t)> callback) {
      m_callSiteInfoCallback = std::move(callback);
  }

 protected:
  JITCompiler() noexcept : m_functionId(0), m_profiler(nullptr) {}

  // サブクラスのみが使用する変数
  JITCompileOptions _options;
  std::string _lastError;
  
  // エラー設定ヘルパー
  void setError(const std::string& error) {
      _lastError = error;
  }

  uint32_t m_functionId;
  JITProfiler* m_profiler;
  bool m_debugInfoEnabled = false;
  
  // プロファイリングコールバック
  std::function<std::vector<uint32_t>(uint32_t)> m_hotspotCallback;
  std::function<TypeFeedbackRecord(uint32_t, uint32_t)> m_typeInfoCallback;
  std::function<double(uint32_t, uint32_t)> m_branchPredictionCallback;
  std::function<LoopProfile(uint32_t, uint32_t)> m_loopInfoCallback;
  std::function<CallSiteProfile(uint32_t, uint32_t)> m_callSiteInfoCallback;

 private:
  // コピー/ムーブ禁止
  JITCompiler(const JITCompiler&) = delete;
  JITCompiler& operator=(const JITCompiler&) = delete;
};

/**
 * ループプロファイル情報
 */
struct LoopProfile {
    double iterations;  // 平均イテレーション数
    uint32_t executions; // 実行回数
    
    LoopProfile() : iterations(0.0), executions(0) {}
};

/**
 * コールサイトプロファイル情報
 */
struct CallSiteProfile {
    uint32_t callCount;     // 呼び出し回数
    uint32_t mostCommonTarget; // 最も多く呼ばれたターゲット関数ID
    uint32_t targetCount;   // そのターゲットが呼ばれた回数
    
    CallSiteProfile() : callCount(0), mostCommonTarget(0), targetCount(0) {}
};

}  // namespace core
}  // namespace aerojs