/**
 * @file interpreter.h
 * @brief バイトコードインタプリタの定義
 * 
 * このファイルはJavaScriptのバイトコード命令を実行するインタプリタの定義を提供します。
 * インタプリタはバイトコード命令を順次実行し、スタックマシンとして動作します。
 */

#ifndef AEROJS_CORE_VM_INTERPRETER_INTERPRETER_H_
#define AEROJS_CORE_VM_INTERPRETER_INTERPRETER_H_

#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

#include "bytecode_instruction.h"
#include "../../runtime/values/value.h"
#include "../../runtime/context/context.h"
#include "../stack/stack.h"
#include "../exception/exception.h"

namespace aerojs {
namespace core {

// 前方宣言
class CallFrame;
class FunctionObject;
class Environment;

/**
 * @brief バイトコードインタプリタクラス
 * 
 * バイトコード命令を実行するインタプリタクラスです。
 * スタックベースの仮想マシンとして実装され、各命令を順次実行します。
 */
class Interpreter {
public:
  /**
   * @brief デフォルトコンストラクタ
   */
  Interpreter();

  /**
   * @brief デストラクタ
   */
  ~Interpreter();

  /**
   * @brief バイトコード命令の配列を実行する
   * 
   * @param instructions 実行するバイトコード命令の配列
   * @param context 実行コンテキスト
   * @param environment 実行環境
   * @return ValuePtr 実行結果
   */
  ValuePtr execute(
      const std::vector<BytecodeInstruction>& instructions,
      ContextPtr context,
      std::shared_ptr<Environment> environment);

  /**
   * @brief 関数を呼び出す
   * 
   * @param func 呼び出す関数オブジェクト
   * @param args 引数の配列
   * @param thisValue thisの値
   * @param context 実行コンテキスト
   * @return ValuePtr 関数の戻り値
   */
  ValuePtr callFunction(
      std::shared_ptr<FunctionObject> func,
      const std::vector<ValuePtr>& args,
      ValuePtr thisValue,
      ContextPtr context);

  /**
   * @brief 例外をスローする
   * 
   * @param error エラーオブジェクト
   * @throws VMException スローされる例外
   */
  void throwException(ValuePtr error);

  /**
   * @brief インタプリタの現在の状態をリセットする
   */
  void reset();

private:
  /** @brief 命令実行関数の型定義 */
  using InstructionHandler = std::function<void(Interpreter*, const BytecodeInstruction&)>;

  /** @brief コールスタック */
  std::vector<std::shared_ptr<CallFrame>> m_callStack;

  /** @brief 値スタック */
  std::shared_ptr<Stack> m_stack;

  /** @brief 実行中のコンテキスト */
  ContextPtr m_currentContext;

  /** @brief 命令ハンドラマップ */
  std::unordered_map<Opcode, InstructionHandler> m_instructionHandlers;

  /** @brief 例外処理ハンドラスタック */
  std::vector<size_t> m_exceptionHandlers;

  /** @brief デバッグモードフラグ */
  bool m_debugMode;

  /**
   * @brief 命令ハンドラを初期化する
   */
  void initializeInstructionHandlers();

  /**
   * @brief 現在のコールフレームを取得する
   * 
   * @return std::shared_ptr<CallFrame> 現在のコールフレーム
   */
  std::shared_ptr<CallFrame> getCurrentCallFrame() const;

  /**
   * @brief 新しいコールフレームをプッシュする
   * 
   * @param frame プッシュするコールフレーム
   */
  void pushCallFrame(std::shared_ptr<CallFrame> frame);

  /**
   * @brief コールフレームをポップする
   * 
   * @return std::shared_ptr<CallFrame> ポップされたコールフレーム
   */
  std::shared_ptr<CallFrame> popCallFrame();

  /** @brief バイトコード命令ハンドラメソッド群 */
  // スタック操作
  void handlePush(const BytecodeInstruction& instruction);
  void handlePop(const BytecodeInstruction& instruction);
  void handleDuplicate(const BytecodeInstruction& instruction);
  void handleSwap(const BytecodeInstruction& instruction);

  // 算術演算
  void handleAdd(const BytecodeInstruction& instruction);
  void handleSub(const BytecodeInstruction& instruction);
  void handleMul(const BytecodeInstruction& instruction);
  void handleDiv(const BytecodeInstruction& instruction);
  void handleMod(const BytecodeInstruction& instruction);
  void handlePow(const BytecodeInstruction& instruction);
  void handleNeg(const BytecodeInstruction& instruction);
  void handleInc(const BytecodeInstruction& instruction);
  void handleDec(const BytecodeInstruction& instruction);

  // ビット演算
  void handleBitAnd(const BytecodeInstruction& instruction);
  void handleBitOr(const BytecodeInstruction& instruction);
  void handleBitXor(const BytecodeInstruction& instruction);
  void handleBitNot(const BytecodeInstruction& instruction);
  void handleLeftShift(const BytecodeInstruction& instruction);
  void handleRightShift(const BytecodeInstruction& instruction);
  void handleUnsignedRightShift(const BytecodeInstruction& instruction);

  // 論理演算
  void handleLogicalAnd(const BytecodeInstruction& instruction);
  void handleLogicalOr(const BytecodeInstruction& instruction);
  void handleLogicalNot(const BytecodeInstruction& instruction);

  // 比較演算
  void handleEqual(const BytecodeInstruction& instruction);
  void handleStrictEqual(const BytecodeInstruction& instruction);
  void handleNotEqual(const BytecodeInstruction& instruction);
  void handleStrictNotEqual(const BytecodeInstruction& instruction);
  void handleLessThan(const BytecodeInstruction& instruction);
  void handleLessThanOrEqual(const BytecodeInstruction& instruction);
  void handleGreaterThan(const BytecodeInstruction& instruction);
  void handleGreaterThanOrEqual(const BytecodeInstruction& instruction);
  void handleInstanceOf(const BytecodeInstruction& instruction);
  void handleIn(const BytecodeInstruction& instruction);

  // 制御フロー
  void handleJump(const BytecodeInstruction& instruction);
  void handleJumpIfTrue(const BytecodeInstruction& instruction);
  void handleJumpIfFalse(const BytecodeInstruction& instruction);
  void handleCall(const BytecodeInstruction& instruction);
  void handleReturn(const BytecodeInstruction& instruction);
  void handleThrow(const BytecodeInstruction& instruction);
  void handleEnterTry(const BytecodeInstruction& instruction);
  void handleLeaveTry(const BytecodeInstruction& instruction);
  void handleEnterCatch(const BytecodeInstruction& instruction);
  void handleLeaveCatch(const BytecodeInstruction& instruction);
  void handleEnterFinally(const BytecodeInstruction& instruction);
  void handleLeaveFinally(const BytecodeInstruction& instruction);

  // 変数操作
  void handleGetLocal(const BytecodeInstruction& instruction);
  void handleSetLocal(const BytecodeInstruction& instruction);
  void handleGetGlobal(const BytecodeInstruction& instruction);
  void handleSetGlobal(const BytecodeInstruction& instruction);
  void handleGetUpvalue(const BytecodeInstruction& instruction);
  void handleSetUpvalue(const BytecodeInstruction& instruction);
  void handleDeclareVar(const BytecodeInstruction& instruction);
  void handleDeclareConst(const BytecodeInstruction& instruction);
  void handleDeclareLet(const BytecodeInstruction& instruction);

  // オブジェクト操作
  void handleNewObject(const BytecodeInstruction& instruction);
  void handleNewArray(const BytecodeInstruction& instruction);
  void handleGetProperty(const BytecodeInstruction& instruction);
  void handleSetProperty(const BytecodeInstruction& instruction);
  void handleDeleteProperty(const BytecodeInstruction& instruction);
  void handleGetElement(const BytecodeInstruction& instruction);
  void handleSetElement(const BytecodeInstruction& instruction);
  void handleDeleteElement(const BytecodeInstruction& instruction);
  void handleNewFunction(const BytecodeInstruction& instruction);
  void handleNewClass(const BytecodeInstruction& instruction);
  void handleGetSuperProperty(const BytecodeInstruction& instruction);
  void handleSetSuperProperty(const BytecodeInstruction& instruction);

  // イテレータ操作
  void handleIteratorInit(const BytecodeInstruction& instruction);
  void handleIteratorNext(const BytecodeInstruction& instruction);
  void handleIteratorClose(const BytecodeInstruction& instruction);

  // 非同期操作
  void handleAwait(const BytecodeInstruction& instruction);
  void handleYield(const BytecodeInstruction& instruction);
  void handleYieldStar(const BytecodeInstruction& instruction);

  // その他
  void handleNop(const BytecodeInstruction& instruction);
  void handleDebugger(const BytecodeInstruction& instruction);
  void handleTypeOf(const BytecodeInstruction& instruction);
  void handleVoid(const BytecodeInstruction& instruction);
  void handleDelete(const BytecodeInstruction& instruction);
  void handleImport(const BytecodeInstruction& instruction);
  void handleExport(const BytecodeInstruction& instruction);
};

/**
 * @brief コールフレームクラス
 * 
 * 関数呼び出しのコンテキストを表すクラスです。
 * 各関数呼び出しごとに作成され、ローカル変数やプログラムカウンタなどの状態を保持します。
 */
class CallFrame {
public:
  /**
   * @brief コンストラクタ
   * 
   * @param function 関数オブジェクト
   * @param environment 実行環境
   * @param thisValue thisの値
   * @param returnAddress 戻りアドレス
   */
  CallFrame(
      std::shared_ptr<FunctionObject> function,
      std::shared_ptr<Environment> environment,
      ValuePtr thisValue,
      size_t returnAddress);

  /**
   * @brief デストラクタ
   */
  ~CallFrame();

  /**
   * @brief 関数オブジェクトを取得する
   * 
   * @return std::shared_ptr<FunctionObject> 関数オブジェクト
   */
  std::shared_ptr<FunctionObject> getFunction() const;

  /**
   * @brief 実行環境を取得する
   * 
   * @return std::shared_ptr<Environment> 実行環境
   */
  std::shared_ptr<Environment> getEnvironment() const;

  /**
   * @brief thisの値を取得する
   * 
   * @return ValuePtr thisの値
   */
  ValuePtr getThisValue() const;

  /**
   * @brief 戻りアドレスを取得する
   * 
   * @return size_t 戻りアドレス
   */
  size_t getReturnAddress() const;

  /**
   * @brief プログラムカウンタを取得する
   * 
   * @return size_t プログラムカウンタ
   */
  size_t getProgramCounter() const;

  /**
   * @brief プログラムカウンタを設定する
   * 
   * @param pc 新しいプログラムカウンタ値
   */
  void setProgramCounter(size_t pc);

  /**
   * @brief プログラムカウンタをインクリメントする
   * 
   * @return size_t インクリメント後のプログラムカウンタ
   */
  size_t incrementProgramCounter();

private:
  /** @brief 関数オブジェクト */
  std::shared_ptr<FunctionObject> m_function;

  /** @brief 実行環境 */
  std::shared_ptr<Environment> m_environment;

  /** @brief thisの値 */
  ValuePtr m_thisValue;

  /** @brief 戻りアドレス */
  size_t m_returnAddress;

  /** @brief プログラムカウンタ */
  size_t m_programCounter;
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_VM_INTERPRETER_INTERPRETER_H_ 