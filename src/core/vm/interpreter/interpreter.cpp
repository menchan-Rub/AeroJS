/**
 * @file interpreter.cpp
 * @brief バイトコードインタプリタの実装
 *
 * このファイルはバイトコード命令を実行するインタプリタの実装を提供します。
 * インタプリタはバイトコード命令を順次実行し、JavaScript言語の意味論に従って動作します。
 */

#include "interpreter.h"

#include <cassert>
#include <iostream>
#include <stdexcept>

#include "../../runtime/context/context.h"
#include "../../runtime/environment/environment.h"
#include "../../runtime/values/function.h"
#include "../../runtime/values/object.h"
#include "../../runtime/values/value.h"
#include "../exception/exception.h"
#include "../stack/stack.h"
#include "bytecode_instruction.h"

namespace aerojs {
namespace core {

//-----------------------------------------------------------------------------
// Interpreter クラスの実装
//-----------------------------------------------------------------------------

Interpreter::Interpreter()
    : m_stack(std::make_shared<Stack>()),
      m_debugMode(false) {
  initializeInstructionHandlers();
}

Interpreter::~Interpreter() {
  // デストラクタの実装
  m_callStack.clear();
  m_exceptionHandlers.clear();
  m_instructionHandlers.clear();
}

void Interpreter::initializeInstructionHandlers() {
  // スタック操作
  m_instructionHandlers[Opcode::PUSH] =
      [this](const BytecodeInstruction& instruction) { this->handlePush(instruction); };
  m_instructionHandlers[Opcode::POP] =
      [this](const BytecodeInstruction& instruction) { this->handlePop(instruction); };
  m_instructionHandlers[Opcode::DUPLICATE] =
      [this](const BytecodeInstruction& instruction) { this->handleDuplicate(instruction); };
  m_instructionHandlers[Opcode::SWAP] =
      [this](const BytecodeInstruction& instruction) { this->handleSwap(instruction); };

  // 算術演算
  m_instructionHandlers[Opcode::ADD] =
      [this](const BytecodeInstruction& instruction) { this->handleAdd(instruction); };
  m_instructionHandlers[Opcode::SUB] =
      [this](const BytecodeInstruction& instruction) { this->handleSub(instruction); };
  m_instructionHandlers[Opcode::MUL] =
      [this](const BytecodeInstruction& instruction) { this->handleMul(instruction); };
  m_instructionHandlers[Opcode::DIV] =
      [this](const BytecodeInstruction& instruction) { this->handleDiv(instruction); };
  m_instructionHandlers[Opcode::MOD] =
      [this](const BytecodeInstruction& instruction) { this->handleMod(instruction); };
  m_instructionHandlers[Opcode::POW] =
      [this](const BytecodeInstruction& instruction) { this->handlePow(instruction); };
  m_instructionHandlers[Opcode::NEG] =
      [this](const BytecodeInstruction& instruction) { this->handleNeg(instruction); };
  m_instructionHandlers[Opcode::INC] =
      [this](const BytecodeInstruction& instruction) { this->handleInc(instruction); };
  m_instructionHandlers[Opcode::DEC] =
      [this](const BytecodeInstruction& instruction) { this->handleDec(instruction); };

  // ビット演算
  m_instructionHandlers[Opcode::BIT_AND] =
      [this](const BytecodeInstruction& instruction) { this->handleBitAnd(instruction); };
  m_instructionHandlers[Opcode::BIT_OR] =
      [this](const BytecodeInstruction& instruction) { this->handleBitOr(instruction); };
  m_instructionHandlers[Opcode::BIT_XOR] =
      [this](const BytecodeInstruction& instruction) { this->handleBitXor(instruction); };
  m_instructionHandlers[Opcode::BIT_NOT] =
      [this](const BytecodeInstruction& instruction) { this->handleBitNot(instruction); };
  m_instructionHandlers[Opcode::LEFT_SHIFT] =
      [this](const BytecodeInstruction& instruction) { this->handleLeftShift(instruction); };
  m_instructionHandlers[Opcode::RIGHT_SHIFT] =
      [this](const BytecodeInstruction& instruction) { this->handleRightShift(instruction); };
  m_instructionHandlers[Opcode::UNSIGNED_RIGHT_SHIFT] =
      [this](const BytecodeInstruction& instruction) { this->handleUnsignedRightShift(instruction); };

  // 論理演算
  m_instructionHandlers[Opcode::LOGICAL_AND] =
      [this](const BytecodeInstruction& instruction) { this->handleLogicalAnd(instruction); };
  m_instructionHandlers[Opcode::LOGICAL_OR] =
      [this](const BytecodeInstruction& instruction) { this->handleLogicalOr(instruction); };
  m_instructionHandlers[Opcode::LOGICAL_NOT] =
      [this](const BytecodeInstruction& instruction) { this->handleLogicalNot(instruction); };

  // 比較演算
  m_instructionHandlers[Opcode::EQUAL] =
      [this](const BytecodeInstruction& instruction) { this->handleEqual(instruction); };
  m_instructionHandlers[Opcode::STRICT_EQUAL] =
      [this](const BytecodeInstruction& instruction) { this->handleStrictEqual(instruction); };
  m_instructionHandlers[Opcode::NOT_EQUAL] =
      [this](const BytecodeInstruction& instruction) { this->handleNotEqual(instruction); };
  m_instructionHandlers[Opcode::STRICT_NOT_EQUAL] =
      [this](const BytecodeInstruction& instruction) { this->handleStrictNotEqual(instruction); };
  m_instructionHandlers[Opcode::LESS_THAN] =
      [this](const BytecodeInstruction& instruction) { this->handleLessThan(instruction); };
  m_instructionHandlers[Opcode::LESS_THAN_OR_EQUAL] =
      [this](const BytecodeInstruction& instruction) { this->handleLessThanOrEqual(instruction); };
  m_instructionHandlers[Opcode::GREATER_THAN] =
      [this](const BytecodeInstruction& instruction) { this->handleGreaterThan(instruction); };
  m_instructionHandlers[Opcode::GREATER_THAN_OR_EQUAL] =
      [this](const BytecodeInstruction& instruction) { this->handleGreaterThanOrEqual(instruction); };
  m_instructionHandlers[Opcode::INSTANCEOF] =
      [this](const BytecodeInstruction& instruction) { this->handleInstanceOf(instruction); };
  m_instructionHandlers[Opcode::IN] =
      [this](const BytecodeInstruction& instruction) { this->handleIn(instruction); };

  // 制御フロー
  m_instructionHandlers[Opcode::JUMP] =
      [this](const BytecodeInstruction& instruction) { this->handleJump(instruction); };
  m_instructionHandlers[Opcode::JUMP_IF_TRUE] =
      [this](const BytecodeInstruction& instruction) { this->handleJumpIfTrue(instruction); };
  m_instructionHandlers[Opcode::JUMP_IF_FALSE] =
      [this](const BytecodeInstruction& instruction) { this->handleJumpIfFalse(instruction); };
  m_instructionHandlers[Opcode::CALL] =
      [this](const BytecodeInstruction& instruction) { this->handleCall(instruction); };
  m_instructionHandlers[Opcode::RETURN] =
      [this](const BytecodeInstruction& instruction) { this->handleReturn(instruction); };
  m_instructionHandlers[Opcode::THROW] =
      [this](const BytecodeInstruction& instruction) { this->handleThrow(instruction); };
  m_instructionHandlers[Opcode::ENTER_TRY] =
      [this](const BytecodeInstruction& instruction) { this->handleEnterTry(instruction); };
  m_instructionHandlers[Opcode::LEAVE_TRY] =
      [this](const BytecodeInstruction& instruction) { this->handleLeaveTry(instruction); };
  m_instructionHandlers[Opcode::ENTER_CATCH] =
      [this](const BytecodeInstruction& instruction) { this->handleEnterCatch(instruction); };
  m_instructionHandlers[Opcode::LEAVE_CATCH] =
      [this](const BytecodeInstruction& instruction) { this->handleLeaveCatch(instruction); };
  m_instructionHandlers[Opcode::ENTER_FINALLY] =
      [this](const BytecodeInstruction& instruction) { this->handleEnterFinally(instruction); };
  m_instructionHandlers[Opcode::LEAVE_FINALLY] =
      [this](const BytecodeInstruction& instruction) { this->handleLeaveFinally(instruction); };

  // 変数操作
  m_instructionHandlers[Opcode::GET_LOCAL] =
      [this](const BytecodeInstruction& instruction) { this->handleGetLocal(instruction); };
  m_instructionHandlers[Opcode::SET_LOCAL] =
      [this](const BytecodeInstruction& instruction) { this->handleSetLocal(instruction); };
  m_instructionHandlers[Opcode::GET_GLOBAL] =
      [this](const BytecodeInstruction& instruction) { this->handleGetGlobal(instruction); };
  m_instructionHandlers[Opcode::SET_GLOBAL] =
      [this](const BytecodeInstruction& instruction) { this->handleSetGlobal(instruction); };
  m_instructionHandlers[Opcode::GET_UPVALUE] =
      [this](const BytecodeInstruction& instruction) { this->handleGetUpvalue(instruction); };
  m_instructionHandlers[Opcode::SET_UPVALUE] =
      [this](const BytecodeInstruction& instruction) { this->handleSetUpvalue(instruction); };
  m_instructionHandlers[Opcode::DECLARE_VAR] =
      [this](const BytecodeInstruction& instruction) { this->handleDeclareVar(instruction); };
  m_instructionHandlers[Opcode::DECLARE_CONST] =
      [this](const BytecodeInstruction& instruction) { this->handleDeclareConst(instruction); };
  m_instructionHandlers[Opcode::DECLARE_LET] =
      [this](const BytecodeInstruction& instruction) { this->handleDeclareLet(instruction); };

  // オブジェクト操作
  m_instructionHandlers[Opcode::NEW_OBJECT] =
      [this](const BytecodeInstruction& instruction) { this->handleNewObject(instruction); };
  m_instructionHandlers[Opcode::NEW_ARRAY] =
      [this](const BytecodeInstruction& instruction) { this->handleNewArray(instruction); };
  m_instructionHandlers[Opcode::GET_PROPERTY] =
      [this](const BytecodeInstruction& instruction) { this->handleGetProperty(instruction); };
  m_instructionHandlers[Opcode::SET_PROPERTY] =
      [this](const BytecodeInstruction& instruction) { this->handleSetProperty(instruction); };
  m_instructionHandlers[Opcode::DELETE_PROPERTY] =
      [this](const BytecodeInstruction& instruction) { this->handleDeleteProperty(instruction); };
  m_instructionHandlers[Opcode::GET_ELEMENT] =
      [this](const BytecodeInstruction& instruction) { this->handleGetElement(instruction); };
  m_instructionHandlers[Opcode::SET_ELEMENT] =
      [this](const BytecodeInstruction& instruction) { this->handleSetElement(instruction); };
  m_instructionHandlers[Opcode::DELETE_ELEMENT] =
      [this](const BytecodeInstruction& instruction) { this->handleDeleteElement(instruction); };
  m_instructionHandlers[Opcode::NEW_FUNCTION] =
      [this](const BytecodeInstruction& instruction) { this->handleNewFunction(instruction); };
  m_instructionHandlers[Opcode::NEW_CLASS] =
      [this](const BytecodeInstruction& instruction) { this->handleNewClass(instruction); };
  m_instructionHandlers[Opcode::GET_SUPER_PROPERTY] =
      [this](const BytecodeInstruction& instruction) { this->handleGetSuperProperty(instruction); };
  m_instructionHandlers[Opcode::SET_SUPER_PROPERTY] =
      [this](const BytecodeInstruction& instruction) { this->handleSetSuperProperty(instruction); };

  // イテレータ操作
  m_instructionHandlers[Opcode::ITERATOR_INIT] =
      [this](const BytecodeInstruction& instruction) { this->handleIteratorInit(instruction); };
  m_instructionHandlers[Opcode::ITERATOR_NEXT] =
      [this](const BytecodeInstruction& instruction) { this->handleIteratorNext(instruction); };
  m_instructionHandlers[Opcode::ITERATOR_CLOSE] =
      [this](const BytecodeInstruction& instruction) { this->handleIteratorClose(instruction); };

  // 非同期操作
  m_instructionHandlers[Opcode::AWAIT] =
      [this](const BytecodeInstruction& instruction) { this->handleAwait(instruction); };
  m_instructionHandlers[Opcode::YIELD] =
      [this](const BytecodeInstruction& instruction) { this->handleYield(instruction); };
  m_instructionHandlers[Opcode::YIELD_STAR] =
      [this](const BytecodeInstruction& instruction) { this->handleYieldStar(instruction); };

  // その他
  m_instructionHandlers[Opcode::NOP] =
      [this](const BytecodeInstruction& instruction) { this->handleNop(instruction); };
  m_instructionHandlers[Opcode::DEBUGGER] =
      [this](const BytecodeInstruction& instruction) { this->handleDebugger(instruction); };
  m_instructionHandlers[Opcode::TYPE_OF] =
      [this](const BytecodeInstruction& instruction) { this->handleTypeOf(instruction); };
  m_instructionHandlers[Opcode::VOID] =
      [this](const BytecodeInstruction& instruction) { this->handleVoid(instruction); };
  m_instructionHandlers[Opcode::DELETE] =
      [this](const BytecodeInstruction& instruction) { this->handleDelete(instruction); };
  m_instructionHandlers[Opcode::IMPORT] =
      [this](const BytecodeInstruction& instruction) { this->handleImport(instruction); };
  m_instructionHandlers[Opcode::EXPORT] =
      [this](const BytecodeInstruction& instruction) { this->handleExport(instruction); };
}

ValuePtr Interpreter::execute(
    const std::vector<BytecodeInstruction>& instructions,
    ContextPtr context,
    std::shared_ptr<Environment> environment) {
  if (instructions.empty()) {
    return Value::createUndefined();
  }

  // コンテキストとスタックを初期化
  m_currentContext = context;
  m_stack->clear();

  try {
    // 命令を順次実行
    for (size_t pc = 0; pc < instructions.size(); pc++) {
      const BytecodeInstruction& instruction = instructions[pc];

      // 命令ハンドラを取得して実行
      auto handlerIt = m_instructionHandlers.find(instruction.getOpcode());
      if (handlerIt != m_instructionHandlers.end()) {
        handlerIt->second(instruction);
      } else {
        // 不明な命令の場合は例外をスロー
        std::string errorMsg = "Unknown opcode: " + std::to_string(static_cast<int>(instruction.getOpcode()));
        throwException(Value::createError(errorMsg));
      }
    }

    // スタックが空でない場合は最後の値を返す
    if (!m_stack->isEmpty()) {
      return m_stack->pop();
    }

    // スタックが空の場合はundefinedを返す
    return Value::createUndefined();
  } catch (const VMException& e) {
    // 例外をキャッチして再スロー
    throw;
  } catch (const std::exception& e) {
    // 標準例外をVMExceptionに変換
    throwException(Value::createError(e.what()));
  }

  // 通常はここには到達しない
  return Value::createUndefined();
}

ValuePtr Interpreter::callFunction(
    std::shared_ptr<FunctionObject> func,
    const std::vector<ValuePtr>& args,
    ValuePtr thisValue,
    ContextPtr context) {
  if (!func) {
    throwException(Value::createTypeError("Cannot call null or undefined"));
  }

  // 関数の環境を取得
  auto environment = func->getEnvironment();

  // 新しいコールフレームを作成
  size_t returnAddress = m_callStack.empty() ? 0 : getCurrentCallFrame()->getProgramCounter() + 1;
  auto callFrame = std::make_shared<CallFrame>(func, environment, thisValue, returnAddress);

  // コールスタックにプッシュ
  pushCallFrame(callFrame);

  try {
    // 関数本体の命令を実行
    auto instructions = func->getInstructions();

    // 引数をスタックにプッシュ
    for (const auto& arg : args) {
      m_stack->push(arg);
    }

    // 関数本体を実行
    return execute(instructions, context, environment);
  } catch (const VMException& e) {
    // 例外をキャッチして再スロー
    throw;
  } finally {
    // コールフレームをポップ
    popCallFrame();
  }

  // 通常はここには到達しない
  return Value::createUndefined();
}

void Interpreter::throwException(ValuePtr error) {
  // VMExceptionを投げる
  throw VMException(error);
}

void Interpreter::reset() {
  // インタプリタの状態をリセット
  m_stack->clear();
  m_callStack.clear();
  m_exceptionHandlers.clear();
  m_currentContext = nullptr;
}

std::shared_ptr<CallFrame> Interpreter::getCurrentCallFrame() const {
  if (m_callStack.empty()) {
    return nullptr;
  }
  return m_callStack.back();
}

void Interpreter::pushCallFrame(std::shared_ptr<CallFrame> frame) {
  if (frame) {
    m_callStack.push_back(frame);
  }
}

std::shared_ptr<CallFrame> Interpreter::popCallFrame() {
  if (m_callStack.empty()) {
    return nullptr;
  }

  auto frame = m_callStack.back();
  m_callStack.pop_back();
  return frame;
}

//-----------------------------------------------------------------------------
// バイトコード命令ハンドラの実装
//-----------------------------------------------------------------------------

// スタック操作
void Interpreter::handlePush(const BytecodeInstruction& instruction) {
  // 定数値をスタックにプッシュ
  m_stack->push(instruction.getOperand(0));
}

void Interpreter::handlePop(const BytecodeInstruction& instruction) {
  // スタックから値をポップ
  if (!m_stack->isEmpty()) {
    m_stack->pop();
  }
}

void Interpreter::handleDuplicate(const BytecodeInstruction& instruction) {
  // スタックの最上位の値を複製
  if (!m_stack->isEmpty()) {
    auto value = m_stack->peek();
    m_stack->push(value);
  }
}

void Interpreter::handleSwap(const BytecodeInstruction& instruction) {
  // スタックの上位2つの値を交換
  if (m_stack->size() >= 2) {
    auto value1 = m_stack->pop();
    auto value2 = m_stack->pop();
    m_stack->push(value1);
    m_stack->push(value2);
  }
}

// 算術演算
void Interpreter::handleAdd(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto right = m_stack->pop();
    auto left = m_stack->pop();
    m_stack->push(Value::add(left, right));
  }
}

void Interpreter::handleSub(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto right = m_stack->pop();
    auto left = m_stack->pop();
    m_stack->push(Value::subtract(left, right));
  }
}

void Interpreter::handleMul(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto right = m_stack->pop();
    auto left = m_stack->pop();
    m_stack->push(Value::multiply(left, right));
  }
}

void Interpreter::handleDiv(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto right = m_stack->pop();
    auto left = m_stack->pop();
    m_stack->push(Value::divide(left, right));
  }
}

void Interpreter::handleMod(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto right = m_stack->pop();
    auto left = m_stack->pop();
    m_stack->push(Value::modulo(left, right));
  }
}

void Interpreter::handlePow(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto right = m_stack->pop();
    auto left = m_stack->pop();
    m_stack->push(Value::power(left, right));
  }
}

void Interpreter::handleNeg(const BytecodeInstruction& instruction) {
  if (!m_stack->isEmpty()) {
    auto value = m_stack->pop();
    m_stack->push(Value::negate(value));
  }
}

void Interpreter::handleInc(const BytecodeInstruction& instruction) {
  if (!m_stack->isEmpty()) {
    auto value = m_stack->pop();
    m_stack->push(Value::increment(value));
  }
}

void Interpreter::handleDec(const BytecodeInstruction& instruction) {
  if (!m_stack->isEmpty()) {
    auto value = m_stack->pop();
    m_stack->push(Value::decrement(value));
  }
}

// ビット演算
void Interpreter::handleBitAnd(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto right = m_stack->pop();
    auto left = m_stack->pop();
    m_stack->push(Value::bitwiseAnd(left, right));
  }
}

void Interpreter::handleBitOr(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto right = m_stack->pop();
    auto left = m_stack->pop();
    m_stack->push(Value::bitwiseOr(left, right));
  }
}

void Interpreter::handleBitXor(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto right = m_stack->pop();
    auto left = m_stack->pop();
    m_stack->push(Value::bitwiseXor(left, right));
  }
}

void Interpreter::handleBitNot(const BytecodeInstruction& instruction) {
  if (!m_stack->isEmpty()) {
    auto value = m_stack->pop();
    m_stack->push(Value::bitwiseNot(value));
  }
}

void Interpreter::handleLeftShift(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto right = m_stack->pop();
    auto left = m_stack->pop();
    m_stack->push(Value::leftShift(left, right));
  }
}

void Interpreter::handleRightShift(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto right = m_stack->pop();
    auto left = m_stack->pop();
    m_stack->push(Value::rightShift(left, right));
  }
}

void Interpreter::handleUnsignedRightShift(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto right = m_stack->pop();
    auto left = m_stack->pop();
    m_stack->push(Value::unsignedRightShift(left, right));
  }
}

// 論理演算
void Interpreter::handleLogicalAnd(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto right = m_stack->pop();
    auto left = m_stack->pop();

    // 左オペランドが偽値なら左オペランドを返す
    if (!Value::toBoolean(left)) {
      m_stack->push(left);
    } else {
      // そうでなければ右オペランドを返す
      m_stack->push(right);
    }
  }
}

void Interpreter::handleLogicalOr(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto right = m_stack->pop();
    auto left = m_stack->pop();

    // 左オペランドが真値なら左オペランドを返す
    if (Value::toBoolean(left)) {
      m_stack->push(left);
    } else {
      // そうでなければ右オペランドを返す
      m_stack->push(right);
    }
  }
}

void Interpreter::handleLogicalNot(const BytecodeInstruction& instruction) {
  if (!m_stack->isEmpty()) {
    auto value = m_stack->pop();
    m_stack->push(Value::createBoolean(!Value::toBoolean(value)));
  }
}

// 制御フロー
void Interpreter::handleJump(const BytecodeInstruction& instruction) {
  // ジャンプ先アドレスを取得
  int jumpOffset = instruction.getOperandAsInt(0);
  auto currentFrame = getCurrentCallFrame();
  if (currentFrame) {
    // プログラムカウンタを更新
    currentFrame->setProgramCounter(currentFrame->getProgramCounter() + jumpOffset);
  }
}

void Interpreter::handleJumpIfTrue(const BytecodeInstruction& instruction) {
  if (!m_stack->isEmpty()) {
    // 条件値を取得
    auto condition = m_stack->pop();
    // 真の場合はジャンプ
    if (Value::toBoolean(condition)) {
      handleJump(instruction);
    }
  }
}

void Interpreter::handleJumpIfFalse(const BytecodeInstruction& instruction) {
  if (!m_stack->isEmpty()) {
    // 条件値を取得
    auto condition = m_stack->pop();
    // 偽の場合はジャンプ
    if (!Value::toBoolean(condition)) {
      handleJump(instruction);
    }
  }
}

void Interpreter::handleCall(const BytecodeInstruction& instruction) {
  // 引数の数を取得
  int argCount = instruction.getOperandAsInt(0);

  // 引数をスタックから取得
  std::vector<ValuePtr> args;
  for (int i = 0; i < argCount; i++) {
    if (!m_stack->isEmpty()) {
      args.insert(args.begin(), m_stack->pop());
    }
  }

  // 関数オブジェクトと this 値を取得
  auto thisValue = m_stack->pop();
  auto funcValue = m_stack->pop();

  // 関数呼び出しを実行
  auto func = std::dynamic_pointer_cast<FunctionObject>(funcValue);
  if (func) {
    auto result = callFunction(func, args, thisValue, m_currentContext);
    m_stack->push(result);
  } else {
    throwException(Value::createTypeError("Value is not a function"));
  }
}

void Interpreter::handleReturn(const BytecodeInstruction& instruction) {
  // スタックから戻り値を取得
  ValuePtr returnValue = Value::createUndefined();
  if (!m_stack->isEmpty()) {
    returnValue = m_stack->pop();
  }

  // 現在のコールフレームを取得
  auto currentFrame = getCurrentCallFrame();
  if (currentFrame) {
    // 戻り値をスタックにプッシュ
    m_stack->push(returnValue);

    // コールフレームをポップ
    popCallFrame();

    // 呼び出し元のフレームに制御を戻す
    if (!m_callStack.empty()) {
      auto callerFrame = getCurrentCallFrame();
      callerFrame->setProgramCounter(currentFrame->getReturnAddress());
    }
  }
}

void Interpreter::handleThrow(const BytecodeInstruction& instruction) {
  // スタックから例外オブジェクトを取得
  if (!m_stack->isEmpty()) {
    auto exception = m_stack->pop();
    throwException(exception);
  } else {
    throwException(Value::createError("Empty stack when throwing exception"));
  }
}

// 変数操作
void Interpreter::handleGetLocal(const BytecodeInstruction& instruction) {
  // ローカル変数のインデックスを取得
  int index = instruction.getOperandAsInt(0);

  // 現在の環境からローカル変数の値を取得
  auto currentFrame = getCurrentCallFrame();
  if (currentFrame) {
    auto environment = currentFrame->getEnvironment();
    auto value = environment->getLocalVariable(index);
    m_stack->push(value);
  }
}

void Interpreter::handleSetLocal(const BytecodeInstruction& instruction) {
  // ローカル変数のインデックスを取得
  int index = instruction.getOperandAsInt(0);

  // スタックから設定する値を取得
  if (!m_stack->isEmpty()) {
    auto value = m_stack->peek();  // ポップせずに取得

    // 現在の環境にローカル変数の値を設定
    auto currentFrame = getCurrentCallFrame();
    if (currentFrame) {
      auto environment = currentFrame->getEnvironment();
      environment->setLocalVariable(index, value);
    }
  }
}

// オブジェクト操作
void Interpreter::handleNewObject(const BytecodeInstruction& instruction) {
  // 新しいオブジェクトを作成
  auto obj = Object::create();
  m_stack->push(obj);
}

void Interpreter::handleGetProperty(const BytecodeInstruction& instruction) {
  // プロパティ名と対象オブジェクトを取得
  if (m_stack->size() >= 2) {
    auto propName = m_stack->pop();
    auto obj = m_stack->pop();

    // プロパティ値を取得
    auto result = Object::getProperty(obj, propName);
    m_stack->push(result);
  }
}

void Interpreter::handleSetProperty(const BytecodeInstruction& instruction) {
  // 値、プロパティ名、対象オブジェクトを取得
  if (m_stack->size() >= 3) {
    auto value = m_stack->pop();
    auto propName = m_stack->pop();
    auto obj = m_stack->pop();

    // プロパティ値を設定
    Object::setProperty(obj, propName, value);

    // 値をスタックに戻す
    m_stack->push(value);
  }
}

void Interpreter::handleDeleteProperty(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto propName = m_stack->pop();
    auto obj = m_stack->pop();

    // プロパティを削除
    bool success = Object::deleteProperty(obj, propName);
    m_stack->push(Value::createBoolean(success));
  }
}

void Interpreter::handleGetElement(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto index = m_stack->pop();
    auto obj = m_stack->pop();

    // 配列要素を取得
    auto result = Object::getElement(obj, index);
    m_stack->push(result);
  }
}

void Interpreter::handleSetElement(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 3) {
    auto value = m_stack->pop();
    auto index = m_stack->pop();
    auto obj = m_stack->pop();

    // 配列要素を設定
    Object::setElement(obj, index, value);

    // 値をスタックに戻す
    m_stack->push(value);
  }
}

void Interpreter::handleDeleteElement(const BytecodeInstruction& instruction) {
  if (m_stack->size() >= 2) {
    auto index = m_stack->pop();
    auto obj = m_stack->pop();

    // 配列要素を削除
    bool success = Object::deleteElement(obj, index);
    m_stack->push(Value::createBoolean(success));
  }
}

void Interpreter::handleNewFunction(const BytecodeInstruction& instruction) {
  // 関数のコードブロックインデックスを取得
  int codeBlockIndex = instruction.getOperand(0);

  // 関数名のインデックスを取得（名前付き関数の場合）
  std::string functionName = "";
  if (instruction.getOperandCount() > 1) {
    int nameIndex = instruction.getOperand(1);
    auto currentFrame = getCurrentCallFrame();
    auto env = currentFrame ? currentFrame->getEnvironment() : nullptr;
    functionName = env ? env->getConstantName(nameIndex) : "";
  }

  // 現在の環境を取得（クロージャに必要）
  auto currentFrame = getCurrentCallFrame();
  auto env = currentFrame ? currentFrame->getEnvironment() : nullptr;

  // コンテキストからコードブロックを取得
  if (m_currentContext && env) {
    auto codeBlock = m_currentContext->getCodeBlock(codeBlockIndex);
    if (codeBlock) {
      // 新しい関数オブジェクトを作成
      auto functionObj = FunctionObject::create(functionName, codeBlock, env);
      m_stack->push(functionObj);
      return;
    }
  }

  // エラー: コードブロックが見つからない
  m_stack->push(Value::createUndefined());
}

void Interpreter::handleNewClass(const BytecodeInstruction& instruction) {
  // クラスのコンストラクタのインデックスを取得
  int constructorIndex = instruction.getOperand(0);

  // クラス名のインデックスを取得
  int nameIndex = instruction.getOperand(1);

  // スーパークラスの有無を確認
  bool hasSuperClass = instruction.getOperandCount() > 2 && instruction.getOperand(2) != 0;

  // クラス名を取得
  auto currentFrame = getCurrentCallFrame();
  auto env = currentFrame ? currentFrame->getEnvironment() : nullptr;
  std::string className = env ? env->getConstantName(nameIndex) : "";

  // スーパークラスを取得（スタックにあれば）
  ValuePtr superClass = Value::createNull();
  if (hasSuperClass && !m_stack->isEmpty()) {
    superClass = m_stack->pop();

    // スーパークラスがコンストラクタでない場合はエラー
    if (!Object::isConstructor(superClass)) {
      throwException(Value::createTypeError("Super class must be a constructor"));
      return;
    }
  }

  // コンテキストからコンストラクタのコードブロックを取得
  if (m_currentContext && env) {
    auto constructorBlock = m_currentContext->getCodeBlock(constructorIndex);
    if (constructorBlock) {
      // クラスオブジェクトを作成
      auto classObj = Object::createClass(className, constructorBlock, env, superClass);
      m_stack->push(classObj);
      return;
    }
  }

  // エラー: コードブロックが見つからない
  m_stack->push(Value::createUndefined());
}

void Interpreter::handleGetSuperProperty(const BytecodeInstruction& instruction) {
  // プロパティ名と対象オブジェクトを取得
  if (m_stack->size() >= 2) {
    auto propName = m_stack->pop();
    auto receiverObj = m_stack->pop();  // 実際のレシーバーオブジェクト

    // 現在のメソッドのthisBindingを取得
    auto currentFrame = getCurrentCallFrame();
    if (!currentFrame) {
      throwException(Value::createReferenceError("Super property access requires this binding"));
      return;
    }

    auto thisValue = currentFrame->getThisValue();

    // メソッドのhomeObjectからスーパークラスのプロトタイプを取得
    auto func = currentFrame->getFunction();
    if (!func || !func->isMethod()) {
      throwException(Value::createReferenceError("Super property access requires method context"));
      return;
    }

    auto homeObject = func->getHomeObject();
    auto superProto = Object::getPrototypeOf(homeObject);

    // superのプロパティを取得
    auto result = Object::getSuperProperty(superProto, propName, thisValue);
    m_stack->push(result);
  }
}

void Interpreter::handleSetSuperProperty(const BytecodeInstruction& instruction) {
  // 値、プロパティ名、対象オブジェクトを取得
  if (m_stack->size() >= 3) {
    auto value = m_stack->pop();
    auto propName = m_stack->pop();
    auto receiverObj = m_stack->pop();  // 実際のレシーバーオブジェクト

    // 現在のメソッドのthisBindingを取得
    auto currentFrame = getCurrentCallFrame();
    if (!currentFrame) {
      throwException(Value::createReferenceError("Super property access requires this binding"));
      return;
    }

    auto thisValue = currentFrame->getThisValue();

    // メソッドのhomeObjectからスーパークラスのプロトタイプを取得
    auto func = currentFrame->getFunction();
    if (!func || !func->isMethod()) {
      throwException(Value::createReferenceError("Super property access requires method context"));
      return;
    }

    auto homeObject = func->getHomeObject();
    auto superProto = Object::getPrototypeOf(homeObject);

    // superのプロパティを設定
    Object::setSuperProperty(superProto, propName, value, thisValue);

    // 値をスタックに戻す
    m_stack->push(value);
  }
}

// イテレータ操作
void Interpreter::handleIteratorInit(const BytecodeInstruction& instruction) {
  if (!m_stack->isEmpty()) {
    auto obj = m_stack->pop();

    // イテレータを取得
    auto iterator = Object::getIterator(obj);
    m_stack->push(iterator);
  }
}

void Interpreter::handleIteratorNext(const BytecodeInstruction& instruction) {
  if (!m_stack->isEmpty()) {
    auto iterator = m_stack->pop();

    // イテレータの次の値を取得
    auto result = Object::iteratorNext(iterator);
    m_stack->push(result);
  }
}

void Interpreter::handleIteratorClose(const BytecodeInstruction& instruction) {
  if (!m_stack->isEmpty()) {
    auto iterator = m_stack->pop();

    // イテレータをクローズ
    Object::iteratorClose(iterator);
  }
}

// --- 非同期操作 ---

/**
 * @brief AWAIT 命令を処理します。
 * @details スタックトップの値 (Promise であることが期待される) を取り出し、
 *          その Promise が解決されるまで現在の実行コンテキスト (CallFrame) を中断します。
 *          Promise でない値が渡された場合は、`Promise.resolve` でラップされます。
 *          中断後、Promise が解決されたら、その値で実行を再開します。
 *          Promise が拒否された場合は、例外を送出します。
 * @param instruction AWAIT バイトコード命令
 * @throws Value 例外 (スタックが空、フレームがない、Promise 関連のエラーなど)
 * @note この関数は実行の中断を要求します。実際のスケジューリングと再開は
 *       インタープリタのメインループまたはイベントループが担当する必要があります。
 *       この実装では、中断が必要であることを示し、関連情報をフレームに設定します。
 */
void Interpreter::handleAwait(const BytecodeInstruction& instruction) {
  // まず、スタックが空ではないかを確認します。await する対象の値が必要です。
  if (m_stack->isEmpty()) {
    // スタックが空の場合、処理を続行できないため、型エラーをスローします。
    throwException(Value::createTypeError("非同期処理(Await)にはスタック上の値が必要です。"));
    return;  // 例外がスローされたら、この関数の実行はここで終了します。
  }

  // スタックの一番上から、await の対象となる値を取り出します。
  Value promiseValue = m_stack->pop();

  // 取り出した値が ECMAScript の Promise オブジェクトかを確認します。
  // もし Promise でなければ、仕様に従って `Promise.resolve()` を使って
  // その値を解決済みの Promise でラップします。これにより、どんな値でも await 可能になります。
  if (!Object::isPromise(promiseValue)) {
    // `Object::createResolvedPromise` は、与えられた値で解決される新しい Promise を生成します。
    // この処理中にメモリ確保エラーなどが発生する可能性も考慮する必要があります。
    try {
      promiseValue = Object::createResolvedPromise(promiseValue);
    } catch (const Value& error) {
      // Promise の生成に失敗した場合、そのエラーをそのままスローします。
      throwException(error);
      return;
    } catch (const std::bad_alloc& e) {
      // メモリ確保失敗は致命的なエラーとして処理します。
      throwException(Value::createMemoryError("Promise生成中にメモリが不足しました。"));
      return;
    }
  }

  // 現在実行中の関数に対応するコールフレームを取得します。
  // await は通常、async 関数内で使用されるため、フレームが存在するはずです。
  CallFrame* currentFrame = getCurrentCallFrame();
  if (!currentFrame) {
    // フレームが存在しない状況 (グローバルスコープなど) で await が呼ばれた場合、
    // これは文法エラーにあたります (トップレベル await は別途考慮)。
    throwException(Value::createSyntaxError("Await は async 関数内でのみ有効です。"));
    return;
  }

  // --- 実行の中断と再開の準備 ---
  // ここからが await の核心部分です。現在の関数の実行を一時停止し、
  // 指定された Promise の状態が変わるのを待つ必要があります。

  // 1. フレームの中断:
  //    現在の実行状態 (次に実行すべき命令の位置 = プログラムカウンタ、スタックの状態など) を
  //    コールフレームに保存します。これにより、後で正確な場所から再開できます。
  //    (具体的な保存方法は CallFrame の実装に依存します)
  currentFrame->suspendExecution(m_programCounter);  // プログラムカウンタを保存 (仮のメソッド名)

  // 2. 再開処理の登録:
  //    指定された Promise (`promiseValue`) が解決 (fulfilled) または拒否 (rejected) されたときに
  //    呼び出されるコールバック関数を、Promise の内部メカニズムに登録します。
  //    これは JavaScript の `.then(onFulfilled, onRejected)` に相当する内部処理です。
  //    - 解決時: 保存されたフレームを再開キューに入れ、解決値を渡す。
  //    - 拒否時: 保存されたフレームを再開キューに入れ、拒否理由 (エラー) を渡す。
  //    この登録処理は、Promise オブジェクトとスケジューラ間の連携が必要です。
  //    例: m_scheduler->addPromiseCallback(promiseValue, currentFrame);

  // 3. インタープリタへの中断通知:
  //    インタープリタのメインループに対して、現在の実行を中断し、
  //    イベントループや他の待機中のタスクに制御を渡す必要があることを伝えます。
  //    これは、インタープリタの状態を示すフラグや特別な戻り値で実現します。
  currentFrame->setAwaitingPromise(promiseValue);     // どの Promise を待っているかフレームに記録 (仮)
  m_executionState = ExecutionState::SuspendedAwait;  // インタープリタの状態を「Awaitにより中断中」に設定

  // この関数の処理はここまでです。実際の実行再開は、Promise が解決/拒否され、
  // スケジューラが該当フレームの実行を再スケジュールした時に行われます。
  // 再開時には、解決値がスタックにプッシュされるか、拒否理由が例外としてスローされます。

  // 注意: 上記のコメントは理想的な実装を示しています。実際のコードは、
  //       インタープリタ全体のアーキテクチャ (イベントループ、タスクキュー、
  //       CallFrame の詳細設計) に大きく依存します。
}

/**
 * @brief YIELD 命令を処理します。
 * @details スタックトップの値を取り出し、現在のジェネレータ関数を中断します。
 *          取り出した値は、イテレータの `next()` メソッドの戻り値 (IteratorResult) の
 *          `value` プロパティとして、`{ value: yieldedValue, done: false }` の形で返されます。
 *          ジェネレータが `next(resumeValue)` で再開されるとき、`resumeValue` が
 *          `yield` 式自体の評価結果としてスタックに積まれます。
 * @param instruction YIELD バイトコード命令
 * @throws Value 例外 (スタックが空、フレームがない、ジェネレータでない関数での yield など)
 * @note この関数はジェネレータの実行の中断を要求します。状態保存と再開は、
 *       インタープリタのメインループとジェネレータオブジェクトの管理機構が連携して行います。
 */
void Interpreter::handleYield(const BytecodeInstruction& instruction) {
  // ジェネレータから外部に渡す (yield する) 値がスタックの一番上にあるはずです。
  if (m_stack->isEmpty()) {
    // yield する値がないのはエラーです。
    throwException(Value::createTypeError("Yield 操作にはスタック上の値が必要です。"));
    return;
  }
  // スタックから yield する値を取り出します。
  Value yieldedValue = m_stack->pop();

  // 現在の実行コンテキストであるコールフレームを取得します。
  CallFrame* currentFrame = getCurrentCallFrame();
  if (!currentFrame) {
    // yield は関数内でしか使用できません。フレームがないのは異常な状態です。
    throwException(Value::createSyntaxError("Yield はジェネレータ関数内でのみ有効です。"));
    return;
  }

  // 現在のフレームがジェネレータ関数のものかを確認する必要があります。
  // (CallFrame に `isGenerator()` のようなメソッドがあると仮定)
  if (!currentFrame->isGenerator()) {
    // ジェネレータでない関数で yield を使うことはできません。
    throwException(Value::createSyntaxError("Yield はジェネレータ関数内でのみ有効です。"));
    return;
  }

  // --- ジェネレータの中断処理 ---
  // 1. 実行状態の保存:
  //    ジェネレータを中断し、後で正確に再開できるように、現在の状態
  //    (プログラムカウンタ、スタックの状態、ローカル変数など) を
  //    ジェネレータオブジェクト (または関連するコンテキスト) に保存します。
  currentFrame->suspendExecution(m_programCounter);  // 実行位置を保存 (仮)

  // 2. IteratorResult の作成:
  //    ジェネレータの `next()` メソッドの呼び出し元に返すためのオブジェクトを作成します。
  //    これは `{ value: yieldedValue, done: false }` という形式になります。
  //    `done: false` はジェネレータがまだ完了していないことを示します。
  Value iteratorResult;
  try {
    iteratorResult = Object::createIteratorResult(yieldedValue, false);
  } catch (const Value& error) {
    throwException(error);  // オブジェクト生成失敗
    return;
  } catch (const std::bad_alloc& e) {
    throwException(Value::createMemoryError("IteratorResult生成中にメモリが不足しました。"));
    return;
  }

  // 3. 中断と結果の通知:
  //    インタープリタのメインループに、ジェネレータが中断したことと、
  //    `next()` の呼び出し元に返す `iteratorResult` を伝えます。
  m_yieldedValue = iteratorResult;                    // 外部に返す値をインタープリタのメンバ変数に保存 (仮)
  m_executionState = ExecutionState::SuspendedYield;  // インタープリタの状態を「Yieldにより中断中」に設定

  // 4. yield 式自体の評価結果:
  //    ジェネレータが `next(resumeValue)` で再開されるとき、`resumeValue` が
  //    この `yield` 式があった場所での評価結果となります。
  //    中断時点では、次に渡される値は不明なので、スタックには何も積まないか、
  //    あるいは `undefined` を積んでおくのが一般的です。再開時にこの値は上書きされます。
  m_stack->push(Value::createUndefined());  // yield 式の評価結果のプレースホルダーとして undefined を積む

  // この関数の処理はここまでです。メインループが `m_executionState` を見て中断処理を行い、
  // `m_yieldedValue` を `next()` の呼び出し元に返します。
  // 再開時には、`next(resumeValue)` で渡された `resumeValue` がスタックに積まれ、
  // 保存されたプログラムカウンタから実行が再開されます。
}

/**
 * @brief YIELD_STAR 命令を処理します。 (委譲付き yield)
 * @details スタックトップのイテラブルオブジェクトからイテレータ (サブイテレータ) を取得し、
 *          そのイテレータに処理を委譲します。サブイテレータが値を yield するたびに、
 *          現在のジェネレータもその値を外部に yield (中断) します。
 *          サブイテレータが完了 (`done: true`) したら、その完了値 (`value`) を
 *          現在のジェネレータのスタックにプッシュし、実行を継続します。
 * @param instruction YIELD_STAR バイトコード命令
 * @throws Value 例外 (スタックが空、イテラブルでない、イテレータ取得エラーなど)
 * @note この命令は、サブイテレータの状態管理を含むため、`yield` よりも複雑です。
 *       エラー処理 (サブイテレータからの例外伝播) や `return`/`throw` の委譲も考慮が必要です。
 */
void Interpreter::handleYieldStar(const BytecodeInstruction& instruction) {
  // 委譲先となるイテラブルオブジェクトがスタックの一番上にあるはずです。
  if (m_stack->isEmpty()) {
    throwException(Value::createTypeError("Yield* 操作にはスタック上のイテラブルな値が必要です。"));
    return;
  }
  Value iterable = m_stack->pop();

  // 現在のコールフレームを取得し、ジェネレータ関数内であることを確認します。
  CallFrame* currentFrame = getCurrentCallFrame();
  if (!currentFrame || !currentFrame->isGenerator()) {
    throwException(Value::createSyntaxError("Yield* はジェネレータ関数内でのみ有効です。"));
    return;
  }

  // --- サブイテレーションの準備 ---
  // 1. イテレータの取得:
  //    与えられた `iterable` オブジェクトからイテレータ (サブイテレータ) を取得します。
  //    これには、オブジェクトの `[Symbol.iterator]()` メソッドを呼び出す内部処理が必要です。
  Value subIterator;
  try {
    subIterator = Object::getIterator(iterable);
  } catch (const Value& error) {
    // イテレータの取得に失敗した場合 (例: `iterable` がイテラブルでなかった)、エラーをスローします。
    throwException(error);
    return;
  }

  // 2. 委譲状態の設定:
  //    現在のフレームに、どのサブイテレータに処理を委譲しているかの情報を保存します。
  //    これにより、中断・再開時に正しいサブイテレータを操作できます。
  currentFrame->setDelegatedIterator(subIterator);   // 委譲先イテレータをフレームに記録 (仮)
  currentFrame->suspendExecution(m_programCounter);  // yield* 命令の次の位置を保存 (仮)

  // --- 最初のイテレーション実行 ---
  // yield* は、サブイテレータが完了するまで `next()` を呼び出し続けるループ構造です。
  // ここでは、そのループの最初のステップを実行します。
  // ジェネレータが再開されたときに `next(value)` で渡された値があれば、
  // それをサブイテレータの `next()` に渡す必要があります。初回は通常 undefined です。
  Value valueToSend = Value::createUndefined();  // 初回または再開時に渡す値 (フレームから取得するべき場合もある)

  try {
    // サブイテレータの `next()` メソッドを呼び出します。
    Value nextResult = Object::iteratorNext(subIterator, valueToSend);

    // `next()` の戻り値 (IteratorResult オブジェクト) を解析します。
    bool isDone = Object::getIteratorResultDone(nextResult);         // done プロパティを取得
    Value resultValue = Object::getIteratorResultValue(nextResult);  // value プロパティを取得

    if (isDone) {
      // --- サブイテレータ完了 ---
      // サブイテレータが完了 (`done: true`) した場合は、その完了値 (`resultValue`) を
      // 現在のジェネレータのスタックにプッシュします。これが `yield*` 式全体の評価結果となります。
      m_stack->push(resultValue);
      // フレームの委譲状態を解除します。
      currentFrame->clearDelegatedIterator();  // 委譲終了 (仮)
      // 実行は `yield*` 命令の次の命令から継続されます (プログラムカウンタは既に進んでいるはず)。
      m_executionState = ExecutionState::Running;  // 実行継続
    } else {
      // --- サブイテレータが Yield ---
      // サブイテレータがまだ完了しておらず (`done: false`)、値を yield した場合は、
      // `handleYield` と同様に、現在のジェネレータを中断します。
      // ただし、外部に返す IteratorResult は、サブイテレータが返した `nextResult` そのものです。
      m_yieldedValue = nextResult;                            // 外部に返す値を設定
      m_executionState = ExecutionState::SuspendedYieldStar;  // 状態を「Yield*により中断中」に設定
      // yield* 式自体の評価結果はまだ確定しないため、スタックには何も積まないか undefined を積みます。
      m_stack->push(Value::createUndefined());  // プレースホルダー
                                                // フレームは中断状態であり、委譲先イテレータの情報も保持されたままです。
    }
  } catch (const Value& error) {
    // --- エラー処理 ---
    // サブイテレータの `next()` 呼び出し中にエラーが発生した場合、
    // そのエラーを現在のジェネレータからスローする必要があります。
    // (サブイテレータに `throw()` メソッドがあれば、それを呼び出す仕様もありますが、ここでは単純化)
    currentFrame->clearDelegatedIterator();      // エラー発生時は委譲状態を解除
    throwException(error);                       // エラーを伝播
    m_executionState = ExecutionState::Running;  // エラー発生後は通常実行に戻る (例外処理へ)
  } catch (const std::bad_alloc& e) {
    currentFrame->clearDelegatedIterator();
    throwException(Value::createMemoryError("Yield* 処理中にメモリが不足しました。"));
    m_executionState = ExecutionState::Running;
  }

  // この関数の処理はここまでです。メインループが `m_executionState` を見て、
  // 中断 (`SuspendedYieldStar`) または継続 (`Running`) を判断します。
  // 中断した場合、再開時には `handleYieldStar` (または専用の再開処理) が再度呼び出され、
  // フレームに保存されたサブイテレータに対して次の `next()` が実行されます。
}

// その他の操作
void Interpreter::handleTypeOf(const BytecodeInstruction& instruction) {
  if (!m_stack->isEmpty()) {
    auto value = m_stack->pop();

    // typeof演算子の結果を取得
    std::string typeofResult = Value::typeOf(value);
    m_stack->push(Value::createString(typeofResult));
  }
}

void Interpreter::handleVoid(const BytecodeInstruction& instruction) {
  if (!m_stack->isEmpty()) {
    // 値を無視してundefinedを返す
    m_stack->pop();
    m_stack->push(Value::createUndefined());
  }
}

void Interpreter::handleDelete(const BytecodeInstruction& instruction) {
  int deleteType = instruction.getOperand(0);

  switch (deleteType) {
    case 0:  // 変数の削除
    {
      int nameIndex = instruction.getOperand(1);

      // 変数名を取得
      auto currentFrame = getCurrentCallFrame();
      auto env = currentFrame ? currentFrame->getEnvironment() : nullptr;
      std::string varName = env ? env->getConstantName(nameIndex) : "";

      // 厳格モードでの変数削除は常にfalse
      if (currentFrame && currentFrame->getFunction() && currentFrame->getFunction()->isStrictMode()) {
        m_stack->push(Value::createBoolean(false));
        return;
      }

      // 環境から変数を削除
      bool success = false;
      if (env && !varName.empty()) {
        success = env->deleteVariable(varName);
      }

      m_stack->push(Value::createBoolean(success));
    } break;

    case 1:  // プロパティの削除
      if (m_stack->size() >= 2) {
        auto propName = m_stack->pop();
        auto obj = m_stack->pop();

        // プロパティを削除
        bool success = Object::deleteProperty(obj, propName);
        m_stack->push(Value::createBoolean(success));
      }
      break;

    case 2:  // 配列要素の削除
      if (m_stack->size() >= 2) {
        auto index = m_stack->pop();
        auto obj = m_stack->pop();

        // 配列要素を削除
        bool success = Object::deleteElement(obj, index);
        m_stack->push(Value::createBoolean(success));
      }
      break;

    default:
      // 不明な削除タイプ
      m_stack->push(Value::createBoolean(false));
      break;
  }
}

void Interpreter::handleImport(const BytecodeInstruction& instruction) {
  // モジュールのパスやスペックを取得
  int specifierIndex = instruction.getOperand(0);

  // モジュールスペックを取得
  auto currentFrame = getCurrentCallFrame();
  auto env = currentFrame ? currentFrame->getEnvironment() : nullptr;
  std::string specifier = env ? env->getConstantName(specifierIndex) : "";

  if (!specifier.empty() && m_currentContext) {
    // モジュールの読み込みを要求
    auto moduleNamespace = m_currentContext->importModule(specifier);
    m_stack->push(moduleNamespace);
  } else {
    // エラー: モジュール指定子が見つからない
    throwException(Value::createError("Cannot resolve module specifier"));
  }
}

void Interpreter::handleExport(const BytecodeInstruction& instruction) {
  // エクスポートの種類を取得
  int exportType = instruction.getOperand(0);

  switch (exportType) {
    case 0:  // 名前付きエクスポート
    {
      int localNameIndex = instruction.getOperand(1);
      int exportNameIndex = instruction.getOperand(2);

      // ローカル名とエクスポート名を取得
      auto currentFrame = getCurrentCallFrame();
      auto env = currentFrame ? currentFrame->getEnvironment() : nullptr;
      std::string localName = env ? env->getConstantName(localNameIndex) : "";
      std::string exportName = env ? env->getConstantName(exportNameIndex) : "";

      if (!localName.empty() && !exportName.empty() && m_currentContext) {
        // 現在のモジュールからローカル変数を取得
        auto value = env->getVariable(localName);

        // モジュールの名前空間オブジェクトに追加
        m_currentContext->exportValue(exportName, value);
      }
    } break;

    case 1:  // デフォルトエクスポート
      if (!m_stack->isEmpty()) {
        auto defaultValue = m_stack->pop();

        if (m_currentContext) {
          // デフォルトエクスポートとして設定
          m_currentContext->exportDefault(defaultValue);
        }
      }
      break;

    case 2:  // 再エクスポート
    {
      int moduleSpecifierIndex = instruction.getOperand(1);
      int exportNameIndex = instruction.getOperand(2);

      // モジュール指定子とエクスポート名を取得
      auto currentFrame = getCurrentCallFrame();
      auto env = currentFrame ? currentFrame->getEnvironment() : nullptr;
      std::string moduleSpecifier = env ? env->getConstantName(moduleSpecifierIndex) : "";
      std::string exportName = env ? env->getConstantName(exportNameIndex) : "";

      if (!moduleSpecifier.empty() && !exportName.empty() && m_currentContext) {
        // 別のモジュールから再エクスポート
        m_currentContext->reExport(moduleSpecifier, exportName);
      }
    } break;
  }
}

// 変数操作
void Interpreter::handleGetGlobal(const BytecodeInstruction& instruction) {
  // グローバル変数の名前またはインデックスを取得
  int nameIndex = instruction.getOperand(0);

  // 変数名を取得
  auto currentFrame = getCurrentCallFrame();
  auto env = currentFrame ? currentFrame->getEnvironment() : nullptr;
  std::string varName = env ? env->getConstantName(nameIndex) : "";

  if (!varName.empty() && m_currentContext) {
    // グローバルオブジェクトから変数を取得
    auto globalObj = m_currentContext->getGlobalObject();
    auto value = Object::getProperty(globalObj, varName);
    m_stack->push(value);
  } else {
    // エラー: グローバル変数の名前が見つからない
    m_stack->push(Value::createUndefined());
  }
}

void Interpreter::handleSetGlobal(const BytecodeInstruction& instruction) {
  // グローバル変数の名前またはインデックスを取得
  int nameIndex = instruction.getOperand(0);

  // 変数名を取得
  auto currentFrame = getCurrentCallFrame();
  auto env = currentFrame ? currentFrame->getEnvironment() : nullptr;
  std::string varName = env ? env->getConstantName(nameIndex) : "";

  if (!varName.empty() && m_currentContext && !m_stack->isEmpty()) {
    // 値を取得
    auto value = m_stack->peek();  // ポップしない

    // グローバルオブジェクトに変数を設定
    auto globalObj = m_currentContext->getGlobalObject();
    Object::setProperty(globalObj, varName, value);
  }
}

void Interpreter::handleGetUpvalue(const BytecodeInstruction& instruction) {
  // アップバリューのインデックスを取得
  int upvalueIndex = instruction.getOperand(0);

  // 現在の環境からアップバリューを取得
  auto currentFrame = getCurrentCallFrame();
  if (currentFrame) {
    auto func = currentFrame->getFunction();
    if (func) {
      auto value = func->getUpvalue(upvalueIndex);
      m_stack->push(value);
    } else {
      m_stack->push(Value::createUndefined());
    }
  } else {
    m_stack->push(Value::createUndefined());
  }
}

void Interpreter::handleSetUpvalue(const BytecodeInstruction& instruction) {
  // アップバリューのインデックスを取得
  int upvalueIndex = instruction.getOperand(0);

  // スタックから設定する値を取得
  if (!m_stack->isEmpty()) {
    auto value = m_stack->peek();  // ポップせずに取得

    // 現在の環境にアップバリューの値を設定
    auto currentFrame = getCurrentCallFrame();
    if (currentFrame) {
      auto func = currentFrame->getFunction();
      if (func) {
        func->setUpvalue(upvalueIndex, value);
      }
    }
  }
}

void Interpreter::handleDeclareVar(const BytecodeInstruction& instruction) {
  // 変数名のインデックスを取得
  int nameIndex = instruction.getOperand(0);

  // 変数の初期値を取得（スタックにあれば）
  ValuePtr initialValue = Value::createUndefined();
  if (instruction.getOperandCount() > 1 && instruction.getOperand(1) != 0) {
    if (!m_stack->isEmpty()) {
      initialValue = m_stack->pop();
    }
  }

  // 変数名を取得
  auto currentFrame = getCurrentCallFrame();
  auto env = currentFrame ? currentFrame->getEnvironment() : nullptr;
  std::string varName = env ? env->getConstantName(nameIndex) : "";

  if (!varName.empty() && env) {
    // 環境に変数を宣言
    env->declareVariable(varName, initialValue, false);
  }
}

void Interpreter::handleDeclareConst(const BytecodeInstruction& instruction) {
  // 定数名のインデックスを取得
  int nameIndex = instruction.getOperand(0);

  // 定数の値を取得（スタックから）
  ValuePtr value = Value::createUndefined();
  if (!m_stack->isEmpty()) {
    value = m_stack->pop();
  }

  // 定数名を取得
  auto currentFrame = getCurrentCallFrame();
  auto env = currentFrame ? currentFrame->getEnvironment() : nullptr;
  std::string constName = env ? env->getConstantName(nameIndex) : "";

  if (!constName.empty() && env) {
    // 環境に定数を宣言（読み取り専用・不変）
    env->declareVariable(constName, value, true);
  }
}

void Interpreter::handleDeclareLet(const BytecodeInstruction& instruction) {
  // let変数名のインデックスを取得
  int nameIndex = instruction.getOperand(0);

  // 変数の初期値を取得（スタックにあれば）
  ValuePtr initialValue = Value::createUndefined();
  if (instruction.getOperandCount() > 1 && instruction.getOperand(1) != 0) {
    if (!m_stack->isEmpty()) {
      initialValue = m_stack->pop();
    }
  }

  // 変数名を取得
  auto currentFrame = getCurrentCallFrame();
  auto env = currentFrame ? currentFrame->getEnvironment() : nullptr;
  std::string varName = env ? env->getConstantName(nameIndex) : "";

  if (!varName.empty() && env) {
    // 環境にlet変数を宣言（ブロックスコープ）
    env->declareBlockScopedVariable(varName, initialValue, false);
  }
}

//-----------------------------------------------------------------------------
// CallFrame クラスの実装
//-----------------------------------------------------------------------------

CallFrame::CallFrame(
    std::shared_ptr<FunctionObject> function,
    std::shared_ptr<Environment> environment,
    ValuePtr thisValue,
    size_t returnAddress)
    : m_function(function),
      m_environment(environment),
      m_thisValue(thisValue),
      m_returnAddress(returnAddress),
      m_programCounter(0) {
}

CallFrame::~CallFrame() {
  // デストラクタの実装
}

std::shared_ptr<FunctionObject> CallFrame::getFunction() const {
  return m_function;
}

std::shared_ptr<Environment> CallFrame::getEnvironment() const {
  return m_environment;
}

ValuePtr CallFrame::getThisValue() const {
  return m_thisValue;
}

size_t CallFrame::getReturnAddress() const {
  return m_returnAddress;
}

size_t CallFrame::getProgramCounter() const {
  return m_programCounter;
}

void CallFrame::setProgramCounter(size_t pc) {
  m_programCounter = pc;
}

size_t CallFrame::incrementProgramCounter() {
  return ++m_programCounter;
}

}  // namespace core
}  // namespace aerojs