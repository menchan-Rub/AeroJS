/**
 * @file vm.h
 * @brief 仮想マシンのコア定義
 *
 * JavaScriptのコード実行のための仮想マシン(VM)システムのメイン定義。
 * このVMはバイトコードを実行し、JITコンパイラと連携して高速な実行を実現します。
 */

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class RuntimeValue;
class BytecodeBlock;
class CallFrame;
class Exception;
class GlobalObject;
class Heap;
class Interpreter;
class JITCompiler;
class NativeFunction;
class Value;
class VMStackFrame;

using ValuePtr = std::shared_ptr<Value>;
using ExceptionPtr = std::shared_ptr<Exception>;
using InterpreterPtr = std::shared_ptr<Interpreter>;
using JITCompilerPtr = std::shared_ptr<JITCompiler>;

/**
 * @brief 仮想マシン実行モード
 */
enum class ExecutionMode {
  kInterpreter,       ///< インタープリタモード
  kBaselineJIT,       ///< ベースラインJITモード
  kOptimizedJIT,      ///< 最適化JITモード
  kUltraOptimizedJIT  ///< 超最適化JITモード
};

/**
 * @brief VM実行のトレースレベル
 */
enum class TraceLevel {
  kNone,      ///< トレースなし
  kBasic,     ///< 基本的なトレース
  kDetailed,  ///< 詳細なトレース情報
  kVerbose    ///< 全てのトレース情報
};

/**
 * @brief VMの設定オプション
 */
struct VMOptions {
  bool enableJIT = true;                      ///< JITコンパイルを有効にする
  bool enableOptimization = true;             ///< 最適化を有効にする
  bool enableGC = true;                       ///< ガベージコレクションを有効にする
  size_t initialStackSize = 1024 * 1024;      ///< 初期スタックサイズ (1MB)
  size_t maxStackSize = 16 * 1024 * 1024;     ///< 最大スタックサイズ (16MB)
  size_t initialHeapSize = 4 * 1024 * 1024;   ///< 初期ヒープサイズ (4MB)
  size_t maxHeapSize = 1024 * 1024 * 1024;    ///< 最大ヒープサイズ (1GB)
  TraceLevel traceLevel = TraceLevel::kNone;  ///< トレースレベル
  bool strictMode = false;                    ///< 厳格モード
  size_t inlineThreshold = 100;               ///< インライン展開の閾値(バイトコード命令数)
  size_t optimizationThreshold = 1000;        ///< 最適化の閾値(実行回数)
  size_t reoptimizationThreshold = 10000;     ///< 再最適化の閾値(実行回数)
  bool enableExceptions = true;               ///< 例外処理を有効にする
  bool enableDebugger = false;                ///< デバッガを有効にする
  std::string locale = "en-US";               ///< ロケール設定
};

/**
 * @brief 仮想マシンクラス
 *
 * JavaScriptを実行するための仮想マシン。バイトコード実行、JIT最適化、
 * スタック管理、例外処理などを担当します。高性能なJavaScript実行環境を提供します。
 */
class VM {
 public:
  /**
   * @brief デフォルト設定でVMを構築する
   */
  VM();

  /**
   * @brief カスタム設定でVMを構築する
   *
   * @param options VMの設定オプション
   */
  explicit VM(const VMOptions& options);

  /**
   * @brief デストラクタ
   */
  ~VM();

  /**
   * @brief コピー禁止
   */
  VM(const VM&) = delete;
  VM& operator=(const VM&) = delete;

  /**
   * @brief ムーブ可能
   */
  VM(VM&&)
  noexcept;
  VM& operator=(VM&&) noexcept;

  /**
   * @brief 新しい実行コンテキストを作成する
   *
   * @param globalObject グローバルオブジェクト
   * @return 新しいコンテキスト
   */
  std::shared_ptr<Context> createContext(std::shared_ptr<GlobalObject> globalObject = nullptr);

  /**
   * @brief JavaScriptコードを評価する
   *
   * @param context 実行コンテキスト
   * @param code 実行するJavaScriptコード
   * @param sourceFile ソースファイル名（オプション）
   * @param startLine 開始行番号（オプション）
   * @return 評価結果
   * @throws ExceptionPtr JavaScriptの例外が発生した場合
   */
  ValuePtr evaluate(std::shared_ptr<Context> context,
                    const std::string& code,
                    const std::string& sourceFile = "",
                    int startLine = 1);

  /**
   * @brief バイトコードを実行する
   *
   * @param context 実行コンテキスト
   * @param bytecodeBlock 実行するバイトコードブロック
   * @return 実行結果
   * @throws ExceptionPtr JavaScriptの例外が発生した場合
   */
  ValuePtr execute(std::shared_ptr<Context> context,
                   std::shared_ptr<BytecodeBlock> bytecodeBlock);

  /**
   * @brief 関数を呼び出す
   *
   * @param context 実行コンテキスト
   * @param function 呼び出す関数
   * @param thisValue thisの値
   * @param args 引数
   * @return 呼び出し結果
   * @throws ExceptionPtr JavaScriptの例外が発生した場合
   */
  ValuePtr callFunction(std::shared_ptr<Context> context,
                        ValuePtr function,
                        ValuePtr thisValue,
                        const std::vector<ValuePtr>& args);

  /**
   * @brief ネイティブ関数を登録する
   *
   * @param name 関数名
   * @param function 関数実装
   * @param context 登録先コンテキスト
   * @return 作成されたネイティブ関数オブジェクト
   */
  ValuePtr registerNativeFunction(const std::string& name,
                                  std::function<ValuePtr(const std::vector<ValuePtr>&)> function,
                                  std::shared_ptr<Context> context);

  /**
   * @brief ガベージコレクションを強制実行する
   */
  void collectGarbage();

  /**
   * @brief VMの実行統計を取得する
   *
   * @return 実行統計情報の文字列表現
   */
  std::string getStatistics() const;

  /**
   * @brief 現在のスタックトレースを取得する
   *
   * @return スタックトレースの文字列表現
   */
  std::string getStackTrace() const;

  /**
   * @brief 実行モードを設定する
   *
   * @param mode 新しい実行モード
   */
  void setExecutionMode(ExecutionMode mode);

  /**
   * @brief 現在の実行モードを取得する
   *
   * @return 現在の実行モード
   */
  ExecutionMode getExecutionMode() const;

  /**
   * @brief デバッガを有効化する
   *
   * @param port デバッガポート
   * @return デバッガが正常に有効化された場合はtrue
   */
  bool enableDebugger(int port = 9229);

  /**
   * @brief デバッガを無効化する
   */
  void disableDebugger();

  /**
   * @brief VMのヒープメモリ使用量を取得する
   *
   * @return ヒープメモリ使用量（バイト）
   */
  size_t getHeapSize() const;

  /**
   * @brief VMのオプションを取得する
   *
   * @return 現在のVMオプション
   */
  const VMOptions& getOptions() const;

  /**
   * @brief 例外を作成する
   *
   * @param message 例外メッセージ
   * @param errorType エラータイプ
   * @return 作成された例外
   */
  ExceptionPtr createException(const std::string& message, const std::string& errorType = "Error");

 private:
  /**
   * @brief VMの初期化処理
   *
   * @param options VMオプション
   */
  void initialize(const VMOptions& options);

  /**
   * @brief フレームスタックを管理する
   */
  void pushFrame(std::shared_ptr<CallFrame> frame);
  std::shared_ptr<CallFrame> popFrame();
  std::shared_ptr<CallFrame> currentFrame() const;

  /**
   * @brief 例外ハンドリング
   */
  void throwException(ExceptionPtr exception);
  ExceptionPtr catchException();
  bool hasUnhandledException() const;
  void clearException();

  // メンバ変数
  VMOptions m_options;
  std::shared_ptr<Heap> m_heap;
  InterpreterPtr m_interpreter;
  JITCompilerPtr m_jitCompiler;
  std::vector<std::shared_ptr<CallFrame>> m_callStack;
  std::atomic<ExecutionMode> m_executionMode;
  ExceptionPtr m_currentException;
  std::mutex m_vmMutex;
  bool m_isDebuggerAttached;
  std::atomic<uint64_t> m_executedInstructionCount;
  std::atomic<uint64_t> m_gcCycleCount;
};

}  // namespace core
}  // namespace aerojs