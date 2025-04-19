/**
 * @file interpreter.cpp
 * @brief バイトコードインタプリタの実装
 * 
 * このファイルはバイトコード命令を実行するインタプリタの実装を提供します。
 * インタプリタはバイトコード命令を順次実行し、JavaScript言語の意味論に従って動作します。
 */

#include "interpreter.h"

#include <iostream>
#include <cassert>
#include <stdexcept>

#include "bytecode_instruction.h"
#include "../stack/stack.h"
#include "../exception/exception.h"
#include "../../runtime/values/value.h"
#include "../../runtime/values/object.h"
#include "../../runtime/values/function.h"
#include "../../runtime/context/context.h"
#include "../../runtime/environment/environment.h"

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
        auto value = m_stack->peek(); // ポップせずに取得
        
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
        auto receiverObj = m_stack->pop(); // 実際のレシーバーオブジェクト
        
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
        auto receiverObj = m_stack->pop(); // 実際のレシーバーオブジェクト
        
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

// 非同期操作
void Interpreter::handleAwait(const BytecodeInstruction& instruction) {
    if (!m_stack->isEmpty()) {
        auto promiseValue = m_stack->pop();
        
        // promiseでなければPromise.resolveで包む
        if (!Object::isPromise(promiseValue)) {
            promiseValue = Object::createResolvedPromise(promiseValue);
        }
        
        // 現在のフレームをサスペンド
        auto currentFrame = getCurrentCallFrame();
        if (currentFrame) {
            // プロミスの解決を待機
            // 実際の実装では非同期コードをサスペンドしておく必要がある
            
            // ここでは簡略化のため、同期的に解決されたと仮定
            auto resolvedValue = Object::getPromiseResult(promiseValue);
            m_stack->push(resolvedValue);
        } else {
            m_stack->push(Value::createUndefined());
        }
    }
}

void Interpreter::handleYield(const BytecodeInstruction& instruction) {
    if (!m_stack->isEmpty()) {
        auto value = m_stack->pop();
        
        // 現在のフレームをサスペンド
        auto currentFrame = getCurrentCallFrame();
        if (currentFrame) {
            // ジェネレータフレームをサスペンド
            // 実際の実装ではフレームの状態をサスペンドにして制御を戻す
            
            // ここでは簡略化のため、イテレータリザルトを作成するだけ
            auto iterResult = Object::createIteratorResult(value, false);
            m_stack->push(iterResult);
        } else {
            m_stack->push(Value::createUndefined());
        }
    }
}

void Interpreter::handleYieldStar(const BytecodeInstruction& instruction) {
    if (!m_stack->isEmpty()) {
        auto iterable = m_stack->pop();
        
        // イテラブルからイテレータを取得
        auto iterator = Object::getIterator(iterable);
        
        // ここでは簡略化のため、最初の値だけを取得
        auto iterResult = Object::iteratorNext(iterator);
        m_stack->push(iterResult);
    }
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
        case 0: // 変数の削除
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
            }
            break;
            
        case 1: // プロパティの削除
            if (m_stack->size() >= 2) {
                auto propName = m_stack->pop();
                auto obj = m_stack->pop();
                
                // プロパティを削除
                bool success = Object::deleteProperty(obj, propName);
                m_stack->push(Value::createBoolean(success));
            }
            break;
            
        case 2: // 配列要素の削除
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
        case 0: // 名前付きエクスポート
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
            }
            break;
            
        case 1: // デフォルトエクスポート
            if (!m_stack->isEmpty()) {
                auto defaultValue = m_stack->pop();
                
                if (m_currentContext) {
                    // デフォルトエクスポートとして設定
                    m_currentContext->exportDefault(defaultValue);
                }
            }
            break;
            
        case 2: // 再エクスポート
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
            }
            break;
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
        auto value = m_stack->peek(); // ポップしない
        
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
        auto value = m_stack->peek(); // ポップせずに取得
        
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

} // namespace core
} // namespace aerojs 