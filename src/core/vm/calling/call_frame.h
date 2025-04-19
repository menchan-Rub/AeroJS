/**
 * @file call_frame.h
 * @brief 仮想マシンのコールフレーム定義
 * 
 * JavaScriptエンジンのVM実行における関数呼び出しフレームの定義。
 * 各フレームは関数呼び出しの状態、ローカル変数、スコープ情報などを保持します。
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class Value;
class Scope;
class BytecodeBlock;
class BytecodeInstruction;
class Function;
class Interpreter;
class JITCompiledCode;

using ValuePtr = std::shared_ptr<Value>;
using ScopePtr = std::shared_ptr<Scope>;
using BytecodeBlockPtr = std::shared_ptr<BytecodeBlock>;
using FunctionPtr = std::shared_ptr<Function>;

/**
 * @brief フレームタイプの列挙型
 */
enum class FrameType {
  kGlobal,       ///< グローバルコードフレーム
  kFunction,     ///< 関数呼び出しフレーム
  kEval,         ///< eval呼び出しフレーム
  kModule,       ///< モジュールフレーム
  kNative,       ///< ネイティブ関数フレーム
  kGenerator,    ///< ジェネレータフレーム
  kAsync,        ///< 非同期関数フレーム
  kDebugger      ///< デバッガフレーム
};

/**
 * @brief 呼び出しフレームの状態
 */
enum class FrameState {
  kActive,       ///< アクティブなフレーム
  kSuspended,    ///< サスペンド状態（ジェネレータや非同期関数用）
  kCompleted,    ///< 完了状態
  kAborted       ///< 中断状態（例外などで）
};

/**
 * @brief コールフレームクラス
 * 
 * JavaScriptの実行コンテキストにおける関数呼び出しなどのスタックフレーム。
 * 各フレームはローカル変数、スコープチェーン、現在の実行位置などの情報を保持します。
 */
class CallFrame {
public:
  /**
   * @brief グローバルフレームを作成
   * 
   * @param context 実行コンテキスト
   * @param bytecodeBlock バイトコードブロック
   * @return グローバルフレームのポインタ
   */
  static std::shared_ptr<CallFrame> createGlobalFrame(
      std::shared_ptr<Context> context,
      BytecodeBlockPtr bytecodeBlock);

  /**
   * @brief 関数フレームを作成
   * 
   * @param context 実行コンテキスト
   * @param function 呼び出される関数
   * @param thisValue this値
   * @param args 引数リスト
   * @param parentFrame 親フレーム
   * @return 関数フレームのポインタ
   */
  static std::shared_ptr<CallFrame> createFunctionFrame(
      std::shared_ptr<Context> context,
      FunctionPtr function,
      ValuePtr thisValue,
      const std::vector<ValuePtr>& args,
      std::shared_ptr<CallFrame> parentFrame);

  /**
   * @brief evalフレームを作成
   * 
   * @param context 実行コンテキスト
   * @param bytecodeBlock バイトコードブロック
   * @param parentFrame 親フレーム
   * @param isDirectEval 直接evalかどうか
   * @return evalフレームのポインタ
   */
  static std::shared_ptr<CallFrame> createEvalFrame(
      std::shared_ptr<Context> context,
      BytecodeBlockPtr bytecodeBlock,
      std::shared_ptr<CallFrame> parentFrame,
      bool isDirectEval);

  /**
   * @brief モジュールフレームを作成
   * 
   * @param context 実行コンテキスト
   * @param bytecodeBlock バイトコードブロック
   * @param moduleNamespace モジュール名前空間オブジェクト
   * @return モジュールフレームのポインタ
   */
  static std::shared_ptr<CallFrame> createModuleFrame(
      std::shared_ptr<Context> context,
      BytecodeBlockPtr bytecodeBlock,
      ValuePtr moduleNamespace);

  /**
   * @brief ネイティブ関数フレームを作成
   * 
   * @param context 実行コンテキスト
   * @param nativeFunction ネイティブ関数
   * @param thisValue this値
   * @param args 引数リスト
   * @param parentFrame 親フレーム
   * @return ネイティブ関数フレームのポインタ
   */
  static std::shared_ptr<CallFrame> createNativeFrame(
      std::shared_ptr<Context> context,
      ValuePtr nativeFunction,
      ValuePtr thisValue,
      const std::vector<ValuePtr>& args,
      std::shared_ptr<CallFrame> parentFrame);

  /**
   * @brief コンストラクタ
   */
  CallFrame(
      std::shared_ptr<Context> context,
      FrameType type,
      BytecodeBlockPtr bytecodeBlock,
      ScopePtr scope,
      ValuePtr thisValue);

  /**
   * @brief デストラクタ
   */
  ~CallFrame();

  /**
   * @brief フレームのタイプを取得
   * 
   * @return フレームタイプ
   */
  FrameType getType() const;

  /**
   * @brief 実行コンテキストを取得
   * 
   * @return コンテキスト
   */
  std::shared_ptr<Context> getContext() const;

  /**
   * @brief バイトコードブロックを取得
   * 
   * @return バイトコードブロック
   */
  BytecodeBlockPtr getBytecodeBlock() const;

  /**
   * @brief 現在のスコープを取得
   * 
   * @return スコープ
   */
  ScopePtr getScope() const;

  /**
   * @brief this値を取得
   * 
   * @return this値
   */
  ValuePtr getThisValue() const;

  /**
   * @brief 現在の命令ポインタを取得
   * 
   * @return 命令ポインタ
   */
  size_t getInstructionPointer() const;

  /**
   * @brief 命令ポインタを設定
   * 
   * @param ip 新しい命令ポインタ
   */
  void setInstructionPointer(size_t ip);

  /**
   * @brief 次の命令に進む
   * 
   * @return 次の命令が存在する場合はtrue
   */
  bool advanceToNextInstruction();

  /**
   * @brief 現在の命令を取得
   * 
   * @return 現在の命令
   */
  const BytecodeInstruction& getCurrentInstruction() const;

  /**
   * @brief 関数の引数を取得
   * 
   * @return 引数リスト
   */
  const std::vector<ValuePtr>& getArguments() const;

  /**
   * @brief ローカル変数を取得
   * 
   * @param index 変数インデックス
   * @return 変数の値
   */
  ValuePtr getLocalVariable(size_t index) const;

  /**
   * @brief ローカル変数を設定
   * 
   * @param index 変数インデックス
   * @param value 設定する値
   */
  void setLocalVariable(size_t index, ValuePtr value);

  /**
   * @brief スコープ変数を取得
   * 
   * @param name 変数名
   * @return 変数の値
   */
  ValuePtr getScopeVariable(const std::string& name);

  /**
   * @brief スコープ変数を設定
   * 
   * @param name 変数名
   * @param value 設定する値
   * @return 設定に成功した場合はtrue
   */
  bool setScopeVariable(const std::string& name, ValuePtr value);

  /**
   * @brief 実行状態を取得
   * 
   * @return 現在の実行状態
   */
  FrameState getState() const;

  /**
   * @brief 実行状態を設定
   * 
   * @param state 新しい実行状態
   */
  void setState(FrameState state);

  /**
   * @brief 戻り値を設定
   * 
   * @param value 戻り値
   */
  void setReturnValue(ValuePtr value);

  /**
   * @brief 戻り値を取得
   * 
   * @return 戻り値
   */
  ValuePtr getReturnValue() const;

  /**
   * @brief 現在のソースの位置情報を取得
   * 
   * @return ソースファイル名、行番号、列番号の組
   */
  std::tuple<std::string, int, int> getSourcePosition() const;

  /**
   * @brief JITコンパイル済みコードを設定
   * 
   * @param compiledCode JITコンパイル済みコード
   */
  void setJITCompiledCode(std::shared_ptr<JITCompiledCode> compiledCode);

  /**
   * @brief JITコンパイル済みコードを取得
   * 
   * @return JITコンパイル済みコード
   */
  std::shared_ptr<JITCompiledCode> getJITCompiledCode() const;

  /**
   * @brief 親フレームを設定
   * 
   * @param parentFrame 親フレーム
   */
  void setParentFrame(std::shared_ptr<CallFrame> parentFrame);

  /**
   * @brief 親フレームを取得
   * 
   * @return 親フレーム
   */
  std::shared_ptr<CallFrame> getParentFrame() const;

  /**
   * @brief フレームが厳格モードかどうかを取得
   * 
   * @return 厳格モードの場合はtrue
   */
  bool isStrictMode() const;

  /**
   * @brief フレームのデバッグ文字列表現を取得
   * 
   * @return デバッグ文字列
   */
  std::string toString() const;

private:
  std::shared_ptr<Context> m_context;            ///< 実行コンテキスト
  FrameType m_type;                              ///< フレームタイプ
  BytecodeBlockPtr m_bytecodeBlock;              ///< バイトコードブロック
  ScopePtr m_scope;                              ///< 現在のスコープ
  ValuePtr m_thisValue;                          ///< this値
  size_t m_instructionPointer;                   ///< 命令ポインタ
  std::vector<ValuePtr> m_arguments;             ///< 引数リスト
  std::vector<ValuePtr> m_localVariables;        ///< ローカル変数
  ValuePtr m_returnValue;                        ///< 戻り値
  FrameState m_state;                            ///< 実行状態
  bool m_strictMode;                             ///< 厳格モード
  std::shared_ptr<CallFrame> m_parentFrame;      ///< 親フレーム
  std::shared_ptr<JITCompiledCode> m_jitCode;    ///< JITコンパイル済みコード
};

} // namespace core
} // namespace aerojs 